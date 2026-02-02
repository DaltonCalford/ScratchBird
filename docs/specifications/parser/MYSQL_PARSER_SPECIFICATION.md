# MySQL Parser Specification for ScratchBird Emulation

**Version:** 1.0
**Status:** Planning
**Target:** Full MySQL 8.0 SQL Compatibility for Emulated Clients

---

## Executive Summary

This specification covers creating a complete MySQL 8.0 SQL parser separate from the ScratchBird V2 parser, plus the virtual catalog handler for MySQL system tables (INFORMATION_SCHEMA, mysql.*, performance_schema). MySQL clients connecting to ScratchBird will be able to use native MySQL SQL syntax and see familiar system schemas.

**Reference:** This document follows the architecture defined in `/docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`

---

## Architecture Overview

```
MySQL Client (mysql, MySQL Workbench, phpMyAdmin, etc.)
      |
      v
+---------------------------------------------------------------------+
|                      MySQL Emulation Layer                           |
|  +---------------+  +---------------+  +------------------------+   |
|  | MySQL         |  | MySQL         |  | MySQLCatalogHandler    |   |
|  | Lexer         |-->| Parser       |  | (INFORMATION_SCHEMA,   |   |
|  +---------------+  +---------------+  |  mysql.*, perf_schema) |   |
|         |                   |          +------------------------+   |
|         v                   v                       |               |
|    MySQL Tokens    SBLR Bytecode           Virtual Tables           |
+---------------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------------+
|                    Shared ScratchBird Pipeline                       |
|  +-------------------+                  +------------------------+   |
|  | SBLR Executor     |----------------->| ScratchBird Storage    |   |
|  | (Single impl)     |                  | Engine                 |   |
|  +-------------------+                  +------------------------+   |
+---------------------------------------------------------------------+
```

**Schema Path:** `/remote/emulation/mysql/{server}/{database}/`

---

## MySQL 8.0 Reserved Keywords

MySQL 8.0 has **679 keywords**, with **262 reserved** (~40%). Reserved keywords require backtick quoting when used as identifiers.

### Reserved Keywords (Must Be Quoted as Identifiers)

```
ACCESSIBLE, ADD, ALL, ALTER, ANALYZE, AND, AS, ASC, ASENSITIVE,
BEFORE, BETWEEN, BIGINT, BINARY, BLOB, BOTH, BY, CALL, CASCADE, CASE,
CHANGE, CHAR, CHARACTER, CHECK, COLLATE, COLUMN, CONDITION, CONSTRAINT,
CONTINUE, CONVERT, CREATE, CROSS, CUBE, CUME_DIST, CURRENT_DATE,
CURRENT_TIME, CURRENT_TIMESTAMP, CURRENT_USER, CURSOR, DATABASE,
DATABASES, DAY_HOUR, DAY_MICROSECOND, DAY_MINUTE, DAY_SECOND, DEC,
DECIMAL, DECLARE, DEFAULT, DELAYED, DELETE, DENSE_RANK, DESC, DESCRIBE,
DETERMINISTIC, DISTINCT, DISTINCTROW, DIV, DOUBLE, DROP, DUAL, EACH,
ELSE, ELSEIF, EMPTY, ENCLOSED, ESCAPED, EXCEPT, EXISTS, EXIT, EXPLAIN,
FALSE, FETCH, FIRST_VALUE, FLOAT, FLOAT4, FLOAT8, FOR, FORCE, FOREIGN,
FROM, FULLTEXT, FUNCTION, GENERATED, GET, GRANT, GROUP, GROUPING,
GROUPS, HAVING, HIGH_PRIORITY, HOUR_MICROSECOND, HOUR_MINUTE,
HOUR_SECOND, IF, IGNORE, IN, INDEX, INFILE, INNER, INOUT, INSENSITIVE,
INSERT, INT, INT1, INT2, INT3, INT4, INT8, INTEGER, INTERVAL, INTO,
IO_AFTER_GTIDS, IO_BEFORE_GTIDS, IS, ITERATE, JOIN, JSON_TABLE, KEY,
KEYS, KILL, LAG, LAST_VALUE, LATERAL, LEAD, LEADING, LEAVE, LEFT,
LIKE, LIMIT, LINEAR, LINES, LOAD, LOCALTIME, LOCALTIMESTAMP, LOCK,
LONG, LONGBLOB, LONGTEXT, LOOP, LOW_PRIORITY, MASTER_BIND,
MASTER_SSL_VERIFY_SERVER_CERT, MATCH, MAXVALUE, MEDIUMBLOB, MEDIUMINT,
MEDIUMTEXT, MIDDLEINT, MINUTE_MICROSECOND, MINUTE_SECOND, MOD, MODIFIES,
NATURAL, NOT, NO_WRITE_TO_BINLOG, NTH_VALUE, NTILE, NULL, NUMERIC, OF,
ON, OPTIMIZE, OPTIMIZER_COSTS, OPTION, OPTIONALLY, OR, ORDER, OUT,
OUTER, OUTFILE, OVER, PARTITION, PERCENT_RANK, PRIMARY, PROCEDURE,
PURGE, RANGE, RANK, READ, READS, READ_WRITE, REAL, RECURSIVE,
REFERENCES, REGEXP, RELEASE, RENAME, REPEAT, REPLACE, REQUIRE, RESIGNAL,
RESTRICT, RETURN, REVOKE, RIGHT, RLIKE, ROW, ROWS, ROW_NUMBER, SCHEMA,
SCHEMAS, SECOND_MICROSECOND, SELECT, SENSITIVE, SEPARATOR, SET, SHOW,
SIGNAL, SMALLINT, SPATIAL, SPECIFIC, SQL, SQLEXCEPTION, SQLSTATE,
SQLWARNING, SQL_BIG_RESULT, SQL_CALC_FOUND_ROWS, SQL_SMALL_RESULT, SSL,
STARTING, STORED, STRAIGHT_JOIN, SYSTEM, TABLE, TERMINATED, THEN,
TINYBLOB, TINYINT, TINYTEXT, TO, TRAILING, TRIGGER, TRUE, UNDO, UNION,
UNIQUE, UNLOCK, UNSIGNED, UPDATE, USAGE, USE, USING, UTC_DATE, UTC_TIME,
UTC_TIMESTAMP, VALUES, VARBINARY, VARCHAR, VARCHARACTER, VARYING,
VIRTUAL, WHEN, WHERE, WHILE, WINDOW, WITH, WRITE, XOR, YEAR_MONTH,
ZEROFILL
```

