#pragma once
#include <string>
#include <vector>

namespace asteria::render {

struct Color {
  int r = 0;
  int g = 0;
  int b = 0;
  std::string toHex() const;
};

struct CircleElement {
  double cx = 0.0;
  double cy = 0.0;
  double radius = 0.0;
  Color stroke;
  double strokeWidth = 1.0;
  std::string fill = "none";
  std::string layer;       // optional: SVG group name for archival/automation
  std::string id;          // optional: stable element id
};

struct LineElement {
  double x1 = 0.0;
  double y1 = 0.0;
  double x2 = 0.0;
  double y2 = 0.0;
  Color stroke;
  double strokeWidth = 1.0;
  std::string layer;
  std::string id;
};

struct TextElement {
  double x = 0.0;
  double y = 0.0;
  std::string content;
  int fontSize = 12;
  std::string anchor = "middle";
  Color fill;
  std::string layer;
  std::string id;
};

struct ChartScene {
  int width = 1000;
  int height = 1000;
  Color background{255, 255, 255};
  std::vector<CircleElement> circles;
  std::vector<LineElement> lines;
  std::vector<TextElement> texts;

  /// Optional metadata embedded as an SVG comment for archival/automation.
  std::string canonicalHash;
  std::string engineMethod;
  std::string houseSystem;
  std::string zodiacMode;
};

}  // namespace asteria::render
