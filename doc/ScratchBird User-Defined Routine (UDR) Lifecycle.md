## **In-Depth Analysis of the UDR Lifecycle**

User-Defined Routines (UDRs) are the modern framework in ScratchBird for creating functions, procedures, and triggers in external programming languages like C++, Java, or Delphi. UDRs communicate with the database engine through a stable API and are managed via plugins. This provides a much safer, more powerful, and more flexible alternative to the legacy UDF mechanism.

The SQL syntax for managing UDRs is integrated directly into the standard CREATE/ALTER/DROP statements for functions and procedures. The key differentiator is the EXTERNAL clause.

### **1\. CREATE FUNCTION / CREATE PROCEDURE (for UDRs)**

This statement declares a new function or procedure and binds it to an implementation in an external plugin.

**Syntax:**

CREATE \[OR ALTER\] {FUNCTION | PROCEDURE} routine\_name  
    \[(parameter\_list)\]  
    \[RETURNS \<return\_type\_or\_list\>\]  
    EXTERNAL NAME 'external\_routine\_name'  
    ENGINE 'engine\_plugin\_name'  
    \[AS 'optional\_body'\];

#### **Clause Breakdown:**

* **CREATE {FUNCTION | PROCEDURE} ...**: The standard declaration for the routine's SQL signature, including its name, input parameters, and return type(s).  
* **EXTERNAL NAME 'external\_routine\_name'**: This is the crucial part that defines the routine as a UDR.  
  * 'external\_routine\_name': A string that identifies the specific function or class within the external plugin. The format is defined by the plugin itself (e.g., 'MyPlugin.MyClass.myFunction').  
* **ENGINE 'engine\_plugin\_name'**: Specifies which UDR engine plugin is responsible for executing this routine. ScratchBird comes with a default 'UDR' engine, but others can be developed.  
* **AS 'optional\_body'**: (Optional) An arbitrary string literal that is passed to the plugin during the routine's initialization. This can be used to pass metadata, configuration, or even a script (if the plugin is a script engine) to the external code.

**Example:**

CREATE FUNCTION MY\_UDR\_ADD (  
    A INTEGER,  
    B INTEGER  
) RETURNS INTEGER  
EXTERNAL NAME 'MyMathLib\!add\_integers'  
ENGINE UDR;

This creates a SQL function MY\_UDR\_ADD that, when called, will be executed by the UDR engine, which in turn will call the add\_integers function inside the MyMathLib module.

### **2\. ALTER FUNCTION / ALTER PROCEDURE (for UDRs)**

To modify a UDR, you must use ALTER with the full EXTERNAL NAME clause, effectively redefining the external binding. You cannot change a PSQL routine into an external one or vice-versa with ALTER; you must DROP and CREATE again.

**Syntax:**

ALTER {FUNCTION | PROCEDURE} routine\_name  
    \[(new\_parameter\_list)\]  
    \[RETURNS \<new\_return\_type\>\]  
    EXTERNAL NAME 'new\_external\_name'  
    ENGINE 'new\_engine\_name'  
    \[AS 'new\_body'\];

This allows you to point the existing SQL routine name to a different external implementation or change its SQL signature.

### **3\. DROP FUNCTION / DROP PROCEDURE (for UDRs)**

Dropping a UDR is identical to dropping a standard PSQL routine. The engine knows from its metadata that the routine is external and handles the unbinding from the plugin.

**Syntax:**

DROP {FUNCTION | PROCEDURE} \[IF EXISTS\] routine\_name;

### **4\. RECREATE and CREATE OR ALTER**

These commands work for UDRs just as they do for PSQL routines, providing convenient atomic drop-and-recreate or create-or-modify semantics. This is the most common way to manage UDRs during development and deployment.

**Syntax:**

RECREATE {FUNCTION | PROCEDURE} routine\_name ... EXTERNAL NAME ...;  
CREATE OR ALTER {FUNCTION | PROCEDURE} routine\_name ... EXTERNAL NAME ...;

The rest of the syntax is identical to their CREATE counterparts.