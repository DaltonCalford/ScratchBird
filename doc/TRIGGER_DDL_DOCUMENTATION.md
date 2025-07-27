# ScratchBird TRIGGER - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

TRIGGER objects in ScratchBird are special stored procedures that execute automatically in response to specific database events. Unlike regular procedures that are called explicitly, triggers are fired by the database engine when certain events occur such as INSERT, UPDATE, DELETE operations on tables, DDL statements, or database connections. Triggers are essential for implementing data validation, auditing, logging, and maintaining data integrity.

### Key Features and Capabilities

- **Event-Driven Execution**: Automatically execute in response to database events
- **Multiple Trigger Types**: Table triggers (DML), database triggers, and DDL triggers
- **Timing Control**: BEFORE and AFTER trigger execution timing
- **Event Filtering**: Respond to specific events (INSERT, UPDATE, DELETE, or combinations)
- **Context Variables**: Access to OLD and NEW record values, trigger action predicates
- **Position Control**: Define execution order with POSITION clause
- **Active/Inactive States**: Enable/disable triggers without dropping them
- **PSQL Programming**: Full procedural language support within trigger bodies
- **External Language Support**: Integration with external languages (Java, C++, etc.)

### ScratchBird-Specific Enhancements

1. **Hierarchical Schema Support**: Triggers support 3-level qualified names (`schema.subschema.trigger`)
2. **Comprehensive DDL Triggers**: Triggers can fire on any DDL statement or specific DDL operations
3. **Database Event Triggers**: Triggers for connection, disconnection, and transaction events
4. **Advanced Position Control**: Fine-grained execution order management
5. **Extended Context Variables**: Enhanced access to trigger execution context
6. **External Language Integration**: Comprehensive foreign language trigger support
7. **IF NOT EXISTS Support**: Conditional creation syntax for deployment scripts
8. **RECREATE Support**: Atomic drop-and-recreate operations
9. **CREATE OR ALTER Support**: Flexible trigger modification syntax

---

## DDL Syntax Reference

### CREATE TRIGGER

Creates a new trigger with specified event types and executable body.

#### **Table Trigger Syntax**
```sql
CREATE [OR ALTER] TRIGGER [IF NOT EXISTS] [schema_name.]trigger_name
    [ACTIVE | INACTIVE]
    {BEFORE | AFTER} {INSERT | UPDATE | DELETE} [OR {INSERT | UPDATE | DELETE}]...
    [POSITION number]
    ON table_name
    [SQL SECURITY {DEFINER | INVOKER}]
AS
[DECLARE
    -- Local variable declarations
    variable_name data_type [DEFAULT value];
]
BEGIN
    -- Trigger body statements
END;
```

#### **Database Trigger Syntax**
```sql
CREATE [OR ALTER] TRIGGER [IF NOT EXISTS] [schema_name.]trigger_name
    [ACTIVE | INACTIVE]
    ON {CONNECT | DISCONNECT | TRANSACTION START | TRANSACTION COMMIT | TRANSACTION ROLLBACK}
    [POSITION number]
    [SQL SECURITY {DEFINER | INVOKER}]
AS
[DECLARE
    -- Local variable declarations
]
BEGIN
    -- Trigger body statements
END;
```

#### **DDL Trigger Syntax**
```sql
CREATE [OR ALTER] TRIGGER [IF NOT EXISTS] [schema_name.]trigger_name
    [ACTIVE | INACTIVE]
    {BEFORE | AFTER} {ddl_event | ANY DDL STATEMENT}
    [POSITION number]
    [SQL SECURITY {DEFINER | INVOKER}]
AS
[DECLARE
    -- Local variable declarations
]
BEGIN
    -- Trigger body statements
END;
```

#### **External Trigger Syntax**
```sql
CREATE [OR ALTER] TRIGGER [IF NOT EXISTS] [schema_name.]trigger_name
    [ACTIVE | INACTIVE]
    {trigger_event_specification}
    [POSITION number]
EXTERNAL NAME 'external_name'
ENGINE engine_name
[AS 'external_body'];
```

#### **Parameters**

- **OR ALTER**: Modify existing trigger or create if doesn't exist
- **IF NOT EXISTS**: Skip creation if trigger already exists (no error)
- **ACTIVE/INACTIVE**: Initial trigger state (default: ACTIVE)
- **BEFORE/AFTER**: Trigger timing relative to the triggering event
- **trigger_events**: Events that cause the trigger to fire
- **POSITION**: Execution order (lower numbers execute first)
- **table_name**: Target table for table triggers
- **SQL SECURITY**: Security context (DEFINER or INVOKER privileges)

---

## CREATE TRIGGER Examples

### **Table Triggers (DML)**

#### **Basic INSERT/UPDATE/DELETE Triggers**
```sql
-- Simple audit trigger for INSERT operations
CREATE TRIGGER tr_customers_insert_audit
    AFTER INSERT ON customers
AS
BEGIN
    INSERT INTO audit_log (table_name, operation, record_id, changed_by, changed_at)
    VALUES ('CUSTOMERS', 'INSERT', NEW.customer_id, CURRENT_USER, CURRENT_TIMESTAMP);
END;

-- UPDATE trigger with OLD/NEW value comparison
CREATE TRIGGER tr_products_update_log
    AFTER UPDATE ON products
AS
BEGIN
    -- Log only if price changed
    IF (OLD.price <> NEW.price) THEN
    BEGIN
        INSERT INTO price_history (product_id, old_price, new_price, changed_by, changed_at)
        VALUES (NEW.product_id, OLD.price, NEW.price, CURRENT_USER, CURRENT_TIMESTAMP);
    END
END;

-- DELETE trigger with data preservation
CREATE TRIGGER tr_employees_delete_archive
    BEFORE DELETE ON employees
AS
BEGIN
    -- Archive employee data before deletion
    INSERT INTO employees_archive (
        employee_id, first_name, last_name, email, hire_date, 
        salary, department, archived_by, archived_at
    )
    VALUES (
        OLD.employee_id, OLD.first_name, OLD.last_name, OLD.email, OLD.hire_date,
        OLD.salary, OLD.department, CURRENT_USER, CURRENT_TIMESTAMP
    );
END;
```

#### **Multi-Event Triggers**
```sql
-- Trigger that fires on INSERT, UPDATE, or DELETE
CREATE TRIGGER tr_inventory_all_changes
    AFTER INSERT OR UPDATE OR DELETE ON inventory
    POSITION 10
AS
DECLARE
    operation_type VARCHAR(10);
    product_id INTEGER;
    old_quantity INTEGER;
    new_quantity INTEGER;
BEGIN
    -- Determine operation type using trigger action predicates
    IF (INSERTING) THEN
    BEGIN
        operation_type = 'INSERT';
        product_id = NEW.product_id;
        old_quantity = 0;
        new_quantity = NEW.quantity;
    END
    ELSE IF (UPDATING) THEN
    BEGIN
        operation_type = 'UPDATE';
        product_id = NEW.product_id;
        old_quantity = OLD.quantity;
        new_quantity = NEW.quantity;
    END
    ELSE IF (DELETING) THEN
    BEGIN
        operation_type = 'DELETE';
        product_id = OLD.product_id;
        old_quantity = OLD.quantity;
        new_quantity = 0;
    END
    
    -- Log inventory change
    INSERT INTO inventory_changes (
        product_id, operation_type, old_quantity, new_quantity,
        changed_by, changed_at
    )
    VALUES (
        :product_id, :operation_type, :old_quantity, :new_quantity,
        CURRENT_USER, CURRENT_TIMESTAMP
    );
    
    -- Update inventory summary
    UPDATE OR INSERT INTO inventory_summary (product_id, current_quantity, last_updated)
    VALUES (:product_id, :new_quantity, CURRENT_TIMESTAMP);
END;
```

