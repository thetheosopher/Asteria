# Astrolog Engine Integration Specification

## Status
- Implemented in the current repo: `AstrologEmbeddedEngine` is the default computation path and computes the supported chart types into normalized domain output.
- Partially implemented: `AstrologCliBridge` exists but only the natal path is substantially wired; other bridge methods remain stubs or bring-up utilities.
- Planned/future-facing: engine-owned location resolution is not complete; atlas/timezone resolution is currently handled by Asteria services outside the embedded engine path.

## Purpose
Define how Asteria uses Astrolog as a computation engine while keeping:
- UI independent
- rendering independent
- persistence independent
- domain model independent

## Integration Strategy
Primary target:
- AstrologEmbeddedEngine

Secondary utility/tooling path:
- AstrologCliBridge

The embedded engine is the product-default implementation.
The CLI bridge exists for:
- regression comparison
- fixture generation
- automation convenience
- bring-up fallback

## Engine Interface

### IChartEngine
Required methods:
- ResolveLocation(query, optional date/time context) -> LocationResolution candidates
- ComputeNatalChart(ChartRequest) -> ComputedChart
- ComputeSynastryChart(ChartRequest) -> ComputedChart
- ComputeCompositeChart(ChartRequest) -> ComputedChart
- ComputeTransitToNatal(ChartRequest) -> ComputedChart
- GetEngineVersion() -> string

Optional later:
- ValidateBirthEvent(BirthEvent) -> warnings
- WarmCache() -> status

## Normalization Boundary
The engine adapter must translate Astrolog-specific concepts into Asteria domain objects:
- chart metadata
- object positions
- house cusps
- aspects
- uncertainty flags
- location/time resolution
- engine diagnostics

No Astrolog switch syntax or raw output should escape the engine layer.

## Vendor Strategy
- Treat Astrolog source as vendored third-party code
- Prefer minimal modifications
- Build adapter code in Asteria-owned files
- Document any vendor patches clearly
- Preserve version traceability

## Embedded Integration Goals
- Avoid spawning external processes during normal app use
- Expose deterministic chart computation functions
- Normalize results into Asteria’s ComputedChart
- Map errors to application-friendly diagnostics

## CLI Bridge Goals
- Support a limited set of automation/test tasks
- Not used by the primary desktop runtime unless explicitly configured
- Useful as an oracle while embedded normalization is stabilized

## Required Chart Inputs
For all chart types, adapter must accept:
- canonical birth/source data
- house system policy
- unknown-time policy
- location resolution data
- timezone/daylight data
- chart type settings

## Unknown-Time Policies
Supported:
- noon_default_with_warning
- unknown_time_no_houses

Rules:
- If time is unknown and noon default is used, emit warning flags
- If time is unknown and no-houses policy is chosen, suppress house-dependent output where required

## Default Astrology Policy
V1 default house system: Placidus
V1 default zodiac mode: Tropical

These are product decisions. Advanced overrides may exist later behind an advanced settings surface.

## Location Resolution Responsibilities
The engine adapter may leverage Astrolog atlas/time-zone capabilities, but Asteria owns the final provenance model and user-facing confidence semantics.

## Error Handling
The adapter must normalize errors into categories:
- invalid_input
- location_not_resolved
- timezone_ambiguous
- engine_failure
- unsupported_combination
- vendor_data_missing

## Caching
Engine layer may expose stable chart hashes derived from:
- source birth data
- chart type
- house system
- zodiac mode
- time policy
- location resolution

## Logging
Engine logging must include:
- request hash
- vendor engine version
- execution time
- warning flags
- normalization issues

No personally sensitive data should be logged without explicit debug mode.
