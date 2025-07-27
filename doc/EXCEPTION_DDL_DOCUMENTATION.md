# ScratchBird EXCEPTION - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

EXCEPTION objects in ScratchBird provide a structured mechanism for defining and handling custom error conditions in stored procedures, functions, and triggers. Exceptions enable developers to create meaningful error messages, implement business logic validation, and provide controlled error handling throughout database applications. They are essential for robust error management and application-level exception handling.

### Key Features and Capabilities

- **Custom Error Definition**: Create application-specific error messages and codes
- **Structured Error Handling**: Integrate with PSQL exception handling blocks (WHEN ... DO)
- **Parameterized Messages**: Support for variable message content using message parameters
- **Error Code Generation**: Automatic assignment of unique exception numbers
- **System Integration**: Full integration with ScratchBird's error handling system
- **Schema Qualification**: Support for hierarchical schemas with 3-level qualification
- **Security Integration**: Granular USAGE permissions on exception objects
- **DDL Trigger Support**: Exception operations can trigger DDL events

### ScratchBird-Specific Enhancements

1. **Hierarchical Schema Support**: Exceptions support 3-level qualified names (catalog.schema.exception)
2. **Enhanced System Catalog**: Comprehensive RDB$EXCEPTIONS table with schema awareness
3. **DDL Trigger Integration**: Support for DDL triggers on exception operations (CREATE/ALTER/DROP)
4. **Advanced Permission Model**: USAGE permissions for controlled exception access
5. **Message Parameter Support**: Flexible message formatting with runtime parameters
6. **Cross-Language Integration**: Exception handling in external language procedures/functions
7. **Comprehensive Error Context**: Rich error information with stack traces and context
8. **Exception Chaining**: Support for nested exception handling and re-raising

---

## DDL Syntax Reference

### CREATE EXCEPTION

Creates a new user-defined exception with a specific error message.

#### **Basic Syntax**
```sql
CREATE EXCEPTION [IF NOT EXISTS] [schema_name.]exception_name 'error_message';
```

#### **Complete Syntax with All Options**
```sql
CREATE EXCEPTION [IF NOT EXISTS] [[catalog.]schema.]exception_name 'error_message_text';
```

#### **Parameters**

- **IF NOT EXISTS**: Skip creation if exception already exists (no error)
- **schema_name**: Optional schema qualification (supports hierarchical schemas)
- **exception_name**: Exception identifier (63 characters max)
- **error_message_text**: Error message string (up to 1021 characters)

---

## CREATE EXCEPTION Examples

### **Basic Exception Creation**

#### **Simple Business Logic Exceptions**
```sql
-- Create exceptions for common business validation errors
CREATE EXCEPTION insufficient_balance 'Account balance is insufficient for this transaction';

CREATE EXCEPTION invalid_customer_id 'Customer ID does not exist in the system';

CREATE EXCEPTION duplicate_email_address 'Email address is already registered';

CREATE EXCEPTION invalid_date_range 'Start date must be earlier than end date';

CREATE EXCEPTION product_not_available 'Product is not available for purchase';
```

#### **Application-Specific Exceptions**
```sql
-- Create exceptions for specific business domains
CREATE EXCEPTION order_already_shipped 'Cannot modify order - it has already been shipped';

CREATE EXCEPTION credit_limit_exceeded 'Transaction would exceed customer credit limit';

CREATE EXCEPTION inventory_shortage 'Insufficient inventory to fulfill order';

CREATE EXCEPTION unauthorized_access 'User does not have permission to access this resource';

CREATE EXCEPTION data_validation_failed 'Data validation failed - please check input values';
```

### **Parameterized Exception Messages**

#### **Exceptions with Parameter Placeholders**
```sql
-- Create exceptions that accept runtime parameters
CREATE EXCEPTION invalid_parameter_value 'Invalid value for parameter: @1';

CREATE EXCEPTION threshold_exceeded 'Value @1 exceeds maximum threshold of @2';

CREATE EXCEPTION record_not_found 'Record with ID @1 not found in table @2';

CREATE EXCEPTION connection_timeout 'Connection timeout after @1 seconds';

CREATE EXCEPTION format_validation_error 'Value "@1" does not match required format @2';
```

#### **Multi-Parameter Exception Messages**
```sql
-- Exceptions supporting multiple parameters
CREATE EXCEPTION transaction_validation_error 
    'Transaction validation failed: Account @1, Amount @2, Reason: @3';

CREATE EXCEPTION date_range_validation_error 
    'Invalid date range: @1 to @2. Maximum allowed range is @3 days';

CREATE EXCEPTION resource_conflict_error 
    'Resource conflict: @1 is currently locked by user @2 since @3';

CREATE EXCEPTION calculation_overflow_error 
    'Calculation overflow: @1 @2 @3 exceeds maximum value of @4';
```

### **Domain-Specific Exception Categories**

#### **Financial System Exceptions**
```sql
-- Financial processing exceptions
CREATE EXCEPTION insufficient_funds 'Account has insufficient funds for withdrawal';

CREATE EXCEPTION invalid_currency_code 'Currency code is not supported';

CREATE EXCEPTION exchange_rate_unavailable 'Exchange rate not available for specified currencies';

CREATE EXCEPTION transaction_limit_exceeded 'Daily transaction limit has been exceeded';

CREATE EXCEPTION account_frozen 'Account is frozen and cannot process transactions';

CREATE EXCEPTION payment_processing_failed 'Payment processing failed - please try again';
```

#### **Inventory Management Exceptions**
```sql
-- Inventory and warehouse exceptions
CREATE EXCEPTION item_out_of_stock 'Item is currently out of stock';

CREATE EXCEPTION warehouse_capacity_exceeded 'Warehouse capacity would be exceeded';

CREATE EXCEPTION invalid_product_category 'Product category is not valid';

CREATE EXCEPTION batch_number_conflict 'Batch number already exists for this product';

CREATE EXCEPTION expiration_date_passed 'Product has passed its expiration date';

CREATE EXCEPTION location_not_accessible 'Storage location is not accessible';
```

#### **User Management Exceptions**
```sql
-- User authentication and authorization exceptions
CREATE EXCEPTION password_too_weak 'Password does not meet security requirements';

CREATE EXCEPTION account_locked 'User account is locked due to multiple failed login attempts';

CREATE EXCEPTION session_expired 'User session has expired - please log in again';

CREATE EXCEPTION role_assignment_conflict 'Cannot assign conflicting roles to user';

CREATE EXCEPTION permission_denied 'User does not have required permissions';

CREATE EXCEPTION duplicate_username 'Username is already taken';
```

### **Hierarchical Schema Exceptions**

#### **3-Level Schema Qualification**
```sql
-- Enterprise application exception organization
CREATE EXCEPTION enterprise.finance.invalid_account_number 
    'Account number format is invalid for financial transactions';

CREATE EXCEPTION enterprise.finance.audit_trail_required 
    'Financial transaction requires complete audit trail';

CREATE EXCEPTION enterprise.hr.employee_not_active 
    'Employee record is not in active status';

CREATE EXCEPTION enterprise.hr.payroll_period_closed 
    'Payroll period is closed and cannot be modified';

-- Regional exception organization
CREATE EXCEPTION regions.northamerica.tax_calculation_error 
    'Tax calculation failed for North American region';

CREATE EXCEPTION regions.europe.vat_validation_failed 
    'VAT number validation failed for European transaction';

-- Department-specific exceptions
CREATE EXCEPTION departments.sales.quota_exceeded 
    'Sales quota has been exceeded for this period';

CREATE EXCEPTION departments.manufacturing.quality_check_failed 
    'Product failed quality control checks';

CREATE EXCEPTION departments.logistics.shipping_address_invalid 
    'Shipping address validation failed';
```

