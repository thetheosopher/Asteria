#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "domain/computed_chart.h"
#include "domain/types.h"

namespace asteria::ui {

struct AppContext;

/// Dockable panel that queries a local Ollama model for an AI interpretation
/// of the currently computed chart.  Streams tokens in real-time.
class AiInterpretationPanel {
 public:
  bool open = false;

  explicit AiInterpretationPanel(AppContext& ctx);
  ~AiInterpretationPanel();

  void draw();

  /// Called by chart panels whenever a new chart is computed.
  void setChart(const domain::ComputedChart& chart, domain::ChartType type);

 private:
  void startGeneration();
  void stopGeneration();
  std::string buildPrompt() const;
  std::string chartFactsBlock() const;

  AppContext& m_ctx;

  // Chart data (set from main thread).
  std::optional<domain::ComputedChart> m_chart;
  domain::ChartType m_chartType = domain::ChartType::Natal;

  // User addendum field.
  char m_focusBuf[512] = {};

  // Streaming state — shared between UI thread and worker.
  std::mutex m_mutex;
  std::string m_streamedText;     // accumulated response text
  std::string m_errorText;        // error from worker
  std::atomic<bool> m_generating{false};
  std::atomic<bool> m_abortFlag{false};
  std::thread m_worker;

  // Snapshot read by draw() each frame (no lock needed — only UI thread reads/writes).
  std::string m_displayText;
  std::string m_displayError;
};

}  // namespace asteria::ui
