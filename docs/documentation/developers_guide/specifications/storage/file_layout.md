# Specification: Database File Layout

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird Alpha |
| **Authors** | Dalton Calford |

## Synopsis

This specification defines the on-disk file layout for ScratchBird databases, including the primary database file, tablespace files, and segment organization.

## Scope

### In Scope

- Primary database file structure
- Tablespace file format
- Bootstrap page layout
- Page numbering
- File extension

### Out of Scope

- WAL file format (see WAL Format spec)
- Temporary file layout
- Configuration file formats

## Background

ScratchBird uses:
- **Primary file** (.sbdb): Bootstrap pages, system tables, default tablespace
- **Tablespace files** (.sbts): User data in separate files
- **Fixed page size**: 8KB-128KB per database
- **Extent-based growth**: Files grow in chunks

## Specification

### Primary Database File (.sbdb)

```
Primary Database File Structure:
┌─────────────────────────────────────────────────────────────────┐
│ Bootstrap Pages (Fixed)                                         │
├─────────────────────────────────────────────────────────────────┤
│ Page 0: Database Header    │ DB metadata, version, page size    │
│ Page 1: System State       │ Clean shutdown, checkpoint info    │
│ Page 2: Catalog Root       │ System catalog B-tree root         │
│ Page 3: FSM Root           │ Free Space Map                     │
│ Page 4: TX Map Root        │ Transaction map (TIP root)         │
│ Page 5: Reserved           │ Reserved for future use            │
├─────────────────────────────────────────────────────────────────┤
│ User Data Pages (Dynamic)                                       │
├─────────────────────────────────────────────────────────────────┤
│ Heap pages, index pages, TOAST pages, etc.                      │
└─────────────────────────────────────────────────────────────────┘
```

#### Database Header (Page 0)

```cpp
struct DatabaseHeader {
    PageHeader page_header;     // Standard header
    
    // Database identification
    uint8_t magic[4];           // 'SBRD'
    uint32_t version;           // Format version
    uint32_t page_size;         // Page size in bytes
    uint8_t database_uuid[16];  // Database UUID
    
    // State
    uint64_t next_xid;          // Next XID to allocate
    uint64_t oldest_xid;        // OIT
    uint32_t catalog_version;   // Catalog schema version
    
    // File info
    uint64_t creation_time;     // Unix timestamp
    uint64_t system_identifier; // Unique system ID
    
    // Compatibility
    uint32_t alpha_version;     // ScratchBird Alpha version
};
```

### Tablespace File (.sbts)

```
Tablespace File Structure:
┌─────────────────────────────────────────────────────────────────┐
│ Page 0: Tablespace Header                                       │
├─────────────────────────────────────────────────────────────────┤
│ magic: 'SBTS'                                                   │
│ version: 1                                                      │
│ tablespace_id: N                                                │
│ page_size: matches database                                     │
│ autoextend: enabled/disabled                                    │
├─────────────────────────────────────────────────────────────────┤
│ Page 1: Tablespace FSM                                          │
├─────────────────────────────────────────────────────────────────┤
│ Allocation bitmap for this tablespace                           │
├─────────────────────────────────────────────────────────────────┤
│ Pages 2+: User data                                             │
└─────────────────────────────────────────────────────────────────┘
```

#### Tablespace Header

```cpp
struct TablespaceHeader {
    PageHeader page_header;
    
    uint8_t magic[4];           // 'SBTS'
    uint16_t version;           // Format version
    uint16_t page_size;         // Page size
    uint16_t tablespace_id;     // ID (1-65535)
    char name[32];              // Tablespace name
    
    // Autoextend settings
    uint8_t autoextend;         // Boolean
    uint32_t autoextend_size_mb;
    uint64_t max_size_mb;
    
    // Statistics
    uint32_t total_pages;
    uint32_t free_pages;
    
    // Link to database
    uint8_t database_uuid[16];
};
```

### Page Addressing

#### Global Page ID (GPID)

```cpp
// 64-bit GPID
// Bits 0-47: Page number (48 bits = 281 trillion pages)
// Bits 48-63: Tablespace ID (16 bits = 65536 tablespaces)

GPID encoding:
├─ Tablespace ID (16 bits) ─┼─ Page Number (48 bits) ─┤
│  0x0000 - 0xFFFF          │  0x000000000000 - 0xFFFFFFFFFFFF  │

Special values:
- GPID = 0: INVALID_GPID
- Tablespace 0: Primary database
- Tablespace 1-65535: User tablespaces
```

#### Page Numbering

```
Page Number Space:
├─ Tablespace 0 (Primary) ─┼─ Tablespace 1 ─┼─ Tablespace 2 ─┼─ ...
│  Page 0, 1, 2, ...       │  Page 0, 1...  │  Page 0, 1...  │
│  Bootstrap + User data   │  User data     │  User data     │

GPID Mapping:
- makeGPID(0, 100) = (0 << 48) | 100 = 100
- makeGPID(1, 0)   = (1 << 48) | 0 = 0x0001000000000000
- makeGPID(1, 100) = (1 << 48) | 100 = 0x0001000000000064
```

### File Extension

```
Algorithm: extendFile(tablespace_id, num_pages)

1. IF tablespace_id == 0:
2.     fd = primary_database_fd_
3. ELSE:
4.     fd = tablespace_fds_[tablespace_id]

5. current_size = getFileSize(fd)
6. new_size = current_size + (num_pages * page_size_)

7. // Check max size limit
8. IF max_size_mb > 0:
9.     IF new_size > max_size_mb * 1024 * 1024:
10.        RETURN SIZE_LIMIT_EXCEEDED

11. // Extend file
12. #ifdef __linux__
13.    fallocate(fd, 0, current_size, num_pages * page_size_)
14. #else
15.    ftruncate(fd, new_size)
16.    // Zero new pages
17.    zero_buf = allocate(page_size_)
18.    FOR i FROM 0 TO num_pages - 1:
19.        pwrite(fd, zero_buf, page_size_, 
20.               current_size + (i * page_size_))
21. #endif

22. // Update FSM
23. FOR p FROM current_size/page_size TO new_size/page_size - 1:
24.    fsm->markFree(p)  // New pages start as free

25. RETURN OK
```

## Invariants

1. **Page Size Consistency**: All files in database use same page size
   - Verification: Check on tablespace open
   
2. **UUID Consistency**: Tablespaces link to correct database
   - Verification: Compare database_uuid on open
   
3. **Page Alignment**: All I/O at page_size boundaries
   - Verification: Assert offset % page_size == 0
   
4. **Bootstrap Reserved**: Pages 0-5 never freed
   - Verification: FSM marks them allocated

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `CORRUPT_FILE` | Invalid magic | Reject open |
| `VERSION_MISMATCH` | File version mismatch | Migration or error |
| `SIZE_LIMIT` | Tablespace max size reached | Extend limit or add tablespace |
| `UUID_MISMATCH` | Tablespace from different DB | Reject attach |

## Performance Considerations

### File Layout Benefits
- **Tablespace isolation**: Different disks for different data
- **Parallel I/O**: Multiple files can be read concurrently
- **Size management**: Limit growth per tablespace

### Autoextend Strategy
- **Growth chunks**: 64MB default
- **Preallocation**: fallocate() for performance
- **Monitoring**: Alert at 80% full

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_file_layout.cpp` | Basic layout |
| `tests/unit/test_tablespace_files.cpp` | Tablespace I/O |
| `tests/unit/test_file_extension.cpp` | Autoextend |

## Related Specifications

- [Page Allocation](./page_allocation.md) - Using file space
- [Buffer I/O](./buffer_io.md) - Reading/writing files

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
