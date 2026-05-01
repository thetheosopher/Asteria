#include "ai_interpretation_panel.h"
#include "app_context.h"
#include "clipboard.h"
#include "export_options.h"
#include "file_dialog.h"
#include "markdown_render.h"
#include "core/report_service.h"
#include "render/comparison_chart_layout.h"
#include "render/natal_chart_layout.h"
#include "render/theme_presets.h"
#include "render/transit_chart_layout.h"
#include "util/ollama_client.h"
#include "imgui.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

namespace asteria::ui {

namespace {

std::string defaultAnalysisLabel(domain::ChartType type) {
  switch (type) {
    case domain::ChartType::Natal:
      return "Natal chart";
    case domain::ChartType::Synastry:
      return "Synastry chart";
    case domain::ChartType::Composite:
      return "Composite chart";
    case domain::ChartType::TransitToNatal:
      return "Transit-to-natal chart";
  }
  return "Chart";
}

std::string defaultQueryTypeLabel(domain::ChartType type) {
  switch (type) {
    case domain::ChartType::Natal:
      return "Natal Interpretation";
    case domain::ChartType::Synastry:
      return "Synastry Interpretation";
    case domain::ChartType::Composite:
      return "Composite Interpretation";
    case domain::ChartType::TransitToNatal:
      return "Transit-to-Natal Interpretation";
  }
  return "AI Interpretation";
}

std::string buildInterpretationFileName(const std::string& analysisLabel,
                                        const std::string& model) {
  std::string baseName = analysisLabel.empty() ? std::string("Chart") : analysisLabel;
  if (!model.empty()) {
    baseName += " - " + model;
  }
  baseName += " - AI Interpretation";
  return sanitizeFileName(baseName) + ".md";
}

std::string buildReportFileName(const std::string& analysisLabel,
                                const std::string& model) {
  std::string baseName = analysisLabel.empty() ? std::string("Chart") : analysisLabel;
  if (!model.empty()) {
    baseName += " - " + model;
  }
  baseName += " - AI Report";
  return sanitizeFileName(baseName) + ".pdf";
}

int parseSettingIndex(const AppContext& ctx, const char* key, int fallback, int maxExclusive) {
  int value = fallback;
  try { value = std::stoi(ctx.getSetting(key, std::to_string(fallback))); }
  catch (...) { value = fallback; }
  if (value < 0 || value >= maxExclusive) return fallback;
  return value;
}

constexpr core::PdfReportTemplate kReportTemplates[] = {
    core::PdfReportTemplate::ClientReport,
    core::PdfReportTemplate::StudyNotes,
    core::PdfReportTemplate::CompactOnePage,
    core::PdfReportTemplate::ArchiveCopy,
};

int reportTemplateIndex(core::PdfReportTemplate reportTemplate) {
  for (int i = 0; i < static_cast<int>(std::size(kReportTemplates)); ++i) {
    if (kReportTemplates[i] == reportTemplate) return i;
  }
  return 0;
}

std::string localTimestampLabel() {
  std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_s(&local, &now);
  char buffer[32] = {};
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
  return buffer;
}

}  // namespace

AiInterpretationPanel::AiInterpretationPanel(AppContext& ctx) : m_ctx(ctx) {}

AiInterpretationPanel::~AiInterpretationPanel() {
  stopGeneration();
}

void AiInterpretationPanel::setChart(const domain::ComputedChart& chart,
                                     domain::ChartType type,
                                     std::string sourceLabel) {
  if (sourceLabel.empty()) {
    sourceLabel = defaultAnalysisLabel(type);
  }
  const std::string queryTypeLabel = defaultQueryTypeLabel(type);

  const bool contextChanged = !m_chart.has_value()
      || m_chart->computedChartId != chart.computedChartId
      || m_chartType != type
      || m_sourceLabel != sourceLabel
      || m_queryTypeLabel != queryTypeLabel
      || !m_customPrompt.empty();

  if (contextChanged) {
    clearOutputState();
  }

  m_chart = chart;
  m_chartType = type;
  m_sourceLabel = std::move(sourceLabel);
  m_queryTypeLabel = queryTypeLabel;
  m_customPrompt.clear();
}

void AiInterpretationPanel::requestInterpretation(const domain::ComputedChart& chart,
                                                  domain::ChartType type,
                                                  std::string sourceLabel) {
  setChart(chart, type, std::move(sourceLabel));
  open = true;
  m_focusOnNextDraw = true;

  const bool ollamaEnabled = m_ctx.getSetting("ollama_enabled", "0") == "1";
  const std::string model = m_ctx.getSetting("ollama_model", "");
  if (ollamaEnabled && !model.empty()) {
    startGeneration();
  }
}

void AiInterpretationPanel::requestCustomInterpretation(std::string prompt,
                                                        std::string analysisLabel,
                                                        std::string queryTypeLabel) {
  if (analysisLabel.empty()) {
    analysisLabel = "Custom AI request";
  }
  if (queryTypeLabel.empty()) {
    queryTypeLabel = "Custom Interpretation";
  }

  const bool contextChanged = m_chart.has_value()
      || m_sourceLabel != analysisLabel
      || m_queryTypeLabel != queryTypeLabel
      || m_customPrompt != prompt;
  if (contextChanged) {
    clearOutputState();
  }

  m_chart.reset();
  m_sourceLabel = std::move(analysisLabel);
  m_queryTypeLabel = std::move(queryTypeLabel);
  m_customPrompt = std::move(prompt);
  open = true;
  m_focusOnNextDraw = true;

  const bool ollamaEnabled = m_ctx.getSetting("ollama_enabled", "0") == "1";
  const std::string model = m_ctx.getSetting("ollama_model", "");
  if (ollamaEnabled && !model.empty()) {
    startGeneration();
  }
}

void AiInterpretationPanel::clearOutputState() {
  stopGeneration();

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_streamedText.clear();
    m_errorText.clear();
  }

