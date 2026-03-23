/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <gtest/gtest.h>

#include "scratchbird/core/btree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace scratchbird::core;
using namespace std::chrono;

namespace {

struct ScenarioMetrics
{
    std::string label;
    size_t entries = 0;
    size_t searches = 0;
    double insert_avg_us = 0.0;
    double search_avg_us = 0.0;
    uint64_t total_results = 0;
};

auto encodeUint32Key(uint32_t value) -> std::vector<uint8_t>
{
    std::vector<uint8_t> key(4);
    key[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    key[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    key[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    key[3] = static_cast<uint8_t>(value & 0xFF);
    return key;
}

auto encodeStringKey(const std::string &text) -> std::vector<uint8_t>
{
    return std::vector<uint8_t>(text.begin(), text.end());
}

auto makeRandomishKey(uint64_t seed) -> std::vector<uint8_t>
{
    auto next = [&seed]() -> uint64_t {
        seed += 0x9E3779B97F4A7C15ULL;
        uint64_t z = seed;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    };

    std::vector<uint8_t> key(24);
    for (size_t offset = 0; offset < key.size(); offset += sizeof(uint64_t))
    {
        const uint64_t value = next();
        std::memcpy(key.data() + offset, &value, sizeof(uint64_t));
    }
    return key;
}

void printScenarioMetrics(const ScenarioMetrics &metrics)
{
    std::cout << std::fixed << std::setprecision(3)
              << "BTREE_PROOF_SCENARIO"
              << ",label=" << metrics.label
              << ",entries=" << metrics.entries
              << ",searches=" << metrics.searches
              << ",insert_avg_us=" << metrics.insert_avg_us
              << ",search_avg_us=" << metrics.search_avg_us
              << ",total_results=" << metrics.total_results
              << std::endl;
}

} // namespace

class BTreeProofCorpusBenchmark : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_ =
            std::make_unique<scratchbird::testing::TestDatabaseFile>(
                "test_btree_proof_corpus");
        ErrorContext ctx;

        ASSERT_EQ(Database::create(test_db_->path(), 8192, &ctx), Status::OK)
            << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_->path(), &ctx), Status::OK) << ctx.message;
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
        }
        test_db_.reset();
    }

    auto allocateRootGpid(ErrorContext *ctx) -> GPID
    {
        auto *pm = db_ ? db_->page_manager() : nullptr;
        if (pm == nullptr)
        {
            if (ctx != nullptr)
            {
                ctx->message = "PageManager not available";
            }
            return 0;
        }

        GPID gpid = 0;
        if (pm->allocatePageInTablespace(PRIMARY_TABLESPACE_ID, &gpid, ctx) !=
            Status::OK)
        {
            return 0;
        }
        return gpid;
    }

    auto openFreshTree(ErrorContext *ctx) -> std::unique_ptr<BTree>
    {
        const UuidV7Bytes index_uuid = generateUuidV7();
        const UuidV7Bytes table_uuid = generateUuidV7();
        const std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};
        const GPID root_gpid = allocateRootGpid(ctx);
        if (root_gpid == 0)
        {
            return nullptr;
        }

        if (BTree::create(db_.get(),
                          index_uuid,
                          table_uuid,
                          column_uuids,
                          root_gpid,
                          ctx) != Status::OK)
        {
            return nullptr;
        }

        return BTree::open(db_.get(), index_uuid, root_gpid, ctx);
    }

    auto runScenario(const std::string &label,
                     const std::vector<std::vector<uint8_t>> &keys) -> ScenarioMetrics
    {
        ErrorContext ctx;
        auto btree = openFreshTree(&ctx);
        ScenarioMetrics metrics;
        metrics.label = label;
        metrics.entries = keys.size();
        metrics.searches = std::min<size_t>(keys.size(), 256);
        if (btree == nullptr)
        {
            ADD_FAILURE() << "openFreshTree failed in scenario " << label << ": "
                          << ctx.message;
            return metrics;
        }

        const auto insert_start = high_resolution_clock::now();
        for (size_t i = 0; i < keys.size(); ++i)
        {
            const TID tid =
                makeTID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i + 1), 1);
            const Status insert_status = btree->insert(keys[i], tid, 1, &ctx);
            if (insert_status != Status::OK)
            {
                ADD_FAILURE() << "insert failed in scenario " << label << ": "
                              << ctx.message;
                return metrics;
            }
        }
        const auto insert_end = high_resolution_clock::now();

        const size_t stride =
            std::max<size_t>(1, keys.size() / std::max<size_t>(1, metrics.searches));
        const auto search_start = high_resolution_clock::now();
        for (size_t i = 0; i < keys.size() && metrics.searches > 0;
             i += stride)
        {
            std::vector<TID> results;
            const Status status = btree->search(keys[i], 0, &results, &ctx);
            if (status != Status::OK)
            {
                ADD_FAILURE() << "search failed in scenario " << label << ": "
                              << ctx.message;
                return metrics;
            }
            metrics.total_results += results.size();
        }
        const auto search_end = high_resolution_clock::now();

        const auto insert_duration =
            duration_cast<microseconds>(insert_end - insert_start);
        const auto search_duration =
            duration_cast<microseconds>(search_end - search_start);
        metrics.insert_avg_us =
            metrics.entries == 0
                ? 0.0
                : insert_duration.count() / static_cast<double>(metrics.entries);
        metrics.search_avg_us =
            metrics.searches == 0
                ? 0.0
                : search_duration.count() / static_cast<double>(metrics.searches);

        printScenarioMetrics(metrics);

        EXPECT_GT(metrics.total_results, 0U);
        EXPECT_LT(metrics.insert_avg_us, 10000.0);
        EXPECT_LT(metrics.search_avg_us, 5000.0);
        return metrics;
    }

    std::unique_ptr<Database> db_;
    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_;
};

