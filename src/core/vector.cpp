/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/vector.h"
#include <sstream>
#include <algorithm>
#include <cstring>
#include <limits>

namespace scratchbird::core
{
    // ===== VectorValue Implementation =====

    VectorValue::VectorValue(const std::vector<float>& values)
        : type_(VectorType::FLOAT32), float32_data_(values) {}

    VectorValue::VectorValue(const std::vector<double>& values)
        : type_(VectorType::FLOAT64), float64_data_(values) {}

    VectorValue::VectorValue(std::vector<float>&& values)
        : type_(VectorType::FLOAT32), float32_data_(std::move(values)) {}

    VectorValue::VectorValue(std::vector<double>&& values)
        : type_(VectorType::FLOAT64), float64_data_(std::move(values)) {}

    auto VectorValue::getDimensions() const -> size_t {
        return type_ == VectorType::FLOAT32 ? float32_data_.size() : float64_data_.size();
    }

    auto VectorValue::getFloat32(size_t index) const -> std::optional<float> {
        if (type_ != VectorType::FLOAT32 || index >= float32_data_.size()) {
            return std::nullopt;
        }
        return float32_data_[index];
    }

    auto VectorValue::getFloat64(size_t index) const -> std::optional<double> {
        if (type_ != VectorType::FLOAT64 || index >= float64_data_.size()) {
            return std::nullopt;
        }
        return float64_data_[index];
    }

    auto VectorValue::getAsFloat64(size_t index) const -> std::optional<double> {
        if (type_ == VectorType::FLOAT32) {
            if (index >= float32_data_.size()) return std::nullopt;
            return static_cast<double>(float32_data_[index]);
        } else {
            if (index >= float64_data_.size()) return std::nullopt;
            return float64_data_[index];
        }
    }

    auto VectorValue::getFloat32Vector() const -> std::optional<std::vector<float>> {
        if (type_ != VectorType::FLOAT32) return std::nullopt;
        return float32_data_;
    }

    auto VectorValue::getFloat64Vector() const -> std::optional<std::vector<double>> {
        if (type_ != VectorType::FLOAT64) return std::nullopt;
        return float64_data_;
    }

    auto VectorValue::magnitude() const -> double {
        double sum = 0.0;
        size_t dims = getDimensions();
        for (size_t i = 0; i < dims; ++i) {
            double val = *getAsFloat64(i);
            sum += val * val;
        }
        return std::sqrt(sum);
    }

    auto VectorValue::normalize() const -> VectorValue {
        double mag = magnitude();
        if (mag == 0.0 || std::isnan(mag)) {
            // Return copy of zero vector
            if (type_ == VectorType::FLOAT32) {
                return VectorValue(float32_data_);
            } else {
                return VectorValue(float64_data_);
            }
        }

        if (type_ == VectorType::FLOAT32) {
            std::vector<float> result;
            result.reserve(float32_data_.size());
            for (float val : float32_data_) {
                result.push_back(static_cast<float>(val / mag));
            }
            return VectorValue(std::move(result));
        } else {
            std::vector<double> result;
            result.reserve(float64_data_.size());
            for (double val : float64_data_) {
                result.push_back(val / mag);
            }
            return VectorValue(std::move(result));
        }
    }

    auto VectorValue::dotProduct(const VectorValue& other) const -> std::optional<double> {
        if (getDimensions() != other.getDimensions()) {
            return std::nullopt;
        }

        double sum = 0.0;
        size_t dims = getDimensions();
        for (size_t i = 0; i < dims; ++i) {
            double a = *getAsFloat64(i);
            double b = *other.getAsFloat64(i);
            sum += a * b;
        }
        return sum;
    }

