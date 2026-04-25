#include "atlas_service.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace asteria::util {

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

static std::string toLower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

/// Split a line on tabs.  Trailing \r is stripped.
static std::vector<std::string> splitTabs(const std::string& line) {
  std::vector<std::string> tokens;
  std::string::size_type start = 0;
  while (start <= line.size()) {
    auto pos = line.find('\t', start);
    if (pos == std::string::npos) {
      std::string tok = line.substr(start);
      // strip trailing \r
      if (!tok.empty() && tok.back() == '\r') tok.pop_back();
      tokens.push_back(std::move(tok));
      break;
    }
    tokens.push_back(line.substr(start, pos - start));
    start = pos + 1;
  }
  return tokens;
}

// Parse "H", "H:MM", "H:MM:SS", or "-H:MM:SS" into decimal hours.
double AtlasService::parseOffset(const std::string& s) {
  if (s.empty()) return 0.0;
  // Strip trailing 's' or 'u' time-type indicators
  std::string clean = s;
  if (!clean.empty() && (clean.back() == 's' || clean.back() == 'u'))
    clean.pop_back();

  bool negative = false;
  std::string::size_type idx = 0;
  if (clean[0] == '-') { negative = true; idx = 1; }

  int h = 0, m = 0, ss = 0;
  auto parts = 0;
  auto n = std::sscanf(clean.c_str() + idx, "%d:%d:%d", &h, &m, &ss);
  if (n >= 1) parts = n;
  double result = h + m / 60.0 + ss / 3600.0;
  return negative ? -result : result;
}

int AtlasService::parseMonth(const std::string& s) {
  static const char* months[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };
  for (int i = 0; i < 12; ++i) {
    if (s == months[i]) return i + 1;
  }
  // Try numeric
  int v = 0;
  if (std::sscanf(s.c_str(), "%d", &v) == 1 && v >= 1 && v <= 12) return v;
  return 1;
}

// Tomohiko Sakamoto's algorithm: returns 0=Sun, 1=Mon … 6=Sat
int AtlasService::dayOfWeek(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y--;
  return ((y + y/4 - y/100 + y/400 + t[m - 1] + d) % 7 + 7) % 7;
}

int AtlasService::daysInMonth(int year, int month) {
  static const int d[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month < 1 || month > 12) return 30;
  int days = d[month - 1];
  if (month == 2) {
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (leap) days = 29;
  }
  return days;
}

/// Resolve day specifications like "15", "lastSun", "Sun>=8", "Fri<=1"
/// into an actual day-of-month for the given year and month.
int AtlasService::resolveDaySpec(const std::string& spec, int year, int month) {
  if (spec.empty()) return 1;

  // Try bare number first
  {
    int d = 0;
    if (std::sscanf(spec.c_str(), "%d", &d) == 1 && d >= 1 && d <= 31)
      return d;
  }

  // Map day-of-week abbreviation to 0=Sun…6=Sat
  auto parseDow = [](const std::string& s) -> int {
    if (s.size() < 3) return -1;
    std::string d3 = s.substr(0, 3);
    if (d3 == "Sun") return 0;
    if (d3 == "Mon") return 1;
    if (d3 == "Tue") return 2;
    if (d3 == "Wed") return 3;
    if (d3 == "Thu") return 4;
    if (d3 == "Fri") return 5;
    if (d3 == "Sat") return 6;
    return -1;
  };

  // "lastSun", "lastMon", etc.
  if (spec.substr(0, 4) == "last") {
    int targetDow = parseDow(spec.substr(4));
    if (targetDow < 0) return 1;
    int dim = daysInMonth(year, month);
    int lastDow = dayOfWeek(year, month, dim);
    int diff = (lastDow - targetDow + 7) % 7;
    return dim - diff;
  }

  // "Sun>=8" — first <dow> on or after day N
  auto geqPos = spec.find(">=");
  if (geqPos != std::string::npos) {
    int targetDow = parseDow(spec.substr(0, geqPos));
    int afterDay = std::atoi(spec.c_str() + geqPos + 2);
    if (targetDow < 0 || afterDay < 1) return 1;
    int dow = dayOfWeek(year, month, afterDay);
    int diff = (targetDow - dow + 7) % 7;
    return afterDay + diff;
  }

  // "Fri<=1" — last <dow> on or before day N
  auto leqPos = spec.find("<=");
  if (leqPos != std::string::npos) {
    int targetDow = parseDow(spec.substr(0, leqPos));
    int beforeDay = std::atoi(spec.c_str() + leqPos + 2);
    if (targetDow < 0 || beforeDay < 1) return 1;
    int dow = dayOfWeek(year, month, beforeDay);
    int diff = (dow - targetDow + 7) % 7;
    return beforeDay - diff;
  }

  return 1;
}

// ---------------------------------------------------------------------------
// Atlas loading
// ---------------------------------------------------------------------------

