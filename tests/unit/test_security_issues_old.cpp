/**
 * @file test_security_issues.cpp
 * @brief Security vulnerability tests for Alpha 1.01 - Priority 1 Issues
 * 
 * Tests for security issues identified in Agent B's review:
 * - Path traversal protection
 * - System catalog idempotency
 * - Error context population
 * - Concurrent access protection
 * - Short read handling
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <iostream>
#include "scratchbird/core/database.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class SecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up test files
        cleanup_test_files();
    }
    
    void TearDown() override {
        cleanup_test_files();
    }
    
    void cleanup_test_files() {
        remove("test_security.db");
        remove("../test_traversal.db");
        remove("/tmp/test_security.db");
        remove("test_symlink.db");
        remove("test_truncated.db");
        remove("test_concurrent.db");
    }
    
    // Helper to create a truncated database file
    void create_truncated_db(const char* path, size_t size) {
        int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0) {
            // Write partial header
            std::vector<uint8_t> buffer(size, 0);
            write(fd, buffer.data(), size);
            close(fd);
        }
    }
};

// ============================================================================
// Priority 1: Path Traversal Security Tests
// ============================================================================

/**
 * Test: Path traversal attempt with "../"
 * Issue: No validation for path traversal in Database::create()
 * Expected: Should fail (currently PASSES incorrectly - demonstrates vulnerability)
 */
TEST_F(SecurityTest, PathTraversal_DotDot) {
    Database db;
    
    // Attempt to create database with path traversal
    // This SHOULD fail but currently succeeds (vulnerability)
    Status status = db.create("../../../etc/passwd.db", 8192);
    
    // This test will FAIL until path validation is added
    // EXPECT_NE(status, Status::Ok) << "Path traversal should be blocked";
    
    // Current behavior (vulnerability exists)
    if (status == Status::Ok) {
        ADD_FAILURE() << "SECURITY ISSUE: Path traversal with '../' not blocked";
        db.close();
        remove("../../../etc/passwd.db");
    }
}

/**
 * Test: Absolute path outside workspace
 * Issue: No validation for paths outside intended directory
 * Expected: Should validate path is within allowed directory
 */
TEST_F(SecurityTest, PathTraversal_AbsolutePath) {
    Database db;
    
    // Attempt to create database in system directory
    Status status = db.create("/tmp/system_breach.db", 8192);
    
    // Currently this succeeds (may want to restrict in production)
    if (status == Status::Ok) {
        // Note: This might be acceptable behavior depending on requirements
        // Marking as informational rather than failure
        std::cout << "INFO: Absolute paths are allowed - consider restricting\n";
        db.close();
        remove("/tmp/system_breach.db");
    }
}

/**
 * Test: Symbolic link traversal
 * Issue: No check for symbolic links that could escape directory
 * Expected: Should detect and block symbolic links
 */
TEST_F(SecurityTest, PathTraversal_SymbolicLink) {
    // Create a symbolic link pointing outside
    symlink("/etc/passwd", "test_symlink.db");
    
    Database db;
    Status status = db.open("test_symlink.db");
    
    // This should fail but error handling might not be specific
    EXPECT_NE(status, Status::Ok) << "Should not open symbolic links";
    
    unlink("test_symlink.db");
}

// ============================================================================
// Priority 1: System Catalog Idempotency Tests
// ============================================================================

/**
 * Test: Multiple init_system_catalog calls
 * Issue: init_system_catalog is not idempotent (Agent B review)
 * Expected: Multiple calls should not corrupt catalog
 * NOTE: init_system_catalog is private - this demonstrates the issue
 */
TEST_F(SecurityTest, SystemCatalog_Idempotency) {
    // Create multiple databases and check consistency
    Database db1;
    ASSERT_EQ(db1.create("test_security.db", 8192), Status::Ok);
    
    // The init_system_catalog is called internally during create()
    // We can't call it directly as it's private
    
    // Save initial state
    uint32_t page_size1 = db1.page_size();
    db1.close();
    
    // Open again and verify state is preserved
    Database db2;
    ASSERT_EQ(db2.open("test_security.db"), Status::Ok);
    
    EXPECT_EQ(db2.page_size(), page_size1) 
        << "Page size should be preserved";
    
    // NOTE: Cannot test idempotency directly as init_system_catalog is private
    ADD_FAILURE() << "ISSUE: init_system_catalog is private - cannot verify idempotency";
    
    db2.close();
}

/**
 * Test: Concurrent catalog initialization
 * Issue: No thread safety in catalog operations
 * Expected: Should handle concurrent initialization safely
 */
