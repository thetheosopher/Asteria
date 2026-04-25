#include <gtest/gtest.h>
#include "engine/astrolog_adapter.h"
#include "engine/astrolog_embedded_engine.h"
#include <filesystem>
#include <cmath>

// Helper to find the Astrolog data directory relative to test executable
static std::string findAstrologDataPath() {
  namespace fs = std::filesystem;
  // The CMakeLists.txt copies data files to <target_dir>/astrolog/
  // Try several paths relative to the test executable
  auto exeDir = fs::current_path();
  std::vector<fs::path> candidates = {
    exeDir / "astrolog",
    exeDir / ".." / "astrolog",
    exeDir / ".." / ".." / "astrolog",
    // Also try the source directory
    fs::path(ASTERIA_SOURCE_DIR) / "third_party" / "astrolog",
  };
  for (const auto& p : candidates) {
    if (fs::exists(p / "astrolog.as")) return p.string();
  }
  // Fallback to source tree
  return (fs::path(ASTERIA_SOURCE_DIR) / "third_party" / "astrolog").string();
}

class AstrologAdapterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dataPath = findAstrologDataPath();
    ASSERT_TRUE(asteria::engine::AstrologAdapter::initialize(dataPath))
        << "Failed to initialize Astrolog from: " << dataPath;
  }

  std::string dataPath;
};

TEST_F(AstrologAdapterTest, EngineVersionContainsAstrolog) {
  auto version = asteria::engine::AstrologAdapter::engineVersion();
  EXPECT_NE(version.find("Astrolog"), std::string::npos);
  EXPECT_NE(version.find("7.80"), std::string::npos);
}

TEST_F(AstrologAdapterTest, MapHouseSystemPlacidus) {
  EXPECT_EQ(asteria::engine::AstrologAdapter::mapHouseSystem("Placidus"), 0);
}

TEST_F(AstrologAdapterTest, MapHouseSystemKoch) {
  EXPECT_EQ(asteria::engine::AstrologAdapter::mapHouseSystem("Koch"), 1);
}

TEST_F(AstrologAdapterTest, MapHouseSystemWhole) {
  EXPECT_EQ(asteria::engine::AstrologAdapter::mapHouseSystem("Whole"), 14);
}

TEST_F(AstrologAdapterTest, SignNameMapping) {
  EXPECT_EQ(asteria::engine::AstrologAdapter::signName(1), "Aries");
  EXPECT_EQ(asteria::engine::AstrologAdapter::signName(4), "Cancer");
  EXPECT_EQ(asteria::engine::AstrologAdapter::signName(10), "Capricorn");
  EXPECT_EQ(asteria::engine::AstrologAdapter::signName(12), "Pisces");
}

TEST_F(AstrologAdapterTest, ComputeChartProducesResults) {
  asteria::engine::AstrologAdapter::ChartInput input;
  // Use a known chart: Jan 1, 2000, 12:00 noon UTC, Greenwich
  input.year = 2000;
  input.month = 1;
  input.day = 1;
  input.timeHours = 12.0;
  input.dstHours = 0.0;
  input.timezoneHours = 0.0;  // UTC
  input.longitudeDeg = 0.0;   // Greenwich
  input.latitudeDeg = 51.5;   // London lat
  input.houseSystem = 0;      // Placidus
  input.sidereal = false;

  asteria::engine::AstrologAdapter::ChartOutput output;
  ASSERT_TRUE(asteria::engine::AstrologAdapter::computeChart(input, output));

  // Should have planet positions
  EXPECT_GE(output.planets.size(), 10u);
  EXPECT_EQ(output.houseCusps.size(), 12u);

  // Sun should be in Capricorn (~280°) on Jan 1
  bool foundSun = false;
  for (const auto& p : output.planets) {
    if (p.name == "Sun") {
      foundSun = true;
      EXPECT_NEAR(p.longitude, 280.0, 5.0)  // ~280° = Capricorn
          << "Sun longitude on Jan 1 2000 should be ~280° (Capricorn)";
      EXPECT_EQ(p.sign, "Capricorn");
      EXPECT_FALSE(p.retrograde);  // Sun never retrogrades
      break;
    }
  }
  EXPECT_TRUE(foundSun) << "Sun not found in computed planets";

  // House cusps should span 0-360
  for (const auto& h : output.houseCusps) {
    EXPECT_GE(h.longitude, 0.0);
    EXPECT_LT(h.longitude, 360.0);
    EXPECT_GE(h.houseNumber, 1);
    EXPECT_LE(h.houseNumber, 12);
  }
}

TEST_F(AstrologAdapterTest, ComputeChartSunInAries) {
  asteria::engine::AstrologAdapter::ChartInput input;
  // April 15, 2000, noon UTC, London — Sun should be in Aries
  input.year = 2000;
  input.month = 4;
  input.day = 15;
  input.timeHours = 12.0;
  input.timezoneHours = 0.0;
  input.longitudeDeg = 0.0;
  input.latitudeDeg = 51.5;

  asteria::engine::AstrologAdapter::ChartOutput output;
  ASSERT_TRUE(asteria::engine::AstrologAdapter::computeChart(input, output));

  for (const auto& p : output.planets) {
    if (p.name == "Sun") {
      EXPECT_EQ(p.sign, "Aries");
      EXPECT_GE(p.longitude, 0.0);
      EXPECT_LT(p.longitude, 30.0);
      break;
    }
  }
}

