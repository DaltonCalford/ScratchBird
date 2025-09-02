/**
 * @file test_page_management_fixed.cpp
 * @brief Fixed page management tests for Alpha 1.03
 * 
 * Updated to account for catalog initialization effects on buffer pool
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <sys/stat.h>
#include <fstream>
#include <vector>
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/buffer_pool.h"

using namespace scratchbird::core;

class PageManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up test files
        remove("test_pm.db");
        remove("test_bp.db");
    }
    
    void TearDown() override {
        // Clean up test files
        remove("test_pm.db");
        remove("test_bp.db");
    }
};

// FSM page creation test - unchanged
TEST_F(PageManagementTest, FSMPageCreation) {
    ASSERT_EQ(Database::create("test_pm.db", 16384), Status::Ok);
    
    // Open database and verify FSM page exists
    Database db;
    ASSERT_EQ(db.open("test_pm.db"), Status::Ok);
    
    // Read FSM page directly
    uint8_t buffer[16384];
    ASSERT_EQ(db.read_page(2, buffer), Status::Ok);
    
    PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
    EXPECT_EQ(header->magic, 0x53425244);  // 'SBRD'
    EXPECT_EQ(header->page_type, PAGE_TYPE_FREE_SPACE_MAP);
    EXPECT_EQ(header->page_id, 2);
}

// Page allocation test - already fixed
TEST_F(PageManagementTest, PageAllocation) {
    Database::create("test_pm.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_pm.db"), Status::Ok);
    
    PageManager* pm = db.page_manager();
    ASSERT_NE(pm, nullptr);
    
    // Initial state: 7 pages total (0-6), 0 free
    // Pages: 0=header, 1=system_catalog, 2=FSM, 3=catalog_root, 4-6=catalog tables
    EXPECT_EQ(pm->total_pages(), 7);
    EXPECT_EQ(pm->free_pages(), 0);
    
    // Allocate a new page
    uint32_t page_id;
    ASSERT_EQ(pm->allocate_page(page_id), Status::Ok);
    EXPECT_EQ(page_id, 7);  // Should be page 7
    
    // Verify allocation
    EXPECT_TRUE(pm->is_allocated(page_id));
    EXPECT_EQ(pm->total_pages(), 8);  // File extended
    EXPECT_EQ(pm->free_pages(), 0);   // New page was allocated
}

// Page freeing test - unchanged
TEST_F(PageManagementTest, PageFreeing) {
    Database::create("test_pm.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_pm.db"), Status::Ok);
    
    PageManager* pm = db.page_manager();
    
    // Allocate a page
    uint32_t page_id;
    ASSERT_EQ(pm->allocate_page(page_id), Status::Ok);
    
    // Free it
    ASSERT_EQ(pm->free_page(page_id), Status::Ok);
    EXPECT_FALSE(pm->is_allocated(page_id));
    EXPECT_EQ(pm->free_pages(), 1);
    
    // Allocate again - should reuse freed page
    uint32_t reused_id;
    ASSERT_EQ(pm->allocate_page(reused_id), Status::Ok);
    EXPECT_EQ(reused_id, page_id);
}

// FSM persistence test - unchanged
TEST_F(PageManagementTest, FSMPersistence) {
    uint32_t allocated_pages[5];
    
    // Create database and allocate pages
    {
        Database::create("test_pm.db", 16384);
        Database db;
        ASSERT_EQ(db.open("test_pm.db"), Status::Ok);
        
        PageManager* pm = db.page_manager();
        
        // Allocate several pages
        for (int i = 0; i < 5; i++) {
            ASSERT_EQ(pm->allocate_page(allocated_pages[i]), Status::Ok);
        }
        
        // Free some pages
        ASSERT_EQ(pm->free_page(allocated_pages[1]), Status::Ok);
        ASSERT_EQ(pm->free_page(allocated_pages[3]), Status::Ok);
        
        // FSM should be flushed on close
    }
    
    // Reopen and verify state
    {
        Database db;
        ASSERT_EQ(db.open("test_pm.db"), Status::Ok);
        
        PageManager* pm = db.page_manager();
        
        // Verify allocations persist
        EXPECT_TRUE(pm->is_allocated(allocated_pages[0]));
        EXPECT_FALSE(pm->is_allocated(allocated_pages[1]));  // Was freed
        EXPECT_TRUE(pm->is_allocated(allocated_pages[2]));
        EXPECT_FALSE(pm->is_allocated(allocated_pages[3]));  // Was freed
        EXPECT_TRUE(pm->is_allocated(allocated_pages[4]));
        
        EXPECT_EQ(pm->free_pages(), 2);
    }
}

/**
 * Test: Buffer pool basic operations
 * Updated: Account for catalog initialization affecting stats
 */
