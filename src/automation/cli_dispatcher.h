#pragma once
#include <string>
#include <vector>
#include "engine/ichart_engine.h"
#include "core/export_service.h"

namespace asteria::automation {

/// Simple CLI dispatcher for batch/automation workflows.
/// All operations use the same domain and engine layers as the app.
class CliDispatcher {
 public:
  explicit CliDispatcher(engine::IChartEngine& engine);

  struct CliResult {
    bool success = false;
    std::string outputJson;
    std::string errorMessage;
  };

  CliResult computeNatal(std::int64_t birthEventId,
                         const std::string& houseSystem = "Placidus",
                         const std::string& zodiacMode = "Tropical") const;

  CliResult computeSynastry(std::int64_t primaryBirthEventId,
                            std::int64_t secondaryBirthEventId,
                            const std::string& houseSystem = "Placidus",
                            const std::string& zodiacMode = "Tropical") const;

  CliResult computeComposite(std::int64_t primaryBirthEventId,
                             std::int64_t secondaryBirthEventId,
                             const std::string& houseSystem = "Placidus",
                             const std::string& zodiacMode = "Tropical") const;

  CliResult computeTransitToNatal(std::int64_t birthEventId,
                                  const std::string& transitDateTime,
                                  const std::string& houseSystem = "Placidus",
                                  const std::string& zodiacMode = "Tropical") const;

  CliResult exportSvg(std::int64_t birthEventId,
                      const std::string& outputPath,
                      const std::string& themeName = "Textbook Light") const;

  CliResult exportPng(std::int64_t birthEventId,
                      const std::string& outputPath,
                      int widthPx = 1000,
                      int heightPx = 1000,
                      int dpi = 150,
                      const std::string& themeName = "Textbook Light") const;

  CliResult resolveLocation(const std::string& query) const;

 private:
  engine::IChartEngine& m_engine;
  core::ExportService m_exportService;

  domain::ChartRequest makeRequest(std::int64_t primaryId,
                                   const std::string& houseSystem,
                                   const std::string& zodiacMode) const;
  std::string chartToJson(const domain::ComputedChart& chart) const;
};

}  // namespace asteria::automation
