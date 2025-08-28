#include "scratchbird/engine/buffer_pool.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <cstring>

using namespace ScratchBird;

class BufferPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use test-friendly configuration
        config_.num_buffers = 16;              // Small pool for testing
        config_.buffer_size = 4096;            // 4KB pages for testing
        config_.hash_table_size = 32;          // Small hash table
        config_.dirty_page_threshold = 0.8;    // High threshold
        config_.background_write_interval = std::chrono::milliseconds(50);
        config_.enable_statistics = true;
        config_.enable_background_writer = false; // Disable for testing
        config_.stats_report_interval = std::chrono::seconds(1);
        config_.clock_hand_advance_size = 4;
        config_.use_adaptive_replacement = true;
        config_.use_huge_pages = false;
        config_.enable_prefetch = false;
    }

    void TearDown() override {
        if (buffer_pool_) {
            buffer_pool_->shutdown();
            buffer_pool_.reset();
        }
    }

    BufferPoolConfig config_;
    std::unique_ptr<BufferPool> buffer_pool_;
};

// Test 1: Configuration validation
TEST_F(BufferPoolTest, ConfigurationValidation)
{
    // Valid configuration
    EXPECT_TRUE(BufferPool::validate_config(config_).empty());

    // Invalid configurations
    BufferPoolConfig invalid_config = config_;
    
    invalid_config.num_buffers = 0;
    EXPECT_FALSE(BufferPool::validate_config(invalid_config).empty());
    
    invalid_config = config_;
    invalid_config.buffer_size = 0;
    EXPECT_FALSE(BufferPool::validate_config(invalid_config).empty());
    
    invalid_config = config_;
    invalid_config.hash_table_size = 0;
    EXPECT_FALSE(BufferPool::validate_config(invalid_config).empty());
    
    invalid_config = config_;
    invalid_config.dirty_page_threshold = 1.5;
    EXPECT_FALSE(BufferPool::validate_config(invalid_config).empty());
    
    invalid_config = config_;
    invalid_config.clock_hand_advance_size = 0;
    EXPECT_FALSE(BufferPool::validate_config(invalid_config).empty());
}

// Test 2: Buffer pool initialization and shutdown
TEST_F(BufferPoolTest, InitializationShutdown)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    
    // Initialize buffer pool
    std::error_code ec = buffer_pool_->initialize();
    EXPECT_FALSE(ec) << "Buffer pool initialization failed: " << ec.message();
    
    // Check initial state
    auto usage_info = buffer_pool_->get_usage_info();
    EXPECT_EQ(usage_info.total_buffers, config_.num_buffers);
    EXPECT_EQ(usage_info.used_buffers, 0);
    EXPECT_EQ(usage_info.dirty_buffers, 0);
    EXPECT_EQ(usage_info.pinned_buffers, 0);
    EXPECT_DOUBLE_EQ(usage_info.usage_percentage, 0.0);
    
    // Verify configuration
    const auto& retrieved_config = buffer_pool_->get_config();
    EXPECT_EQ(retrieved_config.num_buffers, config_.num_buffers);
    EXPECT_EQ(retrieved_config.buffer_size, config_.buffer_size);
    
    // Shutdown (will be called again in TearDown, but that's OK)
    buffer_pool_->shutdown();
}

// Test 3: Basic buffer operations
TEST_F(BufferPoolTest, BasicBufferOperations)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    ASSERT_FALSE(buffer_pool_->initialize());
    
    // Create test buffer tag
    BufferTag tag(123, MAIN_FORKNUM, 456);
    
    // Get buffer (should be a miss)
    bool found = false;
    int buffer_id = buffer_pool_->get_buffer(tag, found);
    EXPECT_NE(buffer_id, INVALID_BUFFER_ID);
    EXPECT_FALSE(found) << "First access should be a miss";
    
    // Verify buffer data access
    char* buffer_data = buffer_pool_->get_buffer_data(buffer_id);
    ASSERT_NE(buffer_data, nullptr);
    
    // Write test data
    const char test_data[] = "Test buffer data content";
    memcpy(buffer_data, test_data, sizeof(test_data));
    
    // Release buffer (mark as dirty)
    buffer_pool_->release_buffer(buffer_id, true);
    
    // Get same buffer again (should be a hit)
    found = false;
    int buffer_id2 = buffer_pool_->get_buffer(tag, found);
    EXPECT_EQ(buffer_id2, buffer_id) << "Should get same buffer";
    EXPECT_TRUE(found) << "Second access should be a hit";
    
    // Verify data is still there
    char* buffer_data2 = buffer_pool_->get_buffer_data(buffer_id2);
    EXPECT_STREQ(buffer_data2, test_data);
    
    // Release buffer
    buffer_pool_->release_buffer(buffer_id2, false);
    
    // Check statistics
    auto stats = buffer_pool_->get_stats();
    EXPECT_GT(stats.buffer_hits.load(), 0);
    EXPECT_GT(stats.buffer_misses.load(), 0);
    EXPECT_GT(stats.get_hit_ratio(), 0.0);
}

