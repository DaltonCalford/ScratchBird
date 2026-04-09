Status: reconstructed_required

# Vector ANN Primary Class Winner and Fallback Obligation Model

## Purpose

This document defines the optimizer obligations specifically for vector and ANN index families.

## Canonical Rule

Vector and ANN families are primary optimizer classes. They must be able to win when semantically appropriate, and if acceleration or residency is unavailable they must degrade through explicit fallback paths rather than disappear silently.

## Winner Obligation

The optimizer shall produce winning vector or ANN plans when:

- the query semantics call for vector or ANN search
- the family is implemented and admitted
- current metrics and residency state justify the family

## Fallback Obligation

If a preferred accelerator-backed or resident variant is unavailable, the optimizer shall fall back explicitly to:

- a non-accelerated vector or ANN family variant if admitted
- another semantically legal family with documented penalties
- an explicit refusal when no legal variant remains

## Refusal Explanation Rule

Absence of a vector or ANN family from the candidate set shall preserve:

- semantic mismatch or unsupported query shape
- residency refusal
- accelerator refusal
- unusable metrics
- degraded or quarantined family state

## Non-Guarantees

This file does not require vector or ANN to win every workload. It requires they participate as primary classes and fail with explicit reasons when they cannot.
