# ScratchBird Design Limits and Maximum Sizes

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## Database Size Limits

### Maximum Database Size by Page Size

The theoretical maximum database size is determined by:
- Page ID: 32-bit unsigned integer (4,294,967,295 pages max)
- Page Size: Configurable (8KB, 16KB, 32KB, 64KB, 128KB)
- **V3**: Supports 8KB through 128KB page sizes.

| Page Size | Maximum Pages | Maximum Database Size |
|-----------|---------------|----------------------|
| 8 KB      | 4,294,967,295 | ~32 TB              |
| 16 KB     | 4,294,967,295 | ~64 TB              |
| 32 KB     | 4,294,967,295 | ~128 TB             |
| 64 KB     | 4,294,967,295 | ~256 TB             |
| 128 KB    | 4,294,967,295 | ~512 TB             |

### Practical Limits

While the theoretical limits are large, practical limits include:
- File system maximum file size
- Available disk space
- Performance considerations for large bitmaps

## Page-Level Limits

### Page Header Size
- Fixed: 64 bytes
- Cannot be changed without breaking compatibility

### Usable Space Per Page
| Page Size | Header Size | Usable Space |
|-----------|-------------|--------------|
| 8 KB      | 64 bytes    | 8,128 bytes  |
| 16 KB     | 64 bytes    | 16,320 bytes |
| 32 KB     | 64 bytes    | 32,704 bytes |
| 64 KB     | 64 bytes    | 65,472 bytes |
| 128 KB    | 64 bytes    | 131,008 bytes |

## Free Space Map (FSM) Limits

### Bitmap Capacity
- 1 bit per page for allocation tracking
- FSM page overhead: PageHeader (64) + metadata (12) = 76 bytes

| Page Size | Bitmap Capacity | Pages Trackable |
|-----------|-----------------|-----------------|
| 8 KB      | 8,116 bytes     | 64,928 pages    |
| 16 KB     | 16,308 bytes    | 130,464 pages   |
| 32 KB     | 32,692 bytes    | 261,536 pages   |
| 64 KB     | 65,460 bytes    | 523,680 pages   |
| 128 KB    | 131,004 bytes   | 1,048,032 pages |

### FSM Chaining (Authoritative)
- FSM chaining is REQUIRED in V3.
- Each FSM page contains `next_fsm_page_id` (uint32) and `prev_fsm_page_id` (uint32) in its header.
- Allocation:
  1. When the current FSM cannot represent the next page range, allocate a new FSM page.
  2. Link `prev_fsm_page_id` and `next_fsm_page_id` bidirectionally.
  3. Update the FSM root pointer in the catalog root page.
- Lookup:
  - FSM page selection is computed by page_id range: `fsm_index = page_id / pages_per_fsm`.
  - Walk the chain until the corresponding FSM range is found.

## Buffer Pool Limits

### Memory Usage
| Configuration | Pages | Memory Usage (16KB pages) |
|---------------|-------|---------------------------|
| Minimum       | 32    | 512 KB                    |
| Default       | 32    | 512 KB                    |
| Large         | 1024  | 16 MB                     |
| Very Large    | 65536 | 1 GB                      |

### Pin Count Limits
- Maximum concurrent pins: Buffer pool size
- Pin count per page: 64-bit unsigned (no overflow in realistic workloads)
- Overflow handling: if pin count exceeds `UINT64_MAX` (theoretical only), raise `ERR_BUFFER_PIN_OVERFLOW`.

## Table and Column Limits

### Maximum Columns per Table

**Hard Limit**: 4,096 columns (MySQL-compatible)
**Practical Limit**: 1,024 columns (SQL Server-compatible)
**Recommended**: < 200 columns for optimal performance

The actual maximum depends on:
- Page size in use
- Column data types
- Null bitmap size (1 bit per column)
- Tuple header overhead (36 bytes)

**Theoretical maximums for 16KB pages** (most common):
| Column Type | Theoretical Max Columns |
|-------------|------------------------|
| BOOLEAN/INT8 | ~14,000 |
| INT16 | ~7,600 |
| INT32 | ~3,900 |
| INT64 | ~2,000 |
| UUID | ~1,000 |
| VARCHAR(1) | ~3,100 |

### Maximum Record Size

The maximum record (tuple) size is constrained by:
- Page size
- Tuple header: 36 bytes
- Item pointer: 8 bytes
- Null bitmap: (num_columns + 7) / 8 bytes

