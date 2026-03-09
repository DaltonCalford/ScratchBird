# Specification: SBLR DML Opcodes

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcodes.generated.h:106-117`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:713-720`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:660-686`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_v3_container.cpp`

## Synopsis

This specification defines the Data Manipulation Language (DML) opcodes for SBLR v3. These opcodes handle data modification operations including INSERT, UPDATE, DELETE, and the MERGE statement with conflict resolution and RETURNING clause support.

## Scope

### In Scope

- INSERT, UPDATE, DELETE statement execution
- MERGE statement (INSERT/UPDATE/DELETE combination)
- Conflict resolution (ON CONFLICT/UPSERT)
- RETURNING clause for data retrieval
- Batch/bulk operations

### Out of Scope

- Query execution (SELECT) - see opcodes_query.md
- DDL operations - see opcodes_ddl.md
- Cursor operations - see opcodes_psql.md

## Background

DML operations modify table data while maintaining ACID properties. The SBLR executor processes DML opcodes by interacting with the storage engine to read, modify, and write tuple data. Index maintenance is performed automatically for affected indexes.

## Specification

### DML Opcode Overview

| Opcode | Hex | Description |
|--------|-----|-------------|
| SBLR3_INSERT | 0x0211 | Insert rows into table |
| SBLR3_UPDATE | 0x0213 | Update existing rows |
| SBLR3_DELETE | 0x0201 | Delete rows from table |
| SBLR3_COPY | 0x0202 | Bulk load/unload data |
| SBLR3_MERGE_START | 0x020A | Begin MERGE statement |
| SBLR3_MERGE_END | 0x0204 | End MERGE statement |
| SBLR3_MERGE_SOURCE | 0x0208 | Define MERGE source |
| SBLR3_MERGE_ON | 0x0206 | Define MERGE join condition |
| SBLR3_MERGE_WHEN_MATCHED | 0x020C | MATCHED clause handler |
| SBLR3_MERGE_WHEN_NOT_MATCHED | 0x020E | NOT MATCHED clause handler |
| SBLR3_MERGE_WHEN_NOT_MATCHED_SOURCE | 0x0210 | NOT MATCHED BY SOURCE clause |
| SBLR3_ON_CONFLICT | 0x061F | UPSERT conflict handler |
| SBLR3_ON_CONFLICT_DO_UPDATE | 0x0627 | Conflict resolution: UPDATE |
| SBLR3_ON_CONFLICT_DO_NOTHING | 0x0625 | Conflict resolution: skip |
| SBLR3_RETURNING | 0x062B | Return modified rows |

---

### SBLR3_INSERT (0x0211)

```cpp
// Source: src/sblr/executor.cpp:713
void executeInsert();

// Source: src/sblr/executor.cpp:660-668
void updateIndexesOnInsert(uint64_t xid,
                          const ID& table_id,
                          const TableInfo& table_info,
                          const std::vector<ColumnInfo>& all_columns,
                          uint32_t page_id,
                          uint16_t item_id,
                          const std::vector<Value>& row_values);
```

**Purpose**: Insert one or more rows into a table.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_id | UUID | Target table UUID |
| table_name_len | uint16_t | Table name length (if ID not known) |
| table_name | char[] | Qualified table name |
| column_count | uint16_t | Number of target columns (0=all) |
| columns | uint16_t[] | Column ordinals |
| values_mode | uint8_t | 1=VALUES, 2=SELECT, 3=DEFAULT |
| values_count | uint32_t | Number of value sets |
| values_data | bytes | Encoded values or subquery bytecode |
| on_conflict_present | uint8_t | 0=none, 1=present |
| on_conflict_data | bytes | ON CONFLICT clause bytecode |
| returning_present | uint8_t | 0=none, 1=present |
| returning_data | bytes | RETURNING clause bytecode |

**Values Encoding (VALUES mode):**
```
For each row:
  uint16_t: expression count
  For each expression:
    bytes: SBLR expression bytecode
```

**Execution Semantics:**

```
Input: Insert payload with table, columns, values
Output: Rows affected count, optional RETURNING result set

1. Resolve table reference
2. Validate column references
3. For each value set:
   a. Evaluate expressions to compute row values
   b. Apply DEFAULT values where needed
   c. Validate NOT NULL constraints
   d. Check type compatibility
   e. Generate identity/sequence values
   f. Insert tuple into storage
   g. Update all indexes
   h. Handle ON CONFLICT if specified
4. If RETURNING: collect and return specified values
5. Return count of rows inserted
```

