# ScratchBird DOMAIN - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

A DOMAIN in ScratchBird is a user-defined data type that acts as a template based on existing data types. Domains provide a way to standardize data types across tables, enforce consistent validation rules, and centralize data type definitions for easier maintenance. They are particularly useful for creating reusable business data types with built-in constraints and default values.

### Key Features and Capabilities

- **Data Type Standardization**: Create consistent data types across multiple tables
- **Constraint Inheritance**: Define validation rules once and apply them everywhere
- **Default Value Management**: Set default values that apply to all domain usage
- **NULL Constraint Control**: Define whether domain fields can accept NULL values
- **Collation Specification**: Set character collation rules for text domains
- **Business Logic Encapsulation**: Embed business rules directly into data type definitions
- **Maintenance Efficiency**: Modify domain definition to update all dependent tables

### ScratchBird-Specific Enhancements

1. **Extended Data Type Support**: Domains can be based on all 52 ScratchBird data types
2. **Hierarchical Schema Integration**: Domains support 3-level qualified names (`schema.subschema.domain`)
3. **Advanced Constraint Validation**: Complex CHECK constraints with full SQL expression support
4. **Enhanced NULL Handling**: Flexible NULL/NOT NULL specification and modification
5. **Performance Optimization**: Efficient domain resolution and constraint validation
6. **Network Type Domains**: Support for INET, CIDR, and MACADDR domain types
7. **Range Type Domains**: Domains based on range types (INT4RANGE, DATERANGE, etc.)

---

## DDL Syntax Reference

### CREATE DOMAIN

Creates a new domain with specified data type, constraints, and default values.

#### **Basic Syntax**
```sql
CREATE DOMAIN [IF NOT EXISTS] [schema_name.]domain_name 
    [AS] data_type
    [DEFAULT default_value]
    [NOT NULL | NULL]
    [CHECK (validation_expression)]
    [COLLATE collation_name];
```

#### **Complete Syntax with All Options**
```sql
CREATE DOMAIN [IF NOT EXISTS] [[catalog.]schema.]domain_name 
    [AS] data_type_specification
    [DEFAULT {literal_value | NULL | USER | CURRENT_USER | CURRENT_TIMESTAMP | CURRENT_DATE | CURRENT_TIME | GEN_UUID()}]
    [NOT NULL]
    [CHECK (search_condition)]
    [COLLATE collation_name];
```

#### **Parameters**

- **IF NOT EXISTS**: Skip creation if domain already exists (no error)
- **schema_name**: Optional schema qualification (supports hierarchical schemas)
- **domain_name**: Domain identifier (63 characters max)
- **data_type**: Any valid ScratchBird data type (see DATA TYPES documentation)
- **DEFAULT**: Default value expression
- **NOT NULL**: Domain cannot accept NULL values
- **CHECK**: Validation constraint using VALUE keyword
- **COLLATE**: Character set collation for text types

---

## CREATE DOMAIN Examples

### **Basic Domain Creation**

#### **Simple Numeric Domain**
```sql
-- Create a domain for positive integers
CREATE DOMAIN positive_int AS INTEGER
    CHECK (VALUE > 0);

-- Create a domain for percentage values
CREATE DOMAIN percentage AS DECIMAL(5,2)
    DEFAULT 0.00
    CHECK (VALUE BETWEEN 0.00 AND 100.00);
```

#### **Text Domain with Constraints**
```sql
-- Create an email domain with validation
CREATE DOMAIN email_address AS VARCHAR(255)
    CHECK (VALUE SIMILAR TO '%@%.%');

-- Create a status domain with enumerated values
CREATE DOMAIN status_type AS VARCHAR(20)
    DEFAULT 'PENDING'
    CHECK (VALUE IN ('PENDING', 'ACTIVE', 'INACTIVE', 'SUSPENDED'));
```

### **Advanced Domain Examples**

