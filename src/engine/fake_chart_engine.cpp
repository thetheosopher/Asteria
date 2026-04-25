#include "fake_chart_engine.h"

namespace asteria::engine {
using asteria::core::Error;
using asteria::core::Result;

core::Result<std::vector<domain::LocationResolution>> FakeChartEngine::resolveLocation(
    const std::string& query,
    const std::string&) const {
  domain::LocationResolution loc;
  loc.locationId = 1;
  loc.queryText = query;
  loc.displayName = "Denver, CO, USA";
  loc.country = "USA";
  loc.region = "CO";
  loc.latitude = 39.7392;
  loc.longitude = -104.9903;
  loc.timezoneName = "America/Denver";
  loc.resolutionMethod = "atlas";
  loc.confidenceScore = 0.99;
  return Result<std::vector<domain::LocationResolution>>::success({loc});
}

static domain::ComputedChart buildSample(const domain::ChartRequest& request, const std::string& engineMethod) {
  domain::ComputedChart chart;
  chart.chartRequestId = request.chartRequestId;
  chart.engineVersion = "fake-0.1";
  chart.engineMethod = engineMethod;
  chart.canonicalHash = "fake-sample-hash";
  chart.houseSystem = request.defaultHouseSystem;
  chart.zodiacMode = request.zodiacMode;
  chart.chartMetadataJson = R"({"title":"Sample Natal Chart","subject":"Demo"})";
  chart.planets = {
    {"Sun", 15.0, 0.0, 0.98, "Aries", 1, false},
    {"Moon", 102.0, 5.0, 13.0, "Cancer", 4, false},
    {"Mercury", 28.0, 1.0, 1.2, "Aries", 1, false},
    {"Venus", 41.0, 0.5, 1.1, "Taurus", 2, false},
    {"Mars", 223.0, -1.2, 0.6, "Scorpio", 8, true}
  };
  for (int i = 1; i <= 12; ++i) {
    chart.houseCusps.push_back({i, (i - 1) * 30.0});
  }
  chart.aspects = {
    {"Sun", "Moon", "Square", 3.0, std::string("applying")},
    {"Sun", "Mars", "Quincunx", 2.0, std::nullopt},
    {"Moon", "Venus", "Sextile", 1.0, std::string("separating")}
  };
  return chart;
}

core::Result<domain::ComputedChart> FakeChartEngine::computeNatalChart(const domain::ChartRequest& request) const {
  return Result<domain::ComputedChart>::success(buildSample(request, "FakeChartEngine::Natal"));
}

core::Result<domain::ComputedChart> FakeChartEngine::computeSynastryChart(const domain::ChartRequest& request) const {
  auto chart = buildSample(request, "FakeChartEngine::Synastry");
  chart.chartMetadataJson = R"({"title":"Sample Synastry Chart"})";
  return Result<domain::ComputedChart>::success(chart);
}

core::Result<domain::ComputedChart> FakeChartEngine::computeCompositeChart(const domain::ChartRequest& request) const {
  auto chart = buildSample(request, "FakeChartEngine::Composite");
  chart.chartMetadataJson = R"({"title":"Sample Composite Chart"})";
  return Result<domain::ComputedChart>::success(chart);
}

core::Result<domain::ComputedChart> FakeChartEngine::computeTransitToNatalChart(const domain::ChartRequest& request) const {
  auto chart = buildSample(request, "FakeChartEngine::TransitToNatal");
  chart.chartMetadataJson = R"({"title":"Sample Transit-to-Natal Chart"})";
  return Result<domain::ComputedChart>::success(chart);
}

std::string FakeChartEngine::getEngineVersion() const { return "fake-0.1"; }

}  // namespace asteria::engine
