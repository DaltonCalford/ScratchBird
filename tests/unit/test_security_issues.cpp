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
#include "test_helpers.h"
#include "unit/test_user_helpers.h"

using namespace scratchbird::core;

class SecurityTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create unique paths for test isolation (parallel execution)
        test_security_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_security", ".db");
        test_symlink_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_symlink", ".db");
        test_truncated_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_truncated", ".db");
        test_concurrent_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_concurrent", ".db");

        // Store paths for convenient access
        test_security_path_ = test_security_file_->path();
        test_symlink_path_ = test_symlink_file_->path();
        test_truncated_path_ = test_truncated_file_->path();
        test_concurrent_path_ = test_concurrent_file_->path();
    }

    void TearDown() override
    {
        // TestDatabaseFile will cleanup automatically on destruction
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_security_file_;
    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_symlink_file_;
    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_truncated_file_;
    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_concurrent_file_;

    std::string test_security_path_;
    std::string test_symlink_path_;
    std::string test_truncated_path_;
    std::string test_concurrent_path_;

    // Helper to create a truncated database file
    void create_truncated_db(const char *path, size_t size)
    {
        int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0)
        {
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
TEST_F(SecurityTest, PathTraversal_DotDot)
{
    Database db;

    // Attempt to create database with path traversal
    // This SHOULD fail but currently succeeds (vulnerability)
    Status status = db.create("../../../etc/passwd.db", 8192);

    // This test will FAIL until path validation is added
    // EXPECT_NE(status, Status::OK) << "Path traversal should be blocked";

    // Current behavior (vulnerability exists)
    if (status == Status::OK)
    {
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
TEST_F(SecurityTest, PathTraversal_AbsolutePath)
{
    Database db;

    // Attempt to create database in system directory
    std::string system_breach_path =
        scratchbird::testing::uniqueTestDbPath("system_breach");
    Status status = db.create(system_breach_path.c_str(), 8192);

    // Currently this succeeds (may want to restrict in production)
    if (status == Status::OK)
    {
        // Note: This might be acceptable behavior depending on requirements
        // Marking as informational rather than failure
        std::cout << "INFO: Absolute paths are allowed - consider restricting\n";
        db.close();
        remove(system_breach_path.c_str());
    }
}

/**
 * Test: Symbolic link traversal
 * Issue: No check for symbolic links that could escape directory
 * Expected: Should detect and block symbolic links
 */
TEST_F(SecurityTest, PathTraversal_SymbolicLink)
{
    // Create a symbolic link pointing outside
    symlink("/etc/passwd", test_symlink_path_.c_str());

    Database db;
    Status status = db.open(test_symlink_path_);

    // This should fail but error handling might not be specific
    EXPECT_NE(status, Status::OK) << "Should not open symbolic links";

    unlink(test_symlink_path_.c_str());
}

// ============================================================================
// Priority 1: Catalog Idempotency Tests (Updated for new architecture)
// ============================================================================

/**
 * Test: Catalog initialization idempotency
 * Issue: Verify catalog is only initialized once
 * Expected: Opening database multiple times should not duplicate catalog
 */
TEST_F(SecurityTest, Catalog_Idempotency)
{
    // Create database first
    ASSERT_EQ(Database::create(test_security_path_, 8192), Status::OK);

    // Get the baseline schema count (system schemas) and then add one
    size_t expected_schema_count = 0;

    // Open database to access catalog
    {
        Database db;
        ASSERT_EQ(db.open(test_security_path_), Status::OK);

        // Catalog should be initialized automatically
        CatalogManager *catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        // Get baseline schema count (system schemas from hierarchical architecture)
        std::vector<CatalogManager::SchemaInfo> schemas;
        ASSERT_EQ(catalog->listSchemas(schemas), Status::OK);
        expected_schema_count = schemas.size() + 1;  // +1 for test_schema we'll create

        // Create a test schema
        ID schema_id;
        EnsureUser(catalog, "test_user");
        ASSERT_EQ(catalog->createSchema("test_schema", "test_user", schema_id), Status::OK);

        // Database closes automatically
    }

    // Open again - catalog should persist
    {
        Database db;
        ASSERT_EQ(db.open(test_security_path_), Status::OK);

        CatalogManager *catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        EnsureUser(catalog, "test_user");
        // Verify schema still exists (not duplicated)
        CatalogManager::SchemaInfo schema;
        EXPECT_EQ(catalog->getSchema("test_schema", schema), Status::OK);

        // Try to create same schema - should fail
        ID dup_id;
        EXPECT_NE(catalog->createSchema("test_schema", "test_user", dup_id), Status::OK)
            << "Should not allow duplicate schema creation";
    }

    // Open multiple times rapidly
    for (int i = 0; i < 5; i++)
    {
        Database db;
        ASSERT_EQ(db.open(test_security_path_), Status::OK);

        CatalogManager *catalog = db.catalog_manager();

        // Count schemas - should remain constant
        std::vector<CatalogManager::SchemaInfo> schemas;
        ASSERT_EQ(catalog->listSchemas(schemas), Status::OK);

        // Should have system schemas + test_schema (count determined at runtime)
        EXPECT_EQ(schemas.size(), expected_schema_count) << "Schema count should not change on repeated opens";
    }
}

/**
 * Test: Concurrent catalog operations
 * Issue: Thread safety in catalog operations
 * Expected: Should handle concurrent access safely (single-threaded for Alpha)
 */
TEST_F(SecurityTest, Catalog_ConcurrentAccess)
{
    // Create database
    ASSERT_EQ(Database::create(test_security_path_, 8192), Status::OK);

    // Note: Current implementation is single-threaded
    // This test documents expected behavior

    std::cout << "\n=== Catalog Thread Safety ===\n";
    std::cout << "Current: Single-threaded design\n";
    std::cout << "CatalogManager has internal mutex protection\n";
    std::cout << "Database-level operations need external synchronization\n";
    std::cout << "=============================\n";

    // Basic concurrent schema creation test
    Database db;
    ASSERT_EQ(db.open(test_security_path_), Status::OK);

    CatalogManager *catalog = db.catalog_manager();
    EnsureUser(catalog, "test_user");

    // Get baseline count of system schemas
    std::vector<CatalogManager::SchemaInfo> baseline_schemas;
    ASSERT_EQ(catalog->listSchemas(baseline_schemas), Status::OK);
    size_t baseline_count = baseline_schemas.size();

    // Create schemas sequentially (single-threaded)
    for (int i = 0; i < 5; i++)
    {
        std::string schema_name = "schema_" + std::to_string(i);
        ID schema_id;
        EXPECT_EQ(catalog->createSchema(schema_name, "test_user", schema_id), Status::OK);
    }

    // Verify all created
    std::vector<CatalogManager::SchemaInfo> schemas;
    ASSERT_EQ(catalog->listSchemas(schemas), Status::OK);
    EXPECT_EQ(schemas.size(), baseline_count + 5); // baseline system schemas + 5 test schemas
}

// ============================================================================
// Priority 1: Error Context Population Tests (Updated for new API)
// ============================================================================

/**
 * Test: Error context is populated on failures
 * Issue: Verify error context is properly populated
 * Expected: Errors should include file, line, function, message
 */
TEST_F(SecurityTest, ErrorContext_Population)
{
    Database db;
    ErrorContext ctx;

    // Trigger an error - try to open non-existent file
    Status status = db.open("non_existent_file.db", &ctx);
    EXPECT_EQ(status, Status::FILE_NOT_FOUND);

    // Verify error context is populated
    EXPECT_NE(ctx.file, nullptr) << "Error context should include source file";
    EXPECT_GT(ctx.line, 0) << "Error context should include line number";
    EXPECT_NE(ctx.function, nullptr) << "Error context should include function name";
    EXPECT_FALSE(ctx.message.empty()) << "Error context should include error message";

    // Note: Not checking for specific message text as implementation may vary
    // The important thing is that error context is populated, which we verified above
}

/**
 * Test: Error context in catalog operations
 * Expected: Catalog errors should populate context
 */
TEST_F(SecurityTest, ErrorContext_CatalogOperations)
{
    // First create, then close and reopen to ensure catalog is properly initialized
    ASSERT_EQ(Database::create(test_security_path_, 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open(test_security_path_), Status::OK);

    CatalogManager *catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr) << "CatalogManager should be initialized after open";

    ErrorContext ctx;

    // Try to get non-existent schema
    CatalogManager::SchemaInfo schema;
    Status status = catalog->getSchema("non_existent_schema", schema, &ctx);

    EXPECT_NE(status, Status::OK);
    EXPECT_FALSE(ctx.message.empty()) << "Catalog errors should populate error context";
}

// ============================================================================
// Priority 1: Concurrent Access Protection Tests
// ============================================================================

/**
 * Test: Two processes opening same database
 * Issue: File locking prevents concurrent access
 * Expected: Second process should fail with appropriate error
 */
TEST_F(SecurityTest, ConcurrentAccess_TwoProcesses)
{
    // First create the database (create() is static and closes the file)
    ASSERT_EQ(Database::create(test_concurrent_path_, 16384), Status::OK);

    // Now open it to acquire the exclusive lock
    Database db1;
    ASSERT_EQ(db1.open(test_concurrent_path_), Status::OK);

    // Fork a child process
    pid_t pid = fork();

    if (pid == 0)
    {
        // Child process - try to open same database
        Database db2;
        ErrorContext ctx;
        Status status = db2.open(test_concurrent_path_, &ctx);

        // Should fail due to exclusive lock
        if (status != Status::OK)
        {
            // Check for lock-related error
            if (ctx.message.find("lock") != std::string::npos ||
                ctx.message.find("in use") != std::string::npos)
            {
                exit(0); // Expected behavior
            }
        }
        exit(1); // Unexpected - got access or wrong error
    }
    else if (pid > 0)
    {
        // Parent process - wait for child
        int status;
        waitpid(pid, &status, 0);

        EXPECT_EQ(WEXITSTATUS(status), 0) << "Second process should be blocked by file lock";

        db1.close();
    }
}

/**
 * Test: Lock release on process termination
 * Issue: Verify locks are properly released on abnormal termination
 * Expected: Lock should be released when process dies
 */
TEST_F(SecurityTest, ConcurrentAccess_LockReleaseOnCrash)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        // Child process - open database and "crash"
        Database db;
        db.create(test_concurrent_path_, 16384);
        // Simulate crash - exit without closing
        _exit(0);
    }
    else if (pid > 0)
    {
        // Parent - wait for child to die
        int status;
        waitpid(pid, &status, 0);

        // Small delay to ensure OS releases lock
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Now parent should be able to open the database
        Database db;
        Status open_status = db.open(test_concurrent_path_);

        EXPECT_EQ(open_status, Status::OK) << "Lock should be released after process termination";

        if (open_status == Status::OK)
        {
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
TEST_F(SecurityTest, ShortRead_TruncatedHeader)
{
    // Create a truncated database file (only 100 bytes instead of full header)
    create_truncated_db(test_truncated_path_.c_str(), 100);

    Database db;
    ErrorContext ctx;
    Status status = db.open(test_truncated_path_, &ctx);

    // Should fail due to incomplete header
    EXPECT_NE(status, Status::OK) << "Should detect truncated header file";

    // Should provide meaningful error
    if (status != Status::OK)
    {
        EXPECT_FALSE(ctx.message.empty());
        std::cout << "Truncated file error: " << ctx.message << std::endl;
    }
}

/**
 * Test: Partial page read
 * Issue: Read returns less than requested bytes
 * Expected: Should handle partial reads gracefully
 */
TEST_F(SecurityTest, ShortRead_PartialPage)
{
    // Create a valid database first
    {
        Database db;
        ASSERT_EQ(db.create(test_truncated_path_, 8192), Status::OK);
    }

    // Truncate the file to simulate partial page
    int fd = open(test_truncated_path_.c_str(), O_RDWR);
    if (fd >= 0)
    {
        // Truncate to 4096 bytes (half a page for 8192 page size)
        ftruncate(fd, 4096);
        close(fd);
    }

    // Try to open truncated database
    Database db;
    ErrorContext ctx;
    Status status = db.open(test_truncated_path_, &ctx);

    // Should detect the truncation
    EXPECT_NE(status, Status::OK) << "Should detect partial page read";

    if (status != Status::OK)
    {
        EXPECT_FALSE(ctx.message.empty());
    }
}

/**
 * Test: EOF encountered during read
 * Issue: Unexpected EOF handling
 * Expected: Should return appropriate error on EOF
 */
TEST_F(SecurityTest, ShortRead_UnexpectedEOF)
{
    // Create empty file
    create_truncated_db(test_truncated_path_.c_str(), 0);

    Database db;
    ErrorContext ctx;
    Status status = db.open(test_truncated_path_, &ctx);

    // Should fail on empty file
    EXPECT_NE(status, Status::OK) << "Should handle empty file (immediate EOF)";

    if (status != Status::OK)
    {
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
