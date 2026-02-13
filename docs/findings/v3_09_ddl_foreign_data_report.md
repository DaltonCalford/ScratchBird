# V3 DDL Foreign Data Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/09_DDL_FOREIGN_DATA.md`

## Summary
- This document is explicitly labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- It describes FDW/SQL-MED DDL surface (FOREIGN DATA WRAPPER, SERVER, USER MAPPING, FOREIGN TABLE). Treat as advisory unless moved into authoritative specs.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Conformance Checklist (Defer to Authoritative Specs)
Captured for cross-reference only; verify in authoritative DDL/FDW specs (if any):

[ ] CREATE/ALTER/DROP FOREIGN DATA WRAPPER syntax and options.
[ ] CREATE/ALTER/DROP SERVER syntax and options.
[ ] CREATE/ALTER/DROP USER MAPPING syntax and options.
[ ] CREATE/ALTER/DROP FOREIGN TABLE syntax and options.
[ ] sys.* passthrough routines linkage to UDR connectors (remote_exec/remote_call/remote_query).

## Notes
- If FDW support is required for V3, it must be included in the authoritative inventory and aligned with catalog and execution specs.
