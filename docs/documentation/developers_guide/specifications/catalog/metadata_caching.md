# Specification: Metadata Caching

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Synopsis

This specification defines the catalog metadata caching system (relcache, syscache), including cache structures, invalidation strategies, and cache management.

## Scope

### In Scope

- relcache (relation cache)
- syscache (system catalog cache)
- Cache invalidation
- Memory management
- Cache statistics

### Out of Scope

- Plan cache (see executor specs)
- Buffer pool (see storage specs)

## Specification

### Cache Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Session Context                          │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   relcache   │  │   syscache   │  │  plan cache  │      │
│  │  (TableInfo) │  │  (TypeInfo)  │  │ (QueryPlan)  │      │
│  │              │  │              │  │              │      │
│  │  LRU eviction│  │  LRU eviction│  │  LRU eviction│      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                 │                 │              │
│         └─────────────────┼─────────────────┘              │
│                           ▼                                │
│              ┌────────────────────────┐                   │
│              │     Cache Manager      │                   │
│              │  (handles invalidation) │                   │
│              └────────────────────────┘                   │
└─────────────────────────────────────────────────────────────┘
```

### relcache (Relation Cache)

```cpp
struct RelCacheEntry {
    ID table_id;                    // Key
    TableInfo table_info;           // Cached data
    uint64_t creation_epoch;        // For invalidation
    std::chrono::steady_clock::time_point last_access;
    uint32_t access_count;
    bool is_valid;
};

class RelCache {
public:
    static constexpr size_t MAX_ENTRIES = 1024;
    static constexpr size_t MAX_MEMORY = 16 * 1024 * 1024; // 16MB
    
    // Lookup table info
    Status getTableInfo(const ID& table_id, TableInfo& info);
    
    // Insert/update entry
    void putTableInfo(const TableInfo& info, uint64_t epoch);
    
    // Invalidate entry
    void invalidate(const ID& table_id);
    
    // Invalidate by epoch
    void invalidateOlderThan(uint64_t epoch);
    
private:
    std::unordered_map<ID, RelCacheEntry, UuidHash> entries_;
    std::mutex mutex_;
    size_t current_memory_ = 0;
    
    // LRU list for eviction
    std::list<ID> lru_list_;
    std::unordered_map<ID, std::list<ID>::iterator> lru_map_;
    
    void evictIfNeeded();
    void evictLRU();
};
```

### syscache (System Cache)

```cpp
enum class SysCacheId : uint8_t {
    TYPE_INFO = 0,          // Data types
    CHARSET_INFO = 1,       // Character sets
    COLLATION_INFO = 2,     // Collations
    SCHEMA_INFO = 3,        // Schemas
    USER_INFO = 4,          // Users
    ROLE_INFO = 5,          // Roles
    // ... more
};

template<typename T>
struct SysCacheEntry {
    ID key;
    T value;
    uint64_t epoch;
    std::chrono::steady_clock::time_point last_access;
};

class SysCache {
public:
    // Type info cache
    Status getTypeInfo(uint16_t type_id, TypeInfo& info);
    void putTypeInfo(const TypeInfo& info, uint64_t epoch);
    
    // Charset info cache
    Status getCharsetInfo(uint16_t charset_id, CharsetInfo& info);
    void putCharsetInfo(const CharsetInfo& info, uint64_t epoch);
    
    // Schema info cache
    Status getSchemaInfo(const ID& schema_id, SchemaInfo& info);
    void putSchemaInfo(const SchemaInfo& info, uint64_t epoch);
    
    // Invalidate by cache ID
    void invalidate(SysCacheId cache_id);
    
    // Invalidate specific entry
    template<typename Key>
    void invalidateEntry(SysCacheId cache_id, const Key& key);
    
private:
    // Separate cache per type
    std::unordered_map<uint16_t, SysCacheEntry<TypeInfo>> type_cache_;
    std::unordered_map<uint16_t, SysCacheEntry<CharsetInfo>> charset_cache_;
    std::unordered_map<ID, SysCacheEntry<SchemaInfo>, UuidHash> schema_cache_;
    
    std::mutex mutex_;
};
```

### Cache Lookup Algorithm

```cpp
Status RelCache::getTableInfo(const ID& table_id, TableInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 1. Check cache
    auto it = entries_.find(table_id);
    if (it != entries_.end()) {
        // Check if still valid
        uint64_t current_epoch = getTableEpoch(table_id);
        if (it->second.creation_epoch >= current_epoch) {
            // Cache hit
            info = it->second.table_info;
            
            // Update LRU
            updateLRU(table_id);
            
            it->second.last_access = std::chrono::steady_clock::now();
            it->second.access_count++;
            
            return Status::OK;
        }
        // Cache entry stale, remove it
        removeEntry(it);
    }
    
    // 2. Cache miss - load from catalog
    TableInfo loaded_info;
    RETURN_IF_ERROR(loadTableInfoFromCatalog(table_id, loaded_info));
    
    // 3. Insert into cache
    RelCacheEntry entry;
    entry.table_id = table_id;
    entry.table_info = loaded_info;
    entry.creation_epoch = getTableEpoch(table_id);
    entry.last_access = std::chrono::steady_clock::now();
    entry.access_count = 1;
    entry.is_valid = true;
    
    evictIfNeeded();
    insertEntry(table_id, std::move(entry));
    
    info = loaded_info;
    return Status::OK;
}
```

### Cache Invalidation

```cpp
// Global invalidation (broadcast to all sessions)
void CatalogManager::broadcastInvalidation(ObjectType type, ID object_id) {
    // 1. Update epoch
    incrementEpoch(object_id);
    
    // 2. Invalidate in shared cache (if any)
    // ...
    
    // 3. Broadcast to all sessions
    for (auto& session : active_sessions_) {
        session->invalidateCache(type, object_id);
    }
}