  m_displayText.clear();
  m_displayError.clear();
  m_statusText.clear();
  m_statusIsError = false;
}

std::string AiInterpretationPanel::currentAnalysisLabel() const {
  if (!m_sourceLabel.empty()) {
    return m_sourceLabel;
  }
  return defaultAnalysisLabel(m_chartType);
}

std::string AiInterpretationPanel::currentQueryTypeLabel() const {
  if (!m_queryTypeLabel.empty()) {
    return m_queryTypeLabel;
  }
  return defaultQueryTypeLabel(m_chartType);
}

// ---------------------------------------------------------------------------
// draw() — main UI thread
// ---------------------------------------------------------------------------
void AiInterpretationPanel::draw() {
  if (!open) return;

  // Check if Ollama is enabled.
  bool ollamaEnabled = m_ctx.getSetting("ollama_enabled", "0") == "1";

  ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
  if (m_focusOnNextDraw) {
    ImGui::SetNextWindowFocus();
  }
  if (!ImGui::Begin("AI Interpretation", &open)) { ImGui::End(); return; }
  m_focusOnNextDraw = false;

  const std::string analysisLabel = currentAnalysisLabel();
  if (!analysisLabel.empty()) {
    ImGui::TextWrapped("Analyzing: %s", analysisLabel.c_str());
  }
  const std::string queryTypeLabel = currentQueryTypeLabel();
  if (!queryTypeLabel.empty()) {
    ImGui::Text("Query Type: %s", queryTypeLabel.c_str());
  }

  std::string model = m_ctx.getSetting("ollama_model", "");
  const bool modelReady = ollamaEnabled && !model.empty();
  if (!ollamaEnabled) {
    ImGui::Separator();
    ImGui::TextWrapped("Ollama integration is disabled. Enable it in Settings and "
                       "select a model to use AI-generated interpretations.");
  } else if (model.empty()) {
    ImGui::Separator();
    ImGui::TextWrapped("No Ollama model selected. Open Settings and choose a model "
                       "from the Ollama Model dropdown.");
  }

  // Status header.
  ImGui::Text("Model: %s", model.empty() ? "None selected" : model.c_str());
  const bool hasContext = m_chart.has_value() || !m_customPrompt.empty();
  if (!hasContext) {
    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
      "No chart or AI request available yet. Compute a chart or send a request from another panel.");
  }
  ImGui::Separator();

  // Snapshot the shared streaming state.
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_displayText = m_streamedText;
    m_displayError = m_errorText;
  }

  // Optional focus question.
  ImGuiStyle& style = ImGui::GetStyle();
  const float panelContentWidth = ImGui::GetContentRegionAvail().x;
  const float labelWidth = ImGui::CalcTextSize("Focus (optional):").x;
  const float generateButtonWidth = ImGui::CalcTextSize("Generate").x + style.FramePadding.x * 2.0f;
  const float stopButtonWidth = ImGui::CalcTextSize("Stop").x + style.FramePadding.x * 2.0f;
  const float primaryButtonWidth = (std::max)(generateButtonWidth, stopButtonWidth);
  const float availableFocusWidth = panelContentWidth - labelWidth - primaryButtonWidth - style.ItemSpacing.x * 2.0f;
  const float focusWidth = (std::max)(1.0f, availableFocusWidth);

  ImGui::Text("Focus (optional):");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(focusWidth);
  ImGui::InputText("##AiFocus", m_focusBuf, sizeof(m_focusBuf));

  ImGui::SameLine();
  bool generating = m_generating.load();
  if (generating) {
    if (ImGui::Button("Stop")) {
      stopGeneration();
    }
  } else {
    const bool canGenerate = hasContext && modelReady;
    if (!canGenerate) ImGui::BeginDisabled();
    if (ImGui::Button("Generate")) {
      startGeneration();
    }
    if (!canGenerate) ImGui::EndDisabled();
  }

  bool canSave = !m_displayText.empty();
  const bool canPackage = m_chart.has_value() || canSave;
  const bool canReport = canSave && m_chart.has_value();
  if (!canSave) ImGui::BeginDisabled();
  if (ImGui::Button("Save...")) {
    saveInterpretation();
  }
  ImGui::SameLine();
  if (ImGui::Button("Copy")) {
    copyInterpretation();
  }
  if (!canSave) ImGui::EndDisabled();

  ImGui::SameLine();
  if (!canPackage) ImGui::BeginDisabled();
  if (ImGui::Button("Copy Package")) {
    copyInterpretationPackage();
  }
  if (!canPackage) ImGui::EndDisabled();

  ImGui::SameLine();
  if (!canReport) ImGui::BeginDisabled();
  if (ImGui::Button("Preview Report")) {
    m_reportPreviewOpen = true;
    ImGui::OpenPopup("Report Preview");
  }
  ImGui::SameLine();
  if (ImGui::Button("Report PDF...")) {
    savePdfReport();
  }
  if (!canReport) ImGui::EndDisabled();

  if (m_chart.has_value()) {
    int templateIndex = reportTemplateIndex(m_reportTemplate);
    ImGui::SetNextItemWidth(190.0f);
    if (ImGui::Combo("Report Template", &templateIndex,
                     "Client Report\0Study Notes\0Compact One-Page\0Archive Copy\0")) {
      m_reportTemplate = kReportTemplates[templateIndex];
      if (m_reportTemplate == core::PdfReportTemplate::ArchiveCopy) {
        m_reportArchivalMode = true;
      }
    }
    ImGui::Checkbox("Built-In Section", &m_reportIncludeBuiltIn);
    ImGui::SameLine();
    ImGui::Checkbox("Vector Chart", &m_reportPreferVectorChart);
    ImGui::SameLine();
    ImGui::Checkbox("Archival", &m_reportArchivalMode);
  }

  // Error display.
  if (!m_displayError.empty()) {
    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", m_displayError.c_str());
  }
  if (!m_statusText.empty()) {
    const ImVec4 color = m_statusIsError
        ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f)
        : ImVec4(0.3f, 0.7f, 0.4f, 1.0f);
    ImGui::TextColored(color, "%s", m_statusText.c_str());
  }

  // Streaming spinner.
  if (generating) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 0.9f, 1.0f), "Generating...");
  }

  ImGui::Separator();

  // Scrolling rich-text area.
  ImGui::BeginChild("AiResponseScroll", ImVec2(0, 0), ImGuiChildFlags_Borders,
                     ImGuiWindowFlags_HorizontalScrollbar);

  if (!m_displayText.empty()) {
    markdown_render::renderMarkdown(m_displayText);

    // Auto-scroll while generating.
    if (generating && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f) {
      ImGui::SetScrollHereY(1.0f);
    }
  } else if (!generating) {
    const std::string placeholder = "Click \"Generate\" to request "
        + currentQueryTypeLabel() + " for " + currentAnalysisLabel() + ".";
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", placeholder.c_str());
  }

  ImGui::EndChild();
  drawReportPreview();
  ImGui::End();
}

