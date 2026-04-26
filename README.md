# Asteria Starter Pack

    Asteria is a native Windows-first C++ astrology application scaffold for power astrologers.

    This starter pack gives you:
    - a complete **docs/** spec pack
    - top-level bootstrap files
    - a **CMake**-based C++ repository scaffold
    - a small fake-engine + SVG export path so you can start iterating immediately
    - placeholders and scripts for vendoring **Astrolog** source under `third_party/astrolog/`

    > **Important:** This package does **not** bundle Astrolog source code. The folder `third_party/astrolog/` contains instructions and placeholder files only. You should download the official Astrolog source package yourself and place it there.

    ## Quick Start

    1. Open this folder in **VSCode**.
    2. Read:
       - `docs/00-product-vision.md`
       - `docs/01-architecture-overview.md`
       - `docs/copilot-instructions.md`
       - `docs/prompt-sequence.md`
    3. Configure with CMake:
       ```powershell
       cmake --preset default
       cmake --build --preset debug
       ```
    4. Run the bootstrap sample:
       ```powershell
       .uild\debug\srcppsteria_app.exe
       ```
    5. Inspect the generated sample SVG next to the executable or in the working directory you launch from.

    ## Current Bootstrap Behavior

    The current scaffold builds a minimal native executable that:
    - creates a fake natal chart in memory
    - normalizes it through a fake engine
    - renders a deterministic SVG using the vector-first rendering path

    This is intentionally small so you can layer in:
    - SQLite persistence
n    - Qt UI shell
    - Astrolog embedded adapter
    - automation CLI
    - richer rendering/layout

    ## Repository Layout

    ```text
    docs/               Product and technical specs
    src/                C++ source code
    tests/              Unit/integration/golden fixtures
    tools/              Scripts and packaging helpers
    third_party/        Vendored external code (Astrolog placeholder only)
    ```

    ## Automation CLI

      The automation executable is built at `build/default/src/automation/Debug/asteria_cli.exe` and now uses the real embedded Astrolog engine for chart computation and export commands.

      Every compute, export, and timeline command also accepts `--input <path>` to load the same request fields from a JSON object file. Inline flags take precedence over the JSON file, so you can keep a reusable request on disk and override just the fields you need for a one-off run. Checked-in request samples live under `tools/examples/automation-cli/`.

    Example commands:

    ```powershell
    .\build\default\src\automation\Debug\asteria_cli.exe compute-natal `
       --primary-datetime 1990-01-01T12:00 `
       --primary-latitude 40 `
       --primary-longitude -75 `
       --primary-timezone -5

    .\build\default\src\automation\Debug\asteria_cli.exe compute-synastry `
       --primary-datetime 1990-01-01T12:00 `
       --primary-latitude 40 `
       --primary-longitude -75 `
       --primary-timezone -5 `
       --secondary-datetime 1992-05-12T08:30 `
       --secondary-latitude 34 `
       --secondary-longitude -118 `
       --secondary-timezone -8

    .\build\default\src\automation\Debug\asteria_cli.exe compute-transit `
       --primary-datetime 1990-01-01T12:00 `
       --primary-latitude 40 `
       --primary-longitude -75 `
       --primary-timezone -5 `
       --transit-datetime 2026-04-24T12:00

    .\build\default\src\automation\Debug\asteria_cli.exe generate-transit-timeline `
       --natal-datetime 1990-01-01T12:00 `
       --natal-latitude 40 `
       --natal-longitude -75 `
       --natal-timezone -5 `
       --start 2026-01-01T00:00 `
       --years 5 `
       --transit-planets Jupiter,Saturn,Uranus `
       --aspects Conjunction,Square,Opposition `
       --output .\reports\transit_timeline.md

    .\build\default\src\automation\Debug\asteria_cli.exe export-svg `
       --chart-type transit `
       --primary-datetime 1990-01-01T12:00 `
       --primary-latitude 40 `
       --primary-longitude -75 `
       --primary-timezone -5 `
       --transit-datetime 2026-04-24T12:00 `
       --output .\exports\transit_chart.svg

    .\build\default\src\automation\Debug\asteria_cli.exe resolve-location --query Denver
    ```

      Example JSON request files:

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

      .\build\default\src\automation\Debug\asteria_cli.exe compute-natal `
         --input .\tools\examples\automation-cli\compute_natal.json `
         --house-system Whole
      ```

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

    ## Suggested Next Steps

    1. Vendor Astrolog into `third_party/astrolog/`.
    2. Implement the SQLite layer under `src/data/`.
    3. Expand the renderer from the sample natal chart to a full wheel layout.
    4. Add a Qt main window shell under `src/ui/`.
    5. Follow `docs/prompt-sequence.md` in order.
