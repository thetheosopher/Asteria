Param(
  [string]$AstrologSourceFolder = ""
)

Write-Host "Asteria third-party bootstrap helper"
Write-Host "-----------------------------------"
Write-Host "This script does not download Astrolog for you."
Write-Host "Place the official Astrolog source tree under third_party/astrolog/."

if ($AstrologSourceFolder -ne "") {
  if (-not (Test-Path $AstrologSourceFolder)) {
    throw "Provided source folder does not exist: $AstrologSourceFolder"
  }
  Copy-Item -Recurse -Force $AstrologSourceFolder\* "$PSScriptRoot\..\..	hird_partystrolog"
  Write-Host "Copied Astrolog source into third_party/astrolog/."
}
