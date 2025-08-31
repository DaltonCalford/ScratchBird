# ScratchBird Technical Specifications

## Page Layout Specifications

### Common Page Header (96 bytes)
```
Offset  Size  Field
0       8     Page ID (unique within filespace)
8       8     LSN (Log Sequence Number for WAL)
16      4     Checksum (CRC32C)
20      2     Page Type (see below)
22      2     Page Flags (see below)
24      2     Free Space Start
26      2     Free Space End  
28      2     Special Space Offset
30      2     Page Version
32      8     Transaction ID (last modifier)
40      8     Previous Page ID (for chains)
48      8     Next Page ID (for chains)
56      16    Filespace UUID (location of this page)
72      8     Shadow LSN (last replicated LSN)
80      4     Replication Flags
84      4     Compression Type (for page-level compression)
88      8     Reserved for future use
```

### Page Flags (16 bits)
```
Bit  Flag
0    DIRTY           - Page modified since last flush
1    REPLICATED      - Page sent to shadow
2    WAL_LOGGED      - Changes are in WAL
3    COMPRESSED      - Page data is compressed
4    ENCRYPTED       - Page data is encrypted
5    PINNED          - Page pinned in buffer
6    HOT             - Frequently accessed page
7    COLD            - Candidate for archive
8    MIGRATING       - Being moved to different filespace
9    SHADOW_DIVERGED - Shadow page differs (promotion occurred)
10   CHECKSUM_VALID  - Checksum has been verified
11   PARTIAL_WRITE   - Page partially written (torn page)
12-15 Reserved
```

### Replication Flags (32 bits)
```
Bit  Flag
0-1  REPL_STATE      - 00=None, 01=Pending, 10=Sent, 11=Confirmed
2    KAFKA_LOGGED    - WAL sent to Kafka
3    SHADOW_ONLY     - Page exists only on shadow
4-7  SHADOW_COUNT    - Number of shadows (0-15)
8-15 PRIORITY        - Replication priority (0-255)
16-31 Reserved
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
0x1C  REPL_STATUS       - Replication status page
0x1D  SHADOW_MAP        - Shadow mapping page
0x1E  WAL_BUFFER        - Buffered WAL records
0x1F  KAFKA_CHECKPOINT  - Kafka offset tracking
0x20  FILESPACE_MAP     - Filespace mapping page
```

### Data Page Layout
```
[Page Header - 96 bytes]
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
[Page Header - 96 bytes]
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
[Page Header - 96 bytes]
[Hash Special - 16 bytes]
  - Bucket number
  - Hash function version
  - Max bucket
  - High mask
  - Low mask
[Hash Items - fixed size slots]
[Overflow pointer if needed]

HASH_OVERFLOW page:
[Page Header - 96 bytes]
[Overflow chain pointer]
[Hash Items continued]
```

### Bitmap Index Page Layout
```
BITMAP_PAGE:
[Page Header - 96 bytes]
[Bitmap Header - 32 bytes]
  - Start row ID
  - End row ID
  - Compression type
  - Word size
[Compressed bitmap data]

BITMAP_META:
[Page Header - 96 bytes]
[Number of distinct values]
[Value-to-bitmap mappings]
```

### GIN Index Page Layout
```
GIN_ENTRY page (B-tree of unique values):
[Page Header - 96 bytes]
[Entry tree items]
[Pointers to posting lists]

GIN_DATA page (posting lists):
[Page Header - 96 bytes]
[Compressed TID lists]
[Continuation pointers]
```

### R-tree Index Page Layout  
```
RTREE_NODE:
[Page Header - 96 bytes]
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
[Page Header - 96 bytes]
[Sorted key-value pairs]
[Bloom filter]

LSM_LN (cold - compressed):
[Page Header - 96 bytes]
[Compression metadata]
[Block index]
[Compressed blocks]
```

### Columnstore Page Layout
```
COLUMN_DATA:
[Page Header - 96 bytes]
[Column Special - 16 bytes]
  - Column ID
  - Compression type
  - Min/max values
  - Null count
[Compressed column values]

COLUMN_DICT:
[Page Header - 96 bytes]
[Dictionary entries]
[Value IDs mapping]
```

### BLOB Page Layout
```
[Page Header - 96 bytes]
[BLOB Header - 32 bytes]
  - Total BLOB size
  - Chunk number  
  - Chunks total
  - Compression type
[BLOB Data]
```

