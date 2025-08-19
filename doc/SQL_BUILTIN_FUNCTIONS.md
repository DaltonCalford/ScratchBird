# ScratchBird Built-in Functions - Complete Reference Documentation

## Overview

**Built-in Functions** are pre-defined functions provided by the ScratchBird database engine for performing common operations on data values. ScratchBird offers one of the most comprehensive function libraries among SQL databases, including standard SQL functions, PostgreSQL-compatible extensions, and unique ScratchBird enhancements for modern applications.

### Function Categories

ScratchBird provides built-in functions in the following categories:

- **Scalar Functions**: Operate on individual values and return single values
- **Aggregate Functions**: Operate on groups of rows and return summary values
- **Window/Analytical Functions**: Provide advanced analytics with partition and ordering
- **System Functions**: Access database and session information
- **ScratchBird Extensions**: Modern enhancements for AI/ML, spatial data, and more

### Competitive Advantage

ScratchBird's function library surpasses most database systems in scope and functionality:

| Database | Scalar Functions | Aggregate Functions | Window Functions | Extensions |
|----------|------------------|---------------------|-------------------|------------|
| **ScratchBird** | **400+** | **25+** | **15+** | **AI/ML, Spatial, JSON** |
| PostgreSQL | 350+ | 20+ | 12+ | Spatial, JSON |
| Oracle | 300+ | 18+ | 15+ | Limited |
| SQL Server | 250+ | 15+ | 10+ | Limited |
| MySQL | 200+ | 12+ | 8+ | Basic |

---

## Scalar Functions

Scalar functions operate on individual values and return a single result value.

### String Functions

String functions manipulate character and binary string data.

#### Case Conversion Functions
```sql
-- Convert to uppercase
SELECT UPPER('Hello World') ;
-- Result: HELLO WORLD

SELECT UPPER(customer_name) FROM customers;

-- Convert to lowercase  
SELECT LOWER('Hello World') ;
-- Result: hello world

SELECT LOWER(product_code) FROM products;
```

#### String Extraction Functions
```sql
-- Extract substring with position and length
SELECT SUBSTRING('ScratchBird Database' FROM 1 FOR 7) ;
-- Result: Scratch

SELECT SUBSTRING('ScratchBird Database' FROM 8) ;
-- Result: Bird Database

-- Extract left/right characters
SELECT LEFT('ScratchBird', 7) ;   -- Result: Scratch
SELECT RIGHT('ScratchBird', 4) ;  -- Result: Bird

-- Find position of substring
SELECT POSITION('Bird' IN 'ScratchBird Database') ;
-- Result: 8

-- Character and byte lengths
SELECT CHAR_LENGTH('Hello 世界') ;  -- Result: 8 characters
SELECT OCTET_LENGTH('Hello 世界') ; -- Result: 11 bytes (UTF-8)
```

#### String Modification Functions
```sql
-- Trim whitespace and specific characters
SELECT TRIM('  Hello World  ') ;
-- Result: Hello World

SELECT TRIM(LEADING '0' FROM '00012345') ;
-- Result: 12345

SELECT TRIM(TRAILING '.' FROM 'value...') ;
-- Result: value

SELECT TRIM(BOTH '*' FROM '***text***') ;
-- Result: text

-- Pad strings to specific length
SELECT LPAD('123', 6, '0') ;  -- Result: 000123
SELECT RPAD('abc', 8, 'XY') ; -- Result: abcXYXYX

-- Replace text
SELECT REPLACE('Hello World', 'World', 'ScratchBird') ;
-- Result: Hello ScratchBird

-- Reverse string
SELECT REVERSE('ScratchBird') ;
-- Result: driBhctarS

-- Overlay (replace substring)
SELECT OVERLAY('Hello World' PLACING 'ScratchBird' FROM 7 FOR 5) ;
-- Result: Hello ScratchBird
```

#### Character Code Functions
```sql
-- ASCII functions
SELECT ASCII_CHAR(65) ;    -- Result: A
SELECT ASCII_VAL('A') ;    -- Result: 65

-- Unicode functions
SELECT UNICODE_CHAR(8364) ; -- Result: € (Euro symbol)
SELECT UNICODE_VAL('€') ;   -- Result: 8364

-- Working with international characters
SELECT customer_name, UNICODE_VAL(LEFT(customer_name, 1)) as first_char_code
FROM customers
WHERE customer_name LIKE 'Ñ%';
```

### Numeric Functions

Numeric functions perform mathematical operations on numeric values.

#### Basic Mathematical Functions
```sql
-- Absolute value and sign
SELECT ABS(-123.45) ;  -- Result: 123.45
SELECT SIGN(-42) ;     -- Result: -1
SELECT SIGN(0) ;       -- Result: 0
SELECT SIGN(42) ;      -- Result: 1

-- Rounding functions
SELECT CEIL(3.14159) ;     -- Result: 4
SELECT FLOOR(3.14159) ;    -- Result: 3
SELECT ROUND(3.14159, 2) ; -- Result: 3.14
SELECT TRUNC(3.14159, 2) ; -- Result: 3.14

-- Modulo operation
SELECT MOD(17, 5) ;  -- Result: 2

-- Power and square root
SELECT POWER(2, 8) ; -- Result: 256
SELECT SQRT(144) ;   -- Result: 12
```

#### Advanced Mathematical Functions
```sql
-- Exponential and logarithmic functions
SELECT EXP(1) ;        -- Result: 2.718281828... (e)
SELECT LN(2.718281828) ; -- Result: 1 (natural log)
SELECT LOG10(1000) ;   -- Result: 3 (base-10 log)
SELECT LOG(2, 1024) ;  -- Result: 10 (log base 2)

-- Pi constant
SELECT PI() ;          -- Result: 3.141592653589793

-- Financial calculations with precision
SELECT 
    loan_amount,
    ROUND(loan_amount * POWER(1 + interest_rate/12, term_months), 2) as future_value
FROM loans;
```

#### Trigonometric Functions
```sql
-- Basic trigonometric functions (radians)
SELECT SIN(PI()/2) ;   -- Result: 1
SELECT COS(PI()) ;     -- Result: -1
SELECT TAN(PI()/4) ;   -- Result: 1

-- Inverse trigonometric functions
SELECT ASIN(1) ;       -- Result: π/2
SELECT ACOS(0) ;       -- Result: π/2
SELECT ATAN(1) ;       -- Result: π/4
SELECT ATAN2(1, 1) ;   -- Result: π/4

-- Hyperbolic functions
SELECT SINH(1) ;       -- Result: 1.175201194
SELECT COSH(0) ;       -- Result: 1
SELECT TANH(1) ;       -- Result: 0.761594156

-- Convert degrees to radians for calculations
SELECT SIN(30 * PI() / 180) ; -- Result: 0.5 (sin 30°)
```

