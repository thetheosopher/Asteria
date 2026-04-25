#include "svg_serializer.h"
#include <fstream>
#include <sstream>

namespace asteria::render {

std::string serializeSvg(const ChartScene& scene) {
  std::ostringstream out;
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" width=")" << scene.width
      << R"(" height=")" << scene.height
      << R"(" viewBox="0 0 )" << scene.width << ' ' << scene.height << R"(">)" << '\n';
  out << R"(  <rect width="100%" height="100%" fill="white"/>)" << '\n';
  for (const auto& c : scene.circles) {
    out << R"(  <circle cx=")" << c.cx << R"(" cy=")" << c.cy << R"(" r=")" << c.radius
        << R"(" stroke=")" << c.stroke.toHex() << R"(" stroke-width=")" << c.strokeWidth
        << R"(" fill=")" << c.fill << R"("/>)" << '\n';
  }
  for (const auto& l : scene.lines) {
    out << R"(  <line x1=")" << l.x1 << R"(" y1=")" << l.y1
        << R"(" x2=")" << l.x2 << R"(" y2=")" << l.y2
        << R"(" stroke=")" << l.stroke.toHex()
        << R"(" stroke-width=")" << l.strokeWidth << R"("/>)" << '\n';
  }
  for (const auto& t : scene.texts) {
    out << R"(  <text x=")" << t.x << R"(" y=")" << t.y
        << R"(" font-family="Segoe UI, Arial, sans-serif" font-size=")" << t.fontSize
        << R"(" text-anchor=")" << t.anchor << R"(" fill=")" << t.fill.toHex() << R"(">)"
        << t.content << R"(</text>)" << '\n';
  }
  out << R"(</svg>)" << '\n';
  return out.str();
}

bool writeSvgFile(const ChartScene& scene, const std::string& filePath) {
  std::ofstream file(filePath, std::ios::binary);
  if (!file) {
    return false;
  }
  file << serializeSvg(scene);
  return true;
}

}  // namespace asteria::render
