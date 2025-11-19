#include <gtest/gtest.h>
#include "scratchbird/core/types.h"
#include "scratchbird/core/array.h"

using namespace scratchbird::core;

class TypeConversionTest : public ::testing::Test {
protected:
    ErrorContext ctx;
};

// Test ARRAY to VARCHAR conversion (PostgreSQL format {1,2,3})
TEST_F(TypeConversionTest, ARRAY_ToString_PostgreSQL_Format) {
    // Create an ARRAY of INT32
    std::vector<int32_t> values = {1, 2, 3};
    std::vector<size_t> dimensions = {3};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);
    auto array_val = TypedValue::makeArray(arr);

    // Test toString() - should use PostgreSQL format
    std::string str = array_val.toString();
    EXPECT_EQ(str, "{1, 2, 3}");
}

// Test ARRAY to VARCHAR explicit conversion
TEST_F(TypeConversionTest, ARRAY_ToVarchar) {
    // Create an ARRAY of INT64
    std::vector<int64_t> values = {10, 20, 30};
    std::vector<size_t> dimensions = {3};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);
    auto array_val = TypedValue::makeArray(arr);

    // Convert to VARCHAR
    auto varchar_val = array_val.convertTo(DataType::VARCHAR, &ctx);
    ASSERT_TRUE(varchar_val.has_value());
    EXPECT_EQ(varchar_val->getVarchar(), "{10, 20, 30}");
}

// Test ARRAY to JSON conversion (JSON format [1,2,3])
TEST_F(TypeConversionTest, ARRAY_ToJSON) {
    // Create an ARRAY of FLOAT64
    std::vector<double> values = {1.5, 2.5, 3.5};
    std::vector<size_t> dimensions = {3};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);
    auto array_val = TypedValue::makeArray(arr);

    // Convert to JSON
    auto json_val = array_val.convertTo(DataType::JSON, &ctx);
    ASSERT_TRUE(json_val.has_value());
    EXPECT_EQ(json_val->getJSON(), "[1.5, 2.5, 3.5]");
}

// Test ARRAY with strings
TEST_F(TypeConversionTest, ARRAY_Strings_ToString) {
    // Create an ARRAY of strings
    std::vector<std::string> values = {"foo", "bar", "baz"};
    std::vector<size_t> dimensions = {3};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);
    auto array_val = TypedValue::makeArray(arr);

    // Test toString()
    std::string str = array_val.toString();
    EXPECT_EQ(str, "{\"foo\", \"bar\", \"baz\"}");
}

// Test 2D ARRAY conversion
TEST_F(TypeConversionTest, ARRAY_2D_ToString) {
    // Create a 2D ARRAY (2x3)
    std::vector<int32_t> values = {1, 2, 3, 4, 5, 6};
    std::vector<size_t> dimensions = {2, 3};
    auto arr = std::make_shared<ArrayValue>(values, dimensions);
    auto array_val = TypedValue::makeArray(arr);

    // Test toString()
    std::string str = array_val.toString();
    EXPECT_EQ(str, "{{1, 2, 3}, {4, 5, 6}}");
}

// Test NULL ARRAY
TEST_F(TypeConversionTest, ARRAY_Null_ToString) {
    auto null_val = TypedValue::makeNull();
    null_val = TypedValue::makeArray(nullptr);

    // toString() should handle NULL
    std::string str = null_val.toString();
    EXPECT_EQ(str, "NULL");
}

// Test MULTIPOINT to VARCHAR conversion
TEST_F(TypeConversionTest, MULTIPOINT_ToVarchar) {
    std::vector<Point> points = {{0, 0}, {1, 1}, {2, 2}};
    auto mp = TypedValue::makeMultiPoint(points);

    auto varchar_val = mp.convertTo(DataType::VARCHAR, &ctx);
    ASSERT_TRUE(varchar_val.has_value());
    EXPECT_EQ(varchar_val->getVarchar(), "MULTIPOINT((0 0), (1 1), (2 2))");
}

// Test MULTILINESTRING to VARCHAR conversion
TEST_F(TypeConversionTest, MULTILINESTRING_ToVarchar) {
    std::vector<Point> line1_points = {{0, 0}, {1, 1}};
    std::vector<Point> line2_points = {{2, 2}, {3, 3}};
    std::vector<LineString> linestrings = {
        LineString(line1_points),
        LineString(line2_points)
    };
    auto mls = TypedValue::makeMultiLineString(linestrings);

    auto varchar_val = mls.convertTo(DataType::VARCHAR, &ctx);
    ASSERT_TRUE(varchar_val.has_value());
    EXPECT_EQ(varchar_val->getVarchar(), "MULTILINESTRING((0 0, 1 1), (2 2, 3 3))");
}

// Test MULTIPOLYGON to VARCHAR conversion
TEST_F(TypeConversionTest, MULTIPOLYGON_ToVarchar) {
    std::vector<Point> ring1 = {{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
    std::vector<Point> ring2 = {{1, 1}, {2, 1}, {2, 2}, {1, 2}, {1, 1}};
    std::vector<Polygon> polygons = {
        Polygon({ring1}),
        Polygon({ring2})
    };
    auto mpoly = TypedValue::makeMultiPolygon(polygons);

    auto varchar_val = mpoly.convertTo(DataType::VARCHAR, &ctx);
    ASSERT_TRUE(varchar_val.has_value());
    // Check that it starts with MULTIPOLYGON and has proper structure
    std::string str = varchar_val->getVarchar();
    EXPECT_TRUE(str.find("MULTIPOLYGON") == 0);
    EXPECT_TRUE(str.find("((0 0, 4 0, 4 4, 0 4, 0 0))") != std::string::npos);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
