# DDL_TYPES.md: CREATE/ALTER/DROP TYPE (ScratchBirdSQL)

## 1. Purpose

Provide a ScratchBirdSQL-native `CREATE TYPE` specification that fully supports
PostgreSQL-style type definitions while mapping to a single internal SBLR
representation. This allows both ScratchBird and PostgreSQL parsers to emit the
same SBLR for type creation and management.

## 2. Scope

Type kinds covered by this specification:
- **ENUM** (ordered labels)
- **RECORD** (composite types)
- **RANGE** (bounded intervals with inclusive/exclusive flags)
- **BASE** (custom base type with I/O functions)

`CREATE DOMAIN` remains the constraint alias mechanism. `CREATE TYPE` defines a
reusable type and may be referenced by domains, table columns, and function
signatures.

## 3. Canonical ScratchBird Syntax

### 3.1 CREATE TYPE

```bnf
<create_type> ::=
    CREATE TYPE [IF NOT EXISTS] [<schema_name>.]<type_name>
        <type_definition>
    [WITH DIALECT (<dialect_tag>)]
    [WITH COMPAT (<compat_name>)]
    [COMMENT '<comment_text>']

<type_definition> ::=
      AS ENUM ( <enum_value_list> )
    | AS RECORD ( <record_field_list> )
    | AS ( <record_field_list> )            -- PostgreSQL composite alias
    | AS RANGE ( <range_options> )
    | AS BASE ( <base_type_options> )
    | AS SHELL                              -- PostgreSQL shell type

<enum_value_list> ::= <enum_value> [, <enum_value>]...
<enum_value> ::= '<label>' [= <position>]

<record_field_list> ::= <record_field> [, <record_field>]...
<record_field> ::= <field_name> <data_type> [COLLATE <collation_name>]
                  [NOT NULL] [DEFAULT <expr>]

<range_options> ::= SUBTYPE = <data_type>
                    [ , SUBTYPE_COLLATION = <collation_name> ]
                    [ , SUBTYPE_OPCLASS = <opclass_name> ]
                    [ , CANONICAL = <function_name> ]
                    [ , SUBTYPE_DIFF = <function_name> ]
                    [ , MULTIRANGE = TRUE | FALSE ]

<base_type_options> ::=
    STORAGE = <data_type>
    , INPUT = <function_name>
    , OUTPUT = <function_name>
    [ , RECEIVE = <function_name> ]
    [ , SEND = <function_name> ]
    [ , TYPMOD_IN = <function_name> ]
    [ , TYPMOD_OUT = <function_name> ]
    [ , ANALYZE = <function_name> ]
    [ , ALIGNMENT = CHAR | SHORT | INT | DOUBLE ]
    [ , STORAGE_MODE = PLAIN | EXTERNAL | EXTENDED | MAIN ]
    [ , CATEGORY = '<category_code>' ]
    [ , PREFERRED = TRUE | FALSE ]
```

### 3.2 ALTER TYPE

```bnf
<alter_type> ::=
    ALTER TYPE [<schema_name>.]<type_name>
        <alter_type_action>

<alter_type_action> ::=
      RENAME TO <new_name>
    | SET SCHEMA <schema_name>
    | ADD VALUE '<label>' [BEFORE '<label>' | AFTER '<label>']   -- ENUM only
    | RENAME VALUE '<old_label>' TO '<new_label>'                -- ENUM only
    | SET ( <range_options> | <base_type_options> )              -- RANGE/BASE only
    | FINALIZE                                                   -- SHELL -> BASE
```

### 3.3 DROP TYPE

```bnf
<drop_type> ::=
    DROP TYPE [IF EXISTS] [<schema_name>.]<type_name> [CASCADE | RESTRICT]
```

## 4. Type Semantics

### 4.1 ENUM
- Labels are **ordered** by position (explicit or implicit).
- Comparison uses position, not string order.
- `ADD VALUE` appends by default; `BEFORE/AFTER` controls ordering.
- `DROP VALUE` is not supported (requires DROP TYPE).
- Maps 1:1 to ENUM domain behavior in
  `docs/specifications/types/DDL_DOMAINS_COMPREHENSIVE.md`.

### 4.2 RECORD (Composite)
- Defines a structured type with named fields.
- Field defaults and NOT NULL apply when the value is constructed or assigned.
- `AS ( ... )` is accepted to mirror PostgreSQL composite syntax.
- Maps 1:1 to RECORD domain behavior in
  `docs/specifications/types/DDL_DOMAINS_COMPREHENSIVE.md`.

### 4.3 RANGE
- Represents a bounded interval of a subtype (e.g., INT, TIMESTAMP).
- Bounds use inclusive/exclusive flags and allow empty ranges.
- Range operators (contains, overlaps, adjacency) are defined by subtype
  operators and comparator functions.