**Maximum single tuple sizes** (absolute maximum, not recommended):
| Page Size | Max Tuple Size | Recommended Max |
|-----------|---------------|-----------------|
| 8 KB | 8,084 bytes | 768 bytes |
| 16 KB | 16,276 bytes | 1,588 bytes |
| 32 KB | 32,660 bytes | 3,226 bytes |
| 64 KB | 65,428 bytes | 6,503 bytes |
| 128 KB | 130,964 bytes | 13,056 bytes |

**TOAST Support**: Values exceeding threshold are automatically moved to TOAST storage, allowing:
- TEXT/VARCHAR: Up to **4 GB** (theoretical), **1 GB recommended** (PostgreSQL-compatible)
- BLOB/BYTEA: Up to **4 GB** (theoretical), **1 GB recommended** (PostgreSQL-compatible)
- JSON/JSONB: Up to **4 GB** (theoretical), **1 GB recommended** (PostgreSQL-compatible)
- VECTOR: Up to **4 GB** (theoretical), **1 GB recommended** (PostgreSQL-compatible)

**TOAST Technical Details**:
- Chunk size: 1,996 bytes per chunk
- Size field: `uint32_t` (4,294,967,295 bytes max = ~4 GB)
- Theoretical maximum: ~4 GB per value
- Practical limit: **1 GB per value** (for PostgreSQL compatibility)
- Compression: Automatic LZ4 compression for compressible data
- Storage strategy: Automatically chosen based on value size and compressibility

**SQL Server Compatibility**:
- In-row limit: 8,060 bytes (similar to SQL Server)
- Overflow data automatically moved to TOAST (like SQL Server's ROW_OVERFLOW_DATA)

## System Catalog Limits

### Maximum Objects per Type

**Updated for SQL Standard Compliance (128-character identifiers)**:

| Object Type | Records per 16KB Page | Practical Limit |
|-------------|----------------------|-----------------|
| Schemas | 57 records | Thousands |
| Tables per schema | 87 records | Thousands |
| Columns per table | 54 records | 1,024 (recommended) |
| Indexes per table | 37 records | Hundreds |

**Catalog Capacity Analysis**:
- Schema names: 128 characters (SQL standard compliant)
- Table names: 128 characters (SQL standard compliant)
- Column names: 128 characters (SQL standard compliant)
- Index names: 128 characters (SQL standard compliant)

## Data Type Limits

### String/Binary Length
- Maximum length: Page size - header - overhead
- Practical maximum: ~32KB for 32KB pages

### Numeric Precision
- Defined in `types/DATA_TYPE_PERSISTENCE_AND_CASTS.md` and `types/03_TYPES_AND_DOMAINS.md`.
- DECIMAL/NUMERIC precision and scale are enforced at parse time and on assignment.

## Transaction Limits

### Write-after Log (WAL)
- WAL is not used in V3 for recovery.
- Replication/PITR, if enabled, uses a separate logical change stream.

### Concurrent Transactions
- No hard limit beyond lock table sizing and memory.
- Minimum supported concurrent transactions: 1,024.
- Transaction ID space is 64-bit and does not wrap in practical lifetimes.

## Performance Considerations

### When Approaching Limits

1. **Large Databases (>1TB)**
   - FSM scan time increases
   - Consider partitioning strategies

2. **Many Small Pages**
   - Higher overhead ratio
   - Consider larger page size

3. **Buffer Pool Exhaustion**
   - Increase buffer pool size
   - Implement better eviction policies

## Recommendations

### Choosing Page Size (Operational Guidance)

- **8 KB**: Good for OLTP, many small records
- **16 KB**: Balanced choice (default)
- **32 KB**: Good for OLAP, large records
- **64 KB**: Good for very large records, data warehousing
- **128 KB**: Good for massive records, archival storage

### Monitoring Limits

Key metrics to monitor:
1. Database file size vs filesystem limits
2. Free pages in FSM
3. Buffer pool hit rate
4. Pin count high water mark

### Non-Goals (V3)
The following capabilities are explicitly out of scope for V3 and MUST NOT be
assumed by implementations:
1. 64-bit page IDs
2. Partial page writes

## Error Handling at Limits

When limits are reached:
- `Status::InvalidArgument`: Page size out of range
- `Status::OOM`: Buffer pool or memory exhausted
- `Status::IoError`: File system limits reached
- `Status::LimitExceeded`: Specific limit-reached error codes

## Summary

ScratchBird V3 defines explicit limits suitable for most applications. The 32-bit
page ID provides up to 128TB databases with 32KB pages.
