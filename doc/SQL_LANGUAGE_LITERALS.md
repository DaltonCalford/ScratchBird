# ScratchBird SQL Language Literals - Complete Reference Documentation

## Overview

**SQL Language Literals** are fundamental constants used in ScratchBird SQL statements to represent fixed values of various data types. This documentation provides comprehensive coverage of all literal types supported by ScratchBird, including standard SQL literals and ScratchBird-specific enhancements.

### Supported Literal Types

ScratchBird supports the following categories of literals:

- **String Literals**: Character strings with various encoding and formatting options
- **Number Literals**: Integer, decimal, floating-point, and hexadecimal numeric values
- **Boolean Literals**: Logical true/false values with three-value logic support
- **Datetime Literals**: Date, time, and timestamp values with time zone support
- **Binary Literals**: Raw binary data in hexadecimal notation
- **NULL Literals**: Represents unknown or missing values

---

## String Literals

String literals represent character data and are the most commonly used literals in SQL statements.

### Character String Literals

Character string literals are sequences of characters enclosed in single quotes (apostrophes).

#### Syntax
```sql
<char-literal> ::=
    [<introducer> charset-name] <quote> [<char>...] <quote>
    [{ <separator> <quote> [<char>...] <quote> }... ]

<separator> ::= { <comment> | <white space> }
<introducer> ::= underscore (U+005F)
<quote> ::= apostrophe (U+0027)
<char> ::= character representation; apostrophe is escaped by doubling
```

#### Basic String Literal Examples
```sql
-- Simple string literal
SELECT 'Hello, World!';
-- Result: Hello, World!

-- String with embedded single quote (escaped by doubling)
SELECT 'Mother O''Reilly''s home-made hooch';
-- Result: Mother O'Reilly's home-made hooch

-- Empty string
SELECT '';
-- Result: (empty string)

-- String with whitespace
SELECT '  Leading and trailing spaces  ';
-- Result:   Leading and trailing spaces  
```

#### Multi-Line String Literals
```sql
-- Whitespace between string parts (concatenated automatically)
SELECT 'First part '
       'Second part '
       'Third part'
;
-- Result: First part Second part Third part

-- Comments between string parts
SELECT 'Database ' /* comment here */ 'Management'
;
-- Result: Database Management

-- Multi-line with proper formatting
SELECT 'Line 1' ||
       CHR(13) || CHR(10) ||  -- CR+LF
       'Line 2'
;
-- Result: Line 1
--         Line 2
```

#### String Literal Limitations
```sql
-- Maximum lengths
-- CHAR/VARCHAR: 32,765 bytes
-- BLOB: 65,533 bytes

-- Example of long string (truncated for display)
SELECT 'This is a very long string that can contain up to 32,765 bytes for VARCHAR...'
;
```

### Alternative String Literals (Q-Quote Syntax)

Alternative string literals allow embedding quotes without escaping using the `Q` keyword.

#### Syntax
```sql
<alternative string literal> ::=
    { q | Q } <quote> <start char> [<char> ...] <end char> <quote>
```

#### Q-Quote Examples
```sql
-- Using curly braces as delimiters
SELECT Q'{This string contains 'single quotes' without escaping}' 
;
-- Result: This string contains 'single quotes' without escaping

-- Using square brackets
SELECT Q'[SQL statement: SELECT 'value' FROM table]'
;
-- Result: SQL statement: SELECT 'value' FROM table

-- Using parentheses
SELECT Q'(It's easy to include apostrophes this way)'
;
-- Result: It's easy to include apostrophes this way

-- Using angle brackets
SELECT Q'<XML tag: <element value="data">content</element>>'
;
-- Result: XML tag: <element value="data">content</element>

-- Using custom delimiter character
SELECT Q'!That's a "quoted" string!'
;
-- Result: That's a "quoted" string

-- Using pipe character as delimiter
SELECT Q'|Complex string with 'quotes' and "double quotes"|'
;
-- Result: Complex string with 'quotes' and "double quotes"
```

