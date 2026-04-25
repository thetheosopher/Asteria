# Architecture Overview

## Architectural Summary
Asteria is a native Windows desktop application written in C++20. It uses:
- a desktop application shell
- a domain model independent of Astrolog internals
- an Astrolog-backed computation adapter
- a vector-first chart rendering pipeline
- a SQLite local database
- an optional automation entry point for batch/script scenarios

## Layered Architecture

### 1. App Layer
Coordinates startup, settings, workspace state, command routing, document/session lifecycle, and application services.

### 2. UI Layer
Responsible for desktop windows, dialogs, chart workspace, library screens, settings screens, export flows, and interaction models.

### 3. Core Application Services
Orchestrates use cases:
- create/edit person
- resolve birth location
- compute natal chart
- compute synastry chart
- compute composite chart
- compute transit-to-natal chart
- export chart
- request interpretation
- run automation jobs

### 4. Domain Layer
Defines canonical models that the rest of the app depends on:
- Person
- BirthEvent
- LocationResolution
- ChartRequest
- ComputedChart
- Theme
- InterpretationNote
- ExportArtifact

This layer must not depend on Astrolog source types or raw CLI output.

### 5. Engine Layer
Implements the chart-computation adapter. Primary implementation target:
- AstrologEmbeddedEngine

Secondary tooling implementation:
- AstrologCliBridge

### 6. Rendering Layer
Owns all visual output. It converts a ComputedChart into:
- a layout model
- a vector scene
- a screen render
- SVG export
- PNG rasterization

### 7. Data Layer
Owns SQLite persistence, schema migrations, repositories, caching, and file-linked export metadata.

### 8. Automation Layer
Exposes a small batch/script interface for regression testing and power-user workflows.

## Core Design Principles
- Source birth data is canonical.
- Computed chart data is cacheable and reproducible.
- Rendering is fully independent of Astrolog’s native graphics.
- The domain model is stable even if the engine adapter changes.
- Uncertain birth-time handling is explicit, visible, and first-class.
- Export fidelity matters as much as on-screen appearance.

## Main Runtime Flow
1. User selects or creates a person.
2. App resolves and validates location/time metadata.
3. App creates a ChartRequest.
4. Engine computes a normalized ComputedChart.
5. Renderer converts ComputedChart to a vector scene.
6. UI displays chart and supporting metadata.
7. Export pipeline writes SVG or PNG using the same scene model.
8. Interpretation pipeline may attach deterministic or LLM-enhanced notes.

## Architectural Boundary Rules
- UI must not parse Astrolog output.
- Engine adapter owns all Astrolog-specific translation.
- Renderer consumes only normalized domain objects.
- Database stores canonical source data and normalized cache artifacts.
- Interpretation must consume structured facts, not screen text.
