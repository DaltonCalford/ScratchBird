# Specification: Page Layout

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/ondisk.h:305`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/heap_page.h:37`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/heap_page.cpp:132`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_heap_page.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_page_management.cpp`

## Synopsis

This specification defines the physical page layout for ScratchBird heap pages, including the page header format, slot array (item pointers), tuple data layout, and special area. The design follows Firebird MGA principles with PostgreSQL-compatible page layout.

## Scope

### In Scope

- Page header structure (80 bytes)
- Item pointer (slot) array format
- Tuple header and data layout
- Heap page special area
- Free space management
- Page validation

### Out of Scope

- Index page layouts (separate specifications)
- TOAST/LOB page layouts (separate specification)
- vNext page format (different specification)

## Background

ScratchBird heap pages use a slotted-page design:

```
┌─────────────────────────────────────────────────────────────┐
│ Page Header (80 bytes)                                      │
├─────────────────────────────────────────────────────────────┤
│ Item Pointer Array (grows downward)                         │
│ - Each entry: offset (4) + length:31 + flags:1 (4 bytes)   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ Free Space                                                  │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│ Tuple Data (grows upward from end)                          │
│ - Aligned to 8-byte boundaries                              │
├─────────────────────────────────────────────────────────────┤
│ Special Area (24 bytes - HeapPageSpecial)                   │
└─────────────────────────────────────────────────────────────┘
```

## Specification

### Data Structures

#### Page Header (80 bytes)

```cpp
// From include/scratchbird/core/ondisk.h:305
#pragma pack(push, 1)
struct PageHeader {
    uint32_t magic;        // 0x00 'SBRD' (0x53425244)
    uint16_t version;      // 0x04 format version (1)
    uint16_t page_type;    // 0x06 PageType enum
    uint32_t page_size;    // 0x08 8192|16384|32768|65536|131072
    uint32_t checksum;     // 0x0C CRC32C of bytes [0x10..page_size)
    uint64_t lsn;          // 0x10 Log Sequence Number (0 if no WAL)
    uint32_t page_id;      // 0x18 page number in file (0-based)
    uint32_t flags;        // 0x1C page-specific flags
    uint8_t  database_uuid[16]; // 0x20 Database UUID (v7)
    uint8_t  object_uuid[16];   // 0x30 Owning object UUID
    uint64_t generation;   // 0x40 page generation for MVCC
    uint16_t free_offset;  // 0x48 offset to start of free space (pd_lower)
    uint16_t item_count;   // 0x4A number of items on page
    uint16_t free_space;   // 0x4C bytes of free space
    uint16_t special_size; // 0x4E size of special area
};
#pragma pack(pop)
static_assert(sizeof(PageHeader) == 80, "PageHeader must be 80 bytes");
```

**Binary Layout:**

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0x00 | magic | 4 bytes | 'SBRD' (0x53425244) |
| 0x04 | version | 2 bytes | Format version (1) |
| 0x06 | page_type | 2 bytes | PageType enum value |
| 0x08 | page_size | 4 bytes | Page size in bytes |
| 0x0C | checksum | 4 bytes | CRC32C checksum |
| 0x10 | lsn | 8 bytes | Log sequence number |
| 0x18 | page_id | 4 bytes | Page number |
| 0x1C | flags | 4 bytes | Page flags |
| 0x20 | database_uuid | 16 bytes | Database UUID |
| 0x30 | object_uuid | 16 bytes | Table/object UUID |
| 0x40 | generation | 8 bytes | Page generation counter |
| 0x48 | free_offset | 2 bytes | pd_lower (scaled) |
| 0x4A | item_count | 2 bytes | Number of items |
| 0x4C | free_space | 2 bytes | Free space in bytes (scaled) |
| 0x4E | special_size | 2 bytes | Special area size (scaled) |

#### Item Pointer (8 bytes)

```cpp
// From include/scratchbird/core/heap_page.h:37
#pragma pack(push, 1)
struct ItemPointer {
    uint32_t offset;      // 0x00 Offset from start of page
    uint32_t length : 31; // 0x04 Length of tuple
    uint32_t flags : 1;   // 0x07:7 0=normal, 1=deleted/unused
};
#pragma pack(pop)
```

