# Asteria V1 Release Checklist

Complete all items before publishing a release.

## Build Verification

- [ ] Clean build succeeds: `cmake --build build/default --config Release`
- [ ] No compiler warnings in Asteria code (vendor warnings suppressed)
- [ ] Debug build also succeeds (for development continuity)

## Testing

- [ ] All unit tests pass: `ctest --build-config Release`
- [ ] All integration tests pass (database, Astrolog engine)
- [ ] Golden SVG output matches expected (determinism check)
- [ ] Manual smoke test: launch app, create a person, compute natal chart

## Feature Verification

### Natal Charts
- [ ] Compute natal chart with known birth time
- [ ] Compute natal chart with unknown time (noon default)
- [ ] Compute natal chart with unknown time (no houses mode)
- [ ] Warning banner appears for uncertain-time charts
- [ ] All standard planets displayed (Sun through Pluto + nodes)
- [ ] House cusps displayed for Placidus, Koch, and Whole sign systems

### Comparison Charts
- [ ] Synastry chart computes and renders
- [ ] Composite chart computes and renders
- [ ] Warning propagates when one partner has uncertain time

### Transit-to-Natal
- [ ] Transit overlay computes and renders
- [ ] Transit planets visually distinct from natal planets

### Interpretation
- [ ] Built-in interpretation generates for all chart types
- [ ] Uncertainty guardrails appear when flags are present
- [ ] Guardrails absent for certain-time charts

### Export
- [ ] SVG export produces valid SVG file
- [ ] PNG export produces valid image file
- [ ] Exported files contain chart content (not empty/blank)

### CLI Automation
- [ ] `asteria-cli compute natal` works
- [ ] `asteria-cli compute synastry` works
- [ ] `asteria-cli compute composite` works
- [ ] `asteria-cli compute transit-to-natal` works
- [ ] `asteria-cli export svg` works
- [ ] `asteria-cli export png` works
- [ ] `asteria-cli resolve location` works

### Database
- [ ] Fresh database creation works (first launch)
- [ ] Migration runner is idempotent
- [ ] Person CRUD operations work
- [ ] Birth event insert/update works with all time accuracy modes
- [ ] Chart request and computed chart persist correctly

## Installer

- [ ] Inno Setup script compiles without errors
- [ ] Installer runs on clean Windows 10/11 machine
- [ ] Start Menu shortcut created
- [ ] Desktop shortcut offered (optional)
- [ ] Uninstall entry appears in Add/Remove Programs
- [ ] Uninstall cleanly removes app files
- [ ] Upgrade over previous version works (if applicable)
- [ ] Astrolog data files installed to correct location
- [ ] Application launches after install

## Legal & Licensing

- [ ] LICENSE.txt included in installer
- [ ] NOTICES.txt includes all third-party attributions
- [ ] Astrolog GPL compliance verified
- [ ] Swiss Ephemeris license terms addressed
- [ ] SQLite public domain status noted

## Version & Metadata

- [ ] Version number updated in CMakeLists.txt
- [ ] Version displayed in application window title
- [ ] Installer version matches application version
- [ ] Git tag created for release
