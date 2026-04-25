#pragma once

#include "result.h"
#include "domain/chart_request.h"

#include <optional>
#include <string>
#include <vector>

namespace asteria::engine { class IChartEngine; }

namespace asteria::core {

class TransitReportService {
 public:
  struct AspectRule {
    std::string aspectType;
    double angleDegrees = 0.0;
    double orbDegrees = 1.0;
  };

  struct Rules {
    std::vector<std::string> transitObjects;
    std::vector<std::string> natalObjects;
    std::vector<AspectRule> aspectRules;
  };

  struct Request {
    domain::ChartRequest chartRequest;
    std::string subjectName;
    std::string startDateTime;
    double rangeYears = 5.0;
    Rules rules = defaultRules();
  };

  struct Event {
    std::string timestamp;
    std::string transitObject;
    std::string natalObject;
    std::string aspectType;
    double orbDegrees = 0.0;
    bool retrograde = false;
  };

  struct Report {
    std::string subjectName;
    std::string startDateTime;
    std::string endDateTime;
    double rangeYears = 0.0;
    std::string timezoneLabel;
    Rules rules;
    std::vector<std::string> warnings;
    std::vector<Event> events;
    std::string bodyMarkdown;
  };

  explicit TransitReportService(engine::IChartEngine& chartEngine);

  Result<Report> generate(const Request& request) const;

  static Rules defaultRules();
  static std::vector<std::string> defaultTransitObjects();
  static std::vector<std::string> defaultNatalObjects();
  static std::vector<AspectRule> defaultAspectRules();
  static std::optional<AspectRule> knownAspectRule(const std::string& aspectType);

 private:
  engine::IChartEngine& m_chartEngine;
};

}  // namespace asteria::core