#### Paired Delimiters
```sql
-- Paired delimiters automatically match
SELECT Q'{Nested {braces} are handled correctly}';
-- Result: Nested {braces} are handled correctly

SELECT Q'(Parentheses (nested) work too)';
-- Result: Parentheses (nested) work too

SELECT Q'[Square [brackets] also work]';
-- Result: Square [brackets] also work

SELECT Q'<Angle <brackets> supported>';
-- Result: Angle <brackets> supported
```

### Character Set Introducers

Character set introducers specify how string literals should be interpreted and stored.

#### Syntax
```sql
<introducer> <charset-name> '<string>'
```

#### Character Set Examples
```sql
-- ASCII character set
SELECT _ASCII 'Hello World';

-- UTF-8 character set
SELECT _UTF8 'Здравствуй мир';
-- Result: Здравствуй мир (Hello World in Russian)

-- ISO Latin-1 character set
SELECT _ISO8859_1 'Café français';
-- Result: Café français

-- Windows-1252 character set
SELECT _WIN1252 'Special characters: ©®™';
-- Result: Special characters: ©®™

-- Unicode character set
SELECT _UNICODE_FSS 'Unicode: ∀∃∈∉∅';
-- Result: Unicode: ∀∃∈∉∅
```

#### Practical Character Set Usage
```sql
-- Inserting multi-language data
INSERT INTO multilingual_table (id, english_text, russian_text, german_text)
VALUES (
    1,
    _ASCII 'Hello',
    _UTF8 'Привет',
    _ISO8859_1 'Hallo'
);

-- Querying with character set specification
SELECT customer_name
FROM customers
WHERE customer_name = _UTF8 'Müller';
```

### Binary String Literals

Binary string literals represent raw binary data using hexadecimal notation.

#### Syntax
```sql
<binary-literal> ::=
    [<introducer> charsetname] X <quote> [<space>...]
    [{ <hexit> [<space>...] <hexit> [<space>...] }...] <quote>
    [{ <separator> <quote> [<space>...]
    [{ <hexit> [<space>...] <hexit> [<space>...] }...] <quote> }...]

<hexdigit> ::= one of 0..9, A..F, a..f
<space> ::= the space character (U+0020)
```

#### Binary String Examples
```sql
-- Basic hexadecimal string
SELECT X'4E657276656E';
-- Returns: 4E657276656E (6-byte binary string)

-- Hexadecimal with character set interpretation
SELECT _ASCII X'4E657276656E';
-- Returns: Nerven (interpreted as ASCII text)

-- UTF-8 encoded string in hex
SELECT _UTF8 X'53C3A46765';
-- Returns: Säge (4 characters, 5 bytes in UTF-8)

-- ISO Latin-1 encoded string
SELECT _ISO8859_1 X'53E46765';
-- Returns: Säge (4 characters, 4 bytes in ISO-8859-1)

-- Windows-1252 with whitespace formatting
SELECT _WIN1252 X'42 49 4E 41 52 59';
-- Returns: BINARY

-- Multi-line hexadecimal string
SELECT _WIN1252 X'42494E'
                 '415259'
;
-- Returns: BINARY
```

#### Binary Data Applications
```sql
-- Storing binary data
INSERT INTO file_storage (filename, file_data)
VALUES ('image.png', X'89504E470D0A1A0A0000000D49484452...');

-- Comparing binary values
SELECT *
FROM binary_data
WHERE data_field = X'DEADBEEF';

-- Binary data with null bytes
SELECT X'00010203040500060708';
-- Valid: can contain any byte value including 0x00
```

---

## Number Literals

Number literals represent numeric values in various formats and precisions.

### Integer Literals

Integer literals represent whole numbers without decimal points.

