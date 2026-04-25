#include "chart_workspace_panel.h"
#include "app_context.h"
#include "core/birth_event_resolver.h"
#include "render/natal_chart_layout.h"
#include "render/transit_chart_layout.h"
#include "render/astro_glyphs.h"
#include "imgui.h"
#include <cmath>
#include <sstream>
#include <algorithm>
#include <vector>
#include <ctime>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace asteria::ui {

namespace {

// Element band colors (with alpha for sign ring fill)
ImU32 elementColor(render::glyphs::Element e, int alpha = 30) {
  using E = render::glyphs::Element;
  switch (e) {
    case E::Fire:  return IM_COL32(200, 60, 40, alpha);
    case E::Earth: return IM_COL32(80, 140, 60, alpha);
    case E::Air:   return IM_COL32(80, 140, 200, alpha);
    case E::Water: return IM_COL32(60, 80, 180, alpha);
  }
  return IM_COL32(100, 100, 100, alpha);
}

// Draw a filled arc sector between two angles at given inner/outer radius
void drawSignBand(ImDrawList* dl, float cx, float cy, float innerR, float outerR,
                  float startAngle, float endAngle, ImU32 color, int segments = 20) {
  // Build a convex polygon: outer arc CW, then inner arc CCW
  std::vector<ImVec2> pts;
  pts.reserve(static_cast<size_t>(segments) * 2 + 2);
  for (int i = 0; i <= segments; ++i) {
    float t = startAngle + (endAngle - startAngle) * static_cast<float>(i) / static_cast<float>(segments);
    pts.push_back(ImVec2(cx + outerR * cosf(t), cy + outerR * sinf(t)));
  }
  for (int i = segments; i >= 0; --i) {
    float t = startAngle + (endAngle - startAngle) * static_cast<float>(i) / static_cast<float>(segments);
    pts.push_back(ImVec2(cx + innerR * cosf(t), cy + innerR * sinf(t)));
  }
  dl->AddConvexPolyFilled(pts.data(), static_cast<int>(pts.size()), color);
}

// Simple collision avoidance: spread planet label angles so none overlap
struct PlacedPlanet {
  float originalAngle;
  float placedAngle;
  int   index;
};

void spreadPlanetAngles(std::vector<PlacedPlanet>& planets, float minSepRad) {
  // Sort by original angle
  std::sort(planets.begin(), planets.end(),
            [](const PlacedPlanet& a, const PlacedPlanet& b) {
              return a.originalAngle < b.originalAngle;
            });
  // Iterative push-apart
  for (int pass = 0; pass < 5; ++pass) {
    for (size_t i = 1; i < planets.size(); ++i) {
      float diff = planets[i].placedAngle - planets[i - 1].placedAngle;
      if (diff < 0) diff += static_cast<float>(2.0 * M_PI);
      if (diff < minSepRad) {
        float push = (minSepRad - diff) * 0.5f;
        planets[i].placedAngle += push;
        planets[i - 1].placedAngle -= push;
      }
    }
    // Also check wrap-around (last vs first)
    if (planets.size() >= 2) {
      float diff = planets.front().placedAngle + static_cast<float>(2.0 * M_PI)
                   - planets.back().placedAngle;
      if (diff < minSepRad) {
        float push = (minSepRad - diff) * 0.5f;
        planets.front().placedAngle += push;
        planets.back().placedAngle -= push;
      }
    }
  }
}

void drawAspectLines(ImDrawList* drawList, float cx, float cy, float r,
                     const std::optional<domain::ComputedChart>& chart) {
  if (!chart) return;

  for (const auto& asp : chart->aspects) {
    // Find planet positions for the aspect endpoints
    double lonA = -1, lonB = -1;
    for (const auto& p : chart->planets) {
      if (p.objectId == asp.objectA) lonA = p.longitudeDegrees;
      if (p.objectId == asp.objectB) lonB = p.longitudeDegrees;
    }
    if (lonA < 0 || lonB < 0) continue;

    float angA = static_cast<float>(lonA * M_PI / 180.0 - M_PI / 2.0);
    float angB = static_cast<float>(lonB * M_PI / 180.0 - M_PI / 2.0);
    float innerR = r * 0.45f;

    ImU32 color;
    if (asp.aspectType == "Conjunction") color = IM_COL32(220, 200, 60, 160);
    else if (asp.aspectType == "Trine" || asp.aspectType == "Sextile") color = IM_COL32(60, 180, 100, 140);
    else if (asp.aspectType == "Square" || asp.aspectType == "Opposition") color = IM_COL32(200, 60, 60, 140);
    else color = IM_COL32(120, 120, 160, 100);

    drawList->AddLine(
        ImVec2(cx + innerR * cosf(angA), cy + innerR * sinf(angA)),
        ImVec2(cx + innerR * cosf(angB), cy + innerR * sinf(angB)),
        color, 1.0f);
  }
}

}  // anonymous namespace

