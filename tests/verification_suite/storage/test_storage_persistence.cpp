/**
 * Storage and Persistence Verification Tests
 * 
 * These tests verify that data is actually stored persistently and survives
 * crashes, power failures, and other failure scenarios.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "scratchbird/engine.h"
#include "scratchbird/engine/storage.h"
#include "scratchbird/engine/wal.h"

namespace fs = std::filesystem;

class StorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / ("storage_test_" + 
                   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(test_dir);
    }
    
    void TearDown() override {
        fs::remove_all(test_dir);
    }
    
    fs::path test_dir;
};

// Test 1: Write-Ahead Logging (WAL) must exist and work
TEST_F(StorageTest, WriteAheadLoggingWorks) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(test_dir / "wal_test.db", {}, status);
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    
    // WAL file should be created
    fs::path wal_path = test_dir / "wal_test.db.wal";
    EXPECT_TRUE(fs::exists(wal_path)) 
        << "WAL file not created - no crash recovery possible!";
    
    auto session = scratchbird::create_session(db, status);
    
    // Start transaction
    auto txn = scratchbird::begin_transaction(session, status);
    
    // Make changes
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE wal_test (id INTEGER, data TEXT)", status), {});
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO wal_test VALUES (1, 'Important data')", status), {});
    
    // WAL should contain uncommitted changes
    auto wal_size_before = fs::file_size(wal_path);
    EXPECT_GT(wal_size_before, 0) 
        << "WAL file is empty - changes not logged!";
    
    // Commit transaction
    scratchbird::commit(txn);
    
    // After checkpoint, WAL should be processed
    scratchbird::checkpoint(db);
    
    // Close and reopen
    scratchbird::close_database(db);
    
    // Verify WAL recovery works
    db = scratchbird::open_database(test_dir / "wal_test.db", status);
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    
    session = scratchbird::create_session(db, status);
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT * FROM wal_test WHERE id = 1", status), {});
    
    ASSERT_FALSE(result.rows.empty()) 
        << "Data lost - WAL recovery failed!";
    EXPECT_EQ(result.rows[0]["data"], "Important data");
    
    scratchbird::close_database(db);
}

// Test 2: Crash recovery simulation
TEST_F(StorageTest, CrashRecoveryWorks) {
    fs::path db_path = test_dir / "crash_test.db";
    
    // Fork a child process that will crash
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - simulate crash during write
        scratchbird::Status status;
        auto db = scratchbird::create_database(db_path.string(), {}, status);
        auto session = scratchbird::create_session(db, status);
        
        scratchbird::execute(scratchbird::prepare(session,
            "CREATE TABLE crash_test (id INTEGER, data TEXT)", status), {});
        
        // Insert some committed data
        auto txn1 = scratchbird::begin_transaction(session, status);
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO crash_test VALUES (1, 'Committed data')", status), {});
        scratchbird::commit(txn1);
        
        // Start another transaction but don't commit
        auto txn2 = scratchbird::begin_transaction(session, status);
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO crash_test VALUES (2, 'Uncommitted data')", status), {});
        
        // Simulate crash
        abort();  // Hard crash without cleanup
    }
    
    // Parent process - wait for child to crash
    int child_status;
    waitpid(pid, &child_status, 0);
    ASSERT_TRUE(WIFSIGNALED(child_status)) << "Child didn't crash as expected";
    
    // Now try to recover the database
    scratchbird::Status status;
    auto db = scratchbird::open_database(db_path.string(), status);
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok) 
        << "Failed to open database after crash";
    
    auto session = scratchbird::create_session(db, status);
    
    // Committed data should be present
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT * FROM crash_test WHERE id = 1", status), {});
    ASSERT_FALSE(result.rows.empty()) 
        << "Committed data lost after crash!";
    EXPECT_EQ(result.rows[0]["data"], "Committed data");
    
    // Uncommitted data should NOT be present
    result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT * FROM crash_test WHERE id = 2", status), {});
    EXPECT_TRUE(result.rows.empty()) 
        << "Uncommitted data persisted after crash - atomicity violated!";
    
    scratchbird::close_database(db);
}

// Test 3: Page-level consistency and checksums
TEST_F(StorageTest, PageConsistencyAndChecksums) {
    scratchbird::Status status;
    scratchbird::CreateDbOptions opts;
    opts.page_size = 4096;
    
    auto db = scratchbird::create_database(test_dir / "checksum_test.db", opts, status);
    auto session = scratchbird::create_session(db, status);
    
    // Create and populate table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE checksum_test (id INTEGER, data TEXT)", status), {});
    
    for (int i = 0; i < 100; i++) {
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO checksum_test VALUES (?, ?)", status), 
            {std::to_string(i), "Data " + std::to_string(i)});
    }
    
    scratchbird::close_database(db);
    
    // Now corrupt a page in the database file
    fs::path db_file = test_dir / "checksum_test.db.seg0";
    
    // Read file content
    std::vector<char> content;
    {
        std::ifstream file(db_file, std::ios::binary);
        content.assign(std::istreambuf_iterator<char>(file),
                      std::istreambuf_iterator<char>());
    }
    
    // Corrupt a page (skip header page)
    if (content.size() > 8192) {
        // Flip some bits in the second page
        for (int i = 4096; i < 4196 && i < content.size(); i++) {
            content[i] ^= 0xFF;
        }
        
        // Write corrupted content back
        std::ofstream file(db_file, std::ios::binary);
        file.write(content.data(), content.size());
    }
    
    // Try to open corrupted database
    db = scratchbird::open_database(test_dir / "checksum_test.db", status);
    
    // Should either fail to open or detect corruption on read
    if (status.code == scratchbird::StatusCode::Ok) {
        session = scratchbird::create_session(db, status);
        auto result = scratchbird::execute(scratchbird::prepare(session,
            "SELECT * FROM checksum_test", status), {});
        
        // Should detect corruption
        EXPECT_NE(result.code, scratchbird::StatusCode::Ok) 
            << "Corruption not detected - no checksum verification!";
        EXPECT_TRUE(result.message.find("corrupt") != std::string::npos ||
                   result.message.find("checksum") != std::string::npos)
            << "Error message doesn't indicate corruption detection";
    } else {
        // Opening should fail with corruption error
        EXPECT_TRUE(status.message.find("corrupt") != std::string::npos ||
                   status.message.find("checksum") != std::string::npos)
            << "Database opened despite corruption - checksums not working!";
    }
}

// Test 4: Storage space management and recycling
TEST_F(StorageTest, StorageSpaceManagement) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(test_dir / "space_test.db", {}, status);
    auto session = scratchbird::create_session(db, status);
    
    // Create table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE space_test (id INTEGER PRIMARY KEY, data TEXT)", status), {});
    
    // Insert large amount of data
    for (int i = 0; i < 1000; i++) {
        std::string large_data(1000, 'X');  // 1KB per row
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO space_test VALUES (?, ?)", status),
            {std::to_string(i), large_data});
    }
    
    // Check file size
    auto size_after_insert = fs::file_size(test_dir / "space_test.db.seg0");
    EXPECT_GT(size_after_insert, 1000 * 1000) 
        << "File size too small for inserted data";
    
    // Delete half the data
    scratchbird::execute(scratchbird::prepare(session,
        "DELETE FROM space_test WHERE id < 500", status), {});
    
    // Vacuum/compact the database
    scratchbird::execute(scratchbird::prepare(session,
        "VACUUM", status), {});
    
    // Check file size after vacuum
    auto size_after_vacuum = fs::file_size(test_dir / "space_test.db.seg0");
    EXPECT_LT(size_after_vacuum, size_after_insert * 0.7) 
        << "VACUUM didn't reclaim space - space management not working!";
    
    // Verify remaining data is intact
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT COUNT(*) as cnt FROM space_test", status), {});
    EXPECT_EQ(result.rows[0]["cnt"], "500") 
        << "Data corrupted during space reclamation";
    
    scratchbird::close_database(db);
}

// Test 5: Multi-version concurrency control (MVCC)
TEST_F(StorageTest, MVCCImplementation) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(test_dir / "mvcc_test.db", {}, status);
    
    // Create two sessions
    auto session1 = scratchbird::create_session(db, status);
    auto session2 = scratchbird::create_session(db, status);
    
    // Setup
    scratchbird::execute(scratchbird::prepare(session1,
        "CREATE TABLE mvcc_test (id INTEGER, value INTEGER)", status), {});
    scratchbird::execute(scratchbird::prepare(session1,
        "INSERT INTO mvcc_test VALUES (1, 100)", status), {});
    
    // Start transactions in both sessions
    auto txn1 = scratchbird::begin_transaction(session1, status);
    auto txn2 = scratchbird::begin_transaction(session2, status);
    
    // Session 1 updates the value
    scratchbird::execute(scratchbird::prepare(session1,
        "UPDATE mvcc_test SET value = 200 WHERE id = 1", status), {});
    
    // Session 2 should still see old value (MVCC)
    auto result2 = scratchbird::execute(scratchbird::prepare(session2,
        "SELECT value FROM mvcc_test WHERE id = 1", status), {});
    ASSERT_FALSE(result2.rows.empty());
    EXPECT_EQ(result2.rows[0]["value"], "100") 
        << "MVCC not working - uncommitted changes visible to other transaction!";
    
    // Commit session 1
    scratchbird::commit(txn1);
    
    // Session 2 with existing transaction should still see old value
    result2 = scratchbird::execute(scratchbird::prepare(session2,
        "SELECT value FROM mvcc_test WHERE id = 1", status), {});
    EXPECT_EQ(result2.rows[0]["value"], "100") 
        << "Read repeatability violated - value changed mid-transaction!";
    
    // Commit session 2 and start new transaction
    scratchbird::commit(txn2);
    txn2 = scratchbird::begin_transaction(session2, status);
    
    // Now session 2 should see new value
    result2 = scratchbird::execute(scratchbird::prepare(session2,
        "SELECT value FROM mvcc_test WHERE id = 1", status), {});
    EXPECT_EQ(result2.rows[0]["value"], "200") 
        << "Committed changes not visible in new transaction!";
    
    scratchbird::close_database(db);
}

// Test 6: Durability guarantees
TEST_F(StorageTest, DurabilityGuarantees) {
    scratchbird::Status status;
    scratchbird::CreateDbOptions opts;
    opts.sync_mode = scratchbird::SyncMode::Full;  // Highest durability
    
    auto db = scratchbird::create_database(test_dir / "durability_test.db", opts, status);
    auto session = scratchbird::create_session(db, status);
    
    // Create table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE durability_test (id INTEGER, data TEXT)", status), {});
    
    // Insert critical data with explicit sync
    auto txn = scratchbird::begin_transaction(session, status);
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO durability_test VALUES (1, 'Critical data')", status), {});
    
    // Commit should ensure data is on disk
    auto commit_status = scratchbird::commit(txn);
    ASSERT_EQ(commit_status.code, scratchbird::StatusCode::Ok);
    
    // Force OS to flush all buffers
    sync();
    
    // Simulate power failure by killing database without cleanup
    // Note: In real test, would use separate process
    scratchbird::close_database(db);
    
    // Clear OS caches (simulates cold start after power failure)
    // Note: Requires root - skip if not available
    system("echo 3 > /proc/sys/vm/drop_caches 2>/dev/null");
    
    // Reopen and verify data survived
    db = scratchbird::open_database(test_dir / "durability_test.db", status);
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    
    session = scratchbird::create_session(db, status);
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT * FROM durability_test WHERE id = 1", status), {});
    
    ASSERT_FALSE(result.rows.empty()) 
        << "Committed data lost - durability guarantee violated!";
    EXPECT_EQ(result.rows[0]["data"], "Critical data");
    
    scratchbird::close_database(db);
}

// Test 7: Large object (BLOB) storage
TEST_F(StorageTest, LargeObjectStorage) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(test_dir / "blob_test.db", {}, status);
    auto session = scratchbird::create_session(db, status);
    
    // Create table with BLOB column
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE blob_test (id INTEGER, data BLOB)", status), {});
    
    // Generate large binary data (10MB)
    std::vector<uint8_t> large_data(10 * 1024 * 1024);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (auto& byte : large_data) {
        byte = dis(gen);
    }
    
    // Insert large object
    auto insert_stmt = scratchbird::prepare(session,
        "INSERT INTO blob_test VALUES (1, ?)", status);
    ASSERT_EQ(scratchbird::execute_blob(insert_stmt, {large_data}).code, 
              scratchbird::StatusCode::Ok)
        << "Failed to insert large object";
    
    // Retrieve and verify
    auto select_stmt = scratchbird::prepare(session,
        "SELECT data FROM blob_test WHERE id = 1", status);
    auto result = scratchbird::execute_blob(select_stmt, {});
    
    ASSERT_FALSE(result.blobs.empty()) 
        << "BLOB data not retrieved";
    ASSERT_EQ(result.blobs[0].size(), large_data.size()) 
        << "BLOB size mismatch";
    
    // Verify content integrity
    bool data_matches = true;
    for (size_t i = 0; i < large_data.size(); i++) {
        if (result.blobs[0][i] != large_data[i]) {
            data_matches = false;
            break;
        }
    }
    EXPECT_TRUE(data_matches) 
        << "BLOB data corrupted during storage/retrieval";
    
    scratchbird::close_database(db);
}

// Test 8: Segment file management
TEST_F(StorageTest, SegmentFileManagement) {
    scratchbird::Status status;
    scratchbird::CreateDbOptions opts;
    opts.segment_size = 1024 * 1024;  // 1MB segments for testing
    
    auto db = scratchbird::create_database(test_dir / "segment_test.db", opts, status);
    auto session = scratchbird::create_session(db, status);
    
    // Create table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE segment_test (id INTEGER, data TEXT)", status), {});
    
    // Insert enough data to span multiple segments
    for (int i = 0; i < 10000; i++) {
        std::string data(1000, 'X');  // 1KB per row
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO segment_test VALUES (?, ?)", status),
            {std::to_string(i), data});
    }
    
    // Check for multiple segment files
    int segment_count = 0;
    for (int i = 0; i < 100; i++) {
        fs::path segment = test_dir / ("segment_test.db.seg" + std::to_string(i));
        if (fs::exists(segment)) {
            segment_count++;
        } else {
            break;
        }
    }
    
    EXPECT_GT(segment_count, 1) 
        << "Data didn't span multiple segments - segment management not working";
    
    // Verify data integrity across segments
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT COUNT(*) as cnt FROM segment_test", status), {});
    EXPECT_EQ(result.rows[0]["cnt"], "10000") 
        << "Data loss across segments";
    
    // Test segment cleanup after DELETE
    scratchbird::execute(scratchbird::prepare(session,
        "DELETE FROM segment_test WHERE id < 9000", status), {});
    scratchbird::execute(scratchbird::prepare(session, "VACUUM", status), {});
    
    // Some segments should be removed
    int segment_count_after = 0;
    for (int i = 0; i < 100; i++) {
        fs::path segment = test_dir / ("segment_test.db.seg" + std::to_string(i));
        if (fs::exists(segment)) {
            segment_count_after++;
        }
    }
    
    EXPECT_LT(segment_count_after, segment_count) 
        << "Unused segments not cleaned up";
    
    scratchbird::close_database(db);
}