# V3 Cascade Drop Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/CASCADE_DROP_SPECIFICATION.md`

## Summary
- This document is explicitly labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- It defines conservative RESTRICT-only drop semantics and dependency handling rules. Treat as advisory unless moved into authoritative specs.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Conformance Checklist (Defer to Authoritative Specs)
Captured for cross-reference only; verify in authoritative DDL/catalog/dependency specs:

[ ] RESTRICT-only drop policy (no CASCADE) across object types.
[ ] Owned vs dependent object categories and auto-drop order.
[ ] Error message format and dependency listing requirements.
[ ] Dependency query APIs / SYS.DEPENDENCIES usage.
[ ] Drop flow (transactional behavior, soft delete, cache invalidation).
[ ] Test case expectations per object type.

## Notes
- If conservative drop semantics are required for V3, they must be specified in authoritative DDL/cascade/drop specs and added to the inventory.
