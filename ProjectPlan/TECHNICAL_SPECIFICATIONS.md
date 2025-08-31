# ScratchBird Technical Specifications

## Page Layout Specifications

### Common Page Header (64 bytes)
```
Offset  Size  Field
0       8     Page ID (unique within database)
8       8     LSN (Log Sequence Number for WAL)
16      4     Checksum (CRC32C)
20      2     Page Type (see below)
22      2     Page Flags
24      2     Free Space Start
26      2     Free Space End  
28      2     Special Space Offset
30      2     Page Version
32      8     Transaction ID (last modifier)
40      8     Previous Page ID (for chains)
48      8     Next Page ID (for chains)
56      8     Reserved for future use
```

### Page Types
```
0x01  DATA_PAGE         - Heap tuple data
0x02  BTREE_LEAF        - B-tree leaf page
0x03  BTREE_INTERNAL    - B-tree internal page
0x04  BLOB_PAGE         - Large object data
0x05  TIP_PAGE          - Transaction Inventory Page
0x06  PIP_PAGE          - Page Inventory Page  
0x07  SCN_PAGE          - System Change Number page
0x08  ROOT_PAGE         - Database root page
0x09  HEADER_PAGE       - Segment header page
0x0A  HASH_BUCKET       - Hash index bucket page
0x0B  HASH_OVERFLOW     - Hash index overflow page
0x0C  HASH_BITMAP       - Hash index bitmap page
0x0D  BITMAP_PAGE       - Bitmap index page
0x0E  BITMAP_META       - Bitmap index metadata
0x0F  GIN_DATA          - GIN posting list page
0x10  GIN_ENTRY         - GIN entry tree page
0x11  GIN_META          - GIN metadata page
0x12  RTREE_NODE        - R-tree node (internal/leaf)
0x13  RTREE_META        - R-tree metadata page
0x14  LSM_L0            - LSM tree level 0 (memtable flush)
0x15  LSM_LN            - LSM tree level N (compacted)
0x16  LSM_META          - LSM metadata page
0x17  COLUMN_DATA       - Columnstore data page
0x18  COLUMN_META       - Columnstore metadata
0x19  COLUMN_DICT       - Columnstore dictionary
0x1A  TTL_INDEX         - TTL index page
0x1B  TTL_META          - TTL metadata page
```

### Data Page Layout
```
[Page Header - 64 bytes]
[Line Pointer Array - grows down]
[Free Space]
[Tuple Data - grows up]
[Special Space - optional]
```

Line Pointer (4 bytes):
- 15 bits: Offset to tuple
- 15 bits: Tuple length  
- 2 bits: Flags (NORMAL, DEAD, REDIRECT)

### Tuple Header (24 bytes minimum)
```
Offset  Size  Field
0       8     Transaction ID (xmin - creator)
8       8     Transaction ID (xmax - deleter/updater)
16      4     Command ID (cid)
20      2     Number of attributes
22      2     Null bitmap offset (0 if no nulls)
```

### B-tree Index Page Layout
```
[Page Header - 64 bytes]
[B-tree Special - 16 bytes]
  - High key offset
  - Number of items
  - Level (0 = leaf)
  - Flags (LEAF, ROOT, DELETED, HAS_GARBAGE)
[Item ID Array - grows down]
[Free Space]
[Index Tuples - grows up]
```

### Hash Index Page Layout
```
HASH_BUCKET page:
[Page Header - 64 bytes]
[Hash Special - 16 bytes]
  - Bucket number
  - Hash function version
  - Max bucket
  - High mask
  - Low mask
[Hash Items - fixed size slots]
[Overflow pointer if needed]

HASH_OVERFLOW page:
[Page Header - 64 bytes]
[Overflow chain pointer]
[Hash Items continued]
```

### Bitmap Index Page Layout
```
BITMAP_PAGE:
[Page Header - 64 bytes]
[Bitmap Header - 32 bytes]
  - Start row ID
  - End row ID
  - Compression type
  - Word size
[Compressed bitmap data]

BITMAP_META:
[Page Header - 64 bytes]
[Number of distinct values]
[Value-to-bitmap mappings]
```

### GIN Index Page Layout
```
GIN_ENTRY page (B-tree of unique values):
[Page Header - 64 bytes]
[Entry tree items]
[Pointers to posting lists]

GIN_DATA page (posting lists):
[Page Header - 64 bytes]
[Compressed TID lists]
[Continuation pointers]
```

### R-tree Index Page Layout  
```
RTREE_NODE:
[Page Header - 64 bytes]
[R-tree Special - 32 bytes]
  - Level (0 = leaf)
  - Number of entries
[Entries - each contains:]
  - MBR (Minimum Bounding Rectangle)
  - Child pointer or TID
```

### LSM Tree Page Layout
```
LSM_L0 (hot - uncompressed):
[Page Header - 64 bytes]
[Sorted key-value pairs]
[Bloom filter]

LSM_LN (cold - compressed):
[Page Header - 64 bytes]
[Compression metadata]
[Block index]
[Compressed blocks]
```

### Columnstore Page Layout
```
COLUMN_DATA:
[Page Header - 64 bytes]
[Column Special - 16 bytes]
  - Column ID
  - Compression type
  - Min/max values
  - Null count
[Compressed column values]

COLUMN_DICT:
[Page Header - 64 bytes]
[Dictionary entries]
[Value IDs mapping]
```

