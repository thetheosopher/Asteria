#include "export_service.h"
#include "render/svg_serializer.h"
#include <fstream>

namespace asteria::core {

Result<domain::ExportArtifact> ExportService::exportSvg(
    const render::ChartScene& scene,
    std::int64_t computedChartId,
    const std::string& filePath,
    const render::ThemePreset& theme) const {
  if (!render::writeSvgFile(scene, filePath)) {
    return Result<domain::ExportArtifact>::failure(
        {"export_failed", "Failed to write SVG file: " + filePath});
  }

  domain::ExportArtifact artifact;
  artifact.computedChartId = computedChartId;
  artifact.exportType = domain::ExportType::Svg;
  artifact.filePath = filePath;
  artifact.widthPx = scene.width;
  artifact.heightPx = scene.height;
  artifact.themeSnapshotJson = R"({"theme":")" + theme.name + R"("})";
  return Result<domain::ExportArtifact>::success(artifact);
}

Result<domain::ExportArtifact> ExportService::exportPng(
    const render::ChartScene& scene,
    std::int64_t computedChartId,
    const std::string& filePath,
    int widthPx,
    int heightPx,
    int dpi,
    const render::ThemePreset& theme) const {
  // TODO: Implement real PNG rasterization from scene graph.
  // For now, write a stub marker file indicating PNG export was requested.
  std::ofstream out(filePath, std::ios::binary);
  if (!out.is_open()) {
    return Result<domain::ExportArtifact>::failure(
        {"export_failed", "Failed to open PNG output file: " + filePath});
  }
  // Write a minimal placeholder (not a valid PNG; real implementation pending)
  out << "ASTERIA_PNG_STUB:" << widthPx << "x" << heightPx << "@" << dpi << "dpi";
  out.close();

  domain::ExportArtifact artifact;
  artifact.computedChartId = computedChartId;
  artifact.exportType = domain::ExportType::Png;
  artifact.filePath = filePath;
  artifact.widthPx = widthPx;
  artifact.heightPx = heightPx;
  artifact.dpi = dpi;
  artifact.themeSnapshotJson = R"({"theme":")" + theme.name + R"("})";
  return Result<domain::ExportArtifact>::success(artifact);
}

}  // namespace asteria::core