TEST_F(AstrologAdapterTest, DifferentHouseSystemsProduceDifferentCusps) {
  asteria::engine::AstrologAdapter::ChartInput input;
  input.year = 2000; input.month = 1; input.day = 1;
  input.timeHours = 12.0;
  input.longitudeDeg = 0.0; input.latitudeDeg = 51.5;

  // Placidus
  input.houseSystem = 0;
  asteria::engine::AstrologAdapter::ChartOutput placidus;
  ASSERT_TRUE(asteria::engine::AstrologAdapter::computeChart(input, placidus));

  // Whole sign
  input.houseSystem = 14;
  asteria::engine::AstrologAdapter::ChartOutput whole;
  ASSERT_TRUE(asteria::engine::AstrologAdapter::computeChart(input, whole));

  // At least some cusps should differ between Placidus and Whole Sign
  bool anyDifferent = false;
  for (size_t i = 0; i < 12; ++i) {
    if (std::fabs(placidus.houseCusps[i].longitude - whole.houseCusps[i].longitude) > 0.01) {
      anyDifferent = true;
      break;
    }
  }
  EXPECT_TRUE(anyDifferent) << "Placidus and Whole Sign should produce different cusps";
}

TEST_F(AstrologAdapterTest, ChartComputationIsDeterministic) {
  asteria::engine::AstrologAdapter::ChartInput input;
  input.year = 1990; input.month = 6; input.day = 15;
  input.timeHours = 14.5;
  input.longitudeDeg = -73.9857; input.latitudeDeg = 40.7484; // NYC

  asteria::engine::AstrologAdapter::ChartOutput out1, out2;
  ASSERT_TRUE(asteria::engine::AstrologAdapter::computeChart(input, out1));
  ASSERT_TRUE(asteria::engine::AstrologAdapter::computeChart(input, out2));

  ASSERT_EQ(out1.planets.size(), out2.planets.size());
  for (size_t i = 0; i < out1.planets.size(); ++i) {
    EXPECT_DOUBLE_EQ(out1.planets[i].longitude, out2.planets[i].longitude)
        << "Planet " << out1.planets[i].name << " position not deterministic";
  }
}

// --- Embedded Engine Integration Tests ---

class AstrologEmbeddedTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dataPath = findAstrologDataPath();
  }
  std::string dataPath;
};

TEST_F(AstrologEmbeddedTest, NatalChartComputeSuccess) {
  asteria::engine::AstrologEmbeddedEngine engine(dataPath);
  asteria::domain::ChartRequest request;
  request.chartRequestId = 42;
  request.chartType = asteria::domain::ChartType::Natal;
  request.defaultHouseSystem = "Placidus";
  request.zodiacMode = "Tropical";
  request.includeHouses = true;

  auto result = engine.computeNatalChart(request);
  ASSERT_TRUE(result.ok()) << "Compute failed: " << result.error().message;

  const auto& chart = result.value();
  EXPECT_EQ(chart.chartRequestId, 42);
  EXPECT_NE(chart.engineVersion.find("Astrolog"), std::string::npos);
  EXPECT_NE(chart.engineMethod.find("Natal"), std::string::npos);
  EXPECT_GE(chart.planets.size(), 10u);
  EXPECT_EQ(chart.houseCusps.size(), 12u);
  EXPECT_GE(chart.aspects.size(), 1u);
}

TEST_F(AstrologEmbeddedTest, NatalChartVersionString) {
  asteria::engine::AstrologEmbeddedEngine engine(dataPath);
  auto version = engine.getEngineVersion();
  EXPECT_NE(version.find("Astrolog"), std::string::npos);
}

TEST_F(AstrologEmbeddedTest, NoHousesPolicyClears) {
  asteria::engine::AstrologEmbeddedEngine engine(dataPath);
  asteria::domain::ChartRequest request;
  request.chartRequestId = 1;
  request.includeHouses = false;

  auto result = engine.computeNatalChart(request);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().houseCusps.empty());
  bool hasNoHousesFlag = false;
  for (const auto& f : result.value().uncertaintyFlags) {
    if (f == "no_houses") hasNoHousesFlag = true;
  }
  EXPECT_TRUE(hasNoHousesFlag);
}

TEST_F(AstrologEmbeddedTest, NoonDefaultWarningEmitted) {
  asteria::engine::AstrologEmbeddedEngine engine(dataPath);
  asteria::domain::ChartRequest request;
  request.chartRequestId = 1;
  request.unknownTimePolicy = "noon_default_with_warning";

  auto result = engine.computeNatalChart(request);
  ASSERT_TRUE(result.ok());
  bool hasNoonFlag = false;
  for (const auto& f : result.value().uncertaintyFlags) {
    if (f == "noon_default_applied") hasNoonFlag = true;
  }
  EXPECT_TRUE(hasNoonFlag);
}

TEST_F(AstrologEmbeddedTest, AspectComputation) {
  asteria::engine::AstrologEmbeddedEngine engine(dataPath);
  asteria::domain::ChartRequest request;
  request.chartRequestId = 1;
  request.includeHouses = true;

  auto result = engine.computeNatalChart(request);
  ASSERT_TRUE(result.ok());

  // Should detect at least some aspects
  EXPECT_GE(result.value().aspects.size(), 1u);

  // Each aspect should have valid structure
  for (const auto& asp : result.value().aspects) {
    EXPECT_FALSE(asp.objectA.empty());
    EXPECT_FALSE(asp.objectB.empty());
    EXPECT_FALSE(asp.aspectType.empty());
    EXPECT_GE(asp.orbDegrees, 0.0);
  }
}
