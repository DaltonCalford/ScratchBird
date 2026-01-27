# **DDL Specification: Functions**

## **Overview**

A function is a reusable subprogram that performs a specific calculation or operation and **must return a value**. Functions are essential for encapsulating business logic, simplifying complex queries, and promoting code reuse. They can be called from SQL statements (SELECT, WHERE, etc.) or from within other procedural code.

Cursor handle returns are supported for PSQL invocation contexts only. For
details on cursor handle semantics and limits, see:
`ScratchBird/docs/specifications/parser/PSQL_CURSOR_HANDLES.md`

## **CREATE FUNCTION**

Defines a new function.

### **Syntax**

CREATE \[ OR REPLACE \] FUNCTION \<function\_name\> (  
    \[ \[ \<parameter\_mode\> \] \<parameter\_name\> \<data\_type\> \[ DEFAULT \<value\> \] \[ , ... \] \]  
)  
RETURNS \<return\_type\>  
    \[ LANGUAGE \<language\_name\> \]  
    \[ { IMMUTABLE | STABLE | VOLATILE } \]  
    \[ SECURITY { DEFINER | INVOKER } \]  
    \[ PARALLEL { SAFE | RESTRICTED | UNSAFE } \]  
    \[ COST \<cost\> \]  
    \[ ROWS \<rows\> \]  
AS $$  
    \-- Function body  
    DECLARE  
        \-- variable declarations  
    BEGIN  
        \-- statements  
        RETURN \<value\>;  
    END;  
$$;

### **Parameters**

* RETURNS \<return\_type\>: Specifies the data type of the value the function returns. Can be a scalar type, SETOF \<type\>, or TABLE(...).  
* LANGUAGE: The implementation language (e.g., plpgsql, sql, python). Default is plpgsql.  
* IMMUTABLE | STABLE | VOLATILE: Hints for the query planner. IMMUTABLE always returns the same result for the same inputs. STABLE results are constant within a single scan. VOLATILE (default) results can change at any time.  
* SECURITY DEFINER | INVOKER: DEFINER (default) runs the function with the permissions of the user who created it. INVOKER runs it with the permissions of the calling user.
* RETURNS CURSOR: Allowed only for PSQL invocation (EXECUTE/CALL). It is not
  valid inside SQL expressions; use RETURNS SETOF/TABLE for SQL-facing
  set-returning functions.

### **Examples**

**1\. Create a simple, immutable calculation function:**

CREATE FUNCTION calculate\_tax(amount DECIMAL(10, 2))  
RETURNS DECIMAL(10, 2\)  
LANGUAGE sql IMMUTABLE  
AS $$    SELECT amount \* 0.13;$$;

**2\. Create a function that reads from a table (STABLE):**

CREATE FUNCTION get\_employee\_name(employee\_id INT)  
RETURNS VARCHAR(100)  
LANGUAGE plpgsql STABLE  
AS $$  
DECLARE  
    emp\_name VARCHAR(100);  
BEGIN  
    SELECT first\_name || ' ' || last\_name  
    INTO emp\_name  
    FROM employees  
    WHERE id \= employee\_id;

    RETURN emp\_name;  
END;  
$$;

**3\. Create a function that returns a set of rows (SETOF):**

CREATE FUNCTION get\_orders\_by\_customer(customer\_id\_in UUID)  
RETURNS SETOF orders \-- Returns rows matching the 'orders' table structure  
AS $$  
BEGIN  
    RETURN QUERY SELECT \* FROM orders WHERE customer\_id \= customer\_id\_in;  
END;  
$$ LANGUAGE plpgsql;

**4\. Create a function that returns a custom table structure:**

CREATE FUNCTION get\_sales\_summary(start\_date DATE, end\_date DATE)  
RETURNS TABLE(product\_name VARCHAR, total\_sold DECIMAL)  
AS $$  
BEGIN  
    RETURN QUERY  
        SELECT p.name, SUM(ol.quantity \* ol.unit\_price)  
        FROM products p  
        JOIN order\_lines ol ON p.id \= ol.product\_id  
        JOIN orders o ON ol.order\_id \= o.id  
        WHERE o.order\_date BETWEEN start\_date AND end\_date  
        GROUP BY p.name;  
END;  
$$ LANGUAGE plpgsql;

## System Functions (Built-in)

ScratchBird provides built-in functions under the `sys` schema for pass-through
query execution via UDR connectors. These functions are installed by the engine
and cannot be redefined.

### sys.remote_query

CREATE FUNCTION sys.remote_query(  
    server_name TEXT,  
    sql_text TEXT,  
    params_json JSON DEFAULT NULL,  
    options_json JSON DEFAULT NULL  
)  
RETURNS SETOF RECORD  
LANGUAGE internal  
SECURITY INVOKER  
VOLATILE;

**Options (options_json):**
- timeout_ms
- transaction_mode (auto, join, new)
- read_only (true/false)
- fetch_size

**Examples:**

SELECT * FROM sys.remote_query('legacy_pg', 'SELECT id, name FROM users');

SELECT * FROM sys.remote_query('legacy_pg', 'SELECT id, name FROM users')
AS t(id INT, name TEXT);

## **ALTER FUNCTION**

Modifies the properties of an existing function. Note that the function signature (name and parameter types) cannot be changed; you must CREATE OR REPLACE to do that.

### **Syntax**

ALTER FUNCTION \<function\_name\> ( \[ \<parameter\_types\> \] )  
    { RENAME TO \<new\_function\_name\>  
    | OWNER TO \<new\_owner\>  
    | SET SCHEMA \<new\_schema\>  
    | { IMMUTABLE | STABLE | VOLATILE }  
    | SECURITY { DEFINER | INVOKER }  
    | SET \<configuration\_parameter\> { TO | \= } \<value\> };

### **Examples**

**1\. Change the volatility of a function:**

\-- If the function was mistakenly marked IMMUTABLE but reads from a table.  
ALTER FUNCTION calculate\_tax(DECIMAL) STABLE;

**2\. Change the security context:**

ALTER FUNCTION admin\_report() SECURITY INVOKER;

## **DROP FUNCTION**

Removes a function from the database.

### **Syntax**

DROP FUNCTION \[ IF EXISTS \] \<function\_name\> ( \[ \<parameter\_types\> \] ) \[ CASCADE | RESTRICT \];

### **Parameters**

* You must specify the parameter types to uniquely identify the function, as functions can be overloaded (same name, different parameters).

### **Examples**

**1\. Drop a function with no parameters:**

DROP FUNCTION get\_server\_version;

**2\. Drop a function with specific parameter types:**

DROP FUNCTION IF EXISTS get\_employee\_name(INT);

**3\. Drop a function and dependent objects:**

\-- If a view uses this function, the view will also be dropped.  
DROP FUNCTION calculate\_tax(DECIMAL) CASCADE;  
