#include "migration_runner.h"
#include "sqlite_database.h"
#include <sqlite3.h>

namespace asteria::data {

static const std::vector<Migration> kMigrations = {
  {1, "Initial schema",
   R"(
CREATE TABLE IF NOT EXISTS people (
  person_id       INTEGER PRIMARY KEY AUTOINCREMENT,
  full_name       TEXT NOT NULL,
  display_name    TEXT NOT NULL DEFAULT '',
  gender          TEXT NOT NULL DEFAULT '',
  tags            TEXT NOT NULL DEFAULT '[]',
  notes           TEXT NOT NULL DEFAULT '',
  created_at      TEXT NOT NULL DEFAULT (datetime('now')),
  updated_at      TEXT NOT NULL DEFAULT (datetime('now')),
  archived_at     TEXT
);

CREATE TABLE IF NOT EXISTS locations (
  location_id             INTEGER PRIMARY KEY AUTOINCREMENT,
  query_text              TEXT NOT NULL,
  display_name            TEXT NOT NULL,
  country                 TEXT NOT NULL DEFAULT '',
  region                  TEXT NOT NULL DEFAULT '',
  latitude                REAL NOT NULL,
  longitude               REAL NOT NULL,
  timezone_name           TEXT NOT NULL,
  timezone_offset_rule_ref TEXT NOT NULL DEFAULT '',
  resolution_method       TEXT NOT NULL DEFAULT 'atlas',
  confidence_score        REAL NOT NULL DEFAULT 0.0,
  provenance_note         TEXT NOT NULL DEFAULT '',
  map_pin_precision_meters REAL,
  created_at              TEXT NOT NULL DEFAULT (datetime('now')),
  updated_at              TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS birth_events (
  birth_event_id      INTEGER PRIMARY KEY AUTOINCREMENT,
  person_id           INTEGER NOT NULL REFERENCES people(person_id),
  birth_date          TEXT NOT NULL,
  birth_time          TEXT,
  time_accuracy       TEXT NOT NULL DEFAULT 'unknown',
  noon_default_applied INTEGER NOT NULL DEFAULT 0,
  houses_enabled      INTEGER NOT NULL DEFAULT 1,
  source_description  TEXT NOT NULL DEFAULT '',
  confidence_score    REAL NOT NULL DEFAULT 0.0,
  city_input          TEXT NOT NULL DEFAULT '',
  location_id         INTEGER REFERENCES locations(location_id),
  timezone_name       TEXT,
  dst_mode            TEXT NOT NULL DEFAULT '',
  created_at          TEXT NOT NULL DEFAULT (datetime('now')),
  updated_at          TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS chart_requests (
  chart_request_id        INTEGER PRIMARY KEY AUTOINCREMENT,
  primary_birth_event_id  INTEGER NOT NULL REFERENCES birth_events(birth_event_id),
  secondary_birth_event_id INTEGER REFERENCES birth_events(birth_event_id),
  chart_type              TEXT NOT NULL DEFAULT 'natal',
  request_time_context    TEXT,
  default_house_system    TEXT NOT NULL DEFAULT 'Placidus',
  zodiac_mode             TEXT NOT NULL DEFAULT 'Tropical',
  unknown_time_policy     TEXT NOT NULL DEFAULT 'unknown_time_no_houses',
  include_houses          INTEGER NOT NULL DEFAULT 1,
  theme_id                INTEGER,
  created_at              TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS computed_charts (
  computed_chart_id   INTEGER PRIMARY KEY AUTOINCREMENT,
  chart_request_id    INTEGER NOT NULL REFERENCES chart_requests(chart_request_id),
  engine_version      TEXT NOT NULL DEFAULT '',
  engine_method       TEXT NOT NULL DEFAULT '',
  canonical_hash      TEXT NOT NULL DEFAULT '',
  house_system        TEXT NOT NULL DEFAULT '',
  zodiac_mode         TEXT NOT NULL DEFAULT '',
  uncertainty_flags   TEXT NOT NULL DEFAULT '[]',
  chart_metadata_json TEXT NOT NULL DEFAULT '{}',
  cached_json_blob    TEXT NOT NULL DEFAULT '{}',
  created_at          TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS themes (
  theme_id            INTEGER PRIMARY KEY AUTOINCREMENT,
  theme_name          TEXT NOT NULL UNIQUE,
  style_family        TEXT NOT NULL DEFAULT 'textbook',
  glyph_set           TEXT NOT NULL DEFAULT '',
  line_weight_profile TEXT NOT NULL DEFAULT '',
  aspect_palette      TEXT NOT NULL DEFAULT '',
  typography_profile  TEXT NOT NULL DEFAULT '',
  background_mode     TEXT NOT NULL DEFAULT '',
  created_at          TEXT NOT NULL DEFAULT (datetime('now')),
  updated_at          TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS interpretation_notes (
  interpretation_note_id      INTEGER PRIMARY KEY AUTOINCREMENT,
  computed_chart_id           INTEGER NOT NULL REFERENCES computed_charts(computed_chart_id),
  source_type                 TEXT NOT NULL DEFAULT 'built_in',
  prompt_version              TEXT,
  certainty_guardrails_applied INTEGER NOT NULL DEFAULT 0,
  body_markdown               TEXT NOT NULL DEFAULT '',
  created_at                  TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS export_artifacts (
  export_artifact_id  INTEGER PRIMARY KEY AUTOINCREMENT,
  computed_chart_id   INTEGER NOT NULL REFERENCES computed_charts(computed_chart_id),
  export_type         TEXT NOT NULL DEFAULT 'svg',
  file_path           TEXT NOT NULL DEFAULT '',
  width_px            INTEGER,
  height_px           INTEGER,
  dpi                 INTEGER,
  theme_snapshot_json TEXT NOT NULL DEFAULT '{}',
  exported_at         TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS app_settings (
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL DEFAULT ''
);
)"},
  {2, "Add coordinate columns to birth_events for direct lat/lon/tz entry",
   R"(
ALTER TABLE birth_events ADD COLUMN latitude_deg REAL;
ALTER TABLE birth_events ADD COLUMN longitude_deg REAL;
ALTER TABLE birth_events ADD COLUMN timezone_offset_hours REAL;
ALTER TABLE birth_events ADD COLUMN dst_offset_hours REAL;
)"}
};

MigrationRunner::MigrationRunner(SQLiteDatabase& db) : m_db(db) {}

const std::vector<Migration>& MigrationRunner::allMigrations() {
  return kMigrations;
}

bool MigrationRunner::ensureVersionTable() {
  return m_db.execute(
    "CREATE TABLE IF NOT EXISTS schema_version ("
    "  version INTEGER NOT NULL,"
    "  description TEXT NOT NULL DEFAULT '',"
    "  applied_at TEXT NOT NULL DEFAULT (datetime('now'))"
    ");");
}

int MigrationRunner::readCurrentVersion() const {
  int version = 0;
  m_db.query("SELECT COALESCE(MAX(version), 0) FROM schema_version;",
    [&](sqlite3_stmt* stmt) {
      version = sqlite3_column_int(stmt, 0);
    });
  return version;
}

int MigrationRunner::currentVersion() const {
  return readCurrentVersion();
}

bool MigrationRunner::applyMigration(const Migration& migration) {
  if (!m_db.execute("BEGIN TRANSACTION;")) return false;
  if (!m_db.execute(migration.sql)) {
    m_db.execute("ROLLBACK;");
    return false;
  }
  bool ok = m_db.executeWithParams(
    "INSERT INTO schema_version (version, description) VALUES (?, ?);",
    [&](sqlite3_stmt* stmt) {
      sqlite3_bind_int(stmt, 1, migration.version);
      sqlite3_bind_text(stmt, 2, migration.description.c_str(), -1, SQLITE_TRANSIENT);
    });
  if (!ok) {
    m_db.execute("ROLLBACK;");
    return false;
  }
  return m_db.execute("COMMIT;");
}

bool MigrationRunner::runMigrations() {
  if (!ensureVersionTable()) return false;
  int current = readCurrentVersion();
  for (const auto& m : allMigrations()) {
    if (m.version > current) {
      if (!applyMigration(m)) return false;
    }
  }
  return true;
}

}  // namespace asteria::data