// ---------------------------------------------------------------------------
// startGeneration() — launches worker thread
// ---------------------------------------------------------------------------
void AiInterpretationPanel::startGeneration() {
  stopGeneration();

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_streamedText.clear();
    m_errorText.clear();
  }
  m_displayText.clear();
  m_displayError.clear();
  m_abortFlag.store(false);
  m_generating.store(true);

  std::string endpoint = m_ctx.getSetting("ollama_endpoint", "http://localhost:11434");
  std::string model    = m_ctx.getSetting("ollama_model", "");
  std::string prompt   = buildPrompt();

  m_worker = std::thread([this, endpoint, model, prompt]() {
    util::OllamaClient client(endpoint);
    bool ok = client.generateStream(model, prompt,
        [this](const std::string& token) -> bool {
          if (m_abortFlag.load()) return false;
          std::lock_guard<std::mutex> lock(m_mutex);
          m_streamedText += token;
          return true;
        });
    if (!ok) {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_errorText = client.lastError();
    }
    m_generating.store(false);
  });
}

void AiInterpretationPanel::stopGeneration() {
  m_abortFlag.store(true);
  if (m_worker.joinable()) m_worker.join();
  m_generating.store(false);
}

void AiInterpretationPanel::saveInterpretation() {
  if (m_displayText.empty()) return;

  const std::string model = m_ctx.getSetting("ollama_model", "");
  const std::string defaultFileName = buildInterpretationFileName(currentAnalysisLabel(), model);

  auto path = showSaveFileDialog(
      L"Save AI Interpretation",
      defaultFileName,
      L"Markdown Files (*.md)\0*.md\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0",
      L"md");
  if (!path) {
    return;
  }

  std::ofstream out(*path, std::ios::binary);
  if (!out.is_open()) {
    m_statusText = "Failed to save AI interpretation.";
    m_statusIsError = true;
    return;
  }

  out << m_displayText;
  if (!out.good()) {
    m_statusText = "Failed to write AI interpretation.";
    m_statusIsError = true;
    return;
  }

  m_statusText = "Saved: " + *path;
  m_statusIsError = false;
}

