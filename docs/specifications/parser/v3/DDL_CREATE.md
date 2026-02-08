# V3 Parser: CREATE (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the authoritative CREATE statement forms for the ScratchBird V3 parser and
how each form MUST emit SBLR V3 opcodes. The engine never parses SQL.

## Scope

Covers CREATE for:
- DATABASE, SCHEMA, TABLE, INDEX, VIEW
- SEQUENCE, DOMAIN, TYPE, FUNCTION, PROCEDURE, PACKAGE
- TRIGGER, POLICY, TABLESPACE, ROLE, USER, GROUP
- SERVER, FOREIGN TABLE, USER MAPPING, SYNONYM
- JOB, EXTENSION, EXCEPTION

## Parsing Rules (Authoritative)

1. Enter DDL parse mode.
2. Identify CREATE object kind and object name.
3. Parse object definition body.
4. Normalize identifiers to canonical case per V3 naming rules.
5. Emit `SBLR3_DDL_CREATE` with typed payload for the object kind.
6. For objects with subcomponents (columns, constraints, indexes), emit embedded
   payloads in declaration order.

## Emission Rules (Authoritative)

All CREATE statements MUST emit:
- `SBLR3_DDL_CREATE` with a typed payload (see `SBLR_V3_OPCODE_PAYLOADS.md`).

Any `CREATE OR REPLACE` MUST emit `flags.create_or_replace = 1` in the payload.

## Statement Families

### CREATE DATABASE

Defines database-level metadata and default storage settings.
Payload: `DDL_CREATE_DATABASE`

### CREATE SCHEMA

Defines a new schema path and optional authorization.
Payload: `DDL_CREATE_SCHEMA`

### CREATE TABLE

Defines a base or partitioned table.
Payload: `DDL_CREATE_TABLE`
- Columns (`COLUMN_DEF`) emitted in order
- Table constraints (`TABLE_CONSTRAINT`) emitted in order
- Partitioning clause (if present)

### CREATE INDEX

Defines an index on a table or expression list.
Payload: `DDL_CREATE_INDEX`
- Index keys (`INDEX_KEY`) emitted in order
- Type-specific options must match `INDEX_TYPE_SPEC` entries

### CREATE VIEW

Defines a view or materialized view.
Payload: `DDL_CREATE_VIEW`
- Query body is emitted as nested SBLR payload

### CREATE SEQUENCE

Defines sequence parameters.
Payload: `DDL_CREATE_SEQUENCE`

### CREATE DOMAIN

Defines a domain with base type and constraints.
Payload: `DDL_CREATE_DOMAIN`

### CREATE TYPE

Defines a user-defined type (composite or enum).
Payload: `DDL_CREATE_TYPE`

### CREATE FUNCTION / PROCEDURE

Defines a routine.
Payloads: `DDL_CREATE_FUNCTION`, `DDL_CREATE_PROCEDURE`
- Parameters emitted in declared order
- Body is emitted as PSQL SBLR block

### CREATE PACKAGE

Defines a package spec/body.
Payload: `DDL_CREATE_PACKAGE`

### CREATE TRIGGER

Defines a trigger on a table or view.
Payload: `DDL_CREATE_TRIGGER`

### CREATE POLICY

Defines a row-level security policy.
Payload: `DDL_CREATE_POLICY`

### CREATE TABLESPACE

Defines a tablespace and storage parameters.
Payload: `DDL_CREATE_TABLESPACE`

### CREATE ROLE / USER / GROUP

Defines a security principal.
Payloads: `DDL_CREATE_ROLE`, `DDL_CREATE_USER`, `DDL_CREATE_GROUP`

### CREATE SERVER / FOREIGN TABLE / USER MAPPING

Defines external server linkage.
Payloads: `DDL_CREATE_SERVER`, `DDL_CREATE_FOREIGN_TABLE`, `DDL_CREATE_USER_MAPPING`

### CREATE SYNONYM

Defines name alias.
Payload: `DDL_CREATE_SYNONYM`

### CREATE JOB

Defines scheduled job.
Payload: `DDL_CREATE_JOB`

### CREATE EXTENSION

Defines extension registration.
Payload: `DDL_CREATE_EXTENSION`

### CREATE EXCEPTION

Defines user-defined exception.
Payload: `DDL_CREATE_EXCEPTION`

## Errors

- Unknown object type: `ERR_DDL_UNSUPPORTED_OBJECT`.
- Unsupported option: `ERR_DDL_UNSUPPORTED_OPTION`.
- Duplicate object without `CREATE OR REPLACE`: `ERR_OBJECT_EXISTS`.

## Related Specs

- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`
- `DDL_ALTER.md`
- `DDL_DROP_TRUNCATE.md`
