#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/type_extractor.h"
#include "scratchbird/core/range.h"
#include "scratchbird/core/network.h"
#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
#include "scratchbird/core/types.h"
#include "scratchbird/sblr/extract_element_ops.h"
#include "scratchbird/sblr/opcodes.h"

using scratchbird::core::TypedValue;
using scratchbird::core::TypeExtractor;
using scratchbird::sblr::ExtractField;

namespace
{
    constexpr int64_t kMicrosPerSecond = 1000000LL;
    constexpr int64_t kSecondsPerDay = 86400LL;
    constexpr int64_t kMicrosPerDay = kSecondsPerDay * kMicrosPerSecond;

    TypedValue ExtractChecked(const TypedValue& source,
                              ExtractField field,
                              const std::vector<TypedValue>& args = {})
    {
        TypedValue out = TypedValue::makeNull();
        std::string err;
        bool ok = scratchbird::sblr::extractElement(source, field, args, &out, &err);
        EXPECT_TRUE(ok) << err;
        return out;
    }

    TypedValue AlterChecked(const TypedValue& source,
                            ExtractField field,
                            const std::vector<TypedValue>& args,
                            const TypedValue& new_value)
    {
        TypedValue out = TypedValue::makeNull();
        std::string err;
        bool ok = scratchbird::sblr::alterElement(source, field, args, new_value, &out, &err);
        EXPECT_TRUE(ok) << err;
        return out;
    }

