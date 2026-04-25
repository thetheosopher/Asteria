#include "cli_dispatcher.h"
#include "core/natal_chart_service.h"
#include "core/comparison_chart_service.h"
#include "render/natal_chart_layout.h"
#include "render/comparison_chart_layout.h"
#include "render/transit_chart_layout.h"
#include "render/theme_presets.h"
#include <sstream>

namespace asteria::automation {

CliDispatcher::CliDispatcher(engine::IChartEngine& engine) : m_engine(engine) {}

domain::ChartRequest CliDispatcher::makeRequest(
    std::int64_t primaryId,
    const std::string& houseSystem,
    const std::string& zodiacMode) const {
  domain::ChartRequest request;
  request.primaryBirthEventId = primaryId;
  request.defaultHouseSystem = houseSystem;
  request.zodiacMode = zodiacMode;
  request.includeHouses = true;
  return request;
}

std::string CliDispatcher::chartToJson(const domain::ComputedChart& chart) const {
  std::ostringstream ss;
  ss << "{\n";
  ss << "  \"engineVersion\": \"" << chart.engineVersion << "\",\n";
  ss << "  \"engineMethod\": \"" << chart.engineMethod << "\",\n";
  ss << "  \"houseSystem\": \"" << chart.houseSystem << "\",\n";
  ss << "  \"zodiacMode\": \"" << chart.zodiacMode << "\",\n";
  ss << "  \"canonicalHash\": \"" << chart.canonicalHash << "\",\n";
  ss << "  \"planets\": [\n";
  for (size_t i = 0; i < chart.planets.size(); ++i) {
    const auto& p = chart.planets[i];
    ss << "    {\"objectId\": \"" << p.objectId
       << "\", \"longitude\": " << p.longitudeDegrees
       << ", \"sign\": \"" << p.sign
       << "\", \"retrograde\": " << (p.retrograde ? "true" : "false") << "}";
    if (i + 1 < chart.planets.size()) ss << ",";
    ss << "\n";
  }
  ss << "  ],\n";
  ss << "  \"houseCusps\": [\n";
  for (size_t i = 0; i < chart.houseCusps.size(); ++i) {
    const auto& h = chart.houseCusps[i];
    ss << "    {\"house\": " << h.houseNumber
       << ", \"longitude\": " << h.longitudeDegrees << "}";
    if (i + 1 < chart.houseCusps.size()) ss << ",";
    ss << "\n";
  }
  ss << "  ],\n";
  ss << "  \"aspects\": [\n";
  for (size_t i = 0; i < chart.aspects.size(); ++i) {
    const auto& a = chart.aspects[i];
    ss << "    {\"objectA\": \"" << a.objectA
       << "\", \"objectB\": \"" << a.objectB
       << "\", \"type\": \"" << a.aspectType
       << "\", \"orb\": " << a.orbDegrees << "}";
    if (i + 1 < chart.aspects.size()) ss << ",";
    ss << "\n";
  }
  ss << "  ],\n";
  ss << "  \"uncertaintyFlags\": [";
  for (size_t i = 0; i < chart.uncertaintyFlags.size(); ++i) {
    ss << "\"" << chart.uncertaintyFlags[i] << "\"";
    if (i + 1 < chart.uncertaintyFlags.size()) ss << ", ";
  }
  ss << "]\n";
  ss << "}\n";
  return ss.str();
}

static render::ThemePreset resolveTheme(const std::string& name) {
  if (name == "Textbook Monochrome") return render::textbookMonochrome();
  if (name == "Luxury Light") return render::luxuryLight();
  if (name == "Luxury Dark") return render::luxuryDark();
  return render::textbookLight();
}

CliDispatcher::CliResult CliDispatcher::computeNatal(
    std::int64_t birthEventId,
    const std::string& houseSystem,
    const std::string& zodiacMode) const {
  auto request = makeRequest(birthEventId, houseSystem, zodiacMode);
  request.chartType = domain::ChartType::Natal;
  core::NatalChartService service(const_cast<engine::IChartEngine&>(m_engine));
  auto result = service.compute(request);
  if (!result.ok()) return {false, "", result.error().message};
  return {true, chartToJson(result.value()), ""};
}

CliDispatcher::CliResult CliDispatcher::computeSynastry(
    std::int64_t primaryBirthEventId,
    std::int64_t secondaryBirthEventId,
    const std::string& houseSystem,
    const std::string& zodiacMode) const {
  auto request = makeRequest(primaryBirthEventId, houseSystem, zodiacMode);
  request.chartType = domain::ChartType::Synastry;
  request.secondaryBirthEventId = secondaryBirthEventId;
  core::ComparisonChartService service(const_cast<engine::IChartEngine&>(m_engine));
  auto result = service.computeSynastry(request);
  if (!result.ok()) return {false, "", result.error().message};
  return {true, chartToJson(result.value()), ""};
}

CliDispatcher::CliResult CliDispatcher::computeComposite(
    std::int64_t primaryBirthEventId,
    std::int64_t secondaryBirthEventId,
    const std::string& houseSystem,
    const std::string& zodiacMode) const {
  auto request = makeRequest(primaryBirthEventId, houseSystem, zodiacMode);
  request.chartType = domain::ChartType::Composite;
  request.secondaryBirthEventId = secondaryBirthEventId;
  core::ComparisonChartService service(const_cast<engine::IChartEngine&>(m_engine));
  auto result = service.computeComposite(request);
  if (!result.ok()) return {false, "", result.error().message};
  return {true, chartToJson(result.value()), ""};
}

CliDispatcher::CliResult CliDispatcher::computeTransitToNatal(
    std::int64_t birthEventId,
    const std::string& transitDateTime,
    const std::string& houseSystem,
    const std::string& zodiacMode) const {
  auto request = makeRequest(birthEventId, houseSystem, zodiacMode);
  request.chartType = domain::ChartType::TransitToNatal;
  request.requestTimeContext = transitDateTime;
  core::ComparisonChartService service(const_cast<engine::IChartEngine&>(m_engine));
  auto result = service.computeTransitToNatal(request);
  if (!result.ok()) return {false, "", result.error().message};
  return {true, chartToJson(result.value()), ""};
}

CliDispatcher::CliResult CliDispatcher::exportSvg(
    std::int64_t birthEventId,
    const std::string& outputPath,
    const std::string& themeName) const {
  auto request = makeRequest(birthEventId, "Placidus", "Tropical");
  request.chartType = domain::ChartType::Natal;
  auto result = m_engine.computeNatalChart(request);
  if (!result.ok()) return {false, "", result.error().message};

  auto theme = resolveTheme(themeName);
  auto scene = render::buildNatalChartScene(result.value(), theme);
  core::ExportMetadata metadata;
  metadata.chartType = "natal";
  metadata.exportProfile = "vector";
  metadata.layoutTemplate = "chart_only";
  metadata.hasWarnings = !result.value().uncertaintyFlags.empty();
  auto exportResult = m_exportService.exportSvg(scene, result.value().computedChartId, outputPath, theme, metadata);
  if (!exportResult.ok()) return {false, "", exportResult.error().message};
  return {true, R"({"exported":")" + outputPath + R"(","format":"svg"})", ""};
}

CliDispatcher::CliResult CliDispatcher::exportPng(
    std::int64_t birthEventId,
    const std::string& outputPath,
    int widthPx, int heightPx, int dpi,
    const std::string& themeName) const {
  auto request = makeRequest(birthEventId, "Placidus", "Tropical");
  request.chartType = domain::ChartType::Natal;
  auto result = m_engine.computeNatalChart(request);
  if (!result.ok()) return {false, "", result.error().message};

  auto theme = resolveTheme(themeName);
  auto scene = render::buildNatalChartScene(result.value(), theme);
  core::ExportMetadata metadata;
  metadata.chartType = "natal";
  metadata.exportProfile = "custom";
  metadata.layoutTemplate = "chart_only";
  metadata.hasWarnings = !result.value().uncertaintyFlags.empty();
  auto exportResult = m_exportService.exportPng(scene, result.value().computedChartId,
                                                 outputPath, widthPx, heightPx, dpi, theme, metadata);
  if (!exportResult.ok()) return {false, "", exportResult.error().message};
  return {true, R"({"exported":")" + outputPath + R"(","format":"png"})", ""};
}

CliDispatcher::CliResult CliDispatcher::resolveLocation(const std::string& query) const {
  auto result = m_engine.resolveLocation(query);
  if (!result.ok()) return {false, "", result.error().message};

  std::ostringstream ss;
  ss << "{\n  \"results\": [\n";
  const auto& locations = result.value();
  for (size_t i = 0; i < locations.size(); ++i) {
    const auto& loc = locations[i];
    ss << "    {\"displayName\": \"" << loc.displayName
       << "\", \"latitude\": " << loc.latitude
       << ", \"longitude\": " << loc.longitude
       << ", \"timezone\": \"" << loc.timezoneName
       << "\", \"confidence\": " << loc.confidenceScore << "}";
    if (i + 1 < locations.size()) ss << ",";
    ss << "\n";
  }
  ss << "  ]\n}\n";
  return {true, ss.str(), ""};
}

}  // namespace asteria::automation
