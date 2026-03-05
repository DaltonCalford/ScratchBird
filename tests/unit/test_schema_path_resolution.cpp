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
#include "scratchbird/core/error_context.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;
using ObjectType = CatalogManager::ObjectType;

namespace {

}  // namespace

class SchemaPathResolutionTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database* db_ = nullptr;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID system_user_id_{};

    void SetUp() override
    {
        test_db_path_ = "/tmp/test_schema_path_" + std::to_string(getpid()) + ".db";
        std::remove(test_db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 16384, &ctx), Status::OK);

        db_ = new Database();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK);

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK);
        ConnectionContext::setCurrent(conn_.get());

        system_user_id_ = catalog_->getSystemUserId(&ctx);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_) {
            db_->close();
            delete db_;
            db_ = nullptr;
            catalog_ = nullptr;
        }
        std::remove(test_db_path_.c_str());
    }

    CatalogManager::ColumnInfo makeColumn(const std::string& name) const
    {
        CatalogManager::ColumnInfo col;
        col.column_name = name;
        col.data_type = static_cast<uint16_t>(DataType::INT32);
        col.max_length = 4;
        col.nullable = false;
        return col;
    }

    ID createSchemaPath(const std::string& path)
    {
        ErrorContext ctx;
        ID schema_id;
        auto status = catalog_->createSchemaPath(path, CatalogManager::SchemaType::APPLICATION,
                                                 schema_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return schema_id;
    }

    ID schemaIdForPath(const std::string& path)
    {
        ErrorContext ctx;
        CatalogManager::SchemaInfo info;
        auto status = catalog_->getSchema(path, info, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return info.schema_id;
    }

    ID ensureSchemaPath(const std::string& path)
    {
        ErrorContext ctx;
        CatalogManager::SchemaInfo info;
        auto status = catalog_->getSchema(path, info, &ctx);
        if (status == Status::OK)
        {
            return info.schema_id;
        }
        // Some catalog builds currently return INVALID_ARGUMENT with
        // "Schema not found: <name>" for missing schemas.
        if (status != Status::NOT_FOUND && status != Status::INVALID_ARGUMENT)
        {
            EXPECT_TRUE(status == Status::NOT_FOUND || status == Status::INVALID_ARGUMENT)
                << ctx.message;
            return ID{};
        }
        return createSchemaPath(path);
    }

    ID createDelimitedSchema(const ID& parent_schema_id, const std::string& name)
    {
        CatalogManager::SchemaInfo schema;
        schema.schema_id = generateUuidV7();
        schema.parent_schema_id = parent_schema_id;
        schema.schema_name = name;
        schema.name_is_delimited = true;
        schema.permissions = 0x0FFF;
        schema.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
        schema.last_modified_time = schema.created_time;

        ErrorContext ctx;
        auto status = catalog_->writeSchemaRecord(schema, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        status = catalog_->readSchemaRecords(&ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        return schema.schema_id;
    }

    ID createTable(const ID& schema_id, const std::string& table_name)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns{makeColumn("id")};
        ID table_id;
        auto status = catalog_->createTable(schema_id, table_name, columns, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }

    ID createView(const ID& schema_id, const std::string& name)
    {
        ErrorContext ctx;
        auto status = catalog_->createView(schema_id, name, "select 1", false, false, false,
                                           {}, ID{}, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        CatalogManager::ViewInfo info;
        status = catalog_->getView(schema_id, name, info, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return info.view_id;
    }

    Status resolveTable(PathType type, const std::vector<std::string>& components,
                        ID& object_id_out, ObjectType& type_out, ErrorContext& ctx)
    {
        ObjectPath path;
        path.type = type;
        path.components = components;
        CatalogManager::ResolveOptions opts;
        return catalog_->resolveObjectPath(path, ObjectType::TABLE, opts, object_id_out, type_out, &ctx);
    }

    Status resolveObject(ObjectType expected_type, PathType type,
                         const std::vector<std::string>& components,
                         ID& object_id_out, ObjectType& type_out, ErrorContext& ctx)
    {
        ObjectPath path;
        path.type = type;
        path.components = components;
        CatalogManager::ResolveOptions opts;
        return catalog_->resolveObjectPath(path, expected_type, opts, object_id_out, type_out, &ctx);
    }

    auto compileSql(const std::string& sql) -> std::vector<uint8_t>
    {
        scratchbird::sblr::QueryCompilerV3 compiler(db_);
        compiler.setCurrentSchema(conn_->getCurrentSchemaId());
        auto compiled = compiler.compile(sql);
        EXPECT_TRUE(compiled.success()) << sql;
        if (!compiled.success())
        {
            return {};
        }
        return compiled.bytecode();
    }

    auto executeSql(const std::string& sql) -> scratchbird::sblr::ExecutionResult
    {
        auto bytecode = compileSql(sql);
        if (bytecode.empty())
        {
            return scratchbird::sblr::ExecutionResult("Failed to compile SQL: " + sql);
        }
        scratchbird::sblr::Executor executor(db_);
        executor.setConnectionContext(conn_.get());
        return executor.execute(bytecode);
    }
};

TEST_F(SchemaPathResolutionTest, UnqualifiedPrefersCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    ID current_table = createTable(user_schema, "target");
    ID search_table = createTable(public_schema, "target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::UNQUALIFIED, {"target"}, resolved_id, resolved_type, ctx);

    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, current_table);
    EXPECT_EQ(resolved_type, ObjectType::TABLE);
    (void)search_table;
}

TEST_F(SchemaPathResolutionTest, UnqualifiedFallsBackToSearchPath)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    ID search_table = createTable(public_schema, "fallback");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::UNQUALIFIED, {"fallback"}, resolved_id, resolved_type, ctx);

    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, search_table);
    EXPECT_EQ(resolved_type, ObjectType::TABLE);
}

TEST_F(SchemaPathResolutionTest, CurrentPathIsStrict)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    createTable(public_schema, "strict_only");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::CURRENT, {"strict_only"}, resolved_id, resolved_type, ctx);

    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(SchemaPathResolutionTest, RelativePathResolvesFromCurrent)
{
    ID user_schema = createSchemaPath("users.alice");
    ID nested_schema = createSchemaPath("users.alice.dev.myproj");

    ID nested_table = createTable(nested_schema, "rel_target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::CURRENT, {"dev", "myproj", "rel_target"},
                                 resolved_id, resolved_type, ctx);

    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, nested_table);
    EXPECT_EQ(resolved_type, ObjectType::TABLE);
}

