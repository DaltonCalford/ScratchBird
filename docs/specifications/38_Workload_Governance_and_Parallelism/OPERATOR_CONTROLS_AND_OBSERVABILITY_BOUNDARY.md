# Operator Controls and Observability Boundary

## Scope

This file defines the operator-facing governance control and inspection boundary
for:

- workload admission
- routing-plan inspection
- SLO and error-budget inspection
- admission-tuning evidence
- autoscale-action evidence
- support-bundle governance summaries

This file does not invent an external orchestrator plane. It defines the
current code-backed operator truth and the reconstructed-required shape of the
governance control surface while the lost-spec rebuild is completed.

## Current code-backed operator truth

The current runtime already exposes structured governance state rather than
free-form text. The authoritative operator inspection families are:

- admission status snapshots
- routing plan snapshots
- SLO status snapshots
- error-budget snapshots
- support-bundle governance safety summaries

These surfaces are derived from:

1. catalog-backed workload class, policy, routing, SLO, and error-budget rows
2. runtime lease counters and queue counters
3. live or recent telemetry samples
4. bounded autoscale and admission-tuning evidence

## Inspection contract

### Admission inspection

The operator-visible admission surface must expose, at minimum:

- scope
- class identity
- policy identity
- reject mode
- binding priority
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

### Routing inspection

The operator-visible routing surface must expose, at minimum:

- class identity
- route identity
- target kind
- target label
- role
- service type
- transport
- route weight
- fallback route identity
- class enabled flag
- route enabled flag

### SLO and error-budget inspection

The operator-visible SLO and error-budget surfaces must expose, at minimum:

- node identity
- role
- evaluation time
- window start
- window end
- burn severity
- action plan
- binding-present flag
- metrics-present flag

The operator surface must never claim healthy governance posture while hiding
that bindings or metrics are absent.

## Control boundary

### Current code-backed rule

Current operator control remains catalog-backed and policy-backed. Runtime
admission behavior is not configured by ad hoc worker-local toggles.

### Reconstructed-required rule

The rebuilt governance control plane must preserve all of the following:

1. policy mutation is catalog-backed
2. runtime decisions are derived from policy plus live counters plus telemetry
3. support bundles expose governance summaries as derivative evidence
4. remote management or cluster-management layers may inspect or stage policy
   changes, but they do not bypass catalog truth

## Queue and worker visibility

The current canonical operator promise is bounded:

- queue depth and active-query counts are visible through governance snapshots
- worker behavior is visible only where runtime diagnostics or gate evidence
  expose it directly
- there is no implied universal real-time scheduler console

## Support-bundle coupling

Governance status is part of operational evidence. Support-bundle or readiness
surfaces must expose governance counts for at least:

- SLO status
- error-budget status
- autoscale actions
- admission tuning actions

These summaries are observational evidence. They are not themselves a control
plane.

## Remote and cluster boundary

The reconstructed manager or cluster-control layer may surface governance
inspection and staged policy changes, but the canonical truth sources remain:

- governance catalog rows
- runtime governance counters
- governance telemetry and evaluation outputs

No later spec may introduce a shadow governance truth source that bypasses
those persisted and runtime families.

## Fail-closed rules

The governance operator surface shall not:

1. fabricate class, policy, route, or profile identities not present in
   catalog truth
2. report active work without an active lease-backed count
3. report healthy SLO or error-budget state without disclosing whether binding
   and metrics are present
4. imply a fleet-wide external orchestrator parity surface that current code
   does not prove

## Explicit non-guarantees

- no comprehensive external workload admin console is implied
- no universal real-time worker dashboard is implied
- no non-catalog shadow control plane is permitted
