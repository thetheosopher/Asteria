#pragma once
#include <optional>
#include <string>
#include <cstdint>
#include "types.h"

namespace asteria::domain {

struct ChartRequest {
  std::int64_t chartRequestId = 0;
  std::int64_t primaryBirthEventId = 0;
  std::optional<std::int64_t> secondaryBirthEventId;
  ChartType chartType = ChartType::Natal;
  std::optional<std::string> requestTimeContext;
  std::string defaultHouseSystem = "Placidus";
  std::string zodiacMode = "Tropical";
  std::string unknownTimePolicy = "unknown_time_no_houses";
  bool includeHouses = true;
  std::optional<std::int64_t> themeId;
  std::string createdAt;
};

}  // namespace asteria::domain
