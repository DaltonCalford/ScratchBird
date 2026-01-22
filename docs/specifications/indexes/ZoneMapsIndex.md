# Zone Maps (Min-Max Indexes) Specification for ScratchBird

**Version:** 1.1
**Date:** November 20, 2025
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Alpha - Phase 2
**Update:** Page-size agnostic (supports 8K, 16K, 32K, 64K, 128K pages)

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Decision](#architecture-decision)
3. [Storage Model Extensions](#storage-model-extensions)
4. [External Dependencies](#external-dependencies)
5. [On-Disk Structures](#on-disk-structures)
6. [Page Size Considerations](#page-size-considerations)
7. [MGA Compliance](#mga-compliance)
8. [Core API](#core-api)
9. [Statistics Collection](#statistics-collection)
10. [Predicate Evaluation](#predicate-evaluation)
11. [DML Integration](#dml-integration)
12. [Query Planner Integration](#query-planner-integration)
13. [Bytecode Integration](#bytecode-integration)
14. [Implementation Steps](#implementation-steps)
15. [Testing Requirements](#testing-requirements)
16. [Performance Targets](#performance-targets)
17. [Future Enhancements](#future-enhancements)

---

## Overview

### Purpose

Zone Maps enable **data skipping** by storing minimum and maximum values for each column within contiguous data regions ("zones"). This allows the query planner to skip entire zones that cannot contain matching rows.

### Key Characteristics

- **Type:** Auxiliary metadata structure (not a traditional index)
- **Granularity:** Per-zone statistics (zone = extent = 64 contiguous heap pages)
- **Space efficiency:** < 0.1% of table size (typically 180 bytes per zone per column)
- **Query acceleration:** 10-100x speedup for selective range queries
- **Best for:** Sorted or semi-sorted data, time-series, analytical workloads
- **Page-size aware:** Adapts to database page size (8K/16K/32K/64K/128K)

### Classic Use Case

```sql
-- Time-series table (sorted by timestamp)
CREATE TABLE events (
    event_id BIGINT,
    timestamp TIMESTAMP,
    user_id BIGINT,
    event_type VARCHAR(50),
    value NUMERIC
);

-- Create zone map on timestamp
CREATE INDEX idx_timestamp_zonemap ON events USING zonemap(timestamp);

-- Query with selective predicate
SELECT * FROM events
WHERE timestamp BETWEEN '2024-01-01' AND '2024-01-02';

-- Without zone map: Scan 365 days of data (full table scan)
-- With zone map: Skip 363 days, scan only 2 days (99.5% skip rate)
```

### Integration Strategy

**Design Decision:** Zone Maps are **standalone index types** that can be created on one or more columns, similar to how Oracle and ClickHouse implement them.

```
Table: events
├── Heap Pages (row data)
└── Zone Map Index
    ├── Meta Page
    └── Zone Statistics Pages
        ├── Zone 0: [pages 1-64]   → min/max statistics
        ├── Zone 1: [pages 65-128] → min/max statistics
        └── Zone N: ...
```

---

## Architecture Decision

### Defining "Zones" in ScratchBird

**Challenge:** ScratchBird uses row-oriented heap storage, not columnar. The spec (Parquet, ClickHouse) assumes columnar storage with natural zone boundaries.

**Solution:** Introduce the concept of **extents** as zone boundaries.

#### Extent Definition

```cpp
// An extent is a contiguous group of heap pages
struct Extent {
    uint32_t start_page;    // First page in extent
    uint32_t num_pages;     // Number of pages (default: 64)
    uint32_t tablespace_id; // Tablespace containing this extent
};

// Default extent size: 64 pages (size in bytes varies by page size)
constexpr uint32_t DEFAULT_EXTENT_SIZE_PAGES = 64;

// Helper to calculate extent size in bytes (page-size aware)
inline uint32_t getExtentSizeBytes(uint32_t page_size) {
    return DEFAULT_EXTENT_SIZE_PAGES * page_size;
}

// Examples:
// 8KB pages: 64 × 8,192 = 524,288 bytes (512 KB)
// 16KB pages: 64 × 16,384 = 1,048,576 bytes (1 MB)
// 32KB pages: 64 × 32,768 = 2,097,152 bytes (2 MB)
// 64KB pages: 64 × 65,536 = 4,194,304 bytes (4 MB)
// 128KB pages: 64 × 131,072 = 8,388,608 bytes (8 MB)
```

**Rationale:**
- **64 pages per extent** (size scales with page size)
- **Contiguous pages** enable efficient sequential I/O
- **Aligned boundaries** simplify statistics collection
- **Extensible** to different sizes per table (configurable)
- **Page-size agnostic** - works across all ScratchBird configurations

#### Zone = Extent Mapping

```
Table Heap:
┌─────────────────────────────────────────────────────────────┐
│ Extent 0 (Pages 0-63, 512KB)                                │
│ ├── Page 0: [row1, row2, ...]                               │
│ ├── Page 1: [row100, row101, ...]                           │
│ └── ...                                                      │
├─────────────────────────────────────────────────────────────┤
│ Extent 1 (Pages 64-127, 512KB)                              │
│ └── ...                                                      │
└─────────────────────────────────────────────────────────────┘

Zone Map Index:
┌─────────────────────────────────────────────────────────────┐
│ ZoneMapEntry[0]: extent_id=0, col=timestamp                 │
│   min=2024-01-01 00:00:00, max=2024-01-01 23:59:59         │
├─────────────────────────────────────────────────────────────┤
│ ZoneMapEntry[1]: extent_id=1, col=timestamp                 │
│   min=2024-01-02 00:00:00, max=2024-01-02 23:59:59         │
└─────────────────────────────────────────────────────────────┘
```

### Configuration Parameters

```cpp
struct ZoneMapConfig {
    uint32_t extent_size_pages;    // Pages per extent (default: 64)
    uint32_t max_string_len;       // Max bytes for string min/max (default: 64)
    bool track_null_count;         // Track NULL counts (default: true)
    bool track_distinct_estimate;  // Use HyperLogLog for distinct count (default: false)
    uint32_t rebuild_threshold;    // Rebuild after N updates (default: 10000)
};

static constexpr ZoneMapConfig DEFAULT_CONFIG = {
    .extent_size_pages = 64,       // 512KB zones
    .max_string_len = 64,          // First 64 bytes of strings
    .track_null_count = true,
    .track_distinct_estimate = false,  // Phase 2 feature
    .rebuild_threshold = 10000
};
```

---

## Storage Model Extensions

### Extent Metadata in Table

To support zone maps, tables need extent tracking metadata.

#### Table Metadata Extension

**File:** `include/scratchbird/core/storage_engine.h`

```cpp
struct TableExtentInfo {
    uint32_t extent_size_pages;     // Pages per extent
    uint32_t num_extents;           // Total extents allocated
    uint64_t next_extent_id;        // Next extent ID to allocate
    std::vector<Extent> extents;    // Extent metadata (in-memory)
};

struct Table {
    // ... existing fields ...

    // NEW: Extent tracking
    TableExtentInfo extent_info;

    // Helper methods
    uint32_t getExtentId(uint32_t page_number) const;
    Extent* getExtent(uint32_t extent_id);
    Status allocateExtent(uint32_t* extent_id_out, ErrorContext* ctx);
};
```

#### Helper Implementation

```cpp
// Calculate which extent a page belongs to
uint32_t Table::getExtentId(uint32_t page_number) const {
    return page_number / extent_info.extent_size_pages;
}

// Get extent metadata
Extent* Table::getExtent(uint32_t extent_id) {
    if (extent_id >= extent_info.extents.size()) {
        return nullptr;
    }
    return &extent_info.extents[extent_id];
}

// Allocate new extent
Status Table::allocateExtent(uint32_t* extent_id_out, ErrorContext* ctx) {
    Extent extent;
    extent.start_page = extent_info.num_extents * extent_info.extent_size_pages;
    extent.num_pages = extent_info.extent_size_pages;
    extent.tablespace_id = this->tablespace_id;

    extent_info.extents.push_back(extent);
    *extent_id_out = extent_info.num_extents++;

    return Status::OK;
}
```

#### Catalog Integration

Zone maps are tracked in the catalog alongside other indexes.

**File:** `src/core/catalog_manager.cpp`

```cpp
// Add to IndexType enum
enum class IndexType : uint8_t {
    // ... existing types ...
    ZONEMAP = 12  // NEW
};

// Zone map specific metadata
struct ZoneMapInfo {
    std::vector<uint16_t> columns;  // Indexed columns
    uint32_t extent_size_pages;     // Zone granularity
    uint32_t num_zones;             // Total zones
    uint64_t last_rebuild_time;     // Timestamp
};
```

---

## External Dependencies

### None Required

Zone Maps use only standard C++ features:
- ✅ Standard library (std::vector, std::optional, std::variant)
- ✅ ScratchBird's existing data type system
- ✅ No external libraries needed

**Optional Enhancement (Phase 2):** HyperLogLog for distinct count estimation
- Library: HdrHistogram or custom implementation
- For now: Use exact distinct counts or skip this feature

---

## On-Disk Structures

### Page Layout

```
Zone Map Index
├── Meta Page
│   ├── Configuration
│   └── Zone count, column info
└── Zone Statistics Pages
    ├── Page 1: ZoneMapEntry[0..N]
    ├── Page 2: ZoneMapEntry[N+1..M]
    └── ...
```

### 1. Zone Map Meta Page

```cpp
#pragma pack(push, 1)

struct SBZoneMapMetaPage {
    PageHeader zm_header;           // Standard page header (64 bytes)

    // Index metadata (48 bytes)
    uint8_t zm_index_uuid[16];      // Index UUID (16 bytes)
    uint32_t zm_table_id;           // Parent table ID (4 bytes)
    uint16_t zm_num_columns;        // Number of indexed columns (2 bytes)
    uint16_t zm_column_ids[8];      // Column IDs (16 bytes max, supports up to 8 columns)
    uint32_t zm_extent_size_pages;  // Pages per zone/extent (4 bytes)
    uint32_t zm_num_zones;          // Total zones tracked (4 bytes)
    uint16_t zm_max_string_len;     // Max string length for min/max (2 bytes)

    // Storage (16 bytes)
    uint64_t zm_first_stats_page;   // First statistics page (8 bytes)
    uint32_t zm_num_stats_pages;    // Number of statistics pages (4 bytes)
    uint32_t zm_reserved1;          // Reserved (4 bytes)

    // Statistics (32 bytes)
    uint64_t zm_total_queries;      // Total queries using zone map (8 bytes)
    uint64_t zm_zones_skipped;      // Zones skipped (8 bytes)
    uint64_t zm_zones_scanned;      // Zones scanned (8 bytes)
    uint64_t zm_last_rebuild_time;  // Unix timestamp (8 bytes)

    // Configuration (16 bytes)
    uint8_t zm_flags;               // Feature flags (1 byte)
    uint8_t zm_reserved2[15];       // Reserved (15 bytes)

    // Padding to fill page (flexible array member)
    uint8_t zm_padding[];           // Flexible - size varies by page_size
} __attribute__((packed));

// No static_assert - size varies by page size (8KB to 128KB)
// Fixed header size: 64 + 48 + 16 + 32 + 16 = 176 bytes

// Flag definitions
constexpr uint8_t ZM_FLAG_TRACK_NULLS = 0x01;
constexpr uint8_t ZM_FLAG_TRACK_DISTINCT = 0x02;
constexpr uint8_t ZM_FLAG_COMPRESSED = 0x04;

#pragma pack(pop)
```

### 2. Zone Statistics Entry

Each zone (extent) needs statistics for each indexed column.

```cpp
#pragma pack(push, 1)

// Value storage (union for different types)
struct ZoneMapValue {
    uint8_t type;           // DataType enum (1 byte)
    uint8_t is_null;        // Is NULL (1 byte)
    uint8_t reserved[2];    // Alignment (2 bytes)

    union {
        int32_t i32;
        int64_t i64;
        double f64;
        uint8_t bytes[64];  // For strings, UUIDs, etc. (first 64 bytes)
    } data;
} __attribute__((packed));

static_assert(sizeof(ZoneMapValue) == 68, "ZoneMapValue must be 68 bytes");

// Statistics for one column in one zone
struct ZoneColumnStats {
    uint16_t column_id;         // Column ID (2 bytes)
    uint16_t flags;             // Flags (2 bytes)

    ZoneMapValue min_value;     // Minimum value (68 bytes)
    ZoneMapValue max_value;     // Maximum value (68 bytes)

    uint64_t null_count;        // Number of NULLs (8 bytes)
    uint64_t row_count;         // Total rows in zone (8 bytes)
    uint64_t distinct_count;    // Distinct values (8 bytes, 0 = unknown)

    uint64_t reserved[2];       // Reserved for future use (16 bytes)
} __attribute__((packed));

static_assert(sizeof(ZoneColumnStats) == 180, "ZoneColumnStats must be 180 bytes");

// Flags
constexpr uint16_t ZCS_FLAG_ALL_NULL = 0x01;
constexpr uint16_t ZCS_FLAG_NO_NULL = 0x02;
constexpr uint16_t ZCS_FLAG_SORTED = 0x04;
constexpr uint16_t ZCS_FLAG_STALE = 0x08;  // Needs rebuild

#pragma pack(pop)
```

### 3. Zone Map Statistics Page

```cpp
#pragma pack(push, 1)

struct SBZoneMapStatsPage {
    PageHeader zm_header;           // Standard page header (64 bytes)
    uint64_t zm_next_page;          // Next stats page (0 if last) (8 bytes)
    uint32_t zm_zone_start_id;      // First zone ID on this page (4 bytes)
    uint16_t zm_num_entries;        // Number of entries on page (2 bytes)
    uint16_t zm_num_columns;        // Columns per entry (2 bytes)

    // Entries: Each entry = num_columns × ZoneColumnStats
    // Flexible array member - size varies by page size
    uint8_t zm_stats_data[];        // Variable-length stats (flexible array)
} __attribute__((packed));

// No static_assert - size varies by page size (8KB to 128KB)
// Fixed header size: 64 + 8 + 4 + 2 + 2 = 80 bytes

// Calculate max zones per page (page-size aware)
inline uint32_t maxZonesPerPage(uint32_t page_size, uint16_t num_columns) {
    constexpr uint32_t STATS_PAGE_HEADER_SIZE = 80;
    uint32_t available_bytes = page_size - STATS_PAGE_HEADER_SIZE;
    uint32_t bytes_per_zone = num_columns * sizeof(ZoneColumnStats);  // num_columns × 180
    return available_bytes / bytes_per_zone;
}

#pragma pack(pop)
```

### Storage Calculation

```cpp
// Example: 1TB table with 8KB pages, 64 pages per extent = 512KB zones
// Total zones: 1TB / 512KB = 2,097,152 zones
// Stats per zone (1 column): 180 bytes
// Total stats: 2,097,152 × 180 = 377 MB (0.037% of table size)

// Example: 10GB table with 16KB pages, 64 pages per extent = 1MB zones
// Total zones: 10GB / 1MB = 10,240 zones
// Stats per zone (3 columns): 540 bytes
// Total stats: 10,240 × 540 = 5.4 MB (0.054% of table size)

// Example: 100GB table with 32KB pages, 64 pages per extent = 2MB zones
// Total zones: 100GB / 2MB = 51,200 zones
// Stats per zone (2 columns): 360 bytes
// Total stats: 51,200 × 360 = 18 MB (0.018% of table size)
```

---

## Page Size Considerations

Zone Maps in ScratchBird adapt to the database page size configured at creation time. This section details how different page sizes affect zone map capacity and performance.

### Supported Page Sizes

ScratchBird supports the following page sizes (all powers of 2):
- 8 KB (8,192 bytes)
- 16 KB (16,384 bytes)
- 32 KB (32,768 bytes)
- 64 KB (65,536 bytes)
- 128 KB (131,072 bytes)

### Extent Size Scaling

Zone Maps use a **fixed number of pages per extent** (64 pages by default), which means extent size in bytes scales with page size:

| Page Size | Pages/Extent | Extent Size (Bytes) | Extent Size (Human) |
|-----------|--------------|---------------------|---------------------|
| 8 KB      | 64           | 524,288             | 512 KB              |
| 16 KB     | 64           | 1,048,576           | 1 MB                |
| 32 KB     | 64           | 2,097,152           | 2 MB                |
| 64 KB     | 64           | 4,194,304           | 4 MB                |
| 128 KB    | 64           | 8,388,608           | 8 MB                |

**Implication:** Larger page sizes result in larger zones, which means:
- **Fewer zones** for the same table size
- **Coarser granularity** for data skipping
- **Less metadata overhead**
- **Potentially lower skip rates** (less selective filtering)

### Zones Per Stats Page

The number of zone statistics that fit on a single stats page varies by page size and column count:

**Formula:** `(page_size - 80) / (num_columns × 180)`

#### Single-Column Zone Map

| Page Size | Available Bytes | Zones Per Page |
|-----------|-----------------|----------------|
| 8 KB      | 8,112           | 45             |
| 16 KB     | 16,304          | 90             |
| 32 KB     | 32,688          | 181            |
| 64 KB     | 65,456          | 363            |
| 128 KB    | 130,992         | 727            |

#### Two-Column Zone Map

| Page Size | Available Bytes | Zones Per Page |
|-----------|-----------------|----------------|
| 8 KB      | 8,112           | 22             |
| 16 KB     | 16,304          | 45             |
| 32 KB     | 32,688          | 90             |
| 64 KB     | 65,456          | 181            |
| 128 KB    | 130,992         | 363            |

#### Three-Column Zone Map

| Page Size | Available Bytes | Zones Per Page |
|-----------|-----------------|----------------|
| 8 KB      | 8,112           | 15             |
| 16 KB     | 16,304          | 30             |
| 32 KB     | 32,688          | 60             |
| 64 KB     | 65,456          | 121            |
| 128 KB    | 130,992         | 242            |

### Stats Pages Required

The number of stats pages needed for a zone map depends on table size, page size, extent size, and column count.

**Formula:** `ceiling(num_zones / zones_per_page)`

**Example 1:** 1 TB table, 8KB pages, 1 column
- Extent size: 512 KB
- Total zones: 1 TB / 512 KB = 2,097,152
- Zones per page: 45
- Stats pages: 2,097,152 / 45 = **46,604 pages** = 363 MB

**Example 2:** 1 TB table, 16KB pages, 1 column
- Extent size: 1 MB
- Total zones: 1 TB / 1 MB = 1,048,576
- Zones per page: 90
- Stats pages: 1,048,576 / 90 = **11,651 pages** = 181 MB

**Example 3:** 100 GB table, 32KB pages, 2 columns
- Extent size: 2 MB
- Total zones: 100 GB / 2 MB = 51,200
- Zones per page: 90
- Stats pages: 51,200 / 90 = **569 pages** = 17.8 MB

### Trade-offs by Page Size

#### Small Pages (8KB)
**Pros:**
- Fine-grained zones (512 KB) → better skip rates
- More selective filtering
- Better for sorted/semi-sorted data

**Cons:**
- More zones to track → more metadata
- More stats pages → more I/O to read zone map
- Higher rebuild cost (more zones)

**Best for:** OLTP workloads, small tables (< 100 GB), highly selective queries

#### Medium Pages (16KB - 32KB)
**Pros:**
- Balanced zone size (1-2 MB)
- Moderate metadata overhead
- Good skip rates for most workloads

**Cons:**
- Slightly coarser than 8KB

**Best for:** Mixed OLTP/OLAP workloads, medium tables (100 GB - 1 TB)

#### Large Pages (64KB - 128KB)
**Pros:**
- Very coarse zones (4-8 MB) → minimal metadata
- Fewer stats pages → fast zone map loading
- Low rebuild cost

**Cons:**
- Coarse granularity → lower skip rates
- Less effective for selective queries
- May scan more unnecessary data

**Best for:** OLAP workloads, large tables (> 1 TB), range scans

### Runtime Calculation Helpers

The `ZoneMapIndex` class provides runtime methods to adapt to the database page size:

```cpp
class ZoneMapIndex {
public:
    // Get current database page size
    uint32_t getPageSize() const {
        return db_->getPageSize();
    }

    // Calculate extent size in bytes
    uint32_t getExtentSizeBytes() const {
        return config_.extent_size_pages * getPageSize();
    }

    // Calculate max zones per stats page
    uint32_t getMaxZonesPerPage() const {
        return maxZonesPerPage(getPageSize(), columns_.size());
    }

    // Calculate number of stats pages needed
    uint32_t calculateStatsPagesNeeded(uint32_t num_zones) const {
        uint32_t zones_per_page = getMaxZonesPerPage();
        return (num_zones + zones_per_page - 1) / zones_per_page;  // Ceiling
    }

    // Validate configuration at creation time
    static Status validateConfig(uint32_t page_size, const ZoneMapConfig& config) {
        // Check page size range
        if (page_size < 8192 || page_size > 131072) {
            return Status::INVALID_ARGUMENT;
        }

        // Check page size is power of 2
        if ((page_size & (page_size - 1)) != 0) {
            return Status::INVALID_ARGUMENT;
        }

        // Check extent size is reasonable
        uint32_t extent_bytes = config.extent_size_pages * page_size;
        if (extent_bytes < page_size || extent_bytes > 16 * 1024 * 1024) {
            return Status::INVALID_ARGUMENT;  // Min 1 page, max 16 MB
        }

        return Status::OK;
    }
};
```

### Recommendations

**For Time-Series Data (sorted by timestamp):**
- Use **8KB or 16KB pages** for fine-grained zones
- Expect 95-99.9% skip rates for selective time range queries
- Example: Log tables, sensor data, financial ticks

**For Analytical Workloads (large scans):**
- Use **32KB or 64KB pages** for coarser zones
- Lower metadata overhead, faster zone map loading
- Still effective for date partitioning, categorical filters

**For Very Large Tables (> 10 TB):**
- Consider **64KB or 128KB pages**
- Minimize zone map metadata size
- May need to increase `extent_size_pages` beyond 64 for even coarser zones

**General Rule:**
- Page size should match your workload's I/O patterns
- Zone maps adapt automatically to your choice
- No code changes needed when page size changes

---

## MGA Compliance

### Challenge: Statistics and MVCC Visibility

Zone maps store aggregate statistics (min, max, count) that change as transactions insert, update, or delete rows. In an MGA system:

**Problem:**
- Transaction T1 inserts rows into extent E (updates zone map statistics)
- Transaction T1 rolls back
- Zone map statistics are now incorrect (include rolled-back rows)

**Solution 1: Optimistic Statistics (RECOMMENDED)**

Zone maps provide **optimistic pruning** and may include uncommitted data temporarily.

```cpp
// Zone map check during query planning
bool ZoneMap::canSkipZone(uint32_t zone_id, const Predicate& pred) {
    auto stats = getZoneStats(zone_id);

    // Optimistic check: Use current min/max (may include uncommitted data)
    if (pred.value < stats.min_value || pred.value > stats.max_value) {
        return true;  // Definitely can skip
    }

    return false;  // Might contain matching rows (scan zone)
}

// Correctness preserved by heap page visibility checks
// Even if zone map says "might match", heap tuples still checked with TIP
```

**Implications:**
1. **False negatives impossible** (won't skip zones with matching data)
2. **False positives possible** (may scan zones with only uncommitted data)
3. **Correctness guaranteed** by TIP checks on heap tuples
4. **Statistics cleanup** during garbage collection

**Solution 2: Pessimistic Statistics (Alternative)**

Track xmin/xmax for each zone's min/max value (complex, not recommended).

### Chosen Approach: Optimistic + Periodic Rebuild

```cpp
struct ZoneColumnStats {
    // ... existing fields ...

    // NO xmin/xmax - statistics are optimistic
    // Rebuild during GC to remove rolled-back data
};

// Rebuild trigger conditions:
// 1. After sweep/GC
// 2. After N updates (configurable threshold)
// 3. Manual REINDEX command
// 4. When statistics age > threshold (e.g., 1 hour)
```

### MGA Compliance Summary

✅ **Correctness:** Zone maps don't affect correctness (heap TIP checks still happen)
✅ **Performance:** Temporary false positives are acceptable
⚠️ **Maintenance:** Require periodic rebuilds to remove stale statistics
✅ **Concurrency:** Read statistics without locking, rebuild with exclusive lock

---

## Tablespace + TID/GPID Requirements

- **Extent ownership:** Zone entries must record `tablespace_id` for each extent and never assume primary tablespace.
- **TID format:** Any tuple references emitted from zone pruning must use `TID` with full `GPID + slot`.
- **Tablespace routing:** Zone map pages must allocate/pin via `root_gpid` and `tablespace_id`.
- **Migration:** When a table moves tablespaces, the zone map must update extent metadata and any stored `TID` references.

---

## Core API

### Class Definition

**File:** `include/scratchbird/core/zonemap_index.h`

```cpp
#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/value.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <optional>

namespace scratchbird {
namespace core {

// Forward declarations
class Database;
class Table;

// Predicate types for zone pruning
enum class PredicateType {
    EQUALS,
    LESS_THAN,
    LESS_THAN_OR_EQUAL,
    GREATER_THAN,
    GREATER_THAN_OR_EQUAL,
    BETWEEN,
    IS_NULL,
    IS_NOT_NULL
};

struct Predicate {
    PredicateType type;
    uint16_t column_id;
    Value value;                // For comparison predicates
    std::optional<Value> value2; // For BETWEEN
};

// Skip decision
enum class SkipDecision {
    EXCLUDE,    // Definitely skip this zone
    INCLUDE,    // Definitely scan this zone
    MAYBE       // Need to scan to be sure
};

// Statistics
struct ZoneMapStatistics {
    uint32_t num_zones;
    uint64_t total_queries;
    uint64_t zones_skipped;
    uint64_t zones_scanned;
    double skip_rate;           // zones_skipped / total_queries
    uint64_t last_rebuild_time;
};

class ZoneMapIndex : public IndexGCInterface {
public:
    // Constructor
    ZoneMapIndex(Database* db, const UuidV7Bytes& index_uuid, uint32_t meta_page);

    // Destructor
    ~ZoneMapIndex();

    // Create new zone map
    static Status create(Database* db,
                        Table* table,
                        const std::vector<uint16_t>& columns,
                        const ZoneMapConfig& config,
                        uint32_t* meta_page_out,
                        ErrorContext* ctx = nullptr);

    // Open existing zone map
    static std::unique_ptr<ZoneMapIndex> open(Database* db,
                                             const UuidV7Bytes& index_uuid,
                                             uint32_t meta_page,
                                             ErrorContext* ctx = nullptr);

    // Update statistics for a zone
    Status updateZoneStats(uint32_t zone_id,
                          uint16_t column_id,
                          const std::vector<Value>& values,
                          ErrorContext* ctx = nullptr);

    // Evaluate predicate against zone
    SkipDecision evaluatePredicate(uint32_t zone_id,
                                   const Predicate& predicate,
                                   ErrorContext* ctx = nullptr);

    // Get zones that might contain matching rows
    std::vector<uint32_t> filterZones(const std::vector<Predicate>& predicates,
                                     ErrorContext* ctx = nullptr);

    // Rebuild all statistics
    Status rebuild(ErrorContext* ctx = nullptr);

    // IndexGCInterface implementation
    Status removeDeadEntries(const std::vector<TID>& dead_tids,
                            uint64_t* entries_removed_out = nullptr,
                            uint64_t* pages_modified_out = nullptr,
                            ErrorContext* ctx = nullptr) override;

    const char* indexTypeName() const override { return "ZoneMap"; }

    // Statistics
    ZoneMapStatistics getStatistics() const;

    // Getters
    const UuidV7Bytes& getIndexUuid() const { return index_uuid_; }
    uint32_t getMetaPage() const { return meta_page_; }
    const std::vector<uint16_t>& getColumns() const { return columns_; }

    // Page-size aware helpers
    uint32_t getPageSize() const;
    uint32_t getExtentSizeBytes() const;
    uint32_t getMaxZonesPerPage() const;
    uint32_t calculateStatsPagesNeeded(uint32_t num_zones) const;

    // Configuration validation
    static Status validateConfig(uint32_t page_size,
                                const ZoneMapConfig& config,
                                ErrorContext* ctx = nullptr);

private:
    // Member variables
    Database* db_;
    UuidV7Bytes index_uuid_;
    uint32_t meta_page_;
    std::vector<uint16_t> columns_;
    ZoneMapConfig config_;

    // Helper methods
    Status loadZoneStats(uint32_t zone_id,
                        std::vector<ZoneColumnStats>* stats_out,
                        ErrorContext* ctx);

    Status saveZoneStats(uint32_t zone_id,
                        const std::vector<ZoneColumnStats>& stats,
                        ErrorContext* ctx);

    SkipDecision evaluateSinglePredicate(const ZoneColumnStats& stats,
                                         const Predicate& pred);

    SkipDecision combineAND(const std::vector<SkipDecision>& decisions);
    SkipDecision combineOR(const std::vector<SkipDecision>& decisions);

    Status rebuildZone(uint32_t zone_id, ErrorContext* ctx);
};

} // namespace core
} // namespace scratchbird
```

---

## Statistics Collection

### Incremental Collection During INSERT

**File:** `src/core/storage_engine.cpp`

```cpp
Status StorageEngine::insertTuple(Table* table,
                                 const std::vector<Value>& values,
                                 TID* tid_out,
                                 ErrorContext* ctx) {
    // ... existing heap insert logic ...
    // Allocate page, insert tuple, etc.

    // NEW: Update zone map statistics
    for (auto& idx_info : table->indexes) {
        if (idx_info.type == IndexType::ZONEMAP) {
            auto* zonemap = static_cast<ZoneMapIndex*>(idx_info.index.get());

            // Determine which zone this page belongs to
            uint32_t zone_id = table->getExtentId(tid_out->page());

            // Extract indexed column values
            std::vector<Value> indexed_values;
            for (uint16_t col_id : zonemap->getColumns()) {
                indexed_values.push_back(values[col_id]);
            }

            // Update zone statistics
            auto status = zonemap->updateZoneStats(zone_id,
                                                  zonemap->getColumns()[0],
                                                  indexed_values,
                                                  ctx);
            if (status != Status::OK) {
                LOG_WARN(Category::INDEX, "Failed to update zone map: %d", status);
                // Non-fatal
            }
        }
    }

    return Status::OK;
}
```

### Zone Statistics Update Algorithm

```cpp
Status ZoneMapIndex::updateZoneStats(uint32_t zone_id,
                                    uint16_t column_id,
                                    const std::vector<Value>& values,
                                    ErrorContext* ctx) {
    // Load current statistics
    std::vector<ZoneColumnStats> stats;
    auto status = loadZoneStats(zone_id, &stats, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Find column in statistics
    ZoneColumnStats* col_stats = nullptr;
    for (auto& s : stats) {
        if (s.column_id == column_id) {
            col_stats = &s;
            break;
        }
    }

    if (!col_stats) {
        return Status::INTERNAL_ERROR;
    }

    // Update min/max incrementally
    for (const auto& val : values) {
        if (val.isNull()) {
            col_stats->null_count++;
        } else {
            // Update min
            if (col_stats->min_value.is_null ||
                compareValues(val, col_stats->min_value) < 0) {
                serializeValue(val, &col_stats->min_value);
            }

            // Update max
            if (col_stats->max_value.is_null ||
                compareValues(val, col_stats->max_value) > 0) {
                serializeValue(val, &col_stats->max_value);
            }
        }

        col_stats->row_count++;
    }

    // Save updated statistics
    return saveZoneStats(zone_id, stats, ctx);
}
```

### Bulk Statistics Collection (Initial Build)

```cpp
Status ZoneMapIndex::rebuild(ErrorContext* ctx) {
    auto* table = db_->getCatalogManager()->getTable(table_id_, ctx);
    if (!table) {
        return Status::TABLE_NOT_FOUND;
    }

    // Iterate through all zones
    for (uint32_t zone_id = 0; zone_id < table->extent_info.num_extents; zone_id++) {
        auto status = rebuildZone(zone_id, ctx);
        if (status != Status::OK) {
            return status;
        }
    }

    // Update metadata
    // ... update last_rebuild_time ...

    return Status::OK;
}

Status ZoneMapIndex::rebuildZone(uint32_t zone_id, ErrorContext* ctx) {
    // Initialize statistics
    std::vector<ZoneColumnStats> stats;
    for (uint16_t col_id : columns_) {
        ZoneColumnStats s;
        s.column_id = col_id;
        s.flags = 0;
        s.min_value.is_null = 1;
        s.max_value.is_null = 1;
        s.null_count = 0;
        s.row_count = 0;
        s.distinct_count = 0;
        stats.push_back(s);
    }

    // Scan all pages in zone
    auto* table = db_->getCatalogManager()->getTable(table_id_, ctx);
    auto extent = table->getExtent(zone_id);
    if (!extent) {
        return Status::OK;  // Empty zone
    }

    auto* txn_mgr = db_->getTransactionManager();
    TransactionId oit = 0, oat = 0, ost = 0, next = 0;
    txn_mgr->getTransactionMarkers(oit, oat, ost, next);

    for (uint32_t page_offset = 0; page_offset < extent->num_pages; page_offset++) {
        uint32_t page_num = extent->start_page + page_offset;

        // Read heap page
        BufferFrame* frame = nullptr;
        auto status = db_->getBufferPool()->pinPage(page_num, &frame, ctx);
        if (status != Status::OK) continue;

        auto* heap_page = reinterpret_cast<SBHeapPage*>(frame->getData());

        // Scan all tuples
        for (uint16_t slot = 0; slot < heap_page->hp_tuple_count; slot++) {
            // Check visibility (only include committed tuples)
            TID tid(page_num, slot);
            auto tuple = getTupleFromSlot(heap_page, slot);
            if (!tuple) continue;

            // Only include committed, non-deleted tuples
            if (tuple->xmin >= oit || tuple->xmax != 0) {
                continue;  // Skip uncommitted or deleted
            }

            // Extract column values
            for (size_t i = 0; i < columns_.size(); i++) {
                auto val = extractColumnValue(tuple, columns_[i]);

                if (val.isNull()) {
                    stats[i].null_count++;
                } else {
                    // Update min/max
                    if (stats[i].min_value.is_null ||
                        compareValues(val, stats[i].min_value) < 0) {
                        serializeValue(val, &stats[i].min_value);
                    }
                    if (stats[i].max_value.is_null ||
                        compareValues(val, stats[i].max_value) > 0) {
                        serializeValue(val, &stats[i].max_value);
                    }
                }

                stats[i].row_count++;
            }
        }

        db_->getBufferPool()->unpinPage(page_num);
    }

    // Save statistics
    return saveZoneStats(zone_id, stats, ctx);
}
```

---

## Predicate Evaluation

### Evaluation Algorithm

```cpp
SkipDecision ZoneMapIndex::evaluateSinglePredicate(
    const ZoneColumnStats& stats,
    const Predicate& pred) {

    switch (pred.type) {
        case PredicateType::EQUALS: {
            // value < min OR value > max → EXCLUDE
            if (compareValues(pred.value, stats.min_value) < 0 ||
                compareValues(pred.value, stats.max_value) > 0) {
                return SkipDecision::EXCLUDE;
            }
            // value == min == max → INCLUDE
            if (compareValues(stats.min_value, stats.max_value) == 0 &&
                compareValues(pred.value, stats.min_value) == 0) {
                return SkipDecision::INCLUDE;
            }
            return SkipDecision::MAYBE;
        }

        case PredicateType::LESS_THAN: {
            // max < value → INCLUDE (all rows satisfy)
            if (compareValues(stats.max_value, pred.value) < 0) {
                return SkipDecision::INCLUDE;
            }
            // min >= value → EXCLUDE (no rows satisfy)
            if (compareValues(stats.min_value, pred.value) >= 0) {
                return SkipDecision::EXCLUDE;
            }
            return SkipDecision::MAYBE;
        }

        case PredicateType::GREATER_THAN: {
            // min > value → INCLUDE
            if (compareValues(stats.min_value, pred.value) > 0) {
                return SkipDecision::INCLUDE;
            }
            // max <= value → EXCLUDE
            if (compareValues(stats.max_value, pred.value) <= 0) {
                return SkipDecision::EXCLUDE;
            }
            return SkipDecision::MAYBE;
        }

        case PredicateType::BETWEEN: {
            // max < low OR min > high → EXCLUDE
            if (compareValues(stats.max_value, pred.value) < 0 ||
                compareValues(stats.min_value, *pred.value2) > 0) {
                return SkipDecision::EXCLUDE;
            }
            // min >= low AND max <= high → INCLUDE
            if (compareValues(stats.min_value, pred.value) >= 0 &&
                compareValues(stats.max_value, *pred.value2) <= 0) {
                return SkipDecision::INCLUDE;
            }
            return SkipDecision::MAYBE;
        }

        case PredicateType::IS_NULL: {
            if (stats.null_count == 0) {
                return SkipDecision::EXCLUDE;
            }
            if (stats.null_count == stats.row_count) {
                return SkipDecision::INCLUDE;
            }
            return SkipDecision::MAYBE;
        }

        case PredicateType::IS_NOT_NULL: {
            if (stats.null_count == stats.row_count) {
                return SkipDecision::EXCLUDE;
            }
            if (stats.null_count == 0) {
                return SkipDecision::INCLUDE;
            }
            return SkipDecision::MAYBE;
        }
    }

    return SkipDecision::MAYBE;
}
```

### Combining Predicates

```cpp
SkipDecision ZoneMapIndex::combineAND(const std::vector<SkipDecision>& decisions) {
    // AND: Any EXCLUDE → zone excluded
    for (auto d : decisions) {
        if (d == SkipDecision::EXCLUDE) {
            return SkipDecision::EXCLUDE;
        }
    }

    // All INCLUDE → zone definitely matches
    bool all_include = true;
    for (auto d : decisions) {
        if (d != SkipDecision::INCLUDE) {
            all_include = false;
            break;
        }
    }
    if (all_include) {
        return SkipDecision::INCLUDE;
    }

    return SkipDecision::MAYBE;
}

SkipDecision ZoneMapIndex::combineOR(const std::vector<SkipDecision>& decisions) {
    // OR: All EXCLUDE → zone excluded
    bool all_exclude = true;
    for (auto d : decisions) {
        if (d != SkipDecision::EXCLUDE) {
            all_exclude = false;
            break;
        }
    }
    if (all_exclude) {
        return SkipDecision::EXCLUDE;
    }

    // Any INCLUDE → zone definitely matches
    for (auto d : decisions) {
        if (d == SkipDecision::INCLUDE) {
            return SkipDecision::INCLUDE;
        }
    }

    return SkipDecision::MAYBE;
}
```

### Filter Zones API

```cpp
std::vector<uint32_t> ZoneMapIndex::filterZones(
    const std::vector<Predicate>& predicates,
    ErrorContext* ctx) {

    std::vector<uint32_t> matching_zones;

    auto* table = db_->getCatalogManager()->getTable(table_id_, ctx);
    if (!table) {
        return matching_zones;
    }

    // Evaluate each zone
    for (uint32_t zone_id = 0; zone_id < table->extent_info.num_extents; zone_id++) {
        // Load statistics
        std::vector<ZoneColumnStats> stats;
        if (loadZoneStats(zone_id, &stats, ctx) != Status::OK) {
            continue;
        }

        // Evaluate all predicates
        std::vector<SkipDecision> decisions;
        for (const auto& pred : predicates) {
            // Find column stats
            ZoneColumnStats* col_stats = nullptr;
            for (auto& s : stats) {
                if (s.column_id == pred.column_id) {
                    col_stats = &s;
                    break;
                }
            }

            if (!col_stats) {
                decisions.push_back(SkipDecision::MAYBE);
                continue;
            }

            auto decision = evaluateSinglePredicate(*col_stats, pred);
            decisions.push_back(decision);
        }

        // Combine decisions (assume AND for now)
        auto final_decision = combineAND(decisions);

        if (final_decision != SkipDecision::EXCLUDE) {
            matching_zones.push_back(zone_id);
        } else {
            // Update statistics
            zones_skipped_++;
        }
    }

    total_queries_++;
    return matching_zones;
}
```

---

## DML Integration

### INSERT Hook

Already covered in Statistics Collection section.

### UPDATE Hook

```cpp
Status StorageEngine::updateTuple(Table* table,
                                 const TID& tid,
                                 const std::vector<Value>& new_values,
                                 ErrorContext* ctx) {
    // ... existing update logic ...

    // NEW: Update zone map if indexed columns changed
    for (auto& idx_info : table->indexes) {
        if (idx_info.type == IndexType::ZONEMAP) {
            auto* zonemap = static_cast<ZoneMapIndex*>(idx_info.index.get());

            // Check if any indexed column changed
            bool changed = false;
            for (uint16_t col_id : zonemap->getColumns()) {
                if (old_values[col_id] != new_values[col_id]) {
                    changed = true;
                    break;
                }
            }

            if (changed) {
                // Mark zone as stale (rebuild later)
                uint32_t zone_id = table->getExtentId(tid.page());
                auto status = zonemap->markZoneStale(zone_id, ctx);
                if (status != Status::OK) {
                    LOG_WARN(Category::INDEX, "Failed to mark zone stale");
                }

                // Optionally rebuild immediately (if threshold reached)
                if (shouldRebuildZone(zonemap, zone_id)) {
                    zonemap->rebuildZone(zone_id, ctx);
                }
            }
        }
    }

    return Status::OK;
}
```

### DELETE Hook

```cpp
Status StorageEngine::deleteTuple(Table* table, const TID& tid, ErrorContext* ctx) {
    // ... existing delete logic (sets xmax) ...

    // Zone maps don't need immediate update on DELETE
    // Statistics will be corrected during rebuild (when GC runs)

    return Status::OK;
}
```

---

## Query Planner Integration

This is the most complex part of zone maps implementation.

### Scan Plan Enhancement

**File:** `src/sblr/query_planner.cpp`

```cpp
struct ScanPlan {
    enum Type {
        FULL_SCAN,      // Scan all pages
        INDEX_SCAN,     // Use B-Tree/Hash index
        ZONE_FILTERED   // Use zone map to filter extents
    };

    Type type;
    Table* table;
    std::vector<Predicate> predicates;

    // NEW: For zone-filtered scans
    std::vector<uint32_t> zones_to_scan;  // Zone IDs to scan
    ZoneMapIndex* zonemap;                // Zone map used
};

Status QueryPlanner::planTableScan(Table* table,
                                   const std::vector<Predicate>& predicates,
                                   ScanPlan* plan_out,
                                   ErrorContext* ctx) {
    // Option 1: Full table scan
    double full_scan_cost = estimateFullScanCost(table);

    // Option 2: Index scan
    double index_scan_cost = INFINITY;
    Index* best_index = nullptr;
    for (auto& idx : table->indexes) {
        if (idx.type != IndexType::ZONEMAP) {
            double cost = estimateIndexScanCost(idx, predicates);
            if (cost < index_scan_cost) {
                index_scan_cost = cost;
                best_index = idx.index.get();
            }
        }
    }

    // Option 3: Zone map filtered scan
    double zonemap_cost = INFINITY;
    ZoneMapIndex* zonemap = nullptr;
    std::vector<uint32_t> zones_to_scan;

    for (auto& idx : table->indexes) {
        if (idx.type == IndexType::ZONEMAP) {
            auto* zm = static_cast<ZoneMapIndex*>(idx.index.get());

            // Check if predicates can use this zone map
            if (canUseZoneMap(zm, predicates)) {
                auto zones = zm->filterZones(predicates, ctx);

                // Calculate cost
                double zone_metadata_cost = 0.1;  // Load zone map metadata
                double extent_scan_cost = zones.size() * estimateExtentScanCost(table);
                double total_cost = zone_metadata_cost + extent_scan_cost;

                if (total_cost < zonemap_cost) {
                    zonemap_cost = total_cost;
                    zonemap = zm;
                    zones_to_scan = zones;
                }
            }
        }
    }

    // Choose best plan
    if (zonemap_cost < full_scan_cost && zonemap_cost < index_scan_cost) {
        plan_out->type = ScanPlan::ZONE_FILTERED;
        plan_out->table = table;
        plan_out->predicates = predicates;
        plan_out->zones_to_scan = zones_to_scan;
        plan_out->zonemap = zonemap;

        LOG_INFO(Category::PLANNER,
                "Using zone map: %u/%u zones selected (%.1f%% skip rate)",
                zones_to_scan.size(),
                table->extent_info.num_extents,
                100.0 * (1.0 - zones_to_scan.size() / (double)table->extent_info.num_extents));
    } else if (index_scan_cost < full_scan_cost) {
        plan_out->type = ScanPlan::INDEX_SCAN;
        // ... fill index scan plan ...
    } else {
        plan_out->type = ScanPlan::FULL_SCAN;
        // ... fill full scan plan ...
    }

    return Status::OK;
}

bool QueryPlanner::canUseZoneMap(ZoneMapIndex* zm,
                                 const std::vector<Predicate>& predicates) {
    // Check if any predicate matches zone map columns
    for (const auto& pred : predicates) {
        for (uint16_t col_id : zm->getColumns()) {
            if (pred.column_id == col_id) {
                // Check if predicate type is supported
                switch (pred.type) {
                    case PredicateType::EQUALS:
                    case PredicateType::LESS_THAN:
                    case PredicateType::GREATER_THAN:
                    case PredicateType::BETWEEN:
                    case PredicateType::IS_NULL:
                        return true;
                    default:
                        break;
                }
            }
        }
    }
    return false;
}
```

### Executor Integration

**File:** `src/sblr/executor.cpp`

```cpp
Status Executor::executeZoneFilteredScan(const ScanPlan& plan,
                                        std::vector<TID>* results_out,
                                        ErrorContext* ctx) {
    results_out->clear();

    auto* table = plan.table;
    auto current_xid = db_->getTransactionManager()->getCurrentTransactionId();

    // Scan only selected zones
    for (uint32_t zone_id : plan.zones_to_scan) {
        auto extent = table->getExtent(zone_id);
        if (!extent) continue;

        // Scan all pages in extent
        for (uint32_t page_offset = 0; page_offset < extent->num_pages; page_offset++) {
            uint32_t page_num = extent->start_page + page_offset;

            BufferFrame* frame = nullptr;
            auto status = db_->getBufferPool()->pinPage(page_num, &frame, ctx);
            if (status != Status::OK) continue;

            auto* heap_page = reinterpret_cast<SBHeapPage*>(frame->getData());

            // Scan tuples
            for (uint16_t slot = 0; slot < heap_page->hp_tuple_count; slot++) {
                TID tid(page_num, slot);
                auto tuple = getTupleFromSlot(heap_page, slot);
                if (!tuple) continue;

                // Check TIP visibility
                bool visible = false;
                isVersionVisible(tuple->xmin, tuple->xmax, current_xid, &visible, ctx);
                if (!visible) continue;

                // Evaluate predicates
                bool matches = true;
                for (const auto& pred : plan.predicates) {
                    if (!evaluatePredicate(tuple, pred)) {
                        matches = false;
                        break;
                    }
                }

                if (matches) {
                    results_out->push_back(tid);
                }
            }

            db_->getBufferPool()->unpinPage(page_num);
        }
    }

    return Status::OK;
}
```

---

## Bytecode Integration

### SQL Syntax

```sql
-- Create standalone zone map index
CREATE INDEX idx_timestamp_zm ON events USING zonemap(timestamp);

-- Create multi-column zone map
CREATE INDEX idx_multi_zm ON events USING zonemap(timestamp, user_id);

-- Specify extent size
CREATE INDEX idx_timestamp_zm ON events USING zonemap(timestamp)
WITH (extent_size_kb = 1024);  -- 1MB extents

-- Drop zone map
DROP INDEX idx_timestamp_zm;

-- Rebuild zone map
REINDEX INDEX idx_timestamp_zm;
```

### AST Additions

**File:** `include/scratchbird/parser/ast_v2.h`

```cpp
struct ZoneMapOptions {
    uint32_t extent_size_kb;    // Extent size in KB (default: 512)
    uint32_t max_string_len;    // Max string length (default: 64)
};

struct CreateIndexStmt : public Statement {
    // ... existing fields ...
    ZoneMapOptions zonemap_options;  // NEW: For ZONEMAP indexes
};
```

### Bytecode Opcodes

No new opcodes needed - extend CREATE_INDEX (0x50):

```cpp
// Bytecode format for CREATE INDEX ... USING zonemap
// [CREATE_INDEX opcode]
// [index type = ZONEMAP]
// [table name]
// [index name]
// [column count]
// [column IDs...]
// [extent_size_kb (uint32)]  // NEW for zonemap
// [max_string_len (uint32)]  // NEW for zonemap
```

### Bytecode Generation

**File:** `src/sblr/bytecode_generator_v2.cpp`

```cpp
void BytecodeGeneratorV2::generateCreateIndex(ResolvedCreateIndexStmt* stmt) {
    // ... existing encoding ...

    // NEW: For ZONEMAP indexes, encode options
    if (stmt->index_type == IndexType::ZONEMAP) {
        encodeUint32(stmt->zonemap_options.extent_size_kb, &bytecode_);
        encodeUint32(stmt->zonemap_options.max_string_len, &bytecode_);
    }
}
```

### Executor Integration

**File:** `src/sblr/executor.cpp`

```cpp
Status Executor::executeCreateIndex(const uint8_t* bytecode, size_t* offset,
                                   ErrorContext* ctx) {
    // ... decode common fields ...

    if (index_type == IndexType::ZONEMAP) {
        // Decode zone map options
        uint32_t extent_size_kb = decodeUint32(bytecode, offset);
        uint32_t max_string_len = decodeUint32(bytecode, offset);

        // Get page size from database (page-size aware conversion)
        uint32_t page_size = db_->getPageSize();

        ZoneMapConfig config;
        config.extent_size_pages = (extent_size_kb * 1024) / page_size;
        config.max_string_len = max_string_len;

        // Validate configuration
        auto validation_status = ZoneMapIndex::validateConfig(page_size, config, ctx);
        if (validation_status != Status::OK) {
            return validation_status;
        }

        // Create zone map
        auto* catalog = db_->getCatalogManager();
        auto table = catalog->getTable(table_name, ctx);
        if (!table) {
            return Status::TABLE_NOT_FOUND;
        }

        uint32_t meta_page = 0;
        auto status = ZoneMapIndex::create(db_, table, columns, config, &meta_page, ctx);
        if (status != Status::OK) {
            return status;
        }

        // Register in catalog
        catalog->registerIndex(table_name, index_name, IndexType::ZONEMAP, meta_page, ctx);

        LOG_INFO(Category::EXECUTOR, "Created zone map '%s' on %s",
                index_name.c_str(), table_name.c_str());
    }

    return Status::OK;
}
```

---

## Implementation Steps

### Phase 1: Storage Foundation (16-24 hours)

1. **Add extent tracking to tables (8-12 hours)**
   - Modify `Table` struct to include `TableExtentInfo`
   - Implement `getExtentId()`, `allocateExtent()` helpers
   - Update table creation to initialize extent metadata
   - Add catalog storage for extent info

2. **Implement page structures (4-6 hours)**
   - Define `SBZoneMapMetaPage` with flexible array member
   - Define `SBZoneMapStatsPage` with flexible array member
   - Define `ZoneMapValue`, `ZoneColumnStats`
   - Implement `maxZonesPerPage()` helper (page-size aware)
   - NO static assertions for page size (must be flexible)

3. **Implement ZoneMapIndex class skeleton (4-6 hours)**
   - `create()` method (allocate meta page)
   - `open()` method (load meta page)
   - `loadZoneStats()` / `saveZoneStats()` helpers
   - Page-size aware helper methods: `getPageSize()`, `getExtentSizeBytes()`, etc.
   - Configuration validation: `validateConfig()` for page size checks

### Phase 2: Statistics Collection (16-24 hours)

1. **Implement value serialization (4-6 hours)**
   - `serializeValue()` for all data types
   - `compareValues()` for all data types
   - Handle strings (first N bytes)
   - Handle NULLs

2. **Implement updateZoneStats (4-6 hours)**
   - Incremental min/max updates
   - NULL counting
   - Row counting

3. **Implement rebuild (8-12 hours)**
   - `rebuild()` method (all zones)
   - `rebuildZone()` method (single zone)
   - Scan heap pages with TIP visibility
   - Extract column values
   - Calculate min/max/null_count

### Phase 3: Predicate Evaluation (12-16 hours)

1. **Implement predicate evaluation (6-8 hours)**
   - `evaluateSinglePredicate()` for all predicate types
   - `combineAND()` / `combineOR()`
   - Test all combinations

2. **Implement filterZones (4-6 hours)**
   - Iterate zones
   - Evaluate predicates
   - Collect matching zones
   - Update statistics

3. **Unit tests (2-2 hours)**
   - Test each predicate type
   - Test predicate combinations

### Phase 4: DML Integration (8-12 hours)

1. **INSERT hook (3-4 hours)**
   - Update zone stats on insert
   - Handle new zones

2. **UPDATE hook (3-4 hours)**
   - Mark zones stale
   - Trigger rebuild if needed

3. **GC integration (2-4 hours)**
   - Rebuild during sweep/GC
   - Clean up stale statistics

### Phase 5: Query Planner (20-28 hours)

1. **Cost model (8-12 hours)**
   - Estimate full scan cost
   - Estimate zone-filtered scan cost
   - Choose best plan

2. **Executor integration (8-12 hours)**
   - Implement `executeZoneFilteredScan()`
   - Integrate with existing scan logic
   - Test end-to-end

3. **Integration tests (4-4 hours)**
   - Test query plans
   - Verify skip rates
   - Benchmark performance

### Phase 6: SQL/Bytecode (8-12 hours)

1. **Parser (3-4 hours)**
   - Add ZONEMAP syntax
   - Parse extent_size_kb option

2. **Bytecode (2-3 hours)**
   - Encode/decode options

3. **Executor (3-5 hours)**
   - Create zone map from SQL

### Phase 7: Documentation (2-4 hours)

1. Update INDEX_ARCHITECTURE.md
2. Add usage examples
3. Document performance characteristics

**Total:** 82-120 hours (10-15 days full-time)

---

## Testing Requirements

### Unit Tests

**File:** `tests/unit/test_zonemap_index.cpp`

```cpp
TEST(ZoneMapTest, PredicateEvaluationEquals) {
    ZoneColumnStats stats;
    stats.min_value = makeValue(10);
    stats.max_value = makeValue(100);
    stats.null_count = 0;
    stats.row_count = 1000;

    ZoneMapIndex zm(/*...*/);

    // Value < min → EXCLUDE
    Predicate pred1{PredicateType::EQUALS, 0, makeValue(5)};
    EXPECT_EQ(zm.evaluateSinglePredicate(stats, pred1), SkipDecision::EXCLUDE);

    // Value > max → EXCLUDE
    Predicate pred2{PredicateType::EQUALS, 0, makeValue(105)};
    EXPECT_EQ(zm.evaluateSinglePredicate(stats, pred2), SkipDecision::EXCLUDE);

    // min <= value <= max → MAYBE
    Predicate pred3{PredicateType::EQUALS, 0, makeValue(50)};
    EXPECT_EQ(zm.evaluateSinglePredicate(stats, pred3), SkipDecision::MAYBE);

    // value == min == max → INCLUDE
    stats.min_value = stats.max_value = makeValue(42);
    Predicate pred4{PredicateType::EQUALS, 0, makeValue(42)};
    EXPECT_EQ(zm.evaluateSinglePredicate(stats, pred4), SkipDecision::INCLUDE);
}

TEST(ZoneMapTest, PredicateEvaluationRange) {
    ZoneColumnStats stats;
    stats.min_value = makeValue(100);
    stats.max_value = makeValue(200);

    ZoneMapIndex zm(/*...*/);

    // BETWEEN [50, 150]
    Predicate pred{PredicateType::BETWEEN, 0, makeValue(50), makeValue(150)};
    EXPECT_EQ(zm.evaluateSinglePredicate(stats, pred), SkipDecision::MAYBE);

    // BETWEEN [150, 250]
    Predicate pred2{PredicateType::BETWEEN, 0, makeValue(150), makeValue(250)};
    EXPECT_EQ(zm.evaluateSinglePredicate(stats, pred2), SkipDecision::MAYBE);

    // BETWEEN [300, 400] → EXCLUDE
    Predicate pred3{PredicateType::BETWEEN, 0, makeValue(300), makeValue(400)};
    EXPECT_EQ(zm.evaluateSinglePredicate(stats, pred3), SkipDecision::EXCLUDE);

    // BETWEEN [100, 200] → INCLUDE (exact match)
    Predicate pred4{PredicateType::BETWEEN, 0, makeValue(100), makeValue(200)};
    EXPECT_EQ(zm.evaluateSinglePredicate(stats, pred4), SkipDecision::INCLUDE);
}

TEST(ZoneMapTest, StatisticsCollection) {
    auto db = createTestDatabase();
    auto table = createTestTable(db.get());

    // Initialize extents
    table->extent_info.extent_size_pages = 64;
    table->allocateExtent(nullptr, nullptr);

    ZoneMapConfig config;
    uint32_t meta_page = 0;
    auto status = ZoneMapIndex::create(db.get(), table,
                                      {0}, config, &meta_page, nullptr);
    ASSERT_EQ(status, Status::OK);

    auto zm = ZoneMapIndex::open(db.get(), UuidV7Bytes{}, meta_page, nullptr);
    ASSERT_NE(zm, nullptr);

    // Insert values
    std::vector<Value> values = {
        makeValue(10), makeValue(50), makeValue(100)
    };

    status = zm->updateZoneStats(0, 0, values, nullptr);
    ASSERT_EQ(status, Status::OK);

    // Verify statistics
    std::vector<ZoneColumnStats> stats;
    status = zm->loadZoneStats(0, &stats, nullptr);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(stats.size(), 1);

    EXPECT_EQ(getInt32(stats[0].min_value), 10);
    EXPECT_EQ(getInt32(stats[0].max_value), 100);
    EXPECT_EQ(stats[0].row_count, 3);
}

TEST(ZoneMapTest, PageSizeAdaptation) {
    // Test zone map works correctly with different page sizes
    std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 65536, 131072};

    for (uint32_t page_size : page_sizes) {
        auto db = createTestDatabaseWithPageSize(page_size);
        auto table = createTestTable(db.get());

        ZoneMapConfig config;
        config.extent_size_pages = 64;

        // Validate configuration
        auto status = ZoneMapIndex::validateConfig(page_size, config, nullptr);
        ASSERT_EQ(status, Status::OK);

        // Create zone map
        uint32_t meta_page = 0;
        status = ZoneMapIndex::create(db.get(), table, {0}, config, &meta_page, nullptr);
        ASSERT_EQ(status, Status::OK);

        auto zm = ZoneMapIndex::open(db.get(), UuidV7Bytes{}, meta_page, nullptr);
        ASSERT_NE(zm, nullptr);

        // Verify page size awareness
        EXPECT_EQ(zm->getPageSize(), page_size);

        uint32_t expected_extent_bytes = 64 * page_size;
        EXPECT_EQ(zm->getExtentSizeBytes(), expected_extent_bytes);

        // Verify zones per page calculation
        uint32_t zones_per_page = zm->getMaxZonesPerPage();
        uint32_t expected = (page_size - 80) / (1 * 180);  // 1 column
        EXPECT_EQ(zones_per_page, expected);
    }
}

TEST(ZoneMapTest, InvalidPageSizeRejected) {
    ZoneMapConfig config;
    config.extent_size_pages = 64;

    // Too small
    auto status = ZoneMapIndex::validateConfig(4096, config, nullptr);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);

    // Too large
    status = ZoneMapIndex::validateConfig(256 * 1024, config, nullptr);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);

    // Not power of 2
    status = ZoneMapIndex::validateConfig(10000, config, nullptr);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);

    // Valid sizes
    for (uint32_t ps : {8192, 16384, 32768, 65536, 131072}) {
        status = ZoneMapIndex::validateConfig(ps, config, nullptr);
        EXPECT_EQ(status, Status::OK);
    }
}
```

### Integration Tests

**File:** `tests/integration/test_zonemap_scan.cpp`

```cpp
TEST(ZoneMapScanTest, FilteredScan) {
    auto db = createTestDatabase();

    // Create table with sorted data
    executeSQL(db.get(), R"(
        CREATE TABLE events (
            id INT,
            timestamp BIGINT
        )
    )");

    // Insert sorted data (simulating time-series)
    // Zone 0: timestamp 0-999
    // Zone 1: timestamp 1000-1999
    // Zone 2: timestamp 2000-2999
    for (int i = 0; i < 3000; i++) {
        std::string sql = "INSERT INTO events VALUES (" +
                         std::to_string(i) + ", " +
                         std::to_string(i) + ")";
        executeSQL(db.get(), sql);
    }

    // Create zone map
    executeSQL(db.get(), R"(
        CREATE INDEX idx_timestamp_zm ON events USING zonemap(timestamp)
    )");

    // Query: timestamp BETWEEN 500 AND 600
    // Should skip zones 1 and 2
    auto results = executeSQL(db.get(), R"(
        SELECT * FROM events WHERE timestamp BETWEEN 500 AND 600
    )");

    EXPECT_EQ(results.size(), 101);  // 500-600 inclusive

    // Check statistics
    auto catalog = db->getCatalogManager();
    auto zm = catalog->getZoneMapIndex("idx_timestamp_zm");
    auto stats = zm->getStatistics();

    EXPECT_GT(stats.zones_skipped, 0);  // Should have skipped zones
    EXPECT_GT(stats.skip_rate, 0.5);    // At least 50% skip rate
}

TEST(ZoneMapScanTest, RebuildAfterUpdates) {
    auto db = createTestDatabase();

    executeSQL(db.get(), R"(
        CREATE TABLE data (
            id INT,
            value INT
        )
    )");

    // Insert data
    for (int i = 0; i < 1000; i++) {
        executeSQL(db.get(), "INSERT INTO data VALUES (" +
                  std::to_string(i) + ", " + std::to_string(i) + ")");
    }

    // Create zone map
    executeSQL(db.get(), "CREATE INDEX idx_value_zm ON data USING zonemap(value)");

    // Get initial statistics
    auto zm = db->getCatalogManager()->getZoneMapIndex("idx_value_zm");
    auto stats_before = getZoneStats(zm, 0);

    // Update many values
    for (int i = 0; i < 100; i++) {
        executeSQL(db.get(), "UPDATE data SET value = 9999 WHERE id = " +
                  std::to_string(i));
    }

    // Rebuild
    executeSQL(db.get(), "REINDEX INDEX idx_value_zm");

    // Get updated statistics
    auto stats_after = getZoneStats(zm, 0);

    // Max should have increased
    EXPECT_GT(getInt32(stats_after.max_value), getInt32(stats_before.max_value));
}

TEST(ZoneMapScanTest, PageSizePerformance) {
    // Verify zone maps work correctly across different page sizes
    std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 65536};

    for (uint32_t page_size : page_sizes) {
        auto db = createTestDatabaseWithPageSize(page_size);

        executeSQL(db.get(), R"(
            CREATE TABLE events (
                id INT,
                timestamp BIGINT
            )
        )");

        // Insert 10,000 sorted rows
        for (int i = 0; i < 10000; i++) {
            executeSQL(db.get(), "INSERT INTO events VALUES (" +
                      std::to_string(i) + ", " + std::to_string(i) + ")");
        }

        // Create zone map
        executeSQL(db.get(),
                  "CREATE INDEX idx_ts_zm ON events USING zonemap(timestamp)");

        auto zm = db->getCatalogManager()->getZoneMapIndex("idx_ts_zm");

        // Verify extent size scales with page size
        uint32_t extent_bytes = zm->getExtentSizeBytes();
        EXPECT_EQ(extent_bytes, 64 * page_size);

        // Query: timestamp BETWEEN 100 AND 200
        auto results = executeSQL(db.get(),
                                 "SELECT * FROM events WHERE timestamp BETWEEN 100 AND 200");
        EXPECT_EQ(results.size(), 101);

        // Verify skip rate (should work at all page sizes)
        auto stats = zm->getStatistics();
        EXPECT_GT(stats.zones_skipped, 0);

        // Log for debugging
        LOG_INFO(Category::TEST,
                "Page size: %u KB, Extent size: %u KB, Zones skipped: %lu",
                page_size / 1024,
                extent_bytes / 1024,
                stats.zones_skipped);
    }
}
```

---

## Performance Targets

### Latency

- **Zone map metadata load:** < 1ms (read meta page, any page size)
- **Zone filtering:** < 10ms for 1000 zones (8KB pages), < 5ms for same zones (larger pages)
- **Statistics update (insert):** < 100 microseconds
- **Zone rebuild times (per zone, varies by page size):**
  - 8KB pages: 512KB zone, ~64 pages → < 500ms
  - 16KB pages: 1MB zone, ~64 pages → < 800ms
  - 32KB pages: 2MB zone, ~64 pages → < 1.5s
  - 64KB pages: 4MB zone, ~64 pages → < 3s

### Skip Rate

Performance varies by page size and data distribution:

**8KB pages (fine-grained zones):**
- **Sorted data:** 95-99.9% skip rate for selective queries
- **Semi-sorted data:** 60-90% skip rate
- **Random data:** 15-30% skip rate

**16KB-32KB pages (medium zones):**
- **Sorted data:** 90-99% skip rate
- **Semi-sorted data:** 50-80% skip rate
- **Random data:** 10-25% skip rate

**64KB-128KB pages (coarse zones):**
- **Sorted data:** 85-95% skip rate
- **Semi-sorted data:** 40-70% skip rate
- **Random data:** 5-20% skip rate (limited effectiveness)

### Space Overhead

Zone map metadata size varies by page size (fewer zones with larger pages):

**8KB pages:**
- **1TB table, 512KB zones:** ~400MB zone map (0.04%)
- **10GB table, 512KB zones:** ~4MB zone map (0.04%)

**16KB pages:**
- **1TB table, 1MB zones:** ~200MB zone map (0.02%)
- **10GB table, 1MB zones:** ~2MB zone map (0.02%)

**32KB pages:**
- **1TB table, 2MB zones:** ~100MB zone map (0.01%)
- **10GB table, 2MB zones:** ~1MB zone map (0.01%)

**64KB+ pages:**
- **1TB table, 4MB+ zones:** ~50MB or less (< 0.005%)
- **10GB table, 4MB+ zones:** ~500KB or less (< 0.005%)

**Target:** < 0.1% of table size (easily achieved at all page sizes)

### Benchmarks

```sql
-- Create 10M row table (sorted by timestamp)
CREATE TABLE events AS
SELECT
    i AS id,
    timestamp '2024-01-01' + (i || ' seconds')::interval AS ts,
    floor(random() * 1000) AS user_id
FROM generate_series(1, 10000000) i;

-- Create zone map
CREATE INDEX idx_ts_zm ON events USING zonemap(ts);

-- Benchmark: Selective query (0.1% of data)
SELECT COUNT(*) FROM events
WHERE ts BETWEEN '2024-01-01 00:00:00' AND '2024-01-01 01:00:00';

-- Expected performance (varies by page size):
-- Without zone map: Full scan ~1-2 seconds (all page sizes)

-- 8KB pages (512KB zones, 2048 zones total):
--   Skip 99.9% of zones (2046 zones), scan 2 zones
--   ~10-20ms

-- 16KB pages (1MB zones, 1024 zones total):
--   Skip 99.9% of zones (1022 zones), scan 2 zones
--   ~8-15ms (fewer zones to check)

-- 32KB pages (2MB zones, 512 zones total):
--   Skip 99.8% of zones (510 zones), scan 2 zones
--   ~5-10ms (much fewer zones to check)

-- 64KB pages (4MB zones, 256 zones total):
--   Skip 99.6% of zones (255 zones), scan 1 zone
--   ~3-8ms (very few zones to check)
```

**Key Insight:** Larger page sizes reduce zone map overhead but may increase false positives. For sorted data, all page sizes deliver excellent performance.

---

## Future Enhancements

### Phase 2 Features

1. **Multi-Column Zone Maps**
   - Composite statistics for correlated columns
   - Better pruning for multi-column predicates

2. **Adaptive Extent Sizing**
   - Adjust extent size based on data distribution
   - Smaller extents for hot data, larger for cold

3. **Distinct Count Estimation**
   - HyperLogLog integration
   - Cardinality estimates for query planning

4. **Zone Map Compression**
   - Delta encoding for numeric min/max
   - Dictionary compression for strings

5. **Hierarchical Zone Maps**
   - Multi-level statistics (partition → extent → page)
   - Faster pruning for very large tables

6. **Expression Zone Maps**
   - Store min/max of expressions (e.g., YEAR(timestamp))
   - Enable pruning on computed columns

---

## Conclusion

This specification provides a complete, implementation-ready design for Zone Maps in ScratchBird.

**Key Features:**
- ✅ Extent-based storage model (64 pages per extent, scales with page size)
- ✅ **Page-size agnostic** (supports 8KB, 16KB, 32KB, 64KB, 128KB pages)
- ✅ MGA-compliant (optimistic statistics, periodic rebuild)
- ✅ Full query planner integration (cost-based zone filtering)
- ✅ Multi-column support
- ✅ Predicate pushdown evaluation
- ✅ SQL syntax: CREATE INDEX USING zonemap(column)
- ✅ Automatic maintenance during INSERT/UPDATE
- ✅ GC integration for statistics cleanup
- ✅ Runtime capacity calculations (no hardcoded page sizes)
- ✅ Flexible array members for page structures

**Implementation Effort:** 82-120 hours (10-15 days)

**Risk Level:** MEDIUM - Requires query planner changes, extent tracking

**Performance:** 10-100x speedup for selective range queries on sorted data (varies by page size)

**Page Size Flexibility:**
- Fine-grained zones (8KB pages): Best skip rates, higher metadata
- Coarse zones (64KB+ pages): Lower metadata, fewer zones to check
- All page sizes: Automatic adaptation, no code changes needed

**Next Steps:**
1. Review this specification
2. Implement Phase 1 (storage foundation with page-size awareness)
3. Implement Phase 2 (statistics collection)
4. Integrate with query planner
5. Benchmark on TPC-H workloads across different page sizes

---

**Specification Status:** READY FOR IMPLEMENTATION (v1.1)
**Reviewer:** Please provide feedback on:
- Extent size choice (64 pages, scales with page size)
- MGA compliance approach (optimistic statistics)
- Query planner integration strategy
- Page-size agnostic design (flexible arrays, runtime calculations)
- Missing considerations or edge cases

**Author:** ScratchBird Architecture Team
**Date:** November 20, 2025
**Version:** 1.1 (Page-size agnostic update)
