#include "app_context.h"
#include <sqlite3.h>

namespace asteria::ui {

std::string AppContext::getSetting(const std::string& key, const std::string& defaultValue) const {
  std::string result = defaultValue;
  database.queryWithParams(
    "SELECT value FROM app_settings WHERE key=?;",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    },
    [&](sqlite3_stmt* stmt) {
      if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      }
    });
  return result;
}

void AppContext::setSetting(const std::string& key, const std::string& value) {
  database.executeWithParams(
    "INSERT INTO app_settings (key, value) VALUES (?, ?) "
    "ON CONFLICT(key) DO UPDATE SET value=excluded.value;",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    });
}

}  // namespace asteria::ui
