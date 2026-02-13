# ScratchBird v3 (native) parser: CREATE TYPE and CREATE DOMAIN user guide

Scope: This document describes what the v3 (native) parser accepts for `CREATE TYPE` and `CREATE DOMAIN` based on the implementation in `src/parser/parser_v3.cpp` and AST definitions in `include/scratchbird/parser/ast_v3.h`.

## CREATE DOMAIN

### Syntax summary

```sql
CREATE DOMAIN [IF NOT EXISTS] <domain_name>
  [AS] <domain_definition>
  [INHERITS (<parent_domain>) ]
  [<domain_constraint> ...]
  [COLLATE <collation_name>]
  [WITH DIALECT(<dialect_name>)]
  [WITH COMPAT(<compat_name>)]
  [WITH INTEGRITY(<integrity_options>)]
  [WITH SECURITY(<security_options>)]
  [WITH VALIDATION(<validation_options>)]
  [WITH QUALITY(<quality_options>)]
  [WITH OPTIONS(<options>)]
;
```

`<domain_definition>`

```sql
<base_type>
RECORD ( <record_field> [, ...] )
ENUM ( <enum_value> [, ...] )
SET OF <element_type>
VARIANT ( <type_name> [, ...] )
```

`<record_field>`

```sql
<field_name> <type_name> [NOT NULL | NULL] [DEFAULT <expression>]
```

`<enum_value>`

```sql
'<label>' [= <integer_position>]
```

`<domain_constraint>`

```sql
[CONSTRAINT <constraint_name>] NOT NULL
[CONSTRAINT <constraint_name>] NULL
[CONSTRAINT <constraint_name>] DEFAULT <expression>
[CONSTRAINT <constraint_name>] CHECK (<expression>)
```

`<integrity_options>`

```sql
UNIQUENESS = TRUE|FALSE,
NORMALIZATION = NONE | <function_name_or_tag>,
NORMALIZATION_FUNCTION = <function_name_or_tag>
```

`<security_options>`

```sql
MASKING = <tag_or_function>,
MASK_PATTERN = <pattern>,
ENCRYPTION = <tag_or_function>,
AUDIT_ACCESS = TRUE|FALSE,
REQUIRE_PRIVILEGE = <privilege_name>
REQUIRE PRIVILEGE = <privilege_name>
```

`<validation_options>`

```sql
FUNCTION = <function_name_or_tag>,
ERROR_MESSAGE = <string_or_tag>
```

`<quality_options>`

```sql
PARSE_FUNCTION = <function_name_or_tag>,
STANDARDIZE_FUNCTION = <function_name_or_tag>,
ENRICH_FUNCTION = <function_name_or_tag>
```

`<options>`

```sql
WRAP = TRUE|FALSE
```

### Notes

- The parser accepts the domain kinds: base type, `RECORD`, `ENUM`, `SET OF`, and `VARIANT`.
- `INHERITS` requires parentheses: `INHERITS (<parent_domain>)`.
- Constraints and `COLLATE` can appear in any order after the definition. Multiple constraints are allowed.
- `WITH ...` blocks can appear multiple times; only known blocks are accepted. Unknown blocks are rejected.
- `ENUM` values must be string literals; optional `= <integer>` assigns a position.
- `WITH OPTIONS (WRAP = ...)` only applies to `CREATE DOMAIN` and currently exposes `WRAP` (enum wrapping behavior).

### Examples

Basic domain with constraints and collation:

```sql
CREATE DOMAIN IF NOT EXISTS customer_email AS VARCHAR(255)
  NOT NULL
  CHECK (VALUE LIKE '%@%')
  DEFAULT 'unknown@example.com'
  COLLATE utf8_nocase
;
```

Record domain:

```sql
CREATE DOMAIN address AS RECORD (
  street VARCHAR(200) NOT NULL,
  city VARCHAR(100) NOT NULL,
  zip_code VARCHAR(20)
);
```

Enum domain with explicit positions and wrap option:

