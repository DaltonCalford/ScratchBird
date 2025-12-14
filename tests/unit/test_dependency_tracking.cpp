#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;

class DependencyTrackingTest : public ::testing::Test
{
protected:
    std::string test_db_path;
    Database *db = nullptr;

    void SetUp() override
    {
        test_db_path = "/tmp/test_dependency_" + std::to_string(getpid()) + ".db";
        // Remove any existing test file
        std::remove(test_db_path.c_str());
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
    }

    void CreateAndOpenDatabase(uint32_t page_size = 16384)
    {
        ErrorContext ctx;

        // Create database
        ASSERT_EQ(Database::create(test_db_path, page_size, &ctx), Status::OK);

        // Open database
        db = new Database();
        Status status = db->open(test_db_path, &ctx);
        if (status != Status::OK)
        {
            std::cerr << "Failed to open database: " << ctx.message << std::endl;
        }
        ASSERT_EQ(status, Status::OK);
    }
};

// ========================================================================
// DEPENDENCY TRACKING TESTS (Phase 1: Infrastructure)
// ========================================================================

TEST_F(DependencyTrackingTest, CreateAndGetDependents)
{
    CreateAndOpenDatabase();
    ErrorContext ctx;

    CatalogManager *catalog = db->catalog_manager();

    // Create test objects (just use IDs - actual objects don't need to exist for dependency tracking)
    ID table_id = generateUuidV7();
    ID view1_id = generateUuidV7();
    ID view2_id = generateUuidV7();

    // Create dependencies: view1 and view2 depend on table
    ID dep1_id, dep2_id;
    ASSERT_EQ(catalog->createDependency(
        view1_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep1_id,
        &ctx), Status::OK);

    ASSERT_EQ(catalog->createDependency(
        view2_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep2_id,
        &ctx), Status::OK);

    // Get dependents
    std::vector<CatalogManager::DependencyInfo> dependents;
    ASSERT_EQ(catalog->getDependents(table_id, dependents, &ctx), Status::OK);

    // Should have 2 dependents
    EXPECT_EQ(dependents.size(), 2);

    // Verify dependency information
    bool found_view1 = false;
    bool found_view2 = false;
    for (const auto& dep : dependents) {
        EXPECT_EQ(dep.referenced_object_id, table_id);
        EXPECT_EQ(dep.referenced_type, CatalogManager::ObjectType::TABLE);
        EXPECT_EQ(dep.dependent_type, CatalogManager::ObjectType::VIEW);

        if (dep.dependent_object_id == view1_id) found_view1 = true;
        if (dep.dependent_object_id == view2_id) found_view2 = true;
    }
    EXPECT_TRUE(found_view1);
    EXPECT_TRUE(found_view2);
}

TEST_F(DependencyTrackingTest, ClearDependencies)
{
    CreateAndOpenDatabase();
    ErrorContext ctx;

    CatalogManager *catalog = db->catalog_manager();

    // Create dependencies
    ID table_id = generateUuidV7();
    ID view_id = generateUuidV7();
    ID dep_id;

    ASSERT_EQ(catalog->createDependency(
        view_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep_id,
        &ctx), Status::OK);

    // Verify dependency exists
    std::vector<CatalogManager::DependencyInfo> dependents;
    ASSERT_EQ(catalog->getDependents(table_id, dependents, &ctx), Status::OK);
    EXPECT_EQ(dependents.size(), 1);

    // Clear dependencies (clearDependenciesFor clears deps where the object is the DEPENDENT)
    ASSERT_EQ(catalog->clearDependenciesFor(view_id, &ctx), Status::OK);

    // Verify dependencies cleared (table should no longer have the view as a dependent)
    dependents.clear();
    ASSERT_EQ(catalog->getDependents(table_id, dependents, &ctx), Status::OK);
    EXPECT_EQ(dependents.size(), 0);
}

TEST_F(DependencyTrackingTest, ReplaceDependencies)
{
    CreateAndOpenDatabase();
    ErrorContext ctx;

    CatalogManager *catalog = db->catalog_manager();

    // Create test objects
    ID table1_id = generateUuidV7();
    ID table2_id = generateUuidV7();
    ID table3_id = generateUuidV7();
    ID view_id = generateUuidV7();

    // Create view that depends on table1 and table2
    ID dep1_id, dep2_id;
    ASSERT_EQ(catalog->createDependency(
        view_id, CatalogManager::ObjectType::VIEW,
        table1_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep1_id,
        &ctx), Status::OK);
    ASSERT_EQ(catalog->createDependency(
        view_id, CatalogManager::ObjectType::VIEW,
        table2_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep2_id,
        &ctx), Status::OK);

    // Replace dependencies: now depend on table2 and table3 (drop table1, keep table2, add table3)
    std::vector<std::pair<ID, CatalogManager::ObjectType>> new_deps;
    new_deps.push_back({table2_id, CatalogManager::ObjectType::TABLE});
    new_deps.push_back({table3_id, CatalogManager::ObjectType::TABLE});

    ASSERT_EQ(catalog->replaceDependencies(view_id, CatalogManager::ObjectType::VIEW, new_deps, &ctx), Status::OK);

    // Verify table1 no longer has view as dependent
    std::vector<CatalogManager::DependencyInfo> table1_deps;
    ASSERT_EQ(catalog->getDependents(table1_id, table1_deps, &ctx), Status::OK);
    EXPECT_EQ(table1_deps.size(), 0);

    // Verify table2 still has view as dependent
    std::vector<CatalogManager::DependencyInfo> table2_deps;
    ASSERT_EQ(catalog->getDependents(table2_id, table2_deps, &ctx), Status::OK);
    EXPECT_EQ(table2_deps.size(), 1);
    EXPECT_EQ(table2_deps[0].dependent_object_id, view_id);

    // Verify table3 now has view as dependent
    std::vector<CatalogManager::DependencyInfo> table3_deps;
    ASSERT_EQ(catalog->getDependents(table3_id, table3_deps, &ctx), Status::OK);
    EXPECT_EQ(table3_deps.size(), 1);
    EXPECT_EQ(table3_deps[0].dependent_object_id, view_id);
}

