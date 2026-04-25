#pragma once

#include "imgui.h"

#include <filesystem>
#include <string>

struct ImFont;
struct ImGuiIO;

namespace asteria::ui::astrology_font {

void load(ImGuiIO& io, const std::filesystem::path& runtimeDir);

ImFont* wheelGlyphFont();
ImFont* inlineGlyphFont();
bool usingLegacyWheelFont();

const char* planetGlyph(const std::string& objectId);
const char* signGlyph(int signIndex);
const char* aspectGlyph(const std::string& aspectType);
const char* retrogradeGlyph();

const char* wheelPlanetGlyph(const std::string& objectId);
const char* wheelSignGlyph(int signIndex);
const char* wheelRetrogradeGlyph();

void drawInlineGlyphLabel(const char* glyph,
                         const std::string& label,
                         ImU32 glyphColor,
                         ImU32 textColor,
                         float spacing = 5.0f);

}  // namespace asteria::ui::astrology_font