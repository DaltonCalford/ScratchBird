# V3 Cache and Buffer Architecture Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/CACHE_AND_BUFFER_ARCHITECTURE.md`

## Summary
- This document is explicitly labeled **non-authoritative** at the top, and it is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- The body also contains a contradictory line: `Status: Authoritative (V3)`.
- Per the inventory rule, this spec is treated as non-authoritative; conformance should be verified against the authoritative specs it references.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative in its header. The internal `Status: Authoritative (V3)` line conflicts with the header and inventory.

## Conformance Checklist (Defer to Authoritative Specs)
The following items are requirements *if* they appear in authoritative specs. This file is not authoritative, so the items below are tracked for cross-reference only and should be verified in the corresponding authoritative specs.

[ ] Buffer pool architecture requirements (clock-sweep, scan-resistant rings, hot/cold segmentation, read-ahead, adaptive flush).
[ ] LSM block cache requirements (sharded LRU/CLOCK, optional admission control, optional compression).
[ ] Statement/plan cache requirements (SQL->SBLR cache, plan cache keyed by schema/privilege versions, deterministic invalidation).
[ ] Result cache requirements (deterministic-only, schema/privilege keyed, table version invalidation, TTL/memory bounds).
[ ] Translation cache requirements (per-dialect, segmented by schema/privileges, shared or per-connection).
[ ] Cache coherence/invalidation rules (schema versioning, privilege bundles, table version IDs, MGA snapshot safety).
[ ] Monitoring requirements (Prometheus metrics + SQL views for cache stats).
[ ] Configuration surface keys and runtime mapping.

## Notes
- For implementation verification, use the authoritative specs referenced within this doc (e.g., `storage/STORAGE_ENGINE_BUFFER_POOL.md`, `operations/MONITORING_SQL_VIEWS.md`, `transaction/*`, etc.).
- The contradictory `Status: Authoritative (V3)` line should be reconciled (either remove or add to inventory if intended to be authoritative).
