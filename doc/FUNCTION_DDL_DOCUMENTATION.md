# ScratchBird FUNCTION - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

FUNCTION objects in ScratchBird are named computational units that accept input parameters and return a single scalar value. Unlike procedures which can have multiple output parameters, functions are designed for use in expressions, SELECT statements, and anywhere a computed value is needed. Functions are essential for creating reusable calculations, data transformations, and complex business logic that returns values.

### Key Features and Capabilities

- **Single Return Value**: Functions return exactly one scalar value of a specified data type
- **Expression Integration**: Functions can be used in SQL expressions, WHERE clauses, and SELECT lists
- **Input Parameters**: Accept typed input parameters for computation
- **PSQL Programming**: Full PSQL language support with variables, loops, and conditions
- **Deterministic Control**: Optional DETERMINISTIC keyword for optimization hints
- **External Language Support**: Integration with external languages (Java, C++, etc.)
- **Security Modes**: SQL SECURITY DEFINER/INVOKER for execution privilege control
- **Local Subroutines**: Nested functions and procedures within function bodies
- **Exception Handling**: Robust error handling with custom exceptions

### ScratchBird-Specific Enhancements

1. **Hierarchical Schema Support**: Functions support 3-level qualified names (`schema.subschema.function`)
2. **Enhanced Return Types**: Support for all ScratchBird data types including extended types
3. **Deterministic Optimization**: DETERMINISTIC/NOT DETERMINISTIC for query optimizer hints
4. **External Language Integration**: Comprehensive foreign language function support
5. **Package Integration**: Functions can be grouped in packages for namespace organization
6. **IF NOT EXISTS Support**: Conditional creation syntax for deployment scripts
7. **RECREATE Support**: Atomic drop-and-recreate operations
8. **CREATE OR ALTER Support**: Flexible function modification syntax
9. **Advanced Parameter Types**: Support for all ScratchBird data types including arrays and custom domains

---

## DDL Syntax Reference

### CREATE FUNCTION

Creates a new user-defined function with specified parameters and return type.

#### **Basic PSQL Function Syntax**
```sql
CREATE [OR ALTER] FUNCTION [IF NOT EXISTS] [schema_name.]function_name
    [(input_parameters)]
    RETURNS data_type [COLLATE collation_name]
    [DETERMINISTIC | NOT DETERMINISTIC]
    [SQL SECURITY {DEFINER | INVOKER}]
AS
[DECLARE
    -- Local variable declarations
    variable_name data_type [DEFAULT value];
    -- Local procedure/function declarations
    DECLARE PROCEDURE local_proc_name ...;
    DECLARE FUNCTION local_func_name ...;
]
BEGIN
    -- Function body statements
    RETURN expression;
END;
```

#### **External Function Syntax**
```sql
CREATE [OR ALTER] FUNCTION [IF NOT EXISTS] [schema_name.]function_name
    [(input_parameters)]
    RETURNS data_type [COLLATE collation_name]
    [DETERMINISTIC | NOT DETERMINISTIC]
EXTERNAL NAME 'external_name'
ENGINE engine_name
[AS 'external_body'];
```

#### **Complete Syntax with All Options**
```sql
CREATE [OR ALTER] FUNCTION [IF NOT EXISTS] [[catalog.]schema.]function_name
    [([IN] parameter_name data_type [DEFAULT value]
      [, [IN] parameter_name data_type [DEFAULT value] ...])]
    RETURNS data_type [COLLATE collation_name]
    [DETERMINISTIC | NOT DETERMINISTIC]
    [SQL SECURITY {DEFINER | INVOKER}]
{
    AS
    [DECLARE
        variable_name data_type [DEFAULT value];
        cursor_name CURSOR FOR (select_statement);
        exception_name EXCEPTION 'error_message';
        DECLARE PROCEDURE local_procedure_name ...;
        DECLARE FUNCTION local_function_name ...;
    ]
    BEGIN
        function_statements
        RETURN expression;
    END
    |
    EXTERNAL NAME 'external_module_entry_point'
    ENGINE engine_name
    [AS 'external_body_source']
};
```

#### **Parameters**

- **OR ALTER**: Modify existing function or create if doesn't exist
- **IF NOT EXISTS**: Skip creation if function already exists (no error)
- **schema_name**: Optional schema qualification (supports hierarchical schemas)
- **function_name**: Function identifier (63 characters max)
- **input_parameters**: Comma-separated list of input parameters with types
- **RETURNS data_type**: Return value data type specification
- **COLLATE**: Optional collation for character return types
- **DETERMINISTIC**: Indicates function returns same result for same inputs (optimization hint)
- **SQL SECURITY**: Security context (DEFINER or INVOKER privileges)
- **DECLARE section**: Local variables, cursors, exceptions, and subroutines
- **RETURN statement**: Required statement returning the function result

---

## CREATE FUNCTION Examples

### **Basic Mathematical Functions**

#### **Simple Calculation Functions**
```sql
-- Basic arithmetic function
CREATE FUNCTION calculate_area(
    length DECIMAL(10,2),
    width DECIMAL(10,2)
)
RETURNS DECIMAL(15,2)
DETERMINISTIC
AS
BEGIN
    RETURN length * width;
END;

-- Usage in SELECT statement
SELECT product_name, calculate_area(length, width) as area
FROM products;

-- Compound interest calculation
CREATE FUNCTION compound_interest(
    principal DECIMAL(15,2),
    rate DECIMAL(5,4),
    periods INTEGER
)
RETURNS DECIMAL(15,2)
DETERMINISTIC
AS
BEGIN
    RETURN principal * POWER((1 + rate), periods) - principal;
END;

-- Usage in expressions
SELECT account_id, 
       balance,
       balance + compound_interest(balance, 0.045, 12) as projected_balance
FROM savings_accounts;
```

#### **String Manipulation Functions**
```sql
-- Format phone number function
CREATE FUNCTION format_phone_number(
    phone_digits VARCHAR(20)
)
RETURNS VARCHAR(20)
DETERMINISTIC
AS
DECLARE
    cleaned_digits VARCHAR(20);
    digit_count INTEGER;
BEGIN
    -- Remove non-digit characters
    cleaned_digits = REPLACE(REPLACE(REPLACE(REPLACE(phone_digits, 
        '(', ''), ')', ''), '-', ''), ' ', '');
    
    digit_count = CHAR_LENGTH(cleaned_digits);
    
    -- Format based on digit count
    IF (digit_count = 10) THEN
        RETURN '(' || SUBSTRING(cleaned_digits FROM 1 FOR 3) || ') ' ||
               SUBSTRING(cleaned_digits FROM 4 FOR 3) || '-' ||
               SUBSTRING(cleaned_digits FROM 7 FOR 4);
    ELSE IF (digit_count = 11 AND SUBSTRING(cleaned_digits FROM 1 FOR 1) = '1') THEN
        RETURN '1-(' || SUBSTRING(cleaned_digits FROM 2 FOR 3) || ') ' ||
               SUBSTRING(cleaned_digits FROM 5 FOR 3) || '-' ||
               SUBSTRING(cleaned_digits FROM 8 FOR 4);
    ELSE
        RETURN phone_digits; -- Return original if can't format
END;

-- Usage example
SELECT customer_name, format_phone_number(phone) as formatted_phone
FROM customers;
```

