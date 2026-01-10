# ScratchBird Design Limits and Maximum Sizes

## Database Size Limits

### Maximum Database Size by Page Size

The theoretical maximum database size is determined by:
- Page ID: 32-bit unsigned integer (4,294,967,295 pages max)
- Page Size: Configurable (8KB, 16KB, 32KB, 64KB, 128KB)
- **Stage 1.1 (September 2025)**: Added support for 64KB and 128KB page sizes

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

### FSM Chaining
- Currently not implemented (Alpha)
- Single FSM page limits initial database size
- Future: FSM chaining for larger databases

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
- Pin count per page: 32-bit unsigned (4,294,967,295)
- Overflow protection: Not implemented (would require uint64_t)

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
- To be defined in Beta phase
- Currently no numeric type implementation

## Transaction Limits

### Write-after Log (WAL, optional)
- Not implemented in Alpha
- Future: 64-bit LSN (practically unlimited) for replication/PITR streams

### Concurrent Transactions
- Alpha: Single-threaded, one transaction
- Future: Limited by memory and lock table size

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

### Choosing Page Size

- **8 KB**: Good for OLTP, many small records
- **16 KB**: Balanced choice (default)
- **32 KB**: Good for OLAP, large records
- **64 KB**: Good for very large records, data warehousing (Stage 1.1+)
- **128 KB**: Good for massive records, archival storage (Stage 1.1+)

### Monitoring Limits

Key metrics to monitor:
1. Database file size vs filesystem limits
2. Free pages in FSM
3. Buffer pool hit rate
4. Pin count high water mark

### Future Enhancements

Planned improvements for Beta/Production:
1. FSM chaining for unlimited pages
2. 64-bit page IDs (optional)
3. Compressed pages
4. Partial page writes

## Error Handling at Limits

When limits are reached:
- `Status::InvalidArgument`: Page size out of range
- `Status::OOM`: Buffer pool or memory exhausted
- `Status::IoError`: File system limits reached
- Future: Specific limit-reached error codes

## Summary

ScratchBird Alpha has generous limits suitable for most applications. The 32-bit page ID provides up to 128TB databases with 32KB pages. The architecture allows for future expansion when needed.
