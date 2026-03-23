#include <gtest/gtest.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/proc_array.h"
#define private public
#include "scratchbird/sblr/executor.h"
#undef private
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using namespace scratchbird::sblr;

class ExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test database
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_executor", ".db");
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 8192, &ctx), Status::OK)
            << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK)
            << ctx.message;

        Status status = db_->initializeProcArray(16, &ctx);
        if (status != Status::OK && status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK)
            << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
        ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK)
            << ctx.message;

        system_user_id_ = db_->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user_id_, true);

        default_schema_id_ = resolveDefaultSchema(&ctx);
        ASSERT_NE(default_schema_id_, ID{});

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(db_->catalog_manager()->getSchema(default_schema_id_, schema_info, &ctx), Status::OK)
            << ctx.message;
        default_schema_name_ = schema_info.schema_name;
    }
    
    void TearDown() override {
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }
    
    std::vector<uint8_t> compileSQL(const std::string& sql) {
        QueryCompilerV3 compiler(db_.get());
        compiler.setCurrentSchema(default_schema_id_);
        auto result = compiler.compile(sql);
        if (!result.success()) {
            if (!result.errors().empty()) {
                std::cerr << "Compile error: " << result.errors().front() << "\n";
            } else {
                std::cerr << "Compile error: unknown\n";
            }
            return {};
        }
        return result.bytecode();
    }
    
    ExecutionResult executeSQL(const std::string& sql) {
        return executeSQLWithSchema(sql, true);
    }

    ExecutionResult executeSQLWithSchema(const std::string& sql, bool set_schema) {
        auto bytecode = compileSQL(sql);
        if (bytecode.empty()) {
            return ExecutionResult("Failed to compile SQL");
        }

        Executor executor(db_.get());
        executor.setConnectionContext(conn_ctx_.get());
        if (set_schema)
        {
            executor.setCurrentSchema(default_schema_id_);
        }
        auto result = executor.execute(bytecode);
        if (!result.success()) {
            std::cerr << "Executor error: " << result.error() << "\n";
        }
        return result;
    }

    ExecutionResult executeSQLConfigured(const std::string& sql,
                                         const std::function<void(Executor&)>& configure)
    {
        auto bytecode = compileSQL(sql);
        if (bytecode.empty())
        {
            return ExecutionResult("Failed to compile SQL");
        }

        Executor executor(db_.get());
        executor.setConnectionContext(conn_ctx_.get());
        executor.setCurrentSchema(default_schema_id_);
        configure(executor);
        auto result = executor.execute(bytecode);
        if (!result.success())
        {
            std::cerr << "Executor error: " << result.error() << "\n";
        }
        return result;
    }

    ID resolveDefaultSchema(ErrorContext* ctx)
    {
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = db_->catalog_manager()->listSchemas(schemas, ctx);
        if (status == Status::OK && !schemas.empty())
        {
            return schemas.front().schema_id;
        }

        ID schema_id;
        status = db_->catalog_manager()->createSchema("main", "SYSTEM", schema_id, ctx);
        if (status == Status::OK)
        {
            return schema_id;
        }
        return ID{};
    }

    std::string makeBinaryPolicyHex(const std::string& column,
                                    Opcode op,
                                    int32_t literal) const
    {
        std::vector<uint8_t> bytes;
        bytes.push_back(static_cast<uint8_t>(Opcode::COLUMN_REF));

        uint8_t len_buf[10];
        size_t len_bytes = scratchbird::sblr::writeUVarint(len_buf, column.size());
        bytes.insert(bytes.end(), len_buf, len_buf + len_bytes);
        bytes.insert(bytes.end(), column.begin(), column.end());

        bytes.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT32));
        uint8_t int_buf[4];
        scratchbird::sblr::writeInt32(int_buf, static_cast<uint32_t>(literal));
        bytes.insert(bytes.end(), int_buf, int_buf + 4);

        bytes.push_back(static_cast<uint8_t>(op));

        static const char hex_chars[] = "0123456789abcdef";
        std::string hex;
        hex.reserve(2 + bytes.size() * 2);
        hex.append("0x");
        for (uint8_t byte : bytes)
        {
            hex.push_back(hex_chars[(byte >> 4) & 0x0F]);
            hex.push_back(hex_chars[byte & 0x0F]);
        }
        return hex;
    }

    ID createUdrFunction(const std::string& udr_name,
                         const std::string& library_path,
                         const std::string& entry_point = "udr_entry")
    {
        ErrorContext ctx;
        ID udr_id;
        Status status = db_->catalog_manager()->createUDR(
            default_schema_id_,
            udr_name,
            library_path,
            entry_point,
            CatalogManager::UDRType::FUNCTION,
            "",
            udr_id,
            &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return udr_id;
    }

    ID registerRuntimeReadyUdrModule(const std::string& engine_name,
                                     const std::string& module_name,
                                     const std::string& library_path,
                                     const std::string& entry_point,
                                     CatalogManager::UDREngineType engine_type =
                                         CatalogManager::UDREngineType::NATIVE)
    {
        ErrorContext ctx;
        ID engine_id{};
        Status status = db_->catalog_manager()->registerUDREngine(
            engine_name, engine_type, "", "{}", engine_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        ID module_id{};
        status = db_->catalog_manager()->registerUDRModule(
            module_name, engine_id, library_path, entry_point, module_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        status = db_->catalog_manager()->validateUDRModule(module_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        status = db_->catalog_manager()->setUDRModuleLoaded(module_id, true, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return module_id;
    }

    Status invokeUdrBoundary(Executor& executor,
                             const ID& udr_id,
                             Executor::UdrInvocationScope scope,
                             const std::vector<Value>& args,
                             ErrorContext* ctx,
                             Value* out_value = nullptr)
    {
        executor.udr_invocation_scope_ = scope;
        Value out{};
        Status status = executor.callUDRFunctionById(udr_id, args, out, ctx);
        if (out_value != nullptr)
        {
            *out_value = out;
        }
        return status;
    }
    
protected:
    std::string test_db_path_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    ID default_schema_id_{};
    ID system_user_id_{};
    std::string default_schema_name_;
};

// ===== CREATE TABLE Tests =====

TEST_F(ExecutorTest, CreateTableSimple) {
    auto result = executeSQL("CREATE TABLE users (id INTEGER NOT NULL)");
    EXPECT_TRUE(result.success()) << result.error();
    
    // Verify table was created
    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    EXPECT_EQ(db_->catalog_manager()->getTable(default_schema_id_, "users", table_info, &ctx),
              Status::OK);
    EXPECT_EQ(table_info.table_name, "users");

    std::vector<CatalogManager::ColumnInfo> columns;
    EXPECT_EQ(db_->catalog_manager()->getColumns(table_info.table_id, columns, &ctx), Status::OK);
    ASSERT_EQ(columns.size(), 1u);
    EXPECT_EQ(columns[0].column_name, "id");
    EXPECT_EQ(columns[0].data_type, static_cast<uint16_t>(DataType::INT32));
    EXPECT_FALSE(columns[0].nullable);
}

TEST_F(ExecutorTest, CreateTableMultipleColumns) {
    auto result = executeSQL("CREATE TABLE products ("
                           "id BIGINT NOT NULL, "
                           "name VARCHAR(100), "
                           "price DOUBLE"
                           ")");
    EXPECT_TRUE(result.success()) << result.error();
    
    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    EXPECT_EQ(db_->catalog_manager()->getTable(default_schema_id_, "products", table_info, &ctx),
              Status::OK);
    std::vector<CatalogManager::ColumnInfo> columns;
    EXPECT_EQ(db_->catalog_manager()->getColumns(table_info.table_id, columns, &ctx), Status::OK);
    EXPECT_EQ(columns.size(), 3u);
}

TEST_F(ExecutorTest, CreateTableDuplicate) {
    auto result1 = executeSQL("CREATE TABLE test (id INTEGER)");
    EXPECT_TRUE(result1.success());
    
    auto result2 = executeSQL("CREATE TABLE test (id INTEGER)");
    EXPECT_FALSE(result2.success());
    EXPECT_NE(result2.error().find("already exists"), std::string::npos);
}

// ===== INSERT Tests =====

TEST_F(ExecutorTest, InsertSimple) {
    // Create table first
    auto create_result = executeSQL("CREATE TABLE users (id INTEGER, name VARCHAR(50))");
    ASSERT_TRUE(create_result.success());
    
    // Insert row
    auto insert_result = executeSQL("INSERT INTO users (id, name) VALUES (1, 'John')");
    EXPECT_TRUE(insert_result.success()) << insert_result.error();
}

TEST_F(ExecutorTest, InsertAllTypes) {
    auto create_result = executeSQL("CREATE TABLE test ("
                                  "i INTEGER, "
                                  "b BIGINT, "
                                  "d DOUBLE, "
                                  "s VARCHAR(100)"
                                  ")");
    ASSERT_TRUE(create_result.success());
    
    auto insert_result = executeSQL("INSERT INTO test (i, b, d, s) "
                                  "VALUES (42, 9999999999, 3.14159, 'Hello World')");
    EXPECT_TRUE(insert_result.success()) << insert_result.error();
}

TEST_F(ExecutorTest, InsertNull) {
    auto create_result = executeSQL("CREATE TABLE nullable (id INTEGER, data VARCHAR(50))");
    ASSERT_TRUE(create_result.success());
    
    auto insert_result = executeSQL("INSERT INTO nullable (id, data) VALUES (1, NULL)");
    EXPECT_TRUE(insert_result.success()) << insert_result.error();
}

TEST_F(ExecutorTest, TriggerBeforeInsertMissingProcedureFailsClosed) {
    ASSERT_TRUE(executeSQL("CREATE TABLE trg_insert_missing_proc (id INTEGER)").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_,
                                               "trg_insert_missing_proc",
                                               table_info,
                                               &ctx),
              Status::OK) << ctx.message;

    CatalogManager::TriggerInfo trigger{};
    trigger.trigger_name = "trg_before_insert_missing_proc";
    trigger.table_id = table_info.table_id;
    trigger.table_name = table_info.table_name;
    trigger.timing = CatalogManager::TriggerTiming::BEFORE;
    trigger.event_mask = 1u << static_cast<uint8_t>(CatalogManager::TriggerEvent::INSERT);
    trigger.granularity = CatalogManager::TriggerGranularity::FOR_EACH_ROW;
    trigger.procedure_name = "missing_insert_proc";
    ASSERT_EQ(db_->catalog_manager()->createTrigger(trigger, &ctx), Status::OK) << ctx.message;

    std::vector<CatalogManager::TriggerInfo> before_triggers;
    ASSERT_EQ(db_->catalog_manager()->listTriggersForTable(
                  table_info.table_id,
                  CatalogManager::TriggerEvent::INSERT,
                  CatalogManager::TriggerTiming::BEFORE,
                  before_triggers,
                  &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(before_triggers.size(), 1u);
    EXPECT_EQ(before_triggers[0].procedure_name, "missing_insert_proc");

    auto insert_result = executeSQL("INSERT INTO trg_insert_missing_proc (id) VALUES (1)");
    EXPECT_FALSE(insert_result.success());
    EXPECT_NE(insert_result.error().find("Procedure not found"), std::string::npos);

    auto count_result = executeSQL("SELECT COUNT(*) FROM trg_insert_missing_proc");
    ASSERT_TRUE(count_result.success()) << count_result.error();
    auto* rs = count_result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 0);
}

TEST_F(ExecutorTest, TriggerBeforeUpdateVetoStopsFurtherCallbacks) {
    ASSERT_TRUE(executeSQL("CREATE TABLE trg_update_veto (id INTEGER, value INTEGER)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO trg_update_veto (id, value) VALUES (1, 10)").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_,
                                               "trg_update_veto",
                                               table_info,
                                               &ctx),
              Status::OK) << ctx.message;

    CatalogManager::TriggerInfo trigger_a{};
    trigger_a.trigger_name = "trg_before_update_a";
    trigger_a.table_id = table_info.table_id;
    trigger_a.table_name = table_info.table_name;
    trigger_a.timing = CatalogManager::TriggerTiming::BEFORE;
    trigger_a.event_mask = 1u << static_cast<uint8_t>(CatalogManager::TriggerEvent::UPDATE);
    trigger_a.granularity = CatalogManager::TriggerGranularity::FOR_EACH_ROW;
    trigger_a.procedure_name = "before_update_a";
    ASSERT_EQ(db_->catalog_manager()->createTrigger(trigger_a, &ctx), Status::OK) << ctx.message;

    CatalogManager::TriggerInfo trigger_b{};
    trigger_b.trigger_name = "trg_before_update_b";
    trigger_b.table_id = table_info.table_id;
    trigger_b.table_name = table_info.table_name;
    trigger_b.timing = CatalogManager::TriggerTiming::BEFORE;
    trigger_b.event_mask = 1u << static_cast<uint8_t>(CatalogManager::TriggerEvent::UPDATE);
    trigger_b.granularity = CatalogManager::TriggerGranularity::FOR_EACH_ROW;
    trigger_b.procedure_name = "before_update_b";
    ASSERT_EQ(db_->catalog_manager()->createTrigger(trigger_b, &ctx), Status::OK) << ctx.message;

    int before_a_calls = 0;
    int before_b_calls = 0;
    auto update_result = executeSQLConfigured(
        "UPDATE trg_update_veto SET value = value + 1 WHERE id = 1",
        [&](Executor& executor) {
            executor.registerTriggerProcedure(
                "before_update_a",
                [&](const Executor::TriggerContext&) {
                    ++before_a_calls;
                    return false;
                });
            executor.registerTriggerProcedure(
                "before_update_b",
                [&](const Executor::TriggerContext&) {
                    ++before_b_calls;
                    return true;
                });
        });
    ASSERT_TRUE(update_result.success()) << update_result.error();
    EXPECT_EQ(before_a_calls, 1);
    EXPECT_EQ(before_b_calls, 0);

    auto value_result = executeSQL("SELECT value FROM trg_update_veto WHERE id = 1");
    ASSERT_TRUE(value_result.success()) << value_result.error();
    auto* rs = value_result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 10);
}

TEST_F(ExecutorTest, TriggerBeforeDeleteVetoStopsFurtherCallbacks) {
    ASSERT_TRUE(executeSQL("CREATE TABLE trg_delete_veto (id INTEGER, value INTEGER)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO trg_delete_veto (id, value) VALUES (1, 10)").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_,
                                               "trg_delete_veto",
                                               table_info,
                                               &ctx),
              Status::OK) << ctx.message;

    CatalogManager::TriggerInfo trigger_a{};
    trigger_a.trigger_name = "trg_before_delete_a";
    trigger_a.table_id = table_info.table_id;
    trigger_a.table_name = table_info.table_name;
    trigger_a.timing = CatalogManager::TriggerTiming::BEFORE;
    trigger_a.event_mask = 1u << static_cast<uint8_t>(CatalogManager::TriggerEvent::DELETE);
    trigger_a.granularity = CatalogManager::TriggerGranularity::FOR_EACH_ROW;
    trigger_a.procedure_name = "before_delete_a";
    ASSERT_EQ(db_->catalog_manager()->createTrigger(trigger_a, &ctx), Status::OK) << ctx.message;

    CatalogManager::TriggerInfo trigger_b{};
    trigger_b.trigger_name = "trg_before_delete_b";
    trigger_b.table_id = table_info.table_id;
    trigger_b.table_name = table_info.table_name;
    trigger_b.timing = CatalogManager::TriggerTiming::BEFORE;
    trigger_b.event_mask = 1u << static_cast<uint8_t>(CatalogManager::TriggerEvent::DELETE);
    trigger_b.granularity = CatalogManager::TriggerGranularity::FOR_EACH_ROW;
    trigger_b.procedure_name = "before_delete_b";
    ASSERT_EQ(db_->catalog_manager()->createTrigger(trigger_b, &ctx), Status::OK) << ctx.message;

    int before_a_calls = 0;
    int before_b_calls = 0;
    auto delete_result = executeSQLConfigured(
        "DELETE FROM trg_delete_veto WHERE id = 1",
        [&](Executor& executor) {
            executor.registerTriggerProcedure(
                "before_delete_a",
                [&](const Executor::TriggerContext&) {
                    ++before_a_calls;
                    return false;
                });
            executor.registerTriggerProcedure(
                "before_delete_b",
                [&](const Executor::TriggerContext&) {
                    ++before_b_calls;
                    return true;
                });
        });
    ASSERT_TRUE(delete_result.success()) << delete_result.error();
    EXPECT_EQ(before_a_calls, 1);
    EXPECT_EQ(before_b_calls, 0);

    auto count_result = executeSQL("SELECT COUNT(1) FROM trg_delete_veto");
    ASSERT_TRUE(count_result.success()) << count_result.error();
    auto* rs = count_result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
}

TEST_F(ExecutorTest, TriggerAfterInsertMissingProcedureFailsClosed) {
    ASSERT_TRUE(executeSQL("CREATE TABLE trg_after_insert_missing_proc (id INTEGER)").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_,
                                               "trg_after_insert_missing_proc",
                                               table_info,
                                               &ctx),
              Status::OK) << ctx.message;

    CatalogManager::TriggerInfo trigger{};
    trigger.trigger_name = "trg_after_insert_missing_proc";
    trigger.table_id = table_info.table_id;
    trigger.table_name = table_info.table_name;
    trigger.timing = CatalogManager::TriggerTiming::AFTER;
    trigger.event_mask = 1u << static_cast<uint8_t>(CatalogManager::TriggerEvent::INSERT);
    trigger.granularity = CatalogManager::TriggerGranularity::FOR_EACH_ROW;
    trigger.procedure_name = "missing_after_insert_proc";
    ASSERT_EQ(db_->catalog_manager()->createTrigger(trigger, &ctx), Status::OK) << ctx.message;

    auto insert_result = executeSQL("INSERT INTO trg_after_insert_missing_proc (id) VALUES (1)");
    EXPECT_FALSE(insert_result.success());
    EXPECT_NE(insert_result.error().find("Procedure not found"), std::string::npos);
}