#### **Date and Time Functions**
```sql
-- Calculate age in years function
CREATE FUNCTION calculate_age(
    birth_date DATE
)
RETURNS INTEGER
DETERMINISTIC
AS
DECLARE
    age_years INTEGER;
BEGIN
    age_years = EXTRACT(YEAR FROM CURRENT_DATE) - EXTRACT(YEAR FROM birth_date);
    
    -- Adjust if birthday hasn't occurred this year
    IF (EXTRACT(MONTH FROM CURRENT_DATE) < EXTRACT(MONTH FROM birth_date) OR
        (EXTRACT(MONTH FROM CURRENT_DATE) = EXTRACT(MONTH FROM birth_date) AND
         EXTRACT(DAY FROM CURRENT_DATE) < EXTRACT(DAY FROM birth_date))) THEN
        age_years = age_years - 1;
    
    RETURN age_years;
END;

-- Business days between dates function
CREATE FUNCTION business_days_between(
    start_date DATE,
    end_date DATE
)
RETURNS INTEGER
DETERMINISTIC
AS
DECLARE
    current_date DATE;
    day_count INTEGER DEFAULT 0;
    day_of_week INTEGER;
BEGIN
    current_date = start_date;
    
    WHILE (current_date <= end_date) DO
    BEGIN
        day_of_week = EXTRACT(WEEKDAY FROM current_date);
        
        -- Monday = 1, Sunday = 7; exclude Saturday (6) and Sunday (7)
        IF (day_of_week BETWEEN 1 AND 5) THEN
            day_count = day_count + 1;
        
        current_date = current_date + 1;
    END
    
    RETURN day_count;
END;
```

### **Business Logic Functions**

#### **Tax and Financial Calculations**
```sql
-- Progressive tax calculation function
CREATE FUNCTION calculate_income_tax(
    gross_income DECIMAL(15,2),
    tax_year INTEGER
)
RETURNS DECIMAL(15,2)
AS
DECLARE
    tax_owed DECIMAL(15,2) DEFAULT 0.00;
    taxable_income DECIMAL(15,2);
BEGIN
    taxable_income = gross_income;
    
    -- 2025 tax brackets (simplified example)
    IF (tax_year = 2025) THEN
    BEGIN
        -- 10% on first $11,000
        IF (taxable_income > 11000) THEN
        BEGIN
            tax_owed = tax_owed + (11000 * 0.10);
            taxable_income = taxable_income - 11000;
        END
        ELSE
        BEGIN
            RETURN taxable_income * 0.10;
        END
        
        -- 12% on next $33,725 ($11,001 to $44,725)
        IF (taxable_income > 33725) THEN
        BEGIN
            tax_owed = tax_owed + (33725 * 0.12);
            taxable_income = taxable_income - 33725;
        END
        ELSE
        BEGIN
            RETURN tax_owed + (taxable_income * 0.12);
        END
        
        -- 22% on remaining income above $44,725
        tax_owed = tax_owed + (taxable_income * 0.22);
    END
    
    RETURN tax_owed;
END;

-- Customer discount calculation
CREATE FUNCTION calculate_customer_discount(
    customer_type VARCHAR(20),
    order_amount DECIMAL(15,2),
    loyalty_years INTEGER
)
RETURNS DECIMAL(15,2)
AS
DECLARE
    base_discount DECIMAL(5,2);
    loyalty_bonus DECIMAL(5,2);
    max_discount DECIMAL(5,2) DEFAULT 25.00;
BEGIN
    -- Base discount by customer type
    base_discount = CASE customer_type
        WHEN 'VIP' THEN 15.00
        WHEN 'PREMIUM' THEN 10.00
        WHEN 'STANDARD' THEN 5.00
        ELSE 0.00
    END;
    
    -- Loyalty bonus (0.5% per year, max 10%)
    loyalty_bonus = LEAST(loyalty_years * 0.5, 10.00);
    
    -- Large order bonus
    IF (order_amount > 10000) THEN
        loyalty_bonus = loyalty_bonus + 2.00;
    ELSE IF (order_amount > 5000) THEN
        loyalty_bonus = loyalty_bonus + 1.00;
    
    -- Apply maximum discount limit
    RETURN LEAST(base_discount + loyalty_bonus, max_discount);
END;
```

#### **Data Validation Functions**
```sql
-- Email validation function
CREATE FUNCTION is_valid_email(
    email_address VARCHAR(255)
)
RETURNS BOOLEAN
DETERMINISTIC
AS
DECLARE
    at_position INTEGER;
    dot_position INTEGER;
    email_length INTEGER;
BEGIN
    -- Check for null or empty
    IF (email_address IS NULL OR TRIM(email_address) = '') THEN
        RETURN FALSE;
    
    email_length = CHAR_LENGTH(TRIM(email_address));
    
    -- Check length limits
    IF (email_length < 5 OR email_length > 255) THEN
        RETURN FALSE;
    
    -- Find @ symbol
    at_position = POSITION('@' IN email_address);
    IF (at_position <= 1 OR at_position = email_length) THEN
        RETURN FALSE;
    
    -- Check for domain part with dot
    dot_position = POSITION('.' IN SUBSTRING(email_address FROM at_position));
    IF (dot_position <= 2 OR dot_position >= (email_length - at_position)) THEN
        RETURN FALSE;
    
    -- Additional checks could be added here
    RETURN TRUE;
END;

-- Credit card validation (Luhn algorithm)
CREATE FUNCTION is_valid_credit_card(
    card_number VARCHAR(20)
)
RETURNS BOOLEAN
DETERMINISTIC
AS
DECLARE
    cleaned_number VARCHAR(16);
    digit_sum INTEGER DEFAULT 0;
    current_digit INTEGER;
    position INTEGER;
    alternate BOOLEAN DEFAULT FALSE;
BEGIN
    -- Remove spaces and hyphens
    cleaned_number = REPLACE(REPLACE(card_number, ' ', ''), '-', '');
    
    -- Check if all digits and proper length
    IF (cleaned_number NOT SIMILAR TO '[0-9]{13,19}') THEN
        RETURN FALSE;
    
    -- Apply Luhn algorithm
    position = CHAR_LENGTH(cleaned_number);
    
    WHILE (position > 0) DO
    BEGIN
        current_digit = CAST(SUBSTRING(cleaned_number FROM position FOR 1) AS INTEGER);
        
        IF (alternate) THEN
        BEGIN
            current_digit = current_digit * 2;
            IF (current_digit > 9) THEN
                current_digit = current_digit - 9;
        END
        
        digit_sum = digit_sum + current_digit;
        alternate = NOT alternate;
        position = position - 1;
    END
    
    RETURN (digit_sum % 10 = 0);
END;
```

