#include <gtest/gtest.h>

#include "automation/cli_dispatcher.h"
#include "engine/ichart_engine.h"

#include <chrono>
#include <filesystem>

namespace {

class CliTestEngine final : public asteria::engine::IChartEngine {
 public:
  asteria::core::Result<std::vector<asteria::domain::LocationResolution>> resolveLocation(
      const std::string& query,
      const std::string&) const override {
    asteria::domain::LocationResolution location;
    location.queryText = query;
    location.displayName = "Denver, CO, USA";
    location.latitude = 39.7392;
    location.longitude = -104.9903;
    location.timezoneName = "America/Denver";
    location.confidenceScore = 0.99;
    return asteria::core::Result<std::vector<asteria::domain::LocationResolution>>::success({location});
  }

  asteria::core::Result<asteria::domain::ComputedChart> computeNatalChart(
      const asteria::domain::ChartRequest& request) const override {
    if (request.chartType == asteria::domain::ChartType::TransitToNatal) {
      return asteria::core::Result<asteria::domain::ComputedChart>::success(
          buildTimelineTransitChart(*request.primaryInput, request));
    }
    return asteria::core::Result<asteria::domain::ComputedChart>::success(
        buildBaseChart("CliTest::Natal", request));
  }

  asteria::core::Result<asteria::domain::ComputedChart> computeSynastryChart(
      const asteria::domain::ChartRequest& request) const override {
    auto chart = buildBaseChart("CliTest::Synastry", request);
    chart.secondaryChart = std::make_shared<asteria::domain::ComputedChart>(
        buildSecondaryChart("CliTest::SynastrySecondary", request));
    chart.interAspects = {{"A:Sun", "B:Moon", "Trine", 1.5, std::string("applying")}};
    return asteria::core::Result<asteria::domain::ComputedChart>::success(chart);
  }

  asteria::core::Result<asteria::domain::ComputedChart> computeCompositeChart(
      const asteria::domain::ChartRequest& request) const override {
    auto chart = buildBaseChart("CliTest::Composite", request);
    chart.planets[0].longitudeDegrees = 123.0;
    return asteria::core::Result<asteria::domain::ComputedChart>::success(chart);
  }

  asteria::core::Result<asteria::domain::ComputedChart> computeTransitToNatalChart(
      const asteria::domain::ChartRequest& request) const override {
    auto chart = buildBaseChart("CliTest::TransitToNatal", request);
    chart.secondaryChart = std::make_shared<asteria::domain::ComputedChart>(
        buildSecondaryChart("CliTest::TransitSecondary", request));
    chart.interAspects = {{"N:Sun", "T:Jupiter", "Sextile", 0.4, std::string("separating")}};
    return asteria::core::Result<asteria::domain::ComputedChart>::success(chart);
  }

  std::string getEngineVersion() const override { return "cli-test-engine"; }

 private:
  static asteria::domain::ComputedChart buildBaseChart(
      const std::string& method,
      const asteria::domain::ChartRequest& request) {
    asteria::domain::ComputedChart chart;
    chart.engineVersion = "cli-test-engine";
    chart.engineMethod = method;
    chart.canonicalHash = method + "-hash";
    chart.houseSystem = request.defaultHouseSystem;
    chart.zodiacMode = request.zodiacMode;
    chart.planets = {
        {"Sun", 15.0, 0.0, 0.98, "Aries", 1, false},
        {"Moon", 102.0, 5.0, 13.0, "Cancer", 4, false},
        {"Mercury", 28.0, 1.0, 1.2, "Aries", 1, false},
        {"Venus", 41.0, 0.5, 1.1, "Taurus", 2, false},
        {"Mars", 223.0, -1.2, 0.6, "Scorpio", 8, true},
        {"Jupiter", 75.0, 0.0, 0.2, "Gemini", 3, false},
        {"Saturn", 300.0, 0.0, -0.1, "Aquarius", 11, true},
    };
    for (int house = 1; house <= 12; ++house) {
      chart.houseCusps.push_back({house, (house - 1) * 30.0});
    }
    chart.aspects = {
        {"Sun", "Moon", "Square", 3.0, std::string("applying")},
        {"Moon", "Venus", "Sextile", 1.0, std::string("separating")},
    };
    chart.uncertaintyFlags = {"approximate_time"};
    return chart;
  }

