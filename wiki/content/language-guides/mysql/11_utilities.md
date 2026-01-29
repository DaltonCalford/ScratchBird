[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL - Utilities

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

---

## Overview

This document covers utility statements in MySQL emulation mode, including SHOW commands, DESCRIBE, USE, EXPLAIN, and administrative commands. These statements help inspect database structure and manage sessions.

---

## SHOW Statements

SHOW statements display information about databases, tables, and server status.

### SHOW DATABASES

List available databases.

```sql
SHOW DATABASES;
SHOW SCHEMAS;  -- Synonym

-- Filter by pattern
SHOW DATABASES LIKE 'test%';
SHOW DATABASES WHERE `Database` = 'mydb';
```

### SHOW TABLES

List tables in current or specified database.

```sql
-- Tables in current database
SHOW TABLES;

-- Tables in specific database
SHOW TABLES FROM mydb;
SHOW TABLES IN mydb;

-- Filter by pattern
SHOW TABLES LIKE 'user%';

-- Show table type
SHOW FULL TABLES;
SHOW FULL TABLES WHERE Table_type = 'BASE TABLE';
SHOW FULL TABLES WHERE Table_type = 'VIEW';
```

### SHOW COLUMNS / DESCRIBE

Display column information for a table.

```sql
-- Show columns
SHOW COLUMNS FROM users;
SHOW COLUMNS FROM mydb.users;

-- Full column info (includes privileges, comments)
SHOW FULL COLUMNS FROM users;

-- Filter columns
SHOW COLUMNS FROM users LIKE 'email%';
SHOW COLUMNS FROM users WHERE `Null` = 'YES';

-- DESCRIBE is a synonym
DESCRIBE users;
DESC users;

-- Specific column
DESCRIBE users email;
```

**Output columns:**
| Column | Description |
|--------|-------------|
| Field | Column name |
| Type | Data type |
| Null | YES or NO |
| Key | PRI, UNI, MUL, or empty |
| Default | Default value |
| Extra | Additional info (auto_increment, etc.) |

### SHOW CREATE TABLE

Display the CREATE TABLE statement.

```sql
SHOW CREATE TABLE users;
SHOW CREATE TABLE mydb.orders;
```

**Output includes:**
- Column definitions with types and constraints
- Primary key and indexes
- Foreign key constraints
- Table options (engine, charset, etc.)

### SHOW CREATE DATABASE

Display the CREATE DATABASE statement.

```sql
SHOW CREATE DATABASE mydb;
SHOW CREATE SCHEMA mydb;
```

### SHOW CREATE VIEW

Display the CREATE VIEW statement.

```sql
SHOW CREATE VIEW active_users;
```

### SHOW CREATE PROCEDURE / FUNCTION

Display stored routine definitions.

```sql
SHOW CREATE PROCEDURE process_order;
SHOW CREATE FUNCTION calculate_tax;
```

### SHOW INDEX

Display index information for a table.

```sql
SHOW INDEX FROM users;
SHOW INDEXES FROM users;
SHOW KEYS FROM users;

-- From specific database
SHOW INDEX FROM mydb.users;

-- Filter
SHOW INDEX FROM users WHERE Key_name = 'PRIMARY';
```

**Output columns:**
| Column | Description |
|--------|-------------|
| Table | Table name |
| Non_unique | 0 if unique, 1 if not |
| Key_name | Index name |
| Seq_in_index | Column position in index |
| Column_name | Column name |
| Collation | Sort order (A = ascending) |
| Cardinality | Estimated unique values |
| Null | YES if column can be NULL |
| Index_type | BTREE, HASH, FULLTEXT, SPATIAL |

### SHOW TABLE STATUS

Display table metadata and statistics.

```sql
-- All tables in current database
SHOW TABLE STATUS;

-- From specific database
SHOW TABLE STATUS FROM mydb;

-- Filter by name
SHOW TABLE STATUS LIKE 'user%';
SHOW TABLE STATUS WHERE Name = 'orders';
```

**Output columns:**
| Column | Description |
|--------|-------------|
| Name | Table name |
| Engine | Storage engine |
| Version | Table format version |
| Row_format | Row storage format |
| Rows | Estimated row count |
| Avg_row_length | Average row length |
| Data_length | Data file size |
| Index_length | Index file size |
| Auto_increment | Next auto_increment value |
| Create_time | Table creation time |
| Update_time | Last update time |
| Collation | Character collation |

### SHOW PROCESSLIST

Display active connections and queries.

```sql
-- Summary view
SHOW PROCESSLIST;

-- Full query text
SHOW FULL PROCESSLIST;
```

**Output columns:**
| Column | Description |
|--------|-------------|
| Id | Connection ID |
| User | Username |
| Host | Client host |
| db | Current database |
| Command | Query, Sleep, etc. |
| Time | Seconds in current state |
| State | Current operation |
| Info | Query text |

### SHOW VARIABLES

Display server variables.

```sql
-- All variables
SHOW VARIABLES;

-- Global variables
SHOW GLOBAL VARIABLES;

-- Session variables
SHOW SESSION VARIABLES;

-- Filter by name
SHOW VARIABLES LIKE 'max_connections';
SHOW VARIABLES LIKE '%timeout%';
SHOW VARIABLES WHERE Variable_name LIKE 'innodb%';
```

### SHOW STATUS

Display server status counters.

```sql
-- Session status
SHOW STATUS;

-- Global status
SHOW GLOBAL STATUS;

-- Filter
SHOW STATUS LIKE 'Connections';
SHOW STATUS LIKE 'Threads%';
SHOW GLOBAL STATUS WHERE Variable_name LIKE 'Com_%';
```

### SHOW WARNINGS / ERRORS

Display warnings and errors from last statement.

```sql
-- Show warnings
SHOW WARNINGS;
SHOW WARNINGS LIMIT 10;

-- Show errors only
SHOW ERRORS;
SHOW ERRORS LIMIT 5;

-- Count warnings/errors
SHOW COUNT(*) WARNINGS;
SHOW COUNT(*) ERRORS;
```

### SHOW GRANTS

Display user privileges.

```sql
-- Current user
SHOW GRANTS;
SHOW GRANTS FOR CURRENT_USER;

-- Specific user
SHOW GRANTS FOR 'john'@'localhost';
```

### SHOW TRIGGERS

Display trigger definitions.

```sql
-- All triggers
SHOW TRIGGERS;

-- From specific database
SHOW TRIGGERS FROM mydb;

-- Filter
SHOW TRIGGERS LIKE 'user%';
SHOW TRIGGERS WHERE `Table` = 'orders';
```

### SHOW PROCEDURE STATUS / FUNCTION STATUS

Display stored routine information.

```sql
-- All procedures
SHOW PROCEDURE STATUS;
SHOW PROCEDURE STATUS WHERE Db = 'mydb';

-- All functions
SHOW FUNCTION STATUS;
SHOW FUNCTION STATUS LIKE 'calc%';
```

### SHOW EVENTS

Display scheduled events.

```sql
SHOW EVENTS;
SHOW EVENTS FROM mydb;
SHOW EVENTS WHERE Status = 'ENABLED';
```

### SHOW PLUGINS

Display installed plugins.

```sql
SHOW PLUGINS;
```

### SHOW ENGINE STATUS

Display storage engine status.

```sql
SHOW ENGINE INNODB STATUS;
SHOW ENGINE PERFORMANCE_SCHEMA STATUS;
```

### SHOW CHARSET / COLLATION

Display character sets and collations.

```sql
-- Character sets
SHOW CHARACTER SET;
SHOW CHARSET;
SHOW CHARACTER SET LIKE 'utf%';

-- Collations
SHOW COLLATION;
SHOW COLLATION WHERE Charset = 'utf8mb4';
```

---

## USE Statement

Change the current database.

```sql
USE mydb;
USE `database-with-special-name`;
```

After USE, unqualified table names refer to tables in the selected database:

```sql
USE mydb;
SELECT * FROM users;  -- Same as: SELECT * FROM mydb.users;
```

---

## EXPLAIN / DESCRIBE

Analyze query execution plans.

### EXPLAIN SELECT

Display query execution plan.

```sql
-- Basic explain
EXPLAIN SELECT * FROM users WHERE email = 'john@example.com';

-- Extended format
EXPLAIN EXTENDED SELECT * FROM orders WHERE status = 'pending';

-- JSON format (MySQL 5.6+)
EXPLAIN FORMAT=JSON SELECT * FROM products WHERE category_id = 1;

-- Tree format (MySQL 8.0+)
EXPLAIN FORMAT=TREE SELECT * FROM orders WHERE customer_id = 123;

-- Analyze (actually execute and show timing)
EXPLAIN ANALYZE SELECT * FROM users WHERE country = 'USA';
```

**EXPLAIN output columns:**
| Column | Description |
|--------|-------------|
| id | SELECT identifier |
| select_type | SIMPLE, PRIMARY, SUBQUERY, etc. |
| table | Table name |
| partitions | Matched partitions |
| type | Join type (system, const, ref, range, ALL) |
| possible_keys | Candidate indexes |
| key | Chosen index |
| key_len | Index key length used |
| ref | Columns compared to index |
| rows | Estimated rows to examine |
| filtered | Percentage of rows filtered |
| Extra | Additional information |

**Join types (best to worst):**
| Type | Description |
|------|-------------|
| system | Table has one row |
| const | At most one matching row (by primary key) |
| eq_ref | One row per combination (primary/unique key) |
| ref | Multiple rows per combination (index) |
| range | Index range scan |
| index | Full index scan |
| ALL | Full table scan |

### EXPLAIN for DML

Explain INSERT, UPDATE, DELETE execution.

```sql
EXPLAIN INSERT INTO logs (message) VALUES ('test');
EXPLAIN UPDATE users SET status = 'active' WHERE id = 1;
EXPLAIN DELETE FROM old_logs WHERE created_at < '2025-01-01';
```

---

## SET Statement

Configure session or global variables.

### Session Variables

```sql
-- Set session variable
SET session_variable = value;
SET SESSION session_variable = value;
SET @@session.variable_name = value;
SET @@variable_name = value;  -- Session by default

-- Examples
SET autocommit = 0;
SET sql_mode = 'STRICT_TRANS_TABLES';
SET @user_variable = 'value';
SET NAMES 'utf8mb4';
SET CHARACTER SET utf8mb4;
```

### Global Variables

```sql
-- Set global variable (requires SUPER privilege)
SET GLOBAL variable_name = value;
SET @@global.variable_name = value;

-- Examples
SET GLOBAL max_connections = 200;
SET GLOBAL wait_timeout = 28800;
```

### Common Variables

```sql
-- SQL mode
SET sql_mode = 'STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION';

-- Transaction isolation
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;

-- Autocommit
SET autocommit = 1;  -- Enable
SET autocommit = 0;  -- Disable

-- Character set
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;
SET CHARACTER SET utf8mb4;

-- Time zone
SET time_zone = '+00:00';
SET time_zone = 'America/New_York';

-- Query timeouts
SET max_execution_time = 30000;  -- 30 seconds

-- Foreign key checks (useful for bulk loads)
SET foreign_key_checks = 0;
-- ... bulk operations ...
SET foreign_key_checks = 1;
```

### User-Defined Variables

```sql
-- Set user variable
SET @my_var = 'value';
SET @count = 0;
SET @start_date = '2026-01-01';

-- Use in queries
SELECT * FROM orders WHERE order_date >= @start_date;
SELECT @count := @count + 1 AS row_num, name FROM users;

-- Multiple variables
SET @a = 1, @b = 2, @c = @a + @b;
```

---

## KILL Statement

Terminate connections or queries.

```sql
-- Kill connection
KILL connection_id;
KILL 1234;

-- Kill query only (connection stays open)
KILL QUERY 1234;
```

Find connection IDs:
```sql
SHOW PROCESSLIST;
```

---

## FLUSH Statement

Clear caches and reload configurations.

```sql
-- Flush privileges (reload grant tables)
FLUSH PRIVILEGES;

-- Flush tables
FLUSH TABLES;
FLUSH TABLES users, orders;
FLUSH TABLES WITH READ LOCK;

-- Flush logs
FLUSH LOGS;
FLUSH BINARY LOGS;
FLUSH ENGINE LOGS;
FLUSH ERROR LOGS;
FLUSH GENERAL LOGS;
FLUSH RELAY LOGS;
FLUSH SLOW LOGS;

-- Flush status
FLUSH STATUS;

-- Flush hosts (clear host cache)
FLUSH HOSTS;

-- Flush query cache (if enabled)
FLUSH QUERY CACHE;
```

---

## LOCK / UNLOCK TABLES

Explicit table locking.

### LOCK TABLES

```sql
-- Read lock (shared)
LOCK TABLES users READ;

-- Write lock (exclusive)
LOCK TABLES users WRITE;

-- Multiple tables
LOCK TABLES users READ, orders WRITE;

-- With alias
LOCK TABLES users AS u READ, orders AS o WRITE;
```

### UNLOCK TABLES

```sql
UNLOCK TABLES;
```

**Important:** When you lock tables, you can only access the locked tables until you unlock. All locks are released on UNLOCK TABLES or when the connection closes.

---

## ANALYZE / OPTIMIZE / CHECK / REPAIR

Table maintenance commands.

### ANALYZE TABLE

Update index statistics.

```sql
ANALYZE TABLE users;
ANALYZE TABLE users, orders, products;
ANALYZE NO_WRITE_TO_BINLOG TABLE users;  -- Don't log to binary log
ANALYZE LOCAL TABLE users;  -- Same as NO_WRITE_TO_BINLOG
```

### OPTIMIZE TABLE

Reclaim space and defragment.

```sql
OPTIMIZE TABLE users;
OPTIMIZE TABLE users, orders;
OPTIMIZE NO_WRITE_TO_BINLOG TABLE large_table;
```

### CHECK TABLE

Check table for errors.

```sql
CHECK TABLE users;
CHECK TABLE users QUICK;      -- Don't scan rows
CHECK TABLE users FAST;       -- Only check improperly closed tables
CHECK TABLE users MEDIUM;     -- Scan rows for incorrect links
CHECK TABLE users EXTENDED;   -- Full key lookup
CHECK TABLE users FOR UPGRADE;  -- Check for upgrade compatibility
```

### REPAIR TABLE

Repair corrupted table (MyISAM only in MySQL).

```sql
REPAIR TABLE myisam_table;
REPAIR TABLE myisam_table QUICK;
REPAIR TABLE myisam_table EXTENDED;
REPAIR TABLE myisam_table USE_FRM;
```

---

## Source / Delimiter

Client-side commands (used in mysql client).

### SOURCE

Execute SQL from file.

```sql
SOURCE /path/to/script.sql;
\. /path/to/script.sql
```

### DELIMITER

Change statement delimiter.

```sql
-- Change delimiter for procedure definition
DELIMITER //
CREATE PROCEDURE my_proc()
BEGIN
    SELECT 1;
    SELECT 2;
END //
DELIMITER ;
```

---

## Help

Display help information.

```sql
-- General help
HELP;

-- Help on specific topic
HELP 'SELECT';
HELP 'CREATE TABLE';
HELP 'data types';
HELP 'functions';
```

---

## Known Limitations

### Current Implementation Status

| Command | Status | Notes |
|---------|--------|-------|
| SHOW DATABASES/SCHEMAS | Implemented | Emits EXT_SHOW_DATABASES |
| SHOW TABLES | Implemented | Emits EXT_SHOW_TABLES with FROM/IN and LIKE |
| SHOW COLUMNS | Implemented | Emits EXT_SHOW_COLUMNS |
| SHOW CREATE TABLE | Implemented | Emits EXT_SHOW_CREATE_TABLE |
| SHOW CREATE DATABASE | Implemented | Emits EXT_SHOW_DATABASE |
| SHOW INDEX/INDEXES/KEY | Implemented | Emits EXT_SHOW_INDEXES |
| SHOW TABLE STATUS | Not implemented | Falls through to error |
| SHOW PROCESSLIST | Not implemented | Falls through to error |
| SHOW VARIABLES | Not implemented | Falls through to error |
| SHOW STATUS | Not implemented | Falls through to error |
| SHOW WARNINGS/ERRORS | Not implemented | Falls through to error |
| SHOW GRANTS | Not implemented | Falls through to error |
| USE | Implemented | Updates search_path via EXT_SET_VARIABLE |
| DESCRIBE/DESC | Implemented | Emits EXT_DESCRIBE_TABLE |
| EXPLAIN | Not implemented | Falls through to error |
| SET (session vars) | Implemented | Emits EXT_SET_VARIABLE |
| SET AUTOCOMMIT | Implemented | Emits EXT_SET_AUTOCOMMIT |
| SET TRANSACTION | Implemented | Emits SET_TRANSACTION with isolation mapping |
| KILL | Not implemented | Falls through to error |
| FLUSH | Not implemented | Falls through to error |
| LOCK/UNLOCK TABLES | Not implemented | Explicit error: "not supported in MySQL emulation yet" |
| ANALYZE TABLE | Not implemented | Falls through to error |
| OPTIMIZE TABLE | Not implemented | Falls through to error |
| CHECK TABLE | Not implemented | Falls through to error |
| REPAIR TABLE | Not implemented | Falls through to error |

### Workarounds

**For SHOW commands:** Use `information_schema` queries instead:
```sql
-- Instead of SHOW TABLES
SELECT table_name FROM information_schema.tables
WHERE table_schema = DATABASE();

-- Instead of SHOW COLUMNS
SELECT * FROM information_schema.columns
WHERE table_schema = DATABASE() AND table_name = 'users';
```

**For EXPLAIN:** Basic EXPLAIN works. For detailed analysis, use PostgreSQL emulation mode which has fuller EXPLAIN ANALYZE support.

---

## See Also

- [Session Configuration](10_session_show_set.md) - SET and SHOW details
- [System Catalog](13_system_catalog.md) - information_schema
- [DML SELECT](06_dml_select.md) - Query syntax
- [PostgreSQL Utilities](../postgresql/11_utilities.md) - PostgreSQL equivalents

