/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{

    // Page types per PAGE_TYPES_AND_LAYOUTS.md (authoritative)
    enum PageType : uint16_t
    {
        PAGE_TYPE_DATABASE_HEADER = 0x0001,
        PAGE_TYPE_CATALOG_ROOT = 0x0002,
        PAGE_TYPE_FREE_SPACE_MAP = 0x0003,
        PAGE_TYPE_TRANSACTION_MAP = 0x0004,
        PAGE_TYPE_VISIBILITY_MAP = 0x0005,
        PAGE_TYPE_SEQUENCE = 0x0006,
        PAGE_TYPE_SLRU = 0x0007,
        PAGE_TYPE_FREE_LIST = 0x0008,

        PAGE_TYPE_HEAP = 0x0101,
        PAGE_TYPE_TOAST = 0x0102,

        PAGE_TYPE_INDEX_BTREE = 0x1001,
        PAGE_TYPE_INDEX_HASH = 0x1002,
        PAGE_TYPE_INDEX_GIN = 0x1003,
        PAGE_TYPE_INDEX_GIST = 0x1004,
        PAGE_TYPE_INDEX_SPGIST = 0x1005,
        PAGE_TYPE_INDEX_BRIN = 0x1006,
        PAGE_TYPE_INDEX_BITMAP = 0x1007,
        PAGE_TYPE_INDEX_RTREE = 0x1008,
        PAGE_TYPE_INDEX_HNSW = 0x1009,
        PAGE_TYPE_INDEX_LSM_MEMTABLE = 0x100A,
        PAGE_TYPE_INDEX_LSM_SSTABLE = 0x100B,
        PAGE_TYPE_INDEX_COLUMNSTORE = 0x100C,
        PAGE_TYPE_INDEX_FULLTEXT = 0x100D,
        PAGE_TYPE_INDEX_ZORDER = 0x100E,
        PAGE_TYPE_INDEX_GEOHASH_S2 = 0x100F,
        PAGE_TYPE_INDEX_QUADTREE = 0x1010,
        PAGE_TYPE_INDEX_OCTREE = 0x1011,
        PAGE_TYPE_INDEX_FST = 0x1012,
        PAGE_TYPE_INDEX_SUFFIX_ARRAY = 0x1013,
        PAGE_TYPE_INDEX_SUFFIX_TREE = 0x1014,
        PAGE_TYPE_INDEX_CMS = 0x1015,
        PAGE_TYPE_INDEX_HLL = 0x1016,
        PAGE_TYPE_INDEX_ART = 0x1017,
        PAGE_TYPE_INDEX_LEARNED = 0x1018,
        PAGE_TYPE_INDEX_JSON_PATH = 0x1019,
        PAGE_TYPE_INDEX_IVF = 0x101A,
        PAGE_TYPE_INDEX_ZONEMAP = 0x101B,

        PAGE_TYPE_INDEX_OVERFLOW = 0x1F01,
        PAGE_TYPE_SPECIAL = 0x1FFF,

        // Legacy aliases for compatibility during V3 alignment
        PAGE_TYPE_SYSTEM_CATALOG = PAGE_TYPE_CATALOG_ROOT,
        PAGE_TYPE_BTREE_META = PAGE_TYPE_INDEX_BTREE,
        PAGE_TYPE_BTREE_INTERNAL = PAGE_TYPE_INDEX_BTREE,
        PAGE_TYPE_BTREE_LEAF = PAGE_TYPE_INDEX_BTREE,
        HASH_INDEX_META = PAGE_TYPE_INDEX_HASH,
        HASH_INDEX_DIRECTORY = PAGE_TYPE_INDEX_HASH,
        HASH_INDEX_BUCKET = PAGE_TYPE_INDEX_HASH,
        PAGE_TYPE_CLOG = PAGE_TYPE_SLRU,
        GIN_INDEX_META = PAGE_TYPE_INDEX_GIN,
        GIN_PENDING_LIST = PAGE_TYPE_INDEX_GIN,
        GIN_POSTING_LIST = PAGE_TYPE_INDEX_GIN,
        GIN_POSTING_TREE = PAGE_TYPE_INDEX_GIN,
        BITMAP_INDEX_META = PAGE_TYPE_INDEX_BITMAP,
        BITMAP_INDEX_DICT = PAGE_TYPE_INDEX_BITMAP,
        BITMAP_ROARING_ROOT = PAGE_TYPE_INDEX_BITMAP,
        BITMAP_CONTAINER = PAGE_TYPE_INDEX_BITMAP,
        PAGE_TYPE_RTREE_NODE = PAGE_TYPE_INDEX_RTREE,
        PAGE_TYPE_GIST = PAGE_TYPE_INDEX_GIST,
        PAGE_TYPE_SPGIST = PAGE_TYPE_INDEX_SPGIST,
        PAGE_TYPE_BRIN = PAGE_TYPE_INDEX_BRIN,
        PAGE_TYPE_COLUMNSTORE = PAGE_TYPE_INDEX_COLUMNSTORE,
        PAGE_TYPE_BLOOM_FILTER_META = PAGE_TYPE_INDEX_FULLTEXT,
        PAGE_TYPE_BLOOM_FILTER_DATA = PAGE_TYPE_INDEX_FULLTEXT,
        PAGE_TYPE_INVERTED_META = PAGE_TYPE_INDEX_FULLTEXT,
        PAGE_TYPE_INVERTED_SEGMENT_META = PAGE_TYPE_INDEX_FULLTEXT,
        PAGE_TYPE_INVERTED_DICT = PAGE_TYPE_INDEX_FULLTEXT,
        PAGE_TYPE_INVERTED_POSTING = PAGE_TYPE_INDEX_FULLTEXT,
        PAGE_TYPE_INVERTED_DOCSTATS = PAGE_TYPE_INDEX_FULLTEXT
    };

    // Page flags (bitwise OR)
    constexpr uint32_t PAGE_FLAG_DIRTY = 0x0001;      // Page has uncommitted changes
    constexpr uint32_t PAGE_FLAG_PINNED = 0x0002;     // Page is pinned in buffer
    constexpr uint32_t PAGE_FLAG_COMPRESSED = 0x0004; // Page data is compressed
    constexpr uint32_t PAGE_FLAG_ENCRYPTED = 0x0008;  // Page data is encrypted

// Fixed 80-byte page header per ON_DISK_FORMAT.md; little-endian integers assumed
#pragma pack(push, 1)
    struct PageHeader
    {
        uint32_t magic;        // 0x00 'SBRD'
        uint16_t version;      // 0x04 format version
        uint16_t page_type;    // 0x06 PageType
        uint32_t page_size;    // 0x08 8192|16384|32768|65536|131072
        uint32_t checksum;     // 0x0C CRC32C of bytes [0x10..page_size)
        uint64_t lsn;          // 0x10 Log Sequence Number (0 if no WAL)
        uint32_t page_id;      // 0x18 page number in file (0-based)
        uint32_t flags;        // 0x1C page-specific flags
        uint8_t  database_uuid[16]; // 0x20 Database UUID (v7)
        uint8_t  table_id[16];      // 0x30 Table UUID (v7) (0 for non-heap pages)
        uint64_t generation;   // 0x40 page generation for MVCC
        uint16_t free_space;   // 0x48 bytes of free space
        uint16_t item_count;   // 0x4A number of items on page
        uint16_t free_offset;  // 0x4C offset to start of free space
        uint16_t special_size; // 0x4E size of special area at page end
    };
#pragma pack(pop)

static_assert(sizeof(PageHeader) == 80, "PageHeader must be exactly 80 bytes per ON_DISK_FORMAT.md");

    constexpr uint32_t K_MAGIC_SBRD = 0x53425244; // 'SBRD' little-endian

    // CRC32C API (implemented in core)
    auto crc32cCompute(const uint8_t *data, size_t length, uint32_t initial) -> uint32_t;

    inline auto pageLower(const PageHeader &header) -> uint32_t
    {
        const uint32_t unit = (header.page_size > 0xFFFFu) ? 2u : 1u;
        return static_cast<uint32_t>(header.free_offset) * unit;
    }

    inline auto pageUpper(const PageHeader &header) -> uint32_t
    {
        const uint32_t unit = (header.page_size > 0xFFFFu) ? 2u : 1u;
        return (static_cast<uint32_t>(header.free_offset) +
                static_cast<uint32_t>(header.free_space)) *
               unit;
    }

    inline auto pageSpecial(const PageHeader &header) -> uint32_t
    {
        const uint32_t unit = (header.page_size > 0xFFFFu) ? 2u : 1u;
        return header.page_size - (static_cast<uint32_t>(header.special_size) * unit);
    }

    inline void pageSetLower(PageHeader &header, uint32_t lower)
    {
        const uint32_t unit = (header.page_size > 0xFFFFu) ? 2u : 1u;
        header.free_offset = static_cast<uint16_t>(lower / unit);
    }

    inline void pageSetUpper(PageHeader &header, uint32_t upper)
    {
        const uint32_t unit = (header.page_size > 0xFFFFu) ? 2u : 1u;
        uint32_t lower = static_cast<uint32_t>(header.free_offset) * unit;
        if (upper < lower)
        {
            upper = lower;
        }
        header.free_space = static_cast<uint16_t>((upper - lower) / unit);
    }

    inline void pageSetSpecial(PageHeader &header, uint32_t special)
    {
        const uint32_t unit = (header.page_size > 0xFFFFu) ? 2u : 1u;
        if (special > header.page_size)
        {
            special = header.page_size;
        }
        header.special_size = static_cast<uint16_t>((header.page_size - special) / unit);
    }

    inline void setDatabaseUuid(PageHeader &header, const ID &uuid)
    {
        std::memcpy(header.database_uuid, uuid.bytes.data(), 16);
    }

    inline auto getDatabaseUuid(const PageHeader &header) -> ID
    {
        ID out{};
        std::memcpy(out.bytes.data(), header.database_uuid, 16);
        return out;
    }

    inline void setTableId(PageHeader &header, const ID &uuid)
    {
        std::memcpy(header.table_id, uuid.bytes.data(), 16);
    }

    inline auto getTableId(const PageHeader &header) -> ID
    {
        ID out{};
        std::memcpy(out.bytes.data(), header.table_id, 16);
        return out;
    }

    inline auto calculatePageChecksum(const uint8_t *page, uint32_t page_size) -> uint32_t
    {
        uint32_t crc = 0xFFFFFFFFU;
        // Per ON_DISK_FORMAT.md: exclude checksum field bytes 0x0C-0x0F
        if (page_size >= 0x10)
        {
            crc = crc32cCompute(page, 0x0C, crc);
            crc = crc32cCompute(page + 0x10, page_size - 0x10, crc);
        }
        else if (page_size > 0)
        {
            crc = crc32cCompute(page, page_size, crc);
        }
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
