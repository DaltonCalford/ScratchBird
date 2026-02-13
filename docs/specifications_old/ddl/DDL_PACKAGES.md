# **DDL Specification: Packages**

## **Overview**

A package is a schema object that groups logically related PL/SQL types, variables, constants, subprograms (procedures and functions), cursors, and exceptions. Packages are compiled and stored in the database as a single unit. They are a powerful tool for organizing code, managing scope, and improving performance.

A package has two parts: a **specification** (the public interface) and a **body** (the implementation details).

## **CREATE PACKAGE (Specification)**

Defines the public interface of the package. This is what other programs and routines can see and use.

### **Syntax**

CREATE \[ OR REPLACE \] PACKAGE \<package\_name\> AS  
    \-- Public type definitions  
    TYPE \<type\_name\> IS ... ;

    \-- Public variable and constant declarations  
    \<variable\_name\> \<data\_type\>;  
    \<constant\_name\> CONSTANT \<data\_type\> := \<value\>;

    \-- Public exception declarations  
    \<exception\_name\> EXCEPTION;

    \-- Public cursor specifications  
    CURSOR \<cursor\_name\> \[ ( \<parameter\_list\> ) \] RETURN \<row\_type\>;

    \-- Public subprogram (procedure and function) specifications  
    PROCEDURE \<procedure\_name\> \[ ( \<parameter\_list\> ) \];  
    FUNCTION \<function\_name\> \[ ( \<parameter\_list\> ) \] RETURN \<return\_type\>;  
END \<package\_name\>;

### **Example: Package Specification**

CREATE OR REPLACE PACKAGE employee\_mgmt AS  
    \-- Public constant  
    MAX\_SALARY CONSTANT DECIMAL(12, 2\) := 250000.00;

    \-- Public exception  
    invalid\_salary EXCEPTION;

    \-- Public procedure declarations  
    PROCEDURE hire\_employee (  
        p\_first\_name VARCHAR,  
        p\_last\_name VARCHAR,  
        p\_salary DECIMAL  
    );

    FUNCTION get\_employee\_name (p\_emp\_id INT) RETURN VARCHAR;  
END employee\_mgmt;

## **CREATE PACKAGE BODY**

Provides the implementation for the subprograms and cursors defined in the package specification. It can also contain private declarations that are hidden from outside the package.

### **Syntax**

CREATE \[ OR REPLACE \] PACKAGE BODY \<package\_name\> AS  
    \-- Private type, variable, and subprogram declarations

    \-- Implementation of public subprograms  
    PROCEDURE \<procedure\_name\> \[ ( \<parameter\_list\> ) \] AS  
    BEGIN  
        \-- implementation code  
    END \<procedure\_name\>;

    FUNCTION \<function\_name\> \[ ( \<parameter\_list\> ) \] RETURN \<return\_type\> AS  
    BEGIN  
        \-- implementation code  
        RETURN \<value\>;  
    END \<function\_name\>;

END \<package\_name\>;

### **Example: Package Body**

CREATE OR REPLACE PACKAGE BODY employee\_mgmt AS  
    \-- Private function (not visible outside the package)  
    FUNCTION validate\_salary(p\_salary DECIMAL) RETURN BOOLEAN AS  
    BEGIN  
        RETURN p\_salary \> 0 AND p\_salary \<= MAX\_SALARY;  
    END validate\_salary;

    \-- Implementation of public procedure  
    PROCEDURE hire\_employee (  
        p\_first\_name VARCHAR,  
        p\_last\_name VARCHAR,  
        p\_salary DECIMAL  
    ) AS  
    BEGIN  
        IF NOT validate\_salary(p\_salary) THEN  
            RAISE invalid\_salary;  
        END IF;

        INSERT INTO employees (first\_name, last\_name, salary)  
        VALUES (p\_first\_name, p\_last\_name, p\_salary);  
    END hire\_employee;

    \-- Implementation of public function  
    FUNCTION get\_employee\_name (p\_emp\_id INT) RETURN VARCHAR AS  
        v\_name VARCHAR(101);  
    BEGIN  
        SELECT first\_name || ' ' || last\_name INTO v\_name  
        FROM employees WHERE id \= p\_emp\_id;  
        RETURN v\_name;  
    END get\_employee\_name;

END employee\_mgmt;

## **ALTER PACKAGE**

Recompiles a package specification or body. This is useful if the package becomes invalid due to changes in dependent objects.

### **Syntax**

ALTER PACKAGE \<package\_name\> COMPILE \[ SPECIFICATION | BODY \];

### **Example**

\-- Recompile the package body if it is invalid  
ALTER PACKAGE employee\_mgmt COMPILE BODY;

## **DROP PACKAGE**

Removes both the package specification and body from the database.

### **Syntax**

DROP PACKAGE \[ BODY \] \<package\_name\>;

### **Parameters**

* BODY: Use this keyword to drop only the package body, leaving the specification intact.

### **Examples**

**1\. Drop the entire package (spec and body):**

DROP PACKAGE employee\_mgmt;

**2\. Drop only the implementation, keeping the public interface:**

DROP PACKAGE BODY employee\_mgmt;  
