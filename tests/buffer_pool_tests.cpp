#include "scratchbird/engine/buffer_pool.h"

#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

using namespace scratchbird::engine;

class BufferPoolTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Use test-friendly configuration
        config_.num_buffers = 16;           // Small pool for testing
        config_.buffer_size = 4096;         // 4KB pages for testing
        config_.dirty_page_threshold = 0.8; // High threshold
        config_.background_write_interval = std::chrono::milliseconds(50);
        config_.enable_statistics = true;
        config_.enable_background_writer = false; // Disable for testing
        config_.stats_report_interval = std::chrono::seconds(1);
        config_.use_huge_pages = false;
        config_.enable_prefetch = false;
    }

    void TearDown() override
    {
        if (buffer_pool_) {
            buffer_pool_->shutdown();
            buffer_pool_.reset();
        }
    }

    BufferPoolConfig config_;
    std::unique_ptr<BufferPool> buffer_pool_;
};

// Test 1: Buffer pool initialization and basic operations
TEST_F(BufferPoolTest, InitializationAndBasicOperations)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);

    // Initialize buffer pool
    std::error_code ec = buffer_pool_->initialize();
    EXPECT_FALSE(ec) << "Buffer pool initialization failed";

    // Check initial state
    EXPECT_EQ(buffer_pool_->capacity(), config_.num_buffers);
    EXPECT_EQ(buffer_pool_->page_size(), config_.buffer_size);

    // Test buffer allocation
    BufferTag tag1{1, MAIN_FORKNUM, 100};
    auto handle1 = buffer_pool_->get(tag1);
    EXPECT_TRUE(handle1.valid());

    // Test that we can get the same buffer again
    auto handle2 = buffer_pool_->get(tag1);
    EXPECT_TRUE(handle2.valid());
    EXPECT_EQ(handle1.index(), handle2.index());

    // Test different buffer
    BufferTag tag2{2, MAIN_FORKNUM, 200};
    auto handle3 = buffer_pool_->get(tag2);
    EXPECT_TRUE(handle3.valid());
    EXPECT_NE(handle1.index(), handle3.index());

    // Test legacy API compatibility
    bool found;
    int buffer_id = buffer_pool_->get_buffer(tag1, found);
    EXPECT_TRUE(found);
    EXPECT_GE(buffer_id, 0);
}

// Test 2: BufferHandle RAII behavior
TEST_F(BufferPoolTest, BufferHandleRAII)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    buffer_pool_->initialize();

    BufferTag tag{1, MAIN_FORKNUM, 100};

    // Test move semantics
    {
        auto handle1 = buffer_pool_->get(tag);
        EXPECT_TRUE(handle1.valid());
        int index = handle1.index();

        // Move constructor
        auto handle2 = std::move(handle1);
        EXPECT_FALSE(handle1.valid());
        EXPECT_TRUE(handle2.valid());
        EXPECT_EQ(handle2.index(), index);

        // Move assignment
        BufferHandle handle3;
        handle3 = std::move(handle2);
        EXPECT_FALSE(handle2.valid());
        EXPECT_TRUE(handle3.valid());
        EXPECT_EQ(handle3.index(), index);
    } // handle3 destructor should release the buffer

    // Buffer should still be cached but not pinned
    auto handle4 = buffer_pool_->get(tag);
    EXPECT_TRUE(handle4.valid());
}

// Test 3: Buffer frame access and operations
TEST_F(BufferPoolTest, BufferFrameOperations)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    buffer_pool_->initialize();

    BufferTag tag{1, MAIN_FORKNUM, 100};
    auto handle = buffer_pool_->get(tag);
    EXPECT_TRUE(handle.valid());

    // Test frame access
    BufferFrame* frame = handle.frame();
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->tag, tag);
    EXPECT_EQ(frame->data.size(), config_.buffer_size);

    // Test data operations
    const char test_data[] = "Hello, Buffer Pool!";
    size_t test_len = strlen(test_data);

    std::memcpy(frame->data.data(), test_data, test_len);

    // Mark buffer as dirty
    handle.mark_dirty();
    EXPECT_TRUE(frame->dirty.load());

    // Verify data integrity
    EXPECT_EQ(std::memcmp(frame->data.data(), test_data, test_len), 0);
}

// Test 4: Clock-sweep replacement algorithm
TEST_F(BufferPoolTest, ClockSweepReplacement)
{
    // Use small buffer pool to force replacement
    BufferPoolConfig small_config = config_;
    small_config.num_buffers = 4;
    buffer_pool_ = std::make_unique<BufferPool>(small_config);
    buffer_pool_->initialize();

    std::vector<BufferHandle> handles;

    // Fill buffer pool beyond capacity
    for (int i = 0; i < 8; ++i) {
        BufferTag tag{static_cast<RelationOid>(i), MAIN_FORKNUM, static_cast<BlockNumber>(i * 100)};
        auto handle = buffer_pool_->get(tag);
        EXPECT_TRUE(handle.valid());

        // Keep first 4 handles to prevent their eviction
        if (i < 4) {
            handles.push_back(std::move(handle));
        }
    }

    // Verify statistics show evictions occurred
    auto stats = buffer_pool_->get_stats();
    EXPECT_GT(stats.evictions(), 0);
    EXPECT_GT(stats.clock_sweeps.load(), 0);
}