ChartWorkspacePanel::ChartWorkspacePanel(AppContext& ctx) : m_ctx(ctx) {}

void ChartWorkspacePanel::setSelectedPerson(std::int64_t personId) {
  if (m_personId == personId) return;
  m_personId = personId;
  m_chart.reset();
  m_hasScene = false;
  interpretationText_.clear();
  statusMessage_.clear();
}

render::ThemePreset ChartWorkspacePanel::currentThemePreset() const {
  switch (currentTheme_) {
    case 0: return render::textbookLight();
    case 1: return render::textbookMonochrome();
    case 2: return render::luxuryLight();
    case 3: return render::luxuryDark();
    default: return render::textbookLight();
  }
}

void ChartWorkspacePanel::computeChart() {
  if (m_personId <= 0) {
    statusMessage_ = "Select a person from the Library first.";
    return;
  }

  auto events = m_ctx.birthEventRepo.findByPersonId(m_personId);
  if (events.empty()) {
    statusMessage_ = "No birth event found. Add birth data in the Library panel.";
    return;
  }

  const auto& be = events.front();

  // Look up an associated Location row (if any)
  std::optional<domain::LocationResolution> loc;
  if (be.locationId) {
    loc = m_ctx.locationRepo.findById(*be.locationId);
  }

  domain::ChartRequest req;
  req.primaryBirthEventId = be.birthEventId;
  req.chartType = (currentChartType_ == 0) ? domain::ChartType::Natal
                                           : domain::ChartType::TransitToNatal;

  // Apply persisted settings as defaults
  static const char* kHouseNames[] = {
      "Placidus", "Koch", "Equal", "Whole", "Campanus", "Regiomontanus"};
  int houseIdx = 0;
  try { houseIdx = std::stoi(m_ctx.getSetting("default_house_system", "0")); }
  catch (...) { houseIdx = 0; }
  if (houseIdx < 0 || houseIdx >= static_cast<int>(IM_ARRAYSIZE(kHouseNames))) houseIdx = 0;
  req.defaultHouseSystem = kHouseNames[houseIdx];
  req.zodiacMode = "Tropical";

  // Resolve birth event into numeric engine input
  req.primaryInput = core::resolveBirthInput(be, loc);

  // Apply unknown-time policy from settings
  // 0=noon-default-no-houses, 1=noon-default-with-houses, 2=prompt (treat as 0)
  int policyIdx = 0;
  try { policyIdx = std::stoi(m_ctx.getSetting("unknown_time_policy", "0")); }
  catch (...) { policyIdx = 0; }
  std::string policy = (policyIdx == 1) ? "noon_default_with_warning"
                                        : "unknown_time_no_houses";
  core::applyUnknownTimePolicy(req, be, policy);

  // For Transit-to-Natal, populate transitInput as "now" at the natal location
  if (req.chartType == domain::ChartType::TransitToNatal) {
    std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    domain::ResolvedBirthInput t;
    t.year  = utc.tm_year + 1900;
    t.month = utc.tm_mon + 1;
    t.day   = utc.tm_mday;
    t.timeHours = utc.tm_hour + utc.tm_min / 60.0 + utc.tm_sec / 3600.0;
    t.timezoneHours = 0.0;  // UTC
    t.dstHours = 0.0;
    t.latitudeDeg = req.primaryInput->latitudeDeg;
    t.longitudeDeg = req.primaryInput->longitudeDeg;
    req.transitInput = t;
  }

  // Cache lookup by canonical hash — first compute a "probe" hash by running
  // through the engine's hash format using only inputs (the engine recomputes
  // the same hash). To avoid duplicating the formula here, just call compute
  // and then optionally persist; the engine already returns identical data.
  core::Result<domain::ComputedChart> result =
      (req.chartType == domain::ChartType::TransitToNatal)
          ? m_ctx.comparisonService.computeTransitToNatal(req)
          : m_ctx.natalService.compute(req);

  if (!result.ok()) {
    statusMessage_ = "Computation failed: " + result.error().message;
    m_chart.reset();
    m_hasScene = false;
    return;
  }

  m_chart = result.value();
  m_scene = render::buildNatalChartScene(*m_chart, currentThemePreset());
  m_hasScene = true;
  statusMessage_ = "Chart computed (" + std::to_string(m_chart->planets.size()) +
                   " planets, " + std::to_string(m_chart->aspects.size()) + " aspects)";

  // Generate interpretation
  auto interpResult = m_ctx.interpretationService.generateBuiltIn(
      *m_chart, req.chartType);
  if (interpResult.ok()) {
    interpretationText_ = interpResult.value().bodyMarkdown;
  } else {
    interpretationText_ = "(Interpretation unavailable)";
  }
}