**Binary Layout:**

| Offset | Field | Size | Bits | Description |
|--------|-------|------|------|-------------|
| 0x00 | offset | 4 bytes | 0-31 | Byte offset to tuple |
| 0x04 | length | 4 bytes | 0-30 | Tuple length (max 2GB) |
| 0x07 | flags | 4 bytes | 31 | LP_DELETED flag |

**Constants:**
```cpp
static constexpr uint32_t FLAG_DELETED = 0x80000000;
static constexpr uint32_t LP_UNUSED = 0;  // offset 0 means unused
```

#### Tuple Header (80 bytes)

```cpp
// From include/scratchbird/core/heap_page.h:91
#pragma pack(push, 1)
struct TupleHeader {
    // Transaction info (16 bytes)
    uint64_t xmin;        // 0x00 Creating transaction
    uint64_t xmax;        // 0x08 Deleting/updating transaction

    // Version chain (12 bytes)
    uint64_t back_version_gpid;  // 0x10 GPID of back version
    uint16_t back_version_slot;  // 0x18 Slot of back version
    uint16_t reserved1;          // 0x1A Padding

    // Tuple metadata (12 bytes)
    GPID ctid_gpid;       // 0x1C Current TID (GPID)
    uint16_t ctid_slot;   // 0x24 Current TID (slot)
    uint16_t infomask;    // 0x26 State flags

    // Null bitmap (4 bytes)
    uint16_t null_bitmap_offset; // 0x28 Offset to null bitmap
    uint16_t padding;            // 0x2A Padding

    // Session scope (16 bytes)
    ID session_id;        // 0x2C Session UUID (temp tables)

    // Canonical record fields (28 bytes)
    ID row_uuid;          // 0x3C Row UUID
    uint32_t record_flags; // 0x4C RHD_* flags
    uint32_t record_format; // 0x50 Format version
    uint32_t payload_len;  // 0x54 Payload length
};
#pragma pack(pop)
```

#### Heap Page Special Area (24 bytes)

```cpp
// From include/scratchbird/core/heap_page.h:278
#pragma pack(push, 1)
struct HeapPageSpecial {
    uint16_t pd_flags;     // 0x00 Page flags
    uint16_t reserved;     // 0x02 Reserved for alignment
    ID table_id;           // 0x04 Table UUID (16 bytes)
    uint64_t pd_prune_xid; // 0x14 Oldest XID for pruning
};
#pragma pack(pop)
```

### Page Layout Diagram

```
8KB Page Layout Example:
┌─────────────────────────────────────────────────────────────┐ 0x0000
│ PageHeader (80 bytes)                                       │
│ - magic, version, page_type, page_size                      │
│ - checksum, lsn, page_id, flags                             │
│ - database_uuid, object_uuid                                │
│ - generation, free_offset, item_count                       │
│ - free_space, special_size                                  │
├─────────────────────────────────────────────────────────────┤ 0x0050
│ Item Pointer Array (grows toward higher addresses)          │
│ ┌─────────────────┐                                         │
│ │ ItemPointer[0]  │ 8 bytes (offset, length+flags)         │
│ ├─────────────────┤                                         │
│ │ ItemPointer[1]  │                                         │
│ ├─────────────────┤                                         │
│ │ ...             │                                         │
│ ├─────────────────┤                                         │
│ │ ItemPointer[N]  │                                         │
│ └─────────────────┘                                         │
├─────────────────────────────────────────────────────────────┤ pd_lower
│                                                             │
│                    FREE SPACE                               │
│                                                             │
├─────────────────────────────────────────────────────────────┤ pd_upper
│ Tuple Data (grows toward lower addresses)                   │
│ ┌─────────────────┐ ◄─── Tuple N (aligned to 8 bytes)       │
│ │ TupleHeader     │ 80 bytes                                │
│ ├─────────────────┤                                         │
│ │ Tuple Data      │ variable                                │
│ └─────────────────┘                                         │
│ ┌─────────────────┐ ◄─── Tuple N-1                          │
│ │ TupleHeader     │                                         │
│ ├─────────────────┤                                         │
│ │ Tuple Data      │                                         │
│ └─────────────────┘                                         │
│ ...                                                         │
├─────────────────────────────────────────────────────────────┤ 0x1FE8 (8176)
│ HeapPageSpecial (24 bytes)                                  │
│ - pd_flags, reserved                                        │
│ - table_id (16 bytes)                                       │
│ - pd_prune_xid                                              │
└─────────────────────────────────────────────────────────────┘ 0x2000 (8192)
```

