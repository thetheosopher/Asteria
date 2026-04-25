#include <gtest/gtest.h>
#include "core/interpretation_service.h"
#include "engine/fake_chart_engine.h"

class InterpretationTest : public ::testing::Test {
 protected:
  asteria::domain::ComputedChart chart;
  asteria::core::InterpretationService service;

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

TEST_F(InterpretationTest, NatalInterpretationProducesMarkdown) {
  auto result = service.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());
  const auto& note = result.value();
  EXPECT_EQ(note.sourceType, asteria::domain::InterpretationSourceType::BuiltIn);
  EXPECT_EQ(note.promptVersion, "built-in-v1");
  EXPECT_FALSE(note.bodyMarkdown.empty());
  EXPECT_NE(note.bodyMarkdown.find("Natal Chart Summary"), std::string::npos);
  EXPECT_NE(note.bodyMarkdown.find("Planetary Positions"), std::string::npos);
  EXPECT_NE(note.bodyMarkdown.find("Sun"), std::string::npos);
}

TEST_F(InterpretationTest, NatalInterpretationIncludesAspects) {
  auto result = service.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.value().bodyMarkdown.find("Key Aspects"), std::string::npos);
  EXPECT_NE(result.value().bodyMarkdown.find("Square"), std::string::npos);
}

TEST_F(InterpretationTest, SynastryInterpretation) {
  auto result = service.generateBuiltIn(chart, asteria::domain::ChartType::Synastry);
  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.value().bodyMarkdown.find("Synastry"), std::string::npos);
}

TEST_F(InterpretationTest, CompositeInterpretation) {
  auto result = service.generateBuiltIn(chart, asteria::domain::ChartType::Composite);
  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.value().bodyMarkdown.find("Composite"), std::string::npos);
}

TEST_F(InterpretationTest, TransitInterpretation) {
  auto result = service.generateBuiltIn(chart, asteria::domain::ChartType::TransitToNatal);
  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.value().bodyMarkdown.find("Transit-to-Natal"), std::string::npos);
}

TEST_F(InterpretationTest, UncertaintyGuardrailsApplied) {
  chart.uncertaintyFlags.push_back("noon_default_applied");
  auto result = service.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());
  const auto& note = result.value();
  EXPECT_TRUE(note.certaintyGuardrailsApplied);
  EXPECT_NE(note.bodyMarkdown.find("Uncertainty Notice"), std::string::npos);
  EXPECT_NE(note.bodyMarkdown.find("noon"), std::string::npos);
}

TEST_F(InterpretationTest, NoGuardrailsWhenCertain) {
  chart.uncertaintyFlags.clear();
  auto result = service.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result.value().certaintyGuardrailsApplied);
}

TEST_F(InterpretationTest, ApproximateTimeWarning) {
  chart.uncertaintyFlags.push_back("approximate_time");
  auto result = service.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.value().bodyMarkdown.find("approximate"), std::string::npos);
}

TEST_F(InterpretationTest, NoHousesWarning) {
  chart.uncertaintyFlags.push_back("no_houses");
  auto result = service.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.value().bodyMarkdown.find("Houses are suppressed"), std::string::npos);
}

TEST_F(InterpretationTest, RetrogradeNoted) {
  auto result = service.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());
  // Mars is retrograde in the fake engine data
  EXPECT_NE(result.value().bodyMarkdown.find("Rx"), std::string::npos);
}

TEST_F(InterpretationTest, InterpretationIsDeterministic) {
  auto r1 = service.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  auto r2 = service.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(r1.ok());
  ASSERT_TRUE(r2.ok());
  EXPECT_EQ(r1.value().bodyMarkdown, r2.value().bodyMarkdown);
}
