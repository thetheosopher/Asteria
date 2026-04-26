#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "domain/person.h"
#include "domain/birth_event.h"
#include "util/atlas_service.h"

namespace asteria::ui {

struct AppContext;

/// Library panel — lists people/birth records, search, and create/edit actions.
/// Backed by PersonRepository and BirthEventRepository via AppContext.
class LibraryPanel {
 public:
  bool open = true;

  explicit LibraryPanel(AppContext& ctx);
  void draw();
  void reloadData();

  /// Currently selected person ID (used by Chart/Compare panels).
  std::int64_t selectedPersonId() const { return m_selectedPersonId; }

 private:
  void refreshPeople();
  void loadBirthEvent();
  void savePerson();
  void saveBirthEvent();
  void updateAtlasResults();
  void applyAtlasEntry(const util::AtlasEntry& entry);

  AppContext& m_ctx;

  char searchBuf_[256] = {};
  int  selectedIdx_     = -1;
  std::int64_t m_selectedPersonId = 0;

  std::vector<domain::Person> m_people;
  domain::BirthEvent m_currentBirthEvent;
  bool m_hasBirthEvent = false;
  bool m_dirty = false;

  // Edit buffers
  char nameBuf_[256]  = {};
  char dateBuf_[64]   = {};
  char timeBuf_[64]   = {};
  char cityBuf_[256]  = {};
  char notesBuf_[512] = {};
  int  timeAccuracy_  = 0; // 0=Exact, 1=Approximate, 2=Unknown

  // Geographic / timezone (numeric, until atlas integration)
  double latDeg_      = 0.0;
  double lonDeg_      = 0.0;
  double tzOffsetHrs_ = 0.0;
  double dstHrs_      = 0.0;

  // Atlas lookup state
  char atlasSearchBuf_[256] = {};
  std::vector<const util::AtlasEntry*> atlasResults_;
  bool atlasPopupOpen_ = false;
  int  atlasSelectedIdx_ = -1;

  float editorPaneHeight_ = -1.0f;
  bool editorPaneCustomized_ = false;
};

}  // namespace asteria::ui
