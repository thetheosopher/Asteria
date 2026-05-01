#pragma once
#include "core/export_service.h"
#include "core/interpretation_service.h"
#include "core/report_service.h"
#include "core/transit_report_service.h"
#include "engine/ichart_engine.h"

#include <string>
#include <vector>

namespace asteria::util { class AtlasService; }

namespace asteria::automation {

/// Simple CLI dispatcher for batch/automation workflows.
/// All operations use the same domain and engine layers as the app.
class CliDispatcher {
 public:
  explicit CliDispatcher(engine::IChartEngine& engine,
                         util::AtlasService* atlasService = nullptr);

  struct CliResult {
    bool success = false;
    std::string outputJson;
    std::string errorMessage;
  };

  struct BirthInputOptions {
    std::string dateTime;
    double latitudeDegrees = 0.0;
    double longitudeDegrees = 0.0;
    double timezoneHours = 0.0;
    double dstHours = 0.0;
  };

  struct NatalChartOptions {
    BirthInputOptions primary;
    std::string houseSystem = "Placidus";
    std::string zodiacMode = "Tropical";
  };

  struct ComparisonChartOptions {
    BirthInputOptions primary;
    BirthInputOptions secondary;
    std::string houseSystem = "Placidus";
    std::string zodiacMode = "Tropical";
  };

  struct TransitChartOptions {
    BirthInputOptions primary;
    BirthInputOptions transit;
    std::string houseSystem = "Placidus";
    std::string zodiacMode = "Tropical";
  };

  enum class ExportChartType {
    Natal,
    Synastry,
    Composite,
    TransitToNatal,
  };

  struct ExportChartOptions {
    ExportChartType chartType = ExportChartType::Natal;
    BirthInputOptions primary;
    BirthInputOptions secondary;
    BirthInputOptions transit;
    std::string houseSystem = "Placidus";
    std::string zodiacMode = "Tropical";
    std::string outputPath;
    std::string themeName = "Textbook Light";
    int widthPx = 1000;
    int heightPx = 1000;
    int dpi = 150;
  };

  struct PdfReportOptions {
    ExportChartOptions chart;
    std::string title;
    std::string sourceLabel;
    std::string modelLabel = "CLI";
    std::string interpretationMarkdown;
    bool includeBuiltIn = true;
    core::PdfReportTemplate reportTemplate = core::PdfReportTemplate::ClientReport;
    bool archivalMode = false;
    bool preferVectorChart = false;
  };

  struct TransitTimelineOptions {
    std::string subjectName;
    BirthInputOptions natal;
    std::string startDateTime;
    double rangeYears = 5.0;
    std::string houseSystem = "Placidus";
    std::string zodiacMode = "Tropical";
    core::TransitReportService::Rules rules = core::TransitReportService::defaultRules();
    std::string outputPath;
  };

  CliResult computeNatal(const NatalChartOptions& options) const;

  CliResult computeSynastry(const ComparisonChartOptions& options) const;

  CliResult computeComposite(const ComparisonChartOptions& options) const;

  CliResult computeTransitToNatal(const TransitChartOptions& options) const;

  CliResult generateTransitTimeline(const TransitTimelineOptions& options) const;

  CliResult exportSvg(const ExportChartOptions& options) const;

  CliResult exportPng(const ExportChartOptions& options) const;

  CliResult exportPdfReport(const PdfReportOptions& options) const;

  CliResult resolveLocation(const std::string& query) const;
  static std::string locationsToJson(const std::vector<domain::LocationResolution>& locations);

 private:
  engine::IChartEngine& m_engine;
  util::AtlasService* m_atlasService = nullptr;
  core::ExportService m_exportService;
  core::InterpretationService m_interpretationService;
  core::ReportService m_reportService;

  domain::ChartRequest makeRequest(const std::string& houseSystem,
                                   const std::string& zodiacMode) const;
  asteria::core::Result<domain::ResolvedBirthInput> resolveBirthInput(
      const BirthInputOptions& options,
      const std::string& label) const;
  asteria::core::Result<domain::ComputedChart> computeExportChart(const ExportChartOptions& options) const;
  asteria::core::Result<render::ChartScene> buildExportScene(const ExportChartOptions& options,
                                                             const domain::ComputedChart& chart,
                                                             const render::ThemePreset& theme) const;
  std::string chartToJson(const domain::ComputedChart& chart) const;
};

}  // namespace asteria::automation