**Index Maintenance:**
- B-tree indexes: Insert key + TID
- Hash indexes: Compute hash, insert to bucket
- GIN indexes: Extract keys, insert to posting lists
- Unique indexes: Check for conflicts

---

### SBLR3_UPDATE (0x0213)

```cpp
// Source: src/sblr/executor.cpp:718
void executeUpdate();

// Source: src/sblr/executor.cpp:670-678
void updateIndexesOnUpdate(uint64_t xid,
                          const ID& table_id,
                          const TableInfo& table_info,
                          const std::vector<ColumnInfo>& all_columns,
                          const std::vector<Value>& old_values,
                          const std::vector<Value>& new_values,
                          TID old_tid,
                          TID new_tid);
```

**Purpose**: Modify existing rows in a table.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_id | UUID | Target table UUID |
| table_name_len | uint16_t | Table name length |
| table_name | char[] | Qualified table name |
| set_clause_count | uint16_t | Number of SET assignments |
| set_clauses | SetClause[] | Column = expression pairs |
| where_present | uint8_t | 0=no WHERE, 1=WHERE clause |
| where_data | bytes | WHERE expression bytecode |
| from_present | uint8_t | 0=no FROM, 1=FROM clause |
| from_data | bytes | FROM clause bytecode |
| returning_present | uint8_t | 0=none, 1=present |
| returning_data | bytes | RETURNING clause bytecode |

**SetClause Structure:**
| Field | Type | Description |
|-------|------|-------------|
| column_id | uint16_t | Target column ordinal |
| expression_len | uint32_t | Expression bytecode length |
| expression | bytes | Value expression bytecode |

**Execution Semantics:**

```
Input: Update payload with table, SET clauses, optional WHERE
Output: Rows affected count, optional RETURNING result set

1. Resolve table reference
2. Acquire appropriate lock (row-level or table-level)
3. Scan table or use index:
   a. If WHERE: evaluate condition for each row
   b. Else: process all rows
4. For each matching row:
   a. Store old values
   b. Evaluate SET expressions
   c. Compute new row values
   d. Validate constraints
   e. Check for conflicts with concurrent updates
   f. Write new tuple version
   g. Update indexes (delete old keys, insert new)
   h. Record in transaction log
   i. Add to RETURNING set if specified
5. Release locks
6. Return affected row count
```

**HOT (Heap-Only Tuple) Optimization:**
```
If update does not modify indexed columns:
  - Create heap-only tuple chain
  - Avoid index updates
  - Update line pointer
Else:
  - Full index maintenance required
```

---

### SBLR3_DELETE (0x0201)

```cpp
// Source: src/sblr/executor.cpp:719
void executeDelete();

// Source: src/sblr/executor.cpp:680-686
void updateIndexesOnDelete(uint64_t xid,
                          const ID& table_id,
                          const TableInfo& table_info,
                          const std::vector<ColumnInfo>& all_columns,
                          const std::vector<Value>& row_values,
                          TID tid);
```

**Purpose**: Remove rows from a table.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_id | UUID | Target table UUID |
| table_name_len | uint16_t | Table name length |
| table_name | char[] | Qualified table name |
| where_present | uint8_t | 0=no WHERE, 1=WHERE clause |
| where_data | bytes | WHERE expression bytecode |
| using_present | uint8_t | 0=no USING, 1=USING clause |
| using_data | bytes | USING clause bytecode |
| returning_present | uint8_t | 0=none, 1=present |
| returning_data | bytes | RETURNING clause bytecode |
| cursor_name_len | uint16_t | For DELETE FROM cursor |
| cursor_name | char[] | Cursor identifier |

**Execution Semantics:**

```
Input: Delete payload with table, optional WHERE
Output: Rows affected count, optional RETURNING result set

1. Resolve table reference
2. Acquire appropriate lock
3. If USING: establish joined table context
4. Scan table or use index:
   a. Evaluate WHERE condition
   b. Or use cursor position
5. For each matching row:
   a. Store row values for RETURNING
   b. Mark tuple as deleted (set xmax)
   c. Remove from all indexes
   d. Record in transaction log
   e. Add to RETURNING set if specified
6. Schedule garbage collection
7. Release locks
8. Return affected row count
```

---

### SBLR3_COPY (0x0202)

```cpp
// Source: src/sblr/executor.cpp:723
void executeCopy();
```

