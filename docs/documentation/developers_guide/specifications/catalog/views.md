# Specification: Views and Materialized Views

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:564`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:556`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp`

## Synopsis

This specification defines view metadata storage, including regular views, materialized views, view refresh strategies, and security options (SECURITY DEFINER, SECURITY BARRIER).

## Scope

### In Scope

- View metadata structures (ViewInfo)
- Regular views vs materialized views
- Materialized view refresh strategies
- View security options
- View definition storage
- Materialized view base table tracking

### Out of Scope

- View query execution (see executor specs)
- View expansion/rewriting (see optimizer specs)
- Physical storage of materialized view data

## Specification

### Materialized View Refresh Strategies

**Source:** `include/scratchbird/core/catalog_manager.h:556`

```cpp
enum class MVRefreshStrategy : uint8_t {
    COMPLETE = 0,       // Full refresh - truncate and repopulate (default)
    INCREMENTAL = 1,    // Incremental refresh - only changed rows
    FAST = 2            // Fast refresh using change log
};
```

**Refresh Strategy Characteristics:**

| Strategy | Speed | Data Freshness | Complexity | Use Case |
|----------|-------|----------------|------------|----------|
| COMPLETE | Slow | Current | Simple | Small views, full refresh OK |
| INCREMENTAL | Medium | Near-current | Medium | Large views, change tracking |
| FAST | Fast | Stale (last refresh) | Complex | Real-time with change log |

### ViewInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:564`

```cpp
struct ViewInfo {
    // Identity
    ID view_id;                     // UUIDv7 view identifier
    ID schema_id;                   // Containing schema
    std::string name;               // View name
    bool name_is_delimited = false; // Quoted identifier flag
    ID owner_id;                    // Owner UUID
    
    // View definition
    std::string definition;         // SELECT query text
    std::vector<std::string> column_names;  // Optional explicit columns
    
    // Security options
    bool check_option = false;      // WITH CHECK OPTION
    bool security_definer = false;  // SECURITY DEFINER (vs INVOKER)
    bool security_barrier = false;  // SECURITY BARRIER
    
    // Metadata
    uint64_t created_time;
    uint64_t last_modified_time;
    
    // Temporary view support
    TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
    ID creating_session_id{};
    uint64_t creating_transaction_id = 0;
    
    // Materialized view attributes
    bool materialized = false;      // True if materialized view
    ID materialized_table_id;       // Physical table storing data
    uint64_t last_refresh_time;     // Timestamp of last REFRESH
    
    // Refresh options
    MVRefreshStrategy refresh_strategy;  // How to refresh
    bool refresh_on_commit = false;      // Auto-refresh on source changes
    std::vector<ID> base_table_ids;      // Tables this MV depends on
    ID change_log_table_id;              // Change tracking table
    bool supports_concurrent = true;     // Can refresh concurrently
    
    ViewInfo() : 
        check_option(false), 
        materialized(false), 
        last_refresh_time(0),
        refresh_strategy(MVRefreshStrategy::COMPLETE), 
        refresh_on_commit(false),
        supports_concurrent(true) {}
};
```

**Field Descriptions:**

| Field | Type | Description |
|-------|------|-------------|
| view_id | ID | UUIDv7 primary key |
| schema_id | ID | Parent schema |
| name | string | View name |
| definition | string | SELECT statement |
| check_option | bool | Enforce WHERE clause on INSERT/UPDATE |
| security_definer | bool | Execute with owner privileges |
| security_barrier | bool | Prevent optimizer pushdown |
| materialized | bool | Store results vs compute on query |
| materialized_table_id | ID | Table ID for MV data |
| refresh_strategy | MVRefreshStrategy | COMPLETE/INCREMENTAL/FAST |
| refresh_on_commit | bool | Auto-refresh on source changes |
| supports_concurrent | bool | Allow reads during refresh |

### View Types

