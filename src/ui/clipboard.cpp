#include "clipboard.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace asteria::ui::clipboard {

namespace {

std::wstring utf8ToWide(const std::string& value) {
  if (value.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
  if (size <= 0) return {};

  std::wstring out(static_cast<size_t>(size - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), size);
  return out;
}

bool putUnicodeText(const std::string& text) {
  const std::wstring wide = utf8ToWide(text);
  const size_t bytes = (wide.size() + 1u) * sizeof(wchar_t);
  HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!handle) return false;

  void* data = GlobalLock(handle);
  if (!data) {
    GlobalFree(handle);
    return false;
  }
  std::memcpy(data, wide.c_str(), bytes);
  GlobalUnlock(handle);

  if (!SetClipboardData(CF_UNICODETEXT, handle)) {
    GlobalFree(handle);
    return false;
  }
  return true;
}

bool putUtf8Format(UINT format, const std::string& text) {
  const size_t bytes = text.size() + 1u;
  HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!handle) return false;

  void* data = GlobalLock(handle);
  if (!data) {
    GlobalFree(handle);
    return false;
  }
  std::memcpy(data, text.c_str(), text.size());
  static_cast<char*>(data)[text.size()] = '\0';
  GlobalUnlock(handle);

  if (!SetClipboardData(format, handle)) {
    GlobalFree(handle);
    return false;
  }
  return true;
}

bool putBitmapDib(int widthPx, int heightPx, const std::vector<unsigned char>& rgb) {
  if (widthPx <= 0 || heightPx <= 0) return false;
  const size_t expectedBytes = static_cast<size_t>(widthPx) * static_cast<size_t>(heightPx) * 3u;
  if (rgb.size() < expectedBytes) return false;

  const int rowStride = ((widthPx * 3 + 3) / 4) * 4;
  const size_t pixelBytes = static_cast<size_t>(rowStride) * static_cast<size_t>(heightPx);
  const size_t totalBytes = sizeof(BITMAPINFOHEADER) + pixelBytes;

  HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, totalBytes);
  if (!handle) return false;

  auto* data = static_cast<unsigned char*>(GlobalLock(handle));
  if (!data) {
    GlobalFree(handle);
    return false;
  }

  auto* header = reinterpret_cast<BITMAPINFOHEADER*>(data);
  *header = {};
  header->biSize = sizeof(BITMAPINFOHEADER);
  header->biWidth = widthPx;
  header->biHeight = heightPx;
  header->biPlanes = 1;
  header->biBitCount = 24;
  header->biCompression = BI_RGB;
  header->biSizeImage = static_cast<DWORD>(pixelBytes);

  unsigned char* pixels = data + sizeof(BITMAPINFOHEADER);
  std::fill(pixels, pixels + pixelBytes, static_cast<unsigned char>(0));
  for (int y = 0; y < heightPx; ++y) {
    const int sourceY = heightPx - 1 - y;
    const unsigned char* source = rgb.data() + static_cast<size_t>(sourceY) * static_cast<size_t>(widthPx) * 3u;
    unsigned char* dest = pixels + static_cast<size_t>(y) * static_cast<size_t>(rowStride);
    for (int x = 0; x < widthPx; ++x) {
      dest[x * 3 + 0] = source[x * 3 + 2];
      dest[x * 3 + 1] = source[x * 3 + 1];
      dest[x * 3 + 2] = source[x * 3 + 0];
    }
  }
  GlobalUnlock(handle);

  if (!SetClipboardData(CF_DIB, handle)) {
    GlobalFree(handle);
    return false;
  }
  return true;
}

std::string removeInlineMarkdown(std::string value) {
  for (const std::string marker : {"**", "__", "`", "*", "_"}) {
    size_t pos = 0;
    while ((pos = value.find(marker, pos)) != std::string::npos) {
      value.erase(pos, marker.size());
    }
  }
  return value;
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

std::string escapeHtml(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char ch : value) {
    switch (ch) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += ch; break;
    }
  }
  return out;
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

std::string buildHtmlClipboardDocument(const std::string& fragment) {
  std::string prefix =
      "Version:0.9\r\n"
      "StartHTML:0000000000\r\n"
      "EndHTML:0000000000\r\n"
      "StartFragment:0000000000\r\n"
      "EndFragment:0000000000\r\n";
  const std::string startMarker = "<!--StartFragment-->";
  const std::string endMarker = "<!--EndFragment-->";
  const std::string html = "<html><body>\r\n" + startMarker + "\r\n" + fragment +
                           "\r\n" + endMarker + "\r\n</body></html>";

  const size_t startHtml = prefix.size();
  const size_t startFragment = startHtml + html.find(startMarker) + startMarker.size() + 2u;
  const size_t endFragment = startHtml + html.find(endMarker) - 2u;
  const size_t endHtml = startHtml + html.size();

  auto writeOffset = [&](const char* label, size_t value) {
    const size_t pos = prefix.find(label);
    if (pos == std::string::npos) return;
    char buffer[11] = {};
    std::snprintf(buffer, sizeof(buffer), "%010zu", value);
    prefix.replace(pos + std::strlen(label), 10, buffer);
  };

  writeOffset("StartHTML:", startHtml);
  writeOffset("EndHTML:", endHtml);
  writeOffset("StartFragment:", startFragment);
  writeOffset("EndFragment:", endFragment);
  return prefix + html;
}

}  // namespace