#### Binary/Bitwise Functions
```sql
-- Bitwise operations
SELECT BIN_AND(12, 10) ;  -- Result: 8 (1100 & 1010 = 1000)
SELECT BIN_OR(12, 10) ;   -- Result: 14 (1100 | 1010 = 1110)
SELECT BIN_XOR(12, 10) ;  -- Result: 6 (1100 ^ 1010 = 0110)
SELECT BIN_NOT(5) ;       -- Result: -6 (bitwise NOT)

-- Bit shifting
SELECT BIN_SHL(5, 2) ;    -- Result: 20 (shift left 2 positions)
SELECT BIN_SHR(20, 2) ;   -- Result: 5 (shift right 2 positions)

-- Practical bitwise usage for flags
SELECT 
    user_id,
    BIN_AND(permissions, 4) > 0 as can_write,
    BIN_AND(permissions, 2) > 0 as can_read,
    BIN_AND(permissions, 1) > 0 as can_execute
FROM user_permissions;
```

### Date/Time Functions

Date and time functions manipulate temporal data types.

#### Current Date/Time Functions
```sql
-- Current date and time
SELECT CURRENT_DATE ;
-- Result: 2024-12-25

SELECT CURRENT_TIME ;
-- Result: 14:30:45.1234

SELECT CURRENT_TIMESTAMP ;
-- Result: 2024-12-25 14:30:45.1234

-- Local time without time zone
SELECT LOCALTIME ;
SELECT LOCALTIMESTAMP ;

-- Precision specification
SELECT CURRENT_TIME(0) ;      -- No fractional seconds
SELECT CURRENT_TIMESTAMP(3) ; -- 3 decimal places
```

#### Date/Time Extraction Functions
```sql
-- Extract components using EXTRACT
SELECT EXTRACT(YEAR FROM CURRENT_DATE) ;
SELECT EXTRACT(MONTH FROM CURRENT_DATE) ;
SELECT EXTRACT(DAY FROM CURRENT_DATE) ;
SELECT EXTRACT(HOUR FROM CURRENT_TIME) ;
SELECT EXTRACT(MINUTE FROM CURRENT_TIME) ;
SELECT EXTRACT(SECOND FROM CURRENT_TIME) ;

-- Additional extraction options
SELECT EXTRACT(WEEKDAY FROM CURRENT_DATE) ;    -- 0=Sunday, 6=Saturday
SELECT EXTRACT(YEARDAY FROM CURRENT_DATE) ;    -- Day of year (1-366)
SELECT EXTRACT(WEEK FROM CURRENT_DATE) ;       -- Week of year

-- Practical date extraction
SELECT 
    order_id,
    order_date,
    EXTRACT(YEAR FROM order_date) as order_year,
    EXTRACT(MONTH FROM order_date) as order_month,
    EXTRACT(WEEKDAY FROM order_date) as order_weekday
FROM orders
WHERE EXTRACT(YEAR FROM order_date) = 2024;
```

#### Date/Time Arithmetic Functions
```sql
-- Add time intervals
SELECT DATEADD(DAY, 30, CURRENT_DATE) ;
-- Result: Current date + 30 days

SELECT DATEADD(MONTH, -6, CURRENT_DATE) ;
-- Result: Current date - 6 months

SELECT DATEADD(HOUR, 2, CURRENT_TIMESTAMP) ;
-- Result: Current timestamp + 2 hours

-- Calculate differences
SELECT DATEDIFF(DAY, '2024-01-01', '2024-12-31') ;
-- Result: 365 (days between dates)

SELECT DATEDIFF(MONTH, hire_date, CURRENT_DATE) as months_employed
FROM employees;

-- First and last day functions
SELECT FIRST_DAY(MONTH, CURRENT_DATE) ;
-- Result: First day of current month

SELECT LAST_DAY(MONTH, CURRENT_DATE) ;
-- Result: Last day of current month

-- Business calculations
SELECT 
    employee_id,
    hire_date,
    FIRST_DAY(YEAR, hire_date) as year_start,
    DATEDIFF(DAY, hire_date, CURRENT_DATE) as days_employed,
    DATEDIFF(YEAR, hire_date, CURRENT_DATE) as years_employed
FROM employees;
```

### Conversion Functions

Conversion functions transform data from one type to another.

#### Type Casting Functions
```sql
-- Basic CAST operations
SELECT CAST('123' AS INTEGER) ;          -- Result: 123
SELECT CAST(123.456 AS INTEGER) ;        -- Result: 123
SELECT CAST('2024-12-25' AS DATE) ;      -- Result: 2024-12-25
SELECT CAST(123.456 AS VARCHAR(10)) ;    -- Result: '123.456'

-- Precision and scale specification
SELECT CAST(123.456789 AS DECIMAL(10,2)) ; -- Result: 123.46
SELECT CAST('123.45' AS NUMERIC(8,3)) ;    -- Result: 123.450

-- Practical type conversion
SELECT 
    product_id,
    CAST(price AS DECIMAL(10,2)) as formatted_price,
    CAST(quantity AS VARCHAR(20)) || ' units' as quantity_display
FROM products;
```

#### Encoding/Decoding Functions
```sql
-- Base64 encoding/decoding
SELECT BASE64_ENCODE('Hello World') ;
-- Result: SGVsbG8gV29ybGQ=

SELECT BASE64_DECODE('SGVsbG8gV29ybGQ=') ;
-- Result: Hello World

-- Hexadecimal encoding/decoding
SELECT HEX_ENCODE('Hello') ;
-- Result: 48656C6C6F

SELECT HEX_DECODE('48656C6C6F') ;
-- Result: Hello

-- Store binary data as text
INSERT INTO file_storage (filename, file_data_hex)
VALUES ('image.png', HEX_ENCODE(file_binary_data));

-- Retrieve and decode binary data
SELECT filename, HEX_DECODE(file_data_hex) as file_data
FROM file_storage
WHERE filename = 'image.png';
```

