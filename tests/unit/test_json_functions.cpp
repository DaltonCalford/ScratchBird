#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/semantic_analyzer.h"
#include <string>

using namespace scratchbird::parser;

class JSONFunctionTest : public ::testing::Test
{
protected:
    void testParse(const std::string &sql, bool should_succeed = true)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        if (should_succeed)
        {
            ASSERT_TRUE(result.success()) << "Parse failed for: " << sql;
            if (!parser.hasErrors())
            {
                // Also test semantic analysis
                SemanticAnalyzer analyzer(parser.stringPool());
                result.statement()->accept(&analyzer);
                EXPECT_FALSE(analyzer.hasErrors()) << "Semantic analysis failed";
            }
        }
        else
        {
            EXPECT_FALSE(result.success()) << "Parse should have failed for: " << sql;
        }
    }
};

// ===== JSON Extraction Function Tests =====

TEST_F(JSONFunctionTest, JSON_EXTRACT_Basic)
{
    std::string sql = "SELECT JSON_EXTRACT(data, '$.name') FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSON_ARROW_Operator)
{
    std::string sql = "SELECT data->'name' FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSON_DOUBLE_ARROW_Operator)
{
    std::string sql = "SELECT data->>'name' FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSON_HASH_ARROW_Operator)
{
    std::string sql = "SELECT data #> ARRAY['address', 'city'] FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSON_HASH_DOUBLE_ARROW_Operator)
{
    std::string sql = "SELECT data #>> ARRAY['address', 'city'] FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSONB_EXTRACT_PATH_Basic)
{
    std::string sql = "SELECT JSONB_EXTRACT_PATH(data, 'address', 'city') FROM users";
    testParse(sql, true);
}

// ===== JSON Construction Function Tests =====

TEST_F(JSONFunctionTest, JSON_OBJECT_Basic)
{
    std::string sql = "SELECT JSON_OBJECT('name', 'John', 'age', 30) FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSON_ARRAY_Basic)
{
    std::string sql = "SELECT JSON_ARRAY(1, 2, 3, 4, 5) FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSONB_BUILD_OBJECT_Basic)
{
    std::string sql = "SELECT JSONB_BUILD_OBJECT('key', value, 'key2', value2) FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSONB_BUILD_ARRAY_Basic)
{
    std::string sql = "SELECT JSONB_BUILD_ARRAY(col1, col2, col3) FROM users";
    testParse(sql, true);
}

// ===== JSON Modification Function Tests =====

TEST_F(JSONFunctionTest, JSON_SET_Basic)
{
    std::string sql = "SELECT JSON_SET(data, '$.name', 'NewName') FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSON_INSERT_Basic)
{
    std::string sql = "SELECT JSON_INSERT(data, '$.email', 'test@example.com') FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSON_REMOVE_Basic)
{
    std::string sql = "SELECT JSON_REMOVE(data, '$.temp_field') FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSONB_SET_Basic)
{
    std::string sql = "SELECT JSONB_SET(data, ARRAY['address', 'city'], '\"NYC\"') FROM users";
    testParse(sql, true);
}

// ===== Complex Query Tests =====

TEST_F(JSONFunctionTest, MultipleJSONOperators)
{
    std::string sql = "SELECT data->'user'->>'name', data #>> ARRAY['address', 'zip'] FROM users";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSONInWHERE)
{
    std::string sql = "SELECT * FROM users WHERE data->>'status' = 'active'";
    testParse(sql, true);
}

TEST_F(JSONFunctionTest, JSONWithAggregates)
{
    std::string sql = "SELECT COUNT(*), data->>'department' FROM users GROUP BY data->>'department'";
    testParse(sql, true);
}

// ===== Error Tests =====

TEST_F(JSONFunctionTest, JSON_OBJECT_OddArgs)
{
    // JSON_OBJECT requires even number of arguments
    std::string sql = "SELECT JSON_OBJECT('key1', 'value1', 'key2') FROM users";
    testParse(sql, false);
}

TEST_F(JSONFunctionTest, JSON_EXTRACT_WrongArgCount)
{
    // JSON_EXTRACT requires exactly 2 arguments
    std::string sql = "SELECT JSON_EXTRACT(data) FROM users";
    testParse(sql, false);
}
