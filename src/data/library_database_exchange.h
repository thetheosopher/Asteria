#pragma once

#include "core/result.h"

#include <filesystem>

namespace asteria::data {

class SQLiteDatabase;

struct LibraryExchangeStats {
  int peopleProcessed = 0;
  int birthEventsProcessed = 0;
  int peopleInserted = 0;
  int peopleUpdated = 0;
  int birthEventsInserted = 0;
  int birthEventsUpdated = 0;
};

class LibraryDatabaseExchange {
 public:
  explicit LibraryDatabaseExchange(SQLiteDatabase& db);

  core::Result<LibraryExchangeStats> exportToFile(const std::filesystem::path& path) const;
  core::Result<LibraryExchangeStats> importFromFile(const std::filesystem::path& path);
  core::Result<LibraryExchangeStats> mergeFromFile(const std::filesystem::path& path);

 private:
  SQLiteDatabase& m_db;
};

}  // namespace asteria::data