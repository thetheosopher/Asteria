#include "svg_serializer.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>

namespace asteria::render {

namespace {

// Deterministic numeric formatting (3 decimal places, fixed point, classic locale).
std::string fmt(double v) {
  std::ostringstream o;
  o.imbue(std::locale::classic());
  o << std::fixed << std::setprecision(3) << v;
  return o.str();
}

// XML-attribute-safe escape.
std::string esc(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default:  out += c;
    }
  }
  return out;
}

std::string idAttr(const std::string& id) {
  return id.empty() ? std::string{} : (R"( id=")" + esc(id) + R"(")");
}

void writeCircle(std::ostringstream& out, const CircleElement& c) {
  out << "    <circle"
      << idAttr(c.id)
      << R"( cx=")" << fmt(c.cx) << R"(")"
      << R"( cy=")" << fmt(c.cy) << R"(")"
      << R"( r=")"  << fmt(c.radius) << R"(")"
      << R"( stroke=")" << c.stroke.toHex() << R"(")"
      << R"( stroke-width=")" << fmt(c.strokeWidth) << R"(")"
      << R"( fill=")" << esc(c.fill) << R"("/>)" << '\n';
}

void writeLine(std::ostringstream& out, const LineElement& l) {
  out << "    <line"
      << idAttr(l.id)
      << R"( x1=")" << fmt(l.x1) << R"(")"
      << R"( y1=")" << fmt(l.y1) << R"(")"
      << R"( x2=")" << fmt(l.x2) << R"(")"
      << R"( y2=")" << fmt(l.y2) << R"(")"
      << R"( stroke=")" << l.stroke.toHex() << R"(")"
      << R"( stroke-width=")" << fmt(l.strokeWidth) << R"("/>)" << '\n';
}

void writeText(std::ostringstream& out, const TextElement& t) {
  out << "    <text"
      << idAttr(t.id)
      << R"( x=")" << fmt(t.x) << R"(")"
      << R"( y=")" << fmt(t.y) << R"(")"
      << R"( font-family="Segoe UI Symbol, Segoe UI, Arial, sans-serif")"
      << R"( font-size=")" << t.fontSize << R"(")"
      << R"( text-anchor=")" << esc(t.anchor) << R"(")"
      << R"( fill=")" << t.fill.toHex() << R"(">)"
      << esc(t.content)
      << R"(</text>)" << '\n';
}

}  // namespace

std::string serializeSvg(const ChartScene& scene) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" width=")" << scene.width
      << R"(" height=")" << scene.height
      << R"(" viewBox="0 0 )" << scene.width << ' ' << scene.height << R"(">)" << '\n';

  // Archival metadata as an SVG comment (does not affect rendering).
  if (!scene.canonicalHash.empty() || !scene.engineMethod.empty()) {
    out << "  <!-- Asteria chart"
        << " hash=" << esc(scene.canonicalHash)
        << " engine=" << esc(scene.engineMethod)
        << " houses=" << esc(scene.houseSystem)
        << " zodiac=" << esc(scene.zodiacMode)
        << " -->" << '\n';
  }

  out << R"(  <rect width="100%" height="100%" fill="white"/>)" << '\n';

  // Group elements by layer name. Within a layer the rendering order is
  // circles → lines → texts which preserves the intended visual stacking.
  std::map<std::string, std::vector<const CircleElement*>> circByLayer;
  std::map<std::string, std::vector<const LineElement*>>   lineByLayer;
  std::map<std::string, std::vector<const TextElement*>>   textByLayer;
  for (const auto& c : scene.circles) circByLayer[c.layer].push_back(&c);
  for (const auto& l : scene.lines)   lineByLayer[l.layer].push_back(&l);
  for (const auto& t : scene.texts)   textByLayer[t.layer].push_back(&t);

  // Collect the union of named layers, sorted alphabetically. Default
  // (unnamed) layer goes last so it composites on top.
  std::vector<std::string> namedLayers;
  for (const auto& kv : circByLayer) if (!kv.first.empty()) namedLayers.push_back(kv.first);
  for (const auto& kv : lineByLayer) if (!kv.first.empty()) namedLayers.push_back(kv.first);
  for (const auto& kv : textByLayer) if (!kv.first.empty()) namedLayers.push_back(kv.first);
  std::sort(namedLayers.begin(), namedLayers.end());
  namedLayers.erase(std::unique(namedLayers.begin(), namedLayers.end()), namedLayers.end());

  auto writeLayer = [&](const std::string& layer) {
    auto cit = circByLayer.find(layer);
    auto lit = lineByLayer.find(layer);
    auto tit = textByLayer.find(layer);
    if (cit == circByLayer.end() && lit == lineByLayer.end() && tit == textByLayer.end())
      return;

    out << R"(  <g class=")" << esc(layer.empty() ? std::string("default") : layer)
        << R"(">)" << '\n';
    if (cit != circByLayer.end()) for (auto* c : cit->second) writeCircle(out, *c);
    if (lit != lineByLayer.end()) for (auto* l : lit->second) writeLine(out, *l);
    if (tit != textByLayer.end()) for (auto* t : tit->second) writeText(out, *t);
    out << "  </g>" << '\n';
  };

  for (const auto& layer : namedLayers) writeLayer(layer);
  writeLayer("");

  out << R"(</svg>)" << '\n';
  return out.str();
}

bool writeSvgFile(const ChartScene& scene, const std::string& filePath) {
  std::ofstream file(filePath, std::ios::binary);
  if (!file) return false;
  file << serializeSvg(scene);
  return true;
}

}  // namespace asteria::render
