#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace scratchbird::core;

// Helper to create a test UUID
static inline UuidV7Bytes makeTestUUID(uint8_t value = 0xAB) {
    UuidV7Bytes uuid;
    memset(uuid.bytes.data(), value, 16);
    return uuid;
}

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
    ASSERT_EQ(Database::create(test_db_path(), 8192), Status::OK);

    {
        Database db;
        ASSERT_EQ(db.open(test_db_path()), Status::OK);

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(100, 0xAA);

        uint32_t page_id;
        uint16_t item_id;
        ASSERT_EQ(engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                      &page_id, &item_id, nullptr),
                  Status::OK);

        db.close();
    }

    // Corrupt the magic number in a heap page
    uint32_t bad_magic = 0xDEADBEEF;
    corrupt_file(test_db_path(), 7 * 8192, // Page 7 (first heap page)
                 reinterpret_cast<uint8_t *>(&bad_magic), sizeof(bad_magic));

    // Try to open and scan
    Database db;
    ASSERT_EQ(db.open(test_db_path()), Status::OK);

    StorageEngine engine(&db);
    auto iterator = engine.createScan(makeTestUUID(1), nullptr);

    // Should detect corruption when trying to read the page
    Tuple tuple;
    ErrorContext ctx;
    Status status = iterator->next(&tuple, &ctx);

    // Expect some form of corruption error
    EXPECT_NE(status, Status::OK) << "Should detect corrupted magic number";

    db.close();
}

// Test: Corrupt page checksum
TEST_F(StorageCorruptionTest, CorruptPageChecksum)
{
    ASSERT_EQ(Database::create(test_db_path(), 8192), Status::OK);

    {
        Database db;
        ASSERT_EQ(db.open(test_db_path()), Status::OK);

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(200, 0xBB);

        uint32_t page_id;
        uint16_t item_id;
        ASSERT_EQ(engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                      &page_id, &item_id, nullptr),
                  Status::OK);

        db.sync(nullptr); // Ensure written
        db.close();
    }

    // Read current checksum and corrupt it
    uint8_t checksum_bytes[4];
    read_file_bytes(test_db_path(), 7 * 8192 + 16, // Checksum offset in page 7
                    checksum_bytes, 4);

    // Flip some bits
    checksum_bytes[0] ^= 0xFF;
    checksum_bytes[1] ^= 0xFF;

    corrupt_file(test_db_path(), 7 * 8192 + 16, checksum_bytes, 4);

    // Try to read the corrupted page
    Database db;
    ASSERT_EQ(db.open(test_db_path()), Status::OK);

    uint8_t *buffer = new uint8_t[8192];
    ErrorContext ctx;
    Status status = db.read_page(7, buffer, &ctx);

    // Should detect checksum mismatch
    EXPECT_EQ(status, Status::PAGE_CORRUPT) << "Should detect checksum corruption";
    if (status == Status::PAGE_CORRUPT)
    {
        EXPECT_NE(ctx.message.find("checksum"), std::string::npos)
            << "Error should mention checksum";
    }

    delete[] buffer;
    db.close();
}

