## **In-Depth Analysis of Core PSQL Syntax**

Procedural SQL (PSQL) in ScratchBird provides the fundamental building blocks for creating stored procedures, functions, and triggers. This document details the core syntax for declaring variables, managing cursors, assigning values, and handling errors.

### **1\. Declarations (DECLARE)**

All local variables and cursors must be declared in a special section at the beginning of a PSQL module, before the first BEGIN.

#### **Variable Declaration**

* **Syntax**: DECLARE \[VARIABLE\] variable\_name \<data\_type\> \[= initial\_value | DEFAULT initial\_value\];  
* **variable\_name**: The name of the variable, typically prefixed with a colon (:) when used in executable statements (e.g., :my\_var).  
* **\<data\_type\>**: Any valid ScratchBird data type, a domain, TYPE OF COLUMN table.column, or TYPE OF DOMAIN domain\_name.  
* **initial\_value**: (Optional) An initial value to assign to the variable upon creation.

**Example:**

DECLARE VARIABLE my\_counter INTEGER \= 0;  
DECLARE VARIABLE employee\_name VARCHAR(100);  
DECLARE VARIABLE old\_salary TYPE OF COLUMN employees.salary;

#### **Cursor Declaration**

A cursor is a named pointer to the result set of a SELECT statement.

* **Syntax**: DECLARE cursor\_name CURSOR FOR (select\_statement);  
* **cursor\_name**: The name used to reference the cursor in OPEN, FETCH, and CLOSE statements.  
* **select\_statement**: The SELECT query whose result set the cursor will iterate over. If the cursor will be used for positioned updates or deletes, the query must include the FOR UPDATE clause.

**Example:**

DECLARE emp\_cursor CURSOR FOR (  
  SELECT first\_name, last\_name FROM employees WHERE department \= 'SALES' FOR UPDATE  
);

### **2\. Assignment Statement**

The assignment statement is used to assign a value to a declared variable.

* **Syntax**: variable\_name \= value\_expression;  
* The colon (:) on the variable name is optional in the assignment statement itself but required almost everywhere else.  
* **Example**: :my\_counter \= :my\_counter \+ 1;

### **3\. Cursor Operations**

Cursors are managed with a three-step process: OPEN, FETCH, and CLOSE.

#### **OPEN**

Opens a declared cursor, which executes its underlying SELECT query and prepares the result set for fetching.

* **Syntax**: OPEN cursor\_name;

#### **FETCH**

Retrieves the next row from an open cursor's result set and places the values into local variables.

* **Syntax**: FETCH cursor\_name INTO :var1, :var2, ...;  
* The FETCH statement is almost always used inside a loop.  
* The system variable ROW\_COUNT can be checked after a FETCH. It will be 1 if a row was successfully fetched and 0 if there were no more rows to fetch.

#### **CLOSE**

Closes an open cursor, releasing all resources associated with its result set.

* **Syntax**: CLOSE cursor\_name;  
* It is good practice to close cursors as soon as you are finished with them.

**Example Loop:**

OPEN emp\_cursor;  
WHILE (ROW\_COUNT \> 0\) DO  
BEGIN  
  FETCH emp\_cursor INTO :fname, :lname;  
  IF (ROW\_COUNT \> 0\) THEN  
    \-- Process the fetched row  
END  
CLOSE emp\_cursor;

### **4\. Error and Exception Handling (WHEN ... DO)**

This construct, placed at the end of a BEGIN...END block, allows you to catch specific errors or user-defined exceptions and execute handling code.

* **Syntax**:  
  BEGIN  
    \-- some work...  
  WHEN {GDSCODE error\_name | SQLCODE \-num | EXCEPTION ex\_name | ANY} \[OR ...\] DO  
  BEGIN  
    \-- error handling code...  
    \-- often includes a ROLLBACK;  
  END

* **GDSCODE**: Catches a specific ScratchBird error by its symbolic name (e.g., lock\_conflict).  
* **SQLCODE**: Catches a standard SQL error by its numeric code.  
* **EXCEPTION**: Catches a custom exception defined with CREATE EXCEPTION.  
* **ANY**: A catch-all handler for any error not previously caught.  
* You can list multiple conditions with OR. Execution falls through to the first matching WHEN block.
