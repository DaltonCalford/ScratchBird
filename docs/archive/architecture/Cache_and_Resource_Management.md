# Cache and Resource Management Architecture

## Multi-Level Cache Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Client Applications                      │
├─────────────────────────────────────────────────────────────┤
│                  Result Cache (Y-Valve Level)                │
│              (Query → Result Set mappings)                   │
├─────────────────────────────────────────────────────────────┤
│                    Connection Pool Manager                   │
│                  (NEW LAYER - Dedicated)                     │
├─────────────────────────────────────────────────────────────┤
│                         Y-Valve Router                       │
│                    (BLR Cache - Parsed SQL)                  │
├─────────────────────────────────────────────────────────────┤
│                      Parser Plugin Layer                     │
├─────────────────────────────────────────────────────────────┤
│                      Execution Engine                        │
│                  (Plan Cache - Optimized Plans)              │
├─────────────────────────────────────────────────────────────┤
│                    Buffer Pool Manager                       │
│            (Page Cache - Database-Aware Caching)             │
├─────────────────────────────────────────────────────────────┤
│                       Storage Engine                         │
│                    (Direct I/O - No OS Cache)                │
└─────────────────────────────────────────────────────────────┘
```

## 1. Database-Controlled Buffer Management

### The Problem with OS Caching

```cpp
// OS doesn't understand database semantics:
// - Doesn't know page importance (root index vs leaf data)
// - Caches garbage collection pages unnecessarily
// - Can't prioritize based on query patterns
// - Double buffering wastes memory
```

### Firebird SuperServer-Style Shared Cache

```cpp
namespace scratchbird::cache {

class SharedBufferPool {
private:
    struct PageInfo {
        PageId page_id;
        uint8_t* data;
        
        // Metadata for intelligent caching
        PageType type;           // INDEX_ROOT, INDEX_LEAF, DATA, TIP, etc.
        uint32_t pin_count;      // Currently in use
        uint32_t reference_count; // Historical usage
        timestamp last_access;
        timestamp last_modified;
        bool is_dirty;
        bool is_garbage;         // Marked for GC
        
        // Multi-version support
        TransactionId oldest_interested_transaction;
        vector<PageVersion> versions;  // For MVCC
    };
    
    // Intelligent page replacement
    class AdaptiveReplacementCache {
        // Combines LRU, LFU, and database-specific heuristics
        list<PageId> t1;  // Recent pages (LRU)
        list<PageId> t2;  // Frequent pages (LFU)
        list<PageId> b1;  // Ghost list for t1
        list<PageId> b2;  // Ghost list for t2
        
        size_t target_t1_size;  // Adaptive parameter
        
    public:
        PageId select_victim() {
            // Never evict:
            // - Pinned pages
            // - Index root pages
            // - Active transaction pages
            // - Hot pages
            
            // Prefer evicting:
            // - Garbage collection pages
            // - Old version pages
            // - Sequential scan pages (unless marked keep)
            
            if (should_evict_from_t1()) {
                return evict_from_t1();
            } else {
                return evict_from_t2();
            }
        }
        
        void adapt_on_hit(PageId page, bool was_in_b1, bool was_in_b2) {
            // Adjust target_t1_size based on ghost list hits
            if (was_in_b1) {
                // Recent page was needed again
                target_t1_size = min(target_t1_size + 1, pool_size);
            } else if (was_in_b2) {
                // Frequent page was needed
                target_t1_size = max(target_t1_size - 1, 0);
            }
        }
    };
    
    // Shared memory segment for all connections
    shared_memory_object shm;
    mapped_region region;
    
    // Page storage
    unordered_map<PageId, PageInfo> pages;
    AdaptiveReplacementCache arc;
    
    // Synchronization
    shared_mutex cache_mutex;  // Multiple readers, single writer
    
