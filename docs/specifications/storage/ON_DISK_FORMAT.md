# ScratchBird On-Disk Format Specification
## AUTHORITATIVE - This defines the exact byte-level format

**MGA Reference:** See `MGA_RULES.md` for Multi-Generational Architecture semantics (visibility, TIP usage, recovery).
**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.

### Version History
- v1.0.0 - Initial specification for Alpha 1.01
- v1.1.0 - Stage 1.1: Added 64KB/128KB page support with extended structures
- v1.2.0 - Stage 1.1: Added compression support with LZ4 baseline
- v1.3.0 - Stage 1.1: Added TOAST/LOB storage for large attributes
- v1.4.0 - Stage 1.1: Added table_id to PageHeader (80 bytes total)
- v1.5.0 - Stage 1.2: Tablespace header v2 and root_gpid catalog fields

---

## Critical Rules

1. **All multi-byte integers are LITTLE-ENDIAN**
2. **All structures are aligned to 8-byte boundaries**
3. **All text is UTF-8 encoded**
4. **All checksums are CRC32C (Castagnoli polynomial)**
5. **All UUIDs are version 7 (time-ordered)**
6. **Heap pages MUST have non-zero table_id; zero table_id is corruption**

---

## Page Layout

### Page Header (80 bytes) - EVERY page starts with this

```c
#pragma pack(push, 1)  // Ensure no padding

typedef struct PageHeader {
    // Identification (16 bytes)
    uint32_t magic;           // 0x00: Must be 0x53425244 ('SBRD')
    uint16_t version;         // 0x04: Format version (1 for Alpha)
    uint16_t page_type;       // 0x06: PageType enum value
    uint32_t page_size;       // 0x08: 8192|16384|32768|65536|131072 (Stage 1.1 added 64K/128K)
    uint32_t checksum;        // 0x0C: CRC32C of bytes [0x10..page_size)
    
    // Location (16 bytes)
    uint64_t lsn;            // 0x10: Log Sequence Number (0 if no optional post-gold WAL)
    uint32_t page_id;        // 0x18: Page number in file (0-based)
    uint32_t flags;          // 0x1C: Page-specific flags
    
    // Identity (32 bytes)
    uint8_t  database_uuid[16]; // 0x20: Database UUID (v7)
    uint8_t  table_id[16];      // 0x30: Table UUID (v7) (0 for non-heap pages)
    
    // MVCC (16 bytes)
    uint64_t generation;     // 0x40: Page generation for MVCC
    uint16_t free_space;     // 0x48: Bytes of free space
    uint16_t item_count;     // 0x4A: Number of items on page
    uint16_t free_offset;    // 0x4C: Offset to start of free space
    uint16_t special_size;   // 0x4E: Size of special area at page end
} PageHeader;  // Total: 80 bytes EXACTLY

#pragma pack(pop)

// Page types
enum PageType {
    PAGE_TYPE_DATABASE_HEADER = 0,  // Page 0 only
    PAGE_TYPE_SYSTEM_CATALOG  = 1,  // Page 1 only
    PAGE_TYPE_FREE_SPACE_MAP  = 2,  // FSM pages
    PAGE_TYPE_HEAP            = 3,  // Data pages
    PAGE_TYPE_BTREE_META      = 4,  // B-tree metadata
    PAGE_TYPE_BTREE_INTERNAL  = 5,  // B-tree internal nodes
    PAGE_TYPE_BTREE_LEAF      = 6,  // B-tree leaf nodes
    PAGE_TYPE_TRANSACTION_MAP = 7,  // TIP pages
    PAGE_TYPE_CATALOG_ROOT    = 8,  // System catalog root page
    // ... more types
};

// Page flags (bitwise OR)
#define PAGE_FLAG_DIRTY      0x0001  // Page has uncommitted changes
#define PAGE_FLAG_PINNED     0x0002  // Page is pinned in buffer
#define PAGE_FLAG_COMPRESSED 0x0004  // Page data is compressed
#define PAGE_FLAG_ENCRYPTED  0x0008  // Page data is encrypted
```

### Checksum Calculation (EXACT Algorithm)

