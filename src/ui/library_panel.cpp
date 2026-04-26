#include "library_panel.h"
#include "app_context.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>

namespace asteria::ui {

namespace {

constexpr float kMinListPaneHeight = 140.0f;
constexpr float kMinEditorPaneHeight = 280.0f;
constexpr float kPreferredEditorPaneHeight = 520.0f;
constexpr float kSplitterHeight = 8.0f;
constexpr float kEditorLabelWidth = 150.0f;

void drawVerticalSplitter(const char* id,
                         float* bottomHeight,
                         float minBottomHeight,
                         float maxBottomHeight,
                         bool* customized) {
  ImVec2 cursor = ImGui::GetCursorScreenPos();
  ImVec2 size(ImGui::GetContentRegionAvail().x, kSplitterHeight);
  if (size.x < 1.0f) size.x = 1.0f;

  ImGui::InvisibleButton(id, size);
  if (ImGui::IsItemActive()) {
    *bottomHeight -= ImGui::GetIO().MouseDelta.y;
    *customized = true;
  }

  *bottomHeight = std::clamp(*bottomHeight, minBottomHeight, maxBottomHeight);

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImU32 color = ImGui::GetColorU32(ImGui::IsItemActive() || ImGui::IsItemHovered()
                                       ? ImGuiCol_SeparatorActive
                                       : ImGuiCol_Separator);
  drawList->AddRectFilled(cursor,
                          ImVec2(cursor.x + size.x, cursor.y + size.y),
                          color,
                          2.0f);
}

void editorLabel(const char* text) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(text);
}

bool beginEditorTable() {
  return ImGui::BeginTable("LibraryEditorForm",
                           2,
                           ImGuiTableFlags_SizingStretchProp,
                           ImVec2(-1.0f, 0.0f));
}

void setupEditorColumns() {
  ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, kEditorLabelWidth);
  ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
}

void nextEditorFieldRow() {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
}

}  // namespace

LibraryPanel::LibraryPanel(AppContext& ctx) : m_ctx(ctx) {
  refreshPeople();
}

