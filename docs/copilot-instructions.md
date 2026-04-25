# Copilot Instructions for Asteria

## Project Summary
Asteria is a native Windows C++20 desktop application for power astrologers. It uses:
- Astrolog as a computation engine behind an adapter
- a normalized internal domain model
- a vector-first rendering engine
- SQLite for local persistence
- optional automation and optional Ollama enhancement

## Primary Architectural Rule
Never let Astrolog-specific types, raw output text, or switch syntax leak outside the engine layer.

## Module Responsibilities

### src/domain
Pure domain entities and value objects.
No UI, DB, or vendor dependencies.

### src/core
Application services and orchestration.
May depend on domain + abstractions.
Must not depend directly on Qt widgets.

### src/engine
Astrolog adapter implementations and normalization logic.

### src/render
Chart layout, scene graph, screen rendering, SVG serialization, PNG rasterization.

### src/data
SQLite repositories, migrations, persistence mapping.

### src/ui
Desktop windows, dialogs, workspace components, user interactions.

### src/automation
Batch/CLI entry points built on core services.

## Coding Rules
- Use C++20.
- Prefer RAII and value semantics where appropriate.
- Keep headers tight and dependencies minimal.
- Favor composition over inheritance unless abstractions genuinely improve testability.
- Separate interface from implementation when module boundaries matter.
- Do not place business logic in UI classes.
- Do not place rendering logic in data repositories.
- Do not place DB access in engine code.

## Naming Conventions
- Types: PascalCase
- Methods/functions: camelCase
- Private members: m_memberName
- Files: snake_case.cpp / snake_case.h or module-consistent naming

## Error Handling
- Return structured errors or result types where possible
- Normalize vendor errors at engine boundary
- Surface user-facing warnings explicitly

## Testing Expectations
When generating code, also suggest:
- unit tests for domain logic
- integration tests for engine normalization
- golden tests for render/export determinism

## Rendering Guidance
- Treat vector scene generation as the canonical output
- Screen render and exports should share the same layout/scene path
- Keep layout deterministic for fixture-based testing

## Unknown-Time Handling
This is a first-class product feature. Any code touching chart creation, interpretation, or rendering must preserve:
- exact time
- approximate time
- unknown time
- noon default warning
- no-houses mode

## Interpretation Guidance
- Built-in interpretation is deterministic and rule-based
- Ollama enhancement is optional and must never overwrite deterministic facts
- Guardrails must be applied when time confidence is low

## Copilot Output Preferences
When asked to generate implementation:
- propose small, testable units first
- prefer explicit interfaces at module boundaries
- generate scaffolding before deep implementation
- include TODO markers for Astrolog normalization assumptions
- avoid speculative astrology rules not present in the project specs

## What Not to Do
- Do not use Astrolog’s native graphics as the product renderer
- Do not write a CLI-wrapper-first architecture
- Do not blur core/domain/ui boundaries
- Do not hide uncertain birth-time assumptions
