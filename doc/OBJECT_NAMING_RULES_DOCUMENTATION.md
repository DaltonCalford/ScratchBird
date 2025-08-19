# ScratchBird Object Naming Rules - Complete Reference Documentation

## Overview

**Object Naming Rules** define how database objects (tables, columns, procedures, functions, etc.) can be named in ScratchBird. Understanding these rules is essential for creating maintainable and portable database schemas that follow SQL standards while leveraging ScratchBird's advanced naming features.

### Key Features

ScratchBird provides comprehensive identifier support with enterprise-grade features:

- **Standard SQL Compliance**: PostgreSQL-style 63-character identifier limits
- **Unicode Support**: Full UTF-8 encoding with international character support
- **PascalCase Mode**: Unique support for modern PascalCase naming conventions
- **Hierarchical Schema Names**: Advanced multi-level schema qualification
- **Flexible Quoting**: Standard SQL double-quote escaping with case preservation
- **Comprehensive Reserved Words**: Extensive keyword system with alternatives

---

## Identifier Syntax and Structure

### Basic Identifier Rules

All ScratchBird identifiers must follow these fundamental rules:

#### **Character Requirements**
```sql
-- Valid identifier characters
identifier ::= 
    <letter> { <letter> | <digit> | <special_char> }*

<letter> ::= A-Z | a-z | Unicode letters
<digit> ::= 0-9
<special_char> ::= _ | $ | { | }
```

#### **Length Limitations**
```sql
-- Maximum identifier lengths
Maximum Characters: 63
Maximum Bytes: 252 (UTF-8 encoding, 4 bytes per character)
Buffer Size: 253 bytes (includes null terminator)

-- Examples within limits
CREATE TABLE customer_orders_2024_q1_summary;  -- 32 chars - valid
CREATE TABLE very_long_table_name_that_exceeds_the_maximum_allowed_chars;  -- 65 chars - ERROR
```

#### **First Character Rules**
```sql
-- Valid first characters
First Character: Must be a letter (A-Z, a-z, Unicode) or underscore (_)

-- Valid examples
CREATE TABLE customers;           -- Letter start
CREATE TABLE _temp_data;         -- Underscore start
CREATE TABLE Κελεπόριζα;         -- Unicode letter start

-- Invalid examples
CREATE TABLE 2024_sales;         -- ERROR: starts with digit
CREATE TABLE $config;           -- ERROR: starts with dollar sign
```

---

## Case Sensitivity and Naming Modes

ScratchBird supports multiple case-handling modes for maximum flexibility and compatibility.

### Standard SQL Mode (Default)

#### **Unquoted Identifiers**
```sql
-- Unquoted identifiers are case-insensitive and stored in uppercase
CREATE TABLE Customers;          -- Stored as: CUSTOMERS
CREATE TABLE customers;          -- Stored as: CUSTOMERS  
CREATE TABLE CUSTOMERS;          -- Stored as: CUSTOMERS

-- All references are equivalent
SELECT * FROM Customers;         -- Valid
SELECT * FROM customers;         -- Valid
SELECT * FROM CUSTOMERS;         -- Valid

-- Column names follow same rules
CREATE TABLE orders (
    OrderId INTEGER,             -- Stored as: ORDERID
    customer_Name VARCHAR(100),  -- Stored as: CUSTOMER_NAME
    ORDER_DATE DATE             -- Stored as: ORDER_DATE
);
```

#### **Quoted Identifiers**  
```sql
-- Quoted identifiers preserve exact case and spacing
CREATE TABLE "CustomerOrders";   -- Stored as: CustomerOrders
CREATE TABLE "customer_orders";  -- Stored as: customer_orders
CREATE TABLE "CUSTOMER_ORDERS";  -- Stored as: CUSTOMER_ORDERS

-- Must reference with exact case and quotes
SELECT * FROM "CustomerOrders";  -- Valid
SELECT * FROM CustomerOrders;    -- ERROR: object not found
SELECT * FROM "customer_orders"; -- ERROR: different object

-- Quoted identifiers can contain spaces and special characters
CREATE TABLE "Customer Orders 2024" (
    "Order ID" INTEGER,
    "Customer Name" VARCHAR(100),
    "Order Date" DATE
);
```

