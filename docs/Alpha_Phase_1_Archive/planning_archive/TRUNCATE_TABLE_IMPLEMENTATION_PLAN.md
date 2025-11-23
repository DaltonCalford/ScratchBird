# TRUNCATE TABLE Implementation Plan

**Date**: November 7, 2025
**Goal**: Complete TRUNCATE TABLE implementation (final DDL Modification operation)
**Estimated Effort**: 10-15 hours

---

## Overview

TRUNCATE TABLE is a DDL operation that quickly removes all rows from a table without:
- Scanning individual rows
- Generating DELETE operations
- Firing triggers
- Creating undo logs for each row

It's significantly faster than `DELETE FROM table` for large tables.

---

## SQL Syntax

```sql
TRUNCATE [TABLE] table_name [RESTART IDENTITY | CONTINUE IDENTITY] [CASCADE | RESTRICT];
```

### Minimal Implementation (Phase 1)
```sql
TRUNCATE TABLE table_name;
```

### Future Enhancements (Post-Alpha)
- `RESTART IDENTITY` - Reset sequences associated with table
- `CONTINUE IDENTITY` - Keep current sequence values (default)
- `CASCADE` - Truncate dependent tables with foreign keys
- `RESTRICT` - Fail if dependent tables exist (default)

---

## Implementation Layers

### 1. AST Node (include/scratchbird/parser/ast.h)

Add to ASTKind enum (after DROP_INDEX):
```cpp
TRUNCATE_TABLE,            // ALPHA Phase 1 - DDL Modifications (final operation)
```

Add TruncateTableStmt class (after DropIndexStmt):
```cpp
// TRUNCATE TABLE statement (ALPHA Phase 1 - DDL Modifications)
class TruncateTableStmt : public Statement
{
public:
    TruncateTableStmt(const SourceSpan &span, StringPool::StringId table_name)
        : Statement(ASTKind::TRUNCATE_TABLE, span), table_name_(table_name)
    {
    }

    StringPool::StringId tableName() const
    {
        return table_name_;
    }

    void accept(ASTVisitor *visitor) override;

private:
    StringPool::StringId table_name_;
};
```

### 2. Token (include/scratchbird/parser/token.h)

Add keyword token (already exists):
```cpp
KW_TRUNCATE,  // (check if exists, may need to add)
```

### 3. Lexer (src/parser/lexer.cpp)

Add keyword mapping:
```cpp
{"TRUNCATE", TokenType::KW_TRUNCATE},
```

### 4. Parser (src/parser/parser.cpp)

Add parser method:
```cpp
Statement *Parser::parseTruncateTable()
{
    auto start_loc = current().location;

    // TRUNCATE already consumed by parseStatement()
    // Optional TABLE keyword
    if (match(TokenType::KW_TABLE))
    {
        advance();
    }

    // Get table name
    if (!check(TokenType::IDENTIFIER))
    {
        error("Expected table name after TRUNCATE TABLE");
        return nullptr;
    }

    auto table_name = current().value.string_id;
    advance();

    // Create AST node
    auto *stmt = arena_.make<TruncateTableStmt>(makeSpan(start_loc), table_name);

    // Consume semicolon if present
    match(TokenType::SEMICOLON);

    return stmt;
}
```

Add to parseStatement() switch:
```cpp
case TokenType::KW_TRUNCATE:
    return parseTruncateTable();
```

### 5. Opcode (include/scratchbird/sblr/opcodes.h)

Add opcode:
```cpp
TRUNCATE_TABLE = 0x22,        // Truncate table (ALPHA Phase 1 - DDL Modifications)
```

### 6. Bytecode Generator (src/sblr/bytecode_generator.cpp)

Add visitor method:
```cpp
void BytecodeGenerator::visit(parser::TruncateTableStmt *node)
{
    // Write opcode
    current_result_->writeOpcode(Opcode::TRUNCATE_TABLE);

    // Write table name
    writeStringId(node->tableName());
}
```

