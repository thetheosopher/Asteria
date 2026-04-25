#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace asteria::domain {

enum class TimeAccuracy {
  Exact,
  Approximate,
  Unknown
};

enum class ChartType {
  Natal,
  Synastry,
  Composite,
  TransitToNatal
};

enum class InterpretationSourceType {
  BuiltIn,
  Ollama
};

enum class ExportType {
  Svg,
  Png
};

struct PlanetPosition {
  std::string objectId;
  double longitudeDegrees = 0.0;
  double latitudeDegrees = 0.0;
  double speed = 0.0;
  std::string sign;
  std::optional<int> house;
  bool retrograde = false;
};

struct HouseCusp {
  int houseNumber = 0;
  double longitudeDegrees = 0.0;
};

struct Aspect {
  std::string objectA;
  std::string objectB;
  std::string aspectType;
  double orbDegrees = 0.0;
  std::optional<std::string> applyingOrSeparating;
};

}  // namespace asteria::domain
