# Location and Time-Zone Specification

## Purpose
Provide trustworthy location and time handling with explicit provenance and confidence.

## V1 Requirements
- city lookup and auto-resolve
- map-picking support
- timezone and DST handling
- provenance and confidence tracking
- support for uncertain or partial birth-time scenarios

## Resolution Workflow
1. User enters city text
2. System returns candidate matches
3. User selects candidate or refines via map
4. System stores normalized location and time-zone data
5. Provenance and confidence are recorded

## Resolution Methods
- atlas lookup
- manual coordinates
- map pin refinement
- imported record

## Provenance Fields
- original query text
- chosen result display name
- resolution method
- confidence score
- user notes
- manual override flags

## Unknown-Time Support
Two modes:
1. Noon default with explicit warning
2. Unknown time / no houses

## UI Expectations
- clearly show confidence
- clearly show manual overrides
- allow user refinement without losing original query history

## Error Cases
- ambiguous city
- timezone conflict
- missing DST confidence
- low precision map pin

## Non-Goals for V1
- geocoding service dependency
- cloud-only location resolution
