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

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <unordered_map>

namespace scratchbird::core
{
    /**
     * JSONB - Binary JSON implementation
     *
     * Provides efficient binary storage for JSON documents with:
     * - Fast path-based access without full parsing
     * - Compact binary encoding
     * - Support for all JSON types (object, array, string, number, bool, null)
     *
     * Binary format:
     * - Type byte (1 byte): Identifies JSON type
     * - Length/count (4 bytes): For strings, arrays, objects
     * - Data: Type-specific binary data
     *
     * Example:
     *   {"name": "John", "age": 30}
     *   -> Binary: [TYPE_OBJECT][2][offset_table][key1_data][val1_data][key2_data][val2_data]
     */

    enum class JSONBType : uint8_t {
        NULL_VALUE = 0,
        TRUE = 1,
        FALSE = 2,
        NUMBER = 3,
        STRING = 4,
        ARRAY = 5,
        OBJECT = 6
    };

    /**
     * JSONBValue - Represents a parsed JSONB value
     */
    class JSONBValue {
    public:
        using Object = std::unordered_map<std::string, JSONBValue>;
        using Array = std::vector<JSONBValue>;
        using Number = double;
        using String = std::string;
        using Null = std::monostate;

        JSONBValue(); // null
        explicit JSONBValue(bool v);
        explicit JSONBValue(double v);
        explicit JSONBValue(const std::string& v);
        explicit JSONBValue(const Object& v);
        explicit JSONBValue(const Array& v);

        // Type checking
        bool isNull() const;
        bool isBool() const;
        bool isNumber() const;
        bool isString() const;
        bool isArray() const;
        bool isObject() const;

        // Value extraction
        bool getBool() const;
        double getNumber() const;
        std::string getString() const;
        const Array& getArray() const;
        const Object& getObject() const;

        // Path-based access
        // e.g., value["user"]["name"] or value[0]["id"]
        auto operator[](const std::string& key) const -> std::optional<JSONBValue>;
        auto operator[](size_t index) const -> std::optional<JSONBValue>;

        // Get value by JSON path (e.g., "user.address.city")
        auto getPath(const std::string& path) const -> std::optional<JSONBValue>;

        // Convert to JSON string
        std::string toJSON() const;

        // Equality
        bool operator==(const JSONBValue& other) const;
        bool operator!=(const JSONBValue& other) const;

    private:
        std::variant<Null, bool, Number, String, Array, Object> value_;
    };

    /**
     * JSONB - Binary JSON encoding/decoding
     */
    class JSONB {
    public:
        // Parse JSON text to JSONB binary
        static auto fromJSON(const std::string& json) -> std::optional<std::vector<uint8_t>>;

        // Decode JSONB binary to JSONBValue
        static auto decode(const std::vector<uint8_t>& binary) -> std::optional<JSONBValue>;

        // Encode JSONBValue to JSONB binary
        static auto encode(const JSONBValue& value) -> std::vector<uint8_t>;

        // Convert JSONB binary to JSON text
        static auto toJSON(const std::vector<uint8_t>& binary) -> std::optional<std::string>;

        // Path-based access on binary data (without full decode)
        static auto getPath(const std::vector<uint8_t>& binary, const std::string& path)
            -> std::optional<JSONBValue>;

        // Validate JSON text
        static bool validateJSON(const std::string& json);

    private:
        // Encoding helpers
        static void encodeValue(std::vector<uint8_t>& buffer, const JSONBValue& value);
        static void encodeString(std::vector<uint8_t>& buffer, const std::string& str);
        static void encodeNumber(std::vector<uint8_t>& buffer, double num);
        static void encodeArray(std::vector<uint8_t>& buffer, const JSONBValue::Array& arr);
        static void encodeObject(std::vector<uint8_t>& buffer, const JSONBValue::Object& obj);

        // Decoding helpers
        static auto decodeValue(const uint8_t* data, size_t size, size_t& offset) -> std::optional<JSONBValue>;
        static auto decodeString(const uint8_t* data, size_t size, size_t& offset) -> std::optional<std::string>;
        static auto decodeNumber(const uint8_t* data, size_t size, size_t& offset) -> std::optional<double>;
        static auto decodeArray(const uint8_t* data, size_t size, size_t& offset) -> std::optional<JSONBValue::Array>;
        static auto decodeObject(const uint8_t* data, size_t size, size_t& offset) -> std::optional<JSONBValue::Object>;

        // JSON parsing helpers
        static auto parseJSON(const std::string& json, size_t& pos) -> std::optional<JSONBValue>;
        static auto parseValue(const std::string& json, size_t& pos) -> std::optional<JSONBValue>;
        static auto parseObject(const std::string& json, size_t& pos) -> std::optional<JSONBValue::Object>;
        static auto parseArray(const std::string& json, size_t& pos) -> std::optional<JSONBValue::Array>;
        static auto parseString(const std::string& json, size_t& pos) -> std::optional<std::string>;
        static auto parseNumber(const std::string& json, size_t& pos) -> std::optional<double>;
        static void skipWhitespace(const std::string& json, size_t& pos);
    };

} // namespace scratchbird::core
