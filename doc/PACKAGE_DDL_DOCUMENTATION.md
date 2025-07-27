# ScratchBird PACKAGE - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

PACKAGE objects in ScratchBird provide a powerful mechanism for organizing and grouping related procedural objects (functions and procedures) into logical units. Packages offer namespace management, modular design capabilities, and enhanced code organization for complex database applications. They consist of two components: the package specification (header) that declares public interfaces, and the optional package body that implements the declared procedures and functions.

### Key Features and Capabilities

- **Namespace Management**: Organize related procedures and functions under a single package name
- **Public Interface Declaration**: Define public APIs through package specifications
- **Private Implementation**: Hide implementation details in package bodies
- **Modular Design**: Group logically related functionality for better code organization
- **Security Integration**: Support for SQL security modes (DEFINER/INVOKER)
- **Version Control**: Independent versioning of package specifications and bodies
- **Dependency Management**: Automatic dependency tracking between packages and procedures/functions
- **Schema Qualification**: Full support for hierarchical schemas with 3-level qualification
- **Performance Optimization**: Efficient compilation and execution of package procedures/functions

### ScratchBird-Specific Enhancements

1. **Two-Part Package System**: Separate package header (specification) and body for interface/implementation separation
2. **SQL Security Support**: DEFINER and INVOKER security modes for package execution
3. **Hierarchical Schema Support**: Packages support 3-level qualified names (catalog.schema.package)
4. **Enhanced System Catalog**: Comprehensive RDB$PACKAGES table with metadata tracking
5. **DDL Trigger Integration**: Support for DDL triggers on package operations
6. **Flexible Recreation**: Support for CREATE OR ALTER and RECREATE operations
7. **Advanced Permission Model**: Granular EXECUTE permissions on packages
8. **Cross-Package Dependencies**: Sophisticated dependency tracking between packages

---

## DDL Syntax Reference

### CREATE PACKAGE

Creates a new package specification that declares the public interface of procedures and functions.

#### **Basic Syntax**
```sql
CREATE PACKAGE [IF NOT EXISTS] [schema_name.]package_name
    [SQL SECURITY {DEFINER | INVOKER}]
AS
BEGIN
    -- Function and procedure declarations
END;
```

#### **Complete Syntax with All Options**
```sql
CREATE PACKAGE [IF NOT EXISTS] [[catalog.]schema.]package_name
    [SQL SECURITY {DEFINER | INVOKER}]
AS
BEGIN
    [FUNCTION function_name[(parameter_list)] RETURNS return_type [DETERMINISTIC];]
    [PROCEDURE procedure_name[(parameter_list)];]
    [-- Multiple function and procedure declarations --]
END;
```

#### **Parameters**

- **IF NOT EXISTS**: Skip creation if package already exists (no error)
- **schema_name**: Optional schema qualification (supports hierarchical schemas)
- **package_name**: Package identifier (63 characters max)
- **SQL SECURITY**: Execution security context (DEFINER or INVOKER)
- **FUNCTION/PROCEDURE declarations**: Public interface definitions

---

## CREATE PACKAGE Examples

### **Basic Package Creation**

#### **Simple Function Package**
```sql
-- Create package for mathematical utilities
CREATE PACKAGE math_utils AS
BEGIN
    FUNCTION square(input_value DOUBLE PRECISION) RETURNS DOUBLE PRECISION DETERMINISTIC;
    FUNCTION cube(input_value DOUBLE PRECISION) RETURNS DOUBLE PRECISION DETERMINISTIC;
    FUNCTION factorial(n INTEGER) RETURNS BIGINT;
END;
```

#### **Mixed Function and Procedure Package**
```sql
-- Create package for string utilities
CREATE PACKAGE string_utils AS
BEGIN
    -- String manipulation functions
    FUNCTION reverse_string(input_str VARCHAR(1000)) RETURNS VARCHAR(1000);
    FUNCTION word_count(input_str VARCHAR(8000)) RETURNS INTEGER;
    
    -- String processing procedures
    PROCEDURE log_string_operation(operation_name VARCHAR(100), input_str VARCHAR(1000));
    PROCEDURE clear_string_cache;
END;
```

### **Advanced Package Examples**

#### **Business Logic Package with Security**
```sql
-- Create package for financial calculations with DEFINER security
CREATE PACKAGE finance_calculations
    SQL SECURITY DEFINER
AS
BEGIN
    -- Interest calculation functions
    FUNCTION simple_interest(principal DECIMAL(15,2), rate DECIMAL(5,4), time_years INTEGER) 
        RETURNS DECIMAL(15,2) DETERMINISTIC;
    
    FUNCTION compound_interest(principal DECIMAL(15,2), rate DECIMAL(5,4), 
                              periods INTEGER, time_years INTEGER) 
        RETURNS DECIMAL(15,2) DETERMINISTIC;
    
    -- Loan processing procedures
    PROCEDURE calculate_loan_payment(loan_amount DECIMAL(15,2), 
                                   annual_rate DECIMAL(5,4), 
                                   loan_term_months INTEGER,
                                   OUT monthly_payment DECIMAL(15,2));
    
    PROCEDURE process_loan_application(customer_id INTEGER, loan_amount DECIMAL(15,2));
END;
```

#### **Data Processing Package**
```sql
-- Create package for data validation and processing
CREATE PACKAGE data_processor
    SQL SECURITY INVOKER
AS
BEGIN
    -- Validation functions
    FUNCTION validate_email(email_address VARCHAR(255)) RETURNS BOOLEAN;
    FUNCTION validate_phone(phone_number VARCHAR(20)) RETURNS BOOLEAN;
    FUNCTION sanitize_input(user_input VARCHAR(4000)) RETURNS VARCHAR(4000);
    
    -- Data processing procedures
    PROCEDURE batch_import_customers(import_batch_id INTEGER);
    PROCEDURE validate_customer_data(customer_id INTEGER, OUT validation_errors VARCHAR(1000));
    PROCEDURE generate_data_report(report_type VARCHAR(50), 
                                 start_date DATE, 
                                 end_date DATE);
END;
```

### **Hierarchical Schema Packages**

#### **3-Level Schema Qualification**
```sql
-- Enterprise-level package organization
CREATE PACKAGE enterprise.finance.accounting_utils AS
BEGIN
    FUNCTION calculate_depreciation(asset_cost DECIMAL(15,2), 
                                  salvage_value DECIMAL(15,2), 
                                  useful_life INTEGER) 
        RETURNS DECIMAL(15,2);
    
    PROCEDURE post_journal_entry(account_code VARCHAR(20), 
                                debit_amount DECIMAL(15,2), 
                                credit_amount DECIMAL(15,2));
END;

-- Regional package organization
CREATE PACKAGE enterprise.sales.region_management AS
BEGIN
    FUNCTION get_regional_quota(region_id INTEGER, fiscal_year INTEGER) 
        RETURNS DECIMAL(15,2);
    
    PROCEDURE update_sales_targets(region_id INTEGER, 
                                 new_target DECIMAL(15,2));
END;

-- Department-specific packages
CREATE PACKAGE enterprise.hr.employee_management AS
BEGIN
    FUNCTION calculate_overtime_pay(employee_id INTEGER, 
                                  regular_hours DECIMAL(5,2), 
                                  overtime_hours DECIMAL(5,2)) 
        RETURNS DECIMAL(10,2);
    
    PROCEDURE process_payroll(pay_period_start DATE, pay_period_end DATE);
END;
```

### **Conditional Package Creation**

