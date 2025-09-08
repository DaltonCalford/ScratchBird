# Advanced Domain Types Specification

## Overview

ScratchBird extends the traditional DOMAIN concept to support complex record types, advanced enums with positional arithmetic, and sets as first-class data types. This provides type safety, code reusability, and powerful data modeling capabilities.

## Record Domains

### Basic Record Definition

```sql
-- Create a record domain
CREATE DOMAIN person_name AS RECORD (
    first_name VARCHAR(50) NOT NULL,
    middle_name VARCHAR(50),
    last_name VARCHAR(50) NOT NULL
);

-- With constraints on fields
CREATE DOMAIN address AS RECORD (
    street1 VARCHAR(100) NOT NULL,
    street2 VARCHAR(100),
    city VARCHAR(50) NOT NULL,
    state CHAR(2) CHECK (state ~ '^[A-Z]{2}$'),
    postal_code VARCHAR(10) CHECK (postal_code ~ '^\d{5}(-\d{4})?$'),
    country VARCHAR(2) DEFAULT 'US'
);

-- Nested records
CREATE DOMAIN contact_info AS RECORD (
    name person_name NOT NULL,  -- Using another domain
    primary_address address,
    mailing_address address,
    phone VARCHAR(20),
    email VARCHAR(100) CHECK (email ~ '^[^@]+@[^@]+\.[^@]+$')
);
```

### Using Record Domains

```sql
-- Create table with record domain
CREATE TABLE customers (
    customer_id UUID GENERATED ALWAYS AS IDENTITY (UUID VERSION 7),
    contact contact_info NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert with record constructor
INSERT INTO customers (contact) VALUES (
    ROW(
        ROW('John', 'Q', 'Public'),  -- name
        ROW('123 Main St', NULL, 'Anytown', 'CA', '12345', 'US'),  -- primary_address
        NULL,  -- mailing_address
        '555-1234',
        'john@example.com'
    )::contact_info
);

-- Alternative syntax with named fields
INSERT INTO customers (contact) VALUES (
    RECORD(
        name := RECORD(
            first_name := 'Jane',
            middle_name := NULL,
            last_name := 'Doe'
        )::person_name,
        primary_address := RECORD(
            street1 := '456 Oak Ave',
            street2 := 'Apt 2B',
            city := 'Springfield',
            state := 'IL',
            postal_code := '62701',
            country := 'US'
        )::address,
        mailing_address := NULL,
        phone := '555-5678',
        email := 'jane@example.com'
    )::contact_info
);
```

### Extracting from Records

```sql
-- EXTRACT syntax for record fields
SELECT 
    EXTRACT(first_name FROM contact.name) AS first,
    EXTRACT(last_name FROM contact.name) AS last,
    EXTRACT(city FROM contact.primary_address) AS city
FROM customers;

-- Dot notation (alternative)
SELECT 
    contact.name.first_name,
    contact.name.last_name,
    contact.primary_address.city
FROM customers;

-- In WHERE clauses
SELECT * FROM customers
WHERE EXTRACT(state FROM contact.primary_address) = 'CA';

-- Nested extraction
SELECT 
    EXTRACT(first_name FROM EXTRACT(name FROM contact)) AS first
FROM customers;
```

### Record Methods

```sql
-- Define methods on record domains
CREATE DOMAIN person_name AS RECORD (
    first_name VARCHAR(50) NOT NULL,
    middle_name VARCHAR(50),
    last_name VARCHAR(50) NOT NULL
) WITH METHODS (
    -- Full name method
    FUNCTION full_name(self) RETURNS VARCHAR AS
        COALESCE(self.first_name || ' ', '') ||
        COALESCE(self.middle_name || ' ', '') ||
        COALESCE(self.last_name, ''),
    
    -- Initials method
    FUNCTION initials(self) RETURNS VARCHAR AS
        SUBSTRING(self.first_name, 1, 1) || '.' ||
        CASE WHEN self.middle_name IS NOT NULL 
            THEN SUBSTRING(self.middle_name, 1, 1) || '.' 
            ELSE '' 
        END ||
        SUBSTRING(self.last_name, 1, 1) || '.',
    
    -- Comparison method
    FUNCTION equals(self, other person_name) RETURNS BOOLEAN AS
        self.first_name = other.first_name AND
        COALESCE(self.middle_name, '') = COALESCE(other.middle_name, '') AND
        self.last_name = other.last_name
);

-- Using methods
SELECT 
    contact.name.full_name() AS full_name,
    contact.name.initials() AS initials
FROM customers;
```

