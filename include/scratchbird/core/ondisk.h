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
#include "scratchbird/core/status.h"

namespace scratchbird::core
{

    // Page types per PAGE_TYPE_ENUMS.md (authoritative)
    enum PageType : uint16_t
    {
        // Core and system
        PAGE_TYPE_DATABASE_HEADER = 0x0000,
        PAGE_TYPE_SYSTEM_STATE = 0x0001,
        PAGE_TYPE_CATALOG_ROOT = 0x0002,
        PAGE_TYPE_CATALOG_PAGE = 0x0003,
        PAGE_TYPE_FSM_ROOT = 0x0004,
        PAGE_TYPE_FSM_PAGE = 0x0005,
        PAGE_TYPE_TRANSACTION_MAP = 0x0006,
        PAGE_TYPE_HEAP = 0x0007,
        PAGE_TYPE_TOAST_META = 0x0008,
        PAGE_TYPE_TOAST_CHUNK = 0x0009,
        PAGE_TYPE_LOB_META = 0x000A,
        PAGE_TYPE_LOB_CHUNK = 0x000B,
        PAGE_TYPE_TEMP_HEAP = 0x000C,
        PAGE_TYPE_NAME_REGISTRY = 0x000D,
        PAGE_TYPE_BOOTSTRAP_RESERVED = 0x000E,
        PAGE_TYPE_FILESPACE_HEADER = 0x000F,

        // Core indexes
        PAGE_TYPE_BTREE_META = 0x0100,
        PAGE_TYPE_BTREE_INTERNAL = 0x0101,
        PAGE_TYPE_BTREE_LEAF = 0x0102,
        PAGE_TYPE_HASH_META = 0x0110,
        PAGE_TYPE_HASH_BUCKET = 0x0111,
        PAGE_TYPE_HASH_OVERFLOW = 0x0112,
        PAGE_TYPE_HASH_BITMAP = 0x0113,
        PAGE_TYPE_GIN_META = 0x0120,
        PAGE_TYPE_GIN_ENTRY = 0x0121,
        PAGE_TYPE_GIN_DATA = 0x0122,
        PAGE_TYPE_GIN_PENDING = 0x0123,
        PAGE_TYPE_GIST_INTERNAL = 0x0130,
        PAGE_TYPE_GIST_LEAF = 0x0131,
        PAGE_TYPE_SPGIST_META = 0x0140,
        PAGE_TYPE_SPGIST_INNER = 0x0141,
        PAGE_TYPE_SPGIST_LEAF = 0x0142,
        PAGE_TYPE_BRIN_META = 0x0150,
        PAGE_TYPE_BRIN_REVMAP = 0x0151,
        PAGE_TYPE_BRIN_DATA = 0x0152,
        PAGE_TYPE_BITMAP_META = 0x0160,
        PAGE_TYPE_BITMAP_DICT = 0x0161,
        PAGE_TYPE_BITMAP_CONTAINER = 0x0162,
        PAGE_TYPE_INVERTED_META = 0x0170,
        PAGE_TYPE_INVERTED_DICT = 0x0171,
        PAGE_TYPE_INVERTED_POSTINGS = 0x0172,
        PAGE_TYPE_SPARSE_META = 0x0180,
        PAGE_TYPE_SPARSE_DICT = 0x0181,
        PAGE_TYPE_SPARSE_POSTINGS = 0x0182,
        PAGE_TYPE_FTS_META = 0x0190,
        PAGE_TYPE_FTS_DICT = 0x0191,
        PAGE_TYPE_FTS_POSTINGS = 0x0192,
        PAGE_TYPE_TRIE_META = 0x01A0,
        PAGE_TYPE_TRIE_NODE = 0x01A1,
        PAGE_TYPE_ART_NODE = 0x01A8,
        PAGE_TYPE_SPATIAL_META = 0x01B0,
        PAGE_TYPE_SPATIAL_NODE = 0x01B1,
        PAGE_TYPE_MINHASH_META = 0x01C0,
        PAGE_TYPE_MINHASH_BUCKET = 0x01C1,
        PAGE_TYPE_BLOOM_META = 0x01D0,
        PAGE_TYPE_BLOOM_RANGE = 0x01D1,
        PAGE_TYPE_SAI_META = 0x01E0,
        PAGE_TYPE_SAI_TERM_DICT = 0x01E1,
        PAGE_TYPE_SAI_POSTINGS = 0x01E2,
        PAGE_TYPE_SAI_RANGE = 0x01E3,
        PAGE_TYPE_SAI_VECTOR = 0x01E4,
        PAGE_TYPE_SASI_META = 0x01F0,
        PAGE_TYPE_SASI_TERM_DICT = 0x01F1,
        PAGE_TYPE_SASI_POSTINGS = 0x01F2,
        PAGE_TYPE_SASI_RANGE = 0x01F3,