#### **IF NOT EXISTS Usage**
```sql
-- Safe package creation for deployment scripts
CREATE PACKAGE IF NOT EXISTS application_utils AS
BEGIN
    FUNCTION get_application_version() RETURNS VARCHAR(20);
    FUNCTION get_database_version() RETURNS VARCHAR(20);
    PROCEDURE log_application_event(event_type VARCHAR(50), event_message VARCHAR(500));
END;

-- Schema-qualified conditional creation
CREATE PACKAGE IF NOT EXISTS business.core.data_access AS
BEGIN
    FUNCTION get_customer_by_id(customer_id INTEGER) RETURNS VARCHAR(1000);
    PROCEDURE save_customer_data(customer_data VARCHAR(4000));
END;
```

---

## CREATE PACKAGE BODY

Creates the implementation body for a previously declared package specification.

### **CREATE PACKAGE BODY Syntax**
```sql
CREATE PACKAGE BODY [IF NOT EXISTS] [schema_name.]package_name
AS
BEGIN
    -- Function and procedure implementations
    [-- Private functions and procedures (not declared in package spec) --]
    [-- Public function and procedure implementations --]
END;
```

### **CREATE PACKAGE BODY Examples**

#### **Mathematical Utilities Implementation**
```sql
-- Implement math_utils package body
CREATE PACKAGE BODY math_utils AS
BEGIN
    -- Public function implementations
    FUNCTION square(input_value DOUBLE PRECISION) RETURNS DOUBLE PRECISION
        DETERMINISTIC
    AS
    BEGIN
        RETURN input_value * input_value;
    END
    
    FUNCTION cube(input_value DOUBLE PRECISION) RETURNS DOUBLE PRECISION
        DETERMINISTIC
    AS
    BEGIN
        RETURN input_value * input_value * input_value;
    END
    
    FUNCTION factorial(n INTEGER) RETURNS BIGINT
    AS
        DECLARE result BIGINT = 1;
        DECLARE counter INTEGER = 1;
    BEGIN
        IF (n < 0) THEN
            EXCEPTION custom_exception 'Factorial not defined for negative numbers';
        
        WHILE (counter <= n) DO
        BEGIN
            result = result * counter;
            counter = counter + 1;
        END
        
        RETURN result;
    END
END;
```

#### **String Utilities with Private Functions**
```sql
-- Implement string utilities with private helper functions
CREATE PACKAGE BODY string_utils AS
BEGIN
    -- Private helper function (not declared in package spec)
    FUNCTION is_whitespace_char(c CHAR(1)) RETURNS BOOLEAN
    AS
    BEGIN
        RETURN (c = ' ' OR c = CHR(9) OR c = CHR(10) OR c = CHR(13));
    END
    
    -- Private helper function for logging
    FUNCTION get_timestamp_string() RETURNS VARCHAR(25)
    AS
    BEGIN
        RETURN CAST(CURRENT_TIMESTAMP AS VARCHAR(25));
    END
    
    -- Public function implementations
    FUNCTION reverse_string(input_str VARCHAR(1000)) RETURNS VARCHAR(1000)
    AS
        DECLARE result VARCHAR(1000) = '';
        DECLARE i INTEGER;
        DECLARE str_length INTEGER;
    BEGIN
        str_length = CHAR_LENGTH(input_str);
        i = str_length;
        
        WHILE (i > 0) DO
        BEGIN
            result = result || SUBSTRING(input_str FROM i FOR 1);
            i = i - 1;
        END
        
        RETURN result;
    END
    
    FUNCTION word_count(input_str VARCHAR(8000)) RETURNS INTEGER
    AS
        DECLARE word_count INTEGER = 0;
        DECLARE i INTEGER = 1;
        DECLARE str_length INTEGER;
        DECLARE in_word BOOLEAN = FALSE;
        DECLARE current_char CHAR(1);
    BEGIN
        str_length = CHAR_LENGTH(input_str);
        
        WHILE (i <= str_length) DO
        BEGIN
            current_char = SUBSTRING(input_str FROM i FOR 1);
            
            IF (is_whitespace_char(current_char)) THEN
            BEGIN
                in_word = FALSE;
            END
            ELSE
            BEGIN
                IF (NOT in_word) THEN
                BEGIN
                    word_count = word_count + 1;
                    in_word = TRUE;
                END
            END
            
            i = i + 1;
        END
        
        RETURN word_count;
    END
    
    -- Public procedure implementations
    PROCEDURE log_string_operation(operation_name VARCHAR(100), input_str VARCHAR(1000))
    AS
    BEGIN
        INSERT INTO string_operation_log (timestamp_value, operation_name, input_length)
        VALUES (get_timestamp_string(), operation_name, CHAR_LENGTH(input_str));
    END
    
    PROCEDURE clear_string_cache
    AS
    BEGIN
        DELETE FROM string_cache WHERE cache_timestamp < CURRENT_TIMESTAMP - INTERVAL '1' HOUR;
    END
END;
```

#### **Complex Business Logic Implementation**
```sql
-- Implement financial calculations package body
CREATE PACKAGE BODY finance_calculations AS
BEGIN
    -- Private constants and helper functions
    FUNCTION validate_financial_inputs(principal DECIMAL(15,2), rate DECIMAL(5,4), time_value INTEGER)
        RETURNS BOOLEAN
    AS
    BEGIN
        RETURN (principal > 0 AND rate >= 0 AND time_value > 0);
    END
    
    -- Public function implementations
    FUNCTION simple_interest(principal DECIMAL(15,2), rate DECIMAL(5,4), time_years INTEGER) 
        RETURNS DECIMAL(15,2)
        DETERMINISTIC
    AS
    BEGIN
        IF (NOT validate_financial_inputs(principal, rate, time_years)) THEN
            EXCEPTION invalid_input 'Invalid financial calculation inputs';
            
        RETURN principal * rate * time_years;
    END
    
    FUNCTION compound_interest(principal DECIMAL(15,2), rate DECIMAL(5,4), 
                              periods INTEGER, time_years INTEGER) 
        RETURNS DECIMAL(15,2)
        DETERMINISTIC
    AS
        DECLARE compound_rate DECIMAL(10,8);
        DECLARE total_periods INTEGER;
        DECLARE result DECIMAL(15,2);
    BEGIN
        IF (NOT validate_financial_inputs(principal, rate, time_years) OR periods <= 0) THEN
            EXCEPTION invalid_input 'Invalid compound interest calculation inputs';
        
        compound_rate = rate / periods;
        total_periods = periods * time_years;
        
        result = principal * POWER(1 + compound_rate, total_periods) - principal;
        
        RETURN result;
    END
    
    -- Public procedure implementations
    PROCEDURE calculate_loan_payment(loan_amount DECIMAL(15,2), 
                                   annual_rate DECIMAL(5,4), 
                                   loan_term_months INTEGER,
                                   OUT monthly_payment DECIMAL(15,2))
    AS
        DECLARE monthly_rate DECIMAL(10,8);
        DECLARE payment_factor DECIMAL(15,8);
    BEGIN
        IF (loan_amount <= 0 OR annual_rate < 0 OR loan_term_months <= 0) THEN
        BEGIN
            monthly_payment = 0;
            RETURN;
        END
        
        monthly_rate = annual_rate / 12;
        
        IF (monthly_rate = 0) THEN
            monthly_payment = loan_amount / loan_term_months;
        ELSE
        BEGIN
            payment_factor = POWER(1 + monthly_rate, loan_term_months);
            monthly_payment = loan_amount * (monthly_rate * payment_factor) / (payment_factor - 1);
        END
    END
    
    PROCEDURE process_loan_application(customer_id INTEGER, loan_amount DECIMAL(15,2))
    AS
        DECLARE customer_credit_score INTEGER;
        DECLARE monthly_income DECIMAL(12,2);
        DECLARE debt_to_income_ratio DECIMAL(5,4);
    BEGIN
        -- Get customer financial information
        SELECT credit_score, monthly_income, debt_to_income 
        FROM customer_financial_info 
        WHERE customer_id = :customer_id
        INTO :customer_credit_score, :monthly_income, :debt_to_income_ratio;
        
        -- Log loan application
        INSERT INTO loan_applications (customer_id, loan_amount, application_date, status)
        VALUES (:customer_id, :loan_amount, CURRENT_DATE, 'PENDING_REVIEW');
        
        -- Basic approval logic (simplified)
        IF (customer_credit_score >= 650 AND debt_to_income_ratio <= 0.40) THEN
        BEGIN
            UPDATE loan_applications 
            SET status = 'PRE_APPROVED'
            WHERE customer_id = :customer_id AND application_date = CURRENT_DATE;
        END
    END
END;
```

