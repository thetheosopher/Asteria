#pragma once
#include <functional>
#include <string>
#include <vector>

namespace asteria::util {

/// Lightweight client for the Ollama REST API.
/// All methods are synchronous and intended to be called from a worker thread.
class OllamaClient {
 public:
  /// @param endpoint  Base URL, e.g. "http://localhost:11434".
  explicit OllamaClient(const std::string& endpoint);

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

 private:
  std::string m_host;
  int m_port = 11434;
  std::string m_lastError;
};

}  // namespace asteria::util
