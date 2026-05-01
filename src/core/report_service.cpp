#include "report_service.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <hpdf.h>

#include "render/png_rasterizer.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace asteria::core {

namespace {

struct HpdfErrorState {
  HPDF_STATUS errorNo = HPDF_OK;
  HPDF_STATUS detailNo = 0;
};

void hpdfErrorHandler(HPDF_STATUS errorNo, HPDF_STATUS detailNo, void* userData) {
  auto* state = static_cast<HpdfErrorState*>(userData);
  if (state && state->errorNo == HPDF_OK) {
    state->errorNo = errorNo;
    state->detailNo = detailNo;
  }
}

struct HpdfDeleter {
  void operator()(HPDF_Doc pdf) const {
    if (pdf) HPDF_Free(pdf);
  }
};

using HpdfHandle = std::unique_ptr<std::remove_pointer_t<HPDF_Doc>, HpdfDeleter>;

struct ReportTemplateSpec {
  float pageWidth = 612.0f;
  float pageHeight = 792.0f;
  float margin = 54.0f;
  float titleSize = 18.0f;
  float sectionSize = 14.0f;
  float bodySize = 10.0f;
  float lineHeight = 13.2f;
  float chartMaxHeight = 360.0f;
  bool compact = false;
  bool emphasizeMetadata = true;
};

struct ReportFonts {
  HPDF_Font regular = nullptr;
  HPDF_Font bold = nullptr;
  bool unicode = false;
};

struct PageState {
  HPDF_Page page = nullptr;
  int pageNumber = 0;
  float y = 0.0f;
};

enum class ReportLineStyle {
  Blank,
  Body,
  Heading,
  Bullet,
  Numbered,
  Rule,
};

struct ReportLine {
  ReportLineStyle style = ReportLineStyle::Body;
  std::string text;
  int headingLevel = 0;
  int number = 0;
};

std::string nowTimestamp() {
  SYSTEMTIME st{};
  GetLocalTime(&st);
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
  return buffer;
}

std::string trim(const std::string& value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  });
  if (first == value.end()) return {};
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  }).base();
  return std::string(first, last);
}

std::string stripInlineMarkdown(std::string value) {
  for (const std::string marker : {"**", "__", "`", "*", "_"}) {
    size_t pos = 0;
    while ((pos = value.find(marker, pos)) != std::string::npos) {
      value.erase(pos, marker.size());
    }
  }
  return value;
}

std::string pdfSafeText(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(value[i]);
    if (ch == '\t') {
      out += ' ';
    } else if (ch >= 32 && ch < 127) {
      out += static_cast<char>(ch);
    } else if (ch >= 128) {
      out += '?';
      while (i + 1u < value.size() && (static_cast<unsigned char>(value[i + 1u]) & 0xC0u) == 0x80u) {
        ++i;
      }
    }
  }
  return out;
}

std::string printableText(const std::string& value, const ReportFonts& fonts) {
  return fonts.unicode ? value : pdfSafeText(value);
}

std::string jsonEscape(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char ch : value) {
    switch (ch) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': break;
      case '\t': out += "\\t"; break;
      default: out += ch; break;
    }
  }
  return out;
}

std::string defaultWindowsFontPath(const char* fileName) {
  char windowsDir[MAX_PATH] = {};
  const UINT size = GetWindowsDirectoryA(windowsDir, MAX_PATH);
  if (size == 0 || size >= MAX_PATH) return {};

  const std::string path = std::string(windowsDir) + "\\Fonts\\" + fileName;
  const DWORD attrs = GetFileAttributesA(path.c_str());
  return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) ? path : std::string();
}

