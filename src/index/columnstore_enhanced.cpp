/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-7: Columnstore Enhancements Implementation
//
// November 25, 2025

#include "scratchbird/index/columnstore_enhanced.h"
#include <algorithm>
#include <cstring>
#include <set>

namespace scratchbird::index {

// ============================================================================
// BitPacker Implementation
// ============================================================================

std::vector<uint8_t> BitPacker::pack(std::span<const uint32_t> values, uint8_t bit_width) {
    if (bit_width == 0 || values.empty()) return {};

    size_t total_bits = values.size() * bit_width;
    size_t num_bytes = (total_bits + 7) / 8;
    std::vector<uint8_t> packed(num_bytes, 0);

    size_t bit_pos = 0;
    for (uint32_t value : values) {
        // Write bit_width bits at bit_pos
        size_t remaining = bit_width;
        uint32_t val = value;

        while (remaining > 0) {
            size_t byte_idx = bit_pos / 8;
            size_t bit_in_byte = bit_pos % 8;
            size_t bits_to_write = std::min(remaining, 8 - bit_in_byte);

            uint8_t mask = (1 << bits_to_write) - 1;
            packed[byte_idx] |= ((val & mask) << bit_in_byte);

            val >>= bits_to_write;
            bit_pos += bits_to_write;
            remaining -= bits_to_write;
        }
    }

    return packed;
}

std::vector<uint32_t> BitPacker::unpack(std::span<const uint8_t> packed,
                                        size_t num_values, uint8_t bit_width) {
    if (bit_width == 0 || num_values == 0) return {};

    std::vector<uint32_t> values(num_values);

    size_t bit_pos = 0;
    for (size_t i = 0; i < num_values; ++i) {
        values[i] = getAt(packed, i, bit_width);
        bit_pos += bit_width;
    }

    return values;
}

uint32_t BitPacker::getAt(std::span<const uint8_t> packed, size_t index, uint8_t bit_width) {
    size_t bit_pos = index * bit_width;
    uint32_t value = 0;
    size_t remaining = bit_width;
    size_t value_bit = 0;

    while (remaining > 0) {
        size_t byte_idx = bit_pos / 8;
        size_t bit_in_byte = bit_pos % 8;
        size_t bits_to_read = std::min(remaining, 8 - bit_in_byte);

        if (byte_idx >= packed.size()) break;

        uint8_t mask = (1 << bits_to_read) - 1;
        uint32_t bits = (packed[byte_idx] >> bit_in_byte) & mask;
        value |= (bits << value_bit);

        bit_pos += bits_to_read;
        value_bit += bits_to_read;
        remaining -= bits_to_read;
    }

    return value;
}

// ============================================================================
// ColumnSegment Implementation
// ============================================================================

template<>
void ColumnSegment::initialize(std::span<const int64_t> values, ColumnEncoding encoding) {
    row_count_ = values.size();
    uncompressed_size_ = values.size() * sizeof(int64_t);
    encoding_ = encoding;

    // Build zone map
    if (!values.empty()) {
        zone_map_.min_value = values[0];
        zone_map_.max_value = values[0];
        zone_map_.row_count = values.size();

        for (int64_t v : values) {
            zone_map_.min_value = std::min(zone_map_.min_value, v);
            zone_map_.max_value = std::max(zone_map_.max_value, v);
        }
    }

    // Encode based on type
    switch (encoding) {
        case ColumnEncoding::PLAIN: {
            data_.resize(values.size() * sizeof(int64_t));
            std::memcpy(data_.data(), values.data(), data_.size());
            break;
        }

        case ColumnEncoding::DICTIONARY: {
            auto dict = std::make_shared<Dictionary<int64_t>>();
            std::vector<uint32_t> indices;
            indices.reserve(values.size());

            for (int64_t v : values) {
                indices.push_back(dict->encode(v));
            }

            // Store indices with bit-packing
            uint8_t bit_width = BitPacker::bitsRequired(dict->size());
            data_ = BitPacker::pack(indices, bit_width);
            bit_width_ = bit_width;
            dictionary_ = dict;
            break;
        }

        case ColumnEncoding::RLE: {
            RLEEncoder<int64_t> encoder;
            encoder.encode(values);

            // Serialize runs
            const auto& runs = encoder.runs();
            data_.resize(runs.size() * (sizeof(int64_t) + sizeof(uint32_t)));
            size_t offset = 0;
            for (const auto& run : runs) {
                std::memcpy(data_.data() + offset, &run.value, sizeof(int64_t));
                offset += sizeof(int64_t);
                std::memcpy(data_.data() + offset, &run.count, sizeof(uint32_t));
                offset += sizeof(uint32_t);
            }
            break;
        }

        case ColumnEncoding::DELTA: {
            DeltaEncoder<int64_t> encoder;
            encoder.encode(values);

            // Store base value + deltas
            data_.resize(sizeof(int64_t) + encoder.deltas().size() * sizeof(int64_t));
            int64_t base = encoder.baseValue();
            std::memcpy(data_.data(), &base, sizeof(int64_t));
            std::memcpy(data_.data() + sizeof(int64_t),
                        encoder.deltas().data(),
                        encoder.deltas().size() * sizeof(int64_t));
            break;
        }

        case ColumnEncoding::BITPACKED: {
            // Shift to positive range for bit-packing
            uint64_t offset = zone_map_.min_value < 0 ?
                              static_cast<uint64_t>(-zone_map_.min_value) : 0;
            uint64_t max_shifted = zone_map_.max_value + offset;
            bit_width_ = BitPacker::bitsRequired(max_shifted);

            std::vector<uint32_t> shifted;
            shifted.reserve(values.size());
            for (int64_t v : values) {
                shifted.push_back(static_cast<uint32_t>(v + offset));
            }

            // Store offset in first 8 bytes
            data_.resize(sizeof(uint64_t));
            std::memcpy(data_.data(), &offset, sizeof(uint64_t));

            auto packed = BitPacker::pack(shifted, bit_width_);
            data_.insert(data_.end(), packed.begin(), packed.end());
            break;
        }

        default:
            // Fall back to PLAIN
            data_.resize(values.size() * sizeof(int64_t));
            std::memcpy(data_.data(), values.data(), data_.size());
            break;
    }
}

template<>
void ColumnSegment::initialize(std::span<const int32_t> values, ColumnEncoding encoding) {
    // Convert to int64 and use that implementation
    std::vector<int64_t> values64(values.begin(), values.end());
    initialize<int64_t>(values64, encoding);
}

template<>
void ColumnSegment::initialize(std::span<const double> values, ColumnEncoding encoding) {
    row_count_ = values.size();
    uncompressed_size_ = values.size() * sizeof(double);
    encoding_ = ColumnEncoding::PLAIN;  // Floats use plain encoding

    // Build zone map (cast to int64 for min/max)
    if (!values.empty()) {
        double min_d = values[0];
        double max_d = values[0];

        for (double v : values) {
            min_d = std::min(min_d, v);
            max_d = std::max(max_d, v);
        }

        zone_map_.min_value = static_cast<int64_t>(min_d);
        zone_map_.max_value = static_cast<int64_t>(max_d);
        zone_map_.row_count = values.size();
    }

    data_.resize(values.size() * sizeof(double));
    std::memcpy(data_.data(), values.data(), data_.size());
}

template<>
int64_t ColumnSegment::getValue(size_t index) const {
    if (index >= row_count_) {
        throw std::out_of_range("Segment index out of range");
    }

    switch (encoding_) {
        case ColumnEncoding::PLAIN: {
            int64_t value;
            std::memcpy(&value, data_.data() + index * sizeof(int64_t), sizeof(int64_t));
            return value;
        }

        case ColumnEncoding::DICTIONARY: {
            auto* dict = static_cast<Dictionary<int64_t>*>(dictionary_.get());
            uint32_t dict_idx = BitPacker::getAt(data_, index, bit_width_);
            return dict->decode(dict_idx);
        }

        case ColumnEncoding::RLE: {
            size_t pos = 0;
            size_t offset = 0;
            while (offset < data_.size()) {
                int64_t value;
                uint32_t count;
                std::memcpy(&value, data_.data() + offset, sizeof(int64_t));
                offset += sizeof(int64_t);
                std::memcpy(&count, data_.data() + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);

                if (index < pos + count) {
                    return value;
                }
                pos += count;
            }
            throw std::out_of_range("RLE decode failed");
        }

        case ColumnEncoding::DELTA: {
            int64_t base;
            std::memcpy(&base, data_.data(), sizeof(int64_t));

            int64_t current = base;
            for (size_t i = 0; i < index; ++i) {
                int64_t delta;
                std::memcpy(&delta, data_.data() + sizeof(int64_t) + i * sizeof(int64_t),
                            sizeof(int64_t));
                current += delta;
            }
            return current;
        }

        case ColumnEncoding::BITPACKED: {
            uint64_t offset;
            std::memcpy(&offset, data_.data(), sizeof(uint64_t));
            std::span<const uint8_t> packed(data_.data() + sizeof(uint64_t),
                                             data_.size() - sizeof(uint64_t));
            uint32_t shifted = BitPacker::getAt(packed, index, bit_width_);
            return static_cast<int64_t>(shifted) - static_cast<int64_t>(offset);
        }

        default:
            throw std::runtime_error("Unsupported encoding");
    }
}

template<>
std::vector<int64_t> ColumnSegment::getValues(size_t start, size_t count) const {
    std::vector<int64_t> result;
    result.reserve(count);

    size_t end = std::min(start + count, static_cast<size_t>(row_count_));
    for (size_t i = start; i < end; ++i) {
        result.push_back(getValue<int64_t>(i));
    }

    return result;
}

std::vector<uint8_t> ColumnSegment::serialize() const {
    std::vector<uint8_t> result;

    // Header
    result.push_back(static_cast<uint8_t>(encoding_));
    result.push_back(static_cast<uint8_t>(compression_));
    result.push_back(bit_width_);

    // Row count
    result.resize(result.size() + sizeof(uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(uint32_t), &row_count_, sizeof(uint32_t));

    // Zone map
    result.resize(result.size() + sizeof(ZoneMap));
    std::memcpy(result.data() + result.size() - sizeof(ZoneMap), &zone_map_, sizeof(ZoneMap));

    // Data size and data
    uint32_t data_size = data_.size();
    result.resize(result.size() + sizeof(uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(uint32_t), &data_size, sizeof(uint32_t));
    result.insert(result.end(), data_.begin(), data_.end());

    // Null bitmap
    uint32_t null_size = null_bitmap_.size();
    result.resize(result.size() + sizeof(uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(uint32_t), &null_size, sizeof(uint32_t));
    result.insert(result.end(), null_bitmap_.begin(), null_bitmap_.end());

    return result;
}

ColumnSegment ColumnSegment::deserialize(std::span<const uint8_t> data) {
    ColumnSegment segment;
    size_t offset = 0;

    // Header
    segment.encoding_ = static_cast<ColumnEncoding>(data[offset++]);
    segment.compression_ = static_cast<CompressionType>(data[offset++]);
    segment.bit_width_ = data[offset++];

    // Row count
    std::memcpy(&segment.row_count_, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Zone map
    std::memcpy(&segment.zone_map_, data.data() + offset, sizeof(ZoneMap));
    offset += sizeof(ZoneMap);

    // Data
    uint32_t data_size;
    std::memcpy(&data_size, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    segment.data_.assign(data.data() + offset, data.data() + offset + data_size);
    offset += data_size;

    // Null bitmap
    uint32_t null_size;
    std::memcpy(&null_size, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    segment.null_bitmap_.assign(data.data() + offset, data.data() + offset + null_size);

    return segment;
}

// ============================================================================
// ColumnGroup Implementation
// ============================================================================

ColumnGroup::ColumnGroup(const std::string& column_name, size_t rows_per_segment)
    : column_name_(column_name), rows_per_segment_(rows_per_segment) {
}

template<>
void ColumnGroup::append(std::span<const int64_t> values, std::span<const bool> nulls) {
    size_t offset = 0;
    while (offset < values.size()) {
        size_t chunk_size = std::min(rows_per_segment_, values.size() - offset);

        std::span<const int64_t> chunk(values.data() + offset, chunk_size);
        ColumnEncoding encoding = chooseEncoding(chunk);

        ColumnSegment segment;
        segment.initialize(chunk, encoding);
        segments_.push_back(std::move(segment));
        zone_maps_.push_back(segments_.back().zoneMap());

        offset += chunk_size;
        total_rows_ += chunk_size;
    }
}

ColumnEncoding ColumnGroup::chooseEncoding(std::span<const int64_t> values) const {
    if (preferred_encoding_ != ColumnEncoding::PLAIN) {
        return preferred_encoding_;
    }

    // Auto-select encoding based on data characteristics
    if (values.empty()) return ColumnEncoding::PLAIN;

    // Check cardinality
    std::set<int64_t> unique_values(values.begin(), values.end());
    double cardinality_ratio = static_cast<double>(unique_values.size()) / values.size();

    if (cardinality_ratio < EncodingSelector::DICT_CARDINALITY_RATIO) {
        return ColumnEncoding::DICTIONARY;
    }

    // Check if sorted/nearly sorted (good for delta)
    bool is_sorted = std::is_sorted(values.begin(), values.end());
    if (is_sorted) {
        return ColumnEncoding::DELTA;
    }

    // Check RLE potential
    size_t run_count = 1;
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i] != values[i - 1]) {
            run_count++;
        }
    }
    double run_ratio = static_cast<double>(run_count) / values.size();
    if (run_ratio < EncodingSelector::RLE_RUN_RATIO) {
        return ColumnEncoding::RLE;
    }

    // Check if bit-packing would help
    int64_t min_val = *std::min_element(values.begin(), values.end());
    int64_t max_val = *std::max_element(values.begin(), values.end());
    uint64_t range = max_val - min_val;

    if (BitPacker::bitsRequired(range) <= 32) {
        return ColumnEncoding::BITPACKED;
    }

    return ColumnEncoding::PLAIN;
}

std::vector<size_t> ColumnGroup::pruneSegments(ZoneMap::PredicateOp op, int64_t value) const {
    std::vector<size_t> matching;

    for (size_t i = 0; i < zone_maps_.size(); ++i) {
        if (zone_maps_[i].mayMatch(op, value)) {
            matching.push_back(i);
        }
    }

    return matching;
}

template<>
int64_t ColumnGroup::getValue(size_t row) const {
    if (row >= total_rows_) {
        throw std::out_of_range("Row index out of range");
    }

    // Find segment
    size_t segment_idx = row / rows_per_segment_;
    size_t row_in_segment = row % rows_per_segment_;

    return segments_[segment_idx].getValue<int64_t>(row_in_segment);
}

ColumnGroup::Stats ColumnGroup::getStats() const {
    Stats stats;
    stats.total_rows = total_rows_;
    stats.segment_count = segments_.size();

    for (const auto& segment : segments_) {
        stats.uncompressed_bytes += segment.uncompressedSize();
        stats.compressed_bytes += segment.compressedSize();
    }

    stats.avg_compression_ratio = stats.uncompressed_bytes > 0 ?
        static_cast<double>(stats.compressed_bytes) / stats.uncompressed_bytes : 1.0;

    return stats;
}

// ============================================================================
// EncodingSelector Implementation
// ============================================================================

template<>
ColumnEncoding EncodingSelector::analyze(std::span<const int64_t> values, Analysis* out) {
    Analysis analysis;
    analysis.value_count = values.size();

    if (values.empty()) {
        if (out) *out = analysis;
        return ColumnEncoding::PLAIN;
    }

    // Compute statistics
    std::set<int64_t> unique_values(values.begin(), values.end());
    analysis.cardinality = unique_values.size();
    analysis.min_value = *unique_values.begin();
    analysis.max_value = *unique_values.rbegin();
    analysis.is_sorted = std::is_sorted(values.begin(), values.end());

    // Count runs
    analysis.run_count = 1;
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i] != values[i - 1]) {
            analysis.run_count++;
        }
    }

    if (out) *out = analysis;

    // Decision logic
    double cardinality_ratio = static_cast<double>(analysis.cardinality) / values.size();
    double run_ratio = static_cast<double>(analysis.run_count) / values.size();

    if (cardinality_ratio < DICT_CARDINALITY_RATIO) {
        return ColumnEncoding::DICTIONARY;
    }

    if (analysis.is_sorted) {
        if (run_ratio < RLE_RUN_RATIO) {
            return ColumnEncoding::DELTA_RLE;
        }
        return ColumnEncoding::DELTA;
    }

    if (run_ratio < RLE_RUN_RATIO) {
        return ColumnEncoding::RLE;
    }

    return ColumnEncoding::PLAIN;
}

// ============================================================================
// ColumnstoreManager Implementation
// ============================================================================

ColumnstoreManager& ColumnstoreManager::getInstance() {
    static ColumnstoreManager instance;
    return instance;
}

std::shared_ptr<ColumnGroup> ColumnstoreManager::createColumnGroup(
    const std::string& name, size_t rows_per_segment) {

    return std::make_shared<ColumnGroup>(name,
        rows_per_segment > 0 ? rows_per_segment : default_rows_per_segment_);
}

void ColumnstoreManager::registerColumnGroup(const std::string& table_name,
                                              std::shared_ptr<ColumnGroup> group) {
    std::string key = table_name + "." + group->name();
    column_groups_[key] = std::move(group);
}

std::shared_ptr<ColumnGroup> ColumnstoreManager::getColumnGroup(
    const std::string& table_name, const std::string& column_name) {

    std::string key = table_name + "." + column_name;
    auto it = column_groups_.find(key);
    return it != column_groups_.end() ? it->second : nullptr;
}

// ============================================================================
// VectorizedBatch Implementation
// ============================================================================

template<typename T>
void VectorizedBatch<T>::load(const ColumnSegment& segment, size_t start, size_t count) {
    size_t actual_count = std::min(count, static_cast<size_t>(segment.rowCount()) - start);
    values_.resize(actual_count);
    valid_.resize(actual_count);
    size_ = actual_count;

    for (size_t i = 0; i < actual_count; ++i) {
        if (segment.isNull(start + i)) {
            valid_[i] = false;
            values_[i] = T{};
        } else {
            valid_[i] = true;
            values_[i] = segment.getValue<T>(start + i);
        }
    }
}

template<typename T>
void VectorizedBatch<T>::add(const VectorizedBatch& other) {
    size_t n = std::min(size_, other.size_);
    for (size_t i = 0; i < n; ++i) {
        if (valid_[i] && other.valid_[i]) {
            values_[i] += other.values_[i];
        } else {
            valid_[i] = false;
        }
    }
}

template<typename T>
void VectorizedBatch<T>::multiply(const VectorizedBatch& other) {
    size_t n = std::min(size_, other.size_);
    for (size_t i = 0; i < n; ++i) {
        if (valid_[i] && other.valid_[i]) {
            values_[i] *= other.values_[i];
        } else {
            valid_[i] = false;
        }
    }
}

template<typename T>
void VectorizedBatch<T>::filter(const std::vector<bool>& selection) {
    std::vector<T> new_values;
    std::vector<bool> new_valid;

    for (size_t i = 0; i < size_ && i < selection.size(); ++i) {
        if (selection[i]) {
            new_values.push_back(values_[i]);
            new_valid.push_back(valid_[i]);
        }
    }

    values_ = std::move(new_values);
    valid_ = std::move(new_valid);
    size_ = values_.size();
}

// Explicit template instantiations
template class VectorizedBatch<int64_t>;
template class VectorizedBatch<int32_t>;
template class VectorizedBatch<double>;
template class VectorizedBatch<float>;

} // namespace scratchbird::index
