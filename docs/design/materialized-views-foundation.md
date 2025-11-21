# Materialized Views - Foundation Design

## Overview

This document describes the foundational infrastructure added for materialized views support. Full implementation is deferred to a future phase.

## What are Materialized Views?

A materialized view is a database object that:
1. **Stores query results physically** (like a table)
2. **Can be queried much faster** than regular views (no re-computation)
3. **Requires explicit refresh** to update with latest data
4. **Consumes storage space** for the materialized data

### Regular View vs Materialized View

```sql
-- Regular View (virtual, no storage)
CREATE VIEW user_summary AS
SELECT status, COUNT(*) as total FROM users GROUP BY status;

-- Query executes COUNT(*) every time
SELECT * FROM user_summary;  -- Slow for large tables

-- Materialized View (physical storage)
CREATE MATERIALIZED VIEW user_summary AS
SELECT status, COUNT(*) as total FROM users GROUP BY status;

-- Query reads pre-computed results
SELECT * FROM user_summary;  -- Fast!

-- Update the materialized data
REFRESH MATERIALIZED VIEW user_summary;
```

## Foundation Infrastructure Added

### 1. Catalog Schema Extensions

**File**: `include/scratchbird/core/catalog_manager.h`

Added to `ViewInfo` struct:
```cpp
// ALPHA Phase 1 - Materialized Views
bool materialized;              // True if this is a materialized view
ID materialized_table_id;       // Physical table storing the data
uint64_t last_refresh_time;     // Timestamp of last REFRESH (0 if never)
```

### 2. AST Support

**File**: `include/scratchbird/parser/ast.h`

#### CreateViewStmt Extended
Added `materialized` flag to support `CREATE MATERIALIZED VIEW`:
```cpp
CreateViewStmt(const SourceSpan& span, StringPool::StringId name,
               SelectStmt* query, bool or_replace = false,
               bool materialized = false)  // NEW PARAMETER

bool materialized() const { return materialized_; }
```

#### New Statement: RefreshMaterializedViewStmt
```cpp
class RefreshMaterializedViewStmt : public Statement
{
public:
    RefreshMaterializedViewStmt(const SourceSpan& span,
                               StringPool::StringId name,
                               bool concurrently = false);

    StringPool::StringId name() const;
    bool concurrently() const;  // CONCURRENTLY option
};
```

#### ASTKind Enum
Added `REFRESH_MATERIALIZED_VIEW` to ASTKind enum.

## Implementation Plan (Future Phase)

### Phase 1: CREATE MATERIALIZED VIEW

**Parser Changes** (`src/parser/parser.cpp`):
```cpp
// Detect MATERIALIZED keyword
if (match(TokenType::MATERIALIZED))
{
    expect(TokenType::VIEW);
    bool materialized = true;
    // ... create CreateViewStmt with materialized=true
}
```

**Bytecode Generator** (`src/sblr/bytecode_generator.cpp`):
```cpp
void BytecodeGenerator::visit(CreateViewStmt* node)
{
    if (node->materialized())
    {
        // Generate CREATE_MATERIALIZED_VIEW opcode
        // Include initial data population
    }
}
```

**Executor** (`src/sblr/executor.cpp`):
```cpp
void Executor::executeCreateMaterializedView()
{
    // 1. Create view metadata in catalog
    // 2. Create physical table for storage
    // 3. Execute view query
    // 4. Populate physical table with results
    // 5. Record creation timestamp
}
```

### Phase 2: Query Execution

When querying a materialized view:
```cpp
void Executor::executeViewQuery(...)
{
    if (view_info.materialized)
    {
        // Query the physical table directly (fast!)
        executeTableScan(view_info.materialized_table_id);
    }
    else
    {
        // Parse and execute view definition (current implementation)
    }
}
```

### Phase 3: REFRESH MATERIALIZED VIEW

**Parser** (new method):
```cpp
RefreshMaterializedViewStmt* Parser::parseRefreshMaterializedView()
{
    // REFRESH [CONCURRENTLY] MATERIALIZED VIEW view_name
}
```

**Executor**:
```cpp
void Executor::executeRefreshMaterializedView()
{
    // 1. Get view metadata
    // 2. Execute view query
    // 3. If CONCURRENTLY:
    //    - Create temp table
    //    - Populate temp table
    //    - Swap with existing table (atomic)
    // 4. Else:
    //    - Truncate existing table
    //    - Populate with new data
    // 5. Update last_refresh_time
}
```

### Phase 4: DROP MATERIALIZED VIEW

Extend existing DROP VIEW:
```cpp
void Executor::executeDropView()
{
    if (view_info.materialized)
    {
        // Drop physical table first
        dropTable(view_info.materialized_table_id);
    }
    // Drop view metadata
}
```

