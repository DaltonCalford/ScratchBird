# V3 Design Limits Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/design_limits.md`

## Summary
- This document is explicitly labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- It defines limits for page sizes, FSM, buffer pool, tuples, TOAST, identifiers, etc., but these must be verified against authoritative storage/catalog/type specs.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Conformance Checklist (Defer to Authoritative Specs)
Captured for cross-reference only; verify in authoritative storage/catalog/type specs:

[ ] Page sizes (8K–128K) and page header size.
[ ] Usable page space calculations.
[ ] FSM chaining requirements and header fields.
[ ] Buffer pool defaults/limits and pin-count overflow handling.
[ ] Column/tuple size limits, null bitmap sizing, tuple header size.
[ ] TOAST thresholds, chunk sizes, max value sizes, compression behavior.
[ ] Catalog identifier length limits (128 chars) and per-page capacity assumptions.
[ ] Data type length/precision rules.
[ ] Transaction limits (no WAL, 64-bit XID).
[ ] Limit-related error codes.

## Notes
- Any authoritative limit values should be moved into authoritative specs; otherwise treat this file as guidance.
