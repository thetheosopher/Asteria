#pragma once
#include <string>
#include <functional>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace asteria::data {

class SQLiteDatabase {
 public:
  explicit SQLiteDatabase(std::string path);
  ~SQLiteDatabase();

  SQLiteDatabase(const SQLiteDatabase&) = delete;
  SQLiteDatabase& operator=(const SQLiteDatabase&) = delete;

  bool open();
  void close();
  bool isOpen() const;
  const std::string& path() const;

  bool execute(const std::string& sql);
  bool executeWithParams(const std::string& sql,
                         const std::function<void(sqlite3_stmt*)>& bindFn);

  using RowCallback = std::function<void(sqlite3_stmt*)>;
  bool query(const std::string& sql, const RowCallback& rowCb);
  bool queryWithParams(const std::string& sql,
                       const std::function<void(sqlite3_stmt*)>& bindFn,
                       const RowCallback& rowCb);

  int lastInsertRowId() const;
  std::string lastError() const;

  sqlite3* handle() const;

 private:
  std::string m_path;
  sqlite3* m_db = nullptr;
};

}  // namespace asteria::data