ReportTemplateSpec templateSpec(PdfReportTemplate reportTemplate, bool archivalMode) {
  ReportTemplateSpec spec;
  switch (reportTemplate) {
    case PdfReportTemplate::ClientReport:
      spec = {612.0f, 792.0f, 54.0f, 19.0f, 14.0f, 10.2f, 13.4f, 350.0f, false, true};
      break;
    case PdfReportTemplate::StudyNotes:
      spec = {612.0f, 792.0f, 48.0f, 17.0f, 13.0f, 9.7f, 12.8f, 300.0f, false, false};
      break;
    case PdfReportTemplate::CompactOnePage:
      spec = {612.0f, 792.0f, 42.0f, 16.0f, 12.2f, 8.4f, 10.6f, 255.0f, true, false};
      break;
    case PdfReportTemplate::ArchiveCopy:
      spec = {612.0f, 792.0f, 58.0f, 18.0f, 13.2f, 9.4f, 12.3f, 325.0f, false, true};
      break;
  }
  if (archivalMode) spec.emphasizeMetadata = true;
  return spec;
}

int detectHeading(const std::string& line, std::string& content) {
  size_t count = 0;
  while (count < line.size() && line[count] == '#') ++count;
  if (count == 0 || count > 4 || count >= line.size() || line[count] != ' ') {
    content = line;
    return 0;
  }
  content = line.substr(count + 1u);
  return static_cast<int>(count);
}

bool detectNumberedList(const std::string& line, int& number, std::string& content) {
  size_t index = 0;
  while (index < line.size() && std::isdigit(static_cast<unsigned char>(line[index])) != 0) ++index;
  if (index == 0 || index >= line.size() || line[index] != '.') return false;
  if (index + 1u >= line.size() || line[index + 1u] != ' ') return false;
  number = std::stoi(line.substr(0, index));
  content = line.substr(index + 2u);
  return true;
}

bool isHorizontalRule(const std::string& line) {
  if (line.size() < 3u) return false;
  const char marker = line.front();
  if (marker != '-' && marker != '*' && marker != '_') return false;
  return std::all_of(line.begin(), line.end(), [&](char ch) {
    return ch == marker || std::isspace(static_cast<unsigned char>(ch)) != 0;
  });
}

std::vector<ReportLine> parseReportLines(const std::string& markdown) {
  std::vector<ReportLine> lines;
  std::istringstream input(markdown);
  std::string line;

  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::string trimmed = trim(line);
    if (trimmed.empty()) {
      lines.push_back({ReportLineStyle::Blank, {}, 0, 0});
      continue;
    }

    if (isHorizontalRule(trimmed)) {
      lines.push_back({ReportLineStyle::Rule, {}, 0, 0});
      continue;
    }

    std::string heading;
    const int headingLevel = detectHeading(trimmed, heading);
    if (headingLevel > 0) {
      lines.push_back({ReportLineStyle::Heading, stripInlineMarkdown(trim(heading)), headingLevel, 0});
      continue;
    }

    if (trimmed.size() >= 2u && (trimmed[0] == '-' || trimmed[0] == '*') && trimmed[1] == ' ') {
      lines.push_back({ReportLineStyle::Bullet, stripInlineMarkdown(trim(trimmed.substr(2))), 0, 0});
      continue;
    }

    int number = 0;
    std::string numberedContent;
    if (detectNumberedList(trimmed, number, numberedContent)) {
      lines.push_back({ReportLineStyle::Numbered, stripInlineMarkdown(trim(numberedContent)), 0, number});
      continue;
    }

    lines.push_back({ReportLineStyle::Body, stripInlineMarkdown(trimmed), 0, 0});
  }

  return lines;
}

