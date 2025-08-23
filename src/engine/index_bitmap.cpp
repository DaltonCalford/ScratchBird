#include "scratchbird/engine/index_bitmap.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>

namespace scratchbird::engine
{

    BitmapIndex::BitmapIndex(FileMap fmap, std::uint32_t page_size, bool unique)
        : fmap_(std::move(fmap)), page_size_(page_size), unique_(unique)
    {
        // Bitmap indexes typically don't enforce uniqueness
        if (unique) {
            // Log warning but continue - could be used for existence checking
        }
    }

    bool BitmapIndex::insert(const std::string& key, std::uint64_t row_id, std::string& err)
    {
        return bitmap_insert(key, row_id, err);
    }

    bool BitmapIndex::insert_with_payload(const std::string& key, std::uint64_t row_id,
                                          const std::string& payload, std::string& err)
    {
        // Bitmap indexes don't typically store payloads, but we can extend this later
        (void)payload; // Suppress unused parameter warning
        return bitmap_insert(key, row_id, err);
    }

    void BitmapIndex::search_equal(const std::string& key, std::vector<std::uint64_t>& out) const
    {
        bitmap_search(key, out);
    }

    void BitmapIndex::search_equal_with_payload(
        const std::string& key, std::vector<std::pair<std::uint64_t, std::string>>& out) const
    {
        // Bitmap indexes don't store payloads, return row IDs with empty payloads
        std::vector<std::uint64_t> row_ids;
        bitmap_search(key, row_ids);

        out.clear();
        out.reserve(row_ids.size());
        for (std::uint64_t row_id : row_ids) {
            out.emplace_back(row_id, "");
        }
    }

    void BitmapIndex::search_range(const std::string& lo, bool lo_incl, const std::string& hi,
                                   bool hi_incl,
                                   std::vector<std::pair<std::string, std::uint64_t>>& out) const
    {
        // Bitmap indexes are not well-suited for range queries
        // This is more of a conceptual implementation
        (void)lo;
        (void)lo_incl;
        (void)hi;
        (void)hi_incl; // Suppress warnings

        out.clear();
        // Could implement by scanning all values between lo and hi
        // but this would be inefficient for bitmap indexes
    }

    std::size_t BitmapIndex::erase_equal(const std::string& key, std::string& err)
    {
        auto it = value_bitmaps_.find(key);
        if (it == value_bitmaps_.end()) {
            return 0; // No entries for this key
        }

        // Clear the entire bitmap for this value
        std::size_t deleted = it->second.bit_count;
        value_bitmaps_.erase(it);
        stats_dirty_ = true;

        return deleted;
    }

    bool BitmapIndex::validate(std::string& error) const
    {
        // Validate bitmap consistency
        try {
            for (const auto& [value, entry] : value_bitmaps_) {
                if (entry.bitmap.empty()) {
                    error = "Empty bitmap for value: " + value;
                    return false;
                }

                // Verify bit count matches actual set bits
                std::uint32_t actual_bits = count_set_bits(entry.bitmap);
                if (actual_bits != entry.bit_count) {
                    error = "Bit count mismatch for value: " + value;
                    return false;
                }
            }
            return true;
        } catch (const std::exception& e) {
            error = "Validation error: " + std::string(e.what());
            return false;
        }
    }

    void BitmapIndex::rebuild_offline()
    {
        // Rebuild bitmap index from scratch
        // This would typically involve:
        // 1. Scanning the base table
        // 2. Rebuilding bitmaps for each distinct value
        // 3. Applying compression

        stats_dirty_ = true;

        // Apply compression to all bitmaps
        for (auto& [value, entry] : value_bitmaps_) {
            compress_bitmap(entry);
        }
    }

    std::string BitmapIndex::collect_statistics() const
    {
        if (stats_dirty_) {
            cached_stats_ = compute_statistics();
            stats_dirty_ = false;
        }

        std::ostringstream oss;
        oss << "Bitmap Index Statistics:\n"
            << "  Total Values: " << cached_stats_.total_values << "\n"
            << "  Total Bitmaps: " << cached_stats_.total_bitmaps << "\n"
            << "  Compressed Bitmaps: " << cached_stats_.compressed_bitmaps << "\n"
            << "  Total Bits: " << cached_stats_.total_bits << "\n"
            << "  Set Bits: " << cached_stats_.set_bits << "\n"
            << "  Compression Ratio: " << cached_stats_.compression_ratio << "\n"
            << "  Average Selectivity: " << cached_stats_.selectivity << "\n";

        return oss.str();
    }

