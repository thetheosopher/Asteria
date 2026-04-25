#include "ollama_client.h"
#include <httplib.h>
#include <sstream>

namespace asteria::util {

OllamaClient::OllamaClient(const std::string& endpoint) {
  // Parse "http://host:port" into host + port.
  std::string url = endpoint;
  // Strip trailing slash.
  while (!url.empty() && url.back() == '/') url.pop_back();
  // Strip scheme.
  auto schemeEnd = url.find("://");
  if (schemeEnd != std::string::npos) url = url.substr(schemeEnd + 3);
  // Split host:port.
  auto colon = url.rfind(':');
  if (colon != std::string::npos) {
    m_host = url.substr(0, colon);
    m_port = std::stoi(url.substr(colon + 1));
  } else {
    m_host = url;
    m_port = 11434;
  }
}

std::vector<std::string> OllamaClient::listModels() {
  m_lastError.clear();
  std::vector<std::string> models;

  httplib::Client cli(m_host, m_port);
  cli.set_connection_timeout(3);  // seconds
  cli.set_read_timeout(5);

  auto res = cli.Get("/api/tags");
  if (!res) {
    m_lastError = "Connection failed: " + httplib::to_string(res.error());
    return models;
  }
  if (res->status != 200) {
    m_lastError = "HTTP " + std::to_string(res->status);
    return models;
  }

  // Minimal JSON parsing — Ollama returns {"models":[{"name":"..."},...]}.
  // We avoid pulling in a full JSON lib by scanning for "name":" patterns.
  const std::string& body = res->body;
  const std::string key = "\"name\":\"";
  size_t pos = 0;
  while ((pos = body.find(key, pos)) != std::string::npos) {
    pos += key.size();
    auto end = body.find('"', pos);
    if (end == std::string::npos) break;
    models.push_back(body.substr(pos, end - pos));
    pos = end + 1;
  }

  return models;
}

bool OllamaClient::generateStream(
    const std::string& model,
    const std::string& prompt,
    std::function<bool(const std::string& token)> onToken) {
  m_lastError.clear();

  httplib::Client cli(m_host, m_port);
  cli.set_connection_timeout(10);
  cli.set_read_timeout(120);

  // Build request JSON manually (no JSON library dependency).
  auto jsonEscape = [](const std::string& s) -> std::string {
    std::string out;
    out.reserve(s.size() + 32);
    for (char c : s) {
      switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
          if (static_cast<unsigned char>(c) < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
            out += buf;
          } else {
            out += c;
          }
      }
    }
    return out;
  };

  std::string requestBody = R"({"model":")" + jsonEscape(model) +
                            R"(","prompt":")" + jsonEscape(prompt) +
                            R"(","stream":true})";

  bool abortedByCallback = false;

  // Use low-level send() to get streaming content_receiver support.
  httplib::Request req;
  req.method = "POST";
  req.path   = "/api/generate";
  req.set_header("Content-Type", "application/json");
  req.body = requestBody;

  req.content_receiver = [&](const char* data, size_t len,
                             uint64_t /*offset*/, uint64_t /*total*/) -> bool {
    // Ollama streams newline-delimited JSON objects.
    std::string chunk(data, len);
    std::istringstream lines(chunk);
    std::string line;
    while (std::getline(lines, line)) {
      if (line.empty()) continue;
      // Extract "response":"..." field.
      const std::string rkey = "\"response\":\"";
      auto rpos = line.find(rkey);
      if (rpos == std::string::npos) continue;
      rpos += rkey.size();

      // Parse the JSON string value (handles basic escapes).
      std::string token;
      for (size_t i = rpos; i < line.size(); ++i) {
        if (line[i] == '"') break;
        if (line[i] == '\\' && i + 1 < line.size()) {
          ++i;
          switch (line[i]) {
            case 'n':  token += '\n'; break;
            case 'r':  token += '\r'; break;
            case 't':  token += '\t'; break;
            case '"':  token += '"';  break;
            case '\\': token += '\\'; break;
            case '/':  token += '/';  break;
            case 'u': {
              // \uXXXX — decode 4 hex digits to a Unicode codepoint, emit UTF-8.
              if (i + 4 < line.size()) {
                char hex[5] = { line[i+1], line[i+2], line[i+3], line[i+4], 0 };
                unsigned long cp = std::strtoul(hex, nullptr, 16);
                i += 4;
                if (cp < 0x80) {
                  token += static_cast<char>(cp);
                } else if (cp < 0x800) {
                  token += static_cast<char>(0xC0 | (cp >> 6));
                  token += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                  token += static_cast<char>(0xE0 | (cp >> 12));
                  token += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                  token += static_cast<char>(0x80 | (cp & 0x3F));
                }
              }
              break;
            }
            default:   token += line[i]; break;
          }
        } else {
          token += line[i];
        }
      }

      if (!token.empty() && onToken) {
        if (!onToken(token)) {
          abortedByCallback = true;
          return false;
        }
      }
    }
    return true;
  };

  httplib::Response res;
  httplib::Error err = httplib::Error::Success;
  bool ok = cli.send(req, res, err);

  if (abortedByCallback) return true;

  if (!ok) {
    m_lastError = "Connection failed: " + httplib::to_string(err);
    return false;
  }
  if (res.status != 200) {
    m_lastError = "HTTP " + std::to_string(res.status) + ": " + res.body;
    return false;
  }
  return true;
}

}  // namespace asteria::util
