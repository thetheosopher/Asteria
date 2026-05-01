#include "cli_dispatcher.h"

#include "core/birth_event_resolver.h"
#include "core/comparison_chart_service.h"
#include "core/natal_chart_service.h"
#include "core/transit_report_service.h"
#include "render/comparison_chart_layout.h"
#include "render/natal_chart_layout.h"
#include "render/theme_presets.h"
#include "render/transit_chart_layout.h"
#include "util/atlas_service.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <ctime>

namespace asteria::automation {

namespace {

std::string jsonEscape(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (char ch : value) {
    switch (ch) {
      case '\\': escaped += "\\\\"; break;
      case '"': escaped += "\\\""; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default: escaped.push_back(ch); break;
    }
  }
  return escaped;
}

void appendIndent(std::ostringstream& out, int indent) {
  out << std::string(indent, ' ');
}

void appendStringArray(std::ostringstream& out,
                       const std::vector<std::string>& values,
                       int) {
  out << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) out << ", ";
    out << '"' << jsonEscape(values[index]) << '"';
  }
  out << "]";
}

void appendAspects(std::ostringstream& out,
                   const std::vector<domain::Aspect>& aspects,
                   int indent) {
  out << "[\n";
  for (std::size_t index = 0; index < aspects.size(); ++index) {
    const auto& aspect = aspects[index];
    appendIndent(out, indent + 2);
    out << "{\"objectA\": \"" << jsonEscape(aspect.objectA)
        << "\", \"objectB\": \"" << jsonEscape(aspect.objectB)
        << "\", \"type\": \"" << jsonEscape(aspect.aspectType)
        << "\", \"orb\": " << aspect.orbDegrees;
    if (aspect.applyingOrSeparating) {
      out << ", \"phase\": \"" << jsonEscape(*aspect.applyingOrSeparating) << '"';
    }
    out << "}";
    if (index + 1 < aspects.size()) out << ",";
    out << "\n";
  }
  appendIndent(out, indent);
  out << "]";
}

void appendChartJson(std::ostringstream& out,
                     const domain::ComputedChart& chart,
                     int indent) {
  appendIndent(out, indent);
  out << "{\n";

  appendIndent(out, indent + 2);
  out << "\"engineVersion\": \"" << jsonEscape(chart.engineVersion) << "\",\n";
  appendIndent(out, indent + 2);
  out << "\"engineMethod\": \"" << jsonEscape(chart.engineMethod) << "\",\n";
  appendIndent(out, indent + 2);
  out << "\"houseSystem\": \"" << jsonEscape(chart.houseSystem) << "\",\n";
  appendIndent(out, indent + 2);
  out << "\"zodiacMode\": \"" << jsonEscape(chart.zodiacMode) << "\",\n";
  appendIndent(out, indent + 2);
  out << "\"canonicalHash\": \"" << jsonEscape(chart.canonicalHash) << "\",\n";

  appendIndent(out, indent + 2);
  out << "\"planets\": [\n";
  for (std::size_t index = 0; index < chart.planets.size(); ++index) {
    const auto& planet = chart.planets[index];
    appendIndent(out, indent + 4);
    out << "{\"objectId\": \"" << jsonEscape(planet.objectId)
        << "\", \"longitude\": " << planet.longitudeDegrees
        << ", \"sign\": \"" << jsonEscape(planet.sign)
        << "\", \"retrograde\": " << (planet.retrograde ? "true" : "false") << "}";
    if (index + 1 < chart.planets.size()) out << ",";
    out << "\n";
  }
  appendIndent(out, indent + 2);
  out << "],\n";

  appendIndent(out, indent + 2);
  out << "\"houseCusps\": [\n";
  for (std::size_t index = 0; index < chart.houseCusps.size(); ++index) {
    const auto& cusp = chart.houseCusps[index];
    appendIndent(out, indent + 4);
    out << "{\"house\": " << cusp.houseNumber
        << ", \"longitude\": " << cusp.longitudeDegrees << "}";
    if (index + 1 < chart.houseCusps.size()) out << ",";
    out << "\n";
  }
  appendIndent(out, indent + 2);
  out << "],\n";

  appendIndent(out, indent + 2);
  out << "\"aspects\": ";
  appendAspects(out, chart.aspects, indent + 2);
  out << ",\n";

  appendIndent(out, indent + 2);
  out << "\"interAspects\": ";
  appendAspects(out, chart.interAspects, indent + 2);
  out << ",\n";

  appendIndent(out, indent + 2);
  out << "\"uncertaintyFlags\": ";
  appendStringArray(out, chart.uncertaintyFlags, indent + 2);
  out << ",\n";

  appendIndent(out, indent + 2);
  out << "\"secondaryChart\": ";
  if (chart.secondaryChart) {
    appendChartJson(out, *chart.secondaryChart, indent + 2);
  } else {
    out << "null";
  }
  out << "\n";

  appendIndent(out, indent);
  out << "}";
}

