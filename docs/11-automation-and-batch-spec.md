# Automation and Batch Specification

## Status
- Implemented in the current repo: the `asteria_cli` utility, JSON `--input` workflow, transit timeline generation, location resolution, and direct export commands.
- Partially implemented: this document mixes current command surface with broader automation goals; use the concrete command names below as authoritative for the present repo.
- Planned/future-facing: direct record-file references, nightly cache warming, and broader regression harnessing remain extensions rather than core product features.

## Purpose
Support light scriptability and batch workflows without making the whole product a CLI wrapper.

## Automation Surfaces

### Surface 1: Internal Automation Service
Used by the desktop app for repeatable export jobs and regression harnesses.

### Surface 2: External CLI Utility
Current executable:
- asteria_cli

## V1 Target Commands
- compute-natal
- compute-synastry
- compute-composite
- compute-transit
- generate-transit-timeline
- export-svg
- export-png
- resolve-location

## Input Formats
Prefer:
- inline resolved-input flags for automation and CI use now
- JSON input files via `--input <path>` for reusable batch requests
- direct file references to canonical person/birth records as a future extension

Rules:
- JSON request files are command-specific root objects.
- Inline flags override JSON file values.
- Birth inputs are nested under `primary`, `secondary`, `transit`, or `natal` objects.
- Birth object keys accept `datetime`, `latitude`, `longitude`, `timezone`, and `dst`.
- Checked-in sample request files live under `tools/examples/automation-cli/`.

## Output Formats
Prefer:
- JSON summaries for compute operations
- file outputs for exports
- machine-readable exit codes

## Use Cases
- batch exports
- visual regression harness
- fixture generation
- nightly cache warming
- developer diagnostics

## Constraints
- automation uses same domain and engine layers as app
- no duplicate business logic in CLI-only code
- outputs should be deterministic where possible

## Current CLI Examples

Compute a natal chart as JSON:

```powershell
.\build\default\src\automation\Debug\asteria_cli.exe compute-natal `
	--primary-datetime 1990-01-01T12:00 `
	--primary-latitude 40 `
	--primary-longitude -75 `
	--primary-timezone -5
```

Compute synastry with inline primary and secondary inputs:

```powershell
.\build\default\src\automation\Debug\asteria_cli.exe compute-synastry `
	--primary-datetime 1990-01-01T12:00 `
	--primary-latitude 40 `
	--primary-longitude -75 `
	--primary-timezone -5 `
	--secondary-datetime 1992-05-12T08:30 `
	--secondary-latitude 34 `
	--secondary-longitude -118 `
	--secondary-timezone -8
```

Compute a transit-to-natal chart with a separate transit moment:

```powershell
.\build\default\src\automation\Debug\asteria_cli.exe compute-transit `
	--primary-datetime 1990-01-01T12:00 `
	--primary-latitude 40 `
	--primary-longitude -75 `
	--primary-timezone -5 `
	--transit-datetime 2026-04-24T12:00
```

Generate the markdown transit timeline report and save it:

```powershell
.\build\default\src\automation\Debug\asteria_cli.exe generate-transit-timeline `
	--natal-datetime 1990-01-01T12:00 `
	--natal-latitude 40 `
	--natal-longitude -75 `
	--natal-timezone -5 `
	--start 2026-01-01T00:00 `
	--years 5 `
	--transit-planets Jupiter,Saturn,Uranus `
	--aspects Conjunction,Square,Opposition `
	--orb Conjunction=1.0 `
	--orb Square=0.8 `
	--output .\reports\transit_timeline.md
```

Export a transit chart directly to SVG:

```powershell
.\build\default\src\automation\Debug\asteria_cli.exe export-svg `
	--chart-type transit `
	--primary-datetime 1990-01-01T12:00 `
	--primary-latitude 40 `
	--primary-longitude -75 `
	--primary-timezone -5 `
	--transit-datetime 2026-04-24T12:00 `
	--output .\exports\transit_chart.svg
```

Export a synastry chart to PNG:

```powershell
.\build\default\src\automation\Debug\asteria_cli.exe export-png `
	--chart-type synastry `
	--primary-datetime 1990-01-01T12:00 `
	--primary-latitude 40 `
	--primary-longitude -75 `
	--primary-timezone -5 `
	--secondary-datetime 1992-05-12T08:30 `
	--secondary-latitude 34 `
	--secondary-longitude -118 `
	--secondary-timezone -8 `
	--width 1400 `
	--height 1400 `
	--dpi 150 `
	--output .\exports\synastry.png
```

Resolve locations from the bundled atlas data:

```powershell
.\build\default\src\automation\Debug\asteria_cli.exe resolve-location --query Denver
```

Load the same compute request from JSON instead of long inline flags:

```json
{
	"primary": {
		"datetime": "1990-01-01T12:00",
		"latitude": 40.0,
		"longitude": -75.0,
		"timezone": -5.0,
		"dst": 0.0
	},
	"houseSystem": "Placidus",
	"zodiacMode": "Tropical"
}
```

```powershell
.\build\default\src\automation\Debug\asteria_cli.exe compute-natal --input .\tools\examples\automation-cli\compute_natal.json
```

Override one field from the JSON file on the command line:

```powershell
.\build\default\src\automation\Debug\asteria_cli.exe compute-natal `
	--input .\tools\examples\automation-cli\compute_natal.json `
	--house-system Whole
```

Timeline requests can carry rules and output paths directly in JSON:

```json
{
	"subjectName": "Casey Example",
	"natal": {
		"datetime": "1990-01-01T12:00",
		"latitude": 40.0,
		"longitude": -75.0,
		"timezone": -5.0,
		"dst": 0.0
	},
	"start": "2026-01-01T00:00",
	"years": 5,
	"transitPlanets": ["Jupiter", "Saturn", "Uranus"],
	"aspects": ["Conjunction", "Square", "Opposition"],
	"orbs": {
		"Conjunction": 1.0,
		"Square": 0.8,
		"Opposition": 1.0
	},
	"output": ".\\reports\\transit_timeline.md"
}
```

```powershell
.\build\default\src\automation\Debug\asteria_cli.exe generate-transit-timeline --input .\tools\examples\automation-cli\generate_transit_timeline.json
```
