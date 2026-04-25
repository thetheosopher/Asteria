#pragma once
#include <string>
#include <optional>
#include <cstdint>
#include "types.h"

namespace asteria::domain {

struct ExportArtifact {
  std::int64_t exportArtifactId = 0;
  std::int64_t computedChartId = 0;
  ExportType exportType = ExportType::Svg;
  std::string filePath;
  std::optional<int> widthPx;
  std::optional<int> heightPx;
  std::optional<int> dpi;
  std::string themeSnapshotJson;
  std::string exportedAt;
};

}  // namespace asteria::domain
