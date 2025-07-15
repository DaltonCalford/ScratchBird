## **In-Depth Analysis of Stored Procedure DDL**

Stored Procedures are reusable blocks of procedural SQL (PSQL) stored in the database. They can accept input parameters and return output parameters, making them a cornerstone of business logic implementation in ScratchBird. This document details the full lifecycle of creating and managing stored procedures.

### **CREATE PROCEDURE**

This statement defines a new stored procedure. A procedure can be implemented in PSQL or can be a wrapper around an external routine.

**Syntax:**

CREATE \[OR ALTER\] PROCEDURE \[IF NOT EXISTS\] procedure\_name  
    \[(input\_parameter\_list)\]  
    \[RETURNS (output\_parameter\_list)\]  
    \[SQL SECURITY {DEFINER | INVOKER}\]  
    AS  
      psql\_body

or for an external procedure:

CREATE \[OR ALTER\] PROCEDURE \[IF NOT EXISTS\] procedure\_name ...  
    EXTERNAL NAME 'external\_name' ENGINE 'engine\_name' \[AS 'body'\];

#### **Clause Breakdown:**

* **CREATE OR ALTER**: If the procedure exists, it is modified; otherwise, it is created.  
* **IF NOT EXISTS**: Prevents an error if the procedure already exists; the statement does nothing.  
* **procedure\_name**: The name of the procedure.  
* **input\_parameter\_list**: (Optional) A comma-separated list of input parameters.  
  * **Syntax**: param\_name \<type\> \[= {DEFAULT \<value\> | \<value\>}\]  
  * \<type\> can be any valid data type or a domain.  
* **RETURNS (output\_parameter\_list)**: (Optional) A comma-separated list of output parameters.  
  * **Syntax**: param\_name \<type\>, ...  
* **SQL SECURITY {DEFINER | INVOKER}**: (Optional) Specifies the security context.  
  * DEFINER (default): The procedure executes with the privileges of the user who defined it.  
  * INVOKER: The procedure executes with the privileges of the calling user.  
* **AS psql\_body**: The main body of the procedure.  
  * **psql\_body**: \[DECLARE ...\] BEGIN ... END  
* **EXTERNAL NAME ...**: Defines an external procedure.  
  * 'external\_name': The name of the function in the external module.  
  * 'engine\_name': The name of the UDR engine plugin (e.g., UDR).  
  * AS 'body': (Optional) An arbitrary string that can be passed to the external engine.

### **ALTER PROCEDURE**

Modifies an existing stored procedure. The syntax is largely identical to CREATE PROCEDURE, allowing for a complete redefinition of the procedure's parameters, security context, and body.

**Syntax:**

ALTER PROCEDURE procedure\_name  
    \[(new\_input\_list)\]  
    \[RETURNS (new\_output\_list)\]  
    \[SQL SECURITY {DEFINER | INVOKER | DROP}\]  
    \[AS new\_psql\_body\]

* You can change any part of the procedure. If you only specify a subset of clauses (e.g., only SQL SECURITY), only those properties are altered.  
* DROP SQL SECURITY removes an explicit security context setting, reverting to the schema or database default.

### **DROP PROCEDURE**

Permanently deletes a stored procedure from the database.

**Syntax:**

DROP PROCEDURE \[IF EXISTS\] procedure\_name;

* **IF EXISTS**: Prevents an error if the procedure does not exist.

### **RECREATE PROCEDURE**

Atomically drops and recreates a procedure. This is useful in deployment scripts to ensure a procedure is in a known state without worrying about whether it existed before. If the procedure doesn't exist, it is simply created.

**Syntax:**

RECREATE PROCEDURE procedure\_name ...

The rest of the syntax is identical to CREATE PROCEDURE.