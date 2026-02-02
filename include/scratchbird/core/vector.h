/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <vector>
#include <optional>
#include <cstdint>
#include <cmath>
#include <string>

namespace scratchbird::core
{
    /**
     * VectorType - Element type for vectors
     */
    enum class VectorType : uint8_t {
        FLOAT32 = 0,
        FLOAT64 = 1
    };

    /**
     * DistanceMetric - Distance/similarity metrics for vectors
     */
    enum class DistanceMetric {
        EUCLIDEAN,      // L2 distance: sqrt(sum((a[i] - b[i])^2))
        COSINE,         // Cosine similarity: dot(a,b) / (||a|| * ||b||)
        MANHATTAN,      // L1 distance: sum(|a[i] - b[i]|)
        DOT_PRODUCT     // Dot product: sum(a[i] * b[i])
    };

    /**
     * VectorValue - Runtime representation of a vector
     */
    class VectorValue {
    public:
        // Constructors
        VectorValue(const std::vector<float>& values);
        VectorValue(const std::vector<double>& values);
        VectorValue(std::vector<float>&& values);
        VectorValue(std::vector<double>&& values);

        // Type queries
        auto getType() const -> VectorType { return type_; }
        auto getDimensions() const -> size_t;
        bool isEmpty() const { return getDimensions() == 0; }

        // Element access
        auto getFloat32(size_t index) const -> std::optional<float>;
        auto getFloat64(size_t index) const -> std::optional<double>;
        auto getAsFloat64(size_t index) const -> std::optional<double>;

        // Get all elements
        auto getFloat32Vector() const -> std::optional<std::vector<float>>;
        auto getFloat64Vector() const -> std::optional<std::vector<double>>;

        // Vector operations
        auto magnitude() const -> double;
        auto normalize() const -> VectorValue;
        auto dotProduct(const VectorValue& other) const -> std::optional<double>;

        // Distance metrics
        auto euclideanDistance(const VectorValue& other) const -> std::optional<double>;
        auto cosineSimilarity(const VectorValue& other) const -> std::optional<double>;
        auto manhattanDistance(const VectorValue& other) const -> std::optional<double>;
        auto distance(const VectorValue& other, DistanceMetric metric) const -> std::optional<double>;

        // Vector arithmetic
        auto add(const VectorValue& other) const -> std::optional<VectorValue>;
        auto subtract(const VectorValue& other) const -> std::optional<VectorValue>;
        auto multiply(double scalar) const -> VectorValue;

        // String conversion
        std::string toString() const;

    private:
        VectorType type_;
        std::vector<float> float32_data_;
        std::vector<double> float64_data_;
    };

    /**
     * Vector - Vector encoding/decoding and utilities
     */
    class Vector {
    public:
        // Parse from string representation: "[1.0, 2.0, 3.0]"
        static auto parse(const std::string& str, VectorType type = VectorType::FLOAT32)
            -> std::optional<VectorValue>;

        // Binary encoding/decoding
        static auto encode(const VectorValue& value) -> std::vector<uint8_t>;
        static auto decode(const std::vector<uint8_t>& binary) -> std::optional<VectorValue>;

        // Validation
        static bool validate(const std::string& str);

        // Create vector from array
        static auto fromFloat32(const std::vector<float>& values) -> VectorValue;
        static auto fromFloat64(const std::vector<double>& values) -> VectorValue;

        // Distance/similarity helpers
        static auto euclideanDistance(const VectorValue& a, const VectorValue& b)
            -> std::optional<double>;
        static auto cosineSimilarity(const VectorValue& a, const VectorValue& b)
            -> std::optional<double>;
        static auto manhattanDistance(const VectorValue& a, const VectorValue& b)
            -> std::optional<double>;
        static auto dotProduct(const VectorValue& a, const VectorValue& b)
            -> std::optional<double>;

    private:
        // Parser helpers
        static auto skipWhitespace(const std::string& str, size_t& pos) -> void;
        static auto parseNumber(const std::string& str, size_t& pos) -> std::optional<double>;
    };

} // namespace scratchbird::core