```c
#include <crc32c/crc32c.h>  // Hardware-accelerated CRC32C

uint32_t calculate_page_checksum(const uint8_t* page, uint32_t page_size) {
    // CRITICAL: The checksum field itself (bytes 0x0C-0x0F) MUST be excluded
    uint32_t crc = 0xFFFFFFFF;  // Initial value per CRC32C spec
    
    // Process header before checksum field
    crc = crc32c_append(crc, page, 12);  // Bytes 0x00-0x0B
    
    // Process everything after checksum field
    crc = crc32c_append(crc, page + 16, page_size - 16);  // Bytes 0x10-end
    
    return crc ^ 0xFFFFFFFF;  // Final XOR per CRC32C spec
}

bool validate_page_checksum(const uint8_t* page, uint32_t page_size) {
    PageHeader* header = (PageHeader*)page;
    uint32_t stored = header->checksum;
    uint32_t calculated = calculate_page_checksum(page, page_size);
    return stored == calculated;
}
```

---

## Database Header Page (Page 0)

Page 0 is special - it contains database-wide metadata:

```c
typedef struct DatabaseHeader {
    PageHeader page_header;      // Standard 80-byte header
    
    // Database identification (64 bytes)
    char     db_name[32];        // Database name (null-terminated)
    uint32_t db_version;         // ScratchBird version that created DB
    uint32_t db_compat_version;  // Minimum version that can read DB
    uint64_t creation_time;      // Unix timestamp (microseconds)
    uint64_t last_checkpoint;    // Last checkpoint timestamp
    uint64_t reserved1[2];       // Reserved for future use
    
    // Configuration (32 bytes)
    uint32_t block_size;         // Must match page_header.page_size
    uint32_t wal_level;          // write-after log (WAL) level (0=none for Alpha; reserved for optional post-gold WAL)
    uint32_t max_connections;    // Maximum connections
    uint32_t encoding;           // Database encoding (UTF8=1)
    uint32_t locale;             // Locale ID
    uint32_t timezone;           // Timezone offset
    uint32_t reserved2[2];       // Reserved
    
    // File layout (32 bytes)
    uint64_t total_pages;        // Total pages in main file
    uint64_t free_pages;         // Number of free pages
    uint64_t next_page_id;       // Next page ID to allocate
    uint64_t system_catalog_page; // Root of system catalog (usually 1)
    
    // Transaction info (32 bytes)
    uint64_t next_transaction_id; // Next transaction ID to assign
    uint64_t oldest_active_xid;   // Oldest active transaction
    uint64_t latest_completed_xid; // Latest completed transaction
    uint64_t reserved3[1];        // Reserved
    
    // Checksums for critical data (16 bytes)
    uint32_t catalog_checksum;   // Checksum of system catalog
    uint32_t reserved4[3];       // Reserved
    
    // Padding to page boundary
    uint8_t  padding[];          // Fill to page_size
} DatabaseHeader;

// Validation
bool validate_database_header(const DatabaseHeader* header) {
    // Check magic
    if (header->page_header.magic != 0x53425244) return false;
    
    // Check page type
    if (header->page_header.page_type != PAGE_TYPE_DATABASE_HEADER) return false;
    
    // Check page ID
    if (header->page_header.page_id != 0) return false;
    
    // Check block size consistency
    if (header->block_size != header->page_header.page_size) return false;
    
    // Check supported page sizes (Stage 1.1 added 64KB and 128KB)
    if (header->block_size != 8192 &&
        header->block_size != 16384 &&
        header->block_size != 32768 &&
        header->block_size != 65536 &&
        header->block_size != 131072) return false;
    
    // Validate checksum
    return validate_page_checksum((uint8_t*)header, header->block_size);
}
```

---

## Tablespace Files (.sbts)

Each tablespace file begins with a tablespace header on page 0. The header version is stored
in the PageHeader `version` field. Tablespace header v2 expands the name to 63 characters
and preserves the v1 layout for remaining fields.

### Tablespace Header (Page 0)

```c
#pragma pack(push, 1)
typedef struct TablespaceHeaderV2 {
    PageHeader page_header;       // Standard 80-byte header

    // Identification (96 bytes)
    char     tablespace_name[64]; // Null-terminated, max 63 chars
    uint8_t  tablespace_uuid[16]; // UUID v7 for tablespace
    uint8_t  database_uuid[16];   // UUID v7 for owning database

    // Configuration (64 bytes)
    uint32_t tablespace_id;       // 0=primary, 1=reserved, 2..65535 custom
    uint32_t page_size;           // Must match database page_size
    uint64_t creation_time;       // Unix timestamp (microseconds)
    uint64_t last_checkpoint;     // Last checkpoint timestamp (microseconds)
    uint32_t autoextend_enabled;  // 1=enabled, 0=disabled
    uint32_t autoextend_size_mb;  // Extend by N MB
    uint64_t max_size_mb;         // 0 = unlimited
    uint64_t reserved1[3];        // Reserved

    // File layout (32 bytes)
    uint64_t total_pages;         // Total pages in file
    uint64_t free_pages;          // Free pages tracked by FSM
    uint64_t next_page_number;    // Allocation hint
    uint64_t fsm_root_page;       // FSM root page (usually 1)

    // Transaction info (32 bytes)
    uint64_t oldest_transaction_id;
    uint64_t latest_completed_xid;
    uint64_t reserved2[2];
} TablespaceHeaderV2;
#pragma pack(pop)
```

