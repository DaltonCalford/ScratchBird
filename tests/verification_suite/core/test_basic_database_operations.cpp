/**
 * Core Database Functionality Verification Tests
 * 
 * These tests verify that the database actually works as a database.
 * No mocking allowed - tests must use real implementation.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include <random>
#include <set>
#include "scratchbird.h"
#include "scratchbird/engine.h"
#include "scratchbird/server.h"

namespace fs = std::filesystem;

class CoreDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / ("scratchbird_test_" + 
                   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(test_dir);
        db_path = test_dir / "test.db";
    }

    void TearDown() override {
        fs::remove_all(test_dir);
    }

    fs::path test_dir;
    fs::path db_path;
};

// Test 1: Verify the main executable actually starts a database server
TEST_F(CoreDatabaseTest, MainExecutableStartsServer) {
    // The main() function should start a server, not just print version
    // This test will fail if main() only prints and exits
    
    std::thread server_thread([]() {
        // Simulate running main with server arguments
        const char* argv[] = {"scratchbird", "--server", "--port", "5432"};
        // main() should block here running the server
        // If it returns immediately, the test fails
        FAIL() << "main() returned immediately instead of running a server";
    });
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Try to connect to the server
    bool connected = false;
    for (int i = 0; i < 10; i++) {
        // Attempt connection (this will fail if no server is running)
        // Real implementation needed here
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    EXPECT_TRUE(connected) << "Could not connect to database server - main() doesn't start a server";
    
    server_thread.detach();
}

// Test 2: Database creation must actually create persistent files
TEST_F(CoreDatabaseTest, DatabaseCreationCreatesPersistentFiles) {
    scratchbird::Status status;
    scratchbird::CreateDbOptions opts;
    opts.page_size = 4096;
    
    auto db = scratchbird::create_database(db_path.string(), opts, status);
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok) 
        << "Failed to create database: " << status.message;
    ASSERT_NE(db, nullptr) << "create_database returned null";
    
    // Verify physical files exist
    EXPECT_TRUE(fs::exists(db_path.string() + ".seg0")) 
        << "Database segment file not created";
    
    // Verify file has actual content (not empty)
    auto file_size = fs::file_size(db_path.string() + ".seg0");
    EXPECT_GT(file_size, 4096) << "Database file is too small to contain real data";
    
    // Close database
    scratchbird::close_database(db);
    
    // Files should still exist after closing
    EXPECT_TRUE(fs::exists(db_path.string() + ".seg0")) 
        << "Database files disappeared after closing";
}

// Test 3: Database must survive process restart
TEST_F(CoreDatabaseTest, DatabaseSurvivesProcessRestart) {
    const std::string test_data = "Critical data that must persist";
    
    // Phase 1: Create database and insert data
    {
        scratchbird::Status status;
        scratchbird::CreateDbOptions opts;
        auto db = scratchbird::create_database(db_path.string(), opts, status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
        
        auto session = scratchbird::create_session(db, status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
        
        // Create table and insert data
        auto create_stmt = scratchbird::prepare(session, 
            "CREATE TABLE test_table (id INTEGER PRIMARY KEY, data TEXT)", status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
        ASSERT_EQ(scratchbird::execute(create_stmt, {}).code, scratchbird::StatusCode::Ok);
        
        auto insert_stmt = scratchbird::prepare(session,
            "INSERT INTO test_table (id, data) VALUES (1, ?)", status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
        ASSERT_EQ(scratchbird::execute(insert_stmt, {test_data}).code, scratchbird::StatusCode::Ok);
        
        scratchbird::close_database(db);
    }
    
    // Simulate process restart
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Phase 2: Open existing database and verify data
    {
        scratchbird::Status status;
        auto db = scratchbird::open_database(db_path.string(), status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok) 
            << "Failed to open existing database";
        
        auto session = scratchbird::create_session(db, status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
        
        auto select_stmt = scratchbird::prepare(session,
            "SELECT data FROM test_table WHERE id = 1", status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
        
        auto result = scratchbird::execute(select_stmt, {});
        ASSERT_EQ(result.code, scratchbird::StatusCode::Ok);
        
        // Verify data persisted
        ASSERT_FALSE(result.rows.empty()) << "No data returned after restart";
        ASSERT_EQ(result.rows[0]["data"], test_data) 
            << "Data corruption: retrieved data doesn't match inserted data";
        
        scratchbird::close_database(db);
    }
}

// Test 4: Basic CRUD operations must work
TEST_F(CoreDatabaseTest, BasicCRUDOperations) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(db_path.string(), {}, status);
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    
    auto session = scratchbird::create_session(db, status);
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    
    // CREATE
    auto create = scratchbird::prepare(session,
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)", status);
    ASSERT_EQ(scratchbird::execute(create, {}).code, scratchbird::StatusCode::Ok);
    
    // INSERT
    auto insert = scratchbird::prepare(session,
        "INSERT INTO users (id, name, age) VALUES (?, ?, ?)", status);
    ASSERT_EQ(scratchbird::execute(insert, {"1", "Alice", "30"}).code, scratchbird::StatusCode::Ok);
    ASSERT_EQ(scratchbird::execute(insert, {"2", "Bob", "25"}).code, scratchbird::StatusCode::Ok);
    
    // SELECT
    auto select = scratchbird::prepare(session, "SELECT * FROM users ORDER BY id", status);
    auto result = scratchbird::execute(select, {});
    ASSERT_EQ(result.code, scratchbird::StatusCode::Ok);
    ASSERT_EQ(result.rows.size(), 2) << "INSERT didn't actually insert data";
    ASSERT_EQ(result.rows[0]["name"], "Alice");
    ASSERT_EQ(result.rows[1]["name"], "Bob");
    
    // UPDATE
    auto update = scratchbird::prepare(session,
        "UPDATE users SET age = ? WHERE name = ?", status);
    ASSERT_EQ(scratchbird::execute(update, {"31", "Alice"}).code, scratchbird::StatusCode::Ok);
    
    // Verify UPDATE
    result = scratchbird::execute(select, {});
    ASSERT_EQ(result.rows[0]["age"], "31") << "UPDATE didn't actually update data";
    
    // DELETE
    auto delete_stmt = scratchbird::prepare(session,
        "DELETE FROM users WHERE id = ?", status);
    ASSERT_EQ(scratchbird::execute(delete_stmt, {"2"}).code, scratchbird::StatusCode::Ok);
    
    // Verify DELETE
    result = scratchbird::execute(select, {});
    ASSERT_EQ(result.rows.size(), 1) << "DELETE didn't actually delete data";
    ASSERT_EQ(result.rows[0]["name"], "Alice");
    
    scratchbird::close_database(db);
}

// Test 5: Transactions must be atomic
TEST_F(CoreDatabaseTest, TransactionAtomicity) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(db_path.string(), {}, status);
    auto session = scratchbird::create_session(db, status);
    
    // Setup
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE accounts (id INTEGER PRIMARY KEY, balance INTEGER)", status), {});
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO accounts VALUES (1, 1000), (2, 1000)", status), {});
    
    // Test rollback
    auto txn = scratchbird::begin_transaction(session, status);
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    
    scratchbird::execute(scratchbird::prepare(session,
        "UPDATE accounts SET balance = balance - 500 WHERE id = 1", status), {});
    scratchbird::execute(scratchbird::prepare(session,
        "UPDATE accounts SET balance = balance + 500 WHERE id = 2", status), {});
    
    // Rollback
    ASSERT_EQ(scratchbird::rollback(txn).code, scratchbird::StatusCode::Ok);
    
    // Verify rollback worked
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT SUM(balance) as total FROM accounts", status), {});
    ASSERT_EQ(result.rows[0]["total"], "2000") 
        << "Transaction rollback failed - data was modified";
    
    // Test commit
    txn = scratchbird::begin_transaction(session, status);
    scratchbird::execute(scratchbird::prepare(session,
        "UPDATE accounts SET balance = balance - 500 WHERE id = 1", status), {});
    scratchbird::execute(scratchbird::prepare(session,
        "UPDATE accounts SET balance = balance + 500 WHERE id = 2", status), {});
    
    // Commit
    ASSERT_EQ(scratchbird::commit(txn).code, scratchbird::StatusCode::Ok);
    
    // Verify commit worked
    result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT balance FROM accounts ORDER BY id", status), {});
    ASSERT_EQ(result.rows[0]["balance"], "500");
    ASSERT_EQ(result.rows[1]["balance"], "1500");
    
    scratchbird::close_database(db);
}

// Test 6: Database must handle large datasets
TEST_F(CoreDatabaseTest, LargeDatasetHandling) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(db_path.string(), {}, status);
    auto session = scratchbird::create_session(db, status);
    
    // Create table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE large_table (id INTEGER PRIMARY KEY, data TEXT)", status), {});
    
    // Insert 10,000 rows
    const int num_rows = 10000;
    auto insert = scratchbird::prepare(session,
        "INSERT INTO large_table (id, data) VALUES (?, ?)", status);
    
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < num_rows; i++) {
        std::string data = "Row data " + std::to_string(i) + " with some padding text";
        ASSERT_EQ(scratchbird::execute(insert, {std::to_string(i), data}).code, 
                  scratchbird::StatusCode::Ok) 
            << "Failed to insert row " << i;
    }
    auto insert_time = std::chrono::steady_clock::now() - start;
    
    // Verify all rows exist
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT COUNT(*) as cnt FROM large_table", status), {});
    ASSERT_EQ(std::stoi(result.rows[0]["cnt"]), num_rows) 
        << "Not all rows were inserted";
    
    // Test query performance
    start = std::chrono::steady_clock::now();
    result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT * FROM large_table WHERE id >= 5000 AND id < 5100", status), {});
    auto query_time = std::chrono::steady_clock::now() - start;
    
    ASSERT_EQ(result.rows.size(), 100) << "Range query returned wrong number of rows";
    
    // Performance assertions
    auto insert_ms = std::chrono::duration_cast<std::chrono::milliseconds>(insert_time).count();
    auto query_ms = std::chrono::duration_cast<std::chrono::milliseconds>(query_time).count();
    
    EXPECT_LT(insert_ms / num_rows, 10) 
        << "Insert performance too slow: " << insert_ms / num_rows << "ms per row";
    EXPECT_LT(query_ms, 100) 
        << "Query performance too slow: " << query_ms << "ms for 100 rows";
    
    scratchbird::close_database(db);
}

// Test 7: Indexes must actually improve performance
TEST_F(CoreDatabaseTest, IndexesImprovePerformance) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(db_path.string(), {}, status);
    auto session = scratchbird::create_session(db, status);
    
    // Create table and insert data
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE perf_test (id INTEGER PRIMARY KEY, value INTEGER, data TEXT)", status), {});
    
    auto insert = scratchbird::prepare(session,
        "INSERT INTO perf_test (id, value, data) VALUES (?, ?, ?)", status);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100000);
    
    for (int i = 0; i < 5000; i++) {
        scratchbird::execute(insert, {
            std::to_string(i),
            std::to_string(dis(gen)),
            "Some random data " + std::to_string(i)
        });
    }
    
    // Measure query time without index
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; i++) {
        auto result = scratchbird::execute(scratchbird::prepare(session,
            "SELECT * FROM perf_test WHERE value = ?", status), {std::to_string(dis(gen))});
    }
    auto time_without_index = std::chrono::steady_clock::now() - start;
    
    // Create index
    ASSERT_EQ(scratchbird::execute(scratchbird::prepare(session,
        "CREATE INDEX idx_value ON perf_test(value)", status), {}).code, 
        scratchbird::StatusCode::Ok) << "Failed to create index";
    
    // Measure query time with index
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; i++) {
        auto result = scratchbird::execute(scratchbird::prepare(session,
            "SELECT * FROM perf_test WHERE value = ?", status), {std::to_string(dis(gen))});
    }
    auto time_with_index = std::chrono::steady_clock::now() - start;
    
    // Index must provide at least 2x speedup
    EXPECT_LT(time_with_index.count(), time_without_index.count() / 2) 
        << "Index didn't improve performance. Without: " << time_without_index.count() 
        << "ns, With: " << time_with_index.count() << "ns";
    
    scratchbird::close_database(db);
}