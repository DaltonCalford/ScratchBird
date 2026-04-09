# Remote Proxy, Migration, and Non-Native Replication Runtime Model

## Status

Reconstructed required specification with partially implemented current runtime.

## Purpose

This document defines the runtime model ScratchBird must use when acting as:

- a remote proxy
- a passthrough execution engine
- a migration bridge
- a replication coordinator for engines without native MGA-compatible replication

## Runtime Layers

The current code-backed runtime substrate includes:

- UDR connector interface
- remote connection pool
- connector capability and metadata catalogs
- passthrough policy
- remote transaction binding
- publication/subscription and replication channel catalogs
- GC cleanup publication records for downstream derivative consumers

These layers together form the runtime basis for proxy and migration behavior.

## UDR Connector Runtime

Current connector types include:

- PostgreSQL
- MySQL
- Firebird
- ScratchBird
- ODBC
- Cassandra
- Milvus
- MongoDB
- Neo4j
- Redis
- MariaDB
- InfluxDB
- ClickHouse
- OpenSearch
- DuckDB

The connector interface currently requires runtime support for:

- initialize and shutdown
- ping and reconnect
- query execution
- command execution
- prepared statements
- cursors
- transaction begin/commit/rollback
- savepoints
- schema introspection
- copy and streaming operations

This means remote bridge execution is a real current runtime contract, not only a catalog wish list.

## Remote Connection Pool Runtime

Current remote connection pooling is structured as:

- registry
- per-server pool
- per-user pool

The pool tracks at least:

- total, active, idle, and waiting connections
- acquire/release totals
- created/destroyed counts
- timeouts
- validation failures
- average lifetime
- average acquire wait

This gives the proxy runtime a real bounded resource model.

## Pooling Semantics

The current runtime model is:

- acquire from idle if possible
- otherwise create until max connections
- otherwise wait with timeout
- on release:
  - rollback if the remote adapter is still in transaction
  - reset the connection
  - return it to idle
  - or destroy it if failed

This is a safe baseline for proxy reuse and prevents accidental cross-request transaction leakage.

## Remote Transaction Binding Rule

ScratchBird is always in a local transaction context.

Remote systems may or may not support compatible transaction participation.

Therefore proxy and migration runtime must use explicit remote transaction binding state instead of assuming ambient transaction equivalence.

The canonical rule is:

- if a remote engine supports explicit remote transaction control, ScratchBird may bind a local transaction to a tracked remote transaction token and state
- if a remote engine does not provide a compatible transaction model, ScratchBird must degrade to bounded, policy-controlled execution instead of pretending the remote side joined the local MGA transaction

## Remote transaction capability classes

The rebuilt runtime model requires every connector to be classified into one of
these transaction-capability classes:

1. `FULL_REMOTE_TXN_CONTROL`
2. `REMOTE_TXN_WITH_LIMITS`
3. `STATEMENT_ONLY`
4. `NO_SAFE_MUTATION_JOIN`

Meaning:

- `FULL_REMOTE_TXN_CONTROL`: the connector can expose begin, commit, rollback,
  savepoint, and bounded remote-transaction identity strong enough for tracked
  binding
- `REMOTE_TXN_WITH_LIMITS`: the connector can expose remote transaction control
  but cannot satisfy the full local MGA intent for every operation class
- `STATEMENT_ONLY`: the connector can execute statements but cannot safely join
  local transactional intent
- `NO_SAFE_MUTATION_JOIN`: the connector may be usable for read, discovery, or
  bounded copy, but must not participate in mutation or replay paths that would
  imply transaction equivalence

This capability class is a first-class runtime input for proxy, migration, and
replication admission.

## Passthrough Policy Rule

All remote execution must be gated by passthrough policy.

Policy decides whether the runtime may admit:

- query
- DML
- DDL
- admin
- procedural execution
- local/remote transaction joining

When `allow_join_local_txn` is false, the engine must not fake shared transaction semantics.

This rule is critical for non-native replication and migration safety.

## Transaction-barrier model for non-native systems