### **Conditional Exception Creation**

#### **IF NOT EXISTS Usage**
```sql
-- Safe exception creation for deployment scripts
CREATE EXCEPTION IF NOT EXISTS validation_error 'Data validation failed';

CREATE EXCEPTION IF NOT EXISTS business_rule_violation 'Business rule violation detected';

-- Schema-qualified conditional creation
CREATE EXCEPTION IF NOT EXISTS application.core.system_error 'System error occurred';

CREATE EXCEPTION IF NOT EXISTS application.core.configuration_invalid 'Configuration is invalid';
```

---

## ALTER EXCEPTION

Modifies the error message of an existing exception.

### **ALTER EXCEPTION Syntax**
```sql
ALTER EXCEPTION [schema_name.]exception_name 'new_error_message';
```

### **ALTER EXCEPTION Examples**

#### **Updating Exception Messages**
```sql
-- Update exception message for clarity
ALTER EXCEPTION insufficient_balance 
    'Account balance is insufficient. Current balance: @1, Required: @2';

-- Enhance message with additional context
ALTER EXCEPTION invalid_customer_id 
    'Customer ID @1 does not exist. Please verify the customer number.';

-- Add parameter support to existing exception
ALTER EXCEPTION product_not_available 
    'Product @1 is not available. Expected availability: @2';

-- Update message for internationalization
ALTER EXCEPTION unauthorized_access 
    'Access denied. User @1 lacks required permission: @2';
```

#### **Message Enhancement for Business Rules**
```sql
-- Enhance financial exception messages
ALTER EXCEPTION credit_limit_exceeded 
    'Transaction amount @1 exceeds credit limit @2. Available credit: @3';

ALTER EXCEPTION transaction_limit_exceeded 
    'Daily transaction limit @1 exceeded. Current total: @2';

-- Improve inventory exception messages
ALTER EXCEPTION inventory_shortage 
    'Insufficient inventory: Available @1, Required @2, Product: @3';

ALTER EXCEPTION warehouse_capacity_exceeded 
    'Warehouse @1 capacity exceeded. Current: @2, Maximum: @3, Requested: @4';
```

#### **Schema-Qualified Exception Updates**
```sql
-- Update exceptions in specific schemas
ALTER EXCEPTION enterprise.finance.invalid_account_number 
    'Invalid account number format: @1. Expected format: @2';

ALTER EXCEPTION regions.europe.vat_validation_failed 
    'VAT number @1 validation failed for country @2. Error: @3';

ALTER EXCEPTION departments.hr.employee_not_active 
    'Employee @1 status is @2. Active status required for operation @3';
```

---

## DROP EXCEPTION

Removes exception definitions from the database. Exceptions can only be dropped if no procedures, functions, or triggers reference them.

### **DROP EXCEPTION Syntax**
```sql
DROP EXCEPTION [IF EXISTS] [schema_name.]exception_name;
```

### **DROP EXCEPTION Examples**

#### **Basic Exception Removal**
```sql
-- Drop specific exceptions
DROP EXCEPTION insufficient_balance;

DROP EXCEPTION invalid_customer_id;

-- Safe dropping with IF EXISTS
DROP EXCEPTION IF EXISTS outdated_validation_error;

DROP EXCEPTION IF EXISTS legacy_business_rule;
```

#### **Schema-Qualified Exception Removal**
```sql
-- Drop exceptions from specific schemas
DROP EXCEPTION enterprise.finance.old_calculation_error;

DROP EXCEPTION IF EXISTS regions.legacy.deprecated_validation;

DROP EXCEPTION departments.temp.test_exception;
```

#### **Dependency Checking Before Drop**
```sql
-- Check for exception dependencies before dropping
SELECT DISTINCT
    rf.RDB$DEPENDENT_NAME as DEPENDENT_OBJECT,
    CASE rf.RDB$DEPENDENT_TYPE
        WHEN 5 THEN 'PROCEDURE'
        WHEN 15 THEN 'FUNCTION'
        WHEN 2 THEN 'TRIGGER'
        ELSE 'OTHER'
    END as DEPENDENT_TYPE
FROM RDB$DEPENDENCIES rf
WHERE rf.RDB$DEPENDED_ON_NAME = 'INSUFFICIENT_BALANCE'
  AND rf.RDB$DEPENDED_ON_TYPE = 7; -- obj_exception

-- Drop exception after confirming no dependencies
DROP EXCEPTION insufficient_balance;
```

---

## RECREATE EXCEPTION

Combines DROP and CREATE operations in a single atomic transaction.

### **RECREATE EXCEPTION Syntax**
```sql
RECREATE EXCEPTION [schema_name.]exception_name 'error_message';
```

### **RECREATE EXCEPTION Examples**

#### **Exception Recreation for Updates**
```sql
-- Recreate exception with improved message
RECREATE EXCEPTION validation_error 
    'Data validation failed for field @1. Expected format: @2, Received: @3';

-- Recreate with parameter support
RECREATE EXCEPTION business_rule_violation 
    'Business rule @1 violated. Current value: @2, Required: @3';

-- Recreate financial exception
RECREATE EXCEPTION payment_processing_failed 
    'Payment processing failed. Transaction ID: @1, Error Code: @2, Message: @3';
```

#### **Schema-Qualified Recreation**
```sql
-- Recreate enterprise-level exceptions
RECREATE EXCEPTION enterprise.security.access_violation 
    'Security violation: User @1 attempted unauthorized action @2 on resource @3';

RECREATE EXCEPTION enterprise.compliance.audit_requirement_failed 
    'Audit requirement @1 not met for transaction @2. Details: @3';
```

---

## Using Exceptions in PSQL Code

### **Raising Exceptions in Procedures and Functions**

#### **Basic Exception Raising**
```sql
-- Create procedure that raises exceptions
CREATE PROCEDURE validate_customer_order(
    customer_id INTEGER,
    order_amount DECIMAL(10,2)
)
AS
    DECLARE customer_balance DECIMAL(10,2);
    DECLARE credit_limit DECIMAL(10,2);
BEGIN
    -- Check if customer exists
    SELECT account_balance, credit_limit
    FROM customers 
    WHERE customer_id = :customer_id
    INTO :customer_balance, :credit_limit;
    
    IF (ROW_COUNT = 0) THEN
        EXCEPTION invalid_customer_id;
    
    -- Check sufficient balance
    IF (customer_balance < order_amount) THEN
        EXCEPTION insufficient_balance;
    
    -- Check credit limit
    IF (order_amount > credit_limit) THEN
        EXCEPTION credit_limit_exceeded;
        
    -- Order is valid
    INSERT INTO order_validation_log (customer_id, order_amount, validation_date)
    VALUES (:customer_id, :order_amount, CURRENT_TIMESTAMP);
END;
```

