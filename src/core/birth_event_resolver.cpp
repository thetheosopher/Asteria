#include "birth_event_resolver.h"
#include <cstdio>
#include <ctime>

namespace asteria::core {

namespace {

// Parse "YYYY-MM-DD" → fills y/m/d. Returns false on malformed input.
bool parseDate(const std::string& s, int& y, int& m, int& d) {
  if (s.size() < 10) return false;
  return std::sscanf(s.c_str(), "%4d-%2d-%2d", &y, &m, &d) == 3;
}

// Parse "HH:MM[:SS]" → decimal hours. Returns 12.0 (noon) on malformed input.
double parseTimeToHours(const std::string& s) {
  int hh = 12, mm = 0, ss = 0;
  int n = std::sscanf(s.c_str(), "%d:%d:%d", &hh, &mm, &ss);
  if (n < 2) return 12.0;
  return hh + mm / 60.0 + ss / 3600.0;
}

}  // namespace

domain::ResolvedBirthInput resolveBirthInput(
    const domain::BirthEvent& event,
    const std::optional<domain::LocationResolution>& location) {
  domain::ResolvedBirthInput out;

  // Date
  parseDate(event.birthDate, out.year, out.month, out.day);

  // Time — use noon when unknown
  if (event.birthTime &&
      event.timeAccuracy != domain::TimeAccuracy::Unknown) {
    out.timeHours = parseTimeToHours(*event.birthTime);
  } else {
    out.timeHours = 12.0;
  }

  // Coordinates / timezone — prefer Location row, fall back to direct fields
  if (location) {
    out.latitudeDeg = location->latitude;
    out.longitudeDeg = location->longitude;
    // Location.timezoneOffsetRuleRef may carry a numeric string like "-7";
    // we don't yet parse zone names, so callers should pre-resolve.
    if (!location->timezoneOffsetRuleRef.empty()) {
      double tz = 0.0;
      if (std::sscanf(location->timezoneOffsetRuleRef.c_str(), "%lf", &tz) == 1) {
        out.timezoneHours = tz;
      }
    }
  } else {
    if (event.latitudeDeg)  out.latitudeDeg  = *event.latitudeDeg;
    if (event.longitudeDeg) out.longitudeDeg = *event.longitudeDeg;
  }

  if (event.timezoneOffsetHours) out.timezoneHours = *event.timezoneOffsetHours;
  if (event.dstOffsetHours)      out.dstHours      = *event.dstOffsetHours;

  return out;
}

void applyUnknownTimePolicy(domain::ChartRequest& request,
                             const domain::BirthEvent& event,
                             const std::string& policy) {
  if (event.timeAccuracy != domain::TimeAccuracy::Unknown) return;

  if (policy == "noon_default_with_warning") {
    request.unknownTimePolicy = "noon_default_with_warning";
    request.includeHouses = true;
    if (request.primaryInput) {
      request.primaryInput->timeHours = 12.0;
    }
  } else {
    // Default: suppress houses
    request.unknownTimePolicy = "unknown_time_no_houses";
    request.includeHouses = false;
  }
}

std::optional<domain::ResolvedBirthInput> resolveTransitMoment(
    const std::string& iso,
    double latitudeDeg,
    double longitudeDeg,
    double timezoneHours) {
  domain::ResolvedBirthInput out;
  int y = 0, m = 0, d = 0, hh = 12, mm = 0, ss = 0;
  // Accept "YYYY-MM-DD", "YYYY-MM-DDTHH:MM" or "YYYY-MM-DDTHH:MM:SS"
  int n = std::sscanf(iso.c_str(), "%4d-%2d-%2dT%d:%d:%d", &y, &m, &d, &hh, &mm, &ss);
  if (n < 3) {
    n = std::sscanf(iso.c_str(), "%4d-%2d-%2d %d:%d:%d", &y, &m, &d, &hh, &mm, &ss);
  }
  if (n < 3) return std::nullopt;
  out.year = y; out.month = m; out.day = d;
  out.timeHours = hh + mm / 60.0 + ss / 3600.0;
  out.latitudeDeg = latitudeDeg;
  out.longitudeDeg = longitudeDeg;
  out.timezoneHours = timezoneHours;
  return out;
}

}  // namespace asteria::core
