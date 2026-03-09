# Specification: Catalog Page Layouts

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:4502`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:298`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/page_manager.cpp`

## Synopsis

This specification defines the on-disk page layouts for catalog storage, including the CatalogRootPage structure, catalog heap pages, and page type definitions for all RDB$-style catalog tables.

## Scope

### In Scope

- CatalogRootPage layout and page pointers
- CatalogHeapPage structure
- Page type definitions for catalog pages
- Record layouts within catalog pages
- Overflow page chaining
- Page allocation and management

### Out of Scope

- User data page layouts (see storage specs)
- Index page structures (see index specs)
- TOAST page layouts (see TOAST spec)

## Specification

### Page Types

**Source:** `include/scratchbird/core/ondisk.h`

```cpp
enum class PageType : uint16_t {
    // Data pages
    HEAP = 0x0001,          // Heap table data
    INDEX_BTREE = 0x0002,   // B-tree index
    INDEX_HASH = 0x0003,    // Hash index
    
    // Catalog pages
    CATALOG_ROOT = 0x0100,  // Root catalog page
    CATALOG_HEAP = 0x0101,  // Catalog table data
    
    // Special pages
    TOAST = 0x0200,         // Oversized data
    FREE_SPACE = 0x0300,    // Free space map
};
```

### CatalogRootPage Layout

**Source:** `src/core/catalog_manager.cpp:4502`

The CatalogRootPage is the bootstrap page containing pointers to all catalog tables:

```cpp
struct CatalogRootPage {
    // Standard page header
    PageHeader header;           // 24 bytes
    
    // Catalog statistics
    uint32_t schema_count;       // Number of schemas
    uint32_t table_count;        // Number of tables
    uint32_t index_count;        // Number of indexes
    uint32_t sequence_count;     // Number of sequences
    
    // Core catalog table pages (40 entries = 160 bytes)
    uint32_t database_table_page;           // sb_database
    uint32_t schema_table_page;             // sb_schema
    uint32_t object_table_page;             // sb_object
    uint32_t object_name_table_page;        // sb_object_name
    uint32_t type_table_page;               // sb_types
    uint32_t type_modifiers_table_page;     // sb_type_modifiers
    uint32_t domains_table_page;            // sb_domains
    uint32_t tables_table_page;             // sb_tables
    uint32_t columns_table_page;            // sb_columns
    uint32_t indexes_table_page;            // sb_indexes
    uint32_t index_columns_table_page;      // sb_index_columns
    uint32_t constraints_table_page;        // sb_constraints
    uint32_t foreign_keys_table_page;       // sb_foreign_keys
    uint32_t sequences_table_page;          // sb_sequences
    uint32_t views_table_page;              // sb_views
    uint32_t triggers_table_page;           // sb_triggers
    uint32_t procedures_table_page;         // sb_procedures
    uint32_t functions_table_page;          // sb_functions
    uint32_t permissions_table_page;        // sb_permissions
    uint32_t users_table_page;              // sb_users
    uint32_t roles_table_page;              // sb_roles
    uint32_t groups_table_page;             // sb_groups
    uint32_t charsets_table_page;           // sb_charsets
    uint32_t collations_table_page;         // sb_collations
    uint32_t timezones_table_page;          // sb_timezones
    uint32_t statistics_table_page;         // sb_statistics
    uint32_t dependencies_table_page;       // sb_dependencies
    uint32_t comments_table_page;           // sb_comments
    uint32_t partitions_table_page;         // sb_partitions
    uint32_t tablespaces_table_page;        // sb_tablespaces
    
    // Extended catalog pages (100 entries = 400 bytes)
    uint32_t replication_channels_page;
    uint32_t replication_cursors_page;
    uint32_t replication_conflicts_page;
    uint32_t shard_policies_page;
    uint32_t shards_page;
    uint32_t shard_migrations_page;
    uint32_t jobs_page;
    uint32_t job_runs_page;
    uint32_t remote_connectors_page;
    uint32_t remote_metadata_page;
    // ... 90 additional reserved slots
    
    // Bootstrap metadata
    uint64_t bootstrap_timestamp;   // When database was created
    uint32_t bootstrap_version;     // Catalog format version
    uint32_t page_size;             // Database page size
    
    // Reserved for future expansion
    uint8_t reserved[2048];         // Padding to 4KB page
};

// Total size: 4096 bytes (one page)
```

**Page Field Summary:**

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 24 | header | Standard page header |
| 24 | 4 | schema_count | Schema count |
| 28 | 4 | table_count | Table count |
| 32 | 4 | index_count | Index count |
| 36 | 4 | sequence_count | Sequence count |
| 40 | 160 | core_pages[40] | Core catalog page pointers |
| 200 | 400 | extended_pages[100] | Extended catalog page pointers |
| 600 | 8 | bootstrap_timestamp | Creation time |
| 608 | 4 | bootstrap_version | Format version |
| 612 | 4 | page_size | Page size |
| 616 | 3480 | reserved | Padding |

### CatalogHeapPage Layout

**Source:** `include/scratchbird/core/catalog_manager.h:298`

Catalog tables use a specialized heap page format:

```cpp
struct CatalogHeapPage {
    // Standard page header
    PageHeader header;      // 24 bytes
    
