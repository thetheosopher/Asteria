#include "library_panel.h"
#include "app_context.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>

namespace asteria::ui {

LibraryPanel::LibraryPanel(AppContext& ctx) : m_ctx(ctx) {
  refreshPeople();
}

void LibraryPanel::refreshPeople() {
  m_people = m_ctx.personRepo.findAll();
}

void LibraryPanel::loadBirthEvent() {
  if (m_selectedPersonId <= 0) {
    m_hasBirthEvent = false;
    return;
  }
  auto events = m_ctx.birthEventRepo.findByPersonId(m_selectedPersonId);
  if (!events.empty()) {
    m_currentBirthEvent = events.front();
    m_hasBirthEvent = true;
    std::strncpy(dateBuf_, m_currentBirthEvent.birthDate.c_str(), sizeof(dateBuf_) - 1);
    if (m_currentBirthEvent.birthTime)
      std::strncpy(timeBuf_, m_currentBirthEvent.birthTime->c_str(), sizeof(timeBuf_) - 1);
    else
      timeBuf_[0] = '\0';
    std::strncpy(cityBuf_, m_currentBirthEvent.cityInput.c_str(), sizeof(cityBuf_) - 1);
    if (m_currentBirthEvent.timeAccuracy == domain::TimeAccuracy::Exact) timeAccuracy_ = 0;
    else if (m_currentBirthEvent.timeAccuracy == domain::TimeAccuracy::Approximate) timeAccuracy_ = 1;
    else timeAccuracy_ = 2;
    latDeg_      = m_currentBirthEvent.latitudeDeg.value_or(0.0);
    lonDeg_      = m_currentBirthEvent.longitudeDeg.value_or(0.0);
    tzOffsetHrs_ = m_currentBirthEvent.timezoneOffsetHours.value_or(0.0);
    dstHrs_      = m_currentBirthEvent.dstOffsetHours.value_or(0.0);
  } else {
    m_currentBirthEvent = {};
    m_currentBirthEvent.personId = m_selectedPersonId;
    m_hasBirthEvent = false;
    dateBuf_[0] = '\0';
    timeBuf_[0] = '\0';
    cityBuf_[0] = '\0';
    timeAccuracy_ = 0;
    latDeg_ = lonDeg_ = tzOffsetHrs_ = dstHrs_ = 0.0;
  }
}

void LibraryPanel::savePerson() {
  if (selectedIdx_ < 0 || selectedIdx_ >= static_cast<int>(m_people.size())) return;
  auto& p = m_people[selectedIdx_];
  p.fullName = nameBuf_;
  p.displayName = p.fullName;
  std::strncpy(notesBuf_, p.notes.c_str(), sizeof(notesBuf_) - 1);
  p.notes = notesBuf_;
  m_ctx.personRepo.update(p);
  m_dirty = false;
}

void LibraryPanel::saveBirthEvent() {
  m_currentBirthEvent.birthDate = dateBuf_;
  if (std::strlen(timeBuf_) > 0)
    m_currentBirthEvent.birthTime = std::string(timeBuf_);
  else
    m_currentBirthEvent.birthTime.reset();
  m_currentBirthEvent.cityInput = cityBuf_;
  switch (timeAccuracy_) {
    case 0: m_currentBirthEvent.timeAccuracy = domain::TimeAccuracy::Exact; break;
    case 1: m_currentBirthEvent.timeAccuracy = domain::TimeAccuracy::Approximate; break;
    default: m_currentBirthEvent.timeAccuracy = domain::TimeAccuracy::Unknown; break;
  }
  m_currentBirthEvent.latitudeDeg          = latDeg_;
  m_currentBirthEvent.longitudeDeg         = lonDeg_;
  m_currentBirthEvent.timezoneOffsetHours  = tzOffsetHrs_;
  m_currentBirthEvent.dstOffsetHours       = dstHrs_;
  if (m_currentBirthEvent.birthEventId > 0) {
    m_ctx.birthEventRepo.update(m_currentBirthEvent);
  } else {
    m_currentBirthEvent.personId = m_selectedPersonId;
    m_ctx.birthEventRepo.insert(m_currentBirthEvent);
  }
  m_hasBirthEvent = true;
}

void LibraryPanel::draw() {
  if (!open) return;
  ImGui::SetNextWindowSize(ImVec2(420, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Library", &open)) { ImGui::End(); return; }

  // Search bar
  ImGui::Text("Search:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-1);
  ImGui::InputText("##LibSearch", searchBuf_, sizeof(searchBuf_));

  ImGui::Separator();

  // Action buttons
  if (ImGui::Button("New Person")) {
    domain::Person p;
    p.fullName = "New Person";
    p.displayName = "New Person";
    if (m_ctx.personRepo.insert(p)) {
      refreshPeople();
      // Select the newly created person
      for (int i = 0; i < static_cast<int>(m_people.size()); ++i) {
        if (m_people[i].personId == p.personId) {
          selectedIdx_ = i;
          m_selectedPersonId = p.personId;
          std::strncpy(nameBuf_, p.fullName.c_str(), sizeof(nameBuf_) - 1);
          notesBuf_[0] = '\0';
          loadBirthEvent();
          break;
        }
      }
    }
  }
  ImGui::SameLine();
  bool hasSelection = selectedIdx_ >= 0 && selectedIdx_ < static_cast<int>(m_people.size());
  if (!hasSelection) ImGui::BeginDisabled();
  if (ImGui::Button("Delete")) {
    if (hasSelection) {
      m_ctx.personRepo.archive(m_people[selectedIdx_].personId);
      selectedIdx_ = -1;
      m_selectedPersonId = 0;
      refreshPeople();
    }
  }
  if (!hasSelection) ImGui::EndDisabled();

  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    refreshPeople();
  }

  ImGui::Separator();

  // People table
  if (ImGui::BeginTable("PeopleTable", 3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit,
        ImVec2(0, ImGui::GetContentRegionAvail().y - 220))) {
    ImGui::TableSetupColumn("Name",    ImGuiTableColumnFlags_None, 180.0f);
    ImGui::TableSetupColumn("Date",    ImGuiTableColumnFlags_None, 120.0f);
    ImGui::TableSetupColumn("Created", ImGuiTableColumnFlags_None, 100.0f);
    ImGui::TableHeadersRow();

    std::string filter(searchBuf_);
    for (int i = 0; i < static_cast<int>(m_people.size()); ++i) {
      auto& p = m_people[i];
      // Substring filter
      if (!filter.empty()) {
        std::string lowerName = p.fullName;
        std::string lowerFilter = filter;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lowerName.find(lowerFilter) == std::string::npos) continue;
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      bool selected = (selectedIdx_ == i);
      if (ImGui::Selectable(p.fullName.c_str(), selected,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        if (selectedIdx_ != i) {
          selectedIdx_ = i;
          m_selectedPersonId = p.personId;
          std::strncpy(nameBuf_, p.fullName.c_str(), sizeof(nameBuf_) - 1);
          std::strncpy(notesBuf_, p.notes.c_str(), sizeof(notesBuf_) - 1);
          loadBirthEvent();
          m_dirty = false;
        }
      }
      ImGui::TableSetColumnIndex(1);
      // Show birth date from the first birth event
      auto events = m_ctx.birthEventRepo.findByPersonId(p.personId);
      if (!events.empty())
        ImGui::TextUnformatted(events.front().birthDate.c_str());
      else
        ImGui::TextDisabled("(none)");
      ImGui::TableSetColumnIndex(2);
      ImGui::TextUnformatted(p.createdAt.substr(0, 10).c_str());
    }
    ImGui::EndTable();
  }

  // Detail editor (below the table)
  if (hasSelection) {
    ImGui::Separator();
    ImGui::Text("Edit Record  (ID: %lld)", static_cast<long long>(m_selectedPersonId));

    if (ImGui::InputText("Name", nameBuf_, sizeof(nameBuf_))) m_dirty = true;

    ImGui::Text("Birth Event:");
    if (ImGui::InputText("Date (YYYY-MM-DD)", dateBuf_, sizeof(dateBuf_))) m_dirty = true;
    if (ImGui::InputText("Time (HH:MM)", timeBuf_, sizeof(timeBuf_))) m_dirty = true;
    if (ImGui::InputText("City", cityBuf_, sizeof(cityBuf_))) m_dirty = true;

    if (ImGui::InputDouble("Latitude (\xC2\xB0, +N)", &latDeg_, 0.0, 0.0, "%.4f"))
      m_dirty = true;
    if (ImGui::InputDouble("Longitude (\xC2\xB0, +E)", &lonDeg_, 0.0, 0.0, "%.4f"))
      m_dirty = true;
    if (ImGui::InputDouble("UTC Offset (hours)", &tzOffsetHrs_, 0.25, 1.0, "%.2f"))
      m_dirty = true;
    if (ImGui::InputDouble("DST Offset (hours)", &dstHrs_, 0.0, 0.0, "%.2f"))
      m_dirty = true;

    const char* accuracyOptions[] = {"Exact", "Approximate", "Unknown"};
    if (ImGui::Combo("Time Accuracy", &timeAccuracy_, accuracyOptions, IM_ARRAYSIZE(accuracyOptions)))
      m_dirty = true;

    if (ImGui::InputTextMultiline("Notes", notesBuf_, sizeof(notesBuf_), ImVec2(-1, 40)))
      m_dirty = true;

    bool wasDirty = m_dirty;
    if (!wasDirty) ImGui::BeginDisabled();
    if (ImGui::Button("Save")) {
      savePerson();
      saveBirthEvent();
      refreshPeople();
    }
    if (!wasDirty) ImGui::EndDisabled();
  }

  ImGui::End();
}

}  // namespace asteria::ui
