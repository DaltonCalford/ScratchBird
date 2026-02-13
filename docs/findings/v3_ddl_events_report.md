# V3 DDL Events Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_EVENTS.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- Parser/emitter support exists for `POST_EVENT` with a **string literal only** event name.
- No executor handling for `SBLR3_PSQL_POST_EVENT` found in V3 executor, so runtime delivery appears unimplemented.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### POST_EVENT (ScratchBird native / Firebird emulation)
[~] Parser: recognizes POST_EVENT and captures expression.
    - `parsePostEventStatement` parses `event_name` as expression (not limited to literal).
[~] Emitter: emits `SBLR3_PSQL_POST_EVENT` **only if** `event_name` is a string literal; otherwise fails.
    - Payload schema only contains `event_name` string; no mode or message.
[ ] Executor: no `SBLR3_PSQL_POST_EVENT` handling found in `src/sblr/executor.cpp`.

### Spec Requirements Not Implemented
[ ] Delivery modes `ON COMMIT` vs `IMMEDIATE` not represented in AST/emitter/executor.
[ ] Optional `MESSAGE` payload not represented in AST/emitter/executor.
[ ] Event name length (127 bytes) validation not found.
[ ] Event UUID generation, payload handling, and registration/delivery mechanics not implemented in executor.

## Notes
- If event notifications are required in V3, they must be moved into authoritative specs and wired through executor + protocol layers.
