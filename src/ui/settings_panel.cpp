#include "settings_panel.h"
#include "app_context.h"
#include "util/ollama_client.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

namespace asteria::ui {

SettingsPanel::SettingsPanel(AppContext& ctx) : m_ctx(ctx) {}

void SettingsPanel::ensureLoaded() {
  if (!m_loaded) loadSettings();
}

void SettingsPanel::loadSettings() {
  defaultTheme_       = std::stoi(m_ctx.getSetting("default_theme", "0"));
  defaultHouseSystem_ = std::stoi(m_ctx.getSetting("default_house_system", "0"));
  unknownTimePolicy_  = std::stoi(m_ctx.getSetting("unknown_time_policy", "0"));
  defaultExportFormat_= std::stoi(m_ctx.getSetting("default_export_format", "0"));
  defaultExportPngProfile_ = std::stoi(m_ctx.getSetting("default_export_png_profile", "0"));
  defaultExportLayout_ = std::stoi(m_ctx.getSetting("default_export_layout", "0"));
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

void SettingsPanel::draw() {
  if (!open) return;
  ImGui::SetNextWindowSize(ImVec2(420, 120), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Settings", &open)) { ImGui::End(); return; }
  ImGui::TextWrapped("Settings have moved to the View menu. Use the Theme, Chart Defaults, Export Defaults, and Interpretation submenus there.");
  ImGui::End();
}

void SettingsPanel::drawViewMenuContents() {
  ensureLoaded();

  if (ImGui::BeginMenu("Theme")) {
    const char* themes[] = {"Textbook Light", "Textbook Monochrome", "Luxury Light", "Luxury Dark"};
    for (int i = 0; i < IM_ARRAYSIZE(themes); ++i) {
      if (ImGui::MenuItem(themes[i], nullptr, defaultTheme_ == i)) {
        defaultTheme_ = i;
        m_ctx.setSetting("default_theme", std::to_string(defaultTheme_));
      }
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Chart Defaults")) {
    const char* houseSystems[] = {"Placidus", "Koch", "Equal", "Whole Sign", "Campanus", "Regiomontanus"};
    if (ImGui::BeginMenu("House System")) {
      for (int i = 0; i < IM_ARRAYSIZE(houseSystems); ++i) {
        if (ImGui::MenuItem(houseSystems[i], nullptr, defaultHouseSystem_ == i)) {
          defaultHouseSystem_ = i;
          m_ctx.setSetting("default_house_system", std::to_string(defaultHouseSystem_));
        }
      }
      ImGui::EndMenu();
    }

    const char* timePolicies[] = {"Noon Default (no houses)", "Noon Default (keep houses)", "Prompt User"};
    if (ImGui::BeginMenu("Unknown Time Policy")) {
      for (int i = 0; i < IM_ARRAYSIZE(timePolicies); ++i) {
        if (ImGui::MenuItem(timePolicies[i], nullptr, unknownTimePolicy_ == i)) {
          unknownTimePolicy_ = i;
          m_ctx.setSetting("unknown_time_policy", std::to_string(unknownTimePolicy_));
        }
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Export Defaults")) {
    const char* exportFormats[] = {"SVG", "PNG"};
    if (ImGui::BeginMenu("Preferred Format")) {
      for (int i = 0; i < IM_ARRAYSIZE(exportFormats); ++i) {
        if (ImGui::MenuItem(exportFormats[i], nullptr, defaultExportFormat_ == i)) {
          defaultExportFormat_ = i;
          m_ctx.setSetting("default_export_format", std::to_string(defaultExportFormat_));
        }
      }
      ImGui::EndMenu();
    }

    const char* pngProfiles[] = {"Screen Share", "Print Preview", "High Resolution"};
    if (ImGui::BeginMenu("PNG Profile")) {
      for (int i = 0; i < IM_ARRAYSIZE(pngProfiles); ++i) {
        if (ImGui::MenuItem(pngProfiles[i], nullptr, defaultExportPngProfile_ == i)) {
          defaultExportPngProfile_ = i;
          m_ctx.setSetting("default_export_png_profile", std::to_string(defaultExportPngProfile_));
        }
      }
      ImGui::EndMenu();
    }

    const char* layoutTemplates[] = {"Chart Only", "Reference Sheet"};
    if (ImGui::BeginMenu("Layout")) {
      for (int i = 0; i < IM_ARRAYSIZE(layoutTemplates); ++i) {
        if (ImGui::MenuItem(layoutTemplates[i], nullptr, defaultExportLayout_ == i)) {
          defaultExportLayout_ = i;
          m_ctx.setSetting("default_export_layout", std::to_string(defaultExportLayout_));
        }
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Interpretation")) {
    if (ImGui::Checkbox("Enable Ollama Integration", &ollamaEnabled_)) {
      m_ctx.setSetting("ollama_enabled", ollamaEnabled_ ? "1" : "0");
    }
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::InputText("Endpoint", ollamaEndpoint_, sizeof(ollamaEndpoint_))) {
      m_ctx.setSetting("ollama_endpoint", ollamaEndpoint_);
    }

    if (ImGui::Button("Refresh Models")) {
      refreshOllamaModels();
    }

    if (!ollamaModels_.empty() && ImGui::BeginMenu("Ollama Model")) {
      for (int i = 0; i < static_cast<int>(ollamaModels_.size()); ++i) {
        if (ImGui::MenuItem(ollamaModels_[i].c_str(), nullptr, ollamaModelIndex_ == i)) {
          ollamaModelIndex_ = i;
          m_ctx.setSetting("ollama_model", ollamaModels_[i]);
        }
      }
      ImGui::EndMenu();
    }

    if (!ollamaModelStatus_.empty()) {
      const bool isErr = ollamaModelStatus_.find("Error") != std::string::npos ||
                         ollamaModelStatus_.find("failed") != std::string::npos;
      ImGui::TextColored(isErr ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f)
                               : ImVec4(0.4f, 0.7f, 0.4f, 1.0f),
                         "%s",
                         ollamaModelStatus_.c_str());
    }
    ImGui::EndMenu();
  }
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