### PascalCase Mode (ScratchBird Enhancement)

ScratchBird provides unique support for modern PascalCase naming conventions:

#### **Enabling PascalCase Mode**
```sql
-- Enable PascalCase mode for database
ALTER DATABASE PASCAL CASE IDENTIFIERS;

-- Check current mode
SELECT CURRENT_PASCAL_CASE_MODE;
-- Result: 1 (enabled) or 0 (disabled)

-- Database header flag
SELECT RDB$GET_CONTEXT('SYSTEM', 'PASCAL_CASE_MODE');
```

#### **PascalCase Behavior**
```sql
-- In PascalCase mode, identifiers maintain original case
CREATE TABLE CustomerOrders;     -- Stored as: CustomerOrders
CREATE TABLE ProductCategories;  -- Stored as: ProductCategories

-- Case-insensitive matching in PascalCase mode
SELECT * FROM customerorders;    -- Matches CustomerOrders
SELECT * FROM CustomerOrders;    -- Matches CustomerOrders
SELECT * FROM CUSTOMERORDERS;    -- Matches CustomerOrders

-- Mixed naming styles in PascalCase mode
CREATE TABLE CustomerDetails (
    CustomerId INTEGER,          -- Stored as: CustomerId
    FirstName VARCHAR(50),       -- Stored as: FirstName
    lastName VARCHAR(50),        -- Stored as: lastName
    EMAIL_ADDRESS VARCHAR(100)   -- Stored as: EMAIL_ADDRESS
);
```

#### **PascalCase Best Practices**
```sql
-- Recommended PascalCase naming conventions
CREATE TABLE CustomerOrders (
    OrderId INTEGER PRIMARY KEY,
    CustomerId INTEGER,
    OrderDate DATE,
    TotalAmount DECIMAL(10,2),
    ShippingAddress VARCHAR(255)
);

CREATE PROCEDURE GetCustomerOrders(
    IN CustomerId INTEGER,
    OUT TotalOrders INTEGER
)
AS
BEGIN
    SELECT COUNT(*) FROM CustomerOrders 
    WHERE CustomerId = :CustomerId
    INTO :TotalOrders;
END;

-- Function naming
CREATE FUNCTION CalculateOrderTotal(
    OrderId INTEGER
) RETURNS DECIMAL(10,2)
AS
BEGIN
    RETURN (SELECT TotalAmount FROM CustomerOrders WHERE OrderId = :OrderId);
END;
```

---

## Reserved Words and Keywords

ScratchBird maintains an extensive list of reserved words that cannot be used as unquoted identifiers.

### Core Reserved Words

#### **DDL Keywords**
```sql
-- These cannot be used as unquoted identifiers
CREATE TABLE create;             -- ERROR: CREATE is reserved
CREATE TABLE table;              -- ERROR: TABLE is reserved
CREATE TABLE index;              -- ERROR: INDEX is reserved

-- Must use quotes to use reserved words
CREATE TABLE "create" (id INTEGER);     -- Valid with quotes
CREATE TABLE "table" (id INTEGER);      -- Valid with quotes

-- Common DDL reserved words
CREATE, ALTER, DROP, TABLE, VIEW, INDEX, PROCEDURE, FUNCTION,
TRIGGER, DATABASE, SCHEMA, DOMAIN, SEQUENCE, GENERATOR,
CONSTRAINT, PRIMARY, FOREIGN, UNIQUE, CHECK, DEFAULT
```

#### **DML Keywords**
```sql
-- Data manipulation reserved words
SELECT, INSERT, UPDATE, DELETE, FROM, WHERE, GROUP, HAVING,
ORDER, BY, DISTINCT, ALL, ANY, SOME, EXISTS, IN, LIKE,
BETWEEN, IS, NULL, NOT, AND, OR, UNION, INTERSECT, EXCEPT

-- Cannot use as unquoted identifiers
CREATE TABLE select (id INTEGER);       -- ERROR
CREATE TABLE where (id INTEGER);        -- ERROR
CREATE TABLE "select" (id INTEGER);     -- Valid with quotes
```