    // Direct I/O to bypass OS cache
    int fd_flags = O_DIRECT | O_SYNC;
    
public:
    Page* fetch_page(PageId page_id, AccessMode mode) {
        // Try shared lock first for reads
        if (mode == AccessMode::READ) {
            shared_lock lock(cache_mutex);
            
            auto it = pages.find(page_id);
            if (it != pages.end()) {
                // Page in cache
                it->second.reference_count++;
                it->second.last_access = now();
                arc.record_hit(page_id);
                return it->second.data;
            }
        }
        
        // Need exclusive lock for cache miss or write
        unique_lock lock(cache_mutex);
        
        // Double-check after acquiring exclusive lock
        auto it = pages.find(page_id);
        if (it != pages.end()) {
            return handle_cached_page(it->second, mode);
        }
        
        // Cache miss - need to load
        return load_page_into_cache(page_id, mode);
    }
    
    void configure_cache_priorities() {
        // Database-specific cache priorities
        set_priority(PageType::INDEX_ROOT, Priority::NEVER_EVICT);
        set_priority(PageType::INDEX_INTERNAL, Priority::HIGH);
        set_priority(PageType::TIP, Priority::HIGH);  // Transaction Inventory
        set_priority(PageType::HEADER, Priority::HIGH);
        set_priority(PageType::DATA, Priority::NORMAL);
        set_priority(PageType::BLOB, Priority::LOW);
        set_priority(PageType::GARBAGE, Priority::EVICT_FIRST);
    }
    
    void prefetch_for_query(const QueryPlan& plan) {
        // Intelligent prefetching based on query plan
        for (const auto& operation : plan.operations) {
            if (operation.type == OpType::INDEX_SCAN) {
                prefetch_index_pages(operation.index_id);
            } else if (operation.type == OpType::SEQUENTIAL_SCAN) {
                start_async_sequential_prefetch(operation.table_id);
            }
        }
    }
};

} // namespace scratchbird::cache
```

### Direct I/O Implementation

```cpp
class DirectIOManager {
private:
    // Bypass OS cache completely
    static constexpr int BLOCK_SIZE = 8192;  // Must be aligned
    
    // Aligned memory allocation for Direct I/O
    void* allocate_aligned_buffer(size_t size) {
        void* buffer;
        if (posix_memalign(&buffer, BLOCK_SIZE, size) != 0) {
            throw bad_alloc();
        }
        return buffer;
    }
    
public:
    void read_page_direct(int fd, PageId page_id, void* buffer) {
        off_t offset = page_id * BLOCK_SIZE;
        
        // Must be aligned for O_DIRECT
        if (pread(fd, buffer, BLOCK_SIZE, offset) != BLOCK_SIZE) {
            throw io_error("Direct I/O read failed");
        }
    }
    
    void write_page_direct(int fd, PageId page_id, const void* buffer) {
        off_t offset = page_id * BLOCK_SIZE;
        
        // Aligned write
        if (pwrite(fd, buffer, BLOCK_SIZE, offset) != BLOCK_SIZE) {
            throw io_error("Direct I/O write failed");
        }
    }
};
```

## 2. Connection Pool Manager (New Dedicated Layer)

```cpp
namespace scratchbird::pool {

// Dedicated connection pool layer between Y-Valve and Engine
class ConnectionPoolManager {
private:
    struct PooledConnection {
        SessionHandle* session;
        ConnectionState state;
        timestamp last_used;
        string last_database;
        TransactionState txn_state;
        map<string, PreparedHandle*> prepared_statements;
        
        void reset() {
            // Reset connection to clean state
            if (txn_state == TransactionState::ACTIVE) {
                engine->rollback(session);
            }
            // Clear session variables
            engine->reset_session(session);
            // Keep prepared statements cached
        }
    };
    
    struct PoolConfig {
        size_t min_connections = 10;
        size_t max_connections = 100;
        duration idle_timeout = 30min;
        duration connection_lifetime = 2h;
        bool pre_warm = true;
    };
    
    // Separate pools per database
    map<string, vector<unique_ptr<PooledConnection>>> pools;
    
