#include "compare_workspace_panel.h"
#include "ai_interpretation_panel.h"
#include "app_context.h"
#include "../render/astro_glyphs.h"
#include "core/birth_event_resolver.h"
#include "imgui.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace asteria::ui {

CompareWorkspacePanel::CompareWorkspacePanel(AppContext& ctx) : m_ctx(ctx) {
  refreshPeople();
}

void CompareWorkspacePanel::refreshPeople() {
  m_people = m_ctx.personRepo.findAll();
  m_nameLabels.clear();
  for (const auto& p : m_people) {
    m_nameLabels.push_back(p.fullName);
  }
}

void CompareWorkspacePanel::computeComparison() {
  if (m_people.empty() || personA_ >= static_cast<int>(m_people.size())
      || personB_ >= static_cast<int>(m_people.size())) {
    statusMessage_ = "Select two different people.";
    return;
  }
  if (personA_ == personB_) {
    statusMessage_ = "Person A and B must be different.";
    return;
  }

  auto eventsA = m_ctx.birthEventRepo.findByPersonId(m_people[personA_].personId);
  auto eventsB = m_ctx.birthEventRepo.findByPersonId(m_people[personB_].personId);
  if (eventsA.empty() || eventsB.empty()) {
    statusMessage_ = "Both people need birth data.";
    return;
  }

  const auto& beA = eventsA.front();
  const auto& beB = eventsB.front();
  std::optional<domain::LocationResolution> locA, locB;
  if (beA.locationId) locA = m_ctx.locationRepo.findById(*beA.locationId);
  if (beB.locationId) locB = m_ctx.locationRepo.findById(*beB.locationId);

  // Build a single ChartRequest carrying both resolved inputs
  domain::ChartRequest req;
  req.primaryBirthEventId = beA.birthEventId;
  req.secondaryBirthEventId = beB.birthEventId;
  req.defaultHouseSystem = "Placidus";
  req.zodiacMode = "Tropical";
  req.primaryInput   = core::resolveBirthInput(beA, locA);
  req.secondaryInput = core::resolveBirthInput(beB, locB);

  // Apply unknown-time policy on the primary
  int policyIdx = 0;
  try { policyIdx = std::stoi(m_ctx.getSetting("unknown_time_policy", "0")); }
  catch (...) { policyIdx = 0; }
  std::string policy = (policyIdx == 1) ? "noon_default_with_warning"
                                        : "unknown_time_no_houses";
  core::applyUnknownTimePolicy(req, beA, policy);

  if (compareMode_ == 0) {
    // Synastry — engine returns combined chart with secondaryChart + interAspects
    req.chartType = domain::ChartType::Synastry;
    auto res = m_ctx.comparisonService.computeSynastry(req);
    if (!res.ok()) {
      statusMessage_ = "Synastry failed: " + res.error().message;
      m_hasResult = false;
      return;
    }
    m_chartA = res.value();
    if (m_chartA->secondaryChart) {
      m_chartB = *m_chartA->secondaryChart;
    } else {
      m_chartB.reset();
    }
    m_compChart.reset();
    m_hasResult = true;
    statusMessage_ = "Synastry: " + m_people[personA_].fullName + " / " +
                     m_people[personB_].fullName + " (" +
                     std::to_string(m_chartA->interAspects.size()) +
                     " inter-aspects)";
    if (m_aiPanel) m_aiPanel->setChart(*m_chartA, domain::ChartType::Synastry);
  } else {
    // Composite — engine returns midpoint chart
    req.chartType = domain::ChartType::Composite;
    auto res = m_ctx.comparisonService.computeComposite(req);
    if (!res.ok()) {
      statusMessage_ = "Composite failed: " + res.error().message;
      m_hasResult = false;
      return;
    }
    m_compChart = res.value();
    m_chartA.reset();
    m_chartB.reset();
    m_hasResult = true;
    statusMessage_ = "Composite: " + m_people[personA_].fullName + " / " +
                     m_people[personB_].fullName;
    if (m_aiPanel) m_aiPanel->setChart(*m_compChart, domain::ChartType::Composite);
  }
}

