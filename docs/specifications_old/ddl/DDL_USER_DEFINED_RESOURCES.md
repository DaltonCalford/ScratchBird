# **DDL Specification: User-Defined Resources**

## **Overview**

ScratchBird supports extending its functionality through user-defined resources, primarily in the form of external functions written in other programming languages like C, C++, Python, or Java. This allows developers to perform complex computations or integrate with external systems directly within SQL.

The process involves two main DDL steps:

1. **CREATE LIBRARY**: Registers a shared object file (.so, .dll) with the database.  
2. **CREATE FUNCTION**: Binds a SQL function signature to a specific function within that registered library.

## **CREATE LIBRARY**

Registers an external, compiled shared library file with the database system.

### **Syntax**

CREATE \[ OR REPLACE \] LIBRARY \<library\_name\>  
    AS \<path\_to\_library\_file\>;

### **Parameters**

* \<library\_name\>: A logical name for the library within the database.  
* \<path\_to\_library\_file\>: The absolute path to the compiled shared library file on the database server's filesystem. The path must be accessible by the database server process.

### **Examples**

**1\. Register a C library for custom math functions:**

CREATE LIBRARY advanced\_math\_lib  
    AS '/usr/local/db\_extensions/libadvanced\_math.so';

**2\. Register a Python library (via a C wrapper extension):**

CREATE LIBRARY python\_udf\_handler  
    AS '/opt/scratchbird/lib/plpython3.so';

## **CREATE FUNCTION (External)**

This is a specific form of CREATE FUNCTION that binds a SQL function to an external resource.

### **Syntax**

CREATE \[ OR REPLACE \] FUNCTION \<function\_name\> (  
    \[ \<parameter\_name\> \<sql\_data\_type\> \[ , ... \] \]  
)  
RETURNS \<sql\_return\_type\>  
AS EXTERNAL NAME '\<library\_name\>:\<function\_symbol\_in\_library\>'  
LANGUAGE \<language\_name\>;

### **Parameters**

* AS EXTERNAL NAME: Specifies the binding to the external code.  
* \<library\_name\>: The name of the library registered via CREATE LIBRARY.  
* \<function\_symbol\_in\_library\>: The exact, case-sensitive name of the function symbol as it exists in the compiled library.  
* LANGUAGE: The language of the external function, which informs the database how to pass arguments and handle return values (e.g., C, CPP, PYTHON).

### **Examples**

**1\. Bind a C function for calculating distance:**

\-- Assumes a C function: double haversine\_distance(double lat1, double lon1, double lat2, double lon2)  
\-- exists in the 'gis\_utils\_lib' library.

CREATE LIBRARY gis\_utils\_lib AS '/var/db\_libs/libgis.so';

CREATE FUNCTION haversine\_distance (  
    lat1 DOUBLE PRECISION,  
    lon1 DOUBLE PRECISION,  
    lat2 DOUBLE PRECISION,  
    lon2 DOUBLE PRECISION  
)  
RETURNS DOUBLE PRECISION  
AS EXTERNAL NAME 'gis\_utils\_lib:haversine\_distance'  
LANGUAGE C;

**2\. Bind a Python function to validate an email address:**

\-- Assumes a Python function \`validate\_email(email\_string)\` exists and is exposed  
\-- through the 'python\_udf\_handler' library.

CREATE FUNCTION py\_validate\_email (email TEXT)  
RETURNS BOOLEAN  
AS EXTERNAL NAME 'python\_udf\_handler:validate\_email'  
LANGUAGE PYTHON;

## **ALTER LIBRARY / ALTER FUNCTION**

The definition of an external resource is generally immutable. To change the path of a library or the binding of an external function, you must use CREATE OR REPLACE or DROP and CREATE the object again.

## **DROP LIBRARY**

Removes the registration of a shared library from the database.

### **Syntax**

DROP LIBRARY \[ IF EXISTS \] \<library\_name\> \[ CASCADE | RESTRICT \];

### **Parameters**

* IF EXISTS: Prevents an error if the library does not exist.  
* CASCADE: Automatically drops all functions that depend on this library.  
* RESTRICT: (Default) Prevents dropping the library if any functions reference it.

### **Examples**

**1\. Drop an unused library:**

DROP LIBRARY old\_math\_lib;

**2\. Drop a library and all its associated functions:**

\-- WARNING: This will also remove any SQL functions bound to this library.  
DROP LIBRARY IF EXISTS gis\_utils\_lib CASCADE;

*(Note: The DROP FUNCTION statement for external functions is the same as for regular functions.)*