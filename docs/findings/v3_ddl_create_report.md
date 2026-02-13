# DDL CREATE - V3 Findings

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DDL_CREATE.md`

Status: **Partially implemented**. Many CREATE forms are parsed and emitted, but emission does not use `SBLR3_DDL_CREATE` as required by the spec; some statement families and option handling are missing or incomplete.

## Spec-Level Gaps (High Impact)
- Spec mandates a single opcode `SBLR3_DDL_CREATE` with typed payloads. Implementation emits specialized opcodes (`SBLR3_CREATE_TABLE`, `SBLR3_CREATE_INDEX`, etc.) and `SBLR3_DDL_CREATE` does not appear in the codebase.
  - Evidence: `src/parser/v3_emitter.cpp:749-1226`, `src/sblr/v3_payload_map.generated.cpp:32-56`.
- `CREATE EXTENSION` is in the spec but is not parsed or emitted.
- Spec error codes `ERR_DDL_UNSUPPORTED_OBJECT`, `ERR_DDL_UNSUPPORTED_OPTION`, `ERR_OBJECT_EXISTS` not mapped in parser errors (parser uses generic error messages).

## Detailed Checklist

### CREATE DATABASE
- [~] Parsed (`parseCreateDatabase`) and emitted via `SBLR3_CREATE_DATABASE`.
  - Evidence: `src/parser/parser_v3.cpp:623-631`, `src/parser/v3_emitter.cpp:862-877`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE SCHEMA
- [~] Parsed and emitted via `SBLR3_CREATE_SCHEMA`.
  - Evidence: `src/parser/parser_v3.cpp:617-621`, `src/parser/v3_emitter.cpp:849-861`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE TABLE
- [~] Parsed and emitted via `SBLR3_CREATE_TABLE`.
  - Evidence: `src/parser/parser_v3.cpp:643-658`, `src/parser/v3_emitter.cpp:739-769`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).
- [~] CREATE OR REPLACE is parsed but only surfaces as `or_replace` in AST; emitter does not set `flags.create_or_replace` in payload (spec requires it).
  - Evidence: `src/parser/parser_v3.cpp:558-586`, `src/parser/v3_emitter.cpp:742-768`.

### CREATE INDEX
- [~] Parsed and emitted via `SBLR3_CREATE_INDEX`.
  - Evidence: `src/parser/parser_v3.cpp:656-666`, `src/parser/v3_emitter.cpp:771-815`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).
- [~] UNIQUE keyword handled; CREATE OR REPLACE not applicable.

### CREATE VIEW
- [~] Parsed and emitted via `SBLR3_CREATE_VIEW`.
  - Evidence: `src/parser/parser_v3.cpp:668-679`, `src/parser/v3_emitter.cpp:817-831`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).
- [~] CREATE OR REPLACE parsed but not emitted as `flags.create_or_replace`.

### CREATE SEQUENCE
- [~] Parsed and emitted via `SBLR3_CREATE_SEQUENCE`.
  - Evidence: `src/parser/parser_v3.cpp:681-692`, `src/parser/v3_emitter.cpp:832-848`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE DOMAIN
- [~] Parsed and emitted via `SBLR3_CREATE_DOMAIN`.
  - Evidence: `src/parser/parser_v3.cpp:638-641`, `src/parser/v3_emitter.cpp:1215-1224`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE TYPE
- [~] Parsed and emitted via `SBLR3_CREATE_TYPE` (payload is minimal).
  - Evidence: `src/parser/parser_v3.cpp:721-724`, `src/parser/v3_emitter.cpp:1226-1245`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE FUNCTION / PROCEDURE
- [~] Parsed and emitted via `SBLR3_CREATE_FUNCTION_STMT` / `SBLR3_CREATE_PROCEDURE_STMT`.
  - Evidence: `src/parser/parser_v3.cpp:693-701`, `src/parser/v3_emitter.cpp:894-956`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE PACKAGE
- [~] Parsed and emitted via `SBLR3_CREATE_PACKAGE_STMT`.
  - Evidence: `src/parser/parser_v3.cpp:709-714`, `src/parser/v3_emitter.cpp:975-992`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE TRIGGER
- [~] Parsed and emitted via `SBLR3_CREATE_TRIGGER`.
  - Evidence: `src/parser/parser_v3.cpp:702-707`, `src/parser/v3_emitter.cpp:957-974`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE POLICY
- [~] Parsed and emitted via `SBLR3_CREATE_POLICY`.
  - Evidence: `src/parser/parser_v3.cpp:749-751`, `src/parser/v3_emitter.cpp:1035-1047`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE TABLESPACE
- [~] Parsed and emitted via `SBLR3_CREATE_TABLESPACE`.
  - Evidence: `src/parser/parser_v3.cpp:632-634`, `src/parser/v3_emitter.cpp:878-893`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE ROLE / USER / GROUP
- [~] Parsed and emitted via `SBLR3_CREATE_ROLE` / `SBLR3_CREATE_USER` / `SBLR3_CREATE_GROUP`.
  - Evidence: `src/parser/parser_v3.cpp:732-745`, `src/parser/v3_emitter.cpp:993-1022`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE SERVER / FOREIGN TABLE / USER MAPPING
- [~] Parsed and emitted via `SBLR3_CREATE_FOREIGN_SERVER`, `SBLR3_CREATE_FOREIGN_TABLE`, `SBLR3_CREATE_USER_MAPPING`.
  - Evidence: `src/parser/parser_v3.cpp:757-764`, `src/parser/v3_emitter.cpp:1049-1121`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE SYNONYM
- [~] Parsed and emitted via `SBLR3_CREATE_SYNONYM`.
  - Evidence: `src/parser/parser_v3.cpp:781-790`, `src/parser/v3_emitter.cpp:1140-1149`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE JOB
- [~] Parsed and emitted via `SBLR3_CREATE_JOB`.
  - Evidence: `src/parser/parser_v3.cpp:802-807`, `src/parser/v3_emitter.cpp:1167-1199`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

### CREATE EXTENSION
- [ ] Not parsed or emitted.
  - Evidence: no `EXTENSION` handling in `src/parser/parser_v3.cpp`.

### CREATE EXCEPTION
- [~] Parsed and emitted via `SBLR3_CREATE_EXCEPTION_STMT`.
  - Evidence: `src/parser/parser_v3.cpp:715-719`, `src/parser/v3_emitter.cpp:1200-1213`.
- [ ] Emission must use `SBLR3_DDL_CREATE` payload (missing).

## Error Codes
- [ ] Spec errors `ERR_DDL_UNSUPPORTED_OBJECT`, `ERR_DDL_UNSUPPORTED_OPTION`, `ERR_OBJECT_EXISTS` not implemented (parser uses generic errors).
  - Evidence: no matches for these errors in `src/`.