#### **Data Validation Triggers**
```sql
-- BEFORE INSERT/UPDATE trigger for data validation
CREATE TRIGGER tr_orders_validate
    BEFORE INSERT OR UPDATE ON orders
AS
DECLARE
    customer_status VARCHAR(20);
    credit_limit DECIMAL(15,2);
    invalid_order EXCEPTION 'Invalid order data';
BEGIN
    -- Validate customer exists and is active
    SELECT status, credit_limit
    FROM customers 
    WHERE customer_id = NEW.customer_id
    INTO :customer_status, :credit_limit;
    
    IF (ROW_COUNT = 0) THEN
        EXCEPTION invalid_order USING ('Customer not found');
    
    IF (customer_status <> 'ACTIVE') THEN
        EXCEPTION invalid_order USING ('Customer account is not active');
    
    -- Validate order amount doesn't exceed credit limit
    IF (NEW.order_total > credit_limit) THEN
        EXCEPTION invalid_order USING ('Order exceeds customer credit limit');
    
    -- Set default values for new orders
    IF (INSERTING AND NEW.order_date IS NULL) THEN
        NEW.order_date = CURRENT_DATE;
    
    IF (INSERTING AND NEW.order_status IS NULL) THEN
        NEW.order_status = 'PENDING';
    
    -- Update modification timestamp
    IF (UPDATING) THEN
        NEW.last_modified = CURRENT_TIMESTAMP;
END;
```

#### **Business Logic Triggers**
```sql
-- Complex business logic trigger
CREATE TRIGGER tr_sales_commission_calc
    AFTER INSERT OR UPDATE ON sales_transactions
    POSITION 5
AS
DECLARE
    commission_rate DECIMAL(5,2);
    commission_amount DECIMAL(15,2);
    sales_person_id INTEGER;
    territory_bonus DECIMAL(5,2) DEFAULT 0.00;
BEGIN
    -- Only process if this is a completed sale
    IF (NEW.transaction_status = 'COMPLETED') THEN
    BEGIN
        -- Get sales person and commission rate
        SELECT sp.sales_person_id, sp.commission_rate, t.territory_bonus
        FROM sales_people sp
        JOIN territories t ON sp.territory_id = t.territory_id
        WHERE sp.sales_person_id = NEW.sales_person_id
        INTO :sales_person_id, :commission_rate, :territory_bonus;
        
        -- Calculate commission with territory bonus
        commission_amount = NEW.sale_amount * ((commission_rate + territory_bonus) / 100);
        
        -- Record commission
        INSERT INTO sales_commissions (
            sales_person_id, transaction_id, sale_amount, 
            commission_rate, territory_bonus, commission_amount,
            commission_date
        )
        VALUES (
            :sales_person_id, NEW.transaction_id, NEW.sale_amount,
            :commission_rate, :territory_bonus, :commission_amount,
            CURRENT_DATE
        );
        
        -- Update sales person statistics
        UPDATE sales_people 
        SET total_sales = total_sales + NEW.sale_amount,
            total_commissions = total_commissions + :commission_amount,
            last_sale_date = CURRENT_DATE
        WHERE sales_person_id = :sales_person_id;
        
        -- Update territory statistics
        UPDATE territories
        SET total_sales = total_sales + NEW.sale_amount,
            sale_count = sale_count + 1
        WHERE territory_id = (
            SELECT territory_id FROM sales_people 
            WHERE sales_person_id = :sales_person_id
        );
    END
END;
```

### **Database Event Triggers**

#### **Connection and Disconnection Triggers**
```sql
-- Connection audit trigger
CREATE TRIGGER tr_user_connect_audit
    ON CONNECT
    POSITION 1
AS
BEGIN
    INSERT INTO connection_log (
        username, connection_time, client_address, client_process,
        connection_id, database_name
    )
    VALUES (
        CURRENT_USER,
        CURRENT_TIMESTAMP,
        RDB$GET_CONTEXT('SYSTEM', 'CLIENT_ADDRESS'),
        RDB$GET_CONTEXT('SYSTEM', 'CLIENT_PROCESS'),
        CURRENT_CONNECTION,
        RDB$GET_CONTEXT('SYSTEM', 'DB_NAME')
    );
    
    -- Update user statistics
    UPDATE OR INSERT INTO user_statistics (username, last_login, total_logins)
    VALUES (CURRENT_USER, CURRENT_TIMESTAMP, 1)
    MATCHING (username);
END;

-- Disconnection cleanup trigger
CREATE TRIGGER tr_user_disconnect_cleanup
    ON DISCONNECT
AS
BEGIN
    -- Log disconnection
    INSERT INTO disconnection_log (
        username, disconnection_time, connection_duration, connection_id
    )
    VALUES (
        CURRENT_USER,
        CURRENT_TIMESTAMP,
        DATEDIFF(SECOND, 
            (SELECT connection_time FROM connection_log 
             WHERE connection_id = CURRENT_CONNECTION), 
            CURRENT_TIMESTAMP),
        CURRENT_CONNECTION
    );
    
    -- Clean up temporary session data
    DELETE FROM temp_session_data WHERE connection_id = CURRENT_CONNECTION;
END;
```

#### **Transaction Event Triggers**
```sql
-- Transaction start logging
CREATE TRIGGER tr_transaction_start_log
    ON TRANSACTION START
AS
BEGIN
    INSERT INTO transaction_log (
        transaction_id, username, start_time, isolation_level
    )
    VALUES (
        CURRENT_TRANSACTION,
        CURRENT_USER,
        CURRENT_TIMESTAMP,
        RDB$GET_CONTEXT('SYSTEM', 'ISOLATION_LEVEL')
    );
END;

-- Transaction commit trigger
CREATE TRIGGER tr_transaction_commit_log
    ON TRANSACTION COMMIT
AS
DECLARE
    transaction_duration INTEGER;
BEGIN
    -- Calculate transaction duration
    SELECT DATEDIFF(MILLISECOND, start_time, CURRENT_TIMESTAMP)
    FROM transaction_log 
    WHERE transaction_id = CURRENT_TRANSACTION
    INTO :transaction_duration;
    
    -- Update transaction log
    UPDATE transaction_log 
    SET commit_time = CURRENT_TIMESTAMP,
        duration_ms = :transaction_duration,
        status = 'COMMITTED'
    WHERE transaction_id = CURRENT_TRANSACTION;
    
    -- Log long-running transactions
    IF (transaction_duration > 30000) THEN -- 30 seconds
    BEGIN
        INSERT INTO long_transaction_log (
            transaction_id, username, duration_ms, commit_time
        )
        VALUES (
            CURRENT_TRANSACTION, CURRENT_USER, 
            :transaction_duration, CURRENT_TIMESTAMP
        );
    END
END;

-- Transaction rollback trigger
CREATE TRIGGER tr_transaction_rollback_log
    ON TRANSACTION ROLLBACK
AS
BEGIN
    UPDATE transaction_log 
    SET rollback_time = CURRENT_TIMESTAMP,
        status = 'ROLLED_BACK'
    WHERE transaction_id = CURRENT_TRANSACTION;
END;
```

