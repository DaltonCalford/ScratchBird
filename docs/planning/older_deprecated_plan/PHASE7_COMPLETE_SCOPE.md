# Phase 7: Tablespace Advanced Features - Complete Scope

**Document Status**: COMPREHENSIVE SCOPE
**Version**: 1.0
**Date**: October 21, 2025
**Priority**: MEDIUM (Post Phase 6)
**Dependencies**: Phase 6 (Attach/Detach) complete

---

## Executive Summary

This document provides a COMPLETE scope of ALL advanced tablespace features for ScratchBird ALPHA. After review of specifications, research, and industry best practices, Phase 7 encompasses **8 major feature areas** with estimated **80-120 hours** of implementation effort.

**User Directive**: "Fully implement TABLESPACE support with all possible optional enhancements implemented."

This scope includes:
1. **Implemented features** (what we have)
2. **Required features** (must have for ALPHA)
3. **Optional enhancements** (nice to have for ALPHA)
4. **Future enhancements** (post-ALPHA)

---

## Current Implementation Status

### ✅ COMPLETE (Phases 0-5 Partial)

| Feature | Status | Effort (hours) |
|---------|--------|----------------|
| **Core Infrastructure** | ✅ COMPLETE | ~110 |
| - GPID addressing | ✅ | 17 |
| - Tablespace file management | ✅ | 13 |
| - Catalog (pg_tablespace) | ✅ | 3 |
| - TID migration to GPID format | ✅ | 8 |
| **SQL DDL** | ✅ COMPLETE | ~21 |
| - CREATE/DROP TABLESPACE | ✅ | 11.5 |
| - ALTER TABLESPACE | ✅ | 7.5 |
| - CREATE TABLE ... TABLESPACE | ✅ | 2 |
| **Autoextend (Partial)** | ⚠️ PARTIAL | ~4.5 |
| - Preallocation | ✅ | 4.5 |
| - Autoextend implementation | ⏸️ | 0 (NOT STARTED) |
| **OFFLINE Migration (Partial)** | ⚠️ PARTIAL | ~10 |
| - Heap page migration | ✅ | 7.5 |
| - B-Tree index TID updates | ✅ | 2 |
| - Hash index TID updates | ✅ | 0.5 |
| - TOAST handling | ⚠️ SIMPLIFIED | 0.5 (warnings only) |
| - Other index types | ⏸️ STUBS | 0 |

**Total Completed**: ~145.5 hours

---

### ⏸️ INCOMPLETE (Must Complete for ALPHA)

| Feature | Status | Estimated Effort |
|---------|--------|------------------|
| **Autoextend Completion** | ⏸️ | 12-18 hours |
| **TOAST Full Implementation** | ⚠️ | 8-12 hours |
| **Other Index Types (5)** | ⏸️ | 17-24 hours |
| **ONLINE Migration** | ⏸️ | 66-96 hours |
| **Attach/Detach** | ⏸️ | 20-30 hours |
| **Bug Fixes** (MVCC→MGA) | ❌ | 2-4 hours |

**Total Remaining (Phases 3-6)**: ~125-184 hours

---

## Phase 7: Advanced Features - Complete Scope

### Feature Area 1: Tablespace Statistics and Monitoring

**Purpose**: Provide visibility into tablespace usage, performance, and health.

**Priority**: HIGH (essential for production monitoring)
**Estimated Effort**: 12-16 hours

---

#### **Task 7.1.1: Per-Tablespace I/O Statistics** (4-6 hours)

**Description**: Track read/write operations, I/O latency, and throughput per tablespace.

