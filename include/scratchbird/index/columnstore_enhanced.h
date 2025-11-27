// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-7: Columnstore Enhancements
//
// Adds advanced features to columnstore indexes:
// - Dictionary encoding for low-cardinality columns
// - Run-length encoding (RLE) for sequential values
// - Delta encoding for sorted/timestamp columns
// - Bit-packing for small integers
// - Zone maps for segment pruning
// - Vectorized execution support
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <vector>
#include <span>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace scratchbird::index {

// ============================================================================
// Encoding Types
// ============================================================================

enum class ColumnEncoding : uint8_t {
    PLAIN = 0,           // No encoding (raw values)
    DICTIONARY = 1,      // Dictionary encoding (low cardinality)
    RLE = 2,             // Run-length encoding
    DELTA = 3,           // Delta encoding
    DELTA_RLE = 4,       // Delta + RLE (for sorted data)
    BITPACKED = 5,       // Bit-packed integers
    FRAME_OF_REF = 6,    // Frame of reference (FOR)
    SPARSE = 7,          // Sparse encoding (mostly nulls)
};

// Compression algorithms
enum class CompressionType : uint8_t {
    NONE = 0,
    LZ4 = 1,
    ZSTD = 2,
    SNAPPY = 3,
};

// ============================================================================
// Zone Map (Min/Max Index)
// ============================================================================

struct ZoneMap {
    int64_t min_value = 0;
    int64_t max_value = 0;
    uint32_t null_count = 0;
    uint32_t row_count = 0;
    bool has_nulls = false;

    // For string types
    std::string min_string;
    std::string max_string;

    // Check if zone can possibly contain value
    bool mayContain(int64_t value) const {
        return value >= min_value && value <= max_value;
    }

    // Check if zone can possibly satisfy predicate
    enum class PredicateOp { EQ, NE, LT, LE, GT, GE };
    bool mayMatch(PredicateOp op, int64_t value) const {
        switch (op) {
            case PredicateOp::EQ:
                return value >= min_value && value <= max_value;
            case PredicateOp::NE:
                return !(min_value == max_value && min_value == value);
            case PredicateOp::LT:
                return min_value < value;
            case PredicateOp::LE:
                return min_value <= value;
            case PredicateOp::GT:
                return max_value > value;
            case PredicateOp::GE:
                return max_value >= value;
        }
        return true;
    }

    // Merge with another zone map
    void merge(const ZoneMap& other) {
        min_value = std::min(min_value, other.min_value);
        max_value = std::max(max_value, other.max_value);
        null_count += other.null_count;
        row_count += other.row_count;
        has_nulls = has_nulls || other.has_nulls;
    }
};

// ============================================================================
// Dictionary Encoding
// ============================================================================

template<typename T>
class Dictionary {
public:
    // Add value to dictionary, return index
    uint32_t encode(const T& value) {
        auto it = value_to_index_.find(value);
        if (it != value_to_index_.end()) {
            return it->second;
        }

        uint32_t index = static_cast<uint32_t>(values_.size());
        values_.push_back(value);
        value_to_index_[value] = index;
        return index;
    }

    // Decode index to value
    const T& decode(uint32_t index) const {
        return values_[index];
    }

    // Get dictionary size
    size_t size() const { return values_.size(); }

    // Check if value exists
    bool contains(const T& value) const {
        return value_to_index_.find(value) != value_to_index_.end();
    }

    // Get index for value (if exists)
    std::optional<uint32_t> getIndex(const T& value) const {
        auto it = value_to_index_.find(value);
        return it != value_to_index_.end() ?
               std::optional<uint32_t>(it->second) : std::nullopt;
    }

    // Get all values
    const std::vector<T>& values() const { return values_; }

    // Serialization
    std::vector<uint8_t> serialize() const;
    static Dictionary<T> deserialize(std::span<const uint8_t> data);

private:
    std::vector<T> values_;
    std::unordered_map<T, uint32_t> value_to_index_;
};

// ============================================================================
// Run-Length Encoding
// ============================================================================

template<typename T>
struct RLERun {
    T value;
    uint32_t count;
};

template<typename T>
class RLEEncoder {
public:
    void encode(std::span<const T> values) {
        runs_.clear();
        if (values.empty()) return;

        T current = values[0];
        uint32_t count = 1;

        for (size_t i = 1; i < values.size(); ++i) {
            if (values[i] == current) {
                count++;
            } else {
                runs_.push_back({current, count});
                current = values[i];
                count = 1;
            }
        }
        runs_.push_back({current, count});
    }

    std::vector<T> decode() const {
        std::vector<T> result;
        for (const auto& run : runs_) {
            for (uint32_t i = 0; i < run.count; ++i) {
                result.push_back(run.value);
            }
        }
        return result;
    }

    // Get value at position
    T get(size_t index) const {
        size_t pos = 0;
        for (const auto& run : runs_) {
            if (index < pos + run.count) {
                return run.value;
            }
            pos += run.count;
        }
        throw std::out_of_range("RLE index out of range");
    }

    const std::vector<RLERun<T>>& runs() const { return runs_; }

