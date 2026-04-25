#include <gtest/gtest.h>

#include "automation/cli_json_input.h"

#include <filesystem>
#include <fstream>

namespace {

class CliJsonInputTest : public ::testing::Test {
 protected:
  std::filesystem::path tempDir;

  void SetUp() override {
    tempDir = std::filesystem::temp_directory_path() / "asteria_cli_json_test";
    std::filesystem::create_directories(tempDir);
  }

  void TearDown() override {
    std::filesystem::remove_all(tempDir);
  }

  std::filesystem::path writeFile(const std::string& name, const std::string& content) const {
    const auto path = tempDir / name;
    std::ofstream stream(path, std::ios::binary);
    stream << content;
    return path;
  }
};

}  // namespace

TEST_F(CliJsonInputTest, LoadsNestedObjectsArraysAndNumberLikeStrings) {
  const auto path = writeFile(
      "timeline.json",
      R"json({
  "subjectName": "Casey Example",
  "natal": {
    "datetime": "1990-01-01T12:00",
    "latitude": 40.0,
    "longitude": -75.0,
    "timezone": -5.0,
    "dst": 0.0
  },
  "years": 5,
  "transitPlanets": ["Jupiter", "Saturn"],
  "orbs": {
    "Conjunction": 1.0,
    "Square": "0.8"
  }
})json");

  auto result = asteria::automation::loadCliJsonFile(path);
  ASSERT_TRUE(result.ok()) << result.error().message;

  const auto& root = result.value();
  const auto* subject = asteria::automation::findCliJsonPath(root, {"subjectName"});
  ASSERT_NE(subject, nullptr);
  EXPECT_EQ(asteria::automation::cliJsonStringValue(*subject).value_or(""), "Casey Example");

  const auto* years = asteria::automation::findCliJsonPath(root, {"years"});
  ASSERT_NE(years, nullptr);
  EXPECT_DOUBLE_EQ(asteria::automation::cliJsonNumberValue(*years).value_or(0.0), 5.0);

  const auto* transitPlanets = asteria::automation::findCliJsonPath(root, {"transitPlanets"});
  ASSERT_NE(transitPlanets, nullptr);
  ASSERT_NE(transitPlanets->asArray(), nullptr);
  ASSERT_EQ(transitPlanets->asArray()->size(), 2u);
  EXPECT_EQ(asteria::automation::cliJsonStringValue(transitPlanets->asArray()->at(0)).value_or(""),
            "Jupiter");

  const auto* squareOrb = asteria::automation::findCliJsonPath(root, {"orbs", "Square"});
  ASSERT_NE(squareOrb, nullptr);
  EXPECT_DOUBLE_EQ(asteria::automation::cliJsonNumberValue(*squareOrb).value_or(0.0), 0.8);
}

TEST_F(CliJsonInputTest, RejectsMalformedJsonWithPosition) {
  const auto path = writeFile("broken.json", R"json({ "primary": { "datetime": "1990-01-01T12:00", } })json");

  auto result = asteria::automation::loadCliJsonFile(path);
  ASSERT_FALSE(result.ok());
  EXPECT_NE(result.error().message.find("broken.json"), std::string::npos);
  EXPECT_NE(result.error().message.find(":1:"), std::string::npos);
}

TEST_F(CliJsonInputTest, RejectsNonObjectRoot) {
  const auto path = writeFile("array.json", R"json([1, 2, 3])json");

  auto result = asteria::automation::loadCliJsonFile(path);
  ASSERT_FALSE(result.ok());
  EXPECT_NE(result.error().message.find("root must be an object"), std::string::npos);
}