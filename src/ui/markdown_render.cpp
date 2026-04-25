#include "markdown_render.h"

#include "imgui.h"

#include <sstream>

namespace asteria::ui::markdown_render {

namespace {

int detectHeading(const std::string& line, std::string& content) {
  size_t i = 0;
  while (i < line.size() && line[i] == '#') ++i;
  if (i == 0 || i > 4 || (i < line.size() && line[i] != ' ')) {
    content = line;
    return 0;
  }
  content = (i + 1 < line.size()) ? line.substr(i + 1) : "";
  return static_cast<int>(i);
}

void renderLineWithBold(const std::string& line, const ImVec4& normalColor,
                        const ImVec4& boldColor) {
  if (line.find("**") == std::string::npos) {
    ImGui::PushStyleColor(ImGuiCol_Text, normalColor);
    ImGui::TextWrapped("%s", line.c_str());
    ImGui::PopStyleColor();
    return;
  }

  std::string clean;
  clean.reserve(line.size());
  bool insideBold = false;
  size_t boldChars = 0;
  size_t totalChars = 0;
  size_t i = 0;
  while (i < line.size()) {
    if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
      insideBold = !insideBold;
      i += 2;
      continue;
    }
    clean += line[i];
    totalChars++;
    if (insideBold) boldChars++;
    i++;
  }

  const bool majorityBold = (totalChars > 0 && boldChars * 2 >= totalChars);
  ImGui::PushStyleColor(ImGuiCol_Text, majorityBold ? boldColor : normalColor);
  ImGui::TextWrapped("%s", clean.c_str());
  ImGui::PopStyleColor();
}

}  // namespace

void renderMarkdown(const std::string& text) {
  const ImVec4 colorNormal = ImGui::GetStyleColorVec4(ImGuiCol_Text);
  const ImVec4 colorBold   = ImVec4(1.0f, 0.9f, 0.5f, 1.0f);
  const ImVec4 colorH1     = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
  const ImVec4 colorH2     = ImVec4(0.5f, 0.85f, 0.95f, 1.0f);
  const ImVec4 colorH3     = ImVec4(0.6f, 0.9f, 0.9f, 1.0f);
  const ImVec4 colorBullet = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

  std::istringstream stream(text);
  std::string line;
  bool prevBlank = false;

  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();

    if (line.empty()) {
      if (!prevBlank) ImGui::Spacing();
      prevBlank = true;
      continue;
    }
    prevBlank = false;

    std::string headingContent;
    const int level = detectHeading(line, headingContent);
    if (level > 0) {
      std::string clean;
      for (size_t i = 0; i < headingContent.size(); ++i) {
        if (i + 1 < headingContent.size() &&
            headingContent[i] == '*' && headingContent[i + 1] == '*') {
          i++;
          continue;
        }
        clean += headingContent[i];
      }
      ImGui::Spacing();
      const float scale = (level == 1) ? 1.4f : (level == 2) ? 1.2f : 1.05f;
      const ImVec4 col = (level == 1) ? colorH1 : (level == 2) ? colorH2 : colorH3;
      ImGui::SetWindowFontScale(scale);
      ImGui::PushStyleColor(ImGuiCol_Text, col);
      ImGui::TextWrapped("%s", clean.c_str());
      ImGui::PopStyleColor();
      ImGui::SetWindowFontScale(1.0f);
      if (level <= 2) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        ImVec4 dimCol = col;
        dimCol.w = 0.3f;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(p.x, p.y - 2), ImVec2(p.x + w, p.y - 2),
            ImGui::GetColorU32(dimCol), 1.0f);
      }
      continue;
    }

    if ((line.size() >= 2) && (line[0] == '-' || line[0] == '*') && line[1] == ' ') {
      const std::string content = line.substr(2);
      ImGui::PushStyleColor(ImGuiCol_Text, colorBullet);
      ImGui::TextUnformatted("  \xE2\x80\xA2");
      ImGui::PopStyleColor();
      ImGui::SameLine();
      renderLineWithBold(content, colorNormal, colorBold);
      continue;
    }

    if (line.size() >= 3 && line[0] >= '1' && line[0] <= '9' && line[1] == '.') {
      const size_t sp = line.find(' ');
      if (sp != std::string::npos && sp < 5) {
        const std::string prefix = line.substr(0, sp + 1);
        const std::string content = line.substr(sp + 1);
        ImGui::PushStyleColor(ImGuiCol_Text, colorBullet);
        ImGui::TextUnformatted(prefix.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        renderLineWithBold(content, colorNormal, colorBold);
        continue;
      }
    }

    if (line.size() >= 3 &&
        (line == "---" || line == "***" || line == "___" ||
         line.find_first_not_of('-') == std::string::npos ||
         line.find_first_not_of('*') == std::string::npos)) {
      ImGui::Separator();
      continue;
    }

    renderLineWithBold(line, colorNormal, colorBold);
  }
}

}  // namespace asteria::ui::markdown_render