For remote systems that do not provide natural MGA-compatible transactional
participation or durable replication forcing, ScratchBird must use explicit
transaction barriers.

The canonical barrier classes are:

1. `DISCOVERY_BARRIER`
2. `SNAPSHOT_BARRIER`
3. `COPY_WINDOW_BARRIER`
4. `CUTOVER_FENCE`
5. `DERIVATIVE_REPLAY_BARRIER`
6. `VALIDATION_BARRIER`

Meaning:

- `DISCOVERY_BARRIER`: capability and metadata discovery only
- `SNAPSHOT_BARRIER`: capture remote metadata and source identity anchor before
  copy or replay begins
- `COPY_WINDOW_BARRIER`: bounded copy phase where source and destination drift
  must be measured, not assumed away
- `CUTOVER_FENCE`: explicit refusal point preventing concurrent mode confusion
  during promote or switchover
- `DERIVATIVE_REPLAY_BARRIER`: downstream replay or apply phase that is
  derivative and policy-governed, not primary transaction truth
- `VALIDATION_BARRIER`: post-phase proof of row counts, signatures, epochs,
  hashes, or other comparison evidence before promotion

These barriers are mandatory for engines without natural replication or forced
transaction semantics compatible with ScratchBird's local MGA model.

## Non-Native Replication Rule

For remote engines that do not support a natural replication stream or an MGA-compatible transaction participation model, ScratchBird must rely on a bounded combination of:

- metadata snapshotting
- remote execution audit
- retry and error inventory
- publication/subscription selection
- replication-channel batching and lag controls
- explicit copy/migration phases

The engine must fail closed rather than claiming transparent replication parity where none exists.

## Non-native apply and replay rule

For non-native engines, replay or replication apply is always derivative.

It must not be described as:

- shared MGA state
- native logical replication parity
- transparent two-way transactional join

Instead it must be described as a controlled combination of:

- snapshot or metadata anchor
- copy and chunked transfer
- remote execution audit
- retry and quarantine state
- validation and cutover fence

## Migration Model

The reconstructed migration model is phase-based:

1. discover remote capabilities
2. capture remote metadata snapshot
3. build or refresh schema mapping
4. classify objects as:
   - passthrough-capable
   - copy-required
   - unsupported
5. execute bounded copy or query phases under passthrough policy
6. track remote errors, retry state, and execution audit
7. promote to replication or subscription state only when policy and connector capability permit it

The required reconstructed runtime also adds:

8. establish a source-identity anchor before cutover
9. fence mutation or route-change surfaces during cutover
10. validate destination state before promotion
11. quarantine rather than auto-promote when validation is incomplete or
    ambiguous

This model is required even where the current runtime implementation is still incomplete.

## GC Publication and Derivative Replication Boundary

Current GC publication records are derivative outputs published only after MGA truth is already known.

They are not WAL or redo truth.

Proxy, migration, and replication consumers may use these derivative records for downstream cleanup or audit coordination, but they may not treat them as primary transaction truth.

## Split-Brain and Retry Runtime Boundary

Replication channel policy already exposes:

- conflict policy
- retry counts
- retry backoff
- lag thresholds
- split-brain fence enablement
- split-brain detect window

The required runtime meaning is:

- conflicting or ambiguous replication state must fence or quarantine rather than silently diverge
- retry is policy-controlled
- migration and proxy lanes must preserve audit and error inventory through retries

## Cutover rule

Cutover is never an implicit side effect of "copy looks complete."

A conforming runtime must require:

1. explicit cutover intent
2. current capability compatibility
3. policy admission
4. validation barrier success
5. route-change fence publication

If any of these are absent, the runtime must remain in pre-cutover or
quarantined state.

## Partial-Implementation Boundary

Current code strongly proves:

- outbound connector interface
- connection pooling
- catalog substrate for remote proxy and replication metadata
- derivative GC publication semantics

Current code read in this pass does not prove that the full migration-state machine and every non-native replication lane are already complete across all supported connectors.

Therefore this is reconstructed required specification with partially implemented current runtime.