        // Columnstore, LSM, sort
        PAGE_TYPE_COLUMNSTORE_META = 0x0200,
        PAGE_TYPE_COLUMNSTORE_SEGMENT = 0x0201,
        PAGE_TYPE_COLUMNSTORE_DICT = 0x0202,
        PAGE_TYPE_COLUMNSTORE_RLE = 0x0203,
        PAGE_TYPE_COLUMNSTORE_BITPACK = 0x0204,
        PAGE_TYPE_LSM_META = 0x0210,
        PAGE_TYPE_LSM_INDEX = 0x0211,
        PAGE_TYPE_LSM_SSTABLE = 0x0212,
        PAGE_TYPE_LSM_FILTER = 0x0213,
        PAGE_TYPE_SORT_META = 0x0220,
        PAGE_TYPE_SORT_RUN = 0x0221,

        // Vector and ANN
        PAGE_TYPE_HNSW_META = 0x0300,
        PAGE_TYPE_HNSW_NODE = 0x0301,
        PAGE_TYPE_IVF_META = 0x0310,
        PAGE_TYPE_IVF_CENTROID = 0x0311,
        PAGE_TYPE_IVF_LIST = 0x0312,
        PAGE_TYPE_DISKANN_META = 0x0320,
        PAGE_TYPE_DISKANN_GRAPH = 0x0321,
        PAGE_TYPE_DISKANN_VECTOR_BLOCK = 0x0322,
        PAGE_TYPE_SCANN_META = 0x0330,
        PAGE_TYPE_SCANN_CENTROID = 0x0331,
        PAGE_TYPE_SCANN_PARTITION = 0x0332,
        PAGE_TYPE_CAGRA_META = 0x0340,
        PAGE_TYPE_CAGRA_NODE = 0x0341,
        PAGE_TYPE_ANNOY_META = 0x0350,
        PAGE_TYPE_ANNOY_NODE = 0x0351,
        PAGE_TYPE_NSG_META = 0x0360,
        PAGE_TYPE_NSG_NODE = 0x0361,
        PAGE_TYPE_VECTOR_FLAT_META = 0x0370,
        PAGE_TYPE_VECTOR_FLAT_SEGMENT = 0x0371,

        // Emulation and Redis
        PAGE_TYPE_DOC_META = 0x0400,
        PAGE_TYPE_DOC_DATA = 0x0401,
        PAGE_TYPE_KV_META = 0x0410,
        PAGE_TYPE_KV_DATA = 0x0411,
        PAGE_TYPE_WIDE_META = 0x0420,
        PAGE_TYPE_WIDE_ROW = 0x0421,
        PAGE_TYPE_GRAPH_META = 0x0430,
        PAGE_TYPE_GRAPH_NODE = 0x0431,
        PAGE_TYPE_GRAPH_EDGE = 0x0432,
        PAGE_TYPE_VECTOR_META = 0x0440,
        PAGE_TYPE_VECTOR_DATA = 0x0441,
        PAGE_TYPE_REDIS_META = 0x0450,
        PAGE_TYPE_REDIS_HASH = 0x0451,
        PAGE_TYPE_REDIS_LIST = 0x0452,
        PAGE_TYPE_REDIS_SET = 0x0453,
        PAGE_TYPE_REDIS_ZSET = 0x0454,
        PAGE_TYPE_REDIS_STREAM = 0x0455,
        PAGE_TYPE_REDIS_BITMAP = 0x0456,
        PAGE_TYPE_REDIS_HLL = 0x0457,
        PAGE_TYPE_REDIS_GEO = 0x0458,

        // vNext multi-model reserved range (0x2000..0x20FF)
        PAGE_TYPE_DOC_COLLECTION_ROOT = 0x2000,
        PAGE_TYPE_DOC_HEAP = 0x2001,
        PAGE_TYPE_DOC_PATH_DICTIONARY = 0x2002,
        PAGE_TYPE_DOC_PATH_POSTINGS = 0x2003,
        PAGE_TYPE_TS_MEASUREMENT_ROOT = 0x2004,
        PAGE_TYPE_TS_SERIES_INDEX = 0x2005,
        PAGE_TYPE_TS_CHUNK = 0x2006,
        PAGE_TYPE_TS_AGG_CACHE = 0x2007,
        PAGE_TYPE_COL_SEGMENT_HEADER = 0x2008,
        PAGE_TYPE_COL_SEGMENT_DATA = 0x2009,
        PAGE_TYPE_COL_ZONE_MAP = 0x200A,
        PAGE_TYPE_COL_DICTIONARY = 0x200B,
        PAGE_TYPE_SEARCH_TERM_DICT = 0x200C,
        PAGE_TYPE_SEARCH_POSTINGS = 0x200D,
        PAGE_TYPE_SEARCH_DOCVALUES = 0x200E,
        PAGE_TYPE_VECTOR_GRAPH = 0x200F,
        PAGE_TYPE_VECTOR_QUANTIZER = 0x2010,
        PAGE_TYPE_VECTOR_POSTING = 0x2011,
        PAGE_TYPE_LSM_RUN_MANIFEST = 0x2012,
        PAGE_TYPE_LSM_RUN_DATA = 0x2013,
        PAGE_TYPE_LSM_BLOOM = 0x2014,
        PAGE_TYPE_RETENTION_MANIFEST = 0x2015
    };

