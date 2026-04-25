#include <gtest/gtest.h>
#include "domain/person.h"
#include "domain/birth_event.h"
#include "domain/chart_request.h"
#include "domain/computed_chart.h"
#include "domain/types.h"
#include "engine/fake_chart_engine.h"
#include "core/natal_chart_service.h"
#include "core/interpretation_service.h"
#include "render/natal_chart_layout.h"
#include "render/comparison_chart_layout.h"
#include "render/transit_chart_layout.h"
#include "render/chart_scene.h"
#include "render/theme_presets.h"

// =============================================================================
// Domain Layer: Uncertain-Time Field Validation
// =============================================================================

TEST(UncertainTimeTest, BirthEventExactTimeDefaults) {
  asteria::domain::BirthEvent event;
  event.timeAccuracy = asteria::domain::TimeAccuracy::Exact;
  event.birthTime = "14:30";
  event.noonDefaultApplied = false;
  event.housesEnabled = true;

  EXPECT_EQ(event.timeAccuracy, asteria::domain::TimeAccuracy::Exact);
  EXPECT_TRUE(event.birthTime.has_value());
  EXPECT_FALSE(event.noonDefaultApplied);
  EXPECT_TRUE(event.housesEnabled);
}

TEST(UncertainTimeTest, BirthEventApproximateTime) {
  asteria::domain::BirthEvent event;
  event.timeAccuracy = asteria::domain::TimeAccuracy::Approximate;
  event.birthTime = "14:00";
  event.noonDefaultApplied = false;
  event.housesEnabled = true;

  EXPECT_EQ(event.timeAccuracy, asteria::domain::TimeAccuracy::Approximate);
  EXPECT_TRUE(event.birthTime.has_value());
  EXPECT_TRUE(event.housesEnabled);
}

TEST(UncertainTimeTest, BirthEventUnknownTimeWithNoonDefault) {
  asteria::domain::BirthEvent event;
  event.timeAccuracy = asteria::domain::TimeAccuracy::Unknown;
  event.noonDefaultApplied = true;
  event.housesEnabled = true;

  EXPECT_EQ(event.timeAccuracy, asteria::domain::TimeAccuracy::Unknown);
  EXPECT_FALSE(event.birthTime.has_value());
  EXPECT_TRUE(event.noonDefaultApplied);
  EXPECT_TRUE(event.housesEnabled);
}

TEST(UncertainTimeTest, BirthEventUnknownTimeNoHouses) {
  asteria::domain::BirthEvent event;
  event.timeAccuracy = asteria::domain::TimeAccuracy::Unknown;
  event.noonDefaultApplied = false;
  event.housesEnabled = false;

  EXPECT_EQ(event.timeAccuracy, asteria::domain::TimeAccuracy::Unknown);
  EXPECT_FALSE(event.housesEnabled);
  EXPECT_FALSE(event.noonDefaultApplied);
}

TEST(UncertainTimeTest, ChartRequestNoonDefaultPolicy) {
  asteria::domain::ChartRequest request;
  request.unknownTimePolicy = "noon_default_with_warning";
  request.includeHouses = true;

  EXPECT_EQ(request.unknownTimePolicy, "noon_default_with_warning");
  EXPECT_TRUE(request.includeHouses);
}

TEST(UncertainTimeTest, ChartRequestNoHousesPolicy) {
  asteria::domain::ChartRequest request;
  request.unknownTimePolicy = "unknown_time_no_houses";
  request.includeHouses = false;

  EXPECT_EQ(request.unknownTimePolicy, "unknown_time_no_houses");
  EXPECT_FALSE(request.includeHouses);
}

TEST(UncertainTimeTest, ComputedChartUncertaintyFlagsAccumulate) {
  asteria::domain::ComputedChart chart;
  chart.uncertaintyFlags.push_back("noon_default_applied");
  chart.uncertaintyFlags.push_back("no_houses");

  EXPECT_EQ(chart.uncertaintyFlags.size(), 2u);
  EXPECT_EQ(chart.uncertaintyFlags[0], "noon_default_applied");
  EXPECT_EQ(chart.uncertaintyFlags[1], "no_houses");
}

// =============================================================================
// Engine Layer: Uncertain-Time Policy Handling
// =============================================================================

TEST(UncertainTimeTest, FakeEngineRespectsNoHousesPolicy) {
  asteria::engine::FakeChartEngine engine;
  asteria::domain::ChartRequest request;
  request.chartRequestId = 10;
  request.chartType = asteria::domain::ChartType::Natal;
  request.includeHouses = false;
  request.unknownTimePolicy = "unknown_time_no_houses";

  auto result = engine.computeNatalChart(request);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().chartRequestId, 10);
}

