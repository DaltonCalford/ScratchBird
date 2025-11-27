// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-8: HNSW Vector Quantization Implementation
//
// November 25, 2025

#include "scratchbird/index/vector_quantization.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>

namespace scratchbird::index {

// ============================================================================
// ScalarQuantizer8 Implementation
// ============================================================================

void ScalarQuantizer8::train(std::span<const float> data, size_t dimension) {
    dimension_ = dimension;
    mins_.resize(dimension, std::numeric_limits<float>::max());
    maxs_.resize(dimension, std::numeric_limits<float>::lowest());
    scales_.resize(dimension);

    size_t num_vectors = data.size() / dimension;

    // Find min/max per dimension
    for (size_t i = 0; i < num_vectors; ++i) {
        for (size_t d = 0; d < dimension; ++d) {
            float val = data[i * dimension + d];
            mins_[d] = std::min(mins_[d], val);
            maxs_[d] = std::max(maxs_[d], val);
        }
    }

    // Compute scales
    for (size_t d = 0; d < dimension; ++d) {
        float range = maxs_[d] - mins_[d];
        scales_[d] = range > 0 ? range / 255.0f : 1.0f;
    }
}

std::vector<uint8_t> ScalarQuantizer8::encode(std::span<const float> vector) const {
    std::vector<uint8_t> codes(dimension_);

    for (size_t d = 0; d < dimension_; ++d) {
        float normalized = (vector[d] - mins_[d]) / scales_[d];
        normalized = std::clamp(normalized, 0.0f, 255.0f);
        codes[d] = static_cast<uint8_t>(normalized + 0.5f);
    }

    return codes;
}

std::vector<float> ScalarQuantizer8::decode(std::span<const uint8_t> codes) const {
    std::vector<float> result(dimension_);

    for (size_t d = 0; d < dimension_; ++d) {
        result[d] = mins_[d] + codes[d] * scales_[d];
    }

    return result;
}

float ScalarQuantizer8::distance(std::span<const uint8_t> a, std::span<const uint8_t> b) const {
    float sum = 0.0f;
    for (size_t d = 0; d < dimension_; ++d) {
        float diff = (static_cast<float>(a[d]) - static_cast<float>(b[d])) * scales_[d];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

float ScalarQuantizer8::asymmetricDistance(std::span<const float> query,
                                           std::span<const uint8_t> stored) const {
    float sum = 0.0f;
    for (size_t d = 0; d < dimension_; ++d) {
        float stored_val = mins_[d] + stored[d] * scales_[d];
        float diff = query[d] - stored_val;
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

std::vector<uint8_t> ScalarQuantizer8::serialize() const {
    std::vector<uint8_t> data;
    size_t header_size = sizeof(size_t) + dimension_ * sizeof(float) * 3;
    data.resize(header_size);

    size_t offset = 0;
    std::memcpy(data.data() + offset, &dimension_, sizeof(size_t));
    offset += sizeof(size_t);

    std::memcpy(data.data() + offset, mins_.data(), dimension_ * sizeof(float));
    offset += dimension_ * sizeof(float);

    std::memcpy(data.data() + offset, maxs_.data(), dimension_ * sizeof(float));
    offset += dimension_ * sizeof(float);

    std::memcpy(data.data() + offset, scales_.data(), dimension_ * sizeof(float));

    return data;
}

ScalarQuantizer8 ScalarQuantizer8::deserialize(std::span<const uint8_t> data) {
    ScalarQuantizer8 sq;

    size_t offset = 0;
    std::memcpy(&sq.dimension_, data.data() + offset, sizeof(size_t));
    offset += sizeof(size_t);

    sq.mins_.resize(sq.dimension_);
    sq.maxs_.resize(sq.dimension_);
    sq.scales_.resize(sq.dimension_);

    std::memcpy(sq.mins_.data(), data.data() + offset, sq.dimension_ * sizeof(float));
    offset += sq.dimension_ * sizeof(float);

    std::memcpy(sq.maxs_.data(), data.data() + offset, sq.dimension_ * sizeof(float));
    offset += sq.dimension_ * sizeof(float);

    std::memcpy(sq.scales_.data(), data.data() + offset, sq.dimension_ * sizeof(float));

    return sq;
}

// ============================================================================
// ScalarQuantizer4 Implementation
// ============================================================================

void ScalarQuantizer4::train(std::span<const float> data, size_t dimension) {
    dimension_ = dimension;
    mins_.resize(dimension);
    scales_.resize(dimension);

    std::vector<float> maxs(dimension, std::numeric_limits<float>::lowest());
    std::fill(mins_.begin(), mins_.end(), std::numeric_limits<float>::max());

    size_t num_vectors = data.size() / dimension;

    for (size_t i = 0; i < num_vectors; ++i) {
        for (size_t d = 0; d < dimension; ++d) {
            float val = data[i * dimension + d];
            mins_[d] = std::min(mins_[d], val);
            maxs[d] = std::max(maxs[d], val);
        }
    }

    for (size_t d = 0; d < dimension; ++d) {
        float range = maxs[d] - mins_[d];
        scales_[d] = range > 0 ? range / 15.0f : 1.0f;
    }
}

std::vector<uint8_t> ScalarQuantizer4::encode(std::span<const float> vector) const {
    size_t num_bytes = (dimension_ + 1) / 2;
    std::vector<uint8_t> codes(num_bytes, 0);

    for (size_t d = 0; d < dimension_; ++d) {
        float normalized = (vector[d] - mins_[d]) / scales_[d];
        normalized = std::clamp(normalized, 0.0f, 15.0f);
        uint8_t code = static_cast<uint8_t>(normalized + 0.5f);

        size_t byte_idx = d / 2;
        if (d % 2 == 0) {
            codes[byte_idx] |= code;
        } else {
            codes[byte_idx] |= (code << 4);
        }
    }

    return codes;
}

std::vector<float> ScalarQuantizer4::decode(std::span<const uint8_t> codes) const {
    std::vector<float> result(dimension_);

    for (size_t d = 0; d < dimension_; ++d) {
        size_t byte_idx = d / 2;
        uint8_t code = (d % 2 == 0) ? (codes[byte_idx] & 0x0F) : (codes[byte_idx] >> 4);
        result[d] = mins_[d] + code * scales_[d];
    }

    return result;
}

float ScalarQuantizer4::asymmetricDistance(std::span<const float> query,
                                           std::span<const uint8_t> stored) const {
    float sum = 0.0f;

    for (size_t d = 0; d < dimension_; ++d) {
        size_t byte_idx = d / 2;
        uint8_t code = (d % 2 == 0) ? (stored[byte_idx] & 0x0F) : (stored[byte_idx] >> 4);
        float stored_val = mins_[d] + code * scales_[d];
        float diff = query[d] - stored_val;
        sum += diff * diff;
    }

    return std::sqrt(sum);
}

// ============================================================================
// ProductQuantizer Implementation
// ============================================================================

ProductQuantizer::ProductQuantizer(size_t dimension, size_t m, size_t ksub)
    : dimension_(dimension), m_(m), ksub_(ksub), dsub_(dimension / m) {

    if (dimension % m != 0) {
        // Pad to make divisible
        dsub_ = (dimension + m - 1) / m;
    }

    codebooks_.resize(m * ksub * dsub_, 0.0f);
}

core::Status ProductQuantizer::train(std::span<const float> data, size_t num_vectors,
                                    size_t max_iterations, ErrorContext* ctx) {
    if (num_vectors < ksub_) {
        if (ctx) {
            ctx->setError("Not enough training data for PQ");
        }
        return core::Status::INVALID_ARGUMENT;
    }

    // Train each subvector independently using k-means
    for (size_t m = 0; m < m_; ++m) {
        // Extract subvectors for this subspace
        std::vector<float> subvectors(num_vectors * dsub_);

        for (size_t i = 0; i < num_vectors; ++i) {
            size_t src_offset = i * dimension_ + m * dsub_;
            size_t dst_offset = i * dsub_;

            for (size_t j = 0; j < dsub_ && (m * dsub_ + j) < dimension_; ++j) {
                if (src_offset + j < data.size()) {
                    subvectors[dst_offset + j] = data[src_offset + j];
                }
            }
        }

        kmeansSubvector(subvectors, m, num_vectors, max_iterations);
    }

    return core::Status::OK;
}

void ProductQuantizer::kmeansSubvector(std::span<const float> subvectors,
                                       size_t subvector_idx,
                                       size_t num_vectors,
                                       size_t max_iterations) {
    // Initialize centroids randomly
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, num_vectors - 1);

    float* codebook = codebooks_.data() + subvector_idx * ksub_ * dsub_;

    // Random initialization
    std::vector<size_t> init_indices(ksub_);
    for (size_t k = 0; k < ksub_; ++k) {
        init_indices[k] = dist(gen);
        for (size_t d = 0; d < dsub_; ++d) {
            codebook[k * dsub_ + d] = subvectors[init_indices[k] * dsub_ + d];
        }
    }

    std::vector<size_t> assignments(num_vectors);
    std::vector<size_t> counts(ksub_);
    std::vector<float> new_centroids(ksub_ * dsub_);

    for (size_t iter = 0; iter < max_iterations; ++iter) {
        // Assign points to nearest centroid
        for (size_t i = 0; i < num_vectors; ++i) {
            float best_dist = std::numeric_limits<float>::max();
            size_t best_k = 0;

            for (size_t k = 0; k < ksub_; ++k) {
                float dist = 0.0f;
                for (size_t d = 0; d < dsub_; ++d) {
                    float diff = subvectors[i * dsub_ + d] - codebook[k * dsub_ + d];
                    dist += diff * diff;
                }

                if (dist < best_dist) {
                    best_dist = dist;
                    best_k = k;
                }
            }

            assignments[i] = best_k;
        }

        // Recompute centroids
        std::fill(new_centroids.begin(), new_centroids.end(), 0.0f);
        std::fill(counts.begin(), counts.end(), 0);

        for (size_t i = 0; i < num_vectors; ++i) {
            size_t k = assignments[i];
            counts[k]++;
            for (size_t d = 0; d < dsub_; ++d) {
                new_centroids[k * dsub_ + d] += subvectors[i * dsub_ + d];
            }
        }

        for (size_t k = 0; k < ksub_; ++k) {
            if (counts[k] > 0) {
                for (size_t d = 0; d < dsub_; ++d) {
                    codebook[k * dsub_ + d] = new_centroids[k * dsub_ + d] / counts[k];
                }
            }
        }
    }
}

std::vector<uint8_t> ProductQuantizer::encode(std::span<const float> vector) const {
    std::vector<uint8_t> codes(m_);

    for (size_t m = 0; m < m_; ++m) {
        const float* codebook = codebooks_.data() + m * ksub_ * dsub_;
        float best_dist = std::numeric_limits<float>::max();
        uint8_t best_k = 0;

        for (size_t k = 0; k < ksub_; ++k) {
            float dist = 0.0f;
            for (size_t d = 0; d < dsub_ && (m * dsub_ + d) < vector.size(); ++d) {
                float diff = vector[m * dsub_ + d] - codebook[k * dsub_ + d];
                dist += diff * diff;
            }

            if (dist < best_dist) {
                best_dist = dist;
                best_k = static_cast<uint8_t>(k);
            }
        }

        codes[m] = best_k;
    }

    return codes;
}

std::vector<float> ProductQuantizer::decode(std::span<const uint8_t> codes) const {
    std::vector<float> result(dimension_, 0.0f);

    for (size_t m = 0; m < m_; ++m) {
        const float* codebook = codebooks_.data() + m * ksub_ * dsub_;
        uint8_t k = codes[m];

        for (size_t d = 0; d < dsub_ && (m * dsub_ + d) < dimension_; ++d) {
            result[m * dsub_ + d] = codebook[k * dsub_ + d];
        }
    }

    return result;
}

std::vector<float> ProductQuantizer::computeDistanceTable(std::span<const float> query) const {
    // Table: m_ x ksub_ distances
    std::vector<float> table(m_ * ksub_);

    for (size_t m = 0; m < m_; ++m) {
        const float* codebook = codebooks_.data() + m * ksub_ * dsub_;

        for (size_t k = 0; k < ksub_; ++k) {
            float dist = 0.0f;
            for (size_t d = 0; d < dsub_ && (m * dsub_ + d) < query.size(); ++d) {
                float diff = query[m * dsub_ + d] - codebook[k * dsub_ + d];
                dist += diff * diff;
            }
            table[m * ksub_ + k] = dist;
        }
    }

    return table;
}

float ProductQuantizer::distanceWithTable(const std::vector<float>& table,
                                         std::span<const uint8_t> codes) const {
    float sum = 0.0f;
    for (size_t m = 0; m < m_; ++m) {
        sum += table[m * ksub_ + codes[m]];
    }
    return std::sqrt(sum);
}

float ProductQuantizer::asymmetricDistance(std::span<const float> query,
                                           std::span<const uint8_t> stored) const {
    auto table = computeDistanceTable(query);
    return distanceWithTable(table, stored);
}

// ============================================================================
// BinaryQuantizer Implementation
// ============================================================================

void BinaryQuantizer::train(std::span<const float> data, size_t dimension) {
    dimension_ = dimension;
    thresholds_.resize(dimension, 0.0f);

    size_t num_vectors = data.size() / dimension;
    if (num_vectors == 0) return;

    // Compute mean per dimension
    for (size_t i = 0; i < num_vectors; ++i) {
        for (size_t d = 0; d < dimension; ++d) {
            thresholds_[d] += data[i * dimension + d];
        }
    }

    for (size_t d = 0; d < dimension; ++d) {
        thresholds_[d] /= num_vectors;
    }
}

std::vector<uint8_t> BinaryQuantizer::encode(std::span<const float> vector) const {
    size_t num_bytes = (dimension_ + 7) / 8;
    std::vector<uint8_t> codes(num_bytes, 0);

    for (size_t d = 0; d < dimension_; ++d) {
        if (vector[d] >= thresholds_[d]) {
            codes[d / 8] |= (1 << (d % 8));
        }
    }

    return codes;
}

uint32_t BinaryQuantizer::hammingDistance(std::span<const uint8_t> a,
                                          std::span<const uint8_t> b) const {
    uint32_t dist = 0;
    size_t num_bytes = std::min(a.size(), b.size());

    for (size_t i = 0; i < num_bytes; ++i) {
        dist += std::popcount(static_cast<uint8_t>(a[i] ^ b[i]));
    }

    return dist;
}

float BinaryQuantizer::asymmetricDistance(std::span<const float> query,
                                          std::span<const uint8_t> stored) const {
    // Approximate: count agreements weighted by distance to threshold
    float sum = 0.0f;

    for (size_t d = 0; d < dimension_; ++d) {
        bool stored_bit = (stored[d / 8] >> (d % 8)) & 1;
        bool query_bit = query[d] >= thresholds_[d];

        if (stored_bit != query_bit) {
            // Disagreement - add squared distance
            float diff = query[d] - thresholds_[d];
            sum += diff * diff;
        }
    }

    return std::sqrt(sum);
}

// ============================================================================
// QuantizedVectorStore Implementation
// ============================================================================

QuantizedVectorStore::QuantizedVectorStore(QuantizationType type, size_t dimension)
    : type_(type), dimension_(dimension) {

    switch (type) {
        case QuantizationType::SQ8:
            sq8_ = std::make_unique<ScalarQuantizer8>();
            code_size_ = dimension;
            break;
        case QuantizationType::SQ4:
            sq4_ = std::make_unique<ScalarQuantizer4>();
            code_size_ = (dimension + 1) / 2;
            break;
        case QuantizationType::PQ:
            pq_ = std::make_unique<ProductQuantizer>(dimension);
            code_size_ = pq_->m();
            break;
        case QuantizationType::BINARY:
            binary_ = std::make_unique<BinaryQuantizer>();
            code_size_ = (dimension + 7) / 8;
            break;
        default:
            code_size_ = dimension * sizeof(float);
            break;
    }
}

core::Status QuantizedVectorStore::train(std::span<const float> sample_data,
                                         size_t num_vectors, ErrorContext* ctx) {
    switch (type_) {
        case QuantizationType::SQ8:
            sq8_->train(sample_data, dimension_);
            break;
        case QuantizationType::SQ4:
            sq4_->train(sample_data, dimension_);
            break;
        case QuantizationType::PQ:
            return pq_->train(sample_data, num_vectors, 25, ctx);
        case QuantizationType::BINARY:
            binary_->train(sample_data, dimension_);
            break;
        default:
            break;
    }
    return core::Status::OK;
}

size_t QuantizedVectorStore::add(std::span<const float> vector) {
    size_t index = num_vectors_++;

    std::vector<uint8_t> encoded;
    switch (type_) {
        case QuantizationType::SQ8:
            encoded = sq8_->encode(vector);
            break;
        case QuantizationType::SQ4:
            encoded = sq4_->encode(vector);
            break;
        case QuantizationType::PQ:
            encoded = pq_->encode(vector);
            break;
        case QuantizationType::BINARY:
            encoded = binary_->encode(vector);
            break;
        default:
            // No quantization - store raw
            encoded.resize(vector.size() * sizeof(float));
            std::memcpy(encoded.data(), vector.data(), encoded.size());
            break;
    }

    codes_.insert(codes_.end(), encoded.begin(), encoded.end());
    return index;
}

std::vector<float> QuantizedVectorStore::get(size_t index) const {
    if (index >= num_vectors_) return {};

    size_t offset = index * code_size_;
    std::span<const uint8_t> code(codes_.data() + offset, code_size_);

    switch (type_) {
        case QuantizationType::SQ8:
            return sq8_->decode(code);
        case QuantizationType::SQ4:
            return sq4_->decode(code);
        case QuantizationType::PQ:
            return pq_->decode(code);
        default: {
            std::vector<float> result(dimension_);
            std::memcpy(result.data(), code.data(), dimension_ * sizeof(float));
            return result;
        }
    }
}

std::vector<std::pair<size_t, float>> QuantizedVectorStore::search(
    std::span<const float> query, size_t k) const {

    std::vector<std::pair<size_t, float>> results;
    results.reserve(num_vectors_);

    for (size_t i = 0; i < num_vectors_; ++i) {
        size_t offset = i * code_size_;
        std::span<const uint8_t> code(codes_.data() + offset, code_size_);

        float dist = 0.0f;
        switch (type_) {
            case QuantizationType::SQ8:
                dist = sq8_->asymmetricDistance(query, code);
                break;
            case QuantizationType::SQ4:
                dist = sq4_->asymmetricDistance(query, code);
                break;
            case QuantizationType::PQ:
                dist = pq_->asymmetricDistance(query, code);
                break;
            case QuantizationType::BINARY:
                dist = binary_->asymmetricDistance(query, code);
                break;
            default: {
                // Exact distance
                for (size_t d = 0; d < dimension_; ++d) {
                    float diff = query[d] - reinterpret_cast<const float*>(code.data())[d];
                    dist += diff * diff;
                }
                dist = std::sqrt(dist);
                break;
            }
        }

        results.emplace_back(i, dist);
    }

    // Partial sort to get top k
    if (k < results.size()) {
        std::partial_sort(results.begin(), results.begin() + k, results.end(),
                          [](const auto& a, const auto& b) { return a.second < b.second; });
        results.resize(k);
    } else {
        std::sort(results.begin(), results.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });
    }

    return results;
}

QuantizationStats QuantizedVectorStore::stats() const {
    QuantizationStats s;
    s.original_bytes = num_vectors_ * dimension_ * sizeof(float);
    s.quantized_bytes = codes_.size();
    s.compression_ratio = s.original_bytes > 0 ?
                          static_cast<float>(s.quantized_bytes) / s.original_bytes : 1.0f;

    // Recall estimates based on quantization type
    switch (type_) {
        case QuantizationType::SQ8:
            s.recall_estimate = 0.99f;
            break;
        case QuantizationType::SQ4:
            s.recall_estimate = 0.95f;
            break;
        case QuantizationType::PQ:
            s.recall_estimate = 0.90f;
            break;
        case QuantizationType::BINARY:
            s.recall_estimate = 0.70f;
            break;
        default:
            s.recall_estimate = 1.0f;
    }

    return s;
}

size_t QuantizedVectorStore::codeSize() const {
    return code_size_;
}

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<QuantizedVectorStore> createQuantizedStore(
    const QuantizationConfig& config, size_t dimension) {

    auto store = std::make_unique<QuantizedVectorStore>(config.type, dimension);

    // Configure PQ parameters if applicable
    if (config.type == QuantizationType::PQ || config.type == QuantizationType::OPQ) {
        store = std::make_unique<QuantizedVectorStore>(config.type, dimension);
    }

    return store;
}

size_t estimateQuantizedMemory(const QuantizationConfig& config,
                               size_t dimension, size_t num_vectors) {
    size_t code_size = 0;

    switch (config.type) {
        case QuantizationType::NONE:
            code_size = dimension * sizeof(float);
            break;
        case QuantizationType::SQ8:
            code_size = dimension;
            break;
        case QuantizationType::SQ4:
            code_size = (dimension + 1) / 2;
            break;
        case QuantizationType::PQ:
        case QuantizationType::OPQ:
            code_size = config.pq_m;
            break;
        case QuantizationType::BINARY:
            code_size = (dimension + 7) / 8;
            break;
    }

    return num_vectors * code_size;
}

} // namespace scratchbird::index
