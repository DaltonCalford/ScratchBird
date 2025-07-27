# ScratchBird PROCEDURE - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

PROCEDURE objects in ScratchBird are named collections of executable SQL statements and programming logic that can accept input parameters and return output parameters. Procedures are essential for implementing business logic, data processing workflows, and complex database operations within the database server. They provide powerful programming constructs for transaction control, error handling, and data manipulation.

### Key Features and Capabilities

- **Input/Output Parameters**: Define typed parameters for data exchange with calling applications
- **PSQL Programming Language**: Full programming language with variables, loops, conditions, and exceptions
- **Transaction Control**: Explicit transaction management within procedure logic
- **Exception Handling**: Robust error handling with custom and system exceptions
- **Security Modes**: SQL SECURITY DEFINER/INVOKER for execution privilege control
- **External Language Support**: Integration with external languages (Java, C++, etc.)
- **Local Subroutines**: Nested procedures and functions within procedure bodies
- **Context Variables**: Access to database and connection context information
- **Cursor Support**: Explicit cursors for complex result set processing

### ScratchBird-Specific Enhancements

1. **Hierarchical Schema Support**: Procedures support 3-level qualified names (`schema.subschema.procedure`)
2. **Enhanced Parameter System**: Rich parameter types including all ScratchBird data types
3. **Advanced Error Handling**: Extended exception system with custom exception types
4. **External Language Integration**: Comprehensive foreign language procedure support
5. **Package Integration**: Procedures can be grouped in packages for namespace organization
6. **IF NOT EXISTS Support**: Conditional creation syntax for deployment scripts
7. **RECREATE Support**: Atomic drop-and-recreate operations
8. **CREATE OR ALTER Support**: Flexible procedure modification syntax
9. **SQL Security Enhancements**: Granular security control with DEFINER/INVOKER modes

---

## DDL Syntax Reference

### CREATE PROCEDURE

Creates a new stored procedure with specified parameters and executable body.

#### **Basic PSQL Procedure Syntax**
```sql
CREATE [OR ALTER] PROCEDURE [IF NOT EXISTS] [schema_name.]procedure_name
    [(input_parameters)]
    [RETURNS (output_parameters)]
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
    -- Procedure body statements
END;
```

#### **External Procedure Syntax**
```sql
CREATE [OR ALTER] PROCEDURE [IF NOT EXISTS] [schema_name.]procedure_name
    [(input_parameters)]
    [RETURNS (output_parameters)]
EXTERNAL NAME 'external_name'
ENGINE engine_name
[AS 'external_body'];
```

#### **Complete Syntax with All Options**
```sql
CREATE [OR ALTER] PROCEDURE [IF NOT EXISTS] [[catalog.]schema.]procedure_name
    [([IN] parameter_name data_type [DEFAULT value]
      [, [IN] parameter_name data_type [DEFAULT value] ...])]
    [RETURNS ([OUT] parameter_name data_type
             [, [OUT] parameter_name data_type ...])]
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
        procedure_statements
    END
    |
    EXTERNAL NAME 'external_module_entry_point'
    ENGINE engine_name
    [AS 'external_body_source']
};
```

#### **Parameters**

- **OR ALTER**: Modify existing procedure or create if doesn't exist
- **IF NOT EXISTS**: Skip creation if procedure already exists (no error)
- **schema_name**: Optional schema qualification (supports hierarchical schemas)
- **procedure_name**: Procedure identifier (63 characters max)
- **input_parameters**: Comma-separated list of input parameters with types
- **output_parameters**: Comma-separated list of output parameters with types
- **SQL SECURITY**: Security context (DEFINER or INVOKER privileges)
- **DECLARE section**: Local variables, cursors, exceptions, and subroutines
- **procedure_statements**: PSQL executable statements

---

## CREATE PROCEDURE Examples

### **Basic PSQL Procedures**

#### **Simple Input/Output Procedure**
```sql
-- Basic procedure with input and output parameters
CREATE PROCEDURE calculate_tax(
    IN gross_amount DECIMAL(15,2),
    IN tax_rate DECIMAL(5,2)
)
RETURNS (
    OUT net_amount DECIMAL(15,2),
    OUT tax_amount DECIMAL(15,2)
)
AS
BEGIN
    tax_amount = gross_amount * (tax_rate / 100);
    net_amount = gross_amount - tax_amount;
END;

-- Usage example
EXECUTE PROCEDURE calculate_tax(1000.00, 8.25);
```

#### **Procedure with Local Variables**
```sql
-- Procedure with local variable declarations
CREATE PROCEDURE process_customer_order(
    IN customer_id INTEGER,
    IN order_date DATE
)
RETURNS (
    OUT order_id INTEGER,
    OUT total_amount DECIMAL(15,2)
)
AS
DECLARE
    order_count INTEGER DEFAULT 0;
    discount_rate DECIMAL(5,2) DEFAULT 0.00;
    customer_status VARCHAR(20);
BEGIN
    -- Get customer status for discount calculation
    SELECT status FROM customers 
    WHERE id = :customer_id
    INTO :customer_status;
    
    -- Determine discount based on customer status
    IF (customer_status = 'PREMIUM') THEN
        discount_rate = 10.00;
    ELSE IF (customer_status = 'VIP') THEN
        discount_rate = 15.00;
    ELSE
        discount_rate = 0.00;
    
    -- Create new order record
    INSERT INTO orders (customer_id, order_date, discount_rate)
    VALUES (:customer_id, :order_date, :discount_rate)
    RETURNING id, total_amount
    INTO :order_id, :total_amount;
END;
```

