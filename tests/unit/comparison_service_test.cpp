#include <gtest/gtest.h>
#include "core/comparison_chart_service.h"
#include "engine/fake_chart_engine.h"

class ComparisonServiceTest : public ::testing::Test {
 protected:
  asteria::engine::FakeChartEngine engine;
};

TEST_F(ComparisonServiceTest, SynastryRequiresSecondaryBirthEvent) {
  asteria::core::ComparisonChartService service(engine);
  asteria::domain::ChartRequest request;
  request.chartType = asteria::domain::ChartType::Synastry;
  // No secondaryBirthEventId set
  auto result = service.computeSynastry(request);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error().code, "missing_secondary");
}

TEST_F(ComparisonServiceTest, SynastrySucceedsWithSecondary) {
  asteria::core::ComparisonChartService service(engine);
  asteria::domain::ChartRequest request;
  request.chartType = asteria::domain::ChartType::Synastry;
  request.secondaryBirthEventId = 42;
  auto result = service.computeSynastry(request);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().engineMethod, "FakeChartEngine::Synastry");
}

TEST_F(ComparisonServiceTest, CompositeRequiresSecondaryBirthEvent) {
  asteria::core::ComparisonChartService service(engine);
  asteria::domain::ChartRequest request;
  request.chartType = asteria::domain::ChartType::Composite;
  auto result = service.computeComposite(request);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error().code, "missing_secondary");
}

TEST_F(ComparisonServiceTest, CompositeSucceedsWithSecondary) {
  asteria::core::ComparisonChartService service(engine);
  asteria::domain::ChartRequest request;
  request.chartType = asteria::domain::ChartType::Composite;
  request.secondaryBirthEventId = 99;
  auto result = service.computeComposite(request);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().engineMethod, "FakeChartEngine::Composite");
}

TEST_F(ComparisonServiceTest, TransitRequiresTimeContext) {
  asteria::core::ComparisonChartService service(engine);
  asteria::domain::ChartRequest request;
  request.chartType = asteria::domain::ChartType::TransitToNatal;
  // No requestTimeContext set
  auto result = service.computeTransitToNatal(request);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error().code, "missing_transit_time");
}

TEST_F(ComparisonServiceTest, TransitSucceedsWithTimeContext) {
  asteria::core::ComparisonChartService service(engine);
  asteria::domain::ChartRequest request;
  request.chartType = asteria::domain::ChartType::TransitToNatal;
  request.requestTimeContext = "2026-04-24T12:00:00Z";
  auto result = service.computeTransitToNatal(request);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().engineMethod, "FakeChartEngine::TransitToNatal");
}
