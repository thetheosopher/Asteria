#include "ai_interpretation_panel.h"
#include "app_context.h"
#include "util/ollama_client.h"
#include "imgui.h"

#include <sstream>
#include <vector>

namespace asteria::ui {

// ---------------------------------------------------------------------------
// Simple Markdown-to-ImGui renderer
// ---------------------------------------------------------------------------
namespace {

// Strip leading '#' characters and return the heading level (0 = not a heading).
int detectHeading(const std::string& line, std::string& content) {
  size_t i = 0;
  while (i < line.size() && line[i] == '#') ++i;
  if (i == 0 || i > 4 || (i < line.size() && line[i] != ' ')) {
    content = line;
    return 0;
  }
  content = (i + 1 < line.size()) ? line.substr(i + 1) : "";
  return static_cast<int>(i);
}

// Strip **bold** markers and render a line using ImGui, with color highlights
// for bold spans. Each call renders one visual line (with wrapping).
void renderLineWithBold(const std::string& line, const ImVec4& normalColor,
                        const ImVec4& boldColor) {
  // We can't do true inline bold+normal reflow in ImGui (no rich-text layout),
  // so we strip the markers and render the whole line as TextWrapped, then
  // overlay highlighted bold segments as separate calls if the line is short.
  //
  // Practical approach: build a clean string (markers removed) for wrapping,
  // and a parallel list of bold ranges. For simple rendering, if the line has
  // any bold markers we highlight the entire line in the bold color; otherwise
  // normal. This avoids the broken SameLine approach while keeping visual
  // distinction.
  //
  // Better approach: render segments. For each segment between ** markers,
  // if it's a bold segment, render with boldColor; otherwise normalColor.
  // Use SameLine only within the same visual line, and let TextWrapped handle
  // each segment. This works if we DON'T use SameLine (each segment gets its
  // own line), but that's also ugly.
  //
  // Best practical approach for ImGui: strip markers, render full line as
  // wrapped text. Bold segments get the bold color applied to the entire line
  // if it starts with bold, or we just strip and render normally. LLM output
  // typically puts **heading text** on its own line, so this works well.

  // Fast path: no bold markers at all.
  if (line.find("**") == std::string::npos) {
    ImGui::PushStyleColor(ImGuiCol_Text, normalColor);
    ImGui::TextWrapped("%s", line.c_str());
    ImGui::PopStyleColor();
    return;
  }

  // Strip all ** markers and determine if the majority of the line is bold.
  std::string clean;
  clean.reserve(line.size());
  bool insideBold = false;
  size_t boldChars = 0;
  size_t totalChars = 0;
  size_t i = 0;
  while (i < line.size()) {
    if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
      insideBold = !insideBold;
      i += 2;
      continue;
    }
    clean += line[i];
    totalChars++;
    if (insideBold) boldChars++;
    i++;
  }

