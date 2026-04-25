#include "compare_workspace_panel.h"
#include "ai_interpretation_panel.h"
#include "app_context.h"
#include "astrology_font.h"
#include "export_options.h"
#include "file_dialog.h"
#include "../render/astro_glyphs.h"
#include "render/comparison_chart_layout.h"
#include "render/export_layout_templates.h"
#include "render/theme_presets.h"
#include "core/birth_event_resolver.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace asteria::ui {

namespace {

int parseSettingIndex(const AppContext& ctx, const char* key, int fallback, int maxExclusive) {
  int value = fallback;
  try { value = std::stoi(ctx.getSetting(key, std::to_string(fallback))); }
  catch (...) { value = fallback; }
  if (value < 0 || value >= maxExclusive) return fallback;
  return value;
}

ImU32 colorU32(const render::Color& color, int alpha = 255) {
  return IM_COL32(color.r, color.g, color.b, alpha);
}

render::Color mixColor(const render::Color& a, const render::Color& b, float weightToB) {
  const auto mixChannel = [&](int ca, int cb) {
    return static_cast<int>(ca + (cb - ca) * weightToB);
  };
  return {mixChannel(a.r, b.r), mixChannel(a.g, b.g), mixChannel(a.b, b.b)};
}

render::ThemePreset currentThemePreset(const AppContext& ctx) {
  return render::themePresetByIndex(parseSettingIndex(ctx, "default_theme", 0, 4));
}

}  // namespace

CompareWorkspacePanel::CompareWorkspacePanel(AppContext& ctx) : m_ctx(ctx) {
  try {
    exportPngProfile_ = std::stoi(m_ctx.getSetting("default_export_png_profile", "0"));
  } catch (...) {
    exportPngProfile_ = 0;
  }
  if (exportPngProfile_ < 0 || exportPngProfile_ > 2) exportPngProfile_ = 0;

  try {
    exportLayoutTemplate_ = std::stoi(m_ctx.getSetting("default_export_layout", "0"));
  } catch (...) {
    exportLayoutTemplate_ = 0;
  }
  if (exportLayoutTemplate_ < 0 || exportLayoutTemplate_ > 1) exportLayoutTemplate_ = 0;

  refreshPeople();
}

