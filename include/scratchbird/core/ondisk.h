#pragma once

#include <cstdint>
#include <cstddef>

namespace scratchbird::core
{

    // Page types per ON_DISK_FORMAT.md
    enum PageType : uint16_t
    {
        PAGE_TYPE_DATABASE_HEADER = 0,
        PAGE_TYPE_SYSTEM_CATALOG = 1,
        PAGE_TYPE_FREE_SPACE_MAP = 2,
        PAGE_TYPE_HEAP = 3,
        PAGE_TYPE_BTREE_META = 4,
        PAGE_TYPE_BTREE_INTERNAL = 5,
        PAGE_TYPE_BTREE_LEAF = 6,
        PAGE_TYPE_TRANSACTION_MAP = 7,
        PAGE_TYPE_CATALOG_ROOT = 8, // Root page for system catalog
        HASH_INDEX_META = 9,        // Hash index meta page
        HASH_INDEX_DIRECTORY = 10,  // Hash index directory page
        HASH_INDEX_BUCKET = 11,     // Hash index bucket page
        PAGE_TYPE_CLOG = 12,        // Commit log page (2-bit transaction status)
        GIN_INDEX_META = 13,        // GIN index meta page
        GIN_PENDING_LIST = 14,      // GIN pending list page
        GIN_POSTING_LIST = 15,      // GIN posting list page
        GIN_POSTING_TREE = 16,      // GIN posting tree page
        BITMAP_INDEX_META = 17,     // Bitmap index meta page
        BITMAP_INDEX_DICT = 18,     // Bitmap index dictionary page
        BITMAP_ROARING_ROOT = 19,   // Roaring bitmap root page
        BITMAP_CONTAINER = 20,      // Roaring bitmap container page
        PAGE_TYPE_RTREE_NODE = 21,  // R-tree node page (internal or leaf)
    };

    // Page flags (bitwise OR)
    constexpr uint32_t PAGE_FLAG_DIRTY = 0x0001;      // Page has uncommitted changes
    constexpr uint32_t PAGE_FLAG_PINNED = 0x0002;     // Page is pinned in buffer
    constexpr uint32_t PAGE_FLAG_COMPRESSED = 0x0004; // Page data is compressed
    constexpr uint32_t PAGE_FLAG_ENCRYPTED = 0x0008;  // Page data is encrypted

// Fixed 64-byte page header; little-endian integers assumed
#pragma pack(push, 1)
    struct PageHeader
    {
        uint32_t magic;     // 0x00 'SBRD'
        uint16_t version;   // 0x04 format version
        uint16_t page_type; // 0x06 PageType
        uint32_t page_size; // 0x08 8192|16384|32768|65536|131072
        uint32_t checksum;  // 0x0C CRC32C of [0x10..page_size)

        uint64_t lsn;     // 0x10
        uint32_t page_id; // 0x18
        uint32_t flags;   // 0x1C

        uint8_t database_uuid[16]; // 0x20 UUID v7 bytes

        uint64_t generation;   // 0x30
        uint16_t free_space;   // 0x38
        uint16_t item_count;   // 0x3A
        uint16_t free_offset;  // 0x3C
        uint16_t special_size; // 0x3E
    };
#pragma pack(pop)

    constexpr uint32_t K_MAGIC_SBRD = 0x53425244; // 'SBRD' little-endian

    // CRC32C API (implemented in core)
    auto crc32cCompute(const uint8_t *data, size_t length, uint32_t initial) -> uint32_t;

    inline auto calculatePageChecksum(const uint8_t *page, uint32_t page_size) -> uint32_t
    {
        // initial value 0xFFFFFFFF, process [0x00..0x0B] and [0x10..page_size)
        uint32_t crc = 0xFFFFFFFFU;
        crc = crc32cCompute(page, 12, crc);
        crc = crc32cCompute(page + 16, page_size - 16, crc);
        return crc ^ 0xFFFFFFFFU;
    }

    inline auto validatePageChecksum(const uint8_t *page, uint32_t page_size) -> bool
    {
        const auto *header = reinterpret_cast<const PageHeader *>(page);
        return header->checksum == calculatePageChecksum(page, page_size);
    }

    inline auto isValidAlphaPageSize(uint32_t page_size) -> bool
    {
        return page_size == 8192U || page_size == 16384U || page_size == 32768U ||
               page_size == 65536U || page_size == 131072U;
    }

} // namespace scratchbird::core
