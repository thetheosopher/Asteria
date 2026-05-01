#include "chart_repository.h"
#include "sqlite_database.h"
#include <sqlite3.h>

namespace asteria::data {

ChartRepository::ChartRepository(SQLiteDatabase& db) : m_db(db) {}

bool ChartRepository::insertRequest(domain::ChartRequest& request) {
  const char* chartTypeStr = "natal";
  switch (request.chartType) {
    case domain::ChartType::Synastry: chartTypeStr = "synastry"; break;
    case domain::ChartType::Composite: chartTypeStr = "composite"; break;
    case domain::ChartType::TransitToNatal: chartTypeStr = "transit_to_natal"; break;
    default: break;
  }
  bool ok = m_db.executeWithParams(
    "INSERT INTO chart_requests (primary_birth_event_id, secondary_birth_event_id, "
    "chart_type, request_time_context, default_house_system, zodiac_mode, "
    "unknown_time_policy, include_houses, theme_id) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_int64(stmt, 1, request.primaryBirthEventId);
      if (request.secondaryBirthEventId) {
        sqlite3_bind_int64(stmt, 2, *request.secondaryBirthEventId);
      } else {
        sqlite3_bind_null(stmt, 2);
      }
      sqlite3_bind_text(stmt, 3, chartTypeStr, -1, SQLITE_STATIC);
      if (request.requestTimeContext) {
        sqlite3_bind_text(stmt, 4, request.requestTimeContext->c_str(), -1, SQLITE_TRANSIENT);
      } else {
        sqlite3_bind_null(stmt, 4);
      }
      sqlite3_bind_text(stmt, 5, request.defaultHouseSystem.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 6, request.zodiacMode.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 7, request.unknownTimePolicy.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(stmt, 8, request.includeHouses ? 1 : 0);
      if (request.themeId) {
        sqlite3_bind_int64(stmt, 9, *request.themeId);
      } else {
        sqlite3_bind_null(stmt, 9);
      }
    });
  if (ok) request.chartRequestId = m_db.lastInsertRowId();
  return ok;
}

bool ChartRepository::insertComputed(domain::ComputedChart& chart) {
  bool ok = m_db.executeWithParams(
    "INSERT INTO computed_charts (chart_request_id, engine_version, engine_method, "
    "canonical_hash, house_system, zodiac_mode, uncertainty_flags, "
    "chart_metadata_json, cached_json_blob) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_int64(stmt, 1, chart.chartRequestId);
      sqlite3_bind_text(stmt, 2, chart.engineVersion.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, chart.engineMethod.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, chart.canonicalHash.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 5, chart.houseSystem.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 6, chart.zodiacMode.c_str(), -1, SQLITE_TRANSIENT);
      std::string flags = "[";
      for (size_t i = 0; i < chart.uncertaintyFlags.size(); ++i) {
        if (i > 0) flags += ",";
        flags += "\"" + chart.uncertaintyFlags[i] + "\"";
      }
      flags += "]";
      sqlite3_bind_text(stmt, 7, flags.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 8, chart.chartMetadataJson.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 9, chart.cachedJsonBlob.c_str(), -1, SQLITE_TRANSIENT);
    });
  if (ok) chart.computedChartId = m_db.lastInsertRowId();
  return ok;
}

bool ChartRepository::insertExportArtifact(domain::ExportArtifact& artifact) {
  const char* exportTypeStr = "svg";
  switch (artifact.exportType) {
    case domain::ExportType::Png: exportTypeStr = "png"; break;
    case domain::ExportType::Pdf: exportTypeStr = "pdf"; break;
    case domain::ExportType::Svg: break;
  }

  bool ok = m_db.executeWithParams(
    "INSERT INTO export_artifacts (computed_chart_id, export_type, file_path, "
    "width_px, height_px, dpi, theme_snapshot_json, export_metadata_json) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_int64(stmt, 1, artifact.computedChartId);
      sqlite3_bind_text(stmt, 2, exportTypeStr, -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 3, artifact.filePath.c_str(), -1, SQLITE_TRANSIENT);
      if (artifact.widthPx) sqlite3_bind_int(stmt, 4, *artifact.widthPx);
      else sqlite3_bind_null(stmt, 4);
      if (artifact.heightPx) sqlite3_bind_int(stmt, 5, *artifact.heightPx);
      else sqlite3_bind_null(stmt, 5);
      if (artifact.dpi) sqlite3_bind_int(stmt, 6, *artifact.dpi);
      else sqlite3_bind_null(stmt, 6);
      sqlite3_bind_text(stmt, 7, artifact.themeSnapshotJson.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 8, artifact.exportMetadataJson.c_str(), -1, SQLITE_TRANSIENT);
    });
  if (ok) artifact.exportArtifactId = m_db.lastInsertRowId();
  return ok;
}

std::optional<domain::ComputedChart> ChartRepository::findComputedByHash(const std::string& canonicalHash) const {
  std::optional<domain::ComputedChart> result;
  m_db.queryWithParams(
    "SELECT computed_chart_id, chart_request_id, engine_version, engine_method, "
    "canonical_hash, house_system, zodiac_mode, uncertainty_flags, "
    "chart_metadata_json, cached_json_blob, created_at "
    "FROM computed_charts WHERE canonical_hash=?;",
    [&](sqlite3_stmt* stmt) { sqlite3_bind_text(stmt, 1, canonicalHash.c_str(), -1, SQLITE_TRANSIENT); },
    [&](sqlite3_stmt* stmt) {
      domain::ComputedChart c;
      c.computedChartId = sqlite3_column_int64(stmt, 0);
      c.chartRequestId = sqlite3_column_int64(stmt, 1);
      c.engineVersion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
      c.engineMethod = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
      c.canonicalHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
      c.houseSystem = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
      c.zodiacMode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
      c.chartMetadataJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
      c.cachedJsonBlob = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
      c.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
      result = c;
    });
  return result;
}

std::vector<domain::ChartRequest> ChartRepository::findRequestsByPerson(std::int64_t birthEventId) const {
  std::vector<domain::ChartRequest> results;
  m_db.queryWithParams(
    "SELECT chart_request_id, primary_birth_event_id, secondary_birth_event_id, "
    "chart_type, request_time_context, default_house_system, zodiac_mode, "
    "unknown_time_policy, include_houses, theme_id, created_at "
    "FROM chart_requests WHERE primary_birth_event_id=?;",
    [&](sqlite3_stmt* stmt) { sqlite3_bind_int64(stmt, 1, birthEventId); },
    [&](sqlite3_stmt* stmt) {
      domain::ChartRequest r;
      r.chartRequestId = sqlite3_column_int64(stmt, 0);
      r.primaryBirthEventId = sqlite3_column_int64(stmt, 1);
      if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
        r.secondaryBirthEventId = sqlite3_column_int64(stmt, 2);
      }
      auto ct = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
      if (ct == "synastry") r.chartType = domain::ChartType::Synastry;
      else if (ct == "composite") r.chartType = domain::ChartType::Composite;
      else if (ct == "transit_to_natal") r.chartType = domain::ChartType::TransitToNatal;
      else r.chartType = domain::ChartType::Natal;
      if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
        r.requestTimeContext = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
      }
      r.defaultHouseSystem = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
      r.zodiacMode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
      r.unknownTimePolicy = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
      r.includeHouses = sqlite3_column_int(stmt, 8) != 0;
      if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) {
        r.themeId = sqlite3_column_int64(stmt, 9);
      }
      r.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
      results.push_back(r);
    });
  return results;
}

}  // namespace asteria::data
