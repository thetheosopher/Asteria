# Build, Release, and Installer Specification

## Goals
Deliver installer-grade polish early while keeping developer workflows straightforward.

## Build System
- CMake
- CMake presets
- Ninja
- MSVC toolchain

## Build Variants
- Debug
- RelWithDebInfo
- Release

## Release Artifacts
- Asteria desktop executable
- required runtime dependencies
- installer package
- optional automation CLI utility
- license/attribution documents
- sample themes
- app icon assets

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
- package local assets and themes
- package default database initialization assets if needed

## Developer Build Goals
- one-command configure
- one-command build
- repeatable local environment
- minimal manual steps after dependency bootstrap

## Release Checklist
- run unit/integration/golden tests
- verify SVG export
- verify PNG export
- verify migration path
- verify installer install/uninstall
- verify licensing bundle