void CompareWorkspacePanel::drawChartCanvas() {
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  if (canvasSize.x < 100) canvasSize.x = 100;
  if (canvasSize.y < 100) canvasSize.y = 100;

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(canvasPos,
      ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
      IM_COL32(20, 20, 24, 255));

  float cx = canvasPos.x + canvasSize.x * 0.5f;
  float cy = canvasPos.y + canvasSize.y * 0.5f;
  float maxR = (canvasSize.x < canvasSize.y ? canvasSize.x : canvasSize.y) * 0.45f;

  // Try to use the larger glyph font (index 1)
  ImFont* glyphFont = (ImGui::GetIO().Fonts->Fonts.Size > 1)
                      ? ImGui::GetIO().Fonts->Fonts[1] : nullptr;

  // Sign glyph labels around the outer ring
  for (int i = 0; i < 12; ++i) {
    float angle = static_cast<float>(i) * 30.0f * static_cast<float>(M_PI) / 180.0f
                  - static_cast<float>(M_PI) / 2.0f;
    float cosA = cosf(angle), sinA = sinf(angle);
    drawList->AddLine(
        ImVec2(cx + maxR * 0.50f * cosA, cy + maxR * 0.50f * sinA),
        ImVec2(cx + maxR * cosA,         cy + maxR * sinA),
        IM_COL32(80, 110, 160, 180), 1.0f);

    float midAngle = angle + 15.0f * static_cast<float>(M_PI) / 180.0f;
    float labelR = maxR * 0.93f;
    const char* signGlyph = render::glyphs::sign(i);
    if (glyphFont) {
      drawList->AddText(glyphFont, 18.0f,
          ImVec2(cx + labelR * cosf(midAngle) - 7,
                 cy + labelR * sinf(midAngle) - 9),
          IM_COL32(180, 200, 230, 255), signGlyph);
    } else {
      drawList->AddText(
          ImVec2(cx + labelR * cosf(midAngle) - 7,
                 cy + labelR * sinf(midAngle) - 6),
          IM_COL32(180, 200, 230, 255), signGlyph);
    }
  }

  auto drawPlanetGlyph = [&](const domain::PlanetPosition& planet,
                              float px, float py, ImU32 glyphCol, ImU32 degCol) {
    const char* glyph = render::glyphs::planet(planet.objectId);
    if (glyph && glyphFont) {
      drawList->AddText(glyphFont, 18.0f, ImVec2(px - 7, py - 9), glyphCol, glyph);
    } else if (glyph) {
      drawList->AddText(ImVec2(px - 5, py - 5), glyphCol, glyph);
    } else {
      std::string label = planet.objectId.substr(0, 2);
      drawList->AddText(ImVec2(px - 5, py - 5), glyphCol, label.c_str());
    }
    // Degree label
    std::string deg = render::glyphs::formatDegMin(planet.longitudeDegrees);
    drawList->AddText(ImVec2(px - 10, py + 7), degCol, deg.c_str());
    // Retrograde
    if (planet.retrograde) {
      drawList->AddText(ImVec2(px + 9, py - 9), IM_COL32(200, 60, 60, 200), "R");
    }
  };

  if (compareMode_ == 0 && m_hasResult && m_chartA && m_chartB) {
    // Synastry bi-wheel
    drawList->AddCircle(ImVec2(cx, cy), maxR, IM_COL32(100, 140, 200, 255), 72, 2.0f);
    drawList->AddCircle(ImVec2(cx, cy), maxR * 0.70f, IM_COL32(200, 140, 100, 255), 72, 2.0f);
    drawList->AddCircle(ImVec2(cx, cy), maxR * 0.50f, IM_COL32(80, 110, 160, 180), 72, 1.0f);

    // Person A planets (outer ring, blue)
    for (const auto& planet : m_chartA->planets) {
      float angle = static_cast<float>(planet.longitudeDegrees * M_PI / 180.0 - M_PI / 2.0);
      float px = cx + maxR * 0.82f * cosf(angle);
      float py = cy + maxR * 0.82f * sinf(angle);
      drawPlanetGlyph(planet, px, py,
          IM_COL32(100, 180, 255, 255), IM_COL32(100, 160, 240, 160));
    }
    // Person B planets (inner ring, orange)
    for (const auto& planet : m_chartB->planets) {
      float angle = static_cast<float>(planet.longitudeDegrees * M_PI / 180.0 - M_PI / 2.0);
      float px = cx + maxR * 0.60f * cosf(angle);
      float py = cy + maxR * 0.60f * sinf(angle);
      drawPlanetGlyph(planet, px, py,
          IM_COL32(255, 180, 80, 255), IM_COL32(240, 160, 80, 160));
    }

    drawList->AddText(ImVec2(canvasPos.x + 10, canvasPos.y + 10),
        IM_COL32(180, 200, 230, 255), "Synastry Bi-Wheel");
  } else if (compareMode_ == 1 && m_hasResult && m_compChart) {
    // Composite wheel
    drawList->AddCircle(ImVec2(cx, cy), maxR, IM_COL32(160, 100, 200, 255), 72, 2.0f);
    drawList->AddCircle(ImVec2(cx, cy), maxR * 0.70f, IM_COL32(130, 90, 170, 180), 72, 1.5f);
    drawList->AddCircle(ImVec2(cx, cy), maxR * 0.35f, IM_COL32(110, 80, 150, 160), 72, 1.0f);

    for (const auto& planet : m_compChart->planets) {
      float angle = static_cast<float>(planet.longitudeDegrees * M_PI / 180.0 - M_PI / 2.0);
      float px = cx + maxR * 0.52f * cosf(angle);
      float py = cy + maxR * 0.52f * sinf(angle);
      drawPlanetGlyph(planet, px, py,
          IM_COL32(200, 160, 255, 255), IM_COL32(180, 140, 220, 160));
    }

    drawList->AddText(ImVec2(canvasPos.x + 10, canvasPos.y + 10),
        IM_COL32(200, 180, 240, 255), "Composite Chart");
  } else {
    // No result yet
    drawList->AddCircle(ImVec2(cx, cy), maxR, IM_COL32(80, 80, 100, 200), 72, 1.5f);
    drawList->AddText(ImVec2(cx - 130, cy - 8), IM_COL32(150, 150, 170, 255),
        "Select two people and click Compare");
  }

  ImGui::Dummy(canvasSize);
}

