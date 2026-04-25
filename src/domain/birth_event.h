#pragma once
#include <string>
#include <optional>
#include <cstdint>
#include "types.h"

namespace asteria::domain {

struct BirthEvent {
  std::int64_t birthEventId = 0;
  std::int64_t personId = 0;
  std::string birthDate;
  std::optional<std::string> birthTime;
  TimeAccuracy timeAccuracy = TimeAccuracy::Unknown;
  bool noonDefaultApplied = false;
  bool housesEnabled = true;
  std::string sourceDescription;
  double confidenceScore = 0.0;
  std::string cityInput;
  std::optional<std::int64_t> locationId;
  std::optional<std::string> timezoneName;
  std::string dstMode;
  // Direct numeric coordinates / timezone — used when no full Location row is
  // resolved via the atlas. The service layer prefers these when present;
  // otherwise it falls back to the Location row indicated by locationId.
  std::optional<double> latitudeDeg;
  std::optional<double> longitudeDeg;
  std::optional<double> timezoneOffsetHours;  // Signed: e.g. -7.0 for PDT (West=negative)
  std::optional<double> dstOffsetHours;       // Additional DST offset (typically 0 or 1)
  std::string createdAt;
  std::string updatedAt;
};

}  // namespace asteria::domain
