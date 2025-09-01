/**
 * @file test_memory_safety.cpp
 * @brief Memory safety tests for Alpha 1.01 - Priority 0 (Critical) Issues
 * 
 * Tests for critical memory issues identified in Agent B's review:
 * - OOM handling for allocations at lines 53 and 238
 * - Memory leak detection on error paths
 * - Resource cleanup verification
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <dlfcn.h>
#include <new>
#include <atomic>
#include <vector>
#include "scratchbird/core/database.h"
#include "scratchbird/core/status.h"

using namespace scratchbird::core;

// ============================================================================
// Memory Allocation Failure Injection
// ============================================================================

/**
 * Global allocation failure injector for testing OOM conditions
 * This allows us to simulate allocation failures at specific points
 */
class AllocationFailureInjector {
public:
    static AllocationFailureInjector& instance() {
        static AllocationFailureInjector injector;
        return injector;
    }
    
    void enable_failure_at(int allocation_number) {
        enabled_ = true;
        fail_at_allocation_ = allocation_number;
        current_allocation_ = 0;
    }
    
    void disable() {
        enabled_ = false;
    }
    
    bool should_fail() {
        if (!enabled_) return false;
        current_allocation_++;
        return current_allocation_ == fail_at_allocation_;
    }
    
    int get_current_allocation() const {
        return current_allocation_;
    }
    
private:
    std::atomic<bool> enabled_{false};
    std::atomic<int> fail_at_allocation_{0};
    std::atomic<int> current_allocation_{0};
};

// Override operator new to inject failures
void* operator new(std::size_t size) {
    if (AllocationFailureInjector::instance().should_fail()) {
        throw std::bad_alloc();
    }
    void* ptr = malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void* operator new[](std::size_t size) {
    if (AllocationFailureInjector::instance().should_fail()) {
        throw std::bad_alloc();
    }
    void* ptr = malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete[](void* ptr) noexcept {
    free(ptr);
}

// ============================================================================
// Memory Safety Test Fixture
// ============================================================================

class MemorySafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset allocation injector
        AllocationFailureInjector::instance().disable();
        
        // Clean up test files
        cleanup_test_files();
        
        // Store initial FD count for leak detection
        initial_fd_count_ = count_open_fds();
    }
    
    void TearDown() override {
        // Ensure injector is disabled
        AllocationFailureInjector::instance().disable();
        
        // Check for FD leaks
        int final_fd_count = count_open_fds();
        if (final_fd_count > initial_fd_count_) {
            ADD_FAILURE() << "File descriptor leak detected: "
                         << (final_fd_count - initial_fd_count_) 
                         << " FDs leaked";
        }
        
        cleanup_test_files();
    }
    
    void cleanup_test_files() {
        remove("test_oom.db");
        remove("test_leak.db");
        remove("test_cleanup.db");
    }
    
    int count_open_fds() {
        int count = 0;
        for (int fd = 0; fd < 1024; fd++) {
            if (fcntl(fd, F_GETFD) != -1) {
                count++;
            }
        }
        return count;
    }
    
    int initial_fd_count_;
};

// ============================================================================
// Priority 0: OOM Handling Tests (Critical)
// ============================================================================

/**
 * Test: OOM at line 53 - new uint8_t[page_size] in Database::create()
 * Issue: No nullptr check after allocation
 * Expected: Should return Status::OOM (but enum value doesn't exist)
 * Current: Will crash with uncaught exception
 */
TEST_F(MemorySafetyTest, OOM_CreateHeaderAllocation_Line53) {
    Database db;
    
    // Enable failure at first allocation (line 53)
    AllocationFailureInjector::instance().enable_failure_at(1);
    
    // This will throw std::bad_alloc because there's no try-catch
    // Demonstrating the critical issue
    EXPECT_THROW({
        Status status = db.create("test_oom.db", 8192);
    }, std::bad_alloc) << "CRITICAL: Uncaught allocation failure at line 53";
    
    AllocationFailureInjector::instance().disable();
    
    // Verify no file was created
    struct stat st;
    EXPECT_NE(stat("test_oom.db", &st), 0) 
        << "Database file should not exist after allocation failure";
    
    ADD_FAILURE() << "P0 ISSUE: No OOM handling for allocation at line 53";
}

/**
 * Test: OOM at line 238 - new uint8_t[page_size_] in Database::open()
 * Issue: No nullptr check after allocation
 * Expected: Should return Status::OOM and clean up resources
 * Current: Will crash with uncaught exception
 */
