# **Temporal Tables Specification**

## **1\. Introduction**

Temporal tables are a powerful feature that brings built-in support for time-based data management to the SQL language. ScratchBird implements **system-versioned temporal tables**, which automatically keep a full history of all data changes (UPDATEs and DELETEs).

This allows you to query the state of your data as it existed at any point in the past, providing a robust foundation for auditing, trend analysis, and point-in-time recovery.

### **How It Works**

When you define a table with system versioning, ScratchBird automatically creates a hidden **history table**.

* When a row is **updated**, the previous version of the row is moved to the history table.  
* When a row is **deleted**, the final version of the row is moved to the history table.

The main table always contains the current "live" data. Queries against the main table are unaffected unless you use special time-travel query syntax.

## **2\. Creating Temporal Tables**

You enable system versioning on a table during its creation using the WITH SYSTEM VERSIONING clause. This requires defining two TIMESTAMP columns that will act as the system-time period.

### **CREATE TABLE ... WITH SYSTEM VERSIONING**

**Syntax:**

CREATE TABLE table\_name (  
    column\_definitions...,  
    valid\_from TIMESTAMP GENERATED ALWAYS AS ROW START,  
    valid\_to   TIMESTAMP GENERATED ALWAYS AS ROW END,  
    PERIOD FOR SYSTEM\_TIME (valid\_from, valid\_to)  
)  
WITH SYSTEM VERSIONING  
\[ ( HISTORY\_TABLE \= history\_table\_name ) \];

**Key Components:**

* **ROW START / ROW END Columns**: Two TIMESTAMP columns are required to store the validity period for each row version. The GENERATED ALWAYS AS clause ensures the database manages their values automatically.  
* **PERIOD FOR SYSTEM\_TIME**: This clause formally defines the two columns as the system-time period.  
* **WITH SYSTEM VERSIONING**: This clause activates the temporal functionality.  
* **HISTORY\_TABLE (Optional)**: You can optionally specify a name for the history table. If omitted, ScratchBird creates one with a default name.

**Example:**

CREATE TABLE employees (  
    emp\_id        INT PRIMARY KEY,  
    emp\_name      TEXT NOT NULL,  
    department    TEXT NOT NULL,  
    salary        DECIMAL(10, 2\) NOT NULL,

    \-- System-time period columns  
    sys\_start     TIMESTAMP GENERATED ALWAYS AS ROW START,  
    sys\_end       TIMESTAMP GENERATED ALWAYS AS ROW END,  
    PERIOD FOR SYSTEM\_TIME (sys\_start, sys\_end)  
)  
WITH SYSTEM VERSIONING ( HISTORY\_TABLE \= employees\_history );

\-- When a row is inserted, sys\_end is set to a "far-future" timestamp.  
INSERT INTO employees (emp\_id, emp\_name, department, salary)  
VALUES (101, 'John Doe', 'Engineering', 80000);

\-- When this row is updated...  
UPDATE employees SET salary \= 90000 WHERE emp\_id \= 101;  
\-- ...the original version (with salary 80000\) is moved to 'employees\_history',  
\-- and its 'sys\_end' is set to the transaction timestamp. The main table  
\-- now contains the new version with salary 90000\.

## **3\. Querying Temporal Data (Time Travel)**

The true power of temporal tables is realized through the extended SELECT syntax, which allows you to query past states of the data.

### **FOR SYSTEM\_TIME AS OF ...**

This clause returns the version of the data that was valid at a specific point in time. It effectively executes the query against the state of the database at that timestamp.

**Syntax:**

SELECT ... FROM table\_name  
    FOR SYSTEM\_TIME AS OF 'timestamp';

**Example:**

\-- What was John Doe's salary on July 1st, 2024?  
SELECT salary  
FROM employees FOR SYSTEM\_TIME AS OF '2024-07-01 12:00:00'  
WHERE emp\_id \= 101;

### **FOR SYSTEM\_TIME BETWEEN ... AND ...**

This clause returns all historical versions of rows that were active at any point within the specified time range.

**Syntax:**

SELECT ... FROM table\_name  
    FOR SYSTEM\_TIME BETWEEN 'start\_timestamp' AND 'end\_timestamp';

**Example:**

\-- Show the complete salary history for John Doe  
SELECT emp\_name, salary, sys\_start, sys\_end  
FROM employees FOR SYSTEM\_TIME BETWEEN '2020-01-01' AND '2025-01-01'  
WHERE emp\_id \= 101;

### **FOR SYSTEM\_TIME ALL**

This clause returns all rows from both the current table and the history table, showing the complete version history.

**Syntax:**

SELECT ... FROM table\_name FOR SYSTEM\_TIME ALL;

## **4\. Managing Temporal Tables**

### **ALTER TABLE ... ADD SYSTEM VERSIONING**

You can add system versioning to an existing table. This operation will automatically create the history table and backfill it if the table already contains data.

**Syntax:**

ALTER TABLE table\_name  
    ADD PERIOD FOR SYSTEM\_TIME (start\_column, end\_column);

ALTER TABLE table\_name  
    ADD SYSTEM VERSIONING ( HISTORY\_TABLE \= history\_table\_name );

### **ALTER TABLE ... DROP SYSTEM VERSIONING**

This command deactivates the temporal functionality. The history table is preserved as a regular standalone table but is no longer automatically updated.

**Syntax:**

ALTER TABLE table\_name  
    DROP SYSTEM VERSIONING;

## **5\. Dropping Temporal Tables**

When you DROP a system-versioned table, ScratchBird will also automatically drop its associated history table.

**Syntax:**

DROP TABLE employees; \-- This will also drop the 'employees\_history' table.  
