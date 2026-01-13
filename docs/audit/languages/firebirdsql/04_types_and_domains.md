# Firebird SQL - Data Types and Domains

## Overview

Firebird SQL provides a rich set of data types for storing various kinds of information. Additionally, Firebird supports **domains**, which are user-defined data types based on existing data types with optional constraints and default values.

**Key Concepts:**
- **Built-in Data Types**: Numeric, character, date/time, binary, and boolean types
- **Domains**: Reusable, named data types with constraints
- **Type Constraints**: NOT NULL, CHECK, DEFAULT at the domain level
- **Collations**: Character set ordering for text comparison

**Important**: The Firebird parser supports basic domain operations. Extended domain features (ENUM, RECORD, SET, VARIANT) are not available in Firebird emulation mode - these are ScratchBird-native extensions.

---

## Data Types

Firebird supports the following built-in data types:

### Numeric Types

| Type | Description | Range/Precision |
|------|-------------|-----------------|
| SMALLINT | 16-bit integer | -32,768 to 32,767 |
| INTEGER, INT | 32-bit integer | -2,147,483,648 to 2,147,483,647 |
| BIGINT | 64-bit integer | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |
| NUMERIC(p,s) | Exact numeric | Precision p, scale s |
| DECIMAL(p,s) | Exact numeric | Precision p, scale s |
| FLOAT | 32-bit floating point | ~7 digits precision |
| DOUBLE PRECISION | 64-bit floating point | ~15 digits precision |

### Character Types

| Type | Description | Notes |
|------|-------------|-------|
| CHAR(n) | Fixed-length character | Padded with spaces |
| VARCHAR(n) | Variable-length character | Up to n characters |
| CHARACTER(n) | Synonym for CHAR(n) | |
| CHARACTER VARYING(n) | Synonym for VARCHAR(n) | |

### Date and Time Types

| Type | Description | Format |
|------|-------------|--------|
| DATE | Calendar date | YYYY-MM-DD |
| TIME | Time of day | HH:MM:SS.mmmm |
| TIMESTAMP | Date and time | YYYY-MM-DD HH:MM:SS.mmmm |

### Binary Types

| Type | Description | Notes |
|------|-------------|-------|
| BLOB | Binary large object | For large binary or text data |
| BLOB SUB_TYPE TEXT | Text blob | Character data blob |

### Boolean Type

| Type | Description | Values |
|------|-------------|--------|
| BOOLEAN | Logical value | TRUE, FALSE, NULL (unknown) |

### Array Types

Firebird supports array columns:
```sql
column_name data_type[lower:upper, ...]
```

Example:
```sql
quarterly_sales INTEGER[1:4]
```

---

## CREATE DOMAIN

### Description

Creates a domain, which is a user-defined data type based on an existing built-in type. Domains allow you to:
- Define reusable data types with consistent constraints
- Apply the same validation rules across multiple columns
- Centralize type definitions for easier maintenance
- Add semantic meaning to basic types

Domains can include:
- Base data type
- Collation sequence
- NOT NULL constraint
- DEFAULT value
- CHECK constraint

### Syntax

```sql
CREATE DOMAIN domain_name [ AS ] base_type
    [ COLLATE collation_name ]
    [ DEFAULT default_value ]
    [ [ CONSTRAINT constraint_name ] NOT NULL ]
    [ [ CONSTRAINT constraint_name ] CHECK ( condition ) ]
```

### Parameters

- **domain_name**: Name of the domain to create
- **base_type**: The underlying data type (e.g., VARCHAR(100), INTEGER, DECIMAL(10,2))
- **COLLATE**: Collation sequence for character types
- **DEFAULT**: Default value for columns using this domain
- **NOT NULL**: Prevents NULL values
- **CHECK**: Validation constraint (use VALUE keyword to refer to the domain value)

### Examples

#### Simple Domain

```sql
CREATE DOMAIN d_email AS VARCHAR(255);
```

Creates a domain for email addresses.

#### Domain with NOT NULL

```sql
CREATE DOMAIN d_username AS VARCHAR(50) NOT NULL;
```

All columns using this domain will be required (NOT NULL).

#### Domain with Default Value

```sql
CREATE DOMAIN d_status AS VARCHAR(20)
    DEFAULT 'PENDING';
```

Columns using this domain default to 'PENDING' if no value is specified.

