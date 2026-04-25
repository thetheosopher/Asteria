#include "transit_timeline_panel.h"

#include "app_context.h"
#include "core/birth_event_resolver.h"
#include "file_dialog.h"
#include "markdown_render.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

namespace asteria::ui {

namespace {

std::vector<std::string> splitCsv(const std::string& value) {
  std::vector<std::string> parts;
  std::stringstream stream(value);
  std::string item;
  while (std::getline(stream, item, ',')) {
    if (!item.empty()) parts.push_back(item);
  }
  return parts;
}

std::string joinCsv(const std::vector<std::string>& values) {
  std::ostringstream out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0) out << ',';
    out << values[i];
  }
  return out.str();
}

std::string aspectSettingSuffix(const std::string& aspectType) {
  std::string key;
  key.reserve(aspectType.size());
  for (char ch : aspectType) {
    if (ch >= 'A' && ch <= 'Z') {
      key.push_back(static_cast<char>(ch - 'A' + 'a'));
    } else if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
      key.push_back(ch);
    }
  }
  return key;
}

std::string defaultTransitTimelineStartDateTime() {
  std::time_t now = std::time(nullptr);
  std::tm localTime{};
#ifdef _WIN32
  localtime_s(&localTime, &now);
#else
  localtime_r(&now, &localTime);
#endif
  localTime.tm_mon -= 6;
  if (std::mktime(&localTime) == -1) {
    return "2025-10-25T00:00";
  }

  char buffer[32];
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M", &localTime) == 0) {
    return "2025-10-25T00:00";
  }
  return buffer;
}

}  // namespace

TransitTimelinePanel::TransitTimelinePanel(AppContext& ctx) : m_ctx(ctx) {
  const std::string defaultStart = defaultTransitTimelineStartDateTime();
  std::snprintf(m_startDateBuf.data(), m_startDateBuf.size(), "%s", defaultStart.c_str());
  loadRuleSettings();
}

TransitTimelinePanel::~TransitTimelinePanel() {
  if (m_worker.joinable()) {
    m_worker.join();
  }
}

void TransitTimelinePanel::setSelectedPerson(std::int64_t personId) {
  if (m_personId == personId) return;

  ++m_generationId;
  if (m_worker.joinable()) {
    m_worker.join();
  }
  m_generating.store(false);

  m_personId = personId;
  m_person.reset();
  m_birthEvent.reset();
  m_location.reset();
  m_displayMarkdown.clear();
  m_workerMarkdown.clear();
  m_statusText.clear();
  m_workerStatus.clear();
  m_statusIsError = false;
  m_workerStatusIsError = false;
}

void TransitTimelinePanel::loadRuleSettings() {
  m_transitPlanetOptions.clear();
  m_aspectOptions.clear();

  const std::vector<std::string> defaultTransitObjects = core::TransitReportService::defaultTransitObjects();
  const std::vector<std::string> defaultEnabledTransitObjects = splitCsv(
      m_ctx.getSetting("transit_timeline_transit_objects", joinCsv(defaultTransitObjects)));

  for (const auto& name : defaultTransitObjects) {
    m_transitPlanetOptions.push_back({
        name,
        std::find(defaultEnabledTransitObjects.begin(), defaultEnabledTransitObjects.end(), name)
            != defaultEnabledTransitObjects.end()});
  }

  const std::vector<core::TransitReportService::AspectRule> defaultAspectRules =
      core::TransitReportService::defaultAspectRules();
  std::vector<std::string> defaultAspectNames;
  defaultAspectNames.reserve(defaultAspectRules.size());
  for (const auto& rule : defaultAspectRules) {
    defaultAspectNames.push_back(rule.aspectType);
  }
  const std::vector<std::string> enabledAspectNames = splitCsv(
      m_ctx.getSetting("transit_timeline_aspects", joinCsv(defaultAspectNames)));

  for (const auto& rule : defaultAspectRules) {
    auto currentRule = rule;
    const std::string settingKey = "transit_timeline_orb_" + aspectSettingSuffix(rule.aspectType);
    try {
      currentRule.orbDegrees = std::stod(
          m_ctx.getSetting(settingKey, std::to_string(rule.orbDegrees)));
    } catch (...) {
      currentRule.orbDegrees = rule.orbDegrees;
    }
    m_aspectOptions.push_back({
        currentRule,
        std::find(enabledAspectNames.begin(), enabledAspectNames.end(), rule.aspectType)
            != enabledAspectNames.end()});
  }
}

