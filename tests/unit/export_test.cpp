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
  auto result = service.exportSvg(scene, 42, path, theme);
  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.value().themeSnapshotJson.find("Textbook Light"), std::string::npos);
}

TEST_F(ExportServiceTest, ExportPngCreatesStubFile) {
  asteria::core::ExportService service;
  auto path = (tempDir / "test_export.png").string();
  auto result = service.exportPng(scene, chart.computedChartId, path, 800, 800, 150, theme);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(std::filesystem::exists(path));
  EXPECT_EQ(result.value().exportType, asteria::domain::ExportType::Png);
  EXPECT_EQ(result.value().widthPx, 800);
  EXPECT_EQ(result.value().heightPx, 800);
  EXPECT_EQ(result.value().dpi, 150);
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