### **DDL Event Triggers**

#### **Schema Change Auditing**
```sql
-- DDL audit trigger for all DDL statements
CREATE TRIGGER tr_ddl_audit
    AFTER ANY DDL STATEMENT
AS
BEGIN
    INSERT INTO ddl_audit_log (
        username, ddl_event, object_type, object_name,
        sql_text, execution_time, connection_id
    )
    VALUES (
        CURRENT_USER,
        RDB$GET_CONTEXT('DDL_TRIGGER', 'EVENT_TYPE'),
        RDB$GET_CONTEXT('DDL_TRIGGER', 'OBJECT_TYPE'),
        RDB$GET_CONTEXT('DDL_TRIGGER', 'OBJECT_NAME'),
        RDB$GET_CONTEXT('DDL_TRIGGER', 'SQL_TEXT'),
        CURRENT_TIMESTAMP,
        CURRENT_CONNECTION
    );
END;

-- Specific DDL event triggers
CREATE TRIGGER tr_table_creation_notify
    AFTER CREATE TABLE
AS
BEGIN
    INSERT INTO schema_change_notifications (
        change_type, object_name, changed_by, change_time, 
        notification_sent
    )
    VALUES (
        'TABLE_CREATED',
        RDB$GET_CONTEXT('DDL_TRIGGER', 'OBJECT_NAME'),
        CURRENT_USER,
        CURRENT_TIMESTAMP,
        FALSE
    );
END;

-- Security-focused DDL trigger
CREATE TRIGGER tr_ddl_security_check
    BEFORE CREATE USER OR ALTER USER OR DROP USER
AS
DECLARE
    admin_user BOOLEAN DEFAULT FALSE;
    security_violation EXCEPTION 'Insufficient privileges for user management';
BEGIN
    -- Check if current user is in admin role
    SELECT TRUE FROM RDB$USER_PRIVILEGES 
    WHERE RDB$USER = CURRENT_USER 
      AND RDB$PRIVILEGE = 'ADMIN'
    INTO :admin_user;
    
    IF (admin_user IS NULL OR admin_user = FALSE) THEN
        EXCEPTION security_violation;
    
    -- Log security-sensitive DDL
    INSERT INTO security_audit_log (
        username, action_type, target_object, action_time
    )
    VALUES (
        CURRENT_USER,
        RDB$GET_CONTEXT('DDL_TRIGGER', 'EVENT_TYPE'),
        RDB$GET_CONTEXT('DDL_TRIGGER', 'OBJECT_NAME'),
        CURRENT_TIMESTAMP
    );
END;
```

### **Hierarchical Schema Triggers**

#### **3-Level Schema Qualification**
```sql
-- Audit trigger in hierarchical schema
CREATE TRIGGER business.finance.accounting.tr_ledger_audit
    AFTER INSERT OR UPDATE OR DELETE ON business.finance.accounting.general_ledger
AS
DECLARE
    operation_type VARCHAR(10);
    account_id INTEGER;
    amount_change DECIMAL(15,2);
BEGIN
    IF (INSERTING) THEN
    BEGIN
        operation_type = 'INSERT';
        account_id = NEW.account_id;
        amount_change = NEW.amount;
    END
    ELSE IF (UPDATING) THEN
    BEGIN
        operation_type = 'UPDATE';
        account_id = NEW.account_id;
        amount_change = NEW.amount - OLD.amount;
    END
    ELSE IF (DELETING) THEN
    BEGIN
        operation_type = 'DELETE';
        account_id = OLD.account_id;
        amount_change = -OLD.amount;
    END
    
    -- Record in audit trail
    INSERT INTO business.finance.audit.accounting_audit (
        operation_type, account_id, amount_change,
        transaction_date, audit_user, audit_timestamp
    )
    VALUES (
        :operation_type, :account_id, :amount_change,
        COALESCE(NEW.transaction_date, OLD.transaction_date),
        CURRENT_USER, CURRENT_TIMESTAMP
    );
END;

-- Cross-schema trigger
CREATE TRIGGER business.hr.payroll.tr_salary_change_notify
    AFTER UPDATE ON business.hr.employees
AS
BEGIN
    -- Only trigger if salary changed
    IF (OLD.salary <> NEW.salary) THEN
    BEGIN
        -- Notify finance department
        INSERT INTO business.finance.notifications.salary_changes (
            employee_id, old_salary, new_salary, 
            effective_date, hr_user, notification_time
        )
        VALUES (
            NEW.employee_id, OLD.salary, NEW.salary,
            NEW.salary_effective_date, CURRENT_USER, CURRENT_TIMESTAMP
        );
        
        -- Update payroll calculations flag
        UPDATE business.hr.payroll.payroll_status 
        SET recalculation_needed = TRUE,
            last_change_date = CURRENT_TIMESTAMP
        WHERE employee_id = NEW.employee_id;
    END
END;
```

### **External Language Triggers**

#### **Java External Triggers**
```sql
-- Complex data validation trigger in Java
CREATE TRIGGER tr_order_validation_java
    BEFORE INSERT OR UPDATE ON orders
EXTERNAL NAME 'OrderValidation.validateOrder'
ENGINE JAVA
AS '
    public static void validateOrder(
        java.sql.Connection connection,
        Object[] oldValues,
        Object[] newValues
    ) throws java.sql.SQLException {
        try {
            OrderValidator validator = new OrderValidator(connection);
            Order order = Order.fromValues(newValues);
            
            ValidationResult result = validator.validateOrder(order);
            
            if (!result.isValid()) {
                throw new java.sql.SQLException(
                    "Order validation failed: " + result.getErrorMessage()
                );
            }
            
            // Apply business rules
            if (order.getAmount() > 10000) {
                order.setRequiresApproval(true);
                order.updateValues(newValues);
            }
            
        } catch (Exception e) {
            throw new java.sql.SQLException("Validation error: " + e.getMessage());
        }
    }
';

-- High-performance calculation trigger in Java
CREATE TRIGGER tr_financial_calc_java
    AFTER INSERT OR UPDATE ON financial_transactions
EXTERNAL NAME 'FinancialCalculator.updateMetrics'
ENGINE JAVA;
```

#### **C++ External Triggers**
```sql
-- High-performance audit trigger in C++
CREATE TRIGGER tr_high_perf_audit
    AFTER INSERT OR UPDATE OR DELETE ON high_volume_table
EXTERNAL NAME 'audit_engine.process_change'
ENGINE CPP;

-- Real-time data processing trigger
CREATE TRIGGER tr_realtime_processing
    AFTER INSERT ON sensor_data
EXTERNAL NAME 'sensor_processor.process_reading'
ENGINE CPP;
```

### **Advanced Trigger Features**

