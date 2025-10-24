/**
 * Test Suite: Catalog MGA Compliance
 *
 * Purpose: Verify that catalog operations correctly implement Firebird MGA
 *          instead of PostgreSQL MVCC.
 *
 * Bugs Tested:
 *  - Bug #1: ALTER TABLESPACE should update records IN-PLACE (not append)
 *  - Bug #2: DROP/DETACH TABLESPACE should mark is_valid=0 (not leave orphaned)
 *
 * Reference: docs/MGA_COMPLIANCE_REVIEW_TABLESPACE.md
 */

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/page.h"
#include <cstdio>
#include <string>

using namespace scratchbird::core;

class CatalogMGAComplianceTest : public ::testing::Test
{
protected:
    std::string test_db_path;
    Database *db = nullptr;

    void SetUp() override
    {
        test_db_path = "/tmp/test_catalog_mga_" + std::to_string(getpid()) + ".db";
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
        ASSERT_EQ(Database::create(test_db_path, page_size, &ctx), Status::OK);
        db = new Database();
        Status status = db->open(test_db_path, &ctx);
        if (status != Status::OK)
        {
            std::cerr << "Failed to open database: " << ctx.message << std::endl;
        }
        ASSERT_EQ(status, Status::OK);
    }

    // Helper to count catalog records for a tablespace
    uint32_t countTablespaceRecords(const std::string &tablespace_name)
    {
        ErrorContext ctx;
        CatalogManager *catalog = db->catalog_manager();

        std::vector<CatalogManager::TablespaceInfo> tablespaces;
        catalog->listTablespaces(tablespaces, &ctx);

        uint32_t count = 0;
        for (const auto &ts : tablespaces)
        {
            if (ts.tablespace_name == tablespace_name)
            {
                count++;
            }
        }
        return count;
    }
};

/**
 * Test Bug #1 Fix: ALTER TABLESPACE Updates In-Place (No Catalog Bloat)
 *
 * Before Fix: ALTER TABLESPACE would append new catalog record (PostgreSQL MVCC)
 * After Fix: ALTER TABLESPACE updates existing catalog record in-place (Firebird MGA)
 *
 * This test verifies:
 * 1. CREATE TABLESPACE creates exactly ONE catalog record
 * 2. Multiple ALTER TABLESPACE operations DO NOT create duplicate records
 * 3. Catalog record count remains 1 after many ALTERs
 * 4. Final ALTER values are correctly persisted
 */
TEST_F(CatalogMGAComplianceTest, AlterTablespaceNoCatalogBloat)
{
    CreateAndOpenDatabase();
    ErrorContext ctx;
    CatalogManager *catalog = db->catalog_manager();

    // Create a tablespace
    std::string ts_path = "/tmp/test_ts_alter_" + std::to_string(getpid()) + ".sbts";
    std::string ts_name = "test_alter_ts";

    uint16_t ts_id;
    ASSERT_EQ(catalog->createTablespace(ts_name, ts_path, 16384, true, ts_id, 0, &ctx),
              Status::OK);

    // Verify: Should have exactly ONE catalog record after CREATE
    EXPECT_EQ(countTablespaceRecords(ts_name), 1);

    // Perform 10 ALTER TABLESPACE operations (toggle AUTOEXTEND)
    for (int i = 0; i < 10; i++)
    {
        bool autoextend = (i % 2 == 0);  // Toggle ON/OFF
        ASSERT_EQ(catalog->alterTablespace(ts_name, autoextend, 0, &ctx), Status::OK);

        // Verify: Should STILL have exactly ONE catalog record (no bloat)
        EXPECT_EQ(countTablespaceRecords(ts_name), 1)
            << "Catalog bloat detected after ALTER #" << (i + 1);
    }

    // Verify final state is persisted correctly
    CatalogManager::TablespaceInfo ts_info;
    ASSERT_EQ(catalog->getTablespaceByName(ts_name, ts_info, &ctx), Status::OK);
    EXPECT_FALSE(ts_info.autoextend);  // Last ALTER set autoextend=OFF (i=9, i%2=1)

    // Cleanup
    std::remove(ts_path.c_str());
}

