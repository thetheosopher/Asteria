# Roadmap

## Status
- Completed in the current repo: Milestones 0 through 4 are substantially present in code, including app shell, library/data flows, comparison workflows, transit reporting, interpretation, and CLI support.
- In progress: Milestone 5 is partially complete with installer scripts, portable packaging, release docs, and published release flow, but long-term installer hardening and upgrade validation remain ongoing maintenance work.
- Planned: V1.1 remains a forward-looking list rather than a committed implementation contract.

## Milestone 0: Repository and Architecture Bootstrap
- Status: Completed
- repo initialized
- CMake and dependency bootstrap
- app shell launches
- SQLite opens
- docs/spec pack committed

## Milestone 1: Natal End-to-End
- Status: Completed
- one birth record flow
- embedded engine spike
- normalized natal chart
- on-screen natal rendering
- SVG export

## Milestone 2: Library and Data Integrity
- Status: Completed (with location UX still simpler than the full long-term design)
- people library
- birth record editing
- location resolution flow
- provenance/confidence model
- unknown-time handling

## Milestone 3: Comparison Workflows
- Status: Completed
- synastry bi-wheel
- composite chart
- theme switching
- PNG export

## Milestone 4: Transit-to-Natal and Interpretation
- Status: Completed
- transit-to-natal workflow
- built-in interpretation
- warning-aware narrative handling
- basic automation CLI

## Milestone 5: Installer Polish
- Status: In Progress
- packaging
- release checklist
- upgrade/install flow
- regression harness hardening

## V1.1 Candidates
- richer Ollama workflows and prompt controls
- richer theme editing
- advanced chart settings panel
- print layout templates
- broader automation surface