// Test 4: Buffer descriptor functionality
TEST_F(BufferPoolTest, BufferDescriptorOperations)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    ASSERT_FALSE(buffer_pool_->initialize());
    
    BufferTag tag(789, MAIN_FORKNUM, 101112);
    
    bool found = false;
    int buffer_id = buffer_pool_->get_buffer(tag, found);
    ASSERT_NE(buffer_id, INVALID_BUFFER_ID);
    
    // Get buffer descriptor
    BufferDescriptor* descriptor = buffer_pool_->get_buffer_descriptor(buffer_id);
    ASSERT_NE(descriptor, nullptr);
    
    // Test tag operations
    EXPECT_TRUE(descriptor->matches_tag(tag));
    EXPECT_EQ(descriptor->get_tag(), tag);
    
    // Test state operations
    EXPECT_TRUE(descriptor->is_valid());
    EXPECT_FALSE(descriptor->is_dirty());
    EXPECT_TRUE(descriptor->is_pinned()); // Should be pinned from get_buffer()
    
    // Test pin operations
    int initial_pin_count = descriptor->get_pin_count();
    EXPECT_GT(initial_pin_count, 0);
    
    descriptor->pin();
    EXPECT_EQ(descriptor->get_pin_count(), initial_pin_count + 1);
    
    descriptor->unpin();
    EXPECT_EQ(descriptor->get_pin_count(), initial_pin_count);
    
    // Test usage bit operations
    EXPECT_TRUE(descriptor->get_usage_bit()); // Should be set from access
    
    bool prev_usage = descriptor->clear_usage_bit();
    EXPECT_TRUE(prev_usage);
    EXPECT_FALSE(descriptor->get_usage_bit());
    
    descriptor->set_usage_bit(true);
    EXPECT_TRUE(descriptor->get_usage_bit());
    
    // Test access tracking
    uint64_t initial_access_count = descriptor->get_access_count();
    descriptor->update_access_time();
    EXPECT_GT(descriptor->get_access_count(), initial_access_count);
    EXPECT_TRUE(descriptor->get_usage_bit()); // Should be set by update_access_time()
    
    buffer_pool_->release_buffer(buffer_id, false);
}

// Test 5: Buffer frame operations
TEST_F(BufferPoolTest, BufferFrameOperations)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    ASSERT_FALSE(buffer_pool_->initialize());
    
    BufferTag tag(555, MAIN_FORKNUM, 777);
    
    bool found = false;
    int buffer_id = buffer_pool_->get_buffer(tag, found);
    ASSERT_NE(buffer_id, INVALID_BUFFER_ID);
    
    char* buffer_data = buffer_pool_->get_buffer_data(buffer_id);
    ASSERT_NE(buffer_data, nullptr);
    
    // Test data operations
    const char test_pattern[] = "BufferFrame test data with specific pattern";
    memcpy(buffer_data, test_pattern, sizeof(test_pattern));
    
    // Verify data
    EXPECT_STREQ(buffer_data, test_pattern);
    
    // Test with const access
    const char* const_buffer_data = buffer_pool_->get_buffer_data(buffer_id);
    EXPECT_STREQ(const_buffer_data, test_pattern);
    
    buffer_pool_->release_buffer(buffer_id, true);
}

// Test 6: Clock-sweep replacement algorithm
TEST_F(BufferPoolTest, ClockSweepReplacement)
{
    // Use smaller buffer pool to force replacement
    config_.num_buffers = 4;
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    ASSERT_FALSE(buffer_pool_->initialize());
    
    std::vector<BufferTag> tags;
    std::vector<int> buffer_ids;
    
    // Fill all buffers
    for (int i = 0; i < 4; ++i) {
        BufferTag tag(static_cast<RelationOid>(i), MAIN_FORKNUM, static_cast<BlockNumber>(i));
        tags.push_back(tag);
        
        bool found = false;
        int buffer_id = buffer_pool_->get_buffer(tag, found);
        EXPECT_NE(buffer_id, INVALID_BUFFER_ID);
        EXPECT_FALSE(found);
        
        buffer_ids.push_back(buffer_id);
        buffer_pool_->release_buffer(buffer_id, false);
    }
    
    // Now request a new buffer - should cause replacement
    BufferTag new_tag(999, MAIN_FORKNUM, 888);
    bool found = false;
    int new_buffer_id = buffer_pool_->get_buffer(new_tag, found);
    EXPECT_NE(new_buffer_id, INVALID_BUFFER_ID);
    EXPECT_FALSE(found);
    
    buffer_pool_->release_buffer(new_buffer_id, false);
    
    // Check that clock sweep occurred
    auto stats = buffer_pool_->get_stats();
    EXPECT_GT(stats.clock_sweeps.load(), 0);
}

