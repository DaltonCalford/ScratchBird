# Specification: SBLR Utility Opcodes

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | sblr |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcodes.generated.h:159-214`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:721-726`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:977-1000`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_v3_container.cpp`

## Synopsis

This specification defines the Utility opcodes for SBLR v3. These opcodes handle administrative operations including EXPLAIN, ANALYZE, COPY, connection management, session variables, SHOW commands, and database maintenance operations.

## Scope

### In Scope

- Query analysis (EXPLAIN, ANALYZE)
- Bulk data operations (COPY)
- Connection management (CONNECT, DISCONNECT)
- Session control (SET, SHOW, RESET)
- Database maintenance (VACUUM, SWEEP)
- Metadata introspection
- MySQL-specific utility commands

### Out of Scope

- DML operations (see opcodes_dml.md)
- DDL operations (see opcodes_ddl.md)
- Transaction control (see opcodes_system.md)

## Background

Utility opcodes provide administrative and diagnostic capabilities for the database engine. They are typically used for database administration, performance tuning, and session management.

## Specification

### Utility Opcode Overview

| Range | Category |
|-------|----------|
| 0x0501-0x0504 | Query analysis |
| 0x0505 | Comments |
| 0x0507-0x050C | Connection management |
| 0x050D-0x0513 | Statement execution |
| 0x050E-0x0511 | MySQL compatibility |
| 0x0514-0x051E | Session variables |
| 0x0520-0x0564 | SHOW commands |
| 0x0318 | SWEEP maintenance |

---

### Query Analysis Opcodes

#### SBLR3_EXPLAIN_PLAN (0x0501)

```cpp
// Source: src/sblr/executor.cpp:722
void executeExplainPlan();
```

**Purpose**: Generate query execution plan.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| query_len | uint32_t | Query bytecode length |
| query | bytes | Query to analyze |
| format | uint8_t | 1=TEXT, 2=JSON, 3=XML, 4=YAML |
| analyze | uint8_t | 0=plan only, 1=execute and collect stats |
| verbose | uint8_t | 0=normal, 1=verbose |
| costs | uint8_t | Include cost estimates |
| buffers | uint8_t | Include buffer stats |
| timing | uint8_t | Include actual timing |
| summary | uint8_t | Include summary row |

**Output Columns:**
| Column | Type | Description |
|--------|------|-------------|
| QUERY PLAN | TEXT | Formatted plan output |

**Plan Format (TEXT):**
```
Seq Scan on users  (cost=0.00..35.50 rows=2550 width=4)
  Filter: (age > 18)
```

**Plan Format (JSON):**
```json
{
  "Plan": {
    "Node Type": "Seq Scan",
    "Relation Name": "users",
    "Startup Cost": 0.00,
    "Total Cost": 35.50,
    "Plan Rows": 2550,
    "Plan Width": 4,
    "Filter": "(age > 18)"
  }
}
```

---

#### SBLR3_ANALYZE (0x0503)

```cpp
// Source: src/sblr/executor.cpp:721
void executeAnalyze();
```

**Purpose**: Collect table and index statistics.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| target_type | uint8_t | 1=TABLE, 2=INDEX, 3=DATABASE, 4=ALL |
| target_id | UUID | Target object ID |
| target_name_len | uint16_t | Name length (if ID not known) |
| target_name | char[] | Qualified name |
| columns_present | uint8_t | 0=all, 1=specific columns |
| column_count | uint16_t | Columns to analyze |
| columns | uint16_t[] | Column ordinals |
| sample_percentage | double | Sample rate (0=auto) |
| verbose | uint8_t | 0=quiet, 1=verbose output |
| skip_locked | uint8_t | 0=wait, 1=skip locked tables |