### Replication Status Page Layout
```
[Page Header - 96 bytes]
[Replication Status - 64 bytes]
  - Primary Database UUID (16 bytes)
  - Current LSN (8 bytes)
  - Last Replicated LSN (8 bytes)
  - Last Kafka Offset (8 bytes)
  - Shadow Count (4 bytes)
  - Replication Mode (4 bytes)
  - Lag Bytes (8 bytes)
  - Lag Time (8 bytes)
[Shadow Entries - each 128 bytes]
  - Shadow UUID (16 bytes)
  - Shadow Host (64 bytes)
  - Last Confirmed LSN (8 bytes)
  - State (4 bytes)
  - Lag (8 bytes)
  - Priority (4 bytes)
  - Flags (4 bytes)
  - Reserved (20 bytes)
```

### Shadow Map Page Layout
```
[Page Header - 96 bytes]
[Shadow Map Header - 32 bytes]
  - Map Version (8 bytes)
  - Entry Count (4 bytes)
  - Last Update (8 bytes)
  - Flags (4 bytes)
  - Reserved (8 bytes)
[Page Mappings - each 32 bytes]
  - Original Page ID (8 bytes)
  - Shadow Page ID (8 bytes)
  - Filespace UUID (16 bytes)
```

### WAL Buffer Page Layout
```
[Page Header - 96 bytes]
[WAL Buffer Header - 32 bytes]
  - First LSN (8 bytes)
  - Last LSN (8 bytes)
  - Record Count (4 bytes)
  - Total Size (4 bytes)
  - Compression (4 bytes)
  - Reserved (4 bytes)
[WAL Records - variable size]
  - Each prefixed with length (4 bytes)
  - WAL record data
```

### Kafka Checkpoint Page Layout
```
[Page Header - 96 bytes]
[Kafka Header - 64 bytes]
  - Topic Name (32 bytes)
  - Consumer Group (32 bytes)
[Partition Checkpoints - each 24 bytes]
  - Partition ID (4 bytes)
  - Offset (8 bytes)
  - Timestamp (8 bytes)
  - Flags (4 bytes)
```

### Filespace Map Page Layout
```
[Page Header - 96 bytes]
[Filespace Count - 4 bytes]
[Filespace Entries - each 512 bytes]
  - UUID (16 bytes)
  - Name (64 bytes)
  - Status (4 bytes): ONLINE/OFFLINE/READONLY/MAINTENANCE
  - OS Type (4 bytes)
  - Device ID (64 bytes)
  - Path (256 bytes)
  - Pattern (64 bytes)
  - Current Files (4 bytes)
  - Total Size (8 bytes)
  - Used Size (8 bytes)
  - Priority (4 bytes)
  - Shadow State (4 bytes)
  - Replication Lag (8 bytes)
  - Reserved (16 bytes)
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

### Tablespace/Filespace Support

#### Filespace Definition
```
Filespace = {
    UUID         - Unique identifier (UUID v7)
    Name         - User-friendly name
    OS           - Operating system (Windows/Linux/MacOS)
    Device       - Device/Volume identifier
    Path         - Full path to directory
    Pattern      - Filename pattern (e.g., "mydata_*.sbd")
    MaxFileSize  - Maximum size per file (default 1GB)
    AutoExtend   - Allow automatic file creation
    Priority     - Storage tier (1=fast SSD, 2=SSD, 3=HDD, 4=archive)
}
```

#### Default Filespaces
```
MAIN (Filespace 0):
  - Always exists, cannot be dropped
  - Contains system catalog, root page, TIP
  - Default location for all objects unless specified
  - Example: /data/scratchbird/main.sbd

TEMP (Filespace 1):
  - For temporary tables and sort operations
  - Can be on faster/volatile storage
  - Example: /ramdisk/scratchbird/temp.sbd

USER-DEFINED:
  - Custom filespaces for specific purposes
  - Can span multiple devices/paths
  - Examples:
    - FAST_INDEXES: /nvme/indexes/idx_*.sbd
    - ARCHIVE: /slowdisk/archive/old_*.sbd
    - LOGS: /logvolume/logs/log_*.sbd
```

#### File Naming Convention
```
Main filespace:
  [path]/database.sbd         - Primary file
  [path]/database.sbd.1       - Extension 1
  [path]/database.sbd.2       - Extension 2

Custom filespace:
  [path]/[pattern]            - User-defined pattern
  [path]/[pattern].1          - Extension 1
  [path]/[pattern].2          - Extension 2

Examples:
  Windows: C:\DBFiles\Production\sales.sbd
  Linux:   /mnt/fast-ssd/indexes/idx_customer.sbd
  Network: \\NAS\backup\archive\historical.sbd
