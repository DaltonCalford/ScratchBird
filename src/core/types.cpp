#include "scratchbird/core/types.h"
#include "scratchbird/core/timezone.h"
#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
#include "scratchbird/core/range.h"
#include "scratchbird/core/vector.h"
#include "scratchbird/core/array.h"
#include <cstring>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <charconv>

namespace scratchbird::core
{
    // Thread-local timezone manager for formatting
    static thread_local TimezoneManager g_tz_manager;

    // ===== VariantValue Implementation =====

    bool VariantValue::operator==(const VariantValue& other) const
    {
        if (actual_type != other.actual_type) {
            return false;
        }

        if (!value || !other.value) {
            // If either is null pointer
            return value == other.value;
        }

        // Deep comparison of TypedValue
        auto cmp = value->equals(*other.value);
        return cmp.has_value() && cmp.value();
    }

    // ===== CompositeValue Implementation =====

    bool CompositeValue::operator==(const CompositeValue& other) const
    {
        if (field_names.size() != other.field_names.size()) {
            return false;
        }

        // Compare field names
        for (size_t i = 0; i < field_names.size(); ++i) {
            if (field_names[i] != other.field_names[i]) {
                return false;
            }
        }

        // Compare field values
        for (size_t i = 0; i < field_values.size(); ++i) {
            if (!field_values[i] || !other.field_values[i]) {
                // If either is null pointer
                if (field_values[i] != other.field_values[i]) {
                    return false;
                }
                continue;
            }

            // Deep comparison of TypedValue
            auto cmp = field_values[i]->equals(*other.field_values[i]);
            if (!cmp.has_value() || !cmp.value()) {
                return false;
            }
        }

        return true;
    }

    // ===== TypedValue Implementation =====

    TypedValue TypedValue::makeInt8(int8_t v)
    {
        return TypedValue(DataType::INT8, v);
    }

    TypedValue TypedValue::makeInt16(int16_t v)
    {
        return TypedValue(DataType::INT16, v);
    }

    TypedValue TypedValue::makeInt32(int32_t v)
    {
        return TypedValue(DataType::INT32, v);
    }

    TypedValue TypedValue::makeInt64(int64_t v)
    {
        return TypedValue(DataType::INT64, v);
    }

    TypedValue TypedValue::makeInt128(int128_t v)
    {
        return TypedValue(DataType::INT128, v);
    }

    TypedValue TypedValue::makeUInt8(uint8_t v)
    {
        return TypedValue(DataType::UINT8, v);
    }

    TypedValue TypedValue::makeUInt16(uint16_t v)
    {
        return TypedValue(DataType::UINT16, v);
    }

    TypedValue TypedValue::makeUInt32(uint32_t v)
    {
        return TypedValue(DataType::UINT32, v);
    }

    TypedValue TypedValue::makeUInt64(uint64_t v)
    {
        return TypedValue(DataType::UINT64, v);
    }

    TypedValue TypedValue::makeFloat32(float v)
    {
        return TypedValue(DataType::FLOAT32, v);
    }

    TypedValue TypedValue::makeFloat64(double v)
    {
        return TypedValue(DataType::FLOAT64, v);
    }

    TypedValue TypedValue::makeDecimal(const std::string &v)
    {
        return TypedValue(DataType::DECIMAL, v);
    }

    TypedValue TypedValue::makeMoney(int64_t cents)
    {
        return TypedValue(DataType::MONEY, cents);
    }

    TypedValue TypedValue::makeChar(const std::string &v)
    {
        return TypedValue(DataType::CHAR, v);
    }

    TypedValue TypedValue::makeVarchar(const std::string &v)
    {
        return TypedValue(DataType::VARCHAR, v);
    }

    TypedValue TypedValue::makeText(const std::string &v)
    {
        return TypedValue(DataType::TEXT, v);
    }

    TypedValue TypedValue::makeBinary(const std::vector<uint8_t> &v)
    {
        return TypedValue(DataType::BINARY, v);
    }

    TypedValue TypedValue::makeBinary(const uint8_t *data, size_t len)
    {
        std::vector<uint8_t> v(data, data + len);
        return TypedValue(DataType::BINARY, v);
    }

    TypedValue TypedValue::makeBoolean(bool v)
    {
        return TypedValue(DataType::BOOLEAN, v);
    }

    TypedValue TypedValue::makeDate(int64_t days_since_epoch)
    {
        return TypedValue(DataType::DATE, days_since_epoch);
    }

    TypedValue TypedValue::makeTime(int64_t microseconds_since_midnight)
    {
        return TypedValue(DataType::TIME, microseconds_since_midnight);
    }

    TypedValue TypedValue::makeTimestamp(int64_t microseconds_since_epoch)
    {
        return TypedValue(DataType::TIMESTAMP, microseconds_since_epoch);
    }

    TypedValue TypedValue::makeInterval(const Interval &interval)
    {
        return TypedValue(DataType::INTERVAL, interval);
    }

    TypedValue TypedValue::makeInterval(int32_t months, int32_t days, int64_t microseconds)
    {
        return TypedValue(DataType::INTERVAL, Interval(months, days, microseconds));
    }

    TypedValue TypedValue::makeUUID(const std::vector<uint8_t> &v)
    {
        return TypedValue(DataType::UUID, v);
    }

    TypedValue TypedValue::makeUUID(const uint8_t *data, size_t len)
    {
        std::vector<uint8_t> v(data, data + len);
        return TypedValue(DataType::UUID, v);
    }

    TypedValue TypedValue::makeJSON(const std::string &v)
    {
        return TypedValue(DataType::JSON, v);
    }

    TypedValue TypedValue::makePoint(const Point &v)
    {
        return TypedValue(DataType::POINT, v);
    }

    TypedValue TypedValue::makePoint(double x, double y)
    {
        return TypedValue(DataType::POINT, Point(x, y));
    }

    TypedValue TypedValue::makeLineString(const LineString &v)
    {
        return TypedValue(DataType::LINESTRING, v);
    }

    TypedValue TypedValue::makeLineString(const std::vector<Point> &points)
    {
        return TypedValue(DataType::LINESTRING, LineString(points));
    }

    TypedValue TypedValue::makePolygon(const Polygon &v)
    {
        return TypedValue(DataType::POLYGON, v);
    }

    TypedValue TypedValue::makePolygon(const std::vector<Point> &exterior_ring)
    {
        return TypedValue(DataType::POLYGON, Polygon(exterior_ring));
    }

    TypedValue TypedValue::makePolygon(const std::vector<std::vector<Point>> &rings)
    {
        return TypedValue(DataType::POLYGON, Polygon(rings));
    }

    TypedValue TypedValue::makeMultiPoint(const MultiPoint &v)
    {
        return TypedValue(DataType::MULTIPOINT, v);
    }

    TypedValue TypedValue::makeMultiPoint(const std::vector<Point> &points)
    {
        return TypedValue(DataType::MULTIPOINT, MultiPoint(points));
    }

    TypedValue TypedValue::makeMultiLineString(const MultiLineString &v)
    {
        return TypedValue(DataType::MULTILINESTRING, v);
    }

    TypedValue TypedValue::makeMultiLineString(const std::vector<LineString> &linestrings)
    {
        return TypedValue(DataType::MULTILINESTRING, MultiLineString(linestrings));
    }

    TypedValue TypedValue::makeMultiPolygon(const MultiPolygon &v)
    {
        return TypedValue(DataType::MULTIPOLYGON, v);
    }

    TypedValue TypedValue::makeMultiPolygon(const std::vector<Polygon> &polygons)
    {
        return TypedValue(DataType::MULTIPOLYGON, MultiPolygon(polygons));
    }

    TypedValue TypedValue::makeGeometryCollection(const GeometryCollection &v)
    {
        return TypedValue(DataType::GEOMETRYCOLLECTION, v);
    }

    TypedValue TypedValue::makeGeometryCollection(const std::vector<std::shared_ptr<TypedValue>> &geometries)
    {
        return TypedValue(DataType::GEOMETRYCOLLECTION, GeometryCollection(geometries));
    }

    TypedValue TypedValue::makeTSVector(const TSVector &v)
    {
        return TypedValue(DataType::TSVECTOR, std::make_shared<TSVector>(v));
    }

    TypedValue TypedValue::makeTSVector(std::shared_ptr<TSVector> v)
    {
        return TypedValue(DataType::TSVECTOR, v);
    }

    TypedValue TypedValue::makeTSQuery(const TSQuery &v)
    {
        // TSQuery is non-copyable (contains unique_ptr), so we need to create a copy manually
        // by serializing and deserializing
        auto binary = v.toBinary();
        auto copy = TSQuery::fromBinary(binary.data(), binary.size());
        if (!copy.has_value())
        {
            throw std::runtime_error("Failed to copy TSQuery");
        }
        return TypedValue(DataType::TSQUERY, std::make_shared<TSQuery>(std::move(*copy)));
    }

    TypedValue TypedValue::makeTSQuery(std::shared_ptr<TSQuery> v)
    {
        return TypedValue(DataType::TSQUERY, v);
    }

    TypedValue TypedValue::makeInt4Range(const Int4Range &v)
    {
        return TypedValue(DataType::INT4RANGE, v);
    }

    TypedValue TypedValue::makeInt8Range(const Int8Range &v)
    {
        return TypedValue(DataType::INT8RANGE, v);
    }

    TypedValue TypedValue::makeNumRange(const NumRange &v)
    {
        return TypedValue(DataType::NUMRANGE, v);
    }

    TypedValue TypedValue::makeDateRange(const DateRange &v)
    {
        return TypedValue(DataType::DATERANGE, v);
    }

