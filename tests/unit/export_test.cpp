#include <gtest/gtest.h>
#include "core/export_service.h"
#include "render/natal_chart_layout.h"
#include "render/theme_presets.h"
#include "engine/fake_chart_engine.h"
#include <filesystem>

class ExportServiceTest : public ::testing::Test {
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
    tempDir = std::filesystem::temp_directory_path() / "asteria_export_test";
    std::filesystem::create_directories(tempDir);
  }

  void TearDown() override {
    std::filesystem::remove_all(tempDir);
  }
};

TEST_F(ExportServiceTest, ExportSvgCreatesFile) {
  asteria::core::ExportService service;
  auto path = (tempDir / "test_export.svg").string();
  auto result = service.exportSvg(scene, chart.computedChartId, path, theme);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(std::filesystem::exists(path));
  EXPECT_GT(std::filesystem::file_size(path), 0u);
  EXPECT_EQ(result.value().exportType, asteria::domain::ExportType::Svg);
  EXPECT_EQ(result.value().filePath, path);
}

TEST_F(ExportServiceTest, ExportSvgRecordsTheme) {
  asteria::core::ExportService service;
  auto path = (tempDir / "themed.svg").string();
  asteria::core::ExportMetadata metadata;
  metadata.chartType = "natal";
  metadata.exportProfile = "vector";
  metadata.layoutTemplate = "reference_sheet";
  metadata.dateTag = "2026-04-25";
  metadata.hasWarnings = true;
  auto result = service.exportSvg(scene, 42, path, theme, metadata);
  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.value().themeSnapshotJson.find("Textbook Light"), std::string::npos);
  EXPECT_NE(result.value().exportMetadataJson.find("chart_type"), std::string::npos);
  EXPECT_NE(result.value().exportMetadataJson.find("reference_sheet"), std::string::npos);
  EXPECT_NE(result.value().exportMetadataJson.find("2026-04-25"), std::string::npos);
  EXPECT_FALSE(result.value().exportedAt.empty());
}

TEST_F(ExportServiceTest, ExportPngCreatesFile) {
  asteria::core::ExportService service;
  auto path = (tempDir / "test_export.png").string();
  asteria::core::ExportMetadata metadata;
  metadata.chartType = "natal";
  metadata.exportProfile = "screen_share";
  metadata.layoutTemplate = "chart_only";
  metadata.dateTag = "2026-04-25";
  auto result = service.exportPng(scene, chart.computedChartId, path, 800, 800, 150, theme, metadata);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(std::filesystem::exists(path));
  EXPECT_GT(std::filesystem::file_size(path), 64u);
  EXPECT_EQ(result.value().exportType, asteria::domain::ExportType::Png);
  EXPECT_EQ(result.value().widthPx, 800);
  EXPECT_EQ(result.value().heightPx, 800);
  EXPECT_EQ(result.value().dpi, 150);
  EXPECT_NE(result.value().exportMetadataJson.find("screen_share"), std::string::npos);
}

TEST_F(ExportServiceTest, ExportSvgFailsOnBadPath) {
  asteria::core::ExportService service;
  auto result = service.exportSvg(scene, 1, "/nonexistent/dir/bad.svg", theme);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error().code, "export_failed");
}

TEST_F(ExportServiceTest, ExportPngFailsOnBadPath) {
  asteria::core::ExportService service;
  auto result = service.exportPng(scene, 1, "/nonexistent/dir/bad.png", 1000, 1000, 150, theme);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error().code, "export_failed");
}
