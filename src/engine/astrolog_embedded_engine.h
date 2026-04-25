#pragma once
#include "ichart_engine.h"

namespace asteria::engine {

class AstrologEmbeddedEngine final : public IChartEngine {
 public:
  core::Result<std::vector<domain::LocationResolution>> resolveLocation(
      const std::string& query,
      const std::string& optionalDateTimeContext = "") const override;
  core::Result<domain::ComputedChart> computeNatalChart(const domain::ChartRequest& request) const override;
  core::Result<domain::ComputedChart> computeSynastryChart(const domain::ChartRequest& request) const override;
  core::Result<domain::ComputedChart> computeCompositeChart(const domain::ChartRequest& request) const override;
  core::Result<domain::ComputedChart> computeTransitToNatalChart(const domain::ChartRequest& request) const override;
  std::string getEngineVersion() const override;
};

}  // namespace asteria::engine
