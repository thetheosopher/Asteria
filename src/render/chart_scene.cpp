#include "chart_scene.h"
#include <sstream>
#include <iomanip>

namespace asteria::render {

std::string Color::toHex() const {
  std::ostringstream out;
  out << '#'
      << std::hex << std::setw(2) << std::setfill('0') << (r & 0xFF)
      << std::setw(2) << std::setfill('0') << (g & 0xFF)
      << std::setw(2) << std::setfill('0') << (b & 0xFF);
  return out.str();
}

}  // namespace asteria::render
