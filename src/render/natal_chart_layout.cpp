#include "natal_chart_layout.h"
#include <cmath>

namespace asteria::render {

static constexpr double kPi = 3.14159265358979323846;

static double degToRad(double deg) {
  return deg * kPi / 180.0;
}

ChartScene buildNatalChartScene(const asteria::domain::ComputedChart& chart, const ThemePreset& theme) {
  ChartScene scene;
  scene.width = 1000;
  scene.height = 1000;

  const double cx = 500.0;
  const double cy = 500.0;
  const double outerR = 420.0;
  const double innerR = 300.0;
  const double houseR = 220.0;

  scene.circles.push_back({cx, cy, outerR, theme.primary, theme.thick, "none"});
  scene.circles.push_back({cx, cy, innerR, theme.secondary, theme.normal, "none"});
  scene.circles.push_back({cx, cy, houseR, theme.secondary, theme.thin, "none"});

  for (int i = 0; i < 12; ++i) {
    const double angle = degToRad(-90.0 + i * 30.0);
    const double x1 = cx + std::cos(angle) * outerR;
    const double y1 = cy + std::sin(angle) * outerR;
    const double x2 = cx + std::cos(angle) * houseR;
    const double y2 = cy + std::sin(angle) * houseR;
    scene.lines.push_back({x1, y1, x2, y2, theme.secondary, theme.thin});
  }

  static const char* signs[12] = {"Ar", "Ta", "Ge", "Cn", "Le", "Vi", "Li", "Sc", "Sg", "Cp", "Aq", "Pi"};
  for (int i = 0; i < 12; ++i) {
    const double angle = degToRad(-75.0 + i * 30.0);
    const double x = cx + std::cos(angle) * ((outerR + innerR) / 2.0);
    const double y = cy + std::sin(angle) * ((outerR + innerR) / 2.0);
    scene.texts.push_back({x, y, signs[i], 18, "middle", theme.text});
  }

  for (const auto& cusp : chart.houseCusps) {
    const double angle = degToRad(-90.0 + cusp.longitudeDegrees);
    const double x1 = cx + std::cos(angle) * innerR;
    const double y1 = cy + std::sin(angle) * innerR;
    const double x2 = cx + std::cos(angle) * houseR;
    const double y2 = cy + std::sin(angle) * houseR;
    scene.lines.push_back({x1, y1, x2, y2, theme.primary, theme.normal});
  }

  for (const auto& planet : chart.planets) {
    const double angle = degToRad(-90.0 + planet.longitudeDegrees);
    const double r = 265.0;
    const double x = cx + std::cos(angle) * r;
    const double y = cy + std::sin(angle) * r;
    scene.texts.push_back({x, y, planet.objectId, 16, "middle", theme.accent});
  }

  scene.texts.push_back({cx, 115.0, "Asteria Sample Natal Chart", 28, "middle", theme.text});
  scene.texts.push_back({cx, 145.0, chart.houseSystem + " / " + chart.zodiacMode, 16, "middle", theme.secondary});

  if (!chart.uncertaintyFlags.empty()) {
    scene.texts.push_back({cx, 175.0, "Warning: uncertain birth-time assumptions present", 14, "middle", {180, 60, 40}});
  }

  return scene;
}

}  // namespace asteria::render
