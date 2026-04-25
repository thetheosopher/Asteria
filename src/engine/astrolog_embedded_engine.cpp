#include "astrolog_embedded_engine.h"
#include "astrolog_adapter.h"
#include <cmath>
#include <functional>
#include <sstream>

namespace asteria::engine {

using asteria::core::Error;
using asteria::core::Result;

AstrologEmbeddedEngine::AstrologEmbeddedEngine(const std::string& dataPath) {
  m_initialized = AstrologAdapter::initialize(dataPath);
}

std::string AstrologEmbeddedEngine::getEngineVersion() const {
  return AstrologAdapter::engineVersion();
}

core::Result<std::vector<domain::LocationResolution>> AstrologEmbeddedEngine::resolveLocation(
    const std::string& query, const std::string&) const {
  // TODO: Integrate with Astrolog's atlas lookup (DisplayAtlasLookup).
  // For now, return a not_implemented error since atlas integration
  // requires loading atlas.as and timezone.as which needs further adapter work.
  return Result<std::vector<domain::LocationResolution>>::failure(
      {"not_implemented", "Location resolution via Astrolog atlas is not yet integrated. "
       "Use coordinates directly in ChartRequest."});
}

Result<domain::ComputedChart> AstrologEmbeddedEngine::computeChartInternal(
    const domain::ChartRequest& request, const std::string& method) const {
  if (!m_initialized) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "Astrolog engine was not initialized successfully."});
  }

  // Build adapter input from ChartRequest
  // Note: ChartRequest contains birth event IDs, not raw birth data.
  // For embedded computation, the caller must provide coordinates via
  // a pre-resolved BirthEvent. We use placeholder defaults here and
  // expect the orchestrating service layer to set proper coordinates.
  AstrologAdapter::ChartInput input;
  // TODO: The ChartRequest currently holds birth event IDs rather than
  // raw coordinates. The service layer should resolve these before calling
  // the engine. For now we use the default location (Seattle) that
  // Astrolog initializes with, which lets us validate the computation pipeline.
  input.houseSystem = AstrologAdapter::mapHouseSystem(request.defaultHouseSystem);
  input.sidereal = (request.zodiacMode == "Sidereal");

  AstrologAdapter::ChartOutput adapterOutput;
  if (!AstrologAdapter::computeChart(input, adapterOutput)) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "Astrolog CastChart failed."});
  }

  // Normalize adapter output into Asteria domain types
  domain::ComputedChart chart;
  chart.chartRequestId = request.chartRequestId;
  chart.engineVersion = adapterOutput.engineVersion;
  chart.engineMethod = method + "/" + adapterOutput.engineMethod;
  chart.houseSystem = request.defaultHouseSystem;
  chart.zodiacMode = request.zodiacMode;

  // Generate a canonical hash from key input parameters
  std::ostringstream hashStream;
  hashStream << request.primaryBirthEventId << ":"
             << request.defaultHouseSystem << ":"
             << request.zodiacMode << ":"
             << input.year << "-" << input.month << "-" << input.day << "T"
             << input.timeHours;
  chart.canonicalHash = hashStream.str();

  // Normalize planet positions
  for (const auto& p : adapterOutput.planets) {
    domain::PlanetPosition pp;
    pp.objectId = p.name;
    pp.longitudeDegrees = p.longitude;
    pp.latitudeDegrees = p.latitude;
    pp.speed = p.speed;
    pp.sign = p.sign;
    pp.house = p.house;
    pp.retrograde = p.retrograde;
    chart.planets.push_back(pp);
  }

  // Normalize house cusps
  for (const auto& h : adapterOutput.houseCusps) {
    domain::HouseCusp hc;
    hc.houseNumber = h.houseNumber;
    hc.longitudeDegrees = h.longitude;
    chart.houseCusps.push_back(hc);
  }

  // Compute aspects from normalized positions
  chart.aspects = computeAspects(chart.planets);

  // Handle unknown-time policies
  if (!request.includeHouses) {
    chart.uncertaintyFlags.push_back("no_houses");
    chart.houseCusps.clear();
  }
  if (request.unknownTimePolicy == "noon_default_with_warning") {
    chart.uncertaintyFlags.push_back("noon_default_applied");
  }

  chart.chartMetadataJson = R"({"engine":"astrolog","version":")" +
      adapterOutput.engineVersion + R"("})";

  return Result<domain::ComputedChart>::success(chart);
}

