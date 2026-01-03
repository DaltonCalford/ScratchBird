#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/data_encryption.h"
#include "scratchbird/core/network.h"
#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
#include <cstring>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace scratchbird::core
{
    namespace
    {
        void appendUint8(std::vector<uint8_t> &out, uint8_t value)
        {
            out.push_back(value);
        }

        void appendUint16(std::vector<uint8_t> &out, uint16_t value)
        {
            out.push_back(static_cast<uint8_t>(value & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        }

        void appendUint32(std::vector<uint8_t> &out, uint32_t value)
        {
            out.push_back(static_cast<uint8_t>(value & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        }

        void appendUint64(std::vector<uint8_t> &out, uint64_t value)
        {
            for (int i = 0; i < 8; ++i)
            {
                out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
            }
        }

        void appendInt32(std::vector<uint8_t> &out, int32_t value)
        {
            appendUint32(out, static_cast<uint32_t>(value));
        }

        void appendInt64(std::vector<uint8_t> &out, int64_t value)
        {
            appendUint64(out, static_cast<uint64_t>(value));
        }

        void appendFloat(std::vector<uint8_t> &out, float value)
        {
            uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            appendUint32(out, bits);
        }

        void appendDouble(std::vector<uint8_t> &out, double value)
        {
            uint64_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            appendUint64(out, bits);
        }

        bool readUint8(const std::vector<uint8_t> &data, size_t &offset, uint8_t &value)
        {
            if (offset + 1 > data.size())
            {
                return false;
            }
            value = data[offset];
            offset += 1;
            return true;
        }

        bool readUint16(const std::vector<uint8_t> &data, size_t &offset, uint16_t &value)
        {
            if (offset + 2 > data.size())
            {
                return false;
            }
            value = static_cast<uint16_t>(data[offset]) |
                    static_cast<uint16_t>(data[offset + 1] << 8);
            offset += 2;
            return true;
        }

        bool readUint32(const std::vector<uint8_t> &data, size_t &offset, uint32_t &value)
        {
            if (offset + 4 > data.size())
            {
                return false;
            }
            value = static_cast<uint32_t>(data[offset]) |
                    (static_cast<uint32_t>(data[offset + 1]) << 8) |
                    (static_cast<uint32_t>(data[offset + 2]) << 16) |
                    (static_cast<uint32_t>(data[offset + 3]) << 24);
            offset += 4;
            return true;
        }

        bool readUint64(const std::vector<uint8_t> &data, size_t &offset, uint64_t &value)
        {
            if (offset + 8 > data.size())
            {
                return false;
            }
            value = 0;
            for (int i = 0; i < 8; ++i)
            {
                value |= (static_cast<uint64_t>(data[offset + i]) << (i * 8));
            }
            offset += 8;
            return true;
        }

        bool readInt32(const std::vector<uint8_t> &data, size_t &offset, int32_t &value)
        {
            uint32_t tmp = 0;
            if (!readUint32(data, offset, tmp))
            {
                return false;
            }
            value = static_cast<int32_t>(tmp);
            return true;
        }

        bool readInt64(const std::vector<uint8_t> &data, size_t &offset, int64_t &value)
        {
            uint64_t tmp = 0;
            if (!readUint64(data, offset, tmp))
            {
                return false;
            }
            value = static_cast<int64_t>(tmp);
            return true;
        }

        bool readFloat(const std::vector<uint8_t> &data, size_t &offset, float &value)
        {
            uint32_t bits = 0;
            if (!readUint32(data, offset, bits))
            {
                return false;
            }
            std::memcpy(&value, &bits, sizeof(value));
            return true;
        }

        bool readDouble(const std::vector<uint8_t> &data, size_t &offset, double &value)
        {
            uint64_t bits = 0;
            if (!readUint64(data, offset, bits))
            {
                return false;
            }
            std::memcpy(&value, &bits, sizeof(value));
            return true;
        }

        bool readBytes(const std::vector<uint8_t> &data, size_t &offset, size_t length,
                       std::vector<uint8_t> &out)
        {
            if (offset + length > data.size())
            {
                return false;
            }
            out.assign(data.begin() + offset, data.begin() + offset + length);
            offset += length;
            return true;
        }

        void appendPoint(std::vector<uint8_t> &out, const Point &point)
        {
            appendInt32(out, point.srid);
            appendDouble(out, point.x);
            appendDouble(out, point.y);
        }

        bool readPoint(const std::vector<uint8_t> &data, size_t &offset, Point &point)
        {
            int32_t srid = 0;
            double x = 0.0;
            double y = 0.0;
            if (!readInt32(data, offset, srid) ||
                !readDouble(data, offset, x) ||
                !readDouble(data, offset, y))
            {
                return false;
            }
            point = Point(x, y, srid);
            return true;
        }

        void appendPointList(std::vector<uint8_t> &out, const std::vector<Point> &points)
        {
            appendUint32(out, static_cast<uint32_t>(points.size()));
            for (const auto &pt : points)
            {
                appendPoint(out, pt);
            }
        }

        bool readPointList(const std::vector<uint8_t> &data, size_t &offset,
                           std::vector<Point> &points_out)
        {
            uint32_t count = 0;
            if (!readUint32(data, offset, count))
            {
                return false;
            }
            points_out.clear();
            points_out.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                Point pt;
                if (!readPoint(data, offset, pt))
                {
                    return false;
                }
                points_out.push_back(std::move(pt));
            }
            return true;
        }

        Status serializeEncryptedRecord(const EncryptedValue &encrypted,
                                        uint32_t key_version,
                                        std::vector<uint8_t> &record_out,
                                        ErrorContext *ctx)
        {
            if (encrypted.iv.size() > std::numeric_limits<uint16_t>::max() ||
                encrypted.auth_tag.size() > std::numeric_limits<uint16_t>::max() ||
                encrypted.ciphertext.size() > std::numeric_limits<uint32_t>::max())
            {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Encrypted value too large");
                return Status::OUT_OF_RANGE;
            }

            record_out.clear();
            record_out.reserve(1 + 4 + 2 + 2 + 4 +
                               encrypted.iv.size() +
                               encrypted.auth_tag.size() +
                               encrypted.ciphertext.size());

            appendUint8(record_out, static_cast<uint8_t>(encrypted.algorithm));
            appendUint32(record_out, key_version);
            appendUint16(record_out, static_cast<uint16_t>(encrypted.iv.size()));
            appendUint16(record_out, static_cast<uint16_t>(encrypted.auth_tag.size()));
            appendUint32(record_out, static_cast<uint32_t>(encrypted.ciphertext.size()));

            record_out.insert(record_out.end(), encrypted.iv.begin(), encrypted.iv.end());
            record_out.insert(record_out.end(), encrypted.auth_tag.begin(), encrypted.auth_tag.end());
            record_out.insert(record_out.end(), encrypted.ciphertext.begin(), encrypted.ciphertext.end());

            return Status::OK;
        }

        Status parseEncryptedRecord(const std::vector<uint8_t> &record,
                                    EncryptedValue &encrypted_out,
                                    uint32_t &key_version_out,
                                    ErrorContext *ctx)
        {
            size_t offset = 0;
            uint8_t algo = 0;
            uint16_t iv_len = 0;
            uint16_t tag_len = 0;
            uint32_t ciphertext_len = 0;

            if (!readUint8(record, offset, algo) ||
                !readUint32(record, offset, key_version_out) ||
                !readUint16(record, offset, iv_len) ||
                !readUint16(record, offset, tag_len) ||
                !readUint32(record, offset, ciphertext_len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid encrypted value record");
                return Status::DATA_CORRUPTED;
            }

            size_t expected_size = offset +
                                   static_cast<size_t>(iv_len) +
                                   static_cast<size_t>(tag_len) +
                                   static_cast<size_t>(ciphertext_len);
            if (expected_size != record.size())
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Encrypted value size mismatch");
                return Status::DATA_CORRUPTED;
            }

            encrypted_out.algorithm = static_cast<EncryptionAlgorithm>(algo);
            encrypted_out.iv.assign(record.begin() + offset,
                                    record.begin() + offset + iv_len);
            offset += iv_len;
            encrypted_out.auth_tag.assign(record.begin() + offset,
                                          record.begin() + offset + tag_len);
            offset += tag_len;
            encrypted_out.ciphertext.assign(record.begin() + offset,
                                            record.begin() + offset + ciphertext_len);
            encrypted_out.key_version = key_version_out;

            return Status::OK;
        }
    } // namespace

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

    TypedValue TypedValue::makeJSON(const std::string& value)
    {
        TypedValue tv(DataType::JSON);
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();
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
        ensureDecrypted();

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
            case DataType::JSON:
            case DataType::JSONB:
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
            case DataType::DATERANGE:
            case DataType::TSRANGE:
            case DataType::TSTZRANGE:
                if (complex_data_ && complex_data_->range_data) {
                    auto* range = static_cast<Range<int64_t>*>(complex_data_->range_data.get());
                    return range->toString();
                }
                return "<empty range>";
            case DataType::INT4RANGE:
                if (complex_data_ && complex_data_->range_data) {
                    auto* range = static_cast<Range<int32_t>*>(complex_data_->range_data.get());
                    return range->toString();
                }
                return "<empty range>";
            case DataType::INT8RANGE:
            case DataType::NUMRANGE:
                if (complex_data_ && complex_data_->range_data) {
                    auto* range = static_cast<Range<int64_t>*>(complex_data_->range_data.get());
                    return range->toString();
                }
                return "<empty range>";
            case DataType::TSVECTOR:
                if (complex_data_ && complex_data_->tsvector) {
                    return complex_data_->tsvector->toString();
                }
                return "<empty tsvector>";
            case DataType::TSQUERY:
                if (complex_data_ && complex_data_->tsquery) {
                    return complex_data_->tsquery->toString();
                }
                return "<empty tsquery>";
            default:
                return "<" + std::string(TypeSystem::getTypeName(type_)) + ">";
        }
    }

    Status TypedValue::encrypt(const std::vector<uint8_t>& key,
                               EncryptionAlgorithm algo,
                               uint32_t key_version,
                               ErrorContext* ctx)
    {
        if (is_null_) {
            return Status::OK;
        }

        if (is_encrypted_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Value is already encrypted");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<uint8_t> plaintext;
        Status status = serializePlainValue(plaintext, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        EncryptedValue encrypted;
        status = DataEncryption::encrypt(plaintext, key, algo, encrypted, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> record;
        status = serializeEncryptedRecord(encrypted, key_version, record, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Clear plaintext payload
        string_data_.clear();
        binary_data_.clear();
        spatial_data_.reset();
        complex_data_.reset();
        std::memset(&data_, 0, sizeof(data_));

        encrypted_data_ = std::move(record);
        encryption_algorithm_ = algo;
        encryption_key_version_ = key_version;
        is_encrypted_ = true;
        is_null_ = false;

        return Status::OK;
    }

    Status TypedValue::decrypt(const std::vector<uint8_t>& key, ErrorContext* ctx)
    {
        if (!is_encrypted_) {
            return Status::OK;
        }
        if (encrypted_data_.empty()) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Encrypted payload missing");
            return Status::INVALID_ARGUMENT;
        }

        EncryptedValue encrypted;
        uint32_t key_version = 0;
        Status status = parseEncryptedRecord(encrypted_data_, encrypted, key_version, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> plaintext;
        status = DataEncryption::decrypt(encrypted, key, plaintext, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        status = deserializePlainValue(plaintext, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        encrypted_data_.clear();
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encryption_key_version_ = 0;
        is_encrypted_ = false;
        is_null_ = false;

        return Status::OK;
    }

    Status TypedValue::setEncryptedData(const std::vector<uint8_t>& record, ErrorContext* ctx)
    {
        EncryptedValue encrypted;
        uint32_t key_version = 0;
        Status status = parseEncryptedRecord(record, encrypted, key_version, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Clear plaintext payload
        string_data_.clear();
        binary_data_.clear();
        spatial_data_.reset();
        complex_data_.reset();
        std::memset(&data_, 0, sizeof(data_));

        encrypted_data_ = record;
        encryption_algorithm_ = encrypted.algorithm;
        encryption_key_version_ = key_version;
        is_encrypted_ = true;
        is_null_ = false;

        return Status::OK;
    }

    // ===== Setters =====

    void TypedValue::setInt32(int32_t value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::INT32;
        is_null_ = false;
        data_.int32_val = value;
    }

    void TypedValue::setInt64(int64_t value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::INT64;
        is_null_ = false;
        data_.int64_val = value;
    }

    void TypedValue::setFloat32(float value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::FLOAT32;
        is_null_ = false;
        data_.float32_val = value;
    }

    void TypedValue::setFloat64(double value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::FLOAT64;
        is_null_ = false;
        data_.float64_val = value;
    }

    void TypedValue::setBool(bool value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::BOOLEAN;
        is_null_ = false;
        data_.bool_val = value;
    }

    void TypedValue::setVarchar(const std::string& value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::VARCHAR;
        is_null_ = false;
        string_data_ = value;
    }

    void TypedValue::setText(const std::string& value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::TEXT;
        is_null_ = false;
        string_data_ = value;
    }

    void TypedValue::setChar(const std::string& value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
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
        ensureDecrypted();
        other.ensureDecrypted();

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
        ensureDecrypted();
        other.ensureDecrypted();

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

    void TypedValue::ensureDecrypted() const
    {
        if (is_encrypted_)
        {
            throw std::runtime_error("Value is encrypted");
        }
    }

    Status TypedValue::serializePlainValue(std::vector<uint8_t>& out, ErrorContext* ctx) const
    {
        out.clear();

        if (is_null_) {
            return Status::OK;
        }
        if (is_encrypted_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Cannot serialize encrypted value");
            return Status::INVALID_ARGUMENT;
        }

        const auto max_u32 = std::numeric_limits<uint32_t>::max();

        auto appendLengthPrefixedString = [&](const std::string& value) -> Status
        {
            if (value.size() > max_u32)
            {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "String value too large to serialize");
                return Status::OUT_OF_RANGE;
            }
            appendUint32(out, static_cast<uint32_t>(value.size()));
            out.insert(out.end(), value.begin(), value.end());
            return Status::OK;
        };

        auto appendLengthPrefixedBinary = [&](const std::vector<uint8_t>& value) -> Status
        {
            if (value.size() > max_u32)
            {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Binary value too large to serialize");
                return Status::OUT_OF_RANGE;
            }
            appendUint32(out, static_cast<uint32_t>(value.size()));
            out.insert(out.end(), value.begin(), value.end());
            return Status::OK;
        };

        auto serializeValueList = [&](const std::vector<TypedValue>& values) -> Status
        {
            if (values.size() > max_u32)
            {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Too many elements to serialize");
                return Status::OUT_OF_RANGE;
            }
            appendUint32(out, static_cast<uint32_t>(values.size()));
            for (const auto& element : values)
            {
                appendUint8(out, element.isNull() ? 1 : 0);
                appendUint16(out, static_cast<uint16_t>(element.type()));

                std::vector<uint8_t> element_data;
                if (!element.isNull())
                {
                    Status status = element.serializePlainValue(element_data, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                }

                if (element_data.size() > max_u32)
                {
                    SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Element too large to serialize");
                    return Status::OUT_OF_RANGE;
                }
                appendUint32(out, static_cast<uint32_t>(element_data.size()));
                out.insert(out.end(), element_data.begin(), element_data.end());
            }
            return Status::OK;
        };

        switch (type_)
        {
            case DataType::INT8:
                appendUint8(out, static_cast<uint8_t>(data_.int8_val));
                break;
            case DataType::INT16:
                appendUint16(out, static_cast<uint16_t>(data_.int16_val));
                break;
            case DataType::INT32:
                appendInt32(out, data_.int32_val);
                break;
            case DataType::INT64:
                appendInt64(out, data_.int64_val);
                break;
            case DataType::UINT8:
                appendUint8(out, data_.uint8_val);
                break;
            case DataType::UINT16:
                appendUint16(out, data_.uint16_val);
                break;
            case DataType::UINT32:
                appendUint32(out, data_.uint32_val);
                break;
            case DataType::UINT64:
                appendUint64(out, data_.uint64_val);
                break;
            case DataType::FLOAT32:
                appendFloat(out, data_.float32_val);
                break;
            case DataType::FLOAT64:
                appendDouble(out, data_.float64_val);
                break;
            case DataType::BOOLEAN:
                appendUint8(out, data_.bool_val ? 1 : 0);
                break;
            case DataType::DATE:
            case DataType::TIME:
            case DataType::TIMESTAMP:
                appendInt64(out, data_.int64_val);
                break;
            case DataType::MONEY:
                appendInt64(out, data_.int64_val);
                break;
            case DataType::CHAR:
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::DECIMAL:
            case DataType::JSON:
            case DataType::JSONB:
            case DataType::XML:
            {
                Status status = appendLengthPrefixedString(string_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
            {
                Status status = appendLengthPrefixedBinary(binary_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::UUID:
            case DataType::INT128:
            {
                if (binary_data_.size() != 16)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Expected 16 bytes for UUID/INT128");
                    return Status::INVALID_ARGUMENT;
                }
                out.insert(out.end(), binary_data_.begin(), binary_data_.end());
                break;
            }
            case DataType::VECTOR:
            {
                Status status = appendLengthPrefixedBinary(binary_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::POINT:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                appendPoint(out, spatial_data_->point);
                break;
            }
            case DataType::LINESTRING:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& line = spatial_data_->linestring;
                appendInt32(out, line.srid);
                appendPointList(out, line.points);
                break;
            }
            case DataType::POLYGON:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& polygon = spatial_data_->polygon;
                appendInt32(out, polygon.srid);
                appendUint32(out, static_cast<uint32_t>(polygon.rings.size()));
                for (const auto& ring : polygon.rings)
                {
                    appendPointList(out, ring);
                }
                break;
            }
            case DataType::MULTIPOINT:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& multi = spatial_data_->multipoint;
                appendInt32(out, multi.srid);
                appendPointList(out, multi.points);
                break;
            }
            case DataType::MULTILINESTRING:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& multi = spatial_data_->multilinestring;
                appendInt32(out, multi.srid);
                appendUint32(out, static_cast<uint32_t>(multi.linestrings.size()));
                for (const auto& line : multi.linestrings)
                {
                    appendInt32(out, line.srid);
                    appendPointList(out, line.points);
                }
                break;
            }
            case DataType::MULTIPOLYGON:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& multi = spatial_data_->multipolygon;
                appendInt32(out, multi.srid);
                appendUint32(out, static_cast<uint32_t>(multi.polygons.size()));
                for (const auto& polygon : multi.polygons)
                {
                    appendInt32(out, polygon.srid);
                    appendUint32(out, static_cast<uint32_t>(polygon.rings.size()));
                    for (const auto& ring : polygon.rings)
                    {
                        appendPointList(out, ring);
                    }
                }
                break;
            }
            case DataType::GEOMETRYCOLLECTION:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& collection = spatial_data_->geometrycollection;
                appendInt32(out, collection.srid);
                if (collection.geometries.size() > max_u32)
                {
                    SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Geometry collection too large to serialize");
                    return Status::OUT_OF_RANGE;
                }
                appendUint32(out, static_cast<uint32_t>(collection.geometries.size()));
                for (const auto& geom : collection.geometries)
                {
                    appendUint8(out, geom ? 0 : 1);
                    if (!geom)
                    {
                        appendUint16(out, 0);
                        appendUint32(out, 0);
                        continue;
                    }
                    appendUint16(out, static_cast<uint16_t>(geom->type()));
                    std::vector<uint8_t> geom_data;
                    Status status = geom->serializePlainValue(geom_data, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                    if (geom_data.size() > max_u32)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Geometry too large to serialize");
                        return Status::OUT_OF_RANGE;
                    }
                    appendUint32(out, static_cast<uint32_t>(geom_data.size()));
                    out.insert(out.end(), geom_data.begin(), geom_data.end());
                }
                break;
            }
            case DataType::INET:
            {
                if (!complex_data_ || !complex_data_->inet)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "INET data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& inet = *complex_data_->inet;
                appendUint8(out, static_cast<uint8_t>(inet.family()));
                appendUint8(out, inet.netmask());
                out.insert(out.end(), inet.data(), inet.data() + inet.size());
                break;
            }
            case DataType::CIDR:
            {
                if (!complex_data_ || !complex_data_->cidr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CIDR data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto inet = complex_data_->cidr->toInet();
                appendUint8(out, static_cast<uint8_t>(inet.family()));
                appendUint8(out, inet.netmask());
                out.insert(out.end(), inet.data(), inet.data() + inet.size());
                break;
            }
            case DataType::MACADDR:
            {
                if (!complex_data_ || !complex_data_->macaddr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "MACADDR data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& bytes = complex_data_->macaddr->bytes();
                out.insert(out.end(), bytes.begin(), bytes.end());
                break;
            }
            case DataType::MACADDR8:
            {
                if (!complex_data_ || !complex_data_->macaddr8)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "MACADDR8 data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& bytes = complex_data_->macaddr8->bytes();
                out.insert(out.end(), bytes.begin(), bytes.end());
                break;
            }
            case DataType::INTERVAL:
            {
                if (!complex_data_ || !complex_data_->interval)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Interval data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& interval = *complex_data_->interval;
                appendInt32(out, interval.months);
                appendInt32(out, interval.days);
                appendInt64(out, interval.microseconds);
                break;
            }
            case DataType::TSVECTOR:
            {
                if (!complex_data_ || !complex_data_->tsvector)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "TSVector data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                out = complex_data_->tsvector->toBinary();
                break;
            }
            case DataType::TSQUERY:
            {
                if (!complex_data_ || !complex_data_->tsquery)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "TSQuery data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                out = complex_data_->tsquery->toBinary();
                break;
            }
            case DataType::INT4RANGE:
            {
                if (!complex_data_ || !complex_data_->range_data)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Range data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                auto range = std::static_pointer_cast<Range<int32_t>>(complex_data_->range_data);
                uint8_t flags = 0;
                if (range->isEmpty())
                {
                    appendUint8(out, 0x01);
                    break;
                }
                if (range->isLowerBounded()) flags |= 0x02;
                if (range->isUpperBounded()) flags |= 0x04;
                if (range->lowerBoundType() == BoundType::INCLUSIVE) flags |= 0x08;
                if (range->upperBoundType() == BoundType::INCLUSIVE) flags |= 0x10;
                appendUint8(out, flags);
                if (flags & 0x02) appendInt32(out, *range->lower());
                if (flags & 0x04) appendInt32(out, *range->upper());
                break;
            }
            case DataType::INT8RANGE:
            case DataType::DATERANGE:
            case DataType::TSRANGE:
            case DataType::TSTZRANGE:
            {
                if (!complex_data_ || !complex_data_->range_data)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Range data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                auto range = std::static_pointer_cast<Range<int64_t>>(complex_data_->range_data);
                uint8_t flags = 0;
                if (range->isEmpty())
                {
                    appendUint8(out, 0x01);
                    break;
                }
                if (range->isLowerBounded()) flags |= 0x02;
                if (range->isUpperBounded()) flags |= 0x04;
                if (range->lowerBoundType() == BoundType::INCLUSIVE) flags |= 0x08;
                if (range->upperBoundType() == BoundType::INCLUSIVE) flags |= 0x10;
                appendUint8(out, flags);
                if (flags & 0x02) appendInt64(out, *range->lower());
                if (flags & 0x04) appendInt64(out, *range->upper());
                break;
            }
            case DataType::NUMRANGE:
            {
                if (!complex_data_ || !complex_data_->range_data)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Range data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                auto range = std::static_pointer_cast<Range<double>>(complex_data_->range_data);
                uint8_t flags = 0;
                if (range->isEmpty())
                {
                    appendUint8(out, 0x01);
                    break;
                }
                if (range->isLowerBounded()) flags |= 0x02;
                if (range->isUpperBounded()) flags |= 0x04;
                if (range->lowerBoundType() == BoundType::INCLUSIVE) flags |= 0x08;
                if (range->upperBoundType() == BoundType::INCLUSIVE) flags |= 0x10;
                appendUint8(out, flags);
                if (flags & 0x02) appendDouble(out, *range->lower());
                if (flags & 0x04) appendDouble(out, *range->upper());
                break;
            }
            case DataType::ARRAY:
            {
                if (!complex_data_ || !complex_data_->array)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Array data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                Status status = serializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::COMPOSITE:
            {
                if (!complex_data_ || !complex_data_->array)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Composite data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                Status status = appendLengthPrefixedString(string_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                status = serializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::VARIANT:
            {
                if (!complex_data_ || !complex_data_->array)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Variant data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                Status status = serializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            default:
                SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Unsupported type for encryption serialization");
                return Status::NOT_SUPPORTED;
        }

        return Status::OK;
    }

    Status TypedValue::deserializePlainValue(const std::vector<uint8_t>& data, ErrorContext* ctx)
    {
        if (type_ == DataType::NULL_TYPE)
        {
            is_null_ = true;
            return Status::OK;
        }

        if (data.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Empty plaintext for non-null value");
            return Status::DATA_CORRUPTED;
        }

        string_data_.clear();
        binary_data_.clear();
        spatial_data_.reset();
        complex_data_.reset();
        std::memset(&data_, 0, sizeof(data_));

        size_t offset = 0;

        auto readLengthPrefixedString = [&](std::string& value_out) -> Status
        {
            uint32_t len = 0;
            if (!readUint32(data, offset, len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid length prefix for string");
                return Status::DATA_CORRUPTED;
            }
            if (offset + len > data.size())
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "String length exceeds payload");
                return Status::DATA_CORRUPTED;
            }
            value_out.assign(reinterpret_cast<const char*>(data.data() + offset), len);
            offset += len;
            return Status::OK;
        };

        auto readLengthPrefixedBinary = [&](std::vector<uint8_t>& value_out) -> Status
        {
            uint32_t len = 0;
            if (!readUint32(data, offset, len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid length prefix for binary");
                return Status::DATA_CORRUPTED;
            }
            if (!readBytes(data, offset, len, value_out))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Binary length exceeds payload");
                return Status::DATA_CORRUPTED;
            }
            return Status::OK;
        };

        auto deserializeValueList = [&](std::vector<TypedValue>& values_out) -> Status
        {
            uint32_t count = 0;
            if (!readUint32(data, offset, count))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid element count");
                return Status::DATA_CORRUPTED;
            }
            values_out.clear();
            values_out.reserve(count);

            for (uint32_t i = 0; i < count; ++i)
            {
                uint8_t is_null = 0;
                uint16_t type_code = 0;
                uint32_t len = 0;
                if (!readUint8(data, offset, is_null) ||
                    !readUint16(data, offset, type_code) ||
                    !readUint32(data, offset, len))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid element header");
                    return Status::DATA_CORRUPTED;
                }

                TypedValue element(static_cast<DataType>(type_code));

                if (is_null)
                {
                    if (len != 0)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Null element has payload");
                        return Status::DATA_CORRUPTED;
                    }
                    values_out.push_back(std::move(element));
                    continue;
                }

                std::vector<uint8_t> element_data;
                if (!readBytes(data, offset, len, element_data))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Element payload exceeds buffer");
                    return Status::DATA_CORRUPTED;
                }

                Status status = element.deserializePlainValue(element_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                values_out.push_back(std::move(element));
            }

            return Status::OK;
        };

        Status status = Status::OK;

        switch (type_)
        {
            case DataType::INT8:
            {
                uint8_t value = 0;
                if (!readUint8(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT8 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int8_val = static_cast<int8_t>(value);
                break;
            }
            case DataType::INT16:
            {
                uint16_t value = 0;
                if (!readUint16(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT16 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int16_val = static_cast<int16_t>(value);
                break;
            }
            case DataType::INT32:
            {
                int32_t value = 0;
                if (!readInt32(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT32 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int32_val = value;
                break;
            }
            case DataType::INT64:
            {
                int64_t value = 0;
                if (!readInt64(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT64 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int64_val = value;
                break;
            }
            case DataType::UINT8:
            {
                uint8_t value = 0;
                if (!readUint8(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid UINT8 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.uint8_val = value;
                break;
            }
            case DataType::UINT16:
            {
                uint16_t value = 0;
                if (!readUint16(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid UINT16 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.uint16_val = value;
                break;
            }
            case DataType::UINT32:
            {
                uint32_t value = 0;
                if (!readUint32(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid UINT32 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.uint32_val = value;
                break;
            }
            case DataType::UINT64:
            {
                uint64_t value = 0;
                if (!readUint64(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid UINT64 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.uint64_val = value;
                break;
            }
            case DataType::FLOAT32:
            {
                float value = 0.0f;
                if (!readFloat(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid FLOAT32 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.float32_val = value;
                break;
            }
            case DataType::FLOAT64:
            {
                double value = 0.0;
                if (!readDouble(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid FLOAT64 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.float64_val = value;
                break;
            }
            case DataType::BOOLEAN:
            {
                uint8_t value = 0;
                if (!readUint8(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid BOOLEAN payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.bool_val = (value != 0);
                break;
            }
            case DataType::DATE:
            case DataType::TIME:
            case DataType::TIMESTAMP:
            case DataType::MONEY:
            {
                int64_t value = 0;
                if (!readInt64(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid temporal payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int64_val = value;
                break;
            }
            case DataType::CHAR:
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::DECIMAL:
            case DataType::JSON:
            case DataType::JSONB:
            case DataType::XML:
            {
                status = readLengthPrefixedString(string_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
            {
                status = readLengthPrefixedBinary(binary_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::UUID:
            case DataType::INT128:
            {
                if (data.size() != 16)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid UUID/INT128 payload size");
                    return Status::DATA_CORRUPTED;
                }
                binary_data_.assign(data.begin(), data.end());
                offset = data.size();
                break;
            }
            case DataType::VECTOR:
            {
                status = readLengthPrefixedBinary(binary_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                if (binary_data_.size() % sizeof(float) != 0)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Vector payload length is not float-aligned");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::POINT:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                if (!readPoint(data, offset, spatial_data_->point))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid POINT payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::LINESTRING:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                if (!readInt32(data, offset, srid))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid LINESTRING payload");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->linestring.srid = srid;
                if (!readPointList(data, offset, spatial_data_->linestring.points))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid LINESTRING points");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::POLYGON:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                uint32_t ring_count = 0;
                if (!readInt32(data, offset, srid) || !readUint32(data, offset, ring_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid POLYGON header");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->polygon.srid = srid;
                spatial_data_->polygon.rings.clear();
                spatial_data_->polygon.rings.reserve(ring_count);
                for (uint32_t i = 0; i < ring_count; ++i)
                {
                    std::vector<Point> ring;
                    if (!readPointList(data, offset, ring))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid POLYGON ring");
                        return Status::DATA_CORRUPTED;
                    }
                    spatial_data_->polygon.rings.push_back(std::move(ring));
                }
                break;
            }
            case DataType::MULTIPOINT:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                if (!readInt32(data, offset, srid))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOINT header");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->multipoint.srid = srid;
                if (!readPointList(data, offset, spatial_data_->multipoint.points))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOINT points");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::MULTILINESTRING:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                uint32_t line_count = 0;
                if (!readInt32(data, offset, srid) || !readUint32(data, offset, line_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTILINESTRING header");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->multilinestring.srid = srid;
                spatial_data_->multilinestring.linestrings.clear();
                spatial_data_->multilinestring.linestrings.reserve(line_count);
                for (uint32_t i = 0; i < line_count; ++i)
                {
                    LineString line;
                    if (!readInt32(data, offset, line.srid) ||
                        !readPointList(data, offset, line.points))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTILINESTRING segment");
                        return Status::DATA_CORRUPTED;
                    }
                    spatial_data_->multilinestring.linestrings.push_back(std::move(line));
                }
                break;
            }
            case DataType::MULTIPOLYGON:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                uint32_t polygon_count = 0;
                if (!readInt32(data, offset, srid) || !readUint32(data, offset, polygon_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOLYGON header");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->multipolygon.srid = srid;
                spatial_data_->multipolygon.polygons.clear();
                spatial_data_->multipolygon.polygons.reserve(polygon_count);
                for (uint32_t i = 0; i < polygon_count; ++i)
                {
                    Polygon polygon;
                    uint32_t ring_count = 0;
                    if (!readInt32(data, offset, polygon.srid) ||
                        !readUint32(data, offset, ring_count))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOLYGON polygon header");
                        return Status::DATA_CORRUPTED;
                    }
                    polygon.rings.clear();
                    polygon.rings.reserve(ring_count);
                    for (uint32_t r = 0; r < ring_count; ++r)
                    {
                        std::vector<Point> ring;
                        if (!readPointList(data, offset, ring))
                        {
                            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOLYGON ring");
                            return Status::DATA_CORRUPTED;
                        }
                        polygon.rings.push_back(std::move(ring));
                    }
                    spatial_data_->multipolygon.polygons.push_back(std::move(polygon));
                }
                break;
            }
            case DataType::GEOMETRYCOLLECTION:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                uint32_t geom_count = 0;
                if (!readInt32(data, offset, srid) || !readUint32(data, offset, geom_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid GEOMETRYCOLLECTION header");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->geometrycollection.srid = srid;
                spatial_data_->geometrycollection.geometries.clear();
                spatial_data_->geometrycollection.geometries.reserve(geom_count);
                for (uint32_t i = 0; i < geom_count; ++i)
                {
                    uint8_t is_null = 0;
                    uint16_t type_code = 0;
                    uint32_t len = 0;
                    if (!readUint8(data, offset, is_null) ||
                        !readUint16(data, offset, type_code) ||
                        !readUint32(data, offset, len))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid GEOMETRYCOLLECTION entry");
                        return Status::DATA_CORRUPTED;
                    }
                    if (is_null)
                    {
                        if (len != 0)
                        {
                            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Null geometry has payload");
                            return Status::DATA_CORRUPTED;
                        }
                        spatial_data_->geometrycollection.geometries.push_back(nullptr);
                        continue;
                    }
                    std::vector<uint8_t> geom_data;
                    if (!readBytes(data, offset, len, geom_data))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Geometry payload exceeds buffer");
                        return Status::DATA_CORRUPTED;
                    }
                    auto geom = std::make_shared<TypedValue>(static_cast<DataType>(type_code));
                    status = geom->deserializePlainValue(geom_data, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                    spatial_data_->geometrycollection.geometries.push_back(std::move(geom));
                }
                break;
            }
            case DataType::INET:
            {
                uint8_t family = 0;
                uint8_t netmask = 0;
                if (!readUint8(data, offset, family) || !readUint8(data, offset, netmask))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INET header");
                    return Status::DATA_CORRUPTED;
                }
                if (family != static_cast<uint8_t>(AddressFamily::IPv4) &&
                    family != static_cast<uint8_t>(AddressFamily::IPv6))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INET address family");
                    return Status::DATA_CORRUPTED;
                }
                size_t addr_len = (family == static_cast<uint8_t>(AddressFamily::IPv4)) ? 4 : 16;
                std::vector<uint8_t> addr_bytes;
                if (!readBytes(data, offset, addr_len, addr_bytes))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INET address bytes");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->inet = std::make_unique<InetAddr>(
                    static_cast<AddressFamily>(family), addr_bytes.data(), netmask);
                break;
            }
            case DataType::CIDR:
            {
                uint8_t family = 0;
                uint8_t netmask = 0;
                if (!readUint8(data, offset, family) || !readUint8(data, offset, netmask))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid CIDR header");
                    return Status::DATA_CORRUPTED;
                }
                if (family != static_cast<uint8_t>(AddressFamily::IPv4) &&
                    family != static_cast<uint8_t>(AddressFamily::IPv6))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid CIDR address family");
                    return Status::DATA_CORRUPTED;
                }
                size_t addr_len = (family == static_cast<uint8_t>(AddressFamily::IPv4)) ? 4 : 16;
                std::vector<uint8_t> addr_bytes;
                if (!readBytes(data, offset, addr_len, addr_bytes))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid CIDR address bytes");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                InetAddr addr(static_cast<AddressFamily>(family), addr_bytes.data(), netmask);
                complex_data_->cidr = std::make_unique<Cidr>(addr);
                break;
            }
            case DataType::MACADDR:
            {
                std::vector<uint8_t> bytes;
                if (!readBytes(data, offset, 6, bytes))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MACADDR bytes");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->macaddr = std::make_unique<MacAddr>(MacAddr::fromBytes(bytes.data()));
                break;
            }
            case DataType::MACADDR8:
            {
                std::vector<uint8_t> bytes;
                if (!readBytes(data, offset, 8, bytes))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MACADDR8 bytes");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->macaddr8 = std::make_unique<MacAddr8>(MacAddr8::fromBytes(bytes.data()));
                break;
            }
            case DataType::INTERVAL:
            {
                int32_t months = 0;
                int32_t days = 0;
                int64_t micros = 0;
                if (!readInt32(data, offset, months) ||
                    !readInt32(data, offset, days) ||
                    !readInt64(data, offset, micros))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INTERVAL payload");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->interval = std::make_unique<Interval>(months, days, micros);
                break;
            }
            case DataType::TSVECTOR:
            {
                auto parsed = TSVector::fromBinary(data);
                if (!parsed)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TSVECTOR payload");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->tsvector = std::make_shared<TSVector>(*parsed);
                offset = data.size();
                break;
            }
            case DataType::TSQUERY:
            {
                auto parsed = TSQuery::fromBinary(data);
                if (!parsed)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TSQUERY payload");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->tsquery = std::make_shared<TSQuery>(std::move(*parsed));
                offset = data.size();
                break;
            }
            case DataType::INT4RANGE:
            {
                uint8_t flags = 0;
                if (!readUint8(data, offset, flags))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT4RANGE flags");
                    return Status::DATA_CORRUPTED;
                }
                if (flags & 0x01)
                {
                    complex_data_ = std::make_unique<ComplexData>();
                    complex_data_->range_data = std::make_shared<Range<int32_t>>();
                    break;
                }
                std::optional<int32_t> lower;
                std::optional<int32_t> upper;
                if (flags & 0x02)
                {
                    int32_t val = 0;
                    if (!readInt32(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT4RANGE lower bound");
                        return Status::DATA_CORRUPTED;
                    }
                    lower = val;
                }
                if (flags & 0x04)
                {
                    int32_t val = 0;
                    if (!readInt32(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT4RANGE upper bound");
                        return Status::DATA_CORRUPTED;
                    }
                    upper = val;
                }
                BoundType lower_type = (flags & 0x08) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                BoundType upper_type = (flags & 0x10) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->range_data = std::make_shared<Range<int32_t>>(lower, upper, lower_type, upper_type);
                break;
            }
            case DataType::INT8RANGE:
            case DataType::DATERANGE:
            case DataType::TSRANGE:
            case DataType::TSTZRANGE:
            {
                uint8_t flags = 0;
                if (!readUint8(data, offset, flags))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid range flags");
                    return Status::DATA_CORRUPTED;
                }
                if (flags & 0x01)
                {
                    complex_data_ = std::make_unique<ComplexData>();
                    complex_data_->range_data = std::make_shared<Range<int64_t>>();
                    break;
                }
                std::optional<int64_t> lower;
                std::optional<int64_t> upper;
                if (flags & 0x02)
                {
                    int64_t val = 0;
                    if (!readInt64(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid range lower bound");
                        return Status::DATA_CORRUPTED;
                    }
                    lower = val;
                }
                if (flags & 0x04)
                {
                    int64_t val = 0;
                    if (!readInt64(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid range upper bound");
                        return Status::DATA_CORRUPTED;
                    }
                    upper = val;
                }
                BoundType lower_type = (flags & 0x08) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                BoundType upper_type = (flags & 0x10) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->range_data = std::make_shared<Range<int64_t>>(lower, upper, lower_type, upper_type);
                break;
            }
            case DataType::NUMRANGE:
            {
                uint8_t flags = 0;
                if (!readUint8(data, offset, flags))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid NUMRANGE flags");
                    return Status::DATA_CORRUPTED;
                }
                if (flags & 0x01)
                {
                    complex_data_ = std::make_unique<ComplexData>();
                    complex_data_->range_data = std::make_shared<Range<double>>();
                    break;
                }
                std::optional<double> lower;
                std::optional<double> upper;
                if (flags & 0x02)
                {
                    double val = 0.0;
                    if (!readDouble(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid NUMRANGE lower bound");
                        return Status::DATA_CORRUPTED;
                    }
                    lower = val;
                }
                if (flags & 0x04)
                {
                    double val = 0.0;
                    if (!readDouble(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid NUMRANGE upper bound");
                        return Status::DATA_CORRUPTED;
                    }
                    upper = val;
                }
                BoundType lower_type = (flags & 0x08) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                BoundType upper_type = (flags & 0x10) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->range_data = std::make_shared<Range<double>>(lower, upper, lower_type, upper_type);
                break;
            }
            case DataType::ARRAY:
            {
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->array = std::make_unique<std::vector<TypedValue>>();
                status = deserializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::COMPOSITE:
            {
                status = readLengthPrefixedString(string_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->array = std::make_unique<std::vector<TypedValue>>();
                status = deserializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::VARIANT:
            {
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->array = std::make_unique<std::vector<TypedValue>>();
                status = deserializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            default:
                SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Unsupported type for encryption deserialization");
                return Status::NOT_SUPPORTED;
        }

        if (offset != data.size())
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Trailing bytes in plaintext payload");
            return Status::DATA_CORRUPTED;
        }

        is_null_ = false;
        return Status::OK;
    }

    void TypedValue::copyFrom(const TypedValue& other)
    {
        is_encrypted_ = other.is_encrypted_;
        encryption_key_version_ = other.encryption_key_version_;
        encryption_algorithm_ = other.encryption_algorithm_;
        encrypted_data_ = other.encrypted_data_;

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
        is_encrypted_ = other.is_encrypted_;
        encryption_key_version_ = other.encryption_key_version_;
        encryption_algorithm_ = other.encryption_algorithm_;
        encrypted_data_ = std::move(other.encrypted_data_);

        if (other.is_null_) {
            other.is_encrypted_ = false;
            other.encryption_key_version_ = 0;
            other.encryption_algorithm_ = EncryptionAlgorithm::NONE;
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
        other.is_encrypted_ = false;
        other.encryption_key_version_ = 0;
        other.encryption_algorithm_ = EncryptionAlgorithm::NONE;
    }

    void TypedValue::clear()
    {
        // Clear string data
        string_data_.clear();

        // Clear binary data
        binary_data_.clear();

        // Clear encrypted payload
        encrypted_data_.clear();
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;

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
        ensureDecrypted();

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
