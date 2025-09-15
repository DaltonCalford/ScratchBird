# TOAST/LOB Storage Implementation

## Overview

TOAST (The Oversized-Attribute Storage Technique) is a mechanism for storing large values that exceed the normal tuple size limits. It allows ScratchBird to handle large binary objects (LOBs), text, and other oversized data by storing them out-of-line in a separate TOAST table.

## Architecture

### Key Components

1. **ToastManager** - Manages TOAST operations for a table
2. **ToastPointer** - Stored in main tuple, points to TOAST data
3. **ToastChunk** - Individual pieces of large values
4. **TOAST Tables** - Special tables storing chunked data

### Design Principles

- **Transparency**: Applications see full values, not pointers
- **Efficiency**: Only TOAST values above threshold
- **Flexibility**: Multiple storage strategies
- **Chunking**: Large values split into manageable pieces

## Storage Strategies

| Strategy | Description | When Used |
|----------|-------------|-----------|
| PLAIN | Store inline (no TOAST) | Small values < 2KB |
| EXTENDED | Out-of-line, uncompressed | Medium values or incompressible |
| COMPRESSED | Inline, compressed | Not implemented (future) |
| EXTERNAL | Out-of-line, compressed | Large compressible values |

## Implementation Details

### TOAST Threshold

```cpp
constexpr uint32_t TOAST_TUPLE_THRESHOLD = 2000;  // 2KB
constexpr uint32_t TOAST_MAX_CHUNK_SIZE = 1996;   // ~2KB chunks
```

Values larger than `TOAST_TUPLE_THRESHOLD` or 1/4 of the page size are candidates for TOASTing.

### TOAST Pointer Structure

```cpp
struct ToastPointer {
    uint8_t  va_header;      // 0x01 = TOAST marker
    uint8_t  va_tag;         // Strategy type
    uint32_t va_rawsize;     // Original size
    uint32_t va_extsize;     // Stored size
    uint32_t va_valueid;     // Unique ID
    uint32_t va_toastrelid;  // TOAST table ID
};
```

### TOAST Table Schema

Each regular table can have an associated TOAST table named `pg_toast_<table_id>` with columns:
- `chunk_id` (INT) - TOAST value ID
- `chunk_seq` (INT) - Chunk sequence number
- `chunk_data` (BYTEA) - Actual chunk data

### Chunking Process

1. Large values are assigned a unique value ID
2. Data is split into chunks of max `TOAST_MAX_CHUNK_SIZE`
3. Each chunk is stored as a tuple in the TOAST table
4. Chunks are numbered sequentially (0-based)

## Operations

### TOASTing a Value

```cpp
ToastManager toast_mgr(db, table_id);
toast_mgr.initialize();  // Creates TOAST table if needed

ToastPointer pointer;
Status status = toast_mgr.toast_value(
    data, size,              // Value to TOAST
    ToastStrategy::EXTENDED, // Strategy
    xmin,                    // Transaction ID
    &pointer                 // Output pointer
);
```

### Detoasting a Value

```cpp
std::vector<uint8_t> data;
Status status = toast_mgr.detoast_value(
    &pointer,    // TOAST pointer
    &data,       // Output buffer
    xmin         // Transaction ID
);
```

### Deleting TOAST Values

```cpp
Status status = toast_mgr.delete_toast_value(
    value_id,    // TOAST value ID
    xmax         // Deleting transaction
);
```

## Compression Integration

When `ToastStrategy::EXTERNAL` is used:
1. Data is compressed using the pluggable compression framework
2. If compression saves >10%, compressed data is stored
3. Otherwise, falls back to uncompressed storage
4. Detoasting automatically handles decompression

## Performance Characteristics

### Space Efficiency
- Reduces main table size
- Allows tuples with very large attributes
- Compression can save 50-90% for text data

### Access Patterns
- Small values: No overhead
- TOASTed values: Extra I/O for retrieval
- Sequential scan: Can skip TOAST retrieval if column not needed

### Chunk Size Trade-offs
- Smaller chunks: More flexible, more overhead
- Larger chunks: Less overhead, may waste space
- Current: ~2KB balances both concerns

## Testing

### Unit Tests
1. **BasicToastOperations** - Simple TOAST/detoast cycle
2. **CompressedToast** - Compression with TOAST
3. **MultipleChunks** - Values spanning multiple chunks
4. **ToastDelete** - Deletion handling
5. **StrategySelection** - Automatic strategy choice
6. **EdgeCases** - Boundary conditions

### Test Coverage
- ✅ All page sizes (8KB - 128KB)
- ✅ Values from 0 to 100+ chunks
- ✅ Compressed and uncompressed
- ✅ Strategy selection logic
- ✅ Error handling

## Future Enhancements

1. **Inline Compression** (COMPRESSED strategy)
   - Compress small-medium values inline
   - Avoid TOAST table overhead

2. **Partial Detoasting**
   - Retrieve only needed chunks
   - Substring operations without full detoast

3. **TOAST Indexes**
   - Index on (chunk_id, chunk_seq)
   - Faster chunk retrieval

4. **Shared TOAST Tables**
   - Multiple tables share TOAST storage
   - Better space utilization

5. **Alternative Chunk Sizes**
   - Configurable per table
   - Optimize for workload

## Integration Notes

### With Storage Engine
- TOAST tables are regular tables
- Use standard tuple operations
- Subject to same MVCC rules

### With Catalog Manager
- TOAST tables created in same schema
- Named systematically: `pg_toast_<UUID>`
- Tracked in system catalog

### With Buffer Pool
- TOAST chunks cached like regular pages
- LRU eviction applies
- No special handling needed

## Best Practices

1. **Schema Design**
   - Use appropriate data types
   - Consider TOAST overhead in capacity planning
   - Monitor TOAST table growth

2. **Application Development**
   - Minimize large value updates
   - Use streaming APIs when available
   - Consider client-side compression

3. **Performance Tuning**
   - Monitor TOAST fetch frequency
   - Adjust thresholds if needed
   - Consider storage strategy per column

## Status

⚠️ **PARTIALLY IMPLEMENTED** - The TOAST/LOB storage framework is implemented but requires integration with the main tuple storage system. The following components are complete:

✅ Core TOAST structures and interfaces
✅ Chunking and reassembly logic
✅ Compression integration
✅ TOAST table creation
✅ Basic operations (toast/detoast/delete)

The following work remains:
- Integration with HeapPage for automatic TOASTing
- Modification of insert_tuple to handle TOAST pointers
- Update of get_tuple to automatically detoast
- Transaction visibility for TOAST values

Once these integrations are complete, TOAST will be fully functional for Stage 1.1.