    constexpr uint16_t PAGE_TYPE_VNEXT_RANGE_START = 0x2000;
    constexpr uint16_t PAGE_TYPE_VNEXT_RANGE_END = 0x20FF;

    // vNext layout constants (section 01 vNext physical page contracts).
    constexpr uint32_t VNEXT_PAGE_SIZE_BYTES = 16384u;
    constexpr uint16_t VNEXT_PAGE_ALIGNMENT_BYTES = 8u;
    constexpr uint16_t VNEXT_BASE_HEADER_BYTES = 64u;
    constexpr uint16_t VNEXT_EXT_HEADER_BYTES = 32u;
    constexpr uint16_t VNEXT_PAYLOAD_REGION_START = 96u; // 0x60
    constexpr uint32_t K_MAGIC_SBPG = 0x53425047; // 'SBPG' little-endian

    // CRC32C API (implemented in core).
    auto crc32cCompute(const uint8_t *data, size_t length, uint32_t initial) -> uint32_t;

    inline auto isVNextPageTypeRange(uint16_t page_type) -> bool
    {
        return page_type >= PAGE_TYPE_VNEXT_RANGE_START &&
               page_type <= PAGE_TYPE_VNEXT_RANGE_END;
    }

    inline auto isKnownVNextPageType(uint16_t page_type) -> bool
    {
        switch (page_type)
        {
            case PAGE_TYPE_DOC_COLLECTION_ROOT:
            case PAGE_TYPE_DOC_HEAP:
            case PAGE_TYPE_DOC_PATH_DICTIONARY:
            case PAGE_TYPE_DOC_PATH_POSTINGS:
            case PAGE_TYPE_TS_MEASUREMENT_ROOT:
            case PAGE_TYPE_TS_SERIES_INDEX:
            case PAGE_TYPE_TS_CHUNK:
            case PAGE_TYPE_TS_AGG_CACHE:
            case PAGE_TYPE_COL_SEGMENT_HEADER:
            case PAGE_TYPE_COL_SEGMENT_DATA:
            case PAGE_TYPE_COL_ZONE_MAP:
            case PAGE_TYPE_COL_DICTIONARY:
            case PAGE_TYPE_SEARCH_TERM_DICT:
            case PAGE_TYPE_SEARCH_POSTINGS:
            case PAGE_TYPE_SEARCH_DOCVALUES:
            case PAGE_TYPE_VECTOR_GRAPH:
            case PAGE_TYPE_VECTOR_QUANTIZER:
            case PAGE_TYPE_VECTOR_POSTING:
            case PAGE_TYPE_LSM_RUN_MANIFEST:
            case PAGE_TYPE_LSM_RUN_DATA:
            case PAGE_TYPE_LSM_BLOOM:
            case PAGE_TYPE_RETENTION_MANIFEST:
                return true;
            default:
                return false;
        }
    }

    // Unknown page type in reserved vNext range must fail page read validation.
    inline auto validateVNextPageTypeKnown(uint16_t page_type) -> Status
    {
        if (!isVNextPageTypeRange(page_type))
        {
            return Status::OK;
        }
        return isKnownVNextPageType(page_type) ? Status::OK : Status::PAGE_CORRUPT;
    }

    // Page flags (bitwise OR)
    constexpr uint32_t PAGE_FLAG_DIRTY = 0x0001;          // Page has uncommitted changes
    constexpr uint32_t PAGE_FLAG_PINNED = 0x0002;         // Page is pinned in buffer
    constexpr uint32_t PAGE_FLAG_COMPRESSED = 0x0004;     // Page data is compressed
    constexpr uint32_t PAGE_FLAG_ENCRYPTED = 0x0008;      // Page data is encrypted
    constexpr uint32_t PAGE_FLAG_SPECIAL = 0x0010;        // Page has populated special area
    constexpr uint32_t PAGE_FLAG_CHECKSUM_VALID = 0x0020; // Checksum must validate

// Fixed 80-byte page header per ON_DISK_FORMAT.md; little-endian integers assumed
#pragma pack(push, 1)
    // vNext base header (64 bytes) and extension header (32 bytes).
    struct VNextBasePageHeader
    {
        uint32_t magic;         // 0x00 'SBPG'
        uint16_t page_type;     // 0x04 PageType
        uint16_t layout_version;// 0x06
        uint32_t page_id;       // 0x08
        uint32_t object_id;     // 0x0C
        uint64_t logical_epoch; // 0x10
        uint64_t writer_txid;   // 0x18
        uint16_t slot_count;    // 0x20
        uint16_t payload_start; // 0x22 expected 0x60
        uint16_t free_start;    // 0x24
        uint16_t free_end;      // 0x26
        uint16_t page_flags;    // 0x28
        uint16_t reserved0;     // 0x2A must be zero
        uint32_t header_crc32c; // 0x2C CRC32C over [0x00..0x2B]
        uint32_t page_crc32c;   // 0x30 CRC32C over page with this field zeroed
        uint32_t reserved1;     // 0x34 must be zero
        uint64_t reserved2;     // 0x38 must be zero
    };