    // Compression ratio
    double compressionRatio(size_t original_count) const {
        if (original_count == 0) return 1.0;
        return static_cast<double>(runs_.size()) / original_count;
    }

private:
    std::vector<RLERun<T>> runs_;
};

// ============================================================================
// Delta Encoding
// ============================================================================

template<typename T>
class DeltaEncoder {
public:
    void encode(std::span<const T> values) {
        deltas_.clear();
        if (values.empty()) return;

        base_value_ = values[0];
        deltas_.reserve(values.size() - 1);

        for (size_t i = 1; i < values.size(); ++i) {
            deltas_.push_back(values[i] - values[i - 1]);
        }
    }

    std::vector<T> decode() const {
        std::vector<T> result;
        result.reserve(deltas_.size() + 1);
        result.push_back(base_value_);

        T current = base_value_;
        for (const auto& delta : deltas_) {
            current += delta;
            result.push_back(current);
        }
        return result;
    }

    T get(size_t index) const {
        if (index == 0) return base_value_;

        T current = base_value_;
        for (size_t i = 0; i < index && i < deltas_.size(); ++i) {
            current += deltas_[i];
        }
        return current;
    }

    T baseValue() const { return base_value_; }
    const std::vector<T>& deltas() const { return deltas_; }

    // Get max delta for bit width calculation
    T maxDelta() const {
        T max_val = 0;
        for (const auto& d : deltas_) {
            T abs_d = d >= 0 ? d : -d;
            max_val = std::max(max_val, abs_d);
        }
        return max_val;
    }

private:
    T base_value_{};
    std::vector<T> deltas_;
};

// ============================================================================
// Bit-Packing
// ============================================================================

class BitPacker {
public:
    // Determine minimum bits needed for value
    static uint8_t bitsRequired(uint64_t max_value) {
        if (max_value == 0) return 1;
        return 64 - __builtin_clzll(max_value);
    }

    // Pack values with specified bit width
    static std::vector<uint8_t> pack(std::span<const uint32_t> values, uint8_t bit_width);

    // Unpack values
    static std::vector<uint32_t> unpack(std::span<const uint8_t> packed,
                                        size_t num_values, uint8_t bit_width);

    // Get specific value from packed data
    static uint32_t getAt(std::span<const uint8_t> packed, size_t index, uint8_t bit_width);
};

// ============================================================================
// Column Segment (encoded data unit)
// ============================================================================

class ColumnSegment {
public:
    ColumnSegment() = default;

    // Initialize with raw data
    template<typename T>
    void initialize(std::span<const T> values, ColumnEncoding encoding = ColumnEncoding::PLAIN);

    // Get encoding type
    ColumnEncoding encoding() const { return encoding_; }

    // Get compression type
    CompressionType compression() const { return compression_; }

    // Get zone map
    const ZoneMap& zoneMap() const { return zone_map_; }

    // Get row count
    uint32_t rowCount() const { return row_count_; }

    // Get null bitmap (if any)
    const std::vector<uint8_t>& nullBitmap() const { return null_bitmap_; }

    // Check if value at index is null
    bool isNull(size_t index) const {
        if (null_bitmap_.empty()) return false;
        size_t byte_idx = index / 8;
        size_t bit_idx = index % 8;
        return byte_idx < null_bitmap_.size() && (null_bitmap_[byte_idx] & (1 << bit_idx));
    }

    // Get value at index (for scalar access)
    template<typename T>
    T getValue(size_t index) const;

    // Get batch of values (for vectorized access)
    template<typename T>
    std::vector<T> getValues(size_t start, size_t count) const;

    // Get encoded data
    const std::vector<uint8_t>& data() const { return data_; }

    // Get dictionary (if dictionary encoded)
    template<typename T>
    const Dictionary<T>* dictionary() const;

    // Compression statistics
    size_t uncompressedSize() const { return uncompressed_size_; }
    size_t compressedSize() const { return data_.size(); }
    double compressionRatio() const {
        return uncompressed_size_ > 0 ?
               static_cast<double>(data_.size()) / uncompressed_size_ : 1.0;
    }

    // Serialization
    std::vector<uint8_t> serialize() const;
    static ColumnSegment deserialize(std::span<const uint8_t> data);

private:
    ColumnEncoding encoding_ = ColumnEncoding::PLAIN;
    CompressionType compression_ = CompressionType::NONE;
    ZoneMap zone_map_;
    uint32_t row_count_ = 0;
    size_t uncompressed_size_ = 0;

    std::vector<uint8_t> data_;           // Encoded data
    std::vector<uint8_t> null_bitmap_;    // Null indicators

    // Type-specific storage
    std::shared_ptr<void> dictionary_;     // For dictionary encoding
    uint8_t bit_width_ = 0;                // For bit-packing
};

// ============================================================================
// Column Group (multiple segments for a column)
// ============================================================================

class ColumnGroup {
public:
    ColumnGroup(const std::string& column_name, size_t rows_per_segment = 65536);

    // Add data
    template<typename T>
    void append(std::span<const T> values, std::span<const bool> nulls = {});

