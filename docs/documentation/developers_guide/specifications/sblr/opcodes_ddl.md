# Specification: SBLR DDL Opcodes

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcodes.generated.h:6-105`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:623-722`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_payloads.cpp:240-260`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_v3_schema.cpp`

## Synopsis

This specification defines the Data Definition Language (DDL) opcodes for SBLR v3. These opcodes handle schema modifications including CREATE, ALTER, DROP operations for tables, indexes, views, sequences, schemas, and related database objects.

## Scope

### In Scope

- CREATE/DROP/ALTER operations for all database objects
- Table constraints (PRIMARY KEY, FOREIGN KEY, UNIQUE, CHECK)
- Column definitions and properties
- Schema and namespace management
- Tablespace operations

### Out of Scope

- Data manipulation (INSERT/UPDATE/DELETE) - see opcodes_dml.md
- Query execution (SELECT) - see opcodes_query.md
- Security/permission DDL - see opcodes_security.md

## Background

DDL operations in SBLR modify the database catalog and schema. Each DDL opcode corresponds to a SQL DDL statement and is executed atomically within a transaction. The executor maintains catalog consistency through the CatalogManager interface.

## Specification

### DDL Opcode Families

| Range | Family |
|-------|--------|
| 0x0101-0x0105 | ALTER operations |
| 0x0106-0x0110 | Tablespace and column properties |
| 0x010A-0x0117 | CREATE/DROP basic objects |
| 0x0118-0x0128 | Extended ALTER operations |
| 0x0129-0x0147 | Extended CREATE operations |
| 0x0148-0x0162 | Extended DROP operations |
| 0x0163-0x017C | Firebird-specific and constraints |

### CREATE Operations

#### SBLR3_CREATE_TABLE (0x010D)

```cpp
// Source: src/sblr/executor.cpp:623
void executeCreateTable();
```

**Purpose**: Create a new table in the database schema.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_name_len | uint16_t | Length of table name |
| table_name | char[] | Table identifier |
| schema_id | UUID | Schema UUID (16 bytes) |
| column_count | uint16_t | Number of columns |
| columns | ColumnDef[] | Column definitions |
| constraint_count | uint16_t | Number of constraints |
| constraints | Constraint[] | Table constraints |
| if_not_exists | uint8_t | 0=false, 1=true |
| temporary | uint8_t | 0=permanent, 1=temp, 2=global temp |

**ColumnDef Structure:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Column name length |
| name | char[] | Column name |
| type | uint16_t | Data type code |
| precision | uint32_t | For DECIMAL/VARCHAR |
| scale | uint32_t | For DECIMAL |
| nullable | uint8_t | 0=NOT NULL, 1=NULLable |
| default_expr_len | uint16_t | Length of default expression |
| default_expr | char[] | Default value expression |
| domain_id | UUID | Domain UUID if domain-typed |
| is_array | uint8_t | Array flag |
| array_size | uint32_t | Array dimensions |

**Execution Semantics:**
1. Validate schema exists and is writable
2. Check table name uniqueness (respecting if_not_exists)
3. Validate column definitions
4. Allocate table ID
5. Create storage structures (pages, B-tree roots)
6. Insert catalog entries
7. Build initial indexes for constraints

---

#### SBLR3_CREATE_INDEX (0x010A)

```cpp
// Source: src/sblr/executor.cpp:624
void executeCreateIndex();
void buildExpressionIndex(uint64_t xid, const TableInfo& table_info, const ID& index_id);
```

**Purpose**: Create an index on a table.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| index_name_len | uint16_t | Index name length |
| index_name | char[] | Index identifier |
| table_id | UUID | Target table UUID |
| unique | uint8_t | 0=non-unique, 1=unique |
| index_type | uint8_t | 0=BTREE, 1=HASH, 2=GIN, 3=GIST, 4=BRIN, 5=HNSW, etc. |
| column_count | uint16_t | Number of indexed columns |
| columns | IndexedColumn[] | Column references |
| expression_len | uint16_t | Expression length (0 for simple) |
| expression | char[] | Index expression (functional index) |
| predicate_len | uint16_t | Partial index predicate length |
| predicate | char[] | WHERE clause for partial index |
| params_len | uint16_t | Index-specific parameters length |
| params | char[] | Index parameters (e.g., fillfactor) |
| concurrently | uint8_t | Build without locking table |

