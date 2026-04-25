#pragma once

#include <string>
#include <vector>

#include "chart_scene.h"
#include "theme_presets.h"

namespace asteria::render {

struct ReferenceSheetFact {
    std::string label;
    std::string value;
};

struct ReferenceSheetSection {
    std::string title;
    std::vector<ReferenceSheetFact> facts;
};

ChartScene buildReferenceSheetScene(const ChartScene& baseScene,
                                    const ThemePreset& theme,
                                    const std::string& title,
                                    const std::string& subtitle,
                                                                        const std::string& footer,
                                                                        const std::vector<ReferenceSheetSection>& sections = {});

}  // namespace asteria::render