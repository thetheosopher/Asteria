#include "sqlite_database.h"
#include <filesystem>
#include <sqlite3.h>

namespace asteria::data {

SQLiteDatabase::SQLiteDatabase(std::string path) : m_path(std::move(path)) {}

SQLiteDatabase::~SQLiteDatabase() { close(); }

bool SQLiteDatabase::open() {
  if (m_db) return true;
  std::filesystem::create_directories(std::filesystem::path(m_path).parent_path());
  int rc = sqlite3_open(m_path.c_str(), &m_db);
  if (rc != SQLITE_OK) {
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
    return false;
  }
  execute("PRAGMA journal_mode=WAL;");
  execute("PRAGMA foreign_keys=ON;");
  return true;
}

void SQLiteDatabase::close() {
  if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
}

bool SQLiteDatabase::isOpen() const { return m_db != nullptr; }
const std::string& SQLiteDatabase::path() const { return m_path; }

bool SQLiteDatabase::execute(const std::string& sql) {
  if (!m_db) return false;
  char* errMsg = nullptr;
  int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
  if (errMsg) sqlite3_free(errMsg);
  return rc == SQLITE_OK;
}

bool SQLiteDatabase::executeWithParams(const std::string& sql,
                                        const std::function<void(sqlite3_stmt*)>& bindFn) {
  if (!m_db) return false;
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) return false;
  if (bindFn) bindFn(stmt);
  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

bool SQLiteDatabase::query(const std::string& sql, const RowCallback& rowCb) {
  return queryWithParams(sql, nullptr, rowCb);
}

bool SQLiteDatabase::queryWithParams(const std::string& sql,
                                      const std::function<void(sqlite3_stmt*)>& bindFn,
                                      const RowCallback& rowCb) {
  if (!m_db) return false;
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) return false;
  if (bindFn) bindFn(stmt);
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    if (rowCb) rowCb(stmt);
  }
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

int SQLiteDatabase::lastInsertRowId() const {
  if (!m_db) return 0;
  return static_cast<int>(sqlite3_last_insert_rowid(m_db));
}

std::string SQLiteDatabase::lastError() const {
  if (!m_db) return "database not open";
  return sqlite3_errmsg(m_db);
}

sqlite3* SQLiteDatabase::handle() const { return m_db; }

}  // namespace asteria::data
