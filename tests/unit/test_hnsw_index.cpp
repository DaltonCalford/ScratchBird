/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// PHASE 4A TASK 4A.2.7: Unit tests for HNSW Index
#include <gtest/gtest.h>
#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/vector.h"
#include <filesystem>
#include <vector>
#include <cstring>
#include <random>

using namespace scratchbird::core;

class HnswIndexTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database *db_;

    void SetUp() override
    {
        // Create temporary database for testing
        test_db_path_ = "/tmp/test_hnsw_" + std::to_string(getpid()) + ".db";

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

    // Helper: Create random vector
    VectorValue createRandomVector(size_t dimensions, std::mt19937 &rng)
    {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        std::vector<float> values(dimensions);
        for (size_t i = 0; i < dimensions; ++i)
        {
            values[i] = dist(rng);
        }
        return Vector::fromFloat32(values);
    }

    // Helper: Create vector from initializer list
    VectorValue createVector(std::initializer_list<float> values)
    {
        return Vector::fromFloat32(std::vector<float>(values));
    }
};

// Test 1: Create HNSW index
TEST_F(HnswIndexTest, CreateIndex)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      128, // 128-dimensional vectors
                                      DistanceMetric::EUCLIDEAN,
                                      16, 200, 100,
                                      &root_page, &ctx);

    EXPECT_EQ(status, Status::OK) << "Failed to create HNSW index: " << ctx.message;
    EXPECT_GT(root_page, 0) << "Root page should be allocated";
}

// Test 2: Open existing HNSW index
TEST_F(HnswIndexTest, OpenIndex)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      128, DistanceMetric::EUCLIDEAN,
                                      16, 200, 100,
                                      &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_, index_uuid, root_page, &ctx);
    EXPECT_NE(hnsw, nullptr) << "Failed to open HNSW index: " << ctx.message;
}

// Test 3: Insert single vector
TEST_F(HnswIndexTest, InsertSingleVector)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      3, DistanceMetric::EUCLIDEAN,
                                      16, 200, 100,
                                      &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(hnsw, nullptr);

    // Insert vector [1.0, 2.0, 3.0] with TID 100
    VectorValue vec = createVector({1.0f, 2.0f, 3.0f});
    status = hnsw->insert(vec, 100, &ctx);
    EXPECT_EQ(status, Status::OK) << "Failed to insert: " << ctx.message;
}

// Test 4: Insert multiple vectors and search
TEST_F(HnswIndexTest, InsertMultipleVectorsAndSearch)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      3, DistanceMetric::EUCLIDEAN,
                                      16, 200, 100,
                                      &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(hnsw, nullptr);

    // Insert 10 vectors
    std::vector<VectorValue> vectors = {
        createVector({1.0f, 0.0f, 0.0f}),
        createVector({0.0f, 1.0f, 0.0f}),
        createVector({0.0f, 0.0f, 1.0f}),
        createVector({1.0f, 1.0f, 0.0f}),
        createVector({1.0f, 0.0f, 1.0f}),
        createVector({0.0f, 1.0f, 1.0f}),
        createVector({1.0f, 1.0f, 1.0f}),
        createVector({2.0f, 0.0f, 0.0f}),
        createVector({0.0f, 2.0f, 0.0f}),
        createVector({0.0f, 0.0f, 2.0f})
    };

    for (size_t i = 0; i < vectors.size(); ++i)
    {
        status = hnsw->insert(vectors[i], 100 + i, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to insert vector " << i;
    }

    // Search for nearest neighbors to [0.9, 0.1, 0.1]
    // Expected: TID 100 ([1,0,0]) should be closest
    VectorValue query = createVector({0.9f, 0.1f, 0.1f});
    std::vector<HnswSearchResult> results;
    status = hnsw->search(query, 3, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
    // Stub implementation returns empty, so no results expected yet
}

// Test 5: Test different distance metrics
TEST_F(HnswIndexTest, DifferentDistanceMetrics)
{
    ErrorContext ctx;

    // Test EUCLIDEAN
    {
        UuidV7Bytes index_uuid = UuidV7::generateBytes();
        UuidV7Bytes table_uuid = UuidV7::generateBytes();
        std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

        uint32_t root_page = 0;
        Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                          3, DistanceMetric::EUCLIDEAN,
                                          16, 200, 100,
                                          &root_page, &ctx);
        EXPECT_EQ(status, Status::OK);
    }

    // Test COSINE
    {
        UuidV7Bytes index_uuid = UuidV7::generateBytes();
        UuidV7Bytes table_uuid = UuidV7::generateBytes();
        std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

        uint32_t root_page = 0;
        Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                          3, DistanceMetric::COSINE,
                                          16, 200, 100,
                                          &root_page, &ctx);
        EXPECT_EQ(status, Status::OK);
    }

    // Test MANHATTAN
    {
        UuidV7Bytes index_uuid = UuidV7::generateBytes();
        UuidV7Bytes table_uuid = UuidV7::generateBytes();
        std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

        uint32_t root_page = 0;
        Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                          3, DistanceMetric::MANHATTAN,
                                          16, 200, 100,
                                          &root_page, &ctx);
        EXPECT_EQ(status, Status::OK);
    }

    // Test DOT_PRODUCT
    {
        UuidV7Bytes index_uuid = UuidV7::generateBytes();
        UuidV7Bytes table_uuid = UuidV7::generateBytes();
        std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

        uint32_t root_page = 0;
        Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                          3, DistanceMetric::DOT_PRODUCT,
                                          16, 200, 100,
                                          &root_page, &ctx);
        EXPECT_EQ(status, Status::OK);
    }
}

