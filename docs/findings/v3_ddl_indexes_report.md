# V3 DDL Indexes Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_INDEXES.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 supports a rich CREATE INDEX syntax (unique, concurrently, index types, expression/partial, include, tablespace, options), but **emitter does not serialize** several critical flags/options and **executor expects a different wire shape**.
- ALTER INDEX in V3 is limited to ACTIVE/INACTIVE/SET options; REINDEX not implemented.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE INDEX
[~] Parser supports: UNIQUE, CONCURRENTLY, IF NOT EXISTS, USING method, expression keys, INCLUDE, WHERE predicate, TABLESPACE, WITH (BLOOM_FILTER/BLOOM_FPR).
[~] AST contains fields for unique/concurrent/if_not_exists, index_type, columns/expr, include, where, tablespace, options.
[ ] Emitter does **not** serialize many fields:
    - `unique`, `concurrent`, `tablespace`, `options` (bloom), and flags are not emitted.
    - `index_name` is treated as required but optional in parser; emitter uses `index_name` in unqualified path even if empty.
    - Predicate/expression payloads are emitted, but executor expects serialized expression bytes + flags in a different format.
[ ] Executor `executeCreateIndex` expects a custom bytecode layout (name, table, unique flag, columns, include, tablespace, index_type byte, options flags, expression/predicate blobs). This does not match current V3 emitter payload schema.

### ALTER INDEX
[~] Parser only supports `ALTER INDEX ... SET (...)` for bloom options via `SET` options and generic RENAME/SET SCHEMA via ALTER RENAME/MOVE.
[~] Executor supports ACTIVE/INACTIVE/SET options (bloom) but no tablespace/attach partition per spec.

### DROP INDEX
[~] Parser supports IF EXISTS and list of names; no CONCURRENTLY/CASCADE semantics.
[~] Executor drops by name; no CASCADE/RESTRICT handling.

### REINDEX
[ ] Not implemented in parser/emitter/executor.

## Notes
- There is a **wire mismatch** between V3 emitter schema (`SBLR3_CREATE_INDEX` payload) and executor’s expected byte layout. This likely breaks expression/partial indexes and flags.
- If these features are required, align emitter with executor or update executor to consume schema payload.
