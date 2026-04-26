#include "chart_workspace_panel.h"
#include "ai_interpretation_panel.h"
#include "app_context.h"
#include "astrology_font.h"
#include "export_options.h"
#include "file_dialog.h"
#include "markdown_render.h"
#include "core/birth_event_resolver.h"
#include "render/export_layout_templates.h"
#include "render/natal_chart_layout.h"
#include "render/transit_chart_layout.h"
#include "render/astro_glyphs.h"
#include "imgui.h"
#include <array>
#include <cmath>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <vector>
#include <ctime>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace asteria::ui {

namespace {

void leftLabel(const char* text, float width = 92.0f) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(text);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(width);
}

ImVec2 polarPoint(float cx, float cy, float radius, float angle) {
  return ImVec2(cx + radius * cosf(angle), cy + radius * sinf(angle));
}

ImVec2 radialUnit(float angle) {
  return ImVec2(cosf(angle), sinf(angle));
}

ImVec2 tangentUnit(float angle) {
  return ImVec2(-sinf(angle), cosf(angle));
}

float angleDistance(float a, float b) {
  return std::fabs(std::atan2(std::sin(a - b), std::cos(a - b)));
}

bool shouldRenderOnWheel(const std::string& objectId) {
  static const std::array<const char*, 10> kWheelObjects = {
      "Sun", "Moon", "Mercury", "Venus", "Mars",
      "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto",
  };
  for (const char* name : kWheelObjects) {
    if (objectId == name) return true;
  }
  return false;
}

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

void drawAspectLines(ImDrawList* drawList, float cx, float cy, float anchorR,
                     float endpointRadius,
                     const std::optional<domain::ComputedChart>& chart) {
  if (!chart) return;

  for (const auto& asp : chart->aspects) {
    if (!shouldRenderOnWheel(asp.objectA) || !shouldRenderOnWheel(asp.objectB)) continue;

    // Find planet positions for the aspect endpoints
    double lonA = -1, lonB = -1;
    for (const auto& p : chart->planets) {
      if (p.objectId == asp.objectA) lonA = p.longitudeDegrees;
      if (p.objectId == asp.objectB) lonB = p.longitudeDegrees;
    }
    if (lonA < 0 || lonB < 0) continue;

    float angA = static_cast<float>(lonA * M_PI / 180.0 - M_PI / 2.0);
    float angB = static_cast<float>(lonB * M_PI / 180.0 - M_PI / 2.0);

    ImU32 color;
    if (asp.aspectType == "Conjunction") color = IM_COL32(220, 200, 60, 160);
    else if (asp.aspectType == "Trine" || asp.aspectType == "Sextile") color = IM_COL32(60, 180, 100, 140);
    else if (asp.aspectType == "Square" || asp.aspectType == "Opposition") color = IM_COL32(200, 60, 60, 140);
    else color = IM_COL32(120, 120, 160, 100);

    const ImVec2 p1 = polarPoint(cx, cy, anchorR, angA);
    const ImVec2 p2 = polarPoint(cx, cy, anchorR, angB);

    drawList->AddLine(p1, p2, color, 1.0f);
    drawList->AddCircleFilled(p1, endpointRadius, color);
    drawList->AddCircleFilled(p2, endpointRadius, color);
  }
}

std::string formatCoord(double value, bool latitude) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(4) << std::abs(value)
      << (latitude ? (value >= 0.0 ? " N" : " S")
                   : (value >= 0.0 ? " E" : " W"));
  return out.str();
}

std::string formatUtcOffset(double hours) {
  const double absHours = std::abs(hours);
  const int wholeHours = static_cast<int>(absHours);
  const int minutes = static_cast<int>((absHours - wholeHours) * 60.0 + 0.5);

  char buf[32];
  std::snprintf(buf, sizeof(buf), "UTC%c%02d:%02d", hours >= 0.0 ? '-' : '+', wholeHours, minutes);
  return buf;
}

const char* timeAccuracyLabel(domain::TimeAccuracy accuracy) {
  switch (accuracy) {
    case domain::TimeAccuracy::Exact: return "Exact";
    case domain::TimeAccuracy::Approximate: return "Approximate";
    case domain::TimeAccuracy::Unknown: return "Unknown";
  }
  return "Unknown";
}

void drawSectionHeader(const char* title, ImU32 accentCol, ImU32 textCol) {
  ImGui::Spacing();
  ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(accentCol), "%s", title);
  ImGui::Separator();
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(textCol));
}

void endSectionHeader() {
  ImGui::PopStyleColor();
}

void drawInfoRow(const char* label, const std::string& value, ImU32 labelCol, ImU32 valueCol) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(labelCol));
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  ImGui::SameLine(130.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(valueCol));
  ImGui::TextWrapped("%s", value.c_str());
  ImGui::PopStyleColor();
}