/**
 * Test Bug #1 Fix: Catalog Record Count Accurate After Multiple ALTERs
 *
 * This test verifies that catalog record_count does NOT grow on UPDATE operations.
 * Before the fix, record_count would increment incorrectly on every ALTER.
 */
TEST_F(CatalogMGAComplianceTest, CatalogRecordCountAccurate)
{
    CreateAndOpenDatabase();
    ErrorContext ctx;
    CatalogManager *catalog = db->catalog_manager();

    // Create two tablespaces
    std::string ts1_path = "/tmp/test_ts1_" + std::to_string(getpid()) + ".sbts";
    std::string ts2_path = "/tmp/test_ts2_" + std::to_string(getpid()) + ".sbts";

    uint16_t ts1_id, ts2_id;
    ASSERT_EQ(catalog->createTablespace("ts1", ts1_path, 16384, true, ts1_id, 0, &ctx),
              Status::OK);
    ASSERT_EQ(catalog->createTablespace("ts2", ts2_path, 16384, true, ts2_id, 0, &ctx),
              Status::OK);

    // List tablespaces (should have PRIMARY + ts1 + ts2 = 3 total)
    std::vector<CatalogManager::TablespaceInfo> tablespaces_before;
    ASSERT_EQ(catalog->listTablespaces(tablespaces_before, &ctx), Status::OK);
    size_t initial_count = tablespaces_before.size();
    EXPECT_GE(initial_count, 3);  // At least PRIMARY, ts1, ts2

    // Perform 50 ALTER operations on ts1
    for (int i = 0; i < 50; i++)
    {
        ASSERT_EQ(catalog->alterTablespace("ts1", (i % 2 == 0), 0, &ctx), Status::OK);
    }

    // List tablespaces again (should have SAME count, not 50 additional records)
    std::vector<CatalogManager::TablespaceInfo> tablespaces_after;
    ASSERT_EQ(catalog->listTablespaces(tablespaces_after, &ctx), Status::OK);

    EXPECT_EQ(tablespaces_after.size(), initial_count)
        << "Catalog record count grew after ALTER operations (MVCC bug)";

    // Cleanup
    std::remove(ts1_path.c_str());
    std::remove(ts2_path.c_str());
}

/**
 * Test Bug #2 Fix: DROP TABLESPACE Marks Catalog Record as Deleted
 *
 * Before Fix: DROP TABLESPACE only removed from cache (catalog record remained valid)
 * After Fix: DROP TABLESPACE marks catalog record as is_valid=0 (Firebird MGA deletion)
 *
 * This test verifies:
 * 1. DROP TABLESPACE removes tablespace from listing
 * 2. Tablespace does NOT reappear after database restart
 * 3. Catalog record is marked as deleted (not just removed from cache)
 */
TEST_F(CatalogMGAComplianceTest, DropTablespacePersistsDeletion)
{
    // Create tablespace
    {
        CreateAndOpenDatabase();
        ErrorContext ctx;
        CatalogManager *catalog = db->catalog_manager();

        std::string ts_path = "/tmp/test_ts_drop_" + std::to_string(getpid()) + ".sbts";
        std::string ts_name = "test_drop_ts";

        uint16_t ts_id;
        ASSERT_EQ(catalog->createTablespace(ts_name, ts_path, 16384, true, ts_id, 0, &ctx),
                  Status::OK);

        // Verify tablespace exists
        EXPECT_EQ(countTablespaceRecords(ts_name), 1);

        // Drop the tablespace
        ASSERT_EQ(catalog->dropTablespace(ts_name, false, &ctx), Status::OK);

        // Verify tablespace is removed from listing
        EXPECT_EQ(countTablespaceRecords(ts_name), 0)
            << "Tablespace still visible after DROP";

        // Close database
        db->close();
        delete db;
        db = nullptr;

        std::remove(ts_path.c_str());
    }

    // Reopen database and verify tablespace stays deleted
    {
        ErrorContext ctx;
        db = new Database();
        ASSERT_EQ(db->open(test_db_path, &ctx), Status::OK);

        CatalogManager *catalog = db->catalog_manager();

        // Verify tablespace does NOT reappear (before fix, it would reappear)
        EXPECT_EQ(countTablespaceRecords("test_drop_ts"), 0)
            << "Tablespace reappeared after restart (Bug #2 not fixed)";

        // Try to access the dropped tablespace (should fail)
        CatalogManager::TablespaceInfo ts_info;
        Status status = catalog->getTablespaceByName("test_drop_ts", ts_info, &ctx);
        EXPECT_EQ(status, Status::NOT_FOUND)
            << "Dropped tablespace still accessible";
    }
}