#### Decimal Integer Examples
```sql
-- Small integers
SELECT 0;          -- Zero
SELECT 42;         -- Positive integer
SELECT -17;        -- Negative integer

-- Large integers
SELECT 2147483647; -- Maximum 32-bit signed integer
SELECT -2147483648; -- Minimum 32-bit signed integer

-- BIGINT range
SELECT 9223372036854775807;  -- Maximum 64-bit signed integer
SELECT -9223372036854775808; -- Minimum 64-bit signed integer

-- INT128 (very large integers)
SELECT 170141183460469231731687303715884105727;
-- Maximum 128-bit signed integer
```

#### Type Determination Rules
```sql
-- INTEGER range: -2,147,483,648 to 2,147,483,647
SELECT 1000;        -- INTEGER type

-- BIGINT range: beyond INTEGER limits
SELECT 3000000000;  -- BIGINT type

-- INT128 range: beyond BIGINT limits  
SELECT 10000000000000000000; -- INT128 type

-- DECFLOAT(34) for values that don't fit in INT128
SELECT 999999999999999999999999999999999999; -- DECFLOAT(34)
```

### Decimal Literals

Decimal literals contain decimal points and represent precise decimal values.

#### Decimal Examples
```sql
-- Basic decimal numbers
SELECT 3.14159;     -- NUMERIC type
SELECT 0.5;         -- NUMERIC type
SELECT 123.456789;  -- NUMERIC type

-- High precision decimals
SELECT 123456789012345678.123456789012345678;
-- NUMERIC(38,18) or DECFLOAT(34)

-- Trailing zeros preserved in NUMERIC
SELECT 100.00;      -- NUMERIC(5,2)
SELECT 0.10000;     -- NUMERIC(6,5)

-- Financial calculations (exact precision)
SELECT 1999.99 + 0.01;     -- 2000.00 (exact)
SELECT 19.95 * 1.0825;     -- Exact decimal arithmetic
```

#### Precision and Scale
```sql
-- NUMERIC type determination
-- Format: NUMERIC(precision, scale)

SELECT 123.45;      -- NUMERIC(5,2)
SELECT 0.123456789; -- NUMERIC(9,9)
SELECT 999999999999999999.99; -- NUMERIC(20,2)

-- DECFLOAT for very high precision
SELECT 1.234567890123456789012345678901234;
-- DECFLOAT(34)
```

### Scientific Notation (Exponential)

Scientific notation represents very large or very small numbers using exponential format.

#### Exponential Examples
```sql
-- Basic scientific notation
SELECT 1.23E4;      -- 12300.0 (DOUBLE PRECISION)
SELECT 2.34E-5;     -- 0.0000234 (DOUBLE PRECISION)
SELECT 6.022E23;    -- Avogadro's number (DOUBLE PRECISION)

-- Large exponents (DECFLOAT)
SELECT 1.5E308;     -- Near DOUBLE PRECISION limit
SELECT 1.5E309;     -- DECFLOAT(34) due to large exponent

-- High precision scientific notation
SELECT 1.23456789012345678901E100;
-- DECFLOAT(34) due to high precision

-- Negative exponents
SELECT 1E-10;       -- 0.0000000001
SELECT 5.67E-100;   -- Very small number
```

#### Type Selection for Exponential
```sql
-- DOUBLE PRECISION conditions
SELECT 1.234E10;    -- Fits in DOUBLE PRECISION
SELECT 2.5E-50;     -- Fits in DOUBLE PRECISION

-- DECFLOAT(34) conditions  
SELECT 1.234567890123456789012E50;  -- 20+ digits
SELECT 1.5E309;     -- Exponent >= 309
SELECT 1.5E-309;    -- Absolute exponent >= 309
```

### Hexadecimal Number Literals

Hexadecimal literals represent integer values using base-16 notation.

#### Hexadecimal Syntax
```sql
-- Syntax: 0x<hexdigits> or 0X<hexdigits>
-- Digits: 0-9, A-F, a-f (case insensitive)
```

