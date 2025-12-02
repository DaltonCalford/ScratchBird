/**
 * @file test_storage_boundaries.cpp
 * @brief Storage Boundary Condition Tests
 *
 * Suite 1 (Data Integrity & On-Disk Format) Compliance Tests
 *
 * Replaces test_storage_boundary.cpp.disabled with tests using current API.
 * Tests boundary conditions for storage operations including:
 * - Maximum tuple sizes
 * - Minimum tuple sizes
 * - Page capacity limits
 * - Edge cases for different page sizes
 */

#include <gtest/gtest.h>
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"
#include <filesystem>
#include <vector>
#include <cstring>

using namespace scratchbird::core;

class StorageBoundariesTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Generate unique database path for this test
        test_db_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_boundaries");
    }

    void TearDown() override
    {
        db_.reset();
        test_db_.reset();
    }

    void createDatabase(uint32_t page_size)
    {
        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_->path(), page_size, &ctx), Status::OK)
            << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_->path(), &ctx), Status::OK)
            << ctx.message;
    }

    std::vector<uint8_t> generatePattern(size_t size, uint8_t pattern)
    {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; i++) {
            data[i] = static_cast<uint8_t>((pattern + i) % 256);
        }
        return data;
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_;
    std::unique_ptr<Database> db_;
};

/**
 * Test: Maximum Tuple Size for 8KB Pages
 *
 * Verifies the maximum tuple that can fit in an 8KB page.
 */
TEST_F(StorageBoundariesTest, MaxTupleSize8K)
{
    const uint32_t PAGE_SIZE = 8192;
    createDatabase(PAGE_SIZE);

    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(PAGE_SIZE);

    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK);

    // Calculate max tuple size
    // Page layout: PageHeader + ItemPointers + Tuple Data + HeapPageSpecial
    size_t page_header_size = sizeof(PageHeader);
    size_t item_pointer_size = sizeof(ItemPointer);
    size_t special_size = sizeof(HeapPageSpecial);
    size_t tuple_header_size = sizeof(TupleHeader);

    // Maximum data size = PAGE_SIZE - headers - alignment
    size_t max_data_size = PAGE_SIZE - page_header_size - item_pointer_size
                           - special_size - tuple_header_size - 64; // Safety margin

    auto max_data = generatePattern(max_data_size, 0xAB);
    uint16_t item_id;

    Status status = page.insertTuple(max_data.data(),
                                      max_data.size() + tuple_header_size,
                                      100, &item_id, &ctx);

    EXPECT_EQ(status, Status::OK)
        << "Should be able to insert maximum-sized tuple in 8KB page";

    if (status == Status::OK) {
        // Verify retrieval
        const uint8_t* retrieved;
        uint32_t size;
        EXPECT_EQ(page.getTuple(item_id, &retrieved, &size, &ctx), Status::OK);
    }
}

/**
 * Test: Maximum Tuple Size for 16KB Pages
 *
 * Verifies the maximum tuple that can fit in a 16KB page.
 */
TEST_F(StorageBoundariesTest, MaxTupleSize16K)
{
    const uint32_t PAGE_SIZE = 16384;
    createDatabase(PAGE_SIZE);

    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(PAGE_SIZE);

    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK);

    // Larger pages can hold larger tuples
    size_t max_data_size = PAGE_SIZE - 256; // Conservative estimate

    auto max_data = generatePattern(max_data_size, 0xCD);
    uint16_t item_id;

    Status status = page.insertTuple(max_data.data(),
                                      max_data.size() + sizeof(TupleHeader),
                                      100, &item_id, &ctx);

    EXPECT_EQ(status, Status::OK)
        << "Should be able to insert large tuple in 16KB page";
}

/**
 * Test: Maximum Tuple Size for 32KB Pages
 *
 * Verifies the maximum tuple that can fit in a 32KB page.
 */
TEST_F(StorageBoundariesTest, MaxTupleSize32K)
{
    const uint32_t PAGE_SIZE = 32768;
    createDatabase(PAGE_SIZE);

    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(PAGE_SIZE);

    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK);

    // Even larger pages
    size_t max_data_size = PAGE_SIZE - 512; // Conservative estimate

    auto max_data = generatePattern(max_data_size, 0xEF);
    uint16_t item_id;

    Status status = page.insertTuple(max_data.data(),
                                      max_data.size() + sizeof(TupleHeader),
                                      100, &item_id, &ctx);

    EXPECT_EQ(status, Status::OK)
        << "Should be able to insert large tuple in 32KB page";
}