render::ThemePreset resolveTheme(const std::string& name) {
  if (name == "Textbook Monochrome") return render::textbookMonochrome();
  if (name == "Luxury Light") return render::luxuryLight();
  if (name == "Luxury Dark") return render::luxuryDark();
  return render::textbookLight();
}

std::string exportChartTypeName(CliDispatcher::ExportChartType chartType) {
  switch (chartType) {
    case CliDispatcher::ExportChartType::Natal:
      return "natal";
    case CliDispatcher::ExportChartType::Synastry:
      return "synastry";
    case CliDispatcher::ExportChartType::Composite:
      return "composite";
    case CliDispatcher::ExportChartType::TransitToNatal:
      return "transit_to_natal";
  }
  return "natal";
}

domain::ChartType domainChartType(CliDispatcher::ExportChartType chartType) {
  switch (chartType) {
    case CliDispatcher::ExportChartType::Synastry:
      return domain::ChartType::Synastry;
    case CliDispatcher::ExportChartType::Composite:
      return domain::ChartType::Composite;
    case CliDispatcher::ExportChartType::TransitToNatal:
      return domain::ChartType::TransitToNatal;
    case CliDispatcher::ExportChartType::Natal:
      return domain::ChartType::Natal;
  }
  return domain::ChartType::Natal;
}

std::string generatedAtLabel() {
  std::time_t now = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  local = *std::localtime(&now);
#endif
  char buffer[32] = {};
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
  return buffer;
}

domain::LocationResolution toLocationResolution(const util::AtlasEntry& entry,
                                                const std::string& query) {
  domain::LocationResolution resolution;
  resolution.queryText = query;
  resolution.displayName = entry.name;
  resolution.region = entry.regionCode;
  resolution.latitude = entry.latitude;
  resolution.longitude = entry.longitude;
  resolution.timezoneName = entry.timezoneName;
  resolution.resolutionMethod = "atlas";
  resolution.confidenceScore = 0.9;
  return resolution;
}

}  // namespace

CliDispatcher::CliDispatcher(engine::IChartEngine& engine,
                             util::AtlasService* atlasService)
    : m_engine(engine),
      m_atlasService(atlasService) {}

domain::ChartRequest CliDispatcher::makeRequest(
    const std::string& houseSystem,
    const std::string& zodiacMode) const {
  domain::ChartRequest request;
  request.defaultHouseSystem = houseSystem;
  request.zodiacMode = zodiacMode;
  request.includeHouses = true;
  return request;
}

asteria::core::Result<domain::ResolvedBirthInput> CliDispatcher::resolveBirthInput(
    const BirthInputOptions& options,
    const std::string& label) const {
  auto input = core::resolveTransitMoment(options.dateTime,
                                          options.latitudeDegrees,
                                          options.longitudeDegrees,
                                          options.timezoneHours);
  if (!input) {
    return asteria::core::Result<domain::ResolvedBirthInput>::failure(
        {"invalid_datetime", label + " datetime must be YYYY-MM-DDTHH:MM[:SS]."});
  }

  input->dstHours = options.dstHours;
  return asteria::core::Result<domain::ResolvedBirthInput>::success(*input);
}

