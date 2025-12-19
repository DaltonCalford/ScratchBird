#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;

class DependencyIntegrationTest : public ::testing::Test
{
protected:
    std::string test_db_path;
    Database *db = nullptr;
    CatalogManager *catalog = nullptr;
    ID schema_id;

    void SetUp() override
    {
        test_db_path = "/tmp/test_dep_integ_" + std::to_string(getpid()) + ".sbdb";
        std::remove(test_db_path.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path, 16384, &ctx), Status::OK);

        db = new Database();
        ASSERT_EQ(db->open(test_db_path, &ctx), Status::OK);
        catalog = db->catalog_manager();
        ASSERT_NE(catalog, nullptr);

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog->getSchema("PUBLIC", schema_info, &ctx), Status::OK);
        schema_id = schema_info.schema_id;
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

    ID createTable(const std::string& name)
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
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }
};

TEST_F(DependencyIntegrationTest, TableViewViewChainBlocksDrop)
{
    ErrorContext ctx;
    ID table_id = createTable("chain_table");

    ID view1_id = generateUuidV7();
    ID view2_id = generateUuidV7();

    ID dep1;
    ASSERT_EQ(catalog->createDependency(
        view1_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep1, &ctx), Status::OK);

    ID dep2;
    ASSERT_EQ(catalog->createDependency(
        view2_id, CatalogManager::ObjectType::VIEW,
        view1_id, CatalogManager::ObjectType::VIEW,
        CatalogManager::DependencyType::NORMAL, dep2, &ctx), Status::OK);

    Status status = catalog->dropTable(table_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
}
