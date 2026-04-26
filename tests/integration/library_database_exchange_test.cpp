#include <gtest/gtest.h>

#include "data/birth_event_repository.h"
#include "data/library_database_exchange.h"
#include "data/migration_runner.h"
#include "data/person_repository.h"
#include "data/sqlite_database.h"

#include <sqlite3.h>

#include <filesystem>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace {

class LibraryDatabaseExchangeTest : public ::testing::Test {
 protected:
  fs::path tempDir;

  void SetUp() override {
    tempDir = fs::temp_directory_path() / "asteria_library_exchange_test";
    fs::remove_all(tempDir);
    fs::create_directories(tempDir);
  }

  void TearDown() override {
    fs::remove_all(tempDir);
  }

  std::unique_ptr<asteria::data::SQLiteDatabase> createDatabase(const std::string& name) {
    fs::path dbPath = tempDir / name / "test.db";
    fs::create_directories(dbPath.parent_path());
    auto db = std::make_unique<asteria::data::SQLiteDatabase>(dbPath.string());
    EXPECT_TRUE(db->open());
    asteria::data::MigrationRunner runner(*db);
    EXPECT_TRUE(runner.runMigrations());
    return db;
  }

  asteria::domain::Person insertPerson(asteria::data::SQLiteDatabase& db,
                                       const std::string& fullName,
                                       const std::string& notes = "") {
    asteria::data::PersonRepository repo(db);
    asteria::domain::Person person;
    person.fullName = fullName;
    person.displayName = fullName;
    person.notes = notes;
    EXPECT_TRUE(repo.insert(person));
    return person;
  }

  asteria::domain::BirthEvent insertBirthEvent(asteria::data::SQLiteDatabase& db,
                                               std::int64_t personId,
                                               const std::string& birthDate,
                                               const std::string& cityInput,
                                               const std::optional<std::string>& birthTime = std::nullopt) {
    asteria::data::BirthEventRepository repo(db);
    asteria::domain::BirthEvent event;
    event.personId = personId;
    event.birthDate = birthDate;
    event.birthTime = birthTime;
    event.cityInput = cityInput;
    event.timeAccuracy = birthTime ? asteria::domain::TimeAccuracy::Exact
                                   : asteria::domain::TimeAccuracy::Unknown;
    event.timezoneName = std::string("America/Denver");
    event.latitudeDeg = 39.7392;
    event.longitudeDeg = -104.9903;
    event.timezoneOffsetHours = -7.0;
    event.dstOffsetHours = 1.0;
    EXPECT_TRUE(repo.insert(event));
    return event;
  }

  int countRows(asteria::data::SQLiteDatabase& db, const std::string& tableName) {
    int count = 0;
    db.query("SELECT COUNT(*) FROM " + tableName + ";",
             [&](sqlite3_stmt* stmt) { count = sqlite3_column_int(stmt, 0); });
    return count;
  }

  std::string readTagsJson(asteria::data::SQLiteDatabase& db, const std::string& fullName) {
    std::string tagsJson;
    db.queryWithParams(
        "SELECT tags FROM people WHERE full_name=?;",
        [&](sqlite3_stmt* stmt) {
          sqlite3_bind_text(stmt, 1, fullName.c_str(), -1, SQLITE_TRANSIENT);
        },
        [&](sqlite3_stmt* stmt) {
          tagsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        });
    return tagsJson;
  }
};

}  // namespace

