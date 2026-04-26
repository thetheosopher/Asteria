# UI / UX Specification

This document reflects the current V1 desktop workflow implemented in the Dear ImGui application.

## UX Goals
Asteria must feel:
- focused
- desktop-native
- fast for experts
- visually calm
- trustworthy when data is uncertain

## Primary Navigation Model
Current main panels:
- Library
- Chart Workspace
- Transit Timeline
- Compare Workspace
- AI Interpretation

Settings are surfaced from the View menu rather than a standalone settings screen.
Location resolution is embedded in library and record-editing workflows rather than a dedicated resolver screen.

## Panel 1: Library
Purpose:
- manage people and birth records
- search, organize, and reopen prior work
- resolve and store location data during record editing

Core elements:
- search box
- people list
- birth event list/editor
- record summary panel
- create/edit/delete actions
- inline location lookup and resolution helpers
- import/export/merge actions exposed from the File menu

## Panel 2: Chart Workspace
Purpose:
- display natal, composite, and transit-to-natal charts
- show metadata, warnings, and interpretation

Core elements:
- chart canvas
- chart-type switcher
- metadata panel
- uncertainty warning area
- export actions
- built-in interpretation output
- send/share actions to the AI Interpretation panel when enabled

## Panel 3: Transit Timeline
Purpose:
- generate longitudinal transit reports for the selected subject
- review, save, and forward markdown output for follow-up interpretation

Core elements:
- subject selection via current library selection
- start date / years configuration
- transit planet and aspect filters
- markdown report output
- save markdown action
- ask AI follow-up action

## Panel 4: Compare Workspace
Purpose:
- display synastry and composite workflows

Core elements:
- person A selector
- person B selector
- quick mode switch:
  - synastry
  - composite
- chart canvas
- supporting metadata
- export actions

## Panel 5: AI Interpretation
Purpose:
- generate local-AI-assisted follow-up interpretations and summaries
- display status, errors, and exported markdown responses

Core elements:
- prompt generation from chart/transit context
- markdown response viewer
- save/export actions
- status and error display

## Settings Surface
Purpose:
- product-level defaults and advanced options

Current settings are available from the View menu and include:
- default theme
- default house system
- unknown-time policy
- export defaults
- interpretation settings
- Ollama integration settings
- developer/debug options

The legacy Settings panel currently redirects users to the View menu rather than acting as a primary surface.

## Record Editing UX
Birth record editing must support:
- exact time
- approximate time
- unknown time
- noon default with visible acknowledgment
- no-houses mode
- source/provenance notes

## Warning UX Rules
Warnings must be visible, not buried:
- unknown time
- noon default
- houses suppressed
- low-confidence location resolution
- manual timezone override

## Power-User UX Expectations
- keyboard navigable
- searchable everywhere sensible
- recent charts easy to reopen
- repeat export actions fast
- deterministic behavior over “magic”

## Theme UX
Theme changes should be:
- fast
- immediately applied
- non-destructive
- export-consistent

## Non-Goals in V1
- standalone map/location-resolver screen
- infinitely customizable layouts
- dashboard clutter
- social or cloud UX
- web-style wizard-heavy flows
