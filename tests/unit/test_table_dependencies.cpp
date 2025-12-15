#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/parser/ast_v2.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;

class TableDependencyTest : public ::testing::Test
{
protected:
    std::string test_db_path;
    Database *db = nullptr;
    CatalogManager *catalog = nullptr;
    ID schema_id;

    void SetUp() override
    {
        test_db_path = "/tmp/test_table_dep_" + std::to_string(getpid()) + ".sbdb";
        std::remove(test_db_path.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path, 16384, &ctx), Status::OK);

        db = new Database();
        ASSERT_EQ(db->open(test_db_path, &ctx), Status::OK);

        catalog = db->catalog_manager();
        ASSERT_NE(catalog, nullptr);

        // Get PUBLIC schema
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

    // Helper: Create a simple table with one column
    ID createTestTable(const std::string& table_name)
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
        Status status = catalog->createTable(schema_id, table_name, columns, table_id, 0, &ctx);
        if (status != Status::OK) {
            ADD_FAILURE() << "Failed to create table: " << ctx.message;
        }
        return table_id;
    }

    // Helper: Create index on table
    ID createTestIndex(const ID& table_id, const std::string& index_name)
    {
        ErrorContext ctx;
        ID index_id;
        std::vector<std::string> col_names = {"id"};
        Status status = catalog->createIndex(table_id, index_name, col_names,
                                            index_id, false, CatalogManager::IndexType::BTREE, 0, &ctx);
        if (status != Status::OK) {
            ADD_FAILURE() << "Failed to create index: " << ctx.message;
        }
        return index_id;
    }

    // Helper: Create trigger on table
    ID createTestTrigger(const ID& table_id, const std::string& trigger_name)
    {
        ErrorContext ctx;
        CatalogManager::TriggerInfo trigger;
        trigger.trigger_id = generateUuidV7();
        trigger.trigger_name = trigger_name;
        trigger.table_id = table_id;
        trigger.event = CatalogManager::TriggerEvent::INSERT;
        trigger.timing = CatalogManager::TriggerTiming::BEFORE;
        trigger.enabled = true;

        Status status = catalog->createTrigger(trigger, &ctx);
        if (status != Status::OK) {
            ADD_FAILURE() << "Failed to create trigger: " << ctx.message;
        }
        return trigger.trigger_id;
    }

    // Helper: Create FK from child to parent table
    ID createTestFK(const ID& child_table_id, const ID& parent_table_id,
                    const std::string& fk_name)
    {
        ErrorContext ctx;

        // Both tables have "id" column
        std::vector<std::string> child_col_names = {"id"};
        std::vector<std::string> parent_col_names = {"id"};

        ID fk_id;
        Status status = catalog->createForeignKey(
            fk_name,
            child_table_id, parent_table_id,
            child_col_names, parent_col_names,
            CatalogManager::FKAction::NO_ACTION,
            CatalogManager::FKAction::NO_ACTION,
            CatalogManager::FKMatchType::SIMPLE,
            fk_id,
            false, false,
            &ctx
        );
        if (status != Status::OK) {
            ADD_FAILURE() << "Failed to create FK: " << ctx.message;
        }
        return fk_id;
    }
};

// ========================================================================
// TEST 1: DROP TABLE fails if view depends on table
// ========================================================================

TEST_F(TableDependencyTest, DropTableFailsIfViewDepends)
{
    ErrorContext ctx;

    // Create base table
    ID table_id = createTestTable("base_table");

    // Create view that depends on table (manually register dependency)
    ID view_id = generateUuidV7();
    ID dep_id;
    ASSERT_EQ(catalog->createDependency(
        view_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL,
        dep_id,
        &ctx), Status::OK);

    // Try to drop table - should fail
    Status status = catalog->dropTable(table_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("depend"), std::string::npos)
        << "Error message should mention dependencies: " << ctx.message;

    // Verify table still exists
    CatalogManager::TableInfo table_info;
    EXPECT_EQ(catalog->getTable(table_id, table_info, &ctx), Status::OK);
}

// ========================================================================
// TEST 2: DROP TABLE fails if parent-side FK references table
// ========================================================================

