# **Types and Domains Specification**

## **1\. Introduction**

The ScratchBird type system is designed to be both comprehensive and powerful, providing a rich set of built-in primitive types while also offering an advanced, extensible DOMAIN system. A **Domain** in ScratchBird is far more than a simple type alias; it is a first-class database object that encapsulates data type, constraints, security rules, validation logic, and data quality rules into a single, reusable definition.

This "smart type" system ensures that data integrity and business rules are consistently enforced across the entire database—in tables, variables, and procedural code—by attaching the rules directly to the data's type.

## **2\. Core Data Types**

ScratchBird supports a wide range of standard SQL data types.

| Category | Types | Description |
| :---- | :---- | :---- |
| **Integers** | SMALLINT, INTEGER, BIGINT, INT128 | Signed integers of 16, 32, 64, and 128 bits. Unsigned variants (UINT8 to UINT64) are also available. |
| **Decimal** | DECIMAL(p,s), NUMERIC(p,s), MONEY, DECFLOAT(16/34) | Exact-precision decimal numbers. MONEY is a fixed-precision currency type. DECFLOAT provides IEEE-754 decimal floating. |
| **Floating-Point** | REAL, DOUBLE PRECISION | Approximate-precision floating-point numbers (32-bit and 64-bit). |
| **Character** | CHAR(n), VARCHAR(n), TEXT | Fixed-length, variable-length, and unlimited-length character strings. |
| **Binary** | BYTEA, BLOB, BINARY(n) | For storing raw binary data. |
| **Date/Time** | DATE, TIME, TIMESTAMP, INTERVAL, TIME WITH TIME ZONE, TIMESTAMP WITH TIME ZONE | Types for storing dates, times of day, timestamps (with optional time zone), and time intervals. |
| **Boolean** | BOOLEAN | Stores TRUE or FALSE truth values. |
| **Special** | UUID, JSON, JSONB, XML | Specialized types for universally unique identifiers and semi-structured documents. |
| **Arrays** | datatype\[\] | A one-dimensional or multi-dimensional array of any other data type (e.g., INTEGER\[\], VARCHAR(10)\[\]). |

### **2.1. Implementation Notes (Alpha)**
- **Fixed-length semantics:** CHAR(n) and BINARY(n) are fixed-length types. Values shorter than n are padded (CHAR uses spaces, BINARY uses 0x00). Overlength values raise an error (no silent truncation).
- **JSONB storage:** In Alpha, JSONB is stored as canonical text (same encoding as JSON). Binary JSON is a future optimization.
- **Arrays:** SQL arrays are defined as homogeneous and may be multi-dimensional, but the current on-disk encoding is a typed element list (see `DATA_TYPE_PERSISTENCE_AND_CASTS.md`). Dimension/bounds enforcement is a planned enhancement.
- **Domains:** Domain values are stored using the base type's canonical encoding. Domain constraints, security, and quality rules are enforced by the DomainManager at write/read time.

### **2.2. SBLR Coverage Notes (Alpha)**
SBLR type markers and typed literal opcodes must exist for all DataTypes to be
fully executable. Coverage gaps and planned opcode additions are tracked in:
- `docs/specifications/sblr/SBLR_OPCODE_REGISTRY.md`
- `docs/findings/SBLR_TYPE_OPCODE_GAPS.md`

## **3\. The DOMAIN Concept**

A DOMAIN allows you to create a custom data type based on a primitive type, but with a specific set of rules and behaviors.

### **3.1. Basic Domain Definition**

The simplest form of a domain adds reusable constraints to a base type.

\-- Create a domain for a product SKU  
CREATE DOMAIN product\_sku AS VARCHAR(20)  
    DEFAULT 'N/A'  
    NOT NULL  
    CHECK (VALUE \~ '^\[A-Z\]{3}-\[0-9\]{5}$'); \-- e.g., 'ABC-12345'

\-- Use the domain in a table  
CREATE TABLE products (  
    sku product\_sku PRIMARY KEY,  
    name TEXT  
);

\-- This insert will fail the domain's CHECK constraint  
INSERT INTO products (sku, name) VALUES ('INVALID-SKU', 'Test Product');

## **4\. Advanced Domain Types**

ScratchBird extends the DOMAIN concept to support complex, structured types.

### **4.1. Record Domains**

A RECORD domain defines a composite type, similar to a struct in other languages, containing multiple fields.

\-- Define a domain for a mailing address  
CREATE DOMAIN mailing\_address AS RECORD (  
    street\_line1 VARCHAR(100) NOT NULL,  
    street\_line2 VARCHAR(100),  
    city VARCHAR(50) NOT NULL,  
    state CHAR(2) NOT NULL,  
    postal\_code VARCHAR(10) NOT NULL  
);

\-- Use the record domain in a table  
CREATE TABLE customers (  
    id UUID PRIMARY KEY,  
    address mailing\_address  
);

