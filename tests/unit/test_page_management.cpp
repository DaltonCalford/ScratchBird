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

// Test FSM page creation during database creation
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

// Test page allocation
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

// Test page freeing
TEST_F(PageManagementTest, PageFreeing) {
    Database::create("test_pm.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_pm.db"), Status::Ok);
    
    PageManager* pm = db.page_manager();
    
    // Allocate a page
    uint32_t page_id;
    ASSERT_EQ(pm->allocate_page(page_id), Status::Ok);
    
    // Free the page
    ASSERT_EQ(pm->free_page(page_id), Status::Ok);
    EXPECT_FALSE(pm->is_allocated(page_id));
    EXPECT_EQ(pm->free_pages(), 1);
    
    // Try to free system pages (should fail)
    EXPECT_NE(pm->free_page(0), Status::Ok);  // Header
    EXPECT_NE(pm->free_page(1), Status::Ok);  // Catalog
    EXPECT_NE(pm->free_page(2), Status::Ok);  // FSM
}

// Test FSM persistence
TEST_F(PageManagementTest, FSMPersistence) {
    uint32_t allocated_pages[5];
    
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

// Test buffer pool basic operations
TEST_F(PageManagementTest, BufferPoolBasics) {
    Database::create("test_bp.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_bp.db"), Status::Ok);
    
    BufferPool* bp = db.buffer_pool();
    ASSERT_NE(bp, nullptr);
    
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
    
    // Check stats
    auto stats = bp->get_stats();
    EXPECT_EQ(stats.hits, 0);
    EXPECT_EQ(stats.misses, 1);
}

// Test buffer pool cache hits
TEST_F(PageManagementTest, BufferPoolCacheHit) {
    Database::create("test_bp.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_bp.db"), Status::Ok);
    
    BufferPool* bp = db.buffer_pool();
    
    // Pin page twice
    void* buffer1;
    void* buffer2;
    ASSERT_EQ(bp->pin_page(0, &buffer1), Status::Ok);
    ASSERT_EQ(bp->pin_page(0, &buffer2), Status::Ok);
    
    // Should be same buffer
    EXPECT_EQ(buffer1, buffer2);
    
    // Check stats
    auto stats = bp->get_stats();
    EXPECT_EQ(stats.hits, 1);   // Second pin was a hit
    EXPECT_EQ(stats.misses, 1); // First pin was a miss
    
    // Unpin twice
    ASSERT_EQ(bp->unpin_page(0, false), Status::Ok);
    ASSERT_EQ(bp->unpin_page(0, false), Status::Ok);
}

// Test buffer pool dirty page handling
TEST_F(PageManagementTest, BufferPoolDirtyPages) {
    Database::create("test_bp.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_bp.db"), Status::Ok);
    
    BufferPool* bp = db.buffer_pool();
    PageManager* pm = db.page_manager();
    
    // Allocate a new page
    uint32_t page_id;
    ASSERT_EQ(pm->allocate_page(page_id), Status::Ok);
    
    // Pin the page
    void* buffer;
    ASSERT_EQ(bp->pin_page(page_id, &buffer), Status::Ok);
    
    // Modify the page
    PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
    header->generation = 42;
    
    // Unpin as dirty
    ASSERT_EQ(bp->unpin_page(page_id, true), Status::Ok);
    
    // Force flush
    ASSERT_EQ(bp->flush_page(page_id), Status::Ok);
    
    // Verify flush worked
    auto stats = bp->get_stats();
    EXPECT_EQ(stats.flushes, 1);
}

// Test buffer pool eviction
TEST_F(PageManagementTest, BufferPoolEviction) {
    Database::create("test_bp.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_bp.db"), Status::Ok);
    
    BufferPool* bp = db.buffer_pool();
    PageManager* pm = db.page_manager();
    
    // Allocate more pages than buffer pool size (32)
    std::vector<uint32_t> pages;
    for (int i = 0; i < 35; i++) {
        uint32_t page_id;
        ASSERT_EQ(pm->allocate_page(page_id), Status::Ok);
        pages.push_back(page_id);
    }
    
    // Pin all pages (this will cause evictions)
    std::vector<void*> buffers;
    for (uint32_t page_id : pages) {
        void* buffer;
        ASSERT_EQ(bp->pin_page(page_id, &buffer), Status::Ok);
        buffers.push_back(buffer);
        
        // Unpin immediately so they can be evicted
        ASSERT_EQ(bp->unpin_page(page_id, false), Status::Ok);
    }
    
    // Check that evictions occurred
    auto stats = bp->get_stats();
    EXPECT_GT(stats.evictions, 0);
}

// Test that system pages are properly initialized
TEST_F(PageManagementTest, SystemPagesInitialized) {
    Database::create("test_pm.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_pm.db"), Status::Ok);
    
    PageManager* pm = db.page_manager();
    
    // System pages should be allocated
    EXPECT_TRUE(pm->is_allocated(0));  // Header
    EXPECT_TRUE(pm->is_allocated(1));  // Catalog
    EXPECT_TRUE(pm->is_allocated(2));  // FSM
}

// Test file extension
TEST_F(PageManagementTest, FileExtension) {
    Database::create("test_pm.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test_pm.db"), Status::Ok);
    
    PageManager* pm = db.page_manager();
    
    uint32_t initial_pages = pm->total_pages();
    
    // Extend file by multiple pages
    ASSERT_EQ(pm->extend_file(10), Status::Ok);
    
    EXPECT_EQ(pm->total_pages(), initial_pages + 10);
    EXPECT_EQ(pm->free_pages(), 10);
    
    // Verify file size increased
    struct stat st;
    ASSERT_EQ(stat("test_pm.db", &st), 0);
    EXPECT_EQ(st.st_size, (initial_pages + 10) * 16384);
}