#### **Business-Specific Domains**
```sql
-- Customer ID domain
CREATE DOMAIN customer_id AS INTEGER
    CHECK (VALUE > 0);

-- Product code domain with pattern validation
CREATE DOMAIN product_code AS CHAR(10)
    CHECK (VALUE SIMILAR TO '[A-Z]{3}-[0-9]{4}-[A-Z]{3}');

-- Price domain with business rules
CREATE DOMAIN price_amount AS DECIMAL(10,2)
    DEFAULT 0.00
    CHECK (VALUE >= 0.00 AND VALUE <= 999999.99);

-- Phone number domain
CREATE DOMAIN phone_number AS VARCHAR(20)
    CHECK (VALUE SIMILAR TO '[+]?[0-9 ().-]+');
```

#### **Date and Time Domains**
```sql
-- Future date domain
CREATE DOMAIN future_date AS DATE
    CHECK (VALUE > CURRENT_DATE);

-- Business hours domain
CREATE DOMAIN business_time AS TIME
    CHECK (VALUE BETWEEN TIME '08:00:00' AND TIME '18:00:00');

-- Audit timestamp domain
CREATE DOMAIN audit_timestamp AS TIMESTAMP
    DEFAULT CURRENT_TIMESTAMP
    NOT NULL;
```

### **ScratchBird Extended Type Domains**

#### **Network Address Domains**
```sql
-- IPv4 address domain
CREATE DOMAIN ipv4_address AS INET
    CHECK (ST_FAMILY(VALUE) = 4);

-- Private network domain
CREATE DOMAIN private_network AS CIDR
    CHECK (VALUE <<= CIDR '192.168.0.0/16' OR 
           VALUE <<= CIDR '10.0.0.0/8' OR 
           VALUE <<= CIDR '172.16.0.0/12');

-- MAC address domain
CREATE DOMAIN ethernet_mac AS MACADDR
    CHECK (VALUE IS NOT NULL);
```

#### **Range Type Domains**
```sql
-- Age range domain
CREATE DOMAIN age_range AS INT4RANGE
    CHECK (LOWER(VALUE) >= 0 AND UPPER(VALUE) <= 120);

-- Price range domain
CREATE DOMAIN price_range AS NUMRANGE
    CHECK (LOWER(VALUE) >= 0.00);

-- Working hours range
CREATE DOMAIN work_schedule AS TSRANGE
    CHECK (UPPER(VALUE) - LOWER(VALUE) <= INTERVAL '12 hours');
```

#### **Array and Vector Domains**
```sql
-- Tag array domain
CREATE DOMAIN tag_list AS VARCHAR(50)[]
    CHECK (ARRAY_LENGTH(VALUE) <= 10);

-- Feature vector domain for AI/ML
CREATE DOMAIN feature_vector AS VECTOR(128)
    CHECK (VECTOR_LENGTH(VALUE) = 128);

-- Coordinate point domain
CREATE DOMAIN location_point AS POINT
    CHECK (ST_X(VALUE) BETWEEN -180 AND 180 AND 
           ST_Y(VALUE) BETWEEN -90 AND 90);
```

### **Hierarchical Schema Domains**

#### **3-Level Schema Qualification**
```sql
-- Create domains in nested schemas
CREATE DOMAIN finance.accounting.currency_amount AS DECIMAL(15,2)
    DEFAULT 0.00
    CHECK (VALUE >= 0.00);

CREATE DOMAIN hr.payroll.salary_amount AS DECIMAL(12,2)
    CHECK (VALUE >= 15000.00);  -- Minimum wage check

CREATE DOMAIN inventory.warehouse.item_count AS INTEGER
    DEFAULT 0
    CHECK (VALUE >= 0);
```

### **Complex Validation Domains**

#### **Multi-Condition Constraints**
```sql
-- Credit score domain
CREATE DOMAIN credit_score AS SMALLINT
    CHECK (VALUE IS NULL OR (VALUE BETWEEN 300 AND 850));

-- Password strength domain
CREATE DOMAIN secure_password AS VARCHAR(100)
    CHECK (CHAR_LENGTH(VALUE) >= 8 AND
           VALUE SIMILAR TO '%[A-Z]%' AND     -- Contains uppercase
           VALUE SIMILAR TO '%[a-z]%' AND     -- Contains lowercase  
           VALUE SIMILAR TO '%[0-9]%' AND     -- Contains digit
           VALUE SIMILAR TO '%[!@#$%^&*()]%'); -- Contains special char

-- Geographic coordinate domain
CREATE DOMAIN geo_coordinate AS DECIMAL(10,7)
    CHECK ((VALUE IS NULL) OR 
           (VALUE BETWEEN -180.0000000 AND 180.0000000));
```

