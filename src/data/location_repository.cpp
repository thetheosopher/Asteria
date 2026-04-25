#include "location_repository.h"
#include "sqlite_database.h"
#include <sqlite3.h>

namespace asteria::data {

LocationRepository::LocationRepository(SQLiteDatabase& db) : m_db(db) {}

bool LocationRepository::insert(domain::LocationResolution& loc) {
  bool ok = m_db.executeWithParams(
    "INSERT INTO locations (query_text, display_name, country, region, latitude, longitude, "
    "timezone_name, timezone_offset_rule_ref, resolution_method, confidence_score, "
    "provenance_note, map_pin_precision_meters) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_text(stmt, 1, loc.queryText.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, loc.displayName.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, loc.country.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, loc.region.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_double(stmt, 5, loc.latitude);
      sqlite3_bind_double(stmt, 6, loc.longitude);
      sqlite3_bind_text(stmt, 7, loc.timezoneName.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 8, loc.timezoneOffsetRuleRef.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 9, loc.resolutionMethod.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_double(stmt, 10, loc.confidenceScore);
      sqlite3_bind_text(stmt, 11, loc.provenanceNote.c_str(), -1, SQLITE_TRANSIENT);
      if (loc.mapPinPrecisionMeters) {
        sqlite3_bind_double(stmt, 12, *loc.mapPinPrecisionMeters);
      } else {
        sqlite3_bind_null(stmt, 12);
      }
    });
  if (ok) loc.locationId = m_db.lastInsertRowId();
  return ok;
}

static domain::LocationResolution rowToLocation(sqlite3_stmt* stmt) {
  domain::LocationResolution loc;
  loc.locationId = sqlite3_column_int64(stmt, 0);
  loc.queryText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  loc.displayName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  loc.country = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
  loc.region = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
  loc.latitude = sqlite3_column_double(stmt, 5);
  loc.longitude = sqlite3_column_double(stmt, 6);
  loc.timezoneName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
  loc.timezoneOffsetRuleRef = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
  loc.resolutionMethod = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
  loc.confidenceScore = sqlite3_column_double(stmt, 10);
  loc.provenanceNote = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
  if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
    loc.mapPinPrecisionMeters = sqlite3_column_double(stmt, 12);
  }
  loc.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
  loc.updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
  return loc;
}

std::optional<domain::LocationResolution> LocationRepository::findById(std::int64_t locationId) const {
  std::optional<domain::LocationResolution> result;
  m_db.queryWithParams(
    "SELECT location_id, query_text, display_name, country, region, latitude, longitude, "
    "timezone_name, timezone_offset_rule_ref, resolution_method, confidence_score, "
    "provenance_note, map_pin_precision_meters, created_at, updated_at "
    "FROM locations WHERE location_id=?;",
    [&](sqlite3_stmt* stmt) { sqlite3_bind_int64(stmt, 1, locationId); },
    [&](sqlite3_stmt* stmt) { result = rowToLocation(stmt); });
  return result;
}

std::vector<domain::LocationResolution> LocationRepository::search(const std::string& query) const {
  std::vector<domain::LocationResolution> results;
  std::string pattern = "%" + query + "%";
  m_db.queryWithParams(
    "SELECT location_id, query_text, display_name, country, region, latitude, longitude, "
    "timezone_name, timezone_offset_rule_ref, resolution_method, confidence_score, "
    "provenance_note, map_pin_precision_meters, created_at, updated_at "
    "FROM locations WHERE display_name LIKE ? OR query_text LIKE ? ORDER BY display_name;",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
    },
    [&](sqlite3_stmt* stmt) { results.push_back(rowToLocation(stmt)); });
  return results;
}

}  // namespace asteria::data
