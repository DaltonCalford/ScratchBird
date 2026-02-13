# DDL DROP / TRUNCATE - V3 Findings

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DDL_DROP_TRUNCATE.md`

Status: **Partially implemented**. DROP/TRUNCATE parsing exists, but emission does not use `SBLR3_DDL_DROP`/`SBLR3_DDL_TRUNCATE`, multi-drop lists emit only the first object, and several object kinds or flags are missing.

## Spec-Level Gaps (High Impact)
- Spec mandates `SBLR3_DDL_DROP` and `SBLR3_DDL_TRUNCATE` opcodes; implementation emits specialized `SBLR3_DROP_*` and `SBLR3_TRUNCATE_TABLE` opcodes.
  - Evidence: `src/parser/v3_emitter.cpp:1988-2105`.
- Multi-drop statements should emit one payload per object; emitter uses only `.front()` for lists, dropping only the first item.
  - Evidence: `src/parser/v3_emitter.cpp:2011-2040` (tables, indexes, views, sequences, schemas, domains, types, functions, procedures, triggers, packages, roles, groups, users, exceptions, foreign tables, synonyms, udrs).
- TRUNCATE flags (`restart_identity`, `cascade`) are parsed but not emitted.
  - Evidence: `src/parser/parser_v3.cpp:6056-6080`, `src/parser/v3_emitter.cpp:2095-2105`.
- Spec error codes (`ERR_DDL_UNSUPPORTED_OBJECT`, `ERR_DEPENDENCY_EXISTS`, `ERR_INVALID_OPERATION`) are not mapped in parser errors.

## Detailed Checklist

### Supported DROP object kinds (per spec)
- [~] DATABASE, SCHEMA, TABLE, INDEX, VIEW, SEQUENCE, DOMAIN, TYPE, FUNCTION, PROCEDURE, PACKAGE, TRIGGER, POLICY, TABLESPACE, ROLE, GROUP, SERVER, FOREIGN TABLE, USER MAPPING, SYNONYM, JOB, EXCEPTION, UDR are parsed.
  - Evidence: `src/parser/parser_v3.cpp:5489-5565`, `src/parser/parser_v3.cpp:5570-6034`.
- [ ] USER drop is missing in v3 parser (only DROP USER MAPPING is supported).
  - Evidence: `src/parser/parser_v3.cpp:5512-5519` (errors for DROP USER).
- [ ] EXTENSION drop is missing.
  - Evidence: no EXTENSION handling in `parseDrop`.

### DROP flags
- [~] IF EXISTS supported for most kinds.
- [~] CASCADE/RESTRICT parsed for some kinds, but behavior is inconsistent (e.g., DROP DOMAIN explicitly rejects CASCADE).
  - Evidence: `src/parser/parser_v3.cpp:5610-5626`.

### TRUNCATE TABLE
- [~] `TRUNCATE TABLE <name_list>` parsed; `RESTART IDENTITY`, `CONTINUE IDENTITY`, `CASCADE` accepted.
  - Evidence: `src/parser/parser_v3.cpp:6050-6080`.
- [ ] `CONTINUE IDENTITY` is not in spec (extra behavior).
- [ ] Emission does not carry flags for RESTART IDENTITY or CASCADE.
  - Evidence: `src/parser/v3_emitter.cpp:2095-2105`.
- [ ] Spec error `ERR_INVALID_OPERATION` for truncating views/system tables is not enforced in parser.

### Emission compliance
- [ ] `SBLR3_DDL_DROP` payload emission missing.
- [ ] `SBLR3_DDL_TRUNCATE` payload emission missing.
- [ ] Multi-drop list emission missing (only first object emitted).