### Free Space Calculations

```cpp
// From include/scratchbird/core/ondisk.h:778
inline auto pageLower(const PageHeader &header) -> uint32_t {
    const uint32_t unit = (header.page_size > 0xFFFFu) ? 2u : 1u;
    return static_cast<uint32_t>(header.free_offset) * unit;
}

inline auto pageUpper(const PageHeader &header) -> uint32_t {
    const uint32_t unit = (header.page_size > 0xFFFFu) ? 2u : 1u;
    return (static_cast<uint32_t>(header.free_offset) +
            static_cast<uint32_t>(header.free_space)) * unit;
}

inline auto pageSpecial(const PageHeader &header) -> uint32_t {
    const uint32_t unit = (header.page_size > 0xFFFFu) ? 2u : 1u;
    return header.page_size - (static_cast<uint32_t>(header.special_size) * unit);
}
```

**Free Space Formula:**
```
available_space = pd_upper - pd_lower
                = page_size - sizeof(PageHeader) 
                  - (item_count * sizeof(ItemPointer))
                  - sizeof(HeapPageSpecial)
                  - tuple_data_size
```

### Interface Contracts

#### Function: `HeapPage::initialize()`

```cpp
// Source: src/core/heap_page.cpp:132
Status HeapPage::initialize(
    uint32_t page_id,
    ErrorContext *ctx = nullptr
);
```

**Preconditions:**
- Page buffer is allocated and zeroed
- `page_size_` is valid (8192, 16384, 32768, 65536, or 131072)

**Postconditions:**
- Header magic set to 'SBRD'
- Version set to 1
- Page type set to PAGE_TYPE_HEAP
- `pd_lower` = sizeof(PageHeader)
- `pd_upper` = page_size - sizeof(HeapPageSpecial)
- Special area initialized with table_id

#### Function: `HeapPage::insertTuple()`

```cpp
// Source: src/core/heap_page.cpp:216
Status HeapPage::insertTuple(
    const uint8_t *tuple_data,
    uint32_t tuple_size,
    uint64_t xmin,
    uint16_t *item_id_out,
    ErrorContext *ctx = nullptr
);
```

**Preconditions:**
- Page is initialized
- Sufficient free space exists
- `tuple_size >= sizeof(TupleHeader)`

**Postconditions:**
- Tuple copied to page at aligned offset
- Item pointer added to slot array
- `pd_upper` updated
- `item_count` incremented
- `*item_id_out` set to allocated slot number

### Algorithms

#### Algorithm: Page Initialization

```
Input:  page_id, page_buffer, page_size
Output: initialized page

1. Zero page buffer
2. 
3. header = CAST page_buffer TO PageHeader
4. header.magic = K_MAGIC_SBRD ('SBRD')
5. header.version = 1
6. header.page_type = PAGE_TYPE_HEAP
7. header.page_size = page_size
8. header.page_id = page_id
9. header.generation = 1
10. header.checksum = 0
11. header.flags = 0
12. header.lsn = 0
13. SET database_uuid FROM database
14. SET object_uuid FROM table_id
15. 
16. pageSetLower(header, sizeof(PageHeader))
17. pageSetUpper(header, page_size - sizeof(HeapPageSpecial))
18. pageSetSpecial(header, page_size - sizeof(HeapPageSpecial))
19. 
20. special = CAST (page_buffer + page_size - sizeof(HeapPageSpecial))
21.           TO HeapPageSpecial
22. special.pd_flags = 0
23. special.table_id = table_id
24. special.pd_prune_xid = 0
```

#### Algorithm: Tuple Insertion

