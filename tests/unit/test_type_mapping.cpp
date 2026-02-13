/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <gtest/gtest.h>
#include "scratchbird/udr/type_mapping.h"
#include "scratchbird/core/types.h"

using namespace scratchbird::core;
using namespace scratchbird::udr;

// =============================================================================
// PostgreSQL Type Mapping Tests
// =============================================================================

class PostgreSQLTypeMappingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test DataType to PostgreSQL OID conversions (toPostgreSQL)
TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Boolean) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::BOOLEAN), 16u);  // BOOL
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_TinyInt) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::TINYINT), 21u);  // INT2
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_SmallInt) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::SMALLINT), 21u);  // INT2
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Integer) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::INTEGER), 23u);  // INT4
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_BigInt) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::BIGINT), 20u);  // INT8
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Real) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::REAL), 700u);  // FLOAT4
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Float) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::FLOAT), 700u);  // FLOAT4
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Double) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::DOUBLE), 701u);  // FLOAT8
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Numeric) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::NUMERIC), 1700u);  // NUMERIC
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Decimal) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::DECIMAL), 1700u);  // NUMERIC
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Char) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::CHAR), 1042u);  // BPCHAR
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Varchar) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::VARCHAR), 1043u);  // VARCHAR
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Text) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::TEXT), 25u);  // TEXT
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Date) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::DATE), 1082u);  // DATE
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Time) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::TIME), 1083u);  // TIME
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Timestamp) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::TIMESTAMP), 1114u);  // TIMESTAMP
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_TimestampWithZone) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::TIMESTAMP_WITH_ZONE), 1184u);  // TIMESTAMPTZ
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Interval) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::INTERVAL), 1186u);  // INTERVAL
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Binary) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::BINARY), 17u);  // BYTEA
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_VarBinary) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::VARBINARY), 17u);  // BYTEA
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Blob) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::BLOB), 17u);  // BYTEA
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Json) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::JSON), 3802u);  // JSONB
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Xml) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::XML), 142u);  // XML
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Uuid) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::UUID), 2950u);  // UUID
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Inet) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::INET), 869u);  // INET
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Cidr) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::CIDR), 650u);  // CIDR
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_MacAddr) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::MACADDR), 829u);  // MACADDR
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Bit) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::BIT), 1560u);  // BIT
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_Null) {
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::NULL_TYPE), 705u);  // UNKNOWN
}

TEST_F(PostgreSQLTypeMappingTest, ToPostgreSQL_UnknownFallback) {
    // Unknown types should fallback to TEXT
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::UNKNOWN), 25u);  // TEXT
}

// Test PostgreSQL OID to DataType conversions (fromPostgreSQL)
TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Bool) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(16u), DataType::BOOLEAN);  // BOOL
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Int2) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(21u), DataType::SMALLINT);  // INT2
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Int4) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(23u), DataType::INTEGER);  // INT4
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Int8) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(20u), DataType::BIGINT);  // INT8
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Float4) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(700u), DataType::REAL);  // FLOAT4
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Float8) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(701u), DataType::DOUBLE);  // FLOAT8
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Numeric) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1700u), DataType::NUMERIC);  // NUMERIC
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Char) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(18u), DataType::CHAR);  // CHAR
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_BpChar) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1042u), DataType::CHAR);  // BPCHAR
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Varchar) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1043u), DataType::VARCHAR);  // VARCHAR
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Text) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(25u), DataType::TEXT);  // TEXT
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Name) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(19u), DataType::TEXT);  // NAME -> TEXT
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Date) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1082u), DataType::DATE);  // DATE
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Time) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1083u), DataType::TIME);  // TIME
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_TimeTz) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1266u), DataType::TIME);  // TIMETZ -> TIME
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Timestamp) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1114u), DataType::TIMESTAMP);  // TIMESTAMP
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_TimestampTz) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1184u), DataType::TIMESTAMP_WITH_ZONE);  // TIMESTAMPTZ
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Interval) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1186u), DataType::INTERVAL);  // INTERVAL
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Bytea) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(17u), DataType::BLOB);  // BYTEA -> BLOB
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Json) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(114u), DataType::JSON);  // JSON
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Jsonb) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(3802u), DataType::JSON);  // JSONB -> JSON
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Xml) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(142u), DataType::XML);  // XML
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Uuid) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(2950u), DataType::UUID);  // UUID
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Inet) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(869u), DataType::INET);  // INET
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Cidr) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(650u), DataType::CIDR);  // CIDR
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_MacAddr) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(829u), DataType::MACADDR);  // MACADDR
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_MacAddr8) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(774u), DataType::MACADDR);  // MACADDR8 -> MACADDR
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Bit) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1560u), DataType::BIT);  // BIT
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_VarBit) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1562u), DataType::BIT);  // VARBIT -> BIT
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Oid) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(26u), DataType::INTEGER);  // OID -> INTEGER
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Xid) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(28u), DataType::INTEGER);  // XID -> INTEGER
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Cid) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(29u), DataType::INTEGER);  // CID -> INTEGER
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Cash) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(790u), DataType::NUMERIC);  // CASH -> NUMERIC
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Unknown) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(705u), DataType::NULL_TYPE);  // UNKNOWN
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_Void) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(2278u), DataType::NULL_TYPE);  // VOID
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_GeometricTypes) {
    // Geometric types should map to TEXT
    EXPECT_EQ(TypeMapping::fromPostgreSQL(600u), DataType::TEXT);   // POINT
    EXPECT_EQ(TypeMapping::fromPostgreSQL(601u), DataType::TEXT);   // LSEG
    EXPECT_EQ(TypeMapping::fromPostgreSQL(603u), DataType::TEXT);   // BOX
    EXPECT_EQ(TypeMapping::fromPostgreSQL(604u), DataType::TEXT);   // POLYGON
    EXPECT_EQ(TypeMapping::fromPostgreSQL(718u), DataType::TEXT);   // CIRCLE
    EXPECT_EQ(TypeMapping::fromPostgreSQL(628u), DataType::TEXT);   // LINE
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_ArrayTypes) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1005u), DataType::ARRAY);   // INT2ARRAY
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1007u), DataType::ARRAY);   // INT4ARRAY
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1009u), DataType::ARRAY);   // TEXTARRAY
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1016u), DataType::ARRAY);   // INT8ARRAY
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1021u), DataType::ARRAY);   // FLOAT4ARRAY
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1022u), DataType::ARRAY);   // FLOAT8ARRAY
    EXPECT_EQ(TypeMapping::fromPostgreSQL(1231u), DataType::ARRAY);   // NUMERICARRAY
    EXPECT_EQ(TypeMapping::fromPostgreSQL(2951u), DataType::ARRAY);   // UUIDARRAY
    EXPECT_EQ(TypeMapping::fromPostgreSQL(3807u), DataType::ARRAY);   // JSONBARRAY
}

TEST_F(PostgreSQLTypeMappingTest, FromPostgreSQL_UnknownOid) {
    EXPECT_EQ(TypeMapping::fromPostgreSQL(99999u), DataType::UNKNOWN);  // Unknown OID
}

