#include "export_service.h"
#include "render/png_rasterizer.h"
#include "render/svg_serializer.h"
#include <Windows.h>
#include <sstream>

namespace asteria::core {

namespace {

std::string nowTimestamp() {
  SYSTEMTIME st{};
  GetLocalTime(&st);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
  return buf;
}

std::string buildThemeSnapshotJson(const render::ThemePreset& theme) {
  std::ostringstream ss;
  ss << "{"
     << R"("theme":")" << theme.name << R"(",)"
     << R"("background":")" << theme.background.toHex() << R"(")"
     << "}";
  return ss.str();
}

std::string buildExportMetadataJson(const render::ChartScene& scene,
                                    const std::string& filePath,
                                    const ExportMetadata& metadata) {
  std::ostringstream ss;
  ss << "{"
     << R"("chart_type":")" << metadata.chartType << R"(",)"
     << R"("export_profile":")" << metadata.exportProfile << R"(",)"
     << R"("layout_template":")" << metadata.layoutTemplate << R"(",)"
     << R"("date_tag":")" << metadata.dateTag << R"(",)"
     << R"("has_warnings":)" << (metadata.hasWarnings ? "true" : "false") << ","
     << R"("canonical_hash":")" << scene.canonicalHash << R"(",)"
     << R"("engine":")" << scene.engineMethod << R"(",)"
     << R"("house_system":")" << scene.houseSystem << R"(",)"
     << R"("zodiac_mode":")" << scene.zodiacMode << R"(",)"
     << R"("file_path":")" << filePath << R"(")"
     << "}";
  return ss.str();
}

}  // namespace

Result<domain::ExportArtifact> ExportService::exportSvg(
    const render::ChartScene& scene,
    std::int64_t computedChartId,
    const std::string& filePath,
    const render::ThemePreset& theme,
    const ExportMetadata& metadata) const {
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
  artifact.themeSnapshotJson = buildThemeSnapshotJson(theme);
  artifact.exportMetadataJson = buildExportMetadataJson(scene, filePath, metadata);
  artifact.exportedAt = nowTimestamp();
  return Result<domain::ExportArtifact>::success(artifact);
}

Result<domain::ExportArtifact> ExportService::exportPng(
    const render::ChartScene& scene,
    std::int64_t computedChartId,
    const std::string& filePath,
    int widthPx,
    int heightPx,
    int dpi,
    const render::ThemePreset& theme,
    const ExportMetadata& metadata) const {
  if (!render::writePngFile(scene, filePath, widthPx, heightPx, dpi)) {
    return Result<domain::ExportArtifact>::failure(
        {"export_failed", "Failed to write PNG file: " + filePath});
  }

  domain::ExportArtifact artifact;
  artifact.computedChartId = computedChartId;
  artifact.exportType = domain::ExportType::Png;
  artifact.filePath = filePath;
  artifact.widthPx = widthPx;
  artifact.heightPx = heightPx;
  artifact.dpi = dpi;
  artifact.themeSnapshotJson = buildThemeSnapshotJson(theme);
  artifact.exportMetadataJson = buildExportMetadataJson(scene, filePath, metadata);
  artifact.exportedAt = nowTimestamp();
  return Result<domain::ExportArtifact>::success(artifact);
}

}  // namespace asteria::core