```

#### Object Storage Assignment
```sql
-- Create tablespace
CREATE TABLESPACE fast_data 
  LOCATION '/nvme/data'
  PATTERN 'fast_*.sbd'
  MAXSIZE 10GB
  AUTOEXTEND ON;

-- Assign objects to tablespaces
CREATE TABLE orders (...) TABLESPACE fast_data;
CREATE INDEX idx_orders ON orders (...) TABLESPACE fast_indexes;
ALTER TABLE old_records SET TABLESPACE archive;

-- Move existing objects
ALTER TABLE customers SET TABLESPACE fast_data;
```

#### Filespace Metadata Page
```
FILESPACE_META page (in main file):
[Page Header - 96 bytes]
[Filespace Count - 4 bytes]
[Filespace Entries - each 512 bytes]:
  - UUID (16 bytes)
  - Name (64 bytes)
  - Status (4 bytes): ONLINE/OFFLINE/READONLY/MAINTENANCE
  - OS Type (4 bytes)
  - Device ID (64 bytes)
  - Path (256 bytes)
  - Pattern (64 bytes)
  - Current Files (4 bytes)
  - Total Size (8 bytes)
  - Used Size (8 bytes)
  - Priority (4 bytes)
  - Flags (4 bytes)
  - Reserved (24 bytes)
```

#### Cross-Filespace References
```
Extended Page ID (16 bytes):
  - Filespace UUID (16 bytes) OR
  - Filespace ID (2 bytes) + Page Number (6 bytes) + Reserved (8 bytes)

This allows any page to reference any other page in any filespace
```

#### Storage Tiering
```
Priority Levels:
  1 - Critical (NVMe/Optane)     - Hot indexes, active OLTP
  2 - Fast (SSD)                  - Normal operations
  3 - Standard (HDD RAID)         - Bulk data
  4 - Archive (HDD/Tape)          - Historical data
  5 - Remote (Network/Cloud)      - Backup/DR

Automatic Migration:
  - Statistics track page temperature
  - Background process migrates cold pages to slower storage
  - Hot pages promoted to faster storage
  - Transparent to queries
```

#### Detach/Attach Operations
```sql
-- Detach a tablespace (makes it portable)
ALTER TABLESPACE archive DETACH;
-- Creates archive.sbd.manifest with metadata

-- Attach a tablespace to different database
ALTER DATABASE ADD TABLESPACE archive 
  FROM '/backup/archive.sbd'
  READONLY;  -- Optional: attach as read-only

-- Clone a tablespace (for testing)
CREATE TABLESPACE test_data 
  AS COPY OF production_data
  LOCATION '/test/data';
```

#### Filespace Management Operations

##### Space Allocation Strategy
```
1. Object Creation:
   - Check if tablespace specified
   - If not, use default (MAIN)
   - Allocate initial extent in target filespace
   - Record in system catalog

2. Space Extension:
   - Try current filespace first
   - If full and AutoExtend=ON, create new file
   - If MaxFileSize reached, error or overflow to MAIN

3. Cross-Filespace Transactions:
   - Single transaction can span multiple filespaces
   - WAL records include filespace ID
   - Recovery replays to correct filespace
```

##### Monitoring and Maintenance
```sql
-- View filespace usage
SELECT * FROM sys.filespaces;
SELECT * FROM sys.filespace_usage;

-- Rebalance data across filespaces
ALTER TABLESPACE fast_data REBALANCE;

-- Shrink filespace (remove empty files)
ALTER TABLESPACE archive SHRINK;

-- Change filespace location (offline operation)
ALTER TABLESPACE old_data 
  SET LOCATION '/new/path'
  PATTERN 'newdata_*.sbd';
```

##### Backup and Recovery
```
Filespace-Aware Backup:
  - Can backup individual filespaces
  - Parallel backup of multiple filespaces
  - Point-in-time recovery per filespace

Example:
  BACKUP TABLESPACE fast_data TO '/backup/fast_data.sbk';
  RESTORE TABLESPACE fast_data FROM '/backup/fast_data.sbk'
    AS OF TIMESTAMP '2024-01-15 10:00:00';
```

##### Platform-Specific Paths
```
Windows:
  - Drive letters: C:\, D:\, E:\
  - UNC paths: \\server\share\path
  - Long paths: \\?\C:\very\long\path

Linux/Unix:
  - Mount points: /mnt/disk1, /data
  - Symbolic links supported
  - NFS mounts: /nfs/remote/path

Cloud Storage (future):
  - S3: s3://bucket/prefix/
  - Azure: azure://container/path/
  - GCS: gs://bucket/path/
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