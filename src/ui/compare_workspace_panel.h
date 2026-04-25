#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "domain/types.h"
#include "domain/person.h"
#include "domain/computed_chart.h"
#include "render/chart_scene.h"

namespace asteria::ui {

struct AppContext;
class AiInterpretationPanel;

/// Compare Workspace panel — synastry/composite with two-person selection.
/// Wired to ComparisonChartService, PersonRepository, BirthEventRepository.
class CompareWorkspacePanel {
 public:
  bool open = true;

  explicit CompareWorkspacePanel(AppContext& ctx);
  void draw();

  void setAiPanel(AiInterpretationPanel* panel) { m_aiPanel = panel; }

 private:
  void refreshPeople();
  void computeComparison();
  void drawChartCanvas();

  AppContext& m_ctx;

  std::vector<domain::Person> m_people;
  std::vector<std::string> m_nameLabels;

  int  personA_     = 0;
  int  personB_     = 0;
  int  compareMode_ = 0;  // 0=Synastry, 1=Composite
  int  exportPngProfile_ = 0;
  int  exportLayoutTemplate_ = 0;
  std::string statusMessage_;

  std::optional<domain::ComputedChart> m_chartA;
  std::optional<domain::ComputedChart> m_chartB;
  std::optional<domain::ComputedChart> m_compChart;
  domain::ChartType m_lastComputedAiChartType_ = domain::ChartType::Synastry;
  std::string m_lastComputedAiSourceLabel_;
  bool m_hasResult = false;
  AiInterpretationPanel* m_aiPanel = nullptr;
};

}  // namespace asteria::ui