---

## ALTER DOMAIN

Modifies existing domain definitions, allowing changes to default values, constraints, and NULL specifications.

### **ALTER DOMAIN Syntax**
```sql
ALTER DOMAIN [schema_name.]domain_name
    { SET DEFAULT default_value |
      DROP DEFAULT |
      SET NOT NULL |
      DROP NOT NULL |
      ADD [CONSTRAINT constraint_name] CHECK (validation_expression) |
      DROP CONSTRAINT [constraint_name] };
```

### **ALTER DOMAIN Examples**

#### **Modifying Default Values**
```sql
-- Set new default value
ALTER DOMAIN status_type 
    SET DEFAULT 'ACTIVE';

-- Remove default value
ALTER DOMAIN price_amount 
    DROP DEFAULT;

-- Update default to current timestamp
ALTER DOMAIN audit_timestamp 
    SET DEFAULT CURRENT_TIMESTAMP;
```

#### **Changing NULL Constraints**
```sql
-- Make domain require NOT NULL
ALTER DOMAIN customer_id 
    SET NOT NULL;

-- Allow NULL values
ALTER DOMAIN phone_number 
    DROP NOT NULL;
```

#### **Adding and Removing Constraints**
```sql
-- Add new constraint
ALTER DOMAIN price_amount 
    ADD CONSTRAINT positive_price CHECK (VALUE > 0);

-- Add constraint with validation
ALTER DOMAIN email_address 
    ADD CHECK (CHAR_LENGTH(VALUE) >= 5);

-- Drop existing constraint
ALTER DOMAIN price_amount 
    DROP CONSTRAINT positive_price;

-- Drop all constraints (anonymous)
ALTER DOMAIN status_type 
    DROP CONSTRAINT;
```

#### **Complex Constraint Updates**
```sql
-- Update percentage domain with stricter validation
ALTER DOMAIN percentage 
    DROP CONSTRAINT;  -- Remove old constraint

ALTER DOMAIN percentage 
    ADD CHECK (VALUE BETWEEN 0.00 AND 100.00 AND 
               MOD(VALUE * 100, 1) = 0);  -- Must be whole percentage
```

---

## DROP DOMAIN

Removes domain definitions from the database. Domains can only be dropped if no tables or other database objects depend on them.

### **DROP DOMAIN Syntax**
```sql
DROP DOMAIN [IF EXISTS] [schema_name.]domain_name;
```

### **DROP DOMAIN Examples**

#### **Basic Domain Removal**
```sql
-- Drop domain if it exists
DROP DOMAIN IF EXISTS status_type;

-- Drop domain from specific schema
DROP DOMAIN finance.currency_amount;

-- Drop domain (will fail if in use)
DROP DOMAIN email_address;
```

#### **Dependency Checking**
```sql
-- Check domain dependencies before dropping
SELECT r.RDB$RELATION_NAME, rf.RDB$FIELD_NAME
FROM RDB$RELATION_FIELDS rf
JOIN RDB$RELATIONS r ON rf.RDB$RELATION_NAME = r.RDB$RELATION_NAME
WHERE rf.RDB$FIELD_SOURCE = 'CUSTOMER_ID';

-- Drop domain after removing dependencies
DROP DOMAIN customer_id;
```

---

## Using Domains in Table Definitions

### **Table Creation with Domains**

#### **Basic Domain Usage**
```sql
-- Create table using domains
CREATE TABLE customers (
    customer_id customer_id PRIMARY KEY,
    email_address email_address UNIQUE,
    status status_type,
    credit_score credit_score,
    created_at audit_timestamp
);

-- Domain defaults and constraints are inherited
INSERT INTO customers (customer_id, email_address) 
VALUES (1, 'john@example.com');
-- status defaults to 'PENDING', created_at to CURRENT_TIMESTAMP
```

