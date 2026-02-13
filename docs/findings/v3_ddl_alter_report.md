# DDL ALTER - V3 Findings

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DDL_ALTER.md`

Status: **Partially implemented**. Core ALTER TABLE actions exist with SBLR emission, but many ALTER families and required emission rules are missing or divergent from the spec.

## Spec-Level Gaps (High Impact)
- Spec mandates `SBLR3_DDL_ALTER` + `SBLR3_DDL_ALTER_ACTION` payloads for all ALTER statements. Implementation instead emits specialized opcodes (`SBLR3_ALTER_TABLE`, `SBLR3_ALTER_SCHEMA`, `SBLR3_RENAME_OBJECT`, etc.).
  - Evidence: `src/parser/v3_emitter.cpp:1294-1905`, `src/sblr/v3_payload_map.generated.cpp:7-21`.
- Many ALTER families in the spec are not parsed (VIEW options, INDEX reset/rebuild, SEQUENCE settings, FUNCTION/PROCEDURE changes, ROLE/USER/GROUP settings, SERVER/FOREIGN TABLE/USER MAPPING options, SYNONYM target, JOB owner/command, SYSTEM RESET, etc.).
- Error codes (`ERR_DDL_UNSUPPORTED_OBJECT`, `ERR_DDL_UNSUPPORTED_ACTION`, `ERR_PERMISSION_DENIED`) are not present in parser error handling; parser currently emits generic error messages.

## Detailed Checklist

### ALTER DATABASE
Spec actions: SET OPTION_KV list, RENAME TO, SET DEFAULT TABLESPACE.
- [~] RENAME TO supported via `parseAlterDatabase`.
  - Evidence: `src/parser/parser_v3.cpp:4386-4433`.
- [ ] SET OPTION_KV list (missing).
- [ ] SET DEFAULT TABLESPACE (missing).
- [ ] Uses `SBLR3_DDL_ALTER` + actions (missing; emits `SBLR3_ALTER_DATABASE`).
  - Evidence: `src/parser/v3_emitter.cpp:1706-1732`.

### ALTER SCHEMA
Spec actions: RENAME TO, SET AUTHORIZATION.
- [~] RENAME TO supported (`parseAlterSchema`).
  - Evidence: `src/parser/parser_v3.cpp:4450-4483`.
- [ ] SET AUTHORIZATION (missing; parser supports OWNER TO / SET PATH instead).
- [ ] Uses `SBLR3_DDL_ALTER` + actions (missing; emits `SBLR3_ALTER_SCHEMA`).
  - Evidence: `src/parser/v3_emitter.cpp:1692-1705`.

### ALTER TABLE
Spec actions: ADD COLUMN, ADD CONSTRAINT, DROP COLUMN [CASCADE|RESTRICT], DROP CONSTRAINT [CASCADE|RESTRICT], ALTER COLUMN SET/DROP DEFAULT, SET/DROP NOT NULL, SET DATA TYPE, SET STATISTICS, SET STORAGE, SET POSITION, RENAME COLUMN, RENAME CONSTRAINT, RENAME TO, SET TABLESPACE, SET SCHEMA, ATTACH/DETACH PARTITION, INHERIT/NO INHERIT, ENABLE/DISABLE TRIGGER [ALL|name], ENABLE/DISABLE/FORCE/NO FORCE RLS, VALIDATE CONSTRAINT.
- [*] ADD COLUMN, ADD CONSTRAINT (supported).
  - Evidence: `src/parser/parser_v3.cpp:5251-5282`.
- [~] DROP COLUMN / DROP CONSTRAINT with CASCADE supported; RESTRICT not parsed explicitly.
  - Evidence: `src/parser/parser_v3.cpp:5284-5301`.
- [*] ALTER COLUMN SET/DROP DEFAULT/NOT NULL, SET DATA TYPE/TYPE, SET STATISTICS, SET STORAGE, SET POSITION supported.
  - Evidence: `src/parser/parser_v3.cpp:5303-5377`.
- [*] RENAME COLUMN/CONSTRAINT/TABLE supported.
  - Evidence: `src/parser/parser_v3.cpp:5409-5430`.
- [*] SET TABLESPACE, SET SCHEMA supported.
  - Evidence: `src/parser/parser_v3.cpp:5432-5438`.
- [*] ATTACH/DETACH PARTITION supported.
  - Evidence: `src/parser/parser_v3.cpp:5381-5408`.
- [*] INHERIT / NO INHERIT supported.
  - Evidence: `src/parser/parser_v3.cpp:5439-5452`.
- [*] ENABLE/DISABLE TRIGGER (ALL/name) supported.
  - Evidence: `src/parser/parser_v3.cpp:5453-5476`.
- [*] ENABLE/DISABLE/FORCE/NO FORCE RLS supported.
  - Evidence: `src/parser/parser_v3.cpp:5459-5494`.
- [*] VALIDATE CONSTRAINT supported.
  - Evidence: `src/parser/parser_v3.cpp:5496-5501`.
- [~] Emission uses `SBLR3_ALTER_TABLE` and a single action payload. Spec expects `SBLR3_DDL_ALTER` + action list.
  - Evidence: `src/parser/v3_emitter.cpp:1294-1654`.

### ALTER INDEX
Spec actions: RENAME TO, SET TABLESPACE, SET OPTION, RESET OPTION, SET STORAGE PARAMETERS, SET STATISTICS, REBUILD/REINDEX.
- [~] RENAME TO supported.
  - Evidence: `src/parser/parser_v3.cpp:4317-4343`.
- [~] SET OPTION (limited: only bloom filter options) supported.
  - Evidence: `src/parser/parser_v3.cpp:4345-4429`.
- [ ] SET TABLESPACE (missing).
- [ ] RESET OPTION (missing).
- [ ] SET STORAGE PARAMETERS (missing).
- [ ] SET STATISTICS (missing).
- [ ] REBUILD/REINDEX (missing).

### ALTER VIEW
Spec actions: RENAME TO, SET SCHEMA, SET OPTION.
- [~] RENAME TO / SET SCHEMA supported via rename/move.
  - Evidence: `src/parser/parser_v3.cpp:4307-4323`.
- [ ] SET OPTION (missing).

### ALTER SEQUENCE
Spec actions: SET INCREMENT, MINVALUE/MAXVALUE, START WITH, CACHE, CYCLE/NO CYCLE, RENAME TO.
- [ ] No ALTER SEQUENCE parsing for these actions; only rename/move supported.
  - Evidence: `src/parser/parser_v3.cpp:4420-4423`.

### ALTER DOMAIN
Spec actions: SET/DROP DEFAULT, SET/DROP NOT NULL, ADD/DROP CHECK, RENAME TO.
- [*] SET/DROP DEFAULT, ADD CHECK, DROP CONSTRAINT, RENAME TO supported.
  - Evidence: `src/parser/parser_v3.cpp:4705-4763`.
- [ ] SET/DROP NOT NULL (missing).

### ALTER TYPE
Spec actions: ADD ATTRIBUTE, DROP ATTRIBUTE, RENAME ATTRIBUTE, SET OPTION, RENAME TO.
- [ ] Spec actions not implemented. Parser handles enum/range/base type options, ADD VALUE, RENAME VALUE, FINALIZE (different feature set).
  - Evidence: `src/parser/parser_v3.cpp:4485-4702`.

### ALTER FUNCTION / PROCEDURE
Spec actions: SET DEFINER/SECURITY, SET COST/ROWS, RENAME TO, SET SCHEMA, REPLACE BODY.
- [ ] Only rename/move supported; no function/procedure alteration options.
  - Evidence: `src/parser/parser_v3.cpp:4399-4413`.

### ALTER PACKAGE
Spec actions: RENAME TO, SET SCHEMA, REPLACE BODY.
- [ ] Only rename/move supported; no REPLACE BODY.
  - Evidence: `src/parser/parser_v3.cpp:4412-4414`.

### ALTER TRIGGER
Spec actions: ENABLE/DISABLE, SET ORDER, RENAME TO, SET TABLE.
- [ ] Only rename/move supported; no enable/disable/order/table.
  - Evidence: `src/parser/parser_v3.cpp:4408-4411`.

### ALTER POLICY
Spec actions: RENAME TO, SET USING, SET CHECK, SET ROLE, ENABLE/DISABLE.
- [~] USING / WITH CHECK / TO roles supported.
  - Evidence: `src/parser/parser_v3.cpp:4765-4820`.
- [ ] RENAME TO (missing).
- [ ] ENABLE/DISABLE (missing).

### ALTER TABLESPACE
Spec actions: RENAME TO, SET LOCATION, SET OPTION.
- [~] RENAME TO supported (within alter tablespace). Autoextend/maxsize actions supported (not in spec).
  - Evidence: `src/parser/parser_v3.cpp:3042-3103`.
- [ ] SET LOCATION (missing).
- [ ] SET OPTION (missing, and current options differ from spec).

### ALTER ROLE / USER / GROUP
Spec actions: RENAME TO, SET PASSWORD, SET OPTIONS, SET DEFAULT ROLE, SET LOGIN/NOLOGIN.
- [ ] Only rename/move supported; no auth/login options.
  - Evidence: `src/parser/parser_v3.cpp:4414-4419`.

### ALTER SERVER / FOREIGN TABLE / USER MAPPING
Spec actions: SET OPTIONS, RENAME TO, SET OWNER.
- [ ] Only rename/move supported; no SET OPTIONS/SET OWNER.
  - Evidence: `src/parser/parser_v3.cpp:4419-4426`.

### ALTER SYNONYM
Spec actions: RENAME TO, SET TARGET.
- [ ] Only rename/move supported; no SET TARGET.
  - Evidence: `src/parser/parser_v3.cpp:4418`.

### ALTER JOB
Spec actions: ENABLE/DISABLE, SET SCHEDULE, SET COMMAND, SET OWNER, RENAME TO.
- [~] Extensive ALTER JOB parser exists (schedule, state, retries, job body, partitioning, etc.).
  - Evidence: `src/parser/parser_v3.cpp:4822-5190`.
- [ ] SET OWNER and RENAME TO are not parsed in ALTER JOB.

### ALTER SYSTEM
Spec actions: SET <option>, RESET <option>.
- [~] SET key = expression supported.
  - Evidence: `src/parser/parser_v3.cpp:5191-5220`.
- [ ] RESET <option> missing.

## Emission Notes
- Alter statements emit specialized opcodes (`SBLR3_ALTER_*`, `SBLR3_RENAME_OBJECT`, `SBLR3_MOVE_OBJECT`, `SBLR3_ALTER_TABLE_SET_TABLESPACE`), not the generic `SBLR3_DDL_ALTER` + action list required by the spec.
  - Evidence: `src/parser/v3_emitter.cpp:1294-1905`, `src/sblr/v3_payload_map.generated.cpp:7-21`.

