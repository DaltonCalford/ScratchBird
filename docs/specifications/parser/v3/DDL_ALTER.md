# V3 Parser: ALTER (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the authoritative ALTER statement forms for the ScratchBird V3 parser and
how each form MUST emit SBLR V3 opcodes. The engine never parses SQL.

## Scope

Covers all ALTER variants for:
- DATABASE, SCHEMA, TABLE, INDEX, VIEW
- SEQUENCE, DOMAIN, TYPE, FUNCTION, PROCEDURE, PACKAGE
- TRIGGER, POLICY, TABLESPACE, ROLE, USER, GROUP
- SERVER, FOREIGN TABLE, USER MAPPING, SYNONYM
- JOB, SYSTEM

## Parsing Rules (Authoritative)

1. Enter DDL parse mode.
2. Identify the target object kind and object name.
3. Parse the action list (single action or action list).
4. Normalize identifiers to canonical case per V3 naming rules.
5. Emit DDL opcodes using `SBLR_V3_OPCODE_PAYLOADS.md` schemas.

## Emission Rules (Authoritative)

All ALTER statements MUST emit:
- `SBLR3_DDL_ALTER` with a typed payload for the object kind.
- Zero or more `SBLR3_DDL_ALTER_ACTION` payloads as ordered actions.

Action ordering MUST preserve user order and is semantically significant.

## Statement Families

### ALTER DATABASE

Allowed actions:
- SET `OPTION_KV` list
- RENAME TO <name>
- SET DEFAULT TABLESPACE

SBLR payload:
- `DDL_ALTER_DATABASE` with `ALTER_ACTION_SET_OPTION` and `ALTER_ACTION_RENAME`

### ALTER SCHEMA

Allowed actions:
- RENAME TO <name>
- SET AUTHORIZATION <role>

SBLR payload:
- `DDL_ALTER_SCHEMA`

### ALTER TABLE

Allowed actions:
- ADD COLUMN
- ADD CONSTRAINT
- DROP COLUMN [CASCADE|RESTRICT]
- DROP CONSTRAINT [CASCADE|RESTRICT]
- ALTER COLUMN SET/DROP DEFAULT
- ALTER COLUMN SET/DROP NOT NULL
- ALTER COLUMN SET DATA TYPE
- ALTER COLUMN SET STATISTICS
- ALTER COLUMN SET STORAGE
- ALTER COLUMN SET POSITION
- RENAME COLUMN
- RENAME CONSTRAINT
- RENAME TO (table)
- SET TABLESPACE
- SET SCHEMA
- ATTACH PARTITION
- DETACH PARTITION
- INHERIT / NO INHERIT
- ENABLE/DISABLE TRIGGER [ALL|name]
- ENABLE/DISABLE/FORCE/NO FORCE ROW LEVEL SECURITY
- VALIDATE CONSTRAINT

SBLR payload:
- `DDL_ALTER_TABLE` + `ALTER_TABLE_ACTION` payload per action

### ALTER INDEX

Allowed actions:
- RENAME TO
- SET TABLESPACE
- SET OPTION <option>
- RESET OPTION <option>
- SET STORAGE PARAMETERS
- SET STATISTICS
- REBUILD / REINDEX

SBLR payload:
- `DDL_ALTER_INDEX`

### ALTER VIEW

Allowed actions:
- RENAME TO
- SET SCHEMA
- SET OPTION <option>

SBLR payload:
- `DDL_ALTER_VIEW`

### ALTER SEQUENCE

Allowed actions:
- SET INCREMENT
- SET MINVALUE / MAXVALUE
- SET START WITH
- SET CACHE
- SET CYCLE / NO CYCLE
- RENAME TO

SBLR payload:
- `DDL_ALTER_SEQUENCE`

### ALTER DOMAIN

Allowed actions:
- SET DEFAULT
- DROP DEFAULT
- SET NOT NULL
- DROP NOT NULL
- ADD CHECK
- DROP CHECK
- RENAME TO

SBLR payload:
- `DDL_ALTER_DOMAIN`

### ALTER TYPE

Allowed actions:
- ADD ATTRIBUTE
- DROP ATTRIBUTE
- RENAME ATTRIBUTE
- SET OPTION
- RENAME TO

SBLR payload:
- `DDL_ALTER_TYPE`

### ALTER FUNCTION / PROCEDURE

Allowed actions:
- SET DEFINER/SECURITY
- SET COST/ROWS (if supported)
- RENAME TO
- SET SCHEMA
- REPLACE BODY (if CREATE OR REPLACE used)

SBLR payload:
- `DDL_ALTER_FUNCTION`, `DDL_ALTER_PROCEDURE`

### ALTER PACKAGE

Allowed actions:
- RENAME TO
- SET SCHEMA
- REPLACE BODY

SBLR payload:
- `DDL_ALTER_PACKAGE`

### ALTER TRIGGER

Allowed actions:
- ENABLE / DISABLE
- SET ORDER
- RENAME TO
- SET TABLE

SBLR payload:
- `DDL_ALTER_TRIGGER`

### ALTER POLICY

Allowed actions:
- RENAME TO
- SET USING
- SET CHECK
- SET ROLE
- ENABLE / DISABLE

SBLR payload:
- `DDL_ALTER_POLICY`

### ALTER TABLESPACE

Allowed actions:
- RENAME TO
- SET LOCATION
- SET OPTION

SBLR payload:
- `DDL_ALTER_TABLESPACE`

### ALTER ROLE / USER / GROUP

Allowed actions:
- RENAME TO
- SET PASSWORD
- SET OPTIONS
- SET DEFAULT ROLE
- SET LOGIN/NOLOGIN

SBLR payload:
- `DDL_ALTER_ROLE`, `DDL_ALTER_USER`, `DDL_ALTER_GROUP`

### ALTER SERVER / FOREIGN TABLE / USER MAPPING

Allowed actions:
- SET OPTIONS
- RENAME TO
- SET OWNER

SBLR payload:
- `DDL_ALTER_SERVER`, `DDL_ALTER_FOREIGN_TABLE`, `DDL_ALTER_USER_MAPPING`

### ALTER SYNONYM

Allowed actions:
- RENAME TO
- SET TARGET

SBLR payload:
- `DDL_ALTER_SYNONYM`

### ALTER JOB

Allowed actions:
- ENABLE / DISABLE
- SET SCHEDULE
- SET COMMAND
- SET OWNER
- RENAME TO

SBLR payload:
- `DDL_ALTER_JOB`

### ALTER SYSTEM

Allowed actions:
- SET <option>
- RESET <option>

SBLR payload:
- `DDL_ALTER_SYSTEM`

## Errors

- Unknown object type: `ERR_DDL_UNSUPPORTED_OBJECT`.
- Unsupported action: `ERR_DDL_UNSUPPORTED_ACTION`.
- Attempt to alter system objects without privilege: `ERR_PERMISSION_DENIED`.

## Related Specs

- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`
- `DDL_CREATE.md`
- `DDL_DROP_TRUNCATE.md`
