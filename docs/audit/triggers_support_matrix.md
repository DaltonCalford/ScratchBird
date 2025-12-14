# Trigger Support Matrix (Snapshot)

**Database-level trigger events (catalog enums present):**
- ON CONNECT
- ON DISCONNECT
- ON TRANSACTION START
- ON TRANSACTION COMMIT
- ON TRANSACTION ROLLBACK

Catalog operations: create/drop/get/list/enable exist. Runtime firing path not fully validated here; needs executor/server hook audit to confirm events invoke triggers.

**Table-level trigger events (typical):**
- BEFORE/AFTER INSERT
- BEFORE/AFTER UPDATE
- BEFORE/AFTER DELETE
- BEFORE/AFTER SELECT (not confirmed)

Current codebase has table trigger catalog structures; need verification of SELECT triggers and runtime firing.

## Gaps/To Verify
- Ensure runtime hooks fire database triggers on connect/disconnect/txn start/commit/rollback.
- Confirm table trigger firing on INSERT/UPDATE/DELETE; verify SELECT triggers are supported or mark unsupported.
- Dependency and drop-blocking: ensure triggers are linked to underlying tables/routines and prevent drops when referenced.
- Emulated engines: preserve native behavior; do not add trigger capabilities beyond their dialects.
- Trigger ordering: triggers have a smallint order and should run lowest→highest for before/after chains; verify implementation honors ordering.
- Runtime path: before trigger runs before data is prepared/returned; after trigger runs after prepare/row emission; document/verify these points.

## Actions
- Audit executor/server codepaths for trigger firing per event and add missing hooks.  
- If SELECT triggers unsupported, document and/or remove from advertised surface.  
- Add tests: DB triggers per event; table triggers per DML; (if applicable) SELECT triggers.  
