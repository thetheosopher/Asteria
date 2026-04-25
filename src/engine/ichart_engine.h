#pragma once
#include <string>
#include <vector>
#include "core/result.h"
#include "domain/location_resolution.h"
#include "domain/chart_request.h"
#include "domain/computed_chart.h"

namespace asteria::engine {

class IChartEngine {
 public:
  virtual ~IChartEngine() = default;

  virtual core::Result<std::vector<domain::LocationResolution>> resolveLocation(
      const std::string& query,
      const std::string& optionalDateTimeContext = "") const = 0;

  virtual core::Result<domain::ComputedChart> computeNatalChart(
      const domain::ChartRequest& request) const = 0;

  virtual core::Result<domain::ComputedChart> computeSynastryChart(
      const domain::ChartRequest& request) const = 0;

  virtual core::Result<domain::ComputedChart> computeCompositeChart(
      const domain::ChartRequest& request) const = 0;

  virtual core::Result<domain::ComputedChart> computeTransitToNatalChart(
      const domain::ChartRequest& request) const = 0;

  virtual std::string getEngineVersion() const = 0;
};

}  // namespace asteria::engine
