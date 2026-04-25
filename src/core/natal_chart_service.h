#pragma once
#include "result.h"
#include "domain/chart_request.h"
#include "domain/computed_chart.h"

namespace asteria::engine { class IChartEngine; }
namespace asteria::data   { class ChartRepository; }

namespace asteria::core {

class NatalChartService {
 public:
  explicit NatalChartService(engine::IChartEngine& chartEngine);

  /// Compute (or fetch from cache, if a repository has been attached).
  Result<domain::ComputedChart> compute(const domain::ChartRequest& request) const;

  /// Attach an optional cache/persistence layer. When set, compute() will
  /// first probe the repository for a chart with the same canonicalHash and
  /// return it as a hit; on miss, the freshly-computed chart is persisted.
  void attachRepository(data::ChartRepository* repo) { m_repo = repo; }

 private:
  engine::IChartEngine& m_chartEngine;
  data::ChartRepository* m_repo = nullptr;  // not owned
};

}  // namespace asteria::core