**Statistics Collected:**
| Statistic | Description |
|-----------|-------------|
| reltuples | Estimated row count |
| relpages | Total pages |
| relallvisible | All-visible pages |
| avg_row_width | Average row width |
| null_frac | Fraction of NULL values |
| n_distinct | Number of distinct values |
| most_common_vals | Most common values |
| most_common_freqs | Frequencies of common values |
| histogram_bounds | Value distribution bounds |
| correlation | Physical ordering correlation |

**Execution:**
```
1. Acquire sample of table pages
2. For each sampled row:
   - Collect column values
3. Compute statistics:
   - NULL fraction
   - Distinct value count
   - Most common values
   - Histogram bounds
4. Update pg_statistic catalog
5. Update reltuples/relpages in pg_class
```

---

### Data Loading Opcodes

#### SBLR3_COPY (0x0202)

```cpp
// Source: src/sblr/executor.cpp:723
void executeCopy();
```

**Purpose**: Bulk data import/export.

**See Also**: Detailed specification in opcodes_dml.md

**Quick Reference:**
| Direction | Format | Source/Target |
|-----------|--------|---------------|
| FROM | TEXT | FILE, STDIN, PROGRAM |
| TO | TEXT | FILE, STDOUT, PROGRAM |
| FROM | CSV | FILE, STDIN |
| TO | CSV | FILE, STDOUT |
| FROM | BINARY | FILE |
| TO | BINARY | FILE |

---

### Connection Management Opcodes

#### SBLR3_CONNECT (0x0507)

**Purpose**: Establish database connection.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| database_len | uint16_t | Database name length |
| database | char[] | Database name |
| user_len | uint16_t | Username length |
| user | char[] | Username |
| password_len | uint16_t | Password length |
| password | char[] | Password |
| options_len | uint16_t | Connection options length |
| options | char[] | Key=value pairs |

**Options:**
| Option | Description |
|--------|-------------|
| application_name | Client application name |
| client_encoding | Character encoding |
| search_path | Schema search path |
| timezone | Session timezone |
| datestyle | Date output format |

---

#### SBLR3_DISCONNECT (0x050C)

**Purpose**: Close database connection.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| force | uint8_t | 0=graceful, 1=force immediate |

**Execution:**
```
1. Commit any open transaction
2. Release session resources
3. Close connection handle
4. Clean up temporary objects
```

---

#### SBLR3_EXECUTE_STMT (0x050D)

**Purpose**: Execute a prepared statement.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| stmt_name_len | uint16_t | Statement name length |
| stmt_name | char[] | Prepared statement identifier |
| param_count | uint16_t | Number of parameters |
| params | Value[] | Parameter values |

---

#### SBLR3_PREPARE_STMT (0x0512)

**Purpose**: Prepare a statement for execution.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| stmt_name_len | uint16_t | Statement name length |
| stmt_name | char[] | Prepared statement identifier |
| query_len | uint32_t | Query text length |
| query | char[] | SQL query text |
| param_types_count | uint16_t | Number of parameter type hints |
| param_types | uint32_t[] | OIDs of parameter types |

---

### Session Variable Opcodes

#### SBLR3_SET_VARIABLE (0x051E)

**Purpose**: Set session variable value.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Variable name length |
| name | char[] | Variable name |
| scope | uint8_t | 1=SESSION, 2=LOCAL, 3=TRANSACTION |
| value_is_expression | uint8_t | 0=literal, 1=expression |
| value_type | uint16_t | Value type code |
| value_len | uint32_t | Value length |
| value | bytes | Value data |

**Common Variables:**
| Variable | Description |
|----------|-------------|
| search_path | Schema search order |
| datestyle | Date display format |
| timezone | Session timezone |
| client_encoding | Character encoding |
| application_name | Client identifier |
| statement_timeout | Query timeout (ms) |
| lock_timeout | Lock wait timeout (ms) |
| idle_in_transaction_session_timeout | Idle timeout |

---

#### SBLR3_SET_NAMES (0x051A)

