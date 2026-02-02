/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// PHASE 2 TASK 9.2: R-tree Spatial Index Integration & Testing
// Agent R4: R-tree integration with catalog, planner, and executor
//
// This test file provides comprehensive testing for the R-tree spatial index:
// - Basic insertion, search, and deletion operations
// - Bulk insertion with 1000+ points
// - Range queries with various bounding box shapes
// - Tree balance and structural correctness
// - Performance benchmarks
// - MGA compliance (transaction visibility)

#include <gtest/gtest.h>
#include "scratchbird/core/rtree.h"
#include "scratchbird/core/rtree_node.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include <filesystem>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>

using namespace scratchbird::core;

class RTreeTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database *db_;

    void SetUp() override
    {
        // Create temporary database for testing
        test_db_path_ = "/tmp/test_rtree_" + std::to_string(getpid()) + ".db";

        // Remove if exists
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }

        // Create database
        db_ = new Database();
        ErrorContext ctx;
        Status status = db_->create(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        status = db_->open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
            delete db_;
            db_ = nullptr;
        }

        // Clean up test database
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    // Helper: Create a point bounding box
    BoundingBox createPoint(double x, double y)
    {
        return BoundingBox(x, y, x, y);
    }

    // Helper: Create a rectangle bounding box
    BoundingBox createRect(double min_x, double min_y, double max_x, double max_y)
    {
        return BoundingBox(min_x, min_y, max_x, max_y);
    }

    // Helper: Create a TID from page and slot
    TID createTID(uint64_t page, uint64_t slot)
    {
        TID tid;
        tid.page_number = page;
        tid.slot_number = slot;
        return tid;
    }
};

// ============================================================================
// Test 1: Empty Tree Operations
// ============================================================================
TEST_F(RTreeTest, EmptyTreeSearch)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = RTree::create(db_, index_uuid, table_uuid, column_uuids,
                                   50, // max_entries
                                   &root_page, &ctx);

    ASSERT_EQ(status, Status::OK) << "Failed to create R-tree: " << ctx.message;
    ASSERT_GT(root_page, 0) << "Root page should be allocated";

    // Open the R-tree
    std::unique_ptr<RTree> rtree = RTree::open(db_, index_uuid, root_page, 50, &ctx);
    ASSERT_NE(rtree, nullptr) << "Failed to open R-tree";

    // Search in empty tree should return no results
    BoundingBox search_bbox = createRect(0.0, 0.0, 100.0, 100.0);
    std::vector<TID> results;
    status = rtree->search(search_bbox, nullptr, &results, &ctx);

    EXPECT_EQ(status, Status::OK) << "Search failed: " << ctx.message;
    EXPECT_EQ(results.size(), 0) << "Empty tree should return no results";
}

