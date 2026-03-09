#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using scratchbird::core::CatalogManager;
using scratchbird::core::ConnectionContext;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::ID;
using scratchbird::core::Status;
using scratchbird::sblr::ExecutionResult;
using scratchbird::sblr::Executor;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::sblr::ResultSet;

namespace {

std::string normalizePath(const std::string& path)
{
    return std::filesystem::absolute(path).lexically_normal().string();
}

void removeIfExists(const std::string& path)
{
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} // namespace

class BackupSqlAdminApiTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("test_backup_sql_admin_api", ".sbdb");
        backup_path_ = scratchbird::testing::uniqueTestDbPath("test_backup_sql_admin_api_backup",
                                                              ".sbkp");
        restore_path_ = scratchbird::testing::uniqueTestDbPath("test_backup_sql_admin_api_restore",
                                                               ".sbdb");
        validation_restore_path_ =
            scratchbird::testing::uniqueTestDbPath("test_backup_sql_admin_api_validate", ".sbdb");

        removeArtifacts();

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 8192, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK) << ctx.message;

        Status proc_status = db_->initializeProcArray(8, &ctx);
        if (proc_status != Status::OK && proc_status != Status::INVALID_ARGUMENT)
        {
            ASSERT_EQ(proc_status, Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_ctx_.get());
        ASSERT_EQ(conn_ctx_->initialize(&ctx), Status::OK) << ctx.message;

        system_user_id_ = db_->catalog_manager()->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user_id_, true);

        std::vector<CatalogManager::SchemaInfo> schemas;
        ASSERT_EQ(db_->catalog_manager()->listSchemas(schemas, &ctx), Status::OK) << ctx.message;
        ASSERT_FALSE(schemas.empty());
        default_schema_id_ = schemas.front().schema_id;

        ensurePrimaryPages(4);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        db_.reset();
        removeArtifacts();
    }

    void ensurePrimaryPages(uint32_t minimum_pages)
    {
        ErrorContext ctx;
        auto* page_mgr = db_->page_manager();
        ASSERT_NE(page_mgr, nullptr);

        while (db_->total_pages() < minimum_pages)
        {
            uint32_t page_id = 0;
            ASSERT_EQ(page_mgr->allocatePage(page_id, &ctx), Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_->sync(&ctx), Status::OK) << ctx.message;
    }

    std::vector<uint8_t> compileSQL(const std::string& sql)
    {
        QueryCompilerV3 compiler(db_.get());
        compiler.setCurrentSchema(default_schema_id_);
        auto result = compiler.compile(sql);
        if (!result.success())
        {
            ADD_FAILURE() << "Compile failed for SQL [" << sql << "]";
            if (!result.errors().empty())
            {
                ADD_FAILURE() << result.errors().front();
            }
            return {};
        }
        return result.bytecode();
    }

    ExecutionResult executeSQL(const std::string& sql)
    {
        auto bytecode = compileSQL(sql);
        if (bytecode.empty())
        {
            return ExecutionResult("Failed to compile SQL");
        }

        Executor executor(db_.get());
        executor.setConnectionContext(conn_ctx_.get());
        executor.setCurrentSchema(default_schema_id_);
        return executor.execute(bytecode);
    }

    ResultSet* requireSingleRowResult(const ExecutionResult& result)
    {
        EXPECT_TRUE(result.success()) << result.error();
        EXPECT_TRUE(result.hasResultSet());
        auto* rs = result.resultSet();
        EXPECT_NE(rs, nullptr);
        if (rs != nullptr)
        {
            EXPECT_EQ(rs->rowCount(), 1u);
        }
        return rs;
    }

    void createTrackedTable(const std::string& table_name)
    {
        ExecutionResult create_result =
            executeSQL("CREATE TABLE " + table_name + " (id INTEGER NOT NULL)");
        ASSERT_TRUE(create_result.success()) << create_result.error();
    }

    CatalogManager::TableInfo lookupTable(const ID& schema_id, const std::string& table_name)
    {
        CatalogManager::TableInfo table_info;
        ErrorContext ctx;
        EXPECT_EQ(db_->catalog_manager()->getTable(schema_id, table_name, table_info, &ctx),
                  Status::OK)
            << ctx.message;
        return table_info;
    }

    void removeArtifacts()
    {
        removeIfExists(test_db_path_);
        removeIfExists(backup_path_);
        removeIfExists(backup_path_ + ".part");
        removeIfExists(backup_path_ + ".sbkjob");
        removeIfExists(restore_path_);
        removeIfExists(validation_restore_path_);
    }

    std::string test_db_path_;
    std::string backup_path_;
    std::string restore_path_;
    std::string validation_restore_path_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    ID default_schema_id_{};
    ID system_user_id_{};
};

