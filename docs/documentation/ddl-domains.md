### DDL: Domains

**What it is**

Domains are user-defined data types based on existing types with optional constraints, defaults, and collations. They act as reusable type templates that encapsulate business rules and validation logic, ensuring consistency across tables and procedures. Think of domains as custom types with built-in validation.

**Why it matters**

- **Consistency**: Apply the same validation rules everywhere the domain is used
- **Maintainability**: Change validation in one place, affects all uses
- **Self-Documentation**: Domain names convey business meaning
- **Type Safety**: Catch invalid data at the database level
- **Reusability**: Share complex type definitions across schemas

**How to use it**

Create domains for common business data types like email addresses, phone numbers, or status codes. Use them in table columns, function parameters, and variables to ensure consistent validation and reduce code duplication.

## CREATE DOMAIN

### Basic Syntax

```sql
CREATE DOMAIN domain_name [AS] base_type
[DEFAULT default_value]
[COLLATE collation_name]
[NOT NULL]
[CHECK (constraint_expression)]
[CHECK (constraint_expression) ...]
```

### Simple Domains

```sql
-- Email domain with validation
CREATE DOMAIN email AS VARCHAR(255)
CHECK (VALUE ~ '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$');

-- Positive integer
CREATE DOMAIN positive_integer AS INTEGER
CHECK (VALUE > 0);

-- Percentage (0-100)
CREATE DOMAIN percentage AS DECIMAL(5,2)
CHECK (VALUE >= 0 AND VALUE <= 100);

-- Phone number
CREATE DOMAIN phone_number AS VARCHAR(20)
CHECK (VALUE ~ '^\+?[1-9]\d{1,14}$');  -- E.164 format

-- US ZIP code
CREATE DOMAIN us_zip_code AS VARCHAR(10)
CHECK (VALUE ~ '^\d{5}(-\d{4})?$');
```

### Domains with Defaults

```sql
-- Status with default
CREATE DOMAIN order_status AS VARCHAR(20)
DEFAULT 'pending'
CHECK (VALUE IN ('pending', 'processing', 'shipped', 'delivered', 'cancelled'));

-- Timestamp with default
CREATE DOMAIN audit_timestamp AS TIMESTAMP
DEFAULT CURRENT_TIMESTAMP
NOT NULL;

-- Boolean with default
CREATE DOMAIN active_flag AS BOOLEAN
DEFAULT TRUE
NOT NULL;

-- Currency with default precision
CREATE DOMAIN money_amount AS DECIMAL(12,2)
DEFAULT 0.00
CHECK (VALUE >= 0);
```

### Domains with Multiple Constraints

```sql
-- Username with multiple rules
CREATE DOMAIN username AS VARCHAR(30)
NOT NULL
CHECK (LENGTH(VALUE) >= 3)
CHECK (VALUE ~ '^[a-zA-Z][a-zA-Z0-9_]*$')
CHECK (LOWER(VALUE) NOT IN ('admin', 'root', 'system'));

-- Product code with complex validation
CREATE DOMAIN product_code AS VARCHAR(20)
NOT NULL
CHECK (VALUE ~ '^[A-Z]{3}-\d{4}-[A-Z0-9]{4}$')
CHECK (VALUE != 'XXX-0000-0000');  -- Reserved code

-- Age with range
CREATE DOMAIN person_age AS INTEGER
CHECK (VALUE >= 0)
CHECK (VALUE <= 150)
CHECK (VALUE IS NOT NULL OR VALUE = 0);  -- 0 means unknown
```

### Domains with Collation

```sql
-- Case-insensitive email
CREATE DOMAIN email_ci AS VARCHAR(255)
COLLATE "unicode_ci"
CHECK (VALUE ~ '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$');

-- Language-specific text
CREATE DOMAIN german_text AS TEXT
COLLATE "de_DE";

-- Binary comparison for codes
CREATE DOMAIN exact_code AS VARCHAR(50)
COLLATE "C"
CHECK (VALUE = UPPER(VALUE));  -- Must be uppercase
```

## Using Domains

### In Table Definitions