#### **Exceptions with Parameters**
```sql
-- Create procedure using parameterized exceptions
CREATE PROCEDURE process_inventory_transaction(
    product_id INTEGER,
    quantity_requested INTEGER,
    transaction_type VARCHAR(20)
)
AS
    DECLARE available_quantity INTEGER;
    DECLARE product_name VARCHAR(100);
    DECLARE warehouse_location VARCHAR(50);
BEGIN
    -- Get product information
    SELECT p.product_name, i.available_quantity, i.warehouse_location
    FROM products p
    JOIN inventory i ON p.product_id = i.product_id
    WHERE p.product_id = :product_id
    INTO :product_name, :available_quantity, :warehouse_location;
    
    IF (ROW_COUNT = 0) THEN
        EXCEPTION record_not_found USING (:product_id, 'products');
    
    -- Check inventory availability for outbound transactions
    IF (transaction_type = 'OUTBOUND' AND available_quantity < quantity_requested) THEN
        EXCEPTION inventory_shortage USING (:product_name, :available_quantity, :quantity_requested);
    
    -- Check warehouse capacity for inbound transactions
    IF (transaction_type = 'INBOUND') THEN
    BEGIN
        DECLARE warehouse_capacity INTEGER;
        DECLARE current_stock INTEGER;
        
        SELECT max_capacity, current_stock
        FROM warehouse_capacity 
        WHERE location = :warehouse_location
        INTO :warehouse_capacity, :current_stock;
        
        IF (current_stock + quantity_requested > warehouse_capacity) THEN
            EXCEPTION warehouse_capacity_exceeded USING (
                :warehouse_location, 
                :current_stock, 
                :warehouse_capacity, 
                :quantity_requested
            );
    END
    
    -- Process the transaction
    IF (transaction_type = 'INBOUND') THEN
        UPDATE inventory 
        SET available_quantity = available_quantity + :quantity_requested
        WHERE product_id = :product_id;
    ELSE
        UPDATE inventory 
        SET available_quantity = available_quantity - :quantity_requested
        WHERE product_id = :product_id;
    
    -- Log successful transaction
    INSERT INTO inventory_transactions (product_id, quantity_change, transaction_type, processed_date)
    VALUES (:product_id, :quantity_requested, :transaction_type, CURRENT_TIMESTAMP);
END;
```

### **Exception Handling in PSQL**

#### **Exception Handling Blocks**
```sql
-- Create procedure with comprehensive exception handling
CREATE PROCEDURE safe_customer_update(
    customer_id INTEGER,
    new_email VARCHAR(255),
    new_credit_limit DECIMAL(10,2)
)
AS
    DECLARE error_message VARCHAR(500);
    DECLARE error_context VARCHAR(200);
BEGIN
    BEGIN
        -- Validate email format
        IF (new_email NOT SIMILAR TO '%@%\.%') THEN
            EXCEPTION format_validation_error USING (:new_email, 'email format');
        
        -- Check for duplicate email
        IF (EXISTS(SELECT 1 FROM customers WHERE email = :new_email AND customer_id <> :customer_id)) THEN
            EXCEPTION duplicate_email_address;
        
        -- Validate credit limit
        IF (new_credit_limit < 0) THEN
            EXCEPTION invalid_parameter_value USING ('credit_limit must be non-negative');
        
        -- Perform update
        UPDATE customers 
        SET email = :new_email, 
            credit_limit = :new_credit_limit,
            last_modified = CURRENT_TIMESTAMP
        WHERE customer_id = :customer_id;
        
        IF (ROW_COUNT = 0) THEN
            EXCEPTION invalid_customer_id;
        
        -- Log successful update
        INSERT INTO customer_audit_log (customer_id, operation, performed_by, performed_date)
        VALUES (:customer_id, 'UPDATE', USER, CURRENT_TIMESTAMP);
        
    WHEN insufficient_balance DO
    BEGIN
        error_message = 'Credit limit validation failed';
        error_context = 'Customer ID: ' || :customer_id;
        INSERT INTO error_log (error_type, error_message, error_context, error_date)
        VALUES ('BUSINESS_RULE', :error_message, :error_context, CURRENT_TIMESTAMP);
        -- Re-raise the exception
        EXCEPTION insufficient_balance;
    END
    
    WHEN duplicate_email_address DO
    BEGIN
        error_message = 'Email address conflict detected';
        error_context = 'Customer ID: ' || :customer_id || ', Email: ' || :new_email;
        INSERT INTO error_log (error_type, error_message, error_context, error_date)
        VALUES ('DATA_CONFLICT', :error_message, :error_context, CURRENT_TIMESTAMP);
        -- Re-raise with additional context
        EXCEPTION duplicate_email_address;
    END
    
    WHEN format_validation_error DO
    BEGIN
        error_message = 'Input format validation failed';
        error_context = 'Customer ID: ' || :customer_id || ', Field: email';
        INSERT INTO error_log (error_type, error_message, error_context, error_date)
        VALUES ('VALIDATION_ERROR', :error_message, :error_context, CURRENT_TIMESTAMP);
        -- Re-raise the exception
        EXCEPTION format_validation_error;
    END
    
    WHEN ANY DO
    BEGIN
        -- Handle unexpected exceptions
        error_message = 'Unexpected error during customer update';
        error_context = 'Customer ID: ' || :customer_id || ', SQL Code: ' || SQLCODE || ', SQL State: ' || SQLSTATE;
        INSERT INTO error_log (error_type, error_message, error_context, error_date)
        VALUES ('SYSTEM_ERROR', :error_message, :error_context, CURRENT_TIMESTAMP);
        -- Re-raise as generic system error
        EXCEPTION data_validation_failed;
    END
END;
```

#### **Exception Chaining and Context**
```sql
-- Create function with nested exception handling
CREATE FUNCTION calculate_compound_interest(
    principal DECIMAL(15,2),
    annual_rate DECIMAL(5,4),
    compounding_periods INTEGER,
    years INTEGER
) RETURNS DECIMAL(15,2)
AS
    DECLARE result DECIMAL(15,2);
    DECLARE calculation_context VARCHAR(200);
BEGIN
    BEGIN
        -- Input validation
        IF (principal <= 0) THEN
            EXCEPTION invalid_parameter_value USING ('principal must be positive');
        
        IF (annual_rate < 0) THEN
            EXCEPTION invalid_parameter_value USING ('annual_rate cannot be negative');
        
        IF (compounding_periods <= 0) THEN
            EXCEPTION invalid_parameter_value USING ('compounding_periods must be positive');
        
        IF (years <= 0) THEN
            EXCEPTION invalid_parameter_value USING ('years must be positive');
        
        -- Perform calculation
        calculation_context = 'P=' || principal || ', R=' || annual_rate || ', N=' || compounding_periods || ', T=' || years;
        
        BEGIN
            result = principal * POWER(1 + (annual_rate / compounding_periods), compounding_periods * years);
            
            -- Check for calculation overflow
            IF (result > 999999999999.99) THEN
                EXCEPTION calculation_overflow_error USING (
                    CAST(principal AS VARCHAR(20)), 
                    CAST(annual_rate AS VARCHAR(10)), 
                    CAST(years AS VARCHAR(10)), 
                    '999999999999.99'
                );
            
        WHEN ANY DO
        BEGIN
            -- Calculation error occurred
            EXCEPTION calculation_overflow_error USING (
                'Compound interest calculation', 
                :calculation_context, 
                'CALCULATION_ERROR', 
                SQLCODE
            );
        END
        END
        
        RETURN result - principal; -- Return interest only
        
    WHEN invalid_parameter_value DO
    BEGIN
        -- Log parameter validation error
        INSERT INTO calculation_error_log (function_name, parameters, error_type, error_date)
        VALUES ('calculate_compound_interest', :calculation_context, 'PARAMETER_VALIDATION', CURRENT_TIMESTAMP);
        -- Re-raise the exception
        EXCEPTION invalid_parameter_value;
    END
    
    WHEN calculation_overflow_error DO
    BEGIN
        -- Log calculation overflow
        INSERT INTO calculation_error_log (function_name, parameters, error_type, error_date)
        VALUES ('calculate_compound_interest', :calculation_context, 'OVERFLOW', CURRENT_TIMESTAMP);
        -- Re-raise the exception
        EXCEPTION calculation_overflow_error;
    END
END;
```

