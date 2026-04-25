# ADR-001: Path B Native C++ Desktop with Astrolog Adapter

## Status
Accepted

## Context
The product goal is a polished native Windows desktop application for power astrologers with:
- strong chart computation
- beautiful chart rendering
- local-first data ownership
- installer-grade polish early
- SVG/PNG export
- optional automation
- optional local LLM enhancement

Astrolog already provides:
- a Windows GUI build
- a Windows CLI build
- official source code in C++
- chart families including synastry, composite, transit-related features
- city/time-zone atlas support
- GPL v2 licensing

These characteristics make Astrolog suitable as a computation engine foundation, but not as the primary presentation layer.

## Decision
Adopt Path B:
- native Windows desktop app in C++
- Astrolog used behind an internal engine adapter
- custom vector-first rendering layer
- SQLite local data model
- small automation/batch interface retained for power workflows and regression testing

## Why Not Path C as Primary
Path C (CLI-first wrapper architecture) was rejected as the primary architecture because:
- it over-centers process orchestration and text/file mediation
- it makes a polished desktop UX more awkward
- it ties the product too directly to CLI output conventions
- it is less elegant for rich rendering and domain normalization

## Why Keep a Small CLI/Automation Utility
A limited automation path is still valuable for:
- batch exports
- regression oracles
- fixture generation
- developer tooling
- script-based workflows

## Consequences
### Positive
- Strong control over visuals and UX
- Clear separation between calculation and presentation
- Good fit for Windows-first desktop polish
- Clean local-first architecture
- Better long-term maintainability of rendering/UI

### Negative
- Higher initial engineering cost than a thin CLI wrapper
- Need to define and maintain a stable normalization boundary from Astrolog into Asteria domain types
- Vendor integration must respect GPL v2 obligations

## Follow-Up Decisions
- Choose rendering implementation details
- Choose UI framework details
- Define vendor strategy for Astrolog source
- Define automation command surface