    TypedValue TypedValue::makeTSRange(const TSRange &v)
    {
        return TypedValue(DataType::TSRANGE, v);
    }

    TypedValue TypedValue::makeTSTZRange(const TSTZRange &v)
    {
        return TypedValue(DataType::TSTZRANGE, v);
    }

    TypedValue TypedValue::makeInet(const InetAddr &v)
    {
        return TypedValue(DataType::INET, v);
    }

    TypedValue TypedValue::makeCidr(const Cidr &v)
    {
        return TypedValue(DataType::CIDR, v);
    }

    TypedValue TypedValue::makeMacAddr(const MacAddr &v)
    {
        return TypedValue(DataType::MACADDR, v);
    }

    TypedValue TypedValue::makeMacAddr8(const MacAddr8 &v)
    {
        return TypedValue(DataType::MACADDR8, v);
    }

    TypedValue TypedValue::makeComposite(const CompositeValue &v)
    {
        return TypedValue(DataType::COMPOSITE, v);
    }

    TypedValue TypedValue::makeComposite(std::vector<std::string> field_names,
                                        std::vector<TypedValue> field_values)
    {
        if (field_names.size() != field_values.size()) {
            throw std::invalid_argument("Field names and values count mismatch");
        }

        // Convert TypedValue vector to shared_ptr vector
        std::vector<std::shared_ptr<TypedValue>> shared_values;
        shared_values.reserve(field_values.size());
        for (auto& val : field_values) {
            shared_values.push_back(std::make_shared<TypedValue>(std::move(val)));
        }

        CompositeValue composite(std::move(field_names), std::move(shared_values));
        return TypedValue(DataType::COMPOSITE, std::move(composite));
    }

    TypedValue TypedValue::makeVector(const VectorValue &v)
    {
        return TypedValue(DataType::VECTOR, std::make_shared<VectorValue>(v));
    }

    TypedValue TypedValue::makeVector(std::shared_ptr<VectorValue> v)
    {
        return TypedValue(DataType::VECTOR, v);
    }

    TypedValue TypedValue::makeVector(const std::vector<float> &values)
    {
        return TypedValue(DataType::VECTOR, std::make_shared<VectorValue>(values));
    }

    TypedValue TypedValue::makeVector(const std::vector<double> &values)
    {
        return TypedValue(DataType::VECTOR, std::make_shared<VectorValue>(values));
    }

    TypedValue TypedValue::makeVariant(const VariantValue &v)
    {
        return TypedValue(DataType::VARIANT, v);
    }

    TypedValue TypedValue::makeVariant(const TypedValue &value)
    {
        // Wrap the value in a VariantValue with its actual type
        VariantValue variant(value.type(), std::make_shared<TypedValue>(value));
        return TypedValue(DataType::VARIANT, std::move(variant));
    }

    TypedValue TypedValue::makeVariant(DataType actual_type, const TypedValue &value)
    {
        // Wrap the value with an explicit type tag
        VariantValue variant(actual_type, std::make_shared<TypedValue>(value));
        return TypedValue(DataType::VARIANT, std::move(variant));
    }

    // Type extraction
    int8_t TypedValue::getInt8() const
    {
        if (type_ != DataType::INT8)
            throw std::runtime_error("Type mismatch: not INT8");
        return std::get<int8_t>(data_);
    }

    int16_t TypedValue::getInt16() const
    {
        if (type_ != DataType::INT16)
            throw std::runtime_error("Type mismatch: not INT16");
        return std::get<int16_t>(data_);
    }

    int32_t TypedValue::getInt32() const
    {
        if (type_ != DataType::INT32)
            throw std::runtime_error("Type mismatch: not INT32");
        return std::get<int32_t>(data_);
    }

    int64_t TypedValue::getInt64() const
    {
        if (type_ != DataType::INT64)
            throw std::runtime_error("Type mismatch: not INT64");
        return std::get<int64_t>(data_);
    }

    int128_t TypedValue::getInt128() const
    {
        if (type_ != DataType::INT128)
            throw std::runtime_error("Type mismatch: not INT128");
        return std::get<int128_t>(data_);
    }

    uint8_t TypedValue::getUInt8() const
    {
        if (type_ != DataType::UINT8)
            throw std::runtime_error("Type mismatch: not UINT8");
        return std::get<uint8_t>(data_);
    }

    uint16_t TypedValue::getUInt16() const
    {
        if (type_ != DataType::UINT16)
            throw std::runtime_error("Type mismatch: not UINT16");
        return std::get<uint16_t>(data_);
    }

    uint32_t TypedValue::getUInt32() const
    {
        if (type_ != DataType::UINT32)
            throw std::runtime_error("Type mismatch: not UINT32");
        return std::get<uint32_t>(data_);
    }

    uint64_t TypedValue::getUInt64() const
    {
        if (type_ != DataType::UINT64)
            throw std::runtime_error("Type mismatch: not UINT64");
        return std::get<uint64_t>(data_);
    }

    float TypedValue::getFloat32() const
    {
        if (type_ != DataType::FLOAT32)
            throw std::runtime_error("Type mismatch: not FLOAT32");
        return std::get<float>(data_);
    }

    double TypedValue::getFloat64() const
    {
        if (type_ != DataType::FLOAT64)
            throw std::runtime_error("Type mismatch: not FLOAT64");
        return std::get<double>(data_);
    }

    std::string TypedValue::getDecimal() const
    {
        if (type_ != DataType::DECIMAL)
            throw std::runtime_error("Type mismatch: not DECIMAL");
        return std::get<std::string>(data_);
    }

    int64_t TypedValue::getMoney() const
    {
        if (type_ != DataType::MONEY)
            throw std::runtime_error("Type mismatch: not MONEY");
        return std::get<int64_t>(data_);
    }

    std::string TypedValue::getChar() const
    {
        if (type_ != DataType::CHAR)
            throw std::runtime_error("Type mismatch: not CHAR");
        return std::get<std::string>(data_);
    }

    std::string TypedValue::getVarchar() const
    {
        if (type_ != DataType::VARCHAR)
            throw std::runtime_error("Type mismatch: not VARCHAR");
        return std::get<std::string>(data_);
    }

    std::string TypedValue::getText() const
    {
        if (type_ != DataType::TEXT)
            throw std::runtime_error("Type mismatch: not TEXT");
        return std::get<std::string>(data_);
    }

    std::vector<uint8_t> TypedValue::getBinary() const
    {
        if (type_ != DataType::BINARY && type_ != DataType::VARBINARY && type_ != DataType::BLOB &&
            type_ != DataType::BYTEA)
            throw std::runtime_error("Type mismatch: not a binary type");
        return std::get<std::vector<uint8_t>>(data_);
    }

    bool TypedValue::getBoolean() const
    {
        if (type_ != DataType::BOOLEAN)
            throw std::runtime_error("Type mismatch: not BOOLEAN");
        return std::get<bool>(data_);
    }

    int64_t TypedValue::getDate() const
    {
        if (type_ != DataType::DATE)
            throw std::runtime_error("Type mismatch: not DATE");
        return std::get<int64_t>(data_);
    }

    int64_t TypedValue::getTime() const
    {
        if (type_ != DataType::TIME)
            throw std::runtime_error("Type mismatch: not TIME");
        return std::get<int64_t>(data_);
    }

    int64_t TypedValue::getTimestamp() const
    {
        if (type_ != DataType::TIMESTAMP)
            throw std::runtime_error("Type mismatch: not TIMESTAMP");
        return std::get<int64_t>(data_);
    }

    Interval TypedValue::getInterval() const
    {
        if (type_ != DataType::INTERVAL)
            throw std::runtime_error("Type mismatch: not INTERVAL");
        return std::get<Interval>(data_);
    }

    std::vector<uint8_t> TypedValue::getUUID() const
    {
        if (type_ != DataType::UUID)
            throw std::runtime_error("Type mismatch: not UUID");
        return std::get<std::vector<uint8_t>>(data_);
    }

    std::string TypedValue::getJSON() const
    {
        if (type_ != DataType::JSON)
            throw std::runtime_error("Type mismatch: not JSON");
        return std::get<std::string>(data_);
    }

    Point TypedValue::getPoint() const
    {
        if (type_ != DataType::POINT)
            throw std::runtime_error("Type mismatch: not POINT");
        return std::get<Point>(data_);
    }

    LineString TypedValue::getLineString() const
    {
        if (type_ != DataType::LINESTRING)
            throw std::runtime_error("Type mismatch: not LINESTRING");
        return std::get<LineString>(data_);
    }

    Polygon TypedValue::getPolygon() const
    {
        if (type_ != DataType::POLYGON)
            throw std::runtime_error("Type mismatch: not POLYGON");
        return std::get<Polygon>(data_);
    }

    MultiPoint TypedValue::getMultiPoint() const
    {
        if (type_ != DataType::MULTIPOINT)
            throw std::runtime_error("Type mismatch: not MULTIPOINT");
        return std::get<MultiPoint>(data_);
    }

    MultiLineString TypedValue::getMultiLineString() const
    {
        if (type_ != DataType::MULTILINESTRING)
            throw std::runtime_error("Type mismatch: not MULTILINESTRING");
        return std::get<MultiLineString>(data_);
    }

    MultiPolygon TypedValue::getMultiPolygon() const
    {
        if (type_ != DataType::MULTIPOLYGON)
            throw std::runtime_error("Type mismatch: not MULTIPOLYGON");
        return std::get<MultiPolygon>(data_);
    }

    GeometryCollection TypedValue::getGeometryCollection() const
    {
        if (type_ != DataType::GEOMETRYCOLLECTION)
            throw std::runtime_error("Type mismatch: not GEOMETRYCOLLECTION");
        return std::get<GeometryCollection>(data_);
    }

