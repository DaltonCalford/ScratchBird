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

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <variant>
#include <memory>
#include <cstdint>

namespace scratchbird::core
{
    /**
     * CompositeFieldType - Type of fields in composite
     */
    enum class CompositeFieldType : uint8_t {
        INT32 = 0,
        INT64 = 1,
        FLOAT32 = 2,
        FLOAT64 = 3,
        STRING = 4,
        BOOL = 5,
        COMPOSITE = 6  // Nested composite
    };

    // Forward declaration
    class CompositeRecord;

    /**
     * CompositeFieldValue - Value of a field (variant)
     */
    using CompositeFieldValue = std::variant<
        int32_t,
        int64_t,
        float,
        double,
        std::string,
        bool,
        std::shared_ptr<CompositeRecord>
    >;

    /**
     * CompositeField - Field definition
     */
    struct CompositeField {
        std::string name;
        CompositeFieldType type;
        std::string nested_type_name;  // For COMPOSITE type

        CompositeField(const std::string& n, CompositeFieldType t)
            : name(n), type(t) {}
        CompositeField(const std::string& n, const std::string& nested_name)
            : name(n), type(CompositeFieldType::COMPOSITE), nested_type_name(nested_name) {}
    };

    /**
     * CompositeRecord - Runtime representation of a composite/record
     * Note: Different from the simple CompositeValue struct in types.h
     */
    class CompositeRecord {
    public:
        // Constructor
        CompositeRecord(const std::string& type_name, const std::vector<CompositeField>& fields);

        // Type queries
        auto getTypeName() const -> const std::string& { return type_name_; }
        auto getFields() const -> const std::vector<CompositeField>& { return fields_; }
        auto getFieldCount() const -> size_t { return fields_.size(); }

        // Field access by name
        auto getField(const std::string& field_name) const -> std::optional<CompositeFieldValue>;
        auto setField(const std::string& field_name, const CompositeFieldValue& value) -> bool;

        // Field access by index
        auto getFieldByIndex(size_t index) const -> std::optional<CompositeFieldValue>;
        auto setFieldByIndex(size_t index, const CompositeFieldValue& value) -> bool;

        // Check if field exists
        bool hasField(const std::string& field_name) const;

        // Get field index by name
        auto getFieldIndex(const std::string& field_name) const -> std::optional<size_t>;

        // String conversion
        std::string toString() const;

        // Get all field values
        auto getValues() const -> const std::vector<CompositeFieldValue>& { return values_; }

    private:
        std::string type_name_;
        std::vector<CompositeField> fields_;
        std::unordered_map<std::string, size_t> field_indices_;
        std::vector<CompositeFieldValue> values_;
    };

    /**
     * Composite - Composite encoding/decoding and utilities
     */
    class Composite {
    public:
        // Create composite from field definitions
        static auto create(const std::string& type_name, const std::vector<CompositeField>& fields)
            -> CompositeRecord;

        // Binary encoding/decoding
        static auto encode(const CompositeRecord& value) -> std::vector<uint8_t>;
        static auto decode(const std::vector<uint8_t>& binary) -> std::optional<CompositeRecord>;

        // Create from map
        static auto fromMap(const std::string& type_name,
                           const std::vector<CompositeField>& fields,
                           const std::unordered_map<std::string, CompositeFieldValue>& values)
            -> std::optional<CompositeRecord>;

        // Compare composites
        static bool equals(const CompositeRecord& a, const CompositeRecord& b);

    private:
        // Encoding helpers
        static void encodeFieldValue(std::vector<uint8_t>& buffer, const CompositeFieldValue& value,
                                     CompositeFieldType type);
        static auto decodeFieldValue(const std::vector<uint8_t>& binary, size_t& pos,
                                     CompositeFieldType type)
            -> std::optional<CompositeFieldValue>;
    };

} // namespace scratchbird::core
