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
 * Columnstore Phase 7 Load Tests
 *
 * Replacement tests focused on correctness and stable runtimes:
 * - Insert and flush with expected stats
 * - Predicate scan with deterministic match count
 * - Multi-column inserts and flush
 */

#include <gtest/gtest.h>
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

class ColumnstoreLoadTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("columnstore_load", ".db");

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
        CompressionType compression,
        uint32_t* root_out = nullptr) {
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
        if (root_out) {
            *root_out = root_page;
        }
        auto index = ColumnstoreIndex::open(&db_, index_uuid, root_page, segment_size, &ctx);
        EXPECT_NE(index, nullptr);
        return index;
    }

    Database db_{};
    std::unique_ptr<TestDatabaseFile> db_file_;
};

TEST_F(ColumnstoreLoadTest, InsertAndFlushUpdatesStats) {
    UuidV7Bytes column_uuid = generateUuidV7();
    auto index = createIndex({column_uuid}, 512, CompressionType::RLE);
    ASSERT_NE(index, nullptr);

    ErrorContext ctx;
    const uint32_t total_rows = 5000;
    for (uint32_t i = 0; i < total_rows; ++i) {
        int32_t value = static_cast<int32_t>(i % 100);
        TID tid{0, static_cast<uint64_t>(i), 0};
        ASSERT_EQ(index->insert(column_uuid, tid, &value, sizeof(int32_t), false, &ctx),
                  Status::OK) << ctx.message;
    }

    ASSERT_EQ(index->flushSegment(column_uuid, &ctx), Status::OK) << ctx.message;

    ColumnstoreIndex::ColumnstoreStats stats{};
    ASSERT_EQ(index->getStats(&stats, &ctx), Status::OK) << ctx.message;
    EXPECT_GT(stats.total_segments, 0u);
    EXPECT_EQ(stats.total_rows, total_rows);
    EXPECT_GT(stats.compression_ratio, 1.0);
}

TEST_F(ColumnstoreLoadTest, PredicateScanReturnsExpectedMatches) {
    UuidV7Bytes column_uuid = generateUuidV7();
    auto index = createIndex({column_uuid}, 256, CompressionType::NONE);
    ASSERT_NE(index, nullptr);

    ErrorContext ctx;
    const uint32_t total_rows = 2000;
    for (uint32_t i = 0; i < total_rows; ++i) {
        int32_t value = static_cast<int32_t>(i % 100);  // 0..99 repeated 20x
        TID tid{0, static_cast<uint64_t>(i), 0};
        ASSERT_EQ(index->insert(column_uuid, tid, &value, sizeof(int32_t), false, &ctx),
                  Status::OK) << ctx.message;
    }

    ASSERT_EQ(index->flushSegment(column_uuid, &ctx), Status::OK) << ctx.message;

    ColumnPredicate predicate;
    predicate.op = ColumnPredicate::Op::GREATER_THAN;
    predicate.value = 50;  // matches 51..99

    ColumnScanIterator iter;
    uint64_t xid = db_.transaction_manager()->getCurrentXid();
    ASSERT_EQ(index->beginScan(column_uuid, &predicate, xid, &iter, &ctx), Status::OK)
        << ctx.message;

    uint32_t matches = 0;
    while (!iter.scan_complete) {
        ColumnScanBatch batch;
        ASSERT_EQ(index->scanNext(&iter, &batch, &ctx), Status::OK) << ctx.message;
        matches += batch.count;
    }

    ASSERT_EQ(index->endScan(&iter, &ctx), Status::OK) << ctx.message;

    const uint32_t expected_per_100 = 49;  // 51..99
    const uint32_t expected = (total_rows / 100) * expected_per_100;
    EXPECT_EQ(matches, expected);
}

TEST_F(ColumnstoreLoadTest, MultiColumnInsertAndFlush) {
    UuidV7Bytes column1 = generateUuidV7();
    UuidV7Bytes column2 = generateUuidV7();
    UuidV7Bytes column3 = generateUuidV7();

    auto index = createIndex({column1, column2, column3}, 256, CompressionType::RLE);
    ASSERT_NE(index, nullptr);

    ErrorContext ctx;
    const uint32_t total_rows = 1500;
    for (uint32_t i = 0; i < total_rows; ++i) {
        TID tid{0, static_cast<uint64_t>(i), 0};
        int32_t v1 = static_cast<int32_t>(i);
        int32_t v2 = static_cast<int32_t>(i % 10);
        int32_t v3 = static_cast<int32_t>((i * 7919) % 1000);

        ASSERT_EQ(index->insert(column1, tid, &v1, sizeof(int32_t), false, &ctx),
                  Status::OK) << ctx.message;
        ASSERT_EQ(index->insert(column2, tid, &v2, sizeof(int32_t), false, &ctx),
                  Status::OK) << ctx.message;
        ASSERT_EQ(index->insert(column3, tid, &v3, sizeof(int32_t), false, &ctx),
                  Status::OK) << ctx.message;
    }

    ASSERT_EQ(index->flushSegment(column1, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(index->flushSegment(column2, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(index->flushSegment(column3, &ctx), Status::OK) << ctx.message;

    ColumnstoreIndex::ColumnstoreStats stats{};
    ASSERT_EQ(index->getStats(&stats, &ctx), Status::OK) << ctx.message;
    EXPECT_GT(stats.total_segments, 0u);
    EXPECT_GE(stats.total_rows, total_rows);
}

}  // namespace