#### **Procedure with Exception Handling**
```sql
-- Procedure with custom exception handling
CREATE PROCEDURE transfer_funds(
    IN from_account INTEGER,
    IN to_account INTEGER,
    IN amount DECIMAL(15,2)
)
AS
DECLARE
    from_balance DECIMAL(15,2);
    to_balance DECIMAL(15,2);
    insufficient_funds EXCEPTION 'Insufficient funds for transfer';
    account_not_found EXCEPTION 'Account not found';
BEGIN
    -- Start transaction
    IN AUTONOMOUS TRANSACTION DO
    BEGIN
        -- Verify source account exists and has sufficient funds
        SELECT balance FROM accounts 
        WHERE account_id = :from_account 
        INTO :from_balance;
        
        IF (ROW_COUNT = 0) THEN
            EXCEPTION account_not_found;
        
        IF (from_balance < amount) THEN
            EXCEPTION insufficient_funds;
        
        -- Verify destination account exists
        SELECT balance FROM accounts 
        WHERE account_id = :to_account 
        INTO :to_balance;
        
        IF (ROW_COUNT = 0) THEN
            EXCEPTION account_not_found;
        
        -- Perform transfer
        UPDATE accounts SET balance = balance - :amount 
        WHERE account_id = :from_account;
        
        UPDATE accounts SET balance = balance + :amount 
        WHERE account_id = :to_account;
        
        -- Log transaction
        INSERT INTO transaction_log (from_account, to_account, amount, transaction_date)
        VALUES (:from_account, :to_account, :amount, CURRENT_TIMESTAMP);
    END
    
    WHEN EXCEPTION insufficient_funds DO
    BEGIN
        -- Log failed transaction
        INSERT INTO error_log (error_message, error_date)
        VALUES ('Transfer failed: Insufficient funds', CURRENT_TIMESTAMP);
        
        -- Re-raise exception
        EXCEPTION;
    END
    
    WHEN EXCEPTION account_not_found DO
    BEGIN
        INSERT INTO error_log (error_message, error_date)
        VALUES ('Transfer failed: Account not found', CURRENT_TIMESTAMP);
        EXCEPTION;
    END
END;
```

### **Advanced PSQL Procedures**

#### **Procedure with Cursors**
```sql
-- Procedure using explicit cursors for batch processing
CREATE PROCEDURE generate_monthly_report(
    IN report_month INTEGER,
    IN report_year INTEGER
)
RETURNS (
    OUT total_customers INTEGER,
    OUT total_revenue DECIMAL(15,2),
    OUT avg_order_value DECIMAL(15,2)
)
AS
DECLARE
    customer_cursor CURSOR FOR (
        SELECT c.customer_id, c.customer_name, 
               SUM(o.total_amount) as customer_total
        FROM customers c
        JOIN orders o ON c.customer_id = o.customer_id
        WHERE EXTRACT(MONTH FROM o.order_date) = :report_month
          AND EXTRACT(YEAR FROM o.order_date) = :report_year
        GROUP BY c.customer_id, c.customer_name
    );
    
    customer_id INTEGER;
    customer_name VARCHAR(100);
    customer_total DECIMAL(15,2);
    order_count INTEGER DEFAULT 0;
BEGIN
    total_customers = 0;
    total_revenue = 0.00;
    
    -- Process each customer's orders
    OPEN customer_cursor;
    
    WHILE (1 = 1) DO
    BEGIN
        FETCH customer_cursor 
        INTO :customer_id, :customer_name, :customer_total;
        
        IF (ROW_COUNT = 0) THEN
            LEAVE;
        
        total_customers = total_customers + 1;
        total_revenue = total_revenue + customer_total;
        order_count = order_count + 1;
        
        -- Insert customer summary
        INSERT INTO monthly_customer_summary 
        (report_month, report_year, customer_id, customer_name, total_spent)
        VALUES (:report_month, :report_year, :customer_id, :customer_name, :customer_total);
    END
    
    CLOSE customer_cursor;
    
    -- Calculate average order value
    IF (order_count > 0) THEN
        avg_order_value = total_revenue / order_count;
    ELSE
        avg_order_value = 0.00;
END;
```

#### **Procedure with Local Subroutines**
```sql
-- Procedure with nested local procedures and functions
CREATE PROCEDURE complex_data_processing(
    IN data_set_id INTEGER
)
AS
DECLARE
    -- Local function for data validation
    DECLARE FUNCTION validate_data_record(
        record_id INTEGER
    ) RETURNS BOOLEAN
    AS
    DECLARE
        error_count INTEGER;
    BEGIN
        SELECT COUNT(*) FROM data_validation_errors 
        WHERE record_id = :record_id
        INTO :error_count;
        
        RETURN (error_count = 0);
    END;
    
    -- Local procedure for data transformation
    DECLARE PROCEDURE transform_record(
        IN source_record_id INTEGER,
        OUT target_record_id INTEGER
    )
    AS
    DECLARE
        source_data VARCHAR(1000);
        transformed_data VARCHAR(1000);
    BEGIN
        SELECT raw_data FROM source_records 
        WHERE id = :source_record_id
        INTO :source_data;
        
        -- Apply transformation rules
        transformed_data = UPPER(source_data);
        transformed_data = REPLACE(transformed_data, '  ', ' ');
        
        INSERT INTO target_records (transformed_data, source_id)
        VALUES (:transformed_data, :source_record_id)
        RETURNING id INTO :target_record_id;
    END;
    
    -- Main procedure variables
    record_cursor CURSOR FOR (
        SELECT record_id FROM data_records 
        WHERE data_set_id = :data_set_id
    );
    current_record_id INTEGER;
    transformed_id INTEGER;
    processed_count INTEGER DEFAULT 0;
    
BEGIN
    -- Process each record in the data set
    OPEN record_cursor;
    
    WHILE (1 = 1) DO
    BEGIN
        FETCH record_cursor INTO :current_record_id;
        
        IF (ROW_COUNT = 0) THEN
            LEAVE;
        
        -- Validate record using local function
        IF (validate_data_record(:current_record_id)) THEN
        BEGIN
            -- Transform record using local procedure
            EXECUTE PROCEDURE transform_record(:current_record_id, :transformed_id);
            processed_count = processed_count + 1;
        END
        ELSE
        BEGIN
            -- Log validation error
            INSERT INTO processing_errors (data_set_id, record_id, error_type)
            VALUES (:data_set_id, :current_record_id, 'VALIDATION_FAILED');
        END
    END
    
    CLOSE record_cursor;
    
    -- Update processing statistics
    UPDATE data_sets 
    SET processed_records = :processed_count, 
        last_processed = CURRENT_TIMESTAMP
    WHERE id = :data_set_id;
END;
```

### **Hierarchical Schema Procedures**