void TransitTimelinePanel::saveRuleSettings() const {
  std::vector<std::string> enabledTransitObjects;
  for (const auto& option : m_transitPlanetOptions) {
    if (option.enabled) enabledTransitObjects.push_back(option.name);
  }
  m_ctx.setSetting("transit_timeline_transit_objects", joinCsv(enabledTransitObjects));

  std::vector<std::string> enabledAspectNames;
  for (const auto& option : m_aspectOptions) {
    if (option.enabled) enabledAspectNames.push_back(option.rule.aspectType);
    const std::string settingKey = "transit_timeline_orb_" + aspectSettingSuffix(option.rule.aspectType);
    m_ctx.setSetting(settingKey, std::to_string(option.rule.orbDegrees));
  }
  m_ctx.setSetting("transit_timeline_aspects", joinCsv(enabledAspectNames));
}

core::TransitReportService::Rules TransitTimelinePanel::currentRules() const {
  core::TransitReportService::Rules rules;
  rules.natalObjects = core::TransitReportService::defaultNatalObjects();
  for (const auto& option : m_transitPlanetOptions) {
    if (option.enabled) rules.transitObjects.push_back(option.name);
  }
  for (const auto& option : m_aspectOptions) {
    if (option.enabled) rules.aspectRules.push_back(option.rule);
  }
  return rules;
}

std::optional<TransitTimelinePanel::PreparedRequest> TransitTimelinePanel::buildRequest() const {
  if (m_personId <= 0) return std::nullopt;

  auto events = m_ctx.birthEventRepo.findByPersonId(m_personId);
  if (events.empty()) return std::nullopt;

  PreparedRequest prepared;
  prepared.person = m_ctx.personRepo.findById(m_personId);
  prepared.birthEvent = events.front();
  if (prepared.birthEvent->locationId) {
    prepared.location = m_ctx.locationRepo.findById(*prepared.birthEvent->locationId);
  }

  domain::ChartRequest req;
  req.primaryBirthEventId = prepared.birthEvent->birthEventId;
  req.chartType = domain::ChartType::TransitToNatal;

  static const char* kHouseNames[] = {
      "Placidus", "Koch", "Equal", "Whole", "Campanus", "Regiomontanus"};
  int houseIdx = 0;
  try { houseIdx = std::stoi(m_ctx.getSetting("default_house_system", "0")); }
  catch (...) { houseIdx = 0; }
  if (houseIdx < 0 || houseIdx >= static_cast<int>(IM_ARRAYSIZE(kHouseNames))) houseIdx = 0;
  req.defaultHouseSystem = kHouseNames[houseIdx];
  req.zodiacMode = "Tropical";
  req.primaryInput = core::resolveBirthInput(*prepared.birthEvent, prepared.location);

  int policyIdx = 0;
  try { policyIdx = std::stoi(m_ctx.getSetting("unknown_time_policy", "0")); }
  catch (...) { policyIdx = 0; }
  const std::string policy = (policyIdx == 1) ? "noon_default_with_warning"
                                              : "unknown_time_no_houses";
  core::applyUnknownTimePolicy(req, *prepared.birthEvent, policy);

  prepared.request.chartRequest = req;
  prepared.request.subjectName = prepared.person
      ? (prepared.person->displayName.empty() ? prepared.person->fullName : prepared.person->displayName)
      : std::string("selected person");
  prepared.request.startDateTime = m_startDateBuf.data();
  prepared.request.rangeYears = static_cast<double>(std::max(m_periodYears, 1));
  prepared.request.rules = currentRules();
  return prepared;
}