#### **Data Type Keywords**
```sql
-- Built-in data type names are reserved
INTEGER, BIGINT, SMALLINT, DECIMAL, NUMERIC, FLOAT, DOUBLE,
REAL, BOOLEAN, CHAR, VARCHAR, BLOB, DATE, TIME, TIMESTAMP

-- Examples
CREATE TABLE data_types (
    integer_val INTEGER,         -- Valid: field name differs from type
    "integer" INTEGER,           -- Valid: quoted identifier
    int_value INTEGER            -- Valid: not exact reserved word
);
```

### Non-Reserved Words

Many function names and system words are non-reserved and can be used as identifiers:

#### **Function Names (Non-Reserved)**
```sql
-- These can be used as unquoted identifiers
CREATE TABLE abs (value INTEGER);           -- Valid: ABS is non-reserved
CREATE TABLE ceil (value INTEGER);          -- Valid: CEIL is non-reserved
CREATE TABLE floor (value INTEGER);         -- Valid: FLOOR is non-reserved
CREATE TABLE round (value INTEGER);         -- Valid: ROUND is non-reserved

-- System names (non-reserved)
CREATE TABLE action (id INTEGER);           -- Valid
CREATE TABLE cascade (id INTEGER);          -- Valid
CREATE TABLE role (id INTEGER);             -- Valid
CREATE TABLE type (id INTEGER);             -- Valid
```

#### **Checking Reserved Status**
```sql
-- Use quotes when uncertain about reserved status
CREATE TABLE "potentially_reserved_word" (id INTEGER);

-- Safe naming pattern to avoid conflicts
CREATE TABLE tbl_user_data;                 -- Prefix pattern
CREATE TABLE customer_info_table;           -- Suffix pattern
CREATE TABLE app_configuration_settings;    -- Descriptive pattern
```

---

## Hierarchical Schema Naming

ScratchBird's hierarchical schema system provides advanced multi-level object qualification.

### Schema Path Structure

#### **Schema Hierarchy Limits**
```sql
-- Schema hierarchy constraints
Maximum Schema Depth: 11 levels
Maximum Path Length: 703 characters
Path Separator: . (dot)
Individual Schema Name: 63 characters maximum

-- Example hierarchy
root.company.division.department.team.project.environment.component.module.subsystem.table
```

#### **Schema Qualified Names**
```sql
-- Basic schema qualification
CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.accounting.reports;

-- Create table in nested schema
CREATE TABLE finance.accounting.reports.monthly_summary (
    report_id INTEGER,
    month_name VARCHAR(20),
    total_revenue DECIMAL(15,2)
);

-- Reference with full qualification
SELECT * FROM finance.accounting.reports.monthly_summary;

-- Schema names follow same identifier rules
CREATE SCHEMA "Finance Department";          -- Quoted schema name
CREATE SCHEMA finance_2024;                  -- Underscore allowed
CREATE SCHEMA Finance2024;                   -- Mixed case in PascalCase mode
```

#### **Multi-Level Qualification**
```sql
-- Parser supports up to 3-level qualified names directly
SELECT finance.accounting.monthly_summary.report_id;    -- 3 levels
SELECT schema1.schema2.table_name.column_name;          -- 3 levels

-- Full hierarchical paths supported in schema context
SET SCHEMA 'finance.accounting.reports';
CREATE TABLE quarterly_summary (                        -- In nested schema
    quarter VARCHAR(10),
    total_amount DECIMAL(15,2)
);
```

### Schema Naming Best Practices

#### **Organizational Hierarchy**
```sql
-- Mirror organizational structure
CREATE SCHEMA company;
CREATE SCHEMA company.americas;
CREATE SCHEMA company.americas.usa;
CREATE SCHEMA company.americas.usa.sales;

CREATE SCHEMA company.europe;
CREATE SCHEMA company.europe.germany;
CREATE SCHEMA company.europe.germany.manufacturing;

-- Application modules
CREATE SCHEMA app;
CREATE SCHEMA app.authentication;
CREATE SCHEMA app.user_management;
CREATE SCHEMA app.reporting;
CREATE SCHEMA app.audit;
```

#### **Environment Separation**
```sql
-- Environment-based schemas
CREATE SCHEMA development;
CREATE SCHEMA development.feature_branches;
CREATE SCHEMA development.integration_tests;

CREATE SCHEMA staging;
CREATE SCHEMA staging.performance_tests;
CREATE SCHEMA staging.user_acceptance;

CREATE SCHEMA production;
CREATE SCHEMA production.live_data;
CREATE SCHEMA production.analytics;
```