### BLOB Page Layout
```
[Page Header - 64 bytes]
[BLOB Header - 32 bytes]
  - Total BLOB size
  - Chunk number  
  - Chunks total
  - Compression type
[BLOB Data]
```

## Index Type Selection Guidelines

### When to Use Each Index Type

**B-tree** (Default):
- General purpose, sorted access
- Range queries, ORDER BY
- Unique constraints
- Primary keys

**Hash**:
- Equality lookups only (=)
- No range support
- Very fast for point queries
- Fixed bucket count (rebuild to resize)

**Bitmap**:
- Low cardinality columns (<1000 distinct values)
- Data warehouse workloads
- Multiple bitmap indexes can be combined
- Excellent compression

**GIN** (Generalized Inverted Index):
- Full-text search
- Array contains operations
- JSON/JSONB queries
- Multi-valued attributes

**R-tree**:
- Spatial data (points, lines, polygons)
- Geographic queries
- Multidimensional ranges
- Nearest neighbor searches

**LSM** (Log-Structured Merge):
- Write-heavy workloads
- Time-series data
- Append-mostly tables
- Can sacrifice some read performance

**Columnstore**:
- Analytics queries
- Aggregations over few columns
- Compression benefits
- Not for OLTP

**TTL** (Time-To-Live):
- Automatic data expiration
- Session data
- Temporary caches
- Compliance (data retention)

## File Organization

### Database File Structure
```
database.sbd (main file):
[File Header - 8KB minimum]
  - Magic number: "SCRATCHBIRD\x01"
  - Version
  - Page size
  - Database ID (UUID v7)
  - Creation timestamp
[Root Page]
[System Catalog Pages]
[Data Pages...]
```

### Multi-File Support
```
database.sbd      - Main file
database.sbd.1    - Overflow file 1 (when > 1GB)
database.sbd.2    - Overflow file 2
database.wal      - Write-ahead log
database.tip      - Transaction inventory (separate file)
```

## Directory Structure

### Source Code Organization
```
src/
├── include/
│   └── scratchbird/
│       ├── engine/
│       ├── parser/
│       └── common/
├── engine/
│   ├── storage/
│   │   ├── page/
│   │   ├── heap/
│   │   ├── index/
│   │   └── blob/
│   ├── transaction/
│   │   ├── mga/
│   │   ├── lock/
│   │   └── deadlock/
│   ├── buffer/
│   │   ├── pool/
│   │   └── cache/
│   ├── catalog/
│   └── access/
├── parser/
│   ├── sql/
│   ├── blr/
│   └── dialect/
├── executor/
│   ├── plan/
│   ├── operator/
│   └── expression/
├── network/
│   ├── protocol/
│   └── listener/
└── utils/
    ├── uuid/
    └── checksum/
```

### Test Organization
```
tests/
├── phase_1_01/
│   ├── test_page_creation.cpp
│   ├── test_file_structure.cpp
│   └── test_all_page_sizes.cpp
├── phase_1_02/
│   └── ...
└── common/
    └── test_helpers.h
```

## Critical Design Decisions

### 1. Endianness
- **Little-endian** for all on-disk structures
- Network byte order for wire protocol

### 2. Character Encoding  
- **UTF-8** for all string data
- Collation sequences stored in catalog

### 3. Identifiers
- **UUID v7** for all database objects
- Stored as 16-byte binary
- Time-ordered for better index locality

### 4. Page Size Support
Must support all sizes with same code:
- 8KB (8192 bytes)
- 16KB (16384 bytes)  
- 32KB (32768 bytes)
- 64KB (65536 bytes)
- 128KB (131072 bytes)

### 5. Limits
- Max database size: 256TB
- Max table size: 32TB
- Max row size: 65KB (excluding BLOBs)
- Max BLOB size: 4GB
- Max columns per table: 1600
- Max index key size: 2KB

### 6. MGA Requirements
Every tuple must have from day one:
- xmin (transaction that created)
- xmax (transaction that deleted/updated)
- Command ID for statement visibility
- Pointer to previous version (once MGA active)

## Implementation Guidelines

### DO Specify:
- Data structures that cross module boundaries
- File formats and wire protocols
- Public APIs and interfaces
- Critical algorithms (MGA, WAL, B-tree)
- Error codes and handling strategies

### DON'T Specify:
- Private class implementations
- Local optimization choices
- Memory management details (beyond requirements)
- Thread pool implementations
- Specific C++ patterns (unless critical)

### Let AI Decide:
- Class hierarchies and design patterns
- Memory allocation strategies
- Caching algorithms (beyond LRU requirement)
- Lock implementation details
- Buffer management internals

## Validation Requirements

Every implementation MUST:
1. Pass all 5 page size tests
2. Verify file structure with hex dump
3. Validate checksums on every page read
4. Handle power failure gracefully
5. Support concurrent access from day one
6. Build with -Wall -Werror

## Progress Tracking

Implementation adds to log:
```
2024-01-15 14:00 1.01.1 CREATED page/page_header.h
2024-01-15 14:30 1.01.1 IMPLEMENTED PageHeader for 8K pages
2024-01-15 15:00 1.01.1 TESTED PageHeader checksum validation PASS
```

## Notes for AI Implementers

1. **Start Simple**: Get basic version working, then optimize
2. **Test First**: Write test before implementation
3. **Document Why**: Comments should explain decisions
4. **Fail Fast**: Validate inputs, assert invariants
5. **Think Concurrent**: Design for multi-threading from start
6. **Profile Later**: Correctness first, performance second