**Data to Track**:
```c
struct TablespaceIOStats {
    // Read statistics
    uint64_t    pages_read;           // Total pages read
    uint64_t    bytes_read;           // Total bytes read
    uint64_t    read_time_us;         // Total read time (microseconds)
    uint64_t    read_operations;      // Number of read operations

    // Write statistics
    uint64_t    pages_written;        // Total pages written
    uint64_t    bytes_written;        // Total bytes written
    uint64_t    write_time_us;        // Total write time (microseconds)
    uint64_t    write_operations;     // Number of write operations

    // Cache statistics
    uint64_t    buffer_hits;          // Pages found in buffer pool
    uint64_t    buffer_misses;        // Pages not in buffer pool

    // Computed metrics
    double      avg_read_latency_ms() {
        return (read_operations > 0) ?
            (read_time_us / 1000.0) / read_operations : 0.0;
    }
    double      avg_write_latency_ms() {
        return (write_operations > 0) ?
            (write_time_us / 1000.0) / write_operations : 0.0;
    }
    double      cache_hit_ratio() {
        uint64_t total = buffer_hits + buffer_misses;
        return (total > 0) ? (double)buffer_hits / total : 0.0;
    }
};
```

**Implementation**:
- Add `TablespaceIOStats` to `TablespaceInfo` struct
- Instrument `Database::readPage()` and `Database::writePage()` to update counters
- Add SQL view: `pg_stat_tablespaces`

**SQL Interface**:
```sql
SELECT * FROM pg_stat_tablespaces;

-- Example output:
-- tablespace_id | name      | pages_read | pages_written | avg_read_ms | avg_write_ms | cache_hit_ratio
-- 1             | primary   | 100000     | 50000         | 0.5         | 1.2          | 0.95
-- 2             | ssd_hot   | 50000      | 25000         | 0.3         | 0.8          | 0.98
-- 3             | hdd_cold  | 10000      | 5000          | 2.5         | 5.0          | 0.75
```

**Files Modified**:
- `include/scratchbird/core/tablespace.h` (~30 lines)
- `src/core/database.cpp` (~100 lines - instrumentation)
- `src/core/catalog_manager.cpp` (~80 lines - pg_stat_tablespaces view)

**Acceptance Criteria**:
- [ ] I/O counters updated on every read/write
- [ ] Statistics queryable via SQL
- [ ] Average latency computed correctly
- [ ] Cache hit ratio computed correctly

---

#### **Task 7.1.2: Space Usage Tracking** (3-4 hours)

**Description**: Track allocated vs free space per tablespace.

**Data to Track**:
```c
struct TablespaceSpaceStats {
    uint64_t    total_pages;          // Total pages allocated to tablespace
    uint64_t    used_pages;           // Pages with data
    uint64_t    free_pages;           // Pages available for allocation
    uint64_t    total_bytes;          // Total file size
    uint64_t    used_bytes;           // Bytes occupied by data
    uint64_t    free_bytes;           // Bytes available

    // Growth tracking
    uint64_t    extension_count;      // Number of autoextend operations
    uint64_t    last_extension_time;  // Timestamp of last extension

    // Computed metrics
    double      utilization_percent() {
        return (total_pages > 0) ?
            (double)used_pages / total_pages * 100.0 : 0.0;
    }
};
```

**SQL Interface**:
```sql
SELECT * FROM pg_tablespace_usage;

-- Example output:
-- name      | total_mb | used_mb | free_mb | utilization | extensions | last_extend
-- primary   | 1024     | 950     | 74      | 92.8%       | 10         | 2025-10-21 10:30:00
-- ssd_hot   | 512      | 400     | 112     | 78.1%       | 5          | 2025-10-21 09:15:00
```

**Files Modified**:
- `src/core/page_manager.cpp` (~50 lines - update counters on allocate/free)
- `src/core/catalog_manager.cpp` (~60 lines - pg_tablespace_usage view)

**Acceptance Criteria**:
- [ ] Space counters accurate
- [ ] Utilization percentage correct
- [ ] Extension tracking works

---

#### **Task 7.1.3: Hot/Cold Page Tracking** (5-6 hours)

**Description**: Track page access patterns to identify hot (frequently accessed) and cold (rarely accessed) pages.

**Purpose**: Enable automatic data placement optimization (hot data on SSD, cold data on HDD).