    auto VectorValue::euclideanDistance(const VectorValue& other) const -> std::optional<double> {
        if (getDimensions() != other.getDimensions()) {
            return std::nullopt;
        }

        double sum = 0.0;
        size_t dims = getDimensions();
        for (size_t i = 0; i < dims; ++i) {
            double a = *getAsFloat64(i);
            double b = *other.getAsFloat64(i);
            double diff = a - b;
            sum += diff * diff;
        }
        return std::sqrt(sum);
    }

    auto VectorValue::cosineSimilarity(const VectorValue& other) const -> std::optional<double> {
        if (getDimensions() != other.getDimensions()) {
            return std::nullopt;
        }

        auto dot = dotProduct(other);
        if (!dot) return std::nullopt;

        double mag_a = magnitude();
        double mag_b = other.magnitude();

        if (mag_a == 0.0 || mag_b == 0.0) {
            return 0.0;  // Undefined, return 0
        }

        return *dot / (mag_a * mag_b);
    }

    auto VectorValue::manhattanDistance(const VectorValue& other) const -> std::optional<double> {
        if (getDimensions() != other.getDimensions()) {
            return std::nullopt;
        }

        double sum = 0.0;
        size_t dims = getDimensions();
        for (size_t i = 0; i < dims; ++i) {
            double a = *getAsFloat64(i);
            double b = *other.getAsFloat64(i);
            sum += std::abs(a - b);
        }
        return sum;
    }

    auto VectorValue::distance(const VectorValue& other, DistanceMetric metric) const
        -> std::optional<double> {
        switch (metric) {
            case DistanceMetric::EUCLIDEAN:
                return euclideanDistance(other);
            case DistanceMetric::COSINE:
                return cosineSimilarity(other);
            case DistanceMetric::MANHATTAN:
                return manhattanDistance(other);
            case DistanceMetric::DOT_PRODUCT:
                return dotProduct(other);
            default:
                return std::nullopt;
        }
    }

    auto VectorValue::add(const VectorValue& other) const -> std::optional<VectorValue> {
        if (getDimensions() != other.getDimensions()) {
            return std::nullopt;
        }

        if (type_ == VectorType::FLOAT32 && other.type_ == VectorType::FLOAT32) {
            std::vector<float> result;
            result.reserve(float32_data_.size());
            for (size_t i = 0; i < float32_data_.size(); ++i) {
                result.push_back(float32_data_[i] + other.float32_data_[i]);
            }
            return VectorValue(std::move(result));
        } else {
            std::vector<double> result;
            result.reserve(getDimensions());
            for (size_t i = 0; i < getDimensions(); ++i) {
                double a = *getAsFloat64(i);
                double b = *other.getAsFloat64(i);
                result.push_back(a + b);
            }
            return VectorValue(std::move(result));
        }
    }

    auto VectorValue::subtract(const VectorValue& other) const -> std::optional<VectorValue> {
        if (getDimensions() != other.getDimensions()) {
            return std::nullopt;
        }

        if (type_ == VectorType::FLOAT32 && other.type_ == VectorType::FLOAT32) {
            std::vector<float> result;
            result.reserve(float32_data_.size());
            for (size_t i = 0; i < float32_data_.size(); ++i) {
                result.push_back(float32_data_[i] - other.float32_data_[i]);
            }
            return VectorValue(std::move(result));
        } else {
            std::vector<double> result;
            result.reserve(getDimensions());
            for (size_t i = 0; i < getDimensions(); ++i) {
                double a = *getAsFloat64(i);
                double b = *other.getAsFloat64(i);
                result.push_back(a - b);
            }
            return VectorValue(std::move(result));
        }
    }

    auto VectorValue::multiply(double scalar) const -> VectorValue {
        if (type_ == VectorType::FLOAT32) {
            std::vector<float> result;
            result.reserve(float32_data_.size());
            for (float val : float32_data_) {
                result.push_back(static_cast<float>(val * scalar));
            }
            return VectorValue(std::move(result));
        } else {
            std::vector<double> result;
            result.reserve(float64_data_.size());
            for (double val : float64_data_) {
                result.push_back(val * scalar);
            }
            return VectorValue(std::move(result));
        }
    }