// PostgreSQL Round-trip tests
TEST_F(PostgreSQLTypeMappingTest, RoundTrip_Boolean) {
    auto original = DataType::BOOLEAN;
    auto oid = TypeMapping::toPostgreSQL(original);
    auto roundtrip = TypeMapping::fromPostgreSQL(oid);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(PostgreSQLTypeMappingTest, RoundTrip_Integer) {
    auto original = DataType::INTEGER;
    auto oid = TypeMapping::toPostgreSQL(original);
    auto roundtrip = TypeMapping::fromPostgreSQL(oid);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(PostgreSQLTypeMappingTest, RoundTrip_BigInt) {
    auto original = DataType::BIGINT;
    auto oid = TypeMapping::toPostgreSQL(original);
    auto roundtrip = TypeMapping::fromPostgreSQL(oid);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(PostgreSQLTypeMappingTest, RoundTrip_Text) {
    auto original = DataType::TEXT;
    auto oid = TypeMapping::toPostgreSQL(original);
    auto roundtrip = TypeMapping::fromPostgreSQL(oid);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(PostgreSQLTypeMappingTest, RoundTrip_Timestamp) {
    auto original = DataType::TIMESTAMP;
    auto oid = TypeMapping::toPostgreSQL(original);
    auto roundtrip = TypeMapping::fromPostgreSQL(oid);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(PostgreSQLTypeMappingTest, RoundTrip_Json) {
    auto original = DataType::JSON;
    auto oid = TypeMapping::toPostgreSQL(original);
    auto roundtrip = TypeMapping::fromPostgreSQL(oid);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(PostgreSQLTypeMappingTest, RoundTrip_Uuid) {
    auto original = DataType::UUID;
    auto oid = TypeMapping::toPostgreSQL(original);
    auto roundtrip = TypeMapping::fromPostgreSQL(oid);
    EXPECT_EQ(roundtrip, original);
}

// =============================================================================
// MySQL Type Mapping Tests
// =============================================================================

class MySQLTypeMappingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test DataType to MySQL type conversions
TEST_F(MySQLTypeMappingTest, ToMySQL_Boolean) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::BOOLEAN), 0x01u);  // MYSQL_TYPE_TINY
}

TEST_F(MySQLTypeMappingTest, ToMySQL_TinyInt) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::TINYINT), 0x01u);  // MYSQL_TYPE_TINY
}

TEST_F(MySQLTypeMappingTest, ToMySQL_SmallInt) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::SMALLINT), 0x02u);  // MYSQL_TYPE_SHORT
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Integer) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::INTEGER), 0x03u);  // MYSQL_TYPE_LONG
}

TEST_F(MySQLTypeMappingTest, ToMySQL_MediumInt) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::MEDIUMINT), 0x03u);  // MYSQL_TYPE_LONG
}

TEST_F(MySQLTypeMappingTest, ToMySQL_BigInt) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::BIGINT), 0x08u);  // MYSQL_TYPE_LONGLONG
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Float) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::FLOAT), 0x04u);  // MYSQL_TYPE_FLOAT
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Real) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::REAL), 0x04u);  // MYSQL_TYPE_FLOAT
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Double) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::DOUBLE), 0x05u);  // MYSQL_TYPE_DOUBLE
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Numeric) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::NUMERIC), 0xf6u);  // MYSQL_TYPE_NEWDECIMAL
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Decimal) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::DECIMAL), 0xf6u);  // MYSQL_TYPE_NEWDECIMAL
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Char) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::CHAR), 0xfeu);  // MYSQL_TYPE_STRING
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Varchar) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::VARCHAR), 0x0fu);  // MYSQL_TYPE_VARCHAR
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Text) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::TEXT), 0xfcu);  // MYSQL_TYPE_BLOB
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Date) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::DATE), 0x0au);  // MYSQL_TYPE_DATE
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Time) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::TIME), 0x0bu);  // MYSQL_TYPE_TIME
}

TEST_F(MySQLTypeMappingTest, ToMySQL_DateTime) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::DATETIME), 0x0cu);  // MYSQL_TYPE_DATETIME
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Timestamp) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::TIMESTAMP), 0x07u);  // MYSQL_TYPE_TIMESTAMP
}

TEST_F(MySQLTypeMappingTest, ToMySQL_TimestampWithZone) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::TIMESTAMP_WITH_ZONE), 0x07u);  // MYSQL_TYPE_TIMESTAMP
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Year) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::YEAR), 0x0du);  // MYSQL_TYPE_YEAR
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Binary) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::BINARY), 0xfcu);  // MYSQL_TYPE_BLOB
}

TEST_F(MySQLTypeMappingTest, ToMySQL_VarBinary) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::VARBINARY), 0xfcu);  // MYSQL_TYPE_BLOB
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Blob) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::BLOB), 0xfcu);  // MYSQL_TYPE_BLOB
}

TEST_F(MySQLTypeMappingTest, ToMySQL_TinyBlob) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::TINYBLOB), 0xfcu);  // MYSQL_TYPE_BLOB
}

TEST_F(MySQLTypeMappingTest, ToMySQL_MediumBlob) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::MEDIUMBLOB), 0xfcu);  // MYSQL_TYPE_BLOB
}

TEST_F(MySQLTypeMappingTest, ToMySQL_LongBlob) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::LONGBLOB), 0xfcu);  // MYSQL_TYPE_BLOB
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Json) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::JSON), 0xf5u);  // MYSQL_TYPE_JSON
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Bit) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::BIT), 0x10u);  // MYSQL_TYPE_BIT
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Null) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::NULL_TYPE), 0x06u);  // MYSQL_TYPE_NULL
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Enum) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::ENUM), 0xf7u);  // MYSQL_TYPE_ENUM
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Set) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::SET), 0xf8u);  // MYSQL_TYPE_SET
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Geometry) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::GEOMETRY), 0xffu);  // MYSQL_TYPE_GEOMETRY
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Point) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::POINT), 0xffu);  // MYSQL_TYPE_GEOMETRY
}

TEST_F(MySQLTypeMappingTest, ToMySQL_LineString) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::LINESTRING), 0xffu);  // MYSQL_TYPE_GEOMETRY
}

TEST_F(MySQLTypeMappingTest, ToMySQL_Polygon) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::POLYGON), 0xffu);  // MYSQL_TYPE_GEOMETRY
}

TEST_F(MySQLTypeMappingTest, ToMySQL_MultiPoint) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::MULTIPOINT), 0xffu);  // MYSQL_TYPE_GEOMETRY
}

TEST_F(MySQLTypeMappingTest, ToMySQL_MultiLineString) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::MULTILINESTRING), 0xffu);  // MYSQL_TYPE_GEOMETRY
}

TEST_F(MySQLTypeMappingTest, ToMySQL_MultiPolygon) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::MULTIPOLYGON), 0xffu);  // MYSQL_TYPE_GEOMETRY
}

TEST_F(MySQLTypeMappingTest, ToMySQL_GeometryCollection) {
    EXPECT_EQ(TypeMapping::toMySQL(DataType::GEOMETRYCOLLECTION), 0xffu);  // MYSQL_TYPE_GEOMETRY
}

// Test MySQL type to DataType conversions
TEST_F(MySQLTypeMappingTest, FromMySQL_Tiny) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x01u), DataType::TINYINT);  // MYSQL_TYPE_TINY
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Short) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x02u), DataType::SMALLINT);  // MYSQL_TYPE_SHORT
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Long) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x03u), DataType::INTEGER);  // MYSQL_TYPE_LONG
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Int24) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x09u), DataType::INTEGER);  // MYSQL_TYPE_INT24 -> INTEGER
}

TEST_F(MySQLTypeMappingTest, FromMySQL_LongLong) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x08u), DataType::BIGINT);  // MYSQL_TYPE_LONGLONG
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Float) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x04u), DataType::FLOAT);  // MYSQL_TYPE_FLOAT
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Double) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x05u), DataType::DOUBLE);  // MYSQL_TYPE_DOUBLE
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Decimal) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x00u), DataType::NUMERIC);  // MYSQL_TYPE_DECIMAL
}

TEST_F(MySQLTypeMappingTest, FromMySQL_NewDecimal) {
    EXPECT_EQ(TypeMapping::fromMySQL(0xf6u), DataType::NUMERIC);  // MYSQL_TYPE_NEWDECIMAL
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Varchar) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x0fu), DataType::VARCHAR);  // MYSQL_TYPE_VARCHAR
}

TEST_F(MySQLTypeMappingTest, FromMySQL_VarString) {
    EXPECT_EQ(TypeMapping::fromMySQL(0xfdu), DataType::VARCHAR);  // MYSQL_TYPE_VAR_STRING -> VARCHAR
}

TEST_F(MySQLTypeMappingTest, FromMySQL_String) {
    EXPECT_EQ(TypeMapping::fromMySQL(0xfeu), DataType::CHAR);  // MYSQL_TYPE_STRING -> CHAR
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Blob) {
    EXPECT_EQ(TypeMapping::fromMySQL(0xfcu), DataType::BLOB);  // MYSQL_TYPE_BLOB
}