TEST_F(BackupSqlAdminApiTest, BackupServiceChannelPauseResumeAndReportProgress)
{
    const std::string normalized_backup_path = normalizePath(backup_path_);

    ExecutionResult paused = executeSQL(
        "BACKUP DATABASE '" + backup_path_ +
        "' WITH (COMPRESSION=NONE, LABEL='sql-paused', MAX_PAGES_PER_INVOCATION=1)");
    ResultSet* paused_rs = requireSingleRowResult(paused);
    ASSERT_NE(paused_rs, nullptr);

    const std::string paused_job_id = paused_rs->getValue(0, 0).toString();
    EXPECT_FALSE(paused_job_id.empty());
    EXPECT_EQ(paused_rs->getValue(0, 1).toString(), "PAUSED");
    EXPECT_LT(paused_rs->getValue(0, 2).toDouble(), 100.0);
    EXPECT_EQ(paused_rs->getValue(0, 5).toString(), normalized_backup_path);
    EXPECT_TRUE(std::filesystem::exists(backup_path_ + ".sbkjob"));

    ExecutionResult progress = executeSQL("SERVICE CHANNEL PROGRESS '" + backup_path_ + "'");
    ResultSet* progress_rs = requireSingleRowResult(progress);
    ASSERT_NE(progress_rs, nullptr);
    EXPECT_EQ(progress_rs->getValue(0, 0).toString(), paused_job_id);
    EXPECT_EQ(progress_rs->getValue(0, 1).toString(), "PAUSED");
    EXPECT_EQ(progress_rs->getValue(0, 5).toString(), normalized_backup_path);

    ExecutionResult resumed = executeSQL("SERVICE CHANNEL BACKUP '" + backup_path_ + "'");
    ResultSet* resumed_rs = requireSingleRowResult(resumed);
    ASSERT_NE(resumed_rs, nullptr);
    EXPECT_EQ(resumed_rs->getValue(0, 0).toString(), paused_job_id);
    EXPECT_EQ(resumed_rs->getValue(0, 1).toString(), "COMPLETED");
    EXPECT_DOUBLE_EQ(resumed_rs->getValue(0, 2).toDouble(), 100.0);
    EXPECT_EQ(resumed_rs->getValue(0, 5).toString(), normalized_backup_path);

    ExecutionResult final_progress = executeSQL("SERVICE CHANNEL PROGRESS '" + backup_path_ + "'");
    ResultSet* final_progress_rs = requireSingleRowResult(final_progress);
    ASSERT_NE(final_progress_rs, nullptr);
    EXPECT_EQ(final_progress_rs->getValue(0, 0).toString(), paused_job_id);
    EXPECT_EQ(final_progress_rs->getValue(0, 1).toString(), "COMPLETED");
    EXPECT_DOUBLE_EQ(final_progress_rs->getValue(0, 2).toDouble(), 100.0);

    EXPECT_TRUE(std::filesystem::exists(backup_path_));
    EXPECT_TRUE(std::filesystem::exists(backup_path_ + ".sbkjob"));
    EXPECT_FALSE(std::filesystem::exists(backup_path_ + ".part"));
}

TEST_F(BackupSqlAdminApiTest, RestorePreservesDatabaseAndCatalogIdentity)
{
    createTrackedTable("restore_items");

    ErrorContext ctx;
    const ID original_database_id = db_->uuid();
    CatalogManager::TableInfo original_table;
    ASSERT_EQ(db_->catalog_manager()->getTable(default_schema_id_,
                                               "restore_items",
                                               original_table,
                                               &ctx),
              Status::OK)
        << ctx.message;

    ExecutionResult backup = executeSQL(
        "BACKUP DATABASE '" + backup_path_ + "' WITH (COMPRESSION=NONE, LABEL='sql-restore')");
    ResultSet* backup_rs = requireSingleRowResult(backup);
    ASSERT_NE(backup_rs, nullptr);
    EXPECT_EQ(backup_rs->getValue(0, 1).toString(), "COMPLETED");

    ExecutionResult restore = executeSQL(
        "RESTORE DATABASE FROM '" + backup_path_ + "' TO '" + restore_path_ +
        "' WITH (IDENTITY_MODE=PRESERVE_UUIDS, VERIFY_CHECKSUMS=TRUE)");
    ResultSet* restore_rs = requireSingleRowResult(restore);
    ASSERT_NE(restore_rs, nullptr);
    EXPECT_EQ(restore_rs->getValue(0, 1).toString(), "COMPLETED");
    EXPECT_EQ(restore_rs->getValue(0, 5).toString(), normalizePath(restore_path_));

    ASSERT_TRUE(std::filesystem::exists(restore_path_));

    ConnectionContext::setCurrent(nullptr);
    conn_ctx_.reset();
    db_.reset();

    Database restored_db;
    ASSERT_EQ(restored_db.open(restore_path_, &ctx), Status::OK) << ctx.message;

    CatalogManager::TableInfo restored_table;
    ASSERT_EQ(restored_db.catalog_manager()->getTable(default_schema_id_,
                                                      "restore_items",
                                                      restored_table,
                                                      &ctx),
              Status::OK)
        << ctx.message;

    EXPECT_EQ(restored_db.uuid(), original_database_id);
    EXPECT_EQ(restored_db.catalog_manager()->getSystemUserId(&ctx), system_user_id_);
    EXPECT_EQ(restored_table.table_id, original_table.table_id);

    restored_db.close();
}

