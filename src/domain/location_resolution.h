#pragma once
#include <string>
#include <optional>
#include <cstdint>

namespace asteria::domain {

struct LocationResolution {
  std::int64_t locationId = 0;
  std::string queryText;
  std::string displayName;
  std::string country;
  std::string region;
  double latitude = 0.0;
  double longitude = 0.0;
  std::string timezoneName;
  std::string timezoneOffsetRuleRef;
  std::string resolutionMethod;
  double confidenceScore = 0.0;
  std::string provenanceNote;
  std::optional<double> mapPinPrecisionMeters;
  std::string createdAt;
  std::string updatedAt;
};

}  // namespace asteria::domain