---

## Special Characters and Unicode Support

### Unicode Character Support

ScratchBird provides comprehensive Unicode support for international applications:

#### **International Characters**
```sql
-- Greek characters
CREATE TABLE Πελάτες (                      -- Customers in Greek
    κωδικός INTEGER,                         -- Code
    όνομα VARCHAR(100)                       -- Name
);

-- Chinese characters  
CREATE TABLE 客户 (                         -- Customers in Chinese
    客户编号 INTEGER,                        -- Customer ID
    客户姓名 VARCHAR(100)                    -- Customer Name
);

-- Arabic characters
CREATE TABLE العملاء (                      -- Customers in Arabic
    رقم_العميل INTEGER,                     -- Customer Number
    اسم_العميل VARCHAR(100)                 -- Customer Name
);

-- Mixed Unicode and ASCII
CREATE TABLE CustomerDetails_顧客詳細 (
    CustomerId INTEGER,
    CustomerName_顧客名 VARCHAR(100),
    Διεύθυνση VARCHAR(200)                   -- Address in Greek
);
```

#### **Emoji and Special Symbols**
```sql
-- Modern Unicode support
CREATE TABLE ProductRatings (
    ProductId INTEGER,
    Rating VARCHAR(20),
    ReviewEmoji VARCHAR(10)                  -- ⭐🌟💯👍👎
);

-- Mathematical symbols
CREATE TABLE ScientificData (
    DataId INTEGER,
    Formula VARCHAR(100),                    -- Can contain ∀∃∈∉∅∪∩
    Result_α DECIMAL(10,5),                 -- Greek letter alpha
    Result_β DECIMAL(10,5)                  -- Greek letter beta
);
```

### Special Character Rules

#### **Allowed Special Characters**
```sql
-- Valid special characters in identifiers
CREATE TABLE customer_data_;                -- Underscore
CREATE TABLE config$settings;              -- Dollar sign
CREATE TABLE data{temp};                   -- Braces (special case)

-- Combining special characters
CREATE TABLE temp_data$backup_;            -- Multiple special chars
CREATE TABLE _internal$config{system};     -- Complex combination
```

#### **Quote Escaping**
```sql
-- Escaping quotes in quoted identifiers
CREATE TABLE "Customer""s Data";           -- Contains quote: Customer"s Data
CREATE TABLE "Table with ""quotes""";      -- Multiple quotes

-- Complex quoted identifiers
CREATE TABLE "Orders (2024)" (             -- Parentheses allowed
    "Order #" INTEGER,                     -- Hash symbol
    "Customer Name (Full)" VARCHAR(100),   -- Mixed special chars
    "Price in $" DECIMAL(10,2)            -- Dollar in quoted name
);
```

---

## Naming Conventions and Best Practices

### Database-Level Naming Standards

#### **Consistent Naming Schemes**
```sql
-- Table naming conventions
-- Option 1: snake_case (traditional)
CREATE TABLE customer_orders;
CREATE TABLE order_line_items;
CREATE TABLE product_categories;

-- Option 2: PascalCase (modern, with PascalCase mode)
CREATE TABLE CustomerOrders;
CREATE TABLE OrderLineItems;  
CREATE TABLE ProductCategories;

-- Option 3: Prefixed approach
CREATE TABLE tbl_customers;
CREATE TABLE tbl_orders;
CREATE TABLE tbl_products;
```

#### **Column Naming Patterns**
```sql
-- Consistent column naming
CREATE TABLE customers (
    -- Primary key pattern
    customer_id INTEGER PRIMARY KEY,        -- snake_case
    -- OR
    CustomerId INTEGER PRIMARY KEY,         -- PascalCase
    
    -- Foreign key pattern
    company_id INTEGER,                     -- References companies.company_id
    -- OR  
    CompanyId INTEGER,                      -- References Companies.CompanyId
    
    -- Standard columns
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    email_address VARCHAR(100),
    phone_number VARCHAR(20),
    
    -- Audit columns
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP,
    created_by VARCHAR(50),
    updated_by VARCHAR(50)
);
```

