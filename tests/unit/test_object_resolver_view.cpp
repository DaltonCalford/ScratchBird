/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "unit/test_user_helpers.h"

#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace core = scratchbird::core;
namespace sblr = scratchbird::sblr;

namespace {

std::string generateUniqueDbPath() {
    std::ostringstream oss;
    oss << "/tmp/test_object_resolver_view_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << ".sbdb";
    return oss.str();
}

std::string objectTypeToString(core::CatalogManager::ObjectType type) {
    switch (type) {
        case core::CatalogManager::ObjectType::SCHEMA: return "SCHEMA";
        case core::CatalogManager::ObjectType::TABLE: return "TABLE";
        case core::CatalogManager::ObjectType::COLUMN: return "COLUMN";
        case core::CatalogManager::ObjectType::INDEX: return "INDEX";
        case core::CatalogManager::ObjectType::VIEW: return "VIEW";
        case core::CatalogManager::ObjectType::SEQUENCE: return "SEQUENCE";
        case core::CatalogManager::ObjectType::CONSTRAINT: return "CONSTRAINT";
        case core::CatalogManager::ObjectType::TRIGGER: return "TRIGGER";
        case core::CatalogManager::ObjectType::PROCEDURE: return "PROCEDURE";
        case core::CatalogManager::ObjectType::FUNCTION: return "FUNCTION";
        case core::CatalogManager::ObjectType::DOMAIN: return "DOMAIN";
        case core::CatalogManager::ObjectType::PACKAGE: return "PACKAGE";
        case core::CatalogManager::ObjectType::UDR: return "UDR";
        case core::CatalogManager::ObjectType::EXCEPTION: return "EXCEPTION";
        case core::CatalogManager::ObjectType::SYNONYM: return "SYNONYM";
        case core::CatalogManager::ObjectType::FOREIGN_TABLE: return "FOREIGN_TABLE";
        case core::CatalogManager::ObjectType::ROLE: return "ROLE";
        case core::CatalogManager::ObjectType::USER: return "USER";
        case core::CatalogManager::ObjectType::GROUP: return "GROUP";
        case core::CatalogManager::ObjectType::TABLESPACE: return "TABLESPACE";
        default: return "UNKNOWN";
    }
}

std::string makeRowKey(const std::vector<std::string>& columns) {
    std::string out;
    out.reserve(256);
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) {
            out.push_back('\x1f');
        }
        out.append(columns[i]);
    }
    return out;
}

}  // namespace

class ObjectResolverViewTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = generateUniqueDbPath();
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(test_db_path_, 16384, &ctx), core::Status::OK)
            << ctx.message;
        ASSERT_EQ(db_.open(test_db_path_, &ctx), core::Status::OK) << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        EnsureUser(catalog_, "test_user", core::ID{}, true);
        ASSERT_EQ(catalog_->createSchema("test", "test_user", test_schema_id_, &ctx),
                  core::Status::OK)
            << ctx.message;

        ASSERT_EQ(db_.connect(conn_, &ctx), core::Status::OK) << ctx.message;
        const core::ID system_user_id = catalog_->getSystemUserId(&ctx);
        conn_->setCurrentUser(system_user_id, true);
        conn_->applyStagedSecurityContext();
        core::ConnectionContext::setCurrent(conn_.get());
        conn_->setCurrentSchemaId(test_schema_id_);

        compiler_ = std::make_unique<sblr::QueryCompilerV3>(&db_);
        compiler_->setCurrentSchema(test_schema_id_);

        executor_ = std::make_unique<sblr::Executor>(&db_);
        executor_->setConnectionContext(conn_.get());
    }

    void TearDown() override {
        core::ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        executor_.reset();
        compiler_.reset();
        db_.close();
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");
    }

    core::ID createTable(const std::string& name) {
        core::ErrorContext ctx;
        std::vector<core::CatalogManager::ColumnInfo> columns;
        core::CatalogManager::ColumnInfo col;
        col.column_name = "id";
        col.data_type = static_cast<uint16_t>(core::DataType::INT32);
        col.max_length = 4;
        col.nullable = false;
        columns.push_back(col);

        core::ID table_id;
        auto status = catalog_->createTable(test_schema_id_, name, columns, table_id, 0, &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;
        return table_id;
    }

    core::ID createView(const std::string& name) {
        core::ErrorContext ctx;
        auto status = catalog_->createView(test_schema_id_, name, "select 1", false, false, false,
                                           {}, core::ID{}, &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;

        core::CatalogManager::ViewInfo info;
        status = catalog_->getView(test_schema_id_, name, info, &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;
        return info.view_id;
    }

    core::ID createSequence(const std::string& name) {
        core::ErrorContext ctx;
        auto status = catalog_->createSequence(test_schema_id_, name, 1, 1, 1000, 1, 1, false, &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;

        core::ID sequence_id;
        status = catalog_->getSequenceIdByName(test_schema_id_, name, sequence_id, &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;
        return sequence_id;
    }

    core::ID createFunction(const std::string& name) {
        core::ErrorContext ctx;
        core::CatalogManager::FunctionInfo info;
        info.function_id = core::generateUuidV7();
        info.schema_id = test_schema_id_;
        info.name = name;
        info.owner_id = catalog_->getSystemUserId(&ctx);
        info.return_type = core::DataType::INT32;
        info.or_replace = false;
        info.deterministic = false;
        info.sql_security = core::CatalogManager::FunctionInfo::SqlSecurity::DEFINER;
        info.source_text = "return 1;";
        info.created_time = 0;
        info.modified_time = 0;

        auto status = catalog_->registerFunction(info, &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;
        return info.function_id;
    }

    core::ID createProcedure(const std::string& name) {
        core::ErrorContext ctx;
        core::CatalogManager::ProcedureInfo info;
        info.procedure_id = core::generateUuidV7();
        info.schema_id = test_schema_id_;
        info.name = name;
        info.owner_id = catalog_->getSystemUserId(&ctx);
        info.or_replace = false;
        info.sql_security = core::CatalogManager::ProcedureInfo::SqlSecurity::DEFINER;
        info.source_text = "begin end";
        info.created_time = 0;
        info.modified_time = 0;

        auto status = catalog_->registerProcedure(info, &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;
        return info.procedure_id;
    }

    sblr::ExecutionResult compileAndExecute(const std::string& sql) {
        auto compile_result = compiler_->compile(sql);
        EXPECT_TRUE(compile_result.success()) << "Compilation failed: " << sql;
        if (!compile_result.success()) {
            return sblr::ExecutionResult("Compilation failed");
        }
        return executor_->execute(compile_result.bytecode());
    }

    std::string test_db_path_;
    core::Database db_;
    core::CatalogManager* catalog_ = nullptr;
    core::ID test_schema_id_;
    std::unique_ptr<core::ConnectionContext> conn_;
    std::unique_ptr<sblr::QueryCompilerV3> compiler_;
    std::unique_ptr<sblr::Executor> executor_;
};

TEST_F(ObjectResolverViewTest, ViewMatchesResolverCache) {
    createTable("widgets");
    createView("widgets_view");
    createSequence("widgets_seq");
    createFunction("widgets_fn");
    createProcedure("widgets_proc");

    core::CatalogManager::ResolveFilter filter;
    std::vector<core::CatalogManager::ResolvedObject> objects;
    core::ErrorContext ctx;
    ASSERT_EQ(catalog_->listResolvedObjects(filter, objects, &ctx), core::Status::OK)
        << ctx.message;

    auto result = compileAndExecute(
        "SELECT object_id, object_type, schema_path, full_path, object_name, dialect_tag, compat_name "
        "FROM sys.catalog.object_resolver");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->columnCount(), 7u);

    std::unordered_set<std::string> actual_rows;
    actual_rows.reserve(rs->rowCount());

    for (size_t row = 0; row < rs->rowCount(); ++row) {
        std::vector<std::string> cols;
        cols.reserve(7);
        for (size_t col = 0; col < rs->columnCount(); ++col) {
            const auto& value = rs->getValue(row, col);
            cols.push_back(value.isNull() ? "" : value.getVarchar());
        }
        actual_rows.insert(makeRowKey(cols));
    }

    std::unordered_set<std::string> expected_rows;
    expected_rows.reserve(objects.size());

    for (const auto& obj : objects) {
        std::vector<std::string> cols = {
            obj.object_id.toString(),
            objectTypeToString(obj.object_type),
            obj.schema_path,
            obj.full_path,
            obj.object_name,
            obj.dialect_tag,
            obj.compat_name
        };
        expected_rows.insert(makeRowKey(cols));
    }

    EXPECT_EQ(actual_rows.size(), expected_rows.size());
    for (const auto& row : expected_rows) {
        EXPECT_TRUE(actual_rows.find(row) != actual_rows.end());
    }
}

TEST_F(ObjectResolverViewTest, ResolverSurvivesDroppedPrimaryKeyTable)
{
    auto create_result = compileAndExecute(
        "CREATE TABLE resolver_pk (id INTEGER PRIMARY KEY, payload VARCHAR(8))");
    ASSERT_TRUE(create_result.success()) << create_result.error();

    auto drop_result = compileAndExecute("DROP TABLE resolver_pk");
    ASSERT_TRUE(drop_result.success()) << drop_result.error();

    core::CatalogManager::ResolveFilter filter;
    std::vector<core::CatalogManager::ResolvedObject> objects;
    core::ErrorContext ctx;
    EXPECT_EQ(catalog_->listResolvedObjects(filter, objects, &ctx), core::Status::OK)
        << ctx.message;
}
