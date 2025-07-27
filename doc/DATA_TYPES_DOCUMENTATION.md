# ScratchBird DATA TYPES - Complete Reference

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

ScratchBird provides a comprehensive type system that extends traditional SQL data types with advanced features for modern applications. The type system includes standard SQL types, unsigned integer extensions, network address types, range types, geometric types, and AI/ML support.

### Key Features and Capabilities

- **Standard SQL Compliance**: Full support for SQL-92/99/2003 data types
- **Unsigned Integer Types**: USMALLINT, UINTEGER, UBIGINT, UINT128
- **Enhanced Text Types**: Large VARCHAR (128KB), case-insensitive text
- **Network Types**: INET, CIDR, MACADDR for network programming
- **Range Types**: Integer, numeric, date, and timestamp ranges
- **Geometric Types**: Points and spatial data
- **AI/ML Types**: Vector types for machine learning applications
- **Full-Text Search**: TSVECTOR and TSQUERY for text search
- **Array Support**: Multi-dimensional arrays with slice operations

### ScratchBird-Specific Enhancements

1. **Extended Size Limits**: VARCHAR up to 128KB (131,072 bytes)
2. **Unsigned Integer Family**: Complete set of unsigned integer types
3. **Network Programming**: Native IPv4/IPv6 and MAC address support
4. **Modern Data Types**: JSON, UUID, and vector types
5. **Range Operations**: PostgreSQL-style range types
6. **Case-Insensitive Text**: CITEXT for locale-aware comparisons

---

## Numeric Data Types

### Signed Integer Types

#### **SMALLINT**
16-bit signed integer (-32,768 to 32,767)
```sql
CREATE TABLE test_smallint (
    id SMALLINT,
    temperature SMALLINT CHECK (temperature BETWEEN -50 AND 50)
);

INSERT INTO test_smallint VALUES (1, 25);
INSERT INTO test_smallint VALUES (2, -10);
```

#### **INTEGER**
32-bit signed integer (-2,147,483,648 to 2,147,483,647)
```sql
CREATE TABLE test_integer (
    record_id INTEGER PRIMARY KEY,
    population INTEGER CHECK (population >= 0)
);

INSERT INTO test_integer VALUES (1, 1500000);
```

#### **BIGINT**
64-bit signed integer (-9,223,372,036,854,775,808 to 9,223,372,036,854,775,807)
```sql
CREATE TABLE test_bigint (
    transaction_id BIGINT,
    total_bytes BIGINT DEFAULT 0
);

INSERT INTO test_bigint VALUES (1234567890123, 9876543210987654321);
```

#### **INT128**
128-bit signed integer (very large integer values)
```sql
CREATE TABLE test_int128 (
    huge_number INT128,
    calculation_result INT128
);

-- Useful for cryptographic calculations and scientific computing
INSERT INTO test_int128 VALUES (340282366920938463463374607431768211455, 0);
```

### Unsigned Integer Types (ScratchBird Extension)

#### **USMALLINT**
16-bit unsigned integer (0 to 65,535)
```sql
CREATE TABLE test_usmallint (
    port_number USMALLINT CHECK (port_number BETWEEN 1 AND 65535),
    status_code USMALLINT
);

INSERT INTO test_usmallint VALUES (8080, 200);
INSERT INTO test_usmallint VALUES (443, 301);
```

#### **UINTEGER**
32-bit unsigned integer (0 to 4,294,967,295)
```sql
CREATE TABLE test_uinteger (
    ip_address_int UINTEGER,  -- IPv4 as 32-bit unsigned
    file_size UINTEGER DEFAULT 0
);

-- Store IPv4 address as integer
INSERT INTO test_uinteger VALUES (3232235777, 1048576); -- 192.168.1.1, 1MB
```

#### **UBIGINT**
64-bit unsigned integer (0 to 18,446,744,073,709,551,615)
```sql
CREATE TABLE test_ubigint (
    global_counter UBIGINT,
    disk_usage_bytes UBIGINT DEFAULT 0
);

INSERT INTO test_ubigint VALUES (18446744073709551615, 5497558138880); -- Max value, 5TB
```

#### **UINT128**
128-bit unsigned integer (0 to 340,282,366,920,938,463,463,374,607,431,768,211,455)
```sql
CREATE TABLE test_uint128 (
    uuid_as_int UINT128,  -- UUID represented as 128-bit integer
    crypto_hash UINT128
);

-- Useful for cryptographic hashes and very large counters
```

