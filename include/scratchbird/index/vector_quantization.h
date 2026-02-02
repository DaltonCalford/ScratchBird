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
// P3-8: HNSW Vector Quantization
//
// Provides 4-8x memory reduction for vector indexes through quantization:
// - Scalar Quantization (SQ): int8/int4 compression
// - Product Quantization (PQ): subvector clustering
// - Optimized Product Quantization (OPQ): learned rotation
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <vector>
#include <cstdint>
#include <span>
#include <memory>
#include <random>

namespace scratchbird::index {

// ============================================================================
// Quantization Types
// ============================================================================

enum class QuantizationType : uint8_t {
    NONE = 0,          // No quantization (float32)
    SQ8 = 1,           // Scalar quantization to int8
    SQ4 = 2,           // Scalar quantization to int4 (packed)
    PQ = 3,            // Product quantization
    OPQ = 4,           // Optimized product quantization
    BINARY = 5,        // Binary quantization (1-bit)
};

// Quantization statistics
struct QuantizationStats {
    size_t original_bytes = 0;      // Unquantized memory
    size_t quantized_bytes = 0;     // Quantized memory
    float compression_ratio = 1.0f;
    float recall_estimate = 1.0f;   // Estimated recall vs exact
    uint32_t codebook_bytes = 0;    // PQ codebook size
};

// ============================================================================
// Scalar Quantization (SQ)
// ============================================================================

// SQ8: Quantize each float to 8-bit integer
class ScalarQuantizer8 {
public:
    ScalarQuantizer8() = default;

    // Train quantizer on sample data to determine min/max
    void train(std::span<const float> data, size_t dimension);

    // Quantize a vector
    std::vector<uint8_t> encode(std::span<const float> vector) const;

    // Dequantize back to float
    std::vector<float> decode(std::span<const uint8_t> codes) const;

    // Compute distance in quantized space (approximate)
    float distance(std::span<const uint8_t> a, std::span<const uint8_t> b) const;

    // Direct distance computation (query is float, stored is quantized)
    float asymmetricDistance(std::span<const float> query,
                            std::span<const uint8_t> stored) const;

    // Get min/max per dimension
    const std::vector<float>& mins() const { return mins_; }
    const std::vector<float>& maxs() const { return maxs_; }

    // Serialization
    std::vector<uint8_t> serialize() const;
    static ScalarQuantizer8 deserialize(std::span<const uint8_t> data);

private:
    std::vector<float> mins_;       // Minimum value per dimension
    std::vector<float> maxs_;       // Maximum value per dimension
    std::vector<float> scales_;     // (max - min) / 255
    size_t dimension_ = 0;
};

// SQ4: Quantize each float to 4-bit integer (2 values per byte)
class ScalarQuantizer4 {
public:
    ScalarQuantizer4() = default;

    void train(std::span<const float> data, size_t dimension);

    // Returns packed format: 2 values per byte
    std::vector<uint8_t> encode(std::span<const float> vector) const;
    std::vector<float> decode(std::span<const uint8_t> codes) const;

    float asymmetricDistance(std::span<const float> query,
                            std::span<const uint8_t> stored) const;

    std::vector<uint8_t> serialize() const;
    static ScalarQuantizer4 deserialize(std::span<const uint8_t> data);

private:
    std::vector<float> mins_;
    std::vector<float> scales_;     // (max - min) / 15
    size_t dimension_ = 0;
};

// ============================================================================
// Product Quantization (PQ)
// ============================================================================

// Divide vector into M subvectors, each quantized to ksub centroids
class ProductQuantizer {
public:
    // Default: 8 subvectors, 256 centroids each
    static constexpr size_t DEFAULT_M = 8;
    static constexpr size_t DEFAULT_KSUB = 256;

    ProductQuantizer(size_t dimension, size_t m = DEFAULT_M, size_t ksub = DEFAULT_KSUB);

    // Train codebooks using k-means clustering
    core::Status train(std::span<const float> data, size_t num_vectors,
                      size_t max_iterations = 25, ErrorContext* ctx = nullptr);

    // Encode vector to M bytes (one code per subvector)
    std::vector<uint8_t> encode(std::span<const float> vector) const;

    // Decode back to approximate float vector
    std::vector<float> decode(std::span<const uint8_t> codes) const;

    // Compute distance using precomputed distance table
    float distance(std::span<const uint8_t> a, std::span<const uint8_t> b) const;

    // Asymmetric distance: exact query, quantized stored
    float asymmetricDistance(std::span<const float> query,
                            std::span<const uint8_t> stored) const;

    // Precompute distance table for a query (speeds up multiple comparisons)
    std::vector<float> computeDistanceTable(std::span<const float> query) const;

    // Distance using precomputed table
    float distanceWithTable(const std::vector<float>& table,
                           std::span<const uint8_t> codes) const;

    // Accessors
    size_t dimension() const { return dimension_; }
    size_t m() const { return m_; }
    size_t ksub() const { return ksub_; }
    size_t dsub() const { return dsub_; }
    size_t codeSize() const { return m_; }  // Bytes per encoded vector

    // Get codebook for subvector i
    std::span<const float> codebook(size_t i) const {
        return std::span<const float>(codebooks_.data() + i * ksub_ * dsub_, ksub_ * dsub_);
    }

    // Serialization
    std::vector<uint8_t> serialize() const;
    static ProductQuantizer deserialize(std::span<const uint8_t> data);

private:
    size_t dimension_;      // Original vector dimension
    size_t m_;              // Number of subvectors
    size_t ksub_;           // Centroids per subvector
    size_t dsub_;           // Dimension per subvector (dimension_ / m_)