    // Pool statistics
    struct PoolStats {
        atomic<size_t> total_connections{0};
        atomic<size_t> active_connections{0};
        atomic<size_t> idle_connections{0};
        atomic<size_t> connections_created{0};
        atomic<size_t> connections_reused{0};
        atomic<size_t> wait_time_ms{0};
    };
    
    map<string, PoolStats> stats;
    
public:
    // Y-Valve requests a connection
    SessionHandle* acquire_connection(const string& database, const ConnectionParams& params) {
        auto& pool = pools[database];
        
        // Try to find idle connection
        for (auto& conn : pool) {
            if (conn->state == ConnectionState::IDLE) {
                conn->state = ConnectionState::ACTIVE;
                conn->reset();
                stats[database].connections_reused++;
                return conn->session;
            }
        }
        
        // Create new if under limit
        if (pool.size() < config.max_connections) {
            auto conn = create_new_connection(database);
            pool.push_back(move(conn));
            stats[database].connections_created++;
            return pool.back()->session;
        }
        
        // Wait for available connection
        return wait_for_connection(database, params.timeout);
    }
    
    void release_connection(SessionHandle* session) {
        // Find and mark as idle
        for (auto& [db, pool] : pools) {
            for (auto& conn : pool) {
                if (conn->session == session) {
                    conn->state = ConnectionState::IDLE;
                    conn->last_used = now();
                    stats[db].active_connections--;
                    stats[db].idle_connections++;
                    return;
                }
            }
        }
    }
    
    void maintain_pools() {
        // Background thread for pool maintenance
        while (running) {
            for (auto& [db, pool] : pools) {
                // Remove expired connections
                remove_expired_connections(pool);
                
                // Ensure minimum connections
                while (pool.size() < config.min_connections) {
                    pool.push_back(create_new_connection(db));
                }
                
                // Pre-warm connections
                if (config.pre_warm) {
                    warm_connections(pool);
                }
            }
            
            sleep_for(1min);
        }
    }
    
    PoolStats get_statistics(const string& database) {
        return stats[database];
    }
};

} // namespace scratchbird::pool
```

## 3. Result Set Cache (Y-Valve Level)

```cpp
namespace scratchbird::yvalve {

class ResultSetCache {
private:
    struct CachedResult {
        ResultSet data;
        timestamp cached_at;
        duration ttl;
        size_t memory_size;
        uint64_t hash;
        
        // Invalidation tracking
        set<TableId> dependent_tables;
        TransactionId cached_at_txn;
        
        bool is_valid() const {
            return (now() - cached_at) < ttl;
        }
    };
    
    // LRU cache with size limit
    class SizeLimitedLRU {
        list<pair<string, CachedResult>> items;
        unordered_map<string, list<pair<string, CachedResult>>::iterator> index;
        size_t max_memory;
        size_t current_memory;
        
    public:
        optional<ResultSet> get(const string& key) {
            auto it = index.find(key);
            if (it == index.end()) {
                return nullopt;
            }
            
            // Move to front (most recently used)
            items.splice(items.begin(), items, it->second);
            
            // Check validity
            if (!it->second->second.is_valid()) {
                evict(key);
                return nullopt;
            }
            
            return it->second->second.data;
        }
        
        void put(const string& key, CachedResult result) {
            // Evict if necessary
            while (current_memory + result.memory_size > max_memory && !items.empty()) {
                evict_lru();
            }
            
            items.push_front({key, result});
            index[key] = items.begin();
            current_memory += result.memory_size;
        }
    };
    
    SizeLimitedLRU cache;
    
    // Invalidation subscriptions
    map<TableId, set<string>> table_to_queries;
    
public:
    optional<ResultSet> get_cached_result(const string& query, const Parameters& params) {
        string cache_key = generate_cache_key(query, params);
        return cache.get(cache_key);
    }
    