std::string CliDispatcher::chartToJson(const domain::ComputedChart& chart) const {
  std::ostringstream out;
  appendChartJson(out, chart, 0);
  out << "\n";
  return out.str();
}

std::string CliDispatcher::locationsToJson(
    const std::vector<domain::LocationResolution>& locations) {
  std::ostringstream out;
  out << "{\n  \"results\": [\n";
  for (std::size_t index = 0; index < locations.size(); ++index) {
    const auto& location = locations[index];
    out << "    {\"displayName\": \"" << jsonEscape(location.displayName)
        << "\", \"latitude\": " << location.latitude
        << ", \"longitude\": " << location.longitude
        << ", \"timezone\": \"" << jsonEscape(location.timezoneName)
        << "\", \"confidence\": " << location.confidenceScore << "}";
    if (index + 1 < locations.size()) out << ",";
    out << "\n";
  }
  out << "  ]\n}\n";
  return out.str();
}

CliDispatcher::CliResult CliDispatcher::computeNatal(
    const NatalChartOptions& options) const {
  auto primaryInput = resolveBirthInput(options.primary, "Primary");
  if (!primaryInput.ok()) return {false, "", primaryInput.error().message};

  auto request = makeRequest(options.houseSystem, options.zodiacMode);
  request.chartType = domain::ChartType::Natal;
  request.primaryInput = primaryInput.value();

  core::NatalChartService service(const_cast<engine::IChartEngine&>(m_engine));
  auto result = service.compute(request);
  if (!result.ok()) return {false, "", result.error().message};
  return {true, chartToJson(result.value()), ""};
}

CliDispatcher::CliResult CliDispatcher::computeSynastry(
    const ComparisonChartOptions& options) const {
  auto primaryInput = resolveBirthInput(options.primary, "Primary");
  if (!primaryInput.ok()) return {false, "", primaryInput.error().message};
  auto secondaryInput = resolveBirthInput(options.secondary, "Secondary");
  if (!secondaryInput.ok()) return {false, "", secondaryInput.error().message};

  auto request = makeRequest(options.houseSystem, options.zodiacMode);
  request.chartType = domain::ChartType::Synastry;
  request.primaryInput = primaryInput.value();
  request.secondaryInput = secondaryInput.value();

  core::ComparisonChartService service(const_cast<engine::IChartEngine&>(m_engine));
  auto result = service.computeSynastry(request);
  if (!result.ok()) return {false, "", result.error().message};
  return {true, chartToJson(result.value()), ""};
}

CliDispatcher::CliResult CliDispatcher::computeComposite(
    const ComparisonChartOptions& options) const {
  auto primaryInput = resolveBirthInput(options.primary, "Primary");
  if (!primaryInput.ok()) return {false, "", primaryInput.error().message};
  auto secondaryInput = resolveBirthInput(options.secondary, "Secondary");
  if (!secondaryInput.ok()) return {false, "", secondaryInput.error().message};

  auto request = makeRequest(options.houseSystem, options.zodiacMode);
  request.chartType = domain::ChartType::Composite;
  request.primaryInput = primaryInput.value();
  request.secondaryInput = secondaryInput.value();

  core::ComparisonChartService service(const_cast<engine::IChartEngine&>(m_engine));
  auto result = service.computeComposite(request);
  if (!result.ok()) return {false, "", result.error().message};
  return {true, chartToJson(result.value()), ""};
}

CliDispatcher::CliResult CliDispatcher::computeTransitToNatal(
    const TransitChartOptions& options) const {
  auto primaryInput = resolveBirthInput(options.primary, "Primary");
  if (!primaryInput.ok()) return {false, "", primaryInput.error().message};
  auto transitInput = resolveBirthInput(options.transit, "Transit");
  if (!transitInput.ok()) return {false, "", transitInput.error().message};

  auto request = makeRequest(options.houseSystem, options.zodiacMode);
  request.chartType = domain::ChartType::TransitToNatal;
  request.primaryInput = primaryInput.value();
  request.transitInput = transitInput.value();
  request.requestTimeContext = options.transit.dateTime;

  core::ComparisonChartService service(const_cast<engine::IChartEngine&>(m_engine));
  auto result = service.computeTransitToNatal(request);
  if (!result.ok()) return {false, "", result.error().message};
  return {true, chartToJson(result.value()), ""};
}

