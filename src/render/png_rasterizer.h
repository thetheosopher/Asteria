#pragma once

#include <string>

#include "chart_scene.h"

namespace asteria::render {

bool writePngFile(const ChartScene& scene,
                  const std::string& filePath,
                  int widthPx,
                  int heightPx,
                  int dpi);

}  // namespace asteria::render