---

## ALTER PACKAGE

Modifies an existing package specification by adding, modifying, or removing function and procedure declarations.

### **ALTER PACKAGE Syntax**
```sql
ALTER PACKAGE [schema_name.]package_name
    [SQL SECURITY {DEFINER | INVOKER}]
AS
BEGIN
    -- Modified function and procedure declarations
END;
```

### **ALTER PACKAGE Examples**

#### **Adding Functions to Existing Package**
```sql
-- Add new functions to math_utils package
ALTER PACKAGE math_utils AS
BEGIN
    -- Existing functions (must be redeclared)
    FUNCTION square(input_value DOUBLE PRECISION) RETURNS DOUBLE PRECISION DETERMINISTIC;
    FUNCTION cube(input_value DOUBLE PRECISION) RETURNS DOUBLE PRECISION DETERMINISTIC;
    FUNCTION factorial(n INTEGER) RETURNS BIGINT;
    
    -- New functions
    FUNCTION power(base_value DOUBLE PRECISION, exponent_value INTEGER) 
        RETURNS DOUBLE PRECISION DETERMINISTIC;
    FUNCTION is_prime(n INTEGER) RETURNS BOOLEAN;
    FUNCTION greatest_common_divisor(a INTEGER, b INTEGER) RETURNS INTEGER DETERMINISTIC;
END;
```

#### **Modifying Function Signatures**
```sql
-- Modify string_utils package to add new parameters
ALTER PACKAGE string_utils AS
BEGIN
    -- Modified function signatures
    FUNCTION reverse_string(input_str VARCHAR(1000), preserve_case BOOLEAN DEFAULT TRUE) 
        RETURNS VARCHAR(1000);
    FUNCTION word_count(input_str VARCHAR(8000), delimiter VARCHAR(10) DEFAULT ' ') 
        RETURNS INTEGER;
    
    -- Enhanced procedure signatures
    PROCEDURE log_string_operation(operation_name VARCHAR(100), 
                                 input_str VARCHAR(1000),
                                 execution_time_ms INTEGER DEFAULT NULL);
    PROCEDURE clear_string_cache(older_than_hours INTEGER DEFAULT 1);
    
    -- New utility functions
    FUNCTION trim_whitespace(input_str VARCHAR(2000)) RETURNS VARCHAR(2000);
    FUNCTION capitalize_words(input_str VARCHAR(1000)) RETURNS VARCHAR(1000);
END;
```

#### **Changing Package Security Model**
```sql
-- Change finance_calculations from DEFINER to INVOKER security
ALTER PACKAGE finance_calculations
    SQL SECURITY INVOKER
AS
BEGIN
    -- All existing function declarations (unchanged)
    FUNCTION simple_interest(principal DECIMAL(15,2), rate DECIMAL(5,4), time_years INTEGER) 
        RETURNS DECIMAL(15,2) DETERMINISTIC;
    
    FUNCTION compound_interest(principal DECIMAL(15,2), rate DECIMAL(5,4), 
                              periods INTEGER, time_years INTEGER) 
        RETURNS DECIMAL(15,2) DETERMINISTIC;
    
    PROCEDURE calculate_loan_payment(loan_amount DECIMAL(15,2), 
                                   annual_rate DECIMAL(5,4), 
                                   loan_term_months INTEGER,
                                   OUT monthly_payment DECIMAL(15,2));
    
    PROCEDURE process_loan_application(customer_id INTEGER, loan_amount DECIMAL(15,2));
END;
```

---

## DROP PACKAGE and DROP PACKAGE BODY

Removes package specifications and/or package bodies from the database.

### **DROP PACKAGE Syntax**
```sql
-- Drop package specification (and body if exists)
DROP PACKAGE [IF EXISTS] [schema_name.]package_name;

-- Drop only package body (specification remains)
DROP PACKAGE BODY [IF EXISTS] [schema_name.]package_name;
```

### **DROP PACKAGE Examples**

#### **Basic Package Removal**
```sql
-- Drop entire package (specification and body)
DROP PACKAGE math_utils;

-- Drop package body only (specification remains)
DROP PACKAGE BODY string_utils;

-- Safe dropping with IF EXISTS
DROP PACKAGE IF EXISTS enterprise.finance.accounting_utils;
DROP PACKAGE BODY IF EXISTS data_processor;
```

#### **Dependency Checking Before Drop**
```sql
-- Check for package dependencies before dropping
SELECT DISTINCT
    rf.RDB$DEPENDENT_NAME as DEPENDENT_OBJECT,
    rf.RDB$DEPENDENT_TYPE as DEPENDENT_TYPE
FROM RDB$DEPENDENCIES rf
WHERE rf.RDB$DEPENDED_ON_NAME = 'MATH_UTILS'
  AND rf.RDB$DEPENDED_ON_TYPE = 18; -- obj_package_header

-- Drop package after confirming no dependencies
DROP PACKAGE math_utils;
```

---

## RECREATE PACKAGE

Combines DROP and CREATE operations in a single atomic transaction.

### **RECREATE PACKAGE Syntax**
```sql
-- Recreate package specification
RECREATE PACKAGE [schema_name.]package_name
    [SQL SECURITY {DEFINER | INVOKER}]
AS
BEGIN
    -- Function and procedure declarations
END;

-- Recreate package body
RECREATE PACKAGE BODY [schema_name.]package_name
AS
BEGIN
    -- Function and procedure implementations
END;
```

### **RECREATE PACKAGE Examples**