void pushInfoTableStyles(ImU32 textCol, ImU32 mutedCol, ImU32 accentCol, const ImVec4& panelBg) {
  const ImVec4 text = ImGui::ColorConvertU32ToFloat4(textCol);
  const ImVec4 muted = ImGui::ColorConvertU32ToFloat4(mutedCol);
  const ImVec4 accent = ImGui::ColorConvertU32ToFloat4(accentCol);
  const ImVec4 rowBg(panelBg.x + 0.015f, panelBg.y + 0.015f, panelBg.z + 0.02f, 1.0f);
  const ImVec4 rowBgAlt(panelBg.x + 0.03f, panelBg.y + 0.03f, panelBg.z + 0.035f, 1.0f);
  const ImVec4 border(muted.x, muted.y, muted.z, 0.45f);
  const ImVec4 headerBg(accent.x, accent.y, accent.z, 0.18f);
  const ImVec4 headerActive(accent.x, accent.y, accent.z, 0.28f);

  ImGui::PushStyleColor(ImGuiCol_Text, text);
  ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, headerBg);
  ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, border);
  ImGui::PushStyleColor(ImGuiCol_TableBorderLight, border);
  ImGui::PushStyleColor(ImGuiCol_TableRowBg, rowBg);
  ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, rowBgAlt);
  ImGui::PushStyleColor(ImGuiCol_Header, headerBg);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerActive);
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerActive);
}

void popInfoTableStyles() {
  ImGui::PopStyleColor(9);
}

std::string signNameForLongitude(double longitude) {
  static const char* kSignNames[] = {
    "Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo",
    "Libra", "Scorpio", "Sagittarius", "Capricorn", "Aquarius", "Pisces"
  };
  int signIndex = static_cast<int>(std::floor(longitude / 30.0)) % 12;
  if (signIndex < 0) signIndex += 12;
  return kSignNames[signIndex];
}

int signIndexForLongitude(double longitude) {
  int signIndex = static_cast<int>(std::floor(longitude / 30.0)) % 12;
  if (signIndex < 0) signIndex += 12;
  return signIndex;
}

std::string formatLongitudeWithSign(double longitude) {
  return render::glyphs::formatPosition(longitude, signNameForLongitude(longitude));
}

void drawLongitudeValue(double longitude, ImU32 glyphCol, ImU32 textCol) {
  double signDeg = std::fmod(longitude, 30.0);
  if (signDeg < 0.0) signDeg += 30.0;
  const int deg = static_cast<int>(signDeg);
  const int min = static_cast<int>((signDeg - deg) * 60.0);
  const char* signGlyph = astrology_font::signGlyph(signIndexForLongitude(longitude));

  std::ostringstream prefix;
  prefix << deg << "\xC2\xB0";
  std::ostringstream suffix;
  suffix << std::setw(2) << std::setfill('0') << min << "'";

  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(textCol));
  ImGui::TextUnformatted(prefix.str().c_str());
  ImGui::PopStyleColor();
  ImGui::SameLine(0.0f, 3.0f);
  astrology_font::drawInlineGlyphLabel(signGlyph, suffix.str(), glyphCol, textCol, 3.0f);
}

void drawLongitudeInfoRow(const char* label, double longitude, ImU32 labelCol, ImU32 glyphCol, ImU32 valueCol) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(labelCol));
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  ImGui::SameLine(130.0f);
  drawLongitudeValue(longitude, glyphCol, valueCol);
}

void drawPlanetLabelCell(const std::string& objectId, ImU32 glyphCol, ImU32 textCol) {
  astrology_font::drawInlineGlyphLabel(astrology_font::planetGlyph(objectId), objectId, glyphCol, textCol);
}

void drawAspectLabelCell(const std::string& aspectType, ImU32 glyphCol, ImU32 textCol) {
  astrology_font::drawInlineGlyphLabel(astrology_font::aspectGlyph(aspectType), aspectType, glyphCol, textCol);
}

int parseSettingIndex(const AppContext& ctx, const char* key, int fallback, int maxExclusive) {
  int value = fallback;
  try { value = std::stoi(ctx.getSetting(key, std::to_string(fallback))); }
  catch (...) { value = fallback; }
  if (value < 0 || value >= maxExclusive) return fallback;
  return value;
}

std::optional<double> findHouseLongitude(const domain::ComputedChart& chart, int houseNumber) {
  auto it = std::find_if(chart.houseCusps.begin(), chart.houseCusps.end(),
                         [&](const domain::HouseCusp& cusp) {
                           return cusp.houseNumber == houseNumber;
                         });
  if (it == chart.houseCusps.end()) return std::nullopt;
  return it->longitudeDegrees;
}

std::string birthTimeLabel(const domain::BirthEvent& birthEvent) {
  if (birthEvent.birthTime && !birthEvent.birthTime->empty()) return *birthEvent.birthTime;
  if (birthEvent.noonDefaultApplied) return "12:00 (defaulted)";
  return "Unknown";
}

std::string chartTypeLabel(int chartTypeIndex) {
  return chartTypeIndex == 0 ? "Natal" : "Transit-to-Natal";
}

std::string formatLongitudePlain(double longitude) {
  return render::glyphs::formatDegMin(longitude) + " " + signNameForLongitude(longitude);
}

}  // anonymous namespace

ChartWorkspacePanel::ChartWorkspacePanel(AppContext& ctx) : m_ctx(ctx) {
  exportPngProfile_ = parseSettingIndex(m_ctx, "default_export_png_profile", 0, 3);
  exportLayoutTemplate_ = parseSettingIndex(m_ctx, "default_export_layout", 0, 2);
}