### Querying Keywords Dynamically

MySQL clients can query all keywords via:
```sql
SELECT * FROM INFORMATION_SCHEMA.KEYWORDS;
SELECT WORD FROM INFORMATION_SCHEMA.KEYWORDS WHERE RESERVED = 1;
```

---

## MySQL 8.0 Data Types

### Numeric Types

| Type | Storage | Range (Signed) | Range (Unsigned) |
|------|---------|----------------|------------------|
| TINYINT | 1 byte | -128 to 127 | 0 to 255 |
| SMALLINT | 2 bytes | -32,768 to 32,767 | 0 to 65,535 |
| MEDIUMINT | 3 bytes | -8,388,608 to 8,388,607 | 0 to 16,777,215 |
| INT/INTEGER | 4 bytes | -2,147,483,648 to 2,147,483,647 | 0 to 4,294,967,295 |
| BIGINT | 8 bytes | -9.2×10^18 to 9.2×10^18 | 0 to 1.8×10^19 |
| DECIMAL(M,D) | Variable | Exact precision | Exact precision |
| NUMERIC(M,D) | Variable | Alias for DECIMAL | Alias for DECIMAL |
| FLOAT | 4 bytes | ~7 decimal digits | N/A |
| DOUBLE | 8 bytes | ~15 decimal digits | N/A |
| BIT(M) | (M+7)/8 bytes | Bit field (M bits) | N/A |
| BOOL/BOOLEAN | 1 byte | Alias for TINYINT(1) | N/A |

**MySQL-Specific Modifiers:**
- `UNSIGNED` - Non-negative values only
- `ZEROFILL` - Left-pad with zeros (deprecated in 8.0.17)
- `AUTO_INCREMENT` - Automatic sequential values

### String Types

| Type | Max Length | Description |
|------|------------|-------------|
| CHAR(M) | 255 bytes | Fixed-length string |
| VARCHAR(M) | 65,535 bytes | Variable-length string |
| BINARY(M) | 255 bytes | Fixed-length binary |
| VARBINARY(M) | 65,535 bytes | Variable-length binary |
| TINYTEXT | 255 bytes | Small text |
| TEXT | 65,535 bytes | Standard text |
| MEDIUMTEXT | 16,777,215 bytes | Medium text |
| LONGTEXT | 4,294,967,295 bytes | Large text |
| TINYBLOB | 255 bytes | Small binary |
| BLOB | 65,535 bytes | Standard binary |
| MEDIUMBLOB | 16,777,215 bytes | Medium binary |
| LONGBLOB | 4,294,967,295 bytes | Large binary |
| ENUM('val1','val2',...) | 1-2 bytes | Enumerated values |
| SET('val1','val2',...) | 1-8 bytes | Set of values |

### Date and Time Types

| Type | Storage | Format | Range |
|------|---------|--------|-------|
| DATE | 3 bytes | YYYY-MM-DD | 1000-01-01 to 9999-12-31 |
| TIME | 3 bytes | HH:MM:SS | -838:59:59 to 838:59:59 |
| DATETIME | 8 bytes | YYYY-MM-DD HH:MM:SS | 1000-01-01 00:00:00 to 9999-12-31 23:59:59 |
| TIMESTAMP | 4 bytes | YYYY-MM-DD HH:MM:SS | 1970-01-01 00:00:01 to 2038-01-19 03:14:07 |
| YEAR | 1 byte | YYYY | 1901 to 2155 |

**MySQL-Specific:**
- Fractional seconds: `DATETIME(fsp)`, `TIME(fsp)`, `TIMESTAMP(fsp)` where fsp = 0-6
- `ON UPDATE CURRENT_TIMESTAMP` - Auto-update on row modification

