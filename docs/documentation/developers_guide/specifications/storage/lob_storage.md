# Specification: Large Object (LOB) Storage

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/ondisk.h:530`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_lob.cpp`

## Synopsis

This specification defines Large Object (LOB) storage for binary data up to 4GB. Unlike TOAST which is column-oriented, LOBs are standalone objects referenced by OID with streaming read/write capabilities.

## Scope

### In Scope

- LOB metadata and chunk structures
- LOB creation, reading, writing
- Partial updates (substring operations)
- LOB deletion and garbage collection
- MGA visibility for LOBs

### Out of Scope

- BFILE (external file references)
- LOB indexing
- LOB deduplication
- SecureFile LOB features

## Background

LOBs differ from TOAST in several ways:

| Feature | TOAST | LOB |
|---------|-------|-----|
| Size limit | 1 GB | 4 GB |
| Reference | Inline pointer | OID reference |
| Partial access | No (all-or-nothing) | Yes (streaming) |
| Updates | Immutable | Mutable |
| Use case | Column values | Documents, media |

## Specification

### Data Structures

#### LobMetaRecordLayout (60 bytes)

```cpp
// From include/scratchbird/core/ondisk.h:530
#pragma pack(push, 1)
struct LobMetaRecordLayout {
    uint8_t lob_uuid[16];       // 16 bytes: LOB unique ID
    uint8_t owner_object_uuid[16]; // 16 bytes: Owner reference
    uint64_t total_len;         // 8 bytes: Total data length
    uint32_t chunk_size;        // 4 bytes: Size per chunk
    uint64_t created_txid;      // 8 bytes: Creating transaction
    uint64_t deleted_txid;      // 8 bytes: Deleting transaction (0 = active)
};
#pragma pack(pop)
static_assert(sizeof(LobMetaRecordLayout) == 60, "LobMetaRecordLayout must be 60 bytes");
```

#### LobChunkRecordHeader (24 bytes)

```cpp
// From include/scratchbird/core/ondisk.h:540
#pragma pack(push, 1)
struct LobChunkRecordHeader {
    uint8_t lob_uuid[16];       // 16 bytes: Owner LOB ID
    uint32_t chunk_index;       // 4 bytes: Chunk sequence
    uint32_t payload_len;       // 4 bytes: Data bytes in chunk
};
#pragma pack(pop)
static_assert(sizeof(LobChunkRecordHeader) == 24, "LobChunkRecordHeader must be 24 bytes");
```

#### LOB Descriptor (In-Memory)

```cpp
struct LobDescriptor {
    ID lob_id;                  // LOB UUID
    ID owner_id;                // Owner table/column
    uint64_t length;            // Current length
    uint32_t chunk_size;        // Chunk size (typically 8KB)
    uint64_t xmin;              // Creating transaction
    uint64_t xmax;              // Deleting transaction
    bool is_temporary;          // In temp tablespace?
    uint32_t ref_count;         // Reference count
};
```

### Page Types

| Page Type | Value | Description |
|-----------|-------|-------------|
| PAGE_TYPE_LOB_META | 0x000A | LOB metadata pages |
| PAGE_TYPE_LOB_CHUNK | 0x000B | LOB data chunks |

### Interface Contracts

#### Function: `createLOB()`

```cpp
Status LobManager::createLOB(
    const ID &owner_id,         // Owning object
    uint64_t initial_size,      // Initial allocation (can be 0)
    bool is_temporary,          // Temp LOB?
    ID *lob_id_out,             // Output: LOB ID
    ErrorContext *ctx
);
```

**Preconditions:**
- Owner exists (if specified)
- Sufficient space in LOB tablespace

**Postconditions:**
- Metadata record created
- LOB ID generated and returned
- Initial chunks allocated (if size > 0)

#### Function: `writeLOB()`

```cpp
Status LobManager::writeLOB(
    const ID &lob_id,           // LOB to write
    uint64_t offset,            // Byte offset to write at
    const uint8_t *data,        // Data to write
    uint32_t length,            // Bytes to write
    uint64_t xid,               // Current transaction
    ErrorContext *ctx
);
```

**Preconditions:**
- LOB exists and is visible to xid
- offset + length <= 4GB (LOB size limit)
- offset can extend current size (sparse LOBs)

**Postconditions:**
- Data written at specified offset
- Metadata updated if length increased
- New chunks allocated as needed

**Algorithm:**
```
1. Load LOB metadata
2. Verify visibility (xmin committed, xmax 0 or aborted)

3. start_chunk = offset / lob.chunk_size
4. end_chunk = (offset + length - 1) / lob.chunk_size

5. FOR chunk_idx FROM start_chunk TO end_chunk:
6.     chunk_offset = (chunk_idx == start_chunk) ? 
7.                     (offset % lob.chunk_size) : 0
8.     chunk_bytes = min(remaining, lob.chunk_size - chunk_offset)
9.     
10.    IF chunk exists:
11.        readModifyWriteChunk(lob_id, chunk_idx, 
12.                            chunk_offset, data, chunk_bytes)
13.    ELSE:
14.        allocateNewChunk(lob_id, chunk_idx)
15.        writeChunk(lob_id, chunk_idx, chunk_offset, data, chunk_bytes)
16.    
17.    data += chunk_bytes
18.    remaining -= chunk_bytes

19. IF offset + length > lob.length:
20.    lob.length = offset + length
21.    updateMetadata(lob)

22. RETURN OK
```