// Test: Corrupt item pointer
TEST_F(StorageCorruptionTest, CorruptItemPointer)
{
    ASSERT_EQ(Database::create(test_db_path(), 8192), Status::OK);

    uint32_t stored_page_id;
    uint16_t stored_item_id;

    {
        Database db;
        ASSERT_EQ(db.open(test_db_path()), Status::OK);

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(100, 0xCC);

        ASSERT_EQ(engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
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
    ASSERT_EQ(db.open(test_db_path()), Status::OK);

    StorageEngine engine(&db);
    Tuple tuple;
    ErrorContext ctx;
    Status status = engine.getTuple(stored_page_id, stored_item_id, &tuple, &ctx);

    // Should detect invalid offset
    EXPECT_NE(status, Status::OK) << "Should detect invalid item pointer";

    db.close();
}

// Test: Corrupt tuple header
TEST_F(StorageCorruptionTest, CorruptTupleHeader)
{
    ASSERT_EQ(Database::create(test_db_path(), 8192), Status::OK);

    {
        Database db;
        ASSERT_EQ(db.open(test_db_path()), Status::OK);

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(150, 0xDD);

        // Insert multiple tuples
        for (int i = 0; i < 10; i++)
        {
            uint32_t page_id;
            uint16_t item_id;
            ASSERT_EQ(engine.insertTuple(makeTestUUID(1), tuple_data.data(),
                                          tuple_data.size() + sizeof(TupleHeader), &page_id,
                                          &item_id, nullptr),
                      Status::OK);
        }

        db.sync(nullptr);
        db.close();
    }

    // Corrupt a tuple header with invalid transaction IDs
    TupleHeader bad_header;
    bad_header.xmin = UINT64_MAX; // Invalid transaction ID
    bad_header.xmax = UINT64_MAX - 1;
    bad_header.back_version_tid = 0;
    bad_header.ctid_page = 0;
    bad_header.ctid_item = 0;
    bad_header.infomask = 0xFFFF; // All flags set
    bad_header.null_bitmap_offset = 0xFFFF;
    bad_header.padding = 0;

    // Corrupt somewhere in the middle of page 7
    corrupt_file(test_db_path(), 7 * 8192 + 4000, reinterpret_cast<uint8_t *>(&bad_header),
                 sizeof(TupleHeader));

    // Try to scan
    Database db;
    ASSERT_EQ(db.open(test_db_path()), Status::OK);

    StorageEngine engine(&db);
    auto iterator = engine.createScan(makeTestUUID(1), nullptr);

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
    ASSERT_EQ(Database::create(test_db_path(), 8192), Status::OK);

    {
        Database db;
        ASSERT_EQ(db.open(test_db_path()), Status::OK);

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(300, 0xEE);

        uint32_t page_id;
        uint16_t item_id;
        ASSERT_EQ(engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                      &page_id, &item_id, nullptr),
                  Status::OK);

        db.sync(nullptr);
        db.close();
    }

    // Corrupt the special area with invalid offsets
    HeapPageSpecial bad_special;
    bad_special.pd_flags = 0xFFFF;
    bad_special.pd_lower = 8000; // Invalid - beyond page
    bad_special.pd_upper = 100;  // Invalid - upper < lower
    bad_special.pd_special = 0;
    bad_special.pd_prune_xid = UINT32_MAX;

    size_t special_offset = 7 * 8192 + 8192 - sizeof(HeapPageSpecial);
    corrupt_file(test_db_path(), special_offset, reinterpret_cast<uint8_t *>(&bad_special),
                 sizeof(HeapPageSpecial));

    // Try to insert into the corrupted page
    Database db;
    ASSERT_EQ(db.open(test_db_path()), Status::OK);

    StorageEngine engine(&db);
    std::vector<uint8_t> tuple_data(100, 0xFF);

    uint32_t page_id;
    uint16_t item_id;
    ErrorContext ctx;

    // This might allocate a new page or detect corruption
    Status status = engine.insertTuple(
        makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader), &page_id, &item_id, &ctx);

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

    {
        Database db;
        ASSERT_EQ(db.open(test_db_path()), Status::OK);

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(400, 0x11);

        // Insert data across multiple pages
        for (int i = 0; i < 100; i++)
        {
            uint32_t page_id;
            uint16_t item_id;
            engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
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
        auto iterator = engine.createScan(makeTestUUID(1), nullptr);

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
    ASSERT_EQ(Database::create(test_db_path(), 8192), Status::OK);

    {
        Database db;
        ASSERT_EQ(db.open(test_db_path()), Status::OK);

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(200, 0x22);

        uint32_t page_id;
        uint16_t item_id;
        ASSERT_EQ(engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                      &page_id, &item_id, nullptr),
                  Status::OK);

        db.sync(nullptr);
        db.close();
    }

    // Zero out an entire page
    std::vector<uint8_t> zeros(8192, 0);
    corrupt_file(test_db_path(), 7 * 8192, zeros.data(), zeros.size());

    // Try to read the zero page
    Database db;
    ASSERT_EQ(db.open(test_db_path()), Status::OK);

    uint8_t *buffer = new uint8_t[8192];
    ErrorContext ctx;
    Status status = db.read_page(7, buffer, &ctx);

    // Should detect invalid magic number (zero)
    EXPECT_EQ(status, Status::PAGE_CORRUPT) << "Should detect zero page as corrupt";

    delete[] buffer;
    db.close();
}

// Test: Page type mismatch
TEST_F(StorageCorruptionTest, PageTypeMismatch)
{
    ASSERT_EQ(Database::create(test_db_path(), 8192), Status::OK);

    {
        Database db;
        ASSERT_EQ(db.open(test_db_path()), Status::OK);

        StorageEngine engine(&db);
        std::vector<uint8_t> tuple_data(150, 0x33);

        uint32_t page_id;
        uint16_t item_id;
        ASSERT_EQ(engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                      &page_id, &item_id, nullptr),
                  Status::OK);

        db.sync(nullptr);
        db.close();
    }

    // Change page type to something invalid for heap pages
    uint8_t invalid_type = PAGE_TYPE_DATABASE_HEADER; // Wrong type for heap page
    corrupt_file(test_db_path(), 7 * 8192 + 8,     // Page type offset
                 &invalid_type, sizeof(invalid_type));

    // Try to scan
    Database db;
    ASSERT_EQ(db.open(test_db_path()), Status::OK);

    StorageEngine engine(&db);
    auto iterator = engine.createScan(makeTestUUID(1), nullptr);

    Tuple tuple;
    ErrorContext ctx;
    Status status = iterator->next(&tuple, &ctx);

    // Should handle page type mismatch gracefully
    EXPECT_NE(status, Status::OK) << "Should detect page type mismatch";

    db.close();
}