    // Catalog-specific header fields
    uint32_t record_count;  // Number of records on this page
    uint32_t free_offset;   // Offset to free space
    uint32_t next_page;     // Next page in chain (0 = end)
    uint32_t reserved;      // Alignment padding
    
    // Variable-length records follow
    uint8_t data[];         // Record data
};
```

**Layout Diagram:**

```
┌─────────────────────────────────────────────────────────────┐
│                     CatalogHeapPage                          │
├─────────────────────────────────────────────────────────────┤
│ PageHeader (24 bytes)                                        │
│   - page_id: uint32                                          │
│   - page_type: PageType (CATALOG_HEAP)                       │
│   - checksum: uint32                                         │
│   - lsn: uint64                                              │
├─────────────────────────────────────────────────────────────┤
│ Catalog Header (16 bytes)                                    │
│   - record_count: uint32                                     │
│   - free_offset: uint32                                      │
│   - next_page: uint32                                        │
│   - reserved: uint32                                         │
├─────────────────────────────────────────────────────────────┤
│ Record Directory (variable)                                  │
│   - Array of (offset, length) pairs                          │
│   - Grows from top of page downward                          │
├─────────────────────────────────────────────────────────────┤
│ Free Space                                                   │
├─────────────────────────────────────────────────────────────┤
│ Record Data (variable)                                       │
│   - Records packed from bottom of page upward                │
│   - Variable length, 8-byte aligned                          │
└─────────────────────────────────────────────────────────────┘
```

### Record Format

Catalog records use a fixed-size header with variable-length trailing data:

```cpp
struct CatalogRecordHeader {
    ID record_id;           // UUID of this record
    uint32_t record_length; // Total length including this header
    uint16_t flags;         // Record flags
    uint16_t version;       // Record version for MGA
    uint64_t xmin;          // Creating transaction ID
    uint64_t xmax;          // Expiring transaction ID (0 = valid)
};
```

**Record Layout:**

```
┌──────────────────────────────────────────────────────────────┐
│                    Catalog Record                            │
├──────────────────────────────────────────────────────────────┤
│ CatalogRecordHeader (48 bytes)                               │
│   - record_id: ID (16 bytes)                                 │
│   - record_length: uint32                                    │
│   - flags: uint16                                            │
│   - version: uint16                                          │
│   - xmin: uint64                                             │
│   - xmax: uint64                                             │
├──────────────────────────────────────────────────────────────┤
│ Fixed Fields (table-specific)                                │
│   - Defined by record structure (e.g., TableRecord)          │
├──────────────────────────────────────────────────────────────┤
│ Variable Fields (optional)                                   │
│   - Large strings stored inline or TOAST reference           │
└──────────────────────────────────────────────────────────────┘
```

### Page Chaining

For catalog tables that exceed one page, records are chained:

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  CatalogPage │───▶│  CatalogPage │───▶│  CatalogPage │
│     #1       │    │     #2       │    │     #3       │
│  (Primary)   │    │  (Overflow)  │    │  (Overflow)  │
└──────────────┘    └──────────────┘    └──────────────┘
   next_page ───────▶ next_page ───────▶ next_page = 0
```

**Chaining Rules:**
1. New pages allocated when free space < 10% of page
2. Records never span pages (simplifies MGA)
3. TOAST used for large variable-length data
4. Page compaction triggered at 30% fragmentation

### Specific Catalog Table Page Layouts

#### sb_schema Page Layout

```cpp
struct SchemaRecord {
    // CatalogRecordHeader (48 bytes)
    
    // Fixed fields (608 bytes)
    ID schema_id;                   // 16 bytes
    ID parent_schema_id;            // 16 bytes
    char schema_name[512];          // 512 bytes
    ID owner_id;                    // 16 bytes
    ID default_tablespace_id;       // 16 bytes
    uint32_t permissions;           // 4 bytes
    ID default_charset_id;          // 16 bytes
    uint8_t name_is_delimited;      // 1 byte
    uint8_t reserved[7];            // 7 bytes
    uint32_t default_collation_id;  // 4 bytes
    ID acl_oid;                     // 16 bytes
    uint64_t created_time;          // 8 bytes
    uint64_t last_modified_time;    // 8 bytes
    uint32_t is_valid;              // 4 bytes
    uint32_t padding;               // 4 bytes
    
    // Total: 656 bytes per record
};
```

**Records per 4KB page:** ~6 records (with overhead)

#### sb_tables Page Layout