#### **Procedure and Function Naming**
```sql
-- Procedure naming conventions
CREATE PROCEDURE get_customer_orders(      -- Verb + object pattern
    customer_id INTEGER
)
AS
BEGIN
    SELECT * FROM customer_orders WHERE customer_id = :customer_id;
END;

-- Function naming conventions  
CREATE FUNCTION calculate_order_total(     -- Verb + object pattern
    order_id INTEGER
) RETURNS DECIMAL(10,2)
AS
BEGIN
    RETURN (SELECT SUM(quantity * unit_price) 
            FROM order_line_items 
            WHERE order_id = :order_id);
END;

-- PascalCase alternatives
CREATE PROCEDURE GetCustomerOrders(
    CustomerId INTEGER
);

CREATE FUNCTION CalculateOrderTotal(
    OrderId INTEGER
) RETURNS DECIMAL(10,2);
```

### Schema Organization Patterns

#### **Functional Schema Organization**
```sql
-- Group by business function
CREATE SCHEMA sales;
CREATE SCHEMA sales.customers;
CREATE SCHEMA sales.orders;
CREATE SCHEMA sales.products;

CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.billing;
CREATE SCHEMA finance.reporting;

CREATE SCHEMA operations;
CREATE SCHEMA operations.inventory;
CREATE SCHEMA operations.shipping;
CREATE SCHEMA operations.procurement;
```

#### **Layered Schema Architecture**
```sql
-- Data layer schemas
CREATE SCHEMA raw_data;                    -- Unprocessed data
CREATE SCHEMA staging;                     -- Data transformation
CREATE SCHEMA processed;                   -- Clean, processed data
CREATE SCHEMA marts;                       -- Business intelligence

-- Application layer schemas
CREATE SCHEMA api;                         -- API-related objects
CREATE SCHEMA app_logic;                   -- Business logic
CREATE SCHEMA app_config;                  -- Configuration
CREATE SCHEMA app_audit;                   -- Audit trails
```

### Anti-Patterns to Avoid

#### **Poor Naming Practices**
```sql
-- Avoid these patterns:

-- 1. Meaningless abbreviations
CREATE TABLE cust;                         -- Use: customers
CREATE TABLE ord;                          -- Use: orders
CREATE TABLE prod;                         -- Use: products

-- 2. Inconsistent naming
CREATE TABLE customers;                    -- snake_case
CREATE TABLE OrderDetails;                -- PascalCase
CREATE TABLE PRODUCT_CATALOG;             -- UPPER_CASE
-- Choose one style and stick to it

-- 3. Reserved word conflicts
CREATE TABLE "order";                     -- Better: orders, customer_orders
CREATE TABLE "user";                      -- Better: users, app_users
CREATE TABLE "table";                     -- Better: data_tables

-- 4. Overly long names
CREATE TABLE customer_order_line_item_shipping_details_and_tracking_info;
-- Better: order_shipping_details

-- 5. Generic names
CREATE TABLE data;                        -- Too generic
CREATE TABLE info;                        -- Too generic  
CREATE TABLE temp;                        -- Unclear purpose
```

#### **Schema Naming Anti-Patterns**
```sql
-- Avoid deep hierarchies without clear purpose
CREATE SCHEMA a.b.c.d.e.f.g.h.i.j.k;     -- Too deep, unclear

-- Avoid inconsistent schema organization
CREATE SCHEMA sales_2024;                 -- Year-based
CREATE SCHEMA marketing;                  -- Function-based
CREATE SCHEMA temp_data;                  -- Purpose-based
-- Choose consistent organization principle
```

---

## Advanced Naming Features

### Dynamic Identifier Construction

#### **Computed Identifiers in Procedures**
```sql
-- Dynamic table/column name construction
CREATE PROCEDURE create_monthly_table(
    month_name VARCHAR(20),
    year_num INTEGER
)
AS
DECLARE VARIABLE table_name VARCHAR(100);
DECLARE VARIABLE sql_stmt VARCHAR(500);
BEGIN
    -- Construct table name
    table_name = 'orders_' || :month_name || '_' || :year_num;
    
    -- Dynamic DDL (requires EXECUTE STATEMENT privileges)
    sql_stmt = 'CREATE TABLE ' || :table_name || 
               ' (order_id INTEGER, order_date DATE, amount DECIMAL(10,2))';
    
    EXECUTE STATEMENT :sql_stmt;
END;

-- Usage
EXECUTE PROCEDURE create_monthly_table('january', 2024);
-- Creates table: orders_january_2024
```

