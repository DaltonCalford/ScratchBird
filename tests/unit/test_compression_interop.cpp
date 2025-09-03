#include <gtest/gtest.h>
#include "scratchbird/core/compression.h"
#include "scratchbird/core/compressed_page_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/buffer_pool.h"
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

class CompressionInteropTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "scratchbird_compression_interop_test";
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }
    
    // Generate test data with varying compressibility
    std::vector<uint8_t> generate_test_data(size_t size, double compressibility) {
        std::vector<uint8_t> data(size);
        std::mt19937 rng(42);
        
        if (compressibility > 0.8) {
            // Highly compressible - repeated pattern
            const char* pattern = "SCRATCHBIRD";
            for (size_t i = 0; i < size; i++) {
                data[i] = pattern[i % strlen(pattern)];
            }
        } else if (compressibility > 0.5) {
            // Moderately compressible - some patterns
            std::uniform_int_distribution<> dist(0, 50);
            for (size_t i = 0; i < size; i++) {
                data[i] = 'A' + (dist(rng) % 26);
            }
        } else {
            // Low compressibility - random data
            std::uniform_int_distribution<> dist(0, 255);
            for (size_t i = 0; i < size; i++) {
                data[i] = dist(rng);
            }
        }
        
        return data;
    }
    
    std::filesystem::path test_dir_;
};

TEST_F(CompressionInteropTest, AllPageSizesWithCompression) {
    SKIP_IF_NO_LZ4();
    
    std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 65536, 131072};
    
    for (uint32_t page_size : page_sizes) {
        // Create database with specific page size
        auto db_path = test_dir_ / ("test_" + std::to_string(page_size) + ".db");
        Status status = Database::create(db_path.string(), page_size);
        ASSERT_EQ(status, Status::Ok) << "Failed to create DB with page size " << page_size;
        
        Database db;
        status = db.open(db_path.string());
        ASSERT_EQ(status, Status::Ok) << "Failed to open DB with page size " << page_size;
        
        // Use compressed page manager
        CompressedPageManager compressed_pm(&db, page_size, CompressionType::LZ4);
        
        // Test writing and reading multiple pages
        for (uint32_t page_id = 10; page_id < 20; page_id++) {
            std::vector<uint8_t> page_buffer(page_size);
            HeapPage heap_page(page_buffer.data(), page_size);
            
            status = heap_page.initialize(page_id);
            ASSERT_EQ(status, Status::Ok);
            
            // Insert data with varying compressibility
            double compressibility = 0.9 - (page_id - 10) * 0.08;
            auto test_data = generate_test_data(page_size / 4, compressibility);
            
            uint16_t item_id;
            status = heap_page.insert_tuple(
                test_data.data(),
                test_data.size(),
                1000 + page_id,
                &item_id
            );
            ASSERT_EQ(status, Status::Ok);
            
            // Write compressed page
            status = compressed_pm.write_page(page_id, page_buffer.data());
            ASSERT_EQ(status, Status::Ok);
            
            // Read it back
            std::vector<uint8_t> read_buffer(page_size);
            status = compressed_pm.read_page(page_id, read_buffer.data());
            ASSERT_EQ(status, Status::Ok);
            
            // Verify data integrity
            EXPECT_EQ(memcmp(page_buffer.data(), read_buffer.data(), page_size), 0);
        }
        
        // Check compression statistics
        const auto& stats = compressed_pm.compression_stats();
        EXPECT_GT(stats.compress_calls, 0u);
        EXPECT_GT(stats.decompress_calls, 0u);
        EXPECT_GT(stats.bytes_in, 0u);
        EXPECT_GT(stats.bytes_out, 0u);
        
        // Log compression ratio for this page size
        double ratio = stats.compression_ratio();
        EXPECT_LT(ratio, 1.0) << "No compression achieved for page size " << page_size;
    }
}

