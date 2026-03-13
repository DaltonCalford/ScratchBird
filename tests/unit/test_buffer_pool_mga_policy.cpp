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

#include <memory>
#include <vector>

#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/page_manager.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class BufferPoolMgaPolicyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>(
            "test_buffer_pool_mga_policy", ".sbrd");

        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx_), Status::OK) << ctx_.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx_), Status::OK) << ctx_.message;

        BufferPool::Config config;
        config.pool_size = 8;
        config.page_size = db_.page_size();
        config.enable_background_writer = false;

        policy_pool_ = std::make_unique<BufferPool>(&db_, config);
        ASSERT_EQ(policy_pool_->initialize(&ctx_), Status::OK) << ctx_.message;
    }

    void TearDown() override
    {
        if (policy_pool_ != nullptr)
        {
            ErrorContext shutdown_ctx;
            EXPECT_EQ(policy_pool_->shutdown(&shutdown_ctx), Status::OK) << shutdown_ctx.message;
            policy_pool_.reset();
        }
        db_.close();
        db_file_.reset();
    }

    auto allocateHeapPage() -> uint32_t
    {
        uint32_t page_id = 0;
        Status status = db_.page_manager()->allocatePage(page_id, &ctx_);
        EXPECT_EQ(status, Status::OK) << ctx_.message;
        if (status != Status::OK)
        {
            return 0;
        }

        std::vector<uint8_t> page_bytes(db_.page_size(), 0);
        HeapPage heap(page_bytes.data(), db_.page_size());
        status = heap.initialize(page_id, &ctx_);
        EXPECT_EQ(status, Status::OK) << ctx_.message;
        if (status != Status::OK)
        {
            return 0;
        }

        status = db_.write_page(page_id, page_bytes.data(), &ctx_);
        EXPECT_EQ(status, Status::OK) << ctx_.message;
        if (status != Status::OK)
        {
            return 0;
        }
        return page_id;
    }

    void pressurePages(const std::vector<uint32_t> &page_ids,
                       BufferPool::AccessStrategy strategy)
    {
        for (uint32_t page_id : page_ids)
        {
            void *buffer = nullptr;
            ASSERT_EQ(policy_pool_->pinPage(page_id, &buffer, &ctx_, strategy), Status::OK)
                << ctx_.message;
            ASSERT_NE(buffer, nullptr);
            ASSERT_EQ(policy_pool_->unpinPage(page_id, false, &ctx_), Status::OK) << ctx_.message;
        }
    }

    auto frameSnapshot(uint32_t page_id) -> BufferPool::MgaFrameSnapshot
    {
        BufferPool::MgaFrameSnapshot snapshot;
        const Status status =
            policy_pool_->getMgaFrameSnapshotGlobal(convertPageIDtoGPID(page_id), &snapshot, &ctx_);
        EXPECT_EQ(status, Status::OK) << ctx_.message;
        return snapshot;
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> db_file_;
    Database db_;
    ErrorContext ctx_;
    std::unique_ptr<BufferPool> policy_pool_;
};

TEST_F(BufferPoolMgaPolicyTest, CommitFenceProtectsTxStatePagesUnderPressure)
{
    void *buffer = nullptr;
    ASSERT_EQ(policy_pool_->pinPage(0, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_NE(buffer, nullptr);
    ASSERT_EQ(policy_pool_->unpinPage(0, false, &ctx_), Status::OK) << ctx_.message;

    policy_pool_->beginCommitFence();

    auto header_before = frameSnapshot(0);
    EXPECT_TRUE(header_before.resident);
    EXPECT_TRUE(header_before.commit_fence_member);
    EXPECT_EQ(header_before.page_class, BufferPool::MgaPageClass::TX_STATE);

    std::vector<uint32_t> scan_pages;
    for (int i = 0; i < 32; ++i)
    {
        scan_pages.push_back(allocateHeapPage());
    }

    pressurePages(scan_pages, BufferPool::AccessStrategy::Sequential);

    auto header_after = frameSnapshot(0);
    EXPECT_TRUE(header_after.resident);
    EXPECT_TRUE(header_after.commit_fence_member);
    EXPECT_EQ(header_after.page_class, BufferPool::MgaPageClass::TX_STATE);

    auto stats = policy_pool_->getStats();
    EXPECT_GT(stats.evictions, 0u);
    EXPECT_GT(stats.mga_frames_scan_probation, 0u);

    policy_pool_->endCommitFence();
    auto header_cleared = frameSnapshot(0);
    EXPECT_FALSE(header_cleared.commit_fence_member);
}

TEST_F(BufferPoolMgaPolicyTest, ChainHeavyPagesSurviveSequentialScanPressure)
{
    const uint32_t hot_page = allocateHeapPage();

    void *buffer = nullptr;
    ASSERT_EQ(policy_pool_->pinPage(hot_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(hot_page, false, &ctx_), Status::OK) << ctx_.message;

    BufferPool::MgaFrameHints hints;
    hints.page_class = BufferPool::MgaPageClass::CHAIN_HEAVY;
    hints.chain_depth_hint = 8;
    hints.prune_safe_horizon_hint = 900;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(hot_page),
                                                       hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;

    for (int i = 0; i < 3; ++i)
    {
        ASSERT_EQ(policy_pool_->pinPage(hot_page, &buffer, &ctx_), Status::OK) << ctx_.message;
        ASSERT_EQ(policy_pool_->unpinPage(hot_page, false, &ctx_), Status::OK) << ctx_.message;
    }

    std::vector<uint32_t> scan_pages;
    for (int i = 0; i < 24; ++i)
    {
        scan_pages.push_back(allocateHeapPage());
    }

    pressurePages(scan_pages, BufferPool::AccessStrategy::Sequential);

    auto hot_snapshot = frameSnapshot(hot_page);
    EXPECT_TRUE(hot_snapshot.resident);
    EXPECT_EQ(hot_snapshot.page_class, BufferPool::MgaPageClass::CHAIN_HEAVY);
    EXPECT_GE(hot_snapshot.chain_depth_hint, 8u);

    auto stats = policy_pool_->getStats();
    EXPECT_GT(stats.mga_chain_heavy_hits, 0u);
    EXPECT_GT(stats.mga_scan_probation_churn, 0u);
}

TEST_F(BufferPoolMgaPolicyTest, GcCandidatePagesAreOfferedToMaintenanceBeforeEviction)
{
    auto *gc = db_.garbage_collector();
    ASSERT_NE(gc, nullptr);

    const uint32_t candidate_page = allocateHeapPage();
    void *buffer = nullptr;
    ASSERT_EQ(policy_pool_->pinPage(candidate_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(candidate_page, false, &ctx_), Status::OK) << ctx_.message;
    const size_t dirty_before = gc->getDirtyPageCount();

    BufferPool::MgaFrameHints hints;
    hints.page_class = BufferPool::MgaPageClass::GC_CANDIDATE;
    hints.dead_version_bytes = 512;
    hints.prune_safe_horizon_hint = 4096;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(candidate_page),
                                                       hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;

    std::vector<uint32_t> pressure_pages;
    for (int i = 0; i < 24; ++i)
    {
        pressure_pages.push_back(allocateHeapPage());
    }

    pressurePages(pressure_pages, BufferPool::AccessStrategy::Sequential);

    auto stats = policy_pool_->getStats();
    EXPECT_GT(stats.mga_gc_handoff_offers, 0u);
    EXPECT_GT(gc->getDirtyPageCount(), dirty_before);
}