#### **3-Level Schema Qualification**
```sql
-- Procedures in hierarchical schemas
CREATE PROCEDURE business.finance.accounting.calculate_depreciation(
    IN asset_id INTEGER,
    IN depreciation_method VARCHAR(20)
)
RETURNS (
    OUT annual_depreciation DECIMAL(15,2),
    OUT remaining_value DECIMAL(15,2)
)
AS
DECLARE
    asset_cost DECIMAL(15,2);
    useful_life INTEGER;
    accumulated_depreciation DECIMAL(15,2);
BEGIN
    -- Get asset details
    SELECT cost, useful_life_years, accumulated_depreciation
    FROM business.finance.assets 
    WHERE asset_id = :asset_id
    INTO :asset_cost, :useful_life, :accumulated_depreciation;
    
    -- Calculate depreciation based on method
    IF (depreciation_method = 'STRAIGHT_LINE') THEN
        annual_depreciation = asset_cost / useful_life;
    ELSE IF (depreciation_method = 'DECLINING_BALANCE') THEN
        annual_depreciation = (asset_cost - accumulated_depreciation) * 0.20;
    ELSE
        annual_depreciation = 0.00;
    
    remaining_value = asset_cost - accumulated_depreciation - annual_depreciation;
END;

-- HR procedures in different schema
CREATE PROCEDURE business.hr.payroll.calculate_employee_pay(
    IN employee_id INTEGER,
    IN pay_period_start DATE,
    IN pay_period_end DATE
)
RETURNS (
    OUT gross_pay DECIMAL(15,2),
    OUT net_pay DECIMAL(15,2),
    OUT total_deductions DECIMAL(15,2)
)
AS
DECLARE
    hourly_rate DECIMAL(10,2);
    hours_worked DECIMAL(8,2);
    overtime_hours DECIMAL(8,2);
    tax_deductions DECIMAL(15,2);
    benefit_deductions DECIMAL(15,2);
BEGIN
    -- Get employee pay rate
    SELECT rate_per_hour FROM business.hr.employees 
    WHERE id = :employee_id
    INTO :hourly_rate;
    
    -- Calculate hours worked
    SELECT SUM(hours_regular), SUM(hours_overtime)
    FROM business.hr.timesheets
    WHERE employee_id = :employee_id
      AND work_date BETWEEN :pay_period_start AND :pay_period_end
    INTO :hours_worked, :overtime_hours;
    
    -- Calculate gross pay (overtime at 1.5x rate)
    gross_pay = (hours_worked * hourly_rate) + (overtime_hours * hourly_rate * 1.5);
    
    -- Calculate deductions
    EXECUTE PROCEDURE business.hr.payroll.calculate_tax_deductions(
        :employee_id, :gross_pay, :tax_deductions);
    
    EXECUTE PROCEDURE business.hr.payroll.calculate_benefit_deductions(
        :employee_id, :benefit_deductions);
    
    total_deductions = tax_deductions + benefit_deductions;
    net_pay = gross_pay - total_deductions;
END;
```

### **External Language Procedures**

#### **Java External Procedure**
```sql
-- External procedure implemented in Java
CREATE PROCEDURE business.analytics.complex_calculation(
    IN input_data BLOB,
    IN calculation_type VARCHAR(50)
)
RETURNS (
    OUT result_data BLOB,
    OUT status_code INTEGER,
    OUT status_message VARCHAR(200)
)
EXTERNAL NAME 'BusinessAnalytics.complexCalculation'
ENGINE JAVA
AS '
    public static void complexCalculation(
        java.sql.Blob inputData,
        String calculationType,
        java.sql.Blob[] resultData,
        int[] statusCode,
        String[] statusMessage
    ) throws java.sql.SQLException {
        try {
            // Complex calculation logic here
            AnalyticsEngine engine = new AnalyticsEngine();
            byte[] input = inputData.getBytes(1, (int)inputData.length());
            
            CalculationResult result = engine.calculate(input, calculationType);
            
            resultData[0] = new javax.sql.rowset.serial.SerialBlob(result.getData());
            statusCode[0] = result.getStatusCode();
            statusMessage[0] = result.getStatusMessage();
            
        } catch (Exception e) {
            statusCode[0] = -1;
            statusMessage[0] = "Error: " + e.getMessage();
        }
    }
';

-- C++ External Procedure
CREATE PROCEDURE system.utilities.crypto_hash(
    IN input_text VARCHAR(8192),
    IN hash_algorithm VARCHAR(20)
)
RETURNS (
    OUT hash_value VARCHAR(128)
)
EXTERNAL NAME 'crypto_utils.calculate_hash'
ENGINE CPP;
```

### **Security-Aware Procedures**

#### **SQL SECURITY DEFINER/INVOKER**
```sql
-- Procedure that runs with definer privileges (secure)
CREATE PROCEDURE security.admin.reset_user_password(
    IN user_id INTEGER,
    IN new_password VARCHAR(100)
)
SQL SECURITY DEFINER
AS
DECLARE
    admin_user VARCHAR(50);
    password_hash VARCHAR(256);
BEGIN
    -- Only allow if current user has admin privileges
    admin_user = CURRENT_USER;
    
    IF (NOT EXISTS(SELECT 1 FROM admin_users WHERE username = :admin_user)) THEN
        EXCEPTION 'Access denied: Admin privileges required';
    
    -- Hash the password using secure function
    EXECUTE PROCEDURE security.crypto.hash_password(:new_password, :password_hash);
    
    -- Update user password
    UPDATE users 
    SET password_hash = :password_hash,
        password_changed_date = CURRENT_TIMESTAMP,
        force_password_change = 0
    WHERE id = :user_id;
    
    -- Log password change
    INSERT INTO security_audit_log (action, user_id, admin_user, timestamp)
    VALUES ('PASSWORD_RESET', :user_id, :admin_user, CURRENT_TIMESTAMP);
END;

-- Procedure that runs with invoker privileges (flexible)
CREATE PROCEDURE business.reporting.generate_user_report(
    IN report_type VARCHAR(50)
)
RETURNS (
    OUT report_data BLOB
)
SQL SECURITY INVOKER
AS
BEGIN
    -- This procedure runs with the privileges of the calling user
    -- Users can only see data they have access to
    
    IF (report_type = 'SALES') THEN
        SELECT report_data FROM generate_sales_report()
        INTO :report_data;
    ELSE IF (report_type = 'INVENTORY') THEN
        SELECT report_data FROM generate_inventory_report()
        INTO :report_data;
    ELSE
        EXCEPTION 'Invalid report type specified';
END;
```

