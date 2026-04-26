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
  const std::string&, const std::string&) const {
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

  // Build adapter input from ChartRequest. Prefer the resolved primaryInput
  // populated by the service layer; fall back to engine defaults if absent
  // (preserves back-compat for tests/CLI that don't yet set primaryInput).
  AstrologAdapter::ChartInput input;
  input.houseSystem = AstrologAdapter::mapHouseSystem(request.defaultHouseSystem);
  input.sidereal = (request.zodiacMode == "Sidereal");

  if (request.primaryInput) {
    const auto& p = *request.primaryInput;
    input.year = p.year;
    input.month = p.month;
    input.day = p.day;
    input.timeHours = p.timeHours;
    input.timezoneHours = p.timezoneHours;
    input.dstHours = p.dstHours;
    input.latitudeDeg = p.latitudeDeg;
    input.longitudeDeg = p.longitudeDeg;
  }

  AstrologAdapter::ChartOutput adapterOutput;
  if (!AstrologAdapter::computeChart(input, adapterOutput)) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "Astrolog CastChart failed."});
  }

  return finalizeChart(request, method, input, adapterOutput);
}

Result<domain::ComputedChart> AstrologEmbeddedEngine::finalizeChart(
    const domain::ChartRequest& request,
    const std::string& method,
    const AstrologAdapter::ChartInput& input,
    const AstrologAdapter::ChartOutput& adapterOutput) const {
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
             << static_cast<int>(request.chartType) << ":"
             << request.defaultHouseSystem << ":"
             << request.zodiacMode << ":"
             << input.year << "-" << input.month << "-" << input.day << "T"
             << input.timeHours << ":"
             << input.latitudeDeg << "," << input.longitudeDeg << "@"
             << input.timezoneHours << "+" << input.dstHours;
  if (request.secondaryInput) {
    const auto& s = *request.secondaryInput;
    hashStream << "|S:" << s.year << "-" << s.month << "-" << s.day << "T"
               << s.timeHours << ":" << s.latitudeDeg << "," << s.longitudeDeg;
  }
  if (request.transitInput) {
    const auto& t = *request.transitInput;
    hashStream << "|T:" << t.year << "-" << t.month << "-" << t.day << "T"
               << t.timeHours;
  }
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
    // Strip house assignments from planets when houses are suppressed
    for (auto& planet : chart.planets) {
      planet.house.reset();
    }
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

AstrologAdapter::ChartInput AstrologEmbeddedEngine::buildInput(
    const domain::ResolvedBirthInput& src,
    const domain::ChartRequest& request) const {
  AstrologAdapter::ChartInput input;
  input.year = src.year;
  input.month = src.month;
  input.day = src.day;
  input.timeHours = src.timeHours;
  input.timezoneHours = src.timezoneHours;
  input.dstHours = src.dstHours;
  input.latitudeDeg = src.latitudeDeg;
  input.longitudeDeg = src.longitudeDeg;
  input.houseSystem = AstrologAdapter::mapHouseSystem(request.defaultHouseSystem);
  input.sidereal = (request.zodiacMode == "Sidereal");
  return input;
}

core::Result<domain::ComputedChart> AstrologEmbeddedEngine::computeSynastryChart(
    const domain::ChartRequest& request) const {
  if (!m_initialized) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "Astrolog engine was not initialized successfully."});
  }
  if (!request.primaryInput || !request.secondaryInput) {
    return Result<domain::ComputedChart>::failure(
        {"missing_input", "Synastry requires both primaryInput and secondaryInput "
         "(resolved birth data) on the ChartRequest."});
  }

  // Compute primary chart
  AstrologAdapter::ChartInput inputA = buildInput(*request.primaryInput, request);
  AstrologAdapter::ChartOutput outA;
  if (!AstrologAdapter::computeChart(inputA, outA)) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "Astrolog CastChart failed for primary."});
  }
  auto primaryR = finalizeChart(request, "AstrologEmbedded::Synastry/A", inputA, outA);
  if (!primaryR.ok()) return primaryR;

  // Compute secondary chart
  AstrologAdapter::ChartInput inputB = buildInput(*request.secondaryInput, request);
  AstrologAdapter::ChartOutput outB;
  if (!AstrologAdapter::computeChart(inputB, outB)) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "Astrolog CastChart failed for secondary."});
  }
  // Build a minimal secondary ChartRequest copy for finalization
  domain::ChartRequest reqB = request;
  reqB.primaryInput = request.secondaryInput;
  auto secondaryR = finalizeChart(reqB, "AstrologEmbedded::Synastry/B", inputB, outB);
  if (!secondaryR.ok()) return secondaryR;

  domain::ComputedChart chart = primaryR.value();
  chart.secondaryChart = std::make_shared<domain::ComputedChart>(secondaryR.value());

  // Compute inter-chart aspects (planet from A vs. planet from B)
  // Reuse the aspect orb table by building a unified list and tagging via
  // objectA="A:Sun"/objectB="B:Moon" naming.
  std::vector<domain::PlanetPosition> joined;
  for (const auto& p : chart.planets) {
    auto pp = p; pp.objectId = "A:" + pp.objectId; joined.push_back(pp);
  }
  for (const auto& p : chart.secondaryChart->planets) {
    auto pp = p; pp.objectId = "B:" + pp.objectId; joined.push_back(pp);
  }
  auto allAspects = computeAspects(joined);
  // Keep only inter-chart aspects (one A:* and one B:*)
  for (const auto& a : allAspects) {
    bool aIsA = a.objectA.rfind("A:", 0) == 0;
    bool bIsB = a.objectB.rfind("B:", 0) == 0;
    bool aIsB = a.objectA.rfind("B:", 0) == 0;
    bool bIsA = a.objectB.rfind("A:", 0) == 0;
    if ((aIsA && bIsB) || (aIsB && bIsA)) {
      chart.interAspects.push_back(a);
    }
  }
  return Result<domain::ComputedChart>::success(chart);
}