TEST_F(ExecutorTest, InsertExpressions) {
    auto create_result = executeSQL("CREATE TABLE calc (result INTEGER)");
    ASSERT_TRUE(create_result.success());
    
    auto insert_result = executeSQL("INSERT INTO calc (result) VALUES (10 + 20 * 3)");
    EXPECT_TRUE(insert_result.success()) << insert_result.error();
}

TEST_F(ExecutorTest, InsertTableNotFound) {
    auto result = executeSQL("INSERT INTO nonexistent (id) VALUES (1)");
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.error().empty());
}

// ===== SELECT Tests =====

TEST_F(ExecutorTest, SelectEmpty) {
    auto create_result = executeSQL("CREATE TABLE empty (id INTEGER)");
    ASSERT_TRUE(create_result.success());
    
    auto select_result = executeSQL("SELECT * FROM empty");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    
    auto* rs = select_result.resultSet();
    EXPECT_EQ(rs->columnCount(), 1u);
    EXPECT_EQ(rs->columnName(0), "id");
    EXPECT_EQ(rs->rowCount(), 0u);
}

TEST_F(ExecutorTest, SelectWithData) {
    // Create and populate table
    ASSERT_TRUE(executeSQL("CREATE TABLE users (id INTEGER, name VARCHAR(50))").success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name) VALUES (1, 'Alice')").success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name) VALUES (2, 'Bob')").success());
    
    // Select all
    auto result = executeSQL("SELECT * FROM users");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    
    auto* rs = result.resultSet();
    EXPECT_EQ(rs->columnCount(), 2u);
    EXPECT_EQ(rs->rowCount(), 2u);
    
    // Check first row
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(rs->getValue(0, 1).toString(), "Alice");
    
    // Check second row
    EXPECT_EQ(rs->getValue(1, 0).toInt64(), 2);
    EXPECT_EQ(rs->getValue(1, 1).toString(), "Bob");
}