TEST_F(BackupSqlAdminApiTest, RestoreAsNewFailsClosedUntilUuidRekeyExists)
{
    ExecutionResult backup = executeSQL(
        "BACKUP DATABASE '" + backup_path_ + "' WITH (COMPRESSION=NONE, LABEL='sql-new-db')");
    ResultSet* backup_rs = requireSingleRowResult(backup);
    ASSERT_NE(backup_rs, nullptr);
    EXPECT_EQ(backup_rs->getValue(0, 1).toString(), "COMPLETED");

    ExecutionResult restore = executeSQL(
        "RESTORE DATABASE FROM '" + backup_path_ + "' TO '" + restore_path_ +
        "' WITH (IDENTITY_MODE=NEW_DATABASE)");

    EXPECT_FALSE(restore.success());
    EXPECT_NE(restore.error().find("dedicated UUID rekey lane"), std::string::npos)
        << restore.error();
    EXPECT_FALSE(std::filesystem::exists(restore_path_));
}

TEST_F(BackupSqlAdminApiTest, ValidateBackupReturnsDiagnosticReportShape)
{
    ExecutionResult backup = executeSQL(
        "BACKUP DATABASE '" + backup_path_ + "' WITH (COMPRESSION=NONE, LABEL='sql-validate')");
    ResultSet* backup_rs = requireSingleRowResult(backup);
    ASSERT_NE(backup_rs, nullptr);
    EXPECT_EQ(backup_rs->getValue(0, 1).toString(), "COMPLETED");

    ExecutionResult validate =
        executeSQL("VALIDATE DATABASE BACKUP FROM '" + backup_path_ + "'");
    ResultSet* validate_rs = requireSingleRowResult(validate);
    ASSERT_NE(validate_rs, nullptr);

    EXPECT_FALSE(validate_rs->getValue(0, 0).toString().empty());
    EXPECT_EQ(validate_rs->getValue(0, 1).toString(), "INFO");
    EXPECT_EQ(validate_rs->getValue(0, 2).toInt64(), 0);
    EXPECT_NE(validate_rs->getValue(0, 3).toString().find("validated successfully"),
              std::string::npos);
    EXPECT_EQ(validate_rs->getValue(0, 4).toString(), normalizePath(backup_path_));
}

TEST_F(BackupSqlAdminApiTest, ValidateRestoreReturnsPreservedUuidDiagnostic)
{
    createTrackedTable("validate_restore_items");

    ExecutionResult backup = executeSQL(
        "BACKUP DATABASE '" + backup_path_ + "' WITH (COMPRESSION=NONE, LABEL='sql-validate-restore')");
    ResultSet* backup_rs = requireSingleRowResult(backup);
    ASSERT_NE(backup_rs, nullptr);
    EXPECT_EQ(backup_rs->getValue(0, 1).toString(), "COMPLETED");

    ExecutionResult validate = executeSQL(
        "VALIDATE DATABASE RESTORE FROM '" + backup_path_ + "' TO '" + validation_restore_path_ +
        "' WITH (MAX_RESTORE_MICROS=5000000, MAX_RPO_MICROS=60000000)");
    ResultSet* validate_rs = requireSingleRowResult(validate);
    ASSERT_NE(validate_rs, nullptr);

    EXPECT_EQ(validate_rs->getValue(0, 1).toString(), "INFO");
    EXPECT_EQ(validate_rs->getValue(0, 2).toInt64(), 0);
    EXPECT_NE(validate_rs->getValue(0, 3).toString().find("preserved database UUID"),
              std::string::npos);
    EXPECT_EQ(validate_rs->getValue(0, 4).toString(), normalizePath(validation_restore_path_));
    EXPECT_TRUE(std::filesystem::exists(validation_restore_path_));
}
