# ScratchBird SQL Operators - Complete Reference Documentation

## Overview

**SQL Operators** in ScratchBird provide the fundamental building blocks for data manipulation, comparison, and computation within SQL expressions. ScratchBird implements a comprehensive operator system that includes all standard SQL operators plus significant enhancements for modern data types, PostgreSQL compatibility, and advanced features like array operations, full-text search, and AI/ML vector processing.

### Operator System Features

ScratchBird's operator system provides enterprise-grade capabilities with modern enhancements:

- **Complete SQL Standard Compliance**: All SQL:2016 standard operators
- **PostgreSQL Compatibility**: Extensive PostgreSQL-compatible operator set
- **Enhanced Data Types**: Support for arrays, ranges, vectors, spatial data, and JSON
- **Advanced Text Processing**: Full-text search with ranking and linguistic features
- **AI/ML Integration**: Vector mathematics and similarity operations
- **Spatial Processing**: PostGIS-compatible geometric operations
- **Modern Type System**: Network types, UUIDs, and enhanced temporal types

---

## Arithmetic Operators

Arithmetic operators perform mathematical calculations on numeric values.

### Basic Arithmetic Operations

#### **Addition and Subtraction**
```sql
-- Addition operator (+)
SELECT 10 + 5;                    -- 15
SELECT 3.14 + 2.86;              -- 6.00
SELECT price + tax FROM orders;

-- Subtraction operator (-)  
SELECT 10 - 3;                   -- 7
SELECT end_date - start_date;    -- Date difference
SELECT balance - withdrawal FROM accounts;

-- Unary operators
SELECT +42;                      -- 42 (unary plus)
SELECT -amount;                  -- Negative amount (unary minus)
SELECT +(price * 1.1);          -- Explicit positive
```

#### **Multiplication and Division**
```sql
-- Multiplication operator (*)
SELECT 6 * 7;                    -- 42
SELECT price * quantity FROM order_items;
SELECT radius * radius * 3.14159 as area;

-- Division operator (/)
SELECT 15 / 3;                   -- 5
SELECT total_amount / item_count;
SELECT 22.0 / 7.0;              -- 3.142857... (decimal division)

-- Integer vs decimal division
SELECT 22 / 7;                   -- 3 (integer division)
SELECT 22.0 / 7;                 -- 3.142857 (decimal division)
SELECT CAST(22 AS DECIMAL) / 7;  -- 3.142857 (forced decimal)
```

#### **Modulo Operations**
```sql
-- MOD function for remainder
SELECT MOD(17, 5);               -- 2
SELECT MOD(customer_id, 10);     -- Distribute customers into groups
SELECT MOD(-17, 5);              -- Implementation-specific handling

-- Using MOD for data partitioning
SELECT customer_id, 
       MOD(customer_id, 4) as partition_number
FROM customers
ORDER BY partition_number;

-- MOD with dates for periodic processing
SELECT order_id,
       MOD(EXTRACT(DAY FROM order_date), 7) as day_of_week_num
FROM orders;
```

### Advanced Arithmetic Examples

#### **Complex Calculations**
```sql
-- Compound interest calculation
SELECT principal * POWER((1 + rate / 100), years) as compound_amount
FROM investments;

-- Geometric calculations
SELECT 2 * 3.14159 * radius as circumference,
       3.14159 * radius * radius as area
FROM circles;

-- Financial calculations with proper decimal precision
SELECT CAST(principal AS DECIMAL(15,2)) * 
       CAST(rate AS DECIMAL(5,4)) / 100 as monthly_interest
FROM loans;
```

#### **Operator Precedence in Arithmetic**
```sql
-- Multiplication and division before addition and subtraction
SELECT 2 + 3 * 4;                -- 14 (not 20)
SELECT (2 + 3) * 4;              -- 20 (explicit grouping)

-- Complex precedence examples
SELECT price + tax * rate / 100;  -- price + ((tax * rate) / 100)
SELECT (price + tax) * rate / 100; -- ((price + tax) * rate) / 100

-- Unary operators have highest precedence
SELECT -amount * tax_rate;        -- (-amount) * tax_rate
SELECT -(amount * tax_rate);      -- -(amount * tax_rate)
```

---

## Comparison Operators

Comparison operators compare values and return boolean results (TRUE, FALSE, or NULL).

### Standard Comparison Operations

#### **Equality and Inequality**
```sql
-- Equality operator (=)
SELECT * FROM customers WHERE status = 'ACTIVE';
SELECT * FROM products WHERE price = 99.99;
SELECT * FROM orders WHERE customer_id = 12345;

-- Inequality operators (<>, !=)
SELECT * FROM employees WHERE department <> 'TEMP';
SELECT * FROM products WHERE category != 'DISCONTINUED';

-- NULL handling in equality
SELECT * FROM customers WHERE middle_name = NULL;     -- Always returns no rows
SELECT * FROM customers WHERE middle_name IS NULL;    -- Correct NULL test
```

#### **Relational Comparisons**
```sql
-- Less than (<) and less than or equal (<=)
SELECT * FROM products WHERE price < 100;
SELECT * FROM employees WHERE hire_date <= DATE '2024-01-01';
SELECT * FROM orders WHERE quantity <= stock_level;

-- Greater than (>) and greater than or equal (>=)
SELECT * FROM accounts WHERE balance > 1000;
SELECT * FROM products WHERE rating >= 4.5;
SELECT * FROM sales WHERE sale_date >= CURRENT_DATE - 30;

-- Range comparisons
SELECT * FROM products 
WHERE price >= 50 AND price <= 200;    -- Standard range

SELECT * FROM employees
WHERE salary BETWEEN 50000 AND 100000; -- BETWEEN operator (equivalent)
```

### Advanced Comparison Operations

#### **BETWEEN Operator**
```sql
-- Numeric BETWEEN
SELECT * FROM products WHERE price BETWEEN 50 AND 200;
SELECT * FROM employees WHERE salary BETWEEN 40000 AND 80000;

-- Date BETWEEN
SELECT * FROM orders 
WHERE order_date BETWEEN DATE '2024-01-01' AND DATE '2024-12-31';

-- BETWEEN with expressions
SELECT * FROM sales
WHERE total_amount BETWEEN (average_sale * 0.8) AND (average_sale * 1.2);

-- NOT BETWEEN
SELECT * FROM products WHERE price NOT BETWEEN 100 AND 500;
```

#### **NULL Comparison Operations**
```sql
-- IS NULL and IS NOT NULL
SELECT * FROM customers WHERE phone IS NULL;
SELECT * FROM products WHERE description IS NOT NULL;

-- IS UNKNOWN (three-value logic)
SELECT * FROM test_results WHERE passed IS UNKNOWN;
SELECT * FROM survey_responses WHERE rating IS NOT UNKNOWN;

-- NULL-safe equality comparison patterns
SELECT * FROM customers 
WHERE COALESCE(middle_name, '') = COALESCE('', '');  -- NULL-safe comparison
```

### Comparison with Different Data Types

#### **String Comparisons**
```sql
-- String comparison (lexicographic order)
SELECT * FROM customers WHERE last_name < 'M';
SELECT * FROM products WHERE product_code >= 'PRD-1000';

-- Case-sensitive vs case-insensitive
SELECT * FROM customers WHERE UPPER(last_name) = 'SMITH';
SELECT * FROM products WHERE name COLLATE EN_US_CI = 'widget';

-- String comparison with different collations
SELECT * FROM customers 
WHERE name COLLATE UTF8_UNICODE_CI = 'José' COLLATE UTF8_UNICODE_CI;
```