#### **Advanced Domain Integration**
```sql
-- Financial application table
CREATE TABLE financial.transactions (
    transaction_id customer_id,  -- Reuses customer_id domain
    amount finance.accounting.currency_amount,
    percentage_fee percentage,
    transaction_date future_date,
    client_ip ipv4_address,
    tags tag_list
);

-- Product inventory table
CREATE TABLE inventory.products (
    product_id customer_id,  -- Reuses domain for IDs
    product_code product_code,
    unit_price price_amount,
    quantity inventory.warehouse.item_count,
    location location_point
);
```

### **Domain Override in Columns**

#### **Overriding Domain Constraints**
```sql
-- Override domain default but keep constraints
CREATE TABLE special_customers (
    customer_id customer_id,
    email_address email_address,
    status status_type DEFAULT 'VIP',  -- Override domain default
    priority_score credit_score NOT NULL  -- Add NOT NULL to nullable domain
);
```

#### **Domain with Additional Column Constraints**
```sql
CREATE TABLE validated_products (
    product_id customer_id PRIMARY KEY,  -- Add PRIMARY KEY
    product_code product_code UNIQUE,    -- Add UNIQUE constraint
    retail_price price_amount CHECK (price_amount > wholesale_price),
    wholesale_price price_amount,
    created_date audit_timestamp
);
```

---

## System Catalog Integration

ScratchBird stores domain definitions in the RDB$FIELDS system table along with field definitions.

### **Querying Domain Information**

#### **List All Domains**
```sql
-- Show all user-defined domains
SELECT 
    RDB$FIELD_NAME as DOMAIN_NAME,
    RDB$FIELD_TYPE as TYPE_CODE,
    RDB$FIELD_LENGTH as LENGTH,
    RDB$FIELD_SCALE as SCALE,
    RDB$DEFAULT_SOURCE as DEFAULT_VALUE,
    RDB$VALIDATION_SOURCE as CHECK_CONSTRAINT,
    RDB$NULL_FLAG as NOT_NULL_FLAG,
    RDB$CHARACTER_SET_ID as CHARSET_ID,
    RDB$COLLATION_ID as COLLATION_ID
FROM RDB$FIELDS 
WHERE RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0
ORDER BY RDB$FIELD_NAME;
```

#### **Domain Dependencies**
```sql
-- Find tables using specific domain
SELECT DISTINCT
    rf.RDB$RELATION_NAME as TABLE_NAME,
    rf.RDB$FIELD_NAME as COLUMN_NAME,
    rf.RDB$FIELD_SOURCE as DOMAIN_NAME
FROM RDB$RELATION_FIELDS rf
WHERE rf.RDB$FIELD_SOURCE = 'EMAIL_ADDRESS'
ORDER BY rf.RDB$RELATION_NAME, rf.RDB$FIELD_NAME;
```

#### **Domain Details with Data Type Information**
```sql
-- Complete domain information
SELECT 
    f.RDB$FIELD_NAME as DOMAIN_NAME,
    CASE f.RDB$FIELD_TYPE
        WHEN 7 THEN 'SMALLINT'
        WHEN 8 THEN 'INTEGER' 
        WHEN 16 THEN 'BIGINT'
        WHEN 10 THEN 'FLOAT'
        WHEN 27 THEN 'DOUBLE PRECISION'
        WHEN 12 THEN 'DATE'
        WHEN 13 THEN 'TIME'
        WHEN 35 THEN 'TIMESTAMP'
        WHEN 37 THEN 'VARCHAR'
        WHEN 261 THEN 'BLOB'
        WHEN 31 THEN 'USMALLINT'     -- ScratchBird extension
        WHEN 32 THEN 'UINTEGER'      -- ScratchBird extension
        WHEN 33 THEN 'UBIGINT'       -- ScratchBird extension
        WHEN 36 THEN 'INET'          -- ScratchBird extension
        WHEN 37 THEN 'CIDR'          -- ScratchBird extension
        WHEN 40 THEN 'INT4RANGE'     -- ScratchBird extension
        ELSE 'OTHER'
    END as DATA_TYPE,
    f.RDB$FIELD_LENGTH as LENGTH,
    f.RDB$FIELD_SCALE as SCALE,
    f.RDB$DEFAULT_SOURCE as DEFAULT_EXPRESSION,
    f.RDB$VALIDATION_SOURCE as CHECK_CONSTRAINT,
    CASE f.RDB$NULL_FLAG 
        WHEN 1 THEN 'NOT NULL' 
        ELSE 'NULLABLE' 
    END as NULL_CONSTRAINT
FROM RDB$FIELDS f
WHERE (f.RDB$SYSTEM_FLAG IS NULL OR f.RDB$SYSTEM_FLAG = 0)
  AND f.RDB$FIELD_NAME NOT STARTING WITH 'RDB$'
ORDER BY f.RDB$FIELD_NAME;
```