ReportFonts loadReportFonts(HPDF_Doc pdf, const PdfReportRequest& request) {
  ReportFonts fonts;
  const std::string regularPath = request.regularFontPath.empty()
      ? defaultWindowsFontPath("segoeui.ttf")
      : request.regularFontPath;
  const std::string boldPath = request.boldFontPath.empty()
      ? defaultWindowsFontPath("segoeuib.ttf")
      : request.boldFontPath;

  if (!regularPath.empty() && !boldPath.empty()) {
    HPDF_UseUTFEncodings(pdf);
    HPDF_SetCurrentEncoder(pdf, "UTF-8");
    const char* regularName = HPDF_LoadTTFontFromFile(pdf, regularPath.c_str(), HPDF_TRUE);
    const char* boldName = HPDF_LoadTTFontFromFile(pdf, boldPath.c_str(), HPDF_TRUE);
    if (regularName && boldName) {
      fonts.regular = HPDF_GetFont(pdf, regularName, "UTF-8");
      fonts.bold = HPDF_GetFont(pdf, boldName, "UTF-8");
      fonts.unicode = fonts.regular != nullptr && fonts.bold != nullptr;
    }
  }

  if (!fonts.regular || !fonts.bold) {
    HPDF_ResetError(pdf);
    fonts.regular = HPDF_GetFont(pdf, "Helvetica", nullptr);
    fonts.bold = HPDF_GetFont(pdf, "Helvetica-Bold", nullptr);
    fonts.unicode = false;
  }
  return fonts;
}

std::vector<std::string> wrapText(HPDF_Page page,
                                  HPDF_Font font,
                                  float fontSize,
                                  const std::string& text,
                                  float maxWidth,
                                  const ReportFonts& fonts) {
  std::vector<std::string> wrapped;
  if (text.empty()) {
    wrapped.push_back({});
    return wrapped;
  }

  HPDF_Page_SetFontAndSize(page, font, fontSize);
  std::istringstream words(text);
  std::string word;
  std::string current;

  auto widthOf = [&](const std::string& value) {
    const std::string printable = printableText(value, fonts);
    return static_cast<float>(HPDF_Page_TextWidth(page, printable.c_str()));
  };

  auto pushLongWord = [&](const std::string& value) {
    std::string fragment;
    for (char ch : value) {
      const std::string candidate = fragment + ch;
      if (!fragment.empty() && widthOf(candidate) > maxWidth) {
        wrapped.push_back(fragment);
        fragment.clear();
      }
      fragment += ch;
    }
    if (!fragment.empty()) current = fragment;
  };

  while (words >> word) {
    const std::string candidate = current.empty() ? word : current + " " + word;
    if (widthOf(candidate) <= maxWidth) {
      current = candidate;
      continue;
    }

    if (!current.empty()) {
      wrapped.push_back(current);
      current.clear();
    }

    if (widthOf(word) <= maxWidth) {
      current = word;
    } else {
      pushLongWord(word);
    }
  }

  if (!current.empty()) wrapped.push_back(current);
  return wrapped;
}

void setPdfStrokeColor(HPDF_Page page, const render::Color& color) {
  HPDF_Page_SetRGBStroke(page,
                         static_cast<float>(color.r) / 255.0f,
                         static_cast<float>(color.g) / 255.0f,
                         static_cast<float>(color.b) / 255.0f);
}

void setPdfFillColor(HPDF_Page page, const render::Color& color) {
  HPDF_Page_SetRGBFill(page,
                       static_cast<float>(color.r) / 255.0f,
                       static_cast<float>(color.g) / 255.0f,
                       static_cast<float>(color.b) / 255.0f);
}