**Purpose**: Bulk data load or unload.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_id | UUID | Target table UUID |
| table_name_len | uint16_t | Table name length |
| table_name | char[] | Qualified table name |
| direction | uint8_t | 0=FROM (import), 1=TO (export) |
| source_type | uint8_t | 0=FILE, 1=STDIN/STDOUT, 2=PROGRAM |
| source_len | uint16_t | Source path/command length |
| source | char[] | File path or command |
| format | uint8_t | 0=TEXT, 1=CSV, 2=BINARY |
| delimiter | char | Field delimiter |
| quote | char | Quote character |
| escape | char | Escape character |
| null_marker_len | uint16_t | NULL representation length |
| null_marker | char[] | NULL string |
| header | uint8_t | 0=no, 1=yes (CSV header) |
| encoding_len | uint16_t | Character encoding length |
| encoding | char[] | Encoding name |
| column_count | uint16_t | Columns to copy (0=all) |
| columns | uint16_t[] | Column ordinals |

**Execution Semantics:**

**COPY FROM:**
```
1. Open source (file, stdin, or program output)
2. Parse rows according to format
3. For each row:
   a. Parse fields
   b. Convert to column types
   c. Insert into table
   d. Update indexes
4. Handle errors according to settings
5. Return rows imported count
```

**COPY TO:**
```
1. Open destination
2. If format=CSV and header=1: write column names
3. Scan table (or execute query)
4. For each row:
   a. Format columns
   b. Write to output
5. Return rows exported count
```

---

### MERGE Statement Opcodes

#### SBLR3_MERGE_START (0x020A)

**Purpose**: Begin MERGE statement execution.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| target_table_id | UUID | Target table UUID |
| target_table_len | uint16_t | Target name length |
| target_table | char[] | Target table name |
| alias_len | uint16_t | Target alias length |
| alias | char[] | Target alias |

#### SBLR3_MERGE_SOURCE (0x0208)

**Purpose**: Define source data for MERGE.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| source_type | uint8_t | 0=TABLE, 1=QUERY, 2=VALUES |
| source_id | UUID | Source table UUID (if TABLE) |
| source_len | uint32_t | Source definition length |
| source_data | bytes | Query or VALUES bytecode |
| alias_len | uint16_t | Source alias length |
| alias | char[] | Source alias |

#### SBLR3_MERGE_ON (0x0206)

**Purpose**: Define merge join condition.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| condition_len | uint32_t | Expression bytecode length |
| condition | bytes | Boolean expression bytecode |

#### SBLR3_MERGE_WHEN_MATCHED (0x020C)

**Purpose**: Handle matched rows.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| action | uint8_t | 1=UPDATE, 2=DELETE, 3=DO NOTHING |
| and_condition_present | uint8_t | Additional filter |
| and_condition_len | uint32_t | Condition bytecode length |
| and_condition | bytes | Additional boolean expression |
| update_set_count | uint16_t | Number of SET clauses (if UPDATE) |
| update_sets | SetClause[] | UPDATE SET clauses |

#### SBLR3_MERGE_WHEN_NOT_MATCHED (0x020E)

**Purpose**: Handle unmatched target rows (INSERT).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| action | uint8_t | 1=INSERT, 2=DO NOTHING |
| and_condition_present | uint8_t | Additional filter |
| column_count | uint16_t | Target columns for INSERT |
| columns | uint16_t[] | Column ordinals |
| values_count | uint16_t | Number of value expressions |
| values_expr | bytes[] | Value expressions bytecode |

#### SBLR3_MERGE_WHEN_NOT_MATCHED_SOURCE (0x0210)

**Purpose**: Handle unmatched source rows (DELETE from target).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| action | uint8_t | 1=DELETE, 2=DO NOTHING, 3=UPDATE |
| and_condition_present | uint8_t | Additional filter |

#### SBLR3_MERGE_END (0x0204)

**Purpose**: Complete MERGE statement.

**Payload:** None (end marker)

**MERGE Execution Flow:**

```
MERGE_START
  └── Initialize target table context
MERGE_SOURCE
  └── Execute source query, materialize results
MERGE_ON
  └── Build hash join on condition
  └── Or: nested loop join
MERGE_WHEN_MATCHED / MERGE_WHEN_NOT_MATCHED / MERGE_WHEN_NOT_MATCHED_SOURCE
  └── For each joined row:
       ├── If MATCHED: execute UPDATE or DELETE
       ├── If NOT MATCHED: execute INSERT
       └── If NOT MATCHED BY SOURCE: optional DELETE
MERGE_END
  └── Finalize, return affected counts
```

