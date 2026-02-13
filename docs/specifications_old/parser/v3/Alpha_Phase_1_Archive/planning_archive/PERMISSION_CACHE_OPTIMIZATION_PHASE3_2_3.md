# Permission Cache Optimization - Phase 3.2.3

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Status**: Planning → Implementation
**Priority**: Medium (2-5x additional performance improvement)
**Estimated Time**: 10-12 hours
**Date**: November 11, 2025

---

## Overview

Phase 3.2.3 adds persistent permission caching across queries with LRU eviction and automatic cache invalidation on GRANT/REVOKE operations. This provides an additional **2-5x performance improvement** on top of the gains from Phase 3.2.1/3.2.2.

---

## Current State (After Phase 3.2.1/3.2.2)

### Query Planner Permission Cache (Phase 3.2.1)
```cpp
// Current: Per-query cache (cleared between queries)
class QueryPlanner {
private:
    struct PermissionCache {
        std::unordered_map<core::ID, bool> table_select;
        std::unordered_map<core::ID, bool> table_insert;
        std::unordered_map<core::ID, bool> table_update;
        std::unordered_map<core::ID, bool> table_delete;
    };
    PermissionCache perm_cache_;  // Cleared at start of each query
};
```

**Limitations**:
- Cache cleared between queries
- No benefit for repeated queries
- No cache sharing across connections

### Executor Permission Checks (Phase 3.2.2)
```cpp
// Current: No caching in executor
bool Executor::checkPermission(...) {
    // Direct catalog query every time
    db_->catalog_manager()->hasPermission(...);
}
```

**Limitations**:
- Every DML statement queries catalog
- No caching across DML operations
- Redundant lookups in same transaction

---

## Target State (Phase 3.2.3)

### Global Permission Cache

```cpp
// New: Database-wide permission cache with LRU eviction
class PermissionCache {
private:
    struct CacheKey {
        core::ID user_id;
        core::ID object_id;
        core::CatalogManager::PermissionObjectType object_type;
        core::CatalogManager::Privilege privilege;

        bool operator==(const CacheKey& other) const;
    };

    struct CacheEntry {
        bool has_permission;
        std::chrono::steady_clock::time_point timestamp;
        size_t access_count;
    };

    // LRU cache with configurable size
    std::unordered_map<CacheKey, CacheEntry> cache_;
    std:list<CacheKey> lru_list_;  // Most recent at front

    size_t max_entries_;  // Default: 1000
    std::chrono::seconds ttl_;  // Default: 60 seconds

public:
    std::optional<bool> lookup(const CacheKey& key);
    void insert(const CacheKey& key, bool has_permission);
    void invalidate(const core::ID& user_id);  // Called on GRANT/REVOKE
    void invalidateAll();  // Called on schema changes

    // Statistics
    size_t hit_count_;
    size_t miss_count_;
    double getHitRate() const;
};
```

---

## Design

### 1. Cache Key Structure

```cpp
struct PermissionCacheKey {
    core::ID user_id;        // UUID (16 bytes)
    core::ID object_id;      // UUID (16 bytes)
    uint8_t object_type;     // 1 byte
    uint32_t privilege;      // 4 bytes
    // Total: 37 bytes per key
};

// Hash function for unordered_map
struct PermissionCacheKeyHash {
    size_t operator()(const PermissionCacheKey& key) const {
        // Combine hashes of all fields
        size_t h1 = std::hash<core::ID>{}(key.user_id);
        size_t h2 = std::hash<core::ID>{}(key.object_id);
        size_t h3 = std::hash<uint8_t>{}(key.object_type);
        size_t h4 = std::hash<uint32_t>{}(key.privilege);
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};
```

### 2. LRU Eviction Strategy

```cpp
void PermissionCache::insert(const CacheKey& key, bool has_permission) {
    // Check if cache is full
    if (cache_.size() >= max_entries_) {
        // Evict least recently used entry
        const CacheKey& lru_key = lru_list_.back();
        cache_.erase(lru_key);
        lru_list_.pop_back();
    }

    // Insert new entry
    CacheEntry entry;
    entry.has_permission = has_permission;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.access_count = 1;

    cache_[key] = entry;
    lru_list_.push_front(key);
}

std::optional<bool> PermissionCache::lookup(const CacheKey& key) {
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        ++miss_count_;
        return std::nullopt;  // Cache miss
    }

    // Check TTL
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
        now - it->second.timestamp);

    if (age > ttl_) {
        // Entry expired
        cache_.erase(it);
        lru_list_.remove(key);
        ++miss_count_;
        return std::nullopt;
    }

    // Cache hit - update LRU
    ++hit_count_;
    ++it->second.access_count;
    lru_list_.remove(key);
    lru_list_.push_front(key);

    return it->second.has_permission;
}
```