**IndexedColumn Structure:**
| Field | Type | Description |
|-------|------|-------------|
| column_id | uint16_t | Column ordinal in table |
| collation_id | uint32_t | Collation for string columns |
| asc | uint8_t | Sort direction (1=ASC, 0=DESC) |
| nulls_first | uint8_t | NULL ordering |

**Execution Semantics:**
1. Validate table exists
2. Check index name uniqueness
3. Validate column references
4. For expression indexes: parse and validate expression
5. Allocate index ID and create storage structures
6. If `concurrently=0`: lock table, scan and populate index
7. If `concurrently=1`: use incremental build protocol
8. Update catalog with index metadata

---

#### SBLR3_CREATE_VIEW (0x010F)

```cpp
// Source: src/sblr/executor.cpp:772
void executeCreateView();
```

**Purpose**: Create a view (virtual table based on query).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| view_name_len | uint16_t | View name length |
| view_name | char[] | View identifier |
| schema_id | UUID | Schema UUID |
| query_len | uint32_t | Length of view query |
| query | char[] | SELECT statement text |
| column_names_count | uint16_t | Explicit column count (0=infer) |
| column_names | char[][] | Column aliases |
| check_option | uint8_t | 0=NONE, 1=LOCAL, 2=CASCADED |
| materialized | uint8_t | 0=regular, 1=materialized |
| recursive | uint8_t | 0=false, 1=true |

---

#### SBLR3_CREATE_SEQUENCE (0x010C)

```cpp
// Source: src/sblr/executor.cpp:764
void executeCreateSequence();
```

**Purpose**: Create a sequence (number generator).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Sequence name length |
| name | char[] | Sequence identifier |
| schema_id | UUID | Schema UUID |
| start_value | int64_t | Initial value |
| increment | int64_t | Step value |
| min_value | int64_t | Minimum value |
| max_value | int64_t | Maximum value |
| cache | int64_t | Cache size |
| cycle | uint8_t | 0=NO CYCLE, 1=CYCLE |

---

#### SBLR3_CREATE_SCHEMA (0x013D)

```cpp
// Source: src/sblr/executor.cpp:700
void executeCreateSchema();
```

**Purpose**: Create a new schema (namespace).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Schema name length |
| name | char[] | Schema identifier |
| owner_id | UUID | Owner user UUID |
| if_not_exists | uint8_t | 0=false, 1=true |
| authorization | uint8_t | 0=current_user, 1=specified owner |

---

#### SBLR3_CREATE_TABLESPACE (0x010E)

```cpp
// Source: src/sblr/executor.cpp:691
void executeCreateTablespace();
```

**Purpose**: Create a tablespace (storage location).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Tablespace name length |
| name | char[] | Tablespace identifier |
| location_len | uint16_t | Path length |
| location | char[] | Filesystem path |
| owner_id | UUID | Owner UUID |

### DROP Operations

#### SBLR3_DROP_TABLE (0x0115)

```cpp
// Source: src/sblr/executor.cpp:694
void executeDropTable();
```

**Purpose**: Remove a table and its data.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_id | UUID | Table UUID (if known) |
| name_len | uint16_t | Table name length |
| name | char[] | Table name (qualified) |
| if_exists | uint8_t | 0=error if missing, 1=silent |
| cascade | uint8_t | 0=RESTRICT, 1=CASCADE |
| temporary | uint8_t | 0=any, 1=temp only |

