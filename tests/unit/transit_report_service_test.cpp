#include <gtest/gtest.h>

#include "core/transit_report_service.h"
#include "engine/ichart_engine.h"

#include <chrono>

namespace {

class TransitReportTestEngine final : public asteria::engine::IChartEngine {
 public:
  asteria::core::Result<std::vector<asteria::domain::LocationResolution>> resolveLocation(
      const std::string&,
      const std::string&) const override {
    return asteria::core::Result<std::vector<asteria::domain::LocationResolution>>::success({});
  }

  asteria::core::Result<asteria::domain::ComputedChart> computeNatalChart(
      const asteria::domain::ChartRequest& request) const override {
    if (request.chartType == asteria::domain::ChartType::TransitToNatal) {
      return asteria::core::Result<asteria::domain::ComputedChart>::success(buildTransitChart(*request.primaryInput));
    }
    return asteria::core::Result<asteria::domain::ComputedChart>::success(buildNatalChart(request));
  }

  asteria::core::Result<asteria::domain::ComputedChart> computeSynastryChart(
      const asteria::domain::ChartRequest&) const override {
    return asteria::core::Result<asteria::domain::ComputedChart>::failure(
        {"not_implemented", "unused in test"});
  }

  asteria::core::Result<asteria::domain::ComputedChart> computeCompositeChart(
      const asteria::domain::ChartRequest&) const override {
    return asteria::core::Result<asteria::domain::ComputedChart>::failure(
        {"not_implemented", "unused in test"});
  }

  asteria::core::Result<asteria::domain::ComputedChart> computeTransitToNatalChart(
      const asteria::domain::ChartRequest&) const override {
    return asteria::core::Result<asteria::domain::ComputedChart>::failure(
        {"not_implemented", "unused in test"});
  }

  std::string getEngineVersion() const override { return "transit-report-test"; }

 private:
  static asteria::domain::ComputedChart buildNatalChart(const asteria::domain::ChartRequest& request) {
    asteria::domain::ComputedChart chart;
    chart.engineMethod = "TransitReportTest::Natal";
    chart.houseSystem = request.defaultHouseSystem;
    chart.zodiacMode = request.zodiacMode;
    chart.planets = {
        {"Sun", 100.0, 0.0, 0.0, "Cancer", 4, false},
        {"Moon", 212.0, 0.0, 0.0, "Scorpio", 8, false},
    };
    chart.uncertaintyFlags = {"approximate_time"};
    return chart;
  }

  static asteria::domain::ComputedChart buildTransitChart(
      const asteria::domain::ResolvedBirthInput& input) {
    using namespace std::chrono;

    const auto currentDay = sys_days{year{input.year} / month{static_cast<unsigned>(input.month)}
                                     / day{static_cast<unsigned>(input.day)}};
    const auto startDay = sys_days{year{2026} / month{1} / day{1}};
    const double dayOffset = duration_cast<hours>(currentDay - startDay).count() / 24.0
        + input.timeHours / 24.0;

    asteria::domain::ComputedChart chart;
    chart.engineMethod = "TransitReportTest::Transit";
    chart.planets = {
        {"Jupiter", 38.0 + dayOffset, 0.0, 1.0, "Taurus", std::nullopt, false},
        {"Mars", 100.4, 0.0, 0.6, "Cancer", std::nullopt, false},
    };
    return chart;
  }
};

asteria::core::TransitReportService::Request makeRequest() {
  asteria::core::TransitReportService::Request request;
  request.subjectName = "Casey Example";
  request.startDateTime = "2026-01-01T00:00:00";
  request.rangeYears = 0.1;
  request.chartRequest.chartType = asteria::domain::ChartType::Natal;
  request.chartRequest.defaultHouseSystem = "Placidus";
  request.chartRequest.zodiacMode = "Tropical";
  request.chartRequest.primaryInput = asteria::domain::ResolvedBirthInput{
      1990, 1, 1, 12.0, -5.0, 0.0, 40.0, -75.0};
  return request;
}

}  // namespace

TEST(TransitReportServiceTest, GeneratesMarkdownForSlowMajorTransits) {
  TransitReportTestEngine engine;
  asteria::core::TransitReportService service(engine);

  auto result = service.generate(makeRequest());
  ASSERT_TRUE(result.ok());

  ASSERT_EQ(result.value().events.size(), 1u);
  EXPECT_EQ(result.value().events.front().timestamp, "2026-01-03 00:00");
  EXPECT_EQ(result.value().events.front().transitObject, "Jupiter");
  EXPECT_EQ(result.value().events.front().aspectType, "Sextile");
  EXPECT_EQ(result.value().events.front().natalObject, "Sun");

  EXPECT_NE(result.value().bodyMarkdown.find("Casey Example"), std::string::npos);
  EXPECT_NE(result.value().bodyMarkdown.find("Jupiter Sextile natal Sun"), std::string::npos);
  EXPECT_EQ(result.value().bodyMarkdown.find("Mars Conjunction natal Sun"), std::string::npos);
  EXPECT_NE(result.value().bodyMarkdown.find("approximate_time"), std::string::npos);
}

TEST(TransitReportServiceTest, HonorsConfiguredPlanetAndAspectSets) {
  TransitReportTestEngine engine;
  asteria::core::TransitReportService service(engine);

  auto request = makeRequest();
  request.rules.transitObjects = {"Mars"};
  request.rules.aspectRules = {{"Conjunction", 0.0, 0.5}};

  auto result = service.generate(request);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.value().events.size(), 1u);
  EXPECT_EQ(result.value().events.front().transitObject, "Mars");
  EXPECT_EQ(result.value().events.front().aspectType, "Conjunction");
  EXPECT_EQ(result.value().events.front().natalObject, "Sun");
  EXPECT_NE(result.value().bodyMarkdown.find("Mars"), std::string::npos);
  EXPECT_NE(result.value().bodyMarkdown.find("Conjunction (0.50 deg orb)"), std::string::npos);
}

TEST(TransitReportServiceTest, HonorsConfiguredOrbRules) {
  TransitReportTestEngine engine;
  asteria::core::TransitReportService service(engine);

  auto request = makeRequest();
  request.rules.transitObjects = {"Mars"};
  request.rules.aspectRules = {{"Conjunction", 0.0, 0.1}};

  auto result = service.generate(request);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().events.empty());
  EXPECT_NE(result.value().bodyMarkdown.find("No matching transits were found"), std::string::npos);
}

TEST(TransitReportServiceTest, RejectsMalformedStartDate) {
  TransitReportTestEngine engine;
  asteria::core::TransitReportService service(engine);

  auto request = makeRequest();
  request.startDateTime = "not-a-date";

  auto result = service.generate(request);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error().code, "invalid_start");
}