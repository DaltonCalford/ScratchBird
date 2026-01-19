# PostgreSQL Types and Domains

**PostgreSQL Emulation Layer - Types and Domains Reference**

This document covers PostgreSQL-compatible type and domain operations in ScratchBird's PostgreSQL emulation layer.

---

## Overview

PostgreSQL provides a rich type system including built-in types, user-defined types (ENUMs, composite types, ranges), and domains (constrained types). Domains allow you to create custom types with validation rules.

**Key Points:**
- Full support for CREATE DOMAIN with constraints (IMPLEMENTED)
- Support for CREATE TYPE AS ENUM and composite types (mapped to domains)
- Range types parsed but not fully implemented
- Domains can have defaults, NOT NULL, and CHECK constraints
- Domain constraints are enforced at runtime

---

## CREATE DOMAIN

Creates a custom type with constraints.

### Description

`CREATE DOMAIN` defines a new data type with optional constraints. Domains are useful for enforcing business rules at the type level, ensuring data integrity across all tables that use the domain.

### Syntax

```sql
CREATE DOMAIN name [ AS ] data_type
    [ COLLATE collation ]
    [ DEFAULT expression ]
    [ constraint [ ... ] ]

constraint:
    [ CONSTRAINT constraint_name ]
    { NOT NULL | NULL | CHECK (expression) }
```

### Parameters

- **name** - Name of the domain
- **data_type** - Underlying data type (INTEGER, TEXT, etc.)
- **COLLATE** - Collation for text domains
- **DEFAULT** - Default value for the domain
- **NOT NULL** - Domain cannot be NULL
- **CHECK** - Constraint expression (use VALUE to reference the domain value)
- **constraint_name** - Optional name for the constraint

### Examples

**Basic domain:**
```sql
CREATE DOMAIN email_address AS TEXT;
```

**Domain with CHECK constraint:**
```sql
CREATE DOMAIN email_address AS TEXT
CHECK (VALUE LIKE '%@%.%');
```

**Domain with NOT NULL:**
```sql
CREATE DOMAIN positive_integer AS INTEGER
NOT NULL
CHECK (VALUE > 0);
```

**Domain with DEFAULT:**
```sql
CREATE DOMAIN account_status AS TEXT
DEFAULT 'active'
CHECK (VALUE IN ('active', 'inactive', 'suspended'));
```

**Domain with multiple constraints:**
```sql
CREATE DOMAIN username AS VARCHAR(50)
NOT NULL
CHECK (LENGTH(VALUE) >= 3)
CHECK (VALUE ~ '^[a-zA-Z0-9_]+$');  -- alphanumeric and underscore only
```

**Domain with named constraints:**
```sql
CREATE DOMAIN price AS NUMERIC(10,2)
CONSTRAINT positive_price CHECK (VALUE >= 0)
CONSTRAINT max_price CHECK (VALUE <= 999999.99);
```

**Email domain with validation:**
```sql
CREATE DOMAIN email AS TEXT
CHECK (
    VALUE ~ '^[a-zA-Z0-9.!#$%&''*+/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$'
);
```

**Phone number domain:**
```sql
CREATE DOMAIN phone_number AS TEXT
CHECK (VALUE ~ '^\+?[1-9]\d{1,14}$');  -- E.164 format
```

**Percentage domain:**
```sql
CREATE DOMAIN percentage AS NUMERIC(5,2)
CHECK (VALUE >= 0 AND VALUE <= 100);
```

**Using domains in tables:**
```sql
CREATE DOMAIN email_address AS TEXT CHECK (VALUE LIKE '%@%');

CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    email email_address NOT NULL,
    backup_email email_address
);

-- This insert will fail due to domain constraint
INSERT INTO users (id, email) VALUES (1, 'invalid-email');

-- This will succeed
INSERT INTO users (id, email) VALUES (1, 'user@example.com');
```

### Notes

- Domains are types, not tables or constraints
- Domains can be used anywhere the underlying type can be used
- Domain constraints are checked on INSERT and UPDATE
- The special VALUE keyword refers to the domain's current value
- Domains can be dropped with DROP DOMAIN
- Domains can be altered with ALTER DOMAIN

### Related Statements

