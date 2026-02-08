# Views Implementation - ALPHA Phase 1

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## Overview

The Views Execution feature enables SQL views in ScratchBird database. A view is a virtual table based on a SELECT query that can be queried like a regular table.

## Implementation Status

### ✅ Completed Features

1. **CREATE VIEW** - Store view definitions
2. **DROP VIEW** - Remove views from catalog
3. **View Query Rewriting** - Transparent view expansion
4. **Column Projection** - SELECT specific columns from views
5. **CREATE OR REPLACE VIEW** - Update existing views
6. **Nested Views** - Views can reference other views
7. **Complex Views** - WHERE clauses, ORDER BY, LIMIT, subqueries

### ⏳ Pending Features

1. **Materialized Views** - Physical storage of view results
2. **REFRESH MATERIALIZED VIEW** - Update materialized view data
3. **Updatable Views** - INSERT/UPDATE/DELETE through views
4. **WITH CHECK OPTION** - Constraint enforcement on updatable views

## Architecture

### 1. View Definition Storage

**File**: `src/parser/parser.cpp` (lines 3692-3714)

When parsing `CREATE VIEW user_names AS SELECT name FROM users`:
1. Capture start offset of SELECT query
2. Parse the SELECT statement
3. Extract SELECT query text using source offsets
4. Store in CreateViewStmt AST node

```cpp
auto select_start_offset = previous().location.offset;
auto *query = parseSelect();
auto select_end_offset = previous().location.offset + previous().length;
std::string query_text(lexer_.input().substr(
    select_start_offset,
    select_end_offset - select_start_offset));
stmt->setQueryDefinitionText(std::move(query_text));
```

**File**: `include/scratchbird/parser/ast.h` (lines 1680-1695)

CreateViewStmt stores:
- View name
- SELECT query AST
- OR REPLACE flag
- **Actual SELECT query text** (for rewriting)

### 2. View Query Rewriting

**File**: `src/sblr/executor.cpp` (lines 6114-6127)

When executing `SELECT * FROM user_names`:
1. Executor looks up "user_names" in catalog
2. Table lookup fails → Check if it's a view
3. If view exists → Call executeViewQuery()
4. Otherwise → Error "Table or view not found"

```cpp
if (status != core::Status::OK)
{
    // Check if this is a view
    core::CatalogManager::ViewInfo view_info;
    auto view_status = db_->catalog_manager()->getView(
        schema_info.schema_id, table_name, view_info, &view_ctx);

    if (view_status == core::Status::OK)
    {
        // This is a view - execute its SELECT query
        executeViewQuery(view_info, select_items, is_select_star);
        return;
    }

    error("Table or view not found: " + table_name);
}
```

### 3. View Execution

**File**: `src/sblr/executor.cpp` (lines 6023-6146)

View execution flow:
1. **Parse** view definition (stored SELECT query)
2. **Generate** bytecode for the parsed query
3. **Execute** bytecode in new Executor instance
4. **Copy** result set to current executor
5. **Apply** column projection if needed

```cpp
void Executor::executeViewQuery(
    const core::CatalogManager::ViewInfo& view_info,
    const std::vector<std::pair<std::string, std::string>>& select_items,
    bool is_select_star)
{
    // 1. Parse view definition
    parser::Lexer lexer(view_info.definition);
    parser::Parser parser(lexer, arena);
    auto* select_stmt = parser.parseStatement();

    // 2. Generate bytecode
    BytecodeGenerator gen(lexer.stringPool(), db_);
    auto bytecode_result = gen.generate(select_stmt);

    // 3. Execute view query
    Executor view_executor(db_);
    view_executor.setConnectionContext(conn_ctx_);  // Preserve security
    auto exec_result = view_executor.execute(bytecode_result.bytecode());

    // 4. Get result set
    auto* view_result_set = exec_result.resultSet();

    // 5. Apply column projection (see below)
    ...
}
```

### 4. Column Projection

**File**: `src/sblr/executor.cpp` (lines 6079-6145)

Two execution paths:

**SELECT * FROM view** - Return all columns:
```cpp
if (is_select_star)
{
    // Copy all columns and rows
    for (size_t i = 0; i < view_result_set->columnCount(); i++)
    {
        current_result_set_->addColumn(
            view_result_set->columnName(i),
            view_result_set->columnType(i));
    }
    // Copy all rows...
}
```

**SELECT col1, col2 FROM view** - Project specific columns:
```cpp
else
{
    // Build column mapping
    std::vector<size_t> column_indices;
    for (const auto& [col_name, alias] : select_items)
    {
        // Find column in view result set
        for (size_t i = 0; i < view_result_set->columnCount(); i++)
        {
            if (view_result_set->columnName(i) == col_name)
            {
                column_indices.push_back(i);
                break;
            }
        }
    }

    // Copy only projected columns and rows
    for (size_t row_idx = 0; row_idx < view_result_set->rowCount(); row_idx++)
    {
        std::vector<Value> row;
        for (size_t proj_idx : column_indices)
        {
            row.push_back(view_result_set->getValue(row_idx, proj_idx));
        }
        current_result_set_->addRow(std::move(row));
    }
}
```