/**
 * Test Bug #2 Fix: DETACH TABLESPACE Marks Catalog Record as Deleted
 *
 * Same as DROP test, but for DETACH operation.
 */
TEST_F(CatalogMGAComplianceTest, DetachTablespacePersistsDeletion)
{
    // Create and detach tablespace
    {
        CreateAndOpenDatabase();
        ErrorContext ctx;
        CatalogManager *catalog = db->catalog_manager();

        std::string ts_path = "/tmp/test_ts_detach_" + std::to_string(getpid()) + ".sbts";
        std::string ts_name = "test_detach_ts";

        uint16_t ts_id;
        ASSERT_EQ(catalog->createTablespace(ts_name, ts_path, 16384, true, ts_id, 0, &ctx),
                  Status::OK);

        // Verify tablespace exists
        EXPECT_EQ(countTablespaceRecords(ts_name), 1);

        // Detach the tablespace
        ASSERT_EQ(catalog->detachTablespace(ts_name, false, &ctx), Status::OK);

        // Verify tablespace is removed from listing
        EXPECT_EQ(countTablespaceRecords(ts_name), 0)
            << "Tablespace still visible after DETACH";

        // Close database
        db->close();
        delete db;
        db = nullptr;
    }

    // Reopen database and verify tablespace stays detached
    {
        ErrorContext ctx;
        db = new Database();
        ASSERT_EQ(db->open(test_db_path, &ctx), Status::OK);

        CatalogManager *catalog = db->catalog_manager();

        // Verify tablespace does NOT reappear
        EXPECT_EQ(countTablespaceRecords("test_detach_ts"), 0)
            << "Tablespace reappeared after restart (Bug #2 not fixed)";

        // Try to access the detached tablespace (should fail)
        CatalogManager::TablespaceInfo ts_info;
        Status status = catalog->getTablespaceByName("test_detach_ts", ts_info, &ctx);
        EXPECT_EQ(status, Status::NOT_FOUND)
            << "Detached tablespace still accessible";
    }

    // Cleanup
    std::string ts_path = "/tmp/test_ts_detach_" + std::to_string(getpid()) + ".sbts";
    std::remove(ts_path.c_str());
}

/**
 * Test Bug #2 Fix: Multiple DROP/CREATE Cycles Don't Bloat Catalog
 *
 * Verifies that repeated CREATE/DROP cycles don't accumulate deleted records.
 */
TEST_F(CatalogMGAComplianceTest, MultipleDropCreateCyclesNoBloat)
{
    CreateAndOpenDatabase();
    ErrorContext ctx;
    CatalogManager *catalog = db->catalog_manager();

    std::string ts_path_template = "/tmp/test_ts_cycle_" + std::to_string(getpid());
    std::string ts_name = "test_cycle_ts";

    // Get initial tablespace count
    std::vector<CatalogManager::TablespaceInfo> initial_ts;
    ASSERT_EQ(catalog->listTablespaces(initial_ts, &ctx), Status::OK);
    size_t initial_count = initial_ts.size();

    // Perform 20 CREATE/DROP cycles
    for (int i = 0; i < 20; i++)
    {
        std::string ts_path = ts_path_template + "_" + std::to_string(i) + ".sbts";

        uint16_t ts_id;
        ASSERT_EQ(catalog->createTablespace(ts_name, ts_path, 16384, true, ts_id, 0, &ctx),
                  Status::OK);

        EXPECT_EQ(countTablespaceRecords(ts_name), 1);

        ASSERT_EQ(catalog->dropTablespace(ts_name, false, &ctx), Status::OK);

        EXPECT_EQ(countTablespaceRecords(ts_name), 0);

        std::remove(ts_path.c_str());
    }

    // Verify total tablespace count hasn't grown (no catalog bloat from deleted records)
    std::vector<CatalogManager::TablespaceInfo> final_ts;
    ASSERT_EQ(catalog->listTablespaces(final_ts, &ctx), Status::OK);

    EXPECT_EQ(final_ts.size(), initial_count)
        << "Catalog bloated after repeated CREATE/DROP cycles";
}

