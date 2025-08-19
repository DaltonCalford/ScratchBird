#ifndef SCRATCHBIRD_ENGINE_ODS_H
#define SCRATCHBIRD_ENGINE_ODS_H

#include <cstddef>
#include <cstdint>

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
    // Notes:
    // - prev/next are used for B-Tree sibling links
    // - scn acts as a per-page LSN for WAL/recovery
    struct PageHeader {
        std::uint32_t checksum{0};       // CRC32C of page (header checksum field zeroed)
        std::uint32_t page_no{0};        // Logical page number within page space
        std::uint16_t space_id{1};       // Page space identifier (1 = DB)
        std::uint16_t type{0};           // PageType as integer
        std::uint32_t flags{0};          // Reserved for future
        std::uint64_t scn{0};            // System change number / epoch (per-page LSN)
        std::uint32_t prev{0};           // Optional backward link
        std::uint32_t next{0};           // Optional forward link
        std::uint32_t page_size{0};      // Size in bytes
        std::uint32_t header_version{1}; // Header layout version
    };

    // Heap constants and helpers
    inline constexpr std::uint16_t HEAP_SLOT_SIZE_BYTES = 2; // one 2-byte offset per slot
    inline constexpr std::uint16_t HEAP_ALIGN_BYTES = 2;     // tuple payload alignment (Phase 1)
    inline constexpr std::uint16_t HEAP_ALIGN8_BYTES = 8;    // optional for 8-byte types (future)
    inline constexpr std::uint16_t HEAP_MAX_SLOTS_PER_PAGE =
        32767; // guard; actual limited by space
    inline constexpr std::uint32_t HEAP_VARLEN_LARGE_SENTINEL =
        0xFFFFu; // 65535 indicates large varlena header follows
    inline constexpr std::uint32_t HEAP_OVERFLOW_THRESHOLD_PCT =
        80; // off-page if tuple > 80% of page (default)

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

    inline std::uint64_t pack_rowid(const RowId& rid)
    {
        return (static_cast<std::uint64_t>(rid.space_id) << 48) |
               (static_cast<std::uint64_t>(rid.page_no) << 16) |
               (static_cast<std::uint64_t>(rid.slot_no));
    }

    inline RowId unpack_rowid(std::uint64_t packed)
    {
        RowId r{};
        r.space_id = static_cast<std::uint16_t>((packed >> 48) & 0xFFFFu);
        r.page_no = static_cast<std::uint32_t>((packed >> 16) & 0xFFFFFFFFu);
        r.slot_no = static_cast<std::uint16_t>(packed & 0xFFFFu);
        return r;
    }

    // Heap page header for slot-directory pages
    struct HeapPageHeader {
        std::uint16_t num_slots{0};  // number of slot entries (incl. dead slots)
        std::uint16_t free_start{0}; // first free byte after headers/tuples region
        std::uint16_t dir_start{0};  // first byte of slot directory from page end (grows backward)
        std::uint16_t flags{0};      // has_free_space, has_overflow, etc.
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

    // Space Catalog payload (Phase 2)
    struct SpaceCatalogPayload {
        std::uint16_t version{1};
        std::uint16_t space_id{1};
        std::uint32_t page_size{4096};
        std::uint32_t pip_root_page{1};
        std::uint32_t tip_root_page{0};
        std::uint32_t segments{1};
        std::uint32_t next_extent_id{1};
    };

    // Overflow reference for off-page varlena
    struct OverflowRef {
        std::uint16_t space_id{1};
        std::uint32_t page_no{0};
        std::uint16_t slot_or_off{
            0}; // slot or byte offset within overflow page (Phase 1: single-chunk; use 0)
        std::uint32_t length{0};
    };

    // Tuple on-disk header (Phase 1; MGA fields placeholders)
    struct TupleHeader {
        std::uint64_t created_xid{0}; // to be enforced in Phase 3
        std::uint64_t deleted_xid{0}; // 0 if visible/live
        std::uint64_t backptr_rid{0}; // packed RowId of prior version or 0
        std::uint16_t num_attrs{0};
        std::uint16_t nullmap_bytes{0};
        std::uint16_t varlena_bytes{0}; // optional aggregate varlena length (may be 0)
        std::uint16_t flags{0};         // has_overflow, etc.
        // followed by: [null-bitmap][attribute directory][attribute data]
    };

    // CRC32C checksum (Castagnoli)
    std::uint32_t crc32c(const void* data, std::size_t len, std::uint32_t seed = 0);

    // Capacity helpers for variable page sizes
    std::uint32_t pagesPerPIP(std::uint32_t page_size);
    std::uint32_t bytesBitPIP(std::uint32_t page_size);
    std::uint32_t transPerTIP(std::uint32_t page_size);
    std::uint32_t gensPerPage(std::uint32_t page_size);

} // namespace scratchbird::engine::ods

#endif // SCRATCHBIRD_ENGINE_ODS_H