### **Advanced Functions with Local Subroutines**

#### **Statistical Functions**
```sql
-- Advanced statistical function with local subroutines
CREATE FUNCTION calculate_statistics(
    data_set_id INTEGER,
    stat_type VARCHAR(20)
)
RETURNS DECIMAL(15,4)
AS
DECLARE
    -- Local function for calculating mean
    DECLARE FUNCTION calculate_mean(dataset_id INTEGER)
    RETURNS DECIMAL(15,4)
    AS
    DECLARE
        total_sum DECIMAL(18,4);
        record_count INTEGER;
    BEGIN
        SELECT SUM(value), COUNT(*)
        FROM statistical_data 
        WHERE dataset_id = :dataset_id
        INTO :total_sum, :record_count;
        
        IF (record_count = 0) THEN
            RETURN 0.0;
        ELSE
            RETURN total_sum / record_count;
    END;
    
    -- Local function for calculating standard deviation
    DECLARE FUNCTION calculate_stddev(dataset_id INTEGER)
    RETURNS DECIMAL(15,4)
    AS
    DECLARE
        mean_value DECIMAL(15,4);
        variance_sum DECIMAL(18,4) DEFAULT 0.0;
        record_count INTEGER DEFAULT 0;
        current_value DECIMAL(15,4);
        
        data_cursor CURSOR FOR (
            SELECT value FROM statistical_data 
            WHERE dataset_id = :dataset_id
        );
    BEGIN
        -- Get mean value
        mean_value = calculate_mean(:dataset_id);
        
        -- Calculate variance
        OPEN data_cursor;
        
        WHILE (1 = 1) DO
        BEGIN
            FETCH data_cursor INTO :current_value;
            IF (ROW_COUNT = 0) THEN LEAVE;
            
            variance_sum = variance_sum + POWER(current_value - mean_value, 2);
            record_count = record_count + 1;
        END
        
        CLOSE data_cursor;
        
        IF (record_count <= 1) THEN
            RETURN 0.0;
        ELSE
            RETURN SQRT(variance_sum / (record_count - 1));
    END;
    
    -- Main function variables
    result_value DECIMAL(15,4);
    
BEGIN
    -- Calculate requested statistic
    IF (stat_type = 'MEAN') THEN
        result_value = calculate_mean(:data_set_id);
    ELSE IF (stat_type = 'STDDEV') THEN
        result_value = calculate_stddev(:data_set_id);
    ELSE IF (stat_type = 'VARIANCE') THEN
        result_value = POWER(calculate_stddev(:data_set_id), 2);
    ELSE
        result_value = NULL;
    
    RETURN result_value;
END;
```

### **Hierarchical Schema Functions**

#### **3-Level Schema Qualification**
```sql
-- Financial analysis functions in hierarchical schemas
CREATE FUNCTION business.finance.accounting.calculate_depreciation(
    asset_cost DECIMAL(15,2),
    useful_life INTEGER,
    method VARCHAR(20)
)
RETURNS DECIMAL(15,2)
DETERMINISTIC
AS
BEGIN
    RETURN CASE method
        WHEN 'STRAIGHT_LINE' THEN asset_cost / useful_life
        WHEN 'DOUBLE_DECLINING' THEN (asset_cost * 2) / useful_life
        WHEN 'SUM_OF_YEARS' THEN asset_cost * (useful_life / ((useful_life * (useful_life + 1)) / 2))
        ELSE 0.00
    END;
END;

-- HR calculation functions
CREATE FUNCTION business.hr.payroll.calculate_overtime_rate(
    base_hourly_rate DECIMAL(8,2),
    overtime_hours DECIMAL(6,2)
)
RETURNS DECIMAL(8,2)
DETERMINISTIC
AS
BEGIN
    -- Standard 1.5x overtime rate
    RETURN base_hourly_rate * 1.5;
END;

CREATE FUNCTION business.hr.benefits.calculate_vacation_days(
    hire_date DATE,
    employee_level VARCHAR(20)
)
RETURNS INTEGER
AS
DECLARE
    years_service INTEGER;
    base_days INTEGER;
BEGIN
    years_service = calculate_age(:hire_date); -- Using previously defined function
    
    -- Base vacation days by level
    base_days = CASE employee_level
        WHEN 'EXECUTIVE' THEN 25
        WHEN 'MANAGER' THEN 20
        WHEN 'SENIOR' THEN 15
        ELSE 10
    END;
    
    -- Additional days for service (1 day per 2 years, max 10 additional)
    RETURN base_days + LEAST(years_service / 2, 10);
END;
```

### **External Language Functions**

#### **Java External Functions**
```sql
-- Complex mathematical function implemented in Java
CREATE FUNCTION math.advanced.calculate_matrix_determinant(
    matrix_data BLOB
)
RETURNS DECIMAL(15,8)
DETERMINISTIC
EXTERNAL NAME 'MathUtils.calculateDeterminant'
ENGINE JAVA
AS '
    public static java.math.BigDecimal calculateDeterminant(java.sql.Blob matrixData) 
    throws java.sql.SQLException {
        try {
            // Complex matrix calculation implementation
            byte[] data = matrixData.getBytes(1, (int)matrixData.length());
            Matrix matrix = Matrix.fromBytes(data);
            
            return new java.math.BigDecimal(matrix.determinant());
        } catch (Exception e) {
            throw new java.sql.SQLException("Matrix calculation error: " + e.getMessage());
        }
    }
';

-- Text processing function in Java
CREATE FUNCTION text.processing.extract_keywords(
    text_content VARCHAR(8000),
    max_keywords INTEGER
)
RETURNS VARCHAR(1000)
EXTERNAL NAME 'TextProcessor.extractKeywords'
ENGINE JAVA;
```

#### **C++ External Functions**
```sql
-- High-performance cryptographic function
CREATE FUNCTION security.crypto.hash_password(
    password VARCHAR(200),
    salt VARCHAR(32)
)
RETURNS VARCHAR(128)
DETERMINISTIC
EXTERNAL NAME 'crypto_utils.hash_with_salt'
ENGINE CPP;

-- Image processing function
CREATE FUNCTION media.image.resize_image(
    image_data BLOB,
    new_width INTEGER,
    new_height INTEGER
)
RETURNS BLOB
EXTERNAL NAME 'image_processor.resize'
ENGINE CPP;
```

### **Security-Aware Functions**

