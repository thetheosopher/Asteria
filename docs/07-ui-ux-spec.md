# UI / UX Specification

## UX Goals
Asteria must feel:
- focused
- desktop-native
- fast for experts
- visually calm
- trustworthy when data is uncertain

## Primary Navigation Model
Recommended main areas:
- Library
- Chart Workspace
- Compare Workspace
- Location Resolver
- Settings

## Screen 1: Library
Purpose:
- manage people and birth records
- search, tag, organize, and reopen prior work

Core elements:
- search box
- tag filter
- people list
- record summary panel
- recent charts
- create/edit/delete actions

## Screen 2: Chart Workspace
Purpose:
- display natal, composite, and transit-to-natal charts
- show metadata, warnings, and interpretation

Core elements:
- chart canvas
- chart-type switcher
- theme selector
- metadata panel
- uncertainty warning area
- export actions
- interpretation panel toggle

## Screen 3: Compare Workspace
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

## Screen 4: Location Resolver
Purpose:
- resolve city input into coordinates/time-zone data
- show provenance and confidence

Core elements:
- search field
- candidate results list
- map preview/pin
- confidence/provenance panel
- accept/override controls

## Screen 5: Settings
Purpose:
- product-level defaults and advanced options

Core elements:
- default theme
- default house system
- unknown-time policy
- export defaults
- interpretation settings
- Ollama integration settings
- developer/debug options

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
- previewable
- non-destructive
- export-consistent

## Non-Goals in V1
- infinitely customizable layouts
- dashboard clutter
- social or cloud UX
- web-style wizard-heavy flows