#### Hexadecimal Examples
```sql
-- Small hex numbers (INTEGER type)
SELECT 0x10;        -- 16 decimal
SELECT 0xFF;        -- 255 decimal  
SELECT 0x6FAA0D3;   -- 117088467 decimal

-- 8-digit hex (INTEGER, signed interpretation)
SELECT 0x7FFFFFFF;  -- 2147483647 (positive)
SELECT 0x80000000;  -- -2147483648 (negative, sign bit set)
SELECT 0xFFFFFFFF;  -- -1 (all bits set)

-- 9+ digit hex (BIGINT type)
SELECT 0x100000000; -- 4294967296 (BIGINT)
SELECT 0x09E44F9A8; -- 2655320488 (BIGINT, positive)

-- Large hex numbers (INT128)
SELECT 0x12345678901234567890123456789012;
-- INT128 type for 17-32 hex digits
```

#### Hexadecimal Value Ranges and Type Mapping
```sql
-- INTEGER range (1-8 hex digits)
SELECT 0x1;         -- 1 (INTEGER)
SELECT 0x7FFFFFFF;  -- 2147483647 (max positive INTEGER)
SELECT 0x80000000;  -- -2147483648 (min negative INTEGER)
SELECT 0xFFFFFFFF;  -- -1 (INTEGER)

-- BIGINT range (9-16 hex digits)  
SELECT 0x100000000; -- 4294967296 (BIGINT)
SELECT 0x7FFFFFFFFFFFFFFF; -- max positive BIGINT
SELECT 0x8000000000000000; -- min negative BIGINT
SELECT 0xFFFFFFFFFFFFFFFF; -- -1 (BIGINT)

-- INT128 range (17-32 hex digits)
SELECT 0x10000000000000000; -- INT128
SELECT 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF; -- max positive INT128
SELECT 0x80000000000000000000000000000000; -- min negative INT128
SELECT 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF; -- -1 (INT128)
```

#### Practical Hexadecimal Usage
```sql
-- Bit manipulation
SELECT 0xF0 & 0x0F; -- 0 (bitwise AND)
SELECT 0xF0 | 0x0F; -- 255 (bitwise OR)

-- Color values (common in applications)
SELECT 0xFF0000;    -- Red (16711680)
SELECT 0x00FF00;    -- Green (65280)
SELECT 0x0000FF;    -- Blue (255)

-- Memory addresses or identifiers
SELECT 0xDEADBEEF;  -- Common test value
SELECT 0xCAFEBABE;  -- Another test value
```

---

## Boolean Literals

Boolean literals represent logical truth values in ScratchBird's three-value logic system.

### Boolean Values

ScratchBird supports three boolean literals corresponding to three-value logic (true, false, unknown).

#### Boolean Literal Examples
```sql
-- True value
SELECT TRUE;
-- Result: TRUE (or 1 in numeric context)

-- False value  
SELECT FALSE;
-- Result: FALSE (or 0 in numeric context)

-- Unknown value
SELECT UNKNOWN;
-- Result: NULL (represents unknown/indeterminate state)
```

### Boolean Operations
```sql
-- Boolean comparisons
SELECT TRUE = TRUE;    -- TRUE
SELECT TRUE = FALSE;   -- FALSE
SELECT TRUE = UNKNOWN; -- UNKNOWN (NULL)

-- Boolean logic operations
SELECT TRUE AND TRUE;     -- TRUE
SELECT TRUE AND FALSE;    -- FALSE
SELECT TRUE AND UNKNOWN;  -- UNKNOWN (NULL)

SELECT TRUE OR FALSE;     -- TRUE
SELECT FALSE OR FALSE;    -- FALSE
SELECT FALSE OR UNKNOWN;  -- UNKNOWN (NULL)

-- Boolean negation
SELECT NOT TRUE;      -- FALSE
SELECT NOT FALSE;     -- TRUE
SELECT NOT UNKNOWN;   -- UNKNOWN (NULL)
```