### **Exception Usage in Triggers**

#### **Data Validation Triggers with Exceptions**
```sql
-- Create trigger that uses exceptions for data validation
CREATE TRIGGER tr_validate_customer_data
    BEFORE INSERT OR UPDATE ON customers
AS
    DECLARE validation_errors VARCHAR(1000) = '';
    DECLARE error_count INTEGER = 0;
BEGIN
    -- Validate email format
    IF (NEW.email IS NOT NULL AND NEW.email NOT SIMILAR TO '%@%\.%') THEN
    BEGIN
        validation_errors = validation_errors || 'Invalid email format; ';
        error_count = error_count + 1;
    END
    
    -- Validate phone number format
    IF (NEW.phone IS NOT NULL AND NEW.phone NOT SIMILAR TO '[0-9\-\(\)\s\+]{10,}') THEN
    BEGIN
        validation_errors = validation_errors || 'Invalid phone format; ';
        error_count = error_count + 1;
    END
    
    -- Validate credit limit
    IF (NEW.credit_limit < 0) THEN
    BEGIN
        validation_errors = validation_errors || 'Credit limit cannot be negative; ';
        error_count = error_count + 1;
    END
    
    -- Validate age
    IF (NEW.date_of_birth IS NOT NULL AND NEW.date_of_birth > CURRENT_DATE - INTERVAL '18' YEAR) THEN
    BEGIN
        validation_errors = validation_errors || 'Customer must be at least 18 years old; ';
        error_count = error_count + 1;
    END
    
    -- Raise exception if validation errors found
    IF (error_count > 0) THEN
        EXCEPTION data_validation_failed USING (:validation_errors);
    
    -- Check for duplicate email on INSERT
    IF (INSERTING AND EXISTS(SELECT 1 FROM customers WHERE email = NEW.email)) THEN
        EXCEPTION duplicate_email_address;
    
    -- Check for duplicate email on UPDATE (excluding current record)
    IF (UPDATING AND EXISTS(SELECT 1 FROM customers WHERE email = NEW.email AND customer_id <> NEW.customer_id)) THEN
        EXCEPTION duplicate_email_address;
END;
```

#### **Business Rule Enforcement Triggers**
```sql
-- Create trigger for order validation with exceptions
CREATE TRIGGER tr_validate_order
    BEFORE INSERT OR UPDATE ON orders
AS
    DECLARE customer_credit_limit DECIMAL(10,2);
    DECLARE customer_balance DECIMAL(10,2);
    DECLARE customer_status VARCHAR(20);
    DECLARE order_total DECIMAL(10,2);
BEGIN
    -- Get customer information
    SELECT credit_limit, account_balance, status
    FROM customers
    WHERE customer_id = NEW.customer_id
    INTO :customer_credit_limit, :customer_balance, :customer_status;
    
    IF (ROW_COUNT = 0) THEN
        EXCEPTION invalid_customer_id;
    
    -- Check customer status
    IF (customer_status <> 'ACTIVE') THEN
        EXCEPTION unauthorized_access USING (NEW.customer_id, customer_status);
    
    -- Calculate order total
    SELECT SUM(quantity * unit_price)
    FROM order_items
    WHERE order_id = NEW.order_id
    INTO :order_total;
    
    order_total = COALESCE(order_total, 0);
    
    -- Check credit limit
    IF (order_total > customer_credit_limit) THEN
        EXCEPTION credit_limit_exceeded USING (
            CAST(order_total AS VARCHAR(20)), 
            CAST(customer_credit_limit AS VARCHAR(20)), 
            CAST(customer_credit_limit - order_total AS VARCHAR(20))
        );
    
    -- Check account balance for immediate payment orders
    IF (NEW.payment_terms = 'IMMEDIATE' AND order_total > customer_balance) THEN
        EXCEPTION insufficient_balance USING (
            CAST(customer_balance AS VARCHAR(20)), 
            CAST(order_total AS VARCHAR(20))
        );
    
    -- Validate order date
    IF (NEW.order_date > CURRENT_DATE) THEN
        EXCEPTION invalid_date_range USING ('Order date cannot be in the future');
END;
```

---

## System Catalog Integration

ScratchBird stores exception definitions in the RDB$EXCEPTIONS system table.

### **Querying Exception Information**

#### **List All Exceptions**
```sql
-- Show all exceptions in the database
SELECT 
    RDB$EXCEPTION_NAME as EXCEPTION_NAME,
    COALESCE(RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME,
    RDB$EXCEPTION_NUMBER as EXCEPTION_NUMBER,
    RDB$MESSAGE as MESSAGE,
    RDB$OWNER as OWNER,
    CASE RDB$SYSTEM_FLAG
        WHEN 1 THEN 'SYSTEM'
        WHEN 0 THEN 'USER_DEFINED'
        ELSE 'UNKNOWN'
    END as EXCEPTION_TYPE,
    RDB$DESCRIPTION as DESCRIPTION
FROM RDB$EXCEPTIONS
ORDER BY SCHEMA_NAME, EXCEPTION_NAME;
```

#### **Exceptions by Schema**
```sql
-- List exceptions grouped by schema hierarchy
SELECT 
    COALESCE(RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME,
    COUNT(*) as EXCEPTION_COUNT,
    STRING_AGG(RDB$EXCEPTION_NAME, ', ' ORDER BY RDB$EXCEPTION_NAME) as EXCEPTIONS
FROM RDB$EXCEPTIONS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
GROUP BY RDB$SCHEMA_NAME
ORDER BY SCHEMA_NAME;
```

#### **Exception Usage Analysis**
```sql
-- Find procedures, functions, and triggers using specific exceptions
SELECT DISTINCT
    d.RDB$DEPENDENT_NAME as DEPENDENT_OBJECT,
    CASE d.RDB$DEPENDENT_TYPE
        WHEN 5 THEN 'PROCEDURE'
        WHEN 15 THEN 'FUNCTION' 
        WHEN 2 THEN 'TRIGGER'
        WHEN 18 THEN 'PACKAGE'
        ELSE 'OTHER'
    END as DEPENDENT_TYPE,
    e.RDB$EXCEPTION_NAME as EXCEPTION_NAME,
    e.RDB$MESSAGE as EXCEPTION_MESSAGE
FROM RDB$DEPENDENCIES d
JOIN RDB$EXCEPTIONS e ON d.RDB$DEPENDED_ON_NAME = e.RDB$EXCEPTION_NAME
WHERE d.RDB$DEPENDED_ON_TYPE = 7  -- obj_exception
ORDER BY EXCEPTION_NAME, DEPENDENT_TYPE, DEPENDENT_OBJECT;
```