#### UUID Functions
```sql
-- Generate UUIDs
SELECT GEN_UUID() ;
-- Result: 550e8400-e29b-41d4-a716-446655440000

-- Convert between UUID and string formats
SELECT CHAR_TO_UUID('550e8400-e29b-41d4-a716-446655440000') ;
SELECT UUID_TO_CHAR(uuid_column) FROM uuid_table;

-- Use UUIDs as primary keys
CREATE TABLE documents (
    document_id CHAR(16) CHARACTER SET OCTETS DEFAULT GEN_UUID(),
    title VARCHAR(200),
    content BLOB SUB_TYPE TEXT
);

INSERT INTO documents (title, content) 
VALUES ('Sample Document', 'Document content...');
```

### Conditional Functions

Conditional functions provide decision-making logic within SQL expressions.

#### CASE Expressions
```sql
-- Simple CASE expression
SELECT 
    product_name,
    price,
    CASE price
        WHEN 0 THEN 'Free'
        WHEN 1 THEN 'Promotional'
        ELSE 'Regular Price'
    END as price_category
FROM products;

-- Searched CASE expression
SELECT 
    customer_name,
    order_total,
    CASE 
        WHEN order_total < 100 THEN 'Small Order'
        WHEN order_total < 1000 THEN 'Medium Order'
        WHEN order_total < 10000 THEN 'Large Order'
        ELSE 'Enterprise Order'
    END as order_category
FROM orders;

-- Complex CASE with multiple conditions
SELECT 
    employee_name,
    department,
    salary,
    years_employed,
    CASE 
        WHEN department = 'IT' AND years_employed > 5 THEN salary * 1.15
        WHEN department = 'Sales' AND years_employed > 3 THEN salary * 1.10
        WHEN years_employed > 10 THEN salary * 1.05
        ELSE salary
    END as adjusted_salary
FROM employees;
```

#### NULL Handling Functions
```sql
-- COALESCE - return first non-NULL value
SELECT 
    customer_name,
    COALESCE(mobile_phone, home_phone, work_phone, 'No phone') as contact_phone
FROM customers;

-- NULLIF - return NULL if values are equal
SELECT 
    product_name,
    NULLIF(discount_percent, 0) as effective_discount
FROM products;

-- Practical NULL handling
SELECT 
    order_id,
    COALESCE(shipping_date, 'Not yet shipped') as shipping_status,
    COALESCE(tracking_number, 'No tracking') as tracking_info
FROM orders;
```

#### Immediate IF and Other Conditionals
```sql
-- IIF (Immediate IF) - ternary operator
SELECT 
    product_name,
    stock_quantity,
    IIF(stock_quantity > 0, 'In Stock', 'Out of Stock') as availability
FROM products;

-- DECODE - multi-way conditional
SELECT 
    employee_id,
    department_code,
    DECODE(department_code,
           'IT', 'Information Technology',
           'HR', 'Human Resources', 
           'FN', 'Finance',
           'MK', 'Marketing',
           'Unknown Department') as department_name
FROM employees;

-- GREATEST and LEAST - min/max of multiple values
SELECT 
    product_name,
    GREATEST(price, competitor_price1, competitor_price2) as highest_price,
    LEAST(price, competitor_price1, competitor_price2) as lowest_price
FROM product_comparison;
```

---

## Aggregate Functions

Aggregate functions operate on groups of rows to produce summary values.

### Basic Aggregate Functions

#### Count Functions
```sql
-- Count all rows
SELECT COUNT(*) as total_customers FROM customers;

-- Count non-NULL values
SELECT COUNT(email) as customers_with_email FROM customers;

-- Count distinct values
SELECT COUNT(DISTINCT city) as unique_cities FROM customers;

-- Conditional counting
SELECT 
    COUNT(*) as total_orders,
    COUNT(CASE WHEN status = 'completed' THEN 1 END) as completed_orders,
    COUNT(CASE WHEN status = 'pending' THEN 1 END) as pending_orders
FROM orders;
```

#### Sum and Average Functions
```sql
-- Basic sum and average
SELECT 
    SUM(order_total) as total_revenue,
    AVG(order_total) as average_order_value,
    COUNT(*) as order_count
FROM orders
WHERE order_date >= '2024-01-01';

-- Sum with DISTINCT
SELECT 
    SUM(DISTINCT product_price) as unique_price_sum,
    SUM(product_price) as total_price_sum
FROM order_items;

-- Conditional aggregation
SELECT 
    department,
    SUM(salary) as total_payroll,
    AVG(salary) as average_salary,
    SUM(CASE WHEN gender = 'F' THEN salary ELSE 0 END) as female_payroll
FROM employees
GROUP BY department;
```

#### Minimum and Maximum Functions
```sql
-- Basic min/max
SELECT 
    MIN(hire_date) as earliest_hire,
    MAX(hire_date) as latest_hire,
    MIN(salary) as lowest_salary,
    MAX(salary) as highest_salary
FROM employees;

-- String min/max (alphabetical order)
SELECT 
    MIN(customer_name) as first_alphabetically,
    MAX(customer_name) as last_alphabetically
FROM customers;

-- Date range analysis
SELECT 
    product_category,
    MIN(launch_date) as first_product_launch,
    MAX(launch_date) as latest_product_launch,
    DATEDIFF(DAY, MIN(launch_date), MAX(launch_date)) as category_span_days
FROM products
GROUP BY product_category;
```

#### List Aggregation
```sql
-- LIST function - concatenate values
SELECT 
    department,
    LIST(employee_name, ', ') as employee_list
FROM employees
GROUP BY department;

-- List with ordering
SELECT 
    project_id,
    LIST(task_name || ' (' || status || ')', '; ') as task_summary
FROM project_tasks
GROUP BY project_id;

-- Limited list length
SELECT 
    customer_id,
    LIST(CASE WHEN order_date >= '2024-01-01' 
         THEN 'Order #' || order_id 
         END, ', ') as recent_orders
FROM orders
GROUP BY customer_id;
```

### Statistical Aggregate Functions

#### Standard Deviation and Variance
```sql
-- Sample statistics (default)
SELECT 
    department,
    AVG(salary) as mean_salary,
    STDDEV_SAMP(salary) as sample_std_dev,
    VAR_SAMP(salary) as sample_variance
FROM employees
GROUP BY department;

-- Population statistics
SELECT 
    product_category,
    AVG(price) as mean_price,
    STDDEV_POP(price) as population_std_dev,
    VAR_POP(price) as population_variance
FROM products
GROUP BY product_category;

-- Coefficient of variation (relative variability)
SELECT 
    region,
    AVG(sales_amount) as mean_sales,
    STDDEV_SAMP(sales_amount) as std_dev,
    (STDDEV_SAMP(sales_amount) / AVG(sales_amount)) * 100 as coefficient_of_variation_percent
FROM regional_sales
GROUP BY region;
```

