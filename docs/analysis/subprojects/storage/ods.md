### On-Disk Structures (ODS)

Core page types, headers, and helpers used across heap and space management.

## Implementation References
- Header: `ScratchBird/include/scratchbird/engine/ods.h`
- Source: `ScratchBird/src/engine/ods.cpp`

### Key structs and constants

#### Page types and header
```1:60:include/scratchbird/engine/ods.h
namespace scratchbird::engine::ods
{

    // Page types
    enum class PageType : std::uint16_t {
        Header = 1,
        Pip = 2,          // Pointer/Free page map
        Tip = 3,          // Transaction inventory page (seed in this phase)
        Data = 4,         // Reserved for data heap
        Blob = 5,         // Reserved for blob pages
        Generator = 6,    // Generator values page
        SpaceCatalog = 7, // Page space catalog/meta
        ScnMap = 8,       // Reserved for SCN mapping page
        // Heap relation pages
        HeapRoot = 9,      // Per-relation heap root metadata
        HeapData = 10,     // Heap tuple data page (slot-directory model)
        HeapOverflow = 11, // Heap overflow payload page for large varlen
        // Index page types (B-Tree and others)
        IndexRoot = 20,   // Per-index root metadata page
        IndexBranch = 21, // B-Tree branch page
        IndexLeaf = 22,   // B-Tree leaf page
        HashDir = 23,     // Hash directory page
        HashBucket = 24,  // Hash bucket page
        GinMeta = 25,     // GIN meta page
        GinPosting = 26,  // GIN posting/tree page
        RTreeNode = 27,   // R-Tree node page
        BitmapPage = 28   // Bitmap page
    };

    // On-disk page header (host byte order for now; future: endian tagging)
    struct PageHeader {
        std::uint32_t checksum{0};
        std::uint32_t page_no{0};
        std::uint16_t space_id{1};
        std::uint16_t type{0};
        std::uint32_t flags{0};
        std::uint64_t scn{0};
        std::uint32_t prev{0};
        std::uint32_t next{0};
        std::uint32_t page_size{0};
        std::uint32_t header_version{1};
    };
```

```24:27:src/engine/ods.cpp
    std::uint32_t crc32c(const void* data, std::size_t len, std::uint32_t seed)
    {
        return crc32c_update(seed, static_cast<const unsigned char*>(data), len);
    }
```

#### Heap constants and RowId packing
```53:92:include/scratchbird/engine/ods.h
    // Heap constants and helpers
    inline constexpr std::uint16_t HEAP_SLOT_SIZE_BYTES = 2;
    inline constexpr std::uint16_t HEAP_ALIGN_BYTES = 2;
    inline constexpr std::uint16_t HEAP_ALIGN8_BYTES = 8;
    inline constexpr std::uint16_t HEAP_MAX_SLOTS_PER_PAGE = 32767;
    inline constexpr std::uint32_t HEAP_VARLEN_LARGE_SENTINEL = 0xFFFFu;
    inline constexpr std::uint32_t HEAP_OVERFLOW_THRESHOLD_PCT = 80;

    inline constexpr std::uint32_t align_up(std::uint32_t value, std::uint32_t align)
    {
        return (value + align - 1) / align * align;
    }

    // RowID (RID) 64-bit packing: [space_id:16][page_no:32][slot_no:16]
    struct RowId {
        std::uint16_t space_id{1};
        std::uint32_t page_no{0};
        std::uint16_t slot_no{0};
    };

    inline std::uint64_t pack_rowid(const RowId& rid);
    inline RowId unpack_rowid(std::uint64_t packed);
```

```39:44:src/engine/ods.cpp
    std::uint32_t pagesPerPIP(std::uint32_t page_size)
    {
        const std::uint32_t bytes = bytesBitPIP(page_size);
        return bytes * 8u; // 1 bit per page
    }
```

#### Tuple and heap headers
```92:141:include/scratchbird/engine/ods.h
    // Heap page header for slot-directory pages
    struct HeapPageHeader {
        std::uint16_t num_slots{0};
        std::uint16_t free_start{0};
        std::uint16_t dir_start{0};
        std::uint16_t flags{0};
    };

    // Heap root payload persisted on a HeapRoot page
    struct HeapRootPayload {
        std::uint16_t version{1};
        std::uint16_t flags{0};
        std::uint32_t first_heap_page{0};
        std::uint32_t last_heap_page{0};
        std::uint32_t free_space_hint_page{0};
        std::uint32_t tuple_format_id{0};
    };

    // Tuple on-disk header (Phase 1; MGA fields placeholders)
    struct TupleHeader {
        std::uint64_t created_xid{0};
        std::uint64_t deleted_xid{0};
        std::uint64_t backptr_rid{0};
        std::uint16_t num_attrs{0};
        std::uint16_t nullmap_bytes{0};
        std::uint16_t varlena_bytes{0};
        std::uint16_t flags{0};
    };
```

```13:25:src/engine/alloc.cpp
    void Allocator::write_header()
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        ods::PageHeader* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        // ... existing code ...
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_->write_page(0, page.data());
    }
```

### Helper functions
```24:44:src/engine/ods.cpp
    std::uint32_t crc32c(const void* data, std::size_t len, std::uint32_t seed)
    {
        return crc32c_update(seed, static_cast<const unsigned char*>(data), len);
    }

    std::uint32_t pagesPerPIP(std::uint32_t page_size)
    {
        const std::uint32_t bytes = bytesBitPIP(page_size);
        return bytes * 8u; // 1 bit per page
    }
```

## Spec Trace
- [REQ-CORE-HEAP-ODS](../../traceability/spec/requirements.md#req-core-heap-ods)
- [REQ-CORE-SPACE-PIP](../../traceability/spec/requirements.md#req-core-space-pip)
- [REQ-CORE-SPACE-TIP-SEED](../../traceability/spec/requirements.md#req-core-space-tip-seed)
- [REQ-CORE-SPACE-CATALOG](../../traceability/spec/requirements.md#req-core-space-catalog)

