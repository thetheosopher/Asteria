#pragma once
#include <vector>
#include <optional>
#include "domain/chart_theme.h"

namespace asteria::data {

class SQLiteDatabase;

class ThemeRepository {
 public:
  explicit ThemeRepository(SQLiteDatabase& db);

  bool insert(domain::ChartTheme& theme);
  bool update(const domain::ChartTheme& theme);
  std::optional<domain::ChartTheme> findById(std::int64_t themeId) const;
  std::optional<domain::ChartTheme> findByName(const std::string& name) const;
  std::vector<domain::ChartTheme> findAll() const;

 private:
  SQLiteDatabase& m_db;
};

}  // namespace asteria::data
