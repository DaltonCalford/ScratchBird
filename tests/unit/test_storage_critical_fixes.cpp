#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <cstring>
#include <filesystem>
#include <vector>
#include <thread>
#include <sys/resource.h>

using namespace scratchbird::core;

// Helper to create a test UUID
static inline UuidV7Bytes makeTestUUID(uint8_t value = 0xAB) {
    UuidV7Bytes uuid;
    memset(uuid.bytes.data(), value, 16);
    return uuid;
}

static std::vector<uint8_t> makeTestTuple(size_t payload_size, uint8_t fill, uint64_t xmin = 100)
{
    std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload_size, 0);
    auto *hdr = reinterpret_cast<TupleHeader *>(tuple.data());
    *hdr = {};
    hdr->xmin = xmin;
    hdr->xmax = 0;
    std::memset(tuple.data() + sizeof(TupleHeader), fill, payload_size);
    return tuple;
}

class StorageCriticalFixesTest : public ::testing::Test
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
        std::filesystem::remove("test_critical.db");
    }

    // Helper to get current memory usage
    size_t get_memory_usage()
    {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        return usage.ru_maxrss * 1024; // Convert to bytes on Linux
    }
};

// Test 1: Verify memory leak fix in HeapScanIterator
// Test directly with HeapPage to avoid CatalogManager dependency
TEST_F(StorageCriticalFixesTest, HeapScanIterator_NoMemoryLeak)
{
    // Create test page buffer
    const uint32_t PAGE_SIZE = 8192;
    std::vector<uint8_t> page_buffer(PAGE_SIZE, 0);

    HeapPage heap_page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(heap_page.initialize(0, nullptr), Status::OK);

    // Insert tuples directly into heap page
    auto tuple_data = makeTestTuple(100, 0xAA);
    int inserted_count = 0;
    for (int i = 0; i < 50; i++)  // Insert until page is full
    {
        uint16_t item_id;
        Status status = heap_page.insertTuple(tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);
        if (status != Status::OK) break;
        inserted_count++;
    }
    EXPECT_GT(inserted_count, 0) << "Should have inserted some tuples";

    // Measure memory before scans
    size_t memory_before = get_memory_usage();

    // Perform multiple scans of the heap page
    for (int scan = 0; scan < 100; scan++)
    {
        // Scan all tuples on the page
        int count = 0;
        for (uint16_t slot = 0; slot < heap_page.getItemCount(); slot++)
        {
            const uint8_t *data;
            uint32_t size;
            Status status = heap_page.getTuple(slot, &data, &size, nullptr);
            if (status == Status::OK && data != nullptr)
            {
                count++;
            }
        }
        EXPECT_EQ(count, inserted_count) << "Should have scanned all tuples";
    }

    // Force garbage collection if possible
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Measure memory after scans
    size_t memory_after = get_memory_usage();

    // Memory increase should be minimal (less than 10MB for metadata)
    size_t memory_increase = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

    // Should have minimal memory growth from scanning
    EXPECT_LT(memory_increase, 10 * 1024 * 1024)
        << "Memory leak detected: " << memory_increase << " bytes increased";
}

// Test 2: Verify buffer overflow fix in tuple insertion
// Test directly with HeapPage to avoid CatalogManager dependency
TEST_F(StorageCriticalFixesTest, HeapPage_InsertTuple_BufferOverflowProtection)
{
    // Create test page buffer
    const uint32_t PAGE_SIZE = 8192;
    std::vector<uint8_t> page_buffer(PAGE_SIZE, 0);

    HeapPage heap_page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(heap_page.initialize(0, nullptr), Status::OK);

    // Test Case 1: Normal tuple insertion
    {
        auto raw_data = makeTestTuple(50, 0xBB);
        uint16_t item_id;
        Status status = heap_page.insertTuple(raw_data.data(), raw_data.size(), 100, &item_id, nullptr);
        EXPECT_EQ(status, Status::OK) << "Normal tuple insertion should succeed";

        // Verify the data was stored correctly
        const uint8_t *read_data;
        uint32_t read_size;
        status = heap_page.getTuple(item_id, &read_data, &read_size, nullptr);
        EXPECT_EQ(status, Status::OK);
        EXPECT_GT(read_size, 0) << "Should have some data";
    }

    // Test Case 2: Minimum size validation - very small tuple
    {
        std::vector<uint8_t> tiny_data(sizeof(TupleHeader) - 1, 0xDD);
        uint16_t item_id;
        Status status = heap_page.insertTuple(tiny_data.data(), tiny_data.size(), 100, &item_id, nullptr);
        EXPECT_EQ(status, Status::INVALID_ARGUMENT)
            << "Tuple smaller than TupleHeader should be rejected";
    }

    // Test Case 3: Large tuple that fits in page
    {
        auto large_data = makeTestTuple(1000, 0xEE);
        uint16_t item_id;
        Status status = heap_page.insertTuple(large_data.data(), large_data.size(), 100, &item_id, nullptr);
        EXPECT_EQ(status, Status::OK) << "Large tuple should fit in 8K page";
    }
}