---

### Conflict Resolution Opcodes

#### SBLR3_ON_CONFLICT (0x061F)

**Purpose**: Begin UPSERT conflict specification.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| target_type | uint8_t | 0=none, 1=COLUMN, 2=CONSTRAINT |

#### SBLR3_ON_CONFLICT_COLUMN (0x0621)

**Purpose**: Specify conflict target columns.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| column_count | uint16_t | Number of columns |
| columns | uint16_t[] | Column ordinals |
| collation_present | uint8_t | 0=no, 1=yes |
| collation_id | uint32_t | Collation for comparison |
| opclass_present | uint8_t | 0=no, 1=yes |
| opclass_len | uint16_t | Operator class name length |
| opclass | char[] | Operator class name |

#### SBLR3_ON_CONFLICT_CONSTRAINT (0x0623)

**Purpose**: Specify conflict target by constraint name.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| constraint_name_len | uint16_t | Constraint name length |
| constraint_name | char[] | Constraint identifier |

#### SBLR3_ON_CONFLICT_DO_NOTHING (0x0625)

**Purpose**: Skip row on conflict.

**Payload:** None

#### SBLR3_ON_CONFLICT_DO_UPDATE (0x0627)

**Purpose**: Update existing row on conflict (UPSERT).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| set_clause_count | uint16_t | Number of SET clauses |
| set_clauses | SetClause[] | Column assignments |
| where_present | uint8_t | 0=no, 1=WHERE clause |
| where_len | uint32_t | WHERE expression length |
| where_data | bytes | WHERE expression bytecode |

**UPSERT Execution Flow:**

```
For each row to insert:
  1. Attempt insert
  2. If unique constraint violation:
     a. If DO NOTHING: skip row, continue
     b. If DO UPDATE:
        i. Find conflicting row
        ii. Evaluate SET expressions (can reference EXCLUDED)
        iii. Apply WHERE condition if present
        iv. Update conflicting row
        v. Return updated row if RETURNING specified
  3. If no conflict: insert normally
```

---

### SBLR3_RETURNING (0x062B)

**Purpose**: Return values from modified rows.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| select_star | uint8_t | 0=expressions, 1=SELECT * |
| expr_count | uint16_t | Number of expressions (if not *) |
| expressions | bytes[] | Expression bytecode array |
| alias_count | uint16_t | Number of column aliases |
| aliases | char[][] | Output column names |

**Execution Semantics:**

```
During INSERT/UPDATE/DELETE:
  1. Create result set with RETURNING structure
  2. For each modified row:
     a. Evaluate RETURNING expressions
     b. Add row to result set
  3. Return result set alongside affected count
```

**Special References:**
- `OLD.*` or `table.*`: Values before modification
- `NEW.*` or `EXCLUDED.*`: Values after modification (INSERT/UPDATE)

### Invariants

1. **ACID Compliance**: All DML operations maintain atomicity, consistency, isolation, and durability
   - Verification: Transaction log and rollback capability

2. **Constraint Enforcement**: Constraints are checked on every modification
   - Verification: Constraint validation in execution path

3. **Index Consistency**: Indexes always reflect table contents
   - Verification: Index maintenance on insert/update/delete

4. **Visibility Rules**: MVCC visibility rules are respected
   - Verification: Tuple header xmin/xmax checks

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `E_UNIQUE_VIOLATION` | Duplicate key on unique index | Use ON CONFLICT or fix data |
| `E_FOREIGN_KEY_VIOLATION` | Referential integrity violation | Add parent row or use DEFERRABLE |
| `E_NOT_NULL_VIOLATION` | NULL in NOT NULL column | Provide non-NULL value |
| `E_CHECK_VIOLATION` | CHECK constraint failed | Satisfy constraint condition |
| `E_EXCLUSION_VIOLATION` | Exclusion constraint failed | Ensure non-overlapping values |
| `E_DEADLOCK` | Circular lock wait detected | Retry transaction |
| `E_LOCK_TIMEOUT` | Could not acquire lock | Retry or increase timeout |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_sblr_v3_container.cpp` | DML operation encoding |
| `tests/unit/test_sblr_jit_functions.cpp` | Function calls in DML |

## Related Specifications

- [opcodes_query.md](./opcodes_query.md) - SELECT and query operations
- [opcodes_expressions.md](./opcodes_expressions.md) - Expression evaluation
- [opcodes_index.md](./opcodes_index.md) - Index maintenance

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
