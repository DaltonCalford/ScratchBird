# Cloud Health Probe, Telemetry, and Support-Bundle Model

Status: reconstructed_required
Section: 20_Diagnostics_Audit_and_Observability

## Purpose

Define the minimum observability and incident-handling contract required for cloud operation.

## Health probe classes

1. Startup probe
- indicates whether the process has completed required initialization and is ready to attempt service readiness
- must remain failing during recovery, startup quarantine, incompatible catalog refusal, or missing secret/certificate material

2. Readiness probe
- indicates whether the node should receive new traffic for the exposed service role
- must fail when the role cannot safely accept new work
- may fail while liveness remains healthy during drain, rotation, or degraded maintenance windows

3. Liveness probe
- indicates whether the process is alive enough to avoid forced replacement
- must not be used as a substitute for readiness or correctness state
- must not return success if the process is hung on an unrecoverable internal failure path

4. Degraded-state health surface
- reports that service is alive but operating under bounded refusal, pressure, replication derivative backlog, shadow-group degradation, or startup quarantine

## Structured telemetry requirements

Cloud-operable deployments must emit:

- structured logs with stable event names and identity fields
- metrics suitable for scraping or collection by standard cloud observability systems
- trace-compatible correlation identifiers for multi-layer request paths
- node, service-role, deployment-generation, and storage-identity fields

At minimum, telemetry must separate:

- manager state
- listener state
- parser-pool state
- engine durable health
- derivative shipping health
- shadow-group readiness
- backup and restore automation state

## Support-bundle model

A support bundle must capture, at minimum:

- deployment identity and generation
- effective configuration material references, excluding raw secret payloads
- probe states and last transitions
- structured logs for manager, listener, parser, and engine layers
- critical metrics snapshots
- active alarms and degraded-state classifications
- backup, restore, derivative queue, and shadow-group state if configured
- compatibility manifest identifiers
- platform and packaging profile identifiers

Support-bundle generation must be non-interactive and suitable for incident automation.

## Cloud incident requirements

A cloud deployment must be able to answer:

- which node and service role failed
- whether the failure is liveness, readiness, or correctness related
- whether MGA durability is healthy while derivative lanes are degraded
- whether current pressure is caused by cgroup limits, engine refusal, or external infrastructure failure
- whether a rolling restart, restore, or manual operator action is required
