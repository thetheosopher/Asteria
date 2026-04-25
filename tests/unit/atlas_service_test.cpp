#include <gtest/gtest.h>
#include "util/atlas_service.h"
#include <string>

// ASTERIA_SOURCE_DIR is defined by CMake (project root path).
static const std::string kAtlasPath =
    std::string(ASTERIA_SOURCE_DIR) + "/assets/atlasbig.as";
static const std::string kTzPath =
    std::string(ASTERIA_SOURCE_DIR) + "/third_party/astrolog/timezone.as";

using asteria::util::AtlasService;
using asteria::util::TimezoneResult;

class AtlasServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(atlas_.loadAtlas(kAtlasPath));
    ASSERT_TRUE(atlas_.loadTimezones(kTzPath));
  }
  AtlasService atlas_;
};

// ---------- Atlas loading ----------

TEST_F(AtlasServiceTest, LoadsNonTrivialEntryCount) {
  EXPECT_GT(atlas_.entryCount(), 200000u);
}

// ---------- Search ----------

TEST_F(AtlasServiceTest, SearchFindsNewYorkCity) {
  auto results = atlas_.search("New York City", 10);
  ASSERT_FALSE(results.empty());
  // First result should be an exact match
  bool found = false;
  for (auto* r : results) {
    if (r->name == "New York City" && r->regionCode == "ny") {
      found = true;
      EXPECT_NEAR(r->latitude,  40.7143, 0.01);
      EXPECT_NEAR(r->longitude, -74.006, 0.01);  // +W in atlas → -W (east is negative? No, +E)
      // Atlas longitude is negated: file has 74.006 (+W), Asteria stores as -74.006... wait.
      // Actually atlas stores +W and we negate to get +E. 74.006W → -74.006 in +E convention. Correct.
      break;
    }
  }
  EXPECT_TRUE(found) << "New York City (ny) not found in atlas results";
}

TEST_F(AtlasServiceTest, SearchIsCaseInsensitive) {
  auto results = atlas_.search("london", 10);
  ASSERT_FALSE(results.empty());
  bool foundGB = false;
  for (auto* r : results) {
    if (r->name == "London" && r->regionCode == "GB") {
      foundGB = true;
      break;
    }
  }
  EXPECT_TRUE(foundGB);
}

TEST_F(AtlasServiceTest, SearchReturnsEmptyForGibberish) {
  auto results = atlas_.search("zzxxqqww", 10);
  EXPECT_TRUE(results.empty());
}

TEST_F(AtlasServiceTest, SearchRespectsMaxResults) {
  auto results = atlas_.search("San", 5);
  EXPECT_LE(static_cast<int>(results.size()), 5);
}

TEST_F(AtlasServiceTest, SearchPrioritizesStartsWithMatch) {
  auto results = atlas_.search("Paris", 10);
  ASSERT_FALSE(results.empty());
  // "Paris" should come before "East Paris" etc.
  EXPECT_EQ(results[0]->name.substr(0, 5), "Paris");
}

// ---------- Timezone resolution ----------

TEST_F(AtlasServiceTest, NewYorkStandardTimeOffset) {
  // January = EST (no DST), UTC-5 → stored as +5 in Asteria convention
  auto tz = atlas_.resolveTimezone("America/New_York", 2024, 1, 15);
  EXPECT_NEAR(tz.utcOffsetHours, 5.0, 0.01);
  EXPECT_NEAR(tz.dstOffsetHours, 0.0, 0.01);
}

TEST_F(AtlasServiceTest, NewYorkDaylightTimeOffset) {
  // July = EDT (DST active), UTC-4 → base 5, DST +1
  auto tz = atlas_.resolveTimezone("America/New_York", 2024, 7, 15);
  EXPECT_NEAR(tz.utcOffsetHours, 5.0, 0.01);
  EXPECT_NEAR(tz.dstOffsetHours, 1.0, 0.01);
}

TEST_F(AtlasServiceTest, LondonWinterOffset) {
  // December = GMT, UTC+0 → offset 0, DST 0
  auto tz = atlas_.resolveTimezone("Europe/London", 2024, 12, 15);
  EXPECT_NEAR(tz.utcOffsetHours, 0.0, 0.01);
  EXPECT_NEAR(tz.dstOffsetHours, 0.0, 0.01);
}