  // If most of the content was bold, render in bold color; otherwise normal.
  bool majorityBold = (totalChars > 0 && boldChars * 2 >= totalChars);
  ImGui::PushStyleColor(ImGuiCol_Text, majorityBold ? boldColor : normalColor);
  ImGui::TextWrapped("%s", clean.c_str());
  ImGui::PopStyleColor();
}

// Render a full markdown string into ImGui widgets.
void renderMarkdown(const std::string& text) {
  const ImVec4 colorNormal = ImGui::GetStyleColorVec4(ImGuiCol_Text);
  const ImVec4 colorBold   = ImVec4(1.0f, 0.9f, 0.5f, 1.0f);   // gold
  const ImVec4 colorH1     = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);   // bright blue
  const ImVec4 colorH2     = ImVec4(0.5f, 0.85f, 0.95f, 1.0f);  // lighter blue
  const ImVec4 colorH3     = ImVec4(0.6f, 0.9f, 0.9f, 1.0f);    // cyan
  const ImVec4 colorBullet = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);    // dim

  std::istringstream stream(text);
  std::string line;
  bool prevBlank = false;

  while (std::getline(stream, line)) {
    // Trim trailing \r (from CRLF).
    if (!line.empty() && line.back() == '\r') line.pop_back();

    // Blank line → spacing.
    if (line.empty()) {
      if (!prevBlank) ImGui::Spacing();
      prevBlank = true;
      continue;
    }
    prevBlank = false;

    // Heading detection.
    std::string headingContent;
    int level = detectHeading(line, headingContent);
    if (level > 0) {
      // Strip bold markers from heading text.
      std::string clean;
      for (size_t i = 0; i < headingContent.size(); ++i) {
        if (i + 1 < headingContent.size() &&
            headingContent[i] == '*' && headingContent[i + 1] == '*') {
          i++; continue;
        }
        clean += headingContent[i];
      }
      ImGui::Spacing();
      float scale = (level == 1) ? 1.4f : (level == 2) ? 1.2f : 1.05f;
      ImVec4 col = (level == 1) ? colorH1 : (level == 2) ? colorH2 : colorH3;
      ImGui::SetWindowFontScale(scale);
      ImGui::PushStyleColor(ImGuiCol_Text, col);
      ImGui::TextWrapped("%s", clean.c_str());
      ImGui::PopStyleColor();
      ImGui::SetWindowFontScale(1.0f);
      if (level <= 2) {
        // Underline for H1/H2.
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImVec4 dimCol = col; dimCol.w = 0.3f;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(p.x, p.y - 2), ImVec2(p.x + w, p.y - 2),
            ImGui::GetColorU32(dimCol), 1.0f);
      }
      continue;
    }

    // Bullet list item (- or *).
    if ((line.size() >= 2) &&
        (line[0] == '-' || line[0] == '*') && line[1] == ' ') {
      std::string content = line.substr(2);
      ImGui::PushStyleColor(ImGuiCol_Text, colorBullet);
      ImGui::TextUnformatted("  \xE2\x80\xA2");  // bullet U+2022
      ImGui::PopStyleColor();
      ImGui::SameLine();
      renderLineWithBold(content, colorNormal, colorBold);
      continue;
    }

    // Numbered list item (1. 2. etc.).
    if (line.size() >= 3 && line[0] >= '1' && line[0] <= '9' && line[1] == '.') {
      size_t sp = line.find(' ');
      if (sp != std::string::npos && sp < 5) {
        std::string prefix = line.substr(0, sp + 1);
        std::string content = line.substr(sp + 1);
        ImGui::PushStyleColor(ImGuiCol_Text, colorBullet);
        ImGui::TextUnformatted(prefix.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        renderLineWithBold(content, colorNormal, colorBold);
        continue;
      }
    }

    // Horizontal rule (---, ***, ___).
    if (line.size() >= 3 &&
        (line == "---" || line == "***" || line == "___" ||
         line.find_first_not_of('-') == std::string::npos ||
         line.find_first_not_of('*') == std::string::npos)) {
      ImGui::Separator();
      continue;
    }

    // Regular paragraph line.
    renderLineWithBold(line, colorNormal, colorBold);
  }
}

}  // namespace

AiInterpretationPanel::AiInterpretationPanel(AppContext& ctx) : m_ctx(ctx) {}

AiInterpretationPanel::~AiInterpretationPanel() {
  stopGeneration();
}

void AiInterpretationPanel::setChart(const domain::ComputedChart& chart,
                                     domain::ChartType type) {
  m_chart = chart;
  m_chartType = type;
}

// ---------------------------------------------------------------------------
// draw() — main UI thread
// ---------------------------------------------------------------------------
void AiInterpretationPanel::draw() {
  if (!open) return;

  // Check if Ollama is enabled.
  bool ollamaEnabled = m_ctx.getSetting("ollama_enabled", "0") == "1";

  ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("AI Interpretation", &open)) { ImGui::End(); return; }

  if (!ollamaEnabled) {
    ImGui::TextWrapped("Ollama integration is disabled. Enable it in Settings and "
                       "select a model to use AI-generated interpretations.");
    ImGui::End();
    return;
  }

  std::string model = m_ctx.getSetting("ollama_model", "");
  if (model.empty()) {
    ImGui::TextWrapped("No Ollama model selected. Open Settings and choose a model "
                       "from the Ollama Model dropdown.");
    ImGui::End();
    return;
  }

  // Status header.
  ImGui::Text("Model: %s", model.c_str());
  if (!m_chart) {
    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
        "No chart computed yet. Compute a chart first.");
  }
  ImGui::Separator();

  // Optional focus question.
  ImGui::Text("Focus (optional):");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-120.0f);
  ImGui::InputText("##AiFocus", m_focusBuf, sizeof(m_focusBuf));

  ImGui::SameLine();
  bool generating = m_generating.load();
  if (generating) {
    if (ImGui::Button("Stop")) {
      stopGeneration();
    }
  } else {
    bool canGenerate = m_chart.has_value();
    if (!canGenerate) ImGui::BeginDisabled();
    if (ImGui::Button("Generate")) {
      startGeneration();
    }
    if (!canGenerate) ImGui::EndDisabled();
  }

  // Snapshot the shared streaming state.
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_displayText = m_streamedText;
    m_displayError = m_errorText;
  }

  // Error display.
  if (!m_displayError.empty()) {
    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", m_displayError.c_str());
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
    renderMarkdown(m_displayText);

    // Auto-scroll while generating.
    if (generating && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f) {
      ImGui::SetScrollHereY(1.0f);
    }
  } else if (!generating) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        "Click \"Generate\" to request an AI interpretation of the current chart.");
  }

  ImGui::EndChild();
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

// ---------------------------------------------------------------------------
// buildPrompt() — constructs the LLM prompt from chart facts
// ---------------------------------------------------------------------------
std::string AiInterpretationPanel::buildPrompt() const {
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
