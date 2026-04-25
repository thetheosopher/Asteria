#pragma once

#include <mutex>

#include "data/sqlite_database.h"
#include "data/person_repository.h"
#include "data/birth_event_repository.h"
#include "data/location_repository.h"
#include "data/chart_repository.h"
#include "engine/ichart_engine.h"
#include "core/natal_chart_service.h"
#include "core/comparison_chart_service.h"
#include "core/interpretation_service.h"
#include "core/transit_report_service.h"
#include "core/export_service.h"
#include "util/atlas_service.h"

namespace asteria::ui {

/// Shared application context passed to all UI panels.
/// Owns the repositories and services; panels hold a reference.
struct AppContext {
  data::SQLiteDatabase& database;
  data::PersonRepository personRepo;
  data::BirthEventRepository birthEventRepo;
  data::LocationRepository locationRepo;
  data::ChartRepository chartRepo;
  std::mutex engineMutex;
  engine::IChartEngine& chartEngine;
  core::NatalChartService natalService;
  core::ComparisonChartService comparisonService;
  core::InterpretationService interpretationService;
  core::TransitReportService transitReportService;
  core::ExportService exportService;
  util::AtlasService atlasService;

  AppContext(data::SQLiteDatabase& db, engine::IChartEngine& engine)
      : database(db),
        personRepo(db),
        birthEventRepo(db),
        locationRepo(db),
        chartRepo(db),
        chartEngine(engine),
        natalService(engine),
        comparisonService(engine),
        transitReportService(engine) {
    natalService.attachRepository(&chartRepo);
  }

  // App settings helpers (key-value in app_settings table)
  std::string getSetting(const std::string& key, const std::string& defaultValue = "") const;
  void setSetting(const std::string& key, const std::string& value);
};

}  // namespace asteria::ui