    std::shared_ptr<TSVector> TypedValue::getTSVector() const
    {
        if (type_ != DataType::TSVECTOR)
            throw std::runtime_error("Type mismatch: not TSVECTOR");
        return std::get<std::shared_ptr<TSVector>>(data_);
    }

    std::shared_ptr<TSQuery> TypedValue::getTSQuery() const
    {
        if (type_ != DataType::TSQUERY)
            throw std::runtime_error("Type mismatch: not TSQUERY");
        return std::get<std::shared_ptr<TSQuery>>(data_);
    }

    Int4Range TypedValue::getInt4Range() const
    {
        if (type_ != DataType::INT4RANGE)
            throw std::runtime_error("Type mismatch: not INT4RANGE");
        return std::get<Int4Range>(data_);
    }

    Int8Range TypedValue::getInt8Range() const
    {
        if (type_ != DataType::INT8RANGE)
            throw std::runtime_error("Type mismatch: not INT8RANGE");
        return std::get<Int8Range>(data_);
    }

    NumRange TypedValue::getNumRange() const
    {
        if (type_ != DataType::NUMRANGE)
            throw std::runtime_error("Type mismatch: not NUMRANGE");
        return std::get<NumRange>(data_);
    }

    DateRange TypedValue::getDateRange() const
    {
        if (type_ != DataType::DATERANGE)
            throw std::runtime_error("Type mismatch: not DATERANGE");
        return std::get<DateRange>(data_);
    }

    TSRange TypedValue::getTSRange() const
    {
        if (type_ != DataType::TSRANGE)
            throw std::runtime_error("Type mismatch: not TSRANGE");
        return std::get<TSRange>(data_);
    }

    TSTZRange TypedValue::getTSTZRange() const
    {
        if (type_ != DataType::TSTZRANGE)
            throw std::runtime_error("Type mismatch: not TSTZRANGE");
        return std::get<TSTZRange>(data_);
    }

    InetAddr TypedValue::getInet() const
    {
        if (type_ != DataType::INET)
            throw std::runtime_error("Type mismatch: not INET");
        return std::get<InetAddr>(data_);
    }

    Cidr TypedValue::getCidr() const
    {
        if (type_ != DataType::CIDR)
            throw std::runtime_error("Type mismatch: not CIDR");
        return std::get<Cidr>(data_);
    }

    MacAddr TypedValue::getMacAddr() const
    {
        if (type_ != DataType::MACADDR)
            throw std::runtime_error("Type mismatch: not MACADDR");
        return std::get<MacAddr>(data_);
    }

    MacAddr8 TypedValue::getMacAddr8() const
    {
        if (type_ != DataType::MACADDR8)
            throw std::runtime_error("Type mismatch: not MACADDR8");
        return std::get<MacAddr8>(data_);
    }

    const CompositeValue& TypedValue::getComposite() const
    {
        if (type_ != DataType::COMPOSITE)
            throw std::runtime_error("Type mismatch: not COMPOSITE");
        return std::get<CompositeValue>(data_);
    }

    TypedValue TypedValue::getField(const std::string& field_name) const
    {
        if (type_ != DataType::COMPOSITE)
            throw std::runtime_error("Type mismatch: not COMPOSITE");

        const auto& composite = std::get<CompositeValue>(data_);

        // Find field by name
        for (size_t i = 0; i < composite.field_names.size(); ++i) {
            if (composite.field_names[i] == field_name) {
                return *composite.field_values[i];
            }
        }

        throw std::runtime_error("Field not found: " + field_name);
    }

    bool TypedValue::hasField(const std::string& field_name) const
    {
        if (type_ != DataType::COMPOSITE)
            return false;

        const auto& composite = std::get<CompositeValue>(data_);

        for (const auto& name : composite.field_names) {
            if (name == field_name) {
                return true;
            }
        }

        return false;
    }

    size_t TypedValue::getFieldCount() const
    {
        if (type_ != DataType::COMPOSITE)
            throw std::runtime_error("Type mismatch: not COMPOSITE");

        const auto& composite = std::get<CompositeValue>(data_);
        return composite.field_names.size();
    }

    const std::vector<std::string>& TypedValue::getFieldNames() const
    {
        if (type_ != DataType::COMPOSITE)
            throw std::runtime_error("Type mismatch: not COMPOSITE");

        const auto& composite = std::get<CompositeValue>(data_);
        return composite.field_names;
    }

    std::shared_ptr<VectorValue> TypedValue::getVector() const
    {
        if (type_ != DataType::VECTOR)
            throw std::runtime_error("Type mismatch: not VECTOR");
        return std::get<std::shared_ptr<VectorValue>>(data_);
    }

    TypedValue TypedValue::getVectorElement(size_t index) const
    {
        if (type_ != DataType::VECTOR)
            throw std::runtime_error("Type mismatch: not VECTOR");

        auto vec = std::get<std::shared_ptr<VectorValue>>(data_);
        if (!vec)
            throw std::runtime_error("Vector is null");

        if (index >= vec->getDimensions())
            throw std::out_of_range("Vector index out of range");

        // Get element as float64
        auto elem = vec->getAsFloat64(index);
        if (!elem.has_value())
            throw std::runtime_error("Failed to access vector element");

        return TypedValue::makeFloat64(*elem);
    }

    TypedValue TypedValue::getVectorSlice(size_t start, size_t end) const
    {
        if (type_ != DataType::VECTOR)
            throw std::runtime_error("Type mismatch: not VECTOR");

        auto vec = std::get<std::shared_ptr<VectorValue>>(data_);
        if (!vec)
            throw std::runtime_error("Vector is null");

        size_t dims = vec->getDimensions();
        if (start >= dims || end > dims || start >= end)
            throw std::out_of_range("Invalid slice range");

        // Extract slice
        std::vector<double> slice_data;
        slice_data.reserve(end - start);

        for (size_t i = start; i < end; ++i) {
            auto elem = vec->getAsFloat64(i);
            if (!elem.has_value())
                throw std::runtime_error("Failed to access vector element");
            slice_data.push_back(*elem);
        }

        return TypedValue::makeVector(slice_data);
    }

    size_t TypedValue::getVectorDimensions() const
    {
        if (type_ != DataType::VECTOR)
            throw std::runtime_error("Type mismatch: not VECTOR");

        auto vec = std::get<std::shared_ptr<VectorValue>>(data_);
        if (!vec)
            return 0;

        return vec->getDimensions();
    }

    TypedValue TypedValue::vectorDistance(const TypedValue& other, DistanceMetric metric) const
    {
        if (type_ != DataType::VECTOR || other.type_ != DataType::VECTOR)
            throw std::runtime_error("Both values must be VECTOR type");

        auto vec1 = std::get<std::shared_ptr<VectorValue>>(data_);
        auto vec2 = std::get<std::shared_ptr<VectorValue>>(other.data_);

        if (!vec1 || !vec2)
            throw std::runtime_error("Vector is null");

        auto distance = vec1->distance(*vec2, metric);
        if (!distance.has_value())
            throw std::runtime_error("Distance calculation failed (dimension mismatch?)");

        return TypedValue::makeFloat64(*distance);
    }

    TypedValue TypedValue::vectorEuclideanDistance(const TypedValue& other) const
    {
        return vectorDistance(other, DistanceMetric::EUCLIDEAN);
    }

    TypedValue TypedValue::vectorManhattanDistance(const TypedValue& other) const
    {
        return vectorDistance(other, DistanceMetric::MANHATTAN);
    }

    TypedValue TypedValue::vectorCosineSimilarity(const TypedValue& other) const
    {
        return vectorDistance(other, DistanceMetric::COSINE);
    }

    TypedValue TypedValue::vectorDotProduct(const TypedValue& other) const
    {
        return vectorDistance(other, DistanceMetric::DOT_PRODUCT);
    }

    const VariantValue& TypedValue::getVariant() const
    {
        if (type_ != DataType::VARIANT)
            throw std::runtime_error("Type mismatch: not VARIANT");
        return std::get<VariantValue>(data_);
    }

    DataType TypedValue::getVariantActualType() const
    {
        if (type_ != DataType::VARIANT)
            throw std::runtime_error("Type mismatch: not VARIANT");

        const auto& variant = std::get<VariantValue>(data_);
        return variant.actual_type;
    }

    TypedValue TypedValue::makeArray(const ArrayValue &v)
    {
        TypedValue result(DataType::ARRAY);
        result.data_ = std::make_shared<ArrayValue>(v);
        return result;
    }

    TypedValue TypedValue::makeArray(std::shared_ptr<ArrayValue> v)
    {
        TypedValue result(DataType::ARRAY);
        result.data_ = v;
        return result;
    }

    std::shared_ptr<ArrayValue> TypedValue::getArray() const
    {
        if (type_ != DataType::ARRAY)
            throw std::runtime_error("Type mismatch: not ARRAY");
        return std::get<std::shared_ptr<ArrayValue>>(data_);
    }

    // GeometryCollection methods
    bool GeometryCollection::isValid() const
    {
        for (const auto& geom : geometries) {
            if (!geom) return false;

            // Each geometry must be a valid spatial type
            DataType type = geom->type();
            if (type != DataType::POINT &&
                type != DataType::LINESTRING &&
                type != DataType::POLYGON &&
                type != DataType::MULTIPOINT &&
                type != DataType::MULTILINESTRING &&
                type != DataType::MULTIPOLYGON &&
                type != DataType::GEOMETRYCOLLECTION) {
                return false;
            }
        }
        return true;
    }