Add to ast.cpp visitor dispatch:
```cpp
void TruncateTableStmt::accept(ASTVisitor *visitor)
{
    visitor->visit(this);
}
```

### 7. Executor (src/sblr/executor.cpp)

Add executor method:
```cpp
void Executor::executeTruncateTable()
{
    // Read table name from bytecode
    std::string table_name = readString();

    // Get current schema
    core::CatalogManager::SchemaInfo schema_info;
    ErrorContext ctx;
    auto status = db_->catalog_manager()->getSchema(current_schema_, schema_info, &ctx);
    if (status != Status::OK)
    {
        throw std::runtime_error("Schema not found: " + current_schema_);
    }

    // Get table info
    core::CatalogManager::TableInfo table_info;
    status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info, &ctx);
    if (status != Status::OK)
    {
        throw std::runtime_error("Table not found: " + table_name);
    }

    // Truncate table via catalog
    status = db_->catalog_manager()->truncateTable(table_info.table_id, &ctx);
    if (status != Status::OK)
    {
        throw std::runtime_error("Failed to truncate table: " + table_name);
    }
}
```

Add to executeStatement() switch:
```cpp
case Opcode::TRUNCATE_TABLE:
    executeTruncateTable();
    break;
```

### 8. Catalog Manager (src/core/catalog_manager.cpp)

Add method declaration (include/scratchbird/core/catalog_manager.h):
```cpp
// TRUNCATE TABLE (ALPHA Phase 1 - DDL Modifications)
auto truncateTable(const ID &table_id, ErrorContext *ctx = nullptr) -> Status;
```

Implement method:
```cpp
auto CatalogManager::truncateTable(const ID &table_id, ErrorContext *ctx) -> Status
{
    // Pin table catalog page
    auto table_catalog_gpid = makeGPID(PRIMARY_TABLESPACE_ID, TABLE_CATALOG_PAGE);
    auto table_pin = buffer_pool_->pinPage(table_catalog_gpid, ctx);
    if (!table_pin)
    {
        return Status::IO_ERROR;
    }

    HeapPage table_catalog_page(table_pin.frame()->data());

    // Find table record
    bool table_found = false;
    GPID first_heap_gpid;
    uint16_t tablespace_id = 0;

    auto item_ids = table_catalog_page.getItemIds();
    for (const auto &item_id : item_ids)
    {
        if (!table_catalog_page.isItemValid(item_id))
            continue;

        auto tuple_data = table_catalog_page.getTuple(item_id);
        TableRecord table_rec;
        std::memcpy(&table_rec, tuple_data, sizeof(TableRecord));

        if (table_rec.table_id == table_id && table_rec.is_valid)
        {
            first_heap_gpid = table_rec.first_heap_gpid;
            tablespace_id = table_rec.tablespace_id;
            table_found = true;

            // Update table metadata
            table_rec.row_count = 0;
            table_rec.last_modified_time = std::time(nullptr);

            // Write back updated record
            std::memcpy(const_cast<uint8_t *>(tuple_data), &table_rec, sizeof(TableRecord));
            table_pin.markDirty();
            break;
        }
    }

    if (!table_found)
    {
        return Status::NOT_FOUND;
    }

    // Unpin table catalog
    table_pin.unpin();

    // Strategy: Mark all heap pages as free by clearing item count
    // This is MGA-compliant as we're just marking space as reusable
    // Actual data remains until overwritten

    GPID current_gpid = first_heap_gpid;
    while (current_gpid.page_id != 0)
    {
        auto heap_pin = buffer_pool_->pinPage(current_gpid, ctx);
        if (!heap_pin)
        {
            return Status::IO_ERROR;
        }

        HeapPage heap_page(heap_pin.frame()->data());

        // Get next page before clearing
        GPID next_gpid = heap_page.getNextPage();

        // Clear all items (fast truncation)
        heap_page.clearAllItems();

        heap_pin.markDirty();
        heap_pin.unpin();

        current_gpid = next_gpid;
    }

    return Status::OK;
}
```

