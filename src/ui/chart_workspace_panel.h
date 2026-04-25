#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "render/chart_scene.h"
#include "render/theme_presets.h"
#include "domain/person.h"
#include "domain/birth_event.h"
#include "domain/location_resolution.h"
#include "domain/computed_chart.h"

namespace asteria::ui {

struct AppContext;
class AiInterpretationPanel;

/// Chart Workspace panel — displays natal/transit charts, metadata, interpretation.
/// Wired to NatalChartService, InterpretationService, ExportService, and the render layer.
class ChartWorkspacePanel {
 public:
  bool open = true;

  explicit ChartWorkspacePanel(AppContext& ctx);
  void draw();

  /// Set the person to display a chart for (called when Library selection changes).
  void setSelectedPerson(std::int64_t personId);

  /// Optionally link the AI interpretation panel so computed charts are pushed to it.
  void setAiPanel(AiInterpretationPanel* panel) { m_aiPanel = panel; }

 private:
  void computeChart();
  void drawChartCanvas();
  void drawInfoSidePanel();
  void drawInterpretationSidePanel();
  std::string buildClipboardText() const;
  render::ThemePreset currentThemePreset() const;

  AppContext& m_ctx;

  int  currentChartType_ = 0;  // 0=Natal, 1=Transit-to-Natal
  int  exportPngProfile_ = 0;
  int  exportLayoutTemplate_ = 0;
  std::string interpretationText_;
  std::string statusMessage_;

  std::int64_t m_personId = 0;
  std::optional<domain::Person> m_person;
  std::optional<domain::BirthEvent> m_birthEvent;
  std::optional<domain::LocationResolution> m_location;
  std::optional<domain::ComputedChart> m_chart;
  render::ChartScene m_scene;
  bool m_hasScene = false;
  bool m_focusOnNextDraw = false;
  AiInterpretationPanel* m_aiPanel = nullptr;
};

}  // namespace asteria::ui
