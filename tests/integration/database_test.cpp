#include <gtest/gtest.h>
#include <filesystem>
#include "data/sqlite_database.h"
#include "data/migration_runner.h"
#include "data/person_repository.h"
#include "data/birth_event_repository.h"
#include "data/location_repository.h"
#include "data/chart_repository.h"
#include "data/theme_repository.h"
#include "domain/export_artifact.h"

namespace fs = std::filesystem;

namespace {

constexpr int kExpectedSchemaVersion = 3;

}  // namespace

class DatabaseTest : public ::testing::Test {
 protected:
  fs::path dbPath;
  std::unique_ptr<asteria::data::SQLiteDatabase> db;

  void SetUp() override {
    dbPath = fs::temp_directory_path() / "asteria_test_db" / "test.db";
    fs::remove_all(dbPath.parent_path());
    db = std::make_unique<asteria::data::SQLiteDatabase>(dbPath.string());
    ASSERT_TRUE(db->open());
    asteria::data::MigrationRunner runner(*db);
    ASSERT_TRUE(runner.runMigrations());
  }

  void TearDown() override {
    db->close();
    fs::remove_all(dbPath.parent_path());
  }
};

TEST_F(DatabaseTest, MigrationRunnerCreatesSchema) {
  asteria::data::MigrationRunner runner(*db);
  EXPECT_EQ(runner.currentVersion(), kExpectedSchemaVersion);
}

TEST_F(DatabaseTest, MigrationIsIdempotent) {
  asteria::data::MigrationRunner runner(*db);
  ASSERT_TRUE(runner.runMigrations());
  EXPECT_EQ(runner.currentVersion(), kExpectedSchemaVersion);
}

TEST_F(DatabaseTest, ExportArtifactInsertRecordsMetadata) {
  asteria::data::PersonRepository personRepo(*db);
  asteria::data::ChartRepository repo(*db);

  asteria::domain::Person person;
  person.fullName = "Export Parent";
  person.displayName = "Export Parent";
  ASSERT_TRUE(personRepo.insert(person));

  asteria::data::BirthEventRepository eventRepo(*db);
  asteria::domain::BirthEvent event;
  event.personId = person.personId;
  event.birthDate = "1990-01-01";
  event.birthTime = std::string("12:00");
  event.timeAccuracy = asteria::domain::TimeAccuracy::Exact;
  ASSERT_TRUE(eventRepo.insert(event));

  asteria::domain::ChartRequest request;
  request.primaryBirthEventId = event.birthEventId;
  ASSERT_TRUE(repo.insertRequest(request));

  asteria::domain::ComputedChart chart;
  chart.chartRequestId = request.chartRequestId;
  chart.engineVersion = "test";
  chart.engineMethod = "test";
  chart.canonicalHash = "export-artifact-test";
  chart.houseSystem = "Placidus";
  chart.zodiacMode = "Tropical";
  ASSERT_TRUE(repo.insertComputed(chart));

  asteria::domain::ExportArtifact artifact;
  artifact.computedChartId = chart.computedChartId;
  artifact.exportType = asteria::domain::ExportType::Pdf;
  artifact.filePath = "report.pdf";
  artifact.widthPx = 612;
  artifact.heightPx = 792;
  artifact.dpi = 300;
  artifact.themeSnapshotJson = R"({"theme":"Textbook Light"})";
  artifact.exportMetadataJson = R"({"report_type":"ai_interpretation"})";

  ASSERT_TRUE(repo.insertExportArtifact(artifact));
  EXPECT_GT(artifact.exportArtifactId, 0);
}

TEST_F(DatabaseTest, PersonInsertAndFindById) {
  asteria::data::PersonRepository repo(*db);
  asteria::domain::Person person;
  person.fullName = "Albert Einstein";
  person.displayName = "Einstein";
  person.gender = "M";
  person.notes = "Physicist";
  ASSERT_TRUE(repo.insert(person));
  EXPECT_GT(person.personId, 0);

  auto found = repo.findById(person.personId);
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->fullName, "Albert Einstein");
  EXPECT_EQ(found->displayName, "Einstein");
}

TEST_F(DatabaseTest, PersonFindAll) {
  asteria::data::PersonRepository repo(*db);
  asteria::domain::Person p1;
  p1.fullName = "Person A";
  p1.displayName = "A";
  asteria::domain::Person p2;
  p2.fullName = "Person B";
  p2.displayName = "B";
  ASSERT_TRUE(repo.insert(p1));
  ASSERT_TRUE(repo.insert(p2));
  auto all = repo.findAll();
  EXPECT_EQ(all.size(), 2u);
}

