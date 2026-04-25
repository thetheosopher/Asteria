#include "natal_chart_service.h"
#include "engine/ichart_engine.h"

namespace asteria::core {

NatalChartService::NatalChartService(engine::IChartEngine& chartEngine) : m_chartEngine(chartEngine) {}

Result<domain::ComputedChart> NatalChartService::compute(const domain::ChartRequest& request) const {
  return m_chartEngine.computeNatalChart(request);
}

}  // namespace asteria::core
