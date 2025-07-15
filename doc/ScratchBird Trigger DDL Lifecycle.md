## **In-Depth Analysis of Trigger DDL**

A trigger is a specialized stored procedure that automatically executes in response to a specific event. ScratchBird supports DML triggers (for INSERT, UPDATE, DELETE), database-level event triggers, and DDL triggers.

### **CREATE TRIGGER**

This statement defines a new trigger.

**Syntax:**

CREATE \[OR ALTER\] TRIGGER \[IF NOT EXISTS\] trigger\_name  
    \[ACTIVE | INACTIVE\]  
    {FOR {table\_name | view\_name} | ON {DATABASE | DDL}}  
    trigger\_event  
    \[POSITION number\]  
    \[SQL SECURITY {DEFINER | INVOKER}\]  
    AS  
      psql\_body

#### **Clause Breakdown:**

* **trigger\_name**: The name of the trigger.  
* **ACTIVE | INACTIVE**: (Optional) ACTIVE (the default) means the trigger will fire. INACTIVE disables it.  
* **FOR table\_name | ON ...**: Specifies the target of the trigger.  
  * FOR table\_name: A DML trigger on a specific table or view.  
  * ON DATABASE: A database-level event trigger.  
  * ON DDL: A DDL trigger that fires on metadata changes.  
* **trigger\_event**: Defines when the trigger fires. This is the most complex part.  
  * **For DML Triggers (FOR table\_name)**:  
    * {BEFORE | AFTER} {INSERT | UPDATE | DELETE | INSERT OR UPDATE OR DELETE}  
    * BEFORE: Fires before the DML operation. Can be used to validate or change data (NEW.column\_name).  
    * AFTER: Fires after the DML operation. Can be used for auditing or cascading effects.  
  * **For Database Triggers (ON DATABASE)**:  
    * {CONNECT | DISCONNECT | TRANSACTION {START | COMMIT | ROLLBACK}}  
  * **For DDL Triggers (ON DDL)**:  
    * {BEFORE | AFTER} {ANY DDL STATEMENT | CREATE TABLE | ALTER TABLE | ...}  
    * Can be a specific DDL command (CREATE VIEW) or a list (CREATE TABLE OR ALTER TABLE).  
* **POSITION number**: (Optional) For DML triggers, this specifies the order of execution when multiple triggers exist for the same event. Lower numbers fire first.  
* **SQL SECURITY**: Same as for procedures (DEFINER or INVOKER).  
* **AS psql\_body**: The PSQL code to execute. In DML triggers, you can access OLD.column and NEW.column context variables, and INSERTING, UPDATING, DELETING boolean context variables.

### **ALTER TRIGGER**

Modifies an existing trigger. This can be a full redefinition or a change to specific properties.

**Syntax:**

ALTER TRIGGER trigger\_name  
    \[ACTIVE | INACTIVE\]  
    \[POSITION new\_position\]  
    \[AS new\_psql\_body\]  
    ...

* You can change the active status or position without redefining the entire trigger.  
* To change the event or target table, you must redefine the trigger using ALTER TRIGGER with the full CREATE TRIGGER syntax.

### **DROP TRIGGER**

Permanently deletes a trigger from the database.

**Syntax:**

DROP TRIGGER \[IF EXISTS\] trigger\_name;

### **RECREATE TRIGGER / CREATE OR ALTER TRIGGER**

These provide atomic drop-and-recreate or create-or-modify semantics, useful for deployment scripts.

**Syntax:**

RECREATE TRIGGER trigger\_name ...  
CREATE OR ALTER TRIGGER trigger\_name ...

The rest of the syntax is identical to CREATE TRIGGER.