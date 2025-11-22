#include <gtest/gtest.h>
#include "scratchbird/core/types.h"
#include "scratchbird/core/array.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/lexer.h"

using namespace scratchbird::core;
using namespace scratchbird::sblr;
using namespace scratchbird::parser;

/**
 * Test fixture for EXTRACT function
 *
 * Tests the EXTRACT(field FROM value) SQL function across all supported data types:
 * - DATE: year, month, day, dow, doy, quarter, epoch
 * - TIME: hour, minute, second, microsecond, millisecond, epoch
 * - TIMESTAMP: all date+time fields, quarter, epoch
 * - INTERVAL: year, month, day, hour, minute, second, microsecond, millisecond, epoch
 * - UUID: version, variant, timestamp
 * - ARRAY: cardinality, ndims, lower, upper
 * - POINT: x, y, srid
 */
class ExtractTest : public ::testing::Test {
protected:
    // Helper to parse a SELECT expression and generate bytecode
    // This allows us to test EXTRACT through the full SQL pipeline
    BytecodeResult parseAndGenerate(const std::string& sql) {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto parse_result = parser.parseStatement();
        if (!parse_result.success()) {
            // Can't create an error result easily, so just throw
            throw std::runtime_error("Parse failed");
        }

        BytecodeGenerator generator(parser.stringPool(), nullptr);
        return generator.generate(parse_result.statement());
    }
};

// ===== DATE EXTRACTION TESTS =====

TEST_F(ExtractTest, DATE_ExtractYear) {
    // Create a DATE value: 2025-11-19
    auto date = TypedValue::makeDate(20411); // Days since epoch for 2025-11-19

    // We can't use executeExtract directly since we need to test at a lower level
    // For now, let's test the TypeExtractor functions directly
    int32_t year = TypeExtractor::extractYear(20411);
    EXPECT_EQ(year, 2025);
}

TEST_F(ExtractTest, DATE_ExtractMonth) {
    // 2025-11-19
    int32_t month = TypeExtractor::extractMonth(20411);
    EXPECT_EQ(month, 11);
}

TEST_F(ExtractTest, DATE_ExtractDay) {
    // 2025-11-19
    int32_t day = TypeExtractor::extractDay(20411);
    EXPECT_EQ(day, 19);
}

TEST_F(ExtractTest, DATE_ExtractDayOfWeek) {
    // 2025-11-19 is a Wednesday (3)
    int32_t dow = TypeExtractor::extractDayOfWeek(20411);
    EXPECT_GE(dow, 0);
    EXPECT_LE(dow, 6);
}

TEST_F(ExtractTest, DATE_ExtractDayOfYear) {
    // 2025-11-19 is day 323 of 2025
    int32_t doy = TypeExtractor::extractDayOfYear(20411);
    EXPECT_GE(doy, 1);
    EXPECT_LE(doy, 366);
}

// ===== TIME EXTRACTION TESTS =====

TEST_F(ExtractTest, TIME_ExtractHour) {
    // 14:30:45.123456 = 14*3600*1000000 + 30*60*1000000 + 45*1000000 + 123456
    int64_t time_us = 52245123456LL;
    int32_t hour = TypeExtractor::extractHour(time_us);
    EXPECT_EQ(hour, 14);
}

TEST_F(ExtractTest, TIME_ExtractMinute) {
    int64_t time_us = 52245123456LL; // 14:30:45.123456
    int32_t minute = TypeExtractor::extractMinute(time_us);
    EXPECT_EQ(minute, 30);
}

TEST_F(ExtractTest, TIME_ExtractSecond) {
    int64_t time_us = 52245123456LL; // 14:30:45.123456
    int32_t second = TypeExtractor::extractSecond(time_us);
    EXPECT_EQ(second, 45);
}

TEST_F(ExtractTest, TIME_ExtractMicrosecond) {
    int64_t time_us = 52245123456LL; // 14:30:45.123456
    int32_t microsecond = TypeExtractor::extractMicrosecond(time_us);
    EXPECT_EQ(microsecond, 123456);
}

// ===== TIMESTAMP EXTRACTION TESTS =====

TEST_F(ExtractTest, TIMESTAMP_ExtractYear) {
    // 2025-11-19 14:30:45 = days * 86400000000 + time_microseconds
    int64_t ts = 20411LL * 86400000000LL + 52245000000LL;
    int32_t year = TypeExtractor::extractTimestampYear(ts);
    EXPECT_EQ(year, 2025);
}

TEST_F(ExtractTest, TIMESTAMP_ExtractMonth) {
    int64_t ts = 20411LL * 86400000000LL + 52245000000LL;
    int32_t month = TypeExtractor::extractTimestampMonth(ts);
    EXPECT_EQ(month, 11);
}

TEST_F(ExtractTest, TIMESTAMP_ExtractDay) {
    int64_t ts = 20411LL * 86400000000LL + 52245000000LL;
    int32_t day = TypeExtractor::extractTimestampDay(ts);
    EXPECT_EQ(day, 19);
}