void ChartWorkspacePanel::drawChartCanvas() {
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  float panelWidth = showInterpretation_ ? 260.0f : 0.0f;
  canvasSize.x -= panelWidth;
  if (canvasSize.x < 100) canvasSize.x = 100;
  if (canvasSize.y < 100) canvasSize.y = 100;

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(canvasPos,
      ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
      IM_COL32(20, 20, 24, 255));

  float cx = canvasPos.x + canvasSize.x * 0.5f;
  float cy = canvasPos.y + canvasSize.y * 0.5f;
  float maxR = (canvasSize.x < canvasSize.y ? canvasSize.x : canvasSize.y) * 0.45f;

  auto theme = currentThemePreset();
  ImU32 primaryCol = IM_COL32(theme.primary.r, theme.primary.g, theme.primary.b, 255);
  ImU32 secondaryCol = IM_COL32(theme.secondary.r, theme.secondary.g, theme.secondary.b, 200);
  ImU32 accentCol = IM_COL32(theme.accent.r, theme.accent.g, theme.accent.b, 255);
  ImU32 textCol = IM_COL32(theme.text.r, theme.text.g, theme.text.b, 255);
  ImU32 dimTextCol = IM_COL32(theme.text.r, theme.text.g, theme.text.b, 140);

  float outerR   = maxR;
  float signInR  = maxR * 0.85f;
  float houseOutR = maxR * 0.65f;
  float innerR   = maxR * 0.30f;

  // --- Element-colored sign bands ---
  for (int i = 0; i < 12; ++i) {
    float startAng = static_cast<float>(i) * 30.0f * static_cast<float>(M_PI) / 180.0f
                     - static_cast<float>(M_PI) / 2.0f;
    float endAng = startAng + 30.0f * static_cast<float>(M_PI) / 180.0f;
    auto elem = render::glyphs::signElement(i);
    drawSignBand(drawList, cx, cy, signInR, outerR, startAng, endAng, elementColor(elem, 35));
  }

  // --- Wheel ring outlines ---
  drawList->AddCircle(ImVec2(cx, cy), outerR, primaryCol, 72, 2.0f);
  drawList->AddCircle(ImVec2(cx, cy), signInR, secondaryCol, 72, 1.5f);
  drawList->AddCircle(ImVec2(cx, cy), houseOutR, secondaryCol, 72, 1.0f);
  drawList->AddCircle(ImVec2(cx, cy), innerR, secondaryCol, 72, 1.0f);

  // --- Sign division lines and glyph labels ---
  // Try to use the larger font (index 1) for glyph labels
  ImFont* glyphFont = (ImGui::GetIO().Fonts->Fonts.Size > 1)
                      ? ImGui::GetIO().Fonts->Fonts[1] : nullptr;
  for (int i = 0; i < 12; ++i) {
    float angle = static_cast<float>(i) * 30.0f * static_cast<float>(M_PI) / 180.0f
                  - static_cast<float>(M_PI) / 2.0f;
    float cosA = cosf(angle), sinA = sinf(angle);
    drawList->AddLine(
        ImVec2(cx + houseOutR * cosA, cy + houseOutR * sinA),
        ImVec2(cx + outerR * cosA,    cy + outerR * sinA),
        secondaryCol, 1.0f);

    // Sign glyph label centered in the band
    float midAngle = angle + 15.0f * static_cast<float>(M_PI) / 180.0f;
    float labelR = (outerR + signInR) * 0.5f;
    const char* signGlyph = render::glyphs::sign(i);
    if (glyphFont) {
      drawList->AddText(glyphFont, 20.0f,
          ImVec2(cx + labelR * cosf(midAngle) - 8,
                 cy + labelR * sinf(midAngle) - 10),
          textCol, signGlyph);
    } else {
      drawList->AddText(
          ImVec2(cx + labelR * cosf(midAngle) - 8,
                 cy + labelR * sinf(midAngle) - 6),
          textCol, signGlyph);
    }
  }

  // --- House cusps from computed chart ---
  if (m_chart && !m_chart->houseCusps.empty()) {
    for (const auto& cusp : m_chart->houseCusps) {
      float angle = static_cast<float>(cusp.longitudeDegrees * M_PI / 180.0 - M_PI / 2.0);
      float cosA = cosf(angle), sinA = sinf(angle);
      // Cusp lines: AC/MC axes are thicker
      float thickness = (cusp.houseNumber == 1 || cusp.houseNumber == 4 ||
                         cusp.houseNumber == 7 || cusp.houseNumber == 10)
                        ? 2.0f : 1.0f;
      drawList->AddLine(
          ImVec2(cx + innerR * cosA, cy + innerR * sinA),
          ImVec2(cx + houseOutR * cosA, cy + houseOutR * sinA),
          primaryCol, thickness);
      // House number label
      float numR = (innerR + houseOutR) * 0.5f;
      float nudge = static_cast<float>(7.0 * M_PI / 180.0);
      char houseNum[4];
      snprintf(houseNum, sizeof(houseNum), "%d", cusp.houseNumber);
      drawList->AddText(
          ImVec2(cx + numR * cosf(angle + nudge) - 4,
                 cy + numR * sinf(angle + nudge) - 5),
          dimTextCol, houseNum);
    }
  }

  // --- Planet glyphs with collision avoidance ---
  if (m_chart) {
    // Build placed-planet list for collision avoidance
    std::vector<PlacedPlanet> placed;
    placed.reserve(m_chart->planets.size());
    for (int i = 0; i < static_cast<int>(m_chart->planets.size()); ++i) {
      float ang = static_cast<float>(m_chart->planets[i].longitudeDegrees * M_PI / 180.0 - M_PI / 2.0);
      placed.push_back({ang, ang, i});
    }
    // Minimum separation ~12 degrees at the planet ring radius
    float minSep = 12.0f * static_cast<float>(M_PI) / 180.0f;
    spreadPlanetAngles(placed, minSep);

    float planetR = (signInR + houseOutR) * 0.5f;    // glyph ring
    float tickR   = houseOutR + 4.0f;                 // inner end of tick mark
    float tickEndR = signInR - 4.0f;                  // outer end of tick mark

    for (const auto& pp : placed) {
      const auto& planet = m_chart->planets[pp.index];
      float ang = pp.placedAngle;
      float px = cx + planetR * cosf(ang);
      float py = cy + planetR * sinf(ang);

      // Tick mark at true position (thin line from house ring to sign ring)
      float trueAng = pp.originalAngle;
      drawList->AddLine(
          ImVec2(cx + tickR * cosf(trueAng), cy + tickR * sinf(trueAng)),
          ImVec2(cx + tickEndR * cosf(trueAng), cy + tickEndR * sinf(trueAng)),
          accentCol, 1.0f);

      // Planet glyph
      const char* glyph = render::glyphs::planet(planet.objectId);
      if (glyph && glyphFont) {
        drawList->AddText(glyphFont, 20.0f,
            ImVec2(px - 8, py - 10), accentCol, glyph);
      } else if (glyph) {
        drawList->AddText(ImVec2(px - 6, py - 6), accentCol, glyph);
      } else {
        // Fallback: first 2 chars
        std::string label = planet.objectId.substr(0, 2);
        drawList->AddText(ImVec2(px - 6, py - 6), accentCol, label.c_str());
      }

      // Degree/minute label below the glyph
      std::string degLabel = render::glyphs::formatDegMin(planet.longitudeDegrees);
      drawList->AddText(ImVec2(px - 12, py + 8), dimTextCol, degLabel.c_str());

      // Retrograde marker
      if (planet.retrograde) {
        drawList->AddText(ImVec2(px + 10, py - 10),
            IM_COL32(200, 60, 60, 200), "R");
      }
    }
  } else {
    // No chart computed yet
    drawList->AddText(ImVec2(cx - 120, cy - 8), textCol,
        "Select a person and click Compute");
  }

  // --- Aspect lines inside the inner circle ---
  drawAspectLines(drawList, cx, cy, maxR, m_chart);

  // --- Title overlay ---
  if (m_chart) {
    std::string title = m_chart->houseSystem + " / " + m_chart->zodiacMode;
    drawList->AddText(ImVec2(canvasPos.x + 10, canvasPos.y + 10), textCol, title.c_str());
    if (!m_chart->uncertaintyFlags.empty()) {
      drawList->AddText(ImVec2(canvasPos.x + 10, canvasPos.y + 28),
          IM_COL32(220, 160, 60, 255), "Warning: uncertain birth time");
    }
  }

  ImGui::Dummy(canvasSize);
}

