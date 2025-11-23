# ALTER TABLE Implementation Plan

**Date**: November 7, 2025
**Feature**: ALTER TABLE (ADD/DROP/RENAME COLUMN, ALTER COLUMN TYPE)
**Status**: Planning Complete - Ready for Implementation
**Parent**: DDL Modifications (ALPHA Phase 1)

---

## OVERVIEW

Complete the DDL Modifications feature by implementing ALTER TABLE functionality. The AST/Parser/Bytecode infrastructure is already in place from the DROP TABLE/INDEX work. This plan focuses on the catalog manager implementation and executor logic.

### What's Already Done ✅

1. **AST Infrastructure** (ast.h lines 1138-1281)
   - AlterTableStmt class with 8 action types
   - Full accessor methods for all variants
   - Visitor pattern support

2. **Parser** (Already has parseAlterTable stub)
   - ALTER keyword recognized
   - TABLE keyword recognized

3. **Bytecode** (opcodes.h)
   - ALTER_TABLE opcode (0x21) defined
   - BytecodeGenerator visitor stub exists

4. **Semantic Analyzer**
   - Visitor stub exists

### What Needs Implementation ❌

1. **Catalog Manager** (NEW methods needed)
   - `addColumn()` - Add a new column to existing table
   - `dropColumn()` - Soft delete a column from table
   - `renameColumn()` - Rename column (update ColumnRecord)
   - `alterColumnType()` - Change column type (MGA considerations)

2. **Executor** (executeAlterTable)
   - Decode bytecode for each ALTER variant
   - Call appropriate catalog methods
   - Handle error conditions

3. **Parser** (parseAlterTable - full implementation)
   - Parse all 8 ALTER TABLE variants
   - Build proper AST nodes

---

## IMPLEMENTATION PRIORITY

Focus on these 4 core operations (80% of use cases):

1. **ALTER TABLE ADD COLUMN** (Highest priority)
2. **ALTER TABLE DROP COLUMN** (High priority)
3. **ALTER TABLE RENAME COLUMN** (Medium priority)
4. **ALTER TABLE ALTER COLUMN TYPE** (Medium priority)

Defer for later:
- ALTER COLUMN SET/DROP DEFAULT
- ADD/DROP CONSTRAINT

---

## CATALOG MANAGER API DESIGN

### 1. ADD COLUMN

```cpp
auto addColumn(const ID &table_id, const ColumnInfo &column_info,
               ErrorContext *ctx = nullptr) -> Status;
```

**MGA Considerations**:
- Add new ColumnRecord with ordinal = max(ordinal) + 1
- Set is_valid = 1
- Update TableRecord.column_count (soft update)
- Existing rows see NULL for new column (default MGA behavior)

**Algorithm**:
```
1. Lock catalog mutex
2. Find table in cache (error if not found)
3. Read all column records for table
4. Determine next ordinal = max(ordinal) + 1
5. Create ColumnRecord:
   - column_id = new UUID
   - ordinal = next ordinal
   - Copy all column properties from ColumnInfo
   - is_valid = 1
6. Write ColumnRecord to columns_table_page
7. Update TableRecord.column_count (+1)
8. Update table cache
9. Unlock and return OK
```

**Edge Cases**:
- Column name already exists → ALREADY_EXISTS
- Table doesn't exist → NOT_FOUND
- Invalid data type → INVALID_ARGUMENT
- Page full → Allocate new column page

### 2. DROP COLUMN

```cpp
auto dropColumn(const ID &table_id, const std::string &column_name,
                bool if_exists, bool cascade, ErrorContext *ctx = nullptr) -> Status;
```

**MGA Considerations**:
- Mark ColumnRecord.is_valid = 0 (soft delete)
- Update TableRecord.column_count (-1)
- DO NOT reorder ordinals (stable positioning)
- Old transactions still see column
- CASCADE: Check for indexes using column, drop them

**Algorithm**:
```
1. Lock catalog mutex
2. Find table in cache (error if not found)
3. Search column records for matching name
4. If not found:
   - if_exists → return OK
   - else → return NOT_FOUND
5. Check for dependent indexes using this column:
   - Search index_cache for indexes on this table
   - For each index, check if column_id is in index.column_ids[]
   - If dependencies exist and !cascade → return INVALID_ARGUMENT
   - If cascade → drop all dependent indexes
6. Mark ColumnRecord.is_valid = 0
7. Update TableRecord.column_count (-1)
8. Update caches
9. Return OK
```