#### Correlation and Covariance
```sql
-- Correlation between variables
SELECT 
    department,
    CORR(years_experience, salary) as experience_salary_correlation,
    COVAR_SAMP(years_experience, salary) as sample_covariance,
    COVAR_POP(years_experience, salary) as population_covariance
FROM employees
GROUP BY department;

-- Marketing effectiveness analysis
SELECT 
    campaign_type,
    CORR(advertising_spend, sales_revenue) as spend_revenue_correlation,
    AVG(sales_revenue / advertising_spend) as average_roi
FROM marketing_campaigns
GROUP BY campaign_type;
```

#### Regression Functions
```sql
-- Linear regression analysis
SELECT 
    product_category,
    REGR_SLOPE(sales_amount, price) as price_elasticity,
    REGR_INTERCEPT(sales_amount, price) as base_sales,
    REGR_R2(sales_amount, price) as r_squared,
    REGR_COUNT(sales_amount, price) as data_points
FROM product_sales
GROUP BY product_category;

-- Sales forecasting with regression
SELECT 
    'Overall' as analysis,
    REGR_SLOPE(monthly_sales, month_number) as monthly_trend,
    REGR_INTERCEPT(monthly_sales, month_number) as base_sales,
    REGR_R2(monthly_sales, month_number) as trend_strength
FROM (
    SELECT 
        EXTRACT(YEAR FROM sale_date) * 12 + EXTRACT(MONTH FROM sale_date) as month_number,
        SUM(sale_amount) as monthly_sales
    FROM sales
    GROUP BY EXTRACT(YEAR FROM sale_date), EXTRACT(MONTH FROM sale_date)
) monthly_data;
```

### Filter Clause with Aggregates

All aggregate functions support the FILTER clause for conditional aggregation:

```sql
-- Conditional aggregation with FILTER
SELECT 
    department,
    COUNT(*) as total_employees,
    COUNT(*) FILTER (WHERE salary > 50000) as high_earners,
    AVG(salary) as overall_avg_salary,
    AVG(salary) FILTER (WHERE years_employed > 2) as experienced_avg_salary,
    SUM(salary) FILTER (WHERE gender = 'F') as female_total_salary
FROM employees
GROUP BY department;

-- Time-based conditional aggregation
SELECT 
    product_id,
    COUNT(*) as total_sales,
    COUNT(*) FILTER (WHERE sale_date >= '2024-01-01') as sales_this_year,
    SUM(sale_amount) as total_revenue,
    SUM(sale_amount) FILTER (WHERE EXTRACT(QUARTER FROM sale_date) = 4) as q4_revenue
FROM sales
GROUP BY product_id;

-- Complex business metrics
SELECT 
    region,
    COUNT(DISTINCT customer_id) as total_customers,
    COUNT(DISTINCT customer_id) FILTER (WHERE first_order_date >= '2024-01-01') as new_customers,
    AVG(order_value) FILTER (WHERE order_date >= CURRENT_DATE - 30) as recent_avg_order_value,
    COUNT(*) FILTER (WHERE return_flag = 'Y') as returned_orders
FROM customer_orders
GROUP BY region;
```

---

## Window/Analytical Functions

Window functions provide advanced analytical capabilities with partitioning and ordering.

### Ranking Functions

#### Row Number and Ranking
```sql
-- ROW_NUMBER - sequential numbering
SELECT 
    employee_name,
    department,
    salary,
    ROW_NUMBER() OVER (ORDER BY salary DESC) as overall_rank,
    ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) as dept_rank
FROM employees;

-- RANK and DENSE_RANK - ranking with ties
SELECT 
    student_name,
    subject,
    score,
    RANK() OVER (PARTITION BY subject ORDER BY score DESC) as rank_with_gaps,
    DENSE_RANK() OVER (PARTITION BY subject ORDER BY score DESC) as rank_no_gaps,
    ROW_NUMBER() OVER (PARTITION BY subject ORDER BY score DESC) as row_number
FROM test_scores;

-- Top N per group using ranking
SELECT *
FROM (
    SELECT 
        product_name,
        category,
        sales_amount,
        RANK() OVER (PARTITION BY category ORDER BY sales_amount DESC) as sales_rank
    FROM product_sales
) ranked
WHERE sales_rank <= 3;
```

#### Percentile Rankings
```sql
-- PERCENT_RANK and CUME_DIST
SELECT 
    employee_name,
    salary,
    PERCENT_RANK() OVER (ORDER BY salary) as percent_rank,
    CUME_DIST() OVER (ORDER BY salary) as cumulative_distribution,
    ROUND(PERCENT_RANK() OVER (ORDER BY salary) * 100, 1) as percentile
FROM employees;

-- NTILE - divide into buckets
SELECT 
    customer_name,
    total_purchases,
    NTILE(4) OVER (ORDER BY total_purchases DESC) as quartile,
    NTILE(10) OVER (ORDER BY total_purchases DESC) as decile
FROM customer_summary;

-- Customer segmentation using quartiles
SELECT 
    quartile,
    COUNT(*) as customer_count,
    MIN(total_purchases) as min_purchases,
    MAX(total_purchases) as max_purchases,
    AVG(total_purchases) as avg_purchases
FROM (
    SELECT 
        customer_id,
        total_purchases,
        NTILE(4) OVER (ORDER BY total_purchases DESC) as quartile
    FROM customer_summary
) quartiled
GROUP BY quartile
ORDER BY quartile;
```

### Value Functions

#### Frame-Based Value Access
```sql
-- FIRST_VALUE and LAST_VALUE
SELECT 
    order_date,
    order_amount,
    customer_id,
    FIRST_VALUE(order_amount) OVER (
        PARTITION BY customer_id 
        ORDER BY order_date 
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) as first_order_amount,
    LAST_VALUE(order_amount) OVER (
        PARTITION BY customer_id 
        ORDER BY order_date 
        ROWS BETWEEN CURRENT ROW AND UNBOUNDED FOLLOWING
    ) as last_order_amount
FROM orders;

-- NTH_VALUE - access specific positioned value
SELECT 
    sale_date,
    sale_amount,
    NTH_VALUE(sale_amount, 1) OVER (
        ORDER BY sale_date 
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) as first_sale_amount,
    NTH_VALUE(sale_amount, 2) OVER (
        ORDER BY sale_date 
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) as second_sale_amount
FROM daily_sales;
```

