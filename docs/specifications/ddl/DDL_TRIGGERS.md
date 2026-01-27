# **DDL Specification: Triggers**

## **Overview**

A trigger is a special type of stored procedure that automatically executes when a specific event occurs in the database. Triggers can be attached to tables to respond to Data Manipulation Language (DML) events (INSERT, UPDATE, DELETE) or to the database itself to respond to Data Definition Language (DDL) or session events (CREATE TABLE, ON CONNECT).

Triggers may declare and use cursor handles inside the trigger body, but they
cannot accept cursor parameters or return cursor handles. See:
`ScratchBird/docs/specifications/parser/PSQL_CURSOR_HANDLES.md`

## **CREATE TRIGGER**

Defines a new trigger.

### **Syntax**

**Table-Level (DML) Trigger:**

CREATE \[ OR REPLACE \] TRIGGER \<trigger\_name\>  
    { BEFORE | AFTER | INSTEAD OF }  
    { INSERT | DELETE | UPDATE \[ OF \<column\_list\> \] }  
    \[ OR ... \]  
    ON \<table\_name\>  
    \[ REFERENCING { OLD \[AS\] \<old\_alias\> | NEW \[AS\] \<new\_alias\> } ... \]  
    \[ FOR EACH { ROW | STATEMENT } \]  
    \[ WHEN ( \<condition\> ) \]  
    \[ POSITION \<integer\> \]  
    EXECUTE { PROCEDURE | FUNCTION } \<function\_name\>( \[ \<arguments\> \] );

**Database-Level (DDL/Session) Trigger:**

CREATE \[ OR REPLACE \] TRIGGER \<trigger\_name\>  
    { ON CONNECT | ON DISCONNECT | ON TRANSACTION { START | COMMIT | ROLLBACK } }  
    | AFTER { CREATE | ALTER | DROP }  
    \[ WHEN ( \<condition\> ) \]  
    \[ POSITION \<integer\> \]  
    EXECUTE { PROCEDURE | FUNCTION } \<function\_name\>( \[ \<arguments\> \] );

### **Parameters**

* BEFORE | AFTER: Specifies whether the trigger function runs before or after the event.  
* INSTEAD OF: For views, specifies that the trigger should run *instead of* the DML operation.  
* FOR EACH ROW: The trigger fires once for each row affected.  
* FOR EACH STATEMENT: (Default) The trigger fires once per DML statement.  
* WHEN (condition): A boolean condition that must be true for the trigger to fire.  
* REFERENCING: Provides access to the OLD (pre-change) and NEW (post-change) row values within a row-level trigger.

### **Examples**

**1\. Create a "last modified" trigger:**

\-- First, create the trigger function  
CREATE FUNCTION update\_modified\_column() RETURNS TRIGGER AS $$  
BEGIN  
    NEW.last\_modified \= CURRENT\_TIMESTAMP;  
    RETURN NEW;  
END;  
$$ LANGUAGE plpgsql;

\-- Then, create the trigger  
CREATE TRIGGER set\_last\_modified  
    BEFORE UPDATE ON products  
    FOR EACH ROW  
    EXECUTE FUNCTION update\_modified\_column();

**2\. Create an audit trigger:**

CREATE TRIGGER audit\_salary\_changes  
    AFTER UPDATE OF salary ON employees  
    FOR EACH ROW  
    WHEN (OLD.salary IS DISTINCT FROM NEW.salary)  
    EXECUTE PROCEDURE log\_salary\_change(OLD.id, OLD.salary, NEW.salary);

**3\. Create a database connect trigger to set session variables:**

CREATE TRIGGER set\_session\_defaults  
    ON CONNECT  
    EXECUTE PROCEDURE initialize\_session();

## **ALTER TRIGGER**

Modifies an existing trigger.

### **Syntax**

ALTER TRIGGER \<trigger\_name\> ON \<table\_name\>  
    { RENAME TO \<new\_trigger\_name\>  
    | ENABLE  
    | DISABLE };

### **Examples**

**1\. Disable a trigger during a large data import:**

ALTER TRIGGER audit\_salary\_changes ON employees DISABLE;

**2\. Rename a trigger for clarity:**

ALTER TRIGGER set\_timestamp ON products RENAME TO set\_last\_modified;

**3\. Re-enable a trigger:**

ALTER TRIGGER audit\_salary\_changes ON employees ENABLE;

## **DROP TRIGGER**

Removes a trigger from the database.

### **Syntax**

DROP TRIGGER \[ IF EXISTS \] \<trigger\_name\> ON \<table\_name\> \[ CASCADE | RESTRICT \];

### **Examples**

**1\. Drop a trigger from a table:**

DROP TRIGGER set\_last\_modified ON products;

**2\. Safely drop a database-level trigger:**

DROP TRIGGER IF EXISTS set\_session\_defaults ON DATABASE;  
