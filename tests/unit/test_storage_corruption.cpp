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
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace scratchbird::core;

class StorageCorruptionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Generate unique database path for this test
        test_db_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_corrupt");
    }

    void TearDown() override
    {
        test_db_.reset();
    }

    const std::string& test_db_path() const { return test_db_->path(); }

    Status createTestTable(Database *db, ID *table_id_out, ErrorContext *ctx)
    {
        if (!db || !table_id_out)
        {
            if (ctx) ctx->message = "Invalid database or table_id_out";
            return Status::INVALID_ARGUMENT;
        }

        auto *catalog = db->catalog_manager();
        if (!catalog)
        {
            if (ctx) ctx->message = "CatalogManager not available";
            return Status::INTERNAL_ERROR;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog->listSchemas(schemas, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        ID schema_id{};
        if (schemas.empty())
        {
            status = catalog->createSchema("public", "test", schema_id, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }
        else
        {
            schema_id = schemas[0].schema_id;
        }

        std::vector<CatalogManager::ColumnInfo> columns;
        CatalogManager::ColumnInfo data_col;
        data_col.column_name = "data";
        data_col.data_type = static_cast<uint16_t>(DataType::BYTEA);
        data_col.max_length = 0;
        data_col.nullable = true;
        data_col.has_default = false;
        columns.push_back(data_col);

        return catalog->createTable(schema_id, "corrupt_test_table", columns, *table_id_out, 0, ctx);
    }

    // Helper to corrupt specific bytes in file
    void corrupt_file(const std::string &filename, size_t offset, const uint8_t *data, size_t size)
    {
        std::fstream file(filename, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(file.is_open());
        file.seekp(offset);
        file.write(reinterpret_cast<const char *>(data), size);
        file.close();
    }

    // Helper to read bytes from file
    void read_file_bytes(const std::string &filename, size_t offset, uint8_t *buffer, size_t size)
    {
        std::ifstream file(filename, std::ios::binary);
        ASSERT_TRUE(file.is_open());
        file.seekg(offset);
        file.read(reinterpret_cast<char *>(buffer), size);
        file.close();
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_;
};

// Test: Corrupt page header magic number
TEST_F(StorageCorruptionTest, CorruptPageHeaderMagic)
{
    // Create database with some data
    const uint32_t page_size = 8192;
    ASSERT_EQ(Database::create(test_db_path(), page_size), Status::OK);
    ID table_id{};
    uint32_t heap_page_id = 0;

    {
        Database db;
        ErrorContext ctx;
        ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(createTestTable(&db, &table_id, &ctx), Status::OK) << ctx.message;

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(100, 0xAA);

        uint16_t item_id;
        ASSERT_EQ(engine.insertTuple(table_id, tuple_data.data(), tuple_data.size(),
                                      &heap_page_id, &item_id, nullptr),
                  Status::OK);

        db.close();
    }

    // Corrupt the magic number in a heap page
    uint32_t bad_magic = 0xDEADBEEF;
    corrupt_file(test_db_path(), heap_page_id * page_size,
                 reinterpret_cast<uint8_t *>(&bad_magic), sizeof(bad_magic));

    // Try to open and scan
    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;

    StorageEngine engine(&db);
    auto iterator = engine.createScan(table_id, nullptr);

    // Should detect corruption when trying to read the page
    Tuple tuple;
    Status status = iterator->next(&tuple, &ctx);

    // Expect some form of corruption error
    EXPECT_NE(status, Status::OK) << "Should detect corrupted magic number";

    db.close();
}

// Test: Corrupt page checksum
TEST_F(StorageCorruptionTest, CorruptPageChecksum)
{
    const uint32_t page_size = 8192;
    ASSERT_EQ(Database::create(test_db_path(), page_size), Status::OK);
    ID table_id{};
    uint32_t heap_page_id = 0;

    {
        Database db;
        ErrorContext ctx;
        ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(createTestTable(&db, &table_id, &ctx), Status::OK) << ctx.message;

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(200, 0xBB);

        uint16_t item_id;
        ASSERT_EQ(engine.insertTuple(table_id, tuple_data.data(), tuple_data.size(),
                                      &heap_page_id, &item_id, nullptr),
                  Status::OK);

        db.sync(nullptr); // Ensure written
        db.close();
    }

    // Ensure checksum validation is enforced for this page.
    uint32_t page_flags = 0;
    read_file_bytes(test_db_path(),
                    heap_page_id * page_size + offsetof(PageHeader, flags),
                    reinterpret_cast<uint8_t *>(&page_flags),
                    sizeof(page_flags));
    page_flags |= PAGE_FLAG_CHECKSUM_VALID;
    corrupt_file(test_db_path(),
                 heap_page_id * page_size + offsetof(PageHeader, flags),
                 reinterpret_cast<const uint8_t *>(&page_flags),
                 sizeof(page_flags));

    // Read current checksum and corrupt it
    uint8_t checksum_bytes[4];
    read_file_bytes(test_db_path(), heap_page_id * page_size + 0x0C,
                    checksum_bytes, 4);

    // Flip some bits
    checksum_bytes[0] ^= 0xFF;
    checksum_bytes[1] ^= 0xFF;

    corrupt_file(test_db_path(), heap_page_id * page_size + 0x0C, checksum_bytes, 4);

    // Try to read the corrupted page
    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;

    uint8_t *buffer = new uint8_t[8192];
    Status status = db.read_page(heap_page_id, buffer, &ctx);

    // Should detect checksum mismatch
    EXPECT_TRUE(status == Status::PAGE_CORRUPT || status == Status::CHECKSUM_MISMATCH)
        << "Should detect checksum corruption; status=" << static_cast<uint32_t>(status)
        << " ctx=" << ctx.message;
    (void)ctx;

    delete[] buffer;
    db.close();
}

// Test: Corrupt item pointer
TEST_F(StorageCorruptionTest, CorruptItemPointer)
{
    ASSERT_EQ(Database::create(test_db_path(), 8192), Status::OK);

    uint32_t stored_page_id;
    uint16_t stored_item_id;
    ID table_id{};

    {
        Database db;
        ErrorContext ctx;
        ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(createTestTable(&db, &table_id, &ctx), Status::OK) << ctx.message;

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(100, 0xCC);

        ASSERT_EQ(engine.insertTuple(table_id, tuple_data.data(), tuple_data.size(),
                                      &stored_page_id, &stored_item_id, nullptr),
                  Status::OK);

        db.sync(nullptr);
        db.close();
    }

    // Corrupt the item pointer to point beyond page boundary
    size_t item_pointer_offset =
        stored_page_id * 8192 + sizeof(PageHeader) + stored_item_id * sizeof(ItemPointer);

    ItemPointer bad_item;
    bad_item.offset = 9000; // Beyond page size
    bad_item.length = 100;
    bad_item.flags = 0;

    corrupt_file(test_db_path(), item_pointer_offset, reinterpret_cast<uint8_t *>(&bad_item),
                 sizeof(ItemPointer));

    // Try to read the tuple
    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;

    StorageEngine engine(&db);
    Tuple tuple;
    Status status = engine.getTuple(stored_page_id, stored_item_id, &tuple, &ctx);

    // Should detect invalid offset
    EXPECT_NE(status, Status::OK) << "Should detect invalid item pointer";

    db.close();
}

// Test: Corrupt tuple header
TEST_F(StorageCorruptionTest, CorruptTupleHeader)
{
    const uint32_t page_size = 8192;
    ASSERT_EQ(Database::create(test_db_path(), page_size), Status::OK);
    ID table_id{};
    uint32_t heap_page_id = 0;

    {
        Database db;
        ErrorContext ctx;
        ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(createTestTable(&db, &table_id, &ctx), Status::OK) << ctx.message;

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(150, 0xDD);

        // Insert multiple tuples
        for (int i = 0; i < 10; i++)
        {
            uint32_t page_id;
            uint16_t item_id;
            ASSERT_EQ(engine.insertTuple(table_id, tuple_data.data(),
                                          tuple_data.size(), &page_id,
                                          &item_id, nullptr),
                      Status::OK);
            if (i == 0)
            {
                heap_page_id = page_id;
            }
        }

        db.sync(nullptr);
        db.close();
    }

    // Corrupt a tuple header with invalid transaction IDs
    TupleHeader bad_header;
    bad_header.xmin = UINT64_MAX; // Invalid transaction ID
    bad_header.xmax = UINT64_MAX - 1;
    bad_header.back_version_gpid = 0;
    bad_header.back_version_slot = 0;
    bad_header.ctid_gpid = 0;
    bad_header.ctid_slot = 0;
    bad_header.infomask = 0xFFFF; // All flags set
    bad_header.null_bitmap_offset = 0xFFFF;
    bad_header.padding = 0;

    // Corrupt somewhere in the middle of page 7
    corrupt_file(test_db_path(), heap_page_id * page_size + 4000, reinterpret_cast<uint8_t *>(&bad_header),
                 sizeof(TupleHeader));

    // Try to scan
    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;

    StorageEngine engine(&db);
    auto iterator = engine.createScan(table_id, nullptr);

    int valid_tuples = 0;
    int errors = 0;
    Tuple tuple;

    while (!iterator->isDone())
    {
        Status status = iterator->next(&tuple, nullptr);
        if (status == Status::OK)
        {
            valid_tuples++;
        }
        else
        {
            errors++;
            // Move past the error
            break;
        }
    }

    // Should have found some valid tuples and hit an error
    EXPECT_GT(valid_tuples, 0) << "Should read some valid tuples";
    EXPECT_GT(errors, 0) << "Should encounter corruption";

    db.close();
}

// Test: Corrupt page special area
TEST_F(StorageCorruptionTest, CorruptPageSpecialArea)
{
    const uint32_t page_size = 8192;
    ASSERT_EQ(Database::create(test_db_path(), page_size), Status::OK);
    ID table_id{};
    uint32_t heap_page_id = 0;

    {
        Database db;
        ErrorContext ctx;
        ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(createTestTable(&db, &table_id, &ctx), Status::OK) << ctx.message;

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(300, 0xEE);

        uint16_t item_id;
        ASSERT_EQ(engine.insertTuple(table_id, tuple_data.data(), tuple_data.size(),
                                      &heap_page_id, &item_id, nullptr),
                  Status::OK);

        db.sync(nullptr);
        db.close();
    }

    // Corrupt the page header offsets
    PageHeader bad_header{};
    bad_header.magic = K_MAGIC_SBRD;
    bad_header.page_size = page_size;
    pageSetLower(bad_header, page_size + 1000); // Invalid - beyond page
    pageSetUpper(bad_header, 100);              // Invalid - upper < lower
    pageSetSpecial(bad_header, 0);

    size_t header_offset = static_cast<size_t>(heap_page_id) * page_size;
    corrupt_file(test_db_path(), header_offset, reinterpret_cast<uint8_t *>(&bad_header),
                 sizeof(PageHeader));

    // Try to insert into the corrupted page
    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;

    StorageEngine engine(&db);
    std::vector<uint8_t> tuple_data(100, 0xFF);

    uint32_t page_id;
    uint16_t item_id;

    // This might allocate a new page or detect corruption
    Status status = engine.insertTuple(
        table_id, tuple_data.data(), tuple_data.size(), &page_id, &item_id, &ctx);

    // Should either detect corruption or allocate new page
    if (status == Status::OK)
    {
        EXPECT_NE(page_id, 7u) << "Should not use corrupted page";
    }

    db.close();
}

// Test: Truncated file (short read)
TEST_F(StorageCorruptionTest, TruncatedFile)
{
    ASSERT_EQ(Database::create(test_db_path(), 8192), Status::OK);
    ID table_id{};

    {
        Database db;
        ErrorContext ctx;
        ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(createTestTable(&db, &table_id, &ctx), Status::OK) << ctx.message;

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(400, 0x11);

        // Insert data across multiple pages
        for (int i = 0; i < 100; i++)
        {
            uint32_t page_id;
            uint16_t item_id;
            engine.insertTuple(table_id, tuple_data.data(), tuple_data.size(),
                                &page_id, &item_id, nullptr);
        }

        db.sync(nullptr);
        db.close();
    }

    // Truncate the file
    std::filesystem::resize_file(test_db_path(), 8192 * 5 + 100); // Partial page

    // Try to open and scan
    Database db;
    ErrorContext ctx;
    Status status = db.open(test_db_path(), &ctx);

    // Opening might succeed, but accessing truncated pages should fail
    if (status == Status::OK)
    {
        StorageEngine engine(&db);
        auto iterator = engine.createScan(table_id, nullptr);

        int tuples_read = 0;
        bool hit_error = false;
        Tuple tuple;

        while (!iterator->isDone() && !hit_error)
        {
            status = iterator->next(&tuple, &ctx);
            if (status == Status::OK)
            {
                tuples_read++;
            }
            else
            {
                hit_error = true;
            }
        }

        EXPECT_TRUE(hit_error) << "Should detect truncated file";
        db.close();
    }
}

// Test: Zero-filled page
TEST_F(StorageCorruptionTest, ZeroFilledPage)
{
    const uint32_t page_size = 8192;
    ASSERT_EQ(Database::create(test_db_path(), page_size), Status::OK);
    ID table_id{};
    uint32_t heap_page_id = 0;

    {
        Database db;
        ErrorContext ctx;
        ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(createTestTable(&db, &table_id, &ctx), Status::OK) << ctx.message;

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(200, 0x22);

        uint16_t item_id;
        ASSERT_EQ(engine.insertTuple(table_id, tuple_data.data(), tuple_data.size(),
                                      &heap_page_id, &item_id, nullptr),
                  Status::OK);

        db.sync(nullptr);
        db.close();
    }

    // Zero out an entire page
    std::vector<uint8_t> zeros(page_size, 0);
    corrupt_file(test_db_path(), heap_page_id * page_size, zeros.data(), zeros.size());

    // Try to read the zero page
    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;

    uint8_t *buffer = new uint8_t[8192];
    Status status = db.read_page(heap_page_id, buffer, &ctx);

    // Should detect invalid magic number (zero)
    EXPECT_EQ(status, Status::PAGE_CORRUPT) << "Should detect zero page as corrupt";

    delete[] buffer;
    db.close();
}

// Test: Page type mismatch
TEST_F(StorageCorruptionTest, PageTypeMismatch)
{
    const uint32_t page_size = 8192;
    ASSERT_EQ(Database::create(test_db_path(), page_size), Status::OK);
    ID table_id{};
    uint32_t heap_page_id = 0;

    {
        Database db;
        ErrorContext ctx;
        ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(createTestTable(&db, &table_id, &ctx), Status::OK) << ctx.message;

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(150, 0x33);

        uint16_t item_id;
        ASSERT_EQ(engine.insertTuple(table_id, tuple_data.data(), tuple_data.size(),
                                      &heap_page_id, &item_id, nullptr),
                  Status::OK);

        db.sync(nullptr);
        db.close();
    }

    // Change page type to something invalid for heap pages
    uint16_t invalid_type = PAGE_TYPE_DATABASE_HEADER; // Wrong type for heap page
    corrupt_file(test_db_path(), heap_page_id * page_size + 0x06,     // Page type offset
                 reinterpret_cast<uint8_t *>(&invalid_type), sizeof(invalid_type));

    // Try to scan
    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;

    StorageEngine engine(&db);
    auto iterator = engine.createScan(table_id, nullptr);

    Tuple tuple;
    Status status = iterator->next(&tuple, &ctx);

    // Should handle page type mismatch gracefully
    EXPECT_NE(status, Status::OK) << "Should detect page type mismatch";

    db.close();
}

// Test: Recovery from corruption
TEST_F(StorageCorruptionTest, RecoveryFromCorruption)
{
    const uint32_t page_size = 8192;
    ASSERT_EQ(Database::create(test_db_path(), page_size), Status::OK);
    ID table_id{};
    uint32_t heap_page_id = 0;

    {
        Database db;
        ErrorContext ctx;
        ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(createTestTable(&db, &table_id, &ctx), Status::OK) << ctx.message;

        StorageEngine engine(&db);

        // Insert tuples across multiple pages
        std::vector<uint8_t> tuple_data(2000, 0x44);
        std::vector<uint32_t> pages;
        for (int i = 0; i < 5000 && pages.size() < 2; i++)
        {
            uint32_t page_id;
            uint16_t item_id;
            engine.insertTuple(table_id, tuple_data.data(), tuple_data.size(),
                                &page_id, &item_id, nullptr);
            if (pages.empty() || pages.back() != page_id)
            {
                pages.push_back(page_id);
            }
        }
        ASSERT_GE(pages.size(), 2u) << "Test requires at least two heap pages";
        heap_page_id = pages[1];

        db.sync(nullptr);
        db.close();
    }

    // Corrupt one page in the middle
    uint32_t bad_magic = 0xBADBADBA;
    corrupt_file(test_db_path(), heap_page_id * page_size,
                 reinterpret_cast<uint8_t *>(&bad_magic), sizeof(bad_magic));

    // Open and try to continue working
    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(test_db_path(), &ctx), Status::OK) << ctx.message;

    StorageEngine engine(&db);

    // Should be able to insert new data (might skip corrupted page)
    std::vector<uint8_t> new_data(100, 0x55);
    uint32_t page_id;
    uint16_t item_id;
    Status status = engine.insertTuple(table_id, new_data.data(), new_data.size(),
                                        &page_id, &item_id, nullptr);

    EXPECT_EQ(status, Status::OK) << "Should be able to insert despite corruption";

    // Scan should work but skip corrupted pages
    auto iterator = engine.createScan(table_id, nullptr);
    int valid_tuples = 0;
    int errors = 0;
    Tuple tuple;

    while (!iterator->isDone())
    {
        status = iterator->next(&tuple, nullptr);
        if (status == Status::OK)
        {
            valid_tuples++;
        }
        else
        {
            errors++;
            // In a real system, would skip to next page
            break;
        }
    }

    std::cout << "\nRecovery test: read " << valid_tuples << " valid tuples, encountered " << errors
              << " errors\n";

    EXPECT_GT(valid_tuples, 0) << "Should read some valid data";

    db.close();
}
