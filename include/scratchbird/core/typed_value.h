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
#include <memory>
#include <optional>
#include <variant>
#include "scratchbird/core/types.h"
#include "scratchbird/core/decfloat.h"
#include "scratchbird/core/range.h"
#include "scratchbird/core/network.h"
#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
#include "scratchbird/core/encryption_key_manager.h"

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
        friend class GlobalUniquenessIndex;

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
        static TypedValue makeInt8(int8_t value);
        static TypedValue makeInt16(int16_t value);
        static TypedValue makeUInt8(uint8_t value);
        static TypedValue makeUInt16(uint16_t value);
        static TypedValue makeUInt32(uint32_t value);
        static TypedValue makeUInt64(uint64_t value);
        static TypedValue makeUInt128(const std::vector<uint8_t>& value);
        static TypedValue makeFloat32(float value);
        static TypedValue makeFloat64(double value);
        static TypedValue makeBool(bool value);
        static TypedValue makeVarchar(const std::string& value);
        static TypedValue makeText(const std::string& value);
        static TypedValue makeChar(const std::string& value);
        static TypedValue makeBinary(const std::vector<uint8_t>& value);
        static TypedValue makeVarbinary(const std::vector<uint8_t>& value);
        static TypedValue makeBlob(const std::vector<uint8_t>& value);
        static TypedValue makeBytea(const std::vector<uint8_t>& value);
        static TypedValue makeXML(const std::string& value);
        static TypedValue makeDecimal(int128_t unscaled_value, uint8_t precision, uint8_t scale);
        static TypedValue makeDecfloat(const DecFloat& value);
        static TypedValue makeDecfloat(DataType type, const std::vector<uint8_t>& bytes);

        // Factory methods for spatial types
        static TypedValue makePoint(const Point& value);
        static TypedValue makeLineString(const LineString& value);
        static TypedValue makePolygon(const Polygon& value);
        static TypedValue makeMultiPoint(const MultiPoint& value);
        static TypedValue makeMultiLineString(const MultiLineString& value);
        static TypedValue makeMultiPolygon(const MultiPolygon& value);
        static TypedValue makeGeometryCollection(const GeometryCollection& value);

        // Factory methods for network types
        static TypedValue makeInet(const InetAddr& value);
        static TypedValue makeCidr(const Cidr& value);
        static TypedValue makeMacAddr(const MacAddr& value);
        static TypedValue makeMacAddr8(const MacAddr8& value);

        // Factory methods for range types
        template<typename T>
        static TypedValue makeDateRange(const Range<T>& value);
        template<typename T>
        static TypedValue makeTSRange(const Range<T>& value);
        template<typename T>
        static TypedValue makeTSTZRange(const Range<T>& value);

        // Factory methods for text search types
        static TypedValue makeTSVector(const TSVector& value);
        static TypedValue makeTSVector(const std::shared_ptr<TSVector>& value);
        static TypedValue makeTSQuery(const std::shared_ptr<TSQuery>& value);

        // Factory methods for temporal types
        static TypedValue makeDate(int64_t days_since_epoch, int32_t offset_seconds = 0);
        static TypedValue makeTime(int64_t microseconds, int32_t offset_seconds = 0);
        static TypedValue makeTimestamp(int64_t microseconds_since_epoch, int32_t offset_seconds = 0);
        static TypedValue makeBoolean(bool value) { return makeBool(value); } // Alias

        // Factory methods for other types
        static TypedValue makeUUID(const std::vector<uint8_t>& value);
        static TypedValue makeInterval(const Interval& value);
        static TypedValue makeArray(const std::vector<TypedValue>& elements);
        static TypedValue makeInt128(const std::vector<uint8_t>& value);
        static TypedValue makeMoney(int64_t value);
        static TypedValue makeVector(const std::vector<float>& value);
        static TypedValue makeInt4Range(const Range<int32_t>& value);
        static TypedValue makeInt8Range(const Range<int64_t>& value);
        static TypedValue makeNumRange(const Range<double>& value);
        static TypedValue makeComposite(const std::vector<std::string>& field_names, const std::vector<TypedValue>& field_values);
        static TypedValue makeVariant(const TypedValue& value);
        static TypedValue makeVariant(DataType type, const TypedValue& value);

        // Type queries
        DataType type() const { return type_; }
        bool isNull() const { return is_null_; }

        // Getters for primitive types
        int32_t getInt32() const;
        int64_t getInt64() const;
        uint8_t getUInt8() const;
        uint16_t getUInt16() const;
        uint32_t getUInt32() const;
        uint64_t getUInt64() const;
        float getFloat32() const;
        double getFloat64() const;
        bool getBool() const;
        std::string getVarchar() const;
        std::string getText() const;
        std::string getChar() const;

        // Getters for spatial types
        Point getPoint() const;
        LineString getLineString() const;
        Polygon getPolygon() const;
        MultiPoint getMultiPoint() const;
        MultiLineString getMultiLineString() const;
        MultiPolygon getMultiPolygon() const;
        const GeometryCollection& getGeometryCollection() const;

        // Getters for network types
        const InetAddr& getInet() const;
        const Cidr& getCidr() const;
        const MacAddr& getMacAddr() const;
        const MacAddr8& getMacAddr8() const;

        // Getters for range types
        template<typename T>
        const Range<T>& getDateRange() const;
        template<typename T>
        const Range<T>& getTSRange() const;
        template<typename T>
        const Range<T>& getTSTZRange() const;

        // Getters for text search types
        const TSVector& getTSVector() const;
        const TSQuery& getTSQuery() const;

        // Getters for temporal types
        int64_t getDate() const;
        int64_t getTime() const;
        int64_t getTimestamp() const;
        int32_t getTimezoneOffsetSeconds() const { return timezone_offset_seconds_; }

        // Getters for other types
        const std::vector<uint8_t>& getUUID() const;
        const std::vector<uint8_t>& getBinary() const;
        int128_t getInt128() const;
        uint128_t getUInt128() const;
        const Interval& getInterval() const;
        const std::vector<TypedValue>& getArray() const;
        const Range<int32_t>& getInt4Range() const;
        const Range<int64_t>& getInt8Range() const;
        const Range<double>& getNumRange() const;
        const std::vector<TypedValue>& getCompositeValues() const;
        std::vector<std::string> getCompositeFieldNames() const;
        const TypedValue& getVariantValue() const;
        std::optional<DataType> getVariantTag() const;
        int128_t getDecimalUnscaled() const { return decimal_unscaled_; }
        uint8_t getDecimalPrecision() const { return decimal_precision_; }
        uint8_t getDecimalScale() const { return decimal_scale_; }
        const std::vector<uint8_t>& getDecfloatBytes() const { return binary_data_; }

        // Utility methods
        std::string toString() const;

        // Compatibility aliases for legacy code with type coercion
        int64_t toInt64() const {
            ensureDecrypted();
            switch (type_) {
                case DataType::INT8: return static_cast<int64_t>(data_.int8_val);
                case DataType::INT16: return static_cast<int64_t>(data_.int16_val);
                case DataType::INT32: return static_cast<int64_t>(data_.int32_val);
                case DataType::INT64: return data_.int64_val;
                case DataType::MONEY: return data_.int64_val;
                case DataType::INT128: return static_cast<int64_t>(getInt128());
                case DataType::UINT8: return static_cast<int64_t>(data_.uint8_val);
                case DataType::UINT16: return static_cast<int64_t>(data_.uint16_val);
                case DataType::UINT32: return static_cast<int64_t>(data_.uint32_val);
                case DataType::UINT64: return static_cast<int64_t>(data_.uint64_val);
                case DataType::UINT128: return static_cast<int64_t>(getUInt128());
                case DataType::BOOLEAN: return data_.bool_val ? 1 : 0;
                case DataType::FLOAT32: return static_cast<int64_t>(data_.float32_val);
                case DataType::FLOAT64: return static_cast<int64_t>(data_.float64_val);
                case DataType::DECIMAL:
                case DataType::DECFLOAT16:
                case DataType::DECFLOAT34:
                    return std::stoll(toString());
                default: return getInt64();
            }
        }
        int32_t toInt32() const {
            ensureDecrypted();
            switch (type_) {
                case DataType::INT8: return static_cast<int32_t>(data_.int8_val);
                case DataType::INT16: return static_cast<int32_t>(data_.int16_val);
                case DataType::INT32: return data_.int32_val;
                case DataType::INT64: return static_cast<int32_t>(data_.int64_val);
                case DataType::MONEY: return static_cast<int32_t>(data_.int64_val);
                case DataType::INT128: return static_cast<int32_t>(getInt128());
                case DataType::UINT8: return static_cast<int32_t>(data_.uint8_val);
                case DataType::UINT16: return static_cast<int32_t>(data_.uint16_val);
                case DataType::UINT32: return static_cast<int32_t>(data_.uint32_val);
                case DataType::UINT64: return static_cast<int32_t>(data_.uint64_val);
                case DataType::UINT128: return static_cast<int32_t>(getUInt128());
                case DataType::FLOAT32: return static_cast<int32_t>(data_.float32_val);
                case DataType::FLOAT64: return static_cast<int32_t>(data_.float64_val);
                case DataType::DECIMAL:
                case DataType::DECFLOAT16:
                case DataType::DECFLOAT34:
                    return static_cast<int32_t>(std::stoll(toString()));
                default: return getInt32();
            }
        }
        double toDouble() const {
            ensureDecrypted();
            switch (type_) {
                case DataType::INT8: return static_cast<double>(data_.int8_val);
                case DataType::INT16: return static_cast<double>(data_.int16_val);
                case DataType::INT32: return static_cast<double>(data_.int32_val);
                case DataType::INT64: return static_cast<double>(data_.int64_val);
                case DataType::INT128: return static_cast<double>(getInt128());
                case DataType::UINT8: return static_cast<double>(data_.uint8_val);
                case DataType::UINT16: return static_cast<double>(data_.uint16_val);
                case DataType::UINT32: return static_cast<double>(data_.uint32_val);
                case DataType::UINT64: return static_cast<double>(data_.uint64_val);
                case DataType::UINT128: return static_cast<double>(getUInt128());
                case DataType::FLOAT32: return static_cast<double>(data_.float32_val);
                case DataType::FLOAT64: return data_.float64_val;
                case DataType::DECIMAL:
                case DataType::DECFLOAT16:
                case DataType::DECFLOAT34:
                case DataType::MONEY:
                    return std::stod(toString());
                default: return getFloat64();
            }
        }
        float toFloat() const {
            ensureDecrypted();
            switch (type_) {
                case DataType::INT32: return static_cast<float>(data_.int32_val);
                case DataType::INT64: return static_cast<float>(data_.int64_val);
                case DataType::FLOAT32: return data_.float32_val;
                case DataType::FLOAT64: return static_cast<float>(data_.float64_val);
                default: return getFloat32();
            }
        }
        bool toBoolean() const { return getBool(); }
        std::string toVarchar() const { return getVarchar(); }
        std::string toText() const { return getText(); }
        bool getBoolean() const { return getBool(); }  // Alternative alias
        bool equals(const TypedValue& other) const { return *this == other; }
        static TypedValue makeJSON(const std::string& value);
        static TypedValue makeJSONB(const std::vector<uint8_t>& value);

        // Encryption support
        bool isEncrypted() const { return is_encrypted_; }
        uint32_t encryptionKeyVersion() const { return encryption_key_version_; }
        EncryptionAlgorithm encryptionAlgorithm() const { return encryption_algorithm_; }
        const std::vector<uint8_t>& encryptedData() const { return encrypted_data_; }

        // Encrypt/decrypt value payload
        Status encrypt(const std::vector<uint8_t>& key,
                       EncryptionAlgorithm algo,
                       uint32_t key_version,
                       ErrorContext* ctx = nullptr);
        Status decrypt(const std::vector<uint8_t>& key,
                       ErrorContext* ctx = nullptr);
        Status setEncryptedData(const std::vector<uint8_t>& record,
                                ErrorContext* ctx = nullptr);

        // Type conversion method
        TypedValue convertTo(DataType target_type) const;
        Status convertTo(const TypeInfo& target_type,
                         TypedValue& result_out,
                         CastFormat format = CastFormat::DEFAULT,
                         ErrorContext* ctx = nullptr) const;

        // Plain-value serialization for canonical storage encoding
        Status serializePlainValue(std::vector<uint8_t>& out, ErrorContext* ctx) const;
        Status deserializePlainValue(const std::vector<uint8_t>& data, ErrorContext* ctx);

        // Setters for primitive types
        void setInt32(int32_t value);
        void setInt64(int64_t value);
        void setFloat32(float value);
        void setFloat64(double value);
        void setBool(bool value);
        void setVarchar(const std::string& value);
        void setText(const std::string& value);
        void setChar(const std::string& value);
        void setDecimalType(uint8_t precision, uint8_t scale);
        void setTimezoneOffsetSeconds(int32_t offset_seconds) { timezone_offset_seconds_ = offset_seconds; }

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
            uint8_t uint8_val;
            uint16_t uint16_val;
            uint32_t uint32_val;
            uint64_t uint64_val;
            float float32_val;
            double float64_val;
            bool bool_val;
        } data_;

        // Storage for string types
        std::string string_data_;

        // Storage for binary types
        std::vector<uint8_t> binary_data_;

        // Decimal storage (scaled integer + metadata)
        int128_t decimal_unscaled_ = 0;
        uint8_t decimal_precision_ = 0;
        uint8_t decimal_scale_ = 0;

        // Temporal timezone offset (seconds)
        int32_t timezone_offset_seconds_ = 0;

        // Storage for spatial types (heap-allocated for complex types)
        struct SpatialData
        {
            Point point;
            LineString linestring;
            Polygon polygon;
            MultiPoint multipoint;
            MultiLineString multilinestring;
            MultiPolygon multipolygon;
            GeometryCollection geometrycollection;
        };
        std::unique_ptr<SpatialData> spatial_data_;

        // Storage for complex types (heap-allocated)
        struct ComplexData
        {
            // Network types
            std::unique_ptr<InetAddr> inet;
            std::unique_ptr<Cidr> cidr;
            std::unique_ptr<MacAddr> macaddr;
            std::unique_ptr<MacAddr8> macaddr8;

            // Range types (using std::any for type erasure)
            std::shared_ptr<void> range_data;

            // Text search types
            std::shared_ptr<TSVector> tsvector;
            std::shared_ptr<TSQuery> tsquery;

            // Other types
            std::unique_ptr<Interval> interval;
            std::unique_ptr<std::vector<TypedValue>> array;
        };
        std::unique_ptr<ComplexData> complex_data_;

        // Encryption metadata and payload
        bool is_encrypted_ = false;
        uint32_t encryption_key_version_ = 0;
        EncryptionAlgorithm encryption_algorithm_ = EncryptionAlgorithm::NONE;
        std::vector<uint8_t> encrypted_data_;

        // Helper methods
        void copyFrom(const TypedValue& other);
        void moveFrom(TypedValue&& other) noexcept;
        void clear();
        void ensureDecrypted() const;
    };

    // Template implementations for range types
    template<typename T>
    TypedValue TypedValue::makeDateRange(const Range<T>& value)
    {
        TypedValue tv(DataType::DATERANGE);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->range_data = std::make_shared<Range<T>>(value);
        return tv;
    }

    template<typename T>
    TypedValue TypedValue::makeTSRange(const Range<T>& value)
    {
        TypedValue tv(DataType::TSRANGE);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->range_data = std::make_shared<Range<T>>(value);
        return tv;
    }

    template<typename T>
    TypedValue TypedValue::makeTSTZRange(const Range<T>& value)
    {
        TypedValue tv(DataType::TSTZRANGE);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->range_data = std::make_shared<Range<T>>(value);
        return tv;
    }

    template<typename T>
    const Range<T>& TypedValue::getDateRange() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::DATERANGE) {
            throw std::runtime_error("Type mismatch: expected DATERANGE");
        }
        if (!complex_data_ || !complex_data_->range_data) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *std::static_pointer_cast<Range<T>>(complex_data_->range_data);
    }

    template<typename T>
    const Range<T>& TypedValue::getTSRange() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::TSRANGE) {
            throw std::runtime_error("Type mismatch: expected TSRANGE");
        }
        if (!complex_data_ || !complex_data_->range_data) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *std::static_pointer_cast<Range<T>>(complex_data_->range_data);
    }

    template<typename T>
    const Range<T>& TypedValue::getTSTZRange() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::TSTZRANGE) {
            throw std::runtime_error("Type mismatch: expected TSTZRANGE");
        }
        if (!complex_data_ || !complex_data_->range_data) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *std::static_pointer_cast<Range<T>>(complex_data_->range_data);
    }

} // namespace scratchbird::core