### Decimal and Floating Point Types

#### **NUMERIC(precision, scale)**
Exact numeric with user-defined precision and scale
```sql
CREATE TABLE test_numeric (
    price NUMERIC(10,2),        -- 8 digits + 2 decimal places
    percentage NUMERIC(5,4),    -- 1 digit + 4 decimal places  
    precise_calc NUMERIC(18,6)  -- 12 digits + 6 decimal places
);

INSERT INTO test_numeric VALUES (12345.67, 0.9875, 1234567890.123456);
```

#### **DECIMAL(precision, scale)**
Synonym for NUMERIC
```sql
CREATE TABLE financial_data (
    amount DECIMAL(15,2),
    tax_rate DECIMAL(5,4) DEFAULT 0.0825,
    exchange_rate DECIMAL(12,6)
);
```

#### **FLOAT(precision)**
Floating point number with specified binary precision
```sql
CREATE TABLE test_float (
    single_precision FLOAT(24),     -- 32-bit float (1-24 precision)
    double_precision FLOAT(53),     -- 64-bit float (25-53 precision)
    measurement FLOAT DEFAULT 0.0   -- Default 32-bit float
);

INSERT INTO test_float VALUES (3.14159, 2.718281828459045, 9.81);
```

#### **DOUBLE PRECISION**
64-bit IEEE floating point
```sql
CREATE TABLE scientific_data (
    coordinate_x DOUBLE PRECISION,
    coordinate_y DOUBLE PRECISION,
    measurement_value DOUBLE PRECISION
);

INSERT INTO scientific_data VALUES (123.456789012345, -987.654321098765, 2.99792458e8);
```

#### **DECFLOAT(precision)**
Decimal floating point (16 or 34 digit precision)
```sql
CREATE TABLE test_decfloat (
    standard_precision DECFLOAT(16),    -- 16 significant digits
    extended_precision DECFLOAT(34),    -- 34 significant digits
    auto_precision DECFLOAT             -- Default 34 digits
);

INSERT INTO test_decfloat VALUES (1.234567890123456, 1.2345678901234567890123456789012345, 3.14159);
```

---

## Character and Text Types

### Fixed and Variable Length Strings

#### **CHAR(length)**
Fixed-length character string (padded with spaces)
```sql
CREATE TABLE test_char (
    country_code CHAR(2),           -- Always 2 characters
    state_code CHAR(2),
    fixed_id CHAR(10) DEFAULT 'UNKNOWN   '
);

INSERT INTO test_char VALUES ('US', 'CA', 'ID001     ');
-- Stored as 'US', 'CA', 'ID001     ' (padded to 10 chars)
```

#### **VARCHAR(length)**
Variable-length character string (up to 128KB in ScratchBird)
```sql
CREATE TABLE test_varchar (
    name VARCHAR(100),
    description VARCHAR(500),
    large_text VARCHAR(131072),      -- ScratchBird: up to 128KB
    email VARCHAR(255) UNIQUE
);

INSERT INTO test_varchar VALUES (
    'John Doe', 
    'Software developer with 10 years experience',
    REPEAT('Large text content ', 1000),  -- Very large text
    'john.doe@example.com'
);
```

#### **CHARACTER VARYING(length)**
Synonym for VARCHAR
```sql
CREATE TABLE documents (
    title CHARACTER VARYING(200),
    content CHARACTER VARYING(65535)
);
```

### Enhanced Character Types (ScratchBird Extensions)

#### **CITEXT**
Case-insensitive text type
```sql
CREATE TABLE test_citext (
    username CITEXT UNIQUE,         -- Case-insensitive unique constraint
    email CITEXT,
    notes CITEXT
);

INSERT INTO test_citext VALUES ('JohnDoe', 'John.Doe@EXAMPLE.COM', 'Important Notes');
INSERT INTO test_citext VALUES ('johndoe', 'john.doe@example.com', 'more notes');
-- Second insert fails due to case-insensitive uniqueness
```

### Character Set and Collation

#### **Character Set Specification**
```sql
CREATE TABLE multilingual (
    english_text VARCHAR(100) CHARACTER SET UTF8,
    chinese_text VARCHAR(100) CHARACTER SET UTF8,
    binary_data VARCHAR(100) CHARACTER SET OCTETS
);
```

