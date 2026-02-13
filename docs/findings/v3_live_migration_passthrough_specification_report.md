# V3 Live Migration Passthrough Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md`

## Summary
- This document is explicitly labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- It defines a comprehensive migration gateway feature set (routing, CDC, dual-write, SQL surface, system tables, metrics). Treat as advisory unless moved into authoritative specs.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Conformance Checklist (Defer to Authoritative Specs)
Captured for cross-reference only; verify only if authoritative specs exist for migration features:

[ ] Migration state machine and SYS$MIGRATION_STATE table.
[ ] Query routing interception after semantic analysis; routing decision matrix.
[ ] Hybrid query execution and dialect translation requirements.
[ ] Background migration worker, bulk load, checkpoints, CDC processors, change applier.
[ ] Dual-write coordination, 2PC, conflict detection/resolution, conflict logging table.
[ ] Cutover validation and rollback procedures.
[ ] SQL surface: CREATE/ALTER MIGRATION SOURCE, START/PAUSE/RESUME/ABORT/CUTOVER/ROLLBACK, SHOW/VALIDATE commands.
[ ] Monitoring: Prometheus metrics, alert rules, system tables for migration history/CDC positions.
[ ] Security: credential management, audit logging, RLS handling, TLS requirements.
[ ] Config surface in `sb_server.conf` and per-migration options.
[ ] Error codes and retry/recovery semantics.

## Notes
- If live migration is a required V3 feature, it must be added to the authoritative inventory and reconciled with architecture layers (engine vs parser vs gateway components).
