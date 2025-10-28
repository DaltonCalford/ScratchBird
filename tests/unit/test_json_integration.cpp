#include <gtest/gtest.h>
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/ast.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include <memory>

using namespace scratchbird;
using namespace scratchbird::sblr;
using namespace scratchbird::parser;

class JSONIntegrationTest : public ::testing::Test
{
protected:
    std::unique_ptr<core::Database> db_;
    std::unique_ptr<Executor> executor_;

    void SetUp() override
    {
        // Create a test database
        db_ = std::make_unique<core::Database>("test_json.db");
        executor_ = std::make_unique<Executor>(db_.get());
    }

    void TearDown() override
    {
        executor_.reset();
        db_.reset();
        // Clean up test database file
        std::remove("test_json.db");
    }

    // Helper to execute SQL and return result
    ExecutionResult executeSQL(const std::string& sql)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);
        auto parse_result = parser.parseStatement();

        if (!parse_result.success()) {
            return ExecutionResult("Parse error");
        }

        BytecodeGenerator generator(db_.get());
        auto bytecode = generator.generate(parse_result.statement());

        return executor_->execute(bytecode);
    }
};

// JSON Extraction Tests

TEST_F(JSONIntegrationTest, JSONExtractSimpleField)
{
    // Test JSON_EXTRACT with simple field
    std::string sql = "SELECT JSON_EXTRACT('{\"name\":\"John\",\"age\":30}', '$.name')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());
    // Should return "John" as JSON
    EXPECT_EQ(value.toString(), "\"John\"");
}

TEST_F(JSONIntegrationTest, JSONExtractNestedField)
{
    // Test nested field extraction
    std::string sql = "SELECT JSON_EXTRACT('{\"user\":{\"name\":\"Alice\",\"age\":25}}', '$.user.name')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());
    EXPECT_EQ(value.toString(), "\"Alice\"");
}

TEST_F(JSONIntegrationTest, JSONExtractArrayElement)
{
    // Test array index extraction
    std::string sql = "SELECT JSON_EXTRACT('{\"scores\":[95,87,92]}', '$.scores[1]')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());
    EXPECT_EQ(value.toString(), "87");
}

TEST_F(JSONIntegrationTest, JSONExtractDeepPath)
{
    // Test deeply nested path
    std::string sql = "SELECT JSON_EXTRACT('{\"data\":{\"user\":{\"profile\":{\"city\":\"NYC\"}}}}', '$.data.user.profile.city')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());
    EXPECT_EQ(value.toString(), "\"NYC\"");
}

TEST_F(JSONIntegrationTest, JSONExtractNonexistentPath)
{
    // Test nonexistent path returns null
    std::string sql = "SELECT JSON_EXTRACT('{\"name\":\"John\"}', '$.nonexistent')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_TRUE(value.isNull());
}

TEST_F(JSONIntegrationTest, JSONArrowOperator)
{
    // Test -> operator (returns JSON)
    std::string sql = "SELECT '{\"name\":\"Bob\",\"age\":35}' -> 'name'";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());
    EXPECT_EQ(value.type(), core::DataType::JSON);
}

TEST_F(JSONIntegrationTest, JSONDoubleArrowOperator)
{
    // Test ->> operator (returns text)
    std::string sql = "SELECT '{\"name\":\"Charlie\"}' ->> 'name'";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());
    EXPECT_EQ(value.type(), core::DataType::TEXT);
}

// JSON Construction Tests

TEST_F(JSONIntegrationTest, JSONObjectConstruction)
{
    // Test JSON_OBJECT
    std::string sql = "SELECT JSON_OBJECT('name', 'David', 'age', 40)";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());

    // Parse and verify JSON
    std::string json_str = value.toString();
    EXPECT_TRUE(json_str.find("\"name\"") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"David\"") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"age\"") != std::string::npos);
    EXPECT_TRUE(json_str.find("40") != std::string::npos);
}

TEST_F(JSONIntegrationTest, JSONArrayConstruction)
{
    // Test JSON_ARRAY
    std::string sql = "SELECT JSON_ARRAY(1, 2, 3, 4, 5)";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());

    std::string json_str = value.toString();
    EXPECT_EQ(json_str, "[1,2,3,4,5]");
}

TEST_F(JSONIntegrationTest, JSONArrayMixedTypes)
{
    // Test JSON_ARRAY with mixed types
    std::string sql = "SELECT JSON_ARRAY('text', 42, 3.14, NULL)";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());

    std::string json_str = value.toString();
    EXPECT_TRUE(json_str.find("\"text\"") != std::string::npos);
    EXPECT_TRUE(json_str.find("42") != std::string::npos);
    EXPECT_TRUE(json_str.find("3.14") != std::string::npos);
    EXPECT_TRUE(json_str.find("null") != std::string::npos);
}

