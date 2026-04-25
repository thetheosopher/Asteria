#pragma once
#include "ichart_engine.h"
#include "astrolog_adapter.h"
#include <string>

namespace asteria::engine {

class AstrologEmbeddedEngine final : public IChartEngine {
 public:
  /// Construct with the path to the Astrolog data directory
  /// (containing astrolog.as, atlas.as, ephem/, etc.)
  explicit AstrologEmbeddedEngine(const std::string& dataPath);

  core::Result<std::vector<domain::LocationResolution>> resolveLocation(
      const std::string& query,
      const std::string& optionalDateTimeContext = "") const override;
  core::Result<domain::ComputedChart> computeNatalChart(const domain::ChartRequest& request) const override;
  core::Result<domain::ComputedChart> computeSynastryChart(const domain::ChartRequest& request) const override;
  core::Result<domain::ComputedChart> computeCompositeChart(const domain::ChartRequest& request) const override;
  core::Result<domain::ComputedChart> computeTransitToNatalChart(const domain::ChartRequest& request) const override;
  std::string getEngineVersion() const override;

 private:
  bool m_initialized = false;

  /// Shared computation logic: set up adapter input from ChartRequest, call CastChart, normalize output.
  core::Result<domain::ComputedChart> computeChartInternal(
      const domain::ChartRequest& request, const std::string& method) const;

  /// Normalize adapter output into a domain::ComputedChart with metadata, aspects, and uncertainty flags.
  core::Result<domain::ComputedChart> finalizeChart(
      const domain::ChartRequest& request,
      const std::string& method,
      const AstrologAdapter::ChartInput& input,
      const AstrologAdapter::ChartOutput& adapterOutput) const;

  /// Build adapter input directly from a ResolvedBirthInput (used for secondary/transit charts).
  AstrologAdapter::ChartInput buildInput(
      const domain::ResolvedBirthInput& src,
      const domain::ChartRequest& request) const;

  /// Compute aspects from planet positions (standard orb-based detection).
  std::vector<domain::Aspect> computeAspects(
      const std::vector<domain::PlanetPosition>& planets) const;
};

}  // namespace asteria::engine
