#include <gtest/gtest.h>

#include "scratchbird/parser/parser_v2.h"

#include <string>
#include <vector>

namespace {

bool parseOk(const std::string& sql)
{
    scratchbird::parser::v2::Parser parser(sql);
    auto result = parser.parseStatement();
    return result.success();
}

} // namespace

// ===== JSON Extraction Function Tests =====

TEST(JSONFunctionTest, JSON_EXTRACT_Basic)
{
    ASSERT_TRUE(parseOk("SELECT JSON_EXTRACT(data, '$.name') FROM users"));
}

TEST(JSONFunctionTest, JSON_ARROW_Operator)
{
    ASSERT_TRUE(parseOk("SELECT data->'name' FROM users"));
}

TEST(JSONFunctionTest, JSON_DOUBLE_ARROW_Operator)
{
    ASSERT_TRUE(parseOk("SELECT data->>'name' FROM users"));
}

TEST(JSONFunctionTest, JSON_HASH_ARROW_Operator)
{
    ASSERT_TRUE(parseOk("SELECT data #> ARRAY['address', 'city'] FROM users"));
}

TEST(JSONFunctionTest, JSON_HASH_DOUBLE_ARROW_Operator)
{
    ASSERT_TRUE(parseOk("SELECT data #>> ARRAY['address', 'city'] FROM users"));
}

TEST(JSONFunctionTest, JSONB_EXTRACT_PATH_Basic)
{
    ASSERT_TRUE(parseOk("SELECT JSONB_EXTRACT_PATH(data, 'address', 'city') FROM users"));
}

// ===== JSON Construction Function Tests =====

TEST(JSONFunctionTest, JSON_OBJECT_Basic)
{
    ASSERT_TRUE(parseOk("SELECT JSON_OBJECT('name', 'John', 'age', 30) FROM users"));
}

TEST(JSONFunctionTest, JSON_ARRAY_Basic)
{
    ASSERT_TRUE(parseOk("SELECT JSON_ARRAY(1, 2, 3, 4, 5) FROM users"));
}

TEST(JSONFunctionTest, JSONB_BUILD_OBJECT_Basic)
{
    ASSERT_TRUE(parseOk("SELECT JSONB_BUILD_OBJECT('key', value, 'key2', value2) FROM users"));
}

TEST(JSONFunctionTest, JSONB_BUILD_ARRAY_Basic)
{
    ASSERT_TRUE(parseOk("SELECT JSONB_BUILD_ARRAY(col1, col2, col3) FROM users"));
}

// ===== JSON Modification Function Tests =====

TEST(JSONFunctionTest, JSON_SET_Basic)
{
    ASSERT_TRUE(parseOk("SELECT JSON_SET(data, '$.name', 'NewName') FROM users"));
}

TEST(JSONFunctionTest, JSON_INSERT_Basic)
{
    ASSERT_TRUE(parseOk("SELECT JSON_INSERT(data, '$.email', 'test@example.com') FROM users"));
}

TEST(JSONFunctionTest, JSON_REMOVE_Basic)
{
    ASSERT_TRUE(parseOk("SELECT JSON_REMOVE(data, '$.temp_field') FROM users"));
}

TEST(JSONFunctionTest, JSONB_SET_Basic)
{
    ASSERT_TRUE(parseOk("SELECT JSONB_SET(data, ARRAY['address', 'city'], '\"NYC\"') FROM users"));
}

// ===== Complex Query Tests =====

TEST(JSONFunctionTest, MultipleJSONOperators)
{
    ASSERT_TRUE(parseOk(
        "SELECT data->'user'->>'name', data #>> ARRAY['address', 'zip'] FROM users"
    ));
}

TEST(JSONFunctionTest, JSONInWHERE)
{
    ASSERT_TRUE(parseOk("SELECT * FROM users WHERE data->>'status' = 'active'"));
}

TEST(JSONFunctionTest, JSONWithAggregates)
{
    ASSERT_TRUE(parseOk("SELECT COUNT(*), data->>'department' FROM users GROUP BY data->>'department'"));
}

// ===== Error Tests =====

TEST(JSONFunctionTest, JSON_OBJECT_OddArgs)
{
    EXPECT_FALSE(parseOk("SELECT JSON_OBJECT('name', 'John', 'age') FROM users"));
}

TEST(JSONFunctionTest, JSON_EXTRACT_WrongArgCount)
{
    EXPECT_FALSE(parseOk("SELECT JSON_EXTRACT(data) FROM users"));
}
