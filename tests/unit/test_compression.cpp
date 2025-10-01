#include <gtest/gtest.h>
#include "scratchbird/core/compression.h"
#include "scratchbird/core/compressed_page_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/heap_page.h"
#include <cstring>
#include <random>
#include <filesystem>

// Define a macro to skip tests when LZ4 is not available
#ifdef HAVE_LZ4
#define SKIP_IF_NO_LZ4()
#else
#define SKIP_IF_NO_LZ4() GTEST_SKIP() << "LZ4 support not available"
#endif

using namespace scratchbird::core;

class CompressionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_dir_ = std::filesystem::temp_directory_path() / "scratchbird_compression_test";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
};

TEST_F(CompressionTest, LZ4CodecBasic)
{
    SKIP_IF_NO_LZ4();
    auto codec = CompressionFactory::create(CompressionType::LZ4);
    ASSERT_NE(codec, nullptr);
    EXPECT_EQ(codec->type(), CompressionType::LZ4);
    EXPECT_STREQ(codec->name(), "LZ4");

    // Test compression of simple data
    const char *input = "Hello, World! This is a test of the LZ4 compression codec. "
                        "It should compress repeated data well. "
                        "repeated data well. repeated data well. repeated data well.";
    uint32_t input_size = strlen(input);

    // Allocate buffers
    uint32_t max_compressed = codec->maxCompressedSize(input_size);
    std::vector<uint8_t> compressed(max_compressed);
    std::vector<uint8_t> decompressed(input_size);

    // Compress
    uint32_t compressed_size;
    Status status = codec->compress(reinterpret_cast<const uint8_t *>(input), input_size,
                                    compressed.data(), max_compressed, &compressed_size);
    ASSERT_EQ(status, Status::OK);
    EXPECT_LT(compressed_size, input_size); // Should compress

    // Decompress
    uint32_t decompressed_size;
    status = codec->decompress(compressed.data(), compressed_size, decompressed.data(), input_size,
                               &decompressed_size);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(decompressed_size, input_size);

    // Verify data
    EXPECT_EQ(memcmp(input, decompressed.data(), input_size), 0);

    // Check stats
    const auto &stats = codec->stats();
    EXPECT_EQ(stats.compress_calls, 1u);
    EXPECT_EQ(stats.decompress_calls, 1u);
    EXPECT_EQ(stats.bytes_in, input_size);
    EXPECT_EQ(stats.bytes_out, compressed_size);
}

TEST_F(CompressionTest, CompressionLevels)
{
    SKIP_IF_NO_LZ4();
    auto codec = CompressionFactory::create(CompressionType::LZ4);
    ASSERT_NE(codec, nullptr);

    // Generate test data with good compression potential
    std::string input(16384, 'A'); // 16KB of 'A's
    for (int i = 0; i < 1000; i++)
    {
        input[i * 16] = 'B' + (i % 26);
    }

    std::vector<uint8_t> compressed_fastest(codec->maxCompressedSize(input.size()));
    std::vector<uint8_t> compressed_default(codec->maxCompressedSize(input.size()));
    std::vector<uint8_t> compressed_best(codec->maxCompressedSize(input.size()));

    uint32_t size_fastest, size_default, size_best;

    // Test FASTEST level
    codec->resetStats();
    Status status = codec->compress(reinterpret_cast<const uint8_t *>(input.data()), input.size(),
                                    compressed_fastest.data(), compressed_fastest.size(),
                                    &size_fastest, CompressionLevel::FASTEST);
    ASSERT_EQ(status, Status::OK);
    auto time_fastest = codec->stats().avgCompressTimeUs();

    // Test DEFAULT level
    codec->resetStats();
    status = codec->compress(reinterpret_cast<const uint8_t *>(input.data()), input.size(),
                             compressed_default.data(), compressed_default.size(), &size_default,
                             CompressionLevel::DEFAULT);
    ASSERT_EQ(status, Status::OK);
    auto time_default = codec->stats().avgCompressTimeUs();

    // Test BEST level
    codec->resetStats();
    status = codec->compress(reinterpret_cast<const uint8_t *>(input.data()), input.size(),
                             compressed_best.data(), compressed_best.size(), &size_best,
                             CompressionLevel::BEST);
    ASSERT_EQ(status, Status::OK);
    auto time_best = codec->stats().avgCompressTimeUs();

    // Verify: better compression takes more time
    EXPECT_LE(size_best, size_default);
    EXPECT_LE(size_default, size_fastest);
    EXPECT_LE(time_fastest, time_default);
    EXPECT_LE(time_default, time_best);
}