CliDispatcher::CliResult CliDispatcher::generateTransitTimeline(
    const TransitTimelineOptions& options) const {
  auto natalInput = resolveBirthInput(options.natal, "Natal");
  if (!natalInput.ok()) return {false, "", natalInput.error().message};

  core::TransitReportService::Request request;
  request.chartRequest.chartType = domain::ChartType::Natal;
  request.chartRequest.defaultHouseSystem = options.houseSystem;
  request.chartRequest.zodiacMode = options.zodiacMode;
  request.chartRequest.primaryInput = natalInput.value();
  request.subjectName = options.subjectName;
  request.startDateTime = options.startDateTime;
  request.rangeYears = options.rangeYears;
  request.rules = options.rules;

  core::TransitReportService service(const_cast<engine::IChartEngine&>(m_engine));
  auto result = service.generate(request);
  if (!result.ok()) {
    return {false, "", result.error().message};
  }

  if (!options.outputPath.empty()) {
    std::filesystem::path outputPath(options.outputPath);
    if (outputPath.has_parent_path()) {
      std::error_code ec;
      std::filesystem::create_directories(outputPath.parent_path(), ec);
      if (ec) {
        return {false, "", "Failed to create output directory: " + outputPath.parent_path().string()};
      }
    }

    std::ofstream out(options.outputPath, std::ios::binary | std::ios::trunc);
    if (!out) {
      return {false, "", "Failed to open output file: " + options.outputPath};
    }
    out << result.value().bodyMarkdown;
    if (!out.good()) {
      return {false, "", "Failed to write output file: " + options.outputPath};
    }
  }

  return {true, result.value().bodyMarkdown, ""};
}

asteria::core::Result<domain::ComputedChart> CliDispatcher::computeExportChart(
    const ExportChartOptions& options) const {
  switch (options.chartType) {
    case ExportChartType::Natal: {
      auto primaryInput = resolveBirthInput(options.primary, "Primary");
      if (!primaryInput.ok()) {
        return asteria::core::Result<domain::ComputedChart>::failure(primaryInput.error());
      }

      auto request = makeRequest(options.houseSystem, options.zodiacMode);
      request.chartType = domain::ChartType::Natal;
      request.primaryInput = primaryInput.value();

      core::NatalChartService service(const_cast<engine::IChartEngine&>(m_engine));
      return service.compute(request);
    }
    case ExportChartType::Synastry: {
      auto primaryInput = resolveBirthInput(options.primary, "Primary");
      if (!primaryInput.ok()) {
        return asteria::core::Result<domain::ComputedChart>::failure(primaryInput.error());
      }
      auto secondaryInput = resolveBirthInput(options.secondary, "Secondary");
      if (!secondaryInput.ok()) {
        return asteria::core::Result<domain::ComputedChart>::failure(secondaryInput.error());
      }

      auto request = makeRequest(options.houseSystem, options.zodiacMode);
      request.chartType = domain::ChartType::Synastry;
      request.primaryInput = primaryInput.value();
      request.secondaryInput = secondaryInput.value();

      core::ComparisonChartService service(const_cast<engine::IChartEngine&>(m_engine));
      return service.computeSynastry(request);
    }
    case ExportChartType::Composite: {
      auto primaryInput = resolveBirthInput(options.primary, "Primary");
      if (!primaryInput.ok()) {
        return asteria::core::Result<domain::ComputedChart>::failure(primaryInput.error());
      }
      auto secondaryInput = resolveBirthInput(options.secondary, "Secondary");
      if (!secondaryInput.ok()) {
        return asteria::core::Result<domain::ComputedChart>::failure(secondaryInput.error());
      }

      auto request = makeRequest(options.houseSystem, options.zodiacMode);
      request.chartType = domain::ChartType::Composite;
      request.primaryInput = primaryInput.value();
      request.secondaryInput = secondaryInput.value();

      core::ComparisonChartService service(const_cast<engine::IChartEngine&>(m_engine));
      return service.computeComposite(request);
    }
    case ExportChartType::TransitToNatal: {
      auto primaryInput = resolveBirthInput(options.primary, "Primary");
      if (!primaryInput.ok()) {
        return asteria::core::Result<domain::ComputedChart>::failure(primaryInput.error());
      }
      auto transitInput = resolveBirthInput(options.transit, "Transit");
      if (!transitInput.ok()) {
        return asteria::core::Result<domain::ComputedChart>::failure(transitInput.error());
      }

      auto request = makeRequest(options.houseSystem, options.zodiacMode);
      request.chartType = domain::ChartType::TransitToNatal;
      request.primaryInput = primaryInput.value();
      request.transitInput = transitInput.value();
      request.requestTimeContext = options.transit.dateTime;

      core::ComparisonChartService service(const_cast<engine::IChartEngine&>(m_engine));
      return service.computeTransitToNatal(request);
    }
  }

  return asteria::core::Result<domain::ComputedChart>::failure(
      {"invalid_chart_type", "Unsupported export chart type."});
}

