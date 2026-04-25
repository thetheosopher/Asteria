#include "birth_event_repository.h"
#include "sqlite_database.h"
#include <sqlite3.h>

namespace asteria::data {

BirthEventRepository::BirthEventRepository(SQLiteDatabase& db) : m_db(db) {}

static void bindOptionalDouble(sqlite3_stmt* stmt, int idx, const std::optional<double>& v) {
  if (v) sqlite3_bind_double(stmt, idx, *v);
  else   sqlite3_bind_null(stmt, idx);
}

bool BirthEventRepository::insert(domain::BirthEvent& event) {
  bool ok = m_db.executeWithParams(
    "INSERT INTO birth_events (person_id, birth_date, birth_time, time_accuracy, "
    "noon_default_applied, houses_enabled, source_description, confidence_score, "
    "city_input, location_id, timezone_name, dst_mode, "
    "latitude_deg, longitude_deg, timezone_offset_hours, dst_offset_hours) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_int64(stmt, 1, event.personId);
      sqlite3_bind_text(stmt, 2, event.birthDate.c_str(), -1, SQLITE_TRANSIENT);
      if (event.birthTime) {
        sqlite3_bind_text(stmt, 3, event.birthTime->c_str(), -1, SQLITE_TRANSIENT);
      } else {
        sqlite3_bind_null(stmt, 3);
      }
      const char* acc = "unknown";
      if (event.timeAccuracy == domain::TimeAccuracy::Exact) acc = "exact";
      else if (event.timeAccuracy == domain::TimeAccuracy::Approximate) acc = "approximate";
      sqlite3_bind_text(stmt, 4, acc, -1, SQLITE_STATIC);
      sqlite3_bind_int(stmt, 5, event.noonDefaultApplied ? 1 : 0);
      sqlite3_bind_int(stmt, 6, event.housesEnabled ? 1 : 0);
      sqlite3_bind_text(stmt, 7, event.sourceDescription.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_double(stmt, 8, event.confidenceScore);
      sqlite3_bind_text(stmt, 9, event.cityInput.c_str(), -1, SQLITE_TRANSIENT);
      if (event.locationId) {
        sqlite3_bind_int64(stmt, 10, *event.locationId);
      } else {
        sqlite3_bind_null(stmt, 10);
      }
      if (event.timezoneName) {
        sqlite3_bind_text(stmt, 11, event.timezoneName->c_str(), -1, SQLITE_TRANSIENT);
      } else {
        sqlite3_bind_null(stmt, 11);
      }
      sqlite3_bind_text(stmt, 12, event.dstMode.c_str(), -1, SQLITE_TRANSIENT);
      bindOptionalDouble(stmt, 13, event.latitudeDeg);
      bindOptionalDouble(stmt, 14, event.longitudeDeg);
      bindOptionalDouble(stmt, 15, event.timezoneOffsetHours);
      bindOptionalDouble(stmt, 16, event.dstOffsetHours);
    });
  if (ok) event.birthEventId = m_db.lastInsertRowId();
  return ok;
}

bool BirthEventRepository::update(const domain::BirthEvent& event) {
  return m_db.executeWithParams(
    "UPDATE birth_events SET birth_date=?, birth_time=?, time_accuracy=?, "
    "noon_default_applied=?, houses_enabled=?, source_description=?, "
    "confidence_score=?, city_input=?, location_id=?, timezone_name=?, "
    "dst_mode=?, latitude_deg=?, longitude_deg=?, "
    "timezone_offset_hours=?, dst_offset_hours=?, "
    "updated_at=datetime('now') WHERE birth_event_id=?;",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_text(stmt, 1, event.birthDate.c_str(), -1, SQLITE_TRANSIENT);
      if (event.birthTime) {
        sqlite3_bind_text(stmt, 2, event.birthTime->c_str(), -1, SQLITE_TRANSIENT);
      } else {
        sqlite3_bind_null(stmt, 2);
      }
      const char* acc = "unknown";
      if (event.timeAccuracy == domain::TimeAccuracy::Exact) acc = "exact";
      else if (event.timeAccuracy == domain::TimeAccuracy::Approximate) acc = "approximate";
      sqlite3_bind_text(stmt, 3, acc, -1, SQLITE_STATIC);
      sqlite3_bind_int(stmt, 4, event.noonDefaultApplied ? 1 : 0);
      sqlite3_bind_int(stmt, 5, event.housesEnabled ? 1 : 0);
      sqlite3_bind_text(stmt, 6, event.sourceDescription.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_double(stmt, 7, event.confidenceScore);
      sqlite3_bind_text(stmt, 8, event.cityInput.c_str(), -1, SQLITE_TRANSIENT);
      if (event.locationId) {
        sqlite3_bind_int64(stmt, 9, *event.locationId);
      } else {
        sqlite3_bind_null(stmt, 9);
      }
      if (event.timezoneName) {
        sqlite3_bind_text(stmt, 10, event.timezoneName->c_str(), -1, SQLITE_TRANSIENT);
      } else {
        sqlite3_bind_null(stmt, 10);
      }
      sqlite3_bind_text(stmt, 11, event.dstMode.c_str(), -1, SQLITE_TRANSIENT);
      bindOptionalDouble(stmt, 12, event.latitudeDeg);
      bindOptionalDouble(stmt, 13, event.longitudeDeg);
      bindOptionalDouble(stmt, 14, event.timezoneOffsetHours);
      bindOptionalDouble(stmt, 15, event.dstOffsetHours);
      sqlite3_bind_int64(stmt, 16, event.birthEventId);
    });
}

static domain::BirthEvent rowToBirthEvent(sqlite3_stmt* stmt) {
  domain::BirthEvent e;
  e.birthEventId = sqlite3_column_int64(stmt, 0);
  e.personId = sqlite3_column_int64(stmt, 1);
  e.birthDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
    e.birthTime = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
  }
  auto accStr = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
  if (accStr == "exact") e.timeAccuracy = domain::TimeAccuracy::Exact;
  else if (accStr == "approximate") e.timeAccuracy = domain::TimeAccuracy::Approximate;
  else e.timeAccuracy = domain::TimeAccuracy::Unknown;
  e.noonDefaultApplied = sqlite3_column_int(stmt, 5) != 0;
  e.housesEnabled = sqlite3_column_int(stmt, 6) != 0;
  e.sourceDescription = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
  e.confidenceScore = sqlite3_column_double(stmt, 8);
  e.cityInput = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
  if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
    e.locationId = sqlite3_column_int64(stmt, 10);
  }
  if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) {
    e.timezoneName = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11)));
  }
  e.dstMode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
  e.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
  e.updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
  if (sqlite3_column_type(stmt, 15) != SQLITE_NULL)
    e.latitudeDeg = sqlite3_column_double(stmt, 15);
  if (sqlite3_column_type(stmt, 16) != SQLITE_NULL)
    e.longitudeDeg = sqlite3_column_double(stmt, 16);
  if (sqlite3_column_type(stmt, 17) != SQLITE_NULL)
    e.timezoneOffsetHours = sqlite3_column_double(stmt, 17);
  if (sqlite3_column_type(stmt, 18) != SQLITE_NULL)
    e.dstOffsetHours = sqlite3_column_double(stmt, 18);
  return e;
}

