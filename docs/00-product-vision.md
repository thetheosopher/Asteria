# Asteria Product Vision

## Status
- Implemented in the current repo: native Windows desktop app, local-first data model, natal/synastry/composite/transit-to-natal charts, SVG/PNG export, built-in interpretation, optional Ollama enhancement.
- Still aspirational in this document: qualitative claims like "beautiful" and "publication-grade" remain product goals, not hard acceptance proof.

## Purpose
Asteria is a native Windows desktop application for power astrologers who want beautiful, accurate, printable astrology charts with a fast local-first workflow and a carefully curated feature set.

## Product Thesis
Most astrology software either excels at calculation but not presentation, or presentation but not workflow depth. Asteria will pair a mature astrology computation engine with a renderer and user experience built specifically for:
- beautiful natal charts
- elegant synastry comparison charts
- composite charts
- transit-to-natal views
- local chart/person management
- publication-grade SVG and PNG exports

## Primary Users
The primary users are power astrologers. They care about:
- accuracy and consistency
- repeatable chart generation
- trustworthy location/time handling
- printable output quality
- batch-friendly and archive-friendly workflows
- local control over data and interpretation

## V1 Scope
### Must-have chart types
- Natal
- Synastry / comparison bi-wheel
- Composite
- Transit-to-natal

### Must-have outputs
- On-screen chart display
- SVG export
- PNG export

### Must-have data workflows
- Single-user local people/chart database
- Canonical source-data storage
- Location/time-zone automation
- Provenance and confidence tracking for uncertain birth data
- Unknown-time support with warnings and no-house mode

### Must-have interpretation
- Basic built-in interpretation text
- Optional local LLM enhancement via Ollama

## Product Character
Asteria will support two visual modes:
1. Printable textbook style
2. Modern minimal / luxury style

The application will be opinionated rather than infinitely configurable. The default house system for V1 is Placidus, with advanced astrology settings deferred behind an advanced workflow.

## Non-Goals for V1
- No cloud sync
- No multi-user features
- No heavy support for asteroids, fixed stars, or expansive object catalogs in the main UI
- No plugin marketplace
- No web-first or browser-first implementation
- No dependence on online services for core chart generation

## Success Criteria
Asteria succeeds in V1 if a power astrologer can:
- create and manage a local library of people and birth records
- generate accurate natal, synastry, composite, and transit-to-natal charts
- produce presentation-quality SVG or PNG output
- trust the treatment of uncertain birth times
- optionally generate grounded narrative interpretation locally
