# Glossary

**Last Updated:** 2026-01-30

Database and ScratchBird terminology reference.

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

**Back-Versioning**
: ScratchBird's MGA approach where updates modify the primary record in-place and move old data to a back version. Contrast with PostgreSQL's forward-versioning.

**B-tree**
: A balanced tree data structure used for most database indexes, efficient for range queries and equality.

**Backup**
: A copy of database data for recovery purposes.

**Bitmap Index**
: Index type optimized for low-cardinality columns using bitmap representations.

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

**CLOG**
: Commit Log - tracks transaction states (committed, aborted) for efficient lookups.

**Columnstore**
: Columnar storage format for analytical queries, storing data column-by-column with per-segment compression and statistics.

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

**DDL**
: Data Definition Language - SQL for defining schema (CREATE, ALTER, DROP).

**Deadlock**
: A situation where two transactions wait for each other indefinitely.

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

**HNSW**
: Hierarchical Navigable Small World - a graph-based index for approximate nearest neighbor vector search. Used for AI/ML similarity queries.

**HBA**
: Host-Based Authentication - configuration controlling which hosts can connect.

**Heap Page**
: A page in the database file that stores table row data.

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

**JSON/JSONB**
: JavaScript Object Notation data types for storing structured data.

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

**LSM-Tree**
: Log-Structured Merge Tree - a write-optimized index structure that buffers writes in a memtable and flushes to sorted SSTables with background compaction.

---

## M

**MGA**
: Multi-Generational Architecture - ScratchBird's concurrency model based on Firebird, where old row versions are kept as back-versions for readers. Uses TIP (Transaction Inventory Pages) rather than snapshots.

**MVCC**
: Multi-Version Concurrency Control - PostgreSQL's approach to concurrency. NOT the same as MGA despite similar goals.

---

## N

**Native Protocol**
: ScratchBird's own wire protocol (port 3092).

**N2O (Newest-to-Oldest)**
: Version chain direction in MGA where the primary record contains the newest data and points backward to older versions.

**NULL**
: Special value representing unknown or missing data.

---

## O

**OAT**
: Oldest Active Transaction - a transaction marker used in MGA.

**ODBC**
: Open Database Connectivity - standard API for database access.

**OID**
: Object Identifier - internal identifier for database objects.

**OIT**
: Oldest Interesting Transaction - a transaction marker used in MGA for garbage collection.

**OST**
: Oldest Snapshot Transaction - a transaction marker used in MGA.

---

## P

**Page**
: Fixed-size unit of storage (typically 8 KB).

**Parser**
: Component that converts SQL text into SBLR bytecode.

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

**SBLR**
: ScratchBird Language Representation - the internal bytecode format that all SQL dialects compile to. Similar to Firebird's BLR but extended with 500+ opcodes.

**SP-GiST**
: Space-Partitioned Generalized Search Tree - index for partitioned key spaces (quad-trees, radix trees, text operations).

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

**Sweep**
: MGA garbage collection process that removes obsolete back-versions older than OIT.

---

## T

**Table**
: Collection of rows organized into columns.

**Tablespace**
: Storage location for database files.

**TOAST**
: The Oversized-Attribute Storage Technique - mechanism for storing large values (> ~2000 bytes) out-of-line. In ScratchBird, includes MGA visibility tracking for proper garbage collection.

**TSVECTOR/TSQUERY**
: Text search data types. TSVECTOR stores a sorted list of lexemes; TSQUERY represents a text search query. Used with full-text and GIN indexes.

**TID**
: Tuple Identifier - the location of a row (page number + line number).

**TIP**
: Transaction Inventory Page - a bitmap storing 2 bits per transaction ID to track transaction states (active, committed, aborted, limbo). Central to MGA visibility checking.

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
: Process that reclaims space from deleted rows (PostgreSQL terminology).

**View**
: Virtual table defined by a query.

**Visibility**
: Whether a row version is visible to a given transaction. In MGA, determined by TIP lookup, not snapshots.

---

## W

**WAL**
: Write-Ahead Log - transaction log written before data pages. In ScratchBird, WAL is optional and not required for recovery.

**Wire Protocol**
: Network protocol for client-server communication.

**Work Memory**
: Memory allocated per operation for sorting and hashing.

---

## Y

**Y-Valve**
: ScratchBird's architecture component that routes connections to the appropriate parser based on protocol.

---

## See Also

- [FAQ](../FAQ.md)
- [SQL Syntax Reference](SQL-Syntax.md)
- [Developer Guide](../developer-guide/README.md)