#### **Complete Package Recreation**
```sql
-- Recreate math utilities package with enhanced functionality
RECREATE PACKAGE math_utils
    SQL SECURITY DEFINER
AS
BEGIN
    -- Enhanced mathematical functions
    FUNCTION square(input_value DOUBLE PRECISION) RETURNS DOUBLE PRECISION DETERMINISTIC;
    FUNCTION cube(input_value DOUBLE PRECISION) RETURNS DOUBLE PRECISION DETERMINISTIC;
    FUNCTION power(base_value DOUBLE PRECISION, exponent_value DOUBLE PRECISION) 
        RETURNS DOUBLE PRECISION DETERMINISTIC;
    FUNCTION factorial(n INTEGER) RETURNS BIGINT;
    
    -- New statistical functions
    FUNCTION mean(values_array DOUBLE PRECISION[]) RETURNS DOUBLE PRECISION;
    FUNCTION standard_deviation(values_array DOUBLE PRECISION[]) RETURNS DOUBLE PRECISION;
    
    -- Mathematical utility procedures
    PROCEDURE validate_number_range(input_value DOUBLE PRECISION, 
                                  min_value DOUBLE PRECISION, 
                                  max_value DOUBLE PRECISION);
END;

-- Recreate the package body with implementations
RECREATE PACKAGE BODY math_utils AS
BEGIN
    -- Implement all declared functions and procedures
    FUNCTION square(input_value DOUBLE PRECISION) RETURNS DOUBLE PRECISION
        DETERMINISTIC
    AS
    BEGIN
        RETURN input_value * input_value;
    END
    
    FUNCTION cube(input_value DOUBLE PRECISION) RETURNS DOUBLE PRECISION
        DETERMINISTIC
    AS
    BEGIN
        RETURN input_value * input_value * input_value;
    END
    
    FUNCTION power(base_value DOUBLE PRECISION, exponent_value DOUBLE PRECISION) 
        RETURNS DOUBLE PRECISION
        DETERMINISTIC
    AS
    BEGIN
        RETURN POWER(base_value, exponent_value);
    END
    
    FUNCTION factorial(n INTEGER) RETURNS BIGINT
    AS
        DECLARE result BIGINT = 1;
        DECLARE counter INTEGER = 1;
    BEGIN
        IF (n < 0) THEN
            EXCEPTION factorial_error 'Factorial not defined for negative numbers';
        
        WHILE (counter <= n) DO
        BEGIN
            result = result * counter;
            counter = counter + 1;
        END
        
        RETURN result;
    END
    
    FUNCTION mean(values_array DOUBLE PRECISION[]) RETURNS DOUBLE PRECISION
    AS
        DECLARE sum_value DOUBLE PRECISION = 0;
        DECLARE i INTEGER = 1;
        DECLARE array_size INTEGER;
    BEGIN
        array_size = ARRAY_LENGTH(values_array);
        
        IF (array_size = 0) THEN
            RETURN NULL;
        
        WHILE (i <= array_size) DO
        BEGIN
            sum_value = sum_value + values_array[i];
            i = i + 1;
        END
        
        RETURN sum_value / array_size;
    END
    
    FUNCTION standard_deviation(values_array DOUBLE PRECISION[]) RETURNS DOUBLE PRECISION
    AS
        DECLARE mean_value DOUBLE PRECISION;
        DECLARE variance DOUBLE PRECISION = 0;
        DECLARE i INTEGER = 1;
        DECLARE array_size INTEGER;
    BEGIN
        array_size = ARRAY_LENGTH(values_array);
        
        IF (array_size <= 1) THEN
            RETURN NULL;
        
        mean_value = mean(values_array);
        
        WHILE (i <= array_size) DO
        BEGIN
            variance = variance + POWER(values_array[i] - mean_value, 2);
            i = i + 1;
        END
        
        RETURN SQRT(variance / (array_size - 1));
    END
    
    PROCEDURE validate_number_range(input_value DOUBLE PRECISION, 
                                  min_value DOUBLE PRECISION, 
                                  max_value DOUBLE PRECISION)
    AS
    BEGIN
        IF (input_value < min_value OR input_value > max_value) THEN
            EXCEPTION value_out_of_range 
                'Value ' || CAST(input_value AS VARCHAR(20)) || 
                ' is outside valid range [' || CAST(min_value AS VARCHAR(20)) || 
                ', ' || CAST(max_value AS VARCHAR(20)) || ']';
    END
END;
```

---

## Using Packages in Applications

### **Calling Package Functions and Procedures**

#### **Function Calls**
```sql
-- Call package functions using package.function_name syntax
SELECT math_utils.square(5.0) as square_result FROM RDB$DATABASE;
SELECT math_utils.factorial(6) as factorial_result FROM RDB$DATABASE;

-- Use package functions in complex expressions
SELECT 
    customer_id,
    math_utils.square(sales_amount) as sales_squared,
    math_utils.cube(profit_margin) as profit_cubed
FROM sales_data
WHERE math_utils.power(growth_rate, 2) > 1.1;

-- Use in computed columns
SELECT 
    product_name,
    string_utils.word_count(description) as description_words,
    string_utils.reverse_string(product_code) as reversed_code
FROM products;
```

#### **Procedure Calls**
```sql
-- Execute package procedures
EXECUTE PROCEDURE finance_calculations.calculate_loan_payment(
    250000.00,  -- loan_amount
    0.045,      -- annual_rate  
    360         -- loan_term_months
);

-- Use in PSQL blocks
DECLARE payment DECIMAL(15,2);
BEGIN
    EXECUTE PROCEDURE finance_calculations.calculate_loan_payment(
        100000.00, 0.04, 240) RETURNING_VALUES payment;
    
    -- Use the calculated payment
    INSERT INTO loan_quotes (customer_id, loan_amount, monthly_payment, quote_date)
    VALUES (1001, 100000.00, :payment, CURRENT_DATE);
END
```

#### **Mixed Package Usage**
```sql
-- Complex query using multiple package functions
WITH customer_analysis AS (
    SELECT 
        c.customer_id,
        c.customer_name,
        string_utils.word_count(c.customer_name) as name_complexity,
        math_utils.square(c.credit_score) as credit_score_squared,
        finance_calculations.compound_interest(
            c.current_balance, 0.05, 12, 1
        ) as projected_interest
    FROM customers c
    WHERE c.status = 'ACTIVE'
)
SELECT * FROM customer_analysis
WHERE name_complexity >= 3
ORDER BY credit_score_squared DESC;
```

### **Package Dependencies and Management**

#### **Inter-Package Dependencies**
```sql
-- Create utility package that depends on other packages
CREATE PACKAGE reporting_utils AS
BEGIN
    FUNCTION generate_financial_summary(customer_id INTEGER) RETURNS VARCHAR(4000);
    FUNCTION format_customer_report(customer_id INTEGER) RETURNS VARCHAR(8000);
    PROCEDURE export_monthly_reports(report_month INTEGER, report_year INTEGER);
END;

-- Implementation using other packages
CREATE PACKAGE BODY reporting_utils AS
BEGIN
    FUNCTION generate_financial_summary(customer_id INTEGER) RETURNS VARCHAR(4000)
    AS
        DECLARE summary VARCHAR(4000);
        DECLARE customer_balance DECIMAL(15,2);
        DECLARE projected_interest DECIMAL(15,2);
    BEGIN
        -- Get customer balance
        SELECT current_balance FROM customers 
        WHERE customer_id = :customer_id
        INTO :customer_balance;
        
        -- Calculate projected interest using finance package
        projected_interest = finance_calculations.compound_interest(
            customer_balance, 0.04, 12, 1
        );
        
        -- Format summary using string utilities
        summary = 'Customer Balance: ' || CAST(customer_balance AS VARCHAR(20)) ||
                 ', Projected Interest: ' || CAST(projected_interest AS VARCHAR(20));
        
        RETURN string_utils.capitalize_words(summary);
    END
    
    -- Other function and procedure implementations...
END;
```

---

## System Catalog Integration

ScratchBird stores package definitions in the RDB$PACKAGES system table.

### **Querying Package Information**

#### **List All Packages**
```sql
-- Show all packages in the database
SELECT 
    RDB$PACKAGE_NAME as PACKAGE_NAME,
    COALESCE(RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME,
    RDB$OWNER as OWNER,
    CASE RDB$SYSTEM_FLAG
        WHEN 1 THEN 'SYSTEM'
        WHEN 0 THEN 'USER_DEFINED'
        ELSE 'UNKNOWN'
    END as PACKAGE_TYPE,
    CASE RDB$SQL_SECURITY
        WHEN 1 THEN 'DEFINER'
        WHEN 0 THEN 'INVOKER'
        ELSE 'DEFAULT'
    END as SQL_SECURITY,
    CASE RDB$VALID_BODY_FLAG
        WHEN 1 THEN 'YES'
        WHEN 0 THEN 'NO'
        ELSE 'UNKNOWN'
    END as HAS_BODY,
    RDB$DESCRIPTION as DESCRIPTION
FROM RDB$PACKAGES
ORDER BY SCHEMA_NAME, PACKAGE_NAME;
```

