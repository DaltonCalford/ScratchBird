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
 * Columnstore Phase 5 Batch Processing Tests
 *
 * Replacement tests focused on correctness with light performance expectations.
 * These tests validate:
 * - RLE compression/decompression correctness
 * - Predicate evaluation correctness
 * - Batch scan iterator correctness
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using scratchbird::testing::TestDatabaseFile;

namespace {

class ColumnstoreBatchPerfTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("columnstore_batch_perf", ".db");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 8192, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;
    }

    void TearDown() override {
        db_.close();
        db_file_.reset();
    }

    std::unique_ptr<ColumnstoreIndex> createIndex(
        const std::vector<UuidV7Bytes>& columns,
        uint32_t segment_size,
        CompressionType compression) {
        UuidV7Bytes index_uuid = generateUuidV7();
        UuidV7Bytes table_uuid = generateUuidV7();
        uint32_t root_page = 0;
        ErrorContext ctx;
        Status status = ColumnstoreIndex::create(
            &db_, index_uuid, table_uuid, columns, segment_size, compression, &root_page, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        if (status != Status::OK) {
            return nullptr;
        }
        auto index = ColumnstoreIndex::open(&db_, index_uuid, root_page, segment_size, &ctx);
        EXPECT_NE(index, nullptr);
        return index;
    }

    ColumnSegment makeInt32Segment(const std::vector<int32_t>& values) {
        ColumnSegment segment;
        segment.data_type = DataType::INT32;
        segment.row_count = static_cast<uint32_t>(values.size());
        segment.data.resize(values.size() * sizeof(int32_t));
        std::memcpy(segment.data.data(), values.data(), values.size() * sizeof(int32_t));
        segment.min_value = values.empty() ? 0 : *std::min_element(values.begin(), values.end());
        segment.max_value = values.empty() ? 0 : *std::max_element(values.begin(), values.end());
        return segment;
    }

    Database db_{};
    std::unique_ptr<TestDatabaseFile> db_file_;
};

TEST_F(ColumnstoreBatchPerfTest, RLECompressionRoundTrip) {
    UuidV7Bytes column_uuid = generateUuidV7();
    auto index = createIndex({column_uuid}, 1024, CompressionType::RLE);
    ASSERT_NE(index, nullptr);

    std::vector<int32_t> values;
    values.reserve(2000);
    for (int i = 0; i < 2000; ++i) {
        values.push_back(i % 10);
    }

    ColumnSegment input = makeInt32Segment(values);

    std::vector<uint8_t> compressed;
    ErrorContext ctx;
    Status status = index->compressRLE(input, &compressed, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_FALSE(compressed.empty());

    ColumnSegment output;
    status = index->decompressRLE(compressed, DataType::INT32, input.row_count, &output, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_EQ(output.row_count, input.row_count);
    ASSERT_EQ(output.data.size(), input.data.size());
    EXPECT_EQ(std::memcmp(output.data.data(), input.data.data(), input.data.size()), 0);
}

TEST_F(ColumnstoreBatchPerfTest, PredicateEvaluationMatchesExpectedCount) {
    UuidV7Bytes column_uuid = generateUuidV7();
    auto index = createIndex({column_uuid}, 1024, CompressionType::NONE);
    ASSERT_NE(index, nullptr);

    std::vector<int32_t> values;
    values.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        values.push_back(i);
    }

    ColumnSegment segment = makeInt32Segment(values);

    ColumnPredicate predicate;
    predicate.op = ColumnPredicate::Op::GREATER_THAN;
    predicate.value = 500;

    std::vector<uint32_t> matching_offsets;
    ErrorContext ctx;
    Status status = index->applyPredicate(segment, predicate, &matching_offsets, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    const size_t expected = 499;  // Values 501..999
    EXPECT_EQ(matching_offsets.size(), expected);
}

TEST_F(ColumnstoreBatchPerfTest, BatchScanIteratorFindsMatches) {
    UuidV7Bytes column_uuid = generateUuidV7();
    auto index = createIndex({column_uuid}, 256, CompressionType::RLE);
    ASSERT_NE(index, nullptr);

    ErrorContext ctx;
    const uint32_t total_rows = 2048;
    for (uint32_t i = 0; i < total_rows; ++i) {
        int32_t value = static_cast<int32_t>(i % 100);
        TID tid{0, static_cast<uint64_t>(i), 0};
        Status status = index->insert(column_uuid, tid, &value, sizeof(int32_t), false, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
    }

    ASSERT_EQ(index->flushSegment(column_uuid, &ctx), Status::OK) << ctx.message;

    ColumnPredicate predicate;
    predicate.op = ColumnPredicate::Op::GREATER_THAN;
    predicate.value = 50;

    ColumnScanIterator iter;
    uint64_t xid = db_.transaction_manager()->getCurrentXid();
    ASSERT_EQ(index->beginScan(column_uuid, &predicate, xid, &iter, &ctx), Status::OK)
        << ctx.message;

    uint32_t total_matches = 0;
    while (!iter.scan_complete) {
        ColumnScanBatch batch;
        ASSERT_EQ(index->scanNext(&iter, &batch, &ctx), Status::OK) << ctx.message;
        total_matches += batch.count;
    }

    ASSERT_EQ(index->endScan(&iter, &ctx), Status::OK) << ctx.message;

    const uint32_t expected_per_100 = 49;  // Values 51..99
    const uint32_t expected = (total_rows / 100) * expected_per_100;
    EXPECT_EQ(total_matches, expected);
}

}  // namespace