    // Get segment count
    size_t segmentCount() const { return segments_.size(); }

    // Get total row count
    size_t rowCount() const { return total_rows_; }

    // Get segment by index
    const ColumnSegment& segment(size_t index) const { return segments_[index]; }

    // Prune segments based on predicate
    std::vector<size_t> pruneSegments(ZoneMap::PredicateOp op, int64_t value) const;

    // Get value at row
    template<typename T>
    T getValue(size_t row) const;

    // Get batch of values
    template<typename T>
    std::vector<T> getValues(size_t start, size_t count) const;

    // Column name
    const std::string& name() const { return column_name_; }

    // Encoding selection
    void setPreferredEncoding(ColumnEncoding encoding) { preferred_encoding_ = encoding; }

    // Compression selection
    void setCompression(CompressionType compression) { compression_ = compression; }

    // Statistics
    struct Stats {
        size_t total_rows = 0;
        size_t segment_count = 0;
        size_t uncompressed_bytes = 0;
        size_t compressed_bytes = 0;
        double avg_compression_ratio = 0.0;
    };
    Stats getStats() const;

private:
    std::string column_name_;
    size_t rows_per_segment_;
    size_t total_rows_ = 0;
    ColumnEncoding preferred_encoding_ = ColumnEncoding::PLAIN;
    CompressionType compression_ = CompressionType::NONE;

    std::vector<ColumnSegment> segments_;
    std::vector<ZoneMap> zone_maps_;  // One per segment

    // Helper to choose encoding based on data
    ColumnEncoding chooseEncoding(std::span<const int64_t> values) const;
};

// ============================================================================
// Vectorized Batch (for SIMD processing)
// ============================================================================

template<typename T>
class VectorizedBatch {
public:
    static constexpr size_t BATCH_SIZE = 1024;  // Typical L1 cache friendly

    VectorizedBatch() = default;
    explicit VectorizedBatch(size_t size) : size_(size) {
        values_.resize(size);
        valid_.resize(size, true);
    }

    // Load from column segment
    void load(const ColumnSegment& segment, size_t start, size_t count);

    // Values array (for SIMD)
    T* data() { return values_.data(); }
    const T* data() const { return values_.data(); }

    // Validity array
    bool* validData() { return valid_.data(); }
    const bool* validData() const { return valid_.data(); }

    // Size
    size_t size() const { return size_; }

    // Access
    T& operator[](size_t i) { return values_[i]; }
    const T& operator[](size_t i) const { return values_[i]; }

    bool isValid(size_t i) const { return valid_[i]; }
    void setValid(size_t i, bool v) { valid_[i] = v; }

    // Vectorized operations
    void add(const VectorizedBatch& other);
    void multiply(const VectorizedBatch& other);
    void filter(const std::vector<bool>& selection);

private:
    std::vector<T> values_;
    std::vector<bool> valid_;
    size_t size_ = 0;
};

// ============================================================================
// Encoding Selection Heuristics
// ============================================================================

class EncodingSelector {
public:
    struct Analysis {
        size_t cardinality = 0;         // Distinct values
        size_t run_count = 0;           // RLE runs
        bool is_sorted = false;
        double null_ratio = 0.0;
        int64_t min_value = 0;
        int64_t max_value = 0;
        size_t value_count = 0;
    };

    // Analyze data and return best encoding
    template<typename T>
    static ColumnEncoding analyze(std::span<const T> values, Analysis* out_analysis = nullptr);

    // Encoding decision thresholds
    static constexpr double DICT_CARDINALITY_RATIO = 0.1;  // Use dict if cardinality < 10%
    static constexpr double RLE_RUN_RATIO = 0.5;           // Use RLE if runs < 50% of values
    static constexpr double SPARSE_NULL_RATIO = 0.9;       // Use sparse if > 90% nulls
};

// ============================================================================
// Columnstore Manager
// ============================================================================

class ColumnstoreManager {
public:
    static ColumnstoreManager& getInstance();

    // Create column group
    std::shared_ptr<ColumnGroup> createColumnGroup(const std::string& name,
                                                    size_t rows_per_segment = 65536);

    // Register column group
    void registerColumnGroup(const std::string& table_name,
                             std::shared_ptr<ColumnGroup> group);

    // Get column group
    std::shared_ptr<ColumnGroup> getColumnGroup(const std::string& table_name,
                                                 const std::string& column_name);

    // Configuration
    void setDefaultEncoding(ColumnEncoding encoding) { default_encoding_ = encoding; }
    void setDefaultCompression(CompressionType compression) { default_compression_ = compression; }
    void setRowsPerSegment(size_t rows) { default_rows_per_segment_ = rows; }

private:
    ColumnstoreManager() = default;

    ColumnEncoding default_encoding_ = ColumnEncoding::PLAIN;
    CompressionType default_compression_ = CompressionType::NONE;
    size_t default_rows_per_segment_ = 65536;

    std::unordered_map<std::string, std::shared_ptr<ColumnGroup>> column_groups_;
};

} // namespace scratchbird::index
