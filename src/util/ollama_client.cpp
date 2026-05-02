#include "ollama_client.h"
#include <httplib.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string_view>

namespace asteria::util {

namespace {

constexpr time_t kListModelsConnectionTimeoutSeconds = 3;
constexpr time_t kListModelsReadTimeoutSeconds = 10;
constexpr time_t kGenerateConnectionTimeoutSeconds = 30;
constexpr time_t kGenerateReadTimeoutSeconds = 30 * 60;
constexpr time_t kGenerateWriteTimeoutSeconds = 30;

struct EndpointParts {
  std::string host = "localhost";
  int port = 11434;
  std::string error;
};

struct StreamRequestResult {
  bool transportOk = false;
  bool abortedByCallback = false;
  int status = -1;
  httplib::Error transportError = httplib::Error::Success;
  std::string responseBody;
  std::string streamError;
};

std::string trim(std::string_view text) {
  size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return std::string(text.substr(begin, end - begin));
}

EndpointParts parseEndpoint(const std::string& endpoint) {
  EndpointParts parts;
  std::string url = trim(endpoint.empty() ? std::string_view("http://localhost:11434")
                                          : std::string_view(endpoint));
  while (!url.empty() && url.back() == '/') url.pop_back();

  const auto schemeEnd = url.find("://");
  if (schemeEnd != std::string::npos) {
    const std::string scheme = url.substr(0, schemeEnd);
    if (scheme != "http") {
      parts.error = "Ollama endpoint must use http:// because this build does not enable HTTPS.";
      return parts;
    }
    url = url.substr(schemeEnd + 3);
  }

  const auto pathStart = url.find('/');
  if (pathStart != std::string::npos) {
    url = url.substr(0, pathStart);
  }
  if (url.empty()) {
    parts.error = "Ollama endpoint is empty.";
    return parts;
  }

  if (url.front() == '[') {
    const auto hostEnd = url.find(']');
    if (hostEnd == std::string::npos) {
      parts.error = "Ollama endpoint has an invalid IPv6 host.";
      return parts;
    }
    parts.host = url.substr(1, hostEnd - 1);
    if (hostEnd + 1 < url.size()) {
      if (url[hostEnd + 1] != ':') {
        parts.error = "Ollama endpoint has an invalid IPv6 port separator.";
        return parts;
      }
      const std::string portText = url.substr(hostEnd + 2);
      char* end = nullptr;
      const long port = std::strtol(portText.c_str(), &end, 10);
      if (portText.empty() || *end != '\0' || port <= 0 || port > 65535) {
        parts.error = "Ollama endpoint has an invalid port.";
        return parts;
      }
      parts.port = static_cast<int>(port);
    }
    return parts;
  }

  const auto colon = url.rfind(':');
  if (colon != std::string::npos && url.find(':') == colon) {
    parts.host = url.substr(0, colon);
    const std::string portText = url.substr(colon + 1);
    char* end = nullptr;
    const long port = std::strtol(portText.c_str(), &end, 10);
    if (parts.host.empty() || portText.empty() || *end != '\0' || port <= 0 || port > 65535) {
      parts.error = "Ollama endpoint has an invalid host or port.";
      return parts;
    }
    parts.port = static_cast<int>(port);
  } else {
    parts.host = url;
  }

  return parts;
}

void appendUtf8(std::string& output, unsigned long codepoint) {
  if (codepoint <= 0x7F) {
    output += static_cast<char>(codepoint);
  } else if (codepoint <= 0x7FF) {
    output += static_cast<char>(0xC0 | (codepoint >> 6));
    output += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else if (codepoint <= 0xFFFF) {
    output += static_cast<char>(0xE0 | (codepoint >> 12));
    output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    output += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else if (codepoint <= 0x10FFFF) {
    output += static_cast<char>(0xF0 | (codepoint >> 18));
    output += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    output += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else {
    appendUtf8(output, 0xFFFD);
  }
}

bool parseHexCodepoint(const std::string& text, size_t pos, unsigned long& codepoint) {
  if (pos + 4 > text.size()) return false;
  codepoint = 0;
  for (size_t i = 0; i < 4; ++i) {
    const unsigned char c = static_cast<unsigned char>(text[pos + i]);
    codepoint <<= 4;
    if (c >= '0' && c <= '9') {
      codepoint += c - '0';
    } else if (c >= 'a' && c <= 'f') {
      codepoint += c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      codepoint += c - 'A' + 10;
    } else {
      return false;
    }
  }
  return true;
}

std::optional<std::string> parseJsonStringAt(const std::string& text, size_t quotePos) {
  if (quotePos >= text.size() || text[quotePos] != '"') return std::nullopt;

  std::string value;
  for (size_t i = quotePos + 1; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '"') {
      return value;
    }
    if (c != '\\') {
      value += c;
      continue;
    }
    if (++i >= text.size()) return std::nullopt;
    switch (text[i]) {
      case '"': value += '"'; break;
      case '\\': value += '\\'; break;
      case '/': value += '/'; break;
      case 'b': value += '\b'; break;
      case 'f': value += '\f'; break;
      case 'n': value += '\n'; break;
      case 'r': value += '\r'; break;
      case 't': value += '\t'; break;
      case 'u': {
        unsigned long codepoint = 0;
        if (!parseHexCodepoint(text, i + 1, codepoint)) return std::nullopt;
        i += 4;
        if (0xD800 <= codepoint && codepoint <= 0xDBFF) {
          if (i + 6 < text.size() && text[i + 1] == '\\' && text[i + 2] == 'u') {
            unsigned long low = 0;
            if (parseHexCodepoint(text, i + 3, low) && 0xDC00 <= low && low <= 0xDFFF) {
              codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
              i += 6;
            } else {
              codepoint = 0xFFFD;
            }
          } else {
            codepoint = 0xFFFD;
          }
        } else if (0xDC00 <= codepoint && codepoint <= 0xDFFF) {
          codepoint = 0xFFFD;
        }
        appendUtf8(value, codepoint);
        break;
      }
      default:
        value += text[i];
        break;
    }
  }

  return std::nullopt;
}

std::optional<std::string> extractJsonStringField(const std::string& json,
                                                  const std::string& fieldName) {
  const std::string key = "\"" + fieldName + "\"";
  size_t searchFrom = 0;
  while (true) {
    const size_t keyPos = json.find(key, searchFrom);
    if (keyPos == std::string::npos) return std::nullopt;

    size_t pos = keyPos + key.size();
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size() || json[pos] != ':') {
      searchFrom = keyPos + key.size();
      continue;
    }
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size() || json[pos] != '"') {
      searchFrom = keyPos + key.size();
      continue;
    }
    return parseJsonStringAt(json, pos);
  }
}

std::string jsonEscape(const std::string& text) {
  std::string out;
  out.reserve(text.size() + 32);
  for (char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buffer[8];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
          out += buffer;
        } else {
          out += c;
        }
    }
  }
  return out;
}

