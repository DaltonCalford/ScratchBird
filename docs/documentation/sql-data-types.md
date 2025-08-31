### Data Types and Type Descriptors

**What it is**

ScratchBird implements a comprehensive type system supporting standard SQL types plus advanced features like arrays, ranges, network types, and vector embeddings. The type parser handles complex specifications including precision, scale, character sets, and collations, providing a foundation for both DDL statements and runtime type checking.

**Why it matters**

- **Data Integrity**: Proper type definitions ensure data validity and prevent errors
- **Performance**: Choosing appropriate types optimizes storage and query performance
- **Compatibility**: Standard SQL types ensure portability across database systems
- **Advanced Features**: Specialized types enable modern use cases like JSON, vectors, and network data

**How to use it**

Use this reference to select appropriate data types for your schema. Each type section includes syntax, storage characteristics, and usage examples. The type parser (`src/engine/types.cpp`) handles both simple and complex type specifications.

## Type System Architecture

**Type Descriptor Structure** (`include/scratchbird/engine/types.h`):
```cpp
struct TypeDescriptor {
    TypeKind kind{TypeKind::Unknown};
    int32_t length{-1};      // for CHAR/VARCHAR
    int32_t precision{-1};   // for NUMERIC/DECIMAL/FLOAT/DECFLOAT
    int32_t scale{-1};       // for NUMERIC/DECIMAL
    int32_t vector_dims{-1}; // for VECTOR(n)
    std::string collation;   // for text types
    std::string charset;     // for text types
};
```

## Numeric Types

### Integer Types

| Type | Storage | Range | Description |
|------|---------|-------|-------------|
| `TINYINT` | 1 byte | -128 to 127 | Smallest signed integer |
| `SMALLINT` | 2 bytes | -32,768 to 32,767 | Small range integer |
| `INTEGER`/`INT` | 4 bytes | -2^31 to 2^31-1 | Standard integer |
| `BIGINT` | 8 bytes | -2^63 to 2^63-1 | Large range integer |
| `INT128` | 16 bytes | -2^127 to 2^127-1 | Very large integer |

### Unsigned Integer Types

| Type | Storage | Range | Description |
|------|---------|-------|-------------|
| `UTINYINT` | 1 byte | 0 to 255 | Unsigned tiny integer |
| `USMALLINT` | 2 bytes | 0 to 65,535 | Unsigned small integer |
| `UINTEGER` | 4 bytes | 0 to 2^32-1 | Unsigned standard integer |
| `UBIGINT` | 8 bytes | 0 to 2^64-1 | Unsigned large integer |
| `UINT128` | 16 bytes | 0 to 2^128-1 | Unsigned very large integer |

### Fixed-Point Types

```sql
NUMERIC(precision, scale)
DECIMAL(precision, scale)
```

- **precision**: Total number of digits (1-38)
- **scale**: Digits after decimal point (0-precision)
- Exact arithmetic, no rounding errors
- Ideal for financial calculations

### Floating-Point Types

| Type | Storage | Description |
|------|---------|-------------|
| `FLOAT[(precision)]` | 4 bytes | Single precision, optional precision hint |
| `DOUBLE PRECISION` | 8 bytes | Double precision IEEE 754 |
| `DECFLOAT[(precision)]` | Variable | Decimal floating-point |

### Numeric Type Examples

```sql
CREATE TABLE financial_data (
    -- Integer types
    id BIGINT PRIMARY KEY,
    quantity INTEGER NOT NULL,
    small_code SMALLINT,
    byte_flag TINYINT,
    
    -- Unsigned for non-negative values
    product_count UINTEGER,
    view_count UBIGINT,
    
    -- Fixed-point for money
    price DECIMAL(10,2),        -- Up to 99999999.99
    tax_rate NUMERIC(5,4),      -- Up to 9.9999
    
    -- Floating-point for scientific data
    measurement DOUBLE PRECISION,
    coefficient FLOAT
);

-- Using numeric types
INSERT INTO financial_data (id, quantity, price, tax_rate, measurement)
VALUES (1, 100, 19.99, 0.0875, 3.14159265359);
```