TEST_F(ExecutorTest, SelectSpecificColumns) {
    ASSERT_TRUE(executeSQL("CREATE TABLE test (a INTEGER, b INTEGER, c INTEGER)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO test (a, b, c) VALUES (1, 2, 3)").success());
    
    auto result = executeSQL("SELECT a, c FROM test");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    
    auto* rs = result.resultSet();
    EXPECT_EQ(rs->columnCount(), 2u);
    EXPECT_EQ(rs->columnName(0), "a");
    EXPECT_EQ(rs->columnName(1), "c");
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(rs->getValue(0, 1).toInt64(), 3);
}

TEST_F(ExecutorTest, SelectExpressionRequiresTableSelectWhenUsingColumnGrants) {
    ASSERT_TRUE(executeSQL("CREATE TABLE docs_perm (id INTEGER, title TEXT, body TEXT)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_perm (id, title, body) VALUES (1, 'Doc1', 'alpha beta')").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_, "docs_perm", table_info, &ctx), Status::OK)
        << ctx.message;

    ID viewer_id;
    ASSERT_EQ(db_->catalog_manager()->createUser("viewer_ft_perm", "", default_schema_id_, false,
                                                 viewer_id, &ctx), Status::OK)
        << ctx.message;
    CatalogManager::BasicUserInfo viewer_info;
    ASSERT_EQ(db_->catalog_manager()->getUserBasic(viewer_id, viewer_info, &ctx), Status::OK)
        << ctx.message;
    ASSERT_FALSE(viewer_info.is_superuser);

    auto revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        viewer_id, CatalogManager::GranteeType::USER,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;
    revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        ID{}, CatalogManager::GranteeType::PUBLIC,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;

    auto status = db_->catalog_manager()->grantColumnPermission(
        table_info.table_id, "title", viewer_id, CatalogManager::GranteeType::USER,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), false, system_user_id_, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    bool has_table_select = true;
    ASSERT_EQ(db_->catalog_manager()->hasPermission(
                  viewer_id, table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
                  CatalogManager::Privilege::SELECT, has_table_select, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(has_table_select);

    conn_ctx_->setCurrentUser(viewer_id, false);
    conn_ctx_->setCurrentSchemaId(default_schema_id_);

    std::string sql = "SELECT title || '!' AS decorated FROM " + default_schema_name_ + ".docs_perm";
    auto result = executeSQLWithSchema(sql, false);
    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find(
                  "Permission denied: SELECT expression requires table-level SELECT on table "),
              std::string::npos);
    EXPECT_NE(result.error().find("docs_perm"), std::string::npos);

    conn_ctx_->setCurrentUser(system_user_id_, true);
}