TEST(UncertainTimeTest, FakeEngineRespectsNoonDefaultPolicy) {
  asteria::engine::FakeChartEngine engine;
  asteria::domain::ChartRequest request;
  request.chartRequestId = 20;
  request.chartType = asteria::domain::ChartType::Natal;
  request.unknownTimePolicy = "noon_default_with_warning";
  request.includeHouses = true;

  auto result = engine.computeNatalChart(request);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().chartRequestId, 20);
}

TEST(UncertainTimeTest, ServicePropagatesUncertaintyToEngine) {
  asteria::engine::FakeChartEngine engine;
  asteria::core::NatalChartService service(engine);
  asteria::domain::ChartRequest request;
  request.chartRequestId = 30;
  request.chartType = asteria::domain::ChartType::Natal;
  request.unknownTimePolicy = "noon_default_with_warning";
  request.includeHouses = true;

  auto result = service.compute(request);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().chartRequestId, 30);
}

// =============================================================================
// Rendering Layer: Warning Banner and No-Houses Behavior
// =============================================================================

static asteria::domain::ComputedChart makeChartWithFlags(
    const std::vector<std::string>& flags, bool withHouses = true) {
  asteria::domain::ComputedChart chart;
  chart.chartRequestId = 1;
  chart.uncertaintyFlags = flags;

  // Add some planet positions for a valid chart
  asteria::domain::PlanetPosition sun;
  sun.objectName = "Sun";
  sun.longitude = 280.0;
  sun.latitude = 0.0;
  sun.speed = 1.0;
  sun.signName = "Capricorn";
  sun.signIndex = 10;
  sun.houseNumber = 1;
  sun.isRetrograde = false;
  chart.planets.push_back(sun);

  asteria::domain::PlanetPosition moon;
  moon.objectName = "Moon";
  moon.longitude = 120.0;
  moon.latitude = 2.0;
  moon.speed = 13.0;
  moon.signName = "Leo";
  moon.signIndex = 5;
  moon.houseNumber = 7;
  moon.isRetrograde = false;
  chart.planets.push_back(moon);

  if (withHouses) {
    for (int i = 1; i <= 12; ++i) {
      asteria::domain::HouseCusp cusp;
      cusp.houseNumber = i;
      cusp.longitude = (i - 1) * 30.0;
      chart.houseCusps.push_back(cusp);
    }
  }

  return chart;
}

TEST(UncertainTimeTest, RenderNatalNoHousesWarning) {
  auto chart = makeChartWithFlags({"no_houses"}, false);
  auto scene = asteria::render::buildNatalChartScene(chart, asteria::render::textbookLight());
  bool hasWarning = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Warning") != std::string::npos ||
        text.content.find("uncertain") != std::string::npos) {
      hasWarning = true;
      break;
    }
  }
  EXPECT_TRUE(hasWarning) << "No-houses chart should render a warning banner";
}

TEST(UncertainTimeTest, RenderNatalNoonDefaultWarning) {
  auto chart = makeChartWithFlags({"noon_default_applied"});
  auto scene = asteria::render::buildNatalChartScene(chart, asteria::render::textbookLight());
  bool hasWarning = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Warning") != std::string::npos ||
        text.content.find("uncertain") != std::string::npos) {
      hasWarning = true;
      break;
    }
  }
  EXPECT_TRUE(hasWarning) << "Noon-default chart should render a warning banner";
}

TEST(UncertainTimeTest, RenderNatalMultipleFlags) {
  auto chart = makeChartWithFlags({"noon_default_applied", "no_houses"}, false);
  auto scene = asteria::render::buildNatalChartScene(chart, asteria::render::textbookLight());
  bool hasWarning = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Warning") != std::string::npos ||
        text.content.find("uncertain") != std::string::npos) {
      hasWarning = true;
      break;
    }
  }
  EXPECT_TRUE(hasWarning) << "Multiple uncertainty flags should still render a warning";
}

TEST(UncertainTimeTest, RenderNatalNoFlagsNoWarning) {
  auto chart = makeChartWithFlags({});
  auto scene = asteria::render::buildNatalChartScene(chart, asteria::render::textbookLight());
  bool hasWarning = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Warning") != std::string::npos ||
        text.content.find("uncertain") != std::string::npos) {
      hasWarning = true;
      break;
    }
  }
  EXPECT_FALSE(hasWarning) << "Chart with no uncertainty should not show a warning";
}