```sql
CREATE DOMAIN status_flag AS ENUM ('pending' = 1, 'active' = 2, 'disabled' = 3)
  WITH OPTIONS (WRAP = TRUE)
;
```

Set domain:

```sql
CREATE DOMAIN tag_ids AS SET OF INT64;
```

Variant domain with validation and security:

```sql
CREATE DOMAIN contact_method AS VARIANT (VARCHAR(255), INT64, UUID)
  WITH VALIDATION (FUNCTION = validate_contact, ERROR_MESSAGE = 'invalid contact')
  WITH SECURITY (MASKING = redact_email, AUDIT_ACCESS = TRUE)
;
```

Domain inheritance:

```sql
CREATE DOMAIN strong_email AS VARCHAR(255)
  INHERITS (customer_email)
  CHECK (VALUE LIKE '%@%')
;
```

## CREATE TYPE

### Syntax summary

```sql
CREATE TYPE [IF NOT EXISTS] <type_name>
  [AS] <type_definition>
  [WITH DIALECT(<dialect_name>)]
  [WITH COMPAT(<compat_name>)]
  [COMMENT '<text>']
;
```

`<type_definition>`

```sql
ENUM ( <enum_value> [, ...] )
RECORD ( <record_field> [, ...] )
( <record_field> [, ...] )
RANGE ( <range_options> )
BASE ( <base_options> )
SHELL
```

`<record_field>`

```sql
<field_name> <type_name> [COLLATE <collation_name>] [NOT NULL | NULL] [DEFAULT <expression>]
```

`<enum_value>`

```sql
'<label>' [= <integer_position>]
```

`<range_options>`

```sql
SUBTYPE = <type_name>,
SUBTYPE_COLLATION = <collation_name>,
SUBTYPE_OPCLASS = <opclass_name>,
CANONICAL = <function_name>,
SUBTYPE_DIFF = <function_name>,
MULTIRANGE = TRUE|FALSE|0|1
```

`<base_options>`

```sql
STORAGE = <type_name>,
INPUT = <function_name>,
OUTPUT = <function_name>,
RECEIVE = <function_name>,
SEND = <function_name>,
TYPMOD_IN = <function_name>,
TYPMOD_OUT = <function_name>,
ANALYZE = <function_name>,
ALIGNMENT = CHAR|SHORT|INT|DOUBLE,
STORAGE_MODE = PLAIN|EXTERNAL|EXTENDED|MAIN,
CATEGORY = '<char>',
PREFERRED = TRUE|FALSE|0|1
```

### Notes

- `RECORD` can be written explicitly (`RECORD (...)`) or implicitly by using parentheses directly (`(...)`).
- `WITH DIALECT(...)` and `WITH COMPAT(...)` may appear multiple times. Unknown `WITH` clauses are rejected.
- `COMMENT '<text>'` is only accepted after all `WITH ...` clauses.
- `RECORD` fields accept optional `COLLATE <name>` (parsed but not stored in the AST).

### Examples

Enum type:

```sql
CREATE TYPE status AS ENUM ('pending', 'active', 'disabled');
```

Record type (implicit RECORD):

```sql
CREATE TYPE address AS (
  street VARCHAR(200) NOT NULL,
  city VARCHAR(100),
  zip_code VARCHAR(20) DEFAULT '00000'
);
```

Range type:

```sql
CREATE TYPE price_range AS RANGE (
  SUBTYPE = DECIMAL(12,2),
  SUBTYPE_COLLATION = utf8_nocase,
  CANONICAL = normalize_price_range,
  MULTIRANGE = TRUE
);
```

Base type:

```sql
CREATE TYPE phone_e164 AS BASE (
  STORAGE = VARCHAR(32),
  INPUT = phone_in,
  OUTPUT = phone_out,
  ALIGNMENT = INT,
  STORAGE_MODE = EXTENDED,
  CATEGORY = 'U',
  PREFERRED = TRUE
);
```

Shell type with compatibility tag:

```sql
CREATE TYPE legacy_id AS SHELL
  WITH COMPAT('postgres')
  COMMENT 'placeholder for legacy type mapping'
;
```
