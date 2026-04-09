# Non-Cluster Upgrade and Rollback Certification Model

## Scope

This file defines the current code-backed certification model for single-node or non-cluster upgrade and rollback validation.

This is authoritative for the current shipped certification lane proven by unit tests and backup-manager behavior.

## Purpose

The non-cluster certification lane proves that a standalone ScratchBird database can:

- create a backup
- restore it
- validate rollback checkpoint compatibility
- reopen the restored database
- assess threshold compliance

without relying on cluster-only orchestration.

## Current certification basis

The current unit tests prove certification against:

- baseline current-version backups
- legacy standalone backup compatibility
- rollback checkpoint version handling
- source and restored database identity continuity

## Required artifact path

The current code-backed certification path uses:

- a live database
- backup creation through `BackupManager`
- restore validation through `runRestoreValidation`

### Current result contract

The validation result currently exposes at least:

- backup chain
- applied backup count
- verified backup count
- backup verified flag
- restore completed flag
- reopen validated flag
- rollback checkpoint validated flag
- thresholds passed flag
- source database id
- restored database id
- rollback checkpoint version
- source database version
- source compat version

Canonical rule:

- certification is not pass/fail by human judgement alone
- these structured result fields are the canonical evidence contract

## Backup and restore identity rules

The current tests prove:

- current baseline restore validation must preserve database identity expectations
- legacy standalone backups remain admissible when the compatibility contract permits

Canonical rule:

- restore validation is identity-aware
- it is not enough for pages to parse; source and restored identity must satisfy the compatibility contract

## Rollback checkpoint rules

The current code-backed test lane treats rollback checkpoint version as explicit certification state.

Canonical rule:

- rollback checkpoint compatibility is a first-class certification gate
- a single-node upgrade or restore path is not certified if rollback checkpoint validation fails

## Threshold rules

The restore-validation lane already includes threshold-based certification such as:

- maximum restore time
- maximum RPO

Canonical rule:

- time and recovery thresholds are part of certification evidence, not merely informative metrics

## Non-cluster boundary

This file covers the single-node or non-cluster lane only.

It does not claim:

- cluster-wide rolling upgrade certification
- distributed term-fencing upgrade certification
- cross-node rollback orchestration

Those belong to reconstructed cluster certification lanes elsewhere in section `31`.

## MGA boundary

This certification lane is still MGA-first:

- restore validation is against durable database state
- backup chain verification does not convert ScratchBird into WAL-authoritative recovery
- rollback checkpoint validation is subordinate to MGA truth and durable page-state correctness