#### Domain with CHECK Constraint

```sql
CREATE DOMAIN d_positive_int AS INTEGER
    CHECK (VALUE > 0);
```

Ensures all values are positive. The `VALUE` keyword refers to the actual column value.

#### Domain with Multiple Constraints

```sql
CREATE DOMAIN d_email_address AS VARCHAR(255)
    NOT NULL
    CHECK (VALUE LIKE '%@%');
```

Email addresses must not be null and must contain '@' symbol.

#### Domain with Named Constraint

```sql
CREATE DOMAIN d_age AS SMALLINT
    CONSTRAINT chk_age_range CHECK (VALUE BETWEEN 0 AND 150);
```

Naming constraints makes error messages clearer.

#### Domain for Money

```sql
CREATE DOMAIN d_money AS DECIMAL(12, 2)
    DEFAULT 0.00
    CHECK (VALUE >= 0);
```

A common pattern for monetary values.

#### Domain for Status Enumeration

```sql
CREATE DOMAIN d_order_status AS VARCHAR(20)
    DEFAULT 'NEW'
    CHECK (VALUE IN ('NEW', 'PROCESSING', 'SHIPPED', 'DELIVERED', 'CANCELLED'));
```

Simulates an enumeration by restricting values to a specific set.

#### Domain with Collation

```sql
CREATE DOMAIN d_case_insensitive_name AS VARCHAR(100)
    COLLATE UNICODE_CI;
```

Uses case-insensitive collation for names.

#### Complex Check Constraint

```sql
CREATE DOMAIN d_percentage AS DECIMAL(5, 2)
    CHECK (VALUE BETWEEN 0.00 AND 100.00);
```

#### Domain Using AS Keyword

```sql
CREATE DOMAIN d_product_code AS CHAR(10)
    NOT NULL
    CHECK (VALUE SIMILAR TO '[A-Z]{3}-[0-9]{5}');
```

The AS keyword is optional but makes the syntax clearer.

### Using Domains in Tables

Once created, use domains like regular data types:

```sql
CREATE TABLE users (
    user_id INTEGER PRIMARY KEY,
    username d_username,
    email d_email_address,
    status d_status,
    age d_age,
    credit_limit d_money
);
```

### Usage Notes

1. **Reusability**: Define a domain once, use it in multiple tables. If you need to change the validation rule, change the domain definition.

2. **Constraint Inheritance**: Columns defined with a domain inherit all its constraints. You can add additional column-level constraints, but you cannot relax domain constraints.

3. **VALUE Keyword**: In CHECK constraints, use `VALUE` to refer to the column value being validated.

4. **Default Values**: The domain's DEFAULT is used unless the column specifies its own DEFAULT.

5. **NOT NULL**: If a domain is NOT NULL, all columns using it must be NOT NULL. You cannot make a domain-based column nullable if the domain is NOT NULL.

6. **Performance**: Domains have no performance overhead - they're resolved at DDL time, not during queries.

### Current Implementation Limitations

**Status**: Implemented (basic domains only)

- Basic domains with base types and simple constraints work
- Extended domain kinds (ENUM, RECORD, SET, VARIANT types) are not supported in Firebird emulation mode
- Complex CHECK constraints may not be fully validated during execution

See "Known Limitations" for details.

### Related Features

