#include "scratchbird/engine/wal.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

class WalTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        test_dir_ = "/tmp/scratchbird_wal_test";
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);

        config_.wal_dir = test_dir_;
        config_.segment_size_mb = 1;   // Small for testing
        config_.buffer_size_kb = 4;    // Small buffer for testing
        config_.fsync_enabled = false; // Faster tests
    }

    void TearDown() override
    {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    scratchbird::engine::WalConfig config_;
};

TEST_F(WalTest, BasicInitialization)
{
    scratchbird::engine::WalManager wal(config_);

    EXPECT_TRUE(wal.initialize());
    EXPECT_GT(wal.get_current_lsn(), 0);

    // Check that WAL directory was created
    EXPECT_TRUE(std::filesystem::exists(test_dir_));

    // Check that at least one WAL segment file was created
    bool found_wal_file = false;
    for (const auto& entry : std::filesystem::directory_iterator(test_dir_)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.compare(0, 4, "wal_") == 0 &&
                filename.compare(filename.length() - 4, 4, ".log") == 0) {
                found_wal_file = true;
                break;
            }
        }
    }
    EXPECT_TRUE(found_wal_file);
}

TEST_F(WalTest, TransactionLogging)
{
    scratchbird::engine::WalManager wal(config_);
    ASSERT_TRUE(wal.initialize());

    std::uint64_t xid = 100;
    std::uint64_t isolation_level = 1; // READ_COMMITTED
    std::uint64_t snapshot_xid = 99;

    // Log transaction begin
    std::uint64_t begin_lsn = wal.log_begin(xid, isolation_level, snapshot_xid);
    EXPECT_GT(begin_lsn, 0);

    // Log some heap operations
    std::uint32_t space_id = 1;
    std::uint32_t page_no = 42;
    std::uint16_t slot_no = 5;

    std::string tuple_data = "test_data_12345";
    std::uint64_t insert_lsn =
        wal.log_heap_insert(xid, begin_lsn, space_id, page_no, slot_no, tuple_data.data(),
                            static_cast<std::uint32_t>(tuple_data.size()));
    EXPECT_GT(insert_lsn, begin_lsn);

    // Log transaction commit
    std::uint64_t commit_lsn = wal.log_commit(xid, insert_lsn);
    EXPECT_GT(commit_lsn, insert_lsn);

    // Force flush
    wal.flush();

    // Verify stats
    auto stats = wal.get_stats();
    EXPECT_EQ(stats.total_records, 3); // begin + insert + commit
    EXPECT_GT(stats.total_bytes_written, 0);
    EXPECT_EQ(stats.current_lsn, commit_lsn + 1); // Next available LSN
}

TEST_F(WalTest, HeapOperations)
{
    scratchbird::engine::WalManager wal(config_);
    ASSERT_TRUE(wal.initialize());

    std::uint64_t xid = 200;
    std::uint32_t space_id = 1;
    std::uint32_t page_no = 100;
    std::uint16_t slot_no = 10;

    std::string old_data = "old_tuple_data";
    std::string new_data = "new_tuple_data_longer";

    // Test INSERT
    std::uint64_t insert_lsn =
        wal.log_heap_insert(xid, 0, space_id, page_no, slot_no, old_data.data(),
                            static_cast<std::uint32_t>(old_data.size()));
    EXPECT_GT(insert_lsn, 0);

    // Test UPDATE
    std::uint64_t update_lsn =
        wal.log_heap_update(xid, insert_lsn, space_id, page_no, slot_no, old_data.data(),
                            static_cast<std::uint32_t>(old_data.size()), new_data.data(),
                            static_cast<std::uint32_t>(new_data.size()));
    EXPECT_GT(update_lsn, insert_lsn);

    // Test DELETE
    std::uint64_t delete_lsn =
        wal.log_heap_delete(xid, update_lsn, space_id, page_no, slot_no, new_data.data(),
                            static_cast<std::uint32_t>(new_data.size()));
    EXPECT_GT(delete_lsn, update_lsn);

    wal.flush();

    auto stats = wal.get_stats();
    EXPECT_EQ(stats.total_records, 3);
}

TEST_F(WalTest, CheckpointLogging)
{
    scratchbird::engine::WalManager wal(config_);
    ASSERT_TRUE(wal.initialize());

    // Create a list of dirty pages
    std::vector<std::pair<std::uint32_t, std::uint32_t>> dirty_pages = {
        {1, 10}, {1, 11}, {1, 12}, {2, 20}, {2, 21}};

    std::uint64_t checkpoint_lsn = wal.log_checkpoint(dirty_pages);
    EXPECT_GT(checkpoint_lsn, 0);

    EXPECT_EQ(wal.get_last_checkpoint_lsn(), checkpoint_lsn);

    wal.flush();

    auto stats = wal.get_stats();
    EXPECT_GE(stats.total_records, 1);
}

TEST_F(WalTest, WalUtilities)
{
    // Test segment filename generation
    std::string filename1 = scratchbird::engine::wal_util::generate_segment_filename(1);
    EXPECT_EQ(filename1, "wal_0000000000000001.log");

    std::string filename_large =
        scratchbird::engine::wal_util::generate_segment_filename(0xABCDEF123456);
    EXPECT_EQ(filename_large, "wal_0000abcdef123456.log");

    // Test segment number parsing
    EXPECT_EQ(scratchbird::engine::wal_util::parse_segment_number("wal_0000000000000001.log"), 1);
    EXPECT_EQ(scratchbird::engine::wal_util::parse_segment_number("wal_0000abcdef123456.log"),
              0xABCDEF123456);
    EXPECT_EQ(scratchbird::engine::wal_util::parse_segment_number("invalid_filename.log"), 0);
    EXPECT_EQ(scratchbird::engine::wal_util::parse_segment_number("wal_short.log"), 0);

    // Test finding WAL segments
    std::filesystem::create_directories(test_dir_);
    std::ofstream(test_dir_ + "/wal_0000000000000001.log").close();
    std::ofstream(test_dir_ + "/wal_0000000000000002.log").close();
    std::ofstream(test_dir_ + "/not_a_wal_file.txt").close();

    auto segments = scratchbird::engine::wal_util::find_wal_segments(test_dir_);
    EXPECT_EQ(segments.size(), 2);

    std::sort(segments.begin(), segments.end());
    EXPECT_EQ(segments[0], "wal_0000000000000001.log");
    EXPECT_EQ(segments[1], "wal_0000000000000002.log");
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
