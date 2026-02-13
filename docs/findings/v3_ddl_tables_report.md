# V3 DDL Tables Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_TABLES.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 parses core `CREATE TABLE`, `ALTER TABLE`, `DROP TABLE`, and `TRUNCATE`, but **storage parameters, temp table semantics, CTAS options, and identity handling are not implemented end-to-end**.
- `CREATE TABLE` **constraints and many column constraints are dropped** in emission/execution (PK/UNIQUE/FK). Column check constraints are also mismatched with the V3 schema.
- `DROP TABLE` and `TRUNCATE TABLE` lose important flags (IF EXISTS, CASCADE/RESTRICT, RESTART/CONTINUE IDENTITY) in the emitter/executor.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE TABLE
[~] Parser supports IF NOT EXISTS, column definitions, table constraints, TABLESPACE, INHERITS, PARTITION BY, ON COMMIT, and CTAS via `AS SELECT` (no WITH DATA/NO DATA).
[ ] Table/column storage parameters (`WITH (...)`, `ALTER TABLE SET (...)`, column `WITH (...)`) are not parsed or represented in AST (no storage parameter structs/fields).
[ ] Temporary table semantics: parser records `temp_type` and `on_commit`, but emitter/executor ignore temp flags and on-commit behavior.
[ ] CTAS: parser sets `as_query`, but emitter never emits `SBLR3_CREATE_TABLE_AS`; executor has no V3 CTAS handling.
[ ] Column `GENERATED ... AS IDENTITY` parsed (`generated_as_identity`), but emitter never serializes identity specs (`IDENTITY_SPEC`) and executor does not handle identity on create.
[ ] Column `GENERATED ALWAYS AS (expr) STORED`: parser accepts expression, but there is no STORED/VIRTUAL flag parsing; emitter maps `generated_always` to stored flag, which is not semantically equivalent.
[ ] Column constraints PRIMARY KEY, UNIQUE, REFERENCES are parsed but **not emitted** in `emitColumnDef`; they are dropped in `CREATE TABLE`.
[ ] Table constraints are emitted in payload, but V3 executor `handleCreateTable` ignores table constraints entirely.
[ ] Column CHECK constraints are parsed, emitter adds `check_expr`, but V3 schema `COLUMN_DEF` has no `check_expr` field (only `check_count`), and executor expects `check_expr`; likely dropped during encoding.

### ALTER TABLE
[~] Parser supports ADD/DROP COLUMN, ALTER COLUMN type/default/not null, ADD/DROP CONSTRAINT, RENAME TABLE/COLUMN/CONSTRAINT, SET SCHEMA/TABLESPACE, SET STATISTICS/STORAGE, INHERIT/NO INHERIT, ENABLE/DISABLE TRIGGER, ENABLE/DISABLE/FORCE RLS, ATTACH/DETACH PARTITION, VALIDATE CONSTRAINT.
[ ] Spec `ALTER TABLE ... OWNER TO ...` is not parsed.
[ ] Storage parameter variants (`ALTER TABLE SET (...)`, `ALTER TABLE ALTER COLUMN ... SET (...)`) are not parsed; only `SET STORAGE <type>` and `SET STATISTICS` are supported.
[ ] ALTER TABLE ADD COLUMN loses PK/UNIQUE/REFERENCES constraints (not emitted by `emitColumnDef`).

### DROP TABLE
[~] Parser supports IF EXISTS, multiple tables, CASCADE/RESTRICT.
[ ] Emitter drops only the first table and does not encode IF EXISTS/CASCADE/RESTRICT flags.
[ ] Executor always drops by resolved table id; IF EXISTS is not honored, CASCADE/RESTRICT not enforced.

### TRUNCATE TABLE
[~] Parser supports RESTART/CONTINUE IDENTITY and CASCADE.
[ ] Parser does not accept RESTRICT keyword.
[ ] Emitter ignores RESTART/CONTINUE and CASCADE flags; executor truncates tables without identity/cascade semantics.

## Key References
- Parser `CREATE TABLE`/constraints: `src/parser/parser_v3.cpp:1191-1355`, `src/parser/parser_v3.cpp:1500-1730`
- AST `CreateTableStmt` / `ColumnDef`: `include/scratchbird/parser/ast_v3.h:632-676`, `include/scratchbird/parser/ast_v3.h:385-420`
- Emitter `CREATE TABLE` / column defs: `src/parser/v3_emitter.cpp:735-820`, `src/parser/v3_emitter.cpp:4054-4114`
- V3 schema `COLUMN_DEF` / `SCHEMA_DDL_CREATE_TABLE`: `src/sblr/v3_schema.generated.cpp:7-20`, `src/sblr/v3_schema.generated.cpp:248-258`
- Executor `handleCreateTable` / `columnInfoFromV3`: `src/sblr/executor.cpp:41176-41320`, `src/sblr/executor.cpp:40303-40440`
- Truncate emitter/executor: `src/parser/v3_emitter.cpp:2088-2107`, `src/sblr/executor.cpp:41539-41560`
- Drop emitter/executor: `src/parser/v3_emitter.cpp:1978-2045`, `src/sblr/executor.cpp:41496-41534`
