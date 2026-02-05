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

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/buffer_pool.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <shared_mutex>

namespace scratchbird
{
    namespace core
    {

        // Forward declarations
        class Database;
        class BufferPool;
        class PageManager;
        class TransactionManager; // For TIP-based visibility checks (Firebird MGA)
        struct ErrorContext;

        /**
         * BRIN (Block Range Index) - Space-efficient index for time-series data
         *
         * BRIN indexes store min/max summaries for ranges of blocks instead of
         * indexing every tuple. This provides 90%+ space savings vs B-Tree while
         * maintaining acceptable query performance for naturally ordered data.
         *
         * ## Use Cases
         * - Time-series data (logs, metrics, IoT sensor data)
         * - Chronologically ordered data (orders, events, transactions)
         * - Append-only workloads
         * - Large tables where index size is a concern
         *
         * ## Architecture (Firebird MGA with Stable TIDs)
         *
         * ```
         * BRIN Index Page
         * ┌─────────────────────────────────────────────┐
         * │ Range 1: Blocks 0-127                       │
         * │   min_value: 100, max_value: 5000           │
         * │   xmin: 42 (created by TX 42)               │
         * │   xmax: 0 (still valid)                     │
         * ├─────────────────────────────────────────────┤
         * │ Range 2: Blocks 128-255                     │
         * │   min_value: 5001, max_value: 10000         │
         * │   xmin: 42, xmax: 0                         │
         * └─────────────────────────────────────────────┘
         *         ↓ Query: WHERE value BETWEEN 200 AND 300
         *         ↓ Scan only Range 1 (min=100, max=5000)
         *         ↓ Skip Range 2 (min=5001 > 300)
         * ```
         *
         * ## MGA Compliance (Phase 6 - November 2025)
         *
         * - **xmin/xmax tracking**: Each range summary has xmin/xmax
         * - **TIP-based visibility**: TransactionId parameter in scan API (NOT snapshots)
         * - **Visibility filtering**: Via TIP lookups (Firebird MGA)
         * - **Garbage collection**: removeDeadEntries() for dead ranges
         * - **Stable TIDs**: Ranges reference stable block numbers
         *
         * ## Implementation Notes
         *
         * - Default range size: 128 blocks (configurable)
         * - Supports numeric and date/time types
         * - NOT suitable for frequently updated columns
         * - Best for append-mostly workloads
         */

        // BRIN page flags
        enum class BrinFlags : uint16_t
        {
            ROOT = 0x0001,        // Root page
            HAS_GARBAGE = 0x0002, // Page has deleted ranges
            COMPRESSED = 0x0004   // Ranges are compressed
        };

        // BRIN range flags
        enum class BrinRangeFlags : uint16_t
        {
            DELETED = 0x0001,       // Logically deleted (xmax set)
            NULL_MIN = 0x0002,      // Min value is NULL
            NULL_MAX = 0x0004,      // Max value is NULL
            ALL_NULL = 0x0008,      // All values in range are NULL
            HAS_NULLS = 0x0010,     // Range contains some NULL values
            SINGLE_VALUE = 0x0020   // All non-NULL values are identical
        };

#pragma pack(push, 1)
        /**
         * BRIN page structure
         *
         * Stores range summaries for consecutive block ranges.
         * Each range covers a fixed number of heap blocks (default 128).
         */
        struct SBBrinPage
        {
            // Standard page header
            PageHeader brin_header; // Standard ScratchBird page header

            // Index identification
            ID brin_index_uuid; // Index UUID v7
            ID brin_table_uuid; // Table this index belongs to

            // BRIN metadata
            uint16_t brin_flags;        // Page flags (see BrinFlags)
            uint16_t brin_count;        // Number of range summaries on page
            uint16_t brin_free_space;   // Free space in bytes
            uint16_t brin_range_size;   // Blocks per range (e.g., 128)
            uint32_t brin_first_block;  // First block number covered by this page
            uint32_t brin_last_block;   // Last block number covered by this page