#### **Date and Time Comparisons**
```sql
-- Date comparisons
SELECT * FROM events WHERE event_date = DATE '2024-12-25';
SELECT * FROM logs WHERE log_timestamp > CURRENT_TIMESTAMP - INTERVAL '1 hour';

-- Time comparisons
SELECT * FROM appointments WHERE start_time BETWEEN TIME '09:00' AND TIME '17:00';

-- Timestamp with timezone comparisons
SELECT * FROM global_events 
WHERE event_time > TIMESTAMP '2024-01-01 00:00:00 UTC';
```

---

## Logical Operators

Logical operators combine boolean expressions using three-value logic (TRUE, FALSE, NULL).

### Basic Logical Operations

#### **AND Operator**
```sql
-- Multiple conditions must be true
SELECT * FROM employees 
WHERE department = 'SALES' AND salary > 50000;

SELECT * FROM products 
WHERE category = 'ELECTRONICS' AND price < 500 AND in_stock = TRUE;

-- AND with NULL handling
SELECT * FROM customers
WHERE status = 'ACTIVE' AND credit_rating IS NOT NULL;

-- Complex AND conditions
SELECT * FROM orders
WHERE order_date >= DATE '2024-01-01' 
  AND customer_id IN (SELECT id FROM premium_customers)
  AND total_amount > 1000;
```

#### **OR Operator**
```sql
-- Any condition can be true
SELECT * FROM customers 
WHERE city = 'New York' OR city = 'Los Angeles';

SELECT * FROM products
WHERE category = 'BOOKS' OR category = 'ELECTRONICS' OR discount > 0.2;

-- OR with parentheses for clarity
SELECT * FROM employees
WHERE (department = 'SALES' OR department = 'MARKETING') 
  AND hire_date > DATE '2023-01-01';

-- OR with NULL values
SELECT * FROM survey_responses
WHERE rating >= 4 OR comment IS NOT NULL;
```

#### **NOT Operator**
```sql
-- Logical negation
SELECT * FROM products WHERE NOT discontinued;
SELECT * FROM customers WHERE NOT (status = 'INACTIVE');

-- NOT with other operators
SELECT * FROM orders WHERE customer_id NOT IN (1, 2, 3);
SELECT * FROM employees WHERE department NOT LIKE 'TEMP%';

-- Double negation
SELECT * FROM products WHERE NOT NOT available;  -- Equivalent to: available
```

### Three-Value Logic

#### **NULL Handling in Logical Operations**
```sql
-- AND truth table with NULL
SELECT 
    TRUE AND TRUE as t_and_t,      -- TRUE
    TRUE AND FALSE as t_and_f,     -- FALSE  
    TRUE AND NULL as t_and_n,      -- NULL
    FALSE AND NULL as f_and_n,     -- FALSE
    NULL AND NULL as n_and_n;      -- NULL

-- OR truth table with NULL
SELECT
    TRUE OR FALSE as t_or_f,       -- TRUE
    FALSE OR FALSE as f_or_f,      -- FALSE
    TRUE OR NULL as t_or_n,        -- TRUE
    FALSE OR NULL as f_or_n,       -- NULL
    NULL OR NULL as n_or_n;        -- NULL

-- NOT truth table with NULL
SELECT
    NOT TRUE as not_t,             -- FALSE
    NOT FALSE as not_f,            -- TRUE
    NOT NULL as not_n;             -- NULL
```

#### **Practical NULL Logic Examples**
```sql
-- Safe boolean expressions with NULL
SELECT customer_id,
       CASE 
           WHEN active IS TRUE THEN 'Active'
           WHEN active IS FALSE THEN 'Inactive' 
           WHEN active IS NULL THEN 'Unknown'
       END as status
FROM customers;

-- NULL-aware filtering
SELECT * FROM products
WHERE (discontinued IS NOT TRUE)  -- Includes NULL and FALSE
  AND (price IS NOT NULL);

-- Complex NULL handling
SELECT * FROM orders
WHERE (shipped_date IS NULL AND order_date < CURRENT_DATE - 7)  -- Overdue
   OR (shipped_date IS NOT NULL AND delivered_date IS NULL);    -- Shipped but not delivered
```

### Logical Operator Precedence

#### **Precedence Rules**
```sql
-- Precedence: NOT > AND > OR
SELECT * FROM products
WHERE NOT discontinued AND price < 100 OR category = 'SALE';
-- Equivalent to: ((NOT discontinued) AND (price < 100)) OR (category = 'SALE')

-- Use parentheses for clarity
SELECT * FROM products  
WHERE NOT (discontinued AND price < 100) OR category = 'SALE';
-- Different meaning: (NOT (discontinued AND price < 100)) OR (category = 'SALE')

-- Complex precedence example
SELECT * FROM employees
WHERE department = 'SALES' OR department = 'MARKETING' AND salary > 60000;
-- Equivalent to: (department = 'SALES') OR ((department = 'MARKETING') AND (salary > 60000))

-- Clearer with explicit parentheses
SELECT * FROM employees
WHERE (department = 'SALES' OR department = 'MARKETING') AND salary > 60000;
```

---

## String Operators

String operators manipulate and compare text data with support for various text processing operations.

### String Concatenation

#### **Concatenation Operator (||)**
```sql
-- Basic string concatenation
SELECT 'Hello' || ' ' || 'World';                    -- 'Hello World'
SELECT first_name || ' ' || last_name as full_name   
FROM employees;

-- Concatenation with NULL handling
SELECT 'Prefix: ' || description;                    -- NULL if description is NULL
SELECT 'Prefix: ' || COALESCE(description, 'N/A');   -- NULL-safe concatenation

-- Multiple string concatenation
SELECT street || ', ' || city || ', ' || state || ' ' || zipcode as full_address
FROM addresses;

-- Concatenation with non-string types
SELECT 'Customer #' || customer_id || ': ' || customer_name
FROM customers;
```

#### **Advanced Concatenation Examples**
```sql
-- Building complex strings
SELECT 'Order ' || order_id || ' for $' || 
       CAST(total_amount AS VARCHAR(20)) || 
       ' placed on ' || CAST(order_date AS VARCHAR(20))
FROM orders;

-- Conditional concatenation
SELECT customer_name ||
       CASE 
           WHEN title IS NOT NULL THEN ' (' || title || ')'
           ELSE ''
       END as display_name
FROM customers;

-- Concatenation with formatting
SELECT TRIM(first_name) || ' ' || 
       UPPER(LEFT(middle_name, 1)) || '. ' || 
       TRIM(last_name) as formatted_name
FROM employees
WHERE middle_name IS NOT NULL;
```

### Pattern Matching Operations

#### **LIKE Operator**
```sql
-- Basic pattern matching
SELECT * FROM customers WHERE last_name LIKE 'Smith%';     -- Starts with 'Smith'
SELECT * FROM products WHERE name LIKE '%widget%';         -- Contains 'widget'
SELECT * FROM employees WHERE phone LIKE '___-___-____';   -- Specific pattern

-- LIKE wildcards
-- % matches any sequence of characters (including empty)
-- _ matches exactly one character

-- Pattern examples
SELECT * FROM customers WHERE name LIKE 'A%';              -- Starts with A
SELECT * FROM customers WHERE name LIKE '%son';            -- Ends with 'son'
SELECT * FROM customers WHERE name LIKE '_mith';           -- _mith (5 chars, ends 'mith')
SELECT * FROM products WHERE sku LIKE 'PRD-____-__';       -- Specific SKU pattern

-- Case sensitivity
SELECT * FROM customers WHERE UPPER(name) LIKE 'JOHN%';    -- Case-insensitive search
```

