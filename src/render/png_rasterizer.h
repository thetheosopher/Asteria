#pragma once

#include <string>
#include <vector>

#include "chart_scene.h"

namespace asteria::render {

struct RasterImage {
    int widthPx = 0;
    int heightPx = 0;
    std::vector<unsigned char> rgb;
};

bool rasterizeSceneRgb(const ChartScene& scene,
                                             int widthPx,
                                             int heightPx,
                                             int dpi,
                                             RasterImage& outImage);

bool writePngFile(const ChartScene& scene,
                  const std::string& filePath,
                  int widthPx,
                  int heightPx,
                  int dpi);

}  // namespace asteria::render