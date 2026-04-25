#include <iostream>
#include <filesystem>

#include "core/natal_chart_service.h"
#include "engine/fake_chart_engine.h"
#include "render/natal_chart_layout.h"
#include "render/svg_serializer.h"
#include "render/theme_presets.h"
#include "data/sqlite_database.h"
#include "data/migration_runner.h"

int main() {
  namespace fs = std::filesystem;
  using namespace asteria;

  const fs::path current = fs::current_path();
  const fs::path dbPath = current / "data" / "asteria.db";
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

  domain::ChartRequest request;
  request.chartRequestId = 1;
  request.chartType = domain::ChartType::Natal;
  request.defaultHouseSystem = "Placidus";
  request.zodiacMode = "Tropical";
  request.includeHouses = true;

  engine::FakeChartEngine fakeEngine;
  core::NatalChartService service(fakeEngine);
  auto result = service.compute(request);
  if (!result.ok()) {
    std::cerr << "Chart computation failed: " << result.error().message << std::endl;
    return 1;
  }

  const auto scene = render::buildNatalChartScene(result.value(), render::textbookLight());
  const fs::path svgPath = current / "sample_natal.svg";
  if (!render::writeSvgFile(scene, svgPath.string())) {
    std::cerr << "Failed to write SVG output." << std::endl;
    return 1;
  }

  std::cout << "Asteria bootstrap build ran successfully.\n";
  std::cout << "Sample SVG written to: " << svgPath.string() << '\n';
  std::cout << "Database initialized at: " << dbPath.string() << '\n';
  return 0;
}
