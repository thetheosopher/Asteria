# Asteria V1 Milestone Readiness

Maps every V1 feature to its implementation status, test coverage, and release tasks.

## Feature → Test → Release Matrix

### Phase 0: Repository Bootstrap
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| CMake build system | CMakeLists.txt, CMakePresets.json | Build CI | ✅ Complete |
| Project scaffold | src/, tests/, third_party/ | smoke_test.cpp | ✅ Complete |

### Phase 1: Core Domain and App Skeleton
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| Domain entities | src/domain/*.h | domain_test.cpp | ✅ Complete |
| Result<T> error handling | src/core/result.h | smoke_test.cpp | ✅ Complete |
| App skeleton (main) | src/app/main.cpp | Build verification | ✅ Complete |

### Phase 2: Database Layer
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| SQLite wrapper | sqlite_database.h/cpp | database_test.cpp | ✅ Complete |
| Migration runner | migration_runner.h/cpp | database_test.cpp | ✅ Complete |
| Person repository | person_repository.h/cpp | database_test.cpp | ✅ Complete |
| BirthEvent repository | birth_event_repository.h/cpp | database_test.cpp | ✅ Complete |
| Location repository | location_repository.h/cpp | database_test.cpp | ✅ Complete |
| Chart repository | chart_repository.h/cpp | database_test.cpp | ✅ Complete |
| Theme repository | theme_repository.h/cpp | database_test.cpp | ✅ Complete |

### Phase 3: Engine Abstractions
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| IChartEngine interface | i_chart_engine.h | engine_test.cpp | ✅ Complete |
| FakeChartEngine | fake_chart_engine.h/cpp | engine_test.cpp | ✅ Complete |
| Engine stubs | embedded/cli bridge .h/cpp | engine_test.cpp | ✅ Complete |
| Normalization types | PlanetPosition, HouseCusp, Aspect | domain_test.cpp | ✅ Complete |

### Phase 4: Rendering Foundation
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| Chart scene graph | chart_scene.h | render_test.cpp | ✅ Complete |
| Theme presets (4 themes) | theme_presets.h/cpp | render_test.cpp | ✅ Complete |
| Natal chart layout | natal_chart_layout.h/cpp | render_test.cpp | ✅ Complete |
| SVG serialization | svg_serializer.h/cpp | render_test.cpp | ✅ Complete |

### Phase 5: First End-to-End Natal Flow
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| NatalChartService | natal_chart_service.h/cpp | engine_test.cpp | ✅ Complete |
| Fake engine fixture | FakeChartEngine | engine_test.cpp | ✅ Complete |

### Phase 6: Astrolog Bring-Up
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| Astrolog library build | third_party/astrolog/CMakeLists.txt | Build verification | ✅ Complete |
| AstrologAdapter | astrolog_adapter.h/cpp | astrolog_engine_test.cpp | ✅ Complete |
| AstrologEmbeddedEngine | astrolog_embedded_engine.h/cpp | astrolog_engine_test.cpp | ✅ Complete |
| AstrologCliBridge | astrolog_cli_bridge.h/cpp | engine_test.cpp | ✅ Complete |
| Regression harness | astrolog_engine_test.cpp | 14 integration tests | ✅ Complete |

### Phase 7: Comparison Charts
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| Synastry engine support | IChartEngine::computeSynastryChart | comparison_service_test.cpp | ✅ Complete |
| Composite engine support | IChartEngine::computeCompositeChart | comparison_service_test.cpp | ✅ Complete |
| Synastry layout | comparison_chart_layout.h/cpp | comparison_render_test.cpp | ✅ Complete |
| Composite layout | comparison_chart_layout.h/cpp | comparison_render_test.cpp | ✅ Complete |

### Phase 8: Transit-to-Natal
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| Transit engine support | IChartEngine::computeTransitToNatalChart | engine_test.cpp | ✅ Complete |
| Transit chart layout | transit_chart_layout.h/cpp | comparison_render_test.cpp | ✅ Complete |

### Phase 9: Unknown-Time and Provenance UX
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| Time accuracy domain model | BirthEvent fields | uncertain_time_test.cpp | ✅ Complete |
| Noon default policy | Engine + domain | uncertain_time_test.cpp | ✅ Complete |
| No-houses mode | Engine + domain | uncertain_time_test.cpp | ✅ Complete |
| Warning banners (render) | All layout functions | uncertain_time_test.cpp | ✅ Complete |
| Interpretation guardrails | InterpretationService | uncertain_time_test.cpp | ✅ Complete |
| Birth record editing UI | — | — | ⏳ Deferred (Qt not yet integrated) |

### Phase 10: Interpretation
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| Built-in interpretation | interpretation_service.h/cpp | interpretation_test.cpp | ✅ Complete |
| Natal/Synastry/Composite/Transit | All 4 chart types | interpretation_test.cpp | ✅ Complete |
| Ollama integration stub | Interface defined | — | ⏳ Optional for V1 |

### Phase 11: Export and Automation
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| SVG export | export_service.h/cpp | export_test.cpp | ✅ Complete |
| PNG export | export_service.h/cpp | export_test.cpp | ✅ Complete |
| CLI dispatcher | cli_dispatcher.h/cpp | cli_dispatcher_test.cpp | ✅ Complete |
| CLI commands (7 commands) | cli_main.cpp | cli_dispatcher_test.cpp | ✅ Complete |

### Phase 12: Installer and Release Hardening
| Feature | Implementation | Tests | Release |
|---------|---------------|-------|---------|
| Packaging folder structure | tools/packaging/ | — | ✅ Complete |
| Installer asset inventory | installer-assets.md | — | ✅ Complete |
| Inno Setup script | asteria.iss | Manual verification | ✅ Complete |
| Release checklist | release-checklist.md | — | ✅ Complete |
| LICENSE.txt | tools/packaging/LICENSE.txt | — | ✅ Complete |
| NOTICES.txt | tools/packaging/NOTICES.txt | — | ✅ Complete |

---

## Test Summary

| Test Suite | Count | Status |
|-----------|-------|--------|
| Unit tests (domain, engine, render, etc.) | ~77 | ✅ All passing |
| Uncertain-time workflow tests | 24 | ✅ All passing |
| Astrolog integration tests | 14 | ✅ All passing |
| **Total** | **115** | **✅ All passing** |

## Known Deferred Items for V1.1

1. **Native desktop UI** (Qt 6 Widgets) — all backend logic is complete but UI shell is placeholder
2. **Ollama LLM integration** — interface defined, runtime implementation deferred
3. **Birth record editing form** — requires Qt UI
4. **Chart workspace UI** — requires Qt UI
5. **Compare workspace UI** — requires Qt UI
6. **Settings panel** — requires Qt UI
7. **Icon/branding assets** — need design input

## V1 Release Readiness

The application core is complete with:
- Full astrological computation via Astrolog 7.80 embedded engine
- All 4 chart types (Natal, Synastry, Composite, Transit-to-Natal)
- SVG and PNG export
- Built-in interpretation with uncertainty guardrails
- CLI automation tool with 7 commands
- SQLite database with migrations
- 115 passing tests across unit and integration suites
- Installer packaging via Inno Setup

**V1 is code-complete pending UI integration (Qt 6).**