TEST_F(DatabaseTest, PersonArchive) {
  asteria::data::PersonRepository repo(*db);
  asteria::domain::Person person;
  person.fullName = "To Archive";
  person.displayName = "Archive";
  ASSERT_TRUE(repo.insert(person));
  ASSERT_TRUE(repo.archive(person.personId));
  auto all = repo.findAll();
  EXPECT_EQ(all.size(), 0u);  // archived people excluded from findAll
}

TEST_F(DatabaseTest, PersonUpdate) {
  asteria::data::PersonRepository repo(*db);
  asteria::domain::Person person;
  person.fullName = "Original Name";
  person.displayName = "Orig";
  ASSERT_TRUE(repo.insert(person));
  person.fullName = "Updated Name";
  ASSERT_TRUE(repo.update(person));
  auto found = repo.findById(person.personId);
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->fullName, "Updated Name");
}

TEST_F(DatabaseTest, BirthEventInsertAndFind) {
  asteria::data::PersonRepository personRepo(*db);
  asteria::domain::Person person;
  person.fullName = "Test Subject";
  person.displayName = "Test";
  ASSERT_TRUE(personRepo.insert(person));

  asteria::data::BirthEventRepository repo(*db);
  asteria::domain::BirthEvent event;
  event.personId = person.personId;
  event.birthDate = "1990-06-15";
  event.birthTime = "14:30";
  event.timeAccuracy = asteria::domain::TimeAccuracy::Exact;
  event.housesEnabled = true;
  event.cityInput = "Denver, CO";
  event.confidenceScore = 0.95;
  ASSERT_TRUE(repo.insert(event));
  EXPECT_GT(event.birthEventId, 0);

  auto found = repo.findById(event.birthEventId);
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->birthDate, "1990-06-15");
  EXPECT_EQ(found->birthTime.value(), "14:30");
  EXPECT_EQ(found->timeAccuracy, asteria::domain::TimeAccuracy::Exact);
}

TEST_F(DatabaseTest, BirthEventUnknownTime) {
  asteria::data::PersonRepository personRepo(*db);
  asteria::domain::Person person;
  person.fullName = "Unknown Time Person";
  person.displayName = "UTP";
  ASSERT_TRUE(personRepo.insert(person));

  asteria::data::BirthEventRepository repo(*db);
  asteria::domain::BirthEvent event;
  event.personId = person.personId;
  event.birthDate = "1985-03-22";
  event.timeAccuracy = asteria::domain::TimeAccuracy::Unknown;
  event.noonDefaultApplied = true;
  event.housesEnabled = false;
  ASSERT_TRUE(repo.insert(event));

  auto found = repo.findById(event.birthEventId);
  ASSERT_TRUE(found.has_value());
  EXPECT_FALSE(found->birthTime.has_value());
  EXPECT_EQ(found->timeAccuracy, asteria::domain::TimeAccuracy::Unknown);
  EXPECT_TRUE(found->noonDefaultApplied);
  EXPECT_FALSE(found->housesEnabled);
}

TEST_F(DatabaseTest, LocationInsertAndSearch) {
  asteria::data::LocationRepository repo(*db);
  asteria::domain::LocationResolution loc;
  loc.queryText = "Denver, CO";
  loc.displayName = "Denver, CO, USA";
  loc.country = "USA";
  loc.region = "CO";
  loc.latitude = 39.7392;
  loc.longitude = -104.9903;
  loc.timezoneName = "America/Denver";
  loc.resolutionMethod = "atlas";
  loc.confidenceScore = 0.99;
  ASSERT_TRUE(repo.insert(loc));
  EXPECT_GT(loc.locationId, 0);

  auto results = repo.search("Denver");
  EXPECT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].displayName, "Denver, CO, USA");
}

TEST_F(DatabaseTest, ThemeInsertAndFind) {
  asteria::data::ThemeRepository repo(*db);
  asteria::domain::ChartTheme theme;
  theme.themeName = "Test Theme";
  theme.styleFamily = "textbook";
  theme.backgroundMode = "light";
  ASSERT_TRUE(repo.insert(theme));
  EXPECT_GT(theme.themeId, 0);

  auto found = repo.findByName("Test Theme");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->styleFamily, "textbook");

  auto all = repo.findAll();
  EXPECT_EQ(all.size(), 1u);
}
