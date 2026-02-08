# V3 Parser: DROP / TRUNCATE (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the authoritative DROP and TRUNCATE statement forms for the ScratchBird
V3 parser and how each form MUST emit SBLR V3 opcodes.

## Scope

Covers DROP for:
- DATABASE, SCHEMA, TABLE, INDEX, VIEW
- SEQUENCE, DOMAIN, TYPE, FUNCTION, PROCEDURE, PACKAGE
- TRIGGER, POLICY, TABLESPACE, ROLE, USER, GROUP
- SERVER, FOREIGN TABLE, USER MAPPING, SYNONYM
- JOB, EXTENSION, EXCEPTION

Covers TRUNCATE for base and partitioned tables.

## Parsing Rules (Authoritative)

1. Enter DDL parse mode.
2. Identify target object kind and name list.
3. Parse CASCADE / RESTRICT and behavior flags.
4. Emit `SBLR3_DDL_DROP` or `SBLR3_DDL_TRUNCATE` with typed payload.

## Emission Rules (Authoritative)

### DROP

Emit:
- `SBLR3_DDL_DROP` with payload `DDL_DROP_OBJECT`
- For multi-drop statements, emit one payload entry per object in order.

Flags:
- `if_exists`
- `cascade`

### TRUNCATE

Emit:
- `SBLR3_DDL_TRUNCATE` with payload `DDL_TRUNCATE`

Flags:
- `restart_identity`
- `cascade` (truncate cascades only if explicitly requested)

## Statement Families

Supported DROP object kinds:
- DATABASE, SCHEMA, TABLE, INDEX, VIEW
- SEQUENCE, DOMAIN, TYPE
- FUNCTION, PROCEDURE, PACKAGE
- TRIGGER, POLICY
- TABLESPACE
- ROLE, USER, GROUP
- SERVER, FOREIGN TABLE, USER MAPPING
- SYNONYM
- JOB, EXTENSION, EXCEPTION

Supported TRUNCATE forms:
- `TRUNCATE TABLE <name_list> [RESTART IDENTITY] [CASCADE]`

## Errors

- Unknown object type: `ERR_DDL_UNSUPPORTED_OBJECT`.
- Drop denied by dependency when `RESTRICT`: `ERR_DEPENDENCY_EXISTS`.
- TRUNCATE on view or system table: `ERR_INVALID_OPERATION`.

## Related Specs

- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`
- `DDL_CREATE.md`
- `DDL_ALTER.md`
