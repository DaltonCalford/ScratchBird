/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// ScratchBird Bitmap Index - Header
// Optimized for low-cardinality columns using Roaring Bitmaps
// Supports fast AND/OR/NOT/XOR operations on multiple conditions

#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/gpid.h"
#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace scratchbird
{
    namespace core
    {
        // Container types for Roaring Bitmap
        enum class ContainerType : uint8_t
        {
            ARRAY = 0,  // Sparse: sorted array of uint16_t (up to 4096 values)
            BITSET = 1, // Dense: page-size-dependent bitset (>4096 values)
                        // 8KB page: 65,536 bits, 16KB: 131,072 bits, etc.
            RUN = 2     // Run-length encoded (future optimization)
        };

        // TASK-CRITICAL-2: MGA-compliant bitmap entry with visibility tracking
        // Per MGA_RULES.md: Store xmin/xmax with each bitmap entry for TIP-based visibility
        // Firebird MGA: Enables index-level visibility checks without heap access
        struct VersionedBitmapEntry
        {
            uint16_t tid_low;   // Low 16 bits of TID (high bits from container key)
            uint64_t xmin;      // Transaction that inserted this entry
            uint64_t xmax;      // Transaction that deleted this entry (0 = still visible)

            VersionedBitmapEntry() : tid_low(0), xmin(0), xmax(0) {}
            VersionedBitmapEntry(uint16_t tid, uint64_t min_xid, uint64_t max_xid = 0)
                : tid_low(tid), xmin(min_xid), xmax(max_xid) {}

            // Firebird MGA visibility check using TIP
            // Per MGA_RULES.md Rule 3: Use TIP-based visibility, NOT snapshots
            bool isVisible(uint64_t current_xid, class TransactionManager *txn_mgr) const;
        };

        // Note: Actual size is 24 bytes due to alignment padding (2 bytes padding after tid_low)
        static_assert(sizeof(VersionedBitmapEntry) <= 32,
                      "VersionedBitmapEntry should fit in 32 bytes");

        /**
         * Page-Size-Based Bitmap Settings
         *
         * Dynamic calculation of bitmap page sizes based on database page size.
         * Provides optimal capacity for all supported page sizes (8KB-128KB).
         *
         * Benefits:
         * - 16KB pages: 2× bitset capacity (131,072 bits)
         * - 32KB pages: 4× bitset capacity (262,144 bits)
         * - 64KB pages: 8× bitset capacity (524,288 bits)
         * - 128KB pages: 16× bitset capacity (1,048,576 bits)
         */
        namespace BitmapSettings
        {
            /**
             * Get bitmap meta page size (equals database page size)
             */
            inline uint32_t getMetaPageSize(uint32_t page_size)
            {
                return page_size;
            }

            /**
             * Get roaring container page size (equals database page size)
             */
            inline uint32_t getContainerPageSize(uint32_t page_size)
            {
                return page_size;
            }

            /**
             * Get bitset container size in bytes
             *
             * For bitset containers, we use the full page size minus the
             * container header to maximize bit density.
             *
             * Examples:
             * - 8KB page:   8,192 bytes = 65,536 bits
             * - 16KB page:  16,384 bytes = 131,072 bits
             * - 32KB page:  32,768 bytes = 262,144 bits
             * - 64KB page:  65,536 bytes = 524,288 bits
             * - 128KB page: 131,072 bytes = 1,048,576 bits
             */
            inline uint32_t getBitsetContainerSize(uint32_t page_size)
            {
                // Reserve space for SBRoaringContainerPage header
                constexpr uint32_t header_size = sizeof(PageHeader) + 16; // ~80 bytes
                return page_size - header_size;
            }

            /**
             * Get number of uint64_t elements in bitset container
             */
            inline uint32_t getBitsetElementCount(uint32_t page_size)
            {
                return getBitsetContainerSize(page_size) / sizeof(uint64_t);
            }

            /**
             * Get maximum bits in bitset container
             */
            inline uint32_t getBitsetMaxBits(uint32_t page_size)
            {
                return getBitsetContainerSize(page_size) * 8;
            }
        } // namespace BitmapSettings

        // Bitmap index meta page (page-size dependent, minimum 8KB)
        struct SBBitmapIndexMetaPage
        {
            PageHeader bmp_header;                    // Standard page header
            UuidV7Bytes bmp_index_uuid;               // Index UUID
            uint32_t bmp_num_distinct_values;         // Number of distinct values
            uint32_t bmp_total_tuples;                // Total tuples indexed
            uint32_t bmp_dictionary_page;             // First dictionary page
            uint32_t bmp_reserved[59];                // Reserved for future use
        };

        // Static assert uses minimum page size (8KB); actual size determined by db->page_size()
        static_assert(sizeof(SBBitmapIndexMetaPage) <= 8192,
                      "SBBitmapIndexMetaPage must fit in minimum page size (8KB)");

        // Dictionary entry mapping value -> bitmap
        struct BitmapDictionaryEntry
        {
            uint64_t value_hash;           // Hash of the indexed value
            uint32_t bitmap_root_page;     // Root page of Roaring Bitmap
            uint32_t cardinality;          // Number of tuples with this value
            uint16_t value_length;         // Length of value data
            uint16_t reserved;
            // Followed by value_data (variable length)
        };

        // Dictionary page storing value -> bitmap mappings
        struct SBBitmapDictionaryPage
        {
            PageHeader bmp_dict_header;
            uint32_t bmp_dict_next_page;   // Next dictionary page (linked list)
            uint16_t bmp_dict_count;       // Number of entries in this page
            uint16_t bmp_dict_free_offset; // Offset to free space
            // Followed by BitmapDictionaryEntry array
        };

        // Static assert uses minimum page size (8KB); actual size determined by db->page_size()
        static_assert(sizeof(SBBitmapDictionaryPage) <= 8192,
                      "SBBitmapDictionaryPage must fit in minimum page size (8KB)");

        // Container pointer in Roaring Bitmap root
        struct ContainerPointer
        {
            uint32_t page_number;          // Page containing container data
            uint16_t num_values;           // Number of set bits
            ContainerType type;            // Container type
            uint8_t reserved;
        };

        static_assert(sizeof(ContainerPointer) == 8,
                      "ContainerPointer must be 8 bytes");

        // Roaring Bitmap root page (65536 container pointers)
        // This represents the top-level structure for one bitmap
        struct SBRoaringBitmapRootPage
        {
            PageHeader rbr_header;
            uint32_t rbr_num_containers;   // Number of non-empty containers
            uint32_t rbr_total_cardinality; // Total number of set bits
            // Followed by ContainerPointer array indexed by high 16 bits
            // ContainerPointer rbr_pointers[65536] would be 512KB
            // So we'll use a sparse representation instead
        };

        // Sparse container index (maps high bits -> container pointer)
        struct SparseContainerIndex
        {
            uint16_t key;                  // High 16 bits
            uint16_t reserved;
            ContainerPointer pointer;
        };

        // Roaring container page (array or bitset)
        struct SBRoaringContainerPage
        {
            PageHeader rcp_header;
            ContainerType rcp_type;
            uint16_t rcp_num_values;       // Number of values in container
            uint16_t rcp_capacity;         // Capacity (for array type)
            uint8_t rcp_reserved[3];
            // Followed by container data:
            // - ARRAY: sorted array of uint16_t values
            // - BITSET: page-size-dependent (use BitmapSettings::getBitsetContainerSize())
            //           8KB page:   8,192 bytes (65,536 bits)
            //           16KB page:  16,384 bytes (131,072 bits)
            //           32KB page:  32,768 bytes (262,144 bits)
            //           64KB page:  65,536 bytes (524,288 bits)
            //           128KB page: 131,072 bytes (1,048,576 bits)
            // - RUN: array of [start, length] pairs
        };

        // Static assert uses minimum page size (8KB); actual size determined by db->page_size()
        static_assert(sizeof(SBRoaringContainerPage) <= 8192,
                      "SBRoaringContainerPage must fit in minimum page size (8KB)");

        // Forward declarations
        class RoaringBitmap;
        class RoaringBitmapIterator;
        class BitmapIndexScanner;

        // Main Bitmap Index class
        // PHASE 2 TASK 2.5: Implements IndexGCInterface for garbage collection
        class BitmapIndex : public IndexGCInterface
        {
        public:
            // Constructor
            BitmapIndex(Database *db, const UuidV7Bytes &index_uuid, GPID meta_gpid);

            // Create a new bitmap index
            static Status create(
                Database *db,
                const UuidV7Bytes &index_uuid,
                GPID meta_gpid,
                ErrorContext *ctx = nullptr);
            static Status create(
                Database *db,
                const UuidV7Bytes &index_uuid,
                uint32_t *meta_page_out,
                ErrorContext *ctx = nullptr);

            // Open an existing bitmap index
            static std::unique_ptr<BitmapIndex> open(
                Database *db,
                const UuidV7Bytes &index_uuid,
                GPID meta_gpid,
                ErrorContext *ctx = nullptr);

            // Destructor
            ~BitmapIndex();

            // Insert a tuple into the bitmap for a specific value
            // PHASE 1.5 TASK 1.5.2c: Migrated to TID struct API
            Status insert(
                const void *value_data,
                size_t value_len,
                const TID &tid,
                ErrorContext *ctx = nullptr);

            // Remove a tuple from all bitmaps
            // PHASE 1.5 TASK 1.5.2c: Migrated to TID struct API
            Status remove(
                const TID &tid,
                ErrorContext *ctx = nullptr);

            // Find all tuple IDs matching a value
            // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            Status find(
                const void *value_data,
                size_t value_len,
                uint64_t current_xid,
                std::vector<TID>* results,
                ErrorContext *ctx = nullptr);

            // Logical operations on bitmaps
            // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            std::vector<TID> findAnd(
                const std::vector<const void *> &values,
                const std::vector<size_t> &value_lens,
                uint64_t current_xid,
                ErrorContext *ctx = nullptr);

            std::vector<TID> findOr(
                const std::vector<const void *> &values,
                const std::vector<size_t> &value_lens,
                uint64_t current_xid,
                ErrorContext *ctx = nullptr);

            std::vector<TID> findNot(
                const void *value_data,
                size_t value_len,
                uint64_t current_xid,
                ErrorContext *ctx = nullptr);

            // Scan operation - iterate over TIDs matching value(s)
            // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            // Returns iterator for streaming results instead of materializing all TIDs
            std::unique_ptr<BitmapIndexScanner> scan(
                const void *value_data,
                size_t value_len,
                uint64_t current_xid,
                ErrorContext *ctx = nullptr);

            // Scan multiple values with OR logic
            std::unique_ptr<BitmapIndexScanner> scanOr(
                const std::vector<const void *> &values,
                const std::vector<size_t> &value_lens,
                uint64_t current_xid,
                ErrorContext *ctx = nullptr);

            // Get statistics
            struct Statistics
            {
                uint32_t num_distinct_values;
                uint32_t total_tuples;
                uint32_t total_pages;
                uint32_t avg_cardinality;
                double compression_ratio;
            };

            Statistics getStatistics(ErrorContext *ctx = nullptr);

            // Get index UUID
            const UuidV7Bytes &getUuid() const { return index_uuid_; }

            // Firebird MGA: Visibility helper for post-filtering TIDs using TIP lookups
            // This helper filters a list of TIDs by checking heap tuple visibility
            // Note: Bitmap indexes still use post-filtering (20-40% overhead) because
            //       they don't store xmin/xmax in the bitmap entries themselves
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            std::vector<TID> filterTidsByVisibility(const std::vector<TID> &tids,
                                                     uint64_t current_xid,
                                                     ErrorContext *ctx);

            // PHASE 2 TASK 2.5: IndexGCInterface implementation
            // PHASE 1.5 TASK 1.5.2c: Migrated to TID struct API
            // Remove index entries pointing to dead tuples
            // Called by garbage collector after heap sweep identifies dead TIDs
            Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

            // Update TIDs after tablespace migration (GPID remap)
            Status updateTIDsAfterMigration(
                const std::unordered_map<TID, TID> &tid_mapping,
                uint64_t *tids_updated_out = nullptr,
                uint64_t *pages_modified_out = nullptr,
                ErrorContext *ctx = nullptr);

            // Get index type name for logging
            const char *indexTypeName() const override
            {
                return "Bitmap";
            }

        private:
            // Helper methods
            Status loadMetaPage(ErrorContext *ctx);

            uint32_t findDictionaryEntry(
                const void *value_data,
                size_t value_len,
                uint32_t *bitmap_root_out,
                ErrorContext *ctx);

            uint32_t createDictionaryEntry(
                const void *value_data,
                size_t value_len,
                ErrorContext *ctx);

            std::shared_ptr<RoaringBitmap> loadBitmap(
                uint32_t bitmap_root_page,
                ErrorContext *ctx);

            uint64_t hashValue(const void *data, size_t len) const;

            // Member variables
            Database *db_;
            BufferPool *buffer_pool_;
            UuidV7Bytes index_uuid_;
            uint32_t meta_page_;
            uint16_t tablespace_id_ = 0;

            // Cached meta page data
            uint32_t num_distinct_values_;
            uint32_t total_tuples_;
            uint32_t dictionary_page_;
            std::unordered_map<uint32_t, std::shared_ptr<RoaringBitmap>> bitmap_cache_;

            Status pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx = nullptr);
            Status unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx = nullptr);
            GPID indexGPID(uint64_t page_num) const;
        };

        // Roaring Bitmap implementation
        class RoaringBitmap
        {
        public:
            RoaringBitmap(Database *db, GPID root_gpid);
            ~RoaringBitmap();

            // TASK-CRITICAL-2: MGA-compliant add with xmin tracking
            // Add a value to the bitmap with insert transaction ID
            // Firebird MGA: Per MGA_RULES.md Rule 6 - store xmin with each entry
            Status add(const TID &tid, uint64_t xmin, ErrorContext *ctx = nullptr);

            // TASK-CRITICAL-2: MGA-compliant remove with xmax marking (logical deletion)
            // Mark value as deleted by setting xmax (Firebird MGA: NO physical removal)
            // Per MGA_RULES.md Rule 5: Use back-versioning with xmax tombstones
            Status remove(const TID &tid, uint64_t xmax, ErrorContext *ctx = nullptr);

            // Physically remove a value (used by GC to purge dead entries)
            Status removePhysical(const TID &tid, ErrorContext *ctx = nullptr);

            // Check if value exists
            bool contains(const TID &tid, ErrorContext *ctx = nullptr);

            // Get all values as a sorted vector (ignores visibility)
            std::vector<TID> toArray(ErrorContext *ctx = nullptr);

            // TASK-CRITICAL-2: Get all versioned entries for visibility filtering
            // Returns entries with xmin/xmax for TIP-based visibility checks
            // Firebird MGA: Per MGA_RULES.md Rule 3 - use TIP for visibility
            std::vector<std::pair<TID, VersionedBitmapEntry>> toVersionedArray(ErrorContext *ctx = nullptr);

            // TASK-CRITICAL-2: Get visible entries only (MGA-compliant filtering)
            // Filters entries using TIP-based visibility at index level (no heap access)
            // Firebird MGA: Per MGA_RULES.md Rule 11 - use TransactionId, NOT Snapshot
            std::vector<TID> toVisibleArray(uint64_t current_xid,
                                                  class TransactionManager *txn_mgr,
                                                  ErrorContext *ctx = nullptr);

            // Cardinality (number of set bits, includes invisible entries)
            uint64_t cardinality() const { return cardinality_; }

            // TASK-CRITICAL-2: Visible cardinality (only entries visible to current_xid)
            uint64_t visibleCardinality(uint64_t current_xid,
                                       class TransactionManager *txn_mgr,
                                       ErrorContext *ctx = nullptr) const;

            // Update TIDs after tablespace migration (GPID remap)
            Status updateTIDsAfterMigration(
                const std::unordered_map<TID, TID> &tid_mapping,
                uint64_t *tids_updated_out = nullptr,
                uint64_t *pages_modified_out = nullptr,
                ErrorContext *ctx = nullptr);

            // Logical operations (static methods)
            static std::unique_ptr<RoaringBitmap> bitwiseAnd(
                const RoaringBitmap &lhs,
                const RoaringBitmap &rhs,
                ErrorContext *ctx = nullptr);

            static std::unique_ptr<RoaringBitmap> bitwiseOr(
                const RoaringBitmap &lhs,
                const RoaringBitmap &rhs,
                ErrorContext *ctx = nullptr);

            static std::unique_ptr<RoaringBitmap> bitwiseNot(
                const RoaringBitmap &bitmap,
                uint64_t universe_size,
                ErrorContext *ctx = nullptr);

        private:
            friend class RoaringBitmapIterator;

            struct Container
            {
                uint64_t key;              // Heap GPID for this container
                ContainerType type;
                uint16_t num_values;
                uint32_t page_number;

                // TASK-CRITICAL-2: Store versioned entries for MGA compliance
                // Firebird MGA: Each entry has xmin/xmax for TIP-based visibility
                std::vector<VersionedBitmapEntry> array_data_versioned;  // For ARRAY containers
                std::vector<uint64_t> bitset_data;  // For BITSET containers (page-size dependent)
                                                     // 8KB:   1,024 uint64_t = 65,536 bits
                                                     // 16KB:  2,048 uint64_t = 131,072 bits
                                                     // 32KB:  4,096 uint64_t = 262,144 bits
                                                     // 64KB:  8,192 uint64_t = 524,288 bits
                                                     // 128KB: 16,384 uint64_t = 1,048,576 bits
                std::unordered_map<uint16_t, VersionedBitmapEntry> bitset_versions; // Bitset version info
            };

            Status loadContainer(uint64_t key, Container *container_out, ErrorContext *ctx);
            Status saveContainer(const Container &container, ErrorContext *ctx);
            Container *findOrCreateContainer(uint64_t key, ErrorContext *ctx);

            static void containerAnd(const Container &lhs, const Container &rhs, Container *result);
            static void containerOr(const Container &lhs, const Container &rhs, Container *result);
            static void containerNot(const Container &container, Container *result);

            Database *db_;
            BufferPool *buffer_pool_;
            uint32_t root_page_;
            uint16_t tablespace_id_;
            uint64_t cardinality_; // 64-bit for large indexes

            std::vector<Container> containers_; // In-memory container cache

            Status pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx = nullptr) const;
            Status unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx = nullptr) const;
            GPID indexGPID(uint64_t page_num) const;
        };

        // Iterator for scanning a Roaring Bitmap
        class RoaringBitmapIterator
        {
        public:
            RoaringBitmapIterator(const RoaringBitmap &bitmap);

            bool hasNext() const;
            TID next(); // Returns TID
            void reset();

        private:
            const RoaringBitmap &bitmap_;
            size_t container_index_;
            size_t value_index_;
        };

        /**
         * BitmapIndexScanner - Iterator for bitmap index scans
         *
         * Provides streaming access to TIDs matching bitmap query conditions.
         * Uses TIP-based visibility filtering for Firebird MGA compliance.
         */
        class BitmapIndexScanner
        {
        public:
            // Firebird MGA: Uses TransactionId for TIP-based visibility (NOT Snapshot*)
            BitmapIndexScanner(BitmapIndex *index,
                              std::shared_ptr<RoaringBitmap> bitmap,
                              uint64_t current_xid,
                              Database *db);
            ~BitmapIndexScanner();

            // Iterator operations
            bool hasNext();
            Status next(TID *tid_out, ErrorContext *ctx = nullptr);

            // Get statistics
            uint64_t getScannedCount() const { return scanned_count_; }
            uint64_t getReturnedCount() const { return returned_count_; }

        private:
            BitmapIndex *index_;
            Database *db_;
            std::shared_ptr<RoaringBitmap> bitmap_;
            std::unique_ptr<RoaringBitmapIterator> iterator_;

            // Firebird MGA: Transaction ID for TIP-based visibility filtering
            uint64_t current_xid_;

            // Statistics
            uint64_t scanned_count_;    // Total TIDs examined
            uint64_t returned_count_;   // TIDs returned (after visibility filtering)
        };

    } // namespace core
} // namespace scratchbird
