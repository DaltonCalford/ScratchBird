# Cluster Heartbeat, Failure Detector, and Security Quorum Runtime Model

## Status

reconstructed_required_with_current_substrate

## Purpose

This document defines the manager-owned heartbeat model, failure-detector thresholds, network-partition tracking, and the relationship between cluster liveness and security quorum decisions.

## Manager-Owned Heartbeat Rule

The optional ScratchBird manager is the server-local cluster agent and heartbeat participant.

The listener is not the heartbeat authority.

Heartbeat and cluster-control semantics belong to:

- manager
- cluster catalog and policy
- engine-admin and remote-management planes

This rule is canonical.

## Failure Detector Catalog

Current failure detector rows define:

- detector id
- cluster id
- detector kind
- heartbeat interval
- optional phi threshold
- optional miss threshold
- optional suspect threshold
- optional fail threshold
- startup grace interval

This means cluster liveness is already represented by typed policy, not just a boolean ping status.

## Network Partition Catalog

Current partition-state rows define:

- partition id
- cluster id
- partition state
- opened time
- optional resolved time
- quorum reachable flag
- local node id
- optional description

Partition member rows define:

- member id
- partition id
- node id
- side id
- reachable flag

This is direct code-backed support for partition-aware cluster behavior.

## Security Quorum Linkage

Security-sensitive cluster behavior must not rely only on liveness.

Instead, the runtime must combine:

- heartbeat/failure-detector state
- partition/quorum reachability state
- security quorum evaluation
- unlock policy and secret-shard threshold rules

This is how cluster liveness and cluster security are canonically joined.

## Security Quorum Evaluation Rule

When remote security quorum cannot be proven:

- `FAIL_OPEN` may permit a narrower bypass-cache path
- `FAIL_CLOSED` must deny
- `REQUIRE_REMOTE` must deny when remote confirmation is absent

This behavior is already reflected in the current `SecurityQuorum` runtime and is required for cluster security surfaces.

## Heartbeat Threshold Rule

The failure detector must interpret liveness through explicit thresholds, not a single monolithic timeout.

Canonical threshold classes include:

- miss
- suspect
- fail
- phi when a phi-style detector is used

The startup grace period is separate and must prevent premature failure classification during bring-up.

## Partition and Split-Brain Rule

If partition state implies quorum is not safely reachable, cluster runtime must:

- record the partition event
- evaluate quorum reachability explicitly
- avoid promoting security-sensitive actions that require remote confirmation
- coordinate with split-brain fence policy where replication or remote management channels depend on it

## Cached vs Remote-Sourced Decision Boundary

The cluster runtime may in some cases allow cached behavior when quorum evaluation allows `ALLOW_CACHE`.

When the evaluation yields `BYPASS_CACHE` or `DENY`, the runtime must not silently reuse cached cluster-security assumptions as if remote confirmation had succeeded.

This is the canonical cache-safety rule for cluster heartbeat and security coordination.

## Partial-Implementation Boundary

Current code proves:

- failure-detector catalog structures
- partition catalog structures
- quorum reachability state
- security quorum runtime semantics
- manager-owned heartbeat intent already present in surrounding management canon

Current code read in this pass does not fully prove a completed production heartbeat bus and full cluster enforcement across all management and replication operations.

Therefore this document is reconstructed required behavior with current
substrate and partial implementation drift.
