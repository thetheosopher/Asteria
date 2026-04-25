#pragma once
#include <vector>
#include <optional>
#include "domain/person.h"

namespace asteria::data {

class SQLiteDatabase;

class PersonRepository {
 public:
  explicit PersonRepository(SQLiteDatabase& db);

  bool insert(domain::Person& person);
  bool update(const domain::Person& person);
  std::optional<domain::Person> findById(std::int64_t personId) const;
  std::vector<domain::Person> findAll() const;
  bool archive(std::int64_t personId);

 private:
  SQLiteDatabase& m_db;
};

}  // namespace asteria::data