**Data to Track**:
```c
// Per-page access tracking (in memory, not persisted)
struct PageAccessInfo {
    uint64_t    last_access_time;     // Timestamp of last access
    uint32_t    access_count;         // Number of accesses (sliding window)
    uint16_t    access_score;         // Computed hotness score (0-100)
};

// Per-tablespace aggregated stats
struct HotColdStats {
    uint64_t    hot_pages;            // Pages accessed > 10 times/minute
    uint64_t    warm_pages;           // Pages accessed 1-10 times/minute
    uint64_t    cold_pages;           // Pages accessed < 1 time/minute
    uint64_t    frozen_pages;         // Pages not accessed in 24 hours
};
```

**Implementation**:
- Maintain in-memory hash map: `page_id → PageAccessInfo`
- Update on every `BufferPool::pinPage()` call
- Periodically decay access counts (sliding window)
- Compute hotness scores

**SQL Interface**:
```sql
-- Identify hot pages for migration to SSD
SELECT page_id, access_score
FROM pg_page_hotness
WHERE tablespace_id = 1
AND access_score > 80
ORDER BY access_score DESC
LIMIT 100;

-- Tablespace-level summary
SELECT * FROM pg_tablespace_hotness;

-- Example output:
-- tablespace | hot_pages | warm_pages | cold_pages | frozen_pages | recommendation
-- primary    | 1000      | 5000       | 10000      | 50000        | "Move 1000 hot pages to SSD"
```

**Future Enhancement**: Automatic hot page migration (move hot pages from HDD to SSD tablespace).

**Files Modified**:
- New file: `include/scratchbird/core/page_hotness.h` (~80 lines)
- New file: `src/core/page_hotness.cpp` (~250 lines)
- `src/core/buffer_pool.cpp` (~30 lines - track access)
- `src/core/catalog_manager.cpp` (~100 lines - SQL views)

**Acceptance Criteria**:
- [ ] Access tracking works
- [ ] Hotness scores computed correctly
- [ ] Hot/cold page counts accurate
- [ ] SQL views return correct data

---

### Feature Area 2: Tablespace Quotas and Limits

**Purpose**: Enforce per-user or per-role tablespace usage limits.

**Priority**: MEDIUM (useful for multi-tenant environments)
**Estimated Effort**: 10-14 hours

---

#### **Task 7.2.1: Per-Tablespace Quota System** (6-8 hours)

**Description**: Limit how much space each user/role can use in a tablespace.

**Catalog Schema**:
```sql
CREATE TABLE pg_tablespace_quotas (
    quota_id        UUID PRIMARY KEY,
    tablespace_id   INT REFERENCES pg_tablespace(tablespace_id),
    role_id         UUID REFERENCES pg_roles(role_id),  -- Or user_id
    quota_bytes     BIGINT,      -- Maximum bytes allowed
    used_bytes      BIGINT,      -- Currently used bytes
    hard_limit      BOOLEAN,     -- If true, BLOCK operations when exceeded
    warn_threshold  INT,         -- Warn at N% of quota (e.g., 80)
    created_at      TIMESTAMP,
    modified_at     TIMESTAMP
);
```

**SQL Interface**:
```sql
-- Set quota for user
ALTER TABLESPACE ssd_hot SET QUOTA 10GB FOR USER alice;

-- Set quota for role
ALTER TABLESPACE hdd_archive SET QUOTA 100GB FOR ROLE developers;

-- View quotas
SELECT * FROM pg_tablespace_quotas WHERE role_id = 'alice-uuid';

-- Example output:
-- tablespace | role  | quota_mb | used_mb | usage_pct | status
-- ssd_hot    | alice | 10000    | 8500    | 85%       | WARN (>80%)
-- hdd_archive| alice | 50000    | 10000   | 20%       | OK
```

**Enforcement**:
- Check quota before `PageManager::allocatePage()`
- If hard limit and quota exceeded, return `Status::QUOTA_EXCEEDED`
- If warn threshold exceeded, log warning (but allow operation)

**Implementation**:
- Add `pg_tablespace_quotas` catalog table
- Add quota check to `PageManager::allocatePage()`
- Track per-user/role page allocations
- Update `used_bytes` on allocate/free

