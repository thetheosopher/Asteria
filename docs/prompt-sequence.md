# Prompt Sequence for Building Asteria in VSCode

This file defines the recommended prompt order for GitHub Copilot / Copilot Chat when scaffolding the project.

---

## Phase 0: Repository Bootstrap

### Prompt 1
Read the docs folder and summarize the architecture, module boundaries, and first milestone for Asteria. Then propose a concrete CMake-based repo scaffold using C++20 with src/, tests/, and third_party/astrolog/.

### Prompt 2
Generate the top-level CMakeLists.txt and CMakePresets.json for a Windows-first C++20 desktop app named Asteria, with subdirectories for src and tests. Keep the files minimal but production-friendly.

### Prompt 3
Generate a README.md for the repo that explains:
- what Asteria is
- the architecture at a high level
- how to configure and build locally
- how Astrolog is vendored in third_party/astrolog

### Prompt 4
Generate .gitignore for:
- CMake
- Ninja
- VSCode
- build outputs
- local database files
- export/cache folders

---

## Phase 1: Core Domain and App Skeleton

### Prompt 5
Using docs/03-domain-model.md, generate C++ headers and source files for the core domain entities:
- Person
- BirthEvent
- LocationResolution
- ChartRequest
- ComputedChart
- ChartTheme
- InterpretationNote
- ExportArtifact

Keep the first pass lightweight and serializable.

### Prompt 6
Generate a small Result<T, Error> or equivalent error-handling utility suitable for the core and engine layers. Keep it simple and testable.

### Prompt 7
Generate a minimal application skeleton with:
- main()
- application startup
- one main window class
- placeholder navigation for Library, Chart Workspace, Compare Workspace, and Settings

Do not add business logic yet.

---

## Phase 2: Database Layer

### Prompt 8
Using docs/06-database-spec.md, generate:
- an SQLite database wrapper
- migration runner
- schema version table
- first migration for people, birth_events, locations, chart_requests, computed_charts, themes, interpretation_notes, export_artifacts, and app_settings

### Prompt 9
Generate repositories for:
- PersonRepository
- BirthEventRepository
- LocationRepository
- ChartRepository
- ThemeRepository

Keep interfaces clean and independent of UI.

### Prompt 10
Generate unit/integration tests for the migration runner and repositories.

---

## Phase 3: Engine Abstractions

### Prompt 11
Using docs/05-astrolog-engine-integration-spec.md, generate:
- IChartEngine interface
- engine error types
- normalized engine result types
- stub implementations for AstrologEmbeddedEngine and AstrologCliBridge

No vendor-specific parsing yet.

### Prompt 12
Generate a small normalization model for:
- planet positions
- house cusps
- aspects
- warning flags
- chart metadata

These types should integrate cleanly into ComputedChart.

### Prompt 13
Generate tests for the engine abstraction layer using a fake engine implementation.

---

## Phase 4: Rendering Foundation

### Prompt 14
Using docs/04-chart-rendering-spec.md, generate the rendering module skeleton:
- layout model types
- scene graph types
- theme model types
- render service interface

No real drawing yet; just structure.

### Prompt 15
Generate theme preset definitions for:
- Textbook Light
- Textbook Monochrome
- Luxury Light
- Luxury Dark

### Prompt 16
Generate the first layout pass for natal charts:
- outer wheel
- sign ring
- house ring
- object placement ring
- warning banner area

Keep geometry deterministic.

### Prompt 17
Generate SVG serialization for the scene graph with a small test that writes a deterministic sample SVG for a fixture chart.

---

## Phase 5: First End-to-End Natal Flow

### Prompt 18
Generate a small application service that:
- loads a Person and BirthEvent
- creates a natal ChartRequest
- calls IChartEngine
- returns a ComputedChart

### Prompt 19
Generate a placeholder chart workspace UI that:
- requests a chart
- renders a placeholder scene
- shows warning state and metadata

### Prompt 20
Generate a fake engine fixture that returns deterministic natal data so the chart workspace can be tested before Astrolog integration is complete.

---

## Phase 6: Astrolog Bring-Up

### Prompt 21
Inspect the vendored Astrolog source layout under third_party/astrolog and propose a safe adapter integration strategy that minimizes vendor modifications. Do not rewrite vendor files; produce an adapter plan only.

### Prompt 22
Generate the first pass of AstrologEmbeddedEngine with clearly marked TODO areas for vendor-bound normalization. Focus on:
- location resolution
- natal chart computation
- structured error handling

### Prompt 23
Generate a regression harness that compares EmbeddedEngine results against the CliBridge for one or two fixed natal fixtures.

---

## Phase 7: Comparison Charts

### Prompt 24
Extend the engine and domain services to support:
- synastry
- composite

Generate stubs, tests, and service wiring.

### Prompt 25
Generate comparison chart layout primitives for:
- bi-wheel synastry
- composite single wheel

### Prompt 26
Generate compare workspace UI with two-person selection and mode switch between synastry and composite.

---

## Phase 8: Transit-to-Natal

### Prompt 27
Extend the engine abstraction and services to support transit-to-natal chart requests.

### Prompt 28
Generate renderer support for an outer transit ring and distinguish transit glyphs/colors from natal glyphs/colors.

### Prompt 29
Generate UI for selecting a transit date/time context and rendering the transit-to-natal chart.

---

## Phase 9: Unknown-Time and Provenance UX

### Prompt 30
Using docs/10-location-timezone-spec.md and docs/07-ui-ux-spec.md, generate:
- birth record editing UI
- exact/approximate/unknown time options
- noon-default warning flow
- no-houses mode handling
- provenance/confidence fields

### Prompt 31
Generate rendering behavior that suppresses house ring and displays explicit warning treatment when no-houses mode is active.

### Prompt 32
Generate tests for uncertain-time workflows across:
- domain
- engine service layer
- rendering warning states
- interpretation guardrails

---

## Phase 10: Interpretation

### Prompt 33
Using docs/09-interpretation-spec.md, generate a deterministic built-in interpretation service that consumes normalized chart facts and emits markdown sections for natal, synastry, composite, and transit-to-natal.

### Prompt 34
Generate an OllamaIntegration interface and a stub local implementation plan, but do not require it at runtime.

### Prompt 35
Generate interpretation panel UI with:
- built-in interpretation display
- optional “Enhance with Ollama” action
- uncertainty disclaimer handling

---

## Phase 11: Export and Automation

### Prompt 36
Using docs/08-export-spec.md, generate export services for:
- SVG
- PNG

The PNG path may initially rasterize from the shared scene graph using a placeholder implementation if needed.

### Prompt 37
Using docs/11-automation-and-batch-spec.md, generate asteria-cli with commands for:
- compute natal
- compute synastry
- compute composite
- compute transit-to-natal
- export svg
- export png
- resolve location

### Prompt 38
Generate integration tests for automation commands using fixtures.

---

## Phase 12: Installer and Release Hardening

### Prompt 39
Using docs/13-build-release-installer-spec.md, generate:
- packaging folder structure
- installer asset list
- release checklist markdown

### Prompt 40
Generate an Inno Setup script template that packages:
- Asteria app
- runtime dependencies
- optional asteria-cli
- docs/license artifacts

### Prompt 41
Generate a final milestone checklist for V1 readiness, mapping features to tests and release tasks.

---

## Prompting Rules for This Project
- Always read docs/copilot-instructions.md first.
- Do not leak Astrolog-specific raw structures outside src/engine.
- Prefer small, testable increments.
- When code generation depends on unknown vendor details, stub the adapter and mark TODOs.
- Preserve warning and provenance handling as first-class concerns.
