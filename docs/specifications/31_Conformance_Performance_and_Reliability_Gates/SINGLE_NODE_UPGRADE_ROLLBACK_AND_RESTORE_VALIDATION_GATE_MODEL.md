# Single-Node Upgrade, Rollback, and Restore Validation Gate Model

## Scope

This file defines the certification contract for non-cluster, single-node:

- upgrade validation
- rollback validation
- restore-backed recovery validation

It is the canonical gate model for the current code-backed single-node upgrade
and rollback certification lane.

## Current code-backed authority

Current code-backed recovery proves:

1. single-node upgrade and rollback certification already has a structured
   result contract
2. that certification is built on backup and restore validation
3. the gate is not merely an ad hoc shell transcript; it has typed result
   semantics

## Gate objective

The gate must prove that a single-node runtime can:

- upgrade without silent data loss
- roll back through a restore-backed path
- preserve correctness of catalog, storage, and index state after validation

## Required result classes

The gate result must distinguish, at minimum:

- upgrade success
- upgrade refusal
- rollback success
- rollback refusal
- restore validation success
- restore validation failure

## Required evidence families

The gate evidence package must include, at minimum:

- run identity
- source build or version identity
- target build or version identity
- backup identity
- restore validation identity
- result classification
- validation summary
- refusal summary when the gate fails

## Restore-backed rollback rule

Canonical rule:

- rollback in this lane is restore-backed, not blind binary downgrade
- restore validation is a first-class gate requirement, not an optional
  post-check

## Validation coverage

The certification lane must cover, at minimum:

- database openability after upgrade
- database openability after rollback
- catalog validity after restore
- storage and index validation after restore
- failure classification when restore validation does not pass

## Relationship to MGA and derivative lanes

Single-node upgrade and rollback certification must preserve the MGA truth
model.

Canonical rule:

- correctness is derived from durable page and row state plus transaction
  inventory truth
- derivative lanes such as export, archive, or shadow evidence do not replace
  restore validation as the acceptance gate

## Fail-closed rules

The gate shall not:

1. report rollback success without restore validation success
2. report upgrade success when required validation evidence is missing
3. collapse restore validation failure into a generic unknown error class

## Reconstructed-required expansion

The rebuild requires later promotion of:

- executed gate-run transcripts
- preserved artifact schema tables
- stronger tie-in between backup identity, restore identity, and operator
  runbook evidence