\-- Insert a value using the ROW constructor  
INSERT INTO customers (id, address) VALUES (  
    '...',  
    ROW('123 Main St', NULL, 'Anytown', 'CA', '12345')::mailing\_address  
);

\-- Access fields using dot notation or EXTRACT  
SELECT address.city FROM customers;  
SELECT EXTRACT(city FROM address) FROM customers;
SELECT ALTER_ELEMENT(FIELD('city') IN address TO 'Boston') FROM customers;

### **4.2. Enum Domains**

An ENUM domain defines a type with a static, ordered set of allowed values.

\-- Define an enum for order status  
CREATE DOMAIN order\_status AS ENUM (  
    'DRAFT', 'SUBMITTED', 'PROCESSING', 'SHIPPED', 'DELIVERED', 'CANCELLED'  
) WITH OPTIONS (WRAP \= FALSE); \-- Do not wrap from last to first

\-- Use in a table  
CREATE TABLE orders (  
    id UUID PRIMARY KEY,  
    status order\_status DEFAULT 'DRAFT'  
);

\-- Manipulate using preferred SQL-style syntax  
DECLARE @current\_status order\_status \= 'SUBMITTED';

\-- Move to the next state in the enum's defined order  
SET NEXT VALUE FOR @current\_status; \-- @current\_status is now 'PROCESSING'

\-- Get the string value and integer position  
PRINT GET VALUE FOR @current\_status;     \-- Prints 'PROCESSING'  
PRINT GET POSITION FOR @current\_status; \-- Prints 2 (0-indexed)

### **4.3. Set Domains**

A SET defines a type representing an unordered collection of unique values of another type.

\-- Define a set of tags  
CREATE DOMAIN tag\_set AS SET OF VARCHAR(50);

\-- Use in a table  
CREATE TABLE articles (  
    id UUID PRIMARY KEY,  
    title TEXT,  
    tags tag\_set  
);

\-- Insert using the SET constructor  
INSERT INTO articles (id, title, tags) VALUES (  
    '...',  
    'Advanced Database Design',  
    SET\['sql', 'database', 'design'\]  
);

\-- Query using set operators  
\-- Check for containment (@\>)  
SELECT title FROM articles WHERE tags @\> SET\['sql'\];

\-- Check for overlap (&&)  
SELECT title FROM articles WHERE tags && SET\['nosql', 'design'\];

## **5\. Advanced Domain Features**

The true power of ScratchBird domains lies in attaching rich behavior and rules.

### **5.1. Security Rules (WITH SECURITY)**

Embed data protection rules directly into the type.

CREATE DOMAIN ssn AS VARCHAR(11)  
    CHECK (VALUE \~ '^\\d{3}-\\d{2}-\\d{4}$')  
    WITH SECURITY (  
        MASK\_FUNCTION \= 'mask\_ssn',           \-- Function to call for display  
        AUDIT\_ACCESS \= TRUE,                  \-- Log every time this data is read  
        REQUIRE\_PERMISSION \= 'view\_pii',      \-- Required permission to see unmasked  
        ENCRYPTION \= 'AES256'                 \-- Encrypt automatically at rest  
    );

### **5.2. Integrity Rules (WITH INTEGRITY)**

Enforce global uniqueness and normalization.

CREATE DOMAIN email\_address AS VARCHAR(255)  
    WITH INTEGRITY (  
        UNIQUE\_ACROSS\_DATABASE \= TRUE,     \-- Value must be unique in ALL tables  
        CASE\_INSENSITIVE \= TRUE,           \-- 'a@b.com' is the same as 'A@B.COM'  
        NORMALIZE\_FUNCTION \= 'normalize\_email' \-- e.g., trim and lowercase on write  
    );

### **5.3. Validation Rules (WITH VALIDATION)**

Define complex, multi-field, or external validation logic.

CREATE DOMAIN mailing\_address AS RECORD (...)  
    WITH VALIDATION (  
        VALIDATE\_FUNCTION \= 'validate\_address\_with\_usps\_api', \-- External check  
        ON\_VIOLATION \= 'REJECT',   \-- Or 'WARN', 'LOG'  
        ERROR\_MESSAGE \= 'The address could not be verified.'  
    );

### **5.4. Data Quality Rules (WITH QUALITY)**

Attach functions for parsing, standardizing, and enriching data upon write.

CREATE DOMAIN phone\_number AS VARCHAR(20)  
    WITH QUALITY (  
        PARSE\_FUNCTION \= 'parse\_phone\_input',     \-- Handle various formats  
        STANDARDIZE\_FUNCTION \= 'format\_to\_e164',  \-- Convert to a standard format  
        ENRICH\_FUNCTION \= 'lookup\_carrier\_info'   \-- Add metadata from an external source  
    );

### **5.5. Domain Inheritance**

Create specialized domains that inherit and extend the rules of a base domain.

