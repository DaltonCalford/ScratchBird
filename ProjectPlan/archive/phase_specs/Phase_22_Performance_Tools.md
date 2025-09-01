# Phase 22: Performance and Monitoring Tools

## Objective
Implement performance monitoring and tuning tools.

## Prerequisites
- Phase 21 complete (advanced features)

## Tasks

### 22.1 Performance Schema
```sql
SDB$STATS_QUERIES (query_id, text, calls, total_time, mean_time)
SDB$STATS_TABLES (table_id, seq_scans, index_scans, inserts, updates, deletes)
SDB$STATS_INDEXES (index_id, scans, tuples_read, tuples_fetched)
```

### 22.2 Query Profiling
```sql
SET profiling = ON;
-- Run queries
SHOW PROFILES;
SHOW PROFILE FOR QUERY 1;
```

### 22.3 Lock Monitoring
```sql
SELECT * FROM SDB$LOCKS;
SELECT * FROM SDB$LOCK_WAITS;
```

### 22.4 Vacuum and Analyze
```sql
VACUUM table_name;
VACUUM FULL table_name;
ANALYZE table_name;
```

### 22.5 Configuration Tuning
```cpp
struct Config {
    size_t shared_buffers;
    size_t work_mem;
    size_t maintenance_work_mem;
    int max_connections;
    int checkpoint_timeout;
};
```

## Files to Create/Modify
- `src/engine/stats_collector.cpp`
- `src/engine/vacuum.cpp`
- `src/engine/config_manager.cpp`

## Validation Tests
```cpp
// Enable statistics
config.set("track_statistics", true);

// Run workload
for(int i = 0; i < 1000; i++) {
    execute("SELECT * FROM users WHERE id = ?", {i});
}

// Check statistics
auto stats = execute("SELECT * FROM SDB$STATS_QUERIES ORDER BY calls DESC");
assert(stats.rows[0]["calls"] == "1000");

// Vacuum test
execute("DELETE FROM large_table WHERE id < 1000");
auto size_before = get_table_size("large_table");
execute("VACUUM large_table");
auto size_after = get_table_size("large_table");
assert(size_after < size_before);

// Lock monitoring
// Start long transaction in thread 1
// Try conflicting operation in thread 2
auto locks = execute("SELECT * FROM SDB$LOCK_WAITS");
assert(locks.rows.size() > 0);
```

## Exit Criteria
- Statistics accurately tracked
- Vacuum reclaims space
- Lock conflicts visible
- Performance improves with tuning