### Boolean in Conditional Logic
```sql
-- Using boolean literals in CASE expressions
SELECT 
    customer_id,
    CASE active_flag
        WHEN TRUE THEN 'Active Customer'
        WHEN FALSE THEN 'Inactive Customer'
        WHEN UNKNOWN THEN 'Status Unknown'
        ELSE 'Invalid Status'
    END as customer_status
FROM customers;

-- Boolean literals in WHERE clauses
SELECT * FROM products WHERE discontinued = FALSE;
SELECT * FROM users WHERE email_verified = TRUE;
SELECT * FROM orders WHERE cancelled IS UNKNOWN;
```

### Boolean Data Type Usage
```sql
-- Creating table with boolean columns
CREATE TABLE feature_flags (
    flag_name VARCHAR(50),
    is_enabled BOOLEAN DEFAULT FALSE,
    is_experimental BOOLEAN DEFAULT TRUE
);

-- Inserting boolean literals
INSERT INTO feature_flags VALUES ('new_ui', TRUE, FALSE);
INSERT INTO feature_flags VALUES ('beta_feature', FALSE, TRUE);
INSERT INTO feature_flags VALUES ('unknown_flag', UNKNOWN, UNKNOWN);

-- Querying boolean values
SELECT flag_name, is_enabled
FROM feature_flags  
WHERE is_enabled = TRUE;

SELECT flag_name
FROM feature_flags
WHERE is_experimental IS NOT UNKNOWN;
```

---

## Datetime Literals

Datetime literals represent date, time, and timestamp values using standardized formats.

### Date Literals

Date literals represent calendar dates using the DATE keyword followed by a formatted string.

#### Date Literal Syntax
```sql
DATE '<date_format>'

<date_format> ::=
    [YYYY<p>]MM<p>DD          -- Year-Month-Day
    | MM<p>DD[<p>{ YYYY | YY }] -- Month-Day-Year  
    | DD<p>MM[<p>{ YYYY | YY }] -- Day-Month-Year

<p> ::= whitespace | . | - | /  -- Date separators
```

#### Date Examples
```sql
-- ISO format (YYYY-MM-DD)
SELECT DATE '2024-12-25';
-- Result: 2024-12-25

-- US format (MM/DD/YYYY)
SELECT DATE '12/25/2024';
-- Result: 2024-12-25

-- European format (DD.MM.YYYY)
SELECT DATE '25.12.2024';
-- Result: 2024-12-25

-- Various separators
SELECT DATE '2024-12-25';  -- Hyphen
SELECT DATE '2024/12/25';  -- Slash
SELECT DATE '2024.12.25';  -- Period
SELECT DATE '2024 12 25';  -- Space

-- Two-digit years (interpreted in current century window)
SELECT DATE '12/25/24';    -- 2024-12-25
SELECT DATE '25.12.99';    -- 1999-12-25 (depends on century window)
```

#### Month Name Support
```sql
-- Full month names (case insensitive)
SELECT DATE '25 December 2024';
SELECT DATE 'December 25, 2024';
SELECT DATE '2024 December 25';

-- Three-letter month abbreviations
SELECT DATE '25 Dec 2024';
SELECT DATE 'Dec 25, 2024';
SELECT DATE '2024 Dec 25';

-- Mixed case month names
SELECT DATE '25 DECEMBER 2024';
SELECT DATE '25 december 2024';
SELECT DATE '25 December 2024';
```

### Time Literals

Time literals represent time-of-day values using the TIME keyword.

#### Time Literal Syntax
```sql
TIME '<time_format>'
TIME '<time_tz_format>'

<time_format> ::= HH[:mm[:SS[<f>NNNN]]]
<time_tz_format> ::= <time_format> [<space>] <time_zone>
<time_zone> ::= { + | - }HH:MM | time_zone_name
<f> ::= : | .  -- Fractional seconds separator
```

