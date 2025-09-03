# ScratchBird On-Disk Format Specification
## AUTHORITATIVE - This defines the exact byte-level format

### Version History
- v1.0.0 - Initial specification for Alpha 1.01

---

## Critical Rules

1. **All multi-byte integers are LITTLE-ENDIAN**
2. **All structures are aligned to 8-byte boundaries**
3. **All text is UTF-8 encoded**
4. **All checksums are CRC32C (Castagnoli polynomial)**
5. **All UUIDs are version 7 (time-ordered)**

---

## Page Layout

### Page Header (64 bytes) - EVERY page starts with this

```c
#pragma pack(push, 1)  // Ensure no padding

typedef struct PageHeader {
    // Identification (16 bytes)
    uint32_t magic;           // 0x00: Must be 0x53425244 ('SBRD')
    uint16_t version;         // 0x04: Format version (1 for Alpha)
    uint16_t page_type;       // 0x06: PageType enum value
    uint32_t page_size;       // 0x08: 8192|16384|32768 ONLY in Alpha
    uint32_t checksum;        // 0x0C: CRC32C of bytes [0x10..page_size)
    
    // Location (16 bytes)
    uint64_t lsn;            // 0x10: Log Sequence Number (0 if no WAL)
    uint32_t page_id;        // 0x18: Page number in file (0-based)
    uint32_t flags;          // 0x1C: Page-specific flags
    
    // Identity (16 bytes)
    uint8_t  database_uuid[16]; // 0x20: Database UUID (v7)
    
    // MVCC (16 bytes)
    uint64_t generation;     // 0x30: Page generation for MVCC
    uint16_t free_space;     // 0x38: Bytes of free space
    uint16_t item_count;     // 0x3A: Number of items on page
    uint16_t free_offset;    // 0x3C: Offset to start of free space
    uint16_t special_size;   // 0x3E: Size of special area at page end
} PageHeader;  // Total: 64 bytes EXACTLY

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
    PageHeader page_header;      // Standard 64-byte header
    
    // Database identification (64 bytes)
    char     db_name[32];        // Database name (null-terminated)
    uint32_t db_version;         // ScratchBird version that created DB
    uint32_t db_compat_version;  // Minimum version that can read DB
    uint64_t creation_time;      // Unix timestamp (microseconds)
    uint64_t last_checkpoint;    // Last checkpoint timestamp
    uint64_t reserved1[2];       // Reserved for future use
    
    // Configuration (32 bytes)
    uint32_t block_size;         // Must match page_header.page_size
    uint32_t wal_level;          // WAL level (0=none for Alpha)
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
    
    // Check supported page sizes for Alpha
    if (header->block_size != 8192 && 
        header->block_size != 16384 && 
        header->block_size != 32768) return false;
    
    // Validate checksum
    return validate_page_checksum((uint8_t*)header, header->block_size);
}
```

---

## Heap Page Layout

Data pages store tuples (rows):

```c
typedef struct HeapPage {
    PageHeader page_header;      // Standard 64-byte header
    ItemPointer items[];         // Array of item pointers
    // ... free space ...
    // ... tuple data grows backward from end ...
} HeapPage;

// Item pointer - points to tuple on page
typedef struct ItemPointer {
    uint16_t offset;    // Offset from page start (must be >= sizeof(PageHeader))
    uint16_t length;    // Length of tuple data
    uint16_t flags;     // Item flags
} ItemPointer;

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
|   Page Header    | 64 bytes
+------------------+ 0x0040
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
test.db.wal      - Write-ahead log (future)
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
   - Update LSN (when WAL enabled)

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
- UUID: 018b9f3a-7d4e-7f3a-9c5d-1234567890ab (v7)