**Execution Semantics:**
1. Resolve table name to ID
2. If `if_exists=1` and not found: return success
3. Check for dependent objects (if RESTRICT)
4. Drop dependent objects (if CASCADE)
5. Remove indexes
6. Deallocate storage pages
7. Remove catalog entries
8. Record in transaction log

---

#### SBLR3_DROP_INDEX (0x0112)

```cpp
// Source: src/sblr/executor.cpp:695
void executeDropIndex();
```

**Purpose**: Remove an index.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| index_id | UUID | Index UUID (if known) |
| name_len | uint16_t | Index name length |
| name | char[] | Index name (qualified) |
| if_exists | uint8_t | 0=error if missing, 1=silent |
| concurrently | uint8_t | Drop without locking |

---

#### SBLR3_DROP_VIEW (0x0117)

```cpp
// Source: src/sblr/executor.cpp:773
void executeDropView();
```

**Purpose**: Remove a view.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | View name length |
| name | char[] | View name (qualified) |
| if_exists | uint8_t | 0=error if missing, 1=silent |
| cascade | uint8_t | 0=RESTRICT, 1=CASCADE |
| materialized | uint8_t | 0=any, 1=materialized only |

---

#### SBLR3_DROP_SEQUENCE (0x0114)

```cpp
// Source: src/sblr/executor.cpp:766
void executeDropSequence();
```

**Purpose**: Remove a sequence.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Sequence name length |
| name | char[] | Sequence name (qualified) |
| if_exists | uint8_t | 0=error if missing, 1=silent |
| cascade | uint8_t | 0=RESTRICT, 1=CASCADE |

### ALTER Operations

#### SBLR3_ALTER_TABLE (0x0103)

```cpp
// Source: src/sblr/executor.cpp:697
void executeAlterTable();
```

**Purpose**: Modify table structure.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_id | UUID | Table UUID |
| name_len | uint16_t | Table name length |
| name | char[] | Table name (qualified) |
| action_count | uint16_t | Number of alter actions |
| actions | AlterAction[] | Actions to perform |

**AlterAction Types:**
| Action Code | Description |
|-------------|-------------|
| 1 | ADD COLUMN |
| 2 | DROP COLUMN |
| 3 | ALTER COLUMN TYPE |
| 4 | ALTER COLUMN SET DEFAULT |
| 5 | ALTER COLUMN DROP DEFAULT |
| 6 | ALTER COLUMN SET NOT NULL |
| 7 | ALTER COLUMN DROP NOT NULL |
| 8 | ADD CONSTRAINT |
| 9 | DROP CONSTRAINT |
| 10 | RENAME TO |
| 11 | SET TABLESPACE |

---

#### SBLR3_ALTER_INDEX (0x011E)

```cpp
// Source: src/sblr/executor.cpp:696
void executeAlterIndex();
```

**Purpose**: Modify index properties.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| index_id | UUID | Index UUID |
| name_len | uint16_t | Index name length |
| name | char[] | Index name (qualified) |
| action | uint8_t | 1=RENAME, 2=SET TABLESPACE, 3=REBUILD |
| new_name_len | uint16_t | New name (for RENAME) |
| new_name | char[] | New identifier |

### Constraint Opcodes

#### SBLR3_PRIMARY_KEY (0x0178)

**Purpose**: Define PRIMARY KEY constraint (used within CREATE/ALTER TABLE).

**Payload Schema (Constraint):**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Constraint name length |
| name | char[] | Constraint identifier |
| column_count | uint16_t | Number of key columns |
| columns | uint16_t[] | Column ordinals |
| deferrable | uint8_t | 0=NOT DEFERRABLE, 1=DEFERRABLE |
| initially_deferred | uint8_t | 0=IMMEDIATE, 1=DEFERRED |

#### SBLR3_FOREIGN_KEY (0x0174)

