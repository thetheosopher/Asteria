# Export Specification

## Status
- Implemented in the current repo: SVG and PNG export, AI report PDF export, PDF report templates, optional vector PDF chart drawing, deterministic SVG/PDF-focused tests, export naming, rich clipboard copy, and persisted export metadata.
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
- PDF reports for AI interpretations

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

## PDF Report Requirements
- PDF report generation is available after AI interpretation text exists.
- Reports include the chart scene used for the AI interpretation and the generated interpretation text.
- Reports can include both the built-in deterministic interpretation and the AI-generated interpretation.
- Supported templates are Client Report, Study Notes, Compact One-Page, and Archive Copy.
- Reports embed a high-quality rasterized chart image through libharu by default and can optionally draw the chart directly with PDF vector primitives.
- Report text is derived from Markdown into a paginated layout with headings, bullets, numbered lists, rules, page headers/footers, embedded Windows UI fonts when available, and plain base-font fallback.
- Report metadata should capture source label, model label, template, chart backend, archival mode, chart hash, engine, house system, zodiac mode, theme snapshot, output path, and exported timestamp.
- Archival mode means metadata-rich, uncompressed report output with stable page chrome; it is not a formal PDF/A compliance claim.

## Clipboard Requirements
- Chart text copy should provide plain text suitable for prompts and notes.
- Chart SVG copy should provide registered SVG clipboard formats with plain text fallback.
- Chart image copy should provide a Windows CF_DIB bitmap with plain text fallback.
- AI interpretation copy should provide Markdown, HTML, and plain text clipboard formats.
- AI package copy should include metadata, built-in interpretation when enabled, AI text, and chart facts as Markdown/HTML/plain text.

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
- report metadata when the export is a PDF report

## Batch Export Compatibility
Export system must be callable from automation workflows. The CLI exposes `export-ai-report-pdf` with chart input flags, interpretation text/file input, report template selection, vector chart output, and archival mode.