    std::vector<uint8_t> makeUint128Bytes(scratchbird::core::uint128_t value)
    {
        std::vector<uint8_t> bytes(16);
        for (int i = 0; i < 16; ++i)
        {
            bytes[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
        }
        return bytes;
    }
}

TEST(ExtractElementTest, DateIsoWeekAndTimezone)
{
    int64_t days = TypeExtractor::ymdToDays(2024, 1, 1); // Monday
    TypedValue date = TypedValue::makeDate(days, -5 * 3600);

    EXPECT_EQ(ExtractChecked(date, ExtractField::YEAR).getInt32(), 2024);
    EXPECT_EQ(ExtractChecked(date, ExtractField::ISO_WEEK).getInt32(), 1);
    EXPECT_EQ(ExtractChecked(date, ExtractField::ISO_DOW).getInt32(), 1);
    EXPECT_EQ(ExtractChecked(date, ExtractField::TIMEZONE_HOUR).getInt32(), -5);
}

TEST(AlterElementTest, DateInvalidLeapYearChange)
{
    int64_t days = TypeExtractor::ymdToDays(2024, 2, 29);
    TypedValue date = TypedValue::makeDate(days, 0);

    TypedValue out = TypedValue::makeNull();
    std::string err;
    bool ok = scratchbird::sblr::alterElement(date, ExtractField::YEAR, {},
                                              TypedValue::makeInt32(2023), &out, &err);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(err.empty());
}

TEST(AlterElementTest, TimestampTimezoneKeepsLocal)
{
    int64_t days = TypeExtractor::ymdToDays(2024, 1, 2);
    int64_t local_micros = days * kMicrosPerDay +
                           (3 * 3600 + 4 * 60 + 5) * kMicrosPerSecond;
    int32_t offset = 2 * 3600;
    int64_t utc_micros = local_micros - static_cast<int64_t>(offset) * kMicrosPerSecond;

    TypedValue ts = TypedValue::makeTimestamp(utc_micros, offset);
    TypedValue updated = AlterChecked(ts, ExtractField::TIMEZONE_HOUR, {},
                                      TypedValue::makeInt32(5));

    EXPECT_EQ(ExtractChecked(updated, ExtractField::HOUR).getInt32(), 3);
    EXPECT_EQ(ExtractChecked(updated, ExtractField::TIMEZONE_HOUR).getInt32(), 5);
}

TEST(ExtractElementTest, IntervalTotals)
{
    scratchbird::core::Interval interval(14, 3,
                                         5 * 3600 * kMicrosPerSecond + 12 * kMicrosPerSecond);
    TypedValue value = TypedValue::makeInterval(interval);

    EXPECT_EQ(ExtractChecked(value, ExtractField::TOTAL_MONTHS).getInt32(), 14);
    EXPECT_EQ(ExtractChecked(value, ExtractField::TOTAL_DAYS).getInt64(), 3);

    double time_seconds = 5.0 * 3600.0 + 12.0;
    EXPECT_NEAR(ExtractChecked(value, ExtractField::TOTAL_SECONDS).getFloat64(), time_seconds, 0.001);

    double epoch_seconds = 14.0 * 30.0 * 86400.0 + 3.0 * 86400.0 + time_seconds;
    EXPECT_NEAR(ExtractChecked(value, ExtractField::EPOCH).getFloat64(), epoch_seconds, 0.001);
}

TEST(ExtractElementTest, Int128HiLo)
{
    scratchbird::core::uint128_t value =
        (static_cast<scratchbird::core::uint128_t>(0x1122334455667788ULL) << 64) |
        0x99AABBCCDDEEFF00ULL;

    TypedValue tv = TypedValue::makeInt128(makeUint128Bytes(value));

    EXPECT_EQ(ExtractChecked(tv, ExtractField::HI64).getUInt64(), 0x1122334455667788ULL);
    EXPECT_EQ(ExtractChecked(tv, ExtractField::LO64).getUInt64(), 0x99AABBCCDDEEFF00ULL);
}

TEST(ExtractElementTest, FloatFlags)
{
    TypedValue nan_val = TypedValue::makeFloat64(std::numeric_limits<double>::quiet_NaN());
    TypedValue inf_val = TypedValue::makeFloat64(std::numeric_limits<double>::infinity());

    EXPECT_TRUE(ExtractChecked(nan_val, ExtractField::IS_NAN).getBool());
    EXPECT_TRUE(ExtractChecked(inf_val, ExtractField::IS_INF).getBool());
}

TEST(ExtractElementTest, DecimalUnscaled)
{
    scratchbird::core::int128_t unscaled = 123456;
    TypedValue dec = TypedValue::makeDecimal(unscaled, 10, 2);
    TypedValue out = ExtractChecked(dec, ExtractField::UNSCALED);

    EXPECT_EQ(static_cast<int64_t>(out.getInt128()), 123456);
}

TEST(ExtractElementTest, MoneyMajorMinor)
{
    TypedValue money = TypedValue::makeMoney(123456);
    EXPECT_EQ(ExtractChecked(money, ExtractField::MAJOR).getInt64(), 12);
    EXPECT_EQ(ExtractChecked(money, ExtractField::MINOR).getInt32(), 3456);
}

TEST(ExtractElementTest, StringLengths)
{
    TypedValue ch = TypedValue::makeChar("ABC   ");
    TypedValue text = TypedValue::makeText("Hello");

    EXPECT_EQ(ExtractChecked(ch, ExtractField::TRIMMED_LENGTH).getInt32(), 3);
    EXPECT_EQ(ExtractChecked(text, ExtractField::CHAR_LENGTH).getInt32(), 5);
    EXPECT_EQ(ExtractChecked(text, ExtractField::OCTET_LENGTH).getInt32(), 5);
}

TEST(ExtractAlterElementTest, JsonPath)
{
    TypedValue json = TypedValue::makeJSON("{\"a\":{\"b\":[1,2]}}");
    TypedValue path = TypedValue::makeText("$.a.b[1]");

    EXPECT_EQ(ExtractChecked(json, ExtractField::PATH, {path}).toString(), "2");

    TypedValue updated = AlterChecked(json, ExtractField::PATH, {path},
                                      TypedValue::makeInt32(7));
    EXPECT_EQ(ExtractChecked(updated, ExtractField::PATH, {path}).toString(), "7");
}

#ifdef HAVE_LIBXML2
TEST(ExtractElementTest, XmlPath)
{
    TypedValue xml = TypedValue::makeXML("<root a=\"1\"><child>2</child></root>");
    TypedValue path = TypedValue::makeText("//child");
    TypedValue out = ExtractChecked(xml, ExtractField::PATH, {path});

    EXPECT_NE(out.toString().find("child"), std::string::npos);
}
#endif

TEST(ExtractAlterElementTest, BinaryByteAndLength)
{
    std::vector<uint8_t> data = {0x0F, 0x00, 0xFF};
    TypedValue bin = TypedValue::makeBinary(data);

    EXPECT_EQ(ExtractChecked(bin, ExtractField::LENGTH).getInt32(), 3);
    EXPECT_EQ(ExtractChecked(bin, ExtractField::BYTE, {TypedValue::makeInt32(2)}).getUInt8(), 0xFF);

    TypedValue updated = AlterChecked(bin, ExtractField::BYTE, {TypedValue::makeInt32(1)},
                                      TypedValue::makeInt32(0x7F));
    EXPECT_EQ(ExtractChecked(updated, ExtractField::BYTE, {TypedValue::makeInt32(1)}).getUInt8(), 0x7F);
}

TEST(ExtractElementTest, VectorMetrics)
{
    TypedValue vec = TypedValue::makeVector({1.0f, 2.0f, 3.0f});
    EXPECT_EQ(ExtractChecked(vec, ExtractField::DIMENSION).getInt32(), 3);

    TypedValue dot = ExtractChecked(vec, ExtractField::DOT,
                                    {TypedValue::makeVector({2.0f, 0.5f, -1.0f})});
    EXPECT_NEAR(dot.getFloat64(), 1.0 * 2.0 + 2.0 * 0.5 + 3.0 * -1.0, 1e-6);
}

TEST(AlterElementTest, ArrayElementUpdate)
{
    TypedValue arr = TypedValue::makeArray({TypedValue::makeInt32(10), TypedValue::makeInt32(20)});

    EXPECT_EQ(ExtractChecked(arr, ExtractField::ELEMENT, {TypedValue::makeInt32(2)}).getInt32(), 20);

    TypedValue updated = AlterChecked(arr, ExtractField::ELEMENT, {TypedValue::makeInt32(1)},
                                      TypedValue::makeInt32(99));
    EXPECT_EQ(ExtractChecked(updated, ExtractField::ELEMENT, {TypedValue::makeInt32(1)}).getInt32(), 99);
    EXPECT_EQ(ExtractChecked(updated, ExtractField::LOWER).getInt32(), 1);
}

TEST(AlterElementTest, CompositeFieldUpdate)
{
    std::vector<std::string> names = {"id", "city"};
    std::vector<TypedValue> values = {TypedValue::makeInt32(1), TypedValue::makeText("Boston")};
    TypedValue comp = TypedValue::makeComposite(names, values);

    EXPECT_EQ(ExtractChecked(comp, ExtractField::FIELD, {TypedValue::makeText("city")}).getText(), "Boston");

    TypedValue updated = AlterChecked(comp, ExtractField::FIELD, {TypedValue::makeInt32(1)},
                                      TypedValue::makeInt32(42));
    EXPECT_EQ(ExtractChecked(updated, ExtractField::FIELD, {TypedValue::makeText("id")}).getInt32(), 42);
}

TEST(AlterElementTest, VariantDatatype)
{
    TypedValue var = TypedValue::makeVariant(TypedValue::makeInt32(5));
    EXPECT_EQ(ExtractChecked(var, ExtractField::DATATYPE).getInt32(),
              static_cast<int32_t>(scratchbird::core::DataType::INT32));

    TypedValue updated = AlterChecked(var, ExtractField::DATATYPE, {},
                                      TypedValue::makeInt32(static_cast<int32_t>(scratchbird::core::DataType::TEXT)));

    EXPECT_EQ(ExtractChecked(updated, ExtractField::DATATYPE).getInt32(),
              static_cast<int32_t>(scratchbird::core::DataType::TEXT));
    EXPECT_EQ(ExtractChecked(updated, ExtractField::VALUE).getText(), "5");
}

TEST(ExtractElementTest, TSVectorLexemes)
{
    std::vector<scratchbird::core::Lexeme> lexemes = {
        scratchbird::core::Lexeme("dog", {2}),
        scratchbird::core::Lexeme("cat", {1})
    };
    scratchbird::core::TSVector vec(lexemes);
    TypedValue tv = TypedValue::makeTSVector(vec);

    TypedValue lex_out = ExtractChecked(tv, ExtractField::LEXEMES);
    const auto &lex_array = lex_out.getArray();
    ASSERT_EQ(lex_array.size(), 2u);
    EXPECT_EQ(lex_array[0].getText(), "cat");
    EXPECT_EQ(lex_array[1].getText(), "dog");
    EXPECT_EQ(ExtractChecked(tv, ExtractField::SIZE).getInt32(), 2);
}

TEST(ExtractElementTest, TSQueryRootOp)
{
    auto query = scratchbird::core::TSQuery::fromString("cat & dog");
    ASSERT_TRUE(query.has_value());
    auto query_ptr = std::make_shared<scratchbird::core::TSQuery>(std::move(*query));
    TypedValue tv = TypedValue::makeTSQuery(query_ptr);

    EXPECT_EQ(ExtractChecked(tv, ExtractField::ROOT_OP).getText(), "AND");
}

TEST(AlterElementTest, RangeUpperUpdate)
{
    scratchbird::core::Range<int32_t> range(1, 10, true, false);
    TypedValue tv = TypedValue::makeInt4Range(range);

    EXPECT_EQ(ExtractChecked(tv, ExtractField::LOWER).getInt32(), 1);
    EXPECT_EQ(ExtractChecked(tv, ExtractField::UPPER).getInt32(), 10);

    TypedValue updated = AlterChecked(tv, ExtractField::UPPER, {}, TypedValue::makeInt32(12));
    EXPECT_EQ(ExtractChecked(updated, ExtractField::UPPER).getInt32(), 12);

    TypedValue emptied = AlterChecked(tv, ExtractField::ISEMPTY, {}, TypedValue::makeBool(true));
    EXPECT_TRUE(ExtractChecked(emptied, ExtractField::ISEMPTY).getBool());
}

TEST(ExtractAlterElementTest, InetNetwork)
{
    auto inet_opt = scratchbird::core::InetAddr::fromString("192.168.1.10/24");
    ASSERT_TRUE(inet_opt.has_value());
    TypedValue inet = TypedValue::makeInet(*inet_opt);

    EXPECT_EQ(ExtractChecked(inet, ExtractField::NETMASK).getInt32(), 24);
    EXPECT_EQ(ExtractChecked(inet, ExtractField::NETWORK).getInet().toString(), "192.168.1.0/24");

    TypedValue updated = AlterChecked(inet, ExtractField::ADDRESS, {},
                                      TypedValue::makeText("192.168.1.20"));
    EXPECT_EQ(ExtractChecked(updated, ExtractField::ADDRESS).getText(), "192.168.1.20");
}

TEST(ExtractElementTest, MacAddrFlags)
{
    scratchbird::core::MacAddr mac(0x08, 0x00, 0x2B, 0x01, 0x02, 0x03);
    TypedValue value = TypedValue::makeMacAddr(mac);

    TypedValue oui = ExtractChecked(value, ExtractField::OUI);
    const auto &bytes = oui.getBinary();
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x08);
    EXPECT_FALSE(ExtractChecked(value, ExtractField::IS_LOCAL).getBool());
}

TEST(AlterElementTest, PointUpdate)
{
    scratchbird::core::Point pt;
    pt.x = 1.5;
    pt.y = -2.0;
    pt.srid = 4326;
    TypedValue value = TypedValue::makePoint(pt);

    EXPECT_DOUBLE_EQ(ExtractChecked(value, ExtractField::X).getFloat64(), 1.5);

    TypedValue updated = AlterChecked(value, ExtractField::X, {},
                                      TypedValue::makeFloat64(2.5));
    EXPECT_DOUBLE_EQ(ExtractChecked(updated, ExtractField::X).getFloat64(), 2.5);
}
