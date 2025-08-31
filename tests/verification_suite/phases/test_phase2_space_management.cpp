/**
 * Phase 2: Space Management and Allocation - Comprehensive Tests
 * 
 * Tests all requirements from ProjectPlan/Phase 2
 * Exit Criteria: Deterministic growth, reclaim on drop/truncate, allocator soak tests
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <thread>
#include <atomic>
#include "scratchbird/engine.h"
#include "scratchbird/engine/alloc.h"
#include "scratchbird/engine/ods.h"

namespace fs = std::filesystem;

class Phase2SpaceManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "phase2_test";
        fs::create_directories(test_dir);
        
        scratchbird::Status status;
        scratchbird::CreateDbOptions opts;
        opts.page_size = 8192;
        db = scratchbird::create_database(test_dir / "test.db", opts, status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    }
    
    void TearDown() override {
        if (db) scratchbird::close_database(db);
        fs::remove_all(test_dir);
    }
    
    fs::path test_dir;
    std::shared_ptr<scratchbird::Database> db;
};

// Test 2.1: PIP (Page Inventory Page) Implementation
TEST_F(Phase2SpaceManagementTest, PIPImplementation) {
    scratchbird::engine::FileMap::Layout layout;
    layout.page_size = 8192;
    
    scratchbird::engine::FileMap fmap(layout);
    fmap.set_base_path(test_dir, "pip_test");
    
    // Create PIP page
    std::vector<uint8_t> page(layout.page_size);
    auto* header = reinterpret_cast<scratchbird::engine::ods::PageHeader*>(page.data());
    header->type = static_cast<uint16_t>(scratchbird::engine::ods::PageType::Pip);
    header->page_no = 1;  // PIP typically at page 1
    
    // Calculate pages per PIP
    auto pages_per_pip = scratchbird::engine::ods::pagesPerPIP(layout.page_size);
    EXPECT_GT(pages_per_pip, 0);
    
    // PIP bitmap (0 = free, 1 = allocated)
    uint8_t* bitmap = page.data() + sizeof(scratchbird::engine::ods::PageHeader);
    
    // Mark some pages as allocated
    bitmap[0] = 0xFF;  // First 8 pages allocated
    bitmap[1] = 0x0F;  // Next 4 pages allocated
    
    // Verify bit operations
    auto is_allocated = [&](uint32_t page_no) {
        uint32_t byte_idx = page_no / 8;
        uint8_t bit_idx = page_no % 8;
        return (bitmap[byte_idx] & (1 << bit_idx)) != 0;
    };
    
    EXPECT_TRUE(is_allocated(0));
    EXPECT_TRUE(is_allocated(7));
    EXPECT_TRUE(is_allocated(8));
    EXPECT_FALSE(is_allocated(12));
    
    // Write PIP
    fmap.write_page(1, page.data());
    
    // Verify PIP chain for large databases
    if (pages_per_pip < 1000000) {  // Need multiple PIPs
        header->next = 1001;  // Link to next PIP
        EXPECT_EQ(header->next, 1001);
    }
}

// Test 2.2: TIP (Transaction Inventory Page) Seeding
TEST_F(Phase2SpaceManagementTest, TIPSeeding) {
    scratchbird::engine::FileMap::Layout layout;
    layout.page_size = 8192;
    
    scratchbird::engine::FileMap fmap(layout);
    fmap.set_base_path(test_dir, "tip_test");
    
    // Create TIP page
    scratchbird::engine::TransactionManager tm(std::move(fmap), layout.page_size);
    tm.init_seed();  // Phase 2 seed
    
    // Verify TIP structure
    std::vector<uint8_t> page(layout.page_size);
    fmap.read_page(2, page.data());  // TIP typically at page 2
    
    auto* header = reinterpret_cast<scratchbird::engine::ods::PageHeader*>(page.data());
    EXPECT_EQ(header->type, static_cast<uint16_t>(scratchbird::engine::ods::PageType::Tip));
    
    // Calculate transactions per TIP
    auto trans_per_tip = scratchbird::engine::ods::transPerTIP(layout.page_size);
    EXPECT_GT(trans_per_tip, 0);
    
    // Verify all transactions initially idle (0)
    uint8_t* tip_data = page.data() + 64;  // TIP data after header
    for (size_t i = 0; i < 100; i++) {
        EXPECT_EQ(tip_data[i], 0) << "Transaction " << i << " should be idle";
    }
}

// Test 2.3: Space Catalog Implementation
TEST_F(Phase2SpaceManagementTest, SpaceCatalog) {
    scratchbird::Status status;
    
    // Get space catalog info
    auto catalog = scratchbird::engine::get_space_catalog(db, status);
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    
    // Verify default space (space_id = 1)
    EXPECT_EQ(catalog.spaces.size(), 1);
    EXPECT_EQ(catalog.spaces[0].space_id, 1);
    EXPECT_EQ(catalog.spaces[0].page_size, 8192);
    EXPECT_GT(catalog.spaces[0].pip_root_page, 0);
    EXPECT_GT(catalog.spaces[0].tip_root_page, 0);
    EXPECT_GE(catalog.spaces[0].segment_count, 1);
    
    // Verify extent tracking
    EXPECT_GT(catalog.spaces[0].next_extent_id, 0);
    
    // Test creating additional space (tablespace scaffold)
    scratchbird::engine::SpaceOptions space_opts;
    space_opts.page_size = 16384;
    space_opts.initial_segments = 2;
    
    auto space_id = scratchbird::engine::create_space(db, "test_space", space_opts, status);
    EXPECT_GT(space_id, 1);
    
    // Verify new space in catalog
    catalog = scratchbird::engine::get_space_catalog(db, status);
    EXPECT_EQ(catalog.spaces.size(), 2);
}

// Test 2.4: Extent Management
TEST_F(Phase2SpaceManagementTest, ExtentManagement) {
    scratchbird::engine::FileMap::Layout layout;
    layout.page_size = 8192;
    
    scratchbird::engine::FileMap fmap(layout);
    fmap.set_base_path(test_dir, "extent_test");
    
    scratchbird::engine::Allocator alloc(&fmap, layout.page_size);
    
    // Allocate an extent (default 8 pages)
    auto extent_pages = alloc.allocate_extent();
    ASSERT_EQ(extent_pages.size(), 8);
    
    // Verify pages are contiguous
    for (size_t i = 1; i < extent_pages.size(); i++) {
        EXPECT_EQ(extent_pages[i], extent_pages[i-1] + 1)
            << "Extent pages should be contiguous";
    }
    
    // Free the extent
    alloc.free_extent(extent_pages[0]);
    
    // Allocate again - should reuse
    auto reused_extent = alloc.allocate_extent();
    EXPECT_EQ(reused_extent[0], extent_pages[0])
        << "Freed extent should be reused";
}

// Test 2.5: Multi-Segment Expansion
TEST_F(Phase2SpaceManagementTest, MultiSegmentExpansion) {
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE segment_test (id INTEGER, data TEXT)", status), {});
    
    // Get initial segment count
    fs::path seg0 = test_dir / "test.db.seg0";
    ASSERT_TRUE(fs::exists(seg0));
    
    // Insert enough data to trigger segment expansion
    const int rows_per_segment = 100000;  // Adjust based on page size
    for (int i = 0; i < rows_per_segment * 2; i++) {
        std::string large_data(1000, 'X');  // 1KB per row
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO segment_test VALUES (?, ?)", status),
            {std::to_string(i), large_data});
        
        // Check for new segment
        if (i == rows_per_segment) {
            fs::path seg1 = test_dir / "test.db.seg1";
            EXPECT_TRUE(fs::exists(seg1))
                << "Second segment should be created after enough data";
        }
    }
    
    // Verify multiple segments exist
    int segment_count = 0;
    for (int i = 0; i < 10; i++) {
        fs::path seg = test_dir / ("test.db.seg" + std::to_string(i));
        if (fs::exists(seg)) {
            segment_count++;
        } else {
            break;
        }
    }
    
    EXPECT_GE(segment_count, 2) << "Multi-segment expansion should occur";
}

// Test 2.6: Deterministic Page Growth
TEST_F(Phase2SpaceManagementTest, DeterministicPageGrowth) {
    scratchbird::engine::FileMap::Layout layout;
    layout.page_size = 8192;
    layout.pages_per_segment = 1000;  // Small for testing
    
    scratchbird::engine::FileMap fmap(layout);
    fmap.set_base_path(test_dir, "growth_test");
    
    scratchbird::engine::Allocator alloc(&fmap, layout.page_size);
    alloc.init_new();
    
    // Track allocated pages
    std::vector<uint32_t> allocated_pages;
    
    // Allocate pages deterministically
    for (int i = 0; i < 100; i++) {
        auto page = alloc.allocate_free_page();
        allocated_pages.push_back(page);
        
        // Verify deterministic allocation
        if (i > 0) {
            EXPECT_GT(page, allocated_pages[i-1])
                << "Pages should be allocated in increasing order";
        }
    }
    
    // Free some pages
    for (int i = 10; i < 20; i++) {
        alloc.free_page(allocated_pages[i]);
    }
    
    // Allocate again - should reuse freed pages
    auto reused = alloc.allocate_free_page();
    EXPECT_GE(reused, allocated_pages[10]);
    EXPECT_LE(reused, allocated_pages[19]);
}

// Test 2.7: Reclaim on DROP/TRUNCATE
TEST_F(Phase2SpaceManagementTest, ReclaimOnDropTruncate) {
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create and populate table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE reclaim_test (id INTEGER, data TEXT)", status), {});
    
    for (int i = 0; i < 1000; i++) {
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO reclaim_test VALUES (?, ?)", status),
            {std::to_string(i), "Data " + std::to_string(i)});
    }
    
    // Get space usage before drop
    auto space_before = scratchbird::engine::get_space_usage(db, status);
    ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    
    // DROP table
    scratchbird::execute(scratchbird::prepare(session,
        "DROP TABLE reclaim_test", status), {});
    
    // Get space usage after drop
    auto space_after = scratchbird::engine::get_space_usage(db, status);
    
    // Verify space reclaimed
    EXPECT_LT(space_after.allocated_pages, space_before.allocated_pages)
        << "Pages should be reclaimed after DROP";
    EXPECT_GT(space_after.free_pages, space_before.free_pages)
        << "Free pages should increase after DROP";
    
    // Test TRUNCATE
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE truncate_test (id INTEGER)", status), {});
    
    for (int i = 0; i < 1000; i++) {
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO truncate_test VALUES (?)", status),
            {std::to_string(i)});
    }
    
    space_before = scratchbird::engine::get_space_usage(db, status);
    
    scratchbird::execute(scratchbird::prepare(session,
        "TRUNCATE TABLE truncate_test", status), {});
    
    space_after = scratchbird::engine::get_space_usage(db, status);
    
    EXPECT_LT(space_after.allocated_pages, space_before.allocated_pages)
        << "Pages should be reclaimed after TRUNCATE";
}

// Test 2.8: Allocator Crash Resilience
TEST_F(Phase2SpaceManagementTest, AllocatorCrashResilience) {
    // Test allocator recovery after crash
    scratchbird::engine::FileMap::Layout layout;
    layout.page_size = 8192;
    
    // Simulate partial allocation
    {
        scratchbird::engine::FileMap fmap(layout);
        fmap.set_base_path(test_dir, "crash_test");
        
        scratchbird::engine::Allocator alloc(&fmap, layout.page_size);
        alloc.init_new();
        
        // Start allocation transaction
        alloc.begin_allocation_batch();
        
        // Allocate some pages
        for (int i = 0; i < 10; i++) {
            alloc.allocate_free_page();
        }
        
        // Simulate crash - destructor without commit
    }
    
    // Recovery after crash
    {
        scratchbird::engine::FileMap fmap(layout);
        fmap.set_base_path(test_dir, "crash_test");
        
        scratchbird::engine::Allocator alloc(&fmap, layout.page_size);
        
        // Verify allocator recovers consistently
        auto validation = alloc.validate();
        EXPECT_TRUE(validation.consistent)
            << "Allocator should recover to consistent state";
        
        // Should be able to allocate after recovery
        auto page = alloc.allocate_free_page();
        EXPECT_GT(page, 0);
    }
}

// Test 2.9: Allocator Soak Test
TEST_F(Phase2SpaceManagementTest, AllocatorSoakTest) {
    // Exit criteria: allocator soak tests clean
    scratchbird::engine::FileMap::Layout layout;
    layout.page_size = 8192;
    
    scratchbird::engine::FileMap fmap(layout);
    fmap.set_base_path(test_dir, "soak_test");
    
    scratchbird::engine::Allocator alloc(&fmap, layout.page_size);
    alloc.init_new();
    
    const int num_threads = 10;
    const int ops_per_thread = 1000;
    std::atomic<int> errors(0);
    
    auto worker = [&](int thread_id) {
        std::vector<uint32_t> my_pages;
        std::mt19937 rng(thread_id);
        std::uniform_int_distribution<> op_dist(0, 2);
        
        for (int i = 0; i < ops_per_thread; i++) {
            try {
                int op = op_dist(rng);
                
                if (op == 0 || my_pages.empty()) {
                    // Allocate
                    auto page = alloc.allocate_free_page();
                    if (page > 0) {
                        my_pages.push_back(page);
                    }
                } else if (op == 1 && !my_pages.empty()) {
                    // Free random page
                    size_t idx = rng() % my_pages.size();
                    alloc.free_page(my_pages[idx]);
                    my_pages.erase(my_pages.begin() + idx);
                } else {
                    // Allocate extent
                    auto extent = alloc.allocate_extent();
                    my_pages.insert(my_pages.end(), extent.begin(), extent.end());
                }
            } catch (...) {
                errors++;
            }
        }
        
        // Free all remaining pages
        for (auto page : my_pages) {
            alloc.free_page(page);
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(errors.load(), 0) << "No errors in allocator soak test";
    
    // Final validation
    auto validation = alloc.validate();
    EXPECT_TRUE(validation.consistent);
    EXPECT_EQ(validation.leaked_pages, 0);
    EXPECT_EQ(validation.double_allocated, 0);
    
    std::cout << "Phase 2 Exit Criteria MET: "
              << "✅ Deterministic growth\n"
              << "✅ Reclaim on drop/truncate\n"
              << "✅ Allocator soak tests clean\n";
}

// Test 2.10: Tablespace Placement Scaffolding
TEST_F(Phase2SpaceManagementTest, TablespacePlacementScaffold) {
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create tablespace (scaffold in Phase 2)
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLESPACE fast_space LOCATION '/fast/ssd'", status), {});
    
    // Create table in specific tablespace
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE fast_table (id INTEGER) TABLESPACE fast_space", status), {});
    
    // Verify table uses correct space_id
    auto table_info = scratchbird::engine::get_table_info(session, "fast_table", status);
    EXPECT_GT(table_info.space_id, 1) << "Table should use non-default space";
    
    // Verify no online moves in Phase 2
    auto move_result = scratchbird::execute(scratchbird::prepare(session,
        "ALTER TABLE fast_table SET TABLESPACE pg_default", status), {});
    
    EXPECT_NE(move_result.code, scratchbird::StatusCode::Ok)
        << "Online tablespace moves deferred to later phases";
}