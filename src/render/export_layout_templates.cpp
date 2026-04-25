#include "export_layout_templates.h"

namespace asteria::render {

ChartScene buildReferenceSheetScene(const ChartScene& baseScene,
                                    const ThemePreset& theme,
                                    const std::string& title,
                                    const std::string& subtitle,
                                    const std::string& footer,
                                    const std::vector<ReferenceSheetSection>& sections) {
  ChartScene scene = baseScene;
  const double margin = 72.0;
  const double headerHeight = 92.0;
  const double footerHeight = 44.0;
  const double factColumnWidth = sections.empty() ? 0.0 : 420.0;
  const double factColumnGap = sections.empty() ? 0.0 : 64.0;

  scene.width = baseScene.width + static_cast<int>(margin * 2.0 + factColumnWidth + factColumnGap);
  scene.height = baseScene.height + static_cast<int>(margin * 2.0 + headerHeight + footerHeight);
  scene.background = {252, 249, 242};

  for (auto& c : scene.circles) {
    c.cx += margin;
    c.cy += margin + headerHeight;
  }
  for (auto& l : scene.lines) {
    l.x1 += margin; l.x2 += margin;
    l.y1 += margin + headerHeight; l.y2 += margin + headerHeight;
  }
  for (auto& t : scene.texts) {
    t.x += margin;
    t.y += margin + headerHeight;
  }

  scene.lines.push_back({margin, margin + headerHeight - 20.0,
                         static_cast<double>(scene.width) - margin, margin + headerHeight - 20.0,
                         theme.secondary, theme.thin, "layout", "layout-rule-top"});
  scene.lines.push_back({margin, static_cast<double>(scene.height) - margin - footerHeight,
                         static_cast<double>(scene.width) - margin,
                         static_cast<double>(scene.height) - margin - footerHeight,
                         theme.secondary, theme.thin, "layout", "layout-rule-bottom"});

  if (!sections.empty()) {
    const double dividerX = margin + static_cast<double>(baseScene.width) + factColumnGap * 0.5;
    scene.lines.push_back({dividerX,
                           margin + headerHeight + 8.0,
                           dividerX,
                           static_cast<double>(scene.height) - margin - footerHeight - 24.0,
                           theme.secondary,
                           theme.thin,
                           "layout",
                           "layout-divider"});
  }

  scene.texts.push_back({margin, margin + 26.0, title, 28, "start", theme.text, "layout", "layout-title"});
  scene.texts.push_back({margin, margin + 56.0, subtitle, 16, "start", theme.secondary, "layout", "layout-subtitle"});
  scene.texts.push_back({margin,
                         static_cast<double>(scene.height) - margin - 12.0,
                         footer,
                         12,
                         "start",
                         theme.secondary,
                         "layout",
                         "layout-footer"});

  if (!sections.empty()) {
    const double factX = margin + static_cast<double>(baseScene.width) + factColumnGap;
    double y = margin + headerHeight + 18.0;
    for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex) {
      const auto& section = sections[sectionIndex];
      scene.texts.push_back({factX,
                             y,
                             section.title,
                             16,
                             "start",
                             theme.accent,
                             "layout",
                             "layout-section-" + std::to_string(sectionIndex)});
      y += 24.0;

      for (size_t factIndex = 0; factIndex < section.facts.size(); ++factIndex) {
        const auto& fact = section.facts[factIndex];
        scene.texts.push_back({factX,
                               y,
                               fact.label + ": " + fact.value,
                               13,
                               "start",
                               theme.text,
                               "layout",
                               "layout-fact-" + std::to_string(sectionIndex) + "-" + std::to_string(factIndex)});
        y += 19.0;
      }

      y += 18.0;
    }
  }
  return scene;
}

}  // namespace asteria::render