### **Conditional Creation and Modification**

#### **IF NOT EXISTS and OR ALTER**
```sql
-- Safe procedure creation (no error if exists)
CREATE PROCEDURE IF NOT EXISTS utilities.log_event(
    IN event_type VARCHAR(50),
    IN event_message VARCHAR(500)
)
AS
BEGIN
    INSERT INTO system_log (event_type, message, logged_at)
    VALUES (:event_type, :event_message, CURRENT_TIMESTAMP);
END;

-- Create or modify existing procedure
CREATE OR ALTER PROCEDURE business.sales.calculate_commission(
    IN sales_person_id INTEGER,
    IN sales_amount DECIMAL(15,2)
)
RETURNS (
    OUT commission_amount DECIMAL(15,2)
)
AS
DECLARE
    commission_rate DECIMAL(5,2);
    sales_tier VARCHAR(20);
BEGIN
    -- Get sales person's commission tier
    SELECT tier FROM sales_people 
    WHERE id = :sales_person_id
    INTO :sales_tier;
    
    -- Updated commission calculation logic
    commission_rate = CASE sales_tier
        WHEN 'JUNIOR' THEN 3.00
        WHEN 'SENIOR' THEN 5.00
        WHEN 'MANAGER' THEN 7.50
        WHEN 'DIRECTOR' THEN 10.00
        ELSE 2.00
    END;
    
    commission_amount = sales_amount * (commission_rate / 100);
    
    -- Bonus for high sales amounts
    IF (sales_amount > 100000) THEN
        commission_amount = commission_amount * 1.25;
END;
```

---

## ALTER PROCEDURE

Modifies existing procedure definitions including parameters, security settings, and body implementation.

### **ALTER PROCEDURE Syntax**
```sql
ALTER PROCEDURE [schema_name.]procedure_name
    [(input_parameters)]
    [RETURNS (output_parameters)]
    [SQL SECURITY {DEFINER | INVOKER}]
AS
[DECLARE
    -- Local declarations
]
BEGIN
    -- Modified procedure body
END;
```

### **Partial ALTER PROCEDURE Syntax**
```sql
-- Change only SQL security mode
ALTER PROCEDURE [schema_name.]procedure_name
    [SQL SECURITY {DEFINER | INVOKER}];
```

### **ALTER PROCEDURE Examples**

#### **Complete Procedure Redefinition**
```sql
-- Alter procedure with new implementation
ALTER PROCEDURE business.finance.calculate_interest(
    IN principal DECIMAL(15,2),
    IN rate DECIMAL(5,2),
    IN periods INTEGER
)
RETURNS (
    OUT simple_interest DECIMAL(15,2),
    OUT compound_interest DECIMAL(15,2)
)
SQL SECURITY DEFINER
AS
BEGIN
    -- Updated calculation with both simple and compound interest
    simple_interest = principal * (rate / 100) * periods;
    compound_interest = principal * POWER((1 + rate / 100), periods) - principal;
END;
```

#### **Security Mode Change**
```sql
-- Change procedure security mode only
ALTER PROCEDURE business.reporting.monthly_summary
    SQL SECURITY INVOKER;

-- Change to definer mode for secure operations
ALTER PROCEDURE security.admin.grant_permissions
    SQL SECURITY DEFINER;
```

---

## RECREATE PROCEDURE

Drops and recreates a procedure in a single atomic operation, preserving dependencies where possible.

### **RECREATE PROCEDURE Syntax**
```sql
RECREATE PROCEDURE [schema_name.]procedure_name
    [(input_parameters)]
    [RETURNS (output_parameters)]
    [SQL SECURITY {DEFINER | INVOKER}]
AS
[DECLARE
    -- Local declarations
]
BEGIN
    -- Procedure body
END;
```

### **RECREATE PROCEDURE Examples**

#### **Complete Procedure Replacement**
```sql
-- Recreate procedure with entirely new implementation
RECREATE PROCEDURE business.inventory.update_stock_levels(
    IN product_id INTEGER,
    IN quantity_change INTEGER,
    IN transaction_type VARCHAR(20)
)
AS
DECLARE
    current_stock INTEGER;
    minimum_stock INTEGER;
    low_stock EXCEPTION 'Stock level below minimum threshold';
BEGIN
    -- Get current stock levels
    SELECT quantity_on_hand, minimum_quantity
    FROM inventory 
    WHERE product_id = :product_id
    INTO :current_stock, :minimum_stock;
    
    -- Apply stock change
    current_stock = current_stock + quantity_change;
    
    -- Check for low stock condition
    IF (current_stock < minimum_stock) THEN
    BEGIN
        -- Log warning
        INSERT INTO inventory_alerts (product_id, alert_type, alert_message)
        VALUES (:product_id, 'LOW_STOCK', 
                'Stock level (' || current_stock || ') below minimum (' || minimum_stock || ')');
        
        -- Raise exception for critical items
        IF (transaction_type = 'SALE' AND current_stock < 0) THEN
            EXCEPTION low_stock;
    END
    
    -- Update inventory
    UPDATE inventory 
    SET quantity_on_hand = :current_stock,
        last_updated = CURRENT_TIMESTAMP
    WHERE product_id = :product_id;
    
    -- Log transaction
    INSERT INTO inventory_transactions (product_id, transaction_type, quantity_change, timestamp)
    VALUES (:product_id, :transaction_type, :quantity_change, CURRENT_TIMESTAMP);
END;
```

---

## DROP PROCEDURE

Removes procedure definitions from the database. Procedures can only be dropped if no other database objects depend on them.

### **DROP PROCEDURE Syntax**
```sql
DROP PROCEDURE [IF EXISTS] [schema_name.]procedure_name;
```

### **DROP PROCEDURE Examples**

#### **Basic Procedure Removal**
```sql
-- Drop procedure if it exists
DROP PROCEDURE IF EXISTS temp_calculation;

-- Drop procedure from specific schema
DROP PROCEDURE business.finance.old_tax_calculation;

-- Drop hierarchical schema procedure
DROP PROCEDURE enterprise.americas.usa.regional_reporting;
```

