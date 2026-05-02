#include <gtest/gtest.h>
#include <httplib.h>

#include "util/ollama_client.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace {

class LocalOllamaServer {
 public:
  httplib::Server server;

  bool start() {
    port_ = server.bind_to_any_port("127.0.0.1");
    if (port_ <= 0) return false;
    thread_ = std::thread([this]() { server.listen_after_bind(); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!server.is_running() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return server.is_running();
  }

  std::string endpoint() const {
    return "http://127.0.0.1:" + std::to_string(port_);
  }

  ~LocalOllamaServer() {
    server.stop();
    if (thread_.joinable()) thread_.join();
  }

 private:
  int port_ = 0;
  std::thread thread_;
};

void writeChunks(httplib::Response& response, std::vector<std::string> chunks) {
  response.set_chunked_content_provider(
      "application/x-ndjson",
      [chunks = std::move(chunks)](size_t offset, httplib::DataSink& sink) mutable {
        if (offset > 0) return true;
        for (const auto& chunk : chunks) {
          if (!sink.write(chunk.data(), chunk.size())) return false;
        }
        sink.done();
        return true;
      });
}

}  // namespace

TEST(OllamaClientTest, StreamsChatContentAcrossSplitNetworkChunks) {
  LocalOllamaServer local;
  local.server.Post("/api/chat", [](const httplib::Request& request, httplib::Response& response) {
    EXPECT_NE(request.body.find("\"messages\""), std::string::npos);
    EXPECT_NE(request.body.find("\"think\":false"), std::string::npos);
    writeChunks(response, {
        "{\"message\":{\"role\":\"assistant\",\"content\":\"Hel",
        "lo\"},\"done\":false}\n{\"message\":{\"role\":\"assistant\",\"content\":\"!\"},\"done\":false}\n",
        "{\"done\":true}\n",
    });
  });
  ASSERT_TRUE(local.start());

  asteria::util::OllamaClient client(local.endpoint());
  std::string output;
  const bool ok = client.generateStream("qwen3:latest", "hello", [&](const std::string& token) {
    output += token;
    return true;
  });

  EXPECT_TRUE(ok) << client.lastError();
  EXPECT_EQ(output, "Hello!");
}

TEST(OllamaClientTest, FallsBackToGenerateWhenChatIsUnsupported) {
  LocalOllamaServer local;
  local.server.Post("/api/chat", [](const httplib::Request&, httplib::Response& response) {
    response.status = 400;
    response.set_content(R"({"error":"chat is not supported by this model"})", "application/json");
  });
  local.server.Post("/api/generate", [](const httplib::Request& request, httplib::Response& response) {
    EXPECT_NE(request.body.find("\"prompt\""), std::string::npos);
    writeChunks(response, {"{\"response\":\"fallback ok\",\"done\":false}\n"});
  });
  ASSERT_TRUE(local.start());

  asteria::util::OllamaClient client(local.endpoint());
  std::string output;
  const bool ok = client.generateStream("legacy:latest", "hello", [&](const std::string& token) {
    output += token;
    return true;
  });

  EXPECT_TRUE(ok) << client.lastError();
  EXPECT_EQ(output, "fallback ok");
}

TEST(OllamaClientTest, ReportsOllamaStreamErrors) {
  LocalOllamaServer local;
  local.server.Post("/api/chat", [](const httplib::Request&, httplib::Response& response) {
    writeChunks(response, {"{\"error\":\"model requires more system memory\"}\n"});
  });
  ASSERT_TRUE(local.start());

  asteria::util::OllamaClient client(local.endpoint());
  const bool ok = client.generateStream("gemma4:31b", "hello", [](const std::string&) {
    return true;
  });

  EXPECT_FALSE(ok);
  EXPECT_NE(client.lastError().find("model requires more system memory"), std::string::npos);
}