void ChartWorkspacePanel::setSelectedPerson(std::int64_t personId) {
  if (m_personId == personId) return;
  m_personId = personId;
  m_person.reset();
  m_birthEvent.reset();
  m_location.reset();
  m_chart.reset();
  m_lastComputedAiSourceLabel_.clear();
  m_hasScene = false;
  interpretationText_.clear();
  statusMessage_.clear();

  if (m_personId <= 0) {
    m_focusOnNextDraw = false;
    return;
  }

  currentChartType_ = 0;
  open = true;
  m_focusOnNextDraw = true;
  computeChart();
}

render::ThemePreset ChartWorkspacePanel::currentThemePreset() const {
  return render::themePresetByIndex(parseSettingIndex(m_ctx, "default_theme", 0, 4));
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

  m_person = m_ctx.personRepo.findById(m_personId);
  const auto& be = events.front();
  m_birthEvent = be;

  // Look up an associated Location row (if any)
  std::optional<domain::LocationResolution> loc;
  if (be.locationId) {
    loc = m_ctx.locationRepo.findById(*be.locationId);
  }
  m_location = loc;

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
  core::Result<domain::ComputedChart> result = [&]() {
    std::lock_guard<std::mutex> engineLock(m_ctx.engineMutex);
    return (req.chartType == domain::ChartType::TransitToNatal)
        ? m_ctx.comparisonService.computeTransitToNatal(req)
        : m_ctx.natalService.compute(req);
  }();

  if (!result.ok()) {
    statusMessage_ = "Computation failed: " + result.error().message;
    m_chart.reset();
    m_hasScene = false;
    return;
  }

  m_chart = result.value();
  if (req.chartType == domain::ChartType::TransitToNatal && m_chart->secondaryChart) {
    m_scene = render::buildTransitToNatalChartScene(*m_chart, *m_chart->secondaryChart, currentThemePreset());
  } else {
    m_scene = render::buildNatalChartScene(*m_chart, currentThemePreset());
  }
  m_hasScene = true;
  const std::string subjectLabel = m_person
      ? (m_person->displayName.empty() ? m_person->fullName : m_person->displayName)
      : std::string("selected person");
  m_lastComputedAiChartType_ = req.chartType;
  m_lastComputedAiSourceLabel_ = (req.chartType == domain::ChartType::TransitToNatal)
      ? ("Transit-to-natal chart for " + subjectLabel)
      : ("Natal chart for " + subjectLabel);
  statusMessage_ = "Chart computed (" + std::to_string(m_chart->planets.size()) +
                   " planets, " + std::to_string(m_chart->aspects.size()) + " aspects)";

  // Push chart to AI panel if connected.
  if (m_aiPanel) {
    m_aiPanel->setChart(*m_chart, req.chartType, m_lastComputedAiSourceLabel_);
  }

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
  if (canvasSize.x < 100) canvasSize.x = 100;
  if (canvasSize.y < 100) canvasSize.y = 100;

  auto theme = currentThemePreset();
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(canvasPos,
      ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
      IM_COL32(theme.background.r, theme.background.g, theme.background.b, 255));

  float cx = canvasPos.x + canvasSize.x * 0.5f;
  float cy = canvasPos.y + canvasSize.y * 0.5f;
  float maxR = (canvasSize.x < canvasSize.y ? canvasSize.x : canvasSize.y) * 0.45f;

  ImU32 primaryCol = IM_COL32(theme.primary.r, theme.primary.g, theme.primary.b, 255);
  ImU32 secondaryCol = IM_COL32(theme.secondary.r, theme.secondary.g, theme.secondary.b, 200);
  ImU32 accentCol = IM_COL32(theme.accent.r, theme.accent.g, theme.accent.b, 255);
  ImU32 textCol = IM_COL32(theme.text.r, theme.text.g, theme.text.b, 255);
  ImU32 dimTextCol = IM_COL32(theme.text.r, theme.text.g, theme.text.b, 140);

  float outerR   = maxR;
  float signInR  = maxR * 0.85f;
  float houseOutR = maxR * 0.65f;
  float innerR   = maxR * 0.30f;
  float aspectAnchorR = houseOutR - maxR * 0.035f;
  float planetR = houseOutR + (signInR - houseOutR) * 0.40f;
  float degreeLabelR = planetR - maxR * 0.048f;
  float leaderStartR = planetR - maxR * 0.03f;
  float endpointDotR = std::max(2.0f, maxR * 0.007f);

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
  ImFont* glyphFont = astrology_font::wheelGlyphFont();
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
    const char* signGlyph = astrology_font::wheelSignGlyph(i);
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

  // --- Aspect lines inside the wheel ---
  drawAspectLines(drawList, cx, cy, aspectAnchorR, endpointDotR, m_chart);

  // --- Planet glyphs with collision avoidance ---
  if (m_chart) {
    // Build placed-planet list for collision avoidance
    std::vector<PlacedPlanet> placed;
    placed.reserve(m_chart->planets.size());
    for (int i = 0; i < static_cast<int>(m_chart->planets.size()); ++i) {
      if (!shouldRenderOnWheel(m_chart->planets[i].objectId)) continue;
      float ang = static_cast<float>(m_chart->planets[i].longitudeDegrees * M_PI / 180.0 - M_PI / 2.0);
      placed.push_back({ang, ang, i});
    }
    // Minimum separation ~12 degrees at the planet ring radius
    float minSep = 12.0f * static_cast<float>(M_PI) / 180.0f;
    spreadPlanetAngles(placed, minSep);

    float tickR   = houseOutR + 4.0f;                 // inner end of tick mark
    float tickEndR = signInR - 4.0f;                  // outer end of tick mark

    for (const auto& pp : placed) {
      const auto& planet = m_chart->planets[pp.index];
      float ang = pp.placedAngle;
      const ImVec2 glyphPos = polarPoint(cx, cy, planetR, ang);
      const ImVec2 radial = radialUnit(ang);
      const ImVec2 tangent = tangentUnit(ang);

      // Tick mark at true position (thin line from house ring to sign ring)
      float trueAng = pp.originalAngle;
      drawList->AddLine(
          ImVec2(cx + tickR * cosf(trueAng), cy + tickR * sinf(trueAng)),
          ImVec2(cx + tickEndR * cosf(trueAng), cy + tickEndR * sinf(trueAng)),
          accentCol, 1.0f);

      // If the glyph was displaced for collision avoidance, draw a subtle leader
      // from the true longitude marker toward the shifted glyph.
      if (angleDistance(ang, trueAng) > (2.0f * static_cast<float>(M_PI) / 180.0f)) {
        const ImVec2 leaderStart = polarPoint(cx, cy, leaderStartR, trueAng);
        const ImVec2 leaderEnd(glyphPos.x - radial.x * 8.0f,
                               glyphPos.y - radial.y * 8.0f);
        drawList->AddLine(leaderStart, leaderEnd, secondaryCol, 1.0f);
      }

      // Planet glyph
      const char* glyph = astrology_font::wheelPlanetGlyph(planet.objectId);
      if (glyph && glyphFont) {
        drawList->AddText(glyphFont, 20.0f,
            ImVec2(glyphPos.x - 8, glyphPos.y - 10), accentCol, glyph);
      } else if (glyph) {
        drawList->AddText(ImVec2(glyphPos.x - 6, glyphPos.y - 6), accentCol, glyph);
      } else {
        // Fallback: first 2 chars
        std::string label = planet.objectId.substr(0, 2);
        drawList->AddText(ImVec2(glyphPos.x - 6, glyphPos.y - 6), accentCol, label.c_str());
      }

      // Degree/minute label on a dedicated inner radial ring so the stack stays
      // visually aligned at every angle.
      std::string degLabel = render::glyphs::formatDegMin(planet.longitudeDegrees);
      const ImVec2 degreePos = polarPoint(cx, cy, degreeLabelR, ang);
      drawList->AddText(ImVec2(degreePos.x - 12, degreePos.y - 5), dimTextCol, degLabel.c_str());

      // Retrograde marker offset tangentially so it doesn't collapse into the
      // degree label when the planet stack rotates around the wheel.
      if (planet.retrograde) {
        const ImVec2 retrogradePos(glyphPos.x + radial.x * 4.0f + tangent.x * 11.0f,
                                   glyphPos.y + radial.y * 4.0f + tangent.y * 11.0f);
        const char* retrogradeGlyph = astrology_font::wheelRetrogradeGlyph();
        if (astrology_font::usingLegacyWheelFont() && glyphFont) {
          drawList->AddText(glyphFont, 15.0f,
              ImVec2(retrogradePos.x - 4.0f, retrogradePos.y - 7.0f),
              IM_COL32(200, 60, 60, 200), retrogradeGlyph);
        } else {
          drawList->AddText(ImVec2(retrogradePos.x - 3.0f, retrogradePos.y - 5.0f),
              IM_COL32(200, 60, 60, 200), retrogradeGlyph);
        }
      }
    }
  } else {
    // No chart computed yet
    drawList->AddText(ImVec2(cx - 120, cy - 8), textCol,
        "Select a person and click Compute");
  }

  // --- Title overlay ---
  if (m_chart && !m_chart->uncertaintyFlags.empty()) {
    drawList->AddText(ImVec2(canvasPos.x + 10, canvasPos.y + 10),
        IM_COL32(220, 160, 60, 255), "Warning: uncertain birth time");
  }

  ImGui::Dummy(canvasSize);
}