TEST_F(SchemaPathResolutionTest, AbsolutePathResolves)
{
    ID nested_schema = createSchemaPath("users.alice.dev.myproj");
    ID nested_table = createTable(nested_schema, "abs_target");

    conn_->setCurrentSchemaId(nested_schema);
    conn_->set_search_path({"public"});

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::ABSOLUTE,
                                 {"users", "alice", "dev", "myproj", "abs_target"},
                                 resolved_id, resolved_type, ctx);

    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, nested_table);
    EXPECT_EQ(resolved_type, ObjectType::TABLE);
}

TEST_F(SchemaPathResolutionTest, AmbiguousSearchPathReturnsError)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");
    ID app_schema = ensureSchemaPath("app");

    createTable(public_schema, "ambiguous");
    createTable(app_schema, "ambiguous");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public", "app"});

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::UNQUALIFIED, {"ambiguous"}, resolved_id, resolved_type, ctx);

    EXPECT_EQ(status, Status::AMBIGUOUS_COLUMN);
}

TEST_F(SchemaPathResolutionTest, CrossTypeAmbiguityReturnsError)
{
    ID schema_id = createSchemaPath("users.alice");

    createTable(schema_id, "dup_name");
    createView(schema_id, "dup_name");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::UNKNOWN, PathType::ABSOLUTE,
                                  {"users", "alice", "dup_name"},
                                  resolved_id, resolved_type, ctx);

    EXPECT_EQ(status, Status::AMBIGUOUS_COLUMN);
}