void drawChartSceneVector(HPDF_Page page,
                          const render::ChartScene& scene,
                          const ReportFonts& fonts,
                          float x,
                          float y,
                          float width,
                          float height) {
  const float scale = (std::min)(width / static_cast<float>(scene.width),
                                 height / static_cast<float>(scene.height));
  const float chartWidth = static_cast<float>(scene.width) * scale;
  const float chartHeight = static_cast<float>(scene.height) * scale;
  const float originX = x + (width - chartWidth) * 0.5f;
  const float originY = y + (height - chartHeight) * 0.5f;

  auto px = [&](double value) { return originX + static_cast<float>(value) * scale; };
  auto py = [&](double value) { return originY + chartHeight - static_cast<float>(value) * scale; };

  setPdfFillColor(page, scene.background);
  HPDF_Page_Rectangle(page, originX, originY, chartWidth, chartHeight);
  HPDF_Page_Fill(page);

  for (const auto& circle : scene.circles) {
    setPdfStrokeColor(page, circle.stroke);
    HPDF_Page_SetLineWidth(page, static_cast<float>(circle.strokeWidth) * scale);
    HPDF_Page_Circle(page, px(circle.cx), py(circle.cy), static_cast<float>(circle.radius) * scale);
    HPDF_Page_Stroke(page);
  }

  for (const auto& line : scene.lines) {
    setPdfStrokeColor(page, line.stroke);
    HPDF_Page_SetLineWidth(page, static_cast<float>(line.strokeWidth) * scale);
    HPDF_Page_MoveTo(page, px(line.x1), py(line.y1));
    HPDF_Page_LineTo(page, px(line.x2), py(line.y2));
    HPDF_Page_Stroke(page);
  }

  for (const auto& text : scene.texts) {
    const std::string printable = printableText(text.content, fonts);
    setPdfFillColor(page, text.fill);
    const float fontSize = (std::max)(5.0f, static_cast<float>(text.fontSize) * scale);
    HPDF_Page_SetFontAndSize(page, fonts.regular, fontSize);
    const float textWidth = static_cast<float>(HPDF_Page_TextWidth(page, printable.c_str()));
    float textX = px(text.x);
    if (text.anchor == "middle") textX -= textWidth * 0.5f;
    else if (text.anchor == "end") textX -= textWidth;
    HPDF_Page_BeginText(page);
    HPDF_Page_TextOut(page, textX, py(text.y), printable.c_str());
    HPDF_Page_EndText(page);
  }
}

std::string buildThemeSnapshotJson(const render::ThemePreset& theme) {
  std::ostringstream ss;
  ss << "{"
     << R"("theme":")" << jsonEscape(theme.name) << R"(",)"
     << R"("background":")" << theme.background.toHex() << R"(")"
     << "}";
  return ss.str();
}

std::string buildReportMetadataJson(const PdfReportRequest& request) {
  std::ostringstream ss;
  ss << "{"
     << R"("report_type":"ai_interpretation",)"
     << R"("title":")" << jsonEscape(request.title) << R"(",)"
     << R"("template":")" << jsonEscape(pdfReportTemplateLabel(request.reportTemplate)) << R"(",)"
     << R"("archival_mode":)" << (request.archivalMode ? "true" : "false") << ","
     << R"("chart_backend":")" << (request.preferVectorChart ? "vector" : "raster") << R"(",)"
     << R"("source_label":")" << jsonEscape(request.sourceLabel) << R"(",)"
     << R"("model_label":")" << jsonEscape(request.modelLabel) << R"(",)"
     << R"("canonical_hash":")" << jsonEscape(request.chartScene.canonicalHash) << R"(",)"
     << R"("engine":")" << jsonEscape(request.chartScene.engineMethod) << R"(",)"
     << R"("house_system":")" << jsonEscape(request.chartScene.houseSystem) << R"(",)"
     << R"("zodiac_mode":")" << jsonEscape(request.chartScene.zodiacMode) << R"(",)"
     << R"("file_path":")" << jsonEscape(request.outputPath) << R"(")"
     << "}";
  return ss.str();
}

}  // namespace

const char* pdfReportTemplateLabel(PdfReportTemplate reportTemplate) {
  switch (reportTemplate) {
    case PdfReportTemplate::ClientReport: return "Client Report";
    case PdfReportTemplate::StudyNotes: return "Study Notes";
    case PdfReportTemplate::CompactOnePage: return "Compact One-Page";
    case PdfReportTemplate::ArchiveCopy: return "Archive Copy";
  }
  return "Client Report";
}