### Tablespace Header Validation Rules

- `page_header.magic` must equal `0x53425244` (`SBRD`)
- `tablespace_id` must match the catalog entry for this tablespace
- `page_size` must match the database page size
- `database_uuid` must match the database UUID unless attach override is used

### Root Page Identifiers

Catalog records store root page identifiers as **root_gpid** instead of root_page. A root_gpid
is a 64-bit Global Page ID: upper 16 bits = tablespace_id, lower 48 bits = page number.
All storage code must use GPIDs for root pages and DML TIDs.

---

## Heap Page Layout

Data pages store tuples (rows):

```c
typedef struct HeapPage {
    PageHeader page_header;      // Standard 80-byte header
    ItemPointer items[];         // Array of item pointers
    // ... free space ...
    // ... tuple data grows backward from end ...
    HeapPageSpecial special;     // Special area at page end
} HeapPage;

// Special area at the end of heap pages (Stage 0)
typedef struct HeapPageSpecial {
    uint16_t pd_flags;      // Page flags
    uint16_t pd_lower;      // Offset to start of free space
    uint16_t pd_upper;      // Offset to end of free space  
    uint16_t pd_special;    // Offset to start of special area
    uint64_t pd_prune_xid;  // Oldest XID for pruning
} HeapPageSpecial;

// Stage 1.1 Extended HeapPageSpecial for pages > 64KB
typedef struct HeapPageSpecialExtended {
    uint16_t pd_flags;      // Page flags
    uint16_t reserved;      // Reserved for alignment
    uint32_t pd_lower;      // Offset to start of free space (supports up to 4GB)
    uint32_t pd_upper;      // Offset to end of free space
    uint32_t pd_special;    // Offset to start of special area
    uint64_t pd_prune_xid;  // Oldest XID for pruning
} HeapPageSpecialExtended;

// Item pointer - points to tuple on page
// NOTE: Stage 1.1 extends this to 32-bit fields for 64KB/128KB page support
typedef struct ItemPointer {
    uint16_t offset;    // Offset from page start (must be >= sizeof(PageHeader))
    uint16_t length;    // Length of tuple data  
    uint16_t flags;     // Item flags
} ItemPointer;

// Stage 1.1 Extended ItemPointer for pages > 64KB
typedef struct ItemPointerExtended {
    uint32_t offset;    // Offset from page start (supports up to 4GB pages)
    uint32_t length: 31;// Length of tuple data (max ~2GB)
    uint32_t flags: 1;  // 0 = normal, 1 = deleted
} ItemPointerExtended;

// Item flags
#define ITEM_FLAG_NORMAL  0x0000  // Normal tuple
#define ITEM_FLAG_DELETED 0x0001  // Marked for deletion
#define ITEM_FLAG_LOCKED  0x0002  // Locked by transaction
#define ITEM_FLAG_UPDATED 0x0004  // Has been updated

// Tuple header (every tuple starts with this)
typedef struct TupleHeader {
    uint32_t t_xmin;         // Insert transaction ID
    uint32_t t_xmax;         // Delete/update transaction ID (or 0)
    uint32_t t_cid;          // Command ID within transaction
    uint16_t t_infomask;     // Tuple flags
    uint16_t t_natts;        // Number of attributes
    uint32_t t_bits[];       // Null bitmap (1 bit per attribute)
    // ... followed by tuple data ...
} TupleHeader;
```

### Page Organization Diagram

```
+------------------+ 0x0000 (Page Start)
|   Page Header    | 80 bytes
+------------------+ 0x0050
|  Item Pointer[0] | 6 bytes
|  Item Pointer[1] | 6 bytes
|      ...         |
|  Item Pointer[n] | 6 bytes
+------------------+ 
|                  |
|   Free Space     | (grows down)
|                  |
+------------------+ free_offset
|                  |
|   Tuple Data     | (grows up)
|                  |
+------------------+
| Special Area     | (optional, e.g., for indexes)
+------------------+ page_size
```

---

## UUID v7 Format

Alpha MUST use UUID v7 (time-ordered) exclusively:

