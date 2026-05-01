#include <gtest/gtest.h>

#include "core/report_service.h"
#include "engine/fake_chart_engine.h"
#include "render/natal_chart_layout.h"
#include "render/theme_presets.h"

#include <filesystem>

class ReportServiceTest : public ::testing::Test {
 protected:
  asteria::domain::ComputedChart chart;
  asteria::render::ChartScene scene;
  asteria::render::ThemePreset theme;
  std::filesystem::path tempDir;

  void SetUp() override {
    asteria::domain::ChartRequest request;
    request.chartRequestId = 1;
    asteria::engine::FakeChartEngine engine;
    auto result = engine.computeNatalChart(request);
    ASSERT_TRUE(result.ok());
    chart = result.value();
    theme = asteria::render::textbookLight();
    scene = asteria::render::buildNatalChartScene(chart, theme);
    tempDir = std::filesystem::temp_directory_path() / "asteria_report_service_test";
    std::filesystem::create_directories(tempDir);
  }

  void TearDown() override {
    std::filesystem::remove_all(tempDir);
  }

  asteria::core::PdfReportRequest baseRequest(const std::filesystem::path& path) const {
    asteria::core::PdfReportRequest request;
    request.title = "Natal Interpretation";
    request.subtitle = "Report fixture";
    request.sourceLabel = "Natal chart for Report fixture";
    request.modelLabel = "test-model";
    request.interpretationMarkdown = "# Overview\n\n- **Sun** in Leo\n- Moon square Mars\n\nPlain paragraph.";
    request.chartScene = scene;
    request.theme = theme;
    request.computedChartId = chart.computedChartId;
    request.outputPath = path.string();
    request.chartImageWidthPx = 320;
    request.chartImageHeightPx = 320;
    request.chartImageDpi = 144;
    return request;
  }
};

TEST_F(ReportServiceTest, MarkdownPlainTextStripsBasicMarkup) {
  const std::string plain = asteria::core::reportPlainTextFromMarkdown(
      "# Heading\n\n- **Sun** in Leo\n1. Moon square Mars\n---\nBody with `code`");
  EXPECT_NE(plain.find("Heading"), std::string::npos);
  EXPECT_NE(plain.find("- Sun in Leo"), std::string::npos);
  EXPECT_NE(plain.find("1. Moon square Mars"), std::string::npos);
  EXPECT_NE(plain.find("---"), std::string::npos);
  EXPECT_NE(plain.find("Body with code"), std::string::npos);
  EXPECT_EQ(plain.find("**"), std::string::npos);
  EXPECT_EQ(plain.find("`"), std::string::npos);
}

TEST_F(ReportServiceTest, PdfReportTemplateLabelsAreStable) {
  EXPECT_STREQ(asteria::core::pdfReportTemplateLabel(asteria::core::PdfReportTemplate::ClientReport),
               "Client Report");
  EXPECT_STREQ(asteria::core::pdfReportTemplateLabel(asteria::core::PdfReportTemplate::StudyNotes),
               "Study Notes");
  EXPECT_STREQ(asteria::core::pdfReportTemplateLabel(asteria::core::PdfReportTemplate::CompactOnePage),
               "Compact One-Page");
  EXPECT_STREQ(asteria::core::pdfReportTemplateLabel(asteria::core::PdfReportTemplate::ArchiveCopy),
               "Archive Copy");
}

TEST_F(ReportServiceTest, ExportPdfReportCreatesFile) {
  asteria::core::ReportService service;
  const auto path = tempDir / "report.pdf";
  auto result = service.exportPdfReport(baseRequest(path));
  ASSERT_TRUE(result.ok()) << result.error().message;
  EXPECT_TRUE(std::filesystem::exists(path));
  EXPECT_GT(std::filesystem::file_size(path), 1024u);
  EXPECT_EQ(result.value().exportType, asteria::domain::ExportType::Pdf);
  EXPECT_EQ(result.value().filePath, path.string());
  EXPECT_NE(result.value().exportMetadataJson.find("ai_interpretation"), std::string::npos);
  EXPECT_NE(result.value().themeSnapshotJson.find("Textbook Light"), std::string::npos);
}

TEST_F(ReportServiceTest, ExportPdfReportSupportsVectorCombinedArchiveMode) {
  asteria::core::ReportService service;
  const auto path = tempDir / "vector_archive_report.pdf";
  auto request = baseRequest(path);
  request.deterministicInterpretationMarkdown = "## Built-In\n\n- Deterministic note";
  request.reportTemplate = asteria::core::PdfReportTemplate::ArchiveCopy;
  request.archivalMode = true;
  request.preferVectorChart = true;

  auto result = service.exportPdfReport(request);
  ASSERT_TRUE(result.ok()) << result.error().message;
  EXPECT_TRUE(std::filesystem::exists(path));
  EXPECT_GT(std::filesystem::file_size(path), 1024u);
  EXPECT_NE(result.value().exportMetadataJson.find("Archive Copy"), std::string::npos);
  EXPECT_NE(result.value().exportMetadataJson.find("\"archival_mode\":true"), std::string::npos);
  EXPECT_NE(result.value().exportMetadataJson.find("\"chart_backend\":\"vector\""), std::string::npos);
}

TEST_F(ReportServiceTest, ExportPdfReportFailsOnBadPath) {
  asteria::core::ReportService service;
  auto request = baseRequest("/nonexistent/dir/report.pdf");
  request.chartImageWidthPx = 80;
  request.chartImageHeightPx = 80;
  auto result = service.exportPdfReport(request);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error().code, "pdf_write_failed");
}
