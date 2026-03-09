# Specification: WAL Record Format

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | Future (vNext) |
| **Authors** | Dalton Calford |

## Synopsis

This specification defines the Write-Ahead Log (WAL) record format for crash recovery. WAL records describe all modifications to database pages, enabling atomicity and durability.

## Scope

### In Scope

- WAL record structure
- Record types (heap, index, commit, etc.)
- WAL header format
- Record chaining
- CRC/checksum

### Out of Scope

- WAL buffering and flushing (see Buffer Manager)
- WAL archiving
- Point-in-time recovery
- Logical decoding

## Background

WAL (Write-Ahead Logging) ensures durability:
1. **Before modifying page**: Write WAL record
2. **Crash recovery**: Replay WAL to restore consistency
3. **Atomicity**: Commit record marks transaction complete

**Note**: ScratchBird Alpha does not implement WAL (no-wal architecture). This spec is for future versions.

## Specification

### Data Structures

#### WAL Record Header (24 bytes)

```cpp
struct WALRecordHeader {
    uint32_t magic;         // 'WALR' (0x57414C52)
    uint16_t record_type;   // Record type enum
    uint16_t flags;         // Record flags
    
    uint64_t xid;           // Transaction ID
    uint64_t lsn;           // Log Sequence Number
    
    uint16_t data_len;      // Payload length
    uint16_t crc16;         // Header CRC
    uint32_t padding;       // Alignment
};
```

#### Record Types

```cpp
enum class WALRecordType : uint16_t {
    // Transaction records
    XLOG_COMMIT = 0x0001,
    XLOG_ABORT = 0x0002,
    XLOG_PREPARE = 0x0003,
    XLOG_COMMIT_PREPARED = 0x0004,
    XLOG_ABORT_PREPARED = 0x0005,
    
    // Heap records
    HEAP_INSERT = 0x0100,
    HEAP_DELETE = 0x0101,
    HEAP_UPDATE = 0x0102,
    HEAP_HOT_UPDATE = 0x0103,
    HEAP_INPLACE = 0x0104,
    HEAP_FREEZE = 0x0105,
    
    // Index records
    BTREE_INSERT = 0x0200,
    BTREE_SPLIT = 0x0201,
    BTREE_VACUUM = 0x0202,
    
    // Storage records
    FSM_UPDATE = 0x0300,
    VM_UPDATE = 0x0301,
    
    // CLOG records
    CLOG_ASSIGN = 0x0400,
    
    // Checkpoint
    CHECKPOINT = 0x0500,
    
    // Bootstrap
    BOOTSTRAP = 0xFF00
};
```

### Record Types Detail

#### HEAP_INSERT

```cpp
struct HeapInsertRecord {
    GPID target_gpid;           // Page to insert into
    uint16_t item_id;           // Item pointer assigned
    uint16_t data_offset;       // Offset in record of tuple data
    uint32_t tuple_len;         // Tuple length
    // Followed by: tuple data
};
```

#### HEAP_UPDATE

```cpp
struct HeapUpdateRecord {
    GPID old_gpid;              // Old tuple location
    uint16_t old_item_id;
    GPID new_gpid;              // New tuple location (back version or primary)
    uint16_t new_item_id;
    uint64_t new_xmin;          // New version's xmin
    uint64_t xmax;              // Old version's xmax
    bool is_hot;                // HOT update?
    // Followed by: new tuple data if moved
};

#### XLOG_COMMIT

```cpp
struct CommitRecord {
    uint64_t xid;               // Committing transaction
    uint64_t commit_time;       // Timestamp
    uint32_t nsubxacts;         // Number of subtransactions
    // Followed by: array of subtransaction XIDs
};
```

### WAL Page Format

```
WAL Page (typically 8KB, independent of DB page size):
┌─────────────────────────────────────────────────────────────┐
│ WAL Page Header (32 bytes)                                  │
│ ├─ magic: 'WALP'                                           │
│ ├─ page_num: Page sequence number                           │
│ ├─ start_lsn: First LSN on page                             │
│ ├─ crc32: Page CRC                                          │
├─────────────────────────────────────────────────────────────┤
│ WAL Record 1                                                │
│ ├─ Record Header (24 bytes)                                │
│ ├─ Record Data (variable)                                  │
│ ├─ Padding to 8-byte alignment                             │
├─────────────────────────────────────────────────────────────┤
│ WAL Record 2                                                │
├─────────────────────────────────────────────────────────────┤
│ ...                                                         │
├─────────────────────────────────────────────────────────────┤
│ WAL Record N                                                │
├─────────────────────────────────────────────────────────────┤
│ (unused space)                                              │
└─────────────────────────────────────────────────────────────┘
```

### Log Sequence Number (LSN)

```cpp
// 64-bit LSN
// Upper 32 bits: WAL file ID (segment number)
// Lower 32 bits: Offset within file

