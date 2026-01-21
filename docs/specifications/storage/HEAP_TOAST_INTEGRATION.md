# Heap-TOAST Integration

## IMPLEMENTATION STATUS: 🟢 FULLY IMPLEMENTED - MGA COMPLIANT (Updated 2025-11-03)

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.

**All TOAST functionality is now complete and MGA-compliant:**
- ✅ **MGA Compliance**: TupleHeader-based TOAST chunks with xmin/xmax (Alpha)
- ✅ **TIP-Based Visibility**: Uses TIP for transaction state, not snapshots (Alpha)
- ✅ **Index Integration**: IndexKeyExtractor detoasts before indexing (Alpha)
- ✅ **Garbage Collection**: 3-stage GC (orphan detection, cleanup, TIP-based) (Alpha)
- ✅ **Comprehensive Testing**: 8 test files, 43+ test cases (Alpha)
- ✅ Value ID recovery on database reopen (fixes data corruption risk)
- ✅ Chunk cleanup on partial write failures (fixes storage leaks)
- ✅ B-tree index scan for efficient deletion (O(log n) instead of O(n))
- ✅ All TOAST strategies working (PLAIN, EXTENDED, EXTERNAL)
- ✅ LZ4 compression support
- ✅ Automatic chunking and reassembly
- ✅ Integration with heap storage

**MGA Compliance Achievements (November 2025)**:
- Chunk format: TupleHeader + value_id (4) + chunk_seq (4) + chunk_size (4)
- TIP-based visibility: `ToastVisibility::isChunkVisible()` uses TIP, not snapshots
- Crash recovery: TIP state recovery, NO write-after log (WAL, optional post-gold) replay
- Garbage collection: Sweep/GC processes TOAST tables with 3-phase GC
- **MGA Scorecard**: 6/6 (100%) - FULL MGA COMPLIANCE

**Key Implementation Files**:
- `include/scratchbird/core/toast.h` - TOAST chunk format, ToastVisibility class
- `src/core/toast.cpp` - ToastManager, TIP-based visibility
- `src/core/index_key_extractor.cpp` - Index detoasting (Alpha)
- `src/core/garbage_collector.cpp` - TOAST GC (Alpha)
- `src/core/vacuum.cpp` - TOAST table processing (Alpha)

## Overview

The Heap-TOAST integration provides automatic handling of large attributes during tuple operations. When tuples contain data that exceeds the TOAST threshold (2KB) or would not fit comfortably in a page, the HeapPage class automatically TOASTs the data during insertion and detoasts it during retrieval.

## Architecture

### HeapPage with TOAST Support

The `HeapPage` class has been extended with an optional constructor that accepts:
- `ToastManager*`: For handling TOAST operations
- `Database*`: For database operations
- `UuidV7Bytes table_id`: To identify the table for TOAST storage

```cpp
// Standard constructor (no TOAST support)
HeapPage(uint8_t* page_data, uint32_t page_size);

// Constructor with TOAST support
HeapPage(uint8_t* page_data, uint32_t page_size, 
         ToastManager* toast_mgr, Database* db, UuidV7Bytes table_id);
```

### Automatic TOASTing During Insert

When `insert_tuple()` is called with TOAST support enabled:

1. **Size Check**: The method checks if the tuple size exceeds the TOAST threshold
2. **TOAST Creation**: If needed, it creates a TOAST pointer instead of storing the full data
3. **Space Optimization**: The toasted tuple (header + TOAST pointer) takes only ~34 bytes
4. **Transparent Storage**: The TOAST pointer is stored in the page like any other tuple

```cpp
Status insert_tuple(const uint8_t* tuple_data, uint32_t tuple_size,
                   uint64_t xmin, uint16_t* item_id_out,
                   ErrorContext* ctx = nullptr);
```

### Automatic Detoasting During Retrieval

Two methods are available for tuple retrieval:

1. **get_tuple()**: Returns the raw tuple data (may contain TOAST pointer)
2. **get_tuple_detoasted()**: Automatically detoasts and returns the full data

