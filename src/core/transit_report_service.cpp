#include "transit_report_service.h"

#include "engine/ichart_engine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace asteria::core {

namespace {

using ClockSeconds = std::chrono::sys_seconds;
using namespace std::chrono_literals;

struct KnownAspectDef {
  const char* name;
  double angleDegrees;
  double defaultOrbDegrees;
};

struct EventKey {
  std::string transitObject;
  std::string natalObject;
  std::string aspectType;

  bool operator<(const EventKey& other) const {
    return std::tie(transitObject, natalObject, aspectType)
        < std::tie(other.transitObject, other.natalObject, other.aspectType);
  }
};

struct DetectedAspect {
  EventKey key;
  double orbDegrees = 0.0;
  bool retrograde = false;
};

struct ActiveWindow {
  ClockSeconds start;
  ClockSeconds last;
  ClockSeconds bestTime;
  double bestOrbDegrees = 0.0;
  bool retrogradeAtBest = false;
};

constexpr std::array<KnownAspectDef, 5> kKnownAspects{{
    {"Conjunction", 0.0, 1.0},
    {"Sextile", 60.0, 1.0},
    {"Square", 90.0, 1.0},
    {"Trine", 120.0, 1.0},
    {"Opposition", 180.0, 1.0},
}};

constexpr auto kSampleStep = 24h;

bool contains(const std::vector<std::string>& names, const std::string& value) {
  return std::find(names.begin(), names.end(), value) != names.end();
}

const TransitReportService::AspectRule* findAspectRule(
    const std::vector<TransitReportService::AspectRule>& rules,
    const std::string& aspectType) {
  auto it = std::find_if(rules.begin(), rules.end(),
                         [&](const TransitReportService::AspectRule& rule) {
                           return rule.aspectType == aspectType;
                         });
  return it == rules.end() ? nullptr : &*it;
}

double angularSeparation(double a, double b) {
  double diff = std::fmod(std::fabs(a - b), 360.0);
  if (diff < 0.0) diff += 360.0;
  if (diff > 180.0) diff = 360.0 - diff;
  return diff;
}

std::string joinList(const std::vector<std::string>& values) {
  std::ostringstream out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0) out << ", ";
    out << values[i];
  }
  return out.str();
}

std::string formatAspectRuleSummary(const std::vector<TransitReportService::AspectRule>& rules) {
  std::ostringstream out;
  for (std::size_t i = 0; i < rules.size(); ++i) {
    if (i > 0) out << ", ";
    out << rules[i].aspectType << " (" << std::fixed << std::setprecision(2)
        << rules[i].orbDegrees << " deg orb)";
    out.unsetf(std::ios::floatfield);
  }
  return out.str();
}

std::string formatUtcOffset(double hours) {
  const double absHours = std::fabs(hours);
  const int totalMinutes = static_cast<int>(std::llround(absHours * 60.0));
  const int wholeHours = totalMinutes / 60;
  const int minutes = totalMinutes % 60;

  char buffer[20];
  std::snprintf(buffer, sizeof(buffer), "UTC%c%02d:%02d",
                hours >= 0.0 ? '+' : '-', wholeHours, minutes);
  return buffer;
}

std::optional<ClockSeconds> parseLocalDateTime(const std::string& value,
                                               double fixedOffsetHours) {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  int parsed = std::sscanf(value.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d",
                           &year, &month, &day, &hour, &minute, &second);
  if (parsed < 3) {
    parsed = std::sscanf(value.c_str(), "%4d-%2d-%2d %2d:%2d:%2d",
                         &year, &month, &day, &hour, &minute, &second);
  }
  if (parsed < 3) {
    parsed = std::sscanf(value.c_str(), "%4d-%2d-%2d", &year, &month, &day);
  }
  if (parsed < 3) return std::nullopt;

  const auto yearValue = std::chrono::year{year};
  const auto monthValue = std::chrono::month{static_cast<unsigned>(month)};
  const auto dayValue = std::chrono::day{static_cast<unsigned>(day)};
  const std::chrono::year_month_day ymd{yearValue, monthValue, dayValue};
  if (!ymd.ok()) return std::nullopt;

  const auto localTime = std::chrono::sys_days{ymd}
      + std::chrono::hours{hour}
      + std::chrono::minutes{minute}
      + std::chrono::seconds{second};
  const auto offsetSeconds = std::chrono::seconds{
      static_cast<std::int64_t>(std::llround(fixedOffsetHours * 3600.0))};
  return std::chrono::time_point_cast<std::chrono::seconds>(localTime - offsetSeconds);
}