```sql
-- Create domains
CREATE DOMAIN email AS VARCHAR(255)
CHECK (VALUE ~ '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$');

CREATE DOMAIN phone AS VARCHAR(20)
CHECK (VALUE ~ '^\+?[1-9]\d{1,14}$');

CREATE DOMAIN positive_money AS DECIMAL(10,2)
CHECK (VALUE > 0);

-- Use in table
CREATE TABLE customers (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email email UNIQUE NOT NULL,        -- Using email domain
    phone phone,                        -- Using phone domain
    credit_limit positive_money         -- Using money domain
);

-- Inserts are validated
INSERT INTO customers (id, name, email, phone, credit_limit)
VALUES (1, 'John Doe', 'john@example.com', '+1234567890', 5000.00);  -- OK

-- This would fail domain constraint
-- INSERT INTO customers (id, name, email, phone, credit_limit)
-- VALUES (2, 'Jane', 'invalid-email', '123', -100);  -- Multiple domain violations
```

### In Function Parameters

```sql
-- Domain for validation
CREATE DOMAIN isbn AS VARCHAR(20)
CHECK (
    -- ISBN-10 or ISBN-13 format
    VALUE ~ '^(97[89])?\d{9}[\dX]$' OR
    VALUE ~ '^(97[89]-?)?\d{1,5}-?\d{1,7}-?\d{1,7}-?[\dX]$'
);

-- Function using domain
CREATE FUNCTION find_book(book_isbn isbn)
RETURNS TABLE(title VARCHAR, author VARCHAR) AS $$
BEGIN
    RETURN QUERY
    SELECT title, author 
    FROM books 
    WHERE isbn = book_isbn;
END;
$$ LANGUAGE plpgsql;

-- Call with valid ISBN
SELECT * FROM find_book('978-0134685991');

-- Call with invalid ISBN fails
-- SELECT * FROM find_book('invalid');  -- Domain constraint violation
```

### In Variables

```sql
-- Domains in PL/pgSQL
CREATE DOMAIN temperature AS DECIMAL(5,2)
CHECK (VALUE >= -273.15);  -- Absolute zero

CREATE FUNCTION convert_temperature(
    temp temperature,
    from_unit CHAR(1),
    to_unit CHAR(1)
) RETURNS temperature AS $$
DECLARE
    result temperature;
BEGIN
    IF from_unit = 'C' AND to_unit = 'F' THEN
        result := temp * 9/5 + 32;
    ELSIF from_unit = 'F' AND to_unit = 'C' THEN
        result := (temp - 32) * 5/9;
    ELSE
        result := temp;
    END IF;
    RETURN result;  -- Automatically validated against domain
END;
$$ LANGUAGE plpgsql;
```

## ALTER DOMAIN

Modify existing domains:

### Add Constraints

```sql
-- Add new CHECK constraint
ALTER DOMAIN email 
ADD CONSTRAINT email_no_temp 
CHECK (VALUE NOT LIKE '%@temp%');

-- Add NOT NULL
ALTER DOMAIN phone_number SET NOT NULL;
```

### Drop Constraints

```sql
-- Drop specific constraint
ALTER DOMAIN email DROP CONSTRAINT email_no_temp;

-- Drop NOT NULL
ALTER DOMAIN phone_number DROP NOT NULL;
```

### Change Default

```sql
-- Set new default
ALTER DOMAIN order_status SET DEFAULT 'new';

-- Remove default
ALTER DOMAIN order_status DROP DEFAULT;
```

### Rename Domain

```sql
-- Rename domain
ALTER DOMAIN old_domain_name RENAME TO new_domain_name;

-- Example
ALTER DOMAIN email RENAME TO email_address;
```

### Validate Constraints

```sql
-- Add constraint without immediate validation
ALTER DOMAIN email 
ADD CONSTRAINT email_lowercase 
CHECK (VALUE = LOWER(VALUE))
NOT VALID;

-- Later, validate existing data
ALTER DOMAIN email VALIDATE CONSTRAINT email_lowercase;
```

### Change Owner

```sql
-- Change domain owner
ALTER DOMAIN email OWNER TO app_admin;
```

## DROP DOMAIN

Remove domains:

```sql
-- Basic drop (fails if in use)
DROP DOMAIN unused_domain;

-- Drop if exists
DROP DOMAIN IF EXISTS temp_domain;

-- Force drop (cascade to dependent objects)
DROP DOMAIN email CASCADE;  -- Drops columns using this domain!

-- Restrict drop (default - fail if dependencies)
DROP DOMAIN important_domain RESTRICT;
```

## Domain Patterns

### Business Rule Domains