    void BitmapIndex::compact_index()
    {
        // Compact bitmap index by:
        // 1. Removing empty bitmaps
        // 2. Recompressing bitmaps with better compression
        // 3. Reorganizing pages for better locality

        auto it = value_bitmaps_.begin();
        while (it != value_bitmaps_.end()) {
            if (it->second.bit_count == 0) {
                it = value_bitmaps_.erase(it);
            } else {
                compress_bitmap(it->second);
                ++it;
            }
        }

        stats_dirty_ = true;
    }

    bool BitmapIndex::open_existing(std::uint32_t root_page)
    {
        root_page_ = root_page;
        // Load bitmap index structure from pages
        // This is a simplified implementation
        return true;
    }

    void BitmapIndex::create_empty()
    {
        // Initialize empty bitmap index
        root_page_ = 1; // Placeholder
        value_bitmaps_.clear();
        stats_dirty_ = true;
    }

    double BitmapIndex::estimate_search_cost(const std::string& key) const
    {
        // Bitmap index search cost is typically very low for equality
        // Cost depends on bitmap size and compression
        auto it = value_bitmaps_.find(key);
        if (it == value_bitmaps_.end()) {
            return 0.1; // Very cheap to determine non-existence
        }

        // Cost is proportional to bitmap size
        double bitmap_size = it->second.bitmap.size();
        return 0.5 + bitmap_size / 1000.0; // Base cost + size factor
    }

    double BitmapIndex::estimate_range_cost(const std::string& lo, const std::string& hi) const
    {
        (void)lo;
        (void)hi; // Suppress warnings

        // Range queries are expensive for bitmap indexes
        // Need to scan multiple bitmaps and combine them
        return std::numeric_limits<double>::infinity();
    }

    double BitmapIndex::estimate_maintenance_cost() const
    {
        // Maintenance cost depends on number of distinct values
        return static_cast<double>(value_bitmaps_.size()) * 0.1;
    }

    BitmapIndexStats BitmapIndex::compute_statistics() const
    {
        BitmapIndexStats stats;

        stats.total_values = value_bitmaps_.size();
        stats.total_bitmaps = value_bitmaps_.size();

        for (const auto& [value, entry] : value_bitmaps_) {
            stats.total_bits += entry.bitmap.size() * 8;
            stats.set_bits += entry.bit_count;
            if (entry.compressed) {
                stats.compressed_bitmaps++;
            }
        }

        if (stats.total_bits > 0) {
            stats.selectivity = static_cast<double>(stats.set_bits) / stats.total_bits;
        }

        // Simplified compression ratio calculation
        if (stats.compressed_bitmaps > 0) {
            stats.compression_ratio = 0.3; // Assume 30% compression on average
        }

        return stats;
    }

    // Bitmap operations
    std::vector<std::uint8_t> BitmapIndex::bitmap_and(const std::vector<std::uint8_t>& a,
                                                      const std::vector<std::uint8_t>& b) const
    {
        std::size_t min_size = std::min(a.size(), b.size());
        std::vector<std::uint8_t> result(min_size);

        for (std::size_t i = 0; i < min_size; ++i) {
            result[i] = a[i] & b[i];
        }

        return result;
    }

    std::vector<std::uint8_t> BitmapIndex::bitmap_or(const std::vector<std::uint8_t>& a,
                                                     const std::vector<std::uint8_t>& b) const
    {
        std::size_t max_size = std::max(a.size(), b.size());
        std::vector<std::uint8_t> result(max_size);

        for (std::size_t i = 0; i < max_size; ++i) {
            std::uint8_t byte_a = (i < a.size()) ? a[i] : 0;
            std::uint8_t byte_b = (i < b.size()) ? b[i] : 0;
            result[i] = byte_a | byte_b;
        }

        return result;
    }

    std::vector<std::uint8_t> BitmapIndex::bitmap_not(const std::vector<std::uint8_t>& bitmap,
                                                      std::uint32_t total_bits) const
    {
        std::vector<std::uint8_t> result = bitmap;

        // Flip all bits
        for (auto& byte : result) {
            byte = ~byte;
        }

        // Clear bits beyond total_bits
        std::uint32_t total_bytes = (total_bits + 7) / 8;
        if (result.size() > total_bytes) {
            result.resize(total_bytes);
        }

        // Clear partial bits in last byte
        if (total_bits % 8 != 0) {
            std::uint8_t mask = (1 << (total_bits % 8)) - 1;
            result.back() &= mask;
        }

        return result;
    }