void CompareWorkspacePanel::draw() {
  if (!open) return;
  ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Compare Workspace", &open)) { ImGui::End(); return; }

  // Refresh people list if needed
  if (m_people.empty()) refreshPeople();

  // Person selectors
  if (!m_nameLabels.empty()) {
    auto getter = [](void* data, int idx) -> const char* {
      auto* labels = static_cast<std::vector<std::string>*>(data);
      return (*labels)[idx].c_str();
    };
    ImGui::SetNextItemWidth(200);
    ImGui::Combo("Person A", &personA_, getter, &m_nameLabels,
                 static_cast<int>(m_nameLabels.size()));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::Combo("Person B", &personB_, getter, &m_nameLabels,
                 static_cast<int>(m_nameLabels.size()));
  } else {
    ImGui::TextDisabled("No people in database. Add people in the Library panel.");
  }
  ImGui::SameLine();

  // Mode switch
  const char* modes[] = {"Synastry", "Composite"};
  ImGui::SetNextItemWidth(130);
  ImGui::Combo("Mode", &compareMode_, modes, IM_ARRAYSIZE(modes));
  ImGui::SameLine();

  if (ImGui::Button("Compare")) {
    refreshPeople();
    computeComparison();
  }
  ImGui::SameLine();

  bool canExport = m_hasResult;
  if (!canExport) ImGui::BeginDisabled();
  if (ImGui::Button("Export SVG")) {
    // Build scene from the comparison and export
    statusMessage_ = "Export not yet implemented for comparison charts.";
  }
  if (!canExport) ImGui::EndDisabled();

  if (!statusMessage_.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "%s", statusMessage_.c_str());
  }

  ImGui::Separator();

  drawChartCanvas();

  ImGui::End();
}

}  // namespace asteria::ui
