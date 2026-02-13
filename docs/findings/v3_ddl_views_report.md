# V3 DDL Views Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_VIEWS.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 parser supports a basic `CREATE VIEW` with column list and `WITH CHECK OPTION`, but **emitter/executor do not implement V3 view creation**.
- `ALTER VIEW`, `DROP VIEW` (V3), `CREATE MATERIALIZED VIEW`, and `REFRESH MATERIALIZED VIEW` are **not implemented** in the V3 path.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE VIEW
[~] Parser supports OR REPLACE, IF NOT EXISTS, optional column list, and `WITH CHECK OPTION`.
[ ] Parser ordering for `WITH [LOCAL|CASCADED] CHECK OPTION` is limited; it expects `WITH CHECK OPTION` then optionally LOCAL/CASCADED.
[ ] Temporary/materialized flags are captured at CREATE dispatch but not emitted.
[ ] V3 emitter does not serialize check option, temp/materialized, or WITH DATA flags.
[ ] V3 executor has no handler for `SBLR3_CREATE_VIEW`.

### ALTER VIEW
[ ] No AST or parser support for ALTER VIEW (rename/owner/set schema/options).

### DROP VIEW
[~] Parser supports IF EXISTS, multiple views, CASCADE.
[ ] V3 emitter drops only first view and does not serialize flags.
[ ] V3 executor has no handler for `SBLR3_DROP_VIEW`.

### Materialized Views
[ ] `CREATE MATERIALIZED VIEW` parsed only as a flag; no WITH [NO] DATA handling or execution support.
[ ] `REFRESH MATERIALIZED VIEW` not parsed/emitted.

## Key References
- Parser `CREATE VIEW`: `src/parser/parser_v3.cpp:2287-2355`
- AST `CreateViewStmt`: `include/scratchbird/parser/ast_v3.h:744-772`
- V3 emitter `CREATE VIEW` payload: `src/parser/v3_emitter.cpp:780-815`
- V3 executor drop handling supports only table/index: `src/sblr/executor.cpp:41496-41534`