    void BitmapIndex::search_multiple_values(const std::vector<std::string>& values,
                                             std::vector<std::uint64_t>& out) const
    {
        if (values.empty()) {
            out.clear();
            return;
        }

        // Start with first value's bitmap
        auto it = value_bitmaps_.find(values[0]);
        if (it == value_bitmaps_.end()) {
            out.clear();
            return;
        }

        std::vector<std::uint8_t> result_bitmap = it->second.bitmap;

        // OR with remaining values
        for (std::size_t i = 1; i < values.size(); ++i) {
            auto value_it = value_bitmaps_.find(values[i]);
            if (value_it != value_bitmaps_.end()) {
                result_bitmap = bitmap_or(result_bitmap, value_it->second.bitmap);
            }
        }

        // Convert bitmap to row IDs
        out.clear();
        for (std::uint32_t bit_pos = 0; bit_pos < result_bitmap.size() * 8; ++bit_pos) {
            if (get_bit(result_bitmap, bit_pos)) {
                out.push_back(bit_pos);
            }
        }
    }

    bool BitmapIndex::bulk_insert(const std::vector<std::pair<std::string, std::uint64_t>>& entries,
                                  std::string& err)
    {
        try {
            for (const auto& [key, row_id] : entries) {
                if (!bitmap_insert(key, row_id, err)) {
                    return false;
                }
            }
            return true;
        } catch (const std::exception& e) {
            err = "Bulk insert failed: " + std::string(e.what());
            return false;
        }
    }

    void BitmapIndex::reorganize_for_compression()
    {
        for (auto& [value, entry] : value_bitmaps_) {
            compress_bitmap(entry);
        }
        stats_dirty_ = true;
    }

    // Private implementation methods

    std::vector<std::uint8_t> BitmapIndex::new_page_buffer(ods::PageType type,
                                                           std::uint32_t page_no) const
    {
        (void)page_no; // Suppress warning
        std::vector<std::uint8_t> buffer(page_size_, 0);
        ods::PageHeader header;
        header.type = static_cast<std::uint16_t>(type);
        header.flags = 0;
        std::memcpy(buffer.data(), &header, sizeof(header));
        return buffer;
    }

    void BitmapIndex::write_page(std::uint32_t page_no, const std::vector<std::uint8_t>& page)
    {
        (void)page_no;
        (void)page; // Suppress warnings
        // In real implementation, would use fmap_ to write page
    }

    void BitmapIndex::read_page(std::uint32_t page_no, std::vector<std::uint8_t>& page) const
    {
        (void)page_no; // Suppress warning
        // In real implementation, would use fmap_ to read page
        // For now, initialize with empty page
        page.resize(page_size_, 0);
        ods::PageHeader header;
        header.type = static_cast<std::uint16_t>(ods::PageType::BitmapPage);
        header.flags = 0;
        std::memcpy(page.data(), &header, sizeof(header));
    }

    BitmapPageHeader BitmapIndex::read_bitmap_header(const std::vector<std::uint8_t>& page) const
    {
        BitmapPageHeader header;
        if (page.size() >= sizeof(ods::PageHeader) + sizeof(BitmapPageHeader)) {
            std::memcpy(&header, page.data() + sizeof(ods::PageHeader), sizeof(header));
        }
        return header;
    }

    void BitmapIndex::write_bitmap_header(std::vector<std::uint8_t>& page,
                                          const BitmapPageHeader& header)
    {
        if (page.size() >= sizeof(ods::PageHeader) + sizeof(BitmapPageHeader)) {
            std::memcpy(page.data() + sizeof(ods::PageHeader), &header, sizeof(header));
        }
    }

    bool BitmapIndex::bitmap_insert(const std::string& value, std::uint64_t row_id,
                                    std::string& err)
    {
        try {
            auto& entry = value_bitmaps_[value];

            // Ensure bitmap is large enough
            std::uint32_t bit_pos = static_cast<std::uint32_t>(row_id);
            std::uint32_t byte_pos = bit_pos / 8;

            if (entry.bitmap.size() <= byte_pos) {
                entry.bitmap.resize(byte_pos + 1, 0);
            }

            // Set the bit
            if (!get_bit(entry.bitmap, bit_pos)) {
                set_bit(entry.bitmap, bit_pos);
                entry.bit_count++;
                stats_dirty_ = true;
            }

            return true;
        } catch (const std::exception& e) {
            err = "Bitmap insert failed: " + std::string(e.what());
            return false;
        }
    }