TEST_F(SchemaPathResolutionTest, GetSchemaHonorsDelimitedNames)
{
    ErrorContext ctx;
    ID delimited_id = createDelimitedSchema(ID{}, "MixedCase");

    CatalogManager::SchemaInfo info;
    Status status = catalog_->getSchema("MixedCase", info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(info.schema_id, delimited_id);

    status = catalog_->getSchema("mixedcase", info, &ctx);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);

    status = catalog_->getSchema("root.MixedCase", info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(info.schema_id, delimited_id);

    status = catalog_->getSchema("root.mixedcase", info, &ctx);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
}

TEST_F(SchemaPathResolutionTest, ResolverHonorsDelimitedSchemaNames)
{
    ErrorContext ctx;
    ID delimited_id = createDelimitedSchema(ID{}, "MixedCase");
    ID table_id = createTable(delimited_id, "widget");

    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::ABSOLUTE, {"MixedCase", "widget"},
                                 resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, table_id);
    EXPECT_EQ(resolved_type, ObjectType::TABLE);

    ErrorContext miss_ctx;
    status = resolveTable(PathType::ABSOLUTE, {"mixedcase", "widget"},
                          resolved_id, resolved_type, miss_ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(SchemaPathResolutionTest, ExecutorDropTableUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    createTable(user_schema, "drop_target");
    createTable(public_schema, "drop_target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    auto result = executeSql("DROP TABLE drop_target");
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::TableInfo table_info;
    ErrorContext ctx;
    EXPECT_NE(catalog_->getTable(user_schema, "drop_target", table_info, &ctx), Status::OK);
    EXPECT_EQ(catalog_->getTable(public_schema, "drop_target", table_info, &ctx), Status::OK);
}

TEST_F(SchemaPathResolutionTest, ExecutorDropTableIfExistsEmitsPostgreSqlNotice)
{
    ID user_schema = createSchemaPath("users.alice");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);
    conn_->set_dialect_tag("postgresql");

    auto result = executeSql("DROP TABLE IF EXISTS missing_table");
    ASSERT_TRUE(result.success()) << result.error();

    auto notices = conn_->consumeNotices();
    ASSERT_EQ(notices.size(), 1u);
    EXPECT_EQ(notices[0], "table \"missing_table\" does not exist, skipping");
}

TEST_F(SchemaPathResolutionTest, ExecutorDropSchemaIfExistsEmitsPostgreSqlNotice)
{
    ID user_schema = createSchemaPath("users.alice");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);
    conn_->set_dialect_tag("postgresql");

    auto result = executeSql("DROP SCHEMA IF EXISTS missing_schema");
    ASSERT_TRUE(result.success()) << result.error();

    auto notices = conn_->consumeNotices();
    ASSERT_EQ(notices.size(), 1u);
    EXPECT_EQ(notices[0], "schema \"missing_schema\" does not exist, skipping");
}

TEST_F(SchemaPathResolutionTest, ExecutorCreateTableUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    createTable(public_schema, "create_target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    auto result = executeSql("CREATE TABLE create_target (id INT)");
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::TableInfo table_info;
    ErrorContext ctx;
    EXPECT_EQ(catalog_->getTable(user_schema, "create_target", table_info, &ctx), Status::OK);
    EXPECT_EQ(catalog_->getTable(public_schema, "create_target", table_info, &ctx), Status::OK);
}

TEST_F(SchemaPathResolutionTest, ExecutorTruncateTableUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    createTable(public_schema, "truncate_target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    auto result = executeSql("TRUNCATE TABLE truncate_target");

    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find("Object not found"), std::string::npos);
}

TEST_F(SchemaPathResolutionTest, ExecutorCreateIndexUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID table_id = createTable(user_schema, "idx_table");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});

    auto result = executeSql("CREATE INDEX idx_current_id ON idx_table (id)");
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::IndexInfo index_info;
    ErrorContext ctx;
    auto status = catalog_->getIndex(table_id, "idx_current_id", index_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
}

TEST_F(SchemaPathResolutionTest, ExecutorDropIndexUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID table_id = createTable(user_schema, "idx_drop_table");

    ErrorContext ctx;
    ID index_id;
    auto status = catalog_->createIndex(table_id, "idx_drop", {"id"}, index_id,
                                        false, CatalogManager::IndexType::BTREE, 0, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});

    auto result = executeSql("DROP INDEX idx_drop_table.idx_drop");
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::IndexInfo index_info;
    status = catalog_->getIndex(table_id, "idx_drop", index_info, &ctx);
    EXPECT_NE(status, Status::OK);
}

TEST_F(SchemaPathResolutionTest, ExecutorAlterTableUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");
    ID user_table_id = createTable(user_schema, "alter_target");
    ID public_table_id = createTable(public_schema, "alter_target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    auto result = executeSql("ALTER TABLE alter_target RENAME COLUMN id TO id_renamed");
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::ColumnInfo column_info;
    ErrorContext ctx;
    EXPECT_EQ(catalog_->getColumn(user_table_id, "id_renamed", column_info, &ctx), Status::OK);
    EXPECT_NE(catalog_->getColumn(user_table_id, "id", column_info, &ctx), Status::OK);
    EXPECT_EQ(catalog_->getColumn(public_table_id, "id", column_info, &ctx), Status::OK);
    EXPECT_NE(catalog_->getColumn(public_table_id, "id_renamed", column_info, &ctx), Status::OK);
}
