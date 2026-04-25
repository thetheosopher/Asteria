# Interpretation Specification

## Purpose
Provide useful textual interpretation without compromising:
- chart-data determinism
- uncertainty honesty
- local-first operation

## Interpretation Layers

### Layer 1: Built-In Interpretation
Deterministic, local, rule-based interpretation for:
- natal
- synastry
- composite
- transit-to-natal

Characteristics:
- concise
- structured
- warning-aware
- not overly mystical by default
- grounded in chart facts available in ComputedChart

### Layer 2: Ollama Enhancement
Optional local LLM enhancement that:
- elaborates on built-in facts
- remains local-first
- clearly labels generated commentary
- includes uncertainty and provenance guardrails

## Interpretation Inputs
Interpretation pipeline consumes:
- normalized chart facts
- warning flags
- chart type
- theme/style preference for tone
- optional user prompt context

## Unknown-Time Guardrails
If birth time is approximate or unknown:
- interpretation must state this
- house-based claims must be suppressed or qualified when houses are unavailable
- noon-default assumptions must be explicit

## Storage Model
Interpretations are stored as InterpretationNote records with:
- source_type
- body_markdown
- guardrail flags
- generation timestamp
- prompt version when applicable

## UX Expectations
- interpretation panel should be collapsible
- built-in interpretation available offline
- Ollama enhancement optional, not required
- generated text should never overwrite deterministic notes

## Non-Goals for V1
- massive interpretive encyclopedias
- remote LLM dependency
- arbitrary prompt playground as a core feature
