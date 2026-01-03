#include <gtest/gtest.h>

#include "scratchbird/core/database.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "test_helpers.h"

#include <memory>
#include <string>
#include <vector>

using namespace scratchbird::core;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

class SQLToBytecodeTest : public ::testing::Test
{
protected:
    std::unique_ptr<Database> db_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<TestDatabaseFile> db_file_;
    ID schema_id_;

    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("test_sql_to_bytecode");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        compiler_ = std::make_unique<QueryCompilerV2>(db_.get());
        executor_ = std::make_unique<Executor>(db_.get());

        createTables();
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        db_.reset();
        db_file_.reset();
    }

    void createTables()
    {
        const std::vector<std::string> ddl = {
            "CREATE TABLE users (id INT, name TEXT, email TEXT)",
            "CREATE TABLE products (price DOUBLE)",
            "CREATE TABLE orders (id BIGINT, total DOUBLE, status TEXT)",
            "CREATE TABLE locations (id INT)",
            "CREATE TABLE routes (id INT)",
            "CREATE TABLE regions (id INT)"
        };

        for (const auto& sql : ddl)
        {
            auto compile_result = compiler_->compile(sql);
            ASSERT_TRUE(compile_result.success()) << "Compile failed for: " << sql;
            auto exec_result = executor_->execute(compile_result.bytecode());
            ASSERT_TRUE(exec_result.success()) << "Execution failed for: " << sql
                                               << " error: " << exec_result.error();
        }
    }

    void testFullPipeline(const std::string& sql)
    {
        auto result = compiler_->compile(sql);
        ASSERT_TRUE(result.success()) << "Compile failed for: " << sql;

        const auto& bytecode = result.bytecode();
        EXPECT_GT(bytecode.size(), 3u);
        EXPECT_EQ(bytecode.front(), static_cast<uint8_t>(Opcode::VERSION));
        EXPECT_EQ(bytecode.back(), static_cast<uint8_t>(Opcode::END));
    }
};

TEST_F(SQLToBytecodeTest, CreateTable)
{
    testFullPipeline("CREATE TABLE users2 ("
                     "id INTEGER NOT NULL, "
                     "name VARCHAR(50), "
                     "email VARCHAR(100) NOT NULL"
                     ")");
}

TEST_F(SQLToBytecodeTest, Insert)
{
    testFullPipeline("INSERT INTO users (id, name, email) "
                     "VALUES (1, 'John Doe', 'john@example.com')");
}

TEST_F(SQLToBytecodeTest, Select)
{
    testFullPipeline("SELECT id, name FROM users WHERE id > 100");
}

TEST_F(SQLToBytecodeTest, ComplexExpression)
{
    testFullPipeline("SELECT price * 1.1 + 5 FROM products WHERE price >= 50.0");
}

// Example showing complete workflow
TEST_F(SQLToBytecodeTest, CompleteExample)
{
    std::vector<std::string> statements = {
        "CREATE TABLE orders2 (id BIGINT NOT NULL, total DOUBLE, status VARCHAR(20))",
        "INSERT INTO orders2 (id, total, status) VALUES (1001, 299.99, 'pending')",
        "SELECT * FROM orders2 WHERE total > 100.0"};

    const auto& create_sql = statements.front();
    auto create_result = compiler_->compile(create_sql);
    ASSERT_TRUE(create_result.success()) << "Compile failed for: " << create_sql;
    const auto& create_bytecode = create_result.bytecode();
    EXPECT_GT(create_bytecode.size(), 3u);
    EXPECT_EQ(create_bytecode.front(), static_cast<uint8_t>(Opcode::VERSION));
    EXPECT_EQ(create_bytecode.back(), static_cast<uint8_t>(Opcode::END));

    auto exec_result = executor_->execute(create_bytecode);
    ASSERT_TRUE(exec_result.success()) << "Execution failed for: " << create_sql
                                       << " error: " << exec_result.error();

    for (size_t i = 1; i < statements.size(); ++i)
    {
        testFullPipeline(statements[i]);
    }
}

// ===== Spatial SQL Integration Tests (Phase 2 Task 9.1) =====

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_Point)
{
    testFullPipeline("SELECT ST_POINT(1.5, 2.5) FROM locations");
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_AsText)
{
    testFullPipeline("SELECT ST_ASTEXT(ST_POINT(1.0, 2.0)) FROM locations");
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_GeometryType)
{
    testFullPipeline("SELECT ST_GEOMETRYTYPE(ST_POINT(0.0, 0.0)) FROM locations");
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_IsValid)
{
    testFullPipeline("SELECT ST_ISVALID(ST_POINT(1.0, 2.0)) FROM locations");
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_MakeLine)
{
    testFullPipeline("SELECT ST_MAKELINE(ST_POINT(0.0, 0.0), ST_POINT(1.0, 1.0)) FROM routes");
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_MakePolygon)
{
    testFullPipeline("SELECT ST_MAKEPOLYGON(ST_MAKELINE(ST_POINT(0.0, 0.0), ST_POINT(1.0, 0.0))) FROM regions");
}

TEST_F(SQLToBytecodeTest, SpatialFunction_ST_AsBinary)
{
    testFullPipeline("SELECT ST_ASBINARY(ST_POINT(5.5, 10.5)) FROM locations");
}

TEST_F(SQLToBytecodeTest, SpatialComplexQuery)
{
    testFullPipeline("SELECT ST_ASTEXT(ST_POINT(5.0, 10.0)) FROM locations");
}
