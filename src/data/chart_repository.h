#pragma once
#include <vector>
#include <optional>
#include "domain/chart_request.h"
#include "domain/computed_chart.h"

namespace asteria::data {

class SQLiteDatabase;

class ChartRepository {
 public:
  explicit ChartRepository(SQLiteDatabase& db);

  bool insertRequest(domain::ChartRequest& request);
  bool insertComputed(domain::ComputedChart& chart);
  std::optional<domain::ComputedChart> findComputedByHash(const std::string& canonicalHash) const;
  std::vector<domain::ChartRequest> findRequestsByPerson(std::int64_t birthEventId) const;

 private:
  SQLiteDatabase& m_db;
};

}  // namespace asteria::data
