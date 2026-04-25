#pragma once
#include "result.h"
#include "domain/computed_chart.h"
#include "domain/interpretation_note.h"

namespace asteria::core {

class InterpretationService {
 public:
  /// Generate a deterministic built-in interpretation for the given chart.
  /// Consumes normalized chart facts and emits structured markdown.
  Result<domain::InterpretationNote> generateBuiltIn(
      const domain::ComputedChart& chart,
      domain::ChartType chartType) const;

 private:
  std::string generateNatalInterpretation(const domain::ComputedChart& chart) const;
  std::string generateSynastryInterpretation(const domain::ComputedChart& chart) const;
  std::string generateCompositeInterpretation(const domain::ComputedChart& chart) const;
  std::string generateTransitInterpretation(const domain::ComputedChart& chart) const;
  std::string generateUncertaintyDisclaimer(const domain::ComputedChart& chart) const;
  std::string describePlanet(const domain::PlanetPosition& planet) const;
  std::string describeAspect(const domain::Aspect& aspect) const;
};

}  // namespace asteria::core