#### **SQL SECURITY DEFINER/INVOKER**
```sql
-- Function with definer privileges (secure access)
CREATE FUNCTION security.admin.get_user_permissions(
    user_name VARCHAR(63)
)
RETURNS VARCHAR(1000)
SQL SECURITY DEFINER
AS
DECLARE
    permissions VARCHAR(1000) DEFAULT '';
    permission_cursor CURSOR FOR (
        SELECT privilege_name 
        FROM user_privileges 
        WHERE username = :user_name
    );
    current_privilege VARCHAR(50);
BEGIN
    -- Only admin users can call this function effectively
    -- Function runs with definer privileges to access security tables
    
    OPEN permission_cursor;
    
    WHILE (1 = 1) DO
    BEGIN
        FETCH permission_cursor INTO :current_privilege;
        IF (ROW_COUNT = 0) THEN LEAVE;
        
        IF (permissions = '') THEN
            permissions = current_privilege;
        ELSE
            permissions = permissions || ',' || current_privilege;
    END
    
    CLOSE permission_cursor;
    
    RETURN permissions;
END;

-- Function with invoker privileges (flexible access)
CREATE FUNCTION reporting.user.get_accessible_tables()
RETURNS VARCHAR(2000)
SQL SECURITY INVOKER
AS
DECLARE
    table_list VARCHAR(2000) DEFAULT '';
    table_cursor CURSOR FOR (
        SELECT table_name 
        FROM user_table_privileges 
        WHERE username = CURRENT_USER
          AND privilege_type IN ('SELECT', 'ALL')
    );
    current_table VARCHAR(63);
BEGIN
    -- This function shows only tables the calling user can access
    
    OPEN table_cursor;
    
    WHILE (1 = 1) DO
    BEGIN
        FETCH table_cursor INTO :current_table;
        IF (ROW_COUNT = 0) THEN LEAVE;
        
        IF (table_list = '') THEN
            table_list = current_table;
        ELSE
            table_list = table_list || ',' || current_table;
    END
    
    CLOSE table_cursor;
    
    RETURN table_list;
END;
```

### **Conditional Creation and Modification**

#### **IF NOT EXISTS and OR ALTER**
```sql
-- Safe function creation (no error if exists)
CREATE FUNCTION IF NOT EXISTS utilities.format_currency(
    amount DECIMAL(15,2),
    currency_code VARCHAR(3)
)
RETURNS VARCHAR(50)
DETERMINISTIC
AS
BEGIN
    RETURN currency_code || ' ' || CAST(amount AS VARCHAR(20));
END;

-- Create or modify existing function
CREATE OR ALTER FUNCTION math.geometry.circle_area(
    radius DECIMAL(10,4)
)
RETURNS DECIMAL(15,8)
DETERMINISTIC
AS
BEGIN
    -- Updated with higher precision PI
    RETURN 3.14159265358979323846 * radius * radius;
END;
```

---

## ALTER FUNCTION

Modifies existing function definitions including parameters, return type, security settings, and implementation.

### **ALTER FUNCTION Syntax**
```sql
ALTER FUNCTION [schema_name.]function_name
    [(input_parameters)]
    RETURNS data_type [COLLATE collation_name]
    [DETERMINISTIC | NOT DETERMINISTIC]
    [SQL SECURITY {DEFINER | INVOKER}]
AS
[DECLARE
    -- Local declarations
]
BEGIN
    -- Modified function body
    RETURN expression;
END;
```

### **Partial ALTER FUNCTION Syntax**
```sql
-- Change only specific attributes
ALTER FUNCTION [schema_name.]function_name
    [DETERMINISTIC | NOT DETERMINISTIC]
    [SQL SECURITY {DEFINER | INVOKER}];
```

### **ALTER FUNCTION Examples**

#### **Complete Function Redefinition**
```sql
-- Alter function with improved implementation
ALTER FUNCTION math.financial.compound_interest(
    principal DECIMAL(15,2),
    annual_rate DECIMAL(5,4),
    compounding_periods INTEGER,
    years INTEGER
)
RETURNS DECIMAL(15,2)
DETERMINISTIC
AS
BEGIN
    -- Updated formula for compound interest with periods per year
    RETURN principal * POWER((1 + annual_rate / compounding_periods), 
                           (compounding_periods * years)) - principal;
END;
```

#### **Partial Function Modifications**
```sql
-- Change function to non-deterministic (if it now uses current date/time)
ALTER FUNCTION reporting.get_current_month_sales()
    NOT DETERMINISTIC;

-- Change security mode
ALTER FUNCTION security.validate_user_access()
    SQL SECURITY DEFINER;

-- Update both deterministic flag and security
ALTER FUNCTION business.calculate_dynamic_pricing()
    NOT DETERMINISTIC
    SQL SECURITY INVOKER;
```

---

## RECREATE FUNCTION

Drops and recreates a function in a single atomic operation, preserving dependencies where possible.

### **RECREATE FUNCTION Syntax**
```sql
RECREATE FUNCTION [schema_name.]function_name
    [(input_parameters)]
    RETURNS data_type [COLLATE collation_name]
    [DETERMINISTIC | NOT DETERMINISTIC]
    [SQL SECURITY {DEFINER | INVOKER}]
AS
[DECLARE
    -- Local declarations
]
BEGIN
    -- Function body
    RETURN expression;
END;
```

### **RECREATE FUNCTION Examples**

#### **Complete Function Replacement**
```sql
-- Recreate function with entirely new logic
RECREATE FUNCTION business.pricing.calculate_discount(
    product_category VARCHAR(50),
    quantity INTEGER,
    customer_tier VARCHAR(20)
)
RETURNS DECIMAL(5,2)
DETERMINISTIC
AS
DECLARE
    base_discount DECIMAL(5,2);
    quantity_discount DECIMAL(5,2);
    tier_multiplier DECIMAL(3,2);
BEGIN
    -- New discount calculation algorithm
    base_discount = CASE product_category
        WHEN 'ELECTRONICS' THEN 5.00
        WHEN 'CLOTHING' THEN 10.00
        WHEN 'BOOKS' THEN 15.00
        WHEN 'FOOD' THEN 3.00
        ELSE 2.00
    END;
    
    -- Quantity-based discount
    quantity_discount = CASE
        WHEN quantity >= 100 THEN 5.00
        WHEN quantity >= 50 THEN 3.00
        WHEN quantity >= 10 THEN 1.00
        ELSE 0.00
    END;
    
    -- Customer tier multiplier
    tier_multiplier = CASE customer_tier
        WHEN 'PLATINUM' THEN 1.5
        WHEN 'GOLD' THEN 1.25
        WHEN 'SILVER' THEN 1.1
        ELSE 1.0
    END;
    
    RETURN LEAST((base_discount + quantity_discount) * tier_multiplier, 30.00);
END;
```

---

## DROP FUNCTION

Removes function definitions from the database. Functions can only be dropped if no other database objects depend on them.

### **DROP FUNCTION Syntax**
```sql
DROP FUNCTION [IF EXISTS] [schema_name.]function_name;
```

### **DROP FUNCTION Examples**