void AiInterpretationPanel::copyInterpretation() {
  if (m_displayText.empty()) return;

  if (clipboard::setMarkdown(m_displayText)) {
    m_statusText = "AI interpretation copied to clipboard.";
    m_statusIsError = false;
  } else {
    m_statusText = "Failed to copy AI interpretation.";
    m_statusIsError = true;
  }
}

void AiInterpretationPanel::copyInterpretationPackage() {
  if (!m_chart && m_displayText.empty()) return;

  std::ostringstream package;
  package << "# " << currentQueryTypeLabel() << "\n\n";
  package << "- **Analysis:** " << currentAnalysisLabel() << "\n";
  const std::string model = m_ctx.getSetting("ollama_model", "");
  if (!model.empty()) {
    package << "- **AI Model:** " << model << "\n";
  }
  package << "- **Generated:** " << generatedAtLabel() << "\n";

  if (m_chart && !m_chart->canonicalHash.empty()) {
    package << "- **Chart Hash:** " << m_chart->canonicalHash << "\n";
  }
  package << "\n";

  const std::string builtIn = deterministicInterpretationMarkdown();
  if (!builtIn.empty()) {
    package << "## Built-In Interpretation\n\n" << builtIn << "\n";
  }
  if (!m_displayText.empty()) {
    package << "## AI Interpretation\n\n" << m_displayText << "\n";
  }
  if (m_chart) {
    package << "## Chart Facts\n\n" << chartFactsBlock() << "\n";
  }

  if (clipboard::setMarkdown(package.str())) {
    m_statusText = "AI interpretation package copied to clipboard.";
    m_statusIsError = false;
  } else {
    m_statusText = "Failed to copy AI interpretation package.";
    m_statusIsError = true;
  }
}

