# Specification: TOAST Storage

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird Alpha |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/toast.h:30`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/toast.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_toast.cpp`

## Synopsis

This specification defines TOAST (The Oversized-Attribute Storage Technique) used for storing large column values. TOAST compresses and/or moves large values out-of-line to separate pages while maintaining MGA visibility semantics.

## Scope

### In Scope

- TOAST threshold and strategy selection
- TOAST pointer format
- Chunk storage format
- Compression/decompression
- MGA-compliant visibility for chunks
- Prefetching for sequential chunk access

### Out of Scope

- LOB (Large Object) storage (separate specification)
- External table storage
- Full-text TOAST handling

## Background

TOAST addresses the limitation that rows must fit on a single page:

1. **Threshold-based**: Values larger than threshold are TOAST candidates
2. **Strategies**: PLAIN, EXTENDED, EXTERNAL, COMPRESSED
3. **Chunking**: Large values split into page-sized chunks
4. **Compression**: Optional LZ4 compression
5. **MGA Compliance**: Chunks have xmin/xmax for visibility

## Specification

### Data Structures

#### ToastSettings (Dynamic based on page size)

```cpp
// From include/scratchbird/core/toast.h:57
namespace ToastSettings {
    constexpr uint32_t THRESHOLD_DIVISOR = 32;   // page_size / 32
    constexpr uint32_t CHUNK_DIVISOR = 4;        // page_size / 4
    constexpr uint32_t TARGET_DIVISOR = 16;      // page_size / 16
    constexpr uint32_t HEADER_SIZE = sizeof(TupleHeader) + 24;

    inline uint32_t getThreshold(uint32_t page_size) {
        return page_size / THRESHOLD_DIVISOR;
    }

    inline uint32_t getMaxChunkSize(uint32_t page_size) {
        return (page_size / CHUNK_DIVISOR) - HEADER_SIZE;
    }

    inline uint32_t getTarget(uint32_t page_size) {
        return page_size / TARGET_DIVISOR;
    }
}
```

#### ToastStrategy Enum

```cpp
// From include/scratchbird/core/toast.h:138
enum class ToastStrategy : uint8_t {
    PLAIN = 0,      // Store inline (no TOAST)
    EXTENDED = 1,   // Out-of-line, uncompressed
    COMPRESSED = 2, // Inline, compressed (future)
    EXTERNAL = 3,   // Out-of-line, compressed
};
```

#### ToastPointer (32 bytes)

```cpp
// From include/scratchbird/core/toast.h:158
#pragma pack(push, 1)
struct ToastPointer {
    ID lob_uuid;              // 16 bytes: Unique value ID
    uint64_t total_len;       // 8 bytes: Uncompressed length
    uint32_t chunk_size;      // 4 bytes: Size per chunk
    uint16_t compression;     // 2 bytes: Compression algorithm
    uint16_t flags;           // 2 bytes: Flags

    static constexpr uint16_t TOAST_COMPRESSED = 0x0001;
    static constexpr uint16_t TOAST_ENCRYPTED = 0x0002;
    static constexpr uint16_t TOAST_INLINE_REF = 0x0004;
};
#pragma pack(pop)
static_assert(sizeof(ToastPointer) == 32, "ToastPointer must be 32 bytes");
```

#### ToastChunk Structure

```cpp
// From include/scratchbird/core/toast.h:195
#pragma pack(push, 1)
struct ToastChunk {
    TupleHeader header;       // 80 bytes: MGA header
    
    // TOAST Metadata (24 bytes)
    ID chunk_id;              // 16 bytes: Owner value ID
    uint32_t chunk_seq;       // 4 bytes: Sequence number
    uint32_t chunk_size;      // 4 bytes: Data bytes in this chunk
    
    // Data (variable, up to TOAST_MAX_CHUNK_SIZE)
    uint8_t chunk_data[TOAST_MAX_CHUNK_SIZE];
};
#pragma pack(pop)
```

### ToastSettings by Page Size