// Test 7: Buffer pool statistics
TEST_F(BufferPoolTest, BufferPoolStatistics)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    ASSERT_FALSE(buffer_pool_->initialize());
    
    // Initial statistics should be zero
    auto stats = buffer_pool_->get_stats();
    EXPECT_EQ(stats.buffer_hits.load(), 0);
    EXPECT_EQ(stats.buffer_misses.load(), 0);
    EXPECT_EQ(stats.buffer_reads.load(), 0);
    
    // Perform some operations
    BufferTag tag1(100, MAIN_FORKNUM, 200);
    bool found = false;
    int buffer_id1 = buffer_pool_->get_buffer(tag1, found); // Miss
    ASSERT_NE(buffer_id1, INVALID_BUFFER_ID);
    buffer_pool_->release_buffer(buffer_id1, true);
    
    // Second access should be hit
    int buffer_id2 = buffer_pool_->get_buffer(tag1, found); // Hit
    EXPECT_EQ(buffer_id2, buffer_id1);
    EXPECT_TRUE(found);
    buffer_pool_->release_buffer(buffer_id2, false);
    
    // Check updated statistics
    stats = buffer_pool_->get_stats();
    EXPECT_EQ(stats.buffer_hits.load(), 1);
    EXPECT_EQ(stats.buffer_misses.load(), 1);
    EXPECT_GT(stats.buffer_reads.load(), 0);
    EXPECT_DOUBLE_EQ(stats.get_hit_ratio(), 0.5);
    
    // Test statistics reset
    buffer_pool_->reset_stats();
    stats = buffer_pool_->get_stats();
    EXPECT_EQ(stats.buffer_hits.load(), 0);
    EXPECT_EQ(stats.buffer_misses.load(), 0);
}

// Test 8: Buffer flushing
TEST_F(BufferPoolTest, BufferFlushing)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    ASSERT_FALSE(buffer_pool_->initialize());
    
    std::vector<int> buffer_ids;
    
    // Create some dirty buffers
    for (int i = 0; i < 3; ++i) {
        BufferTag tag(static_cast<RelationOid>(i + 10), MAIN_FORKNUM, static_cast<BlockNumber>(i + 20));
        
        bool found = false;
        int buffer_id = buffer_pool_->get_buffer(tag, found);
        ASSERT_NE(buffer_id, INVALID_BUFFER_ID);
        
        buffer_pool_->release_buffer(buffer_id, true); // Mark as dirty
        buffer_ids.push_back(buffer_id);
    }
    
    // Test individual buffer flush
    std::error_code ec = buffer_pool_->flush_buffer(buffer_ids[0]);
    EXPECT_FALSE(ec) << "Buffer flush should succeed: " << ec.message();
    
    // Test flush all buffers
    size_t flushed_count = buffer_pool_->flush_all_buffers();
    EXPECT_GE(flushed_count, 2); // At least 2 dirty buffers should be flushed
    
    // Check statistics
    auto stats = buffer_pool_->get_stats();
    EXPECT_GT(stats.buffer_writes.load(), 0);
}

// Test 9: Relation buffer invalidation
TEST_F(BufferPoolTest, RelationBufferInvalidation)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    ASSERT_FALSE(buffer_pool_->initialize());
    
    RelationOid test_relation = 12345;
    std::vector<int> buffer_ids;
    
    // Create buffers for test relation
    for (int i = 0; i < 3; ++i) {
        BufferTag tag(test_relation, MAIN_FORKNUM, static_cast<BlockNumber>(i));
        
        bool found = false;
        int buffer_id = buffer_pool_->get_buffer(tag, found);
        ASSERT_NE(buffer_id, INVALID_BUFFER_ID);
        
        buffer_pool_->release_buffer(buffer_id, false);
        buffer_ids.push_back(buffer_id);
    }
    
    // Create buffer for different relation
    BufferTag other_tag(99999, MAIN_FORKNUM, 1);
    bool found = false;
    int other_buffer_id = buffer_pool_->get_buffer(other_tag, found);
    ASSERT_NE(other_buffer_id, INVALID_BUFFER_ID);
    buffer_pool_->release_buffer(other_buffer_id, false);
    
    // Invalidate test relation buffers
    size_t invalidated_count = buffer_pool_->invalidate_relation_buffers(test_relation);
    EXPECT_EQ(invalidated_count, 3);
    
    // Verify buffers are invalidated (should be misses now)
    for (int i = 0; i < 3; ++i) {
        BufferTag tag(test_relation, MAIN_FORKNUM, static_cast<BlockNumber>(i));
        found = false;
        int buffer_id = buffer_pool_->get_buffer(tag, found);
        EXPECT_FALSE(found) << "Buffer should be invalidated and result in miss";
        buffer_pool_->release_buffer(buffer_id, false);
    }
    
    // Other relation buffer should still be valid
    found = false;
    int other_buffer_id2 = buffer_pool_->get_buffer(other_tag, found);
    EXPECT_TRUE(found) << "Other relation buffer should still be valid";
    buffer_pool_->release_buffer(other_buffer_id2, false);
}

