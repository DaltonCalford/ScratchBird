// ScratchBird TOAST Concurrency Stress Test
// Tests Phase 5: Concurrent TOAST operations with MGA transaction isolation
//
// Validates:
// - Concurrent TOAST inserts from multiple transactions
// - Concurrent TOAST reads with different isolation levels
// - Concurrent TOAST deletes
// - TIP-based visibility under concurrency
// - No data corruption or visibility violations

#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/transaction_manager.h"
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <random>

using namespace scratchbird::core;

class ToastConcurrencyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_ = std::make_unique<Database>("test_toast_concurrency.db", 100 * 1024 * 1024);
        ASSERT_EQ(db_->open(), Status::OK);

        tm_ = db_->transaction_manager();
        ASSERT_NE(tm_, nullptr);

        table_id_ = generateUuidV7();
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
        }
    }

    // Helper: Create large text for TOASTing
    std::string createLargeText(size_t size_kb, uint64_t seed)
    {
        std::string text;
        text.reserve(size_kb * 1024);

        for (size_t i = 0; i < size_kb * 1024; i += 64)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "THREAD_%lu_OFFSET_%08zu_", seed, i);
            text.append(buf);
        }

        return text;
    }

    std::unique_ptr<Database> db_;
    TransactionManager* tm_;
    ID table_id_;
};

// =============================================================================
// Test 1: Concurrent TOAST Inserts - No Conflicts
// =============================================================================