#### **Collation Specification**
```sql
CREATE TABLE sorted_data (
    case_sensitive VARCHAR(100) COLLATE UTF8,
    case_insensitive VARCHAR(100) COLLATE EN_US_CI,
    accent_insensitive VARCHAR(100) COLLATE EN_US_AI
);
```

---

## Binary Large Object Types

### **BLOB**
Binary Large Object for storing large binary or text data

#### **Basic BLOB**
```sql
CREATE TABLE test_blob (
    id INTEGER PRIMARY KEY,
    binary_data BLOB,               -- Generic binary data
    document BLOB SUB_TYPE 1,       -- Text BLOB
    image BLOB SUB_TYPE 0           -- Binary BLOB (default)
);
```

#### **BLOB with Subtype and Segment Size**
```sql
CREATE TABLE media_storage (
    media_id INTEGER,
    
    -- Text content with 8KB segments
    text_content BLOB SUB_TYPE TEXT SEGMENT SIZE 8192,
    
    -- Binary content with 16KB segments  
    binary_content BLOB SUB_TYPE 0 SEGMENT SIZE 16384,
    
    -- Character set for text BLOBs
    xml_data BLOB SUB_TYPE TEXT CHARACTER SET UTF8
);
```

#### **BLOB Usage Examples**
```sql
-- Insert text into BLOB
INSERT INTO media_storage (media_id, text_content) 
VALUES (1, 'This is a large text document that will be stored in a BLOB...');

-- Insert binary data
INSERT INTO media_storage (media_id, binary_content)
VALUES (2, X'89504E470D0A1A0A0000000D494844520000001000000010'); -- PNG header

-- Query BLOB data
SELECT media_id, CAST(text_content AS VARCHAR(1000)) as preview
FROM media_storage 
WHERE media_id = 1;
```

---

## Date and Time Types

### **DATE**
Date without time (year, month, day)
```sql
CREATE TABLE test_date (
    birth_date DATE,
    hire_date DATE DEFAULT CURRENT_DATE,
    expiry_date DATE
);

INSERT INTO test_date VALUES ('1990-05-15', '2023-01-01', '2025-12-31');
INSERT INTO test_date VALUES (DATE '2000-12-25', CURRENT_DATE, DATE '2030-06-15');
```

### **TIME**
Time without date (hour, minute, second, microsecond)
```sql
CREATE TABLE test_time (
    start_time TIME,
    end_time TIME,
    precise_time TIME(3)  -- 3 digits of sub-second precision
);

INSERT INTO test_time VALUES ('09:30:00', '17:45:30', '12:34:56.789');
INSERT INTO test_time VALUES (TIME '14:15:16', CURRENT_TIME, TIME '23:59:59.999');
```

### **TIME WITH TIME ZONE**
Time with timezone information
```sql
CREATE TABLE global_schedule (
    event_time TIME WITH TIME ZONE,
    local_time TIME,
    timezone_offset SMALLINT
);

INSERT INTO global_schedule VALUES (
    TIME '14:30:00 America/New_York',
    TIME '14:30:00',
    -5
);
```

### **TIMESTAMP**
Date and time combined (most commonly used)
```sql
CREATE TABLE test_timestamp (
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP,
    precise_timestamp TIMESTAMP(6)  -- 6 digits microsecond precision
);

INSERT INTO test_timestamp VALUES (
    CURRENT_TIMESTAMP, 
    TIMESTAMP '2023-12-25 15:30:45.123456',
    TIMESTAMP '2024-01-01 00:00:00.000001'
);
```

### **TIMESTAMP WITH TIME ZONE** 
Timestamp with timezone information
```sql
CREATE TABLE global_events (
    event_id INTEGER,
    event_timestamp TIMESTAMP WITH TIME ZONE,
    local_timestamp TIMESTAMP
);

INSERT INTO global_events VALUES (
    1,
    TIMESTAMP '2023-12-25 15:30:45 UTC',
    TIMESTAMP '2023-12-25 10:30:45'  -- EST equivalent
);
```

---

## Advanced Data Types

