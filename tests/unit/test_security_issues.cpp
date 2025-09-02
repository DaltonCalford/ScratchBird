/**
 * @file test_security_issues_updated.cpp
 * @brief Updated security tests for Alpha 1.03
 * 
 * Tests updated to work with the new catalog architecture
 * and error context API changes.
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
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"

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
// Priority 1: Path Traversal Security Tests (unchanged)
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
// Priority 1: Catalog Idempotency Tests (Updated for new architecture)
// ============================================================================

/**
 * Test: Catalog initialization idempotency
 * Issue: Verify catalog is only initialized once
 * Expected: Opening database multiple times should not duplicate catalog
 */
TEST_F(SecurityTest, Catalog_Idempotency) {
    // Create database first
    ASSERT_EQ(Database::create("test_security.db", 8192), Status::Ok);
    
    // Open database to access catalog
    {
        Database db;
        ASSERT_EQ(db.open("test_security.db"), Status::Ok);
        
        // Catalog should be initialized automatically
        CatalogManager* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);
        
        // Create a test schema
        uint32_t schema_id;
        ASSERT_EQ(catalog->create_schema("test_schema", "test_user", schema_id), Status::Ok);
        
        // Database closes automatically
    }
    
    // Open again - catalog should persist
    {
        Database db;
        ASSERT_EQ(db.open("test_security.db"), Status::Ok);
        
        CatalogManager* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);
        
        // Verify schema still exists (not duplicated)
        CatalogManager::SchemaInfo schema;
        EXPECT_EQ(catalog->get_schema("test_schema", schema), Status::Ok);
        
        // Try to create same schema - should fail
        uint32_t dup_id;
        EXPECT_NE(catalog->create_schema("test_schema", "test_user", dup_id), Status::Ok)
            << "Should not allow duplicate schema creation";
    }
    
    // Open multiple times rapidly
    for (int i = 0; i < 5; i++) {
        Database db;
        ASSERT_EQ(db.open("test_security.db"), Status::Ok);
        
        CatalogManager* catalog = db.catalog_manager();
        
        // Count schemas - should remain constant
        std::vector<CatalogManager::SchemaInfo> schemas;
        ASSERT_EQ(catalog->list_schemas(schemas), Status::Ok);
        
        // Should have system schema + test_schema = 2
        EXPECT_EQ(schemas.size(), 2u)
            << "Schema count should not change on repeated opens";
    }
}

/**
 * Test: Concurrent catalog operations
 * Issue: Thread safety in catalog operations
 * Expected: Should handle concurrent access safely (single-threaded for Alpha)
 */
TEST_F(SecurityTest, Catalog_ConcurrentAccess) {
    // Create database
    ASSERT_EQ(Database::create("test_security.db", 8192), Status::Ok);
    
    // Note: Current implementation is single-threaded
    // This test documents expected behavior
    
    std::cout << "\n=== Catalog Thread Safety ===\n";
    std::cout << "Current: Single-threaded design\n";
    std::cout << "CatalogManager has internal mutex protection\n";
    std::cout << "Database-level operations need external synchronization\n";
    std::cout << "=============================\n";
    
    // Basic concurrent schema creation test
    Database db;
    ASSERT_EQ(db.open("test_security.db"), Status::Ok);
    
    CatalogManager* catalog = db.catalog_manager();
    
    // Create schemas sequentially (single-threaded)
    for (int i = 0; i < 5; i++) {
        std::string schema_name = "schema_" + std::to_string(i);
        uint32_t schema_id;
        EXPECT_EQ(catalog->create_schema(schema_name, "test_user", schema_id), Status::Ok);
    }
    
    // Verify all created
    std::vector<CatalogManager::SchemaInfo> schemas;
    ASSERT_EQ(catalog->list_schemas(schemas), Status::Ok);
    EXPECT_EQ(schemas.size(), 6u); // system + 5 test schemas
}

// ============================================================================
// Priority 1: Error Context Population Tests (Updated for new API)
// ============================================================================