#### **Package Functions and Procedures**
```sql
-- List all functions and procedures in packages
SELECT 
    p.RDB$PACKAGE_NAME as PACKAGE_NAME,
    f.RDB$FUNCTION_NAME as FUNCTION_NAME,
    'FUNCTION' as OBJECT_TYPE,
    f.RDB$RETURN_ARGUMENT as RETURN_TYPE,
    f.RDB$DESCRIPTION as DESCRIPTION
FROM RDB$PACKAGES p
JOIN RDB$FUNCTIONS f ON p.RDB$PACKAGE_NAME = f.RDB$PACKAGE_NAME
WHERE f.RDB$PACKAGE_NAME IS NOT NULL

UNION ALL

SELECT 
    p.RDB$PACKAGE_NAME as PACKAGE_NAME,
    pr.RDB$PROCEDURE_NAME as PROCEDURE_NAME,
    'PROCEDURE' as OBJECT_TYPE,
    NULL as RETURN_TYPE,
    pr.RDB$DESCRIPTION as DESCRIPTION
FROM RDB$PACKAGES p
JOIN RDB$PROCEDURES pr ON p.RDB$PACKAGE_NAME = pr.RDB$PACKAGE_NAME
WHERE pr.RDB$PACKAGE_NAME IS NOT NULL

ORDER BY PACKAGE_NAME, OBJECT_TYPE, FUNCTION_NAME;
```

#### **Package Dependencies**
```sql
-- Find dependencies on packages
SELECT DISTINCT
    d.RDB$DEPENDENT_NAME as DEPENDENT_OBJECT,
    CASE d.RDB$DEPENDENT_TYPE
        WHEN 2 THEN 'TRIGGER'
        WHEN 5 THEN 'PROCEDURE'
        WHEN 15 THEN 'FUNCTION'
        WHEN 18 THEN 'PACKAGE'
        ELSE 'OTHER'
    END as DEPENDENT_TYPE,
    d.RDB$DEPENDED_ON_NAME as PACKAGE_NAME
FROM RDB$DEPENDENCIES d
WHERE d.RDB$DEPENDED_ON_TYPE = 18  -- obj_package_header
ORDER BY PACKAGE_NAME, DEPENDENT_TYPE, DEPENDENT_OBJECT;
```

### **Schema-Aware Package Queries**

#### **Packages by Schema**
```sql
-- List packages grouped by schema hierarchy
SELECT 
    COALESCE(RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME,
    COUNT(*) as PACKAGE_COUNT,
    STRING_AGG(RDB$PACKAGE_NAME, ', ' ORDER BY RDB$PACKAGE_NAME) as PACKAGES
FROM RDB$PACKAGES
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
GROUP BY RDB$SCHEMA_NAME
ORDER BY SCHEMA_NAME;
```

#### **Package Usage Analysis**
```sql
-- Analyze package usage across the database
WITH package_usage AS (
    SELECT 
        p.RDB$PACKAGE_NAME,
        COUNT(DISTINCT f.RDB$FUNCTION_NAME) as FUNCTION_COUNT,
        COUNT(DISTINCT pr.RDB$PROCEDURE_NAME) as PROCEDURE_COUNT,
        COUNT(DISTINCT d.RDB$DEPENDENT_NAME) as DEPENDENT_COUNT
    FROM RDB$PACKAGES p
    LEFT JOIN RDB$FUNCTIONS f ON p.RDB$PACKAGE_NAME = f.RDB$PACKAGE_NAME
    LEFT JOIN RDB$PROCEDURES pr ON p.RDB$PACKAGE_NAME = pr.RDB$PACKAGE_NAME
    LEFT JOIN RDB$DEPENDENCIES d ON p.RDB$PACKAGE_NAME = d.RDB$DEPENDED_ON_NAME
        AND d.RDB$DEPENDED_ON_TYPE = 18
    WHERE p.RDB$SYSTEM_FLAG = 0 OR p.RDB$SYSTEM_FLAG IS NULL
    GROUP BY p.RDB$PACKAGE_NAME
)
SELECT 
    RDB$PACKAGE_NAME as PACKAGE_NAME,
    FUNCTION_COUNT,
    PROCEDURE_COUNT,
    FUNCTION_COUNT + PROCEDURE_COUNT as TOTAL_OBJECTS,
    DEPENDENT_COUNT as USAGE_COUNT
FROM package_usage
ORDER BY USAGE_COUNT DESC, TOTAL_OBJECTS DESC;
```

### **Package Security and Permissions**

#### **Package Permissions Query**
```sql
-- Show EXECUTE permissions on packages
SELECT 
    p.RDB$PACKAGE_NAME as PACKAGE_NAME,
    pr.RDB$USER as GRANTEE,
    pr.RDB$GRANTOR as GRANTOR,
    pr.RDB$PRIVILEGE as PRIVILEGE,
    CASE pr.RDB$GRANT_OPTION
        WHEN 1 THEN 'YES'
        ELSE 'NO'
    END as GRANT_OPTION
FROM RDB$PACKAGES p
JOIN RDB$USER_PRIVILEGES pr ON p.RDB$PACKAGE_NAME = pr.RDB$RELATION_NAME
WHERE pr.RDB$OBJECT_TYPE = 18  -- obj_package_header
  AND pr.RDB$PRIVILEGE = 'X'   -- EXECUTE privilege
ORDER BY PACKAGE_NAME, GRANTEE;
```

---

## Package Security and Permissions

### **Granting and Revoking Package Permissions**

#### **GRANT EXECUTE on Packages**
```sql
-- Grant execute permission on package to user
GRANT EXECUTE ON PACKAGE math_utils TO user1;

-- Grant execute permission to role
GRANT EXECUTE ON PACKAGE finance_calculations TO role_financial_analyst;

-- Grant execute with grant option
GRANT EXECUTE ON PACKAGE string_utils TO user2 WITH GRANT OPTION;

-- Grant execute to multiple users
GRANT EXECUTE ON PACKAGE enterprise.hr.employee_management 
  TO user_hr1, user_hr2, role_hr_manager;
```

#### **REVOKE EXECUTE on Packages**
```sql
-- Revoke execute permission
REVOKE EXECUTE ON PACKAGE math_utils FROM user1;

-- Revoke execute from role
REVOKE EXECUTE ON PACKAGE finance_calculations FROM role_financial_analyst;

-- Revoke grant option
REVOKE GRANT OPTION FOR EXECUTE ON PACKAGE string_utils FROM user2;
```

### **SQL Security Modes**

#### **DEFINER Security Mode**
```sql
-- Package executes with creator's privileges
CREATE PACKAGE secure_admin_utils
    SQL SECURITY DEFINER
AS
BEGIN
    FUNCTION get_system_info() RETURNS VARCHAR(1000);
    PROCEDURE cleanup_temp_data;
END;

-- Even if called by low-privilege user, executes with DEFINER's rights
SELECT secure_admin_utils.get_system_info() FROM RDB$DATABASE;
```

#### **INVOKER Security Mode**
```sql
-- Package executes with caller's privileges  
CREATE PACKAGE user_utilities
    SQL SECURITY INVOKER
AS
BEGIN
    FUNCTION get_my_data() RETURNS VARCHAR(2000);
    PROCEDURE update_my_profile(new_info VARCHAR(500));
END;

-- Executes with current user's privileges
EXECUTE PROCEDURE user_utilities.update_my_profile('New profile info');
```

