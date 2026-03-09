# Specification: Heap Tuple Format

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/heap_page.h:91`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/heap_page.h:276`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_heap_page.cpp`

## Synopsis

This specification defines the heap tuple format used for row storage in ScratchBird, including the tuple header structure, infomask flags, and data layout.

## Scope

### In Scope

- Tuple header structure (80 bytes)
- Infomask flags and their meanings
- Tuple data layout after header
- Null bitmap handling
- System column storage

### Out of Scope

- TOAST pointer format (see TOAST specification)
- Version chain mechanics (see Version Chains)
- Index tuple formats (see Index specs)

## Background

Heap tuples store row data with a fixed 80-byte header containing:
- Transaction IDs (xmin, xmax)
- Version chain pointers
- Visibility flags (infomask)
- System metadata

The design balances Firebird MGA requirements with PostgreSQL compatibility.

## Specification

### Data Structures

#### TupleHeader (80 bytes)

```cpp
// From include/scratchbird/core/heap_page.h:91
#pragma pack(push, 1)
struct TupleHeader {
    // Transaction info (16 bytes)
    uint64_t xmin;        // 0x00: Creating transaction ID
    uint64_t xmax;        // 0x08: Deleting/updating transaction ID (0 = not deleted)

    // Version chain (12 bytes) - Firebird MGA back versioning
    uint64_t back_version_gpid;  // 0x10: GPID of back version (INVALID_GPID if none)
    uint16_t back_version_slot;  // 0x18: Slot of back version
    uint16_t reserved1;          // 0x1A: Padding

    // Tuple metadata (12 bytes)
    GPID ctid_gpid;       // 0x1C: Current tuple GPID (tablespace + page)
    uint16_t ctid_slot;   // 0x24: Current tuple slot
    uint16_t infomask;    // 0x26: State flags

    // Null bitmap (4 bytes)
    uint16_t null_bitmap_offset; // 0x28: Offset to null bitmap (0 if no nulls)
    uint16_t padding;            // 0x2A: Padding

    // Session scope (16 bytes)
    ID session_id;        // 0x2C: Session UUID (temp tables), zero for permanent

