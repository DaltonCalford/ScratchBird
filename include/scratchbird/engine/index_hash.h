#ifndef SCRATCHBIRD_ENGINE_INDEX_HASH_H
#define SCRATCHBIRD_ENGINE_INDEX_HASH_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_family.h"
#include "scratchbird/engine/ods.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    /**
     * Hash index tunables for performance optimization
     */
    struct HashIndexTunables {
        std::uint32_t initial_buckets{1024};
        double max_load_factor{0.75};
        std::uint32_t bucket_split_threshold{8};
        bool enable_extensible_hashing{true};
        std::uint32_t overflow_chain_limit{4};
    };

    /**
     * Hash directory entry for extensible hashing
     */
    struct HashDirEntry {
        std::uint32_t bucket_page{0};
        std::uint32_t local_depth{0};
        std::uint32_t entry_count{0};
    };

    /**
     * Hash bucket header structure
     */
    struct HashBucketHeader {
        std::uint16_t num_slots{0};
        std::uint16_t free_start{0};
        std::uint16_t dir_start{0};
        std::uint32_t local_depth{0};
        std::uint32_t overflow_page{0}; // Chain to overflow bucket if needed
        std::uint32_t split_image{0};   // For extensible hashing splits
    };

    /**
     * Hash index entry structure
     */
    struct HashEntry {
        std::uint32_t hash_value;
        std::string key;
        std::uint64_t row_id;
        std::string payload; // For INCLUDE columns
    };

    /**
     * Hash function interface for different key types
     */
    class HashFunction
    {
      public:
        virtual ~HashFunction() = default;
        virtual std::uint32_t hash(const std::string& key) const = 0;
        virtual std::string name() const = 0;
    };

    /**
     * Universal hash function for numeric and string keys
     */
    class UniversalHashFunction : public HashFunction
    {
      public:
        UniversalHashFunction(std::uint32_t a = 31, std::uint32_t b = 17);
        std::uint32_t hash(const std::string& key) const override;
        std::string name() const override
        {
            return "universal";
        }

      private:
        std::uint32_t a_, b_;
        static constexpr std::uint32_t LARGE_PRIME = 982451653U;
    };

    /**
     * FNV-1a hash function for string keys
     */
    class FNVHashFunction : public HashFunction
    {
      public:
        std::uint32_t hash(const std::string& key) const override;
        std::string name() const override
        {
            return "fnv1a";
        }

      private:
        static constexpr std::uint32_t FNV_OFFSET_BASIS = 2166136261U;
        static constexpr std::uint32_t FNV_PRIME = 16777619U;
    };

    /**
     * Hash index statistics for monitoring and optimization
     */
    struct HashIndexStats {
        std::uint32_t total_buckets{0};
        std::uint32_t total_entries{0};
        std::uint32_t overflow_buckets{0};
        std::uint32_t empty_buckets{0};
        std::uint32_t max_chain_length{0};
        double average_chain_length{0.0};
        double load_factor{0.0};
        std::uint32_t directory_size{0};
        std::uint32_t global_depth{0};
        std::string hash_function;
        std::vector<std::uint32_t> bucket_distribution;
    };

    /**
     * Hash Index implementation with extensible hashing
     */
    class HashIndex : public IndexFamily
    {
      public:
        HashIndex(FileMap fmap, std::uint32_t page_size, bool unique);
        ~HashIndex() override = default;

        void set_tunables(const HashIndexTunables& tunables)
        {
            tunables_ = tunables;
        }
        void set_hash_function(std::unique_ptr<HashFunction> hash_func)
        {
            hash_func_ = std::move(hash_func);
        }

        // IndexFamily interface
        bool insert(const std::string& key, std::uint64_t row_id, std::string& err) override;
        bool insert_with_payload(const std::string& key, std::uint64_t row_id,
                                 const std::string& payload, std::string& err) override;
        void search_equal(const std::string& key, std::vector<std::uint64_t>& out) const override;
        void search_equal_with_payload(
            const std::string& key,
            std::vector<std::pair<std::uint64_t, std::string>>& out) const override;
        void search_range(const std::string& lo, bool lo_incl, const std::string& hi, bool hi_incl,
                          std::vector<std::pair<std::string, std::uint64_t>>& out) const override;
        std::size_t erase_equal(const std::string& key, std::string& err) override;

        bool validate(std::string& error) const override;
        void rebuild_offline() override;
        std::string collect_statistics() const override;
        void compact_index() override;

        IndexMethod get_method() const override
        {
            return IndexMethod::Hash;
        }
        std::uint32_t root_page() const override
        {
            return directory_page_;
        }
        bool open_existing(std::uint32_t root_page) override;
        void create_empty() override;

        // Cost estimation
        double estimate_search_cost(const std::string& key) const override;
        double estimate_range_cost(const std::string& lo, const std::string& hi) const override;
        double estimate_maintenance_cost() const override;

        // Hash-specific operations
        void expand_directory();
        void split_bucket(std::uint32_t bucket_page);
        void merge_bucket(std::uint32_t bucket_page);
        HashIndexStats compute_statistics() const;

        // Bulk operations
        bool bulk_insert(const std::vector<HashEntry>& entries, std::string& err);
        void reorganize_for_distribution();

      private:
        // Page management
        std::vector<std::uint8_t> new_page_buffer(ods::PageType type, std::uint32_t page_no) const;
        void write_page(std::uint32_t page_no, const std::vector<std::uint8_t>& page);
        void read_page(std::uint32_t page_no, std::vector<std::uint8_t>& page) const;

        // Directory operations
        void write_directory();
        void read_directory();
        std::uint32_t get_bucket_for_hash(std::uint32_t hash_value) const;
        std::uint32_t allocate_bucket_page();

        // Bucket operations
        HashBucketHeader read_bucket_header(const std::vector<std::uint8_t>& page) const;
        void write_bucket_header(std::vector<std::uint8_t>& page, const HashBucketHeader& header);
        bool bucket_insert(std::uint32_t bucket_page, const HashEntry& entry, std::string& err);
        bool bucket_search(std::uint32_t bucket_page, const std::string& key,
                           std::vector<HashEntry>& out) const;
        bool bucket_delete(std::uint32_t bucket_page, const std::string& key, std::size_t& deleted);

        // Entry encoding/decoding
        void encode_entry(std::vector<std::uint8_t>& page, std::uint16_t& offset,
                          const HashEntry& entry);
        void decode_entry(const std::vector<std::uint8_t>& page, std::uint16_t offset,
                          HashEntry& entry);
        std::uint16_t entry_size(const HashEntry& entry) const;

        // Hash distribution analysis
        void analyze_hash_distribution();
        bool needs_reorganization() const;

        // Collision handling
        std::uint32_t create_overflow_bucket(std::uint32_t primary_bucket);
        void chain_overflow_bucket(std::uint32_t bucket_page, std::uint32_t overflow_page);

        FileMap fmap_;
        std::uint32_t page_size_{4096};
        bool unique_{false};
        HashIndexTunables tunables_;
        std::unique_ptr<HashFunction> hash_func_;

        // Directory structure
        std::uint32_t directory_page_{0};
        std::uint32_t global_depth_{0};
        std::vector<HashDirEntry> directory_;

        // Statistics
        mutable HashIndexStats cached_stats_;
        mutable bool stats_dirty_{true};
    };

    /**
     * BTree wrapper to implement IndexFamily interface
     * This allows existing BTree code to work with the new family system
     */
    class BTreeIndexFamily : public IndexFamily
    {
      public:
        BTreeIndexFamily(FileMap fmap, std::uint32_t page_size, bool unique);
        ~BTreeIndexFamily() override = default;

        // IndexFamily interface - delegates to BTreeIndex
        bool insert(const std::string& key, std::uint64_t row_id, std::string& err) override;
        bool insert_with_payload(const std::string& key, std::uint64_t row_id,
                                 const std::string& payload, std::string& err) override;
        void search_equal(const std::string& key, std::vector<std::uint64_t>& out) const override;
        void search_equal_with_payload(
            const std::string& key,
            std::vector<std::pair<std::uint64_t, std::string>>& out) const override;
        void search_range(const std::string& lo, bool lo_incl, const std::string& hi, bool hi_incl,
                          std::vector<std::pair<std::string, std::uint64_t>>& out) const override;
        std::size_t erase_equal(const std::string& key, std::string& err) override;

        bool validate(std::string& error) const override;
        void rebuild_offline() override;
        std::string collect_statistics() const override;
        void compact_index() override {}; // BTree doesn't need compaction

        IndexMethod get_method() const override
        {
            return IndexMethod::BTree;
        }
        std::uint32_t root_page() const override;
        bool open_existing(std::uint32_t root_page) override;
        void create_empty() override;

        double estimate_search_cost(const std::string& key) const override;
        double estimate_range_cost(const std::string& lo, const std::string& hi) const override;
        double estimate_maintenance_cost() const override;

        // Access to underlying BTree
        BTreeIndex* btree()
        {
            return btree_.get();
        }
        const BTreeIndex* btree() const
        {
            return btree_.get();
        }

      private:
        std::unique_ptr<BTreeIndex> btree_;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_HASH_H
