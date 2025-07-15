## **In-Depth Analysis of Function DDL**

User-Defined Functions (UDFs) in ScratchBird are routines that accept zero or more input parameters and return a single scalar value. They can be used in nearly any place where a value expression is allowed. ScratchBird supports functions written in PSQL, external functions using the modern UDR engine, and legacy external functions.

### **CREATE FUNCTION**

This statement defines a new function.

**Syntax (PSQL and UDR):**

CREATE \[OR ALTER\] FUNCTION \[IF NOT EXISTS\] function\_name  
    \[(input\_parameter\_list)\]  
    RETURNS \<return\_type\>  
    \[DETERMINISTIC\]  
    \[SQL SECURITY {DEFINER | INVOKER}\]  
    AS  
      psql\_body

or for an external UDR function:

CREATE \[OR ALTER\] FUNCTION \[IF NOT EXISTS\] function\_name ...  
    EXTERNAL NAME 'external\_name' ENGINE 'engine\_name' \[AS 'body'\];

#### **Clause Breakdown:**

* **function\_name**: The name of the function.  
* **input\_parameter\_list**: (Optional) A comma-separated list of input parameters: param\_name \<type\> \[= {DEFAULT \<value\> | \<value\>}\].  
* **RETURNS \<return\_type\>**: Mandatory clause specifying the data type of the single value the function will return.  
* **DETERMINISTIC**: (Optional) A hint to the optimizer that the function will always return the same result for the same input arguments. This can enable certain optimizations, like caching results. NOT DETERMINISTIC is the default.  
* **SQL SECURITY**: Same as for procedures (DEFINER or INVOKER).  
* **AS psql\_body**: The PSQL implementation, which must contain a RETURN \<value\>; statement.  
* **EXTERNAL NAME ...**: Defines an external function via the UDR engine, same as for procedures.

### **DECLARE EXTERNAL FUNCTION (Legacy)**

This is the older syntax for declaring a function from an external library (e.g., a .dll or .so file). It is maintained for backward compatibility.

**Syntax:**

DECLARE EXTERNAL FUNCTION function\_name  
    \[\<parameter\_type\> \[BY {DESCRIPTOR | VALUE | SCALAR\_ARRAY}\] \[NULL\], ...\]  
    RETURNS \<return\_type\> \[BY {VALUE | DESCRIPTOR}\] \[FREE\_IT\]  
    ENTRY\_POINT 'entry\_point\_name'  
    MODULE\_NAME 'module\_name';

This syntax involves low-level details about how parameters are passed (by reference, by value, by descriptor) and is more complex than the modern UDR approach.

### **ALTER FUNCTION**

Modifies an existing function. The syntax is identical to CREATE FUNCTION, allowing a complete redefinition. It also allows for altering specific properties without a full redefinition.

**Syntax:**

ALTER FUNCTION function\_name  
    \[DETERMINISTIC | NOT DETERMINISTIC\]  
    \[SQL SECURITY {DEFINER | INVOKER | DROP}\]  
    ...

* You can change the deterministic property or security context independently of the function body.

### **DROP FUNCTION**

Permanently deletes a function from the database.

**Syntax:**

DROP {FUNCTION | EXTERNAL FUNCTION} \[IF EXISTS\] function\_name;

* You must use DROP EXTERNAL FUNCTION for legacy UDFs.  
* **IF EXISTS**: Prevents an error if the function does not exist.

### **RECREATE FUNCTION / CREATE OR ALTER FUNCTION**

These work identically to their procedure counterparts, providing atomic drop-and-recreate or create-or-modify semantics.

**Syntax:**

RECREATE FUNCTION function\_name ...  
CREATE OR ALTER FUNCTION function\_name ...

The rest of the syntax is identical to CREATE FUNCTION.