#### **Basic Function Removal**
```sql
-- Drop function if it exists
DROP FUNCTION IF EXISTS temp_calculation;

-- Drop function from specific schema
DROP FUNCTION business.finance.old_interest_calc;

-- Drop external function
DROP EXTERNAL FUNCTION legacy_crypto_hash;

-- Drop hierarchical schema function
DROP FUNCTION enterprise.americas.usa.regional_tax_calc;
```

#### **Dependency Management**
```sql
-- Check function dependencies before dropping
SELECT DISTINCT
    dep.RDB$DEPENDENT_NAME as DEPENDENT_OBJECT,
    dep.RDB$DEPENDENT_TYPE as OBJECT_TYPE
FROM RDB$DEPENDENCIES dep
WHERE dep.RDB$DEPENDED_ON_NAME = 'CALCULATE_DISCOUNT'
  AND dep.RDB$DEPENDED_ON_TYPE = 15; -- Function type

-- Drop function after removing dependencies
DROP FUNCTION calculate_discount;
```

---

## Function Usage and Integration

### **Using Functions in SQL Statements**

#### **SELECT Statement Usage**
```sql
-- Function in SELECT list
SELECT 
    customer_id,
    order_total,
    calculate_tax(order_total, 0.0825) as tax_amount,
    order_total + calculate_tax(order_total, 0.0825) as final_total
FROM orders;

-- Function in WHERE clause
SELECT * 
FROM customers 
WHERE calculate_age(birth_date) >= 18;

-- Function in ORDER BY
SELECT product_name, price
FROM products
ORDER BY calculate_discount('ELECTRONICS', 1, 'GOLD') DESC;

-- Function with subqueries
SELECT 
    department,
    AVG(salary) as avg_salary,
    calculate_tax(AVG(salary), 0.25) as avg_tax
FROM employees
GROUP BY department
HAVING AVG(salary) > 50000;
```

#### **Complex Expression Usage**
```sql
-- Functions in CASE expressions
SELECT 
    customer_id,
    CASE 
        WHEN calculate_age(birth_date) < 25 THEN 'YOUNG'
        WHEN calculate_age(birth_date) < 65 THEN 'ADULT'
        ELSE 'SENIOR'
    END as age_category,
    CASE
        WHEN is_valid_email(email) THEN 'VALID'
        ELSE 'INVALID'
    END as email_status
FROM customers;

-- Functions in aggregate expressions
SELECT 
    product_category,
    COUNT(*) as product_count,
    AVG(calculate_discount(product_category, 1, 'STANDARD')) as avg_discount,
    SUM(price * (1 - calculate_discount(product_category, 1, 'STANDARD')/100)) as discounted_total
FROM products
GROUP BY product_category;
```

### **Function Composition and Nesting**

#### **Nested Function Calls**
```sql
-- Multiple function composition
SELECT 
    customer_name,
    format_phone_number(phone) as formatted_phone,
    calculate_age(birth_date) as age,
    calculate_discount(
        'ELECTRONICS', 
        5, 
        CASE 
            WHEN calculate_age(birth_date) >= 65 THEN 'SENIOR'
            ELSE 'STANDARD'
        END
    ) as applicable_discount
FROM customers;

-- Complex business logic with nested functions
SELECT 
    order_id,
    customer_id,
    base_amount,
    calculate_tax(
        base_amount - (base_amount * calculate_discount(
            product_category, 
            quantity, 
            customer_tier
        ) / 100),
        get_tax_rate(billing_state)
    ) as final_tax
FROM order_details;
```

### **Functions in Views and Computed Columns**

#### **View Definitions with Functions**
```sql
-- Create view using multiple functions
CREATE VIEW customer_analysis AS
SELECT 
    c.customer_id,
    c.customer_name,
    calculate_age(c.birth_date) as age,
    CASE 
        WHEN is_valid_email(c.email) THEN c.email
        ELSE 'INVALID EMAIL'
    END as validated_email,
    calculate_customer_discount(c.customer_type, 1000.00, c.loyalty_years) as discount_rate,
    business_days_between(c.last_order_date, CURRENT_DATE) as days_since_last_order
FROM customers c;

-- Computed columns using functions
ALTER TABLE products 
ADD COLUMN discounted_price COMPUTED BY (
    price * (1 - calculate_discount(category, 1, 'STANDARD') / 100)
);
```

---

## System Catalog Integration

ScratchBird stores function definitions in the RDB$FUNCTIONS system table and related metadata tables.

### **Querying Function Information**

#### **List All Functions**
```sql
-- Show all user-defined functions
SELECT 
    RDB$FUNCTION_NAME as FUNCTION_NAME,
    RDB$SCHEMA_NAME as SCHEMA_NAME,
    RDB$OWNER_NAME as OWNER,
    RDB$RETURN_ARGUMENT as RETURN_TYPE,
    CASE RDB$DETERMINISTIC_FLAG
        WHEN 1 THEN 'DETERMINISTIC'
        WHEN 0 THEN 'NOT DETERMINISTIC'
        ELSE 'UNKNOWN'
    END as DETERMINISTIC,
    CASE RDB$SQL_SECURITY
        WHEN 0 THEN 'INVOKER'
        WHEN 1 THEN 'DEFINER'
        ELSE 'INHERITED'
    END as SQL_SECURITY,
    RDB$DESCRIPTION as DESCRIPTION,
    CASE WHEN RDB$FUNCTION_SOURCE IS NOT NULL THEN 'PSQL' ELSE 'EXTERNAL' END as IMPLEMENTATION
FROM RDB$FUNCTIONS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY RDB$SCHEMA_NAME, RDB$FUNCTION_NAME;
```

#### **Function Parameters and Return Types**
```sql
-- List function parameters and return information
SELECT 
    f.RDB$FUNCTION_NAME,
    fa.RDB$ARGUMENT_NAME,
    fa.RDB$ARGUMENT_POSITION,
    CASE fa.RDB$ARGUMENT_MECHANISM
        WHEN 0 THEN 'INPUT'
        WHEN 1 THEN 'RETURN'
        ELSE 'UNKNOWN'
    END as PARAMETER_TYPE,
    fld.RDB$FIELD_TYPE,
    fld.RDB$FIELD_LENGTH,
    fld.RDB$FIELD_SCALE,
    fld.RDB$NULL_FLAG,
    fa.RDB$DEFAULT_SOURCE as DEFAULT_VALUE
FROM RDB$FUNCTIONS f
JOIN RDB$FUNCTION_ARGUMENTS fa ON f.RDB$FUNCTION_NAME = fa.RDB$FUNCTION_NAME
JOIN RDB$FIELDS fld ON fa.RDB$FIELD_SOURCE = fld.RDB$FIELD_NAME
WHERE f.RDB$SYSTEM_FLAG = 0 OR f.RDB$SYSTEM_FLAG IS NULL
ORDER BY f.RDB$FUNCTION_NAME, fa.RDB$ARGUMENT_POSITION;
```

