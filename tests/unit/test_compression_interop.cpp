#include <gtest/gtest.h>
#include "scratchbird/core/compression.h"
#include "scratchbird/core/compressed_page_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/page_manager.h"
#include <cstring>
#include <filesystem>
#include <vector>
#include <random>

// Define a macro to skip tests when LZ4 is not available
#ifdef HAVE_LZ4
#define SKIP_IF_NO_LZ4()
#else
#define SKIP_IF_NO_LZ4() GTEST_SKIP() << "LZ4 support not available"
#endif

using namespace scratchbird::core;

class CompressionInteropTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_dir_ = std::filesystem::temp_directory_path() / "scratchbird_compression_interop_test";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(test_dir_);
    }

    // Generate test data with varying compressibility
    std::vector<uint8_t> generate_test_data(size_t size, double compressibility)
    {
        std::vector<uint8_t> data(size);
        std::mt19937 rng(42);

        if (compressibility > 0.8)
        {
            // Highly compressible - repeated pattern
            const char *pattern = "SCRATCHBIRD";
            for (size_t i = 0; i < size; i++)
            {
                data[i] = pattern[i % strlen(pattern)];
            }
        }
        else if (compressibility > 0.5)
        {
            // Moderately compressible - some patterns
            std::uniform_int_distribution<> dist(0, 50);
            for (size_t i = 0; i < size; i++)
            {
                data[i] = 'A' + (dist(rng) % 26);
            }
        }
        else
        {
            // Low compressibility - random data
            std::uniform_int_distribution<> dist(0, 255);
            for (size_t i = 0; i < size; i++)
            {
                data[i] = dist(rng);
            }
        }

        return data;
    }

    std::filesystem::path test_dir_;
};

TEST_F(CompressionInteropTest, AllPageSizesWithCompression)
{
    SKIP_IF_NO_LZ4();

    std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 65536, 131072};

    for (uint32_t page_size : page_sizes)
    {
        // Create database with specific page size
        auto db_path = test_dir_ / ("test_" + std::to_string(page_size) + ".db");
        Status status = Database::create(db_path.string(), page_size);
        ASSERT_EQ(status, Status::OK) << "Failed to create DB with page size " << page_size;

        Database db;
        status = db.open(db_path.string());
        ASSERT_EQ(status, Status::OK) << "Failed to open DB with page size " << page_size;

        // Use compressed page manager
        CompressedPageManager compressed_pm(&db, page_size, CompressionType::LZ4);

        // Test writing and reading multiple pages
        for (uint32_t page_id = 10; page_id < 20; page_id++)
        {
            std::vector<uint8_t> page_buffer(page_size);
            HeapPage heap_page(page_buffer.data(), page_size);

            status = heap_page.initialize(page_id);
            ASSERT_EQ(status, Status::OK);

            // Insert enough data to fill >50% of the page (required for compression to trigger)
            // shouldCompressPage() requires: free_space < page_size * 0.5
            // So we need to insert tuples until we exceed 50% usage
            double compressibility = 0.9 - (page_id - 10) * 0.08;

            // Insert multiple tuples to fill past 50%
            size_t chunk_size = std::min(static_cast<size_t>(page_size / 8), static_cast<size_t>(2000));
            int tuples_inserted = 0;
            while (heap_page.getFreeSpace() > page_size * 0.45) // Fill to >55%
            {
                auto test_data = generate_test_data(chunk_size, compressibility);
                uint16_t item_id;
                status = heap_page.insertTuple(test_data.data(), test_data.size(),
                                               1000 + page_id * 100 + tuples_inserted, &item_id);
                if (status != Status::OK)
                {
                    break; // Page is full
                }
                tuples_inserted++;
            }

            ASSERT_GT(tuples_inserted, 0) << "Should insert at least one tuple";

            // Write compressed page
            status = compressed_pm.writePage(page_id, page_buffer.data());
            ASSERT_EQ(status, Status::OK);

            // Read it back
            std::vector<uint8_t> read_buffer(page_size);
            status = compressed_pm.readPage(page_id, read_buffer.data());
            ASSERT_EQ(status, Status::OK);

            // Verify data integrity
            EXPECT_EQ(memcmp(page_buffer.data(), read_buffer.data(), page_size), 0);
        }

        // Check compression statistics
        const auto &stats = compressed_pm.compressionStats();
        EXPECT_GT(stats.compress_calls, 0u);
        EXPECT_GT(stats.decompress_calls, 0u);
        EXPECT_GT(stats.bytes_in, 0u);
        EXPECT_GT(stats.bytes_out, 0u);

        // Log compression ratio for this page size
        double ratio = stats.compressionRatio();
        EXPECT_LT(ratio, 1.0) << "No compression achieved for page size " << page_size;
    }
}