  static asteria::domain::ComputedChart buildSecondaryChart(
      const std::string& method,
      const asteria::domain::ChartRequest& request) {
    auto chart = buildBaseChart(method, request);
    chart.planets[0].objectId = "Sun";
    chart.planets[0].longitudeDegrees = 210.0;
    chart.planets[1].longitudeDegrees = 240.0;
    return chart;
  }

  static asteria::domain::ComputedChart buildTimelineTransitChart(
      const asteria::domain::ResolvedBirthInput& input,
      const asteria::domain::ChartRequest& request) {
    using namespace std::chrono;
    const auto currentDay = sys_days{year{input.year} / month{static_cast<unsigned>(input.month)}
                                     / day{static_cast<unsigned>(input.day)}};
    const auto startDay = sys_days{year{2026} / month{1} / day{1}};
    const double dayOffset = duration_cast<hours>(currentDay - startDay).count() / 24.0
        + input.timeHours / 24.0;

    auto chart = buildBaseChart("CliTest::TimelineTransit", request);
    chart.planets = {
        {"Jupiter", 38.0 + dayOffset, 0.0, 1.0, "Taurus", std::nullopt, false},
        {"Mars", 100.4, 0.0, 0.6, "Cancer", std::nullopt, false},
    };
    chart.houseCusps.clear();
    chart.aspects.clear();
    chart.uncertaintyFlags.clear();
    return chart;
  }
};

class CliDispatcherTest : public ::testing::Test {
 protected:
  CliTestEngine engine;
  std::filesystem::path tempDir;

  void SetUp() override {
    tempDir = std::filesystem::temp_directory_path() / "asteria_cli_test";
    std::filesystem::create_directories(tempDir);
  }

  void TearDown() override {
    std::filesystem::remove_all(tempDir);
  }

  static asteria::automation::CliDispatcher::BirthInputOptions primaryInput() {
    return {"1990-01-01T12:00:00", 40.0, -75.0, -5.0, 0.0};
  }

  static asteria::automation::CliDispatcher::BirthInputOptions secondaryInput() {
    return {"1992-05-12T08:30:00", 34.0, -118.0, -8.0, 0.0};
  }

  static asteria::automation::CliDispatcher::BirthInputOptions transitInput() {
    return {"2026-04-24T12:00:00", 40.0, -75.0, -5.0, 0.0};
  }
};

}  // namespace

TEST_F(CliDispatcherTest, ComputeNatalReturnsJson) {
  asteria::automation::CliDispatcher dispatcher(engine);
  asteria::automation::CliDispatcher::NatalChartOptions options;
  options.primary = primaryInput();

  auto result = dispatcher.computeNatal(options);
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.outputJson.find("CliTest::Natal"), std::string::npos);
  EXPECT_NE(result.outputJson.find("\"planets\""), std::string::npos);
}

TEST_F(CliDispatcherTest, ComputeSynastryIncludesSecondaryChartAndInterAspects) {
  asteria::automation::CliDispatcher dispatcher(engine);
  asteria::automation::CliDispatcher::ComparisonChartOptions options;
  options.primary = primaryInput();
  options.secondary = secondaryInput();

  auto result = dispatcher.computeSynastry(options);
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.outputJson.find("CliTest::Synastry"), std::string::npos);
  EXPECT_NE(result.outputJson.find("\"secondaryChart\""), std::string::npos);
  EXPECT_NE(result.outputJson.find("A:Sun"), std::string::npos);
}

TEST_F(CliDispatcherTest, ComputeCompositeReturnsJson) {
  asteria::automation::CliDispatcher dispatcher(engine);
  asteria::automation::CliDispatcher::ComparisonChartOptions options;
  options.primary = primaryInput();
  options.secondary = secondaryInput();

  auto result = dispatcher.computeComposite(options);
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.outputJson.find("CliTest::Composite"), std::string::npos);
}