## Storage Model

### Physical Table Structure

Each materialized view has an associated hidden table:
```
Table Name: _mv_<view_id>
Columns: Same as view SELECT columns
Indexes: Optional (can add indexes for performance)
```

### Catalog Entries

1. **View Catalog Entry**
   - view_id
   - name
   - definition (SELECT query)
   - materialized = true
   - materialized_table_id → physical table

2. **Table Catalog Entry** (for physical storage)
   - table_id = materialized_table_id
   - name = "_mv_<view_id>"
   - hidden = true (not visible in catalog queries)

## Refresh Strategies

### 1. Complete Refresh (Basic)
```sql
REFRESH MATERIALIZED VIEW user_summary;
```
- Truncate existing data
- Re-execute query
- Insert all new rows
- Simple but blocks concurrent reads

### 2. Concurrent Refresh (Advanced)
```sql
REFRESH MATERIALIZED VIEW CONCURRENTLY user_summary;
```
- Create temporary table
- Populate temp table
- Atomically swap tables
- Allows concurrent reads during refresh

### 3. Incremental Refresh (Future)
```sql
REFRESH MATERIALIZED VIEW INCREMENTALLY user_summary;
```
- Track changes to base tables
- Apply only delta changes
- Much faster for large views
- Requires change tracking infrastructure

## Performance Considerations

### Benefits
- **Fast Queries**: No query re-execution
- **Consistent Results**: Snapshot of data at refresh time
- **Reduced Load**: Doesn't query base tables
- **Indexable**: Can add indexes to materialized data

### Costs
- **Storage**: Requires disk space for results
- **Staleness**: Data may be outdated until refresh
- **Refresh Time**: Can be expensive for large result sets
- **Maintenance**: Need to schedule refreshes

## Example Use Cases

### 1. Expensive Aggregations
```sql
-- Query takes 10 minutes on 1B rows
CREATE MATERIALIZED VIEW daily_stats AS
SELECT
    DATE(created_at) as day,
    COUNT(*) as total_orders,
    SUM(amount) as revenue
FROM orders
GROUP BY DATE(created_at);

-- Refresh nightly
REFRESH MATERIALIZED VIEW daily_stats;

-- Query in milliseconds
SELECT * FROM daily_stats WHERE day = CURRENT_DATE;
```

### 2. Complex Joins
```sql
-- Avoid expensive join on every query
CREATE MATERIALIZED VIEW user_profiles AS
SELECT
    u.id,
    u.name,
    u.email,
    p.bio,
    p.avatar_url,
    s.subscription_level
FROM users u
JOIN profiles p ON u.id = p.user_id
JOIN subscriptions s ON u.id = s.user_id;

-- Refresh hourly
```

### 3. Reporting Dashboards
```sql
-- Pre-compute dashboard metrics
CREATE MATERIALIZED VIEW dashboard_metrics AS
SELECT
    'users' as metric,
    COUNT(*) as value
FROM users
UNION ALL
SELECT
    'active_users' as metric,
    COUNT(*) as value
FROM users WHERE last_login > NOW() - INTERVAL '7 days'
UNION ALL
SELECT
    'revenue' as metric,
    SUM(amount) as value
FROM orders WHERE created_at >= DATE_TRUNC('month', NOW());

-- Refresh every 5 minutes
```

## Testing Strategy

### Unit Tests
1. CREATE MATERIALIZED VIEW parsing
2. REFRESH parsing (with/without CONCURRENTLY)
3. Physical table creation
4. Query execution (materialized vs regular)
5. DROP with cleanup

### Integration Tests
1. End-to-end materialized view lifecycle
2. Concurrent refresh without blocking reads
3. Multiple refreshes updating data
4. View dependencies (materialized view on another view)
5. Performance comparison (materialized vs regular)

## Future Enhancements

1. **Automatic Refresh** - Background job to refresh periodically
2. **Incremental Refresh** - Update only changed data
3. **Refresh Dependencies** - Cascade refresh to dependent views
4. **Staleness Warnings** - Alert when data is too old
5. **Partial Refresh** - Refresh specific partitions only

## References

- PostgreSQL Materialized Views: https://www.postgresql.org/docs/current/sql-creatematerializedview.html
- Oracle Materialized Views: https://docs.oracle.com/en/database/oracle/oracle-database/19/dwhsg/basic-materialized-views.html

## Current Status

✅ Catalog schema extended
✅ AST support added
⏳ Parser implementation (pending)
⏳ Bytecode generation (pending)
⏳ Executor implementation (pending)
⏳ Testing (pending)

**Estimated Effort**: 20-30 hours for full implementation

---

**Author**: Claude Code
**Date**: November 2025
**Status**: Foundation Complete - Implementation Deferred