    struct VNextExtensionHeader
    {
        uint16_t ext_version;      // 0x40
        uint16_t ext_flags;        // 0x42
        uint64_t min_visible_txid; // 0x44
        uint64_t max_visible_txid; // 0x4C
        uint64_t owner_txid;       // 0x54
        uint32_t payload_crc32c;   // 0x5C
    };

    struct VNextSlotDirectoryEntry
    {
        uint16_t slot_offset;
        uint16_t slot_len;
        uint16_t slot_flags;
        uint16_t slot_reserved;
    };

    enum class VNextPointerSwapState : uint8_t
    {
        PREPARE = 0,
        COMMITTED = 1
    };

    // Root/manifest slot record for atomic publish in vNext no-WAL flow.
    struct VNextPointerSwapRecord
    {
        uint64_t swap_epoch;    // +0x00
        uint32_t old_page_id;   // +0x08
        uint32_t new_page_id;   // +0x0C
        uint64_t owner_txid;    // +0x10
        uint32_t record_crc32c; // +0x18
        uint8_t state;          // +0x1C
        uint8_t reserved[3];    // +0x1D
    };

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
        uint8_t  object_uuid[16];   // 0x30 Owning object UUID (table/index/catalog object)
        uint64_t generation;   // 0x40 page generation for MVCC
        uint16_t free_space;   // 0x48 bytes of free space
        uint16_t item_count;   // 0x4A number of items on page
        uint16_t free_offset;  // 0x4C offset to start of free space
        uint16_t special_size; // 0x4E size of special area at page end
    };
#pragma pack(pop)

static_assert(sizeof(VNextBasePageHeader) == 64,
              "VNextBasePageHeader must be exactly 64 bytes");
static_assert(sizeof(VNextExtensionHeader) == 32,
              "VNextExtensionHeader must be exactly 32 bytes");
static_assert(sizeof(VNextSlotDirectoryEntry) == 8,
              "VNextSlotDirectoryEntry must be exactly 8 bytes");
static_assert(sizeof(VNextPointerSwapRecord) == 32,
              "VNextPointerSwapRecord must be exactly 32 bytes");
static_assert(sizeof(PageHeader) == 80, "PageHeader must be exactly 80 bytes per ON_DISK_FORMAT.md");

    inline auto isValidVNextPageHeaderBounds(const VNextBasePageHeader &header,
                                             uint32_t page_size = VNEXT_PAGE_SIZE_BYTES) -> bool
    {
        if (header.magic != K_MAGIC_SBPG)
        {
            return false;
        }
        if (header.payload_start != VNEXT_PAYLOAD_REGION_START)
        {
            return false;
        }
        if (header.free_start < header.payload_start)
        {
            return false;
        }
        if (header.free_start > header.free_end)
        {
            return false;
        }
        if (header.free_end > page_size)
        {
            return false;
        }
        if (header.reserved0 != 0u || header.reserved1 != 0u || header.reserved2 != 0u)
        {
            return false;
        }
        return true;
    }

    inline auto isValidVNextVisibilityRange(const VNextExtensionHeader &ext) -> bool
    {
        if (ext.min_visible_txid == 0u && ext.max_visible_txid == 0u)
        {
            return true;
        }
        return ext.min_visible_txid <= ext.max_visible_txid;
    }

    inline auto isValidVNextSlotDirectoryEntry(const VNextSlotDirectoryEntry &slot,
                                               uint32_t page_size = VNEXT_PAGE_SIZE_BYTES) -> bool
    {
        if (slot.slot_reserved != 0u)
        {
            return false;
        }
        if ((slot.slot_offset % VNEXT_PAGE_ALIGNMENT_BYTES) != 0u)
        {
            return false;
        }
        if (slot.slot_offset < VNEXT_PAYLOAD_REGION_START)
        {
            return false;
        }
        const uint32_t end = static_cast<uint32_t>(slot.slot_offset) +
                             static_cast<uint32_t>(slot.slot_len);
        return end <= page_size;
    }

    inline auto computeVNextPointerSwapRecordCrc32c(const VNextPointerSwapRecord &record) -> uint32_t
    {
        VNextPointerSwapRecord copy = record;
        copy.record_crc32c = 0u;
        return crc32cCompute(reinterpret_cast<const uint8_t *>(&copy), sizeof(copy), 0u);
    }

    inline auto isValidVNextPointerSwapRecord(const VNextPointerSwapRecord &record) -> bool
    {
        if (record.state > static_cast<uint8_t>(VNextPointerSwapState::COMMITTED))
        {
            return false;
        }
        if (record.reserved[0] != 0u || record.reserved[1] != 0u || record.reserved[2] != 0u)
        {
            return false;
        }
        return record.record_crc32c == computeVNextPointerSwapRecordCrc32c(record);
    }

    inline auto selectVNextAuthoritativeSwapSlot(const VNextPointerSwapRecord &slot0,
                                                 const VNextPointerSwapRecord &slot1,
                                                 uint8_t &selected_slot) -> Status
    {
        const bool slot0_valid = isValidVNextPointerSwapRecord(slot0) &&
                                 slot0.state == static_cast<uint8_t>(VNextPointerSwapState::COMMITTED);
        const bool slot1_valid = isValidVNextPointerSwapRecord(slot1) &&
                                 slot1.state == static_cast<uint8_t>(VNextPointerSwapState::COMMITTED);

        if (slot0_valid && !slot1_valid)
        {
            selected_slot = 0u;
            return Status::OK;
        }
        if (!slot0_valid && slot1_valid)
        {
            selected_slot = 1u;
            return Status::OK;
        }
        if (!slot0_valid && !slot1_valid)
        {
            return Status::PAGE_CORRUPT;
        }

        if (slot0.swap_epoch > slot1.swap_epoch)
        {
            selected_slot = 0u;
            return Status::OK;
        }
        if (slot1.swap_epoch > slot0.swap_epoch)
        {
            selected_slot = 1u;
            return Status::OK;
        }

        return Status::PAGE_CORRUPT;
    }

    // Canonical fixed bootstrap page map (section 06).
    constexpr uint32_t BOOTSTRAP_PAGE_DATABASE_HEADER = 0;
    constexpr uint32_t BOOTSTRAP_PAGE_SYSTEM_STATE = 1;
    constexpr uint32_t BOOTSTRAP_PAGE_CATALOG_ROOT = 2;
    constexpr uint32_t BOOTSTRAP_PAGE_FSM_ROOT = 3;
    constexpr uint32_t BOOTSTRAP_PAGE_TX_MAP_ROOT = 4;
    constexpr uint32_t BOOTSTRAP_PAGE_RESERVED = 5;
    constexpr uint32_t BOOTSTRAP_FIXED_PAGE_COUNT = 6;

