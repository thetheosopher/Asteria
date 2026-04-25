#include <iostream>
#include <filesystem>

#include "data/sqlite_database.h"
#include "data/migration_runner.h"
#include "engine/astrolog_embedded_engine.h"
#include "ui/app_window.h"

int main() {
  namespace fs = std::filesystem;
  using namespace asteria;

  // Initialize database
  const fs::path current = fs::current_path();
  const fs::path dbPath = current / "data" / "asteria.db";
  fs::create_directories(dbPath.parent_path());
  data::SQLiteDatabase database(dbPath.string());
  if (!database.open()) {
    std::cerr << "Failed to open database at: " << dbPath << std::endl;
    return 1;
  }

  data::MigrationRunner migrations(database);
  if (!migrations.runMigrations()) {
    std::cerr << "Failed to run database migrations: " << database.lastError() << std::endl;
    return 1;
  }
  std::cout << "Database schema version: " << migrations.currentVersion() << '\n';
  std::cout << "Database initialized at: " << dbPath.string() << '\n';

  // Initialize computation engine
  const fs::path astrologDataPath = current / "astrolog_data";
  engine::AstrologEmbeddedEngine chartEngine(astrologDataPath.string());
  std::cout << "Engine version: " << chartEngine.getEngineVersion() << '\n';

  // Launch GUI
  return ui::runApplication(database, chartEngine);
}