std::string lowerCopy(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

std::string ollamaErrorMessage(const std::string& body) {
  if (const auto error = extractJsonStringField(body, "error")) {
    return "Ollama error: " + *error;
  }

  const std::string compact = trim(body);
  if (!compact.empty()) {
    constexpr size_t kMaxBodyChars = 600;
    return compact.size() > kMaxBodyChars ? compact.substr(0, kMaxBodyChars) + "..." : compact;
  }
  return {};
}

std::string connectionErrorMessage(httplib::Error error) {
  const std::string detail = httplib::to_string(error);
  if (error == httplib::Error::Read) {
    return "Connection failed while reading the Ollama stream: " + detail +
           ". Large local models may still be loading, may need more memory, or may have stopped responding.";
  }
  if (error == httplib::Error::ConnectionTimeout) {
    return "Connection timed out while contacting Ollama. Check that the Ollama server is running and reachable.";
  }
  return "Connection failed: " + detail;
}

std::string resultErrorMessage(const StreamRequestResult& result) {
  if (!result.streamError.empty()) {
    return result.streamError;
  }
  if (!result.transportOk) {
    return connectionErrorMessage(result.transportError);
  }
  const std::string ollamaError = ollamaErrorMessage(result.responseBody);
  if (!ollamaError.empty()) {
    return "HTTP " + std::to_string(result.status) + ": " + ollamaError;
  }
  return "HTTP " + std::to_string(result.status);
}

bool shouldFallbackToGenerate(const StreamRequestResult& result) {
  if (result.abortedByCallback || result.transportError == httplib::Error::Read) return false;

  const std::string message = lowerCopy(result.streamError.empty()
      ? ollamaErrorMessage(result.responseBody)
      : result.streamError);
  if ((result.status == 400 || result.status == 404 || result.status == 405) && message.empty()) {
    return true;
  }
  return message.find("chat") != std::string::npos &&
         (message.find("support") != std::string::npos ||
          message.find("unsupported") != std::string::npos ||
          message.find("not found") != std::string::npos);
}

StreamRequestResult streamOllamaRequest(
    httplib::Client& cli,
    const std::string& path,
    const std::string& requestBody,
    const std::vector<std::string>& tokenFields,
    const std::function<bool(const std::string& token)>& onToken) {
  StreamRequestResult result;
  std::string pendingLine;

  auto processLine = [&](std::string line) -> bool {
    line = trim(line);
    if (line.empty()) return true;

    if (const auto error = extractJsonStringField(line, "error")) {
      result.streamError = "Ollama error: " + *error;
      return false;
    }

    for (const auto& field : tokenFields) {
      if (const auto token = extractJsonStringField(line, field)) {
        if (!token->empty() && onToken) {
          if (!onToken(*token)) {
            result.abortedByCallback = true;
            return false;
          }
        }
        return true;
      }
    }

    return true;
  };

  httplib::Request req;
  req.method = "POST";
  req.path = path;
  req.set_header("Content-Type", "application/json");
  req.set_header("Accept", "application/x-ndjson");
  req.body = requestBody;
  req.response_handler = [&](const httplib::Response& response) -> bool {
    result.status = response.status;
    return true;
  };

  req.content_receiver = [&](const char* data, size_t len,
                             uint64_t /*offset*/, uint64_t /*total*/) -> bool {
    std::string_view chunk(data, len);
    if (result.status != 200) {
      result.responseBody.append(chunk.data(), chunk.size());
      return true;
    }

    pendingLine.append(chunk.data(), chunk.size());
    size_t lineStart = 0;
    while (true) {
      const size_t newline = pendingLine.find('\n', lineStart);
      if (newline == std::string::npos) break;
      if (!processLine(pendingLine.substr(lineStart, newline - lineStart))) {
        return false;
      }
      lineStart = newline + 1;
    }
    if (lineStart > 0) {
      pendingLine.erase(0, lineStart);
    }
    return true;
  };

  httplib::Response response;
  result.transportOk = cli.send(req, response, result.transportError);
  if (result.transportOk && result.status < 0) {
    result.status = response.status;
  }
  if (result.transportOk && result.status == 200 && !pendingLine.empty()) {
    processLine(pendingLine);
  }
  if (result.responseBody.empty() && !response.body.empty()) {
    result.responseBody = response.body;
  }
  return result;
}

bool streamSucceeded(const StreamRequestResult& result) {
  return result.transportOk && result.status == 200 && result.streamError.empty();
}

}  // namespace

