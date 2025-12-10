#include <gtest/gtest.h>


#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include <memory>

using namespace scratchbird;
using namespace scratchbird::parser;
using namespace scratchbird::sblr;
using namespace scratchbird::core;

class TriggerTest : public ::testing::Test
{
protected:
    std::unique_ptr<Database> db;
    StringPool string_pool;

    void SetUp() override
    {
        // Create in-memory database
        db = std::make_unique<Database>("test_triggers.db", 1024 * 1024); // 1MB
    }

    void TearDown() override
    {
        db.reset();
        // Clean up test database file
        std::remove("test_triggers.db");
    }

    // Helper to execute SQL and return result
    ExecutionResult executeSql(const std::string& sql)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto parse_result = parser.parseStatement();
        if (!parse_result.success())
        {
            return ExecutionResult("Parse failed: " + parse_result.error());
        }

        BytecodeGenerator generator(parser.stringPool());
        auto bytecode_result = generator.generate(parse_result.statement());
        if (!bytecode_result.success())
        {
            return ExecutionResult("Bytecode generation failed: " + bytecode_result.error());
        }

        Executor executor(db.get());
        return executor.execute(bytecode_result.bytecode());
    }

    // Helper to create test table
    void createTestTable()
    {
        auto result = executeSql("CREATE TABLE users (id INT32, name VARCHAR, age INT32)");
        ASSERT_TRUE(result.success()) << "Failed to create test table: " << result.error();
    }
};

// Test 1: CREATE TRIGGER with AFTER INSERT
TEST_F(TriggerTest, CreateTriggerAfterInsert)
{
    createTestTable();

    std::string sql = "CREATE TRIGGER log_insert AFTER INSERT ON users "
                      "FOR EACH ROW EXECUTE PROCEDURE log_user_insert()";

    auto result = executeSql(sql);
    EXPECT_TRUE(result.success()) << "Failed: " << result.error();
}

// Test 2: CREATE TRIGGER with BEFORE UPDATE
TEST_F(TriggerTest, CreateTriggerBeforeUpdate)
{
    createTestTable();

    std::string sql = "CREATE TRIGGER validate_update BEFORE UPDATE ON users "
                      "FOR EACH ROW EXECUTE PROCEDURE validate_user_update()";

    auto result = executeSql(sql);
    EXPECT_TRUE(result.success()) << "Failed: " << result.error();
}

// Test 3: CREATE TRIGGER with AFTER DELETE
TEST_F(TriggerTest, CreateTriggerAfterDelete)
{
    createTestTable();

    std::string sql = "CREATE TRIGGER audit_delete AFTER DELETE ON users "
                      "FOR EACH ROW EXECUTE PROCEDURE audit_user_delete()";

    auto result = executeSql(sql);
    EXPECT_TRUE(result.success()) << "Failed: " << result.error();
}

// Test 4: CREATE TRIGGER with BEFORE INSERT
TEST_F(TriggerTest, CreateTriggerBeforeInsert)
{
    createTestTable();

    std::string sql = "CREATE TRIGGER check_insert BEFORE INSERT ON users "
                      "FOR EACH ROW EXECUTE PROCEDURE check_user_insert()";

    auto result = executeSql(sql);
    EXPECT_TRUE(result.success()) << "Failed: " << result.error();
}

// Test 5: DROP TRIGGER
TEST_F(TriggerTest, DropTrigger)
{
    createTestTable();

    // Create trigger first
    auto create_result = executeSql(
        "CREATE TRIGGER log_insert AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE log_user_insert()");
    ASSERT_TRUE(create_result.success()) << "Failed to create: " << create_result.error();

    // Drop trigger
    auto drop_result = executeSql("DROP TRIGGER log_insert");
    EXPECT_TRUE(drop_result.success()) << "Failed to drop: " << drop_result.error();
}

// Test 6: Multiple triggers on same table
TEST_F(TriggerTest, MultipleTriggers)
{
    createTestTable();

    // Create multiple triggers
    auto result1 = executeSql(
        "CREATE TRIGGER before_ins BEFORE INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE check_insert()");
    EXPECT_TRUE(result1.success()) << "Failed 1: " << result1.error();

    auto result2 = executeSql(
        "CREATE TRIGGER after_ins AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE log_insert()");
    EXPECT_TRUE(result2.success()) << "Failed 2: " << result2.error();

    auto result3 = executeSql(
        "CREATE TRIGGER before_upd BEFORE UPDATE ON users "
        "FOR EACH ROW EXECUTE PROCEDURE validate_update()");
    EXPECT_TRUE(result3.success()) << "Failed 3: " << result3.error();
}