---

## Advanced Package Features

### **Package Versioning and Evolution**

#### **Package Specification Evolution**
```sql
-- Version 1.0: Initial package
CREATE PACKAGE customer_api AS
BEGIN
    FUNCTION get_customer(customer_id INTEGER) RETURNS VARCHAR(1000);
    PROCEDURE save_customer(customer_data VARCHAR(1000));
END;

-- Version 1.1: Add new function (backward compatible)
ALTER PACKAGE customer_api AS
BEGIN
    -- Existing functions
    FUNCTION get_customer(customer_id INTEGER) RETURNS VARCHAR(1000);
    PROCEDURE save_customer(customer_data VARCHAR(1000));
    
    -- New function
    FUNCTION validate_customer(customer_data VARCHAR(1000)) RETURNS BOOLEAN;
END;

-- Version 2.0: Change function signature (breaking change)
ALTER PACKAGE customer_api AS
BEGIN
    -- Modified signature (breaking change)
    FUNCTION get_customer(customer_id INTEGER, include_history BOOLEAN DEFAULT FALSE) 
        RETURNS VARCHAR(2000);
    PROCEDURE save_customer(customer_data VARCHAR(1000));
    FUNCTION validate_customer(customer_data VARCHAR(1000)) RETURNS BOOLEAN;
    
    -- New procedure
    PROCEDURE archive_customer(customer_id INTEGER);
END;
```

#### **Managing Package Dependencies**
```sql
-- Create dependency tracking view
CREATE VIEW package_dependency_tree AS
WITH RECURSIVE dep_tree AS (
    -- Base case: packages with no dependencies
    SELECT 
        p.RDB$PACKAGE_NAME as package_name,
        0 as dependency_level,
        CAST(p.RDB$PACKAGE_NAME AS VARCHAR(1000)) as dependency_path
    FROM RDB$PACKAGES p
    WHERE NOT EXISTS (
        SELECT 1 FROM RDB$DEPENDENCIES d
        WHERE d.RDB$DEPENDENT_NAME = p.RDB$PACKAGE_NAME
          AND d.RDB$DEPENDENT_TYPE = 18
    )
    
    UNION ALL
    
    -- Recursive case: packages that depend on others
    SELECT 
        d.RDB$DEPENDENT_NAME,
        dt.dependency_level + 1,
        CAST(dt.dependency_path || ' -> ' || d.RDB$DEPENDENT_NAME AS VARCHAR(1000))
    FROM RDB$DEPENDENCIES d
    JOIN dep_tree dt ON d.RDB$DEPENDED_ON_NAME = dt.package_name
    WHERE d.RDB$DEPENDENT_TYPE = 18
      AND d.RDB$DEPENDED_ON_TYPE = 18
)
SELECT * FROM dep_tree;
```

### **Package Testing and Validation**

#### **Package Unit Testing Framework**
```sql
-- Create testing package for math_utils
CREATE PACKAGE test_math_utils AS
BEGIN
    PROCEDURE test_square;
    PROCEDURE test_factorial;
    PROCEDURE test_power;
    PROCEDURE run_all_tests;
END;

CREATE PACKAGE BODY test_math_utils AS
BEGIN
    PROCEDURE test_square
    AS
        DECLARE result DOUBLE PRECISION;
        DECLARE expected DOUBLE PRECISION;
    BEGIN
        -- Test positive numbers
        result = math_utils.square(5.0);
        expected = 25.0;
        IF (result <> expected) THEN
            EXCEPTION test_failure 'square(5.0) failed: expected ' || expected || ', got ' || result;
        
        -- Test negative numbers
        result = math_utils.square(-3.0);
        expected = 9.0;
        IF (result <> expected) THEN
            EXCEPTION test_failure 'square(-3.0) failed: expected ' || expected || ', got ' || result;
        
        -- Test zero
        result = math_utils.square(0.0);
        expected = 0.0;
        IF (result <> expected) THEN
            EXCEPTION test_failure 'square(0.0) failed: expected ' || expected || ', got ' || result;
    END
    
    PROCEDURE test_factorial
    AS
        DECLARE result BIGINT;
    BEGIN
        -- Test factorial(0)
        result = math_utils.factorial(0);
        IF (result <> 1) THEN
            EXCEPTION test_failure 'factorial(0) should equal 1';
        
        -- Test factorial(5)
        result = math_utils.factorial(5);
        IF (result <> 120) THEN
            EXCEPTION test_failure 'factorial(5) should equal 120';
        
        -- Test negative input (should raise exception)
        BEGIN
            result = math_utils.factorial(-1);
            EXCEPTION test_failure 'factorial(-1) should raise exception';
        WHEN ANY DO
            -- Expected exception, test passes
        END
    END
    
    PROCEDURE test_power
    AS
        DECLARE result DOUBLE PRECISION;
    BEGIN
        result = math_utils.power(2.0, 3);
        IF (ABS(result - 8.0) > 0.001) THEN
            EXCEPTION test_failure 'power(2.0, 3) should equal 8.0';
    END
    
    PROCEDURE run_all_tests
    AS
    BEGIN
        EXECUTE PROCEDURE test_square;
        EXECUTE PROCEDURE test_factorial;
        EXECUTE PROCEDURE test_power;
        
        INSERT INTO test_results (package_name, test_date, status, message)
        VALUES ('math_utils', CURRENT_TIMESTAMP, 'PASSED', 'All tests passed');
    END
END;
```

#### **Package Performance Monitoring**
```sql
-- Create performance monitoring package
CREATE PACKAGE package_monitor AS
BEGIN
    PROCEDURE start_timing(operation_name VARCHAR(100));
    PROCEDURE end_timing(operation_name VARCHAR(100));
    FUNCTION get_average_execution_time(package_name VARCHAR(63), 
                                      function_name VARCHAR(63)) 
        RETURNS DOUBLE PRECISION;
END;

CREATE PACKAGE BODY package_monitor AS
BEGIN
    PROCEDURE start_timing(operation_name VARCHAR(100))
    AS
    BEGIN
        INSERT INTO package_timing_log (operation_name, start_time)
        VALUES (:operation_name, CURRENT_TIMESTAMP);
    END
    
    PROCEDURE end_timing(operation_name VARCHAR(100))
    AS
        DECLARE start_time TIMESTAMP;
        DECLARE execution_time DOUBLE PRECISION;
    BEGIN
        SELECT MAX(start_time) FROM package_timing_log
        WHERE operation_name = :operation_name AND end_time IS NULL
        INTO :start_time;
        
        execution_time = EXTRACT(MILLISECOND FROM (CURRENT_TIMESTAMP - start_time));
        
        UPDATE package_timing_log
        SET end_time = CURRENT_TIMESTAMP, execution_time_ms = :execution_time
        WHERE operation_name = :operation_name AND start_time = :start_time;
    END
    
    FUNCTION get_average_execution_time(package_name VARCHAR(63), 
                                      function_name VARCHAR(63)) 
        RETURNS DOUBLE PRECISION
    AS
        DECLARE avg_time DOUBLE PRECISION;
    BEGIN
        SELECT AVG(execution_time_ms) FROM package_timing_log
        WHERE operation_name = :package_name || '.' || :function_name
          AND execution_time_ms IS NOT NULL
        INTO :avg_time;
        
        RETURN COALESCE(avg_time, 0);
    END
END;
```

---

## Error Handling and Troubleshooting

### **Common Package Errors**