void ChartWorkspacePanel::draw() {
  if (!open) return;
  ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Chart Workspace", &open)) { ImGui::End(); return; }

  // Toolbar
  const char* chartTypes[] = {"Natal", "Transit-to-Natal"};
  ImGui::SetNextItemWidth(160);
  ImGui::Combo("Chart Type", &currentChartType_, chartTypes, IM_ARRAYSIZE(chartTypes));
  ImGui::SameLine();

  const char* themes[] = {"Textbook Light", "Textbook Mono", "Luxury Light", "Luxury Dark"};
  ImGui::SetNextItemWidth(160);
  ImGui::Combo("Theme", &currentTheme_, themes, IM_ARRAYSIZE(themes));
  ImGui::SameLine();

  if (ImGui::Button("Compute")) {
    computeChart();
  }
  ImGui::SameLine();

  bool canExport = m_hasScene;
  if (!canExport) ImGui::BeginDisabled();
  if (ImGui::Button("Export SVG")) {
    auto theme = currentThemePreset();
    auto result = m_ctx.exportService.exportSvg(
        m_scene, m_chart ? m_chart->computedChartId : 0,
        "natal_chart.svg", theme);
    if (result.ok()) {
      statusMessage_ = "Exported: " + result.value().filePath;
    } else {
      statusMessage_ = "Export failed: " + result.error().message;
    }
  }
  if (!canExport) ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::Checkbox("Interpretation", &showInterpretation_);

  // Status line
  if (!statusMessage_.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "%s", statusMessage_.c_str());
  }

  ImGui::Separator();

  // Chart canvas
  drawChartCanvas();

  // Interpretation side panel
  if (showInterpretation_) {
    ImGui::SameLine();
    ImGui::BeginChild("InterpretationSide", ImVec2(250, 0), ImGuiChildFlags_Borders);
    ImGui::Text("Interpretation");
    ImGui::Separator();
    if (interpretationText_.empty()) {
      ImGui::TextWrapped("Select a person from the Library and click "
                         "Compute to see interpretation here.");
    } else {
      ImGui::TextWrapped("%s", interpretationText_.c_str());
    }
    ImGui::EndChild();
  }

  ImGui::End();
}

}  // namespace asteria::ui
