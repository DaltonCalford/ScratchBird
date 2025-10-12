#include "scratchbird/core/array.h"
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <functional>

namespace scratchbird::core
{
    // ===== ArrayValue Implementation =====

    ArrayValue::ArrayValue(ArrayElementType type, const std::vector<size_t>& dimensions)
        : element_type_(type), dimensions_(dimensions) {
        size_t total = getTotalElements();
        switch (type) {
            case ArrayElementType::INT32:
                int32_data_.resize(total, 0);
                break;
            case ArrayElementType::INT64:
                int64_data_.resize(total, 0);
                break;
            case ArrayElementType::FLOAT32:
                float32_data_.resize(total, 0.0f);
                break;
            case ArrayElementType::FLOAT64:
                float64_data_.resize(total, 0.0);
                break;
            case ArrayElementType::STRING:
                string_data_.resize(total, "");
                break;
            case ArrayElementType::BOOL:
                bool_data_.resize(total, false);
                break;
        }
    }

    ArrayValue::ArrayValue(const std::vector<int32_t>& values, const std::vector<size_t>& dimensions)
        : element_type_(ArrayElementType::INT32), dimensions_(dimensions), int32_data_(values) {}

    ArrayValue::ArrayValue(const std::vector<int64_t>& values, const std::vector<size_t>& dimensions)
        : element_type_(ArrayElementType::INT64), dimensions_(dimensions), int64_data_(values) {}

    ArrayValue::ArrayValue(const std::vector<float>& values, const std::vector<size_t>& dimensions)
        : element_type_(ArrayElementType::FLOAT32), dimensions_(dimensions), float32_data_(values) {}

    ArrayValue::ArrayValue(const std::vector<double>& values, const std::vector<size_t>& dimensions)
        : element_type_(ArrayElementType::FLOAT64), dimensions_(dimensions), float64_data_(values) {}

    ArrayValue::ArrayValue(const std::vector<std::string>& values, const std::vector<size_t>& dimensions)
        : element_type_(ArrayElementType::STRING), dimensions_(dimensions), string_data_(values) {}

    ArrayValue::ArrayValue(const std::vector<bool>& values, const std::vector<size_t>& dimensions)
        : element_type_(ArrayElementType::BOOL), dimensions_(dimensions), bool_data_(values) {}

    auto ArrayValue::getTotalElements() const -> size_t {
        if (dimensions_.empty()) return 0;
        return std::accumulate(dimensions_.begin(), dimensions_.end(), size_t(1),
                              std::multiplies<size_t>());
    }

    auto ArrayValue::indicesToFlat(const std::vector<size_t>& indices) const -> std::optional<size_t> {
        if (indices.size() != dimensions_.size()) {
            return std::nullopt;
        }

        size_t flat_index = 0;
        size_t stride = 1;

        // Row-major order
        for (int i = static_cast<int>(dimensions_.size()) - 1; i >= 0; --i) {
            if (indices[i] >= dimensions_[i]) {
                return std::nullopt;  // Out of bounds
            }
            flat_index += indices[i] * stride;
            stride *= dimensions_[i];
        }

        return flat_index;
    }

    auto ArrayValue::getElement(size_t flat_index) const -> std::optional<Element> {
        if (flat_index >= getTotalElements()) {
            return std::nullopt;
        }

        switch (element_type_) {
            case ArrayElementType::INT32:
                return int32_data_[flat_index];
            case ArrayElementType::INT64:
                return int64_data_[flat_index];
            case ArrayElementType::FLOAT32:
                return float32_data_[flat_index];
            case ArrayElementType::FLOAT64:
                return float64_data_[flat_index];
            case ArrayElementType::STRING:
                return string_data_[flat_index];
            case ArrayElementType::BOOL:
                return bool_data_[flat_index];
        }
        return std::nullopt;
    }

    auto ArrayValue::setElement(size_t flat_index, const Element& value) -> bool {
        if (flat_index >= getTotalElements()) {
            return false;
        }

        switch (element_type_) {
            case ArrayElementType::INT32:
                if (auto* v = std::get_if<int32_t>(&value)) {
                    int32_data_[flat_index] = *v;
                    return true;
                }
                break;
            case ArrayElementType::INT64:
                if (auto* v = std::get_if<int64_t>(&value)) {
                    int64_data_[flat_index] = *v;
                    return true;
                }
                break;
            case ArrayElementType::FLOAT32:
                if (auto* v = std::get_if<float>(&value)) {
                    float32_data_[flat_index] = *v;
                    return true;
                }
                break;
            case ArrayElementType::FLOAT64:
                if (auto* v = std::get_if<double>(&value)) {
                    float64_data_[flat_index] = *v;
                    return true;
                }
                break;
            case ArrayElementType::STRING:
                if (auto* v = std::get_if<std::string>(&value)) {
                    string_data_[flat_index] = *v;
                    return true;
                }
                break;
            case ArrayElementType::BOOL:
                if (auto* v = std::get_if<bool>(&value)) {
                    bool_data_[flat_index] = *v;
                    return true;
                }
                break;
        }
        return false;
    }

