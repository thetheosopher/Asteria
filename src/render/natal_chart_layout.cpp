#include "natal_chart_layout.h"
#include "astro_glyphs.h"
#include <cmath>
#include <cstdio>

#include <array>

namespace asteria::render {

static constexpr double kPi = 3.14159265358979323846;

static double degToRad(double deg) { return deg * kPi / 180.0; }

static bool shouldRenderOnWheel(const std::string& objectId) {
  static const std::array<const char*, 10> kWheelObjects = {
      "Sun", "Moon", "Mercury", "Venus", "Mars",
      "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto",
  };
  for (const char* name : kWheelObjects) {
    if (objectId == name) return true;
  }
  return false;
}

static std::pair<double, double> polarPoint(double cx, double cy, double radius, double angle) {
  return {cx + std::cos(angle) * radius, cy + std::sin(angle) * radius};
}

ChartScene buildNatalChartScene(const asteria::domain::ComputedChart& chart, const ThemePreset& theme) {
  ChartScene scene;
  scene.width = 1000;
  scene.height = 1000;
  scene.background = theme.background;
  scene.canonicalHash = chart.canonicalHash;
  scene.engineMethod  = chart.engineMethod;
  scene.houseSystem   = chart.houseSystem;
  scene.zodiacMode    = chart.zodiacMode;

  const double cx = 500.0;
  const double cy = 500.0;
  const double outerR = 420.0;
  const double innerR = 300.0;
  const double houseR = 220.0;

  // --- Wheel rings ---
  {
    CircleElement c{cx, cy, outerR, theme.primary, theme.thick, "none"};
    c.layer = "rings"; c.id = "ring-outer"; scene.circles.push_back(c);
  }
  {
    CircleElement c{cx, cy, innerR, theme.secondary, theme.normal, "none"};
    c.layer = "rings"; c.id = "ring-inner"; scene.circles.push_back(c);
  }
  {
    CircleElement c{cx, cy, houseR, theme.secondary, theme.thin, "none"};
    c.layer = "rings"; c.id = "ring-house"; scene.circles.push_back(c);
  }

  // --- Sign division lines & glyph labels ---
  for (int i = 0; i < 12; ++i) {
    const double angle = degToRad(-90.0 + i * 30.0);
    const double x1 = cx + std::cos(angle) * outerR;
    const double y1 = cy + std::sin(angle) * outerR;
    const double x2 = cx + std::cos(angle) * houseR;
    const double y2 = cy + std::sin(angle) * houseR;
    LineElement l{x1, y1, x2, y2, theme.secondary, theme.thin};
    l.layer = "signs";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "sign-div-%d", i);
    l.id = buf;
    scene.lines.push_back(l);
  }
  for (int i = 0; i < 12; ++i) {
    const double angle = degToRad(-75.0 + i * 30.0);
    const double x = cx + std::cos(angle) * ((outerR + innerR) / 2.0);
    const double y = cy + std::sin(angle) * ((outerR + innerR) / 2.0);
    TextElement t{x, y, glyphs::sign(i), 22, "middle", theme.text};
    t.layer = "signs";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "sign-%d", i);
    t.id = buf;
    scene.texts.push_back(t);
  }

  // --- House cusps ---
  for (const auto& cusp : chart.houseCusps) {
    const double angle = degToRad(-90.0 + cusp.longitudeDegrees);
    const double x1 = cx + std::cos(angle) * innerR;
    const double y1 = cy + std::sin(angle) * innerR;
    const double x2 = cx + std::cos(angle) * houseR;
    const double y2 = cy + std::sin(angle) * houseR;
    bool isAxis = (cusp.houseNumber == 1 || cusp.houseNumber == 4 ||
                   cusp.houseNumber == 7 || cusp.houseNumber == 10);
    LineElement l{x1, y1, x2, y2, theme.primary,
                  isAxis ? theme.thick : theme.normal};
    l.layer = "houses";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "cusp-%d", cusp.houseNumber);
    l.id = buf;
    scene.lines.push_back(l);
  }

  // --- Planet glyphs and labels ---
  for (const auto& planet : chart.planets) {
    if (!shouldRenderOnWheel(planet.objectId)) continue;
    const double angle = degToRad(-90.0 + planet.longitudeDegrees);
    const double glyphR = 248.0;
    const double degreeR = 230.0;
    const auto [x, y] = polarPoint(cx, cy, glyphR, angle);
    const auto [degreeX, degreeY] = polarPoint(cx, cy, degreeR, angle);
    const char* glyph = glyphs::planet(planet.objectId);
    std::string label = glyph ? glyph : planet.objectId;
    {
      TextElement t{x, y, label, 20, "middle", theme.accent};
      t.layer = "planets";
      t.id = "planet-" + planet.objectId;
      scene.texts.push_back(t);
    }
    {
      TextElement t{degreeX, degreeY, glyphs::formatDegMin(planet.longitudeDegrees),
                    11, "middle", theme.secondary};
      t.layer = "planets";
      t.id = "planet-" + planet.objectId + "-deg";
      scene.texts.push_back(t);
    }
    if (planet.retrograde) {
      TextElement t{x + 14.0, y - 6.0, "R", 10, "start", {200, 60, 40}};
      t.layer = "planets";
      t.id = "planet-" + planet.objectId + "-rx";
      scene.texts.push_back(t);
    }
  }

  // --- Aspect lines (drawn inside the inner circle) ---
  if (!chart.aspects.empty()) {
    auto findLon = [&](const std::string& name) -> double {
      for (const auto& p : chart.planets) {
        if (p.objectId == name) return p.longitudeDegrees;
      }
      return 0.0;
    };
    int idx = 0;
    for (const auto& asp : chart.aspects) {
      if (!shouldRenderOnWheel(asp.objectA) || !shouldRenderOnWheel(asp.objectB)) continue;
      double angA = degToRad(-90.0 + findLon(asp.objectA));
      double angB = degToRad(-90.0 + findLon(asp.objectB));
      double r = houseR - 6.0;
      double x1 = cx + std::cos(angA) * r;
      double y1 = cy + std::sin(angA) * r;
      double x2 = cx + std::cos(angB) * r;
      double y2 = cy + std::sin(angB) * r;
      Color col = theme.secondary;
      if (asp.aspectType == "Conjunction" || asp.aspectType == "Opposition" ||
          asp.aspectType == "Square") col = theme.primary;
      else if (asp.aspectType == "Trine" || asp.aspectType == "Sextile")
        col = theme.accent;
      LineElement l{x1, y1, x2, y2, col, theme.thin};
      l.layer = "aspects";
      char buf[32];
      std::snprintf(buf, sizeof(buf), "aspect-%d", idx++);
      l.id = buf;
      scene.lines.push_back(l);
    }
  }

  if (!chart.uncertaintyFlags.empty()) {
    TextElement t{cx, 175.0,
                  "Warning: uncertain birth-time assumptions present",
                  14, "middle", {180, 60, 40}};
    t.layer = "title"; t.id = "title-warning";
    scene.texts.push_back(t);
  }

  return scene;
}

}  // namespace asteria::render
