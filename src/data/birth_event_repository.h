#pragma once
#include <vector>
#include <optional>
#include "domain/birth_event.h"

namespace asteria::data {

class SQLiteDatabase;

class BirthEventRepository {
 public:
  explicit BirthEventRepository(SQLiteDatabase& db);

  bool insert(domain::BirthEvent& event);
  bool update(const domain::BirthEvent& event);
  std::optional<domain::BirthEvent> findById(std::int64_t birthEventId) const;
  std::vector<domain::BirthEvent> findByPersonId(std::int64_t personId) const;

 private:
  SQLiteDatabase& m_db;
};

}  // namespace asteria::data