TEST_F(ExecutorTest, IndexedSelectPathStillAppliesRlsPolicies) {
    ASSERT_TRUE(executeSQL("CREATE TABLE docs_rls (id INTEGER, tenant_id INTEGER, body TEXT)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_rls (id, tenant_id, body) VALUES (1, 1, 'alpha doc')").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_rls (id, tenant_id, body) VALUES (2, 2, 'alpha doc')").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_rls (id, tenant_id, body) VALUES (3, 1, 'beta doc')").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_docs_rls_id ON docs_rls USING BTREE (id)").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_, "docs_rls", table_info, &ctx), Status::OK)
        << ctx.message;

    ID viewer_id;
    ASSERT_EQ(db_->catalog_manager()->createUser("viewer_ft_rls", "", default_schema_id_, false,
                                                 viewer_id, &ctx), Status::OK)
        << ctx.message;
    CatalogManager::BasicUserInfo viewer_info;
    ASSERT_EQ(db_->catalog_manager()->getUserBasic(viewer_id, viewer_info, &ctx), Status::OK)
        << ctx.message;
    ASSERT_FALSE(viewer_info.is_superuser);

    auto revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        viewer_id, CatalogManager::GranteeType::USER,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;
    revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        ID{}, CatalogManager::GranteeType::PUBLIC,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;

    const uint32_t select_priv = static_cast<uint32_t>(CatalogManager::Privilege::SELECT);
    ASSERT_EQ(db_->catalog_manager()->grantColumnPermission(
                  table_info.table_id, "id", viewer_id, CatalogManager::GranteeType::USER,
                  select_priv, false, system_user_id_, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(db_->catalog_manager()->grantColumnPermission(
                  table_info.table_id, "tenant_id", viewer_id, CatalogManager::GranteeType::USER,
                  select_priv, false, system_user_id_, &ctx),
              Status::OK)
        << ctx.message;

    bool has_table_select = true;
    ASSERT_EQ(db_->catalog_manager()->hasPermission(
                  viewer_id, table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
                  CatalogManager::Privilege::SELECT, has_table_select, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(has_table_select);

    ASSERT_EQ(db_->catalog_manager()->setTableRLS(table_info.table_id, true, true, &ctx), Status::OK)
        << ctx.message;

    ID policy_id;
    std::string policy_expr = makeBinaryPolicyHex("tenant_id", Opcode::EXPR_EQ, 1);
    ASSERT_EQ(db_->catalog_manager()->createPolicy(
                  table_info.table_id, "tenant_only", CatalogManager::PolicyType::ALL,
                  {}, policy_expr, policy_expr, policy_id, &ctx),
              Status::OK)
        << ctx.message;

    conn_ctx_->setCurrentUser(viewer_id, false);
    conn_ctx_->setCurrentSchemaId(default_schema_id_);

    std::string sql = "SELECT id, tenant_id FROM " + default_schema_name_ + ".docs_rls";
    auto result = executeSQLWithSchema(sql, false);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 2u);
    EXPECT_EQ(rs->getValue(0, 1).toInt64(), 1);
    EXPECT_EQ(rs->getValue(1, 1).toInt64(), 1);

    conn_ctx_->setCurrentUser(system_user_id_, true);
}

TEST_F(ExecutorTest, RoleGrantedColumnsRemainVisibleUnderLeastPrivilegeDefaults) {
    ASSERT_TRUE(executeSQL("CREATE TABLE docs_role_cols (id INTEGER, title TEXT, body TEXT)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_role_cols (id, title, body) VALUES (1, 'Doc1', 'alpha beta')").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_, "docs_role_cols", table_info, &ctx), Status::OK)
        << ctx.message;

    ID viewer_id{};
    ASSERT_EQ(db_->catalog_manager()->createUser("viewer_role_cols", "", default_schema_id_, false,
                                                 viewer_id, &ctx), Status::OK)
        << ctx.message;

    ID role_id{};
    ASSERT_EQ(db_->catalog_manager()->createRole("role_cols_reader", system_user_id_,
                                                 default_schema_id_, role_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(db_->catalog_manager()->grantRole(role_id, viewer_id, system_user_id_, false, &ctx),
              Status::OK)
        << ctx.message;

    auto revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        viewer_id, CatalogManager::GranteeType::USER,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;
    revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        ID{}, CatalogManager::GranteeType::PUBLIC,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;

    const uint32_t select_priv = static_cast<uint32_t>(CatalogManager::Privilege::SELECT);
    ASSERT_EQ(db_->catalog_manager()->grantColumnPermission(
                  table_info.table_id, "id", role_id, CatalogManager::GranteeType::ROLE,
                  select_priv, false, system_user_id_, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(db_->catalog_manager()->grantColumnPermission(
                  table_info.table_id, "title", role_id, CatalogManager::GranteeType::ROLE,
                  select_priv, false, system_user_id_, &ctx),
              Status::OK)
        << ctx.message;

    conn_ctx_->setCurrentUser(viewer_id, false);
    conn_ctx_->setCurrentSchemaId(default_schema_id_);

    std::string star_sql = "SELECT * FROM " + default_schema_name_ + ".docs_role_cols";
    auto star_result = executeSQLWithSchema(star_sql, false);
    ASSERT_TRUE(star_result.success()) << star_result.error();
    ASSERT_TRUE(star_result.hasResultSet());
    auto* rs = star_result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->columnCount(), 2u);
    EXPECT_EQ(rs->columnName(0), "id");
    EXPECT_EQ(rs->columnName(1), "title");
    EXPECT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(rs->getValue(0, 1).toString(), "Doc1");

    std::string denied_sql = "SELECT body FROM " + default_schema_name_ + ".docs_role_cols";
    auto denied_result = executeSQLWithSchema(denied_sql, false);
    EXPECT_FALSE(denied_result.success());
    EXPECT_NE(denied_result.error().find("Permission denied: SELECT on column body"),
              std::string::npos);

    conn_ctx_->setCurrentUser(system_user_id_, true);
}

TEST_F(ExecutorTest, RlsWithoutPoliciesFailsClosedForNonOwner) {
    ASSERT_TRUE(executeSQL("CREATE TABLE docs_rls_default (id INTEGER, tenant_id INTEGER, body TEXT)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_rls_default (id, tenant_id, body) VALUES (1, 1, 'alpha doc')").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_rls_default (id, tenant_id, body) VALUES (2, 2, 'beta doc')").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_, "docs_rls_default", table_info, &ctx), Status::OK)
        << ctx.message;

    ID viewer_id{};
    ASSERT_EQ(db_->catalog_manager()->createUser("viewer_rls_default", "", default_schema_id_, false,
                                                 viewer_id, &ctx), Status::OK)
        << ctx.message;

    auto revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        viewer_id, CatalogManager::GranteeType::USER,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;
    revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        ID{}, CatalogManager::GranteeType::PUBLIC,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;

    const uint32_t select_priv = static_cast<uint32_t>(CatalogManager::Privilege::SELECT);
    ASSERT_EQ(db_->catalog_manager()->grantColumnPermission(
                  table_info.table_id, "id", viewer_id, CatalogManager::GranteeType::USER,
                  select_priv, false, system_user_id_, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(db_->catalog_manager()->grantColumnPermission(
                  table_info.table_id, "tenant_id", viewer_id, CatalogManager::GranteeType::USER,
                  select_priv, false, system_user_id_, &ctx),
              Status::OK)
        << ctx.message;

    ASSERT_EQ(db_->catalog_manager()->setTableRLS(table_info.table_id, true, false, &ctx), Status::OK)
        << ctx.message;

    conn_ctx_->setCurrentUser(viewer_id, false);
    conn_ctx_->setCurrentSchemaId(default_schema_id_);

    std::string sql = "SELECT id, tenant_id FROM " + default_schema_name_ + ".docs_rls_default";
    auto result = executeSQLWithSchema(sql, false);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_NE(result.resultSet(), nullptr);
    EXPECT_EQ(result.resultSet()->rowCount(), 0u);

    conn_ctx_->setCurrentUser(system_user_id_, true);
}

TEST_F(ExecutorTest, RoleScopedRlsPoliciesDenyUsersOutsidePolicyRoles) {
    ASSERT_TRUE(executeSQL("CREATE TABLE docs_rls_role (id INTEGER, tenant_id INTEGER, body TEXT)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_rls_role (id, tenant_id, body) VALUES (1, 1, 'alpha doc')").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_rls_role (id, tenant_id, body) VALUES (2, 2, 'beta doc')").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_, "docs_rls_role", table_info, &ctx), Status::OK)
        << ctx.message;

    ID viewer_id{};
    ASSERT_EQ(db_->catalog_manager()->createUser("viewer_rls_role", "", default_schema_id_, false,
                                                 viewer_id, &ctx), Status::OK)
        << ctx.message;

    ID role_id{};
    ASSERT_EQ(db_->catalog_manager()->createRole("tenant_one_role", system_user_id_,
                                                 default_schema_id_, role_id, &ctx), Status::OK)
        << ctx.message;

    auto revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        viewer_id, CatalogManager::GranteeType::USER,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;
    revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        ID{}, CatalogManager::GranteeType::PUBLIC,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;

    const uint32_t select_priv = static_cast<uint32_t>(CatalogManager::Privilege::SELECT);
    ASSERT_EQ(db_->catalog_manager()->grantColumnPermission(
                  table_info.table_id, "id", viewer_id, CatalogManager::GranteeType::USER,
                  select_priv, false, system_user_id_, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(db_->catalog_manager()->grantColumnPermission(
                  table_info.table_id, "tenant_id", viewer_id, CatalogManager::GranteeType::USER,
                  select_priv, false, system_user_id_, &ctx),
              Status::OK)
        << ctx.message;

    ASSERT_EQ(db_->catalog_manager()->setTableRLS(table_info.table_id, true, false, &ctx), Status::OK)
        << ctx.message;

    ID policy_id{};
    std::string policy_expr = makeBinaryPolicyHex("tenant_id", Opcode::EXPR_EQ, 1);
    ASSERT_EQ(db_->catalog_manager()->createPolicy(
                  table_info.table_id, "tenant_one_only", CatalogManager::PolicyType::ALL,
                  {"tenant_one_role"}, policy_expr, policy_expr, policy_id, &ctx),
              Status::OK)
        << ctx.message;

    conn_ctx_->setCurrentUser(viewer_id, false);
    conn_ctx_->setCurrentSchemaId(default_schema_id_);

    std::string sql = "SELECT id, tenant_id FROM " + default_schema_name_ + ".docs_rls_role";
    auto denied_result = executeSQLWithSchema(sql, false);
    ASSERT_TRUE(denied_result.success()) << denied_result.error();
    ASSERT_TRUE(denied_result.hasResultSet());
    ASSERT_NE(denied_result.resultSet(), nullptr);
    EXPECT_EQ(denied_result.resultSet()->rowCount(), 0u);

    ASSERT_EQ(db_->catalog_manager()->grantRole(role_id, viewer_id, system_user_id_, false, &ctx),
              Status::OK)
        << ctx.message;
    // Security catalog changes are transaction-scoped under MGA semantics, so
    // the granted role must be committed before a separate attachment can see it.
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    std::unique_ptr<ConnectionContext> role_conn_ctx;
    ASSERT_EQ(db_->connect(role_conn_ctx, &ctx), Status::OK) << ctx.message;
    ConnectionContext::setCurrent(role_conn_ctx.get());
    ASSERT_EQ(role_conn_ctx->initialize(&ctx), Status::OK) << ctx.message;
    role_conn_ctx->setCurrentUser(viewer_id, false);
    role_conn_ctx->setCurrentSchemaId(default_schema_id_);

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty());

    Executor allowed_executor(db_.get());
    allowed_executor.setConnectionContext(role_conn_ctx.get());
    auto allowed_result = allowed_executor.execute(bytecode);
    ASSERT_TRUE(allowed_result.success()) << allowed_result.error();
    ASSERT_TRUE(allowed_result.hasResultSet());
    auto* rs = allowed_result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(rs->getValue(0, 1).toInt64(), 1);

    ConnectionContext::setCurrent(conn_ctx_.get());

    conn_ctx_->setCurrentUser(system_user_id_, true);
}

TEST_F(ExecutorTest, QueryResultCacheIsolatedPerSecurityContext) {
    ASSERT_TRUE(executeSQL("CREATE TABLE docs_rls_cache_scope (id INTEGER, tenant_id INTEGER, body TEXT)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_rls_cache_scope (id, tenant_id, body) VALUES (1, 1, 'alpha doc')").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_rls_cache_scope (id, tenant_id, body) VALUES (2, 2, 'beta doc')").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_, "docs_rls_cache_scope", table_info, &ctx), Status::OK)
        << ctx.message;

    ID viewer_one_id{};
    ASSERT_EQ(db_->catalog_manager()->createUser("viewer_rls_cache_one", "", default_schema_id_, false,
                                                 viewer_one_id, &ctx), Status::OK)
        << ctx.message;

    ID viewer_two_id{};
    ASSERT_EQ(db_->catalog_manager()->createUser("viewer_rls_cache_two", "", default_schema_id_, false,
                                                 viewer_two_id, &ctx), Status::OK)
        << ctx.message;

    ID role_one_id{};
    ASSERT_EQ(db_->catalog_manager()->createRole("tenant_cache_one_role", system_user_id_,
                                                 default_schema_id_, role_one_id, &ctx), Status::OK)
        << ctx.message;

    ID role_two_id{};
    ASSERT_EQ(db_->catalog_manager()->createRole("tenant_cache_two_role", system_user_id_,
                                                 default_schema_id_, role_two_id, &ctx), Status::OK)
        << ctx.message;

    auto revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        ID{}, CatalogManager::GranteeType::PUBLIC,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;

    const uint32_t select_priv = static_cast<uint32_t>(CatalogManager::Privilege::SELECT);
    for (const auto& viewer_id : {viewer_one_id, viewer_two_id})
    {
        ASSERT_EQ(db_->catalog_manager()->grantColumnPermission(
                      table_info.table_id, "id", viewer_id, CatalogManager::GranteeType::USER,
                      select_priv, false, system_user_id_, &ctx),
                  Status::OK)
            << ctx.message;
        ASSERT_EQ(db_->catalog_manager()->grantColumnPermission(
                      table_info.table_id, "tenant_id", viewer_id, CatalogManager::GranteeType::USER,
                      select_priv, false, system_user_id_, &ctx),
                  Status::OK)
            << ctx.message;
    }

    ASSERT_EQ(db_->catalog_manager()->setTableRLS(table_info.table_id, true, false, &ctx), Status::OK)
        << ctx.message;

    ID policy_one_id{};
    std::string tenant_one_expr = makeBinaryPolicyHex("tenant_id", Opcode::EXPR_EQ, 1);
    ASSERT_EQ(db_->catalog_manager()->createPolicy(
                  table_info.table_id, "tenant_cache_one_only", CatalogManager::PolicyType::ALL,
                  {"tenant_cache_one_role"}, tenant_one_expr, tenant_one_expr, policy_one_id, &ctx),
              Status::OK)
        << ctx.message;

    ID policy_two_id{};
    std::string tenant_two_expr = makeBinaryPolicyHex("tenant_id", Opcode::EXPR_EQ, 2);
    ASSERT_EQ(db_->catalog_manager()->createPolicy(
                  table_info.table_id, "tenant_cache_two_only", CatalogManager::PolicyType::ALL,
                  {"tenant_cache_two_role"}, tenant_two_expr, tenant_two_expr, policy_two_id, &ctx),
              Status::OK)
        << ctx.message;

    ASSERT_EQ(db_->catalog_manager()->grantRole(role_one_id, viewer_one_id, system_user_id_, false, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(db_->catalog_manager()->grantRole(role_two_id, viewer_two_id, system_user_id_, false, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    std::string sql = "SELECT id, tenant_id FROM " + default_schema_name_ + ".docs_rls_cache_scope";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty());

    std::unique_ptr<ConnectionContext> viewer_one_ctx;
    ASSERT_EQ(db_->connect(viewer_one_ctx, &ctx), Status::OK) << ctx.message;
    ConnectionContext::setCurrent(viewer_one_ctx.get());
    ASSERT_EQ(viewer_one_ctx->initialize(&ctx), Status::OK) << ctx.message;
    viewer_one_ctx->setCurrentUser(viewer_one_id, false);
    viewer_one_ctx->setCurrentSchemaId(default_schema_id_);

    Executor viewer_one_executor(db_.get());
    viewer_one_executor.setConnectionContext(viewer_one_ctx.get());
    auto viewer_one_result = viewer_one_executor.execute(bytecode);
    ASSERT_TRUE(viewer_one_result.success()) << viewer_one_result.error();
    ASSERT_TRUE(viewer_one_result.hasResultSet());
    ASSERT_NE(viewer_one_result.resultSet(), nullptr);
    ASSERT_EQ(viewer_one_result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(viewer_one_result.resultSet()->getValue(0, 0).toInt64(), 1);
    EXPECT_EQ(viewer_one_result.resultSet()->getValue(0, 1).toInt64(), 1);

    std::unique_ptr<ConnectionContext> viewer_two_ctx;
    ASSERT_EQ(db_->connect(viewer_two_ctx, &ctx), Status::OK) << ctx.message;
    ConnectionContext::setCurrent(viewer_two_ctx.get());
    ASSERT_EQ(viewer_two_ctx->initialize(&ctx), Status::OK) << ctx.message;
    viewer_two_ctx->setCurrentUser(viewer_two_id, false);
    viewer_two_ctx->setCurrentSchemaId(default_schema_id_);

    Executor viewer_two_executor(db_.get());
    viewer_two_executor.setConnectionContext(viewer_two_ctx.get());
    auto viewer_two_result = viewer_two_executor.execute(bytecode);
    ASSERT_TRUE(viewer_two_result.success()) << viewer_two_result.error();
    ASSERT_TRUE(viewer_two_result.hasResultSet());
    ASSERT_NE(viewer_two_result.resultSet(), nullptr);
    ASSERT_EQ(viewer_two_result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(viewer_two_result.resultSet()->getValue(0, 0).toInt64(), 2);
    EXPECT_EQ(viewer_two_result.resultSet()->getValue(0, 1).toInt64(), 2);

    ConnectionContext::setCurrent(conn_ctx_.get());
    conn_ctx_->setCurrentUser(system_user_id_, true);
}

TEST_F(ExecutorTest, RoleGrantInvalidatesPermissionCacheForRoleBasedTableAccess) {
    ASSERT_TRUE(executeSQL("CREATE TABLE docs_role_select (id INTEGER, title TEXT)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_role_select (id, title) VALUES (1, 'Doc1')").success());
    ASSERT_TRUE(executeSQL("INSERT INTO docs_role_select (id, title) VALUES (2, 'Doc2')").success());

    ErrorContext ctx;
    CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_, "docs_role_select", table_info, &ctx), Status::OK)
        << ctx.message;

    ID viewer_id{};
    ASSERT_EQ(db_->catalog_manager()->createUser("viewer_role_select", "", default_schema_id_, false,
                                                 viewer_id, &ctx), Status::OK)
        << ctx.message;

    ID role_id{};
    ASSERT_EQ(db_->catalog_manager()->createRole("role_select_reader", system_user_id_,
                                                 default_schema_id_, role_id, &ctx), Status::OK)
        << ctx.message;

    auto revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        viewer_id, CatalogManager::GranteeType::USER,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;
    revoke_status = db_->catalog_manager()->revokePermission(
        table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
        ID{}, CatalogManager::GranteeType::PUBLIC,
        static_cast<uint32_t>(CatalogManager::Privilege::SELECT), &ctx);
    ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND) << ctx.message;

    ASSERT_EQ(db_->catalog_manager()->grantPermission(
                  table_info.table_id, CatalogManager::PermissionObjectType::TABLE,
                  role_id, CatalogManager::GranteeType::ROLE,
                  static_cast<uint32_t>(CatalogManager::Privilege::SELECT), false,
                  system_user_id_, &ctx),
              Status::OK)
        << ctx.message;

    conn_ctx_->setCurrentUser(viewer_id, false);
    conn_ctx_->setCurrentSchemaId(default_schema_id_);

    std::string sql = "SELECT id, title FROM " + default_schema_name_ + ".docs_role_select";
    auto denied_result = executeSQLWithSchema(sql, false);
    EXPECT_FALSE(denied_result.success());
    EXPECT_NE(denied_result.error().find("Permission denied: SELECT on table"),
              std::string::npos);

    conn_ctx_->setCurrentUser(system_user_id_, true);
    ASSERT_EQ(db_->catalog_manager()->grantRole(role_id, viewer_id, system_user_id_, false, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(conn_ctx_->commit(&ctx), Status::OK) << ctx.message;

    std::unique_ptr<ConnectionContext> role_conn_ctx;
    ASSERT_EQ(db_->connect(role_conn_ctx, &ctx), Status::OK) << ctx.message;
    ConnectionContext::setCurrent(role_conn_ctx.get());
    ASSERT_EQ(role_conn_ctx->initialize(&ctx), Status::OK) << ctx.message;
    role_conn_ctx->setCurrentUser(viewer_id, false);
    role_conn_ctx->setCurrentSchemaId(default_schema_id_);

    Executor allowed_executor(db_.get());
    allowed_executor.setConnectionContext(role_conn_ctx.get());
    auto allowed_result = allowed_executor.execute(compileSQL(sql));
    ASSERT_TRUE(allowed_result.success()) << allowed_result.error();
    ASSERT_TRUE(allowed_result.hasResultSet());
    ASSERT_NE(allowed_result.resultSet(), nullptr);
    EXPECT_EQ(allowed_result.resultSet()->rowCount(), 2u);

    ConnectionContext::setCurrent(conn_ctx_.get());
    conn_ctx_->setCurrentUser(system_user_id_, true);
}

// ===== Integration Tests =====

TEST_F(ExecutorTest, CompleteWorkflow) {
    // Create table
    auto create = executeSQL("CREATE TABLE employees ("
                           "id INTEGER NOT NULL, "
                           "name VARCHAR(100), "
                           "salary DOUBLE"
                           ")");
    ASSERT_TRUE(create.success()) << create.error();
    
    // Insert data
    std::vector<std::string> inserts = {
        "INSERT INTO employees (id, name, salary) VALUES (1, 'Alice', 50000.0)",
        "INSERT INTO employees (id, name, salary) VALUES (2, 'Bob', 60000.0)",
        "INSERT INTO employees (id, name, salary) VALUES (3, 'Charlie', 55000.0)"
    };
    
    for (const auto& sql : inserts) {
        auto result = executeSQL(sql);
        ASSERT_TRUE(result.success()) << sql << " failed: " << result.error();
    }
    
    // Select and verify
    auto select = executeSQL("SELECT * FROM employees");
    ASSERT_TRUE(select.success()) << select.error();
    ASSERT_TRUE(select.hasResultSet());
    
    auto* rs = select.resultSet();
    EXPECT_EQ(rs->rowCount(), 3u);
    
    // Print results
    std::stringstream ss;
    rs->print(ss);
    std::cout << "\nQuery Results:\n" << ss.str() << std::endl;
}

TEST_F(ExecutorTest, MixedDataTypes) {
    auto create = executeSQL("CREATE TABLE mixed ("
                           "int_col INTEGER, "
                           "bigint_col BIGINT, "
                           "double_col DOUBLE, "
                           "string_col VARCHAR(50)"
                           ")");
    ASSERT_TRUE(create.success());
    
    auto insert = executeSQL("INSERT INTO mixed (int_col, bigint_col, double_col, string_col) "
                           "VALUES (42, 1234567890123, 3.14159, 'Test String')");
    ASSERT_TRUE(insert.success());
    
    auto select = executeSQL("SELECT * FROM mixed");
    ASSERT_TRUE(select.success());
    ASSERT_TRUE(select.hasResultSet());
    
    auto* rs = select.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 42);
    EXPECT_EQ(rs->getValue(0, 1).toInt64(), 1234567890123);
    EXPECT_NEAR(rs->getValue(0, 2).toDouble(), 3.14159, 0.00001);
    EXPECT_EQ(rs->getValue(0, 3).toString(), "Test String");
}

TEST_F(ExecutorTest, ImplicitTextNumericComparisonInWhere) {
    ASSERT_TRUE(executeSQL("CREATE TABLE coercion_cmp (i INTEGER)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO coercion_cmp (i) VALUES (1)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO coercion_cmp (i) VALUES (2)").success());

    auto result = executeSQL("SELECT i FROM coercion_cmp WHERE i = '2'");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 2);
}

TEST_F(ExecutorTest, ImplicitTextNumericComparisonRejectsInvalidText) {
    ASSERT_TRUE(executeSQL("CREATE TABLE coercion_cmp_err (i INTEGER)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO coercion_cmp_err (i) VALUES (1)").success());

    auto result = executeSQL("SELECT i FROM coercion_cmp_err WHERE i = 'abc'");
    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find("Invalid text representation"), std::string::npos);
}

TEST_F(ExecutorTest, ImplicitTextNumericArithmeticInInsert) {
    ASSERT_TRUE(executeSQL("CREATE TABLE coercion_math (v DOUBLE)").success());

    auto ok_insert = executeSQL("INSERT INTO coercion_math (v) VALUES (2 + '3')");
    ASSERT_TRUE(ok_insert.success()) << ok_insert.error();

    auto bad_insert = executeSQL("INSERT INTO coercion_math (v) VALUES ('abc' + 1)");
    EXPECT_FALSE(bad_insert.success());
    EXPECT_NE(bad_insert.error().find("Invalid text representation"), std::string::npos);
}

TEST_F(ExecutorTest, ThreeValuedBooleanOperatorsFollowSqlSemantics) {
    auto result = executeSQL(
        "SELECT "
        "TRUE AND NULL AS and_true_null, "
        "FALSE AND NULL AS and_false_null, "
        "TRUE OR NULL AS or_true_null, "
        "FALSE OR NULL AS or_false_null, "
        "NOT NULL AS not_null_value");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    ASSERT_EQ(rs->columnCount(), 5u);

    EXPECT_TRUE(rs->getValue(0, 0).isNull());
    ASSERT_FALSE(rs->getValue(0, 1).isNull());
    EXPECT_FALSE(rs->getValue(0, 1).toBoolean());
    ASSERT_FALSE(rs->getValue(0, 2).isNull());
    EXPECT_TRUE(rs->getValue(0, 2).toBoolean());
    EXPECT_TRUE(rs->getValue(0, 3).isNull());
    EXPECT_TRUE(rs->getValue(0, 4).isNull());
}

TEST_F(ExecutorTest, IsDistinctFromUsesNullSafeEquality) {
    auto result = executeSQL(
        "SELECT "
        "NULL IS DISTINCT FROM NULL AS d1, "
        "1 IS DISTINCT FROM NULL AS d2, "
        "NULL IS NOT DISTINCT FROM NULL AS d3, "
        "1 IS NOT DISTINCT FROM NULL AS d4, "
        "'2' IS NOT DISTINCT FROM 2 AS d5");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    ASSERT_EQ(rs->columnCount(), 5u);

    ASSERT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_FALSE(rs->getValue(0, 0).toBoolean());
    ASSERT_FALSE(rs->getValue(0, 1).isNull());
    EXPECT_TRUE(rs->getValue(0, 1).toBoolean());
    ASSERT_FALSE(rs->getValue(0, 2).isNull());
    EXPECT_TRUE(rs->getValue(0, 2).toBoolean());
    ASSERT_FALSE(rs->getValue(0, 3).isNull());
    EXPECT_FALSE(rs->getValue(0, 3).toBoolean());
    ASSERT_FALSE(rs->getValue(0, 4).isNull());
    EXPECT_TRUE(rs->getValue(0, 4).toBoolean());
}

TEST_F(ExecutorTest, IsTrueAndIsFalseTreatNullAsFalseForPositivePredicate) {
    auto result = executeSQL(
        "SELECT "
        "NULL IS TRUE AS t1, "
        "NULL IS NOT TRUE AS t2, "
        "NULL IS FALSE AS t3, "
        "NULL IS NOT FALSE AS t4");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    ASSERT_EQ(rs->columnCount(), 4u);

    ASSERT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_FALSE(rs->getValue(0, 0).toBoolean());
    ASSERT_FALSE(rs->getValue(0, 1).isNull());
    EXPECT_TRUE(rs->getValue(0, 1).toBoolean());
    ASSERT_FALSE(rs->getValue(0, 2).isNull());
    EXPECT_FALSE(rs->getValue(0, 2).toBoolean());
    ASSERT_FALSE(rs->getValue(0, 3).isNull());
    EXPECT_TRUE(rs->getValue(0, 3).toBoolean());
}

TEST_F(ExecutorTest, IsUnknownFollowsNullPredicateSemantics) {
    auto result = executeSQL(
        "SELECT "
        "NULL IS UNKNOWN AS u1, "
        "NULL IS NOT UNKNOWN AS u2, "
        "TRUE IS UNKNOWN AS u3, "
        "TRUE IS NOT UNKNOWN AS u4");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    ASSERT_EQ(rs->columnCount(), 4u);

    ASSERT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_TRUE(rs->getValue(0, 0).toBoolean());
    ASSERT_FALSE(rs->getValue(0, 1).isNull());
    EXPECT_FALSE(rs->getValue(0, 1).toBoolean());
    ASSERT_FALSE(rs->getValue(0, 2).isNull());
    EXPECT_FALSE(rs->getValue(0, 2).toBoolean());
    ASSERT_FALSE(rs->getValue(0, 3).isNull());
    EXPECT_TRUE(rs->getValue(0, 3).toBoolean());
}

TEST_F(ExecutorTest, BitXorOperatorAndPrecedence) {
    auto result = executeSQL(
        "SELECT "
        "5 ^ 3 AS xor_value, "
        "1 | 2 ^ 3 & 7 AS precedence_value");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    ASSERT_EQ(rs->columnCount(), 2u);
    EXPECT_EQ(rs->getValue(0, 0).toInt64(), 6);
    EXPECT_EQ(rs->getValue(0, 1).toInt64(), 1);
}

TEST_F(ExecutorTest, TemporalArithmeticWithImplicitTextCoercion) {
    auto result = executeSQL(
        "SELECT "
        "'2024-01-01 00:00:00' + CAST('interval 1 day' AS INTERVAL) AS shifted");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    ASSERT_EQ(rs->columnCount(), 1u);
    EXPECT_FALSE(rs->getValue(0, 0).isNull());
    EXPECT_NE(rs->getValue(0, 0).toString().find("2024"), std::string::npos);
}

TEST_F(ExecutorTest, OperatorStrictModeDisablesImplicitCasts) {
    auto prewarm_temporal = executeSQL(
        "SELECT '2024-01-01 00:00:00' + CAST('interval 1 day' AS INTERVAL) AS strict_temporal");
    ASSERT_TRUE(prewarm_temporal.success()) << prewarm_temporal.error();

    auto set_on = executeSQL("SET operator.strict_mode = TRUE");
    ASSERT_TRUE(set_on.success()) << set_on.error();

    auto strict_numeric = executeSQL("SELECT 2 + '3' AS strict_numeric");
    EXPECT_FALSE(strict_numeric.success());
    EXPECT_NE(strict_numeric.error().find("Implicit casts disabled"), std::string::npos);

    auto strict_temporal = executeSQL(
        "SELECT '2024-01-01 00:00:00' + CAST('interval 1 day' AS INTERVAL) AS strict_temporal");
    EXPECT_FALSE(strict_temporal.success());
    EXPECT_NE(strict_temporal.error().find("Implicit casts disabled"), std::string::npos);

    auto set_off = executeSQL("SET operator.strict_mode = FALSE");
    ASSERT_TRUE(set_off.success()) << set_off.error();

    auto relaxed = executeSQL("SELECT 2 + '3' AS relaxed_numeric");
    ASSERT_TRUE(relaxed.success()) << relaxed.error();
    ASSERT_TRUE(relaxed.hasResultSet());
    auto* rs = relaxed.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_DOUBLE_EQ(rs->getValue(0, 0).toDouble(), 5.0);
}

TEST_F(ExecutorTest, OperatorStrictModeDisablesImplicitComparisonCasts) {
    auto set_on = executeSQL("SET operator.strict_mode = TRUE");
    ASSERT_TRUE(set_on.success()) << set_on.error();

    auto strict_compare = executeSQL("SELECT '5' = 5 AS strict_compare");
    EXPECT_FALSE(strict_compare.success());
    EXPECT_NE(strict_compare.error().find("Implicit casts disabled"), std::string::npos);

    auto set_off = executeSQL("SET operator.strict_mode = FALSE");
    ASSERT_TRUE(set_off.success()) << set_off.error();

    auto relaxed_compare = executeSQL("SELECT '5' = 5 AS relaxed_compare");
    ASSERT_TRUE(relaxed_compare.success()) << relaxed_compare.error();
    ASSERT_TRUE(relaxed_compare.hasResultSet());
    auto* rs = relaxed_compare.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_TRUE(rs->getValue(0, 0).toBoolean());
}

TEST_F(ExecutorTest, FunctionCallRejectsSignatureMismatchDeterministically) {
    CatalogManager::FunctionInfo fn{};
    fn.function_id = generateUuidV7();
    fn.schema_id = default_schema_id_;
    fn.name = "fn_sig_mismatch";
    fn.owner_id = system_user_id_;
    fn.return_type = DataType::INT64;
    fn.bytecode = scratchbird::testing::minimalCompiledStoredCodeBytecode("fn_sig_mismatch");
    fn.parameters.push_back({
        "p_value",
        DataType::INT64,
        0,
        0,
        CatalogManager::ParameterMode::IN,
        false,
        ""
    });

    ErrorContext reg_ctx;
    ASSERT_EQ(db_->catalog_manager()->registerFunction(fn, &reg_ctx), Status::OK)
        << reg_ctx.message;

    Executor executor(db_.get());
    executor.setConnectionContext(conn_ctx_.get());
    executor.setCurrentSchema(default_schema_id_);

    Value out;
    ErrorContext call_ctx;
    Status st = executor.callFunctionByName(
        "fn_sig_mismatch",
        {Value::makeVarchar("not_a_number")},
        out,
        &call_ctx);

    EXPECT_EQ(st, Status::DATATYPE_MISMATCH);
    EXPECT_EQ(call_ctx.code, Status::DATATYPE_MISMATCH);
    EXPECT_NE(std::string(call_ctx.message).find("expected fn_sig_mismatch(INT64)"), std::string::npos);
}

TEST_F(ExecutorTest, FunctionCallAllowsDeterministicNumericWidening) {
    CatalogManager::FunctionInfo fn{};
    fn.function_id = generateUuidV7();
    fn.schema_id = default_schema_id_;
    fn.name = "fn_sig_widen";
    fn.owner_id = system_user_id_;
    fn.return_type = DataType::INT64;
    fn.bytecode = scratchbird::testing::minimalCompiledStoredCodeBytecode("fn_sig_widen");
    fn.parameters.push_back({
        "p_value",
        DataType::INT64,
        0,
        0,
        CatalogManager::ParameterMode::IN,
        false,
        ""
    });

    ErrorContext reg_ctx;
    ASSERT_EQ(db_->catalog_manager()->registerFunction(fn, &reg_ctx), Status::OK)
        << reg_ctx.message;

    Executor executor(db_.get());
    executor.setConnectionContext(conn_ctx_.get());
    executor.setCurrentSchema(default_schema_id_);

    Value out;
    ErrorContext call_ctx;
    Status st = executor.callFunctionByName(
        "fn_sig_widen",
        {Value::makeInt32(42)},
        out,
        &call_ctx);

    EXPECT_EQ(st, Status::OK) << call_ctx.message;
}

TEST_F(ExecutorTest, FunctionCallUnknownSymbolReturnsUndefinedFunction) {
    Executor executor(db_.get());
    executor.setConnectionContext(conn_ctx_.get());
    executor.setCurrentSchema(default_schema_id_);

    Value out;
    ErrorContext call_ctx;
    Status st = executor.callFunctionByName(
        "fn_missing_symbol",
        {Value::makeInt32(1)},
        out,
        &call_ctx);

    EXPECT_EQ(st, Status::UNDEFINED_FUNCTION);
    EXPECT_EQ(call_ctx.code, Status::UNDEFINED_FUNCTION);
    EXPECT_NE(std::string(call_ctx.message).find("Function not found"), std::string::npos);
}

TEST_F(ExecutorTest, ProcedureCallRejectsSignatureMismatchDeterministically) {
    CatalogManager::ProcedureInfo proc{};
    proc.procedure_id = generateUuidV7();
    proc.schema_id = default_schema_id_;
    proc.name = "proc_sig_mismatch";
    proc.owner_id = system_user_id_;
    proc.bytecode = scratchbird::testing::minimalCompiledStoredCodeBytecode("proc_sig_mismatch");
    proc.parameters.push_back({
        "p_value",
        DataType::INT64,
        0,
        0,
        CatalogManager::ParameterMode::IN,
        false,
        ""
    });

    ErrorContext reg_ctx;
    ASSERT_EQ(db_->catalog_manager()->registerProcedure(proc, &reg_ctx), Status::OK)
        << reg_ctx.message;

    Executor executor(db_.get());
    executor.setConnectionContext(conn_ctx_.get());
    executor.setCurrentSchema(default_schema_id_);

    auto result = executor.callProcedureByName(
        "proc_sig_mismatch",
        {Value::makeVarchar("bad_input")});

    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find("expected proc_sig_mismatch(INT64)"), std::string::npos);
}

TEST_F(ExecutorTest, ProcedureCallAllowsDeterministicNumericWidening) {
    CatalogManager::ProcedureInfo proc{};
    proc.procedure_id = generateUuidV7();
    proc.schema_id = default_schema_id_;
    proc.name = "proc_sig_widen";
    proc.owner_id = system_user_id_;
    proc.bytecode = scratchbird::testing::minimalCompiledStoredCodeBytecode("proc_sig_widen");
    proc.parameters.push_back({
        "p_value",
        DataType::INT64,
        0,
        0,
        CatalogManager::ParameterMode::IN,
        false,
        ""
    });

    ErrorContext reg_ctx;
    ASSERT_EQ(db_->catalog_manager()->registerProcedure(proc, &reg_ctx), Status::OK)
        << reg_ctx.message;

    Executor executor(db_.get());
    executor.setConnectionContext(conn_ctx_.get());
    executor.setCurrentSchema(default_schema_id_);

    auto result = executor.callProcedureByName(
        "proc_sig_widen",
        {Value::makeInt32(7)});

    EXPECT_TRUE(result.success()) << result.error();
}

TEST_F(ExecutorTest, ProcedureCallRejectsArgumentCountMismatchDeterministically) {
    CatalogManager::ProcedureInfo proc{};
    proc.procedure_id = generateUuidV7();
    proc.schema_id = default_schema_id_;
    proc.name = "proc_sig_count";
    proc.owner_id = system_user_id_;
    proc.bytecode = scratchbird::testing::minimalCompiledStoredCodeBytecode("proc_sig_count");
    proc.parameters.push_back({
        "p_first",
        DataType::INT64,
        0,
        0,
        CatalogManager::ParameterMode::IN,
        false,
        ""
    });
    proc.parameters.push_back({
        "p_second",
        DataType::INT64,
        0,
        0,
        CatalogManager::ParameterMode::IN,
        false,
        ""
    });

    ErrorContext reg_ctx;
    ASSERT_EQ(db_->catalog_manager()->registerProcedure(proc, &reg_ctx), Status::OK)
        << reg_ctx.message;

    Executor executor(db_.get());
    executor.setConnectionContext(conn_ctx_.get());
    executor.setCurrentSchema(default_schema_id_);

    auto result = executor.callProcedureByName(
        "proc_sig_count",
        {Value::makeInt64(1)});

    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find("expected proc_sig_count(INT64, INT64)"), std::string::npos);
}

TEST_F(ExecutorTest, UdrBoundaryRejectsMissingExecutePermissionDeterministically) {
    const ID udr_id = createUdrFunction("udr_perm_reject", "udr/perm_reject.so");

    ErrorContext ctx;
    ID viewer_id{};
    ASSERT_EQ(db_->catalog_manager()->createUser("udr_viewer_perm", "", default_schema_id_, false,
                                                 viewer_id, &ctx),
              Status::OK) << ctx.message;

    // Remove every execute-grant row for this UDR (user/role/group/public)
    // so the boundary test is deterministic.
    for (int pass = 0; pass < 8; ++pass)
    {
        std::vector<CatalogManager::ObjectPermissionInfo> perms;
        ASSERT_EQ(db_->catalog_manager()->getObjectPermissions(udr_id, perms, &ctx), Status::OK)
            << ctx.message;
        if (perms.empty())
        {
            break;
        }

        for (const auto& perm : perms)
        {
            auto revoke_status = db_->catalog_manager()->revokeObjectPermission(
                udr_id, perm.grantee_id, &ctx);
            ASSERT_TRUE(revoke_status == Status::OK || revoke_status == Status::NOT_FOUND)
                << ctx.message;
        }
    }

    const bool has_execute = db_->catalog_manager()->hasObjectPermission(
        udr_id, viewer_id, CatalogManager::PERM_EXECUTE, &ctx);
    EXPECT_FALSE(has_execute);

    conn_ctx_->pushSecurityContext(
        viewer_id,
        ID{},
        false,
        ConnectionContext::SecurityMode::INVOKER,
        udr_id);

    Executor executor(db_.get());
    executor.setConnectionContext(conn_ctx_.get());
    executor.setCurrentSchema(default_schema_id_);

    ErrorContext call_ctx;
    Status st = invokeUdrBoundary(executor,
                                  udr_id,
                                  Executor::UdrInvocationScope::FUNCTION,
                                  {},
                                  &call_ctx);
    EXPECT_EQ(st, Status::PERMISSION_DENIED);
    EXPECT_EQ(call_ctx.code, Status::PERMISSION_DENIED);
    EXPECT_EQ(call_ctx.vnext_code, "UDR_1507");
    conn_ctx_->popSecurityContext();
}

TEST_F(ExecutorTest, UdrBoundaryRejectsSandboxPathDeterministically) {
    const ID udr_id = createUdrFunction("udr_sandbox_reject", "../escape/udr.so");

    Executor executor(db_.get());
    executor.setConnectionContext(conn_ctx_.get());
    executor.setCurrentSchema(default_schema_id_);

    ErrorContext call_ctx;
    Status st = invokeUdrBoundary(executor,
                                  udr_id,
                                  Executor::UdrInvocationScope::PROCEDURE,
                                  {},
                                  &call_ctx);
    EXPECT_EQ(st, Status::PERMISSION_DENIED);
    EXPECT_EQ(call_ctx.code, Status::PERMISSION_DENIED);
    EXPECT_EQ(call_ctx.vnext_code, "UDR_1508");
}

TEST_F(ExecutorTest, UdrBoundaryRejectsInvocationQuotaDeterministically) {
    const ID udr_id = createUdrFunction("udr_quota_reject", "udr/quota_reject.so");

    Executor executor(db_.get());
    executor.setConnectionContext(conn_ctx_.get());
    executor.setCurrentSchema(default_schema_id_);

    ErrorContext call_ctx;
    std::vector<Value> args;
    args.push_back(Value::makeVarchar(std::string(1024 * 1024 + 8, 'q')));

    Status st = invokeUdrBoundary(executor,
                                  udr_id,
                                  Executor::UdrInvocationScope::TRIGGER,
                                  args,
                                  &call_ctx);
    EXPECT_EQ(st, Status::CONFIGURATION_LIMIT_EXCEEDED);
    EXPECT_EQ(call_ctx.code, Status::CONFIGURATION_LIMIT_EXCEEDED);
    EXPECT_EQ(call_ctx.vnext_code, "UDR_1512");
}

TEST_F(ExecutorTest, UdrBoundaryExecutesConfiguredRuntimeModuleDeterministically) {
    const std::string library_path = "udr/runtime_dispatch.so";
    const std::string entry_point = "udr_arg_count";
    registerRuntimeReadyUdrModule("runtime_native_engine",
                                  "runtime_dispatch_module",
                                  library_path,
                                  entry_point);

    const ID udr_id = createUdrFunction("udr_runtime_dispatch", library_path, entry_point);

    Executor executor(db_.get());
    executor.setConnectionContext(conn_ctx_.get());
    executor.setCurrentSchema(default_schema_id_);

    ErrorContext call_ctx;
    Value out{};
    Status st = invokeUdrBoundary(executor,
                                  udr_id,
                                  Executor::UdrInvocationScope::FUNCTION,
                                  {Value::makeInt32(7), Value::makeVarchar("x"), Value::makeNull()},
                                  &call_ctx,
                                  &out);
    EXPECT_EQ(st, Status::OK) << call_ctx.message;
    EXPECT_EQ(out.toInt64(), 3);

    ErrorContext sp_ctx;
    EXPECT_EQ(conn_ctx_->createSavepoint("post_udr_runtime_ok", &sp_ctx), Status::OK) << sp_ctx.message;
    EXPECT_EQ(conn_ctx_->releaseSavepoint("post_udr_runtime_ok", &sp_ctx), Status::OK) << sp_ctx.message;
}

TEST_F(ExecutorTest, UdrBoundaryRejectsUnconfiguredRuntimeIncludesScopeAndKeepsTransactionUsable) {
    const ID udr_id = createUdrFunction("udr_runtime_missing", "udr/runtime_missing.so");

    Executor executor(db_.get());
    executor.setConnectionContext(conn_ctx_.get());
    executor.setCurrentSchema(default_schema_id_);

    struct ScopeExpectation {
        Executor::UdrInvocationScope scope;
        const char* label;
    };
    const std::vector<ScopeExpectation> scopes{
        {Executor::UdrInvocationScope::FUNCTION, "FUNCTION"},
        {Executor::UdrInvocationScope::PROCEDURE, "PROCEDURE"},
        {Executor::UdrInvocationScope::TRIGGER, "TRIGGER"},
    };

    for (size_t i = 0; i < scopes.size(); ++i)
    {
        ErrorContext call_ctx;
        SCOPED_TRACE(scopes[i].label);
        Status st = invokeUdrBoundary(executor, udr_id, scopes[i].scope, {}, &call_ctx);
        EXPECT_EQ(st, Status::NOT_FOUND);
        EXPECT_EQ(call_ctx.code, Status::NOT_FOUND);
        EXPECT_EQ(call_ctx.vnext_code, "UDR_1502");
        EXPECT_NE(call_ctx.message.find(std::string("scope=") + scopes[i].label), std::string::npos);

        ErrorContext sp_ctx;
        std::string savepoint_name = "post_udr_boundary_" + std::to_string(i);
        EXPECT_EQ(conn_ctx_->createSavepoint(savepoint_name, &sp_ctx), Status::OK) << sp_ctx.message;
        EXPECT_EQ(conn_ctx_->releaseSavepoint(savepoint_name, &sp_ctx), Status::OK) << sp_ctx.message;
    }
}