### **Exception Metadata Queries**

#### **Exception Numbers and Messages**
```sql
-- Show exception numbers and message details
SELECT 
    RDB$EXCEPTION_NAME,
    RDB$EXCEPTION_NUMBER,
    LENGTH(RDB$MESSAGE) as MESSAGE_LENGTH,
    RDB$MESSAGE,
    CASE 
        WHEN RDB$MESSAGE LIKE '%@%' THEN 'PARAMETERIZED'
        ELSE 'STATIC'
    END as MESSAGE_TYPE
FROM RDB$EXCEPTIONS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY RDB$EXCEPTION_NUMBER;
```

#### **Custom vs System Exceptions**
```sql
-- Distinguish between system and user-defined exceptions
SELECT 
    CASE RDB$SYSTEM_FLAG
        WHEN 1 THEN 'SYSTEM'
        WHEN 0 THEN 'USER_DEFINED'
        ELSE 'UNKNOWN'
    END as EXCEPTION_CATEGORY,
    COUNT(*) as EXCEPTION_COUNT,
    MIN(RDB$EXCEPTION_NUMBER) as MIN_NUMBER,
    MAX(RDB$EXCEPTION_NUMBER) as MAX_NUMBER
FROM RDB$EXCEPTIONS
GROUP BY RDB$SYSTEM_FLAG
ORDER BY RDB$SYSTEM_FLAG;
```

### **Exception Dependencies and Impact Analysis**

#### **Unused Exceptions**
```sql
-- Find exceptions that are not referenced by any code
SELECT 
    e.RDB$EXCEPTION_NAME,
    e.RDB$MESSAGE,
    COALESCE(e.RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME
FROM RDB$EXCEPTIONS e
WHERE (e.RDB$SYSTEM_FLAG = 0 OR e.RDB$SYSTEM_FLAG IS NULL)
  AND NOT EXISTS (
      SELECT 1 FROM RDB$DEPENDENCIES d
      WHERE d.RDB$DEPENDED_ON_NAME = e.RDB$EXCEPTION_NAME
        AND d.RDB$DEPENDED_ON_TYPE = 7
  )
ORDER BY SCHEMA_NAME, e.RDB$EXCEPTION_NAME;
```

#### **Most Used Exceptions**
```sql
-- Find most frequently referenced exceptions
SELECT 
    e.RDB$EXCEPTION_NAME,
    e.RDB$MESSAGE,
    COUNT(DISTINCT d.RDB$DEPENDENT_NAME) as USAGE_COUNT,
    STRING_AGG(DISTINCT 
        CASE d.RDB$DEPENDENT_TYPE
            WHEN 5 THEN 'PROCEDURE'
            WHEN 15 THEN 'FUNCTION'
            WHEN 2 THEN 'TRIGGER'
            ELSE 'OTHER'
        END, ', '
    ) as USED_IN_OBJECT_TYPES
FROM RDB$EXCEPTIONS e
LEFT JOIN RDB$DEPENDENCIES d ON e.RDB$EXCEPTION_NAME = d.RDB$DEPENDED_ON_NAME
    AND d.RDB$DEPENDED_ON_TYPE = 7
WHERE e.RDB$SYSTEM_FLAG = 0 OR e.RDB$SYSTEM_FLAG IS NULL
GROUP BY e.RDB$EXCEPTION_NAME, e.RDB$MESSAGE
HAVING COUNT(DISTINCT d.RDB$DEPENDENT_NAME) > 0
ORDER BY USAGE_COUNT DESC, e.RDB$EXCEPTION_NAME;
```

---

## Exception Security and Permissions

### **Granting and Revoking Exception Permissions**

#### **GRANT USAGE on Exceptions**
```sql
-- Grant usage permission on exception to user
GRANT USAGE ON EXCEPTION insufficient_balance TO user1;

-- Grant usage permission to role
GRANT USAGE ON EXCEPTION data_validation_failed TO role_application_user;

-- Grant usage with grant option
GRANT USAGE ON EXCEPTION business_rule_violation TO user2 WITH GRANT OPTION;

-- Grant usage to multiple users
GRANT USAGE ON EXCEPTION enterprise.finance.credit_limit_exceeded 
  TO user_finance1, user_finance2, role_finance_manager;
```

#### **REVOKE USAGE on Exceptions**
```sql
-- Revoke usage permission
REVOKE USAGE ON EXCEPTION insufficient_balance FROM user1;

-- Revoke usage from role
REVOKE USAGE ON EXCEPTION data_validation_failed FROM role_application_user;

-- Revoke grant option
REVOKE GRANT OPTION FOR USAGE ON EXCEPTION business_rule_violation FROM user2;
```

### **Exception Permission Analysis**

#### **Exception Permissions Query**
```sql
-- Show USAGE permissions on exceptions
SELECT 
    e.RDB$EXCEPTION_NAME as EXCEPTION_NAME,
    pr.RDB$USER as GRANTEE,
    pr.RDB$GRANTOR as GRANTOR,
    pr.RDB$PRIVILEGE as PRIVILEGE,
    CASE pr.RDB$GRANT_OPTION
        WHEN 1 THEN 'YES'
        ELSE 'NO'
    END as GRANT_OPTION
FROM RDB$EXCEPTIONS e
JOIN RDB$USER_PRIVILEGES pr ON e.RDB$EXCEPTION_NAME = pr.RDB$RELATION_NAME
WHERE pr.RDB$OBJECT_TYPE = 7  -- obj_exception
  AND pr.RDB$PRIVILEGE = 'U'  -- USAGE privilege
ORDER BY EXCEPTION_NAME, GRANTEE;
```

---

## Advanced Exception Features

### **Exception Message Formatting and Parameters**

#### **Dynamic Exception Messages**
```sql
-- Create function that formats exception parameters
CREATE FUNCTION format_validation_error(
    field_name VARCHAR(50),
    field_value VARCHAR(100),
    expected_format VARCHAR(100)
) RETURNS VARCHAR(500)
AS
    DECLARE formatted_message VARCHAR(500);
BEGIN
    formatted_message = 'Validation failed for field "' || field_name || 
                       '". Value "' || COALESCE(field_value, 'NULL') || 
                       '" does not match expected format: ' || expected_format;
    RETURN formatted_message;
END;

-- Use in procedure with custom exception handling
CREATE PROCEDURE validate_customer_input(
    customer_email VARCHAR(255),
    customer_phone VARCHAR(20),
    customer_ssn VARCHAR(11)
)
AS
    DECLARE error_message VARCHAR(500);
BEGIN
    -- Email validation
    IF (customer_email NOT SIMILAR TO '%@%\.%') THEN
    BEGIN
        error_message = format_validation_error('email', customer_email, 'user@domain.com');
        EXCEPTION format_validation_error USING (:error_message);
    END
    
    -- Phone validation
    IF (customer_phone NOT SIMILAR TO '[0-9\-\(\)\s\+]{10,}') THEN
    BEGIN
        error_message = format_validation_error('phone', customer_phone, '(XXX) XXX-XXXX');
        EXCEPTION format_validation_error USING (:error_message);
    END
    
    -- SSN validation
    IF (customer_ssn NOT SIMILAR TO '[0-9]{3}-[0-9]{2}-[0-9]{4}') THEN
    BEGIN
        error_message = format_validation_error('ssn', customer_ssn, 'XXX-XX-XXXX');
        EXCEPTION format_validation_error USING (:error_message);
    END
END;
```

