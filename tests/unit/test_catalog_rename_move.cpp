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
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"
#include <cstdio>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

using namespace scratchbird::core;
using ObjectType = CatalogManager::ObjectType;

class CatalogRenameMoveTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    std::string udr_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID system_user_id_{};
    int foreign_server_counter_ = 0;

    void SetUp() override
    {
        test_db_path_ = "/tmp/test_catalog_rename_move_" + std::to_string(getpid()) + ".db";
        udr_path_ = scratchbird::testing::uniqueTestDbPath("libudr", ".so");
        std::remove(test_db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        system_user_id_ = catalog_->getSystemUserId(&ctx);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
            catalog_ = nullptr;
        }
        std::remove(test_db_path_.c_str());
    }

    void reopenDatabase()
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
        }

        db_ = std::make_unique<Database>();
        ErrorContext ctx;
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        system_user_id_ = catalog_->getSystemUserId(&ctx);
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

    ID createTable(const ID& schema_id, const std::string& table_name)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns{makeColumn("id")};
        ID table_id;
        auto status = catalog_->createTable(schema_id, table_name, columns, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }

    ID createIndex(const ID& table_id, const std::string& index_name)
    {
        ErrorContext ctx;
        ID index_id;
        auto status = catalog_->createIndex(table_id, index_name, {"id"}, index_id, false,
                                            CatalogManager::IndexType::BTREE, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return index_id;
    }

    ID createTrigger(const ID& table_id, const std::string& table_name,
                     const std::string& trigger_name)
    {
        ErrorContext ctx;
        CatalogManager::TriggerInfo trigger{};
        trigger.trigger_id = generateUuidV7();
        trigger.trigger_name = trigger_name;
        trigger.table_id = table_id;
        trigger.table_name = table_name;
        trigger.timing = CatalogManager::TriggerTiming::BEFORE;
        trigger.event_mask = 1u << static_cast<uint8_t>(CatalogManager::TriggerEvent::INSERT);
        trigger.granularity = CatalogManager::TriggerGranularity::FOR_EACH_ROW;

        auto status = catalog_->createTrigger(trigger, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return trigger.trigger_id;
    }

    ID createConstraint(const ID& table_id, const std::string& constraint_name)
    {
        ErrorContext ctx;
        CatalogManager::ConstraintInfo constraint{};
        constraint.constraint_name = constraint_name;
        constraint.table_id = table_id;
        constraint.constraint_type = CatalogManager::ConstraintType::PRIMARY_KEY;
        constraint.column_names = {"id"};
        constraint.owner_id = system_user_id_;

        ID constraint_id;
        auto status = catalog_->createConstraint(constraint, constraint_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return constraint_id;
    }

    ID createFunction(const ID& schema_id, const std::string& name)
    {
        ErrorContext ctx;
        CatalogManager::FunctionInfo info;
        info.function_id = generateUuidV7();
        info.schema_id = schema_id;
        info.name = name;
        info.owner_id = system_user_id_;
        info.return_type = DataType::INT32;
        info.or_replace = false;
        info.deterministic = false;
        info.sql_security = CatalogManager::FunctionInfo::SqlSecurity::DEFINER;
        info.source_text = "return 1;";
        info.created_time = 0;
        info.modified_time = 0;

        auto status = catalog_->registerFunction(info, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return info.function_id;
    }

    ID createProcedure(const ID& schema_id, const std::string& name)
    {
        ErrorContext ctx;
        CatalogManager::ProcedureInfo info;
        info.procedure_id = generateUuidV7();
        info.schema_id = schema_id;
        info.name = name;
        info.owner_id = system_user_id_;
        info.or_replace = false;
        info.sql_security = CatalogManager::ProcedureInfo::SqlSecurity::DEFINER;
        info.source_text = "begin end";
        info.created_time = 0;
        info.modified_time = 0;

        auto status = catalog_->registerProcedure(info, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return info.procedure_id;
    }

    ID createSequence(const ID& schema_id, const std::string& name)
    {
        ErrorContext ctx;
        auto status = catalog_->createSequence(schema_id, name, 1, 1, 1000, 1, 1, false, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        ID sequence_id;
        status = catalog_->getSequenceIdByName(schema_id, name, sequence_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return sequence_id;
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

    ID createSynonym(const ID& schema_id, const std::string& name,
                     const std::string& target_path)
    {
        ErrorContext ctx;
        ID synonym_id;
        auto status = catalog_->createSynonym(schema_id, name, target_path, ObjectType::TABLE,
                                              false, synonym_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return synonym_id;
    }

    ID createForeignServer(const std::string& name_prefix)
    {
        ErrorContext ctx;
        ID server_id;
        std::string name = name_prefix + "_" + std::to_string(++foreign_server_counter_);
        auto status = catalog_->createForeignServer(name, "test", "localhost", 5432, "",
                                                    server_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return server_id;
    }

    ID createForeignTable(const ID& schema_id, const std::string& name)
    {
        ErrorContext ctx;
        ID server_id = createForeignServer("fdw_server");
        ID table_id;
        auto status = catalog_->createForeignTable(schema_id, name, server_id, "public",
                                                   "remote_table", "", table_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }

    ID createPackage(const ID& schema_id, const std::string& name)
    {
        ErrorContext ctx;
        ID package_id;
        auto status = catalog_->createPackage(schema_id, name, "package header", "package body",
                                              package_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return package_id;
    }

    ID createUDR(const ID& schema_id, const std::string& name)
    {
        ErrorContext ctx;
        ID udr_id;
        auto status = catalog_->createUDR(schema_id, name, udr_path_, "entry",
                                          CatalogManager::UDRType::FUNCTION, "signature",
                                          udr_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return udr_id;
    }

    ID createException(const ID& schema_id, const std::string& name)
    {
        ErrorContext ctx;
        ID exception_id;
        auto status = catalog_->createException(schema_id, name, "error message",
                                                exception_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return exception_id;
    }

    ID columnIdForName(const ID& table_id, const std::string& name)
    {
        ErrorContext ctx;
        CatalogManager::ColumnInfo info;
        auto status = catalog_->getColumn(table_id, name, info, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return info.column_id;
    }

    Status resolveObject(ObjectType expected_type,
                         PathType type,
                         const std::vector<std::string>& components,
                         ID& object_id_out,
                         ObjectType& type_out,
                         ErrorContext& ctx)
    {
        ObjectPath path;
        path.type = type;
        path.components = components;
        CatalogManager::ResolveOptions opts;
        return catalog_->resolveObjectPath(path, expected_type, opts, object_id_out, type_out, &ctx);
    }
};

TEST_F(CatalogRenameMoveTest, RenameTableUpdatesResolverAndPersists)
{
    ID schema_id = createSchemaPath("users.alice");
    ID table_id = createTable(schema_id, "orders");
    conn_->setCurrentSchemaId(schema_id);

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::TABLE, PathType::ABSOLUTE,
                                  {"users", "alice", "orders"}, resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, table_id);

    status = catalog_->renameObject(ObjectType::TABLE, table_id, "orders_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ErrorContext renamed_ctx;
    status = resolveObject(ObjectType::TABLE, PathType::ABSOLUTE,
                           {"users", "alice", "orders_new"},
                           resolved_id, resolved_type, renamed_ctx);
    ASSERT_EQ(status, Status::OK) << renamed_ctx.message;
    EXPECT_EQ(resolved_id, table_id);

    ErrorContext missing_ctx;
    status = resolveObject(ObjectType::TABLE, PathType::ABSOLUTE,
                           {"users", "alice", "orders"},
                           resolved_id, resolved_type, missing_ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);

    ASSERT_EQ(db_->buffer_pool()->flushAll(&ctx), Status::OK) << ctx.message;
    reopenDatabase();

    ErrorContext reopen_ctx;
    status = resolveObject(ObjectType::TABLE, PathType::ABSOLUTE,
                           {"users", "alice", "orders_new"},
                           resolved_id, resolved_type, reopen_ctx);
    ASSERT_EQ(status, Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, table_id);

    status = resolveObject(ObjectType::TABLE, PathType::ABSOLUTE,
                           {"users", "alice", "orders"},
                           resolved_id, resolved_type, reopen_ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(CatalogRenameMoveTest, MoveTableUpdatesResolverAndColumnPaths)
{
    ID source_schema = createSchemaPath("users.alice");
    ID target_schema = schemaIdForPath("app");
    ID table_id = createTable(source_schema, "inventory");
    ID column_id = columnIdForName(table_id, "id");

    ErrorContext ctx;
    Status status = catalog_->moveObject(ObjectType::TABLE, table_id, target_schema,
                                         std::nullopt, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID resolved_id;
    ObjectType resolved_type;
    status = resolveObject(ObjectType::TABLE, PathType::ABSOLUTE,
                           {"app", "inventory"}, resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, table_id);

    status = resolveObject(ObjectType::COLUMN, PathType::ABSOLUTE,
                           {"app", "inventory", "id"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, column_id);
    EXPECT_EQ(resolved_type, ObjectType::COLUMN);

    status = resolveObject(ObjectType::TABLE, PathType::ABSOLUTE,
                           {"users", "alice", "inventory"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(CatalogRenameMoveTest, RenameColumnUpdatesResolver)
{
    ID schema_id = createSchemaPath("users.alice");
    ID table_id = createTable(schema_id, "products");
    ID column_id = columnIdForName(table_id, "id");

    ErrorContext ctx;
    Status status = catalog_->renameObject(ObjectType::COLUMN, column_id, "item_id", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID resolved_id;
    ObjectType resolved_type;
    status = resolveObject(ObjectType::COLUMN, PathType::ABSOLUTE,
                           {"users", "alice", "products", "item_id"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, column_id);

    status = resolveObject(ObjectType::COLUMN, PathType::ABSOLUTE,
                           {"users", "alice", "products", "id"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(CatalogRenameMoveTest, RenameIndexUpdatesResolver)
{
    ID schema_id = createSchemaPath("users.alice");
    ID table_id = createTable(schema_id, "orders");
    ID index_id = createIndex(table_id, "orders_idx");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::INDEX, PathType::ABSOLUTE,
                                  {"users", "alice", "orders", "orders_idx"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, index_id);

    status = catalog_->renameObject(ObjectType::INDEX, index_id, "orders_idx_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::INDEX, PathType::ABSOLUTE,
                           {"users", "alice", "orders", "orders_idx_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, index_id);

    status = resolveObject(ObjectType::INDEX, PathType::ABSOLUTE,
                           {"users", "alice", "orders", "orders_idx"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(CatalogRenameMoveTest, RenameTriggerUpdatesResolver)
{
    ID schema_id = createSchemaPath("users.alice");
    ID table_id = createTable(schema_id, "events");
    ID trigger_id = createTrigger(table_id, "events", "events_trg");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::TRIGGER, PathType::ABSOLUTE,
                                  {"users", "alice", "events", "events_trg"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, trigger_id);

    status = catalog_->renameObject(ObjectType::TRIGGER, trigger_id, "events_trg_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::TRIGGER, PathType::ABSOLUTE,
                           {"users", "alice", "events", "events_trg_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, trigger_id);

    status = resolveObject(ObjectType::TRIGGER, PathType::ABSOLUTE,
                           {"users", "alice", "events", "events_trg"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(CatalogRenameMoveTest, RenameConstraintUpdatesResolverAndPersists)
{
    ID schema_id = createSchemaPath("users.alice");
    ID table_id = createTable(schema_id, "accounts");
    ID constraint_id = createConstraint(table_id, "pk_accounts");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::CONSTRAINT, PathType::ABSOLUTE,
                                  {"users", "alice", "accounts", "pk_accounts"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, constraint_id);

    status = catalog_->renameObject(ObjectType::CONSTRAINT, constraint_id, "pk_accounts_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::CONSTRAINT, PathType::ABSOLUTE,
                           {"users", "alice", "accounts", "pk_accounts_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, constraint_id);

    status = resolveObject(ObjectType::CONSTRAINT, PathType::ABSOLUTE,
                           {"users", "alice", "accounts", "pk_accounts"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);

    ASSERT_EQ(db_->buffer_pool()->flushAll(&ctx), Status::OK) << ctx.message;
    reopenDatabase();

    ErrorContext reopen_ctx;
    status = resolveObject(ObjectType::CONSTRAINT, PathType::ABSOLUTE,
                           {"users", "alice", "accounts", "pk_accounts_new"},
                           resolved_id, resolved_type, reopen_ctx);
    ASSERT_EQ(status, Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, constraint_id);
}

TEST_F(CatalogRenameMoveTest, MoveTableScopedObjectsIsRejected)
{
    ID schema_id = createSchemaPath("users.alice");
    ID table_id = createTable(schema_id, "history");
    ID index_id = createIndex(table_id, "history_idx");
    ID trigger_id = createTrigger(table_id, "history", "history_trg");
    ID constraint_id = createConstraint(table_id, "pk_history");
    ID target_schema = schemaIdForPath("app");

    ErrorContext ctx;
    EXPECT_EQ(catalog_->moveObject(ObjectType::INDEX, index_id, target_schema,
                                   std::nullopt, &ctx),
              Status::INVALID_ARGUMENT);
    EXPECT_EQ(catalog_->moveObject(ObjectType::TRIGGER, trigger_id, target_schema,
                                   std::nullopt, &ctx),
              Status::INVALID_ARGUMENT);
    EXPECT_EQ(catalog_->moveObject(ObjectType::CONSTRAINT, constraint_id, target_schema,
                                   std::nullopt, &ctx),
              Status::INVALID_ARGUMENT);
}

TEST_F(CatalogRenameMoveTest, ResolverRebuildRestoresTableScopedObjects)
{
    ID schema_id = createSchemaPath("users.alice");
    ID table_id = createTable(schema_id, "audit");
    ID index_id = createIndex(table_id, "audit_idx");
    ID trigger_id = createTrigger(table_id, "audit", "audit_trg");
    ID constraint_id = createConstraint(table_id, "pk_audit");

    ErrorContext ctx;
    ASSERT_EQ(db_->buffer_pool()->flushAll(&ctx), Status::OK) << ctx.message;
    reopenDatabase();

    ErrorContext reopen_ctx;
    ID resolved_id;
    ObjectType resolved_type;
    EXPECT_EQ(resolveObject(ObjectType::INDEX, PathType::ABSOLUTE,
                            {"users", "alice", "audit", "audit_idx"},
                            resolved_id, resolved_type, reopen_ctx),
              Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, index_id);

    EXPECT_EQ(resolveObject(ObjectType::TRIGGER, PathType::ABSOLUTE,
                            {"users", "alice", "audit", "audit_trg"},
                            resolved_id, resolved_type, reopen_ctx),
              Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, trigger_id);

    EXPECT_EQ(resolveObject(ObjectType::CONSTRAINT, PathType::ABSOLUTE,
                            {"users", "alice", "audit", "pk_audit"},
                            resolved_id, resolved_type, reopen_ctx),
              Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, constraint_id);
}

TEST_F(CatalogRenameMoveTest, ResolverRebuildRestoresSchemaScopedObjects)
{
    ID schema_id = createSchemaPath("users.alice");
    createTable(schema_id, "syn_target");
    ID synonym_id = createSynonym(schema_id, "syn_obj", "users.alice.syn_target");
    ID foreign_table_id = createForeignTable(schema_id, "foreign_obj");
    ID package_id = createPackage(schema_id, "pkg_obj");
    ID udr_id = createUDR(schema_id, "udr_obj");
    ID exception_id = createException(schema_id, "ex_obj");

    ErrorContext ctx;
    ASSERT_EQ(db_->buffer_pool()->flushAll(&ctx), Status::OK) << ctx.message;
    reopenDatabase();

    ErrorContext reopen_ctx;
    ID resolved_id;
    ObjectType resolved_type;
    EXPECT_EQ(resolveObject(ObjectType::SYNONYM, PathType::ABSOLUTE,
                            {"users", "alice", "syn_obj"},
                            resolved_id, resolved_type, reopen_ctx),
              Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, synonym_id);

    EXPECT_EQ(resolveObject(ObjectType::FOREIGN_TABLE, PathType::ABSOLUTE,
                            {"users", "alice", "foreign_obj"},
                            resolved_id, resolved_type, reopen_ctx),
              Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, foreign_table_id);

    EXPECT_EQ(resolveObject(ObjectType::PACKAGE, PathType::ABSOLUTE,
                            {"users", "alice", "pkg_obj"},
                            resolved_id, resolved_type, reopen_ctx),
              Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, package_id);

    EXPECT_EQ(resolveObject(ObjectType::UDR, PathType::ABSOLUTE,
                            {"users", "alice", "udr_obj"},
                            resolved_id, resolved_type, reopen_ctx),
              Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, udr_id);

    EXPECT_EQ(resolveObject(ObjectType::EXCEPTION, PathType::ABSOLUTE,
                            {"users", "alice", "ex_obj"},
                            resolved_id, resolved_type, reopen_ctx),
              Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, exception_id);
}

TEST_F(CatalogRenameMoveTest, RenameSequenceUpdatesResolverAndPersists)
{
    ID schema_id = createSchemaPath("users.alice");
    ID sequence_id = createSequence(schema_id, "order_seq");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::SEQUENCE, PathType::ABSOLUTE,
                                  {"users", "alice", "order_seq"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, sequence_id);

    status = catalog_->renameObject(ObjectType::SEQUENCE, sequence_id, "order_seq_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::SEQUENCE, PathType::ABSOLUTE,
                           {"users", "alice", "order_seq_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, sequence_id);

    status = resolveObject(ObjectType::SEQUENCE, PathType::ABSOLUTE,
                           {"users", "alice", "order_seq"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);

    ASSERT_EQ(db_->buffer_pool()->flushAll(&ctx), Status::OK) << ctx.message;
    reopenDatabase();

    ErrorContext reopen_ctx;
    status = resolveObject(ObjectType::SEQUENCE, PathType::ABSOLUTE,
                           {"users", "alice", "order_seq_new"},
                           resolved_id, resolved_type, reopen_ctx);
    ASSERT_EQ(status, Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, sequence_id);
}

TEST_F(CatalogRenameMoveTest, MoveSequenceUpdatesResolver)
{
    ID source_schema = createSchemaPath("users.alice");
    ID target_schema = schemaIdForPath("app");
    ID sequence_id = createSequence(source_schema, "inventory_seq");

    ErrorContext ctx;
    Status status = catalog_->moveObject(ObjectType::SEQUENCE, sequence_id, target_schema,
                                         std::nullopt, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID resolved_id;
    ObjectType resolved_type;
    status = resolveObject(ObjectType::SEQUENCE, PathType::ABSOLUTE,
                           {"app", "inventory_seq"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, sequence_id);

    status = resolveObject(ObjectType::SEQUENCE, PathType::ABSOLUTE,
                           {"users", "alice", "inventory_seq"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(CatalogRenameMoveTest, RenameViewUpdatesResolverAndPersists)
{
    ID schema_id = createSchemaPath("users.alice");
    ID view_id = createView(schema_id, "orders_view");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::VIEW, PathType::ABSOLUTE,
                                  {"users", "alice", "orders_view"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, view_id);

    status = catalog_->renameObject(ObjectType::VIEW, view_id, "orders_view_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::VIEW, PathType::ABSOLUTE,
                           {"users", "alice", "orders_view_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, view_id);

    status = resolveObject(ObjectType::VIEW, PathType::ABSOLUTE,
                           {"users", "alice", "orders_view"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);

    ASSERT_EQ(db_->buffer_pool()->flushAll(&ctx), Status::OK) << ctx.message;
    reopenDatabase();

    ErrorContext reopen_ctx;
    status = resolveObject(ObjectType::VIEW, PathType::ABSOLUTE,
                           {"users", "alice", "orders_view_new"},
                           resolved_id, resolved_type, reopen_ctx);
    ASSERT_EQ(status, Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, view_id);
}

TEST_F(CatalogRenameMoveTest, MoveViewUpdatesResolver)
{
    ID source_schema = createSchemaPath("users.alice");
    ID target_schema = schemaIdForPath("app");
    ID view_id = createView(source_schema, "inventory_view");

    ErrorContext ctx;
    Status status = catalog_->moveObject(ObjectType::VIEW, view_id, target_schema,
                                         std::nullopt, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID resolved_id;
    ObjectType resolved_type;
    status = resolveObject(ObjectType::VIEW, PathType::ABSOLUTE,
                           {"app", "inventory_view"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, view_id);

    status = resolveObject(ObjectType::VIEW, PathType::ABSOLUTE,
                           {"users", "alice", "inventory_view"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(CatalogRenameMoveTest, RenameSynonymUpdatesResolver)
{
    ID schema_id = createSchemaPath("users.alice");
    createTable(schema_id, "target_table");
    ID synonym_id = createSynonym(schema_id, "orders_syn", "users.alice.target_table");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::SYNONYM, PathType::ABSOLUTE,
                                  {"users", "alice", "orders_syn"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, synonym_id);

    status = catalog_->renameObject(ObjectType::SYNONYM, synonym_id, "orders_syn_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::SYNONYM, PathType::ABSOLUTE,
                           {"users", "alice", "orders_syn_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, synonym_id);
}

TEST_F(CatalogRenameMoveTest, MoveSynonymUpdatesResolver)
{
    ID source_schema = createSchemaPath("users.alice");
    ID target_schema = schemaIdForPath("app");
    createTable(source_schema, "syn_target");
    ID synonym_id = createSynonym(source_schema, "move_syn", "users.alice.syn_target");

    ErrorContext ctx;
    Status status = catalog_->moveObject(ObjectType::SYNONYM, synonym_id, target_schema,
                                         std::nullopt, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID resolved_id;
    ObjectType resolved_type;
    status = resolveObject(ObjectType::SYNONYM, PathType::ABSOLUTE,
                           {"app", "move_syn"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, synonym_id);

    status = resolveObject(ObjectType::SYNONYM, PathType::ABSOLUTE,
                           {"users", "alice", "move_syn"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(CatalogRenameMoveTest, RenameForeignTableUpdatesResolver)
{
    ID schema_id = createSchemaPath("users.alice");
    ID table_id = createForeignTable(schema_id, "orders_ft");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::FOREIGN_TABLE, PathType::ABSOLUTE,
                                  {"users", "alice", "orders_ft"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, table_id);

    status = catalog_->renameObject(ObjectType::FOREIGN_TABLE, table_id, "orders_ft_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::FOREIGN_TABLE, PathType::ABSOLUTE,
                           {"users", "alice", "orders_ft_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, table_id);
}

TEST_F(CatalogRenameMoveTest, MoveForeignTableUpdatesResolver)
{
    ID source_schema = createSchemaPath("users.alice");
    ID target_schema = schemaIdForPath("app");
    ID table_id = createForeignTable(source_schema, "inventory_ft");

    ErrorContext ctx;
    Status status = catalog_->moveObject(ObjectType::FOREIGN_TABLE, table_id, target_schema,
                                         std::nullopt, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID resolved_id;
    ObjectType resolved_type;
    status = resolveObject(ObjectType::FOREIGN_TABLE, PathType::ABSOLUTE,
                           {"app", "inventory_ft"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, table_id);

    status = resolveObject(ObjectType::FOREIGN_TABLE, PathType::ABSOLUTE,
                           {"users", "alice", "inventory_ft"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(CatalogRenameMoveTest, RenameFunctionUpdatesResolverAndPersists)
{
    ID schema_id = createSchemaPath("users.alice");
    ID func_id = createFunction(schema_id, "calc_total");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::FUNCTION, PathType::ABSOLUTE,
                                  {"users", "alice", "calc_total"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, func_id);

    status = catalog_->renameObject(ObjectType::FUNCTION, func_id, "calc_total_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::FUNCTION, PathType::ABSOLUTE,
                           {"users", "alice", "calc_total_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, func_id);

    status = resolveObject(ObjectType::FUNCTION, PathType::ABSOLUTE,
                           {"users", "alice", "calc_total"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);

    ASSERT_EQ(db_->buffer_pool()->flushAll(&ctx), Status::OK) << ctx.message;
    reopenDatabase();

    ErrorContext reopen_ctx;
    status = resolveObject(ObjectType::FUNCTION, PathType::ABSOLUTE,
                           {"users", "alice", "calc_total_new"},
                           resolved_id, resolved_type, reopen_ctx);
    ASSERT_EQ(status, Status::OK) << reopen_ctx.message;
    EXPECT_EQ(resolved_id, func_id);
}

TEST_F(CatalogRenameMoveTest, MoveFunctionUpdatesResolver)
{
    ID source_schema = createSchemaPath("users.alice");
    ID target_schema = schemaIdForPath("app");
    ID func_id = createFunction(source_schema, "move_func");

    ErrorContext ctx;
    Status status = catalog_->moveObject(ObjectType::FUNCTION, func_id, target_schema,
                                         std::nullopt, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID resolved_id;
    ObjectType resolved_type;
    status = resolveObject(ObjectType::FUNCTION, PathType::ABSOLUTE,
                           {"app", "move_func"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, func_id);

    status = resolveObject(ObjectType::FUNCTION, PathType::ABSOLUTE,
                           {"users", "alice", "move_func"},
                           resolved_id, resolved_type, ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(CatalogRenameMoveTest, RenameProcedureUpdatesResolver)
{
    ID schema_id = createSchemaPath("users.alice");
    ID proc_id = createProcedure(schema_id, "update_orders");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::PROCEDURE, PathType::ABSOLUTE,
                                  {"users", "alice", "update_orders"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, proc_id);

    status = catalog_->renameObject(ObjectType::PROCEDURE, proc_id, "update_orders_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::PROCEDURE, PathType::ABSOLUTE,
                           {"users", "alice", "update_orders_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, proc_id);
}

TEST_F(CatalogRenameMoveTest, MoveProcedureUpdatesResolver)
{
    ID source_schema = createSchemaPath("users.alice");
    ID target_schema = schemaIdForPath("app");
    ID proc_id = createProcedure(source_schema, "move_proc");

    ErrorContext ctx;
    Status status = catalog_->moveObject(ObjectType::PROCEDURE, proc_id, target_schema,
                                         std::nullopt, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID resolved_id;
    ObjectType resolved_type;
    status = resolveObject(ObjectType::PROCEDURE, PathType::ABSOLUTE,
                           {"app", "move_proc"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, proc_id);
}

TEST_F(CatalogRenameMoveTest, RenamePackageUpdatesResolver)
{
    ID schema_id = createSchemaPath("users.alice");
    ID package_id = createPackage(schema_id, "order_pkg");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::PACKAGE, PathType::ABSOLUTE,
                                  {"users", "alice", "order_pkg"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, package_id);

    status = catalog_->renameObject(ObjectType::PACKAGE, package_id, "order_pkg_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::PACKAGE, PathType::ABSOLUTE,
                           {"users", "alice", "order_pkg_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, package_id);
}

TEST_F(CatalogRenameMoveTest, MovePackageUpdatesResolver)
{
    ID source_schema = createSchemaPath("users.alice");
    ID target_schema = schemaIdForPath("app");
    ID package_id = createPackage(source_schema, "move_pkg");

    ErrorContext ctx;
    Status status = catalog_->moveObject(ObjectType::PACKAGE, package_id, target_schema,
                                         std::nullopt, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID resolved_id;
    ObjectType resolved_type;
    status = resolveObject(ObjectType::PACKAGE, PathType::ABSOLUTE,
                           {"app", "move_pkg"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, package_id);
}

TEST_F(CatalogRenameMoveTest, RenameUDRUpdatesResolver)
{
    ID schema_id = createSchemaPath("users.alice");
    ID udr_id = createUDR(schema_id, "order_udr");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::UDR, PathType::ABSOLUTE,
                                  {"users", "alice", "order_udr"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, udr_id);

    status = catalog_->renameObject(ObjectType::UDR, udr_id, "order_udr_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::UDR, PathType::ABSOLUTE,
                           {"users", "alice", "order_udr_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, udr_id);
}

TEST_F(CatalogRenameMoveTest, MoveUDRUpdatesResolver)
{
    ID source_schema = createSchemaPath("users.alice");
    ID target_schema = schemaIdForPath("app");
    ID udr_id = createUDR(source_schema, "move_udr");

    ErrorContext ctx;
    Status status = catalog_->moveObject(ObjectType::UDR, udr_id, target_schema,
                                         std::nullopt, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID resolved_id;
    ObjectType resolved_type;
    status = resolveObject(ObjectType::UDR, PathType::ABSOLUTE,
                           {"app", "move_udr"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, udr_id);
}

TEST_F(CatalogRenameMoveTest, RenameExceptionUpdatesResolver)
{
    ID schema_id = createSchemaPath("users.alice");
    ID exception_id = createException(schema_id, "order_error");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::EXCEPTION, PathType::ABSOLUTE,
                                  {"users", "alice", "order_error"},
                                  resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, exception_id);

    status = catalog_->renameObject(ObjectType::EXCEPTION, exception_id, "order_error_new", &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = resolveObject(ObjectType::EXCEPTION, PathType::ABSOLUTE,
                           {"users", "alice", "order_error_new"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, exception_id);
}

TEST_F(CatalogRenameMoveTest, MoveExceptionUpdatesResolver)
{
    ID source_schema = createSchemaPath("users.alice");
    ID target_schema = schemaIdForPath("app");
    ID exception_id = createException(source_schema, "move_error");

    ErrorContext ctx;
    Status status = catalog_->moveObject(ObjectType::EXCEPTION, exception_id, target_schema,
                                         std::nullopt, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID resolved_id;
    ObjectType resolved_type;
    status = resolveObject(ObjectType::EXCEPTION, PathType::ABSOLUTE,
                           {"app", "move_error"},
                           resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, exception_id);
}

TEST_F(CatalogRenameMoveTest, ColumnNameIsDelimitedPersists)
{
    ID schema_id = createSchemaPath("users.alice");

    CatalogManager::ColumnInfo col = makeColumn("MixedCaseCol");
    col.name_is_delimited = true;

    ErrorContext ctx;
    ID table_id;
    Status status = catalog_->createTable(schema_id, "case_table", {col}, table_id, 0, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    reopenDatabase();

    CatalogManager::ColumnInfo info;
    status = catalog_->getColumn(table_id, "MixedCaseCol", info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_TRUE(info.name_is_delimited);

    status = catalog_->getColumn(table_id, "mixedcasecol", info, &ctx);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
}

TEST_F(CatalogRenameMoveTest, ConstraintNameIsDelimitedPersists)
{
    ID schema_id = createSchemaPath("users.alice");
    ID table_id = createTable(schema_id, "constraint_case");

    CatalogManager::ConstraintInfo constraint{};
    constraint.constraint_name = "MixedCaseConstraint";
    constraint.name_is_delimited = true;
    constraint.table_id = table_id;
    constraint.constraint_type = CatalogManager::ConstraintType::PRIMARY_KEY;
    constraint.column_names = {"id"};
    constraint.owner_id = system_user_id_;

    ErrorContext ctx;
    ID constraint_id;
    Status status = catalog_->createConstraint(constraint, constraint_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    reopenDatabase();

    CatalogManager::ConstraintInfo info;
    status = catalog_->getConstraint(constraint_id, info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_TRUE(info.name_is_delimited);

    status = catalog_->getConstraintByName(table_id, "mixedcaseconstraint", info, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);

    status = catalog_->getConstraintByName(table_id, "MixedCaseConstraint", info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(info.constraint_id, constraint_id);
}