TEST_F(CompressionInteropTest, MixedCompressedUncompressed) {
    SKIP_IF_NO_LZ4();
    
    auto db_path = test_dir_ / "mixed_compression.db";
    Status status = Database::create(db_path.string(), 16384);
    ASSERT_EQ(status, Status::Ok);
    
    Database db;
    status = db.open(db_path.string());
    ASSERT_EQ(status, Status::Ok);
    
    CompressedPageManager compressed_pm(&db, 16384, CompressionType::LZ4);
    
    // Write some pages compressed and some uncompressed
    std::vector<bool> should_compress = {true, false, true, true, false, true};
    std::vector<std::vector<uint8_t>> original_pages;
    
    for (size_t i = 0; i < should_compress.size(); i++) {
        uint32_t page_id = 100 + i;
        std::vector<uint8_t> page_buffer(16384);
        HeapPage heap_page(page_buffer.data(), 16384);
        
        status = heap_page.initialize(page_id);
        ASSERT_EQ(status, Status::Ok);
        
        if (should_compress[i]) {
            // Insert highly compressible data
            auto test_data = generate_test_data(8000, 0.9);
            uint16_t item_id;
            status = heap_page.insert_tuple(test_data.data(), test_data.size(), 
                                          2000 + i, &item_id);
            ASSERT_EQ(status, Status::Ok);
        } else {
            // Insert incompressible data (random)
            auto test_data = generate_test_data(100, 0.1);
            uint16_t item_id;
            status = heap_page.insert_tuple(test_data.data(), test_data.size(),
                                          2000 + i, &item_id);
            ASSERT_EQ(status, Status::Ok);
        }
        
        original_pages.push_back(page_buffer);
        
        status = compressed_pm.write_page(page_id, page_buffer.data());
        ASSERT_EQ(status, Status::Ok);
    }
    
    // Read all pages back and verify
    for (size_t i = 0; i < should_compress.size(); i++) {
        uint32_t page_id = 100 + i;
        std::vector<uint8_t> read_buffer(16384);
        
        status = compressed_pm.read_page(page_id, read_buffer.data());
        ASSERT_EQ(status, Status::Ok);
        
        // Verify data matches original
        EXPECT_EQ(memcmp(original_pages[i].data(), read_buffer.data(), 16384), 0);
    }
}

TEST_F(CompressionInteropTest, CompressionWithBufferPool) {
    SKIP_IF_NO_LZ4();
    
    auto db_path = test_dir_ / "buffer_pool_compression.db";
    Status status = Database::create(db_path.string(), 32768);
    ASSERT_EQ(status, Status::Ok);
    
    Database db;
    status = db.open(db_path.string());
    ASSERT_EQ(status, Status::Ok);
    
    // Use compressed page manager with buffer pool
    CompressedPageManager compressed_pm(&db, 32768, CompressionType::LZ4);
    BufferPool* bp = db.buffer_pool();
    
    // Allocate some pages
    std::vector<uint32_t> page_ids = {100, 101, 102, 103, 104};
    
    for (uint32_t page_id : page_ids) {
        void* buffer;
        status = bp->pin_page(page_id, &buffer);
        ASSERT_EQ(status, Status::Ok);
        
        // Initialize as heap page with compressible data
        HeapPage heap_page(static_cast<uint8_t*>(buffer), 32768);
        status = heap_page.initialize(page_id);
        ASSERT_EQ(status, Status::Ok);
        
        // Insert highly compressible data
        std::string pattern = "SCRATCHBIRD DATABASE ENGINE TEST PATTERN ";
        std::string data;
        for (int i = 0; i < 100; i++) {
            data += pattern;
        }
        
        uint16_t item_id;
        status = heap_page.insert_tuple(
            reinterpret_cast<const uint8_t*>(data.data()),
            data.size(),
            5000 + page_id,
            &item_id
        );
        ASSERT_EQ(status, Status::Ok);
        
        // Unpin with dirty flag
        status = bp->unpin_page(page_id, true);
        ASSERT_EQ(status, Status::Ok);
    }
    
    // Flush all pages (this should compress them)
    status = bp->flush_all();
    ASSERT_EQ(status, Status::Ok);
    
    // Now read them back through compressed page manager
    for (uint32_t page_id : page_ids) {
        std::vector<uint8_t> read_buffer(32768);
        status = compressed_pm.read_page(page_id, read_buffer.data());
        ASSERT_EQ(status, Status::Ok);
        
        // Verify it's a valid heap page
        HeapPage heap_page(read_buffer.data(), 32768);
        status = heap_page.validate();
        ASSERT_EQ(status, Status::Ok);
        
        // Verify we can read the tuple
        const uint8_t* tuple_data;
        uint32_t tuple_size;
        status = heap_page.get_tuple(0, &tuple_data, &tuple_size);
        ASSERT_EQ(status, Status::Ok);
        EXPECT_GT(tuple_size, 4000u);
    }
    
    // Check compression statistics
    const auto& stats = compressed_pm.compression_stats();
    EXPECT_GT(stats.decompress_calls, 0u);
}