void ChartWorkspacePanel::drawInfoSidePanel() {
  auto theme = currentThemePreset();
  const ImU32 accentCol = IM_COL32(theme.accent.r, theme.accent.g, theme.accent.b, 255);
  const ImU32 headingCol = IM_COL32(theme.primary.r, theme.primary.g, theme.primary.b, 255);
  const ImU32 textCol = IM_COL32(theme.text.r, theme.text.g, theme.text.b, 255);
  const ImU32 mutedCol = IM_COL32(theme.secondary.r, theme.secondary.g, theme.secondary.b, 255);
  const ImVec4 panelBg(
      std::min(theme.background.r + 6, 255) / 255.0f,
      std::min(theme.background.g + 6, 255) / 255.0f,
      std::min(theme.background.b + 8, 255) / 255.0f,
      1.0f);

  ImGui::PushStyleColor(ImGuiCol_ChildBg, panelBg);
  ImGui::BeginChild("NatalInfoSide", ImVec2(0, 0), ImGuiChildFlags_Borders);
  ImGui::PopStyleColor();

  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(headingCol));
  ImGui::TextUnformatted(currentChartType_ == 0 ? "Natal Chart Notes" : "Transit-to-Natal Notes");
  ImGui::PopStyleColor();
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(mutedCol));
  if (currentChartType_ == 0) {
    ImGui::TextWrapped("Person, birthplace, chart settings, and object positions in a compact reference panel.");
  } else {
    ImGui::TextWrapped("Natal baseline with active transit context and inter-aspects for the current moment.");
  }
  ImGui::PopStyleColor();

  drawSectionHeader("Person", accentCol, textCol);
  drawInfoRow("Name", m_person ? m_person->displayName : std::string("No person selected"), mutedCol, textCol);
  if (m_birthEvent) {
    drawInfoRow("Birth Date", m_birthEvent->birthDate.empty() ? std::string("Unknown") : m_birthEvent->birthDate, mutedCol, textCol);
    drawInfoRow("Birth Time",
                m_birthEvent->birthTime ? *m_birthEvent->birthTime : std::string("Unknown / noon default"),
                mutedCol, textCol);
    drawInfoRow("Accuracy", timeAccuracyLabel(m_birthEvent->timeAccuracy), mutedCol, textCol);
  }
  endSectionHeader();

  drawSectionHeader("Location", accentCol, textCol);
  if (m_birthEvent || m_location) {
    const std::string locationName = m_location ? m_location->displayName :
        (m_birthEvent && !m_birthEvent->cityInput.empty() ? m_birthEvent->cityInput : std::string("Unknown"));
    drawInfoRow("Place", locationName, mutedCol, textCol);

    const double latitude = m_location ? m_location->latitude : (m_birthEvent && m_birthEvent->latitudeDeg ? *m_birthEvent->latitudeDeg : 0.0);
    const double longitude = m_location ? m_location->longitude : (m_birthEvent && m_birthEvent->longitudeDeg ? *m_birthEvent->longitudeDeg : 0.0);
    drawInfoRow("Latitude", formatCoord(latitude, true), mutedCol, textCol);
    drawInfoRow("Longitude", formatCoord(longitude, false), mutedCol, textCol);

    std::string zoneName = m_location ? m_location->timezoneName :
        (m_birthEvent && m_birthEvent->timezoneName ? *m_birthEvent->timezoneName : std::string());
    if (zoneName.empty()) zoneName = "Manual / unresolved";
    drawInfoRow("Time Zone", zoneName, mutedCol, textCol);

    if (m_birthEvent && m_birthEvent->timezoneOffsetHours) {
      drawInfoRow("Offset", formatUtcOffset(*m_birthEvent->timezoneOffsetHours), mutedCol, textCol);
    }
    if (m_birthEvent && m_birthEvent->dstOffsetHours) {
      std::ostringstream dst;
      dst << *m_birthEvent->dstOffsetHours << "h";
      drawInfoRow("DST", dst.str(), mutedCol, textCol);
    }
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(mutedCol));
    ImGui::TextWrapped("Compute a natal chart to populate location details.");
    ImGui::PopStyleColor();
  }
  endSectionHeader();

  drawSectionHeader("Chart", accentCol, textCol);
  if (m_chart) {
    drawInfoRow("Mode", chartTypeLabel(currentChartType_), mutedCol, textCol);
    drawInfoRow("House System", m_chart->houseSystem, mutedCol, textCol);
    drawInfoRow("Zodiac", m_chart->zodiacMode, mutedCol, textCol);
    drawInfoRow("Engine", m_chart->engineMethod.empty() ? std::string("Astrolog") : m_chart->engineMethod, mutedCol, textCol);
    drawInfoRow("Planets", std::to_string(m_chart->planets.size()), mutedCol, textCol);
    if (m_chart->secondaryChart) {
      drawInfoRow("Transit Planets", std::to_string(m_chart->secondaryChart->planets.size()), mutedCol, textCol);
      drawInfoRow("Inter-Aspects", std::to_string(m_chart->interAspects.size()), mutedCol, textCol);
    }
    drawInfoRow("Aspects", std::to_string(m_chart->aspects.size()), mutedCol, textCol);
    if (!m_chart->uncertaintyFlags.empty()) {
      drawInfoRow("Warnings", m_chart->uncertaintyFlags.front(), mutedCol, textCol);
    }
  }
  endSectionHeader();

  drawSectionHeader("Angles", accentCol, textCol);
  if (m_chart && !m_chart->houseCusps.empty()) {
    auto drawAngle = [&](const char* label, int houseNumber) {
      auto it = std::find_if(m_chart->houseCusps.begin(), m_chart->houseCusps.end(),
                             [&](const domain::HouseCusp& cusp) {
                               return cusp.houseNumber == houseNumber;
                             });
      if (it != m_chart->houseCusps.end()) {
        drawLongitudeInfoRow(label, it->longitudeDegrees, mutedCol, accentCol, textCol);
      }
    };
    drawAngle("Ascendant", 1);
    drawAngle("IC", 4);
    drawAngle("Descendant", 7);
    drawAngle("Midheaven", 10);
  }
  endSectionHeader();

  drawSectionHeader("Object Positions", accentCol, textCol);
  pushInfoTableStyles(textCol, mutedCol, accentCol, panelBg);
  if (m_chart && ImGui::BeginTable("PlanetPositions", 4,
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
        ImVec2(-1, 220))) {
    ImGui::TableSetupColumn("Body", ImGuiTableColumnFlags_WidthFixed, 86.0f);
    ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("House", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Speed", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableHeadersRow();

    for (const auto& planet : m_chart->planets) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      drawPlanetLabelCell(planet.objectId, accentCol, textCol);

      ImGui::TableSetColumnIndex(1);
      drawLongitudeValue(planet.longitudeDegrees, accentCol, textCol);

      ImGui::TableSetColumnIndex(2);
      if (planet.house) ImGui::Text("%d", *planet.house);
      else ImGui::TextDisabled("-");

      ImGui::TableSetColumnIndex(3);
      std::ostringstream speed;
      speed << std::fixed << std::setprecision(2) << planet.speed;
      if (planet.retrograde) speed << " R";
      ImGui::TextUnformatted(speed.str().c_str());
    }
    ImGui::EndTable();
  }
  popInfoTableStyles();
  endSectionHeader();

  drawSectionHeader("Major Aspects", accentCol, textCol);
  pushInfoTableStyles(textCol, mutedCol, accentCol, panelBg);
  if (m_chart && ImGui::BeginTable("AspectTable", 5,
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
        ImVec2(-1, 170))) {
    ImGui::TableSetupColumn("A", ImGuiTableColumnFlags_WidthStretch, 1.2f);
    ImGui::TableSetupColumn("Aspect", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("B", ImGuiTableColumnFlags_WidthStretch, 1.2f);
    ImGui::TableSetupColumn("Orb", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn("Phase", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableHeadersRow();

    if (m_chart) {
      for (const auto& asp : m_chart->aspects) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        drawPlanetLabelCell(asp.objectA, accentCol, textCol);
        ImGui::TableSetColumnIndex(1);
        drawAspectLabelCell(asp.aspectType, accentCol, textCol);
        ImGui::TableSetColumnIndex(2);
        drawPlanetLabelCell(asp.objectB, accentCol, textCol);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.2f", asp.orbDegrees);
        ImGui::TableSetColumnIndex(4);
        if (asp.applyingOrSeparating) ImGui::TextUnformatted(asp.applyingOrSeparating->c_str());
        else ImGui::TextDisabled("-");
      }
    }
    ImGui::EndTable();
  }
  popInfoTableStyles();
  endSectionHeader();

  drawSectionHeader("House Cusps", accentCol, textCol);
  pushInfoTableStyles(textCol, mutedCol, accentCol, panelBg);
  if (m_chart && ImGui::CollapsingHeader("House Cusps Table", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::BeginTable("HouseCusps", 2,
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
          ImVec2(-1, 180))) {
      ImGui::TableSetupColumn("House", ImGuiTableColumnFlags_WidthFixed, 60.0f);
      ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch, 1.0f);
      ImGui::TableHeadersRow();
      for (const auto& cusp : m_chart->houseCusps) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", cusp.houseNumber);
        ImGui::TableSetColumnIndex(1);
        drawLongitudeValue(cusp.longitudeDegrees, accentCol, textCol);
      }
      ImGui::EndTable();
    }
  }
  popInfoTableStyles();
  endSectionHeader();

  drawSectionHeader("Interpretation", accentCol, textCol);
  if (interpretationText_.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(mutedCol));
    ImGui::TextWrapped("Compute the chart to see interpretation notes here.");
    ImGui::PopStyleColor();
  } else {
    markdown_render::renderMarkdown(interpretationText_);
  }
  endSectionHeader();

  ImGui::EndChild();
}

