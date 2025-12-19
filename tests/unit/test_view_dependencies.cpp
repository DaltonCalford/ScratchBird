#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;

class ViewDependencyTest : public ::testing::Test
{
protected:
    std::string test_db_path;
    Database *db = nullptr;
    CatalogManager *catalog = nullptr;
    ID schema_id;

    void SetUp() override
    {
        test_db_path = "/tmp/test_view_dep_" + std::to_string(getpid()) + ".sbdb";
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

    ID createView(const std::string& name)
    {
        ErrorContext ctx;
        Status status = catalog->createView(schema_id, name, "SELECT 1", false,
                                            false, false, {}, ID{}, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        ID view_id;
        status = catalog->getViewIdByName(name, view_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return view_id;
    }
};

TEST_F(ViewDependencyTest, DropViewFailsWhenDependentViewExists)
{
    ErrorContext ctx;
    ID base_view_id = createView("base_view");
    ID dependent_view_id = createView("dependent_view");

    ID dep_id;
    ASSERT_EQ(catalog->createDependency(
        dependent_view_id, CatalogManager::ObjectType::VIEW,
        base_view_id, CatalogManager::ObjectType::VIEW,
        CatalogManager::DependencyType::NORMAL, dep_id, &ctx), Status::OK);

    Status status = catalog->dropView(base_view_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("view"), std::string::npos) << ctx.message;
}

TEST_F(ViewDependencyTest, DropViewSucceedsAfterRemovingDependency)
{
    ErrorContext ctx;
    ID base_view_id = createView("base_view2");
    ID dependent_view_id = createView("dependent_view2");

    ID dep_id;
    ASSERT_EQ(catalog->createDependency(
        dependent_view_id, CatalogManager::ObjectType::VIEW,
        base_view_id, CatalogManager::ObjectType::VIEW,
        CatalogManager::DependencyType::NORMAL, dep_id, &ctx), Status::OK);

    ASSERT_EQ(catalog->clearDependenciesFor(dependent_view_id, &ctx), Status::OK);

    Status status = catalog->dropView(base_view_id, false, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
}