**Files Modified**:
- `include/scratchbird/core/catalog_manager.h` (~40 lines)
- `src/core/catalog_manager.cpp` (~200 lines)
- `src/core/page_manager.cpp` (~80 lines - quota enforcement)
- `src/parser/parser.cpp` (~100 lines - ALTER TABLESPACE ... SET QUOTA syntax)

**Acceptance Criteria**:
- [ ] Quotas enforced on page allocation
- [ ] Hard limits block operations
- [ ] Warnings logged at threshold
- [ ] SQL interface works

---

#### **Task 7.2.2: Tablespace Reserved Space** (4-6 hours)

**Description**: Reserve a percentage of tablespace for admin operations (prevents user operations from filling tablespace completely).

**Configuration**:
```sql
ALTER TABLESPACE ssd_hot SET RESERVED 5 PERCENT;  -- Reserve 5% for admin
```

**Enforcement**:
- Calculate reserved bytes: `total_bytes * reserved_percent / 100`
- User operations fail when: `used_bytes + new_bytes > (total_bytes - reserved_bytes)`
- Admin operations (superuser) bypass reservation

**Files Modified**:
- `src/core/page_manager.cpp` (~60 lines)
- `src/parser/parser.cpp` (~50 lines)

**Acceptance Criteria**:
- [ ] Reserved space enforced
- [ ] Admin operations bypass reservation
- [ ] User operations fail at reserved threshold

---

### Feature Area 3: Tablespace Compression

**Purpose**: Transparent page-level compression to save disk space.

**Priority**: MEDIUM-HIGH (significant space savings)
**Estimated Effort**: 12-16 hours

---

#### **Task 7.3.1: Compression Framework** (6-8 hours)

**Description**: Implement pluggable compression algorithms for tablespaces.

**Supported Algorithms**:
- **None**: No compression (default)
- **LZ4**: Fast compression (10-20% savings, low CPU)
- **ZSTD**: High compression (30-50% savings, moderate CPU)
- **ZLIB**: Maximum compression (50-70% savings, high CPU)

**Tablespace Configuration**:
```sql
CREATE TABLESPACE compressed_ts
    LOCATION '/data/compressed.sbts'
    COMPRESSION lz4;  -- or zstd, zlib

ALTER TABLESPACE compressed_ts SET COMPRESSION zstd;
```

**Implementation**:
- Add `compression_algorithm` to `TablespaceInfo`
- Compress pages before `Database::writePage()`
- Decompress pages after `Database::readPage()`
- Store compressed size in page header

**Page Header Extension**:
```c
struct CompressedPageHeader {
    uint16_t    compressed_size;      // Actual size on disk
    uint16_t    uncompressed_size;    // Original size
    uint8_t     compression_algorithm; // 0=None, 1=LZ4, 2=ZSTD, 3=ZLIB
    uint8_t     compression_level;    // Algorithm-specific level
};
```

**Files Modified**:
- `include/scratchbird/core/compression.h` (NEW ~100 lines)
- `src/core/compression.cpp` (NEW ~300 lines)
- `src/core/database.cpp` (~150 lines - compress/decompress on I/O)
- `src/core/tablespace.cpp` (~50 lines - compression config)

**Dependencies**: LZ4, ZSTD, ZLIB libraries (already common)

**Acceptance Criteria**:
- [ ] LZ4 compression/decompression works
- [ ] ZSTD compression/decompression works
- [ ] Compressed pages readable
- [ ] Space savings measurable
- [ ] Performance overhead < 10%

---

#### **Task 7.3.2: Compression Statistics** (3-4 hours)

**Description**: Track compression ratio and performance impact.

**Statistics**:
```c
struct CompressionStats {
    uint64_t    pages_compressed;     // Total pages compressed
    uint64_t    bytes_before;         // Total bytes before compression
    uint64_t    bytes_after;          // Total bytes after compression
    uint64_t    compression_time_us;  // Time spent compressing
    uint64_t    decompression_time_us;// Time spent decompressing

    double compression_ratio() {
        return (bytes_before > 0) ?
            (double)bytes_after / bytes_before : 1.0;
    }
    double space_saved_mb() {
        return (bytes_before - bytes_after) / (1024.0 * 1024.0);
    }
};
```