```cpp
struct TableRecord {
    // CatalogRecordHeader (48 bytes)
    
    // Fixed fields (variable, ~400 bytes)
    ID table_id;                    // 16 bytes
    ID schema_id;                   // 16 bytes
    char table_name[512];           // 512 bytes
    ID owner_id;                    // 16 bytes
    uint64_t root_gpid;             // 8 bytes
    uint32_t column_count;          // 4 bytes
    uint64_t row_count;             // 8 bytes
    uint8_t table_type;             // 1 byte
    uint8_t has_toast;              // 1 byte
    uint8_t rls_enabled;            // 1 byte
    uint8_t rls_forced;             // 1 byte
    uint8_t temp_metadata_scope;    // 1 byte
    uint8_t temp_data_scope;        // 1 byte
    uint8_t temp_on_commit;         // 1 byte
    uint8_t temp_flags;             // 1 byte
    uint8_t name_is_delimited;      // 1 byte
    ID tablespace_id;               // 16 bytes
    ID default_charset_id;          // 16 bytes
    uint32_t default_collation_id;  // 4 bytes
    ID storage_params_oid;          // 16 bytes
    ID creating_session_id;         // 16 bytes
    uint64_t creating_transaction_id; // 8 bytes
    ID temp_parent_table_id;        // 16 bytes
    ID temp_schema_id;              // 16 bytes
    uint64_t created_time;          // 8 bytes
    uint64_t last_modified_time;    // 8 bytes
    uint64_t policy_epoch;          // 8 bytes
    uint32_t is_valid;              // 4 bytes
    uint32_t padding;               // 4 bytes
    
    // Total: ~720 bytes per record
};
```

**Records per 4KB page:** ~5 records

#### sb_columns Page Layout

```cpp
struct ColumnRecord {
    // CatalogRecordHeader (48 bytes)
    
    // Fixed fields (~720 bytes)
    ID table_id;                    // 16 bytes
    ID column_id;                   // 16 bytes
    char column_name[512];          // 512 bytes
    uint16_t ordinal;               // 2 bytes
    uint16_t data_type;             // 2 bytes
    uint32_t type_precision;        // 4 bytes
    uint32_t type_scale;            // 4 bytes
    uint32_t max_length;            // 4 bytes
    ID domain_id;                   // 16 bytes
    uint8_t is_array;               // 1 byte
    uint32_t array_size;            // 4 bytes
    uint8_t nullable;               // 1 byte
    uint8_t has_default;            // 1 byte
    uint8_t is_primary_key;         // 1 byte
    uint8_t is_unique;              // 1 byte
    uint8_t is_foreign_key;         // 1 byte
    uint8_t is_generated;           // 1 byte
    uint8_t storage_type;           // 1 byte
    uint8_t with_timezone;          // 1 byte
    uint8_t name_is_delimited;      // 1 byte
    ID charset_id;                  // 16 bytes
    ID timezone_id;                 // 16 bytes
    uint32_t collation_id;          // 4 bytes
    char default_value[128];        // 128 bytes
    ID default_value_oid;           // 16 bytes
    ID check_expr_oid;              // 16 bytes
    uint64_t created_time;          // 8 bytes
    uint32_t is_valid;              // 4 bytes
    uint32_t padding;               // 4 bytes
    
    // Total: ~768 bytes per record
};
```

**Records per 4KB page:** ~5 records

### Page Allocation Strategy

```cpp
// Catalog page allocation rules
constexpr uint32_t CATALOG_PAGES_PER_EXTENT = 8;  // Allocate in groups
constexpr uint32_t CATALOG_PAGE_FILL_TARGET = 70; // 70% fill before extending

// Allocation algorithm
Status allocateCatalogPage(uint32_t& page_id_out) {
    1. Check current page for free space
    2. If insufficient space:
       a. Check if next page in chain exists
       b. If not, allocate new page
       c. Link to chain
    3. Return page ID
}
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|--------------|
| `PAGE_INV_001` | CatalogRootPage is always page 0 | Page 0 type check |
| `PAGE_INV_002` | All catalog pages have valid checksums | Header validation |
| `PAGE_INV_003` | Record offsets are 8-byte aligned | Offset validation |
| `PAGE_INV_004` | next_page chain never has cycles | Cycle detection |
| `PAGE_INV_005` | Record count matches directory entries | Consistency check |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `PAGE_CORRUPTED` | Checksum mismatch | Log error, attempt recovery |
| `PAGE_OVERFLOW` | Record too large for page | Use TOAST storage |
| `CHAIN_BROKEN` | Invalid next_page pointer | Repair chain |
| `RECORD_TOO_LARGE` | Record exceeds page size | Reject operation |

## Related Specifications

- [catalog_table_layouts.md](./catalog_table_layouts.md) - Catalog table schemas
- [bootstrap_sequence.md](./bootstrap_sequence.md) - Page initialization
- [metadata_caching.md](./metadata_caching.md) - Page caching

## Appendix

### Page Size Considerations

| Page Size | Records/Page (sb_tables) | Extent Size | Recommended Use |
|-----------|-------------------------|-------------|-----------------|
| 4KB | 5 | 32KB | Standard databases |
| 8KB | 10 | 64KB | Large catalogs |
| 16KB | 21 | 128KB | Enterprise deployments |
| 32KB | 42 | 256KB | Data warehouse |

### Glossary

| Term | Definition |
|------|------------|
| Page | Fixed-size disk block (4KB default) |
| Extent | Contiguous group of pages |
| GPID | Global Page ID (tablespace + page number) |
| LSN | Log Sequence Number for recovery |
| MGA | Multi-Generational Architecture |
| TOAST | The Oversized-Attribute Storage Technique |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
