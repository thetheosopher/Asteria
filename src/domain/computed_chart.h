#pragma once
#include <string>
#include <vector>
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
};

}  // namespace asteria::domain
