#include "astrolog_cli_bridge.h"

namespace asteria::engine {

using asteria::core::Result;

core::Result<std::vector<domain::LocationResolution>> AstrologCliBridge::resolveLocation(
    const std::string&, const std::string&) const {
  return Result<std::vector<domain::LocationResolution>>::failure(
      {"not_implemented", "AstrologCliBridge::resolveLocation is not implemented yet."});
}

core::Result<domain::ComputedChart> AstrologCliBridge::computeNatalChart(const domain::ChartRequest&) const {
  return Result<domain::ComputedChart>::failure(
      {"not_implemented", "AstrologCliBridge::computeNatalChart is not implemented yet."});
}

core::Result<domain::ComputedChart> AstrologCliBridge::computeSynastryChart(const domain::ChartRequest&) const {
  return Result<domain::ComputedChart>::failure(
      {"not_implemented", "AstrologCliBridge::computeSynastryChart is not implemented yet."});
}

core::Result<domain::ComputedChart> AstrologCliBridge::computeCompositeChart(const domain::ChartRequest&) const {
  return Result<domain::ComputedChart>::failure(
      {"not_implemented", "AstrologCliBridge::computeCompositeChart is not implemented yet."});
}

core::Result<domain::ComputedChart> AstrologCliBridge::computeTransitToNatalChart(const domain::ChartRequest&) const {
  return Result<domain::ComputedChart>::failure(
      {"not_implemented", "AstrologCliBridge::computeTransitToNatalChart is not implemented yet."});
}

std::string AstrologCliBridge::getEngineVersion() const { return "astrolog-cli-todo"; }

}  // namespace asteria::engine
