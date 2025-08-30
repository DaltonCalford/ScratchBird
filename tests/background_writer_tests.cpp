#include "scratchbird/engine/background_writer.h"
#include "scratchbird/engine/buffer_pool.h"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace scratchbird::engine;

// Mock BufferPool for testing
class MockBufferPool : public BufferPool
{
  public:
    MockBufferPool() : BufferPool(BufferPoolConfig{}) {}

    // Mock methods for testing
    std::uint32_t dirty_buffer_count_ = 0;
    std::uint32_t flush_calls_ = 0;

    std::uint32_t get_dirty_buffer_count() const
    {
        return dirty_buffer_count_;
    }

    void simulate_dirty_buffers(std::uint32_t count)
    {
        dirty_buffer_count_ = count;
    }

    void simulate_flush()
    {
        flush_calls_++;
        dirty_buffer_count_ = 0; // Simulate successful flush
    }
};

class BackgroundWriterTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mock_pool_ = std::make_unique<MockBufferPool>();

        // Configure background writer for fast testing
        config_.write_interval = std::chrono::milliseconds(50);
        config_.max_buffers_per_cycle = 50;
        config_.dirty_buffer_threshold = 5;
        config_.enabled = true;
    }

    void TearDown() override
    {
        if (bg_writer_) {
            bg_writer_->stop();
        }
    }

    std::unique_ptr<MockBufferPool> mock_pool_;
    std::unique_ptr<BackgroundWriter> bg_writer_;
    BackgroundWriterConfig config_;
};

TEST_F(BackgroundWriterTest, ConstructorAndBasicOperations)
{
    // Test constructor with valid buffer pool
    EXPECT_NO_THROW(
        { bg_writer_ = std::make_unique<BackgroundWriter>(mock_pool_.get(), config_); });

    // Test constructor with null buffer pool
    EXPECT_THROW({ BackgroundWriter invalid_writer(nullptr, config_); }, std::invalid_argument);
}

TEST_F(BackgroundWriterTest, StartStopOperations)
{
    bg_writer_ = std::make_unique<BackgroundWriter>(mock_pool_.get(), config_);

    // Initially not running
    EXPECT_FALSE(bg_writer_->is_running());

    // Start the background writer
    bg_writer_->start();
    EXPECT_TRUE(bg_writer_->is_running());

    // Starting again should be safe (no-op)
    bg_writer_->start();
    EXPECT_TRUE(bg_writer_->is_running());

    // Stop the background writer
    bg_writer_->stop();
    EXPECT_FALSE(bg_writer_->is_running());

    // Stopping again should be safe (no-op)
    bg_writer_->stop();
    EXPECT_FALSE(bg_writer_->is_running());
}

TEST_F(BackgroundWriterTest, ConfigurationManagement)
{
    bg_writer_ = std::make_unique<BackgroundWriter>(mock_pool_.get(), config_);

    // Test getting initial configuration
    auto initial_config = bg_writer_->get_config();
    EXPECT_EQ(initial_config.write_interval, config_.write_interval);
    EXPECT_EQ(initial_config.max_buffers_per_cycle, config_.max_buffers_per_cycle);
    EXPECT_EQ(initial_config.dirty_buffer_threshold, config_.dirty_buffer_threshold);
    EXPECT_EQ(initial_config.enabled, config_.enabled);

    // Test updating configuration
    BackgroundWriterConfig new_config = config_;
    new_config.write_interval = std::chrono::milliseconds(200);
    new_config.max_buffers_per_cycle = 25;
    new_config.dirty_buffer_threshold = 15;

    bg_writer_->update_config(new_config);

    auto updated_config = bg_writer_->get_config();
    EXPECT_EQ(updated_config.write_interval, new_config.write_interval);
    EXPECT_EQ(updated_config.max_buffers_per_cycle, new_config.max_buffers_per_cycle);
    EXPECT_EQ(updated_config.dirty_buffer_threshold, new_config.dirty_buffer_threshold);
}

TEST_F(BackgroundWriterTest, DisableEnableViaConfiguration)
{
    bg_writer_ = std::make_unique<BackgroundWriter>(mock_pool_.get(), config_);

    // Start the writer
    bg_writer_->start();
    EXPECT_TRUE(bg_writer_->is_running());

    // Disable via configuration
    BackgroundWriterConfig disabled_config = config_;
    disabled_config.enabled = false;
    bg_writer_->update_config(disabled_config);

    // Should stop automatically
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(bg_writer_->is_running());

    // Enable via configuration
    disabled_config.enabled = true;
    bg_writer_->update_config(disabled_config);

    // Should start automatically
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(bg_writer_->is_running());
}