#### Function: `readLOB()`

```cpp
Status LobManager::readLOB(
    const ID &lob_id,           // LOB to read
    uint64_t offset,            // Byte offset to read from
    uint32_t length,            // Bytes to read
    uint8_t *buffer,            // Output buffer
    uint32_t *bytes_read_out,   // Actual bytes read
    uint64_t xid,               // Current transaction
    ErrorContext *ctx
);
```

**Preconditions:**
- LOB exists and is visible
- Buffer has space for `length` bytes

**Postconditions:**
- Up to `length` bytes copied to buffer
- `*bytes_read_out` set to actual bytes (may be less if EOF)

#### Function: `deleteLOB()`

```cpp
Status LobManager::deleteLOB(
    const ID &lob_id,           // LOB to delete
    uint64_t xmax,              // Deleting transaction
    ErrorContext *ctx
);
```

**Preconditions:**
- LOB exists and is visible
- xmax is valid transaction ID

**Postconditions:**
- xmax set in metadata (MGA soft delete)
- Chunks NOT immediately freed
- Physical cleanup by GC when safe

**Algorithm:**
```
1. Load LOB metadata
2. Verify visibility

3. // MGA soft delete
4. metadata.deleted_txid = xmax
5. metadata.xmax = xmax
6. writeMetadata(metadata)

7. // Notify GC
8. garbage_collector->notifyLobDeleted(lob_id, xmax)

9. RETURN OK
```

#### Function: `truncateLOB()`

```cpp
Status LobManager::truncateLOB(
    const ID &lob_id,
    uint64_t new_length,        // New length (can be 0)
    uint64_t xid,
    ErrorContext *ctx
);
```

**Preconditions:**
- LOB exists and is visible
- new_length <= current length

**Postconditions:**
- LOB truncated to new_length
- Excess chunks marked for deletion

### LOB Garbage Collection

```
Algorithm: collectLobGarbage(oit)

1. FOR each LOB metadata record:
2.     IF lob.xmax != 0 AND lob.xmax < oit:
3.         // LOB deleted by transaction older than OIT
4.         // Safe to physically remove
5.         
6.         num_chunks = ceil(lob.length / lob.chunk_size)
7.         FOR chunk_idx FROM 0 TO num_chunks - 1:
8.             freeChunk(lob.lob_id, chunk_idx)
9.         
10.        freeMetadataRecord(lob.lob_id)
11.        stats.lobs_freed++
```

## Invariants

1. **LOB ID Uniqueness**: Each LOB has globally unique UUID
   - Verification: UUID v7 generation guarantees uniqueness
   
2. **Chunk Completeness**: All chunks in range [0, ceil(length/chunk_size)) exist
   - Verification: Assert during read, repair during GC
   
3. **Length Consistency**: Sum of chunk payload_lens = length
   - Verification: Recalculate on metadata load
   
4. **MGA Visibility**: LOB visibility follows same rules as heap tuples
   - Verification: Use isVersionVisible() for metadata

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `NOT_FOUND` | LOB doesn't exist | Return error |
| `NOT_VISIBLE` | LOB deleted or not committed | Return error |
| `INVALID_ARGUMENT` | Offset beyond 4GB limit | Return error |
| `PAGE_FULL` | No space for new chunks | Extend tablespace |
| `TRUNCATED` | Read beyond EOF | Return partial data |

## Performance Considerations

### Chunk Size Selection
- **Small chunks (4KB)**: Better for small LOBs, less waste
- **Large chunks (64KB)**: Better for large LOBs, less metadata
- **Default**: 8KB (matches page size)

### Random Access
- **Offset calculation**: chunk_index = offset / chunk_size
- **Lookup**: Index on (lob_uuid, chunk_index) for O(1) access
- **Benefit**: Any byte accessible in ~2 disk reads

### Streaming Reads
- **Sequential prefetch**: Read ahead N chunks
- **Buffering**: Cache recent chunks in LOB buffer
- **Benefit**: High throughput for sequential access

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_lob.cpp` | Basic LOB operations |
| `tests/unit/test_lob_streaming.cpp` | Streaming read/write |
| `tests/unit/test_lob_partial.cpp` | Partial updates |
| `tests/unit/test_lob_gc.cpp` | Garbage collection |

## Comparison: TOAST vs LOB

| Aspect | TOAST | LOB |
|--------|-------|-----|
| Max size | 1 GB | 4 GB |
| Mutability | Immutable | Mutable |
| Access pattern | All-or-nothing | Random access |
| Reference type | Inline pointer | OID reference |
| Use case | Column data | Documents, media |
| Compression | Yes | Optional |
| Deduplication | No | Future |

## Related Specifications

- [TOAST Storage](./toast_storage.md) - Alternative large value storage
- [Heap Format](./heap_format.md) - Heap tuples referencing LOBs
- [GC Sweep](./gc_sweep.md) - LOB garbage collection

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