**Purpose**: Set client character encoding.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| encoding_len | uint16_t | Encoding name length |
| encoding | char[] | Encoding name (e.g., 'UTF8') |

---

#### SBLR3_SET_SQL_DIALECT (0x051C)

**Purpose**: Set SQL dialect for parsing.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| dialect_len | uint16_t | Dialect name length |
| dialect | char[] | Dialect (PostgreSQL, MySQL, Firebird) |

---

#### SBLR3_SET_LOCAL_TIMEOUT (0x0518)

**Purpose**: Set statement timeout.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| timeout_ms | uint32_t | Timeout in milliseconds (0=none) |

---

#### SBLR3_SET_BIT (0x0514) / SBLR3_SET_BYTE (0x0516)

**Purpose**: Set specific bit/byte in binary data.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| target_len | uint16_t | Target variable name length |
| target | char[] | Variable name |
| offset | uint32_t | Bit/byte offset |
| value | uint8_t | New value |

---

### SHOW Command Opcodes (0x0520-0x0564)

#### SHOW Command Overview

| Opcode | Hex | Description |
|--------|-----|-------------|
| SBLR3_SHOW_ALL | 0x0520 | Show all settings |
| SBLR3_SHOW_VARIABLE | 0x0560 | Show specific variable |
| SBLR3_SHOW_VERSION | 0x0562 | Show server version |
| SBLR3_SHOW_DATABASES | 0x052E | List databases |
| SBLR3_SHOW_DATABASE | 0x052C | Show current database |
| SBLR3_SHOW_TABLES | 0x055C | List tables |
| SBLR3_SHOW_TABLE | 0x055A | Show table info |
| SBLR3_SHOW_COLUMNS | 0x0526 | List columns |
| SBLR3_SHOW_INDEXES | 0x053C | List indexes |
| SBLR3_SHOW_INDEX | 0x053A | Show index info |
| SBLR3_SHOW_SCHEMA | 0x054E | Show schema info |
| SBLR3_SHOW_SCHEMAS | 0x054E | List schemas |
| SBLR3_SHOW_VIEWS | 0x0564 | List views |
| SBLR3_SHOW_FUNCTIONS | 0x0534 | List functions |
| SBLR3_SHOW_PROCEDURES | 0x0548 | List procedures |
| SBLR3_SHOW_TRIGGERS | 0x055E | List triggers |
| SBLR3_SHOW_GRANTS | 0x0538 | Show privileges |
| SBLR3_SHOW_COLLATIONS | 0x0524 | List collations |
| SBLR3_SHOW_CREATE_TABLE | 0x052A | Show CREATE TABLE |
| SBLR3_SHOW_TRANSACTION_LEVEL | 0x0314 | Show isolation level |
| SBLR3_SHOW_SEARCH_PATH | 0x0554 | Show search path |
| SBLR3_SHOW_SQL_DIALECT | 0x0556 | Show current dialect |

---

#### SBLR3_SHOW_ALL (0x0520)

**Purpose**: Display all session configuration parameters.

**Output Columns:**
| Column | Type | Description |
|--------|------|-------------|
| name | VARCHAR | Parameter name |
| setting | TEXT | Current value |
| unit | VARCHAR | Unit of measurement |
| category | VARCHAR | Parameter category |
| short_desc | TEXT | Brief description |
| extra_desc | TEXT | Additional details |
| context | VARCHAR | Set context |
| vartype | VARCHAR | Value type |
| source | VARCHAR | Source of setting |
| min_val | TEXT | Minimum value |
| max_val | TEXT | Maximum value |

---

#### SBLR3_SHOW_TABLES (0x055C)

**Purpose**: List tables in schema.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| schema_len | uint16_t | Schema name length (0=current) |
| schema | char[] | Schema name |
| pattern_len | uint16_t | Filter pattern length |
| pattern | char[] | LIKE pattern for filtering |
| full | uint8_t | 0=names only, 1=with types |