std::string formatLocalDateTime(ClockSeconds utcTime, double fixedOffsetHours) {
  const auto offsetSeconds = std::chrono::seconds{
      static_cast<std::int64_t>(std::llround(fixedOffsetHours * 3600.0))};
  const auto localTime = utcTime + offsetSeconds;
  const auto dayPoint = std::chrono::floor<std::chrono::days>(localTime);
  const std::chrono::year_month_day ymd{dayPoint};
  const std::chrono::hh_mm_ss tod{localTime - dayPoint};

  std::ostringstream out;
  out << std::setfill('0')
      << std::setw(4) << static_cast<int>(ymd.year()) << '-'
      << std::setw(2) << static_cast<unsigned>(ymd.month()) << '-'
      << std::setw(2) << static_cast<unsigned>(ymd.day()) << ' '
      << std::setw(2) << tod.hours().count() << ':'
      << std::setw(2) << tod.minutes().count();
  return out.str();
}

domain::ResolvedBirthInput buildTransitInput(const domain::ResolvedBirthInput& natalInput,
                                             ClockSeconds utcTime) {
  const double fixedOffsetHours = natalInput.timezoneHours + natalInput.dstHours;
  const auto offsetSeconds = std::chrono::seconds{
      static_cast<std::int64_t>(std::llround(fixedOffsetHours * 3600.0))};
  const auto localTime = utcTime + offsetSeconds;
  const auto dayPoint = std::chrono::floor<std::chrono::days>(localTime);
  const std::chrono::year_month_day ymd{dayPoint};
  const std::chrono::hh_mm_ss tod{localTime - dayPoint};

  domain::ResolvedBirthInput input;
  input.year = static_cast<int>(ymd.year());
  input.month = static_cast<unsigned>(ymd.month());
  input.day = static_cast<unsigned>(ymd.day());
  input.timeHours = static_cast<double>(tod.hours().count())
      + static_cast<double>(tod.minutes().count()) / 60.0
      + static_cast<double>(tod.seconds().count()) / 3600.0;
  input.timezoneHours = natalInput.timezoneHours;
  input.dstHours = natalInput.dstHours;
  input.latitudeDeg = natalInput.latitudeDeg;
  input.longitudeDeg = natalInput.longitudeDeg;
  return input;
}

const domain::PlanetPosition* findPlanet(const domain::ComputedChart& chart,
                                         const std::string& objectId) {
  auto it = std::find_if(chart.planets.begin(), chart.planets.end(),
                         [&](const domain::PlanetPosition& planet) {
                           return planet.objectId == objectId;
                         });
  return it == chart.planets.end() ? nullptr : &*it;
}

std::optional<DetectedAspect> detectAspect(const domain::ComputedChart& natalChart,
                                           const domain::ComputedChart& transitChart,
                                           const EventKey& key,
                                           const TransitReportService::AspectRule& rule) {
  const domain::PlanetPosition* natalPlanet = findPlanet(natalChart, key.natalObject);
  const domain::PlanetPosition* transitPlanet = findPlanet(transitChart, key.transitObject);
  if (natalPlanet == nullptr || transitPlanet == nullptr) {
    return std::nullopt;
  }

  const double separation = angularSeparation(transitPlanet->longitudeDegrees,
                                              natalPlanet->longitudeDegrees);
  const double orbDegrees = std::fabs(separation - rule.angleDegrees);
  if (orbDegrees > rule.orbDegrees) return std::nullopt;

  return DetectedAspect{key, orbDegrees, transitPlanet->retrograde};
}

