# Build, Release, and Installer Specification

This document describes the current repository build and packaging workflow.

## Goals
Deliver installer-grade polish early while keeping developer workflows straightforward.

## Build System
- CMake
- CMake presets
- MSVC multi-config generator via Visual Studio 2022 (`Visual Studio 17 2022`)
- Inno Setup installer packaging
- PowerShell portable ZIP packaging script

## Build Variants
- Debug
- Release
- RelWithDebInfo and MinSizeRel remain available through the Visual Studio generator when needed

## Release Artifacts
- Asteria desktop executable
- installer package (`AsteriaSetup-<version>.exe`)
- portable ZIP package (`Asteria-<version>-portable.zip`)
- optional automation CLI utility
- license/attribution documents
- bundled runtime assets (`atlasbig.as`, `timezone.as`, `astrolog_data/`, `Astro.ttf`, app icon assets)

## Installer Goals
- simple install path
- Start Menu shortcut
- Desktop shortcut optional
- uninstall entry
- app version visible
- clean upgrades over prior versions

## Recommended Early Installer
Inno Setup

## Packaging Requirements
- include vendor notices
- include GPL-compliance materials appropriate to distribution posture
- package local assets, fonts, and runtime data files used by the app at launch
- package `atlasbig.as` and `timezone.as` next to the executable
- package Astrolog engine files under `astrolog_data/`
- include libharu attribution for PDF report generation
- no pre-built database required; the SQLite database is created on first launch

## Developer Build Goals
- one-command configure
- one-command build
- repeatable local environment
- minimal manual steps after dependency bootstrap

## Release Checklist
- run unit/integration/golden tests
- verify SVG export
- verify PNG export
- verify chart text, SVG, bitmap, AI interpretation, and AI package clipboard copy
- verify AI interpretation PDF report export, report templates, vector chart mode, and archival mode
- verify `asteria_cli export-ai-report-pdf`
- verify migration path
- verify installer install/uninstall
- verify licensing bundle
