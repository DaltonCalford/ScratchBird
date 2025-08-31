/**
 * End-to-End Integration Tests
 * 
 * These tests verify that all components work together correctly in realistic scenarios.
 * They test complete workflows and ensure the database behaves like a real database system.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <sstream>
#include "scratchbird.h"
#include "scratchbird/server.h"
#include "scratchbird/engine.h"

namespace fs = std::filesystem;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "integration_test";
        fs::create_directories(test_dir);
    }
    
    void TearDown() override {
        fs::remove_all(test_dir);
    }
    
    fs::path test_dir;
};

// Test 1: Complete database lifecycle
TEST_F(IntegrationTest, CompleteDatabaseLifecycle) {
    fs::path db_path = test_dir / "lifecycle.db";
    
    // Phase 1: Create and populate database
    {
        scratchbird::Status status;
        scratchbird::CreateDbOptions opts;
        opts.page_size = 8192;
        opts.default_charset = "UTF8";
        
        auto db = scratchbird::create_database(db_path.string(), opts, status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok)
            << "Failed to create database: " << status.message;
        
        auto session = scratchbird::create_session(db, status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
        
        // Create schema
        std::vector<std::string> ddl_statements = {
            "CREATE TABLE users (id INTEGER PRIMARY KEY, username TEXT UNIQUE NOT NULL, "
            "email TEXT NOT NULL, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",
            
            "CREATE TABLE posts (id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL, "
            "title TEXT NOT NULL, content TEXT, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
            "FOREIGN KEY (user_id) REFERENCES users(id))",
            
            "CREATE INDEX idx_posts_user ON posts(user_id)",
            "CREATE INDEX idx_posts_created ON posts(created_at)",
            
            "CREATE VIEW recent_posts AS "
            "SELECT p.*, u.username FROM posts p "
            "JOIN users u ON p.user_id = u.id "
            "WHERE p.created_at > datetime('now', '-7 days')"
        };
        
        for (const auto& ddl : ddl_statements) {
            auto result = scratchbird::execute(
                scratchbird::prepare(session, ddl, status), {});
            ASSERT_EQ(result.code, scratchbird::StatusCode::Ok)
                << "Failed to execute: " << ddl << " - " << result.message;
        }
        
        // Insert test data
        auto insert_user = scratchbird::prepare(session,
            "INSERT INTO users (id, username, email) VALUES (?, ?, ?)", status);
        
        for (int i = 1; i <= 10; i++) {
            auto result = scratchbird::execute(insert_user, {
                std::to_string(i),
                "user" + std::to_string(i),
                "user" + std::to_string(i) + "@example.com"
            });
            ASSERT_EQ(result.code, scratchbird::StatusCode::Ok);
        }
        
        auto insert_post = scratchbird::prepare(session,
            "INSERT INTO posts (user_id, title, content) VALUES (?, ?, ?)", status);
        
        for (int i = 1; i <= 50; i++) {
            auto result = scratchbird::execute(insert_post, {
                std::to_string((i % 10) + 1),
                "Post " + std::to_string(i),
                "Content of post " + std::to_string(i)
            });
            ASSERT_EQ(result.code, scratchbird::StatusCode::Ok);
        }
        
        scratchbird::close_database(db);
    }
    
    // Phase 2: Reopen and query
    {
        scratchbird::Status status;
        auto db = scratchbird::open_database(db_path.string(), status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok)
            << "Failed to reopen database";
        
        auto session = scratchbird::create_session(db, status);
        
        // Test complex query
        auto complex_query = scratchbird::prepare(session,
            "SELECT u.username, COUNT(p.id) as post_count, "
            "MAX(p.created_at) as last_post "
            "FROM users u "
            "LEFT JOIN posts p ON u.id = p.user_id "
            "GROUP BY u.id, u.username "
            "HAVING COUNT(p.id) > 3 "
            "ORDER BY post_count DESC", status);
        
        auto result = scratchbird::execute(complex_query, {});
        ASSERT_EQ(result.code, scratchbird::StatusCode::Ok);
        ASSERT_FALSE(result.rows.empty())
            << "Complex query returned no results";
        
        // Verify data integrity
        for (const auto& row : result.rows) {
            int post_count = std::stoi(row.at("post_count"));
            EXPECT_GT(post_count, 3)
                << "HAVING clause not working correctly";
        }
        
        // Test view
        auto view_result = scratchbird::execute(
            scratchbird::prepare(session, "SELECT * FROM recent_posts", status), {});
        ASSERT_EQ(view_result.code, scratchbird::StatusCode::Ok)
            << "View query failed";
        
        scratchbird::close_database(db);
    }
    
    // Phase 3: Backup and restore
    {
        fs::path backup_path = test_dir / "backup.db";
        
        scratchbird::Status status;
        bool backup_success = scratchbird::backup_database(
            db_path.string(), backup_path.string(), status);
        ASSERT_TRUE(backup_success)
            << "Database backup failed: " << status.message;
        
        // Verify backup is valid
        auto backup_db = scratchbird::open_database(backup_path.string(), status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok)
            << "Cannot open backup database";
        
        auto session = scratchbird::create_session(backup_db, status);
        auto result = scratchbird::execute(
            scratchbird::prepare(session, "SELECT COUNT(*) as cnt FROM users", status), {});
        ASSERT_EQ(result.rows[0]["cnt"], "10")
            << "Backup doesn't contain correct data";
        
        scratchbird::close_database(backup_db);
    }
}

// Test 2: Client-Server mode
TEST_F(IntegrationTest, ClientServerMode) {
    fs::path db_path = test_dir / "server.db";
    
    // Create database
    scratchbird::Status status;
    auto db = scratchbird::create_database(db_path.string(), {}, status);
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    scratchbird::close_database(db);
    
    // Start server in background thread
    scratchbird::Server server;
    server.set_database_path(db_path.string());
    
    std::thread server_thread([&server]() {
        bool started = server.start("127.0.0.1", 15432);
        EXPECT_TRUE(started) << "Server failed to start";
    });
    
    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Connect as client
    scratchbird::ClientConnection client;
    bool connected = client.connect("127.0.0.1", 15432, "user", "password");
    ASSERT_TRUE(connected) << "Failed to connect to server";
    
    // Execute commands via client
    auto result = client.execute("CREATE TABLE test (id INTEGER, data TEXT)");
    EXPECT_TRUE(result.success) << "Failed to create table via client";
    
    result = client.execute("INSERT INTO test VALUES (1, 'Hello'), (2, 'World')");
    EXPECT_TRUE(result.success) << "Failed to insert via client";
    
    result = client.execute("SELECT * FROM test ORDER BY id");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.rows.size(), 2);
    EXPECT_EQ(result.rows[0]["data"], "Hello");
    EXPECT_EQ(result.rows[1]["data"], "World");
    
    // Test prepared statements via client
    auto stmt_id = client.prepare("INSERT INTO test VALUES (?, ?)");
    EXPECT_GT(stmt_id, 0) << "Failed to prepare statement";
    
    result = client.execute_prepared(stmt_id, {"3", "Prepared"});
    EXPECT_TRUE(result.success);
    
    // Test transactions via client
    EXPECT_TRUE(client.begin_transaction());
    client.execute("INSERT INTO test VALUES (4, 'Transaction')");
    EXPECT_TRUE(client.rollback());
    
    result = client.execute("SELECT COUNT(*) as cnt FROM test");
    EXPECT_EQ(result.rows[0]["cnt"], "3") 
        << "Rollback didn't work - transaction not atomic";
    
    // Disconnect
    client.disconnect();
    
    // Stop server
    server.stop();
    server_thread.join();
}

// Test 3: SQL compliance test suite
TEST_F(IntegrationTest, SQLComplianceTestSuite) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(test_dir / "sql_test.db", {}, status);
    auto session = scratchbird::create_session(db, status);
    
    // Test various SQL features
    struct SQLTest {
        std::string name;
        std::string sql;
        bool should_succeed;
        std::function<void(const scratchbird::Result&)> validator;
    };
    
    std::vector<SQLTest> tests = {
        // DDL Tests
        {"CREATE TABLE", "CREATE TABLE t1 (a INT, b TEXT)", true, nullptr},
        {"ALTER TABLE ADD", "ALTER TABLE t1 ADD COLUMN c REAL", true, nullptr},
        {"CREATE UNIQUE INDEX", "CREATE UNIQUE INDEX idx1 ON t1(a)", true, nullptr},
        
        // DML Tests
        {"INSERT", "INSERT INTO t1 (a, b, c) VALUES (1, 'test', 3.14)", true, nullptr},
        {"UPDATE", "UPDATE t1 SET b = 'updated' WHERE a = 1", true, nullptr},
        {"DELETE", "DELETE FROM t1 WHERE a > 10", true, nullptr},
        
        // Subqueries
        {"Scalar subquery", 
         "SELECT (SELECT COUNT(*) FROM t1) as cnt", true,
         [](const auto& r) { EXPECT_EQ(r.rows.size(), 1); }},
        
        {"IN subquery",
         "SELECT * FROM t1 WHERE a IN (SELECT a FROM t1 WHERE c > 2)", true, nullptr},
        
        // Joins
        {"INNER JOIN", 
         "SELECT * FROM t1 INNER JOIN t1 t2 ON t1.a = t2.a", true, nullptr},
        
        {"LEFT JOIN",
         "SELECT * FROM t1 LEFT JOIN t1 t2 ON t1.a = t2.a", true, nullptr},
        
        // Aggregates
        {"GROUP BY",
         "SELECT b, COUNT(*), AVG(c) FROM t1 GROUP BY b", true, nullptr},
        
        {"HAVING",
         "SELECT b, COUNT(*) as cnt FROM t1 GROUP BY b HAVING COUNT(*) > 1", true, nullptr},
        
        // Window functions (if supported)
        {"Window function",
         "SELECT a, ROW_NUMBER() OVER (ORDER BY a) as rn FROM t1", true, nullptr},
        
        // CTEs
        {"WITH clause",
         "WITH cte AS (SELECT * FROM t1) SELECT * FROM cte", true, nullptr},
        
        // Constraints
        {"CHECK constraint",
         "CREATE TABLE t2 (a INT CHECK (a > 0))", true, nullptr},
        
        {"Foreign key",
         "CREATE TABLE t3 (id INT PRIMARY KEY, t1_a INT REFERENCES t1(a))", true, nullptr},
    };
    
    for (const auto& test : tests) {
        auto result = scratchbird::execute(
            scratchbird::prepare(session, test.sql, status), {});
        
        if (test.should_succeed) {
            EXPECT_EQ(result.code, scratchbird::StatusCode::Ok)
                << "Test '" << test.name << "' failed: " << result.message;
            
            if (test.validator) {
                test.validator(result);
            }
        } else {
            EXPECT_NE(result.code, scratchbird::StatusCode::Ok)
                << "Test '" << test.name << "' should have failed but succeeded";
        }
    }
}

// Test 4: isql tool functionality
TEST_F(IntegrationTest, ISQLToolFunctionality) {
    fs::path db_path = test_dir / "isql_test.db";
    
    // Create database
    scratchbird::Status status;
    auto db = scratchbird::create_database(db_path.string(), {}, status);
    scratchbird::close_database(db);
    
    // Test isql executable exists and works
    std::string isql_path = "/workspace/build/tools/isql/isql";
    
    ASSERT_TRUE(fs::exists(isql_path))
        << "isql tool not built - critical tool missing!";
    
    // Test basic isql commands via pipe
    std::stringstream commands;
    commands << "CREATE TABLE test (id INT, name TEXT);\n";
    commands << "INSERT INTO test VALUES (1, 'Alice'), (2, 'Bob');\n";
    commands << "SELECT * FROM test;\n";
    commands << ".tables\n";
    commands << ".schema test\n";
    commands << ".quit\n";
    
    std::string cmd = "echo '" + commands.str() + "' | " + 
                     isql_path + " " + db_path.string();
    
    FILE* pipe = popen(cmd.c_str(), "r");
    ASSERT_NE(pipe, nullptr) << "Failed to run isql";
    
    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int exit_code = pclose(pipe);
    EXPECT_EQ(exit_code, 0) << "isql exited with error";
    
    // Verify output contains expected results
    EXPECT_TRUE(output.find("Alice") != std::string::npos)
        << "isql didn't return query results";
    EXPECT_TRUE(output.find("Bob") != std::string::npos)
        << "isql didn't return all results";
    EXPECT_TRUE(output.find("test") != std::string::npos)
        << ".tables command didn't work";
    EXPECT_TRUE(output.find("CREATE TABLE") != std::string::npos)
        << ".schema command didn't work";
}

// Test 5: Performance benchmarks meet requirements
TEST_F(IntegrationTest, PerformanceBenchmarks) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(test_dir / "perf.db", {}, status);
    auto session = scratchbird::create_session(db, status);
    
    // Create test table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE perf_test (id INTEGER PRIMARY KEY, "
        "col1 INTEGER, col2 TEXT, col3 REAL)", status), {});
    
    // Benchmark 1: Insert performance
    auto start = std::chrono::high_resolution_clock::now();
    
    auto insert = scratchbird::prepare(session,
        "INSERT INTO perf_test VALUES (?, ?, ?, ?)", status);
    
    const int num_inserts = 10000;
    for (int i = 0; i < num_inserts; i++) {
        scratchbird::execute(insert, {
            std::to_string(i),
            std::to_string(i * 2),
            "Text " + std::to_string(i),
            std::to_string(i * 3.14)
        });
    }
    
    auto insert_time = std::chrono::high_resolution_clock::now() - start;
    double inserts_per_sec = num_inserts / 
        std::chrono::duration<double>(insert_time).count();
    
    EXPECT_GT(inserts_per_sec, 1000)
        << "Insert performance too low: " << inserts_per_sec << " inserts/sec";
    
    // Benchmark 2: Index creation
    start = std::chrono::high_resolution_clock::now();
    
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE INDEX idx_col1 ON perf_test(col1)", status), {});
    
    auto index_time = std::chrono::high_resolution_clock::now() - start;
    double index_ms = std::chrono::duration<double, std::milli>(index_time).count();
    
    EXPECT_LT(index_ms, 5000)
        << "Index creation too slow: " << index_ms << "ms for " << num_inserts << " rows";
    
    // Benchmark 3: Query performance with index
    start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; i++) {
        int random_value = rand() % num_inserts;
        auto result = scratchbird::execute(scratchbird::prepare(session,
            "SELECT * FROM perf_test WHERE col1 = ?", status),
            {std::to_string(random_value * 2)});
    }
    
    auto query_time = std::chrono::high_resolution_clock::now() - start;
    double queries_per_sec = 1000 / 
        std::chrono::duration<double>(query_time).count();
    
    EXPECT_GT(queries_per_sec, 10000)
        << "Query performance too low: " << queries_per_sec << " queries/sec";
    
    // Benchmark 4: Join performance
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE perf_test2 AS SELECT * FROM perf_test", status), {});
    
    start = std::chrono::high_resolution_clock::now();
    
    auto join_result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT COUNT(*) FROM perf_test t1 "
        "JOIN perf_test2 t2 ON t1.col1 = t2.col1 "
        "WHERE t1.col3 > 1000", status), {});
    
    auto join_time = std::chrono::high_resolution_clock::now() - start;
    double join_ms = std::chrono::duration<double, std::milli>(join_time).count();
    
    EXPECT_LT(join_ms, 1000)
        << "Join too slow: " << join_ms << "ms for " << num_inserts << " x " << num_inserts;
    
    scratchbird::close_database(db);
}

// Test 6: Error handling and recovery
TEST_F(IntegrationTest, ErrorHandlingAndRecovery) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(test_dir / "error.db", {}, status);
    auto session = scratchbird::create_session(db, status);
    
    // Test various error conditions
    
    // 1. Syntax errors
    auto result = scratchbird::execute(
        scratchbird::prepare(session, "SELCT * FROM nowhere", status), {});
    EXPECT_NE(result.code, scratchbird::StatusCode::Ok);
    EXPECT_TRUE(result.message.find("syntax") != std::string::npos ||
                result.message.find("SELCT") != std::string::npos)
        << "Syntax error not properly reported";
    
    // 2. Constraint violations
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE test (id INTEGER PRIMARY KEY, val INTEGER NOT NULL)", status), {});
    
    result = scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO test (id, val) VALUES (1, NULL)", status), {});
    EXPECT_NE(result.code, scratchbird::StatusCode::Ok);
    EXPECT_TRUE(result.message.find("NULL") != std::string::npos ||
                result.message.find("constraint") != std::string::npos)
        << "NULL constraint violation not reported";
    
    // 3. Unique violations
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO test VALUES (1, 100)", status), {});
    
    result = scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO test VALUES (1, 200)", status), {});
    EXPECT_NE(result.code, scratchbird::StatusCode::Ok);
    EXPECT_TRUE(result.message.find("unique") != std::string::npos ||
                result.message.find("primary") != std::string::npos)
        << "Unique constraint violation not reported";
    
    // 4. Foreign key violations
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE parent (id INTEGER PRIMARY KEY)", status), {});
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE child (id INTEGER, parent_id INTEGER REFERENCES parent(id))", status), {});
    
    result = scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO child VALUES (1, 999)", status), {});
    EXPECT_NE(result.code, scratchbird::StatusCode::Ok);
    EXPECT_TRUE(result.message.find("foreign") != std::string::npos ||
                result.message.find("reference") != std::string::npos)
        << "Foreign key violation not reported";
    
    // 5. Division by zero
    result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT 1 / 0", status), {});
    EXPECT_NE(result.code, scratchbird::StatusCode::Ok);
    EXPECT_TRUE(result.message.find("zero") != std::string::npos ||
                result.message.find("division") != std::string::npos)
        << "Division by zero not handled";
    
    // 6. Out of memory simulation (large result set)
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE large (id INTEGER, data TEXT)", status), {});
    
    // Try to create a massive cross join
    for (int i = 0; i < 1000; i++) {
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO large VALUES (?, ?)", status),
            {std::to_string(i), std::string(1000, 'X')});
    }
    
    // This should either succeed with proper memory management
    // or fail gracefully with OOM error
    result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT * FROM large l1, large l2, large l3", status), {});
    
    if (result.code != scratchbird::StatusCode::Ok) {
        EXPECT_TRUE(result.message.find("memory") != std::string::npos ||
                    result.message.find("large") != std::string::npos)
            << "Out of memory not handled gracefully";
    }
    
    // Database should still be functional after errors
    result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT 1 as test", status), {});
    EXPECT_EQ(result.code, scratchbird::StatusCode::Ok)
        << "Database not functional after error conditions";
    
    scratchbird::close_database(db);
}