TEST_F(MySQLTypeMappingTest, FromMySQL_TinyBlob) {
    EXPECT_EQ(TypeMapping::fromMySQL(0xf9u), DataType::BLOB);  // MYSQL_TYPE_TINY_BLOB -> BLOB
}

TEST_F(MySQLTypeMappingTest, FromMySQL_MediumBlob) {
    EXPECT_EQ(TypeMapping::fromMySQL(0xfau), DataType::BLOB);  // MYSQL_TYPE_MEDIUM_BLOB -> BLOB
}

TEST_F(MySQLTypeMappingTest, FromMySQL_LongBlob) {
    EXPECT_EQ(TypeMapping::fromMySQL(0xfbu), DataType::BLOB);  // MYSQL_TYPE_LONG_BLOB -> BLOB
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Date) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x0au), DataType::DATE);  // MYSQL_TYPE_DATE
}

TEST_F(MySQLTypeMappingTest, FromMySQL_NewDate) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x0eu), DataType::DATE);  // MYSQL_TYPE_NEWDATE -> DATE
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Time) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x0bu), DataType::TIME);  // MYSQL_TYPE_TIME
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Time2) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x13u), DataType::TIME);  // MYSQL_TYPE_TIME2 -> TIME
}

TEST_F(MySQLTypeMappingTest, FromMySQL_DateTime) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x0cu), DataType::DATETIME);  // MYSQL_TYPE_DATETIME
}

TEST_F(MySQLTypeMappingTest, FromMySQL_DateTime2) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x12u), DataType::DATETIME);  // MYSQL_TYPE_DATETIME2 -> DATETIME
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Timestamp) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x07u), DataType::TIMESTAMP);  // MYSQL_TYPE_TIMESTAMP
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Timestamp2) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x11u), DataType::TIMESTAMP);  // MYSQL_TYPE_TIMESTAMP2 -> TIMESTAMP
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Year) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x0du), DataType::YEAR);  // MYSQL_TYPE_YEAR
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Bit) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x10u), DataType::BIT);  // MYSQL_TYPE_BIT
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Json) {
    EXPECT_EQ(TypeMapping::fromMySQL(0xf5u), DataType::JSON);  // MYSQL_TYPE_JSON
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Enum) {
    EXPECT_EQ(TypeMapping::fromMySQL(0xf7u), DataType::ENUM);  // MYSQL_TYPE_ENUM
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Set) {
    EXPECT_EQ(TypeMapping::fromMySQL(0xf8u), DataType::SET);  // MYSQL_TYPE_SET
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Geometry) {
    EXPECT_EQ(TypeMapping::fromMySQL(0xffu), DataType::GEOMETRY);  // MYSQL_TYPE_GEOMETRY
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Null) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x06u), DataType::NULL_TYPE);  // MYSQL_TYPE_NULL
}

TEST_F(MySQLTypeMappingTest, FromMySQL_Unknown) {
    EXPECT_EQ(TypeMapping::fromMySQL(0x99u), DataType::UNKNOWN);  // Unknown type
}

// MySQL Round-trip tests
TEST_F(MySQLTypeMappingTest, RoundTrip_TinyInt) {
    auto original = DataType::TINYINT;
    auto mysql_type = TypeMapping::toMySQL(original);
    auto roundtrip = TypeMapping::fromMySQL(mysql_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(MySQLTypeMappingTest, RoundTrip_Integer) {
    auto original = DataType::INTEGER;
    auto mysql_type = TypeMapping::toMySQL(original);
    auto roundtrip = TypeMapping::fromMySQL(mysql_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(MySQLTypeMappingTest, RoundTrip_BigInt) {
    auto original = DataType::BIGINT;
    auto mysql_type = TypeMapping::toMySQL(original);
    auto roundtrip = TypeMapping::fromMySQL(mysql_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(MySQLTypeMappingTest, RoundTrip_Double) {
    auto original = DataType::DOUBLE;
    auto mysql_type = TypeMapping::toMySQL(original);
    auto roundtrip = TypeMapping::fromMySQL(mysql_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(MySQLTypeMappingTest, RoundTrip_Varchar) {
    auto original = DataType::VARCHAR;
    auto mysql_type = TypeMapping::toMySQL(original);
    auto roundtrip = TypeMapping::fromMySQL(mysql_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(MySQLTypeMappingTest, RoundTrip_DateTime) {
    auto original = DataType::DATETIME;
    auto mysql_type = TypeMapping::toMySQL(original);
    auto roundtrip = TypeMapping::fromMySQL(mysql_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(MySQLTypeMappingTest, RoundTrip_Json) {
    auto original = DataType::JSON;
    auto mysql_type = TypeMapping::toMySQL(original);
    auto roundtrip = TypeMapping::fromMySQL(mysql_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(MySQLTypeMappingTest, RoundTrip_Enum) {
    auto original = DataType::ENUM;
    auto mysql_type = TypeMapping::toMySQL(original);
    auto roundtrip = TypeMapping::fromMySQL(mysql_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(MySQLTypeMappingTest, RoundTrip_Set) {
    auto original = DataType::SET;
    auto mysql_type = TypeMapping::toMySQL(original);
    auto roundtrip = TypeMapping::fromMySQL(mysql_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(MySQLTypeMappingTest, RoundTrip_Geometry) {
    auto original = DataType::GEOMETRY;
    auto mysql_type = TypeMapping::toMySQL(original);
    auto roundtrip = TypeMapping::fromMySQL(mysql_type);
    EXPECT_EQ(roundtrip, original);
}

// =============================================================================
// Firebird Type Mapping Tests
// =============================================================================

class FirebirdTypeMappingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test DataType to Firebird BLR type conversions
TEST_F(FirebirdTypeMappingTest, ToFirebird_Boolean) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::BOOLEAN), 23u);  // BLR_BOOLEAN
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_TinyInt) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::TINYINT), 7u);  // BLR_SHORT
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_SmallInt) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::SMALLINT), 7u);  // BLR_SHORT
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Integer) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::INTEGER), 8u);  // BLR_LONG
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_BigInt) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::BIGINT), 16u);  // BLR_INT64
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Int128) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::INT128), 26u);  // BLR_INT128
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Float) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::FLOAT), 10u);  // BLR_FLOAT
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Real) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::REAL), 10u);  // BLR_FLOAT
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Double) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::DOUBLE), 27u);  // BLR_DOUBLE
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Numeric) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::NUMERIC), 16u);  // BLR_INT64
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Decimal) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::DECIMAL), 16u);  // BLR_INT64
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Char) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::CHAR), 14u);  // BLR_TEXT
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Varchar) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::VARCHAR), 14u);  // BLR_TEXT
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Date) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::DATE), 12u);  // BLR_SQL_DATE
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Time) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::TIME), 13u);  // BLR_SQL_TIME
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_TimeWithZone) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::TIME_WITH_ZONE), 28u);  // BLR_TIME_TZ
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Timestamp) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::TIMESTAMP), 35u);  // BLR_TIMESTAMP
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_TimestampWithZone) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::TIMESTAMP_WITH_ZONE), 29u);  // BLR_TIMESTAMP_TZ
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Blob) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::BLOB), 261u);  // BLR_BLOB
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_BlobSubTypeText) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::BLOB_SUB_TYPE_TEXT), 261u);  // BLR_BLOB
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Array) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::ARRAY), 9u);  // BLR_QUAD/ARRAY
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_Null) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::NULL_TYPE), 0u);
}

TEST_F(FirebirdTypeMappingTest, ToFirebird_UnknownFallback) {
    EXPECT_EQ(TypeMapping::toFirebird(DataType::UNKNOWN), 14u);  // BLR_TEXT fallback
}

