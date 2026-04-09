# Shard Routing, Multi-Shard Guard, and Commit Log Model

## Scope

This file defines the current code-backed runtime model for:

- deterministic shard routing
- routing epoch validation
- multi-shard write guard policy
- shard transaction ordering
- per-shard durable commit log

This file is authoritative for the current cluster write path substrate that already exists in code.

## Deterministic shard router

### Purpose

The deterministic shard router chooses a single routing target for a given:

- `table_id`
- `shard_key`
- `routing_epoch`

### Current plan model

The current code-backed routing plan persists in runtime memory as:

- `table_id`
- `routing_epoch`
- non-empty `targets`

Each target currently carries:

- `shard_id`
- `leader_node_id`
- `leader_endpoint`
- `route_weight`

### Admission rules for plans

The current code refuses routing plans when:

- `table_id` is zero
- `targets` is empty
- any target has zero `shard_id`
- any target has zero `leader_node_id`
- any target has `route_weight = 0`

### Routing algorithm

The current code-backed selection algorithm is:

1. locate routing plan by `table_id`
2. reject if plan missing
3. reject if plan has no targets
4. if request carries `expected_routing_epoch`, reject if it differs from plan `routing_epoch`
5. compute deterministic `FNV-1a 64-bit` hash over:
   - `shard_key`
   - delimiter `|`
   - `table_id.toString()`
6. reduce hash modulo total route weight
7. walk targets in plan order using cumulative weight
8. choose the first target whose cumulative weight exceeds the bucket

### Canonical consequences

The routing path is:

- deterministic for identical inputs
- weighted
- epoch-bound

The unit tests prove identical inputs produce the same chosen shard and leader identity.

### Stale-routing refusal

The unit tests prove stale routing epoch is rejected with:

- `Status::INVALID_TRANSACTION_STATE`
- reason `STALE_ROUTING_EPOCH`

Canonical rule:

- a client or executor carrying stale routing epoch must replan or rebind; it must not be silently rerouted on stale topology assumptions

## Multi-shard write guard

### Purpose

The current runtime already has a guard for write sets spanning more than one shard.

### Policy shape

The current guard policy is:

- `allow_cross_shard`
- `require_explicit_override`

### Evaluation algorithm

The current guard computes unique shard count for the write set and then applies policy:

- if only one unique shard is touched, the guard allows the write
- if multiple unique shards are touched and `allow_cross_shard = false`, reject
- if multiple unique shards are touched and `allow_cross_shard = true` but `require_explicit_override = true` and no override is present, reject
- if multiple unique shards are touched and explicit override is present, allow

### Current refusal reasons

The unit tests prove:

- `MULTI_SHARD_WRITE_NOT_ALLOWED`
- `MULTI_SHARD_WRITE_REQUIRES_OVERRIDE`

Canonical rule:

- cross-shard writes are not implicit
- policy and explicit override both matter

## Write fencing boundary

The cluster write path has multiple independent guard layers:

- routing epoch validation
- leader identity validation
- leader term validation
- multi-shard guard validation
- session epoch pin validation

Canonical rule:

- these layers are cumulative, not interchangeable
- satisfying one guard does not waive the others
- refusal must be fail-closed at the earliest violated guard

The cluster write lane must never silently downgrade a stale or fenced write
into an accepted local write on the assumption that later repair will fix it.

## Session epoch pins

The current runtime surface already includes `SessionEpochPins` and validation for:

- `cluster_config_epoch`
- `schema_epoch`
- `security_epoch`

These are separate from routing epoch and leader term. They protect broader session correctness against stale cluster, schema, or security assumptions.

Canonical rule:

- cluster routing epoch, session epoch pins, and leader fencing are distinct guard layers and must not be collapsed into one generic stale-state error model

## Shard transaction ordering

### GTXID

The current code-backed global transaction identity for a shard lane is:

- `shard_id`
- `local_txn_id`

### Order book

The runtime already tracks per-shard:

- `last_allocated`
- `last_committed`
- `last_applied`

This is authoritative ordering state for the shard runtime lane.

### Canonical meaning

The shard transaction order book is not MGA visibility truth. It is cluster or replication ordering truth for the shard lane.

It exists to sequence shard-lane publication and follower apply. It does not
replace tuple-version visibility, transaction inventory truth, or page-image
durability truth inside the local MGA engine.

