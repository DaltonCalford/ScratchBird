# Glossary

**Last Updated:** 2026-02-03


Database and ScratchBird terminology.


---

## A

**ACID**
: Atomicity, Consistency, Isolation, Durability - properties that guarantee database transactions are processed reliably.

**Aggregate Function**
: A function that operates on multiple rows to return a single value (e.g., COUNT, SUM, AVG).

**Authentication**
: The process of verifying user identity, typically through username and password.

**Authorization**
: The process of determining what resources a user can access.

---

## B

**B-tree**
: A balanced tree data structure used for most database indexes, efficient for range queries and equality.

**Backup**
: A copy of database data for recovery purposes.

**BRIN Index**
: Block Range Index - efficient for large tables with naturally ordered data.

**Buffer Pool**
: Memory area that caches database pages to reduce disk I/O.

---

## C

**Catalog**
: System tables that store database metadata (tables, columns, indexes, users).

**Checkpoint**
: Process of writing dirty pages from buffer pool to disk.

**Column**
: A vertical field in a table, representing a single attribute.

**Commit**
: Finalize a transaction, making changes permanent.

**Connection Pool**
: A cache of database connections for reuse, improving performance.

**Constraint**
: A rule enforced on data (PRIMARY KEY, FOREIGN KEY, CHECK, UNIQUE, NOT NULL).

**COPY**
: Efficient bulk data loading command.

**CTE**
: Common Table Expression - a named temporary result set (`WITH` clause).

---

## D

**Database**
: A collection of related tables and objects.

**Deadlock**
: A situation where two transactions wait for each other indefinitely.

**DDL**
: Data Definition Language - SQL for defining schema (CREATE, ALTER, DROP).

**DML**
: Data Manipulation Language - SQL for modifying data (INSERT, UPDATE, DELETE).

**DSN**
: Data Source Name - configuration for ODBC connections.

---

## E

**Embedded Mode**
: Running the database engine within an application without a separate server.

**EXPLAIN**
: Command to show the query execution plan.

---

## F

**Foreign Key**
: A constraint that enforces referential integrity between tables.

**Full Table Scan**
: Reading every row in a table, typically indicating a missing index.

---

## G

**GIN Index**
: Generalized Inverted Index - for full-text search, arrays, and JSON.

**GIST Index**
: Generalized Search Tree - for spatial data and complex types.

**Grant**
: SQL command to give privileges to users.

---

## H

**Hash Index**
: Index type optimized for equality comparisons only.

**HBA**
: Host-Based Authentication - configuration controlling which hosts can connect.

---

## I

**Index**
: Data structure that improves query performance by providing quick lookups.

**Index Scan**
: Using an index to locate rows efficiently.

**Isolation Level**
: Defines how transaction changes are visible to other transactions.

---

## J

**JDBC**
: Java Database Connectivity - Java API for database access.

**Join**
: Combining rows from multiple tables based on related columns.

---

## K

**Key**
: A column or set of columns that uniquely identifies rows (PRIMARY KEY) or references other tables (FOREIGN KEY).

---

## L

**Lock**
: Mechanism to control concurrent access to data.

**Log**
: Record of database operations for recovery and auditing.

---

## M

**MGA**
: Multi-Generational Architecture - concurrency model where old row versions are kept for readers.

**MVCC**
: Multi-Version Concurrency Control - similar concept to MGA.

---

## N

**Native Protocol**
: ScratchBird's own wire protocol (port 3092).

**NULL**
: Special value representing unknown or missing data.

---

## O

**ODBC**
: Open Database Connectivity - standard API for database access.

**OID**
: Object Identifier - internal identifier for database objects.

---

## P

**Page**
: Fixed-size unit of storage (typically 8 KB).

**Partitioning**
: Dividing a large table into smaller pieces for manageability.

**PID**
: Process Identifier.

**Prepared Statement**
: Pre-compiled SQL statement that can be executed multiple times with different parameters.

**Primary Key**
: Column(s) that uniquely identify each row in a table.

**Privilege**
: Permission to perform specific operations.

---

## Q

**Query**
: SQL statement that retrieves or modifies data.

**Query Plan**
: The steps the database uses to execute a query.

---

## R

**RDBMS**
: Relational Database Management System.

**Replication**
: Copying data to multiple servers for availability or load distribution.

**Rollback**
: Undo a transaction's changes.

**Row**
: A horizontal record in a table.

**RLS**
: Row-Level Security - restricting which rows users can access.

---

## S

**Schema**
: A namespace for organizing database objects.

**SCRAM**
: Salted Challenge Response Authentication Mechanism - secure password authentication.

**Sequence**
: Object that generates sequential numbers (for auto-increment).

**Session**
: A connection to the database from a client.

**Shared Buffer**
: The buffer pool in ScratchBird configuration.

**SQL**
: Structured Query Language.

**SSL/TLS**
: Encryption protocols for secure connections.

**Stored Procedure**
: Server-side code that can be called from SQL.

---

## T

**Table**
: Collection of rows organized into columns.

**Tablespace**
: Storage location for database files.

**Transaction**
: A sequence of operations treated as a single unit.

**Trigger**
: Code that executes automatically on data changes.

**Tuple**
: A row in a table.

---

## U

**UNIQUE Constraint**
: Ensures all values in a column are distinct.

**UUID**
: Universally Unique Identifier - 128-bit identifier.

---

## V

**Vacuum**
: Process that reclaims space from deleted rows.

**View**
: Virtual table defined by a query.

---

## W

**WAL**
: Write-Ahead Log - transaction log written before data pages.

**Wire Protocol**
: Network protocol for client-server communication.

**Work Memory**
: Memory allocated per operation for sorting and hashing.

---

## See Also

- [FAQ](faq/index.md)
- [SQL Language Guide](language-guide/index.md)