#### **Dependency Management**
```sql
-- Check procedure dependencies before dropping
SELECT DISTINCT
    dep.RDB$DEPENDENT_NAME as DEPENDENT_OBJECT,
    dep.RDB$DEPENDENT_TYPE as OBJECT_TYPE
FROM RDB$DEPENDENCIES dep
WHERE dep.RDB$DEPENDED_ON_NAME = 'CALCULATE_COMMISSION'
  AND dep.RDB$DEPENDED_ON_TYPE = 5; -- Procedure type

-- Drop procedure after removing dependencies
DROP PROCEDURE calculate_commission;
```

---

## Procedure Execution and Usage

### **Executing Procedures**

#### **EXECUTE PROCEDURE Statement**
```sql
-- Execute procedure with input parameters
EXECUTE PROCEDURE calculate_tax(1000.00, 8.25);

-- Execute procedure with output parameters
EXECUTE PROCEDURE process_order(12345, CURRENT_DATE)
RETURNING_VALUES :order_id, :total_amount;

-- Execute procedure in SELECT statement (if returns single result)
SELECT order_id, total_amount 
FROM calculate_order_totals(12345);
```

#### **Procedure Calls in Applications**
```sql
-- Call procedure from another procedure
CREATE PROCEDURE master_order_processing(
    IN customer_id INTEGER
)
AS
DECLARE
    order_id INTEGER;
    tax_amount DECIMAL(15,2);
    final_total DECIMAL(15,2);
BEGIN
    -- Call sub-procedures
    EXECUTE PROCEDURE create_order(:customer_id)
    RETURNING_VALUES :order_id;
    
    EXECUTE PROCEDURE calculate_order_tax(:order_id)
    RETURNING_VALUES :tax_amount;
    
    EXECUTE PROCEDURE finalize_order(:order_id, :tax_amount)
    RETURNING_VALUES :final_total;
END;
```

### **Using Procedures in Packages**

#### **Package-Based Procedure Organization**
```sql
-- Define procedures in package specification
CREATE PACKAGE business.finance.calculations
AS
BEGIN
    PROCEDURE calculate_tax(
        IN amount DECIMAL(15,2),
        IN rate DECIMAL(5,2)
    ) RETURNS (
        OUT tax_amount DECIMAL(15,2)
    );
    
    PROCEDURE calculate_discount(
        IN customer_type VARCHAR(20),
        IN order_amount DECIMAL(15,2)
    ) RETURNS (
        OUT discount_amount DECIMAL(15,2)
    );
END;

-- Implement procedures in package body
CREATE PACKAGE BODY business.finance.calculations
AS
BEGIN
    PROCEDURE calculate_tax(
        IN amount DECIMAL(15,2),
        IN rate DECIMAL(5,2)
    ) RETURNS (
        OUT tax_amount DECIMAL(15,2)
    )
    AS
    BEGIN
        tax_amount = amount * (rate / 100);
    END;
    
    PROCEDURE calculate_discount(
        IN customer_type VARCHAR(20),
        IN order_amount DECIMAL(15,2)
    ) RETURNS (
        OUT discount_amount DECIMAL(15,2)
    )
    AS
    BEGIN
        discount_amount = CASE customer_type
            WHEN 'VIP' THEN order_amount * 0.15
            WHEN 'PREMIUM' THEN order_amount * 0.10
            WHEN 'STANDARD' THEN order_amount * 0.05
            ELSE 0.00
        END;
    END;
END;

-- Call package procedures
EXECUTE PROCEDURE business.finance.calculations.calculate_tax(1000.00, 8.25);
```

---

## System Catalog Integration

ScratchBird stores procedure definitions in the RDB$PROCEDURES system table and related metadata tables.

### **Querying Procedure Information**

#### **List All Procedures**
```sql
-- Show all user-defined procedures
SELECT 
    RDB$PROCEDURE_NAME as PROCEDURE_NAME,
    RDB$SCHEMA_NAME as SCHEMA_NAME,
    RDB$OWNER_NAME as OWNER,
    RDB$PROCEDURE_TYPE as TYPE,
    CASE RDB$SQL_SECURITY
        WHEN 0 THEN 'INVOKER'
        WHEN 1 THEN 'DEFINER'
        ELSE 'INHERITED'
    END as SQL_SECURITY,
    RDB$DESCRIPTION as DESCRIPTION,
    CASE WHEN RDB$PROCEDURE_SOURCE IS NOT NULL THEN 'PSQL' ELSE 'EXTERNAL' END as IMPLEMENTATION
FROM RDB$PROCEDURES
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY RDB$SCHEMA_NAME, RDB$PROCEDURE_NAME;
```

#### **Procedure Parameters**
```sql
-- List procedure parameters
SELECT 
    p.RDB$PROCEDURE_NAME,
    pp.RDB$PARAMETER_NAME,
    pp.RDB$PARAMETER_NUMBER,
    CASE pp.RDB$PARAMETER_TYPE
        WHEN 0 THEN 'INPUT'
        WHEN 1 THEN 'OUTPUT'
        ELSE 'UNKNOWN'
    END as PARAMETER_TYPE,
    f.RDB$FIELD_TYPE,
    f.RDB$FIELD_LENGTH,
    f.RDB$FIELD_SCALE,
    f.RDB$NULL_FLAG,
    pp.RDB$DEFAULT_SOURCE as DEFAULT_VALUE
FROM RDB$PROCEDURES p
JOIN RDB$PROCEDURE_PARAMETERS pp ON p.RDB$PROCEDURE_NAME = pp.RDB$PROCEDURE_NAME
JOIN RDB$FIELDS f ON pp.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME
WHERE p.RDB$SYSTEM_FLAG = 0 OR p.RDB$SYSTEM_FLAG IS NULL
ORDER BY p.RDB$PROCEDURE_NAME, pp.RDB$PARAMETER_TYPE, pp.RDB$PARAMETER_NUMBER;
```

