#pragma once
#include "domain/computed_chart.h"
#include "chart_scene.h"
#include "theme_presets.h"

namespace asteria::render {

ChartScene buildNatalChartScene(const asteria::domain::ComputedChart& chart, const ThemePreset& theme);

}  // namespace asteria::render