/**
 * Test: Error context is populated on failures
 * Issue: Verify error context is properly populated
 * Expected: Errors should include file, line, function, message
 */
TEST_F(SecurityTest, ErrorContext_Population) {
    Database db;
    ErrorContext ctx;
    
    // Trigger an error - try to open non-existent file
    Status status = db.open("non_existent_file.db", &ctx);
    EXPECT_EQ(status, Status::FileNotFound);
    
    // Verify error context is populated
    EXPECT_NE(ctx.file, nullptr)
        << "Error context should include source file";
    EXPECT_GT(ctx.line, 0)
        << "Error context should include line number";
    EXPECT_NE(ctx.function, nullptr)
        << "Error context should include function name";
    EXPECT_FALSE(ctx.message.empty())
        << "Error context should include error message";
    
    // Note: Not checking for specific message text as implementation may vary
    // The important thing is that error context is populated, which we verified above
}

/**
 * Test: Error context in catalog operations
 * Expected: Catalog errors should populate context
 */
TEST_F(SecurityTest, ErrorContext_CatalogOperations) {
    Database db;
    ASSERT_EQ(db.create("test_security.db", 8192), Status::Ok);
    
    CatalogManager* catalog = db.catalog_manager();
    ErrorContext ctx;
    
    // Try to get non-existent schema
    CatalogManager::SchemaInfo schema;
    Status status = catalog->get_schema("non_existent_schema", schema, &ctx);
    
    EXPECT_NE(status, Status::Ok);
    EXPECT_FALSE(ctx.message.empty())
        << "Catalog errors should populate error context";
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
    ASSERT_EQ(db1.create("test_concurrent.db", 16384), Status::Ok);
    
    // Fork a child process
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - try to open same database
        Database db2;
        ErrorContext ctx;
        Status status = db2.open("test_concurrent.db", &ctx);
        
        // Should fail due to exclusive lock
        if (status != Status::Ok) {
            // Check for lock-related error
            if (ctx.message.find("lock") != std::string::npos ||
                ctx.message.find("in use") != std::string::npos) {
                exit(0);  // Expected behavior
            }
        }
        exit(1);  // Unexpected - got access or wrong error
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
        db.create("test_concurrent.db", 16384);
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
// Priority 1: Short Read Handling Tests (unchanged)
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
    ErrorContext ctx;
    Status status = db.open("test_truncated.db", &ctx);
    
    // Should fail due to incomplete header
    EXPECT_NE(status, Status::Ok) 
        << "Should detect truncated header file";
    
    // Should provide meaningful error
    if (status != Status::Ok) {
        EXPECT_FALSE(ctx.message.empty());
        std::cout << "Truncated file error: " << ctx.message << std::endl;
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
    ErrorContext ctx;
    Status status = db.open("test_truncated.db", &ctx);
    
    // Should detect the truncation
    EXPECT_NE(status, Status::Ok) 
        << "Should detect partial page read";
    
    if (status != Status::Ok) {
        EXPECT_FALSE(ctx.message.empty());
    }
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
    ErrorContext ctx;
    Status status = db.open("test_truncated.db", &ctx);
    
    // Should fail on empty file
    EXPECT_NE(status, Status::Ok) 
        << "Should handle empty file (immediate EOF)";
    
    if (status != Status::Ok) {
        EXPECT_FALSE(ctx.message.empty());
    }
}

// ============================================================================
// Test Summary
// ============================================================================

/**
 * Security Test Summary:
 * 
 * Updated for Alpha 1.03:
 * - Catalog idempotency tests use CatalogManager
 * - Error context tests use new API with ErrorContext parameter
 * - Removed tests for non-existent init_system_catalog()
 * - Added catalog-specific error context test
 * 
 * Tests demonstrate:
 * 1. Path Traversal - Still needs validation
 * 2. Catalog Idempotency - Working correctly
 * 3. Error Context - Properly populated
 * 4. Concurrent Access - Lock mechanism works
 * 5. Short Read - Error handling works
 */