/**
 * Test: Tuple Too Large for Page
 *
 * Verifies that attempting to insert a tuple larger than page capacity fails.
 */
TEST_F(StorageBoundariesTest, TupleTooLarge)
{
    const uint32_t PAGE_SIZE = 8192;
    createDatabase(PAGE_SIZE);

    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(PAGE_SIZE);

    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK);

    // Try to insert tuple larger than entire page
    size_t oversized = PAGE_SIZE + 1000;
    auto oversized_data = generatePattern(oversized, 0xFF);
    uint16_t item_id;

    Status status = page.insertTuple(oversized_data.data(),
                                      oversized_data.size() + sizeof(TupleHeader),
                                      100, &item_id, &ctx);

    EXPECT_NE(status, Status::OK)
        << "Inserting oversized tuple should fail";
}

/**
 * Test: Page Capacity - Multiple Tuples
 *
 * Verifies page correctly handles filling to capacity with multiple tuples.
 */
TEST_F(StorageBoundariesTest, PageCapacityMultipleTuples)
{
    const uint32_t PAGE_SIZE = 8192;
    createDatabase(PAGE_SIZE);

    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(PAGE_SIZE);

    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK);

    std::vector<uint16_t> item_ids;
    const size_t TUPLE_SIZE = 500;

    // Keep inserting until page is full
    for (int i = 0; i < 100; i++) {
        auto data = generatePattern(TUPLE_SIZE, static_cast<uint8_t>(i));
        uint16_t item_id;

        Status status = page.insertTuple(data.data(),
                                         data.size() + sizeof(TupleHeader),
                                         100 + i, &item_id, &ctx);

        if (status == Status::OK) {
            item_ids.push_back(item_id);
        } else {
            // Page is full
            break;
        }
    }

    EXPECT_GT(item_ids.size(), 5)
        << "Should be able to insert multiple tuples before page fills";

    EXPECT_LT(item_ids.size(), 100)
        << "Page should eventually fill and reject inserts";

    // Verify all inserted tuples are retrievable
    for (size_t i = 0; i < item_ids.size(); i++) {
        const uint8_t* data;
        uint32_t size;
        EXPECT_EQ(page.getTuple(item_ids[i], &data, &size, &ctx), Status::OK)
            << "Tuple " << i << " should be retrievable";
    }
}

/**
 * Test: Zero-Length Tuple Data
 *
 * Verifies handling of tuples with zero-length data payload.
 */
TEST_F(StorageBoundariesTest, ZeroLengthTuple)
{
    const uint32_t PAGE_SIZE = 8192;
    createDatabase(PAGE_SIZE);

    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(PAGE_SIZE);

    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK);

    // Insert tuple with just TupleHeader, no data
    std::vector<uint8_t> empty(sizeof(TupleHeader), 0);
    uint16_t item_id;

    Status status = page.insertTuple(empty.data(), sizeof(TupleHeader),
                                      100, &item_id, &ctx);

    EXPECT_EQ(status, Status::OK)
        << "Should be able to insert zero-length tuple";

    if (status == Status::OK) {
        const uint8_t* data;
        uint32_t size;
        EXPECT_EQ(page.getTuple(item_id, &data, &size, &ctx), Status::OK);
        EXPECT_GE(size, sizeof(TupleHeader));
    }
}

/**
 * Test: Single Byte Tuple
 *
 * Verifies handling of minimum non-empty tuple (1 byte of data).
 */
TEST_F(StorageBoundariesTest, SingleByteTuple)
{
    const uint32_t PAGE_SIZE = 8192;
    createDatabase(PAGE_SIZE);

    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(PAGE_SIZE);

    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK);

    std::vector<uint8_t> one_byte = {0x42};
    uint16_t item_id;

    Status status = page.insertTuple(one_byte.data(),
                                      one_byte.size() + sizeof(TupleHeader),
                                      100, &item_id, &ctx);

    EXPECT_EQ(status, Status::OK)
        << "Should be able to insert 1-byte tuple";

    if (status == Status::OK) {
        const uint8_t* data;
        uint32_t size;
        EXPECT_EQ(page.getTuple(item_id, &data, &size, &ctx), Status::OK);
        EXPECT_GE(size, sizeof(TupleHeader) + 1);
    }
}

