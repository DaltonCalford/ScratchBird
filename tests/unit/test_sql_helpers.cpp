#include <gtest/gtest.h>

#include "scratchbird/client/sql_helpers.h"

namespace scratchbird::client {
namespace {

using protocol::ProtocolCodec;
using protocol::WireType;

TEST(SqlHelpersTest, SubstituteParametersKeepsSingleCharacterStringValuesQuoted) {
    std::vector<ProtocolCodec::ColumnValue> params = {
        ProtocolCodec::ColumnValue::fromString("5"),
        ProtocolCodec::ColumnValue::fromString("7"),
    };
    std::vector<WireType> types = {
        WireType::VARCHAR,
        WireType::VARCHAR,
    };

    const std::string sql = substituteParameters(
        "SELECT $1::INTEGER, $2::INTEGER",
        params,
        types);

    EXPECT_EQ(sql, "SELECT '5'::INTEGER, '7'::INTEGER");
}

TEST(SqlHelpersTest, SubstituteParametersPreservesBinaryIntegerValues) {
    std::vector<ProtocolCodec::ColumnValue> params = {
        ProtocolCodec::ColumnValue::fromInt32(5),
        ProtocolCodec::ColumnValue::fromInt32(7),
    };
    std::vector<WireType> types = {
        WireType::INT32,
        WireType::INT32,
    };

    const std::string sql = substituteParameters(
        "SELECT $1::INTEGER, $2::INTEGER",
        params,
        types);

    EXPECT_EQ(sql, "SELECT 5::INTEGER, 7::INTEGER");
}

} // namespace
} // namespace scratchbird::client
