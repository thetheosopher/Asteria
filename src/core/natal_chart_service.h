#pragma once
#include "result.h"
#include "domain/chart_request.h"
#include "domain/computed_chart.h"

namespace asteria::engine { class IChartEngine; }

namespace asteria::core {

class NatalChartService {
 public:
  explicit NatalChartService(engine::IChartEngine& chartEngine);
  Result<domain::ComputedChart> compute(const domain::ChartRequest& request) const;

 private:
  engine::IChartEngine& m_chartEngine;
};

}  // namespace asteria::core
