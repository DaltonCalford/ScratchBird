#ifndef SCRATCHBIRD_ENGINE_INDEX_FAMILY_H
#define SCRATCHBIRD_ENGINE_INDEX_FAMILY_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index.h"
#include "scratchbird/engine/ods.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    // Forward declarations for index families
    class BTreeIndex;
    class HashIndex;
    class BitmapIndex;
    class GinIndex;
    class RTreeIndex;

    /**
     * Abstract base class for all index families.
     * Provides common interface for index operations across different implementations.
     */
    class IndexFamily
    {
      public:
        virtual ~IndexFamily() = default;

        // Core index operations
        virtual bool insert(const std::string& key, std::uint64_t row_id, std::string& err) = 0;
        virtual bool insert_with_payload(const std::string& key, std::uint64_t row_id,
                                         const std::string& payload, std::string& err) = 0;
        virtual void search_equal(const std::string& key,
                                  std::vector<std::uint64_t>& out) const = 0;
        virtual void search_equal_with_payload(
            const std::string& key,
            std::vector<std::pair<std::uint64_t, std::string>>& out) const = 0;
        virtual void
        search_range(const std::string& lo, bool lo_incl, const std::string& hi, bool hi_incl,
                     std::vector<std::pair<std::string, std::uint64_t>>& out) const = 0;
        virtual std::size_t erase_equal(const std::string& key, std::string& err) = 0;

        // Index maintenance operations
        virtual bool validate(std::string& error) const = 0;
        virtual void rebuild_offline() = 0;
        virtual std::string collect_statistics() const = 0;
        virtual void compact_index() = 0;

        // Index metadata
        virtual IndexMethod get_method() const = 0;
        virtual std::uint32_t root_page() const = 0;
        virtual bool open_existing(std::uint32_t root_page) = 0;
        virtual void create_empty() = 0;

        // Cost estimation for optimizer
        virtual double estimate_search_cost(const std::string& key) const = 0;
        virtual double estimate_range_cost(const std::string& lo, const std::string& hi) const = 0;
        virtual double estimate_maintenance_cost() const = 0;
    };

    /**
     * Factory for creating index family instances.
     */
    class IndexFamilyFactory
    {
      public:
        static std::unique_ptr<IndexFamily> create_index(IndexMethod method, FileMap fmap,
                                                         std::uint32_t page_size, bool unique);

        // Family-specific validation before creation
        static std::vector<ValidationMessage>
        validate_method_options(const IndexCreateOptions& opts);

        // Get supported operations for each family
        static bool supports_range_queries(IndexMethod method);
        static bool supports_partial_indexes(IndexMethod method);
        static bool supports_include_columns(IndexMethod method);
        static bool supports_expression_indexes(IndexMethod method);
    };

    /**
     * Index family configuration and parameters.
     */
    struct IndexFamilyConfig {
        IndexMethod method{IndexMethod::BTree};

        // Hash index specific
        struct {
            std::uint32_t initial_buckets{1024};
            double load_factor{0.75};
            bool extensible_hashing{true};
        } hash_config;

        // Bitmap index specific
        struct {
            std::uint32_t compression_threshold{1000};
            bool use_rle_compression{true};
            bool use_wah_compression{false};
        } bitmap_config;

        // GIN index specific
        struct {
            std::uint32_t posting_list_threshold{100};
            bool compress_posting_lists{true};
            std::string tokenizer{"simple"};
        } gin_config;

        // R-Tree index specific
        struct {
            std::uint32_t max_entries_per_node{50};
            std::uint32_t min_entries_per_node{20};
            double split_strategy{0.4}; // R*-tree split strategy
        } rtree_config;
    };

    /**
     * Index scan interface for family-specific scan operations.
     */
    class IndexScan
    {
      public:
        virtual ~IndexScan() = default;

        virtual bool init(const std::string& key_condition) = 0;
        virtual bool next(std::uint64_t& row_id, std::string& key, std::string& payload) = 0;
        virtual void reset() = 0;
        virtual bool is_finished() const = 0;

        // Scan statistics
        virtual std::uint64_t rows_scanned() const = 0;
        virtual std::uint64_t pages_accessed() const = 0;
    };

    // Family-specific scan types
    class HashIndexScan : public IndexScan
    {
      public:
        HashIndexScan(HashIndex* index) : index_(index) {}

        bool init(const std::string& key_condition) override;
        bool next(std::uint64_t& row_id, std::string& key, std::string& payload) override;
        void reset() override;
        bool is_finished() const override;
        std::uint64_t rows_scanned() const override;
        std::uint64_t pages_accessed() const override;

      private:
        HashIndex* index_;
        bool finished_{false};
        std::uint64_t rows_scanned_{0};
        std::uint64_t pages_accessed_{0};
        std::string current_key_;
        std::vector<std::uint64_t> results_;
        std::size_t result_index_{0};
    };

    class BitmapIndexScan : public IndexScan
    {
      public:
        BitmapIndexScan(BitmapIndex* index) : index_(index) {}

        bool init(const std::string& key_condition) override;
        bool next(std::uint64_t& row_id, std::string& key, std::string& payload) override;
        void reset() override;
        bool is_finished() const override;
        std::uint64_t rows_scanned() const override;
        std::uint64_t pages_accessed() const override;

      private:
        BitmapIndex* index_;
        bool finished_{false};
        std::uint64_t rows_scanned_{0};
        std::uint64_t pages_accessed_{0};
        std::vector<std::uint8_t> bitmap_;
        std::size_t bitmap_position_{0};
    };

    class GinIndexScan : public IndexScan
    {
      public:
        GinIndexScan(GinIndex* index) : index_(index) {}

        bool init(const std::string& key_condition) override;
        bool next(std::uint64_t& row_id, std::string& key, std::string& payload) override;
        void reset() override;
        bool is_finished() const override;
        std::uint64_t rows_scanned() const override;
        std::uint64_t pages_accessed() const override;

      private:
        GinIndex* index_;
        bool finished_{false};
        std::uint64_t rows_scanned_{0};
        std::uint64_t pages_accessed_{0};
        std::vector<std::string> tokens_;
        std::size_t token_index_{0};
        std::vector<std::uint64_t> posting_list_;
        std::size_t posting_index_{0};
    };

    class RTreeIndexScan : public IndexScan
    {
      public:
        RTreeIndexScan(RTreeIndex* index) : index_(index) {}

        bool init(const std::string& key_condition) override;
        bool next(std::uint64_t& row_id, std::string& key, std::string& payload) override;
        void reset() override;
        bool is_finished() const override;
        std::uint64_t rows_scanned() const override;
        std::uint64_t pages_accessed() const override;

      private:
        RTreeIndex* index_;
        bool finished_{false};
        std::uint64_t rows_scanned_{0};
        std::uint64_t pages_accessed_{0};
        struct Rectangle {
            double min_x, min_y, max_x, max_y;
        } query_rect_;
        std::vector<std::pair<std::uint64_t, Rectangle>> results_;
        std::size_t result_index_{0};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_FAMILY_H
