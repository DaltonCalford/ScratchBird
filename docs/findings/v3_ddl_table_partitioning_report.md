# V3 Table Partitioning Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_TABLE_PARTITIONING.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 parser recognizes `PARTITION BY` in `CREATE TABLE` and `ALTER TABLE ... ATTACH/DETACH PARTITION`, but the **V3 emitter/executor do not persist partitioning metadata or implement routing**.
- `CREATE TABLE ... PARTITION OF ... FOR VALUES ...` and `DEFAULT` partition syntax are **not parsed** in V3.
- DML routing, migration semantics, and partition error codes are present only in legacy (non-V3) executor paths and are **not wired for V3**.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### Partitioned Table Definition
[~] Parser supports `CREATE TABLE ... PARTITION BY {RANGE|LIST|HASH} (...)` and stores `partition_by` + `partition_columns` in AST.
[ ] V3 emitter does not serialize `partition_by` / `partition_columns` into `SBLR3_CREATE_TABLE` payload (schema supports `partitioning` but is unused).
[ ] V3 executor `handleCreateTable` ignores partitioning fields entirely and does not persist partition strategy/columns metadata.

### Creating Child Partitions
[ ] `CREATE TABLE child PARTITION OF parent FOR VALUES ...` is not parsed in V3.
[~] `ALTER TABLE parent ATTACH PARTITION child FOR VALUES ...` is parsed and emitted in V3.
[ ] V3 `ATTACH PARTITION` stores bounds as a raw string (no validation of range/list/default syntax, no constraint validation).
[~] `ALTER TABLE parent DETACH PARTITION child` is parsed/emitted and updates metadata only.

### DML Routing and Migration
[ ] V3 path does not implement INSERT/UPDATE/DELETE routing based on partition keys or partition pruning.
[ ] Error conditions (`PARTITION_NOT_FOUND`, `PARTITION_AMBIGUOUS`, `PARTITION_CONSTRAINT_VIOLATION`) are not surfaced in V3.
[~] Legacy executor (non-V3 opcodes) contains partition metadata and routing logic, but V3 create/attach paths do not populate metadata for it.

### Dropping Partitions
[ ] V3 `DROP TABLE` does not check if a table is still attached as a partition; spec expects drop to fail if still attached.

## Key References
- Parser `PARTITION BY` in CREATE TABLE: `src/parser/parser_v3.cpp:1286-1347`
- AST partition fields: `include/scratchbird/parser/ast_v3.h:654-673`
- V3 emitter `CREATE TABLE` (no partition payload): `src/parser/v3_emitter.cpp:735-820`
- V3 executor `handleCreateTable`: `src/sblr/executor.cpp:41176-41320`
- V3 ALTER TABLE attach/detach: parser `src/parser/parser_v3.cpp:5470-5585`, emitter `src/parser/v3_emitter.cpp:1607-1626`, executor `src/sblr/executor.cpp:42490-42670`
- Legacy partition routing logic (non-V3): `src/sblr/executor.cpp:15382-15850`