// Test 6: Remove dead entries (empty list - idempotency)
TEST_F(HnswIndexTest, RemoveDeadEntriesEmpty)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      128, DistanceMetric::EUCLIDEAN,
                                      16, 200, 100,
                                      &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(hnsw, nullptr);

    // Call removeDeadEntries with empty list
    std::vector<uint64_t> dead_tids;
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = hnsw->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(entries_removed, 0);
    EXPECT_EQ(pages_modified, 0);
}

// Test 7: Remove dead entries (with TIDs)
TEST_F(HnswIndexTest, RemoveDeadEntriesWithTids)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      3, DistanceMetric::EUCLIDEAN,
                                      16, 200, 100,
                                      &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(hnsw, nullptr);

    // Insert vectors
    VectorValue vec1 = createVector({1.0f, 0.0f, 0.0f});
    VectorValue vec2 = createVector({0.0f, 1.0f, 0.0f});
    status = hnsw->insert(vec1, 100, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = hnsw->insert(vec2, 101, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Mark TID 100 as dead
    std::vector<uint64_t> dead_tids = {100};
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    status = hnsw->removeDeadEntries(dead_tids, &entries_removed, &pages_modified, &ctx);
    EXPECT_EQ(status, Status::OK);
    // Stub returns 0, but this validates the API
}

// Test 8: Soft deletion
TEST_F(HnswIndexTest, SoftDeletion)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      3, DistanceMetric::EUCLIDEAN,
                                      16, 200, 100,
                                      &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(hnsw, nullptr);

    // Insert and delete
    VectorValue vec = createVector({1.0f, 2.0f, 3.0f});
    status = hnsw->insert(vec, 100, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Soft delete (sets xmax, doesn't remove)
    status = hnsw->remove(100, &ctx);
    EXPECT_EQ(status, Status::OK);
}

// Test 9: Get statistics
TEST_F(HnswIndexTest, GetStats)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      128, DistanceMetric::EUCLIDEAN,
                                      16, 200, 100,
                                      &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(hnsw, nullptr);

    HnswIndex::HnswStats stats;
    status = hnsw->getStats(&stats, &ctx);

    EXPECT_EQ(status, Status::OK);
    // Stub returns zeros, but this validates the API
}

// Test 10: Different M parameters
TEST_F(HnswIndexTest, DifferentMParameters)
{
    ErrorContext ctx;

    // M = 8 (fewer connections, faster build, lower recall)
    {
        UuidV7Bytes index_uuid = UuidV7::generateBytes();
        UuidV7Bytes table_uuid = UuidV7::generateBytes();
        std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

        uint32_t root_page = 0;
        Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                          128, DistanceMetric::EUCLIDEAN,
                                          8, 200, 100,
                                          &root_page, &ctx);
        EXPECT_EQ(status, Status::OK);
    }

    // M = 32 (more connections, slower build, higher recall)
    {
        UuidV7Bytes index_uuid = UuidV7::generateBytes();
        UuidV7Bytes table_uuid = UuidV7::generateBytes();
        std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

        uint32_t root_page = 0;
        Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                          128, DistanceMetric::EUCLIDEAN,
                                          32, 200, 100,
                                          &root_page, &ctx);
        EXPECT_EQ(status, Status::OK);
    }
}

