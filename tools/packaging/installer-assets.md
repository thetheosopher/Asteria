# Installer Asset Inventory

All assets required for the Asteria installer package.

## Core Application

| Asset | Source Path | Required |
|-------|-----------|----------|
| asteria_app.exe | `build/default/src/app/Release/asteria_app.exe` | Yes |
| asteria_cli.exe | `build/default/src/automation/Release/asteria_cli.exe` | Optional |

## Runtime Dependencies

| Asset | Source Path | Required |
|-------|-----------|----------|
| VCRUNTIME140.dll | VS Redist / system | Yes (or bundled redist) |
| MSVCP140.dll | VS Redist / system | Yes (or bundled redist) |

> **Note**: Prefer installing the VC++ Redistributable as a prerequisite
> rather than bundling individual DLLs.

## Astrolog Data Files

| Asset | Source Path | Required |
|-------|-----------|----------|
| astrolog.as | `third_party/astrolog/astrolog.as` | Yes |
| atlas.as | `third_party/astrolog/atlas.as` | Yes |
| timezone.as | `third_party/astrolog/timezone.as` | Yes |
| sefstars.txt | `third_party/astrolog/sefstars.txt` | Yes |
| seorbel.txt | `third_party/astrolog/seorbel.txt` | Yes |
| ephem/*.se1 | `third_party/astrolog/ephem/` | Yes |

## License & Legal

| Asset | Source Path | Required |
|-------|-----------|----------|
| LICENSE.txt | `tools/packaging/LICENSE.txt` | Yes |
| NOTICES.txt | `tools/packaging/NOTICES.txt` | Yes |

## Branding & Icons

| Asset | Source Path | Required |
|-------|-----------|----------|
| asteria.ico | `tools/packaging/assets/asteria.ico` | Yes |
| asteria-banner.bmp | `tools/packaging/assets/asteria-banner.bmp` | Optional |
| asteria-small.bmp | `tools/packaging/assets/asteria-small.bmp` | Optional |

> Inno Setup wizard images: banner = 164x314, small = 55x55 (BMP format).

## Database Initialization

| Asset | Source Path | Required |
|-------|-----------|----------|
| (none — created at first run) | N/A | N/A |

> The SQLite database is created automatically on first launch via the
> migration runner. No pre-built database is needed in the installer.

## Theme Defaults

| Asset | Source Path | Required |
|-------|-----------|----------|
| (built into binary) | N/A | N/A |

> Theme presets (Textbook Light, Textbook Monochrome, Luxury Light,
> Luxury Dark) are compiled into the application binary.
