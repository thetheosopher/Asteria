#include "comparison_chart_service.h"
#include "engine/ichart_engine.h"

namespace asteria::core {

ComparisonChartService::ComparisonChartService(engine::IChartEngine& chartEngine)
    : m_chartEngine(chartEngine) {}

Result<domain::ComputedChart> ComparisonChartService::computeSynastry(
    const domain::ChartRequest& request) const {
  if (!request.secondaryBirthEventId) {
    return Result<domain::ComputedChart>::failure(
        {"missing_secondary", "Synastry requires a secondary birth event"});
  }
  return m_chartEngine.computeSynastryChart(request);
}

Result<domain::ComputedChart> ComparisonChartService::computeComposite(
    const domain::ChartRequest& request) const {
  if (!request.secondaryBirthEventId) {
    return Result<domain::ComputedChart>::failure(
        {"missing_secondary", "Composite requires a secondary birth event"});
  }
  return m_chartEngine.computeCompositeChart(request);
}

Result<domain::ComputedChart> ComparisonChartService::computeTransitToNatal(
    const domain::ChartRequest& request) const {
  if (!request.requestTimeContext) {
    return Result<domain::ComputedChart>::failure(
        {"missing_transit_time", "Transit-to-natal requires a transit date/time context"});
  }
  return m_chartEngine.computeTransitToNatalChart(request);
}

}  // namespace asteria::core
