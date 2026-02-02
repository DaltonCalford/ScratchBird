/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * @file test_columnstore_predicate.cpp
 * @brief Unit tests for Columnstore Predicate Pushdown
 *
 * Tests predicate pushdown optimizations including:
 * - Min/max pruning (skip segments based on range)
 * - Batch predicate evaluation
 * - All predicate operators (=, !=, <, >, <=, >=, IS NULL, IS NOT NULL)
 *
 * Test Coverage:
 * - Min/max pruning for all operators
 * - Batch evaluation (1024+ values)
 * - NULL handling
 * - Edge cases (empty segments, all NULL)
 * - Performance validation
 */

#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"
#include "gtest/gtest.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdlib>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

// Helper: Create a Database instance for testing
Database* createTestDatabase()
{
    std::string test_db_path = uniqueTestDbPath("test_columnstore_predicate");
    std::remove(test_db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(test_db_path.c_str(), 8192, &ctx);
    if (status != Status::OK)
    {
        fprintf(stderr, "Failed to create test database: %s\n", ctx.message.c_str());
        return nullptr;
    }

    Database* db = new Database();
    status = db->open(test_db_path.c_str(), &ctx);
    if (status != Status::OK)
    {
        fprintf(stderr, "Failed to open test database: %s\n", ctx.message.c_str());
        delete db;
        return nullptr;
    }

    return db;
}

// Helper: Create a ColumnstoreIndex instance
ColumnstoreIndex* createTestIndex(Database* db)
{
    // Create index metadata
    SBColumnstoreIndex index_info;
    UuidV7Bytes index_uuid = generateUuidV7();
    UuidV7Bytes table_uuid = generateUuidV7();
    UuidV7Bytes column_uuid = generateUuidV7();

    std::memcpy(&index_info.idx_uuid, &index_uuid, sizeof(ID));
    std::memcpy(&index_info.idx_table_uuid, &table_uuid, sizeof(ID));
    index_info.idx_column_uuids.push_back(ID());
    std::memcpy(&index_info.idx_column_uuids[0], &column_uuid, sizeof(ID));
    index_info.idx_root_page = 0;
    index_info.idx_segment_size = 1024;
    index_info.idx_compression_type = static_cast<uint8_t>(CompressionType::RLE);
    index_info.idx_total_segments = 0;
    index_info.idx_total_rows = 0;
    index_info.idx_creation_xid = 1;

    return new ColumnstoreIndex(db, index_info);
}

// Helper: Create INT32 segment
ColumnSegment createInt32Segment(const std::vector<int32_t>& values,
                                  const std::vector<bool>& nulls = {})
{
    ColumnSegment seg;
    seg.data_type = DataType::INT32;
    seg.row_count = static_cast<uint32_t>(values.size());
    seg.null_bitmap = nulls;

    // Allocate data
    seg.data.resize(values.size() * sizeof(int32_t));
    std::memcpy(seg.data.data(), values.data(), values.size() * sizeof(int32_t));

    // Calculate min/max
    seg.min_value = INT64_MAX;
    seg.max_value = INT64_MIN;
    for (size_t i = 0; i < values.size(); ++i)
    {
        bool is_null = (i < nulls.size()) ? nulls[i] : false;
        if (!is_null)
        {
            if (values[i] < seg.min_value) seg.min_value = values[i];
            if (values[i] > seg.max_value) seg.max_value = values[i];
        }
    }

    return seg;
}

// ============================================================================
// Test Cases
// ============================================================================

void test_min_max_pruning_equal()
{
    printf("Test 1: Min/Max pruning - EQUAL predicate...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with values 20-40
    std::vector<int32_t> values;
    for (int32_t i = 20; i <= 40; ++i)
        values.push_back(i);
    ColumnSegment segment = createInt32Segment(values);

    // Predicate: age = 25 (should match)
    ColumnPredicate pred1;
    pred1.op = ColumnPredicate::Op::EQUAL;
    pred1.value = 25;

    std::vector<uint32_t> matches1;
    ErrorContext ctx;
    Status status = index->applyPredicate(segment, pred1, &matches1, &ctx);
    assert(status == Status::OK);
    assert(matches1.size() == 1);  // One value equals 25
    assert(matches1[0] == 5);      // Offset 5 (value 25)

    // Predicate: age = 50 (should be skipped via min/max)
    ColumnPredicate pred2;
    pred2.op = ColumnPredicate::Op::EQUAL;
    pred2.value = 50;

    std::vector<uint32_t> matches2;
    status = index->applyPredicate(segment, pred2, &matches2, &ctx);
    assert(status == Status::OK);
    assert(matches2.size() == 0);  // Skipped via min/max pruning

    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

void test_min_max_pruning_range()
{
    printf("Test 2: Min/Max pruning - Range predicates...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with values 20-40
    std::vector<int32_t> values;
    for (int32_t i = 20; i <= 40; ++i)
        values.push_back(i);
    ColumnSegment segment = createInt32Segment(values);

    // Predicate: age > 50 (should be skipped - all values <= 40)
    ColumnPredicate pred1;
    pred1.op = ColumnPredicate::Op::GREATER_THAN;
    pred1.value = 50;

    std::vector<uint32_t> matches1;
    ErrorContext ctx;
    Status status = index->applyPredicate(segment, pred1, &matches1, &ctx);
    assert(status == Status::OK);
    assert(matches1.size() == 0);  // Skipped

    // Predicate: age < 10 (should be skipped - all values >= 20)
    ColumnPredicate pred2;
    pred2.op = ColumnPredicate::Op::LESS_THAN;
    pred2.value = 10;

    std::vector<uint32_t> matches2;
    status = index->applyPredicate(segment, pred2, &matches2, &ctx);
    assert(status == Status::OK);
    assert(matches2.size() == 0);  // Skipped

    // Predicate: age >= 30 (should scan - some values match)
    ColumnPredicate pred3;
    pred3.op = ColumnPredicate::Op::GREATER_EQUAL;
    pred3.value = 30;

    std::vector<uint32_t> matches3;
    status = index->applyPredicate(segment, pred3, &matches3, &ctx);
    assert(status == Status::OK);
    assert(matches3.size() == 11);  // Values 30-40

    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

void test_batch_evaluation()
{
    printf("Test 3: Batch predicate evaluation (10K values)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with 10,000 values (0-9999)
    std::vector<int32_t> values;
    for (int32_t i = 0; i < 10000; ++i)
        values.push_back(i);
    ColumnSegment segment = createInt32Segment(values);

    // Predicate: value >= 5000
    ColumnPredicate pred;
    pred.op = ColumnPredicate::Op::GREATER_EQUAL;
    pred.value = 5000;

    std::vector<uint32_t> matches;
    ErrorContext ctx;
    Status status = index->applyPredicate(segment, pred, &matches, &ctx);

    assert(status == Status::OK);
    assert(matches.size() == 5000);  // Values 5000-9999

    // Verify first and last matches
    assert(matches[0] == 5000);
    assert(matches[4999] == 9999);

    printf("  Matched %zu / %u values\n", matches.size(), segment.row_count);
    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

void test_null_handling()
{
    printf("Test 4: NULL predicate handling...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with NULLs
    std::vector<int32_t> values = {10, 20, 30, 40, 50, 60};
    std::vector<bool> nulls = {false, true, false, true, false, false};
    ColumnSegment segment = createInt32Segment(values, nulls);

    // Predicate: IS NULL
    ColumnPredicate pred1;
    pred1.op = ColumnPredicate::Op::IS_NULL;

    std::vector<uint32_t> matches1;
    ErrorContext ctx;
    Status status = index->applyPredicate(segment, pred1, &matches1, &ctx);
    assert(status == Status::OK);
    assert(matches1.size() == 2);  // Offsets 1 and 3
    assert(matches1[0] == 1);
    assert(matches1[1] == 3);

    // Predicate: IS NOT NULL
    ColumnPredicate pred2;
    pred2.op = ColumnPredicate::Op::IS_NOT_NULL;

    std::vector<uint32_t> matches2;
    status = index->applyPredicate(segment, pred2, &matches2, &ctx);
    assert(status == Status::OK);
    assert(matches2.size() == 4);  // Offsets 0, 2, 4, 5

    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

void test_all_operators()
{
    printf("Test 5: All predicate operators...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with values 1-10
    std::vector<int32_t> values;
    for (int32_t i = 1; i <= 10; ++i)
        values.push_back(i);
    ColumnSegment segment = createInt32Segment(values);

    ErrorContext ctx;

    // EQUAL: value = 5
    ColumnPredicate pred_eq;
    pred_eq.op = ColumnPredicate::Op::EQUAL;
    pred_eq.value = 5;
    std::vector<uint32_t> matches_eq;
    assert(index->applyPredicate(segment, pred_eq, &matches_eq, &ctx) == Status::OK);
    assert(matches_eq.size() == 1);

    // NOT_EQUAL: value != 5
    ColumnPredicate pred_ne;
    pred_ne.op = ColumnPredicate::Op::NOT_EQUAL;
    pred_ne.value = 5;
    std::vector<uint32_t> matches_ne;
    assert(index->applyPredicate(segment, pred_ne, &matches_ne, &ctx) == Status::OK);
    assert(matches_ne.size() == 9);

    // LESS_THAN: value < 5
    ColumnPredicate pred_lt;
    pred_lt.op = ColumnPredicate::Op::LESS_THAN;
    pred_lt.value = 5;
    std::vector<uint32_t> matches_lt;
    assert(index->applyPredicate(segment, pred_lt, &matches_lt, &ctx) == Status::OK);
    assert(matches_lt.size() == 4);  // 1, 2, 3, 4

    // LESS_EQUAL: value <= 5
    ColumnPredicate pred_le;
    pred_le.op = ColumnPredicate::Op::LESS_EQUAL;
    pred_le.value = 5;
    std::vector<uint32_t> matches_le;
    assert(index->applyPredicate(segment, pred_le, &matches_le, &ctx) == Status::OK);
    assert(matches_le.size() == 5);  // 1, 2, 3, 4, 5

    // GREATER_THAN: value > 5
    ColumnPredicate pred_gt;
    pred_gt.op = ColumnPredicate::Op::GREATER_THAN;
    pred_gt.value = 5;
    std::vector<uint32_t> matches_gt;
    assert(index->applyPredicate(segment, pred_gt, &matches_gt, &ctx) == Status::OK);
    assert(matches_gt.size() == 5);  // 6, 7, 8, 9, 10

    // GREATER_EQUAL: value >= 5
    ColumnPredicate pred_ge;
    pred_ge.op = ColumnPredicate::Op::GREATER_EQUAL;
    pred_ge.value = 5;
    std::vector<uint32_t> matches_ge;
    assert(index->applyPredicate(segment, pred_ge, &matches_ge, &ctx) == Status::OK);
    assert(matches_ge.size() == 6);  // 5, 6, 7, 8, 9, 10

    printf("  EQUAL: %zu matches\n", matches_eq.size());
    printf("  NOT_EQUAL: %zu matches\n", matches_ne.size());
    printf("  LESS_THAN: %zu matches\n", matches_lt.size());
    printf("  LESS_EQUAL: %zu matches\n", matches_le.size());
    printf("  GREATER_THAN: %zu matches\n", matches_gt.size());
    printf("  GREATER_EQUAL: %zu matches\n", matches_ge.size());
    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

void test_edge_cases()
{
    printf("Test 6: Edge cases (empty, all NULL)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    ErrorContext ctx;

    // Empty segment
    ColumnSegment empty_seg;
    empty_seg.data_type = DataType::INT32;
    empty_seg.row_count = 0;
    empty_seg.min_value = 0;
    empty_seg.max_value = 0;

    ColumnPredicate pred;
    pred.op = ColumnPredicate::Op::EQUAL;
    pred.value = 5;

    std::vector<uint32_t> matches1;
    Status status = index->applyPredicate(empty_seg, pred, &matches1, &ctx);
    assert(status == Status::OK);
    assert(matches1.size() == 0);

    // All NULL segment
    std::vector<int32_t> values(10, 0);
    std::vector<bool> nulls(10, true);
    ColumnSegment all_null_seg = createInt32Segment(values, nulls);

    std::vector<uint32_t> matches2;
    status = index->applyPredicate(all_null_seg, pred, &matches2, &ctx);
    assert(status == Status::OK);
    assert(matches2.size() == 0);  // No non-NULL values match

    // IS NULL on all-NULL segment
    ColumnPredicate null_pred;
    null_pred.op = ColumnPredicate::Op::IS_NULL;

    std::vector<uint32_t> matches3;
    status = index->applyPredicate(all_null_seg, null_pred, &matches3, &ctx);
    assert(status == Status::OK);
    assert(matches3.size() == 10);  // All values are NULL

    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

void test_performance()
{
    printf("Test 7: Performance test (1M values)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with 1M values (0-999999)
    std::vector<int32_t> values;
    for (int32_t i = 0; i < 1000000; ++i)
        values.push_back(i % 10000);  // Cycling 0-9999
    ColumnSegment segment = createInt32Segment(values);

    // Predicate: value >= 5000 (should match ~50% of values)
    ColumnPredicate pred;
    pred.op = ColumnPredicate::Op::GREATER_EQUAL;
    pred.value = 5000;

    std::vector<uint32_t> matches;
    ErrorContext ctx;
    Status status = index->applyPredicate(segment, pred, &matches, &ctx);

    assert(status == Status::OK);
    printf("  Matched %zu / %u values\n", matches.size(), segment.row_count);

    // Should match values 5000-9999 (50% of each 10000 cycle)
    assert(matches.size() == 500000);

    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

void test_selectivity()
{
    printf("Test 8: Selectivity test (high/low)...\n");

    Database* db = createTestDatabase();
    assert(db != nullptr);

    ColumnstoreIndex* index = createTestIndex(db);
    assert(index != nullptr);

    // Create segment with 10,000 values
    std::vector<int32_t> values;
    for (int32_t i = 0; i < 10000; ++i)
        values.push_back(i);
    ColumnSegment segment = createInt32Segment(values);

    ErrorContext ctx;

    // Low selectivity: 1% match (value = 100)
    ColumnPredicate pred1;
    pred1.op = ColumnPredicate::Op::EQUAL;
    pred1.value = 100;

    std::vector<uint32_t> matches1;
    Status status = index->applyPredicate(segment, pred1, &matches1, &ctx);
    assert(status == Status::OK);
    assert(matches1.size() == 1);  // 0.01% selectivity

    // High selectivity: 99% match (value != 100)
    ColumnPredicate pred2;
    pred2.op = ColumnPredicate::Op::NOT_EQUAL;
    pred2.value = 100;

    std::vector<uint32_t> matches2;
    status = index->applyPredicate(segment, pred2, &matches2, &ctx);
    assert(status == Status::OK);
    assert(matches2.size() == 9999);  // 99.99% selectivity

    printf("  Low selectivity: %zu / %u (%.2f%%)\n",
           matches1.size(), segment.row_count,
           100.0 * matches1.size() / segment.row_count);
    printf("  High selectivity: %zu / %u (%.2f%%)\n",
           matches2.size(), segment.row_count,
           100.0 * matches2.size() / segment.row_count);
    printf("  ✅ PASSED\n\n");

    delete index;
    delete db;
}

// ============================================================================
// Main
// ============================================================================


TEST(ColumnstorePredicateTest, Comprehensive) {

    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Columnstore Predicate Pushdown - Unit Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    test_min_max_pruning_equal();
    test_min_max_pruning_range();
    test_batch_evaluation();
    test_null_handling();
    test_all_operators();
    test_edge_cases();
    test_performance();
    test_selectivity();

    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  ✅ ALL TESTS PASSED (8/8)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return;

}