### 3. Cache Invalidation

```cpp
// Called when GRANT/REVOKE executed
void PermissionCache::invalidateUser(const core::ID& user_id) {
    // Remove all entries for this user
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (it->first.user_id == user_id) {
            lru_list_.remove(it->first);
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void PermissionCache::invalidateObject(const core::ID& object_id) {
    // Remove all entries for this object (table/schema/etc.)
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (it->first.object_id == object_id) {
            lru_list_.remove(it->first);
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void PermissionCache::invalidateAll() {
    // Clear entire cache (called on major schema changes)
    cache_.clear();
    lru_list_.clear();
}
```

---

## Implementation Plan

### Phase 3.2.3.1: Core Cache Implementation (4-5 hours)

**Tasks**:
1. Create `PermissionCache` class (2 hours)
   - File: `include/scratchbird/core/permission_cache.h`
   - File: `src/core/permission_cache.cpp`
   - Implement cache key, hash function
   - Implement LRU list management
   - Implement TTL checking

2. Add cache to Database class (1 hour)
   - Add `permission_cache_` member to Database
   - Initialize in Database::open()
   - Make accessible to query planner and executor

3. Thread safety (1 hour)
   - Add std::shared_mutex for reader-writer lock
   - Multiple readers, single writer
   - Lock-free reads when possible

### Phase 3.2.3.2: Integration with Query Planner (2-3 hours)

**Tasks**:
1. Update QueryPlanner::checkTablePermission() (1 hour)
   - Check global cache first
   - On cache miss, query catalog
   - Insert result into global cache

2. Remove local cache from QueryPlanner (30 min)
   - Delete PermissionCache struct
   - Remove clearPermissionCache() calls
   - Simplify code

3. Testing (1 hour)
   - Verify cache hits work
   - Verify cross-query caching
   - Performance benchmarks

### Phase 3.2.3.3: Integration with Executor (2-3 hours)

**Tasks**:
1. Update Executor::checkPermission() (1 hour)
   - Check global cache first
   - On cache miss, query catalog
   - Insert result into global cache

2. Testing (1 hour)
   - Verify DML operations use cache
   - Verify cache sharing across DML statements

### Phase 3.2.3.4: Cache Invalidation (3-4 hours)

**Tasks**:
1. Update GRANT executor (1 hour)
   - Call cache invalidation after successful GRANT
   - Invalidate user + object

2. Update REVOKE executor (1 hour)
   - Call cache invalidation after successful REVOKE
   - Invalidate user + object

3. Update CREATE/DROP USER/ROLE (1 hour)
   - Invalidate all entries for deleted user/role
   - Invalidate all when roles change

4. Testing (1 hour)
   - Verify GRANT invalidates cache
   - Verify REVOKE invalidates cache
   - Verify cached denials become grants

---

## Performance Analysis

### Before (Phase 3.2.2)

**Scenario**: Execute 100 queries with same permissions

| Operation | Cache Lookups | Catalog Queries | Time |
|-----------|---------------|-----------------|------|
| 100 SELECTs | 0 | 100 | 500ms |
| 100 INSERTs | 0 | 100 | 500ms |
| Mixed (50/50) | 0 | 100 | 500ms |

**Average**: 5ms per permission check (catalog query)

### After (Phase 3.2.3)

**Scenario**: Execute 100 queries with same permissions

| Operation | Cache Lookups | Catalog Queries | Time |
|-----------|---------------|-----------------|------|
| 100 SELECTs | 99 hits | 1 | 100ms |
| 100 INSERTs | 99 hits | 1 | 100ms |
| Mixed (50/50) | 99 hits | 1 | 100ms |

**Average**: 1ms per permission check (cache hit)
**Speedup**: **5x improvement**

### Cache Hit Rate Projections

| Workload Type | Expected Hit Rate | Improvement |
|---------------|-------------------|-------------|
| Repeated queries | 95-99% | 5x |
| Mixed workload | 80-90% | 3x |
| Random queries | 50-70% | 2x |

---

## Memory Usage

### Cache Size Calculations