bool AtlasService::loadAtlas(const std::string& atlasPath) {
  std::ifstream file(atlasPath);
  if (!file.is_open()) return false;

  m_entries.clear();
  std::string line;
  std::string currentTimezone;

  // Skip header lines (comments and -YY directive)
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == ';' || line[0] == '@') continue;
    if (line.substr(0, 3) == "-YY") continue;
    break;  // First data line
  }

  // Process the first data line, then the rest
  auto processLine = [&](const std::string& ln) {
    auto cols = splitTabs(ln);
    if (cols.size() < 4) return;

    AtlasEntry entry;
    // Column 1: longitude (Astrolog: +W, Asteria: +E → negate)
    entry.longitude = -std::strtod(cols[0].c_str(), nullptr);
    // Column 2: latitude (+N in both)
    entry.latitude = std::strtod(cols[1].c_str(), nullptr);
    // Column 3: country/region code
    entry.regionCode = cols[2];
    // Column 4: city name
    entry.name = cols[3];
    // Column 5 (optional): timezone — if absent, inherit previous
    if (cols.size() >= 5 && !cols[4].empty()) {
      currentTimezone = cols[4];
    }
    entry.timezoneName = currentTimezone;

    m_entries.push_back(std::move(entry));
  };

  // Process the line we already read
  if (!line.empty() && line[0] != ';') processLine(line);

  // Process remaining lines
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == ';') continue;
    processLine(line);
  }

  return !m_entries.empty();
}

// ---------------------------------------------------------------------------
// Timezone loading
// ---------------------------------------------------------------------------

bool AtlasService::loadTimezones(const std::string& timezonePath) {
  std::ifstream file(timezonePath);
  if (!file.is_open()) return false;

  m_dstRules.clear();
  m_zones.clear();
  m_zoneAliases.clear();

  std::string line;
  int section = 0;  // 0=preamble, 1=Part1, 2=Part2, 3=Part3

  while (std::getline(file, line)) {
    // Strip trailing \r
    if (!line.empty() && line.back() == '\r') line.pop_back();

    if (line.empty() || line[0] == ';' || line[0] == '@') continue;

    // Detect section markers
    if (line.substr(0, 4) == "-YY1") { section = 1; continue; }
    if (line.substr(0, 4) == "-YY2") { section = 2; continue; }
    if (line.substr(0, 4) == "-YY3") { section = 3; continue; }

    if (section == 1) {
      // Part 1: DST rule sets
      // Rule header: "RuleName\tcount"
      // Followed by count transition lines
      auto cols = splitTabs(line);
      if (cols.size() < 2) continue;
      std::string ruleName = cols[0];
      int count = std::atoi(cols[1].c_str());
      if (count <= 0) continue;

      DstRuleSet ruleSet;
      for (int i = 0; i < count && std::getline(file, line); ++i) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto tc = splitTabs(line);
        if (tc.size() < 6) continue;

        DstTransition t;
        t.yearStart = std::atoi(tc[0].c_str());
        if (tc[1] == "only")
          t.yearEnd = t.yearStart;
        else if (tc[1] == "max")
          t.yearEnd = INT_MAX;
        else
          t.yearEnd = std::atoi(tc[1].c_str());

        t.month = parseMonth(tc[2]);
        t.daySpec = tc[3];

        // Time field — may have 's' or 'u' suffix
        {
          const std::string& tf = tc[4];
          t.timeType = 'w';
          if (!tf.empty() && (tf.back() == 's' || tf.back() == 'u'))
            t.timeType = tf.back();
          t.timeHours = parseOffset(tf);
        }

        t.offsetHours = std::strtod(tc[5].c_str(), nullptr);
        ruleSet.transitions.push_back(std::move(t));
      }
      m_dstRules[ruleName] = std::move(ruleSet);

    } else if (section == 2) {
      // Part 2: Zone definitions
      // Zone header: "ZoneName\tcount"
      auto cols = splitTabs(line);
      if (cols.size() < 2) continue;
      std::string zoneName = cols[0];
      int count = std::atoi(cols[1].c_str());
      if (count <= 0) continue;

      ZoneDefinition zoneDef;
      for (int i = 0; i < count && std::getline(file, line); ++i) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto ec = splitTabs(line);
        if (ec.size() < 2) continue;

        ZoneEntry ze;
        ze.baseOffsetHours = parseOffset(ec[0]);
        ze.ruleName = ec[1];

        // Check if rule is "-" (no DST) or a fixed numeric offset
        if (ze.ruleName == "-") {
          ze.isFixedDst = true;
          ze.fixedDstOffset = 0.0;
        } else {
          // Try to parse as a number (fixed DST offset)
          char* endp = nullptr;
          double v = std::strtod(ze.ruleName.c_str(), &endp);
          if (endp != ze.ruleName.c_str() && *endp == '\0') {
            ze.isFixedDst = true;
            ze.fixedDstOffset = v;
          }
        }

        // "Until" date columns (optional: year, month, day, time)
        if (ec.size() >= 3 && !ec[2].empty()) {
          ze.untilYear = std::atoi(ec[2].c_str());
          ze.untilMonth = (ec.size() >= 4 && !ec[3].empty()) ? parseMonth(ec[3]) : 1;
          ze.untilDay = (ec.size() >= 5 && !ec[4].empty()) ? std::atoi(ec[4].c_str()) : 1;
          ze.untilTime = (ec.size() >= 6 && !ec[5].empty()) ? parseOffset(ec[5]) : 0.0;
        }
        // else: untilYear stays INT_MAX (forever)

        zoneDef.entries.push_back(std::move(ze));
      }
      m_zones[zoneName] = std::move(zoneDef);

    } else if (section == 3) {
      // Part 3: Aliases
      auto cols = splitTabs(line);
      if (cols.size() >= 2) {
        m_zoneAliases[cols[0]] = cols[1];
      }
    }
  }

  return !m_zones.empty();
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

