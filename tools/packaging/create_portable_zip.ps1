<#
.SYNOPSIS
    Creates a portable Asteria ZIP package from a Release build.

.DESCRIPTION
    Gathers the Release executable, data files, fonts, icon, licenses
    and packages them into Asteria-<version>-portable.zip.

.EXAMPLE
    .\create_portable_zip.ps1
#>

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path "$PSScriptRoot\..\.."
$BuildRoot = Join-Path $ProjectRoot "build\default"
$Version = "1.0.0"
$OutputDir = Join-Path $PSScriptRoot "Output"
$StagingDir = Join-Path $OutputDir "Asteria-$Version-portable"
$ZipFile = Join-Path $OutputDir "Asteria-$Version-portable.zip"

# Clean staging
if (Test-Path $StagingDir) { Remove-Item $StagingDir -Recurse -Force }
if (Test-Path $ZipFile) { Remove-Item $ZipFile -Force }
New-Item -ItemType Directory -Path $StagingDir -Force | Out-Null

Write-Host "Staging portable package..."

# Main executable
$appExe = Join-Path $BuildRoot "src\app\Release\asteria_app.exe"
if (!(Test-Path $appExe)) {
    Write-Error "Release build not found at $appExe. Run: cmake --build build/default --config Release"
    exit 1
}
Copy-Item $appExe $StagingDir

# CLI executable (optional)
$cliExe = Join-Path $BuildRoot "src\automation\Release\asteria_cli.exe"
if (Test-Path $cliExe) {
    Copy-Item $cliExe $StagingDir
}

# Atlas data (next to exe)
Copy-Item (Join-Path $ProjectRoot "assets\atlasbig.as") $StagingDir

# Timezone data (next to exe)
Copy-Item (Join-Path $ProjectRoot "third_party\astrolog\timezone.as") $StagingDir

# Astrology font
Copy-Item (Join-Path $ProjectRoot "assets\fonts\Astro.ttf") $StagingDir

# Icon PNG (for About screen)
Copy-Item (Join-Path $ProjectRoot "assets\asteria_icon.png") $StagingDir

# Astrolog data directory
$astrologDataDir = Join-Path $StagingDir "astrolog_data"
New-Item -ItemType Directory -Path $astrologDataDir -Force | Out-Null
Copy-Item (Join-Path $ProjectRoot "third_party\astrolog\astrolog.as") $astrologDataDir
Copy-Item (Join-Path $ProjectRoot "third_party\astrolog\sefstars.txt") $astrologDataDir
Copy-Item (Join-Path $ProjectRoot "third_party\astrolog\seorbel.txt") $astrologDataDir

$ephemDir = Join-Path $astrologDataDir "ephem"
New-Item -ItemType Directory -Path $ephemDir -Force | Out-Null
Copy-Item (Join-Path $ProjectRoot "third_party\astrolog\ephem\*") $ephemDir -Recurse

# License and notices
Copy-Item (Join-Path $PSScriptRoot "LICENSE.txt") $StagingDir
Copy-Item (Join-Path $PSScriptRoot "NOTICES.txt") $StagingDir

# Create ZIP
Write-Host "Creating ZIP: $ZipFile"
Compress-Archive -Path "$StagingDir\*" -DestinationPath $ZipFile -CompressionLevel Optimal

# Cleanup staging
Remove-Item $StagingDir -Recurse -Force

Write-Host "Portable package created: $ZipFile"
Write-Host "Done."