            // Sibling navigation
            uint64_t brin_left_sibling;  // Left sibling page number
            uint64_t brin_right_sibling; // Right sibling page number

            // MGA compliance (Phase 4A.1)
            uint64_t brin_xmin; // Page creation transaction
            uint64_t brin_xmax; // Page deletion transaction (0 if active)
            uint64_t brin_lsn;  // Last LSN that modified this page

            // Statistics
            uint64_t brin_ranges_total;   // Total ranges in entire index
            uint64_t brin_ranges_deleted; // Deleted ranges (need GC compaction)

            uint8_t brin_padding[64]; // Reserved for future use

            // Range summaries follow immediately after header
            // Each range is a SBBrinRange structure
        };

        /**
         * BRIN range summary
         *
         * Stores min/max values for a range of blocks.
         * Variable-size structure (min/max values can be different sizes).
         */
        struct SBBrinRange
        {
            uint32_t brn_start_block; // First block in range
            uint32_t brn_end_block;   // Last block in range (inclusive)
            uint16_t brn_flags;       // Range flags (see BrinRangeFlags)
            uint16_t brn_min_len;     // Length of min_value in bytes
            uint16_t brn_max_len;     // Length of max_value in bytes

            // MGA compliance (Phase 4A.1)
            uint64_t brn_xmin; // Transaction that created this range
            uint64_t brn_xmax; // Transaction that deleted this range (0 if active)

            // Variable-length data follows:
            // - uint8_t min_value[brn_min_len]  // Minimum value in range
            // - uint8_t max_value[brn_max_len]  // Maximum value in range
            //
            // Access via:
            //   const uint8_t* get_min_value() const { return (uint8_t*)(this + 1); }
            //   const uint8_t* get_max_value() const { return get_min_value() + brn_min_len; }
        };

#pragma pack(pop)

        /**
         * BRIN index metadata (stored in catalog)
         */
        struct SBBrinIndex
        {
            ID idx_uuid;                        // Index UUID v7
            ID idx_table_uuid;                  // Table UUID
            std::vector<ID> idx_column_uuids;   // Indexed columns (usually 1)
            uint32_t idx_root_page;             // Root page number
            uint16_t idx_tablespace_id = 0;
            uint16_t idx_range_size;            // Blocks per range (default 128)
            uint64_t idx_total_ranges;          // Total number of ranges
            uint64_t idx_creation_xid;          // Transaction that created index
            uint8_t idx_value_type;             // Data type (from DataType enum)
        };

        /**
         * BRIN Index Implementation
         *
         * Implements IndexGCInterface for garbage collection integration.
         * Follows B-Tree MGA pattern for consistency.
         */
        class BrinIndex : public IndexGCInterface
        {
        public:
            BrinIndex(Database *db, SBBrinIndex index_info);
            ~BrinIndex();

            // Static factory methods
            static Status create(Database *db,
                                 const UuidV7Bytes &index_uuid,
                                 const UuidV7Bytes &table_uuid,
                                 const std::vector<UuidV7Bytes> &column_uuids,
                                 uint8_t value_type,            // DataType enum value
                                 uint16_t range_size = 128,     // Blocks per range
                                 GPID root_gpid = 0,
                                 ErrorContext *ctx = nullptr);

            static Status create(Database *db,
                                 const UuidV7Bytes &index_uuid,
                                 const UuidV7Bytes &table_uuid,
                                 const std::vector<UuidV7Bytes> &column_uuids,
                                 uint8_t value_type,
                                 uint16_t range_size,
                                 uint16_t tablespace_id,
                                 uint32_t *root_page_out,
                                 ErrorContext *ctx = nullptr);

