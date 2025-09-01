# Phase 3: Page Management System

## Objective
Implement page-based storage with headers and checksums.

## Prerequisites
- Phase 2 complete (database lifecycle)

## Technical Specifications
- **Buffer Pool**: See `/references/technical_specifications/STORAGE_ENGINE_BUFFER_POOL.md`
- **Page Management**: See `/references/technical_specifications/STORAGE_ENGINE_PAGE_MANAGEMENT.md`
- **Storage Main**: See `/references/technical_specifications/STORAGE_ENGINE_MAIN.md`

## Tasks

### 3.1 Page Header Structure
Define in `include/scratchbird/engine/ods.h`:
```cpp
struct PageHeader {
    uint32_t checksum;
    uint16_t page_size;
    uint16_t type;
    uint32_t page_no;
    uint16_t space_id;
    uint16_t flags;
    uint32_t next;
    uint32_t prev;
};
```

### 3.2 Page Types
Define page type enumeration:
- `Undefined = 0`
- `HeapData = 1`
- `HeapRoot = 2`
- `Pip = 3` (Page Inventory)
- `Tip = 4` (Transaction Inventory)

### 3.3 Page I/O Operations
Implement in `src/engine/page_io.cpp`:
- `read_page(page_no) -> vector<uint8_t>`
- `write_page(page_no, data)`
- `allocate_page() -> page_no`
- `free_page(page_no)`

### 3.4 Checksum Validation
- Implement CRC32C checksum
- Validate on read
- Calculate on write

## Files to Create/Modify
- `include/scratchbird/engine/ods.h`
- `src/engine/page_io.cpp`

## Validation Tests
```cpp
// Write and read page
vector<uint8_t> page(8192);
write_page(db, 1, page);
auto read = read_page(db, 1);
assert(read == page);

// Checksum validation
page[0] = 0xFF;  // Corrupt
write_page(db, 2, page);
assert(validate_checksum(2) == false);
```

## Exit Criteria
- Pages written and read correctly
- Checksums detect corruption
- Page allocation tracking works