core::Result<domain::ComputedChart> AstrologEmbeddedEngine::computeCompositeChart(
    const domain::ChartRequest& request) const {
  if (!m_initialized) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "Astrolog engine was not initialized successfully."});
  }
  if (!request.primaryInput || !request.secondaryInput) {
    return Result<domain::ComputedChart>::failure(
        {"missing_input", "Composite requires both primaryInput and secondaryInput."});
  }

  // Compute both natal charts
  AstrologAdapter::ChartInput inputA = buildInput(*request.primaryInput, request);
  AstrologAdapter::ChartOutput outA;
  if (!AstrologAdapter::computeChart(inputA, outA)) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "CastChart failed for composite primary."});
  }
  AstrologAdapter::ChartInput inputB = buildInput(*request.secondaryInput, request);
  AstrologAdapter::ChartOutput outB;
  if (!AstrologAdapter::computeChart(inputB, outB)) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "CastChart failed for composite secondary."});
  }

  // Build composite as midpoints of corresponding planets / cusps.
  AstrologAdapter::ChartOutput composite;
  composite.engineVersion = outA.engineVersion;
  composite.engineMethod = outA.engineMethod + "+midpoint";

  auto midLongitude = [](double a, double b) {
    double diff = b - a;
    if (diff > 180.0) diff -= 360.0;
    if (diff < -180.0) diff += 360.0;
    double m = a + diff / 2.0;
    if (m < 0.0) m += 360.0;
    if (m >= 360.0) m -= 360.0;
    return m;
  };

  std::size_t n = std::min(outA.planets.size(), outB.planets.size());
  for (std::size_t i = 0; i < n; ++i) {
    AstrologAdapter::ChartOutput::PlanetData p = outA.planets[i];
    p.longitude = midLongitude(outA.planets[i].longitude, outB.planets[i].longitude);
    p.latitude = (outA.planets[i].latitude + outB.planets[i].latitude) / 2.0;
    p.speed = (outA.planets[i].speed + outB.planets[i].speed) / 2.0;
    int signIdx = static_cast<int>(p.longitude) / 30 + 1;
    p.signIndex = signIdx;
    p.sign = AstrologAdapter::signName(signIdx);
    p.retrograde = p.speed < 0.0;
    composite.planets.push_back(p);
  }
  std::size_t hc = std::min(outA.houseCusps.size(), outB.houseCusps.size());
  for (std::size_t i = 0; i < hc; ++i) {
    AstrologAdapter::ChartOutput::HouseCuspData c = outA.houseCusps[i];
    c.longitude = midLongitude(outA.houseCusps[i].longitude, outB.houseCusps[i].longitude);
    composite.houseCusps.push_back(c);
  }

  return finalizeChart(request, "AstrologEmbedded::Composite", inputA, composite);
}