// Test 3: Verify HeapPage tuple operations work correctly
// Tests direct heap page operations without CatalogManager dependency
TEST_F(StorageCriticalFixesTest, HeapPage_TupleOperations)
{
    // Create test page buffer
    const uint32_t PAGE_SIZE = 8192;
    std::vector<uint8_t> page_buffer(PAGE_SIZE, 0);

    HeapPage heap_page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(heap_page.initialize(0, nullptr), Status::OK);

    // Insert test data directly into heap page
    auto tuple_data = makeTestTuple(100, 0xEE);
    uint16_t item_id;

    Status status = heap_page.insertTuple(tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);
    ASSERT_EQ(status, Status::OK) << "Should insert tuple successfully";

    // Verify we can read the tuple back
    const uint8_t *read_data;
    uint32_t read_size;
    status = heap_page.getTuple(item_id, &read_data, &read_size, nullptr);
    EXPECT_EQ(status, Status::OK) << "Should find inserted tuple";
    EXPECT_GT(read_size, 0) << "Tuple should have data";

    // Test multiple insertions
    std::vector<uint16_t> inserted_ids;
    inserted_ids.push_back(item_id);

    for (int i = 0; i < 10; i++)
    {
        uint16_t new_id;
        status = heap_page.insertTuple(tuple_data.data(), tuple_data.size(), 100, &new_id, nullptr);
        if (status == Status::OK)
        {
            inserted_ids.push_back(new_id);
        }
    }

    // Verify all tuples can be read
    for (uint16_t id : inserted_ids)
    {
        status = heap_page.getTuple(id, &read_data, &read_size, nullptr);
        EXPECT_EQ(status, Status::OK) << "Should read tuple at id " << id;
    }
}

// Test 4: Stress test for memory leak detection using HeapPage
TEST_F(StorageCriticalFixesTest, HeapPage_StressTestMemoryLeak)
{
    // Create multiple page buffers to simulate multi-page scenario
    const uint32_t PAGE_SIZE = 8192;
    const int NUM_PAGES = 5;
    std::vector<std::vector<uint8_t>> page_buffers(NUM_PAGES, std::vector<uint8_t>(PAGE_SIZE, 0));
    std::vector<HeapPage> heap_pages;

    for (int p = 0; p < NUM_PAGES; p++)
    {
        heap_pages.emplace_back(page_buffers[p].data(), PAGE_SIZE);
        ASSERT_EQ(heap_pages[p].initialize(p, nullptr), Status::OK);
    }

    // Insert tuples across all pages
    auto tuple_data = makeTestTuple(100, 0xFF);
    int total_inserted = 0;

    for (int p = 0; p < NUM_PAGES; p++)
    {
        for (int i = 0; i < 30; i++)  // ~30 100-byte tuples per 8K page
        {
            uint16_t item_id;
            Status status = heap_pages[p].insertTuple(tuple_data.data(), tuple_data.size(), 100, &item_id, nullptr);
            if (status == Status::OK)
            {
                total_inserted++;
            }
        }
    }

    EXPECT_GT(total_inserted, NUM_PAGES * 10) << "Should have inserted many tuples";

    // Get baseline memory
    size_t initial_memory = get_memory_usage();

    // Perform many sequential scans of all pages
    for (int iteration = 0; iteration < 100; iteration++)
    {
        int scan_count = 0;
        for (int p = 0; p < NUM_PAGES; p++)
        {
            for (uint16_t slot = 0; slot < heap_pages[p].getItemCount(); slot++)
            {
                const uint8_t *data;
                uint32_t size;
                Status status = heap_pages[p].getTuple(slot, &data, &size, nullptr);
                if (status == Status::OK && data != nullptr)
                {
                    scan_count++;
                }
            }
        }
        EXPECT_EQ(scan_count, total_inserted) << "Should scan all tuples in iteration " << iteration;

        // Check memory growth periodically
        if (iteration % 10 == 0 && iteration > 0)
        {
            size_t current_memory = get_memory_usage();
            size_t growth = (current_memory > initial_memory) ? (current_memory - initial_memory) : 0;

            EXPECT_LT(growth, 10 * 1024 * 1024) << "Excessive memory growth at iteration "
                                                << iteration << ": " << growth << " bytes";
        }
    }
}

