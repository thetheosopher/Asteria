#include "astrolog_embedded_engine.h"

namespace asteria::engine {

using asteria::core::Error;
using asteria::core::Result;

core::Result<std::vector<domain::LocationResolution>> AstrologEmbeddedEngine::resolveLocation(
    const std::string&, const std::string&) const {
  return Result<std::vector<domain::LocationResolution>>::failure(
      {"not_implemented", "AstrologEmbeddedEngine::resolveLocation is not implemented yet."});
}

core::Result<domain::ComputedChart> AstrologEmbeddedEngine::computeNatalChart(const domain::ChartRequest&) const {
  return Result<domain::ComputedChart>::failure(
      {"not_implemented", "AstrologEmbeddedEngine::computeNatalChart is not implemented yet."});
}

core::Result<domain::ComputedChart> AstrologEmbeddedEngine::computeSynastryChart(const domain::ChartRequest&) const {
  return Result<domain::ComputedChart>::failure(
      {"not_implemented", "AstrologEmbeddedEngine::computeSynastryChart is not implemented yet."});
}

core::Result<domain::ComputedChart> AstrologEmbeddedEngine::computeCompositeChart(const domain::ChartRequest&) const {
  return Result<domain::ComputedChart>::failure(
      {"not_implemented", "AstrologEmbeddedEngine::computeCompositeChart is not implemented yet."});
}

core::Result<domain::ComputedChart> AstrologEmbeddedEngine::computeTransitToNatalChart(const domain::ChartRequest&) const {
  return Result<domain::ComputedChart>::failure(
      {"not_implemented", "AstrologEmbeddedEngine::computeTransitToNatalChart is not implemented yet."});
}

std::string AstrologEmbeddedEngine::getEngineVersion() const { return "astrolog-embedded-todo"; }

}  // namespace asteria::engine