            // Legacy convenience overload (page-number based)
            static Status create(Database *db,
                                 const UuidV7Bytes &index_uuid,
                                 const UuidV7Bytes &table_uuid,
                                 const std::vector<UuidV7Bytes> &column_uuids,
                                 uint8_t value_type,
                                 uint16_t range_size,
                                 uint32_t *root_page_out,
                                 ErrorContext *ctx = nullptr);

            static std::unique_ptr<BrinIndex> open(Database *db,
                                                   const UuidV7Bytes &index_uuid,
                                                   GPID root_gpid,
                                                   ErrorContext *ctx = nullptr);

            // Legacy convenience overload (page-number based)
            static std::unique_ptr<BrinIndex> open(Database *db,
                                                   const UuidV7Bytes &index_uuid,
                                                   uint32_t root_page,
                                                   ErrorContext *ctx = nullptr);

            /**
             * Insert/update range summary for a value
             *
             * Updates the min/max for the range containing the specified block.
             * Creates new range if needed.
             *
             * @param value The indexed value (encoded as bytes)
             * @param block_number The heap block number where tuple resides
             * @param ctx Error context
             */
            Status insert(const std::vector<uint8_t> &value,
                          uint32_t block_number,
                          ErrorContext *ctx = nullptr);

            /**
             * Scan ranges and return block numbers that might contain matching values
             *
             * Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
             * Returns block numbers filtered by TIP visibility checks on range summaries.
             *
             * @param min_value Minimum value (nullptr for -infinity)
             * @param max_value Maximum value (nullptr for +infinity)
             * @param current_xid Current transaction ID for TIP-based visibility
             * @param block_numbers_out Output: block numbers to scan
             * @param ctx Error context
             * @return Status OK if successful
             *
             * Example:
             *   WHERE value BETWEEN 100 AND 200
             *   → Returns blocks from ranges where [min,max] overlaps [100,200]
             *
             * Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
             */
            Status scan(const std::vector<uint8_t> *min_value,
                        const std::vector<uint8_t> *max_value,
                        uint64_t current_xid,
                        std::vector<uint32_t> *block_numbers_out,
                        ErrorContext *ctx = nullptr);

            /**
             * Remove a value from range summary
             *
             * Updates min/max if necessary. Does NOT shrink ranges.
             * Full recalculation deferred to GC compaction.
             *
             * @param value The value to remove
             * @param block_number The block number
             * @param ctx Error context
             */
            Status remove(const std::vector<uint8_t> &value,
                          uint32_t block_number,
                          ErrorContext *ctx = nullptr);

            /**
             * GC compaction operations (ScratchBird MGA GC, not PostgreSQL VACUUM)
             */
            struct GcCompactionStats
            {
                uint64_t ranges_visited;
                uint64_t ranges_removed;
                uint64_t ranges_updated;
                uint64_t bytes_reclaimed;
            };

            Status gcCompact(GcCompactionStats *stats_out = nullptr,
                             ErrorContext *ctx = nullptr);

            /**
             * Phase 4A.1.4: Remove dead range summaries
             * PHASE 1.5 TASK 1.5.2e: Migrated to TID struct API
             *
             * Called by garbage collector after heap sweep identifies dead tuples.
             * Extracts block numbers from TIDs and removes/marks ranges as deleted.
             *
             * @param dead_tids Tuple IDs that are dead (block numbers extracted from TIDs)
             * @param entries_removed_out Output: number of ranges removed
             * @param pages_modified_out Output: number of pages modified
             * @param ctx Error context
             * @return Status OK if successful
             */
            Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

            /**
             * Update block-range references after table migration.
             *
             * Maps range start/end blocks using the provided old->new GPID mapping.
             */
            Status updateTIDsAfterMigration(const std::unordered_map<TID, TID> &tid_mapping,
                                            uint64_t *ranges_updated_out = nullptr,
                                            uint64_t *pages_modified_out = nullptr,
                                            ErrorContext *ctx = nullptr);

