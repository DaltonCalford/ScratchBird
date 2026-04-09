# Workload Class Resolution and Admission Binding Model

## Purpose

This document defines the current ScratchBird workload-class resolution,
admission-policy binding, and runtime lease behavior from the implemented
`WorkloadGovernance` runtime.

This file is authoritative for:
- how a statement is classified into a workload class
- how an admission policy is selected
- how queueing, rejection, and lease release work
- how later accelerator and device-specific governance extends the existing
  governance pipeline without replacing it

This file does not redefine MGA transaction truth, lock truth, or durability.
Governance is an admission and scheduling layer only.

## Current code-backed authority

The current runtime authority is the `WorkloadGovernance` subsystem and the
catalog rows it consumes for:
- workload classes
- workload routes
- admission policies
- admission bindings
- SLO telemetry and evaluation rows
- error-budget status rows
- admission tuning history rows

The current query descriptor fields consumed by the resolver are:
- `connection`
- `sql`
- `database_name`
- `schema_name`
- `client_app`
- `resource_tag`

When a descriptor omits `schema_name`, `client_app`, or `resource_tag`, the
runtime derives them from the live connection context.

## Descriptor normalization

Before class matching begins, the runtime shall normalize descriptor state in
this order:
1. `database_name`:
   - use the supplied descriptor value if non-empty
   - else use the basename of the current database path
2. `schema_name`:
   - use the supplied descriptor value if non-empty
   - else use `connection.current_schema()`
   - else use the first element of the connection search path
3. `client_app`:
   - use the supplied descriptor value if non-empty
   - else use session variable `APPLICATION_NAME`
   - else use session variable `SB$APPLICATION_NAME`
4. `resource_tag`:
   - use the supplied descriptor value if non-empty
   - else use session variable `RESOURCE_TAG`
   - else use session variable `SB$RESOURCE_TAG`
5. `user_name`:
   - derive from the current user id through catalog lookup when present
6. `role_name`:
   - derive from the active role id through catalog lookup when present
7. `query_type`:
   - derive from SQL classification
8. `statement_tag`:
   - derive from SQL statement tagging

All string matching is case-insensitive unless the specific match mode says
otherwise.

## Workload class resolution algorithm

The runtime shall resolve workload classes as follows:
1. read all workload-class catalog entries
2. discard rows where `is_valid = false` or `is_enabled = false`
3. sort the remaining rows by:
   - `priority` descending
   - `class_name` ascending
4. iterate in sorted order and evaluate `match_kind`
5. the first successful match wins
6. if no class matches, resolution succeeds with `matched = false`

Supported current match kinds are:
- `ROLE`
- `USER`
- `DATABASE`
- `SCHEMA`
- `CLIENT_APP`
- `STATEMENT_TAG`
- `QUERY_TYPE`
- `REGEX`
- `RESOURCE_TAG`
- `CUSTOM`

Matching semantics are:
- `ROLE`, `USER`, `DATABASE`, `SCHEMA`, `CLIENT_APP`, `STATEMENT_TAG`,
  `RESOURCE_TAG`: case-insensitive equality
- `QUERY_TYPE`: lowercased equality against the derived query-type label
- `REGEX`: case-insensitive regex search over raw SQL text
- `CUSTOM`:
  - first attempt case-insensitive regex search over raw SQL text
  - if the regex is invalid, fall back to case-insensitive substring search

An invalid regex must never abort classification. It may only cause that rule to
fail match and continue scanning.

## Admission binding selection algorithm

After class resolution, the runtime shall select an admission policy as follows:
1. read all admission policies
2. discard rows where `is_valid = false` or `is_enabled = false`
3. for each surviving policy, read all bindings for that policy
4. discard bindings where `is_valid = false` or `is_enabled = false`
5. admit a binding candidate when either:
   - `target_kind = CLUSTER`
   - `target_kind = WORKLOAD_CLASS` and the resolved class id matches
6. if the candidate set is empty, binding resolution succeeds with
   `matched = false`
7. otherwise sort candidates by:
   - `binding.priority` ascending
   - `policy_name` ascending