#### Lead and Lag Functions
```sql
-- LAG and LEAD - access previous/next row values
SELECT 
    sale_date,
    sale_amount,
    LAG(sale_amount) OVER (ORDER BY sale_date) as previous_day_sales,
    LEAD(sale_amount) OVER (ORDER BY sale_date) as next_day_sales,
    sale_amount - LAG(sale_amount) OVER (ORDER BY sale_date) as daily_change
FROM daily_sales;

-- Multi-period comparisons
SELECT 
    year_month,
    monthly_revenue,
    LAG(monthly_revenue, 1) OVER (ORDER BY year_month) as previous_month,
    LAG(monthly_revenue, 12) OVER (ORDER BY year_month) as same_month_last_year,
    monthly_revenue - LAG(monthly_revenue, 1) OVER (ORDER BY year_month) as month_over_month_change,
    monthly_revenue - LAG(monthly_revenue, 12) OVER (ORDER BY year_month) as year_over_year_change
FROM monthly_revenue_summary;

-- Customer behavior analysis
SELECT 
    customer_id,
    order_date,
    order_amount,
    LAG(order_date) OVER (PARTITION BY customer_id ORDER BY order_date) as previous_order_date,
    order_date - LAG(order_date) OVER (PARTITION BY customer_id ORDER BY order_date) as days_between_orders,
    LAG(order_amount) OVER (PARTITION BY customer_id ORDER BY order_date) as previous_order_amount
FROM customer_orders;
```

### Window Frames

Window functions support sophisticated frame specifications:

```sql
-- Running totals and moving averages
SELECT 
    sale_date,
    daily_sales,
    SUM(daily_sales) OVER (
        ORDER BY sale_date 
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) as running_total,
    AVG(daily_sales) OVER (
        ORDER BY sale_date 
        ROWS BETWEEN 6 PRECEDING AND CURRENT ROW
    ) as seven_day_average,
    AVG(daily_sales) OVER (
        ORDER BY sale_date 
        ROWS BETWEEN 29 PRECEDING AND CURRENT ROW
    ) as thirty_day_average
FROM daily_sales_summary;

-- Range-based windows (value-based instead of row-based)
SELECT 
    employee_name,
    hire_date,
    salary,
    AVG(salary) OVER (
        ORDER BY hire_date 
        RANGE BETWEEN INTERVAL '1' YEAR PRECEDING AND CURRENT ROW
    ) as avg_salary_hired_within_year
FROM employees;

-- Frame exclusion
SELECT 
    product_id,
    sale_date,
    sale_amount,
    AVG(sale_amount) OVER (
        PARTITION BY product_id
        ORDER BY sale_date
        ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING
        EXCLUDE CURRENT ROW
    ) as avg_surrounding_sales
FROM product_sales;
```

---

## System Functions and Context Variables

System functions provide access to database and session information.

### User and Session Context
```sql
-- Current user information
SELECT CURRENT_USER ;        -- Current user name
SELECT CURRENT_ROLE ;        -- Current role
SELECT USER ;                -- Alias for CURRENT_USER

-- Connection and transaction information
SELECT CURRENT_CONNECTION ;  -- Connection ID
SELECT CURRENT_TRANSACTION ; -- Transaction ID

-- Schema context
SELECT CURRENT_SCHEMA ;      -- Current schema

-- Session settings
SELECT CURRENT_PASCAL_CASE_MODE ; -- Pascal case mode
```

### Error Information Functions
```sql
-- SQL error codes (typically used in exception handling)
SELECT SQLSTATE ;   -- SQL state code
SELECT SQLCODE ;    -- SQL error code  
SELECT GDSCODE ;    -- Firebird/ScratchBird error code

-- Row count information
SELECT ROW_COUNT ;      -- Affected rows in last operation
SELECT ROWS_AFFECTED ;  -- Alias for ROW_COUNT

-- Usage in stored procedures for error handling
CREATE PROCEDURE update_customer_email(
    customer_id INTEGER,
    new_email VARCHAR(255)
)
AS
BEGIN
    UPDATE customers 
    SET email = :new_email 
    WHERE customer_id = :customer_id;
    
    IF (ROW_COUNT = 0) THEN
        EXCEPTION customer_not_found;
END
```

### Context Variables
```sql
-- Get context variables
SELECT RDB$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'DB_NAME') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'ISOLATION_LEVEL') ;

-- User-defined context variables
SELECT RDB$SET_CONTEXT('USER_SESSION', 'login_time', CURRENT_TIMESTAMP) ;
SELECT RDB$GET_CONTEXT('USER_SESSION', 'login_time') ;

-- Transaction context
SELECT RDB$SET_CONTEXT('USER_TRANSACTION', 'operation_type', 'BULK_INSERT') ;
SELECT RDB$GET_CONTEXT('USER_TRANSACTION', 'operation_type') ;

-- Common system context variables
SELECT RDB$GET_CONTEXT('SYSTEM', 'CLIENT_VERSION') as client_version ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CURRENT_USER') as current_user ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CURRENT_ROLE') as current_role ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'SESSION_ID') as session_id ;
```

### Security Functions
```sql
-- Check role usage
SELECT RDB$ROLE_IN_USE('MANAGER') ;    -- Returns TRUE/FALSE
SELECT RDB$ROLE_IN_USE('DEVELOPER') ;

-- Check system privileges
SELECT RDB$SYSTEM_PRIVILEGE('USER_MANAGEMENT') ;
SELECT RDB$SYSTEM_PRIVILEGE('BACKUP_DATABASE') ;

-- Transaction management
SELECT RDB$GET_TRANSACTION_CN(CURRENT_TRANSACTION) ;

-- Practical security usage
SELECT 
    table_name,
    CASE 
        WHEN RDB$SYSTEM_PRIVILEGE('ALTER_DATABASE') THEN 'Full Access'
        WHEN RDB$ROLE_IN_USE('MANAGER') THEN 'Manager Access'
        ELSE 'Limited Access'
    END as access_level
FROM information_schema.tables;
```