```sql
-- Social Security Number
CREATE DOMAIN ssn AS VARCHAR(11)
CHECK (VALUE ~ '^\d{3}-\d{2}-\d{4}$')
CHECK (VALUE NOT IN ('000-00-0000', '666-66-6666'));  -- Invalid SSNs

-- Credit Card (simplified)
CREATE DOMAIN credit_card AS VARCHAR(19)
CHECK (VALUE ~ '^\d{4}[\s-]?\d{4}[\s-]?\d{4}[\s-]?\d{4}$');

-- IP Address
CREATE DOMAIN ip_address AS VARCHAR(45)
CHECK (
    -- IPv4
    VALUE ~ '^(\d{1,3}\.){3}\d{1,3}$' OR
    -- IPv6 (simplified)
    VALUE ~ '^([0-9a-fA-F]{0,4}:){7}[0-9a-fA-F]{0,4}$'
);

-- URL
CREATE DOMAIN url AS VARCHAR(2048)
CHECK (VALUE ~ '^https?://[a-zA-Z0-9][-a-zA-Z0-9._]*(\.[a-zA-Z]{2,})+');
```

### Enumeration Domains

```sql
-- Order priorities
CREATE DOMAIN priority_level AS INTEGER
CHECK (VALUE IN (1, 2, 3, 4, 5))
DEFAULT 3;

-- Days of week
CREATE DOMAIN day_of_week AS INTEGER
CHECK (VALUE BETWEEN 1 AND 7);  -- 1=Monday, 7=Sunday

-- Month names
CREATE DOMAIN month_name AS VARCHAR(10)
CHECK (VALUE IN (
    'January', 'February', 'March', 'April', 'May', 'June',
    'July', 'August', 'September', 'October', 'November', 'December'
));

-- Country codes
CREATE DOMAIN country_code AS CHAR(2)
CHECK (VALUE ~ '^[A-Z]{2}$')
CHECK (VALUE IN (
    'US', 'CA', 'GB', 'FR', 'DE', 'JP', 'CN', 'IN', 'BR', 'AU'
    -- Add more as needed
));
```

### Measurement Domains

```sql
-- Distance in meters
CREATE DOMAIN distance_meters AS DECIMAL(10,2)
CHECK (VALUE >= 0);

-- Weight in kilograms
CREATE DOMAIN weight_kg AS DECIMAL(10,3)
CHECK (VALUE > 0);

-- Currency with symbol
CREATE DOMAIN currency_code AS CHAR(3)
CHECK (VALUE IN ('USD', 'EUR', 'GBP', 'JPY', 'CAD', 'AUD'));

-- Latitude
CREATE DOMAIN latitude AS DECIMAL(10,8)
CHECK (VALUE >= -90 AND VALUE <= 90);

-- Longitude
CREATE DOMAIN longitude AS DECIMAL(11,8)
CHECK (VALUE >= -180 AND VALUE <= 180);
```

### Composite Domain Pattern

```sql
-- Base domains
CREATE DOMAIN street_address AS VARCHAR(200) NOT NULL;
CREATE DOMAIN city_name AS VARCHAR(100) NOT NULL;
CREATE DOMAIN state_code AS CHAR(2) 
CHECK (VALUE ~ '^[A-Z]{2}$');
CREATE DOMAIN postal_code AS VARCHAR(10)
CHECK (VALUE ~ '^\d{5}(-\d{4})?$');

-- Use in structured table
CREATE TABLE addresses (
    id INTEGER PRIMARY KEY,
    street street_address,
    city city_name,
    state state_code,
    zip postal_code,
    country country_code DEFAULT 'US'
);
```

## Domain vs Check Constraints

### When to Use Domains

```sql
-- Good: Reusable validation
CREATE DOMAIN email AS VARCHAR(255)
CHECK (VALUE ~ '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$');

-- Use in multiple places
CREATE TABLE users (email email);
CREATE TABLE customers (contact_email email);
CREATE TABLE newsletters (subscriber_email email);
```

### When to Use Table Constraints

```sql
-- Bad for domain: Table-specific business rule
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    total DECIMAL(10,2),
    discount DECIMAL(10,2),
    -- This is table-specific, not a reusable type
    CHECK (discount <= total * 0.5)  -- Max 50% discount
);
```

## Complex Domain Examples

### Financial Domain Set

