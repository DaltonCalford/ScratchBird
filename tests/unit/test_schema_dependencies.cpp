#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;

class SchemaDependencyTest : public ::testing::Test
{
protected:
    std::string test_db_path;
    Database *db = nullptr;
    CatalogManager *catalog = nullptr;

    void SetUp() override
    {
        test_db_path = "/tmp/test_schema_dep_" + std::to_string(getpid()) + ".sbdb";
        std::remove(test_db_path.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path, 16384, &ctx), Status::OK);

        db = new Database();
        ASSERT_EQ(db->open(test_db_path, &ctx), Status::OK);
        catalog = db->catalog_manager();
        ASSERT_NE(catalog, nullptr);
    }

    void TearDown() override
    {
        if (db)
        {
            db->close();
            delete db;
            db = nullptr;
        }
        std::remove(test_db_path.c_str());
        std::remove((test_db_path + "-lock").c_str());
    }

    ID createSchema(const std::string& name)
    {
        ErrorContext ctx;
        ID schema_id;
        Status status = catalog->createSchema(name, "system", schema_id, &ctx);
        EXPECT_EQ(status, Status::OK) << "createSchema failed: " << ctx.message;
        return schema_id;
    }

    ID createTableInSchema(const ID& schema_id, const std::string& name)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns;
        CatalogManager::ColumnInfo col;
        col.column_id = generateUuidV7();
        col.column_name = "id";
        col.data_type = static_cast<uint16_t>(DataType::INT32);
        col.nullable = false;
        col.ordinal = 0;
        columns.push_back(col);

        ID table_id;
        Status status = catalog->createTable(schema_id, name, columns, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << "createTable failed: " << ctx.message;
        return table_id;
    }
};

TEST_F(SchemaDependencyTest, DropSchemaSucceedsWhenEmpty)
{
    ErrorContext ctx;
    ID schema_id = createSchema("empty_schema");

    Status status = catalog->dropSchema(schema_id, false, &ctx);
    EXPECT_EQ(status, Status::OK) << "dropSchema failed: " << ctx.message;
}

TEST_F(SchemaDependencyTest, DropSchemaFailsWhenContainsTable)
{
    ErrorContext ctx;
    ID schema_id = createSchema("schema_with_table");
    createTableInSchema(schema_id, "t1");

    Status status = catalog->dropSchema(schema_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    std::string msg = ctx.message;
    EXPECT_NE(msg.find("Tables: 1"), std::string::npos) << msg;
    EXPECT_NE(msg.find("Total:"), std::string::npos) << msg;
}

TEST_F(SchemaDependencyTest, DropSchemaFailsWhenContainsView)
{
    ErrorContext ctx;
    ID schema_id = createSchema("schema_with_view");

    Status status = catalog->createView(schema_id, "v1", "SELECT 1", false,
                                        false, false, {}, ID{}, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = catalog->dropSchema(schema_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    std::string msg = ctx.message;
    EXPECT_NE(msg.find("Views: 1"), std::string::npos) << msg;
}

TEST_F(SchemaDependencyTest, DropSchemaFailsWhenContainsSequence)
{
    ErrorContext ctx;
    ID schema_id = createSchema("schema_with_seq");

    Status status = catalog->createSequence(schema_id, "seq1", 1, 1, 1000, 1, 1, false, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = catalog->dropSchema(schema_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    std::string msg = ctx.message;
    EXPECT_NE(msg.find("Sequences: 1"), std::string::npos) << msg;
}