    auto ArrayValue::at(const std::vector<size_t>& indices) const -> std::optional<Element> {
        auto flat = indicesToFlat(indices);
        if (!flat) return std::nullopt;
        return getElement(*flat);
    }

    auto ArrayValue::set(const std::vector<size_t>& indices, const Element& value) -> bool {
        auto flat = indicesToFlat(indices);
        if (!flat) return false;
        return setElement(*flat, value);
    }

    auto ArrayValue::slice(const std::vector<std::pair<size_t, size_t>>& ranges) const
        -> std::optional<ArrayValue> {
        if (ranges.size() != dimensions_.size()) {
            return std::nullopt;
        }

        // Calculate new dimensions
        std::vector<size_t> new_dimensions;
        for (size_t i = 0; i < ranges.size(); ++i) {
            if (ranges[i].first >= ranges[i].second || ranges[i].second > dimensions_[i]) {
                return std::nullopt;
            }
            new_dimensions.push_back(ranges[i].second - ranges[i].first);
        }

        ArrayValue result(element_type_, new_dimensions);

        // Copy sliced elements
        std::function<void(std::vector<size_t>&, size_t)> copySlice;
        copySlice = [&](std::vector<size_t>& indices, size_t dim) {
            if (dim == dimensions_.size()) {
                // At leaf, copy element
                std::vector<size_t> src_indices = indices;
                for (size_t i = 0; i < src_indices.size(); ++i) {
                    src_indices[i] += ranges[i].first;
                }
                auto elem = at(src_indices);
                if (elem) {
                    result.set(indices, *elem);
                }
                return;
            }

            for (size_t i = 0; i < new_dimensions[dim]; ++i) {
                indices[dim] = i;
                copySlice(indices, dim + 1);
            }
        };

        std::vector<size_t> indices(dimensions_.size(), 0);
        copySlice(indices, 0);

        return result;
    }

    auto ArrayValue::reshape(const std::vector<size_t>& new_dimensions) const -> std::optional<ArrayValue> {
        size_t new_total = std::accumulate(new_dimensions.begin(), new_dimensions.end(), size_t(1),
                                          std::multiplies<size_t>());
        if (new_total != getTotalElements()) {
            return std::nullopt;
        }

        ArrayValue result(element_type_, new_dimensions);

        // Copy data
        switch (element_type_) {
            case ArrayElementType::INT32:
                result.int32_data_ = int32_data_;
                break;
            case ArrayElementType::INT64:
                result.int64_data_ = int64_data_;
                break;
            case ArrayElementType::FLOAT32:
                result.float32_data_ = float32_data_;
                break;
            case ArrayElementType::FLOAT64:
                result.float64_data_ = float64_data_;
                break;
            case ArrayElementType::STRING:
                result.string_data_ = string_data_;
                break;
            case ArrayElementType::BOOL:
                result.bool_data_ = bool_data_;
                break;
        }

        return result;
    }

    auto ArrayValue::flatten() const -> ArrayValue {
        return *reshape({getTotalElements()});
    }

    auto ArrayValue::transpose() const -> std::optional<ArrayValue> {
        if (dimensions_.size() != 2) {
            return std::nullopt;  // Only for 2D arrays
        }

        std::vector<size_t> new_dims = {dimensions_[1], dimensions_[0]};
        ArrayValue result(element_type_, new_dims);

        for (size_t i = 0; i < dimensions_[0]; ++i) {
            for (size_t j = 0; j < dimensions_[1]; ++j) {
                auto elem = at({i, j});
                if (elem) {
                    result.set({j, i}, *elem);
                }
            }
        }

        return result;
    }

    auto ArrayValue::getInt32Vector() const -> std::optional<std::vector<int32_t>> {
        if (element_type_ != ArrayElementType::INT32) return std::nullopt;
        return int32_data_;
    }