- [ALTER DOMAIN](#alter-domain) - Modify domain properties
- [DROP DOMAIN](#drop-domain) - Remove a domain
- [CREATE TABLE](02_tables_and_constraints.md#create-table) - Use domains in table definitions

---

## ALTER DOMAIN

### Description

Modifies an existing domain definition. You can:
- Add or drop DEFAULT values
- Add or drop CHECK constraints
- Add or drop NOT NULL constraint
- Rename the domain

**Important**: You cannot change the base data type of a domain using ALTER DOMAIN. To change the type, you must drop and recreate the domain (which requires dropping all dependent columns first).

### Syntax

```sql
ALTER DOMAIN domain_name
{
    SET DEFAULT default_value |
    DROP DEFAULT |
    ADD [ CONSTRAINT constraint_name ] CHECK ( condition ) |
    DROP CONSTRAINT constraint_name |
    ADD [ CONSTRAINT constraint_name ] NOT NULL |
    DROP NOT NULL |
    RENAME TO new_domain_name
}
```

### Parameters

- **SET DEFAULT**: Add or change the default value
- **DROP DEFAULT**: Remove the default value
- **ADD CHECK**: Add a validation constraint
- **DROP CONSTRAINT**: Remove a named constraint
- **ADD NOT NULL**: Make the domain non-nullable
- **DROP NOT NULL**: Allow NULL values
- **RENAME TO**: Change the domain name

### Examples

#### Set Default Value

```sql
ALTER DOMAIN d_status SET DEFAULT 'ACTIVE';
```

#### Drop Default Value

```sql
ALTER DOMAIN d_status DROP DEFAULT;
```

#### Add CHECK Constraint

```sql
ALTER DOMAIN d_age ADD CONSTRAINT chk_adult CHECK (VALUE >= 18);
```

#### Add Named CHECK Constraint

```sql
ALTER DOMAIN d_email_address
    ADD CONSTRAINT chk_email_format CHECK (VALUE LIKE '%@%.%');
```

#### Drop a Constraint

```sql
ALTER DOMAIN d_age DROP CONSTRAINT chk_age_range;
```

You need to know the constraint name to drop it.

#### Add NOT NULL Constraint

```sql
ALTER DOMAIN d_company_name ADD NOT NULL;
```

**Warning**: This will fail if any existing columns using this domain contain NULL values.

#### Drop NOT NULL Constraint

```sql
ALTER DOMAIN d_company_name DROP NOT NULL;
```

Allows columns using this domain to accept NULL values.

#### Rename Domain

```sql
ALTER DOMAIN d_email RENAME TO d_email_address;
```

Changes the domain name. All dependent columns automatically use the new name.

#### Multiple Changes (Require Multiple Statements)

```sql
-- Drop old default and set new one
ALTER DOMAIN d_status DROP DEFAULT;
ALTER DOMAIN d_status SET DEFAULT 'PENDING';

-- Drop old check and add new one
ALTER DOMAIN d_age DROP CONSTRAINT chk_age_range;
ALTER DOMAIN d_age ADD CONSTRAINT chk_adult CHECK (VALUE >= 21);
```

### Usage Notes

1. **Single Operation**: Each ALTER DOMAIN statement performs one operation. Multiple changes require multiple statements.

2. **Existing Data**: When adding constraints (CHECK, NOT NULL), the database validates that existing data satisfies the new constraint. The operation fails if validation fails.

3. **Cannot Change Base Type**: To change from VARCHAR(50) to VARCHAR(100), you must:
   - Create a new domain with the new type
   - Alter all dependent columns to use the new domain
   - Drop the old domain

4. **Constraint Names**: When dropping constraints, you need the constraint name. Query the system catalog to find constraint names if unknown.

5. **Dependencies**: All columns using the domain are automatically affected by alterations.

6. **Default Value Precedence**: Column-level DEFAULT overrides domain DEFAULT. Changing domain DEFAULT doesn't affect columns with explicit defaults.

### Impact on Dependent Columns

When you alter a domain, the change affects all columns using that domain:

```sql
-- Create domain and table
CREATE DOMAIN d_status AS VARCHAR(20) DEFAULT 'NEW';
CREATE TABLE orders (
    order_id INTEGER,
    status d_status
);

-- Change domain default
ALTER DOMAIN d_status SET DEFAULT 'PENDING';

-- New rows now use 'PENDING' as default
INSERT INTO orders (order_id) VALUES (1);  -- status = 'PENDING'
```

### Current Implementation Limitations

**Status**: Implemented (limited actions)

Supported operations:
- SET DEFAULT / DROP DEFAULT
- ADD CHECK / DROP CONSTRAINT
- RENAME TO

Check the "Known Limitations" section for any restrictions.

### Related Features

- [CREATE DOMAIN](#create-domain) - Create a new domain
- [DROP DOMAIN](#drop-domain) - Remove a domain

---

## DROP DOMAIN

### Description

Removes a domain from the database. The domain must not be in use by any table columns.

### Syntax

```sql
DROP DOMAIN domain_name
```

### Parameters

- **domain_name**: The name of the domain to drop

### Examples

#### Drop a Domain

```sql
DROP DOMAIN d_email_address;
```

#### Drop Multiple Domains

```sql
DROP DOMAIN d_old_status;
DROP DOMAIN d_deprecated_type;
DROP DOMAIN d_unused_domain;
```

#### Drop After Removing Dependencies

```sql
-- First, alter tables to stop using the domain
ALTER TABLE users ALTER COLUMN email TYPE VARCHAR(255);
ALTER TABLE contacts ALTER COLUMN email TYPE VARCHAR(255);

-- Then drop the domain
DROP DOMAIN d_email;
```

### Usage Notes

1. **Dependencies**: You cannot drop a domain if any columns are using it. You'll get an error:
   ```
   Cannot delete DOMAIN D_EMAIL
   There are <n> dependencies
   ```

2. **No CASCADE**: Firebird doesn't support `DROP DOMAIN ... CASCADE`. You must manually alter or drop all dependent columns first.

3. **Find Dependencies**: Query the system catalog to find which columns use a domain:
   ```sql
   SELECT
       r.RDB$RELATION_NAME AS table_name,
       f.RDB$FIELD_NAME AS column_name
   FROM RDB$RELATION_FIELDS f
   JOIN RDB$RELATIONS r ON f.RDB$RELATION_NAME = r.RDB$RELATION_NAME
   JOIN RDB$FIELDS fld ON f.RDB$FIELD_SOURCE = fld.RDB$FIELD_NAME
   WHERE fld.RDB$FIELD_NAME = 'D_EMAIL'
   AND r.RDB$RELATION_TYPE = 0;  -- User tables only
   ```

4. **Replace Domain**: To replace a domain:
   ```sql
   -- Create new domain
   CREATE DOMAIN d_email_v2 AS VARCHAR(320);  -- Updated to new spec

   -- Alter columns to use new domain
   ALTER TABLE users ALTER COLUMN email TYPE d_email_v2;

   -- Drop old domain
   DROP DOMAIN d_email;

   -- Optionally rename new domain
   ALTER DOMAIN d_email_v2 RENAME TO d_email;
   ```

5. **System Domains**: You cannot drop system-defined domains (those starting with RDB$).

### Safety Considerations

Before dropping a domain:

1. Verify no columns are using it
2. Check for any application code references
3. Consider creating a backup
4. Document why the domain is being removed

### Current Implementation Limitations

**Status**: Implemented

DROP DOMAIN works as expected for basic domains.

See "Known Limitations" for any restrictions.

### Related Features

- [CREATE DOMAIN](#create-domain) - Create a new domain
- [ALTER DOMAIN](#alter-domain) - Modify a domain

---

## CREATE TYPE

### Description

Firebird SQL does not support `CREATE TYPE` as a standalone DDL statement. This is a dialect design decision.

**Status**: Not available (by dialect design)

### Background

Unlike PostgreSQL or Oracle, Firebird does not have:
- CREATE TYPE for composite types
- CREATE TYPE for enumeration types
- CREATE TYPE for range types
- CREATE TYPE for object types

### Firebird Alternatives

**For Type Aliases**: Use `CREATE DOMAIN`
```sql
-- Instead of CREATE TYPE email_type AS VARCHAR(255)
CREATE DOMAIN email_type AS VARCHAR(255);
```

**For Enumerations**: Use domains with CHECK constraints
```sql
CREATE DOMAIN status_type AS VARCHAR(20)
    CHECK (VALUE IN ('ACTIVE', 'INACTIVE', 'PENDING'));
```

**For Composite Types**: Not supported; use tables instead
```sql
-- Instead of composite type, use a table
CREATE TABLE address_type (
    address_id INTEGER PRIMARY KEY,
    street VARCHAR(100),
    city VARCHAR(50),
    state VARCHAR(2),
    zip VARCHAR(10)
);
```

### ScratchBird Extensions

ScratchBird's native V2 parser supports extended type creation including:
- ENUM types
- RECORD types
- SET types
- VARIANT types

However, these are **not available in Firebird emulation mode**. When using Firebird SQL dialect, stick to standard Firebird features (domains and built-in types).

### Related Features

- [CREATE DOMAIN](#create-domain) - Firebird's mechanism for custom types

---

## Known Limitations

### Implemented

**CREATE DOMAIN**
- Basic domain creation works correctly
- Supports base types, DEFAULT, NOT NULL, CHECK constraints, and COLLATE
- Constraint validation during domain operations is functional
- **Not Supported**: Extended domain kinds (ENUM, RECORD, SET, VARIANT) - these are ScratchBird-native features not available in Firebird emulation mode

**ALTER DOMAIN**
- SET DEFAULT / DROP DEFAULT operations work
- ADD CHECK / DROP CONSTRAINT operations work
- RENAME TO operation works
- **Not Supported**: Cannot change base data type (by design - you must drop and recreate)
- **Partial**: ADD NOT NULL / DROP NOT NULL status should be verified

**DROP DOMAIN**
- Works correctly for domains not in use
- Properly prevents dropping domains with dependencies
- No CASCADE option (by design - must manually handle dependencies)

### Missing Features

**CREATE TYPE**
- Not available by dialect design (Firebird doesn't support CREATE TYPE)
- Use CREATE DOMAIN instead for type aliases
- Workaround: Use domains with constraints for enumeration-like behavior

**Extended Domain Types**
- ENUM domains - not supported in Firebird emulation (ScratchBird-native only)
- RECORD domains - not supported in Firebird emulation
- SET domains - not supported in Firebird emulation
- VARIANT domains - not supported in Firebird emulation
- These extended types are documented in `/home/dcalford/CliWork/ScratchBird/docs/specifications/types/DDL_DOMAINS_COMPREHENSIVE.md` but are exclusive to ScratchBird's native V2 parser

### Specification Deltas

**Firebird Emulation vs Native**

When using Firebird emulation mode:
- Only traditional Firebird domain features are available
- Base types: numeric, character, date/time, blob, boolean, array
- Constraints: DEFAULT, NOT NULL, CHECK (with VALUE keyword)
- Operations: CREATE/ALTER/DROP DOMAIN

When using ScratchBird native V2 parser:
- All Firebird features plus extensions
- Additional type kinds: ENUM, RECORD, SET, VARIANT
- Enhanced domain capabilities
- Richer type system

**Domain Constraint Enforcement**

- Parser accepts all constraint syntax
- V2 pipeline validates and enforces constraints
- Some complex CHECK constraints may not be fully validated at runtime
- Constraint errors should provide clear messages with constraint names

### Workarounds

**For Enumeration Types**:
```sql
-- Use domain with CHECK constraint instead of ENUM
CREATE DOMAIN d_gender AS CHAR(1)
    CHECK (VALUE IN ('M', 'F', 'O'));

CREATE DOMAIN d_priority AS VARCHAR(10)
    CHECK (VALUE IN ('LOW', 'MEDIUM', 'HIGH', 'URGENT'));
```

**For Complex Types**:
```sql
-- Use tables instead of composite types
CREATE TABLE address (
    address_id INTEGER PRIMARY KEY,
    street VARCHAR(100),
    city VARCHAR(50),
    state CHAR(2),
    zip VARCHAR(10)
);

-- Reference from other tables
CREATE TABLE customers (
    customer_id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    billing_address_id INTEGER REFERENCES address(address_id),
    shipping_address_id INTEGER REFERENCES address(address_id)
);
```

**For Changing Domain Type**:
```sql
-- Cannot use ALTER DOMAIN to change type
-- Instead:
-- 1. Create new domain
CREATE DOMAIN d_email_v2 AS VARCHAR(320);

-- 2. Alter all dependent columns
ALTER TABLE users ALTER COLUMN email TYPE d_email_v2;
ALTER TABLE contacts ALTER COLUMN email TYPE d_email_v2;

-- 3. Drop old domain
DROP DOMAIN d_email;

-- 4. Rename new domain (optional)
ALTER DOMAIN d_email_v2 RENAME TO d_email;
```

### Best Practices

1. **Name Domains Clearly**: Use prefixes like `d_` to distinguish domains from tables
2. **Document Constraints**: Use named constraints with clear, descriptive names
3. **Centralize Validation**: Put business rules in domain CHECKs rather than table CHECKs when the rule applies across multiple tables
4. **Version Domain Names**: When changing a domain significantly, consider creating a new domain (d_email_v2) rather than altering the existing one
5. **Check Dependencies**: Always query system catalog before dropping domains

### Specification References

- `/home/dcalford/CliWork/ScratchBird/docs/specifications/reference/firebird/`
- `/home/dcalford/CliWork/ScratchBird/docs/specifications/types/DDL_DOMAINS_COMPREHENSIVE.md`
- `/home/dcalford/CliWork/ScratchBird/docs/audit/16_firebird_parser_statement_reference_actual.md`
