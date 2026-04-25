# Domain Model

## Design Goals
The domain model exists to decouple:
- user data management
- chart requests
- normalized astrology computation output
- rendering
- interpretation
- export history

No domain type may expose Astrolog-specific source structs or switch syntax.

## Core Entities

### Person
Represents an individual or named subject.

Fields:
- person_id
- full_name
- display_name
- gender
- tags[]
- notes
- created_at
- updated_at
- archived_at nullable

### BirthEvent
Represents canonical source birth data.

Fields:
- birth_event_id
- person_id
- birth_date
- birth_time nullable
- time_accuracy enum: exact | approximate | unknown
- noon_default_applied boolean
- houses_enabled boolean
- source_description
- confidence_score
- city_input
- location_id nullable
- timezone_id nullable
- dst_mode
- created_at
- updated_at

### LocationResolution
Represents a resolved place and provenance trail.

Fields:
- location_id
- query_text
- display_name
- country
- region
- latitude
- longitude
- timezone_name
- timezone_offset_rule_ref
- resolution_method enum: atlas | manual | map_pin | imported
- confidence_score
- provenance_note
- map_pin_precision_meters nullable
- created_at
- updated_at

### ChartRequest
Represents a request to compute a chart.

Fields:
- chart_request_id
- primary_birth_event_id
- secondary_birth_event_id nullable
- chart_type enum: natal | synastry | composite | transit_to_natal
- request_time_context nullable
- default_house_system
- zodiac_mode
- unknown_time_policy
- include_houses boolean
- theme_id nullable
- created_at

### ComputedChart
Represents normalized output from the engine adapter.

Fields:
- computed_chart_id
- chart_request_id
- engine_version
- engine_method
- canonical_hash
- house_system
- zodiac_mode
- uncertainty_flags[]
- chart_metadata_json
- cached_json_blob
- created_at

### ChartTheme
Represents chart appearance presets.

Fields:
- theme_id
- theme_name
- style_family enum: textbook | luxury
- glyph_set
- line_weight_profile
- aspect_palette
- typography_profile
- background_mode
- created_at
- updated_at

### InterpretationNote
Represents attached interpretation text.

Fields:
- interpretation_note_id
- computed_chart_id
- source_type enum: built_in | ollama
- prompt_version nullable
- certainty_guardrails_applied boolean
- body_markdown
- created_at

### ExportArtifact
Represents export history and file metadata.

Fields:
- export_artifact_id
- computed_chart_id
- export_type enum: svg | png
- file_path
- width_px nullable
- height_px nullable
- dpi nullable
- theme_snapshot_json
- exported_at

## Supporting Value Objects

### PlanetPosition
- object_id
- longitude
- latitude
- speed
- sign
- house nullable
- retrograde boolean

### HouseCusp
- house_number
- longitude

### Aspect
- object_a
- object_b
- aspect_type
- orb
- applying_or_separating nullable

### WarningFlag
Examples:
- unknown_birth_time
- noon_default_used
- houses_suppressed
- location_low_confidence
- timezone_manually_overridden

## Domain Rules
- Source data is canonical.
- ComputedChart is always derivable from source BirthEvent and ChartRequest.
- Unknown time may disable houses entirely.
- Noon default must always generate a visible warning.
- Interpretation must inherit certainty flags from ComputedChart.