/**
 * Test: Page Fragmentation After Deletes
 *
 * Verifies page handles fragmentation when tuples are deleted.
 */
TEST_F(StorageBoundariesTest, PageFragmentation)
{
    const uint32_t PAGE_SIZE = 8192;
    createDatabase(PAGE_SIZE);

    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(PAGE_SIZE);

    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK);

    std::vector<uint16_t> item_ids;
    const size_t TUPLE_SIZE = 300;

    // Insert 10 tuples
    for (int i = 0; i < 10; i++) {
        auto data = generatePattern(TUPLE_SIZE, static_cast<uint8_t>(i));
        uint16_t item_id;

        ASSERT_EQ(page.insertTuple(data.data(), data.size() + sizeof(TupleHeader),
                                    100 + i, &item_id, &ctx), Status::OK);
        item_ids.push_back(item_id);
    }

    // Delete every other tuple
    for (size_t i = 0; i < item_ids.size(); i += 2) {
        EXPECT_EQ(page.deleteTuple(item_ids[i], 200 + i, &ctx), Status::OK);
    }

    // Try to insert new tuple - should reuse freed space
    auto new_data = generatePattern(TUPLE_SIZE, 0xAB);
    uint16_t new_item_id;

    Status status = page.insertTuple(new_data.data(),
                                      new_data.size() + sizeof(TupleHeader),
                                      300, &new_item_id, &ctx);

    EXPECT_EQ(status, Status::OK)
        << "Should be able to reuse space from deleted tuples";
}

/**
 * Test: Item Pointer Array Growth
 *
 * Verifies page handles growing item pointer array correctly.
 */
TEST_F(StorageBoundariesTest, ItemPointerArrayGrowth)
{
    const uint32_t PAGE_SIZE = 8192;
    createDatabase(PAGE_SIZE);

    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(PAGE_SIZE);

    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK);

    // Insert many small tuples to grow item pointer array
    std::vector<uint16_t> item_ids;
    const size_t SMALL_TUPLE_SIZE = 50;

    for (int i = 0; i < 100; i++) {
        auto data = generatePattern(SMALL_TUPLE_SIZE, static_cast<uint8_t>(i));
        uint16_t item_id;

        Status status = page.insertTuple(data.data(),
                                         data.size() + sizeof(TupleHeader),
                                         100 + i, &item_id, &ctx);

        if (status == Status::OK) {
            item_ids.push_back(item_id);
        } else {
            // Either page full or item array full
            break;
        }
    }

    EXPECT_GT(item_ids.size(), 20)
        << "Should handle multiple small tuples and item pointer growth";
}

/**
 * Test: Alignment and Padding
 *
 * Verifies tuples are properly aligned in memory.
 */
TEST_F(StorageBoundariesTest, TupleAlignment)
{
    const uint32_t PAGE_SIZE = 8192;
    createDatabase(PAGE_SIZE);

    ErrorContext ctx;
    std::vector<uint8_t> page_buffer(PAGE_SIZE);

    HeapPage page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(page.initialize(1, &ctx), Status::OK);

    // Insert odd-sized tuples
    std::vector<size_t> sizes = {1, 3, 5, 7, 11, 13, 17, 19, 23};

    for (size_t size : sizes) {
        auto data = generatePattern(size, 0xAA);
        uint16_t item_id;

        Status status = page.insertTuple(data.data(),
                                         data.size() + sizeof(TupleHeader),
                                         100, &item_id, &ctx);

        if (status == Status::OK) {
            // Verify retrieval
            const uint8_t* retrieved;
            uint32_t retrieved_size;
            EXPECT_EQ(page.getTuple(item_id, &retrieved, &retrieved_size, &ctx), Status::OK);

            // Verify pointer is aligned (typically 8-byte alignment)
            uintptr_t addr = reinterpret_cast<uintptr_t>(retrieved);
            EXPECT_EQ(addr % 8, 0) << "Tuple should be 8-byte aligned";
        }
    }
}