#### Time Examples
```sql
-- Basic time formats
SELECT TIME '14:30:00';
-- Result: 14:30:00.0000

SELECT TIME '9:15';
-- Result: 09:15:00.0000

SELECT TIME '23:59:59';
-- Result: 23:59:59.0000

-- With fractional seconds
SELECT TIME '14:30:15.1234';
-- Result: 14:30:15.1234

SELECT TIME '09:00:00.5';
-- Result: 09:00:00.5000

-- Different fractional separators
SELECT TIME '14:30:15.1234';  -- Period
SELECT TIME '14:30:15:1234';  -- Colon
```

#### Time with Time Zone
```sql
-- Time with numeric time zone offset
SELECT TIME '14:30:00+02:00';
-- Result: 14:30:00.0000+02:00

SELECT TIME '09:15:30-05:00';
-- Result: 09:15:30.0000-05:00

-- Time with named time zone
SELECT TIME '14:30:00 Europe/Berlin';
SELECT TIME '09:15:30 America/New_York';
SELECT TIME '22:45:00 Asia/Tokyo';

-- UTC time zone
SELECT TIME '14:30:00+00:00';
SELECT TIME '14:30:00 UTC';
```

### Timestamp Literals

Timestamp literals combine date and time into a single value.

#### Timestamp Literal Syntax
```sql
TIMESTAMP '<timestamp_format>'
TIMESTAMP '<timestamp_tz_format>'

<timestamp_format> ::= <date_format> [<space> <time_format>]
<timestamp_tz_format> ::= <timestamp_format> [<space>] <time_zone>
```

#### Timestamp Examples
```sql
-- Basic timestamp
SELECT TIMESTAMP '2024-12-25 14:30:00';
-- Result: 2024-12-25 14:30:00.0000

-- Timestamp with fractional seconds
SELECT TIMESTAMP '2024-12-25 14:30:15.1234';
-- Result: 2024-12-25 14:30:15.1234

-- Various date formats in timestamp
SELECT TIMESTAMP '12/25/2024 2:30:15 PM';
SELECT TIMESTAMP '25.12.2024 14:30:15';
SELECT TIMESTAMP '2024-12-25T14:30:15';  -- ISO format

-- Timestamp without time portion (defaults to 00:00:00)
SELECT TIMESTAMP '2024-12-25';
-- Result: 2024-12-25 00:00:00.0000
```

#### Timestamp with Time Zone
```sql
-- Timestamp with numeric offset
SELECT TIMESTAMP '2024-12-25 14:30:00+02:00';
-- Result: 2024-12-25 14:30:00.0000+02:00

SELECT TIMESTAMP '2024-12-25 09:15:30-05:00';
-- Result: 2024-12-25 09:15:30.0000-05:00

-- Timestamp with named time zone
SELECT TIMESTAMP '2024-12-25 14:30:00 Europe/Berlin';
SELECT TIMESTAMP '2024-12-25 09:15:30 America/New_York';
SELECT TIMESTAMP '2024-12-25 22:45:00 Asia/Tokyo';

-- Current timestamp in different time zones
SELECT TIMESTAMP '2024-12-25 12:00:00 UTC';
SELECT TIMESTAMP '2024-12-25 13:00:00 Europe/London';
SELECT TIMESTAMP '2024-12-25 07:00:00 America/New_York';
```

