#include <gtest/gtest.h>
#include "engine/fake_chart_engine.h"
#include "render/natal_chart_layout.h"
#include "render/theme_presets.h"

TEST(SmokeTest, FakeEngineProducesNatalChart) {
  using namespace asteria;
  domain::ChartRequest request;
  request.chartRequestId = 42;
  request.chartType = domain::ChartType::Natal;
  engine::FakeChartEngine engine;
  auto result = engine.computeNatalChart(request);
  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result.value().planets.empty());
  EXPECT_FALSE(result.value().houseCusps.empty());
  EXPECT_FALSE(result.value().aspects.empty());
}

TEST(SmokeTest, NatalChartSceneIsNonEmpty) {
  using namespace asteria;
  domain::ChartRequest request;
  request.chartRequestId = 1;
  request.chartType = domain::ChartType::Natal;
  engine::FakeChartEngine engine;
  auto result = engine.computeNatalChart(request);
  ASSERT_TRUE(result.ok());
  auto scene = render::buildNatalChartScene(result.value(), render::textbookLight());
  EXPECT_FALSE(scene.circles.empty());
  EXPECT_FALSE(scene.texts.empty());
  EXPECT_FALSE(scene.lines.empty());
}
