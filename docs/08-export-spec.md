# Export Specification

## Status
- Implemented in the current repo: SVG and PNG export, deterministic SVG-focused tests, export naming, and persisted export metadata.
- Partially implemented: the document describes stronger profile semantics and future "presentation-clean" options that are not yet fully surfaced as end-user controls.

## Purpose
Export must produce dependable, presentation-quality chart files suitable for:
- personal archives
- client reports
- classroom handouts
- publication preparation
- social sharing

## Supported V1 Formats
- SVG
- PNG

## Shared Export Rules
- Exports are generated from the same vector scene model used for rendering
- Export metadata should record theme, chart type, date/time, and warning state
- Output should be deterministic for identical inputs

## SVG Requirements
- Primary archival/vector format
- Preserve text as text where practical
- Stable structure for future automation/testing
- Clean namespace and metadata handling

## PNG Requirements
Profiles:
- standard share
- high resolution
- print preview

## Naming Convention
Recommended:
- {person-or-pair}_{chart-type}_{yyyy-mm-dd}_{theme}.{ext}

## Warning Inclusion
When uncertainty warnings are present:
- default export includes visible warning annotation
- future option may allow “presentation-clean” export, but V1 defaults to explicit truthfulness

## Export Metadata
Persist:
- export type
- dimensions
- theme snapshot
- computed chart id
- exported timestamp
- file path

## Batch Export Compatibility
Export system must be callable from automation workflows in a later-compatible way.
