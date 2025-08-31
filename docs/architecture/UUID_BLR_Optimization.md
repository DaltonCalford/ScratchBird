# UUID-Based BLR Optimization Architecture

## Overview

ScratchBird uses a dual-layer approach: hierarchical paths for human interaction, UUIDs for machine execution. This provides intuitive navigation without runtime overhead.

## Two-Layer Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   User/SQL Layer                             │
│         (Hierarchical Paths, Names, Navigation)              │
│     [root].[app].[accounting].[acct_foo].customers           │
├─────────────────────────────────────────────────────────────┤
│                  Parse & Resolution Layer                    │
│            (Path → UUID Translation, Caching)                │
│     Resolves to UUID: 7f3e4a92-8b1c-4d5e-9f2a-1b3c4d5e6f7a  │
├─────────────────────────────────────────────────────────────┤
│                      BLR Layer                               │
│              (UUID-Only References, No Paths)                │
│     SCAN_TABLE 0x7f3e4a928b1c4d5e9f2a1b3c4d5e6f7a          │
├─────────────────────────────────────────────────────────────┤
│                   Execution Engine                           │
│            (Direct UUID → Object Lookup, O(1))               │
└─────────────────────────────────────────────────────────────┘
```

## UUID Resolution Architecture

### Parse-Time Resolution (Once)

```cpp
namespace scratchbird::parser {

class SQLToBLRCompiler {
private:
    // Cache for path → UUID mappings
    struct PathCache {
        LRUCache<string, UUID> path_to_uuid;
        map<UUID, ObjectMetadata> uuid_metadata;
        
        // Statistics for adaptive caching
        map<string, size_t> access_count;
        map<string, timestamp> last_access;
    };
    
    PathCache cache;
    
public:
    BLRProgram compile_sql(const string& sql, const ParseContext& context) {
        AST ast = parse_sql(sql);
        BLRProgram blr;
        
        // Resolve all object references to UUIDs at parse time
        for (auto& node : ast.nodes) {
            if (node.type == NodeType::TABLE_REF) {
                UUID table_uuid = resolve_table_to_uuid(node.table_path);
                
                // BLR contains only UUID, no path
                blr.add_instruction(OpCode::SCAN_TABLE, table_uuid);
                
            } else if (node.type == NodeType::COLUMN_REF) {
                UUID column_uuid = resolve_column_to_uuid(
                    node.table_path, 
                    node.column_name
                );
                
                blr.add_instruction(OpCode::LOAD_COLUMN, column_uuid);
            }
        }
        
        return blr;
    }
    
private:
    UUID resolve_table_to_uuid(const string& table_path) {
        // Check cache first - O(1) for hot paths
        if (auto cached = cache.path_to_uuid.get(table_path)) {
            cache.access_count[table_path]++;
            cache.last_access[table_path] = now();
            return *cached;
        }
        
        // Not in cache - do full resolution
        SchemaNode* schema = schema_resolver.resolve_schema(table_path);
        TableDescriptor* table = catalog.get_table(schema->schema_id, table_name);
        
        UUID table_uuid = table->table_uuid;
        
        // Cache for future use
        cache.path_to_uuid.put(table_path, table_uuid);
        
        // Adaptive caching - promote frequently used paths
        if (cache.access_count[table_path] > HOT_PATH_THRESHOLD) {
            cache.path_to_uuid.pin(table_path);  // Keep in cache
        }
        
        return table_uuid;
    }
};

} // namespace scratchbird::parser
```

### BLR Structure with UUIDs

```cpp
namespace scratchbird::blr {

// BLR instructions use UUIDs exclusively
struct BLRInstruction {
    OpCode opcode;
    union {
        UUID object_uuid;      // For tables, columns, procedures
        int64_t int_value;     // For constants
        double float_value;    // For constants
    };
    vector<UUID> column_uuids;  // For multi-column operations
};

// Example BLR program (binary representation)
class BLRProgram {
    vector<BLRInstruction> instructions;
    
    // Example: SELECT name, salary FROM employees WHERE dept_id = 10
    // After compilation:
    // [
    //   {SCAN_TABLE, uuid_employees},
    //   {FILTER_START},
    //   {LOAD_COLUMN, uuid_dept_id},
    //   {LOAD_CONST, 10},
    //   {COMPARE_EQ},
    //   {FILTER_END},
    //   {PROJECT, [uuid_name, uuid_salary]}
    // ]
    // Note: No string paths anywhere in BLR!
};

} // namespace scratchbird::blr
```

### Execution with Direct UUID Lookup

```cpp
namespace scratchbird::engine {

class BLRExecutor {
private:
    // UUID → Object direct mapping (O(1) lookup)
    struct ObjectRegistry {
        unordered_map<UUID, TableDescriptor*> tables;
        unordered_map<UUID, ColumnDescriptor*> columns;
        unordered_map<UUID, ProcedureDescriptor*> procedures;
        unordered_map<UUID, IndexDescriptor*> indexes;
    };
    
