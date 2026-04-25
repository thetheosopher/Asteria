# Automation and Batch Specification

## Purpose
Support light scriptability and batch workflows without making the whole product a CLI wrapper.

## Automation Surfaces

### Surface 1: Internal Automation Service
Used by the desktop app for repeatable export jobs and regression harnesses.

### Surface 2: External CLI Utility
Suggested name:
- asteria-cli

## V1 Target Commands
- compute natal
- compute synastry
- compute composite
- compute transit-to-natal
- export svg
- export png
- resolve location

## Input Formats
Prefer:
- JSON input file
- direct file references to canonical person/birth records
- future optional inline arguments

## Output Formats
Prefer:
- JSON summaries for compute operations
- file outputs for exports
- machine-readable exit codes

## Use Cases
- batch exports
- visual regression harness
- fixture generation
- nightly cache warming
- developer diagnostics

## Constraints
- automation uses same domain and engine layers as app
- no duplicate business logic in CLI-only code
- outputs should be deterministic where possible
