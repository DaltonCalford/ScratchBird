# Firebird Domains and Extended Type Support

## Firebird DOMAIN Support

### What are Domains?

Domains in Firebird are user-defined data types that encapsulate:
- Base data type
- Constraints (CHECK, NOT NULL)
- Default values
- Collations
- Character sets

They provide type safety and reusability across tables.

### Basic Domain Syntax

```sql
-- Simple domain with constraint
CREATE DOMAIN EMAIL AS VARCHAR(255)
    CHECK (VALUE LIKE '%@%.%')
    NOT NULL;

-- Domain with default
CREATE DOMAIN AUDIT_TIMESTAMP AS TIMESTAMP
    DEFAULT CURRENT_TIMESTAMP
    NOT NULL;

-- Numeric domain with range
CREATE DOMAIN PERCENTAGE AS DECIMAL(5,2)
    CHECK (VALUE BETWEEN 0 AND 100);

-- Using domains in tables
CREATE TABLE users (
    id INTEGER,
    email EMAIL,  -- Uses EMAIL domain
    created_at AUDIT_TIMESTAMP,
    discount PERCENTAGE
);
```

## Extended Domain Features for ScratchBird

### 1. Enum Domains with Position Support

```sql
-- Create enum domain with positional access
CREATE DOMAIN STATUS_ENUM AS VARCHAR(20)
    ENUM ('pending', 'processing', 'completed', 'cancelled')
    WITH POSITION;

-- Usage examples
DECLARE @status STATUS_ENUM = 'pending';

-- Get position (0-based)
SELECT POSITION(@status);  -- Returns 0

-- Move to next position
SET @status = NEXT(@status);  -- Now 'processing'

-- Move with wrapping
CREATE DOMAIN DAY_OF_WEEK AS VARCHAR(10)
    ENUM ('Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun')
    WITH POSITION WRAP;

DECLARE @day DAY_OF_WEEK = 'Sun';
SET @day = NEXT(@day);  -- Wraps to 'Mon'

-- Arithmetic on enums
SET @day = @day + 3;  -- Advance 3 positions
SET @day = @day - 1;  -- Go back 1 position
```

### 2. Complex/Record Type Domains

```sql
-- Create composite domain
CREATE DOMAIN ADDRESS_TYPE AS RECORD (
    street VARCHAR(100),
    city VARCHAR(50),
    state CHAR(2),
    zip VARCHAR(10),
    country VARCHAR(50) DEFAULT 'USA'
);

-- Create domain with nested records
CREATE DOMAIN PERSON_TYPE AS RECORD (
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    birth_date DATE,
    address ADDRESS_TYPE,
    phones VARCHAR(20) ARRAY[5]
);

-- Usage in tables
CREATE TABLE customers (
    id INTEGER PRIMARY KEY,
    info PERSON_TYPE,
    billing_address ADDRESS_TYPE
);

-- Accessing fields
SELECT 
    id,
    info.first_name,
    info.address.city,
    info.phones[1]
FROM customers;

-- Inserting records
INSERT INTO customers (id, info) VALUES (
    1,
    ROW(
        'John',           -- first_name
        'Doe',            -- last_name
        DATE '1990-01-01', -- birth_date
        ROW(              -- address
            '123 Main St',
            'New York',
            'NY',
            '10001',
            'USA'
        ),
        ARRAY['555-1234', '555-5678']  -- phones
    )::PERSON_TYPE
);
```

### 3. Constrained Array Domains

```sql
-- Array domain with size constraint
CREATE DOMAIN EMAIL_LIST AS VARCHAR(255) ARRAY[10]
    CHECK (EVERY(VALUE LIKE '%@%.%'));

-- Array domain with element constraints
CREATE DOMAIN POSITIVE_NUMBERS AS INTEGER ARRAY
    CHECK (EVERY(VALUE > 0));

-- Multi-dimensional array domain
CREATE DOMAIN MATRIX_3X3 AS DOUBLE PRECISION ARRAY[3][3]
    CHECK (ARRAY_DIMS(VALUE) = '[1:3][1:3]');
```

## Unsigned Integer Support

### Native Unsigned Types

```sql
-- Unsigned integer types
CREATE DOMAIN UINT8 AS SMALLINT
    CHECK (VALUE BETWEEN 0 AND 255);

CREATE DOMAIN UINT16 AS INTEGER
    CHECK (VALUE BETWEEN 0 AND 65535);

CREATE DOMAIN UINT32 AS BIGINT
    CHECK (VALUE BETWEEN 0 AND 4294967295);

CREATE DOMAIN UINT64 AS DECIMAL(20,0)
    CHECK (VALUE BETWEEN 0 AND 18446744073709551615);

-- Better: Native implementation
UINT8    -- 0 to 255
UINT16   -- 0 to 65,535
UINT32   -- 0 to 4,294,967,295
UINT64   -- 0 to 18,446,744,073,709,551,615
```

### Implementation in ScratchBird

```cpp
// Internal type system
enum UniversalType {
    // Signed integers
    INT8, INT16, INT32, INT64,
    
    // Unsigned integers (ADDED)
    UINT8, UINT16, UINT32, UINT64,
    
    // ... other types
};

// Type mapping
struct TypeMapping {
    // MySQL has native unsigned
    {"TINYINT UNSIGNED", UINT8},
    {"SMALLINT UNSIGNED", UINT16},
    {"INT UNSIGNED", UINT32},
    {"BIGINT UNSIGNED", UINT64},
    
    // PostgreSQL - use domains
    {"uint8", UINT8},   // Custom domain
    {"uint16", UINT16}, // Custom domain
    {"uint32", UINT32}, // Custom domain
    {"uint64", UINT64}, // Custom domain
    
    // Firebird - use domains or native
    {"UINT8", UINT8},
    {"UINT16", UINT16},
    {"UINT32", UINT32},
    {"UINT64", UINT64},
};
```

