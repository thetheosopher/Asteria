#pragma once

namespace asteria::data { class SQLiteDatabase; }
namespace asteria::engine { class IChartEngine; }

namespace asteria::ui {

/// Bootstraps the Win32 window, Direct3D 11 device, and ImGui context.
/// Runs the main loop; returns the process exit code.
int runApplication(data::SQLiteDatabase& database, engine::IChartEngine& engine);

}  // namespace asteria::ui
