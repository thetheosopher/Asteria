#include <gtest/gtest.h>
#include "automation/cli_dispatcher.h"
#include "engine/fake_chart_engine.h"
#include <filesystem>

class CliDispatcherTest : public ::testing::Test {
 protected:
  asteria::engine::FakeChartEngine engine;
  std::filesystem::path tempDir;

  void SetUp() override {
    tempDir = std::filesystem::temp_directory_path() / "asteria_cli_test";
    std::filesystem::create_directories(tempDir);
  }

  void TearDown() override {
    std::filesystem::remove_all(tempDir);
  }
};

TEST_F(CliDispatcherTest, ComputeNatalReturnsJson) {
  asteria::automation::CliDispatcher dispatcher(engine);
  auto result = dispatcher.computeNatal(1);
  EXPECT_TRUE(result.success);
  EXPECT_FALSE(result.outputJson.empty());
  EXPECT_NE(result.outputJson.find("engineVersion"), std::string::npos);
  EXPECT_NE(result.outputJson.find("planets"), std::string::npos);
}

TEST_F(CliDispatcherTest, ComputeSynastryReturnsJson) {
  asteria::automation::CliDispatcher dispatcher(engine);
  auto result = dispatcher.computeSynastry(1, 2);
  EXPECT_TRUE(result.success);
  EXPECT_NE(result.outputJson.find("Synastry"), std::string::npos);
}

TEST_F(CliDispatcherTest, CompositeReturnsJson) {
  asteria::automation::CliDispatcher dispatcher(engine);
  auto result = dispatcher.computeComposite(1, 2);
  EXPECT_TRUE(result.success);
  EXPECT_NE(result.outputJson.find("Composite"), std::string::npos);
}

TEST_F(CliDispatcherTest, TransitRequiresDateTime) {
  asteria::automation::CliDispatcher dispatcher(engine);
  // With transit time
  auto result = dispatcher.computeTransitToNatal(1, "2026-04-24T12:00:00Z");
  EXPECT_TRUE(result.success);
  EXPECT_NE(result.outputJson.find("TransitToNatal"), std::string::npos);
}

TEST_F(CliDispatcherTest, ResolveLocationReturnsJson) {
  asteria::automation::CliDispatcher dispatcher(engine);
  auto result = dispatcher.resolveLocation("Denver, CO");
  EXPECT_TRUE(result.success);
  EXPECT_NE(result.outputJson.find("Denver"), std::string::npos);
  EXPECT_NE(result.outputJson.find("latitude"), std::string::npos);
}

TEST_F(CliDispatcherTest, ExportSvgCreatesFile) {
  asteria::automation::CliDispatcher dispatcher(engine);
  auto path = (tempDir / "cli_export.svg").string();
  auto result = dispatcher.exportSvg(1, path);
  EXPECT_TRUE(result.success);
  EXPECT_TRUE(std::filesystem::exists(path));
}

TEST_F(CliDispatcherTest, ExportPngCreatesFile) {
  asteria::automation::CliDispatcher dispatcher(engine);
  auto path = (tempDir / "cli_export.png").string();
  auto result = dispatcher.exportPng(1, path, 800, 800, 150);
  EXPECT_TRUE(result.success);
  EXPECT_TRUE(std::filesystem::exists(path));
}
