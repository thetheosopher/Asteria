# Testing Strategy

This document describes the intended V1 testing envelope. The checked-in automated suite is currently a subset of this plan; treat the items below as strategy targets rather than a claim that every category is already fully implemented.

## Goals
Ensure:
- chart computation correctness
- stable normalization
- deterministic exports
- trustworthy uncertain-time handling
- reliable database migrations
- non-regressing UI-critical workflows

## Test Categories

### Unit Tests
Cover:
- domain object validation
- hash/canonicalization logic
- warning-flag derivation
- theme parsing
- export naming

### Integration Tests
Cover:
- Astrolog adapter normalization
- SQLite repositories
- location resolution flows
- interpretation guardrail behavior
- automation commands

### Golden / Snapshot Tests
Cover:
- SVG output for fixed fixtures
- PNG output hash/tolerance checks
- chart layout stability for representative charts

### Fixture Sets
Include:
- exact-time natal
- approximate-time natal
- unknown-time natal
- synastry pair
- composite pair
- transit-to-natal case
- location ambiguity case

## Regression Oracle Strategy
During engine bring-up, CLI-assisted comparison may be used as a regression oracle for embedded integration behavior.

## Determinism Rules
- same request + same engine version + same theme = same SVG structure where practical
- warning treatment must be testable
- uncertainty flags must be stable and explicit

## Manual QA Focus Areas
- label collisions
- warning visibility
- export correctness
- location resolution confidence UX
- installer behavior