### JSON Type

| Type | Max Size | Description |
|------|----------|-------------|
| JSON | ~1GB | Native JSON storage with validation |

### Spatial Types

| Type | Description |
|------|-------------|
| GEOMETRY | Any geometry type |
| POINT | Single point |
| LINESTRING | Line |
| POLYGON | Polygon |
| MULTIPOINT | Collection of points |
| MULTILINESTRING | Collection of lines |
| MULTIPOLYGON | Collection of polygons |
| GEOMETRYCOLLECTION | Any collection |

---

## MySQL 8.0 Operators

### Arithmetic Operators

| Operator | Description |
|----------|-------------|
| `+` | Addition |
| `-` | Subtraction |
| `*` | Multiplication |
| `/` | Division (returns DOUBLE) |
| `DIV` | Integer division |
| `%`, `MOD` | Modulo |
| `-` (unary) | Negation |

### Comparison Operators

| Operator | Description |
|----------|-------------|
| `=` | Equal |
| `<>`, `!=` | Not equal |
| `<` | Less than |
| `<=` | Less than or equal |
| `>` | Greater than |
| `>=` | Greater than or equal |
| `<=>` | NULL-safe equal (returns 1 if both NULL) |
| `IS NULL` | NULL test |
| `IS NOT NULL` | Not NULL test |
| `BETWEEN ... AND ...` | Range test |
| `NOT BETWEEN ... AND ...` | Not in range |
| `IN (...)` | Set membership |
| `NOT IN (...)` | Not in set |
| `LIKE` | Pattern matching |
| `NOT LIKE` | Pattern not matching |
| `REGEXP`, `RLIKE` | Regular expression match |
| `NOT REGEXP` | Regular expression not match |

### Logical Operators

| Operator | Description |
|----------|-------------|
| `AND`, `&&` | Logical AND |
| `OR`, `\|\|` | Logical OR |
| `NOT`, `!` | Logical NOT |
| `XOR` | Logical XOR |

### Bitwise Operators

| Operator | Description |
|----------|-------------|
| `&` | Bitwise AND |
| `\|` | Bitwise OR |
| `^` | Bitwise XOR |
| `~` | Bitwise NOT |
| `<<` | Left shift |
| `>>` | Right shift |

### JSON Operators

| Operator | Description |
|----------|-------------|
| `->` | JSON path extraction (returns JSON) |
| `->>` | JSON path extraction (returns text) |

### Assignment Operators

| Operator | Context |
|----------|---------|
| `=` | SET statements, UPDATE |
| `:=` | User variable assignment |

### String Operators

| Operator | Description |
|----------|-------------|
| `COLLATE` | Character set collation |

---

## MySQL 8.0 Built-in Functions

### Aggregate Functions

```
AVG(), BIT_AND(), BIT_OR(), BIT_XOR(), COUNT(), COUNT(DISTINCT),
GROUP_CONCAT(), JSON_ARRAYAGG(), JSON_OBJECTAGG(), MAX(), MIN(),
STD(), STDDEV(), STDDEV_POP(), STDDEV_SAMP(), SUM(), VAR_POP(),
VAR_SAMP(), VARIANCE()
```

### Window Functions

```
CUME_DIST(), DENSE_RANK(), FIRST_VALUE(), LAG(), LAST_VALUE(), LEAD(),
NTH_VALUE(), NTILE(), PERCENT_RANK(), RANK(), ROW_NUMBER()
```

**OVER Clause Syntax:**
```sql
function_name() OVER (
    [PARTITION BY expr [, expr ...]]
    [ORDER BY expr [ASC|DESC] [, expr [ASC|DESC] ...]]
    [frame_clause]
)

frame_clause:
    {ROWS | RANGE} {frame_start | BETWEEN frame_start AND frame_end}

frame_start/frame_end:
    UNBOUNDED PRECEDING | n PRECEDING | CURRENT ROW |
    n FOLLOWING | UNBOUNDED FOLLOWING
```

### String Functions

```
ASCII(), BIN(), BIT_LENGTH(), CHAR(), CHAR_LENGTH(), CHARACTER_LENGTH(),
CONCAT(), CONCAT_WS(), ELT(), EXPORT_SET(), FIELD(), FIND_IN_SET(),
FORMAT(), FROM_BASE64(), HEX(), INSERT(), INSTR(), LCASE(), LEFT(),
LENGTH(), LIKE, LOAD_FILE(), LOCATE(), LOWER(), LPAD(), LTRIM(),
MAKE_SET(), MATCH(), MID(), OCT(), OCTET_LENGTH(), ORD(), POSITION(),
QUOTE(), REPEAT(), REPLACE(), REVERSE(), RIGHT(), RPAD(), RTRIM(),
SOUNDEX(), SOUNDS LIKE, SPACE(), STRCMP(), SUBSTR(), SUBSTRING(),
SUBSTRING_INDEX(), TO_BASE64(), TRIM(), UCASE(), UNHEX(), UPPER(),
WEIGHT_STRING()
```