#### **LIKE with ESCAPE Clause**
```sql
-- Escaping special characters
SELECT * FROM products WHERE description LIKE '%20\% off%' ESCAPE '\';
SELECT * FROM files WHERE filename LIKE '%\_%backup%' ESCAPE '\';

-- Using different escape characters
SELECT * FROM data WHERE value LIKE '%^%percent^_sign%' ESCAPE '^';

-- Searching for literal wildcards
SELECT * FROM logs WHERE message LIKE '%\%complete\%%' ESCAPE '\';
SELECT * FROM patterns WHERE template LIKE '%\_example\_file%' ESCAPE '\';
```

#### **SIMILAR TO Operator (SQL:2003 Regular Expressions)**
```sql
-- Regular expression pattern matching
SELECT * FROM customers WHERE name SIMILAR TO '[A-Z][a-z]+';           -- Capitalized names
SELECT * FROM products WHERE sku SIMILAR TO 'PRD-[0-9]{4}-[A-Z]{2}';   -- Specific format
SELECT * FROM emails WHERE email SIMILAR TO '%@[a-z]+\.[a-z]{2,3}';    -- Email pattern

-- Character classes and quantifiers
SELECT * FROM phone_numbers WHERE phone SIMILAR TO '[0-9]{3}-[0-9]{3}-[0-9]{4}';
SELECT * FROM postal_codes WHERE code SIMILAR TO '[0-9]{5}(-[0-9]{4})?';

-- Complex regex patterns
SELECT * FROM product_codes 
WHERE code SIMILAR TO '(PRD|SVC|KIT)-[0-9]+(-(A|B|C))?';
```

### Substring Operations

#### **CONTAINING Operator**
```sql
-- Case-insensitive substring search
SELECT * FROM products WHERE description CONTAINING 'wireless';
SELECT * FROM customers WHERE notes CONTAINING 'VIP';

-- Multiple CONTAINING conditions
SELECT * FROM articles 
WHERE content CONTAINING 'database' 
  AND content CONTAINING 'performance';

-- CONTAINING vs LIKE
SELECT * FROM products WHERE name LIKE '%Widget%';         -- Case-sensitive
SELECT * FROM products WHERE name CONTAINING 'Widget';     -- Case-insensitive
```

#### **STARTING WITH Operator**
```sql
-- Prefix matching (case-insensitive)
SELECT * FROM customers WHERE last_name STARTING WITH 'Mc';
SELECT * FROM products WHERE sku STARTING WITH 'ELEC';

-- STARTING WITH vs LIKE
SELECT * FROM customers WHERE name LIKE 'John%';           -- Case-sensitive prefix
SELECT * FROM customers WHERE name STARTING WITH 'John';   -- Case-insensitive prefix

-- Multiple prefix conditions
SELECT * FROM employees 
WHERE (last_name STARTING WITH 'Van' OR last_name STARTING WITH 'Von');
```

### Advanced String Operations

#### **Collation-Specific Operations**
```sql
-- Using specific collations
SELECT * FROM customers 
WHERE name = 'José' COLLATE UTF8_UNICODE_CI;

SELECT * FROM products 
WHERE description LIKE '%café%' COLLATE FR_FR;

-- Case-insensitive comparisons
SELECT * FROM users 
WHERE username = 'ADMIN' COLLATE UTF8_GENERAL_CI;

-- Collation in ORDER BY
SELECT name FROM customers 
ORDER BY name COLLATE EN_US;
```

---

## Bitwise Operators

Bitwise operators perform bit-level operations on integer values, useful for flag manipulation and binary data processing.

### Basic Bitwise Operations

#### **Bitwise AND (BIN_AND)**
```sql
-- Bitwise AND operation
SELECT BIN_AND(12, 10);                    -- 8 (binary: 1100 & 1010 = 1000)
SELECT BIN_AND(255, 15);                   -- 15 (binary: 11111111 & 00001111 = 00001111)

-- Flag testing with bitwise AND
SELECT customer_id, permissions,
       BIN_AND(permissions, 1) as can_read,      -- Test bit 0
       BIN_AND(permissions, 2) as can_write,     -- Test bit 1  
       BIN_AND(permissions, 4) as can_delete     -- Test bit 2
FROM user_permissions;

-- Multiple flag testing
SELECT * FROM users 
WHERE BIN_AND(user_flags, 7) = 7;               -- Has flags 1, 2, and 4 set

-- Filtering by specific bits
SELECT * FROM products
WHERE BIN_AND(attributes, 16) <> 0;             -- Has attribute flag 16 set
```

#### **Bitwise OR (BIN_OR)**
```sql
-- Bitwise OR operation
SELECT BIN_OR(8, 4);                       -- 12 (binary: 1000 | 0100 = 1100)
SELECT BIN_OR(5, 3);                       -- 7 (binary: 0101 | 0011 = 0111)

-- Setting flags with bitwise OR
UPDATE user_permissions 
SET permissions = BIN_OR(permissions, 8)    -- Set bit 3 (add new permission)
WHERE user_id = 12345;

-- Combining multiple flags
SELECT customer_id,
       BIN_OR(BIN_OR(read_flag, write_flag), admin_flag) as combined_permissions
FROM permission_matrix;

-- Building flag values
SELECT BIN_OR(BIN_OR(1, 4), 16) as multi_flags;     -- 21 (flags 1, 4, and 16)
```

#### **Bitwise XOR (BIN_XOR)**
```sql
-- Bitwise XOR (exclusive OR)
SELECT BIN_XOR(12, 10);                    -- 6 (binary: 1100 ^ 1010 = 0110)
SELECT BIN_XOR(15, 15);                    -- 0 (same values = 0)

-- Toggle flags with XOR
UPDATE feature_flags 
SET flags = BIN_XOR(flags, 32)             -- Toggle bit 5
WHERE entity_id = 67890;

-- XOR for simple encryption/decryption
SELECT BIN_XOR(BIN_XOR(original_value, encryption_key), encryption_key) as decrypted;

-- Finding differences between flag sets
SELECT user1_flags, user2_flags,
       BIN_XOR(user1_flags, user2_flags) as flag_differences
FROM permission_comparison;
```

#### **Bitwise NOT (BIN_NOT)**
```sql
-- Bitwise NOT (complement)
SELECT BIN_NOT(0);                         -- -1 (all bits flipped)
SELECT BIN_NOT(15);                        -- -16 (depends on integer size)

-- Clearing specific flags (using NOT with AND)
UPDATE user_settings
SET options = BIN_AND(options, BIN_NOT(64))  -- Clear bit 6
WHERE user_id = 54321;

-- Mask creation with NOT
SELECT BIN_NOT(7) as mask;                 -- Creates mask with bits 0,1,2 cleared
```

### Bit Shifting Operations

#### **Left Shift (BIN_SHL)**
```sql
-- Left shift (multiply by powers of 2)
SELECT BIN_SHL(5, 1);                      -- 10 (5 * 2^1)
SELECT BIN_SHL(5, 2);                      -- 20 (5 * 2^2)  
SELECT BIN_SHL(1, 8);                      -- 256 (1 * 2^8)

-- Creating flag values by shifting
SELECT BIN_SHL(1, 0) as flag0,             -- 1
       BIN_SHL(1, 1) as flag1,             -- 2
       BIN_SHL(1, 2) as flag2,             -- 4
       BIN_SHL(1, 3) as flag3;             -- 8

-- Efficient multiplication by powers of 2
SELECT quantity * BIN_SHL(1, 3) as times_eight    -- Multiply by 8
FROM inventory;

-- Building bit masks
SELECT BIN_SHL(1, bit_position) as single_bit_mask
FROM bit_positions;
```

