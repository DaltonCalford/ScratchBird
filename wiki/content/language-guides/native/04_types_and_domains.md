[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Native V2 SQL - Types and Domains

## Overview

This document describes custom type and domain features in ScratchBird's Native V2 SQL dialect. Domains allow you to define reusable custom data types with constraints, defaults, and validation rules.

ScratchBird supports several domain kinds beyond basic scalar types:
- **BASIC** - Standard scalar type with constraints
- **ENUM** - Enumerated type with fixed set of values
- **RECORD** - Composite type with named fields
- **SET** - Collection of values of a given type
- **VARIANT** - Tagged union type (one of several types)

**Parser Pipeline:** V2 Parser → AST v2 → SemanticAnalyzerV2 → BytecodeGeneratorV2 → Executor

**Source Code References:**
- Parser: `/ScratchBird/src/parser/parser_v2.cpp`
- AST: `/ScratchBird/include/scratchbird/parser/ast_v2.h`
- Semantic/Bytecode: `/ScratchBird/src/sblr/semantic_analyzer_v2.cpp`, `/ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `/ScratchBird/src/sblr/executor.cpp`

---

## CREATE DOMAIN

### Description

Creates a custom domain (user-defined type) based on an existing data type, with optional constraints and defaults. Domains promote consistency and reusability across your schema.

### Syntax

**Basic Domain:**
```sql
CREATE DOMAIN <domain_name> AS <base_type>
    [DEFAULT <expression>]
    [NOT NULL | NULL]
    [CHECK (<constraint_expression>)]
```

**Enum Domain:**
```sql
CREATE DOMAIN <domain_name> AS ENUM (<value> [, ...])
```

**Record Domain:**
```sql
CREATE DOMAIN <domain_name> AS RECORD (
    <field_name> <type> [, ...]
)
```

**Set Domain:**
```sql
CREATE DOMAIN <domain_name> AS SET OF <element_type>
```

**Variant Domain:**
```sql
CREATE DOMAIN <domain_name> AS VARIANT (
    <type_alternative> [, ...]
)
```

### Parameters

- **domain_name**: Name of the domain (schema-qualified if desired)
- **base_type**: Underlying data type for basic domains
- **DEFAULT**: Default value for the domain
- **NOT NULL**: Prevents NULL values
- **CHECK**: Validation constraint using VALUE keyword
- **ENUM values**: List of allowed string values for enum domains
- **RECORD fields**: Named fields with types for composite domains
- **SET element_type**: Element type for set collections
- **VARIANT types**: Alternative types for variant domains

### Examples

**Example 1: Create a basic domain with constraints**
```sql
CREATE DOMAIN email_address AS VARCHAR(255)
    NOT NULL
    CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$');
```

**Example 2: Create a domain with default**
```sql
CREATE DOMAIN positive_integer AS INTEGER
    DEFAULT 0
    CHECK (VALUE >= 0);
```

**Example 3: Create an enum domain**
```sql
CREATE DOMAIN order_status AS ENUM (
    'pending',
    'processing',
    'shipped',
    'delivered',
    'cancelled'
);
```

**Example 4: Create an enum domain for user roles**
```sql
CREATE DOMAIN user_role AS ENUM (
    'admin',
    'moderator',
    'user',
    'guest'
);
```

**Example 5: Create a record domain for addresses**
```sql
CREATE DOMAIN address AS RECORD (
    street VARCHAR(200),
    city VARCHAR(100),
    state VARCHAR(2),
    zip VARCHAR(10),
    country VARCHAR(100)
);
```

**Example 6: Create a record domain for coordinates**
```sql
CREATE DOMAIN geo_point AS RECORD (
    latitude DOUBLE PRECISION,
    longitude DOUBLE PRECISION,
    altitude DOUBLE PRECISION
);
```

**Example 7: Create a set domain**
```sql
CREATE DOMAIN tag_set AS SET OF VARCHAR(50);
```

**Example 8: Create a variant domain**
```sql
CREATE DOMAIN id_type AS VARIANT (
    INTEGER,
    UUID,
    VARCHAR(100)
);
```

**Example 9: Domain for currency amounts**
```sql
CREATE DOMAIN currency_amount AS DECIMAL(15, 2)
    CHECK (VALUE >= 0);
```

**Example 10: Domain for percentage values**
```sql
CREATE DOMAIN percentage AS DECIMAL(5, 2)
    CHECK (VALUE >= 0 AND VALUE <= 100);
```

### Usage in Tables

Once created, domains can be used like any other data type:

```sql
-- Using basic domain
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    email email_address,
    age positive_integer
);