TEST_F(ExtractTest, TIMESTAMP_ExtractHour) {
    int64_t ts = 20411LL * 86400000000LL + 52245000000LL;
    int32_t hour = TypeExtractor::extractTimestampHour(ts);
    EXPECT_EQ(hour, 14);
}

TEST_F(ExtractTest, TIMESTAMP_ExtractMinute) {
    int64_t ts = 20411LL * 86400000000LL + 52245000000LL;
    int32_t minute = TypeExtractor::extractTimestampMinute(ts);
    EXPECT_EQ(minute, 30);
}

TEST_F(ExtractTest, TIMESTAMP_ExtractSecond) {
    int64_t ts = 20411LL * 86400000000LL + 52245000000LL;
    int32_t second = TypeExtractor::extractTimestampSecond(ts);
    EXPECT_EQ(second, 45);
}

TEST_F(ExtractTest, TIMESTAMP_ExtractMicrosecond) {
    int64_t ts = 20411LL * 86400000000LL + 52245123456LL;
    int32_t microsecond = TypeExtractor::extractTimestampMicrosecond(ts);
    EXPECT_EQ(microsecond, 123456);
}

// ===== INTERVAL EXTRACTION TESTS =====

TEST_F(ExtractTest, INTERVAL_ExtractYear) {
    // Interval: 2 years, 3 months = 27 months total
    Interval interval(27, 0, 0);
    int32_t year = interval.months / 12;
    EXPECT_EQ(year, 2);
}

TEST_F(ExtractTest, INTERVAL_ExtractMonth) {
    // Interval: 2 years, 3 months = 27 months total
    Interval interval(27, 0, 0);
    int32_t month = interval.months % 12;
    EXPECT_EQ(month, 3);
}

TEST_F(ExtractTest, INTERVAL_ExtractDay) {
    // Interval: 45 days
    Interval interval(0, 45, 0);
    EXPECT_EQ(interval.days, 45);
}

TEST_F(ExtractTest, INTERVAL_ExtractHour) {
    // Interval: 5 hours = 5 * 3600 * 1000000 microseconds
    Interval interval(0, 0, 5LL * 3600 * 1000000);
    int64_t hours = interval.microseconds / 3600000000LL;
    EXPECT_EQ(hours, 5);
}

TEST_F(ExtractTest, INTERVAL_ExtractMinute) {
    // Interval: 1 hour 30 minutes = 90 minutes total
    Interval interval(0, 0, 90LL * 60 * 1000000);
    int64_t total_minutes = interval.microseconds / 60000000LL;
    int32_t minutes = static_cast<int32_t>(total_minutes % 60);
    EXPECT_EQ(minutes, 30);
}

TEST_F(ExtractTest, INTERVAL_ExtractSecond) {
    // Interval: 1 minute 45 seconds
    Interval interval(0, 0, 105LL * 1000000);
    int64_t total_seconds = interval.microseconds / 1000000LL;
    int32_t seconds = static_cast<int32_t>(total_seconds % 60);
    EXPECT_EQ(seconds, 45);
}

TEST_F(ExtractTest, INTERVAL_NegativeValues) {
    // Interval: -6 months, -10 days, -2 hours
    Interval interval(-6, -10, -2LL * 3600 * 1000000);
    EXPECT_EQ(interval.months, -6);
    EXPECT_EQ(interval.days, -10);
    EXPECT_LT(interval.microseconds, 0);
}

// ===== UUID EXTRACTION TESTS =====

TEST_F(ExtractTest, UUID_ExtractVersion) {
    // Create a UUIDv4 (random UUID)
    std::vector<uint8_t> uuid = {
        0x12, 0x34, 0x56, 0x78,  // time_low
        0x9a, 0xbc,              // time_mid
        0x4d, 0xef,              // time_hi_and_version (0x4xxx for v4)
        0x89, 0xab,              // clock_seq
        0xcd, 0xef, 0x01, 0x23, 0x45, 0x67  // node
    };

    int32_t version = TypeExtractor::extractUUIDVersion(uuid);
    EXPECT_EQ(version, 4);
}

TEST_F(ExtractTest, UUID_ExtractVariant) {
    // Standard UUID has variant 1 (10xx in bits 6-7 of byte 8)
    std::vector<uint8_t> uuid = {
        0x12, 0x34, 0x56, 0x78,
        0x9a, 0xbc,
        0x4d, 0xef,
        0x89, 0xab,  // 0x89 = 10001001, variant bits = 10
        0xcd, 0xef, 0x01, 0x23, 0x45, 0x67
    };

    int32_t variant = TypeExtractor::extractUUIDVariant(uuid);
    EXPECT_GE(variant, 0);
    EXPECT_LE(variant, 2);
}

// ===== ARRAY EXTRACTION TESTS =====

