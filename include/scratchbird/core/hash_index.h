#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/id.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace scratchbird
{
    namespace core
    {
        // Forward declarations
        class Database;
        class BufferPool;

        // Constants
        constexpr uint32_t INITIAL_GLOBAL_DEPTH = 4;     // 16 initial buckets
        constexpr uint32_t MAX_GLOBAL_DEPTH = 20;        // 1M max buckets
        constexpr uint32_t BUCKET_FILL_THRESHOLD = 90;   // Split at 90% full
        constexpr uint32_t MAX_OVERFLOW_CHAIN = 5;       // Force split after 5 overflow pages

        // Hash function ID
        constexpr uint32_t HASH_FUNC_MURMUR3 = 1;

        // ===== On-Disk Structures =====

        // Meta Page - Page 0 of hash index
        struct SBHashIndexMetaPage
        {
            PageHeader      hip_header;           // Standard page header (64 bytes)
            ID              hip_index_uuid;       // Index UUID (16 bytes)
            uint32_t        hip_hash_func_id;     // Hash function ID (4 bytes) - always HASH_FUNC_MURMUR3
            uint32_t        hip_global_depth;     // Global depth (4 bytes) - max 20
            uint64_t        hip_directory_page;   // First directory page number (8 bytes)
            uint64_t        hip_num_tuples;       // Total number of indexed tuples (8 bytes)
            uint64_t        hip_num_deleted;      // Number of deleted entries (8 bytes)
            uint8_t         hip_reserved[8104];   // Reserved for future use
        } __attribute__((packed));

        static_assert(sizeof(SBHashIndexMetaPage) == 8192, "Meta page must be exactly 8KB");

        // Directory Page - Maps hash values to bucket pages
        struct SBHashDirectoryPage
        {
            PageHeader      hdp_header;           // Standard page header (64 bytes)
            uint64_t        hdp_next_page;        // Next directory page (0 if last) (8 bytes)
            uint64_t        hdp_bucket_pointers[(8192 - 72) / 8]; // Bucket page numbers (1015 pointers)
        } __attribute__((packed));

        static_assert(sizeof(SBHashDirectoryPage) == 8192, "Directory page must be exactly 8KB");

        // Hash Entry - Stores hash and tuple ID
        struct HashEntry
        {
            uint64_t        he_key_hash;          // Full 64-bit hash of the key
            uint64_t        he_tuple_id;          // TupleId (page_id << 32 | item_id)
                                                  // Special value: 0 means deleted entry
        } __attribute__((packed));

        static_assert(sizeof(HashEntry) == 16, "HashEntry must be 16 bytes");

        // Bucket Page - Stores hash entries
        struct SBHashBucketPage
        {
            PageHeader      hbp_header;           // Standard page header (64 bytes)
            uint16_t        hbp_entry_count;      // Number of entries in this page (2 bytes)
            uint16_t        hbp_local_depth;      // Local depth of this bucket (2 bytes)
            uint32_t        hbp_deleted_count;    // Number of deleted entries (4 bytes)
            uint64_t        hbp_overflow_page;    // Next overflow page (0 if none) (8 bytes)
            uint8_t         hbp_reserved[12];     // Reserved for alignment (12 bytes)
            HashEntry       hbp_entries[(8192 - 92) / 16]; // Hash entries (506 entries)
        } __attribute__((packed));

        static_assert(sizeof(SBHashBucketPage) == 8192, "Bucket page must be exactly 8KB");

        // Maximum entries per bucket page
        constexpr uint16_t MAX_ENTRIES_PER_BUCKET = (8192 - 92) / 16;

        // ===== Hash Index Class =====

        class HashIndex
        {
        public:
            // Constructor - requires database and index UUID
            // The index must already exist (pages allocated)
            HashIndex(Database* db, const ID& index_uuid);

            // Create a new hash index
            // Allocates meta page, initial directory, and initial bucket pages
            static Status create(Database* db, const ID& index_uuid,
                               uint32_t* meta_page_out, ErrorContext* ctx = nullptr);

            // Open an existing hash index
            static std::unique_ptr<HashIndex> open(Database* db, const ID& index_uuid,
                                                   uint32_t meta_page, ErrorContext* ctx = nullptr);

            // Destructor
            ~HashIndex();

            // Insert a key-value pair
            // key_data: pointer to key data
            // key_len: length of key in bytes
            // tuple_id: tuple ID (page_id << 32 | item_id)
            Status insert(const void* key_data, size_t key_len, uint64_t tuple_id,
                         ErrorContext* ctx = nullptr);

            // Find all tuple IDs for a given key
            // Returns a vector of tuple IDs (may be empty if key not found)
            std::vector<uint64_t> find(const void* key_data, size_t key_len,
                                      ErrorContext* ctx = nullptr);

            // Remove a specific entry
            // Only removes the entry matching both key and tuple_id
            Status remove(const void* key_data, size_t key_len, uint64_t tuple_id,
                         ErrorContext* ctx = nullptr);

            // Vacuum the index - remove deleted entries and consolidate pages
            Status vacuum(ErrorContext* ctx = nullptr);

            // Get index statistics
            struct Statistics
            {
                uint64_t num_tuples;         // Total entries
                uint64_t num_deleted;        // Deleted entries
                uint32_t global_depth;       // Current global depth
                uint32_t num_buckets;        // Number of buckets (2^global_depth)
                uint32_t num_overflow_pages; // Number of overflow pages
                double   avg_entries_per_bucket; // Average entries per bucket
                double   load_factor;        // Percentage full
            };

            Statistics getStatistics(ErrorContext* ctx = nullptr);

            // Get index UUID
            const ID& getIndexUuid() const { return index_uuid_; }

            // Get meta page number
            uint32_t getMetaPage() const { return meta_page_; }

        private:
            Database*       db_;
            BufferPool*     buffer_pool_;
            ID              index_uuid_;
            uint32_t        meta_page_;

            // Helper methods
            uint32_t getDirectoryIndex(uint64_t hash, uint32_t global_depth);
            uint64_t findBucketPageForKey(uint64_t hash, ErrorContext* ctx);
            Status allocateBucketPage(uint32_t* page_num_out, ErrorContext* ctx);
            Status allocateOverflowPage(uint32_t* page_num_out, ErrorContext* ctx);
            Status splitBucket(uint32_t bucket_page, uint64_t hash, ErrorContext* ctx);
            Status expandDirectory(ErrorContext* ctx);
            bool bucketNeedsSplit(SBHashBucketPage* bucket);
            uint16_t countEntriesInBucket(uint32_t bucket_page, ErrorContext* ctx);
            Status redistributeEntries(SBHashBucketPage* old_bucket,
                                      SBHashBucketPage* new_bucket,
                                      uint32_t new_local_depth, ErrorContext* ctx);

            // No copy or move
            HashIndex(const HashIndex&) = delete;
            HashIndex& operator=(const HashIndex&) = delete;
            HashIndex(HashIndex&&) = delete;
            HashIndex& operator=(HashIndex&&) = delete;
        };

    } // namespace core
} // namespace scratchbird