asteria::core::Result<render::ChartScene> CliDispatcher::buildExportScene(
    const ExportChartOptions& options,
    const domain::ComputedChart& chart,
    const render::ThemePreset& theme) const {
  switch (options.chartType) {
    case ExportChartType::Natal:
      return asteria::core::Result<render::ChartScene>::success(
          render::buildNatalChartScene(chart, theme));
    case ExportChartType::Synastry:
      if (!chart.secondaryChart) {
        return asteria::core::Result<render::ChartScene>::failure(
            {"missing_secondary_chart", "Synastry export requires the secondary chart to be present."});
      }
      return asteria::core::Result<render::ChartScene>::success(
          render::buildSynastryChartScene(chart, *chart.secondaryChart, theme));
    case ExportChartType::Composite:
      return asteria::core::Result<render::ChartScene>::success(
          render::buildCompositeChartScene(chart, theme));
    case ExportChartType::TransitToNatal:
      if (!chart.secondaryChart) {
        return asteria::core::Result<render::ChartScene>::failure(
            {"missing_secondary_chart", "Transit export requires the transit chart to be present."});
      }
      return asteria::core::Result<render::ChartScene>::success(
          render::buildTransitToNatalChartScene(chart, *chart.secondaryChart, theme));
  }

  return asteria::core::Result<render::ChartScene>::failure(
      {"invalid_chart_type", "Unsupported export chart type."});
}

CliDispatcher::CliResult CliDispatcher::exportSvg(
    const ExportChartOptions& options) const {
  auto chart = computeExportChart(options);
  if (!chart.ok()) return {false, "", chart.error().message};

  auto theme = resolveTheme(options.themeName);
  auto scene = buildExportScene(options, chart.value(), theme);
  if (!scene.ok()) return {false, "", scene.error().message};

  core::ExportMetadata metadata;
  metadata.chartType = exportChartTypeName(options.chartType);
  metadata.exportProfile = "vector";
  metadata.layoutTemplate = "chart_only";
  metadata.hasWarnings = !chart.value().uncertaintyFlags.empty();
  auto exportResult = m_exportService.exportSvg(scene.value(), chart.value().computedChartId,
                                                options.outputPath, theme, metadata);
  if (!exportResult.ok()) return {false, "", exportResult.error().message};
  return {true, R"({"exported":")" + jsonEscape(options.outputPath) + R"(","format":"svg"})", ""};
}