#### **Position Control and Execution Order**
```sql
-- First trigger - data validation (position 1)
CREATE TRIGGER tr_data_validate
    BEFORE INSERT OR UPDATE ON orders
    POSITION 1
AS
BEGIN
    -- Critical validation logic
    IF (NEW.order_total <= 0) THEN
        EXCEPTION 'Order total must be positive';
END;

-- Second trigger - business rules (position 5)
CREATE TRIGGER tr_business_rules
    BEFORE INSERT OR UPDATE ON orders
    POSITION 5
AS
BEGIN
    -- Apply business rules after validation
    IF (NEW.customer_type = 'VIP') THEN
        NEW.priority_level = 'HIGH';
END;

-- Third trigger - default values (position 10)
CREATE TRIGGER tr_set_defaults
    BEFORE INSERT ON orders
    POSITION 10
AS
BEGIN
    -- Set default values last
    IF (NEW.order_date IS NULL) THEN
        NEW.order_date = CURRENT_DATE;
    
    IF (NEW.order_status IS NULL) THEN
        NEW.order_status = 'PENDING';
END;

-- After triggers for logging (low position numbers execute first)
CREATE TRIGGER tr_audit_log
    AFTER INSERT OR UPDATE OR DELETE ON orders
    POSITION 1
AS
BEGIN
    -- Primary audit logging
    INSERT INTO audit_log (table_name, operation, timestamp)
    VALUES ('ORDERS', 
            CASE WHEN INSERTING THEN 'INSERT'
                 WHEN UPDATING THEN 'UPDATE'
                 WHEN DELETING THEN 'DELETE' END,
            CURRENT_TIMESTAMP);
END;

CREATE TRIGGER tr_detailed_audit
    AFTER INSERT OR UPDATE OR DELETE ON orders
    POSITION 5
AS
BEGIN
    -- Detailed audit after primary logging
    INSERT INTO detailed_audit_log (
        table_name, record_id, old_values, new_values, operation
    )
    VALUES ('ORDERS',
            COALESCE(NEW.order_id, OLD.order_id),
            CASE WHEN DELETING THEN OLD ELSE NULL END,
            CASE WHEN INSERTING OR UPDATING THEN NEW ELSE NULL END,
            CASE WHEN INSERTING THEN 'INSERT'
                 WHEN UPDATING THEN 'UPDATE'
                 WHEN DELETING THEN 'DELETE' END);
END;
```

#### **Conditional Creation and Management**
```sql
-- Safe trigger creation (no error if exists)
CREATE TRIGGER IF NOT EXISTS tr_basic_audit
    AFTER INSERT OR UPDATE OR DELETE ON customers
AS
BEGIN
    INSERT INTO audit_log (table_name, operation, record_id, timestamp)
    VALUES ('CUSTOMERS',
            CASE WHEN INSERTING THEN 'INSERT'
                 WHEN UPDATING THEN 'UPDATE'
                 WHEN DELETING THEN 'DELETE' END,
            COALESCE(NEW.customer_id, OLD.customer_id),
            CURRENT_TIMESTAMP);
END;

-- Create or modify existing trigger
CREATE OR ALTER TRIGGER tr_inventory_tracking
    AFTER INSERT OR UPDATE OR DELETE ON inventory
AS
BEGIN
    -- Updated tracking logic with enhanced features
    INSERT INTO inventory_tracking (
        product_id, operation_type, quantity_change,
        old_quantity, new_quantity, tracking_timestamp,
        user_id, connection_id
    )
    VALUES (
        COALESCE(NEW.product_id, OLD.product_id),
        CASE WHEN INSERTING THEN 'INSERT'
             WHEN UPDATING THEN 'UPDATE'
             WHEN DELETING THEN 'DELETE' END,
        COALESCE(NEW.quantity, 0) - COALESCE(OLD.quantity, 0),
        COALESCE(OLD.quantity, 0),
        COALESCE(NEW.quantity, 0),
        CURRENT_TIMESTAMP,
        CURRENT_USER,
        CURRENT_CONNECTION
    );
END;
```

---

## ALTER TRIGGER

Modifies existing trigger definitions including timing, events, position, and implementation.

### **ALTER TRIGGER Syntax**
```sql
ALTER TRIGGER [schema_name.]trigger_name
    [ACTIVE | INACTIVE]
    [{BEFORE | AFTER} {trigger_events}]
    [POSITION number]
    [SQL SECURITY {DEFINER | INVOKER}]
[AS
[DECLARE
    -- Local declarations
]
BEGIN
    -- Modified trigger body
END];
```

### **ALTER TRIGGER Examples**

#### **Change Trigger State**
```sql
-- Temporarily disable trigger
ALTER TRIGGER tr_audit_customers INACTIVE;

-- Re-enable trigger
ALTER TRIGGER tr_audit_customers ACTIVE;

-- Change trigger position
ALTER TRIGGER tr_validation_orders POSITION 5;
```

#### **Modify Trigger Events and Implementation**
```sql
-- Change trigger to fire on different events
ALTER TRIGGER tr_inventory_log
    AFTER INSERT OR UPDATE OR DELETE ON inventory
    POSITION 10
AS
BEGIN
    -- Updated implementation with enhanced logging
    INSERT INTO inventory_changes (
        product_id, operation_type, old_quantity, new_quantity,
        changed_by, changed_at, change_reason
    )
    VALUES (
        COALESCE(NEW.product_id, OLD.product_id),
        CASE WHEN INSERTING THEN 'INSERT'
             WHEN UPDATING THEN 'UPDATE'
             WHEN DELETING THEN 'DELETE' END,
        COALESCE(OLD.quantity, 0),
        COALESCE(NEW.quantity, 0),
        CURRENT_USER,
        CURRENT_TIMESTAMP,
        RDB$GET_CONTEXT('USER_SESSION', 'CHANGE_REASON')
    );
END;
```

#### **Change Security Mode**
```sql
-- Change trigger to run with definer privileges
ALTER TRIGGER tr_security_audit
    SQL SECURITY DEFINER;

-- Partial alter - only change timing and position
ALTER TRIGGER tr_data_validation
    BEFORE INSERT OR UPDATE
    POSITION 1;
```

---

## RECREATE TRIGGER

Drops and recreates a trigger in a single atomic operation.

### **RECREATE TRIGGER Syntax**
```sql
RECREATE TRIGGER [schema_name.]trigger_name
    [ACTIVE | INACTIVE]
    trigger_event_specification
    [POSITION number]
    [SQL SECURITY {DEFINER | INVOKER}]
AS
[DECLARE
    -- Local declarations
]
BEGIN
    -- Trigger body
END;
```

### **RECREATE TRIGGER Examples**