static const char* kBirthEventSelectColumns =
  "birth_event_id, person_id, birth_date, birth_time, time_accuracy, "
  "noon_default_applied, houses_enabled, source_description, confidence_score, "
  "city_input, location_id, timezone_name, dst_mode, created_at, updated_at, "
  "latitude_deg, longitude_deg, timezone_offset_hours, dst_offset_hours";

std::optional<domain::BirthEvent> BirthEventRepository::findById(std::int64_t birthEventId) const {
  std::optional<domain::BirthEvent> result;
  std::string sql = std::string("SELECT ") + kBirthEventSelectColumns +
                    " FROM birth_events WHERE birth_event_id=?;";
  m_db.queryWithParams(sql.c_str(),
    [&](sqlite3_stmt* stmt) { sqlite3_bind_int64(stmt, 1, birthEventId); },
    [&](sqlite3_stmt* stmt) { result = rowToBirthEvent(stmt); });
  return result;
}

std::vector<domain::BirthEvent> BirthEventRepository::findByPersonId(std::int64_t personId) const {
  std::vector<domain::BirthEvent> results;
  std::string sql = std::string("SELECT ") + kBirthEventSelectColumns +
                    " FROM birth_events WHERE person_id=?;";
  m_db.queryWithParams(sql.c_str(),
    [&](sqlite3_stmt* stmt) { sqlite3_bind_int64(stmt, 1, personId); },
    [&](sqlite3_stmt* stmt) { results.push_back(rowToBirthEvent(stmt)); });
  return results;
}

}  // namespace asteria::data