**Purpose**: Define FOREIGN KEY constraint.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Constraint name length |
| name | char[] | Constraint identifier |
| column_count | uint16_t | Number of local columns |
| local_columns | uint16_t[] | Column ordinals in this table |
| ref_table_id | UUID | Referenced table UUID |
| ref_column_count | uint16_t | Number of referenced columns |
| ref_columns | uint16_t[] | Column ordinals in referenced table |
| on_delete | uint8_t | 0=NO ACTION, 1=RESTRICT, 2=CASCADE, 3=SET NULL, 4=SET DEFAULT |
| on_update | uint8_t | Same as on_delete |
| deferrable | uint8_t | Deferrable flag |
| match_type | uint8_t | 0=SIMPLE, 1=FULL, 2=PARTIAL |

#### SBLR3_UNIQUE_CONSTRAINT (0x017C)

**Purpose**: Define UNIQUE constraint.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Constraint name length |
| name | char[] | Constraint identifier |
| column_count | uint16_t | Number of columns |
| columns | uint16_t[] | Column ordinals |
| deferrable | uint8_t | Deferrable flag |
| nulls_distinct | uint8_t | 0=NULLS NOT DISTINCT, 1=NULLS DISTINCT |

#### SBLR3_CHECK_CONSTRAINT (0x0107)

**Purpose**: Define CHECK constraint.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Constraint name length |
| name | char[] | Constraint identifier |
| expression_len | uint16_t | CHECK expression length |
| expression | char[] | Boolean expression |
| deferrable | uint8_t | Deferrable flag |

### Column Property Opcodes

#### SBLR3_DEFAULT_VALUE (0x0110)

**Purpose**: Specify column default value.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| expression_len | uint16_t | Default expression length |
| expression | char[] | Expression text or literal |

#### SBLR3_NOT_NULL (0x0177)

**Purpose**: Specify NOT NULL constraint.

**Payload:** None (marker opcode)

#### SBLR3_GENERATED_COLUMN (0x0175)

**Purpose**: Define computed/generated column.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| expression_len | uint16_t | Generation expression length |
| expression | char[] | Expression text |
| always | uint8_t | 0=BY DEFAULT, 1=ALWAYS |
| stored | uint8_t | 0=VIRTUAL, 1=STORED |

#### SBLR3_IDENTITY_COLUMN (0x0176)

**Purpose**: Define IDENTITY (auto-increment) column.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| sequence_id | UUID | Associated sequence UUID |
| always | uint8_t | 0=BY DEFAULT, 1=ALWAYS |
| start_value | int64_t | Initial value |
| increment | int64_t | Step value |

### Schema Operations

#### SBLR3_ALTER_SCHEMA (0x0123)

```cpp
// Source: src/sblr/executor.cpp:702
void executeAlterSchema();
```

**Purpose**: Modify schema properties.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| schema_id | UUID | Schema UUID |
| name_len | uint16_t | Schema name length |
| name | char[] | Schema name |
| action | uint8_t | 1=RENAME TO, 2=CHANGE OWNER |
| new_owner_id | UUID | New owner UUID |

#### SBLR3_DROP_SCHEMA (0x015B)

```cpp
// Source: src/sblr/executor.cpp:701
void executeDropSchema();
```

**Purpose**: Remove a schema.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| schema_id | UUID | Schema UUID |
| name_len | uint16_t | Schema name length |
| name | char[] | Schema name |
| if_exists | uint8_t | 0=error if missing, 1=silent |
| cascade | uint8_t | 0=RESTRICT, 1=CASCADE |

### Tablespace Operations

#### SBLR3_ALTER_TABLE_SET_TABLESPACE (0x0105)

```cpp
// Source: src/sblr/executor.cpp:693
void executeAlterTableSetTablespace();
```

**Purpose**: Move table to different tablespace.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_id | UUID | Table UUID |
| tablespace_id | UUID | Target tablespace UUID |
| index_move | uint8_t | 0=leave indexes, 1=move all |

#### SBLR3_ATTACH_TABLESPACE (0x0106)

```cpp
// Source: src/sblr/executor.cpp:711
void executeAttachTablespace();
```

