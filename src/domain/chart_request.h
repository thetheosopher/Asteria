#pragma once
#include <optional>
#include <string>
#include <cstdint>
#include "types.h"

namespace asteria::domain {

/// Resolved numeric birth/transit input that the engine can consume directly
/// without additional repository lookups. Populated by service layer from
/// BirthEvent + Location records before invoking the engine.
struct ResolvedBirthInput {
  int year = 2000;
  int month = 1;
  int day = 1;
  double timeHours = 12.0;     // 24-hour decimal (e.g., 14.5 = 2:30 PM)
  double timezoneHours = 0.0;  // Signed: positive = east of UTC
  double dstHours = 0.0;       // Additional DST offset (typically 0 or 1)
  double latitudeDeg = 0.0;    // Positive = north
  double longitudeDeg = 0.0;   // Positive = east (Asteria convention)
};

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

  // Resolved engine inputs — populated by service layer.
  std::optional<ResolvedBirthInput> primaryInput;
  std::optional<ResolvedBirthInput> secondaryInput;
  std::optional<ResolvedBirthInput> transitInput;
};

}  // namespace asteria::domain
