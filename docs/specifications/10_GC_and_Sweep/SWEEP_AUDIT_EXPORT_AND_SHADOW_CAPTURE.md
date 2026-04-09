# Sweep Audit Export and Shadow Capture

Status: current_authority

## Purpose

ScratchBird sweep may emit derivative records to aid replication, shadowing, and temporal forensics. These outputs are downstream of local MGA truth. They are never the primary recovery authority.

## Derivative lanes

Sweep supports three derivative lanes:

- `write_after_log`: ordered post-commit publication stream used for replication and downstream consumers
- `shadow_capture`: page-image duplication into one or more shadow files under the same forced-write discipline as the primary filespace
- `temporal_archive`: export of committed and, where policy permits, rolled-back transaction history into an external archive store or temporal database

## Truth-source precedence

Truth-source precedence is fixed:

1. local durable MGA state
2. committed transaction inventory and prepared-state evidence
3. verified page images and version chains
4. derivative sweep outputs

Derivative outputs may explain, replicate, or preserve history. They must not be used to override local durable truth.

## Required event payload

For every destructive reclaim action, the emitted derivative record shall contain at minimum:

- filespace identifier
- page identifier
- page family
- version-chain identifier or row locator
- creating transaction identifier
- superseding or deleting transaction identifier if present
- reclaim classification
- sweep pass identifier
- committed schema epoch
- checksum state
- generation or repair markers relevant to the page family
- emission timestamp and ordering key

## Write-after log rules

The write-after log shall follow these rules:

- it is emitted after the authoritative local commit state exists
- it is emitted before destructive prune when the lane is enabled for sweep capture
- it is suitable for replication and downstream audit consumption
- it is not a WAL and must not be treated as replay authority for correctness reconstruction
- missing or delayed write-after records must be treated as derivative-lane degradation, not transaction-truth loss

## Shadow capture rules

When shadow capture is enabled:

- page images copied to a shadow file must preserve page checksums, generation markers, and page-family identity
- forced-write posture must match the primary filespace's correctness requirements
- shadow lag is not permitted to outrun local prune when the policy is `required_before_prune`
- shadow failure in `best_effort` mode marks the sweep pass degraded but does not redefine local truth

## Temporal archive rules

Temporal archive export shall follow these rules:

- committed history is eligible for export after local commit truth exists and before destructive prune when configured
- rolled-back history may be exported only if policy explicitly allows forensic retention of rolled-back actions
- exported history is immutable derivative evidence and must preserve transaction-state class
- archive export must not block ordinary visibility rules inside the primary engine

## Policy modes

Each derivative lane shall operate in one of these policy modes:

- `disabled`
- `best_effort`
- `required_before_prune`

`required_before_prune` means sweep shall retain reclaimable history until the derivative lane succeeds or the policy is changed. `best_effort` means local sweep may proceed after recording the failure.

## Failure handling

If a derivative lane fails:

- record the failure with page, row, transaction, and lane identifiers
- downgrade lane health for operator visibility
- refuse prune for affected items if the lane is `required_before_prune`
- continue local validation and non-destructive walk where safe

## Relationship to section 42

These derivative lanes support recovery, replication, shadowing, and forensics, but section 42 remains authoritative on the rule that recovery is performed by reconciling durable MGA state rather than replaying derivative logs.