std::string reportPlainTextFromMarkdown(const std::string& markdown) {
  std::ostringstream out;
  for (const auto& line : parseReportLines(markdown)) {
    switch (line.style) {
      case ReportLineStyle::Blank:
        out << '\n';
        break;
      case ReportLineStyle::Bullet:
        out << "- " << line.text << '\n';
        break;
      case ReportLineStyle::Numbered:
        out << line.number << ". " << line.text << '\n';
        break;
      case ReportLineStyle::Rule:
        out << "---\n";
        break;
      case ReportLineStyle::Heading:
      case ReportLineStyle::Body:
        out << line.text << '\n';
        break;
    }
  }
  return out.str();
}

Result<domain::ExportArtifact> ReportService::exportPdfReport(const PdfReportRequest& request) const {
  if (request.outputPath.empty()) {
    return Result<domain::ExportArtifact>::failure({"invalid_report_path", "PDF report output path is required."});
  }
  if (request.interpretationMarkdown.empty() && request.deterministicInterpretationMarkdown.empty()) {
    return Result<domain::ExportArtifact>::failure({"empty_report", "PDF report interpretation text is empty."});
  }

  render::RasterImage chartImage;
  if (!request.preferVectorChart && !render::rasterizeSceneRgb(request.chartScene,
                                                               request.chartImageWidthPx,
                                                               request.chartImageHeightPx,
                                                               request.chartImageDpi,
                                                               chartImage)) {
    return Result<domain::ExportArtifact>::failure({"chart_rasterization_failed", "Failed to rasterize chart for PDF report."});
  }

  HpdfErrorState errorState;
  HpdfHandle pdf(HPDF_New(hpdfErrorHandler, &errorState));
  if (!pdf) {
    return Result<domain::ExportArtifact>::failure({"pdf_create_failed", "Failed to create PDF document."});
  }

  HPDF_SetCompressionMode(pdf.get(), request.archivalMode ? HPDF_COMP_NONE : HPDF_COMP_TEXT);
  HPDF_SetInfoAttr(pdf.get(), HPDF_INFO_CREATOR, "Asteria");
  const ReportFonts fonts = loadReportFonts(pdf.get(), request);
  if (!fonts.regular || !fonts.bold) {
    return Result<domain::ExportArtifact>::failure({"pdf_font_failed", "Failed to load PDF fonts."});
  }

  const std::string title = request.title.empty() ? std::string("Asteria AI Interpretation") : request.title;
  HPDF_SetInfoAttr(pdf.get(), HPDF_INFO_TITLE, pdfSafeText(title).c_str());

  HPDF_Image image = nullptr;
  if (!request.preferVectorChart) {
    image = HPDF_LoadRawImageFromMem(pdf.get(),
        chartImage.rgb.data(),
        chartImage.widthPx,
        chartImage.heightPx,
        HPDF_CS_DEVICE_RGB,
        8);
    if (!image) {
      return Result<domain::ExportArtifact>::failure({"pdf_image_failed", "Failed to embed chart image in PDF report."});
    }
  }

  const ReportTemplateSpec spec = templateSpec(request.reportTemplate, request.archivalMode);
  const float bodyWidth = spec.pageWidth - spec.margin * 2.0f;
  PageState pageState;

  auto drawText = [&](const std::string& text, HPDF_Font drawFont, float size, float x, float baseline) {
    const std::string printable = printableText(text, fonts);
    HPDF_Page_BeginText(pageState.page);
    HPDF_Page_SetFontAndSize(pageState.page, drawFont, size);
    HPDF_Page_TextOut(pageState.page, x, baseline, printable.c_str());
    HPDF_Page_EndText(pageState.page);
  };

  auto drawFooter = [&](HPDF_Page footerPage, int pageNumber) {
    const std::string left = request.archivalMode && !request.chartScene.canonicalHash.empty()
        ? "Asteria archival report / " + request.chartScene.canonicalHash
        : "Asteria AI interpretation report";
    const std::string right = "Page " + std::to_string(pageNumber);
    HPDF_Page_SetRGBFill(footerPage, 0.35f, 0.35f, 0.35f);
    HPDF_Page_SetFontAndSize(footerPage, fonts.regular, 8.0f);
    HPDF_Page_BeginText(footerPage);
    HPDF_Page_TextOut(footerPage, spec.margin, 30.0f, printableText(left, fonts).c_str());
    const float rightWidth = static_cast<float>(HPDF_Page_TextWidth(footerPage, right.c_str()));
    HPDF_Page_TextOut(footerPage, spec.pageWidth - spec.margin - rightWidth, 30.0f, right.c_str());
    HPDF_Page_EndText(footerPage);
  };

  auto addPage = [&]() {
    pageState.page = HPDF_AddPage(pdf.get());
    pageState.pageNumber++;
    HPDF_Page_SetWidth(pageState.page, spec.pageWidth);
    HPDF_Page_SetHeight(pageState.page, spec.pageHeight);
    pageState.y = spec.pageHeight - spec.margin;
    if (pageState.pageNumber > 1) {
      drawText(title, fonts.regular, 8.5f, spec.margin, spec.pageHeight - 30.0f);
      pageState.y -= 14.0f;
    }
  };

  auto ensureSpace = [&](float needed) {
    if (!pageState.page || pageState.y - needed < spec.margin + 22.0f) {
      if (pageState.page) drawFooter(pageState.page, pageState.pageNumber);
      addPage();
    }
  };

  addPage();
  drawText(title, fonts.bold, spec.titleSize, spec.margin, pageState.y);
  pageState.y -= spec.titleSize + 6.0f;

  if (!request.subtitle.empty()) {
    drawText(request.subtitle, fonts.regular, spec.bodySize, spec.margin, pageState.y);
    pageState.y -= spec.lineHeight + 2.0f;
  }
  if (!request.generatedAtLabel.empty()) {
    drawText("Generated: " + request.generatedAtLabel, fonts.regular, 8.5f, spec.margin, pageState.y);
    pageState.y -= 11.5f;
  }
  if (!request.sourceLabel.empty() && spec.emphasizeMetadata) {
    drawText("Source: " + request.sourceLabel, fonts.regular, 8.5f, spec.margin, pageState.y);
    pageState.y -= 11.5f;
  }
  if (!request.modelLabel.empty() && spec.emphasizeMetadata) {
    drawText("Model: " + request.modelLabel, fonts.regular, 8.5f, spec.margin, pageState.y);
    pageState.y -= 11.5f;
  }
  if (request.archivalMode) {
    drawText("Template: " + std::string(pdfReportTemplateLabel(request.reportTemplate)), fonts.regular, 8.5f, spec.margin, pageState.y);
    pageState.y -= 11.5f;
  }
  pageState.y -= 8.0f;

  const float imageMaxWidth = bodyWidth;
  const float sourceWidth = request.preferVectorChart ? static_cast<float>(request.chartScene.width) : static_cast<float>(chartImage.widthPx);
  const float sourceHeight = request.preferVectorChart ? static_cast<float>(request.chartScene.height) : static_cast<float>(chartImage.heightPx);
  const float imageScale = (std::min)(imageMaxWidth / sourceWidth, spec.chartMaxHeight / sourceHeight);
  const float imageWidth = sourceWidth * imageScale;
  const float imageHeight = sourceHeight * imageScale;
  const float imageX = spec.margin + (bodyWidth - imageWidth) * 0.5f;
  pageState.y -= imageHeight;
  if (request.preferVectorChart) {
    drawChartSceneVector(pageState.page, request.chartScene, fonts, imageX, pageState.y, imageWidth, imageHeight);
  } else {
    HPDF_Page_DrawImage(pageState.page, image, imageX, pageState.y, imageWidth, imageHeight);
  }
  pageState.y -= spec.compact ? 14.0f : 24.0f;

  auto drawMarkdownSection = [&](const std::string& sectionTitle, const std::string& markdown) {
    if (markdown.empty()) return;
    ensureSpace(spec.sectionSize + 24.0f);
    drawText(sectionTitle, fonts.bold, spec.sectionSize, spec.margin, pageState.y);
    pageState.y -= spec.sectionSize + 7.0f;

    for (const auto& line : parseReportLines(markdown)) {
      if (line.style == ReportLineStyle::Blank) {
        pageState.y -= spec.compact ? 4.0f : 7.0f;
        continue;
      }

      if (line.style == ReportLineStyle::Rule) {
        ensureSpace(10.0f);
        HPDF_Page_SetRGBStroke(pageState.page, 0.78f, 0.78f, 0.78f);
        HPDF_Page_SetLineWidth(pageState.page, 0.5f);
        HPDF_Page_MoveTo(pageState.page, spec.margin, pageState.y);
        HPDF_Page_LineTo(pageState.page, spec.pageWidth - spec.margin, pageState.y);
        HPDF_Page_Stroke(pageState.page);
        pageState.y -= 9.0f;
        continue;
      }

      const bool heading = line.style == ReportLineStyle::Heading;
      const bool listItem = line.style == ReportLineStyle::Bullet || line.style == ReportLineStyle::Numbered;
      const float size = heading ? (line.headingLevel <= 1 ? spec.bodySize + 2.2f : spec.bodySize + 1.1f) : spec.bodySize;
      const float lineHeight = heading ? spec.lineHeight + 3.0f : spec.lineHeight;
      HPDF_Font drawFont = heading ? fonts.bold : fonts.regular;
      const float x = listItem ? spec.margin + 16.0f : spec.margin;
      const float maxWidth = listItem ? bodyWidth - 16.0f : bodyWidth;
      const std::string prefix = line.style == ReportLineStyle::Bullet
          ? "- "
          : (line.style == ReportLineStyle::Numbered ? std::to_string(line.number) + ". " : "");
      const auto wrapped = wrapText(pageState.page,
                                    drawFont,
                                    size,
                                    line.text,
                                    maxWidth - (listItem ? 12.0f : 0.0f),
                                    fonts);

      if (heading) pageState.y -= 4.0f;
      for (size_t index = 0; index < wrapped.size(); ++index) {
        ensureSpace(lineHeight + 2.0f);
        const std::string text = (listItem && index == 0u) ? prefix + wrapped[index] : wrapped[index];
        drawText(text, drawFont, size, x, pageState.y);
        pageState.y -= lineHeight;
      }
      if (heading) pageState.y -= 2.0f;
    }
  };

  drawMarkdownSection("Built-In Interpretation", request.deterministicInterpretationMarkdown);
  drawMarkdownSection("AI Interpretation", request.interpretationMarkdown);
  if (pageState.page) drawFooter(pageState.page, pageState.pageNumber);

  if (HPDF_SaveToFile(pdf.get(), request.outputPath.c_str()) != HPDF_OK || errorState.errorNo != HPDF_OK) {
    std::ostringstream message;
    message << "Failed to write PDF report: " << request.outputPath;
    if (errorState.errorNo != HPDF_OK) {
      message << " (hpdf error " << errorState.errorNo << ", detail " << errorState.detailNo << ")";
    }
    return Result<domain::ExportArtifact>::failure({"pdf_write_failed", message.str()});
  }

  domain::ExportArtifact artifact;
  artifact.computedChartId = request.computedChartId;
  artifact.exportType = domain::ExportType::Pdf;
  artifact.filePath = request.outputPath;
  artifact.widthPx = static_cast<int>(spec.pageWidth);
  artifact.heightPx = static_cast<int>(spec.pageHeight);
  artifact.dpi = request.chartImageDpi;
  artifact.themeSnapshotJson = buildThemeSnapshotJson(request.theme);
  artifact.exportMetadataJson = buildReportMetadataJson(request);
  artifact.exportedAt = nowTimestamp();
  return Result<domain::ExportArtifact>::success(artifact);
}

}  // namespace asteria::core
