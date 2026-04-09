# Benchmark Results and Machine Capture Contract

Status: current_authority

## Purpose

This file defines the minimum artifact set required for any benchmark run to be considered comparable and auditable.

## Required artifact classes

A benchmark or matrix run must preserve:
- raw suite results
- consolidated result tables or CSV outputs
- engine profile and version identity
- scenario and dataset identity
- run timestamp
- host or VM machine capture
- quarantine marker when results are incomplete or suspect

## Benchmark-project evidence roots

The external benchmark project currently maintains:
- `results/` for preserved run outputs
- quarantined result trees for suspect or incomplete runs
- `system-info/` for machine collectors, schemas, and submission helpers
- comparison and consolidated reporting under project docs and scripts

## Full clean build or test relationship

A full clean build, test, and benchmark program must keep separate evidence roots for:
- repo-local correctness results under `ScratchBird/tests/results/`
- compatibility results under `ScratchBird/tests/compatibility/results/`
- external benchmark matrix outputs under `ScratchBird-Benchmarks/results/`

These must not be merged into one opaque artifact bucket.

## Fail-closed comparison rule

Cross-machine or cross-engine benchmark comparison is invalid unless the machine capture is present and the compared runs state their configuration and scenario identity.