#### **Right Shift (BIN_SHR)**
```sql
-- Right shift (divide by powers of 2)
SELECT BIN_SHR(20, 1);                     -- 10 (20 / 2^1)
SELECT BIN_SHR(20, 2);                     -- 5 (20 / 2^2)
SELECT BIN_SHR(256, 8);                    -- 1 (256 / 2^8)

-- Extracting high-order bits
SELECT BIN_SHR(value, 8) as high_byte     -- Extract upper 8 bits
FROM binary_data;

-- Efficient division by powers of 2
SELECT BIN_SHR(total_amount, 1) as half_amount    -- Divide by 2
FROM financial_data;

-- Bit field extraction
SELECT BIN_AND(BIN_SHR(flags, 4), 15) as middle_nibble  -- Extract bits 4-7
FROM flag_data;
```

### Advanced Bitwise Applications

#### **Flag Management System**
```sql
-- Comprehensive flag management
CREATE TABLE user_permissions (
    user_id INTEGER,
    permissions INTEGER DEFAULT 0
);

-- Define permission constants
-- READ = 1 (bit 0), WRITE = 2 (bit 1), DELETE = 4 (bit 2), ADMIN = 8 (bit 3)

-- Grant permissions (set bits)
UPDATE user_permissions 
SET permissions = BIN_OR(permissions, 3)    -- Grant READ and WRITE (1 + 2)
WHERE user_id = 12345;

-- Revoke permissions (clear bits)
UPDATE user_permissions
SET permissions = BIN_AND(permissions, BIN_NOT(2))  -- Revoke WRITE
WHERE user_id = 12345;

-- Check permissions
SELECT user_id,
       CASE WHEN BIN_AND(permissions, 1) <> 0 THEN 'Y' ELSE 'N' END as can_read,
       CASE WHEN BIN_AND(permissions, 2) <> 0 THEN 'Y' ELSE 'N' END as can_write,
       CASE WHEN BIN_AND(permissions, 4) <> 0 THEN 'Y' ELSE 'N' END as can_delete,
       CASE WHEN BIN_AND(permissions, 8) <> 0 THEN 'Y' ELSE 'N' END as is_admin
FROM user_permissions;
```

#### **Bit-Packed Data Storage**
```sql
-- Store multiple boolean flags in single integer
CREATE TABLE product_attributes (
    product_id INTEGER,
    flags INTEGER  -- Bit-packed attributes
);

-- Flag definitions:
-- Bit 0: Is Active, Bit 1: Is Featured, Bit 2: Is Discounted, 
-- Bit 3: Is New, Bit 4: Is Limited Edition, Bit 5: Is Digital

-- Set multiple attributes
INSERT INTO product_attributes VALUES 
(1001, BIN_OR(BIN_OR(1, 2), 8));  -- Active, Featured, New

-- Query products with specific attributes
SELECT product_id 
FROM product_attributes
WHERE BIN_AND(flags, 3) = 3        -- Both Active AND Featured
  AND BIN_AND(flags, 4) <> 0;      -- AND Discounted

-- Count products by attribute
SELECT 
    COUNT(CASE WHEN BIN_AND(flags, 1) <> 0 THEN 1 END) as active_count,
    COUNT(CASE WHEN BIN_AND(flags, 2) <> 0 THEN 1 END) as featured_count,
    COUNT(CASE WHEN BIN_AND(flags, 4) <> 0 THEN 1 END) as discounted_count
FROM product_attributes;
```

---

## Array Operators (ScratchBird Enhancement)

ScratchBird provides PostgreSQL-compatible array operators for advanced data structure manipulation.

### Array Containment Operations

#### **Contains Operator (@>)**
```sql
-- Array contains element
SELECT ARRAY[1,2,3,4] @> ARRAY[2,3];       -- TRUE
SELECT ARRAY['a','b','c'] @> ARRAY['b'];    -- TRUE
SELECT ARRAY[1,2] @> ARRAY[1,2,3];         -- FALSE

-- Using with table data
SELECT * FROM products 
WHERE tags @> ARRAY['electronics', 'wireless'];    -- Products with both tags

SELECT customer_id, interests
FROM customer_profiles
WHERE interests @> ARRAY['technology'];             -- Customers interested in technology

-- Multi-dimensional array containment
SELECT * FROM matrix_data
WHERE coordinates @> ARRAY[[1,2], [3,4]];         -- Contains coordinate pairs
```

#### **Contained By Operator (<@)**
```sql
-- Array is contained by another array
SELECT ARRAY[2,3] <@ ARRAY[1,2,3,4];       -- TRUE
SELECT ARRAY['x'] <@ ARRAY['x','y','z'];    -- TRUE
SELECT ARRAY[1,5] <@ ARRAY[1,2,3];         -- FALSE

-- Query for subset relationships
SELECT product_id, available_colors
FROM product_variants
WHERE available_colors <@ ARRAY['red', 'blue', 'green', 'yellow'];

-- Find customers whose interests are subset of campaign targets
SELECT customer_id 
FROM customer_profiles cp
JOIN marketing_campaigns mc ON cp.interests <@ mc.target_interests;
```

#### **Overlaps Operator (&&)**
```sql
-- Arrays have common elements
SELECT ARRAY[1,2,3] && ARRAY[3,4,5];       -- TRUE (common element: 3)
SELECT ARRAY['a','b'] && ARRAY['c','d'];    -- FALSE (no common elements)

-- Find products with overlapping categories
SELECT p1.product_id, p2.product_id
FROM products p1, products p2
WHERE p1.categories && p2.categories
  AND p1.product_id < p2.product_id;

-- Marketing campaign overlap analysis
SELECT campaign_id, target_demographics
FROM campaigns
WHERE target_demographics && ARRAY['young_adults', 'professionals'];
```

### Array Manipulation Functions

#### **Array Construction and Modification**
```sql
-- ARRAY_APPEND - Add element to end
SELECT ARRAY_APPEND(ARRAY[1,2,3], 4);             -- [1,2,3,4]
SELECT ARRAY_APPEND(customer_tags, 'VIP') FROM customers;

-- ARRAY_PREPEND - Add element to beginning  
SELECT ARRAY_PREPEND(0, ARRAY[1,2,3]);            -- [0,1,2,3]
SELECT ARRAY_PREPEND('urgent', task_labels) FROM tasks;

-- ARRAY_CAT - Concatenate arrays
SELECT ARRAY_CAT(ARRAY[1,2], ARRAY[3,4]);         -- [1,2,3,4]
SELECT ARRAY_CAT(old_tags, new_tags) as all_tags FROM tag_updates;

-- ARRAY_REMOVE - Remove all instances of element
SELECT ARRAY_REMOVE(ARRAY[1,2,3,2,4], 2);         -- [1,3,4]
SELECT ARRAY_REMOVE(skill_list, 'deprecated_skill') FROM employee_skills;

-- ARRAY_REPLACE - Replace all instances of element
SELECT ARRAY_REPLACE(ARRAY['old','new','old'], 'old', 'updated');  -- ['updated','new','updated']
```

