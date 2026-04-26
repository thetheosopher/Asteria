# Asteria Packaging Structure

This folder contains installer scripts and release assets for Asteria.

## Directory Layout

```
tools/packaging/
├── README.md              # This file
├── asteria.iss            # Inno Setup installer script
├── create_portable_zip.ps1 # Portable ZIP packaging script
├── release-checklist.md   # Pre-release QA checklist
├── installer-assets.md    # Asset inventory for installer
├── LICENSE.txt            # License file bundled in installer
├── NOTICES.txt            # Third-party attribution notices
└── assets/
    └── (icon files, splash images — added at release time)
```

## Building the Installer

### Prerequisites
- [Inno Setup 6](https://jrsoftware.org/isinfo.php) installed
- Asteria built in Release configuration:
  ```
  cmake --build build/default --config Release
  ```

### Steps
1. Build Asteria in Release mode
2. Open `asteria.iss` in Inno Setup Compiler
3. Click "Compile" (or run `iscc.exe asteria.iss` from command line)
4. Installer output: `tools/packaging/Output/AsteriaSetup-<version>.exe`

### Optional Portable ZIP

After a Release build, create a portable package with:

```powershell
pwsh -File tools/packaging/create_portable_zip.ps1
```

Portable output: `tools/packaging/Output/Asteria-<version>-portable.zip`

### One-liner (if iscc.exe is on PATH)
```
iscc.exe tools/packaging/asteria.iss
```

## Asset Sources
- Application icon: `tools/packaging/assets/asteria.ico`
- License: project root `LICENSE` (or `tools/packaging/LICENSE.txt`)
- Third-party notices: `tools/packaging/NOTICES.txt`