**SQL Interface**:
```sql
SELECT * FROM pg_tablespace_compression;

-- Example output:
-- tablespace | algorithm | pages | ratio | saved_mb | avg_compress_ms | avg_decompress_ms
-- compressed | lz4       | 10000 | 0.75  | 250      | 0.1             | 0.05
-- archive    | zstd      | 50000 | 0.50  | 5000     | 0.5             | 0.3
```

**Files Modified**:
- `src/core/compression.cpp` (~80 lines)
- `src/core/catalog_manager.cpp` (~60 lines - SQL view)

**Acceptance Criteria**:
- [ ] Compression ratio accurate
- [ ] Space savings calculated
- [ ] Performance impact measured

---

#### **Task 7.3.3: Adaptive Compression** (3-4 hours)

**Description**: Automatically choose compression algorithm based on data characteristics.

**Heuristics**:
- If page highly compressible (sample ratio < 0.5): Use ZSTD (high compression)
- If page moderately compressible (0.5 < ratio < 0.8): Use LZ4 (fast)
- If page incompressible (ratio > 0.9): Use NONE (skip compression)

**Implementation**:
- Sample first N bytes of page
- Attempt compression with LZ4
- If ratio < threshold, use compression
- Otherwise, skip

**Files Modified**:
- `src/core/compression.cpp` (~100 lines)

**Acceptance Criteria**:
- [ ] Adaptive compression works
- [ ] Incompressible pages skipped
- [ ] CPU overhead minimized

---

### Feature Area 4: Tablespace Encryption

**Purpose**: At-rest encryption for sensitive data.

**Priority**: MEDIUM (security requirement for some deployments)
**Estimated Effort**: 14-18 hours

---

#### **Task 7.4.1: Encryption Framework** (8-10 hours)

**Description**: Implement AES-256-GCM encryption for tablespace pages.

**Configuration**:
```sql
CREATE TABLESPACE encrypted_ts
    LOCATION '/data/encrypted.sbts'
    ENCRYPTION aes256;

-- Provide encryption key (from key management system)
ALTER TABLESPACE encrypted_ts SET ENCRYPTION KEY 'base64-encoded-key';
```

**Implementation**:
- Encrypt pages before write, decrypt after read
- Store encryption metadata in page header
- Support key rotation (re-encrypt all pages)

**Page Header Extension**:
```c
struct EncryptedPageHeader {
    uint8_t     encryption_algorithm;  // 0=None, 1=AES-256-GCM
    uint8_t     key_version;           // For key rotation
    uint8_t     iv[12];                // Initialization vector (GCM nonce)
    uint8_t     auth_tag[16];          // Authentication tag (GCM)
};
```

**Files Modified**:
- New file: `include/scratchbird/core/encryption.h` (~80 lines)
- New file: `src/core/encryption.cpp` (~350 lines)
- `src/core/database.cpp` (~120 lines - encrypt/decrypt on I/O)

**Dependencies**: OpenSSL or libsodium

**Acceptance Criteria**:
- [ ] Pages encrypted before write
- [ ] Pages decrypted after read
- [ ] Authentication tag verified (tamper detection)
- [ ] Performance overhead < 15%

---

#### **Task 7.4.2: Key Management Integration** (6-8 hours)

**Description**: Integrate with external key management systems (KMS).

**Supported KMS**:
- Local keyring (encrypted file)
- HashiCorp Vault
- AWS KMS
- Azure Key Vault

**Configuration**:
```sql
-- Local keyring
ALTER DATABASE SET encryption_keyring = '/etc/scratchbird/keyring.enc';

-- HashiCorp Vault
ALTER DATABASE SET encryption_keyring = 'vault://vault.example.com:8200/keys/scratchbird';

-- AWS KMS
ALTER DATABASE SET encryption_keyring = 'aws-kms://us-east-1/key-id';
```

**Files Modified**:
- New file: `include/scratchbird/core/key_management.h` (~100 lines)
- New file: `src/core/key_management.cpp` (~400 lines)

