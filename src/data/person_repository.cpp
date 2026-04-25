#include "person_repository.h"
#include "sqlite_database.h"
#include <sqlite3.h>

namespace asteria::data {

PersonRepository::PersonRepository(SQLiteDatabase& db) : m_db(db) {}

bool PersonRepository::insert(domain::Person& person) {
  bool ok = m_db.executeWithParams(
    "INSERT INTO people (full_name, display_name, gender, tags, notes) VALUES (?, ?, ?, ?, ?);",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_text(stmt, 1, person.fullName.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, person.displayName.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, person.gender.c_str(), -1, SQLITE_TRANSIENT);
      std::string tagsJson = "[";
      for (size_t i = 0; i < person.tags.size(); ++i) {
        if (i > 0) tagsJson += ",";
        tagsJson += "\"" + person.tags[i] + "\"";
      }
      tagsJson += "]";
      sqlite3_bind_text(stmt, 4, tagsJson.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 5, person.notes.c_str(), -1, SQLITE_TRANSIENT);
    });
  if (ok) person.personId = m_db.lastInsertRowId();
  return ok;
}

bool PersonRepository::update(const domain::Person& person) {
  return m_db.executeWithParams(
    "UPDATE people SET full_name=?, display_name=?, gender=?, notes=?, "
    "updated_at=datetime('now') WHERE person_id=?;",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_text(stmt, 1, person.fullName.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, person.displayName.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, person.gender.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, person.notes.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(stmt, 5, person.personId);
    });
}

static domain::Person rowToPerson(sqlite3_stmt* stmt) {
  domain::Person p;
  p.personId = sqlite3_column_int64(stmt, 0);
  p.fullName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  p.displayName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  p.gender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
  // tags stored as JSON array - simple parse
  p.notes = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
  p.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
  p.updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
  if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
    p.archivedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
  }
  return p;
}

std::optional<domain::Person> PersonRepository::findById(std::int64_t personId) const {
  std::optional<domain::Person> result;
  m_db.queryWithParams(
    "SELECT person_id, full_name, display_name, gender, tags, notes, "
    "created_at, updated_at, archived_at FROM people WHERE person_id=?;",
    [&](sqlite3_stmt* stmt) { sqlite3_bind_int64(stmt, 1, personId); },
    [&](sqlite3_stmt* stmt) { result = rowToPerson(stmt); });
  return result;
}

std::vector<domain::Person> PersonRepository::findAll() const {
  std::vector<domain::Person> results;
  m_db.query(
    "SELECT person_id, full_name, display_name, gender, tags, notes, "
    "created_at, updated_at, archived_at FROM people WHERE archived_at IS NULL "
    "ORDER BY full_name;",
    [&](sqlite3_stmt* stmt) { results.push_back(rowToPerson(stmt)); });
  return results;
}

bool PersonRepository::archive(std::int64_t personId) {
  return m_db.executeWithParams(
    "UPDATE people SET archived_at=datetime('now') WHERE person_id=?;",
    [&](sqlite3_stmt* stmt) { sqlite3_bind_int64(stmt, 1, personId); });
}

}  // namespace asteria::data