            /**
             * Get index type name for logging
             */
            const char *indexTypeName() const override
            {
                return "BRIN";
            }

            /**
             * Get statistics
             */
            struct BrinStats
            {
                uint64_t total_ranges;
                uint64_t deleted_ranges;
                uint64_t total_pages;
                uint64_t blocks_covered;
                double avg_range_selectivity; // Estimate of range pruning effectiveness
            };

            Status getStats(BrinStats *stats_out, ErrorContext *ctx = nullptr);

            /**
             * PHASE 5 TASK 5.3.4: Update Block Ranges After Tablespace Migration
             *
             * Traverses all BRIN pages and updates brn_start_block and brn_end_block fields
             * based on the provided page mapping (GPID -> GPID).
             *
             * **IMPORTANT**: BRIN stores BLOCK RANGES, not individual TIDs!
             * The mapping is from old GPID to new GPID (page-level, not tuple-level).
             *
             * @param page_mapping Map from old GPID (uint64_t) to new GPID (uint64_t)
             * @param ranges_updated_out Output: number of ranges updated
             * @param pages_modified_out Output: number of pages modified
             * @param ctx Error context
             * @return Status::OK if successful
             */
            Status updateBlockRangesAfterMigration(
                const std::unordered_map<uint64_t, uint64_t> &page_mapping,
                uint64_t *ranges_updated_out = nullptr,
                uint64_t *pages_modified_out = nullptr,
                ErrorContext *ctx = nullptr);

        private:
            Database *db_;
            SBBrinIndex index_info_;

            // Revmap: Maps range_start_block → page_number for O(1) lookup
            // Key: range starting block number (e.g., 0, 128, 256, ...)
            // Value: page number containing that range
            std::unordered_map<uint32_t, uint32_t> revmap_;
            mutable std::shared_mutex revmap_mutex_;

            // Helper methods

            /**
             * Find the page containing range summary for given block number
             */
            Status find_range_page(uint32_t block_number,
                                   uint64_t *page_num_out,
                                   bool write_lock,
                                   ErrorContext *ctx);

            /**
             * Check if value is within range [min, max]
             */
            bool value_in_range(const uint8_t *value, uint16_t value_len,
                                const uint8_t *min_val, uint16_t min_len,
                                const uint8_t *max_val, uint16_t max_len) const;

            /**
             * Compare two values based on data type
             * Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
             */
            int compare_values(const uint8_t *v1, uint16_t v1_len,
                               const uint8_t *v2, uint16_t v2_len) const;

            /**
             * Check if range summary is visible to current transaction
             * Firebird MGA: Uses TIP-based visibility checking (NOT snapshots)
             */
            bool is_range_visible(const SBBrinRange *range,
                                  uint64_t current_xid,
                                  ErrorContext *ctx) const;

            Status pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx = nullptr,
                                BufferPool::AccessStrategy strategy = BufferPool::AccessStrategy::Normal);
            Status unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx = nullptr);
            GPID indexGPID(uint64_t page_num) const;

            /**
             * Update range summary with new min/max values
             */
            Status update_range_summary(uint64_t page_num,
                                        SBBrinRange *range,
                                        const std::vector<uint8_t> &value,
                                        ErrorContext *ctx);

            /**
             * Split BRIN page if full
             */
            Status split_page(uint64_t page_num, ErrorContext *ctx);

            /**
             * Build/rebuild revmap by scanning all pages
             */
            Status build_revmap(ErrorContext *ctx);

            /**
             * Add range to revmap
             */
            void revmap_add(uint32_t range_start_block, uint32_t page_num);

            /**
             * Remove range from revmap
             */
            void revmap_remove(uint32_t range_start_block);

            /**
             * Lookup page number for a range
             * Returns 0 if not found (will fall back to linear scan)
             */
            uint32_t revmap_lookup(uint32_t range_start_block) const;
        };

    } // namespace core
} // namespace scratchbird