    // Codebooks: m_ codebooks, each with ksub_ centroids of dimension dsub_
    // Layout: [m0_c0, m0_c1, ..., m0_ck, m1_c0, m1_c1, ...]
    std::vector<float> codebooks_;

    // K-means helper
    void kmeansSubvector(std::span<const float> subvectors, size_t subvector_idx,
                         size_t num_vectors, size_t max_iterations);
};

// ============================================================================
// Optimized Product Quantization (OPQ)
// ============================================================================

// PQ with learned rotation matrix for better quantization
class OptimizedProductQuantizer {
public:
    OptimizedProductQuantizer(size_t dimension, size_t m = 8, size_t ksub = 256);

    // Train both rotation matrix and PQ codebooks
    core::Status train(std::span<const float> data, size_t num_vectors,
                      size_t max_iterations = 25, size_t opq_iterations = 10,
                      ErrorContext* ctx = nullptr);

    // Encode: rotate then PQ encode
    std::vector<uint8_t> encode(std::span<const float> vector) const;

    // Decode: PQ decode then inverse rotate
    std::vector<float> decode(std::span<const uint8_t> codes) const;

    // Asymmetric distance
    float asymmetricDistance(std::span<const float> query,
                            std::span<const uint8_t> stored) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    static OptimizedProductQuantizer deserialize(std::span<const uint8_t> data);

private:
    size_t dimension_;
    std::vector<float> rotation_matrix_;  // dimension_ x dimension_
    std::unique_ptr<ProductQuantizer> pq_;

    void applyRotation(std::span<const float> input, std::span<float> output) const;
    void applyInverseRotation(std::span<const float> input, std::span<float> output) const;
    void trainRotation(std::span<const float> data, size_t num_vectors,
                       size_t iterations);
};

// ============================================================================
// Binary Quantization
// ============================================================================

// Extreme compression: 1 bit per dimension
class BinaryQuantizer {
public:
    BinaryQuantizer() = default;

    // Train: compute mean per dimension (threshold for binarization)
    void train(std::span<const float> data, size_t dimension);

    // Encode to bits (dimension bits, packed into bytes)
    std::vector<uint8_t> encode(std::span<const float> vector) const;

    // Hamming distance (number of different bits)
    uint32_t hammingDistance(std::span<const uint8_t> a,
                            std::span<const uint8_t> b) const;

    // Asymmetric distance: float query vs binary stored
    // Returns approximate L2 distance
    float asymmetricDistance(std::span<const float> query,
                            std::span<const uint8_t> stored) const;

    size_t codeSize() const { return (dimension_ + 7) / 8; }

    std::vector<uint8_t> serialize() const;
    static BinaryQuantizer deserialize(std::span<const uint8_t> data);

private:
    std::vector<float> thresholds_;  // Mean per dimension
    size_t dimension_ = 0;
};

// ============================================================================
// Quantized Vector Storage
// ============================================================================

// Manages storage of quantized vectors
class QuantizedVectorStore {
public:
    QuantizedVectorStore(QuantizationType type, size_t dimension);

    // Initialize/train quantizer from sample data
    core::Status train(std::span<const float> sample_data, size_t num_vectors,
                      ErrorContext* ctx = nullptr);

    // Add a vector (returns index)
    size_t add(std::span<const float> vector);

    // Get approximate vector by index
    std::vector<float> get(size_t index) const;

    // Search: find k nearest neighbors to query
    std::vector<std::pair<size_t, float>> search(
        std::span<const float> query, size_t k) const;

    // Get quantization statistics
    QuantizationStats stats() const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    static QuantizedVectorStore deserialize(std::span<const uint8_t> data);

    // Accessors
    QuantizationType type() const { return type_; }
    size_t dimension() const { return dimension_; }
    size_t size() const { return num_vectors_; }
    size_t codeSize() const;

private:
    QuantizationType type_;
    size_t dimension_;
    size_t num_vectors_ = 0;

    // Quantizers (only one is active based on type)
    std::unique_ptr<ScalarQuantizer8> sq8_;
    std::unique_ptr<ScalarQuantizer4> sq4_;
    std::unique_ptr<ProductQuantizer> pq_;
    std::unique_ptr<OptimizedProductQuantizer> opq_;
    std::unique_ptr<BinaryQuantizer> binary_;

    // Encoded vectors storage
    std::vector<uint8_t> codes_;
    size_t code_size_ = 0;
};

// ============================================================================
// Factory and Configuration
// ============================================================================

struct QuantizationConfig {
    QuantizationType type = QuantizationType::SQ8;
    size_t pq_m = 8;            // For PQ/OPQ: number of subvectors
    size_t pq_ksub = 256;       // For PQ/OPQ: centroids per subvector
    size_t training_samples = 10000;  // Samples for training
    size_t max_iterations = 25;       // Training iterations

    // Preset configurations
    static QuantizationConfig fast() {
        QuantizationConfig cfg;
        cfg.type = QuantizationType::SQ8;
        return cfg;
    }

    static QuantizationConfig balanced() {
        QuantizationConfig cfg;
        cfg.type = QuantizationType::PQ;
        cfg.pq_m = 8;
        return cfg;
    }

    static QuantizationConfig highCompression() {
        QuantizationConfig cfg;
        cfg.type = QuantizationType::PQ;
        cfg.pq_m = 16;
        return cfg;
    }

    static QuantizationConfig extreme() {
        QuantizationConfig cfg;
        cfg.type = QuantizationType::BINARY;
        return cfg;
    }
};

// Create quantized store with configuration
std::unique_ptr<QuantizedVectorStore> createQuantizedStore(
    const QuantizationConfig& config, size_t dimension);

// Estimate memory usage
size_t estimateQuantizedMemory(const QuantizationConfig& config,
                               size_t dimension, size_t num_vectors);

} // namespace scratchbird::index