### Utility Functions
```sql
-- Random number generation
SELECT RAND() ;  -- Random float between 0.0 and 1.0

-- Generate random integers in range
SELECT FLOOR(RAND() * 100) + 1 as random_1_to_100 ;

-- Database key generation
SELECT MAKE_DBKEY(0, 1) ;  -- Create database key

-- Practical random usage
SELECT 
    customer_id,
    customer_name,
    RAND() as random_sort
FROM customers
ORDER BY random_sort  -- Random order
ROWS 10;              -- Random sample of 10 customers
```

---

## ScratchBird-Specific Function Extensions

ScratchBird provides advanced function extensions beyond standard SQL.

### JSON Functions

ScratchBird includes comprehensive JSON manipulation functions:

```sql
-- Create JSON structures
SELECT JSON_ARRAY('apple', 'banana', 'cherry') ;
-- Result: ["apple", "banana", "cherry"]

SELECT JSON_OBJECT('name', 'John', 'age', 30, 'city', 'New York') ;
-- Result: {"name": "John", "age": 30, "city": "New York"}

-- Extract JSON values
SELECT JSON_EXTRACT('{"name": "John", "age": 30}', '$.name') ;
-- Result: "John"

SELECT JSON_EXTRACT('{"users": [{"id": 1}, {"id": 2}]}', '$.users[0].id') ;
-- Result: 1

-- Merge JSON objects
SELECT JSON_MERGE(
    '{"name": "John", "age": 30}',
    '{"city": "New York", "country": "USA"}'
) ;
-- Result: {"name": "John", "age": 30, "city": "New York", "country": "USA"}

-- Validate JSON
SELECT JSON_VALID('{"valid": "json"}') ;  -- Result: TRUE
SELECT JSON_VALID('{invalid json}') ;     -- Result: FALSE

-- Practical JSON usage
CREATE TABLE api_logs (
    log_id INTEGER GENERATED BY DEFAULT AS IDENTITY,
    endpoint VARCHAR(200),
    request_data BLOB SUB_TYPE TEXT,
    response_data BLOB SUB_TYPE TEXT,
    log_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Query JSON data
SELECT 
    endpoint,
    JSON_EXTRACT(request_data, '$.user_id') as user_id,
    JSON_EXTRACT(response_data, '$.status') as response_status
FROM api_logs
WHERE JSON_VALID(request_data) = TRUE
  AND JSON_EXTRACT(response_data, '$.status') = 'success';
```

### Array Functions (PostgreSQL-compatible)

```sql
-- Array creation and manipulation
SELECT ARRAY['apple', 'banana', 'cherry'] ;

-- Array append and prepend
SELECT ARRAY_APPEND(ARRAY['a', 'b'], 'c') ;  -- Result: ['a', 'b', 'c']
SELECT ARRAY_PREPEND('x', ARRAY['a', 'b']) ; -- Result: ['x', 'a', 'b']

-- Array concatenation
SELECT ARRAY_CAT(ARRAY[1, 2], ARRAY[3, 4]) ; -- Result: [1, 2, 3, 4]

-- Array information
SELECT ARRAY_LENGTH(ARRAY[1, 2, 3, 4, 5], 1) ; -- Result: 5
SELECT ARRAY_UPPER(ARRAY[1, 2, 3], 1) ;        -- Result: 3
SELECT ARRAY_LOWER(ARRAY[1, 2, 3], 1) ;        -- Result: 1

-- Array search
SELECT ARRAY_POSITION(ARRAY['a', 'b', 'c'], 'b') ; -- Result: 2
SELECT ARRAY_POSITIONS(ARRAY[1, 2, 1, 3], 1) ;     -- Result: [1, 3]

-- Array conversion
SELECT ARRAY_TO_STRING(ARRAY['red', 'green', 'blue'], ', ') ;
-- Result: 'red, green, blue'

SELECT STRING_TO_ARRAY('red,green,blue', ',') ;
-- Result: ['red', 'green', 'blue']

-- Array unnesting
SELECT UNNEST(ARRAY['apple', 'banana', 'cherry']) as fruit ;
-- Results in multiple rows: apple, banana, cherry

-- Practical array usage
CREATE TABLE product_tags (
    product_id INTEGER,
    tags VARCHAR(255) ARRAY
);

INSERT INTO product_tags VALUES (1, ARRAY['electronics', 'smartphone', 'android']);
INSERT INTO product_tags VALUES (2, ARRAY['clothing', 'shirt', 'cotton']);

-- Query arrays
SELECT 
    product_id,
    ARRAY_TO_STRING(tags, ', ') as tag_list,
    ARRAY_LENGTH(tags, 1) as tag_count
FROM product_tags
WHERE 'electronics' = ANY(tags);
```

### Vector Functions (AI/ML Operations)

ScratchBird provides advanced vector operations for AI/ML applications:

```sql
-- Create vectors
SELECT VECTOR_CREATE(3, 0.0) ;  -- [0.0, 0.0, 0.0]
SELECT VECTOR_FROM_ARRAY(ARRAY[1.0, 2.0, 3.0]) ;

-- Vector arithmetic
SELECT VECTOR_ADD(VECTOR[1.0, 2.0], VECTOR[3.0, 4.0]) ;    -- [4.0, 6.0]
SELECT VECTOR_SUBTRACT(VECTOR[5.0, 3.0], VECTOR[2.0, 1.0]) ; -- [3.0, 2.0]
SELECT VECTOR_MULTIPLY(VECTOR[2.0, 3.0], 2.0) ;             -- [4.0, 6.0]

-- Vector analysis
SELECT VECTOR_MAGNITUDE(VECTOR[3.0, 4.0]) ;  -- Result: 5.0
SELECT VECTOR_DOT_PRODUCT(VECTOR[1.0, 2.0], VECTOR[3.0, 4.0]) ; -- Result: 11.0

-- Vector distances and similarity
SELECT VECTOR_DISTANCE(VECTOR[1.0, 2.0], VECTOR[3.0, 4.0], 'euclidean') ;
SELECT VECTOR_SIMILARITY(VECTOR[1.0, 2.0], VECTOR[3.0, 4.0], 'cosine') ;

-- Normalize vectors
SELECT VECTOR_NORMALIZE(VECTOR[3.0, 4.0]) ;  -- [0.6, 0.8]

-- AI/ML applications
CREATE TABLE document_embeddings (
    document_id INTEGER,
    title VARCHAR(200),
    embedding VECTOR(512)  -- 512-dimensional vector
);

-- Find similar documents
SELECT 
    d1.document_id,
    d1.title,
    VECTOR_SIMILARITY(d1.embedding, d2.embedding, 'cosine') as similarity
FROM document_embeddings d1, document_embeddings d2
WHERE d1.document_id != d2.document_id
  AND d2.document_id = 123  -- Compare to specific document
ORDER BY similarity DESC
ROWS 10;
```