### **Exception Logging and Monitoring**

#### **Comprehensive Exception Logging System**
```sql
-- Create exception logging infrastructure
CREATE TABLE exception_log (
    log_id INTEGER GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
    exception_name VARCHAR(63) NOT NULL,
    exception_message VARCHAR(1021),
    occurred_in_object VARCHAR(63),
    object_type VARCHAR(20),
    user_name VARCHAR(63),
    session_id INTEGER,
    transaction_id INTEGER,
    error_context VARCHAR(4000),
    stack_trace VARCHAR(8000),
    occurred_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    resolved_at TIMESTAMP,
    resolution_notes VARCHAR(1000)
);

-- Create logging procedure for exceptions
CREATE PROCEDURE log_exception_occurrence(
    exception_name VARCHAR(63),
    exception_message VARCHAR(1021),
    occurred_in_object VARCHAR(63),
    object_type VARCHAR(20),
    error_context VARCHAR(4000) DEFAULT NULL,
    stack_trace VARCHAR(8000) DEFAULT NULL
)
AS
BEGIN
    INSERT INTO exception_log (
        exception_name,
        exception_message,
        occurred_in_object,
        object_type,
        user_name,
        session_id,
        transaction_id,
        error_context,
        stack_trace
    ) VALUES (
        :exception_name,
        :exception_message,
        :occurred_in_object,
        :object_type,
        USER,
        CURRENT_CONNECTION,
        CURRENT_TRANSACTION,
        :error_context,
        :stack_trace
    );
END;

-- Enhanced procedure with automatic exception logging
CREATE PROCEDURE process_order_with_logging(
    customer_id INTEGER,
    product_id INTEGER,
    quantity INTEGER
)
AS
    DECLARE procedure_name VARCHAR(63) = 'process_order_with_logging';
    DECLARE error_context VARCHAR(4000);
BEGIN
    BEGIN
        -- Set error context
        error_context = 'Customer: ' || customer_id || ', Product: ' || product_id || ', Quantity: ' || quantity;
        
        -- Business logic validation
        IF (NOT EXISTS(SELECT 1 FROM customers WHERE customer_id = :customer_id AND status = 'ACTIVE')) THEN
            EXCEPTION invalid_customer_id;
        
        IF (NOT EXISTS(SELECT 1 FROM products WHERE product_id = :product_id)) THEN
            EXCEPTION record_not_found USING (:product_id, 'products');
        
        -- Check inventory
        IF ((SELECT available_quantity FROM inventory WHERE product_id = :product_id) < quantity) THEN
            EXCEPTION inventory_shortage;
        
        -- Process the order (placeholder)
        INSERT INTO orders (customer_id, order_date, status) 
        VALUES (:customer_id, CURRENT_DATE, 'PENDING');
        
    WHEN invalid_customer_id DO
    BEGIN
        EXECUTE PROCEDURE log_exception_occurrence(
            'invalid_customer_id', 
            'Customer ID does not exist or is not active',
            :procedure_name,
            'PROCEDURE',
            :error_context,
            NULL
        );
        EXCEPTION invalid_customer_id;
    END
    
    WHEN inventory_shortage DO
    BEGIN
        EXECUTE PROCEDURE log_exception_occurrence(
            'inventory_shortage',
            'Insufficient inventory to fulfill order',
            :procedure_name,
            'PROCEDURE',
            :error_context,
            NULL
        );
        EXCEPTION inventory_shortage;
    END
    
    WHEN ANY DO
    BEGIN
        EXECUTE PROCEDURE log_exception_occurrence(
            'system_error',
            'Unexpected system error: ' || SQLCODE || ' - ' || SQLSTATE,
            :procedure_name,
            'PROCEDURE',
            :error_context,
            'SQL Error Code: ' || SQLCODE || ', SQL State: ' || SQLSTATE
        );
        EXCEPTION data_validation_failed;
    END
END;
```

### **Exception Testing and Validation Framework**

#### **Exception Testing Infrastructure**
```sql
-- Create exception testing framework
CREATE TABLE exception_test_results (
    test_id INTEGER GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
    test_name VARCHAR(100) NOT NULL,
    exception_name VARCHAR(63) NOT NULL,
    expected_message VARCHAR(1021),
    actual_message VARCHAR(1021),
    test_status VARCHAR(20), -- 'PASSED', 'FAILED', 'ERROR'
    test_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    test_notes VARCHAR(500)
);

-- Exception testing procedure
CREATE PROCEDURE test_exception_behavior(
    test_name VARCHAR(100),
    test_procedure_name VARCHAR(63),
    expected_exception VARCHAR(63),
    test_parameters VARCHAR(1000) DEFAULT NULL
)
AS
    DECLARE actual_exception VARCHAR(63);
    DECLARE actual_message VARCHAR(1021);
    DECLARE test_status VARCHAR(20) = 'FAILED';
    DECLARE test_notes VARCHAR(500);
BEGIN
    BEGIN
        -- Execute the test procedure (this should be dynamic SQL in real implementation)
        -- For demonstration, we'll simulate different test scenarios
        
        IF (test_procedure_name = 'validate_customer_order') THEN
        BEGIN
            -- Simulate calling the procedure with test parameters
            EXECUTE PROCEDURE validate_customer_order(-1, 100.00); -- Invalid customer ID
        END
        
        -- If we reach here, no exception was raised
        test_status = 'FAILED';
        test_notes = 'Expected exception ' || expected_exception || ' was not raised';
        
    WHEN EXCEPTION invalid_customer_id DO
    BEGIN
        actual_exception = 'invalid_customer_id';
        actual_message = 'Customer ID does not exist in the system';
        
        IF (actual_exception = expected_exception) THEN
        BEGIN
            test_status = 'PASSED';
            test_notes = 'Exception raised as expected';
        END
        ELSE
        BEGIN
            test_status = 'FAILED';
            test_notes = 'Expected ' || expected_exception || ', got ' || actual_exception;
        END
    END
    
    WHEN ANY DO
    BEGIN
        actual_exception = 'unexpected_exception';
        actual_message = 'SQL Code: ' || SQLCODE || ', SQL State: ' || SQLSTATE;
        test_status = 'ERROR';
        test_notes = 'Unexpected exception occurred during test';
    END
    END
    
    -- Record test results
    INSERT INTO exception_test_results (
        test_name,
        exception_name,
        expected_message,
        actual_message,
        test_status,
        test_notes
    ) VALUES (
        :test_name,
        :expected_exception,
        'Expected exception message',
        :actual_message,
        :test_status,
        :test_notes
    );
END;

-- Test suite execution
CREATE PROCEDURE run_exception_test_suite
AS
BEGIN
    -- Test invalid customer scenarios
    EXECUTE PROCEDURE test_exception_behavior(
        'Invalid Customer ID Test',
        'validate_customer_order',
        'invalid_customer_id',
        'customer_id=-1, order_amount=100.00'
    );
    
    -- Test insufficient balance scenarios  
    EXECUTE PROCEDURE test_exception_behavior(
        'Insufficient Balance Test',
        'validate_customer_order', 
        'insufficient_balance',
        'customer_id=1001, order_amount=99999.99'
    );
    
    -- Generate test report
    SELECT 
        test_name,
        exception_name,
        test_status,
        test_notes,
        test_date
    FROM exception_test_results
    WHERE test_date >= CURRENT_DATE
    ORDER BY test_date DESC;
END;
```