    void cache_result(const string& query, const Parameters& params, 
                     const ResultSet& result, const QueryPlan& plan) {
        // Don't cache if:
        // - Query modifies data
        // - Result set too large
        // - Contains volatile functions (NOW(), RANDOM())
        
        if (!should_cache(query, result, plan)) {
            return;
        }
        
        CachedResult cached{
            .data = result,
            .cached_at = now(),
            .ttl = determine_ttl(plan),
            .memory_size = calculate_size(result),
            .dependent_tables = extract_tables(plan)
        };
        
        string cache_key = generate_cache_key(query, params);
        cache.put(cache_key, cached);
        
        // Track for invalidation
        for (TableId table : cached.dependent_tables) {
            table_to_queries[table].insert(cache_key);
        }
    }
    
    void invalidate_table(TableId table) {
        // Invalidate all cached results that depend on this table
        if (table_to_queries.count(table)) {
            for (const string& query : table_to_queries[table]) {
                cache.evict(query);
            }
            table_to_queries[table].clear();
        }
    }
    
    duration determine_ttl(const QueryPlan& plan) {
        // Intelligent TTL based on query characteristics
        if (plan.accesses_system_tables()) {
            return 1h;  // System tables change rarely
        }
        if (plan.is_aggregate_only()) {
            return 5min;  // Aggregates moderately stable
        }
        if (plan.has_joins()) {
            return 30s;  // Joins change frequently
        }
        return 10s;  // Default conservative TTL
    }
};

} // namespace scratchbird::yvalve
```

## 4. Resource Governance (Engine Level)

```cpp
namespace scratchbird::engine {

class ResourceGovernor {
private:
    struct ResourceLimits {
        optional<size_t> max_memory;
        optional<duration> max_execution_time;
        optional<size_t> max_rows_returned;
        optional<size_t> max_temp_space;
        optional<size_t> max_parallel_degree;
    };
    
    struct ResourceTracking {
        atomic<size_t> memory_used{0};
        atomic<size_t> temp_space_used{0};
        atomic<size_t> rows_processed{0};
        time_point start_time;
        
        bool verbose_monitoring = false;
        vector<ResourceSnapshot> snapshots;  // When verbose
    };
    
    // Per-session or per-query tracking
    map<SessionId, ResourceTracking> session_resources;
    map<QueryId, ResourceTracking> query_resources;
    
    // Global resource pools
    atomic<size_t> total_memory_allocated{0};
    atomic<size_t> total_temp_space{0};
    
    bool monitoring_enabled = false;
    
public:
    void start_monitoring(SessionId session, bool verbose = false) {
        monitoring_enabled = true;
        session_resources[session].verbose_monitoring = verbose;
        session_resources[session].start_time = now();
    }
    
    void check_limits(SessionId session, QueryId query) {
        if (!monitoring_enabled) return;  // Fast path when not monitoring
        
        auto& tracking = query_resources[query];
        auto& limits = get_limits(session);
        
        // Memory limit
        if (limits.max_memory && tracking.memory_used > *limits.max_memory) {
            throw resource_exceeded_error("Memory limit exceeded");
        }
        
        // Time limit
        if (limits.max_execution_time) {
            auto elapsed = now() - tracking.start_time;
            if (elapsed > *limits.max_execution_time) {
                throw resource_exceeded_error("Execution time limit exceeded");
            }
        }
        
        // Row limit
        if (limits.max_rows_returned && tracking.rows_processed > *limits.max_rows_returned) {
            throw resource_exceeded_error("Row limit exceeded");
        }
    }
    
    ResourceReport get_resource_report(SessionId session) {
        auto& tracking = session_resources[session];
        
        ResourceReport report{
            .memory_used = tracking.memory_used.load(),
            .memory_peak = calculate_peak_memory(tracking),
            .temp_space_used = tracking.temp_space_used.load(),
            .execution_time = now() - tracking.start_time,
            .rows_processed = tracking.rows_processed.load()
        };
        
        if (tracking.verbose_monitoring) {
            // Include detailed snapshots
            report.timeline = tracking.snapshots;
            report.memory_breakdown = get_memory_breakdown(session);
            report.wait_stats = get_wait_statistics(session);
        }
        
        return report;
    }
    