#### **Complete Trigger Replacement**
```sql
-- Recreate trigger with entirely new logic
RECREATE TRIGGER tr_order_processing
    AFTER INSERT OR UPDATE ON orders
    POSITION 5
AS
DECLARE
    shipping_cost DECIMAL(10,2);
    tax_amount DECIMAL(10,2);
    total_cost DECIMAL(15,2);
BEGIN
    -- New comprehensive order processing logic
    
    -- Calculate shipping cost based on weight and distance
    SELECT calculate_shipping_cost(NEW.total_weight, NEW.shipping_distance)
    FROM dual INTO :shipping_cost;
    
    -- Calculate tax based on shipping address
    SELECT calculate_tax(NEW.subtotal, NEW.shipping_state)
    FROM dual INTO :tax_amount;
    
    -- Update order totals
    total_cost = NEW.subtotal + shipping_cost + tax_amount;
    
    UPDATE orders 
    SET shipping_cost = :shipping_cost,
        tax_amount = :tax_amount,
        total_amount = :total_cost,
        last_calculated = CURRENT_TIMESTAMP
    WHERE order_id = NEW.order_id;
    
    -- Create shipping record for orders over $100
    IF (total_cost > 100) THEN
    BEGIN
        INSERT INTO shipping_requests (
            order_id, shipping_type, estimated_cost, request_date
        )
        VALUES (
            NEW.order_id, 'STANDARD', :shipping_cost, CURRENT_DATE
        );
    END
END;
```

---

## DROP TRIGGER

Removes trigger definitions from the database.

### **DROP TRIGGER Syntax**
```sql
DROP TRIGGER [IF EXISTS] [schema_name.]trigger_name;
```

### **DROP TRIGGER Examples**

#### **Basic Trigger Removal**
```sql
-- Drop trigger if it exists
DROP TRIGGER IF EXISTS tr_temp_audit;

-- Drop trigger from specific schema
DROP TRIGGER business.finance.tr_payment_validation;

-- Drop hierarchical schema trigger
DROP TRIGGER enterprise.americas.usa.tr_regional_compliance;
```

#### **Cleanup Operations**
```sql
-- Drop related triggers before table changes
DROP TRIGGER tr_orders_audit;
DROP TRIGGER tr_orders_validation;
DROP TRIGGER tr_orders_notification;

-- Safe cleanup with existence check
DROP TRIGGER IF EXISTS tr_legacy_processing;
```

---

## Trigger Context and Variables

### **OLD and NEW Variables**

#### **Accessing Record Values**
```sql
-- Trigger showing OLD and NEW usage
CREATE TRIGGER tr_employee_salary_audit
    AFTER UPDATE ON employees
AS
BEGIN
    -- NEW contains the updated values
    -- OLD contains the original values
    
    IF (OLD.salary <> NEW.salary) THEN
    BEGIN
        INSERT INTO salary_audit (
            employee_id, employee_name,
            old_salary, new_salary, salary_change,
            change_percentage, changed_by, change_date
        )
        VALUES (
            NEW.employee_id,
            NEW.first_name || ' ' || NEW.last_name,
            OLD.salary,
            NEW.salary,
            NEW.salary - OLD.salary,
            ((NEW.salary - OLD.salary) / OLD.salary) * 100,
            CURRENT_USER,
            CURRENT_TIMESTAMP
        );
    END
    
    -- Access other changed fields
    IF (OLD.department <> NEW.department) THEN
    BEGIN
        INSERT INTO department_transfers (
            employee_id, old_department, new_department,
            transfer_date, processed_by
        )
        VALUES (
            NEW.employee_id, OLD.department, NEW.department,
            CURRENT_DATE, CURRENT_USER
        );
    END
END;
```

### **Trigger Action Predicates**

#### **INSERTING, UPDATING, DELETING**
```sql
-- Multi-event trigger using action predicates
CREATE TRIGGER tr_comprehensive_audit
    AFTER INSERT OR UPDATE OR DELETE ON products
AS
DECLARE
    audit_action VARCHAR(10);
    product_id INTEGER;
    product_data VARCHAR(1000);
BEGIN
    -- Determine which action triggered the trigger
    IF (INSERTING) THEN
    BEGIN
        audit_action = 'INSERT';
        product_id = NEW.product_id;
        product_data = 'Name: ' || NEW.product_name || 
                      ', Price: ' || CAST(NEW.price AS VARCHAR(20));
    END
    ELSE IF (UPDATING) THEN
    BEGIN
        audit_action = 'UPDATE';
        product_id = NEW.product_id;
        product_data = 'Old: ' || OLD.product_name || '/' || CAST(OLD.price AS VARCHAR(20)) ||
                      ', New: ' || NEW.product_name || '/' || CAST(NEW.price AS VARCHAR(20));
    END
    ELSE IF (DELETING) THEN
    BEGIN
        audit_action = 'DELETE';
        product_id = OLD.product_id;
        product_data = 'Deleted: ' || OLD.product_name || 
                      ', Price: ' || CAST(OLD.price AS VARCHAR(20));
    END
    
    -- Log the action
    INSERT INTO product_audit (
        product_id, action_type, audit_data, 
        audit_user, audit_timestamp
    )
    VALUES (
        :product_id, :audit_action, :product_data,
        CURRENT_USER, CURRENT_TIMESTAMP
    );
    
    -- Conditional logic based on action
    IF (UPDATING AND OLD.price <> NEW.price) THEN
    BEGIN
        INSERT INTO price_change_notifications (
            product_id, old_price, new_price, change_date
        )
        VALUES (NEW.product_id, OLD.price, NEW.price, CURRENT_DATE);
    END
END;
```

### **Context Variables in Triggers**

#### **System Context Information**
```sql
-- Trigger using system context variables
CREATE TRIGGER tr_detailed_connection_audit
    AFTER INSERT OR UPDATE OR DELETE ON sensitive_data
AS
BEGIN
    INSERT INTO security_audit (
        table_name, operation_type, record_id,
        username, connection_id, transaction_id,
        client_address, client_process, application_name,
        audit_timestamp
    )
    VALUES (
        'SENSITIVE_DATA',
        CASE WHEN INSERTING THEN 'INSERT'
             WHEN UPDATING THEN 'UPDATE'
             WHEN DELETING THEN 'DELETE' END,
        COALESCE(NEW.id, OLD.id),
        CURRENT_USER,
        CURRENT_CONNECTION,
        CURRENT_TRANSACTION,
        RDB$GET_CONTEXT('SYSTEM', 'CLIENT_ADDRESS'),
        RDB$GET_CONTEXT('SYSTEM', 'CLIENT_PROCESS'),
        RDB$GET_CONTEXT('SYSTEM', 'APPLICATION_NAME'),
        CURRENT_TIMESTAMP
    );
END;
```

---

## System Catalog Integration

ScratchBird stores trigger definitions in the RDB$TRIGGERS system table and related metadata tables.

### **Querying Trigger Information**

#### **List All Triggers**
```sql
-- Show all user-defined triggers
SELECT 
    RDB$TRIGGER_NAME as TRIGGER_NAME,
    RDB$SCHEMA_NAME as SCHEMA_NAME,
    RDB$RELATION_NAME as TABLE_NAME,
    CASE RDB$TRIGGER_TYPE
        WHEN 1 THEN 'BEFORE INSERT'
        WHEN 2 THEN 'AFTER INSERT'
        WHEN 3 THEN 'BEFORE UPDATE'
        WHEN 4 THEN 'AFTER UPDATE'
        WHEN 5 THEN 'BEFORE DELETE'
        WHEN 6 THEN 'AFTER DELETE'
        WHEN 17 THEN 'BEFORE INSERT OR UPDATE'
        WHEN 18 THEN 'AFTER INSERT OR UPDATE'
        ELSE 'OTHER'
    END as TRIGGER_TYPE,
    RDB$TRIGGER_SEQUENCE as POSITION,
    CASE RDB$TRIGGER_INACTIVE
        WHEN 0 THEN 'ACTIVE'
        WHEN 1 THEN 'INACTIVE'
        ELSE 'UNKNOWN'
    END as STATUS,
    CASE RDB$SQL_SECURITY
        WHEN 0 THEN 'INVOKER'
        WHEN 1 THEN 'DEFINER'
        ELSE 'INHERITED'
    END as SQL_SECURITY,
    RDB$DESCRIPTION as DESCRIPTION
FROM RDB$TRIGGERS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY RDB$SCHEMA_NAME, RDB$RELATION_NAME, RDB$TRIGGER_SEQUENCE;
```