## Domain Inheritance

```sql
-- Base domain
CREATE DOMAIN POSITIVE_INT AS INTEGER
    CHECK (VALUE > 0);

-- Derived domain with additional constraints
CREATE DOMAIN SMALL_POSITIVE_INT AS POSITIVE_INT
    CHECK (VALUE <= 1000);

-- Multi-level inheritance
CREATE DOMAIN ID_TYPE AS POSITIVE_INT
    NOT NULL;

CREATE DOMAIN USER_ID AS ID_TYPE;
CREATE DOMAIN ORDER_ID AS ID_TYPE;

-- Type safety
DECLARE @uid USER_ID = 123;
DECLARE @oid ORDER_ID = 456;
-- @uid = @oid;  -- Type error: incompatible domains
```

## Domain Methods and Operators

```sql
-- Domain with methods
CREATE DOMAIN CURRENCY AS DECIMAL(19,4)
    WITH METHODS (
        FUNCTION format() RETURNS VARCHAR(50)
        AS BEGIN
            RETURN '$' || TO_CHAR(SELF, '999,999,999.99');
        END,
        
        FUNCTION add_tax(rate DECIMAL) RETURNS CURRENCY
        AS BEGIN
            RETURN SELF * (1 + rate);
        END
    );

-- Usage
DECLARE @price CURRENCY = 99.99;
SELECT @price.format();  -- Returns '$99.99'
SELECT @price.add_tax(0.08);  -- Returns 107.99
```

## Pattern Domains

```sql
-- Phone number domain with pattern
CREATE DOMAIN PHONE_NUMBER AS VARCHAR(20)
    PATTERN '^\+?[1-9]\d{1,14}$'  -- E.164 format
    WITH METHODS (
        FUNCTION format_us() RETURNS VARCHAR(20)
        AS BEGIN
            -- Format as (XXX) XXX-XXXX
            RETURN '(' || SUBSTRING(SELF, 1, 3) || ') ' ||
                   SUBSTRING(SELF, 4, 3) || '-' ||
                   SUBSTRING(SELF, 7, 4);
        END
    );

-- Email with validation
CREATE DOMAIN EMAIL_ADDRESS AS VARCHAR(255)
    PATTERN '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
    WITH METHODS (
        FUNCTION domain() RETURNS VARCHAR(255)
        AS BEGIN
            RETURN SUBSTRING(SELF FROM POSITION('@' IN SELF) + 1);
        END,
        
        FUNCTION local_part() RETURNS VARCHAR(255)
        AS BEGIN
            RETURN SUBSTRING(SELF FROM 1 FOR POSITION('@' IN SELF) - 1);
        END
    );
```

## Smart Enum Domains

```sql
-- State machine enum
CREATE DOMAIN ORDER_STATE AS VARCHAR(20)
    ENUM ('draft', 'submitted', 'approved', 'shipped', 'delivered', 'cancelled')
    WITH TRANSITIONS (
        'draft' -> ('submitted', 'cancelled'),
        'submitted' -> ('approved', 'cancelled'),
        'approved' -> ('shipped', 'cancelled'),
        'shipped' -> ('delivered'),
        'delivered' -> (),  -- Terminal state
        'cancelled' -> ()   -- Terminal state
    );

-- Usage with validation
DECLARE @state ORDER_STATE = 'draft';
SET @state = 'submitted';  -- OK
SET @state = 'delivered';  -- ERROR: Invalid transition

-- Check valid transitions
SELECT NEXT_STATES(@state);  -- Returns ('approved', 'cancelled')
```

## Implementation Strategy

### Phase 1: Basic Domains
```sql
-- Support basic domain creation
CREATE DOMAIN domain_name AS base_type
    [DEFAULT default_value]
    [NOT NULL]
    [CHECK (condition)];
```

### Phase 2: Enum Domains
```sql
-- Add enum support with position
CREATE DOMAIN name AS base_type
    ENUM (values)
    [WITH POSITION [WRAP]];
```

### Phase 3: Complex Types
```sql
-- Add record/composite types
CREATE DOMAIN name AS RECORD (
    field1 type1,
    field2 type2
);
```

### Phase 4: Advanced Features
- Domain inheritance
- Domain methods
- Pattern matching
- State machines

## Compatibility Mapping

| Feature | Firebird | PostgreSQL | MySQL | MSSQL | ScratchBird |
|---------|----------|------------|-------|-------|-------------|
| Basic Domains | ✅ | ✅ (CREATE TYPE) | ❌ | ✅ (CREATE TYPE) | ✅ |
| Check Constraints | ✅ | ✅ | ❌ | ✅ | ✅ |
| Enum Types | ❌ | ✅ | ✅ | ❌ | ✅ Enhanced |
| Record Types | ❌ | ✅ (Composite) | ❌ | ✅ (Table Type) | ✅ |
| Unsigned Integers | ❌ | ❌ | ✅ | ❌ | ✅ Native |
| Domain Methods | ❌ | ❌ | ❌ | ❌ | ✅ New |
| Position Enums | ❌ | ❌ | ❌ | ❌ | ✅ New |

## Benefits

1. **Type Safety**: Catch errors at domain level
2. **Reusability**: Define once, use everywhere
3. **Maintainability**: Central type definitions
4. **Documentation**: Self-documenting schema
5. **Validation**: Built-in business rules
6. **Migration**: Easier schema evolution