TEST(UncertainTimeTest, RenderComparisonWarningBanner) {
  auto chart1 = makeChartWithFlags({"noon_default_applied"});
  auto chart2 = makeChartWithFlags({});
  auto scene = asteria::render::buildSynastryChartScene(
      chart1, chart2, asteria::render::textbookLight());
  bool hasWarning = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Warning") != std::string::npos ||
        text.content.find("uncertain") != std::string::npos) {
      hasWarning = true;
      break;
    }
  }
  EXPECT_TRUE(hasWarning) << "Synastry should show warning when one partner has uncertain time";
}

TEST(UncertainTimeTest, RenderTransitWarningBanner) {
  auto natal = makeChartWithFlags({"no_houses"}, false);
  auto transit = makeChartWithFlags({});
  auto scene = asteria::render::buildTransitToNatalChartScene(
      natal, transit, asteria::render::textbookLight());
  bool hasWarning = false;
  for (const auto& text : scene.texts) {
    if (text.content.find("Warning") != std::string::npos ||
        text.content.find("uncertain") != std::string::npos) {
      hasWarning = true;
      break;
    }
  }
  EXPECT_TRUE(hasWarning) << "Transit should show warning when natal chart has uncertain time";
}

// =============================================================================
// Interpretation Layer: Guardrails for Uncertain Time
// =============================================================================

TEST(UncertainTimeTest, InterpretationNoonDefaultGuardrail) {
  auto chart = makeChartWithFlags({"noon_default_applied"});
  asteria::core::InterpretationService svc;
  auto result = svc.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());

  auto& interp = result.value();
  EXPECT_TRUE(interp.certaintyGuardrailsApplied);
  EXPECT_NE(interp.bodyMarkdown.find("noon"), std::string::npos)
      << "Noon default interpretation should mention noon";
}

TEST(UncertainTimeTest, InterpretationNoHousesGuardrail) {
  auto chart = makeChartWithFlags({"no_houses"}, false);
  asteria::core::InterpretationService svc;
  auto result = svc.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());

  auto& interp = result.value();
  EXPECT_TRUE(interp.certaintyGuardrailsApplied);
  EXPECT_NE(interp.bodyMarkdown.find("suppressed"), std::string::npos)
      << "No-houses interpretation should mention houses suppressed";
}

TEST(UncertainTimeTest, InterpretationApproximateTimeGuardrail) {
  auto chart = makeChartWithFlags({"approximate_time"});
  asteria::core::InterpretationService svc;
  auto result = svc.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());

  auto& interp = result.value();
  EXPECT_TRUE(interp.certaintyGuardrailsApplied);
  EXPECT_NE(interp.bodyMarkdown.find("approximate"), std::string::npos)
      << "Approximate time interpretation should mention approximate time";
}

TEST(UncertainTimeTest, InterpretationMultipleFlagsGuardrail) {
  auto chart = makeChartWithFlags({"noon_default_applied", "no_houses"}, false);
  asteria::core::InterpretationService svc;
  auto result = svc.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());

  auto& interp = result.value();
  EXPECT_TRUE(interp.certaintyGuardrailsApplied);
  EXPECT_NE(interp.bodyMarkdown.find("noon"), std::string::npos);
  EXPECT_NE(interp.bodyMarkdown.find("suppressed"), std::string::npos)
      << "Multiple flags should produce multiple disclaimers";
}

TEST(UncertainTimeTest, InterpretationNoFlagsNoGuardrails) {
  auto chart = makeChartWithFlags({});
  asteria::core::InterpretationService svc;
  auto result = svc.generateBuiltIn(chart, asteria::domain::ChartType::Natal);
  ASSERT_TRUE(result.ok());

  auto& interp = result.value();
  EXPECT_FALSE(interp.certaintyGuardrailsApplied);
  EXPECT_EQ(interp.bodyMarkdown.find("Uncertainty"), std::string::npos)
      << "Chart without flags should not produce uncertainty notices";
}

TEST(UncertainTimeTest, InterpretationSynastryWithUncertainty) {
  auto chart = makeChartWithFlags({"noon_default_applied"});
  asteria::core::InterpretationService svc;
  auto result = svc.generateBuiltIn(chart, asteria::domain::ChartType::Synastry);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().certaintyGuardrailsApplied);
}

TEST(UncertainTimeTest, InterpretationCompositeWithUncertainty) {
  auto chart = makeChartWithFlags({"no_houses"}, false);
  asteria::core::InterpretationService svc;
  auto result = svc.generateBuiltIn(chart, asteria::domain::ChartType::Composite);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().certaintyGuardrailsApplied);
}

TEST(UncertainTimeTest, InterpretationTransitWithUncertainty) {
  auto chart = makeChartWithFlags({"approximate_time"});
  asteria::core::InterpretationService svc;
  auto result = svc.generateBuiltIn(chart, asteria::domain::ChartType::Transit);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().certaintyGuardrailsApplied);
}