**Acceptance Criteria**:
- [ ] Local keyring works
- [ ] External KMS integration works
- [ ] Key rotation supported

---

### Feature Area 5: Per-Tablespace Buffer Pools

**Purpose**: Isolate buffer pool memory per tablespace for better cache control.

**Priority**: MEDIUM (useful for tiered storage)
**Estimated Effort**: 10-14 hours

---

#### **Task 7.5.1: Separate Buffer Pools** (6-8 hours)

**Description**: Allow each tablespace to have its own dedicated buffer pool.

**Configuration**:
```sql
ALTER TABLESPACE ssd_hot SET BUFFER_POOL_SIZE 1GB;
ALTER TABLESPACE hdd_cold SET BUFFER_POOL_SIZE 128MB;
```

**Benefits**:
- Hot tablespace gets more cache
- Cold tablespace doesn't evict hot pages
- Better cache hit ratios

**Implementation**:
- Create separate `BufferPool` instance per tablespace
- Route page requests to correct buffer pool
- Aggregate statistics across all pools

**Files Modified**:
- `src/core/buffer_pool.cpp` (~200 lines)
- `src/core/tablespace.cpp` (~80 lines)

**Acceptance Criteria**:
- [ ] Each tablespace has own buffer pool
- [ ] Memory limits enforced per pool
- [ ] Cache hit ratio improves for hot tablespace

---

#### **Task 7.5.2: Shared vs Dedicated Pools** (4-6 hours)

**Description**: Allow some tablespaces to share global buffer pool, others to have dedicated pools.

**Configuration**:
```sql
-- Use global buffer pool (default)
ALTER TABLESPACE primary SET BUFFER_POOL shared;

-- Use dedicated buffer pool
ALTER TABLESPACE ssd_hot SET BUFFER_POOL dedicated SIZE 1GB;
```

**Files Modified**:
- `src/core/buffer_pool.cpp` (~150 lines)

**Acceptance Criteria**:
- [ ] Shared pool works
- [ ] Dedicated pools work
- [ ] Memory limits respected

---

### Feature Area 6: Tablespace-Level Backup/Restore

**Purpose**: Backup and restore individual tablespaces.

**Priority**: MEDIUM-HIGH (operational requirement)
**Estimated Effort**: 12-16 hours

---

#### **Task 7.6.1: Tablespace Backup** (6-8 hours)

**Description**: Export a tablespace to a backup file.

**SQL Interface**:
```sql
-- Backup tablespace to file
BACKUP TABLESPACE ssd_hot TO '/backup/ssd_hot_20251021.sbts.bak';

-- Incremental backup (only changed pages since last backup)
BACKUP TABLESPACE ssd_hot TO '/backup/ssd_hot_incr_20251021.sbts.bak' INCREMENTAL;
```

**Implementation**:
- Copy tablespace file to backup location
- Record checkpoint LSN (for consistency)
- Optionally compress backup file

**Files Modified**:
- New file: `src/core/tablespace_backup.cpp` (~300 lines)
- `src/parser/parser.cpp` (~80 lines - BACKUP syntax)

**Acceptance Criteria**:
- [ ] Full backup works
- [ ] Incremental backup works
- [ ] Backup file valid

---

#### **Task 7.6.2: Tablespace Restore** (6-8 hours)

**Description**: Restore a tablespace from backup.

**SQL Interface**:
```sql
-- Restore tablespace
RESTORE TABLESPACE ssd_hot FROM '/backup/ssd_hot_20251021.sbts.bak';

-- Restore to different location
RESTORE TABLESPACE ssd_hot FROM '/backup/ssd_hot_20251021.sbts.bak'
    TO '/new_location/ssd_hot.sbts';
```

**Files Modified**:
- `src/core/tablespace_backup.cpp` (~250 lines)
- `src/parser/parser.cpp` (~60 lines - RESTORE syntax)

**Acceptance Criteria**:
- [ ] Restore works
- [ ] Data accessible after restore
- [ ] Indexes valid after restore

---

### Feature Area 7: Automatic Data Placement