---

## Advanced Domain Features

### **Domain Validation Functions**

#### **Custom Validation with SQL Functions**
```sql
-- Create validation function
CREATE FUNCTION validate_credit_card(card_number VARCHAR(20))
RETURNS BOOLEAN
AS
BEGIN
    -- Luhn algorithm implementation
    RETURN (MOD(luhn_checksum(card_number), 10) = 0);
END;

-- Create domain using custom function
CREATE DOMAIN credit_card_number AS VARCHAR(20)
    CHECK (validate_credit_card(VALUE));
```

#### **Date Range Validation**
```sql
-- Domain for dates within business years
CREATE DOMAIN business_date AS DATE
    CHECK (EXTRACT(YEAR FROM VALUE) BETWEEN 2020 AND 2030 AND
           EXTRACT(MONTH FROM VALUE) BETWEEN 1 AND 12);

-- Domain for future dates only
CREATE DOMAIN future_timestamp AS TIMESTAMP
    CHECK (VALUE > CURRENT_TIMESTAMP);
```

### **Collation and Character Set Domains**

#### **Text Domains with Collation**
```sql
-- Case-insensitive domain
CREATE DOMAIN case_insensitive_name AS VARCHAR(100)
    COLLATE EN_US_CI;

-- Unicode text domain
CREATE DOMAIN unicode_text AS VARCHAR(1000) CHARACTER SET UTF8
    COLLATE UNICODE_CI;

-- Binary comparison domain
CREATE DOMAIN exact_match_code AS VARCHAR(50)
    COLLATE BINARY;
```

### **Array and Complex Type Domains**

#### **Array Domains with Validation**
```sql
-- String array with size limits
CREATE DOMAIN keyword_list AS VARCHAR(50)[]
    CHECK (ARRAY_LENGTH(VALUE) BETWEEN 1 AND 20);

-- Numeric array with value constraints
CREATE DOMAIN score_array AS INTEGER[]
    CHECK (ALL(SELECT item FROM UNNEST(VALUE) AS item WHERE item BETWEEN 0 AND 100));
```

#### **JSON Domains with Schema Validation**
```sql
-- JSON domain with structure validation
CREATE DOMAIN user_profile AS JSON
    CHECK (JSON_VALID(VALUE) AND 
           JSON_EXTRACT(VALUE, '$.name') IS NOT NULL AND
           JSON_EXTRACT(VALUE, '$.email') IS NOT NULL);
```

---

## Performance Considerations

### **Domain Resolution Performance**

1. **Index Compatibility**: Domains inherit indexing characteristics from base types
2. **Constraint Evaluation**: CHECK constraints are evaluated on every INSERT/UPDATE
3. **Schema Resolution**: 3-level qualified domains may have slight resolution overhead
4. **Memory Usage**: Domain metadata is cached for efficient repeated access

### **Optimization Strategies**

#### **Efficient Domain Design**
```sql
-- Good: Simple, fast constraint
CREATE DOMAIN positive_id AS INTEGER 
    CHECK (VALUE > 0);

-- Less optimal: Complex constraint with function calls
CREATE DOMAIN complex_validation AS VARCHAR(100)
    CHECK (UPPER(TRIM(VALUE)) SIMILAR TO '[A-Z0-9 ]+' AND
           CHAR_LENGTH(TRIM(VALUE)) >= 5 AND
           NOT EXISTS (SELECT 1 FROM blacklisted_values WHERE value = UPPER(TRIM(VALUE))));
```