TEST_F(LibraryDatabaseExchangeTest, ExportAndImportRoundTripReplacesExistingLibraryRows) {
  auto db = createDatabase("roundtrip");
  auto originalPerson = insertPerson(*db, "Alice Example", "Original notes");
  insertBirthEvent(*db, originalPerson.personId, "1990-01-02", "Denver, CO", "08:15");
  EXPECT_TRUE(db->executeWithParams(
      "UPDATE people SET tags=? WHERE person_id=?;",
      [&](sqlite3_stmt* stmt) {
        const char* tags = "[\"mentor\",\"scientist\"]";
        sqlite3_bind_text(stmt, 1, tags, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, originalPerson.personId);
      }));
  EXPECT_TRUE(db->executeWithParams(
      "INSERT INTO chart_requests (primary_birth_event_id) VALUES (?);",
      [&](sqlite3_stmt* stmt) {
        sqlite3_bind_int64(stmt, 1, 1);
      }));

  asteria::data::LibraryDatabaseExchange exchange(*db);
  const fs::path exportPath = tempDir / "roundtrip" / "library.json";
  auto exportResult = exchange.exportToFile(exportPath);
  ASSERT_TRUE(exportResult.ok()) << exportResult.error().message;
  EXPECT_EQ(exportResult.value().peopleProcessed, 1);
  EXPECT_EQ(exportResult.value().birthEventsProcessed, 1);

  auto extraPerson = insertPerson(*db, "Bob Extra", "Should be removed");
  insertBirthEvent(*db, extraPerson.personId, "1985-05-06", "Boulder, CO", std::nullopt);
  EXPECT_EQ(countRows(*db, "people"), 2);
  EXPECT_EQ(countRows(*db, "chart_requests"), 1);

  auto importResult = exchange.importFromFile(exportPath);
  ASSERT_TRUE(importResult.ok()) << importResult.error().message;
  EXPECT_EQ(importResult.value().peopleInserted, 1);
  EXPECT_EQ(importResult.value().birthEventsInserted, 1);
  EXPECT_EQ(countRows(*db, "people"), 1);
  EXPECT_EQ(countRows(*db, "birth_events"), 1);
  EXPECT_EQ(countRows(*db, "chart_requests"), 0);

  asteria::data::PersonRepository personRepo(*db);
  auto people = personRepo.findAll();
  ASSERT_EQ(people.size(), 1u);
  EXPECT_EQ(people.front().fullName, "Alice Example");
  EXPECT_EQ(people.front().notes, "Original notes");

  asteria::data::BirthEventRepository birthRepo(*db);
  auto events = birthRepo.findByPersonId(people.front().personId);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events.front().birthDate, "1990-01-02");
  ASSERT_TRUE(events.front().birthTime.has_value());
  EXPECT_EQ(events.front().birthTime.value(), "08:15");
  EXPECT_EQ(readTagsJson(*db, "Alice Example"), "[\"mentor\",\"scientist\"]");
}

TEST_F(LibraryDatabaseExchangeTest, MergeUpdatesMatchingPersonAndAddsNewRows) {
  auto targetDb = createDatabase("target");
  auto aliceTarget = insertPerson(*targetDb, "Alice Example", "Old notes");
  insertBirthEvent(*targetDb, aliceTarget.personId, "1990-01-02", "Denver, CO", "08:15");

  auto sourceDb = createDatabase("source");
  auto aliceSource = insertPerson(*sourceDb, "Alice Example", "Updated notes");
  insertBirthEvent(*sourceDb, aliceSource.personId, "1990-01-02", "Denver, CO", "09:45");
  auto bobSource = insertPerson(*sourceDb, "Bob Example", "New person");
  insertBirthEvent(*sourceDb, bobSource.personId, "1985-05-06", "Boulder, CO", std::nullopt);

  asteria::data::LibraryDatabaseExchange sourceExchange(*sourceDb);
  const fs::path mergePath = tempDir / "merge.json";
  auto sourceExport = sourceExchange.exportToFile(mergePath);
  ASSERT_TRUE(sourceExport.ok()) << sourceExport.error().message;

  asteria::data::LibraryDatabaseExchange targetExchange(*targetDb);
  auto mergeResult = targetExchange.mergeFromFile(mergePath);
  ASSERT_TRUE(mergeResult.ok()) << mergeResult.error().message;
  EXPECT_EQ(mergeResult.value().peopleInserted, 1);
  EXPECT_EQ(mergeResult.value().peopleUpdated, 1);
  EXPECT_EQ(mergeResult.value().birthEventsInserted, 1);
  EXPECT_EQ(mergeResult.value().birthEventsUpdated, 1);

  asteria::data::PersonRepository personRepo(*targetDb);
  auto people = personRepo.findAll();
  ASSERT_EQ(people.size(), 2u);

  auto aliceIt = std::find_if(people.begin(), people.end(), [](const asteria::domain::Person& person) {
    return person.fullName == "Alice Example";
  });
  ASSERT_NE(aliceIt, people.end());
  EXPECT_EQ(aliceIt->notes, "Updated notes");

  asteria::data::BirthEventRepository birthRepo(*targetDb);
  auto aliceEvents = birthRepo.findByPersonId(aliceIt->personId);
  ASSERT_EQ(aliceEvents.size(), 1u);
  ASSERT_TRUE(aliceEvents.front().birthTime.has_value());
  EXPECT_EQ(aliceEvents.front().birthTime.value(), "09:45");

  auto bobIt = std::find_if(people.begin(), people.end(), [](const asteria::domain::Person& person) {
    return person.fullName == "Bob Example";
  });
  ASSERT_NE(bobIt, people.end());
  auto bobEvents = birthRepo.findByPersonId(bobIt->personId);
  ASSERT_EQ(bobEvents.size(), 1u);
  EXPECT_EQ(bobEvents.front().birthDate, "1985-05-06");
}