### Datetime Literal Applications
```sql
-- Using datetime literals in DML operations
INSERT INTO events (event_name, event_date, start_time, created_at)
VALUES (
    'Annual Meeting',
    DATE '2024-12-25',
    TIME '14:30:00',
    TIMESTAMP '2024-01-15 10:30:00'
);

-- Datetime literals in WHERE clauses
SELECT * FROM orders
WHERE order_date = DATE '2024-12-25';

SELECT * FROM appointments  
WHERE appointment_time BETWEEN TIME '09:00:00' AND TIME '17:00:00';

SELECT * FROM log_entries
WHERE created_at >= TIMESTAMP '2024-01-01 00:00:00'
  AND created_at < TIMESTAMP '2024-02-01 00:00:00';

-- Datetime arithmetic with literals
SELECT 
    event_date,
    event_date + 7 as one_week_later,
    event_date - DATE '2024-01-01' as days_since_new_year
FROM events
WHERE event_date >= DATE '2024-12-01';
```

---

## NULL Literals

NULL represents unknown or missing values in ScratchBird's SQL implementation.

### NULL Literal Usage
```sql
-- Explicit NULL literal
SELECT NULL;
-- Result: NULL

-- NULL in comparisons (always results in UNKNOWN)
SELECT NULL = NULL;      -- NULL (unknown)
SELECT NULL <> 5;       -- NULL (unknown)
SELECT 'text' = NULL;   -- NULL (unknown)

-- Proper NULL testing
SELECT NULL IS NULL;     -- TRUE
SELECT NULL IS NOT NULL; -- FALSE
SELECT 5 IS NULL;        -- FALSE
```

### NULL in Data Operations
```sql
-- Inserting NULL values
INSERT INTO customers (id, name, email, phone)
VALUES (1, 'John Doe', 'john@example.com', NULL);

-- Querying for NULL values
SELECT * FROM customers WHERE phone IS NULL;
SELECT * FROM customers WHERE phone IS NOT NULL;

-- NULL in expressions
SELECT 
    name,
    email,
    COALESCE(phone, 'No phone provided') as phone_display
FROM customers;

-- NULL propagation in expressions
SELECT 
    5 + NULL as addition_result,     -- NULL
    'text' || NULL as concat_result, -- NULL
    NULL * 10 as multiplication_result -- NULL
;
```

---

## Literal Type Conversion and Casting

### Automatic Type Conversion
```sql
-- String to number conversion
SELECT '123' + 456;      -- 579 (automatic conversion)
SELECT '3.14' * 2;       -- 6.28

-- Number to string conversion
SELECT 'Value: ' || 123; -- 'Value: 123'

-- Date/time conversions
SELECT DATE '2024-12-25' + 7;  -- 2025-01-01
```

### Explicit Casting
```sql
-- CAST function for explicit conversion
SELECT CAST('123.45' AS DECIMAL(10,2));
SELECT CAST(123 AS VARCHAR(10));
SELECT CAST('2024-12-25' AS DATE);

-- Alternative casting syntax
SELECT '123.45'::DECIMAL(10,2);
SELECT 123::VARCHAR(10);
SELECT '14:30:00'::TIME;
```

### Type-Specific Literal Usage
```sql
-- Ensuring specific numeric types
SELECT CAST(123 AS SMALLINT);   -- SMALLINT
SELECT CAST(123 AS INTEGER);    -- INTEGER
SELECT CAST(123 AS BIGINT);     -- BIGINT
SELECT CAST(123 AS DECIMAL(10,2)); -- DECIMAL

-- Character type variations
SELECT CAST('text' AS CHAR(10));     -- Fixed-length
SELECT CAST('text' AS VARCHAR(100)); -- Variable-length
SELECT CAST('text' AS BLOB SUB_TYPE TEXT); -- BLOB
```

---

## Best Practices and Guidelines

### String Literal Best Practices
```sql
-- Use Q-quote syntax for complex strings containing quotes
-- Good:
SELECT Q'{JSON: {"name": "value", "key": "data"}}';

-- Instead of:
SELECT '{"name": "value", "key": "data"}';

-- Specify character sets for non-ASCII data
INSERT INTO multilingual_data (text_field)
VALUES (_UTF8 'Ñoño niño en España');

-- Use appropriate string length for data
CREATE TABLE efficient_strings (
    short_code CHAR(5),           -- Fixed length for codes
    description VARCHAR(255),     -- Variable length for descriptions
    large_text BLOB SUB_TYPE TEXT -- Large text data
);
```

