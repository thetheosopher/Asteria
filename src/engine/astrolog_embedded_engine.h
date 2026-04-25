#pragma once
#include "ichart_engine.h"
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

  /// Compute aspects from planet positions (standard orb-based detection).
  std::vector<domain::Aspect> computeAspects(
      const std::vector<domain::PlanetPosition>& planets) const;
};

}  // namespace asteria::engine