### **BOOLEAN**
True/false logical values
```sql
CREATE TABLE test_boolean (
    is_active BOOLEAN DEFAULT TRUE,
    is_deleted BOOLEAN DEFAULT FALSE,
    has_permission BOOLEAN
);

INSERT INTO test_boolean VALUES (TRUE, FALSE, NULL);
INSERT INTO test_boolean VALUES (1, 0, TRUE);  -- 1=TRUE, 0=FALSE
```

### **UUID**
Universally Unique Identifier (128-bit)
```sql
CREATE TABLE test_uuid (
    record_id UUID PRIMARY KEY DEFAULT GEN_UUID(),
    user_id UUID,
    session_id UUID DEFAULT GEN_UUID()
);

INSERT INTO test_uuid (user_id) VALUES ('550e8400-e29b-41d4-a716-446655440000');
INSERT INTO test_uuid (user_id) VALUES (GEN_UUID());

-- UUID utility functions
SELECT 
    GEN_UUID() as new_uuid,
    CHAR_TO_UUID('550e8400-e29b-41d4-a716-446655440000') as parsed_uuid,
    UUID_TO_CHAR(GEN_UUID()) as uuid_string;
```

### **JSON**
JavaScript Object Notation data
```sql
CREATE TABLE test_json (
    id INTEGER PRIMARY KEY,
    metadata JSON,
    config JSON,
    user_data JSON
);

INSERT INTO test_json VALUES (
    1,
    '{"version": "1.0", "author": "John Doe"}',
    '{"theme": "dark", "notifications": true, "language": "en"}',
    '{"preferences": {"color": "blue", "size": "large"}, "history": [1,2,3]}'
);

-- JSON functions
SELECT 
    id,
    JSON_EXTRACT(metadata, '$.version') as version,
    JSON_EXTRACT(config, '$.theme') as theme,
    JSON_VALID(user_data) as is_valid_json
FROM test_json;
```

---

## Network Data Types (ScratchBird Extensions)

### **INET**
IPv4 and IPv6 address storage
```sql
CREATE TABLE network_devices (
    device_id INTEGER PRIMARY KEY,
    ip_address INET,
    gateway INET,
    dns_server INET
);

INSERT INTO network_devices VALUES (
    1, 
    INET '192.168.1.100',
    INET '192.168.1.1',
    INET '8.8.8.8'
);

INSERT INTO network_devices VALUES (
    2,
    INET '2001:0db8:85a3:0000:0000:8a2e:0370:7334',
    INET '2001:0db8:85a3::1',
    INET '::1'
);
```

### **CIDR**
Network address blocks with subnet mask
```sql
CREATE TABLE network_subnets (
    subnet_id INTEGER PRIMARY KEY,
    network_block CIDR,
    description VARCHAR(100)
);

INSERT INTO network_subnets VALUES (
    1, 
    CIDR '192.168.1.0/24',
    'Main office network'
);

INSERT INTO network_subnets VALUES (
    2,
    CIDR '10.0.0.0/8', 
    'Private network range'
);

INSERT INTO network_subnets VALUES (
    3,
    CIDR '2001:db8::/32',
    'IPv6 documentation network'
);
```

### **MACADDR**
MAC (Media Access Control) addresses
```sql
CREATE TABLE network_interfaces (
    interface_id INTEGER PRIMARY KEY,
    mac_address MACADDR UNIQUE,
    device_name VARCHAR(50),
    interface_type VARCHAR(20)
);

INSERT INTO network_interfaces VALUES (
    1,
    MACADDR '08:00:2b:01:02:03',
    'eth0',
    'Ethernet'
);

INSERT INTO network_interfaces VALUES (
    2,
    MACADDR '00-1B-63-84-45-E6',  -- Alternative format
    'wlan0', 
    'WiFi'
);
```

---

## Range Data Types (ScratchBird Extensions)

### **INT4RANGE**
Integer ranges for 32-bit integers
```sql
CREATE TABLE test_int4range (
    id INTEGER PRIMARY KEY,
    valid_range INT4RANGE,
    age_range INT4RANGE,
    temperature_range INT4RANGE
);

INSERT INTO test_int4range VALUES (
    1,
    INT4RANGE '[1,100]',      -- Closed range: includes 1 and 100
    INT4RANGE '[18,65)',      -- Half-open: includes 18, excludes 65
    INT4RANGE '(-10,50]'      -- Half-open: excludes -10, includes 50
);

-- Range operations
SELECT 
    id,
    valid_range @> 50 as contains_50,                    -- Contains operator
    age_range && INT4RANGE '[25,35]' as overlaps_25_35,  -- Overlap operator
    LOWER(temperature_range) as min_temp,                -- Lower bound
    UPPER(temperature_range) as max_temp                 -- Upper bound
FROM test_int4range;
```