**Per Entry**:
- Cache key: 37 bytes
- Cache entry: 16 bytes (bool + timestamp + counter)
- LRU node: 40 bytes (list overhead)
- Hash map overhead: ~16 bytes
- **Total**: ~109 bytes per entry

**Default Configuration** (1000 entries):
- Total memory: ~109 KB
- Very reasonable for the performance gain

**Large Configuration** (10,000 entries):
- Total memory: ~1.09 MB
- Still acceptable

---

## Configuration Options

### Cache Settings (Database-level)

```sql
-- Set cache size
SET permission_cache_max_entries = 1000;  -- Default

-- Set TTL
SET permission_cache_ttl = 60;  -- Seconds, default 60

-- Disable cache (for debugging)
SET permission_cache_enabled = false;

-- View cache statistics
SELECT * FROM sys.permission_cache_stats;
```

### Statistics Exposed

```cpp
struct PermissionCacheStats {
    size_t total_entries;      // Current cache size
    size_t max_entries;        // Maximum capacity
    size_t hit_count;          // Total hits
    size_t miss_count;         // Total misses
    double hit_rate;           // Percentage
    size_t eviction_count;     // LRU evictions
    size_t invalidation_count; // GRANT/REVOKE invalidations
};
```

---

## Testing Strategy

### Unit Tests

1. **Cache Operations**:
   - Insert and lookup
   - LRU eviction when full
   - TTL expiration
   - Cache invalidation

2. **Thread Safety**:
   - Concurrent reads
   - Concurrent writes
   - Reader-writer scenarios

3. **Hash Function**:
   - No collisions for common keys
   - Uniform distribution

### Integration Tests

1. **Cross-Query Caching**:
   ```sql
   -- First query (cache miss)
   SELECT * FROM employees;  -- ~5ms

   -- Second query (cache hit)
   SELECT * FROM employees;  -- ~1ms
   ```

2. **Cache Invalidation**:
   ```sql
   -- Query succeeds (cached as granted)
   SELECT * FROM employees;

   -- Revoke permission
   REVOKE SELECT ON TABLE employees FROM alice;

   -- Query fails (cache invalidated)
   SELECT * FROM employees;  -- Permission denied
   ```

3. **Mixed Workload**:
   ```sql
   -- Multiple operations on same table
   SELECT * FROM employees;
   INSERT INTO employees VALUES (...);
   UPDATE employees SET ...;
   DELETE FROM employees WHERE ...;
   -- All use cached permissions
   ```

---

## Backward Compatibility

### Migration from Phase 3.2.1/3.2.2

- ✅ No breaking changes
- ✅ Existing code works without modification
- ✅ Cache is transparent to query planner/executor
- ✅ Can be disabled for debugging

### Configuration

```cpp
// Database open with cache enabled (default)
Database db;
db.open("mydb.db");

// Database open with cache disabled (debugging)
Database db;
db.open("mydb.db", /* cache_enabled = */ false);
```

---

## Success Criteria

✅ **Performance**:
- [ ] 2-5x speedup for repeated queries
- [ ] >90% cache hit rate in typical workloads
- [ ] <2% memory overhead

✅ **Correctness**:
- [ ] Cache invalidation works on GRANT/REVOKE
- [ ] No stale cache entries after permissions change
- [ ] Thread-safe under concurrent access

✅ **Code Quality**:
- [ ] Clean cache abstraction
- [ ] Comprehensive tests
- [ ] Performance monitoring

---

## Risks and Mitigations

### Risk 1: Cache Invalidation Bugs
**Risk**: Stale cache entries after GRANT/REVOKE
**Mitigation**:
- TTL as safety net (60s default)
- Comprehensive invalidation tests
- Can disable cache if issues found

### Risk 2: Memory Usage
**Risk**: Cache grows too large
**Mitigation**:
- LRU eviction keeps size bounded
- Configurable max size
- Monitoring tools

### Risk 3: Thread Safety
**Risk**: Race conditions in concurrent access
**Mitigation**:
- Use std::shared_mutex (reader-writer lock)
- Comprehensive thread safety tests
- Lock-free fast path when possible

---

## Next Steps

1. Implement Phase 3.2.3.1 (Core cache)
2. Integrate with query planner (Phase 3.2.3.2)
3. Integrate with executor (Phase 3.2.3.3)
4. Implement invalidation (Phase 3.2.3.4)
5. Performance benchmarks
6. Documentation

---

**Status**: Ready to implement
**Start Date**: November 11, 2025
**Target Completion**: Phase 3.2.3 in current session

