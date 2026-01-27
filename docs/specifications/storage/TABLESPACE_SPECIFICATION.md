# Tablespace Specification for ScratchBird

**Document Status**: DRAFT
**Version**: 1.0
**Date**: 2025-10-20
**Author**: Research and Analysis

---

## Executive Summary

This document specifies the tablespace feature for ScratchBird, enabling multi-file database support with sophisticated data placement control. Based on comprehensive research of PostgreSQL, Oracle, MySQL/InnoDB, and Firebird implementations, this specification adapts proven patterns to ScratchBird's Firebird-based MGA architecture.

**Key Benefits**:
- **Capacity Expansion**: Overcome single-file size limits by distributing data across multiple files
- **Performance Optimization**: Place hot/cold data on appropriate storage tiers (SSD/HDD)
- **Data Lifecycle Management**: Archive/retire old data by detaching tablespace files
- **Flexible Data Placement**: Partition tables across tablespaces based on criteria (date, geography, etc.)
- **Operational Flexibility**: Attach tablespaces from other databases (with validation)

**Implementation Effort**: 120-180 hours (3-4.5 weeks)

---

## Table of Contents

1. [Research Summary](#1-research-summary)
2. [Tablespace Concepts](#2-tablespace-concepts)
3. [Architecture Design](#3-architecture-design)
4. [Data Structures](#4-data-structures)
5. [API Specification](#5-api-specification)
6. [Migration and Online Operations](#6-migration-and-online-operations)
7. [Partitioning and Data Placement](#7-partitioning-and-data-placement)
8. [Attach/Detach Operations](#8-attachdetach-operations)
9. [Integration Points](#9-integration-points)
10. [Implementation Plan](#10-implementation-plan)
11. [Testing Strategy](#11-testing-strategy)
12. [Appendix: Research Findings](#12-appendix-research-findings)

---

## 1. Research Summary

### 1.1 PostgreSQL Tablespaces

**Key Findings**:
- **Directory-based**: Tablespace is a directory on the filesystem
- **Syntax**: `CREATE TABLESPACE name LOCATION '/path/to/directory'`
- **Migration**: `ALTER TABLE SET TABLESPACE` (NOT online, requires exclusive lock)
- **Online Tool**: `pg_repack` extension for online table migration
- **Partitioning**: Each partition can specify its own tablespace via `TABLESPACE` clause
- **Date-based Distribution**:
  ```sql
  CREATE TABLE measurement_y2025m01
    PARTITION OF measurement
    FOR VALUES FROM ('2025-01-01') TO ('2025-02-01')
    TABLESPACE fasttablespace;
  ```

**Limitations**:
- No built-in online migration (requires external tools)
- Tablespace path hard-coded (moving requires symlinks or re-creation)

### 1.2 Oracle Tablespaces

**Key Findings**:
- **File-based**: Tablespace contains one or more datafiles
- **Smallfile vs Bigfile**:
  - Smallfile: Up to 1022 datafiles per tablespace
  - Bigfile: Single datafile (up to 4B blocks = 128TB with 32K blocks)
- **Autoextend**:
  ```sql
  CREATE TABLESPACE ts1
  DATAFILE 'file1.dbf' SIZE 100M
  AUTOEXTEND ON NEXT 10M MAXSIZE 2G;
  ```
- **MAXSIZE Limits**: `MAXSIZE UNLIMITED` limited by block size (32GB for 8K blocks)
- **Online Migration**:
  - Oracle 12.2+: `ALTER TABLE MOVE ONLINE` supported
  - Earlier: `DBMS_REDEFINITION` for online table reorganization
  ```sql
  ALTER TABLE APP_TABLE MOVE TABLESPACE NEW_TBS ONLINE PARALLEL 16;
  ```

**Strengths**:
- Sophisticated autoextend with granular control
- Built-in online migration (Oracle 12.2+)
- Strong support for storage tiering

### 1.3 MySQL/InnoDB Tablespaces

**Key Findings**:
- **Three Types**:
  1. **System Tablespace** (`ibdata1`): Shared, all tables (legacy mode)
  2. **File-per-Table** (`innodb_file_per_table=ON`): Each table in its own `.ibd` file (default)
  3. **General Tablespace**: Shared, multiple tables, created via `CREATE TABLESPACE`
- **CREATE TABLESPACE**:
  ```sql
  CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
  ALTER TABLE t1 TABLESPACE=ts1;
  ```
- **AUTOEXTEND_SIZE** (MySQL 8.0.23+):
  - Maximum: 4GB
  - Default growth: 1 extent (small files), 1 extent (< 32 extents), 4 extents (>= 32 extents)
- **System Tablespace Autoextend**:
  ```sql
  innodb_data_file_path = ibdata1:12M:autoextend:max:5G
  ```
- **Transportable Tablespaces**:
  ```sql
  -- Source server
  FLUSH TABLES tbl1 FOR EXPORT;
  -- Copy .ibd and .cfg files
  UNLOCK TABLES;

  -- Destination server
  CREATE TABLE tbl1 (...);  -- Same definition
  ALTER TABLE tbl1 DISCARD TABLESPACE;
  -- Copy files to destination
  ALTER TABLE tbl1 IMPORT TABLESPACE;
  ```

**Limitations**:
- No MAXSIZE for general/file-per-table tablespaces (only system tablespace)
- `ALTER TABLE ... TABLESPACE` always causes full table rebuild
- Partitioned table tablespace placement deprecated (MySQL 5.7.24) and removed (MySQL 8.0.13)

### 1.4 Firebird Multi-File Databases

**Key Findings**:
- **Secondary Files**: Add files via `ALTER DATABASE ADD FILE`
  ```sql
  ALTER DATABASE ADD FILE 'D:\test4.fdb' STARTING AT PAGE 30001;
  ```
- **No "Tablespaces"**: Secondary files are sequential extensions, not independent tablespaces
- **Growth Strategy**:
  - Specify `STARTING AT PAGE` or `LENGTH` (in pages)
  - Last file grows dynamically until filesystem limit
  - Up to 65,000 secondary files (Firebird 2.1+)
- **File Size Limits**:
  - Firebird 2.x: 32TB per database
  - Firebird 3.x: 64TB per database
  - Page size: 1K, 2K, 4K, 8K, 16K (Firebird 3.x), 32K (Firebird 4.0+)
- **Hard Links**: Multi-file databases have hard links to secondary files (difficult to relocate)
- **No Parallel Access**: Server treats multi-file database as one sequential file
- **Backup**: `nbackup` supports incremental backups with file-level operations

**Modern Recommendation**: Multi-file databases considered anachronism; use single large file on modern filesystems

---

## 2. Tablespace Concepts

### 2.1 Tablespace Definition

A **tablespace** is a logical storage container that maps to one or more physical files on the filesystem. It provides:

1. **Logical Isolation**: Group related data together
2. **Physical Control**: Specify exact storage location
3. **Storage Tiering**: Place data on appropriate media (SSD, HDD, network storage)
4. **Capacity Management**: Distribute database across multiple files/devices
5. **Operational Flexibility**: Attach/detach files for archival or migration

### 2.2 ScratchBird Tablespace Model

**Hybrid Approach**: Combines best aspects of researched systems:
- **Firebird-style simplicity**: Page-based addressing, stable TIDs
- **Oracle-style control**: Autoextend, MAXSIZE, granular growth parameters
- **PostgreSQL-style flexibility**: Directory-based placement, partition-aware
- **MySQL-style portability**: Transportable tablespace support

**Key Differences from Firebird**:
- Tablespaces are **independent storage units**, not sequential file extensions
- Tables/indexes explicitly assigned to tablespaces
- Support for moving objects between tablespaces online
- First-class support for partitioned table distribution

---

## 3. Architecture Design

### 3.1 Tablespace Storage Model

```
ScratchBird Database
├─ Primary File (database.sbdb)
│  ├─ Page 0: DatabaseHeader (with tablespace catalog reference)
│  ├─ Page 1: System Catalog Root
│  ├─ Page 2: Free Space Map
│  ├─ Page 3-N: Catalog tables, system data
│  └─ Pages N+1...: Default tablespace data
│
├─ Tablespace 2 (ts_hot.sbts)
│  ├─ Page 0: TablespaceHeader
│  ├─ Page 1: Tablespace Free Space Map
│  └─ Pages 2+: User data
│
├─ Tablespace 3 (ts_archive_2024.sbts)
│  ├─ Page 0: TablespaceHeader
│  ├─ Page 1: Tablespace Free Space Map
│  └─ Pages 2+: User data (e.g., 2024 archived data)
│
└─ Tablespace 4 (ts_cold.sbts)
   ├─ Page 0: TablespaceHeader
   ├─ Page 1: Tablespace Free Space Map
   └─ Pages 2+: User data
```

### 3.2 Page Addressing with Tablespaces

**Global Page ID (GPID)**: 64-bit identifier
```
+----------------------+-------------------------+
| Tablespace ID (16)   | Page Number (48)        |
+----------------------+-------------------------+
```

- **Tablespace ID** (16 bits): 0 = primary file, 1 = reserved, 2-65535 = tablespace files
- **Page Number** (48 bits): Page within tablespace (up to 281TB with 16K pages)

**Stable TID Preservation** (Firebird MGA):
- TIDs remain `(GPID, slot)` format
- Heap tuples never move between tablespaces (MGA invariant)
- Indexes store TIDs pointing to heap, independent of tablespace

### 3.3 Component Modifications

#### 3.3.1 PageManager Extension

**New Responsibilities**:
- Manage multiple tablespace files
- Track per-tablespace free space
- Allocate pages from specific tablespaces
- Extend tablespace files based on autoextend parameters

**New API**:
```cpp
class PageManager {
public:
    // Existing API (implicitly uses default tablespace)
    Status allocatePage(uint32_t &page_id, ErrorContext *ctx = nullptr);

    // NEW: Tablespace-aware allocation
    Status allocatePageInTablespace(uint16_t tablespace_id,
                                    uint64_t &gpid_out,
                                    ErrorContext *ctx = nullptr);

    // NEW: Tablespace management
    Status createTablespace(uint16_t tablespace_id,
                           const TablespaceInfo &info,
                           ErrorContext *ctx = nullptr);
    Status dropTablespace(uint16_t tablespace_id,
                         bool force,
                         ErrorContext *ctx = nullptr);
    Status extendTablespace(uint16_t tablespace_id,
                           ErrorContext *ctx = nullptr);

    // NEW: Statistics
    Status getTablespaceStats(uint16_t tablespace_id,
                             TablespaceStats *stats_out,
                             ErrorContext *ctx = nullptr);
};
```

#### 3.3.2 BufferPool Extension

**New Responsibilities**:
- Pin pages from any tablespace via GPID
- Cache pages from multiple files
- Track file descriptors for all tablespaces

**Modified API**:
```cpp
class BufferPool {
public:
    // Existing API with uint32_t page_id (primary file only)
    Status pinPage(uint32_t page_id, void **buffer_out, ErrorContext *ctx = nullptr);

    // NEW: Tablespace-aware pinning
    Status pinPageGlobal(uint64_t gpid, void **buffer_out, ErrorContext *ctx = nullptr);

    // NEW: Flush specific tablespace
    Status flushTablespace(uint16_t tablespace_id, ErrorContext *ctx = nullptr);
};
```

#### 3.3.3 CatalogManager Extension

**New System Tables**:
1. **pg_tablespace**: Tablespace definitions
2. **pg_tablespace_files**: Datafiles within tablespaces

**Modified Structures**:
```cpp
struct SchemaInfo {
    uint16_t default_tablespace_id = 0;  // EXISTING (line 53)
    // ... other fields
};

struct TableInfo {
    uint16_t tablespace_id = 0;  // EXISTING (line 86)
    // ... other fields
};

// NEW: IndexInfo extension needed
struct IndexInfo {
    uint16_t tablespace_id = 0;  // NEW: Index tablespace
    // ... other fields
};
```

**New API**:
```cpp
class CatalogManager {
public:
    // NEW: Tablespace operations
    Status createTablespace(const TablespaceInfo &ts_info,
                           ErrorContext *ctx = nullptr);
    Status getTablespace(uint16_t tablespace_id,
                        TablespaceInfo &info,
                        ErrorContext *ctx = nullptr);
    Status listTablespaces(std::vector<TablespaceInfo> &tablespaces,
                          ErrorContext *ctx = nullptr);
    Status dropTablespace(uint16_t tablespace_id,
                         ErrorContext *ctx = nullptr);

    // NEW: Object-to-tablespace mapping
    Status moveTableToTablespace(const ID &table_id,
                                uint16_t target_tablespace_id,
                                bool online,
                                ErrorContext *ctx = nullptr);
};
```

#### 3.3.4 Database Extension

**New Responsibilities**:
- Open/close multiple tablespace files
- Maintain tablespace file descriptor map

**Modified Structure**:
```cpp
class Database {
private:
    int fd_ = -1;  // EXISTING: Primary file descriptor

    // NEW: Tablespace file management
    std::unordered_map<uint16_t, int> tablespace_fds_;  // tablespace_id → fd
    std::mutex tablespace_mutex_;

public:
    // NEW: Tablespace file operations
    Status openTablespace(uint16_t tablespace_id,
                         const std::string &path,
                         ErrorContext *ctx = nullptr);
    Status closeTablespace(uint16_t tablespace_id,
                          ErrorContext *ctx = nullptr);

    // NEW: Read/write with GPID
    Status read_page_global(uint64_t gpid, void *buffer,
                           ErrorContext *ctx = nullptr) const;
    Status write_page_global(uint64_t gpid, const void *buffer,
                            ErrorContext *ctx = nullptr);
};
```

---

## 4. Data Structures

### 4.1 TablespaceHeader (Page 0 of Tablespace File)

```cpp
#pragma pack(push, 1)
struct TablespaceHeader {
    PageHeader page_header;             // Standard 64-byte header

    // Identification (64 bytes)
    char tablespace_name[64];           // Name (max 63 chars + null)
    UuidV7Bytes tablespace_uuid;        // UUID v7 (16 bytes)
    UuidV7Bytes database_uuid;          // Database UUID (16 bytes)

    // Configuration (64 bytes)
    uint32_t tablespace_id;             // Tablespace ID (matches catalog)
    uint32_t page_size;                 // Must match database page_size
    uint64_t creation_time;             // Unix timestamp (microseconds)
    uint64_t last_checkpoint;           // Last checkpoint timestamp
    uint32_t autoextend_enabled;        // 1 = enabled, 0 = disabled
    uint32_t autoextend_size_mb;        // Extend by N MB each time
    uint64_t max_size_mb;               // Maximum size (0 = unlimited)
    uint64_t reserved1[3];

    // File layout (32 bytes)
    uint64_t total_pages;               // Total pages in this file
    uint64_t free_pages;                // Number of free pages
    uint64_t next_page_number;          // Next page to allocate
    uint64_t fsm_root_page;             // FSM for this tablespace (page 1)

    // Transaction info (32 bytes)
    uint64_t oldest_transaction_id;     // OIT for MVCC (synced with primary)
    uint64_t latest_completed_xid;      // Latest transaction
    uint64_t reserved2[2];

    // Padding to page boundary
};
#pragma pack(pop)
```

### 4.2 pg_tablespace Catalog Table

```cpp
#pragma pack(push, 1)
struct SBTablespaceCatalog {
    // Header
    uint8_t is_valid;                   // 1 = valid, 0 = deleted
    uint8_t reserved1[7];

    // Identification
    uint16_t tablespace_id;             // Tablespace ID (0 = primary, 1 = reserved, 2-65535 custom)
    char tablespace_name[64];           // Name (null-terminated)
    UuidV7Bytes tablespace_uuid;        // UUID v7

    // Configuration
    uint32_t autoextend_enabled;        // 1 = yes, 0 = no
    uint32_t autoextend_size_mb;        // Extend size (MB)
    uint64_t max_size_mb;               // Max size (MB, 0 = unlimited)
    uint32_t prealloc_pages;            // Pages to preallocate on creation
    uint32_t flags;                     // Bitmask of flags

    // File information
    char primary_path[256];             // Path to primary file
    uint32_t file_count;                // Number of files (usually 1)
    uint32_t reserved2;

    // Statistics
    uint64_t total_size_mb;             // Current total size (MB)
    uint64_t used_size_mb;              // Used size (MB)
    uint64_t free_size_mb;              // Free size (MB)
    uint64_t table_count;               // Number of tables in this tablespace
    uint64_t index_count;               // Number of indexes in this tablespace

    // Timestamps
    uint64_t created_time;              // Creation timestamp
    uint64_t last_modified_time;        // Last modification
    uint64_t last_extended_time;        // Last autoextend time

    uint8_t reserved3[64];
};
#pragma pack(pop)
```

### 4.3 pg_tablespace_files Catalog Table

```cpp
#pragma pack(push, 1)
struct SBTablespaceFileCatalog {
    // Header
    uint8_t is_valid;                   // 1 = valid, 0 = deleted
    uint8_t reserved1[7];

    // Identification
    uint16_t tablespace_id;             // Parent tablespace
    uint16_t file_index;                // File index (0, 1, 2, ...)
    UuidV7Bytes file_uuid;              // UUID v7 for this file

    // File information
    char file_path[256];                // Absolute path to file
    uint64_t starting_page;             // Starting page in tablespace (usually 0 for single file)
    uint64_t page_count;                // Number of pages in this file
    uint64_t max_pages;                 // Maximum pages (0 = unlimited)

    // Status
    uint8_t is_online;                  // 1 = online, 0 = offline
    uint8_t reserved2[7];

    // Timestamps
    uint64_t created_time;
    uint64_t last_modified_time;

    uint8_t reserved3[64];
};
#pragma pack(pop)
```

### 4.4 TablespaceInfo (In-Memory)

```cpp
struct TablespaceInfo {
    uint16_t tablespace_id = 0;
    std::string tablespace_name;
    UuidV7Bytes tablespace_uuid;

    // Configuration
    bool autoextend_enabled = true;
    uint32_t autoextend_size_mb = 100;  // Default: 100 MB increments
    uint64_t max_size_mb = 0;           // 0 = unlimited
    uint32_t prealloc_pages = 0;        // 0 = no preallocation

    // Files
    std::vector<std::string> file_paths;

    // Statistics
    uint64_t total_size_mb = 0;
    uint64_t used_size_mb = 0;
    uint64_t free_size_mb = 0;
    uint64_t table_count = 0;
    uint64_t index_count = 0;

    // Timestamps
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

---

## 5. API Specification

### 5.1 SQL DDL

#### 5.1.1 CREATE TABLESPACE

```sql
CREATE TABLESPACE tablespace_name
  LOCATION '/path/to/tablespace/file.sbts'
  [ AUTOEXTEND { ON | OFF } ]
  [ AUTOEXTEND_SIZE size_in_mb ]
  [ MAXSIZE { size_in_mb | UNLIMITED } ]
  [ PREALLOC size_in_mb ];

-- Examples
CREATE TABLESPACE ts_hot
  LOCATION '/mnt/ssd/scratchbird/ts_hot.sbts'
  AUTOEXTEND ON
  AUTOEXTEND_SIZE 100
  MAXSIZE 50000;  -- 50 GB

CREATE TABLESPACE ts_archive_2024
  LOCATION '/mnt/hdd/archive/ts_archive_2024.sbts'
  AUTOEXTEND ON
  AUTOEXTEND_SIZE 500
  MAXSIZE UNLIMITED;
```

#### 5.1.2 ALTER TABLESPACE

```sql
ALTER TABLESPACE tablespace_name
  { AUTOEXTEND { ON | OFF } }
  | { AUTOEXTEND_SIZE size_in_mb }
  | { MAXSIZE { size_in_mb | UNLIMITED } }
  | { RENAME TO new_tablespace_name };

-- Examples
ALTER TABLESPACE ts_hot AUTOEXTEND_SIZE 200;
ALTER TABLESPACE ts_cold MAXSIZE 100000;  -- 100 GB
ALTER TABLESPACE ts_archive_2024 RENAME TO ts_archive_2024_readonly;
```

#### 5.1.3 DROP TABLESPACE

```sql
DROP TABLESPACE tablespace_name [ FORCE ];

-- Examples
DROP TABLESPACE ts_temp;          -- Fails if not empty
DROP TABLESPACE ts_archive_2024 FORCE;  -- Removes even if has objects (unsafe!)
```

#### 5.1.4 CREATE TABLE with Tablespace

```sql
CREATE TABLE table_name (
    col1 INT,
    col2 VARCHAR(100),
    ...
) TABLESPACE tablespace_name;

-- Example
CREATE TABLE orders_2025 (
    order_id BIGSERIAL PRIMARY KEY,
    customer_id INT,
    order_date TIMESTAMP WITH TIME ZONE,
    total DECIMAL(10,2)
) TABLESPACE ts_hot;
```

#### 5.1.5 CREATE INDEX with Tablespace

```sql
CREATE INDEX index_name ON table_name (column_list)
  TABLESPACE tablespace_name;

-- Example
CREATE INDEX idx_orders_2025_date ON orders_2025 (order_date)
  TABLESPACE ts_hot;
```

#### 5.1.6 ALTER TABLE MOVE

```sql
ALTER TABLE table_name
  SET TABLESPACE tablespace_name
  [ ONLINE ];

-- Examples
ALTER TABLE orders_2024 SET TABLESPACE ts_archive_2024;  -- Offline (locks table)
ALTER TABLE orders_2025 SET TABLESPACE ts_cold ONLINE;   -- Online (concurrent reads)
```

### 5.2 C++ API

#### 5.2.1 PageManager Extensions

```cpp
class PageManager {
public:
    // Create a new tablespace
    Status createTablespace(uint16_t tablespace_id,
                           const std::string &file_path,
                           const TablespaceConfig &config,
                           ErrorContext *ctx = nullptr);

    // Open existing tablespace
    Status openTablespace(uint16_t tablespace_id,
                         const std::string &file_path,
                         ErrorContext *ctx = nullptr);

    // Close tablespace
    Status closeTablespace(uint16_t tablespace_id,
                          ErrorContext *ctx = nullptr);

    // Allocate page in specific tablespace
    Status allocatePageInTablespace(uint16_t tablespace_id,
                                   uint64_t *gpid_out,
                                   ErrorContext *ctx = nullptr);

    // Free page in any tablespace
    Status freePageGlobal(uint64_t gpid,
                         ErrorContext *ctx = nullptr);

    // Check if page is allocated
    bool isAllocatedGlobal(uint64_t gpid) const;

    // Extend tablespace (auto or manual)
    Status extendTablespace(uint16_t tablespace_id,
                           uint32_t num_pages,
                           ErrorContext *ctx = nullptr);

    // Get tablespace statistics
    Status getTablespaceStats(uint16_t tablespace_id,
                             TablespaceStats *stats_out,
                             ErrorContext *ctx = nullptr);

    // Flush tablespace
    Status flushTablespace(uint16_t tablespace_id,
                          ErrorContext *ctx = nullptr);
};

struct TablespaceConfig {
    bool autoextend_enabled = true;
    uint32_t autoextend_size_mb = 100;
    uint64_t max_size_mb = 0;  // 0 = unlimited
    uint32_t prealloc_pages = 0;
};

struct TablespaceStats {
    uint64_t total_pages;
    uint64_t free_pages;
    uint64_t allocated_pages;
    uint64_t total_size_mb;
    uint64_t used_size_mb;
    uint64_t free_size_mb;
};
```

#### 5.2.2 CatalogManager Extensions

```cpp
class CatalogManager {
public:
    // Tablespace operations
    Status createTablespace(const TablespaceInfo &ts_info,
                           ErrorContext *ctx = nullptr);

    Status getTablespace(uint16_t tablespace_id,
                        TablespaceInfo &info,
                        ErrorContext *ctx = nullptr);

    Status getTablespaceByName(const std::string &name,
                              TablespaceInfo &info,
                              ErrorContext *ctx = nullptr);

    Status listTablespaces(std::vector<TablespaceInfo> &tablespaces,
                          ErrorContext *ctx = nullptr);

    Status updateTablespace(uint16_t tablespace_id,
                           const TablespaceInfo &updated_info,
                           ErrorContext *ctx = nullptr);

    Status dropTablespace(uint16_t tablespace_id,
                         bool force,
                         ErrorContext *ctx = nullptr);

    // Move table to different tablespace
    Status moveTableToTablespace(const ID &table_id,
                                uint16_t target_tablespace_id,
                                bool online,
                                ErrorContext *ctx = nullptr);

    // Move index to different tablespace
    Status moveIndexToTablespace(const ID &index_id,
                                uint16_t target_tablespace_id,
                                ErrorContext *ctx = nullptr);
};
```

---

## 6. Migration and Online Operations

### 6.1 Offline Migration (ALTER TABLE SET TABLESPACE)

**Algorithm**:
1. Acquire **EXCLUSIVE LOCK** on table (blocks all readers/writers)
2. Allocate new pages in target tablespace
3. Copy all tuples from source to target tablespace
   - Preserve TIDs: `(new_gpid, same_slot)`
   - Update all indexes with new TIDs
4. Update catalog: `table.tablespace_id = target_tablespace_id`
5. Free old pages in source tablespace
6. Release lock

**Pros**:
- Simple implementation
- Guaranteed consistency

**Cons**:
- Table unavailable during migration
- Can take hours for large tables

### 6.2 Online Migration (ALTER TABLE SET TABLESPACE ONLINE)

**Algorithm** (inspired by `pg_repack` and Oracle `MOVE ONLINE`):
1. Acquire **SHARE LOCK** on table (allows reads, blocks writes temporarily)
2. Create shadow table in target tablespace
3. Copy all existing tuples to shadow table (batched, with progress tracking)
4. Create triggers to capture concurrent INSERT/UPDATE/DELETE into delta log
5. Release SHARE LOCK (table now fully available for reads/writes)
6. **Catch-up Pass**: Apply delta log changes to shadow table (repeat until delta is small)
7. Acquire **EXCLUSIVE LOCK** briefly (milliseconds)
8. Apply final delta log changes
9. Swap table metadata (catalog update)
10. Drop shadow table and delta log
11. Release EXCLUSIVE LOCK

**Pros**:
- Minimal downtime (only brief exclusive lock)
- Safe for production

**Cons**:
- Complex implementation
- Requires additional space (shadow table)
- Triggers add overhead during migration

**Detailed MGA design:** See `docs/specifications/storage/TABLESPACE_ONLINE_MIGRATION.md`.

**Implementation Priority**: **Alpha** (offline migration included; online migration post-alpha)

### 6.3 Online Migration Catalog Placeholders (Planned)

Add catalog structures to track online migration state and progress.

**Planned system tables:**
- `pg_tablespace_migrations` (authoritative state)
- `pg_tablespace_migration_deltas` (internal delta log metadata)

**Proposed fields (`pg_tablespace_migrations`):**
- `table_uuid` (UUID)
- `source_tablespace_id` (uint16)
- `target_tablespace_id` (uint16)
- `shadow_table_uuid` (UUID)
- `delta_log_uuid` (UUID)
- `state` (PREPARE|COPY|CATCH_UP|CUTOVER|CLEANUP|DONE|FAILED|CANCELED)
- `migration_start_xid` (uint64)
- `cutover_xid` (uint64)
- `rows_copied` (uint64)
- `rows_total_est` (uint64)
- `rows_per_sec` (double)
- `eta_seconds` (uint64)
- `last_lag_ms` (uint64)
- `last_lag_sample_at` (timestamp)
- `throttle_state` (text)
- `throttle_sleep_ms` (uint32)
- `last_progress_at` (timestamp)
- `last_error_code` (int32)
- `created_at` (timestamp)
- `updated_at` (timestamp)

These tables back the monitoring views described in the online migration spec.

---

## 7. Partitioning and Data Placement

### 7.1 Partition-Level Tablespace Assignment

**Scenario**: Orders table with monthly partitions, recent months on SSD, old months on HDD.

```sql
-- Create tablespaces
CREATE TABLESPACE ts_ssd
  LOCATION '/mnt/ssd/ts_ssd.sbts'
  MAXSIZE 100000;  -- 100 GB

CREATE TABLESPACE ts_hdd
  LOCATION '/mnt/hdd/ts_hdd.sbts'
  MAXSIZE UNLIMITED;

-- Create partitioned table
CREATE TABLE orders (
    order_id BIGSERIAL,
    order_date DATE NOT NULL,
    customer_id INT,
    total DECIMAL(10,2)
) PARTITION BY RANGE (order_date);

-- Recent partitions on SSD
CREATE TABLE orders_202501 PARTITION OF orders
  FOR VALUES FROM ('2025-01-01') TO ('2025-02-01')
  TABLESPACE ts_ssd;

CREATE TABLE orders_202502 PARTITION OF orders
  FOR VALUES FROM ('2025-02-01') TO ('2025-03-01')
  TABLESPACE ts_ssd;

-- Archive partitions on HDD
CREATE TABLE orders_202401 PARTITION OF orders
  FOR VALUES FROM ('2024-01-01') TO ('2024-02-01')
  TABLESPACE ts_hdd;

CREATE TABLE orders_202402 PARTITION OF orders
  FOR VALUES FROM ('2024-02-01') TO ('2024-03-01')
  TABLESPACE ts_hdd;
```

### 7.2 Data Retirement via Detach

**Scenario**: Retire 2023 data by detaching partition and archiving tablespace file.

```sql
-- Step 1: Detach partition (instant, metadata-only)
ALTER TABLE orders DETACH PARTITION orders_202301;

-- Step 2: Drop table (now safe, doesn't affect main table)
DROP TABLE orders_202301;

-- Step 3: Detach tablespace from database
-- (Custom ScratchBird command, not SQL standard)
ALTER TABLESPACE ts_archive_2023 DETACH;

-- Filesystem: Move ts_archive_2023.sbts to archival storage
-- $ mv /mnt/hdd/ts_archive_2023.sbts /backup/archive/
```

### 7.3 Attach Tablespace from Archive

**Scenario**: Re-attach 2023 data for historical analysis.

```sql
-- Step 1: Attach tablespace file
ALTER TABLESPACE ts_archive_2023 ATTACH
  LOCATION '/backup/archive/ts_archive_2023.sbts'
  VALIDATE;  -- Check database_uuid, page_size match

-- Step 2: Recreate table referencing tablespace
CREATE TABLE orders_202301 (
    order_id BIGSERIAL,
    order_date DATE NOT NULL,
    customer_id INT,
    total DECIMAL(10,2)
) TABLESPACE ts_archive_2023;

-- Step 3: Attach partition back to main table
ALTER TABLE orders ATTACH PARTITION orders_202301
  FOR VALUES FROM ('2023-01-01') TO ('2023-02-01');
```

---

## 8. Attach/Detach Operations

### 8.1 DETACH TABLESPACE

**Purpose**: Disconnect tablespace from database without deleting data.

**SQL Syntax**:
```sql
ALTER TABLESPACE tablespace_name DETACH;
```

**Preconditions**:
- No active connections using tablespace
- All tables/indexes in tablespace must be dropped or moved
- Tablespace must be empty

**Algorithm**:
1. Check tablespace has no objects (tables, indexes)
2. Flush all dirty pages in tablespace
3. Sync tablespace file to disk
4. Close file descriptor
5. Mark tablespace as `DETACHED` in pg_tablespace (or remove entry)
6. Update database header (if needed)

**Result**: Tablespace file remains on disk but database no longer references it.

### 8.2 ATTACH TABLESPACE

**Purpose**: Connect existing tablespace file to database.

**SQL Syntax**:
```sql
ALTER TABLESPACE tablespace_name ATTACH
  LOCATION '/path/to/existing.sbts'
  [ VALIDATE ];
```

**Preconditions**:
- Tablespace file exists at specified path
- Tablespace UUID not already in use
- (If VALIDATE) Page size and database UUID match

**Algorithm**:
1. Open tablespace file read-only
2. Read TablespaceHeader (page 0)
3. **Validation**:
   - Check `tablespace_header.page_size == database.page_size`
   - Check `tablespace_header.database_uuid` (warn if mismatch, allow override)
4. Allocate new tablespace_id (or use tablespace_header.tablespace_id)
5. Insert row into pg_tablespace
6. Open file read-write
7. Update database header (if needed)

**Result**: Tablespace available for querying existing data or creating new objects.

### 8.3 Cross-Database Attach

**Scenario**: Attach tablespace from another ScratchBird database.

**Requirements**:
1. **Page size must match**: Source and target databases must have same page size (e.g., 16K)
2. **UUID mismatch warning**: TablespaceHeader contains original database UUID; warn user but allow attachment with `FORCE`
3. **Catalog sync**: User must manually create table definitions in target database matching original schema

**Example**:
```sql
-- On source database (db_source.sbdb):
-- Detach tablespace with invoices_2024 table
ALTER TABLESPACE ts_invoices_2024 DETACH;

-- Filesystem: Copy file to target database
-- $ cp /data/db_source/ts_invoices_2024.sbts /data/db_target/

-- On target database (db_target.sbdb):
-- Attach with FORCE (ignores UUID mismatch)
ALTER TABLESPACE ts_invoices_2024 ATTACH
  LOCATION '/data/db_target/ts_invoices_2024.sbts'
  VALIDATE FORCE;

-- Recreate table schema (must match original exactly!)
CREATE TABLE invoices_2024 (
    invoice_id BIGSERIAL PRIMARY KEY,
    invoice_date DATE,
    amount DECIMAL(10,2)
) TABLESPACE ts_invoices_2024;

-- Data now accessible in target database
SELECT COUNT(*) FROM invoices_2024;
```

**Risks**:
- Schema mismatch causes corruption or crashes
- Transaction ID conflicts (OIT/OST from source may conflict with target)

**Recommendation**: Only attach tablespaces from same database instance or carefully validated compatible databases.

---

## 9. Integration Points

### 9.1 Transaction Manager

**Impact**: Transaction IDs (xmin/xmax) span multiple tablespaces.

**Changes**:
- OIT/OST tracked per-database, not per-tablespace
- CLOG remains in primary file (global commit status)
- Tablespace headers store `oldest_transaction_id` for recovery

**Sweep Coordination**:
- Sweep must visit all tablespaces to advance OIT
- GarbageCollector iterates all tablespaces calling `removeDeadEntries()`

### 9.2 Buffer Pool

**Impact**: Buffer pool must handle pages from multiple files.

**Changes**:
- Buffer tags now use GPID (64-bit) instead of page_id (32-bit)
- Hash table keyed on `(tablespace_id, page_number)` tuple
- Eviction policy unchanged (LRU per buffer)

**Performance Consideration**:
- Bufferpool size applies across ALL tablespaces (shared pool)
- Hot tablespace (SSD) may benefit from dedicated buffer pool (future enhancement)

### 9.3 Indexes

**Impact**: Indexes may reside in different tablespace than heap.

**Changes**:
- Index stores heap TIDs as `(GPID, slot)` (64-bit GPID instead of 32-bit page_id)
- Index pages themselves stored in `index.tablespace_id`
- Visibility checks fetch heap tuple via GPID (cross-tablespace access)

**Example**:
```sql
CREATE TABLE orders (id INT PRIMARY KEY, ...) TABLESPACE ts_data;
CREATE INDEX idx_orders_date ON orders(order_date) TABLESPACE ts_index;
```
- Heap tuples: `ts_data` (tablespace_id=2)
- Index entries: `ts_index` (tablespace_id=3)
- Index stores TIDs like `(gpid=0x0002000000001234, slot=5)` → points to ts_data

### 9.4 Backup and Recovery

**Impact**: Backups must capture all tablespace files.

**Backup Strategy**:
1. **Cold Backup**: Shutdown database, copy all `.sbdb` and `.sbts` files
2. **Hot Backup** (future): Use copy-on-write or snapshot mechanisms per tablespace

**Recovery Strategy**:
- Restore all files to exact paths recorded in pg_tablespace
- Database initialization reads pg_tablespace and opens all tablespace files
- Missing tablespace file → ERROR (database won't start)

**Future Enhancement**: Support relocating tablespace files at restore time (similar to PostgreSQL tablespace_map)

---

## 10. Implementation Plan

### Alpha Stage 1: Core Infrastructure (40-60 hours)

#### Task 1.1: Data Structures and Catalog (12-16 hours)
- Define `TablespaceHeader` structure
- Define `SBTablespaceCatalog` and `SBTablespaceFileCatalog` structures
- Add `pg_tablespace` and `pg_tablespace_files` system tables to CatalogManager
- Add `tablespace_id` to IndexInfo (currently missing)
- Create `TablespaceInfo` in-memory structure

**Deliverables**:
- `include/scratchbird/core/tablespace.h` (~300 lines)
- Updates to `catalog_manager.h` (~100 lines)
- Updates to `catalog_manager.cpp` for new tables (~400 lines)

#### Task 1.2: GPID Addressing (16-24 hours)
- Define GPID type (`uint64_t`) and helper functions:
  - `makeGPID(tablespace_id, page_number)`
  - `getTablespaceID(gpid)`
  - `getPageNumber(gpid)`
- Update `PageManager` to use GPID internally
- Update `BufferPool` to support GPID-based pinning
- Update `Database` for `read_page_global()` / `write_page_global()`

**Deliverables**:
- `include/scratchbird/core/gpid.h` (~100 lines)
- Updates to `page_manager.h/cpp` (~500 lines)
- Updates to `buffer_pool.h/cpp` (~400 lines)
- Updates to `database.h/cpp` (~300 lines)

#### Task 1.3: Tablespace File Management (12-20 hours)
- Implement `PageManager::createTablespace()`
- Implement `PageManager::openTablespace()`
- Implement `PageManager::closeTablespace()`
- Implement tablespace file descriptor map in Database
- Implement tablespace-specific FSM (Free Space Map)

**Deliverables**:
- `src/core/tablespace.cpp` (~600 lines)
- Integration with `page_manager.cpp` (~400 lines)

### Alpha Stage 2: SQL DDL and Catalog Operations (30-40 hours)

#### Task 2.1: CREATE/DROP TABLESPACE (12-16 hours)
- SQL parser support for `CREATE TABLESPACE` syntax
- SQL parser support for `DROP TABLESPACE` syntax
- CatalogManager implementation of create/drop operations
- Validation logic (path validation, duplicate names, etc.)

**Deliverables**:
- Parser changes in `sql/parser` (~300 lines)
- CatalogManager methods (~400 lines)
- Unit tests (~300 lines)

#### Task 2.2: ALTER TABLESPACE (8-12 hours)
- SQL parser support for `ALTER TABLESPACE` syntax
- CatalogManager update operations (autoextend, maxsize, rename)
- Apply changes to TablespaceHeader on disk

**Deliverables**:
- Parser changes (~200 lines)
- CatalogManager methods (~300 lines)
- Unit tests (~200 lines)

#### Task 2.3: Table/Index Creation with Tablespace (10-12 hours)
- SQL parser: `CREATE TABLE ... TABLESPACE` clause
- SQL parser: `CREATE INDEX ... TABLESPACE` clause
- Update StorageEngine to allocate pages in specified tablespace
- Update catalog writes for `table.tablespace_id`, `index.tablespace_id`

**Deliverables**:
- Parser changes (~200 lines)
- StorageEngine changes (~300 lines)
- Unit tests (~300 lines)

### Alpha Stage 3: Autoextend and Growth (20-30 hours)

#### Task 3.1: Autoextend Implementation (12-18 hours)
- Implement `PageManager::extendTablespace()` based on autoextend parameters
- Hook into allocation path: when out of free pages, call extendTablespace()
- Enforce MAXSIZE limits
- Update tablespace statistics after extension

**Deliverables**:
- `page_manager.cpp` extensions (~400 lines)
- Unit tests (~300 lines)

#### Task 3.2: Preallocation (8-12 hours)
- Implement `preallocatePages()` helper in PageManager
- Call during `createTablespace()` if `prealloc_pages > 0`
- Write zeroed pages to disk (or use `fallocate()` on Linux)

**Deliverables**:
- PageManager method (~200 lines)
- Unit tests (~200 lines)

### Alpha Stage 4: Migration (Offline Only) (30-40 hours)

#### Task 4.1: Offline Table Migration (20-28 hours)
- Implement `CatalogManager::moveTableToTablespace()` (offline mode)
- Algorithm:
  1. Lock table (EXCLUSIVE)
  2. Scan all heap pages
  3. Copy tuples to new tablespace
  4. Update all indexes with new GPIDs
  5. Update catalog
  6. Free old pages
- Add progress tracking and cancellation support

**Deliverables**:
- CatalogManager method (~600 lines)
- Integration test (~400 lines)

#### Task 4.2: Offline Index Migration (10-12 hours)
- Implement `CatalogManager::moveIndexToTablespace()`
- Simpler than table (no heap tuple updates)
- Rebuild index in new tablespace, update catalog

**Deliverables**:
- CatalogManager method (~300 lines)
- Integration test (~200 lines)

---

### Summary: Alpha Stages 1-4 Total

| Stage | Task | Estimated Hours | Priority |
|-------|------|-----------------|----------|
| Alpha 1 | Core Infrastructure | 40-60 | CRITICAL |
| Alpha 2 | SQL DDL | 30-40 | HIGH |
| Alpha 3 | Autoextend | 20-30 | MEDIUM |
| Alpha 4 | Migration (Offline) | 30-40 | HIGH |
| **TOTAL** | | **120-170** | |

**Adjusted Total**: **120-180 hours** (3-4.5 weeks for single developer)

---

### Post-Alpha Extensions

#### Stage 5: Online Migration (40-60 hours) (post-alpha)
- Implement shadow table approach
- Delta log for concurrent writes
- Catch-up phase with incremental application
- Brief exclusive lock for final swap

#### Stage 6: Attach/Detach (20-30 hours) (post-alpha)
- ALTER TABLESPACE ATTACH/DETACH syntax
- UUID validation and FORCE option
- Cross-database attach with warnings
- Recovery from detached tablespace (startup error handling)

#### Stage 7: Advanced Features (30-50 hours) (post-alpha)
- Per-tablespace buffer pools
- Tablespace-level backup/restore
- Parallel extension (multi-threaded autoextend)
- Compression per tablespace

---

## 11. Testing Strategy

### 11.1 Unit Tests

**Tablespace Creation and Management**:
- Create tablespace with various configurations (autoextend on/off, maxsize, prealloc)
- Open/close tablespace files
- Validate header checksums and metadata

**GPID Operations**:
- GPID encoding/decoding (`makeGPID`, `getTablespaceID`, `getPageNumber`)
- Allocate pages in different tablespaces
- Free pages via GPID

**Catalog Operations**:
- Insert/update/delete pg_tablespace entries
- List all tablespaces
- Get tablespace by name/ID

**Autoextend**:
- Trigger autoextend when free pages exhausted
- Verify file grows by autoextend_size_mb
- Enforce MAXSIZE limit (allocation fails when limit reached)

### 11.2 Integration Tests

**Cross-Tablespace Tables**:
- Create table in tablespace A, index in tablespace B
- Insert rows, query via index
- Verify heap tuples in A, index pages in B

**Partitioning**:
- Create partitioned table with partitions in different tablespaces
- Insert data spanning multiple partitions
- Query with partition pruning, verify correct tablespace access

**Migration**:
- Create table in default tablespace
- Populate with 10,000 rows
- Migrate to tablespace B offline
- Verify all rows accessible, indexes updated, old pages freed

**Attach/Detach**:
- Create table in dedicated tablespace
- Detach tablespace
- Verify database still operates (without detached data)
- Re-attach tablespace
- Verify data accessible again

### 11.3 Stress Tests

**Concurrent Allocations**:
- 100 threads allocating pages in same tablespace
- Verify no GPID collisions
- Verify autoextend thread-safe

**Large Data Volume**:
- Create table with 100M rows in tablespace
- Trigger multiple autoextends (verify grows to 100GB+)
- Query performance with GPID addressing

**MAXSIZE Enforcement**:
- Create tablespace with MAXSIZE 1000MB
- Insert until tablespace full
- Verify graceful error (not crash)

### 11.4 Failure Tests

**Tablespace File Missing**:
- Remove .sbts file from filesystem
- Attempt to start database
- Verify ERROR with clear message (not crash)

**Corrupt Tablespace Header**:
- Corrupt page 0 of tablespace file
- Attempt to open tablespace
- Verify checksum error detected

**Disk Full During Autoextend**:
- Fill filesystem to capacity
- Trigger autoextend
- Verify graceful failure (transaction rolls back, error reported)

---

## 12. Appendix: Research Findings

### A.1 PostgreSQL Deep Dive

**Implementation Files**:
- `src/backend/commands/tablespace.c`: CREATE/DROP TABLESPACE
- `src/backend/storage/file/fd.c`: Tablespace file descriptor management
- `src/include/catalog/pg_tablespace.h`: Catalog structure

**Key Insights**:
- Tablespaces are symlinks in `pg_tblspc/` directory pointing to real directories
- Each tablespace has subdirectory per database OID
- Moving tablespace requires `pg_repack` extension (external tool)

**Limitations**:
- Changing tablespace location requires downtime (or complex pg_repack)
- No built-in incremental growth control (depends on filesystem)

### A.2 Oracle Deep Dive

**Documentation**:
- Oracle Database Administrator's Guide: "Managing Tablespaces and Datafiles"
- Oracle 19c SQL Language Reference: `CREATE TABLESPACE`

**Key Insights**:
- Bigfile tablespaces introduced in Oracle 10g (single 128TB datafile)
- `AUTOEXTEND ON NEXT 10M MAXSIZE 2G` provides granular control
- `ALTER TABLE MOVE ONLINE` (12.2+) uses internal shadow table mechanism
- Earlier versions use `DBMS_REDEFINITION` package for online reorg

**Best Practices**:
- Use Bigfile for simplicity (no 1022-file limit)
- Set MAXSIZE to prevent runaway growth
- AUTOEXTEND size should be large enough to reduce extend frequency (reduces fragmentation)

### A.3 MySQL/InnoDB Deep Dive

**Implementation Files**:
- `storage/innobase/fsp/fsp0space.cc`: Tablespace management
- `storage/innobase/dict/dict0dict.cc`: Data dictionary for tablespaces

**Key Insights**:
- `innodb_file_per_table=ON` is default since MySQL 5.6 (each table separate .ibd)
- General tablespaces added in MySQL 5.7 (shared tablespace for multiple tables)
- `AUTOEXTEND_SIZE` (8.0.23+) configurable but capped at 4GB
- Transportable tablespaces use `FLUSH TABLES ... FOR EXPORT` + file copy

**Limitations**:
- No MAXSIZE for general/file-per-table (only system tablespace)
- `ALTER TABLE ... TABLESPACE` always rebuilds table (no online option)
- Partition-level tablespace support removed in MySQL 8.0.13

### A.4 Firebird Deep Dive

**Documentation**:
- Firebird 5.0 Language Reference: `ALTER DATABASE`
- IBPhoenix: "Limits of the Firebird Database"

**Key Insights**:
- Multi-file databases are **sequential extensions**, not independent tablespaces
- `ALTER DATABASE ADD FILE 'file2.fdb' STARTING AT PAGE 30001;`
- Up to 65,000 secondary files (Firebird 2.1+)
- No parallel I/O (treats multi-file as single sequential file)
- Modern advice: Avoid multi-file unless filesystem requires it (FAT32 4GB limit)

**MGA Implications for ScratchBird**:
- Stable TIDs critical: Tuples never move, so GPID addressing preserves this
- OIT-based garbage collection must span all tablespaces
- Back-version chains (xmin/xmax) work identically across tablespaces

---

## Conclusion

This specification provides a comprehensive design for tablespaces in ScratchBird, combining best practices from PostgreSQL, Oracle, MySQL/InnoDB, and Firebird. The proposed implementation preserves ScratchBird's Firebird-based MGA architecture while adding enterprise-grade data placement and lifecycle management capabilities.

**Next Steps**:
1. Review and approve specification
2. Allocate engineering resources (estimated 120-180 hours)
3. Implement Alpha Stage 1 (Core Infrastructure)
4. Implement Alpha Stage 2 (SQL DDL)
5. Implement Alpha Stage 3 (Autoextend)
6. Implement Alpha Stage 4 (Offline Migration)
7. Alpha validation with real workloads
8. Plan post-alpha stages 5-7 (Online Migration, Attach/Detach, Advanced Features)

---

**End of Specification**