```c
typedef struct UUIDv7 {
    uint32_t timestamp_high;    // Unix timestamp (seconds) high 32 bits
    uint16_t timestamp_low;     // Unix timestamp low 16 bits
    uint16_t random_a;          // Random bits and version (0b0111xxxx)
    uint16_t random_b;          // Random bits and variant (0b10xxxxxx)
    uint8_t  random_c[6];       // Random bits
} UUIDv7;

UUIDv7 generate_uuid_v7() {
    UUIDv7 uuid;
    
    // Get current Unix timestamp in milliseconds
    uint64_t timestamp_ms = get_unix_timestamp_ms();
    
    // Split timestamp into fields
    uuid.timestamp_high = (timestamp_ms >> 16) & 0xFFFFFFFF;
    uuid.timestamp_low = timestamp_ms & 0xFFFF;
    
    // Set version (7) in high bits of random_a
    uuid.random_a = (get_random_uint16() & 0x0FFF) | 0x7000;
    
    // Set variant (10) in high bits of random_b
    uuid.random_b = (get_random_uint16() & 0x3FFF) | 0x8000;
    
    // Fill remaining with random
    get_random_bytes(uuid.random_c, 6);
    
    return uuid;
}
```

---

## File Structure

### Main Database File

```
test.db:
  Page 0: Database Header
  Page 1: System Catalog Root
  Page 2: Free Space Map
  Page 3+: Data/Index pages
```

### File Naming Convention

```
test.db          - Main database file
test.db.wal      - Write-after log (WAL, optional post-gold) (future)
test.db.1        - Segment 1 when file > 1GB
test.db.2        - Segment 2
test.db.lock     - Lock file (contains PID)
```

---

## Validation Requirements

Every page operation MUST:

1. **On Read**:
   - Verify magic number
   - Validate checksum
   - Check page_id matches expected
   - Verify page_type is valid

2. **On Write**:
   - Update generation number
   - Recalculate checksum
   - Set dirty flag
   - Update LSN (only when optional write-after log (WAL) is enabled post-gold)

3. **Error Handling**:
   ```c
   Status read_page(int fd, uint32_t page_id, void* buffer, uint32_t page_size) {
       // Seek to page
       off_t offset = (off_t)page_id * page_size;
       if (lseek(fd, offset, SEEK_SET) != offset) {
           return SB_ERR_IO_ERROR;
       }
       
       // Read page
       ssize_t bytes = read(fd, buffer, page_size);
       if (bytes != page_size) {
           return SB_ERR_IO_ERROR;
       }
       
       // Validate
       PageHeader* header = (PageHeader*)buffer;
       
       if (header->magic != 0x53425244) {
           return SB_ERR_PAGE_CORRUPT;
       }
       
       if (header->page_id != page_id) {
           return SB_ERR_PAGE_CORRUPT;
       }
       
       if (!validate_page_checksum(buffer, page_size)) {
           return SB_ERR_CHECKSUM_MISMATCH;
       }
       
       return SB_OK;
   }
   ```

## Variable-Length Structures and Limits

- ItemPointer array starts immediately after PageHeader and grows downward into free space as tuples are added.
- Maximum number of ItemPointers per page is implementation-defined by available free space:
  - max_item_pointers = floor((free_offset - sizeof(PageHeader)) / sizeof(ItemPointer))
- When insufficient space exists to add a new ItemPointer or tuple payload:
  - Return SB_ERR_PAGE_FULL and let the caller allocate a new page (Alpha). Splitting/compaction policies will be introduced later.
- System Catalog Growth:
  - The system catalog root may point to additional catalog pages when full; Alpha may store only minimal entries and expand in later phases.

---

## Implementation Checklist

- [ ] Define all structures with exact byte layout
- [ ] Implement CRC32C checksum (hardware-accelerated if available)
- [ ] Implement UUID v7 generator
- [ ] Create page read/write functions with validation
- [ ] Add comprehensive tests for each page type
- [ ] Document any platform-specific considerations

---

## Platform Considerations

### Linux
- Use `O_DIRECT` for bypassing page cache (optional)
- Use `posix_fadvise()` for read-ahead hints
- Use `fdatasync()` for durability

### macOS
- Use `fcntl(F_FULLFSYNC)` for durability
- No `O_DIRECT` equivalent

### Windows
- Use `FILE_FLAG_NO_BUFFERING` for direct I/O
- Use `FlushFileBuffers()` for durability

---

## Test Vectors