    ObjectRegistry registry;
    
public:
    ExecutionResult execute(const BLRProgram& blr) {
        for (const auto& instruction : blr.instructions) {
            switch (instruction.opcode) {
                case OpCode::SCAN_TABLE: {
                    // Direct O(1) lookup - no path traversal!
                    TableDescriptor* table = registry.tables[instruction.object_uuid];
                    scan_table(table);
                    break;
                }
                
                case OpCode::LOAD_COLUMN: {
                    // Direct O(1) lookup
                    ColumnDescriptor* column = registry.columns[instruction.object_uuid];
                    load_column(column);
                    break;
                }
                
                case OpCode::CALL_PROCEDURE: {
                    // Direct O(1) lookup
                    ProcedureDescriptor* proc = registry.procedures[instruction.object_uuid];
                    
                    // Stored procedures already have BLR with UUIDs
                    execute(proc->compiled_blr);
                    break;
                }
            }
        }
    }
};

} // namespace scratchbird::engine
```

## Adaptive Path Cache

```cpp
class AdaptivePathCache {
private:
    struct CacheEntry {
        UUID object_uuid;
        size_t hit_count = 0;
        timestamp last_access;
        size_t resolution_cost_ms;  // How long it took to resolve
        bool is_pinned = false;     // Keep in cache
    };
    
    // Multi-level cache
    LRUCache<string, CacheEntry> l1_cache;  // Hot paths (in-memory)
    LRUCache<string, CacheEntry> l2_cache;  // Warm paths (in-memory)
    PersistentCache<string, UUID> l3_cache; // Cold paths (on-disk)
    
    // Statistics for optimization
    struct CacheStats {
        atomic<size_t> total_lookups{0};
        atomic<size_t> l1_hits{0};
        atomic<size_t> l2_hits{0};
        atomic<size_t> l3_hits{0};
        atomic<size_t> misses{0};
        
        double get_hit_rate() const {
            return double(l1_hits + l2_hits + l3_hits) / total_lookups;
        }
    };
    
    CacheStats stats;
    
public:
    UUID resolve_path(const string& path) {
        stats.total_lookups++;
        
        // L1 - Hot paths
        if (auto entry = l1_cache.get(path)) {
            stats.l1_hits++;
            entry->hit_count++;
            entry->last_access = now();
            return entry->object_uuid;
        }
        
        // L2 - Warm paths
        if (auto entry = l2_cache.get(path)) {
            stats.l2_hits++;
            entry->hit_count++;
            
            // Promote to L1 if accessed frequently
            if (entry->hit_count > L1_PROMOTION_THRESHOLD) {
                l1_cache.put(path, *entry);
            }
            
            return entry->object_uuid;
        }
        
        // L3 - Cold paths (disk)
        if (auto uuid = l3_cache.get(path)) {
            stats.l3_hits++;
            
            // Promote to L2
            CacheEntry entry{*uuid, 1, now(), 0};
            l2_cache.put(path, entry);
            
            return *uuid;
        }
        
        // Miss - resolve and cache
        stats.misses++;
        auto start = high_resolution_clock::now();
        
        UUID uuid = full_path_resolution(path);
        
        auto resolution_time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start
        ).count();
        
        // Add to appropriate cache level based on resolution cost
        CacheEntry entry{uuid, 1, now(), resolution_time};
        
        if (resolution_time > EXPENSIVE_RESOLUTION_THRESHOLD) {
            // Expensive to resolve - keep in L1
            entry.is_pinned = true;
            l1_cache.put(path, entry);
        } else {
            // Normal resolution - start in L2
            l2_cache.put(path, entry);
        }
        
        // Always persist to L3
        l3_cache.put(path, uuid);
        
