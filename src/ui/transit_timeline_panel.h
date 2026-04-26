#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "core/transit_report_service.h"
#include "domain/birth_event.h"
#include "domain/location_resolution.h"
#include "domain/person.h"

namespace asteria::ui {

struct AppContext;
class AiInterpretationPanel;

class TransitTimelinePanel {
 public:
  bool open = true;

  explicit TransitTimelinePanel(AppContext& ctx);
  ~TransitTimelinePanel();

  void draw();
  void setSelectedPerson(std::int64_t personId);
  void setAiPanel(AiInterpretationPanel* panel) { m_aiPanel = panel; }

 private:
  struct TransitPlanetOption {
    std::string name;
    bool enabled = true;
  };

  struct AspectOption {
    core::TransitReportService::AspectRule rule;
    bool enabled = true;
  };

  struct PreparedRequest {
    core::TransitReportService::Request request;
    std::optional<domain::Person> person;
    std::optional<domain::BirthEvent> birthEvent;
    std::optional<domain::LocationResolution> location;
  };

  std::optional<PreparedRequest> buildRequest() const;
  core::TransitReportService::Rules currentRules() const;
  void loadRuleSettings();
  void saveRuleSettings() const;
  void startGeneration();
  void requestAiInsights();
  void saveMarkdown();
  void syncWorkerState();
  std::string buildAiPrompt() const;
  std::string currentAiAnalysisLabel() const;

  AppContext& m_ctx;

  std::int64_t m_personId = 0;
  std::optional<domain::Person> m_person;
  std::optional<domain::BirthEvent> m_birthEvent;
  std::optional<domain::LocationResolution> m_location;

  int m_periodYears = 5;
  std::array<char, 32> m_startDateBuf{};
  std::vector<TransitPlanetOption> m_transitPlanetOptions;
  std::vector<AspectOption> m_aspectOptions;

  std::mutex m_mutex;
  std::thread m_worker;
  std::atomic<bool> m_generating{false};
  std::atomic<std::uint64_t> m_generationId{0};
  std::string m_workerMarkdown;
  std::string m_workerStatus;
  bool m_workerStatusIsError = false;

  std::string m_displayMarkdown;
  std::string m_statusText;
  bool m_statusIsError = false;
  AiInterpretationPanel* m_aiPanel = nullptr;
};

}  // namespace asteria::ui