```sql
-- Account number with check digit
CREATE DOMAIN account_number AS VARCHAR(20)
CHECK (
    LENGTH(VALUE) = 10 AND
    VALUE ~ '^\d{10}$' AND
    -- Luhn algorithm check (simplified)
    MOD(
        (SUBSTRING(VALUE, 1, 1)::INT * 2) +
        SUBSTRING(VALUE, 2, 1)::INT +
        (SUBSTRING(VALUE, 3, 1)::INT * 2) +
        -- ... continue pattern
        SUBSTRING(VALUE, 10, 1)::INT,
        10
    ) = 0
);

-- IBAN
CREATE DOMAIN iban AS VARCHAR(34)
CHECK (
    VALUE ~ '^[A-Z]{2}\d{2}[A-Z0-9]+$' AND
    LENGTH(VALUE) BETWEEN 15 AND 34
);

-- Currency amount with precision
CREATE DOMAIN precise_money AS DECIMAL(19,4)
CHECK (VALUE >= -999999999999999.9999 AND VALUE <= 999999999999999.9999);
```

### Temporal Domain Set

```sql
-- Business hours
CREATE DOMAIN business_hour AS TIME
CHECK (VALUE >= '08:00:00' AND VALUE <= '18:00:00');

-- Future date
CREATE DOMAIN future_date AS DATE
CHECK (VALUE > CURRENT_DATE);

-- Age-appropriate date
CREATE DOMAIN birth_date AS DATE
CHECK (
    VALUE <= CURRENT_DATE AND
    VALUE >= CURRENT_DATE - INTERVAL '150 years'
);

-- Duration in seconds
CREATE DOMAIN duration_seconds AS INTEGER
CHECK (VALUE >= 0 AND VALUE <= 86400);  -- Max 24 hours
```

## Migration and Refactoring

### Converting Columns to Domains

```sql
-- Before: Repeated constraints
CREATE TABLE old_users (
    email VARCHAR(255) CHECK (email ~ '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$')
);
CREATE TABLE old_customers (
    email VARCHAR(255) CHECK (email ~ '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$')
);

-- After: Using domain
CREATE DOMAIN email AS VARCHAR(255)
CHECK (VALUE ~ '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$');

ALTER TABLE old_users ALTER COLUMN email TYPE email;
ALTER TABLE old_customers ALTER COLUMN email TYPE email;
```

### Domain Versioning

```sql
-- Version 1
CREATE DOMAIN password_v1 AS VARCHAR(100)
CHECK (LENGTH(VALUE) >= 8);

-- Version 2 with stronger requirements
CREATE DOMAIN password_v2 AS VARCHAR(100)
CHECK (
    LENGTH(VALUE) >= 12 AND
    VALUE ~ '[A-Z]' AND  -- Has uppercase
    VALUE ~ '[a-z]' AND  -- Has lowercase
    VALUE ~ '[0-9]' AND  -- Has digit
    VALUE ~ '[!@#$%^&*]' -- Has special char
);

-- Gradual migration
ALTER TABLE users ADD COLUMN new_password password_v2;
-- Migrate data...
ALTER TABLE users DROP COLUMN password;
ALTER TABLE users RENAME COLUMN new_password TO password;
```

## Implementation Details

**Parser Implementation** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_domain`: Handles CREATE/ALTER/DROP DOMAIN
- Captures base type, DEFAULT, CHECK, COLLATE, NOT NULL
- Supports multiple CHECK constraints

**AST Structure** (`include/scratchbird/engine/ast.h`):
```cpp
struct DdlDomainAst {
    std::string name;
    std::string base_type;
    std::string default_value;
    std::vector<std::string> check_constraints;
    std::string collation;
    bool not_null{false};
    std::string action;  // CREATE|ALTER|DROP
};
```

**VALUE Keyword**:
- In domain constraints, VALUE represents the domain value being checked
- Replaced with actual column/variable name at runtime

**Code Anchors**:
- Domain parser: `src/engine/parser_ddl.cpp` (parse_ddl_domain)
- AST definition: `include/scratchbird/engine/ast.h` (DdlDomainAst)
- Constraint handling: Multiple CHECK constraints supported

## See also

- [Data Types](./sql-data-types.md) - Base types for domains
- [Tables](./ddl-tables.md) - Using domains in columns
- [Check Constraints](./ddl-tables.md) - Table-level validation
- [Functions](./psql-routines-and-triggers.md) - Domains in parameters
- [Collations](./ddl-collations-charsets.md) - Text comparison rules