### Valid Page Header (8KB page)
```
Offset  Hex Values                                         ASCII
0x0000: 44 52 42 53 01 00 00 00 00 20 00 00 AB CD EF 12  DRBS..... ......
0x0010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x0020: 01 8b 9f 3a 7d 4e 7f 3a 9c 5d 12 34 56 78 90 ab  ...:}N.:].4Vx..
0x0030: 01 00 00 00 00 00 00 00 00 1F 01 00 40 00 00 00  ............@...
```

This represents:
- Magic: 0x53425244 ('SBRD' little-endian)
- Version: 1
- Page Type: 0 (DATABASE_HEADER)
- Page Size: 8192 (0x2000)
- Checksum: 0x12EFCDAB (example)

---

## Page Compression

### Overview
Pages can be transparently compressed to reduce I/O and storage requirements. Compression is applied at the page level during write operations and decompression occurs during read operations.

### Compression Header
When a page is compressed, the compressed data is prefixed with a compression header:

```c
#pragma pack(push, 1)
struct CompressedPageHeader {
    uint32_t uncompressed_size;  // Original page size
    uint32_t compressed_size;    // Compressed data size (excluding this header)
    uint8_t  compression_type;   // CompressionType enum value
    uint8_t  reserved[3];        // Reserved for alignment
    uint32_t checksum;          // CRC32C of compressed data
};
#pragma pack(pop)
```

### Compression Types
```c
enum class CompressionType : uint8_t {
    NONE = 0,       // No compression
    LZ4 = 1,        // LZ4 compression (baseline)
    ZSTD = 2,       // Zstandard compression (future)
    SNAPPY = 3,     // Snappy compression (future)
};
```

### Compressed Page Layout
When a page is compressed:
1. The PageHeader.flags has PAGE_FLAG_COMPRESSED (0x0004) set
2. After the PageHeader comes the CompressedPageHeader
3. After the CompressedPageHeader comes the compressed page data

```
[PageHeader - 80 bytes]
[CompressedPageHeader - 16 bytes]
[Compressed Data - variable size]
[Padding to page_size]
```

### Compression Rules
1. System pages (0-2) are NEVER compressed
2. Compression is only applied if it saves at least 10% space
3. The compressed page (including headers) must fit within page_size
4. Compression type must match what was configured for the database
5. Both compressed and uncompressed pages can coexist in the same database

### Checksum Calculation for Compressed Pages
1. The PageHeader checksum covers the entire page as normal
2. The CompressedPageHeader checksum covers only the compressed data:
   ```c
   uint32_t checksum = crc32c_compute(compressed_data, compressed_size, 0xFFFFFFFF) ^ 0xFFFFFFFF;
   ```

---

## TOAST (The Oversized-Attribute Storage Technique)

### Overview
TOAST allows storage of large attributes that exceed normal tuple size limits by storing them out-of-line in a separate TOAST table.

### TOAST Pointer
When a value is TOASTed, the main tuple contains a TOAST pointer instead of the actual data:

```c
#pragma pack(push, 1)
struct ToastPointer {
    uint8_t  va_header;      // 0x01 = TOAST marker
    uint8_t  va_tag;         // ToastStrategy value
    uint32_t va_rawsize;     // Original (uncompressed) data size
    uint32_t va_extsize;     // External stored size
    uint32_t va_valueid;     // Unique identifier for this TOAST value
    uint32_t va_toastrelid;  // TOAST table ID
};
#pragma pack(pop)
```

### TOAST Strategies
```c
enum class ToastStrategy : uint8_t {
    PLAIN = 0,      // Store inline (no TOAST)
    EXTENDED = 1,   // Store out-of-line, uncompressed
    COMPRESSED = 2, // Store inline, compressed (future)
    EXTERNAL = 3,   // Store out-of-line, compressed
};
```

### TOAST Chunk Format
Large values are split into chunks and stored in the TOAST table:

```
Chunk Tuple Format:
[chunk_id: 4 bytes][chunk_seq: 4 bytes][chunk_size: 4 bytes][chunk_data: variable]
```

### TOAST Rules
1. Values > 2000 bytes are candidates for TOASTing
2. Values > page_size/4 must be TOASTed
3. Chunks are limited to 1996 bytes each
4. TOAST tables are named `pg_toast_<UUID>`
5. Compression is optional (EXTERNAL strategy)

### TOAST Table Schema
- chunk_id (INT32): TOAST value identifier
- chunk_seq (INT32): Chunk sequence number (0-based)
- chunk_data (BYTEA): Actual chunk data

- UUID: 018b9f3a-7d4e-7f3a-9c5d-1234567890ab (v7)