// Test Firebird BLR type to DataType conversions
TEST_F(FirebirdTypeMappingTest, FromFirebird_Boolean) {
    EXPECT_EQ(TypeMapping::fromFirebird(23u), DataType::BOOLEAN);  // BLR_BOOLEAN
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Short) {
    EXPECT_EQ(TypeMapping::fromFirebird(7u), DataType::SMALLINT);  // BLR_SHORT
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Long) {
    EXPECT_EQ(TypeMapping::fromFirebird(8u), DataType::INTEGER);  // BLR_LONG
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Int64) {
    EXPECT_EQ(TypeMapping::fromFirebird(16u), DataType::BIGINT);  // BLR_INT64
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Int128) {
    EXPECT_EQ(TypeMapping::fromFirebird(26u), DataType::INT128);  // BLR_INT128
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_DecFixed) {
    EXPECT_EQ(TypeMapping::fromFirebird(26u), DataType::INT128);  // BLR_DEC_FIXED -> INT128
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Dec16) {
    EXPECT_EQ(TypeMapping::fromFirebird(24u), DataType::DECFLOAT16);  // BLR_DEC16
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Dec34) {
    EXPECT_EQ(TypeMapping::fromFirebird(25u), DataType::DECFLOAT34);  // BLR_DEC34
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Float) {
    EXPECT_EQ(TypeMapping::fromFirebird(10u), DataType::FLOAT);  // BLR_FLOAT
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Double) {
    EXPECT_EQ(TypeMapping::fromFirebird(27u), DataType::DOUBLE);  // BLR_DOUBLE
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_DFloat) {
    EXPECT_EQ(TypeMapping::fromFirebird(11u), DataType::DOUBLE);  // BLR_D_FLOAT -> DOUBLE
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Text) {
    EXPECT_EQ(TypeMapping::fromFirebird(14u), DataType::VARCHAR);  // BLR_TEXT -> VARCHAR
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_SqlDate) {
    EXPECT_EQ(TypeMapping::fromFirebird(12u), DataType::DATE);  // BLR_SQL_DATE
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_SqlTime) {
    EXPECT_EQ(TypeMapping::fromFirebird(13u), DataType::TIME);  // BLR_SQL_TIME
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_TimeTz) {
    EXPECT_EQ(TypeMapping::fromFirebird(28u), DataType::TIME_WITH_ZONE);  // BLR_TIME_TZ
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Timestamp) {
    EXPECT_EQ(TypeMapping::fromFirebird(35u), DataType::TIMESTAMP);  // BLR_TIMESTAMP
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_TimestampTz) {
    EXPECT_EQ(TypeMapping::fromFirebird(29u), DataType::TIMESTAMP_WITH_ZONE);  // BLR_TIMESTAMP_TZ
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Blob) {
    EXPECT_EQ(TypeMapping::fromFirebird(261u), DataType::BLOB);  // BLR_BLOB
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Array) {
    EXPECT_EQ(TypeMapping::fromFirebird(9u), DataType::ARRAY);  // BLR_QUAD -> ARRAY
}

TEST_F(FirebirdTypeMappingTest, FromFirebird_Unknown) {
    EXPECT_EQ(TypeMapping::fromFirebird(999u), DataType::UNKNOWN);  // Unknown BLR type
}

// Firebird Round-trip tests
TEST_F(FirebirdTypeMappingTest, RoundTrip_Boolean) {
    auto original = DataType::BOOLEAN;
    auto blr_type = TypeMapping::toFirebird(original);
    auto roundtrip = TypeMapping::fromFirebird(blr_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(FirebirdTypeMappingTest, RoundTrip_SmallInt) {
    auto original = DataType::SMALLINT;
    auto blr_type = TypeMapping::toFirebird(original);
    auto roundtrip = TypeMapping::fromFirebird(blr_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(FirebirdTypeMappingTest, RoundTrip_Integer) {
    auto original = DataType::INTEGER;
    auto blr_type = TypeMapping::toFirebird(original);
    auto roundtrip = TypeMapping::fromFirebird(blr_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(FirebirdTypeMappingTest, RoundTrip_BigInt) {
    auto original = DataType::BIGINT;
    auto blr_type = TypeMapping::toFirebird(original);
    auto roundtrip = TypeMapping::fromFirebird(blr_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(FirebirdTypeMappingTest, RoundTrip_Int128) {
    auto original = DataType::INT128;
    auto blr_type = TypeMapping::toFirebird(original);
    auto roundtrip = TypeMapping::fromFirebird(blr_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(FirebirdTypeMappingTest, RoundTrip_Double) {
    auto original = DataType::DOUBLE;
    auto blr_type = TypeMapping::toFirebird(original);
    auto roundtrip = TypeMapping::fromFirebird(blr_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(FirebirdTypeMappingTest, RoundTrip_Timestamp) {
    auto original = DataType::TIMESTAMP;
    auto blr_type = TypeMapping::toFirebird(original);
    auto roundtrip = TypeMapping::fromFirebird(blr_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(FirebirdTypeMappingTest, RoundTrip_Blob) {
    auto original = DataType::BLOB;
    auto blr_type = TypeMapping::toFirebird(original);
    auto roundtrip = TypeMapping::fromFirebird(blr_type);
    EXPECT_EQ(roundtrip, original);
}

TEST_F(FirebirdTypeMappingTest, RoundTrip_Array) {
    auto original = DataType::ARRAY;
    auto blr_type = TypeMapping::toFirebird(original);
    auto roundtrip = TypeMapping::fromFirebird(blr_type);
    EXPECT_EQ(roundtrip, original);
}

// =============================================================================
// SBWP Type Mapping Tests
// =============================================================================

class SBWPTypeMappingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test DataType to SBWP type code conversions
TEST_F(SBWPTypeMappingTest, ToSBWP_AllTypes) {
    EXPECT_EQ(TypeMapping::toSBWP(DataType::UNKNOWN), 0u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::INT8), 1u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::INT16), 2u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::INT32), 3u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::INT64), 4u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::INT128), 5u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::UINT8), 6u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::UINT16), 7u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::UINT32), 8u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::UINT64), 9u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::FLOAT32), 10u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::FLOAT64), 11u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::DECIMAL), 12u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::MONEY), 13u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::UINT128), 14u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::DECFLOAT16), 15u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::DECFLOAT34), 16u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::CHAR), 20u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::VARCHAR), 21u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::TEXT), 22u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::BINARY), 30u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::VARBINARY), 31u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::BLOB), 32u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::BYTEA), 33u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::DATE), 40u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::TIME), 41u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::TIMESTAMP), 42u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::INTERVAL), 43u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::BOOLEAN), 50u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::UUID), 60u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::JSON), 61u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::JSONB), 62u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::XML), 63u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::VECTOR), 64u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::POINT), 65u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::LINESTRING), 66u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::POLYGON), 67u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::MULTIPOINT), 68u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::MULTILINESTRING), 69u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::MULTIPOLYGON), 70u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::GEOMETRYCOLLECTION), 71u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::ARRAY), 72u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::COMPOSITE), 73u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::TSVECTOR), 74u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::TSQUERY), 75u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::INT4RANGE), 76u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::INT8RANGE), 77u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::NUMRANGE), 78u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::TSRANGE), 79u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::TSTZRANGE), 80u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::DATERANGE), 81u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::INET), 86u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::CIDR), 87u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::MACADDR), 88u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::MACADDR8), 89u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::VARIANT), 90u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::NULL_TYPE), 255u);
}

