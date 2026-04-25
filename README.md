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

    ## Suggested Next Steps

    1. Vendor Astrolog into `third_party/astrolog/`.
    2. Implement the SQLite layer under `src/data/`.
    3. Expand the renderer from the sample natal chart to a full wheel layout.
    4. Add a Qt main window shell under `src/ui/`.
    5. Follow `docs/prompt-sequence.md` in order.