OllamaClient::OllamaClient(const std::string& endpoint) {
  const EndpointParts parts = parseEndpoint(endpoint);
  m_host = parts.host;
  m_port = parts.port;
  m_endpointError = parts.error;
}

OllamaClient::~OllamaClient() {
  stop();
}

void OllamaClient::stop() {
  std::shared_ptr<httplib::Client> client;
  {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    client = m_activeClient;
  }
  if (client) {
    client->stop();
  }
}

std::vector<std::string> OllamaClient::listModels() {
  m_lastError.clear();
  std::vector<std::string> models;
  if (!m_endpointError.empty()) {
    m_lastError = m_endpointError;
    return models;
  }

  auto cli = std::make_shared<httplib::Client>(m_host, m_port);
  {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    m_activeClient = cli;
  }
  auto clearActiveClient = [&]() {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    if (m_activeClient == cli) {
      m_activeClient.reset();
    }
  };

  cli->set_connection_timeout(kListModelsConnectionTimeoutSeconds);
  cli->set_read_timeout(kListModelsReadTimeoutSeconds);

  auto res = cli->Get("/api/tags");
  clearActiveClient();
  if (!res) {
    m_lastError = connectionErrorMessage(res.error());
    return models;
  }
  if (res->status != 200) {
    const std::string bodyError = ollamaErrorMessage(res->body);
    m_lastError = "HTTP " + std::to_string(res->status) +
                  (bodyError.empty() ? std::string() : ": " + bodyError);
    return models;
  }

  const std::string& body = res->body;
  size_t pos = 0;
  while ((pos = body.find("\"name\"", pos)) != std::string::npos) {
    if (const auto name = extractJsonStringField(body.substr(pos), "name")) {
      models.push_back(*name);
    }
    pos += 6;
  }

  return models;
}