## Text Types

### Character Types

```sql
CHAR(length)           -- Fixed-length, blank-padded
VARCHAR(length)        -- Variable-length
CHARACTER(length)      -- Synonym for CHAR
CHARACTER VARYING(length) -- Synonym for VARCHAR
CITEXT                 -- Case-insensitive text
```

### Character Set and Collation

```sql
type CHARACTER SET charset_name COLLATE collation_name
```

### Text Type Examples

```sql
CREATE TABLE user_profiles (
    -- Fixed-length for codes
    country_code CHAR(2) NOT NULL,
    postal_code CHAR(10),
    
    -- Variable-length for names
    username VARCHAR(50) UNIQUE,
    full_name VARCHAR(200),
    
    -- With charset and collation
    bio VARCHAR(1000) CHARACTER SET UTF8 COLLATE unicode_ci,
    
    -- Case-insensitive
    email CITEXT NOT NULL
);

-- Text operations
INSERT INTO user_profiles (country_code, username, email)
VALUES ('US', 'john_doe', 'John.Doe@Example.Com');

-- CITEXT comparison is case-insensitive
SELECT * FROM user_profiles 
WHERE email = 'john.doe@example.com';  -- Matches despite case difference
```

## Boolean Type

```sql
BOOLEAN  -- TRUE, FALSE, or NULL
```

### Boolean Examples

```sql
CREATE TABLE feature_flags (
    feature_name VARCHAR(100) PRIMARY KEY,
    is_enabled BOOLEAN DEFAULT FALSE,
    is_experimental BOOLEAN,
    requires_auth BOOLEAN NOT NULL
);

-- Boolean literals
INSERT INTO feature_flags VALUES 
    ('dark_mode', TRUE, FALSE, FALSE),
    ('beta_api', FALSE, TRUE, TRUE),
    ('new_ui', NULL, NULL, FALSE);  -- NULL represents unknown

-- Boolean logic
SELECT * FROM feature_flags
WHERE is_enabled = TRUE 
  AND (is_experimental = FALSE OR is_experimental IS NULL);
```

## Binary and Special Types

### Binary Types

```sql
BLOB           -- Binary Large Object
BYTEA          -- Binary data (PostgreSQL-style)
```

### UUID Type

```sql
UUID           -- 128-bit universally unique identifier
```

### JSON Type

```sql
JSON           -- JSON text storage
JSONB          -- Binary JSON (when implemented)
```

### Special Type Examples

```sql
CREATE TABLE documents (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    content BLOB,
    metadata JSON,
    thumbnail BYTEA
);

-- UUID literals
INSERT INTO documents (id, metadata) VALUES 
    (UUID 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11',
     '{"title": "Report", "version": 2}');

-- JSON operations
SELECT id, metadata->>'title' AS title
FROM documents
WHERE metadata->>'version' = '2';
```

## Temporal Types

### Date and Time Types

| Type | Description | Format |
|------|-------------|--------|
| `DATE` | Calendar date | YYYY-MM-DD |
| `TIME` | Time of day | HH:MM:SS[.ffffff] |
| `TIME WITH TIME ZONE` | Time with timezone | HH:MM:SS[.ffffff]±TZ |
| `TIMESTAMP` | Date and time | YYYY-MM-DD HH:MM:SS[.ffffff] |
| `TIMESTAMP WITH TIME ZONE` | Timestamp with timezone | YYYY-MM-DD HH:MM:SS[.ffffff]±TZ |

### Temporal Type Examples

```sql
CREATE TABLE events (
    event_id BIGINT PRIMARY KEY,
    event_date DATE NOT NULL,
    start_time TIME,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    scheduled_at TIMESTAMP WITH TIME ZONE
);

-- Temporal literals
INSERT INTO events VALUES (
    1,
    DATE '2024-03-15',
    TIME '14:30:00',
    TIMESTAMP '2024-01-15 09:30:45.123456',
    TIMESTAMP '2024-03-15 14:30:00+05:30'
);

-- Date/time operations
SELECT * FROM events
WHERE event_date BETWEEN DATE '2024-01-01' AND DATE '2024-12-31'
  AND start_time < TIME '18:00:00';
```