#### **Index-Friendly Domains**
```sql
-- Domains that support efficient indexing
CREATE DOMAIN indexed_email AS VARCHAR(255)
    CHECK (VALUE SIMILAR TO '%@%.%');

-- Use domain in indexed columns
CREATE TABLE users (
    user_id INTEGER PRIMARY KEY,
    email indexed_email,
    INDEX idx_email (email)  -- Domain supports indexing
);
```

---

## Administrative Operations

### **Domain Migration and Updates**

#### **Safe Domain Updates**
```sql
-- Step 1: Check dependencies
SELECT COUNT(*) as USAGE_COUNT
FROM RDB$RELATION_FIELDS 
WHERE RDB$FIELD_SOURCE = 'OLD_DOMAIN';

-- Step 2: Create new domain
CREATE DOMAIN new_domain AS VARCHAR(300)  -- Increased size
    CHECK (VALUE SIMILAR TO '%@%.%');

-- Step 3: Update table columns (requires manual process)
ALTER TABLE customers ALTER COLUMN email TYPE new_domain;

-- Step 4: Drop old domain
DROP DOMAIN old_domain;
```

#### **Domain Documentation**
```sql
-- Add comments to domains (requires RDB$FIELDS description)
COMMENT ON DOMAIN email_address IS 
    'Standard email address format validation for all user communications';

COMMENT ON DOMAIN currency_amount IS 
    'Monetary amounts in base currency units with 2 decimal precision';
```

### **Backup and Restore Considerations**

- Domains are included in database backups as DDL statements
- Domain dependencies are preserved during restore operations
- Constraint validation occurs during data restore
- Schema-qualified domains maintain their schema hierarchy

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:2186-2241`
- **Classes**: `domain_clause`, `domain_default`, `domain_constraints`
- **Functionality**: Parsing CREATE/ALTER/DROP DOMAIN syntax

#### **DDL Node Classes**
- **File**: `src/dsql/DdlNodes.h:931-1044`
- **Classes**:
  - `CreateDomainNode` (lines 931-971): Domain creation logic
  - `AlterDomainNode` (lines 974-1026): Domain modification operations  
  - `DropDomainNode` (lines 1029-1044): Domain removal with dependency checking

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:46-75`
- **Table**: `RDB$FIELDS` - Stores domain definitions
- **Fields**:
  - `f_fld_name`: Domain name
  - `f_fld_v_blr`, `f_fld_v_source`: CHECK constraint validation
  - `f_fld_default`, `f_fld_dsource`: Default value definitions
  - `f_fld_null_flag`: NOT NULL constraint flag

#### **Data Type Integration**
- **File**: `src/include/firebird/impl/dsc_pub.h:43-113`
- **Integration**: Domains support all 52 ScratchBird data types
- **Extensions**: Network types, range types, vector types, geometric types

### **Core Classes and Functions**

#### **CreateDomainNode Methods**
- `checkPermission()`: Validates schema access rights
- `execute()`: Creates domain in system catalog
- Constraint validation and default value processing

#### **AlterDomainNode Methods**
- `checkUpdate()`: Validates domain modification safety
- `getDomainType()`: Retrieves current domain definition
- `modifyLocalFieldIndex()`: Updates dependent indexes

#### **DropDomainNode Methods**
- `deleteDimensionRecords()`: Cleans up array dimension metadata
- Dependency checking before domain removal
- Cascade deletion of related constraints

### **Storage Structures**

Domains are stored in the RDB$FIELDS system table with the following key characteristics:

- **Name Storage**: Domain names with schema qualification
- **Type Information**: Base data type, length, scale, precision
- **Constraint Storage**: CHECK constraints stored as BLR and source text
- **Default Values**: Default expressions stored as BLR and source text
- **Metadata Flags**: System flags, NULL constraints, character set information

---

## Error Handling and Troubleshooting

### **Common Domain Errors**

#### **Domain Creation Errors**
```sql
-- Error: Domain already exists
CREATE DOMAIN email_address AS VARCHAR(255);
-- Solution: Use IF NOT EXISTS or choose different name

-- Error: Invalid base type
CREATE DOMAIN invalid_domain AS NONEXISTENT_TYPE;
-- Solution: Use valid ScratchBird data type

-- Error: Invalid constraint
CREATE DOMAIN bad_domain AS INTEGER CHECK (INVALID_FUNCTION(VALUE));
-- Solution: Use valid SQL expressions in constraints
```

