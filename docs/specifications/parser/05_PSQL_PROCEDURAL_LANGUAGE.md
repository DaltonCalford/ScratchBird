# **PSQL Procedural Language**

## **1\. Introduction**

PSQL is the powerful, fully-featured procedural language of ScratchBird, designed to execute complex logic directly within the database engine. It enables developers to encapsulate business rules, create robust data processing routines, and build sophisticated APIs by moving application logic closer to the data.

### **Core PSQL Principles**

* **Compilation to Bytecode**: All PSQL code—whether in a procedure, function, or an anonymous EXECUTE BLOCK—is compiled into a highly efficient, stack-based bytecode format called SBLR (ScratchBird Binary Language Representation). This bytecode is then executed by a dedicated virtual machine, which supports advanced optimizations like adaptive specialization and JIT (Just-In-Time) compilation for hot code paths.  
* **Strong Typing and Flexibility**: The language is strongly typed, leveraging the full ScratchBird type system, including advanced DOMAIN types. It also supports polymorphic VARIANT variables for maximum flexibility.  
* **Robust Control and Error Handling**: PSQL provides a full suite of control flow statements (IF, CASE, loops) and a modern, powerful TRY/EXCEPT block for structured exception handling.  
* **Seamless SQL Integration**: PSQL is tightly integrated with DML. Variables can be assigned directly from query results (SELECT INTO), and result sets can be iterated over (FOR SELECT) or manipulated as first-class SET objects.

## **2\. Program Structure and Variables**

### **2.1. Blocks (BEGIN...END)**

The fundamental unit of a PSQL program is the block. A block can contain variable declarations, executable statements, and exception handlers.

\[ DECLARE  
    variable\_name \[CONSTANT\] datatype \[DEFAULT value\];  
    ... \]  
BEGIN  
    \-- executable statements  
    ...  
\[ EXCEPTION  
    WHEN condition THEN  
        \-- error handling statements  
    ... \]  
END;

### **2.2. Variable Declaration**

Variables must be declared in the DECLARE section of a block before they are used.

DECLARE  
    \-- Standard types  
    @counter       INTEGER := 0;  
    @customer\_name VARCHAR(100);

    \-- Using a DOMAIN for type safety  
    @current\_status order\_status;

    \-- A record variable to hold an entire row  
    @product\_rec    products%ROWTYPE;

    \-- A constant value  
    @tax\_rate      CONSTANT DECIMAL(5,4) \= 0.0825;

### **2.3. Assignment**

Variables can be assigned values using the SET command or by selecting a single row of data directly into them.

\-- Direct assignment  
SET @counter \= @counter \+ 1;

\-- Assigning from a query (must return exactly one row)  
SELECT name, price  
FROM products  
WHERE product\_id \= 123  
INTO @customer\_name, @product\_rec.price;

## **3\. Control Flow Statements**

### **3.1. Conditional Logic (IF, CASE)**

\-- IF-ELSIF-ELSE statement  
IF @stock \< 10 THEN  
    RAISE NOTICE 'Stock is low for product %', @product\_id;  
ELSIF @stock \< 50 THEN  
    \-- do something else  
ELSE  
    \-- do a third thing  
END IF;

\-- CASE statement  
CASE @status\_code  
    WHEN 200 THEN SET @message \= 'OK';  
    WHEN 404 THEN SET @message \= 'Not Found';  
    ELSE SET @message \= 'Unknown Error';  
END CASE;

#### **CASE statement syntax (PSQL control flow)**
Status: Planned (PSQL statement-level CASE; distinct from CASE expression).

**Simple CASE**
```
CASE <expr>
    WHEN <expr> THEN <statement> [;]
    [ WHEN <expr> THEN <statement> [;] ]*
    [ ELSE <statement> [;] ]
END CASE;
```

**Searched CASE**
```
CASE
    WHEN <condition> THEN <statement> [;]
    [ WHEN <condition> THEN <statement> [;] ]*
    [ ELSE <statement> [;] ]
END CASE;
```

