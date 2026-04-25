#include "settings_panel.h"
#include "app_context.h"
#include "util/ollama_client.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

namespace asteria::ui {

SettingsPanel::SettingsPanel(AppContext& ctx) : m_ctx(ctx) {}

void SettingsPanel::loadSettings() {
  defaultTheme_       = std::stoi(m_ctx.getSetting("default_theme", "0"));
  defaultHouseSystem_ = std::stoi(m_ctx.getSetting("default_house_system", "0"));
  unknownTimePolicy_  = std::stoi(m_ctx.getSetting("unknown_time_policy", "0"));
  defaultExportFormat_= std::stoi(m_ctx.getSetting("default_export_format", "0"));
  ollamaEnabled_      = m_ctx.getSetting("ollama_enabled", "0") == "1";
  auto endpoint       = m_ctx.getSetting("ollama_endpoint", "http://localhost:11434");
  std::strncpy(ollamaEndpoint_, endpoint.c_str(), sizeof(ollamaEndpoint_) - 1);
  auto savedModel     = m_ctx.getSetting("ollama_model", "");
  if (ollamaEnabled_ && !savedModel.empty()) {
    refreshOllamaModels();
    auto it = std::find(ollamaModels_.begin(), ollamaModels_.end(), savedModel);
    if (it != ollamaModels_.end())
      ollamaModelIndex_ = static_cast<int>(it - ollamaModels_.begin());
  }
  m_loaded = true;
  m_dirty = false;
}

void SettingsPanel::saveSettings() {
  m_ctx.setSetting("default_theme", std::to_string(defaultTheme_));
  m_ctx.setSetting("default_house_system", std::to_string(defaultHouseSystem_));
  m_ctx.setSetting("unknown_time_policy", std::to_string(unknownTimePolicy_));
  m_ctx.setSetting("default_export_format", std::to_string(defaultExportFormat_));
  m_ctx.setSetting("ollama_enabled", ollamaEnabled_ ? "1" : "0");
  m_ctx.setSetting("ollama_endpoint", ollamaEndpoint_);
  if (ollamaModelIndex_ >= 0 && ollamaModelIndex_ < static_cast<int>(ollamaModels_.size()))
    m_ctx.setSetting("ollama_model", ollamaModels_[ollamaModelIndex_]);
  m_dirty = false;
}

void SettingsPanel::draw() {
  if (!open) return;
  if (!m_loaded) loadSettings();

  ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Settings", &open)) { ImGui::End(); return; }

  ImGui::Text("Application Settings");
  ImGui::Separator();

  // Theme
  const char* themes[] = {"Textbook Light", "Textbook Monochrome",
                           "Luxury Light", "Luxury Dark"};
  if (ImGui::Combo("Default Theme", &defaultTheme_, themes, IM_ARRAYSIZE(themes)))
    m_dirty = true;

  // House system
  const char* houseSystems[] = {"Placidus", "Koch", "Equal", "Whole Sign",
                                 "Campanus", "Regiomontanus"};
  if (ImGui::Combo("House System", &defaultHouseSystem_,
               houseSystems, IM_ARRAYSIZE(houseSystems)))
    m_dirty = true;

  // Unknown time policy
  const char* timePolicies[] = {"Noon Default (no houses)",
                                 "Noon Default (keep houses)",
                                 "Prompt User"};
  if (ImGui::Combo("Unknown Time Policy", &unknownTimePolicy_,
               timePolicies, IM_ARRAYSIZE(timePolicies)))
    m_dirty = true;

  ImGui::Separator();
  ImGui::Text("Export");
  const char* exportFormats[] = {"SVG", "PNG"};
  if (ImGui::Combo("Default Export", &defaultExportFormat_,
               exportFormats, IM_ARRAYSIZE(exportFormats)))
    m_dirty = true;

  ImGui::Separator();
  ImGui::Text("Interpretation");
  if (ImGui::Checkbox("Enable Ollama Integration", &ollamaEnabled_))
    m_dirty = true;
  if (ollamaEnabled_) {
    if (ImGui::InputText("Ollama Endpoint", ollamaEndpoint_, sizeof(ollamaEndpoint_)))
      m_dirty = true;

    // Model selector
    ImGui::SameLine();
    if (ImGui::Button("Refresh Models")) {
      refreshOllamaModels();
    }
    if (!ollamaModels_.empty()) {
      // Build null-separated string for ImGui::Combo.
      std::string comboStr;
      for (const auto& m : ollamaModels_) { comboStr += m; comboStr += '\0'; }
      comboStr += '\0';
      if (ImGui::Combo("Ollama Model", &ollamaModelIndex_, comboStr.c_str()))
        m_dirty = true;
    }
    if (!ollamaModelStatus_.empty()) {
      bool isErr = ollamaModelStatus_.find("Error") != std::string::npos ||
                   ollamaModelStatus_.find("failed") != std::string::npos;
      ImVec4 col = isErr ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f)
                         : ImVec4(0.4f, 0.7f, 0.4f, 1.0f);
      ImGui::TextColored(col, "%s", ollamaModelStatus_.c_str());
    } else {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
          "Requires a running Ollama instance.");
    }
  }

  ImGui::Separator();
  bool wasDirty = m_dirty;
  if (!wasDirty) ImGui::BeginDisabled();
  if (ImGui::Button("Save Settings")) {
    saveSettings();
  }
  if (!wasDirty) ImGui::EndDisabled();
  if (!m_dirty) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 0.4f, 1.0f), "Saved");
  }

  ImGui::End();
}

void SettingsPanel::refreshOllamaModels() {
  util::OllamaClient client(ollamaEndpoint_);
  ollamaModels_ = client.listModels();
  if (ollamaModels_.empty()) {
    ollamaModelStatus_ = client.lastError().empty()
        ? "No models found on server."
        : "Error: " + client.lastError();
    ollamaModelIndex_ = -1;
  } else {
    ollamaModelStatus_ = std::to_string(ollamaModels_.size()) + " model(s) found.";
    // Try to preserve current selection.
    auto saved = m_ctx.getSetting("ollama_model", "");
    auto it = std::find(ollamaModels_.begin(), ollamaModels_.end(), saved);
    if (it != ollamaModels_.end())
      ollamaModelIndex_ = static_cast<int>(it - ollamaModels_.begin());
    else
      ollamaModelIndex_ = 0;
  }
}

}  // namespace asteria::ui