    bool GeometryCollection::operator==(const GeometryCollection& other) const
    {
        if (srid != other.srid) return false;
        if (geometries.size() != other.geometries.size()) return false;

        for (size_t i = 0; i < geometries.size(); ++i) {
            // Both nullptr
            if (!geometries[i] && !other.geometries[i]) continue;
            // One is nullptr, other is not
            if (!geometries[i] || !other.geometries[i]) return false;
            // Both exist - compare types first
            if (geometries[i]->type() != other.geometries[i]->type()) return false;

            // Compare values based on type
            DataType type = geometries[i]->type();
            if (type == DataType::POINT) {
                if (geometries[i]->getPoint() != other.geometries[i]->getPoint()) return false;
            } else if (type == DataType::LINESTRING) {
                if (geometries[i]->getLineString() != other.geometries[i]->getLineString()) return false;
            } else if (type == DataType::POLYGON) {
                if (geometries[i]->getPolygon() != other.geometries[i]->getPolygon()) return false;
            } else if (type == DataType::MULTIPOINT) {
                if (geometries[i]->getMultiPoint() != other.geometries[i]->getMultiPoint()) return false;
            } else if (type == DataType::MULTILINESTRING) {
                if (geometries[i]->getMultiLineString() != other.geometries[i]->getMultiLineString()) return false;
            } else if (type == DataType::MULTIPOLYGON) {
                if (geometries[i]->getMultiPolygon() != other.geometries[i]->getMultiPolygon()) return false;
            } else if (type == DataType::GEOMETRYCOLLECTION) {
                if (geometries[i]->getGeometryCollection() != other.geometries[i]->getGeometryCollection()) return false;
            }
        }
        return true;
    }

    TypedValue TypedValue::unwrapVariant() const
    {
        if (type_ != DataType::VARIANT)
            throw std::runtime_error("Type mismatch: not VARIANT");

        const auto& variant = std::get<VariantValue>(data_);
        if (!variant.value)
            throw std::runtime_error("VARIANT contains null value");

        return *variant.value;
    }

    bool TypedValue::variantIs(DataType expected_type) const
    {
        if (type_ != DataType::VARIANT)
            return false;

        const auto& variant = std::get<VariantValue>(data_);
        return variant.actual_type == expected_type;
    }

    TypedValue TypedValue::variantCast(DataType target_type) const
    {
        if (type_ != DataType::VARIANT)
            throw std::runtime_error("Type mismatch: not VARIANT");

        const auto& variant = std::get<VariantValue>(data_);

        // Direct type match - just unwrap
        if (variant.actual_type == target_type) {
            return unwrapVariant();
        }

        // Need to convert the wrapped value to target type
        if (!variant.value)
            throw std::runtime_error("VARIANT contains null value");

        // Try to convert using convertTo
        auto converted = variant.value->convertTo(target_type);
        if (!converted.has_value())
            throw std::runtime_error("Cannot cast VARIANT to target type");

        return *converted;
    }

    std::string TypedValue::toString() const
    {
        if (isNull())
            return "NULL";

        switch (type_)
        {
            case DataType::INT8:
                return TypeConverter::int8ToString(getInt8());
            case DataType::INT16:
                return TypeConverter::int16ToString(getInt16());
            case DataType::INT32:
                return TypeConverter::int32ToString(getInt32());
            case DataType::INT64:
                return TypeConverter::int64ToString(getInt64());
            case DataType::INT128:
                return TypeConverter::int128ToString(getInt128());
            case DataType::UINT8:
                return TypeConverter::uint8ToString(getUInt8());
            case DataType::UINT16:
                return TypeConverter::uint16ToString(getUInt16());
            case DataType::UINT32:
                return TypeConverter::uint32ToString(getUInt32());
            case DataType::UINT64:
                return TypeConverter::uint64ToString(getUInt64());
            case DataType::FLOAT32:
                return TypeConverter::float32ToString(getFloat32());
            case DataType::FLOAT64:
                return TypeConverter::float64ToString(getFloat64());
            case DataType::DECIMAL:
                return getDecimal();
            case DataType::MONEY:
                return TypeConverter::moneyToString(getMoney());
            case DataType::CHAR:
            case DataType::VARCHAR:
            case DataType::TEXT:
                return std::get<std::string>(data_);
            case DataType::BOOLEAN:
                return TypeConverter::booleanToString(getBoolean());
            case DataType::DATE:
                return TypeConverter::dateToString(getDate());
            case DataType::TIME:
                return TypeConverter::timeToString(getTime());
            case DataType::TIMESTAMP:
                return TypeConverter::timestampToString(getTimestamp());
            case DataType::INTERVAL:
                return TypeConverter::intervalToString(getInterval());
            case DataType::UUID:
                return TypeConverter::uuidToString(getUUID());
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
                return TypeConverter::binaryToHex(getBinary());
            case DataType::JSON:
                return getJSON();
            case DataType::POINT: {
                auto pt = getPoint();
                std::ostringstream oss;
                oss << "POINT(" << pt.x << " " << pt.y << ")";
                return oss.str();
            }
            case DataType::LINESTRING: {
                auto line = getLineString();
                std::ostringstream oss;
                oss << "LINESTRING(";
                for (size_t i = 0; i < line.points.size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << line.points[i].x << " " << line.points[i].y;
                }
                oss << ")";
                return oss.str();
            }
            case DataType::POLYGON: {
                auto poly = getPolygon();
                std::ostringstream oss;
                oss << "POLYGON(";
                for (size_t r = 0; r < poly.rings.size(); ++r) {
                    if (r > 0) oss << ", ";
                    oss << "(";
                    const auto& ring = poly.rings[r];
                    for (size_t i = 0; i < ring.size(); ++i) {
                        if (i > 0) oss << ", ";
                        oss << ring[i].x << " " << ring[i].y;
                    }
                    oss << ")";
                }
                oss << ")";
                return oss.str();
            }
            case DataType::TSVECTOR: {
                auto vec = getTSVector();
                return vec->toString();
            }
            case DataType::TSQUERY: {
                auto query = getTSQuery();
                return query->toString();
            }
            case DataType::INT4RANGE: {
                auto range = getInt4Range();
                return range.toString();
            }
            case DataType::INT8RANGE: {
                auto range = getInt8Range();
                return range.toString();
            }
            case DataType::NUMRANGE: {
                auto range = getNumRange();
                return range.toString();
            }
            case DataType::DATERANGE: {
                auto range = getDateRange();
                return range.toString();
            }
            case DataType::TSRANGE: {
                auto range = getTSRange();
                return range.toString();
            }
            case DataType::TSTZRANGE: {
                auto range = getTSTZRange();
                return range.toString();
            }
            case DataType::INET:
                return getInet().toString();
            case DataType::CIDR:
                return getCidr().toString();
            case DataType::MACADDR:
                return getMacAddr().toString();
            case DataType::MACADDR8:
                return getMacAddr8().toString();
            case DataType::COMPOSITE:
            {
                const auto& composite = getComposite();
                std::string result = "ROW(";
                for (size_t i = 0; i < composite.field_names.size(); ++i) {
                    if (i > 0) result += ", ";
                    result += composite.field_names[i] + ": ";
                    if (composite.field_values[i]) {
                        result += composite.field_values[i]->toString();
                    } else {
                        result += "NULL";
                    }
                }
                result += ")";
                return result;
            }
            case DataType::VECTOR:
            {
                auto vec = getVector();
                if (!vec) return "NULL";
                return vec->toString();
            }
            case DataType::VARIANT:
            {
                const auto& variant = getVariant();
                std::string type_name = TypeSystem::getTypeName(variant.actual_type);
                if (!variant.value) {
                    return "VARIANT[" + type_name + "](NULL)";
                }
                return "VARIANT[" + type_name + "](" + variant.value->toString() + ")";
            }
            case DataType::ARRAY:
            {
                // PostgreSQL-style array formatting {1,2,3}
                auto arr = getArray();
                if (!arr) return "NULL";

                // Convert ArrayValue JSON format [1,2,3] to PostgreSQL format {1,2,3}
                std::string json_str = arr->toString();
                std::string pg_str;
                for (char c : json_str) {
                    if (c == '[') pg_str += '{';
                    else if (c == ']') pg_str += '}';
                    else pg_str += c;
                }
                return pg_str;
            }
            case DataType::MULTIPOINT: {
                auto mp = getMultiPoint();
                std::ostringstream oss;
                oss << "MULTIPOINT(";
                for (size_t i = 0; i < mp.points.size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << "(" << mp.points[i].x << " " << mp.points[i].y << ")";
                }
                oss << ")";
                return oss.str();
            }
            case DataType::MULTILINESTRING: {
                auto mls = getMultiLineString();
                std::ostringstream oss;
                oss << "MULTILINESTRING(";
                for (size_t i = 0; i < mls.linestrings.size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << "(";
                    for (size_t j = 0; j < mls.linestrings[i].points.size(); ++j) {
                        if (j > 0) oss << ", ";
                        oss << mls.linestrings[i].points[j].x << " " << mls.linestrings[i].points[j].y;
                    }
                    oss << ")";
                }
                oss << ")";
                return oss.str();
            }
            case DataType::MULTIPOLYGON: {
                auto mpoly = getMultiPolygon();
                std::ostringstream oss;
                oss << "MULTIPOLYGON(";
                for (size_t i = 0; i < mpoly.polygons.size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << "(";
                    for (size_t r = 0; r < mpoly.polygons[i].rings.size(); ++r) {
                        if (r > 0) oss << ", ";
                        oss << "(";
                        const auto& ring = mpoly.polygons[i].rings[r];
                        for (size_t j = 0; j < ring.size(); ++j) {
                            if (j > 0) oss << ", ";
                            oss << ring[j].x << " " << ring[j].y;
                        }
                        oss << ")";
                    }
                    oss << ")";
                }
                oss << ")";
                return oss.str();
            }
            case DataType::GEOMETRYCOLLECTION: {
                auto gc = getGeometryCollection();
                std::ostringstream oss;
                oss << "GEOMETRYCOLLECTION(";
                for (size_t i = 0; i < gc.geometries.size(); ++i) {
                    if (i > 0) oss << ", ";

                    const auto& geom_ptr = gc.geometries[i];
                    if (!geom_ptr) continue;

                    DataType geom_type = geom_ptr->type();
                    if (geom_type == DataType::POINT) {
                        auto pt = geom_ptr->getPoint();
                        oss << "POINT(" << pt.x << " " << pt.y << ")";
                    } else if (geom_type == DataType::LINESTRING) {
                        auto ls = geom_ptr->getLineString();
                        oss << "LINESTRING(";
                        for (size_t j = 0; j < ls.points.size(); ++j) {
                            if (j > 0) oss << ", ";
                            oss << ls.points[j].x << " " << ls.points[j].y;
                        }
                        oss << ")";
                    } else if (geom_type == DataType::POLYGON) {
                        auto poly = geom_ptr->getPolygon();
                        oss << "POLYGON(";
                        for (size_t r = 0; r < poly.rings.size(); ++r) {
                            if (r > 0) oss << ", ";
                            oss << "(";
                            const auto& ring = poly.rings[r];
                            for (size_t j = 0; j < ring.size(); ++j) {
                                if (j > 0) oss << ", ";
                                oss << ring[j].x << " " << ring[j].y;
                            }
                            oss << ")";
                        }
                        oss << ")";
                    }
                }
                oss << ")";
                return oss.str();
            }
            default:
                return "<unknown>";
        }
    }