### **INT8RANGE**
Integer ranges for 64-bit integers (BIGINT)
```sql
CREATE TABLE big_ranges (
    range_id INTEGER,
    transaction_range INT8RANGE,
    timestamp_range INT8RANGE
);

INSERT INTO big_ranges VALUES (
    1,
    INT8RANGE '[1000000000, 9999999999]',
    INT8RANGE '[1640995200, 1672531199]'  -- Unix timestamps for 2022
);
```

### **NUMRANGE**
Numeric ranges for DECIMAL/NUMERIC values
```sql
CREATE TABLE price_ranges (
    category VARCHAR(50),
    price_range NUMRANGE,
    discount_range NUMRANGE
);

INSERT INTO price_ranges VALUES (
    'Electronics',
    NUMRANGE '[10.00, 999.99]',
    NUMRANGE '[0.05, 0.30)'     -- 5% to 30% (excluding 30%)
);
```

### **DATERANGE**
Date ranges
```sql
CREATE TABLE event_schedules (
    event_name VARCHAR(100),
    event_dates DATERANGE,
    registration_period DATERANGE
);

INSERT INTO event_schedules VALUES (
    'Annual Conference',
    DATERANGE '[2023-06-15, 2023-06-17]',
    DATERANGE '[2023-01-01, 2023-06-01)'
);

-- Date range queries
SELECT 
    event_name,
    event_dates @> DATE '2023-06-16' as includes_date,
    UPPER(registration_period) - LOWER(registration_period) as registration_days
FROM event_schedules;
```

### **TSRANGE and TSTZRANGE**
Timestamp ranges (with and without timezone)
```sql
CREATE TABLE maintenance_windows (
    window_id INTEGER,
    maintenance_period TSRANGE,
    global_period TSTZRANGE
);

INSERT INTO maintenance_windows VALUES (
    1,
    TSRANGE '[2023-12-25 02:00:00, 2023-12-25 06:00:00)',
    TSTZRANGE '[2023-12-25 02:00:00 UTC, 2023-12-25 06:00:00 UTC)'
);
```

---

## Geometric Data Types (ScratchBird Extensions)

### **POINT**
2D geometric point (x, y coordinates)
```sql
CREATE TABLE locations (
    location_id INTEGER PRIMARY KEY,
    coordinates POINT,
    map_position POINT,
    gps_location POINT
);

INSERT INTO locations VALUES (
    1,
    POINT '(37.7749, -122.4194)',   -- San Francisco coordinates
    POINT '(0, 0)',                  -- Origin point
    POINT '(40.7128, -74.0060)'     -- New York coordinates
);

-- Point operations and functions
SELECT 
    location_id,
    coordinates,
    ST_DISTANCE(coordinates, POINT '(0,0)') as distance_from_origin,
    ST_WITHIN(coordinates, 'POLYGON((30 -130, 50 -130, 50 -60, 30 -60, 30 -130))') as in_usa
FROM locations;
```

---

## Full-Text Search Types (ScratchBird Extensions)

### **TSVECTOR**
Text search vector for full-text search
```sql
CREATE TABLE documents (
    doc_id INTEGER PRIMARY KEY,
    title VARCHAR(200),
    content TEXT,
    search_vector TSVECTOR
);

-- Populate search vector
INSERT INTO documents VALUES (
    1,
    'Database Programming Guide',
    'This comprehensive guide covers SQL, stored procedures, and database design.',
    TO_TSVECTOR('english', 'Database Programming Guide comprehensive SQL procedures design')
);

-- Update search vector with trigger
CREATE OR REPLACE TRIGGER update_search_vector
    BEFORE INSERT OR UPDATE ON documents
    FOR EACH ROW
BEGIN
    NEW.search_vector = TO_TSVECTOR('english', COALESCE(NEW.title, '') || ' ' || COALESCE(NEW.content, ''));
END;
```