```sql
-- Regular view
CREATE VIEW active_customers AS
SELECT customer_id, name, email
FROM customers
WHERE status = 'ACTIVE';

-- View with explicit columns
CREATE VIEW customer_summary (id, name, order_count) AS
SELECT c.customer_id, c.name, COUNT(o.order_id)
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.name;

-- View with CHECK OPTION
CREATE VIEW us_customers AS
SELECT * FROM customers WHERE country = 'US'
WITH CHECK OPTION;  -- Prevents inserting non-US customers

-- SECURITY DEFINER view
CREATE VIEW sensitive_data AS
SELECT * FROM internal_data
WITH SECURITY DEFINER;  -- Runs with view owner's privileges
```

### Materialized Views

```sql
-- Basic materialized view
CREATE MATERIALIZED VIEW monthly_sales AS
SELECT 
    DATE_TRUNC('month', order_date) as month,
    SUM(total_amount) as revenue
FROM orders
GROUP BY DATE_TRUNC('month', order_date);

-- Materialized view with refresh strategy
CREATE MATERIALIZED VIEW daily_stats 
REFRESH INCREMENTAL ON COMMIT AS
SELECT 
    DATE(order_date) as day,
    COUNT(*) as order_count
FROM orders
GROUP BY DATE(order_date);

-- Refresh commands
REFRESH MATERIALIZED VIEW monthly_sales;           -- Complete refresh
REFRESH MATERIALIZED VIEW CONCURRENTLY monthly_sales;  -- Concurrent refresh
```

### sb_views Catalog Table

```cpp
struct ViewRecord {
    // Primary key
    ID view_id;
    
    // Identity
    ID schema_id;
    char name[512];
    ID owner_id;
    uint8_t name_is_delimited;
    uint8_t reserved[7];
    
    // View definition (stored in TOAST if large)
    ID definition_oid;              // TOAST reference for definition
    char column_names[2048];        // Inline column list or TOAST ref
    
    // Security flags
    uint8_t check_option;
    uint8_t security_definer;
    uint8_t security_barrier;
    uint8_t reserved2[5];
    
    // Materialized view flags
    uint8_t materialized;
    uint8_t refresh_strategy;       // MVRefreshStrategy
    uint8_t refresh_on_commit;
    uint8_t supports_concurrent;
    uint8_t reserved3[4];
    
    // Materialized view storage
    ID materialized_table_id;
    uint64_t last_refresh_time;
    
    // Base table dependencies
    uint16_t base_table_count;
    ID base_table_ids[16];          // Up to 16 base tables
    ID change_log_table_id;         // For FAST refresh
    
    // Temporary view support
    uint8_t temp_metadata_scope;
    uint8_t reserved4[7];
    ID creating_session_id;
    uint64_t creating_transaction_id;
    
    // Metadata
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Security Options

**SECURITY DEFINER vs INVOKER:**

```sql
-- SECURITY DEFINER (default in some databases)
-- View executes with privileges of the view owner
CREATE VIEW restricted_view AS SELECT * FROM sensitive_table
WITH SECURITY DEFINER;

-- User only needs SELECT on view, not on underlying table
-- Access control at view level

-- SECURITY INVOKER (default in ScratchBird)
-- View executes with privileges of the current user
CREATE VIEW open_view AS SELECT * FROM public_table
WITH SECURITY INVOKER;

-- User needs SELECT on both view and underlying table
```

**SECURITY BARRIER:**

```sql
-- SECURITY BARRIER prevents optimizer from pushing predicates
-- into the view query, ensuring security checks happen first
CREATE VIEW secure_user_data AS
SELECT * FROM user_data WHERE user_id = CURRENT_USER_ID()
WITH SECURITY BARRIER;

-- Without barrier, optimizer might apply WHERE before filtering
-- With barrier, filter is always applied first
```

### Materialized View Refresh

**Complete Refresh:**
```
1. TRUNCATE materialized_table
2. INSERT INTO materialized_table
   EXECUTE view_definition