**Purpose**: Attach an existing tablespace.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | Tablespace name length |
| name | char[] | Tablespace identifier |
| location_len | uint16_t | Path length |
| location | char[] | Filesystem path |

#### SBLR3_DETACH_TABLESPACE (0x0111)

```cpp
// Source: src/sblr/executor.cpp:712
void executeDetachTablespace();
```

**Purpose**: Detach a tablespace.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| tablespace_id | UUID | Tablespace UUID |
| name_len | uint16_t | Name length (alternative) |
| name | char[] | Tablespace name |

### Special Operations

#### SBLR3_TRUNCATE_TABLE (0x017B)

```cpp
// Source: src/sblr/executor.cpp:709
void executeTruncateTable();
```

**Purpose**: Quickly remove all rows from a table.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_id | UUID | Table UUID |
| name_len | uint16_t | Table name length |
| name | char[] | Table name (qualified) |
| restart_identity | uint8_t | Reset sequences? |
| cascade | uint8_t | 0=RESTRICT, 1=CASCADE |
| continue_identity | uint8_t | Keep sequence values |

**Execution Semantics:**
1. Acquire exclusive table lock
2. For RESTART: reset associated sequences
3. Deallocate all data pages
4. Reset high-water marks
5. Preserve table structure and indexes
6. Much faster than DELETE without WHERE

#### SBLR3_RENAME_OBJECT (0x016F)

```cpp
// Source: src/sblr/executor.cpp:698
void executeRenameObject();
```

**Purpose**: Rename any database object.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| object_type | uint8_t | 1=TABLE, 2=INDEX, 3=VIEW, 4=SEQUENCE, etc. |
| old_name_len | uint16_t | Old name length |
| old_name | char[] | Current qualified name |
| new_name_len | uint16_t | New name length |
| new_name | char[] | New identifier (unqualified) |

#### SBLR3_MOVE_OBJECT (0x016C)

```cpp
// Source: src/sblr/executor.cpp:699
void executeMoveObject();
```

**Purpose**: Move object between schemas.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| object_type | uint8_t | Object type code |
| object_id | UUID | Object UUID |
| target_schema_id | UUID | Destination schema |
| new_name_len | uint16_t | Optional rename |
| new_name | char[] | New name (or empty) |

### Invariants

1. **Catalog Consistency**: All DDL operations maintain catalog referential integrity
   - Verification: Foreign key checks in catalog tables

2. **Atomicity**: Each DDL opcode is atomic (all-or-nothing)
   - Verification: Transaction rollback on error

3. **Name Uniqueness**: Object names are unique within their namespace
   - Verification: Unique constraints on catalog tables

4. **Storage Integrity**: Physical storage allocation matches catalog state
   - Verification: Page manager consistency checks

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `E_TABLE_EXISTS` | CREATE TABLE with existing name | Use IF NOT EXISTS or DROP first |
| `E_TABLE_NOT_FOUND` | Operation on non-existent table | Verify name or use IF EXISTS |
| `E_COLUMN_EXISTS` | ADD COLUMN with duplicate name | Choose different name |
| `E_COLUMN_NOT_FOUND` | Reference to unknown column | Verify column name |
| `E_CONSTRAINT_VIOLATION` | Adding constraint fails validation | Fix data then retry |
| `E_DEPENDENT_OBJECTS` | DROP with RESTRICT and dependencies | Use CASCADE or drop dependencies |
| `E_INVALID_DEFAULT` | Invalid default expression | Correct expression syntax |
| `E_INSUFFICIENT_PRIVILEGE` | User lacks DDL permission | Grant appropriate privileges |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_sblr_v3_schema.cpp` | Schema creation and modification |
| `tests/unit/test_sblr_type_opcodes.cpp` | Type-related DDL |

## Related Specifications

- [v3_payload_schemas.md](./v3_payload_schemas.md) - Payload encoding details
- [opcodes_dml.md](./opcodes_dml.md) - Data manipulation opcodes
- [opcodes_index.md](./opcodes_index.md) - Index-specific operations

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