### Numeric Functions

```
ABS(), ACOS(), ASIN(), ATAN(), ATAN2(), CEIL(), CEILING(), CONV(),
COS(), COT(), CRC32(), DEGREES(), EXP(), FLOOR(), LN(), LOG(), LOG2(),
LOG10(), MOD(), PI(), POW(), POWER(), RADIANS(), RAND(), ROUND(),
SIGN(), SIN(), SQRT(), TAN(), TRUNCATE()
```

### Date and Time Functions

```
ADDDATE(), ADDTIME(), CONVERT_TZ(), CURDATE(), CURRENT_DATE(),
CURRENT_TIME(), CURRENT_TIMESTAMP(), CURTIME(), DATE(), DATE_ADD(),
DATE_FORMAT(), DATE_SUB(), DATEDIFF(), DAY(), DAYNAME(), DAYOFMONTH(),
DAYOFWEEK(), DAYOFYEAR(), EXTRACT(), ALTER_ELEMENT(), FROM_DAYS(), FROM_UNIXTIME(),
GET_FORMAT(), HOUR(), LAST_DAY(), LOCALTIME(), LOCALTIMESTAMP(),
MAKEDATE(), MAKETIME(), MICROSECOND(), MINUTE(), MONTH(), MONTHNAME(),
NOW(), PERIOD_ADD(), PERIOD_DIFF(), QUARTER(), SEC_TO_TIME(), SECOND(),
STR_TO_DATE(), SUBDATE(), SUBTIME(), SYSDATE(), TIME(), TIME_FORMAT(),
TIME_TO_SEC(), TIMEDIFF(), TIMESTAMP(), TIMESTAMPADD(), TIMESTAMPDIFF(),
TO_DAYS(), TO_SECONDS(), UNIX_TIMESTAMP(), UTC_DATE(), UTC_TIME(),
UTC_TIMESTAMP(), WEEK(), WEEKDAY(), WEEKOFYEAR(), YEAR(), YEARWEEK()
```

### JSON Functions

```
JSON_ARRAY(), JSON_ARRAYAGG(), JSON_ARRAY_APPEND(), JSON_ARRAY_INSERT(),
JSON_CONTAINS(), JSON_CONTAINS_PATH(), JSON_DEPTH(), JSON_EXTRACT(),
JSON_INSERT(), JSON_KEYS(), JSON_LENGTH(), JSON_MERGE_PATCH(),
JSON_MERGE_PRESERVE(), JSON_OBJECT(), JSON_OBJECTAGG(), JSON_OVERLAPS(),
JSON_PRETTY(), JSON_QUOTE(), JSON_REMOVE(), JSON_REPLACE(), JSON_SCHEMA_VALID(),
JSON_SCHEMA_VALIDATION_REPORT(), JSON_SEARCH(), JSON_SET(), JSON_STORAGE_FREE(),
JSON_STORAGE_SIZE(), JSON_TABLE(), JSON_TYPE(), JSON_UNQUOTE(), JSON_VALID(),
JSON_VALUE(), MEMBER OF()
```

### Control Flow Functions

```
CASE ... WHEN ... THEN ... ELSE ... END
IF(condition, true_val, false_val)
IFNULL(expr1, expr2)
NULLIF(expr1, expr2)
COALESCE(expr1, expr2, ...)
```

### Cast Functions

```
BINARY, CAST(expr AS type), CONVERT(expr, type), CONVERT(expr USING charset)
```

### Information Functions

```
BENCHMARK(), CHARSET(), COERCIBILITY(), COLLATION(), CONNECTION_ID(),
CURRENT_ROLE(), CURRENT_USER(), DATABASE(), FOUND_ROWS(), ICU_VERSION(),
LAST_INSERT_ID(), ROW_COUNT(), SCHEMA(), SESSION_USER(), SYSTEM_USER(),
USER(), VERSION()
```

### Encryption Functions

```
AES_DECRYPT(), AES_ENCRYPT(), MD5(), RANDOM_BYTES(), SHA1(), SHA2(),
STATEMENT_DIGEST(), STATEMENT_DIGEST_TEXT()
```

---

## MySQL 8.0 DML Syntax

### SELECT Statement

```sql
SELECT
    [ALL | DISTINCT | DISTINCTROW]
    [HIGH_PRIORITY]
    [STRAIGHT_JOIN]
    [SQL_SMALL_RESULT | SQL_BIG_RESULT]
    [SQL_BUFFER_RESULT]
    [SQL_NO_CACHE]
    [SQL_CALC_FOUND_ROWS]
    select_expr [, select_expr ...]
    [INTO @var_name [, @var_name ...] | INTO OUTFILE 'file_name' | INTO DUMPFILE 'file_name']
    [FROM table_references
        [PARTITION partition_list]]
    [WHERE where_condition]
    [GROUP BY {col_name | expr | position} [ASC | DESC], ... [WITH ROLLUP]]
    [HAVING where_condition]
    [WINDOW window_name AS (window_spec) [, window_name AS (window_spec)] ...]
    [ORDER BY {col_name | expr | position} [ASC | DESC] [, ...]]
    [LIMIT {[offset,] row_count | row_count OFFSET offset}]
    [FOR {UPDATE | SHARE}
        [OF tbl_name [, tbl_name ...]]
        [NOWAIT | SKIP LOCKED]]
```

