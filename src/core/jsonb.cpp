#include "scratchbird/core/jsonb.h"
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace scratchbird::core
{
    // ===== JSONBValue Implementation =====

    JSONBValue::JSONBValue() : value_(Null{}) {}

    JSONBValue::JSONBValue(bool v) : value_(v) {}

    JSONBValue::JSONBValue(double v) : value_(v) {}

    JSONBValue::JSONBValue(const std::string& v) : value_(v) {}

    JSONBValue::JSONBValue(const Object& v) : value_(v) {}

    JSONBValue::JSONBValue(const Array& v) : value_(v) {}

    bool JSONBValue::isNull() const { return std::holds_alternative<Null>(value_); }
    bool JSONBValue::isBool() const { return std::holds_alternative<bool>(value_); }
    bool JSONBValue::isNumber() const { return std::holds_alternative<Number>(value_); }
    bool JSONBValue::isString() const { return std::holds_alternative<String>(value_); }
    bool JSONBValue::isArray() const { return std::holds_alternative<Array>(value_); }
    bool JSONBValue::isObject() const { return std::holds_alternative<Object>(value_); }

    bool JSONBValue::getBool() const { return std::get<bool>(value_); }
    double JSONBValue::getNumber() const { return std::get<Number>(value_); }
    std::string JSONBValue::getString() const { return std::get<String>(value_); }
    const JSONBValue::Array& JSONBValue::getArray() const { return std::get<Array>(value_); }
    const JSONBValue::Object& JSONBValue::getObject() const { return std::get<Object>(value_); }

    auto JSONBValue::operator[](const std::string& key) const -> std::optional<JSONBValue> {
        if (!isObject()) return std::nullopt;
        const auto& obj = getObject();
        auto it = obj.find(key);
        if (it == obj.end()) return std::nullopt;
        return it->second;
    }

    auto JSONBValue::operator[](size_t index) const -> std::optional<JSONBValue> {
        if (!isArray()) return std::nullopt;
        const auto& arr = getArray();
        if (index >= arr.size()) return std::nullopt;
        return arr[index];
    }

    auto JSONBValue::getPath(const std::string& path) const -> std::optional<JSONBValue> {
        if (path.empty()) return *this;

        // Simple path parsing: split by '.'
        std::vector<std::string> parts;
        size_t start = 0;
        size_t pos = 0;
        while ((pos = path.find('.', start)) != std::string::npos) {
            parts.push_back(path.substr(start, pos - start));
            start = pos + 1;
        }
        parts.push_back(path.substr(start));

        // Navigate path
        std::optional<JSONBValue> current = *this;
        for (const auto& part : parts) {
            if (!current.has_value()) return std::nullopt;
            current = (*current)[part];
        }
        return current;
    }

    std::string JSONBValue::toJSON() const {
        if (isNull()) return "null";
        if (isBool()) return getBool() ? "true" : "false";
        if (isNumber()) {
            std::ostringstream oss;
            oss << getNumber();
            return oss.str();
        }
        if (isString()) {
            // Escape string
            std::string result = "\"";
            for (char c : getString()) {
                switch (c) {
                    case '\"': result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default: result += c;
                }
            }
            result += "\"";
            return result;
        }
        if (isArray()) {
            std::string result = "[";
            const auto& arr = getArray();
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) result += ",";
                result += arr[i].toJSON();
            }
            result += "]";
            return result;
        }
        if (isObject()) {
            std::string result = "{";
            const auto& obj = getObject();
            bool first = true;
            for (const auto& [key, val] : obj) {
                if (!first) result += ",";
                first = false;
                result += "\"" + key + "\":" + val.toJSON();
            }
            result += "}";
            return result;
        }
        return "null";
    }

    bool JSONBValue::operator==(const JSONBValue& other) const {
        return value_ == other.value_;
    }

    bool JSONBValue::operator!=(const JSONBValue& other) const {
        return !(*this == other);
    }

    // ===== JSONB Implementation =====

    auto JSONB::fromJSON(const std::string& json) -> std::optional<std::vector<uint8_t>> {
        size_t pos = 0;
        auto value = parseJSON(json, pos);
        if (!value) return std::nullopt;
        return encode(*value);
    }

    auto JSONB::decode(const std::vector<uint8_t>& binary) -> std::optional<JSONBValue> {
        if (binary.empty()) return std::nullopt;
        size_t offset = 0;
        return decodeValue(binary.data(), binary.size(), offset);
    }

    auto JSONB::encode(const JSONBValue& value) -> std::vector<uint8_t> {
        std::vector<uint8_t> buffer;
        encodeValue(buffer, value);
        return buffer;
    }

    auto JSONB::toJSON(const std::vector<uint8_t>& binary) -> std::optional<std::string> {
        auto value = decode(binary);
        if (!value) return std::nullopt;
        return value->toJSON();
    }

    auto JSONB::getPath(const std::vector<uint8_t>& binary, const std::string& path)
        -> std::optional<JSONBValue> {
        auto value = decode(binary);
        if (!value) return std::nullopt;
        return value->getPath(path);
    }

    bool JSONB::validateJSON(const std::string& json) {
        size_t pos = 0;
        return parseJSON(json, pos).has_value();
    }

    // Encoding helpers

    void JSONB::encodeValue(std::vector<uint8_t>& buffer, const JSONBValue& value) {
        if (value.isNull()) {
            buffer.push_back(static_cast<uint8_t>(JSONBType::NULL_VALUE));
        } else if (value.isBool()) {
            buffer.push_back(value.getBool() ?
                static_cast<uint8_t>(JSONBType::TRUE) :
                static_cast<uint8_t>(JSONBType::FALSE));
        } else if (value.isNumber()) {
            buffer.push_back(static_cast<uint8_t>(JSONBType::NUMBER));
            encodeNumber(buffer, value.getNumber());
        } else if (value.isString()) {
            buffer.push_back(static_cast<uint8_t>(JSONBType::STRING));
            encodeString(buffer, value.getString());
        } else if (value.isArray()) {
            buffer.push_back(static_cast<uint8_t>(JSONBType::ARRAY));
            encodeArray(buffer, value.getArray());
        } else if (value.isObject()) {
            buffer.push_back(static_cast<uint8_t>(JSONBType::OBJECT));
            encodeObject(buffer, value.getObject());
        }
    }

    void JSONB::encodeString(std::vector<uint8_t>& buffer, const std::string& str) {
        uint32_t len = str.length();
        buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&len), reinterpret_cast<const uint8_t*>(&len) + 4);
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

    void JSONB::encodeNumber(std::vector<uint8_t>& buffer, double num) {
        buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&num), reinterpret_cast<const uint8_t*>(&num) + 8);
    }

    void JSONB::encodeArray(std::vector<uint8_t>& buffer, const JSONBValue::Array& arr) {
        uint32_t count = arr.size();
        buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&count), reinterpret_cast<const uint8_t*>(&count) + 4);
        for (const auto& elem : arr) {
            encodeValue(buffer, elem);
        }
    }

    void JSONB::encodeObject(std::vector<uint8_t>& buffer, const JSONBValue::Object& obj) {
        uint32_t count = obj.size();
        buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&count), reinterpret_cast<const uint8_t*>(&count) + 4);
        for (const auto& [key, val] : obj) {
            encodeString(buffer, key);
            encodeValue(buffer, val);
        }
    }

    // Decoding helpers

    auto JSONB::decodeValue(const uint8_t* data, size_t size, size_t& offset) -> std::optional<JSONBValue> {
        if (offset >= size) return std::nullopt;

        JSONBType type = static_cast<JSONBType>(data[offset++]);

        switch (type) {
            case JSONBType::NULL_VALUE:
                return JSONBValue();
            case JSONBType::TRUE:
                return JSONBValue(true);
            case JSONBType::FALSE:
                return JSONBValue(false);
            case JSONBType::NUMBER: {
                auto num = decodeNumber(data, size, offset);
                if (!num) return std::nullopt;
                return JSONBValue(*num);
            }
            case JSONBType::STRING: {
                auto str = decodeString(data, size, offset);
                if (!str) return std::nullopt;
                return JSONBValue(*str);
            }
            case JSONBType::ARRAY: {
                auto arr = decodeArray(data, size, offset);
                if (!arr) return std::nullopt;
                return JSONBValue(*arr);
            }
            case JSONBType::OBJECT: {
                auto obj = decodeObject(data, size, offset);
                if (!obj) return std::nullopt;
                return JSONBValue(*obj);
            }
            default:
                return std::nullopt;
        }
    }

    auto JSONB::decodeString(const uint8_t* data, size_t size, size_t& offset) -> std::optional<std::string> {
        if (offset + 4 > size) return std::nullopt;
        uint32_t len;
        std::memcpy(&len, data + offset, 4);
        offset += 4;
        if (offset + len > size) return std::nullopt;
        std::string result(reinterpret_cast<const char*>(data + offset), len);
        offset += len;
        return result;
    }

    auto JSONB::decodeNumber(const uint8_t* data, size_t size, size_t& offset) -> std::optional<double> {
        if (offset + 8 > size) return std::nullopt;
        double result;
        std::memcpy(&result, data + offset, 8);
        offset += 8;
        return result;
    }

    auto JSONB::decodeArray(const uint8_t* data, size_t size, size_t& offset) -> std::optional<JSONBValue::Array> {
        if (offset + 4 > size) return std::nullopt;
        uint32_t count;
        std::memcpy(&count, data + offset, 4);
        offset += 4;

        JSONBValue::Array result;
        for (uint32_t i = 0; i < count; ++i) {
            auto elem = decodeValue(data, size, offset);
            if (!elem) return std::nullopt;
            result.push_back(*elem);
        }
        return result;
    }

    auto JSONB::decodeObject(const uint8_t* data, size_t size, size_t& offset) -> std::optional<JSONBValue::Object> {
        if (offset + 4 > size) return std::nullopt;
        uint32_t count;
        std::memcpy(&count, data + offset, 4);
        offset += 4;

        JSONBValue::Object result;
        for (uint32_t i = 0; i < count; ++i) {
            auto key = decodeString(data, size, offset);
            if (!key) return std::nullopt;
            auto val = decodeValue(data, size, offset);
            if (!val) return std::nullopt;
            result[*key] = *val;
        }
        return result;
    }

    // JSON parsing helpers

    void JSONB::skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.length() && std::isspace(json[pos])) {
            ++pos;
        }
    }

    auto JSONB::parseJSON(const std::string& json, size_t& pos) -> std::optional<JSONBValue> {
        skipWhitespace(json, pos);
        return parseValue(json, pos);
    }

    auto JSONB::parseValue(const std::string& json, size_t& pos) -> std::optional<JSONBValue> {
        skipWhitespace(json, pos);
        if (pos >= json.length()) return std::nullopt;

        char c = json[pos];
        if (c == '{') {
            auto obj = parseObject(json, pos);
            if (!obj) return std::nullopt;
            return JSONBValue(*obj);
        }
        if (c == '[') {
            auto arr = parseArray(json, pos);
            if (!arr) return std::nullopt;
            return JSONBValue(*arr);
        }
        if (c == '"') {
            auto str = parseString(json, pos);
            if (!str) return std::nullopt;
            return JSONBValue(*str);
        }
        if (c == 't' && json.substr(pos, 4) == "true") {
            pos += 4;
            return JSONBValue(true);
        }
        if (c == 'f' && json.substr(pos, 5) == "false") {
            pos += 5;
            return JSONBValue(false);
        }
        if (c == 'n' && json.substr(pos, 4) == "null") {
            pos += 4;
            return JSONBValue();
        }
        if (c == '-' || std::isdigit(c)) {
            auto num = parseNumber(json, pos);
            if (!num) return std::nullopt;
            return JSONBValue(*num);
        }
        return std::nullopt;
    }

    auto JSONB::parseObject(const std::string& json, size_t& pos) -> std::optional<JSONBValue::Object> {
        if (json[pos] != '{') return std::nullopt;
        ++pos;

        JSONBValue::Object result;
        skipWhitespace(json, pos);

        if (pos < json.length() && json[pos] == '}') {
            ++pos;
            return result;
        }

        while (pos < json.length()) {
            skipWhitespace(json, pos);
            auto key = parseString(json, pos);
            if (!key) return std::nullopt;

            skipWhitespace(json, pos);
            if (pos >= json.length() || json[pos] != ':') return std::nullopt;
            ++pos;

            auto val = parseValue(json, pos);
            if (!val) return std::nullopt;

            result[*key] = *val;

            skipWhitespace(json, pos);
            if (pos >= json.length()) return std::nullopt;
            if (json[pos] == '}') {
                ++pos;
                return result;
            }
            if (json[pos] != ',') return std::nullopt;
            ++pos;
        }
        return std::nullopt;
    }

    auto JSONB::parseArray(const std::string& json, size_t& pos) -> std::optional<JSONBValue::Array> {
        if (json[pos] != '[') return std::nullopt;
        ++pos;

        JSONBValue::Array result;
        skipWhitespace(json, pos);

        if (pos < json.length() && json[pos] == ']') {
            ++pos;
            return result;
        }

        while (pos < json.length()) {
            auto val = parseValue(json, pos);
            if (!val) return std::nullopt;
            result.push_back(*val);

            skipWhitespace(json, pos);
            if (pos >= json.length()) return std::nullopt;
            if (json[pos] == ']') {
                ++pos;
                return result;
            }
            if (json[pos] != ',') return std::nullopt;
            ++pos;
        }
        return std::nullopt;
    }

    auto JSONB::parseString(const std::string& json, size_t& pos) -> std::optional<std::string> {
        if (json[pos] != '"') return std::nullopt;
        ++pos;

        std::string result;
        while (pos < json.length()) {
            char c = json[pos];
            if (c == '"') {
                ++pos;
                return result;
            }
            if (c == '\\') {
                ++pos;
                if (pos >= json.length()) return std::nullopt;
                char escape = json[pos];
                switch (escape) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: return std::nullopt;
                }
            } else {
                result += c;
            }
            ++pos;
        }
        return std::nullopt;
    }

    auto JSONB::parseNumber(const std::string& json, size_t& pos) -> std::optional<double> {
        size_t start = pos;
        if (pos < json.length() && json[pos] == '-') ++pos;

        if (pos >= json.length() || !std::isdigit(json[pos])) return std::nullopt;

        while (pos < json.length() && std::isdigit(json[pos])) ++pos;

        if (pos < json.length() && json[pos] == '.') {
            ++pos;
            if (pos >= json.length() || !std::isdigit(json[pos])) return std::nullopt;
            while (pos < json.length() && std::isdigit(json[pos])) ++pos;
        }

        if (pos < json.length() && (json[pos] == 'e' || json[pos] == 'E')) {
            ++pos;
            if (pos < json.length() && (json[pos] == '+' || json[pos] == '-')) ++pos;
            if (pos >= json.length() || !std::isdigit(json[pos])) return std::nullopt;
            while (pos < json.length() && std::isdigit(json[pos])) ++pos;
        }

        try {
            return std::stod(json.substr(start, pos - start));
        } catch (...) {
            return std::nullopt;
        }
    }

} // namespace scratchbird::core