    // ===== TypeSystem Implementation =====

    bool TypeSystem::isNumeric(DataType type)
    {
        return isInteger(type) || isFloatingPoint(type) || type == DataType::DECIMAL ||
               type == DataType::MONEY;
    }

    bool TypeSystem::isInteger(DataType type)
    {
        return type == DataType::INT8 || type == DataType::INT16 || type == DataType::INT32 ||
               type == DataType::INT64 || type == DataType::INT128 || type == DataType::UINT8 ||
               type == DataType::UINT16 || type == DataType::UINT32 || type == DataType::UINT64;
    }

    bool TypeSystem::isFloatingPoint(DataType type)
    {
        return type == DataType::FLOAT32 || type == DataType::FLOAT64;
    }

    bool TypeSystem::isString(DataType type)
    {
        return type == DataType::CHAR || type == DataType::VARCHAR || type == DataType::TEXT;
    }

    bool TypeSystem::isBinary(DataType type)
    {
        return type == DataType::BINARY || type == DataType::VARBINARY || type == DataType::BLOB ||
               type == DataType::BYTEA;
    }

    bool TypeSystem::isTemporal(DataType type)
    {
        return type == DataType::DATE || type == DataType::TIME || type == DataType::TIMESTAMP ||
               type == DataType::INTERVAL;
    }

    bool TypeSystem::isFixedLength(DataType type)
    {
        switch (type)
        {
            case DataType::INT8:
            case DataType::INT16:
            case DataType::INT32:
            case DataType::INT64:
            case DataType::INT128:
            case DataType::UINT8:
            case DataType::UINT16:
            case DataType::UINT32:
            case DataType::UINT64:
            case DataType::FLOAT32:
            case DataType::FLOAT64:
            case DataType::MONEY:
            case DataType::DATE:
            case DataType::TIME:
            case DataType::TIMESTAMP:
            case DataType::INTERVAL:
            case DataType::BOOLEAN:
            case DataType::CHAR:
            case DataType::BINARY:
            case DataType::UUID:
                return true;
            default:
                return false;
        }
    }

    bool TypeSystem::isVariableLength(DataType type)
    {
        return isString(type) || isBinary(type) || type == DataType::JSON ||
               type == DataType::ARRAY || type == DataType::COMPOSITE;
    }

    auto TypeSystem::getFixedSize(DataType type) -> std::optional<uint32_t>
    {
        switch (type)
        {
            case DataType::INT8:
                return 1;
            case DataType::INT16:
                return 2;
            case DataType::INT32:
                return 4;
            case DataType::INT64:
                return 8;
            case DataType::INT128:
                return 16;
            case DataType::UINT8:
                return 1;
            case DataType::UINT16:
                return 2;
            case DataType::UINT32:
                return 4;
            case DataType::UINT64:
                return 8;
            case DataType::FLOAT32:
                return 4;
            case DataType::FLOAT64:
                return 8;
            case DataType::MONEY:
                return 8;
            case DataType::DATE:
                return 8;
            case DataType::TIME:
                return 8;
            case DataType::TIMESTAMP:
                return 8;
            case DataType::INTERVAL:
                return 16;
            case DataType::BOOLEAN:
                return 1;
            case DataType::UUID:
                return 16;
            default:
                return std::nullopt;
        }
    }

    auto TypeSystem::getMinSize(DataType type) -> uint32_t
    {
        auto fixed = getFixedSize(type);
        if (fixed)
            return *fixed;
        return 0; // Variable length types have min size 0
    }

    auto TypeSystem::getMaxSize(DataType type) -> std::optional<uint32_t>
    {
        auto fixed = getFixedSize(type);
        if (fixed)
            return fixed;
        return std::nullopt; // Variable length types have no max
    }

    auto TypeSystem::getTypeName(DataType type) -> std::string
    {
        switch (type)
        {
            case DataType::UNKNOWN:
                return "UNKNOWN";
            case DataType::INT8:
                return "INT8";
            case DataType::INT16:
                return "INT16";
            case DataType::INT32:
                return "INT32";
            case DataType::INT64:
                return "INT64";
            case DataType::INT128:
                return "INT128";
            case DataType::UINT8:
                return "UINT8";
            case DataType::UINT16:
                return "UINT16";
            case DataType::UINT32:
                return "UINT32";
            case DataType::UINT64:
                return "UINT64";
            case DataType::FLOAT32:
                return "FLOAT32";
            case DataType::FLOAT64:
                return "FLOAT64";
            case DataType::DECIMAL:
                return "DECIMAL";
            case DataType::MONEY:
                return "MONEY";
            case DataType::CHAR:
                return "CHAR";
            case DataType::VARCHAR:
                return "VARCHAR";
            case DataType::TEXT:
                return "TEXT";
            case DataType::BINARY:
                return "BINARY";
            case DataType::VARBINARY:
                return "VARBINARY";
            case DataType::BLOB:
                return "BLOB";
            case DataType::BYTEA:
                return "BYTEA";
            case DataType::DATE:
                return "DATE";
            case DataType::TIME:
                return "TIME";
            case DataType::TIMESTAMP:
                return "TIMESTAMP";
            case DataType::INTERVAL:
                return "INTERVAL";
            case DataType::BOOLEAN:
                return "BOOLEAN";
            case DataType::UUID:
                return "UUID";
            case DataType::JSON:
                return "JSON";
            case DataType::JSONB:
                return "JSONB";
            case DataType::XML:
                return "XML";
            case DataType::VECTOR:
                return "VECTOR";
            case DataType::POINT:
                return "POINT";
            case DataType::LINESTRING:
                return "LINESTRING";
            case DataType::POLYGON:
                return "POLYGON";
            case DataType::ARRAY:
                return "ARRAY";
            case DataType::COMPOSITE:
                return "COMPOSITE";
            case DataType::VARIANT:
                return "VARIANT";
            case DataType::TSVECTOR:
                return "TSVECTOR";
            case DataType::TSQUERY:
                return "TSQUERY";
            case DataType::INT4RANGE:
                return "INT4RANGE";
            case DataType::INT8RANGE:
                return "INT8RANGE";
            case DataType::NUMRANGE:
                return "NUMRANGE";
            case DataType::DATERANGE:
                return "DATERANGE";
            case DataType::TSRANGE:
                return "TSRANGE";
            case DataType::TSTZRANGE:
                return "TSTZRANGE";
            case DataType::INET:
                return "INET";
            case DataType::CIDR:
                return "CIDR";
            case DataType::MACADDR:
                return "MACADDR";
            case DataType::MACADDR8:
                return "MACADDR8";
            case DataType::NULL_TYPE:
                return "NULL";
            default:
                return "UNKNOWN";
        }
    }