// Test SBWP type code to DataType conversions
TEST_F(SBWPTypeMappingTest, FromSBWP_AllTypes) {
    EXPECT_EQ(TypeMapping::fromSBWP(0u), DataType::UNKNOWN);
    EXPECT_EQ(TypeMapping::fromSBWP(1u), DataType::INT8);
    EXPECT_EQ(TypeMapping::fromSBWP(2u), DataType::INT16);
    EXPECT_EQ(TypeMapping::fromSBWP(3u), DataType::INT32);
    EXPECT_EQ(TypeMapping::fromSBWP(4u), DataType::INT64);
    EXPECT_EQ(TypeMapping::fromSBWP(5u), DataType::INT128);
    EXPECT_EQ(TypeMapping::fromSBWP(6u), DataType::UINT8);
    EXPECT_EQ(TypeMapping::fromSBWP(7u), DataType::UINT16);
    EXPECT_EQ(TypeMapping::fromSBWP(8u), DataType::UINT32);
    EXPECT_EQ(TypeMapping::fromSBWP(9u), DataType::UINT64);
    EXPECT_EQ(TypeMapping::fromSBWP(10u), DataType::FLOAT32);
    EXPECT_EQ(TypeMapping::fromSBWP(11u), DataType::FLOAT64);
    EXPECT_EQ(TypeMapping::fromSBWP(12u), DataType::DECIMAL);
    EXPECT_EQ(TypeMapping::fromSBWP(13u), DataType::MONEY);
    EXPECT_EQ(TypeMapping::fromSBWP(14u), DataType::UINT128);
    EXPECT_EQ(TypeMapping::fromSBWP(15u), DataType::DECFLOAT16);
    EXPECT_EQ(TypeMapping::fromSBWP(16u), DataType::DECFLOAT34);
    EXPECT_EQ(TypeMapping::fromSBWP(20u), DataType::CHAR);
    EXPECT_EQ(TypeMapping::fromSBWP(21u), DataType::VARCHAR);
    EXPECT_EQ(TypeMapping::fromSBWP(22u), DataType::TEXT);
    EXPECT_EQ(TypeMapping::fromSBWP(30u), DataType::BINARY);
    EXPECT_EQ(TypeMapping::fromSBWP(31u), DataType::VARBINARY);
    EXPECT_EQ(TypeMapping::fromSBWP(32u), DataType::BLOB);
    EXPECT_EQ(TypeMapping::fromSBWP(33u), DataType::BYTEA);
    EXPECT_EQ(TypeMapping::fromSBWP(40u), DataType::DATE);
    EXPECT_EQ(TypeMapping::fromSBWP(41u), DataType::TIME);
    EXPECT_EQ(TypeMapping::fromSBWP(42u), DataType::TIMESTAMP);
    EXPECT_EQ(TypeMapping::fromSBWP(43u), DataType::INTERVAL);
    EXPECT_EQ(TypeMapping::fromSBWP(50u), DataType::BOOLEAN);
    EXPECT_EQ(TypeMapping::fromSBWP(60u), DataType::UUID);
    EXPECT_EQ(TypeMapping::fromSBWP(61u), DataType::JSON);
    EXPECT_EQ(TypeMapping::fromSBWP(62u), DataType::JSONB);
    EXPECT_EQ(TypeMapping::fromSBWP(63u), DataType::XML);
    EXPECT_EQ(TypeMapping::fromSBWP(64u), DataType::VECTOR);
    EXPECT_EQ(TypeMapping::fromSBWP(65u), DataType::POINT);
    EXPECT_EQ(TypeMapping::fromSBWP(66u), DataType::LINESTRING);
    EXPECT_EQ(TypeMapping::fromSBWP(67u), DataType::POLYGON);
    EXPECT_EQ(TypeMapping::fromSBWP(68u), DataType::MULTIPOINT);
    EXPECT_EQ(TypeMapping::fromSBWP(69u), DataType::MULTILINESTRING);
    EXPECT_EQ(TypeMapping::fromSBWP(70u), DataType::MULTIPOLYGON);
    EXPECT_EQ(TypeMapping::fromSBWP(71u), DataType::GEOMETRYCOLLECTION);
    EXPECT_EQ(TypeMapping::fromSBWP(72u), DataType::ARRAY);
    EXPECT_EQ(TypeMapping::fromSBWP(73u), DataType::COMPOSITE);
    EXPECT_EQ(TypeMapping::fromSBWP(74u), DataType::TSVECTOR);
    EXPECT_EQ(TypeMapping::fromSBWP(75u), DataType::TSQUERY);
    EXPECT_EQ(TypeMapping::fromSBWP(76u), DataType::INT4RANGE);
    EXPECT_EQ(TypeMapping::fromSBWP(77u), DataType::INT8RANGE);
    EXPECT_EQ(TypeMapping::fromSBWP(78u), DataType::NUMRANGE);
    EXPECT_EQ(TypeMapping::fromSBWP(79u), DataType::TSRANGE);
    EXPECT_EQ(TypeMapping::fromSBWP(80u), DataType::TSTZRANGE);
    EXPECT_EQ(TypeMapping::fromSBWP(81u), DataType::DATERANGE);
    EXPECT_EQ(TypeMapping::fromSBWP(86u), DataType::INET);
    EXPECT_EQ(TypeMapping::fromSBWP(87u), DataType::CIDR);
    EXPECT_EQ(TypeMapping::fromSBWP(88u), DataType::MACADDR);
    EXPECT_EQ(TypeMapping::fromSBWP(89u), DataType::MACADDR8);
    EXPECT_EQ(TypeMapping::fromSBWP(90u), DataType::VARIANT);
    EXPECT_EQ(TypeMapping::fromSBWP(255u), DataType::NULL_TYPE);
}

TEST_F(SBWPTypeMappingTest, FromSBWP_InvalidCode) {
    EXPECT_EQ(TypeMapping::fromSBWP(999u), DataType::UNKNOWN);  // Invalid code
    EXPECT_EQ(TypeMapping::fromSBWP(256u), DataType::UNKNOWN);  // Out of range
}

// SBWP Round-trip tests
TEST_F(SBWPTypeMappingTest, RoundTrip_AllTypes) {
    // Test round-trip for a subset of types
    std::vector<DataType> types = {
        DataType::BOOLEAN,
        DataType::INT8,
        DataType::INT16,
        DataType::INT32,
        DataType::INT64,
        DataType::INT128,
        DataType::UINT8,
        DataType::UINT16,
        DataType::UINT32,
        DataType::UINT64,
        DataType::FLOAT32,
        DataType::FLOAT64,
        DataType::DECIMAL,
        DataType::CHAR,
        DataType::VARCHAR,
        DataType::TEXT,
        DataType::BINARY,
        DataType::BLOB,
        DataType::DATE,
        DataType::TIME,
        DataType::TIMESTAMP,
        DataType::INTERVAL,
        DataType::UUID,
        DataType::JSON,
        DataType::JSONB,
        DataType::XML,
        DataType::POINT,
        DataType::ARRAY,
        DataType::COMPOSITE,
        DataType::INET,
        DataType::CIDR,
        DataType::MACADDR,
        DataType::VARIANT,
        DataType::NULL_TYPE
    };
    
    for (auto original : types) {
        auto sbwp_code = TypeMapping::toSBWP(original);
        auto roundtrip = TypeMapping::fromSBWP(sbwp_code);
        EXPECT_EQ(roundtrip, original) << "Round-trip failed for type code " << static_cast<uint32_t>(original);
    }
}

// =============================================================================
// Array Type Tests
// =============================================================================

class ArrayTypeMappingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test getArrayElementType
TEST_F(ArrayTypeMappingTest, GetArrayElementType_Int2Array) {
    EXPECT_EQ(TypeMapping::getArrayElementType(1005u), 21u);  // INT2ARRAY -> INT2
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_Int4Array) {
    EXPECT_EQ(TypeMapping::getArrayElementType(1007u), 23u);  // INT4ARRAY -> INT4
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_Int8Array) {
    EXPECT_EQ(TypeMapping::getArrayElementType(1016u), 20u);  // INT8ARRAY -> INT8
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_Float4Array) {
    EXPECT_EQ(TypeMapping::getArrayElementType(1021u), 700u);  // FLOAT4ARRAY -> FLOAT4
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_Float8Array) {
    EXPECT_EQ(TypeMapping::getArrayElementType(1022u), 701u);  // FLOAT8ARRAY -> FLOAT8
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_NumericArray) {
    EXPECT_EQ(TypeMapping::getArrayElementType(1231u), 1700u);  // NUMERICARRAY -> NUMERIC
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_TextArray) {
    EXPECT_EQ(TypeMapping::getArrayElementType(1009u), 25u);  // TEXTARRAY -> TEXT
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_ByteaArray) {
    EXPECT_EQ(TypeMapping::getArrayElementType(1001u), 17u);  // BYTEAARRAY -> BYTEA
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_BpCharArray) {
    EXPECT_EQ(TypeMapping::getArrayElementType(1014u), 1042u);  // BPCHARARRAY -> BPCHAR
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_VarCharArray) {
    EXPECT_EQ(TypeMapping::getArrayElementType(1015u), 1043u);  // VARCHARARRAY -> VARCHAR
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_UuidArray) {
    EXPECT_EQ(TypeMapping::getArrayElementType(2951u), 2950u);  // UUIDARRAY -> UUID
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_JsonbArray) {
    EXPECT_EQ(TypeMapping::getArrayElementType(3807u), 3802u);  // JSONBARRAY -> JSONB
}

