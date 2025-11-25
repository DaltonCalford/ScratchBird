#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/network.h"
#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
#include <cstring>
#include <cmath>
#include <stdexcept>

namespace scratchbird::core
{
    // Define ComplexData structure for network, range, and text search types
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

    TypedValue TypedValue::makeUInt8(uint8_t value)
    {
        TypedValue tv(DataType::UINT8);
        tv.is_null_ = false;
        tv.data_.uint8_val = value;
        return tv;
    }

    TypedValue TypedValue::makeUInt16(uint16_t value)
    {
        TypedValue tv(DataType::UINT16);
        tv.is_null_ = false;
        tv.data_.uint16_val = value;
        return tv;
    }

    TypedValue TypedValue::makeUInt32(uint32_t value)
    {
        TypedValue tv(DataType::UINT32);
        tv.is_null_ = false;
        tv.data_.uint32_val = value;
        return tv;
    }

    TypedValue TypedValue::makeUInt64(uint64_t value)
    {
        TypedValue tv(DataType::UINT64);
        tv.is_null_ = false;
        tv.data_.uint64_val = value;
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

    TypedValue TypedValue::makePoint(const Point& value)
    {
        TypedValue tv(DataType::POINT);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->point = value;
        return tv;
    }

    TypedValue TypedValue::makeLineString(const LineString& value)
    {
        TypedValue tv(DataType::LINESTRING);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->linestring = value;
        return tv;
    }

    TypedValue TypedValue::makePolygon(const Polygon& value)
    {
        TypedValue tv(DataType::POLYGON);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->polygon = value;
        return tv;
    }

    TypedValue TypedValue::makeMultiPoint(const MultiPoint& value)
    {
        TypedValue tv(DataType::MULTIPOINT);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->multipoint = value;
        return tv;
    }

    TypedValue TypedValue::makeMultiLineString(const MultiLineString& value)
    {
        TypedValue tv(DataType::MULTILINESTRING);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->multilinestring = value;
        return tv;
    }

    TypedValue TypedValue::makeMultiPolygon(const MultiPolygon& value)
    {
        TypedValue tv(DataType::MULTIPOLYGON);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->multipolygon = value;
        return tv;
    }

    TypedValue TypedValue::makeGeometryCollection(const GeometryCollection& value)
    {
        TypedValue tv(DataType::GEOMETRYCOLLECTION);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->geometrycollection = value;
        return tv;
    }

    // Network types
    TypedValue TypedValue::makeInet(const InetAddr& value)
    {
        TypedValue tv(DataType::INET);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->inet = std::make_unique<InetAddr>(value);
        return tv;
    }

    TypedValue TypedValue::makeCidr(const Cidr& value)
    {
        TypedValue tv(DataType::CIDR);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->cidr = std::make_unique<Cidr>(value);
        return tv;
    }

    TypedValue TypedValue::makeMacAddr(const MacAddr& value)
    {
        TypedValue tv(DataType::MACADDR);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->macaddr = std::make_unique<MacAddr>(value);
        return tv;
    }

    TypedValue TypedValue::makeMacAddr8(const MacAddr8& value)
    {
        TypedValue tv(DataType::MACADDR8);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->macaddr8 = std::make_unique<MacAddr8>(value);
        return tv;
    }

    // Text search types
    TypedValue TypedValue::makeTSVector(const TSVector& value)
    {
        TypedValue tv(DataType::TSVECTOR);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->tsvector = std::make_shared<TSVector>(value);
        return tv;
    }

    TypedValue TypedValue::makeTSVector(const std::shared_ptr<TSVector>& value)
    {
        TypedValue tv(DataType::TSVECTOR);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->tsvector = value;
        return tv;
    }

    TypedValue TypedValue::makeTSQuery(const std::shared_ptr<TSQuery>& value)
    {
        TypedValue tv(DataType::TSQUERY);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->tsquery = value;
        return tv;
    }

    // Temporal types
    TypedValue TypedValue::makeDate(int64_t days_since_epoch)
    {
        TypedValue tv(DataType::DATE);
        tv.is_null_ = false;
        tv.data_.int64_val = days_since_epoch;
        return tv;
    }

    TypedValue TypedValue::makeTime(int64_t microseconds)
    {
        TypedValue tv(DataType::TIME);
        tv.is_null_ = false;
        tv.data_.int64_val = microseconds;
        return tv;
    }

    TypedValue TypedValue::makeTimestamp(int64_t microseconds_since_epoch)
    {
        TypedValue tv(DataType::TIMESTAMP);
        tv.is_null_ = false;
        tv.data_.int64_val = microseconds_since_epoch;
        return tv;
    }

    // Other types
    TypedValue TypedValue::makeUUID(const std::vector<uint8_t>& value)
    {
        TypedValue tv(DataType::UUID);
        tv.is_null_ = false;
        tv.binary_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeInterval(const Interval& value)
    {
        TypedValue tv(DataType::INTERVAL);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->interval = std::make_unique<Interval>(value);
        return tv;
    }

    TypedValue TypedValue::makeArray(const std::vector<TypedValue>& elements)
    {
        TypedValue tv(DataType::ARRAY);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->array = std::make_unique<std::vector<TypedValue>>(elements);
        return tv;
    }

    TypedValue TypedValue::makeInt128(const std::vector<uint8_t>& value)
    {
        TypedValue tv(DataType::INT128);
        tv.is_null_ = false;
        tv.binary_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeMoney(int64_t value)
    {
        TypedValue tv(DataType::MONEY);
        tv.is_null_ = false;
        tv.data_.int64_val = value;
        return tv;
    }

    TypedValue TypedValue::makeVector(const std::vector<float>& value)
    {
        TypedValue tv(DataType::VECTOR);
        tv.is_null_ = false;
        // Store as binary data (convert floats to bytes)
        tv.binary_data_.resize(value.size() * sizeof(float));
        std::memcpy(tv.binary_data_.data(), value.data(), value.size() * sizeof(float));
        return tv;
    }

    TypedValue TypedValue::makeInt4Range(const Range<int32_t>& value)
    {
        TypedValue tv(DataType::INT4RANGE);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->range_data = std::make_shared<Range<int32_t>>(value);
        return tv;
    }

    TypedValue TypedValue::makeInt8Range(const Range<int64_t>& value)
    {
        TypedValue tv(DataType::INT8RANGE);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->range_data = std::make_shared<Range<int64_t>>(value);
        return tv;
    }

    TypedValue TypedValue::makeNumRange(const Range<double>& value)
    {
        TypedValue tv(DataType::NUMRANGE);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->range_data = std::make_shared<Range<double>>(value);
        return tv;
    }

    TypedValue TypedValue::makeComposite(const std::vector<std::string>& field_names, const std::vector<TypedValue>& field_values)
    {
        if (field_names.size() != field_values.size()) {
            throw std::runtime_error("Field names and values count mismatch");
        }
        TypedValue tv(DataType::COMPOSITE);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->array = std::make_unique<std::vector<TypedValue>>(field_values);
        // Store field names in string_data as a serialized format
        std::string names_str;
        for (const auto& name : field_names) {
            names_str += name + "\0";
        }
        tv.string_data_ = names_str;
        return tv;
    }

    TypedValue TypedValue::makeVariant(const TypedValue& value)
    {
        TypedValue tv(DataType::VARIANT);
        tv.is_null_ = value.is_null_;
        // Store the wrapped value's type and data
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->array = std::make_unique<std::vector<TypedValue>>();
        tv.complex_data_->array->push_back(value);
        return tv;
    }

    TypedValue TypedValue::makeVariant(DataType type, const TypedValue& value)
    {
        TypedValue tv(DataType::VARIANT);
        tv.is_null_ = value.is_null_;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->array = std::make_unique<std::vector<TypedValue>>();
        // Store type information as first element
        TypedValue type_val(DataType::INT32);
        type_val.data_.int32_val = static_cast<int32_t>(type);
        type_val.is_null_ = false;
        tv.complex_data_->array->push_back(type_val);
        tv.complex_data_->array->push_back(value);
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

    uint8_t TypedValue::getUInt8() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::UINT8) {
            throw std::runtime_error("Type mismatch: expected UINT8");
        }
        return data_.uint8_val;
    }

    uint16_t TypedValue::getUInt16() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::UINT16) {
            throw std::runtime_error("Type mismatch: expected UINT16");
        }
        return data_.uint16_val;
    }