    std::string VectorValue::toString() const {
        std::ostringstream oss;
        oss << "[";
        size_t dims = getDimensions();
        for (size_t i = 0; i < dims; ++i) {
            if (i > 0) oss << ", ";
            if (type_ == VectorType::FLOAT32) {
                oss << float32_data_[i];
            } else {
                oss << float64_data_[i];
            }
        }
        oss << "]";
        return oss.str();
    }

    // ===== Vector Implementation =====

    auto Vector::parse(const std::string& str, VectorType type) -> std::optional<VectorValue> {
        size_t pos = 0;
        skipWhitespace(str, pos);

        // Expect '['
        if (pos >= str.length() || str[pos] != '[') {
            return std::nullopt;
        }
        ++pos;

        std::vector<double> values;
        skipWhitespace(str, pos);

        // Empty vector
        if (pos < str.length() && str[pos] == ']') {
            if (type == VectorType::FLOAT32) {
                return VectorValue(std::vector<float>());
            } else {
                return VectorValue(std::vector<double>());
            }
        }

        // Parse numbers
        bool found_closing_bracket = false;
        while (pos < str.length()) {
            skipWhitespace(str, pos);

            auto num = parseNumber(str, pos);
            if (!num) return std::nullopt;
            values.push_back(*num);

            skipWhitespace(str, pos);

            if (pos >= str.length()) break;

            if (str[pos] == ']') {
                ++pos;
                found_closing_bracket = true;
                break;
            }

            if (str[pos] == ',') {
                ++pos;
                continue;
            }

            // Invalid character
            return std::nullopt;
        }

        // Must have found closing bracket
        if (!found_closing_bracket) return std::nullopt;

        // Convert to appropriate type
        if (type == VectorType::FLOAT32) {
            std::vector<float> float32_values;
            float32_values.reserve(values.size());
            for (double val : values) {
                float32_values.push_back(static_cast<float>(val));
            }
            return VectorValue(std::move(float32_values));
        } else {
            return VectorValue(std::move(values));
        }
    }