#pragma pack(push, 1)
    struct BootstrapSystemStatePage
    {
        PageHeader page_header;
        uint8_t clean_shutdown;
        uint8_t engine_mode;
        uint8_t cluster_state;
        uint8_t reserved0;
        uint64_t last_checkpoint_txid;
        uint64_t last_checkpoint_time;
        uint64_t startup_counter;
        uint64_t restart_generation;
        uint64_t last_clean_shutdown_generation;
        uint64_t config_flags;
        uint64_t reserved[16];
    };

    constexpr uint64_t SYSTEM_STATE_STARTUP_RECON_VERSION = 2;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_VERSION_SLOT = 0;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_OUTCOME_SLOT = 1;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_STATUS_SLOT = 2;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_TIP_ABORTED_SLOT = 3;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_TIP_PREPARED_SLOT = 4;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_STALE_PREPARED_SLOT = 5;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_CLOG_SYNC_SLOT = 6;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_RELINKABLE_SLOT = 7;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_BLOCKED_SLOT = 8;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_FLAGS_SLOT = 9;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_QUARANTINABLE_SLOT = 10;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_UNRECOVERABLE_SLOT = 11;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_CLASS_SLOT = 12;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_ACTION_SLOT = 13;
    constexpr size_t SYSTEM_STATE_STARTUP_RECON_REPAIR_PLAN_SLOT = 14;

    constexpr uint64_t SYSTEM_STATE_STARTUP_RECON_FLAG_CLEAN_MARKER = 1ULL << 0;
    constexpr uint64_t SYSTEM_STATE_STARTUP_RECON_FLAG_STARTUP_REPAIR = 1ULL << 1;
    constexpr uint64_t SYSTEM_STATE_STARTUP_RECON_FLAG_PAGE_SCAN_FINDINGS = 1ULL << 2;
    constexpr uint64_t SYSTEM_STATE_STARTUP_RECON_FLAG_CORRUPT_PAGES = 1ULL << 3;
    constexpr uint64_t SYSTEM_STATE_STARTUP_RECON_FLAG_QUARANTINE_ACTIVE = 1ULL << 4;

    struct BootstrapCatalogRootHeader
    {
        uint16_t catalog_version;
        uint16_t entry_count;
        uint32_t reserved;
    };

    struct BootstrapCatalogRootEntry
    {
        uint8_t object_uuid[16];
        uint32_t root_page_id;
        uint32_t root_index_page_id;
    };

    struct BootstrapFsmRootPage
    {
        PageHeader page_header;
        uint32_t total_pages;
        uint32_t free_pages;
        uint32_t next_fsm_page;
        uint8_t bitmap[];
    };

    struct BootstrapTxMapRootPage
    {
        PageHeader page_header;
        uint16_t tip_version;
        uint16_t reserved0;
        uint32_t first_tip_page_id;
        uint32_t tip_page_count;
        uint64_t next_txid;
        uint64_t oldest_active_txid;
        uint64_t latest_completed_txid;
        uint64_t reserved[8];
    };
