#include "theme_presets.h"

namespace asteria::render {

ThemePreset textbookLight() {
  return {"Textbook Light", {247,243,234}, {78,66,52}, {132,120,104}, {32,97,143}, {42,35,28}, 1.0, 1.6, 2.4};
}

ThemePreset textbookMonochrome() {
  return {"Textbook Monochrome", {242,240,236}, {38,38,38}, {112,112,112}, {150,150,150}, {24,24,24}, 1.0, 1.4, 2.0};
}

ThemePreset luxuryLight() {
  return {"Luxury Light", {243,237,226}, {54,44,36}, {164,145,110}, {194,164,96}, {40,32,26}, 1.0, 1.8, 2.8};
}

ThemePreset luxuryDark() {
  return {"Luxury Dark", {20,18,22}, {212,204,188}, {128,116,86}, {199,168,92}, {232,228,220}, 1.0, 1.8, 2.8};
}

}  // namespace asteria::render
