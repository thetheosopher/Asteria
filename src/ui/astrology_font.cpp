#include "astrology_font.h"

#include "render/astro_glyphs.h"

#include <unordered_map>

namespace asteria::ui::astrology_font {

namespace fs = std::filesystem;

namespace {

struct FontState {
  ImFont* uiFont = nullptr;
  ImFont* inlineGlyph = nullptr;
  ImFont* wheelGlyph = nullptr;
  bool useLegacyWheelEncoding = false;
};

FontState g_fontState;

bool fileExists(const fs::path& path) {
  std::error_code ec;
  return fs::exists(path, ec) && !ec;
}

const char* legacyPlanetGlyph(const std::string& objectId) {
  static const std::unordered_map<std::string, const char*> kPlanetGlyphs = {
      {"Sun", "Q"},      {"Moon", "R"},    {"Mercury", "S"},
      {"Venus", "T"},    {"Mars", "U"},    {"Jupiter", "V"},
      {"Saturn", "W"},   {"Uranus", "X"},  {"Neptune", "Y"},
      {"Pluto", "Z"},    {"NNode", "<"},   {"SNode", ">"},
      {"Chiron", "t"},
  };
  if (auto it = kPlanetGlyphs.find(objectId); it != kPlanetGlyphs.end()) {
    return it->second;
  }
  return nullptr;
}

const char* legacySignGlyph(int signIndex) {
  static const char* kSigns[] = {
      "A", "B", "C", "D", "E", "F",
      "G", "H", "I", "J", "K", "L",
  };
  if (signIndex >= 1 && signIndex <= 12) return kSigns[signIndex - 1];
  if (signIndex >= 0 && signIndex < 12) return kSigns[signIndex];
  return nullptr;
}

const char* legacyAspectGlyph(const std::string& aspectType) {
  static const std::unordered_map<std::string, const char*> kAspectGlyphs = {
      {"Conjunction", "!"},
      {"Opposition", "\""},
      {"Square", "#"},
      {"Trine", "$"},
      {"Sextile", "'"},
      {"Quincunx", "&"},
      {"SemiSextile", "%"},
      {"SemiSquare", "("},
      {"SesquiQuadrate", ")"},
      {"Quintile", "+"},
      {"BiQuintile", "*"},
  };
  if (auto it = kAspectGlyphs.find(aspectType); it != kAspectGlyphs.end()) {
    return it->second;
  }
  return nullptr;
}

}  // namespace

void load(ImGuiIO& io, const std::filesystem::path& runtimeDir) {
  g_fontState = {};

  const char* segoeUI = "C:\\Windows\\Fonts\\segoeui.ttf";
  const char* segoeSymbol = "C:\\Windows\\Fonts\\seguisym.ttf";
  const fs::path astroPath = runtimeDir / "Astro.ttf";

  if (fileExists(segoeUI)) {
    g_fontState.uiFont = io.Fonts->AddFontFromFileTTF(segoeUI, 16.0f);
  } else {
    g_fontState.uiFont = io.Fonts->AddFontDefault();
  }

  if (fileExists(segoeSymbol)) {
    ImFontConfig mergeConfig;
    mergeConfig.MergeMode = true;
    mergeConfig.PixelSnapH = true;
    static const ImWchar glyphRanges[] = {
      0x2500, 0x26FF,
      0x2700, 0x27BF,
      0x00B0, 0x00B0,
      0,
    };
    io.Fonts->AddFontFromFileTTF(segoeSymbol, 16.0f, &mergeConfig, glyphRanges);
    g_fontState.inlineGlyph = io.Fonts->AddFontFromFileTTF(segoeSymbol, 17.0f);
  }
  if (!g_fontState.inlineGlyph) g_fontState.inlineGlyph = g_fontState.uiFont;

  if (fileExists(astroPath)) {
    const std::string astroPathString = astroPath.string();
    g_fontState.wheelGlyph = io.Fonts->AddFontFromFileTTF(astroPathString.c_str(), 24.0f);
    g_fontState.useLegacyWheelEncoding = g_fontState.wheelGlyph != nullptr;
  }

  if (!g_fontState.wheelGlyph) {
    const char* fallbackGlyphFont = fileExists(segoeSymbol) ? segoeSymbol : segoeUI;
    if (fallbackGlyphFont && fileExists(fallbackGlyphFont)) {
      if (!g_fontState.wheelGlyph) {
        g_fontState.wheelGlyph = io.Fonts->AddFontFromFileTTF(fallbackGlyphFont, 24.0f);
      }
    }
  }

  io.Fonts->Build();
}

ImFont* wheelGlyphFont() {
  return g_fontState.wheelGlyph;
}

ImFont* inlineGlyphFont() {
  return g_fontState.inlineGlyph;
}

bool usingLegacyAstroFont() {
  return g_fontState.useLegacyWheelEncoding;
}

const char* planetGlyph(const std::string& objectId) {
  return render::glyphs::planet(objectId);
}

const char* signGlyph(int signIndex) {
  return render::glyphs::sign(signIndex);
}

const char* aspectGlyph(const std::string& aspectType) {
  return render::glyphs::aspect(aspectType);
}

const char* retrogradeGlyph() {
  return "R";
}

bool usingLegacyWheelFont() {
  return g_fontState.useLegacyWheelEncoding;
}

const char* wheelPlanetGlyph(const std::string& objectId) {
  if (g_fontState.useLegacyWheelEncoding) {
    return legacyPlanetGlyph(objectId);
  }
  return planetGlyph(objectId);
}

const char* wheelSignGlyph(int signIndex) {
  if (g_fontState.useLegacyWheelEncoding) {
    return legacySignGlyph(signIndex);
  }
  return signGlyph(signIndex);
}

const char* wheelRetrogradeGlyph() {
  return g_fontState.useLegacyWheelEncoding ? "M" : "R";
}

void drawInlineGlyphLabel(const char* glyph,
                         const std::string& label,
                         ImU32 glyphColor,
                         ImU32 textColor,
                         float spacing) {
  const bool hasGlyph = glyph && *glyph;
  if (hasGlyph) {
    if (ImFont* font = inlineGlyphFont()) ImGui::PushFont(font);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(glyphColor));
    ImGui::TextUnformatted(glyph);
    ImGui::PopStyleColor();
    if (inlineGlyphFont()) ImGui::PopFont();
  }

  if (!label.empty()) {
    if (hasGlyph) ImGui::SameLine(0.0f, spacing);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(textColor));
    ImGui::TextUnformatted(label.c_str());
    ImGui::PopStyleColor();
  }
}

}  // namespace asteria::ui::astrology_font