    auto ArrayValue::getInt64Vector() const -> std::optional<std::vector<int64_t>> {
        if (element_type_ != ArrayElementType::INT64) return std::nullopt;
        return int64_data_;
    }

    auto ArrayValue::getFloat32Vector() const -> std::optional<std::vector<float>> {
        if (element_type_ != ArrayElementType::FLOAT32) return std::nullopt;
        return float32_data_;
    }

    auto ArrayValue::getFloat64Vector() const -> std::optional<std::vector<double>> {
        if (element_type_ != ArrayElementType::FLOAT64) return std::nullopt;
        return float64_data_;
    }

    auto ArrayValue::getStringVector() const -> std::optional<std::vector<std::string>> {
        if (element_type_ != ArrayElementType::STRING) return std::nullopt;
        return string_data_;
    }

    auto ArrayValue::getBoolVector() const -> std::optional<std::vector<bool>> {
        if (element_type_ != ArrayElementType::BOOL) return std::nullopt;
        return bool_data_;
    }

    std::string ArrayValue::toString() const {
        std::ostringstream oss;

        std::function<void(const std::vector<size_t>&, size_t)> printArray;
        printArray = [&](const std::vector<size_t>& indices, size_t dim) {
            if (dim == dimensions_.size()) {
                // At leaf, print element
                auto elem = at(indices);
                if (elem) {
                    std::visit([&oss](auto&& arg) {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_same_v<T, std::string>) {
                            oss << "\"" << arg << "\"";
                        } else if constexpr (std::is_same_v<T, bool>) {
                            oss << (arg ? "true" : "false");
                        } else {
                            oss << arg;
                        }
                    }, *elem);
                }
                return;
            }

            oss << "[";
            for (size_t i = 0; i < dimensions_[dim]; ++i) {
                if (i > 0) oss << ", ";
                std::vector<size_t> new_indices = indices;
                new_indices.push_back(i);
                printArray(new_indices, dim + 1);
            }
            oss << "]";
        };

