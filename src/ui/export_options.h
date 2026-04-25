#pragma once

#include <string>

namespace asteria::ui {

enum class ExportPngProfile {
  ScreenShare = 0,
  PrintPreview = 1,
  HighResolution = 2,
};

enum class ExportLayoutTemplate {
  ChartOnly = 0,
  ReferenceSheet = 1,
};

struct ExportPngSpec {
  int widthPx;
  int heightPx;
  int dpi;
};

const char* exportPngProfileLabel(ExportPngProfile profile);
ExportPngSpec exportPngProfileSpec(ExportPngProfile profile);
const char* exportLayoutTemplateLabel(ExportLayoutTemplate layout);
std::string buildRecommendedExportFileName(const std::string& subject,
                                           const std::string& chartType,
                                           const std::string& dateTag,
                                           const std::string& themeName,
                                           const std::string& extension);
std::string currentDateTag();

}  // namespace asteria::ui