## Network Types

```sql
INET           -- IPv4/IPv6 address with optional netmask
CIDR           -- IPv4/IPv6 network specification
MACADDR        -- MAC address
```

### Network Type Examples

```sql
CREATE TABLE network_devices (
    device_id INTEGER PRIMARY KEY,
    ip_address INET NOT NULL,
    subnet CIDR,
    mac_address MACADDR UNIQUE
);

INSERT INTO network_devices VALUES 
    (1, '192.168.1.100', '192.168.1.0/24', '08:00:27:b0:d0:86'),
    (2, '2001:db8::1', '2001:db8::/32', 'aa:bb:cc:dd:ee:ff'),
    (3, '10.0.0.50/32', '10.0.0.0/8', NULL);

-- Network queries
SELECT * FROM network_devices
WHERE ip_address << CIDR '192.168.0.0/16';  -- Is in subnet
```

## Range Types

Range types represent a range of values with bounds:

```sql
INT4RANGE      -- Range of INTEGER
INT8RANGE      -- Range of BIGINT  
NUMRANGE       -- Range of NUMERIC
DATERANGE      -- Range of DATE
TSRANGE        -- Range of TIMESTAMP
TSTZRANGE      -- Range of TIMESTAMP WITH TIME ZONE
```

### Range Type Examples

```sql
CREATE TABLE room_bookings (
    room_id INTEGER,
    booking_period TSRANGE,
    price_range NUMRANGE,
    EXCLUDE USING gist (room_id WITH =, booking_period WITH &&)
);

-- Range literals: [lower, upper), [lower, upper], (lower, upper), (lower, upper]
INSERT INTO room_bookings VALUES 
    (101, '[2024-01-15 09:00, 2024-01-15 17:00)', '[100, 200)'),
    (102, '[2024-01-15 14:00, 2024-01-16 12:00)', '[150, 250]');

-- Range operations
SELECT * FROM room_bookings
WHERE booking_period && '[2024-01-15 12:00, 2024-01-15 13:00)';  -- Overlaps lunch
```

## Advanced Types

### Geometry Types

```sql
POINT          -- 2D point
```

### Full-Text Search Types

```sql
TSVECTOR       -- Text search document
TSQUERY        -- Text search query
```

### Machine Learning Types

```sql
VECTOR(dimensions)  -- Fixed-dimension vector for embeddings
```

### Advanced Type Examples

```sql
CREATE TABLE ml_documents (
    doc_id BIGINT PRIMARY KEY,
    location POINT,
    search_vector TSVECTOR,
    embedding VECTOR(768)  -- BERT-base embedding dimension
);

-- Vector operations (when implemented)
INSERT INTO ml_documents (doc_id, location, embedding) VALUES 
    (1, POINT(37.7749, -122.4194), '[0.1, 0.2, ...]'::VECTOR(768));

-- Full-text search
UPDATE ml_documents 
SET search_vector = to_tsvector('english', 'Database documentation guide');

SELECT * FROM ml_documents
WHERE search_vector @@ to_tsquery('english', 'database & guide');
```

## Arrays

Any base type can be made into an array by adding brackets:

```sql
type[]         -- One-dimensional array
type[][]       -- Two-dimensional array
```

### Array Examples

```sql
CREATE TABLE product_catalog (
    product_id INTEGER PRIMARY KEY,
    tags VARCHAR(50)[],           -- Array of strings
    prices DECIMAL(10,2)[],       -- Array of prices
    dimensions INTEGER[3],        -- Fixed-size array
    matrix INTEGER[][]           -- 2D array
);

INSERT INTO product_catalog VALUES (
    1,
    ARRAY['electronics', 'mobile', 'smartphone'],
    ARRAY[599.99, 649.99, 699.99],
    ARRAY[150, 70, 8],  -- length, width, height in mm
    ARRAY[[1,2,3], [4,5,6]]
);

-- Array operations
SELECT * FROM product_catalog
WHERE 'mobile' = ANY(tags)
  AND prices[1] < 600.00;
```

