#pragma once

#include <cstdint>
#include <string>

#include "domain/export_artifact.h"
#include "render/chart_scene.h"
#include "render/theme_presets.h"
#include "result.h"

namespace asteria::core {

enum class PdfReportTemplate {
  ClientReport,
  StudyNotes,
  CompactOnePage,
  ArchiveCopy,
};

struct PdfReportRequest {
  std::string title;
  std::string subtitle;
  std::string sourceLabel;
  std::string modelLabel;
  std::string generatedAtLabel;
  std::string interpretationMarkdown;
  std::string deterministicInterpretationMarkdown;
  render::ChartScene chartScene;
  render::ThemePreset theme;
  std::int64_t computedChartId = 0;
  std::string outputPath;
  PdfReportTemplate reportTemplate = PdfReportTemplate::ClientReport;
  bool archivalMode = false;
  bool preferVectorChart = false;
  std::string regularFontPath;
  std::string boldFontPath;
  int chartImageWidthPx = 1800;
  int chartImageHeightPx = 1800;
  int chartImageDpi = 300;
};

const char* pdfReportTemplateLabel(PdfReportTemplate reportTemplate);
std::string reportPlainTextFromMarkdown(const std::string& markdown);

class ReportService {
 public:
  Result<domain::ExportArtifact> exportPdfReport(const PdfReportRequest& request) const;
};

}  // namespace asteria::core