void LibraryPanel::reloadData() {
  refreshPeople();

  selectedIdx_ = -1;
  if (m_selectedPersonId > 0) {
    for (int index = 0; index < static_cast<int>(m_people.size()); ++index) {
      if (m_people[index].personId == m_selectedPersonId) {
        selectedIdx_ = index;
        std::strncpy(nameBuf_, m_people[index].fullName.c_str(), sizeof(nameBuf_) - 1);
        nameBuf_[sizeof(nameBuf_) - 1] = '\0';
        std::strncpy(notesBuf_, m_people[index].notes.c_str(), sizeof(notesBuf_) - 1);
        notesBuf_[sizeof(notesBuf_) - 1] = '\0';
        loadBirthEvent();
        m_dirty = false;
        return;
      }
    }
  }

  m_selectedPersonId = 0;
  nameBuf_[0] = '\0';
  notesBuf_[0] = '\0';
  dateBuf_[0] = '\0';
  timeBuf_[0] = '\0';
  cityBuf_[0] = '\0';
  atlasSearchBuf_[0] = '\0';
  atlasResults_.clear();
  atlasSelectedIdx_ = -1;
  m_currentBirthEvent = {};
  m_hasBirthEvent = false;
  m_dirty = false;
  latDeg_ = 0.0;
  lonDeg_ = 0.0;
  tzOffsetHrs_ = 0.0;
  dstHrs_ = 0.0;
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

  const float availableHeight = ImGui::GetContentRegionAvail().y;
  const bool showEditor = hasSelection;
  float editorHeight = 0.0f;
  if (showEditor) {
    const float maxEditorHeight = (std::max)(kMinEditorPaneHeight,
                                             availableHeight - kMinListPaneHeight - kSplitterHeight);
    if (!editorPaneCustomized_) {
      editorPaneHeight_ = (std::min)(kPreferredEditorPaneHeight, maxEditorHeight);
    } else {
      editorPaneHeight_ = std::clamp(editorPaneHeight_, kMinEditorPaneHeight, maxEditorHeight);
    }
    editorHeight = editorPaneHeight_;
  }

  const float listHeight = showEditor
      ? (std::max)(kMinListPaneHeight, availableHeight - editorHeight - kSplitterHeight)
      : availableHeight;
  if (ImGui::BeginChild("LibraryPeoplePane", ImVec2(0.0f, listHeight), ImGuiChildFlags_Borders)) {
    if (ImGui::BeginTable("PeopleTable", 3,
          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
          ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
          ImGuiTableFlags_SizingFixedFit,
          ImVec2(0.0f, -1.0f))) {
      ImGui::TableSetupColumn("Name",    ImGuiTableColumnFlags_None, 180.0f);
      ImGui::TableSetupColumn("Date",    ImGuiTableColumnFlags_None, 120.0f);
      ImGui::TableSetupColumn("Created", ImGuiTableColumnFlags_None, 100.0f);
      ImGui::TableHeadersRow();

      std::string filter(searchBuf_);
      for (int i = 0; i < static_cast<int>(m_people.size()); ++i) {
        auto& p = m_people[i];
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
        auto events = m_ctx.birthEventRepo.findByPersonId(p.personId);
        if (!events.empty()) ImGui::TextUnformatted(events.front().birthDate.c_str());
        else ImGui::TextDisabled("(none)");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(p.createdAt.substr(0, 10).c_str());
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();

  if (showEditor) {
    drawVerticalSplitter("LibraryListEditorSplitter",
                         &editorPaneHeight_,
                         kMinEditorPaneHeight,
                         (std::max)(kMinEditorPaneHeight, availableHeight - kMinListPaneHeight - kSplitterHeight),
                         &editorPaneCustomized_);

    if (ImGui::BeginChild("LibraryEditorPane", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
      ImGui::Text("Edit Record  (ID: %lld)", static_cast<long long>(m_selectedPersonId));
      ImGui::Separator();

      if (beginEditorTable()) {
        setupEditorColumns();

        nextEditorFieldRow();
        editorLabel("Name");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##Name", nameBuf_, sizeof(nameBuf_))) m_dirty = true;

        nextEditorFieldRow();
        editorLabel("Birth Date");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##BirthDate", dateBuf_, sizeof(dateBuf_))) m_dirty = true;

        nextEditorFieldRow();
        editorLabel("Birth Time");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##BirthTime", timeBuf_, sizeof(timeBuf_))) m_dirty = true;

        if (m_ctx.atlasService.isLoaded()) {
          nextEditorFieldRow();
          editorLabel("Location Lookup");
          ImGui::TableSetColumnIndex(1);
          ImGui::SetNextItemWidth(-1.0f);
          bool searchChanged = ImGui::InputText("##AtlasSearch", atlasSearchBuf_, sizeof(atlasSearchBuf_));
          ImGui::TextDisabled("Type city name to search the atlas.");
          if (searchChanged) {
            updateAtlasResults();
          }

          if (!atlasResults_.empty()) {
            float atlasListHeight = std::min(static_cast<float>(atlasResults_.size()) * 20.0f + 6.0f, 140.0f);
            if (ImGui::BeginChild("##AtlasResults", ImVec2(-1.0f, atlasListHeight), ImGuiChildFlags_Borders)) {
              for (int i = 0; i < static_cast<int>(atlasResults_.size()); ++i) {
                auto* entry = atlasResults_[i];
                char label[512];
                std::snprintf(label, sizeof(label), "%s (%s)  %.4f%s, %.4f%s  [%s]",
                              entry->name.c_str(),
                              entry->regionCode.c_str(),
                              std::abs(entry->latitude),
                              entry->latitude >= 0 ? "N" : "S",
                              std::abs(entry->longitude),
                              entry->longitude >= 0 ? "E" : "W",
                              entry->timezoneName.c_str());
                if (ImGui::Selectable(label, atlasSelectedIdx_ == i)) {
                  applyAtlasEntry(*entry);
                  atlasResults_.clear();
                  atlasSearchBuf_[0] = '\0';
                  break;
                }
              }
            }
            ImGui::EndChild();
          }
        }

        nextEditorFieldRow();
        editorLabel("City");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##City", cityBuf_, sizeof(cityBuf_))) m_dirty = true;

        nextEditorFieldRow();
        editorLabel("Latitude (deg, +N)");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputDouble("##Latitude", &latDeg_, 0.0, 0.0, "%.4f")) m_dirty = true;

        nextEditorFieldRow();
        editorLabel("Longitude (deg, +E)");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputDouble("##Longitude", &lonDeg_, 0.0, 0.0, "%.4f")) m_dirty = true;

        nextEditorFieldRow();
        editorLabel("UTC Offset (hours)");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputDouble("##UtcOffset", &tzOffsetHrs_, 0.25, 1.0, "%.2f")) m_dirty = true;

        nextEditorFieldRow();
        editorLabel("DST Offset (hours)");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputDouble("##DstOffset", &dstHrs_, 0.0, 0.0, "%.2f")) m_dirty = true;

        nextEditorFieldRow();
        editorLabel("Time Accuracy");
        ImGui::TableSetColumnIndex(1);
        {
          const char* accuracyOptions[] = {"Exact", "Approximate", "Unknown"};
          ImGui::SetNextItemWidth(-1.0f);
          if (ImGui::Combo("##TimeAccuracy", &timeAccuracy_, accuracyOptions, IM_ARRAYSIZE(accuracyOptions))) {
            m_dirty = true;
          }
        }

        nextEditorFieldRow();
        editorLabel("Notes");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextMultiline("##Notes", notesBuf_, sizeof(notesBuf_), ImVec2(-1.0f, 96.0f))) {
          m_dirty = true;
        }

        ImGui::EndTable();
      }

      bool wasDirty = m_dirty;
      if (!wasDirty) ImGui::BeginDisabled();
      if (ImGui::Button("Save")) {
        savePerson();
        saveBirthEvent();
        refreshPeople();
      }
      if (!wasDirty) ImGui::EndDisabled();
    }
    ImGui::EndChild();
  }

  ImGui::End();
}

