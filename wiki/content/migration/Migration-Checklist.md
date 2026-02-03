# Migration Checklist

**Last Updated:** 2026-02-03

---

## Pre‑Migration

- [ ] Inventory source schemas, tables, indexes, triggers, procedures.
- [ ] Capture data size, row counts, and top tables.
- [ ] Decide migration scope (whole DB vs subset of schemas/tables).
- [ ] Identify compatibility gaps and required type mappings.
- [ ] Prepare rollback plan and operational ownership.

## Source Preparation

- [ ] Enable CDC (logical replication / binlog / trigger capture).
- [ ] Create a dedicated migration user with required privileges.
- [ ] Confirm network connectivity from ScratchBird to source.
- [ ] Snapshot baseline row counts for comparison.

## ScratchBird Preparation

- [ ] Create target database and schema tree.
- [ ] Create foreign server definition for source.
- [ ] Create migration source with CDC config.
- [ ] Verify authentication and connectivity from ScratchBird.

## Bulk Load Phase

- [ ] START MIGRATION for a pilot table.
- [ ] Verify bulk load progress and throttling.
- [ ] Confirm index creation strategy (defer indexes if needed).

## CDC Synchronization

- [ ] Confirm CDC stream connected and lag metrics flowing.
- [ ] Monitor pending change queue and apply rate.
- [ ] Validate row counts and checksums (sample or full).

## Dual‑Write Phase

- [ ] Enable DUAL_WRITE and validate consistency.
- [ ] Confirm write conflicts are handled per strategy.
- [ ] Run application smoke tests against ScratchBird.

## Cutover

- [ ] VALIDATE MIGRATION (sample or full).
- [ ] CUTOVER table or schema.
- [ ] Confirm routing switches to LOCAL_ONLY.

## Post‑Cutover

- [ ] Monitor errors and performance.
- [ ] Verify no CDC lag and no remote writes.
- [ ] Decommission legacy access when ready.

## Rollback (If Required)

- [ ] ROLLBACK MIGRATION with preserve‑local‑data option if needed.
- [ ] Verify routing switches to remote.
- [ ] Capture errors and remediation notes.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
