Status: reconstructed_required_with_current_substrate

# Optimizer Candidate Bundle Trace and Winner Explanation Model

## Purpose

This document defines the canonical trace model for optimizer candidate enumeration, comparison, and winner selection.

## Canonical Rule

Every plan winner shall be explainable from a deterministic candidate bundle. The optimizer may use heuristics and calibration, but it shall preserve enough structured evidence to explain why the winner was chosen and why the losing candidates lost.

## Candidate Bundle

For each planned statement, the optimizer shall build a candidate bundle containing:

- statement or access-path identity
- legal candidate list
- family-native metrics packet for each candidate
- normalized comparison frame for each candidate
- accelerator admission state where applicable
- disqualifier list where candidates were refused

## Winner Selection Trace

The canonical winner trace shall preserve:

- final winning candidate
- ordered ranking of materially comparable candidates
- normalized cost components
- explicit winning credits
- explicit losing penalties
- any fail-closed refusal applied before ranking

## Required Explanation Fields

The optimizer shall be able to expose:

- why a family entered the candidate set
- why a family was absent from the candidate set
- why a family lost to another candidate
- whether stale, degraded, or partial metrics changed the outcome
- whether resident-memory or accelerator state changed the outcome

## Family-Parity Rule

The trace model shall not privilege one family by omitting structured evidence for another family. If a family is implemented and legal for the query shape, its admission or refusal must be visible in the candidate bundle.

## Refusal Classes

Candidate refusal shall be classified explicitly, including:

- semantic mismatch
- unsupported operator shape
- stale or unusable metrics
- accelerator refusal
- memory-pressure refusal
- ordering or residual-filter mismatch

## Diagnostics Rule

The same structured trace shall be usable by:

- plan explanation surfaces
- certification gates
- regression comparison tools
- operator diagnostics

## Non-Guarantees

This file does not require every internal planner detail to be exposed verbatim to end users. It requires a deterministic engine-owned explanation record.
