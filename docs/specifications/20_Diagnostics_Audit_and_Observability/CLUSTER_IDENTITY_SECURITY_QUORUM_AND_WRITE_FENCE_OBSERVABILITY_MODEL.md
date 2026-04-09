# Cluster Identity, Security Quorum, and Write Fence Observability Model

## Scope

This file defines the operator-visible observability contract for:

- persisted cluster identity posture
- cluster-config epoch posture
- write-fence refusals
- routing-epoch refusals
- security-quorum posture
- partition and quorum reachability posture

## Current code-backed authority

Current code-backed recovery proves:

1. database header state persists cluster identity and cluster-config epoch
2. write admission distinguishes stale fencing token, wrong leader, and routing
   epoch mismatch
3. failure-detector, partition, and quorum-evaluation substrate already exists
4. current routing and fencing observability already exposes leader term,
   routing epoch, and fencing rejection counters

## Required operator outputs

The operator-visible lane must remain able to surface, at minimum:

- local cluster identity presence class
- cluster-config epoch
- leader term by shard, when applicable
- routing epoch by shard, when applicable
- write-fence refusal counts by reason
- quorum reachability posture
- partition posture
- security-quorum decision posture

## Cluster identity posture

Cluster identity observability must distinguish:

- standalone database identity posture
- cluster-bound database identity posture
- stale or mismatched cluster-config epoch posture, when detected

Canonical rule:

- a zeroed standalone identity is a valid explicit state
- it must not be conflated with corrupted or missing identity state

## Write-fence refusal posture

Operator outputs must preserve distinct refusal classes for at least:

- stale fencing token
- not current leader
- routing epoch mismatch

Canonical rule:

- write-fence refusals must not be collapsed into one generic denial counter

## Security-quorum posture

The observability lane must preserve the evaluated security-quorum posture such
as:

- cache-allowed posture
- bypass-cache posture
- deny posture
- require-remote posture

If current runtime surfaces expose a coarser classification, that is
implementation drift against this canonical model.

## Partition and quorum posture

Partition and quorum observability must allow operators to correlate:

- partition state
- quorum reachable flag
- security-quorum evaluation
- write-fence or remote-confirmation consequences

## Fail-closed rules

The observability layer shall not:

1. report healthy cluster write posture while fencing refusals are hidden
2. report healthy security quorum while quorum reachability is unknown or
   denied without surfacing that distinction
3. collapse standalone identity, clustered identity, and corrupted identity
   posture into one generic state