#### **Procedure Dependencies**
```sql
-- Find procedure dependencies
SELECT 
    p.RDB$PROCEDURE_NAME as PROCEDURE_NAME,
    dep.RDB$DEPENDED_ON_NAME as DEPENDS_ON,
    CASE dep.RDB$DEPENDED_ON_TYPE
        WHEN 0 THEN 'TABLE'
        WHEN 1 THEN 'VIEW'
        WHEN 2 THEN 'TRIGGER'
        WHEN 5 THEN 'PROCEDURE'
        WHEN 15 THEN 'USER_FUNCTION'
        ELSE 'OTHER'
    END as DEPENDENCY_TYPE
FROM RDB$PROCEDURES p
JOIN RDB$DEPENDENCIES dep ON p.RDB$PROCEDURE_NAME = dep.RDB$DEPENDENT_NAME
WHERE p.RDB$SYSTEM_FLAG = 0 OR p.RDB$SYSTEM_FLAG IS NULL
  AND dep.RDB$DEPENDENT_TYPE = 5  -- Procedure type
ORDER BY p.RDB$PROCEDURE_NAME, dep.RDB$DEPENDED_ON_NAME;
```

### **Procedure Source Code**
```sql
-- View procedure source code
SELECT 
    RDB$PROCEDURE_NAME,
    RDB$PROCEDURE_SOURCE
FROM RDB$PROCEDURES
WHERE RDB$PROCEDURE_NAME = 'CALCULATE_TAX'
  AND (RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL);
```

### **Schema-Aware Procedure Queries**

#### **Procedures by Schema**
```sql
-- List procedures grouped by schema hierarchy
SELECT 
    COALESCE(RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME,
    COUNT(*) as PROCEDURE_COUNT,
    COUNT(CASE WHEN RDB$PROCEDURE_SOURCE IS NOT NULL THEN 1 END) as PSQL_PROCEDURES,
    COUNT(CASE WHEN RDB$PROCEDURE_SOURCE IS NULL THEN 1 END) as EXTERNAL_PROCEDURES
FROM RDB$PROCEDURES
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
GROUP BY RDB$SCHEMA_NAME
ORDER BY SCHEMA_NAME;
```

---

## Advanced Procedure Features

### **Context Variables and Built-in Functions**

#### **Using Context Variables**
```sql
-- Procedure using database context information
CREATE PROCEDURE audit.system.log_user_activity(
    IN activity_type VARCHAR(50),
    IN activity_details VARCHAR(500)
)
AS
DECLARE
    current_user_name VARCHAR(63);
    current_connection_id BIGINT;
    current_transaction_id BIGINT;
    client_address VARCHAR(255);
    client_process VARCHAR(255);
BEGIN
    -- Get context information
    current_user_name = CURRENT_USER;
    current_connection_id = CURRENT_CONNECTION;
    current_transaction_id = CURRENT_TRANSACTION;
    client_address = RDB$GET_CONTEXT('SYSTEM', 'CLIENT_ADDRESS');
    client_process = RDB$GET_CONTEXT('SYSTEM', 'CLIENT_PROCESS');
    
    -- Log activity
    INSERT INTO user_activity_log (
        username, connection_id, transaction_id,
        client_address, client_process,
        activity_type, activity_details, logged_at
    ) VALUES (
        :current_user_name, :current_connection_id, :current_transaction_id,
        :client_address, :client_process,
        :activity_type, :activity_details, CURRENT_TIMESTAMP
    );
END;
```

#### **Custom Context Variables**
```sql
-- Set and use custom context variables
CREATE PROCEDURE business.session.set_user_preferences(
    IN preference_name VARCHAR(50),
    IN preference_value VARCHAR(200)
)
AS
BEGIN
    -- Set custom context variable
    RDB$SET_CONTEXT('USER_SESSION', :preference_name, :preference_value);
END;

CREATE PROCEDURE business.session.get_user_preference(
    IN preference_name VARCHAR(50)
)
RETURNS (
    OUT preference_value VARCHAR(200)
)
AS
BEGIN
    -- Get custom context variable
    preference_value = RDB$GET_CONTEXT('USER_SESSION', :preference_name);
END;
```

### **Advanced Error Handling**

#### **Custom Exception Types**
```sql
-- Procedure with sophisticated error handling
CREATE PROCEDURE business.finance.process_payment(
    IN payment_id INTEGER,
    IN amount DECIMAL(15,2)
)
AS
DECLARE
    -- Custom exceptions
    invalid_amount EXCEPTION 'Payment amount must be positive';
    payment_not_found EXCEPTION 'Payment record not found';
    insufficient_funds EXCEPTION 'Insufficient account balance';
    payment_already_processed EXCEPTION 'Payment has already been processed';
    
    -- Variables
    payment_status VARCHAR(20);
    account_balance DECIMAL(15,2);
    account_id INTEGER;
BEGIN
    -- Validate input
    IF (amount <= 0) THEN
        EXCEPTION invalid_amount;
    
    -- Get payment details
    SELECT status, account_id
    FROM payments 
    WHERE id = :payment_id
    INTO :payment_status, :account_id;
    
    IF (ROW_COUNT = 0) THEN
        EXCEPTION payment_not_found;
    
    IF (payment_status = 'PROCESSED') THEN
        EXCEPTION payment_already_processed;
    
    -- Check account balance
    SELECT balance FROM accounts 
    WHERE id = :account_id
    INTO :account_balance;
    
    IF (account_balance < amount) THEN
        EXCEPTION insufficient_funds;
    
    -- Process payment in transaction
    IN AUTONOMOUS TRANSACTION DO
    BEGIN
        -- Update account balance
        UPDATE accounts 
        SET balance = balance - :amount
        WHERE id = :account_id;
        
        -- Update payment status
        UPDATE payments 
        SET status = 'PROCESSED',
            processed_amount = :amount,
            processed_date = CURRENT_TIMESTAMP
        WHERE id = :payment_id;
        
        -- Log successful payment
        INSERT INTO payment_log (payment_id, amount, status, processed_by, timestamp)
        VALUES (:payment_id, :amount, 'SUCCESS', CURRENT_USER, CURRENT_TIMESTAMP);
    END
    
    WHEN EXCEPTION insufficient_funds DO
    BEGIN
        -- Log failed payment attempt
        INSERT INTO payment_log (payment_id, amount, status, error_message, timestamp)
        VALUES (:payment_id, :amount, 'FAILED', 'Insufficient funds', CURRENT_TIMESTAMP);
        
        -- Update payment status
        UPDATE payments 
        SET status = 'FAILED',
            error_message = 'Insufficient account balance'
        WHERE id = :payment_id;
        
        -- Re-raise the exception
        EXCEPTION;
    END
    
    WHEN ANY DO
    BEGIN
        -- Handle unexpected errors
        INSERT INTO payment_log (payment_id, amount, status, error_message, timestamp)
        VALUES (:payment_id, :amount, 'ERROR', 'Unexpected error occurred', CURRENT_TIMESTAMP);
        
        -- Re-raise the exception
        EXCEPTION;
    END
END;
```

