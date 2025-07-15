## **In-Depth Analysis of Package DDL**

A package in ScratchBird is a schema object that groups together logically related procedures, functions, and variables. It consists of two parts: a **package header** (the specification) and an optional **package body** (the implementation). This separation allows you to provide a public interface while hiding the implementation details.

### **CREATE PACKAGE (Header)**

This statement defines the package header, which contains the public declarations of the procedures and functions within the package.

**Syntax:**

CREATE \[OR ALTER\] PACKAGE \[IF NOT EXISTS\] package\_name  
    \[SQL SECURITY {DEFINER | INVOKER}\]  
AS  
BEGIN  
    procedure\_declaration;  
    function\_declaration;  
    ...  
END

#### **Clause Breakdown:**

* **package\_name**: The name of the package.  
* **SQL SECURITY**: (Optional) Sets the default security context for all routines in the package. Can be overridden by individual routines.  
* **procedure\_declaration**: The declaration of a procedure header.  
  * **Syntax**: PROCEDURE procedure\_name \[(inputs)\] \[RETURNS (outputs)\];  
* **function\_declaration**: The declaration of a function header.  
  * **Syntax**: FUNCTION function\_name \[(inputs)\] RETURNS \<type\>;

### **CREATE PACKAGE BODY**

This statement defines the implementation for the routines declared in the package header.

**Syntax:**

CREATE \[OR ALTER\] PACKAGE BODY \[IF NOT EXISTS\] package\_name  
AS  
BEGIN  
    procedure\_implementation  
    function\_implementation  
    ...  
END

* The procedure\_implementation and function\_implementation must match the declarations in the package header and include the full AS BEGIN ... END body for each routine.  
* A package body can be created or dropped independently of the header, but it cannot exist without a corresponding header.

### **ALTER PACKAGE / ALTER PACKAGE BODY**

Modifying a package is typically done using CREATE OR ALTER PACKAGE or CREATE OR ALTER PACKAGE BODY, which requires a full redefinition. The grammar also allows for a limited ALTER PACKAGE statement to change only the security context.

**Syntax:**

ALTER PACKAGE package\_name \[SQL SECURITY {DEFINER | INVOKER | DROP}\];

### **DROP PACKAGE / DROP PACKAGE BODY**

These statements delete the package header and body.

**Syntax:**

DROP PACKAGE \[IF EXISTS\] package\_name;  
DROP PACKAGE BODY \[IF EXISTS\] package\_name;

* Dropping the package header (DROP PACKAGE) will automatically drop the package body as well.  
* You can drop the body (DROP PACKAGE BODY) without affecting the header. This is useful when you need to re-implement the package logic.

### **RECREATE PACKAGE / RECREATE PACKAGE BODY**

These commands provide atomic drop-and-recreate semantics, which is useful for deployment scripts.

**Syntax:**

RECREATE PACKAGE package\_name ...  
RECREATE PACKAGE BODY package\_name ...

The rest of the syntax is identical to their CREATE counterparts.