#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include "scratchbird/core/types.h"

namespace scratchbird::core
{
    /**
     * TypedValue - Runtime-polymorphic value container
     *
     * Stores values of any SQL type with runtime type information.
     * Used for:
     * - Domain constraint validation
     * - Expression evaluation
     * - Row-level security checks
     * - Query parameters
     *
     * Storage Strategy:
     * - Small types (primitives): Inline in union
     * - String types: std::string member
     * - Binary types: std::vector<uint8_t> member
     * - Complex types (COMPOSITE, ARRAY): std::unique_ptr to heap-allocated structures
     */
    class TypedValue
    {
    public:
        // Constructors
        TypedValue();  // NULL value
        explicit TypedValue(DataType type);  // NULL value of specific type

        // Copy and move semantics
        TypedValue(const TypedValue& other);
        TypedValue(TypedValue&& other) noexcept;
        TypedValue& operator=(const TypedValue& other);
        TypedValue& operator=(TypedValue&& other) noexcept;
        ~TypedValue();

        // Factory methods for primitive types
        static TypedValue makeNull(DataType type = DataType::NULL_TYPE);
        static TypedValue makeInt32(int32_t value);
        static TypedValue makeInt64(int64_t value);
        static TypedValue makeFloat32(float value);
        static TypedValue makeFloat64(double value);
        static TypedValue makeBool(bool value);
        static TypedValue makeVarchar(const std::string& value);
        static TypedValue makeText(const std::string& value);
        static TypedValue makeChar(const std::string& value);

        // Type queries
        DataType type() const { return type_; }
        bool isNull() const { return is_null_; }

        // Getters for primitive types
        int32_t getInt32() const;
        int64_t getInt64() const;
        float getFloat32() const;
        double getFloat64() const;
        bool getBool() const;
        std::string getVarchar() const;
        std::string getText() const;
        std::string getChar() const;

        // Setters for primitive types
        void setInt32(int32_t value);
        void setInt64(int64_t value);
        void setFloat32(float value);
        void setFloat64(double value);
        void setBool(bool value);
        void setVarchar(const std::string& value);
        void setText(const std::string& value);
        void setChar(const std::string& value);

        // Comparison operators
        bool operator==(const TypedValue& other) const;
        bool operator!=(const TypedValue& other) const { return !(*this == other); }
        bool operator<(const TypedValue& other) const;
        bool operator<=(const TypedValue& other) const;
        bool operator>(const TypedValue& other) const { return other < *this; }
        bool operator>=(const TypedValue& other) const { return !(*this < other); }

    private:
        DataType type_;
        bool is_null_;

        // Storage for primitive types (inline, no heap allocation)
        union PrimitiveData
        {
            int8_t int8_val;
            int16_t int16_val;
            int32_t int32_val;
            int64_t int64_val;
            float float32_val;
            double float64_val;
            bool bool_val;
        } data_;

        // Storage for string types
        std::string string_data_;

        // Storage for binary types
        std::vector<uint8_t> binary_data_;

        // Helper methods
        void copyFrom(const TypedValue& other);
        void moveFrom(TypedValue&& other) noexcept;
        void clear();
    };

} // namespace scratchbird::core
