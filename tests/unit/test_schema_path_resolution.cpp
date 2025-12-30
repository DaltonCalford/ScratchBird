#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;
using ObjectType = CatalogManager::ObjectType;

class SchemaPathResolutionTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database* db_ = nullptr;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

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

    ID createTable(const ID& schema_id, const std::string& table_name)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns{makeColumn("id")};
        ID table_id;
        auto status = catalog_->createTable(schema_id, table_name, columns, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
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
};

TEST_F(SchemaPathResolutionTest, UnqualifiedPrefersCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    ID current_table = createTable(user_schema, "target");
    ID search_table = createTable(public_schema, "target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});

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
    ID app_schema = schemaIdForPath("app");

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
