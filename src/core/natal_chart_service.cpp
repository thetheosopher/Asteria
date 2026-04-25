#include "natal_chart_service.h"
#include "engine/ichart_engine.h"
#include "data/chart_repository.h"

namespace asteria::core {

NatalChartService::NatalChartService(engine::IChartEngine& chartEngine) : m_chartEngine(chartEngine) {}

Result<domain::ComputedChart> NatalChartService::compute(const domain::ChartRequest& request) const {
  // Compute first; the engine returns a chart that includes a canonicalHash
  // derived purely from inputs. We then check the repository for an existing
  // entry with that hash. If hit, return the cached version (which carries
  // the persisted computedChartId); otherwise persist the new one.
  auto result = m_chartEngine.computeNatalChart(request);
  if (!result.ok() || !m_repo) return result;

  auto cached = m_repo->findComputedByHash(result.value().canonicalHash);
  if (cached) {
    // Restore in-memory data from the freshly computed result (the cached
    // row only stores metadata + JSON blob, not the planet vectors).
    cached->planets    = result.value().planets;
    cached->houseCusps = result.value().houseCusps;
    cached->aspects    = result.value().aspects;
    return Result<domain::ComputedChart>::success(*cached);
  }

  // Persist the new chart (best-effort; failure is non-fatal).
  domain::ComputedChart toStore = result.value();
  m_repo->insertComputed(toStore);
  return Result<domain::ComputedChart>::success(toStore);
}

}  // namespace asteria::core