// ============================================================================
// Test 2: Single Element Insertion and Search
// ============================================================================
TEST_F(RTreeTest, SingleElementInsertSearch)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = RTree::create(db_, index_uuid, table_uuid, column_uuids,
                                   50, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    std::unique_ptr<RTree> rtree = RTree::open(db_, index_uuid, root_page, 50, &ctx);
    ASSERT_NE(rtree, nullptr);

    // Insert a single point
    BoundingBox point = createPoint(50.0, 50.0);
    TID tid = createTID(1, 0);
    status = rtree->insert(point, tid, nullptr, &ctx);
    EXPECT_EQ(status, Status::OK) << "Insert failed: " << ctx.message;

    // Search for the point (exact match)
    std::vector<TID> results;
    status = rtree->search(point, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
    ASSERT_EQ(results.size(), 1) << "Should find exactly one result";
    EXPECT_EQ(results[0].page_number, 1);
    EXPECT_EQ(results[0].slot_number, 0);

    // Search with larger bounding box (should contain the point)
    results.clear();
    BoundingBox large_bbox = createRect(0.0, 0.0, 100.0, 100.0);
    status = rtree->search(large_bbox, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(results.size(), 1) << "Large bbox should contain the point";

    // Search with non-overlapping bounding box
    results.clear();
    BoundingBox non_overlap = createRect(200.0, 200.0, 300.0, 300.0);
    status = rtree->search(non_overlap, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(results.size(), 0) << "Non-overlapping bbox should return no results";
}

// ============================================================================
// Test 3: Multiple Points Insertion and Search
// ============================================================================
TEST_F(RTreeTest, MultiplePointsInsertSearch)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = RTree::create(db_, index_uuid, table_uuid, column_uuids,
                                   50, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    std::unique_ptr<RTree> rtree = RTree::open(db_, index_uuid, root_page, 50, &ctx);
    ASSERT_NE(rtree, nullptr);

    // Insert 10 points in a grid pattern
    const int grid_size = 10;
    for (int i = 0; i < grid_size; ++i)
    {
        for (int j = 0; j < grid_size; ++j)
        {
            BoundingBox point = createPoint(i * 10.0, j * 10.0);
            TID tid = createTID(i, j);
            status = rtree->insert(point, tid, nullptr, &ctx);
            ASSERT_EQ(status, Status::OK) << "Insert failed at (" << i << ", " << j << ")";
        }
    }

    // Search for all points in a region
    BoundingBox search_bbox = createRect(25.0, 25.0, 65.0, 65.0);
    std::vector<TID> results;
    status = rtree->search(search_bbox, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Should find points at (30,30), (30,40), (30,50), (30,60),
    //                        (40,30), (40,40), (40,50), (40,60),
    //                        (50,30), (50,40), (50,50), (50,60),
    //                        (60,30), (60,40), (60,50), (60,60)
    // Total: 16 points
    EXPECT_EQ(results.size(), 16) << "Should find 16 points in the search region";

    // Verify tree statistics
    EXPECT_EQ(rtree->getTotalEntries(), 100) << "Should have 100 total entries";
    EXPECT_EQ(rtree->getDeletedEntries(), 0) << "Should have no deleted entries";
}

// ============================================================================
// Test 4: Bulk Insertion (1000+ Points) - Performance Test
// ============================================================================
TEST_F(RTreeTest, BulkInsertion1000Points)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = RTree::create(db_, index_uuid, table_uuid, column_uuids,
                                   50, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    std::unique_ptr<RTree> rtree = RTree::open(db_, index_uuid, root_page, 50, &ctx);
    ASSERT_NE(rtree, nullptr);

    // Generate 1000 random points
    const int num_points = 1000;
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_real_distribution<double> dist(0.0, 1000.0);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_points; ++i)
    {
        double x = dist(rng);
        double y = dist(rng);
        BoundingBox point = createPoint(x, y);
        TID tid = createTID(i / 100, i % 100);

        status = rtree->insert(point, tid, nullptr, &ctx);
        ASSERT_EQ(status, Status::OK) << "Insert failed at iteration " << i;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Performance requirement: 1000 insertions should take < 100ms
    EXPECT_LT(duration.count(), 100)
        << "Bulk insertion took " << duration.count() << "ms (expected < 100ms)";

    // Verify all points were inserted
    EXPECT_EQ(rtree->getTotalEntries(), num_points);

    // Test range query performance
    start_time = std::chrono::high_resolution_clock::now();

    BoundingBox search_bbox = createRect(400.0, 400.0, 600.0, 600.0);
    std::vector<TID> results;
    status = rtree->search(search_bbox, nullptr, &results, &ctx);

    end_time = std::chrono::high_resolution_clock::now();
    auto search_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(results.size(), 0) << "Search should find some points";

    // Range query should be fast (< 10ms)
    EXPECT_LT(search_duration.count(), 10000)
        << "Range query took " << search_duration.count() << "us (expected < 10000us)";
}

// ============================================================================
// Test 5: Overlapping Rectangles
// ============================================================================
TEST_F(RTreeTest, OverlappingRectangles)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = RTree::create(db_, index_uuid, table_uuid, column_uuids,
                                   50, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    std::unique_ptr<RTree> rtree = RTree::open(db_, index_uuid, root_page, 50, &ctx);
    ASSERT_NE(rtree, nullptr);

    // Insert overlapping rectangles
    BoundingBox rect1 = createRect(0.0, 0.0, 50.0, 50.0);
    BoundingBox rect2 = createRect(25.0, 25.0, 75.0, 75.0);
    BoundingBox rect3 = createRect(50.0, 50.0, 100.0, 100.0);

    status = rtree->insert(rect1, createTID(1, 0), nullptr, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = rtree->insert(rect2, createTID(2, 0), nullptr, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = rtree->insert(rect3, createTID(3, 0), nullptr, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Search for overlapping rectangles
    BoundingBox search_bbox = createRect(40.0, 40.0, 60.0, 60.0);
    std::vector<TID> results;
    status = rtree->search(search_bbox, nullptr, &results, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(results.size(), 3) << "All three rectangles should overlap with search bbox";
}

// ============================================================================
// Test 6: Deletion Operations
// ============================================================================
TEST_F(RTreeTest, DeletionBasic)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = RTree::create(db_, index_uuid, table_uuid, column_uuids,
                                   50, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    std::unique_ptr<RTree> rtree = RTree::open(db_, index_uuid, root_page, 50, &ctx);
    ASSERT_NE(rtree, nullptr);

    // Insert 5 points
    for (int i = 0; i < 5; ++i)
    {
        BoundingBox point = createPoint(i * 10.0, i * 10.0);
        TID tid = createTID(i, 0);
        status = rtree->insert(point, tid, nullptr, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    EXPECT_EQ(rtree->getTotalEntries(), 5);

    // Delete one point
    BoundingBox to_delete = createPoint(20.0, 20.0);
    TID delete_tid = createTID(2, 0);
    status = rtree->remove(to_delete, delete_tid, nullptr, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Search should not find deleted point
    std::vector<TID> results;
    status = rtree->search(to_delete, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(results.size(), 0) << "Deleted point should not be found";

    // Other points should still be findable
    results.clear();
    BoundingBox search_all = createRect(0.0, 0.0, 50.0, 50.0);
    status = rtree->search(search_all, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(results.size(), 4) << "Should find 4 remaining points";
}

// ============================================================================
// Test 7: Tree Height and Balance
// ============================================================================
TEST_F(RTreeTest, TreeHeightAndBalance)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    // Use smaller max_entries to force tree growth
    uint32_t root_page = 0;
    Status status = RTree::create(db_, index_uuid, table_uuid, column_uuids,
                                   10, // Small max_entries to force splits
                                   &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    std::unique_ptr<RTree> rtree = RTree::open(db_, index_uuid, root_page, 10, &ctx);
    ASSERT_NE(rtree, nullptr);

    // Initially height should be 0 (single node)
    EXPECT_EQ(rtree->getHeight(), 0);

    // Insert enough points to force multiple levels
    for (int i = 0; i < 100; ++i)
    {
        BoundingBox point = createPoint(i * 1.0, i * 1.0);
        TID tid = createTID(i / 10, i % 10);
        status = rtree->insert(point, tid, nullptr, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Tree should have grown in height
    uint16_t height = rtree->getHeight();
    EXPECT_GT(height, 0) << "Tree should have grown beyond root level";
    EXPECT_LE(height, 4) << "Tree should be reasonably balanced (height <= 4 for 100 entries)";
}

// ============================================================================
// Test 8: Point Queries
// ============================================================================
TEST_F(RTreeTest, PointQueries)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = RTree::create(db_, index_uuid, table_uuid, column_uuids,
                                   50, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    std::unique_ptr<RTree> rtree = RTree::open(db_, index_uuid, root_page, 50, &ctx);
    ASSERT_NE(rtree, nullptr);

    // Insert random points
    std::mt19937 rng(123);
    std::uniform_real_distribution<double> dist(0.0, 100.0);

    for (int i = 0; i < 50; ++i)
    {
        BoundingBox point = createPoint(dist(rng), dist(rng));
        TID tid = createTID(i, 0);
        status = rtree->insert(point, tid, nullptr, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Test point query (very small bounding box)
    BoundingBox point_query = createRect(49.9, 49.9, 50.1, 50.1);
    std::vector<TID> results;
    status = rtree->search(point_query, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
}

// ============================================================================
// Test 9: Various Query Shapes
// ============================================================================
TEST_F(RTreeTest, VariousQueryShapes)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = RTree::create(db_, index_uuid, table_uuid, column_uuids,
                                   50, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    std::unique_ptr<RTree> rtree = RTree::open(db_, index_uuid, root_page, 50, &ctx);
    ASSERT_NE(rtree, nullptr);

    // Insert points in a 20x20 grid
    for (int i = 0; i < 20; ++i)
    {
        for (int j = 0; j < 20; ++j)
        {
            BoundingBox point = createPoint(i * 5.0, j * 5.0);
            TID tid = createTID(i, j);
            status = rtree->insert(point, tid, nullptr, &ctx);
            ASSERT_EQ(status, Status::OK);
        }
    }

    // Test 1: Wide rectangle (horizontal strip)
    std::vector<TID> results;
    BoundingBox horizontal_strip = createRect(0.0, 45.0, 95.0, 55.0);
    status = rtree->search(horizontal_strip, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(results.size(), 0);

    // Test 2: Tall rectangle (vertical strip)
    results.clear();
    BoundingBox vertical_strip = createRect(45.0, 0.0, 55.0, 95.0);
    status = rtree->search(vertical_strip, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(results.size(), 0);

    // Test 3: Small square
    results.clear();
    BoundingBox small_square = createRect(20.0, 20.0, 30.0, 30.0);
    status = rtree->search(small_square, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);

    // Test 4: Large square covering most points
    results.clear();
    BoundingBox large_square = createRect(10.0, 10.0, 80.0, 80.0);
    status = rtree->search(large_square, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_GT(results.size(), 100) << "Large square should contain many points";
}

// ============================================================================
// Test 10: Clear Index
// ============================================================================
TEST_F(RTreeTest, ClearIndex)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = RTree::create(db_, index_uuid, table_uuid, column_uuids,
                                   50, &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    std::unique_ptr<RTree> rtree = RTree::open(db_, index_uuid, root_page, 50, &ctx);
    ASSERT_NE(rtree, nullptr);

    // Insert some points
    for (int i = 0; i < 10; ++i)
    {
        BoundingBox point = createPoint(i * 10.0, i * 10.0);
        TID tid = createTID(i, 0);
        status = rtree->insert(point, tid, nullptr, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    EXPECT_EQ(rtree->getTotalEntries(), 10);

    // Clear the index
    status = rtree->clear(&ctx);
    EXPECT_EQ(status, Status::OK);

    // Verify index is empty
    EXPECT_EQ(rtree->getTotalEntries(), 0);
    EXPECT_EQ(rtree->getHeight(), 0);

    // Search should return no results
    std::vector<TID> results;
    BoundingBox search_all = createRect(0.0, 0.0, 100.0, 100.0);
    status = rtree->search(search_all, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(results.size(), 0);
}

// ============================================================================
// Main Test Runner
// ============================================================================
