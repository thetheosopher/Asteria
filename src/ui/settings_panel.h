#pragma once

#include <string>
#include <vector>

namespace asteria::ui {

struct AppContext;

/// Settings panel — defaults for house system, theme, unknown-time policy, etc.
/// Persisted to the app_settings key-value table via AppContext.
class SettingsPanel {
 public:
  bool open = false;

  explicit SettingsPanel(AppContext& ctx);
  void draw();
  void ensureLoaded();
  void drawViewMenuContents();
  bool isOllamaEnabled() const { return ollamaEnabled_; }

 private:
  void loadSettings();
  void refreshOllamaModels();

  AppContext& m_ctx;

  int  defaultTheme_       = 0;
  int  defaultHouseSystem_ = 0;
  int  unknownTimePolicy_  = 0;
  int  defaultExportFormat_ = 0;
  int  defaultExportPngProfile_ = 0;
  int  defaultExportLayout_ = 0;
  bool ollamaEnabled_      = false;
  char ollamaEndpoint_[256] = "http://localhost:11434";
  std::vector<std::string> ollamaModels_;
  int  ollamaModelIndex_   = -1;
  std::string ollamaModelStatus_;  // status message from last refresh
  bool m_dirty = false;
  bool m_loaded = false;
};

}  // namespace asteria::ui
