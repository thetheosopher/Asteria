# Database Specification

## Status
- Implemented in the current repo: SQLite storage, startup migrations, canonical source-data persistence, computed chart caching, settings storage, and export/interpretation tables.
- Partially implemented: theme persistence and some richer backup/restore workflows described here are still lighter than the full target state.
- Planned/future-facing: manual backup commands and more defensive migration UX remain design goals.

## Database Technology
SQLite is the canonical local data store for Asteria.

## Why SQLite
- single-user local desktop use case
- no service dependency
- easy backup and portability
- sufficient performance for chart/person library workflows
- robust transactional semantics

## Database Files
Suggested structure:
- /data/asteria.db
- /data/exports/
- /data/cache/

## Schema Areas

### people
Stores Person records.

### birth_events
Stores canonical source birth data.

### locations
Stores normalized location/time-zone resolution records.

### chart_requests
Stores the chart computation requests.

### computed_charts
Stores normalized cached chart results.

### themes
Stores built-in and user-customized theme presets.

### interpretation_notes
Stores deterministic and LLM-enhanced interpretation notes.

### export_artifacts
Stores export history and file references.

### app_settings
Stores user/application settings.

## Suggested Keys
- Prefer integer primary keys for local simplicity
- Use unique stable hashes where reproducibility matters

## Migration Strategy
- Schema version tracked with pragma + migration table
- Every migration idempotent
- On startup: validate, backup if needed, migrate forward

## Caching Strategy
- Source data is canonical
- Computed charts are cached by canonical hash
- Cache invalidates when:
  - birth source changes
  - house system changes
  - location resolution changes
  - engine version changes
  - unknown-time policy changes

## File and Blob Policy
- Prefer structured tables + JSON for computed chart cache payload
- Avoid storing large binary exports directly in SQLite
- Store export metadata in DB and files on disk

## Backup and Restore
V1 should support:
- manual backup command
- export of DB copy when app is closed
- defensive backup before major schema migration

## Privacy Posture
- local-first by default
- no remote sync in V1
- user data remains on the device unless manually exported