std::vector<domain::Aspect> AstrologEmbeddedEngine::computeAspects(
    const std::vector<domain::PlanetPosition>& planets) const {
  // Standard aspect definitions: angle, name, default orb
  struct AspectDef {
    double angle;
    const char* name;
    double orb;
  };
  static const AspectDef aspects[] = {
    {0.0,   "Conjunction",       8.0},
    {180.0, "Opposition",        8.0},
    {90.0,  "Square",            7.0},
    {120.0, "Trine",             7.0},
    {60.0,  "Sextile",           6.0},
    {150.0, "Quincunx",          3.0},
    {30.0,  "SemiSextile",       3.0},
    {45.0,  "SemiSquare",        3.0},
    {135.0, "SesquiQuadrate",    3.0},
    {72.0,  "Quintile",          2.0},
    {144.0, "BiQuintile",        2.0},
  };

  std::vector<domain::Aspect> result;

  // Only compute aspects between the main 10 planets (Sun through Pluto)
  int limit = std::min(static_cast<int>(planets.size()), 10);
  for (int i = 0; i < limit; ++i) {
    for (int j = i + 1; j < limit; ++j) {
      double diff = std::fabs(planets[i].longitudeDegrees - planets[j].longitudeDegrees);
      if (diff > 180.0) diff = 360.0 - diff;

      for (const auto& asp : aspects) {
        double orbActual = std::fabs(diff - asp.angle);
        if (orbActual <= asp.orb) {
          domain::Aspect a;
          a.objectA = planets[i].objectId;
          a.objectB = planets[j].objectId;
          a.aspectType = asp.name;
          a.orbDegrees = orbActual;

          // Determine applying vs separating based on speed differential
          double speedDiff = planets[i].speed - planets[j].speed;
          double rawDiff = planets[i].longitudeDegrees - planets[j].longitudeDegrees;
          if (rawDiff < 0) rawDiff += 360.0;
          if (rawDiff > 180.0) rawDiff = 360.0 - rawDiff;
          // Simplified: if the faster planet is closing the gap, it's applying
          if (std::fabs(speedDiff) > 0.001) {
            a.applyingOrSeparating = (speedDiff < 0) ? "applying" : "separating";
          }

          result.push_back(a);
          break; // Only one aspect per pair
        }
      }
    }
  }

  return result;
}

core::Result<domain::ComputedChart> AstrologEmbeddedEngine::computeNatalChart(
    const domain::ChartRequest& request) const {
  return computeChartInternal(request, "AstrologEmbedded::Natal");
}

core::Result<domain::ComputedChart> AstrologEmbeddedEngine::computeSynastryChart(
    const domain::ChartRequest& request) const {
  // TODO: Implement proper synastry by computing two separate charts
  // and returning the inter-chart aspects. For now, compute as natal.
  return computeChartInternal(request, "AstrologEmbedded::Synastry");
}

core::Result<domain::ComputedChart> AstrologEmbeddedEngine::computeCompositeChart(
    const domain::ChartRequest& request) const {
  // TODO: Implement proper composite by computing midpoints between
  // two charts. For now, compute as natal.
  return computeChartInternal(request, "AstrologEmbedded::Composite");
}

core::Result<domain::ComputedChart> AstrologEmbeddedEngine::computeTransitToNatalChart(
    const domain::ChartRequest& request) const {
  // TODO: Implement proper transit-to-natal by computing current transit
  // positions against the natal chart. For now, compute as natal.
  return computeChartInternal(request, "AstrologEmbedded::TransitToNatal");
}

}  // namespace asteria::engine
