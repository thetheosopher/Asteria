#pragma once
#include <string>
#include <vector>
#include <functional>

namespace asteria::data {

class SQLiteDatabase;

struct Migration {
  int version;
  std::string description;
  std::string sql;
};

class MigrationRunner {
 public:
  explicit MigrationRunner(SQLiteDatabase& db);

  bool runMigrations();
  int currentVersion() const;

 private:
  bool ensureVersionTable();
  int readCurrentVersion() const;
  bool applyMigration(const Migration& migration);

  SQLiteDatabase& m_db;
  static const std::vector<Migration>& allMigrations();
};

}  // namespace asteria::data
