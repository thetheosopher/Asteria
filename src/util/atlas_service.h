#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>

namespace asteria::util {

/// A single city entry from the Astrolog atlas file.
struct AtlasEntry {
  std::string name;
  std::string regionCode;   // ISO country code or US state / CA province (lowercase)
  double latitude  = 0.0;   // +N / -S (standard geographic convention)
  double longitude = 0.0;   // +E / -W (standard geographic convention)
  std::string timezoneName; // IANA timezone, e.g. "America/New_York"
};

/// Result of resolving a timezone for a specific date.
struct TimezoneResult {
  double utcOffsetHours = 0.0;  // Positive = west of UTC (Asteria convention)
  double dstOffsetHours = 0.0;  // Typically 0 or 1
  std::string timezoneName;
};

/// Loads and queries the Astrolog atlas (atlasbig.as) and timezone (timezone.as) files.
/// All 222K+ city entries are held in memory for fast substring search.
class AtlasService {
 public:
  /// Load the atlas city file.  Returns false on I/O error.
  bool loadAtlas(const std::string& atlasPath);

  /// Load the timezone change file.  Returns false on I/O error.
  bool loadTimezones(const std::string& timezonePath);

  /// Case-insensitive substring search.  Returns pointers into the internal vector
  /// (valid until the AtlasService is destroyed or reloaded).
  std::vector<const AtlasEntry*> search(const std::string& query,
                                         int maxResults = 20) const;

  /// Resolve the UTC offset and DST offset for a given IANA timezone name and date.
  /// Uses the historical DST rules from timezone.as.
  /// \p timeHours is decimal hours (0–24) in local wall-clock time.
  TimezoneResult resolveTimezone(const std::string& timezoneName,
                                  int year, int month, int day,
                                  double timeHours = 12.0) const;

  bool isLoaded() const { return !m_entries.empty(); }
  std::size_t entryCount() const { return m_entries.size(); }

 private:
  // --- Atlas data ---
  std::vector<AtlasEntry> m_entries;

  // --- Timezone data (implementation details) ---

  struct DstTransition {
    int yearStart = 0;
    int yearEnd   = 0;   // INT_MAX for "max"
    int month     = 1;
    std::string daySpec;  // "15", "lastSun", "Sun>=8", etc.
    double timeHours = 0.0;
    char   timeType  = 'w';  // 'w'=wall, 's'=standard, 'u'=UTC
    double offsetHours = 0.0; // 0=DST off, 1=DST on (usually)
  };

  struct DstRuleSet {
    std::vector<DstTransition> transitions;
  };

  struct ZoneEntry {
    double baseOffsetHours = 0.0;  // +W convention
    std::string ruleName;          // "-" = none, or rule-set name
    double fixedDstOffset  = 0.0;
    bool   isFixedDst      = false;
    // "Until" date (absent → lasts forever)
    int    untilYear  = 2147483647;
    int    untilMonth = 1;
    int    untilDay   = 1;
    double untilTime  = 0.0;
  };

  struct ZoneDefinition {
    std::vector<ZoneEntry> entries;
  };

  std::unordered_map<std::string, DstRuleSet>    m_dstRules;
  std::unordered_map<std::string, ZoneDefinition> m_zones;
  std::unordered_map<std::string, std::string>    m_zoneAliases;

  // Helpers
  const ZoneEntry* findActiveZoneEntry(const ZoneDefinition& zone,
                                        int year, int month, int day) const;
  double evaluateDstOffset(const DstRuleSet& rules,
                           int year, int month, int day) const;
  static int resolveDaySpec(const std::string& spec, int year, int month);
  static int dayOfWeek(int y, int m, int d);   // 0=Sun … 6=Sat
  static int daysInMonth(int year, int month);
  static double parseOffset(const std::string& s);
  static int parseMonth(const std::string& s);
};

}  // namespace asteria::util
