#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "types.h"

namespace asteria::domain {

struct ComputedChart {
  std::int64_t computedChartId = 0;
  std::int64_t chartRequestId = 0;
  std::string engineVersion;
  std::string engineMethod;
  std::string canonicalHash;
  std::string houseSystem;
  std::string zodiacMode;
  std::vector<std::string> uncertaintyFlags;
  std::string chartMetadataJson;
  std::string cachedJsonBlob;
  std::string createdAt;

  std::vector<PlanetPosition> planets;
  std::vector<HouseCusp> houseCusps;
  std::vector<Aspect> aspects;

  /// Secondary chart for synastry/composite (if applicable).
  /// Owned via shared_ptr to allow ComputedChart to remain copyable while
  /// keeping a self-referential nested type.
  std::shared_ptr<ComputedChart> secondaryChart;

  /// Cross-chart aspects for synastry (planet from primary -> planet from secondary).
  std::vector<Aspect> interAspects;
};

}  // namespace asteria::domain
