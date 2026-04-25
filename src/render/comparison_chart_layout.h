#pragma once
#include "domain/computed_chart.h"
#include "chart_scene.h"
#include "theme_presets.h"

namespace asteria::render {

/// Builds a bi-wheel synastry scene: inner wheel = person A, outer ring = person B.
/// Both charts share the same planet data for now; person A uses the inner positions,
/// person B planets are rendered in an outer ring.
ChartScene buildSynastryChartScene(const domain::ComputedChart& chartA,
                                   const domain::ComputedChart& chartB,
                                   const ThemePreset& theme);

/// Builds a single-wheel composite chart scene (midpoint composite).
ChartScene buildCompositeChartScene(const domain::ComputedChart& compositeChart,
                                    const ThemePreset& theme);

}  // namespace asteria::render