## Enum Domains with Positional Arithmetic

### Basic Enum Definition

```sql
-- Simple enum
CREATE DOMAIN hex_digit AS ENUM (
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
);

-- Enum with explicit values
CREATE DOMAIN day_of_week AS ENUM (
    'Sunday' = 0,
    'Monday' = 1,
    'Tuesday' = 2,
    'Wednesday' = 3,
    'Thursday' = 4,
    'Friday' = 5,
    'Saturday' = 6
);

-- Enum with wrap-around behavior
CREATE DOMAIN month AS ENUM (
    'January', 'February', 'March', 'April', 'May', 'June',
    'July', 'August', 'September', 'October', 'November', 'December'
) WITH OPTIONS (
    WRAP = TRUE,  -- Enable wrap-around
    START_INDEX = 1  -- 1-based instead of 0-based
);
```

### Positional Arithmetic

```sql
-- Arithmetic operations on enums
DECLARE @hex hex_digit = 'F';
DECLARE @day day_of_week = 'Wednesday';
DECLARE @month month = 'December';

-- Position access (0-based by default)
SELECT POSITION(@hex);  -- Returns 15
SELECT POSITION(@day);  -- Returns 3

-- Arithmetic with wrapping
SELECT @hex + 3;  -- With WRAP=TRUE: Returns '2' (15+3=18, 18%16=2)
SELECT @month + 2;  -- Returns 'February' (December + 2 months)
SELECT @day - 10;  -- Returns 'Sunday' (3-10=-7, wraps to 0)

-- Without wrapping (default)
CREATE DOMAIN status AS ENUM ('New', 'Active', 'Completed', 'Archived');
DECLARE @status status = 'Completed';
SELECT @status + 1;  -- Returns 'Archived'
SELECT @status + 2;  -- ERROR: Enum overflow

-- Casting between representations
SELECT CAST(15 AS hex_digit);  -- Returns 'F'
SELECT CAST('F' AS INT) FROM hex_digit;  -- Returns 15
SELECT hex_digit::INT WHERE hex_digit = 'F';  -- Returns 15
```

### Advanced Enum Features

```sql
-- State machine domain
CREATE DOMAIN order_state AS ENUM (
    'Draft',
    'Submitted',
    'Processing',
    'Shipped',
    'Delivered',
    'Cancelled',
    'Refunded'
) WITH OPTIONS (
    WRAP = FALSE,
    TRANSITIONS = ARRAY[
        ['Draft', 'Submitted'],
        ['Draft', 'Cancelled'],
        ['Submitted', 'Processing'],
        ['Submitted', 'Cancelled'],
        ['Processing', 'Shipped'],
        ['Processing', 'Cancelled'],
        ['Shipped', 'Delivered'],
        ['Delivered', 'Refunded']
    ]
);

-- Validate state transitions
CREATE FUNCTION can_transition(
    from_state order_state,
    to_state order_state
) RETURNS BOOLEAN AS $$
BEGIN
    RETURN EXISTS (
        SELECT 1 FROM order_state::transitions
        WHERE transition[0] = from_state 
          AND transition[1] = to_state
    );
END;
$$ LANGUAGE plpgsql;

-- Bitwise enum (flags)
CREATE DOMAIN permissions AS ENUM FLAGS (
    'Read' = 1,
    'Write' = 2,
    'Delete' = 4,
    'Admin' = 8
);

-- Use bitwise operations
DECLARE @perms permissions = 'Read' | 'Write';  -- Value = 3
SELECT @perms & 'Write';  -- Returns TRUE
SELECT @perms | 'Delete';  -- Adds Delete permission
```

### Enum Ranges and Comparisons

```sql
-- Range operations
CREATE DOMAIN priority AS ENUM (
    'Critical', 'High', 'Medium', 'Low', 'Trivial'
) WITH OPTIONS (
    ORDERING = 'DESC'  -- Critical > High > Medium > Low > Trivial
);

-- Comparisons use positional values
SELECT * FROM tasks 
WHERE priority >= 'Medium'  -- Gets Critical, High, Medium
ORDER BY priority;  -- Orders by enum position

-- BETWEEN with enums
SELECT * FROM calendar_events
WHERE day_of_week BETWEEN 'Monday' AND 'Friday';

-- IN with enum ranges
SELECT * FROM products
WHERE status IN (enum_range('Active', 'Shipped'));
```