3. UPDATE last_refresh_time
```

**Incremental Refresh:**
```
1. Identify changed rows in base tables
2. DELETE changed rows from materialized_table
3. INSERT new/changed rows
4. UPDATE last_refresh_time
```

**Concurrent Refresh:**
```
1. Create new physical table with refreshed data
2. In single transaction:
   a. Drop old materialized_table
   b. Rename new table to materialized_table
3. No blocking of reads during refresh
```

## Algorithms

### Algorithm: Create View

```
Input:  Schema ID, view name, SELECT definition, options
Output: View ID

1. Parse and validate SELECT definition
2. Extract column names from SELECT
3. If column names explicitly provided:
   a. Verify count matches SELECT columns
   b. Use provided names
4. Identify base tables from SELECT
5. Generate UUIDv7 for view_id
6. If materialized:
   a. Create physical table for MV data
   b. Set materialized_table_id
   c. Execute SELECT to populate
7. Create ViewRecord
8. Create dependencies on base tables
9. Commit transaction
```

### Algorithm: Refresh Materialized View

```
Input:  View ID, refresh strategy, concurrent flag
Output: Success/Failure

1. Look up view info
2. If !materialized: error

3. If concurrent:
   a. Create temporary table
   b. Populate with fresh data
   c. Atomic swap: drop old, rename new
   
4. If !concurrent:
   a. Acquire exclusive lock on materialized_table
   b. Switch on refresh_strategy:
      
      case COMPLETE:
        - TRUNCATE materialized_table
        - INSERT ... SELECT from view definition
      
      case INCREMENTAL:
        - Get changed row IDs from base tables
        - DELETE changed rows from MV
        - INSERT new/changed rows
      
      case FAST:
        - Read change_log_table
        - Apply changes to MV
        - Clear processed changes
   
   c. Release lock

5. UPDATE last_refresh_time
6. Commit transaction
```

### Algorithm: Expand View

```
Input:  Query containing view reference
Output: Query with view expanded

1. Identify view references in FROM clause
2. For each view:
   a. Look up view definition
   b. Parse view SELECT
   c. Replace view reference with subquery:
      
      -- Original
      SELECT * FROM active_customers WHERE name LIKE 'A%';
      
      -- Expanded
      SELECT * FROM (
          SELECT customer_id, name, email
          FROM customers
          WHERE status = 'ACTIVE'
      ) AS active_customers
      WHERE name LIKE 'A%';
   
   d. If security_definer:
      - Wrap with privilege check
   e. If check_option and INSERT/UPDATE:
      - Add validation

3. Return expanded query
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `VIEW_INV_001` | view_id is valid UUIDv7 | isUuidV7Local() check |
| `VIEW_INV_002` | schema_id references valid schema | Foreign key check |
| `VIEW_INV_003` | Definition parses as valid SELECT | Parser validation |
| `VIEW_INV_004` | Base tables exist | Dependency validation |
| `VIEW_INV_005` | MV has valid materialized_table_id | Referential check |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `VIEW_EXISTS` | Name conflict | Choose different name |
| `INVALID_DEFINITION` | Invalid SELECT | Fix definition |
| `MATERIALIZED_NO_TABLE` | MV missing storage table | Recreate MV |
| `REFRESH_FAILED` | Refresh error | Check base tables |
| `CIRCULAR_VIEW` | View references itself | Fix definition |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_views.cpp` | View CRUD |
| `tests/unit/test_materialized_views.cpp` | MV operations |
| `tests/unit/test_view_expansion.cpp` | Query expansion |
| `tests/unit/test_view_security.cpp` | Security options |

## Related Specifications

- [tables.md](./tables.md) - MV storage tables
- [dependency_tracking.md](./dependency_tracking.md) - View dependencies
- [invalidation.md](./invalidation.md) - MV cache invalidation

## Appendix

### View Record Size

| Component | Size |
|-----------|------|
| Header | 48 bytes |
| Identity | 544 bytes |
| Security flags | 8 bytes |
| MV fields | 32 bytes |
| Dependencies | 136 bytes |
| Temporary | 24 bytes |
| Metadata | 16 bytes |
| **Total** | **~808 bytes** |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