std::vector<const AtlasEntry*> AtlasService::search(
    const std::string& query, int maxResults) const {
  std::vector<const AtlasEntry*> results;
  if (query.empty() || m_entries.empty()) return results;

  std::string lowerQuery = toLower(query);

  // Score entries: exact-start match scores higher than substring match
  struct Scored {
    const AtlasEntry* entry;
    int score;  // 0=starts-with, 1=contains
  };
  std::vector<Scored> scored;

  for (auto& e : m_entries) {
    std::string lowerName = toLower(e.name);
    auto pos = lowerName.find(lowerQuery);
    if (pos == std::string::npos) continue;
    int score = (pos == 0) ? 0 : 1;
    scored.push_back({&e, score});
    // Collect more than maxResults for sorting, but cap for performance
    if (static_cast<int>(scored.size()) > maxResults * 10) break;
  }

  // Sort: starts-with first, then alphabetically
  std::sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
    if (a.score != b.score) return a.score < b.score;
    return a.entry->name < b.entry->name;
  });

  int count = std::min(maxResults, static_cast<int>(scored.size()));
  results.reserve(count);
  for (int i = 0; i < count; ++i) {
    results.push_back(scored[i].entry);
  }
  return results;
}

// ---------------------------------------------------------------------------
// Timezone resolution
// ---------------------------------------------------------------------------

const AtlasService::ZoneEntry* AtlasService::findActiveZoneEntry(
    const ZoneDefinition& zone, int year, int month, int day) const {
  // Walk entries in order; each has an "until" date.
  // The first entry whose "until" date is AFTER our target is the active one.
  for (auto& ze : zone.entries) {
    if (ze.untilYear == INT_MAX) return &ze;  // Forever entry
    // Compare: target date vs until date
    if (year < ze.untilYear) return &ze;
    if (year == ze.untilYear) {
      if (month < ze.untilMonth) return &ze;
      if (month == ze.untilMonth && day < ze.untilDay) return &ze;
    }
  }
  // Fallback: return last entry
  return zone.entries.empty() ? nullptr : &zone.entries.back();
}

double AtlasService::evaluateDstOffset(const DstRuleSet& rules,
                                        int year, int month, int day) const {
  // Collect all transitions that could affect the given date.
  // We look at transitions from the previous year (to determine starting state)
  // and the current year.
  struct Resolved {
    int year, month, day;
    double offset;
  };
  std::vector<Resolved> transitions;

  for (auto& t : rules.transitions) {
    // Check year-1 and year
    for (int y : {year - 1, year}) {
      if (y < t.yearStart || y > t.yearEnd) continue;
      int d = resolveDaySpec(t.daySpec, y, t.month);
      transitions.push_back({y, t.month, d, t.offsetHours});
    }
  }

  if (transitions.empty()) return 0.0;

  // Sort by date
  std::sort(transitions.begin(), transitions.end(),
            [](const Resolved& a, const Resolved& b) {
              if (a.year != b.year) return a.year < b.year;
              if (a.month != b.month) return a.month < b.month;
              return a.day < b.day;
            });

  // Find the last transition on or before the target date
  double currentOffset = 0.0;
  for (auto& t : transitions) {
    if (t.year > year) break;
    if (t.year == year && t.month > month) break;
    if (t.year == year && t.month == month && t.day > day) break;
    currentOffset = t.offset;
  }

  return currentOffset;
}

TimezoneResult AtlasService::resolveTimezone(
    const std::string& timezoneName,
    int year, int month, int day, double /*timeHours*/) const {

  TimezoneResult result;
  result.timezoneName = timezoneName;

  // Resolve alias
  std::string resolvedName = timezoneName;
  auto aliasIt = m_zoneAliases.find(resolvedName);
  if (aliasIt != m_zoneAliases.end()) {
    resolvedName = aliasIt->second;
  }

  auto zoneIt = m_zones.find(resolvedName);
  if (zoneIt == m_zones.end()) return result;

  const auto* ze = findActiveZoneEntry(zoneIt->second, year, month, day);
  if (!ze) return result;

  result.utcOffsetHours = ze->baseOffsetHours;

  if (ze->isFixedDst) {
    result.dstOffsetHours = ze->fixedDstOffset;
  } else {
    // Look up the DST rule set
    auto ruleIt = m_dstRules.find(ze->ruleName);
    if (ruleIt != m_dstRules.end()) {
      result.dstOffsetHours = evaluateDstOffset(ruleIt->second, year, month, day);
    }
  }

  return result;
}

}  // namespace asteria::util
