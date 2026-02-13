# **DDL Specification: Procedures**

## **Overview**

A procedure is a reusable subprogram that performs a specific action or set of actions. Unlike functions, procedures **do not return a value** directly and are invoked using the CALL statement. They are ideal for encapsulating complex business logic, data modification workflows, and administrative tasks. They can modify data using DML statements and control transactions.

## **CREATE PROCEDURE**

Defines a new stored procedure.

### **Syntax**

CREATE \[ OR REPLACE \] PROCEDURE \<procedure\_name\> (  
    \[ \[ \<parameter\_mode\> \] \<parameter\_name\> \<data\_type\> \[ DEFAULT \<value\> \] \[ , ... \] \]  
)  
    \[ LANGUAGE \<language\_name\> \]  
    \[ SECURITY { DEFINER | INVOKER } \]  
    \[ SET \<configuration\_parameter\> \= \<value\> \]  
AS $$  
    \-- Procedure body  
    DECLARE  
        \-- variable declarations  
    BEGIN  
        \-- statements  
    END;  
$$;

### **Parameters**

* \<parameter\_mode\>: Can be IN (default, read-only), OUT (write-only, for returning values), or INOUT (read-write).  
* LANGUAGE: The implementation language (e.g., plpgsql). Default is plpgsql.  
* SECURITY DEFINER | INVOKER: DEFINER (default) runs the procedure with the permissions of its owner. INVOKER runs with the permissions of the caller.

### **Examples**

**1\. Create a simple procedure to add a new employee:**

CREATE PROCEDURE add\_employee(  
    first\_name\_in VARCHAR(50),  
    last\_name\_in VARCHAR(50),  
    hire\_date\_in DATE  
)  
LANGUAGE plpgsql  
AS $$  
BEGIN  
    INSERT INTO employees (first\_name, last\_name, hire\_date)  
    VALUES (first\_name\_in, last\_name\_in, hire\_date\_in);  
END;  
$$;

**Usage:** CALL add\_employee('Jane', 'Doe', '2025-09-15');

**2\. Create a procedure with an OUT parameter:**

CREATE PROCEDURE get\_employee\_count(OUT total\_employees INT)  
AS $$  
BEGIN  
    SELECT COUNT(\*) INTO total\_employees FROM employees;  
END;  
$$;

**Usage:**

DECLARE @count INT;  
CALL get\_employee\_count(@count);  
\-- @count now holds the number of employees.

**3\. Create a procedure that manages a transaction:**

CREATE PROCEDURE transfer\_funds(  
    from\_acct INT,  
    to\_acct INT,  
    amount DECIMAL(12, 2\)  
)  
AS $$  
BEGIN  
    UPDATE accounts SET balance \= balance \- amount WHERE account\_id \= from\_acct;  
    UPDATE accounts SET balance \= balance \+ amount WHERE account\_id \= to\_acct;  
    COMMIT;  
EXCEPTION  
    WHEN OTHERS THEN  
        ROLLBACK;  
        RAISE; \-- Re-raise the original error  
END;  
$$;

## System Procedures (Built-in)

ScratchBird provides built-in procedures under the `sys` schema for pass-through
execution via UDR connectors. These procedures are installed by the engine and
cannot be redefined.

### sys.remote_exec

CREATE PROCEDURE sys.remote_exec(  
    server_name TEXT,  
    sql_text TEXT,  
    params_json JSON DEFAULT NULL,  
    options_json JSON DEFAULT NULL  
)  
LANGUAGE internal  
SECURITY INVOKER;

### sys.remote_call

CREATE PROCEDURE sys.remote_call(  
    server_name TEXT,  
    routine_name TEXT,  
    params_json JSON DEFAULT NULL,  
    options_json JSON DEFAULT NULL  
)  
LANGUAGE internal  
SECURITY INVOKER;

**Options (options_json):**
- timeout_ms
- transaction_mode (auto, join, new)
- read_only (true/false)
- fetch_size

**Examples:**

CALL sys.remote_exec('legacy_pg', 'CREATE TABLE tmp(id int)');
CALL sys.remote_call('legacy_pg', 'rebuild_indexes', '{"schema":"public"}');

## **ALTER PROCEDURE**

Modifies the properties of an existing procedure. The signature (name and parameter types) cannot be changed without using CREATE OR REPLACE.

### **Syntax**

ALTER PROCEDURE \<procedure\_name\> ( \[ \<parameter\_types\> \] )  
    { RENAME TO \<new\_procedure\_name\>  
    | OWNER TO \<new\_owner\>  
    | SET SCHEMA \<new\_schema\>  
    | SECURITY { DEFINER | INVOKER }  
    | SET \<configuration\_parameter\> \= \<value\> };

### **Examples**

**1\. Change the owner of a procedure:**

ALTER PROCEDURE admin\_task() OWNER TO 'db\_super\_admin';

**2\. Change the security context to run with the caller's permissions:**

ALTER PROCEDURE transfer\_funds(INT, INT, DECIMAL) SECURITY INVOKER;

## **DROP PROCEDURE**

Removes a procedure from the database.

### **Syntax**

DROP PROCEDURE \[ IF EXISTS \] \<procedure\_name\> ( \[ \<parameter\_types\> \] ) \[ CASCADE | RESTRICT \];

### **Parameters**

* You must specify the parameter types to uniquely identify the procedure, as procedures can be overloaded.

### **Examples**

**1\. Drop a procedure with a specific signature:**

DROP PROCEDURE add\_employee(VARCHAR, VARCHAR, DATE);

**2\. Safely drop a procedure that might not exist:**

DROP PROCEDURE IF EXISTS old\_maintenance\_task();  