TEST_F(DependencyTrackingTest, CacheConsistency)
{
    CreateAndOpenDatabase();
    ErrorContext ctx;

    CatalogManager *catalog = db->catalog_manager();

    // Create dependencies
    ID table_id = generateUuidV7();
    ID view1_id = generateUuidV7();
    ID view2_id = generateUuidV7();
    ID func_id = generateUuidV7();
    ID dep1_id, dep2_id, dep3_id;

    ASSERT_EQ(catalog->createDependency(
        view1_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep1_id,
        &ctx), Status::OK);
    ASSERT_EQ(catalog->createDependency(
        view2_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep2_id,
        &ctx), Status::OK);
    ASSERT_EQ(catalog->createDependency(
        func_id, CatalogManager::ObjectType::FUNCTION,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep3_id,
        &ctx), Status::OK);

    // Get dependents multiple times to verify cache consistency
    std::vector<CatalogManager::DependencyInfo> deps1;
    ASSERT_EQ(catalog->getDependents(table_id, deps1, &ctx), Status::OK);
    EXPECT_EQ(deps1.size(), 3);

    std::vector<CatalogManager::DependencyInfo> deps2;
    ASSERT_EQ(catalog->getDependents(table_id, deps2, &ctx), Status::OK);
    EXPECT_EQ(deps2.size(), 3);

    // Verify same results
    EXPECT_EQ(deps1.size(), deps2.size());
    for (size_t i = 0; i < deps1.size(); i++) {
        bool found = false;
        for (const auto& dep : deps2) {
            if (dep.dependent_object_id == deps1[i].dependent_object_id &&
                dep.dependent_type == deps1[i].dependent_type) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }

    // Modify dependencies and verify cache updates
    ASSERT_EQ(catalog->clearDependenciesFor(view2_id, &ctx), Status::OK);

    // Verify updated dependencies
    std::vector<CatalogManager::DependencyInfo> deps3;
    ASSERT_EQ(catalog->getDependents(table_id, deps3, &ctx), Status::OK);
    // Should still have view1 and func dependencies
    EXPECT_GE(deps3.size(), 2);
}

TEST_F(DependencyTrackingTest, MultipleDependencyTypes)
{
    CreateAndOpenDatabase();
    ErrorContext ctx;

    CatalogManager *catalog = db->catalog_manager();

    // Create table with dependencies of different types
    ID table_id = generateUuidV7();
    ID view_id = generateUuidV7();
    ID func_id = generateUuidV7();
    ID proc_id = generateUuidV7();
    ID trigger_id = generateUuidV7();
    ID dep1_id, dep2_id, dep3_id, dep4_id;

    ASSERT_EQ(catalog->createDependency(
        view_id, CatalogManager::ObjectType::VIEW,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep1_id,
        &ctx), Status::OK);
    ASSERT_EQ(catalog->createDependency(
        func_id, CatalogManager::ObjectType::FUNCTION,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep2_id,
        &ctx), Status::OK);
    ASSERT_EQ(catalog->createDependency(
        proc_id, CatalogManager::ObjectType::PROCEDURE,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep3_id,
        &ctx), Status::OK);
    ASSERT_EQ(catalog->createDependency(
        trigger_id, CatalogManager::ObjectType::TRIGGER,
        table_id, CatalogManager::ObjectType::TABLE,
        CatalogManager::DependencyType::NORMAL, dep4_id,
        &ctx), Status::OK);

    // Get all dependents
    std::vector<CatalogManager::DependencyInfo> dependents;
    ASSERT_EQ(catalog->getDependents(table_id, dependents, &ctx), Status::OK);

    EXPECT_EQ(dependents.size(), 4);

    // Count by type
    int views = 0, functions = 0, procedures = 0, triggers = 0;
    for (const auto& dep : dependents) {
        switch (dep.dependent_type) {
            case CatalogManager::ObjectType::VIEW: views++; break;
            case CatalogManager::ObjectType::FUNCTION: functions++; break;
            case CatalogManager::ObjectType::PROCEDURE: procedures++; break;
            case CatalogManager::ObjectType::TRIGGER: triggers++; break;
            default: break;
        }
    }

    EXPECT_EQ(views, 1);
    EXPECT_EQ(functions, 1);
    EXPECT_EQ(procedures, 1);
    EXPECT_EQ(triggers, 1);
}
