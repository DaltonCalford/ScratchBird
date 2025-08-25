#ifndef SCRATCHBIRD_ENGINE_INDEX_COLUMNSTORE_H
#define SCRATCHBIRD_ENGINE_INDEX_COLUMNSTORE_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_family.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    /**
     * Columnstore compression algorithms
     */
    enum class CompressionAlgorithm {
        None,
        Dictionary, // Dictionary encoding for strings
        RunLength,  // Run-length encoding for repeated values
        BitPacking, // Bit packing for integers
        LZ4,        // LZ4 compression
        ZSTD,       // Zstandard compression
        SNAPPY,     // Snappy compression
        Delta       // Delta encoding for sorted sequences
    };

    /**
     * Column segment metadata
     */
    struct ColumnSegment {
        std::uint32_t column_index;
        std::uint32_t data_page;
        std::uint32_t dict_page;
        std::uint32_t null_bitmap_page;
        CompressionAlgorithm compression;
        std::uint64_t uncompressed_size;
        std::uint64_t compressed_size;
        std::uint64_t row_count;

        // Statistics for query optimization
        std::string min_value;
        std::string max_value;
        std::uint64_t distinct_values;
        double selectivity;
    };

    /**
     * Columnstore index implementation
     * Optimized for analytical workloads with columnar storage
     */
    class ColumnstoreIndex : public IndexFamily
    {
      public:
        ColumnstoreIndex(FileMap&& fmap, std::uint32_t page_size, bool unique = false);
        ~ColumnstoreIndex() override;

        // Core index operations
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

        // Index maintenance
        bool validate(std::string& error) const override;
        void rebuild_offline() override;
        std::string collect_statistics() const override;
        void compact_index() override;

        // Index metadata
        IndexMethod get_method() const override
        {
            return IndexMethod::Columnstore;
        }
        std::uint32_t root_page() const override
        {
            return meta_page_;
        }
        bool open_existing(std::uint32_t root_page) override;
        void create_empty() override;

        // Cost estimation
        double estimate_search_cost(const std::string& key) const override;
        double estimate_range_cost(const std::string& lo, const std::string& hi) const override;
        double estimate_maintenance_cost() const override;

        // Columnstore-specific operations
        bool add_column(const std::string& column_name, const std::string& data_type,
                        std::string& err);
        bool compress_segment(std::uint32_t column_index, CompressionAlgorithm algorithm,
                              std::string& err);
        std::vector<ColumnSegment> get_column_segments() const;

        // Analytical query optimization
        bool supports_vectorized_operations() const
        {
            return true;
        }
        bool supports_parallel_scan() const
        {
            return true;
        }
        std::vector<std::uint64_t> column_scan(std::uint32_t column_index,
                                               const std::string& predicate) const;

      private:
        FileMap fmap_;
        std::uint32_t page_size_;
        std::uint32_t meta_page_;
        bool unique_;

        // Column segments management
        std::vector<ColumnSegment> column_segments_;
        std::unordered_map<std::string, std::uint32_t> column_name_to_index_;

        // Forward declarations for compression encoders
        class DictionaryEncoder;
        class RunLengthEncoder;
        class BitPackingEncoder;

        // Compression management
        std::unique_ptr<DictionaryEncoder> dictionary_encoder_;
        std::unique_ptr<RunLengthEncoder> rle_encoder_;
        std::unique_ptr<BitPackingEncoder> bitpack_encoder_;

        // Internal operations
        bool load_metadata(std::string& err);
        bool save_metadata(std::string& err);
        bool create_column_segment(const std::string& column_name, const std::string& data_type,
                                   std::string& err);
        std::vector<std::uint8_t> compress_data(const std::vector<std::uint8_t>& data,
                                                CompressionAlgorithm algorithm) const;
        std::vector<std::uint8_t> decompress_data(const std::vector<std::uint8_t>& compressed_data,
                                                  CompressionAlgorithm algorithm) const;

        // Individual compression algorithm implementations
        std::vector<std::uint8_t> compress_lz4(const std::vector<std::uint8_t>& data) const;
        std::vector<std::uint8_t>
        decompress_lz4(const std::vector<std::uint8_t>& compressed_data) const;
        std::vector<std::uint8_t> compress_zstd(const std::vector<std::uint8_t>& data) const;
        std::vector<std::uint8_t>
        decompress_zstd(const std::vector<std::uint8_t>& compressed_data) const;
        std::vector<std::uint8_t> compress_snappy(const std::vector<std::uint8_t>& data) const;
        std::vector<std::uint8_t>
        decompress_snappy(const std::vector<std::uint8_t>& compressed_data) const;

        std::uint32_t allocate_page();
    };

    /**
     * Columnstore scan implementation for analytical queries
     */
    class ColumnstoreScan : public IndexScan
    {
      public:
        ColumnstoreScan(ColumnstoreIndex* index) : index_(index) {}

        bool init(const std::string& key_condition) override;
        bool next(std::uint64_t& row_id, std::string& key, std::string& payload) override;
        void reset() override;
        bool is_finished() const override;
        std::uint64_t rows_scanned() const override;
        std::uint64_t pages_accessed() const override;

        // Analytical scan specific operations
        bool init_column_scan(std::uint32_t column_index, const std::string& predicate);
        bool supports_vectorized_batch() const
        {
            return true;
        }
        std::uint64_t get_batch_size(std::vector<std::uint64_t>& row_ids,
                                     std::vector<std::string>& values,
                                     std::uint64_t max_batch_size);

      private:
        ColumnstoreIndex* index_;
        bool finished_{false};
        std::uint64_t rows_scanned_{0};
        std::uint64_t pages_accessed_{0};

        // Column scan state
        std::uint32_t current_column_index_{0};
        std::string current_predicate_;
        std::vector<std::uint64_t> column_scan_results_;
        std::size_t result_position_{0};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_COLUMNSTORE_H