    bool BitmapIndex::bitmap_search(const std::string& value, std::vector<std::uint64_t>& out) const
    {
        out.clear();

        auto it = value_bitmaps_.find(value);
        if (it == value_bitmaps_.end()) {
            return true; // Empty result set is valid
        }

        const auto& bitmap = it->second.bitmap;
        for (std::uint32_t bit_pos = 0; bit_pos < bitmap.size() * 8; ++bit_pos) {
            if (get_bit(bitmap, bit_pos)) {
                out.push_back(bit_pos);
            }
        }

        return true;
    }

    void BitmapIndex::compress_bitmap(BitmapEntry& entry)
    {
        if (entry.compressed || entry.bitmap.size() < tunables_.compression_threshold) {
            return; // Already compressed or too small to benefit
        }

        if (tunables_.use_rle_compression) {
            auto compressed = rle_compress(entry.bitmap);
            if (compressed.size() < entry.bitmap.size()) {
                entry.bitmap = std::move(compressed);
                entry.compressed = true;
            }
        }
    }

    void BitmapIndex::decompress_bitmap(BitmapEntry& entry)
    {
        if (!entry.compressed) {
            return;
        }

        // Decompress based on compression type (simplified)
        if (tunables_.use_rle_compression) {
            entry.bitmap = rle_decompress(entry.bitmap);
            entry.compressed = false;
        }
    }

    // Compression implementations (simplified)
    std::vector<std::uint8_t> BitmapIndex::rle_compress(const std::vector<std::uint8_t>& bitmap)
    {
        std::vector<std::uint8_t> compressed;
        // Simplified RLE compression implementation
        // Format: [count, value, count, value, ...]

        if (bitmap.empty())
            return compressed;

        std::uint8_t current = bitmap[0];
        std::uint8_t count = 1;

        for (std::size_t i = 1; i < bitmap.size(); ++i) {
            if (bitmap[i] == current && count < 255) {
                count++;
            } else {
                compressed.push_back(count);
                compressed.push_back(current);
                current = bitmap[i];
                count = 1;
            }
        }

        compressed.push_back(count);
        compressed.push_back(current);

        return compressed;
    }

    std::vector<std::uint8_t>
    BitmapIndex::rle_decompress(const std::vector<std::uint8_t>& compressed)
    {
        std::vector<std::uint8_t> bitmap;

        for (std::size_t i = 0; i < compressed.size(); i += 2) {
            if (i + 1 < compressed.size()) {
                std::uint8_t count = compressed[i];
                std::uint8_t value = compressed[i + 1];

                for (std::uint8_t j = 0; j < count; ++j) {
                    bitmap.push_back(value);
                }
            }
        }

        return bitmap;
    }

    std::vector<std::uint8_t> BitmapIndex::wah_compress(const std::vector<std::uint8_t>& bitmap)
    {
        (void)bitmap; // Suppress warning
        // WAH compression is more complex - placeholder implementation
        return bitmap;
    }

    std::vector<std::uint8_t>
    BitmapIndex::wah_decompress(const std::vector<std::uint8_t>& compressed)
    {
        // WAH decompression - placeholder implementation
        return compressed;
    }

    // Utility functions
    void BitmapIndex::set_bit(std::vector<std::uint8_t>& bitmap, std::uint32_t bit_pos)
    {
        std::uint32_t byte_pos = bit_pos / 8;
        std::uint32_t bit_offset = bit_pos % 8;

        if (byte_pos < bitmap.size()) {
            bitmap[byte_pos] |= (1 << bit_offset);
        }
    }

    bool BitmapIndex::get_bit(const std::vector<std::uint8_t>& bitmap, std::uint32_t bit_pos) const
    {
        std::uint32_t byte_pos = bit_pos / 8;
        std::uint32_t bit_offset = bit_pos % 8;

        if (byte_pos >= bitmap.size()) {
            return false;
        }

        return (bitmap[byte_pos] & (1 << bit_offset)) != 0;
    }

    std::uint32_t BitmapIndex::count_set_bits(const std::vector<std::uint8_t>& bitmap) const
    {
        std::uint32_t count = 0;
        for (std::uint8_t byte : bitmap) {
            // Use __builtin_popcount for efficient bit counting
            count += __builtin_popcount(byte);
        }
        return count;
    }

} // namespace scratchbird::engine
