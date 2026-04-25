#pragma once
#include "ichart_engine.h"
#include <string>

namespace asteria::engine {

/// CLI bridge that invokes the Astrolog executable and parses its text output.
/// Used for regression comparison and fixture generation, not primary computation.
class AstrologCliBridge final : public IChartEngine {
 public:
  /// Construct with the path to the astrolog.exe executable.
  explicit AstrologCliBridge(const std::string& exePath);

  core::Result<std::vector<domain::LocationResolution>> resolveLocation(
      const std::string& query,
      const std::string& optionalDateTimeContext = "") const override;
  core::Result<domain::ComputedChart> computeNatalChart(const domain::ChartRequest& request) const override;
  core::Result<domain::ComputedChart> computeSynastryChart(const domain::ChartRequest& request) const override;
  core::Result<domain::ComputedChart> computeCompositeChart(const domain::ChartRequest& request) const override;
  core::Result<domain::ComputedChart> computeTransitToNatalChart(const domain::ChartRequest& request) const override;
  std::string getEngineVersion() const override;

 private:
  std::string m_exePath;

  /// Execute astrolog.exe with given arguments and capture stdout.
  std::string executeAstrolog(const std::string& args) const;
};

}  // namespace asteria::engine