TEST_F(PageManagementTest, BufferPoolBasics) {
    Database::create("test_bp.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_bp.db"), Status::Ok);
    
    BufferPool* bp = db.buffer_pool();
    ASSERT_NE(bp, nullptr);
    
    // Reset stats to account for catalog initialization
    auto initial_stats = bp->get_stats();
    
    // Pin header page
    void* buffer;
    ASSERT_EQ(bp->pin_page(0, &buffer), Status::Ok);
    ASSERT_NE(buffer, nullptr);
    
    // Verify it's the header page
    PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
    EXPECT_EQ(header->magic, 0x53425244);
    EXPECT_EQ(header->page_type, PAGE_TYPE_DATABASE_HEADER);
    
    // Unpin
    ASSERT_EQ(bp->unpin_page(0, false), Status::Ok);
    
    // Check stats relative to initial
    auto final_stats = bp->get_stats();
    
    // We expect one additional access (could be hit or miss depending on catalog usage)
    EXPECT_GE(final_stats.hits + final_stats.misses, 
              initial_stats.hits + initial_stats.misses + 1)
        << "Should have at least one additional buffer pool access";
}

/**
 * Test: Buffer pool cache hits
 * Updated: Use relative stats to account for catalog
 */
TEST_F(PageManagementTest, BufferPoolCacheHit) {
    Database::create("test_bp.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_bp.db"), Status::Ok);
    
    BufferPool* bp = db.buffer_pool();
    
    // Get initial stats after catalog init
    auto initial_stats = bp->get_stats();
    
    // Pin page twice
    void* buffer1;
    void* buffer2;
    ASSERT_EQ(bp->pin_page(0, &buffer1), Status::Ok);
    ASSERT_EQ(bp->pin_page(0, &buffer2), Status::Ok);
    
    // Should be same buffer
    EXPECT_EQ(buffer1, buffer2);
    
    // Check stats
    auto stats = bp->get_stats();
    
    // First pin might be hit or miss (depending on catalog usage)
    // Second pin should definitely be a hit
    EXPECT_GT(stats.hits, initial_stats.hits)
        << "Second pin should be a cache hit";
    
    // Unpin twice
    ASSERT_EQ(bp->unpin_page(0, false), Status::Ok);
    ASSERT_EQ(bp->unpin_page(0, false), Status::Ok);
}

// Test buffer pool dirty page handling - unchanged
TEST_F(PageManagementTest, BufferPoolDirtyPages) {
    Database::create("test_bp.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_bp.db"), Status::Ok);
    
    BufferPool* bp = db.buffer_pool();
    PageManager* pm = db.page_manager();
    
    // Allocate a new page
    uint32_t page_id;
    ASSERT_EQ(pm->allocate_page(page_id), Status::Ok);
    
    // Pin and modify
    void* buffer;
    ASSERT_EQ(bp->pin_page(page_id, &buffer), Status::Ok);
    
    // Write some data
    memset(buffer, 0xAB, 16384);
    
    // Unpin as dirty
    ASSERT_EQ(bp->unpin_page(page_id, true), Status::Ok);
    
    // Flush
    ASSERT_EQ(bp->flush_page(page_id), Status::Ok);
    
    // Verify data persisted
    uint8_t verify_buffer[16384];
    ASSERT_EQ(db.read_page(page_id, verify_buffer), Status::Ok);
    EXPECT_EQ(verify_buffer[0], 0xAB);
    EXPECT_EQ(verify_buffer[16383], 0xAB);
}

// Test buffer pool eviction - unchanged
TEST_F(PageManagementTest, BufferPoolEviction) {
    Database::create("test_bp.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_bp.db"), Status::Ok);
    
    BufferPool* bp = db.buffer_pool();
    PageManager* pm = db.page_manager();
    
    // Allocate many pages (more than buffer pool capacity)
    std::vector<uint32_t> pages;
    for (int i = 0; i < 40; i++) {  // More than default pool size
        uint32_t page_id;
        ASSERT_EQ(pm->allocate_page(page_id), Status::Ok);
        pages.push_back(page_id);
    }
    
    // Pin all pages (will cause evictions)
    for (uint32_t page_id : pages) {
        void* buffer;
        ASSERT_EQ(bp->pin_page(page_id, &buffer), Status::Ok);
        ASSERT_EQ(bp->unpin_page(page_id, false), Status::Ok);
    }
    
    // Check eviction stats
    auto stats = bp->get_stats();
    EXPECT_GT(stats.evictions, 0) << "Should have evicted pages";
}

// Test large page allocation - unchanged
TEST_F(PageManagementTest, LargePageAllocation) {
    Database::create("test_pm.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_pm.db"), Status::Ok);
    
    PageManager* pm = db.page_manager();
    
    // Allocate 100 pages
    std::vector<uint32_t> pages;
    for (int i = 0; i < 100; i++) {
        uint32_t page_id;
        ASSERT_EQ(pm->allocate_page(page_id), Status::Ok);
        pages.push_back(page_id);
        EXPECT_TRUE(pm->is_allocated(page_id));
    }
    
    // Free every other page
    for (size_t i = 0; i < pages.size(); i += 2) {
        ASSERT_EQ(pm->free_page(pages[i]), Status::Ok);
    }
    
    EXPECT_EQ(pm->free_pages(), 50);
    
    // Allocate 50 more - should reuse freed pages
    for (int i = 0; i < 50; i++) {
        uint32_t page_id;
        ASSERT_EQ(pm->allocate_page(page_id), Status::Ok);
        
        // Should reuse a freed page
        bool is_reused = false;
        for (size_t j = 0; j < pages.size(); j += 2) {
            if (page_id == pages[j]) {
                is_reused = true;
                break;
            }
        }
        EXPECT_TRUE(is_reused) << "Should reuse freed pages";
    }
}