Notes:
- `<statement>` may be a single PSQL statement or a BEGIN...END block.
- This is a statement-level control structure. For expression use (inside DML),
  use the CASE expression in SQL.

### **3.2. Loops (LOOP, WHILE, FOR)**

\-- Basic LOOP with an EXIT condition  
LOOP  
    SET @counter \= @counter \+ 1;  
    EXIT WHEN @counter \> 100;  
END LOOP;

\-- WHILE loop  
WHILE @counter \< 100 LOOP  
    \-- statements  
    SET @counter \= @counter \+ 1;  
END LOOP;

\-- FOR loop for a range of integers  
FOR i IN 1..10 LOOP  
    INSERT INTO numbers (value) VALUES (i);  
END LOOP;

\-- FOR loop to iterate over a query result  
FOR @product\_rec IN SELECT \* FROM products WHERE category \= 'Beverages' LOOP  
    RAISE NOTICE 'Processing product: %', @product\_rec.name;  
END LOOP;

## **4\. Cursors and Result Set Handling**

### **4.1. Universal Cursors**

ScratchBird features **Universal Cursors**, which can be declared for any multi-record data source, not just SELECT statements.

\-- Declare a cursor for a table, view, set-returning function, or SET variable  
DECLARE @user\_cursor CURSOR FOR users;  
DECLARE @report\_cursor CURSOR FOR SELECT \* FROM quarterly\_report\_view;  
DECLARE @active\_users\_cursor CURSOR FOR get\_active\_users(); \-- Function call  
DECLARE @my\_set\_cursor CURSOR FOR @my\_set\_variable; \-- SET type

### **4.2. Cursor Operations**

Cursors provide fine-grained, row-by-row control over a result set.

DECLARE  
    @order\_cursor CURSOR SCROLL FOR SELECT \* FROM orders FOR UPDATE;  
    @order\_rec orders%ROWTYPE;  
BEGIN  
    OPEN @order\_cursor;

    FETCH FIRST FROM @order\_cursor INTO @order\_rec; \-- Fetch the first row  
    RAISE NOTICE 'First order is %', @order\_rec.order\_id;

    FETCH RELATIVE 5 FROM @order\_cursor INTO @order\_rec; \-- Move 5 rows forward  
    RAISE NOTICE 'Sixth order is %', @order\_rec.order\_id;

    \-- Modify the row the cursor is currently on  
    UPDATE orders SET status \= 'FLAGGED' WHERE CURRENT OF @order\_cursor;

    CLOSE @order\_cursor;  
END;

### **4.3. FOR SELECT ... INTO Loop**

For simple, forward-only iteration, the FOR SELECT loop is a convenient alternative to manual cursor handling.

DECLARE @total\_price MONEY \= 0;  
BEGIN  
    FOR @order\_rec IN SELECT \* FROM order\_details WHERE order\_id \= 101 LOOP  
        SET @total\_price \= @total\_price \+ (@order\_rec.quantity \* @order\_rec.unit\_price);  
    END LOOP;  
END;

### **4.4. Automatic Set Conversion**

Any SELECT statement can be implicitly or explicitly converted to a SET type. A SET variable holds the entire result set in memory, allowing it to be passed to other procedures or iterated over multiple times with full, scrollable cursor support.

DECLARE  
    @active\_customers\_set SET OF customers%ROWTYPE;  
BEGIN  
    \-- Automatically convert the SELECT result into a set  
    @active\_customers\_set \= (SELECT \* FROM customers WHERE status \= 'ACTIVE');

    \-- Now you can use a fully scrollable cursor on this in-memory set  
    DECLARE @cust\_cursor CURSOR SCROLL FOR @active\_customers\_set;  
    ...  
END;

### **4.5. Cursor Handle Passing**

PSQL supports passing live cursor handles into and out of procedures and
functions without copying result data. This enables shared cursor state
between routines (same snapshot and position).

See: `ScratchBird/docs/specifications/parser/PSQL_CURSOR_HANDLES.md`