8. choose the first candidate as the effective policy and binding

`CLUSTER` bindings are global default governance scopes.
`WORKLOAD_CLASS` bindings are workload-specific overlays.

## Session association and counter hygiene

Once a class or policy is selected, the runtime shall associate live session
state with the current `proc_id`.

The runtime shall:
- refresh active connection proc-id inventory from the database security-stack
  snapshot
- prune stale `session_class_map_` entries whose proc id is no longer active
- prune stale `session_policy_map_` entries whose proc id is no longer active
- associate the current session with the resolved class when present
- associate the current session with the active policy when a lease is granted

Governance counters are session-derived and may not become independent truth.
If the live session inventory and the cached map disagree, the live inventory is
authoritative.

## Admission checks

The admission decision shall evaluate the effective policy in this order:
1. validate that the catalog manager is active
2. resolve workload class
3. resolve admission binding
4. if no binding matched, admit immediately
5. evaluate `max_concurrent_sessions`
6. evaluate `max_concurrent_queries`
7. if the policy rejects immediately, return a non-admitted decision
8. if the policy queues, block under the policy condition variable until either:
   - capacity becomes available
   - queue timeout is reached
   - the policy or binding becomes invalid
9. on successful admission, return an active lease

Current code-backed rejection semantics include at least:
- `GOV_1500`: internal governance resolution failure
- `GOV_1501`: `max_concurrent_sessions` rejection

The policy-level reject mode remains authoritative for:
- immediate rejection
- bounded queueing
- queue timeout

## Lease semantics

An `AdmissionLease` represents one active policy admission for one live proc id.

A lease shall:
- be move-only
- release automatically on destruction
- release explicitly when the governed statement or session finishes the scoped
  activity
- decrement active-query counters on release
- preserve the resolved workload-class id and policy id for the active scope

Governance leases are runtime coordination only. They do not participate in MGA
visibility, commit publication, or crash recovery truth.

## Required reconstructed extension for accelerator workloads

Accelerator admission shall extend the existing model instead of replacing it.
The extension is mandatory and now code-backed for Beta 1.

Additional required classifier inputs are:
- vector-search workload kind
- ANN family runtime class
- accelerator-required flag
- accelerator-preferred flag
- accelerator-capability profile
- target device affinity hint

The extension shall operate as follows:
1. perform ordinary workload-class resolution first
2. evaluate whether the chosen access-path family requests accelerator service
3. map the request into an accelerator profile:
   - `CPU_ONLY`
   - `CPU_PREFERRED`
   - `ACCELERATOR_PREFERRED`
   - `ACCELERATOR_REQUIRED`
4. apply admission policy, device budget, and residency budget checks
5. if no accelerator lane is available:
   - reject when the request is `ACCELERATOR_REQUIRED`
   - otherwise fall back according to the index family contract

This preserves one governance pipeline for SQL, vector, maintenance, and
accelerator work.

## Required catalog growth

To make accelerator governance implementable without ambiguity, the canonical
catalog model shall extend the existing workload admission-policy and
admission-binding rows rather than introduce a parallel accelerator catalog.

Admission policy rows shall additionally carry:
- accelerator profile name
- memory budget bytes
- pinned residency target bytes
- concurrent build limit
- concurrent search limit
- prewarm policy
- fallback policy
- degraded-state override

Admission binding rows shall additionally carry:
- device class
- device id or device pool id

These are required Beta 1 specification elements and are materialized through
the existing admission-policy and admission-binding catalog rows.

## Operator surfaces

The runtime shall continue to support current governance inspection for:
- routing plan
- admission status
- SLO status
- error-budget status
- admission tuning history

The same runtime must also expose accelerator-governance status once present,
including:
- workload-class to accelerator-profile mapping
- active accelerator admissions
- queued accelerator admissions
- memory budget pressure
- forced fallback count
- residency warmup state

## Non-guarantees

The governance layer does not guarantee:
- fair sharing across all statements regardless of configured policy
- MGA lock arbitration
- distributed cluster placement truth
- durability correctness
- implicit GPU availability

Governance is authoritative for admission and scheduling policy only.
