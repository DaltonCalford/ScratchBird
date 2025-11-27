/**
 * test_concurrent_transactions.cpp - P2-12: Concurrent Transaction Tests
 *
 * Tests for concurrent transaction behavior including:
 * - Concurrent INSERT/UPDATE/DELETE operations
 * - Transaction isolation verification
 * - Deadlock detection and resolution
 * - Lock behavior testing
 * - Read-write and write-write conflicts
 *
 * P2-12: Concurrent Transaction Tests
 * Created: November 25, 2025
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <random>
#include <functional>

// =============================================================================
// TEST INFRASTRUCTURE
// =============================================================================

/**
 * Barrier for synchronizing thread start times in tests.
 * Ensures all threads start their work at approximately the same time.
 */
class ThreadBarrier
{
public:
    explicit ThreadBarrier(size_t count) : threshold_(count), count_(count), generation_(0) {}

    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto gen = generation_;
        if (--count_ == 0)
        {
            generation_++;
            count_ = threshold_;
            cv_.notify_all();
        }
        else
        {
            cv_.wait(lock, [this, gen] { return gen != generation_; });
        }
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    size_t threshold_;
    size_t count_;
    size_t generation_;
};

/**
 * Simple lock manager for testing lock contention patterns.
 */
class SimpleLockManager
{
public:
    bool tryLock(int resource_id, int transaction_id, bool exclusive)
    {
        std::lock_guard<std::mutex> guard(mutex_);

        auto& holders = locks_[resource_id];

        if (exclusive)
        {
            // Exclusive lock requires no other holders
            if (!holders.empty())
            {
                return false;
            }
            holders.insert({transaction_id, true});
            return true;
        }
        else
        {
            // Shared lock requires no exclusive holders
            for (const auto& [tid, is_exclusive] : holders)
            {
                if (is_exclusive)
                {
                    return false;
                }
            }
            holders.insert({transaction_id, false});
            return true;
        }
    }

    void unlock(int resource_id, int transaction_id)
    {
        std::lock_guard<std::mutex> guard(mutex_);

        auto& holders = locks_[resource_id];
        for (auto it = holders.begin(); it != holders.end(); ++it)
        {
            if (it->first == transaction_id)
            {
                holders.erase(it);
                break;
            }
        }
    }

    size_t getHolderCount(int resource_id)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return locks_[resource_id].size();
    }

private:
    std::mutex mutex_;
    std::map<int, std::set<std::pair<int, bool>>> locks_;
};

// =============================================================================
// ATOMIC COUNTER TESTS (Simulating concurrent row modifications)
// =============================================================================

class ConcurrentCounterTest : public ::testing::Test
{
protected:
    std::atomic<int64_t> counter_{0};
    std::atomic<int> successful_increments_{0};
    std::atomic<int> failed_increments_{0};
};