---

## Error Handling and Troubleshooting

### **Common Exception Errors**

#### **Exception Creation Errors**
```sql
-- Error: Exception already exists
CREATE EXCEPTION duplicate_error 'This is a duplicate';
-- Solution: Use IF NOT EXISTS or ALTER EXCEPTION

-- Error: Message too long (over 1021 characters)
CREATE EXCEPTION long_message 'This message is too long...';
-- Solution: Shorten the message or split into multiple exceptions

-- Error: Invalid characters in exception name
CREATE EXCEPTION 'invalid-name-with-hyphens' 'Invalid name';
-- Solution: Use valid SQL identifiers (letters, numbers, underscores)
```

#### **Exception Usage Errors**
```sql
-- Error: Exception not found
EXCEPTION nonexistent_exception;
-- Solution: Create the exception or verify the name

-- Error: Permission denied
EXCEPTION restricted_exception;
-- Solution: Grant USAGE permission on the exception

-- Error: Cannot drop exception in use
DROP EXCEPTION referenced_exception;
-- Solution: Remove dependencies first
```

### **Debugging Exception Issues**

#### **Exception Dependency Analysis**
```sql
-- Find all references to a specific exception
SELECT 
    p.RDB$PROCEDURE_NAME as PROCEDURE_NAME,
    f.RDB$FUNCTION_NAME as FUNCTION_NAME,
    t.RDB$TRIGGER_NAME as TRIGGER_NAME,
    'References exception: ' || e.RDB$EXCEPTION_NAME as REFERENCE_TYPE
FROM RDB$EXCEPTIONS e
LEFT JOIN RDB$DEPENDENCIES d ON e.RDB$EXCEPTION_NAME = d.RDB$DEPENDED_ON_NAME
    AND d.RDB$DEPENDED_ON_TYPE = 7
LEFT JOIN RDB$PROCEDURES p ON d.RDB$DEPENDENT_NAME = p.RDB$PROCEDURE_NAME
    AND d.RDB$DEPENDENT_TYPE = 5
LEFT JOIN RDB$FUNCTIONS f ON d.RDB$DEPENDENT_NAME = f.RDB$FUNCTION_NAME
    AND d.RDB$DEPENDENT_TYPE = 15
LEFT JOIN RDB$TRIGGERS t ON d.RDB$DEPENDENT_NAME = t.RDB$TRIGGER_NAME
    AND d.RDB$DEPENDENT_TYPE = 2
WHERE e.RDB$EXCEPTION_NAME = 'YOUR_EXCEPTION_NAME';
```

#### **Exception Message Analysis**
```sql
-- Analyze exception message patterns
SELECT 
    RDB$EXCEPTION_NAME,
    RDB$MESSAGE,
    CASE 
        WHEN RDB$MESSAGE LIKE '%@1%' THEN 'Single Parameter'
        WHEN RDB$MESSAGE LIKE '%@2%' THEN 'Multiple Parameters'
        WHEN RDB$MESSAGE LIKE '%@%' THEN 'Parameterized'
        ELSE 'Static Message'
    END as MESSAGE_TYPE,
    LENGTH(RDB$MESSAGE) as MESSAGE_LENGTH
FROM RDB$EXCEPTIONS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY MESSAGE_TYPE, MESSAGE_LENGTH DESC;
```

### **Exception Migration and Maintenance**

#### **Exception Migration Procedures**
```sql
-- Safe exception migration procedure
CREATE PROCEDURE migrate_exception_message(
    exception_name VARCHAR(63),
    new_message VARCHAR(1021),
    backup_old_message BOOLEAN DEFAULT TRUE
)
AS
    DECLARE old_message VARCHAR(1021);
    DECLARE migration_date TIMESTAMP;
BEGIN
    -- Get current message
    SELECT RDB$MESSAGE FROM RDB$EXCEPTIONS 
    WHERE RDB$EXCEPTION_NAME = :exception_name
    INTO :old_message;
    
    IF (ROW_COUNT = 0) THEN
        EXCEPTION record_not_found USING (:exception_name, 'RDB$EXCEPTIONS');
    
    migration_date = CURRENT_TIMESTAMP;
    
    -- Backup old message if requested
    IF (backup_old_message) THEN
    BEGIN
        INSERT INTO exception_migration_log (
            exception_name,
            old_message,
            new_message,
            migration_date,
            performed_by
        ) VALUES (
            :exception_name,
            :old_message,
            :new_message,
            :migration_date,
            USER
        );
    END
    
    -- Update the exception
    EXECUTE STATEMENT 'ALTER EXCEPTION ' || exception_name || ' ''' || new_message || '''';
    
    -- Log successful migration
    INSERT INTO system_log (operation_type, object_name, details, performed_date)
    VALUES ('EXCEPTION_MIGRATION', :exception_name, 'Message updated successfully', :migration_date);
END;
```

---

## Best Practices

### **Exception Design Guidelines**

1. **Descriptive Names**: Use clear, descriptive exception names that indicate the error condition
2. **Consistent Messaging**: Maintain consistent message formats across related exceptions
3. **Parameter Usage**: Design messages to accept parameters for runtime context
4. **Schema Organization**: Group related exceptions in appropriate schemas
5. **Documentation**: Document exception usage and expected parameters
6. **Testing**: Implement comprehensive exception testing procedures

### **Recommended Exception Patterns**

#### **Domain-Specific Exception Hierarchies**
```sql
-- Financial domain exceptions
CREATE EXCEPTION finance.insufficient_funds 'Account balance insufficient for transaction';
CREATE EXCEPTION finance.invalid_currency 'Currency code @1 is not supported';
CREATE EXCEPTION finance.transaction_limit_exceeded 'Transaction amount @1 exceeds limit @2';

-- User management exceptions  
CREATE EXCEPTION security.authentication_failed 'Authentication failed for user @1';
CREATE EXCEPTION security.authorization_denied 'User @1 lacks permission for operation @2';
CREATE EXCEPTION security.session_expired 'User session expired after @1 minutes';

-- Data validation exceptions
CREATE EXCEPTION validation.required_field_missing 'Required field @1 is missing or empty';
CREATE EXCEPTION validation.invalid_format 'Field @1 format invalid. Expected: @2, Received: @3';
CREATE EXCEPTION validation.value_out_of_range 'Field @1 value @2 outside valid range @3 to @4';
```