    void allocate_memory(SessionId session, size_t bytes) {
        if (!monitoring_enabled) {
            // Fast path - just allocate
            return do_allocate(bytes);
        }
        
        // Tracked allocation
        auto& tracking = session_resources[session];
        tracking.memory_used += bytes;
        total_memory_allocated += bytes;
        
        if (tracking.verbose_monitoring) {
            tracking.snapshots.push_back({
                .timestamp = now(),
                .memory = tracking.memory_used.load(),
                .event = "memory_allocate",
                .details = to_string(bytes) + " bytes"
            });
        }
        
        check_limits(session, current_query_id());
    }
};

} // namespace scratchbird::engine
```

## 5. Utility Direct Engine Access

```cpp
namespace scratchbird::utilities {

// Backup utility - direct engine access, no parser needed
class BackupUtility {
private:
    IExecutionEngine* engine;
    BLRProgram backup_program;  // Pre-compiled BLR
    
public:
    BackupUtility() {
        // Load pre-compiled BLR for backup operations
        backup_program = load_compiled_blr("backup.blr");
    }
    
    void perform_backup(const string& database, const string& backup_path) {
        // Direct engine connection - no Y-Valve, no parser
        auto db_handle = engine->open_database(database);
        auto session = engine->create_session(db_handle);
        
        // Execute pre-compiled BLR
        Parameters params{backup_path};
        engine->execute_blr(session, backup_program, params);
        
        engine->destroy_session(session);
        engine->close_database(db_handle);
    }
};

// Replication utility
class ReplicationUtility {
private:
    IExecutionEngine* source_engine;
    IExecutionEngine* target_engine;
    
    // Pre-compiled BLR programs
    BLRProgram read_changes_blr;
    BLRProgram apply_changes_blr;
    
public:
    void replicate_changes() {
        // Direct engine access for both source and target
        auto changes = source_engine->execute_blr(
            source_session, 
            read_changes_blr,
            {last_replicated_txn}
        );
        
        target_engine->execute_blr(
            target_session,
            apply_changes_blr,
            {changes}
        );
    }
};

// Scheduled job executor
class JobScheduler {
private:
    struct ScheduledJob {
        string name;
        BLRProgram compiled_blr;  // Pre-compiled from SQL
        CronExpression schedule;
        SessionHandle* dedicated_session;
    };
    
    vector<ScheduledJob> jobs;
    
public:
    void create_job(const string& name, const string& sql, const string& schedule) {
        // Parse SQL to BLR ONCE when job is created
        ParseContext ctx;
        BLRProgram blr = sql_parser->parse(sql, ctx);
        
        jobs.push_back({
            .name = name,
            .compiled_blr = blr,
            .schedule = parse_cron(schedule),
            .dedicated_session = engine->create_session(database)
        });
    }
    
    void execute_job(ScheduledJob& job) {
        // Direct BLR execution - no parsing needed
        engine->execute_blr(job.dedicated_session, job.compiled_blr);
    }
};

} // namespace scratchbird::utilities
```

## Architecture Summary

### Cache Hierarchy
1. **Result Cache** (Y-Valve) - Complete query results
2. **BLR Cache** (Y-Valve) - Parsed SQL → BLR mappings
3. **Plan Cache** (Engine) - Optimized execution plans
4. **Buffer Pool** (Storage) - Database pages with intelligence

### Connection Pooling
- **New dedicated layer** between Y-Valve and Engine
- Not in parser (CPU intensive already)
- Not in engine (too low level)
- Perfect separation of concerns

### Resource Governance
- **Opt-in monitoring** - Zero overhead when disabled
- **Extensive reporting** when enabled
- **Per-session and per-query tracking**
- **Configurable limits and actions**

### Direct Engine Access
- **Utilities bypass parser** - Use pre-compiled BLR
- **SQL → BLR happens once** when job/script created
- **Maximum efficiency** for automated tasks

This architecture provides:
- **Intelligent caching** at every level
- **Database-aware buffer management**
- **Efficient connection reuse**
- **Detailed resource monitoring**
- **Direct path for utilities**