void ChartWorkspacePanel::draw() {
  if (!open) return;
  ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
  if (m_focusOnNextDraw) {
    ImGui::SetNextWindowFocus();
  }
  if (!ImGui::Begin("Chart Workspace", &open)) { ImGui::End(); return; }
  m_focusOnNextDraw = false;

  exportPngProfile_ = parseSettingIndex(m_ctx, "default_export_png_profile", 0, 3);
  exportLayoutTemplate_ = parseSettingIndex(m_ctx, "default_export_layout", 0, 2);

  // Toolbar
  const char* chartTypes[] = {"Natal", "Transit-to-Natal"};
  leftLabel("Chart Type", 160.0f);
  if (ImGui::Combo("##ChartType", &currentChartType_, chartTypes, IM_ARRAYSIZE(chartTypes))) {
    if (m_personId > 0) {
      computeChart();
    }
  }
  ImGui::SameLine();

  if (ImGui::Button("Compute")) {
    computeChart();
  }
  ImGui::SameLine();

  const bool canCopy = m_chart.has_value();
  if (!canCopy) ImGui::BeginDisabled();
  if (ImGui::Button("Copy Chart Text")) {
    const std::string clipboardText = buildClipboardText();
    if (!clipboardText.empty()) {
      ImGui::SetClipboardText(clipboardText.c_str());
      statusMessage_ = "Chart text copied to clipboard.";
    }
  }
  if (!canCopy) ImGui::EndDisabled();
  ImGui::SameLine();

  const bool canAskAi = m_chart.has_value() && m_aiPanel != nullptr;
  if (!canAskAi) ImGui::BeginDisabled();
  if (ImGui::Button("AI")) {
    m_aiPanel->requestInterpretation(*m_chart, m_lastComputedAiChartType_, m_lastComputedAiSourceLabel_);
  }
  if (!canAskAi) ImGui::EndDisabled();
  ImGui::SameLine();

  bool canExport = m_hasScene;
  if (!canExport) ImGui::BeginDisabled();

  const int preferredExportFormat = parseSettingIndex(m_ctx, "default_export_format", 0, 2);

  auto buildSceneForExport = [&]() {
    const auto theme = currentThemePreset();
    render::ChartScene scene = (m_chart && m_hasScene)
        ? render::buildNatalChartScene(*m_chart, theme)
        : m_scene;
    if (exportLayoutTemplate_ == static_cast<int>(ExportLayoutTemplate::ReferenceSheet)) {
      std::vector<render::ReferenceSheetSection> sections;
      if (m_person) {
        sections.push_back({"Subject", {
            {"Name", m_person->fullName},
            {"Display", m_person->displayName.empty() ? m_person->fullName : m_person->displayName},
            {"Chart", currentChartType_ == 0 ? "Natal" : "Transit to Natal"},
        }});
      }
      if (m_birthEvent) {
        sections.push_back({"Birth", {
            {"Date", m_birthEvent->birthDate.empty() ? currentDateTag() : m_birthEvent->birthDate},
            {"Time", birthTimeLabel(*m_birthEvent)},
            {"Accuracy", timeAccuracyLabel(m_birthEvent->timeAccuracy)},
        }});
      }
      if (m_location) {
        sections.push_back({"Location", {
            {"Place", m_location->displayName},
            {"Latitude", formatCoord(m_location->latitude, true)},
            {"Longitude", formatCoord(m_location->longitude, false)},
            {"Timezone", m_location->timezoneName},
        }});
      }
      if (m_chart) {
        render::ReferenceSheetSection chartSection{"Chart Facts", {
            {"House System", m_chart->houseSystem},
            {"Zodiac", m_chart->zodiacMode},
            {"Planets", std::to_string(m_chart->planets.size())},
            {"Aspects", std::to_string(m_chart->aspects.size())},
        }};
        if (auto asc = findHouseLongitude(*m_chart, 1)) chartSection.facts.push_back({"Asc", formatLongitudeWithSign(*asc)});
        if (auto mc = findHouseLongitude(*m_chart, 10)) chartSection.facts.push_back({"MC", formatLongitudeWithSign(*mc)});
        sections.push_back(chartSection);

        render::ReferenceSheetSection planetSection{"Objects", {}};
        for (const auto& planet : m_chart->planets) {
          planetSection.facts.push_back({planet.objectId, formatLongitudeWithSign(planet.longitudeDegrees)});
        }
        sections.push_back(std::move(planetSection));
      }

      const std::string title = m_person
          ? (m_person->displayName.empty() ? m_person->fullName : m_person->displayName)
          : std::string("Asteria Chart");
      const std::string subtitle = m_chart
          ? (m_chart->houseSystem + " / " + m_chart->zodiacMode)
          : std::string();
      const std::string footer = m_birthEvent && !m_birthEvent->birthDate.empty()
          ? ("Birth date: " + m_birthEvent->birthDate)
          : std::string("Asteria export");
      scene = render::buildReferenceSheetScene(scene, theme, title, subtitle, footer, sections);
    }
    return scene;
  };

  auto buildExportMetadata = [&](const std::string& profileName) {
    core::ExportMetadata metadata;
    metadata.chartType = currentChartType_ == 0 ? "natal" : "transit_to_natal";
    metadata.exportProfile = profileName;
    metadata.layoutTemplate = exportLayoutTemplate_ == 0 ? "chart_only" : "reference_sheet";
    metadata.dateTag = m_birthEvent && !m_birthEvent->birthDate.empty() ? m_birthEvent->birthDate : currentDateTag();
    metadata.hasWarnings = m_chart && !m_chart->uncertaintyFlags.empty();
    return metadata;
  };

  auto buildDefaultName = [&](const std::string& extension) {
    const std::string subject = m_person
        ? (m_person->displayName.empty() ? m_person->fullName : m_person->displayName)
        : std::string("chart");
    const std::string chartType = currentChartType_ == 0 ? "natal" : "transit_to_natal";
    const std::string dateTag = m_birthEvent && !m_birthEvent->birthDate.empty() ? m_birthEvent->birthDate : currentDateTag();
    return buildRecommendedExportFileName(subject, chartType, dateTag, currentThemePreset().name, extension);
  };

  auto exportSvgAction = [&]() {
    auto path = showSaveFileDialog(
        L"Export Natal Chart",
        buildDefaultName("svg"),
        L"SVG Files (*.svg)\0*.svg\0All Files (*.*)\0*.*\0\0",
        L"svg");
    if (!path) return;

    auto theme = currentThemePreset();
    auto scene = buildSceneForExport();
    auto metadata = buildExportMetadata("vector");
    auto result = m_ctx.exportService.exportSvg(
        scene, m_chart ? m_chart->computedChartId : 0,
        *path, theme, metadata);
    statusMessage_ = result.ok() ? "Exported: " + result.value().filePath
                                 : "Export failed: " + result.error().message;
  };

  auto exportPngAction = [&]() {
    auto path = showSaveFileDialog(
        L"Export Natal Chart PNG",
        buildDefaultName("png"),
        L"PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0\0",
        L"png");
    if (!path) return;

    auto theme = currentThemePreset();
    auto scene = buildSceneForExport();
    const auto spec = exportPngProfileSpec(static_cast<ExportPngProfile>(exportPngProfile_));
    auto metadata = buildExportMetadata(exportPngProfileLabel(static_cast<ExportPngProfile>(exportPngProfile_)));
    auto result = m_ctx.exportService.exportPng(
        scene, m_chart ? m_chart->computedChartId : 0,
        *path, spec.widthPx, spec.heightPx, spec.dpi, theme, metadata);
    statusMessage_ = result.ok() ? "Exported: " + result.value().filePath
                                 : "Export failed: " + result.error().message;
  };

  if (preferredExportFormat == 1) {
    if (ImGui::Button("Export PNG")) exportPngAction();
    ImGui::SameLine();
    if (ImGui::Button("Export SVG")) exportSvgAction();
  } else {
    if (ImGui::Button("Export SVG")) exportSvgAction();
    ImGui::SameLine();
    if (ImGui::Button("Export PNG")) exportPngAction();
  }
  if (!canExport) ImGui::EndDisabled();

  // Status line
  if (!statusMessage_.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "%s", statusMessage_.c_str());
  }

  ImGui::Separator();

  const float infoWidth = 380.0f;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float contentHeight = ImGui::GetContentRegionAvail().y;
  const float chartWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x - infoWidth - spacing);

  ImGui::BeginChild("NatalChartCanvas", ImVec2(chartWidth, contentHeight), false);
  drawChartCanvas();
  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginChild("NatalInfoColumn", ImVec2(infoWidth, contentHeight), false);
  drawInfoSidePanel();
  ImGui::EndChild();

  ImGui::End();
}

