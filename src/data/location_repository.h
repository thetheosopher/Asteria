#pragma once
#include <vector>
#include <optional>
#include "domain/location_resolution.h"

namespace asteria::data {

class SQLiteDatabase;

class LocationRepository {
 public:
  explicit LocationRepository(SQLiteDatabase& db);

  bool insert(domain::LocationResolution& loc);
  std::optional<domain::LocationResolution> findById(std::int64_t locationId) const;
  std::vector<domain::LocationResolution> search(const std::string& query) const;

 private:
  SQLiteDatabase& m_db;
};

}  // namespace asteria::data
