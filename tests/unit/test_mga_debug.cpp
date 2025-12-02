#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/page_manager.h"
#include <cstring>
#include <filesystem>
#include <vector>
#include <iostream>

using namespace scratchbird::core;

class MGADebugTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        cleanup_test_files();
    }

    void TearDown() override
    {
        cleanup_test_files();
    }

    void cleanup_test_files()
    {
        std::filesystem::remove("/tmp/test_mga_debug.db");
        std::filesystem::remove("/tmp/test_tmp_path.sbdb");
    }
};

// Test Database::create with /tmp path (to verify server issue)
TEST_F(MGADebugTest, CreateDatabaseInTmpPath)
{
    const char* path = "/tmp/test_tmp_path.sbdb";
    std::filesystem::remove(path);  // Clean up first

    std::cerr << "Testing Database::create at " << path << std::endl;

    ErrorContext ctx;
    Status status = Database::create(path, 16384, &ctx);
    std::cerr << "Create status: " << static_cast<int>(status) << std::endl;
    if (status != Status::OK) {
        std::cerr << "Create error: " << ctx.message << std::endl;
    }
    ASSERT_EQ(status, Status::OK) << "Failed to create: " << ctx.message;

    // Verify file exists
    ASSERT_TRUE(std::filesystem::exists(path));

    // Now open
    Database db;
    status = db.open(path, &ctx);
    std::cerr << "Open status: " << static_cast<int>(status) << std::endl;
    if (status != Status::OK) {
        std::cerr << "Open error: " << ctx.message << std::endl;
    }
    ASSERT_EQ(status, Status::OK) << "Failed to open: " << ctx.message;

    std::cerr << "Database in /tmp created and opened successfully!" << std::endl;
    db.close();
    std::filesystem::remove(path);
}

// Debug test to trace exactly where StorageEngine::insertTuple hangs
TEST_F(MGADebugTest, DebugInsertTuple)
{
    std::cerr << "DEBUG: Creating database...\n";
    ASSERT_EQ(Database::create("/tmp/test_mga_debug.db", 8192), Status::OK);

    std::cerr << "DEBUG: Opening database...\n";
    Database db;
    ASSERT_EQ(db.open("/tmp/test_mga_debug.db"), Status::OK);

    std::cerr << "DEBUG: Getting catalog_manager...\n";
    CatalogManager* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    // Create schema
    std::cerr << "DEBUG: Creating schema...\n";
    ErrorContext ctx;
    ID schema_id;
    Status status = catalog->createSchema("test_schema", "test_user", schema_id, &ctx);
    std::cerr << "DEBUG: createSchema returned status=" << static_cast<int>(status) << "\n";

    if (status == Status::DUPLICATE_OBJECT) {
        CatalogManager::SchemaInfo schema_info;
        status = catalog->getSchema("test_schema", schema_info, &ctx);
        schema_id = schema_info.schema_id;
    }
    ASSERT_TRUE(status == Status::OK || status == Status::DUPLICATE_OBJECT) << ctx.message;

    // Create table
    std::cerr << "DEBUG: Creating table...\n";
    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo col;
    col.column_name = "data";
    col.data_type = 25; // TEXT type
    col.max_length = 0;
    col.nullable = true;
    columns.push_back(col);

    ID table_id;
    status = catalog->createTable(schema_id, "test_table", columns, table_id, 0, &ctx);
    std::cerr << "DEBUG: createTable returned status=" << static_cast<int>(status) << "\n";
    ASSERT_EQ(status, Status::OK) << "Failed to create table: " << ctx.message;

    // Get storage engine
    std::cerr << "DEBUG: Getting storage_engine...\n";
    StorageEngine* engine = db.storage_engine();
    ASSERT_NE(engine, nullptr);

    // Check page manager state
    std::cerr << "DEBUG: Getting page_manager...\n";
    PageManager* pm = db.page_manager();
    ASSERT_NE(pm, nullptr);

    uint32_t total_pages = pm->totalPages();
    std::cerr << "DEBUG: totalPages=" << total_pages << "\n";

    // Prepare tuple data
    std::cerr << "DEBUG: Preparing tuple data (50 bytes)...\n";
    std::vector<uint8_t> small_data(50, 0xAA);
    uint32_t page_id;
    uint16_t item_id;

    // Try to insert tuple - this is where it hangs
    std::cerr << "DEBUG: Calling insertTuple...\n";
    std::cerr << "DEBUG: About to call insertTuple...\n";
    std::cerr.flush();

    status = engine->insertTuple(
        table_id,
        small_data.data(),
        small_data.size(),
        &page_id,
        &item_id,
        &ctx
    );

    std::cerr << "DEBUG: insertTuple returned status=" << static_cast<int>(status) << "\n";
    ASSERT_EQ(status, Status::OK) << "Failed to insert tuple: " << ctx.message;

    std::cerr << "DEBUG: Insert succeeded at page_id=" << page_id << ", item_id=" << item_id << "\n";

    db.close();
    std::cerr << "DEBUG: Test complete!\n";
}
