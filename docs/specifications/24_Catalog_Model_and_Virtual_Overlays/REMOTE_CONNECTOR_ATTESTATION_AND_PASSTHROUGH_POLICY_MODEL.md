# Remote Connector Attestation and Passthrough Policy Model

## Scope

This file defines the current code-backed catalog substrate for remote connectors, connector attestation, and passthrough policy.

This file is authoritative for:

- remote connector identity and state
- connector attestation material
- connection-mount overlay behavior
- passthrough policy gating
- prepared remote statement catalog identity

## Remote connector row family

### Purpose

The remote connector family is the persisted control-plane record for an engine-side connector that talks to a foreign or peer engine.

### Current required fields

The current code-backed row requires:

- `remote_connector_id`
- `fdw_server_id`
- `fdw_id`
- `connector_name`
- `engine_name`
- `endpoint_uri`
- valid connector `state`

Optional persisted fields include:

- `default_mapping_id`
- `policy_id`
- `engine_version_text`
- `module_checksum`
- `last_probe_time`
- `last_ready_time`
- `failure_count`

### Admission rules

The current code enforces:

- referenced foreign server must exist
- default mapping, when present, must exist
- `last_ready_time >= last_probe_time` when both exist
- duplicate connector per foreign server is rejected
- duplicate connector name is rejected

### Attestation rules

The current code-backed state machine requires stronger attestation for `READY` and `DEGRADED`:

- `module_checksum` must be non-zero
- `engine_version_text` must be present and non-empty

Canonical rule:

- a connector may not be considered ready without attested runtime identity

### State transition rules

The current code validates state transitions against an explicit `isAllowedRemoteConnectorTransition` rule.

Canonical rule:

- connector state is not arbitrary admin text
- invalid transitions are rejected with `CONSTRAINT_VIOLATION`

## Overlay mount behavior

The current code-backed connector row has side effects on the overlay schema tree:

- when a valid connector is present, an overlay child under `connections` is ensured for `connector_name`
- when a connector is renamed, the prior mount is dropped if names no longer conflict
- when a connector is invalidated or deleted, the previous connection mount schema is dropped

Canonical rule:

- connector catalog state and overlay mount exposure are coupled
- connection mounts are catalog-derived, not free-form namespace creation

## Remote passthrough policy

### Purpose

This policy row defines what kinds of remote execution are allowed through a specific connector.

### Required fields

The current code-backed row requires:

- `remote_policy_id`
- `remote_connector_id`
- `audit_level`

At least one of the following must be enabled:

- `allow_query`
- `allow_dml`
- `allow_ddl`
- `allow_admin`
- `allow_procedural`

### Optional constraints

The current row also supports:

- `allow_join_local_txn`
- `required_capabilities`
- `max_rows`
- `max_bytes`
- `timeout_ms`

### Canonical meaning

This row is the authoritative gate for remote passthrough. It prevents a connector from implicitly becoming a universal tunnel.

Canonical rule:

- passthrough permission is connector-scoped and explicit
- local transaction joining is separately controlled
- capability requirements can be persisted and audited

## Current connector capability profile

The current `udr_connector.cpp` runtime proves a connector bootstrap scaffold exists for:

- PostgreSQL
- MySQL
- Firebird
- ScratchBird
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

### Current bootstrap capability categories

The current scaffold publishes capability-profile families such as:

- metadata snapshot
- projection mapping
- query passthrough
- DML passthrough
- DDL passthrough
- admin passthrough
- prepared lifecycle
- transaction modes
- cancel and timeout semantics
- describe or comment surface
- error mapping
- degraded mode
- signoff readiness

### Meaning

The current connector layer is not “no implementation”. It already has typed engine families, default transport profiles, default auth profiles, and capability declarations.

What it does not prove yet is full commercial-grade parity for all remote engines.

## Prepared remote statement substrate

The current code around remote prepared statements already requires persisted identity for:

- connector
- session
- statement name
- command text
- remote handle
- optional expiry

Canonical rule:

- prepared remote execution is catalog-addressable state, not only transient process memory

## Proxy and trusted-channel boundary

The current code elsewhere in `catalog_manager.cpp` already enforces:

- forwarded identity is rejected on untrusted proxy channels
- trusted proxy channels require proxy identity
- trusted proxy channels require MTLS transport rule

Canonical rule:

- remote connector and proxy lanes are security-bound
- forwarded identity is never accepted merely because a connector exists

## Current code-backed versus reconstructed-required behavior

### Current code-backed

The current code proves:

- persisted connector rows
- attested ready or degraded states
- state-transition validation
- overlay mount projection
- connector-scoped passthrough policy
- bootstrap connector capability scaffolds for many donor engines

### Required reconstructed behavior

The lost-spec rebuild requires this substrate to be used for:

- donor-engine capability assessment
- cutover planning
- passthrough safety gating
- eventual data proxy and migration orchestration

### Drift rule

No donor-engine or proxy integration may bypass:

- connector attestation
- connector state transition rules
- passthrough policy
- trusted-proxy identity rules
