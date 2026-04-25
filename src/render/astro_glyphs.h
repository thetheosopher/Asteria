#pragma once
#include <string>
#include <unordered_map>

namespace asteria::render {

/// Astrological glyph mappings — Unicode codepoints for planets, signs, and aspects.
/// These render correctly with fonts that cover Miscellaneous Symbols (U+2600) block.
namespace glyphs {

// Planet glyphs (Unicode Miscellaneous Symbols)
inline const char* planet(const std::string& objectId) {
  static const std::unordered_map<std::string, const char*> map = {
    {"Sun",      "\xe2\x98\x89"},  // ☉ U+2609
    {"Moon",     "\xe2\x98\xbd"},  // ☽ U+263D
    {"Mercury",  "\xe2\x98\xbf"},  // ☿ U+263F
    {"Venus",    "\xe2\x99\x80"},  // ♀ U+2640
    {"Mars",     "\xe2\x99\x82"},  // ♂ U+2642
    {"Jupiter",  "\xe2\x99\x83"},  // ♃ U+2643
    {"Saturn",   "\xe2\x99\x84"},  // ♄ U+2644
    {"Uranus",   "\xe2\x99\x85"},  // ♅ U+2645
    {"Neptune",  "\xe2\x99\x86"},  // ♆ U+2646
    {"Pluto",    "\xe2\x99\x87"},  // ♇ U+2647
    // Nodes and points
    {"NNode",    "\xe2\x98\x8a"},  // ☊ U+260A
    {"SNode",    "\xe2\x98\x8b"},  // ☋ U+260B
    {"Chiron",   "\xe2\x9a\xb7"},  // ⚷ U+26B7
  };
  auto it = map.find(objectId);
  return it != map.end() ? it->second : nullptr;
}

// Zodiac sign glyphs (U+2648–U+2653)
inline const char* sign(int signIndex) {
  // signIndex: 0=Aries..11=Pisces
  static const char* signs[] = {
    "\xe2\x99\x88",  // ♈ Aries   U+2648
    "\xe2\x99\x89",  // ♉ Taurus  U+2649
    "\xe2\x99\x8a",  // ♊ Gemini  U+264A
    "\xe2\x99\x8b",  // ♋ Cancer  U+264B
    "\xe2\x99\x8c",  // ♌ Leo     U+264C
    "\xe2\x99\x8d",  // ♍ Virgo   U+264D
    "\xe2\x99\x8e",  // ♎ Libra   U+264E
    "\xe2\x99\x8f",  // ♏ Scorpio U+264F
    "\xe2\x99\x90",  // ♐ Sagitt. U+2650
    "\xe2\x99\x91",  // ♑ Capric. U+2651
    "\xe2\x99\x92",  // ♒ Aquar.  U+2652
    "\xe2\x99\x93",  // ♓ Pisces  U+2653
  };
  if (signIndex >= 0 && signIndex < 12) return signs[signIndex];
  return "?";
}

// Sign glyph by name
inline const char* signByName(const std::string& name) {
  static const std::unordered_map<std::string, int> nameToIndex = {
    {"Aries", 0}, {"Taurus", 1}, {"Gemini", 2}, {"Cancer", 3},
    {"Leo", 4}, {"Virgo", 5}, {"Libra", 6}, {"Scorpio", 7},
    {"Sagittarius", 8}, {"Capricorn", 9}, {"Aquarius", 10}, {"Pisces", 11},
    // Short forms from Astrolog
    {"Ari", 0}, {"Tau", 1}, {"Gem", 2}, {"Can", 3},
    {"Leo", 4}, {"Vir", 5}, {"Lib", 6}, {"Sco", 7},
    {"Sag", 8}, {"Cap", 9}, {"Aqu", 10}, {"Pis", 11},
  };
  auto it = nameToIndex.find(name);
  if (it != nameToIndex.end()) return sign(it->second);
  return "?";
}

// Aspect glyphs
inline const char* aspect(const std::string& aspectType) {
  static const std::unordered_map<std::string, const char*> map = {
    {"Conjunction",    "\xe2\x98\x8c"},  // ☌ U+260C
    {"Opposition",     "\xe2\x98\x8d"},  // ☍ U+260D
    {"Trine",          "\xe2\x96\xb3"},  // △ U+25B3
    {"Square",         "\xe2\x96\xa1"},  // □ U+25A1
    {"Sextile",        "\xe2\x9c\xb6"},  // ✶ U+2736 (sextile star)
    {"Quincunx",       "Qx"},
    {"SemiSextile",    "SSx"},
    {"SemiSquare",     "SSq"},
    {"SesquiQuadrate", "Sq+"},
    {"Quintile",       "Q"},
    {"BiQuintile",     "bQ"},
  };
  auto it = map.find(aspectType);
  return it != map.end() ? it->second : aspectType.c_str();
}

// Element colors for sign bands (fire/earth/air/water)
enum class Element { Fire, Earth, Air, Water };

inline Element signElement(int zeroBasedIndex) {
  // Fire: Ari(0), Leo(4), Sag(8)
  // Earth: Tau(1), Vir(5), Cap(9)
  // Air: Gem(2), Lib(6), Aqu(10)
  // Water: Can(3), Sco(7), Pis(11)
  static const Element elems[12] = {
    Element::Fire, Element::Earth, Element::Air, Element::Water,
    Element::Fire, Element::Earth, Element::Air, Element::Water,
    Element::Fire, Element::Earth, Element::Air, Element::Water,
  };
  if (zeroBasedIndex >= 0 && zeroBasedIndex < 12) return elems[zeroBasedIndex];
  return Element::Fire;
}

/// Format planet position as degree°sign'minute (e.g. "15°♈32'")
inline std::string formatPosition(double longitude, const std::string& signName) {
  double signDeg = std::fmod(longitude, 30.0);
  int deg = static_cast<int>(signDeg);
  int min = static_cast<int>((signDeg - deg) * 60.0);
  const char* signGlyph = signByName(signName);
  char buf[32];
  snprintf(buf, sizeof(buf), "%d%s%s%02d'", deg, "\xc2\xb0", signGlyph, min);
  return buf;
}

/// Format as degree°minute' without sign glyph
inline std::string formatDegMin(double longitude) {
  double signDeg = std::fmod(longitude, 30.0);
  int deg = static_cast<int>(signDeg);
  int min = static_cast<int>((signDeg - deg) * 60.0);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d\xc2\xb0%02d'", deg, min);
  return buf;
}

}  // namespace glyphs
}  // namespace asteria::render