TEST_F(TableDependencyTest, DropTableFailsIfParentFK)
{
    ErrorContext ctx;

    // Create parent and child tables
    ID parent_id = createTestTable("parent_table");
    ID child_id = createTestTable("child_table");

    // Create FK: child -> parent
    ID fk_id = createTestFK(child_id, parent_id, "fk_test");

    // Try to drop parent table - should fail (parent-side FK blocks)
    Status status = catalog->dropTable(parent_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("depend"), std::string::npos)
        << "Error message should mention dependencies: " << ctx.message;

    // Verify parent table still exists
    CatalogManager::TableInfo parent_info;
    EXPECT_EQ(catalog->getTable(parent_id, parent_info, &ctx), Status::OK);
}

// ========================================================================
// TEST 3: DROP TABLE succeeds and auto-drops indexes
// ========================================================================

TEST_F(TableDependencyTest, DropTableAutoDropsIndexes)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("table_with_indexes");

    // Create multiple indexes
    ID idx1_id = createTestIndex(table_id, "idx1");
    ID idx2_id = createTestIndex(table_id, "idx2");

    // Verify indexes exist
    CatalogManager::IndexInfo idx_info;
    ASSERT_EQ(catalog->getIndex(idx1_id, idx_info, &ctx), Status::OK);
    ASSERT_EQ(catalog->getIndex(idx2_id, idx_info, &ctx), Status::OK);

    // Drop table - should succeed and auto-drop indexes
    Status status = catalog->dropTable(table_id, false, &ctx);
    if (status != Status::OK) {
        std::cerr << "DROP TABLE failed with status: " << static_cast<int>(status)
                  << ", message: '" << ctx.message << "'" << std::endl;
    }
    EXPECT_EQ(status, Status::OK) << "DROP TABLE should succeed: " << ctx.message;

    // Verify table is gone
    CatalogManager::TableInfo table_info;
    EXPECT_NE(catalog->getTable(table_id, table_info, &ctx), Status::OK);

    // Verify indexes are gone
    EXPECT_NE(catalog->getIndex(idx1_id, idx_info, &ctx), Status::OK);
    EXPECT_NE(catalog->getIndex(idx2_id, idx_info, &ctx), Status::OK);
}

// ========================================================================
// TEST 4: DROP TABLE succeeds and auto-drops triggers
// ========================================================================

TEST_F(TableDependencyTest, DropTableAutoDropsTriggers)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("table_with_triggers");

    // Create triggers
    ID trig1_id = createTestTrigger(table_id, "trig1");
    ID trig2_id = createTestTrigger(table_id, "trig2");

    // Verify triggers exist
    CatalogManager::TriggerInfo trig_info;
    ASSERT_EQ(catalog->getTrigger(trig1_id, trig_info, &ctx), Status::OK);
    ASSERT_EQ(catalog->getTrigger(trig2_id, trig_info, &ctx), Status::OK);

    // Drop table - should succeed and auto-drop triggers
    Status status = catalog->dropTable(table_id, false, &ctx);
    EXPECT_EQ(status, Status::OK) << "DROP TABLE should succeed: " << ctx.message;

    // Verify table is gone
    CatalogManager::TableInfo table_info;
    EXPECT_NE(catalog->getTable(table_id, table_info, &ctx), Status::OK);

    // Verify triggers are gone
    EXPECT_NE(catalog->getTrigger(trig1_id, trig_info, &ctx), Status::OK);
    EXPECT_NE(catalog->getTrigger(trig2_id, trig_info, &ctx), Status::OK);
}

// ========================================================================
// TEST 5: DROP TABLE succeeds and auto-drops child-side FKs
// ========================================================================

TEST_F(TableDependencyTest, DropTableAutoDropsChildFK)
{
    ErrorContext ctx;

    // Create parent and child tables
    ID parent_id = createTestTable("parent_table_2");
    ID child_id = createTestTable("child_table_2");

    // Create FK: child -> parent
    ID fk_id = createTestFK(child_id, parent_id, "fk_test_2");

    // Verify FK exists
    CatalogManager::ForeignKeyInfo fk_info;
    ASSERT_EQ(catalog->getForeignKey(fk_id, fk_info, &ctx), Status::OK);

    // Drop child table - should succeed and auto-drop FK
    Status status = catalog->dropTable(child_id, false, &ctx);
    EXPECT_EQ(status, Status::OK) << "DROP TABLE should succeed: " << ctx.message;

    // Verify child table is gone
    CatalogManager::TableInfo child_info;
    EXPECT_NE(catalog->getTable(child_id, child_info, &ctx), Status::OK);

    // Verify FK is gone
    EXPECT_NE(catalog->getForeignKey(fk_id, fk_info, &ctx), Status::OK);

    // Verify parent table still exists
    CatalogManager::TableInfo parent_info;
    EXPECT_EQ(catalog->getTable(parent_id, parent_info, &ctx), Status::OK);
}