## **5\. Structured Exception Handling**

PSQL uses a modern TRY/EXCEPT block for robust error handling.

DECLARE @error\_info exception\_info;  
BEGIN  
    TRY  
        \-- A block of code that might fail  
        UPDATE accounts SET balance \= balance \- 100 WHERE id \= @account\_id;  
        IF (SELECT balance FROM accounts WHERE id \= @account\_id) \< 0 THEN  
            RAISE insufficient\_funds; \-- Raise a user-defined exception  
        END IF;

    \-- Catch a specific, user-defined exception  
    EXCEPT WHEN insufficient\_funds THEN  
        RAISE WARNING 'Account has insufficient funds.';  
        ROLLBACK;

    \-- Catch a standard SQL error by its SQLSTATE code  
    EXCEPT WHEN SQLSTATE '23505' THEN  
        RAISE NOTICE 'Duplicate key violation occurred.';

    \-- Catch all other exceptions  
    EXCEPT WHEN OTHERS THEN  
        SET @error\_info \= GET EXCEPTION\_INFO; \-- Get detailed error context  
        INSERT INTO error\_log (message, detail, stack\_trace)  
        VALUES (@error\_info.message, @error\_info.detail, @error\_info.stack\_trace);  
        RAISE; \-- Re-raise the original exception  
    END TRY;  
END;

## **6\. Anonymous Blocks (EXECUTE BLOCK)**

EXECUTE BLOCK allows you to run a PSQL code block on the fly without needing to create a stored procedure. This is ideal for scripting and complex, multi-statement operations.

\-- An anonymous block to archive old data  
EXECUTE BLOCK (archive\_date DATE \= CURRENT\_DATE \- INTERVAL '1 year')  
AS  
BEGIN  
    INSERT INTO logs\_archive SELECT \* FROM logs WHERE log\_date \< archive\_date;  
    DELETE FROM logs WHERE log\_date \< archive\_date;  
END;

## **7\. Autonomous Transactions and Block-Level Triggers**

This is a powerful and unique feature of ScratchBird's PSQL, tightly integrated with EXECUTE BLOCK.

### **7.1. WITH AUTONOMOUS TRANSACTION**

An autonomous transaction is an independent transaction that can be committed or rolled back regardless of the state of the parent transaction. This is essential for tasks like logging or auditing, where you need to record an action even if the main operation fails.

BEGIN; \-- Start main transaction  
    UPDATE products SET inventory \= inventory \- 1 WHERE id \= 123;

    \-- This block runs in its own transaction  
    EXECUTE BLOCK WITH AUTONOMOUS TRANSACTION AS  
    BEGIN  
        \-- This log entry will be saved even if the main transaction rolls back  
        INSERT INTO audit\_log (action, user, timestamp)  
        VALUES ('Product 123 inventory updated', CURRENT\_USER, NOW());  
        COMMIT; \-- This commits only the autonomous block's work  
    END;

\-- This might fail, causing the main transaction to roll back  
UPDATE accounts SET balance \= balance \- 99.99 WHERE ...

ROLLBACK; \-- The inventory update is undone, but the audit\_log entry remains.

### **7.2. Block-Level Triggers (ON COMMIT/ON ROLLBACK)**

Within an autonomous block, you can define actions that fire when that specific block is committed or rolled back.

EXECUTE BLOCK WITH AUTONOMOUS TRANSACTION AS  
BEGIN  
    \-- Define actions to take based on the block's outcome  
    ON COMMIT DO  
        PERFORM pg\_notify('batch\_processor', 'SUCCESS');

    ON ROLLBACK DO  
        PERFORM pg\_notify('batch\_processor', 'FAILURE');

    \-- Main logic of the block  
    PERFORM risky\_data\_processing();

    IF (SELECT status FROM temp\_table) \= 'OK' THEN  
        COMMIT; \-- Fires the ON COMMIT trigger  
    ELSE  
        ROLLBACK; \-- Fires the ON ROLLBACK trigger  
    END IF;  
END;  