std::string markdownToPlainText(const std::string& markdown) {
  std::ostringstream out;
  std::istringstream input(markdown);
  std::string line;

  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::string content;
    if (detectHeading(line, content) > 0) {
      out << removeInlineMarkdown(trim(content)) << '\n';
      continue;
    }

    std::string trimmed = trim(line);
    if (trimmed.size() >= 2u && (trimmed[0] == '-' || trimmed[0] == '*') && trimmed[1] == ' ') {
      out << "- " << removeInlineMarkdown(trim(trimmed.substr(2))) << '\n';
      continue;
    }

    out << removeInlineMarkdown(line) << '\n';
  }

  return out.str();
}

std::string markdownToHtmlFragment(const std::string& markdown) {
  std::ostringstream out;
  std::istringstream input(markdown);
  std::string line;
  bool inList = false;

  auto closeList = [&]() {
    if (inList) {
      out << "</ul>\n";
      inList = false;
    }
  };

  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::string trimmed = trim(line);
    if (trimmed.empty()) {
      closeList();
      continue;
    }

    std::string heading;
    const int level = detectHeading(trimmed, heading);
    if (level > 0) {
      closeList();
      const int safeLevel = (std::min)(4, (std::max)(1, level));
      out << "<h" << safeLevel << ">" << escapeHtml(removeInlineMarkdown(trim(heading)))
          << "</h" << safeLevel << ">\n";
      continue;
    }

    if (trimmed.size() >= 2u && (trimmed[0] == '-' || trimmed[0] == '*') && trimmed[1] == ' ') {
      if (!inList) {
        out << "<ul>\n";
        inList = true;
      }
      out << "<li>" << escapeHtml(removeInlineMarkdown(trim(trimmed.substr(2)))) << "</li>\n";
      continue;
    }

    closeList();
    out << "<p>" << escapeHtml(removeInlineMarkdown(trimmed)) << "</p>\n";
  }

  closeList();
  return out.str();
}

bool setClipboard(const ClipboardPayload& payload) {
  const bool hasBitmap = payload.bitmapWidthPx > 0 && payload.bitmapHeightPx > 0 && !payload.bitmapRgb.empty();
  if (payload.plainText.empty() && payload.markdown.empty() && payload.htmlFragment.empty() && payload.svg.empty() && !hasBitmap) {
    return false;
  }

  const HWND owner = GetActiveWindow();
  if (!OpenClipboard(owner)) return false;
  EmptyClipboard();

  bool ok = true;
  const std::string plainText = !payload.plainText.empty()
      ? payload.plainText
      : (!payload.markdown.empty() ? markdownToPlainText(payload.markdown) : payload.svg);
  if (!plainText.empty()) ok = putUnicodeText(plainText) && ok;

  if (!payload.markdown.empty()) {
    const UINT markdownMime = RegisterClipboardFormatW(L"text/markdown");
    const UINT markdownName = RegisterClipboardFormatW(L"Markdown");
    ok = putUtf8Format(markdownMime, payload.markdown) && ok;
    ok = putUtf8Format(markdownName, payload.markdown) && ok;
  }

  if (!payload.htmlFragment.empty()) {
    const UINT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
    ok = putUtf8Format(htmlFormat, buildHtmlClipboardDocument(payload.htmlFragment)) && ok;
  }

  if (!payload.svg.empty()) {
    const UINT svgMime = RegisterClipboardFormatW(L"image/svg+xml");
    const UINT svgName = RegisterClipboardFormatW(L"SVG");
    ok = putUtf8Format(svgMime, payload.svg) && ok;
    ok = putUtf8Format(svgName, payload.svg) && ok;
  }

  if (hasBitmap) {
    ok = putBitmapDib(payload.bitmapWidthPx, payload.bitmapHeightPx, payload.bitmapRgb) && ok;
  }

  CloseClipboard();
  return ok;
}

bool setText(const std::string& text) {
  ClipboardPayload payload;
  payload.plainText = text;
  return setClipboard(payload);
}

bool setMarkdown(const std::string& markdown) {
  ClipboardPayload payload;
  payload.plainText = markdownToPlainText(markdown);
  payload.markdown = markdown;
  payload.htmlFragment = markdownToHtmlFragment(markdown);
  return setClipboard(payload);
}

bool setSvg(const std::string& svg, const std::string& fallbackText) {
  ClipboardPayload payload;
  payload.plainText = fallbackText;
  payload.svg = svg;
  return setClipboard(payload);
}

bool setBitmap(int widthPx,
               int heightPx,
               const std::vector<unsigned char>& rgb,
               const std::string& fallbackText) {
  ClipboardPayload payload;
  payload.plainText = fallbackText;
  payload.bitmapWidthPx = widthPx;
  payload.bitmapHeightPx = heightPx;
  payload.bitmapRgb = rgb;
  return setClipboard(payload);
}

}  // namespace asteria::ui::clipboard