        return uuid;
    }
    
    void preload_common_paths() {
        // Preload frequently used system paths
        vector<string> common_paths = {
            "[root].[sys]",
            "[root].[sys].tables",
            "[root].[sys].columns",
            "[root].[sys].procedures",
            "[root].[sec].users",
            "[root].[sec].roles"
        };
        
        for (const auto& path : common_paths) {
            UUID uuid = full_path_resolution(path);
            CacheEntry entry{uuid, 0, now(), 0, true};  // Pinned
            l1_cache.put(path, entry);
        }
    }
    
    void analyze_and_optimize() {
        // Run periodically to optimize cache
        
        // Find patterns in missed paths
        auto missed_patterns = analyze_miss_patterns();
        
        // Preload paths matching common patterns
        for (const auto& pattern : missed_patterns) {
            preload_pattern(pattern);
        }
        
        // Adjust cache sizes based on hit rates
        if (stats.get_hit_rate() < TARGET_HIT_RATE) {
            increase_cache_sizes();
        }
        
        // Persist hot paths for next startup
        persist_hot_paths();
    }
};
```

## Benefits of UUID-Based BLR

### 1. **Zero Runtime Path Resolution**
```cpp
// Traditional (path-based):
for (each row) {
    resolve_path("[root].[app].[crm].customers");  // Expensive!
    access_table();
}

// ScratchBird (UUID-based):
UUID table_uuid = ...; // Resolved once at parse time
for (each row) {
    access_table(table_uuid);  // O(1) direct access
}
```

### 2. **Rename Operations Are Free**
```sql
-- Rename table - only updates catalog, not BLR
ALTER TABLE [root].[app].[crm].customers 
RENAME TO [root].[app].[crm].clients;

-- All stored procedures/views still work!
-- Their BLR contains UUID, not the name
```

### 3. **Schema Reorganization Without Breaking Code**
```sql
-- Move entire schema tree
ALTER SCHEMA [root].[app].[old_location]
MOVE TO [root].[app].[new_location];

-- All compiled BLR still works - uses UUIDs
```

### 4. **Federation Optimization**
```cpp
// Remote objects get local UUID aliases
struct RemoteObjectMapping {
    UUID local_uuid;      // Used in BLR
    string remote_server;
    string remote_path;
    
    // Resolution happens once, then cached
};
```

## Stored Procedure Optimization

```sql
CREATE PROCEDURE calculate_bonus(emp_id INTEGER)
AS BEGIN
    -- This SQL is parsed once to BLR with UUIDs
    SELECT salary * 0.1 
    FROM [root].[app].[hr].employees 
    WHERE id = emp_id;
END;

-- Stored BLR (conceptual):
-- [
--   {PARAM_LOAD, 0},  -- emp_id parameter
--   {SCAN_TABLE, 0x7f3e4a92...},  -- employees UUID
--   {FILTER_START},
--   {LOAD_COLUMN, 0x8a9b2c3d...},  -- id column UUID
--   {PARAM_LOAD, 0},
--   {COMPARE_EQ},
--   {FILTER_END},
--   {LOAD_COLUMN, 0x9c8d7e6f...},  -- salary column UUID
--   {LOAD_CONST, 0.1},
--   {MULTIPLY},
--   {RETURN}
-- ]
```

## Cache Statistics and Monitoring

```sql
-- View cache performance
SELECT * FROM [root].[sys].path_cache_stats;

-- Results:
-- total_lookups | l1_hits | l2_hits | l3_hits | misses | hit_rate
-- 1000000      | 950000  | 40000   | 8000    | 2000   | 99.8%

-- View hot paths
SELECT path, hit_count, last_access, is_pinned
FROM [root].[sys].path_cache_entries
WHERE cache_level = 'L1'
ORDER BY hit_count DESC
LIMIT 20;

-- Force cache preload
CALL [root].[sys].preload_path_cache('[root].[app].[production]');

-- Clear cache (admin only)
CALL [root].[sys].clear_path_cache();
```

## Performance Impact

### Without UUID Optimization
- Every query: Path resolution O(log n) per object
- Stored procedures: Re-resolve paths every execution
- Joins: Path resolution for each joined table
- **Total overhead**: 10-30% of query time

### With UUID Optimization  
- Parse time: One-time path resolution
- Execution: Direct O(1) UUID lookup
- Stored procedures: Zero resolution (pre-compiled BLR)
- Joins: Direct UUID access
- **Total overhead**: < 0.1% of query time

## Implementation Priority

1. **Phase 1**: UUID registry in engine
2. **Phase 2**: BLR compiler with UUID resolution
3. **Phase 3**: Basic path cache (L1 only)
4. **Phase 4**: Multi-level adaptive cache
5. **Phase 5**: Cache analytics and optimization

This UUID-based BLR approach gives us the best of both worlds: intuitive hierarchical navigation for humans, and lightning-fast UUID-based execution for the machine!