Add HeapPage method (include/scratchbird/core/heap_page.h):
```cpp
// Clear all items (for TRUNCATE TABLE)
void clearAllItems();
```

Implement (src/core/heap_page.cpp):
```cpp
void HeapPage::clearAllItems()
{
    auto *header = getHeader();
    header->item_count = 0;
    header->free_space_offset = HEAP_PAGE_DATA_OFFSET;
    header->used_space = 0;
}
```

---

## MGA Compliance

TRUNCATE TABLE is MGA-compliant because:
1. **No transaction visibility checks needed** - Removing all data atomically
2. **Fast space reclamation** - Marks pages as free without scanning rows
3. **No back-versions created** - This is a table-level operation
4. **Indexes handled separately** - Can rebuild or clear index pages
5. **Table metadata updated** - row_count reset, last_modified_time updated

---

## Testing Strategy

### Unit Tests
1. Create table, insert data, TRUNCATE, verify empty
2. TRUNCATE empty table (no-op)
3. TRUNCATE non-existent table (error)
4. Multiple TRUNCATE operations
5. TRUNCATE with indexes (verify indexes cleared)

### Integration Tests
1. Large table (1M rows) - verify performance vs DELETE
2. TRUNCATE within transaction - verify commit/rollback
3. Concurrent TRUNCATE and SELECT
4. TRUNCATE with foreign keys (future: should fail with RESTRICT)

---

## Performance Characteristics

| Operation | Time Complexity | Space Complexity |
|-----------|----------------|------------------|
| DELETE FROM table | O(n) rows | O(n) undo log |
| TRUNCATE TABLE | O(p) pages | O(1) |

For a table with 1M rows across 10K pages:
- DELETE: Scans 1M rows, creates 1M undo records
- TRUNCATE: Clears 10K pages, no undo records

**Expected speedup**: 100-1000x for large tables

---

## Files to Modify

1. `include/scratchbird/parser/ast.h` - Add TRUNCATE_TABLE, TruncateTableStmt
2. `include/scratchbird/parser/token.h` - Add KW_TRUNCATE (if needed)
3. `src/parser/lexer.cpp` - Add TRUNCATE keyword mapping
4. `src/parser/parser.cpp` - Add parseTruncateTable()
5. `src/parser/ast.cpp` - Add TruncateTableStmt::accept()
6. `include/scratchbird/sblr/opcodes.h` - Add TRUNCATE_TABLE opcode
7. `src/sblr/bytecode_generator.cpp` - Add visitor for TruncateTableStmt
8. `include/scratchbird/sblr/bytecode_visitor.h` - Add visit() declaration
9. `src/sblr/executor.cpp` - Add executeTruncateTable()
10. `include/scratchbird/core/catalog_manager.h` - Add truncateTable() declaration
11. `src/core/catalog_manager.cpp` - Implement truncateTable()
12. `include/scratchbird/core/heap_page.h` - Add clearAllItems() declaration
13. `src/core/heap_page.cpp` - Implement clearAllItems()

**Total**: 13 files

---

## Success Criteria

✅ SQL parsing works: `TRUNCATE TABLE users;`
✅ Bytecode generation successful
✅ Executor clears all table data
✅ Table metadata updated (row_count = 0)
✅ Heap pages marked as free
✅ Build completes with zero errors
✅ Performance significantly faster than DELETE

---

## Future Enhancements (Post-Alpha)

1. **RESTART IDENTITY** - Reset associated sequences
2. **CASCADE** - Truncate dependent tables
3. **Index rebuilding** - Option to rebuild indexes after truncate
4. **Partition support** - TRUNCATE specific partitions
5. **Statistics update** - Update table statistics after truncate
6. **Foreign key handling** - Proper CASCADE/RESTRICT with FK dependencies

---

**Status**: Ready for implementation
**Priority**: HIGH (completes DDL Modifications to 100%)
**Complexity**: LOW-MEDIUM
**Risk**: LOW (simple operation, no complex dependencies)