**Purpose**: Automatically move hot data to fast tablespaces, cold data to slow tablespaces.

**Priority**: LOW (nice to have, complex)
**Estimated Effort**: 16-20 hours

---

#### **Task 7.7.1: Hot Page Migration** (8-10 hours)

**Description**: Automatically migrate hot pages from HDD to SSD tablespace.

**Configuration**:
```sql
-- Enable automatic tiering
ALTER TABLESPACE hdd_archive SET AUTO_TIER TO ssd_hot THRESHOLD 80;
-- If page access score > 80, migrate to ssd_hot
```

**Implementation**:
- Background thread monitors page hotness (from Task 7.1.3)
- If page score > threshold, migrate page to target tablespace
- Update indexes to point to new location

**Files Modified**:
- New file: `src/core/auto_tier.cpp` (~400 lines)

**Acceptance Criteria**:
- [ ] Hot pages migrated automatically
- [ ] Indexes updated
- [ ] No data loss

---

#### **Task 7.7.2: Cold Page Demotion** (8-10 hours)

**Description**: Automatically move cold pages from SSD to HDD tablespace.

**Configuration**:
```sql
-- Enable automatic demotion
ALTER TABLESPACE ssd_hot SET AUTO_DEMOTE TO hdd_archive THRESHOLD 20;
-- If page access score < 20, demote to hdd_archive
```

**Files Modified**:
- `src/core/auto_tier.cpp` (~300 lines)

**Acceptance Criteria**:
- [ ] Cold pages demoted automatically
- [ ] Indexes updated
- [ ] SSD space freed

---

### Feature Area 8: Tablespace Replication

**Purpose**: Selectively replicate certain tablespaces to shadow database.

**Priority**: LOW (advanced use case)
**Estimated Effort**: 10-14 hours

---

#### **Task 7.8.1: Selective Tablespace Replication** (6-8 hours)

**Description**: Choose which tablespaces to replicate.

**Configuration**:
```sql
-- Replicate only hot tablespace (critical data)
ALTER TABLESPACE ssd_hot SET REPLICATE TO shadow_db;

-- Do not replicate archive tablespace (non-critical data)
ALTER TABLESPACE hdd_archive SET REPLICATE OFF;
```

**Files Modified**:
- `src/core/replication.cpp` (~200 lines)

**Acceptance Criteria**:
- [ ] Selected tablespaces replicated
- [ ] Non-selected tablespaces not replicated

---

#### **Task 7.8.2: Tablespace-Level Replication Lag** (4-6 hours)

**Description**: Monitor replication lag per tablespace.

**SQL Interface**:
```sql
SELECT * FROM pg_replication_lag;

-- Example output:
-- tablespace | lag_bytes | lag_time | last_replicated
-- ssd_hot    | 1024      | 0.5s     | 2025-10-21 10:30:00
-- primary    | 4096      | 2.0s     | 2025-10-21 10:29:58
```

**Files Modified**:
- `src/core/replication.cpp` (~100 lines)

**Acceptance Criteria**:
- [ ] Lag tracked per tablespace
- [ ] SQL view works

---

## Phase 7 Summary

### All Feature Areas

| Feature Area | Priority | Estimated Hours | Optional? |
|--------------|----------|----------------|-----------|
| **1. Statistics & Monitoring** | HIGH | 12-16 | NO (Required) |
| **2. Quotas & Limits** | MEDIUM | 10-14 | YES (Nice to have) |
| **3. Compression** | MEDIUM-HIGH | 12-16 | YES (Nice to have) |
| **4. Encryption** | MEDIUM | 14-18 | YES (Security requirement) |
| **5. Per-TS Buffer Pools** | MEDIUM | 10-14 | YES (Performance) |
| **6. Backup/Restore** | MEDIUM-HIGH | 12-16 | NO (Required) |
| **7. Auto Data Placement** | LOW | 16-20 | YES (Advanced) |
| **8. Replication** | LOW | 10-14 | YES (Advanced) |

**Total Effort**: 96-128 hours

### Recommended Scope for ALPHA