#### **Function Dependencies**
```sql
-- Find function dependencies
SELECT 
    f.RDB$FUNCTION_NAME as FUNCTION_NAME,
    dep.RDB$DEPENDED_ON_NAME as DEPENDS_ON,
    CASE dep.RDB$DEPENDED_ON_TYPE
        WHEN 0 THEN 'TABLE'
        WHEN 1 THEN 'VIEW'
        WHEN 2 THEN 'TRIGGER'
        WHEN 5 THEN 'PROCEDURE'
        WHEN 15 THEN 'FUNCTION'
        ELSE 'OTHER'
    END as DEPENDENCY_TYPE
FROM RDB$FUNCTIONS f
JOIN RDB$DEPENDENCIES dep ON f.RDB$FUNCTION_NAME = dep.RDB$DEPENDENT_NAME
WHERE f.RDB$SYSTEM_FLAG = 0 OR f.RDB$SYSTEM_FLAG IS NULL
  AND dep.RDB$DEPENDENT_TYPE = 15  -- Function type
ORDER BY f.RDB$FUNCTION_NAME, dep.RDB$DEPENDED_ON_NAME;
```

### **Function Source Code and Metadata**
```sql
-- View function source code and properties
SELECT 
    RDB$FUNCTION_NAME,
    RDB$FUNCTION_SOURCE,
    RDB$DETERMINISTIC_FLAG,
    RDB$SQL_SECURITY,
    RDB$ENGINE_NAME,
    RDB$ENTRYPOINT,
    RDB$MODULE_NAME
FROM RDB$FUNCTIONS
WHERE RDB$FUNCTION_NAME = 'CALCULATE_TAX'
  AND (RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL);
```

### **Schema-Aware Function Queries**

#### **Functions by Schema Hierarchy**
```sql
-- List functions grouped by schema hierarchy
SELECT 
    COALESCE(RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME,
    COUNT(*) as FUNCTION_COUNT,
    COUNT(CASE WHEN RDB$FUNCTION_SOURCE IS NOT NULL THEN 1 END) as PSQL_FUNCTIONS,
    COUNT(CASE WHEN RDB$FUNCTION_SOURCE IS NULL THEN 1 END) as EXTERNAL_FUNCTIONS,
    COUNT(CASE WHEN RDB$DETERMINISTIC_FLAG = 1 THEN 1 END) as DETERMINISTIC_FUNCTIONS
FROM RDB$FUNCTIONS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
GROUP BY RDB$SCHEMA_NAME
ORDER BY SCHEMA_NAME;
```

---

## Advanced Function Features

### **Deterministic Functions and Optimization**

#### **Query Optimizer Benefits**
```sql
-- Deterministic function - optimizer can cache results
CREATE FUNCTION math.constants.pi()
RETURNS DECIMAL(15,14)
DETERMINISTIC
AS
BEGIN
    RETURN 3.14159265358979;
END;

-- Non-deterministic function - recalculated each time
CREATE FUNCTION utilities.current_timestamp_string()
RETURNS VARCHAR(30)
NOT DETERMINISTIC
AS
BEGIN
    RETURN CAST(CURRENT_TIMESTAMP AS VARCHAR(30));
END;

-- Usage showing optimization benefits
SELECT 
    product_id,
    radius,
    -- This calculation uses cached PI value for deterministic function
    math.constants.pi() * radius * radius as area,
    -- This gets recalculated for each row
    utilities.current_timestamp_string() as calculated_at
FROM circular_products;
```

#### **Complex Deterministic Calculations**
```sql
-- Deterministic mathematical constants and calculations
CREATE FUNCTION math.constants.euler_number()
RETURNS DECIMAL(15,14)
DETERMINISTIC
AS
BEGIN
    RETURN 2.71828182845905;
END;

CREATE FUNCTION math.trigonometry.sin_degrees(
    degrees DECIMAL(10,4)
)
RETURNS DECIMAL(15,10)
DETERMINISTIC
AS
DECLARE
    radians DECIMAL(15,10);
BEGIN
    radians = degrees * math.constants.pi() / 180.0;
    RETURN SIN(radians);
END;
```

### **Exception Handling in Functions**

#### **Robust Error Handling**
```sql
-- Function with comprehensive error handling
CREATE FUNCTION financial.calculations.safe_divide(
    dividend DECIMAL(15,4),
    divisor DECIMAL(15,4)
)
RETURNS DECIMAL(15,4)
AS
DECLARE
    division_by_zero EXCEPTION 'Division by zero not allowed';
    invalid_input EXCEPTION 'Invalid input parameters';
BEGIN
    -- Validate inputs
    IF (dividend IS NULL OR divisor IS NULL) THEN
        EXCEPTION invalid_input;
    
    -- Check for division by zero
    IF (divisor = 0) THEN
        EXCEPTION division_by_zero;
    
    RETURN dividend / divisor;
    
    WHEN EXCEPTION division_by_zero DO
        RETURN NULL; -- Return NULL for division by zero
    
    WHEN EXCEPTION invalid_input DO
        RETURN NULL; -- Return NULL for invalid inputs
    
    WHEN ANY DO
        RETURN NULL; -- Return NULL for any other error
END;

-- Function with business logic validation
CREATE FUNCTION business.inventory.calculate_reorder_point(
    average_daily_usage DECIMAL(10,2),
    lead_time_days INTEGER,
    safety_stock_days INTEGER
)
RETURNS INTEGER 
AS
DECLARE
    invalid_parameters EXCEPTION 'Invalid reorder calculation parameters';
    result INTEGER;
BEGIN
    -- Validate all parameters are positive
    IF (average_daily_usage <= 0 OR lead_time_days <= 0 OR safety_stock_days < 0) THEN
        EXCEPTION invalid_parameters;
    
    -- Calculate reorder point
    result = CEILING(average_daily_usage * (lead_time_days + safety_stock_days));
    
    -- Ensure minimum reorder point
    IF (result < 1) THEN
        result = 1;
    
    RETURN result;
    
    WHEN EXCEPTION invalid_parameters DO
    BEGIN
        -- Log error and return default value
        INSERT INTO calculation_errors (function_name, error_message, error_time)
        VALUES ('calculate_reorder_point', 'Invalid parameters provided', CURRENT_TIMESTAMP);
        
        RETURN 10; -- Default minimum reorder point
    END
END;
```

### **Performance Optimization Techniques**

