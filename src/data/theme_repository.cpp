#include "theme_repository.h"
#include "sqlite_database.h"
#include <sqlite3.h>

namespace asteria::data {

ThemeRepository::ThemeRepository(SQLiteDatabase& db) : m_db(db) {}

bool ThemeRepository::insert(domain::ChartTheme& theme) {
  bool ok = m_db.executeWithParams(
    "INSERT INTO themes (theme_name, style_family, glyph_set, line_weight_profile, "
    "aspect_palette, typography_profile, background_mode) VALUES (?, ?, ?, ?, ?, ?, ?);",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_text(stmt, 1, theme.themeName.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, theme.styleFamily.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, theme.glyphSet.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, theme.lineWeightProfile.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 5, theme.aspectPalette.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 6, theme.typographyProfile.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 7, theme.backgroundMode.c_str(), -1, SQLITE_TRANSIENT);
    });
  if (ok) theme.themeId = m_db.lastInsertRowId();
  return ok;
}

bool ThemeRepository::update(const domain::ChartTheme& theme) {
  return m_db.executeWithParams(
    "UPDATE themes SET theme_name=?, style_family=?, glyph_set=?, line_weight_profile=?, "
    "aspect_palette=?, typography_profile=?, background_mode=?, "
    "updated_at=datetime('now') WHERE theme_id=?;",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_text(stmt, 1, theme.themeName.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, theme.styleFamily.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, theme.glyphSet.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, theme.lineWeightProfile.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 5, theme.aspectPalette.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 6, theme.typographyProfile.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 7, theme.backgroundMode.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(stmt, 8, theme.themeId);
    });
}

static domain::ChartTheme rowToTheme(sqlite3_stmt* stmt) {
  domain::ChartTheme t;
  t.themeId = sqlite3_column_int64(stmt, 0);
  t.themeName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  t.styleFamily = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  t.glyphSet = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
  t.lineWeightProfile = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
  t.aspectPalette = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
  t.typographyProfile = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
  t.backgroundMode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
  t.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
  t.updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
  return t;
}

std::optional<domain::ChartTheme> ThemeRepository::findById(std::int64_t themeId) const {
  std::optional<domain::ChartTheme> result;
  m_db.queryWithParams(
    "SELECT theme_id, theme_name, style_family, glyph_set, line_weight_profile, "
    "aspect_palette, typography_profile, background_mode, created_at, updated_at "
    "FROM themes WHERE theme_id=?;",
    [&](sqlite3_stmt* stmt) { sqlite3_bind_int64(stmt, 1, themeId); },
    [&](sqlite3_stmt* stmt) { result = rowToTheme(stmt); });
  return result;
}

std::optional<domain::ChartTheme> ThemeRepository::findByName(const std::string& name) const {
  std::optional<domain::ChartTheme> result;
  m_db.queryWithParams(
    "SELECT theme_id, theme_name, style_family, glyph_set, line_weight_profile, "
    "aspect_palette, typography_profile, background_mode, created_at, updated_at "
    "FROM themes WHERE theme_name=?;",
    [&](sqlite3_stmt* stmt) { sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT); },
    [&](sqlite3_stmt* stmt) { result = rowToTheme(stmt); });
  return result;
}

std::vector<domain::ChartTheme> ThemeRepository::findAll() const {
  std::vector<domain::ChartTheme> results;
  m_db.query(
    "SELECT theme_id, theme_name, style_family, glyph_set, line_weight_profile, "
    "aspect_palette, typography_profile, background_mode, created_at, updated_at "
    "FROM themes ORDER BY theme_name;",
    [&](sqlite3_stmt* stmt) { results.push_back(rowToTheme(stmt)); });
  return results;
}

}  // namespace asteria::data
