/**
 * Concurrency and Race Condition Tests
 * 
 * These tests verify thread safety, proper locking, and absence of race conditions.
 * They will expose deadlocks, data races, and consistency violations.
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <random>
#include <barrier>
#include "scratchbird/engine.h"

class ConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = std::filesystem::temp_directory_path() / "concurrency_test";
        std::filesystem::create_directories(test_dir);
        
        scratchbird::Status status;
        db = scratchbird::create_database(test_dir / "test.db", {}, status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    }
    
    void TearDown() override {
        if (db) {
            scratchbird::close_database(db);
        }
        std::filesystem::remove_all(test_dir);
    }
    
    std::filesystem::path test_dir;
    std::shared_ptr<scratchbird::Database> db;
};

// Test 1: Concurrent INSERT operations must not lose data
TEST_F(ConcurrencyTest, ConcurrentInsertsNoDataLoss) {
    const int num_threads = 10;
    const int inserts_per_thread = 1000;
    
    // Create table
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE concurrent_test (id INTEGER PRIMARY KEY, thread_id INTEGER, value INTEGER)", 
        status), {});
    
    std::atomic<int> id_counter(0);
    std::vector<std::thread> threads;
    std::atomic<int> failed_inserts(0);
    
    // Barrier to ensure all threads start simultaneously
    std::barrier sync_point(num_threads);
    
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, thread_id = t]() {
            scratchbird::Status local_status;
            auto local_session = scratchbird::create_session(db, local_status);
            if (local_status.code != scratchbird::StatusCode::Ok) {
                failed_inserts += inserts_per_thread;
                return;
            }
            
            sync_point.arrive_and_wait();  // Synchronize start
            
            for (int i = 0; i < inserts_per_thread; i++) {
                int id = id_counter.fetch_add(1);
                auto stmt = scratchbird::prepare(local_session,
                    "INSERT INTO concurrent_test VALUES (?, ?, ?)", local_status);
                auto result = scratchbird::execute(stmt, {
                    std::to_string(id),
                    std::to_string(thread_id),
                    std::to_string(i)
                });
                
                if (result.code != scratchbird::StatusCode::Ok) {
                    failed_inserts++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify no inserts failed
    EXPECT_EQ(failed_inserts.load(), 0) 
        << "Some inserts failed under concurrent load";
    
    // Verify all data is present
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT COUNT(*) as cnt FROM concurrent_test", status), {});
    int expected_count = num_threads * inserts_per_thread;
    EXPECT_EQ(std::stoi(result.rows[0]["cnt"]), expected_count)
        << "Data loss during concurrent inserts";
    
    // Verify data integrity - each thread's data should be complete
    for (int t = 0; t < num_threads; t++) {
        result = scratchbird::execute(scratchbird::prepare(session,
            "SELECT COUNT(*) as cnt FROM concurrent_test WHERE thread_id = ?", status),
            {std::to_string(t)});
        EXPECT_EQ(std::stoi(result.rows[0]["cnt"]), inserts_per_thread)
            << "Thread " << t << " lost data";
    }
}

// Test 2: Deadlock detection and resolution
TEST_F(ConcurrencyTest, DeadlockDetectionAndResolution) {
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create tables
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE account_a (id INTEGER PRIMARY KEY, balance INTEGER)", status), {});
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE account_b (id INTEGER PRIMARY KEY, balance INTEGER)", status), {});
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO account_a VALUES (1, 1000)", status), {});
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO account_b VALUES (1, 1000)", status), {});
    
    std::atomic<int> deadlock_detected(0);
    std::atomic<int> transactions_completed(0);
    
    // Thread 1: Transfer A -> B
    std::thread t1([&]() {
        scratchbird::Status local_status;
        auto local_session = scratchbird::create_session(db, local_status);
        auto txn = scratchbird::begin_transaction(local_session, local_status);
        
        // Lock account_a first
        scratchbird::execute(scratchbird::prepare(local_session,
            "UPDATE account_a SET balance = balance - 100 WHERE id = 1", local_status), {});
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Try to lock account_b
        auto result = scratchbird::execute(scratchbird::prepare(local_session,
            "UPDATE account_b SET balance = balance + 100 WHERE id = 1", local_status), {});
        
        if (result.code == scratchbird::StatusCode::DeadlockDetected) {
            deadlock_detected++;
            scratchbird::rollback(txn);
        } else {
            scratchbird::commit(txn);
            transactions_completed++;
        }
    });
    
    // Thread 2: Transfer B -> A (opposite order)
    std::thread t2([&]() {
        scratchbird::Status local_status;
        auto local_session = scratchbird::create_session(db, local_status);
        auto txn = scratchbird::begin_transaction(local_session, local_status);
        
        // Lock account_b first
        scratchbird::execute(scratchbird::prepare(local_session,
            "UPDATE account_b SET balance = balance - 50 WHERE id = 1", local_status), {});
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Try to lock account_a
        auto result = scratchbird::execute(scratchbird::prepare(local_session,
            "UPDATE account_a SET balance = balance + 50 WHERE id = 1", local_status), {});
        
        if (result.code == scratchbird::StatusCode::DeadlockDetected) {
            deadlock_detected++;
            scratchbird::rollback(txn);
        } else {
            scratchbird::commit(txn);
            transactions_completed++;
        }
    });
    
    t1.join();
    t2.join();
    
    // Either deadlock was detected OR both transactions completed
    // But not neither (hanging) or data corruption
    EXPECT_TRUE(deadlock_detected > 0 || transactions_completed == 2)
        << "Deadlock not detected and transactions didn't complete - system hung!";
    
    // Verify data consistency
    auto result_a = scratchbird::execute(scratchbird::prepare(session,
        "SELECT balance FROM account_a WHERE id = 1", status), {});
    auto result_b = scratchbird::execute(scratchbird::prepare(session,
        "SELECT balance FROM account_b WHERE id = 1", status), {});
    
    int balance_a = std::stoi(result_a.rows[0]["balance"]);
    int balance_b = std::stoi(result_b.rows[0]["balance"]);
    
    // Total should always be 2000
    EXPECT_EQ(balance_a + balance_b, 2000)
        << "Money created or destroyed - transaction isolation violated!";
}

// Test 3: Reader-Writer lock correctness
TEST_F(ConcurrencyTest, ReaderWriterLockCorrectness) {
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE rw_test (id INTEGER PRIMARY KEY, value INTEGER)", status), {});
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO rw_test VALUES (1, 0)", status), {});
    
    const int num_readers = 10;
    const int num_writers = 3;
    const int operations_per_thread = 100;
    
    std::atomic<int> concurrent_readers(0);
    std::atomic<int> concurrent_writers(0);
    std::atomic<bool> violation_detected(false);
    
    auto reader_task = [&]() {
        scratchbird::Status local_status;
        auto local_session = scratchbird::create_session(db, local_status);
        
        for (int i = 0; i < operations_per_thread; i++) {
            concurrent_readers++;
            
            // Check for violation: writer active while reading
            if (concurrent_writers > 0) {
                violation_detected = true;
            }
            
            auto result = scratchbird::execute(scratchbird::prepare(local_session,
                "SELECT value FROM rw_test WHERE id = 1", local_status), {});
            
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            concurrent_readers--;
        }
    };
    
    auto writer_task = [&]() {
        scratchbird::Status local_status;
        auto local_session = scratchbird::create_session(db, local_status);
        
        for (int i = 0; i < operations_per_thread; i++) {
            concurrent_writers++;
            
            // Check for violations
            if (concurrent_writers > 1) {
                violation_detected = true;  // Multiple writers
            }
            if (concurrent_readers > 0) {
                violation_detected = true;  // Readers during write
            }
            
            scratchbird::execute(scratchbird::prepare(local_session,
                "UPDATE rw_test SET value = value + 1 WHERE id = 1", local_status), {});
            
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            concurrent_writers--;
        }
    };
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_readers; i++) {
        threads.emplace_back(reader_task);
    }
    for (int i = 0; i < num_writers; i++) {
        threads.emplace_back(writer_task);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_FALSE(violation_detected)
        << "Reader-Writer lock violation: concurrent reads and writes detected";
    
    // Verify final value is correct
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT value FROM rw_test WHERE id = 1", status), {});
    int final_value = std::stoi(result.rows[0]["value"]);
    int expected_value = num_writers * operations_per_thread;
    
    EXPECT_EQ(final_value, expected_value)
        << "Lost updates - concurrent writes not properly synchronized";
}

// Test 4: Connection pool thread safety
TEST_F(ConcurrencyTest, ConnectionPoolThreadSafety) {
    const int num_threads = 50;
    const int operations_per_thread = 100;
    
    scratchbird::ConnectionPool pool(db, 10);  // Pool size = 10
    std::atomic<int> active_connections(0);
    std::atomic<int> max_active(0);
    std::atomic<int> errors(0);
    
    auto worker = [&]() {
        for (int i = 0; i < operations_per_thread; i++) {
            try {
                auto conn = pool.acquire();
                
                int current = active_connections.fetch_add(1) + 1;
                int prev_max = max_active.load();
                while (prev_max < current && 
                       !max_active.compare_exchange_weak(prev_max, current)) {
                    // Update max
                }
                
                // Simulate work
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                
                active_connections--;
                // Connection automatically released when out of scope
            } catch (...) {
                errors++;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(errors.load(), 0)
        << "Connection pool errors under concurrent access";
    
    EXPECT_LE(max_active.load(), 10)
        << "Connection pool exceeded maximum size - not properly limiting connections";
}

// Test 5: Prepared statement cache thread safety
TEST_F(ConcurrencyTest, PreparedStatementCacheThreadSafety) {
    const int num_threads = 20;
    const int queries_per_thread = 500;
    
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE cache_test (id INTEGER, value INTEGER)", status), {});
    
    std::atomic<int> cache_hits(0);
    std::atomic<int> cache_misses(0);
    std::atomic<int> errors(0);
    
    auto worker = [&](int thread_id) {
        scratchbird::Status local_status;
        auto local_session = scratchbird::create_session(db, local_status);
        
        for (int i = 0; i < queries_per_thread; i++) {
            // Use a small set of queries to test cache
            int query_id = i % 10;
            std::string query = "SELECT * FROM cache_test WHERE id = " + 
                               std::to_string(query_id);
            
            auto stmt = scratchbird::prepare_cached(local_session, query, local_status);
            
            if (local_status.code != scratchbird::StatusCode::Ok) {
                errors++;
                continue;
            }
            
            if (stmt.from_cache) {
                cache_hits++;
            } else {
                cache_misses++;
            }
            
            // Execute the statement
            auto result = scratchbird::execute(stmt, {});
            if (result.code != scratchbird::StatusCode::Ok) {
                errors++;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(errors.load(), 0)
        << "Errors in prepared statement cache under concurrent access";
    
    // Cache should have significant hit rate
    double hit_rate = (double)cache_hits / (cache_hits + cache_misses);
    EXPECT_GT(hit_rate, 0.8)
        << "Cache hit rate too low: " << hit_rate << " - cache not working properly";
}

// Test 6: Global state race conditions
TEST_F(ConcurrencyTest, GlobalStateRaceConditions) {
    // Test that global state is properly protected
    const int num_threads = 20;
    const int operations = 1000;
    
    std::atomic<int> race_detected(0);
    
    auto worker = [&]() {
        for (int i = 0; i < operations; i++) {
            // Try to trigger race conditions in global state
            
            // Test 1: Executor XID counter
            auto xid1 = scratchbird::get_next_transaction_id();
            auto xid2 = scratchbird::get_next_transaction_id();
            if (xid2 <= xid1) {
                race_detected++;  // XID went backwards!
            }
            
            // Test 2: Constraint deferred state
            bool prev = scratchbird::get_constraints_deferred();
            scratchbird::set_constraints_deferred(!prev);
            bool curr = scratchbird::get_constraints_deferred();
            if (curr == prev) {
                race_detected++;  // State change didn't take effect
            }
            scratchbird::set_constraints_deferred(prev);  // Restore
            
            // Test 3: Prepared statement handle allocation
            auto handle1 = scratchbird::allocate_statement_handle();
            auto handle2 = scratchbird::allocate_statement_handle();
            if (handle2 <= handle1) {
                race_detected++;  // Handle allocation not atomic
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(race_detected.load(), 0)
        << "Race conditions detected in global state management";
}

// Test 7: Stress test - mixed operations
TEST_F(ConcurrencyTest, StressTestMixedOperations) {
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create multiple tables
    for (int i = 0; i < 5; i++) {
        std::string table = "stress_" + std::to_string(i);
        scratchbird::execute(scratchbird::prepare(session,
            "CREATE TABLE " + table + " (id INTEGER PRIMARY KEY, data TEXT)", status), {});
    }
    
    const int num_threads = 30;
    const int duration_seconds = 5;
    std::atomic<bool> stop(false);
    std::atomic<int> operations_completed(0);
    std::atomic<int> errors(0);
    
    auto worker = [&](int thread_id) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> table_dist(0, 4);
        std::uniform_int_distribution<> op_dist(0, 3);
        std::uniform_int_distribution<> id_dist(1, 1000);
        
        scratchbird::Status local_status;
        auto local_session = scratchbird::create_session(db, local_status);
        
        while (!stop) {
            int table_id = table_dist(gen);
            int op_type = op_dist(gen);
            int id = id_dist(gen);
            std::string table = "stress_" + std::to_string(table_id);
            
            try {
                switch (op_type) {
                    case 0: {  // INSERT
                        auto stmt = scratchbird::prepare(local_session,
                            "INSERT OR REPLACE INTO " + table + " VALUES (?, ?)", local_status);
                        scratchbird::execute(stmt, {std::to_string(id), "Data " + std::to_string(id)});
                        break;
                    }
                    case 1: {  // UPDATE
                        auto stmt = scratchbird::prepare(local_session,
                            "UPDATE " + table + " SET data = ? WHERE id = ?", local_status);
                        scratchbird::execute(stmt, {"Updated " + std::to_string(id), std::to_string(id)});
                        break;
                    }
                    case 2: {  // DELETE
                        auto stmt = scratchbird::prepare(local_session,
                            "DELETE FROM " + table + " WHERE id = ?", local_status);
                        scratchbird::execute(stmt, {std::to_string(id)});
                        break;
                    }
                    case 3: {  // SELECT
                        auto stmt = scratchbird::prepare(local_session,
                            "SELECT * FROM " + table + " WHERE id = ?", local_status);
                        scratchbird::execute(stmt, {std::to_string(id)});
                        break;
                    }
                }
                operations_completed++;
            } catch (...) {
                errors++;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker, i);
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    stop = true;
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Calculate operations per second
    double ops_per_second = (double)operations_completed / duration_seconds;
    
    std::cout << "Stress test completed: " << operations_completed 
              << " operations, " << errors << " errors, "
              << ops_per_second << " ops/sec" << std::endl;
    
    EXPECT_LT(errors.load(), operations_completed.load() * 0.01)
        << "Too many errors under stress: " << errors << " out of " << operations_completed;
    
    EXPECT_GT(ops_per_second, 1000)
        << "Performance too low under concurrent load: " << ops_per_second << " ops/sec";
    
    // Verify database consistency
    for (int i = 0; i < 5; i++) {
        std::string table = "stress_" + std::to_string(i);
        auto result = scratchbird::execute(scratchbird::prepare(session,
            "SELECT COUNT(*) as cnt FROM " + table, status), {});
        // Just verify query works - table not corrupted
        EXPECT_EQ(result.code, scratchbird::StatusCode::Ok)
            << "Table " << table << " corrupted during stress test";
    }
}