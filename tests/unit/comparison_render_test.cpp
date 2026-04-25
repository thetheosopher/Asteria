#include <gtest/gtest.h>
#include "render/comparison_chart_layout.h"
#include "render/transit_chart_layout.h"
#include "render/svg_serializer.h"
#include "render/theme_presets.h"
#include "engine/fake_chart_engine.h"

class ComparisonRenderTest : public ::testing::Test {
 protected:
  asteria::domain::ComputedChart chartA;
  asteria::domain::ComputedChart chartB;

  void SetUp() override {
    asteria::engine::FakeChartEngine engine;

    asteria::domain::ChartRequest requestA;
    requestA.chartRequestId = 1;
    auto resultA = engine.computeNatalChart(requestA);
    ASSERT_TRUE(resultA.ok());
    chartA = resultA.value();

    asteria::domain::ChartRequest requestB;
    requestB.chartRequestId = 2;
    auto resultB = engine.computeNatalChart(requestB);
    ASSERT_TRUE(resultB.ok());
    chartB = resultB.value();
  }
};

TEST_F(ComparisonRenderTest, SynastryLayoutProducesGeometry) {
  auto theme = asteria::render::textbookLight();
  auto scene = asteria::render::buildSynastryChartScene(chartA, chartB, theme);
  EXPECT_EQ(scene.width, 1000);
  EXPECT_EQ(scene.height, 1000);
  EXPECT_GE(scene.circles.size(), 4u);  // outer, mid, inner, house
  EXPECT_GE(scene.lines.size(), 12u);   // sign dividers
  EXPECT_GE(scene.texts.size(), 12u);   // sign labels + planets from both charts
}

TEST_F(ComparisonRenderTest, SynastryLayoutHasBothChartPlanets) {
  auto theme = asteria::render::textbookLight();
  auto scene = asteria::render::buildSynastryChartScene(chartA, chartB, theme);
  // Should have planets from both A and B (5 each) + 12 signs + title + subtitle = ~24+ texts
  int sunCount = 0;
  for (const auto& text : scene.texts) {
    if (text.content == "Sun") sunCount++;
  }
  EXPECT_EQ(sunCount, 2);  // One from each chart
}

TEST_F(ComparisonRenderTest, SynastryLayoutHasTitleText) {
  auto theme = asteria::render::textbookLight();
  auto scene = asteria::render::buildSynastryChartScene(chartA, chartB, theme);
  bool hasTitle = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Synastry") != std::string::npos) {
      hasTitle = true;
      break;
    }
  }
  EXPECT_TRUE(hasTitle);
}

TEST_F(ComparisonRenderTest, SynastryWithUncertaintyWarning) {
  chartA.uncertaintyFlags.push_back("noon_default_applied");
  auto theme = asteria::render::textbookLight();
  auto scene = asteria::render::buildSynastryChartScene(chartA, chartB, theme);
  bool hasWarning = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Warning") != std::string::npos) {
      hasWarning = true;
      break;
    }
  }
  EXPECT_TRUE(hasWarning);
}

TEST_F(ComparisonRenderTest, CompositeLayoutProducesGeometry) {
  auto theme = asteria::render::luxuryDark();
  auto scene = asteria::render::buildCompositeChartScene(chartA, theme);
  EXPECT_EQ(scene.width, 1000);
  EXPECT_GE(scene.circles.size(), 3u);
  EXPECT_GE(scene.lines.size(), 12u);
}

TEST_F(ComparisonRenderTest, CompositeLayoutHasTitleText) {
  auto theme = asteria::render::textbookLight();
  auto scene = asteria::render::buildCompositeChartScene(chartA, theme);
  bool hasTitle = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Composite") != std::string::npos) {
      hasTitle = true;
      break;
    }
  }
  EXPECT_TRUE(hasTitle);
}

TEST_F(ComparisonRenderTest, SynastryIsDeterministic) {
  auto theme = asteria::render::textbookLight();
  auto scene1 = asteria::render::buildSynastryChartScene(chartA, chartB, theme);
  auto scene2 = asteria::render::buildSynastryChartScene(chartA, chartB, theme);
  auto svg1 = asteria::render::serializeSvg(scene1);
  auto svg2 = asteria::render::serializeSvg(scene2);
  EXPECT_EQ(svg1, svg2);
}

// --- Transit chart tests ---

TEST_F(ComparisonRenderTest, TransitLayoutProducesGeometry) {
  auto theme = asteria::render::textbookLight();
  auto scene = asteria::render::buildTransitToNatalChartScene(chartA, chartB, theme);
  EXPECT_EQ(scene.width, 1000);
  EXPECT_GE(scene.circles.size(), 4u);  // outerR, transitR, innerR, houseR
  EXPECT_GE(scene.lines.size(), 12u);
}

TEST_F(ComparisonRenderTest, TransitLayoutHasTransitPrefixedPlanets) {
  auto theme = asteria::render::textbookLight();
  auto scene = asteria::render::buildTransitToNatalChartScene(chartA, chartB, theme);
  bool hasTransitPrefix = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("t.") != std::string::npos) {
      hasTransitPrefix = true;
      break;
    }
  }
  EXPECT_TRUE(hasTransitPrefix);
}

TEST_F(ComparisonRenderTest, TransitLayoutHasTitleText) {
  auto theme = asteria::render::textbookLight();
  auto scene = asteria::render::buildTransitToNatalChartScene(chartA, chartB, theme);
  bool hasTitle = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Transit-to-Natal") != std::string::npos) {
      hasTitle = true;
      break;
    }
  }
  EXPECT_TRUE(hasTitle);
}

TEST_F(ComparisonRenderTest, TransitIsDeterministic) {
  auto theme = asteria::render::luxuryLight();
  auto scene1 = asteria::render::buildTransitToNatalChartScene(chartA, chartB, theme);
  auto scene2 = asteria::render::buildTransitToNatalChartScene(chartA, chartB, theme);
  auto svg1 = asteria::render::serializeSvg(scene1);
  auto svg2 = asteria::render::serializeSvg(scene2);
  EXPECT_EQ(svg1, svg2);
}

TEST_F(ComparisonRenderTest, TransitWithUncertaintyWarning) {
  chartA.uncertaintyFlags.push_back("approximate_time");
  auto theme = asteria::render::textbookLight();
  auto scene = asteria::render::buildTransitToNatalChartScene(chartA, chartB, theme);
  bool hasWarning = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Warning") != std::string::npos) {
      hasWarning = true;
      break;
    }
  }
  EXPECT_TRUE(hasWarning);
}