**Output Columns (full=0):**
| Column | Type |
|--------|------|
| table_name | VARCHAR |

**Output Columns (full=1):**
| Column | Type | Description |
|--------|------|-------------|
| table_name | VARCHAR | Table name |
| table_type | VARCHAR | BASE TABLE, VIEW, etc. |
| owner | VARCHAR | Table owner |

---

#### SBLR3_SHOW_COLUMNS (0x0526)

**Purpose**: Show column information for table.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_len | uint16_t | Table name length |
| table | char[] | Qualified table name |
| schema_len | uint16_t | Schema override |
| schema | char[] | Schema name |

**Output Columns:**
| Column | Type | Description |
|--------|------|-------------|
| column_name | VARCHAR | Column name |
| data_type | VARCHAR | Data type |
| is_nullable | VARCHAR | YES/NO |
| column_default | TEXT | Default expression |
| character_maximum_length | INT | For char types |
| numeric_precision | INT | For numeric types |
| numeric_scale | INT | Decimal places |
| ordinal_position | INT | Column order |

---

#### SBLR3_SHOW_INDEXES (0x053C)

**Purpose**: List indexes for table.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_len | uint16_t | Table name length |
| table | char[] | Qualified table name |
| extended | uint8_t | 0=basic, 1=extended info |

**Output Columns:**
| Column | Type | Description |
|--------|------|-------------|
| index_name | VARCHAR | Index name |
| index_type | VARCHAR | BTREE, HASH, etc. |
| unique | BOOLEAN | Is unique? |
| column_name | VARCHAR | Indexed column |
| direction | VARCHAR | ASC/DESC |

---

#### SBLR3_SHOW_CREATE_TABLE (0x052A)

**Purpose**: Generate CREATE TABLE statement.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_len | uint16_t | Table name length |
| table | char[] | Qualified table name |
| pretty | uint8_t | 0=compact, 1=formatted |

**Output Columns:**
| Column | Type |
|--------|------|
| table | VARCHAR |
| create_statement | TEXT |

---

### Database Maintenance Opcodes

#### SBLR3_SWEEP (0x0318)

```cpp
// Source: src/sblr/executor.cpp:726
void executeSweep();
```

**Purpose**: Perform database sweep (garbage collection).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| target_type | uint8_t | 1=DATABASE, 2=TABLE |
| target_id | UUID | Target object |
| target_name_len | uint16_t | Name length |
| target_name | char[] | Qualified name |
| options | uint32_t | Sweep options bitmask |

**Sweep Options:**
| Bit | Option | Description |
|-----|--------|-------------|
| 0 | CHECK_INTEGRITY | Verify page structure |
| 1 | RECLAIM_SPACE | Compact and free pages |
| 2 | UPDATE_STATISTICS | Refresh table stats |
| 3 | CLEANUP_VERSIONS | Remove old record versions |

**Execution:**
```
1. Scan all data pages
2. For each tuple version chain:
   a. Identify visible versions
   b. Mark obsolete versions as garbage
   c. Compact remaining versions
3. Update page headers
4. Add freed pages to free list
5. Update statistics
```

---

#### SBLR3_COMMENT (0x0505)

**Purpose**: Add or modify object comment.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| object_type | uint8_t | 1=TABLE, 2=COLUMN, 3=INDEX, etc. |
| object_id | UUID | Object UUID |
| object_name_len | uint16_t | Name length (if ID not known) |
| object_name | char[] | Qualified name |
| column_name_len | uint16_t | For column comments |
| column_name | char[] | Column name |
| comment_len | uint16_t | Comment text length |
| comment | char[] | Comment text (empty to remove) |

---

### Debug Opcodes

#### SBLR3_DEBUG_SPAN (0x0508)