std::vector<DetectedAspect> detectActiveAspects(
    const domain::ComputedChart& natalChart,
    const domain::ComputedChart& transitChart,
    const TransitReportService::Rules& rules) {
  std::vector<DetectedAspect> aspects;

  for (const auto& transitPlanet : transitChart.planets) {
    if (!contains(rules.transitObjects, transitPlanet.objectId)) continue;

    for (const auto& natalPlanet : natalChart.planets) {
      if (!contains(rules.natalObjects, natalPlanet.objectId)) continue;

      for (const auto& aspectRule : rules.aspectRules) {
        EventKey key{transitPlanet.objectId, natalPlanet.objectId, aspectRule.aspectType};
        auto detected = detectAspect(natalChart, transitChart, key, aspectRule);
        if (detected) {
          aspects.push_back(*detected);
          break;
        }
      }
    }
  }

  return aspects;
}

std::string buildMarkdown(const TransitReportService::Report& report) {
  std::ostringstream out;
  out << "## Transit-to-Natal Timeline\n\n";
  out << "**Subject:** " << (report.subjectName.empty() ? std::string("Selected person") : report.subjectName) << "  \n";
  out << "**Start:** " << report.startDateTime << "  \n";
  out << "**End:** " << report.endDateTime << "  \n";
  out << "**Range:** " << std::fixed << std::setprecision(1) << report.rangeYears << " years  \n";
  out << "**Times:** fixed natal offset (" << report.timezoneLabel << ")\n\n";
  out.unsetf(std::ios::floatfield);

  if (!report.warnings.empty()) {
    out << "> **Uncertainty Notice:**\n";
    for (const auto& warning : report.warnings) {
      out << "> - " << warning << "\n";
    }
    out << ">\n\n";
  }

  out << "**Included transits:** " << joinList(report.rules.transitObjects) << "  \n";
  out << "**Included natal planets:** " << joinList(report.rules.natalObjects) << "  \n";
  out << "**Aspect rules:** " << formatAspectRuleSummary(report.rules.aspectRules) << "\n\n";

  if (report.events.empty()) {
    out << "No matching transits were found in this range.\n";
    return out.str();
  }

  std::string currentYear;
  for (const auto& event : report.events) {
    const std::string eventYear = event.timestamp.substr(0, 4);
    if (eventYear != currentYear) {
      if (!currentYear.empty()) out << "\n";
      out << "### " << eventYear << "\n\n";
      currentYear = eventYear;
    }

    out << "- " << event.timestamp << ' ' << report.timezoneLabel << ": "
        << event.transitObject << ' ' << event.aspectType
        << " natal " << event.natalObject
        << " (orb " << std::fixed << std::setprecision(2) << event.orbDegrees << " deg";
    if (event.retrograde) {
      out << ", Rx";
    }
    out << ")\n";
    out.unsetf(std::ios::floatfield);
  }

  return out.str();
}

}  // namespace

TransitReportService::TransitReportService(engine::IChartEngine& chartEngine)
    : m_chartEngine(chartEngine) {}

std::vector<std::string> TransitReportService::defaultTransitObjects() {
  return {"Jupiter", "Saturn", "Uranus", "Neptune", "Pluto"};
}

std::vector<std::string> TransitReportService::defaultNatalObjects() {
  return {"Sun", "Moon", "Mercury", "Venus", "Mars",
          "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto"};
}

std::optional<TransitReportService::AspectRule> TransitReportService::knownAspectRule(
    const std::string& aspectType) {
  for (const auto& aspect : kKnownAspects) {
    if (aspectType == aspect.name) {
      return AspectRule{aspect.name, aspect.angleDegrees, aspect.defaultOrbDegrees};
    }
  }
  return std::nullopt;
}

std::vector<TransitReportService::AspectRule> TransitReportService::defaultAspectRules() {
  std::vector<AspectRule> rules;
  for (const auto& aspect : kKnownAspects) {
    rules.push_back({aspect.name, aspect.angleDegrees, aspect.defaultOrbDegrees});
  }
  return rules;
}

TransitReportService::Rules TransitReportService::defaultRules() {
  return {defaultTransitObjects(), defaultNatalObjects(), defaultAspectRules()};
}