    uint32_t TypedValue::getUInt32() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::UINT32) {
            throw std::runtime_error("Type mismatch: expected UINT32");
        }
        return data_.uint32_val;
    }

    uint64_t TypedValue::getUInt64() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::UINT64) {
            throw std::runtime_error("Type mismatch: expected UINT64");
        }
        return data_.uint64_val;
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

    Point TypedValue::getPoint() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::POINT) {
            throw std::runtime_error("Type mismatch: expected POINT");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->point;
    }

    LineString TypedValue::getLineString() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::LINESTRING) {
            throw std::runtime_error("Type mismatch: expected LINESTRING");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->linestring;
    }

    Polygon TypedValue::getPolygon() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::POLYGON) {
            throw std::runtime_error("Type mismatch: expected POLYGON");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->polygon;
    }

    MultiPoint TypedValue::getMultiPoint() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::MULTIPOINT) {
            throw std::runtime_error("Type mismatch: expected MULTIPOINT");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->multipoint;
    }

    MultiLineString TypedValue::getMultiLineString() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::MULTILINESTRING) {
            throw std::runtime_error("Type mismatch: expected MULTILINESTRING");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->multilinestring;
    }

    MultiPolygon TypedValue::getMultiPolygon() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::MULTIPOLYGON) {
            throw std::runtime_error("Type mismatch: expected MULTIPOLYGON");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->multipolygon;
    }

    const GeometryCollection& TypedValue::getGeometryCollection() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::GEOMETRYCOLLECTION) {
            throw std::runtime_error("Type mismatch: expected GEOMETRYCOLLECTION");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->geometrycollection;
    }

    // Network types
    const InetAddr& TypedValue::getInet() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::INET) {
            throw std::runtime_error("Type mismatch: expected INET");
        }
        if (!complex_data_ || !complex_data_->inet) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->inet;
    }

    const Cidr& TypedValue::getCidr() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::CIDR) {
            throw std::runtime_error("Type mismatch: expected CIDR");
        }
        if (!complex_data_ || !complex_data_->cidr) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->cidr;
    }

    const MacAddr& TypedValue::getMacAddr() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::MACADDR) {
            throw std::runtime_error("Type mismatch: expected MACADDR");
        }
        if (!complex_data_ || !complex_data_->macaddr) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->macaddr;
    }

    const MacAddr8& TypedValue::getMacAddr8() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::MACADDR8) {
            throw std::runtime_error("Type mismatch: expected MACADDR8");
        }
        if (!complex_data_ || !complex_data_->macaddr8) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->macaddr8;
    }

    // Text search types
    const TSVector& TypedValue::getTSVector() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::TSVECTOR) {
            throw std::runtime_error("Type mismatch: expected TSVECTOR");
        }
        if (!complex_data_ || !complex_data_->tsvector) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->tsvector;
    }

    const TSQuery& TypedValue::getTSQuery() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::TSQUERY) {
            throw std::runtime_error("Type mismatch: expected TSQUERY");
        }
        if (!complex_data_ || !complex_data_->tsquery) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->tsquery;
    }

    // Other types
    const std::vector<uint8_t>& TypedValue::getUUID() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::UUID) {
            throw std::runtime_error("Type mismatch: expected UUID");
        }
        return binary_data_;
    }

    const Interval& TypedValue::getInterval() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::INTERVAL) {
            throw std::runtime_error("Type mismatch: expected INTERVAL");
        }
        if (!complex_data_ || !complex_data_->interval) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->interval;
    }

    // Temporal types
    int64_t TypedValue::getDate() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::DATE) {
            throw std::runtime_error("Type mismatch: expected DATE");
        }
        return data_.int64_val;
    }

    int64_t TypedValue::getTime() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::TIME) {
            throw std::runtime_error("Type mismatch: expected TIME");
        }
        return data_.int64_val;
    }

    int64_t TypedValue::getTimestamp() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::TIMESTAMP) {
            throw std::runtime_error("Type mismatch: expected TIMESTAMP");
        }
        return data_.int64_val;
    }

    const std::vector<TypedValue>& TypedValue::getArray() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        if (type_ != DataType::ARRAY) {
            throw std::runtime_error("Type mismatch: expected ARRAY");
        }
        if (!complex_data_ || !complex_data_->array) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->array;
    }

    // toString method
    std::string TypedValue::toString() const
    {
        if (is_null_) {
            return "NULL";
        }

        switch (type_) {
            case DataType::INT32:
                return std::to_string(data_.int32_val);
            case DataType::INT64:
                return std::to_string(data_.int64_val);
            case DataType::UINT8:
                return std::to_string(data_.uint8_val);
            case DataType::UINT16:
                return std::to_string(data_.uint16_val);
            case DataType::UINT32:
                return std::to_string(data_.uint32_val);
            case DataType::UINT64:
                return std::to_string(data_.uint64_val);
            case DataType::FLOAT32:
                return std::to_string(data_.float32_val);
            case DataType::FLOAT64:
                return std::to_string(data_.float64_val);
            case DataType::BOOLEAN:
                return data_.bool_val ? "true" : "false";
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::CHAR:
                return string_data_;
            case DataType::DATE:
                return "DATE(" + std::to_string(data_.int64_val) + ")";
            case DataType::TIME:
                return "TIME(" + std::to_string(data_.int64_val) + ")";
            case DataType::TIMESTAMP:
                return "TIMESTAMP(" + std::to_string(data_.int64_val) + ")";
            case DataType::INET:
                if (complex_data_ && complex_data_->inet) {
                    return complex_data_->inet->toString();
                }
                return "INET(?)";
            case DataType::CIDR:
                if (complex_data_ && complex_data_->cidr) {
                    return complex_data_->cidr->toString();
                }
                return "CIDR(?)";
            case DataType::MACADDR:
                if (complex_data_ && complex_data_->macaddr) {
                    return complex_data_->macaddr->toString();
                }
                return "MACADDR(?)";
            case DataType::MACADDR8:
                if (complex_data_ && complex_data_->macaddr8) {
                    return complex_data_->macaddr8->toString();
                }
                return "MACADDR8(?)";
            default:
                return "<" + std::string(TypeSystem::getTypeName(type_)) + ">";
        }
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
            case DataType::POINT:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->point == other.spatial_data_->point;
            case DataType::LINESTRING:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->linestring == other.spatial_data_->linestring;
            case DataType::POLYGON:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->polygon == other.spatial_data_->polygon;
            case DataType::MULTIPOINT:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->multipoint == other.spatial_data_->multipoint;
            case DataType::MULTILINESTRING:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->multilinestring == other.spatial_data_->multilinestring;
            case DataType::MULTIPOLYGON:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->multipolygon == other.spatial_data_->multipolygon;
            case DataType::GEOMETRYCOLLECTION:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->geometrycollection == other.spatial_data_->geometrycollection;
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

        // Copy spatial data
        if (other.spatial_data_) {
            spatial_data_ = std::make_unique<SpatialData>(*other.spatial_data_);
        }

        // Copy complex data
        if (other.complex_data_) {
            complex_data_ = std::make_unique<ComplexData>();
            if (other.complex_data_->inet) {
                complex_data_->inet = std::make_unique<InetAddr>(*other.complex_data_->inet);
            }
            if (other.complex_data_->cidr) {
                complex_data_->cidr = std::make_unique<Cidr>(*other.complex_data_->cidr);
            }
            if (other.complex_data_->macaddr) {
                complex_data_->macaddr = std::make_unique<MacAddr>(*other.complex_data_->macaddr);
            }
            if (other.complex_data_->macaddr8) {
                complex_data_->macaddr8 = std::make_unique<MacAddr8>(*other.complex_data_->macaddr8);
            }
            if (other.complex_data_->tsvector) {
                complex_data_->tsvector = other.complex_data_->tsvector; // shared_ptr copy
            }
            if (other.complex_data_->tsquery) {
                complex_data_->tsquery = other.complex_data_->tsquery; // shared_ptr copy
            }
            if (other.complex_data_->interval) {
                complex_data_->interval = std::make_unique<Interval>(*other.complex_data_->interval);
            }
            if (other.complex_data_->array) {
                complex_data_->array = std::make_unique<std::vector<TypedValue>>(*other.complex_data_->array);
            }
            complex_data_->range_data = other.complex_data_->range_data; // shared_ptr copy
        }
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

        // Move spatial data
        spatial_data_ = std::move(other.spatial_data_);

        // Move complex data
        complex_data_ = std::move(other.complex_data_);

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

        // Clear spatial data
        spatial_data_.reset();

        // Clear complex data
        complex_data_.reset();

        // Reset to NULL
        is_null_ = true;
        type_ = DataType::NULL_TYPE;
        std::memset(&data_, 0, sizeof(data_));
    }

    // GeometryCollection equality operator implementation
    // Must be here (not in type_system.cpp) to avoid circular dependency with TypedValue
    bool GeometryCollection::operator==(const GeometryCollection& other) const
    {
        if (srid != other.srid) return false;
        if (geometries.size() != other.geometries.size()) return false;

        // Compare each geometry
        for (size_t i = 0; i < geometries.size(); ++i) {
            // Check if both pointers are null or both are valid
            bool this_null = (geometries[i] == nullptr);
            bool other_null = (other.geometries[i] == nullptr);

            if (this_null != other_null) return false;
            if (this_null) continue;  // Both are null, equal

            // Compare actual geometry values using TypedValue's operator==
            const TypedValue& this_geom = *geometries[i];
            const TypedValue& other_geom = *other.geometries[i];
            if (!(this_geom == other_geom)) return false;
        }

        return true;
    }

    // Type conversion implementation
    TypedValue TypedValue::convertTo(DataType target_type) const
    {
        if (is_null_) {
            return TypedValue(target_type);
        }

        // If already the target type, return copy
        if (type_ == target_type) {
            return *this;
        }

        // Implement common conversions
        switch (target_type) {
            case DataType::INT32:
                if (type_ == DataType::INT64) return makeInt32(static_cast<int32_t>(data_.int64_val));
                if (type_ == DataType::FLOAT64) {
                    // P0-5: Check for NaN/Infinity
                    if (std::isnan(data_.float64_val)) {
                        throw std::runtime_error("Cannot convert NaN to integer");
                    }
                    if (std::isinf(data_.float64_val)) {
                        throw std::runtime_error("Cannot convert Infinity to integer");
                    }
                    if (data_.float64_val > static_cast<double>(INT32_MAX) ||
                        data_.float64_val < static_cast<double>(INT32_MIN)) {
                        throw std::runtime_error("Float value out of range for INT32");
                    }
                    return makeInt32(static_cast<int32_t>(data_.float64_val));
                }
                if (type_ == DataType::FLOAT32) {
                    // P0-5: Check for NaN/Infinity
                    if (std::isnan(data_.float32_val)) {
                        throw std::runtime_error("Cannot convert NaN to integer");
                    }
                    if (std::isinf(data_.float32_val)) {
                        throw std::runtime_error("Cannot convert Infinity to integer");
                    }
                    if (data_.float32_val > static_cast<float>(INT32_MAX) ||
                        data_.float32_val < static_cast<float>(INT32_MIN)) {
                        throw std::runtime_error("Float value out of range for INT32");
                    }
                    return makeInt32(static_cast<int32_t>(data_.float32_val));
                }
                if (type_ == DataType::BOOLEAN) return makeInt32(data_.bool_val ? 1 : 0);
                break;

            case DataType::INT64:
                if (type_ == DataType::INT32) return makeInt64(static_cast<int64_t>(data_.int32_val));
                if (type_ == DataType::FLOAT64) {
                    // P0-5: Check for NaN/Infinity
                    if (std::isnan(data_.float64_val)) {
                        throw std::runtime_error("Cannot convert NaN to integer");
                    }
                    if (std::isinf(data_.float64_val)) {
                        throw std::runtime_error("Cannot convert Infinity to integer");
                    }
                    // Note: INT64_MAX cannot be exactly represented as double, so we use a safe threshold
                    if (data_.float64_val >= 9.223372036854776e18 ||  // > INT64_MAX
                        data_.float64_val < static_cast<double>(INT64_MIN)) {
                        throw std::runtime_error("Float value out of range for INT64");
                    }
                    return makeInt64(static_cast<int64_t>(data_.float64_val));
                }
                if (type_ == DataType::FLOAT32) {
                    // P0-5: Check for NaN/Infinity
                    if (std::isnan(data_.float32_val)) {
                        throw std::runtime_error("Cannot convert NaN to integer");
                    }
                    if (std::isinf(data_.float32_val)) {
                        throw std::runtime_error("Cannot convert Infinity to integer");
                    }
                    return makeInt64(static_cast<int64_t>(data_.float32_val));
                }
                if (type_ == DataType::BOOLEAN) return makeInt64(data_.bool_val ? 1 : 0);
                break;

            case DataType::FLOAT64:
                if (type_ == DataType::INT32) return makeFloat64(static_cast<double>(data_.int32_val));
                if (type_ == DataType::INT64) return makeFloat64(static_cast<double>(data_.int64_val));
                if (type_ == DataType::FLOAT32) return makeFloat64(static_cast<double>(data_.float32_val));
                break;

            case DataType::FLOAT32:
                if (type_ == DataType::INT32) return makeFloat32(static_cast<float>(data_.int32_val));
                if (type_ == DataType::INT64) return makeFloat32(static_cast<float>(data_.int64_val));
                if (type_ == DataType::FLOAT64) return makeFloat32(static_cast<float>(data_.float64_val));
                break;

            case DataType::BOOLEAN:
                if (type_ == DataType::INT32) return makeBool(data_.int32_val != 0);
                if (type_ == DataType::INT64) return makeBool(data_.int64_val != 0);
                if (type_ == DataType::FLOAT64) return makeBool(data_.float64_val != 0.0);
                if (type_ == DataType::FLOAT32) return makeBool(data_.float32_val != 0.0f);
                break;

            case DataType::VARCHAR:
            case DataType::TEXT:
                return makeText(toString());

            default:
                break;
        }

        // If no conversion available, throw error
        throw std::runtime_error("Cannot convert from " + std::to_string(static_cast<int>(type_)) +
                                 " to " + std::to_string(static_cast<int>(target_type)));
    }

} // namespace scratchbird::core