## Shard commit log

### Purpose

The current runtime already implements a durable per-shard commit log.

### Durable path

The durable file path is:

- `root_directory / <shard_uuid>.scl`

### Current entry shape

Each persisted line is tab-separated and contains:

- `local_txn_id`
- `commit_timestamp_ns`
- `payload_format`
- hex-encoded `payload`

### Append algorithm

The current code-backed append path is:

1. reject zero `shard_id` or zero `local_txn_id`
2. reject `payload_format` containing tab or newline
3. create root directory if needed
4. compute expected next `local_txn_id` from in-memory per-shard state
5. reject if incoming `local_txn_id` is not exactly the expected next value
6. serialize one tab-separated line ending with newline
7. open file in append-binary mode
8. write full line
9. `fflush`
10. `_commit` on Windows or `fsync` on POSIX
11. `fclose`
12. only then update in-memory `last_local_txn_id_by_shard`

### Durability rule

Canonical rule:

- append completion requires flush plus fsync-style durability, not buffered append alone
- append success means the shard-lane derivative durability step completed for
  that entry

## MGA and anti-WAL rule

The shard commit log is not the local database truth source.

Canonical rule:

1. local MGA commit, page state, and transaction inventory remain authoritative
   for local database truth
2. the shard commit log is a derivative cluster-ordering lane used for shard
   publication, apply ordering, and recovery of the cluster lane
3. no implementation may reinterpret the shard commit log as a universal WAL
   that supersedes local MGA truth
4. corruption or unavailability of the shard commit log is a cluster-lane
   failure, not permission to rewrite local MGA correctness rules

### Read algorithm

The current code-backed read path is:

1. open shard log file
2. read line by line
3. split exactly into four TSV fields
4. parse numeric fields
5. require strictly increasing sequence starting at `1`
6. hex-decode payload
7. materialize ordered entries

### Corruption and refusal rules

The current code returns corruption or refusal on:

- malformed line structure
- malformed numeric fields
- out-of-order sequence
- malformed hex payload

Canonical rule:

- malformed or out-of-order commit-log material is fail-closed
- the read path must not silently skip bad entries and continue as though the
  shard log were valid

## Operator evidence contract

Routing and commit-log behavior must remain diagnosable through operator
surfaces. At minimum, the runtime and observability lanes must make it possible
to distinguish:

- stale routing epoch refusal
- stale leader or wrong leader refusal
- multi-shard guard refusal
- out-of-order local transaction id refusal
- durability write failure
- malformed read-path commit-log refusal
- malformed payload encoding

Canonical rule:

- shard commit log is append-ordered and self-checking
- corruption is fail-closed

## Relationship to MGA

The shard commit log is not the primary recovery truth for ScratchBird. ScratchBird remains MGA-first.

Canonical split:

- MGA database state is the recovery authority
- shard commit log is a cluster or replication ordering artifact for the shard lane

So:

- the commit log supports cluster ordering and follower apply
- it does not convert ScratchBird into a WAL-authoritative engine

## Observability contract

The current observability registry already exposes:

- `sb_cluster_leader_term`
- `sb_cluster_fencing_rejections_total`
- `sb_cluster_routing_requests_total`
- `sb_cluster_routing_epoch`
- `sb_cluster_replication_lag_txn`
- `sb_cluster_replication_lag_seconds`
- `sb_cluster_replication_apply_total`

Canonical rule:

- routing, fencing, and replication lanes must remain operator-visible
- rejection reason classes must stay externally countable

## Current code-backed versus reconstructed-required behavior

### Current code-backed

The current code proves:

- deterministic weighted shard routing
- stale-routing rejection
- explicit multi-shard write guard
- shard leader-term and routing-epoch fencing
- durable per-shard commit-log append and read path

### Required reconstructed behavior

The rebuilt cluster manager and remote deployment lane must preserve these invariants when:

- changing routing plans
- changing cluster configuration epoch
- scheduling migrations
- applying remote management instructions that alter shard placement or write policy

### Drift rule

No future cluster control or proxy migration feature may:

- bypass routing epoch validation
- bypass leader-term fencing
- silently permit cross-shard writes that require override
- treat the shard commit log as WAL truth