## Sets as First-Class Data Types

### Set Type Definition

```sql
-- Define SET as a special collection type
CREATE TYPE email_set AS SET OF VARCHAR(100);
CREATE TYPE tag_set AS SET OF VARCHAR(50);
CREATE TYPE id_set AS SET OF UUID;

-- Domain-based sets
CREATE DOMAIN permission_set AS SET OF permissions;
CREATE DOMAIN state_set AS SET OF order_state;

-- Record sets (result sets)
CREATE TYPE customer_result_set AS SET OF RECORD (
    id UUID,
    name VARCHAR(100),
    email VARCHAR(100),
    total_orders INT
);
```

### Set Operations

```sql
-- Create table with set columns
CREATE TABLE articles (
    article_id UUID GENERATED ALWAYS AS IDENTITY (UUID VERSION 7),
    title VARCHAR(200),
    tags tag_set,
    mentioned_ids id_set
);

-- Insert with set literals
INSERT INTO articles (title, tags, mentioned_ids) VALUES (
    'Database Design',
    SET['database', 'design', 'sql'],
    SET[
        '123e4567-e89b-12d3-a456-426614174000'::UUID,
        '987fcdeb-51a2-43d1-9f12-123456789abc'::UUID
    ]
);

-- Set operations
SELECT 
    title,
    CARDINALITY(tags) AS tag_count,
    tags @> SET['sql'] AS has_sql_tag,  -- Contains
    tags && SET['database', 'nosql'] AS has_db_tags,  -- Overlaps
    tags || SET['new'] AS tags_with_new,  -- Union
    tags - SET['design'] AS tags_without_design  -- Difference
FROM articles;

-- Set membership
SELECT * FROM articles
WHERE 'database' IN tags;

-- Set aggregation
SELECT 
    array_agg(DISTINCT tag) AS all_tags
FROM articles, UNNEST(tags) AS tag;
```

### Sets as Query Results

```sql
-- Function returning a set
CREATE FUNCTION get_active_customers() 
RETURNS SET OF customer_result_set AS $$
BEGIN
    RETURN QUERY
    SELECT 
        customer_id,
        name,
        email,
        COUNT(order_id) AS total_orders
    FROM customers c
    LEFT JOIN orders o ON c.customer_id = o.customer_id
    WHERE c.status = 'Active'
    GROUP BY customer_id, name, email;
END;
$$ LANGUAGE plpgsql;

-- Pass sets between procedures
CREATE PROCEDURE process_customer_batch(
    customers SET OF customer_result_set
) AS $$
DECLARE
    cust customer_result_set;
BEGIN
    FOR cust IN SELECT * FROM customers LOOP
        -- Process each customer
        PERFORM send_email(cust.email);
    END LOOP;
END;
$$ LANGUAGE plpgsql;

-- Call with set parameter
CALL process_customer_batch(get_active_customers());
```

### Set Optimizations

```sql
-- Lazy set evaluation
CREATE FUNCTION large_result_set() 
RETURNS SET OF record 
WITH (LAZY = TRUE) AS $$
BEGIN
    -- Set is materialized only as needed
    RETURN QUERY
    SELECT * FROM huge_table
    WHERE complex_condition;
END;
$$ LANGUAGE plpgsql;

-- Set caching
CREATE FUNCTION cached_lookup(key VARCHAR)
RETURNS SET OF record
WITH (CACHE = '5 minutes') AS $$
BEGIN
    RETURN QUERY
    SELECT * FROM lookup_table
    WHERE lookup_key = key;
END;
$$ LANGUAGE plpgsql;

-- Set streaming (for large results)
CREATE FUNCTION stream_results()
RETURNS SET OF record
WITH (STREAM = TRUE, BATCH_SIZE = 1000) AS $$
BEGIN
    -- Results streamed in batches
    RETURN QUERY
    SELECT * FROM massive_table;
END;
$$ LANGUAGE plpgsql;
```

## Pattern Validation for Domains