#### **Triggers by Table**
```sql
-- List all triggers for a specific table
SELECT 
    t.RDB$TRIGGER_NAME,
    CASE t.RDB$TRIGGER_TYPE
        WHEN 1 THEN 'BEFORE INSERT'
        WHEN 2 THEN 'AFTER INSERT'
        WHEN 3 THEN 'BEFORE UPDATE'
        WHEN 4 THEN 'AFTER UPDATE'
        WHEN 5 THEN 'BEFORE DELETE'
        WHEN 6 THEN 'AFTER DELETE'
        WHEN 17 THEN 'BEFORE INSERT OR UPDATE'
        WHEN 18 THEN 'AFTER INSERT OR UPDATE'
        WHEN 25 THEN 'BEFORE INSERT OR DELETE'
        WHEN 26 THEN 'AFTER INSERT OR DELETE'
        WHEN 27 THEN 'BEFORE UPDATE OR DELETE'
        WHEN 28 THEN 'AFTER UPDATE OR DELETE'
        WHEN 113 THEN 'BEFORE INSERT OR UPDATE OR DELETE'
        WHEN 114 THEN 'AFTER INSERT OR UPDATE OR DELETE'
        ELSE 'TYPE_' || t.RDB$TRIGGER_TYPE
    END as TRIGGER_TYPE,
    t.RDB$TRIGGER_SEQUENCE as POSITION,
    CASE t.RDB$TRIGGER_INACTIVE
        WHEN 0 THEN 'ACTIVE'
        ELSE 'INACTIVE'
    END as STATUS
FROM RDB$TRIGGERS t
WHERE t.RDB$RELATION_NAME = 'ORDERS'
  AND (t.RDB$SYSTEM_FLAG = 0 OR t.RDB$SYSTEM_FLAG IS NULL)
ORDER BY t.RDB$TRIGGER_SEQUENCE;
```

#### **Database and DDL Triggers**
```sql
-- List database event triggers
SELECT 
    RDB$TRIGGER_NAME,
    CASE RDB$TRIGGER_TYPE
        WHEN 8192 THEN 'ON CONNECT'
        WHEN 8193 THEN 'ON DISCONNECT'
        WHEN 8194 THEN 'ON TRANSACTION START'
        WHEN 8195 THEN 'ON TRANSACTION COMMIT'
        WHEN 8196 THEN 'ON TRANSACTION ROLLBACK'
        ELSE 'DB_TYPE_' || RDB$TRIGGER_TYPE
    END as TRIGGER_TYPE,
    RDB$TRIGGER_SEQUENCE as POSITION,
    CASE RDB$TRIGGER_INACTIVE WHEN 0 THEN 'ACTIVE' ELSE 'INACTIVE' END as STATUS
FROM RDB$TRIGGERS
WHERE RDB$TRIGGER_TYPE >= 8192 AND RDB$TRIGGER_TYPE < 16384
  AND (RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL)
ORDER BY RDB$TRIGGER_TYPE, RDB$TRIGGER_SEQUENCE;

-- List DDL triggers
SELECT 
    RDB$TRIGGER_NAME,
    'DDL TRIGGER' as TRIGGER_TYPE,
    RDB$TRIGGER_SEQUENCE as POSITION,
    CASE RDB$TRIGGER_INACTIVE WHEN 0 THEN 'ACTIVE' ELSE 'INACTIVE' END as STATUS
FROM RDB$TRIGGERS
WHERE RDB$TRIGGER_TYPE >= 16384
  AND (RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL)
ORDER BY RDB$TRIGGER_SEQUENCE;
```

### **Trigger Source Code and Dependencies**
```sql
-- View trigger source code
SELECT 
    RDB$TRIGGER_NAME,
    RDB$TRIGGER_SOURCE,
    RDB$ENGINE_NAME,
    RDB$ENTRYPOINT
FROM RDB$TRIGGERS
WHERE RDB$TRIGGER_NAME = 'TR_AUDIT_CUSTOMERS'
  AND (RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL);

-- Find trigger dependencies
SELECT 
    t.RDB$TRIGGER_NAME as TRIGGER_NAME,
    dep.RDB$DEPENDED_ON_NAME as DEPENDS_ON,
    CASE dep.RDB$DEPENDED_ON_TYPE
        WHEN 0 THEN 'TABLE'
        WHEN 1 THEN 'VIEW'
        WHEN 5 THEN 'PROCEDURE'
        WHEN 15 THEN 'FUNCTION'
        ELSE 'OTHER'
    END as DEPENDENCY_TYPE
FROM RDB$TRIGGERS t
JOIN RDB$DEPENDENCIES dep ON t.RDB$TRIGGER_NAME = dep.RDB$DEPENDENT_NAME
WHERE t.RDB$SYSTEM_FLAG = 0 OR t.RDB$SYSTEM_FLAG IS NULL
  AND dep.RDB$DEPENDENT_TYPE = 2  -- Trigger type
ORDER BY t.RDB$TRIGGER_NAME, dep.RDB$DEPENDED_ON_NAME;
```

### **Schema-Aware Trigger Queries**

#### **Triggers by Schema Hierarchy**
```sql
-- List triggers grouped by schema hierarchy
SELECT 
    COALESCE(RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME,
    COUNT(*) as TRIGGER_COUNT,
    COUNT(CASE WHEN RDB$TRIGGER_INACTIVE = 0 THEN 1 END) as ACTIVE_TRIGGERS,
    COUNT(CASE WHEN RDB$TRIGGER_INACTIVE = 1 THEN 1 END) as INACTIVE_TRIGGERS,
    COUNT(CASE WHEN RDB$TRIGGER_SOURCE IS NOT NULL THEN 1 END) as PSQL_TRIGGERS,
    COUNT(CASE WHEN RDB$TRIGGER_SOURCE IS NULL THEN 1 END) as EXTERNAL_TRIGGERS
FROM RDB$TRIGGERS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
GROUP BY RDB$SCHEMA_NAME
ORDER BY SCHEMA_NAME;
```

---

## Advanced Trigger Patterns

### **Audit Trail Pattern**

