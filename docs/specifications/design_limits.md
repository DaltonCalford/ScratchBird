# ScratchBird Design Limits and Maximum Sizes

## Database Size Limits

### Maximum Database Size by Page Size

The theoretical maximum database size is determined by:
- Page ID: 32-bit unsigned integer (4,294,967,295 pages max)
- Page Size: Configurable (8KB, 16KB, 32KB for Alpha)

| Page Size | Maximum Pages | Maximum Database Size |
|-----------|---------------|----------------------|
| 8 KB      | 4,294,967,295 | ~32 TB              |
| 16 KB     | 4,294,967,295 | ~64 TB              |
| 32 KB     | 4,294,967,295 | ~128 TB             |

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

## Free Space Map (FSM) Limits

### Bitmap Capacity
- 1 bit per page for allocation tracking
- FSM page overhead: PageHeader (64) + metadata (12) = 76 bytes

| Page Size | Bitmap Capacity | Pages Trackable |
|-----------|-----------------|-----------------|
| 8 KB      | 8,116 bytes     | 64,928 pages    |
| 16 KB     | 16,308 bytes    | 130,464 pages   |
| 32 KB     | 32,692 bytes    | 261,536 pages   |

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

## System Catalog Limits

### Maximum Objects per Type
- Schemas: Limited by page size and catalog structure
- Tables per schema: ~1000s (depending on name length)
- Columns per table: ~100s (depending on definitions)

## Data Type Limits

### String/Binary Length
- Maximum length: Page size - header - overhead
- Practical maximum: ~32KB for 32KB pages

### Numeric Precision
- To be defined in Beta phase
- Currently no numeric type implementation

## Transaction Limits

### Write-Ahead Log (WAL)
- Not implemented in Alpha
- Future: 64-bit LSN (practically unlimited)

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