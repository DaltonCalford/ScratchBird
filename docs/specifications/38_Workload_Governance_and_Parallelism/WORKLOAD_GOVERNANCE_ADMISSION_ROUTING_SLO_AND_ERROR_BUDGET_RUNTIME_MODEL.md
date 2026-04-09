Status: current_authority

# Workload Governance Admission, Routing, SLO, and Error Budget Runtime Model

## Purpose

This file defines the runtime model for workload governance: admission,
routing-plan inspection, SLO evaluation, and error-budget inspection.

## Query descriptor model

The current runtime query descriptor carries:

- connection
- SQL text
- database name
- schema name
- client application
- resource tag

## Admission decision model

The current runtime admission decision carries:

- admitted flag
- queued flag
- status
- code
- detail
- class ID
- policy ID
- class name
- policy name

Admission is therefore a structured runtime contract, not a boolean yes/no flag.

## Admission lease model

Admission uses a lease object with the following current semantics:

1. the lease is move-only
2. the lease is active or inactive
3. the lease remembers workload class and policy identity
4. lease release returns counters to the governance subsystem

## Runtime matching model

The current runtime classifies and routes work using bounded query metadata.

Current code-backed classification behavior includes:

1. statement-tag extraction from SQL
2. query-type classification including:
   - select
   - insert
   - update
   - delete
   - merge
   - copy
   - ddl
   - other
   - unknown

## Admission status snapshot

The current admission snapshot row carries:

- scope
- class name
- policy name
- reject mode
- binding priority
- class priority
- max concurrent sessions
- max concurrent queries
- max queue depth
- queue timeout
- active sessions
- active queries
- queued queries
- class enabled flag
- policy enabled flag
- binding enabled flag

## Routing plan snapshot

The current routing plan snapshot row carries:

- class name
- class priority
- route name
- target kind
- target label
- role
- service type
- transport
- route weight
- fallback route name
- class enabled flag
- route enabled flag

## Reject-mode model

Current reject modes are:

- `REJECT`
- `QUEUE`
- `SHED_LOW_PRIORITY`

## SLO telemetry and evaluation model

The current runtime accepts SLO telemetry samples containing:

- node ID
- role
- sample time
- CPU utilization percentage
- queue pressure percentage
- current node count
- validity flag

The runtime then evaluates catalog-backed SLO policies over those samples and
persisted SLO windows.

## SLO status snapshot

The current SLO status row carries:

- node ID
- node name
- role
- profile name
- evaluation time
- window start
- window end
- request count
- success count
- error count
- availability target percentage
- availability SLI percentage
- latency p95 target
- latency p95 observed
- latency p99 target
- latency p99 observed
- error-rate target percentage
- error-rate SLI percentage
- short burn rate
- long burn rate
- burn severity
- action plan
- binding-present flag
- metrics-present flag

## Error budget snapshot

The current error-budget row carries:

- node ID
- node name
- role
- profile name
- evaluation time
- window start
- window end
- allowed bad requests
- observed bad requests
- remaining bad requests
- remaining budget percentage
- short burn rate
- long burn rate
- burn severity
- binding-present flag
- metrics-present flag

## Action-plan model

Current code-backed action-plan vocabulary includes:

- `NONE`
- `ADMISSION_TIGHTEN`
- `SCALE_OUT`
- `SCALE_OUT_AND_TIGHTEN`
- `INCIDENT_PAGE`

## Runtime truth rule

Runtime snapshots are derived from:

1. catalog policy rows
2. live counter state
3. live or recent telemetry
4. persisted SLO window history

## Fail-closed rules

The governance runtime shall not:

1. fabricate route or policy identities not present in catalog truth
2. report an admitted workload as active without an active lease
3. report SLO or error-budget health without indicating whether binding or metrics are actually present
