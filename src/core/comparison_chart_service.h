#pragma once
#include "result.h"
#include "domain/chart_request.h"
#include "domain/computed_chart.h"

namespace asteria::engine { class IChartEngine; }

namespace asteria::core {

class ComparisonChartService {
 public:
  explicit ComparisonChartService(engine::IChartEngine& chartEngine);

  Result<domain::ComputedChart> computeSynastry(const domain::ChartRequest& request) const;
  Result<domain::ComputedChart> computeComposite(const domain::ChartRequest& request) const;
  Result<domain::ComputedChart> computeTransitToNatal(const domain::ChartRequest& request) const;

 private:
  engine::IChartEngine& m_chartEngine;
};

}  // namespace asteria::core
