# Heap-TOAST Integration


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)



## IMPLEMENTATION STATUS: ⚠️ SPECIFICATION (Authoritative for Alpha)

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional optional extension (replication/PITR).
Any WAL references in this document describe an optional optional extension stream for
replication/PITR only.

This document defines the **required behavior**. Implementation status must be validated against this spec; legacy claims of completeness are non-authoritative.

**Key Implementation Files**:
- `include/scratchbird/core/toast.h` - TOAST chunk format, ToastVisibility class
- `src/core/toast.cpp` - ToastManager, TIP-based visibility
- `src/core/index_key_extractor.cpp` - Index detoasting (Alpha)
- `src/core/garbage_collector.cpp` - TOAST GC (Alpha)
- `src/core/vacuum.cpp` - TOAST table processing (Alpha)

## Overview

The Heap-TOAST integration provides automatic handling of large attributes during tuple operations. When tuples contain data that exceeds the TOAST threshold (PostgreSQL-compatible) or would not fit comfortably in a page, the HeapPage class automatically TOASTs the **attribute values** during insertion and detoasts them during retrieval.

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

1. **Per-Attribute Check**: Each varlen attribute is checked against the TOAST threshold
2. **TOAST Creation**: Attributes exceeding the threshold are replaced by a TOAST pointer
3. **Tuple Fallback**: If the row still exceeds page limits, tuple-level TOAST may be used as a fallback
4. **Transparent Storage**: TOAST pointers are stored in the tuple payload like any other value

```cpp
Status insert_tuple(const uint8_t* tuple_data, uint32_t tuple_size,
                   uint64_t transaction_id, uint16_t* item_id_out,
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
                          uint64_t transaction_id, ErrorContext* ctx = nullptr);
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
// Create large data (> 2048 bytes)
std::vector<uint8_t> large_data(5000);
// ... fill with data ...

// Insert - will automatically TOAST if needed
uint16_t item_id;
Status s = heap_page.insert_tuple(large_data.data(), 
                                 large_data.size() + sizeof(SBRecordHeader),
                                 transaction_id, &item_id);
```

### Retrieving Toasted Data

```cpp
// Option 1: Get raw data (with TOAST pointer)
const uint8_t* raw_data;
uint32_t raw_size;
heap_page.get_tuple(item_id, &raw_data, &raw_size);
// raw_size will be sizeof(SBRecordHeader) + sizeof(ToastPointer) if toasted

// Option 2: Get detoasted data
std::vector<uint8_t> detoasted_buffer;
heap_page.get_tuple_detoasted(item_id, &detoasted_buffer, transaction_id);
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

### TOAST Threshold (PostgreSQL-Compatible)
- Values > 2048 bytes are candidates for TOASTing
- Per-attribute TOAST is applied first
- Tuple-level TOAST is a fallback only if the row still exceeds page limits

### TOAST Pointer Storage
When a value is toasted, the heap tuple stores:
- SBRecordHeader (record header)
- ToastPointer (42 bytes)
  - va_header: 0x01 (TOAST marker)
  - va_tag: Strategy (EXTENDED/EXTERNAL)
  - va_rawsize: Original size
  - va_extsize: Stored size (may be compressed)
  - va_valueid: TOAST value UUID (16 bytes)
  - va_toastrelid: TOAST table UUID (16 bytes)

### Backwards Compatibility
HeapPages created without TOAST support continue to work normally. Only pages created with the TOAST-enabled constructor will perform automatic TOASTing.

## Testing

Testing requirements are defined in the TOAST/LOB storage specification and must cover per-attribute TOAST, tuple-level fallback, MGA visibility, and GC behavior.

## Future Enhancements

1. **Inline Compression**: Support COMPRESSED strategy for in-page compression
2. **Partial Detoasting**: Retrieve only portions of toasted values
3. **TOAST Prefetching**: Predictive loading of TOAST chunks
4. **Alternative Storage**: Support for external storage backendsrt for external storage backends
