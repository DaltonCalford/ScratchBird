#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/types.h"

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
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        test_db_path_ = "/tmp/test_catalog_rename_move_" + std::to_string(getpid()) + ".db";
        std::remove(test_db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
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