        printArray({}, 0);
        return oss.str();
    }

    // ===== Array Implementation =====

    auto Array::parse(const std::string& str, ArrayElementType type) -> std::optional<ArrayValue> {
        size_t pos = 0;
        std::vector<ArrayValue::Element> elements;
        std::vector<size_t> dimensions;

        if (!parseArray(str, pos, type, elements, dimensions)) {
            return std::nullopt;
        }

        if (dimensions.empty()) {
            return std::nullopt;
        }

        // Create array based on type
        switch (type) {
            case ArrayElementType::INT32: {
                std::vector<int32_t> values;
                for (const auto& elem : elements) {
                    if (auto* v = std::get_if<int32_t>(&elem)) {
                        values.push_back(*v);
                    }
                }
                return ArrayValue(values, dimensions);
            }
            case ArrayElementType::INT64: {
                std::vector<int64_t> values;
                for (const auto& elem : elements) {
                    if (auto* v = std::get_if<int64_t>(&elem)) {
                        values.push_back(*v);
                    }
                }
                return ArrayValue(values, dimensions);
            }
            case ArrayElementType::FLOAT32: {
                std::vector<float> values;
                for (const auto& elem : elements) {
                    if (auto* v = std::get_if<float>(&elem)) {
                        values.push_back(*v);
                    }
                }
                return ArrayValue(values, dimensions);
            }
            case ArrayElementType::FLOAT64: {
                std::vector<double> values;
                for (const auto& elem : elements) {
                    if (auto* v = std::get_if<double>(&elem)) {
                        values.push_back(*v);
                    }
                }
                return ArrayValue(values, dimensions);
            }
            case ArrayElementType::STRING: {
                std::vector<std::string> values;
                for (const auto& elem : elements) {
                    if (auto* v = std::get_if<std::string>(&elem)) {
                        values.push_back(*v);
                    }
                }
                return ArrayValue(values, dimensions);
            }
            case ArrayElementType::BOOL: {
                std::vector<bool> values;
                for (const auto& elem : elements) {
                    if (auto* v = std::get_if<bool>(&elem)) {
                        values.push_back(*v);
                    }
                }
                return ArrayValue(values, dimensions);
            }
        }

        return std::nullopt;
    }

    auto Array::encode(const ArrayValue& value) -> std::vector<uint8_t> {
        std::vector<uint8_t> buffer;

        // Element type
        buffer.push_back(static_cast<uint8_t>(value.getElementType()));

        // Rank (number of dimensions)
        uint8_t rank = static_cast<uint8_t>(value.getRank());
        buffer.push_back(rank);

        // Dimensions
        for (size_t dim : value.getDimensions()) {
            uint32_t d = static_cast<uint32_t>(dim);
            buffer.push_back(static_cast<uint8_t>(d & 0xFF));
            buffer.push_back(static_cast<uint8_t>((d >> 8) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((d >> 16) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((d >> 24) & 0xFF));
        }

        // Elements
        size_t total = value.getTotalElements();
        switch (value.getElementType()) {
            case ArrayElementType::INT32: {
                auto vec = value.getInt32Vector();
                if (!vec) break;
                for (int32_t val : *vec) {
                    uint32_t bits = static_cast<uint32_t>(val);
                    buffer.push_back(static_cast<uint8_t>(bits & 0xFF));
                    buffer.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
                    buffer.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
                    buffer.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
                }
                break;
            }
            case ArrayElementType::INT64: {
                auto vec = value.getInt64Vector();
                if (!vec) break;
                for (int64_t val : *vec) {
                    uint64_t bits = static_cast<uint64_t>(val);
                    for (int i = 0; i < 8; ++i) {
                        buffer.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
                    }
                }
                break;
            }
            case ArrayElementType::FLOAT32: {
                auto vec = value.getFloat32Vector();
                if (!vec) break;
                for (float val : *vec) {
                    uint32_t bits;
                    std::memcpy(&bits, &val, sizeof(float));
                    buffer.push_back(static_cast<uint8_t>(bits & 0xFF));
                    buffer.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
                    buffer.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
                    buffer.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
                }
                break;
            }
            case ArrayElementType::FLOAT64: {
                auto vec = value.getFloat64Vector();
                if (!vec) break;
                for (double val : *vec) {
                    uint64_t bits;
                    std::memcpy(&bits, &val, sizeof(double));
                    for (int i = 0; i < 8; ++i) {
                        buffer.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
                    }
                }
                break;
            }
            case ArrayElementType::STRING: {
                auto vec = value.getStringVector();
                if (!vec) break;
                for (const auto& val : *vec) {
                    uint32_t len = static_cast<uint32_t>(val.length());
                    buffer.push_back(static_cast<uint8_t>(len & 0xFF));
                    buffer.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
                    buffer.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
                    buffer.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
                    for (char c : val) {
                        buffer.push_back(static_cast<uint8_t>(c));
                    }
                }
                break;
            }
            case ArrayElementType::BOOL: {
                auto vec = value.getBoolVector();
                if (!vec) break;
                for (bool val : *vec) {
                    buffer.push_back(val ? 1 : 0);
                }
                break;
            }
        }

        return buffer;
    }

    auto Array::decode(const std::vector<uint8_t>& binary) -> std::optional<ArrayValue> {
        if (binary.size() < 2) return std::nullopt;

        size_t pos = 0;

        // Element type
        ArrayElementType type = static_cast<ArrayElementType>(binary[pos++]);

        // Rank
        uint8_t rank = binary[pos++];

        // Dimensions
        if (binary.size() < 2 + rank * 4) return std::nullopt;
        std::vector<size_t> dimensions;
        for (uint8_t i = 0; i < rank; ++i) {
            uint32_t d = static_cast<uint32_t>(binary[pos]) |
                        (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                        (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                        (static_cast<uint32_t>(binary[pos + 3]) << 24);
            pos += 4;
            dimensions.push_back(d);
        }

        size_t total = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                      std::multiplies<size_t>());

        // Decode elements
        switch (type) {
            case ArrayElementType::INT32: {
                if (binary.size() < pos + total * 4) return std::nullopt;
                std::vector<int32_t> values;
                values.reserve(total);
                for (size_t i = 0; i < total; ++i) {
                    uint32_t bits = static_cast<uint32_t>(binary[pos]) |
                                   (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                                   (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                                   (static_cast<uint32_t>(binary[pos + 3]) << 24);
                    pos += 4;
                    values.push_back(static_cast<int32_t>(bits));
                }
                return ArrayValue(values, dimensions);
            }
            case ArrayElementType::INT64: {
                if (binary.size() < pos + total * 8) return std::nullopt;
                std::vector<int64_t> values;
                values.reserve(total);
                for (size_t i = 0; i < total; ++i) {
                    uint64_t bits = 0;
                    for (int j = 0; j < 8; ++j) {
                        bits |= static_cast<uint64_t>(binary[pos++]) << (j * 8);
                    }
                    values.push_back(static_cast<int64_t>(bits));
                }
                return ArrayValue(values, dimensions);
            }
            case ArrayElementType::FLOAT32: {
                if (binary.size() < pos + total * 4) return std::nullopt;
                std::vector<float> values;
                values.reserve(total);
                for (size_t i = 0; i < total; ++i) {
                    uint32_t bits = static_cast<uint32_t>(binary[pos]) |
                                   (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                                   (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                                   (static_cast<uint32_t>(binary[pos + 3]) << 24);
                    pos += 4;
                    float val;
                    std::memcpy(&val, &bits, sizeof(float));
                    values.push_back(val);
                }
                return ArrayValue(values, dimensions);
            }
            case ArrayElementType::FLOAT64: {
                if (binary.size() < pos + total * 8) return std::nullopt;
                std::vector<double> values;
                values.reserve(total);
                for (size_t i = 0; i < total; ++i) {
                    uint64_t bits = 0;
                    for (int j = 0; j < 8; ++j) {
                        bits |= static_cast<uint64_t>(binary[pos++]) << (j * 8);
                    }
                    double val;
                    std::memcpy(&val, &bits, sizeof(double));
                    values.push_back(val);
                }
                return ArrayValue(values, dimensions);
            }
            case ArrayElementType::STRING: {
                std::vector<std::string> values;
                values.reserve(total);
                for (size_t i = 0; i < total; ++i) {
                    if (binary.size() < pos + 4) return std::nullopt;
                    uint32_t len = static_cast<uint32_t>(binary[pos]) |
                                  (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                                  (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                                  (static_cast<uint32_t>(binary[pos + 3]) << 24);
                    pos += 4;
                    if (binary.size() < pos + len) return std::nullopt;
                    std::string str(reinterpret_cast<const char*>(&binary[pos]), len);
                    pos += len;
                    values.push_back(str);
                }
                return ArrayValue(values, dimensions);
            }
            case ArrayElementType::BOOL: {
                if (binary.size() < pos + total) return std::nullopt;
                std::vector<bool> values;
                values.reserve(total);
                for (size_t i = 0; i < total; ++i) {
                    values.push_back(binary[pos++] != 0);
                }
                return ArrayValue(values, dimensions);
            }
        }

        return std::nullopt;
    }

    bool Array::validate(const std::string& str) {
        // Simple validation - try to parse
        return parse(str, ArrayElementType::INT32).has_value() ||
               parse(str, ArrayElementType::FLOAT64).has_value() ||
               parse(str, ArrayElementType::STRING).has_value();
    }

    auto Array::fromInt32(const std::vector<int32_t>& values, const std::vector<size_t>& dimensions)
        -> std::optional<ArrayValue> {
        size_t total = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                      std::multiplies<size_t>());
        if (total != values.size()) return std::nullopt;
        return ArrayValue(values, dimensions);
    }

    auto Array::fromInt64(const std::vector<int64_t>& values, const std::vector<size_t>& dimensions)
        -> std::optional<ArrayValue> {
        size_t total = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                      std::multiplies<size_t>());
        if (total != values.size()) return std::nullopt;
        return ArrayValue(values, dimensions);
    }

    auto Array::fromFloat32(const std::vector<float>& values, const std::vector<size_t>& dimensions)
        -> std::optional<ArrayValue> {
        size_t total = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                      std::multiplies<size_t>());
        if (total != values.size()) return std::nullopt;
        return ArrayValue(values, dimensions);
    }

    auto Array::fromFloat64(const std::vector<double>& values, const std::vector<size_t>& dimensions)
        -> std::optional<ArrayValue> {
        size_t total = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                      std::multiplies<size_t>());
        if (total != values.size()) return std::nullopt;
        return ArrayValue(values, dimensions);
    }

    auto Array::fromString(const std::vector<std::string>& values, const std::vector<size_t>& dimensions)
        -> std::optional<ArrayValue> {
        size_t total = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                      std::multiplies<size_t>());
        if (total != values.size()) return std::nullopt;
        return ArrayValue(values, dimensions);
    }

    auto Array::fromBool(const std::vector<bool>& values, const std::vector<size_t>& dimensions)
        -> std::optional<ArrayValue> {
        size_t total = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                      std::multiplies<size_t>());
        if (total != values.size()) return std::nullopt;
        return ArrayValue(values, dimensions);
    }

    auto Array::concatenate(const ArrayValue& a, const ArrayValue& b, size_t axis)
        -> std::optional<ArrayValue> {
        if (a.getElementType() != b.getElementType()) {
            return std::nullopt;
        }
        if (a.getRank() != b.getRank() || axis >= a.getRank()) {
            return std::nullopt;
        }

        // Check that all dimensions except axis are equal
        for (size_t i = 0; i < a.getRank(); ++i) {
            if (i != axis && a.getDimensions()[i] != b.getDimensions()[i]) {
                return std::nullopt;
            }
        }

        // Calculate new dimensions
        std::vector<size_t> new_dims = a.getDimensions();
        new_dims[axis] += b.getDimensions()[axis];

        // Create result array
        ArrayValue result(a.getElementType(), new_dims);

        // Copy elements (simplified - just concatenate in row-major order along axis)
        // This is a simplified implementation
        return std::nullopt;  // TODO: Implement full concatenation logic
    }

    auto Array::skipWhitespace(const std::string& str, size_t& pos) -> void {
        while (pos < str.length() && std::isspace(str[pos])) {
            ++pos;
        }
    }

    auto Array::parseValue(const std::string& str, size_t& pos, ArrayElementType type)
        -> std::optional<ArrayValue::Element> {
        skipWhitespace(str, pos);
        if (pos >= str.length()) return std::nullopt;

        if (type == ArrayElementType::STRING) {
            if (str[pos] != '"') return std::nullopt;
            ++pos;
            size_t start = pos;
            while (pos < str.length() && str[pos] != '"') {
                ++pos;
            }
            if (pos >= str.length()) return std::nullopt;
            std::string val = str.substr(start, pos - start);
            ++pos;
            return val;
        }

        if (type == ArrayElementType::BOOL) {
            if (str.substr(pos, 4) == "true") {
                pos += 4;
                return true;
            }
            if (str.substr(pos, 5) == "false") {
                pos += 5;
                return false;
            }
            return std::nullopt;
        }

        // Parse number
        size_t start = pos;
        if (str[pos] == '-' || str[pos] == '+') ++pos;
        while (pos < str.length() && (std::isdigit(str[pos]) || str[pos] == '.' ||
               str[pos] == 'e' || str[pos] == 'E' || str[pos] == '+' || str[pos] == '-')) {
            ++pos;
        }

        std::string num_str = str.substr(start, pos - start);
        try {
            if (type == ArrayElementType::INT32) {
                return static_cast<int32_t>(std::stoi(num_str));
            } else if (type == ArrayElementType::INT64) {
                return static_cast<int64_t>(std::stoll(num_str));
            } else if (type == ArrayElementType::FLOAT32) {
                return static_cast<float>(std::stof(num_str));
            } else if (type == ArrayElementType::FLOAT64) {
                return std::stod(num_str);
            }
        } catch (...) {
            return std::nullopt;
        }

        return std::nullopt;
    }

    auto Array::parseArray(const std::string& str, size_t& pos, ArrayElementType type,
                          std::vector<ArrayValue::Element>& elements, std::vector<size_t>& dimensions)
        -> bool {
        skipWhitespace(str, pos);
        if (pos >= str.length() || str[pos] != '[') {
            return false;
        }
        ++pos;

        skipWhitespace(str, pos);

        // Empty array
        if (pos < str.length() && str[pos] == ']') {
            ++pos;
            if (dimensions.empty()) {
                dimensions.push_back(0);
            }
            return true;
        }

        size_t current_dim = 0;
        bool first = true;

        while (pos < str.length()) {
            skipWhitespace(str, pos);

            if (str[pos] == '[') {
                // Nested array
                size_t prev_size = elements.size();
                std::vector<size_t> sub_dimensions;
                if (!parseArray(str, pos, type, elements, sub_dimensions)) {
                    return false;
                }
                if (first) {
                    dimensions.push_back(0);  // Will be set later
                    dimensions.insert(dimensions.end(), sub_dimensions.begin(), sub_dimensions.end());
                    first = false;
                }
                current_dim++;
            } else {
                // Scalar value
                auto val = parseValue(str, pos, type);
                if (!val) return false;
                elements.push_back(*val);
                if (first) {
                    dimensions.push_back(0);  // Will be set later
                    first = false;
                }
                current_dim++;
            }

            skipWhitespace(str, pos);

            if (pos >= str.length()) return false;

            if (str[pos] == ']') {
                ++pos;
                if (dimensions.size() > 0) {
                    dimensions[0] = current_dim;
                }
                return true;
            }

            if (str[pos] == ',') {
                ++pos;
                continue;
            }

            return false;
        }

        return false;
    }

} // namespace scratchbird::core