void LibraryPanel::updateAtlasResults() {
  std::string query(atlasSearchBuf_);
  if (query.size() < 2) {
    atlasResults_.clear();
    return;
  }
  atlasResults_ = m_ctx.atlasService.search(query, 20);
  atlasSelectedIdx_ = -1;
}

void LibraryPanel::applyAtlasEntry(const util::AtlasEntry& entry) {
  // Populate city, lat, lon
  std::strncpy(cityBuf_, entry.name.c_str(), sizeof(cityBuf_) - 1);
  cityBuf_[sizeof(cityBuf_) - 1] = '\0';
  latDeg_ = entry.latitude;
  lonDeg_ = entry.longitude;

  // Resolve timezone using the birth date (if available)
  int year = 2000, month = 1, day = 1;
  double timeHrs = 12.0;
  if (std::strlen(dateBuf_) >= 10) {
    std::sscanf(dateBuf_, "%4d-%2d-%2d", &year, &month, &day);
  }
  if (std::strlen(timeBuf_) >= 5) {
    int hh = 12, mm = 0;
    std::sscanf(timeBuf_, "%d:%d", &hh, &mm);
    timeHrs = hh + mm / 60.0;
  }

  auto tz = m_ctx.atlasService.resolveTimezone(entry.timezoneName,
                                                year, month, day, timeHrs);
  tzOffsetHrs_ = tz.utcOffsetHours;
  dstHrs_      = tz.dstOffsetHours;

  // Store timezone name on the birth event
  m_currentBirthEvent.timezoneName = entry.timezoneName;
  m_dirty = true;
}

}  // namespace asteria::ui