#### **Comprehensive Audit System**
```sql
-- Generic audit trigger template
CREATE TRIGGER tr_audit_template
    AFTER INSERT OR UPDATE OR DELETE ON audit_target_table
    POSITION 1
AS
DECLARE
    audit_operation VARCHAR(10);
    old_values VARCHAR(4000);
    new_values VARCHAR(4000);
    changed_columns VARCHAR(1000);
BEGIN
    -- Determine operation
    audit_operation = CASE 
        WHEN INSERTING THEN 'INSERT'
        WHEN UPDATING THEN 'UPDATE'
        WHEN DELETING THEN 'DELETE'
    END;
    
    -- Build old values JSON (simplified)
    IF (NOT INSERTING) THEN
    BEGIN
        old_values = '{"id":' || OLD.id || 
                    ',"name":"' || COALESCE(OLD.name, '') || '"' ||
                    ',"status":"' || COALESCE(OLD.status, '') || '"}';
    END
    
    -- Build new values JSON (simplified)
    IF (NOT DELETING) THEN
    BEGIN
        new_values = '{"id":' || NEW.id || 
                    ',"name":"' || COALESCE(NEW.name, '') || '"' ||
                    ',"status":"' || COALESCE(NEW.status, '') || '"}';
    END
    
    -- Identify changed columns for updates
    IF (UPDATING) THEN
    BEGIN
        changed_columns = '';
        IF (COALESCE(OLD.name, '') <> COALESCE(NEW.name, '')) THEN
            changed_columns = changed_columns || 'name,';
        IF (COALESCE(OLD.status, '') <> COALESCE(NEW.status, '')) THEN
            changed_columns = changed_columns || 'status,';
        -- Remove trailing comma
        IF (changed_columns <> '') THEN
            changed_columns = LEFT(changed_columns, CHAR_LENGTH(changed_columns) - 1);
    END
    
    -- Insert audit record
    INSERT INTO audit_trail (
        table_name, record_id, operation_type,
        old_values, new_values, changed_columns,
        changed_by, changed_at, connection_id, transaction_id
    )
    VALUES (
        'AUDIT_TARGET_TABLE',
        COALESCE(NEW.id, OLD.id),
        :audit_operation,
        :old_values,
        :new_values,
        :changed_columns,
        CURRENT_USER,
        CURRENT_TIMESTAMP,
        CURRENT_CONNECTION,
        CURRENT_TRANSACTION
    );
END;
```

### **Data Synchronization Pattern**

#### **Multi-Table Synchronization**
```sql
-- Synchronization trigger for denormalized data
CREATE TRIGGER tr_customer_sync
    AFTER UPDATE ON customers
AS
BEGIN
    -- Update denormalized customer name in orders
    IF (OLD.first_name <> NEW.first_name OR 
        OLD.last_name <> NEW.last_name) THEN
    BEGIN
        UPDATE orders 
        SET customer_name = NEW.first_name || ' ' || NEW.last_name,
            last_updated = CURRENT_TIMESTAMP
        WHERE customer_id = NEW.customer_id;
    END
    
    -- Update customer status in related tables
    IF (OLD.status <> NEW.status) THEN
    BEGIN
        UPDATE customer_orders 
        SET customer_status = NEW.status
        WHERE customer_id = NEW.customer_id;
        
        UPDATE customer_payments
        SET customer_status = NEW.status
        WHERE customer_id = NEW.customer_id;
    END
    
    -- Invalidate cached data
    INSERT INTO cache_invalidation (table_name, record_id, invalidation_time)
    VALUES ('CUSTOMERS', NEW.customer_id, CURRENT_TIMESTAMP);
END;
```

### **Business Rule Enforcement Pattern**

#### **Complex Business Rules**
```sql
-- Multi-constraint business rule trigger
CREATE TRIGGER tr_order_business_rules
    BEFORE INSERT OR UPDATE ON orders
    POSITION 5
AS
DECLARE
    customer_credit_limit DECIMAL(15,2);
    current_outstanding DECIMAL(15,2);
    order_limit_exceeded EXCEPTION 'Order exceeds customer credit limit';
    invalid_discount EXCEPTION 'Discount percentage exceeds allowed limit';
    seasonal_restriction EXCEPTION 'Product not available in current season';
BEGIN
    -- Credit limit check
    SELECT credit_limit FROM customers 
    WHERE customer_id = NEW.customer_id
    INTO :customer_credit_limit;
    
    SELECT COALESCE(SUM(total_amount), 0)
    FROM orders 
    WHERE customer_id = NEW.customer_id 
      AND status IN ('PENDING', 'PROCESSING')
    INTO :current_outstanding;
    
    IF (current_outstanding + NEW.total_amount > customer_credit_limit) THEN
        EXCEPTION order_limit_exceeded;
    
    -- Discount validation
    IF (NEW.discount_percentage > 25.0) THEN
    BEGIN
        -- Check if user has authority for high discounts
        IF (NOT EXISTS(SELECT 1 FROM user_privileges 
                      WHERE username = CURRENT_USER 
                        AND privilege = 'HIGH_DISCOUNT')) THEN
            EXCEPTION invalid_discount;
    END
    
    -- Seasonal product restrictions
    IF (EXISTS(SELECT 1 FROM order_items oi
               JOIN products p ON oi.product_id = p.product_id
               WHERE oi.order_id = NEW.order_id
                 AND p.seasonal_restriction IS NOT NULL
                 AND EXTRACT(MONTH FROM CURRENT_DATE) NOT BETWEEN 
                     p.season_start_month AND p.season_end_month)) THEN
        EXCEPTION seasonal_restriction;
    
    -- Auto-apply business rules
    IF (NEW.total_amount > 1000) THEN
        NEW.requires_approval = TRUE;
    
    IF (NEW.shipping_country <> 'USA') THEN
        NEW.international_fee = NEW.total_amount * 0.03;
END;
```

---

## Error Handling and Troubleshooting

### **Common Trigger Errors**

#### **Trigger Creation Errors**
```sql
-- Error: Trigger already exists
CREATE TRIGGER duplicate_trigger_name AFTER INSERT ON table1 AS BEGIN END;
-- Solution: Use CREATE OR ALTER or IF NOT EXISTS

-- Error: Invalid trigger timing/event combination
CREATE TRIGGER invalid_trigger BEFORE CONNECT AS BEGIN END;  -- CONNECT triggers can't be BEFORE
-- Solution: Use proper timing for trigger type

-- Error: Invalid table reference
CREATE TRIGGER tr_test AFTER INSERT ON nonexistent_table AS BEGIN END;
-- Solution: Ensure target table exists

-- Error: Syntax error in trigger body
CREATE TRIGGER tr_test AFTER INSERT ON table1 AS BEGIN INVALID SQL; END;
-- Solution: Check PSQL syntax in trigger body
```

#### **Runtime Trigger Errors**
```sql
-- Error: Mutating table error (trigger modifying its own table)
CREATE TRIGGER tr_problematic
    AFTER INSERT ON orders
AS
BEGIN
    -- This can cause infinite recursion
    INSERT INTO orders (customer_id, total) VALUES (999, 100);
END;
-- Solution: Use BEFORE triggers or avoid self-modification

-- Error: Exception in trigger halts operation
CREATE TRIGGER tr_with_exception
    BEFORE INSERT ON orders
AS
BEGIN
    IF (NEW.total <= 0) THEN
        EXCEPTION 'Invalid order total';  -- This will prevent INSERT
END;
-- Solution: Handle exceptions appropriately or use AFTER triggers for logging
```

