#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "core/report_service.h"
#include "domain/computed_chart.h"
#include "domain/types.h"
#include "render/chart_scene.h"
#include "render/theme_presets.h"

namespace asteria::util {
class OllamaClient;
}

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
  void setChart(const domain::ComputedChart& chart,
                domain::ChartType type,
                std::string sourceLabel = {});

  /// Called by workspace AI buttons to open/focus the panel and start generation.
  void requestInterpretation(const domain::ComputedChart& chart,
                             domain::ChartType type,
                             std::string sourceLabel = {});

  /// Called by non-chart panels that want to run a custom AI query using their own prompt.
  void requestCustomInterpretation(std::string prompt,
                                   std::string analysisLabel,
                                   std::string queryTypeLabel);

 private:
  void startGeneration();
  void stopGeneration();
  void saveInterpretation();
  void copyInterpretation();
  void copyInterpretationPackage();
  void savePdfReport();
  void drawReportPreview();
  void clearOutputState();
  std::string currentAnalysisLabel() const;
  std::string currentQueryTypeLabel() const;
  render::ThemePreset currentThemePreset() const;
  std::optional<render::ChartScene> buildReportScene() const;
  core::PdfReportRequest buildReportRequest(const std::string& outputPath = {}) const;
  std::string deterministicInterpretationMarkdown() const;
  std::string generatedAtLabel() const;
  std::string buildPrompt() const;
  std::string chartFactsBlock() const;

  AppContext& m_ctx;

  // Chart data (set from main thread).
  std::optional<domain::ComputedChart> m_chart;
  domain::ChartType m_chartType = domain::ChartType::Natal;
  std::string m_sourceLabel;
  std::string m_queryTypeLabel;
  std::string m_customPrompt;
  bool m_focusOnNextDraw = false;

  // User addendum field.
  char m_focusBuf[512] = {};
  core::PdfReportTemplate m_reportTemplate = core::PdfReportTemplate::ClientReport;
  bool m_reportIncludeBuiltIn = true;
  bool m_reportArchivalMode = false;
  bool m_reportPreferVectorChart = false;
  bool m_reportPreviewOpen = false;

  // Streaming state — shared between UI thread and worker.
  std::mutex m_mutex;
  std::string m_streamedText;     // accumulated response text
  std::string m_errorText;        // error from worker
  std::atomic<bool> m_generating{false};
  std::atomic<bool> m_abortFlag{false};
  std::thread m_worker;
  std::mutex m_activeClientMutex;
  std::shared_ptr<asteria::util::OllamaClient> m_activeClient;

  // Snapshot read by draw() each frame (no lock needed — only UI thread reads/writes).
  std::string m_displayText;
  std::string m_displayError;
  std::string m_statusText;
  bool m_statusIsError = false;
};

}  // namespace asteria::ui