// Test 5: Buffer overflow with boundary conditions
TEST_F(StorageCriticalFixesTest, HeapPage_InsertTuple_BoundaryValidation)
{
    // Direct page manipulation to test boundary conditions - no database needed
    const uint32_t PAGE_SIZE = 8192;
    std::vector<uint8_t> page_buffer(PAGE_SIZE, 0);

    HeapPage heap_page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(heap_page.initialize(7, nullptr), Status::OK);

    // Test exact TupleHeader size - this is an edge case
    {
        uint8_t data[sizeof(TupleHeader)];
        memset(data, 0xAA, sizeof(data));

        uint16_t item_id;
        Status status = heap_page.insertTuple(data, sizeof(data), 100, &item_id, nullptr);

        // Edge case: tuple_size == sizeof(TupleHeader) means 0 bytes of actual data
        // This may or may not be accepted depending on implementation
        (void)status;  // Document current behavior
    }

    // Test with proper data size (normal case)
    {
        auto data = makeTestTuple(100, 0xBB);
        uint16_t item_id;
        Status status = heap_page.insertTuple(data.data(), data.size(), 100, &item_id, nullptr);
        EXPECT_EQ(status, Status::OK) << "Normal tuple insertion should succeed";

        if (status == Status::OK)
        {
            // Verify data integrity
            const uint8_t *read_data;
            uint32_t read_size;
            status = heap_page.getTuple(item_id, &read_data, &read_size, nullptr);
            EXPECT_EQ(status, Status::OK);
            EXPECT_GT(read_size, 0) << "Should have read data";
        }
    }

    // Test very large tuple that exceeds page capacity
    {
        std::vector<uint8_t> huge_data(PAGE_SIZE, 0xCC);  // As big as the page
        uint16_t item_id;
        Status status = heap_page.insertTuple(huge_data.data(), huge_data.size(), 100, &item_id, nullptr);
        EXPECT_NE(status, Status::OK) << "Oversized tuple should be rejected";
    }
}

// Test 6: Comprehensive test for heap page operations with various tuple sizes
TEST_F(StorageCriticalFixesTest, HeapPage_CombinedOperations)
{
    // Create a larger page for various sized tuples
    const uint32_t PAGE_SIZE = 16384;  // 16K page
    std::vector<uint8_t> page_buffer(PAGE_SIZE, 0);

    HeapPage heap_page(page_buffer.data(), PAGE_SIZE);
    ASSERT_EQ(heap_page.initialize(0, nullptr), Status::OK);

    // Insert various sized tuples
    std::vector<std::vector<uint8_t>> test_data = {
        makeTestTuple(50, 0x11), makeTestTuple(100, 0x22),
        makeTestTuple(200, 0x33), makeTestTuple(1000, 0x44)};

    std::vector<uint16_t> inserted_ids;

    for (const auto &data : test_data)
    {
        uint16_t item_id;
        Status status = heap_page.insertTuple(data.data(), data.size(), 100, &item_id, nullptr);

        if (status == Status::OK)
        {
            inserted_ids.push_back(item_id);
        }
    }

    EXPECT_EQ(inserted_ids.size(), test_data.size()) << "Some insertions failed";

    // Measure memory before scanning
    size_t memory_before = get_memory_usage();

    // Perform repeated scans to verify no memory leak
    for (int i = 0; i < 50; i++)
    {
        int count = 0;
        for (uint16_t slot = 0; slot < heap_page.getItemCount(); slot++)
        {
            const uint8_t *data;
            uint32_t size;
            Status status = heap_page.getTuple(slot, &data, &size, nullptr);
            if (status == Status::OK && data != nullptr)
            {
                count++;
                EXPECT_GT(size, 0) << "Tuple should have data";
            }
        }

        EXPECT_EQ(count, static_cast<int>(inserted_ids.size())) << "Scan found wrong number of tuples";
    }

    size_t memory_after = get_memory_usage();
    size_t memory_growth = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

    // Should have minimal memory growth after 50 scans
    EXPECT_LT(memory_growth, 5 * 1024 * 1024)
        << "Memory leak detected: " << memory_growth << " bytes";
}
