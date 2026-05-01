#pragma once

#include <string>
#include <vector>

namespace asteria::ui::clipboard {

struct ClipboardPayload {
  std::string plainText;
  std::string markdown;
  std::string htmlFragment;
  std::string svg;
  int bitmapWidthPx = 0;
  int bitmapHeightPx = 0;
  std::vector<unsigned char> bitmapRgb;
};

std::string markdownToPlainText(const std::string& markdown);
std::string markdownToHtmlFragment(const std::string& markdown);

bool setClipboard(const ClipboardPayload& payload);
bool setText(const std::string& text);
bool setMarkdown(const std::string& markdown);
bool setSvg(const std::string& svg, const std::string& fallbackText);
bool setBitmap(int widthPx,
               int heightPx,
               const std::vector<unsigned char>& rgb,
               const std::string& fallbackText);

}  // namespace asteria::ui::clipboard
