#pragma once
#include <string>
#include <cstdint>

namespace asteria::domain {

struct ChartTheme {
  std::int64_t themeId = 0;
  std::string themeName;
  std::string styleFamily;
  std::string glyphSet;
  std::string lineWeightProfile;
  std::string aspectPalette;
  std::string typographyProfile;
  std::string backgroundMode;
  std::string createdAt;
  std::string updatedAt;
};

}  // namespace asteria::domain
