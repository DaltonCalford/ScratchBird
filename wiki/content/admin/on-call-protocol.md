# On-Call Protocol

**Last Updated:** 2026-03-13

---

## Scope

This protocol covers the non-cluster ScratchBird runtime stack:

- native listener
- PostgreSQL listener
- MySQL listener
- Firebird listener
- parser pools
- engine
- manager or service controller where deployed

---

## Severity Levels

| Severity | Definition | Response |
|----------|------------|----------|
| `SEV-1` | Service unavailable or unsafe for continued writes | Page primary and secondary immediately; engineering owner joins without waiting for business hours |
| `SEV-2` | Sustained degraded readiness, open burn, or widespread admission rejection | Page primary immediately; secondary joins if not stabilized in 15 minutes |
| `SEV-3` | Single-tenant or contained degradation with workaround | Primary responds during on-call window; escalate if spread increases |
| `SEV-4` | Documentation, drill, or low-risk operational follow-up | Queue for business hours and track in the weekly ops review |

---

## First 15 Minutes

1. Confirm the affected listener or dialect and database.
2. Verify stack health:
   ```bash
   pgrep -af 'sb_listener|sb_parser|sb_server|sb_manager'
   ```
3. Open a native listener session and record:
   ```sql
   SHOW READINESS HEALTH WINDOW MINUTES 15;
   SHOW ALERT DASHBOARD WINDOW MINUTES 15;
   SHOW SLO STATUS;
   SHOW ERROR BUDGET STATUS;
   SHOW SUPPORT BUNDLE SAFETY WINDOW MINUTES 60;
   ```
4. If the incident is capacity-related, also record:
   ```sql
   SHOW AUTOSCALE ACTIONS WINDOW MINUTES 60;
   SHOW ADMISSION TUNING HISTORY WINDOW MINUTES 60;
   ```
5. Start the incident log before restarting any component.

---

## Escalation Ladder

1. Primary on-call
2. Secondary on-call
3. Shared runtime owner
4. Domain owner for the affected subsystem:
   - security or auth for auth or TLS failures
   - storage or recovery for backup, restore, sweep, or retained-evidence failures
   - listener or parser owner for protocol-specific ingress failures
5. Release or security owner if the event suggests supply-chain or
   vulnerability exposure

Escalate immediately without waiting for timeout when:

- readiness is `BLOCKED`
- error budget is exhausted or actively burning without mitigation
- support-bundle safety reports missing or unsafe evidence
- listener recovery would require bypassing the supported listener/parser/server
  boundary

---

## Operator Rules

- Do not bypass the listener/parser boundary to reach the engine directly.
- Do not assume a transactionless session exists. Every reconnect or post-commit
  session is already in a new transaction.
- Do not treat `public` as the unconditional default schema. Resolve the
  effective schema from session override, then user/role/group defaults, then
  `users.public`.
- For MySQL emulation incidents, validate only native MySQL dialect behavior.

---

## Evidence Packet

Every escalated incident should carry:

- severity and customer impact
- affected dialect or listener
- database and resolved current schema
- readiness health snapshot
- alert dashboard snapshot
- SLO and error-budget snapshot
- autoscale and admission-tuning snapshot when applicable
- support-bundle safety output
- bundle manifest reference if a support bundle was captured
- restart or recovery actions already attempted

---

## Communication Cadence

| Severity | Update Interval | Minimum Audience |
|----------|-----------------|------------------|
| `SEV-1` | 15 minutes | On-call chain plus product and ops stakeholders |
| `SEV-2` | 30 minutes | On-call chain plus service owner |
| `SEV-3` | 60 minutes | On-call chain |
| `SEV-4` | Next business update | Ticket owner |

---

## Drill Requirements

- Primary and secondary on-call operators must complete the scheduled drill set
  in the current PH6 drill calendar.
- Every drill must exercise the listener/parser/server boundary and record the
  PH6 readiness, SLO, and support-bundle evidence surfaces.
- Any drill gap that leaves an operational step ambiguous must become a
  documentation update before the drill is marked complete.

---

## Exit And Review

An incident may move to follow-up only when:

- readiness is back to `READY` or a documented accepted `DEGRADED` state
- alerts are acknowledged and routed
- current burn state is understood and stable
- support evidence is attached to the incident record
- ownership for follow-up actions is explicit

---

*Last updated: 2026-03-13 | Wiki version synced with PH6 operational closure*