```
Input:  tuple_data, tuple_size, xmin
Output: item_id

1. IF tuple_size < sizeof(TupleHeader):
2.     RETURN INVALID_ARGUMENT
3. 
4. IF tuple_size > max_tuple_size:
5.     RETURN INVALID_ARGUMENT
6.
7. // Check if TOAST needed
8. IF shouldToast(tuple_size, page_size):
9.     toast_tuple = performTOAST(tuple_data, tuple_size)
10.    data_to_insert = toast_tuple.data
11.    actual_size = toast_tuple.size
12. ELSE:
13.    data_to_insert = tuple_data
14.    actual_size = tuple_size
15.
16. // Check free space
17. IF NOT hasFreeSpace(actual_size + sizeof(ItemPointer)):
18.     RETURN PAGE_FULL
19.
20. // Find or allocate item slot
21. item_id = getItemCount()
22. FOR i FROM 0 TO getItemCount() - 1:
23.     IF items[i].isDeleted() AND items[i].length >= actual_size:
24.         item_id = i
25.         BREAK
26.
27. // Calculate tuple offset (from end, aligned to 8 bytes)
28. tuple_offset = pageUpper(header) - actual_size
29. tuple_offset = (tuple_offset / 8) * 8  // Align down
30.
31. // Validate offset
32. IF tuple_offset + actual_size > page_size:
33.     RETURN PAGE_CORRUPT
34.
35. // Copy tuple data
36. COPY data_to_insert TO page_data[tuple_offset]
37.
38. // Initialize tuple header fields
39. tuple_hdr = CAST page_data[tuple_offset] TO TupleHeader
40. tuple_hdr.xmin = xmin
41. tuple_hdr.xmax = 0
42. tuple_hdr.back_version_gpid = INVALID_GPID
43. tuple_hdr.back_version_slot = 0
44. tuple_hdr.ctid_gpid = MAKE_GPID(tablespace_id, page_id)
45. tuple_hdr.ctid_slot = item_id
46. tuple_hdr.infomask = 0
47. tuple_hdr.null_bitmap_offset = 0
48. tuple_hdr.session_id = normalized_session_id
49.
50. // Update or add item pointer
51. IF item_id == getItemCount():
52.     pageSetLower(header, pageLower(header) + sizeof(ItemPointer))
53. 
54. items[item_id].offset = tuple_offset
55. items[item_id].length = actual_size
56. items[item_id].setDeleted(false)
57.
58. // Update upper boundary
59. pageSetUpper(header, tuple_offset)
60.
61. RETURN item_id
```

#### Algorithm: Page Validation

```
Input:  page_buffer, page_size
Output: Status (OK or CORRUPT)

1. header = CAST page_buffer TO PageHeader
2.
3. // Check magic
4. IF header.magic != K_MAGIC_SBRD:
5.     RETURN PAGE_CORRUPT
6.
7. // Check page type
8. IF header.page_type != PAGE_TYPE_HEAP:
9.     RETURN PAGE_CORRUPT
10.
11. // Check page size consistency
12. IF header.page_size != page_size:
13.     RETURN PAGE_CORRUPT
14.
15. // Validate boundaries
16. pd_lower = pageLower(header)
17. pd_upper = pageUpper(header)
18. pd_special = pageSpecial(header)
19.
20. IF pd_lower < sizeof(PageHeader):
21.     RETURN PAGE_CORRUPT
22.
23. IF pd_lower > pd_upper:
24.     RETURN PAGE_CORRUPT
25.
26. IF pd_upper > pd_special:
27.     RETURN PAGE_CORRUPT
28.
29. IF pd_special != page_size - sizeof(HeapPageSpecial):
30.     RETURN PAGE_CORRUPT
31.
32. // Validate item pointers
33. item_count = (pd_lower - sizeof(PageHeader)) / sizeof(ItemPointer)
34. FOR i FROM 0 TO item_count - 1:
35.     item = items[i]
36.     IF NOT item.isDeleted():
37.         IF item.offset < pd_upper:
38.             RETURN PAGE_CORRUPT
39.         IF item.offset + item.length > pd_special:
40.             RETURN PAGE_CORRUPT
41.
42. // Verify checksum
43. IF NOT validatePageChecksum(page_buffer, page_size):
44.     RETURN CHECKSUM_MISMATCH
45.
46. RETURN OK
```

### Page Flags