\-- Base domain with common rules  
CREATE DOMAIN identifier AS VARCHAR(50)  
    CHECK (LENGTH(VALUE) \> 3);

\-- User ID inherits from identifier and adds a prefix check  
CREATE DOMAIN user\_id AS identifier  
    CHECK (VALUE LIKE 'user-%')  
    INHERITS identifier;

## **6\. Polymorphic Type (VARIANT)**

For maximum flexibility in procedural code, ScratchBird provides the VARIANT type, which can hold a value of any other type at runtime.

## **7\. Canonical Storage Encoding (Alpha)**

The canonical on-disk encoding for unencrypted heap tuples **must** match `TypedValue::serializePlainValue` unless explicitly noted. This guarantees that encrypted payloads, uniqueness indexes, and heap storage all round-trip with the same byte layout.
For implementation details and CAST error semantics, see `docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md`.

### **7.1. Numeric storage**
- **INT8/INT16/INT32/INT64/UINT8/UINT16/UINT32/UINT64**: stored as fixed-width integers (little-endian).
- **FLOAT32/FLOAT64**: stored as IEEE-754 floats (little-endian).
- **DECIMAL/NUMERIC**: stored as a **scaled integer only**. The storage width is determined by the column precision (not per-row):  
  - precision ≤ 2 → 1 byte  
  - precision ≤ 4 → 2 bytes  
  - precision ≤ 9 → 4 bytes  
  - precision ≤ 18 → 8 bytes  
  - precision ≤ 38 → 16 bytes  
  Scale is recorded in column metadata; no precision/scale bytes are stored in the tuple.
- **MONEY**: stored as a fixed-width `INT64` (scaled 10^-4).

### **7.2. String and binary storage**
- **CHAR/VARCHAR/TEXT/JSON/JSONB/XML**: stored as `uint32 length + bytes` (little-endian length) with **no implicit truncation**.
- **BINARY/VARBINARY/BLOB/BYTEA/VECTOR**: stored as `uint32 length + raw bytes` (little-endian length). No text conversion on write.

### **7.3. Temporal storage**
ScratchBird uses Firebird-compatible time formats plus a timezone offset:
- **DATE**: `int32 MJD` + `int32 offset_seconds` (UTC normalized using `server.time.date_default_time`).
- **TIME**: `int32 deci-ms` + `int32 offset_seconds`.
- **TIMESTAMP**: `int32 MJD` + `int32 deci-ms` + `int32 offset_seconds`.

All timestamps are normalized to UTC on write, with `offset_seconds` preserved for display and reconstruction.

### **7.4. UUID/INT128**
- **UUID/INT128**: fixed 16 bytes.

## **8\. CAST/CONVERT Semantics (Alpha)**

### **8.1. Mandatory conversions**
Every type must support conversion to string (`TEXT/VARCHAR`) and numeric types must support string-to-number conversions with deterministic error reporting.

Required behavior:
- **String → numeric**: reject non-numeric input with `INVALID_TEXT_REPRESENTATION`.
- **Numeric → string**: always supported using canonical formatting.
- **Temporal ↔ string**: `DATE/TIME/TIMESTAMP` parse and format `YYYY-MM-DD`, `HH:MM:SS[.ffff]`, and `YYYY-MM-DD HH:MM:SS[.ffff]` with optional timezone offsets.
- **Binary ↔ string**: uses `CAST(... USING <format>)`.

### **8.2. CAST ... USING formats**
Binary-to-text and text-to-binary casts accept:
- `USING HEX` (default)
- `USING BASE64`
- `USING ESCAPE`

Unsupported format names must fail with `NOT_SUPPORTED`.

### **8.3. Overlength handling**
`CHAR(n)` and `VARCHAR(n)` inserts and casts **must reject** values exceeding `n` with `STRING_DATA_RIGHT_TRUNCATION`. No hidden truncations.

### **8.4. Parser/SBLR integration**
Parser and SBLR CAST handling must follow this spec (Sections 7–8) and must transport the `USING` format into the resolved AST and bytecode.

\-- A generic logging procedure using VARIANT  
CREATE PROCEDURE log\_change(  
    @table\_name TEXT,  
    @column\_name TEXT,  
    @old\_value VARIANT,  
    @new\_value VARIANT  
) AS  
DECLARE  
    @old\_type TEXT;  
BEGIN  
    \-- Extract the runtime type of the value  
    SET @old\_type \= EXTRACT(DATATYPE FROM @old\_value);

    INSERT INTO audit\_log (table\_name, column\_name, old\_value\_text, old\_value\_type)  
    VALUES (@table\_name, @column\_name, CAST(@old\_value AS TEXT), @old\_type);

    \-- Perform type-specific logic  
    IF @new\_value IS OF TYPE NUMERIC THEN  
        \-- Do something specific for numbers  
    END IF;  
END;  