## Security

View execution preserves security context:
- **Connection Context** is passed to view executor
- **RLS Policies** apply to underlying tables
- **Column Permissions** enforce access control
- **User Identity** maintained throughout execution

```cpp
Executor view_executor(db_);
view_executor.setConnectionContext(conn_ctx_);  // Security preserved!
```

## Example Usage

```sql
-- Create a base table
CREATE TABLE users (
    id INT,
    name VARCHAR(100),
    email VARCHAR(100),
    status INT
);

-- Create a view
CREATE VIEW active_users AS
SELECT id, name FROM users WHERE status = 1;

-- Query the view (automatically expanded)
SELECT * FROM active_users;
-- Behind the scenes: Executes "SELECT id, name FROM users WHERE status = 1"

-- Project specific columns
SELECT name FROM active_users;
-- Returns only the 'name' column from the view

-- Create a nested view
CREATE VIEW user_count AS
SELECT COUNT(*) FROM active_users;

-- Update a view definition
CREATE OR REPLACE VIEW active_users AS
SELECT id, name, email FROM users WHERE status = 1;

-- Remove a view
DROP VIEW active_users;
```

## Testing

### Test Files

1. **test_create_view_ast.cpp** - AST and query definition storage
   - 4/4 tests passing
   - Validates query text extraction
   - Tests CREATE OR REPLACE

2. **test_views_comprehensive.cpp** - Full feature coverage
   - 10/10 tests passing (for supported features)
   - 5 tests disabled (waiting for parser enhancements)
   - Tests: WHERE, ORDER BY, LIMIT, nested views, DROP VIEW

### Test Coverage

✅ Query definition storage
✅ Basic SELECT * views
✅ Column projection
✅ WHERE clauses
✅ ORDER BY
✅ LIMIT
✅ Nested views
✅ CREATE OR REPLACE
✅ DROP VIEW parsing
✅ Subqueries in WHERE

⏭️ Complex WHERE (AND/OR) - parser limitation
⏭️ Column aliases (AS) - parser limitation
⏭️ DISTINCT - parser limitation
⏭️ Calculated columns - parser limitation

## Performance Considerations

1. **View Expansion** - Each view query is parsed and executed dynamically
2. **No Caching** - View definitions are parsed every execution (future optimization)
3. **Result Set Copy** - View results are copied to current executor (necessary for lifetime)
4. **Nested Views** - Each level adds parsing/execution overhead

## Future Enhancements

### Materialized Views

Materialize view results physically:
```sql
CREATE MATERIALIZED VIEW user_summary AS
SELECT status, COUNT(*) as total FROM users GROUP BY status;

-- Refresh the materialized data
REFRESH MATERIALIZED VIEW user_summary;
```

**Implementation Requirements**:
1. Add `materialized` boolean to ViewInfo
2. Create physical table to store results
3. Implement REFRESH command
4. Add timestamp tracking for staleness
5. Handle incremental refresh (advanced)

### Updatable Views

Allow DML through views:
```sql
-- Must meet updatability criteria
CREATE VIEW recent_users AS
SELECT * FROM users WHERE created_at > '2024-01-01';

-- Should insert into underlying table
INSERT INTO recent_users (id, name) VALUES (100, 'Alice');

-- Should update underlying table
UPDATE recent_users SET name = 'Bob' WHERE id = 100;
```

**Requirements**:
1. Check view updatability (single table, no aggregation, etc.)
2. Rewrite INSERT/UPDATE/DELETE to target base table
3. Apply view WHERE clause as additional constraint
4. Implement WITH CHECK OPTION validation

### Query Optimization

1. **View Definition Caching** - Cache parsed AST in memory
2. **View Inlining** - Merge view query into outer query (optimizer)
3. **Predicate Pushdown** - Push WHERE conditions into view
4. **Index Usage** - Utilize indexes on base tables

## Commits

1. **a469350** - Store Actual SELECT Query in CREATE VIEW
2. **8c7e592** - Implement View Query Rewriting/Expansion
3. **0b9efc4** - Column Projection & Comprehensive Testing

## Files Modified

### Core Implementation
- `include/scratchbird/parser/ast.h` - CreateViewStmt with query text
- `include/scratchbird/parser/lexer.h` - input() accessor
- `src/parser/parser.cpp` - Query text extraction
- `src/sblr/bytecode_generator.cpp` - Write actual query text
- `include/scratchbird/sblr/executor.h` - executeViewQuery() declaration
- `src/sblr/executor.cpp` - View execution and projection

### Tests
- `tests/unit/test_create_view_ast.cpp` - 4 passing tests
- `tests/unit/test_views_comprehensive.cpp` - 10 passing tests

## References

- SQL Standard: ISO/IEC 9075-2:2016 (SQL/Foundation) - Section 11.27 (View Definition)
- PostgreSQL Views: https://www.postgresql.org/docs/current/sql-createview.html
- MySQL Views: https://dev.mysql.com/doc/refman/8.0/en/create-view.html

---

**Author**: Claude Code
**Date**: November 2025
**Status**: ALPHA Phase 1 - Core Views Execution Complete