    auto Vector::encode(const VectorValue& value) -> std::vector<uint8_t> {
        std::vector<uint8_t> buffer;

        // Type byte
        buffer.push_back(static_cast<uint8_t>(value.getType()));

        // Dimension count (4 bytes)
        uint32_t dims = static_cast<uint32_t>(value.getDimensions());
        buffer.push_back(static_cast<uint8_t>(dims & 0xFF));
        buffer.push_back(static_cast<uint8_t>((dims >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((dims >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((dims >> 24) & 0xFF));

        // Data
        if (value.getType() == VectorType::FLOAT32) {
            auto vec = value.getFloat32Vector();
            if (!vec) return buffer;
            for (float val : *vec) {
                uint32_t bits;
                std::memcpy(&bits, &val, sizeof(float));
                buffer.push_back(static_cast<uint8_t>(bits & 0xFF));
                buffer.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
                buffer.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
                buffer.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
            }
        } else {
            auto vec = value.getFloat64Vector();
            if (!vec) return buffer;
            for (double val : *vec) {
                uint64_t bits;
                std::memcpy(&bits, &val, sizeof(double));
                for (int i = 0; i < 8; ++i) {
                    buffer.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
                }
            }
        }

        return buffer;
    }

    auto Vector::decode(const std::vector<uint8_t>& binary) -> std::optional<VectorValue> {
        if (binary.size() < 5) return std::nullopt;

        size_t pos = 0;

        // Type byte
        VectorType type = static_cast<VectorType>(binary[pos++]);

        // Dimension count
        uint32_t dims = static_cast<uint32_t>(binary[pos]) |
                       (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                       (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                       (static_cast<uint32_t>(binary[pos + 3]) << 24);
        pos += 4;

        if (type == VectorType::FLOAT32) {
            size_t expected_size = 5 + dims * 4;
            if (binary.size() < expected_size) return std::nullopt;

            std::vector<float> values;
            values.reserve(dims);
            for (uint32_t i = 0; i < dims; ++i) {
                uint32_t bits = static_cast<uint32_t>(binary[pos]) |
                               (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                               (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                               (static_cast<uint32_t>(binary[pos + 3]) << 24);
                pos += 4;
                float val;
                std::memcpy(&val, &bits, sizeof(float));
                values.push_back(val);
            }
            return VectorValue(std::move(values));
        } else {
            size_t expected_size = 5 + dims * 8;
            if (binary.size() < expected_size) return std::nullopt;

            std::vector<double> values;
            values.reserve(dims);
            for (uint32_t i = 0; i < dims; ++i) {
                uint64_t bits = 0;
                for (int j = 0; j < 8; ++j) {
                    bits |= static_cast<uint64_t>(binary[pos++]) << (j * 8);
                }
                double val;
                std::memcpy(&val, &bits, sizeof(double));
                values.push_back(val);
            }
            return VectorValue(std::move(values));
        }
    }

    bool Vector::validate(const std::string& str) {
        return parse(str).has_value();
    }

    auto Vector::fromFloat32(const std::vector<float>& values) -> VectorValue {
        return VectorValue(values);
    }

    auto Vector::fromFloat64(const std::vector<double>& values) -> VectorValue {
        return VectorValue(values);
    }

    auto Vector::euclideanDistance(const VectorValue& a, const VectorValue& b)
        -> std::optional<double> {
        return a.euclideanDistance(b);
    }

    auto Vector::cosineSimilarity(const VectorValue& a, const VectorValue& b)
        -> std::optional<double> {
        return a.cosineSimilarity(b);
    }

    auto Vector::manhattanDistance(const VectorValue& a, const VectorValue& b)
        -> std::optional<double> {
        return a.manhattanDistance(b);
    }

    auto Vector::dotProduct(const VectorValue& a, const VectorValue& b)
        -> std::optional<double> {
        return a.dotProduct(b);
    }

    auto Vector::skipWhitespace(const std::string& str, size_t& pos) -> void {
        while (pos < str.length() && std::isspace(str[pos])) {
            ++pos;
        }
    }

    auto Vector::parseNumber(const std::string& str, size_t& pos) -> std::optional<double> {
        skipWhitespace(str, pos);
        if (pos >= str.length()) return std::nullopt;

        size_t start = pos;
        bool has_sign = false;
        bool has_decimal = false;
        bool has_exponent = false;

        // Optional sign
        if (str[pos] == '+' || str[pos] == '-') {
            has_sign = true;
            ++pos;
        }

        // Digits before decimal
        bool has_digits = false;
        while (pos < str.length() && std::isdigit(str[pos])) {
            has_digits = true;
            ++pos;
        }

        // Decimal point
        if (pos < str.length() && str[pos] == '.') {
            has_decimal = true;
            ++pos;

            // Digits after decimal
            while (pos < str.length() && std::isdigit(str[pos])) {
                has_digits = true;
                ++pos;
            }
        }

        if (!has_digits) return std::nullopt;

        // Exponent
        if (pos < str.length() && (str[pos] == 'e' || str[pos] == 'E')) {
            has_exponent = true;
            ++pos;

            // Optional sign
            if (pos < str.length() && (str[pos] == '+' || str[pos] == '-')) {
                ++pos;
            }

            // Exponent digits
            bool has_exp_digits = false;
            while (pos < str.length() && std::isdigit(str[pos])) {
                has_exp_digits = true;
                ++pos;
            }

            if (!has_exp_digits) return std::nullopt;
        }

        std::string num_str = str.substr(start, pos - start);
        try {
            return std::stod(num_str);
        } catch (...) {
            return std::nullopt;
        }
    }

} // namespace scratchbird::core
