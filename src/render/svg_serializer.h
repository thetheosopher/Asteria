#pragma once
#include <string>
#include "chart_scene.h"

namespace asteria::render {

std::string serializeSvg(const ChartScene& scene);
bool writeSvgFile(const ChartScene& scene, const std::string& filePath);

}  // namespace asteria::render