#### **Efficient Function Design**
```sql
-- Optimized lookup function using local variables
CREATE FUNCTION business.pricing.get_product_price(
    product_code VARCHAR(20),
    price_type VARCHAR(10)
)
RETURNS DECIMAL(10,2)
AS
DECLARE
    base_price DECIMAL(10,2);
    discount_multiplier DECIMAL(5,4) DEFAULT 1.0000;
BEGIN
    -- Single query to get base price
    SELECT standard_price 
    FROM products 
    WHERE code = :product_code
    INTO :base_price;
    
    -- Quick price type calculation without additional queries
    discount_multiplier = CASE price_type
        WHEN 'RETAIL' THEN 1.0000
        WHEN 'WHOLESALE' THEN 0.8500
        WHEN 'EMPLOYEE' THEN 0.7500
        WHEN 'CLEARANCE' THEN 0.5000
        ELSE 1.0000
    END;
    
    RETURN base_price * discount_multiplier;
END;

-- Caching function for expensive calculations
CREATE FUNCTION analytics.metrics.get_cached_metric(
    metric_name VARCHAR(50),
    calculation_date DATE
)
RETURNS DECIMAL(15,4)
AS
DECLARE
    cached_value DECIMAL(15,4);
    cache_age INTEGER;
BEGIN
    -- Check cache first
    SELECT metric_value, DATEDIFF(DAY, cached_date, CURRENT_DATE)
    FROM metric_cache 
    WHERE metric_name = :metric_name 
      AND cached_date = :calculation_date
    INTO :cached_value, :cache_age;
    
    -- Return cached value if found and fresh
    IF (cached_value IS NOT NULL AND cache_age = 0) THEN
        RETURN cached_value;
    
    -- Calculate new value (expensive operation would go here)
    cached_value = 0.0; -- Placeholder for complex calculation
    
    -- Update cache
    UPDATE OR INSERT INTO metric_cache (metric_name, cached_date, metric_value, cached_at)
    VALUES (:metric_name, :calculation_date, :cached_value, CURRENT_TIMESTAMP);
    
    RETURN cached_value;
END;
```

---

## Error Handling and Troubleshooting

### **Common Function Errors**

#### **Function Creation Errors**
```sql
-- Error: Function already exists
CREATE FUNCTION duplicate_name() RETURNS INTEGER AS BEGIN RETURN 1; END;
-- Solution: Use CREATE OR ALTER or IF NOT EXISTS

-- Error: Invalid return type
CREATE FUNCTION test() RETURNS INVALID_TYPE AS BEGIN RETURN 1; END;
-- Solution: Use valid ScratchBird data types

-- Error: Missing RETURN statement
CREATE FUNCTION test() RETURNS INTEGER AS BEGIN END;
-- Solution: Add RETURN statement in all code paths
```

#### **Runtime Execution Errors**
```sql
-- Error: Function returns NULL when NOT NULL expected
CREATE FUNCTION get_required_value() RETURNS INTEGER NOT NULL AS 
BEGIN RETURN NULL; END;
-- Solution: Ensure all code paths return non-null values

-- Error: Type mismatch in RETURN
CREATE FUNCTION get_number() RETURNS INTEGER AS 
BEGIN RETURN 'text'; END;
-- Solution: Return value matching declared type
```

### **Debugging Functions**

#### **Function Testing and Validation**
```sql
-- Test function with various inputs
SELECT 
    calculate_tax(1000, 0.0825) as normal_case,
    calculate_tax(0, 0.0825) as zero_amount,
    calculate_tax(1000, 0) as zero_rate,
    calculate_tax(NULL, 0.0825) as null_amount,
    calculate_tax(1000, NULL) as null_rate;

-- Test function error handling
SELECT 
    CASE
        WHEN safe_divide(10, 2) = 5 THEN 'PASS'
        ELSE 'FAIL'
    END as normal_division,
    CASE
        WHEN safe_divide(10, 0) IS NULL THEN 'PASS'
        ELSE 'FAIL'
    END as division_by_zero,
    CASE
        WHEN safe_divide(NULL, 2) IS NULL THEN 'PASS'
        ELSE 'FAIL'
    END as null_input;
```

#### **Performance Analysis**
```sql
-- Monitor function execution performance
SELECT 
    f.RDB$FUNCTION_NAME,
    fs.MON$STAT_ID,
    fs.MON$STAT_TIME,
    fs.MON$MEMORY_USED,
    fs.MON$MEMORY_ALLOCATED
FROM RDB$FUNCTIONS f
JOIN MON$FUNCTION_STATS fs ON f.RDB$FUNCTION_NAME = fs.MON$FUNCTION_NAME
WHERE f.RDB$SYSTEM_FLAG = 0
ORDER BY fs.MON$STAT_TIME DESC;
```

---

## Best Practices

### **Function Design Guidelines**

1. **Single Responsibility**: Each function should perform one specific calculation or operation
2. **Deterministic Marking**: Mark functions as DETERMINISTIC when appropriate for optimization
3. **Input Validation**: Always validate input parameters before processing
4. **Error Handling**: Implement comprehensive exception handling
5. **Return Path Coverage**: Ensure all code paths return appropriate values
6. **Performance**: Avoid complex queries within functions when possible

### **Recommended Function Patterns**

#### **Input Validation Pattern**
```sql
CREATE FUNCTION utilities.validation.validate_and_process(
    input_value DECIMAL(10,2),
    operation_type VARCHAR(20)
)
RETURNS DECIMAL(10,2)
AS
DECLARE
    invalid_input EXCEPTION 'Invalid input parameters';
BEGIN
    -- Validate inputs first
    IF (input_value IS NULL OR input_value < 0) THEN
        EXCEPTION invalid_input;
    
    IF (operation_type IS NULL OR operation_type = '') THEN
        EXCEPTION invalid_input;
    
    -- Process valid inputs
    RETURN CASE operation_type
        WHEN 'DOUBLE' THEN input_value * 2
        WHEN 'SQUARE' THEN input_value * input_value
        WHEN 'SQRT' THEN SQRT(input_value)
        ELSE input_value
    END;
    
    WHEN EXCEPTION invalid_input DO
        RETURN NULL;
END;
```

#### **Lookup Function Pattern**
```sql
CREATE FUNCTION business.lookups.get_tax_rate(
    state_code CHAR(2),
    tax_type VARCHAR(20)
)
RETURNS DECIMAL(6,4)
DETERMINISTIC
AS
DECLARE
    tax_rate DECIMAL(6,4);
BEGIN
    SELECT rate 
    FROM tax_rates 
    WHERE state = :state_code 
      AND type = :tax_type
      AND effective_date <= CURRENT_DATE
      AND (expiration_date IS NULL OR expiration_date > CURRENT_DATE)
    INTO :tax_rate;
    
    RETURN COALESCE(tax_rate, 0.0000);
END;
```

---

## Migration and Integration

### **Function Migration Strategies**

#### **From Other Database Systems**
```sql
-- Convert MySQL function to ScratchBird
-- MySQL: CREATE FUNCTION calc_bonus(salary DECIMAL(10,2)) RETURNS DECIMAL(10,2) READS SQL DATA
-- ScratchBird equivalent:
CREATE FUNCTION calc_bonus(
    salary DECIMAL(10,2)
)
RETURNS DECIMAL(10,2)
SQL SECURITY INVOKER
AS
DECLARE
    performance_multiplier DECIMAL(4,2);
BEGIN
    SELECT bonus_multiplier 
    FROM employee_performance 
    WHERE employee_salary = :salary
    INTO :performance_multiplier;
    
    RETURN salary * COALESCE(performance_multiplier, 0.05);
END;
```

