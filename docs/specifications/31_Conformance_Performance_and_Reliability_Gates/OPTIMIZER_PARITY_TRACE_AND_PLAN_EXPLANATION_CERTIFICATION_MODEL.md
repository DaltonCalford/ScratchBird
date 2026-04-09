Status: reconstructed_required_with_current_substrate

# Optimizer Parity Trace and Plan Explanation Certification Model

## Purpose

This document defines the certification evidence required to prove that optimizer candidate parity and plan explanation surfaces are working correctly.

## Required Proof Classes

Certification shall preserve proof for:

- candidate-set completeness
- family-native metrics ingestion
- normalized cost comparison
- winner explanation trace
- refusal explanation trace

## Required Corpus

The certification corpus shall include workloads where:

- different index families win
- resident-memory state changes the winner
- accelerator admission changes the winner
- stale or degraded metrics change the ranking
- a candidate is refused for a documented reason

## Evidence Record

Each certification record shall preserve:

- query or statement identity
- candidate bundle
- normalized metrics frame
- chosen winner
- explanation trace
- result correctness evidence

## Failure Criteria

Certification fails when:

- a legal implemented family is missing from the candidate set without a refusal record
- a plan winner cannot be explained from the structured trace
- explanation output contradicts the internal comparison record
- accelerator refusal removes the base family rather than only the accelerator variant

## Non-Guarantees

This file does not require one public EXPLAIN syntax. It requires deterministic traceable evidence for certification and operator explanation surfaces.