TEST_F(BTreeProofCorpusBenchmark, CompressionSensitiveAccessProfiles)
{
    std::vector<std::vector<uint8_t>> uuid_keys;
    uuid_keys.reserve(2048);
    for (size_t i = 0; i < 2048; ++i)
    {
        const auto uuid = generateUuidV7();
        uuid_keys.emplace_back(uuid.bytes.begin(), uuid.bytes.end());
    }

    std::vector<std::vector<uint8_t>> prefixed_string_keys;
    prefixed_string_keys.reserve(2048);
    for (size_t i = 0; i < 2048; ++i)
    {
        prefixed_string_keys.push_back(encodeStringKey(
            "tenant:000042:partition:20260319:index-proof:key:" +
            std::to_string(i)));
    }

    std::vector<std::vector<uint8_t>> random_keys;
    random_keys.reserve(2048);
    for (size_t i = 0; i < 2048; ++i)
    {
        random_keys.push_back(makeRandomishKey(
            0xC0FFEEULL + static_cast<uint64_t>(i) * 17ULL));
    }

    const auto uuid_metrics = runScenario("uuidv7_compressed", uuid_keys);
    const auto string_metrics =
        runScenario("string_prefix_compressed", prefixed_string_keys);
    const auto random_metrics = runScenario("random_uncompressed", random_keys);

    EXPECT_LT(uuid_metrics.search_avg_us, 5000.0);
    EXPECT_LT(string_metrics.search_avg_us, 5000.0);
    EXPECT_LT(random_metrics.search_avg_us, 5000.0);
}

TEST_F(BTreeProofCorpusBenchmark, RestartAnchorAndCompactionMaintenanceCorpus)
{
    ErrorContext ctx;
    auto btree = openFreshTree(&ctx);
    ASSERT_TRUE(btree != nullptr) << ctx.message;

    std::vector<std::vector<uint8_t>> keys;
    keys.reserve(4096);
    for (uint32_t i = 0; i < 4096; ++i)
    {
        keys.push_back(encodeStringKey(
            "restart-anchor-proof/common/prefix/block/" +
            std::to_string(i / 32) + "/row/" + std::to_string(i)));
    }

    for (size_t i = 0; i < keys.size(); ++i)
    {
        const TID tid =
            makeTID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i + 1), 1);
        ASSERT_EQ(btree->insert(keys[i], tid, 1, &ctx), Status::OK)
            << ctx.message;
    }

    const auto search_start = high_resolution_clock::now();
    uint64_t search_hits = 0;
    for (size_t i = 0; i < keys.size(); i += 17)
    {
        std::vector<TID> results;
        ASSERT_EQ(btree->search(keys[i], 0, &results, &ctx), Status::OK)
            << ctx.message;
        search_hits += results.size();
    }
    const auto search_end = high_resolution_clock::now();

    size_t deleted = 0;
    for (size_t i = 0; i < keys.size(); i += 2)
    {
        const TID tid =
            makeTID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(i + 1), 1);
        ASSERT_EQ(btree->remove(keys[i], tid, 2, &ctx), Status::OK)
            << ctx.message;
        ++deleted;
    }

    BTree::GcCompactionStats stats;
    const auto compact_start = high_resolution_clock::now();
    ASSERT_EQ(btree->gcCompact(&stats, &ctx), Status::OK) << ctx.message;
    const auto compact_end = high_resolution_clock::now();

    for (size_t i = 1; i < keys.size(); i += 514)
    {
        std::vector<TID> results;
        ASSERT_EQ(btree->search(keys[i], 0, &results, &ctx), Status::OK)
            << ctx.message;
        ASSERT_FALSE(results.empty());
    }

    for (size_t i = 0; i < keys.size(); i += 514)
    {
        std::vector<TID> results;
        ASSERT_EQ(btree->search(keys[i], 0, &results, &ctx), Status::NOT_FOUND)
            << ctx.message;
    }

    const double avg_search_us =
        duration_cast<microseconds>(search_end - search_start).count() /
        static_cast<double>(keys.size() / 17);
    const double compaction_us =
        static_cast<double>(
            duration_cast<microseconds>(compact_end - compact_start).count());

    std::cout << std::fixed << std::setprecision(3)
              << "BTREE_PROOF_MAINTENANCE"
              << ",label=restart_anchor_gc_compaction"
              << ",entries=" << keys.size()
              << ",deleted=" << deleted
              << ",search_hits=" << search_hits
              << ",avg_search_us=" << avg_search_us
              << ",gc_compaction_us=" << compaction_us
              << ",pages_visited=" << stats.pages_visited
              << ",pages_compacted=" << stats.pages_compacted
              << ",nodes_removed=" << stats.nodes_removed
              << ",bytes_reclaimed=" << stats.bytes_reclaimed
              << ",pages_merged=" << stats.pages_merged
              << std::endl;

    EXPECT_GT(search_hits, 0U);
    EXPECT_GT(stats.pages_visited, 0U);
    EXPECT_GT(stats.pages_compacted, 0U);
    EXPECT_GE(stats.nodes_removed, deleted / 2);
    EXPECT_GT(stats.bytes_reclaimed, 0U);
    EXPECT_LT(avg_search_us, 5000.0);
}