// ========================================================================
// TEST 6: DROP TABLE error message lists all blocking dependencies
// ========================================================================

TEST_F(TableDependencyTest, DropTableErrorListsAllDependencies)
{
    ErrorContext ctx;

    // Create base table
    ID table_id = createTestTable("base_table_multi_dep");

    // Create multiple blocking dependencies
    ID view1_id = generateUuidV7();
    ID view2_id = generateUuidV7();
    ID func_id = generateUuidV7();

    ID dep1, dep2, dep3;
    ASSERT_EQ(catalog->createDependency(
        view1_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep1, &ctx), Status::OK);

    ASSERT_EQ(catalog->createDependency(
        view2_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep2, &ctx), Status::OK);

    ASSERT_EQ(catalog->createDependency(
        func_id, CatalogManager::ObjectType::FUNCTION,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep3, &ctx), Status::OK);

    // Try to drop table
    Status status = catalog->dropTable(table_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);

    // Verify error message is comprehensive
    std::string error_msg(ctx.message);
    EXPECT_NE(error_msg.find("depend"), std::string::npos)
        << "Should mention dependencies";

    // Should list both views and function (note: may not have names in error since
    // we didn't actually create the view/function objects, just the dependencies)
    EXPECT_NE(error_msg.find("VIEW"), std::string::npos)
        << "Should mention VIEW type";
    EXPECT_NE(error_msg.find("FUNCTION"), std::string::npos)
        << "Should mention FUNCTION type";
}

// ========================================================================
// TEST 7: DROP TABLE after dropping dependent view succeeds
// ========================================================================

TEST_F(TableDependencyTest, DropTableAfterDroppingView)
{
    ErrorContext ctx;

    // Create base table
    ID table_id = createTestTable("base_table_view");

    // Create view dependency
    ID view_id = generateUuidV7();
    ID dep_id;
    ASSERT_EQ(catalog->createDependency(
        view_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep_id, &ctx), Status::OK);

    // Try to drop table - should fail
    EXPECT_EQ(catalog->dropTable(table_id, false, &ctx), Status::CONSTRAINT_VIOLATION);

    // Remove the view dependency
    ASSERT_EQ(catalog->clearDependenciesFor(view_id, &ctx), Status::OK);

    // Now drop table - should succeed
    Status status = catalog->dropTable(table_id, false, &ctx);
    EXPECT_EQ(status, Status::OK) << "DROP TABLE should succeed after dependency removed: "
                                  << ctx.message;

    // Verify table is gone
    CatalogManager::TableInfo table_info;
    EXPECT_NE(catalog->getTable(table_id, table_info, &ctx), Status::OK);
}

// ========================================================================
// TEST 8: Complex scenario - table with mixed dependencies
// ========================================================================

TEST_F(TableDependencyTest, ComplexMixedDependencies)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("complex_table");

    // Add owned objects (should auto-drop)
    ID idx_id = createTestIndex(table_id, "complex_idx");
    ID trig_id = createTestTrigger(table_id, "complex_trig");

    // Add blocking dependency (should prevent drop)
    ID view_id = generateUuidV7();
    ID dep_id;
    ASSERT_EQ(catalog->createDependency(
        view_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep_id, &ctx), Status::OK);

    // Try to drop - should fail due to view
    EXPECT_EQ(catalog->dropTable(table_id, false, &ctx), Status::CONSTRAINT_VIOLATION);

    // Verify owned objects still exist
    CatalogManager::IndexInfo idx_info;
    CatalogManager::TriggerInfo trig_info;
    EXPECT_EQ(catalog->getIndex(idx_id, idx_info, &ctx), Status::OK);
    EXPECT_EQ(catalog->getTrigger(trig_id, trig_info, &ctx), Status::OK);

    // Remove blocking dependency
    ASSERT_EQ(catalog->clearDependenciesFor(view_id, &ctx), Status::OK);

    // Now drop should succeed and auto-drop owned objects
    Status status = catalog->dropTable(table_id, false, &ctx);
    EXPECT_EQ(status, Status::OK) << "DROP TABLE should succeed: " << ctx.message;

    // Verify everything is gone
    CatalogManager::TableInfo table_info;
    EXPECT_NE(catalog->getTable(table_id, table_info, &ctx), Status::OK);
    EXPECT_NE(catalog->getIndex(idx_id, idx_info, &ctx), Status::OK);
    EXPECT_NE(catalog->getTrigger(trig_id, trig_info, &ctx), Status::OK);
}

