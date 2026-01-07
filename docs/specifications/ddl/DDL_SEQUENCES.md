# **DDL Specification: Sequences (Generators)**

## **Overview**

A sequence, also known as a generator, is a database object that produces a sequence of unique integers. Sequences are commonly used to generate primary key values for tables, ensuring that each new row receives a unique ID. ScratchBird prefers an explicit, SQL-style syntax for sequence operations over traditional function calls.

## **CREATE SEQUENCE**

Defines a new sequence generator.

### **Syntax**

CREATE SEQUENCE \[ IF NOT EXISTS \] \<sequence\_name\>  
    \[ AS \<data\_type\> \]  
    \[ START WITH \<start\_value\> \]  
    \[ INCREMENT BY \<increment\_value\> \]  
    \[ MINVALUE \<min\_value\> | NO MINVALUE \]  
    \[ MAXVALUE \<max\_value\> | NO MAXVALUE \]  
    \[ CACHE \<cache\_size\> \]  
    \[ \[NO\] CYCLE \];

### **Parameters**

* AS \<data\_type\>: The underlying integer type (INTEGER, BIGINT). Default is BIGINT.  
* START WITH: The first value the sequence will generate.  
* INCREMENT BY: The value to add for each subsequent number.  
* MINVALUE / MAXVALUE: The lower and upper bounds of the sequence.  
* CACHE: The number of sequence values to pre-allocate in memory for faster access.  
* CYCLE: Allows the sequence to wrap around and restart from MINVALUE after reaching MAXVALUE.

### **Examples**

**1\. Create a standard primary key sequence:**

CREATE SEQUENCE user\_id\_seq  
    START WITH 1000  
    INCREMENT BY 1;

**2\. Create a sequence for stepping by a larger number:**

CREATE SEQUENCE ticket\_number\_seq  
    AS BIGINT  
    START WITH 1000000  
    INCREMENT BY 10;

**3\. Create a sequence that cycles:**

CREATE SEQUENCE four\_digit\_cyclical\_code  
    START WITH 1000  
    MINVALUE 1000  
    MAXVALUE 9999  
    CYCLE;

## **ALTER SEQUENCE**

Modifies the properties of an existing sequence.

### **Syntax**

ALTER SEQUENCE \[ IF EXISTS \] \<sequence\_name\>  
    \[ AS \<data\_type\> \]  
    \[ INCREMENT BY \<increment\_value\> \]  
    \[ MINVALUE \<min\_value\> | NO MINVALUE \]  
    \[ MAXVALUE \<max\_value\> | NO MAXVALUE \]  
    \[ RESTART \[ WITH \<restart\_value\> \] \]  
    \[ CACHE \<cache\_size\> \]  
    \[ \[NO\] CYCLE \];

### **Parameters**

* RESTART \[ WITH \<value\> \]: Resets the sequence counter. If no value is provided, it resets to the original START WITH value.

### **Examples**

**1\. Change the increment of a sequence:**

ALTER SEQUENCE user\_id\_seq INCREMENT BY 2;

**2\. Restart a sequence at a specific number:**

\-- This will cause the next value to be 5000  
ALTER SEQUENCE user\_id\_seq RESTART WITH 5000;

**3\. Disable the cycle behavior:**

ALTER SEQUENCE four\_digit\_cyclical\_code NO CYCLE;

## **DROP SEQUENCE**

Removes a sequence from the database.

### **Syntax**

DROP SEQUENCE \[ IF EXISTS \] \<sequence\_name\> \[ , ... \] \[ CASCADE | RESTRICT \];

### **Parameters**

* CASCADE: If the sequence is tied to a table's IDENTITY column, this will also alter the column to remove the sequence dependency.  
* RESTRICT: (Default) Prevents dropping if an object (like an IDENTITY column) depends on it.

### **Examples**

**1\. Drop a sequence:**

DROP SEQUENCE old\_id\_generator;

**2\. Safely drop a sequence that might not exist:**

DROP SEQUENCE IF EXISTS temp\_seq;

## **Using Sequences (SQL-Style Syntax)**

ScratchBird promotes a more readable, SQL-native syntax for interacting with sequences.

### **Syntax**

* **Get Next Value:** NEXT VALUE FOR \<sequence\_name\>  
* **Get Current Value:** CURRENT VALUE FOR \<sequence\_name\>  
* **Set Value:** SET \<sequence\_name\> TO \<value\>

### **Examples**

**1\. In a table's DEFAULT clause:**

CREATE TABLE invoices (  
    invoice\_id BIGINT PRIMARY KEY DEFAULT (NEXT VALUE FOR ticket\_number\_seq),  
    details TEXT  
);

**2\. In an INSERT statement:**

INSERT INTO users (id, username)  
VALUES (NEXT VALUE FOR user\_id\_seq, 'jdoe');

**3\. In procedural code (PSQL):**

DECLARE  
    @new\_user\_id BIGINT;  
BEGIN  
    SET @new\_user\_id \= NEXT VALUE FOR user\_id\_seq;  
    \-- ... use the new ID  
END;  