void CompareWorkspacePanel::refreshPeople() {
  const std::int64_t previousPersonA =
      (personA_ >= 0 && personA_ < static_cast<int>(m_people.size()))
          ? m_people[personA_].personId
          : 0;
  const std::int64_t previousPersonB =
      (personB_ >= 0 && personB_ < static_cast<int>(m_people.size()))
          ? m_people[personB_].personId
          : 0;

  m_people = m_ctx.personRepo.findAll();
  m_nameLabels.clear();
  personA_ = 0;
  personB_ = m_people.size() > 1 ? 1 : 0;

  for (int i = 0; i < static_cast<int>(m_people.size()); ++i) {
    const auto& p = m_people[i];
    m_nameLabels.push_back(p.fullName);
    if (p.personId == previousPersonA) personA_ = i;
    if (p.personId == previousPersonB) personB_ = i;
  }

  if (!m_people.empty()) {
    if (personA_ >= static_cast<int>(m_people.size())) personA_ = 0;
    if (personB_ >= static_cast<int>(m_people.size())) personB_ = m_people.size() > 1 ? 1 : 0;
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

  const auto theme = currentThemePreset(m_ctx);
  const render::Color personAColor = mixColor(theme.accent, theme.primary, 0.22f);
  const render::Color personBColor = mixColor(theme.secondary, theme.primary, 0.35f);
  const render::Color compositeColor = mixColor(theme.accent, theme.secondary, 0.45f);

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(canvasPos,
      ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
      colorU32(theme.background));

  float cx = canvasPos.x + canvasSize.x * 0.5f;
  float cy = canvasPos.y + canvasSize.y * 0.5f;
  float maxR = (canvasSize.x < canvasSize.y ? canvasSize.x : canvasSize.y) * 0.45f;

  ImFont* glyphFont = astrology_font::wheelGlyphFont();

  // Sign glyph labels around the outer ring
  for (int i = 0; i < 12; ++i) {
    float angle = static_cast<float>(i) * 30.0f * static_cast<float>(M_PI) / 180.0f
                  - static_cast<float>(M_PI) / 2.0f;
    float cosA = cosf(angle), sinA = sinf(angle);
    drawList->AddLine(
        ImVec2(cx + maxR * 0.50f * cosA, cy + maxR * 0.50f * sinA),
        ImVec2(cx + maxR * cosA,         cy + maxR * sinA),
      colorU32(theme.secondary, 180), 1.0f);

    float midAngle = angle + 15.0f * static_cast<float>(M_PI) / 180.0f;
    float labelR = maxR * 0.93f;
    const char* signGlyph = astrology_font::wheelSignGlyph(i);
    if (glyphFont) {
      drawList->AddText(glyphFont, 18.0f,
          ImVec2(cx + labelR * cosf(midAngle) - 7,
                 cy + labelR * sinf(midAngle) - 9),
           colorU32(theme.text), signGlyph);
    } else {
      drawList->AddText(
          ImVec2(cx + labelR * cosf(midAngle) - 7,
                 cy + labelR * sinf(midAngle) - 6),
           colorU32(theme.text), signGlyph);
    }
  }

  auto drawPlanetGlyph = [&](const domain::PlanetPosition& planet,
                              float px, float py, ImU32 glyphCol, ImU32 degCol) {
    const char* glyph = astrology_font::wheelPlanetGlyph(planet.objectId);
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
      const char* retrogradeGlyph = astrology_font::wheelRetrogradeGlyph();
      if (astrology_font::usingLegacyWheelFont() && glyphFont) {
        drawList->AddText(glyphFont, 14.0f, ImVec2(px + 7, py - 11), IM_COL32(200, 60, 60, 200), retrogradeGlyph);
      } else {
        drawList->AddText(ImVec2(px + 9, py - 9), IM_COL32(200, 60, 60, 200), retrogradeGlyph);
      }
    }
  };

  if (compareMode_ == 0 && m_hasResult && m_chartA && m_chartB) {
    // Synastry bi-wheel
    drawList->AddCircle(ImVec2(cx, cy), maxR, colorU32(personAColor), 72, 2.0f);
    drawList->AddCircle(ImVec2(cx, cy), maxR * 0.70f, colorU32(personBColor), 72, 2.0f);
    drawList->AddCircle(ImVec2(cx, cy), maxR * 0.50f, colorU32(theme.secondary, 180), 72, 1.0f);

    // Person A planets (outer ring, blue)
    for (const auto& planet : m_chartA->planets) {
      float angle = static_cast<float>(planet.longitudeDegrees * M_PI / 180.0 - M_PI / 2.0);
      float px = cx + maxR * 0.82f * cosf(angle);
      float py = cy + maxR * 0.82f * sinf(angle);
      drawPlanetGlyph(planet, px, py,
          colorU32(personAColor), colorU32(personAColor, 160));
    }
    // Person B planets (inner ring, orange)
    for (const auto& planet : m_chartB->planets) {
      float angle = static_cast<float>(planet.longitudeDegrees * M_PI / 180.0 - M_PI / 2.0);
      float px = cx + maxR * 0.60f * cosf(angle);
      float py = cy + maxR * 0.60f * sinf(angle);
      drawPlanetGlyph(planet, px, py,
          colorU32(personBColor), colorU32(personBColor, 160));
    }

    drawList->AddText(ImVec2(canvasPos.x + 10, canvasPos.y + 10),
        colorU32(theme.text), "Synastry Bi-Wheel");
  } else if (compareMode_ == 1 && m_hasResult && m_compChart) {
    // Composite wheel
    drawList->AddCircle(ImVec2(cx, cy), maxR, colorU32(compositeColor), 72, 2.0f);
    drawList->AddCircle(ImVec2(cx, cy), maxR * 0.70f, colorU32(theme.secondary, 180), 72, 1.5f);
    drawList->AddCircle(ImVec2(cx, cy), maxR * 0.35f, colorU32(theme.secondary, 160), 72, 1.0f);

    for (const auto& planet : m_compChart->planets) {
      float angle = static_cast<float>(planet.longitudeDegrees * M_PI / 180.0 - M_PI / 2.0);
      float px = cx + maxR * 0.52f * cosf(angle);
      float py = cy + maxR * 0.52f * sinf(angle);
      drawPlanetGlyph(planet, px, py,
          colorU32(compositeColor), colorU32(compositeColor, 160));
    }

    drawList->AddText(ImVec2(canvasPos.x + 10, canvasPos.y + 10),
        colorU32(theme.text), "Composite Chart");
  } else {
    // No result yet
    drawList->AddCircle(ImVec2(cx, cy), maxR, colorU32(theme.secondary, 200), 72, 1.5f);
    drawList->AddText(ImVec2(cx - 130, cy - 8), colorU32(theme.text, 210),
        "Select two people and click Compare");
  }

  ImGui::Dummy(canvasSize);
}