bool OllamaClient::generateStream(
    const std::string& model,
    const std::string& prompt,
    std::function<bool(const std::string& token)> onToken) {
  m_lastError.clear();
  if (!m_endpointError.empty()) {
    m_lastError = m_endpointError;
    return false;
  }

  auto cli = std::make_shared<httplib::Client>(m_host, m_port);
  {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    m_activeClient = cli;
  }
  auto clearActiveClient = [&]() {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    if (m_activeClient == cli) {
      m_activeClient.reset();
    }
  };

  cli->set_connection_timeout(kGenerateConnectionTimeoutSeconds);
  cli->set_read_timeout(kGenerateReadTimeoutSeconds);
  cli->set_write_timeout(kGenerateWriteTimeoutSeconds);

  const std::string escapedModel = jsonEscape(model);
  const std::string escapedPrompt = jsonEscape(prompt);
  const std::string chatBody = R"({"model":")" + escapedModel +
      R"(","messages":[{"role":"user","content":")" + escapedPrompt +
      R"("}],"stream":true,"think":false,"keep_alive":"10m"})";

  StreamRequestResult chatResult = streamOllamaRequest(
      *cli, "/api/chat", chatBody, {"content", "response"}, onToken);
  if (chatResult.abortedByCallback) {
    clearActiveClient();
    return true;
  }
  if (streamSucceeded(chatResult)) {
    clearActiveClient();
    return true;
  }

  if (shouldFallbackToGenerate(chatResult)) {
    const std::string generateBody = R"({"model":")" + escapedModel +
        R"(","prompt":")" + escapedPrompt +
        R"(","stream":true,"keep_alive":"10m"})";
    StreamRequestResult generateResult = streamOllamaRequest(
        *cli, "/api/generate", generateBody, {"response", "content"}, onToken);
    if (generateResult.abortedByCallback) {
      clearActiveClient();
      return true;
    }
    if (streamSucceeded(generateResult)) {
      clearActiveClient();
      return true;
    }
    m_lastError = resultErrorMessage(generateResult);
    clearActiveClient();
    return false;
  }

  m_lastError = resultErrorMessage(chatResult);
  clearActiveClient();
  return false;
}

}  // namespace asteria::util
