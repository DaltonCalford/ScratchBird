[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL Data Types and Domains

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

This document covers data types available in MySQL emulation mode. MySQL provides a rich set of data types for storing integers, decimals, strings, dates, binary data, and specialized types like JSON and spatial data.

**Important:** MySQL does not support user-defined domains or custom types like PostgreSQL. All type definitions must use built-in MySQL types.

---

## Data Type Categories

MySQL data types are organized into several categories:

1. **Numeric Types**: Integers, decimals, floating-point
2. **String Types**: Character strings, binary strings
3. **Date and Time Types**: Temporal data
4. **Binary Types**: BLOB types for binary data
5. **Boolean Type**: TRUE/FALSE values
6. **Spatial Types**: Geographic and geometric data
7. **JSON Type**: JSON document storage
8. **Special Types**: ENUM, SET

---

## Numeric Types

### Integer Types

MySQL provides several integer types with different storage sizes and value ranges.

#### TINYINT

Smallest integer type, 1 byte storage.

**Syntax:**
```sql
TINYINT[(M)] [UNSIGNED] [ZEROFILL]
```

**Range:**
- Signed: -128 to 127
- Unsigned: 0 to 255

**Examples:**
```sql
CREATE TABLE example (
    age TINYINT,
    count TINYINT UNSIGNED
);
```

#### SMALLINT

Small integer type, 2 bytes storage.

**Syntax:**
```sql
SMALLINT[(M)] [UNSIGNED] [ZEROFILL]
```

**Range:**
- Signed: -32,768 to 32,767
- Unsigned: 0 to 65,535

**Examples:**
```sql
CREATE TABLE example (
    port_number SMALLINT UNSIGNED,
    temperature SMALLINT
);
```

#### MEDIUMINT

Medium integer type, 3 bytes storage.

**Syntax:**
```sql
MEDIUMINT[(M)] [UNSIGNED] [ZEROFILL]
```

**Range:**
- Signed: -8,388,608 to 8,388,607
- Unsigned: 0 to 16,777,215

**Examples:**
```sql
CREATE TABLE example (
    population MEDIUMINT UNSIGNED
);
```

#### INT / INTEGER

Standard integer type, 4 bytes storage.

**Syntax:**
```sql
INT[(M)] [UNSIGNED] [ZEROFILL]
INTEGER[(M)] [UNSIGNED] [ZEROFILL]
```

**Range:**
- Signed: -2,147,483,648 to 2,147,483,647
- Unsigned: 0 to 4,294,967,295

**Examples:**
```sql
CREATE TABLE users (
    user_id INT PRIMARY KEY,
    views INT UNSIGNED DEFAULT 0
);
```

#### BIGINT

Largest integer type, 8 bytes storage.

**Syntax:**
```sql
BIGINT[(M)] [UNSIGNED] [ZEROFILL]
```

**Range:**
- Signed: -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
- Unsigned: 0 to 18,446,744,073,709,551,615

**Examples:**
```sql
CREATE TABLE statistics (
    total_bytes BIGINT UNSIGNED,
    offset_value BIGINT
);
```

### Decimal Types

#### DECIMAL / NUMERIC

Exact fixed-point numeric type.

**Syntax:**
```sql
DECIMAL[(M[,D])] [UNSIGNED] [ZEROFILL]
NUMERIC[(M[,D])] [UNSIGNED] [ZEROFILL]
```

**Parameters:**
- `M`: Total number of digits (precision), default 10, max 65
- `D`: Number of digits after decimal point (scale), default 0, max 30

**Examples:**
```sql
CREATE TABLE products (
    price DECIMAL(10, 2),           -- Up to 99,999,999.99
    tax_rate DECIMAL(5, 4),         -- Up to 9.9999
    weight DECIMAL(8, 3) UNSIGNED   -- Up to 99,999.999
);
```

**Notes:**
- Use DECIMAL for financial calculations requiring exact precision
- `NUMERIC` is a synonym for `DECIMAL`
- Values are stored exactly as specified

### Floating-Point Types

#### FLOAT

Single-precision floating-point number.

**Syntax:**
```sql
FLOAT[(M,D)] [UNSIGNED] [ZEROFILL]
FLOAT(p)
```

**Parameters:**
- `M,D`: Display width and decimals (deprecated in MySQL 8.0.17+)
- `p`: Precision in bits (0-24 = FLOAT, 25-53 = DOUBLE)

**Examples:**
```sql
CREATE TABLE measurements (
    temperature FLOAT,
    latitude FLOAT(10, 6),
    longitude FLOAT(10, 6)
);
```

**Notes:**
- Approximate values (may have rounding errors)
- 4 bytes storage
- Use for scientific calculations where approximation is acceptable

#### DOUBLE / REAL

Double-precision floating-point number.

**Syntax:**
```sql
DOUBLE[(M,D)] [UNSIGNED] [ZEROFILL]
DOUBLE PRECISION[(M,D)] [UNSIGNED] [ZEROFILL]
REAL[(M,D)] [UNSIGNED] [ZEROFILL]
```

**Examples:**
```sql
CREATE TABLE scientific_data (
    measurement DOUBLE,
    precision_value DOUBLE PRECISION,
    calculation REAL
);
```

**Notes:**
- 8 bytes storage
- More precise than FLOAT
- Still approximate (use DECIMAL for exact values)

---

## String Types

### Character String Types

#### CHAR

Fixed-length character string.

**Syntax:**
```sql
CHAR[(M)] [CHARACTER SET charset] [COLLATE collation]
```

**Parameters:**
- `M`: Length in characters (0-255), default 1

**Examples:**
```sql
CREATE TABLE codes (
    country_code CHAR(2),           -- Always 2 characters
    state_code CHAR(2),
    fixed_id CHAR(10)
);
```

**Notes:**
- Right-padded with spaces to specified length
- Trailing spaces removed on retrieval (by default)
- Efficient for fixed-length data

#### VARCHAR

Variable-length character string.

**Syntax:**
```sql
VARCHAR(M) [CHARACTER SET charset] [COLLATE collation]
```

**Parameters:**
- `M`: Maximum length in characters (0-65,535)

**Examples:**
```sql
CREATE TABLE users (
    username VARCHAR(50),
    email VARCHAR(255),
    bio VARCHAR(500)
);
```

**Notes:**
- Only stores actual length + length prefix (1-2 bytes)
- More space-efficient than CHAR for variable data
- Most commonly used string type

#### TEXT Types

Variable-length text storage for longer strings.

**Syntax:**
```sql
TINYTEXT [CHARACTER SET charset] [COLLATE collation]
TEXT[(M)] [CHARACTER SET charset] [COLLATE collation]
MEDIUMTEXT [CHARACTER SET charset] [COLLATE collation]
LONGTEXT [CHARACTER SET charset] [COLLATE collation]
```

**Sizes:**
- `TINYTEXT`: Up to 255 bytes
- `TEXT`: Up to 65,535 bytes (64 KB)
- `MEDIUMTEXT`: Up to 16,777,215 bytes (16 MB)
- `LONGTEXT`: Up to 4,294,967,295 bytes (4 GB)

**Examples:**
```sql
CREATE TABLE articles (
    summary TINYTEXT,
    content TEXT,
    full_document MEDIUMTEXT,
    raw_data LONGTEXT
);
```

**Notes:**
- Cannot have DEFAULT values (except NULL)
- Cannot be used in indexes without prefix length
- Stored outside the table row (in MySQL)

### Binary String Types

#### BINARY

Fixed-length binary string.

**Syntax:**
```sql
BINARY[(M)]
```

**Parameters:**
- `M`: Length in bytes (0-255), default 1

**Examples:**
```sql
CREATE TABLE binary_data (
    hash BINARY(32),        -- Fixed 32-byte hash
    token BINARY(16)        -- Fixed 16-byte token
);
```

#### VARBINARY

Variable-length binary string.

**Syntax:**
```sql
VARBINARY(M)
```

**Parameters:**
- `M`: Maximum length in bytes (0-65,535)

**Examples:**
```sql
CREATE TABLE encrypted (
    encrypted_data VARBINARY(1000),
    salt VARBINARY(32)
);
```

#### BLOB Types

Binary Large OBject storage.

**Syntax:**
```sql
TINYBLOB
BLOB[(M)]
MEDIUMBLOB
LONGBLOB
```

**Sizes:**
- `TINYBLOB`: Up to 255 bytes
- `BLOB`: Up to 65,535 bytes (64 KB)
- `MEDIUMBLOB`: Up to 16,777,215 bytes (16 MB)
- `LONGBLOB`: Up to 4,294,967,295 bytes (4 GB)

**Examples:**
```sql
CREATE TABLE files (
    thumbnail BLOB,
    image MEDIUMBLOB,
    video LONGBLOB
);
```

---

## Date and Time Types

### DATE

Stores date values without time.

**Syntax:**
```sql
DATE
```

**Format:** 'YYYY-MM-DD'

**Range:** '1000-01-01' to '9999-12-31'

**Examples:**
```sql
CREATE TABLE events (
    birth_date DATE,
    hire_date DATE,
    expiry_date DATE
);

INSERT INTO events (birth_date) VALUES ('2000-01-15');
```

### TIME

Stores time values without date.

**Syntax:**
```sql
TIME[(fsp)]
```

**Parameters:**
- `fsp`: Fractional seconds precision (0-6), default 0

**Format:** 'HH:MM:SS[.fraction]'

**Range:** '-838:59:59.000000' to '838:59:59.000000'

**Examples:**
```sql
CREATE TABLE schedules (
    start_time TIME,
    duration TIME(3),       -- With milliseconds
    offset TIME
);

INSERT INTO schedules (start_time) VALUES ('14:30:00');
```

### DATETIME

Stores date and time together.

**Syntax:**
```sql
DATETIME[(fsp)]
```

**Parameters:**
- `fsp`: Fractional seconds precision (0-6), default 0

**Format:** 'YYYY-MM-DD HH:MM:SS[.fraction]'

**Range:** '1000-01-01 00:00:00.000000' to '9999-12-31 23:59:59.999999'

**Examples:**
```sql
CREATE TABLE logs (
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME(6),         -- With microseconds
    processed_at DATETIME
);

INSERT INTO logs (created_at) VALUES ('2024-01-15 14:30:45');
```

### TIMESTAMP

Stores date and time with automatic timezone conversion.

**Syntax:**
```sql
TIMESTAMP[(fsp)]
```

**Parameters:**
- `fsp`: Fractional seconds precision (0-6), default 0

**Range:** '1970-01-01 00:00:01.000000' UTC to '2038-01-19 03:14:07.999999' UTC

**Examples:**
```sql
CREATE TABLE audit (
    created TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);
```

**Notes:**
- Stored as UTC, converted to session timezone on retrieval
- Automatically updated on row modification (with ON UPDATE)
- Subject to 2038 problem (use DATETIME for dates beyond 2038)

### YEAR

Stores year values.

**Syntax:**
```sql
YEAR[(4)]
```

**Range:** 1901 to 2155, or 0000

**Examples:**
```sql
CREATE TABLE vehicles (
    manufacture_year YEAR
);

INSERT INTO vehicles (manufacture_year) VALUES (2024);
```

---

## Boolean Type

### BOOL / BOOLEAN

Synonym for TINYINT(1).

**Syntax:**
```sql
BOOL
BOOLEAN
```

**Examples:**
```sql
CREATE TABLE settings (
    is_active BOOLEAN DEFAULT TRUE,
    is_deleted BOOL DEFAULT FALSE,
    has_permission BOOLEAN
);

INSERT INTO settings (is_active) VALUES (TRUE);
INSERT INTO settings (is_active) VALUES (1);  -- Same as TRUE
```

**Notes:**
- TRUE is synonym for 1
- FALSE is synonym for 0
- Stored as TINYINT(1)

---

## Special Types

### ENUM

String object that can have one value from a list of permitted values.

**Syntax:**
```sql
ENUM('value1', 'value2', ...) [CHARACTER SET charset] [COLLATE collation]
```

**Examples:**
```sql
CREATE TABLE orders (
    order_id INT PRIMARY KEY,
    status ENUM('pending', 'processing', 'shipped', 'delivered', 'cancelled')
);

INSERT INTO orders (order_id, status) VALUES (1, 'pending');
INSERT INTO orders (order_id, status) VALUES (2, 'shipped');
```

**Notes:**
- Values stored as integers internally (1, 2, 3, ...)
- Space-efficient for columns with limited values
- Can have up to 65,535 distinct values
- Empty string and NULL are special cases

### SET

String object that can have zero or more values from a list.

**Syntax:**
```sql
SET('value1', 'value2', ...) [CHARACTER SET charset] [COLLATE collation]
```

**Examples:**
```sql
CREATE TABLE permissions (
    user_id INT PRIMARY KEY,
    roles SET('admin', 'editor', 'viewer', 'moderator')
);

INSERT INTO permissions VALUES (1, 'admin,editor');
INSERT INTO permissions VALUES (2, 'viewer');
```

**Notes:**
- Stored as a bitmap
- Can have up to 64 distinct members
- Values can be combined

### JSON

Stores JSON documents.

**Syntax:**
```sql
JSON
```

**Examples:**
```sql
CREATE TABLE documents (
    doc_id INT PRIMARY KEY,
    metadata JSON,
    data JSON
);

INSERT INTO documents (doc_id, metadata) VALUES
    (1, '{"author": "John", "tags": ["tech", "mysql"]}');

-- Query JSON data:
SELECT doc_id, metadata->'$.author' AS author FROM documents;
```

**Notes:**
- Validates JSON on insert
- Supports JSON functions and operators
- Binary storage format (more efficient than TEXT)
- Supports indexing with generated columns

---

## Spatial Types

MySQL supports spatial data types for geographic information systems (GIS).

### GEOMETRY

Base type for spatial data.

**Syntax:**
```sql
GEOMETRY
POINT
LINESTRING
POLYGON
MULTIPOINT
MULTILINESTRING
MULTIPOLYGON
GEOMETRYCOLLECTION
```

**Examples:**
```sql
CREATE TABLE locations (
    id INT PRIMARY KEY,
    position POINT,
    boundary POLYGON,
    route LINESTRING
);

-- Insert point:
INSERT INTO locations (id, position)
VALUES (1, ST_GeomFromText('POINT(40.7128 -74.0060)'));
```

**Notes:**
- Requires spatial functions for manipulation
- Can be indexed with SPATIAL indexes
- Coordinate systems supported

---

## Type Modifiers

### UNSIGNED

Specifies that a numeric type cannot hold negative values.

**Examples:**
```sql
CREATE TABLE stats (
    count INT UNSIGNED,
    total BIGINT UNSIGNED
);
```

**Notes:**
- Doubles the positive range
- Parsed but constraint not currently enforced (see Known Limitations)

### ZEROFILL

Pads displayed values with leading zeros.

**Examples:**
```sql
CREATE TABLE formatted (
    id INT(5) ZEROFILL,     -- Displays as 00001, 00002, etc.
    code INT(8) ZEROFILL
);
```

**Notes:**
- Implies UNSIGNED
- Affects display only, not storage
- Parsed but not currently implemented (see Known Limitations)

---

## Character Sets and Collations

### Character Set

Defines the character encoding for string data.

**Common Character Sets:**
- `utf8mb4`: Full UTF-8 support (recommended)
- `utf8`: UTF-8 (deprecated, limited to 3 bytes)
- `latin1`: Western European
- `ascii`: US ASCII

**Examples:**
```sql
CREATE TABLE multilingual (
    id INT PRIMARY KEY,
    content VARCHAR(1000) CHARACTER SET utf8mb4
);
```

### Collation

Defines string comparison rules.

**Common Collations:**
- `utf8mb4_unicode_ci`: Unicode, case-insensitive
- `utf8mb4_0900_ai_ci`: Latest Unicode, accent/case-insensitive
- `utf8mb4_bin`: Binary (case-sensitive)

**Examples:**
```sql
CREATE TABLE data (
    id INT PRIMARY KEY,
    name VARCHAR(100) COLLATE utf8mb4_unicode_ci
);
```

---

## Type Conversion

### CAST

Converts a value to a specific type.

**Syntax:**
```sql
CAST(expression AS type)
```

**Examples:**
```sql
SELECT CAST('123' AS SIGNED);           -- String to integer
SELECT CAST(123.45 AS CHAR);            -- Number to string
SELECT CAST('2024-01-15' AS DATE);      -- String to date
SELECT CAST(NOW() AS TIME);             -- Datetime to time
```

### CONVERT

Alternative syntax for type conversion.

**Syntax:**
```sql
CONVERT(expression, type)
CONVERT(expression USING charset)
```

**Examples:**
```sql
SELECT CONVERT('123', SIGNED);
SELECT CONVERT(name USING utf8mb4) FROM users;
```

---

## Domains

### Overview

MySQL does not support user-defined domains like PostgreSQL does.

**PostgreSQL domain equivalent (NOT SUPPORTED):**
```sql
-- This does NOT work in MySQL:
CREATE DOMAIN email_address AS VARCHAR(255)
    CHECK (VALUE LIKE '%@%');
```

**MySQL alternative using CHECK constraints:**
```sql
CREATE TABLE users (
    id INT PRIMARY KEY,
    email VARCHAR(255) CHECK (email LIKE '%@%')
);
```

### Notes

- Use CHECK constraints on columns instead of domains
- Use ENUM for restricted value sets
- Consider application-level validation

---

## Known Limitations

### Missing Features

- **CREATE DOMAIN**: Not supported by MySQL dialect (by design)
  - **Workaround**: Use CHECK constraints on individual columns

- **ALTER DOMAIN**: Not supported by MySQL dialect (by design)

- **DROP DOMAIN**: Not supported by MySQL dialect (by design)

- **CREATE TYPE**: Not supported by MySQL dialect (by design)
  - **Workaround**: Use ENUM or SET types for custom value sets

### Partial Implementation

- **UNSIGNED modifier**: Parsed and stored internally but NOT emitted to bytecode
  - Constraint is not enforced
  - Should generate `CHECK (column >= 0)` constraint
  - **Status**: Requires implementation

- **ZEROFILL modifier**: Parsed but NOT implemented
  - Display formatting is not applied
  - Should add formatting metadata to catalog
  - **Status**: Requires implementation

### Spec Deltas

- **Type storage**: ScratchBird may use different internal representations than MySQL
- **Type limits**: Some type size limits may differ from MySQL
- **Collation behavior**: Collation support may not be complete for all character sets

### Implementation Priority

According to `/docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md`:

**Post-Alpha (High Priority):**
- Implement UNSIGNED as CHECK constraint (1-2 days)

**Beta Target (Medium Priority):**
- Implement ZEROFILL formatting (2-3 days)
- Full character set/collation support
- Spatial type operations