Result<TransitReportService::Report> TransitReportService::generate(
    const Request& request) const {
  if (!request.chartRequest.primaryInput) {
    return Result<Report>::failure(
        {"missing_input", "Transit report generation requires a resolved primary input."});
  }
  if (request.rangeYears <= 0.0) {
    return Result<Report>::failure(
        {"invalid_range", "Transit report range must be greater than zero years."});
  }

  Rules rules = request.rules;
  if (rules.natalObjects.empty()) {
    rules.natalObjects = defaultNatalObjects();
  }
  if (rules.transitObjects.empty()) {
    return Result<Report>::failure(
        {"invalid_rules", "Transit report generation requires at least one transit planet."});
  }
  if (rules.aspectRules.empty()) {
    return Result<Report>::failure(
        {"invalid_rules", "Transit report generation requires at least one aspect rule."});
  }
  for (const auto& aspectRule : rules.aspectRules) {
    if (aspectRule.aspectType.empty() || aspectRule.orbDegrees <= 0.0
        || aspectRule.angleDegrees < 0.0 || aspectRule.angleDegrees > 180.0) {
      return Result<Report>::failure(
          {"invalid_rules", "Transit report aspect rules must include a name, an angle between 0 and 180, and a positive orb."});
    }
  }

  const domain::ResolvedBirthInput& natalInput = *request.chartRequest.primaryInput;
  const double fixedOffsetHours = natalInput.timezoneHours + natalInput.dstHours;
  auto startTime = parseLocalDateTime(request.startDateTime, fixedOffsetHours);
  if (!startTime) {
    return Result<Report>::failure(
        {"invalid_start", "Transit report start date/time must be YYYY-MM-DD or YYYY-MM-DDTHH:MM[:SS]."});
  }

  const auto rangeSeconds = std::chrono::seconds{
      static_cast<std::int64_t>(std::llround(request.rangeYears * 365.2425 * 24.0 * 3600.0))};
  const ClockSeconds endTime = *startTime + rangeSeconds;

  domain::ChartRequest natalRequest = request.chartRequest;
  natalRequest.chartType = domain::ChartType::Natal;
  auto natalResult = m_chartEngine.computeNatalChart(natalRequest);
  if (!natalResult.ok()) {
    return Result<Report>::failure(natalResult.error());
  }
  const domain::ComputedChart natalChart = natalResult.value();

  std::unordered_map<std::int64_t, domain::ComputedChart> transitChartCache;

  auto getTransitChart = [&](ClockSeconds when,
                             const domain::ComputedChart*& outChart,
                             Error& outError) -> bool {
    const auto key = std::chrono::duration_cast<std::chrono::seconds>(
        when.time_since_epoch()).count();
    auto cached = transitChartCache.find(key);
    if (cached != transitChartCache.end()) {
      outChart = &cached->second;
      return true;
    }

    domain::ChartRequest transitRequest = request.chartRequest;
    transitRequest.chartType = domain::ChartType::TransitToNatal;
    transitRequest.includeHouses = false;
    transitRequest.requestTimeContext.reset();
    transitRequest.secondaryBirthEventId.reset();
    transitRequest.secondaryInput.reset();
    transitRequest.transitInput.reset();
    transitRequest.primaryInput = buildTransitInput(natalInput, when);

    auto transitResult = m_chartEngine.computeNatalChart(transitRequest);
    if (!transitResult.ok()) {
      outError = transitResult.error();
      return false;
    }

    auto inserted = transitChartCache.emplace(key, std::move(transitResult.value()));
    outChart = &inserted.first->second;
    return true;
  };

  auto refineEvent = [&](const EventKey& key,
                         ClockSeconds bestTime,
                         ClockSeconds windowStart,
                         ClockSeconds windowEnd,
                         double bestOrbDegrees,
                         bool retrogradeAtBest,
                         Event& event,
                         Error& outError) -> bool {
    struct SearchStep {
      std::chrono::seconds step;
      int radiusUnits;
    };

    const AspectRule* aspectRule = findAspectRule(rules.aspectRules, key.aspectType);
    if (aspectRule == nullptr) {
      outError = {"invalid_rules", "Transit report attempted to refine an aspect that is no longer configured."};
      return false;
    }

    ClockSeconds refinedTime = bestTime;
    double refinedOrbDegrees = bestOrbDegrees;
    bool refinedRetrograde = retrogradeAtBest;

    const std::array<SearchStep, 4> searchSteps{{
        {std::chrono::hours{6}, 4},
        {std::chrono::hours{1}, 6},
        {std::chrono::minutes{5}, 6},
        {std::chrono::minutes{1}, 10},
    }};

    for (const auto& search : searchSteps) {
      const ClockSeconds searchStart = std::max(windowStart,
          refinedTime - search.step * search.radiusUnits);
      const ClockSeconds searchEnd = std::min(windowEnd,
          refinedTime + search.step * search.radiusUnits);

      for (ClockSeconds current = searchStart; current <= searchEnd; current += search.step) {
        const domain::ComputedChart* transitChart = nullptr;
        if (!getTransitChart(current, transitChart, outError)) return false;

        auto detected = detectAspect(natalChart, *transitChart, key, *aspectRule);
        if (!detected) continue;

        if (detected->orbDegrees < refinedOrbDegrees) {
          refinedTime = current;
          refinedOrbDegrees = detected->orbDegrees;
          refinedRetrograde = detected->retrograde;
        }
      }
    }

    event.timestamp = formatLocalDateTime(refinedTime, fixedOffsetHours);
    event.transitObject = key.transitObject;
    event.natalObject = key.natalObject;
    event.aspectType = key.aspectType;
    event.orbDegrees = refinedOrbDegrees;
    event.retrograde = refinedRetrograde;
    return true;
  };

  std::map<EventKey, ActiveWindow> activeWindows;
  std::vector<std::pair<ClockSeconds, Event>> resolvedEvents;
  Error chartError;

  auto flushWindow = [&](const EventKey& key,
                         const ActiveWindow& window,
                         ClockSeconds nextSampleTime) -> bool {
    Event event;
    const ClockSeconds windowStart = std::max(*startTime, window.start - kSampleStep);
    const ClockSeconds windowEnd = std::min(endTime, nextSampleTime);
    if (!refineEvent(key, window.bestTime, windowStart, windowEnd,
                     window.bestOrbDegrees, window.retrogradeAtBest, event, chartError)) {
      return false;
    }

    const auto resolvedTime = parseLocalDateTime(event.timestamp, fixedOffsetHours).value_or(window.bestTime);
    resolvedEvents.push_back({resolvedTime, std::move(event)});
    return true;
  };

  for (ClockSeconds sampleTime = *startTime; sampleTime <= endTime; sampleTime += kSampleStep) {
    const domain::ComputedChart* transitChart = nullptr;
    if (!getTransitChart(sampleTime, transitChart, chartError)) {
      return Result<Report>::failure(chartError);
    }

    const auto detectedAspects = detectActiveAspects(natalChart, *transitChart, rules);
    std::set<EventKey> seenKeys;
    for (const auto& detected : detectedAspects) {
      seenKeys.insert(detected.key);
      auto [it, inserted] = activeWindows.emplace(
          detected.key,
          ActiveWindow{sampleTime, sampleTime, sampleTime, detected.orbDegrees, detected.retrograde});
      if (!inserted) {
        it->second.last = sampleTime;
        if (detected.orbDegrees < it->second.bestOrbDegrees) {
          it->second.bestOrbDegrees = detected.orbDegrees;
          it->second.bestTime = sampleTime;
          it->second.retrogradeAtBest = detected.retrograde;
        }
      }
    }

    for (auto it = activeWindows.begin(); it != activeWindows.end();) {
      if (seenKeys.contains(it->first)) {
        ++it;
        continue;
      }

      if (!flushWindow(it->first, it->second, sampleTime)) {
        return Result<Report>::failure(chartError);
      }
      it = activeWindows.erase(it);
    }
  }

  for (const auto& [key, window] : activeWindows) {
    if (!flushWindow(key, window, endTime)) {
      return Result<Report>::failure(chartError);
    }
  }

  std::sort(resolvedEvents.begin(), resolvedEvents.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.first < rhs.first;
            });

  Report report;
  report.subjectName = request.subjectName;
  report.startDateTime = formatLocalDateTime(*startTime, fixedOffsetHours);
  report.endDateTime = formatLocalDateTime(endTime, fixedOffsetHours);
  report.rangeYears = request.rangeYears;
  report.timezoneLabel = formatUtcOffset(fixedOffsetHours);
  report.rules = std::move(rules);
  report.warnings = natalChart.uncertaintyFlags;

  report.events.reserve(resolvedEvents.size());
  for (auto& pair : resolvedEvents) {
    report.events.push_back(std::move(pair.second));
  }
  report.bodyMarkdown = buildMarkdown(report);
  return Result<Report>::success(report);
}

}  // namespace asteria::core
