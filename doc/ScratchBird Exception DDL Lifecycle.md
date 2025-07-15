## **In-Depth Analysis of Exception DDL**

In ScratchBird's Procedural SQL (PSQL), an exception is a named error condition that can be raised to interrupt the normal flow of execution and signal that an error has occurred. Declaring custom exceptions makes error handling more robust and meaningful.

### **CREATE EXCEPTION**

This statement defines a new, user-defined exception with a default message.

**Syntax:**

CREATE \[OR ALTER\] EXCEPTION \[IF NOT EXISTS\] exception\_name 'message\_text';

#### **Clause Breakdown:**

* **exception\_name**: The name for the new exception. This name will be used in PSQL to raise the exception (EXCEPTION my\_exception;) or to catch it in a WHEN block.  
* **'message\_text'**: A string literal that serves as the default error message when this exception is raised. The message can have a maximum length of 1021 bytes.

**Usage in PSQL:**

\-- Raising the exception  
IF (some\_condition) THEN  
  EXCEPTION E\_INSUFFICIENT\_STOCK;

\-- Catching the exception  
BEGIN  
  ...  
WHEN GDSCODE E\_INSUFFICIENT\_STOCK DO  
  \-- Handle the error  
END

### **ALTER EXCEPTION**

Modifies the message text of an existing exception.

**Syntax:**

ALTER EXCEPTION exception\_name 'new\_message\_text';

This statement only allows you to change the message associated with the exception.

### **DROP EXCEPTION**

Permanently deletes a custom exception from the database. An exception cannot be dropped if it is currently referenced by any stored procedure, trigger, or other PSQL module.

**Syntax:**

DROP EXCEPTION \[IF EXISTS\] exception\_name;

* **IF EXISTS**: Prevents an error from being thrown if the exception does not exist.

### **RECREATE EXCEPTION / CREATE OR ALTER EXCEPTION**

These commands provide convenient ways to manage exceptions in development and deployment scripts.

* **RECREATE EXCEPTION**: Atomically drops the exception if it exists and then creates it with the new definition.  
* **CREATE OR ALTER EXCEPTION**: Modifies the exception if it exists, or creates it if it does not. This is generally preferred as it preserves dependencies.

**Syntax:**

RECREATE EXCEPTION exception\_name 'message\_text';  
CREATE OR ALTER EXCEPTION exception\_name 'message\_text';  
