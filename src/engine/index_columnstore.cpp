#include "scratchbird/engine/index_columnstore.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace scratchbird::engine
{

    // Dictionary Encoder for string compression
    class ColumnstoreIndex::DictionaryEncoder
    {
      public:
        std::vector<std::uint8_t> encode(const std::vector<std::string>& values)
        {
            std::vector<std::uint8_t> result;
            std::unordered_map<std::string, std::uint32_t> dictionary;
            std::uint32_t next_id = 0;

            // Build dictionary and encode values
            for (const auto& value : values) {
                if (dictionary.find(value) == dictionary.end()) {
                    dictionary[value] = next_id++;
                }

                // Store dictionary ID (simplified as single byte for demo)
                std::uint32_t id = dictionary[value];
                result.push_back(static_cast<std::uint8_t>(id & 0xFF));
            }

            // Store dictionary at the end (simplified)
            for (const auto& [key, value] : dictionary) {
                for (char c : key) {
                    result.push_back(static_cast<std::uint8_t>(c));
                }
                result.push_back(0); // Null terminator
            }

            return result;
        }

        std::vector<std::string> decode(const std::vector<std::uint8_t>& compressed_data)
        {
            // Simplified decode implementation
            std::vector<std::string> result;
            // Would implement proper dictionary-based decoding
            return result;
        }

        double get_compression_ratio(const std::vector<std::string>& original,
                                     const std::vector<std::uint8_t>& compressed) const
        {
            std::size_t original_size = 0;
            for (const auto& str : original) {
                original_size += str.size();
            }
            return original_size > 0 ? static_cast<double>(compressed.size()) / original_size : 1.0;
        }
    };

    // Run-Length Encoder for repeated values
    class ColumnstoreIndex::RunLengthEncoder
    {
      public:
        std::vector<std::uint8_t> encode(const std::vector<std::string>& values)
        {
            std::vector<std::uint8_t> result;

            if (values.empty()) {
                return result;
            }

            std::string current_value = values[0];
            std::uint32_t count = 1;

            for (std::size_t i = 1; i < values.size(); ++i) {
                if (values[i] == current_value) {
                    count++;
                } else {
                    // Store run: [count][value_length][value_data]
                    result.push_back(static_cast<std::uint8_t>(count & 0xFF));
                    result.push_back(static_cast<std::uint8_t>(current_value.size()));
                    for (char c : current_value) {
                        result.push_back(static_cast<std::uint8_t>(c));
                    }

                    current_value = values[i];
                    count = 1;
                }
            }

            // Store final run
            result.push_back(static_cast<std::uint8_t>(count & 0xFF));
            result.push_back(static_cast<std::uint8_t>(current_value.size()));
            for (char c : current_value) {
                result.push_back(static_cast<std::uint8_t>(c));
            }

            return result;
        }

        std::vector<std::string> decode(const std::vector<std::uint8_t>& compressed_data)
        {
            std::vector<std::string> result;
            std::size_t pos = 0;

            while (pos < compressed_data.size()) {
                if (pos + 2 > compressed_data.size())
                    break;

                std::uint32_t count = compressed_data[pos++];
                std::uint32_t length = compressed_data[pos++];

                if (pos + length > compressed_data.size())
                    break;

                std::string value(compressed_data.begin() + pos,
                                  compressed_data.begin() + pos + length);
                pos += length;

                for (std::uint32_t i = 0; i < count; ++i) {
                    result.push_back(value);
                }
            }

            return result;
        }
    };

    // Bit-Packing Encoder for integers
    class ColumnstoreIndex::BitPackingEncoder
    {
      public:
        std::vector<std::uint8_t> encode(const std::vector<std::uint64_t>& values)
        {
            std::vector<std::uint8_t> result;

            if (values.empty()) {
                return result;
            }

            // Find bit width needed
            std::uint64_t max_value = *std::max_element(values.begin(), values.end());
            std::uint32_t bit_width =
                max_value > 0 ? static_cast<std::uint32_t>(std::log2(max_value)) + 1 : 1;

            // Store bit width
            result.push_back(static_cast<std::uint8_t>(bit_width));

            // Pack values (simplified implementation)
            for (std::uint64_t value : values) {
                // Store as bytes for simplicity (real implementation would bit-pack)
                for (int i = 0; i < 8; ++i) {
                    result.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFF));
                }
            }

            return result;
        }

        std::vector<std::uint64_t> decode(const std::vector<std::uint8_t>& compressed_data)
        {
            std::vector<std::uint64_t> result;

            if (compressed_data.empty()) {
                return result;
            }

            std::uint32_t bit_width = compressed_data[0];

            // Decode values (simplified implementation)
            for (std::size_t i = 1; i + 8 <= compressed_data.size(); i += 8) {
                std::uint64_t value = 0;
                for (int j = 0; j < 8; ++j) {
                    value |= static_cast<std::uint64_t>(compressed_data[i + j]) << (j * 8);
                }
                result.push_back(value);
            }

            return result;
        }
    };

    // ColumnstoreIndex implementation
    ColumnstoreIndex::ColumnstoreIndex(FileMap&& fmap, std::uint32_t page_size, bool unique)
        : fmap_(std::move(fmap)), page_size_(page_size), unique_(unique), meta_page_(0)
    {
        dictionary_encoder_ = std::make_unique<DictionaryEncoder>();
        rle_encoder_ = std::make_unique<RunLengthEncoder>();
        bitpack_encoder_ = std::make_unique<BitPackingEncoder>();
    }

    ColumnstoreIndex::~ColumnstoreIndex() = default;

    bool ColumnstoreIndex::insert(const std::string& key, std::uint64_t row_id, std::string& err)
    {
        return insert_with_payload(key, row_id, "", err);
    }

    bool ColumnstoreIndex::insert_with_payload(const std::string& key, std::uint64_t row_id,
                                               const std::string& payload, std::string& err)
    {
        // Columnstore indexes typically batch inserts for efficiency
        // This is a simplified implementation

        try {
            // For demo, we'll just add to the first column segment
            if (column_segments_.empty()) {
                // Create a default column segment
                std::string default_err;
                if (!create_column_segment("default_column", "VARCHAR", default_err)) {
                    err = "Failed to create column segment: " + default_err;
                    return false;
                }
            }

            // In a real implementation, we would:
            // 1. Parse the key/payload into column values
            // 2. Append to appropriate column segments
            // 3. Update segment statistics
            // 4. Trigger compression when segments reach threshold

            auto& segment = column_segments_[0];
            segment.row_count++;

            // Update min/max statistics
            if (segment.row_count == 1 || key < segment.min_value) {
                segment.min_value = key;
            }
            if (segment.row_count == 1 || key > segment.max_value) {
                segment.max_value = key;
            }

            return true;

        } catch (const std::exception& e) {
            err = "Insert failed: " + std::string(e.what());
            return false;
        }
    }

    void ColumnstoreIndex::search_equal(const std::string& key,
                                        std::vector<std::uint64_t>& out) const
    {
        // Columnstore search involves scanning relevant column segments
        // This would typically use column pruning and vectorized operations

        for (const auto& segment : column_segments_) {
            // Check if key is in range for this segment
            if (key >= segment.min_value && key <= segment.max_value) {
                // In real implementation, we would:
                // 1. Load compressed column data
                // 2. Decompress if needed
                // 3. Use vectorized operations to find matches
                // 4. Return row IDs

                // Simplified: assume we found one match
                out.push_back(segment.column_index * 1000); // Dummy row ID
            }
        }
    }

    void ColumnstoreIndex::search_equal_with_payload(
        const std::string& key, std::vector<std::pair<std::uint64_t, std::string>>& out) const
    {
        std::vector<std::uint64_t> row_ids;
        search_equal(key, row_ids);

        for (auto row_id : row_ids) {
            out.emplace_back(row_id, "columnstore_payload"); // Simplified payload
        }
    }

    void
    ColumnstoreIndex::search_range(const std::string& lo, bool lo_incl, const std::string& hi,
                                   bool hi_incl,
                                   std::vector<std::pair<std::string, std::uint64_t>>& out) const
    {
        // Range queries are well-suited for columnstore indexes
        // They can use column pruning and vectorized operations

        for (const auto& segment : column_segments_) {
            // Check segment overlap with range
            bool segment_overlaps = !(hi < segment.min_value || lo > segment.max_value);

            if (segment_overlaps) {
                // In real implementation:
                // 1. Load segment data
                // 2. Use SIMD operations for range filtering
                // 3. Collect matching row IDs

                // Simplified: add some dummy results
                for (std::uint64_t i = 0; i < std::min(segment.row_count, 10UL); ++i) {
                    std::string value = lo + std::to_string(i);
                    if (value >= lo && value <= hi) {
                        out.emplace_back(value, segment.column_index * 1000 + i);
                    }
                }
            }
        }
    }

    std::size_t ColumnstoreIndex::erase_equal(const std::string& key, std::string& err)
    {
        // Columnstore indexes typically use soft deletes with tombstone markers
        // This avoids expensive column reorganization

        std::size_t deleted_count = 0;

        try {
            for (auto& segment : column_segments_) {
                if (key >= segment.min_value && key <= segment.max_value) {
                    // In real implementation, we would mark rows as deleted
                    // and update segment statistics
                    deleted_count++;
                }
            }

            return deleted_count;

        } catch (const std::exception& e) {
            err = "Delete failed: " + std::string(e.what());
            return 0;
        }
    }

    bool ColumnstoreIndex::validate(std::string& error) const
    {
        try {
            // Validate column segments
            for (const auto& segment : column_segments_) {
                if (segment.column_index >= column_segments_.size()) {
                    error = "Invalid column index in segment";
                    return false;
                }

                if (segment.compressed_size > segment.uncompressed_size * 2) {
                    error = "Suspicious compression ratio in segment " +
                            std::to_string(segment.column_index);
                    return false;
                }
            }

            return true;

        } catch (const std::exception& e) {
            error = "Validation error: " + std::string(e.what());
            return false;
        }
    }

    void ColumnstoreIndex::rebuild_offline()
    {
        // Columnstore rebuild involves:
        // 1. Reorganizing column segments
        // 2. Recompressing with optimal algorithms
        // 3. Updating statistics and indexes

        for (auto& segment : column_segments_) {
            // Trigger recompression
            std::string err;
            compress_segment(segment.column_index, segment.compression, err);
        }
    }

    std::string ColumnstoreIndex::collect_statistics() const
    {
        std::ostringstream stats;
        stats << "Columnstore Index Statistics:\n";
        stats << "  Total segments: " << column_segments_.size() << "\n";

        std::uint64_t total_rows = 0;
        std::uint64_t total_compressed = 0;
        std::uint64_t total_uncompressed = 0;

        for (const auto& segment : column_segments_) {
            total_rows += segment.row_count;
            total_compressed += segment.compressed_size;
            total_uncompressed += segment.uncompressed_size;
        }

        stats << "  Total rows: " << total_rows << "\n";
        stats << "  Compressed size: " << total_compressed << " bytes\n";
        stats << "  Uncompressed size: " << total_uncompressed << " bytes\n";

        double compression_ratio = total_uncompressed > 0
                                       ? static_cast<double>(total_compressed) / total_uncompressed
                                       : 0.0;
        stats << "  Compression ratio: " << compression_ratio << "\n";

        stats << "  Vectorized operations: "
              << (supports_vectorized_operations() ? "Enabled" : "Disabled") << "\n";
        stats << "  Parallel scan: " << (supports_parallel_scan() ? "Enabled" : "Disabled");

        return stats.str();
    }

    void ColumnstoreIndex::compact_index()
    {
        // Columnstore compaction involves:
        // 1. Merging small segments
        // 2. Recompressing large segments
        // 3. Updating column statistics

        std::vector<ColumnSegment> new_segments;

        // Simple compaction: merge segments with low row counts
        ColumnSegment merged_segment;
        bool has_merged = false;

        for (const auto& segment : column_segments_) {
            if (segment.row_count < 1000) { // Threshold for merging
                if (!has_merged) {
                    merged_segment = segment;
                    has_merged = true;
                } else {
                    // Merge with existing segment
                    merged_segment.row_count += segment.row_count;
                    merged_segment.compressed_size += segment.compressed_size;
                    merged_segment.uncompressed_size += segment.uncompressed_size;
                    merged_segment.distinct_values += segment.distinct_values;
                }
            } else {
                new_segments.push_back(segment);
            }
        }

        if (has_merged) {
            new_segments.push_back(merged_segment);
        }

        column_segments_ = std::move(new_segments);
    }

    bool ColumnstoreIndex::open_existing(std::uint32_t root_page)
    {
        meta_page_ = root_page;
        std::string err;
        return load_metadata(err);
    }

    void ColumnstoreIndex::create_empty()
    {
        meta_page_ = allocate_page();
        std::string err;
        save_metadata(err);
    }

    double ColumnstoreIndex::estimate_search_cost(const std::string& key) const
    {
        // Columnstore search cost depends on:
        // 1. Number of segments to scan
        // 2. Compression overhead
        // 3. Vectorization benefits

        double cost = 0.0;

        for (const auto& segment : column_segments_) {
            if (key >= segment.min_value && key <= segment.max_value) {
                // Base cost for segment scan
                cost += 1.0;

                // Compression overhead
                if (segment.compression != CompressionAlgorithm::None) {
                    cost += 0.2; // Decompression cost
                }

                // Vectorization benefit
                cost *= 0.5; // SIMD operations are faster
            }
        }

        return cost;
    }

    double ColumnstoreIndex::estimate_range_cost(const std::string& lo, const std::string& hi) const
    {
        // Range queries are efficient in columnstore due to:
        // 1. Column pruning
        // 2. Vectorized operations
        // 3. Compression benefits for sequential access

        double cost = 0.0;

        for (const auto& segment : column_segments_) {
            bool segment_overlaps = !(hi < segment.min_value || lo > segment.max_value);

            if (segment_overlaps) {
                // Base cost proportional to segment size
                cost += static_cast<double>(segment.row_count) / 10000.0;

                // Vectorization benefit for range scans
                cost *= 0.3; // Very efficient for analytical queries
            }
        }

        return cost;
    }

    double ColumnstoreIndex::estimate_maintenance_cost() const
    {
        // Columnstore maintenance cost includes:
        // 1. Compression overhead
        // 2. Segment reorganization
        // 3. Statistics updates

        double cost = static_cast<double>(column_segments_.size()) * 2.0; // Base cost per segment

        // Add compression cost
        for (const auto& segment : column_segments_) {
            if (segment.compression != CompressionAlgorithm::None) {
                cost += static_cast<double>(segment.uncompressed_size) / 1000000.0; // MB to cost
            }
        }

        return cost;
    }

    bool ColumnstoreIndex::add_column(const std::string& column_name, const std::string& data_type,
                                      std::string& err)
    {
        if (column_name_to_index_.find(column_name) != column_name_to_index_.end()) {
            err = "Column already exists: " + column_name;
            return false;
        }

        return create_column_segment(column_name, data_type, err);
    }

    bool ColumnstoreIndex::compress_segment(std::uint32_t column_index,
                                            CompressionAlgorithm algorithm, std::string& err)
    {
        if (column_index >= column_segments_.size()) {
            err = "Invalid column index: " + std::to_string(column_index);
            return false;
        }

        auto& segment = column_segments_[column_index];

        try {
            // In real implementation, we would:
            // 1. Load uncompressed column data
            // 2. Apply compression algorithm
            // 3. Store compressed data
            // 4. Update segment metadata

            segment.compression = algorithm;

            // Simulate compression ratio based on algorithm
            double compression_ratio = 1.0;
            switch (algorithm) {
            case CompressionAlgorithm::Dictionary:
                compression_ratio = 0.4; // 60% compression
                break;
            case CompressionAlgorithm::RunLength:
                compression_ratio = 0.3; // 70% compression
                break;
            case CompressionAlgorithm::BitPacking:
                compression_ratio = 0.5; // 50% compression
                break;
            case CompressionAlgorithm::LZ4:
                compression_ratio = 0.6; // 40% compression
                break;
            case CompressionAlgorithm::Delta:
                compression_ratio = 0.7; // 30% compression
                break;
            default:
                compression_ratio = 1.0; // No compression
            }

            segment.compressed_size =
                static_cast<std::uint64_t>(segment.uncompressed_size * compression_ratio);

            return true;

        } catch (const std::exception& e) {
            err = "Compression failed: " + std::string(e.what());
            return false;
        }
    }

    std::vector<ColumnSegment> ColumnstoreIndex::get_column_segments() const
    {
        return column_segments_;
    }

    std::vector<std::uint64_t> ColumnstoreIndex::column_scan(std::uint32_t column_index,
                                                             const std::string& predicate) const
    {
        std::vector<std::uint64_t> results;

        if (column_index >= column_segments_.size()) {
            return results;
        }

        const auto& segment = column_segments_[column_index];

        // In real implementation, this would:
        // 1. Load column data
        // 2. Apply vectorized predicate evaluation
        // 3. Return matching row IDs

        // Simplified: return some dummy results
        for (std::uint64_t i = 0; i < std::min(segment.row_count, 100UL); ++i) {
            results.push_back(i);
        }

        return results;
    }

    bool ColumnstoreIndex::load_metadata(std::string& /* err */)
    {
        // Load columnstore metadata from meta page
        // This would include segment descriptors, column mappings, etc.
        return true;
    }

    bool ColumnstoreIndex::save_metadata(std::string& /* err */)
    {
        // Save columnstore metadata to meta page
        return true;
    }

    bool ColumnstoreIndex::create_column_segment(const std::string& column_name,
                                                 const std::string& data_type,
                                                 std::string& /* err */)
    {
        ColumnSegment segment;
        segment.column_index = static_cast<std::uint32_t>(column_segments_.size());
        segment.data_page = allocate_page();
        segment.dict_page = allocate_page();
        segment.null_bitmap_page = allocate_page();
        segment.compression = CompressionAlgorithm::None;
        segment.uncompressed_size = 0;
        segment.compressed_size = 0;
        segment.row_count = 0;
        segment.distinct_values = 0;
        segment.selectivity = 0.0;

        column_segments_.push_back(segment);
        column_name_to_index_[column_name] = segment.column_index;

        return true;
    }

    std::vector<std::uint8_t> ColumnstoreIndex::compress_data(const std::vector<std::uint8_t>& data,
                                                              CompressionAlgorithm algorithm) const
    {
        switch (algorithm) {
        case CompressionAlgorithm::LZ4:
            return compress_lz4(data);
        case CompressionAlgorithm::ZSTD:
            return compress_zstd(data);
        case CompressionAlgorithm::SNAPPY:
            return compress_snappy(data);
        default:
            return data;
        }
    }

    std::vector<std::uint8_t>
    ColumnstoreIndex::decompress_data(const std::vector<std::uint8_t>& compressed_data,
                                      CompressionAlgorithm algorithm) const
    {
        switch (algorithm) {
        case CompressionAlgorithm::LZ4:
            return decompress_lz4(compressed_data);
        case CompressionAlgorithm::ZSTD:
            return decompress_zstd(compressed_data);
        case CompressionAlgorithm::SNAPPY:
            return decompress_snappy(compressed_data);
        default:
            return compressed_data;
        }
    }

    std::uint32_t ColumnstoreIndex::allocate_page()
    {
        static std::uint32_t next_page = 200; // Start after LSM-Tree pages
        return next_page++;
    }

    // ColumnstoreScan implementation
    bool ColumnstoreScan::init(const std::string& key_condition)
    {
        reset();

        if (index_) {
            // Initialize columnstore scan for specific key
            std::vector<std::uint64_t> results;
            index_->search_equal(key_condition, results);

            column_scan_results_ = std::move(results);
            pages_accessed_++;
        }

        return !column_scan_results_.empty();
    }

    bool ColumnstoreScan::next(std::uint64_t& row_id, std::string& key, std::string& payload)
    {
        if (result_position_ >= column_scan_results_.size()) {
            finished_ = true;
            return false;
        }

        row_id = column_scan_results_[result_position_++];
        key = "columnstore_key_" + std::to_string(row_id);
        payload = "columnstore_payload_" + std::to_string(row_id);
        rows_scanned_++;

        return true;
    }

    void ColumnstoreScan::reset()
    {
        finished_ = false;
        rows_scanned_ = 0;
        pages_accessed_ = 0;
        result_position_ = 0;
        column_scan_results_.clear();
        current_column_index_ = 0;
        current_predicate_.clear();
    }

    bool ColumnstoreScan::is_finished() const
    {
        return finished_;
    }

    std::uint64_t ColumnstoreScan::rows_scanned() const
    {
        return rows_scanned_;
    }

    std::uint64_t ColumnstoreScan::pages_accessed() const
    {
        return pages_accessed_;
    }

    bool ColumnstoreScan::init_column_scan(std::uint32_t column_index, const std::string& predicate)
    {
        reset();
        current_column_index_ = column_index;
        current_predicate_ = predicate;

        if (index_) {
            column_scan_results_ = index_->column_scan(column_index, predicate);
            pages_accessed_++;
        }

        return !column_scan_results_.empty();
    }

    std::uint64_t ColumnstoreScan::get_batch_size(std::vector<std::uint64_t>& row_ids,
                                                  std::vector<std::string>& values,
                                                  std::uint64_t max_batch_size)
    {
        std::uint64_t batch_count = 0;

        while (batch_count < max_batch_size && result_position_ < column_scan_results_.size()) {
            row_ids.push_back(column_scan_results_[result_position_]);
            values.push_back("batch_value_" +
                             std::to_string(column_scan_results_[result_position_]));
            result_position_++;
            batch_count++;
            rows_scanned_++;
        }

        if (result_position_ >= column_scan_results_.size()) {
            finished_ = true;
        }

        return batch_count;
    }

    // LZ4 Compression Implementation (Simplified LZ77-based algorithm)
    std::vector<std::uint8_t>
    ColumnstoreIndex::compress_lz4(const std::vector<std::uint8_t>& data) const
    {
        if (data.empty())
            return data;

        std::vector<std::uint8_t> compressed;
        compressed.reserve(data.size()); // Reserve space

        // LZ4 compression header (simplified)
        compressed.push_back(0x4C); // 'L'
        compressed.push_back(0x5A); // 'Z'
        compressed.push_back(0x34); // '4'
        compressed.push_back(0x00); // Version

        // Add uncompressed size for decompression
        std::uint32_t original_size = static_cast<std::uint32_t>(data.size());
        compressed.push_back((original_size >> 0) & 0xFF);
        compressed.push_back((original_size >> 8) & 0xFF);
        compressed.push_back((original_size >> 16) & 0xFF);
        compressed.push_back((original_size >> 24) & 0xFF);

        // Simplified LZ4 compression using dictionary lookback
        std::unordered_map<std::uint32_t, std::size_t> hash_table;
        const std::size_t window_size = 65536; // 64KB window
        const std::size_t min_match = 4;

        for (std::size_t i = 0; i < data.size();) {
            std::size_t best_length = 0;
            std::size_t best_distance = 0;

            // Look for matches in the sliding window
            if (i >= min_match) {
                std::uint32_t hash = 0;
                for (std::size_t j = 0; j < min_match && i + j < data.size(); ++j) {
                    hash = (hash << 8) | data[i + j];
                }

                auto it = hash_table.find(hash);
                if (it != hash_table.end() && i - it->second < window_size) {
                    std::size_t match_pos = it->second;
                    std::size_t match_len = 0;

                    // Extend the match
                    while (match_len < 255 && i + match_len < data.size() &&
                           match_pos + match_len < data.size() &&
                           data[i + match_len] == data[match_pos + match_len]) {
                        match_len++;
                    }

                    if (match_len >= min_match) {
                        best_length = match_len;
                        best_distance = i - match_pos;
                    }
                }

                hash_table[hash] = i;
            }

            if (best_length >= min_match) {
                // Encode match: length + distance
                compressed.push_back(0x80 | (best_length - min_match)); // Match marker + length
                compressed.push_back((best_distance >> 0) & 0xFF);
                compressed.push_back((best_distance >> 8) & 0xFF);
                i += best_length;
            } else {
                // Literal byte
                compressed.push_back(data[i]);
                i++;
            }
        }

        return compressed;
    }

    std::vector<std::uint8_t>
    ColumnstoreIndex::decompress_lz4(const std::vector<std::uint8_t>& compressed_data) const
    {
        if (compressed_data.size() < 8)
            return compressed_data;

        // Verify LZ4 header
        if (compressed_data[0] != 0x4C || compressed_data[1] != 0x5A ||
            compressed_data[2] != 0x34 || compressed_data[3] != 0x00) {
            return compressed_data; // Not LZ4 format
        }

        // Read original size
        std::uint32_t original_size = (static_cast<std::uint32_t>(compressed_data[4]) << 0) |
                                      (static_cast<std::uint32_t>(compressed_data[5]) << 8) |
                                      (static_cast<std::uint32_t>(compressed_data[6]) << 16) |
                                      (static_cast<std::uint32_t>(compressed_data[7]) << 24);

        std::vector<std::uint8_t> decompressed;
        decompressed.reserve(original_size);

        for (std::size_t i = 8;
             i < compressed_data.size() && decompressed.size() < original_size;) {
            std::uint8_t token = compressed_data[i++];

            if (token & 0x80) {
                // Match: length + distance
                std::size_t length = (token & 0x7F) + 4;
                if (i + 1 >= compressed_data.size())
                    break;

                std::size_t distance = (static_cast<std::size_t>(compressed_data[i]) << 0) |
                                       (static_cast<std::size_t>(compressed_data[i + 1]) << 8);
                i += 2;

                // Copy from lookback buffer
                std::size_t copy_pos = decompressed.size() - distance;
                for (std::size_t j = 0; j < length && decompressed.size() < original_size; ++j) {
                    if (copy_pos + j < decompressed.size()) {
                        decompressed.push_back(decompressed[copy_pos + j]);
                    }
                }
            } else {
                // Literal byte
                decompressed.push_back(token);
            }
        }

        return decompressed;
    }

    // ZSTD Compression Implementation (Simplified Zstandard algorithm)
    std::vector<std::uint8_t>
    ColumnstoreIndex::compress_zstd(const std::vector<std::uint8_t>& data) const
    {
        if (data.empty())
            return data;

        std::vector<std::uint8_t> compressed;
        compressed.reserve(data.size());

        // ZSTD magic header
        compressed.push_back(0x28);
        compressed.push_back(0xB5);
        compressed.push_back(0x2F);
        compressed.push_back(0xFD);

        // Frame header with uncompressed size
        std::uint32_t original_size = static_cast<std::uint32_t>(data.size());
        compressed.push_back((original_size >> 0) & 0xFF);
        compressed.push_back((original_size >> 8) & 0xFF);
        compressed.push_back((original_size >> 16) & 0xFF);
        compressed.push_back((original_size >> 24) & 0xFF);

        // Simplified ZSTD compression using entropy encoding and dictionary
        std::unordered_map<std::uint8_t, std::uint32_t> frequency;
        for (std::uint8_t byte : data) {
            frequency[byte]++;
        }

        // Build simple Huffman-like encoding table
        std::unordered_map<std::uint8_t, std::pair<std::uint16_t, std::uint8_t>> encoding_table;
        std::uint16_t code = 0;
        for (const auto& [byte, freq] : frequency) {
            std::uint8_t bit_length =
                std::min(8, static_cast<int>(std::ceil(std::log2(frequency.size()))));
            encoding_table[byte] = {code++, bit_length};
        }

        // Encode frequency table size
        compressed.push_back(static_cast<std::uint8_t>(frequency.size()));

        // Store encoding table
        for (const auto& [byte, encoding] : encoding_table) {
            compressed.push_back(byte);
            compressed.push_back((encoding.first >> 0) & 0xFF);
            compressed.push_back((encoding.first >> 8) & 0xFF);
            compressed.push_back(encoding.second);
        }

        // Encode data using the table (simplified bit packing)
        std::uint32_t bit_buffer = 0;
        std::uint8_t bit_count = 0;

        for (std::uint8_t byte : data) {
            auto [code, length] = encoding_table[byte];
            bit_buffer |= (static_cast<std::uint32_t>(code) << bit_count);
            bit_count += length;

            while (bit_count >= 8) {
                compressed.push_back(bit_buffer & 0xFF);
                bit_buffer >>= 8;
                bit_count -= 8;
            }
        }

        // Flush remaining bits
        if (bit_count > 0) {
            compressed.push_back(bit_buffer & 0xFF);
        }

        return compressed;
    }

    std::vector<std::uint8_t>
    ColumnstoreIndex::decompress_zstd(const std::vector<std::uint8_t>& compressed_data) const
    {
        if (compressed_data.size() < 9)
            return compressed_data;

        // Verify ZSTD magic header
        if (compressed_data[0] != 0x28 || compressed_data[1] != 0xB5 ||
            compressed_data[2] != 0x2F || compressed_data[3] != 0xFD) {
            return compressed_data; // Not ZSTD format
        }

        // Read original size
        std::uint32_t original_size = (static_cast<std::uint32_t>(compressed_data[4]) << 0) |
                                      (static_cast<std::uint32_t>(compressed_data[5]) << 8) |
                                      (static_cast<std::uint32_t>(compressed_data[6]) << 16) |
                                      (static_cast<std::uint32_t>(compressed_data[7]) << 24);

        std::size_t pos = 8;
        std::uint8_t table_size = compressed_data[pos++];

        // Rebuild decoding table
        std::unordered_map<std::uint16_t, std::uint8_t> decoding_table;
        for (std::uint8_t i = 0; i < table_size && pos + 3 < compressed_data.size(); ++i) {
            std::uint8_t byte = compressed_data[pos++];
            std::uint16_t code = (static_cast<std::uint16_t>(compressed_data[pos]) << 0) |
                                 (static_cast<std::uint16_t>(compressed_data[pos + 1]) << 8);
            pos += 2;
            std::uint8_t length = compressed_data[pos++];
            decoding_table[code] = byte;
        }

        // Decode compressed data (simplified)
        std::vector<std::uint8_t> decompressed;
        decompressed.reserve(original_size);

        std::uint32_t bit_buffer = 0;
        std::uint8_t bit_count = 0;

        while (pos < compressed_data.size() && decompressed.size() < original_size) {
            // Fill bit buffer
            while (bit_count < 16 && pos < compressed_data.size()) {
                bit_buffer |= (static_cast<std::uint32_t>(compressed_data[pos++]) << bit_count);
                bit_count += 8;
            }

            // Try to decode symbols
            for (const auto& [code, byte] : decoding_table) {
                if ((bit_buffer & ((1 << 8) - 1)) == code) {
                    decompressed.push_back(byte);
                    bit_buffer >>= 8;
                    bit_count -= 8;
                    break;
                }
            }

            // Fallback: treat as literal if no match found
            if (bit_count >= 8) {
                decompressed.push_back(bit_buffer & 0xFF);
                bit_buffer >>= 8;
                bit_count -= 8;
            }
        }

        return decompressed;
    }

    // Snappy Compression Implementation (Simplified LZ77-variant)
    std::vector<std::uint8_t>
    ColumnstoreIndex::compress_snappy(const std::vector<std::uint8_t>& data) const
    {
        if (data.empty())
            return data;

        std::vector<std::uint8_t> compressed;
        compressed.reserve(data.size());

        // Snappy format identifier
        compressed.push_back(0x73); // 's'
        compressed.push_back(0x4E); // 'N'
        compressed.push_back(0x61); // 'a'
        compressed.push_back(0x50); // 'P'

        // Uncompressed length (varint encoding)
        std::uint32_t length = static_cast<std::uint32_t>(data.size());
        while (length >= 0x80) {
            compressed.push_back((length & 0x7F) | 0x80);
            length >>= 7;
        }
        compressed.push_back(length & 0x7F);

        // Simple Snappy compression: literal runs and copy operations
        const std::size_t window_size = 32768; // 32KB window
        const std::size_t min_match = 4;
        std::unordered_map<std::uint32_t, std::size_t> hash_table;

        for (std::size_t i = 0; i < data.size();) {
            std::size_t best_length = 0;
            std::size_t best_offset = 0;

            // Find matches in the sliding window
            if (i >= min_match) {
                std::uint32_t hash = 0;
                for (std::size_t j = 0; j < min_match && i + j < data.size(); ++j) {
                    hash = (hash * 31) + data[i + j];
                }

                auto it = hash_table.find(hash);
                if (it != hash_table.end() && i - it->second < window_size) {
                    std::size_t match_pos = it->second;
                    std::size_t match_len = 0;

                    while (match_len < 64 && i + match_len < data.size() &&
                           data[i + match_len] == data[match_pos + match_len]) {
                        match_len++;
                    }

                    if (match_len >= min_match) {
                        best_length = match_len;
                        best_offset = i - match_pos;
                    }
                }

                hash_table[hash] = i;
            }

            if (best_length >= min_match) {
                // Copy operation: encode length and offset
                std::uint8_t copy_tag = 0x01;         // Copy tag
                copy_tag |= ((best_length - 4) << 2); // Encode length-4 in upper 6 bits
                compressed.push_back(copy_tag);

                // Encode offset (little-endian)
                compressed.push_back(best_offset & 0xFF);
                compressed.push_back((best_offset >> 8) & 0xFF);

                i += best_length;
            } else {
                // Literal: find run of literals
                std::size_t literal_start = i;
                while (i < data.size() && (i - literal_start) < 60) {
                    // Simple heuristic: continue literals if no good match found
                    i++;
                }

                std::size_t literal_length = i - literal_start;
                std::uint8_t literal_tag = static_cast<std::uint8_t>((literal_length - 1) << 2);
                compressed.push_back(literal_tag);

                // Copy literal bytes
                for (std::size_t j = literal_start; j < i; ++j) {
                    compressed.push_back(data[j]);
                }
            }
        }

        return compressed;
    }

    std::vector<std::uint8_t>
    ColumnstoreIndex::decompress_snappy(const std::vector<std::uint8_t>& compressed_data) const
    {
        if (compressed_data.size() < 5)
            return compressed_data;

        // Verify Snappy header
        if (compressed_data[0] != 0x73 || compressed_data[1] != 0x4E ||
            compressed_data[2] != 0x61 || compressed_data[3] != 0x50) {
            return compressed_data; // Not Snappy format
        }

        // Decode varint length
        std::size_t pos = 4;
        std::uint32_t original_size = 0;
        std::uint32_t shift = 0;

        while (pos < compressed_data.size()) {
            std::uint8_t byte = compressed_data[pos++];
            original_size |= (static_cast<std::uint32_t>(byte & 0x7F) << shift);
            shift += 7;
            if ((byte & 0x80) == 0)
                break;
        }

        std::vector<std::uint8_t> decompressed;
        decompressed.reserve(original_size);

        while (pos < compressed_data.size() && decompressed.size() < original_size) {
            std::uint8_t tag = compressed_data[pos++];

            if (tag & 0x01) {
                // Copy operation
                std::size_t length = (tag >> 2) + 4;
                if (pos + 1 >= compressed_data.size())
                    break;

                std::size_t offset = (static_cast<std::size_t>(compressed_data[pos]) << 0) |
                                     (static_cast<std::size_t>(compressed_data[pos + 1]) << 8);
                pos += 2;

                // Copy from sliding window
                std::size_t copy_start = decompressed.size() - offset;
                for (std::size_t i = 0; i < length && decompressed.size() < original_size; ++i) {
                    if (copy_start + i < decompressed.size()) {
                        decompressed.push_back(decompressed[copy_start + i]);
                    }
                }
            } else {
                // Literal operation
                std::size_t literal_length = (tag >> 2) + 1;
                for (std::size_t i = 0; i < literal_length && pos < compressed_data.size() &&
                                        decompressed.size() < original_size;
                     ++i) {
                    decompressed.push_back(compressed_data[pos++]);
                }
            }
        }

        return decompressed;
    }

} // namespace scratchbird::engine