### **TSQUERY**
Text search query for full-text search operations
```sql
CREATE TABLE saved_searches (
    search_id INTEGER PRIMARY KEY,
    search_name VARCHAR(100),
    search_query TSQUERY
);

INSERT INTO saved_searches VALUES (
    1,
    'Database Programming',
    TO_TSQUERY('english', 'database & programming')
);

INSERT INTO saved_searches VALUES (
    2,
    'SQL or Procedures',
    TO_TSQUERY('english', 'SQL | procedures')
);

-- Full-text search
SELECT 
    d.doc_id,
    d.title,
    TS_RANK(d.search_vector, s.search_query) as rank
FROM documents d, saved_searches s
WHERE d.search_vector @@ s.search_query 
  AND s.search_id = 1
ORDER BY rank DESC;
```

---

## AI/ML Data Types (ScratchBird Extensions)

### **VECTOR**
Multi-dimensional vector for machine learning applications
```sql
CREATE TABLE ml_features (
    feature_id INTEGER PRIMARY KEY,
    feature_name VARCHAR(100),
    feature_vector VECTOR(128),     -- 128-dimensional vector
    embedding VECTOR(512),          -- 512-dimensional embedding
    weights VECTOR(10)              -- 10-dimensional weight vector
);

INSERT INTO ml_features VALUES (
    1,
    'Image Features',
    VECTOR '[0.1, 0.2, 0.3, 0.4, ...]',  -- 128 values
    VECTOR '[0.001, 0.002, 0.003, ...]', -- 512 values  
    VECTOR '[0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0.0]'
);

-- Vector operations
SELECT 
    feature_id,
    feature_name,
    VECTOR_DISTANCE(feature_vector, VECTOR '[0.15, 0.25, 0.35, ...]') as similarity,
    VECTOR_LENGTH(weights) as weight_magnitude
FROM ml_features
ORDER BY similarity ASC  -- Most similar first
LIMIT 10;
```

---

## Array Data Types

### **Single-Dimensional Arrays**
```sql
CREATE TABLE test_arrays (
    id INTEGER PRIMARY KEY,
    int_array INTEGER[10],          -- Array of 10 integers
    text_array VARCHAR(50)[5],      -- Array of 5 strings
    date_array DATE[7],             -- Array of 7 dates
    flexible_array INTEGER[]       -- Variable-length array
);

INSERT INTO test_arrays VALUES (
    1,
    [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
    ['Mon', 'Tue', 'Wed', 'Thu', 'Fri'],
    ['2023-01-01', '2023-01-02', '2023-01-03', '2023-01-04', '2023-01-05', '2023-01-06', '2023-01-07'],
    [100, 200, 300]
);

-- Array operations
SELECT 
    id,
    int_array[1] as first_int,                    -- Array indexing (1-based)
    int_array[5:8] as slice,                      -- Array slicing
    ARRAY_LENGTH(flexible_array) as array_size,   -- Array length
    100 = ANY(flexible_array) as contains_100     -- Array membership
FROM test_arrays;
```

### **Multi-Dimensional Arrays**
```sql
CREATE TABLE matrix_data (
    matrix_id INTEGER PRIMARY KEY,
    matrix_2d INTEGER[3][3],        -- 3x3 matrix
    cube_3d INTEGER[2][2][2],       -- 2x2x2 cube
    description VARCHAR(100)
);

INSERT INTO matrix_data VALUES (
    1,
    [[1, 2, 3], [4, 5, 6], [7, 8, 9]],           -- 2D array
    [[[1, 2], [3, 4]], [[5, 6], [7, 8]]],        -- 3D array
    '3x3 Identity-like matrix and 2x2x2 cube'
);

-- Multi-dimensional array access
SELECT 
    matrix_id,
    matrix_2d[2][2] as center_element,    -- Access element at [2,2]
    cube_3d[1][1][1] as corner_element,   -- Access 3D element
    description
FROM matrix_data;
```

---

## Type Conversion and Casting

### **Explicit Casting**
```sql
-- CAST function
SELECT 
    CAST(123 AS VARCHAR(10)) as int_to_string,
    CAST('456' AS INTEGER) as string_to_int,
    CAST('2023-12-25' AS DATE) as string_to_date,
    CAST(CURRENT_TIMESTAMP AS DATE) as timestamp_to_date;

-- Type conversion with formatting
SELECT
    CAST(123.456 AS VARCHAR(20)) as default_format,
    CAST(123.456 AS DECIMAL(10,2)) as rounded_decimal,
    CAST('2023-12-25 15:30:45' AS TIMESTAMP) as string_to_timestamp;
```