```sql
-- Domain with pattern validation
CREATE DOMAIN email AS VARCHAR(255)
    CHECK (VALUE ~ '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$')
    WITH VALIDATION (
        PATTERN = '^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$',
        ERROR_MESSAGE = 'Invalid email format',
        ERROR_HINT = 'Email must be in format: user@domain.com'
    );

-- Phone number with formatting
CREATE DOMAIN phone_number AS VARCHAR(20)
    WITH VALIDATION (
        PATTERN = '^\+?[1-9]\d{1,14}$',  -- E.164 format
        FORMATTER = 'format_phone_number',  -- Custom formatting function
        NORMALIZER = 'normalize_phone_number'  -- Strip formatting on store
    );

-- Complex validation with multiple patterns
CREATE DOMAIN product_code AS VARCHAR(20)
    WITH VALIDATION (
        PATTERNS = ARRAY[
            '^[A-Z]{3}-\d{4}$',  -- Format: ABC-1234
            '^\d{10}$',          -- Format: 1234567890
            '^[A-Z]\d[A-Z]\d[A-Z]\d$'  -- Format: A1B2C3
        ],
        ERROR_MESSAGE = 'Product code must match one of the accepted formats'
    );
```

## Domain Inheritance

```sql
-- Base domain
CREATE DOMAIN base_id AS VARCHAR(50)
    CHECK (LENGTH(VALUE) >= 5);

-- Inherited domain with additional constraints
CREATE DOMAIN customer_id AS base_id
    CHECK (VALUE ~ '^CUST-');

CREATE DOMAIN order_id AS base_id
    CHECK (VALUE ~ '^ORD-');

-- Record inheritance
CREATE DOMAIN base_entity AS RECORD (
    id UUID,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE DOMAIN customer_entity AS base_entity WITH (
    name VARCHAR(100) NOT NULL,
    email email NOT NULL,  -- Using email domain
    status order_state DEFAULT 'Draft'
);
```

## System Views for Domains

```sql
-- View all domains
CREATE VIEW sys.domains AS
SELECT 
    domain_schema,
    domain_name,
    domain_type,  -- 'SIMPLE', 'RECORD', 'ENUM', 'SET'
    base_type,
    is_nullable,
    default_value,
    check_constraints,
    enum_values,
    record_fields,
    options
FROM information_schema.domains;

-- Domain dependencies
CREATE VIEW sys.domain_dependencies AS
SELECT 
    domain_name,
    dependent_schema,
    dependent_table,
    dependent_column,
    dependency_type
FROM sys.domain_usage;

-- Enum domain values
CREATE VIEW sys.enum_values AS
SELECT 
    domain_schema,
    domain_name,
    enum_value,
    enum_position,
    is_default
FROM sys.enum_domains
ORDER BY domain_name, enum_position;
```

## Performance Considerations

### Storage Optimization

```sql
-- Compact storage for enums (stored as small integers)
CREATE DOMAIN status AS ENUM ('A', 'B', 'C') 
    WITH STORAGE (TYPE = 'INT1');  -- 1 byte storage

-- Record domains with compression
CREATE DOMAIN large_record AS RECORD (
    data1 TEXT,
    data2 TEXT,
    data3 TEXT
) WITH STORAGE (COMPRESSION = 'LZ4');

-- Set storage optimization
CREATE TYPE id_set AS SET OF UUID
    WITH STORAGE (
        STRUCTURE = 'BITMAP',  -- For dense sets
        COMPRESSION = 'RLE'     -- Run-length encoding
    );
```

### Indexing

```sql
-- Index on enum columns (uses positional values)
CREATE INDEX idx_priority ON tasks(priority);

-- Index on record field
CREATE INDEX idx_customer_email 
    ON customers((EXTRACT(email FROM contact)));

-- Index on set membership
CREATE INDEX idx_tags ON articles USING GIN(tags);

-- Partial index on enum value
CREATE INDEX idx_active_orders 
    ON orders(order_id) 
    WHERE status = 'Active';
```

## Migration and Compatibility

### From Traditional Columns

```sql
-- Convert separate columns to record domain
ALTER TABLE customers 
    ADD COLUMN name_new person_name;

UPDATE customers 
SET name_new = ROW(first_name, middle_name, last_name)::person_name;

ALTER TABLE customers 
    DROP COLUMN first_name,
    DROP COLUMN middle_name,
    DROP COLUMN last_name,
    RENAME COLUMN name_new TO name;

-- Convert VARCHAR to enum
ALTER TABLE orders 
    ALTER COLUMN status TYPE order_state 
    USING status::order_state;
```

This advanced domain system provides type safety, code reusability, and powerful modeling capabilities that go far beyond traditional SQL!