#### **Consistent Exception Handling Patterns**
```sql
-- Standard exception handling template
CREATE PROCEDURE template_procedure_with_exceptions(
    input_parameter INTEGER
)
AS
    DECLARE procedure_name VARCHAR(63) = 'template_procedure_with_exceptions';
    DECLARE error_context VARCHAR(500);
BEGIN
    BEGIN
        -- Set error context for logging
        error_context = 'Input: ' || input_parameter;
        
        -- Business logic with exception raising
        IF (input_parameter < 0) THEN
            EXCEPTION invalid_parameter_value USING ('input_parameter must be non-negative');
        
        -- Main procedure logic here...
        
    WHEN invalid_parameter_value DO
    BEGIN
        -- Log the exception
        INSERT INTO error_log (procedure_name, error_type, error_context, error_date)
        VALUES (:procedure_name, 'PARAMETER_VALIDATION', :error_context, CURRENT_TIMESTAMP);
        -- Re-raise the exception
        EXCEPTION invalid_parameter_value;
    END
    
    WHEN ANY DO
    BEGIN
        -- Log unexpected exceptions
        INSERT INTO error_log (procedure_name, error_type, error_context, error_date)
        VALUES (:procedure_name, 'SYSTEM_ERROR', :error_context || ' SQL: ' || SQLCODE, CURRENT_TIMESTAMP);
        -- Raise generic exception
        EXCEPTION data_validation_failed;
    END
END;
```

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:1984-2010`
- **Grammar Rules**:
  - `exception_clause` (lines 1987-1991): CREATE EXCEPTION syntax
  - `alter_exception_clause` (lines 2003-2010): ALTER EXCEPTION syntax
  - `excp_statement` (lines 3883-3890): EXCEPTION statement in PSQL
- **Functionality**: Parsing CREATE/ALTER/DROP EXCEPTION with message strings

#### **DDL Node Classes**
- **File**: `src/dsql/DdlNodes.h:1070-1166`
- **Classes**:
  - `CreateAlterExceptionNode` (lines 1070-1121): Exception creation/modification
  - `DropExceptionNode` (lines 1124-1162): Exception removal
  - `RecreateExceptionNode` (lines 1165-1166): Atomic recreation operations

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:494-504`
- **Table**: `RDB$EXCEPTIONS` - Stores exception definitions
- **Fields**:
  - `f_xcp_name`: Exception name
  - `f_xcp_number`: Unique exception number
  - `f_xcp_msg`: Exception message text
  - `f_xcp_desc`: Exception description
  - `f_xcp_sys_flag`: System vs user-defined flag
  - `f_xcp_class`: Exception class information
  - `f_xcp_owner`: Exception owner
  - `f_xcp_schema`: Schema name for hierarchical support

#### **Exception Usage in PSQL**
- **File**: `src/dsql/parse.y:3883-3890`
- **Functionality**: EXCEPTION statement with parameter support
- Exception raising with USING clause for parameterized messages

### **Core Classes and Functions**

#### **CreateAlterExceptionNode Methods**
- `dsqlPass()`: DSQL compilation and name qualification
- `execute()`: Exception creation in system catalog with unique number assignment
- `checkPermission()`: Security validation for exception operations

#### **Exception Integration**
- **File**: `src/dsql/parse.y:1145-1151, 1439-1444`
- **Functionality**: GRANT/REVOKE USAGE ON EXCEPTION syntax
- **File**: `src/dsql/parse.y:4590-4592`
- **Functionality**: DDL trigger support for exception operations

### **Storage Structures**

Exceptions are stored in the RDB$EXCEPTIONS system table with:

- **Identity**: Name, unique number, schema qualification
- **Message**: Error message text with parameter placeholder support
- **Metadata**: Owner, system flag, class information
- **Schema Support**: Full hierarchical schema qualification
- **Dependencies**: Integration with RDB$DEPENDENCIES for usage tracking

---

## Administrative Operations

### **Exception Backup and Restore**

#### **Backup Considerations**
- Exception definitions are included in database backups as DDL
- Message text and parameter formats are preserved
- Unique exception numbers are maintained across backup/restore
- Dependencies on procedures/functions/triggers are preserved

#### **Restore Procedures**
```sql
-- Verify exceptions after restore
SELECT 
    RDB$EXCEPTION_NAME,
    RDB$EXCEPTION_NUMBER,
    RDB$MESSAGE,
    CASE RDB$SYSTEM_FLAG
        WHEN 1 THEN 'SYSTEM'
        ELSE 'USER'
    END as EXCEPTION_TYPE
FROM RDB$EXCEPTIONS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY RDB$EXCEPTION_NUMBER;

-- Test exception functionality
EXECUTE BLOCK AS
BEGIN
    EXCEPTION insufficient_balance;
WHEN insufficient_balance DO
BEGIN
    -- Exception handling works correctly
END
END;
```

### **Exception Maintenance**

#### **Regular Maintenance Tasks**
```sql
-- Monitor exception usage
CREATE VIEW exception_usage_summary AS
SELECT 
    e.RDB$EXCEPTION_NAME,
    e.RDB$MESSAGE,
    COALESCE(e.RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME,
    COUNT(DISTINCT d.RDB$DEPENDENT_NAME) as USAGE_COUNT,
    STRING_AGG(DISTINCT 
        CASE d.RDB$DEPENDENT_TYPE
            WHEN 5 THEN 'PROCEDURE'
            WHEN 15 THEN 'FUNCTION'
            WHEN 2 THEN 'TRIGGER'
            ELSE 'OTHER'
        END, ', '
    ) as USED_IN_TYPES
FROM RDB$EXCEPTIONS e
LEFT JOIN RDB$DEPENDENCIES d ON e.RDB$EXCEPTION_NAME = d.RDB$DEPENDED_ON_NAME
    AND d.RDB$DEPENDED_ON_TYPE = 7
WHERE e.RDB$SYSTEM_FLAG = 0 OR e.RDB$SYSTEM_FLAG IS NULL
GROUP BY e.RDB$EXCEPTION_NAME, e.RDB$MESSAGE, e.RDB$SCHEMA_NAME;

-- Check for orphaned exceptions
SELECT * FROM exception_usage_summary WHERE USAGE_COUNT = 0;
```

---

## Migration from Other Database Systems

### **From Oracle PL/SQL Exceptions**
Oracle user-defined exceptions translate to ScratchBird:
```sql
-- Oracle PL/SQL
DECLARE
    insufficient_balance EXCEPTION;
BEGIN
    RAISE insufficient_balance;
END;

-- ScratchBird equivalent
CREATE EXCEPTION insufficient_balance 'Account balance is insufficient';

-- Usage in PSQL
BEGIN
    EXCEPTION insufficient_balance;
END;
```

### **From PostgreSQL Custom Exceptions**
PostgreSQL RAISE EXCEPTION translates to ScratchBird:
```sql
-- PostgreSQL
RAISE EXCEPTION 'Invalid customer ID: %', customer_id;

-- ScratchBird approach
CREATE EXCEPTION invalid_customer_id 'Invalid customer ID: @1';
-- Usage:
EXCEPTION invalid_customer_id USING (:customer_id);
```

### **From SQL Server Custom Errors**
SQL Server RAISERROR can be replaced with ScratchBird exceptions:
```sql
-- SQL Server
RAISERROR('Insufficient balance: %d', 16, 1, @balance);

-- ScratchBird approach
CREATE EXCEPTION insufficient_balance 'Insufficient balance: @1';
-- Usage:
EXCEPTION insufficient_balance USING (:balance);
```

---