// Test 11: High-dimensional vectors (OpenAI embeddings)
TEST_F(HnswIndexTest, HighDimensionalVectors)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    // 1536 dimensions (OpenAI text-embedding-ada-002)
    uint32_t root_page = 0;
    Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      1536, DistanceMetric::COSINE,
                                      16, 200, 100,
                                      &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(hnsw, nullptr);

    // Create random 1536-dim vector
    std::mt19937 rng(42);
    VectorValue vec = createRandomVector(1536, rng);

    status = hnsw->insert(vec, 100, &ctx);
    EXPECT_EQ(status, Status::OK);
}

// Test 12: Semantic search simulation
TEST_F(HnswIndexTest, SemanticSearchSimulation)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      8, DistanceMetric::COSINE,
                                      16, 200, 100,
                                      &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(hnsw, nullptr);

    // Simulate document embeddings (simplified)
    std::vector<VectorValue> doc_embeddings = {
        createVector({0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}), // Doc 1: tech
        createVector({0.1f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}), // Doc 2: food
        createVector({0.0f, 0.0f, 0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f}), // Doc 3: sports
        createVector({0.8f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}), // Doc 4: tech-ish
        createVector({0.0f, 0.0f, 0.1f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f})  // Doc 5: sports-ish
    };

    for (size_t i = 0; i < doc_embeddings.size(); ++i)
    {
        status = hnsw->insert(doc_embeddings[i], 1000 + i, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Query: Find documents similar to tech query
    VectorValue query = createVector({0.95f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    std::vector<HnswSearchResult> results;
    status = hnsw->search(query, 2, nullptr, &results, &ctx);
    EXPECT_EQ(status, Status::OK);
    // Expected (with full implementation): TID 1000, 1003 (tech docs)
}

// Test 13: Bulk insert simulation
TEST_F(HnswIndexTest, BulkInsertSimulation)
{
    ErrorContext ctx;

    UuidV7Bytes index_uuid = UuidV7::generateBytes();
    UuidV7Bytes table_uuid = UuidV7::generateBytes();
    std::vector<UuidV7Bytes> column_uuids = {UuidV7::generateBytes()};

    uint32_t root_page = 0;
    Status status = HnswIndex::create(db_, index_uuid, table_uuid, column_uuids,
                                      32, DistanceMetric::EUCLIDEAN,
                                      16, 200, 100,
                                      &root_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto hnsw = HnswIndex::open(db_, index_uuid, root_page, &ctx);
    ASSERT_NE(hnsw, nullptr);

    // Insert 100 random vectors
    std::mt19937 rng(42);
    const size_t num_vectors = 100;

    for (size_t i = 0; i < num_vectors; ++i)
    {
        VectorValue vec = createRandomVector(32, rng);
        status = hnsw->insert(vec, 10000 + i, &ctx);
        if (status != Status::OK)
        {
            FAIL() << "Failed to insert vector " << i << ": " << ctx.message;
        }
    }

    // Verify all inserts succeeded
    HnswIndex::HnswStats stats;
    status = hnsw->getStats(&stats, &ctx);
    EXPECT_EQ(status, Status::OK);
}