CliDispatcher::CliResult CliDispatcher::exportPng(
    const ExportChartOptions& options) const {
  auto chart = computeExportChart(options);
  if (!chart.ok()) return {false, "", chart.error().message};

  auto theme = resolveTheme(options.themeName);
  auto scene = buildExportScene(options, chart.value(), theme);
  if (!scene.ok()) return {false, "", scene.error().message};

  core::ExportMetadata metadata;
  metadata.chartType = exportChartTypeName(options.chartType);
  metadata.exportProfile = "custom";
  metadata.layoutTemplate = "chart_only";
  metadata.hasWarnings = !chart.value().uncertaintyFlags.empty();
  auto exportResult = m_exportService.exportPng(scene.value(), chart.value().computedChartId,
                                                options.outputPath, options.widthPx,
                                                options.heightPx, options.dpi, theme, metadata);
  if (!exportResult.ok()) return {false, "", exportResult.error().message};
  return {true, R"({"exported":")" + jsonEscape(options.outputPath) + R"(","format":"png"})", ""};
}

CliDispatcher::CliResult CliDispatcher::exportPdfReport(
    const PdfReportOptions& options) const {
  if (options.chart.outputPath.empty()) {
    return {false, "", "--output is required for export-ai-report-pdf."};
  }

  auto chart = computeExportChart(options.chart);
  if (!chart.ok()) return {false, "", chart.error().message};

  auto theme = resolveTheme(options.chart.themeName);
  auto scene = buildExportScene(options.chart, chart.value(), theme);
  if (!scene.ok()) return {false, "", scene.error().message};

  std::string builtInMarkdown;
  if (options.includeBuiltIn) {
    auto builtIn = m_interpretationService.generateBuiltIn(chart.value(), domainChartType(options.chart.chartType));
    if (!builtIn.ok()) return {false, "", builtIn.error().message};
    builtInMarkdown = builtIn.value().bodyMarkdown;
  }

  std::filesystem::path outputPath(options.chart.outputPath);
  if (outputPath.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec) {
      return {false, "", "Failed to create output directory: " + outputPath.parent_path().string()};
    }
  }

  core::PdfReportRequest request;
  request.title = options.title.empty()
      ? (exportChartTypeName(options.chart.chartType) + " AI Report")
      : options.title;
  request.subtitle = options.sourceLabel.empty() ? exportChartTypeName(options.chart.chartType) : options.sourceLabel;
  request.sourceLabel = request.subtitle;
  request.modelLabel = options.modelLabel.empty() ? std::string("CLI") : options.modelLabel;
  request.generatedAtLabel = generatedAtLabel();
  request.interpretationMarkdown = options.interpretationMarkdown;
  request.deterministicInterpretationMarkdown = builtInMarkdown;
  request.chartScene = scene.value();
  request.theme = theme;
  request.computedChartId = chart.value().computedChartId;
  request.outputPath = options.chart.outputPath;
  request.reportTemplate = options.reportTemplate;
  request.archivalMode = options.archivalMode || options.reportTemplate == core::PdfReportTemplate::ArchiveCopy;
  request.preferVectorChart = options.preferVectorChart;
  request.chartImageWidthPx = options.chart.widthPx;
  request.chartImageHeightPx = options.chart.heightPx;
  request.chartImageDpi = options.chart.dpi;

  auto exportResult = m_reportService.exportPdfReport(request);
  if (!exportResult.ok()) return {false, "", exportResult.error().message};
  return {true,
        R"({"exported":")" + jsonEscape(options.chart.outputPath) +
          R"(","format":"pdf","template":")" + jsonEscape(core::pdfReportTemplateLabel(options.reportTemplate)) +
              R"(","chartBackend":")" + (options.preferVectorChart ? "vector" : "raster") + R"("})",
          ""};
}

CliDispatcher::CliResult CliDispatcher::resolveLocation(const std::string& query) const {
  if (m_atlasService != nullptr && m_atlasService->isLoaded()) {
    std::vector<domain::LocationResolution> locations;
    for (const auto* entry : m_atlasService->search(query, 20)) {
      locations.push_back(toLocationResolution(*entry, query));
    }
    return {true, locationsToJson(locations), ""};
  }

  auto result = m_engine.resolveLocation(query);
  if (!result.ok()) return {false, "", result.error().message};
  return {true, locationsToJson(result.value()), ""};
}

}  // namespace asteria::automation
