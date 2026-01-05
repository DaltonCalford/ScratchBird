#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/network.h"
#include "scratchbird/core/range.h"
#include "scratchbird/core/tsquery.h"
#include "scratchbird/core/tsvector.h"

using namespace scratchbird::core;

namespace {

void expectPlainRoundTrip(const TypedValue& original)
{
    ErrorContext ctx;
    std::vector<uint8_t> encoded;
    ASSERT_EQ(original.serializePlainValue(encoded, &ctx), Status::OK) << ctx.message;

    TypedValue decoded(original.type());
    if (original.type() == DataType::DECIMAL)
    {
        decoded.setDecimalType(original.getDecimalPrecision(), original.getDecimalScale());
    }
    ASSERT_EQ(decoded.deserializePlainValue(encoded, &ctx), Status::OK) << ctx.message;

    std::vector<uint8_t> encoded_roundtrip;
    ASSERT_EQ(decoded.serializePlainValue(encoded_roundtrip, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(encoded, encoded_roundtrip);
}

TypedValue convertToType(const TypedValue& source,
                         const TypeInfo& target,
                         CastFormat format = CastFormat::DEFAULT)
{
    ErrorContext ctx;
    TypedValue result;
    Status status = source.convertTo(target, result, format, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
    if (status != Status::OK)
    {
        return TypedValue::makeNull(target.type);
    }
    return result;
}

}  // namespace

TEST(PlainValuePersistenceTest, RoundTrip_Primitives)
{
    auto int8_val = convertToType(TypedValue::makeInt32(127), TypeInfo(DataType::INT8));
    auto int16_val = convertToType(TypedValue::makeInt32(32000), TypeInfo(DataType::INT16));

    expectPlainRoundTrip(int8_val);
    expectPlainRoundTrip(int16_val);
    expectPlainRoundTrip(TypedValue::makeInt32(2000000));
    expectPlainRoundTrip(TypedValue::makeInt64(9000000000LL));
    expectPlainRoundTrip(TypedValue::makeUInt8(200));
    expectPlainRoundTrip(TypedValue::makeUInt16(65000));
    expectPlainRoundTrip(TypedValue::makeUInt32(4000000000U));
    expectPlainRoundTrip(TypedValue::makeUInt64(1800000000000000000ULL));
    expectPlainRoundTrip(TypedValue::makeFloat32(3.14f));
    expectPlainRoundTrip(TypedValue::makeFloat64(2.718281828));
    expectPlainRoundTrip(TypedValue::makeDecimal(123456, 10, 2));
    expectPlainRoundTrip(TypedValue::makeMoney(123456));
    expectPlainRoundTrip(TypedValue::makeBool(true));
    expectPlainRoundTrip(TypedValue::makeChar("abc"));
    expectPlainRoundTrip(TypedValue::makeVarchar("hello"));
    expectPlainRoundTrip(TypedValue::makeText("world"));

    std::vector<uint8_t> int128_bytes = {
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    expectPlainRoundTrip(TypedValue::makeInt128(int128_bytes));
}

TEST(PlainValuePersistenceTest, RoundTrip_TemporalUuidBinary)
{
    expectPlainRoundTrip(TypedValue::makeDate(0, 3600));
    expectPlainRoundTrip(TypedValue::makeTime(3600 * 1000000LL + 12345, -1800));
    expectPlainRoundTrip(TypedValue::makeTimestamp(123456789LL, 0));
    expectPlainRoundTrip(TypedValue::makeInterval(Interval{2, 3, 4000000}));

    std::vector<uint8_t> uuid_bytes = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
    };
    expectPlainRoundTrip(TypedValue::makeUUID(uuid_bytes));

    auto hex_text = TypedValue::makeVarchar("48656c6c6f");
    expectPlainRoundTrip(convertToType(hex_text, TypeInfo(DataType::BLOB), CastFormat::HEX));
    expectPlainRoundTrip(convertToType(hex_text, TypeInfo(DataType::BYTEA), CastFormat::HEX));
    expectPlainRoundTrip(convertToType(hex_text, TypeInfo(DataType::BINARY), CastFormat::HEX));
    expectPlainRoundTrip(convertToType(hex_text, TypeInfo(DataType::VARBINARY), CastFormat::HEX));

    auto json_text = TypedValue::makeVarchar("{\"a\":1}");
    expectPlainRoundTrip(convertToType(json_text, TypeInfo(DataType::JSON)));
    expectPlainRoundTrip(convertToType(json_text, TypeInfo(DataType::JSONB)));
    expectPlainRoundTrip(convertToType(json_text, TypeInfo(DataType::XML)));
}

TEST(PlainValuePersistenceTest, RoundTrip_SpatialRangesAndCollections)
{
    Point pt(1.5, -2.25);
    LineString ls({Point(0, 0), Point(1, 1), Point(2, 3)});
    Polygon poly({{Point(0, 0), Point(0, 1), Point(1, 1), Point(0, 0)}});
    MultiPoint mp({Point(0, 0), Point(1, 2)});
    MultiLineString mls({ls});
    MultiPolygon mpoly({poly});

    expectPlainRoundTrip(TypedValue::makePoint(pt));
    expectPlainRoundTrip(TypedValue::makeLineString(ls));
    expectPlainRoundTrip(TypedValue::makePolygon(poly));
    expectPlainRoundTrip(TypedValue::makeMultiPoint(mp));
    expectPlainRoundTrip(TypedValue::makeMultiLineString(mls));
    expectPlainRoundTrip(TypedValue::makeMultiPolygon(mpoly));

    std::vector<std::shared_ptr<TypedValue>> geoms;
    geoms.push_back(std::make_shared<TypedValue>(TypedValue::makePoint(pt)));
    geoms.push_back(std::make_shared<TypedValue>(TypedValue::makeLineString(ls)));
    expectPlainRoundTrip(TypedValue::makeGeometryCollection(GeometryCollection(std::move(geoms))));

    expectPlainRoundTrip(TypedValue::makeInt4Range(Range<int32_t>(1, 10)));
    expectPlainRoundTrip(TypedValue::makeInt8Range(Range<int64_t>(100, 200, true, false)));
    expectPlainRoundTrip(TypedValue::makeNumRange(Range<double>(0.5, 9.5, BoundType::EXCLUSIVE, BoundType::INCLUSIVE)));
    expectPlainRoundTrip(TypedValue::makeDateRange(Range<int64_t>(0, 7)));
    expectPlainRoundTrip(TypedValue::makeTSRange(Range<int64_t>(1000, 2000)));
    expectPlainRoundTrip(TypedValue::makeTSTZRange(Range<int64_t>(3000, 4000)));

    expectPlainRoundTrip(TypedValue::makeVector({1.0f, 2.0f, 3.0f}));
}

TEST(PlainValuePersistenceTest, RoundTrip_TextSearchNetworkCompositeVariant)
{
    auto tsv = TSVector::fromString("'foo':1 'bar':2");
    ASSERT_TRUE(tsv.has_value());
    expectPlainRoundTrip(TypedValue::makeTSVector(*tsv));

    auto tsq = TSQuery::fromString("foo & bar");
    ASSERT_TRUE(tsq.has_value());
    auto tsq_ptr = std::make_shared<TSQuery>(std::move(*tsq));
    expectPlainRoundTrip(TypedValue::makeTSQuery(tsq_ptr));

    auto inet = InetAddr::fromIPv4(127, 0, 0, 1, 24);
    auto cidr = Cidr::fromIPv4(192, 168, 1, 0, 24);
    expectPlainRoundTrip(TypedValue::makeInet(inet));
    expectPlainRoundTrip(TypedValue::makeCidr(cidr));
    expectPlainRoundTrip(TypedValue::makeMacAddr(MacAddr(0x08, 0x00, 0x2b, 0x01, 0x02, 0x03)));
    expectPlainRoundTrip(TypedValue::makeMacAddr8(MacAddr8(0x08, 0x00, 0x2b, 0x01, 0x02, 0x03, 0x04, 0x05)));

    std::vector<TypedValue> array_values;
    array_values.push_back(TypedValue::makeInt32(1));
    array_values.push_back(TypedValue::makeInt32(2));
    array_values.push_back(TypedValue::makeInt32(3));
    expectPlainRoundTrip(TypedValue::makeArray(array_values));

    std::vector<std::string> field_names = {"id", "name"};
    std::vector<TypedValue> field_values = {
        TypedValue::makeInt32(7),
        TypedValue::makeVarchar("alice")
    };
    expectPlainRoundTrip(TypedValue::makeComposite(field_names, field_values));

    expectPlainRoundTrip(TypedValue::makeVariant(TypedValue::makeVarchar("variant")));
}

TEST(TypeCastTest, StringTemporalUuidBinary)
{
    auto date_val = convertToType(TypedValue::makeVarchar("2024-03-15"),
                                  TypeInfo(DataType::DATE));
    auto date_text = convertToType(date_val, TypeInfo(DataType::VARCHAR));
    EXPECT_EQ(date_text.getVarchar(), "2024-03-15");

    auto time_val = convertToType(TypedValue::makeVarchar("09:30:45"),
                                  TypeInfo(DataType::TIME));
    auto time_text = convertToType(time_val, TypeInfo(DataType::VARCHAR));
    EXPECT_EQ(time_text.getVarchar(), "09:30:45");

    auto ts_val = convertToType(TypedValue::makeVarchar("2024-03-15 10:30:00"),
                                TypeInfo(DataType::TIMESTAMP));
    auto ts_text = convertToType(ts_val, TypeInfo(DataType::VARCHAR));
    EXPECT_EQ(ts_text.getVarchar(), "2024-03-15 10:30:00");

    auto uuid_val = convertToType(
        TypedValue::makeVarchar("00112233-4455-6677-8899-aabbccddeeff"),
        TypeInfo(DataType::UUID));
    auto uuid_text = convertToType(uuid_val, TypeInfo(DataType::VARCHAR));
    EXPECT_EQ(uuid_text.getVarchar(), "00112233-4455-6677-8899-aabbccddeeff");

    auto base64_blob = convertToType(TypedValue::makeVarchar("SGVsbG8="),
                                     TypeInfo(DataType::BLOB),
                                     CastFormat::BASE64);
    auto base64_text = convertToType(base64_blob, TypeInfo(DataType::VARCHAR),
                                     CastFormat::BASE64);
    EXPECT_EQ(base64_text.getVarchar(), "SGVsbG8=");

    auto escape_blob = convertToType(TypedValue::makeVarchar("\\001"),
                                     TypeInfo(DataType::BLOB),
                                     CastFormat::ESCAPE);
    auto escape_text = convertToType(escape_blob, TypeInfo(DataType::VARCHAR),
                                     CastFormat::ESCAPE);
    EXPECT_EQ(escape_text.getVarchar(), "\\001");
}
