#include "interpretation_service.h"
#include <sstream>

namespace asteria::core {

Result<domain::InterpretationNote> InterpretationService::generateBuiltIn(
    const domain::ComputedChart& chart,
    domain::ChartType chartType) const {
  domain::InterpretationNote note;
  note.computedChartId = chart.computedChartId;
  note.sourceType = domain::InterpretationSourceType::BuiltIn;
  note.promptVersion = "built-in-v1";

  std::string body;
  switch (chartType) {
    case domain::ChartType::Natal:
      body = generateNatalInterpretation(chart);
      break;
    case domain::ChartType::Synastry:
      body = generateSynastryInterpretation(chart);
      break;
    case domain::ChartType::Composite:
      body = generateCompositeInterpretation(chart);
      break;
    case domain::ChartType::TransitToNatal:
      body = generateTransitInterpretation(chart);
      break;
  }

  std::string disclaimer = generateUncertaintyDisclaimer(chart);
  if (!disclaimer.empty()) {
    body = disclaimer + "\n\n" + body;
    note.certaintyGuardrailsApplied = true;
  }

  note.bodyMarkdown = body;
  return Result<domain::InterpretationNote>::success(note);
}

std::string InterpretationService::generateNatalInterpretation(
    const domain::ComputedChart& chart) const {
  std::ostringstream ss;
  ss << "## Natal Chart Summary\n\n";
  ss << "**House System:** " << chart.houseSystem << "  \n";
  ss << "**Zodiac Mode:** " << chart.zodiacMode << "\n\n";

  if (!chart.planets.empty()) {
    ss << "### Planetary Positions\n\n";
    for (const auto& p : chart.planets) {
      ss << "- " << describePlanet(p) << "\n";
    }
    ss << "\n";
  }

  if (!chart.aspects.empty()) {
    ss << "### Key Aspects\n\n";
    for (const auto& a : chart.aspects) {
      ss << "- " << describeAspect(a) << "\n";
    }
    ss << "\n";
  }

  return ss.str();
}

std::string InterpretationService::generateSynastryInterpretation(
    const domain::ComputedChart& chart) const {
  std::ostringstream ss;
  ss << "## Synastry Chart Summary\n\n";
  ss << "This synastry chart compares planetary positions between two individuals.\n\n";
  ss << "**House System:** " << chart.houseSystem << "  \n";
  ss << "**Zodiac Mode:** " << chart.zodiacMode << "\n\n";

  if (!chart.aspects.empty()) {
    ss << "### Inter-Chart Aspects\n\n";
    for (const auto& a : chart.aspects) {
      ss << "- " << describeAspect(a) << "\n";
    }
    ss << "\n";
  }

  return ss.str();
}

std::string InterpretationService::generateCompositeInterpretation(
    const domain::ComputedChart& chart) const {
  std::ostringstream ss;
  ss << "## Composite Chart Summary\n\n";
  ss << "The composite chart represents the midpoint relationship dynamic.\n\n";
  ss << "**House System:** " << chart.houseSystem << "  \n";
  ss << "**Zodiac Mode:** " << chart.zodiacMode << "\n\n";

  if (!chart.planets.empty()) {
    ss << "### Composite Planetary Positions\n\n";
    for (const auto& p : chart.planets) {
      ss << "- " << describePlanet(p) << "\n";
    }
    ss << "\n";
  }

  return ss.str();
}

std::string InterpretationService::generateTransitInterpretation(
    const domain::ComputedChart& chart) const {
  std::ostringstream ss;
  ss << "## Transit-to-Natal Chart Summary\n\n";
  ss << "Current transiting planets are compared against the natal chart.\n\n";
  ss << "**House System:** " << chart.houseSystem << "  \n";
  ss << "**Zodiac Mode:** " << chart.zodiacMode << "\n\n";

  if (!chart.aspects.empty()) {
    ss << "### Active Transit Aspects\n\n";
    for (const auto& a : chart.aspects) {
      ss << "- " << describeAspect(a) << "\n";
    }
    ss << "\n";
  }

  return ss.str();
}

std::string InterpretationService::generateUncertaintyDisclaimer(
    const domain::ComputedChart& chart) const {
  if (chart.uncertaintyFlags.empty()) return "";

  std::ostringstream ss;
  ss << "> **Uncertainty Notice:** This chart was computed with the following caveats:\n";
  for (const auto& flag : chart.uncertaintyFlags) {
    if (flag == "noon_default_applied") {
      ss << "> - Birth time was unknown; noon was used as a default. "
         << "House placements and Moon position may be inaccurate.\n";
    } else if (flag == "approximate_time") {
      ss << "> - Birth time is approximate. House cusps and fast-moving "
         << "bodies (especially the Moon) may have reduced accuracy.\n";
    } else if (flag == "no_houses") {
      ss << "> - Houses are suppressed because birth time is unavailable. "
         << "All house-based interpretations are omitted.\n";
    } else {
      ss << "> - " << flag << "\n";
    }
  }
  ss << "> \n> House-based claims should be treated with caution.\n";
  return ss.str();
}

std::string InterpretationService::describePlanet(const domain::PlanetPosition& planet) const {
  std::ostringstream ss;
  ss << "**" << planet.objectId << "** in " << planet.sign;
  if (planet.house) {
    ss << " (House " << *planet.house << ")";
  }
  int deg = static_cast<int>(planet.longitudeDegrees) % 30;
  int min = static_cast<int>((planet.longitudeDegrees - static_cast<int>(planet.longitudeDegrees)) * 60) % 60;
  if (min < 0) min = -min;
  ss << " at " << deg << "\u00b0" << min << "'";
  if (planet.retrograde) {
    ss << " (Rx)";
  }
  return ss.str();
}

std::string InterpretationService::describeAspect(const domain::Aspect& aspect) const {
  std::ostringstream ss;
  ss << aspect.objectA << " " << aspect.aspectType << " " << aspect.objectB;
  ss << " (orb: " << aspect.orbDegrees << "\u00b0";
  if (aspect.applyingOrSeparating) {
    ss << ", " << *aspect.applyingOrSeparating;
  }
  ss << ")";
  return ss.str();
}

}  // namespace asteria::core
