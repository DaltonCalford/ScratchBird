# Cluster Identity and Write Fencing Runtime Model

## Scope

This file defines the current code-backed runtime model for:

- cluster identity carried by the database header
- shard leader state
- write fencing
- routing epoch validation

This file is authoritative for cluster write admission while the broader cluster-control lane is being rebuilt.

## Cluster identity runtime basis

The database carries persisted cluster identity in page-zero header state:

- `cluster_id`
- `node_id`
- `cluster_config_epoch`

Canonical rule:

- cluster write admission and cluster routing decisions must bind to persisted local cluster identity
- a standalone database is explicitly represented by zeroed cluster and node identity, not by missing metadata

## Shard leader state

The current write-fencing runtime uses leader-state material per shard:

- `shard_id`
- `leader_node_id`
- `leader_term`
- `routing_epoch`

This leader-state material is authoritative for write admission. It is not advisory telemetry.

## Write admission request

The current code-backed write admission request includes:

- `shard_id`
- `node_id`
- fencing token carrying:
  - `shard_id`
  - `leader_term`
- optional `routing_epoch`

## Write admission decision

### Allowed write

A write is admitted only when all of the following hold:

- shard leader state exists
- request `node_id` is the current `leader_node_id`
- fencing token `leader_term` matches current shard leader term
- if a routing epoch is supplied, it matches the current shard routing epoch

### Rejection classes

The current tests prove the runtime distinguishes:

- stale fencing token
- not current leader
- routing epoch mismatch

The current code maps those to stable result classes such as:

- `INVALID_TRANSACTION_STATE`
- `PERMISSION_DENIED`

### Canonical meaning

This is fail-closed cluster write admission. A stale leader or stale routing view does not get a best-effort write path.

## Fencing after leadership rotation

The unit tests prove:

- after leadership rotates and `leader_term` increments, the old leader stays fenced
- a request carrying the old term is rejected even if other state is unchanged
- only the new leader with the new term is admitted

Canonical rule:

- leader rotation must invalidate earlier write authority immediately

## Routing epoch discipline

The unit tests also prove:

- stale routing epoch is independently rejectable even when leader term matches

Canonical rule:

- write fencing depends on both leader authority and routing authority
- a node may be the right leader for the wrong routing epoch and must still be fenced

## Relationship to MGA

Write fencing does not replace MGA. It sits outside tuple visibility and protects cluster write ownership at the shard or route level.

Canonical split:

- MGA decides row visibility and commit truth
- cluster write fencing decides whether a given node may publish the write on that shard path

## Current code-backed versus reconstructed-required behavior

### Current code-backed

The current code proves:

- persisted cluster identity
- runtime shard leader state
- leader-term fencing
- routing-epoch fencing

### Required reconstructed behavior

The rebuilt cluster-control lane must carry these same invariants through:

- remote deployment
- manager-driven routing changes
- shard migration cutover
- remote management assess or apply operations

No remote-control or cluster queue implementation may bypass leader-term and routing-epoch fencing.
