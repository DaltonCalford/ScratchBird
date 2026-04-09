# Cluster Identity, Key Shard, and Write Fence Failure Model

## Scope

This file defines the failure-model boundary for:

- persisted cluster identity
- key-shard custody and quorum-sensitive security posture
- leader-term and routing-epoch write fencing

## Governing rule

Cluster identity and write fencing are correctness boundaries, not advisory
control features.

Canonical rule:

- stale or mismatched identity and fencing posture must fail closed
- local MGA truth is not waived by cluster-control trouble

## Cluster identity failure classes

The failure model must distinguish:

- standalone explicit identity
- cluster identity mismatch
- stale cluster-config epoch
- corrupted or unreadable identity state

Standalone identity is a valid non-cluster posture. It is not itself a fault.

Cluster mismatch or corrupted identity is a fault and must not be silently
downgraded into standalone operation.

## Key-shard custody failure classes

The failure model must distinguish:

- missing shard custody rows
- duplicate or inconsistent shard custody rows
- insufficient shard availability for quorum-sensitive operations
- stale holder-custody posture

Canonical rule:

- missing or inconsistent shard custody blocks quorum-sensitive security
  operations
- it does not authorize fail-open secret reconstruction

## Write-fence failure classes

The failure model must distinguish:

- stale leader term
- wrong leader identity
- stale routing epoch
- missing shard leader state

Canonical rule:

- any of these failures blocks cluster-lane write publication
- they do not permit best-effort write continuation

## Security-quorum linkage

Where remote confirmation or shard-threshold security posture is required, the
runtime must combine:

- failure-detector posture
- partition or quorum reachability posture
- security-quorum evaluation
- key-shard custody posture

Canonical rule:

- inability to prove required quorum is a deny or bounded-cache posture
- it is not permission to treat stale confirmation as live confirmation

## MGA boundary

Cluster-control faults do not redefine local MGA truth.

Canonical split:

- local page and row state plus transaction inventory remain local truth
- cluster identity, key-shard custody, and write fencing govern cluster-lane
  admission and security-sensitive remote posture

## Recovery boundary

Recovery from these fault classes must preserve:

- explicit identity verification
- explicit shard-custody verification
- explicit leader-term and routing-epoch verification

No recovery path may claim success while those checks remain unknown.
