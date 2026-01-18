[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL Programmable SQL

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

This document covers programmable SQL objects in MySQL emulation mode, including stored procedures, functions, triggers, and dynamic SQL execution.

**Important:** Programmable SQL features are not currently implemented in the MySQL parser. This documentation describes the intended MySQL syntax and behavior for future implementation.

---

## Stored Procedures

### Overview

Stored procedures are named SQL code blocks that can accept parameters, contain logic (variables, loops, conditions), and return results.

### CREATE PROCEDURE

Creates a stored procedure.

#### Syntax

```sql
CREATE [DEFINER = user] PROCEDURE procedure_name ([parameter[,...]])
    [characteristic ...] routine_body

parameter:
    [ IN | OUT | INOUT ] param_name type

characteristic:
    COMMENT 'string'
  | LANGUAGE SQL
  | [NOT] DETERMINISTIC
  | { CONTAINS SQL | NO SQL | READS SQL DATA | MODIFIES SQL DATA }
  | SQL SECURITY { DEFINER | INVOKER }

routine_body:
    Valid SQL procedure statement
```

#### Parameters

- `DEFINER`: User context for procedure execution
- `IN`: Input parameter (default)
- `OUT`: Output parameter
- `INOUT`: Both input and output parameter
- `DETERMINISTIC`: Same inputs always produce same results
- `CONTAINS SQL`: Contains SQL but no read/write operations
- `NO SQL`: No SQL statements
- `READS SQL DATA`: Reads data but no writes
- `MODIFIES SQL DATA`: Reads and writes data
- `SQL SECURITY`: Security context (DEFINER or INVOKER)

#### Examples

**Basic procedure:**
```sql
CREATE PROCEDURE get_user_count()
BEGIN
    SELECT COUNT(*) FROM users;
END;
```

**Procedure with IN parameter:**
```sql
CREATE PROCEDURE get_user_by_id(IN user_id INT)
BEGIN
    SELECT * FROM users WHERE id = user_id;
END;
```

**Procedure with OUT parameter:**
```sql
CREATE PROCEDURE get_total_sales(OUT total DECIMAL(10,2))
BEGIN
    SELECT SUM(amount) INTO total FROM sales;
END;
```

**Procedure with INOUT parameter:**
```sql
CREATE PROCEDURE increment_value(INOUT val INT)
BEGIN
    SET val = val + 1;
END;
```

**Complex procedure with variables and logic:**
```sql
DELIMITER //
CREATE PROCEDURE process_order(IN order_id INT, OUT status VARCHAR(50))
BEGIN
    DECLARE order_total DECIMAL(10,2);
    DECLARE customer_balance DECIMAL(10,2);

    SELECT total INTO order_total FROM orders WHERE id = order_id;
    SELECT balance INTO customer_balance FROM customers
        WHERE id = (SELECT customer_id FROM orders WHERE id = order_id);

    IF customer_balance >= order_total THEN
        UPDATE orders SET status = 'approved' WHERE id = order_id;
        SET status = 'approved';
    ELSE
        UPDATE orders SET status = 'rejected' WHERE id = order_id;
        SET status = 'rejected';
    END IF;
END //
DELIMITER ;
```

**Procedure with cursor:**
```sql
CREATE PROCEDURE list_users()
BEGIN
    DECLARE done INT DEFAULT FALSE;
    DECLARE user_name VARCHAR(100);
    DECLARE cur CURSOR FOR SELECT name FROM users;
    DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = TRUE;

    OPEN cur;

    read_loop: LOOP
        FETCH cur INTO user_name;
        IF done THEN
            LEAVE read_loop;
        END IF;
        SELECT user_name;
    END LOOP;

    CLOSE cur;
END;
```

#### Current Status

**NOT IMPLEMENTED:** CREATE PROCEDURE is not currently implemented in the MySQL parser and will result in parse errors.

### CALL

Executes a stored procedure.

#### Syntax

```sql
CALL procedure_name([parameter[,...]])
```

#### Examples

**Call procedure with no parameters:**
```sql
CALL get_user_count();
```

**Call with IN parameter:**
```sql
CALL get_user_by_id(42);
```

**Call with OUT parameter:**
```sql
CALL get_total_sales(@total);
SELECT @total;
```

**Call with INOUT parameter:**
```sql
SET @val = 10;
CALL increment_value(@val);
SELECT @val;  -- Returns 11
```

#### Current Status

**NOT IMPLEMENTED:** CALL is not currently implemented in the MySQL parser.

### DROP PROCEDURE

Removes a stored procedure.

#### Syntax

```sql
DROP PROCEDURE [IF EXISTS] procedure_name
```

#### Examples

```sql
DROP PROCEDURE get_user_count;
DROP PROCEDURE IF EXISTS process_order;
```

#### Current Status

**NOT IMPLEMENTED:** DROP PROCEDURE is not currently implemented.

### ALTER PROCEDURE

Modifies procedure characteristics (not the body).

#### Syntax

```sql
ALTER PROCEDURE procedure_name [characteristic ...]
```

#### Current Status

**NOT IMPLEMENTED:** ALTER PROCEDURE is not currently implemented.

---

## Stored Functions

### Overview

Stored functions are similar to procedures but return a single value and can be used in SQL expressions.

### CREATE FUNCTION

Creates a stored function.

#### Syntax

```sql
CREATE [DEFINER = user] FUNCTION function_name ([parameter[,...]])
    RETURNS type
    [characteristic ...] routine_body

parameter:
    param_name type
```

#### Examples

**Simple function:**
```sql
CREATE FUNCTION get_tax(amount DECIMAL(10,2))
    RETURNS DECIMAL(10,2)
    DETERMINISTIC
BEGIN
    RETURN amount * 0.10;
END;
```

**Function with logic:**
```sql
CREATE FUNCTION get_discount_rate(customer_id INT)
    RETURNS DECIMAL(5,2)
    READS SQL DATA
BEGIN
    DECLARE total_orders INT;
    SELECT COUNT(*) INTO total_orders FROM orders WHERE customer_id = customer_id;

    IF total_orders >= 100 THEN
        RETURN 0.20;
    ELSEIF total_orders >= 50 THEN
        RETURN 0.15;
    ELSEIF total_orders >= 10 THEN
        RETURN 0.10;
    ELSE
        RETURN 0.05;
    END IF;
END;
```

**Using a function in queries:**
```sql
SELECT product_id, price, get_tax(price) AS tax
FROM products;

SELECT * FROM orders
WHERE total > 100 * (1 + get_tax(100));
```

#### Current Status

**NOT IMPLEMENTED:** CREATE FUNCTION is not currently implemented in the MySQL parser.

### DROP FUNCTION

Removes a stored function.

#### Syntax

```sql
DROP FUNCTION [IF EXISTS] function_name
```

#### Current Status

**NOT IMPLEMENTED:** DROP FUNCTION is not currently implemented.

---

## Triggers

### Overview

Triggers are database callbacks that automatically execute in response to specific events on a table.

Trigger quick reference: [Trigger Cheat Sheet](../../user-guides/Trigger-Cheat-Sheet.md)

### CREATE TRIGGER

Creates a trigger.

#### Syntax

```sql
CREATE [DEFINER = user] TRIGGER trigger_name
    trigger_time trigger_event
    ON table_name FOR EACH ROW
    [trigger_order]
    trigger_body

trigger_time: { BEFORE | AFTER }

trigger_event: { INSERT | UPDATE | DELETE }

trigger_order: { FOLLOWS | PRECEDES } other_trigger_name
```

#### Parameters

- `BEFORE`: Execute before the triggering event
- `AFTER`: Execute after the triggering event
- `FOR EACH ROW`: Trigger executes for each affected row
- `FOLLOWS/PRECEDES`: Order relative to other triggers

#### Examples

**BEFORE INSERT trigger:**
```sql
CREATE TRIGGER validate_user_email
BEFORE INSERT ON users
FOR EACH ROW
BEGIN
    IF NEW.email NOT LIKE '%@%' THEN
        SIGNAL SQLSTATE '45000'
        SET MESSAGE_TEXT = 'Invalid email format';
    END IF;
END;
```

**AFTER INSERT trigger:**
```sql
CREATE TRIGGER log_new_order
AFTER INSERT ON orders
FOR EACH ROW
BEGIN
    INSERT INTO order_log (order_id, action, created_at)
    VALUES (NEW.id, 'created', NOW());
END;
```

**BEFORE UPDATE trigger:**
```sql
CREATE TRIGGER update_modified_timestamp
BEFORE UPDATE ON products
FOR EACH ROW
BEGIN
    SET NEW.updated_at = NOW();
END;
```

**AFTER UPDATE trigger:**
```sql
CREATE TRIGGER track_price_changes
AFTER UPDATE ON products
FOR EACH ROW
BEGIN
    IF OLD.price != NEW.price THEN
        INSERT INTO price_history (product_id, old_price, new_price, changed_at)
        VALUES (NEW.id, OLD.price, NEW.price, NOW());
    END IF;
END;
```

**BEFORE DELETE trigger:**
```sql
CREATE TRIGGER prevent_admin_delete
BEFORE DELETE ON users
FOR EACH ROW
BEGIN
    IF OLD.role = 'admin' THEN
        SIGNAL SQLSTATE '45000'
        SET MESSAGE_TEXT = 'Cannot delete admin users';
    END IF;
END;
```

**AFTER DELETE trigger:**
```sql
CREATE TRIGGER cleanup_user_data
AFTER DELETE ON users
FOR EACH ROW
BEGIN
    DELETE FROM user_sessions WHERE user_id = OLD.id;
    DELETE FROM user_preferences WHERE user_id = OLD.id;
END;
```

**Trigger ordering:**
```sql
CREATE TRIGGER log_order_first
AFTER INSERT ON orders
FOR EACH ROW
BEGIN
    INSERT INTO audit_log VALUES (NEW.id, 'order_created', NOW());
END;

CREATE TRIGGER update_inventory_second
AFTER INSERT ON orders
FOR EACH ROW
FOLLOWS log_order_first
BEGIN
    UPDATE inventory SET quantity = quantity - NEW.quantity
    WHERE product_id = NEW.product_id;
END;
```

#### Trigger Context Variables (NEW / OLD)

**Description**

Row-level triggers in MySQL expose pseudo-records for row values before/after
the change.

**Syntax**
```sql
NEW.<column>
OLD.<column>
```

**Examples**
```sql
SET NEW.updated_at = NOW();
INSERT INTO audit_log (id, old_price, new_price)
VALUES (NEW.id, OLD.price, NEW.price);
```

**Status**
- Parsed as part of the trigger body text only; the SQL body is not executed.
- Runtime trigger execution requires a registered C++ trigger procedure
  (see `Executor::registerTriggerProcedure`).

#### Current Status

**PARTIAL:** CREATE TRIGGER is parsed and stored. The trigger body is captured
as text (stored in an implicit procedure), but no SQL trigger interpreter runs
it at execution time.

### DROP TRIGGER

Removes a trigger.

#### Syntax

```sql
DROP TRIGGER [IF EXISTS] [schema_name.]trigger_name
```

#### Examples

```sql
DROP TRIGGER validate_user_email;
DROP TRIGGER IF EXISTS mydb.log_new_order;
```

#### Current Status

**NOT IMPLEMENTED:** DROP TRIGGER is not currently implemented.

---

## Procedural SQL Constructs

### Variables

#### Declare Variables

```sql
DECLARE variable_name type [DEFAULT value];
```

**Examples:**
```sql
DECLARE user_count INT DEFAULT 0;
DECLARE user_name VARCHAR(100);
DECLARE total_amount DECIMAL(10,2) DEFAULT 0.0;
```

#### Set Variables

```sql
SET variable_name = value;
SET variable_name := value;
```

**Examples:**
```sql
SET user_count = 10;
SELECT COUNT(*) INTO user_count FROM users;
```

### Control Flow

#### IF Statement

```sql
IF condition THEN
    statements
[ELSEIF condition THEN
    statements]
[ELSE
    statements]
END IF;
```

#### CASE Statement

```sql
CASE case_value
    WHEN when_value THEN statements
    [WHEN when_value THEN statements ...]
    [ELSE statements]
END CASE;
```

#### LOOP

```sql
[label:] LOOP
    statements
END LOOP [label];
```

#### WHILE Loop

```sql
[label:] WHILE condition DO
    statements
END WHILE [label];
```

#### REPEAT Loop

```sql
[label:] REPEAT
    statements
UNTIL condition
END REPEAT [label];
```

### Cursors

#### Declare Cursor

```sql
DECLARE cursor_name CURSOR FOR select_statement;
```

#### Open Cursor

```sql
OPEN cursor_name;
```

#### Fetch from Cursor

```sql
FETCH cursor_name INTO variable [, variable ...];
```

#### Close Cursor

```sql
CLOSE cursor_name;
```

### Error Handling

#### DECLARE HANDLER

```sql
DECLARE handler_type HANDLER
    FOR condition_value [, condition_value ...]
    statement

handler_type:
    CONTINUE | EXIT | UNDO

condition_value:
    SQLSTATE [VALUE] sqlstate_value
  | condition_name
  | SQLWARNING
  | NOT FOUND
  | SQLEXCEPTION
```

**Example:**
```sql
DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = TRUE;
DECLARE EXIT HANDLER FOR SQLEXCEPTION
BEGIN
    ROLLBACK;
    SELECT 'An error occurred' AS message;
END;
```

---

## Dynamic SQL

### PREPARE

Prepares a statement for execution.

#### Syntax

```sql
PREPARE stmt_name FROM preparable_stmt;
```

#### Examples

```sql
PREPARE stmt FROM 'SELECT * FROM users WHERE id = ?';
SET @id = 1;
EXECUTE stmt USING @id;
DEALLOCATE PREPARE stmt;
```

#### Current Status

**NOT IMPLEMENTED:** PREPARE is not currently implemented.

### EXECUTE

Executes a prepared statement.

#### Syntax

```sql
EXECUTE stmt_name [USING @variable [, @variable ...]];
```

#### Current Status

**NOT IMPLEMENTED:** EXECUTE is not currently implemented.

### DEALLOCATE PREPARE

Releases a prepared statement.

#### Syntax

```sql
{DEALLOCATE | DROP} PREPARE stmt_name;
```

#### Current Status

**NOT IMPLEMENTED:** DEALLOCATE PREPARE is not currently implemented.

---

## Known Limitations

### Missing Features

- **CREATE PROCEDURE**: Not implemented in the parser
  - **Priority**: High (Beta target)
  - **Workaround**: Use application-level logic

- **CREATE FUNCTION**: Not implemented in the parser
  - **Priority**: High (Beta target)
  - **Workaround**: Use application-level functions or inline expressions

- **CREATE TRIGGER**: Not implemented in the parser
  - **Priority**: High (Beta target)
  - **Workaround**: Implement trigger logic in application code

- **CALL statement**: Not implemented
  - **Priority**: High (Beta target)

- **EXECUTE/PREPARE**: Dynamic SQL not implemented
  - **Priority**: Medium (Beta target)

- **All procedural constructs**: Variables, control flow, cursors, error handlers
  - **Priority**: High (Beta target)
  - **Workaround**: Use application-level logic

### Spec Deltas

- **Procedural language**: MySQL's procedural SQL differs from other databases
- **Trigger timing**: BEFORE/AFTER semantics must match MySQL behavior
- **Function determinism**: Deterministic vs non-deterministic function behavior

### Implementation Priority

**Beta Target:**
- CREATE PROCEDURE and CALL (2-3 weeks)
- CREATE FUNCTION (1-2 weeks)
- CREATE TRIGGER (1-2 weeks)
- Procedural constructs (2-3 weeks)
- Dynamic SQL (1 week)

**Total estimated effort:** 7-11 weeks for complete programmable SQL support