    auto TypeSystem::parseTypeName(const std::string &name) -> std::optional<DataType>
    {
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        if (upper == "INT8" || upper == "TINYINT")
            return DataType::INT8;
        if (upper == "INT16" || upper == "SMALLINT")
            return DataType::INT16;
        if (upper == "INT32" || upper == "INT" || upper == "INTEGER")
            return DataType::INT32;
        if (upper == "INT64" || upper == "BIGINT")
            return DataType::INT64;
        if (upper == "INT128")
            return DataType::INT128;
        if (upper == "UINT8")
            return DataType::UINT8;
        if (upper == "UINT16")
            return DataType::UINT16;
        if (upper == "UINT32")
            return DataType::UINT32;
        if (upper == "UINT64")
            return DataType::UINT64;
        if (upper == "FLOAT32" || upper == "REAL" || upper == "FLOAT")
            return DataType::FLOAT32;
        if (upper == "FLOAT64" || upper == "DOUBLE")
            return DataType::FLOAT64;
        if (upper == "DECIMAL" || upper == "NUMERIC")
            return DataType::DECIMAL;
        if (upper == "MONEY")
            return DataType::MONEY;
        if (upper == "CHAR" || upper == "CHARACTER")
            return DataType::CHAR;
        if (upper == "VARCHAR" || upper == "CHARACTER VARYING")
            return DataType::VARCHAR;
        if (upper == "TEXT")
            return DataType::TEXT;
        if (upper == "BINARY")
            return DataType::BINARY;
        if (upper == "VARBINARY")
            return DataType::VARBINARY;
        if (upper == "BLOB")
            return DataType::BLOB;
        if (upper == "BYTEA")
            return DataType::BYTEA;
        if (upper == "DATE")
            return DataType::DATE;
        if (upper == "TIME")
            return DataType::TIME;
        if (upper == "TIMESTAMP")
            return DataType::TIMESTAMP;
        if (upper == "INTERVAL")
            return DataType::INTERVAL;
        if (upper == "BOOLEAN" || upper == "BOOL")
            return DataType::BOOLEAN;
        if (upper == "UUID")
            return DataType::UUID;
        if (upper == "JSON")
            return DataType::JSON;
        if (upper == "JSONB")
            return DataType::JSONB;
        if (upper == "XML")
            return DataType::XML;
        if (upper == "VECTOR")
            return DataType::VECTOR;
        if (upper == "POINT")
            return DataType::POINT;
        if (upper == "LINESTRING")
            return DataType::LINESTRING;
        if (upper == "POLYGON")
            return DataType::POLYGON;
        if (upper == "TSVECTOR")
            return DataType::TSVECTOR;
        if (upper == "TSQUERY")
            return DataType::TSQUERY;
        if (upper == "INT4RANGE")
            return DataType::INT4RANGE;
        if (upper == "INT8RANGE")
            return DataType::INT8RANGE;
        if (upper == "NUMRANGE")
            return DataType::NUMRANGE;
        if (upper == "DATERANGE")
            return DataType::DATERANGE;
        if (upper == "TSRANGE")
            return DataType::TSRANGE;
        if (upper == "TSTZRANGE")
            return DataType::TSTZRANGE;
        if (upper == "INET")
            return DataType::INET;
        if (upper == "CIDR")
            return DataType::CIDR;
        if (upper == "MACADDR")
            return DataType::MACADDR;
        if (upper == "MACADDR8")
            return DataType::MACADDR8;

        return std::nullopt;
    }

    bool TypeSystem::isCompatible(DataType from, DataType to)
    {
        if (from == to)
            return true;

        // Numeric types are compatible with each other
        if (isNumeric(from) && isNumeric(to))
            return true;

        // String types are compatible with each other
        if (isString(from) && isString(to))
            return true;

        // Binary types are compatible with each other
        if (isBinary(from) && isBinary(to))
            return true;

        return false;
    }

    bool TypeSystem::isImplicitlyConvertible(DataType from, DataType to)
    {
        if (from == to)
            return true;

        // Integer widening is implicit
        if (from == DataType::INT8 &&
            (to == DataType::INT16 || to == DataType::INT32 || to == DataType::INT64))
            return true;
        if (from == DataType::INT16 && (to == DataType::INT32 || to == DataType::INT64))
            return true;
        if (from == DataType::INT32 && to == DataType::INT64)
            return true;

        // Integer to float is implicit
        if (isInteger(from) && isFloatingPoint(to))
            return true;

        // FLOAT32 to FLOAT64 is implicit
        if (from == DataType::FLOAT32 && to == DataType::FLOAT64)
            return true;

        // VARCHAR and TEXT are implicitly convertible
        if ((from == DataType::VARCHAR || from == DataType::CHAR) && to == DataType::TEXT)
            return true;

        return false;
    }

    bool TypeSystem::isExplicitlyConvertible(DataType from, DataType to)
    {
        // All numeric types can be explicitly converted
        if (isNumeric(from) && isNumeric(to))
            return true;

        // String to any type (with parsing)
        if (isString(from))
            return true;

        // Any type to string
        if (isString(to))
            return true;

        // UUID <-> binary
        if (from == DataType::UUID && isBinary(to))
            return true;
        if (isBinary(from) && to == DataType::UUID)
            return true;

        // JSON <-> string
        if ((from == DataType::JSON && isString(to)) || (isString(from) && to == DataType::JSON))
            return true;

        return false;
    }

    auto TypeSystem::getCoercionPrecedence(DataType type) -> int
    {
        switch (type)
        {
            case DataType::INT8:
                return 1;
            case DataType::INT16:
                return 2;
            case DataType::INT32:
                return 3;
            case DataType::INT64:
                return 4;
            case DataType::FLOAT32:
                return 5;
            case DataType::FLOAT64:
                return 6;
            case DataType::DECIMAL:
                return 7;
            case DataType::VARCHAR:
            case DataType::TEXT:
                return 8;
            default:
                return 0;
        }
    }

    auto TypeSystem::getCommonType(DataType left, DataType right) -> std::optional<DataType>
    {
        if (left == right)
            return left;

        // Use precedence for numeric types
        if (isNumeric(left) && isNumeric(right))
        {
            int left_prec = getCoercionPrecedence(left);
            int right_prec = getCoercionPrecedence(right);
            return left_prec > right_prec ? left : right;
        }

        // Strings: prefer TEXT over VARCHAR over CHAR
        if (isString(left) && isString(right))
        {
            if (left == DataType::TEXT || right == DataType::TEXT)
                return DataType::TEXT;
            if (left == DataType::VARCHAR || right == DataType::VARCHAR)
                return DataType::VARCHAR;
            return DataType::CHAR;
        }

        return std::nullopt;
    }

    // ===== TypeConverter Implementation - String to Type =====

