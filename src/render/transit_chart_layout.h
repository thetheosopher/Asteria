#pragma once
#include "domain/computed_chart.h"
#include "chart_scene.h"
#include "theme_presets.h"

namespace asteria::render {

/// Builds a transit-to-natal chart scene with an outer transit ring.
/// Transit planets are rendered in a distinct style from natal planets.
ChartScene buildTransitToNatalChartScene(const domain::ComputedChart& natalChart,
                                         const domain::ComputedChart& transitChart,
                                         const ThemePreset& theme);

}  // namespace asteria::render