### **Debugging Triggers**

#### **Trigger Debugging Techniques**
```sql
-- Debug trigger with logging
CREATE TRIGGER tr_debug_example
    BEFORE INSERT OR UPDATE ON debug_table
AS
BEGIN
    -- Log trigger execution
    INSERT INTO trigger_debug_log (
        trigger_name, operation, record_id, execution_time
    )
    VALUES (
        'TR_DEBUG_EXAMPLE',
        CASE WHEN INSERTING THEN 'INSERT' ELSE 'UPDATE' END,
        NEW.id,
        CURRENT_TIMESTAMP
    );
    
    -- Log variable values
    INSERT INTO trigger_debug_log (
        trigger_name, debug_message, execution_time
    )
    VALUES (
        'TR_DEBUG_EXAMPLE',
        'Processing record with total: ' || CAST(NEW.total AS VARCHAR(20)),
        CURRENT_TIMESTAMP
    );
END;
```

#### **Trigger Performance Analysis**
```sql
-- Monitor trigger execution statistics
SELECT 
    t.RDB$TRIGGER_NAME,
    ts.MON$STAT_ID,
    ts.MON$STAT_TIME,
    ts.MON$MEMORY_USED,
    ts.MON$MEMORY_ALLOCATED
FROM RDB$TRIGGERS t
JOIN MON$TRIGGER_STATS ts ON t.RDB$TRIGGER_NAME = ts.MON$TRIGGER_NAME
WHERE t.RDB$SYSTEM_FLAG = 0
ORDER BY ts.MON$STAT_TIME DESC;
```

---

## Best Practices

### **Trigger Design Guidelines**

1. **Keep Triggers Simple**: Minimize complex logic in triggers
2. **Avoid Recursive Triggers**: Prevent triggers from modifying their own tables
3. **Use Appropriate Timing**: BEFORE for validation, AFTER for logging
4. **Position Management**: Use POSITION to control execution order
5. **Exception Handling**: Handle exceptions appropriately for trigger type
6. **Performance Considerations**: Avoid expensive operations in high-frequency triggers

### **Recommended Trigger Patterns**

#### **Audit Trigger Pattern**
```sql
CREATE TRIGGER tr_audit_pattern
    AFTER INSERT OR UPDATE OR DELETE ON target_table
    POSITION 1
AS
BEGIN
    INSERT INTO audit_log (
        table_name, record_id, operation, 
        changed_by, changed_at
    )
    VALUES (
        'TARGET_TABLE',
        COALESCE(NEW.id, OLD.id),
        CASE WHEN INSERTING THEN 'I'
             WHEN UPDATING THEN 'U' 
             WHEN DELETING THEN 'D' END,
        CURRENT_USER,
        CURRENT_TIMESTAMP
    );
END;
```

#### **Validation Trigger Pattern**
```sql
CREATE TRIGGER tr_validation_pattern
    BEFORE INSERT OR UPDATE ON target_table
    POSITION 1
AS
DECLARE
    validation_error EXCEPTION 'Data validation failed';
BEGIN
    -- Validate required fields
    IF (NEW.required_field IS NULL) THEN
        EXCEPTION validation_error USING ('Required field cannot be null');
    
    -- Validate business rules
    IF (NEW.amount < 0) THEN
        EXCEPTION validation_error USING ('Amount must be positive');
    
    -- Set default values
    IF (NEW.status IS NULL) THEN
        NEW.status = 'ACTIVE';
END;
```

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:4476-4655`
- **Classes**: `trigger_clause`, `create_trigger_start`, `create_trigger_common`
- **Functionality**: Parsing CREATE/ALTER/DROP TRIGGER syntax with events and timing

#### **DDL Node Classes**
- **File**: `src/dsql/DdlNodes.h:696-812`
- **Classes**:
  - `CreateAlterTriggerNode` (lines 696-774): Trigger creation and modification
  - `DropTriggerNode` (lines 777-803): Trigger removal
  - `RecreateTriggerNode` (lines 806-812): Atomic drop-and-recreate operations

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:465-485`
- **Table**: `RDB$TRIGGERS` - Stores trigger definitions
- **Fields**:
  - `f_trg_name`: Trigger name
  - `f_trg_relation`: Target table/relation
  - `f_trg_type`: Trigger type and timing
  - `f_trg_sequence`: Execution position
  - `f_trg_source`: PSQL source code
  - `f_trg_inactive`: Active/inactive state

### **Core Trigger Operations**

#### **CreateAlterTriggerNode Methods**
- Handles both CREATE and ALTER operations
- Event type validation and encoding
- Position management and ordering
- PSQL source code compilation
- External trigger registration
- Active/inactive state management

#### **Trigger Execution Engine**
- Event detection and trigger firing
- OLD/NEW variable population
- Context variable management
- Exception handling and propagation
- Position-based execution ordering

#### **Storage Structures**

Triggers are stored in the RDB$TRIGGERS system table with:
- **Identity**: Name, schema, target relation
- **Event Configuration**: Trigger type, timing, events
- **Execution Control**: Position, active/inactive state
- **Implementation**: PSQL source or external module reference
- **Security**: SQL security mode and ownership details

---

## Administrative Operations

### **Trigger Maintenance**

#### **Trigger Performance Monitoring**
```sql
-- Monitor trigger execution patterns
CREATE VIEW trigger_performance AS
SELECT 
    t.RDB$TRIGGER_NAME,
    t.RDB$RELATION_NAME,
    COUNT(ts.MON$STAT_ID) as EXECUTION_COUNT,
    AVG(ts.MON$STAT_TIME) as AVG_EXECUTION_TIME,
    MAX(ts.MON$STAT_TIME) as MAX_EXECUTION_TIME,
    SUM(ts.MON$MEMORY_USED) as TOTAL_MEMORY_USED
FROM RDB$TRIGGERS t
LEFT JOIN MON$TRIGGER_STATS ts ON t.RDB$TRIGGER_NAME = ts.MON$TRIGGER_NAME
WHERE t.RDB$SYSTEM_FLAG = 0 OR t.RDB$SYSTEM_FLAG IS NULL
GROUP BY t.RDB$TRIGGER_NAME, t.RDB$RELATION_NAME
ORDER BY EXECUTION_COUNT DESC;
```

#### **Trigger Management Procedures**
```sql
-- Procedure to disable all triggers on a table
CREATE PROCEDURE disable_table_triggers(
    IN table_name VARCHAR(63)
)
AS
DECLARE
    trigger_name VARCHAR(63);
    trigger_cursor CURSOR FOR (
        SELECT RDB$TRIGGER_NAME
        FROM RDB$TRIGGERS 
        WHERE RDB$RELATION_NAME = :table_name
          AND RDB$TRIGGER_INACTIVE = 0
          AND (RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL)
    );
BEGIN
    OPEN trigger_cursor;
    
    WHILE (1 = 1) DO
    BEGIN
        FETCH trigger_cursor INTO :trigger_name;
        IF (ROW_COUNT = 0) THEN LEAVE;
        
        EXECUTE STATEMENT 'ALTER TRIGGER ' || trigger_name || ' INACTIVE';
    END
    
    CLOSE trigger_cursor;
END;
```

---

