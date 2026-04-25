#include "transit_chart_layout.h"
#include <cmath>

namespace asteria::render {

static constexpr double kPi = 3.14159265358979323846;

static double degToRad(double deg) {
  return deg * kPi / 180.0;
}

ChartScene buildTransitToNatalChartScene(const domain::ComputedChart& natalChart,
                                         const domain::ComputedChart& transitChart,
                                         const ThemePreset& theme) {
  ChartScene scene;
  scene.width = 1000;
  scene.height = 1000;
  scene.background = theme.background;

  const double cx = 500.0;
  const double cy = 500.0;
  const double outerR = 460.0;   // transit ring outer edge
  const double transitR = 380.0; // transit ring inner edge / sign ring outer
  const double innerR = 300.0;   // natal ring outer edge
  const double houseR = 220.0;   // natal ring inner edge

  // Rings
  scene.circles.push_back({cx, cy, outerR, theme.accent, theme.thick, "none"});
  scene.circles.push_back({cx, cy, transitR, theme.primary, theme.normal, "none"});
  scene.circles.push_back({cx, cy, innerR, theme.primary, theme.normal, "none"});
  scene.circles.push_back({cx, cy, houseR, theme.secondary, theme.thin, "none"});

  // Sign dividers
  for (int i = 0; i < 12; ++i) {
    const double angle = degToRad(-90.0 + i * 30.0);
    const double x1 = cx + std::cos(angle) * outerR;
    const double y1 = cy + std::sin(angle) * outerR;
    const double x2 = cx + std::cos(angle) * houseR;
    const double y2 = cy + std::sin(angle) * houseR;
    scene.lines.push_back({x1, y1, x2, y2, theme.secondary, theme.thin});
  }

  // Sign labels between transit and natal rings
  static const char* signs[12] = {"Ar", "Ta", "Ge", "Cn", "Le", "Vi", "Li", "Sc", "Sg", "Cp", "Aq", "Pi"};
  for (int i = 0; i < 12; ++i) {
    const double angle = degToRad(-75.0 + i * 30.0);
    const double x = cx + std::cos(angle) * ((transitR + innerR) / 2.0);
    const double y = cy + std::sin(angle) * ((transitR + innerR) / 2.0);
    scene.texts.push_back({x, y, signs[i], 16, "middle", theme.text});
  }

  // Natal house cusps
  for (const auto& cusp : natalChart.houseCusps) {
    const double angle = degToRad(-90.0 + cusp.longitudeDegrees);
    const double x1 = cx + std::cos(angle) * innerR;
    const double y1 = cy + std::sin(angle) * innerR;
    const double x2 = cx + std::cos(angle) * houseR;
    const double y2 = cy + std::sin(angle) * houseR;
    scene.lines.push_back({x1, y1, x2, y2, theme.primary, theme.thin});
  }

  // Natal planets (inner ring) - standard accent color
  for (const auto& planet : natalChart.planets) {
    const double angle = degToRad(-90.0 + planet.longitudeDegrees);
    const double r = 260.0;
    const double x = cx + std::cos(angle) * r;
    const double y = cy + std::sin(angle) * r;
    scene.texts.push_back({x, y, planet.objectId, 14, "middle", theme.accent});
  }

  // Transit planets (outer ring) - distinct color (primary) and prefixed with "t."
  Color transitColor = {theme.accent.r / 2, theme.accent.g / 2, theme.accent.b + (255 - theme.accent.b) / 2};
  for (const auto& planet : transitChart.planets) {
    const double angle = degToRad(-90.0 + planet.longitudeDegrees);
    const double r = 420.0;
    const double x = cx + std::cos(angle) * r;
    const double y = cy + std::sin(angle) * r;
    scene.texts.push_back({x, y, "t." + planet.objectId, 13, "middle", transitColor});
  }

  scene.texts.push_back({cx, 40.0, "Transit-to-Natal Chart", 28, "middle", theme.text});
  scene.texts.push_back({cx, 70.0, natalChart.houseSystem + " / " + natalChart.zodiacMode, 16, "middle", theme.secondary});

  // Uncertainty warnings
  bool hasWarning = !natalChart.uncertaintyFlags.empty() || !transitChart.uncertaintyFlags.empty();
  if (hasWarning) {
    scene.texts.push_back({cx, 100.0, "Warning: uncertain birth-time assumptions present", 14, "middle", {180, 60, 40}});
  }

  return scene;
}

}  // namespace asteria::render