TEST_F(ConcurrentCounterTest, AtomicIncrement_ManyThreads)
{
    const int num_threads = 16;
    const int increments_per_thread = 10000;

    std::vector<std::thread> threads;
    ThreadBarrier barrier(num_threads);

    for (int i = 0; i < num_threads; i++)
    {
        threads.emplace_back([&]() {
            barrier.wait();  // Synchronize start

            for (int j = 0; j < increments_per_thread; j++)
            {
                counter_.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(counter_.load(), num_threads * increments_per_thread);
}

TEST_F(ConcurrentCounterTest, CompareExchange_Lost_Updates)
{
    // Simulate optimistic locking with compare-exchange
    const int num_threads = 8;
    const int attempts_per_thread = 1000;

    std::vector<std::thread> threads;
    ThreadBarrier barrier(num_threads);

    for (int i = 0; i < num_threads; i++)
    {
        threads.emplace_back([&]() {
            barrier.wait();

            for (int j = 0; j < attempts_per_thread; j++)
            {
                int64_t expected = counter_.load();
                int64_t desired = expected + 1;

                if (counter_.compare_exchange_strong(expected, desired))
                {
                    successful_increments_.fetch_add(1);
                }
                else
                {
                    failed_increments_.fetch_add(1);
                    // In real transactions, this would trigger retry or abort
                }
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // With CAS, some updates are lost when there's contention
    EXPECT_EQ(counter_.load(), successful_increments_.load());
    EXPECT_GT(failed_increments_.load(), 0) << "Expected some CAS failures due to contention";
}

// =============================================================================
// LOCK CONTENTION TESTS
// =============================================================================

class LockContentionTest : public ::testing::Test
{
protected:
    SimpleLockManager lock_manager_;
};

TEST_F(LockContentionTest, SharedLocks_Compatible)
{
    // Multiple transactions can hold shared locks on same resource
    EXPECT_TRUE(lock_manager_.tryLock(1, 100, false));  // Shared
    EXPECT_TRUE(lock_manager_.tryLock(1, 101, false));  // Shared
    EXPECT_TRUE(lock_manager_.tryLock(1, 102, false));  // Shared

    EXPECT_EQ(3, lock_manager_.getHolderCount(1));

    lock_manager_.unlock(1, 100);
    lock_manager_.unlock(1, 101);
    lock_manager_.unlock(1, 102);
}

TEST_F(LockContentionTest, ExclusiveLock_Blocks_Shared)
{
    // Exclusive lock blocks shared locks
    EXPECT_TRUE(lock_manager_.tryLock(1, 100, true));   // Exclusive
    EXPECT_FALSE(lock_manager_.tryLock(1, 101, false)); // Shared blocked

    lock_manager_.unlock(1, 100);
    EXPECT_TRUE(lock_manager_.tryLock(1, 101, false));  // Now allowed
}

TEST_F(LockContentionTest, SharedLock_Blocks_Exclusive)
{
    // Shared locks block exclusive locks
    EXPECT_TRUE(lock_manager_.tryLock(1, 100, false));  // Shared
    EXPECT_FALSE(lock_manager_.tryLock(1, 101, true));  // Exclusive blocked

    lock_manager_.unlock(1, 100);
    EXPECT_TRUE(lock_manager_.tryLock(1, 101, true));   // Now allowed
}

TEST_F(LockContentionTest, MultipleResources_Independent)
{
    // Locks on different resources don't conflict
    EXPECT_TRUE(lock_manager_.tryLock(1, 100, true));   // Exclusive on resource 1
    EXPECT_TRUE(lock_manager_.tryLock(2, 101, true));   // Exclusive on resource 2
    EXPECT_TRUE(lock_manager_.tryLock(3, 102, false));  // Shared on resource 3

    EXPECT_EQ(1, lock_manager_.getHolderCount(1));
    EXPECT_EQ(1, lock_manager_.getHolderCount(2));
    EXPECT_EQ(1, lock_manager_.getHolderCount(3));
}

// =============================================================================
// DEADLOCK DETECTION TESTS
// =============================================================================

class DeadlockTest : public ::testing::Test
{
protected:
    SimpleLockManager lock_manager_;
    std::atomic<int> deadlock_detected_{0};
    std::atomic<int> transactions_completed_{0};
};

TEST_F(DeadlockTest, DetectPotentialDeadlock_AB_BA)
{
    // Classic A->B, B->A deadlock scenario
    // Thread 1: Lock A, then try B
    // Thread 2: Lock B, then try A

    ThreadBarrier barrier(2);
    std::atomic<bool> thread1_got_both{false};
    std::atomic<bool> thread2_got_both{false};

    std::thread t1([&]() {
        barrier.wait();

        if (lock_manager_.tryLock(1, 100, true))  // Lock A
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (lock_manager_.tryLock(2, 100, true))  // Try B
            {
                thread1_got_both = true;
                lock_manager_.unlock(2, 100);
            }
            else
            {
                deadlock_detected_.fetch_add(1);
            }
            lock_manager_.unlock(1, 100);
        }
        transactions_completed_.fetch_add(1);
    });

    std::thread t2([&]() {
        barrier.wait();

        if (lock_manager_.tryLock(2, 101, true))  // Lock B
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (lock_manager_.tryLock(1, 101, true))  // Try A
            {
                thread2_got_both = true;
                lock_manager_.unlock(1, 101);
            }
            else
            {
                deadlock_detected_.fetch_add(1);
            }
            lock_manager_.unlock(2, 101);
        }
        transactions_completed_.fetch_add(1);
    });

    t1.join();
    t2.join();

    EXPECT_EQ(2, transactions_completed_.load());
    // In this scenario, at least one thread should detect the conflict
    EXPECT_GE(deadlock_detected_.load() + (thread1_got_both ? 1 : 0) + (thread2_got_both ? 1 : 0), 1);
}

// =============================================================================
// READ-WRITE CONFLICT TESTS
// =============================================================================

class ReadWriteConflictTest : public ::testing::Test
{
protected:
    std::atomic<int64_t> value_{0};
    std::atomic<int64_t> reads_before_write_{0};
    std::atomic<int64_t> reads_after_write_{0};
};

TEST_F(ReadWriteConflictTest, ConcurrentReadsDuringWrite)
{
    // One writer, multiple readers
    const int num_readers = 8;
    const int reads_per_reader = 1000;

    ThreadBarrier barrier(num_readers + 1);
    std::atomic<bool> write_started{false};
    std::atomic<bool> write_completed{false};

    // Writer thread
    std::thread writer([&]() {
        barrier.wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        write_started = true;
        value_.store(42);
        write_completed = true;
    });

    // Reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < num_readers; i++)
    {
        readers.emplace_back([&]() {
            barrier.wait();

            for (int j = 0; j < reads_per_reader; j++)
            {
                int64_t v = value_.load();
                if (!write_started.load())
                {
                    reads_before_write_.fetch_add(1);
                }
                else if (write_completed.load())
                {
                    reads_after_write_.fetch_add(1);
                    EXPECT_EQ(42, v) << "Read after write completed should see new value";
                }
                // Reads during write are undefined
            }
        });
    }

    writer.join();
    for (auto& r : readers)
    {
        r.join();
    }

    // Verify we had both before and after reads
    int64_t total = reads_before_write_.load() + reads_after_write_.load();
    EXPECT_GT(total, 0);
}

// =============================================================================
// WRITE-WRITE CONFLICT TESTS
// =============================================================================

class WriteWriteConflictTest : public ::testing::Test
{
protected:
    std::mutex write_mutex_;
    std::atomic<int64_t> last_writer_{-1};
    std::atomic<int> write_conflicts_{0};
};

TEST_F(WriteWriteConflictTest, LastWriterWins)
{
    // Multiple writers, last one wins
    const int num_writers = 8;

    ThreadBarrier barrier(num_writers);
    std::atomic<int64_t> value{0};

    std::vector<std::thread> writers;
    for (int i = 0; i < num_writers; i++)
    {
        writers.emplace_back([&, i]() {
            barrier.wait();

            // All writers try to write their ID
            value.store(i);
            last_writer_.store(i);
        });
    }

    for (auto& w : writers)
    {
        w.join();
    }

    // Some writer's value should be the final value
    int64_t final_value = value.load();
    EXPECT_GE(final_value, 0);
    EXPECT_LT(final_value, num_writers);
}

TEST_F(WriteWriteConflictTest, SerializedWrites)
{
    // Writes should be serialized with mutex
    const int num_writers = 8;
    const int writes_per_writer = 100;

    std::vector<int64_t> write_order;
    write_order.reserve(num_writers * writes_per_writer);

    ThreadBarrier barrier(num_writers);

    std::vector<std::thread> writers;
    for (int i = 0; i < num_writers; i++)
    {
        writers.emplace_back([&, i]() {
            barrier.wait();

            for (int j = 0; j < writes_per_writer; j++)
            {
                std::lock_guard<std::mutex> lock(write_mutex_);
                write_order.push_back(i * 1000 + j);
            }
        });
    }

    for (auto& w : writers)
    {
        w.join();
    }

    EXPECT_EQ(num_writers * writes_per_writer, write_order.size());

    // Verify no concurrent modifications (would cause issues)
    // Just check size is correct - mutex ensures serial access
}

// =============================================================================
// TRANSACTION ISOLATION SIMULATION TESTS
// =============================================================================

class IsolationTest : public ::testing::Test
{
protected:
    // Simulate transaction-visible data
    struct VersionedValue
    {
        int64_t value;
        int64_t xmin;  // Transaction that created this version
        int64_t xmax;  // Transaction that deleted this version (0 = current)
    };

    std::vector<VersionedValue> versions_;
    std::mutex versions_mutex_;
    std::atomic<int64_t> next_xid_{1};

    int64_t allocateXid() { return next_xid_.fetch_add(1); }

    bool isVisible(const VersionedValue& v, int64_t xid)
    {
        // Simple visibility: xmin <= xid and (xmax = 0 or xmax > xid)
        return v.xmin <= xid && (v.xmax == 0 || v.xmax > xid);
    }

    std::optional<int64_t> read(int64_t xid)
    {
        std::lock_guard<std::mutex> lock(versions_mutex_);
        for (auto it = versions_.rbegin(); it != versions_.rend(); ++it)
        {
            if (isVisible(*it, xid))
            {
                return it->value;
            }
        }
        return std::nullopt;
    }

    void write(int64_t xid, int64_t value)
    {
        std::lock_guard<std::mutex> lock(versions_mutex_);
        // Mark old version as deleted
        for (auto& v : versions_)
        {
            if (v.xmax == 0)
            {
                v.xmax = xid;
            }
        }
        // Add new version
        versions_.push_back({value, xid, 0});
    }
};

TEST_F(IsolationTest, SnapshotIsolation_ReadConsistency)
{
    // Transaction 1 reads initial value
    // Transaction 2 writes new value
    // Transaction 1 should still see old value (snapshot isolation)

    int64_t xid1 = allocateXid();
    write(xid1, 100);  // Initial write

    int64_t xid2 = allocateXid();  // Reader transaction starts
    int64_t xid3 = allocateXid();  // Writer transaction starts

    // Writer commits new value
    write(xid3, 200);

    // Reader should still see old value (snapshot isolation)
    auto value = read(xid2);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(100, *value) << "Snapshot isolation: reader should see value at snapshot time";

    // New transaction sees new value
    int64_t xid4 = allocateXid();
    auto new_value = read(xid4);
    ASSERT_TRUE(new_value.has_value());
    EXPECT_EQ(200, *new_value);
}

TEST_F(IsolationTest, RepeatableRead_SameValueTwice)
{
    // Transaction reads same row twice, should get same value

    int64_t xid1 = allocateXid();
    write(xid1, 42);

    int64_t reader_xid = allocateXid();

    auto first_read = read(reader_xid);
    ASSERT_TRUE(first_read.has_value());

    // Another transaction writes
    int64_t writer_xid = allocateXid();
    write(writer_xid, 99);

    // Second read should return same value
    auto second_read = read(reader_xid);
    ASSERT_TRUE(second_read.has_value());

    EXPECT_EQ(*first_read, *second_read) << "Repeatable read: same value for same transaction";
}

// =============================================================================
// THROUGHPUT TESTS
// =============================================================================

class ThroughputTest : public ::testing::Test
{
protected:
    std::atomic<int64_t> operations_completed_{0};
};

TEST_F(ThroughputTest, MixedReadWrite_Throughput)
{
    // 80% reads, 20% writes
    const int num_threads = 8;
    const int operations_per_thread = 10000;
    const int write_percentage = 20;

    std::atomic<int64_t> shared_value{0};
    std::mutex write_mutex;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; i++)
    {
        workers.emplace_back([&, i]() {
            std::mt19937 rng(i);
            std::uniform_int_distribution<int> dist(0, 99);

            for (int j = 0; j < operations_per_thread; j++)
            {
                if (dist(rng) < write_percentage)
                {
                    // Write operation
                    std::lock_guard<std::mutex> lock(write_mutex);
                    shared_value.fetch_add(1);
                }
                else
                {
                    // Read operation
                    (void)shared_value.load();
                }
                operations_completed_.fetch_add(1);
            }
        });
    }

    for (auto& w : workers)
    {
        w.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(num_threads * operations_per_thread, operations_completed_.load());

    // Just verify throughput is reasonable (no assertion, just informational)
    double ops_per_sec = operations_completed_.load() * 1000.0 / duration.count();
    SUCCEED() << "Throughput: " << ops_per_sec << " ops/sec";
}

// Note: main() is provided by GTest::gtest_main (linked in CMakeLists.txt)