**MUST HAVE** (Required for production):
- ✅ Feature Area 1: Statistics & Monitoring (12-16 hours)
- ✅ Feature Area 6: Backup/Restore (12-16 hours)

**SHOULD HAVE** (High value):
- Feature Area 3: Compression (12-16 hours)
- Feature Area 4: Encryption (14-18 hours)

**NICE TO HAVE** (If time permits):
- Feature Area 2: Quotas & Limits (10-14 hours)
- Feature Area 5: Per-TS Buffer Pools (10-14 hours)

**DEFER TO POST-ALPHA**:
- Feature Area 7: Auto Data Placement (16-20 hours)
- Feature Area 8: Replication (10-14 hours)

### Recommended ALPHA Scope

**Phase 7 for ALPHA**: 50-66 hours
- Statistics & Monitoring (12-16 hours) - MUST
- Backup/Restore (12-16 hours) - MUST
- Compression (12-16 hours) - SHOULD
- Encryption (14-18 hours) - SHOULD

**Phase 7 Post-ALPHA**: 46-62 hours
- Quotas & Limits (10-14 hours)
- Per-TS Buffer Pools (10-14 hours)
- Auto Data Placement (16-20 hours)
- Replication (10-14 hours)

---

## Complete Implementation Timeline

### All Phases for ALPHA

| Phase | Description | Status | Estimated Hours | Notes |
|-------|-------------|--------|----------------|-------|
| **0** | Research & Spec | ✅ | 24 | COMPLETE |
| **1** | Core Infrastructure | ✅ | 33 | COMPLETE |
| **1.5** | TID Migration | ✅ | 8 | COMPLETE |
| **2** | SQL DDL | ✅ | 21 | COMPLETE |
| **3** | Autoextend | ⏸️ | 12-18 | NOT STARTED |
| **4** | Migration Infra | ✅ | 9.5 | COMPLETE |
| **5.1** | OFFLINE Migration | ⚠️ | 8-12 | TOAST simplified |
| **5.2** | B-Tree Index | ✅ | 2 | COMPLETE |
| **5.3** | Other Indexes | ⏸️ | 17-24 | 5 types pending |
| **5.4** | ONLINE Migration | ⏸️ | 66-96 | NOT STARTED |
| **6** | Attach/Detach | ⏸️ | 20-30 | NOT STARTED |
| **7** | Advanced (ALPHA) | ⏸️ | 50-66 | NOT STARTED |
| **BUG** | MVCC→MGA Fix | ❌ | 2-4 | CRITICAL |

**Total for ALPHA**: **273-376 hours**
**Completed**: **110 hours**
**Remaining**: **163-266 hours**

---

## Success Criteria for ALPHA Completion

**Tablespace functionality is COMPLETE when**:

### Core Functionality
- [ ] ✅ All infrastructure complete (Phases 0-2)
- [ ] ✅ Autoextend works (Phase 3)
- [ ] ✅ OFFLINE migration for all data types (Phase 5.1)
- [ ] ✅ All 7 index types support migration (Phase 5.2-5.3)
- [ ] ✅ ONLINE migration works (Phase 5.4)
- [ ] ✅ Attach/Detach works (Phase 6)

### Advanced Features (Phase 7)
- [ ] ✅ Statistics & monitoring (I/O stats, space usage, hot/cold tracking)
- [ ] ✅ Backup/restore per tablespace
- [ ] ✅ Compression (LZ4, ZSTD)
- [ ] ✅ Encryption (AES-256-GCM)

### Code Quality
- [ ] ✅ Bug #1 fixed (cross-page UPDATE uses MGA, not MVCC)
- [ ] ✅ All tests pass (unit, integration, performance, stress)
- [ ] ✅ Documentation complete (user + developer)
- [ ] ✅ Zero known critical bugs
- [ ] ✅ Performance acceptable (< 5% overhead for non-migrating workloads)

---

**Document Version**: 1.0
**Last Updated**: October 21, 2025
**Status**: COMPREHENSIVE SCOPE COMPLETE
**Next Steps**: Review scope, prioritize features, begin implementation
