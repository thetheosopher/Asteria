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
  std::string createdAt;
  std::string updatedAt;
};

}  // namespace asteria::domain