### **Dynamic SQL in Procedures**

#### **EXECUTE STATEMENT Usage**
```sql
-- Procedure using dynamic SQL
CREATE PROCEDURE reporting.dynamic.generate_report(
    IN table_name VARCHAR(63),
    IN filter_condition VARCHAR(500),
    IN sort_column VARCHAR(63)
)
RETURNS (
    OUT report_data BLOB
)
AS
DECLARE
    sql_statement VARCHAR(2000);
    temp_cursor CURSOR FOR (
        EXECUTE STATEMENT :sql_statement
    );
    report_row VARCHAR(1000);
    report_content VARCHAR(32000) DEFAULT '';
BEGIN
    -- Build dynamic SQL
    sql_statement = 'SELECT * FROM ' || :table_name;
    
    IF (filter_condition IS NOT NULL AND filter_condition <> '') THEN
        sql_statement = sql_statement || ' WHERE ' || :filter_condition;
    
    IF (sort_column IS NOT NULL AND sort_column <> '') THEN
        sql_statement = sql_statement || ' ORDER BY ' || :sort_column;
    
    -- Execute dynamic query and build report
    FOR EXECUTE STATEMENT :sql_statement 
        INTO :report_row
    DO
    BEGIN
        report_content = report_content || :report_row || ASCII_CHAR(13) || ASCII_CHAR(10);
    END
    
    -- Convert to BLOB
    report_data = CAST(report_content AS BLOB SUB_TYPE TEXT);
END;
```

---

## Error Handling and Troubleshooting

### **Common Procedure Errors**

#### **Procedure Creation Errors**
```sql
-- Error: Procedure already exists
CREATE PROCEDURE duplicate_name() AS BEGIN END;
-- Solution: Use CREATE OR ALTER or IF NOT EXISTS

-- Error: Invalid parameter data type
CREATE PROCEDURE test(IN param INVALID_TYPE) AS BEGIN END;
-- Solution: Use valid ScratchBird data types

-- Error: Syntax error in procedure body
CREATE PROCEDURE test() AS BEGIN INVALID SQL; END;
-- Solution: Check PSQL syntax and SQL statements
```

#### **Runtime Execution Errors**
```sql
-- Error: Parameter count mismatch
EXECUTE PROCEDURE calculate_tax(1000.00);  -- Missing second parameter
-- Solution: Provide all required parameters

-- Error: Type mismatch in parameter
EXECUTE PROCEDURE calculate_tax('invalid', 8.25);  -- String instead of number
-- Solution: Use correct data types
```

### **Debugging Procedures**

#### **Procedure Debugging Techniques**
```sql
-- Add debug logging to procedures
CREATE PROCEDURE debug_example(
    IN input_value INTEGER
)
AS
DECLARE
    debug_message VARCHAR(500);
BEGIN
    -- Log procedure entry
    debug_message = 'Procedure started with input: ' || input_value;
    INSERT INTO debug_log (message, timestamp) VALUES (:debug_message, CURRENT_TIMESTAMP);
    
    -- Add debug points throughout procedure
    IF (input_value < 0) THEN
    BEGIN
        debug_message = 'Negative input detected: ' || input_value;
        INSERT INTO debug_log (message, timestamp) VALUES (:debug_message, CURRENT_TIMESTAMP);
        EXCEPTION 'Invalid input value';
    END
    
    -- Log procedure completion
    INSERT INTO debug_log (message, timestamp) 
    VALUES ('Procedure completed successfully', CURRENT_TIMESTAMP);
END;
```

#### **Performance Analysis**
```sql
-- Monitor procedure execution statistics
SELECT 
    p.RDB$PROCEDURE_NAME,
    ps.MON$STAT_ID,
    ps.MON$STAT_TIME,
    ps.MON$MEMORY_USED,
    ps.MON$MEMORY_ALLOCATED
FROM RDB$PROCEDURES p
JOIN MON$PROCEDURE_STATS ps ON p.RDB$PROCEDURE_NAME = ps.MON$PROCEDURE_NAME
WHERE p.RDB$SYSTEM_FLAG = 0
ORDER BY ps.MON$STAT_TIME DESC;
```

---

## Best Practices

### **Procedure Design Guidelines**

1. **Parameter Naming**: Use descriptive parameter names with consistent prefixes (IN/OUT)
2. **Error Handling**: Always implement comprehensive exception handling
3. **Transaction Management**: Use explicit transaction control for data consistency
4. **Documentation**: Include meaningful comments in procedure bodies
5. **Security**: Choose appropriate SQL SECURITY mode based on use case
6. **Performance**: Optimize SQL statements and avoid unnecessary operations

### **Recommended Procedure Patterns**

#### **Data Validation Procedure Pattern**
```sql
CREATE PROCEDURE utilities.validation.validate_email(
    IN email_address VARCHAR(255)
)
RETURNS (
    OUT is_valid BOOLEAN,
    OUT error_message VARCHAR(200)
)
AS
BEGIN
    is_valid = TRUE;
    error_message = NULL;
    
    -- Check for null or empty
    IF (email_address IS NULL OR TRIM(email_address) = '') THEN
    BEGIN
        is_valid = FALSE;
        error_message = 'Email address cannot be empty';
        EXIT;
    END
    
    -- Check for @ symbol
    IF (POSITION('@' IN email_address) = 0) THEN
    BEGIN
        is_valid = FALSE;
        error_message = 'Email address must contain @ symbol';
        EXIT;
    END
    
    -- Check for domain part
    IF (POSITION('.' IN SUBSTRING(email_address FROM POSITION('@' IN email_address))) = 0) THEN
    BEGIN
        is_valid = FALSE;
        error_message = 'Email address must contain valid domain';
    END
END;
```