core::Result<domain::ComputedChart> AstrologEmbeddedEngine::computeTransitToNatalChart(
    const domain::ChartRequest& request) const {
  if (!m_initialized) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "Astrolog engine was not initialized successfully."});
  }
  if (!request.primaryInput) {
    return Result<domain::ComputedChart>::failure(
        {"missing_input", "Transit-to-natal requires primaryInput (natal data)."});
  }
  if (!request.transitInput) {
    return Result<domain::ComputedChart>::failure(
        {"missing_input", "Transit-to-natal requires transitInput (transit moment data)."});
  }

  // Compute natal chart as the primary (this drives houses/cusps)
  auto natal = computeChartInternal(request, "AstrologEmbedded::TransitToNatal/Natal");
  if (!natal.ok()) return natal;

  // Compute transit chart at the requested moment, using natal location
  domain::ResolvedBirthInput tInput = *request.transitInput;
  // If transit lat/lon were not provided, default to natal location
  if (tInput.latitudeDeg == 0.0 && tInput.longitudeDeg == 0.0) {
    tInput.latitudeDeg = request.primaryInput->latitudeDeg;
    tInput.longitudeDeg = request.primaryInput->longitudeDeg;
    tInput.timezoneHours = request.primaryInput->timezoneHours;
  }
  AstrologAdapter::ChartInput inputT = buildInput(tInput, request);
  AstrologAdapter::ChartOutput outT;
  if (!AstrologAdapter::computeChart(inputT, outT)) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "CastChart failed for transit moment."});
  }

  domain::ChartRequest reqT = request;
  reqT.primaryInput = tInput;
  reqT.includeHouses = false;  // Transit chart needs no separate houses
  auto transitR = finalizeChart(reqT, "AstrologEmbedded::TransitToNatal/Transit", inputT, outT);
  if (!transitR.ok()) return transitR;

  domain::ComputedChart chart = natal.value();
  chart.secondaryChart = std::make_shared<domain::ComputedChart>(transitR.value());

  // Cross-aspects: natal vs transit
  std::vector<domain::PlanetPosition> joined;
  for (const auto& p : chart.planets) {
    auto pp = p; pp.objectId = "N:" + pp.objectId; joined.push_back(pp);
  }
  for (const auto& p : chart.secondaryChart->planets) {
    auto pp = p; pp.objectId = "T:" + pp.objectId; joined.push_back(pp);
  }
  auto allAspects = computeAspects(joined);
  for (const auto& a : allAspects) {
    bool aIsN = a.objectA.rfind("N:", 0) == 0;
    bool bIsT = a.objectB.rfind("T:", 0) == 0;
    bool aIsT = a.objectA.rfind("T:", 0) == 0;
    bool bIsN = a.objectB.rfind("N:", 0) == 0;
    if ((aIsN && bIsT) || (aIsT && bIsN)) {
      chart.interAspects.push_back(a);
    }
  }
  return Result<domain::ComputedChart>::success(chart);
}

}  // namespace asteria::engine
