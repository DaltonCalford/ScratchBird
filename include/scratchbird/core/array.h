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
#include <string>
#include <variant>
#include <memory>

namespace scratchbird::core
{
    /**
     * ArrayElementType - Type of elements in the array
     */
    enum class ArrayElementType : uint8_t {
        INT32 = 0,
        INT64 = 1,
        FLOAT32 = 2,
        FLOAT64 = 3,
        STRING = 4,
        BOOL = 5
    };

    /**
     * ArrayValue - Runtime representation of a multi-dimensional array
     */
    class ArrayValue {
    public:
        using Element = std::variant<int32_t, int64_t, float, double, std::string, bool>;

        // Constructors
        ArrayValue(ArrayElementType type, const std::vector<size_t>& dimensions);
        ArrayValue(const std::vector<int32_t>& values, const std::vector<size_t>& dimensions);
        ArrayValue(const std::vector<int64_t>& values, const std::vector<size_t>& dimensions);
        ArrayValue(const std::vector<float>& values, const std::vector<size_t>& dimensions);
        ArrayValue(const std::vector<double>& values, const std::vector<size_t>& dimensions);
        ArrayValue(const std::vector<std::string>& values, const std::vector<size_t>& dimensions);
        ArrayValue(const std::vector<bool>& values, const std::vector<size_t>& dimensions);

        // Type queries
        auto getElementType() const -> ArrayElementType { return element_type_; }
        auto getDimensions() const -> const std::vector<size_t>& { return dimensions_; }
        auto getRank() const -> size_t { return dimensions_.size(); }
        auto getTotalElements() const -> size_t;
        bool isEmpty() const { return getTotalElements() == 0; }

        // Element access (flat index)
        auto getElement(size_t flat_index) const -> std::optional<Element>;
        auto setElement(size_t flat_index, const Element& value) -> bool;

        // Multi-dimensional indexing
        auto at(const std::vector<size_t>& indices) const -> std::optional<Element>;
        auto set(const std::vector<size_t>& indices, const Element& value) -> bool;

        // Slicing
        auto slice(const std::vector<std::pair<size_t, size_t>>& ranges) const -> std::optional<ArrayValue>;

        // Array operations
        auto reshape(const std::vector<size_t>& new_dimensions) const -> std::optional<ArrayValue>;
        auto flatten() const -> ArrayValue;
        auto transpose() const -> std::optional<ArrayValue>;  // Only for 2D arrays

        // Type-specific getters
        auto getInt32Vector() const -> std::optional<std::vector<int32_t>>;
        auto getInt64Vector() const -> std::optional<std::vector<int64_t>>;
        auto getFloat32Vector() const -> std::optional<std::vector<float>>;
        auto getFloat64Vector() const -> std::optional<std::vector<double>>;
        auto getStringVector() const -> std::optional<std::vector<std::string>>;
        auto getBoolVector() const -> std::optional<std::vector<bool>>;

        // String conversion
        std::string toString() const;

    private:
        ArrayElementType element_type_;
        std::vector<size_t> dimensions_;

        // Storage for different types
        std::vector<int32_t> int32_data_;
        std::vector<int64_t> int64_data_;
        std::vector<float> float32_data_;
        std::vector<double> float64_data_;
        std::vector<std::string> string_data_;
        std::vector<bool> bool_data_;

        // Helper: convert multi-dimensional indices to flat index
        auto indicesToFlat(const std::vector<size_t>& indices) const -> std::optional<size_t>;
    };

    /**
     * Array - Array encoding/decoding and utilities
     */
    class Array {
    public:
        // Parse from string representation
        static auto parse(const std::string& str, ArrayElementType type) -> std::optional<ArrayValue>;

        // Binary encoding/decoding
        static auto encode(const ArrayValue& value) -> std::vector<uint8_t>;
        static auto decode(const std::vector<uint8_t>& binary) -> std::optional<ArrayValue>;

        // Validation
        static bool validate(const std::string& str);

        // Create arrays from vectors
        static auto fromInt32(const std::vector<int32_t>& values, const std::vector<size_t>& dimensions)
            -> std::optional<ArrayValue>;
        static auto fromInt64(const std::vector<int64_t>& values, const std::vector<size_t>& dimensions)
            -> std::optional<ArrayValue>;
        static auto fromFloat32(const std::vector<float>& values, const std::vector<size_t>& dimensions)
            -> std::optional<ArrayValue>;
        static auto fromFloat64(const std::vector<double>& values, const std::vector<size_t>& dimensions)
            -> std::optional<ArrayValue>;
        static auto fromString(const std::vector<std::string>& values, const std::vector<size_t>& dimensions)
            -> std::optional<ArrayValue>;
        static auto fromBool(const std::vector<bool>& values, const std::vector<size_t>& dimensions)
            -> std::optional<ArrayValue>;

        // Array operations
        static auto concatenate(const ArrayValue& a, const ArrayValue& b, size_t axis)
            -> std::optional<ArrayValue>;

    private:
        // Parser helpers
        static auto skipWhitespace(const std::string& str, size_t& pos) -> void;
        static auto parseValue(const std::string& str, size_t& pos, ArrayElementType type)
            -> std::optional<ArrayValue::Element>;
        static auto parseArray(const std::string& str, size_t& pos, ArrayElementType type,
                              std::vector<ArrayValue::Element>& elements, std::vector<size_t>& dimensions)
            -> bool;
    };

} // namespace scratchbird::core