#### **Array Analysis Functions**
```sql
-- ARRAY_LENGTH - Get array length
SELECT ARRAY_LENGTH(ARRAY[1,2,3,4], 1);           -- 4 (first dimension)
SELECT customer_id, ARRAY_LENGTH(purchase_history, 1) as order_count
FROM customer_data;

-- ARRAY_NDIMS - Number of dimensions
SELECT ARRAY_NDIMS(ARRAY[[1,2],[3,4]]);           -- 2
SELECT ARRAY_NDIMS(ARRAY[1,2,3]);                 -- 1

-- CARDINALITY - Total number of elements
SELECT CARDINALITY(ARRAY[1,2,3]);                 -- 3
SELECT CARDINALITY(ARRAY[[1,2],[3,4,5]]);         -- 5

-- ARRAY_UPPER/ARRAY_LOWER - Dimension bounds
SELECT ARRAY_UPPER(ARRAY[1,2,3], 1);              -- 3 (upper bound)
SELECT ARRAY_LOWER(ARRAY[1,2,3], 1);              -- 1 (lower bound)
```

#### **Array Search Functions**
```sql
-- ARRAY_POSITION - Find first position of element
SELECT ARRAY_POSITION(ARRAY['a','b','c','b'], 'b');     -- 2
SELECT ARRAY_POSITION(skill_array, 'Python') as python_skill_position
FROM job_requirements;

-- ARRAY_POSITIONS - Find all positions of element
SELECT ARRAY_POSITIONS(ARRAY[1,2,3,2,4], 2);            -- [2,4]
SELECT ARRAY_POSITIONS(rating_history, 5) as perfect_ratings
FROM product_reviews;

-- Array element access by position
SELECT (ARRAY['first', 'second', 'third'])[2];          -- 'second'
SELECT tags[1] as primary_tag FROM product_catalog;
```

### Array String Conversion

#### **ARRAY_TO_STRING and STRING_TO_ARRAY**
```sql
-- ARRAY_TO_STRING - Convert array to delimited string
SELECT ARRAY_TO_STRING(ARRAY['red','green','blue'], ',');        -- 'red,green,blue'
SELECT ARRAY_TO_STRING(ARRAY['a','b','c'], ' | ');               -- 'a | b | c'

-- With NULL handling
SELECT ARRAY_TO_STRING(ARRAY['a',NULL,'c'], ',', 'MISSING');     -- 'a,MISSING,c'

-- STRING_TO_ARRAY - Convert delimited string to array
SELECT STRING_TO_ARRAY('red,green,blue', ',');                   -- ['red','green','blue']
SELECT STRING_TO_ARRAY('a::b::c', '::');                         -- ['a','b','c']

-- Practical applications
SELECT customer_id,
       STRING_TO_ARRAY(interests_csv, ',') as interests_array
FROM customer_import;

UPDATE products 
SET tag_array = STRING_TO_ARRAY(tag_string, '|')
WHERE tag_string IS NOT NULL;
```

#### **UNNEST Function**
```sql
-- UNNEST - Expand array to rows
SELECT UNNEST(ARRAY[1,2,3,4]);
-- Results:
-- 1
-- 2  
-- 3
-- 4

-- Practical UNNEST usage
SELECT customer_id, UNNEST(purchase_categories) as category
FROM customer_analytics;

-- UNNEST with position
SELECT * FROM UNNEST(ARRAY['a','b','c']) WITH ORDINALITY AS t(element, position);
-- Results:
-- element | position
-- a       | 1
-- b       | 2
-- c       | 3

-- Complex UNNEST operations
SELECT p.product_id, t.tag, t.tag_position
FROM products p,
     UNNEST(p.tags) WITH ORDINALITY AS t(tag, tag_position)
WHERE t.tag LIKE '%premium%';
```

### Advanced Array Examples

#### **Multi-dimensional Arrays**
```sql
-- Creating 2D arrays
SELECT ARRAY[[1,2,3], [4,5,6], [7,8,9]] as matrix;

-- Accessing 2D array elements
SELECT (ARRAY[[1,2],[3,4]])[1][2];                     -- 2

-- Working with coordinate data
CREATE TABLE locations (
    location_id INTEGER,
    coordinates INTEGER[][]  -- Array of [x,y] coordinate pairs
);

INSERT INTO locations VALUES 
(1, ARRAY[[10,20], [30,40], [50,60]]);

-- Query 2D array data
SELECT location_id, 
       ARRAY_LENGTH(coordinates, 1) as point_count,
       coordinates[1][1] as first_x,
       coordinates[1][2] as first_y
FROM locations;
```

#### **Array Aggregation**
```sql
-- ARRAY_AGG - Aggregate values into array
SELECT department,
       ARRAY_AGG(employee_name ORDER BY hire_date) as employees
FROM employees
GROUP BY department;

-- Array aggregation with filtering
SELECT customer_id,
       ARRAY_AGG(product_id) FILTER (WHERE rating >= 4) as liked_products
FROM reviews
GROUP BY customer_id;

-- Complex array aggregation
SELECT category,
       ARRAY_AGG(DISTINCT brand ORDER BY brand) as available_brands,
       ARRAY_AGG(price ORDER BY price DESC) as price_range
FROM products
GROUP BY category;
```

---

## Vector Operators (ScratchBird AI/ML Enhancement)

ScratchBird provides comprehensive vector operations for AI/ML applications, similarity search, and mathematical computations.

### Vector Creation and Basic Operations

#### **Vector Construction**
```sql
-- VECTOR_CREATE - Create vectors from components
SELECT VECTOR_CREATE(1.0, 2.0, 3.0, 4.0) as vec4d;
SELECT VECTOR_CREATE(0.5, -0.3, 0.8) as vec3d;

-- VECTOR_FROM_ARRAY - Create vector from array
SELECT VECTOR_FROM_ARRAY(ARRAY[1.5, 2.7, 3.1, 4.9]) as vector_from_arr;
SELECT VECTOR_FROM_ARRAY(embedding_array) as feature_vector 
FROM ml_features;

-- RANDOM_VECTOR - Generate random vectors
SELECT RANDOM_VECTOR(128) as random_embedding;     -- 128-dimensional random vector
SELECT RANDOM_VECTOR(256, -1.0, 1.0) as normalized_random;  -- Range -1 to 1

-- VECTOR_TO_ARRAY - Convert vector back to array
SELECT VECTOR_TO_ARRAY(feature_vector) as array_representation
FROM embeddings;
```

#### **Vector Properties**
```sql
-- VECTOR_DIMENSIONS - Get vector dimensionality
SELECT VECTOR_DIMENSIONS(VECTOR_CREATE(1,2,3,4));  -- 4
SELECT document_id, VECTOR_DIMENSIONS(text_embedding) as embedding_size
FROM document_embeddings;

-- VECTOR_MAGNITUDE - Calculate vector length/norm
SELECT VECTOR_MAGNITUDE(VECTOR_CREATE(3,4));       -- 5.0 (sqrt(3² + 4²))
SELECT customer_id, VECTOR_MAGNITUDE(preference_vector) as preference_strength
FROM customer_profiles;

-- VECTOR_NORMALIZE - Create unit vector
SELECT VECTOR_NORMALIZE(VECTOR_CREATE(3,4,0));     -- [0.6, 0.8, 0.0]
SELECT product_id, VECTOR_NORMALIZE(feature_vector) as normalized_features
FROM product_features;
```

### Vector Arithmetic Operations