#### **Audit Trail Procedure Pattern**
```sql
CREATE PROCEDURE audit.system.record_data_change(
    IN table_name VARCHAR(63),
    IN record_id INTEGER,
    IN action_type VARCHAR(20),
    IN old_values VARCHAR(2000),
    IN new_values VARCHAR(2000)
)
AS
BEGIN
    INSERT INTO audit_trail (
        table_name, record_id, action_type,
        old_values, new_values,
        changed_by, changed_at,
        connection_id, transaction_id
    ) VALUES (
        :table_name, :record_id, :action_type,
        :old_values, :new_values,
        CURRENT_USER, CURRENT_TIMESTAMP,
        CURRENT_CONNECTION, CURRENT_TRANSACTION
    );
END;
```

---

## Migration and Integration

### **Procedure Migration Strategies**

#### **From Other Database Systems**
```sql
-- Convert Oracle PL/SQL procedure to ScratchBird PSQL
-- Oracle version:
-- CREATE OR REPLACE PROCEDURE calc_bonus(emp_id NUMBER, bonus OUT NUMBER)
-- ScratchBird equivalent:
CREATE OR ALTER PROCEDURE calc_bonus(
    IN emp_id INTEGER
)
RETURNS (
    OUT bonus DECIMAL(15,2)
)
AS
DECLARE
    salary DECIMAL(15,2);
    performance_rating DECIMAL(3,2);
BEGIN
    SELECT salary, performance_rating
    FROM employees 
    WHERE employee_id = :emp_id
    INTO :salary, :performance_rating;
    
    bonus = salary * performance_rating * 0.1;
END;
```

### **Deployment Automation**

#### **Procedure Deployment Scripts**
```sql
-- deployment_procedures.sql
-- Safe deployment with IF NOT EXISTS
CREATE PROCEDURE IF NOT EXISTS business.deploy.validate_environment()
AS
BEGIN
    -- Environment validation logic
    IF (NOT EXISTS(SELECT 1 FROM information_schema.tables WHERE table_name = 'CONFIG')) THEN
        EXCEPTION 'Database environment not properly configured';
END;

-- Deploy procedures with dependencies
EXECUTE PROCEDURE business.deploy.validate_environment();

CREATE OR ALTER PROCEDURE business.finance.calculate_tax(
    IN amount DECIMAL(15,2),
    IN rate DECIMAL(5,2)
) RETURNS (OUT tax_amount DECIMAL(15,2))
AS BEGIN
    tax_amount = amount * (rate / 100);
END;
```

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:3110-3180`
- **Classes**: `procedure_clause`, `psql_procedure_clause`, `external_procedure_clause`
- **Functionality**: Parsing CREATE/ALTER/DROP PROCEDURE syntax with parameters

#### **DDL Node Classes**
- **File**: `src/dsql/DdlNodes.h:542-647`
- **Classes**:
  - `CreateAlterProcedureNode` (lines 542-610): Procedure creation and modification
  - `DropProcedureNode` (lines 613-642): Procedure removal with dependency checking
  - `RecreateProcedureNode` (lines 645-647): Atomic drop-and-recreate operations

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:405-425`
- **Tables**: 
  - `RDB$PROCEDURES` - Stores procedure definitions
  - `RDB$PROCEDURE_PARAMETERS` - Stores procedure parameter information
- **Fields**:
  - `f_prc_name`: Procedure name
  - `f_prc_source`: PSQL source code
  - `f_prc_security`: SQL security mode (DEFINER/INVOKER)
  - `f_prc_type`: Procedure type (0=stored, 1=external)

### **Core Procedure Operations**

#### **CreateAlterProcedureNode Methods**
- Handles both CREATE and ALTER operations (create/alter flags)
- Parameter validation and type checking
- PSQL source code compilation and storage
- External procedure registration
- SQL security mode configuration

#### **Parameter Management**
- Input parameter parsing with default values
- Output parameter definition and validation
- Type compatibility checking
- Parameter metadata storage in RDB$PROCEDURE_PARAMETERS

#### **PSQL Execution Engine**
- Statement compilation and execution
- Variable scoping and management
- Exception handling and propagation
- Transaction control integration

### **Storage Structures**

Procedures are stored in the RDB$PROCEDURES system table with:
- **Identity**: Name, schema, owner information
- **Implementation**: PSQL source code or external module reference
- **Security**: SQL security mode and ownership details
- **Metadata**: Description, system flags, modification timestamps
- **Parameters**: Linked to RDB$PROCEDURE_PARAMETERS for input/output definitions

---

## Administrative Operations

### **Procedure Maintenance**

#### **Procedure Statistics and Monitoring**
```sql
-- Monitor procedure execution statistics
CREATE VIEW procedure_performance AS
SELECT 
    p.RDB$PROCEDURE_NAME,
    p.RDB$SCHEMA_NAME,
    COUNT(ps.MON$STAT_ID) as EXECUTION_COUNT,
    AVG(ps.MON$STAT_TIME) as AVG_EXECUTION_TIME,
    MAX(ps.MON$STAT_TIME) as MAX_EXECUTION_TIME,
    SUM(ps.MON$MEMORY_USED) as TOTAL_MEMORY_USED
FROM RDB$PROCEDURES p
LEFT JOIN MON$PROCEDURE_STATS ps ON p.RDB$PROCEDURE_NAME = ps.MON$PROCEDURE_NAME
WHERE p.RDB$SYSTEM_FLAG = 0 OR p.RDB$SYSTEM_FLAG IS NULL
GROUP BY p.RDB$PROCEDURE_NAME, p.RDB$SCHEMA_NAME
ORDER BY EXECUTION_COUNT DESC;
```

#### **Procedure Dependency Analysis**
```sql
-- Comprehensive dependency analysis
CREATE PROCEDURE admin.analyze_procedure_dependencies(
    IN procedure_name VARCHAR(63)
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
        WHERE dep.RDB$DEPENDENT_NAME = :procedure_name
          AND dep.RDB$DEPENDENT_TYPE = 5
    );
BEGIN
    report_text = 'Dependency Report for Procedure: ' || procedure_name || ASCII_CHAR(13) || ASCII_CHAR(10);
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