TEST_F(ArrayTypeMappingTest, GetArrayElementType_Unknown) {
    EXPECT_EQ(TypeMapping::getArrayElementType(9999u), 0u);  // Unknown array type
}

// Test getArrayTypeOid
TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_Int2) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(21u), 1005u);  // INT2 -> INT2ARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_Int4) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(23u), 1007u);  // INT4 -> INT4ARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_Int8) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(20u), 1016u);  // INT8 -> INT8ARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_Float4) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(700u), 1021u);  // FLOAT4 -> FLOAT4ARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_Float8) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(701u), 1022u);  // FLOAT8 -> FLOAT8ARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_Numeric) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(1700u), 1231u);  // NUMERIC -> NUMERICARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_Text) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(25u), 1009u);  // TEXT -> TEXTARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_Bytea) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(17u), 1001u);  // BYTEA -> BYTEAARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_BpChar) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(1042u), 1014u);  // BPCHAR -> BPCHARARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_VarChar) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(1043u), 1015u);  // VARCHAR -> VARCHARARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_Uuid) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(2950u), 2951u);  // UUID -> UUIDARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_Jsonb) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(3802u), 3807u);  // JSONB -> JSONBARRAY
}

TEST_F(ArrayTypeMappingTest, GetArrayTypeOid_Unknown) {
    EXPECT_EQ(TypeMapping::getArrayTypeOid(9999u), 0u);  // Unknown element type
}

// Test isPostgreSQLArray
TEST_F(ArrayTypeMappingTest, IsPostgreSQLArray_True) {
    EXPECT_TRUE(TypeMapping::isPostgreSQLArray(1000u));   // First array OID
    EXPECT_TRUE(TypeMapping::isPostgreSQLArray(1005u));   // INT2ARRAY
    EXPECT_TRUE(TypeMapping::isPostgreSQLArray(1007u));   // INT4ARRAY
    EXPECT_TRUE(TypeMapping::isPostgreSQLArray(1009u));   // TEXTARRAY
    EXPECT_TRUE(TypeMapping::isPostgreSQLArray(1016u));   // INT8ARRAY
    EXPECT_TRUE(TypeMapping::isPostgreSQLArray(1021u));   // FLOAT4ARRAY
    EXPECT_TRUE(TypeMapping::isPostgreSQLArray(1022u));   // FLOAT8ARRAY
    EXPECT_TRUE(TypeMapping::isPostgreSQLArray(1231u));   // NUMERICARRAY
    EXPECT_TRUE(TypeMapping::isPostgreSQLArray(2951u));   // UUIDARRAY
    EXPECT_TRUE(TypeMapping::isPostgreSQLArray(3807u));   // JSONBARRAY
}

TEST_F(ArrayTypeMappingTest, IsPostgreSQLArray_False) {
    EXPECT_FALSE(TypeMapping::isPostgreSQLArray(16u));    // BOOL
    EXPECT_FALSE(TypeMapping::isPostgreSQLArray(23u));    // INT4
    EXPECT_FALSE(TypeMapping::isPostgreSQLArray(25u));    // TEXT
    EXPECT_FALSE(TypeMapping::isPostgreSQLArray(700u));   // FLOAT4
    EXPECT_FALSE(TypeMapping::isPostgreSQLArray(0u));     // Invalid
}

// Array round-trip tests
TEST_F(ArrayTypeMappingTest, ArrayRoundTrip_Int2) {
    auto element_oid = 21u;
    auto array_oid = TypeMapping::getArrayTypeOid(element_oid);
    auto roundtrip = TypeMapping::getArrayElementType(array_oid);
    EXPECT_EQ(roundtrip, element_oid);
}

TEST_F(ArrayTypeMappingTest, ArrayRoundTrip_Int4) {
    auto element_oid = 23u;
    auto array_oid = TypeMapping::getArrayTypeOid(element_oid);
    auto roundtrip = TypeMapping::getArrayElementType(array_oid);
    EXPECT_EQ(roundtrip, element_oid);
}

TEST_F(ArrayTypeMappingTest, ArrayRoundTrip_Text) {
    auto element_oid = 25u;
    auto array_oid = TypeMapping::getArrayTypeOid(element_oid);
    auto roundtrip = TypeMapping::getArrayElementType(array_oid);
    EXPECT_EQ(roundtrip, element_oid);
}

TEST_F(ArrayTypeMappingTest, ArrayRoundTrip_Numeric) {
    auto element_oid = 1700u;
    auto array_oid = TypeMapping::getArrayTypeOid(element_oid);
    auto roundtrip = TypeMapping::getArrayElementType(array_oid);
    EXPECT_EQ(roundtrip, element_oid);
}

// =============================================================================
// Type Information Tests
// =============================================================================

class TypeInformationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test getTypeName
TEST_F(TypeInformationTest, GetTypeName_AllTypes) {
    EXPECT_EQ(TypeMapping::getTypeName(DataType::UNKNOWN), "UNKNOWN");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::BOOLEAN), "BOOLEAN");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::TINYINT), "TINYINT");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::SMALLINT), "SMALLINT");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::MEDIUMINT), "MEDIUMINT");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::INTEGER), "INTEGER");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::BIGINT), "BIGINT");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::INT128), "INT128");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::DECFLOAT16), "DECFLOAT(16)");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::DECFLOAT34), "DECFLOAT(34)");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::REAL), "REAL");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::FLOAT), "REAL");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::DOUBLE), "DOUBLE");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::NUMERIC), "NUMERIC");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::DECIMAL), "NUMERIC");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::DATE), "DATE");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::TIME), "TIME");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::TIME_WITH_ZONE), "TIME WITH TIME ZONE");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::TIMESTAMP), "TIMESTAMP");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::TIMESTAMP_WITH_ZONE), "TIMESTAMP WITH TIME ZONE");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::INTERVAL), "INTERVAL");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::YEAR), "YEAR");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::CHAR), "CHAR");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::VARCHAR), "VARCHAR");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::TEXT), "TEXT");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::BINARY), "BINARY");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::VARBINARY), "VARBINARY");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::BLOB), "BLOB");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::TINYBLOB), "BLOB");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::MEDIUMBLOB), "BLOB");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::LONGBLOB), "BLOB");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::JSON), "JSON");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::XML), "XML");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::UUID), "UUID");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::INET), "INET");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::CIDR), "CIDR");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::MACADDR), "MACADDR");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::BIT), "BIT");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::GEOMETRY), "GEOMETRY");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::POINT), "POINT");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::LINESTRING), "LINESTRING");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::POLYGON), "POLYGON");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::MULTIPOINT), "MULTIPOINT");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::MULTILINESTRING), "MULTILINESTRING");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::MULTIPOLYGON), "MULTIPOLYGON");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::GEOMETRYCOLLECTION), "GEOMETRYCOLLECTION");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::ENUM), "ENUM");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::SET), "SET");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::ARRAY), "ARRAY");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::COMPOSITE), "COMPOSITE");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::DOMAIN), "DOMAIN");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::ROW), "ROW");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::NULL_TYPE), "NULL");
    EXPECT_EQ(TypeMapping::getTypeName(DataType::BLOB_SUB_TYPE_TEXT), "BLOB SUB_TYPE TEXT");
}