#### **Package Creation Errors**
```sql
-- Error: Package already exists
CREATE PACKAGE existing_package AS BEGIN END;
-- Solution: Use IF NOT EXISTS or ALTER PACKAGE

-- Error: Function/procedure already declared
CREATE PACKAGE bad_package AS
BEGIN
    FUNCTION test_func() RETURNS INTEGER;
    FUNCTION test_func() RETURNS VARCHAR(100);  -- Duplicate declaration
END;
-- Solution: Remove duplicate declarations

-- Error: Invalid SQL Security option
CREATE PACKAGE bad_security
    SQL SECURITY INVALID_OPTION  -- Invalid option
AS BEGIN END;
-- Solution: Use DEFINER or INVOKER
```

#### **Package Body Errors**
```sql
-- Error: Package body without specification
CREATE PACKAGE BODY nonexistent_package AS BEGIN END;
-- Solution: Create package specification first

-- Error: Function implementation doesn't match specification
-- Package spec declares: FUNCTION test() RETURNS INTEGER;
-- Body implements: FUNCTION test() RETURNS VARCHAR(100); -- Mismatch
-- Solution: Match function signatures exactly
```

#### **Package Usage Errors**
```sql
-- Error: Function not found in package
SELECT nonexistent_package.missing_function() FROM RDB$DATABASE;
-- Solution: Verify package and function names

-- Error: Permission denied
EXECUTE PROCEDURE restricted_package.admin_function();
-- Solution: Grant EXECUTE permission on package
```

### **Debugging Package Issues**

#### **Package Compilation Diagnostics**
```sql
-- Check package compilation status
SELECT 
    RDB$PACKAGE_NAME,
    CASE RDB$VALID_BODY_FLAG
        WHEN 1 THEN 'VALID'
        WHEN 0 THEN 'INVALID'
        ELSE 'UNKNOWN'
    END as BODY_STATUS,
    RDB$PACKAGE_HEADER_SOURCE,
    RDB$PACKAGE_BODY_SOURCE
FROM RDB$PACKAGES
WHERE RDB$PACKAGE_NAME = 'YOUR_PACKAGE_NAME';
```

#### **Package Dependency Analysis**
```sql
-- Find broken package dependencies
SELECT DISTINCT
    d.RDB$DEPENDENT_NAME as BROKEN_PACKAGE,
    d.RDB$DEPENDED_ON_NAME as MISSING_DEPENDENCY,
    CASE d.RDB$DEPENDED_ON_TYPE
        WHEN 18 THEN 'PACKAGE'
        WHEN 15 THEN 'FUNCTION'
        WHEN 5 THEN 'PROCEDURE'
        ELSE 'OTHER'
    END as DEPENDENCY_TYPE
FROM RDB$DEPENDENCIES d
LEFT JOIN RDB$PACKAGES p ON d.RDB$DEPENDED_ON_NAME = p.RDB$PACKAGE_NAME
    AND d.RDB$DEPENDED_ON_TYPE = 18
WHERE d.RDB$DEPENDENT_TYPE = 18
  AND d.RDB$DEPENDED_ON_TYPE = 18
  AND p.RDB$PACKAGE_NAME IS NULL;
```

### **Package Migration Issues**

#### **Package Recreation for Schema Changes**
```sql
-- Safe package recreation during schema migration
BEGIN
    -- Save existing permissions
    EXECUTE BLOCK AS
    DECLARE package_perms VARCHAR(4000);
    BEGIN
        SELECT STRING_AGG(
            'GRANT EXECUTE ON PACKAGE ' || RDB$RELATION_NAME || 
            ' TO ' || RDB$USER || 
            CASE RDB$GRANT_OPTION WHEN 1 THEN ' WITH GRANT OPTION' ELSE '' END || ';'
        ) FROM RDB$USER_PRIVILEGES
        WHERE RDB$RELATION_NAME = 'OLD_PACKAGE'
          AND RDB$OBJECT_TYPE = 18
        INTO :package_perms;
        
        -- Store permissions for later restoration
        INSERT INTO temp_package_permissions (package_name, permissions)
        VALUES ('OLD_PACKAGE', :package_perms);
    END
    
    -- Recreate package
    RECREATE PACKAGE old_package AS
    BEGIN
        -- New package specification
    END;
    
    -- Restore permissions
    EXECUTE BLOCK AS
    DECLARE permissions VARCHAR(4000);
    BEGIN
        SELECT permissions FROM temp_package_permissions
        WHERE package_name = 'OLD_PACKAGE'
        INTO :permissions;
        
        IF (permissions IS NOT NULL) THEN
            EXECUTE STATEMENT :permissions;
    END
    
    -- Cleanup
    DELETE FROM temp_package_permissions WHERE package_name = 'OLD_PACKAGE';
    
    COMMIT;
END
```

---

## Best Practices

### **Package Design Guidelines**

1. **Interface Segregation**: Keep package interfaces focused and cohesive
2. **Dependency Management**: Minimize cross-package dependencies
3. **Version Compatibility**: Design for backward compatibility when possible
4. **Security Model**: Choose appropriate SQL Security mode for each package
5. **Documentation**: Document package specifications thoroughly
6. **Testing**: Implement comprehensive unit tests for package functions

### **Recommended Package Patterns**

#### **Utility Package Pattern**
```sql
-- Create focused utility packages for specific domains
CREATE PACKAGE date_utils AS
BEGIN
    FUNCTION is_business_day(check_date DATE) RETURNS BOOLEAN;
    FUNCTION add_business_days(start_date DATE, days_to_add INTEGER) RETURNS DATE;
    FUNCTION get_month_end(input_date DATE) RETURNS DATE;
    PROCEDURE validate_date_range(start_date DATE, end_date DATE);
END;

CREATE PACKAGE number_utils AS  
BEGIN
    FUNCTION format_currency(amount DECIMAL(15,2), currency_code VARCHAR(3)) RETURNS VARCHAR(50);
    FUNCTION round_to_nearest(value DECIMAL(15,6), nearest DECIMAL(15,6)) RETURNS DECIMAL(15,6);
    FUNCTION is_within_tolerance(value1 DECIMAL(15,6), value2 DECIMAL(15,6), tolerance DECIMAL(15,6)) RETURNS BOOLEAN;
END;
```

#### **Business Logic Package Pattern**
```sql
-- Organize business logic by functional area
CREATE PACKAGE order_management
    SQL SECURITY DEFINER
AS
BEGIN
    FUNCTION calculate_order_total(order_id INTEGER) RETURNS DECIMAL(15,2);
    FUNCTION check_inventory_availability(product_id INTEGER, quantity INTEGER) RETURNS BOOLEAN;
    PROCEDURE process_order(order_id INTEGER);
    PROCEDURE cancel_order(order_id INTEGER, reason VARCHAR(200));
END;

CREATE PACKAGE customer_service
    SQL SECURITY INVOKER
AS
BEGIN
    FUNCTION get_customer_summary(customer_id INTEGER) RETURNS VARCHAR(4000);
    PROCEDURE log_customer_interaction(customer_id INTEGER, interaction_type VARCHAR(50), notes VARCHAR(1000));
    PROCEDURE update_customer_preferences(customer_id INTEGER, preferences VARCHAR(2000));
END;
```

