# Automation CLI Examples

These JSON files are checked-in request bodies for the `asteria_cli` `--input` workflow.

Run them from the repository root with the built executable at `build/default/src/automation/Debug/asteria_cli.exe`.

Examples:

```powershell
.\build\default\src\automation\Debug\asteria_cli.exe compute-natal --input .\tools\examples\automation-cli\compute_natal.json

.\build\default\src\automation\Debug\asteria_cli.exe compute-synastry --input .\tools\examples\automation-cli\compute_synastry.json

.\build\default\src\automation\Debug\asteria_cli.exe compute-composite --input .\tools\examples\automation-cli\compute_composite.json

.\build\default\src\automation\Debug\asteria_cli.exe compute-transit --input .\tools\examples\automation-cli\compute_transit.json

.\build\default\src\automation\Debug\asteria_cli.exe generate-transit-timeline --input .\tools\examples\automation-cli\generate_transit_timeline.json

.\build\default\src\automation\Debug\asteria_cli.exe export-svg --input .\tools\examples\automation-cli\export_svg_transit.json

.\build\default\src\automation\Debug\asteria_cli.exe export-png --input .\tools\examples\automation-cli\export_png_synastry.json

.\build\default\src\automation\Debug\asteria_cli.exe export-ai-report-pdf --input .\tools\examples\automation-cli\export_ai_report_pdf.json

.\build\default\src\automation\Debug\asteria_cli.exe resolve-location --input .\tools\examples\automation-cli\resolve_location.json
```

Inline flags still override any field from the JSON file.