### JOIN Syntax

```sql
table_reference:
    table_factor
  | joined_table

table_factor:
    tbl_name [[AS] alias] [index_hint_list]
  | (table_references)
  | {OJ table_reference LEFT OUTER JOIN table_reference ON conditional_expr}
  | (SELECT ...) [AS] alias

joined_table:
    table_reference [INNER | CROSS] JOIN table_factor [join_specification]
  | table_reference STRAIGHT_JOIN table_factor [ON conditional_expr]
  | table_reference {LEFT | RIGHT} [OUTER] JOIN table_reference join_specification
  | table_reference NATURAL [{LEFT | RIGHT} [OUTER]] JOIN table_factor

join_specification:
    ON conditional_expr
  | USING (column_list)
```

### Common Table Expressions (WITH Clause)

```sql
WITH [RECURSIVE]
    cte_name [(col_name [, col_name ...])] AS (subquery)
    [, cte_name [(col_name [, col_name ...])] AS (subquery)] ...
SELECT ...
```

### INSERT Statement

```sql
INSERT [LOW_PRIORITY | DELAYED | HIGH_PRIORITY] [IGNORE]
    [INTO] tbl_name
    [PARTITION (partition_name [, partition_name] ...)]
    [(col_name [, col_name] ...)]
    { {VALUES | VALUE} (value_list) [, (value_list)] ...
      | SELECT ...
      | TABLE tbl_name
      | SET col_name={expr | DEFAULT} [, col_name={expr | DEFAULT}] ...
    }
    [AS row_alias[(col_alias [, col_alias] ...)]]
    [ON DUPLICATE KEY UPDATE
        col_name = {expr | DEFAULT | [row_alias.]col_name} [, ...]]
```

### UPDATE Statement

```sql
UPDATE [LOW_PRIORITY] [IGNORE] table_reference
    SET col_name = {expr | DEFAULT} [, col_name = {expr | DEFAULT}] ...
    [WHERE where_condition]
    [ORDER BY ...]
    [LIMIT row_count]

-- Multi-table UPDATE
UPDATE [LOW_PRIORITY] [IGNORE] table_references
    SET col_name = {expr | DEFAULT} [, col_name = {expr | DEFAULT}] ...
    [WHERE where_condition]
```

### DELETE Statement

```sql
DELETE [LOW_PRIORITY] [QUICK] [IGNORE]
    FROM tbl_name [[AS] tbl_alias]
    [PARTITION (partition_name [, partition_name] ...)]
    [WHERE where_condition]
    [ORDER BY ...]
    [LIMIT row_count]

-- Multi-table DELETE
DELETE [LOW_PRIORITY] [QUICK] [IGNORE]
    tbl_name[.*] [, tbl_name[.*]] ...
    FROM table_references
    [WHERE where_condition]
```

### REPLACE Statement (MySQL-Specific)

```sql
REPLACE [LOW_PRIORITY | DELAYED]
    [INTO] tbl_name
    [PARTITION (partition_name [, partition_name] ...)]
    [(col_name [, col_name] ...)]
    { {VALUES | VALUE} (value_list) [, (value_list)] ...
      | SET col_name={expr | DEFAULT} [, col_name={expr | DEFAULT}] ...
      | SELECT ...
    }
```

---

## MySQL 8.0 DDL Syntax

### CREATE DATABASE/SCHEMA

```sql
CREATE {DATABASE | SCHEMA} [IF NOT EXISTS] db_name
    [create_specification] ...

create_specification:
    [DEFAULT] CHARACTER SET [=] charset_name
  | [DEFAULT] COLLATE [=] collation_name
  | [DEFAULT] ENCRYPTION [=] {'Y' | 'N'}
```

**Emulation Note:** In ScratchBird emulation, `CREATE DATABASE/SCHEMA` registers emulated schema metadata only and does not create physical database files.

### CREATE TABLE