// Test 10: Concurrent buffer access (basic thread safety)
TEST_F(BufferPoolTest, ConcurrentBufferAccess)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    ASSERT_FALSE(buffer_pool_->initialize());
    
    const int num_threads = 4;
    const int operations_per_thread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> total_operations{0};
    
    // Start concurrent threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, operations_per_thread, &total_operations]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);
            
            for (int op = 0; op < operations_per_thread; ++op) {
                RelationOid relation_oid = static_cast<RelationOid>(t * 100 + op);
                BlockNumber block_number = static_cast<BlockNumber>(dis(gen));
                
                BufferTag tag(relation_oid, MAIN_FORKNUM, block_number);
                
                bool found = false;
                int buffer_id = buffer_pool_->get_buffer(tag, found);
                if (buffer_id != INVALID_BUFFER_ID) {
                    // Simulate some work
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    
                    buffer_pool_->release_buffer(buffer_id, op % 2 == 0); // Some dirty
                    total_operations.fetch_add(1);
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify operations completed
    EXPECT_EQ(total_operations.load(), num_threads * operations_per_thread);
    
    // Check that statistics make sense
    auto stats = buffer_pool_->get_stats();
    EXPECT_GT(stats.buffer_hits.load() + stats.buffer_misses.load(), 0);
}

// Test 11: Configuration updates
TEST_F(BufferPoolTest, ConfigurationUpdates)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    ASSERT_FALSE(buffer_pool_->initialize());
    
    // Update runtime configuration
    BufferPoolConfig new_config = config_;
    new_config.dirty_page_threshold = 0.9;
    new_config.background_write_interval = std::chrono::milliseconds(25);
    new_config.enable_prefetch = true;
    
    std::error_code ec = buffer_pool_->update_config(new_config);
    EXPECT_FALSE(ec) << "Configuration update should succeed: " << ec.message();
    
    // Verify configuration was updated
    const auto& updated_config = buffer_pool_->get_config();
    EXPECT_DOUBLE_EQ(updated_config.dirty_page_threshold, 0.9);
    EXPECT_EQ(updated_config.background_write_interval, std::chrono::milliseconds(25));
    EXPECT_TRUE(updated_config.enable_prefetch);
    
    // Try invalid configuration update
    BufferPoolConfig invalid_config = config_;
    invalid_config.dirty_page_threshold = 1.5; // Invalid
    
    ec = buffer_pool_->update_config(invalid_config);
    EXPECT_TRUE(ec) << "Invalid configuration update should fail";
}

// Test 12: Usage information reporting
TEST_F(BufferPoolTest, UsageInformationReporting)
{
    buffer_pool_ = std::make_unique<BufferPool>(config_);
    ASSERT_FALSE(buffer_pool_->initialize());
    
    // Initial usage should be empty
    auto usage_info = buffer_pool_->get_usage_info();
    EXPECT_EQ(usage_info.total_buffers, config_.num_buffers);
    EXPECT_EQ(usage_info.used_buffers, 0);
    EXPECT_EQ(usage_info.dirty_buffers, 0);
    EXPECT_EQ(usage_info.pinned_buffers, 0);
    EXPECT_DOUBLE_EQ(usage_info.usage_percentage, 0.0);
    
    // Use some buffers
    std::vector<int> buffer_ids;
    for (int i = 0; i < 5; ++i) {
        BufferTag tag(static_cast<RelationOid>(i), MAIN_FORKNUM, static_cast<BlockNumber>(i));
        
        bool found = false;
        int buffer_id = buffer_pool_->get_buffer(tag, found);
        ASSERT_NE(buffer_id, INVALID_BUFFER_ID);
        
        buffer_pool_->release_buffer(buffer_id, i % 2 == 0); // Some dirty
        buffer_ids.push_back(buffer_id);
    }
    
    // Check updated usage
    usage_info = buffer_pool_->get_usage_info();
    EXPECT_GT(usage_info.used_buffers, 0);
    EXPECT_GT(usage_info.usage_percentage, 0.0);
    EXPECT_GT(usage_info.hit_ratio, 0.0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}