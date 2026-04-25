#pragma once
#include <string>
#include <cstdint>
#include "types.h"

namespace asteria::domain {

struct InterpretationNote {
  std::int64_t interpretationNoteId = 0;
  std::int64_t computedChartId = 0;
  InterpretationSourceType sourceType = InterpretationSourceType::BuiltIn;
  std::string promptVersion;
  bool certaintyGuardrailsApplied = false;
  std::string bodyMarkdown;
  std::string createdAt;
};

}  // namespace asteria::domain