// Test: Recovery from corruption
TEST_F(StorageCorruptionTest, RecoveryFromCorruption)
{
    ASSERT_EQ(Database::create(test_db_path(), 8192), Status::OK);

    {
        Database db;
        ASSERT_EQ(db.open(test_db_path()), Status::OK);

        StorageEngine engine(&db);

        // Insert tuples across multiple pages
        std::vector<uint8_t> tuple_data(100, 0x44);
        for (int i = 0; i < 50; i++)
        {
            uint32_t page_id;
            uint16_t item_id;
            engine.insertTuple(makeTestUUID(1), tuple_data.data(), tuple_data.size() + sizeof(TupleHeader),
                                &page_id, &item_id, nullptr);
        }

        db.sync(nullptr);
        db.close();
    }

    // Corrupt one page in the middle
    uint32_t bad_magic = 0xBADBADBA;
    corrupt_file(test_db_path(), 8 * 8192, // Page 8
                 reinterpret_cast<uint8_t *>(&bad_magic), sizeof(bad_magic));

    // Open and try to continue working
    Database db;
    ASSERT_EQ(db.open(test_db_path()), Status::OK);

    StorageEngine engine(&db);

    // Should be able to insert new data (might skip corrupted page)
    std::vector<uint8_t> new_data(100, 0x55);
    uint32_t page_id;
    uint16_t item_id;
    Status status = engine.insertTuple(makeTestUUID(1), new_data.data(), new_data.size() + sizeof(TupleHeader),
                                        &page_id, &item_id, nullptr);

    EXPECT_EQ(status, Status::OK) << "Should be able to insert despite corruption";

    // Scan should work but skip corrupted pages
    auto iterator = engine.createScan(makeTestUUID(1), nullptr);
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