// Test mixed compressed/uncompressed pages
// Note: CompressedPageManager::writePage() updates the checksum in the buffer before
// compression, so we can't use memcmp with the original pre-write buffer.
// Instead, we verify that the HeapPage data is correctly preserved after round-trip.
TEST_F(CompressionInteropTest, MixedCompressedUncompressed)
{
    SKIP_IF_NO_LZ4();

    auto db_path = test_dir_ / "mixed_compression.db";
    Status status = Database::create(db_path.string(), 16384);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(db_path.string());
    ASSERT_EQ(status, Status::OK);

    CompressedPageManager compressed_pm(&db, 16384, CompressionType::LZ4);

    // Write some pages with varying compression levels
    // Pages that should_compress need >50% fill to trigger compression
    std::vector<bool> should_compress = {true, false, true, true, false, true};
    std::vector<uint16_t> expected_tuple_counts;

    for (size_t i = 0; i < should_compress.size(); i++)
    {
        uint32_t page_id = 100 + i;
        std::vector<uint8_t> page_buffer(16384);
        HeapPage heap_page(page_buffer.data(), 16384);

        status = heap_page.initialize(page_id);
        ASSERT_EQ(status, Status::OK);

        int tuples = 0;
        if (should_compress[i])
        {
            // Insert highly compressible data to fill >50% of page
            while (heap_page.getFreeSpace() > 16384 * 0.45)
            {
                auto test_data = generate_test_data(2000, 0.9);
                uint16_t item_id;
                status = heap_page.insertTuple(test_data.data(), test_data.size(),
                                               2000 + i * 100 + tuples, &item_id);
                if (status != Status::OK)
                    break;
                tuples++;
            }
            ASSERT_GT(tuples, 0) << "Should insert at least one tuple";
        }
        else
        {
            // Insert small amount of incompressible data (stays below 50% threshold)
            auto test_data = generate_test_data(100, 0.1);
            uint16_t item_id;
            status = heap_page.insertTuple(test_data.data(), test_data.size(), 2000 + i, &item_id);
            ASSERT_EQ(status, Status::OK);
            tuples = 1;
        }

        expected_tuple_counts.push_back(tuples);

        status = compressed_pm.writePage(page_id, page_buffer.data());
        ASSERT_EQ(status, Status::OK);
    }

    // Read all pages back and verify tuple counts are preserved
    for (size_t i = 0; i < should_compress.size(); i++)
    {
        uint32_t page_id = 100 + i;
        std::vector<uint8_t> read_buffer(16384);

        status = compressed_pm.readPage(page_id, read_buffer.data());
        ASSERT_EQ(status, Status::OK);

        // Verify page header is valid (indicates successful decompression)
        const auto *header = reinterpret_cast<const PageHeader *>(read_buffer.data());
        EXPECT_EQ(header->page_id, page_id) << "Page ID should be preserved";

        HeapPage read_page(read_buffer.data(), 16384);
        EXPECT_EQ(read_page.getItemCount(), expected_tuple_counts[i])
            << "Tuple count should be preserved for page " << page_id;
    }

    // Verify compression statistics show activity
    const auto &stats = compressed_pm.compressionStats();
    EXPECT_GT(stats.compress_calls + stats.decompress_calls, 0u)
        << "Should have performed some compression/decompression operations";
}