    // Canonical record fields (28 bytes)
    ID row_uuid;          // 0x3C: Stable row UUID across versions
    uint32_t record_flags; // 0x4C: RHD_* flags
    uint32_t record_format; // 0x50: Format version
    uint32_t payload_len;  // 0x54: Bytes after header
};
#pragma pack(pop)
static_assert(sizeof(TupleHeader) == 80, "TupleHeader must be 80 bytes");
```

#### Tuple Header Binary Layout

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0x00 | xmin | 8 | Creating transaction ID |
| 0x08 | xmax | 8 | Deleting/updating XID (0 = alive) |
| 0x10 | back_version_gpid | 8 | Back version page (INVALID_GPID = none) |
| 0x18 | back_version_slot | 2 | Back version slot |
| 0x1A | reserved1 | 2 | Padding |
| 0x1C | ctid_gpid | 8 | Current location GPID |
| 0x24 | ctid_slot | 2 | Current location slot |
| 0x26 | infomask | 2 | Visibility and state flags |
| 0x28 | null_bitmap_offset | 2 | Offset to null bitmap from tuple start |
| 0x2A | padding | 2 | Padding |
| 0x2C | session_id | 16 | Session UUID (for temp tables) |
| 0x3C | row_uuid | 16 | Stable row identifier |
| 0x4C | record_flags | 4 | Additional flags |
| 0x50 | record_format | 4 | Tuple format version |
| 0x54 | payload_len | 4 | Length of data after header |
| 0x58 | -- | 8 | Unused (to reach 96 bytes total with data start) |

### Infomask Flags

```cpp
// From include/scratchbird/core/heap_page.h:125
static constexpr uint16_t HEAP_HAS_NULLS = 0x0001;        // Has null columns
static constexpr uint16_t HEAP_XMIN_COMMITTED = 0x0002;   // xmin committed
static constexpr uint16_t HEAP_XMIN_INVALID = 0x0004;     // xmin aborted
static constexpr uint16_t HEAP_XMAX_COMMITTED = 0x0008;   // xmax committed
static constexpr uint16_t HEAP_XMAX_INVALID = 0x0010;     // xmax aborted
static constexpr uint16_t HEAP_XMAX_IS_MULTI = 0x0020;    // xmax is MultiXID
static constexpr uint16_t HEAP_UPDATED = 0x0040;          // Tuple was updated
static constexpr uint16_t HEAP_MOVED = 0x0080;            // Tuple moved
static constexpr uint16_t HEAP_XMIN_FROZEN = 0x0100;      // xmin is FROZEN_XID
static constexpr uint16_t HEAP_HOT_UPDATED = 0x0200;      // HOT update
static constexpr uint16_t HEAP_CHAIN = 0x0400;            // In version chain
```

### Record Flags (record_flags)

```cpp
// From include/scratchbird/core/heap_page.h:138
static constexpr uint32_t RHD_DELETED = 0x0001;      // Tuple is deleted
static constexpr uint32_t RHD_CHAINED = 0x0002;      // Part of chain
static constexpr uint32_t RHD_MOVED = 0x0004;        // Tuple moved
static constexpr uint32_t RHD_TOAST_PTR = 0x0008;    // Contains TOAST pointer
static constexpr uint32_t RECORD_FORMAT_V1 = 1;      // Current format version
```

### Tuple Data Layout

```
Complete Tuple on Page:
┌─────────────────────────────────────────────────────────────┐
│ TupleHeader (80 bytes)                                      │
│ - xmin, xmax                                                │
│ - back_version pointer                                      │
│ - ctid (self pointer)                                       │
│ - infomask, flags                                           │
│ - row_uuid                                                  │
├─────────────────────────────────────────────────────────────┤
│ Null Bitmap (optional, variable)                           │
│ - 1 bit per nullable column                                │
│ - Only present if HEAP_HAS_NULLS set                       │
├─────────────────────────────────────────────────────────────┤
│ User Data (variable)                                       │
│ - Column values in order                                   │
│ - Fixed-width columns first                                │
│ - Variable-width after                                     │
├─────────────────────────────────────────────────────────────┤
│ Padding (to 8-byte alignment)                              │
└─────────────────────────────────────────────────────────────┘
```

### HeapPageSpecial (24 bytes)

```cpp
// From include/scratchbird/core/heap_page.h:278
#pragma pack(push, 1)
struct HeapPageSpecial {
    uint16_t pd_flags;     // 0x00: Page flags
    uint16_t reserved;     // 0x02: Alignment
    ID table_id;           // 0x04: Table UUID
    uint64_t pd_prune_xid; // 0x14: Oldest XID for pruning
};
#pragma pack(pop)
```

## Interface Contracts

### TupleHeader Methods

```cpp
// Visibility helpers
bool hasNulls() const;                    // Check HEAP_HAS_NULLS
bool isDeleted() const;                   // xmax committed and not updated
bool isUpdated() const;                   // Check HEAP_UPDATED
bool hasBackVersion() const;              // back_version_gpid != INVALID_GPID

// Version chain
TID getBackVersionTID() const;            // Get back version location
void setBackVersionTID(GPID gpid, uint16_t slot);

// Location
TID getTID() const;                       // Get current location
void setTID(GPID gpid, uint16_t slot);

// Record flags
bool hasRecordFlag(uint32_t flag) const;
void setRecordFlag(uint32_t flag, bool enabled);

// Transaction IDs
uint64_t getCreateTxid() const;           // Return xmin
uint64_t getDeleteTxid() const;           // Return xmax
uint64_t getLastEditTxidSystem() const;   // xmax if deleted, else xmin
```

## Algorithms

### Algorithm: Calculate Null Bitmap Size

```
Input:  num_nullable_columns
Output: bitmap_size_bytes

1. bits_needed = num_nullable_columns
2. bytes_needed = (bits_needed + 7) / 8  // Round up
3. RETURN bytes_needed
```

### Algorithm: Check Column Null

```
Input:  tuple_data, column_index
Output: is_null (boolean)