## Type Casting

### CAST Syntax

```sql
CAST(expression AS type)
expression::type            -- PostgreSQL-style
```

### TYPE OF

```sql
TYPE OF domain_name
TYPE OF table.column
TYPEOF(expression)
```

### Casting Examples

```sql
-- Explicit casting
SELECT 
    CAST('42' AS INTEGER),
    '3.14'::DECIMAL(5,2),
    CAST('2024-01-15' AS DATE),
    '{"key": "value"}'::JSON;

-- TYPE OF for domain types
CREATE DOMAIN email AS VARCHAR(255) 
    CHECK (VALUE ~ '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$');

CREATE TABLE users (
    user_email email,
    backup_email TYPE OF email  -- Same type as domain
);

-- Dynamic typing
DECLARE VARIABLE v TYPE OF COLUMN users.user_email;
```

## Type Parsing Implementation

The type parser (`src/engine/types.cpp::parse_type_spec`) handles:

1. **Base type recognition**: Case-insensitive matching
2. **Parameter extraction**: Length, precision, scale
3. **Modifier parsing**: CHARACTER SET, COLLATE
4. **Array notation**: Bracket suffixes

### Parser Examples

```cpp
// Input: "VARCHAR(100) CHARACTER SET UTF8 COLLATE unicode_ci"
// Parsed: {kind: VarChar, length: 100, charset: "UTF8", collation: "unicode_ci"}

// Input: "DECIMAL(10,2)"  
// Parsed: {kind: Decimal, precision: 10, scale: 2}

// Input: "VECTOR(1536)"
// Parsed: {kind: Vector, vector_dims: 1536}
```

## Best Practices

### Choosing Numeric Types

```sql
-- Use appropriate integer sizes
CREATE TABLE statistics (
    view_count UBIGINT,        -- Can grow very large
    rating TINYINT CHECK (rating BETWEEN 1 AND 5),  -- Small range
    year SMALLINT CHECK (year BETWEEN 1900 AND 2100)
);

-- Use DECIMAL for money, not FLOAT
CREATE TABLE invoices (
    amount DECIMAL(12,2),      -- Exact decimal arithmetic
    -- NOT: amount FLOAT       -- Would cause rounding errors
);
```

### Text Type Selection

```sql
-- Use VARCHAR for variable data
username VARCHAR(50),          -- Variable usernames

-- Use CHAR for fixed-length codes  
country_code CHAR(2),          -- Always 2 characters

-- Use CITEXT for case-insensitive matching
email CITEXT,                  -- Email comparison ignores case
```

### Temporal Best Practices

```sql
-- Use TIMESTAMP WITH TIME ZONE for global applications
CREATE TABLE user_sessions (
    login_time TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Use DATE for day-level granularity
CREATE TABLE holidays (
    holiday_date DATE PRIMARY KEY
);
```

## Implementation Details

**Source Files**:
- Type definitions: `include/scratchbird/engine/types.h`
- Type parser: `src/engine/types.cpp` (parse_type_spec function)
- Type formatting: `src/engine/types.cpp` (to_string function)
- Expression type parsing: `src/engine/parser_expr.cpp` (parse_type_descriptor)

**Parser Characteristics**:
- Case-insensitive type matching
- Supports both SQL standard and PostgreSQL extensions
- Handles complex specifications with modifiers
- Thread-safe parsing functions

## See also

- [Operators](./sql-operators.md) - Type casting operators
- [Lexical Structure](./sql-lexical.md) - Type literal formats
- [DDL Tables](./ddl-tables.md) - Using types in table definitions
- [Domains](./ddl-domains.md) - Creating custom types