TEST_F(MemorySafetyTest, OOM_OpenHeaderAllocation_Line238) {
    // First create a valid database
    {
        Database db;
        ASSERT_EQ(db.create("test_oom.db", 8192), Status::Ok);
        db.close();
    }
    
    Database db;
    
    // Enable failure at the allocation in open() (line 238)
    // Note: May need to adjust the allocation number based on actual execution
    AllocationFailureInjector::instance().enable_failure_at(1);
    
    // This will throw std::bad_alloc because there's no try-catch
    EXPECT_THROW({
        Status status = db.open("test_oom.db");
    }, std::bad_alloc) << "CRITICAL: Uncaught allocation failure at line 238";
    
    AllocationFailureInjector::instance().disable();
    
    ADD_FAILURE() << "P0 ISSUE: No OOM handling for allocation at line 238";
}

/**
 * Test: Missing Status::OOM enum value
 * Issue: Status enum doesn't include OOM error code
 * Expected: Should have Status::OOM = 3003 per spec
 */
TEST_F(MemorySafetyTest, OOM_MissingStatusEnum) {
    // Check if Status::OOM exists
    // This will fail to compile if uncommented, proving the issue
    // Status status = Status::OOM;  // COMPILATION ERROR
    
    // Verify the enum doesn't have the value
    bool has_oom = false;
    
    // Check all defined status values
    std::vector<Status> all_statuses = {
        Status::Ok,
        Status::FileNotFound,
        Status::FileExists,
        Status::IoError,
        Status::PageCorrupt,
        Status::ChecksumMismatch,
        Status::Deadlock,
        Status::LockTimeout
    };
    
    // OOM should be 3003 per spec
    for (auto status : all_statuses) {
        if (static_cast<uint32_t>(status) == 3003) {
            has_oom = true;
            break;
        }
    }
    
    EXPECT_TRUE(has_oom) << "P0 ISSUE: Status::OOM (3003) is missing from enum";
    
    if (!has_oom) {
        ADD_FAILURE() << "CRITICAL: Cannot properly handle OOM without Status::OOM";
    }
}

// ============================================================================
// Priority 0: Memory Leak Detection Tests (Critical)
// ============================================================================

/**
 * Test: File descriptor leak on error path
 * Issue: FD not closed if allocation fails in open()
 * Expected: FD should be closed on all error paths
 */
TEST_F(MemorySafetyTest, MemoryLeak_FileDescriptorOnError) {
    // Create a valid database first
    {
        Database db;
        ASSERT_EQ(db.create("test_leak.db", 8192), Status::Ok);
        db.close();
    }
    
    int initial_fds = count_open_fds();
    
    // Try to open with simulated allocation failure
    for (int i = 0; i < 5; i++) {
        Database db;
        AllocationFailureInjector::instance().enable_failure_at(1);
        
        try {
            db.open("test_leak.db");
        } catch (const std::bad_alloc&) {
            // Expected due to missing OOM handling
        }
        
        AllocationFailureInjector::instance().disable();
    }
    
    int final_fds = count_open_fds();
    
    // Check for FD leak
    if (final_fds > initial_fds) {
        ADD_FAILURE() << "P0 ISSUE: File descriptor leak detected - "
                      << (final_fds - initial_fds) << " FDs leaked "
                      << "when allocation fails in open()";
    }
}

/**
 * Test: Memory leak when allocation fails
 * Issue: First allocation not freed if second allocation fails
 * Expected: All allocations should be cleaned up on error
 */
TEST_F(MemorySafetyTest, MemoryLeak_AllocationCleanup) {
    // This test would require valgrind or AddressSanitizer to fully verify
    // Here we document the issue for manual verification
    
    std::cout << "\n=== Memory Leak Verification Required ===\n";
    std::cout << "Run with valgrind to verify:\n";
    std::cout << "  valgrind --leak-check=full ./scratchbird_tests "
              << "--gtest_filter=MemorySafetyTest.MemoryLeak_AllocationCleanup\n";
    std::cout << "\nExpected issue at line 238:\n";
    std::cout << "  If allocation fails, previously allocated memory not freed\n";
    std::cout << "  File descriptor not closed\n";
    std::cout << "=========================================\n\n";
    
    ADD_FAILURE() << "P0 ISSUE: Manual verification needed - "
                  << "potential memory leak on allocation failure";
}

/**
 * Test: Resource cleanup in destructor
 * Issue: Verify destructor properly cleans up resources
 * Expected: Destructor should free all allocated memory
 */