// Session-local invalidation
void Session::invalidateCache(ObjectType type, ID object_id) {
    switch (type) {
        case ObjectType::TABLE:
            relcache_.invalidate(object_id);
            // Also invalidate dependent plans
            plan_cache_.invalidateByTable(object_id);
            break;
            
        case ObjectType::TYPE:
            syscache_.invalidateEntry(SysCacheId::TYPE_INFO, type_id);
            break;
            
        case ObjectType::SCHEMA:
            syscache_.invalidateEntry(SysCacheId::SCHEMA_INFO, object_id);
            // Invalidate all tables in schema
            for (auto& table_id : getTablesInSchema(object_id)) {
                relcache_.invalidate(table_id);
            }
            break;
            
        // ... other types
    }
}
```

### Memory Management

```cpp
void RelCache::evictIfNeeded() {
    // Check memory limit
    while (current_memory_ > MAX_MEMORY && !lru_list_.empty()) {
        evictLRU();
    }
    
    // Check entry count limit
    while (entries_.size() >= MAX_ENTRIES && !lru_list_.empty()) {
        evictLRU();
    }
}

void RelCache::evictLRU() {
    // Get least recently used
    ID lru_id = lru_list_.back();
    lru_list_.pop_back();
    
    auto it = entries_.find(lru_id);
    if (it != entries_.end()) {
        current_memory_ -= estimateMemoryUsage(it->second);
        entries_.erase(it);
    }
    
    lru_map_.erase(lru_id);
}
```

### Cache Statistics

```cpp
struct CacheStatistics {
    // relcache stats
    size_t relcache_entries;
    size_t relcache_memory;
    uint64_t relcache_hits;
    uint64_t relcache_misses;
    double relcache_hit_ratio;
    
    // syscache stats (per cache)
    size_t type_cache_entries;
    uint64_t type_cache_hits;
    uint64_t type_cache_misses;
    
    size_t charset_cache_entries;
    uint64_t charset_cache_hits;
    uint64_t charset_cache_misses;
    
    size_t schema_cache_entries;
    uint64_t schema_cache_hits;
    uint64_t schema_cache_misses;
};

CacheStatistics getCacheStatistics() {
    CacheStatistics stats;
    
    // relcache
    stats.relcache_entries = relcache_.size();
    stats.relcache_memory = relcache_.memoryUsage();
    stats.relcache_hits = relcache_hit_counter_.load();
    stats.relcache_misses = relcache_miss_counter_.load();
    stats.relcache_hit_ratio = calculateHitRatio(
        stats.relcache_hits, stats.relcache_misses);
    
    // ... other caches
    
    return stats;
}
```

## Algorithms

### Algorithm: Cache Warming

```
Input:  List of frequently accessed tables
Output: Pre-populated cache

1. For each table in priority list:
   a. Load TableInfo from catalog
   b. Load all ColumnInfo
   c. Load all IndexInfo
   d. Insert into relcache
   e. Pin entries (prevent eviction)

2. Load system types into syscache

3. Load character sets and collations

4. Release pins (normal LRU applies)
```

### Algorithm: Cache Flush

```
Input:  Cache flush mode (ALL or OLDER_THAN)
Output: Cleared cache

1. Acquire all cache locks

2. If mode == ALL:
   a. Clear all entries
   b. Reset memory counter
   
3. If mode == OLDER_THAN(epoch):
   a. For each entry:
      - If entry.epoch < epoch: remove

4. Release locks

5. Log flush statistics
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `CACHE_INV_001` | Cached data never older than epoch | Epoch check |
| `CACHE_INV_002` | Memory limits enforced | Eviction |
| `CACHE_INV_003` | Thread-safe access | Mutex verification |
| `CACHE_INV_004` | LRU order maintained | List check |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `CACHE_FULL` | Cannot add entry (memory) | Evict and retry |
| `CACHE_MISS` | Entry not found | Load from catalog |
| `STALE_ENTRY` | Entry epoch outdated | Reload |

## Related Specifications

- [invalidation.md](./invalidation.md) - Cache invalidation
- [ddl_operations.md](./ddl_operations.md) - DDL that triggers invalidation

## Appendix

### Cache Size Guidelines

| Cache | Default Entries | Default Memory | Typical Hit Ratio |
|-------|-----------------|----------------|-------------------|
| relcache | 1024 | 16 MB | 95%+ |
| type cache | 256 | 1 MB | 99%+ |
| charset cache | 64 | 256 KB | 99%+ |
| schema cache | 256 | 2 MB | 98%+ |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