TEST_F(CompressionTest, CompressedPageHeader)
{
    // Test the compressed page header structure
    CompressedPageHeader header;
    header.uncompressed_size = 8192;
    header.compressed_size = 4096;
    header.compression_type = static_cast<uint8_t>(CompressionType::LZ4);
    memset(header.reserved, 0, sizeof(header.reserved));
    header.checksum = 0x12345678;

    // Verify size and alignment
    EXPECT_EQ(sizeof(CompressedPageHeader), 16u);
    EXPECT_EQ(offsetof(CompressedPageHeader, uncompressed_size), 0u);
    EXPECT_EQ(offsetof(CompressedPageHeader, compressed_size), 4u);
    EXPECT_EQ(offsetof(CompressedPageHeader, compression_type), 8u);
    EXPECT_EQ(offsetof(CompressedPageHeader, checksum), 12u);
}

TEST_F(CompressionTest, PageCompressionIntegration)
{
    SKIP_IF_NO_LZ4();
    // Create a test database
    auto db_path = test_dir_ / "compression_test.db";
    Status status = Database::create(db_path.string(), 8192);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(db_path.string());
    ASSERT_EQ(status, Status::OK);

    // Create compressed page manager
    CompressedPageManager compressed_pm(&db, 8192, CompressionType::LZ4);

    // Create a heap page with compressible data
    std::vector<uint8_t> page_buffer(8192);
    HeapPage heap_page(page_buffer.data(), 8192);

    status = heap_page.initialize(100); // Page ID 100
    ASSERT_EQ(status, Status::OK);

    // Insert repetitive data that should compress well
    std::string tuple_data = "This is test data that will be repeated many times. ";
    for (int i = 0; i < 50; i++)
    {
        tuple_data += "Repeated pattern. ";
    }

    uint16_t item_id;
    status = heap_page.insertTuple(reinterpret_cast<const uint8_t *>(tuple_data.data()),
                                    tuple_data.size(),
                                    1000, // xmin
                                    &item_id);
    ASSERT_EQ(status, Status::OK);

    // Write compressed page
    status = compressed_pm.writePage(100, page_buffer.data());
    ASSERT_EQ(status, Status::OK);

    // Read it back
    std::vector<uint8_t> read_buffer(8192);
    status = compressed_pm.readPage(100, read_buffer.data());
    ASSERT_EQ(status, Status::OK);

    // Verify the data is identical
    EXPECT_EQ(memcmp(page_buffer.data(), read_buffer.data(), 8192), 0);

    // Check compression actually happened
    const auto &stats = compressed_pm.compressionStats();
    EXPECT_GT(stats.compress_calls, 0u);
    EXPECT_GT(stats.decompress_calls, 0u);
    EXPECT_LT(stats.compressionRatio(), 0.9); // At least 10% compression
}

TEST_F(CompressionTest, SystemPagesNotCompressed)
{
    SKIP_IF_NO_LZ4();
    // Create a test database
    auto db_path = test_dir_ / "system_pages_test.db";
    Status status = Database::create(db_path.string(), 8192);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(db_path.string());
    ASSERT_EQ(status, Status::OK);

    // Create compressed page manager
    CompressedPageManager compressed_pm(&db, 8192, CompressionType::LZ4);

    // System pages (0, 1, 2) should not be compressed
    std::vector<uint8_t> page_buffer(8192);

    // Write and read page 0 (database header)
    status = compressed_pm.writePage(0, page_buffer.data());
    ASSERT_EQ(status, Status::OK);

    status = compressed_pm.readPage(0, page_buffer.data());
    ASSERT_EQ(status, Status::OK);

    // Check no compression happened for system pages
    const auto &stats = compressed_pm.compressionStats();
    EXPECT_EQ(stats.compress_calls, 0u);
    EXPECT_EQ(stats.decompress_calls, 0u);
}