    auto TypeConverter::stringToInt8(const std::string &str, ErrorContext *ctx)
        -> std::optional<int8_t>
    {
        try
        {
            int val = std::stoi(str);
            if (val < -128 || val > 127)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Value out of range for INT8");
                return std::nullopt;
            }
            return static_cast<int8_t>(val);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid INT8 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToInt16(const std::string &str, ErrorContext *ctx)
        -> std::optional<int16_t>
    {
        try
        {
            int val = std::stoi(str);
            if (val < -32768 || val > 32767)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Value out of range for INT16");
                return std::nullopt;
            }
            return static_cast<int16_t>(val);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid INT16 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToInt32(const std::string &str, ErrorContext *ctx)
        -> std::optional<int32_t>
    {
        try
        {
            return std::stoi(str);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid INT32 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToInt64(const std::string &str, ErrorContext *ctx)
        -> std::optional<int64_t>
    {
        try
        {
            return std::stoll(str);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid INT64 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToInt128(const std::string &str, ErrorContext *ctx)
        -> std::optional<int128_t>
    {
        try
        {
            // Parse string as int128_t
            // Handle sign
            size_t pos = 0;
            bool negative = false;
            if (!str.empty() && str[0] == '-')
            {
                negative = true;
                pos = 1;
            }
            else if (!str.empty() && str[0] == '+')
            {
                pos = 1;
            }

            if (pos >= str.size())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid INT128 value");
                return std::nullopt;
            }

            int128_t result = 0;
            for (; pos < str.size(); ++pos)
            {
                if (!std::isdigit(str[pos]))
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid INT128 value");
                    return std::nullopt;
                }

                int digit = str[pos] - '0';

                // Check for overflow before multiplying
                const int128_t max_div_10 = std::numeric_limits<int128_t>::max() / 10;
                if (result > max_div_10)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Value out of range for INT128");
                    return std::nullopt;
                }

                result = result * 10 + digit;
            }

            return negative ? -result : result;
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid INT128 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToUInt8(const std::string &str, ErrorContext *ctx)
        -> std::optional<uint8_t>
    {
        try
        {
            unsigned long val = std::stoul(str);
            if (val > 255)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Value out of range for UINT8");
                return std::nullopt;
            }
            return static_cast<uint8_t>(val);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UINT8 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToUInt16(const std::string &str, ErrorContext *ctx)
        -> std::optional<uint16_t>
    {
        try
        {
            unsigned long val = std::stoul(str);
            if (val > 65535)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Value out of range for UINT16");
                return std::nullopt;
            }
            return static_cast<uint16_t>(val);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UINT16 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToUInt32(const std::string &str, ErrorContext *ctx)
        -> std::optional<uint32_t>
    {
        try
        {
            unsigned long val = std::stoul(str);
            // On some platforms, unsigned long might be 64-bit
            if (val > std::numeric_limits<uint32_t>::max())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Value out of range for UINT32");
                return std::nullopt;
            }
            return static_cast<uint32_t>(val);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UINT32 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToUInt64(const std::string &str, ErrorContext *ctx)
        -> std::optional<uint64_t>
    {
        try
        {
            return std::stoull(str);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UINT64 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToMoney(const std::string &str, ErrorContext *ctx)
        -> std::optional<int64_t>
    {
        try
        {
            // Parse money string - supports formats like:
            // "$123.45", "123.45", "-$50.25", "-50.25"
            std::string clean_str = str;

            // Remove whitespace
            clean_str.erase(std::remove_if(clean_str.begin(), clean_str.end(), ::isspace), clean_str.end());

            if (clean_str.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid MONEY value");
                return std::nullopt;
            }

            // Handle sign
            bool negative = false;
            size_t pos = 0;
            if (clean_str[0] == '-')
            {
                negative = true;
                pos = 1;
            }
            else if (clean_str[0] == '+')
            {
                pos = 1;
            }

            // Remove currency symbol if present
            if (pos < clean_str.size() && clean_str[pos] == '$')
            {
                pos++;
            }

            if (pos >= clean_str.size())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid MONEY value");
                return std::nullopt;
            }

            // Parse the number part
            std::string num_str = clean_str.substr(pos);
            double value = std::stod(num_str);

            // Convert to cents (multiply by 100 and round)
            int64_t cents = static_cast<int64_t>(std::round(value * 100.0));

            return negative ? -cents : cents;
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid MONEY value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToFloat32(const std::string &str, ErrorContext *ctx)
        -> std::optional<float>
    {
        try
        {
            return std::stof(str);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid FLOAT32 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToFloat64(const std::string &str, ErrorContext *ctx)
        -> std::optional<double>
    {
        try
        {
            return std::stod(str);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid FLOAT64 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToBoolean(const std::string &str, ErrorContext *ctx)
        -> std::optional<bool>
    {
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower == "true" || lower == "t" || lower == "yes" || lower == "y" || lower == "1")
            return true;
        if (lower == "false" || lower == "f" || lower == "no" || lower == "n" || lower == "0")
            return false;

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid BOOLEAN value");
        return std::nullopt;
    }

    // Type to string conversions
    auto TypeConverter::int8ToString(int8_t v) -> std::string
    {
        return std::to_string(static_cast<int>(v));
    }

    auto TypeConverter::int16ToString(int16_t v) -> std::string
    {
        return std::to_string(v);
    }

    auto TypeConverter::int32ToString(int32_t v) -> std::string
    {
        return std::to_string(v);
    }

    auto TypeConverter::int64ToString(int64_t v) -> std::string
    {
        return std::to_string(v);
    }

    auto TypeConverter::int128ToString(int128_t v) -> std::string
    {
#if HAS_INT128
        // For native __int128 support
        if (v == 0) return "0";

        bool negative = v < 0;
        __int128 abs_val = negative ? -v : v;

        std::string result;
        while (abs_val > 0) {
            result = char('0' + (abs_val % 10)) + result;
            abs_val /= 10;
        }

        return negative ? "-" + result : result;
#else
        // For struct-based fallback
        std::ostringstream oss;
        oss << "INT128{high=" << v.high << ",low=" << v.low << "}";
        return oss.str();
#endif
    }

    auto TypeConverter::uint8ToString(uint8_t v) -> std::string
    {
        return std::to_string(static_cast<unsigned int>(v));
    }

    auto TypeConverter::uint16ToString(uint16_t v) -> std::string
    {
        return std::to_string(v);
    }

    auto TypeConverter::uint32ToString(uint32_t v) -> std::string
    {
        return std::to_string(v);
    }

    auto TypeConverter::uint64ToString(uint64_t v) -> std::string
    {
        return std::to_string(v);
    }

    auto TypeConverter::float32ToString(float v) -> std::string
    {
        std::ostringstream oss;
        oss << std::setprecision(7) << v;
        return oss.str();
    }

    auto TypeConverter::float64ToString(double v) -> std::string
    {
        std::ostringstream oss;
        oss << std::setprecision(15) << v;
        return oss.str();
    }

    auto TypeConverter::moneyToString(int64_t cents) -> std::string
    {
        // Format as currency: $123.45 (assuming 2 decimal places for cents)
        bool negative = cents < 0;
        int64_t abs_cents = negative ? -cents : cents;

        int64_t dollars = abs_cents / 100;
        int64_t remaining_cents = abs_cents % 100;

        std::ostringstream oss;
        if (negative) {
            oss << "-";
        }
        oss << "$" << dollars << "." << std::setfill('0') << std::setw(2) << remaining_cents;
        return oss.str();
    }

    auto TypeConverter::booleanToString(bool v) -> std::string
    {
        return v ? "true" : "false";
    }

    auto TypeConverter::dateToString(int64_t days) -> std::string
    {
        // Convert days since epoch to YYYY-MM-DD
        // Using Unix epoch (1970-01-01) as reference
        int64_t z = days + 719468; // Days from 0000-03-01
        int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        int64_t doe = z - era * 146097;
        int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int64_t y = yoe + era * 400;
        int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        int64_t mp = (5 * doy + 2) / 153;
        int64_t d = doy - (153 * mp + 2) / 5 + 1;
        int64_t m = mp + (mp < 10 ? 3 : -9);
        y += (m <= 2);

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(4) << y << '-' << std::setw(2) << m << '-'
            << std::setw(2) << d;
        return oss.str();
    }

    auto TypeConverter::timeToString(int64_t microseconds) -> std::string
    {
        int64_t seconds = microseconds / 1000000;
        int64_t us = microseconds % 1000000;
        int64_t hours = seconds / 3600;
        int64_t minutes = (seconds % 3600) / 60;
        int64_t secs = seconds % 60;

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << hours << ':' << std::setw(2) << minutes << ':'
            << std::setw(2) << secs;
        if (us > 0)
        {
            oss << '.' << std::setw(6) << us;
        }
        return oss.str();
    }

    auto TypeConverter::timestampToString(int64_t microseconds) -> std::string
    {
        // Input is always in GMT (microseconds since epoch)
        // Output in UTC with timezone offset for backward compatibility
        // Use TimezoneManager for proper formatting
        return g_tz_manager.formatTimestamp(microseconds, TimezoneManager::TZ_UTC, true);
    }

    auto TypeConverter::intervalToString(const Interval &interval) -> std::string
    {
        // Format as PostgreSQL-style interval: "X years Y mons Z days HH:MM:SS.microseconds"
        std::ostringstream oss;
        bool has_output = false;

        // Years and months
        if (interval.months != 0) {
            int32_t years = interval.months / 12;
            int32_t months = interval.months % 12;

            if (years != 0) {
                oss << years << (years == 1 ? " year" : " years");
                has_output = true;
            }
            if (months != 0) {
                if (has_output) oss << " ";
                oss << months << (months == 1 ? " mon" : " mons");
                has_output = true;
            }
        }

        // Days
        if (interval.days != 0) {
            if (has_output) oss << " ";
            oss << interval.days << (interval.days == 1 ? " day" : " days");
            has_output = true;
        }

        // Time component
        if (interval.microseconds != 0 || !has_output) {
            int64_t total_seconds = interval.microseconds / 1000000;
            int64_t us = interval.microseconds % 1000000;

            // Handle negative time
            bool negative = total_seconds < 0 || (total_seconds == 0 && us < 0);
            if (negative) {
                total_seconds = -total_seconds;
                us = -us;
            }

            int64_t hours = total_seconds / 3600;
            int64_t minutes = (total_seconds % 3600) / 60;
            int64_t seconds = total_seconds % 60;

            if (has_output) oss << " ";
            if (negative) oss << "-";
            oss << std::setfill('0') << std::setw(2) << hours << ":"
                << std::setw(2) << minutes << ":"
                << std::setw(2) << seconds;

            if (us != 0) {
                oss << "." << std::setw(6) << (us < 0 ? -us : us);
            }
        }

        return oss.str();
    }

    auto TypeConverter::uuidToString(const std::vector<uint8_t> &uuid) -> std::string
    {
        if (uuid.size() != 16)
            return "<invalid-uuid>";

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < 16; i++)
        {
            if (i == 4 || i == 6 || i == 8 || i == 10)
                oss << '-';
            oss << std::setw(2) << static_cast<int>(uuid[i]);
        }
        return oss.str();
    }

    auto TypeConverter::binaryToHex(const std::vector<uint8_t> &data) -> std::string
    {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::setfill('0');
        for (uint8_t byte : data)
        {
            oss << std::setw(2) << static_cast<int>(byte);
        }
        return oss.str();
    }

    auto TypeConverter::binaryToBase64(const std::vector<uint8_t> &data) -> std::string
    {
        static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "0123456789+/";

        std::string result;
        int val = 0;
        int valb = -6;
        for (uint8_t c : data)
        {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0)
            {
                result.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6)
            result.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        while (result.size() % 4)
            result.push_back('=');
        return result;
    }

    // Numeric conversions with overflow detection
    auto TypeConverter::int8ToInt16(int8_t v) -> int16_t
    {
        return static_cast<int16_t>(v);
    }

    auto TypeConverter::int8ToInt32(int8_t v) -> int32_t
    {
        return static_cast<int32_t>(v);
    }

    auto TypeConverter::int8ToInt64(int8_t v) -> int64_t
    {
        return static_cast<int64_t>(v);
    }

    auto TypeConverter::int16ToInt8(int16_t v, ErrorContext *ctx) -> std::optional<int8_t>
    {
        if (v < -128 || v > 127)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT16 to INT8");
            return std::nullopt;
        }
        return static_cast<int8_t>(v);
    }

    auto TypeConverter::int16ToInt32(int16_t v) -> int32_t
    {
        return static_cast<int32_t>(v);
    }

    auto TypeConverter::int16ToInt64(int16_t v) -> int64_t
    {
        return static_cast<int64_t>(v);
    }

    auto TypeConverter::int32ToInt8(int32_t v, ErrorContext *ctx) -> std::optional<int8_t>
    {
        if (v < -128 || v > 127)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT32 to INT8");
            return std::nullopt;
        }
        return static_cast<int8_t>(v);
    }

    auto TypeConverter::int32ToInt16(int32_t v, ErrorContext *ctx) -> std::optional<int16_t>
    {
        if (v < -32768 || v > 32767)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT32 to INT16");
            return std::nullopt;
        }
        return static_cast<int16_t>(v);
    }

    auto TypeConverter::int32ToInt64(int32_t v) -> int64_t
    {
        return static_cast<int64_t>(v);
    }

    auto TypeConverter::int64ToInt8(int64_t v, ErrorContext *ctx) -> std::optional<int8_t>
    {
        if (v < -128 || v > 127)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT64 to INT8");
            return std::nullopt;
        }
        return static_cast<int8_t>(v);
    }

    auto TypeConverter::int64ToInt16(int64_t v, ErrorContext *ctx) -> std::optional<int16_t>
    {
        if (v < -32768 || v > 32767)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT64 to INT16");
            return std::nullopt;
        }
        return static_cast<int16_t>(v);
    }

    auto TypeConverter::int64ToInt32(int64_t v, ErrorContext *ctx) -> std::optional<int32_t>
    {
        if (v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT64 to INT32");
            return std::nullopt;
        }
        return static_cast<int32_t>(v);
    }

    // ===== TypeExtractor Implementation =====

    auto TypeExtractor::extractYear(int64_t days_since_epoch) -> int32_t
    {
        int64_t z = days_since_epoch + 719468;
        int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        int64_t yoe = (z - era * 146097 - (z - era * 146097) / 1460 + (z - era * 146097) / 36524 -
                       (z - era * 146097) / 146096) /
                      365;
        int64_t y = yoe + era * 400;
        int64_t doy = z - era * 146097 - (365 * yoe + yoe / 4 - yoe / 100);
        int64_t mp = (5 * doy + 2) / 153;
        int64_t m = mp + (mp < 10 ? 3 : -9);
        return static_cast<int32_t>(y + (m <= 2));
    }

    auto TypeExtractor::extractMonth(int64_t days_since_epoch) -> int32_t
    {
        int64_t z = days_since_epoch + 719468;
        int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        int64_t doe = z - era * 146097;
        int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        int64_t mp = (5 * doy + 2) / 153;
        return static_cast<int32_t>(mp + (mp < 10 ? 3 : -9));
    }

    auto TypeExtractor::extractDay(int64_t days_since_epoch) -> int32_t
    {
        int64_t z = days_since_epoch + 719468;
        int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        int64_t doe = z - era * 146097;
        int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        int64_t mp = (5 * doy + 2) / 153;
        return static_cast<int32_t>(doy - (153 * mp + 2) / 5 + 1);
    }

    auto TypeExtractor::extractHour(int64_t microseconds) -> int32_t
    {
        return static_cast<int32_t>((microseconds / 1000000) / 3600);
    }

    auto TypeExtractor::extractMinute(int64_t microseconds) -> int32_t
    {
        return static_cast<int32_t>(((microseconds / 1000000) % 3600) / 60);
    }

    auto TypeExtractor::extractSecond(int64_t microseconds) -> int32_t
    {
        return static_cast<int32_t>((microseconds / 1000000) % 60);
    }

    auto TypeExtractor::extractMicrosecond(int64_t microseconds) -> int32_t
    {
        return static_cast<int32_t>(microseconds % 1000000);
    }

    auto TypeExtractor::extractUUIDVersion(const std::vector<uint8_t> &uuid) -> int32_t
    {
        if (uuid.size() != 16)
            return -1;
        return (uuid[6] >> 4) & 0x0F;
    }

    auto TypeExtractor::extractUUIDVariant(const std::vector<uint8_t> &uuid) -> int32_t
    {
        if (uuid.size() != 16)
            return -1;
        return (uuid[8] >> 6) & 0x03;
    }

    auto TypeExtractor::extractUUIDTimestamp(const std::vector<uint8_t> &uuid,
                                             ErrorContext *ctx) -> std::optional<int64_t>
    {
        if (uuid.size() != 16)
            return std::nullopt;

        int version = extractUUIDVersion(uuid);
        if (version != 1)
            return std::nullopt;  // Only UUID v1 has timestamp

        // Extract timestamp from UUID v1 (60-bit timestamp in 100-nanosecond intervals since Oct 15, 1582)
        uint64_t time_low = (static_cast<uint64_t>(uuid[0]) << 24) |
                            (static_cast<uint64_t>(uuid[1]) << 16) |
                            (static_cast<uint64_t>(uuid[2]) << 8) |
                            static_cast<uint64_t>(uuid[3]);
        uint64_t time_mid = (static_cast<uint64_t>(uuid[4]) << 8) |
                            static_cast<uint64_t>(uuid[5]);
        uint64_t time_hi = (static_cast<uint64_t>(uuid[6] & 0x0F) << 8) |
                           static_cast<uint64_t>(uuid[7]);

        uint64_t uuid_timestamp = (time_hi << 48) | (time_mid << 32) | time_low;

        // Convert from 100-nanosecond intervals to microseconds
        // UUID epoch is Oct 15, 1582; Unix epoch is Jan 1, 1970
        // Difference: 12219292800 seconds = 122192928000000000 * 100ns
        const uint64_t UUID_TO_UNIX_OFFSET = 122192928000000000ULL;
        if (uuid_timestamp < UUID_TO_UNIX_OFFSET)
            return std::nullopt;

        uint64_t unix_100ns = uuid_timestamp - UUID_TO_UNIX_OFFSET;
        return static_cast<int64_t>(unix_100ns / 10);  // Convert to microseconds
    }

    auto TypeExtractor::extractDayOfWeek(int64_t days_since_epoch) -> int32_t
    {
        // Jan 1, 1970 was a Thursday (4)
        // Calculate day of week: 0=Sunday, 1=Monday, ..., 6=Saturday
        int64_t dow = (days_since_epoch + 4) % 7;
        if (dow < 0)
            dow += 7;
        return static_cast<int32_t>(dow);
    }

    auto TypeExtractor::extractDayOfYear(int64_t days_since_epoch) -> int32_t
    {
        // Convert to year/month/day, then calculate day of year
        int32_t year = extractYear(days_since_epoch);
        int32_t month = extractMonth(days_since_epoch);
        int32_t day = extractDay(days_since_epoch);

        // Days in each month
        static const int days_before_month[13] = {
            0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
        };

        int32_t doy = days_before_month[month] + day;

        // Add 1 for leap year if after February
        if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
            doy++;
        }

        return doy;
    }

    auto TypeExtractor::extractTimestampYear(int64_t microseconds_since_epoch) -> int32_t
    {
        int64_t days = microseconds_since_epoch / 86400000000LL;
        return extractYear(days);
    }

    auto TypeExtractor::extractTimestampMonth(int64_t microseconds_since_epoch) -> int32_t
    {
        int64_t days = microseconds_since_epoch / 86400000000LL;
        return extractMonth(days);
    }

    auto TypeExtractor::extractTimestampDay(int64_t microseconds_since_epoch) -> int32_t
    {
        int64_t days = microseconds_since_epoch / 86400000000LL;
        return extractDay(days);
    }

    auto TypeExtractor::extractTimestampHour(int64_t microseconds_since_epoch) -> int32_t
    {
        int64_t time_us = microseconds_since_epoch % 86400000000LL;
        if (time_us < 0)
            time_us += 86400000000LL;
        return extractHour(time_us);
    }

    auto TypeExtractor::extractTimestampMinute(int64_t microseconds_since_epoch) -> int32_t
    {
        int64_t time_us = microseconds_since_epoch % 86400000000LL;
        if (time_us < 0)
            time_us += 86400000000LL;
        return extractMinute(time_us);
    }

    auto TypeExtractor::extractTimestampSecond(int64_t microseconds_since_epoch) -> int32_t
    {
        int64_t time_us = microseconds_since_epoch % 86400000000LL;
        if (time_us < 0)
            time_us += 86400000000LL;
        return extractSecond(time_us);
    }

    auto TypeExtractor::extractTimestampMicrosecond(int64_t microseconds_since_epoch) -> int32_t
    {
        int64_t time_us = microseconds_since_epoch % 86400000000LL;
        if (time_us < 0)
            time_us += 86400000000LL;
        return extractMicrosecond(time_us);
    }

    // ===== Convenience Conversion Methods =====

    int64_t TypedValue::toInt64() const
    {
        switch (type_)
        {
            case DataType::INT8:
                return getInt8();
            case DataType::INT16:
                return getInt16();
            case DataType::INT32:
                return getInt32();
            case DataType::INT64:
                return getInt64();
            case DataType::FLOAT32:
                return static_cast<int64_t>(getFloat32());
            case DataType::FLOAT64:
                return static_cast<int64_t>(getFloat64());
            case DataType::BOOLEAN:
                return getBoolean() ? 1 : 0;
            default:
                throw std::runtime_error("Cannot convert " + TypeSystem::getTypeName(type_) +
                                         " to int64");
        }
    }

    double TypedValue::toDouble() const
    {
        switch (type_)
        {
            case DataType::INT8:
                return static_cast<double>(getInt8());
            case DataType::INT16:
                return static_cast<double>(getInt16());
            case DataType::INT32:
                return static_cast<double>(getInt32());
            case DataType::INT64:
                return static_cast<double>(getInt64());
            case DataType::FLOAT32:
                return static_cast<double>(getFloat32());
            case DataType::FLOAT64:
                return getFloat64();
            case DataType::BOOLEAN:
                return getBoolean() ? 1.0 : 0.0;
            default:
                throw std::runtime_error("Cannot convert " + TypeSystem::getTypeName(type_) +
                                         " to double");
        }
    }

    bool TypedValue::toBoolean() const
    {
        switch (type_)
        {
            case DataType::BOOLEAN:
                return getBoolean();
            case DataType::INT8:
                return getInt8() != 0;
            case DataType::INT16:
                return getInt16() != 0;
            case DataType::INT32:
                return getInt32() != 0;
            case DataType::INT64:
                return getInt64() != 0;
            case DataType::FLOAT32:
                return getFloat32() != 0.0f;
            case DataType::FLOAT64:
                return getFloat64() != 0.0;
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::CHAR:
                return !getVarchar().empty();
            default:
                throw std::runtime_error("Cannot convert " + TypeSystem::getTypeName(type_) +
                                         " to boolean");
        }
    }

} // namespace scratchbird::core
