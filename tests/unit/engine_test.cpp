#include <gtest/gtest.h>
#include "engine/ichart_engine.h"
#include "engine/fake_chart_engine.h"
#include "engine/astrolog_embedded_engine.h"
#include "engine/astrolog_cli_bridge.h"
#include "core/natal_chart_service.h"

TEST(EngineTest, FakeEngineVersion) {
  asteria::engine::FakeChartEngine engine;
  EXPECT_EQ(engine.getEngineVersion(), "fake-0.1");
}

TEST(EngineTest, FakeNatalChart) {
  asteria::engine::FakeChartEngine engine;
  asteria::domain::ChartRequest request;
  request.chartRequestId = 1;
  request.chartType = asteria::domain::ChartType::Natal;
  auto result = engine.computeNatalChart(request);
  ASSERT_TRUE(result.ok());
  const auto& chart = result.value();
  EXPECT_EQ(chart.engineMethod, "FakeChartEngine::Natal");
  EXPECT_EQ(chart.houseSystem, "Placidus");
  EXPECT_EQ(chart.zodiacMode, "Tropical");
  EXPECT_EQ(chart.planets.size(), 5u);
  EXPECT_EQ(chart.houseCusps.size(), 12u);
  EXPECT_GE(chart.aspects.size(), 1u);
}

TEST(EngineTest, FakeSynastryChart) {
  asteria::engine::FakeChartEngine engine;
  asteria::domain::ChartRequest request;
  request.chartType = asteria::domain::ChartType::Synastry;
  auto result = engine.computeSynastryChart(request);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().engineMethod, "FakeChartEngine::Synastry");
}

TEST(EngineTest, FakeCompositeChart) {
  asteria::engine::FakeChartEngine engine;
  asteria::domain::ChartRequest request;
  request.chartType = asteria::domain::ChartType::Composite;
  auto result = engine.computeCompositeChart(request);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().engineMethod, "FakeChartEngine::Composite");
}

TEST(EngineTest, FakeTransitToNatalChart) {
  asteria::engine::FakeChartEngine engine;
  asteria::domain::ChartRequest request;
  request.chartType = asteria::domain::ChartType::TransitToNatal;
  auto result = engine.computeTransitToNatalChart(request);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().engineMethod, "FakeChartEngine::TransitToNatal");
}

TEST(EngineTest, FakeLocationResolution) {
  asteria::engine::FakeChartEngine engine;
  auto result = engine.resolveLocation("Denver, CO");
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(result.value()[0].displayName, "Denver, CO, USA");
  EXPECT_NEAR(result.value()[0].latitude, 39.7392, 0.01);
}

TEST(EngineTest, EmbeddedEngineWithoutInit) {
  // Engine with nonexistent data path should fail gracefully
  asteria::engine::AstrologEmbeddedEngine engine("nonexistent_path");
  asteria::domain::ChartRequest request;
  auto result = engine.computeNatalChart(request);
  // May succeed with defaults or fail - either is acceptable
  // The key test is that it doesn't crash
  (void)result;
}

TEST(EngineTest, CliBridgeWithMissingExe) {
  asteria::engine::AstrologCliBridge engine("nonexistent_astrolog.exe");
  asteria::domain::ChartRequest request;
  auto result = engine.computeNatalChart(request);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error().code, "engine_failure");
}

TEST(EngineTest, NatalChartServiceDelegatesToEngine) {
  asteria::engine::FakeChartEngine fakeEngine;
  asteria::core::NatalChartService service(fakeEngine);
  asteria::domain::ChartRequest request;
  request.chartRequestId = 99;
  request.chartType = asteria::domain::ChartType::Natal;
  auto result = service.compute(request);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().chartRequestId, 99);
}
