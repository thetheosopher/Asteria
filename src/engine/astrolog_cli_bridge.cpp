#include "astrolog_cli_bridge.h"
#include <array>
#include <cstdio>
#include <sstream>
#include <filesystem>

namespace asteria::engine {

using asteria::core::Result;

AstrologCliBridge::AstrologCliBridge(const std::string& exePath) : m_exePath(exePath) {}

std::string AstrologCliBridge::executeAstrolog(const std::string& args) const {
  if (!std::filesystem::exists(m_exePath)) return "";

  std::string cmd = "\"" + m_exePath + "\" " + args + " 2>&1";

#ifdef _WIN32
  FILE* pipe = _popen(cmd.c_str(), "r");
#else
  FILE* pipe = popen(cmd.c_str(), "r");
#endif
  if (!pipe) return "";

  std::string output;
  std::array<char, 4096> buffer;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }

#ifdef _WIN32
  _pclose(pipe);
#else
  pclose(pipe);
#endif
  return output;
}

std::string AstrologCliBridge::getEngineVersion() const {
  // Astrolog -Hc outputs version info
  auto output = executeAstrolog("-Hc");
  if (output.empty()) return "astrolog-cli-unavailable";
  // Extract first line which typically contains version
  auto pos = output.find('\n');
  if (pos != std::string::npos) output = output.substr(0, pos);
  return "AstrologCLI/" + output;
}

core::Result<std::vector<domain::LocationResolution>> AstrologCliBridge::resolveLocation(
    const std::string&, const std::string&) const {
  // TODO: Implement using Astrolog's -N switch for atlas lookup
  return Result<std::vector<domain::LocationResolution>>::failure(
      {"not_implemented", "AstrologCliBridge::resolveLocation is not yet implemented."});
}

core::Result<domain::ComputedChart> AstrologCliBridge::computeNatalChart(
    const domain::ChartRequest& request) const {
  if (!std::filesystem::exists(m_exePath)) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "Astrolog executable not found at: " + m_exePath});
  }

  // Build command line for a natal chart computation.
  // -v produces a standard planet listing.
  // For now, use default location/time since ChartRequest holds IDs not raw data.
  // TODO: Resolve birth event data before passing to CLI.
  std::string args = "-v -c " + std::to_string(0); // Placidus house system
  auto output = executeAstrolog(args);

  if (output.empty()) {
    return Result<domain::ComputedChart>::failure(
        {"engine_failure", "Astrolog CLI produced no output."});
  }

  // TODO: Parse the text output into normalized ComputedChart.
  // For now, return a stub chart with the raw output stored in metadata.
  domain::ComputedChart chart;
  chart.chartRequestId = request.chartRequestId;
  chart.engineVersion = getEngineVersion();
  chart.engineMethod = "AstrologCLI::Natal";
  chart.houseSystem = request.defaultHouseSystem;
  chart.zodiacMode = request.zodiacMode;
  chart.chartMetadataJson = std::string(R"({"rawOutput":"see_cachedJsonBlob"})");
  chart.cachedJsonBlob = output;

  return Result<domain::ComputedChart>::success(chart);
}

core::Result<domain::ComputedChart> AstrologCliBridge::computeSynastryChart(
    const domain::ChartRequest&) const {
  return Result<domain::ComputedChart>::failure(
      {"not_implemented", "AstrologCliBridge::computeSynastryChart is not yet implemented."});
}

core::Result<domain::ComputedChart> AstrologCliBridge::computeCompositeChart(
    const domain::ChartRequest&) const {
  return Result<domain::ComputedChart>::failure(
      {"not_implemented", "AstrologCliBridge::computeCompositeChart is not yet implemented."});
}

core::Result<domain::ComputedChart> AstrologCliBridge::computeTransitToNatalChart(
    const domain::ChartRequest&) const {
  return Result<domain::ComputedChart>::failure(
      {"not_implemented", "AstrologCliBridge::computeTransitToNatalChart is not yet implemented."});
}

}  // namespace asteria::engine