#### **Data Access Package Pattern**
```sql
-- Create data access layer packages
CREATE PACKAGE customer_data AS
BEGIN
    FUNCTION get_customer_by_email(email VARCHAR(255)) RETURNS INTEGER;
    FUNCTION customer_exists(customer_id INTEGER) RETURNS BOOLEAN;
    PROCEDURE save_customer(customer_data VARCHAR(4000));
    PROCEDURE delete_customer(customer_id INTEGER);
END;

CREATE PACKAGE product_data AS
BEGIN
    FUNCTION find_products_by_category(category_id INTEGER) RETURNS VARCHAR(8000);
    FUNCTION get_product_price(product_id INTEGER) RETURNS DECIMAL(15,2);
    PROCEDURE update_product_inventory(product_id INTEGER, quantity_change INTEGER);
END;
```

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:3381-3517`
- **Grammar Rules**: 
  - `package_clause` (lines 3384-3392): CREATE PACKAGE syntax
  - `package_body_clause` (lines 3463-3478): CREATE PACKAGE BODY syntax
  - `alter_package_clause` (lines 3434-3447): ALTER PACKAGE syntax
- **Functionality**: Parsing CREATE/ALTER/DROP PACKAGE syntax with SQL security

#### **DDL Node Classes**
- **File**: `src/dsql/PackageNodes.h:32-242`
- **Classes**:
  - `CreateAlterPackageNode` (lines 32-119): Package specification creation/modification
  - `DropPackageNode` (lines 122-159): Package specification removal
  - `CreatePackageBodyNode` (lines 166-200): Package body creation  
  - `DropPackageBodyNode` (lines 203-236): Package body removal
  - `RecreatePackageNode` and `RecreatePackageBodyNode`: Atomic recreation operations

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:694-706`
- **Table**: `RDB$PACKAGES` - Stores package definitions
- **Fields**:
  - `f_pkg_name`: Package name
  - `f_pkg_header_source`: Package specification source code
  - `f_pkg_body_source`: Package body source code
  - `f_pkg_valid_body_flag`: Body compilation status
  - `f_pkg_class`: Package class/type information
  - `f_pkg_owner`: Package owner
  - `f_pkg_sys_flag`: System vs user-defined flag
  - `f_pkg_sql_security`: SQL Security mode (DEFINER/INVOKER)
  - `f_pkg_schema`: Schema name for hierarchical support

#### **Object Type Constants**
- **File**: `src/jrd/constants.h`
- **Constants**: `obj_package_header` - Object type for packages in system catalog

### **Core Classes and Functions**

#### **CreateAlterPackageNode Methods**
- `dsqlPass()`: DSQL compilation and name qualification
- `execute()`: Package creation in system catalog
- `executeCreate()`: New package creation logic
- `executeAlter()`: Package modification logic
- `checkPermission()`: Security validation

#### **CreatePackageBodyNode Methods**
- `dsqlPass()`: Body compilation and validation against specification
- `execute()`: Package body creation with function/procedure compilation
- `checkPermission()`: Permission validation for package body creation

#### **Package Integration**
- **File**: `src/dsql/parse.y:1138-1144, 1432-1438`
- **Functionality**: GRANT/REVOKE EXECUTE ON PACKAGE syntax
- **File**: `src/dsql/parse.y:4614-4618`
- **Functionality**: DDL trigger support for package operations

### **Storage Structures**

Packages are stored in the RDB$PACKAGES system table with:

- **Identity**: Name, schema, owner information
- **Source Code**: Separate storage for specification and body source
- **Compilation Status**: Body validity flags and compilation metadata
- **Security**: SQL Security mode and permission integration
- **Schema Support**: Full hierarchical schema qualification
- **Dependencies**: Integration with RDB$DEPENDENCIES for cross-package references

---

## Administrative Operations

### **Package Backup and Restore**

#### **Backup Considerations**
- Package specifications and bodies are included in database backups as DDL
- Source code preservation maintains exact package definitions
- Dependencies on functions/procedures are preserved
- SQL Security settings are maintained across backup/restore

#### **Restore Procedures**
```sql
-- Verify packages after restore
SELECT 
    RDB$PACKAGE_NAME,
    CASE RDB$VALID_BODY_FLAG
        WHEN 1 THEN 'VALID'
        WHEN 0 THEN 'INVALID' 
        ELSE 'NO_BODY'
    END as BODY_STATUS,
    RDB$SQL_SECURITY as SECURITY_MODE
FROM RDB$PACKAGES
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL;

-- Test package functionality
SELECT math_utils.square(5.0) FROM RDB$DATABASE;
EXECUTE PROCEDURE string_utils.log_string_operation('test', 'sample');
```

### **Package Maintenance**

#### **Regular Maintenance Tasks**
```sql
-- Monitor package usage and performance
CREATE VIEW package_usage_stats AS
SELECT 
    p.RDB$PACKAGE_NAME,
    p.RDB$OWNER,
    CASE p.RDB$VALID_BODY_FLAG
        WHEN 1 THEN 'VALID'
        ELSE 'INVALID'
    END as BODY_STATUS,
    COUNT(DISTINCT f.RDB$FUNCTION_NAME) as FUNCTION_COUNT,
    COUNT(DISTINCT pr.RDB$PROCEDURE_NAME) as PROCEDURE_COUNT,
    COUNT(DISTINCT d.RDB$DEPENDENT_NAME) as USAGE_COUNT
FROM RDB$PACKAGES p
LEFT JOIN RDB$FUNCTIONS f ON p.RDB$PACKAGE_NAME = f.RDB$PACKAGE_NAME
LEFT JOIN RDB$PROCEDURES pr ON p.RDB$PACKAGE_NAME = pr.RDB$PACKAGE_NAME  
LEFT JOIN RDB$DEPENDENCIES d ON p.RDB$PACKAGE_NAME = d.RDB$DEPENDED_ON_NAME
    AND d.RDB$DEPENDED_ON_TYPE = 18
WHERE p.RDB$SYSTEM_FLAG = 0 OR p.RDB$SYSTEM_FLAG IS NULL
GROUP BY p.RDB$PACKAGE_NAME, p.RDB$OWNER, p.RDB$VALID_BODY_FLAG;

-- Check for unused packages
SELECT RDB$PACKAGE_NAME
FROM package_usage_stats
WHERE USAGE_COUNT = 0;
```

---

## Migration from Other Database Systems

### **From Oracle PL/SQL Packages**
Oracle packages translate to ScratchBird with minimal changes:
```sql
-- Oracle PL/SQL Package
CREATE OR REPLACE PACKAGE employee_pkg AS
    FUNCTION get_salary(emp_id NUMBER) RETURN NUMBER;
    PROCEDURE update_salary(emp_id NUMBER, new_salary NUMBER);
END employee_pkg;

-- ScratchBird equivalent
CREATE PACKAGE employee_pkg AS
BEGIN
    FUNCTION get_salary(emp_id INTEGER) RETURNS DECIMAL(10,2);
    PROCEDURE update_salary(emp_id INTEGER, new_salary DECIMAL(10,2));
END;
```

### **From PostgreSQL Schema Functions**
PostgreSQL schema-grouped functions can be organized into ScratchBird packages:
```sql
-- PostgreSQL approach (functions in schema)
CREATE SCHEMA utilities;
CREATE FUNCTION utilities.string_reverse(text) RETURNS text ...;
CREATE FUNCTION utilities.word_count(text) RETURNS integer ...;

-- ScratchBird package approach
CREATE PACKAGE utilities AS
BEGIN
    FUNCTION string_reverse(input_text VARCHAR(1000)) RETURNS VARCHAR(1000);
    FUNCTION word_count(input_text VARCHAR(1000)) RETURNS INTEGER;
END;
```

### **From SQL Server Schema Functions**
SQL Server functions can be grouped into ScratchBird packages:
```sql
-- SQL Server approach
CREATE SCHEMA BusinessLogic;
CREATE FUNCTION BusinessLogic.CalculateTotal(@OrderID int) RETURNS money ...;

-- ScratchBird package approach
CREATE PACKAGE business_logic AS
BEGIN
    FUNCTION calculate_total(order_id INTEGER) RETURNS DECIMAL(15,2);
END;
```

---

