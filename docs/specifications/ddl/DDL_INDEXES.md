# **DDL Specification: Indexes**

## **Overview**

An index is a performance-related database object that provides efficient access to data in a table's rows based on the values in the indexed columns. Without an index, the database must perform a full table scan to find relevant rows.

## **CREATE INDEX**

Creates a new index on one or more columns of a table.

### **Syntax**

CREATE \[ UNIQUE \] INDEX \[ CONCURRENTLY \] \[ IF NOT EXISTS \] \<index\_name\>  
    ON \<table\_name\> \[ USING \<method\> \]  
    (  
        { \<column\_name\> | ( \<expression\> ) }  
        \[ COLLATE \<collation\> \] \[ \<opclass\> \]  
        \[ ASC | DESC \] \[ NULLS { FIRST | LAST } \]  
        \[ , ... \]  
    )  
    \[ INCLUDE ( \<column\_list\> ) \]  
    \[ WHERE \<predicate\> \]  
    \[ TABLESPACE \<tablespace\_name\> \];

### **Parameters**

* UNIQUE: Ensures that no two rows have the same value in the indexed column(s).  
* CONCURRENTLY: Creates the index without locking the table against writes.  
* USING \<method\>: Specifies the index type (e.g., BTREE, HASH, GIN, GIST). The default is BTREE.  
* Expression: Allows creating an index on an expression or function, e.g., (LOWER(email)).  
* INCLUDE: Creates a "covering index" by storing extra non-key columns within the index.  
* WHERE \<predicate\>: Creates a "partial index" that only includes rows matching the predicate.

### **Examples**

**1\. Create a standard B-Tree index:**

CREATE INDEX idx\_employees\_lastname ON employees (last\_name);

**2\. Create a unique, multi-column index:**

CREATE UNIQUE INDEX idx\_orders\_customer\_date ON orders (customer\_id, order\_date DESC);

**3\. Create an expression-based index for case-insensitive lookups:**

CREATE INDEX idx\_users\_email\_lower ON users (LOWER(email));

**4\. Create a partial index for active orders only:**

CREATE INDEX idx\_orders\_active ON orders (order\_date)  
WHERE status \= 'ACTIVE';

**5\. Create a covering index to optimize a specific query:**

\-- Speeds up: SELECT name, email FROM users WHERE department\_id \= ?  
CREATE INDEX idx\_users\_dept\_covering ON users (department\_id)  
INCLUDE (name, email);

## **ALTER INDEX**

Modifies an existing index.

### **Syntax**

ALTER INDEX \[ IF EXISTS \] \<index\_name\>  
    { RENAME TO \<new\_index\_name\>  
    | SET TABLESPACE \<tablespace\_name\>  
    | ATTACH PARTITION \<partition\_index\_name\>  
    | SET ( \<storage\_parameter\> \= \<value\> \[ , ... \] ) };

### **Examples**

**1\. Rename an index:**

ALTER INDEX idx\_emp\_name RENAME TO idx\_employees\_last\_name;

**2\. Move an index to a different tablespace (e.g., faster SSDs):**

ALTER INDEX idx\_orders\_active SET TABLESPACE fast\_indexes;

## **DROP INDEX**

Removes an index from the database.

### **Syntax**

DROP INDEX \[ CONCURRENTLY \] \[ IF EXISTS \] \<index\_name\> \[ , ... \] \[ CASCADE | RESTRICT \];

### **Parameters**

* CONCURRENTLY: Drops the index without locking the table.  
* IF EXISTS: Prevents an error if the index does not exist.  
* CASCADE: Automatically drops objects that depend on the index.

### **Examples**

**1\. Drop a single index:**

DROP INDEX idx\_old\_query;

**2\. Safely drop multiple indexes concurrently:**

DROP INDEX CONCURRENTLY IF EXISTS temp\_idx\_1, temp\_idx\_2;

## **REINDEX**

Rebuilds an index or all indexes on a table. Useful after large data modifications or to reclaim space.

### **Syntax**

REINDEX \[ VERBOSE \] { INDEX | TABLE | SCHEMA | DATABASE } \<name\>;

### **Examples**

**1\. Rebuild a single, fragmented index:**

REINDEX INDEX idx\_orders\_customer\_date;

**2\. Rebuild all indexes on a table:**

REINDEX TABLE employees;  