LSN encoding:
├─ File ID (32 bits) ─┼─ Offset (32 bits) ─┤
│  0x00000000 - ...   │  0x00000000 - ...  │

Example:
- LSN 0/1000000: File 0, offset 0x1000000 (16MB)
- LSN 1/8000000: File 1, offset 0x8000000 (128MB)
```

### WAL File Naming

```
WAL files in pg_wal/ directory:

000000010000000000000001  // First file
000000010000000000000002  // Second file
│││││││││││││││││││││││││
│└────┴────┴────┴────┘│││  Timeline ID (hex)
└───────┴─────────────┘││  Logical file number
                       └┘   Segment (256MB each)

Format: TTTTTTTTLLLLLLLLLLLLLLLLSS
- T: Timeline (8 hex digits)
- L: Log segment (16 hex digits)
- S: Segment (2 hex digits, usually 00-FF for 4GB segments)
```

### Recovery Algorithm

```
Algorithm: recoverFromWAL(checkpoint_lsn)

1. // Find checkpoint
2. checkpoint = readCheckpointRecord(checkpoint_lsn)
3. 
4. // Start from checkpoint LSN
5. current_lsn = checkpoint.redo_lsn
6. 
7. WHILE current_lsn < latest_lsn:
8.     record = readWALRecord(current_lsn)
9.     
10.    SWITCH record.type:
11.        CASE HEAP_INSERT:
12.            redoHeapInsert(record)
13.        CASE HEAP_UPDATE:
14.            redoHeapUpdate(record)
15.        CASE HEAP_DELETE:
16.            redoHeapDelete(record)
17.        CASE XLOG_COMMIT:
18.            // Mark transaction committed in CLOG
19.            clog->setStatus(record.xid, COMMITTED)
20.        CASE XLOG_ABORT:
21.            // Mark transaction aborted
22.            clog->setStatus(record.xid, ABORTED)
23.        // ... other types
24.    
25.    current_lsn = record.next_lsn
26.
27. // Recovery complete
28. createCheckpoint()  // Fresh start point
```

## Invariants

1. **LSN Monotonicity**: Records written in increasing LSN order
   - Verification: Assert next_lsn > current_lsn
   
2. **Page LSN**: Each page records LSN of last modifying record
   - Verification: Set on every page modification
   
3. **WAL Before Data**: WAL flushed before dirty page
   - Verification: fsync WAL before marking page clean

## Performance Considerations

### WAL Volume
- **Typical**: 10-30% of data volume
- **Full page writes**: After crash, full pages written to WAL
- **Compression**: Future optimization

### WAL Flush Strategy
- **Synchronous**: fsync on each commit (safest, slowest)
- **Asynchronous**: Group commits, periodic fsync
- **Trade-off**: Durability vs performance

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_wal_format.cpp` | Record structure |
| `tests/unit/test_wal_recovery.cpp` | Crash recovery |
| `tests/unit/test_wal_checksum.cpp` | CRC validation |

## Related Specifications

- [Checkpoint](./checkpoint.md) - Checkpoint records
- [Transaction Lifecycle](./transaction_lifecycle.md) - Commit/abort

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
