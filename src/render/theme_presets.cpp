#include "theme_presets.h"

namespace asteria::render {

ThemePreset textbookLight() {
  return {"Textbook Light", {30,30,30}, {100,100,100}, {0,102,153}, {20,20,20}, 1.0, 1.6, 2.4};
}

ThemePreset textbookMonochrome() {
  return {"Textbook Monochrome", {20,20,20}, {80,80,80}, {120,120,120}, {10,10,10}, 1.0, 1.4, 2.0};
}

ThemePreset luxuryLight() {
  return {"Luxury Light", {32,30,38}, {160,145,110}, {194,164,96}, {32,30,38}, 1.0, 1.8, 2.8};
}

ThemePreset luxuryDark() {
  return {"Luxury Dark", {212,204,188}, {128,116,86}, {199,168,92}, {232,228,220}, 1.0, 1.8, 2.8};
}

}  // namespace asteria::render