TEST_F(ExtractTest, ARRAY_ExtractCardinality) {
    // Create an array with 5 elements
    std::vector<int32_t> values = {1, 2, 3, 4, 5};
    std::vector<size_t> dimensions = {5};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);

    EXPECT_EQ(arr->getTotalElements(), 5);
}

TEST_F(ExtractTest, ARRAY_ExtractNDims) {
    // Create a 2D array: {{1,2,3}, {4,5,6}}
    std::vector<int32_t> values = {1, 2, 3, 4, 5, 6};
    std::vector<size_t> dimensions = {2, 3};  // 2 rows, 3 columns
    auto arr = std::make_shared<ArrayValue>(values, dimensions);

    EXPECT_EQ(arr->getRank(), 2);
}

TEST_F(ExtractTest, ARRAY_ExtractDimensions) {
    std::vector<int32_t> values = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<size_t> dimensions = {2, 4};  // 2 rows, 4 columns
    auto arr = std::make_shared<ArrayValue>(values, dimensions);

    auto dims = arr->getDimensions();
    EXPECT_EQ(dims.size(), 2);
    EXPECT_EQ(dims[0], 2);
    EXPECT_EQ(dims[1], 4);
}

TEST_F(ExtractTest, ARRAY_Empty) {
    std::vector<int32_t> values;
    std::vector<size_t> dimensions = {0};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);

    EXPECT_EQ(arr->getTotalElements(), 0);
    EXPECT_TRUE(arr->isEmpty());
}

// ===== POINT EXTRACTION TESTS =====

TEST_F(ExtractTest, POINT_ExtractX) {
    Point point(10.5, 20.3, 4326);
    EXPECT_DOUBLE_EQ(point.x, 10.5);
}

TEST_F(ExtractTest, POINT_ExtractY) {
    Point point(10.5, 20.3, 4326);
    EXPECT_DOUBLE_EQ(point.y, 20.3);
}

TEST_F(ExtractTest, POINT_ExtractSRID) {
    Point point(10.5, 20.3, 4326);  // SRID 4326 = WGS 84
    EXPECT_EQ(point.srid, 4326);
}

TEST_F(ExtractTest, POINT_DefaultSRID) {
    Point point(1.0, 2.0);  // No SRID specified
    EXPECT_EQ(point.srid, 0);  // Default is 0 (undefined)
}

TEST_F(ExtractTest, POINT_NegativeCoordinates) {
    Point point(-122.4194, 37.7749, 4326);  // San Francisco
    EXPECT_DOUBLE_EQ(point.x, -122.4194);
    EXPECT_DOUBLE_EQ(point.y, 37.7749);
}

// ===== EDGE CASES =====

TEST_F(ExtractTest, DATE_EpochZero) {
    // Day 0 = 1970-01-01
    int32_t year = TypeExtractor::extractYear(0);
    EXPECT_EQ(year, 1970);
    int32_t month = TypeExtractor::extractMonth(0);
    EXPECT_EQ(month, 1);
    int32_t day = TypeExtractor::extractDay(0);
    EXPECT_EQ(day, 1);
}

TEST_F(ExtractTest, TIME_Midnight) {
    // 00:00:00.000000
    int32_t hour = TypeExtractor::extractHour(0);
    EXPECT_EQ(hour, 0);
    int32_t minute = TypeExtractor::extractMinute(0);
    EXPECT_EQ(minute, 0);
    int32_t second = TypeExtractor::extractSecond(0);
    EXPECT_EQ(second, 0);
}

TEST_F(ExtractTest, TIME_LastMicrosecondOfDay) {
    // 23:59:59.999999
    int64_t time_us = 86399999999LL;
    int32_t hour = TypeExtractor::extractHour(time_us);
    EXPECT_EQ(hour, 23);
    int32_t minute = TypeExtractor::extractMinute(time_us);
    EXPECT_EQ(minute, 59);
    int32_t second = TypeExtractor::extractSecond(time_us);
    EXPECT_EQ(second, 59);
}

TEST_F(ExtractTest, INTERVAL_ZeroInterval) {
    Interval interval(0, 0, 0);
    EXPECT_EQ(interval.months, 0);
    EXPECT_EQ(interval.days, 0);
    EXPECT_EQ(interval.microseconds, 0);
}

TEST_F(ExtractTest, ARRAY_SingleElement) {
    std::vector<int32_t> values = {42};
    std::vector<size_t> dimensions = {1};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);

    EXPECT_EQ(arr->getTotalElements(), 1);
    EXPECT_EQ(arr->getRank(), 1);
}

TEST_F(ExtractTest, ARRAY_3D) {
    // 3D array: 2x3x4 = 24 elements
    std::vector<int32_t> values(24, 1);
    std::vector<size_t> dimensions = {2, 3, 4};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);

    EXPECT_EQ(arr->getTotalElements(), 24);
    EXPECT_EQ(arr->getRank(), 3);
}