```sql
CREATE [TEMPORARY] TABLE [IF NOT EXISTS] tbl_name
    (create_definition, ...)
    [table_options]
    [partition_options]

CREATE [TEMPORARY] TABLE [IF NOT EXISTS] tbl_name
    [(create_definition, ...)]
    [table_options]
    [partition_options]
    [IGNORE | REPLACE]
    [AS] query_expression

CREATE [TEMPORARY] TABLE [IF NOT EXISTS] tbl_name
    { LIKE old_tbl_name | (LIKE old_tbl_name) }

create_definition:
    col_name column_definition
  | {INDEX | KEY} [index_name] [index_type] (key_part,...) [index_option] ...
  | {FULLTEXT | SPATIAL} [INDEX | KEY] [index_name] (key_part,...) [index_option] ...
  | [CONSTRAINT [symbol]] PRIMARY KEY [index_type] (key_part,...) [index_option] ...
  | [CONSTRAINT [symbol]] UNIQUE [INDEX | KEY] [index_name] [index_type] (key_part,...) [index_option] ...
  | [CONSTRAINT [symbol]] FOREIGN KEY [index_name] (col_name,...) reference_definition
  | [CONSTRAINT [symbol]] CHECK (expr) [[NOT] ENFORCED]

column_definition:
    data_type [NOT NULL | NULL] [DEFAULT {literal | (expr)}]
      [VISIBLE | INVISIBLE]
      [AUTO_INCREMENT] [UNIQUE [KEY]] [[PRIMARY] KEY]
      [COMMENT 'string']
      [COLLATE collation_name]
      [COLUMN_FORMAT {FIXED | DYNAMIC | DEFAULT}]
      [ENGINE_ATTRIBUTE [=] 'string']
      [SECONDARY_ENGINE_ATTRIBUTE [=] 'string']
      [STORAGE {DISK | MEMORY}]
      [reference_definition]
      [check_constraint_definition]
  | data_type [COLLATE collation_name]
      [GENERATED ALWAYS] AS (expr)
      [VIRTUAL | STORED] [NOT NULL | NULL]
      [VISIBLE | INVISIBLE]
      [UNIQUE [KEY]] [[PRIMARY] KEY]
      [COMMENT 'string']
      [reference_definition]
      [check_constraint_definition]

reference_definition:
    REFERENCES tbl_name (key_part,...)
      [MATCH FULL | MATCH PARTIAL | MATCH SIMPLE]
      [ON DELETE reference_option]
      [ON UPDATE reference_option]

reference_option:
    RESTRICT | CASCADE | SET NULL | NO ACTION | SET DEFAULT
```

### CREATE INDEX

```sql
CREATE [UNIQUE | FULLTEXT | SPATIAL] INDEX index_name
    [index_type]
    ON tbl_name (key_part,...)
    [index_option] ...
    [algorithm_option | lock_option] ...

key_part: {col_name [(length)] | (expr)} [ASC | DESC]

index_type: USING {BTREE | HASH}

index_option:
    KEY_BLOCK_SIZE [=] value
  | index_type
  | WITH PARSER parser_name
  | COMMENT 'string'
  | {VISIBLE | INVISIBLE}
  | ENGINE_ATTRIBUTE [=] 'string'
  | SECONDARY_ENGINE_ATTRIBUTE [=] 'string'

algorithm_option: ALGORITHM [=] {DEFAULT | INPLACE | COPY}
lock_option: LOCK [=] {DEFAULT | NONE | SHARED | EXCLUSIVE}
```

### CREATE VIEW

```sql
CREATE [OR REPLACE]
    [ALGORITHM = {UNDEFINED | MERGE | TEMPTABLE}]
    [DEFINER = user]
    [SQL SECURITY { DEFINER | INVOKER }]
    VIEW view_name [(column_list)]
    AS select_statement
    [WITH [CASCADED | LOCAL] CHECK OPTION]
```

### CREATE TRIGGER

```sql
CREATE [DEFINER = user]
    TRIGGER [IF NOT EXISTS] trigger_name
    trigger_time trigger_event
    ON tbl_name FOR EACH ROW
    [trigger_order]
    trigger_body

trigger_time: { BEFORE | AFTER }
trigger_event: { INSERT | UPDATE | DELETE }
trigger_order: { FOLLOWS | PRECEDES } other_trigger_name
```

### CREATE PROCEDURE/FUNCTION

```sql
CREATE [DEFINER = user]
    PROCEDURE [IF NOT EXISTS] sp_name ([proc_parameter[,...]])
    [characteristic ...] routine_body

CREATE [DEFINER = user]
    FUNCTION [IF NOT EXISTS] sp_name ([func_parameter[,...]])
    RETURNS type
    [characteristic ...] routine_body

proc_parameter: [IN | OUT | INOUT] param_name type
func_parameter: param_name type

characteristic:
    COMMENT 'string'
  | LANGUAGE SQL
  | [NOT] DETERMINISTIC
  | { CONTAINS SQL | NO SQL | READS SQL DATA | MODIFIES SQL DATA }
  | SQL SECURITY { DEFINER | INVOKER }
```

---

## MySQL 8.0 Stored Procedure Syntax

### Compound Statements

```sql
[begin_label:] BEGIN
    [statement_list]
END [end_label]
```

### Variable Declaration

```sql
DECLARE var_name [, var_name] ... type [DEFAULT value]
```

### Condition Handlers

```sql
DECLARE {EXIT | CONTINUE} HANDLER FOR condition_value [, condition_value] ...
    statement

condition_value:
    mysql_error_code
  | SQLSTATE [VALUE] sqlstate_value
  | condition_name
  | SQLWARNING
  | NOT FOUND
  | SQLEXCEPTION
```

### Control Flow