### **Implicit Type Conversion**
```sql
-- Automatic conversions in expressions
SELECT 
    123 + 456.78 as int_plus_decimal,      -- Result: DECIMAL
    'Value: ' || 123 as string_concat,     -- Result: VARCHAR
    CURRENT_DATE + 30 as date_plus_days,   -- Result: DATE
    '2023-01-01'::DATE as string_to_date;  -- PostgreSQL-style casting
```

---

## Data Type Constraints and Validation

### **Domain-Based Validation**
```sql
-- Create domains for common validation patterns
CREATE DOMAIN email_type AS VARCHAR(255) 
    CHECK (VALUE SIMILAR TO '%@%.%');

CREATE DOMAIN positive_integer AS INTEGER 
    CHECK (VALUE > 0);

CREATE DOMAIN percentage AS DECIMAL(5,2) 
    CHECK (VALUE BETWEEN 0.00 AND 100.00);

-- Use domains in table definitions
CREATE TABLE customers (
    customer_id positive_integer PRIMARY KEY,
    email email_type UNIQUE,
    discount_rate percentage DEFAULT 0.00
);
```

### **Advanced Constraints**
```sql
CREATE TABLE products (
    product_id INTEGER PRIMARY KEY,
    price DECIMAL(10,2) CHECK (price > 0),
    category VARCHAR(50) CHECK (category IN ('Electronics', 'Clothing', 'Books', 'Home')),
    launch_date DATE CHECK (launch_date >= DATE '2020-01-01'),
    dimensions POINT CHECK (ST_X(dimensions) > 0 AND ST_Y(dimensions) > 0),
    tags VARCHAR(100)[] CHECK (ARRAY_LENGTH(tags) <= 10)
);
```

---

## Storage and Performance Considerations

### **Storage Sizes**

| Data Type | Storage Size | Range/Precision |
|-----------|--------------|-----------------|
| SMALLINT | 2 bytes | -32,768 to 32,767 |
| INTEGER | 4 bytes | -2,147,483,648 to 2,147,483,647 |
| BIGINT | 8 bytes | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |
| INT128 | 16 bytes | ±170,141,183,460,469,231,731,687,303,715,884,105,727 |
| USMALLINT | 2 bytes | 0 to 65,535 |
| UINTEGER | 4 bytes | 0 to 4,294,967,295 |
| UBIGINT | 8 bytes | 0 to 18,446,744,073,709,551,615 |
| UINT128 | 16 bytes | 0 to 340,282,366,920,938,463,463,374,607,431,768,211,455 |
| REAL | 4 bytes | 6 decimal digits precision |
| DOUBLE PRECISION | 8 bytes | 15 decimal digits precision |
| DECIMAL(p,s) | Variable | User-defined precision |
| CHAR(n) | n bytes | Fixed length |
| VARCHAR(n) | Variable + 2 bytes | Up to n characters (128KB max) |
| DATE | 4 bytes | 4713 BC to 5874897 AD |
| TIME | 4 bytes | 00:00:00 to 23:59:59.9999 |
| TIMESTAMP | 8 bytes | Combined date and time |
| BOOLEAN | 1 byte | TRUE, FALSE, NULL |
| UUID | 16 bytes | 128-bit unique identifier |
| BLOB | Variable | Up to 4GB |
| JSON | Variable | Variable length like BLOB |

### **Performance Tips**

1. **Choose Appropriate Sizes**: Use smallest data type that fits your needs
2. **Index Considerations**: Smaller data types create more efficient indexes
3. **Alignment**: Group similar-sized columns together for better storage
4. **NULL vs Default**: Consider storage implications of NULL vs default values

```sql
-- Optimized table layout
CREATE TABLE optimized_layout (
    -- Group small fixed-size types together
    id INTEGER NOT NULL,
    status SMALLINT DEFAULT 1,
    flags SMALLINT DEFAULT 0,
    is_active BOOLEAN DEFAULT TRUE,
    
    -- Variable-length types at the end
    name VARCHAR(100),
    description VARCHAR(500),
    metadata JSON,
    
    -- BLOB/large data last
    content BLOB
);
```