**Purpose**: Mark execution span for debugging/tracing.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| span_name_len | uint16_t | Span name length |
| span_name | char[] | Span identifier |
| span_id | uint64_t | Unique span ID |
| parent_id | uint64_t | Parent span ID (0=root) |
| attributes_len | uint16_t | JSON attributes length |
| attributes | char[] | Key-value pairs (JSON) |
| operation | uint8_t | 1=START, 2=END, 3=EVENT |

---

### MySQL Compatibility Opcodes

#### SBLR3_MYSQL_FLUSH (0x050E)

**Purpose**: Flush tables/logs (MySQL compatibility).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| option_len | uint16_t | Flush option length |
| option | char[] | TABLES, LOGS, STATUS, etc. |
| table_count | uint16_t | Specific tables (0=all) |
| tables | char[][] | Table names |
| with_read_lock | uint8_t | Acquire read lock |

---

#### SBLR3_MYSQL_LOCK_TABLES (0x0510)

**Purpose**: Acquire table locks (MySQL compatibility).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_count | uint16_t | Number of tables |
| locks | TableLock[] | Lock specifications |

**TableLock Structure:**
| Field | Type | Description |
|-------|------|-------------|
| table_len | uint16_t | Table name length |
| table | char[] | Table name |
| lock_type | uint8_t | 1=READ, 2=WRITE, 3=READ_LOCAL |
| alias_len | uint16_t | Alias length |
| alias | char[] | Table alias |

---

#### SBLR3_MYSQL_UNLOCK_TABLES (0x0511)

**Purpose**: Release all table locks.

**Payload:** None

**Execution:**
```
1. Release all locks held by session
2. Clear lock wait queue entries
3. Wake waiters if any
```

---

#### SBLR3_MYSQL_KILL (0x050F)

**Purpose**: Terminate a connection/query (MySQL compatibility).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| process_id | uint32_t | Connection ID to kill |
| kill_type | uint8_t | 1=CONNECTION, 2=QUERY |

---

### Descriptor Opcodes

#### SBLR3_DESCRIBE_TABLE (0x050A)

**Purpose**: Describe table structure (similar to SHOW COLUMNS).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_len | uint16_t | Table name length |
| table | char[] | Qualified table name |
| extended | uint8_t | 0=basic, 1=extended info |

---

### List Management Opcodes

#### SBLR3_BEGIN_LIST (0x0602) / SBLR3_END_LIST (0x0605)

**Purpose**: Mark list boundaries in bytecode.

**Usage:**
```
BEGIN_LIST
  element1
  element2
  ...
END_LIST
```

---

#### SBLR3_ASSIGNMENT (0x0601)

**Purpose**: Variable assignment marker.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| target_len | uint16_t | Target variable name length |
| target | char[] | Variable name |

---

#### SBLR3_COLUMN_DEF (0x0603)

**Purpose**: Column definition marker.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Column name length |
| name | char[] | Column name |
| type | uint16_t | Data type code |
| modifiers_present | uint8_t | Type modifiers follow |

---

### Scan Hint Opcode

#### SBLR3_SCAN_HINT (0x0657)

**Purpose**: Provide optimizer hints for table access.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| hint_type | uint8_t | 1=INDEX, 2=SEQSCAN, 3=ORDERED |
| index_name_len | uint16_t | Index to use (if INDEX hint) |
| index_name | char[] | Index name |

---

### Invariants

1. **Session Isolation**: Session variable changes affect only current session
2. **Visibility**: SHOW commands respect user privileges
3. **Consistency**: Maintenance operations maintain catalog consistency

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `E_UNDEFINED_OBJECT` | SHOW/DESCRIBE on non-existent object | Verify object name |
| `E_INSUFFICIENT_PRIVILEGE` | User lacks required permissions | Grant appropriate privileges |
| `E_LOCK_NOT_AVAILABLE` | Cannot acquire required lock | Retry or adjust timeout |

## Related Specifications

- [opcodes_system.md](./opcodes_system.md) - System control opcodes
- [opcodes_ddl.md](./opcodes_ddl.md) - DDL operations

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