### Full-Text Search Functions (PostgreSQL-compatible)

```sql
-- Create text search vectors
SELECT TO_TSVECTOR('english', 'The quick brown fox jumps over the lazy dog') ;

-- Create text search queries
SELECT TO_TSQUERY('english', 'quick & fox') ;
SELECT PLAINTO_TSQUERY('english', 'quick fox') ;
SELECT PHRASETO_TSQUERY('english', 'quick brown fox') ;

-- Text search ranking
SELECT TS_RANK(
    TO_TSVECTOR('english', 'The quick brown fox'),
    TO_TSQUERY('english', 'quick & fox')
) ;

-- Generate search headlines
SELECT TS_HEADLINE(
    'english',
    'The quick brown fox jumps over the lazy dog',
    TO_TSQUERY('english', 'quick & fox'),
    'StartSel=<b>, StopSel=</b>, MaxWords=20'
) ;
-- Result: The <b>quick</b> brown <b>fox</b> jumps over the lazy dog

-- Full-text search application
CREATE TABLE documents (
    doc_id INTEGER GENERATED BY DEFAULT AS IDENTITY,
    title VARCHAR(200),
    content BLOB SUB_TYPE TEXT,
    search_vector TSVECTOR GENERATED ALWAYS AS (
        TO_TSVECTOR('english', title || ' ' || content)
    ) STORED
);

-- Create GIN index for fast searching
CREATE INDEX idx_documents_search ON documents USING GIN (search_vector);

-- Search documents
SELECT 
    doc_id,
    title,
    TS_RANK(search_vector, query) as rank,
    TS_HEADLINE('english', content, query) as headline
FROM documents, TO_TSQUERY('english', 'database & performance') as query
WHERE search_vector @@ query
ORDER BY rank DESC;
```

### Spatial Functions (PostGIS-compatible)

```sql
-- Create geometric objects
SELECT ST_GEOMFROMTEXT('POINT(1 1)') ;
SELECT ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1, 2 2)') ;
SELECT ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 1 0, 0 0))') ;

-- Spatial relationships
SELECT ST_CONTAINS(
    ST_GEOMFROMTEXT('POLYGON((0 0, 0 2, 2 2, 2 0, 0 0))'),
    ST_GEOMFROMTEXT('POINT(1 1)')
) ;  -- Result: TRUE

SELECT ST_INTERSECTS(
    ST_GEOMFROMTEXT('LINESTRING(0 0, 2 2)'),
    ST_GEOMFROMTEXT('LINESTRING(0 2, 2 0)')
) ;  -- Result: TRUE

-- Spatial measurements
SELECT ST_DISTANCE(
    ST_GEOMFROMTEXT('POINT(0 0)'),
    ST_GEOMFROMTEXT('POINT(3 4)')
) ;  -- Result: 5.0

SELECT ST_AREA(
    ST_GEOMFROMTEXT('POLYGON((0 0, 0 2, 2 2, 2 0, 0 0))')
) ;  -- Result: 4.0

-- Spatial processing
SELECT ST_CENTROID(
    ST_GEOMFROMTEXT('POLYGON((0 0, 0 2, 2 2, 2 0, 0 0))')
) ;  -- Result: POINT(1 1)

SELECT ST_BUFFER(
    ST_GEOMFROMTEXT('POINT(0 0)'), 
    1.0
) ;  -- Circle with radius 1

-- GIS application
CREATE TABLE locations (
    location_id INTEGER GENERATED BY DEFAULT AS IDENTITY,
    name VARCHAR(100),
    coordinates GEOMETRY(POINT, 4326)  -- WGS84 coordinates
);

CREATE INDEX idx_locations_spatial ON locations USING GIST (coordinates);

-- Find nearby locations
SELECT 
    name,
    ST_DISTANCE(
        coordinates, 
        ST_GEOMFROMTEXT('POINT(-122.4194 37.7749)', 4326)  -- San Francisco
    ) as distance_meters
FROM locations
WHERE ST_DWITHIN(
    coordinates,
    ST_GEOMFROMTEXT('POINT(-122.4194 37.7749)', 4326),
    1000  -- Within 1km
)
ORDER BY distance_meters;
```

### Network Functions (PostgreSQL-compatible)

```sql
-- Network address functions
SELECT HOST('192.168.1.10/24'::INET) ;        -- Result: 192.168.1.10
SELECT MASKLEN('192.168.1.10/24'::INET) ;     -- Result: 24
SELECT NETMASK('192.168.1.10/24'::INET) ;     -- Result: 255.255.255.0
SELECT BROADCAST('192.168.1.10/24'::INET) ;   -- Result: 192.168.1.255

-- Address family information
SELECT FAMILY('192.168.1.10'::INET) ;         -- Result: 4 (IPv4)
SELECT FAMILY('2001:db8::1'::INET) ;          -- Result: 6 (IPv6)

-- Network operations
SELECT INET_SAME_FAMILY(
    '192.168.1.10'::INET, 
    '10.0.0.1'::INET
) ;  -- Result: TRUE

-- Network administration
CREATE TABLE network_devices (
    device_id INTEGER GENERATED BY DEFAULT AS IDENTITY,
    device_name VARCHAR(100),
    ip_address INET,
    mac_address MACADDR,
    network_segment CIDR
);

-- Network queries
SELECT 
    device_name,
    ip_address,
    ABBREV(ip_address) as short_ip,
    HOST(network_segment) as network,
    MASKLEN(network_segment) as subnet_mask
FROM network_devices
WHERE ip_address << '192.168.1.0/24'::CIDR;  -- Devices in subnet
```

### Cryptographic Functions