### **Deployment Best Practices**

#### **Function Deployment Scripts**
```sql
-- Safe deployment with conditional creation
CREATE FUNCTION IF NOT EXISTS business.utils.format_currency(
    amount DECIMAL(15,2),
    currency_code VARCHAR(3)
)
RETURNS VARCHAR(50)
DETERMINISTIC
AS
BEGIN
    RETURN CASE currency_code
        WHEN 'USD' THEN '$' || CAST(amount AS VARCHAR(20))
        WHEN 'EUR' THEN '€' || CAST(amount AS VARCHAR(20))
        WHEN 'GBP' THEN '£' || CAST(amount AS VARCHAR(20))
        ELSE currency_code || ' ' || CAST(amount AS VARCHAR(20))
    END;
END;

-- Version-aware function updates
CREATE OR ALTER FUNCTION business.calculations.calculate_shipping(
    weight DECIMAL(8,2),
    distance INTEGER,
    service_level VARCHAR(20)
)
RETURNS DECIMAL(10,2)
DETERMINISTIC
AS
BEGIN
    -- Version 2.0 shipping calculation
    RETURN CASE service_level
        WHEN 'OVERNIGHT' THEN weight * 2.50 + distance * 0.15
        WHEN 'EXPRESS' THEN weight * 1.75 + distance * 0.10
        WHEN 'STANDARD' THEN weight * 1.25 + distance * 0.05
        ELSE weight * 1.00 + distance * 0.03
    END;
END;
```

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:3257-3378`
- **Classes**: `function_clause`, `psql_function_clause`, `external_function_clause`
- **Functionality**: Parsing CREATE/ALTER/DROP FUNCTION syntax with return types

#### **DDL Node Classes**
- **File**: `src/dsql/DdlNodes.h:391-539`
- **Classes**:
  - `CreateAlterFunctionNode` (lines 391-467): Function creation and modification
  - `DropFunctionNode` (lines 506-535): Function removal with dependency checking
  - `RecreateFunctionNode` (lines 538-539): Atomic drop-and-recreate operations

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:435-455`
- **Tables**:
  - `RDB$FUNCTIONS` - Stores function definitions
  - `RDB$FUNCTION_ARGUMENTS` - Stores function parameter information
- **Fields**:
  - `f_fun_name`: Function name
  - `f_fun_source`: PSQL source code
  - `f_fun_security`: SQL security mode (DEFINER/INVOKER)
  - `f_fun_deterministic`: Deterministic flag for optimization

### **Core Function Operations**

#### **CreateAlterFunctionNode Methods**
- Handles both CREATE and ALTER operations (create/alter flags)
- Return type validation and checking
- PSQL source code compilation and storage
- External function registration
- Deterministic flag management
- SQL security mode configuration

#### **Return Type Management**
- Single return type specification with ParameterClause
- Type compatibility checking with function body
- Collation support for character return types
- Integration with ScratchBird's extended type system

#### **PSQL Execution Engine Integration**
- Function call resolution and parameter binding
- Return value validation and type conversion
- Exception handling and propagation
- Deterministic result caching for optimization

### **Storage Structures**

Functions are stored in the RDB$FUNCTIONS system table with:
- **Identity**: Name, schema, owner information
- **Implementation**: PSQL source code or external module reference
- **Return Specification**: Return type, collation, and constraints
- **Optimization**: Deterministic flag for query optimizer
- **Security**: SQL security mode and ownership details
- **Parameters**: Linked to RDB$FUNCTION_ARGUMENTS for input parameters

---

## Administrative Operations

### **Function Maintenance**

#### **Function Performance Monitoring**
```sql
-- Monitor function execution patterns
CREATE VIEW function_performance AS
SELECT 
    f.RDB$FUNCTION_NAME,
    f.RDB$SCHEMA_NAME,
    COUNT(fs.MON$STAT_ID) as EXECUTION_COUNT,
    AVG(fs.MON$STAT_TIME) as AVG_EXECUTION_TIME,
    MAX(fs.MON$STAT_TIME) as MAX_EXECUTION_TIME,
    SUM(fs.MON$MEMORY_USED) as TOTAL_MEMORY_USED,
    f.RDB$DETERMINISTIC_FLAG as IS_DETERMINISTIC
FROM RDB$FUNCTIONS f
LEFT JOIN MON$FUNCTION_STATS fs ON f.RDB$FUNCTION_NAME = fs.MON$FUNCTION_NAME
WHERE f.RDB$SYSTEM_FLAG = 0 OR f.RDB$SYSTEM_FLAG IS NULL
GROUP BY f.RDB$FUNCTION_NAME, f.RDB$SCHEMA_NAME, f.RDB$DETERMINISTIC_FLAG
ORDER BY EXECUTION_COUNT DESC;
```

#### **Function Dependency Analysis**
```sql
-- Comprehensive function dependency analysis
CREATE PROCEDURE admin.analyze_function_dependencies(
    IN function_name VARCHAR(63)
)
RETURNS (
    OUT dependency_report BLOB
)
AS
DECLARE
    report_text VARCHAR(8000) DEFAULT '';
    dep_name VARCHAR(63);
    dep_type VARCHAR(20);
    
    dep_cursor CURSOR FOR (
        SELECT 
            dep.RDB$DEPENDED_ON_NAME,
            CASE dep.RDB$DEPENDED_ON_TYPE
                WHEN 0 THEN 'TABLE'
                WHEN 1 THEN 'VIEW'
                WHEN 2 THEN 'TRIGGER'
                WHEN 5 THEN 'PROCEDURE'
                WHEN 15 THEN 'FUNCTION'
                ELSE 'OTHER'
            END
        FROM RDB$DEPENDENCIES dep
        WHERE dep.RDB$DEPENDENT_NAME = :function_name
          AND dep.RDB$DEPENDENT_TYPE = 15
    );
BEGIN
    report_text = 'Dependency Report for Function: ' || function_name || ASCII_CHAR(13) || ASCII_CHAR(10);
    report_text = report_text || 'Generated: ' || CURRENT_TIMESTAMP || ASCII_CHAR(13) || ASCII_CHAR(10) || ASCII_CHAR(13) || ASCII_CHAR(10);
    
    OPEN dep_cursor;
    
    WHILE (1 = 1) DO
    BEGIN
        FETCH dep_cursor INTO :dep_name, :dep_type;
        IF (ROW_COUNT = 0) THEN LEAVE;
        
        report_text = report_text || dep_type || ': ' || dep_name || ASCII_CHAR(13) || ASCII_CHAR(10);
    END
    
    CLOSE dep_cursor;
    
    dependency_report = CAST(report_text AS BLOB SUB_TYPE TEXT);
END;
```

---