void CompareWorkspacePanel::draw() {
  if (!open) return;
  ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Compare Workspace", &open)) { ImGui::End(); return; }

  exportPngProfile_ = parseSettingIndex(m_ctx, "default_export_png_profile", 0, 3);
  exportLayoutTemplate_ = parseSettingIndex(m_ctx, "default_export_layout", 0, 2);

  // Keep selectors in sync with Library changes while preserving current choices.
  refreshPeople();

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
  int preferredExportFormat = 0;
  try {
    preferredExportFormat = std::stoi(m_ctx.getSetting("default_export_format", "0"));
  } catch (...) {
    preferredExportFormat = 0;
  }
  if (preferredExportFormat < 0 || preferredExportFormat > 1) preferredExportFormat = 0;

  const auto exportTheme = currentThemePreset(m_ctx);
  auto buildComparisonScene = [&]() {
    render::ChartScene scene;
    if (compareMode_ == 0 && m_chartA && m_chartB) {
      scene = render::buildSynastryChartScene(*m_chartA, *m_chartB, exportTheme);
    } else if (m_compChart) {
      scene = render::buildCompositeChartScene(*m_compChart, exportTheme);
    }

    if (exportLayoutTemplate_ == static_cast<int>(ExportLayoutTemplate::ReferenceSheet)) {
      const std::string nameA = (personA_ >= 0 && personA_ < static_cast<int>(m_people.size()))
          ? m_people[personA_].fullName
          : std::string("Person A");
      const std::string nameB = (personB_ >= 0 && personB_ < static_cast<int>(m_people.size()))
          ? m_people[personB_].fullName
          : std::string("Person B");
      std::vector<render::ReferenceSheetSection> sections = {
          {"People", {{"Person A", nameA}, {"Person B", nameB}}},
          {"Chart", {{"Mode", compareMode_ == 0 ? "Synastry" : "Composite"},
                     {"Date", currentDateTag()},
                     {"Warnings", ((m_chartA && !m_chartA->uncertaintyFlags.empty()) ||
                                     (m_chartB && !m_chartB->uncertaintyFlags.empty()) ||
                                     (m_compChart && !m_compChart->uncertaintyFlags.empty())) ? "Present" : "None"}}}
      };
      if (compareMode_ == 0 && m_chartA && m_chartB) {
        sections.push_back({"Synastry", {{"Inter-aspects", std::to_string(m_chartA->interAspects.size())},
                                           {"Outer ring", nameA},
                                           {"Inner ring", nameB}}});
      } else if (m_compChart) {
        sections.push_back({"Composite", {{"Planets", std::to_string(m_compChart->planets.size())},
                                            {"Aspects", std::to_string(m_compChart->aspects.size())}}});
      }
      scene = render::buildReferenceSheetScene(
          scene,
          exportTheme,
          nameA + " + " + nameB,
          compareMode_ == 0 ? "Synastry" : "Composite",
          "Asteria comparison export",
          sections);
    }
    return scene;
  };

  auto buildComparisonMetadata = [&](const std::string& profileName) {
    core::ExportMetadata metadata;
    metadata.chartType = compareMode_ == 0 ? "synastry" : "composite";
    metadata.exportProfile = profileName;
    metadata.layoutTemplate = exportLayoutTemplate_ == 0 ? "chart_only" : "reference_sheet";
    metadata.dateTag = currentDateTag();
    metadata.hasWarnings = (m_chartA && !m_chartA->uncertaintyFlags.empty())
                           || (m_chartB && !m_chartB->uncertaintyFlags.empty())
                           || (m_compChart && !m_compChart->uncertaintyFlags.empty());
    return metadata;
  };

  auto defaultComparisonName = [&](const std::string& extension) {
    const std::string nameA = (personA_ >= 0 && personA_ < static_cast<int>(m_people.size()))
        ? m_people[personA_].fullName
        : std::string("person_a");
    const std::string nameB = (personB_ >= 0 && personB_ < static_cast<int>(m_people.size()))
        ? m_people[personB_].fullName
        : std::string("person_b");
    const std::string chartType = compareMode_ == 0 ? "synastry" : "composite";
    return buildRecommendedExportFileName(nameA + "_" + nameB,
                                          chartType,
                                          currentDateTag(),
                                          exportTheme.name,
                                          extension);
  };

  auto exportSvgAction = [&]() {
    auto path = showSaveFileDialog(
        L"Export Comparison Chart",
        defaultComparisonName("svg"),
        L"SVG Files (*.svg)\0*.svg\0All Files (*.*)\0*.*\0\0",
        L"svg");

    if (!path) return;

    const bool isSynastry = compareMode_ == 0;
    const auto scene = buildComparisonScene();
    const std::int64_t computedChartId = isSynastry && m_chartA
        ? m_chartA->computedChartId
        : (m_compChart ? m_compChart->computedChartId : 0);
    auto metadata = buildComparisonMetadata("vector");
    auto result = m_ctx.exportService.exportSvg(scene, computedChartId, *path, exportTheme, metadata);
    statusMessage_ = result.ok() ? "Exported: " + result.value().filePath
                                 : "Export failed: " + result.error().message;
  };

  auto exportPngAction = [&]() {
    auto path = showSaveFileDialog(
        L"Export Comparison Chart PNG",
        defaultComparisonName("png"),
        L"PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0\0",
        L"png");
    if (!path) return;

    const auto scene = buildComparisonScene();
    const auto spec = exportPngProfileSpec(static_cast<ExportPngProfile>(exportPngProfile_));
    const std::int64_t computedChartId = compareMode_ == 0 && m_chartA
        ? m_chartA->computedChartId
        : (m_compChart ? m_compChart->computedChartId : 0);
    auto metadata = buildComparisonMetadata(exportPngProfileLabel(static_cast<ExportPngProfile>(exportPngProfile_)));
    auto result = m_ctx.exportService.exportPng(scene,
                                                computedChartId,
                                                *path,
                                                spec.widthPx,
                                                spec.heightPx,
                                                spec.dpi,
                                                exportTheme,
                                                metadata);
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

  if (!statusMessage_.empty()) {
    ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "%s", statusMessage_.c_str());
  }

  ImGui::Separator();

  drawChartCanvas();

  ImGui::End();
}

}  // namespace asteria::ui