#pragma pack(pop)

// TOAST/LOB canonical page payload contracts (section 11).
#pragma pack(push, 1)
    struct ToastMetaPageLayout
    {
        uint8_t table_uuid[16];
        uint32_t toast_threshold;
        uint32_t chunk_size;
        uint32_t reserved[8];
    };

    struct ToastChunkRecordHeader
    {
        uint8_t lob_uuid[16];
        uint32_t chunk_index;
        uint32_t payload_len;
    };

    struct LobMetaRecordLayout
    {
        uint8_t lob_uuid[16];
        uint8_t owner_object_uuid[16];
        uint64_t total_len;
        uint32_t chunk_size;
        uint64_t created_txid;
        uint64_t deleted_txid;
    };

    struct LobChunkRecordHeader
    {
        uint8_t lob_uuid[16];
        uint32_t chunk_index;
        uint32_t payload_len;
    };
#pragma pack(pop)

// Canonical index special-area header contract (section 05).
#pragma pack(push, 1)
    struct IndexPageHeader
    {
        uint8_t index_uuid[16];
        uint16_t page_level;
        uint16_t flags;
        uint32_t right_sibling;
        uint32_t left_sibling;
        uint16_t opaque_len;
        uint16_t reserved;
    };
#pragma pack(pop)

    static_assert(sizeof(IndexPageHeader) == 32,
                  "IndexPageHeader must be exactly 32 bytes");

    constexpr uint16_t INDEX_PAGE_FLAG_ROOT = 0x0001;
    constexpr uint16_t INDEX_PAGE_FLAG_RIGHTMOST = 0x0002;
    constexpr uint16_t INDEX_PAGE_FLAG_LEFTMOST = 0x0004;
    constexpr uint16_t INDEX_PAGE_FLAG_DELETED = 0x0008;
    constexpr uint16_t INDEX_PAGE_FLAG_RESERVED_MASK = 0xFFF0;

    inline auto isValidIndexPageFlags(uint16_t flags) -> bool
    {
        return (flags & INDEX_PAGE_FLAG_RESERVED_MASK) == 0u;
    }

    inline auto isValidIndexPageHeaderBasic(const IndexPageHeader &header,
                                            uint16_t expected_opaque_len) -> bool
    {
        if (!isValidIndexPageFlags(header.flags))
        {
            return false;
        }
        if (header.reserved != 0u)
        {
            return false;
        }
        return header.opaque_len == expected_opaque_len;
    }

    inline void setIndexPageHeaderUuid(IndexPageHeader &header, const ID &uuid)
    {
        std::memcpy(header.index_uuid, uuid.bytes.data(), 16);
    }

    inline auto getIndexPageHeaderUuid(const IndexPageHeader &header) -> ID
    {
        ID out{};
        std::memcpy(out.bytes.data(), header.index_uuid, 16);
        return out;
    }

    inline auto isCanonicalIndexPageType(uint16_t page_type) -> bool
    {
        switch (page_type)
        {
            case PAGE_TYPE_BTREE_META:
            case PAGE_TYPE_BTREE_INTERNAL:
            case PAGE_TYPE_BTREE_LEAF:
            case PAGE_TYPE_HASH_META:
            case PAGE_TYPE_HASH_BUCKET:
            case PAGE_TYPE_HASH_OVERFLOW:
            case PAGE_TYPE_HASH_BITMAP:
            case PAGE_TYPE_GIN_META:
            case PAGE_TYPE_GIN_ENTRY:
            case PAGE_TYPE_GIN_DATA:
            case PAGE_TYPE_GIN_PENDING:
            case PAGE_TYPE_GIST_INTERNAL:
            case PAGE_TYPE_GIST_LEAF:
            case PAGE_TYPE_SPGIST_META:
            case PAGE_TYPE_SPGIST_INNER:
            case PAGE_TYPE_SPGIST_LEAF:
            case PAGE_TYPE_BRIN_META:
            case PAGE_TYPE_BRIN_REVMAP:
            case PAGE_TYPE_BRIN_DATA:
            case PAGE_TYPE_BITMAP_META:
            case PAGE_TYPE_BITMAP_DICT:
            case PAGE_TYPE_BITMAP_CONTAINER:
            case PAGE_TYPE_INVERTED_META:
            case PAGE_TYPE_INVERTED_DICT:
            case PAGE_TYPE_INVERTED_POSTINGS:
            case PAGE_TYPE_SPARSE_META:
            case PAGE_TYPE_SPARSE_DICT:
            case PAGE_TYPE_SPARSE_POSTINGS:
            case PAGE_TYPE_FTS_META:
            case PAGE_TYPE_FTS_DICT:
            case PAGE_TYPE_FTS_POSTINGS:
            case PAGE_TYPE_TRIE_META:
            case PAGE_TYPE_TRIE_NODE:
            case PAGE_TYPE_ART_NODE:
            case PAGE_TYPE_SPATIAL_META:
            case PAGE_TYPE_SPATIAL_NODE:
            case PAGE_TYPE_MINHASH_META:
            case PAGE_TYPE_MINHASH_BUCKET:
            case PAGE_TYPE_BLOOM_META:
            case PAGE_TYPE_BLOOM_RANGE:
            case PAGE_TYPE_SAI_META:
            case PAGE_TYPE_SAI_TERM_DICT:
            case PAGE_TYPE_SAI_POSTINGS:
            case PAGE_TYPE_SAI_RANGE:
            case PAGE_TYPE_SAI_VECTOR:
            case PAGE_TYPE_SASI_META:
            case PAGE_TYPE_SASI_TERM_DICT:
            case PAGE_TYPE_SASI_POSTINGS:
            case PAGE_TYPE_SASI_RANGE:
            case PAGE_TYPE_COLUMNSTORE_META:
            case PAGE_TYPE_COLUMNSTORE_SEGMENT:
            case PAGE_TYPE_COLUMNSTORE_DICT:
            case PAGE_TYPE_COLUMNSTORE_RLE:
            case PAGE_TYPE_COLUMNSTORE_BITPACK:
            case PAGE_TYPE_LSM_META:
            case PAGE_TYPE_LSM_INDEX:
            case PAGE_TYPE_LSM_SSTABLE:
            case PAGE_TYPE_LSM_FILTER:
            case PAGE_TYPE_SORT_META:
            case PAGE_TYPE_SORT_RUN:
            case PAGE_TYPE_HNSW_META:
            case PAGE_TYPE_HNSW_NODE:
            case PAGE_TYPE_IVF_META:
            case PAGE_TYPE_IVF_CENTROID:
            case PAGE_TYPE_IVF_LIST:
            case PAGE_TYPE_DISKANN_META:
            case PAGE_TYPE_DISKANN_GRAPH:
            case PAGE_TYPE_DISKANN_VECTOR_BLOCK:
            case PAGE_TYPE_SCANN_META:
            case PAGE_TYPE_SCANN_CENTROID:
            case PAGE_TYPE_SCANN_PARTITION:
            case PAGE_TYPE_CAGRA_META:
            case PAGE_TYPE_CAGRA_NODE:
            case PAGE_TYPE_ANNOY_META:
            case PAGE_TYPE_ANNOY_NODE:
            case PAGE_TYPE_NSG_META:
            case PAGE_TYPE_NSG_NODE:
            case PAGE_TYPE_VECTOR_FLAT_META:
            case PAGE_TYPE_VECTOR_FLAT_SEGMENT:
                return true;
            default:
                return false;
        }
    }

    inline auto isValidIndexSiblingContract(const IndexPageHeader &header) -> bool
    {
        const bool is_root = (header.flags & INDEX_PAGE_FLAG_ROOT) != 0u;
        const bool is_rightmost = (header.flags & INDEX_PAGE_FLAG_RIGHTMOST) != 0u;
        const bool is_leftmost = (header.flags & INDEX_PAGE_FLAG_LEFTMOST) != 0u;

        if (is_leftmost && header.left_sibling != 0u)
        {
            return false;
        }
        if (is_rightmost && header.right_sibling != 0u)
        {
            return false;
        }
        if (!is_leftmost && header.left_sibling == 0u)
        {
            return false;
        }
        if (!is_rightmost && header.right_sibling == 0u)
        {
            return false;
        }
        if (is_root)
        {
            if (header.left_sibling != 0u || header.right_sibling != 0u)
            {
                return false;
            }
            if (!is_leftmost || !is_rightmost)
            {
                return false;
            }
        }
        return true;
    }

    inline auto isValidIndexPageHeaderForType(const PageHeader &page_header,
                                              const IndexPageHeader &index_header,
                                              uint16_t expected_opaque_len) -> bool
    {
        if (!isCanonicalIndexPageType(page_header.page_type))
        {
            return false;
        }
        if (!isValidIndexPageHeaderBasic(index_header, expected_opaque_len))
        {
            return false;
        }
        return isValidIndexSiblingContract(index_header);
    }

    static_assert(sizeof(ToastMetaPageLayout) == 56,
                  "ToastMetaPageLayout must be 56 bytes");
    static_assert(sizeof(ToastChunkRecordHeader) == 24,
                  "ToastChunkRecordHeader must be 24 bytes");
    static_assert(sizeof(LobMetaRecordLayout) == 60,
                  "LobMetaRecordLayout must be 60 bytes");
    static_assert(sizeof(LobChunkRecordHeader) == 24,
                  "LobChunkRecordHeader must be 24 bytes");

    constexpr uint32_t K_MAGIC_SBRD = 0x53425244; // 'SBRD' little-endian

    inline auto isValidLobOrToastChunkPayload(uint32_t payload_len, uint32_t chunk_size) -> bool
    {
        return chunk_size != 0u && payload_len <= chunk_size;
    }

    inline auto expectedLobOrToastChunkCount(uint64_t total_len, uint32_t chunk_size) -> uint64_t
    {
        if (chunk_size == 0u)
        {
            return 0u;
        }
        return (total_len + static_cast<uint64_t>(chunk_size) - 1u) /
               static_cast<uint64_t>(chunk_size);
    }

    inline auto isValidLobOrToastChunkIndex(uint32_t chunk_index, uint64_t total_len,
                                            uint32_t chunk_size) -> bool
    {
        const uint64_t chunk_count = expectedLobOrToastChunkCount(total_len, chunk_size);
        return chunk_count != 0u && static_cast<uint64_t>(chunk_index) < chunk_count;
    }

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
        if (header.special_size == 0)
        {
            header.flags &= ~PAGE_FLAG_SPECIAL;
        }
        else
        {
            header.flags |= PAGE_FLAG_SPECIAL;
        }
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

    inline void setObjectUuid(PageHeader &header, const ID &uuid)
    {
        std::memcpy(header.object_uuid, uuid.bytes.data(), 16);
    }

    inline auto getObjectUuid(const PageHeader &header) -> ID
    {
        ID out{};
        std::memcpy(out.bytes.data(), header.object_uuid, 16);
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
        const uint32_t computed = calculatePageChecksum(page, page_size);
        if (computed == header->checksum)
        {
            return true;
        }
        return (header->flags & PAGE_FLAG_CHECKSUM_VALID) == 0u;
    }

    inline auto validatePageHeaderContract(const PageHeader &header, uint32_t expected_page_size,
                                           uint16_t expected_page_type,
                                           const ID *expected_database_uuid,
                                           const ID *expected_object_uuid) -> Status
    {
        if (header.magic != K_MAGIC_SBRD)
        {
            return Status::PAGE_CORRUPT;
        }
        const bool valid_size = header.page_size == 8192U || header.page_size == 16384U ||
                                header.page_size == 32768U || header.page_size == 65536U ||
                                header.page_size == 131072U;
        if (!valid_size)
        {
            return Status::PAGE_CORRUPT;
        }
        if (expected_page_size != 0u && header.page_size != expected_page_size)
        {
            return Status::PAGE_CORRUPT;
        }
        if (expected_page_type != 0xFFFFu && header.page_type != expected_page_type)
        {
            return Status::PAGE_CORRUPT;
        }
        if (expected_database_uuid != nullptr &&
            getDatabaseUuid(header) != *expected_database_uuid)
        {
            return Status::PAGE_CORRUPT;
        }
        if (expected_object_uuid != nullptr && getObjectUuid(header) != *expected_object_uuid)
        {
            return Status::PAGE_CORRUPT;
        }

        const uint32_t lower = pageLower(header);
        const uint32_t upper = pageUpper(header);
        const uint32_t special = pageSpecial(header);
        if (lower < sizeof(PageHeader) || lower > upper || upper > special ||
            special > header.page_size)
        {
            return Status::PAGE_CORRUPT;
        }
        return Status::OK;
    }

    inline auto validatePageContract(const uint8_t *page, uint32_t expected_page_size,
                                     uint16_t expected_page_type,
                                     const ID *expected_database_uuid,
                                     const ID *expected_object_uuid) -> Status
    {
        if (page == nullptr)
        {
            return Status::INVALID_ARGUMENT;
        }
        const auto *header = reinterpret_cast<const PageHeader *>(page);
        const Status header_status = validatePageHeaderContract(
            *header, expected_page_size, expected_page_type, expected_database_uuid,
            expected_object_uuid);
        if (header_status != Status::OK)
        {
            return header_status;
        }
        if (!validatePageChecksum(page, header->page_size))
        {
            return Status::CHECKSUM_MISMATCH;
        }
        return Status::OK;
    }

    inline void preparePageForWrite(uint8_t *page, uint32_t page_size, uint32_t page_id)
    {
        auto *header = reinterpret_cast<PageHeader *>(page);
        const bool had_valid_checksum = (header->flags & PAGE_FLAG_CHECKSUM_VALID) != 0u;
        header->page_id = page_id;
        header->lsn = 0; // Alpha has no WAL and keeps lsn zeroed on write.
        if (had_valid_checksum)
        {
            header->generation += 1;
        }
        else if (header->generation == 0)
        {
            header->generation = 1;
        }
        header->flags |= PAGE_FLAG_CHECKSUM_VALID;
        header->checksum = calculatePageChecksum(page, page_size);
    }

    inline auto isValidAlphaPageSize(uint32_t page_size) -> bool
    {
        return page_size == 8192U || page_size == 16384U || page_size == 32768U ||
               page_size == 65536U || page_size == 131072U;
    }

} // namespace scratchbird::core
