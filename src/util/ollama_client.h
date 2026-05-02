#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace httplib {
class Client;
}

namespace asteria::util {

/// Lightweight client for the Ollama REST API.
/// All methods are synchronous and intended to be called from a worker thread.
class OllamaClient {
 public:
  /// @param endpoint  Base URL, e.g. "http://localhost:11434".
  explicit OllamaClient(const std::string& endpoint);
  ~OllamaClient();

  /// Fetch the list of available model names from the server.
  /// Returns an empty vector on failure (sets lastError()).
  std::vector<std::string> listModels();

  /// Stream a completion. The callback is invoked once per token chunk with
  /// the text fragment. Return false from the callback to abort generation.
  /// Returns true if the generation completed (or was aborted by callback).
  bool generateStream(const std::string& model,
                      const std::string& prompt,
                      std::function<bool(const std::string& token)> onToken);

  /// Human-readable description of the last error (empty on success).
  const std::string& lastError() const { return m_lastError; }

  /// Cancel any in-flight HTTP request owned by this client.
  void stop();

 private:
  std::string m_host;
  int m_port = 11434;
  std::string m_endpointError;
  std::string m_lastError;
  std::mutex m_clientMutex;
  std::shared_ptr<httplib::Client> m_activeClient;
};

}  // namespace asteria::util
