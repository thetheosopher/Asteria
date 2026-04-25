#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "render/chart_scene.h"
#include "render/theme_presets.h"
#include "domain/computed_chart.h"

namespace asteria::ui {

struct AppContext;

/// Chart Workspace panel — displays natal/transit charts, metadata, interpretation.
/// Wired to NatalChartService, InterpretationService, ExportService, and the render layer.
class ChartWorkspacePanel {
 public:
  bool open = true;

  explicit ChartWorkspacePanel(AppContext& ctx);
  void draw();

  /// Set the person to display a chart for (called when Library selection changes).
  void setSelectedPerson(std::int64_t personId);

 private:
  void computeChart();
  void drawChartCanvas();
  void drawInterpretationSidePanel();
  render::ThemePreset currentThemePreset() const;

  AppContext& m_ctx;

  int  currentTheme_     = 0;
  int  currentChartType_ = 0;  // 0=Natal, 1=Transit-to-Natal
  bool showInterpretation_ = false;
  std::string interpretationText_;
  std::string statusMessage_;

  std::int64_t m_personId = 0;
  std::optional<domain::ComputedChart> m_chart;
  render::ChartScene m_scene;
  bool m_hasScene = false;
};

}  // namespace asteria::ui