// Test getTypeSize for fixed-size types
TEST_F(TypeInformationTest, GetTypeSize_FixedSize) {
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::BOOLEAN), 1u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::TINYINT), 1u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::SMALLINT), 2u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::MEDIUMINT), 3u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::INTEGER), 4u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::FLOAT), 4u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::REAL), 4u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::BIGINT), 8u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::DOUBLE), 8u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::TIMESTAMP), 8u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::TIMESTAMP_WITH_ZONE), 8u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::TIME), 8u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::TIME_WITH_ZONE), 8u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::DATE), 4u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::INT128), 16u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::DECFLOAT16), 16u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::DECFLOAT34), 34u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::INTERVAL), 16u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::UUID), 16u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::INET), 19u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::CIDR), 19u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::MACADDR), 6u);
}

// Test getTypeSize for variable-length types
TEST_F(TypeInformationTest, GetTypeSize_VariableLength) {
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::VARCHAR), 0u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::TEXT), 0u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::BLOB), 0u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::JSON), 0u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::XML), 0u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::ARRAY), 0u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::COMPOSITE), 0u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::GEOMETRY), 0u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::POINT), 0u);
    EXPECT_EQ(TypeMapping::getTypeSize(DataType::UNKNOWN), 0u);
}

// Test isVariableLength
TEST_F(TypeInformationTest, IsVariableLength_True) {
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::VARCHAR));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::TEXT));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::BLOB));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::TINYBLOB));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::MEDIUMBLOB));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::LONGBLOB));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::JSON));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::XML));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::VARBINARY));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::ARRAY));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::COMPOSITE));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::GEOMETRY));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::POINT));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::LINESTRING));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::POLYGON));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::MULTIPOINT));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::MULTILINESTRING));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::MULTIPOLYGON));
    EXPECT_TRUE(TypeMapping::isVariableLength(DataType::GEOMETRYCOLLECTION));
}

TEST_F(TypeInformationTest, IsVariableLength_False) {
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::BOOLEAN));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::TINYINT));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::SMALLINT));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::INTEGER));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::BIGINT));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::FLOAT));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::DOUBLE));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::DATE));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::TIME));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::TIMESTAMP));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::UUID));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::INET));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::CHAR));
    EXPECT_FALSE(TypeMapping::isVariableLength(DataType::BINARY));
}

// Test isNumericType
TEST_F(TypeInformationTest, IsNumericType_True) {
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::TINYINT));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::SMALLINT));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::MEDIUMINT));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::INTEGER));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::BIGINT));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::INT128));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::REAL));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::FLOAT));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::DOUBLE));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::NUMERIC));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::DECIMAL));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::DECFLOAT16));
    EXPECT_TRUE(TypeMapping::isNumericType(DataType::DECFLOAT34));
}

TEST_F(TypeInformationTest, IsNumericType_False) {
    EXPECT_FALSE(TypeMapping::isNumericType(DataType::BOOLEAN));
    EXPECT_FALSE(TypeMapping::isNumericType(DataType::CHAR));
    EXPECT_FALSE(TypeMapping::isNumericType(DataType::VARCHAR));
    EXPECT_FALSE(TypeMapping::isNumericType(DataType::TEXT));
    EXPECT_FALSE(TypeMapping::isNumericType(DataType::DATE));
    EXPECT_FALSE(TypeMapping::isNumericType(DataType::TIME));
    EXPECT_FALSE(TypeMapping::isNumericType(DataType::TIMESTAMP));
    EXPECT_FALSE(TypeMapping::isNumericType(DataType::BLOB));
    EXPECT_FALSE(TypeMapping::isNumericType(DataType::JSON));
    EXPECT_FALSE(TypeMapping::isNumericType(DataType::UUID));
}

// Test isStringType
TEST_F(TypeInformationTest, IsStringType_True) {
    EXPECT_TRUE(TypeMapping::isStringType(DataType::CHAR));
    EXPECT_TRUE(TypeMapping::isStringType(DataType::VARCHAR));
    EXPECT_TRUE(TypeMapping::isStringType(DataType::TEXT));
    EXPECT_TRUE(TypeMapping::isStringType(DataType::JSON));
    EXPECT_TRUE(TypeMapping::isStringType(DataType::XML));
}

TEST_F(TypeInformationTest, IsStringType_False) {
    EXPECT_FALSE(TypeMapping::isStringType(DataType::BOOLEAN));
    EXPECT_FALSE(TypeMapping::isStringType(DataType::INTEGER));
    EXPECT_FALSE(TypeMapping::isStringType(DataType::BLOB));
    EXPECT_FALSE(TypeMapping::isStringType(DataType::BINARY));
    EXPECT_FALSE(TypeMapping::isStringType(DataType::DATE));
    EXPECT_FALSE(TypeMapping::isStringType(DataType::UUID));
    EXPECT_FALSE(TypeMapping::isStringType(DataType::ARRAY));
}

// Test isTemporalType
TEST_F(TypeInformationTest, IsTemporalType_True) {
    EXPECT_TRUE(TypeMapping::isTemporalType(DataType::DATE));
    EXPECT_TRUE(TypeMapping::isTemporalType(DataType::TIME));
    EXPECT_TRUE(TypeMapping::isTemporalType(DataType::TIME_WITH_ZONE));
    EXPECT_TRUE(TypeMapping::isTemporalType(DataType::TIMESTAMP));
    EXPECT_TRUE(TypeMapping::isTemporalType(DataType::TIMESTAMP_WITH_ZONE));
    EXPECT_TRUE(TypeMapping::isTemporalType(DataType::INTERVAL));
    EXPECT_TRUE(TypeMapping::isTemporalType(DataType::YEAR));
}

TEST_F(TypeInformationTest, IsTemporalType_False) {
    EXPECT_FALSE(TypeMapping::isTemporalType(DataType::INTEGER));
    EXPECT_FALSE(TypeMapping::isTemporalType(DataType::VARCHAR));
    EXPECT_FALSE(TypeMapping::isTemporalType(DataType::BLOB));
    EXPECT_FALSE(TypeMapping::isTemporalType(DataType::BOOLEAN));
    EXPECT_FALSE(TypeMapping::isTemporalType(DataType::UUID));
}

// Test isBinaryType
TEST_F(TypeInformationTest, IsBinaryType_True) {
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::BINARY));
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::VARBINARY));
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::BLOB));
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::TINYBLOB));
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::MEDIUMBLOB));
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::LONGBLOB));
}

TEST_F(TypeInformationTest, IsBinaryType_False) {
    EXPECT_FALSE(TypeMapping::isBinaryType(DataType::INTEGER));
    EXPECT_FALSE(TypeMapping::isBinaryType(DataType::CHAR));
    EXPECT_FALSE(TypeMapping::isBinaryType(DataType::VARCHAR));
    EXPECT_FALSE(TypeMapping::isBinaryType(DataType::TEXT));
    EXPECT_FALSE(TypeMapping::isBinaryType(DataType::DATE));
    EXPECT_FALSE(TypeMapping::isBinaryType(DataType::UUID));
}

// =============================================================================
// Edge Cases and Error Handling Tests
// =============================================================================

class TypeMappingEdgeCasesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test that all DataType values can be converted to each external format
TEST_F(TypeMappingEdgeCasesTest, AllTypesToPostgreSQL) {
    // Test that no crash occurs for any type
    for (uint16_t i = 0; i <= 255; ++i) {
        auto type = static_cast<DataType>(i);
        auto oid = TypeMapping::toPostgreSQL(type);
        EXPECT_GT(oid, 0u);  // Should return a valid OID
    }
}

TEST_F(TypeMappingEdgeCasesTest, AllTypesToMySQL) {
    for (uint16_t i = 0; i <= 255; ++i) {
        auto type = static_cast<DataType>(i);
        auto mysql_type = TypeMapping::toMySQL(type);
        EXPECT_LE(mysql_type, 255u);  // Valid uint8 range
    }
}