```sql
-- IF
IF search_condition THEN statement_list
    [ELSEIF search_condition THEN statement_list] ...
    [ELSE statement_list]
END IF

-- CASE
CASE case_value
    WHEN when_value THEN statement_list
    [WHEN when_value THEN statement_list] ...
    [ELSE statement_list]
END CASE

-- LOOP
[begin_label:] LOOP
    statement_list
END LOOP [end_label]

-- WHILE
[begin_label:] WHILE search_condition DO
    statement_list
END WHILE [end_label]

-- REPEAT
[begin_label:] REPEAT
    statement_list
UNTIL search_condition
END REPEAT [end_label]

-- ITERATE (continue)
ITERATE label

-- LEAVE (break)
LEAVE label
```

### Cursors

```sql
DECLARE cursor_name CURSOR FOR select_statement
OPEN cursor_name
FETCH cursor_name INTO var_name [, var_name] ...
CLOSE cursor_name
```

---

## MySQL 8.0 System Tables

### INFORMATION_SCHEMA Tables (~37 tables)

| Table | Description |
|-------|-------------|
| SCHEMATA | Databases (schemas) |
| TABLES | Tables in all databases |
| COLUMNS | Columns in all tables |
| STATISTICS | Index statistics |
| KEY_COLUMN_USAGE | Key column constraints |
| TABLE_CONSTRAINTS | Table constraints |
| REFERENTIAL_CONSTRAINTS | Foreign key relationships |
| CHECK_CONSTRAINTS | Check constraints |
| VIEWS | View definitions |
| ROUTINES | Stored procedures/functions |
| PARAMETERS | Routine parameters |
| TRIGGERS | Trigger definitions |
| EVENTS | Scheduled events |
| PARTITIONS | Partition information |
| CHARACTER_SETS | Available character sets |
| COLLATIONS | Available collations |
| COLLATION_CHARACTER_SET_APPLICABILITY | Charset-collation mappings |
| ENGINES | Available storage engines |
| USER_PRIVILEGES | User privileges |
| SCHEMA_PRIVILEGES | Schema privileges |
| TABLE_PRIVILEGES | Table privileges |
| COLUMN_PRIVILEGES | Column privileges |
| PROCESSLIST | Active threads |
| KEYWORDS | Reserved words |
| COLUMN_STATISTICS | Histogram statistics |
| FILES | Tablespace files |
| RESOURCE_GROUPS | Resource groups |
| INNODB_* | InnoDB-specific tables (~20 tables) |

### mysql Database Tables (~31 tables)

| Table | Description |
|-------|-------------|
| user | User accounts and global privileges |
| db | Database-level privileges |
| tables_priv | Table-level privileges |
| columns_priv | Column-level privileges |
| procs_priv | Stored routine privileges |
| proxies_priv | Proxy user privileges |
| global_grants | Dynamic privileges |
| role_edges | Role relationships |
| default_roles | Default role assignments |
| password_history | Password history |
| func | User-defined functions |
| plugin | Server plugins |
| servers | Federated server definitions |
| time_zone* | Time zone tables (5 tables) |
| general_log | General query log |
| slow_log | Slow query log |
| help_* | Help system tables (4 tables) |

### performance_schema Tables (~100+ tables)

Key categories:
- **Setup tables**: setup_actors, setup_instruments, setup_consumers, setup_objects
- **Instance tables**: mutex_instances, rwlock_instances, socket_instances
- **Wait event tables**: events_waits_current, events_waits_history
- **Statement event tables**: events_statements_current, events_statements_history
- **Transaction event tables**: events_transactions_current, events_transactions_history
- **Connection tables**: accounts, hosts, users, threads
- **Summary tables**: events_*_summary_by_*, file_summary_*, memory_summary_*
- **Replication tables**: replication_connection_status, replication_applier_status

Note: performance_schema and PROCESSLIST monitoring tables must map to ScratchBird
native monitoring views defined in `operations/MONITORING_SQL_VIEWS.md`.
Column-level mappings follow `operations/MONITORING_DIALECT_MAPPINGS.md`.

---

## Implementation Phases

### Phase 1: MySQL Lexer (Est. 20-25 hours)
1. Define all MySQL token types (~80 types)
2. Implement keyword recognition (~679 keywords, ~262 reserved)
3. Implement MySQL-specific literals (hex strings, bit values)
4. Implement MySQL operators including `<=>`, `DIV`, `:=`, `->`, `->>`
5. Handle backtick-quoted identifiers
6. Unit tests for lexer

### Phase 2: MySQL Parser Core (Est. 40-50 hours)
1. Statement dispatch (DDL, DML, DCL, Transaction)
2. Expression parsing (operators, functions, CASE, CAST)
3. Type parsing (MySQL type names and modifiers)
4. Column/table reference parsing with backtick quoting
5. Unit tests for each statement type

### Phase 3: MySQL Parser DDL (Est. 30-40 hours)
1. CREATE/ALTER/DROP DATABASE with CHARACTER SET and COLLATE
2. CREATE/ALTER/DROP TABLE with all MySQL options
3. CREATE/DROP INDEX including FULLTEXT and SPATIAL
4. CREATE/ALTER/DROP VIEW with DEFINER and CHECK OPTION
5. CREATE/ALTER/DROP PROCEDURE/FUNCTION
6. CREATE/DROP TRIGGER with timing and order
7. CREATE/DROP EVENT