// JSON Modification Tests

TEST_F(JSONIntegrationTest, JSONSet)
{
    // Test JSON_SET to modify existing value
    std::string sql = "SELECT JSON_SET('{\"name\":\"Eve\",\"age\":28}', '$.age', 29)";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());

    std::string json_str = value.toString();
    EXPECT_TRUE(json_str.find("\"age\":29") != std::string::npos);
}

TEST_F(JSONIntegrationTest, JSONSetNewField)
{
    // Test JSON_SET to add new field
    std::string sql = "SELECT JSON_SET('{\"name\":\"Frank\"}', '$.email', 'frank@test.com')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());

    std::string json_str = value.toString();
    EXPECT_TRUE(json_str.find("\"email\"") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"frank@test.com\"") != std::string::npos);
}

TEST_F(JSONIntegrationTest, JSONInsertNewOnly)
{
    // Test JSON_INSERT only inserts if path doesn't exist
    std::string sql = "SELECT JSON_INSERT('{\"name\":\"Grace\"}', '$.email', 'grace@test.com')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());

    std::string json_str = value.toString();
    EXPECT_TRUE(json_str.find("\"email\"") != std::string::npos);
}

TEST_F(JSONIntegrationTest, JSONInsertExistingSkipped)
{
    // Test JSON_INSERT doesn't overwrite existing
    std::string sql = "SELECT JSON_INSERT('{\"name\":\"Helen\",\"age\":30}', '$.age', 25)";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());

    std::string json_str = value.toString();
    // Age should still be 30, not 25
    EXPECT_TRUE(json_str.find("\"age\":30") != std::string::npos);
}

TEST_F(JSONIntegrationTest, JSONRemove)
{
    // Test JSON_REMOVE
    std::string sql = "SELECT JSON_REMOVE('{\"name\":\"Ivy\",\"age\":32,\"city\":\"LA\"}', '$.age')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());

    std::string json_str = value.toString();
    // age should be removed
    EXPECT_TRUE(json_str.find("\"age\"") == std::string::npos);
    EXPECT_TRUE(json_str.find("\"name\"") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"city\"") != std::string::npos);
}

TEST_F(JSONIntegrationTest, JSONRemoveArrayElement)
{
    // Test JSON_REMOVE with array
    std::string sql = "SELECT JSON_REMOVE('{\"scores\":[95,87,92]}', '$.scores[1]')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());

    std::string json_str = value.toString();
    // Should have 2 elements now (95 and 92)
    EXPECT_TRUE(json_str.find("95") != std::string::npos);
    EXPECT_TRUE(json_str.find("92") != std::string::npos);
}

// Complex Real-World Scenarios

TEST_F(JSONIntegrationTest, ComplexNestedJSON)
{
    // Test complex nested JSON operations
    std::string json = R"({
        "user": {
            "id": 123,
            "profile": {
                "name": "John Doe",
                "email": "john@example.com",
                "preferences": {
                    "theme": "dark",
                    "notifications": true
                }
            },
            "orders": [
                {"id": 1, "total": 99.99},
                {"id": 2, "total": 149.50},
                {"id": 3, "total": 75.00}
            ]
        }
    })";

    // Extract nested preference
    std::string sql = "SELECT JSON_EXTRACT('" + json + "', '$.user.profile.preferences.theme')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());
    EXPECT_EQ(value.toString(), "\"dark\"");
}

TEST_F(JSONIntegrationTest, JSONModificationChain)
{
    // Test chaining JSON modifications
    std::string json = "{\"name\":\"Test\",\"version\":1}";

    // Add new field, then modify it
    std::string sql = "SELECT JSON_SET(JSON_SET('" + json + "', '$.status', 'active'), '$.version', 2)";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_FALSE(value.isNull());

    std::string json_str = value.toString();
    EXPECT_TRUE(json_str.find("\"status\":\"active\"") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"version\":2") != std::string::npos);
}

TEST_F(JSONIntegrationTest, InvalidJSONHandling)
{
    // Test that invalid JSON returns NULL
    std::string sql = "SELECT JSON_EXTRACT('{invalid json}', '$.field')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_TRUE(value.isNull());
}

TEST_F(JSONIntegrationTest, NullInputHandling)
{
    // Test NULL JSON data
    std::string sql = "SELECT JSON_EXTRACT(NULL, '$.field')";
    auto result = executeSQL(sql);

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.hasResultSet());

    auto rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1);

    auto value = rs->getValue(0, 0);
    EXPECT_TRUE(value.isNull());
}
