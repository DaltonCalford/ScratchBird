/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <utility>
#include <cstring>
#include <set>
#include <unordered_set>
#include <algorithm>  // for std::sort
#include <chrono>
#include <limits>
#include <thread>

#include "scratchbird/core/btree.h"
#include "scratchbird/core/btree_page.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/index_factory.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/transaction_manager.h"  // Firebird MGA: For isVersionVisible (TIP-based visibility)
#include "scratchbird/core/logger.h"
#include "scratchbird/core/gpid.h"

namespace scratchbird::core
{
    namespace
    {
        constexpr uint32_t kSplitRetryLimit = 16;
        constexpr uint16_t kLeafSearchRestartInterval = 8;
        constexpr uint16_t kInternalSearchRestartInterval = 8;
        constexpr uint32_t kHotLeafReservedFreePercent = 15;
        constexpr uint32_t kCleanupCompactMinFreePercent = 15;

        auto percentThresholdBytes(uint32_t page_size, uint32_t percent) -> uint32_t
        {
            if (page_size <= sizeof(SBBTreePage))
            {
                return 0;
            }

            const uint32_t usable_bytes = page_size - sizeof(SBBTreePage);
            return std::max<uint32_t>(1u, (usable_bytes * percent + 99u) / 100u);
        }

        auto loadBTreeNodeView(const uint8_t *page_data,
                               uint32_t page_size,
                               const uint16_t *offsets,
                               uint16_t index,
                               bool is_leaf,
                               const SBBTreeNode **node_out,
                               const uint8_t **key_data_out,
                               uint32_t *node_size_out) -> bool;

        auto measureLeafGarbageBytes(const uint8_t *page_data,
                                     uint32_t page_size,
                                     uint64_t *garbage_bytes_out) -> bool
        {
            if (garbage_bytes_out == nullptr)
            {
                return false;
            }
            *garbage_bytes_out = 0;

            if (page_data == nullptr || page_size < sizeof(SBBTreePage))
            {
                return false;
            }

            const auto *page = reinterpret_cast<const SBBTreePage *>(page_data);
            if (page->btr_level != 0)
            {
                return true;
            }

            const auto *offsets =
                reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));
            for (uint16_t slot = 0; slot < page->btr_count; ++slot)
            {
                const SBBTreeNode *node = nullptr;
                const uint8_t *node_key_data = nullptr;
                uint32_t node_size = 0;
                if (!loadBTreeNodeView(page_data,
                                       page_size,
                                       offsets,
                                       slot,
                                       true,
                                       &node,
                                       &node_key_data,
                                       &node_size))
                {
                    return false;
                }
                (void)node_key_data;

                if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
                {
                    *garbage_bytes_out += node_size;
                }
            }