- [ALTER DOMAIN](#alter-domain)
- [DROP DOMAIN](#drop-domain)
- [CREATE TYPE](#create-type)

---

## ALTER DOMAIN

Modifies an existing domain.

### Description

`ALTER DOMAIN` changes a domain's constraints, default value, or name.

### Syntax

```sql
ALTER DOMAIN name
    { SET DEFAULT expression | DROP DEFAULT }
  | { SET | DROP } NOT NULL
  | ADD constraint [ NOT VALID ]
  | DROP CONSTRAINT [ IF EXISTS ] constraint_name [ RESTRICT | CASCADE ]
  | RENAME CONSTRAINT constraint_name TO new_constraint_name
  | VALIDATE CONSTRAINT constraint_name
  | OWNER TO new_owner
  | RENAME TO new_name
  | SET SCHEMA new_schema
```

### Parameters

- **SET DEFAULT** - Set a new default value
- **DROP DEFAULT** - Remove the default value
- **SET NOT NULL** - Add NOT NULL constraint
- **DROP NOT NULL** - Remove NOT NULL constraint
- **ADD constraint** - Add a new CHECK constraint
- **DROP CONSTRAINT** - Remove a constraint
- **RENAME CONSTRAINT** - Rename a constraint
- **VALIDATE CONSTRAINT** - Validate a constraint added with NOT VALID
- **OWNER TO** - Change domain owner
- **RENAME TO** - Rename the domain
- **SET SCHEMA** - Move domain to different schema

### Examples

**Set default value:**
```sql
ALTER DOMAIN account_status SET DEFAULT 'pending';
```

**Drop default value:**
```sql
ALTER DOMAIN account_status DROP DEFAULT;
```

**Add NOT NULL constraint:**
```sql
ALTER DOMAIN username SET NOT NULL;
```

**Drop NOT NULL constraint:**
```sql
ALTER DOMAIN email_address DROP NOT NULL;
```

**Add CHECK constraint:**
```sql
ALTER DOMAIN price
ADD CONSTRAINT reasonable_price CHECK (VALUE <= 100000);
```

**Add constraint without validating existing data:**
```sql
ALTER DOMAIN email_address
ADD CONSTRAINT valid_email CHECK (VALUE LIKE '%@%')
NOT VALID;

-- Later, validate existing values
ALTER DOMAIN email_address VALIDATE CONSTRAINT valid_email;
```

**Drop a constraint:**
```sql
ALTER DOMAIN price DROP CONSTRAINT positive_price;
```

**Drop constraint with CASCADE:**
```sql
ALTER DOMAIN email_address DROP CONSTRAINT valid_email CASCADE;
```

**Rename a constraint:**
```sql
ALTER DOMAIN price RENAME CONSTRAINT positive_price TO non_negative_price;
```

**Rename a domain:**
```sql
ALTER DOMAIN email_address RENAME TO email;
```

**Change domain owner:**
```sql
ALTER DOMAIN email_address OWNER TO admin;
```

**Move domain to different schema:**
```sql
ALTER DOMAIN email_address SET SCHEMA public_types;
```

### Notes

- SET NOT NULL fails if existing values contain NULL
- ADD constraint can use NOT VALID to skip existing data validation
- VALIDATE CONSTRAINT checks existing values against NOT VALID constraint
- CASCADE drops objects dependent on the constraint
- ALTER DOMAIN affects all tables using the domain

### Related Statements

- [CREATE DOMAIN](#create-domain)
- [DROP DOMAIN](#drop-domain)

---

## DROP DOMAIN

Removes a domain from the database.

### Description

`DROP DOMAIN` removes one or more domain definitions. Use CASCADE to drop columns that use the domain.

### Syntax

```sql
DROP DOMAIN [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]
```

### Parameters

- **IF EXISTS** - Do not error if domain doesn't exist
- **name** - Name of the domain to drop
- **CASCADE** - Drop columns and tables that use the domain
- **RESTRICT** - Refuse if any columns use the domain (default)

### Examples

**Drop a domain:**
```sql
DROP DOMAIN email_address;
```

**Drop with IF EXISTS:**
```sql
DROP DOMAIN IF EXISTS old_type;
```

**Drop multiple domains:**
```sql
DROP DOMAIN email_address, phone_number, username;
```

**Drop with CASCADE:**
```sql
DROP DOMAIN email_address CASCADE;
```

### Notes

- RESTRICT is the default behavior
- CASCADE will drop or alter columns using the domain
- IF EXISTS prevents errors in repeatable scripts
- Dropping a domain may cause dependent queries/functions to fail

### Related Statements

- [CREATE DOMAIN](#create-domain)
- [ALTER DOMAIN](#alter-domain)

---

## CREATE TYPE

Creates a user-defined type.

### Description

`CREATE TYPE` defines a new data type. PostgreSQL supports enumerated types (ENUM), composite types (structured records), range types, and base types. In ScratchBird, ENUMs and composite types are mapped to domain payloads.

### Syntax

```sql
-- Enumerated type
CREATE TYPE name AS ENUM ( [ 'label' [, ... ] ] )

-- Composite type
CREATE TYPE name AS ( [ attribute_name data_type [ COLLATE collation ] [, ... ] ] )

-- Range type (parsed but not fully implemented)
CREATE TYPE name AS RANGE (
    SUBTYPE = subtype
    [ , SUBTYPE_OPCLASS = subtype_operator_class ]
    [ , COLLATION = collation ]
    [ , CANONICAL = canonical_function ]
    [ , SUBTYPE_DIFF = subtype_diff_function ]
    [ , MULTIRANGE_TYPE_NAME = multirange_type_name ]
)
```

### Parameters

- **name** - Name of the type
- **label** - Enum value label (for ENUM types)
- **attribute_name** - Field name (for composite types)
- **data_type** - Data type for field (for composite types)
- **SUBTYPE** - Element type (for range types)

### Examples

**Enumerated type (ENUM):**
```sql
CREATE TYPE status_enum AS ENUM ('pending', 'active', 'inactive', 'archived');
```

**Using ENUM in table:**
```sql
CREATE TYPE order_status AS ENUM ('pending', 'processing', 'shipped', 'delivered', 'cancelled');

CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    status order_status DEFAULT 'pending'
);

INSERT INTO orders (id, status) VALUES (1, 'processing');
-- This will fail - not a valid enum value:
-- INSERT INTO orders (id, status) VALUES (2, 'invalid');
```

**Composite type:**
```sql
CREATE TYPE address AS (
    street TEXT,
    city TEXT,
    state VARCHAR(2),
    zip_code VARCHAR(10)
);
```

**Using composite type in table:**
```sql
CREATE TYPE full_name AS (
    first_name TEXT,
    middle_name TEXT,
    last_name TEXT
);

CREATE TABLE employees (
    id INTEGER PRIMARY KEY,
    name full_name,
    salary NUMERIC
);

-- Insert using ROW constructor
INSERT INTO employees (id, name, salary)
VALUES (1, ROW('John', 'Q', 'Public'), 50000);

-- Access composite field
SELECT (name).first_name FROM employees WHERE id = 1;
```

**Multiple value ENUM:**
```sql
CREATE TYPE day_of_week AS ENUM (
    'Monday',
    'Tuesday',
    'Wednesday',
    'Thursday',
    'Friday',
    'Saturday',
    'Sunday'
);
```

**Product type example:**
```sql
CREATE TYPE product_category AS ENUM (
    'electronics',
    'clothing',
    'food',
    'books',
    'toys'
);

CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name TEXT,
    category product_category
);
```

**Complex composite type:**
```sql
CREATE TYPE contact_info AS (
    email TEXT,
    phone TEXT,
    address TEXT
);

CREATE TYPE employee_record AS (
    emp_id INTEGER,
    emp_name TEXT,
    contact contact_info,
    hire_date DATE
);
```

**Range type (PostgreSQL-specific, limited support):**
```sql
-- Integer range
CREATE TYPE int_range AS RANGE (SUBTYPE = integer);

-- Timestamp range
CREATE TYPE ts_range AS RANGE (SUBTYPE = timestamp);
```

### Notes

- ENUM values are ordered in the order they're defined
- ENUM values are case-sensitive
- Composite types can contain other composite types
- Range types are parsed but may not be fully supported in executor
- Types can be used as column types, function parameters, and return types
- DROP TYPE removes a user-defined type

### Related Statements

- [ALTER TYPE](#alter-type-partial)
- [DROP TYPE](#drop-type-partial)
- [CREATE DOMAIN](#create-domain)

---

## PostgreSQL Built-in Types

### Numeric Types

| Type | Storage | Range | Description |
|------|---------|-------|-------------|
| SMALLINT (INT2) | 2 bytes | -32,768 to 32,767 | Small integer |
| INTEGER (INT, INT4) | 4 bytes | -2,147,483,648 to 2,147,483,647 | Typical integer |
| BIGINT (INT8) | 8 bytes | Very large range | Large integer |
| DECIMAL(p,s) | Variable | Up to 131,072 digits | Exact numeric |
| NUMERIC(p,s) | Variable | Up to 131,072 digits | Exact numeric (same as DECIMAL) |
| REAL (FLOAT4) | 4 bytes | 6 decimal digits precision | Single precision float |
| DOUBLE PRECISION (FLOAT8) | 8 bytes | 15 decimal digits precision | Double precision float |
| SERIAL | 4 bytes | 1 to 2,147,483,647 | Auto-incrementing integer |
| BIGSERIAL | 8 bytes | 1 to 9,223,372,036,854,775,807 | Auto-incrementing large integer |
| SMALLSERIAL | 2 bytes | 1 to 32,767 | Auto-incrementing small integer |

### Character Types

| Type | Description |
|------|-------------|
| CHAR(n) | Fixed-length character string |
| VARCHAR(n) | Variable-length with limit |
| TEXT | Variable unlimited length |

### Date/Time Types

| Type | Description |
|------|-------------|
| DATE | Date (year, month, day) |
| TIME | Time of day (without time zone) |
| TIMESTAMP | Date and time (without time zone) |
| TIMESTAMPTZ | Date and time with time zone |
| INTERVAL | Time span |

### Boolean Type

| Type | Values |
|------|--------|
| BOOLEAN | TRUE, FALSE, NULL |

### Binary Type

| Type | Description |
|------|-------------|
| BYTEA | Binary data |

### UUID Type

| Type | Description |
|------|-------------|
| UUID | Universally Unique Identifier |

### JSON Types

| Type | Description |
|------|-------------|
| JSON | Textual JSON data |
| JSONB | Binary JSON (recommended) |

### Array Types

Any type can be made an array by appending `[]`:
```sql
INTEGER[]        -- Array of integers
TEXT[]           -- Array of text
TIMESTAMP[][]    -- Multi-dimensional array
```

---

## Best Practices

### Domain Usage

- Use domains to enforce business rules at the database level
- Create domains for commonly validated types (email, phone, SSN, etc.)
- Document domain constraints clearly
- Use meaningful domain names
- Consider performance impact of complex CHECK expressions

### Type Selection

- Use ENUM for small, fixed sets of values
- Use domains for constrained built-in types
- Use composite types for related data that's always used together
- Prefer JSONB over JSON for better performance
- Use appropriate numeric types (don't use BIGINT when INTEGER suffices)

### Constraint Design

- Make CHECK constraints self-documenting
- Use named constraints for clarity
- Consider using NOT VALID for large table alterations
- Test domain constraints thoroughly before deploying

---

## Known Limitations

### Implemented Features

✅ **CREATE DOMAIN (BASIC)** - Fully implemented with DEFAULT, NOT NULL, and CHECK constraints.

✅ **ALTER DOMAIN** - Supported for all common operations (SET DEFAULT, DROP DEFAULT, ADD/DROP CONSTRAINT, RENAME).

✅ **DROP DOMAIN** - Fully implemented with CASCADE/RESTRICT support.

✅ **CREATE TYPE (ENUM)** - Mapped to domain payloads, working.

✅ **CREATE TYPE (Composite)** - Mapped to domain payloads (RECORD kind).

### Partial Implementation

⚠️ **CREATE TYPE (RANGE)** - Range type family parsed but not fully implemented. SUBTYPE and other range options are accepted but underlying range logic may not be complete.

⚠️ **ALTER TYPE** - Limited support. Adding/renaming enum values may not be fully implemented.

⚠️ **DROP TYPE** - Partially supported.

### Spec Deltas

📝 **Type Mapping** - PostgreSQL CREATE TYPE for ENUMs and composite types is mapped to ScratchBird domain payloads. This provides compatibility but may have subtle behavioral differences from native PostgreSQL types.

📝 **Range Types** - Range type syntax is parsed but the full range type semantics (range operators, functions, etc.) depend on executor support.

---

## See Also

### Related Documentation
- [Tables and Constraints](02_tables_and_constraints.md)
- [Programmable SQL](05_programmable_sql.md)

### Specifications
- `/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `/docs/specifications/types/DDL_DOMAINS_COMPREHENSIVE.md`
- `/docs/audit/17_postgresql_parser_statement_reference_actual.md`

### Source Code
- Parser: `/src/parser/postgresql/pg_parser_ddl.cpp`
- Executor: `/src/sblr/executor.cpp` (EXT_CREATE_DOMAIN, EXT_ALTER_DOMAIN handlers)
