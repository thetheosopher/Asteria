#pragma once
#include <string>
#include "chart_scene.h"

namespace asteria::render {

struct ThemePreset {
  std::string name;
  Color primary;
  Color secondary;
  Color accent;
  Color text;
  double thin = 1.0;
  double normal = 2.0;
  double thick = 3.0;
};

ThemePreset textbookLight();
ThemePreset textbookMonochrome();
ThemePreset luxuryLight();
ThemePreset luxuryDark();

}  // namespace asteria::render
