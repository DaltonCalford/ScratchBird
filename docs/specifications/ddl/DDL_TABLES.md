# **DDL Specification: Tables**

## **Overview**

Tables are the fundamental structures for storing data in a relational database, organized into rows and columns. ScratchBird supports standard SQL tables, temporary tables, and advanced features like generated columns and UUID identities.

## **CREATE TABLE**

Creates a new table to store data.

### **Syntax**

CREATE \[ { GLOBAL | LOCAL } TEMPORARY | TEMP \] TABLE \[ IF NOT EXISTS \] \<table\_name\> (  
    \<column\_definition\> \[ , ... \]  
    \[ , \<table\_constraint\> \[ , ... \] \]  
)  
\[ WITH ( \<table\_storage\_parameter\> \= \<value\> \[ , ... \] ) \]  
\[ TABLESPACE \<tablespace\_name\> \]  
\[ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } \] \-- For temporary tables  
\[ AS \<select\_statement\> \[ WITH \[NO\] DATA \] \];

### **Column Definition**

\<column\_name\> \<data\_type\> \[ \<column\_constraint\> ... \] \[ WITH ( \<column\_storage\_parameter\> \= \<value\> \[ , ... \] ) \]

### **Column Constraints**

* NOT NULL: Ensures the column cannot have a NULL value.  
* UNIQUE: Ensures all values in the column are unique.  
* PRIMARY KEY: A combination of NOT NULL and UNIQUE, identifying each row.  
* DEFAULT \<value\>: Provides a default value if one is not specified.  
* CHECK (\<expression\>): Constrains the value with a boolean expression.  
* REFERENCES \<other\_table\>(\<column\>): Defines a foreign key relationship.  
* GENERATED ALWAYS AS (\<expression\>) STORED: A column computed from other columns.  
* GENERATED { ALWAYS | BY DEFAULT } AS IDENTITY: An auto-incrementing integer or UUID column.

### **Storage Parameters (Optional)**

Storage parameters configure table defaults and per-column overrides for varlen encoding, TOAST, and numeric storage. See `ScratchBird/docs/specifications/beta_requirements/optional/STORAGE_ENCODING_OPTIMIZATIONS.md`.

**Table storage parameters:**
* storage\_format: 1 or 2 (v2 enables varlen header v2 and per-column TOAST)
* toast\_strategy: plain | extended | external | compressed
* toast\_threshold: integer bytes
* toast\_target: integer bytes
* toast\_compression: none | lz4 | zstd
* numeric\_storage: scaled | packed

**Column storage parameters:**
* toast\_strategy: plain | extended | external | compressed
* toast\_threshold: integer bytes
* toast\_target: integer bytes
* toast\_compression: none | lz4 | zstd
* numeric\_storage: scaled | packed (NUMERIC only)

### **Examples**

**1\. Create a basic table:**

CREATE TABLE employees (  
    id INTEGER PRIMARY KEY,  
    first\_name VARCHAR(50) NOT NULL,  
    last\_name VARCHAR(50) NOT NULL,  
    hire\_date DATE DEFAULT CURRENT\_DATE  
);

**2\. Create a table with foreign keys and checks:**

CREATE TABLE orders (  
    order\_id UUID GENERATED ALWAYS AS IDENTITY, \-- Uses UUIDv7  
    employee\_id INTEGER REFERENCES employees(id),  
    order\_date TIMESTAMP NOT NULL,  
    amount DECIMAL(10, 2\) NOT NULL CHECK (amount \> 0),  
    PRIMARY KEY (order\_id)  
);

**3\. Create a temporary table:**

CREATE TEMPORARY TABLE session\_data (  
    key VARCHAR(100) PRIMARY KEY,  
    value JSONB  
) ON COMMIT PRESERVE ROWS;

**4\. Create a table from a query (CTAS):**

CREATE TABLE high\_value\_orders AS  
SELECT \* FROM orders WHERE amount \> 1000  
WITH DATA;

**5\. Create a table with v2 storage defaults:**

CREATE TABLE documents (  
    id BIGINT PRIMARY KEY,  
    title VARCHAR(200) WITH (toast\_strategy \= plain),  
    body TEXT WITH (toast\_strategy \= external, toast\_compression \= lz4),  
    price NUMERIC(50, 4) WITH (numeric\_storage \= packed)  
)  
WITH (storage\_format \= 2, toast\_threshold \= 2048);

**6\. Update table defaults and override a column:**

ALTER TABLE documents SET (toast\_strategy \= external, toast\_compression \= zstd);  
ALTER TABLE documents ALTER COLUMN title  
    SET (toast\_strategy \= plain);

## **ALTER TABLE**

Modifies the structure of an existing table.

### **Syntax**

ALTER TABLE \[ IF EXISTS \] \<table\_name\>  
    { ADD \[ COLUMN \] \<column\_definition\>  
    | DROP \[ COLUMN \] \[ IF EXISTS \] \<column\_name\> \[ CASCADE | RESTRICT \]  
    | ALTER \[ COLUMN \] \<column\_name\> { SET DATA TYPE \<new\_type\> | SET DEFAULT \<expr\> | DROP DEFAULT | { SET | DROP } NOT NULL }  
    | ADD \<table\_constraint\>  
    | DROP CONSTRAINT \[ IF EXISTS \] \<constraint\_name\> \[ CASCADE | RESTRICT \]  
    | RENAME \[ COLUMN \] \<column\_name\> TO \<new\_column\_name\>  
    | RENAME TO \<new\_table\_name\>  
    | SET SCHEMA \<new\_schema\>  
    | OWNER TO \<new\_owner\> };

### **Examples**

**1\. Add a new column:**

ALTER TABLE employees ADD COLUMN email VARCHAR(100) UNIQUE;

**2\. Change a column's data type:**

ALTER TABLE orders ALTER COLUMN amount TYPE DECIMAL(12, 2);

**3\. Add a foreign key constraint:**

ALTER TABLE employees ADD CONSTRAINT fk\_manager  
    FOREIGN KEY (manager\_id) REFERENCES employees(id);

**4\. Drop a column:**

ALTER TABLE employees DROP COLUMN old\_employee\_code;

**5\. Rename a table:**

ALTER TABLE employees RENAME TO staff;

## **DROP TABLE**

Removes a table and all its data. This operation is irreversible.

### **Syntax**

DROP TABLE \[ IF EXISTS \] \<table\_name\> \[ , ... \] \[ CASCADE | RESTRICT \];

### **Parameters**

* IF EXISTS: Prevents an error if the table does not exist.  
* CASCADE: Automatically drops dependent objects (like views or foreign keys referencing this table).  
* RESTRICT: (Default) Prevents dropping if other objects depend on it.

### **Examples**

**1\. Drop a single table:**

DROP TABLE old\_products;

**2\. Drop multiple tables safely:**

DROP TABLE IF EXISTS temp\_table1, temp\_table2;

**3\. Drop a table and its dependent views:**

\-- WARNING: This will also drop any view that uses 'employees'.  
DROP TABLE employees CASCADE;

## **TRUNCATE TABLE**

Quickly removes all rows from a table. It is much faster than DELETE for large tables as it doesn't scan the table.

### **Syntax**

TRUNCATE \[ TABLE \] \<table\_name\> \[ , ... \]  
    \[ RESTART IDENTITY | CONTINUE IDENTITY \]  
    \[ CASCADE | RESTRICT \];

### **Examples**

**1\. Truncate a log table and reset its identity sequence:**

TRUNCATE TABLE event\_log RESTART IDENTITY;  
