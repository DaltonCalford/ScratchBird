# V3 Engine Core Unified Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/ENGINE_CORE_UNIFIED_SPEC.md`

## Summary
- The document is explicitly labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- It aggregates and summarizes requirements from many authoritative specs. Conformance must be verified against those underlying specs, not here.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Conformance Checklist (Defer to Authoritative Specs)
The following items are summary pointers. Verify implementation in the referenced authoritative specs instead of this doc.

[ ] Storage engine requirements (on-disk format, page sizes, buffer pool, tablespaces, TOAST/LOB, compression).
[ ] Catalog/schema system requirements (catalog root, schema path resolution, dependency tracking).
[ ] Transaction/locking/GC requirements (MGA lifecycle, lock manager, sweep/GC).
[ ] DDL/DML execution semantics (object lifecycle, table features, DML semantics, triggers).
[ ] Index requirements (index types, versioning/GC, migration-safe maintenance).
[ ] Query optimizer and execution requirements (stats, cost model, plan caching, SBLR execution).
[ ] Types and serialization requirements (type system, on-disk encoding).
[ ] Security core requirements (auth, authorization, audit).
[ ] Monitoring and observability requirements (system views, metrics).
[ ] Backup/restore requirements.
[ ] Scheduler/job system requirements.
[ ] UDR runtime requirements.

## Notes
- This unified spec is best treated as a navigation index for the authoritative documents listed in section 3.
- If this document is intended to be authoritative, it must be added to the inventory and the non-authoritative banner removed.
