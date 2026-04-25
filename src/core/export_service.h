#pragma once
#include "result.h"
#include "domain/export_artifact.h"
#include "render/chart_scene.h"
#include "render/theme_presets.h"

namespace asteria::core {

struct ExportMetadata {
    std::string chartType = "natal";
    std::string exportProfile = "vector";
    std::string layoutTemplate = "chart_only";
    std::string dateTag;
    bool hasWarnings = false;
};

class ExportService {
 public:
  /// Export a chart scene to SVG file.
  Result<domain::ExportArtifact> exportSvg(
      const render::ChartScene& scene,
      std::int64_t computedChartId,
      const std::string& filePath,
            const render::ThemePreset& theme,
            const ExportMetadata& metadata = {}) const;

  /// Export a chart scene to PNG file.
  /// Currently a placeholder that writes a minimal stub file.
  Result<domain::ExportArtifact> exportPng(
      const render::ChartScene& scene,
      std::int64_t computedChartId,
      const std::string& filePath,
      int widthPx,
      int heightPx,
      int dpi,
      const render::ThemePreset& theme,
      const ExportMetadata& metadata = {}) const;
};

}  // namespace asteria::core