TEST_F(BackgroundWriterTest, StatisticsCollection)
{
    bg_writer_ = std::make_unique<BackgroundWriter>(mock_pool_.get(), config_);

    // Initial statistics should be zero
    auto initial_stats = bg_writer_->get_stats();
    EXPECT_EQ(initial_stats.write_cycles, 0);
    EXPECT_EQ(initial_stats.buffers_written, 0);
    EXPECT_EQ(initial_stats.bytes_written, 0);
    EXPECT_EQ(initial_stats.batches_processed, 0);

    // Test statistics reset
    bg_writer_->reset_stats();
    auto reset_stats = bg_writer_->get_stats();
    EXPECT_EQ(reset_stats.write_cycles, 0);
    EXPECT_EQ(reset_stats.buffers_written, 0);
    EXPECT_EQ(reset_stats.bytes_written, 0);
    EXPECT_EQ(reset_stats.batches_processed, 0);

    // Test performance metrics
    EXPECT_EQ(bg_writer_->get_avg_write_latency_us(), 0.0);
    EXPECT_EQ(bg_writer_->get_write_throughput_bps(), 0.0);
}

TEST_F(BackgroundWriterTest, WriteOperationsWithDirtyBuffers)
{
    bg_writer_ = std::make_unique<BackgroundWriter>(mock_pool_.get(), config_);

    // Simulate dirty buffers above threshold
    mock_pool_->simulate_dirty_buffers(20);

    // Start background writer
    bg_writer_->start();

    // Wait for at least one write cycle
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check that statistics were updated
    auto stats = bg_writer_->get_stats();
    EXPECT_GT(stats.write_cycles, 0);

    bg_writer_->stop();
}

TEST_F(BackgroundWriterTest, WriteThresholdRespected)
{
    bg_writer_ = std::make_unique<BackgroundWriter>(mock_pool_.get(), config_);

    // Set dirty buffers below threshold
    mock_pool_->simulate_dirty_buffers(2); // Below threshold of 5

    bg_writer_->start();

    // Wait for potential write cycles
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Should have cycles but no actual writes (below threshold)
    auto stats = bg_writer_->get_stats();
    EXPECT_GT(stats.write_cycles, 0);
    EXPECT_EQ(stats.buffers_written, 0); // No buffers should be written below threshold

    bg_writer_->stop();
}

TEST_F(BackgroundWriterTest, ForceWriteCycle)
{
    bg_writer_ = std::make_unique<BackgroundWriter>(mock_pool_.get(), config_);

    // Start with longer interval to test force write
    BackgroundWriterConfig slow_config = config_;
    slow_config.write_interval = std::chrono::seconds(10); // Very long interval
    slow_config.dirty_buffer_threshold = 1; // Lower threshold so cycles actually process
    bg_writer_->update_config(slow_config);

    // Set up some dirty buffers in mock
    mock_pool_->simulate_dirty_buffers(5);

    bg_writer_->start();

    auto initial_stats = bg_writer_->get_stats();

    // Force a write cycle
    bg_writer_->force_write_cycle();

    // Wait briefly for the forced cycle to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto after_stats = bg_writer_->get_stats();
    EXPECT_GT(after_stats.write_cycles, initial_stats.write_cycles);

    bg_writer_->stop();
}

TEST_F(BackgroundWriterTest, IOThrottling)
{
    // Test I/O rate limiting configuration
    config_.max_io_rate = 1024 * 1024; // 1 MB/s limit

    bg_writer_ = std::make_unique<BackgroundWriter>(mock_pool_.get(), config_);

    auto config = bg_writer_->get_config();
    EXPECT_EQ(config.max_io_rate, 1024 * 1024);

    // Test with unlimited I/O (default)
    config_.max_io_rate = 0;
    bg_writer_->update_config(config_);

    config = bg_writer_->get_config();
    EXPECT_EQ(config.max_io_rate, 0);
}

TEST_F(BackgroundWriterTest, ThreadSafety)
{
    bg_writer_ = std::make_unique<BackgroundWriter>(mock_pool_.get(), config_);

    bg_writer_->start();

    // Test concurrent configuration updates
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i]() {
            BackgroundWriterConfig thread_config = config_;
            thread_config.max_buffers_per_cycle = 50 + i * 10;

            for (int j = 0; j < 10; ++j) {
                bg_writer_->update_config(thread_config);
                auto stats = bg_writer_->get_stats();
                // Just accessing stats to test thread safety
                (void)stats.write_cycles;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    bg_writer_->stop();

    // Should complete without crashes or hangs
    SUCCEED();
}

TEST_F(BackgroundWriterTest, ProperShutdown)
{
    bg_writer_ = std::make_unique<BackgroundWriter>(mock_pool_.get(), config_);

    bg_writer_->start();
    EXPECT_TRUE(bg_writer_->is_running());

    // Test that destructor properly shuts down
    bg_writer_.reset();

    // Should complete without hanging
    SUCCEED();
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
