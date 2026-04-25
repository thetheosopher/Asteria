#include "settings_panel.h"
#include "app_context.h"
#include "imgui.h"
#include <cstring>

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
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
        "Requires a running Ollama instance.");
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

}  // namespace asteria::ui