#### **Basic Vector Math**
```sql
-- VECTOR_ADD - Element-wise addition
SELECT VECTOR_ADD(
    VECTOR_CREATE(1,2,3), 
    VECTOR_CREATE(4,5,6)
);  -- [5,7,9]

-- Combining feature vectors
SELECT customer_id,
       VECTOR_ADD(demographic_vector, behavioral_vector) as combined_profile
FROM customer_analytics;

-- VECTOR_SUBTRACT - Element-wise subtraction
SELECT VECTOR_SUBTRACT(
    VECTOR_CREATE(10,8,6),
    VECTOR_CREATE(2,3,1)
);  -- [8,5,5]

-- Computing feature differences
SELECT product1_id, product2_id,
       VECTOR_SUBTRACT(p1.features, p2.features) as feature_diff
FROM products p1, products p2
WHERE p1.category = p2.category AND p1.product_id < p2.product_id;

-- VECTOR_MULTIPLY - Scalar multiplication
SELECT VECTOR_MULTIPLY(VECTOR_CREATE(1,2,3), 2.5);  -- [2.5, 5.0, 7.5]

-- Scaling embeddings
UPDATE document_embeddings 
SET scaled_embedding = VECTOR_MULTIPLY(embedding, confidence_score)
WHERE confidence_score IS NOT NULL;
```

#### **Advanced Vector Operations**
```sql
-- VECTOR_DOT_PRODUCT - Dot product (similarity measure)
SELECT VECTOR_DOT_PRODUCT(
    VECTOR_CREATE(1,2,3),
    VECTOR_CREATE(4,5,6)
);  -- 32 (1*4 + 2*5 + 3*6)

-- Document similarity using dot product
SELECT d1.doc_id, d2.doc_id,
       VECTOR_DOT_PRODUCT(d1.embedding, d2.embedding) as similarity
FROM documents d1, documents d2
WHERE d1.doc_id < d2.doc_id
  AND VECTOR_DOT_PRODUCT(d1.embedding, d2.embedding) > 0.8;

-- VECTOR_DISTANCE - Euclidean distance
SELECT VECTOR_DISTANCE(
    VECTOR_CREATE(0,0,0),
    VECTOR_CREATE(3,4,0)
);  -- 5.0

-- Find nearest neighbors
SELECT target.product_id, candidate.product_id,
       VECTOR_DISTANCE(target.embedding, candidate.embedding) as distance
FROM products target, products candidate
WHERE target.product_id = 12345
  AND candidate.product_id != target.product_id
ORDER BY distance
LIMIT 10;
```

### Vector Similarity Functions

#### **Similarity Measures**
```sql
-- VECTOR_SIMILARITY - Cosine similarity
SELECT VECTOR_SIMILARITY(
    VECTOR_NORMALIZE(VECTOR_CREATE(1,2,3)),
    VECTOR_NORMALIZE(VECTOR_CREATE(2,4,6))
);  -- 1.0 (perfectly similar directions)

-- Content-based recommendations
SELECT c.customer_id, p.product_id,
       VECTOR_SIMILARITY(c.preference_vector, p.feature_vector) as match_score
FROM customers c, products p
WHERE VECTOR_SIMILARITY(c.preference_vector, p.feature_vector) > 0.7
ORDER BY c.customer_id, match_score DESC;

-- Semantic search using similarity
SELECT document_id, title,
       VECTOR_SIMILARITY(
           embedding, 
           :query_embedding
       ) as relevance_score
FROM documents
WHERE VECTOR_SIMILARITY(embedding, :query_embedding) > 0.6
ORDER BY relevance_score DESC
LIMIT 20;
```

#### **Vector Clustering and Analysis**
```sql
-- K-means clustering using vector operations
WITH cluster_centers AS (
    SELECT 1 as cluster_id, VECTOR_CREATE(1,1) as center
    UNION ALL
    SELECT 2 as cluster_id, VECTOR_CREATE(5,5) as center
    UNION ALL  
    SELECT 3 as cluster_id, VECTOR_CREATE(9,1) as center
)
SELECT d.data_id,
       (SELECT cluster_id 
        FROM cluster_centers c
        ORDER BY VECTOR_DISTANCE(d.point_vector, c.center)
        LIMIT 1) as assigned_cluster
FROM data_points d;

-- Vector variance and statistical analysis
SELECT category,
       VECTOR_ADD(AVG(feature_vector)) as mean_vector,
       AVG(VECTOR_MAGNITUDE(feature_vector)) as avg_magnitude
FROM product_features
GROUP BY category;
```

### AI/ML Integration Examples

#### **Embedding-Based Search**
```sql
-- Semantic product search
CREATE TABLE product_embeddings (
    product_id INTEGER,
    name_embedding VECTOR(384),     -- Text embedding
    image_embedding VECTOR(512),    -- Image embedding
    combined_embedding VECTOR(896)  -- Concatenated features
);

-- Multi-modal similarity search
SELECT p.product_id, p.name,
       (VECTOR_SIMILARITY(pe.name_embedding, :text_query_embedding) * 0.6 +
        VECTOR_SIMILARITY(pe.image_embedding, :image_query_embedding) * 0.4) as combined_score
FROM products p
JOIN product_embeddings pe ON p.product_id = pe.product_id
WHERE (VECTOR_SIMILARITY(pe.name_embedding, :text_query_embedding) * 0.6 +
       VECTOR_SIMILARITY(pe.image_embedding, :image_query_embedding) * 0.4) > 0.5
ORDER BY combined_score DESC
LIMIT 50;
```

#### **Recommendation Systems**
```sql
-- Collaborative filtering with vector operations
WITH user_similarities AS (
    SELECT u1.user_id as user1, u2.user_id as user2,
           VECTOR_SIMILARITY(u1.preference_vector, u2.preference_vector) as similarity
    FROM user_profiles u1, user_profiles u2
    WHERE u1.user_id < u2.user_id
      AND VECTOR_SIMILARITY(u1.preference_vector, u2.preference_vector) > 0.7
)
SELECT target_user.user_id,
       p.product_id,
       AVG(r.rating * us.similarity) / AVG(us.similarity) as predicted_rating
FROM user_profiles target_user,
     user_similarities us,
     ratings r,
     products p
WHERE (target_user.user_id = us.user1 OR target_user.user_id = us.user2)
  AND (r.user_id = us.user1 OR r.user_id = us.user2)
  AND r.user_id != target_user.user_id
  AND r.product_id = p.product_id
  AND target_user.user_id = :target_user_id
GROUP BY target_user.user_id, p.product_id
HAVING AVG(r.rating * us.similarity) / AVG(us.similarity) >= 4.0
ORDER BY predicted_rating DESC
LIMIT 20;
```

#### **Anomaly Detection with Vectors**
```sql
-- Detect anomalies using vector distance from normal patterns
WITH normal_pattern AS (
    SELECT AVG(behavioral_vector) as avg_behavior
    FROM user_sessions
    WHERE is_normal = TRUE
      AND session_date >= CURRENT_DATE - 30
)
SELECT s.session_id, s.user_id,
       VECTOR_DISTANCE(s.behavioral_vector, np.avg_behavior) as anomaly_score
FROM user_sessions s, normal_pattern np
WHERE s.session_date >= CURRENT_DATE - 1
  AND VECTOR_DISTANCE(s.behavioral_vector, np.avg_behavior) > 3.0  -- Threshold
ORDER BY anomaly_score DESC;
```

---

## Spatial Operators (PostGIS Compatible)

ScratchBird provides comprehensive spatial operations compatible with PostGIS for geographic and geometric data processing.

### Spatial Relationships