| Page Size | Threshold | Max Chunk Size | Target |
|-----------|-----------|----------------|--------|
| 8 KB | 256 B | 2,020 B | 512 B |
| 16 KB | 512 B | 4,068 B | 1,024 B |
| 32 KB | 1,024 B | 8,164 B | 2,048 B |
| 64 KB | 2,048 B | 16,356 B | 4,096 B |
| 128 KB | 4,096 B | 32,740 B | 8,192 B |

### Interface Contracts

#### Function: `shouldToast()`

```cpp
// From include/scratchbird/core/toast.h:314
static bool shouldToast(uint32_t size, uint32_t page_size);
```

**Algorithm:**
```
1. threshold = ToastSettings::getThreshold(page_size)
2. max_inline = page_size / 4  // Conservative: 25% of page
3. RETURN (size > threshold) OR (size > max_inline)
```

#### Function: `chooseStrategy()`

```cpp
// From include/scratchbird/core/toast.h:265
static ToastStrategy chooseStrategy(
    const uint8_t *data,
    uint32_t size,
    uint32_t page_size,
    bool compress_enabled = true
);
```

**Algorithm:**
```
1. IF NOT compress_enabled:
2.     RETURN EXTENDED

3. // Try compression
4. compressed_size = lz4CompressBound(size)
5. target_size = ToastSettings::getTarget(page_size)
6. 
7. IF compressed_size < target_size:
8.     RETURN EXTERNAL  // Compresses well, use compressed out-of-line
9. ELSE:
10.    RETURN EXTENDED  // Incompressible, use uncompressed out-of-line
```

#### Function: `toastValue()`

```cpp
// Source: src/core/toast.cpp
Status ToastManager::toastValue(
    const uint8_t *data,       // Data to TOAST
    uint32_t size,             // Data size
    ToastStrategy strategy,    // Storage strategy
    uint64_t xmin,             // Creating transaction
    ToastPointer *pointer_out, // Output: pointer to store
    ErrorContext *ctx
);
```

**Preconditions:**
- `shouldToast(size, page_size)` returns true
- TOAST table exists for parent table
- `xmin` is valid transaction ID

**Postconditions:**
- Data stored in TOAST table as chunks
- `pointer_out` contains valid ToastPointer
- Chunks have xmin set, xmax = 0

**Algorithm:**
```
1. Generate new value_id (UUID v7)
2. 
3. IF strategy == EXTERNAL:
4.     compressed = lz4Compress(data, size)
5.     store_data = compressed.data
6.     store_size = compressed.size
7. ELSE:
8.     store_data = data
9.     store_size = size
10.
11. chunk_size = ToastSettings::getMaxChunkSize(page_size)
12. num_chunks = ceil(store_size / chunk_size)
13.
14. FOR seq FROM 0 TO num_chunks - 1:
15.     chunk_data = store_data + (seq * chunk_size)
16.     chunk_len = min(chunk_size, store_size - (seq * chunk_size))
17.     writeToastChunk(value_id, seq, chunk_data, chunk_len, xmin)
18.
19. // Build ToastPointer
20. pointer_out->lob_uuid = value_id
21. pointer_out->total_len = size  // Original uncompressed size
22. pointer_out->chunk_size = chunk_size
23. pointer_out->compression = (strategy == EXTERNAL) ? LZ4 : NONE
24. pointer_out->flags = 0
25.
26. RETURN OK
```

#### Function: `detoastValue()`

```cpp
// Source: src/core/toast.cpp
Status ToastManager::detoastValue(
    const ToastPointer *pointer,
    std::vector<uint8_t> *data_out,
    uint64_t xmin,             // Reader's transaction ID
    ErrorContext *ctx
);
```

**Preconditions:**
- `pointer` is valid ToastPointer
- Chunks exist in TOAST table

**Postconditions:**
- `data_out` contains reconstructed data
- Visibility checked for each chunk