// Test 7: Trigger on non-existent table (should fail)
TEST_F(TriggerTest, TriggerOnNonExistentTable)
{
    std::string sql = "CREATE TRIGGER log_insert AFTER INSERT ON nonexistent "
                      "FOR EACH ROW EXECUTE PROCEDURE log_insert()";

    auto result = executeSql(sql);
    EXPECT_FALSE(result.success()) << "Should fail on non-existent table";
}

// Test 8: DROP non-existent trigger (should fail)
TEST_F(TriggerTest, DropNonExistentTrigger)
{
    auto result = executeSql("DROP TRIGGER nonexistent_trigger");
    EXPECT_FALSE(result.success()) << "Should fail on non-existent trigger";
}

// Test 9: Trigger naming - different names
TEST_F(TriggerTest, TriggerNaming)
{
    createTestTable();

    auto result1 = executeSql(
        "CREATE TRIGGER tr_users_bi BEFORE INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE check()");
    EXPECT_TRUE(result1.success()) << "Failed 1: " << result1.error();

    auto result2 = executeSql(
        "CREATE TRIGGER TR_USERS_AI AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE log()");
    EXPECT_TRUE(result2.success()) << "Failed 2: " << result2.error();

    auto result3 = executeSql(
        "CREATE TRIGGER users_before_update BEFORE UPDATE ON users "
        "FOR EACH ROW EXECUTE PROCEDURE validate()");
    EXPECT_TRUE(result3.success()) << "Failed 3: " << result3.error();
}

// Test 10: All trigger event types
TEST_F(TriggerTest, AllEventTypes)
{
    createTestTable();

    // INSERT trigger
    auto ins_result = executeSql(
        "CREATE TRIGGER tr_insert AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE proc_insert()");
    EXPECT_TRUE(ins_result.success()) << "INSERT trigger failed: " << ins_result.error();

    // UPDATE trigger
    auto upd_result = executeSql(
        "CREATE TRIGGER tr_update AFTER UPDATE ON users "
        "FOR EACH ROW EXECUTE PROCEDURE proc_update()");
    EXPECT_TRUE(upd_result.success()) << "UPDATE trigger failed: " << upd_result.error();

    // DELETE trigger
    auto del_result = executeSql(
        "CREATE TRIGGER tr_delete AFTER DELETE ON users "
        "FOR EACH ROW EXECUTE PROCEDURE proc_delete()");
    EXPECT_TRUE(del_result.success()) << "DELETE trigger failed: " << del_result.error();
}

// Test 11: Trigger with procedure name variations
TEST_F(TriggerTest, ProcedureNameVariations)
{
    createTestTable();

    auto result1 = executeSql(
        "CREATE TRIGGER tr1 AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE simple_proc()");
    EXPECT_TRUE(result1.success()) << "Failed 1: " << result1.error();

    auto result2 = executeSql(
        "CREATE TRIGGER tr2 AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE my_schema.complex_proc()");
    EXPECT_TRUE(result2.success()) << "Failed 2: " << result2.error();

    auto result3 = executeSql(
        "CREATE TRIGGER tr3 AFTER INSERT ON users "
        "FOR EACH ROW EXECUTE PROCEDURE PROC_UPPER_CASE()");
    EXPECT_TRUE(result3.success()) << "Failed 3: " << result3.error();
}

// Test 12: CREATE and DROP multiple times
TEST_F(TriggerTest, CreateDropCycle)
{
    createTestTable();

    for (int i = 0; i < 3; i++)
    {
        // Create
        auto create_result = executeSql(
            "CREATE TRIGGER test_trigger AFTER INSERT ON users "
            "FOR EACH ROW EXECUTE PROCEDURE test_proc()");
        EXPECT_TRUE(create_result.success())
            << "Iteration " << i << " create failed: " << create_result.error();

        // Drop
        auto drop_result = executeSql("DROP TRIGGER test_trigger");
        EXPECT_TRUE(drop_result.success())
            << "Iteration " << i << " drop failed: " << drop_result.error();
    }
}
