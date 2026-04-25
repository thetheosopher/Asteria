#include "chart_workspace_panel.h"
#include "ai_interpretation_panel.h"
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
#include <iomanip>

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

}  // anonymous namespace

ChartWorkspacePanel::ChartWorkspacePanel(AppContext& ctx) : m_ctx(ctx) {}

void ChartWorkspacePanel::setSelectedPerson(std::int64_t personId) {
  if (m_personId == personId) return;
  m_personId = personId;
  m_person.reset();
  m_birthEvent.reset();
  m_location.reset();
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

  // Push chart to AI panel if connected.
  if (m_aiPanel) {
    m_aiPanel->setChart(*m_chart, req.chartType);
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
  ImGui::TextUnformatted("Natal Chart Notes");
  ImGui::PopStyleColor();
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(mutedCol));
  ImGui::TextWrapped("Person, birthplace, chart settings, and object positions in a compact reference panel.");
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
    drawInfoRow("House System", m_chart->houseSystem, mutedCol, textCol);
    drawInfoRow("Zodiac", m_chart->zodiacMode, mutedCol, textCol);
    drawInfoRow("Engine", m_chart->engineMethod.empty() ? std::string("Astrolog") : m_chart->engineMethod, mutedCol, textCol);
    drawInfoRow("Planets", std::to_string(m_chart->planets.size()), mutedCol, textCol);
    drawInfoRow("Aspects", std::to_string(m_chart->aspects.size()), mutedCol, textCol);
    if (!m_chart->uncertaintyFlags.empty()) {
      drawInfoRow("Warnings", m_chart->uncertaintyFlags.front(), mutedCol, textCol);
    }
  }
  endSectionHeader();

  drawSectionHeader("Object Positions", accentCol, textCol);
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
      std::string body = std::string(render::glyphs::planet(planet.objectId) ? render::glyphs::planet(planet.objectId) : "") + " " + planet.objectId;
      ImGui::TextUnformatted(body.c_str());

      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(render::glyphs::formatPosition(planet.longitudeDegrees, planet.sign).c_str());

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
  endSectionHeader();

  if (m_chart && ImGui::CollapsingHeader("House Cusps", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        ImGui::TextUnformatted(render::glyphs::formatDegMin(cusp.longitudeDegrees).c_str());
      }
      ImGui::EndTable();
    }
  }

  if (showInterpretation_) {
    drawSectionHeader("Interpretation", accentCol, textCol);
    if (interpretationText_.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(mutedCol));
      ImGui::TextWrapped("Compute the chart to see interpretation notes here.");
      ImGui::PopStyleColor();
    } else {
      ImGui::TextWrapped("%s", interpretationText_.c_str());
    }
    endSectionHeader();
  }

  ImGui::EndChild();
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

}  // namespace asteria::ui
