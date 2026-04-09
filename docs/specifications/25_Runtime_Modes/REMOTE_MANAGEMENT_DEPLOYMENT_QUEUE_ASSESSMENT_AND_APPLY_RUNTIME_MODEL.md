Status: reconstructed_required

# Remote Management Deployment Queue Assessment and Apply Runtime Model

## Purpose

This document defines the canonical runtime lifecycle for remote-management instructions once they enter the manager and engine control plane.

## Canonical Rule

Every remote-management instruction shall move through a deterministic queue, assessment, and apply lifecycle. Direct mutation outside that lifecycle is non-conforming for remote operations.

Beta 1 package boundary:

- for package `07`, the admitted runtime is a local single-target assessment and apply queue for one managed server or database target
- multi-target cluster dispatch, cross-node fanout, and cluster deployment-history consensus remain later-scope surfaces and must fail closed here

## Runtime Lifecycle

The canonical lifecycle is:

1. queued
2. capability snapshot attached
3. assessment started
4. assessment admitted or refused
5. apply pending
6. apply executing
7. apply committed
8. post-apply verification
9. completed or quarantined

## Queue Identity

Each queue item shall preserve:

- instruction identity
- submission source
- target set
- priority or scheduling class
- expiration or review deadline if any
- capability snapshot reference

## Assessment Rules

Assessment shall evaluate at minimum:

- target availability
- target capability compatibility
- privilege and policy authorization
- safety and maintenance windows
- dependency requirements between changes
- drift against target-local current state

## Apply Rules

Apply may begin only after successful assessment. The runtime shall record:

- apply start time
- local executor identity
- resulting state
- refusal or failure reason if not committed

## Verification Rules

After apply, the runtime shall verify:

- target-local state matches the accepted instruction
- local persistence and any admitted management history record are aligned
- the change did not leave the target in a degraded unmanaged state

## Quarantine Rules

The runtime shall quarantine when:

- assessment and apply evidence disagree
- local and cluster persistence diverge after retry
- a partial apply cannot be proven safe
- the change materially affects security, memory, plugin, or listener posture and final proof is missing

## Control Plane Boundary

The manager and controller may coordinate the lifecycle, but the listener remains a bounded local runtime surface and is not the authority for cluster deployment history.

For the bounded Beta 1 single-target lane, the authoritative durable state is the target-local record plus any admitted single-target management history row. The runtime must not imply cluster-wide dispatch durability where no cluster target set exists.

## Non-Guarantees

This file does not claim the full queue runtime is already fully shipped. It defines the reconstructed required runtime contract.