render::ThemePreset AiInterpretationPanel::currentThemePreset() const {
  return render::themePresetByIndex(parseSettingIndex(m_ctx, "default_theme", 0, 4));
}

std::optional<render::ChartScene> AiInterpretationPanel::buildReportScene() const {
  if (!m_chart) return std::nullopt;

  const auto theme = currentThemePreset();
  switch (m_chartType) {
    case domain::ChartType::Natal:
      return render::buildNatalChartScene(*m_chart, theme);
    case domain::ChartType::Synastry:
      if (m_chart->secondaryChart) {
        return render::buildSynastryChartScene(*m_chart, *m_chart->secondaryChart, theme);
      }
      return std::nullopt;
    case domain::ChartType::Composite:
      return render::buildCompositeChartScene(*m_chart, theme);
    case domain::ChartType::TransitToNatal:
      if (m_chart->secondaryChart) {
        return render::buildTransitToNatalChartScene(*m_chart, *m_chart->secondaryChart, theme);
      }
      return std::nullopt;
  }
  return std::nullopt;
}

std::string AiInterpretationPanel::deterministicInterpretationMarkdown() const {
  if (!m_reportIncludeBuiltIn || !m_chart) return {};

  auto result = m_ctx.interpretationService.generateBuiltIn(*m_chart, m_chartType);
  if (!result.ok()) return {};
  return result.value().bodyMarkdown;
}

std::string AiInterpretationPanel::generatedAtLabel() const {
  return localTimestampLabel();
}

core::PdfReportRequest AiInterpretationPanel::buildReportRequest(const std::string& outputPath) const {
  core::PdfReportRequest request;
  request.title = currentQueryTypeLabel();
  request.subtitle = currentAnalysisLabel();
  request.sourceLabel = currentAnalysisLabel();
  const std::string model = m_ctx.getSetting("ollama_model", "");
  request.modelLabel = model.empty() ? std::string("Ollama") : model;
  request.generatedAtLabel = generatedAtLabel();
  request.interpretationMarkdown = m_displayText;
  request.deterministicInterpretationMarkdown = deterministicInterpretationMarkdown();
  if (auto scene = buildReportScene()) {
    request.chartScene = *scene;
  }
  request.theme = currentThemePreset();
  request.computedChartId = m_chart ? m_chart->computedChartId : 0;
  request.outputPath = outputPath;
  request.reportTemplate = m_reportTemplate;
  request.archivalMode = m_reportArchivalMode || m_reportTemplate == core::PdfReportTemplate::ArchiveCopy;
  request.preferVectorChart = m_reportPreferVectorChart;
  return request;
}

