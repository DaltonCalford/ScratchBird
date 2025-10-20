// ScratchBird Bitmap Index - Header
// Optimized for low-cardinality columns using Roaring Bitmaps
// Supports fast AND/OR/NOT/XOR operations on multiple conditions

#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/index_gc_interface.h"
#include <vector>
#include <cstdint>
#include <memory>

namespace scratchbird
{
    namespace core
    {
        // Container types for Roaring Bitmap
        enum class ContainerType : uint8_t
        {
            ARRAY = 0,  // Sparse: sorted array of uint16_t (up to 4096 values)
            BITSET = 1, // Dense: 8KB bitset (>4096 values)
            RUN = 2     // Run-length encoded (future optimization)
        };

        // Bitmap index meta page (8192 bytes)
        struct SBBitmapIndexMetaPage
        {
            PageHeader bmp_header;                    // Standard page header
            UuidV7Bytes bmp_index_uuid;               // Index UUID
            uint32_t bmp_num_distinct_values;         // Number of distinct values
            uint32_t bmp_total_tuples;                // Total tuples indexed
            uint32_t bmp_dictionary_page;             // First dictionary page
            uint32_t bmp_reserved[59];                // Reserved for future use
        };

        static_assert(sizeof(SBBitmapIndexMetaPage) <= 8192,
                      "SBBitmapIndexMetaPage must fit in one page");

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

        static_assert(sizeof(SBBitmapDictionaryPage) <= 8192,
                      "SBBitmapDictionaryPage must fit in one page");

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
            // - BITSET: 8192 bytes (65536 bits)
            // - RUN: array of [start, length] pairs
        };

        static_assert(sizeof(SBRoaringContainerPage) <= 8192,
                      "SBRoaringContainerPage must fit in one page");

        // Forward declarations
        class RoaringBitmap;
        class RoaringBitmapIterator;

        // Main Bitmap Index class
        // PHASE 2 TASK 2.5: Implements IndexGCInterface for garbage collection
        class BitmapIndex : public IndexGCInterface
        {
        public:
            // Constructor
            BitmapIndex(Database *db, const UuidV7Bytes &index_uuid, uint32_t meta_page);

            // Create a new bitmap index
            static Status create(
                Database *db,
                const UuidV7Bytes &index_uuid,
                uint32_t *meta_page_out,
                ErrorContext *ctx = nullptr);

            // Open an existing bitmap index
            static std::unique_ptr<BitmapIndex> open(
                Database *db,
                const UuidV7Bytes &index_uuid,
                uint32_t meta_page,
                ErrorContext *ctx = nullptr);

            // Destructor
            ~BitmapIndex();

            // Insert a tuple into the bitmap for a specific value
            Status insert(
                const void *value_data,
                size_t value_len,
                uint64_t tuple_id,
                ErrorContext *ctx = nullptr);

            // Remove a tuple from all bitmaps
            Status remove(
                uint64_t tuple_id,
                ErrorContext *ctx = nullptr);

            // Find all tuple IDs matching a value
            // PHASE 1 TASK 1.1.4: Added Snapshot parameter for MVCC visibility filtering
            std::vector<uint64_t> find(
                const void *value_data,
                size_t value_len,
                struct Snapshot *snapshot,
                ErrorContext *ctx = nullptr);

            // Logical operations on bitmaps
            // PHASE 1 TASK 1.1.4: Added Snapshot parameter for MVCC visibility filtering
            std::vector<uint64_t> findAnd(
                const std::vector<const void *> &values,
                const std::vector<size_t> &value_lens,
                struct Snapshot *snapshot,
                ErrorContext *ctx = nullptr);

            std::vector<uint64_t> findOr(
                const std::vector<const void *> &values,
                const std::vector<size_t> &value_lens,
                struct Snapshot *snapshot,
                ErrorContext *ctx = nullptr);

            std::vector<uint64_t> findNot(
                const void *value_data,
                size_t value_len,
                struct Snapshot *snapshot,
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

            // PHASE 1 TASK 1.5: Visibility helper for post-filtering TIDs
            // Note: Snapshot is used as an incomplete type in method signatures
            // The actual type is TransactionManager::Snapshot, defined in transaction_manager.h
            // This helper filters a list of TIDs by checking heap tuple visibility
            std::vector<uint64_t> filterTidsByVisibility(const std::vector<uint64_t> &tids,
                                                          const struct Snapshot *snapshot,
                                                          ErrorContext *ctx);

            // PHASE 2 TASK 2.5: IndexGCInterface implementation
            // Remove index entries pointing to dead tuples
            // Called by garbage collector after heap sweep identifies dead TIDs
            Status removeDeadEntries(const std::vector<uint64_t> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

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

            std::unique_ptr<RoaringBitmap> loadBitmap(
                uint32_t bitmap_root_page,
                ErrorContext *ctx);

            uint64_t hashValue(const void *data, size_t len) const;

            // Member variables
            Database *db_;
            BufferPool *buffer_pool_;
            UuidV7Bytes index_uuid_;
            uint32_t meta_page_;

            // Cached meta page data
            uint32_t num_distinct_values_;
            uint32_t total_tuples_;
            uint32_t dictionary_page_;
        };

        // Roaring Bitmap implementation
        class RoaringBitmap
        {
        public:
            RoaringBitmap(Database *db, uint32_t root_page);
            ~RoaringBitmap();

            // Add a value to the bitmap
            Status add(uint32_t value, ErrorContext *ctx = nullptr);

            // Remove a value from the bitmap
            Status remove(uint32_t value, ErrorContext *ctx = nullptr);

            // Check if value exists
            bool contains(uint32_t value, ErrorContext *ctx = nullptr);

            // Get all values as a sorted vector
            std::vector<uint32_t> toArray(ErrorContext *ctx = nullptr);

            // Cardinality (number of set bits)
            uint32_t cardinality() const { return cardinality_; }

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
                uint32_t universe_size,
                ErrorContext *ctx = nullptr);

        private:
            friend class RoaringBitmapIterator;

            struct Container
            {
                uint16_t key;              // High 16 bits
                ContainerType type;
                uint16_t num_values;
                uint32_t page_number;
                std::vector<uint16_t> array_data;  // For ARRAY containers
                std::vector<uint64_t> bitset_data; // For BITSET containers (1024 uint64_t)
            };

            Status loadContainer(uint16_t key, Container *container_out, ErrorContext *ctx);
            Status saveContainer(const Container &container, ErrorContext *ctx);
            Container *findOrCreateContainer(uint16_t key, ErrorContext *ctx);

            static void containerAnd(const Container &lhs, const Container &rhs, Container *result);
            static void containerOr(const Container &lhs, const Container &rhs, Container *result);
            static void containerNot(const Container &container, Container *result);

            Database *db_;
            BufferPool *buffer_pool_;
            uint32_t root_page_;
            uint32_t cardinality_;

            std::vector<Container> containers_; // In-memory container cache
        };

        // Iterator for scanning a Roaring Bitmap
        class RoaringBitmapIterator
        {
        public:
            RoaringBitmapIterator(const RoaringBitmap &bitmap);

            bool hasNext() const;
            uint32_t next();
            void reset();

        private:
            const RoaringBitmap &bitmap_;
            size_t container_index_;
            size_t value_index_;
        };

    } // namespace core
} // namespace scratchbird