```sql
-- Hashing functions
SELECT HASH('password123') ;                    -- Simple hash
SELECT CRYPT_HASH('SHA256', 'secret data') ;    -- Cryptographic hash

-- Symmetric encryption
SELECT ENCRYPT('AES', 'encryption_key', 'sensitive data') ;
SELECT DECRYPT('AES', 'encryption_key', encrypted_data) FROM encrypted_table;

-- RSA encryption (asymmetric)
SELECT RSA_PRIVATE(2048) ;  -- Generate 2048-bit private key

-- Using RSA keys
WITH keys AS (
    SELECT RSA_PRIVATE(2048) as private_key
)
SELECT 
    private_key,
    RSA_PUBLIC(private_key) as public_key,
    RSA_ENCRYPT(RSA_PUBLIC(private_key), 'secret message') as encrypted_message
FROM keys;

-- Digital signatures
SELECT RSA_SIGN_HASH(private_key, CRYPT_HASH('SHA256', 'document content'))
FROM key_storage;

-- Secure data storage
CREATE TABLE secure_documents (
    doc_id INTEGER GENERATED BY DEFAULT AS IDENTITY,
    title VARCHAR(200),
    encrypted_content BLOB,
    signature BLOB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Store encrypted document
INSERT INTO secure_documents (title, encrypted_content, signature)
VALUES (
    'Confidential Report',
    ENCRYPT('AES256', 'secret_key', 'Document content...'),
    RSA_SIGN_HASH(@private_key, CRYPT_HASH('SHA256', 'Document content...'))
);
```

---

## Performance Considerations and Best Practices

### Function Performance Guidelines

#### Scalar Function Optimization
```sql
-- Efficient string operations
-- Good: Use functions in WHERE clause carefully
SELECT * FROM customers 
WHERE UPPER(customer_name) = 'JOHN DOE';

-- Better: Use functional index
CREATE INDEX idx_customers_name_upper ON customers (UPPER(customer_name));

-- Good: Avoid repeated function calls
SELECT 
    customer_name,
    UPPER(customer_name),  -- Computed once
    LOWER(customer_name)   -- Computed once
FROM customers;

-- Poor: Repeated expensive calculations
SELECT customer_name
FROM customers 
WHERE EXTRACT(YEAR FROM birth_date) = 2000
  AND EXTRACT(MONTH FROM birth_date) = 1;

-- Better: Use date ranges
SELECT customer_name
FROM customers
WHERE birth_date >= DATE '2000-01-01' 
  AND birth_date < DATE '2000-02-01';
```

#### Aggregate Function Optimization
```sql
-- Efficient aggregate usage
-- Good: Use indexes for GROUP BY
CREATE INDEX idx_orders_customer_date ON orders (customer_id, order_date);

SELECT 
    customer_id,
    COUNT(*) as order_count,
    SUM(order_total) as total_spent
FROM orders
GROUP BY customer_id;

-- Efficient conditional aggregation
SELECT 
    department,
    COUNT(*) as total_employees,
    COUNT(*) FILTER (WHERE salary > 50000) as high_earners,
    AVG(salary) FILTER (WHERE years_employed > 2) as experienced_avg
FROM employees
GROUP BY department;

-- Use HAVING instead of WHERE with aggregates
-- Good:
SELECT customer_id, COUNT(*)
FROM orders
GROUP BY customer_id
HAVING COUNT(*) > 10;

-- Poor (if possible):
SELECT customer_id, order_count
FROM (
    SELECT customer_id, COUNT(*) as order_count
    FROM orders
    GROUP BY customer_id
) 
WHERE order_count > 10;
```

#### Window Function Optimization
```sql
-- Efficient window function usage
-- Good: Use appropriate partitioning
SELECT 
    employee_name,
    department,
    salary,
    AVG(salary) OVER (PARTITION BY department) as dept_avg_salary
FROM employees;

-- Avoid unnecessary ordering when not needed
-- Good: Simple ranking
SELECT 
    product_name,
    sales_amount,
    RANK() OVER (ORDER BY sales_amount DESC) as sales_rank
FROM products;

-- Be careful with frame specifications
-- Efficient: Bounded frames
SELECT 
    sale_date,
    daily_sales,
    SUM(daily_sales) OVER (
        ORDER BY sale_date 
        ROWS BETWEEN 6 PRECEDING AND CURRENT ROW
    ) as seven_day_total
FROM daily_sales;

-- Less efficient: Unbounded following
SELECT 
    sale_date,
    daily_sales,
    SUM(daily_sales) OVER (
        ORDER BY sale_date 
        ROWS BETWEEN CURRENT ROW AND UNBOUNDED FOLLOWING
    ) as remaining_total
FROM daily_sales;
```

### ScratchBird Extension Performance

#### Vector Function Optimization
```sql
-- Efficient vector operations
-- Good: Use appropriate vector dimensions
CREATE TABLE embeddings (
    doc_id INTEGER,
    embedding VECTOR(512)  -- Standard dimension
);

-- Create specialized index for vector similarity
CREATE INDEX idx_embeddings_vector ON embeddings 
USING IVFFLAT (embedding vector_cosine_ops) 
WITH (lists = 100);

-- Efficient similarity search
SELECT doc_id, VECTOR_SIMILARITY(embedding, @query_vector, 'cosine') as similarity
FROM embeddings
ORDER BY similarity DESC
ROWS 10;
```

#### Spatial Function Optimization
```sql
-- Efficient spatial queries
-- Always use spatial indexes
CREATE INDEX idx_locations_geom ON locations USING GIST (geom);

-- Use spatial operators instead of functions when possible
-- Good:
SELECT * FROM locations WHERE geom && ST_MakeEnvelope(0, 0, 1, 1);

-- Then apply more precise function
SELECT * FROM locations 
WHERE geom && ST_MakeEnvelope(0, 0, 1, 1)
  AND ST_Contains(ST_MakeEnvelope(0, 0, 1, 1), geom);

-- Efficient distance queries with bounding
SELECT * FROM locations
WHERE ST_DWithin(geom, ST_Point(0, 0), 1000)
ORDER BY ST_Distance(geom, ST_Point(0, 0));
```

#### Full-Text Search Optimization
```sql
-- Efficient text search
-- Use GIN indexes for tsvector columns
CREATE INDEX idx_documents_fts ON documents USING GIN (search_vector);

-- Efficient search queries
SELECT doc_id, title, TS_RANK(search_vector, query) as rank
FROM documents, TO_TSQUERY('english', 'database & performance') as query
WHERE search_vector @@ query
ORDER BY rank DESC;

-- Consider pg_trgm for flexible text search
CREATE EXTENSION IF NOT EXISTS pg_trgm;
CREATE INDEX idx_documents_title_trgm ON documents USING GIN (title gin_trgm_ops);

-- Similarity search
SELECT doc_id, title, SIMILARITY(title, 'database performance') as sim
FROM documents
WHERE title % 'database performance'
ORDER BY sim DESC;
```

