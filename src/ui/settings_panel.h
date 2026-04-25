#pragma once

#include <string>

namespace asteria::ui {

struct AppContext;

/// Settings panel — defaults for house system, theme, unknown-time policy, etc.
/// Persisted to the app_settings key-value table via AppContext.
class SettingsPanel {
 public:
  bool open = true;

  explicit SettingsPanel(AppContext& ctx);
  void draw();

 private:
  void loadSettings();
  void saveSettings();

  AppContext& m_ctx;

  int  defaultTheme_       = 0;
  int  defaultHouseSystem_ = 0;
  int  unknownTimePolicy_  = 0;
  int  defaultExportFormat_ = 0;
  bool ollamaEnabled_      = false;
  char ollamaEndpoint_[256] = "http://localhost:11434";
  bool m_dirty = false;
  bool m_loaded = false;
};

}  // namespace asteria::ui