- `SUBTYPE_OPCLASS`, `CANONICAL`, and `SUBTYPE_DIFF` mirror PostgreSQL range
  semantics and are used for indexing and canonicalization.

### 4.4 BASE
- Defines a custom base type using declared I/O functions.
- `STORAGE` selects the internal representation (e.g., VARBINARY, BLOB).
- `INPUT`/`OUTPUT` are required and must be stable and deterministic.
- Optional `RECEIVE`/`SEND` define binary wire format conversions.
- `ALIGNMENT`, `STORAGE_MODE`, `CATEGORY`, and `PREFERRED` map to PostgreSQL
  type metadata and planner preferences.

### 4.5 SHELL
- Placeholder type definition for PostgreSQL compatibility.
- Only allowed for PostgreSQL parser mapping.
- Must be finalized with `ALTER TYPE ... FINALIZE` before use.

## 5. Catalog Representation (ScratchBird)

ScratchBird stores types in the **domains catalog** with a `domain_type`
identifying the type kind. This allows the Postgres parser to map all type
definitions to the same internal store.

**DomainRecord mappings** (see `docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`):
- `domain_type = TYPE_ENUM`   -> enum labels stored in `enum_values_oid`
- `domain_type = TYPE_RECORD` -> record fields stored in `fields_oid`
- `domain_type = TYPE_RANGE`  -> range descriptor stored in `fields_oid`
- `domain_type = TYPE_BASE`   -> base-type handler descriptor stored in `fields_oid`
- `domain_type = TYPE_SHELL`  -> shell type (no payload until finalized)

## 6. SBLR Mapping

All `CREATE/ALTER/DROP TYPE` statements compile to the same SBLR domain
opcodes used by `CREATE DOMAIN`, with a `domain_type` discriminator.

Recommended mapping:
- `CREATE TYPE` -> `OP_DOMAIN_CREATE` with `domain_type` set to TYPE_*
- `ALTER TYPE`  -> `OP_DOMAIN_ALTER` with `domain_type` validation
- `DROP TYPE`   -> `OP_DOMAIN_DROP`

Type-specific payloads reuse `SBLR_DOMAIN_PAYLOADS.md` with a discriminator:
- ENUM: `enum_values_oid` payload
- RECORD: record field payload
- RANGE: range descriptor payload
- BASE: base handler payload

## 7. PostgreSQL Compatibility Mapping

| PostgreSQL DDL | ScratchBirdSQL | Internal Mapping |
| --- | --- | --- |
| `CREATE TYPE name AS ( ... )` | `CREATE TYPE name AS RECORD ( ... )` | TYPE_RECORD |
| `CREATE TYPE name AS ENUM (...)` | `CREATE TYPE name AS ENUM (...)` | TYPE_ENUM |
| `CREATE TYPE name AS RANGE (...)` | `CREATE TYPE name AS RANGE (...)` | TYPE_RANGE |
| `CREATE TYPE name ( INPUT = ..., OUTPUT = ... )` | `CREATE TYPE name AS BASE (...)` | TYPE_BASE |
| `CREATE TYPE name;` | `CREATE TYPE name AS SHELL` | TYPE_SHELL |

## 8. SHOW / EXTRACT Support

```sql
SHOW CREATE TYPE my_type;
EXTRACT TYPE my_type;
```

`SHOW CREATE TYPE` must emit canonical ScratchBirdSQL syntax.

## 9. Security and Permissions

- `CREATE TYPE` requires `CREATE` on target schema.
- `ALTER/DROP TYPE` requires ownership or admin privilege.
- `USAGE ON TYPE` is required for cross-schema references.

## 10. Errors

- `TYPE_ALREADY_EXISTS`
- `TYPE_NOT_FOUND`
- `INVALID_ENUM_LABEL`
- `INVALID_RANGE_SUBTYPE`
- `TYPE_SHELL_NOT_FINALIZED`
- `DEPENDENT_OBJECTS_EXIST`

## 11. Examples

```sql
-- ENUM
CREATE TYPE status AS ENUM ('PENDING', 'ACTIVE', 'SUSPENDED');

-- RECORD (ScratchBird canonical)
CREATE TYPE address AS RECORD (
    street VARCHAR(100),
    city VARCHAR(50),
    state CHAR(2)
);

-- RECORD (PostgreSQL-compatible alias)
CREATE TYPE address_pg AS (
    street VARCHAR(100),
    city VARCHAR(50),
    state CHAR(2)
);

-- RANGE
CREATE TYPE int_range AS RANGE (
    SUBTYPE = INTEGER,
    SUBTYPE_DIFF = int4range_diff,
    CANONICAL = int4range_canonical
);

-- BASE
CREATE TYPE money_t AS BASE (
    STORAGE = VARBINARY(16),
    INPUT = money_in,
    OUTPUT = money_out,
    SEND = money_send,
    RECEIVE = money_recv,
    CATEGORY = 'N',
    PREFERRED = TRUE
);
```