void TransitTimelinePanel::startGeneration() {
  auto prepared = buildRequest();
  if (!prepared) {
    m_statusText = m_personId <= 0
        ? "Select a person from the Library first."
        : "No birth event found. Add birth data in the Library panel.";
    m_statusIsError = true;
    return;
  }

  m_person = prepared->person;
  m_birthEvent = prepared->birthEvent;
  m_location = prepared->location;
  m_periodYears = std::max(m_periodYears, 1);

  if (m_worker.joinable()) {
    m_worker.join();
  }

  const std::uint64_t generationId = ++m_generationId;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_workerStatus.clear();
    m_workerStatusIsError = false;
  }
  m_statusText.clear();
  m_statusIsError = false;
  m_generating.store(true);

  core::TransitReportService::Request request = prepared->request;
  m_worker = std::thread([this, generationId, request]() mutable {
    auto result = [&]() {
      std::lock_guard<std::mutex> engineLock(m_ctx.engineMutex);
      return m_ctx.transitReportService.generate(request);
    }();

    if (generationId != m_generationId.load()) {
      m_generating.store(false);
      return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (result.ok()) {
      m_workerMarkdown = result.value().bodyMarkdown;
      m_workerStatus = "Transit list generated (" + std::to_string(result.value().events.size()) + " events).";
      m_workerStatusIsError = false;
    } else {
      m_workerStatus = "Transit list failed: " + result.error().message;
      m_workerStatusIsError = true;
    }
    m_generating.store(false);
  });
}

void TransitTimelinePanel::syncWorkerState() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_workerMarkdown.empty()) {
    m_displayMarkdown = m_workerMarkdown;
  }
  if (!m_workerStatus.empty()) {
    m_statusText = m_workerStatus;
    m_statusIsError = m_workerStatusIsError;
    m_workerStatus.clear();
  }
}

void TransitTimelinePanel::saveMarkdown() {
  if (m_displayMarkdown.empty()) return;

  const std::string subject = m_person
      ? (m_person->displayName.empty() ? m_person->fullName : m_person->displayName)
      : std::string("Transit Timeline");
  const std::string defaultFileName = sanitizeFileName(subject + " - Transit Timeline") + ".md";

  auto path = showSaveFileDialog(
      L"Save Transit Timeline",
      defaultFileName,
      L"Markdown Files (*.md)\0*.md\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0",
      L"md");
  if (!path) return;

  std::ofstream out(*path, std::ios::binary);
  if (!out.is_open()) {
    m_statusText = "Failed to save transit timeline.";
    m_statusIsError = true;
    return;
  }

  out << m_displayMarkdown;
  if (!out.good()) {
    m_statusText = "Failed to write transit timeline.";
    m_statusIsError = true;
    return;
  }

  m_statusText = "Saved: " + *path;
  m_statusIsError = false;
}