#### **Identifier Validation Functions**
```sql
-- Create validation function for identifiers
CREATE FUNCTION validate_identifier(
    identifier_name VARCHAR(63)
) RETURNS BOOLEAN
AS
BEGIN
    -- Check length
    IF (CHAR_LENGTH(:identifier_name) > 63) THEN
        RETURN FALSE;
    
    -- Check first character is letter or underscore
    IF (LEFT(:identifier_name, 1) NOT SIMILAR TO '[A-Za-z_]') THEN
        RETURN FALSE;
    
    -- Check all characters are valid
    IF (:identifier_name NOT SIMILAR TO '[A-Za-z0-9_${}]*') THEN
        RETURN FALSE;
    
    RETURN TRUE;
END;

-- Usage
SELECT validate_identifier('valid_table_name');    -- TRUE
SELECT validate_identifier('123invalid');          -- FALSE
SELECT validate_identifier('toolong' || REPEAT('x', 60)); -- FALSE
```

### Metadata Introspection

#### **Querying Object Names**
```sql
-- Find objects with specific naming patterns
SELECT RDB$RELATION_NAME 
FROM RDB$RELATIONS 
WHERE RDB$RELATION_NAME LIKE '%CUSTOMER%'
  AND RDB$SYSTEM_FLAG = 0;

-- Find all PascalCase table names
SELECT RDB$RELATION_NAME
FROM RDB$RELATIONS
WHERE RDB$RELATION_NAME SIMILAR TO '[A-Z][a-z]*([A-Z][a-z]*)*'
  AND RDB$SYSTEM_FLAG = 0;

-- Find tables with specific schema
SELECT RDB$RELATION_NAME
FROM RDB$RELATIONS r
JOIN RDB$SCHEMAS s ON r.RDB$SCHEMA_NAME = s.RDB$SCHEMA_NAME
WHERE s.RDB$SCHEMA_NAME = 'FINANCE.ACCOUNTING';
```

#### **Identifier Case Analysis**
```sql
-- Analyze naming convention usage in database
SELECT 
    CASE 
        WHEN RDB$RELATION_NAME SIMILAR TO '[A-Z_0-9]*' THEN 'UPPER_CASE'
        WHEN RDB$RELATION_NAME SIMILAR TO '[a-z_0-9]*' THEN 'lower_case'
        WHEN RDB$RELATION_NAME SIMILAR TO '[A-Z][a-z]*([A-Z][a-z]*)*' THEN 'PascalCase'
        WHEN RDB$RELATION_NAME SIMILAR TO '[a-z]+(_[a-z0-9]+)*' THEN 'snake_case'
        ELSE 'mixed_case'
    END as naming_convention,
    COUNT(*) as table_count
FROM RDB$RELATIONS
WHERE RDB$SYSTEM_FLAG = 0
GROUP BY 1
ORDER BY table_count DESC;
```

---

## Migration and Compatibility Considerations

### Migrating from Other Databases

#### **From Firebird/InterBase**
```sql
-- Firebird identifiers are already compatible
-- Length limit is same (63 characters)
-- Case handling is identical for standard mode

-- Consider enabling PascalCase mode for modern naming
ALTER DATABASE PASCAL CASE IDENTIFIERS;

-- Update naming conventions gradually
CREATE TABLE CustomerOrders AS SELECT * FROM CUSTOMER_ORDERS;
DROP TABLE CUSTOMER_ORDERS;
```

#### **From PostgreSQL**
```sql
-- PostgreSQL uses same 63-character limit
-- Case handling is identical
-- Direct migration possible for most identifiers

-- PostgreSQL quoted identifiers work directly
CREATE TABLE "CaseSensitiveTable" (
    "ColumnName" INTEGER
);

-- Hierarchical schema mapping
-- PostgreSQL: schema.table
-- ScratchBird: schema.table (plus deeper nesting)
```