TEST_F(CompressionTest, CompressionFactory)
{
    // Test factory methods
    EXPECT_TRUE(CompressionFactory::isSupported(CompressionType::NONE));
#ifdef HAVE_LZ4
    EXPECT_TRUE(CompressionFactory::isSupported(CompressionType::LZ4));
#else
    EXPECT_FALSE(CompressionFactory::isSupported(CompressionType::LZ4));
#endif
    EXPECT_FALSE(CompressionFactory::isSupported(CompressionType::ZSTD));
    EXPECT_FALSE(CompressionFactory::isSupported(CompressionType::SNAPPY));

    auto types = CompressionFactory::supportedTypes();
#ifdef HAVE_LZ4
    EXPECT_EQ(types.size(), 2u);
    EXPECT_EQ(types[0], CompressionType::NONE);
    EXPECT_EQ(types[1], CompressionType::LZ4);
#else
    EXPECT_EQ(types.size(), 1u);
    EXPECT_EQ(types[0], CompressionType::NONE);
#endif

    EXPECT_STREQ(CompressionFactory::getName(CompressionType::NONE), "None");
#ifdef HAVE_LZ4
    EXPECT_STREQ(CompressionFactory::getName(CompressionType::LZ4), "LZ4");
    EXPECT_STREQ(CompressionFactory::getName(CompressionType::ZSTD), "Zstandard");
    EXPECT_STREQ(CompressionFactory::getName(CompressionType::SNAPPY), "Snappy");
#else
    EXPECT_STREQ(CompressionFactory::getName(CompressionType::LZ4), "LZ4 (not available)");
    EXPECT_STREQ(CompressionFactory::getName(CompressionType::ZSTD), "Zstandard (not available)");
    EXPECT_STREQ(CompressionFactory::getName(CompressionType::SNAPPY), "Snappy (not available)");
#endif
}

TEST_F(CompressionTest, LargePageCompression)
{
    SKIP_IF_NO_LZ4();
    auto codec = CompressionFactory::create(CompressionType::LZ4);
    ASSERT_NE(codec, nullptr);

    // Test with all supported page sizes
    std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 65536, 131072};

    for (uint32_t page_size : page_sizes)
    {
        // Create page with pattern that compresses well
        std::vector<uint8_t> page(page_size);

        // Fill with repeating pattern
        const char *pattern = "ScratchBird Database Page Data Pattern ";
        size_t pattern_len = strlen(pattern);
        for (size_t i = 0; i < page_size; i++)
        {
            page[i] = pattern[i % pattern_len];
        }

        // Add some variation to make it more realistic
        std::mt19937 rng(42);
        std::uniform_int_distribution<> dist(0, 255);
        for (size_t i = 0; i < page_size / 100; i++)
        {
            page[rng() % page_size] = dist(rng);
        }

        // Compress
        uint32_t max_compressed = codec->maxCompressedSize(page_size);
        std::vector<uint8_t> compressed(max_compressed);
        uint32_t compressed_size;

        Status status = codec->compress(page.data(), page_size, compressed.data(), max_compressed,
                                        &compressed_size);
        ASSERT_EQ(status, Status::OK);

        // Should achieve reasonable compression
        double ratio = static_cast<double>(compressed_size) / page_size;
        EXPECT_LT(ratio, 0.8) << "Page size: " << page_size;

        // Decompress and verify
        std::vector<uint8_t> decompressed(page_size);
        uint32_t decompressed_size;

        status = codec->decompress(compressed.data(), compressed_size, decompressed.data(),
                                   page_size, &decompressed_size);
        ASSERT_EQ(status, Status::OK);
        EXPECT_EQ(decompressed_size, page_size);
        EXPECT_EQ(memcmp(page.data(), decompressed.data(), page_size), 0);
    }
}