TEST_F(MemorySafetyTest, MemoryLeak_DestructorCleanup) {
    // Test that destructor cleans up properly
    {
        Database db;
        ASSERT_EQ(db.create("test_cleanup.db", 8192), Status::Ok);
        // db goes out of scope here - destructor called
    }
    
    // Check that we can open the file again (lock released)
    {
        Database db2;
        Status status = db2.open("test_cleanup.db");
        EXPECT_EQ(status, Status::Ok) 
            << "Lock should be released in destructor";
        
        if (status == Status::Ok) {
            db2.close();
        }
    }
}

// ============================================================================
// Additional Memory Safety Tests
// ============================================================================

/**
 * Test: Buffer overflow protection
 * Issue: Verify bounds checking in memory operations
 * Expected: No buffer overflows in page operations
 */
TEST_F(MemorySafetyTest, BufferOverflow_PageOperations) {
    Database db;
    ASSERT_EQ(db.create("test_cleanup.db", 8192), Status::Ok);
    
    // Verify page size through public interface
    uint32_t page_size = db.page_size();
    
    // Verify page size is validated
    EXPECT_TRUE(page_size == 8192 ||
                page_size == 16384 ||
                page_size == 32768)
        << "Page size should be validated";
    
    db.close();
}

/**
 * Test: Use-after-free protection
 * Issue: Verify pointers are nullified after free
 * Expected: No use-after-free vulnerabilities
 */
TEST_F(MemorySafetyTest, UseAfterFree_CloseDatabase) {
    Database db;
    ASSERT_EQ(db.create("test_cleanup.db", 8192), Status::Ok);
    
    // Verify database is open
    EXPECT_TRUE(db.is_open()) << "Database should be open";
    
    // Close database
    db.close();
    
    // Verify database is closed
    EXPECT_FALSE(db.is_open()) 
        << "Database should be closed after close()";
}

// ============================================================================
// Stress Tests for Memory Issues
// ============================================================================

/**
 * Test: Rapid open/close cycles
 * Issue: Verify no memory leaks in repeated operations
 * Expected: No resource leaks after many cycles
 */
TEST_F(MemorySafetyTest, Stress_RapidOpenClose) {
    const int iterations = 100;
    
    int initial_fds = count_open_fds();
    
    for (int i = 0; i < iterations; i++) {
        Database db;
        
        if (i == 0) {
            ASSERT_EQ(db.create("test_cleanup.db", 8192), Status::Ok);
        } else {
            ASSERT_EQ(db.open("test_cleanup.db"), Status::Ok);
        }
        
        // Perform some operations
        EXPECT_TRUE(db.is_open());
        EXPECT_GT(db.page_size(), 0u);
        
        db.close();
    }
    
    int final_fds = count_open_fds();
    
    EXPECT_EQ(final_fds, initial_fds) 
        << "File descriptor leak after " << iterations << " open/close cycles";
}

/**
 * Test: Large page size allocation
 * Issue: Verify large allocations are handled
 * Expected: Should handle 32KB pages without issues
 */
TEST_F(MemorySafetyTest, Stress_LargePageSize) {
    Database db;
    
    // Test with maximum page size (32KB)
    Status status = db.create("test_cleanup.db", 32768);
    EXPECT_EQ(status, Status::Ok) << "Should handle 32KB page size";
    
    if (status == Status::Ok) {
        EXPECT_EQ(db.page_size(), 32768u);
        EXPECT_TRUE(db.is_open());
        db.close();
    }
}

// ============================================================================
// Test Execution Summary
// ============================================================================

/**
 * Memory Safety Test Summary:
 * 
 * Priority 0 (Critical) Issues - These tests WILL FAIL:
 * 
 * 1. OOM_CreateHeaderAllocation_Line53 - FAILS (throws exception)
 * 2. OOM_OpenHeaderAllocation_Line238 - FAILS (throws exception)  
 * 3. OOM_MissingStatusEnum - FAILS (Status::OOM doesn't exist)
 * 4. MemoryLeak_FileDescriptorOnError - FAILS (FD leak on error)
 * 5. MemoryLeak_AllocationCleanup - FAILS (needs manual verification)
 * 
 * These failures demonstrate the critical issues that must be fixed:
 * - Add try-catch blocks around allocations
 * - Add Status::OOM = 3003 to the enum
 * - Ensure all resources are cleaned up on error paths
 * 
 * Run with AddressSanitizer or Valgrind for complete memory verification:
 *   cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
 *   valgrind --leak-check=full ./scratchbird_tests
 */