#### **From SQL Server**
```sql
-- SQL Server uses 128-character limit, ScratchBird uses 63
-- May need to shorten some identifiers

-- SQL Server bracket notation not supported
-- [Table Name] -> "Table Name"

-- Enable PascalCase mode for SQL Server-style naming
ALTER DATABASE PASCAL CASE IDENTIFIERS;
```

#### **From Oracle**
```sql
-- Oracle uses 30-character limit (older) or 128 (newer)
-- ScratchBird 63-character limit is compatible with most Oracle names

-- Oracle's case handling is similar to ScratchBird standard mode
-- Unquoted -> uppercase, quoted -> preserve case

-- Oracle's schema.table.column syntax maps well to ScratchBird
```

### Cross-Platform Naming Strategies

#### **Portable Naming Conventions**
```sql
-- Use lowercase with underscores for maximum portability
CREATE TABLE customer_orders (
    order_id INTEGER,
    customer_id INTEGER,
    order_date DATE
);

-- Avoid database-specific features
-- Instead of: CREATE TABLE orders PARTITION BY...
-- Use standard: CREATE TABLE orders_2024_q1, orders_2024_q2

-- Use standard SQL reserved word alternatives
CREATE TABLE app_users;                   -- Instead of "user"
CREATE TABLE customer_orders;             -- Instead of "order"
CREATE TABLE data_groups;                 -- Instead of "group"
```

---

## Troubleshooting Common Naming Issues

### Identifier Length Errors

#### **Error: Identifier Too Long**
```sql
-- Error example
CREATE TABLE this_is_a_very_long_table_name_that_exceeds_the_maximum_length;
-- Error: identifier is too long (maximum length is 63 characters)

-- Solutions
-- 1. Abbreviate meaningfully
CREATE TABLE very_long_table_name_max_len;

-- 2. Use prefixes/suffixes  
CREATE TABLE long_name_tbl;

-- 3. Restructure naming
CREATE TABLE client_order_summary;        -- Instead of: customer_order_summary_report_data
```

### Reserved Word Conflicts

#### **Error: Syntax Error Near Reserved Word**
```sql
-- Error examples
CREATE TABLE order (id INTEGER);          -- ERROR: ORDER is reserved
CREATE TABLE table (name VARCHAR(50));    -- ERROR: TABLE is reserved

-- Solutions
-- 1. Use quotes
CREATE TABLE "order" (id INTEGER);        -- Valid but not recommended

-- 2. Modify name (preferred)
CREATE TABLE orders (id INTEGER);         -- Better
CREATE TABLE customer_orders (id INTEGER); -- Even better
CREATE TABLE data_tables (name VARCHAR(50)); -- Better than "table"

-- 3. Use prefixes
CREATE TABLE app_order (id INTEGER);
CREATE TABLE tbl_configuration (name VARCHAR(50));
```

### Case Sensitivity Issues

#### **Error: Object Not Found**
```sql
-- Common case sensitivity problems

-- 1. Created with quotes, accessed without
CREATE TABLE "CustomerOrders" (id INTEGER);
SELECT * FROM CustomerOrders;             -- ERROR: not found

-- Solution: Use quotes consistently
SELECT * FROM "CustomerOrders";           -- Correct

-- 2. Mixed case expectations
CREATE TABLE customers (id INTEGER);      -- Stored as CUSTOMERS
SELECT * FROM Customers;                  -- Works (case insensitive)
SELECT * FROM "Customers";                -- ERROR: not found

-- Solution: Understand storage rules
SELECT * FROM customers;                  -- Correct
SELECT * FROM CUSTOMERS;                  -- Also correct
```

### Schema Resolution Problems

#### **Error: Schema Not Found**
```sql
-- Schema path errors
CREATE TABLE finance.accounting.reports.summary (id INTEGER);
-- Error: schema "finance.accounting.reports" does not exist

-- Solution: Create schemas in order
CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.accounting.reports;
-- Then create table
CREATE TABLE finance.accounting.reports.summary (id INTEGER);

-- Check current schema path
SELECT RDB$GET_CONTEXT('SCHEMA', 'CURRENT_PATH');
SELECT RDB$GET_CONTEXT('SCHEMA', 'ACCESSIBLE_SCHEMAS');
```