            return true;
        }

        auto currentProcIdForIndexLocks() -> uint32_t
        {
            const int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
            return (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0u;
        }

        auto makeIndexPageLockTag(const UuidV7Bytes &index_uuid, uint64_t page_num) -> LockTag
        {
            LockTag tag{};
            tag.target_type = LockTarget::LOCK_TARGET_PAGE;
            tag.object_uuid = index_uuid;
            tag.page_num = page_num;
            return tag;
        }

        auto acquireIndexPageLock(Database *db,
                                  const UuidV7Bytes &index_uuid,
                                  uint64_t page_num,
                                  LockMode mode,
                                  bool wait,
                                  ErrorContext *ctx) -> Status
        {
            if (db == nullptr || db->lock_manager() == nullptr || page_num == 0)
            {
                return Status::OK;
            }
            return db->lock_manager()->acquireLock(currentProcIdForIndexLocks(),
                                                  makeIndexPageLockTag(index_uuid, page_num),
                                                  mode, wait, 0, ctx);
        }

        void releaseIndexPageLock(Database *db,
                                  const UuidV7Bytes &index_uuid,
                                  uint64_t page_num,
                                  LockMode mode,
                                  ErrorContext *ctx)
        {
            if (db == nullptr || db->lock_manager() == nullptr || page_num == 0)
            {
                return;
            }
            (void)db->lock_manager()->releaseLock(currentProcIdForIndexLocks(),
                                                 makeIndexPageLockTag(index_uuid, page_num),
                                                 mode, ctx);
        }

        auto pinIndexPageGlobal(Database *db,
                                uint16_t tablespace_id,
                                uint64_t page_num,
                                void **buffer,
                                ErrorContext *ctx) -> Status
        {
            if (db == nullptr || db->buffer_pool() == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Database/buffer pool unavailable for B-tree page pin");
                return Status::INVALID_ARGUMENT;
            }
            return db->buffer_pool()->pinPageGlobal(makeGPID(tablespace_id, page_num), buffer,
                                                    ctx);
        }

        auto unpinIndexPageGlobal(Database *db,
                                  uint16_t tablespace_id,
                                  uint64_t page_num,
                                  bool dirty,
                                  ErrorContext *ctx) -> Status
        {
            if (db == nullptr || db->buffer_pool() == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Database/buffer pool unavailable for B-tree page unpin");
                return Status::INVALID_ARGUMENT;
            }
            return db->buffer_pool()->unpinPageGlobal(makeGPID(tablespace_id, page_num), dirty,
                                                      ctx);
        }

        auto isBTreeRuntimeFamily(CatalogManager::IndexType index_type) -> bool
        {
            const auto *caps = IndexFactory::lookupCapabilities(index_type);
            return caps != nullptr &&
                   caps->runtime_class == IndexFactory::IndexRuntimeClass::BTREE;
        }

        auto hotRightLeafReservedFreeBytes(uint32_t page_size) -> uint32_t
        {
            if (page_size <= sizeof(SBBTreePage))
            {
                return 0;
            }

            const uint32_t usable_bytes = page_size - sizeof(SBBTreePage);
            return std::max<uint32_t>(
                1u, (usable_bytes * kHotLeafReservedFreePercent + 99u) / 100u);
        }

        auto estimatedLeafInsertFootprint(const std::vector<uint8_t> &key) -> uint32_t
        {
            return static_cast<uint32_t>(sizeof(SBBTreeNode) + key.size() +
                                         sizeof(OnDiskTID) + sizeof(uint16_t));
        }

        auto btreeNodeStorageSize(const SBBTreeNode *node, bool is_leaf) -> uint32_t
        {
            if (node == nullptr)
            {
                return 0;
            }

            uint64_t node_size = sizeof(SBBTreeNode) + static_cast<uint64_t>(node->btn_key_len);
            if (is_leaf)
            {
                node_size += static_cast<uint64_t>(node->btn_tuple_count) * sizeof(OnDiskTID);
            }

            if (node_size > std::numeric_limits<uint32_t>::max())
            {
                return 0;
            }
            return static_cast<uint32_t>(node_size);
        }

        enum class BTreePageShapeKind : uint8_t
        {
            Leaf,
            Internal,
            Invalid,
        };

        struct BTreePageShape
        {
            BTreePageShapeKind kind = BTreePageShapeKind::Invalid;
            bool normalized = false;
            const char *error_message = nullptr;
        };

        auto normalizeBTreePageShape(SBBTreePage *page) -> BTreePageShape
        {
            BTreePageShape result;
            if (page == nullptr)
            {
                result.error_message = "Null B-tree page";
                return result;
            }

            const uint16_t leaf_page_type =
                static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_LEAF);
            const uint16_t internal_page_type =
                static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_INTERNAL);
            const bool leaf_flag =
                (page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0;
            const bool leaf_type = page->btr_header.page_type == leaf_page_type;
            const bool internal_type = page->btr_header.page_type == internal_page_type;

            if (leaf_type)
            {
                if (page->btr_level != 0)
                {
                    result.error_message = "Leaf B-tree page has non-zero level";
                    return result;
                }
                if (page->btr_rightmost_child != 0)
                {
                    result.error_message = "Leaf B-tree page has rightmost child pointer";
                    return result;
                }
                if (!leaf_flag)
                {
                    page->btr_flags |= static_cast<uint16_t>(BTreeFlags::LEAF);
                    result.normalized = true;
                }
                result.kind = BTreePageShapeKind::Leaf;
                return result;
            }

            if (internal_type)
            {
                if (page->btr_level == 0)
                {
                    result.error_message = "Internal B-tree page has zero level";
                    return result;
                }
                if (page->btr_count > 0 && page->btr_rightmost_child == 0)
                {
                    result.error_message = "Internal B-tree page missing rightmost child pointer";
                    return result;
                }
                if (leaf_flag)
                {
                    page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::LEAF);
                    result.normalized = true;
                }
                result.kind = BTreePageShapeKind::Internal;
                return result;
            }

            if (page->btr_level == 0 && page->btr_rightmost_child == 0)
            {
                page->btr_header.page_type = leaf_page_type;
                page->btr_flags |= static_cast<uint16_t>(BTreeFlags::LEAF);
                result.kind = BTreePageShapeKind::Leaf;
                result.normalized = true;
                return result;
            }

            if (page->btr_level > 0)
            {
                if (page->btr_count > 0 && page->btr_rightmost_child == 0)
                {
                    result.error_message = "Internal B-tree page missing rightmost child pointer";
                    return result;
                }
                page->btr_header.page_type = internal_page_type;
                page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::LEAF);
                result.kind = BTreePageShapeKind::Internal;
                result.normalized = true;
                return result;
            }

            result.error_message = "B-tree page leaf/internal shape is invalid";
            return result;
        }

        auto loadBTreeNodeView(const uint8_t *page_data,
                               uint32_t page_size,
                               const uint16_t *offsets,
                               uint16_t node_index,
                               bool is_leaf,
                               const SBBTreeNode **node_out,
                               const uint8_t **key_data_out,
                               uint32_t *node_size_out) -> bool
        {
            if (page_data == nullptr || offsets == nullptr || node_out == nullptr ||
                key_data_out == nullptr || node_size_out == nullptr)
            {
                return false;
            }

            const uint16_t node_offset = offsets[node_index];
            if (node_offset < sizeof(SBBTreePage) ||
                static_cast<uint64_t>(node_offset) + sizeof(SBBTreeNode) > page_size)
            {
                return false;
            }

            const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + node_offset);
            const uint32_t node_size = btreeNodeStorageSize(node, is_leaf);
            if (node_size == 0 || static_cast<uint64_t>(node_offset) + node_size > page_size)
            {
                return false;
            }

            *node_out = node;
            *key_data_out = reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
            *node_size_out = node_size;
            return true;
        }

        void initializeCanonicalBTreePageImage(Database *db,
                                               SBBTreePage *page,
                                               uint32_t page_size,
                                               uint64_t page_num,
                                               const ID &index_uuid,
                                               uint16_t page_type)
        {
            std::memset(page, 0, page_size);

            page->btr_header.magic = K_MAGIC_SBRD;
            page->btr_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1);
            page->btr_header.header_bytes = CANONICAL_PAGE_HEADER_BYTES;
            page->btr_header.page_type = page_type;
            page->btr_header.flags = 0;
            page->btr_header.page_size = page_size;
            page->btr_header.header_checksum = 0;
            page->btr_header.payload_checksum = 0;
            page->btr_header.page_id = page_num;
            setDatabaseUuid(page->btr_header, db != nullptr ? db->uuid() : ID{});
            setObjectUuid(page->btr_header, index_uuid);
            page->btr_header.generation = 0;
            page->btr_header.flush_generation = 0;
            page->btr_header.checkpoint_generation = 0;
            page->btr_header.repair_epoch = 0;
            page->btr_header.repair_state_raw =
                static_cast<uint16_t>(PageRepairState::REPAIR_NONE);
            pageSetLower(page->btr_header, sizeof(SBBTreePage));
            pageSetUpper(page->btr_header, page_size);
            pageSetSpecial(page->btr_header, page_size);
        }

        auto initializeBTreeRuntimePage(Database *db,
                                        uint8_t *page_data,
                                        uint32_t page_size,
                                        uint64_t page_num,
                                        const ID &index_uuid,
                                        const ID &table_uuid,
                                        uint16_t level,
                                        uint16_t flags) -> Status
        {
            auto *page = reinterpret_cast<SBBTreePage *>(page_data);
            const uint16_t page_type =
                (flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0
                    ? static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_LEAF)
                    : static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_INTERNAL);
            initializeCanonicalBTreePageImage(db, page, page_size, page_num, index_uuid,
                                              page_type);

            BTreePage btree_page(page_data, page_size);
            return btree_page.initialize(index_uuid, table_uuid, level, flags);
        }
    } // namespace


    // ============================================================================
    // B-TREE PREFIX COMPRESSION HELPER FUNCTIONS
    // ============================================================================

    // Calculate the common prefix length between two keys
    // Used to determine how many leading bytes can be omitted when storing compressed keys
    // Returns the number of matching bytes from the start of both keys
    static uint16_t calculate_prefix_length(const std::vector<uint8_t>& key1,
                                           const std::vector<uint8_t>& key2)
    {
        uint16_t len = std::min(static_cast<uint16_t>(key1.size()),
                               static_cast<uint16_t>(key2.size()));
        uint16_t prefix = 0;

        while (prefix < len && key1[prefix] == key2[prefix]) {
            prefix++;
        }

        return prefix;
    }

    // Compress a key by storing only the suffix after removing common prefix
    // prev_key: the previous key on the page (used to find common prefix)
    // current_key: the key to compress
    // prefix_len_out: receives the calculated prefix length
    // Returns: vector containing only the suffix (compressed key data)
    static std::vector<uint8_t> compress_key(const std::vector<uint8_t>& prev_key,
                                             const std::vector<uint8_t>& current_key,
                                             uint16_t* prefix_len_out)
    {
        // Calculate common prefix
        uint16_t prefix_len = calculate_prefix_length(prev_key, current_key);

        // Don't compress if:
        // 1. No common prefix exists (prefix_len == 0)
        // 2. Key is too short (< 8 bytes) - overhead exceeds benefit
        // 3. Prefix is too small (< 4 bytes) - not worth the complexity
        if (prefix_len == 0 || current_key.size() < 8 || prefix_len < 4) {
            *prefix_len_out = 0;
            return current_key; // Return full uncompressed key
        }

        // Store only the suffix
        *prefix_len_out = prefix_len;
        std::vector<uint8_t> suffix(current_key.begin() + prefix_len, current_key.end());
        return suffix;
    }

    // Decompress a key by combining the prefix from prev_key with stored suffix
    // prev_key: the previous key on the page (provides the prefix)
    // compressed_key: the stored suffix data
    // prefix_len: length of prefix to take from prev_key
    // Returns: full reconstructed key
    static std::vector<uint8_t> decompress_key(const std::vector<uint8_t>& prev_key,
                                               const uint8_t* compressed_key_data,
                                               uint16_t compressed_key_len,
                                               uint16_t prefix_len)
    {
        // If no compression (prefix_len == 0), return the data as-is
        if (prefix_len == 0) {
            return std::vector<uint8_t>(compressed_key_data,
                                       compressed_key_data + compressed_key_len);
        }

        // Validate prefix length doesn't exceed prev_key size
        if (prefix_len > prev_key.size()) {
            // Corruption detected - return compressed data as fallback
            return std::vector<uint8_t>(compressed_key_data,
                                       compressed_key_data + compressed_key_len);
        }

        // Reconstruct: prefix from prev_key + suffix from compressed data
        std::vector<uint8_t> full_key;
        full_key.reserve(prefix_len + compressed_key_len);

        // Copy prefix from previous key
        full_key.insert(full_key.end(), prev_key.begin(), prev_key.begin() + prefix_len);

        // Append suffix from compressed data
        full_key.insert(full_key.end(), compressed_key_data,
                       compressed_key_data + compressed_key_len);

        return full_key;
    }

    auto extractLeafHighKey(const SBBTreePage *page, std::vector<uint8_t> *key_out) -> bool
    {
        if (page == nullptr || key_out == nullptr)
        {
            return false;
        }

        key_out->clear();
        if (page->btr_count == 0)
        {
            return true;
        }

        const auto *page_data = reinterpret_cast<const uint8_t *>(page);
        const uint32_t page_size = page->btr_header.page_size;
        if (page_size < sizeof(SBBTreePage))
        {
            return false;
        }

        const auto *offsets = reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));
        const uint32_t offset_array_end =
            sizeof(SBBTreePage) + (static_cast<uint32_t>(page->btr_count) * sizeof(uint16_t));
        if (offset_array_end > page->btr_high_water || page->btr_high_water > page_size)
        {
            return false;
        }

        std::vector<uint8_t> prev_key;
        for (uint16_t i = 0; i < page->btr_count; ++i)
        {
            const uint16_t node_offset = offsets[i];
            if (node_offset + sizeof(SBBTreeNode) > page_size)
            {
                return false;
            }

            const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + node_offset);
            const uint32_t node_storage_size = btreeNodeStorageSize(node, true);
            if (node_storage_size == 0 || node_offset + node_storage_size > page_size)
            {
                return false;
            }

            const uint8_t *node_key_data =
                reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
            prev_key =
                decompress_key(prev_key, node_key_data, node->btn_key_len, node->btn_prefix_len);
        }

        *key_out = std::move(prev_key);
        return true;
    }

    struct BTreePageOpenInspection
    {
        Status status = Status::OK;
        const char *error_message = nullptr;
        uint32_t computed_prefix_total = 0;
        uint16_t computed_min_prefix_len = 0;
        bool has_deleted_entries = false;
        bool restart_contract_ready = false;
        uint16_t restart_interval = 0;
        uint16_t restart_count = 0;
    };

    static auto inspect_btree_page(const SBBTreePage *page,
                                   uint32_t expected_page_id,
                                   bool require_root) -> BTreePageOpenInspection
    {
        BTreePageOpenInspection result;
        if (page == nullptr)
        {
            result.status = Status::INVALID_ARGUMENT;
            result.error_message = "Null B-tree page";
            return result;
        }

        const uint32_t page_size = page->btr_header.page_size;
        if (page->btr_header.magic != K_MAGIC_SBRD)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "Invalid B-tree page magic";
            return result;
        }

        if (page_size < sizeof(SBBTreePage))
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "Invalid B-tree page size";
            return result;
        }

        if (page->btr_header.page_id != expected_page_id)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "B-tree page id/header mismatch";
            return result;
        }

        SBBTreePage page_copy = *page;
        const BTreePageShape shape = normalizeBTreePageShape(&page_copy);
        if (shape.kind == BTreePageShapeKind::Invalid)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = shape.error_message != nullptr
                                       ? shape.error_message
                                       : "Invalid B-tree page shape";
            return result;
        }

        const bool is_root =
            (page->btr_flags & static_cast<uint16_t>(BTreeFlags::ROOT)) != 0;
        if (require_root && !is_root)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "B-tree root page missing ROOT flag";
            return result;
        }

        if (is_root && page->btr_parent_page != 0)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "B-tree root page has parent pointer";
            return result;
        }

        if (page->btr_high_water < sizeof(SBBTreePage) || page->btr_high_water > page_size)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "B-tree page high-water mark is invalid";
            return result;
        }

        const uint32_t offset_bytes =
            static_cast<uint32_t>(page->btr_count) * sizeof(uint16_t);
        const uint32_t offset_array_end = sizeof(SBBTreePage) + offset_bytes;
        if (offset_array_end > page->btr_high_water)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "B-tree offset array overlaps node area";
            return result;
        }

        const uint32_t expected_free_space = page->btr_high_water - offset_array_end;
        if (page->btr_free_space != expected_free_space)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "B-tree free-space accounting mismatch";
            return result;
        }

        if (shape.kind == BTreePageShapeKind::Leaf && page->btr_rightmost_child != 0)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "Leaf B-tree page has rightmost child pointer";
            return result;
        }

        if (shape.kind == BTreePageShapeKind::Internal && page->btr_count > 0 &&
            page->btr_rightmost_child == 0)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "Internal B-tree page missing rightmost child pointer";
            return result;
        }

        const auto *page_data = reinterpret_cast<const uint8_t *>(page);
        const auto *offsets =
            reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));
        std::vector<uint8_t> prev_key;
        const uint16_t restart_interval =
            shape.kind == BTreePageShapeKind::Leaf ? kLeafSearchRestartInterval
                                                   : kInternalSearchRestartInterval;
        result.restart_interval = restart_interval;
        result.restart_count =
            (page->btr_count == 0)
                ? 0
                : static_cast<uint16_t>((page->btr_count + restart_interval - 1) /
                                        restart_interval);
        result.restart_contract_ready = true;

        for (uint16_t i = 0; i < page->btr_count; ++i)
        {
            if (offsets[i] < page->btr_high_water)
            {
                result.status = Status::PAGE_CORRUPT;
                result.error_message = "B-tree node offset falls below high-water mark";
                return result;
            }

            const SBBTreeNode *node = nullptr;
            const uint8_t *node_key_data = nullptr;
            uint32_t node_size = 0;
            if (!loadBTreeNodeView(page_data, page_size, offsets, i,
                                   shape.kind == BTreePageShapeKind::Leaf,
                                   &node, &node_key_data, &node_size))
            {
                (void)node_size;
                result.status = Status::PAGE_CORRUPT;
                result.error_message = "B-tree node bounds are invalid";
                return result;
            }

            if (node->btn_prefix_len > prev_key.size())
            {
                result.status = Status::PAGE_CORRUPT;
                result.error_message = "B-tree node prefix exceeds previous key";
                return result;
            }

            if ((i % restart_interval) == 0 && node->btn_prefix_len != 0)
            {
                result.restart_contract_ready = false;
            }

            if (node->btn_prefix_len > 0)
            {
                result.computed_prefix_total += node->btn_prefix_len;
                if (result.computed_min_prefix_len == 0 ||
                    node->btn_prefix_len < result.computed_min_prefix_len)
                {
                    result.computed_min_prefix_len = node->btn_prefix_len;
                }
            }

            if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0 ||
                node->btn_xmax != 0)
            {
                result.has_deleted_entries = true;
            }

            prev_key = decompress_key(prev_key, node_key_data, node->btn_key_len,
                                      node->btn_prefix_len);
        }

        if (page->btr_prefix_total != result.computed_prefix_total)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "B-tree prefix-total metadata mismatch";
            return result;
        }

        if (page->btr_min_prefix_len != result.computed_min_prefix_len)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "B-tree min-prefix metadata mismatch";
            return result;
        }

        const bool compressed_flag =
            (page->btr_flags & static_cast<uint16_t>(BTreeFlags::COMPRESSED)) != 0;
        const bool should_be_marked_compressed =
            (page->btr_prefix_total > 0) || (page->btr_suffix_total > 0);
        if (compressed_flag != should_be_marked_compressed)
        {
            result.status = Status::PAGE_CORRUPT;
            result.error_message = "B-tree compressed-flag metadata mismatch";
            return result;
        }

        return result;
    }

    static auto inspect_root_btree_page(const SBBTreePage *page,
                                        uint32_t expected_page_id) -> BTreePageOpenInspection
    {
        return inspect_btree_page(page, expected_page_id, true);
    }

    // Compute minimal separator key between left_max and right_min for internal nodes.
    // Returns a key S such that left_max < S <= right_min, and S is as short as possible.
    static std::vector<uint8_t> minimal_separator_key(const std::vector<uint8_t> &left_max,
                                                      const std::vector<uint8_t> &right_min,
                                                      uint16_t *suffix_trunc_out)
    {
        if (suffix_trunc_out)
        {
            *suffix_trunc_out = 0;
        }

        if (right_min.empty())
        {
            return right_min;
        }

        // Correctness-first Beta 1 rule:
        // keep full separator keys in internal pages instead of shortened prefixes.
        // The previous prefix-shortening path reduced internal fanout but made routing
        // ambiguous under sustained split propagation. Use the full right-min key as the
        // canonical parent separator until the truncated-separator search contract is
        // re-specified and re-verified end-to-end.
        (void)left_max;
        return right_min;
    }

    struct InternalKeyEntry
    {
        std::vector<uint8_t> key;
        uint16_t suffix_trunc = 0;
        uint16_t flags = 0;
    };

    struct LeafEntryData
    {
        std::vector<uint8_t> key;
        std::vector<OnDiskTID> tids;
        uint16_t flags = 0;
        uint64_t xmin = 0;
        uint64_t xmax = 0;
    };

    static auto validate_internal_children(const std::vector<uint64_t> &children,
                                           const char *phase,
                                           ErrorContext *ctx) -> Status
    {
        if (children.empty())
        {
            if (ctx != nullptr)
            {
                const std::string message =
                    std::string("B-tree internal page has no children during ") + phase;
                ctx->set(Status::PAGE_CORRUPT, message.c_str(), __FILE__, __LINE__, __func__);
            }
            return Status::PAGE_CORRUPT;
        }

        for (size_t i = 0; i < children.size(); ++i)
        {
            if (children[i] == 0)
            {
                if (ctx != nullptr)
                {
                    const std::string message =
                        std::string("B-tree internal page child pointer is zero during ") +
                        phase + " at child index " + std::to_string(i);
                    ctx->set(Status::PAGE_CORRUPT, message.c_str(), __FILE__, __LINE__,
                             __func__);
                }
                return Status::PAGE_CORRUPT;
            }
        }

        return Status::OK;
    }

    static Status rebuild_leaf_page(SBBTreePage *page,
                                    uint32_t page_size,
                                    const std::vector<LeafEntryData> &entries,
                                    ErrorContext *ctx)
    {
        std::vector<uint8_t> temp(page_size, 0);
        auto new_header = *page;
        new_header.btr_header.page_type =
            static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_LEAF);
        new_header.btr_level = 0;
        new_header.btr_flags |= static_cast<uint16_t>(BTreeFlags::LEAF);
        new_header.btr_count = 0;
        new_header.btr_high_water = page_size;
        new_header.btr_free_space = page_size - sizeof(SBBTreePage);
        new_header.btr_prefix_total = 0;
        new_header.btr_suffix_total = 0;
        new_header.btr_min_prefix_len = 0;
        new_header.btr_rightmost_child = 0;

        std::memcpy(temp.data(), &new_header, sizeof(SBBTreePage));
        auto *offsets = reinterpret_cast<uint16_t *>(temp.data() + sizeof(SBBTreePage));

        std::vector<uint8_t> prev_key;
        uint16_t min_prefix = 0;
        bool has_garbage = false;

        for (size_t entry_index = 0; entry_index < entries.size(); ++entry_index)
        {
            const auto &entry = entries[entry_index];
            uint16_t prefix_len = 0;
            std::vector<uint8_t> stored_key = entry.key;
            const bool force_restart_anchor =
                (entry_index % kLeafSearchRestartInterval) == 0;
            const auto compression = static_cast<BTreeCompressionType>(new_header.btr_compression);
            if (!force_restart_anchor && (compression == BTreeCompressionType::PREFIX ||
                                          compression == BTreeCompressionType::BOTH ||
                                          compression == BTreeCompressionType::ADAPTIVE))
            {
                stored_key = compress_key(prev_key, entry.key, &prefix_len);
            }

            if (prefix_len > 0)
            {
                new_header.btr_prefix_total += prefix_len;
                if (min_prefix == 0 || prefix_len < min_prefix)
                {
                    min_prefix = prefix_len;
                }
            }

            const uint32_t node_size =
                sizeof(SBBTreeNode) + stored_key.size() +
                static_cast<uint32_t>(entry.tids.size() * sizeof(OnDiskTID));

            if (new_header.btr_free_space < (node_size + sizeof(uint16_t)))
            {
                return Status::PAGE_FULL;
            }

            new_header.btr_high_water -= node_size;
            auto *node = reinterpret_cast<SBBTreeNode *>(temp.data() + new_header.btr_high_water);
            uint16_t node_flags = entry.flags;
            node_flags &= ~(static_cast<uint16_t>(BTreeNodeFlags::FIRST_ON_PAGE) |
                            static_cast<uint16_t>(BTreeNodeFlags::LAST_ON_PAGE));
            if (entry_index == 0)
            {
                node_flags |= static_cast<uint16_t>(BTreeNodeFlags::FIRST_ON_PAGE);
            }
            if (entry_index + 1 == entries.size())
            {
                node_flags |= static_cast<uint16_t>(BTreeNodeFlags::LAST_ON_PAGE);
            }
            node->btn_flags = node_flags;
            node->btn_prefix_len = prefix_len;
            node->btn_suffix_trunc = 0;
            node->btn_key_len = static_cast<uint16_t>(stored_key.size());
            node->btn_tuple_count = static_cast<uint32_t>(entry.tids.size());
            node->btn_child_page = 0;
            node->btn_xmin = entry.xmin;
            node->btn_xmax = entry.xmax;

            uint8_t *key_location = reinterpret_cast<uint8_t *>(node) + sizeof(SBBTreeNode);
            if (!stored_key.empty())
            {
                std::memcpy(key_location, stored_key.data(), stored_key.size());
            }
            auto *tid_location =
                reinterpret_cast<OnDiskTID *>(key_location + stored_key.size());
            for (size_t t = 0; t < entry.tids.size(); ++t)
            {
                tid_location[t] = entry.tids[t];
            }

            offsets[new_header.btr_count] =
                static_cast<uint16_t>(new_header.btr_high_water);
            new_header.btr_count++;
            new_header.btr_free_space -= (node_size + sizeof(uint16_t));

            prev_key = entry.key;
            has_garbage =
                has_garbage ||
                ((entry.flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0) ||
                (entry.xmax != 0);
        }

        new_header.btr_min_prefix_len = min_prefix;
        if (new_header.btr_prefix_total > 0)
        {
            new_header.btr_flags |= static_cast<uint16_t>(BTreeFlags::COMPRESSED);
        }
        else
        {
            new_header.btr_flags &= ~static_cast<uint16_t>(BTreeFlags::COMPRESSED);
        }

        if (has_garbage)
        {
            new_header.btr_flags |= static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);
        }
        else
        {
            new_header.btr_flags &= ~static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);
        }

        const uint32_t offset_array_end =
            sizeof(SBBTreePage) +
            (static_cast<uint32_t>(new_header.btr_count) * sizeof(uint16_t));
        pageSetLower(new_header.btr_header, offset_array_end);
        pageSetUpper(new_header.btr_header, new_header.btr_high_water);
        pageSetSpecial(new_header.btr_header, page_size);

        std::memcpy(temp.data(), &new_header, sizeof(SBBTreePage));
        std::memcpy(page, temp.data(), page_size);
        return Status::OK;
    }

    static Status rebuild_internal_page(SBBTreePage *page,
                                        uint32_t page_size,
                                        const std::vector<InternalKeyEntry> &keys,
                                        const std::vector<uint64_t> &children,
                                        ErrorContext *ctx)
    {
        if (children.size() != keys.size() + 1)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Invalid internal page rebuild: children size mismatch");
            return Status::INVALID_ARGUMENT;
        }

        Status child_status = validate_internal_children(children, "internal page rebuild", ctx);
        if (child_status != Status::OK)
        {
            return child_status;
        }

        std::vector<uint8_t> temp(page_size, 0);
        auto new_header = *page;
        if (new_header.btr_level == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "Attempted to rebuild internal B-tree page with zero level");
            return Status::PAGE_CORRUPT;
        }
        new_header.btr_header.page_type =
            static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_INTERNAL);
        new_header.btr_flags &= ~static_cast<uint16_t>(BTreeFlags::LEAF);
        new_header.btr_count = 0;
        new_header.btr_high_water = page_size;
        new_header.btr_free_space = page_size - sizeof(SBBTreePage);
        new_header.btr_prefix_total = 0;
        new_header.btr_suffix_total = 0;
        new_header.btr_min_prefix_len = 0;

        std::memcpy(temp.data(), &new_header, sizeof(SBBTreePage));
        auto *offsets = reinterpret_cast<uint16_t *>(temp.data() + sizeof(SBBTreePage));
        std::vector<uint8_t> prev_key;
        uint16_t min_prefix = 0;
        bool has_garbage = false;

        for (size_t i = 0; i < keys.size(); ++i)
        {
            uint16_t prefix_len = 0;
            std::vector<uint8_t> stored_key = keys[i].key;
            const bool force_restart_anchor =
                (i % kInternalSearchRestartInterval) == 0;
            const auto compression = static_cast<BTreeCompressionType>(new_header.btr_compression);
            if (!force_restart_anchor && (compression == BTreeCompressionType::PREFIX ||
                                          compression == BTreeCompressionType::BOTH ||
                                          compression == BTreeCompressionType::ADAPTIVE))
            {
                stored_key = compress_key(prev_key, keys[i].key, &prefix_len);
            }

            if (prefix_len > 0)
            {
                new_header.btr_prefix_total += prefix_len;
                if (min_prefix == 0 || prefix_len < min_prefix)
                {
                    min_prefix = prefix_len;
                }
            }

            const uint32_t node_size =
                sizeof(SBBTreeNode) + stored_key.size() + sizeof(uint64_t);

            if (new_header.btr_free_space < (node_size + sizeof(uint16_t)))
            {
                return Status::PAGE_FULL;
            }

            new_header.btr_high_water -= node_size;
            auto *node = reinterpret_cast<SBBTreeNode *>(temp.data() + new_header.btr_high_water);
            uint16_t node_flags = keys[i].flags;
            node_flags &= ~(static_cast<uint16_t>(BTreeNodeFlags::FIRST_ON_PAGE) |
                            static_cast<uint16_t>(BTreeNodeFlags::LAST_ON_PAGE));
            if (i == 0)
            {
                node_flags |= static_cast<uint16_t>(BTreeNodeFlags::FIRST_ON_PAGE);
            }
            if (i + 1 == keys.size())
            {
                node_flags |= static_cast<uint16_t>(BTreeNodeFlags::LAST_ON_PAGE);
            }
            node->btn_flags = node_flags;
            node->btn_prefix_len = prefix_len;
            node->btn_suffix_trunc = keys[i].suffix_trunc;
            node->btn_key_len = static_cast<uint16_t>(stored_key.size());
            node->btn_tuple_count = 0;
            node->btn_child_page = children[i];
            node->btn_xmin = 0;
            node->btn_xmax = 0;

            uint8_t *key_location = reinterpret_cast<uint8_t *>(node) + sizeof(SBBTreeNode);
            if (!stored_key.empty())
            {
                std::memcpy(key_location, stored_key.data(), stored_key.size());
            }

            offsets[new_header.btr_count] =
                static_cast<uint16_t>(new_header.btr_high_water);
            new_header.btr_count++;
            new_header.btr_free_space -= (node_size + sizeof(uint16_t));
            new_header.btr_suffix_total += keys[i].suffix_trunc;
            prev_key = keys[i].key;
            has_garbage =
                has_garbage ||
                ((keys[i].flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0);
        }

        uint64_t rightmost = children.back();
        if (rightmost == 0 && !children.empty())
        {
            for (auto it = children.rbegin(); it != children.rend(); ++it)
            {
                if (*it != 0)
                {
                    rightmost = *it;
                    break;
                }
            }
        }
        new_header.btr_rightmost_child = rightmost;
        new_header.btr_min_prefix_len = min_prefix;

        if (new_header.btr_prefix_total > 0 || new_header.btr_suffix_total > 0)
        {
            new_header.btr_flags |= static_cast<uint16_t>(BTreeFlags::COMPRESSED);
        }
        else
        {
            new_header.btr_flags &= ~static_cast<uint16_t>(BTreeFlags::COMPRESSED);
        }

        if (has_garbage)
        {
            new_header.btr_flags |= static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);
        }
        else
        {
            new_header.btr_flags &= ~static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);
        }

        const uint32_t offset_array_end =
            sizeof(SBBTreePage) +
            (static_cast<uint32_t>(new_header.btr_count) * sizeof(uint16_t));
        pageSetLower(new_header.btr_header, offset_array_end);
        pageSetUpper(new_header.btr_header, new_header.btr_high_water);
        pageSetSpecial(new_header.btr_header, page_size);

        std::memcpy(temp.data(), &new_header, sizeof(SBBTreePage));
        std::memcpy(page, temp.data(), page_size);
        return Status::OK;
    }

    static auto extract_internal_children(const SBBTreePage *page,
                                          uint32_t page_size,
                                          std::vector<uint64_t> *children_out,
                                          ErrorContext *ctx) -> Status
    {
        if (page == nullptr || children_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "extract_internal_children received null argument");
            return Status::INVALID_ARGUMENT;
        }

        children_out->clear();

        const auto shape = normalizeBTreePageShape(const_cast<SBBTreePage *>(page));
        if (shape.kind != BTreePageShapeKind::Internal)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "extract_internal_children called on non-internal page");
            return Status::PAGE_CORRUPT;
        }

        const auto *page_data = reinterpret_cast<const uint8_t *>(page);
        const auto *offsets =
            reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));

        children_out->reserve(static_cast<size_t>(page->btr_count) + 1);
        for (uint16_t i = 0; i < page->btr_count; ++i)
        {
            const SBBTreeNode *node = nullptr;
            const uint8_t *node_key_data = nullptr;
            uint32_t node_size = 0;
            if (!loadBTreeNodeView(page_data, page_size, offsets, i, false,
                                   &node, &node_key_data, &node_size))
            {
                (void)node_key_data;
                (void)node_size;
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Internal page contains invalid node during child extraction");
                return Status::PAGE_CORRUPT;
            }

            children_out->push_back(node->btn_child_page);
        }

        children_out->push_back(page->btr_rightmost_child);
        return validate_internal_children(*children_out, "internal child extraction", ctx);
    }

    static auto verify_internal_child_layout(const SBBTreePage *page,
                                             uint32_t page_size,
                                             const std::vector<uint64_t> &expected_children,
                                             const char *phase,
                                             ErrorContext *ctx) -> Status
    {
        std::vector<uint64_t> actual_children;
        Status status = extract_internal_children(page, page_size, &actual_children, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (actual_children.size() != expected_children.size())
        {
            if (ctx != nullptr)
            {
                const std::string message =
                    std::string("Internal child layout size mismatch during ") + phase +
                    " expected=" + std::to_string(expected_children.size()) +
                    " actual=" + std::to_string(actual_children.size());
                ctx->set(Status::INDEX_CORRUPTED, message.c_str(), __FILE__, __LINE__, __func__);
            }
            return Status::INDEX_CORRUPTED;
        }

        for (size_t i = 0; i < expected_children.size(); ++i)
        {
            if (actual_children[i] != expected_children[i])
            {
                if (ctx != nullptr)
                {
                    const std::string message =
                        std::string("Internal child layout mismatch during ") + phase +
                        " at index " + std::to_string(i) +
                        " expected=" + std::to_string(expected_children[i]) +
                        " actual=" + std::to_string(actual_children[i]);
                    ctx->set(Status::INDEX_CORRUPTED, message.c_str(), __FILE__, __LINE__,
                             __func__);
                }
                return Status::INDEX_CORRUPTED;
            }
        }

        return Status::OK;
    }

    BTree::BTree(Database *db, SBBTreeIndex index_info)
        : db_(db), index_info_(std::move(index_info))
    {
        // Constructor implementation
    }

    BTree::~BTree()
    {
        // Destructor implementation
    }

    void BTree::clearRightmostLeafHint()
    {
        std::lock_guard<std::mutex> lock(rightmost_leaf_hint_mutex_);
        rightmost_leaf_hint_.page_num = 0;
        rightmost_leaf_hint_.high_key.clear();
    }

    void BTree::updateRightmostLeafHint(uint64_t page_num, const std::vector<uint8_t> &high_key)
    {
        std::lock_guard<std::mutex> lock(rightmost_leaf_hint_mutex_);
        rightmost_leaf_hint_.page_num = page_num;
        rightmost_leaf_hint_.high_key = high_key;
    }

    void BTree::storeCleanupDebtSnapshot(const IndexCleanupDebtSnapshot &snapshot)
    {
        std::lock_guard<std::mutex> lock(cleanup_debt_snapshot_mutex_);
        last_cleanup_debt_snapshot_ = snapshot;
    }

    Status BTree::getCleanupDebtSnapshot(IndexCleanupDebtSnapshot *snapshot_out,
                                         ErrorContext *ctx) const
    {
        (void)ctx;
        if (snapshot_out == nullptr)
        {
            return Status::INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(cleanup_debt_snapshot_mutex_);
        *snapshot_out = last_cleanup_debt_snapshot_;
        return Status::OK;
    }

    auto BTree::create(Database *db, const UuidV7Bytes &index_uuid, const UuidV7Bytes &table_uuid,
                       const std::vector<UuidV7Bytes> &column_uuids, GPID root_gpid,
                       ErrorContext *ctx) -> Status
    {
        if (!db || root_gpid == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to BTree::create");
            return Status::INVALID_ARGUMENT;
        }

        BufferPool *buffer_pool = db->buffer_pool();
        if (!buffer_pool)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database has no buffer pool");
            return Status::INVALID_ARGUMENT;
        }

        uint32_t root_page = static_cast<uint32_t>(getPageNumber(root_gpid));

        // Step 2: Pin root page
        void *root_page_data_ptr = nullptr;
        Status status = buffer_pool->pinPageGlobal(root_gpid, &root_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin root page for B-tree");
            return status;
        }

        uint32_t page_size = db->page_size();
        const uint16_t root_flags = static_cast<uint16_t>(BTreeFlags::ROOT) |
                                    static_cast<uint16_t>(BTreeFlags::LEAF) |
                                    static_cast<uint16_t>(BTreeFlags::LEFTMOST) |
                                    static_cast<uint16_t>(BTreeFlags::RIGHTMOST);

        // Step 6: Initialize BTreePage helper and call initialize()
        try
        {
            status = initializeBTreeRuntimePage(
                db, reinterpret_cast<uint8_t *>(root_page_data_ptr), page_size, root_page,
                index_uuid, table_uuid, 0, root_flags);
            if (status != Status::OK)
            {
                buffer_pool->unpinPageGlobal(root_gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to initialize B-tree page");
                return status;
            }
        }
        catch (const std::exception &e)
        {
            buffer_pool->unpinPageGlobal(root_gpid, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, e.what());
            return Status::INVALID_ARGUMENT;
        }

        // Step 7: Unpin page (mark dirty)
        buffer_pool->unpinPageGlobal(root_gpid, true, ctx);
        return Status::OK;
    }

    auto BTree::open(Database *db, const UuidV7Bytes &index_uuid, GPID root_gpid,
                     ErrorContext *ctx) -> std::unique_ptr<BTree>
    {
        if (!db)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database");
            return nullptr;
        }

        BufferPool *buffer_pool = db->buffer_pool();
        if (!buffer_pool)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database has no buffer pool");
            return nullptr;
        }

        uint32_t root_page = static_cast<uint32_t>(getPageNumber(root_gpid));

        // Step 1: Pin root page to validate
        void *root_page_data_ptr = nullptr;
        Status status = buffer_pool->pinPageGlobal(root_gpid, &root_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin root page");
            return nullptr;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(root_page_data_ptr);

        // Step 2: Verify it's a B-tree page (check page_type)
        if (page->btr_header.page_type != static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_LEAF) &&
            page->btr_header.page_type != static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_INTERNAL))
        {
            buffer_pool->unpinPageGlobal(root_gpid, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid B-tree page type");
            return nullptr;
        }

        if (page->btr_header.page_size != db->page_size())
        {
            buffer_pool->unpinPageGlobal(root_gpid, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "B-tree page size does not match database");
            return nullptr;
        }

        // Step 3: Verify index_uuid matches
        if (std::memcmp(page->btr_index_uuid.bytes.data(), index_uuid.bytes.data(), 16) != 0)
        {
            buffer_pool->unpinPageGlobal(root_gpid, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "B-tree index UUID mismatch");
            return nullptr;
        }

        const BTreePageOpenInspection inspection = inspect_root_btree_page(page, root_page);
        if (inspection.status != Status::OK)
        {
            buffer_pool->unpinPageGlobal(root_gpid, false, ctx);
            SET_ERROR_CONTEXT(ctx, inspection.status, inspection.error_message);
            return nullptr;
        }

        // Step 4: Load SBBTreeIndex structure from page
        SBBTreeIndex index_info;
        std::memcpy(index_info.idx_uuid.bytes.data(), page->btr_index_uuid.bytes.data(), 16);
        std::memcpy(index_info.idx_table_uuid.bytes.data(), page->btr_table_uuid.bytes.data(), 16);
        index_info.idx_root_page = root_page;
        index_info.idx_tablespace_id = getTablespaceID(root_gpid);
        index_info.idx_height = page->btr_level + 1;
        index_info.idx_column_ids.clear();
        index_info.idx_flags = 0;
        index_info.idx_tuple_count = 0;
        index_info.idx_page_count = 1;
        index_info.idx_deleted_count = 0;
        index_info.idx_collation_id = 100;

        CatalogManager::IndexInfo catalog_info;
        ErrorContext catalog_ctx;
        auto *catalog = db->catalog_manager();
        if (catalog && catalog->getIndex(index_info.idx_uuid, catalog_info, &catalog_ctx) == Status::OK)
        {
            if (!isBTreeRuntimeFamily(catalog_info.index_type))
            {
                buffer_pool->unpinPageGlobal(root_gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Catalog index type is not mapped to the B-tree runtime");
                return nullptr;
            }

            if (catalog_info.root_gpid != 0 && catalog_info.root_gpid != root_gpid)
            {
                buffer_pool->unpinPageGlobal(root_gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Catalog/root GPID mismatch for B-tree open");
                return nullptr;
            }

            if (catalog_info.table_id != ID{} && catalog_info.table_id != index_info.idx_table_uuid)
            {
                buffer_pool->unpinPageGlobal(root_gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Catalog/root table UUID mismatch for B-tree open");
                return nullptr;
            }

            index_info.idx_table_uuid = catalog_info.table_id;
            index_info.idx_column_ids = catalog_info.column_ids;
            index_info.idx_flags = catalog_info.is_unique ? 1u : 0u;
            index_info.idx_tablespace_id =
                catalog_info.tablespace_id != 0 ? catalog_info.tablespace_id
                                                : getTablespaceID(root_gpid);
            index_info.idx_collation_id = catalog_info.collation_id;
        }

        // Step 5: Unpin page
        buffer_pool->unpinPageGlobal(root_gpid, false, ctx);

        // Step 6: Create and return BTree instance
        return std::make_unique<BTree>(db, index_info);
    }

    GPID BTree::indexGPID(uint64_t page_num) const
    {
        return makeGPID(index_info_.idx_tablespace_id, page_num);
    }

    Status BTree::persistRootGPID(uint64_t root_page_num, ErrorContext *ctx)
    {
        auto *catalog = db_ != nullptr ? db_->catalog_manager() : nullptr;
        if (catalog == nullptr)
        {
            return Status::OK;
        }

        const Status status = catalog->updateIndexRootGPID(index_info_.idx_uuid,
                                                           indexGPID(root_page_num),
                                                           ctx);
        if (status == Status::NOT_FOUND || status == Status::INDEX_NOT_FOUND)
        {
            if (ctx != nullptr)
            {
                ctx->code = Status::OK;
                ctx->message.clear();
            }
            return Status::OK;
        }
        return status;
    }

    Status BTree::pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx,
                               BufferPool::AccessStrategy strategy)
    {
        return db_->buffer_pool()->pinPageGlobal(indexGPID(page_num), buffer, ctx, strategy);
    }

    Status BTree::unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx)
    {
        return db_->buffer_pool()->unpinPageGlobal(indexGPID(page_num), dirty, ctx);
    }

    auto BTree::updateSplitSiblingLinks(uint64_t left_page_num,
                                        uint64_t right_page_num,
                                        SBBTreePage *left_page,
                                        SBBTreePage *right_page,
                                        ErrorContext *ctx) -> Status
    {
        const uint64_t old_right_sibling = left_page->btr_right_sibling;
        if (old_right_sibling != 0)
        {
            Status status = acquireIndexPageLock(db_, index_info_.idx_uuid, old_right_sibling,
                                                 LockMode::LOCK_EXCLUSIVE, false, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status,
                                  "Failed to acquire old right sibling lock during B-tree split");
                return status;
            }

            void *old_right_data_ptr = nullptr;
            status = pinIndexPageGlobal(db_, index_info_.idx_tablespace_id, old_right_sibling,
                                        &old_right_data_ptr, ctx);
            if (status != Status::OK)
            {
                releaseIndexPageLock(db_, index_info_.idx_uuid, old_right_sibling,
                                     LockMode::LOCK_EXCLUSIVE, ctx);
                return status;
            }

            auto *old_right_page = reinterpret_cast<SBBTreePage *>(old_right_data_ptr);
            old_right_page->btr_left_sibling = right_page_num;
            unpinIndexPageGlobal(db_, index_info_.idx_tablespace_id, old_right_sibling, true, ctx);
            releaseIndexPageLock(db_, index_info_.idx_uuid, old_right_sibling,
                                 LockMode::LOCK_EXCLUSIVE, ctx);
        }

        left_page->btr_right_sibling = right_page_num;
        right_page->btr_left_sibling = left_page_num;
        right_page->btr_right_sibling = old_right_sibling;

        if ((left_page->btr_flags & static_cast<uint16_t>(BTreeFlags::RIGHTMOST)) != 0)
        {
            left_page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
            right_page->btr_flags |= static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
        }

        return Status::OK;
    }

    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    // Task 17 MGA Phase 3.1: Added xid parameter for transaction tracking
    auto BTree::insert(const std::vector<uint8_t> &key, const TID &tid, uint64_t xid,
                       ErrorContext *ctx)
        -> Status
    {
        // TOAST Detoasting: Keys are detoasted at higher level before insert
        // StorageEngine uses IndexKeyExtractor::extractKey() which calls
        // toast_mgr->detoastIfNeeded() to expand TOAST pointers before indexing.
        // This follows Option 3 from the original architectural analysis:
        // - BTree::insert() receives pre-detoasted keys from StorageEngine
        // - IndexKeyExtractor caches detoasted values for efficiency
        // - No ToastManager reference needed in index classes

        for (uint32_t attempt = 0; attempt < kSplitRetryLimit; ++attempt)
        {
            // Find the appropriate leaf page for this key
            uint64_t leaf_page_num = 0;
            Status status = find_leaf_page(key, &leaf_page_num, true, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Get proc_id from ConnectionContext (Phase 2 complete)
            int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
            const uint32_t proc_id =
                (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;
            LockManager *lock_mgr = db_->lock_manager();

            void *page_data_ptr = nullptr;
            status = pinIndexPage(leaf_page_num, &page_data_ptr, ctx);
            if (status != Status::OK)
            {
                if (lock_mgr != nullptr)
                {
                    LockTag leaf_tag{};
                    leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    leaf_tag.object_uuid = index_info_.idx_uuid;
                    leaf_tag.page_num = leaf_page_num;
                    lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
                }
                return status;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
            uint32_t page_size = page->btr_header.page_size;
            const BTreePageShape page_shape = normalizeBTreePageShape(page);
            if (page_shape.kind == BTreePageShapeKind::Invalid)
            {
                unpinIndexPage(leaf_page_num, false, ctx);

                if (lock_mgr != nullptr)
                {
                    LockTag leaf_tag{};
                    leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    leaf_tag.object_uuid = index_info_.idx_uuid;
                    leaf_tag.page_num = leaf_page_num;
                    lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
                }

                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  page_shape.error_message != nullptr
                                      ? page_shape.error_message
                                      : "Invalid leaf page shape during insert");
                return Status::PAGE_CORRUPT;
            }
            if (page_shape.kind != BTreePageShapeKind::Leaf)
            {
                unpinIndexPage(leaf_page_num, page_shape.normalized, ctx);

                if (lock_mgr != nullptr)
                {
                    LockTag leaf_tag{};
                    leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    leaf_tag.object_uuid = index_info_.idx_uuid;
                    leaf_tag.page_num = leaf_page_num;
                    lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
                }

                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "find_leaf_page returned non-leaf page during insert");
                return Status::PAGE_CORRUPT;
            }
            bool page_modified = page_shape.normalized;

            Tuple tuple;
            tuple.tid = tid;
            tuple.data = nullptr;
            tuple.data_size = 0;

            try
            {
                BTreePage btree_page(reinterpret_cast<uint8_t *>(page_data_ptr), page_size);
                bool should_presplit_hot_right_edge = false;
                bool hot_right_edge_detected = false;
                if ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::RIGHTMOST)) != 0 &&
                    (page->btr_flags & static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE)) == 0 &&
                    page->btr_count > 0)
                {
                    std::vector<uint8_t> current_high_key;
                    if (extractLeafHighKey(page, &current_high_key) && !current_high_key.empty() &&
                        compare_keys(key, current_high_key) > 0)
                    {
                        hot_right_edge_detected = true;
                        const uint32_t reserved_free =
                            hotRightLeafReservedFreeBytes(page_size);
                        const uint32_t insert_footprint =
                            estimatedLeafInsertFootprint(key);
                        const uint32_t leaf_free_space = page->btr_free_space;

                        should_presplit_hot_right_edge =
                            reserved_free > 0 &&
                            leaf_free_space > insert_footprint &&
                            leaf_free_space <= reserved_free + insert_footprint;
                    }
                }
                if (hot_right_edge_detected)
                {
                    hot_leaf_right_edge_detections_.fetch_add(1, std::memory_order_relaxed);
                }
                if (should_presplit_hot_right_edge)
                {
                    hot_leaf_right_edge_presplits_.fetch_add(1, std::memory_order_relaxed);
                }

                status = should_presplit_hot_right_edge
                             ? Status::PAGE_FULL
                             : btree_page.add_node(key, tuple, xid, ctx);
                if (status == Status::PAGE_FULL)
                {
                    if ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE)) != 0)
                    {
                        GcCompactionStats compact_stats{};
                        Status compact_status =
                            compactPage(reinterpret_cast<uint8_t *>(page_data_ptr), page_size,
                                        compact_stats, ctx);
                        if (compact_status != Status::OK)
                        {
                            unpinIndexPage(leaf_page_num, page_modified, ctx);

                            if (lock_mgr != nullptr)
                            {
                                LockTag leaf_tag{};
                                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                                leaf_tag.object_uuid = index_info_.idx_uuid;
                                leaf_tag.page_num = leaf_page_num;
                                lock_mgr->releaseLock(proc_id, leaf_tag,
                                                      LockMode::LOCK_EXCLUSIVE, ctx);
                            }

                            return compact_status;
                        }
                        page_modified = true;
                        status = btree_page.add_node(key, tuple, xid, ctx);
                    }

                    if (status == Status::OK)
                    {
                        if ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::RIGHTMOST)) != 0)
                        {
                            updateRightmostLeafHint(leaf_page_num, key);
                        }

                        unpinIndexPage(leaf_page_num, true, ctx);

                        if (lock_mgr != nullptr)
                        {
                            LockTag leaf_tag{};
                            leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                            leaf_tag.object_uuid = index_info_.idx_uuid;
                            leaf_tag.page_num = leaf_page_num;
                            lock_mgr->releaseLock(proc_id, leaf_tag,
                                                  LockMode::LOCK_EXCLUSIVE, ctx);
                        }

                        if (bloom_filter_)
                        {
                            Status bf_status = bloom_filter_->insert(key.data(), key.size(), ctx);
                            if (bf_status != Status::OK)
                            {
                                LOG_WARNING(STORAGE, "B-Tree bloom filter insert failed: %d",
                                            static_cast<int>(bf_status));
                            }
                        }

                        return Status::OK;
                    }

                    if (status != Status::PAGE_FULL)
                    {
                        unpinIndexPage(leaf_page_num, page_modified, ctx);

                        if (lock_mgr != nullptr)
                        {
                            LockTag leaf_tag{};
                            leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                            leaf_tag.object_uuid = index_info_.idx_uuid;
                            leaf_tag.page_num = leaf_page_num;
                            lock_mgr->releaseLock(proc_id, leaf_tag,
                                                  LockMode::LOCK_EXCLUSIVE, ctx);
                        }

                        return status;
                    }

                    unpinIndexPage(leaf_page_num, page_modified, ctx);
                    clearRightmostLeafHint();

                    status = split_leaf_page(leaf_page_num, key, tid, xid, nullptr, ctx);

                    if (lock_mgr != nullptr)
                    {
                        LockTag leaf_tag{};
                        leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                        leaf_tag.object_uuid = index_info_.idx_uuid;
                        leaf_tag.page_num = leaf_page_num;
                        lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
                    }

                    if (status == Status::OK)
                    {
                        if (bloom_filter_)
                        {
                            Status bf_status = bloom_filter_->insert(key.data(), key.size(), ctx);
                            if (bf_status != Status::OK)
                            {
                                LOG_WARNING(STORAGE, "B-Tree bloom filter insert failed: %d",
                                            static_cast<int>(bf_status));
                            }
                        }
                        return Status::OK;
                    }

                    if (status == Status::LOCK_CONFLICT ||
                        status == Status::LOCK_TIMEOUT || status == Status::DEADLOCK)
                    {
                        if (hot_right_edge_detected)
                        {
                            hot_leaf_right_edge_split_retries_.fetch_add(
                                1, std::memory_order_relaxed);
                        }
                        std::this_thread::yield();
                        continue;
                    }
                    return status;
                }
                else if (status != Status::OK)
                {
                    unpinIndexPage(leaf_page_num, page_modified, ctx);

                    if (lock_mgr != nullptr)
                    {
                        LockTag leaf_tag{};
                        leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                        leaf_tag.object_uuid = index_info_.idx_uuid;
                        leaf_tag.page_num = leaf_page_num;
                        lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
                    }

                    return status;
                }

                if ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::RIGHTMOST)) != 0)
                {
                    updateRightmostLeafHint(leaf_page_num, key);
                }

                unpinIndexPage(leaf_page_num, true, ctx);

                if (lock_mgr != nullptr)
                {
                    LockTag leaf_tag{};
                    leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    leaf_tag.object_uuid = index_info_.idx_uuid;
                    leaf_tag.page_num = leaf_page_num;
                    lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
                }

                if (bloom_filter_)
                {
                    Status bf_status = bloom_filter_->insert(key.data(), key.size(), ctx);
                    if (bf_status != Status::OK)
                    {
                        LOG_WARNING(STORAGE, "B-Tree bloom filter insert failed: %d",
                                    static_cast<int>(bf_status));
                    }
                }

                return Status::OK;
            }
            catch (const std::exception &e)
            {
                unpinIndexPage(leaf_page_num, page_modified, ctx);

                if (lock_mgr != nullptr)
                {
                    LockTag leaf_tag{};
                    leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    leaf_tag.object_uuid = index_info_.idx_uuid;
                    leaf_tag.page_num = leaf_page_num;
                    lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
                }

                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, e.what());
                return Status::INVALID_ARGUMENT;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::LOCK_TIMEOUT,
                          "B-tree insert exceeded split retry budget under hot-page contention");
        return Status::LOCK_TIMEOUT;
    }

    // Searches for a key within a single B-Tree leaf page by binary-searching
    // deterministic restart anchors and then scanning within the bounded block.
    // Pages that do not satisfy the restart-anchor contract fall back to the
    // full linear/decompress path for compatibility.
    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    // Task 17 MGA Phase 3.3: Added snapshot parameter for visibility filtering
    auto BTree::searchPage(const SBBTreePage *page, const std::vector<uint8_t> &key,
                           uint64_t current_xid,
                           std::vector<TID> *tids_out) const -> bool
    {
        if (page == nullptr || tids_out == nullptr)
        {
            return false;
        }

        const auto *page_data = reinterpret_cast<const uint8_t *>(page);
        const uint32_t page_size = page->btr_header.page_size;
        if (page_size < sizeof(SBBTreePage))
        {
            return false;
        }

        const auto *offsets = reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));
        const uint32_t offset_array_end =
            sizeof(SBBTreePage) + (static_cast<uint32_t>(page->btr_count) * sizeof(uint16_t));
        if (offset_array_end > page->btr_high_water || page->btr_high_water > page_size)
        {
            return false;
        }

        auto node_view = [&](uint16_t node_index,
                             const SBBTreeNode **node_out,
                             const uint8_t **key_data_out) -> bool
        {
            uint32_t node_size = 0;
            if (!loadBTreeNodeView(page_data, page_size, offsets, node_index, true,
                                   node_out, key_data_out, &node_size))
            {
                return false;
            }
            if (offsets[node_index] < page->btr_high_water)
            {
                return false;
            }
            return true;
        };

        auto linear_scan = [&]() -> bool
        {
            std::vector<uint8_t> prev_key;
            bool found_any = false;
            for (uint16_t i = 0; i < page->btr_count; ++i)
            {
                const SBBTreeNode *node = nullptr;
                const uint8_t *node_key_data = nullptr;
                if (!node_view(i, &node, &node_key_data))
                {
                    return false;
                }
                std::vector<uint8_t> full_key = decompress_key(prev_key, node_key_data,
                                                               node->btn_key_len,
                                                               node->btn_prefix_len);
                const bool is_deleted =
                    (node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0;
                int cmp = compare_keys(key, full_key.data(), full_key.size());
                if (cmp == 0)
                {
                    if (!is_deleted && isEntryVisible(node->btn_xmin, node->btn_xmax, current_xid))
                    {
                        const auto *tuple_ids_ptr = reinterpret_cast<const OnDiskTID *>(
                            node_key_data + node->btn_key_len);
                        for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
                        {
                            tids_out->push_back(fromOnDiskTID(tuple_ids_ptr[j]));
                        }
                        found_any = true;
                    }
                }
                else if (cmp < 0)
                {
                    break;
                }
                prev_key = full_key;
            }
            return found_any;
        };

        if (page->btr_count == 0)
        {
            return false;
        }

        const uint16_t restart_interval = kLeafSearchRestartInterval;
        bool restart_contract_ready = true;
        for (uint16_t anchor_index = 0; anchor_index < page->btr_count;
             anchor_index = static_cast<uint16_t>(anchor_index + restart_interval))
        {
            const SBBTreeNode *anchor_node = nullptr;
            const uint8_t *anchor_key_data = nullptr;
            if (!node_view(anchor_index, &anchor_node, &anchor_key_data))
            {
                return false;
            }
            if (anchor_node->btn_prefix_len != 0)
            {
                restart_contract_ready = false;
                break;
            }
        }

        if (!restart_contract_ready)
        {
            return linear_scan();
        }

        const uint16_t anchor_count = static_cast<uint16_t>(
            (page->btr_count + restart_interval - 1) / restart_interval);
        auto anchor_node_index = [](uint16_t anchor_slot) -> uint16_t
        {
            return static_cast<uint16_t>(anchor_slot * kLeafSearchRestartInterval);
        };
        auto compare_anchor = [&](uint16_t anchor_slot) -> int
        {
            const uint16_t node_index = anchor_node_index(anchor_slot);
            const SBBTreeNode *node = nullptr;
            const uint8_t *node_key_data = nullptr;
            if (!node_view(node_index, &node, &node_key_data))
            {
                return 1;
            }
            return compare_keys(key, node_key_data, node->btn_key_len);
        };

        uint16_t low = 0;
        uint16_t high = anchor_count;
        while (low < high)
        {
            const uint16_t mid = static_cast<uint16_t>(low + ((high - low) / 2));
            if (compare_anchor(mid) > 0)
            {
                low = static_cast<uint16_t>(mid + 1);
            }
            else
            {
                high = mid;
            }
        }

        const uint16_t first_not_less_anchor = low;
        low = 0;
        high = anchor_count;
        while (low < high)
        {
            const uint16_t mid = static_cast<uint16_t>(low + ((high - low) / 2));
            if (compare_anchor(mid) < 0)
            {
                high = mid;
            }
            else
            {
                low = static_cast<uint16_t>(mid + 1);
            }
        }

        const uint16_t first_greater_anchor = low;
        if (first_greater_anchor == 0)
        {
            return false;
        }

        uint16_t start_anchor = static_cast<uint16_t>(first_greater_anchor - 1);
        if (first_not_less_anchor < anchor_count && compare_anchor(first_not_less_anchor) == 0)
        {
            start_anchor =
                (first_not_less_anchor == 0) ? 0 : static_cast<uint16_t>(first_not_less_anchor - 1);
        }

        const uint16_t start_index = anchor_node_index(start_anchor);
        const uint16_t end_index =
            (first_greater_anchor < anchor_count) ? anchor_node_index(first_greater_anchor)
                                                  : page->btr_count;
        const SBBTreeNode *start_node = nullptr;
        const uint8_t *start_key_data = nullptr;
        if (!node_view(start_index, &start_node, &start_key_data))
        {
            return false;
        }
        std::vector<uint8_t> prev_key(start_key_data, start_key_data + start_node->btn_key_len);
        bool found_any = false;

        for (uint16_t i = start_index; i < end_index; ++i)
        {
            const SBBTreeNode *node = nullptr;
            const uint8_t *node_key_data = nullptr;
            if (!node_view(i, &node, &node_key_data))
            {
                return false;
            }
            std::vector<uint8_t> full_key =
                (i == start_index)
                    ? prev_key
                    : decompress_key(prev_key, node_key_data, node->btn_key_len,
                                     node->btn_prefix_len);
            const bool is_deleted =
                (node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0;
            int cmp = compare_keys(key, full_key.data(), full_key.size());
            if (cmp == 0)
            {
                if (!is_deleted && isEntryVisible(node->btn_xmin, node->btn_xmax, current_xid))
                {
                    const auto *tuple_ids_ptr =
                        reinterpret_cast<const OnDiskTID *>(node_key_data + node->btn_key_len);
                    for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
                    {
                        tids_out->push_back(fromOnDiskTID(tuple_ids_ptr[j]));
                    }
                    found_any = true;
                }
            }
            else if (cmp < 0)
            {
                break;
            }
            prev_key = full_key;
        }

        return found_any;
    }

    auto BTree::find_leaf_page(const std::vector<uint8_t> &key, uint64_t *page_num_out,
                               bool write_lock, ErrorContext *ctx) -> Status
    {
        LockManager *lock_mgr = db_->lock_manager();

        // Get proc_id from ConnectionContext (thread-local storage)
        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        const uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;

        uint64_t hinted_page_num = 0;
        std::vector<uint8_t> hinted_high_key;
        {
            std::lock_guard<std::mutex> lock(rightmost_leaf_hint_mutex_);
            hinted_page_num = rightmost_leaf_hint_.page_num;
            hinted_high_key = rightmost_leaf_hint_.high_key;
        }

        if (hinted_page_num != 0 &&
            (hinted_high_key.empty() || compare_keys(key, hinted_high_key) >= 0))
        {
            void *page_data_ptr = nullptr;
            Status status = pinIndexPage(hinted_page_num, &page_data_ptr, ctx);
            if (status != Status::OK)
            {
                clearRightmostLeafHint();
            }
            else
            {
                if (lock_mgr != nullptr)
                {
                    LockTag page_tag{};
                    page_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    page_tag.object_uuid = index_info_.idx_uuid;
                    page_tag.page_num = hinted_page_num;
                    page_tag.offset_num = 0;
                    page_tag.padding = 0;

                    const LockMode lock_mode =
                        write_lock ? LockMode::LOCK_EXCLUSIVE : LockMode::LOCK_SHARE;
                    status = lock_mgr->acquireLock(proc_id, page_tag, lock_mode, true, 0, ctx);
                    if (status != Status::OK)
                    {
                        unpinIndexPage(hinted_page_num, false, ctx);
                        return status;
                    }
                }

                auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
                const BTreePageShape shape = normalizeBTreePageShape(page);
                std::vector<uint8_t> actual_high_key;
                const bool extracted_high_key = extractLeafHighKey(page, &actual_high_key);
                const bool is_rightmost =
                    (page->btr_flags & static_cast<uint16_t>(BTreeFlags::RIGHTMOST)) != 0;
                const bool hint_valid =
                    shape.kind == BTreePageShapeKind::Leaf &&
                    is_rightmost &&
                    extracted_high_key &&
                    (actual_high_key.empty() || compare_keys(key, actual_high_key) >= 0);

                if (hint_valid)
                {
                    if (!actual_high_key.empty())
                    {
                        updateRightmostLeafHint(hinted_page_num, actual_high_key);
                    }
                    *page_num_out = hinted_page_num;
                    unpinIndexPage(hinted_page_num, shape.normalized, ctx);
                    return Status::OK;
                }

                unpinIndexPage(hinted_page_num, shape.normalized, ctx);

                if (lock_mgr != nullptr)
                {
                    LockTag page_tag{};
                    page_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    page_tag.object_uuid = index_info_.idx_uuid;
                    page_tag.page_num = hinted_page_num;
                    lock_mgr->releaseLock(
                        proc_id, page_tag,
                        write_lock ? LockMode::LOCK_EXCLUSIVE : LockMode::LOCK_SHARE, ctx);
                }

                if (shape.kind != BTreePageShapeKind::Leaf || !is_rightmost ||
                    !extracted_high_key)
                {
                    clearRightmostLeafHint();
                }
                else if (!actual_high_key.empty())
                {
                    updateRightmostLeafHint(hinted_page_num, actual_high_key);
                }
            }
        }

        uint64_t current_page_num = index_info_.idx_root_page;
        uint64_t previous_page_num = 0; // For lock coupling

        while (true)
        {
            void *page_data_ptr;
            Status status = pinIndexPage(current_page_num, &page_data_ptr, ctx);
            if (status != Status::OK)
            {
                // Release previous lock if held
                if (previous_page_num != 0 && lock_mgr != nullptr)
                {
                    LockTag prev_tag{};
                    prev_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    prev_tag.object_uuid = index_info_.idx_uuid;
                    prev_tag.page_num = previous_page_num;
                    lock_mgr->releaseLock(
                        proc_id, prev_tag,
                        write_lock ? LockMode::LOCK_EXCLUSIVE : LockMode::LOCK_SHARE, ctx);
                }
                return status;
            }

            // HIGH-3 FIX: Document B-Tree lock coupling pattern
            //
            // LOCK COUPLING PROTOCOL (Crabbing/Hand-Over-Hand Locking)
            // ========================================================
            //
            // Purpose: Safely traverse B-tree during concurrent operations while minimizing lock contention.
            //
            // Algorithm Overview:
            // 1. Start at root, acquire lock on current page
            // 2. Read current page to find next child
            // 3. Acquire lock on child page (now holding TWO locks)
            // 4. Release lock on parent page (hand-over-hand motion)
            // 5. Repeat steps 2-4 until reaching leaf
            //
            // Correctness Guarantees:
            // - Always hold at least ONE lock during traversal (prevents page eviction)
            // - Briefly hold TWO locks during transition (parent + child)
            // - Release parent ONLY AFTER successfully acquiring child lock
            // - If child lock fails, keep parent lock and return error
            //
            // Concurrency Benefits:
            // - Multiple readers can traverse tree simultaneously (SHARE locks)
            // - Writers use EXCLUSIVE locks, blocking readers at that page
            // - Releasing parent early allows other threads to access upper tree
            // - Minimizes lock hold time compared to holding all locks from root to leaf
            //
            // Why Not Hold All Locks?
            // - Holding root→leaf locks would serialize ALL tree operations
            // - Lock coupling allows concurrent access to different tree branches
            // - Example: Thread A descending left branch, Thread B descending right branch
            //
            // Edge Cases Handled:
            // - Lock acquisition failure: Keep previous lock, unpin current, return error
            // - Leaf page reached: Keep lock held for caller (insert/search/delete needs it)
            // - Previous lock = 0: First iteration (root), no previous lock to release
            //
            // Performance Characteristics:
            // - Lock hold time: O(tree_height) instead of O(1) for whole-tree lock
            // - Concurrency: Multiple operations can proceed in parallel
            // - Deadlock-free: Always acquire locks in top-down order (root→leaf)
            //
            // Alternative Approaches (Not Used):
            // - Optimistic Lock Coupling: Release parent before acquiring child (risky)
            // - B-link Trees: Right-link pointers allow lock-free traversal (complex)
            // - Lock-free Algorithms: Require atomic compare-and-swap (high complexity)
            //
            // Thread Safety:
            // - Each thread maintains its own previous_page_num variable (line 443)
            // - Lock manager handles concurrent lock requests with wait queues
            // - Buffer pool ensures pinned pages aren't evicted
            //
            // Example Execution (3-level tree, A→B→C traversal):
            //
            // Time | Held Locks         | Action
            // -----|-------------------|----------------------------------
            // T1   | A (root)          | Acquired lock on root page A
            // T2   | A, B              | Acquired lock on child B (2 locks)
            // T3   | B                 | Released lock on A (hand-over)
            // T4   | B, C              | Acquired lock on child C (2 locks)
            // T5   | C (leaf)          | Released lock on B (hand-over)
            // T6   | C (leaf)          | Return to caller with lock held
            //
            // Acquire lock on current page using lock coupling
            if (lock_mgr != nullptr)
            {
                LockTag page_tag{};
                page_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                page_tag.object_uuid = index_info_.idx_uuid;
                page_tag.page_num = current_page_num;
                page_tag.offset_num = 0;
                page_tag.padding = 0;

                LockMode lock_mode = write_lock ? LockMode::LOCK_EXCLUSIVE : LockMode::LOCK_SHARE;

                // Step 1: Acquire lock on CURRENT page (child)
                // At this point we hold locks on: previous_page (parent), current_page (child)
                status = lock_mgr->acquireLock(proc_id, page_tag, lock_mode, true, 0, ctx);
                if (status != Status::OK)
                {
                    unpinIndexPage(current_page_num, false, ctx);

                    // CRITICAL: If we fail to acquire child lock, keep parent lock held
                    // Release previous lock if held (cleanup on error)
                    if (previous_page_num != 0)
                    {
                        LockTag prev_tag{};
                        prev_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                        prev_tag.object_uuid = index_info_.idx_uuid;
                        prev_tag.page_num = previous_page_num;
                        lock_mgr->releaseLock(proc_id, prev_tag, lock_mode, ctx);
                    }

                    SET_ERROR_CONTEXT(ctx, status,
                                      "Failed to acquire page lock during B-tree traversal");
                    return status;
                }

                // Step 2: Release lock on PREVIOUS page (parent) - the "coupling" step
                // This is the core of lock coupling: hand-over-hand motion
                // We now hold lock ONLY on current_page (child)
                if (previous_page_num != 0)
                {
                    LockTag prev_tag{};
                    prev_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    prev_tag.object_uuid = index_info_.idx_uuid;
                    prev_tag.page_num = previous_page_num;
                    lock_mgr->releaseLock(proc_id, prev_tag, lock_mode, ctx);
                }
                // At this point: previous lock released, current lock held
                // Other threads can now access the parent page we just released
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
            const BTreePageShape shape = normalizeBTreePageShape(page);
            if (shape.kind == BTreePageShapeKind::Invalid)
            {
                unpinIndexPage(current_page_num, false, ctx);

                if (lock_mgr != nullptr)
                {
                    LockTag page_tag{};
                    page_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    page_tag.object_uuid = index_info_.idx_uuid;
                    page_tag.page_num = current_page_num;
                    lock_mgr->releaseLock(
                        proc_id, page_tag,
                        write_lock ? LockMode::LOCK_EXCLUSIVE : LockMode::LOCK_SHARE, ctx);
                }

                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  shape.error_message != nullptr
                                      ? shape.error_message
                                      : "Invalid B-tree page shape during traversal");
                return Status::PAGE_CORRUPT;
            }

            if (shape.kind == BTreePageShapeKind::Leaf)
            {
                // Found leaf page - keep lock held and return
                *page_num_out = current_page_num;
                unpinIndexPage(current_page_num, shape.normalized, ctx);
                // Note: Lock is kept held on leaf page for caller
                return Status::OK;
            }

            const auto *page_data = reinterpret_cast<const uint8_t *>(page);
            uint64_t next_page_num = 0;
            const auto *offsets =
                reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));

            bool restart_contract_ready = true;
            for (uint16_t i = 0; i < page->btr_count;
                 i = static_cast<uint16_t>(i + kInternalSearchRestartInterval))
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);
                if (node->btn_prefix_len != 0)
                {
                    restart_contract_ready = false;
                    break;
                }
            }

            if (restart_contract_ready)
            {
                const uint16_t anchor_count = static_cast<uint16_t>(
                    (page->btr_count + kInternalSearchRestartInterval - 1) /
                    kInternalSearchRestartInterval);
                auto anchor_node_index = [](uint16_t anchor_slot) -> uint16_t
                {
                    return static_cast<uint16_t>(anchor_slot * kInternalSearchRestartInterval);
                };
                auto compare_anchor = [&](uint16_t anchor_slot) -> int
                {
                    const uint16_t node_index = anchor_node_index(anchor_slot);
                    const auto *node =
                        reinterpret_cast<const SBBTreeNode *>(page_data + offsets[node_index]);
                    const uint8_t *node_key_data =
                        reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
                    return compare_keys(key, node_key_data, node->btn_key_len);
                };

                uint16_t low_anchor = 0;
                uint16_t high_anchor = anchor_count;
                while (low_anchor < high_anchor)
                {
                    const uint16_t mid_anchor = static_cast<uint16_t>(
                        low_anchor + ((high_anchor - low_anchor) / 2));
                    if (compare_anchor(mid_anchor) < 0)
                    {
                        high_anchor = mid_anchor;
                    }
                    else
                    {
                        low_anchor = static_cast<uint16_t>(mid_anchor + 1);
                    }
                }

                const uint16_t first_greater_anchor = low_anchor;
                const uint16_t start_anchor =
                    (first_greater_anchor == 0) ? 0
                                                : static_cast<uint16_t>(first_greater_anchor - 1);
                const uint16_t start_index = anchor_node_index(start_anchor);
                const uint16_t next_anchor_slot = std::min<uint16_t>(
                    anchor_count, static_cast<uint16_t>(start_anchor + 2));
                const uint16_t end_index =
                    (next_anchor_slot < anchor_count)
                        ? anchor_node_index(next_anchor_slot)
                        : page->btr_count;

                const auto *start_node =
                    reinterpret_cast<const SBBTreeNode *>(page_data + offsets[start_index]);
                const uint8_t *start_key_data =
                    reinterpret_cast<const uint8_t *>(start_node) + sizeof(SBBTreeNode);
                std::vector<uint8_t> prev_key(start_key_data,
                                              start_key_data + start_node->btn_key_len);

                for (uint16_t i = start_index; i < end_index; ++i)
                {
                    const auto *node =
                        reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);
                    const uint8_t *node_key_data =
                        reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
                    std::vector<uint8_t> full_key =
                        (i == start_index)
                            ? prev_key
                            : decompress_key(prev_key, node_key_data, node->btn_key_len,
                                             node->btn_prefix_len);
                    if (compare_keys(key, full_key.data(), full_key.size()) < 0)
                    {
                        next_page_num = node->btn_child_page;
                        break;
                    }
                    prev_key = full_key;
                }
            }

            if (next_page_num == 0)
            {
                std::vector<uint8_t> prev_key;
                for (uint16_t i = 0; i < page->btr_count; ++i)
                {
                    const auto *node =
                        reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);
                    const uint8_t *node_key_data =
                        reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
                    std::vector<uint8_t> full_key = decompress_key(
                        prev_key, node_key_data, node->btn_key_len, node->btn_prefix_len);
                    if (compare_keys(key, full_key.data(), full_key.size()) < 0)
                    {
                        next_page_num = node->btn_child_page;
                        break;
                    }
                    prev_key = full_key;
                }
            }

            // If we didn't find a suitable child (key >= all keys), use the rightmost child
            // The rightmost child pointer is stored in the page header (btr_rightmost_child)
            if (next_page_num == 0)
            {
                // Use the rightmost child pointer from page header
                next_page_num = page->btr_rightmost_child;

                if (next_page_num == 0)
                {
                    // Missing rightmost child pointer - this is a corruption issue
                    unpinIndexPage(current_page_num, false, ctx);

                    // Release lock on current page before returning error
                    if (lock_mgr != nullptr)
                    {
                        LockTag page_tag{};
                        page_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                        page_tag.object_uuid = index_info_.idx_uuid;
                        page_tag.page_num = current_page_num;
                        lock_mgr->releaseLock(
                            proc_id, page_tag,
                            write_lock ? LockMode::LOCK_EXCLUSIVE : LockMode::LOCK_SHARE, ctx);
                    }

                    const std::string detail =
                        "Internal node missing rightmost child pointer"
                        " (root_page=" + std::to_string(index_info_.idx_root_page) +
                        ", current_page=" + std::to_string(current_page_num) +
                        ", level=" + std::to_string(page->btr_level) +
                        ", key_count=" + std::to_string(page->btr_count) +
                        ", flags=" + std::to_string(page->btr_flags) +
                        ", page_type=" + std::to_string(page->btr_header.page_type) +
                        ", tablespace_id=" +
                        std::to_string(index_info_.idx_tablespace_id) + ")";
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, detail.c_str());
                    return Status::PAGE_CORRUPT;
                }
            }

            unpinIndexPage(current_page_num, shape.normalized, ctx);

            // Track previous page for lock coupling
            previous_page_num = current_page_num;
            current_page_num = next_page_num;
        }
    }

    // PHASE 1 TASK 1.1.1: Added Snapshot parameter (not yet used - Phase 1 Task 1.2 will implement filtering)
    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    auto BTree::search(const std::vector<uint8_t> &key,
                       uint64_t current_xid,
                       std::vector<TID> *tids_out,
                       ErrorContext *ctx) -> Status
    {
        if (bloom_filter_ && !bloom_filter_->test(key.data(), key.size(), ctx))
        {
            return Status::NOT_FOUND;
        }

        uint64_t leaf_page_num;
        Status status = find_leaf_page(key, &leaf_page_num, false, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Get proc_id from ConnectionContext (Phase 2 complete)
        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        const uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;
        LockManager *lock_mgr = db_->lock_manager();

        BufferPool *bp = db_->buffer_pool();
        void *page_data_ptr;
        status = pinIndexPage(leaf_page_num, &page_data_ptr, ctx);
        if (status != Status::OK)
        {
            // Release lock acquired by find_leaf_page
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_SHARE, ctx);
            }
            return status;
        }

        const auto *page = reinterpret_cast<const SBBTreePage *>(page_data_ptr);

        // Firebird MGA: Pass transaction ID to searchPage for TIP-based visibility filtering
        bool found = searchPage(page, key, current_xid, tids_out);

        unpinIndexPage(leaf_page_num, false, ctx);

        // Release lock after search
        if (lock_mgr != nullptr)
        {
            LockTag leaf_tag{};
            leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
            leaf_tag.object_uuid = index_info_.idx_uuid;
            leaf_tag.page_num = leaf_page_num;
            lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_SHARE, ctx);
        }

        // Firebird MGA: Index-level TIP-based visibility filtering active
        // Entries with btn_xmin/btn_xmax are filtered at index level using isVersionVisible()
        // This provides 10-100x speedup for queries with many deleted tuples by avoiding
        // unnecessary heap page accesses.

        if (found)
        {
            return Status::OK;
        }

        return Status::NOT_FOUND;
    }

    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    // Task 17 MGA Phase 3.1: Added xid parameter (will be used in Phase 3.2 for markDeleted)
    auto BTree::remove(const std::vector<uint8_t> &key, const TID &tid, uint64_t xid,
                       ErrorContext *ctx)
        -> Status
    {
        // FIXED: Use xid to set btn_xmax for MGA-compliant logical deletion
        // Find the appropriate leaf page for this key
        uint64_t leaf_page_num;
        Status status = find_leaf_page(key, &leaf_page_num, true, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Get proc_id from ConnectionContext (Phase 2 complete)
        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        const uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;
        LockManager *lock_mgr = db_->lock_manager();

        BufferPool *bp = db_->buffer_pool();
        void *page_data_ptr;
        status = pinIndexPage(leaf_page_num, &page_data_ptr, ctx);
        if (status != Status::OK)
        {
            // Release lock acquired by find_leaf_page
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
            }
            return status;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
        const auto *page_data = reinterpret_cast<const uint8_t *>(page);

        // Get the node offsets array
        auto *offsets = reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(page_data_ptr) +
                                                     sizeof(SBBTreePage));

        // Search for the key and matching tuple_id
        bool found = false;
        uint16_t node_to_remove = 0;

        // Track previous key for decompression
        std::vector<uint8_t> prev_key;

        for (uint16_t i = 0; i < page->btr_count; ++i)
        {
            const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);

            // Extract the node's key
            const uint8_t *node_key_data =
                reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);

            // Decompress key if compressed
            std::vector<uint8_t> full_key = decompress_key(prev_key, node_key_data,
                                                           node->btn_key_len,
                                                           node->btn_prefix_len);

            // Compare decompressed key
            int cmp = compare_keys(key, full_key.data(), full_key.size());
            if (cmp == 0)
            {
                // Check if the tid matches
                const auto *tuple_ids_ptr =
                    reinterpret_cast<const OnDiskTID *>(node_key_data + node->btn_key_len);

                for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
                {
                    if (fromOnDiskTID(tuple_ids_ptr[j]) == tid)
                    {
                        found = true;
                        node_to_remove = i;
                        break;
                    }
                }

                if (found)
                {
                    break;
                }
            }

            // Update prev_key for next iteration
            prev_key = full_key;
        }

        if (!found)
        {
            unpinIndexPage(leaf_page_num, false, ctx);

            // Release lock on not found
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
            }

            return Status::NOT_FOUND;
        }

        // MGA-compliant logical deletion: Set btn_xmax to mark entry as deleted
        // This follows Firebird MGA principles (MGA_RULES.md Rule 6)
        // Entry remains in index but becomes invisible to transactions with xid >= xmax
        auto *node_to_mark = reinterpret_cast<SBBTreeNode *>(
            reinterpret_cast<uint8_t *>(page_data_ptr) + offsets[node_to_remove]);
        node_to_mark->btn_xmax = xid;

        // Set HAS_GARBAGE flag to indicate page needs GC compaction for physical cleanup
        page->btr_flags |= static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);

        // Mark page as dirty since we modified it
        unpinIndexPage(leaf_page_num, true, ctx);

        // Release lock after successful remove
        if (lock_mgr != nullptr)
        {
            LockTag leaf_tag{};
            leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
            leaf_tag.object_uuid = index_info_.idx_uuid;
            leaf_tag.page_num = leaf_page_num;
            lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
        }

        return Status::OK;
    }

    // Task 17 MGA Phase 3.2: Soft deletion support (mark deleted instead of physical removal)
    auto BTree::markDeleted(const std::vector<uint8_t> &key, const TID &tid, uint64_t xmax,
                           ErrorContext *ctx) -> Status
    {
        // Navigate to leaf page containing this key
        uint64_t leaf_page_num;
        Status status = find_leaf_page(key, &leaf_page_num, true, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Get proc_id from ConnectionContext
        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        const uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;
        LockManager *lock_mgr = db_->lock_manager();

        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;
        status = pinIndexPage(leaf_page_num, &page_buffer, ctx);
        if (status != Status::OK)
        {
            // Release lock acquired by find_leaf_page
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
            }
            return status;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
        uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);

        // Scan entries on page to find matching key + TID
        auto *offsets = reinterpret_cast<uint16_t *>(page_data + sizeof(SBBTreePage));
        bool found = false;

        std::vector<uint8_t> prev_key;
        for (uint16_t i = 0; i < page->btr_count; i++)
        {
            auto *node = reinterpret_cast<SBBTreeNode *>(page_data + offsets[i]);

            // Extract key (with decompression if needed)
            uint8_t *node_key_data = reinterpret_cast<uint8_t *>(node) + sizeof(SBBTreeNode);
            std::vector<uint8_t> node_key = decompress_key(prev_key, node_key_data,
                                                           node->btn_key_len,
                                                           node->btn_prefix_len);
            const bool is_deleted =
                (node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0;

            // Check if key matches
            if (!is_deleted && compare_keys(key, node_key) == 0)
            {
                // Key matches - check if TID matches
                uint8_t *tid_data = node_key_data + node->btn_key_len;
                auto *tids = reinterpret_cast<OnDiskTID *>(tid_data);

                for (uint32_t j = 0; j < node->btn_tuple_count; j++)
                {
                    TID node_tid = fromOnDiskTID(tids[j]);
                    if (node_tid == tid)
                    {
                        // Found it! Set btn_xmax to mark as deleted
                        node->btn_xmax = xmax;
                        found = true;
                        break;
                    }
                }
            }

            if (found)
            {
                break;
            }

            prev_key = std::move(node_key);
        }

        if (found)
        {
            page->btr_flags |= static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);
        }

        // Unpin page (mark dirty if modified)
        unpinIndexPage(leaf_page_num, found, ctx);

        // Release lock
        if (lock_mgr != nullptr)
        {
            LockTag leaf_tag{};
            leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
            leaf_tag.object_uuid = index_info_.idx_uuid;
            leaf_tag.page_num = leaf_page_num;
            lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
        }

        return found ? Status::OK : Status::NOT_FOUND;
    }

    auto BTree::restoreDeleted(const std::vector<uint8_t> &key,
                               const TID &tid,
                               uint64_t deleting_xid,
                               ErrorContext *ctx) -> Status
    {
        uint64_t leaf_page_num = 0;
        Status status = find_leaf_page(key, &leaf_page_num, true, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        const uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;
        LockManager *lock_mgr = db_->lock_manager();

        void *page_buffer = nullptr;
        status = pinIndexPage(leaf_page_num, &page_buffer, ctx);
        if (status != Status::OK)
        {
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
            }
            return status;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
        auto *page_data = reinterpret_cast<uint8_t *>(page_buffer);
        auto *offsets = reinterpret_cast<uint16_t *>(page_data + sizeof(SBBTreePage));

        bool restored = false;
        std::vector<uint8_t> prev_key;
        for (uint16_t i = 0; i < page->btr_count; ++i)
        {
            auto *node = reinterpret_cast<SBBTreeNode *>(page_data + offsets[i]);
            uint8_t *node_key_data = reinterpret_cast<uint8_t *>(node) + sizeof(SBBTreeNode);
            std::vector<uint8_t> node_key = decompress_key(prev_key,
                                                           node_key_data,
                                                           node->btn_key_len,
                                                           node->btn_prefix_len);
            prev_key = node_key;
            const bool is_deleted =
                (node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0;

            if (is_deleted || compare_keys(key, node_key) != 0)
            {
                continue;
            }

            uint8_t *tid_data = node_key_data + node->btn_key_len;
            auto *tids = reinterpret_cast<OnDiskTID *>(tid_data);
            for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
            {
                if (fromOnDiskTID(tids[j]) != tid)
                {
                    continue;
                }

                if (node->btn_xmax == 0 || node->btn_xmax == deleting_xid)
                {
                    node->btn_xmax = 0;
                    restored = true;
                }
                break;
            }

            if (restored)
            {
                break;
            }
        }

        unpinIndexPage(leaf_page_num, restored, ctx);

        if (lock_mgr != nullptr)
        {
            LockTag leaf_tag{};
            leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
            leaf_tag.object_uuid = index_info_.idx_uuid;
            leaf_tag.page_num = leaf_page_num;
            lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
        }

        return restored ? Status::OK : Status::NOT_FOUND;
    }

    auto BTree::purge(const std::vector<uint8_t> &key, const TID &tid, ErrorContext *ctx)
        -> Status
    {
        uint64_t leaf_page_num = 0;
        Status status = find_leaf_page(key, &leaf_page_num, true, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        const uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;
        LockManager *lock_mgr = db_->lock_manager();

        void *page_buffer = nullptr;
        status = pinIndexPage(leaf_page_num, &page_buffer, ctx);
        if (status != Status::OK)
        {
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
            }
            return status;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
        auto *page_data = reinterpret_cast<uint8_t *>(page_buffer);
        auto *offsets = reinterpret_cast<uint16_t *>(page_data + sizeof(SBBTreePage));

        bool purged = false;
        std::vector<uint8_t> prev_key;
        for (uint16_t i = 0; i < page->btr_count; ++i)
        {
            auto *node = reinterpret_cast<SBBTreeNode *>(page_data + offsets[i]);
            uint8_t *node_key_data = reinterpret_cast<uint8_t *>(node) + sizeof(SBBTreeNode);
            std::vector<uint8_t> node_key = decompress_key(prev_key,
                                                           node_key_data,
                                                           node->btn_key_len,
                                                           node->btn_prefix_len);
            prev_key = node_key;
            const bool is_deleted =
                (node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0;

            if (is_deleted || compare_keys(key, node_key) != 0)
            {
                continue;
            }

            uint8_t *tid_data = node_key_data + node->btn_key_len;
            auto *tids = reinterpret_cast<OnDiskTID *>(tid_data);
            for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
            {
                if (fromOnDiskTID(tids[j]) != tid)
                {
                    continue;
                }

                node->btn_flags |= static_cast<uint16_t>(BTreeNodeFlags::DELETED);
                page->btr_flags |= static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);
                purged = true;
                break;
            }

            if (purged)
            {
                break;
            }
        }

        unpinIndexPage(leaf_page_num, purged, ctx);

        if (lock_mgr != nullptr)
        {
            LockTag leaf_tag{};
            leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
            leaf_tag.object_uuid = index_info_.idx_uuid;
            leaf_tag.page_num = leaf_page_num;
            lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
        }

        return purged ? Status::OK : Status::NOT_FOUND;
    }

    // Firebird MGA: Check if index entry is visible using TIP-based visibility
    bool BTree::isEntryVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) const
    {
        // ===========================================================================================
        // FIREBIRD MGA VISIBILITY - TIP-based, NOT snapshot-based
        // Per MGA_RULES.md Rule 3 (lines 121-145)
        // ===========================================================================================

        // If no transaction specified, entry is always visible (used by GC compaction, etc.)
        if (current_xid == 0)
        {
            return true;
        }

        // Get transaction manager
        TransactionManager *txn_mgr = db_->transaction_manager();
        if (txn_mgr == nullptr)
        {
            return true; // No transaction tracking - always visible
        }

        // Special case: xmin = 0 means legacy entry or system operation (always visible)
        if (xmin == 0)
        {
            return true;
        }

        // A delete performed by the current transaction must hide the entry even
        // when the same transaction also created it.
        if (xmax == current_xid)
        {
            return false;
        }

        // Own changes always visible once we know we have not already deleted
        // the entry in the current transaction.
        if (xmin == current_xid)
        {
            return true;
        }

        // User-facing searches should respect the active connection runtime context only when
        // the caller-provided reader XID matches that connection's live transaction/statement.
        // Tests and maintenance paths can legitimately pass explicit XIDs on a thread that still
        // has an unrelated ConnectionContext bound, and those callers need deterministic
        // inventory truth instead of inheriting the ambient connection state.
        if (ConnectionContext *conn_ctx = ConnectionContext::getCurrent(); conn_ctx != nullptr)
        {
            const uint64_t runtime_reader_xid = conn_ctx->getStatementXID();
            const uint64_t runtime_tx_xid = conn_ctx->getCurrentXid();
            if (current_xid == runtime_reader_xid || current_xid == runtime_tx_xid)
            {
                return txn_mgr->evaluateRuntimeRecordVisibility(
                                  xmin, xmax, current_xid, conn_ctx)
                    .visible;
            }
        }

        return txn_mgr->isInventoryRecordVisible(xmin, xmax, current_xid);
    }

    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    auto BTree::split_leaf_page(uint64_t left_page_num, const std::vector<uint8_t> &new_key,
                                const TID &new_tid, uint64_t xid,
                                uint64_t *right_page_num_out,
                                ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        PageManager *pm = db_->page_manager();
        (void)bp;
        if (right_page_num_out != nullptr)
        {
            *right_page_num_out = 0;
        }

        // Allocate new right page
        GPID right_gpid = 0;
        Status status = pm->allocatePageInTablespace(index_info_.idx_tablespace_id, &right_gpid, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate new page for split");
            return status;
        }
        uint32_t right_page_num_u32 = static_cast<uint32_t>(getPageNumber(right_gpid));
        uint64_t right_page_num = right_page_num_u32;
        if (right_page_num_out != nullptr)
        {
            *right_page_num_out = right_page_num;
        }

        // Pin both pages
        void *left_page_data_ptr;
        void *right_page_data_ptr;

        status = pinIndexPage(left_page_num, &left_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            pm->freePageGlobal(right_gpid, ctx);
            return status;
        }

        status = pinIndexPage(right_page_num, &right_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            unpinIndexPage(left_page_num, false, ctx);
            pm->freePageGlobal(right_gpid, ctx);
            return status;
        }

        auto *left_page = reinterpret_cast<SBBTreePage *>(left_page_data_ptr);
        auto *right_page = reinterpret_cast<SBBTreePage *>(right_page_data_ptr);
        uint32_t page_size = left_page->btr_header.page_size;
        const BTreePageShape left_shape = normalizeBTreePageShape(left_page);
        if (left_shape.kind == BTreePageShapeKind::Invalid)
        {
            unpinIndexPage(left_page_num, false, ctx);
            unpinIndexPage(right_page_num, false, ctx);
            pm->freePageGlobal(right_gpid, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              left_shape.error_message != nullptr
                                  ? left_shape.error_message
                                  : "Invalid left B-tree page shape during leaf split");
            return Status::PAGE_CORRUPT;
        }
        if (left_shape.kind != BTreePageShapeKind::Leaf)
        {
            unpinIndexPage(left_page_num, left_shape.normalized, ctx);
            unpinIndexPage(right_page_num, false, ctx);
            pm->freePageGlobal(right_gpid, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "split_leaf_page called on non-leaf page");
            return Status::PAGE_CORRUPT;
        }
        bool left_page_dirty = left_shape.normalized;

        try
        {
            // Initialize right page as leaf (never ROOT/LEFTMOST/RIGHTMOST at creation)
            uint16_t right_flags = static_cast<uint16_t>(BTreeFlags::LEAF);
            status = initializeBTreeRuntimePage(
                db_, reinterpret_cast<uint8_t *>(right_page_data_ptr), page_size,
                right_page_num, left_page->btr_index_uuid, left_page->btr_table_uuid,
                left_page->btr_level, right_flags);
            if (status != Status::OK)
            {
                unpinIndexPage(left_page_num, left_page_dirty, ctx);
                unpinIndexPage(right_page_num, false, ctx);
                pm->freePageGlobal(right_gpid, ctx);
                return status;
            }
            BTreePage right_btree_page(reinterpret_cast<uint8_t *>(right_page_data_ptr),
                                       page_size);

            // Calculate split point
            BTreePage left_btree_page(reinterpret_cast<uint8_t *>(left_page_data_ptr), page_size);
            uint16_t split_point = left_btree_page.find_split_point();

            // Get left page node offsets
            auto *left_offsets = reinterpret_cast<uint16_t *>(
                reinterpret_cast<uint8_t *>(left_page_data_ptr) + sizeof(SBBTreePage));

            // Move second half of nodes to right page (with decompression)
            for (uint16_t i = split_point; i < left_page->btr_count; ++i)
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(
                    reinterpret_cast<uint8_t *>(left_page_data_ptr) + left_offsets[i]);
                std::vector<uint8_t> node_key;
                std::vector<TID> tuple_ids;
                status = BTreePage::get_node(reinterpret_cast<uint8_t *>(left_page_data_ptr),
                                             page_size, i, node_key, tuple_ids);
                if (status != Status::OK)
                {
                    unpinIndexPage(left_page_num, left_page_dirty, ctx);
                    unpinIndexPage(right_page_num, false, ctx);
                    pm->freePageGlobal(right_gpid, ctx);
                    return status;
                }

                // Add each tuple to right page
                for (const auto &tid : tuple_ids)
                {
                    Tuple tuple;
                    tuple.tid = tid;
                    tuple.data = nullptr;
                    tuple.data_size = 0;

                    // Task 17 MGA Phase 3.1: Preserve original xmin during page split
                    status = right_btree_page.add_node(node_key, tuple, node->btn_xmin, ctx);
                    if (status != Status::OK)
                    {
                        unpinIndexPage(left_page_num, left_page_dirty, ctx);
                        unpinIndexPage(right_page_num, false, ctx);
                        pm->freePageGlobal(right_gpid, ctx);
                        return status;
                    }
                }
            }

            // Collect remaining left-page entries for rebuild/compaction
            std::vector<LeafEntryData> left_entries;
            left_entries.reserve(split_point);
            std::vector<uint8_t> prev_key;
            for (uint16_t i = 0; i < split_point; ++i)
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(
                    reinterpret_cast<uint8_t *>(left_page_data_ptr) + left_offsets[i]);
                const uint8_t *node_key_data =
                    reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
                std::vector<uint8_t> full_key = decompress_key(prev_key, node_key_data,
                                                               node->btn_key_len,
                                                               node->btn_prefix_len);
                prev_key = full_key;

                LeafEntryData entry;
                entry.key = std::move(full_key);
                entry.flags = node->btn_flags;
                entry.xmin = node->btn_xmin;
                entry.xmax = node->btn_xmax;

                const auto *tuple_ids_ptr =
                    reinterpret_cast<const OnDiskTID *>(node_key_data + node->btn_key_len);
                entry.tids.reserve(node->btn_tuple_count);
                for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
                {
                    entry.tids.push_back(tuple_ids_ptr[j]);
                }
                left_entries.push_back(std::move(entry));
            }

            right_page->btr_parent_page = left_page->btr_parent_page;
            status = updateSplitSiblingLinks(left_page_num, right_page_num, left_page, right_page,
                                             ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(left_page_num, left_page_dirty, ctx);
                unpinIndexPage(right_page_num, false, ctx);
                pm->freePageGlobal(right_gpid, ctx);
                return status;
            }

            // Rebuild left page to compact and apply compression
            status = rebuild_leaf_page(left_page, page_size, left_entries, ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(left_page_num, left_page_dirty, ctx);
                unpinIndexPage(right_page_num, false, ctx);
                pm->freePageGlobal(right_gpid, ctx);
                return status;
            }
            left_page_dirty = true;

            Tuple pending_tuple;
            pending_tuple.tid = new_tid;
            pending_tuple.data = nullptr;
            pending_tuple.data_size = 0;

            std::vector<uint8_t> right_min_key;
            std::vector<TID> tmp_tids;
            bool insert_into_right = true;
            if (right_page->btr_count > 0)
            {
                status = BTreePage::get_node(reinterpret_cast<uint8_t *>(right_page_data_ptr),
                                             page_size, 0, right_min_key, tmp_tids);
                if (status != Status::OK)
                {
                    unpinIndexPage(left_page_num, left_page_dirty, ctx);
                    unpinIndexPage(right_page_num, false, ctx);
                    pm->freePageGlobal(right_gpid, ctx);
                    return status;
                }
                insert_into_right = compare_keys(new_key, right_min_key) >= 0;
            }

            BTreePage refreshed_left_btree_page(
                reinterpret_cast<uint8_t *>(left_page_data_ptr), page_size);
            Status pending_insert_status =
                insert_into_right
                    ? right_btree_page.add_node(new_key, pending_tuple, xid, ctx)
                    : refreshed_left_btree_page.add_node(new_key, pending_tuple, xid, ctx);
            if (pending_insert_status != Status::OK)
            {
                unpinIndexPage(left_page_num, left_page_dirty, ctx);
                unpinIndexPage(right_page_num, false, ctx);
                pm->freePageGlobal(right_gpid, ctx);
                return pending_insert_status;
            }

            if ((right_page->btr_flags & static_cast<uint16_t>(BTreeFlags::RIGHTMOST)) != 0 &&
                insert_into_right)
            {
                updateRightmostLeafHint(right_page_num, new_key);
            }
            else if ((left_page->btr_flags & static_cast<uint16_t>(BTreeFlags::RIGHTMOST)) != 0 &&
                     !insert_into_right)
            {
                updateRightmostLeafHint(left_page_num, new_key);
            }

            // Get separator key (minimal separator between left max and right min)
            right_min_key.clear();
            tmp_tids.clear();
            status = BTreePage::get_node(reinterpret_cast<uint8_t *>(right_page_data_ptr),
                                         page_size, 0, right_min_key, tmp_tids);
            if (status != Status::OK)
            {
                unpinIndexPage(left_page_num, left_page_dirty, ctx);
                unpinIndexPage(right_page_num, false, ctx);
                pm->freePageGlobal(right_gpid, ctx);
                return status;
            }

            std::vector<uint8_t> left_max_key;
            tmp_tids.clear();
            if (left_page->btr_count > 0)
            {
                status = BTreePage::get_node(reinterpret_cast<uint8_t *>(left_page_data_ptr),
                                             page_size, static_cast<uint16_t>(left_page->btr_count - 1),
                                             left_max_key, tmp_tids);
                if (status != Status::OK)
                {
                    unpinIndexPage(left_page_num, left_page_dirty, ctx);
                    unpinIndexPage(right_page_num, false, ctx);
                    pm->freePageGlobal(right_gpid, ctx);
                    return status;
                }
            }

            uint16_t separator_suffix_trunc = 0;
            std::vector<uint8_t> separator_key =
                minimal_separator_key(left_max_key, right_min_key, &separator_suffix_trunc);

            // Unpin both pages (mark as dirty)
            unpinIndexPage(left_page_num, true, ctx);
            unpinIndexPage(right_page_num, true, ctx);

            // Insert separator key into parent
            return insert_into_parent(left_page_num, separator_key, right_page_num,
                                      separator_suffix_trunc, ctx);
        }
        catch (const std::exception &e)
        {
            unpinIndexPage(left_page_num, left_page_dirty, ctx);
            unpinIndexPage(right_page_num, false, ctx);
            pm->freePageGlobal(right_gpid, ctx);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, e.what());
            return Status::INVALID_ARGUMENT;
        }
    }

    auto BTree::split_internal_page(uint64_t parent_page_num,
                                    uint64_t left_child_page_num,
                                    const std::vector<uint8_t> &separator_key,
                                    uint64_t right_page_num,
                                    uint16_t separator_suffix_trunc,
                                    ErrorContext *ctx) -> Status
    {
        PageManager *pm = db_->page_manager();

        // The caller already holds the exclusive lock on parent_page_num.
        // Re-acquiring that same lock here can fail as a self-conflict once the
        // root/internal page fills, leaving the leaf split published but the
        // parent-child publication incomplete.

        // Allocate new right internal page
        GPID new_right_gpid = 0;
        Status status = pm->allocatePageInTablespace(index_info_.idx_tablespace_id, &new_right_gpid, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate new internal page for split");
            return status;
        }
        uint32_t new_right_page_num_u32 = static_cast<uint32_t>(getPageNumber(new_right_gpid));
        uint64_t new_right_page_num = new_right_page_num_u32;

        // Pin both pages
        void *parent_page_data_ptr = nullptr;
        void *new_right_page_data_ptr = nullptr;

        status = pinIndexPage(parent_page_num, &parent_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            pm->freePageGlobal(new_right_gpid, ctx);
            return status;
        }

        status = pinIndexPage(new_right_page_num, &new_right_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            unpinIndexPage(parent_page_num, false, ctx);
            pm->freePageGlobal(new_right_gpid, ctx);
            return status;
        }

        auto *parent_page = reinterpret_cast<SBBTreePage *>(parent_page_data_ptr);
        auto *new_right_page = reinterpret_cast<SBBTreePage *>(new_right_page_data_ptr);
        uint32_t page_size = parent_page->btr_header.page_size;

        try
        {
            // Initialize new right page as internal (not leaf, not root)
            uint16_t internal_flags =
                parent_page->btr_flags & ~static_cast<uint16_t>(BTreeFlags::LEAF);
            internal_flags &= ~static_cast<uint16_t>(BTreeFlags::ROOT);
            internal_flags &= ~static_cast<uint16_t>(BTreeFlags::LEFTMOST);
            internal_flags &= ~static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
            status = initializeBTreeRuntimePage(
                db_, reinterpret_cast<uint8_t *>(new_right_page_data_ptr), page_size,
                new_right_page_num, parent_page->btr_index_uuid,
                parent_page->btr_table_uuid, parent_page->btr_level, internal_flags);
            if (status != Status::OK)
            {
                unpinIndexPage(parent_page_num, false, ctx);
                unpinIndexPage(new_right_page_num, false, ctx);
                pm->freePageGlobal(new_right_gpid, ctx);
                return status;
            }
            BTreePage new_right_btree_page(reinterpret_cast<uint8_t *>(new_right_page_data_ptr),
                                           page_size);

            // Extract keys and children from parent
            std::vector<InternalKeyEntry> keys;
            std::vector<uint64_t> children;
            keys.reserve(parent_page->btr_count + 1);
            children.reserve(parent_page->btr_count + 2);

            auto *offsets = reinterpret_cast<const uint16_t *>(
                reinterpret_cast<const uint8_t *>(parent_page_data_ptr) + sizeof(SBBTreePage));
            std::vector<uint8_t> prev_key;

            for (uint16_t i = 0; i < parent_page->btr_count; ++i)
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(
                    reinterpret_cast<const uint8_t *>(parent_page_data_ptr) + offsets[i]);
                const uint8_t *node_key_data =
                    reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
                std::vector<uint8_t> full_key = decompress_key(prev_key, node_key_data,
                                                               node->btn_key_len,
                                                               node->btn_prefix_len);
                prev_key = full_key;

                InternalKeyEntry entry;
                entry.key = std::move(full_key);
                entry.suffix_trunc = node->btn_suffix_trunc;
                entry.flags = node->btn_flags;
                keys.push_back(std::move(entry));
                children.push_back(node->btn_child_page);
            }
            {
                uint64_t rightmost = parent_page->btr_rightmost_child;
                if (rightmost == 0 && parent_page->btr_count > 0)
                {
                    unpinIndexPage(parent_page_num, false, ctx);
                    unpinIndexPage(new_right_page_num, false, ctx);
                    pm->freePageGlobal(new_right_gpid, ctx);
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "Parent internal page missing rightmost child before split");
                    return Status::PAGE_CORRUPT;
                }
                if (rightmost == 0 && parent_page->btr_count == 0)
                {
                    rightmost = left_child_page_num;
                }
                children.push_back(rightmost);
            }

            status = validate_internal_children(children, "internal split source extraction", ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(parent_page_num, false, ctx);
                unpinIndexPage(new_right_page_num, false, ctx);
                pm->freePageGlobal(new_right_gpid, ctx);
                return status;
            }

            // Find child index for insertion
            size_t child_index = keys.size();
            bool found_child = false;
            for (size_t i = 0; i < children.size(); ++i)
            {
                if (children[i] == left_child_page_num)
                {
                    child_index = i;
                    found_child = true;
                    break;
                }
            }
            if (!found_child)
            {
                unpinIndexPage(parent_page_num, false, ctx);
                unpinIndexPage(new_right_page_num, false, ctx);
                pm->freePageGlobal(new_right_gpid, ctx);
                SET_ERROR_CONTEXT(ctx, Status::INDEX_CORRUPTED,
                                  "Parent child pointer not found during internal split");
                return Status::INDEX_CORRUPTED;
            }

            // Insert separator key and new right child
            InternalKeyEntry new_entry;
            new_entry.key = separator_key;
            new_entry.suffix_trunc = separator_suffix_trunc;
            new_entry.flags = 0;
            keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(child_index), new_entry);
            children.insert(children.begin() + static_cast<std::ptrdiff_t>(child_index + 1),
                            right_page_num);

            // Split arrays
            const size_t total_keys = keys.size();
            const size_t split_index = total_keys / 2;

            InternalKeyEntry promoted = keys[split_index];

            std::vector<InternalKeyEntry> left_keys(keys.begin(), keys.begin() + split_index);
            std::vector<InternalKeyEntry> right_keys(keys.begin() + split_index + 1, keys.end());

            std::vector<uint64_t> left_children(children.begin(),
                                                children.begin() + split_index + 1);
            std::vector<uint64_t> right_children(children.begin() + split_index + 1,
                                                 children.end());

            status = validate_internal_children(left_children, "internal split left rebuild", ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(parent_page_num, false, ctx);
                unpinIndexPage(new_right_page_num, false, ctx);
                pm->freePageGlobal(new_right_gpid, ctx);
                return status;
            }
            status = validate_internal_children(right_children, "internal split right rebuild", ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(parent_page_num, false, ctx);
                unpinIndexPage(new_right_page_num, false, ctx);
                pm->freePageGlobal(new_right_gpid, ctx);
                return status;
            }

            // Rebuild left (parent) and right pages
            status = rebuild_internal_page(parent_page, page_size, left_keys, left_children, ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(parent_page_num, false, ctx);
                unpinIndexPage(new_right_page_num, false, ctx);
                pm->freePageGlobal(new_right_gpid, ctx);
                return status;
            }
            status = verify_internal_child_layout(parent_page, page_size, left_children,
                                                  "internal split left rebuild", ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(parent_page_num, false, ctx);
                unpinIndexPage(new_right_page_num, false, ctx);
                pm->freePageGlobal(new_right_gpid, ctx);
                return status;
            }

            status = rebuild_internal_page(new_right_page, page_size, right_keys, right_children, ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(parent_page_num, false, ctx);
                unpinIndexPage(new_right_page_num, false, ctx);
                pm->freePageGlobal(new_right_gpid, ctx);
                return status;
            }
            status = verify_internal_child_layout(new_right_page, page_size, right_children,
                                                  "internal split right rebuild", ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(parent_page_num, false, ctx);
                unpinIndexPage(new_right_page_num, false, ctx);
                pm->freePageGlobal(new_right_gpid, ctx);
                return status;
            }

            // Update parent pointers for children moved to right page
            for (uint64_t child_page : right_children)
            {
                void *child_page_data_ptr = nullptr;
                if (pinIndexPage(child_page, &child_page_data_ptr, ctx) == Status::OK)
                {
                    auto *child_page_ptr = reinterpret_cast<SBBTreePage *>(child_page_data_ptr);
                    child_page_ptr->btr_parent_page = new_right_page_num;
                    unpinIndexPage(child_page, true, ctx);
                }
            }

            new_right_page->btr_parent_page = parent_page->btr_parent_page;
            status = updateSplitSiblingLinks(parent_page_num, new_right_page_num, parent_page,
                                             new_right_page, ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(parent_page_num, false, ctx);
                unpinIndexPage(new_right_page_num, false, ctx);
                pm->freePageGlobal(new_right_gpid, ctx);
                return status;
            }

            // Unpin both pages (mark dirty)
            unpinIndexPage(parent_page_num, true, ctx);
            unpinIndexPage(new_right_page_num, true, ctx);

            // Insert promoted key into parent
            status = insert_into_parent(parent_page_num, promoted.key, new_right_page_num,
                                        promoted.suffix_trunc, ctx);
            return status;
        }
        catch (const std::exception &e)
        {
            unpinIndexPage(parent_page_num, false, ctx);
            unpinIndexPage(new_right_page_num, false, ctx);
            pm->freePageGlobal(new_right_gpid, ctx);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, e.what());
            return Status::INVALID_ARGUMENT;
        }
    }

    auto BTree::insert_into_parent(uint64_t left_page_num,
                                   const std::vector<uint8_t> &separator_key,
                                   uint64_t right_page_num,
                                   uint16_t separator_suffix_trunc,
                                   ErrorContext *ctx) -> Status
    {
        // Pin left page to get parent info
        void *left_page_data_ptr;
        Status status = pinIndexPage(left_page_num, &left_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *left_page = reinterpret_cast<SBBTreePage *>(left_page_data_ptr);
        uint64_t parent_page_num = left_page->btr_parent_page;
        bool left_is_root = (left_page->btr_flags & static_cast<uint16_t>(BTreeFlags::ROOT)) != 0;

        unpinIndexPage(left_page_num, false, ctx);

        // If no parent (was root), create new root
        if (parent_page_num == 0 || left_is_root)
        {
            return create_new_root(left_page_num, separator_key, right_page_num,
                                   separator_suffix_trunc, ctx);
        }

        Status lock_status = acquireIndexPageLock(db_, index_info_.idx_uuid, parent_page_num,
                                                  LockMode::LOCK_EXCLUSIVE, false, ctx);
        if (lock_status != Status::OK)
        {
            return lock_status;
        }

        // Pin parent page
        void *parent_page_data_ptr;
        status = pinIndexPage(parent_page_num, &parent_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            releaseIndexPageLock(db_, index_info_.idx_uuid, parent_page_num,
                                 LockMode::LOCK_EXCLUSIVE, ctx);
            return status;
        }

        auto *parent_page = reinterpret_cast<SBBTreePage *>(parent_page_data_ptr);
        uint32_t page_size = parent_page->btr_header.page_size;

        // Extract keys and children from parent
        std::vector<InternalKeyEntry> keys;
        std::vector<uint64_t> children;
        keys.reserve(parent_page->btr_count + 1);
        children.reserve(parent_page->btr_count + 2);

        auto *parent_offsets = reinterpret_cast<uint16_t *>(
            reinterpret_cast<uint8_t *>(parent_page_data_ptr) + sizeof(SBBTreePage));

        std::vector<uint8_t> prev_key;
        for (uint16_t i = 0; i < parent_page->btr_count; ++i)
        {
            const auto *node = reinterpret_cast<const SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(parent_page_data_ptr) + parent_offsets[i]);
            const uint8_t *existing_key_data =
                reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);

            std::vector<uint8_t> full_existing_key = decompress_key(prev_key, existing_key_data,
                                                                     node->btn_key_len,
                                                                     node->btn_prefix_len);
            prev_key = full_existing_key;

            InternalKeyEntry entry;
            entry.key = std::move(full_existing_key);
            entry.suffix_trunc = node->btn_suffix_trunc;
            entry.flags = node->btn_flags;
            keys.push_back(std::move(entry));
            children.push_back(node->btn_child_page);
        }
        {
            uint64_t rightmost = parent_page->btr_rightmost_child;
            if (rightmost == 0 && parent_page->btr_count > 0)
            {
                unpinIndexPage(parent_page_num, false, ctx);
                releaseIndexPageLock(db_, index_info_.idx_uuid, parent_page_num,
                                     LockMode::LOCK_EXCLUSIVE, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Parent internal page missing rightmost child before insert");
                return Status::PAGE_CORRUPT;
            }
            if (rightmost == 0 && parent_page->btr_count == 0)
            {
                rightmost = left_page_num;
            }
            children.push_back(rightmost);
        }

        status = validate_internal_children(children, "parent insert source extraction", ctx);
        if (status != Status::OK)
        {
            unpinIndexPage(parent_page_num, false, ctx);
            releaseIndexPageLock(db_, index_info_.idx_uuid, parent_page_num,
                                 LockMode::LOCK_EXCLUSIVE, ctx);
            return status;
        }

        // Find child index for insertion
        size_t child_index = keys.size();
        bool found_child = false;
        for (size_t i = 0; i < children.size(); ++i)
        {
            if (children[i] == left_page_num)
            {
                child_index = i;
                found_child = true;
                break;
            }
        }
        if (!found_child)
        {
            unpinIndexPage(parent_page_num, false, ctx);
            releaseIndexPageLock(db_, index_info_.idx_uuid, parent_page_num,
                                 LockMode::LOCK_EXCLUSIVE, ctx);
            SET_ERROR_CONTEXT(ctx, Status::INDEX_CORRUPTED,
                              "Parent child pointer not found during insert");
            return Status::INDEX_CORRUPTED;
        }

        // Insert separator key and right child pointer
        InternalKeyEntry new_entry;
        new_entry.key = separator_key;
        new_entry.suffix_trunc = separator_suffix_trunc;
        new_entry.flags = 0;
        keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(child_index), new_entry);
        children.insert(children.begin() + static_cast<std::ptrdiff_t>(child_index + 1),
                        right_page_num);

        // Try to rebuild parent page with new entry
        status = rebuild_internal_page(parent_page, page_size, keys, children, ctx);
        if (status == Status::PAGE_FULL)
        {
            unpinIndexPage(parent_page_num, false, ctx);
            status = split_internal_page(parent_page_num, left_page_num, separator_key,
                                         right_page_num, separator_suffix_trunc, ctx);
            releaseIndexPageLock(db_, index_info_.idx_uuid, parent_page_num,
                                 LockMode::LOCK_EXCLUSIVE, ctx);
            return status;
        }
        if (status != Status::OK)
        {
            unpinIndexPage(parent_page_num, false, ctx);
            releaseIndexPageLock(db_, index_info_.idx_uuid, parent_page_num,
                                 LockMode::LOCK_EXCLUSIVE, ctx);
            return status;
        }
        status = verify_internal_child_layout(parent_page, page_size, children,
                                              "insert_into_parent rebuild", ctx);
        if (status != Status::OK)
        {
            unpinIndexPage(parent_page_num, false, ctx);
            releaseIndexPageLock(db_, index_info_.idx_uuid, parent_page_num,
                                 LockMode::LOCK_EXCLUSIVE, ctx);
            return status;
        }

        // Ensure right child's parent pointer is set
        void *right_page_data_ptr = nullptr;
        if (pinIndexPage(right_page_num, &right_page_data_ptr, ctx) == Status::OK)
        {
            auto *right_page = reinterpret_cast<SBBTreePage *>(right_page_data_ptr);
            right_page->btr_parent_page = parent_page_num;
            unpinIndexPage(right_page_num, true, ctx);
        }

        // Unpin parent (mark as dirty)
        unpinIndexPage(parent_page_num, true, ctx);
        releaseIndexPageLock(db_, index_info_.idx_uuid, parent_page_num,
                             LockMode::LOCK_EXCLUSIVE, ctx);

        return Status::OK;
    }

    auto BTree::create_new_root(uint64_t left_page_num, const std::vector<uint8_t> &separator_key,
                                uint64_t right_page_num, uint16_t separator_suffix_trunc,
                                ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        PageManager *pm = db_->page_manager();

        // Allocate new root page
        GPID new_root_gpid = 0;
        Status status = pm->allocatePageInTablespace(index_info_.idx_tablespace_id, &new_root_gpid, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate new root page");
            return status;
        }
        uint32_t new_root_page_num_u32 = static_cast<uint32_t>(getPageNumber(new_root_gpid));
        uint64_t new_root_page_num = new_root_page_num_u32;

        // Pin all three pages
        void *left_page_data_ptr;
        void *right_page_data_ptr;
        void *new_root_page_data_ptr;

        status = pinIndexPage(left_page_num, &left_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            pm->freePageGlobal(new_root_gpid, ctx);
            return status;
        }

        status = pinIndexPage(right_page_num, &right_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            unpinIndexPage(left_page_num, false, ctx);
            pm->freePageGlobal(new_root_gpid, ctx);
            return status;
        }

        status = pinIndexPage(new_root_page_num, &new_root_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            unpinIndexPage(left_page_num, false, ctx);
            unpinIndexPage(right_page_num, false, ctx);
            pm->freePageGlobal(new_root_gpid, ctx);
            return status;
        }

        auto *left_page = reinterpret_cast<SBBTreePage *>(left_page_data_ptr);
        auto *right_page = reinterpret_cast<SBBTreePage *>(right_page_data_ptr);
        auto *new_root_page = reinterpret_cast<SBBTreePage *>(new_root_page_data_ptr);
        uint32_t page_size = left_page->btr_header.page_size;

        try
        {
            // Initialize new root as internal page at level+1
            uint16_t root_flags = static_cast<uint16_t>(BTreeFlags::ROOT) |
                                  static_cast<uint16_t>(BTreeFlags::LEFTMOST) |
                                  static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
            status = initializeBTreeRuntimePage(
                db_, reinterpret_cast<uint8_t *>(new_root_page_data_ptr), page_size,
                new_root_page_num, left_page->btr_index_uuid, left_page->btr_table_uuid,
                static_cast<uint16_t>(left_page->btr_level + 1), root_flags);
            if (status != Status::OK)
            {
                unpinIndexPage(left_page_num, false, ctx);
                unpinIndexPage(right_page_num, false, ctx);
                unpinIndexPage(new_root_page_num, false, ctx);
                pm->freePageGlobal(new_root_gpid, ctx);
                return status;
            }
            BTreePage new_root_btree_page(reinterpret_cast<uint8_t *>(new_root_page_data_ptr),
                                          page_size);

            // Add single entry: left_child (implicitly first), separator_key -> right_child
            uint32_t node_size = sizeof(SBBTreeNode) + separator_key.size() + sizeof(uint64_t);

            // For the first (leftmost) child, we typically store a dummy entry or special handling
            // In this implementation, we'll store the separator key with right_child pointer
            // The leftmost child is implicitly handled through tree traversal

            // Allocate space from end of page
            new_root_page->btr_high_water -= node_size;
            auto *new_node = reinterpret_cast<SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(new_root_page_data_ptr) +
                new_root_page->btr_high_water);

            // Populate node
            new_node->btn_flags = 0;
            new_node->btn_flags |= static_cast<uint16_t>(BTreeNodeFlags::FIRST_ON_PAGE);
            new_node->btn_flags |= static_cast<uint16_t>(BTreeNodeFlags::LAST_ON_PAGE);
            new_node->btn_prefix_len = 0;
            new_node->btn_suffix_trunc = separator_suffix_trunc;
            new_node->btn_key_len = separator_key.size();
            new_node->btn_tuple_count = 0;
            new_node->btn_child_page = left_page_num; // Left child for this key
            new_node->btn_xmin = 0;
            new_node->btn_xmax = 0;

            // Copy key
            uint8_t *key_location = reinterpret_cast<uint8_t *>(new_node) + sizeof(SBBTreeNode);
            memcpy(key_location, separator_key.data(), separator_key.size());

            // Update offset array
            auto *root_offsets = reinterpret_cast<uint16_t *>(
                reinterpret_cast<uint8_t *>(new_root_page_data_ptr) + sizeof(SBBTreePage));
            root_offsets[0] = new_root_page->btr_high_water;

            // Update root header
            new_root_page->btr_count = 1;
            new_root_page->btr_free_space -= (node_size + sizeof(uint16_t));
            new_root_page->btr_suffix_total = separator_suffix_trunc;
            if (separator_suffix_trunc > 0)
            {
                new_root_page->btr_flags |= static_cast<uint16_t>(BTreeFlags::COMPRESSED);
            }
            pageSetLower(new_root_page->btr_header,
                         sizeof(SBBTreePage) + sizeof(uint16_t));
            pageSetUpper(new_root_page->btr_header, new_root_page->btr_high_water);
            pageSetSpecial(new_root_page->btr_header, page_size);

            // CRITICAL FIX: Set the rightmost child pointer to right_page_num
            // The root now has one separator key that points left to left_page_num (stored in
            // btn_child_page) and the rightmost child is right_page_num
            new_root_page->btr_rightmost_child = right_page_num;

            // Remove ROOT flag from left page, update parent
            left_page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::ROOT);
            left_page->btr_parent_page = new_root_page_num;

            // Update right page parent
            right_page->btr_parent_page = new_root_page_num;

            // Update index info with new root page
            index_info_.idx_root_page = new_root_page_num;
            index_info_.idx_height++;

            status = persistRootGPID(new_root_page_num, ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(left_page_num, false, ctx);
                unpinIndexPage(right_page_num, false, ctx);
                unpinIndexPage(new_root_page_num, false, ctx);
                return status;
            }

            // Unpin all pages (mark as dirty)
            unpinIndexPage(left_page_num, true, ctx);
            unpinIndexPage(right_page_num, true, ctx);
            unpinIndexPage(new_root_page_num, true, ctx);

            return Status::OK;
        }
        catch (const std::exception &e)
        {
            unpinIndexPage(left_page_num, false, ctx);
            unpinIndexPage(right_page_num, false, ctx);
            unpinIndexPage(new_root_page_num, false, ctx);
            pm->freePageGlobal(new_root_gpid, ctx);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, e.what());
            return Status::INVALID_ARGUMENT;
        }
    }

    // ============================================================================
    // GC COMPACTION (ScratchBird MGA GC, not PostgreSQL VACUUM)
    // ============================================================================

    auto BTree::gcCompact(GcCompactionStats *stats_out, ErrorContext *ctx) -> Status
    {
        GcCompactionStats stats = {};

        if (!db_ || !db_->buffer_pool() || !db_->page_manager())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database state for GC compaction");
            return Status::INVALID_ARGUMENT;
        }

        BufferPool *bp = db_->buffer_pool();

        // Start from root and traverse all pages
        uint32_t root_page = static_cast<uint32_t>(index_info_.idx_root_page);

        // Pin root page
        void *root_page_data_ptr = nullptr;
        Status status = pinIndexPage(root_page, &root_page_data_ptr, ctx,
                                     BufferPool::AccessStrategy::Vacuum);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin root page for GC compaction");
            return status;
        }

        auto *root = reinterpret_cast<SBBTreePage *>(root_page_data_ptr);
        uint16_t tree_height = root->btr_level + 1;

        unpinIndexPage(root_page, false, ctx);

        // GC-compact pages level by level, bottom-up
        // This allows us to merge pages and update parent pointers correctly
        for (int16_t level = 0; level < tree_height; ++level)
        {
            // Find all pages at this level by traversing siblings
            std::vector<uint32_t> pages_at_level;

            // Start from leftmost page at this level
            uint32_t current_page = root_page;

            // Navigate down to the correct level
            for (int16_t l = tree_height - 1; l > level; --l)
            {
                void *page_data_ptr = nullptr;
                status = pinIndexPage(current_page, &page_data_ptr, ctx,
                                      BufferPool::AccessStrategy::Vacuum);
                if (status != Status::OK)
                {
                    continue;
                }

                auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);

                // For internal nodes, follow the leftmost child
                if (page->btr_count > 0)
                {
                    auto *offsets = reinterpret_cast<uint16_t *>(
                        reinterpret_cast<uint8_t *>(page_data_ptr) + sizeof(SBBTreePage));
                    auto *first_node = reinterpret_cast<SBBTreeNode *>(
                        reinterpret_cast<uint8_t *>(page_data_ptr) + offsets[0]);
                    current_page = static_cast<uint32_t>(first_node->btn_child_page);
                }
                else
                {
                    // Use rightmost child if no keys
                    current_page = static_cast<uint32_t>(page->btr_rightmost_child);
                }

                unpinIndexPage(static_cast<uint32_t>(page->btr_header.page_id), false, ctx);
            }

            // Now traverse all siblings at this level
            while (current_page != 0)
            {
                pages_at_level.push_back(current_page);

                void *page_data_ptr = nullptr;
                status = pinIndexPage(current_page, &page_data_ptr, ctx,
                                      BufferPool::AccessStrategy::Vacuum);
                if (status != Status::OK)
                {
                    break;
                }

                auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
                uint64_t next_page = page->btr_right_sibling;

                unpinIndexPage(current_page, false, ctx);

                current_page = static_cast<uint32_t>(next_page);
            }

            // GC-compact each page at this level
            for (uint32_t page_id : pages_at_level)
            {
                stats.pages_visited++;
                status = gcCompactPage(page_id, stats, ctx);
                if (status != Status::OK && status != Status::NOT_FOUND)
                {
                    // Continue compaction even if one page fails
                    continue;
                }
            }

            // GC compaction currently performs in-page cleanup only.
            // Structural sibling merges stay disabled here until the separator-key rewrite path is
            // fully revalidated for every internal-page shape. This keeps GC reclamation correct
            // without risking parent/child routing corruption during maintenance.
        }

        if (stats_out)
        {
            *stats_out = stats;
        }

        return Status::OK;
    }

    auto BTree::gcCompactPage(uint32_t page_id, GcCompactionStats &stats, ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();

        void *page_data_ptr = nullptr;
        Status status = pinIndexPage(page_id, &page_data_ptr, ctx,
                                     BufferPool::AccessStrategy::Vacuum);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
        uint32_t page_size = page->btr_header.page_size;
        const BTreePageShape shape = normalizeBTreePageShape(page);
        if (shape.kind == BTreePageShapeKind::Invalid)
        {
            unpinIndexPage(page_id, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              shape.error_message != nullptr
                                  ? shape.error_message
                                  : "Invalid B-tree page shape during GC compaction");
            return Status::PAGE_CORRUPT;
        }
        const bool page_shape_dirty = shape.normalized;
        const bool is_leaf = shape.kind == BTreePageShapeKind::Leaf;

        TransactionManager *txn_mgr = db_->transaction_manager();
        uint64_t current_xid = txn_mgr ? txn_mgr->getCurrentXid() : 0;

        // Check if page has garbage to compact
        if (!(page->btr_flags & static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE)))
        {
            unpinIndexPage(page_id, page_shape_dirty, ctx);
            return Status::OK;
        }

        // Count deleted nodes
        auto *offsets = reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(page_data_ptr) +
                                                     sizeof(SBBTreePage));

        uint16_t deleted_count = 0;
        for (uint16_t i = 0; i < page->btr_count; ++i)
        {
            auto *node = reinterpret_cast<SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(page_data_ptr) + offsets[i]);

            bool is_deleted = (node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0;
            if (!is_deleted && node->btn_xmax != 0)
            {
                bool delete_visible = (current_xid == 0);
                if (!delete_visible && txn_mgr)
                {
                    delete_visible = txn_mgr->isVersionVisible(node->btn_xmax, current_xid);
                }
                if (delete_visible)
                {
                    node->btn_flags |= static_cast<uint16_t>(BTreeNodeFlags::DELETED);
                    is_deleted = true;
                }
            }

            if (is_deleted)
            {
                deleted_count++;
            }
        }

        if (deleted_count == 0)
        {
            // Clear the HAS_GARBAGE flag
            page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);
            unpinIndexPage(page_id, true, ctx);
            return Status::OK;
        }

        if (!is_leaf)
        {
            // GC compaction is deliberately leaf-only for now. Internal-page rewrite paths are
            // still undergoing restart-anchor/separator revalidation, so leave internal garbage
            // markers in place rather than risking search-routing corruption during maintenance.
            unpinIndexPage(page_id, page_shape_dirty, ctx);
            return Status::OK;
        }

        // Compact the page
        status = compactPage(reinterpret_cast<uint8_t *>(page_data_ptr), page_size, stats, ctx);
        if (status == Status::OK)
        {
            const BTreePageOpenInspection inspection =
                inspect_btree_page(page, page_id, false);
            if (inspection.status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, inspection.status, inspection.error_message);
                status = inspection.status;
            }
        }

        if (status == Status::OK)
        {
            stats.pages_compacted++;
            stats.nodes_removed += deleted_count;
            unpinIndexPage(page_id, true, ctx);
        }
        else
        {
            unpinIndexPage(page_id, page_shape_dirty, ctx);
        }

        return status;
    }

    auto BTree::compactPage(uint8_t *page_data, uint32_t page_size, GcCompactionStats &stats, ErrorContext *ctx) -> Status
    {
        auto *page = reinterpret_cast<SBBTreePage *>(page_data);
        const auto *offsets = reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));
        const BTreePageShape shape = normalizeBTreePageShape(page);
        if (shape.kind == BTreePageShapeKind::Invalid)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              shape.error_message != nullptr
                                  ? shape.error_message
                                  : "Invalid B-tree page shape during page compaction");
            return Status::PAGE_CORRUPT;
        }
        const bool is_leaf = shape.kind == BTreePageShapeKind::Leaf;
        uint64_t bytes_reclaimed = 0;
        std::vector<uint8_t> prev_key;

        if (is_leaf)
        {
            std::vector<LeafEntryData> live_entries;
            live_entries.reserve(page->btr_count);

            for (uint16_t i = 0; i < page->btr_count; ++i)
            {
                const SBBTreeNode *node = nullptr;
                const uint8_t *node_key_data = nullptr;
                uint32_t node_size = 0;
                if (!loadBTreeNodeView(page_data, page_size, offsets, i, true,
                                       &node, &node_key_data, &node_size))
                {
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "B-tree leaf compaction encountered invalid node bounds");
                    return Status::PAGE_CORRUPT;
                }

                std::vector<uint8_t> full_key = decompress_key(prev_key, node_key_data,
                                                               node->btn_key_len,
                                                               node->btn_prefix_len);
                prev_key = full_key;

                if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
                {
                    bytes_reclaimed += node_size;
                    continue;
                }

                LeafEntryData entry;
                entry.key = std::move(full_key);
                entry.flags = node->btn_flags;
                entry.xmin = node->btn_xmin;
                entry.xmax = node->btn_xmax;

                const auto *tuple_ids_ptr =
                    reinterpret_cast<const OnDiskTID *>(node_key_data + node->btn_key_len);
                entry.tids.reserve(node->btn_tuple_count);
                for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
                {
                    entry.tids.push_back(tuple_ids_ptr[j]);
                }

                live_entries.push_back(std::move(entry));
            }

            Status status = rebuild_leaf_page(page, page_size, live_entries, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }
        else
        {
            std::vector<InternalKeyEntry> live_keys;
            std::vector<uint64_t> children;
            live_keys.reserve(page->btr_count);
            children.reserve(static_cast<size_t>(page->btr_count) + 1);

            for (uint16_t i = 0; i < page->btr_count; ++i)
            {
                const SBBTreeNode *node = nullptr;
                const uint8_t *node_key_data = nullptr;
                uint32_t node_size = 0;
                if (!loadBTreeNodeView(page_data, page_size, offsets, i, false,
                                       &node, &node_key_data, &node_size))
                {
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "B-tree internal compaction encountered invalid node bounds");
                    return Status::PAGE_CORRUPT;
                }

                std::vector<uint8_t> full_key = decompress_key(prev_key, node_key_data,
                                                               node->btn_key_len,
                                                               node->btn_prefix_len);
                prev_key = full_key;

                if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
                {
                    bytes_reclaimed += node_size;
                    continue;
                }

                InternalKeyEntry entry;
                entry.key = std::move(full_key);
                entry.suffix_trunc = node->btn_suffix_trunc;
                entry.flags = node->btn_flags;
                live_keys.push_back(std::move(entry));
                children.push_back(node->btn_child_page);
            }

            children.push_back(page->btr_rightmost_child);
            Status status = rebuild_internal_page(page, page_size, live_keys, children, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        stats.bytes_reclaimed += bytes_reclaimed;

        return Status::OK;
    }

    bool BTree::shouldMergePages(const SBBTreePage *page1, const SBBTreePage *page2) const
    {
        if (!page1 || !page2)
        {
            return false;
        }

        // Don't merge if they're not siblings
        if (page1->btr_right_sibling != page2->btr_header.page_id)
        {
            return false;
        }

        // Don't merge if they're at different levels
        if (page1->btr_level != page2->btr_level)
        {
            return false;
        }

        // Don't merge if they belong to different indices
        if (std::memcmp(page1->btr_index_uuid.bytes.data(), page2->btr_index_uuid.bytes.data(),
                        16) != 0)
        {
            return false;
        }

        // Calculate total space needed if merged
        uint32_t page_size = page1->btr_header.page_size;

        // Space needed for all nodes from both pages
        uint32_t total_nodes = page1->btr_count + page2->btr_count;
        uint32_t total_offset_space = total_nodes * sizeof(uint16_t);

        // Calculate approximate data size (conservative estimate)
        uint32_t page1_data_size = page_size - page1->btr_free_space - sizeof(SBBTreePage);
        uint32_t page2_data_size = page_size - page2->btr_free_space - sizeof(SBBTreePage);
        uint32_t total_data_size = page1_data_size + page2_data_size;

        uint32_t required_space = sizeof(SBBTreePage) + total_offset_space + total_data_size;

        // Only merge if combined data fits in one page with some margin (80% threshold)
        uint32_t threshold = (page_size * 80) / 100;

        return required_space <= threshold;
    }

    auto BTree::mergePages(uint32_t left_page_id, uint32_t right_page_id, GcCompactionStats &stats,
                           ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        PageManager *pm = db_->page_manager();

        // Pin both pages
        void *left_data_ptr = nullptr;
        void *right_data_ptr = nullptr;

        Status status = pinIndexPage(left_page_id, &left_data_ptr, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        status = pinIndexPage(right_page_id, &right_data_ptr, ctx);
        if (status != Status::OK)
        {
            unpinIndexPage(left_page_id, false, ctx);
            return status;
        }

        auto *left_page = reinterpret_cast<SBBTreePage *>(left_data_ptr);
        auto *right_page = reinterpret_cast<SBBTreePage *>(right_data_ptr);
        uint32_t page_size = left_page->btr_header.page_size;
        const BTreePageShape left_shape = normalizeBTreePageShape(left_page);
        if (left_shape.kind == BTreePageShapeKind::Invalid)
        {
            unpinIndexPage(left_page_id, false, ctx);
            unpinIndexPage(right_page_id, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              left_shape.error_message != nullptr
                                  ? left_shape.error_message
                                  : "Invalid B-tree page shape during merge");
            return Status::PAGE_CORRUPT;
        }
        const bool left_page_dirty = left_shape.normalized;
        const bool is_leaf = left_shape.kind == BTreePageShapeKind::Leaf;

        if (!is_leaf)
        {
            unpinIndexPage(left_page_id, left_page_dirty, ctx);
            unpinIndexPage(right_page_id, false, ctx);
            return Status::NOT_SUPPORTED;
        }

        std::vector<LeafEntryData> merged_entries;
        merged_entries.reserve(static_cast<size_t>(left_page->btr_count) +
                               static_cast<size_t>(right_page->btr_count));

        const auto append_leaf_entries = [&](const uint8_t *page_data,
                                             const SBBTreePage *page_hdr) -> Status
        {
            const auto *offsets = reinterpret_cast<const uint16_t *>(
                page_data + sizeof(SBBTreePage));
            std::vector<uint8_t> prev_key;

            for (uint16_t i = 0; i < page_hdr->btr_count; ++i)
            {
                const SBBTreeNode *node = nullptr;
                const uint8_t *node_key_data = nullptr;
                uint32_t node_size = 0;
                if (!loadBTreeNodeView(page_data, page_size, offsets, i, true,
                                       &node, &node_key_data, &node_size))
                {
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "B-tree page merge encountered invalid leaf node bounds");
                    return Status::PAGE_CORRUPT;
                }

                LeafEntryData entry;
                entry.key = decompress_key(prev_key,
                                           node_key_data,
                                           node->btn_key_len,
                                           node->btn_prefix_len);
                prev_key = entry.key;
                entry.flags = node->btn_flags;
                entry.xmin = node->btn_xmin;
                entry.xmax = node->btn_xmax;

                const auto *tuple_ids_ptr =
                    reinterpret_cast<const OnDiskTID *>(node_key_data + node->btn_key_len);
                entry.tids.reserve(node->btn_tuple_count);
                for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
                {
                    entry.tids.push_back(tuple_ids_ptr[j]);
                }

                merged_entries.push_back(std::move(entry));
            }

            return Status::OK;
        };

        status = append_leaf_entries(reinterpret_cast<const uint8_t *>(left_data_ptr),
                                     left_page);
        if (status != Status::OK)
        {
            unpinIndexPage(left_page_id, false, ctx);
            unpinIndexPage(right_page_id, false, ctx);
            return status;
        }
        status = append_leaf_entries(reinterpret_cast<const uint8_t *>(right_data_ptr),
                                     right_page);
        if (status != Status::OK)
        {
            unpinIndexPage(left_page_id, false, ctx);
            unpinIndexPage(right_page_id, false, ctx);
            return status;
        }

        // Update sibling pointers
        left_page->btr_right_sibling = right_page->btr_right_sibling;
        if ((right_page->btr_flags & static_cast<uint16_t>(BTreeFlags::RIGHTMOST)) != 0)
        {
            left_page->btr_flags |= static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
        }
        else
        {
            left_page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
        }

        status = rebuild_leaf_page(left_page, page_size, merged_entries, ctx);
        if (status != Status::OK)
        {
            unpinIndexPage(left_page_id, false, ctx);
            unpinIndexPage(right_page_id, false, ctx);
            return status;
        }

        // Update right sibling's left pointer if it exists
        if (right_page->btr_right_sibling != 0)
        {
            void *right_sibling_data_ptr = nullptr;
            status = pinIndexPage(static_cast<uint32_t>(right_page->btr_right_sibling),
                                 &right_sibling_data_ptr, ctx);
            if (status == Status::OK)
            {
                auto *right_sibling = reinterpret_cast<SBBTreePage *>(right_sibling_data_ptr);
                right_sibling->btr_left_sibling = left_page_id;
                unpinIndexPage(static_cast<uint32_t>(right_page->btr_right_sibling), true, ctx);
            }
        }

        // CRITICAL FIX (Issue 2.11): Update parent to remove separator key for right page
        // When pages are merged, the separator key in the parent that points to the right page
        // must be removed to maintain B-tree structure integrity
        uint64_t parent_page_num = right_page->btr_parent_page;

        // Mark pages as dirty and unpin before updating parent
        unpinIndexPage(left_page_id, true, ctx);
        unpinIndexPage(right_page_id, false, ctx);

        // Free the right page
        pm->freePageGlobal(indexGPID(right_page_id), ctx);

        // Update parent if it exists (not root)
        if (parent_page_num != 0)
        {
            status = removeFromParent(parent_page_num, right_page_id, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                // Log warning but don't fail the merge - pages are already merged
                // The parent inconsistency will be detected during validation
            }

            // STOR-L3: Check if parent is underutilized and can be merged with sibling
            // This implements recursive upward merge to maintain B-tree balance
            if (status == Status::OK)
            {
                checkAndMergeParentRecursive(parent_page_num, stats, ctx);
            }
        }

        stats.pages_merged++;

        return Status::OK;
    }

    auto BTree::removeFromParent(uint64_t parent_page_num, uint64_t child_page_id,
                                 ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();

        // Pin parent page
        void *parent_data_ptr = nullptr;
        Status status = pinIndexPage(parent_page_num, &parent_data_ptr, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *parent_page = reinterpret_cast<SBBTreePage *>(parent_data_ptr);
        const uint32_t page_size = parent_page->btr_header.page_size;
        const auto *page_data = reinterpret_cast<const uint8_t *>(parent_data_ptr);
        const auto *parent_offsets = reinterpret_cast<const uint16_t *>(
            page_data + sizeof(SBBTreePage));

        std::vector<InternalKeyEntry> keys;
        std::vector<uint64_t> children;
        keys.reserve(parent_page->btr_count);
        children.reserve(static_cast<size_t>(parent_page->btr_count) + 1);

        std::vector<uint8_t> prev_key;
        for (uint16_t i = 0; i < parent_page->btr_count; ++i)
        {
            const SBBTreeNode *node = nullptr;
            const uint8_t *node_key_data = nullptr;
            uint32_t node_size = 0;
            if (!loadBTreeNodeView(page_data, page_size, parent_offsets, i, false,
                                   &node, &node_key_data, &node_size))
            {
                unpinIndexPage(parent_page_num, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Parent page contains invalid internal node bounds");
                return Status::PAGE_CORRUPT;
            }

            InternalKeyEntry entry;
            entry.key = decompress_key(prev_key,
                                       node_key_data,
                                       node->btn_key_len,
                                       node->btn_prefix_len);
            prev_key = entry.key;
            entry.suffix_trunc = node->btn_suffix_trunc;
            entry.flags = node->btn_flags;
            keys.push_back(std::move(entry));
            children.push_back(node->btn_child_page);
        }
        children.push_back(parent_page->btr_rightmost_child);

        auto child_it = std::find(children.begin(), children.end(), child_page_id);
        if (child_it == children.end())
        {
            unpinIndexPage(parent_page_num, false, ctx);
            return Status::NOT_FOUND;
        }

        const size_t child_index = static_cast<size_t>(std::distance(children.begin(), child_it));
        size_t key_index_to_remove = child_index;
        if (child_index > 0)
        {
            key_index_to_remove = child_index - 1;
        }
        if (key_index_to_remove < keys.size())
        {
            keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(key_index_to_remove));
        }
        children.erase(child_it);

        const Status rebuild_status = rebuild_internal_page(parent_page,
                                                            page_size,
                                                            keys,
                                                            children,
                                                            ctx);
        if (rebuild_status != Status::OK)
        {
            unpinIndexPage(parent_page_num, false, ctx);
            return rebuild_status;
        }

        unpinIndexPage(parent_page_num, true, ctx);

        return Status::OK;
    }

    // PHASE 2 TASK 2.2: IndexGCInterface implementation
    // Remove index entries pointing to dead tuples
    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    Status BTree::removeDeadEntries(const std::vector<TID> &dead_tids,
                                    uint64_t *entries_removed_out,
                                    uint64_t *pages_modified_out,
                                    ErrorContext *ctx)
    {
        IndexCleanupDebtSnapshot cleanup_debt_snapshot{};

        // Initialize output counters
        if (entries_removed_out != nullptr)
        {
            *entries_removed_out = 0;
        }
        if (pages_modified_out != nullptr)
        {
            *pages_modified_out = 0;
        }

        // Early exit if no dead TIDs
        if (dead_tids.empty())
        {
            storeCleanupDebtSnapshot(cleanup_debt_snapshot);
            return Status::OK;
        }

        std::unordered_set<TID> dead_set;
        for (const TID &tid : dead_tids)
        {
            dead_set.insert(tid);
        }

        BufferPool *bp = db_->buffer_pool();
        if (!bp)
        {
            storeCleanupDebtSnapshot(cleanup_debt_snapshot);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool is null");
            return Status::INVALID_ARGUMENT;
        }

        // Statistics
        uint64_t total_entries_removed = 0;
        uint64_t total_pages_modified = 0;
        bool had_errors = false;

        // Find the leftmost leaf page
        // Strategy: Start from root and go left at each level until we hit a leaf
        uint64_t current_page_num = index_info_.idx_root_page;

        // Navigate to leftmost leaf
        while (true)
        {
            void *page_buffer = nullptr;
            Status pin_status = pinIndexPage(current_page_num, &page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                storeCleanupDebtSnapshot(cleanup_debt_snapshot);
                SET_ERROR_CONTEXT(ctx, pin_status, "Failed to pin page during GC navigation");
                return pin_status;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
            uint16_t level = page->btr_level;

            if (level == 0)
            {
                // We've reached a leaf, unpin and break
                unpinIndexPage(current_page_num, false, ctx);
                break;
            }

            // Internal node - go to leftmost child
            if (page->btr_count == 0)
            {
                // Empty internal node - shouldn't happen, but handle gracefully
                unpinIndexPage(current_page_num, false, ctx);
                storeCleanupDebtSnapshot(cleanup_debt_snapshot);
                SET_ERROR_CONTEXT(ctx, Status::INDEX_CORRUPTED, "Empty internal node encountered");
                return Status::INDEX_CORRUPTED;
            }

            // Get the first node to find its left child
            uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);
            auto *first_node = reinterpret_cast<SBBTreeNode *>(
                page_data + sizeof(SBBTreePage));

            uint64_t next_page = first_node->btn_child_page;
            unpinIndexPage(current_page_num, false, ctx);

            current_page_num = next_page;
        }

        // Now scan all leaf pages left-to-right using sibling pointers
        uint64_t leaf_page_num = current_page_num;

        while (leaf_page_num != 0)
        {
            void *page_buffer = nullptr;
            Status pin_status = pinIndexPage(leaf_page_num, &page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                LOG_WARNING(VACUUM, "B-Tree GC: Failed to pin leaf page %lu: %d",
                            leaf_page_num, static_cast<int>(pin_status));
                had_errors = true;
                break; // Stop scanning on error
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
            uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);
            const uint32_t page_size = page->btr_header.page_size;

            // Verify this is a leaf page
            if (page->btr_level != 0)
            {
                LOG_WARNING(VACUUM, "B-Tree GC: Expected leaf page but got level %u at page %lu",
                            page->btr_level, leaf_page_num);
                unpinIndexPage(leaf_page_num, false, ctx);
                had_errors = true;
                break;
            }

            uint16_t entry_count = page->btr_count;
            uint64_t entries_removed_this_page = 0;
            uint64_t reclaimable_bytes_this_page = 0;
            bool page_modified = false;

            // Scan all entries on this leaf page
            // We'll mark entries as deleted by setting the DELETED flag
            auto *offsets = reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));

            for (uint16_t slot = 0; slot < entry_count; slot++)
            {
                const SBBTreeNode *node = nullptr;
                const uint8_t *node_key_data = nullptr;
                uint32_t node_size = 0;
                if (!loadBTreeNodeView(page_data,
                                       page_size,
                                       offsets,
                                       slot,
                                       true,
                                       &node,
                                       &node_key_data,
                                       &node_size))
                {
                    LOG_WARNING(VACUUM,
                                "B-Tree GC: Invalid node bounds on leaf page %lu slot %u",
                                leaf_page_num,
                                static_cast<unsigned>(slot));
                    had_errors = true;
                    break;
                }
                (void)node_key_data;

                // Skip already deleted entries
                if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
                {
                    reclaimable_bytes_this_page += node_size;
                    continue;
                }

                // Get tuple IDs for this entry
                uint16_t key_len = node->btn_key_len;
                uint32_t tuple_count = node->btn_tuple_count;
                const uint8_t *tuple_ids_ptr =
                    reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode) + key_len;
                const auto *tuple_ids = reinterpret_cast<const OnDiskTID *>(tuple_ids_ptr);

                // Check if any of this entry's tuple IDs are in the dead set
                bool has_dead_tuples = false;
                for (uint32_t i = 0; i < tuple_count; i++)
                {
                    if (dead_set.find(fromOnDiskTID(tuple_ids[i])) != dead_set.end())
                    {
                        has_dead_tuples = true;
                        break;
                    }
                }

                if (has_dead_tuples)
                {
                    // Mark entry as deleted
                    auto *mutable_node =
                        reinterpret_cast<SBBTreeNode *>(page_data + offsets[slot]);
                    mutable_node->btn_flags |= static_cast<uint16_t>(BTreeNodeFlags::DELETED);
                    entries_removed_this_page++;
                    reclaimable_bytes_this_page += node_size;
                    page_modified = true;
                }
            }

            if (had_errors)
            {
                unpinIndexPage(leaf_page_num, page_modified, ctx);
                break;
            }

            // If we modified this page, set the HAS_GARBAGE flag
            if (page_modified)
            {
                page->btr_flags |= static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);
                const uint32_t compact_threshold_bytes =
                    percentThresholdBytes(page_size, kCleanupCompactMinFreePercent);
                if (reclaimable_bytes_this_page >= compact_threshold_bytes)
                {
                    GcCompactionStats compact_stats{};
                    Status compact_status = compactPage(page_data, page_size, compact_stats, ctx);
                    if (compact_status == Status::OK)
                    {
                        const BTreePageOpenInspection inspection =
                            inspect_btree_page(page, leaf_page_num, false);
                        if (inspection.status != Status::OK)
                        {
                            compact_status = inspection.status;
                            SET_ERROR_CONTEXT(ctx, inspection.status, inspection.error_message);
                        }
                    }

                    if (compact_status != Status::OK)
                    {
                        LOG_WARNING(VACUUM,
                                    "B-Tree GC: Immediate compaction failed on leaf page %lu: %d",
                                    leaf_page_num,
                                    static_cast<int>(compact_status));
                        had_errors = true;
                        cleanup_debt_snapshot.repair_required = true;
                    }
                }
                total_pages_modified++;
                total_entries_removed += entries_removed_this_page;
            }

            uint64_t residual_garbage_bytes = 0;
            if (!measureLeafGarbageBytes(page_data, page_size, &residual_garbage_bytes))
            {
                LOG_WARNING(VACUUM,
                            "B-Tree GC: Failed to measure residual garbage on leaf page %lu",
                            leaf_page_num);
                had_errors = true;
                cleanup_debt_snapshot.repair_required = true;
            }
            else if (residual_garbage_bytes > 0 ||
                     (page->btr_flags & static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE)) != 0)
            {
                cleanup_debt_snapshot.backlog_pages++;
                cleanup_debt_snapshot.backlog_bytes += residual_garbage_bytes;
                if (cleanup_debt_snapshot.first_locality_page_id == 0)
                {
                    cleanup_debt_snapshot.first_locality_page_id =
                        static_cast<uint32_t>(leaf_page_num);
                }
            }

            uint64_t next_leaf = page->btr_right_sibling;

            // Unpin current page
            unpinIndexPage(leaf_page_num, page_modified, ctx);

            if (had_errors)
            {
                break;
            }

            // Move to next leaf
            leaf_page_num = next_leaf;
        }

        // Update output counters
        if (entries_removed_out != nullptr)
        {
            *entries_removed_out = total_entries_removed;
        }
        if (pages_modified_out != nullptr)
        {
            *pages_modified_out = total_pages_modified;
        }

        if (bloom_filter_ && total_entries_removed > 0)
        {
            Status bf_status = rebuildBloomFilter(ctx);
            if (bf_status != Status::OK)
            {
                LOG_WARNING(VACUUM, "B-Tree bloom filter rebuild failed: %d",
                            static_cast<int>(bf_status));
            }
        }

        storeCleanupDebtSnapshot(cleanup_debt_snapshot);

        // Return appropriate status
        if (had_errors)
        {
            // Had some errors but may have removed some entries
            // Caller can check entries_removed_out to see progress
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    // ============================================================================
    // PHASE 5 TASK 5.2: B-TREE TID UPDATES FOR TABLESPACE MIGRATION
    // ============================================================================

    /**
     * updateTIDsAfterMigration - Update TIDs in B-Tree leaf nodes after table migration
     *
     * This method is called when a table has been migrated to a different tablespace.
     * It traverses all leaf nodes in the B-Tree and updates TIDs that reference
     * migrated heap pages.
     *
     * Algorithm:
     * 1. Navigate to leftmost leaf page (same as removeDeadEntries)
     * 2. Scan all leaf pages left-to-right using sibling pointers
     * 3. For each leaf page:
     *    a. Scan all index entries (nodes)
     *    b. For each entry, scan all tuple IDs (OnDiskTID format)
     *    c. Check if TID is in tid_mapping (old TID -> new TID)
     *    d. If found, update TID with new GPID/slot
     *    f. Mark page as dirty if any TIDs updated
     * 4. Continue to next leaf via btr_right_sibling pointer
     * 5. Return total TIDs updated and pages modified
     *
     * Note: TIDs are stored on-disk as packed (GPID + slot) and updated in-place.
     *
     * @param tid_mapping Map of old TID -> new TID for migrated tuples
     * @param tids_updated_out Output: Total number of TIDs updated
     * @param pages_modified_out Output: Total number of leaf pages modified
     * @param ctx Error context
     * @return Status::OK on success, error status otherwise
     */
    Status BTree::updateTIDsAfterMigration(const std::unordered_map<TID, TID> &tid_mapping,
                                          uint64_t *tids_updated_out,
                                          uint64_t *pages_modified_out,
                                          ErrorContext *ctx)
    {
        // Initialize output counters
        if (tids_updated_out != nullptr)
        {
            *tids_updated_out = 0;
        }
        if (pages_modified_out != nullptr)
        {
            *pages_modified_out = 0;
        }

        // Early exit if no TID mapping (empty table or no migration)
        if (tid_mapping.empty())
        {
            return Status::OK;
        }

        BufferPool *bp = db_->buffer_pool();
        if (!bp)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool is null");
            return Status::INVALID_ARGUMENT;
        }

        // Statistics
        uint64_t total_tids_updated = 0;
        uint64_t total_pages_modified = 0;
        bool had_errors = false;

        // ===== STEP 1: Find the leftmost leaf page =====
        // Strategy: Start from root and go left at each level until we hit a leaf
        uint64_t current_page_num = index_info_.idx_root_page;

        // Navigate to leftmost leaf
        while (true)
        {
            void *page_buffer = nullptr;
            Status pin_status = pinIndexPage(current_page_num, &page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, pin_status,
                                "Failed to pin page during TID update navigation");
                return pin_status;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
            uint16_t level = page->btr_level;

            if (level == 0)
            {
                // We've reached a leaf, unpin and break
                unpinIndexPage(current_page_num, false, ctx);
                break;
            }

            // Internal node - go to leftmost child
            if (page->btr_count == 0)
            {
                // Empty internal node - shouldn't happen, but handle gracefully
                unpinIndexPage(current_page_num, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::INDEX_CORRUPTED,
                                "Empty internal node encountered during TID update");
                return Status::INDEX_CORRUPTED;
            }

            // Get the first node to find its left child
            uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);
            auto *offsets = reinterpret_cast<uint16_t *>(page_data + sizeof(SBBTreePage));
            auto *first_node = reinterpret_cast<SBBTreeNode *>(page_data + offsets[0]);

            uint64_t next_page = first_node->btn_child_page;
            unpinIndexPage(current_page_num, false, ctx);

            current_page_num = next_page;
        }

        // ===== STEP 2: Scan all leaf pages left-to-right using sibling pointers =====
        uint64_t leaf_page_num = current_page_num;

        while (leaf_page_num != 0)
        {
            void *page_buffer = nullptr;
            Status pin_status = pinIndexPage(leaf_page_num, &page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                LOG_WARNING(CATALOG,
                           "B-Tree TID update: Failed to pin leaf page %lu: %d",
                           leaf_page_num, static_cast<int>(pin_status));
                had_errors = true;
                break; // Stop scanning on error
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
            uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);

            // Verify this is a leaf page
            if (page->btr_level != 0)
            {
                LOG_WARNING(CATALOG,
                           "B-Tree TID update: Expected leaf page but got level %u at page %lu",
                           page->btr_level, leaf_page_num);
                unpinIndexPage(leaf_page_num, false, ctx);
                had_errors = true;
                break;
            }

            uint16_t entry_count = page->btr_count;
            uint64_t tids_updated_this_page = 0;
            bool page_modified = false;

            // ===== STEP 3: Scan all entries on this leaf page =====
            auto *offsets = reinterpret_cast<uint16_t *>(page_data + sizeof(SBBTreePage));

            for (uint16_t slot = 0; slot < entry_count; slot++)
            {
                auto *node = reinterpret_cast<SBBTreeNode *>(page_data + offsets[slot]);

                // Skip deleted entries (from GC compaction operations)
                if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
                {
                    continue;
                }

                // Get tuple IDs for this entry
                uint16_t key_len = node->btn_key_len;
                uint32_t tuple_count = node->btn_tuple_count;

                // Tuple IDs are stored right after the key data
                uint8_t *tuple_ids_ptr = reinterpret_cast<uint8_t *>(node) +
                                        sizeof(SBBTreeNode) + key_len;
                auto *tuple_ids = reinterpret_cast<OnDiskTID *>(tuple_ids_ptr);

                // ===== STEP 4: Update each tuple ID if it is in tid_mapping =====
                for (uint32_t i = 0; i < tuple_count; i++)
                {
                    TID old_tid = fromOnDiskTID(tuple_ids[i]);

                    // Check if this TID was migrated
                    auto it = tid_mapping.find(old_tid);
                    if (it != tid_mapping.end())
                    {
                        // Update the TID in-place
                        tuple_ids[i] = toOnDiskTID(it->second);
                        tids_updated_this_page++;
                        page_modified = true;
                    }
                }
            }

            // If we modified this page, update statistics
            if (page_modified)
            {
                total_pages_modified++;
                total_tids_updated += tids_updated_this_page;
            }

            uint64_t next_leaf = page->btr_right_sibling;

            // Unpin current page (mark dirty if modified)
            unpinIndexPage(leaf_page_num, page_modified, ctx);

            // Move to next leaf
            leaf_page_num = next_leaf;
        }

        // Update output counters
        if (tids_updated_out != nullptr)
        {
            *tids_updated_out = total_tids_updated;
        }
        if (pages_modified_out != nullptr)
        {
            *pages_modified_out = total_pages_modified;
        }

        // Return appropriate status
        if (had_errors)
        {
            // Had some errors but may have updated some TIDs
            // Caller can check tids_updated_out to see progress
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                            "Errors encountered during B-Tree TID update");
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto BTree::bulkLoad(std::vector<std::pair<std::vector<uint8_t>, TID>> &entries,
                         uint64_t xid,
                         ErrorContext *ctx,
                         BulkLoadStats *stats_out) -> Status
    {
        BulkLoadStats stats{};
        const auto elapsed_micros =
            [](const std::chrono::steady_clock::time_point &start_time) -> uint64_t
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start_time)
                    .count());
        };

        // P1-11: Bulk loading optimization for B-Tree index construction
        //
        // Bottom-up construction algorithm (O(N) after sorting):
        //   Phase 1: Sort entries by key - O(N log N)
        //   Phase 2: Build leaf pages from sorted entries - O(N)
        //   Phase 3: Build internal nodes bottom-up - O(N)
        //
        // Benefits over individual inserts:
        //   - No page splits during construction (pre-allocate full pages)
        //   - Better page utilization (~95% vs gradual filling)
        //   - Sequential I/O (no random page updates)
        //   - 3-5x faster for large datasets
        //
        // Reference: PostgreSQL's _bt_load() in nbtree/nbtsort.c

        if (entries.empty())
        {
            return Status::OK; // Nothing to load
        }

        const uint64_t prior_root_page = index_info_.idx_root_page;
        const GPID prior_root_gpid = indexGPID(prior_root_page);
        bool prior_root_structurally_empty = false;
        {
            void *prior_root_ptr = nullptr;
            ErrorContext prior_root_ctx;
            Status prior_root_status = pinIndexPage(prior_root_page, &prior_root_ptr, &prior_root_ctx);
            if (prior_root_status == Status::OK && prior_root_ptr != nullptr)
            {
                const auto *prior_root =
                    reinterpret_cast<const SBBTreePage *>(prior_root_ptr);
                prior_root_structurally_empty =
                    prior_root->btr_level == 0 &&
                    prior_root->btr_count == 0 &&
                    prior_root->btr_left_sibling == 0 &&
                    prior_root->btr_right_sibling == 0 &&
                    prior_root->btr_rightmost_child == 0;
                unpinIndexPage(prior_root_page, false, &prior_root_ctx);
            }
        }

        BufferPool *buffer_pool = db_->buffer_pool();
        PageManager *page_manager = db_->page_manager();
        uint32_t page_size = db_->page_size();

        if (!buffer_pool || !page_manager)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                            "Database components not initialized for bulk load");
            return Status::INVALID_ARGUMENT;
        }

        // ========================================================================
        // Phase 1: Sort entries by key when needed - O(N) fast-path for ordered input
        // ========================================================================
        const auto sort_phase_start = std::chrono::steady_clock::now();
        bool entries_already_sorted = true;
        for (size_t i = 1; i < entries.size(); ++i)
        {
            if (compare_keys(entries[i - 1].first, entries[i].first) > 0)
            {
                entries_already_sorted = false;
                break;
            }
        }

        if (!entries_already_sorted)
        {
            std::sort(entries.begin(), entries.end(),
                      [this](const auto &a, const auto &b) {
                          return this->compare_keys(a.first, b.first) < 0;
                      });
        }
        stats.input_already_sorted = entries_already_sorted;
        stats.sort_us += elapsed_micros(sort_phase_start);

        const auto bulk_strategy = BufferPool::AccessStrategy::BulkWrite;

        // ========================================================================
        // Phase 2: Build leaf pages from sorted entries - O(N)
        // ========================================================================

        // Calculate available space per leaf page (after header and offsets overhead)
        // We leave 5% slack for header growth and alignment
        uint32_t available_space = page_size - sizeof(SBBTreePage);
        uint32_t target_fill = (available_space * 95) / 100; // 95% fill factor

        // Calculate average entry size for estimation
        // Entry size = SBBTreeNode header + key size + TID (uint64_t)
        size_t total_key_size = 0;
        for (const auto &entry : entries)
        {
            total_key_size += entry.first.size();
        }
        size_t avg_key_size = entries.empty() ? 16 : (total_key_size / entries.size());
        size_t avg_entry_size = sizeof(SBBTreeNode) + avg_key_size + sizeof(uint64_t) + sizeof(uint16_t);

        // Estimate entries per page (conservative to avoid overflow)
        size_t entries_per_page = target_fill / avg_entry_size;
        if (entries_per_page < 2) entries_per_page = 2; // Minimum 2 entries per page

        // Build leaf pages
        std::vector<uint32_t> leaf_pages;
        std::vector<std::vector<uint8_t>> separator_keys; // First key of each leaf page (except first)

        const auto leaf_build_start = std::chrono::steady_clock::now();
        size_t entry_idx = 0;
        while (entry_idx < entries.size())
        {
            // Allocate a new leaf page
            uint32_t leaf_page_num = 0;
            GPID leaf_gpid = 0;
            Status status = page_manager->allocatePageInTablespace(index_info_.idx_tablespace_id,
                                                                  &leaf_gpid, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to allocate leaf page during bulk load");
                return status;
            }
            leaf_page_num = static_cast<uint32_t>(getPageNumber(leaf_gpid));

            // Pin and initialize the page
            void *page_data_ptr = nullptr;
            status = pinIndexPage(leaf_page_num, &page_data_ptr, ctx, bulk_strategy);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to pin leaf page during bulk load");
                return status;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
            uint16_t leaf_flags = static_cast<uint16_t>(BTreeFlags::LEAF);
            if (leaf_pages.empty())
            {
                leaf_flags |= static_cast<uint16_t>(BTreeFlags::LEFTMOST);
            }
            status = initializeBTreeRuntimePage(
                db_, reinterpret_cast<uint8_t *>(page_data_ptr), page_size, leaf_page_num,
                index_info_.idx_uuid, index_info_.idx_table_uuid, 0, leaf_flags);
            if (status != Status::OK)
            {
                unpinIndexPage(leaf_page_num, false, ctx);
                SET_ERROR_CONTEXT(ctx, status,
                                  "Failed to initialize leaf page during bulk load");
                return status;
            }
            page->btr_xmin = xid;
            page->btr_xmax = 0;

            // Record first key as separator for internal nodes (except first page)
            if (!leaf_pages.empty() && entry_idx < entries.size())
            {
                separator_keys.push_back(entries[entry_idx].first);
            }

            // Fill the leaf page with entries
            BTreePage btree_page(reinterpret_cast<uint8_t *>(page_data_ptr), page_size);
            std::vector<uint8_t> prev_full_key_for_page;

            while (entry_idx < entries.size())
            {
                const auto &entry = entries[entry_idx];

                // Create tuple and add node
                Tuple tuple;
                tuple.tid = entry.second;
                tuple.data = nullptr;
                tuple.data_size = 0;

                status = btree_page.append_sorted_leaf_node(
                    entry.first, tuple, xid, prev_full_key_for_page, ctx);
                if (status != Status::OK)
                {
                    if (status == Status::PAGE_FULL)
                    {
                        break;
                    }
                    unpinIndexPage(leaf_page_num, false, ctx);
                    return status;
                }

                if (bloom_filter_)
                {
                    Status bf_status =
                        bloom_filter_->insert(entry.first.data(), entry.first.size(), ctx);
                    if (bf_status != Status::OK)
                    {
                        LOG_WARNING(STORAGE, "B-Tree bloom filter insert failed during bulk load: %d",
                                    static_cast<int>(bf_status));
                    }
                }

                prev_full_key_for_page = entry.first;
                entry_idx++;
            }

            // Link to previous leaf page
            if (!leaf_pages.empty())
            {
                page->btr_left_sibling = leaf_pages.back();

                // Update previous page's right sibling pointer
                void *prev_page_ptr = nullptr;
                status = pinIndexPage(leaf_pages.back(), &prev_page_ptr, ctx, bulk_strategy);
                if (status == Status::OK)
                {
                    auto *prev_page = reinterpret_cast<SBBTreePage *>(prev_page_ptr);
                    prev_page->btr_right_sibling = leaf_page_num;
                    unpinIndexPage(leaf_pages.back(), true, ctx);
                }
            }

            leaf_pages.push_back(leaf_page_num);
            unpinIndexPage(leaf_page_num, true, ctx);
        }

        // Mark last leaf page as rightmost
        if (!leaf_pages.empty())
        {
            void *last_page_ptr = nullptr;
            Status status = pinIndexPage(leaf_pages.back(), &last_page_ptr, ctx, bulk_strategy);
            if (status == Status::OK)
            {
                auto *last_page = reinterpret_cast<SBBTreePage *>(last_page_ptr);
                last_page->btr_flags |= static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
                unpinIndexPage(leaf_pages.back(), true, ctx);
            }
        }
        stats.leaf_build_us += elapsed_micros(leaf_build_start);

        // ========================================================================
        // Phase 3: Build internal nodes bottom-up - O(N)
        // ========================================================================

        // If only one leaf page, it becomes the root
        if (leaf_pages.size() == 1)
        {
            const auto root_finalize_start = std::chrono::steady_clock::now();
            void *root_page_ptr = nullptr;
            Status status = pinIndexPage(leaf_pages[0], &root_page_ptr, ctx, bulk_strategy);
            if (status == Status::OK)
            {
                auto *root_page = reinterpret_cast<SBBTreePage *>(root_page_ptr);
                root_page->btr_flags |= static_cast<uint16_t>(BTreeFlags::ROOT);
                unpinIndexPage(leaf_pages[0], true, ctx);
            }
            index_info_.idx_root_page = leaf_pages[0];
            index_info_.idx_height = 1;
            index_info_.idx_page_count = 1;
            index_info_.idx_tuple_count = entries.size();
            status = persistRootGPID(index_info_.idx_root_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            updateRightmostLeafHint(leaf_pages[0], entries.back().first);
            if (prior_root_structurally_empty &&
                prior_root_page != index_info_.idx_root_page)
            {
                ErrorContext reclaim_ctx;
                Status reclaim_status =
                    page_manager->freePageGlobal(prior_root_gpid, &reclaim_ctx);
                if (reclaim_status != Status::OK)
                {
                    LOG_WARNING(STORAGE,
                                "Failed to reclaim prior empty B-tree root after bulk load: %s",
                                reclaim_ctx.message.c_str());
                }
            }
            stats.root_finalize_us += elapsed_micros(root_finalize_start);
            if (stats_out != nullptr)
            {
                *stats_out = stats;
            }
            return Status::OK;
        }

        // Build internal levels until we have a single root
        std::vector<uint32_t> current_level = leaf_pages;
        std::vector<std::vector<uint8_t>> current_separators = separator_keys;
        uint16_t level = 1;

        const auto internal_build_start = std::chrono::steady_clock::now();
        while (current_level.size() > 1)
        {
            std::vector<uint32_t> next_level;
            std::vector<std::vector<uint8_t>> next_separators;

            // Calculate children per internal page
            // Internal node entry: SBBTreeNode header + key + child pointer (uint64_t)
            size_t avg_sep_size = 0;
            for (const auto &sep : current_separators)
            {
                avg_sep_size += sep.size();
            }
            avg_sep_size = current_separators.empty() ? 16 : (avg_sep_size / current_separators.size());
            size_t internal_entry_size = sizeof(SBBTreeNode) + avg_sep_size + sizeof(uint16_t);
            size_t children_per_page = target_fill / internal_entry_size;
            if (children_per_page < 2) children_per_page = 2;

            size_t child_idx = 0;
            size_t sep_idx = 0;

            while (child_idx < current_level.size())
            {
                // Allocate internal page
                uint32_t internal_page_num = 0;
                GPID internal_gpid = 0;
                Status status = page_manager->allocatePageInTablespace(index_info_.idx_tablespace_id,
                                                                      &internal_gpid, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to allocate internal page during bulk load");
                    return status;
                }
                internal_page_num = static_cast<uint32_t>(getPageNumber(internal_gpid));

                void *page_data_ptr = nullptr;
                status = pinIndexPage(internal_page_num, &page_data_ptr, ctx, bulk_strategy);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to pin internal page during bulk load");
                    return status;
                }

                auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
                uint16_t internal_flags = 0;
                if (next_level.empty())
                {
                    internal_flags |= static_cast<uint16_t>(BTreeFlags::LEFTMOST);
                }
                status = initializeBTreeRuntimePage(
                    db_, reinterpret_cast<uint8_t *>(page_data_ptr), page_size,
                    internal_page_num, index_info_.idx_uuid, index_info_.idx_table_uuid,
                    level, internal_flags);
                if (status != Status::OK)
                {
                    unpinIndexPage(internal_page_num, false, ctx);
                    SET_ERROR_CONTEXT(ctx, status,
                                      "Failed to initialize internal page during bulk load");
                    return status;
                }
                page->btr_xmin = xid;
                page->btr_xmax = 0;

                // Record first separator for next level (except first internal page)
                if (!next_level.empty() && sep_idx < current_separators.size())
                {
                    next_separators.push_back(current_separators[sep_idx]);
                }

                // Fill internal page with child pointers and separator keys
                //
                // B-tree internal node structure:
                //   - N keys separate N+1 children
                //   - node[i].btn_child_page = child to the LEFT of key[i]
                //   - btr_rightmost_child = rightmost child (to the right of all keys)
                //
                // Given children: [c0, c1, c2, c3] and separators: [k0, k1, k2]
                //   c0 < k0 < c1 < k1 < c2 < k2 < c3
                //
                // We store:
                //   node[0]: key=k0, btn_child_page=c0  (c0 is left of k0)
                //   node[1]: key=k1, btn_child_page=c1  (c1 is left of k1)
                //   node[2]: key=k2, btn_child_page=c2  (c2 is left of k2)
                //   btr_rightmost_child = c3             (c3 is right of k2)

                auto *offsets = reinterpret_cast<uint16_t *>(
                    reinterpret_cast<uint8_t *>(page) + sizeof(SBBTreePage));

                size_t page_children = 0;
                size_t first_child_idx_for_page = child_idx;

                // Add entries: each key[i] paired with child[i] (left of key)
                // We need N-1 separators for N children
                // children[first..last] where last-first+1 = number of children
                while (child_idx < current_level.size() &&
                       page_children < children_per_page)
                {
                    // For children beyond the first, we need a separator key
                    if (page_children > 0)
                    {
                        if (sep_idx >= current_separators.size())
                        {
                            break; // No more separators available
                        }

                        // Calculate entry size for this separator + child
                        uint32_t entry_size = sizeof(SBBTreeNode) + current_separators[sep_idx].size();

                        // Check if entry fits
                        uint32_t needed = entry_size + sizeof(uint16_t);
                        if (page->btr_free_space < needed)
                        {
                            break;
                        }

                        // Allocate node from high water mark
                        page->btr_high_water -= entry_size;
                        auto *node = reinterpret_cast<SBBTreeNode *>(
                            reinterpret_cast<uint8_t *>(page) + page->btr_high_water);

                        // Initialize internal node
                        // btn_child_page points to child[page_children-1] (to the LEFT of this key)
                        node->btn_flags = 0;
                        node->btn_prefix_len = 0;
                        node->btn_suffix_trunc = 0;
                        node->btn_key_len = current_separators[sep_idx].size();
                        node->btn_tuple_count = 0; // Internal nodes don't have tuples
                        node->btn_child_page = current_level[child_idx - 1]; // Previous child is to the left
                        node->btn_xmin = xid;
                        node->btn_xmax = 0;

                        // Copy separator key
                        uint8_t *key_location = reinterpret_cast<uint8_t *>(node) + sizeof(SBBTreeNode);
                        std::memcpy(key_location, current_separators[sep_idx].data(),
                                   current_separators[sep_idx].size());

                        // Update offsets array
                        offsets[page->btr_count] = page->btr_high_water;
                        page->btr_count++;
                        page->btr_free_space -= needed;

                        sep_idx++;
                    }

                    // Update parent pointer of this child page
                    void *child_ptr = nullptr;
                    status = pinIndexPage(current_level[child_idx], &child_ptr, ctx, bulk_strategy);
                    if (status == Status::OK)
                    {
                        auto *child_page = reinterpret_cast<SBBTreePage *>(child_ptr);
                        child_page->btr_parent_page = internal_page_num;
                        unpinIndexPage(current_level[child_idx], true, ctx);
                    }

                    child_idx++;
                    page_children++;
                }

                // Set rightmost child pointer - the last child added
                // This is the child to the RIGHT of all separator keys on this page
                if (page_children > 0)
                {
                    page->btr_rightmost_child = current_level[child_idx - 1];
                }

                // Link to previous internal page
                if (!next_level.empty())
                {
                    page->btr_left_sibling = next_level.back();

                    void *prev_page_ptr = nullptr;
                    status = pinIndexPage(next_level.back(), &prev_page_ptr, ctx, bulk_strategy);
                    if (status == Status::OK)
                    {
                        auto *prev_page = reinterpret_cast<SBBTreePage *>(prev_page_ptr);
                        prev_page->btr_right_sibling = internal_page_num;
                        unpinIndexPage(next_level.back(), true, ctx);
                    }
                }

                next_level.push_back(internal_page_num);
                unpinIndexPage(internal_page_num, true, ctx);
            }

            // Mark last internal page as rightmost
            if (!next_level.empty())
            {
                void *last_page_ptr = nullptr;
                Status status = pinIndexPage(next_level.back(), &last_page_ptr, ctx, bulk_strategy);
                if (status == Status::OK)
                {
                    auto *last_page = reinterpret_cast<SBBTreePage *>(last_page_ptr);
                    last_page->btr_flags |= static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
                    unpinIndexPage(next_level.back(), true, ctx);
                }
            }

            current_level = next_level;
            current_separators = next_separators;
            level++;
        }
        stats.internal_build_us += elapsed_micros(internal_build_start);

        // ========================================================================
        // Phase 4: Set the root page
        // ========================================================================

        if (!current_level.empty())
        {
            const auto root_finalize_start = std::chrono::steady_clock::now();
            void *root_page_ptr = nullptr;
            Status status = pinIndexPage(current_level[0], &root_page_ptr, ctx, bulk_strategy);
            if (status == Status::OK)
            {
                auto *root_page = reinterpret_cast<SBBTreePage *>(root_page_ptr);
                root_page->btr_flags |= static_cast<uint16_t>(BTreeFlags::ROOT);
                root_page->btr_parent_page = 0; // Root has no parent
                unpinIndexPage(current_level[0], true, ctx);
            }
            index_info_.idx_root_page = current_level[0];
            index_info_.idx_height = level;
            index_info_.idx_page_count = leaf_pages.size() + (current_level.size() > 1 ? current_level.size() : 0);
            index_info_.idx_tuple_count = entries.size();
            status = persistRootGPID(index_info_.idx_root_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            updateRightmostLeafHint(leaf_pages.back(), entries.back().first);
            if (prior_root_structurally_empty &&
                prior_root_page != index_info_.idx_root_page)
            {
                ErrorContext reclaim_ctx;
                Status reclaim_status =
                    page_manager->freePageGlobal(prior_root_gpid, &reclaim_ctx);
                if (reclaim_status != Status::OK)
                {
                    LOG_WARNING(STORAGE,
                                "Failed to reclaim prior empty B-tree root after bulk load: %s",
                                reclaim_ctx.message.c_str());
                }
            }
            stats.root_finalize_us += elapsed_micros(root_finalize_start);
        }

        if (stats_out != nullptr)
        {
            *stats_out = stats;
        }
        return Status::OK;
    }

    Status BTree::attachBloomFilter(const BloomFilterConfig &config,
                                    uint64_t estimated_keys,
                                    ErrorContext *ctx)
    {
        if (bloom_filter_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Bloom filter already attached");
            return Status::INVALID_ARGUMENT;
        }

        GPID meta_gpid = 0;
        Status status = BloomFilter::create(db_, index_info_.idx_uuid, config, estimated_keys,
                                           index_info_.idx_tablespace_id, &meta_gpid, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        bloom_filter_ = BloomFilter::open(db_, meta_gpid, ctx);
        if (!bloom_filter_)
        {
            return Status::IO_ERROR;
        }
        bloom_filter_->setTargetFpr(config.target_fpr);

        return rebuildBloomFilter(ctx);
    }

    Status BTree::loadBloomFilter(GPID meta_gpid, double target_fpr, ErrorContext *ctx)
    {
        if (bloom_filter_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Bloom filter already attached");
            return Status::INVALID_ARGUMENT;
        }

        bloom_filter_ = BloomFilter::open(db_, meta_gpid, ctx);
        if (!bloom_filter_)
        {
            return Status::IO_ERROR;
        }

        bloom_filter_->setTargetFpr(target_fpr);
        return Status::OK;
    }

    Status BTree::detachBloomFilter(ErrorContext *ctx)
    {
        if (!bloom_filter_)
        {
            return Status::OK;
        }

        Status status = bloom_filter_->drop(ctx);
        bloom_filter_.reset();
        return status;
    }

    Status BTree::rebuildBloomFilter(ErrorContext *ctx)
    {
        if (!bloom_filter_)
        {
            return Status::OK;
        }

        Status status = bloom_filter_->clear(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        TransactionManager *txn_mgr = db_->transaction_manager();
        uint64_t oit = txn_mgr ? txn_mgr->getOldestXid() : 0;
        auto iter = rangeScan(nullptr, nullptr, oit, true, true, ctx);
        if (!iter)
        {
            return Status::IO_ERROR;
        }

        std::vector<uint8_t> key;
        TID tid;
        while (iter->hasNext())
        {
            status = iter->next(&key, &tid, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            status = bloom_filter_->insert(key.data(), key.size(), ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        return Status::OK;
    }

    // STOR-L3: Recursively check and merge underutilized parent pages
    // This maintains B-tree balance by propagating merges up the tree
    void BTree::checkAndMergeParentRecursive(uint64_t page_num, GcCompactionStats &stats,
                                             ErrorContext *ctx)
    {
        BufferPool *bp = db_->buffer_pool();
        if (!bp)
        {
            return;
        }

        // Pin the page to check its fill ratio
        void *page_data_ptr = nullptr;
        Status status = pinIndexPage(page_num, &page_data_ptr, ctx);
        if (status != Status::OK)
        {
            return;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);

        // Don't merge root pages
        if ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::ROOT)) != 0)
        {
            unpinIndexPage(page_num, false, ctx);
            return;
        }

        // Calculate fill ratio - only consider merging if below 50%
        uint32_t page_size = page->btr_header.page_size;
        uint32_t used_space = page_size - page->btr_free_space - sizeof(SBBTreePage);
        uint32_t available_space = page_size - sizeof(SBBTreePage);
        uint32_t fill_percent = (used_space * 100) / available_space;

        if (fill_percent >= 50)
        {
            // Page is sufficiently full, no merge needed
            unpinIndexPage(page_num, false, ctx);
            return;
        }

        // Get sibling information
        uint64_t left_sibling = page->btr_left_sibling;
        uint64_t right_sibling = page->btr_right_sibling;
        uint64_t parent_page = page->btr_parent_page;

        unpinIndexPage(page_num, false, ctx);

        // Try to merge with right sibling first (more common in B-tree operations)
        if (right_sibling != 0)
        {
            void *right_data_ptr = nullptr;
            status = pinIndexPage(right_sibling, &right_data_ptr, ctx);
            if (status == Status::OK)
            {
                void *left_data_ptr = nullptr;
                status = pinIndexPage(page_num, &left_data_ptr, ctx);
                if (status == Status::OK)
                {
                    auto *left_page_hdr = reinterpret_cast<SBBTreePage *>(left_data_ptr);
                    auto *right_page_hdr = reinterpret_cast<SBBTreePage *>(right_data_ptr);

                    bool should_merge = shouldMergePages(left_page_hdr, right_page_hdr);

                    unpinIndexPage(page_num, false, ctx);
                    unpinIndexPage(right_sibling, false, ctx);

                    if (should_merge)
                    {
                        status = mergePages(page_num, right_sibling, stats, ctx);
                        // mergePages will recursively call checkAndMergeParentRecursive
                        // for the parent, so we can return here
                        return;
                    }
                }
                else
                {
                    unpinIndexPage(right_sibling, false, ctx);
                }
            }
        }

        // Try to merge with left sibling if right merge didn't happen
        if (left_sibling != 0)
        {
            void *left_data_ptr = nullptr;
            status = pinIndexPage(left_sibling, &left_data_ptr, ctx);
            if (status == Status::OK)
            {
                void *right_data_ptr = nullptr;
                status = pinIndexPage(page_num, &right_data_ptr, ctx);
                if (status == Status::OK)
                {
                    auto *left_page_hdr = reinterpret_cast<SBBTreePage *>(left_data_ptr);
                    auto *right_page_hdr = reinterpret_cast<SBBTreePage *>(right_data_ptr);

                    bool should_merge = shouldMergePages(left_page_hdr, right_page_hdr);

                    unpinIndexPage(left_sibling, false, ctx);
                    unpinIndexPage(page_num, false, ctx);

                    if (should_merge)
                    {
                        status = mergePages(left_sibling, page_num, stats, ctx);
                        // mergePages will recursively call checkAndMergeParentRecursive
                        // for the parent, so we can return here
                        return;
                    }
                }
                else
                {
                    unpinIndexPage(left_sibling, false, ctx);
                }
            }
        }

        // If no merge was possible but we have a parent, check if we should
        // propagate the check upward anyway (for cascading underutilization)
        // This is a conservative approach - only propagate if we couldn't merge
        // but the parent might still benefit from checking
        (void)parent_page; // Parent will be checked when its children merge
    }

} // namespace scratchbird::core