TEST_F(CompressionInteropTest, PageSizeMigrationWithCompression) {
    SKIP_IF_NO_LZ4();
    
    // Test that compressed pages work correctly when migrating between page sizes
    // This simulates backup/restore or page size changes
    
    // Step 1: Create database with 16KB pages and compressed data
    auto source_path = test_dir_ / "source_16k.db";
    Status status = Database::create(source_path.string(), 16384);
    ASSERT_EQ(status, Status::Ok);
    
    {
        Database source_db;
        status = source_db.open(source_path.string());
        ASSERT_EQ(status, Status::Ok);
        
        CompressedPageManager compressed_pm(&source_db, 16384, CompressionType::LZ4);
        
        // Write compressed test data
        std::vector<uint8_t> page_buffer(16384);
        HeapPage heap_page(page_buffer.data(), 16384);
        status = heap_page.initialize(100);
        ASSERT_EQ(status, Status::Ok);
        
        auto test_data = generate_test_data(8000, 0.95);
        uint16_t item_id;
        status = heap_page.insert_tuple(test_data.data(), test_data.size(), 3000, &item_id);
        ASSERT_EQ(status, Status::Ok);
        
        status = compressed_pm.write_page(100, page_buffer.data());
        ASSERT_EQ(status, Status::Ok);
    }
    
    // Step 2: Create database with 64KB pages
    auto dest_path = test_dir_ / "dest_64k.db";
    status = Database::create(dest_path.string(), 65536);
    ASSERT_EQ(status, Status::Ok);
    
    // Step 3: In a real migration, we would copy and convert pages
    // For this test, we verify that different page sizes can handle compression
    {
        Database dest_db;
        status = dest_db.open(dest_path.string());
        ASSERT_EQ(status, Status::Ok);
        
        CompressedPageManager compressed_pm(&dest_db, 65536, CompressionType::LZ4);
        
        // Create similar compressed content in larger page
        std::vector<uint8_t> page_buffer(65536);
        HeapPage heap_page(page_buffer.data(), 65536);
        status = heap_page.initialize(100);
        ASSERT_EQ(status, Status::Ok);
        
        // Insert more data to utilize larger page
        for (int i = 0; i < 4; i++) {
            auto test_data = generate_test_data(8000, 0.95);
            uint16_t item_id;
            status = heap_page.insert_tuple(test_data.data(), test_data.size(), 
                                          3000 + i, &item_id);
            ASSERT_EQ(status, Status::Ok);
        }
        
        status = compressed_pm.write_page(100, page_buffer.data());
        ASSERT_EQ(status, Status::Ok);
        
        // Verify compression worked on larger page
        const auto& stats = compressed_pm.compression_stats();
        EXPECT_LT(stats.compression_ratio(), 0.5);  // Should compress well
    }
}

TEST_F(CompressionInteropTest, CompressionErrorHandling) {
    // Test compression behavior with various error conditions
    
    auto db_path = test_dir_ / "error_handling.db";
    Status status = Database::create(db_path.string(), 8192);
    ASSERT_EQ(status, Status::Ok);
    
    Database db;
    status = db.open(db_path.string());
    ASSERT_EQ(status, Status::Ok);
    
    // Test with unsupported compression type (should fall back gracefully)
    CompressedPageManager compressed_pm(&db, 8192, CompressionType::ZSTD);
    
    // Should work even without ZSTD support (falls back to no compression)
    std::vector<uint8_t> page_buffer(8192);
    HeapPage heap_page(page_buffer.data(), 8192);
    status = heap_page.initialize(50);
    ASSERT_EQ(status, Status::Ok);
    
    auto test_data = generate_test_data(1000, 0.5);
    uint16_t item_id;
    status = heap_page.insert_tuple(test_data.data(), test_data.size(), 4000, &item_id);
    ASSERT_EQ(status, Status::Ok);
    
    // Should succeed even without compression
    status = compressed_pm.write_page(50, page_buffer.data());
    ASSERT_EQ(status, Status::Ok);
    
    std::vector<uint8_t> read_buffer(8192);
    status = compressed_pm.read_page(50, read_buffer.data());
    ASSERT_EQ(status, Status::Ok);
    
    // Verify data integrity
    EXPECT_EQ(memcmp(page_buffer.data(), read_buffer.data(), 8192), 0);
}