TEST_F(CliDispatcherTest, ComputeTransitReturnsJson) {
  asteria::automation::CliDispatcher dispatcher(engine);
  asteria::automation::CliDispatcher::TransitChartOptions options;
  options.primary = primaryInput();
  options.transit = transitInput();

  auto result = dispatcher.computeTransitToNatal(options);
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.outputJson.find("CliTest::TransitToNatal"), std::string::npos);
  EXPECT_NE(result.outputJson.find("N:Sun"), std::string::npos);
}

TEST_F(CliDispatcherTest, ResolveLocationReturnsJson) {
  asteria::automation::CliDispatcher dispatcher(engine);
  auto result = dispatcher.resolveLocation("Denver, CO");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.outputJson.find("Denver"), std::string::npos);
  EXPECT_NE(result.outputJson.find("latitude"), std::string::npos);
}

TEST_F(CliDispatcherTest, ExportSvgCreatesFile) {
  asteria::automation::CliDispatcher dispatcher(engine);
  asteria::automation::CliDispatcher::ExportChartOptions options;
  options.chartType = asteria::automation::CliDispatcher::ExportChartType::Natal;
  options.primary = primaryInput();
  options.outputPath = (tempDir / "cli_export.svg").string();

  auto result = dispatcher.exportSvg(options);
  ASSERT_TRUE(result.success);
  EXPECT_TRUE(std::filesystem::exists(options.outputPath));
}

TEST_F(CliDispatcherTest, ExportPngCreatesFile) {
  asteria::automation::CliDispatcher dispatcher(engine);
  asteria::automation::CliDispatcher::ExportChartOptions options;
  options.chartType = asteria::automation::CliDispatcher::ExportChartType::TransitToNatal;
  options.primary = primaryInput();
  options.transit = transitInput();
  options.outputPath = (tempDir / "cli_export.png").string();
  options.widthPx = 800;
  options.heightPx = 800;
  options.dpi = 150;

  auto result = dispatcher.exportPng(options);
  ASSERT_TRUE(result.success);
  EXPECT_TRUE(std::filesystem::exists(options.outputPath));
}

TEST_F(CliDispatcherTest, ExportPdfReportCreatesFile) {
  asteria::automation::CliDispatcher dispatcher(engine);
  asteria::automation::CliDispatcher::PdfReportOptions options;
  options.chart.chartType = asteria::automation::CliDispatcher::ExportChartType::Natal;
  options.chart.primary = primaryInput();
  options.chart.outputPath = (tempDir / "cli_report.pdf").string();
  options.chart.widthPx = 320;
  options.chart.heightPx = 320;
  options.chart.dpi = 144;
  options.title = "CLI PDF Report";
  options.sourceLabel = "CLI fixture";
  options.interpretationMarkdown = "## AI Interpretation\n\n- Generated note";
  options.reportTemplate = asteria::core::PdfReportTemplate::StudyNotes;
  options.preferVectorChart = true;

  auto result = dispatcher.exportPdfReport(options);
  ASSERT_TRUE(result.success) << result.errorMessage;
  EXPECT_TRUE(std::filesystem::exists(options.chart.outputPath));
  EXPECT_NE(result.outputJson.find("pdf"), std::string::npos);
  EXPECT_NE(result.outputJson.find("Study Notes"), std::string::npos);
}

TEST_F(CliDispatcherTest, GenerateTransitTimelineReturnsMarkdownAndSavesFile) {
  asteria::automation::CliDispatcher dispatcher(engine);
  asteria::automation::CliDispatcher::TransitTimelineOptions options;
  options.subjectName = "Casey Example";
  options.natal = primaryInput();
  options.startDateTime = "2026-01-01T00:00:00";
  options.rangeYears = 0.1;
  options.outputPath = (tempDir / "timeline.md").string();

  auto result = dispatcher.generateTransitTimeline(options);
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.outputJson.find("## Transit-to-Natal Timeline"), std::string::npos);
  EXPECT_NE(result.outputJson.find("Jupiter Sextile natal Sun"), std::string::npos);
  EXPECT_TRUE(std::filesystem::exists(options.outputPath));
}