**Edge Cases**:
- Last column in table → INVALID_ARGUMENT (can't drop all columns)
- Column is primary key → INVALID_ARGUMENT (use DROP CONSTRAINT first)
- Indexes depend on column + RESTRICT → INVALID_ARGUMENT
- Indexes depend on column + CASCADE → Drop indexes first

### 3. RENAME COLUMN

```cpp
auto renameColumn(const ID &table_id, const std::string &old_name,
                  const std::string &new_name, ErrorContext *ctx = nullptr) -> Status;
```

**MGA Considerations**:
- Update ColumnRecord.column_name in-place
- No ordinal changes
- No is_valid changes
- Old transactions see old name (TIP-based visibility)

**Algorithm**:
```
1. Lock catalog mutex
2. Find table in cache
3. Search for old column name
4. Check new name doesn't exist → ALREADY_EXISTS
5. Validate new name (SQL identifier rules)
6. Update ColumnRecord.column_name
7. Mark columns_table_page dirty
8. Update cache
9. Return OK
```

**Edge Cases**:
- Old name doesn't exist → NOT_FOUND
- New name already exists → ALREADY_EXISTS
- Invalid new name → INVALID_ARGUMENT

### 4. ALTER COLUMN TYPE

```cpp
auto alterColumnType(const ID &table_id, const std::string &column_name,
                     const TypeInfo &new_type, ErrorContext *ctx = nullptr) -> Status;
```

**MGA Considerations**:
- **COMPLEX**: Requires data conversion for existing rows
- **Phase 1 approach**: Only allow compatible type changes
  - INT32 → INT64 (widening)
  - VARCHAR(10) → VARCHAR(20) (length increase)
  - NOT NULL → NULLABLE (relaxation)
- **Full implementation**: Requires table rewrite (defer to Phase 2)

**Algorithm (Phase 1 - Compatible Changes Only)**:
```
1. Lock catalog mutex
2. Find table and column
3. Check type compatibility:
   - Same type family (numeric, string, etc.)
   - New type wider/more permissive than old
   - If incompatible → return INVALID_ARGUMENT with message
4. Update ColumnRecord:
   - data_type = new type
   - type_precision = new precision
   - type_scale = new scale
5. Mark page dirty
6. Update cache
7. Return OK
```

**Edge Cases**:
- Incompatible type change → INVALID_ARGUMENT
- Narrowing type change → INVALID_ARGUMENT (Phase 1)
- Column doesn't exist → NOT_FOUND

---

## EXECUTOR IMPLEMENTATION

### executeAlterTable() Full Implementation

```cpp
void Executor::executeAlterTable()
{
    // Read bytecode
    std::string table_name = readString();
    uint8_t action_byte = readUint8();
    auto action = static_cast<AlterTableStmt::AlterAction>(action_byte);

    // Get table
    ErrorContext ctx;
    core::CatalogManager::TableInfo table_info;
    auto status = db_->catalog_manager()->getTable("PUBLIC", table_name, table_info, &ctx);
    if (status != Status::OK)
        throw std::runtime_error("Table not found: " + table_name);

    // Dispatch based on action
    switch (action) {
        case AlterTableStmt::AlterAction::ADD_COLUMN: {
            std::string col_name = readString();
            uint16_t data_type = readUint16();
            uint32_t precision = readUint32();
            uint32_t scale = readUint32();
            bool nullable = readBool();

            core::CatalogManager::ColumnInfo col_info;
            col_info.column_name = col_name;
            col_info.data_type = static_cast<core::DataType>(data_type);
            col_info.type_precision = precision;
            col_info.type_scale = scale;
            col_info.nullable = nullable;

            status = db_->catalog_manager()->addColumn(table_info.table_id, col_info, &ctx);
            if (status != Status::OK)
                throw std::runtime_error("Failed to add column: " + ctx.message);
            break;
        }

        case AlterTableStmt::AlterAction::DROP_COLUMN: {
            std::string col_name = readString();
            bool if_exists = readBool();
            bool cascade = readBool();

            status = db_->catalog_manager()->dropColumn(
                table_info.table_id, col_name, if_exists, cascade, &ctx);

            if (status != Status::OK && !(status == Status::NOT_FOUND && if_exists))
                throw std::runtime_error("Failed to drop column: " + ctx.message);
            break;
        }

        case AlterTableStmt::AlterAction::RENAME_COLUMN: {
            std::string old_name = readString();
            std::string new_name = readString();

            status = db_->catalog_manager()->renameColumn(
                table_info.table_id, old_name, new_name, &ctx);

            if (status != Status::OK)
                throw std::runtime_error("Failed to rename column: " + ctx.message);
            break;
        }

        case AlterTableStmt::AlterAction::ALTER_COLUMN_TYPE: {
            std::string col_name = readString();
            uint16_t new_type = readUint16();
            uint32_t new_precision = readUint32();
            uint32_t new_scale = readUint32();

            core::CatalogManager::TypeInfo type_info;
            type_info.data_type = static_cast<core::DataType>(new_type);
            type_info.type_precision = new_precision;
            type_info.type_scale = new_scale;

            status = db_->catalog_manager()->alterColumnType(
                table_info.table_id, col_name, type_info, &ctx);

            if (status != Status::OK)
                throw std::runtime_error("Failed to alter column type: " + ctx.message);
            break;
        }

        default:
            throw std::runtime_error("ALTER TABLE action not implemented");
    }
}
```

---

## PARSER IMPLEMENTATION

Need to implement parseAlterTable() with full syntax support:

```sql
-- Supported syntax:
ALTER TABLE table_name ADD COLUMN column_name data_type [NOT NULL] [DEFAULT expr];
ALTER TABLE table_name DROP COLUMN column_name [IF EXISTS] [CASCADE | RESTRICT];
ALTER TABLE table_name RENAME COLUMN old_name TO new_name;
ALTER TABLE table_name ALTER COLUMN column_name TYPE new_type;
```

Parser implementation already has infrastructure from DROP TABLE work. Follow same pattern.

---

## BYTECODE FORMAT

Extend existing ALTER_TABLE opcode (0x21):

```
Opcode: ALTER_TABLE (0x21)
Format:
  - table_name (string)
  - action (uint8_t) - AlterAction enum value

  If ADD_COLUMN:
    - column_name (string)
    - data_type (uint16_t)
    - type_precision (uint32_t)
    - type_scale (uint32_t)
    - nullable (bool)

  If DROP_COLUMN:
    - column_name (string)
    - if_exists (bool)
    - cascade (bool)

  If RENAME_COLUMN:
    - old_column_name (string)
    - new_column_name (string)

  If ALTER_COLUMN_TYPE:
    - column_name (string)
    - new_data_type (uint16_t)
    - new_type_precision (uint32_t)
    - new_type_scale (uint32_t)
```

---

## MGA COMPLIANCE CHECKLIST

| Operation | MGA Requirement | Implementation |
|-----------|-----------------|----------------|
| ADD COLUMN | Soft insert | ✅ New ColumnRecord with is_valid=1 |
| DROP COLUMN | Soft delete | ✅ Mark is_valid=0, no physical delete |
| RENAME COLUMN | Update in-place | ✅ Direct column_name update |
| ALTER TYPE | Compatible only | ✅ Phase 1: Compatible changes only |
| CASCADE | Recursive drops | ✅ Drop dependent indexes first |
| Visibility | TIP-based | ✅ Old transactions see old schema |
| Ordinals | Stable | ✅ Never reorder column ordinals |

---

## TESTING PLAN

### Integration Tests (6-8 hours)

1. **test_alter_table_add_column.cpp**
   - Add single column
   - Add multiple columns
   - Add with DEFAULT
   - Add duplicate column (error)
   - Verify existing rows see NULL

2. **test_alter_table_drop_column.cpp**
   - Drop single column
   - Drop with IF EXISTS
   - Drop last column (error)
   - Drop non-existent column (error)
   - Drop with CASCADE (indexes)

3. **test_alter_table_rename_column.cpp**
   - Rename valid column
   - Rename to existing name (error)
   - Rename non-existent column (error)

4. **test_alter_table_alter_type.cpp**
   - Compatible type changes (INT32→INT64, VARCHAR(10)→VARCHAR(20))
   - Incompatible type changes (error)

5. **test_alter_table_cascade.cpp**
   - DROP COLUMN with index dependency
   - Verify CASCADE drops indexes
   - Verify RESTRICT fails

---

## IMPLEMENTATION TIMELINE

| Task | Estimated Time | Complexity |
|------|----------------|------------|
| **Catalog Manager** | **12-14 hours** | **High** |
| - addColumn() | 3-4 hours | Medium |
| - dropColumn() | 4-5 hours | High (CASCADE logic) |
| - renameColumn() | 2-3 hours | Low |
| - alterColumnType() | 3-4 hours | Medium |
| **Executor** | **3-4 hours** | Medium |
| - executeAlterTable() full impl | 3-4 hours | Medium |
| **Parser** | **4-5 hours** | Medium |
| - parseAlterTable() full impl | 4-5 hours | Medium |
| **Bytecode Generator** | **2-3 hours** | Medium |
| - Full ALTER TABLE visitor | 2-3 hours | Medium |
| **Testing** | **6-8 hours** | High |
| **Documentation** | **2-3 hours** | Low |
| **TOTAL** | **29-37 hours** | |

---

## SUCCESS CRITERIA

✅ All four core ALTER TABLE operations work:
- ADD COLUMN
- DROP COLUMN (with CASCADE/RESTRICT)
- RENAME COLUMN
- ALTER COLUMN TYPE (compatible changes)

✅ 100% MGA compliant (soft deletes, stable ordinals, TIP visibility)

✅ Full integration tests passing

✅ Project builds cleanly

✅ Documentation updated

---

## DEPENDENCIES

- ✅ DROP TABLE/INDEX infrastructure (COMPLETE)
- ✅ AST/Parser/Bytecode infrastructure (COMPLETE)
- ✅ Catalog column management (EXISTS - writeColumnRecords, readColumnRecords)
- ✅ Error handling (ErrorContext pattern)
- ✅ UUID/ID type system (COMPLETE)

**Status**: All dependencies satisfied - Ready to implement!

---

## NEXT STEPS

1. Start with catalog manager implementation (highest complexity)
2. Then executor (medium complexity)
3. Then parser (medium complexity)
4. Finally bytecode generator (low complexity)
5. Integration tests throughout
6. Documentation at end

**Estimated completion**: 29-37 hours total work
**Priority**: HIGH (completes DDL Modifications feature)
**MGA Risk**: LOW (following established patterns)

---

**Created by**: Claude Code
**Date**: November 7, 2025
**Status**: ✅ PLANNING COMPLETE - Ready for implementation