-- Using enum domain
CREATE TABLE orders (
    id BIGINT PRIMARY KEY,
    status order_status DEFAULT 'pending'
);

-- Using record domain
CREATE TABLE locations (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    coordinates geo_point
);

-- Using set domain
CREATE TABLE articles (
    id INTEGER PRIMARY KEY,
    title VARCHAR(200),
    tags tag_set
);
```

### Notes

- Domains enforce constraints automatically on INSERT and UPDATE
- CHECK constraints use VALUE keyword to reference the domain value
- Domains can be used anywhere the base type can be used
- Multiple columns can share the same domain
- Changing domain constraints affects all columns using the domain
- Domains improve schema documentation and maintainability

---

## ALTER DOMAIN

### Description

Modifies an existing domain's constraints, defaults, or compatibility settings.

### Syntax

```sql
ALTER DOMAIN <domain_name> SET DEFAULT <expression>
ALTER DOMAIN <domain_name> DROP DEFAULT
ALTER DOMAIN <domain_name> ADD CHECK (<constraint_expression>)
ALTER DOMAIN <domain_name> DROP CONSTRAINT <constraint_name>
ALTER DOMAIN <domain_name> SET COMPAT <compatibility_name>
ALTER DOMAIN <domain_name> DROP COMPAT
ALTER DOMAIN <domain_name> RENAME TO <new_name>
```

### Parameters

- **SET DEFAULT**: Sets or changes the default value
- **DROP DEFAULT**: Removes the default value
- **ADD CHECK**: Adds a new constraint
- **DROP CONSTRAINT**: Removes a named constraint
- **SET COMPAT**: Sets compatibility mode (implementation-specific)
- **DROP COMPAT**: Removes compatibility mode
- **RENAME TO**: Renames the domain

### Examples

**Example 1: Change default value**
```sql
ALTER DOMAIN positive_integer SET DEFAULT 1;
```

**Example 2: Remove default**
```sql
ALTER DOMAIN email_address DROP DEFAULT;
```

**Example 3: Add a constraint**
```sql
ALTER DOMAIN currency_amount ADD CHECK (VALUE <= 999999999.99);
```

**Example 4: Drop a constraint**
```sql
ALTER DOMAIN percentage DROP CONSTRAINT percentage_check;
```

**Example 5: Rename a domain**
```sql
ALTER DOMAIN email_address RENAME TO email_addr;
```

**Example 6: Set compatibility mode**
```sql
ALTER DOMAIN user_role SET COMPAT firebird_enum;
```

**Example 7: Drop compatibility mode**
```sql
ALTER DOMAIN user_role DROP COMPAT;
```

### Notes

- Changes to domains affect all columns using that domain
- Adding constraints validates existing data
- Constraint addition may fail if existing data violates the constraint
- Renaming doesn't affect column definitions (transparent)
- SET/DROP COMPAT is implementation-specific for dialect compatibility

---

## DROP DOMAIN

### Description

Removes a domain from the database.

### Syntax

```sql
DROP DOMAIN [IF EXISTS] <domain_name> RESTRICT
```

### Parameters

- **IF EXISTS**: Prevents error if domain doesn't exist
- **domain_name**: Name of the domain to drop
- **RESTRICT**: Required. Prevents drop if domain is in use

### Examples

**Example 1: Drop a domain**
```sql
DROP DOMAIN email_address RESTRICT;
```

**Example 2: Safely drop a domain**
```sql
DROP DOMAIN IF EXISTS old_status RESTRICT;
```

**Example 3: Drop multiple domains**
```sql
DROP DOMAIN IF EXISTS legacy_type_1 RESTRICT;
DROP DOMAIN IF EXISTS legacy_type_2 RESTRICT;
```

### Notes

- Cannot drop domains that are in use by tables (RESTRICT is required)
- Must drop or alter all columns using the domain first
- Domain drops are transactional
- No CASCADE option available - must be explicit about dependencies

---

## Built-in Data Types

For reference, ScratchBird supports these standard SQL data types that can be used as base types for domains:

**Numeric Types:**
- `INTEGER`, `INT`, `SMALLINT`, `BIGINT` - Integer numbers
- `DECIMAL(p,s)`, `NUMERIC(p,s)` - Fixed-point decimal
- `REAL`, `DOUBLE PRECISION`, `FLOAT` - Floating-point
- `SERIAL`, `BIGSERIAL` - Auto-incrementing integers

**String Types:**
- `VARCHAR(n)`, `CHARACTER VARYING(n)` - Variable-length string
- `CHAR(n)`, `CHARACTER(n)` - Fixed-length string
- `TEXT` - Variable-length text

**Binary Types:**
- `BYTEA` - Binary data
- `BLOB` - Binary large object

**Date/Time Types:**
- `DATE` - Calendar date
- `TIME` - Time of day
- `TIMESTAMP` - Date and time
- `TIMESTAMP WITH TIME ZONE` - Timestamp with timezone
- `INTERVAL` - Time interval

**Boolean:**
- `BOOLEAN` - True/false value

**UUID:**
- `UUID` - Universally unique identifier

**JSON:**
- `JSON` - JSON data
- `JSONB` - Binary JSON (faster operations)

**Special Types:**
- `ARRAY` - Array of values
- `HSTORE` - Key-value store

---

## CREATE TYPE

### Description

Creates a custom composite, enum, range, base, or shell type. CREATE TYPE provides PostgreSQL-style type creation as an alternative to CREATE DOMAIN.

### Syntax

**Enum Type:**
```sql
CREATE TYPE [IF NOT EXISTS] <type_name> [AS] ENUM (
    '<label>' [= <position>] [, ...]
)
```

**Record/Composite Type:**
```sql
CREATE TYPE [IF NOT EXISTS] <type_name> [AS] RECORD (
    <field_name> <type> [COLLATE <collation>] [NOT NULL | NULL] [DEFAULT <expr>] [, ...]
)
-- or without RECORD keyword:
CREATE TYPE <type_name> (
    <field_name> <type> [, ...]
)
```

**Range Type:**
```sql
CREATE TYPE [IF NOT EXISTS] <type_name> [AS] RANGE (
    SUBTYPE = <type>,
    [SUBTYPE_COLLATION = <collation>,]
    [SUBTYPE_OPCLASS = <opclass>,]
    [CANONICAL = <function>,]
    [SUBTYPE_DIFF = <function>,]
    [MULTIRANGE = {TRUE | FALSE}]
)
```

**Base Type:**
```sql
CREATE TYPE [IF NOT EXISTS] <type_name> [AS] BASE (
    INPUT = <function>,
    OUTPUT = <function>,
    [RECEIVE = <function>,]
    [SEND = <function>,]
    [TYPMOD_IN = <function>,]
    [TYPMOD_OUT = <function>,]
    [ANALYZE = <function>,]
    [STORAGE = <type>,]
    [ALIGNMENT = {CHAR | SHORT | INT | DOUBLE},]
    [STORAGE_MODE = {PLAIN | EXTERNAL | EXTENDED | MAIN},]
    [CATEGORY = '<char>',]
    [PREFERRED = {TRUE | FALSE}]
)
```

**Shell Type:**
```sql
CREATE TYPE [IF NOT EXISTS] <type_name> [AS] SHELL
```

### Examples

**Example 1: Enum type**
```sql
CREATE TYPE mood AS ENUM ('sad', 'ok', 'happy');
```

**Example 2: Enum type with explicit positions**
```sql
CREATE TYPE priority AS ENUM ('low' = 1, 'medium' = 5, 'high' = 10);
```

**Example 3: Composite type**
```sql
CREATE TYPE address AS RECORD (
    street VARCHAR(200),
    city VARCHAR(100) NOT NULL,
    zip VARCHAR(10) DEFAULT '00000'
);
```

**Example 4: Range type**
```sql
CREATE TYPE float_range AS RANGE (
    SUBTYPE = FLOAT,
    SUBTYPE_DIFF = float_diff
);
```

**Example 5: Shell type placeholder**
```sql
CREATE TYPE custom_type AS SHELL;
```

### Implementation Status

- V2 parser: `parseCreateType()` handles IF NOT EXISTS, schema-qualified names, and all five type kinds: ENUM (with positions), RECORD (with fields, collation, nullability, defaults), RANGE (with full options), BASE (with I/O functions, alignment, storage), and SHELL

---

## DROP TYPE

### Description

Removes one or more types.

### Syntax

```sql
DROP TYPE [IF EXISTS] <type_name> [, ...] [CASCADE | RESTRICT]
```

### Examples

```sql
DROP TYPE mood;
DROP TYPE IF EXISTS address, priority CASCADE;
```

### Implementation Status

- V2 parser: `parseDropType()` supports IF EXISTS, multiple types, CASCADE, and RESTRICT

---

## Known Limitations

### Partial Implementation

**Domain Kinds:**
- BASIC domains: Fully implemented
- ENUM domains: Fully implemented
- RECORD domains: Implemented (serialization needs validation)
- SET domains: Implemented (serialization needs validation)
- VARIANT domains: Implemented (serialization needs validation)
- Spec reference: `/docs/specifications/types/03_TYPES_AND_DOMAINS.md`

**CREATE TYPE:**
- Fully parsed in V2 for ENUM, RECORD, RANGE, BASE, and SHELL type kinds
- Catalog registration and runtime usage need validation for complex type kinds (RANGE, BASE)

### Known Gaps

**Domain Features:**
- COLLATE clause not fully supported for string domains
- Domain inheritance not supported
- Array domains (domain arrays) may have limitations
- Type casts between domains not fully implemented

**Advanced Type Features:**
- ROW type expressions partial
- Type resolution in complex expressions may have edge cases
- Custom type input/output function wiring needs validation

### Spec Deltas

**Record/Variant Serialization:**
- Implementation exists but full serialization needs validation
- SBLR bytecode encoding for complex domains needs testing
- Cross-session persistence of complex domain values needs verification

**Domain Constraints:**
- Multi-column CHECK constraints not supported in domains
- CHECK constraint expression complexity may have limits
- Constraint evaluation order not specified
- Spec reference: `/docs/specifications/types/DDL_DOMAINS_COMPREHENSIVE.md`

**Type Compatibility:**
- SET COMPAT/DROP COMPAT functionality not fully documented
- Compatibility modes for dialect emulation not complete
- Type coercion rules between domains need clarification

### General Notes

- All domain and type DDL operations are fully transactional
- Domains and types are persisted using ScratchBird's Multi-Generational Architecture (MGA)
- Domain constraints are enforced at runtime during DML operations
- Full implementation status in `/docs/audit/parsers/V2/SUMMARY.md`
- Critical findings in `/docs/audit/parsers/CRITICAL_FINDINGS.md`