void TransitTimelinePanel::draw() {
  if (!open) return;

  syncWorkerState();

  ImGui::SetNextWindowSize(ImVec2(620, 620), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Transit Timeline", &open)) { ImGui::End(); return; }

  if (m_person) {
    const std::string subject = m_person->displayName.empty() ? m_person->fullName : m_person->displayName;
    ImGui::TextWrapped("Subject: %s", subject.c_str());
  } else {
    ImGui::TextDisabled("Select a person in the Library to generate a transit timeline.");
  }

  ImGui::Separator();

  const bool generating = m_generating.load();
  const auto rules = currentRules();
  const bool hasValidRules = !rules.transitObjects.empty() && !rules.aspectRules.empty();
  if (generating) ImGui::BeginDisabled();
  ImGui::SetNextItemWidth(170.0f);
  ImGui::InputText("Start Date", m_startDateBuf.data(), m_startDateBuf.size());
  ImGui::SameLine();
  ImGui::SetNextItemWidth(100.0f);
  ImGui::InputInt("Period (years)", &m_periodYears);
  m_periodYears = std::max(m_periodYears, 1);
  ImGui::SameLine();
  if (!hasValidRules) ImGui::BeginDisabled();
  if (ImGui::Button("Generate")) {
    startGeneration();
  }
  if (!hasValidRules) ImGui::EndDisabled();
  if (generating) ImGui::EndDisabled();

  ImGui::SameLine();
  const bool canSave = !m_displayMarkdown.empty() && !generating;
  if (!canSave) ImGui::BeginDisabled();
  if (ImGui::Button("Save")) {
    saveMarkdown();
  }
  if (!canSave) ImGui::EndDisabled();

  ImGui::SameLine();
  if (generating) {
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 0.9f, 1.0f), "Generating...");
  } else {
    ImGui::TextDisabled("Format: YYYY-MM-DD or YYYY-MM-DDTHH:MM.");
  }

  if (!m_statusText.empty()) {
    const ImVec4 color = m_statusIsError
        ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f)
        : ImVec4(0.3f, 0.7f, 0.4f, 1.0f);
    ImGui::TextColored(color, "%s", m_statusText.c_str());
  }

  if (ImGui::CollapsingHeader("Rules", ImGuiTreeNodeFlags_DefaultOpen)) {
    bool rulesChanged = false;

    ImGui::TextUnformatted("Transit Planets");
    for (std::size_t i = 0; i < m_transitPlanetOptions.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      rulesChanged |= ImGui::Checkbox("##TransitPlanet", &m_transitPlanetOptions[i].enabled);
      ImGui::SameLine();
      ImGui::TextUnformatted(m_transitPlanetOptions[i].name.c_str());
      if ((i % 3) != 2 && i + 1 < m_transitPlanetOptions.size()) {
        ImGui::SameLine(0.0f, 18.0f);
      }
      ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Aspect Set and Orbs");
    if (ImGui::BeginTable("TransitTimelineAspectRules", 3,
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Use", ImGuiTableColumnFlags_WidthFixed, 48.0f);
      ImGui::TableSetupColumn("Aspect", ImGuiTableColumnFlags_WidthStretch, 1.0f);
      ImGui::TableSetupColumn("Orb", ImGuiTableColumnFlags_WidthFixed, 100.0f);
      ImGui::TableHeadersRow();

      for (std::size_t i = 0; i < m_aspectOptions.size(); ++i) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushID(static_cast<int>(i + 100));
        rulesChanged |= ImGui::Checkbox("##AspectEnabled", &m_aspectOptions[i].enabled);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(m_aspectOptions[i].rule.aspectType.c_str());
        ImGui::TableSetColumnIndex(2);
        rulesChanged |= ImGui::InputDouble("##AspectOrb", &m_aspectOptions[i].rule.orbDegrees, 0.1, 0.5, "%.2f");
        if (m_aspectOptions[i].rule.orbDegrees < 0.05) {
          m_aspectOptions[i].rule.orbDegrees = 0.05;
        }
        ImGui::PopID();
      }

      ImGui::EndTable();
    }

    if (rulesChanged) {
      saveRuleSettings();
    }
  }

  if (!hasValidRules) {
    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
                       "Select at least one transit planet and one aspect rule.");
  }

  ImGui::Separator();

  ImGui::BeginChild("TransitTimelineMarkdown", ImVec2(0, 0), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_HorizontalScrollbar);
  if (!m_displayMarkdown.empty()) {
    markdown_render::renderMarkdown(m_displayMarkdown);
  } else if (!generating) {
    ImGui::TextDisabled("Generate the timeline to review it here before saving.");
  }
  ImGui::EndChild();

  ImGui::End();
}

}  // namespace asteria::ui