std::string ChartWorkspacePanel::buildClipboardText() const {
  if (!m_chart) return {};

  std::ostringstream out;
  out << "Asteria Chart Information\n\n";
  out << "Chart Type: " << chartTypeLabel(currentChartType_) << '\n';

  if (m_person) {
    out << "Name: " << (m_person->displayName.empty() ? m_person->fullName : m_person->displayName) << '\n';
  }

  if (m_birthEvent) {
    out << "Birth Date: " << (m_birthEvent->birthDate.empty() ? "Unknown" : m_birthEvent->birthDate) << '\n';
    out << "Birth Time: " << birthTimeLabel(*m_birthEvent) << '\n';
    out << "Time Accuracy: " << timeAccuracyLabel(m_birthEvent->timeAccuracy) << '\n';
  }

  if (m_birthEvent || m_location) {
    const std::string locationName = m_location ? m_location->displayName :
        (m_birthEvent && !m_birthEvent->cityInput.empty() ? m_birthEvent->cityInput : std::string("Unknown"));
    const double latitude = m_location ? m_location->latitude :
        (m_birthEvent && m_birthEvent->latitudeDeg ? *m_birthEvent->latitudeDeg : 0.0);
    const double longitude = m_location ? m_location->longitude :
        (m_birthEvent && m_birthEvent->longitudeDeg ? *m_birthEvent->longitudeDeg : 0.0);
    std::string zoneName = m_location ? m_location->timezoneName :
        (m_birthEvent && m_birthEvent->timezoneName ? *m_birthEvent->timezoneName : std::string());
    if (zoneName.empty()) zoneName = "Manual / unresolved";

    out << "Place: " << locationName << '\n';
    out << "Latitude: " << formatCoord(latitude, true) << '\n';
    out << "Longitude: " << formatCoord(longitude, false) << '\n';
    out << "Time Zone: " << zoneName << '\n';
    if (m_birthEvent && m_birthEvent->timezoneOffsetHours) {
      out << "UTC Offset: " << formatUtcOffset(*m_birthEvent->timezoneOffsetHours) << '\n';
    }
    if (m_birthEvent && m_birthEvent->dstOffsetHours) {
      out << "DST Offset: " << *m_birthEvent->dstOffsetHours << "h\n";
    }
  }

  out << "\nChart Settings:\n";
  out << "- House System: " << m_chart->houseSystem << '\n';
  out << "- Zodiac: " << m_chart->zodiacMode << '\n';
  out << "- Engine: " << (m_chart->engineMethod.empty() ? "Astrolog" : m_chart->engineMethod) << '\n';

  if (!m_chart->uncertaintyFlags.empty()) {
    out << "\nWarnings:\n";
    for (const auto& warning : m_chart->uncertaintyFlags) {
      out << "- " << warning << '\n';
    }
  }

  out << "\nAngles:\n";
  for (const auto houseNumber : {1, 4, 7, 10}) {
    if (auto longitude = findHouseLongitude(*m_chart, houseNumber)) {
      const char* label = houseNumber == 1 ? "Ascendant" :
                          houseNumber == 4 ? "IC" :
                          houseNumber == 7 ? "Descendant" : "Midheaven";
      out << "- " << label << ": " << formatLongitudePlain(*longitude) << '\n';
    }
  }

  out << "\nPlanetary Positions:\n";
  for (const auto& planet : m_chart->planets) {
    out << "- " << planet.objectId << " in " << planet.sign;
    if (planet.house) out << " (House " << *planet.house << ")";
    out << " at " << render::glyphs::formatDegMin(planet.longitudeDegrees);
    if (planet.retrograde) out << " (Rx)";
    out << '\n';
  }

  out << "\nMajor Aspects:\n";
  for (const auto& asp : m_chart->aspects) {
    out << "- " << asp.objectA << ' ' << asp.aspectType << ' ' << asp.objectB;
    out << " (orb: " << std::fixed << std::setprecision(2) << asp.orbDegrees;
    if (asp.applyingOrSeparating) out << ", " << *asp.applyingOrSeparating;
    out << ")\n";
  }

  return out.str();
}

}  // namespace asteria::ui
