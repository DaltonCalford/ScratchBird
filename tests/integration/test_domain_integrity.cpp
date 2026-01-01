#include <gtest/gtest.h>
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include <chrono>
#include <filesystem>
#include <sstream>
#include <thread>

using namespace scratchbird::core;
using namespace scratchbird::sblr;

namespace
{
    std::string generateUniqueDbPath()
    {
        std::ostringstream oss;
        oss << "/tmp/test_domain_integrity_"
            << std::this_thread::get_id() << "_"
            << std::chrono::steady_clock::now().time_since_epoch().count()
            << ".sbdb";
        return oss.str();
    }
}

class DomainIntegrityIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = generateUniqueDbPath();
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        domain_mgr_ = db_.domain_manager();
        ASSERT_NE(domain_mgr_, nullptr);

        CatalogManager::SchemaInfo schema_info;
        status = catalog_->getSchema("PUBLIC", schema_info, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
        schema_id_ = schema_info.schema_id;

        status = domain_mgr_->createBasicDomain(schema_id_, "global_unique_text", DataType::TEXT,
                                                0, 0, false, "", {}, domain_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        DomainIntegrity integrity;
        integrity.uniqueness_check = true;
        status = domain_mgr_->setIntegrityOptions(domain_id_, integrity, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        auto columns_one = buildColumns();
        status = catalog_->createTable(schema_id_, "domain_table_one", columns_one, table_one_id_, 0, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        auto columns_two = buildColumns();
        status = catalog_->createTable(schema_id_, "domain_table_two", columns_two, table_two_id_, 0, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        compiler_ = std::make_unique<QueryCompilerV2>(&db_);
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<Executor>(&db_);
    }

    void TearDown() override
    {
        compiler_.reset();
        executor_.reset();
        db_.close();
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");
    }

    ExecutionResult compileAndExecute(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            std::string errors;
            for (const auto& err : compile_result.errors())
            {
                errors += err + "\n";
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    std::vector<CatalogManager::ColumnInfo> buildColumns()
    {
        CatalogManager::ColumnInfo id_col;
        id_col.column_id = generateUuidV7();
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.nullable = false;
        id_col.ordinal = 0;

        CatalogManager::ColumnInfo val_col;
        val_col.column_id = generateUuidV7();
        val_col.column_name = "val";
        val_col.data_type = static_cast<uint16_t>(DataType::TEXT);
        val_col.nullable = false;
        val_col.ordinal = 1;
        val_col.domain_id = domain_id_;

        return {id_col, val_col};
    }

    std::string test_db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    DomainManager* domain_mgr_ = nullptr;
    ID schema_id_;
    ID domain_id_;
    ID table_one_id_;
    ID table_two_id_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
};

TEST_F(DomainIntegrityIntegrationTest, RejectsDuplicateDomainValuesAcrossTables)
{
    auto insert_one = compileAndExecute(
        "INSERT INTO domain_table_one (id, val) VALUES (1, 'alpha')");
    ASSERT_TRUE(insert_one.success()) << insert_one.error();

    auto duplicate = compileAndExecute(
        "INSERT INTO domain_table_two (id, val) VALUES (1, 'alpha')");
    ASSERT_FALSE(duplicate.success());
    EXPECT_NE(duplicate.error().find("Domain uniqueness"), std::string::npos);
}

TEST_F(DomainIntegrityIntegrationTest, AllowsReuseAfterDelete)
{
    auto insert_one = compileAndExecute(
        "INSERT INTO domain_table_one (id, val) VALUES (1, 'alpha')");
    ASSERT_TRUE(insert_one.success()) << insert_one.error();

    auto insert_two = compileAndExecute(
        "INSERT INTO domain_table_two (id, val) VALUES (1, 'beta')");
    ASSERT_TRUE(insert_two.success()) << insert_two.error();

    auto update_conflict = compileAndExecute(
        "UPDATE domain_table_two SET val = 'alpha' WHERE id = 1");
    ASSERT_FALSE(update_conflict.success());

    auto delete_one = compileAndExecute(
        "DELETE FROM domain_table_one WHERE id = 1");
    ASSERT_TRUE(delete_one.success()) << delete_one.error();

    auto update_ok = compileAndExecute(
        "UPDATE domain_table_two SET val = 'alpha' WHERE id = 1");
    ASSERT_TRUE(update_ok.success()) << update_ok.error();
}
