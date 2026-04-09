Status: reconstructed_required

# All Implemented Index Families Primary Class Planning and Refusal Model

## Purpose

This document turns the primary-family parity rule into the optimizer’s canonical admission and refusal model.

## Canonical Rule

Every implemented index family is a primary planning class. Any absence from candidate generation must be explained by a structured refusal reason, not by a silent secondary-class downgrade.

## Candidate Admission Rule

For every query shape the optimizer shall:

1. enumerate all implemented families that are semantically legal
2. request current family-native metrics
3. create one or more candidate variants for each admitted family
4. preserve a refusal record for any legal family not admitted further

## Required Refusal Reasons

The canonical refusal reasons are:

- semantic mismatch
- unsupported operator shape
- missing required runtime capability
- unusable metrics
- stale metrics beyond current policy
- memory or accelerator admission refusal
- fail-closed family-specific safety rule

## Primary-Class Rule

No refusal reason may be expressed as:

- secondary family
- advisory only
- not preferred by default
- ignored unless hinted

Those are policy bugs, not canonical states.

## Plan Cache Interaction

If a family was absent due to a refusal reason during plan creation, the cached plan shall preserve that refusal reason so later diagnostics can explain the candidate set that actually existed.

## Non-Guarantees

This file does not require every family to win equally often. It requires every implemented family to enter the same primary-class admission framework.
