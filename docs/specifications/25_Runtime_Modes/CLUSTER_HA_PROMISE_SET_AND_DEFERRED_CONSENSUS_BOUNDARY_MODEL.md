# Cluster HA Promise Set and Deferred Consensus Boundary Model

Status: reconstructed_required

## Purpose

Close the product-scope ambiguity around cluster and HA by naming the current first-market promise set and the explicitly deferred consensus/replication set.

## First-market promise set

The current promise set includes:
- manager heartbeat and server-agent publication
- remote management instruction queue and drift inspection
- shared-definition coordination and cluster-shared identity rules
- row UUID continuity and remote execution preconditions
- routing epoch, fencing token, and session pin safety primitives
- proxy migration and weak-donor cutover workflows

## Explicitly deferred HA set

The following remain outside the first-market promise set unless a later section explicitly promotes them:
- autonomous leader election
- quorum commit authority
- replicated write-log authority
- automatic primary failover
- universal mixed-version rolling upgrade
- broad segmentation healing
- cluster-wide read-consistency parity

## Product rule

1. ScratchBird may ship bounded cluster management without claiming full HA consensus semantics.
2. Any surface outside the first-market promise set must stay `unsupported_boundary` or `target_state_only` until promoted with full operational truth.
3. Remote management and identity propagation do not imply replicated commit authority.

## Failover rule

Current canonical operations are operator-mediated and fail closed.
No specification may imply autonomous failover or term-based authority transfer unless the consensus and commit-authority sections are promoted out of unsupported-boundary status.