**Algorithm:**
```
1. num_chunks = ceil(pointer->total_len / pointer->chunk_size)
2. chunks_data = []
3.
4. // Build GPID list for prefetching
5. gpids = []
6. FOR seq FROM 0 TO num_chunks - 1:
7.     gpid = lookupToastChunkGpid(pointer->lob_uuid, seq)
8.     gpids.push_back(gpid)
9.
10. // Prefetch all chunks
11. buffer_pool->prefetchPagesGlobal(gpids, ctx)
12.
13. // Read and verify chunks
14. FOR seq FROM 0 TO num_chunks - 1:
15.     chunk = readToastChunk(gpids[seq], ctx)
16.     
17.     // MGA visibility check
18.     IF NOT isChunkVisible(chunk.header, xmin):
19.         RETURN NOT_VISIBLE
20.     
21.     chunks_data.append(chunk.chunk_data, chunk.chunk_size)
22.
23. // Decompress if needed
24. IF pointer->flags & TOAST_COMPRESSED:
25.     *data_out = lz4Decompress(chunks_data, pointer->total_len)
26. ELSE:
27.     *data_out = chunks_data
28.
29. RETURN OK
```

### MGA Visibility for Chunks

```cpp
// From include/scratchbird/core/toast_visibility.h
bool isChunkVisible(const TupleHeader &header, uint64_t reader_xid) {
    // Rule 1: Own changes always visible
    IF header.xmin == reader_xid:
        RETURN true
    
    // Rule 2: Frozen chunks always visible
    IF header.xmin <= FROZEN_XID:
        RETURN true
    
    // Rule 3: Check xmin state in TIP
    xmin_state = getTransactionState(header.xmin)
    IF xmin_state != COMMITTED:
        RETURN false  // Not committed, not visible
    
    // Rule 4: Check xmax
    IF header.xmax == 0:
        RETURN true  // Not deleted
    
    IF header.xmax == reader_xid:
        RETURN true  // Deleted by reader (can still see)
    
    xmax_state = getTransactionState(header.xmax)
    IF xmax_state != COMMITTED:
        RETURN true  // Delete not committed
    
    // xmax is committed - chunk is dead
    RETURN false
}
```

## Invariants

1. **Chunk Completeness**: All chunks for a value must exist or none
   - Verification: Count chunks match expected from total_len
   
2. **Sequence Continuity**: Chunk sequences are 0..N without gaps
   - Verification: Assert during read
   
3. **Size Consistency**: Sum of chunk sizes equals total_len (if uncompressed)
   - Verification: Compare after assembly
   
4. **MGA Compliance**: Chunks follow same visibility rules as heap tuples
   - Verification: Use same isVersionVisible() function

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `NOT_FOUND` | Chunk missing | Return error, data incomplete |
| `COMPRESSION_ERROR` | LZ4 compress/decompress failed | Return error, try uncompressed |
| `NOT_VISIBLE` | Chunk not visible to reader | Return error, treat as missing |
| `PAGE_FULL` | No space for new chunks | Extend TOAST table or return error |

## Performance Considerations

### Page Size Impact

| Page Size | 1 MB Value Chunks | Detoast Time |
|-----------|-------------------|--------------|
| 8 KB | 519 chunks | ~100 ms |
| 16 KB | 259 chunks | ~50 ms |
| 32 KB | 130 chunks | ~25 ms |
| 64 KB | 65 chunks | ~12 ms |
| 128 KB | 33 chunks | ~6 ms |

### Prefetching Benefit
- **Without prefetch**: Each chunk causes separate disk read
- **With prefetch**: Sequential chunks read in single I/O
- **Benefit**: 4-10x speedup for large values

### Compression Ratios
- **Text data**: Typically 2-4x compression
- **Binary data**: 1-1.5x (often incompressible)
- **Strategy**: EXTENDED for binary, EXTERNAL for text

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_toast.cpp` | Basic TOAST operations |
| `tests/unit/test_toast_compression.cpp` | LZ4 compression |
| `tests/unit/test_toast_prefetch.cpp` | Prefetching |
| `tests/unit/test_toast_visibility.cpp` | MGA visibility |

## Related Specifications

- [Heap Format](./heap_format.md) - Tuple format containing TOAST pointers
- [Page Allocation](./page_allocation.md) - TOAST chunk page allocation
- [MGA Visibility Rules](./mga_visibility_rules.md) - Chunk visibility

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