### Numeric Literal Best Practices
```sql
-- Use appropriate numeric types for data ranges
CREATE TABLE optimized_numbers (
    tiny_number SMALLINT,         -- -32,768 to 32,767
    normal_number INTEGER,        -- -2.1B to 2.1B
    big_number BIGINT,           -- Very large integers
    precise_decimal DECIMAL(15,2), -- Financial data
    floating_point DOUBLE PRECISION -- Scientific calculations
);

-- Use hexadecimal for bit patterns and flags
SELECT 
    user_permissions & 0x04 as can_write,  -- Check write permission bit
    user_permissions & 0x02 as can_read,   -- Check read permission bit
    user_permissions & 0x01 as can_execute -- Check execute permission bit
FROM user_access;
```

### Datetime Literal Best Practices
```sql
-- Use ISO format for consistency
-- Good:
SELECT * FROM events WHERE event_date >= DATE '2024-01-01';

-- Include time zones for distributed applications
INSERT INTO global_events (event_name, event_time)
VALUES ('Conference Call', TIMESTAMP '2024-12-25 14:00:00 UTC');

-- Use appropriate precision for time values
CREATE TABLE time_tracking (
    start_time TIME,                    -- Standard time precision
    precise_timestamp TIMESTAMP,       -- Microsecond precision
    duration_seconds INTEGER           -- Duration as integer seconds
);
```

### NULL Handling Best Practices
```sql
-- Always use IS NULL / IS NOT NULL for NULL testing
-- Good:
SELECT * FROM customers WHERE phone IS NOT NULL;

-- Wrong:
-- SELECT * FROM customers WHERE phone <> NULL;  -- Always returns no rows

-- Provide defaults for NULL values
SELECT 
    customer_name,
    COALESCE(phone, 'Not provided') as phone_display,
    COALESCE(email, 'No email') as email_display
FROM customers;

-- Use NULLIF for conditional NULL assignment
SELECT 
    customer_name,
    NULLIF(phone, '') as clean_phone  -- Convert empty string to NULL
FROM customers;
```

---

## Common Pitfalls and Troubleshooting

### String Literal Issues
```sql
-- Issue: Incorrect quote escaping
-- Wrong:
-- SELECT 'Don't do this';  -- Syntax error

-- Correct:
SELECT 'Don''t do this';     -- Doubled apostrophe
SELECT Q'[Don't do this]';   -- Q-quote syntax

-- Issue: Character set mismatches
-- Be explicit about character sets for international data
INSERT INTO utf8_table (text_field) 
VALUES (_UTF8 'Proper encoding: ñoño');
```

### Numeric Literal Issues
```sql
-- Issue: Unexpected type assignment
SELECT typeof(123);          -- INTEGER
SELECT typeof(123.0);        -- NUMERIC
SELECT typeof(1.23E4);       -- DOUBLE PRECISION

-- Issue: Hexadecimal sign interpretation
SELECT 0x80000000;           -- -2147483648 (negative!)
SELECT 0x080000000;          -- 2147483648 (positive)

-- Solution: Be aware of type boundaries and use casting when needed
SELECT CAST(0x80000000 AS BIGINT); -- Explicit type
```

### Datetime Literal Issues
```sql
-- Issue: Ambiguous date formats
-- Depends on locale settings:
SELECT DATE '01/02/2024';     -- Jan 2 or Feb 1?

-- Solution: Use ISO format
SELECT DATE '2024-01-02';     -- Unambiguous

-- Issue: Time zone confusion
-- Local time vs UTC:
SELECT TIMESTAMP '2024-12-25 12:00:00';     -- Local time
SELECT TIMESTAMP '2024-12-25 12:00:00 UTC'; -- UTC time
```