void AiInterpretationPanel::drawReportPreview() {
  if (!m_reportPreviewOpen) return;

  bool openPreview = true;
  if (ImGui::BeginPopupModal("Report Preview", &openPreview, ImGuiWindowFlags_AlwaysAutoResize)) {
    const core::PdfReportRequest request = buildReportRequest();
    ImGui::Text("%s", request.title.c_str());
    ImGui::TextDisabled("%s", request.subtitle.c_str());
    ImGui::Separator();
    ImGui::Text("Template: %s", core::pdfReportTemplateLabel(request.reportTemplate));
    ImGui::Text("Chart: %s", buildReportScene() ? (request.preferVectorChart ? "Vector PDF drawing" : "Raster image embed") : "Unavailable");
    ImGui::Text("Built-In Section: %s", request.deterministicInterpretationMarkdown.empty() ? "No" : "Yes");
    ImGui::Text("Archival: %s", request.archivalMode ? "Yes" : "No");
    ImGui::Text("Generated: %s", request.generatedAtLabel.c_str());

    ImGui::Separator();
    ImGui::BeginChild("ReportPreviewBody", ImVec2(560.0f, 360.0f), ImGuiChildFlags_Borders);
    if (!request.deterministicInterpretationMarkdown.empty()) {
      markdown_render::renderMarkdown("## Built-In Interpretation\n\n" + request.deterministicInterpretationMarkdown);
      ImGui::Separator();
    }
    if (!request.interpretationMarkdown.empty()) {
      markdown_render::renderMarkdown("## AI Interpretation\n\n" + request.interpretationMarkdown);
    } else {
      ImGui::TextDisabled("No AI interpretation text is available for the report yet.");
    }
    ImGui::EndChild();

    if (ImGui::Button("Close")) {
      openPreview = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (!openPreview) {
    m_reportPreviewOpen = false;
  }
}

void AiInterpretationPanel::savePdfReport() {
  if (m_displayText.empty()) return;

  auto scene = buildReportScene();
  if (!scene) {
    m_statusText = "No chart is available for this AI report.";
    m_statusIsError = true;
    return;
  }

  const std::string model = m_ctx.getSetting("ollama_model", "");
  const std::string defaultFileName = buildReportFileName(currentAnalysisLabel(), model);

  auto path = showSaveFileDialog(
      L"Save AI Interpretation PDF Report",
      defaultFileName,
      L"PDF Files (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0\0",
      L"pdf");
  if (!path) {
    return;
  }

  core::PdfReportRequest request = buildReportRequest(*path);
  request.chartScene = *scene;

  auto result = m_ctx.reportService.exportPdfReport(request);
  if (!result.ok()) {
    m_statusText = "PDF report failed: " + result.error().message;
    m_statusIsError = true;
    return;
  }

  auto artifact = result.value();
  if (artifact.computedChartId > 0) {
    m_ctx.chartRepo.insertExportArtifact(artifact);
  }

  m_statusText = "PDF report saved: " + artifact.filePath;
  m_statusIsError = false;
}

// ---------------------------------------------------------------------------
// buildPrompt() — constructs the LLM prompt from chart facts
// ---------------------------------------------------------------------------
std::string AiInterpretationPanel::buildPrompt() const {
  if (!m_customPrompt.empty()) {
    std::ostringstream custom;
    custom << m_customPrompt;

    std::string focus(m_focusBuf);
    if (!focus.empty()) {
      custom << "\n\n**Additional user focus:** " << focus << "\n";
    }
    return custom.str();
  }

  std::ostringstream ss;
  ss << "You are an expert astrologer providing a detailed, insightful "
        "interpretation of an astrological chart. Use the chart data below "
        "to produce a comprehensive reading. Format your response in "
        "Markdown with bold section headers.\n\n";

  // Chart type context.
  switch (m_chartType) {
    case domain::ChartType::Natal:
      ss << "This is a **natal (birth) chart** interpretation.\n\n";
      break;
    case domain::ChartType::Synastry:
      ss << "This is a **synastry (relationship compatibility) chart** "
            "interpretation. Compare the two charts.\n\n";
      break;
    case domain::ChartType::Composite:
      ss << "This is a **composite chart** interpretation. The midpoint "
            "positions represent the relationship itself.\n\n";
      break;
    case domain::ChartType::TransitToNatal:
      ss << "This is a **transit-to-natal chart** interpretation. Analyze "
            "how current planetary transits affect the natal chart.\n\n";
      break;
  }

  if (!m_sourceLabel.empty()) {
    ss << "**Analysis target:** " << m_sourceLabel << "\n\n";
  }

  // Uncertainty warnings.
  if (m_chart && !m_chart->uncertaintyFlags.empty()) {
    ss << "**Important caveats:** ";
    for (const auto& f : m_chart->uncertaintyFlags) {
      if (f == "noon_default_applied")
        ss << "Birth time is unknown (noon default used); house placements "
              "and Moon position may be inaccurate. ";
      else if (f == "no_houses")
        ss << "Houses are suppressed because birth time is unavailable. "
              "Do NOT discuss house placements. ";
      else
        ss << f << " ";
    }
    ss << "\n\n";
  }

  ss << chartFactsBlock();

  // User addendum.
  std::string focus(m_focusBuf);
  if (!focus.empty()) {
    ss << "\n**User's focus question:** " << focus << "\n\n";
  }

  ss << "Please provide:\n"
        "1. An overview of dominant themes\n"
        "2. Key planetary placements and their significance\n"
        "3. Notable aspect patterns\n"
        "4. A summary of strengths and challenges\n";

  return ss.str();
}

std::string AiInterpretationPanel::chartFactsBlock() const {
  if (!m_chart) return "";

  std::ostringstream ss;
  ss << "## Chart Data\n\n";
  ss << "- **House System:** " << m_chart->houseSystem << "\n";
  ss << "- **Zodiac:** " << m_chart->zodiacMode << "\n\n";

  ss << "### Planetary Positions\n";
  for (const auto& p : m_chart->planets) {
    ss << "- " << p.objectId << " in " << p.sign
       << " at " << static_cast<int>(p.longitudeDegrees) << "\xC2\xB0";
    if (p.house) ss << " (House " << *p.house << ")";
    if (p.retrograde) ss << " [Rx]";
    ss << "\n";
  }

  if (!m_chart->houseCusps.empty()) {
    ss << "\n### House Cusps\n";
    for (const auto& c : m_chart->houseCusps) {
      ss << "- House " << c.houseNumber << ": "
         << static_cast<int>(c.longitudeDegrees) << "\xC2\xB0\n";
    }
  }

  if (!m_chart->aspects.empty()) {
    ss << "\n### Aspects\n";
    for (const auto& a : m_chart->aspects) {
      ss << "- " << a.objectA << " " << a.aspectType << " " << a.objectB
         << " (orb " << a.orbDegrees << "\xC2\xB0";
      if (a.applyingOrSeparating) ss << ", " << *a.applyingOrSeparating;
      ss << ")\n";
    }
  }

  // Secondary chart (synastry/composite).
  if (m_chart->secondaryChart) {
    ss << "\n### Secondary Chart Positions\n";
    for (const auto& p : m_chart->secondaryChart->planets) {
      ss << "- " << p.objectId << " in " << p.sign
         << " at " << static_cast<int>(p.longitudeDegrees) << "\xC2\xB0";
      if (p.house) ss << " (House " << *p.house << ")";
      if (p.retrograde) ss << " [Rx]";
      ss << "\n";
    }
  }

  if (!m_chart->interAspects.empty()) {
    ss << "\n### Inter-Chart Aspects\n";
    for (const auto& a : m_chart->interAspects) {
      ss << "- " << a.objectA << " " << a.aspectType << " " << a.objectB
         << " (orb " << a.orbDegrees << "\xC2\xB0)\n";
    }
  }

  return ss.str();
}

}  // namespace asteria::ui
