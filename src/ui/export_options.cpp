#include "export_options.h"

#include "file_dialog.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace asteria::ui {

const char* exportPngProfileLabel(ExportPngProfile profile) {
  switch (profile) {
    case ExportPngProfile::ScreenShare: return "Screen Share";
    case ExportPngProfile::PrintPreview: return "Print Preview";
    case ExportPngProfile::HighResolution: return "High Resolution";
  }
  return "Screen Share";
}

ExportPngSpec exportPngProfileSpec(ExportPngProfile profile) {
  switch (profile) {
    case ExportPngProfile::ScreenShare: return {1600, 1600, 144};
    case ExportPngProfile::PrintPreview: return {2400, 2400, 240};
    case ExportPngProfile::HighResolution: return {3600, 3600, 300};
  }
  return {1600, 1600, 144};
}

const char* exportLayoutTemplateLabel(ExportLayoutTemplate layout) {
  switch (layout) {
    case ExportLayoutTemplate::ChartOnly: return "Chart Only";
    case ExportLayoutTemplate::ReferenceSheet: return "Reference Sheet";
  }
  return "Chart Only";
}

std::string currentDateTag() {
  SYSTEMTIME st{};
  GetLocalTime(&st);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
  return buf;
}

static std::string slugify(const std::string& value) {
  std::string out = sanitizeFileName(value);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
    if (std::isspace(ch) || ch == '-') return '_';
    return static_cast<char>(std::tolower(ch));
  });
  while (out.find("__") != std::string::npos) {
    out.replace(out.find("__"), 2, "_");
  }
  return out;
}

std::string buildRecommendedExportFileName(const std::string& subject,
                                           const std::string& chartType,
                                           const std::string& dateTag,
                                           const std::string& themeName,
                                           const std::string& extension) {
  return slugify(subject) + "_" + slugify(chartType) + "_" + slugify(dateTag) +
         "_" + slugify(themeName) + "." + extension;
}

}  // namespace asteria::ui