// Test 5: Statistics collection
TEST_F(BufferPoolTest, StatisticsCollection)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    buffer_pool_->initialize();

    // Reset statistics
    buffer_pool_->reset_stats();
    auto stats = buffer_pool_->get_stats();
    EXPECT_EQ(stats.hits(), 0);
    EXPECT_EQ(stats.misses(), 0);

    BufferTag tag{1, MAIN_FORKNUM, 100};

    // First access should be a miss
    auto handle1 = buffer_pool_->get(tag);
    stats = buffer_pool_->get_stats();
    EXPECT_EQ(stats.misses(), 1);
    EXPECT_EQ(stats.hits(), 0);

    // Second access should be a hit
    auto handle2 = buffer_pool_->get(tag);
    stats = buffer_pool_->get_stats();
    EXPECT_EQ(stats.misses(), 1);
    EXPECT_EQ(stats.hits(), 1);

    // Calculate hit ratio
    EXPECT_DOUBLE_EQ(stats.get_hit_ratio(), 0.5);
}

// Test 6: Flush operations
TEST_F(BufferPoolTest, FlushOperations)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    buffer_pool_->initialize();

    std::atomic<int> flush_count{0};
    buffer_pool_->set_flush_callback(
        [&flush_count](const BufferFrame& frame) { flush_count.fetch_add(1); });

    // Create some dirty buffers
    std::vector<BufferTag> tags;
    for (int i = 0; i < 4; ++i) {
        BufferTag tag{static_cast<RelationOid>(i + 1), MAIN_FORKNUM,
                      static_cast<BlockNumber>((i + 1) * 100)};
        tags.push_back(tag);

        auto handle = buffer_pool_->get(tag);
        EXPECT_TRUE(handle.valid());
        handle.mark_dirty();
    } // Handles go out of scope, reducing refcount to 0

    // Flush dirty buffers
    size_t flushed = buffer_pool_->flush_dirty_batch(10);
    EXPECT_GT(flushed, 0);
    EXPECT_GT(flush_count.load(), 0);

    auto stats = buffer_pool_->get_stats();
    EXPECT_GT(stats.flushes(), 0);
}

// Test 7: Legacy compatibility
TEST_F(BufferPoolTest, LegacyCompatibility)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    buffer_pool_->initialize();

    // Test legacy BufferTag constructors
    BufferTag tag1{42, 1000}; // file_id, page_no constructor
    EXPECT_EQ(tag1.file_id(), 42);
    EXPECT_EQ(tag1.page_no(), 1000);
    EXPECT_EQ(tag1.relation_oid, 42);
    EXPECT_EQ(tag1.block_number, 1000);

    // Test legacy statistics interface
    auto stats = buffer_pool_->get_stats();
    uint64_t hits = stats.hits();
    uint64_t misses = stats.misses();
    uint64_t evictions = stats.evictions();
    uint64_t flushes = stats.flushes();

    // Values should be accessible without error
    EXPECT_GE(hits, 0);
    EXPECT_GE(misses, 0);
    EXPECT_GE(evictions, 0);
    EXPECT_GE(flushes, 0);
}

// Test 8: Concurrent access (basic)
TEST_F(BufferPoolTest, BasicConcurrentAccess)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    buffer_pool_->initialize();

    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 10;
    std::vector<std::thread> threads;

    std::atomic<int> success_count{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                BufferTag tag{static_cast<RelationOid>(t), MAIN_FORKNUM,
                              static_cast<BlockNumber>(i)};
                auto handle = buffer_pool_->get(tag);
                if (handle.valid()) {
                    success_count.fetch_add(1);
                    // Simulate some work
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * operations_per_thread);
}

// Test 9: BufferTag hash function
TEST_F(BufferPoolTest, BufferTagHashFunction)
{
    BufferTagHash hasher;

    BufferTag tag1{1, MAIN_FORKNUM, 100};
    BufferTag tag2{1, MAIN_FORKNUM, 101};
    BufferTag tag3{2, MAIN_FORKNUM, 100};
    BufferTag tag4{1, FSM_FORKNUM, 100};

    size_t hash1 = hasher(tag1);
    size_t hash2 = hasher(tag2);
    size_t hash3 = hasher(tag3);
    size_t hash4 = hasher(tag4);

    // Different tags should have different hashes (high probability)
    EXPECT_NE(hash1, hash2);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash1, hash4);
    EXPECT_NE(hash2, hash3);

    // Same tag should have same hash
    BufferTag tag1_copy{1, MAIN_FORKNUM, 100};
    size_t hash1_copy = hasher(tag1_copy);
    EXPECT_EQ(hash1, hash1_copy);
}

// Test 10: Configuration access
TEST_F(BufferPoolTest, ConfigurationAccess)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    buffer_pool_->initialize();

    const auto& retrieved_config = buffer_pool_->get_config();
    EXPECT_EQ(retrieved_config.num_buffers, config_.num_buffers);
    EXPECT_EQ(retrieved_config.buffer_size, config_.buffer_size);
    EXPECT_EQ(retrieved_config.enable_background_writer, config_.enable_background_writer);
    EXPECT_EQ(retrieved_config.dirty_page_threshold, config_.dirty_page_threshold);
}
