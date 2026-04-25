#include <gtest/gtest.h>
#include "render/chart_scene.h"
#include "render/natal_chart_layout.h"
#include "render/svg_serializer.h"
#include "render/theme_presets.h"
#include "engine/fake_chart_engine.h"
#include <fstream>
#include <filesystem>

class RenderTest : public ::testing::Test {
 protected:
  asteria::domain::ComputedChart chart;

  void SetUp() override {
    asteria::domain::ChartRequest request;
    request.chartRequestId = 1;
    request.chartType = asteria::domain::ChartType::Natal;
    asteria::engine::FakeChartEngine engine;
    auto result = engine.computeNatalChart(request);
    ASSERT_TRUE(result.ok());
    chart = result.value();
  }
};

TEST_F(RenderTest, TextbookLightTheme) {
  auto theme = asteria::render::textbookLight();
  EXPECT_EQ(theme.name, "Textbook Light");
}

TEST_F(RenderTest, TextbookMonochromeTheme) {
  auto theme = asteria::render::textbookMonochrome();
  EXPECT_EQ(theme.name, "Textbook Monochrome");
}

TEST_F(RenderTest, LuxuryLightTheme) {
  auto theme = asteria::render::luxuryLight();
  EXPECT_EQ(theme.name, "Luxury Light");
}

TEST_F(RenderTest, LuxuryDarkTheme) {
  auto theme = asteria::render::luxuryDark();
  EXPECT_EQ(theme.name, "Luxury Dark");
}

TEST_F(RenderTest, NatalLayoutProducesGeometry) {
  auto scene = asteria::render::buildNatalChartScene(chart, asteria::render::textbookLight());
  EXPECT_EQ(scene.width, 1000);
  EXPECT_EQ(scene.height, 1000);
  EXPECT_GE(scene.circles.size(), 3u);  // outer, inner, house rings
  EXPECT_GE(scene.lines.size(), 12u);   // at least sign dividers
  EXPECT_GE(scene.texts.size(), 12u);   // at least sign labels
}

TEST_F(RenderTest, NatalLayoutWithUncertaintyWarning) {
  chart.uncertaintyFlags.push_back("noon_default_applied");
  auto scene = asteria::render::buildNatalChartScene(chart, asteria::render::textbookLight());
  bool hasWarning = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Warning") != std::string::npos ||
        text.content.find("uncertain") != std::string::npos) {
      hasWarning = true;
      break;
    }
  }
  EXPECT_TRUE(hasWarning);
}

TEST_F(RenderTest, ColorToHex) {
  asteria::render::Color c{0xFF, 0x00, 0xAB};
  EXPECT_EQ(c.toHex(), "#ff00ab");
}

TEST_F(RenderTest, SvgSerializationContainsExpectedElements) {
  auto scene = asteria::render::buildNatalChartScene(chart, asteria::render::textbookLight());
  std::string svg = asteria::render::serializeSvg(scene);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find("</svg>"), std::string::npos);
  EXPECT_NE(svg.find("<circle"), std::string::npos);
  EXPECT_NE(svg.find("<line"), std::string::npos);
  EXPECT_NE(svg.find("<text"), std::string::npos);
  EXPECT_NE(svg.find("Asteria"), std::string::npos);
}

TEST_F(RenderTest, SvgWriteToFile) {
  auto scene = asteria::render::buildNatalChartScene(chart, asteria::render::textbookLight());
  namespace fs = std::filesystem;
  fs::path goldenDir = fs::temp_directory_path() / "asteria_test";
  fs::create_directories(goldenDir);
  fs::path svgPath = goldenDir / "golden_natal.svg";
  ASSERT_TRUE(asteria::render::writeSvgFile(scene, svgPath.string()));
  EXPECT_TRUE(fs::exists(svgPath));
  EXPECT_GT(fs::file_size(svgPath), 0u);
  fs::remove_all(goldenDir);
}

TEST_F(RenderTest, SvgDeterminism) {
  auto theme = asteria::render::textbookLight();
  auto scene1 = asteria::render::buildNatalChartScene(chart, theme);
  auto scene2 = asteria::render::buildNatalChartScene(chart, theme);
  std::string svg1 = asteria::render::serializeSvg(scene1);
  std::string svg2 = asteria::render::serializeSvg(scene2);
  EXPECT_EQ(svg1, svg2) << "SVG output must be deterministic for the same input";
}