#### **Containment Operations**
```sql
-- ST_CONTAINS - Test if geometry A contains geometry B
SELECT ST_CONTAINS(
    ST_GEOMFROMTEXT('POLYGON((0 0, 0 10, 10 10, 10 0, 0 0))'),
    ST_GEOMFROMTEXT('POINT(5 5)')
);  -- TRUE

-- Find stores within delivery zones
SELECT s.store_id, s.name
FROM stores s, delivery_zones dz
WHERE ST_CONTAINS(dz.zone_geometry, s.location)
  AND dz.zone_name = 'Downtown';

-- ST_WITHIN - Test if geometry A is within geometry B
SELECT property_id, address
FROM properties
WHERE ST_WITHIN(property_location, 
    (SELECT boundary FROM city_limits WHERE city = 'Springfield'));

-- ST_INTERSECTS - Test if geometries intersect
SELECT r1.route_id, r2.route_id
FROM delivery_routes r1, delivery_routes r2
WHERE r1.route_id < r2.route_id
  AND ST_INTERSECTS(r1.route_path, r2.route_path);
```

#### **Topological Operations**
```sql
-- ST_TOUCHES - Test if geometries touch at boundary
SELECT p1.parcel_id, p2.parcel_id
FROM land_parcels p1, land_parcels p2
WHERE p1.parcel_id < p2.parcel_id
  AND ST_TOUCHES(p1.boundary, p2.boundary);

-- ST_CROSSES - Test if geometries cross
SELECT road_id, river_id
FROM roads r, rivers rv
WHERE ST_CROSSES(r.road_geometry, rv.river_geometry);

-- ST_OVERLAPS - Test if geometries overlap (partial intersection)
SELECT zone1.zone_id, zone2.zone_id,
       ST_AREA(ST_INTERSECTION(zone1.geometry, zone2.geometry)) as overlap_area
FROM management_zones zone1, management_zones zone2
WHERE zone1.zone_id < zone2.zone_id
  AND ST_OVERLAPS(zone1.geometry, zone2.geometry);

-- ST_DISJOINT - Test if geometries don't intersect
SELECT COUNT(*) as isolated_properties
FROM properties p
WHERE ST_DISJOINT(p.property_boundary, 
    (SELECT ST_UNION(road_geometry) FROM major_roads));
```

### Distance and Measurement Operations

#### **Distance Functions**
```sql
-- ST_DISTANCE - Calculate distance between geometries
SELECT customer_id, store_id,
       ST_DISTANCE(customer_location, store_location) as distance_meters
FROM customers c, stores s
ORDER BY customer_id, distance_meters;

-- ST_DWITHIN - Test if geometries are within specified distance
SELECT h.hospital_id, s.school_id
FROM hospitals h, schools s
WHERE ST_DWITHIN(h.location, s.location, 1000);  -- Within 1km

-- Find nearest facilities
SELECT customer_id,
       (SELECT store_id 
        FROM stores s
        ORDER BY ST_DISTANCE(c.location, s.location)
        LIMIT 1) as nearest_store
FROM customers c;

-- Distance-based service areas
SELECT service_id, location,
       COUNT(*) as customers_within_range
FROM services s, customers c
WHERE ST_DWITHIN(s.location, c.location, s.service_radius)
GROUP BY service_id, location;
```

#### **Area and Length Calculations**
```sql
-- ST_AREA - Calculate area of polygons
SELECT zone_id, zone_name,
       ST_AREA(zone_boundary) as area_square_meters,
       ST_AREA(zone_boundary) / 10000 as area_hectares
FROM management_zones;

-- ST_LENGTH - Calculate length of linestrings
SELECT route_id,
       ST_LENGTH(route_geometry) as route_length_meters,
       ST_LENGTH(route_geometry) / 1000 as route_length_km
FROM delivery_routes;

-- Perimeter calculations
SELECT property_id,
       ST_AREA(property_boundary) as area,
       ST_LENGTH(ST_BOUNDARY(property_boundary)) as perimeter
FROM properties;

-- Complex area calculations
SELECT district_id,
       ST_AREA(district_boundary) as total_area,
       ST_AREA(ST_INTERSECTION(district_boundary, water_bodies.geometry)) as water_area,
       (ST_AREA(district_boundary) - 
        ST_AREA(ST_INTERSECTION(district_boundary, water_bodies.geometry))) as land_area
FROM districts d, water_bodies w;
```

### Geometric Construction Operations

#### **Buffer and Envelope Operations**
```sql
-- ST_BUFFER - Create buffer around geometry
SELECT facility_id,
       ST_BUFFER(facility_location, 500) as service_area_500m,
       ST_BUFFER(facility_location, 1000) as extended_area_1km
FROM public_facilities;

-- Variable buffer distances
SELECT store_id,
       ST_BUFFER(store_location, 
         CASE store_type 
           WHEN 'supermarket' THEN 2000
           WHEN 'convenience' THEN 500  
           WHEN 'pharmacy' THEN 1000
         END) as catchment_area
FROM stores;

-- ST_ENVELOPE - Get bounding box
SELECT region_id,
       ST_ENVELOPE(region_boundary) as bounding_box,
       ST_AREA(ST_ENVELOPE(region_boundary)) as bbox_area
FROM geographic_regions;

-- ST_CENTROID - Calculate geometric center
SELECT property_id,
       ST_CENTROID(property_boundary) as center_point,
       ST_DISTANCE(property_entrance, ST_CENTROID(property_boundary)) as entrance_offset
FROM properties;
```

#### **Set Operations on Geometries**
```sql
-- ST_UNION - Combine geometries
SELECT district_id,
       ST_UNION(ST_COLLECT(parcel_geometry)) as district_boundary
FROM land_parcels
GROUP BY district_id;

-- ST_INTERSECTION - Find intersection of geometries
SELECT p1.parcel_id, p2.parcel_id,
       ST_INTERSECTION(p1.geometry, p2.geometry) as overlap_area
FROM parcels p1, parcels p2
WHERE p1.parcel_id < p2.parcel_id
  AND ST_INTERSECTS(p1.geometry, p2.geometry);

-- ST_DIFFERENCE - Subtract one geometry from another
SELECT property_id,
       ST_DIFFERENCE(property_boundary, building_footprints) as open_space
FROM properties p
JOIN (SELECT property_id, ST_UNION(building_geometry) as building_footprints
      FROM buildings GROUP BY property_id) b 
  ON p.property_id = b.property_id;

-- Complex geometric operations
SELECT watershed_id,
       ST_AREA(watershed_boundary) as total_area,
       ST_AREA(ST_INTERSECTION(watershed_boundary, urban_areas)) as urban_area,
       ST_AREA(ST_INTERSECTION(watershed_boundary, forest_cover)) as forest_area
FROM watersheds w, urban_development u, forest_inventory f;
```

### Geometry Input/Output Operations

#### **Format Conversion Functions**
```sql
-- ST_GEOMFROMTEXT - Create geometry from WKT
SELECT ST_GEOMFROMTEXT('POINT(-122.4194 37.7749)', 4326) as san_francisco;
SELECT ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 1 0, 0 0))') as unit_square;

-- ST_ASTEXT - Convert geometry to WKT
SELECT location_id, ST_ASTEXT(geometry) as wkt_representation
FROM geographic_features;

-- ST_GEOMFROMWKB - Create geometry from WKB
SELECT feature_id, ST_GEOMFROMWKB(binary_geometry) as geometry
FROM imported_features;

-- ST_ASBINARY - Convert geometry to WKB
SELECT feature_id, ST_ASBINARY(geometry) as wkb_data
FROM features_for_export;

-- GeoJSON conversion (ScratchBird extension)
SELECT property_id, ST_ASGEOJSON(property_boundary) as geojson
FROM properties
WHERE property_type = 'commercial';
```

### Advanced Spatial Analysis