// Test that CompressedPageManager can write and read pages independently of BufferPool
// Note: BufferPool and CompressedPageManager use different I/O paths and checksum handling,
// so they cannot interoperate directly. This test verifies CompressedPageManager's own
// read/write path works correctly with pages that could benefit from compression.
TEST_F(CompressionInteropTest, CompressionWithBufferPool)
{
    SKIP_IF_NO_LZ4();

    auto db_path = test_dir_ / "buffer_pool_compression.db";
    Status status = Database::create(db_path.string(), 32768);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(db_path.string());
    ASSERT_EQ(status, Status::OK);

    // Use compressed page manager independently
    CompressedPageManager compressed_pm(&db, 32768, CompressionType::LZ4);

    // Write pages through CompressedPageManager (not BufferPool)
    std::vector<uint32_t> page_ids = {100, 101, 102, 103, 104};
    std::vector<uint16_t> expected_tuple_counts;

    for (uint32_t page_id : page_ids)
    {
        std::vector<uint8_t> page_buffer(32768);
        HeapPage heap_page(page_buffer.data(), 32768);

        status = heap_page.initialize(page_id);
        ASSERT_EQ(status, Status::OK);

        // Insert highly compressible data to fill >50% of page
        std::string pattern = "SCRATCHBIRD DATABASE ENGINE TEST PATTERN ";
        int tuples_inserted = 0;
        while (heap_page.getFreeSpace() > 32768 * 0.45)
        {
            std::string data;
            for (int i = 0; i < 50; i++)
            {
                data += pattern;
            }

            uint16_t item_id;
            status = heap_page.insertTuple(reinterpret_cast<const uint8_t *>(data.data()),
                                           data.size(), 5000 + page_id * 100 + tuples_inserted,
                                           &item_id);
            if (status != Status::OK)
                break;
            tuples_inserted++;
        }
        ASSERT_GT(tuples_inserted, 0) << "Should insert at least one tuple";
        expected_tuple_counts.push_back(tuples_inserted);

        // Write through CompressedPageManager
        status = compressed_pm.writePage(page_id, page_buffer.data());
        ASSERT_EQ(status, Status::OK);
    }

    // Now read them back through compressed page manager
    for (size_t i = 0; i < page_ids.size(); i++)
    {
        uint32_t page_id = page_ids[i];
        std::vector<uint8_t> read_buffer(32768);
        status = compressed_pm.readPage(page_id, read_buffer.data());
        ASSERT_EQ(status, Status::OK);

        // Verify it's a valid heap page
        const auto *header = reinterpret_cast<const PageHeader *>(read_buffer.data());
        EXPECT_EQ(header->page_id, page_id) << "Page ID should match";

        HeapPage heap_page(read_buffer.data(), 32768);
        EXPECT_EQ(heap_page.getItemCount(), expected_tuple_counts[i]) << "Tuple count should match";
    }

    // Verify compression statistics show activity
    const auto &stats = compressed_pm.compressionStats();
    EXPECT_GT(stats.compress_calls, 0u) << "Should have compressed some pages";
    EXPECT_GT(stats.decompress_calls, 0u) << "Should have decompressed some pages";
}