```cpp
// From include/scratchbird/core/ondisk.h:238
constexpr uint32_t PAGE_FLAG_DIRTY = 0x0001;          // Uncommitted changes
constexpr uint32_t PAGE_FLAG_PINNED = 0x0002;         // Pinned in buffer
constexpr uint32_t PAGE_FLAG_COMPRESSED = 0x0004;     // Compressed data
constexpr uint32_t PAGE_FLAG_ENCRYPTED = 0x0008;      // Encrypted data
constexpr uint32_t PAGE_FLAG_SPECIAL = 0x0010;        // Special area populated
constexpr uint32_t PAGE_FLAG_CHECKSUM_VALID = 0x0020; // Checksum must validate
```

### Tuple Header Flags (infomask)

```cpp
// From include/scratchbird/core/heap_page.h:125
static constexpr uint16_t HEAP_HAS_NULLS = 0x0001;
static constexpr uint16_t HEAP_XMIN_COMMITTED = 0x0002;
static constexpr uint16_t HEAP_XMIN_INVALID = 0x0004;
static constexpr uint16_t HEAP_XMAX_COMMITTED = 0x0008;
static constexpr uint16_t HEAP_XMAX_INVALID = 0x0010;
static constexpr uint16_t HEAP_XMAX_IS_MULTI = 0x0020;
static constexpr uint16_t HEAP_UPDATED = 0x0040;
static constexpr uint16_t HEAP_MOVED = 0x0080;
static constexpr uint16_t HEAP_XMIN_FROZEN = 0x0100;
static constexpr uint16_t HEAP_HOT_UPDATED = 0x0200;
static constexpr uint16_t HEAP_CHAIN = 0x0400;
```

## Invariants

1. **Magic Number**: All valid pages have magic = 'SBRD'
   - Verification: Checked on every page read
   
2. **Boundary Ordering**: `pd_lower <= pd_upper <= pd_special <= page_size`
   - Verification: Checked in validate()
   
3. **Item Pointer Validity**: All item pointers point within `pd_upper..pd_special`
   - Verification: Checked in validate()
   
4. **8-byte Alignment**: All tuple offsets are 8-byte aligned
   - Verification: `tuple_offset = (tuple_offset / 8) * 8`
   
5. **Checksum Consistency**: Checksum must validate for committed pages
   - Verification: CRC32C computed over page data

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PAGE_CORRUPT` | Invalid magic | Return error, may attempt repair |
| `PAGE_CORRUPT` | Boundary violation | Return error, page may be unusable |
| `CHECKSUM_MISMATCH` | CRC check failed | Return error, indicates corruption |
| `PAGE_FULL` | Insufficient free space | Allocate new page |
| `INVALID_ARGUMENT` | Tuple too small/large | Return error to caller |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_heap_page.cpp` | Page operations, layout |
| `tests/unit/test_page_management.cpp` | Page allocation |
| `tests/unit/test_page_contract_stage_s1.cpp` | Page contract validation |
| `tests/unit/test_vnext_page_contract.cpp` | vNext page layout |

## Migration Notes

N/A - Initial page layout specification for ScratchBird Alpha.

## Related Specifications

- [Version Chain Format](./version_chain_format.md) - Tuple versions within pages
- [Transaction Lifecycle](./transaction_lifecycle.md) - Transaction IDs in tuple headers
- [GC Sweep Algorithm](./gc_sweep_algorithm.md) - Page compaction

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Page | Fixed-size block of storage (8KB-128KB) |
| Slot | Item pointer array entry |
| Item Pointer | Offset/length pair pointing to tuple |
| pd_lower | Offset to end of item pointer array |
| pd_upper | Offset to start of tuple data |
| pd_special | Offset to special area |
| Tuple | Row version stored on page |
| Alignment | 8-byte boundary requirement for tuples |

### Supported Page Sizes

| Size | Alpha Support | Notes |
|------|---------------|-------|
| 8192 | Yes | Default size |
| 16384 | Yes | |
| 32768 | Yes | |
| 65536 | Yes | |
| 131072 | Yes | Maximum |

### References

- PostgreSQL page layout documentation
- Firebird page layout documentation
- `ON_DISK_FORMAT.md` - Complete on-disk format

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