/**
 * Test Combined: ALTER + DROP/CREATE Cycles
 *
 * Stress test combining both Bug #1 and Bug #2 fixes.
 */
TEST_F(CatalogMGAComplianceTest, CombinedAlterDropCreateStress)
{
    CreateAndOpenDatabase();
    ErrorContext ctx;
    CatalogManager *catalog = db->catalog_manager();

    std::string ts_path = "/tmp/test_ts_combined_" + std::to_string(getpid()) + ".sbts";
    std::string ts_name = "test_combined_ts";

    // Cycle: CREATE → 10 ALTERs → DROP (repeat 5 times)
    for (int cycle = 0; cycle < 5; cycle++)
    {
        uint16_t ts_id;
        ASSERT_EQ(catalog->createTablespace(ts_name, ts_path, 16384, true, ts_id, 0, &ctx),
                  Status::OK);

        // Verify single record after CREATE
        EXPECT_EQ(countTablespaceRecords(ts_name), 1);

        // Perform 10 ALTERs
        for (int i = 0; i < 10; i++)
        {
            ASSERT_EQ(catalog->alterTablespace(ts_name, (i % 2 == 0), 0, &ctx), Status::OK);

            // Verify still single record (Bug #1 fix)
            EXPECT_EQ(countTablespaceRecords(ts_name), 1);
        }

        // DROP
        ASSERT_EQ(catalog->dropTablespace(ts_name, false, &ctx), Status::OK);

        // Verify zero records after DROP (Bug #2 fix)
        EXPECT_EQ(countTablespaceRecords(ts_name), 0);

        std::remove(ts_path.c_str());
    }
}

/**
 * Test Regression: CREATE uses append (correct behavior)
 *
 * Verify that CREATE TABLESPACE still correctly appends new records.
 * The MGA fix should only affect UPDATE/DELETE, not INSERT.
 */
TEST_F(CatalogMGAComplianceTest, CreateTablespaceStillAppendsCorrectly)
{
    CreateAndOpenDatabase();
    ErrorContext ctx;
    CatalogManager *catalog = db->catalog_manager();

    // Get initial count
    std::vector<CatalogManager::TablespaceInfo> initial_ts;
    ASSERT_EQ(catalog->listTablespaces(initial_ts, &ctx), Status::OK);
    size_t initial_count = initial_ts.size();

    // Create 3 new tablespaces
    std::vector<std::string> ts_paths;
    for (int i = 0; i < 3; i++)
    {
        std::string ts_path = "/tmp/test_ts_create_" + std::to_string(getpid()) +
                             "_" + std::to_string(i) + ".sbts";
        std::string ts_name = "test_create_ts_" + std::to_string(i);

        uint16_t ts_id;
        ASSERT_EQ(catalog->createTablespace(ts_name, ts_path, 16384, true, ts_id, 0, &ctx),
                  Status::OK);

        ts_paths.push_back(ts_path);
    }

    // Verify count increased by 3 (CREATE still appends correctly)
    std::vector<CatalogManager::TablespaceInfo> final_ts;
    ASSERT_EQ(catalog->listTablespaces(final_ts, &ctx), Status::OK);

    EXPECT_EQ(final_ts.size(), initial_count + 3)
        << "CREATE TABLESPACE not appending records correctly (regression)";

    // Cleanup
    for (const auto &path : ts_paths)
    {
        std::remove(path.c_str());
    }
}
