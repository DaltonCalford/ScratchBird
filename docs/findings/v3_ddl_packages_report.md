# V3 DDL Packages Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_PACKAGES.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 supports CREATE PACKAGE and CREATE PACKAGE BODY via raw text capture; DROP PACKAGE implemented. No ALTER PACKAGE COMPILE and no DROP PACKAGE BODY handling.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE PACKAGE / CREATE PACKAGE BODY
[~] Parser supports `CREATE [OR REPLACE] PACKAGE` and `CREATE [OR REPLACE] PACKAGE BODY` with body capture.
[~] AST captures `is_body`, `header`, `body` as raw text.
[~] Emitter sends `spec` and `body` bytes to `SBLR3_CREATE_PACKAGE_STMT`.
[~] Executor stores package header/body and supports OR REPLACE by drop+create.

### ALTER PACKAGE
[ ] Not implemented (no parser/emitter/executor support for COMPILE SPEC/BODY).

### DROP PACKAGE
[~] Parser supports DROP PACKAGE with IF EXISTS. No `DROP PACKAGE BODY` keyword support.
[~] Executor drops entire package; no body-only drop.

## Notes
- If DROP PACKAGE BODY and ALTER PACKAGE COMPILE are required, AST/emitter/executor must be extended.