#### **Spatial Clustering and Analysis**
```sql
-- Density-based clustering of points
WITH clustered_points AS (
    SELECT point_id, location,
           ST_CLUSTERDBSCAN(location, 100, 5) OVER() as cluster_id
    FROM point_features
)
SELECT cluster_id, 
       COUNT(*) as point_count,
       ST_CENTROID(ST_COLLECT(location)) as cluster_center
FROM clustered_points
WHERE cluster_id IS NOT NULL
GROUP BY cluster_id;

-- Voronoi diagrams for service area analysis
SELECT service_id,
       ST_VORONOIPOLYGONS(ST_COLLECT(service_location)) as voronoi_cells
FROM services
GROUP BY region_id;

-- Convex hull analysis
SELECT region_id,
       ST_CONVEXHULL(ST_COLLECT(facility_location)) as coverage_envelope,
       ST_AREA(ST_CONVEXHULL(ST_COLLECT(facility_location))) as coverage_area
FROM facilities
GROUP BY region_id;
```

#### **Routing and Network Analysis**
```sql
-- Shortest path using geometry (simplified example)
SELECT route_id,
       ST_MAKELINE(ARRAY[start_point, end_point]) as direct_line,
       ST_LENGTH(ST_MAKELINE(ARRAY[start_point, end_point])) as straight_distance
FROM route_requests;

-- Service accessibility analysis
SELECT facility_id,
       COUNT(*) as accessible_population,
       AVG(ST_DISTANCE(f.location, p.location)) as avg_distance
FROM facilities f, population_centers p
WHERE ST_DWITHIN(f.location, p.location, f.service_radius)
GROUP BY facility_id;

-- Geometric network connectivity
SELECT n1.node_id, n2.node_id,
       ST_DISTANCE(n1.location, n2.location) as edge_length
FROM network_nodes n1, network_nodes n2
WHERE n1.node_id < n2.node_id
  AND ST_DWITHIN(n1.location, n2.location, :max_connection_distance)
ORDER BY edge_length;
```

---

## Operator Precedence and Evaluation Order

Understanding operator precedence is crucial for writing correct SQL expressions without excessive parentheses.

### Precedence Hierarchy (Highest to Lowest)

#### **1. Unary Operators (Highest Precedence)**
```sql
-- Unary plus and minus
SELECT -amount * tax_rate;        -- (-amount) * tax_rate
SELECT +price / 100;              -- (+price) / 100

-- Unary NOT
SELECT NOT active AND verified;   -- (NOT active) AND verified
SELECT NOT (active AND verified); -- Different meaning
```

#### **2. Multiplicative Operators**
```sql
-- Multiplication, division have same precedence (left-to-right)
SELECT 12 / 3 * 2;               -- (12 / 3) * 2 = 8
SELECT 12 * 3 / 2;               -- (12 * 3) / 2 = 18

-- Modulo has same precedence as multiplication/division
SELECT 15 + 4 * 2 - MOD(10, 3);  -- 15 + (4 * 2) - (MOD(10, 3)) = 22
```

#### **3. Additive Operators**
```sql
-- Addition and subtraction (left-to-right)
SELECT 10 - 3 + 2;               -- (10 - 3) + 2 = 9
SELECT 10 + 3 - 2;               -- (10 + 3) - 2 = 11

-- String concatenation has same precedence
SELECT 'Hello' || ' ' || 'World' || '!';  -- Left-to-right concatenation
```

#### **4. Comparison Operators**
```sql
-- All comparison operators have same precedence
SELECT price > 100 AND quantity < 10;     -- (price > 100) AND (quantity < 10)
SELECT amount BETWEEN 50 AND 200 OR status = 'VIP';  -- (amount BETWEEN 50 AND 200) OR (status = 'VIP')

-- Special operators
SELECT name LIKE 'A%' AND age >= 18;      -- (name LIKE 'A%') AND (age >= 18)
SELECT value IN (1,2,3) OR value IS NULL; -- (value IN (1,2,3)) OR (value IS NULL)
```

#### **5. Logical NOT**
```sql
-- NOT has higher precedence than AND/OR
SELECT NOT active AND verified;   -- (NOT active) AND verified
SELECT NOT active OR verified;    -- (NOT active) OR verified
SELECT NOT (active OR verified);  -- Different: NOT (active OR verified)
```

#### **6. Logical AND**
```sql
-- AND has higher precedence than OR
SELECT condition1 OR condition2 AND condition3;  -- condition1 OR (condition2 AND condition3)
SELECT (condition1 OR condition2) AND condition3; -- Different grouping
```

#### **7. Logical OR (Lowest Precedence)**
```sql
-- OR has lowest precedence among logical operators
SELECT a = 1 OR b = 2 AND c = 3;  -- a = 1 OR (b = 2 AND c = 3)
SELECT (a = 1 OR b = 2) AND c = 3; -- Different meaning
```

### Complex Precedence Examples

#### **Mixed Arithmetic and Logical**
```sql
-- Complex expression breakdown
SELECT price + tax * rate / 100 > budget AND quantity <= stock
-- Evaluation order:
-- 1. tax * rate (multiplication)
-- 2. (result) / 100 (division)
-- 3. price + (result) (addition)
-- 4. (result) > budget (comparison)
-- 5. quantity <= stock (comparison)
-- 6. (result1) AND (result2) (logical AND)

-- Clear version with parentheses
SELECT (price + ((tax * rate) / 100)) > budget AND quantity <= stock;
```

#### **String Operations with Logic**
```sql
-- String concatenation and comparison
SELECT first_name || ' ' || last_name LIKE '%Smith%' AND active = TRUE
-- Evaluation order:
-- 1. first_name || ' ' || last_name (concatenation, left-to-right)
-- 2. (result) LIKE '%Smith%' (comparison)
-- 3. active = TRUE (comparison)
-- 4. (result1) AND (result2) (logical AND)

-- Explicit grouping for clarity
SELECT (first_name || ' ' || last_name) LIKE '%Smith%' AND active = TRUE;
```

#### **Bitwise and Arithmetic Precedence**
```sql
-- Bitwise operations in context
SELECT BIN_AND(flags, 7) = 7 AND status <> 'DISABLED'
-- Evaluation order:
-- 1. BIN_AND(flags, 7) (function call)
-- 2. (result) = 7 (comparison)
-- 3. status <> 'DISABLED' (comparison)
-- 4. (result1) AND (result2) (logical AND)
```

### Best Practices for Operator Usage

#### **Use Parentheses for Clarity**
```sql
-- Unclear due to precedence rules
SELECT salary + bonus * tax_rate < budget AND department = 'SALES' OR manager_approved = TRUE;

-- Clear with explicit grouping
SELECT ((salary + (bonus * tax_rate)) < budget AND department = 'SALES') OR manager_approved = TRUE;

-- Even better: break into readable components
SELECT 
    (salary + (bonus * tax_rate)) as total_compensation,
    (total_compensation < budget AND department = 'SALES') as needs_approval,
    (needs_approval OR manager_approved = TRUE) as can_proceed;
```

#### **Consistent Style Guidelines**
```sql
-- Good: Consistent spacing and grouping
SELECT (price * quantity) + shipping_cost as total,
       (customer_type = 'PREMIUM') AND (order_amount > 1000) as is_vip_order
FROM orders;

-- Avoid: Unclear operator mixing
SELECT price*quantity+shipping OR customer='VIP'AND amount>1000;

-- Better: Clear logical structure
SELECT 
    (price * quantity + shipping_cost) as total_cost,
    (customer_type = 'VIP' AND amount > 1000) as qualifies_for_discount
FROM orders;
```