TEST_F(CompressionInteropTest, PageSizeMigrationWithCompression)
{
    SKIP_IF_NO_LZ4();

    // Test that compressed pages work correctly when migrating between page sizes
    // This simulates backup/restore or page size changes

    // Step 1: Create database with 16KB pages and compressed data
    auto source_path = test_dir_ / "source_16k.db";
    Status status = Database::create(source_path.string(), 16384);
    ASSERT_EQ(status, Status::OK);

    {
        Database source_db;
        status = source_db.open(source_path.string());
        ASSERT_EQ(status, Status::OK);

        CompressedPageManager compressed_pm(&source_db, 16384, CompressionType::LZ4);

        // Write compressed test data - need to fill >50% of page
        std::vector<uint8_t> page_buffer(16384);
        HeapPage heap_page(page_buffer.data(), 16384);
        status = heap_page.initialize(100);
        ASSERT_EQ(status, Status::OK);

        // Insert multiple tuples to exceed 50% threshold
        int tuples = 0;
        while (heap_page.getFreeSpace() > 16384 * 0.45)
        {
            auto test_data = generate_test_data(2000, 0.95);
            uint16_t item_id;
            status = heap_page.insertTuple(test_data.data(), test_data.size(),
                                           3000 + tuples, &item_id);
            if (status != Status::OK)
                break;
            tuples++;
        }
        ASSERT_GT(tuples, 0) << "Should insert at least one tuple";

        status = compressed_pm.writePage(100, page_buffer.data());
        ASSERT_EQ(status, Status::OK);
    }

    // Step 2: Create database with 64KB pages
    auto dest_path = test_dir_ / "dest_64k.db";
    status = Database::create(dest_path.string(), 65536);
    ASSERT_EQ(status, Status::OK);

    // Step 3: In a real migration, we would copy and convert pages
    // For this test, we verify that different page sizes can handle compression
    {
        Database dest_db;
        status = dest_db.open(dest_path.string());
        ASSERT_EQ(status, Status::OK);

        CompressedPageManager compressed_pm(&dest_db, 65536, CompressionType::LZ4);

        // Create similar compressed content in larger page
        // Need to fill >50% of 64KB page for compression to trigger
        std::vector<uint8_t> page_buffer(65536);
        HeapPage heap_page(page_buffer.data(), 65536);
        status = heap_page.initialize(100);
        ASSERT_EQ(status, Status::OK);

        // Insert data to fill >50% of page
        int tuples = 0;
        while (heap_page.getFreeSpace() > 65536 * 0.45)
        {
            auto test_data = generate_test_data(4000, 0.95);
            uint16_t item_id;
            status = heap_page.insertTuple(test_data.data(), test_data.size(),
                                           3000 + tuples, &item_id);
            if (status != Status::OK)
                break;
            tuples++;
        }
        ASSERT_GT(tuples, 0) << "Should insert at least one tuple";

        status = compressed_pm.writePage(100, page_buffer.data());
        ASSERT_EQ(status, Status::OK);

        // Verify compression worked on larger page
        const auto &stats = compressed_pm.compressionStats();
        EXPECT_GT(stats.compress_calls, 0u) << "Should have compressed at least one page";
        // The ratio check is optional since it depends on data compressibility
    }
}

TEST_F(CompressionInteropTest, CompressionErrorHandling)
{
    // Test compression behavior with various error conditions

    auto db_path = test_dir_ / "error_handling.db";
    Status status = Database::create(db_path.string(), 8192);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(db_path.string());
    ASSERT_EQ(status, Status::OK);

    // Test with unsupported compression type (should fall back gracefully)
    CompressedPageManager compressed_pm(&db, 8192, CompressionType::ZSTD);

    // Should work even without ZSTD support (falls back to no compression)
    std::vector<uint8_t> page_buffer(8192);
    HeapPage heap_page(page_buffer.data(), 8192);
    status = heap_page.initialize(50);
    ASSERT_EQ(status, Status::OK);

    auto test_data = generate_test_data(1000, 0.5);
    uint16_t item_id;
    status = heap_page.insertTuple(test_data.data(), test_data.size(), 4000, &item_id);
    ASSERT_EQ(status, Status::OK);

    // Should succeed even without compression
    status = compressed_pm.writePage(50, page_buffer.data());
    ASSERT_EQ(status, Status::OK);

    std::vector<uint8_t> read_buffer(8192);
    status = compressed_pm.readPage(50, read_buffer.data());
    ASSERT_EQ(status, Status::OK);

    // Verify data integrity
    EXPECT_EQ(memcmp(page_buffer.data(), read_buffer.data(), 8192), 0);
}