// ========================================================================
// TEST 9: Verify dependencies are properly tracked
// ========================================================================

TEST_F(TableDependencyTest, DependencyTrackingVerification)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("tracked_table");

    // Create index and trigger
    ID idx_id = createTestIndex(table_id, "tracked_idx");
    ID trig_id = createTestTrigger(table_id, "tracked_trig");

    // Get all dependents of the table
    std::vector<CatalogManager::DependencyInfo> deps;
    ASSERT_EQ(catalog->getDependents(table_id, deps, &ctx), Status::OK);

    // Should have at least 2 dependencies (index and trigger)
    EXPECT_GE(deps.size(), 2) << "Should have at least index and trigger dependencies";

    // Verify we have index and trigger dependencies
    bool found_index = false;
    bool found_trigger = false;

    for (const auto& dep : deps) {
        if (dep.dependent_type == CatalogManager::ObjectType::INDEX &&
            dep.dependent_object_id == idx_id) {
            found_index = true;
            EXPECT_EQ(dep.dependency_type, CatalogManager::DependencyType::AUTO);
        }
        if (dep.dependent_type == CatalogManager::ObjectType::TRIGGER &&
            dep.dependent_object_id == trig_id) {
            found_trigger = true;
            EXPECT_EQ(dep.dependency_type, CatalogManager::DependencyType::AUTO);
        }
    }

    EXPECT_TRUE(found_index) << "Should have index dependency";
    EXPECT_TRUE(found_trigger) << "Should have trigger dependency";
}

// ========================================================================
// TEST 10: Foreign key dependency types
// ========================================================================

TEST_F(TableDependencyTest, ForeignKeyDependencyTypes)
{
    ErrorContext ctx;

    // Create parent and child tables
    ID parent_id = createTestTable("fk_parent");
    ID child_id = createTestTable("fk_child");

    // Create FK
    ID fk_id = createTestFK(child_id, parent_id, "fk_dep_test");

    // Get dependencies for parent table
    std::vector<CatalogManager::DependencyInfo> parent_deps;
    ASSERT_EQ(catalog->getDependents(parent_id, parent_deps, &ctx), Status::OK);

    // Parent should have NORMAL dependency (blocks drop)
    bool found_normal_dep = false;
    for (const auto& dep : parent_deps) {
        if (dep.dependent_object_id == fk_id &&
            dep.dependent_type == CatalogManager::ObjectType::CONSTRAINT) {
            EXPECT_EQ(dep.dependency_type, CatalogManager::DependencyType::NORMAL);
            found_normal_dep = true;
        }
    }
    EXPECT_TRUE(found_normal_dep) << "Parent should have NORMAL FK dependency";

    // Get dependencies for child table
    std::vector<CatalogManager::DependencyInfo> child_deps;
    ASSERT_EQ(catalog->getDependents(child_id, child_deps, &ctx), Status::OK);

    // Child should have AUTO dependency (auto-drops)
    bool found_auto_dep = false;
    for (const auto& dep : child_deps) {
        if (dep.dependent_object_id == fk_id &&
            dep.dependent_type == CatalogManager::ObjectType::CONSTRAINT) {
            EXPECT_EQ(dep.dependency_type, CatalogManager::DependencyType::AUTO);
            found_auto_dep = true;
        }
    }
    EXPECT_TRUE(found_auto_dep) << "Child should have AUTO FK dependency";
}