### Phase 4: MySQL Parser DML (Est. 30-40 hours)
1. SELECT with all MySQL extensions (hints, modifiers, LIMIT/OFFSET)
2. INSERT with ON DUPLICATE KEY UPDATE
3. UPDATE (single and multi-table)
4. DELETE (single and multi-table)
5. REPLACE statement
6. JOINs including STRAIGHT_JOIN and OJ syntax
7. WITH clause (CTEs) including RECURSIVE
8. Window functions with OVER clause

### Phase 5: MySQL Parser Stored Programs (Est. 35-45 hours)
1. BEGIN...END compound statements
2. DECLARE (variables, conditions, handlers, cursors)
3. Control flow (IF, CASE, LOOP, WHILE, REPEAT)
4. Cursor operations (OPEN, FETCH, CLOSE)
5. SIGNAL and RESIGNAL for errors
6. ITERATE and LEAVE

### Phase 6: MySQL Catalog Handler (Est. 35-45 hours)
1. Create `MySQLCatalogHandler` class
2. Implement INFORMATION_SCHEMA views (~37 tables)
3. Implement mysql.* system table views (~31 tables)
4. Implement performance_schema views (key tables, ~20-30)
5. Register handler in virtual_catalog.cpp
6. Unit tests for each system table

### Phase 7: Integration & Testing (Est. 20-30 hours)
1. Connect parser to SBLR bytecode generator
2. Verify bytecode generation for all statement types
3. End-to-end tests with MySQL syntax
4. Test system table queries
5. Test with MySQL clients (mysql CLI, MySQL Workbench)
6. Performance benchmarking

---

## File Organization

### New Files to Create

```
include/scratchbird/parser/mysql/
├── mysql_lexer.h          # MySQL lexer interface
├── mysql_parser.h         # MySQL parser interface
├── mysql_token.h          # MySQL token types
└── mysql_codegen.h        # MySQL -> SBLR generator

src/parser/mysql/
├── mysql_lexer.cpp
├── mysql_parser.cpp
├── mysql_parser_ddl.cpp   # DDL parsing
├── mysql_parser_dml.cpp   # DML parsing
├── mysql_parser_sp.cpp    # Stored program parsing
└── mysql_codegen.cpp      # SBLR generation

include/scratchbird/catalog/
└── mysql_catalog.h

src/catalog/
└── mysql_catalog.cpp

tests/unit/
├── test_mysql_lexer.cpp
├── test_mysql_parser_ddl.cpp
├── test_mysql_parser_dml.cpp
├── test_mysql_parser_sp.cpp
└── test_mysql_catalog.cpp
```

### Files to Modify

```
src/catalog/virtual_catalog.cpp     # Register MySQLCatalogHandler
include/scratchbird/catalog/virtual_catalog.h
tests/CMakeLists.txt                # Add new test files
src/CMakeLists.txt                  # Add new source files
```

---

## Total Estimated Effort

| Phase | Hours |
|-------|-------|
| Phase 1: Lexer | 20-25 |
| Phase 2: Parser Core | 40-50 |
| Phase 3: Parser DDL | 30-40 |
| Phase 4: Parser DML | 30-40 |
| Phase 5: Stored Programs | 35-45 |
| Phase 6: Catalog Handler | 35-45 |
| Phase 7: Integration | 20-30 |
| **Total** | **210-275 hours** |

---

## Success Criteria

1. MySQL SQL syntax parses correctly (DDL, DML, stored programs)
2. All INFORMATION_SCHEMA tables return correct data
3. All mysql.* system tables return correct data
4. Key performance_schema tables return useful data
5. Existing ScratchBird tests continue to pass
6. MySQL CLI (`mysql`) can connect and query
7. MySQL Workbench can connect and browse
8. Performance comparable to ScratchBird V2 parser

---

## References

- [MySQL 8.0 Reference Manual](https://dev.mysql.com/doc/refman/8.0/en/)
- [MySQL 8.0 Keywords](https://dev.mysql.com/doc/refman/8.0/en/keywords.html)
- [MySQL 8.0 Data Types](https://dev.mysql.com/doc/refman/8.0/en/data-types.html)
- [MySQL 8.0 Functions](https://dev.mysql.com/doc/refman/8.0/en/functions.html)
- [MySQL 8.0 SQL Statements](https://dev.mysql.com/doc/refman/8.0/en/sql-statements.html)
- [INFORMATION_SCHEMA Tables](https://dev.mysql.com/doc/refman/8.0/en/information-schema.html)
- [Performance Schema Tables](https://dev.mysql.com/doc/refman/8.0/en/performance-schema.html)
- MySQL Emulation Protocol Behavior: `docs/specifications/wire_protocols/MYSQL_EMULATION_BEHAVIOR.md`
- SBLR Opcodes: `include/scratchbird/sblr/opcodes.h`
- Schema Architecture: `docs/specifications/SCHEMA_ARCHITECTURE.md`
- Emulated Parser Spec: `docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