TEST_F(AtlasServiceTest, LondonSummerOffset) {
  // July = BST (DST), UTC+1 → offset 0, DST +1
  auto tz = atlas_.resolveTimezone("Europe/London", 2024, 7, 15);
  EXPECT_NEAR(tz.utcOffsetHours, 0.0, 0.01);
  EXPECT_NEAR(tz.dstOffsetHours, 1.0, 0.01);
}

TEST_F(AtlasServiceTest, TokyoNoDst) {
  // Japan doesn't use DST. UTC+9 → offset -9
  auto tz = atlas_.resolveTimezone("Asia/Tokyo", 2024, 7, 15);
  EXPECT_NEAR(tz.utcOffsetHours, -9.0, 0.01);
  EXPECT_NEAR(tz.dstOffsetHours, 0.0, 0.01);
}

TEST_F(AtlasServiceTest, SydneyDstInJanuary) {
  // January = Australian summer, AEDT (DST active). UTC+11 → base -10, DST +1
  auto tz = atlas_.resolveTimezone("Australia/Sydney", 2024, 1, 15);
  EXPECT_NEAR(tz.utcOffsetHours, -10.0, 0.01);
  EXPECT_NEAR(tz.dstOffsetHours, 1.0, 0.01);
}

TEST_F(AtlasServiceTest, SydneyNoDstInJuly) {
  // July = Australian winter, AEST. UTC+10 → base -10, DST 0
  auto tz = atlas_.resolveTimezone("Australia/Sydney", 2024, 7, 15);
  EXPECT_NEAR(tz.utcOffsetHours, -10.0, 0.01);
  EXPECT_NEAR(tz.dstOffsetHours, 0.0, 0.01);
}

TEST_F(AtlasServiceTest, HistoricalTimezoneResolution) {
  // US DST was year-round in 1942-1945 (War Time)
  // In July 1943, New York should have DST=1
  auto tz = atlas_.resolveTimezone("America/New_York", 1943, 7, 15);
  EXPECT_NEAR(tz.utcOffsetHours, 5.0, 0.01);
  EXPECT_NEAR(tz.dstOffsetHours, 1.0, 0.01);
}

TEST_F(AtlasServiceTest, UnknownTimezoneReturnsZeros) {
  auto tz = atlas_.resolveTimezone("Fake/Nowhere", 2024, 1, 1);
  EXPECT_NEAR(tz.utcOffsetHours, 0.0, 0.01);
  EXPECT_NEAR(tz.dstOffsetHours, 0.0, 0.01);
}

TEST_F(AtlasServiceTest, AliasResolution) {
  // America/Kralendijk → America/Curacao (from Part 3)
  auto tz = atlas_.resolveTimezone("America/Kralendijk", 2024, 7, 15);
  auto tzTarget = atlas_.resolveTimezone("America/Curacao", 2024, 7, 15);
  EXPECT_NEAR(tz.utcOffsetHours, tzTarget.utcOffsetHours, 0.01);
  EXPECT_NEAR(tz.dstOffsetHours, tzTarget.dstOffsetHours, 0.01);
}

// ---------- Integration: search + resolve ----------

TEST_F(AtlasServiceTest, SearchAndResolveLosAngeles) {
  auto results = atlas_.search("Los Angeles", 20);
  ASSERT_FALSE(results.empty());
  // Find the California entry specifically
  const asteria::util::AtlasEntry* la = nullptr;
  for (auto* r : results) {
    if (r->regionCode == "ca" && r->name == "Los Angeles") { la = r; break; }
  }
  ASSERT_NE(la, nullptr) << "Los Angeles, CA not found in results";

  // July = PDT: base 8, DST +1
  auto tz = atlas_.resolveTimezone(la->timezoneName, 2024, 7, 4);
  EXPECT_NEAR(tz.utcOffsetHours, 8.0, 0.01);
  EXPECT_NEAR(tz.dstOffsetHours, 1.0, 0.01);
}
