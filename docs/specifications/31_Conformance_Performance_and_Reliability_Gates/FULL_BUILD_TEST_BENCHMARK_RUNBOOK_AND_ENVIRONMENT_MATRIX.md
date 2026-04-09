Status: reconstructed_required_with_current_substrate

# Full Build Test Benchmark Runbook and Environment Matrix

## Purpose

This document defines the canonical operator runbook for a full clean, build, test, and benchmark cycle and the environment matrix that must be captured for that cycle.

## Canonical Rule

A full-cycle runbook is valid only when the environment matrix is captured alongside the run. The runbook and the environment matrix are one certification object, not separate optional notes.

## Environment Matrix

The matrix shall preserve:

- host or container profile
- build toolchain profile
- runtime dependency profile
- benchmark harness profile
- engine configuration profile
- data-volume profile
- concurrency profile
- accelerator profile if applicable

## Runbook Phases

The canonical phases are:

1. prerequisite audit
2. clean phase
3. build phase
4. repo-local test phase
5. compatibility or conformance phase where selected
6. benchmark harness phase where selected
7. artifact collection and baseline comparison

## Phase Output Rule

Each phase shall preserve:

- phase identity
- phase start and end markers
- output root or artifact path
- failure or success classification

## Selection Rule

The runbook may omit optional phases only when the omission is recorded explicitly in the environment matrix and final artifact record.

## Non-Guarantees

This file does not require every operator run to include every phase. It requires a canonical runbook vocabulary and environment matrix for those phases that are run.
