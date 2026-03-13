# Operations Runbook

**Last Updated:** 2026-03-13

---

## Topology And Operating Invariants

- Client traffic never connects directly to the engine. The supported path is
  `listener -> parser pool -> server (IPC)`.
- Operate and troubleshoot the listener, parser pool, and engine as one stack.
  A healthy engine process alone is not a healthy service.
- Administrative SQL surfaces should be run through the native listener, not by
  bypassing the listener/parser boundary.
- Every session is always in a transaction. `COMMIT` or `ROLLBACK` ends the
  current transaction and immediately starts the next one.
- Current schema resolution is:
  1. explicit session override
  2. user default
  3. role default
  4. group default
  5. `users.public`
- MySQL emulation accepts native MySQL dialect only.

---

## Five-Minute Health Check

1. Verify the runtime stack is present:
   ```bash
   pgrep -af 'sb_listener|sb_parser|sb_server|sb_manager'
   ```
2. Confirm the native listener can answer PH6 operational queries:
   ```sql
   SHOW READINESS HEALTH WINDOW MINUTES 15;
   SHOW ALERT DASHBOARD WINDOW MINUTES 15;
   SHOW SLO STATUS;
   SHOW ERROR BUDGET STATUS;
   SHOW SUPPORT BUNDLE SAFETY WINDOW MINUTES 60;
   ```
3. If the incident involves saturation or scaling:
   ```sql
   SHOW AUTOSCALE ACTIONS WINDOW MINUTES 60;
   SHOW ADMISSION TUNING HISTORY WINDOW MINUTES 60;
   ```
4. If the readiness state is `DEGRADED` or `BLOCKED`, capture a redacted
   support bundle through the deployment's support-bundle entrypoint before
   restarting services.

---

## Runbook Index

| Scenario | Trigger | First Checks | Required Evidence |
|----------|---------|--------------|-------------------|
| Service unavailable | Listener port unreachable or protocol handshake failure | Process list, listener logs, readiness health | Stack status, affected listener, first failure timestamp |
| Error-budget burn | SLO or error budget status shows burn/open breach | `SHOW SLO STATUS`, `SHOW ERROR BUDGET STATUS`, alert dashboard | Role, node, SLI values, burn events |
| Admission saturation | Requests rejected or queued work does not drain | `SHOW AUTOSCALE ACTIONS`, `SHOW ADMISSION TUNING HISTORY`, readiness health | Rejection codes, autoscale actions, tuning events |
| Support escalation | Customer-impacting incident needs operator handoff | `SHOW SUPPORT BUNDLE SAFETY`, capture support bundle, alert dashboard | Redacted bundle manifest, readiness state, evidence families present |
| Sweep/evidence blockage | Prune backlog or evidence-retention alerts | Readiness health, support-bundle safety, sweep logs | Retained evidence status, local spool state |
| Parser or listener churn | Repeated reconnects or dialect-specific failures | Listener/parser process list, native client probe, alert dashboard | Affected dialect, parser pool health, recent restarts |

---

## Incident Playbooks

### Service Unavailable

1. Confirm whether the failure is at the listener, parser, or engine layer.
2. Prefer restoring the listener/parser stack before restarting the engine.
3. Do not expose or use a direct engine port as a workaround.
4. Once service returns, run:
   ```sql
   SHOW READINESS HEALTH WINDOW MINUTES 15;
   SHOW ALERT DASHBOARD WINDOW MINUTES 15;
   ```

### SLO Burn Or Error-Budget Breach

1. Record the affected role and node:
   ```sql
   SHOW SLO STATUS;
   SHOW ERROR BUDGET STATUS;
   ```
2. If admission changes were applied recently, confirm whether the runtime has
   already tightened or relaxed governance:
   ```sql
   SHOW AUTOSCALE ACTIONS WINDOW MINUTES 60;
   SHOW ADMISSION TUNING HISTORY WINDOW MINUTES 60;
   ```
3. Escalate immediately if the role remains `BLOCKED`, if burn is still open
   after mitigation, or if alerts are unrouted or unacked.

### Support Bundle Capture

1. Check whether a safe redacted bundle can be produced:
   ```sql
   SHOW SUPPORT BUNDLE SAFETY WINDOW MINUTES 60;
   ```
2. Capture the bundle through the supported support-bundle entrypoint for the
   deployment.
3. Attach the bundle manifest, readiness snapshot, and relevant SLO/error-budget
   rows to the incident record.

### Schema Or Session-Context Confusion

1. Confirm the connected identity and the resolved current schema.
2. Remember that reconnecting does not create a transactionless session; a new
   transaction starts immediately after `COMMIT` or `ROLLBACK`.
3. If object visibility differs across users, review user/role/group default
   schema settings before assuming catalog corruption.

---

## Dialect-Specific Connectivity Notes

- PostgreSQL emulation: use a PostgreSQL-compatible client against the
  PostgreSQL listener.
- MySQL emulation: use a MySQL-compatible client and send native MySQL dialect
  only.
- Firebird emulation: use a Firebird-compatible client against the Firebird
  listener.
- Native administration: use the native listener for operational `SHOW ...`
  surfaces.

---

## Handoff Requirements

Before handing an incident to the next operator or engineering owner, include:

- impacted listener or dialect and affected database
- current readiness state
- SLO/error-budget snapshot
- autoscale/tuning snapshot if admission was involved
- support-bundle safety result and bundle manifest reference if captured
- exact timestamps for first failure, mitigation start, and current state

---

*Last updated: 2026-03-13 | Wiki version synced with PH6 operational closure*
