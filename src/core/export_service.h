#pragma once
#include "result.h"
#include "domain/export_artifact.h"
#include "render/chart_scene.h"
#include "render/theme_presets.h"

namespace asteria::core {

class ExportService {
 public:
  /// Export a chart scene to SVG file.
  Result<domain::ExportArtifact> exportSvg(
      const render::ChartScene& scene,
      std::int64_t computedChartId,
      const std::string& filePath,
      const render::ThemePreset& theme) const;

  /// Export a chart scene to PNG file.
  /// Currently a placeholder that writes a minimal stub file.
  Result<domain::ExportArtifact> exportPng(
      const render::ChartScene& scene,
      std::int64_t computedChartId,
      const std::string& filePath,
      int widthPx,
      int heightPx,
      int dpi,
      const render::ThemePreset& theme) const;
};

}  // namespace asteria::core
