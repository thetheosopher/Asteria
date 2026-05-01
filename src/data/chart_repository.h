#pragma once
#include <vector>
#include <optional>
#include "domain/chart_request.h"
#include "domain/computed_chart.h"
#include "domain/export_artifact.h"

namespace asteria::data {

class SQLiteDatabase;

class ChartRepository {
 public:
  explicit ChartRepository(SQLiteDatabase& db);

  bool insertRequest(domain::ChartRequest& request);
  bool insertComputed(domain::ComputedChart& chart);
  bool insertExportArtifact(domain::ExportArtifact& artifact);
  std::optional<domain::ComputedChart> findComputedByHash(const std::string& canonicalHash) const;
  std::vector<domain::ChartRequest> findRequestsByPerson(std::int64_t birthEventId) const;

 private:
  SQLiteDatabase& m_db;
};

}  // namespace asteria::data