TEST_F(TypeMappingEdgeCasesTest, AllTypesToFirebird) {
    for (uint16_t i = 0; i <= 255; ++i) {
        auto type = static_cast<DataType>(i);
        auto blr_type = TypeMapping::toFirebird(type);
        if (type == DataType::NULL_TYPE) {
            EXPECT_EQ(blr_type, 0u);
        } else {
            EXPECT_GT(blr_type, 0u);
        }
    }
}

TEST_F(TypeMappingEdgeCasesTest, AllTypesToSBWP) {
    for (uint16_t i = 0; i <= 255; ++i) {
        auto type = static_cast<DataType>(i);
        auto sbwp_code = TypeMapping::toSBWP(type);
        EXPECT_LE(sbwp_code, 255u);
    }
}

TEST_F(TypeMappingEdgeCasesTest, AllTypesGetTypeName) {
    for (uint16_t i = 0; i <= 255; ++i) {
        auto type = static_cast<DataType>(i);
        auto name = TypeMapping::getTypeName(type);
        EXPECT_FALSE(name.empty());  // Should always return a name
    }
}

TEST_F(TypeMappingEdgeCasesTest, AllTypesGetTypeSize) {
    for (uint16_t i = 0; i <= 255; ++i) {
        auto type = static_cast<DataType>(i);
        auto size = TypeMapping::getTypeSize(type);
        // Size should be 0 (variable) or a reasonable fixed size
        EXPECT_TRUE(size == 0 || (size >= 1 && size <= 100));
    }
}

TEST_F(TypeMappingEdgeCasesTest, BoundaryOIDs) {
    // Test boundary OID values
    EXPECT_EQ(TypeMapping::fromPostgreSQL(0u), DataType::UNKNOWN);
    EXPECT_EQ(TypeMapping::fromPostgreSQL(0xFFFFFFFFu), DataType::UNKNOWN);
}

TEST_F(TypeMappingEdgeCasesTest, BoundaryBLRTypes) {
    // Test boundary BLR type values
    EXPECT_EQ(TypeMapping::fromFirebird(0u), DataType::UNKNOWN);
    EXPECT_EQ(TypeMapping::fromFirebird(0xFFFFFFFFu), DataType::UNKNOWN);
}

TEST_F(TypeMappingEdgeCasesTest, BoundaryMySQLTypes) {
    // Test boundary MySQL type values
    EXPECT_EQ(TypeMapping::fromMySQL(0u), DataType::NUMERIC);  // MYSQL_TYPE_DECIMAL
    EXPECT_EQ(TypeMapping::fromMySQL(255u), DataType::GEOMETRY);  // MYSQL_TYPE_GEOMETRY
}

// =============================================================================
// Comprehensive Cross-Database Consistency Tests
// =============================================================================

class CrossDatabaseConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test that numeric types map consistently across databases
TEST_F(CrossDatabaseConsistencyTest, NumericTypesMapping) {
    std::vector<std::tuple<DataType, uint32_t, uint8_t, uint32_t>> numericTypes = {
        {DataType::SMALLINT, 21u, 0x02u, 7u},
        {DataType::INTEGER, 23u, 0x03u, 8u},
        {DataType::BIGINT, 20u, 0x08u, 16u},
        {DataType::FLOAT, 700u, 0x04u, 10u},
        {DataType::DOUBLE, 701u, 0x05u, 27u},
        {DataType::NUMERIC, 1700u, 0xf6u, 16u},
    };
    
    for (const auto& [type, pg_oid, mysql_type, fb_blr] : numericTypes) {
        EXPECT_EQ(TypeMapping::toPostgreSQL(type), pg_oid)
            << "PostgreSQL mapping mismatch for type " << static_cast<int>(type);
        EXPECT_EQ(TypeMapping::toMySQL(type), mysql_type)
            << "MySQL mapping mismatch for type " << static_cast<int>(type);
        EXPECT_EQ(TypeMapping::toFirebird(type), fb_blr)
            << "Firebird mapping mismatch for type " << static_cast<int>(type);
        EXPECT_TRUE(TypeMapping::isNumericType(type))
            << "Type should be numeric: " << static_cast<int>(type);
    }
}

// Test that string types map consistently across databases
TEST_F(CrossDatabaseConsistencyTest, StringTypesMapping) {
    // Note: MySQL uses BLOB for TEXT, so we only check PostgreSQL and Firebird
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::VARCHAR), 1043u);
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::TEXT), 25u);
    EXPECT_EQ(TypeMapping::toFirebird(DataType::VARCHAR), 14u);
    EXPECT_EQ(TypeMapping::toFirebird(DataType::CHAR), 14u);
    
    EXPECT_TRUE(TypeMapping::isStringType(DataType::CHAR));
    EXPECT_TRUE(TypeMapping::isStringType(DataType::VARCHAR));
    EXPECT_TRUE(TypeMapping::isStringType(DataType::TEXT));
}

// Test that temporal types map consistently across databases
TEST_F(CrossDatabaseConsistencyTest, TemporalTypesMapping) {
    std::vector<std::tuple<DataType, uint32_t, uint8_t, uint32_t>> temporalTypes = {
        {DataType::DATE, 1082u, 0x0au, 12u},
        {DataType::TIME, 1083u, 0x0bu, 13u},
        {DataType::TIMESTAMP, 1114u, 0x07u, 35u},
    };
    
    for (const auto& [type, pg_oid, mysql_type, fb_blr] : temporalTypes) {
        EXPECT_EQ(TypeMapping::toPostgreSQL(type), pg_oid)
            << "PostgreSQL mapping mismatch for temporal type";
        EXPECT_EQ(TypeMapping::toMySQL(type), mysql_type)
            << "MySQL mapping mismatch for temporal type";
        EXPECT_EQ(TypeMapping::toFirebird(type), fb_blr)
            << "Firebird mapping mismatch for temporal type";
        EXPECT_TRUE(TypeMapping::isTemporalType(type))
            << "Type should be temporal";
    }
}

// Test that binary types map consistently across databases
TEST_F(CrossDatabaseConsistencyTest, BinaryTypesMapping) {
    // PostgreSQL: all binary types map to BYTEA (17)
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::BINARY), 17u);
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::VARBINARY), 17u);
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::BLOB), 17u);
    
    // MySQL: all binary types map to BLOB (0xfc)
    EXPECT_EQ(TypeMapping::toMySQL(DataType::BINARY), 0xfcu);
    EXPECT_EQ(TypeMapping::toMySQL(DataType::VARBINARY), 0xfcu);
    EXPECT_EQ(TypeMapping::toMySQL(DataType::BLOB), 0xfcu);
    
    // Firebird: all binary types map to BLOB (261)
    EXPECT_EQ(TypeMapping::toFirebird(DataType::BLOB), 261u);
    
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::BINARY));
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::VARBINARY));
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::BLOB));
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::TINYBLOB));
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::MEDIUMBLOB));
    EXPECT_TRUE(TypeMapping::isBinaryType(DataType::LONGBLOB));
}

// Test that special types map correctly
TEST_F(CrossDatabaseConsistencyTest, SpecialTypesMapping) {
    // UUID
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::UUID), 2950u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::UUID), 60u);
    
    // JSON
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::JSON), 3802u);  // JSONB
    EXPECT_EQ(TypeMapping::toMySQL(DataType::JSON), 0xf5u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::JSON), 61u);
    
    // XML
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::XML), 142u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::XML), 63u);
    
    // NULL
    EXPECT_EQ(TypeMapping::toPostgreSQL(DataType::NULL_TYPE), 705u);
    EXPECT_EQ(TypeMapping::toMySQL(DataType::NULL_TYPE), 0x06u);
    EXPECT_EQ(TypeMapping::toFirebird(DataType::NULL_TYPE), 0u);
    EXPECT_EQ(TypeMapping::toSBWP(DataType::NULL_TYPE), 255u);
}

// Main entry point for the test executable
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