1. header = CAST tuple_data TO TupleHeader
2. IF NOT (header.infomask & HEAP_HAS_NULLS):
3.     RETURN false  // No nulls in this tuple

4. bitmap_offset = header.null_bitmap_offset
5. IF bitmap_offset == 0:
6.     RETURN false

7. byte_idx = column_index / 8
8. bit_idx = column_index % 8
9. bitmap_byte = tuple_data[bitmap_offset + byte_idx]
10. RETURN (bitmap_byte & (1 << bit_idx)) != 0
```

### Algorithm: Extract System Columns

```
Input:  tuple_data, tuple_size
Output: row_uuid, last_edit_txid

1. IF tuple_size < sizeof(TupleHeader):
2.     RETURN INVALID_ARGUMENT

3. header = CAST tuple_data TO TupleHeader
4. row_uuid = header.row_uuid

5. IF (header.record_flags & RHD_DELETED) AND header.xmax != 0:
6.     last_edit_txid = header.xmax
7. ELSE:
8.     last_edit_txid = header.xmin

9. RETURN (row_uuid, last_edit_txid)
```

## Invariants

1. **Header Size**: TupleHeader is always exactly 80 bytes
   - Verification: static_assert(sizeof(TupleHeader) == 80)
   
2. **XID Ordering**: For valid tuples, xmin <= xmax (if xmax != 0)
   - Verification: Assert during tuple creation
   
3. **Back Version Validity**: If hasBackVersion(), back_version points to older tuple
   - Verification: Version chain traversal validates
   
4. **CTID Consistency**: ctid points to this tuple's location
   - Verification: Set on insert/update, never modified

## Decision Trees

```
Is tuple visible to reader?
│
├── infomask & HEAP_XMIN_FROZEN ───────────────► YES (frozen)
│
├── xmin == reader_xid ────────────────────────► YES (own changes)
│
├── infomask & HEAP_XMIN_COMMITTED ────────────► Check xmax
│   │
│   ├── xmax == 0 ─────────────────────────────► YES (alive)
│   ├── xmax == reader_xid ────────────────────► YES (deleted by self)
│   └── infomask & HEAP_XMAX_INVALID ──────────► YES (delete aborted)
│
├── infomask & HEAP_XMIN_INVALID ──────────────► NO (insert aborted)
│
└── ELSE: Need TIP lookup
    ├── getTransactionState(xmin) == ABORTED ──► NO
    ├── getTransactionState(xmin) == ACTIVE ───► NO
    └── getTransactionState(xmin) == COMMITTED ► Check xmax (above)
```

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `INVALID_ARGUMENT` | Tuple size < sizeof(TupleHeader) | Reject tuple |
| `PAGE_CORRUPT` | Invalid XID in header | Log error, skip tuple |
| `NOT_FOUND` | Back version pointer invalid | Stop chain traversal |

## Performance Considerations

### Header Size Trade-offs
- **80 bytes**: Larger than PostgreSQL (23 bytes), smaller than Firebird
- **Benefit**: Includes row_uuid for stable identity, full GPID support
- **Cost**: ~3% space overhead for narrow rows

### Null Bitmap Optimization
- **No nulls**: Offset = 0, no bitmap stored
- **With nulls**: Bitmap stored immediately after header
- **Benefit**: Saves space when no nullable columns

### Alignment
- **TupleHeader**: Naturally aligned (no gaps)
- **User data**: 8-byte aligned after header
- **Benefit**: Efficient SIMD and direct access

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_heap_page.cpp` | Tuple operations |
| `tests/unit/test_tuple_header.cpp` | Header manipulation |
| `tests/unit/test_infomask.cpp` | Flag handling |
| `tests/unit/test_null_bitmap.cpp` | Null handling |

## Related Specifications

- [Page Layout](./page_layout.md) - Heap page structure
- [Version Chain Format](./version_chain_format.md) - Back version pointers
- [TOAST Storage](./toast_storage.md) - Large value handling
- [MGA Visibility Rules](./mga_visibility_rules.md) - Visibility computation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