```cpp
// Get raw tuple (may contain TOAST pointer)
Status get_tuple(uint16_t item_id, const uint8_t** data_out,
                uint32_t* size_out, ErrorContext* ctx = nullptr);

// Get detoasted tuple (full data)
Status get_tuple_detoasted(uint16_t item_id, std::vector<uint8_t>* buffer,
                          uint64_t xmin, ErrorContext* ctx = nullptr);
```

### Automatic Cleanup During Delete

When `delete_tuple()` is called:
1. Checks if the tuple contains a TOAST pointer
2. If found, deletes the associated TOAST chunks
3. Marks the tuple as deleted in the heap page

## Usage Examples

### Creating a HeapPage with TOAST Support

```cpp
// Assume we have initialized database, buffer pool, etc.
// UuidV7Generator uuid_gen; // Assume uuid_gen is a UuidV7Generator instance
UuidV7Bytes table_id = generate_uuid_v7(); // Use a generated UUID
ToastManager toast_mgr(db, table_id);
std::vector<uint8_t> page_buffer(PAGE_SIZE);

// Create heap page with TOAST support
HeapPage heap_page(page_buffer.data(), PAGE_SIZE, &toast_mgr, db, table_id);
heap_page.initialize(page_id);
```

### Inserting Large Data

```cpp
// Create large data (> 2KB)
std::vector<uint8_t> large_data(5000);
// ... fill with data ...

// Insert - will automatically TOAST if needed
uint16_t item_id;
Status s = heap_page.insert_tuple(large_data.data(), 
                                 large_data.size() + sizeof(TupleHeader),
                                 xmin, &item_id);
```

### Retrieving Toasted Data

```cpp
// Option 1: Get raw data (with TOAST pointer)
const uint8_t* raw_data;
uint32_t raw_size;
heap_page.get_tuple(item_id, &raw_data, &raw_size);
// raw_size will be sizeof(TupleHeader) + sizeof(ToastPointer) if toasted

// Option 2: Get detoasted data
std::vector<uint8_t> detoasted_buffer;
heap_page.get_tuple_detoasted(item_id, &detoasted_buffer, xmin);
// detoasted_buffer contains the full original data
```

## Performance Considerations

### Benefits
1. **Space Efficiency**: Large tuples don\'t consume entire pages
2. **More Tuples per Page**: Toasted tuples are tiny (~34 bytes)
3. **Selective Detoasting**: Only detoast when actually needed
4. **Compression**: TOAST can compress data for additional savings

### Trade-offs
1. **Extra I/O**: Detoasting requires reading TOAST chunks
2. **Memory Usage**: Detoasting creates temporary buffers
3. **CPU Overhead**: Compression/decompression if used

## Best Practices

1. **Use get_tuple() when possible**: If you don\'t need the full data, avoid detoasting
2. **Batch Operations**: When scanning, consider if you really need all large attributes
3. **Monitor TOAST Tables**: TOAST tables can grow large and may need maintenance
4. **Consider Page Size**: Larger pages reduce the need for TOASTing

## Implementation Details

### TOAST Threshold
- Values > 2000 bytes are candidates for TOASTing
- Values > page_size/4 must be TOASTed
- Small values are never toasted (overhead not worth it)

### TOAST Pointer Storage
When a value is toasted, the heap tuple stores:
- TupleHeader (16 bytes)
- ToastPointer (18 bytes)
  - va_header: 0x01 (TOAST marker)
  - va_tag: Strategy (EXTENDED/EXTERNAL)
  - va_rawsize: Original size
  - va_extsize: Stored size (may be compressed)
  - va_valueid: Unique ID for TOAST chunks
  - va_toastrelid: TOAST table ID

### Backwards Compatibility
HeapPages created without TOAST support continue to work normally. Only pages created with the TOAST-enabled constructor will perform automatic TOASTing.

## Testing

Comprehensive tests are provided in `test_heap_toast_integration.cpp`:
- Basic TOAST insert and retrieve
- Small tuples (no TOAST)
- TOAST deletion and cleanup
- Multiple toasted tuples
- Compressed TOAST data
- HeapPage without TOAST manager

## Future Enhancements

1. **Inline Compression**: Support COMPRESSED strategy for in-page compression
2. **Partial Detoasting**: Retrieve only portions of toasted values
3. **TOAST Prefetching**: Predictive loading of TOAST chunks
4. **Alternative Storage**: Support for external storage backendsrt for external storage backends