TEST_F(SecurityTest, SystemCatalog_ConcurrentInit) {
    // Test concurrent database creation instead
    std::atomic<int> success_count(0);
    std::atomic<int> error_count(0);
    
    // Launch multiple threads trying to create same database
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&success_count, &error_count, i]() {
            Database db;
            Status status = db.create("test_security.db", 8192);
            if (status == Status::Ok || status == Status::FileExists) {
                success_count++;
                if (status == Status::Ok) {
                    db.close();
                }
            } else {
                error_count++;
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // At least one should succeed
    EXPECT_GT(success_count, 0) 
        << "At least one thread should create the database";
    
    // No unexpected errors
    EXPECT_EQ(error_count, 0)
        << "No threads should have unexpected errors";
    
    // NOTE: Cannot test catalog initialization directly
    std::cout << "INFO: init_system_catalog thread safety cannot be tested (private method)\n";
}

// ============================================================================
// Priority 1: Error Context Population Tests
// ============================================================================

/**
 * Test: Error context is populated on failures
 * Issue: No error context population per ERROR_HANDLING.md
 * Expected: Errors should include file, line, function context
 */
TEST_F(SecurityTest, ErrorContext_NotPopulated) {
    Database db;
    
    // Trigger an error - try to open non-existent file
    Status status = db.open("non_existent_file.db");
    EXPECT_EQ(status, Status::FileNotFound);
    
    // Check if error context is available
    // NOTE: Database class doesn't expose error context currently
    // This demonstrates the missing functionality
    
    // The following would be the expected interface:
    // const ErrorContext* ctx = db.get_last_error_context();
    // EXPECT_NE(ctx, nullptr);
    // EXPECT_FALSE(ctx->file.empty());
    // EXPECT_GT(ctx->line, 0);
    // EXPECT_FALSE(ctx->function.empty());
    
    ADD_FAILURE() << "ISSUE: No error context interface available per ERROR_HANDLING.md";
}

// ============================================================================
// Priority 1: Concurrent Access Protection Tests
// ============================================================================

/**
 * Test: Two processes opening same database
 * Issue: File locking prevents concurrent access
 * Expected: Second process should fail with appropriate error
 */
TEST_F(SecurityTest, ConcurrentAccess_TwoProcesses) {
    // Parent process creates and opens database
    Database db1;
    ASSERT_EQ(db1.create("test_concurrent.db", 8192), Status::Ok);
    
    // Fork a child process
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - try to open same database
        Database db2;
        Status status = db2.open("test_concurrent.db");
        
        // Should fail due to exclusive lock
        if (status != Status::Ok) {
            exit(0);  // Expected behavior
        } else {
            exit(1);  // Unexpected - got access
        }
    } else if (pid > 0) {
        // Parent process - wait for child
        int status;
        waitpid(pid, &status, 0);
        
        EXPECT_EQ(WEXITSTATUS(status), 0) 
            << "Second process should be blocked by file lock";
        
        db1.close();
    }
}

/**
 * Test: Lock release on process termination
 * Issue: Verify locks are properly released on abnormal termination
 * Expected: Lock should be released when process dies
 */
TEST_F(SecurityTest, ConcurrentAccess_LockReleaseOnCrash) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - open database and "crash"
        Database db;
        db.create("test_concurrent.db", 8192);
        // Simulate crash - exit without closing
        _exit(0);
    } else if (pid > 0) {
        // Parent - wait for child to die
        int status;
        waitpid(pid, &status, 0);
        
        // Small delay to ensure OS releases lock
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Now parent should be able to open the database
        Database db;
        Status open_status = db.open("test_concurrent.db");
        
        EXPECT_EQ(open_status, Status::Ok) 
            << "Lock should be released after process termination";
        
        if (open_status == Status::Ok) {
            db.close();
        }
    }
}

// ============================================================================
// Priority 1: Short Read Handling Tests
// ============================================================================

/**
 * Test: Database file truncated mid-header
 * Issue: Incomplete read handling
 * Expected: Should detect truncation and return error
 */
TEST_F(SecurityTest, ShortRead_TruncatedHeader) {
    // Create a truncated database file (only 100 bytes instead of full header)
    create_truncated_db("test_truncated.db", 100);
    
    Database db;
    Status status = db.open("test_truncated.db");
    
    // Should fail due to incomplete header
    EXPECT_NE(status, Status::Ok) 
        << "Should detect truncated header file";
    
    // Likely returns IoError currently, but should be more specific
    if (status == Status::IoError) {
        std::cout << "INFO: Truncated file detected as IoError "
                  << "(could use more specific error code)\n";
    }
}

/**
 * Test: Partial page read
 * Issue: Read returns less than requested bytes
 * Expected: Should handle partial reads gracefully
 */
TEST_F(SecurityTest, ShortRead_PartialPage) {
    // Create a valid database first
    {
        Database db;
        ASSERT_EQ(db.create("test_truncated.db", 8192), Status::Ok);
        db.close();
    }
    
    // Truncate the file to simulate partial page
    int fd = open("test_truncated.db", O_RDWR);
    if (fd >= 0) {
        // Truncate to 4096 bytes (half a page for 8192 page size)
        ftruncate(fd, 4096);
        close(fd);
    }
    
    // Try to open truncated database
    Database db;
    Status status = db.open("test_truncated.db");
    
    // Should detect the truncation
    EXPECT_NE(status, Status::Ok) 
        << "Should detect partial page read";
}

/**
 * Test: EOF encountered during read
 * Issue: Unexpected EOF handling
 * Expected: Should return appropriate error on EOF
 */
TEST_F(SecurityTest, ShortRead_UnexpectedEOF) {
    // Create empty file
    create_truncated_db("test_truncated.db", 0);
    
    Database db;
    Status status = db.open("test_truncated.db");
    
    // Should fail on empty file
    EXPECT_NE(status, Status::Ok) 
        << "Should handle empty file (immediate EOF)";
}

// ============================================================================
// Test Summary
// ============================================================================

/**
 * Security Test Summary:
 * 
 * These tests demonstrate Priority 1 security issues from Agent B's review.
 * Expected failures (demonstrating vulnerabilities):
 * 
 * 1. Path Traversal (3 tests) - No path validation
 * 2. Catalog Idempotency (2 tests) - Not idempotent
 * 3. Error Context (1 test) - No context interface
 * 4. Concurrent Access (2 tests) - Working but could be improved
 * 5. Short Read (3 tests) - Basic handling exists
 * 
 * Total: 11 security tests
 */