#### **Domain Usage Errors**
```sql
-- Error: Domain constraint violation
INSERT INTO customers (email) VALUES ('invalid-email');
-- Solution: Ensure data matches domain constraints

-- Error: Cannot drop domain in use
DROP DOMAIN email_address;
ERROR: Domain EMAIL_ADDRESS is used in table CUSTOMERS(EMAIL)
-- Solution: Remove dependencies before dropping domain
```

### **Debugging Domain Issues**

#### **Constraint Validation Problems**
```sql
-- Test domain constraint separately
SELECT 'test@example.com' WHERE 'test@example.com' SIMILAR TO '%@%.%';

-- Check domain definition
SELECT RDB$VALIDATION_SOURCE 
FROM RDB$FIELDS 
WHERE RDB$FIELD_NAME = 'EMAIL_ADDRESS';
```

#### **Performance Issues**
```sql
-- Identify expensive domain constraints
SELECT 
    rf.RDB$RELATION_NAME,
    rf.RDB$FIELD_NAME,
    f.RDB$VALIDATION_SOURCE
FROM RDB$RELATION_FIELDS rf
JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME
WHERE f.RDB$VALIDATION_SOURCE CONTAINING 'FUNCTION'
   OR f.RDB$VALIDATION_SOURCE CONTAINING 'SELECT';
```

---

## Best Practices

### **Domain Design Guidelines**

1. **Naming Conventions**: Use descriptive, business-oriented names
2. **Constraint Simplicity**: Keep CHECK constraints simple and fast
3. **Schema Organization**: Use hierarchical schemas for domain categorization
4. **Documentation**: Comment domains for future maintenance
5. **Testing**: Validate domain constraints thoroughly before deployment

### **Recommended Domain Patterns**

#### **Standard Business Domains**
```sql
-- Financial domains
CREATE DOMAIN money AS DECIMAL(15,2) CHECK (VALUE >= 0);
CREATE DOMAIN percentage AS DECIMAL(5,2) CHECK (VALUE BETWEEN 0 AND 100);

-- Identity domains  
CREATE DOMAIN positive_id AS INTEGER CHECK (VALUE > 0);
CREATE DOMAIN uuid_type AS UUID DEFAULT GEN_UUID();

-- Text domains
CREATE DOMAIN email AS VARCHAR(320) CHECK (VALUE SIMILAR TO '%@%.%');
CREATE DOMAIN phone AS VARCHAR(20) CHECK (VALUE SIMILAR TO '[+]?[0-9 ().-]+');

-- Status domains
CREATE DOMAIN status AS VARCHAR(20) DEFAULT 'ACTIVE' 
    CHECK (VALUE IN ('ACTIVE', 'INACTIVE', 'PENDING', 'DELETED'));
```

---

## Migration from Other Database Systems

### **From PostgreSQL**
PostgreSQL domains translate directly to ScratchBird with minimal changes:
```sql
-- PostgreSQL syntax (mostly compatible)
CREATE DOMAIN us_postal_code AS TEXT
    CHECK(VALUE ~ '^\d{5}$' OR VALUE ~ '^\d{5}-\d{4}$');

-- ScratchBird equivalent
CREATE DOMAIN us_postal_code AS VARCHAR(10)
    CHECK (VALUE SIMILAR TO '[0-9]{5}' OR VALUE SIMILAR TO '[0-9]{5}-[0-9]{4}');
```

### **From Oracle**
Oracle doesn't have direct domain equivalent, use CHECK constraints:
```sql
-- Oracle CHECK constraint
ALTER TABLE customers ADD CONSTRAINT ck_email 
    CHECK (email_address LIKE '%@%.%');

-- ScratchBird domain approach
CREATE DOMAIN email_address AS VARCHAR(255)
    CHECK (VALUE SIMILAR TO '%@%.%');

CREATE TABLE customers (
    customer_id INTEGER,
    email_address email_address
);
```

