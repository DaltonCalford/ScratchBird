Status: reconstructed_required_with_current_substrate

# Remote Management Local and Cluster Persistence Consistency Model

## Purpose

This document defines the canonical persistence model for remote-management instructions, deployments, and drift records across the target database and the cluster management layer.

## Current code-backed authority

Current code in Beta 1 proves the bounded dual-persistence substrate consumed by
package `05`:

- target-local listener runtime-target and generation rows persist applied or
  refused generation state plus `drift_state`
- cluster-fabric link, session, task, event, and error rows persist the
  cluster-side deployment object families used by the manager-side control seam
- remote-connector audit, binding, and error rows preserve adjacent remote
  execution and heartbeat-style timing state where current control paths
  require it

The operator-facing instruction lifecycle continues to build on these
persisted families. It does not replace them with a separate durability model.

## Canonical Rule

Accepted remote-management changes shall be retained in two authoritative scopes:

- the local database receiving or applying the change
- the cluster management record of the change, its intent, and its deployment history

Loss of one scope is a consistency fault, not an implementation detail to ignore.

## Dual-Persistence Objects

The dual-persisted object families are:

- instruction records
- target-binding records
- assessment results
- apply results
- drift records
- deployment-event history
- capability snapshots relevant to admission or refusal

## Local Persistence Role

Local persistence is authoritative for:

- the target database’s last accepted configuration state
- the target-local apply status
- local refusal details
- local drift against the intended target state

## Cluster Persistence Role

Cluster persistence is authoritative for:

- queued intent
- multi-target deployment coordination
- deployment history across servers
- remote auditability
- replay or reassessment of pending instructions

## Consistency States

The canonical dual-persistence consistency states are:

- `CONSISTENT`
- `LOCAL_ONLY_PENDING_CLUSTER_CONFIRM`
- `CLUSTER_ONLY_PENDING_LOCAL_CONFIRM`
- `DIVERGED`
- `REQUIRES_OPERATOR_REPAIR`

## Publication Order

For a new remote-management instruction, the canonical publication order is:

1. persist cluster intent record
2. assign instruction identity and target set
3. deliver for local assessment
4. persist local assessment result
5. persist cluster assessment acknowledgment
6. apply locally if admitted
7. persist local apply result
8. persist cluster deployment-event confirmation

## Failure Rules

If the sequence above is interrupted, the system shall preserve enough state to determine:

- whether the instruction was only queued
- whether it was assessed but not applied
- whether it was applied locally but not cluster-confirmed
- whether local and cluster records disagree

Automatic cleanup shall never erase the ability to classify the instruction into one of these states.

## Drift Rules

Drift may be discovered from:

- target-local effective state diverging from intended state
- cluster record diverging from target-local record
- capability changes making a previously admitted instruction no longer valid

Every drift record shall identify both the intended source state and the observed target state.

## Repair Rules

Repair may take one of these actions:

- cluster record refresh from trusted local proof
- local replay from trusted cluster intent
- quarantine of the instruction or target
- operator-required manual reconciliation

## Non-Guarantees

This file does not require full distributed consensus for all management actions. It requires explicit dual-persistence semantics and drift classification.
