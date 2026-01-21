# ScratchBird Backup and Restore Specification

**Version:** 2.0
**Last Updated:** 2026-01-07
**Status:** ✅ Complete

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.
**Table Footnote:** In comparison tables below, ScratchBird WAL references are optional post-gold (replication/PITR).

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture and Design Principles](#2-architecture-and-design-principles)
3. [Backup Types and Strategies](#3-backup-types-and-strategies)
4. [Backup File Format](#4-backup-file-format)
5. [Backup Process](#5-backup-process)
6. [Restore Process](#6-restore-process)
7. [Point-in-Time Recovery (PITR)](#7-point-in-time-recovery-pitr)
8. [MGA-Specific Considerations](#8-mga-specific-considerations)
9. [Security and Encryption](#9-security-and-encryption)
10. [Performance Optimization](#10-performance-optimization)
11. [Validation and Verification](#11-validation-and-verification)
12. [Cluster Backup and Restore](#12-cluster-backup-and-restore)
13. [Monitoring and Observability](#13-monitoring-and-observability)
14. [Error Handling and Recovery](#14-error-handling-and-recovery)
15. [API Reference](#15-api-reference)
16. [Examples and Use Cases](#16-examples-and-use-cases)
17. [Best Practices](#17-best-practices)
18. [Testing Requirements](#18-testing-requirements)

---

## 1. Overview

### 1.1. Purpose

This document provides the complete specification for backup and restore functionality in ScratchBird Database Engine. The system is designed to provide:

- **Data Protection** - Reliable backup and recovery of database state
- **Business Continuity** - Minimal data loss and rapid recovery
- **Compliance** - Audit trails and retention policies
- **Flexibility** - Multiple backup types and strategies
- **Performance** - Minimal impact on production workloads
- **Security** - Encrypted and authenticated backups

### 1.2. Scope

This specification covers:

- Full, incremental, and differential backup strategies
- Backup file formats and data structures
- Backup and restore processes and algorithms
- Point-in-Time Recovery (PITR) capabilities
- MGA transaction visibility during backup/restore
- Encryption, compression, and validation
- Single-node and distributed cluster scenarios
- Performance optimization techniques
- Monitoring and observability

### 1.3. Key Requirements

| Requirement | Description | Priority |
|-------------|-------------|----------|
| **Consistency** | Backups MUST represent a transactionally consistent database state | P0 |
| **Reliability** | Restore MUST produce a bit-exact replica of the backed-up state | P0 |
| **Performance** | Backup MUST NOT block concurrent transactions | P0 |
| **Security** | Backups MUST support encryption and access control | P0 |
| **Validation** | Backup integrity MUST be verifiable without full restore | P1 |
| **Compression** | Space-efficient storage with minimal CPU overhead | P1 |
| **PITR** | Support for point-in-time recovery to any transaction | P2 |
| **Streaming** | Support for streaming backups to remote storage | P2 |

---

## 2. Architecture and Design Principles

### 2.1. Design Philosophy

ScratchBird's backup system is built on these principles:

1. **MGA-Native** - Leverages Multi-Generational Architecture for consistent snapshots
2. **Non-Blocking** - Uses MVCC snapshot isolation to avoid blocking writers
3. **Incremental** - Supports incremental backups to minimize storage and time
4. **Verifiable** - Built-in checksums and validation at every level
5. **Secure by Default** - Encryption and authentication integrated from the start
6. **Cluster-Aware** - Designed for distributed systems from the beginning

### 2.2. Integration with MGA

ScratchBird uses Firebird-style Multi-Generational Architecture (MGA), NOT PostgreSQL-style write-after log (WAL, optional post-gold):

- **No write-after log (WAL, optional post-gold) for Recovery** - Backup/restore does not rely on write-after log (WAL, optional post-gold) replay
- **Transaction Markers** - Uses OIT, OAT, OST, NEXT for visibility
- **Record Versioning** - Multiple versions (xmin/xmax) are backed up
- **Snapshot Isolation** - Backup sees a consistent snapshot based on transaction markers

**Critical:** Backup consistency is achieved through MGA snapshot isolation, NOT through write-after log (WAL, optional post-gold) checkpoints.

### 2.3. Backup Storage Architecture

```
Backup Storage Hierarchy:
├── Primary Backups (required)
│   ├── Full Backup (base)
│   └── Incremental Backups (chain)
├── PITR Archive (optional, low priority)
│   └── Transaction Log Archive
├── Metadata (required)
│   ├── Backup Catalog
│   ├── Checksums
│   └── Encryption Keys
└── Validation (recommended)
    ├── Test Restores
    └── Integrity Reports
```

### 2.4. Temporal Tables and PITR

**Important:** With ScratchBird's temporal table support, PITR has **lower priority** than traditional RDBMS:

- **Temporal Tables** provide application-level point-in-time queries
- **PITR** is useful for disaster recovery and replication, not primary time-travel
- **Write-after log (WAL) Archive** for PITR is optional and low-priority feature

---

## 3. Backup Types and Strategies

### 3.1. Full Backup

**Definition:** Complete snapshot of all database pages at a specific transaction point.

**Characteristics:**
- Self-contained (no dependencies)
- Largest storage footprint
- Fastest restore
- Required as base for incremental/differential backups

**Use Cases:**
- Initial backup in a backup chain
- Weekly baseline backups
- Before major schema changes
- Compliance archival

**Transaction Visibility:**
```
Full Backup sees:
  - All committed transactions with xmax < backup_snapshot_tx
  - No uncommitted transactions
  - Respects MGA visibility rules (OIT, OAT, OST)
```

### 3.2. Incremental Backup

**Definition:** Backup of pages modified since the last backup (full or incremental).

**Characteristics:**
- Requires previous backup in chain
- Smaller storage footprint
- Faster backup operation
- Slower restore (must apply chain)

**Page Selection Algorithm:**
```cpp
// Incremental backup selects pages where:
page.lsn > last_backup_lsn
```

**Use Cases:**
- Hourly or continuous backups
- Minimizing backup window
- Reducing network/storage costs

**Restore Process:**
1. Restore base full backup
2. Apply incremental backups in chronological order
3. Reconstruct final state

### 3.3. Differential Backup

**Definition:** Backup of pages modified since the last FULL backup.

**Characteristics:**
- Requires only base full backup
- Moderate storage (larger than incremental)
- Faster restore than incremental chain
- Independent differential backups

**Page Selection Algorithm:**
```cpp
// Differential backup selects pages where:
page.lsn > base_full_backup_lsn
```

**Use Cases:**
- Daily backups with weekly full
- Simplified restore process
- Avoiding long incremental chains

**Restore Process:**
1. Restore base full backup
2. Apply latest differential backup
3. Complete (only 2 files needed)

### 3.4. Hot vs. Cold Backup

#### Hot Backup (Online)

**Definition:** Backup performed while database is running and accepting transactions.

**Mechanism:**
```
1. Acquire snapshot transaction ID (backup_tx_id)
2. Record transaction markers (OIT, OAT, OST, NEXT)
3. Iterate pages using snapshot visibility
4. Concurrent transactions continue normally
5. MVCC ensures consistent view
```

**Advantages:**
- No downtime
- No service interruption
- Continuous availability

**Disadvantages:**
- Slightly larger backup (includes old versions)
- May include dead tuples (pre-sweep)

#### Cold Backup (Offline)

**Definition:** Backup performed with database shut down.

**Mechanism:**
```
1. Shutdown database gracefully
2. Wait for all transactions to complete
3. Copy database files directly
4. Restart database
```

**Advantages:**
- Simplest implementation
- Smallest backup size
- Guaranteed consistency

**Disadvantages:**
- Requires downtime
- Not suitable for 24/7 systems

### 3.5. Streaming Backup

**Definition:** Continuous or near-continuous backup stream to remote storage.

**Mechanism:**
```
1. Monitor page modifications (via LSN)
2. Stream changed pages to remote endpoint
3. Maintain remote backup state
4. Periodic checkpoints for consistency
```

**Use Cases:**
- Geo-redundant backup
- Cloud storage integration
- Continuous data protection

**Status:** Planned for Beta phase.

---

## 4. Backup File Format

### 4.1. File Format Overview

A ScratchBird backup file (`.sbbackup`) contains:

```
+---------------------------+
| File Header (256 bytes)   |
+---------------------------+
| Metadata Section          |
|   - Transaction State     |
|   - Catalog Snapshot      |
|   - Tablespace Info       |
+---------------------------+
| Page Data Blocks          |
|   - Page Header           |
|   - Page Data             |
|   - Checksum              |
+---------------------------+
| Index (optional)          |
|   - Page Directory        |
|   - Quick Seek Table      |
+---------------------------+
| Footer (256 bytes)        |
|   - Checksums             |
|   - Signatures            |
+---------------------------+
```

### 4.2. File Header Structure

**Fixed Size:** 256 bytes
**Version:** 2.0

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 8 | `char[8]` | **Magic** | `SBBACKUP` (0x5342424143_4B5550) |
| 8 | 4 | `uint32_t` | **Version** | Format version (2) |
| 12 | 4 | `uint32_t` | **Flags** | Bit flags (see below) |
| 16 | 16 | `uint8_t[16]` | **Database UUID** | Unique database identifier |
| 32 | 8 | `uint64_t` | **Creation Time** | Microseconds since Unix epoch |
| 40 | 8 | `uint64_t` | **Backup TX ID** | Transaction ID of backup snapshot |
| 48 | 8 | `uint64_t` | **OIT** | Oldest Interesting Transaction |
| 56 | 8 | `uint64_t` | **OAT** | Oldest Active Transaction |
| 64 | 8 | `uint64_t` | **OST** | Oldest Snapshot Transaction |
| 72 | 8 | `uint64_t` | **NEXT** | Next Transaction ID |
| 80 | 8 | `uint64_t` | **Page Size** | Database page size (bytes) |
| 88 | 8 | `uint64_t` | **Total Pages** | Number of pages in backup |
| 96 | 8 | `uint64_t` | **LSN** | Log Sequence Number |
| 104 | 4 | `uint32_t` | **Compression** | 0=None, 1=LZ4, 2=Zstd, 3=Snappy |
| 108 | 4 | `uint32_t` | **Encryption** | 0=None, 1=AES-256-GCM |
| 112 | 32 | `uint8_t[32]` | **Encryption Key ID** | Reference to encryption key |
| 144 | 64 | `uint8_t[64]` | **Checksum** | SHA-256 of header (offset 0-143) |
| 208 | 48 | `uint8_t[48]` | **Reserved** | Reserved for future use |

**Flags Bit Layout (offset 12, 4 bytes = 32 bits):**

| Bit | Mask | Field | Description |
|-----|------|-------|-------------|
| 0 | 0x0001 | FULL_BACKUP | 1=Full, 0=Incremental |
| 1 | 0x0002 | DIFFERENTIAL | 1=Differential, 0=Incremental |
| 2 | 0x0004 | COMPRESSED | 1=Compressed data blocks |
| 3 | 0x0008 | ENCRYPTED | 1=Encrypted data blocks |
| 4 | 0x0010 | VALIDATED | 1=Checksums verified |
| 5 | 0x0020 | CLUSTER_BACKUP | 1=Multi-shard cluster backup |
| 6 | 0x0040 | STREAMING | 1=Streaming backup format |
| 7 | 0x0080 | HOT_BACKUP | 1=Hot (online), 0=Cold |
| 8-31 | | | Reserved for future use |

### 4.3. Metadata Section

**Variable Size:** Immediately follows file header.

```
+---------------------------+
| Metadata Header (64 bytes)|
|   - Section Size          |
|   - Checksum              |
+---------------------------+
| Transaction State         |
|   - Active TX List        |
|   - Committed TX List     |
|   - Aborted TX List       |
+---------------------------+
| Catalog Snapshot          |
|   - Tables                |
|   - Indexes               |
|   - Domains               |
|   - Sequences             |
|   - Roles                 |
+---------------------------+
| Tablespace Information    |
|   - Tablespace List       |
|   - File Paths            |
|   - UUIDs                 |
+---------------------------+
| Configuration             |
|   - Database Parameters   |
|   - Security Settings     |
+---------------------------+
```

**Metadata Header Structure:**

| Offset | Size | Type | Field |
|--------|------|------|-------|
| 0 | 8 | `uint64_t` | Metadata section size (bytes) |
| 8 | 4 | `uint32_t` | Metadata format version |
| 12 | 4 | `uint32_t` | Compression type |
| 16 | 32 | `uint8_t[32]` | SHA-256 checksum of metadata |
| 48 | 16 | `uint8_t[16]` | Reserved |

**Transaction State:**

```protobuf
message TransactionState {
  uint64 snapshot_tx = 1;        // Backup snapshot transaction ID
  uint64 oit = 2;                // Oldest Interesting Transaction
  uint64 oat = 3;                // Oldest Active Transaction
  uint64 ost = 4;                // Oldest Snapshot Transaction
  uint64 next = 5;               // Next Transaction ID
  repeated uint64 active_tx = 6; // Active transactions at backup time
}
```

**Catalog Snapshot:**

Serialized catalog entries at backup snapshot:
- System catalog tables (pg_class, pg_attribute, etc.)
- User-defined objects (tables, indexes, domains, etc.)
- Security objects (roles, policies)
- Sequence states

### 4.4. Page Data Blocks

**Structure:** Variable-length records, one per backed-up page.

```
+---------------------------+
| Block Header (32 bytes)   |
|   - Page ID               |
|   - LSN                   |
|   - Compressed Size       |
|   - Uncompressed Size     |
|   - Checksum              |
+---------------------------+
| Page Data (variable)      |
|   [compressed/encrypted]  |
+---------------------------+
```

**Block Header Structure:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 8 | `uint64_t` | **Page ID** | Global page identifier |
| 8 | 8 | `uint64_t` | **LSN** | Page LSN at backup time |
| 16 | 4 | `uint32_t` | **Compressed Size** | Size of page data (bytes) |
| 20 | 4 | `uint32_t` | **Uncompressed Size** | Original page size |
| 24 | 4 | `uint32_t` | **Flags** | Compression, encryption flags |
| 28 | 4 | `uint32_t` | **CRC32** | CRC-32C checksum |

**Page Data:** Raw page bytes (optionally compressed/encrypted).

### 4.5. Page Index (Optional)

For fast random access during restore:

```
+---------------------------+
| Index Header (64 bytes)   |
|   - Entry Count           |
|   - Index Format          |
+---------------------------+
| Index Entries             |
|   [Page ID -> Offset]     |
+---------------------------+
```

**Index Entry Structure:**

| Field | Size | Type | Description |
|-------|------|------|-------------|
| Page ID | 8 | `uint64_t` | Page identifier |
| File Offset | 8 | `uint64_t` | Byte offset in backup file |
| Block Size | 4 | `uint32_t` | Size of page block (bytes) |

### 4.6. File Footer

**Fixed Size:** 256 bytes
**Location:** End of file

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 8 | `uint64_t` | **Magic** | `SBBACKND` (end marker) |
| 8 | 8 | `uint64_t` | **Header Offset** | Offset to file header (0) |
| 16 | 8 | `uint64_t` | **Metadata Offset** | Offset to metadata section |
| 24 | 8 | `uint64_t` | **Data Offset** | Offset to first page block |
| 32 | 8 | `uint64_t` | **Index Offset** | Offset to page index (or 0) |
| 40 | 8 | `uint64_t` | **Total File Size** | Total backup file size (bytes) |
| 48 | 32 | `uint8_t[32]` | **SHA-256 Checksum** | Checksum of entire file |
| 80 | 64 | `uint8_t[64]` | **Digital Signature** | Ed25519 signature (optional) |
| 144 | 112 | `uint8_t[112]` | **Reserved** | Reserved for future use |

---

## 5. Backup Process

### 5.1. Backup Lifecycle

```
┌──────────────────────────────────────────────────────────┐
│ 1. PREPARE                                               │
│    - Validate parameters                                 │
│    - Check permissions                                   │
│    - Allocate resources                                  │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│ 2. BEGIN SNAPSHOT                                        │
│    - Acquire snapshot transaction ID                     │
│    - Record transaction markers (OIT, OAT, OST, NEXT)    │
│    - Create backup file and write header                 │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│ 3. BACKUP METADATA                                       │
│    - Serialize catalog snapshot                          │
│    - Write transaction state                             │
│    - Write tablespace info                               │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│ 4. BACKUP PAGES (loop)                                   │
│    - Iterate pages using snapshot visibility             │
│    - Apply compression (if enabled)                      │
│    - Apply encryption (if enabled)                       │
│    - Write page blocks with checksums                    │
│    - Report progress                                     │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│ 5. FINALIZE                                              │
│    - Write page index (if enabled)                       │
│    - Write file footer with checksums                    │
│    - Optionally sign backup file                         │
│    - Verify backup integrity                             │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│ 6. COMMIT                                                │
│    - Close backup file                                   │
│    - Update backup catalog                               │
│    - Release resources                                   │
│    - Log completion                                      │
└──────────────────────────────────────────────────────────┘
```

### 5.2. Snapshot Acquisition

**Critical:** ScratchBird uses MGA snapshot isolation, NOT write-after log (WAL, optional post-gold) checkpoints.

```cpp
// Pseudo-code for snapshot acquisition
BackupSnapshot acquire_backup_snapshot() {
    BackupSnapshot snapshot;

    // Acquire transaction manager lock
    TxManager::lock();

    // Get current transaction markers
    snapshot.backup_tx_id = TxManager::get_next_tx_id();
    snapshot.oit = TxManager::get_oit();
    snapshot.oat = TxManager::get_oat();
    snapshot.ost = TxManager::get_ost();
    snapshot.next = TxManager::get_next();

    // Record active transactions
    snapshot.active_txs = TxManager::get_active_transactions();

    TxManager::unlock();

    return snapshot;
}
```

**Visibility Rule:**

A page version is visible to the backup snapshot if:

```cpp
bool is_visible_to_backup(PageHeader* page, BackupSnapshot* snapshot) {
    // Record must be committed before backup snapshot
    if (page->xmin >= snapshot->backup_tx_id) {
        return false;  // Created after backup started
    }

    // Record must not be deleted before backup snapshot
    if (page->xmax != 0 && page->xmax < snapshot->backup_tx_id) {
        return false;  // Deleted before backup started
    }

    // Check transaction commit status
    if (!TxManager::is_committed(page->xmin)) {
        return false;  // Created by uncommitted transaction
    }

    if (page->xmax != 0 && TxManager::is_committed(page->xmax)) {
        return false;  // Deleted by committed transaction
    }

    return true;  // Visible to backup snapshot
}
```

### 5.3. Page Iteration

**Full Backup:** Iterate all pages in database.

```cpp
void backup_full(BackupContext* ctx) {
    BackupSnapshot snapshot = acquire_backup_snapshot();

    for (auto& tablespace : database->tablespaces()) {
        for (PageID page_id = 0; page_id < tablespace.total_pages(); ++page_id) {
            Page* page = tablespace.read_page(page_id);

            if (is_visible_to_backup(page->header(), &snapshot)) {
                write_page_block(ctx, page);
            }

            if (ctx->progress_callback) {
                ctx->progress_callback(page_id, tablespace.total_pages());
            }
        }
    }
}
```

**Incremental Backup:** Iterate only changed pages.

```cpp
void backup_incremental(BackupContext* ctx, LSN base_lsn) {
    BackupSnapshot snapshot = acquire_backup_snapshot();

    for (auto& tablespace : database->tablespaces()) {
        for (PageID page_id = 0; page_id < tablespace.total_pages(); ++page_id) {
            Page* page = tablespace.read_page(page_id);

            // Only backup pages modified since base LSN
            if (page->header()->lsn > base_lsn) {
                if (is_visible_to_backup(page->header(), &snapshot)) {
                    write_page_block(ctx, page);
                }
            }
        }
    }
}
```

### 5.4. Compression

**Supported Algorithms:**

| Algorithm | Ratio | Speed | CPU | Use Case |
|-----------|-------|-------|-----|----------|
| **None** | 1.0x | Fastest | None | Fast backup, slow storage |
| **LZ4** | 2-3x | Very Fast | Low | Balanced default |
| **Zstd** | 3-5x | Fast | Medium | Space-efficient |
| **Snappy** | 2-2.5x | Very Fast | Low | Streaming backup |

**Compression Process:**

```cpp
void write_page_block_compressed(BackupFile* file, Page* page) {
    size_t uncompressed_size = page->size();
    size_t compressed_size;
    uint8_t* compressed_data;

    // Compress page data
    switch (file->compression_type()) {
        case LZ4:
            compressed_data = lz4_compress(page->data(), uncompressed_size, &compressed_size);
            break;
        case ZSTD:
            compressed_data = zstd_compress(page->data(), uncompressed_size, &compressed_size);
            break;
        default:
            compressed_data = page->data();
            compressed_size = uncompressed_size;
    }

    // Write block header
    BlockHeader header = {
        .page_id = page->id(),
        .lsn = page->lsn(),
        .compressed_size = compressed_size,
        .uncompressed_size = uncompressed_size,
        .flags = file->compression_type(),
        .crc32 = crc32c(compressed_data, compressed_size)
    };

    file->write(&header, sizeof(header));
    file->write(compressed_data, compressed_size);
}
```

### 5.5. Progress Reporting

**Callback Interface:**

```cpp
typedef void (*BackupProgressCallback)(
    uint64_t pages_completed,
    uint64_t total_pages,
    uint64_t bytes_written,
    void* user_data
);

struct BackupOptions {
    BackupProgressCallback progress_callback;
    void* progress_user_data;
    uint32_t progress_interval_ms;  // Callback interval
};
```

**Progress Metrics:**

- Pages processed / total pages
- Bytes written / estimated total
- Elapsed time / estimated time remaining
- Current throughput (pages/sec, MB/sec)

### 5.6. Cancellation

**Cancellation Mechanism:**

```cpp
struct BackupContext {
    std::atomic<bool> cancel_requested{false};
    std::mutex mutex;
    std::condition_variable cv;
};

void cancel_backup(BackupContext* ctx) {
    ctx->cancel_requested = true;
    ctx->cv.notify_all();
}

void backup_main_loop(BackupContext* ctx) {
    for (PageID page_id = 0; page_id < total_pages; ++page_id) {
        // Check for cancellation
        if (ctx->cancel_requested) {
            cleanup_partial_backup(ctx);
            throw BackupCancelledException();
        }

        backup_page(ctx, page_id);
    }
}
```

**Cleanup on Cancellation:**

1. Close backup file
2. Delete partial backup file (unless preserve_on_cancel flag is set)
3. Release resources
4. Log cancellation event

---

## 6. Restore Process

### 6.1. Restore Lifecycle

```
┌──────────────────────────────────────────────────────────┐
│ 1. VALIDATE BACKUP                                       │
│    - Check file magic and version                        │
│    - Verify checksums                                    │
│    - Validate metadata                                   │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│ 2. PREPARE RESTORE                                       │
│    - Check target location                               │
│    - Verify sufficient disk space                        │
│    - Check permissions                                   │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│ 3. CREATE DATABASE FILES                                 │
│    - Create database directory                           │
│    - Initialize database header                          │
│    - Create tablespace files                             │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│ 4. RESTORE METADATA                                      │
│    - Restore catalog (tables, indexes, etc.)             │
│    - Restore transaction state                           │
│    - Restore tablespace configuration                    │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│ 5. RESTORE PAGES (loop)                                  │
│    - Read page blocks from backup                        │
│    - Decrypt (if encrypted)                              │
│    - Decompress (if compressed)                          │
│    - Write pages to database files                       │
│    - Report progress                                     │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│ 6. REBUILD INDEXES (optional)                            │
│    - Rebuild secondary indexes                           │
│    - Verify index integrity                              │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│ 7. FINALIZE                                              │
│    - Set transaction markers (OIT, OAT, OST, NEXT)       │
│    - Mark database as online                             │
│    - Run integrity checks                                │
│    - Log completion                                      │
└──────────────────────────────────────────────────────────┘
```

### 6.2. Backup Validation

**Validation Steps:**

```cpp
bool validate_backup_file(const std::string& backup_path) {
    BackupFile file(backup_path);

    // 1. Check file magic
    if (file.read_magic() != "SBBACKUP") {
        return false;  // Not a ScratchBird backup file
    }

    // 2. Check format version
    uint32_t version = file.read_version();
    if (version > CURRENT_BACKUP_VERSION) {
        return false;  // Unsupported version
    }

    // 3. Verify header checksum
    if (!file.verify_header_checksum()) {
        return false;  // Corrupted header
    }

    // 4. Verify metadata checksum
    if (!file.verify_metadata_checksum()) {
        return false;  // Corrupted metadata
    }

    // 5. Verify footer and file checksum
    if (!file.verify_footer_checksum()) {
        return false;  // Corrupted file
    }

    // 6. Verify digital signature (if present)
    if (file.has_signature() && !file.verify_signature()) {
        return false;  // Invalid signature
    }

    return true;
}
```

### 6.3. Incremental Restore

**Restore Chain:**

```
Base Full Backup (T0)
    │
    ├── Incremental Backup 1 (T0 → T1)
    │
    ├── Incremental Backup 2 (T1 → T2)
    │
    └── Incremental Backup 3 (T2 → T3)

Restore to T3:
  1. Restore Base Full (T0)
  2. Apply Incremental 1 (T0 → T1)
  3. Apply Incremental 2 (T1 → T2)
  4. Apply Incremental 3 (T2 → T3)
```

**Algorithm:**

```cpp
void restore_incremental_chain(const std::vector<std::string>& chain) {
    // Sort by backup timestamp
    std::sort(chain.begin(), chain.end(), compare_by_timestamp);

    // First file must be a full backup
    if (!chain[0].is_full_backup()) {
        throw RestoreException("First backup must be full backup");
    }

    // Restore base full backup
    restore_full(chain[0]);

    // Apply incremental backups in order
    for (size_t i = 1; i < chain.size(); ++i) {
        if (!chain[i].is_incremental()) {
            throw RestoreException("Expected incremental backup");
        }

        apply_incremental(chain[i]);
    }

    finalize_restore();
}

void apply_incremental(const std::string& incremental_backup) {
    BackupFile file(incremental_backup);

    // Read and apply each page block
    while (file.has_more_pages()) {
        PageBlock block = file.read_page_block();

        // Overwrite page in database
        database->write_page(block.page_id, block.data);
    }
}
```

### 6.4. Restore Performance

**Parallel Restore:**

```cpp
void restore_parallel(BackupFile* file, size_t num_threads) {
    // Build page index for random access
    PageIndex index = file->build_page_index();

    // Split pages among worker threads
    std::vector<std::thread> workers;
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([&, i]() {
            for (size_t j = i; j < index.size(); j += num_threads) {
                PageBlock block = file->read_page_block(index[j].offset);
                database->write_page(block.page_id, block.data);
            }
        });
    }

    // Wait for all workers to complete
    for (auto& worker : workers) {
        worker.join();
    }
}
```

**Optimization Techniques:**

1. **Parallel Page Restore** - Multiple threads restore pages concurrently
2. **Buffered I/O** - Large read/write buffers to minimize syscalls
3. **Direct I/O** - Bypass OS page cache for large restores
4. **Index Rebuild** - Optionally defer index rebuild to post-restore

---

## 7. Point-in-Time Recovery (PITR)

### 7.1. Overview

**Note:** PITR has **lower priority** in ScratchBird due to temporal table support.

**PITR Capabilities:**

- Restore to specific transaction ID
- Restore to specific timestamp
- Restore to named savepoint

**Use Cases:**

- Disaster recovery (accidental data deletion)
- Logical replication initialization
- Compliance and audit requirements

### 7.2. PITR Architecture

**Requirements for PITR:**

1. **Base Full Backup** - Starting point
2. **Transaction Log Archive** - Continuous log of transactions
3. **Recovery Target** - Transaction ID or timestamp

**Note:** Transaction log archive is distinct from write-after log (WAL, optional post-gold), which ScratchBird does not use for recovery.

### 7.3. Transaction Log Archive

**Archive Format:**

```
Transaction Log Archive File (.sbtxlog):

+---------------------------+
| Archive Header            |
|   - Start TX ID           |
|   - End TX ID             |
|   - Timestamp Range       |
+---------------------------+
| Transaction Records       |
|   - TX ID                 |
|   - Operation Type        |
|   - Table/Row             |
|   - Old/New Values        |
+---------------------------+
```

**Transaction Record:**

```protobuf
message TransactionRecord {
  uint64 tx_id = 1;
  uint64 timestamp = 2;
  enum OpType {
    INSERT = 0;
    UPDATE = 1;
    DELETE = 2;
    DDL = 3;
  }
  OpType op_type = 3;
  uint64 table_oid = 4;
  bytes old_row = 5;  // For UPDATE/DELETE
  bytes new_row = 6;  // For INSERT/UPDATE
}
```

### 7.4. PITR Restore Process

**Algorithm:**

```cpp
void restore_pitr(const std::string& base_backup,
                  uint64_t target_tx_id) {
    // 1. Restore base full backup
    restore_full(base_backup);

    // 2. Find all transaction log archives after base backup
    uint64_t base_tx_id = base_backup.get_tx_id();
    std::vector<std::string> archives = find_archives(base_tx_id, target_tx_id);

    // 3. Replay transactions up to target
    for (const auto& archive : archives) {
        TxLogFile log(archive);

        while (log.has_more_records()) {
            TxRecord record = log.read_record();

            if (record.tx_id > target_tx_id) {
                break;  // Reached target
            }

            replay_transaction(record);
        }
    }

    // 4. Set database transaction markers to target
    database->set_next_tx_id(target_tx_id + 1);
    database->set_oit(calculate_oit());
    database->set_oat(calculate_oat());

    finalize_restore();
}

void replay_transaction(const TxRecord& record) {
    switch (record.op_type) {
        case INSERT:
            executor->insert_row(record.table_oid, record.new_row);
            break;
        case UPDATE:
            executor->update_row(record.table_oid, record.old_row, record.new_row);
            break;
        case DELETE:
            executor->delete_row(record.table_oid, record.old_row);
            break;
        case DDL:
            executor->execute_ddl(record.ddl_statement);
            break;
    }
}
```

### 7.5. PITR and Temporal Tables

**Important:** ScratchBird's temporal tables provide application-level time travel:

```sql
-- Query historical data without PITR
SELECT * FROM employees FOR SYSTEM_TIME AS OF '2026-01-01 12:00:00';

-- Query data between time ranges
SELECT * FROM employees FOR SYSTEM_TIME BETWEEN
  '2026-01-01' AND '2026-01-07';
```

**When to Use PITR vs. Temporal Tables:**

| Use Case | Solution | Reason |
|----------|----------|--------|
| Accidental DELETE | PITR restore | Temporal tables don't help if history is deleted |
| Query historical data | Temporal tables | Faster, no restore required |
| Disaster recovery | PITR restore | Full database recovery |
| Compliance audit | Temporal tables | Built-in history tracking |
| Logical replication | PITR archive | Initialize replica at specific point |

**Priority:** PITR is **P2** (low priority) due to temporal table support.

---

## 8. MGA-Specific Considerations

### 8.1. MGA vs. Write-after log (WAL)-Based Backup

**Critical Difference:**

| Aspect | ScratchBird (MGA) | PostgreSQL (write-after log (WAL)) |
|--------|-------------------|------------------|
| **Recovery** | No write-after log (WAL) replay | Write-after log (WAL) replay required |
| **Consistency** | Snapshot isolation | Write-after log (WAL) checkpoint |
| **Backup Mode** | Snapshot transaction | pg_start_backup() |
| **Concurrency** | No blocking | No blocking |
| **Record Versions** | Multiple versions backed up | Single version + write-after log (WAL) |

**MGA Implications:**

1. **No write-after log (WAL) Dependency** - Backup does not require write-after log (WAL) archiving for recovery
2. **Snapshot Visibility** - Backup sees consistent snapshot using transaction markers
3. **Old Versions** - Backup may include dead tuples (before sweep)
4. **Transaction Markers** - OIT, OAT, OST, NEXT must be preserved

### 8.2. Transaction Visibility During Backup

**Visibility Rules:**

```cpp
// MGA visibility check for backup
bool is_visible_to_backup(TupleHeader* tuple, BackupSnapshot* snapshot) {
    // Check xmin (creation transaction)
    if (tuple->xmin >= snapshot->backup_tx_id) {
        return false;  // Created after backup started
    }

    if (!is_tx_committed(tuple->xmin, snapshot)) {
        return false;  // Created by uncommitted transaction
    }

    // Check xmax (deletion transaction)
    if (tuple->xmax == 0) {
        return true;  // Not deleted, visible
    }

    if (tuple->xmax >= snapshot->backup_tx_id) {
        return true;  // Deleted after backup, still visible
    }

    if (is_tx_committed(tuple->xmax, snapshot)) {
        return false;  // Deleted by committed transaction before backup
    }

    return true;  // Visible
}

bool is_tx_committed(uint64_t tx_id, BackupSnapshot* snapshot) {
    // Check if transaction is in committed state
    TxState state = tx_manager->get_tx_state(tx_id);
    return state == TX_COMMITTED;
}
```

### 8.3. Garbage Collection and Backup

**Issue:** Backup may include dead tuples if sweep has not run.

**Solutions:**

1. **Run Sweep Before Backup** (recommended for cold backups)
   ```sql
   -- Force sweep/GC before backup (VACUUM is a PostgreSQL alias)
   SWEEP;
   ```

2. **Accept Dead Tuples** (acceptable for hot backups)
   - Backup includes dead tuples
   - Sweep will clean them after restore

3. **Filter Dead Tuples During Backup** (performance trade-off)
   ```cpp
   bool should_backup_tuple(TupleHeader* tuple, BackupSnapshot* snapshot) {
       if (!is_visible_to_backup(tuple, snapshot)) {
           return false;
       }

       // Optionally skip dead tuples
       if (is_dead_tuple(tuple, snapshot)) {
           return false;  // Skip dead tuple
       }

       return true;
   }
   ```

**Recommendation:** Accept dead tuples in hot backups for simplicity.

### 8.4. Transaction Marker Preservation

**Critical:** Transaction markers must be correctly restored.

```cpp
void restore_transaction_markers(BackupMetadata* metadata) {
    // Restore transaction markers from backup
    tx_manager->set_oit(metadata->oit);
    tx_manager->set_oat(metadata->oat);
    tx_manager->set_ost(metadata->ost);
    tx_manager->set_next(metadata->next);

    // Mark all transactions before backup_tx_id as committed
    for (uint64_t tx = metadata->oit; tx < metadata->backup_tx_id; ++tx) {
        tx_manager->set_tx_state(tx, TX_COMMITTED);
    }

    // Mark transactions after backup_tx_id as aborted
    for (uint64_t tx = metadata->backup_tx_id; tx < metadata->next; ++tx) {
        tx_manager->set_tx_state(tx, TX_ABORTED);
    }
}
```

**Validation:**

After restore, verify transaction markers:
- OIT ≤ OAT ≤ OST ≤ NEXT
- All active transactions in backup are now committed or aborted

---

## 9. Security and Encryption

### 9.1. Encryption Architecture

**Encryption Modes:**

| Mode | Scope | Use Case |
|------|-------|----------|
| **File-Level** | Entire backup file | Transport, storage |
| **Block-Level** | Per-page encryption | Selective encryption |
| **Metadata** | Catalog and headers | Sensitive schema |

**Encryption Algorithm:** AES-256-GCM (authenticated encryption)

### 9.2. Key Management

**Encryption Key Hierarchy:**

```
Master Key (MEK)
    │
    ├── Database Encryption Key (DEK)
    │       │
    │       ├── Backup Encryption Key (BEK)
    │       │       │
    │       │       └── Per-Page Keys (derived)
    │       │
    │       └── Archive Encryption Key (AEK)
    │
    └── Key Encryption Key (KEK)
```

**Key Generation:**

```cpp
struct BackupEncryptionKey {
    uint8_t key[32];        // AES-256 key (256 bits)
    uint8_t iv[16];         // Initialization vector
    uint8_t key_id[32];     // Key identifier
    uint64_t created_at;    // Creation timestamp
};

BackupEncryptionKey generate_backup_key() {
    BackupEncryptionKey bek;

    // Generate random key
    RAND_bytes(bek.key, sizeof(bek.key));
    RAND_bytes(bek.iv, sizeof(bek.iv));

    // Compute key ID (SHA-256 of key)
    SHA256(bek.key, sizeof(bek.key), bek.key_id);

    bek.created_at = current_timestamp();

    return bek;
}
```

**Key Storage:**

```sql
-- Backup encryption keys stored in system catalog
CREATE TABLE sb_backup_keys (
    key_id BYTEA PRIMARY KEY,           -- SHA-256 of key
    encrypted_key BYTEA NOT NULL,       -- Key encrypted with KEK
    iv BYTEA NOT NULL,                  -- Initialization vector
    created_at TIMESTAMP NOT NULL,
    created_by TEXT NOT NULL,           -- User who created backup
    backup_path TEXT,                   -- Path to backup file
    status TEXT DEFAULT 'active'        -- active, revoked, expired
);
```

### 9.3. Encryption Process

**Page Encryption:**

```cpp
void write_encrypted_page(BackupFile* file, Page* page, BackupEncryptionKey* bek) {
    size_t page_size = page->size();
    uint8_t* plaintext = page->data();
    uint8_t* ciphertext = (uint8_t*)malloc(page_size + 16);  // +16 for GCM tag

    // Derive per-page IV (IV + page_id)
    uint8_t page_iv[16];
    memcpy(page_iv, bek->iv, 12);
    *(uint32_t*)(page_iv + 12) = page->id();

    // Encrypt page using AES-256-GCM
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, bek->key, page_iv);

    int len;
    EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, page_size);

    // Get authentication tag
    uint8_t tag[16];
    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);

    // Write encrypted block
    BlockHeader header = {
        .page_id = page->id(),
        .compressed_size = page_size + 16,
        .uncompressed_size = page_size,
        .flags = ENCRYPTED | AES_256_GCM,
        .crc32 = crc32c(ciphertext, page_size + 16)
    };

    file->write(&header, sizeof(header));
    file->write(ciphertext, page_size);
    file->write(tag, 16);  // Write authentication tag

    EVP_CIPHER_CTX_free(ctx);
    free(ciphertext);
}
```

### 9.4. Decryption Process

**Page Decryption:**

```cpp
void read_encrypted_page(BackupFile* file, PageID page_id, BackupEncryptionKey* bek) {
    BlockHeader header = file->read_block_header();

    uint8_t* ciphertext = (uint8_t*)malloc(header.compressed_size);
    uint8_t* plaintext = (uint8_t*)malloc(header.uncompressed_size);

    file->read(ciphertext, header.compressed_size - 16);

    // Read authentication tag
    uint8_t tag[16];
    file->read(tag, 16);

    // Derive per-page IV
    uint8_t page_iv[16];
    memcpy(page_iv, bek->iv, 12);
    *(uint32_t*)(page_iv + 12) = page_id;

    // Decrypt page using AES-256-GCM
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, bek->key, page_iv);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag);

    int len;
    EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, header.compressed_size - 16);

    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) <= 0) {
        throw DecryptionException("Authentication tag verification failed");
    }

    EVP_CIPHER_CTX_free(ctx);

    // Write decrypted page to database
    database->write_page(page_id, plaintext);

    free(ciphertext);
    free(plaintext);
}
```

### 9.5. Access Control

**Permissions Required:**

```sql
-- Backup privileges
GRANT BACKUP ON DATABASE mydb TO backup_user;

-- Restore privileges
GRANT RESTORE ON DATABASE mydb TO restore_user;

-- Key management privileges
GRANT KEY_MANAGEMENT ON DATABASE mydb TO security_admin;
```

**Audit Logging:**

```sql
-- All backup/restore operations are logged
INSERT INTO audit_log (event_type, user, database, details, timestamp)
VALUES ('BACKUP_START', current_user, current_database,
        json_build_object('path', '/backups/mydb.sbbackup'), now());
```

---

## 10. Performance Optimization

### 10.1. Backup Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| **Throughput** | 100+ MB/sec | Uncompressed, local disk |
| **Compression Ratio** | 2-4x | LZ4 or Zstd |
| **CPU Overhead** | < 30% | Single core, LZ4 compression |
| **I/O Impact** | < 20% | Production workload impact |
| **Memory Usage** | < 512 MB | Backup process |

### 10.2. I/O Optimization

**Direct I/O:**

```cpp
void configure_backup_io(BackupFile* file) {
    // Use direct I/O to bypass OS page cache
    #ifdef __linux__
    int fd = open(file->path(), O_WRONLY | O_CREAT | O_DIRECT, 0644);
    #endif

    // Set large read buffer (1 MB)
    file->set_read_buffer_size(1024 * 1024);

    // Set large write buffer (1 MB)
    file->set_write_buffer_size(1024 * 1024);

    // Enable write-behind (async writes)
    file->enable_async_writes(true);
}
```

**Readahead:**

```cpp
void optimize_page_reads(Database* db) {
    // Prefetch next N pages while processing current page
    const size_t PREFETCH_WINDOW = 16;

    for (PageID page_id = 0; page_id < total_pages; ++page_id) {
        // Prefetch ahead
        for (size_t i = 1; i <= PREFETCH_WINDOW && page_id + i < total_pages; ++i) {
            db->prefetch_page(page_id + i);
        }

        // Process current page
        Page* page = db->read_page(page_id);
        backup_page(page);
    }
}
```

### 10.3. Compression Optimization

**Adaptive Compression:**

```cpp
CompressionType choose_compression(Page* page) {
    // Sample page to estimate compressibility
    size_t sample_size = 4096;  // 4 KB sample
    size_t compressed_size = lz4_compress_bound(sample_size);
    uint8_t* sample_compressed = (uint8_t*)malloc(compressed_size);

    size_t actual_compressed = lz4_compress(
        page->data(),
        sample_size,
        sample_compressed
    );

    float compression_ratio = (float)sample_size / actual_compressed;

    free(sample_compressed);

    // Choose compression based on compressibility
    if (compression_ratio > 3.0) {
        return ZSTD;  // Highly compressible, use best compression
    } else if (compression_ratio > 1.5) {
        return LZ4;   // Moderately compressible, use fast compression
    } else {
        return NONE;  // Incompressible, skip compression
    }
}
```

### 10.4. Parallel Backup

**Multi-Threaded Backup:**

```cpp
void backup_parallel(BackupContext* ctx, size_t num_threads) {
    // Divide pages among worker threads
    std::vector<std::thread> workers;
    std::mutex output_mutex;

    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([&, i]() {
            for (PageID page_id = i; page_id < total_pages; page_id += num_threads) {
                Page* page = database->read_page(page_id);

                // Compress page (thread-local)
                PageBlock block = compress_page(page);

                // Write to backup file (serialized)
                {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    ctx->file->write_page_block(block);
                }
            }
        });
    }

    // Wait for all workers
    for (auto& worker : workers) {
        worker.join();
    }
}
```

**Note:** Parallel backup requires careful ordering or page index for correct restore.

### 10.5. Network Optimization (Streaming Backup)

**TCP Tuning:**

```cpp
void optimize_network_backup(int socket_fd) {
    // Enable TCP_NODELAY (disable Nagle's algorithm)
    int flag = 1;
    setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    // Set large send buffer (1 MB)
    int send_buffer = 1024 * 1024;
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &send_buffer, sizeof(send_buffer));

    // Set socket timeout
    struct timeval timeout = {.tv_sec = 30, .tv_usec = 0};
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}
```

**Compression for Network:**

Use Snappy or LZ4 for streaming backups (low latency, reasonable compression).

---

## 11. Validation and Verification

### 11.1. Backup Validation Levels

| Level | Checks | Time | Reliability |
|-------|--------|------|-------------|
| **L0: Quick** | Magic, version, header checksum | < 1 sec | 90% |
| **L1: Metadata** | + Metadata checksum, file structure | < 10 sec | 95% |
| **L2: Pages** | + All page checksums | Minutes | 99% |
| **L3: Full** | + Full file checksum, signature | Minutes | 99.9% |
| **L4: Restore** | + Test restore to temp database | Hours | 99.99% |

### 11.2. Checksum Verification

**Hierarchical Checksums:**

```
File Checksum (SHA-256)
    │
    ├── Header Checksum (SHA-256)
    │
    ├── Metadata Checksum (SHA-256)
    │
    ├── Page Block Checksums (CRC-32C)
    │   ├── Page 0: CRC-32C
    │   ├── Page 1: CRC-32C
    │   └── ...
    │
    └── Footer Checksum (SHA-256)
```

**Verification Process:**

```cpp
bool verify_backup_integrity(const std::string& backup_path) {
    BackupFile file(backup_path);

    // Level 0: Quick check
    if (!file.verify_magic()) return false;
    if (!file.verify_version()) return false;
    if (!file.verify_header_checksum()) return false;

    // Level 1: Metadata check
    if (!file.verify_metadata_checksum()) return false;

    // Level 2: All page checksums
    size_t corrupted_pages = 0;
    for (PageID page_id = 0; page_id < file.total_pages(); ++page_id) {
        if (!file.verify_page_checksum(page_id)) {
            LOG_ERROR("Page {} checksum verification failed", page_id);
            corrupted_pages++;
        }
    }

    if (corrupted_pages > 0) {
        LOG_ERROR("Found {} corrupted pages", corrupted_pages);
        return false;
    }

    // Level 3: Full file checksum
    if (!file.verify_file_checksum()) return false;

    // Level 3: Digital signature (if present)
    if (file.has_signature() && !file.verify_signature()) {
        LOG_ERROR("Digital signature verification failed");
        return false;
    }

    return true;
}
```

### 11.3. Test Restore

**Automated Test Restore:**

```bash
#!/bin/bash
# Test restore to temporary database

BACKUP_FILE="/backups/mydb_20260107.sbbackup"
TEST_DIR="/tmp/test_restore_$$"

# Create temporary restore location
mkdir -p "$TEST_DIR"

# Restore to temporary location
scratchbird-restore \
    --backup "$BACKUP_FILE" \
    --target "$TEST_DIR/testdb" \
    --validate

# Run integrity checks
scratchbird-check \
    --database "$TEST_DIR/testdb" \
    --full

# Cleanup
rm -rf "$TEST_DIR"

echo "Test restore completed successfully"
```

### 11.4. Integrity Checks After Restore

```sql
-- Run comprehensive integrity checks after restore

-- 1. Check transaction markers
SELECT verify_transaction_markers();

-- 2. Check catalog consistency
SELECT verify_catalog_integrity();

-- 3. Check all table data
SELECT verify_table_integrity('*');

-- 4. Check all indexes
SELECT verify_index_integrity('*');

-- 5. Check foreign key constraints
SELECT verify_foreign_keys('*');

-- 6. Check domain constraints
SELECT verify_domain_constraints('*');
```

---

## 12. Cluster Backup and Restore

### 12.1. Distributed Backup Architecture

**Cluster Backup Strategy:**

```
Cluster (3 nodes, 16 shards, RF=2):
    Node 1: Shards [0,1,2,3,4]
    Node 2: Shards [4,5,6,7,8,9,10]
    Node 3: Shards [10,11,12,13,14,15,0,1,2]

Backup Approaches:
1. Per-Shard Backup (recommended)
   - Each shard backed up independently
   - Parallel backup across nodes
   - Flexible restore

2. Per-Node Backup
   - Each node backs up its shards
   - Simpler orchestration
   - Node-level granularity

3. Cluster-Consistent Snapshot
   - Coordinated snapshot across all nodes
   - Globally consistent point-in-time
   - Most complex
```

### 12.2. Per-Shard Backup

**Algorithm:**

```cpp
void backup_cluster_sharded(ClusterBackupContext* ctx) {
    // 1. Coordinate cluster-wide snapshot
    uint64_t cluster_snapshot_tx = coordinator->acquire_cluster_snapshot();

    // 2. Backup each shard in parallel
    std::vector<std::future<void>> shard_backups;

    for (ShardID shard_id = 0; shard_id < ctx->num_shards; ++shard_id) {
        shard_backups.push_back(std::async([&, shard_id]() {
            Node* primary = cluster->get_shard_primary(shard_id);

            BackupOptions opts;
            opts.snapshot_tx = cluster_snapshot_tx;
            opts.output_path = format("/backups/shard_{}.sbbackup", shard_id);

            primary->backup_shard(shard_id, opts);
        }));
    }

    // 3. Wait for all shards to complete
    for (auto& future : shard_backups) {
        future.wait();
    }

    // 4. Generate cluster backup manifest
    ClusterBackupManifest manifest;
    manifest.snapshot_tx = cluster_snapshot_tx;
    manifest.timestamp = current_timestamp();
    manifest.num_shards = ctx->num_shards;

    for (ShardID shard_id = 0; shard_id < ctx->num_shards; ++shard_id) {
        manifest.shard_backups.push_back({
            .shard_id = shard_id,
            .backup_path = format("/backups/shard_{}.sbbackup", shard_id),
            .checksum = calculate_file_checksum(manifest.shard_backups.back().backup_path)
        });
    }

    // 5. Write manifest
    write_cluster_manifest(manifest, "/backups/cluster_manifest.json");
}
```

### 12.3. Cluster-Consistent Snapshot

**Coordinated Snapshot Protocol:**

```
Coordinator (Raft Leader):
    1. Broadcast PREPARE_SNAPSHOT to all nodes
    2. Wait for all nodes to acknowledge (quorum)
    3. Assign global snapshot TX ID
    4. Broadcast BEGIN_SNAPSHOT with TX ID
    5. All nodes begin backup using same TX ID

Nodes:
    1. Receive PREPARE_SNAPSHOT
    2. Flush pending transactions
    3. Acknowledge to coordinator
    4. Receive BEGIN_SNAPSHOT with TX ID
    5. Acquire local snapshot using TX ID
    6. Perform backup
```

**Implementation:**

```cpp
uint64_t ClusterCoordinator::acquire_cluster_snapshot() {
    // 1. Prepare phase
    PrepareSnapshotRequest req;
    std::vector<PrepareSnapshotResponse> responses;

    for (Node* node : cluster->nodes()) {
        PrepareSnapshotResponse resp = node->prepare_snapshot(req);
        responses.push_back(resp);
    }

    // 2. Wait for quorum
    if (responses.size() < quorum_size()) {
        throw ClusterBackupException("Failed to reach quorum");
    }

    // 3. Assign global snapshot TX ID
    uint64_t cluster_snapshot_tx = tx_manager->get_next_tx_id();

    // 4. Begin snapshot on all nodes
    BeginSnapshotRequest begin_req;
    begin_req.snapshot_tx = cluster_snapshot_tx;

    for (Node* node : cluster->nodes()) {
        node->begin_snapshot(begin_req);
    }

    return cluster_snapshot_tx;
}
```

### 12.4. Cluster Restore

**Restore Process:**

```cpp
void restore_cluster(const std::string& manifest_path) {
    // 1. Read cluster backup manifest
    ClusterBackupManifest manifest = read_cluster_manifest(manifest_path);

    // 2. Validate manifest
    validate_manifest(manifest);

    // 3. Restore each shard to its primary node
    std::vector<std::future<void>> shard_restores;

    for (const auto& shard_backup : manifest.shard_backups) {
        shard_restores.push_back(std::async([&]() {
            Node* primary = cluster->get_shard_primary(shard_backup.shard_id);
            primary->restore_shard(shard_backup.backup_path);
        }));
    }

    // 4. Wait for all shards to complete
    for (auto& future : shard_restores) {
        future.wait();
    }

    // 5. Replicate to secondary replicas
    for (ShardID shard_id = 0; shard_id < manifest.num_shards; ++shard_id) {
        replicate_shard_to_secondaries(shard_id);
    }

    // 6. Verify cluster consistency
    verify_cluster_integrity();
}
```

### 12.5. Cluster Backup Manifest

**JSON Format:**

```json
{
  "version": "1.0",
  "cluster_id": "550e8400-e29b-41d4-a716-446655440000",
  "snapshot_tx": 123456789,
  "timestamp": "2026-01-07T12:00:00Z",
  "num_shards": 16,
  "replication_factor": 2,
  "shard_backups": [
    {
      "shard_id": 0,
      "backup_path": "/backups/shard_0.sbbackup",
      "checksum": "sha256:abcdef...",
      "size_bytes": 1073741824,
      "primary_node": "node1"
    },
    // ... more shards
  ],
  "metadata": {
    "backup_tool": "scratchbird-backup",
    "backup_tool_version": "1.0.0",
    "compression": "lz4",
    "encryption": "aes-256-gcm"
  }
}
```

---

## 13. Monitoring and Observability

### 13.1. Backup Metrics

**Key Metrics:**

| Metric | Type | Description | Alerting |
|--------|------|-------------|----------|
| `backup_duration_seconds` | Gauge | Time to complete backup | > 4 hours |
| `backup_size_bytes` | Gauge | Backup file size | > 100 GB |
| `backup_throughput_mbps` | Gauge | Backup write throughput | < 50 MB/sec |
| `backup_pages_total` | Counter | Total pages backed up | - |
| `backup_pages_skipped` | Counter | Pages skipped (incremental) | - |
| `backup_compression_ratio` | Gauge | Compression ratio achieved | < 1.5x |
| `backup_errors_total` | Counter | Backup errors encountered | > 0 |
| `backup_success_total` | Counter | Successful backups | - |
| `backup_last_success_timestamp` | Gauge | Timestamp of last successful backup | > 24h ago |

**OpenTelemetry Metrics:**

```cpp
// Instrument backup process with OpenTelemetry
void backup_with_metrics(BackupContext* ctx) {
    auto meter = opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter("scratchbird.backup");

    auto duration = meter->CreateHistogram("backup_duration_seconds");
    auto throughput = meter->CreateGauge("backup_throughput_mbps");
    auto pages = meter->CreateCounter("backup_pages_total");

    auto start = std::chrono::steady_clock::now();

    try {
        perform_backup(ctx);

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();

        duration->Record(elapsed);
        throughput->Record(ctx->bytes_written / elapsed / 1024 / 1024);
        pages->Add(ctx->pages_backed_up);

        meter->CreateCounter("backup_success_total")->Add(1);

    } catch (const std::exception& e) {
        meter->CreateCounter("backup_errors_total")->Add(1);
        throw;
    }
}
```

### 13.2. Progress Reporting

**Real-Time Progress:**

```cpp
struct BackupProgress {
    uint64_t pages_completed;
    uint64_t total_pages;
    uint64_t bytes_written;
    uint64_t estimated_total_bytes;
    double elapsed_seconds;
    double estimated_remaining_seconds;
    double throughput_mbps;
};

void report_backup_progress(const BackupProgress& progress) {
    double percent = 100.0 * progress.pages_completed / progress.total_pages;

    printf("\rBackup progress: %.1f%% (%lu/%lu pages) | "
           "%.1f MB/sec | ETA: %.0f sec",
           percent,
           progress.pages_completed,
           progress.total_pages,
           progress.throughput_mbps,
           progress.estimated_remaining_seconds);

    fflush(stdout);
}
```

### 13.3. Logging

**Structured Logging:**

```cpp
// Log backup start
LOG_INFO("Backup started",
    "database", ctx->database_name,
    "backup_path", ctx->backup_path,
    "backup_type", ctx->backup_type,
    "compression", ctx->compression,
    "encryption", ctx->encryption);

// Log backup progress
LOG_INFO("Backup progress",
    "pages_completed", progress.pages_completed,
    "total_pages", progress.total_pages,
    "percent", percent,
    "throughput_mbps", progress.throughput_mbps);

// Log backup completion
LOG_INFO("Backup completed",
    "database", ctx->database_name,
    "backup_path", ctx->backup_path,
    "duration_seconds", elapsed,
    "size_bytes", file_size,
    "compression_ratio", compression_ratio,
    "checksum", checksum);
```

### 13.4. Alerting

**Alert Rules:**

```yaml
# Prometheus alert rules for backup monitoring

groups:
  - name: backup_alerts
    rules:
      - alert: BackupFailed
        expr: increase(backup_errors_total[1h]) > 0
        for: 5m
        annotations:
          summary: "Backup failed"
          description: "Backup errors detected in the last hour"

      - alert: BackupTooSlow
        expr: backup_throughput_mbps < 50
        for: 10m
        annotations:
          summary: "Backup throughput too low"
          description: "Backup throughput is below 50 MB/sec"

      - alert: BackupOverdue
        expr: time() - backup_last_success_timestamp > 86400
        annotations:
          summary: "Backup overdue"
          description: "No successful backup in the last 24 hours"

      - alert: BackupSizeTooLarge
        expr: backup_size_bytes > 100e9
        annotations:
          summary: "Backup file too large"
          description: "Backup file exceeds 100 GB"
```

---

## 14. Error Handling and Recovery

### 14.1. Backup Error Scenarios

| Error | Cause | Recovery |
|-------|-------|----------|
| **Disk Full** | Insufficient space | Free space, resume backup |
| **Permission Denied** | File/dir permissions | Fix permissions, retry |
| **Database Locked** | Concurrent operations | Wait and retry |
| **I/O Error** | Disk failure | Check disk, retry on different volume |
| **Corruption** | Data corruption | Run integrity checks, backup from replica |
| **Timeout** | Slow I/O or network | Increase timeout, optimize I/O |
| **Network Error** | Network failure | Retry with exponential backoff |
| **Out of Memory** | Large pages | Reduce buffer sizes, process in chunks |

### 14.2. Backup Retry Logic

```cpp
bool backup_with_retry(BackupContext* ctx, int max_retries = 3) {
    int attempt = 0;
    int backoff_ms = 1000;  // Start with 1 second

    while (attempt < max_retries) {
        try {
            perform_backup(ctx);
            return true;  // Success

        } catch (const DiskFullException& e) {
            LOG_ERROR("Disk full, cannot retry: {}", e.what());
            throw;  // Cannot retry

        } catch (const IOErrorException& e) {
            LOG_WARN("I/O error on attempt {}: {}", attempt + 1, e.what());
            attempt++;

            if (attempt < max_retries) {
                std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
                backoff_ms *= 2;  // Exponential backoff
            }

        } catch (const NetworkErrorException& e) {
            LOG_WARN("Network error on attempt {}: {}", attempt + 1, e.what());
            attempt++;

            if (attempt < max_retries) {
                std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
                backoff_ms *= 2;
            }
        }
    }

    LOG_ERROR("Backup failed after {} attempts", max_retries);
    return false;
}
```

### 14.3. Restore Error Scenarios

| Error | Cause | Recovery |
|-------|-------|----------|
| **Corrupted Backup** | File corruption | Use backup validation, restore from different backup |
| **Missing File** | File deleted/moved | Locate file, update path |
| **Version Mismatch** | Incompatible version | Upgrade/downgrade software, use migration tool |
| **Decryption Failure** | Wrong key | Obtain correct key |
| **Checksum Mismatch** | File corruption | Retry, use validated backup |
| **Insufficient Space** | Disk full | Free space, restore to larger volume |
| **Permission Denied** | File/dir permissions | Fix permissions |

### 14.4. Partial Backup Recovery

**Scenario:** Backup interrupted, partial backup file exists.

**Options:**

1. **Discard and Restart** (default)
   ```cpp
   if (is_partial_backup(backup_path)) {
       LOG_WARN("Partial backup detected, discarding");
       unlink(backup_path);
       start_new_backup();
   }
   ```

2. **Resume Backup** (if supported)
   ```cpp
   if (is_partial_backup(backup_path) && ctx->allow_resume) {
       LOG_INFO("Resuming partial backup");
       BackupState state = load_backup_state(backup_path);
       resume_backup_from_state(state);
   }
   ```

### 14.5. Backup Verification Failures

**Scenario:** Backup completes but verification fails.

**Action:**

```cpp
bool backup_and_verify(BackupContext* ctx) {
    // Perform backup
    perform_backup(ctx);

    // Verify backup integrity
    if (!verify_backup_integrity(ctx->backup_path)) {
        LOG_ERROR("Backup verification failed, discarding corrupted backup");

        // Move to quarantine for investigation
        std::string quarantine_path = ctx->backup_path + ".corrupted";
        rename(ctx->backup_path.c_str(), quarantine_path.c_str());

        // Optionally retry
        if (ctx->retry_on_verification_failure) {
            LOG_INFO("Retrying backup");
            return backup_with_retry(ctx);
        }

        return false;
    }

    return true;
}
```

---

## 15. API Reference

### 15.1. C API

**Backup Functions:**

```c
// Initialize backup context
sb_backup_context_t* sb_backup_init(
    const char* database_path,
    const char* backup_path,
    const sb_backup_options_t* options
);

// Start backup
int sb_backup_start(sb_backup_context_t* ctx);

// Backup pages (incremental, for progress reporting)
int sb_backup_step(
    sb_backup_context_t* ctx,
    int num_pages  // Number of pages to backup (-1 for all)
);

// Finalize backup
int sb_backup_finish(sb_backup_context_t* ctx);

// Cleanup backup context
void sb_backup_free(sb_backup_context_t* ctx);

// Convenience function (all-in-one)
int sb_backup(
    const char* database_path,
    const char* backup_path,
    const sb_backup_options_t* options
);
```

**Restore Functions:**

```c
// Restore from backup
int sb_restore(
    const char* backup_path,
    const char* target_database_path,
    const sb_restore_options_t* options
);

// Validate backup file
int sb_backup_validate(
    const char* backup_path,
    int validation_level  // 0-4 (quick to full restore)
);
```

**Options Structures:**

```c
typedef struct sb_backup_options {
    int backup_type;              // 0=full, 1=incremental, 2=differential
    int compression;              // 0=none, 1=lz4, 2=zstd, 3=snappy
    int encryption;               // 0=none, 1=aes-256-gcm
    const char* encryption_key;   // Hex-encoded encryption key (64 chars)
    int num_threads;              // Number of parallel threads (0=auto)
    sb_backup_progress_callback_t progress_callback;
    void* progress_user_data;
    int progress_interval_ms;     // Callback interval
} sb_backup_options_t;

typedef struct sb_restore_options {
    int validate;                 // Validation level (0-4)
    int num_threads;              // Number of parallel threads
    const char* decryption_key;   // Hex-encoded decryption key
    sb_restore_progress_callback_t progress_callback;
    void* progress_user_data;
} sb_restore_options_t;
```

### 15.2. SQL API

**Backup Commands:**

```sql
-- Full backup
BACKUP DATABASE TO '/backups/mydb.sbbackup';

-- Incremental backup
BACKUP DATABASE INCREMENTAL TO '/backups/mydb_inc.sbbackup';

-- Compressed backup
BACKUP DATABASE TO '/backups/mydb.sbbackup' WITH COMPRESSION='lz4';

-- Encrypted backup
BACKUP DATABASE TO '/backups/mydb.sbbackup'
  WITH ENCRYPTION='aes-256-gcm', KEY='key_id_or_path';

-- Streaming backup
BACKUP DATABASE TO 's3://bucket/mydb.sbbackup'
  WITH COMPRESSION='zstd', ENCRYPTION='aes-256-gcm';
```

**Restore Commands:**

```sql
-- Restore from backup (requires superuser)
RESTORE DATABASE FROM '/backups/mydb.sbbackup' TO '/data/mydb';

-- Restore with validation
RESTORE DATABASE FROM '/backups/mydb.sbbackup' TO '/data/mydb'
  WITH VALIDATE=true;

-- Point-in-time restore
RESTORE DATABASE FROM '/backups/mydb.sbbackup'
  UNTIL TRANSACTION 123456789;
```

**Validation Commands:**

```sql
-- Validate backup file
SELECT sb_validate_backup('/backups/mydb.sbbackup', level => 3);

-- Check backup metadata
SELECT * FROM sb_backup_info('/backups/mydb.sbbackup');
```

### 15.3. C++ API

```cpp
namespace scratchbird::backup {

class BackupManager {
public:
    // Full backup
    void backup_full(
        const std::string& database_path,
        const std::string& backup_path,
        const BackupOptions& options = {}
    );

    // Incremental backup
    void backup_incremental(
        const std::string& database_path,
        const std::string& backup_path,
        const std::string& base_backup_path,
        const BackupOptions& options = {}
    );

    // Restore
    void restore(
        const std::string& backup_path,
        const std::string& target_path,
        const RestoreOptions& options = {}
    );

    // Validate
    bool validate(
        const std::string& backup_path,
        ValidationLevel level = ValidationLevel::FULL
    );

    // PITR restore
    void restore_pitr(
        const std::string& base_backup_path,
        const std::vector<std::string>& archive_paths,
        uint64_t target_tx_id
    );
};

} // namespace scratchbird::backup
```

---

## 16. Examples and Use Cases

### 16.1. Daily Full Backup

**Shell Script:**

```bash
#!/bin/bash
# Daily full backup with compression and validation

DATABASE="/data/production.sbdb"
BACKUP_DIR="/backups/daily"
DATE=$(date +%Y%m%d)
BACKUP_FILE="$BACKUP_DIR/production_$DATE.sbbackup"

# Create backup directory
mkdir -p "$BACKUP_DIR"

# Perform backup
scratchbird-backup \
    --database "$DATABASE" \
    --output "$BACKUP_FILE" \
    --compression lz4 \
    --validate \
    --progress

# Verify backup
scratchbird-validate "$BACKUP_FILE" --level 3

# Upload to S3 (optional)
aws s3 cp "$BACKUP_FILE" s3://backups/production/

# Clean up old backups (keep last 7 days)
find "$BACKUP_DIR" -name "production_*.sbbackup" -mtime +7 -delete

echo "Backup completed: $BACKUP_FILE"
```

### 16.2. Hourly Incremental Backup

**Shell Script:**

```bash
#!/bin/bash
# Hourly incremental backup

DATABASE="/data/production.sbdb"
BACKUP_DIR="/backups/incremental"
TIMESTAMP=$(date +%Y%m%d_%H%M)
BACKUP_FILE="$BACKUP_DIR/production_inc_$TIMESTAMP.sbbackup"

# Find latest full backup
BASE_BACKUP=$(ls -t /backups/daily/production_*.sbbackup | head -1)

# Perform incremental backup
scratchbird-backup \
    --database "$DATABASE" \
    --output "$BACKUP_FILE" \
    --incremental \
    --base "$BASE_BACKUP" \
    --compression lz4

echo "Incremental backup completed: $BACKUP_FILE"
```

### 16.3. Encrypted Backup

**Shell Script:**

```bash
#!/bin/bash
# Encrypted backup for compliance

DATABASE="/data/sensitive.sbdb"
BACKUP_FILE="/backups/sensitive.sbbackup"
KEY_FILE="/secure/backup.key"

# Generate encryption key (first time only)
if [ ! -f "$KEY_FILE" ]; then
    openssl rand -hex 32 > "$KEY_FILE"
    chmod 600 "$KEY_FILE"
fi

# Perform encrypted backup
scratchbird-backup \
    --database "$DATABASE" \
    --output "$BACKUP_FILE" \
    --encryption aes-256-gcm \
    --key-file "$KEY_FILE" \
    --compression zstd \
    --validate

echo "Encrypted backup completed"
```

### 16.4. Streaming Backup to S3

**Python Script:**

```python
import subprocess
import boto3

def backup_to_s3(database_path, s3_bucket, s3_key):
    """Stream backup directly to S3"""

    # Start backup process
    backup_proc = subprocess.Popen([
        'scratchbird-backup',
        '--database', database_path,
        '--output', '-',  # Write to stdout
        '--compression', 'zstd',
    ], stdout=subprocess.PIPE)

    # Upload to S3
    s3 = boto3.client('s3')
    s3.upload_fileobj(
        backup_proc.stdout,
        s3_bucket,
        s3_key
    )

    # Wait for backup to complete
    backup_proc.wait()

    if backup_proc.returncode != 0:
        raise Exception("Backup failed")

    print(f"Backup uploaded to s3://{s3_bucket}/{s3_key}")

# Usage
backup_to_s3(
    '/data/production.sbdb',
    'my-backups',
    'production/backup_20260107.sbbackup'
)
```

### 16.5. Point-in-Time Recovery

**Shell Script:**

```bash
#!/bin/bash
# Restore to specific point in time

BASE_BACKUP="/backups/daily/production_20260107.sbbackup"
ARCHIVE_DIR="/archives/txlogs"
TARGET_TX=123456789
RESTORE_PATH="/data/restored.sbdb"

# Perform PITR restore
scratchbird-restore \
    --backup "$BASE_BACKUP" \
    --archives "$ARCHIVE_DIR/*.sbtxlog" \
    --target-tx "$TARGET_TX" \
    --output "$RESTORE_PATH" \
    --validate

echo "Restored to transaction $TARGET_TX"
```

### 16.6. Cluster Backup

**Shell Script:**

```bash
#!/bin/bash
# Backup distributed cluster

CLUSTER_CONFIG="/etc/scratchbird/cluster.conf"
BACKUP_DIR="/backups/cluster"
DATE=$(date +%Y%m%d)

# Perform cluster backup (coordinated snapshot)
scratchbird-cluster-backup \
    --config "$CLUSTER_CONFIG" \
    --output "$BACKUP_DIR/cluster_$DATE" \
    --compression lz4 \
    --validate

# Result: creates manifest and per-shard backups
# /backups/cluster/cluster_20260107/
#   ├── manifest.json
#   ├── shard_0.sbbackup
#   ├── shard_1.sbbackup
#   └── ...

echo "Cluster backup completed"
```

---

## 17. Best Practices

### 17.1. Backup Schedule

**Recommended Schedule:**

| Frequency | Type | Retention | Use Case |
|-----------|------|-----------|----------|
| **Weekly** | Full | 4 weeks | Baseline backup |
| **Daily** | Differential | 7 days | Daily recovery point |
| **Hourly** | Incremental | 24 hours | Recent recovery |
| **Continuous** | Transaction Log | 7 days | PITR (optional) |

**Example crontab:**

```cron
# Weekly full backup (Sunday 2 AM)
0 2 * * 0 /scripts/backup-full.sh

# Daily differential backup (2 AM)
0 2 * * 1-6 /scripts/backup-differential.sh

# Hourly incremental backup
0 * * * * /scripts/backup-incremental.sh

# Continuous transaction log archiving (if PITR enabled)
*/5 * * * * /scripts/archive-txlogs.sh
```

### 17.2. Storage Best Practices

**Local Storage:**

- Use dedicated backup volume (separate from database volume)
- Enable hardware RAID for redundancy
- Use fast storage (SSD) for backup/restore performance

**Remote Storage:**

- Store backups in geographically separate location
- Use object storage (S3, GCS, Azure Blob) for durability
- Enable versioning and lifecycle policies

**Backup Retention:**

```yaml
# Example retention policy
retention:
  full:
    daily: 7    # Keep daily full backups for 7 days
    weekly: 4   # Keep weekly full backups for 4 weeks
    monthly: 12 # Keep monthly full backups for 12 months
    yearly: 7   # Keep yearly full backups for 7 years

  incremental:
    hourly: 24  # Keep hourly incremental backups for 24 hours
```

### 17.3. Security Best Practices

**Encryption:**

- **Always encrypt backups** containing sensitive data
- Use AES-256-GCM for strong authenticated encryption
- Store encryption keys in secure key management system (KMS)
- Rotate encryption keys regularly (quarterly)

**Access Control:**

- Restrict backup file permissions (600 or 640)
- Use dedicated backup user with minimal privileges
- Audit all backup/restore operations
- Implement multi-person authorization for production restores

**Validation:**

- **Always validate backups** after creation
- Perform test restores regularly (monthly)
- Store checksums separately from backups
- Use digital signatures for tamper detection

### 17.4. Performance Best Practices

**Backup Performance:**

- Run backups during low-activity periods (off-hours)
- Use compression to reduce I/O and storage costs
- Use parallel backup for large databases (multiple threads)
- Consider incremental backups for large databases (faster)

**Restore Performance:**

- Use differential backups to minimize restore chain length
- Rebuild indexes after restore (parallel index builds)
- Use parallel restore for large databases
- Consider restoring to fast storage (SSD) first

**Network Performance:**

- Use compression for network backups (reduce bandwidth)
- Tune TCP buffers for streaming backups
- Consider local backup + async upload to remote storage

### 17.5. Disaster Recovery Best Practices

**Recovery Time Objective (RTO):**

- Define acceptable downtime for each system
- Practice restores regularly to meet RTO targets
- Document restore procedures
- Automate restore process where possible

**Recovery Point Objective (RPO):**

- Define acceptable data loss for each system
- Align backup frequency with RPO requirements
- Use transaction log archiving for low RPO (< 1 hour)

**Disaster Recovery Testing:**

- Test full restore quarterly
- Test PITR restore annually
- Document and time all procedures
- Update runbooks with lessons learned

**Example DR Test Plan:**

```yaml
# Disaster Recovery Test Plan

quarterly_test:
  - name: "Full Restore Test"
    steps:
      - Select latest full backup
      - Restore to test environment
      - Verify data integrity
      - Run application smoke tests
      - Document time taken and issues
    success_criteria:
      - RTO < 4 hours
      - Zero data corruption
      - All smoke tests pass

annual_test:
  - name: "PITR Restore Test"
    steps:
      - Select base backup and archives
      - Restore to specific transaction ID
      - Verify restored state
      - Run application tests
    success_criteria:
      - RTO < 8 hours
      - Restored to exact transaction
      - All tests pass
```

### 17.6. Monitoring Best Practices

**Metrics to Monitor:**

- Backup success/failure rate
- Backup duration trends
- Backup file size trends
- Last successful backup timestamp
- Backup validation results

**Alerts to Configure:**

- Backup failed (immediate)
- Backup overdue (24 hours)
- Backup too slow (> 4 hours)
- Backup validation failed (immediate)
- Backup size anomaly (> 20% change)

**Example Monitoring Dashboard:**

```
ScratchBird Backup Dashboard
┌────────────────────────────────────────────────┐
│ Last Backup Status                             │
│ ✅ Success (2 hours ago)                       │
│ Duration: 1h 23m | Size: 45.2 GB | Ratio: 2.8x│
└────────────────────────────────────────────────┘

┌────────────────────────────────────────────────┐
│ Backup Trend (Last 7 Days)                     │
│ Success Rate: 100% (7/7)                       │
│ Avg Duration: 1h 31m                           │
│ Avg Size: 46.1 GB                              │
└────────────────────────────────────────────────┘

┌────────────────────────────────────────────────┐
│ Active Alerts                                  │
│ (none)                                         │
└────────────────────────────────────────────────┘
```

---

## 18. Testing Requirements

### 18.1. Unit Tests

**Required Coverage:**

- Backup snapshot acquisition
- Page visibility rules
- Compression algorithms
- Encryption/decryption
- Checksum verification
- File format parsing

**Example Test:**

```cpp
TEST(BackupTest, SnapshotVisibility) {
    // Setup: create pages with different xmin/xmax
    Page* page1 = create_page(/*xmin=*/100, /*xmax=*/0);
    Page* page2 = create_page(/*xmin=*/200, /*xmax=*/0);
    Page* page3 = create_page(/*xmin=*/100, /*xmax=*/150);

    BackupSnapshot snapshot;
    snapshot.backup_tx_id = 180;

    // Test visibility
    EXPECT_TRUE(is_visible_to_backup(page1, &snapshot));  // Created before, not deleted
    EXPECT_FALSE(is_visible_to_backup(page2, &snapshot)); // Created after
    EXPECT_FALSE(is_visible_to_backup(page3, &snapshot)); // Deleted before
}
```

### 18.2. Integration Tests

**Required Scenarios:**

1. **Full Backup and Restore**
   - Backup database with various data types
   - Restore to new location
   - Verify data integrity

2. **Incremental Backup Chain**
   - Full backup + 3 incremental backups
   - Restore chain
   - Verify final state

3. **Compression and Encryption**
   - Backup with LZ4, Zstd, Snappy
   - Backup with AES-256-GCM
   - Verify correct decryption and decompression

4. **Concurrent Transactions**
   - Start backup
   - Run concurrent inserts/updates/deletes
   - Verify backup consistency

5. **Error Scenarios**
   - Disk full during backup
   - Corrupted backup file
   - Missing encryption key
   - Interrupted restore

### 18.3. Performance Tests

**Benchmarks:**

- Backup throughput (GB/hour)
- Restore throughput (GB/hour)
- Compression ratios for different algorithms
- CPU overhead for compression/encryption
- Memory usage during backup/restore

**Example Benchmark:**

```cpp
BENCHMARK(BackupPerformance) {
    Database db = create_test_database(/*size=*/10_GB);
    BackupOptions opts;
    opts.compression = LZ4;

    auto start = std::chrono::steady_clock::now();
    backup_full(db, "/tmp/backup.sbbackup", opts);
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    double throughput_mbps = (10.0 * 1024) / elapsed;

    EXPECT_GT(throughput_mbps, 100.0);  // > 100 MB/sec
}
```

### 18.4. Compliance Tests

**Required Tests:**

- Backup file format compliance
- Transaction marker preservation
- Catalog integrity after restore
- MGA visibility rules
- Encryption strength (FIPS 140-2)
- Audit log completeness

---

## Appendix A: File Format Version History

| Version | Date | Changes |
|---------|------|---------|
| **1.0** | 2024-06 | Initial backup format |
| **2.0** | 2026-01 | Added cluster support, digital signatures, page index |

---

## Appendix B: Glossary

| Term | Definition |
|------|------------|
| **MGA** | Multi-Generational Architecture (Firebird-style MVCC) |
| **OIT** | Oldest Interesting Transaction |
| **OAT** | Oldest Active Transaction |
| **OST** | Oldest Snapshot Transaction |
| **NEXT** | Next Transaction ID to be assigned |
| **LSN** | Log Sequence Number (page modification counter) |
| **PITR** | Point-In-Time Recovery |
| **MVCC** | Multi-Version Concurrency Control |
| **Sweep** | Garbage collection process in MGA |
| **xmin** | Transaction ID that created a tuple |
| **xmax** | Transaction ID that deleted a tuple |
| **BEK** | Backup Encryption Key |
| **RF** | Replication Factor |

---

## Appendix C: References

- [Firebird MGA Architecture](../../MGA_RULES.md)
- [ScratchBird Security Specification](Security Design Specification/00_SECURITY_SPEC_INDEX.md)
- [ScratchBird Cluster Specification](Cluster Specification Work/SBCLUSTER-SUMMARY.md)
- [Transaction Model Specification](sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md)
- [PostgreSQL Backup and Recovery](https://www.postgresql.org/docs/current/backup.html)
- [Firebird Backup and Restore](https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref40/firebird-40-language-reference.html#fblangref40-management-backup)

---

**Document Status:** ✅ Complete
**Last Review:** 2026-01-07
**Next Review:** 2026-04-07 (Quarterly)
**Owner:** ScratchBird Core Team