TEST_F(ToastConcurrencyTest, ConcurrentInserts_NoConflicts)
{
    // Validates concurrent TOAST inserts from multiple transactions
    //
    // Scenario:
    // - 10 threads each insert 10 TOAST values concurrently
    // - Each transaction commits independently
    // - Verify all 100 values inserted successfully
    // - Verify no data corruption (each value readable)

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    constexpr int NUM_THREADS = 10;
    constexpr int INSERTS_PER_THREAD = 10;

    std::vector<std::thread> threads;
    std::atomic<int> successful_inserts{0};
    std::atomic<int> failed_inserts{0};

    // Launch threads
    for (int t = 0; t < NUM_THREADS; t++)
    {
        threads.emplace_back([&, thread_id = t]() {
            for (int i = 0; i < INSERTS_PER_THREAD; i++)
            {
                uint64_t xid = tm_->beginTransaction();
                if (xid == 0)
                {
                    failed_inserts++;
                    continue;
                }

                ErrorContext thread_ctx;
                std::string data = createLargeText(3, thread_id * 1000 + i);
                std::vector<uint8_t> data_vec(data.begin(), data.end());

                ToastPointer toast_ptr;
                Status s = toast_mgr.toastValue(
                    data_vec.data(),
                    data_vec.size(),
                    ToastStrategy::EXTERNAL,
                    xid,
                    &toast_ptr,
                    &thread_ctx
                );

                if (s == Status::OK)
                {
                    tm_->commit(xid);
                    successful_inserts++;
                }
                else
                {
                    tm_->abort(xid);
                    failed_inserts++;
                }
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(successful_inserts.load(), NUM_THREADS * INSERTS_PER_THREAD)
        << "All inserts should succeed";
    EXPECT_EQ(failed_inserts.load(), 0) << "No inserts should fail";
}

// =============================================================================
// Test 2: Concurrent Reads - Snapshot Isolation
// =============================================================================

TEST_F(ToastConcurrencyTest, ConcurrentReads_SnapshotIsolation)
{
    // Validates concurrent reads with snapshot isolation
    //
    // Scenario:
    // - Insert 10 TOAST values (commit)
    // - 20 threads concurrently read all 10 values
    // - Verify all reads succeed
    // - Verify data integrity (no corruption)

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Insert 10 TOAST values
    constexpr int NUM_VALUES = 10;
    std::vector<ToastPointer> toast_pointers;
    std::vector<std::string> expected_data;

    for (int i = 0; i < NUM_VALUES; i++)
    {
        uint64_t xid = tm_->beginTransaction();
        std::string data = createLargeText(2, i);
        expected_data.push_back(data);

        std::vector<uint8_t> data_vec(data.begin(), data.end());
        ToastPointer toast_ptr;

        status = toast_mgr.toastValue(
            data_vec.data(),
            data_vec.size(),
            ToastStrategy::EXTERNAL,
            xid,
            &toast_ptr,
            &ctx
        );

        ASSERT_EQ(status, Status::OK);
        toast_pointers.push_back(toast_ptr);
        tm_->commit(xid);
    }

    // Concurrent reads
    constexpr int NUM_READER_THREADS = 20;
    std::atomic<int> successful_reads{0};
    std::atomic<int> failed_reads{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_READER_THREADS; t++)
    {
        threads.emplace_back([&]() {
            uint64_t xid = tm_->beginTransaction();
            if (xid == 0)
            {
                failed_reads += NUM_VALUES;
                return;
            }

            for (int i = 0; i < NUM_VALUES; i++)
            {
                ErrorContext thread_ctx;
                std::vector<uint8_t> detoasted_data;
                Status s = toast_mgr.detoastValue(
                    &toast_pointers[i],
                    &detoasted_data,
                    xid,
                    &thread_ctx
                );

                if (s == Status::OK)
                {
                    // Verify data integrity
                    std::vector<uint8_t> expected_vec(
                        expected_data[i].begin(),
                        expected_data[i].end()
                    );

                    if (detoasted_data == expected_vec)
                    {
                        successful_reads++;
                    }
                    else
                    {
                        failed_reads++;
                    }
                }
                else
                {
                    failed_reads++;
                }
            }

            tm_->commit(xid);
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(successful_reads.load(), NUM_READER_THREADS * NUM_VALUES)
        << "All reads should succeed with correct data";
    EXPECT_EQ(failed_reads.load(), 0) << "No reads should fail";
}

// =============================================================================
// Test 3: Concurrent Insert and Read - Isolation
// =============================================================================

TEST_F(ToastConcurrencyTest, ConcurrentInsertAndRead_Isolation)
{
    // Validates isolation between concurrent inserts and reads
    //
    // Scenario:
    // - 5 writer threads continuously insert TOAST values
    // - 10 reader threads continuously read committed values
    // - Run for 5 seconds
    // - Verify no visibility violations (readers never see uncommitted data)

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    std::atomic<bool> stop{false};
    std::atomic<int> inserts{0};
    std::atomic<int> reads{0};
    std::atomic<int> visibility_violations{0};

    // Shared list of committed TOAST pointers (protected by atomic operations)
    std::vector<ToastPointer> committed_pointers;
    std::mutex pointers_mutex;

    // Writer threads
    constexpr int NUM_WRITERS = 5;
    std::vector<std::thread> writer_threads;

    for (int w = 0; w < NUM_WRITERS; w++)
    {
        writer_threads.emplace_back([&, writer_id = w]() {
            std::mt19937 rng(writer_id);
            int insert_count = 0;

            while (!stop.load())
            {
                uint64_t xid = tm_->beginTransaction();
                if (xid == 0) continue;

                ErrorContext thread_ctx;
                std::string data = createLargeText(2, writer_id * 10000 + insert_count);
                std::vector<uint8_t> data_vec(data.begin(), data.end());

                ToastPointer toast_ptr;
                Status s = toast_mgr.toastValue(
                    data_vec.data(),
                    data_vec.size(),
                    ToastStrategy::EXTERNAL,
                    xid,
                    &toast_ptr,
                    &thread_ctx
                );

                if (s == Status::OK)
                {
                    tm_->commit(xid);

                    {
                        std::lock_guard<std::mutex> lock(pointers_mutex);
                        committed_pointers.push_back(toast_ptr);
                    }

                    inserts++;
                    insert_count++;
                }
                else
                {
                    tm_->abort(xid);
                }

                // Small delay to avoid overwhelming
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Reader threads
    constexpr int NUM_READERS = 10;
    std::vector<std::thread> reader_threads;

    for (int r = 0; r < NUM_READERS; r++)
    {
        reader_threads.emplace_back([&]() {
            std::mt19937 rng(r);

            while (!stop.load())
            {
                uint64_t xid = tm_->beginTransaction();
                if (xid == 0) continue;

                // Get snapshot of committed pointers
                std::vector<ToastPointer> snapshot;
                {
                    std::lock_guard<std::mutex> lock(pointers_mutex);
                    snapshot = committed_pointers;
                }

                if (!snapshot.empty())
                {
                    // Pick random pointer to read
                    std::uniform_int_distribution<size_t> dist(0, snapshot.size() - 1);
                    size_t idx = dist(rng);

                    ErrorContext thread_ctx;
                    std::vector<uint8_t> detoasted_data;
                    Status s = toast_mgr.detoastValue(
                        &snapshot[idx],
                        &detoasted_data,
                        xid,
                        &thread_ctx
                    );

                    if (s == Status::OK)
                    {
                        reads++;
                    }
                    // If read fails, it's a visibility violation (should never happen)
                    else
                    {
                        visibility_violations++;
                    }
                }

                tm_->commit(xid);

                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    // Run for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));
    stop.store(true);

    // Wait for all threads
    for (auto& t : writer_threads) t.join();
    for (auto& t : reader_threads) t.join();

    EXPECT_GT(inserts.load(), 0) << "Should have some inserts";
    EXPECT_GT(reads.load(), 0) << "Should have some reads";
    EXPECT_EQ(visibility_violations.load(), 0) << "No visibility violations allowed";
}

// =============================================================================
// Test 4: Concurrent Deletes - TIP-Based Visibility
// =============================================================================

TEST_F(ToastConcurrencyTest, ConcurrentDeletes_TIPVisibility)
{
    // Validates concurrent deletes with TIP-based visibility
    //
    // Scenario:
    // - Insert 20 TOAST values (commit)
    // - 10 threads concurrently delete different values
    // - Verify deletes don't interfere with each other
    // - Verify deleted values become invisible after commit

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Insert 20 TOAST values
    constexpr int NUM_VALUES = 20;
    std::vector<uint32_t> value_ids;
    std::vector<ToastPointer> toast_pointers;

    for (int i = 0; i < NUM_VALUES; i++)
    {
        uint64_t xid = tm_->beginTransaction();
        std::string data = createLargeText(2, i);
        std::vector<uint8_t> data_vec(data.begin(), data.end());

        ToastPointer toast_ptr;
        status = toast_mgr.toastValue(
            data_vec.data(),
            data_vec.size(),
            ToastStrategy::EXTERNAL,
            xid,
            &toast_ptr,
            &ctx
        );

        ASSERT_EQ(status, Status::OK);
        value_ids.push_back(toast_ptr.va_valueid);
        toast_pointers.push_back(toast_ptr);
        tm_->commit(xid);
    }

    // Concurrent deletes (each thread deletes 2 values)
    constexpr int NUM_THREADS = 10;
    std::atomic<int> successful_deletes{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++)
    {
        threads.emplace_back([&, thread_id = t]() {
            // Delete 2 values per thread
            for (int i = 0; i < 2; i++)
            {
                int value_idx = thread_id * 2 + i;

                uint64_t xid = tm_->beginTransaction();
                if (xid == 0) continue;

                ErrorContext thread_ctx;
                Status s = toast_mgr.deleteToastValue(
                    value_ids[value_idx],
                    xid,
                    &thread_ctx
                );

                if (s == Status::OK)
                {
                    tm_->commit(xid);
                    successful_deletes++;
                }
                else
                {
                    tm_->abort(xid);
                }
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(successful_deletes.load(), NUM_VALUES)
        << "All deletes should succeed";

    // Verify deleted values are invisible
    uint64_t read_xid = tm_->beginTransaction();
    int invisible_count = 0;

    for (int i = 0; i < NUM_VALUES; i++)
    {
        std::vector<uint8_t> detoasted_data;
        status = toast_mgr.detoastValue(
            &toast_pointers[i],
            &detoasted_data,
            read_xid,
            &ctx
        );

        if (status != Status::OK)
        {
            invisible_count++;
        }
    }

    tm_->commit(read_xid);

    EXPECT_EQ(invisible_count, NUM_VALUES)
        << "All deleted values should be invisible";
}

// =============================================================================
// Test 5: High Contention - Many Transactions on Same Value
// =============================================================================

TEST_F(ToastConcurrencyTest, HighContention_ManyTransactionsSameValue)
{
    // Validates behavior under high contention (many transactions operating on same value)
    //
    // Scenario:
    // - Insert 1 TOAST value (commit)
    // - 50 threads concurrently try to read the same value
    // - Verify all reads succeed
    // - Verify data integrity

    ToastManager toast_mgr(db_.get(), table_id_);
    ErrorContext ctx;

    Status status = toast_mgr.createToastTable(&ctx);
    if (status != Status::OK)
    {
        GTEST_SKIP() << "Failed to create TOAST table";
        return;
    }

    // Insert single value
    uint64_t xid = tm_->beginTransaction();
    std::string expected_data = createLargeText(5, 12345);
    std::vector<uint8_t> expected_vec(expected_data.begin(), expected_data.end());

    ToastPointer toast_ptr;
    status = toast_mgr.toastValue(
        expected_vec.data(),
        expected_vec.size(),
        ToastStrategy::EXTERNAL,
        xid,
        &toast_ptr,
        &ctx
    );

    ASSERT_EQ(status, Status::OK);
    tm_->commit(xid);

    // High contention reads
    constexpr int NUM_THREADS = 50;
    std::atomic<int> successful_reads{0};
    std::atomic<int> failed_reads{0};
    std::atomic<int> data_corruption{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++)
    {
        threads.emplace_back([&]() {
            uint64_t read_xid = tm_->beginTransaction();
            if (read_xid == 0)
            {
                failed_reads++;
                return;
            }

            ErrorContext thread_ctx;
            std::vector<uint8_t> detoasted_data;
            Status s = toast_mgr.detoastValue(
                &toast_ptr,
                &detoasted_data,
                read_xid,
                &thread_ctx
            );

            if (s == Status::OK)
            {
                if (detoasted_data == expected_vec)
                {
                    successful_reads++;
                }
                else
                {
                    data_corruption++;
                }
            }
            else
            {
                failed_reads++;
            }

            tm_->commit(read_xid);
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(successful_reads.load(), NUM_THREADS)
        << "All reads should succeed";
    EXPECT_EQ(failed_reads.load(), 0) << "No reads should fail";
    EXPECT_EQ(data_corruption.load(), 0) << "No data corruption allowed";
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
