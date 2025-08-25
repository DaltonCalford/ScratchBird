#ifndef SCRATCHBIRD_ENGINE_INDEX_BITMAP_H
#define SCRATCHBIRD_ENGINE_INDEX_BITMAP_H

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
     * Bitmap index tunables for performance optimization
     */
    struct BitmapIndexTunables {
        std::uint32_t compression_threshold{1000}; // Compress bitmaps with more than N bits
        bool use_rle_compression{true};            // Run-length encoding
        bool use_wah_compression{true};            // Word-Aligned Hybrid compression
        std::uint32_t bitmap_cache_size{100};      // Number of bitmaps to cache
    };

    /**
     * Bitmap page header structure
     */
    struct BitmapPageHeader {
        std::uint16_t num_bitmaps{0}; // Number of distinct values in this page
        std::uint16_t free_start{0};  // Start of free space
        std::uint16_t dir_start{0};   // Start of slot directory
        std::uint32_t total_rows{0};  // Total number of rows represented
        std::uint32_t min_row_id{0};  // Minimum row ID in this page
        std::uint32_t max_row_id{0};  // Maximum row ID in this page
    };

    /**
     * Bitmap entry structure (for each distinct value)
     */
    struct BitmapEntry {
        std::string value;                // The distinct value
        std::vector<std::uint8_t> bitmap; // Bitmap data
        std::uint32_t bit_count{0};       // Number of set bits
        bool compressed{false};           // Whether bitmap is compressed
    };

    /**
     * Compressed bitmap formats
     */
    enum class CompressionType : std::uint8_t {
        None = 0,
        RLE = 1,    // Run-length encoding
        WAH = 2,    // Word-Aligned Hybrid
        Roaring = 3 // Roaring bitmaps (future)
    };

    /**
     * Bitmap statistics for monitoring and optimization
     */
    struct BitmapIndexStats {
        std::uint32_t total_values{0};                 // Number of distinct values
        std::uint32_t total_bitmaps{0};                // Number of bitmap pages
        std::uint32_t compressed_bitmaps{0};           // Number of compressed bitmaps
        std::uint64_t total_bits{0};                   // Total bits across all bitmaps
        std::uint64_t set_bits{0};                     // Total set bits
        double compression_ratio{1.0};                 // Average compression ratio
        double selectivity{0.0};                       // Average selectivity per value
        std::vector<std::uint32_t> value_distribution; // Frequency of each value
    };

    /**
     * Bitmap Index implementation
     * Optimized for low-cardinality columns with good compression
     */
    class BitmapIndex : public IndexFamily
    {
      public:
        BitmapIndex(FileMap fmap, std::uint32_t page_size, bool unique);
        ~BitmapIndex() override = default;

        void set_tunables(const BitmapIndexTunables& tunables)
        {
            tunables_ = tunables;
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
            return IndexMethod::Bitmap;
        }
        std::uint32_t root_page() const override
        {
            return root_page_;
        }
        bool open_existing(std::uint32_t root_page) override;
        void create_empty() override;

        // Cost estimation
        double estimate_search_cost(const std::string& key) const override;
        double estimate_range_cost(const std::string& lo, const std::string& hi) const override;
        double estimate_maintenance_cost() const override;

        // Bitmap-specific operations
        void compress_bitmap(BitmapEntry& entry);
        void decompress_bitmap(BitmapEntry& entry);
        BitmapIndexStats compute_statistics() const;

        // Bitmap operations (AND, OR, XOR, NOT)
        std::vector<std::uint8_t> bitmap_and(const std::vector<std::uint8_t>& a,
                                             const std::vector<std::uint8_t>& b) const;
        std::vector<std::uint8_t> bitmap_or(const std::vector<std::uint8_t>& a,
                                            const std::vector<std::uint8_t>& b) const;
        std::vector<std::uint8_t> bitmap_not(const std::vector<std::uint8_t>& bitmap,
                                             std::uint32_t total_bits) const;

        // Multi-value search (for complex WHERE clauses)
        void search_multiple_values(const std::vector<std::string>& values,
                                    std::vector<std::uint64_t>& out) const;

        // Bulk operations
        bool bulk_insert(const std::vector<std::pair<std::string, std::uint64_t>>& entries,
                         std::string& err);
        void reorganize_for_compression();

      private:
        // Page management
        std::vector<std::uint8_t> new_page_buffer(ods::PageType type, std::uint32_t page_no) const;
        void write_page(std::uint32_t page_no, const std::vector<std::uint8_t>& page);
        void read_page(std::uint32_t page_no, std::vector<std::uint8_t>& page) const;

        // Bitmap operations
        BitmapPageHeader read_bitmap_header(const std::vector<std::uint8_t>& page) const;
        void write_bitmap_header(std::vector<std::uint8_t>& page, const BitmapPageHeader& header);
        bool bitmap_insert(const std::string& value, std::uint64_t row_id, std::string& err);
        bool bitmap_search(const std::string& value, std::vector<std::uint64_t>& out) const;

        // Entry encoding/decoding
        void encode_bitmap_entry(std::vector<std::uint8_t>& page, std::uint16_t& offset,
                                 const BitmapEntry& entry);
        void decode_bitmap_entry(const std::vector<std::uint8_t>& page, std::uint16_t offset,
                                 BitmapEntry& entry);
        std::uint16_t bitmap_entry_size(const BitmapEntry& entry) const;

        // Compression algorithms
        std::vector<std::uint8_t> rle_compress(const std::vector<std::uint8_t>& bitmap);
        std::vector<std::uint8_t> rle_decompress(const std::vector<std::uint8_t>& compressed);
        std::vector<std::uint8_t> wah_compress(const std::vector<std::uint8_t>& bitmap);
        std::vector<std::uint8_t> wah_decompress(const std::vector<std::uint8_t>& compressed);

        // Utility functions
        void set_bit(std::vector<std::uint8_t>& bitmap, std::uint32_t bit_pos);
        bool get_bit(const std::vector<std::uint8_t>& bitmap, std::uint32_t bit_pos) const;
        std::uint32_t count_set_bits(const std::vector<std::uint8_t>& bitmap) const;

        FileMap fmap_;
        std::uint32_t page_size_{4096};
        bool unique_{false}; // Bitmap indexes are typically not unique
        BitmapIndexTunables tunables_;

        // Index structure
        std::uint32_t root_page_{0};
        std::unordered_map<std::string, BitmapEntry> value_bitmaps_; // In-memory cache

        // Statistics
        mutable BitmapIndexStats cached_stats_;
        mutable bool stats_dirty_{true};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_BITMAP_H
