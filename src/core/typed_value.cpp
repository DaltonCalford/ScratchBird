#include "scratchbird/core/typed_value.h"
#include <cstring>
#include <stdexcept>

namespace scratchbird::core
{
    // ===== Constructors and Destructor =====

    TypedValue::TypedValue()
        : type_(DataType::NULL_TYPE), is_null_(true), data_{}
    {
    }

    TypedValue::TypedValue(DataType type)
        : type_(type), is_null_(true), data_{}
    {
    }

    TypedValue::TypedValue(const TypedValue& other)
        : type_(other.type_), is_null_(other.is_null_), data_{}
    {
        copyFrom(other);
    }

    TypedValue::TypedValue(TypedValue&& other) noexcept
        : type_(other.type_), is_null_(other.is_null_), data_{}
    {
        moveFrom(std::move(other));
    }

    TypedValue& TypedValue::operator=(const TypedValue& other)
    {
        if (this != &other)
        {
            clear();
            type_ = other.type_;
            is_null_ = other.is_null_;
            copyFrom(other);
        }
        return *this;
    }

    TypedValue& TypedValue::operator=(TypedValue&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            type_ = other.type_;
            is_null_ = other.is_null_;
            moveFrom(std::move(other));
        }
        return *this;
    }

    TypedValue::~TypedValue()
    {
        clear();
    }

    // ===== Factory Methods =====

    TypedValue TypedValue::makeNull(DataType type)
    {
        return TypedValue(type);
    }

    TypedValue TypedValue::makeInt32(int32_t value)
    {
        TypedValue tv(DataType::INT32);
        tv.is_null_ = false;
        tv.data_.int32_val = value;
        return tv;
    }

    TypedValue TypedValue::makeInt64(int64_t value)
    {
        TypedValue tv(DataType::INT64);
        tv.is_null_ = false;
        tv.data_.int64_val = value;
        return tv;
    }

    TypedValue TypedValue::makeFloat32(float value)
    {
        TypedValue tv(DataType::FLOAT32);
        tv.is_null_ = false;
        tv.data_.float32_val = value;
        return tv;
    }

    TypedValue TypedValue::makeFloat64(double value)
    {
        TypedValue tv(DataType::FLOAT64);
        tv.is_null_ = false;
        tv.data_.float64_val = value;
        return tv;
    }

    TypedValue TypedValue::makeBool(bool value)
    {
        TypedValue tv(DataType::BOOLEAN);
        tv.is_null_ = false;
        tv.data_.bool_val = value;
        return tv;
    }

    TypedValue TypedValue::makeVarchar(const std::string& value)
    {
        TypedValue tv(DataType::VARCHAR);
        tv.is_null_ = false;
        tv.string_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeText(const std::string& value)
    {
        TypedValue tv(DataType::TEXT);
        tv.is_null_ = false;
        tv.string_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeChar(const std::string& value)
    {
        TypedValue tv(DataType::CHAR);
        tv.is_null_ = false;
        tv.string_data_ = value;
        return tv;
    }

    // ===== Getters =====

    int32_t TypedValue::getInt32() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::INT32) {
            throw std::runtime_error("Type mismatch: expected INT32");
        }
        return data_.int32_val;
    }

    int64_t TypedValue::getInt64() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::INT64) {
            throw std::runtime_error("Type mismatch: expected INT64");
        }
        return data_.int64_val;
    }

    float TypedValue::getFloat32() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::FLOAT32) {
            throw std::runtime_error("Type mismatch: expected FLOAT32");
        }
        return data_.float32_val;
    }

    double TypedValue::getFloat64() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::FLOAT64) {
            throw std::runtime_error("Type mismatch: expected FLOAT64");
        }
        return data_.float64_val;
    }

    bool TypedValue::getBool() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::BOOLEAN) {
            throw std::runtime_error("Type mismatch: expected BOOLEAN");
        }
        return data_.bool_val;
    }

    std::string TypedValue::getVarchar() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::VARCHAR) {
            throw std::runtime_error("Type mismatch: expected VARCHAR");
        }
        return string_data_;
    }

    std::string TypedValue::getText() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::TEXT) {
            throw std::runtime_error("Type mismatch: expected TEXT");
        }
        return string_data_;
    }

    std::string TypedValue::getChar() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::CHAR) {
            throw std::runtime_error("Type mismatch: expected CHAR");
        }
        return string_data_;
    }

    // ===== Setters =====

    void TypedValue::setInt32(int32_t value)
    {
        type_ = DataType::INT32;
        is_null_ = false;
        data_.int32_val = value;
    }

    void TypedValue::setInt64(int64_t value)
    {
        type_ = DataType::INT64;
        is_null_ = false;
        data_.int64_val = value;
    }

    void TypedValue::setFloat32(float value)
    {
        type_ = DataType::FLOAT32;
        is_null_ = false;
        data_.float32_val = value;
    }

    void TypedValue::setFloat64(double value)
    {
        type_ = DataType::FLOAT64;
        is_null_ = false;
        data_.float64_val = value;
    }

    void TypedValue::setBool(bool value)
    {
        type_ = DataType::BOOLEAN;
        is_null_ = false;
        data_.bool_val = value;
    }

    void TypedValue::setVarchar(const std::string& value)
    {
        type_ = DataType::VARCHAR;
        is_null_ = false;
        string_data_ = value;
    }

    void TypedValue::setText(const std::string& value)
    {
        type_ = DataType::TEXT;
        is_null_ = false;
        string_data_ = value;
    }

    void TypedValue::setChar(const std::string& value)
    {
        type_ = DataType::CHAR;
        is_null_ = false;
        string_data_ = value;
    }

    // ===== Comparison Operators =====

    bool TypedValue::operator==(const TypedValue& other) const
    {
        // NULL handling: NULL == NULL is false in SQL
        if (is_null_ || other.is_null_) {
            return false;
        }

        // Type must match
        if (type_ != other.type_) {
            return false;
        }

        // Compare by type
        switch (type_)
        {
            case DataType::INT32:
                return data_.int32_val == other.data_.int32_val;
            case DataType::INT64:
                return data_.int64_val == other.data_.int64_val;
            case DataType::FLOAT32:
                return data_.float32_val == other.data_.float32_val;
            case DataType::FLOAT64:
                return data_.float64_val == other.data_.float64_val;
            case DataType::BOOLEAN:
                return data_.bool_val == other.data_.bool_val;
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::CHAR:
                return string_data_ == other.string_data_;
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
                return binary_data_ == other.binary_data_;
            default:
                // For unimplemented types, consider not equal
                return false;
        }
    }

    bool TypedValue::operator<(const TypedValue& other) const
    {
        // NULL handling: NULL < anything is false
        if (is_null_ || other.is_null_) {
            return false;
        }

        // Type must match
        if (type_ != other.type_) {
            throw std::runtime_error("Cannot compare values of different types");
        }

        // Compare by type
        switch (type_)
        {
            case DataType::INT32:
                return data_.int32_val < other.data_.int32_val;
            case DataType::INT64:
                return data_.int64_val < other.data_.int64_val;
            case DataType::FLOAT32:
                return data_.float32_val < other.data_.float32_val;
            case DataType::FLOAT64:
                return data_.float64_val < other.data_.float64_val;
            case DataType::BOOLEAN:
                return data_.bool_val < other.data_.bool_val;
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::CHAR:
                return string_data_ < other.string_data_;
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
                return binary_data_ < other.binary_data_;
            default:
                throw std::runtime_error("Comparison not supported for this type");
        }
    }

    bool TypedValue::operator<=(const TypedValue& other) const
    {
        return *this == other || *this < other;
    }

    // ===== Helper Methods =====

    void TypedValue::copyFrom(const TypedValue& other)
    {
        if (other.is_null_) {
            return;  // Nothing to copy for NULL values
        }

        // Copy primitive data
        std::memcpy(&data_, &other.data_, sizeof(data_));

        // Copy string data
        string_data_ = other.string_data_;

        // Copy binary data
        binary_data_ = other.binary_data_;
    }

    void TypedValue::moveFrom(TypedValue&& other) noexcept
    {
        if (other.is_null_) {
            return;  // Nothing to move for NULL values
        }

        // Move primitive data (simple copy)
        std::memcpy(&data_, &other.data_, sizeof(data_));

        // Move string data
        string_data_ = std::move(other.string_data_);

        // Move binary data
        binary_data_ = std::move(other.binary_data_);

        // Mark other as null
        other.is_null_ = true;
        other.type_ = DataType::NULL_TYPE;
    }

    void TypedValue::clear()
    {
        // Clear string data
        string_data_.clear();

        // Clear binary data
        binary_data_.clear();

        // Reset to NULL
        is_null_ = true;
        type_ = DataType::NULL_TYPE;
        std::memset(&data_, 0, sizeof(data_));
    }

} // namespace scratchbird::core
