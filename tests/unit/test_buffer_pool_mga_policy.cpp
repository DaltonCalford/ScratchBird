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

#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/mga_failpoint_manager.h"
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

        rebuildPolicyPool(makePolicyConfig());
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

    auto allocateTypedPage(uint16_t page_type,
                           uint32_t flags = 0,
                           uint32_t object_tag = 0) -> uint32_t
    {
        uint32_t page_id = 0;
        Status status = db_.page_manager()->allocatePage(page_id, &ctx_);
        EXPECT_EQ(status, Status::OK) << ctx_.message;
        if (status != Status::OK)
        {
            return 0;
        }

        std::vector<uint8_t> page_bytes(db_.page_size(), 0);
        auto *header = reinterpret_cast<PageHeader *>(page_bytes.data());
        header->magic = K_MAGIC_SBRD;
        header->page_type = page_type;
        header->page_id = page_id;
        header->flags = flags;
        if (object_tag != 0)
        {
            setObjectUuid(*header, makeObjectUuid(object_tag));
        }

        status = db_.write_page(page_id, page_bytes.data(), &ctx_);
        EXPECT_EQ(status, Status::OK) << ctx_.message;
        if (status != Status::OK)
        {
            return 0;
        }
        return page_id;
    }

    void touchPage(uint32_t page_id,
                   BufferPool::AccessStrategy strategy = BufferPool::AccessStrategy::Normal,
                   BufferPool::WorkloadClass workload_class =
                       BufferPool::WorkloadClass::Unspecified)
    {
        void *buffer = nullptr;
        ASSERT_EQ(policy_pool_->pinPage(page_id, &buffer, &ctx_, strategy, workload_class),
                  Status::OK)
            << ctx_.message;
        ASSERT_NE(buffer, nullptr);
        ASSERT_EQ(policy_pool_->unpinPage(page_id, false, &ctx_), Status::OK) << ctx_.message;
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

    auto domainSnapshot(BufferPool::PolicyDomain domain) const
        -> BufferPool::DomainAccountingSnapshot
    {
        const auto snapshots = policy_pool_->getDomainAccountingSnapshot();
        return snapshots[static_cast<size_t>(domain)];
    }

    auto makePolicyConfig() const -> BufferPool::Config
    {
        BufferPool::Config config;
        config.pool_size = 8;
        config.page_size = db_.page_size();
        config.enable_background_writer = false;
        return config;
    }

    void rebuildPolicyPool(const BufferPool::Config &config)
    {
        if (policy_pool_ != nullptr)
        {
            ErrorContext shutdown_ctx;
            ASSERT_EQ(policy_pool_->shutdown(&shutdown_ctx), Status::OK) << shutdown_ctx.message;
            policy_pool_.reset();
        }

        policy_pool_ = std::make_unique<BufferPool>(&db_, config);
        ASSERT_EQ(policy_pool_->initialize(&ctx_), Status::OK) << ctx_.message;
    }

    auto makeObjectUuid(uint32_t object_tag) const -> ID
    {
        ID object_uuid{};
        object_uuid.bytes[0] = static_cast<uint8_t>(object_tag & 0xFFu);
        object_uuid.bytes[1] = static_cast<uint8_t>((object_tag >> 8) & 0xFFu);
        object_uuid.bytes[2] = static_cast<uint8_t>((object_tag >> 16) & 0xFFu);
        object_uuid.bytes[3] = static_cast<uint8_t>((object_tag >> 24) & 0xFFu);
        object_uuid.bytes[15] = 0xA5u;
        return object_uuid;
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
    EXPECT_EQ(header_before.residency_tier, BufferPool::ResidencyTier::PinBiased);

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
    EXPECT_EQ(header_after.residency_tier, BufferPool::ResidencyTier::PinBiased);

    auto stats = policy_pool_->getStats();
    EXPECT_GT(stats.evictions, 0u);
    EXPECT_GT(stats.mga_frames_scan_probation, 0u);

    policy_pool_->endCommitFence();
    auto header_cleared = frameSnapshot(0);
    EXPECT_FALSE(header_cleared.commit_fence_member);
}

TEST_F(BufferPoolMgaPolicyTest, NonRootTransactionMapPagesStayCriticalButEvictable)
{
    const uint32_t tx_map_leaf = allocateTypedPage(PAGE_TYPE_TRANSACTION_MAP);

    touchPage(0);
    touchPage(tx_map_leaf);

    const auto tx_map_before = frameSnapshot(tx_map_leaf);
    EXPECT_TRUE(tx_map_before.resident);
    EXPECT_EQ(tx_map_before.page_class, BufferPool::MgaPageClass::TX_STATE);
    EXPECT_EQ(tx_map_before.policy_domain, BufferPool::PolicyDomain::CriticalSystem);
    EXPECT_EQ(tx_map_before.residency_tier, BufferPool::ResidencyTier::Probationary);

    std::vector<uint32_t> pressure_pages;
    for (int i = 0; i < 48; ++i)
    {
        pressure_pages.push_back(allocateHeapPage());
    }

    for (int pass = 0; pass < 3 && frameSnapshot(tx_map_leaf).resident; ++pass)
    {
        pressurePages(pressure_pages, BufferPool::AccessStrategy::Normal);
    }

    EXPECT_TRUE(frameSnapshot(0).resident);
    EXPECT_FALSE(frameSnapshot(tx_map_leaf).resident);
}

TEST_F(BufferPoolMgaPolicyTest, DirtyNonBootstrapZeroTypePageDoesNotBecomeBootstrapTxState)
{
    const uint32_t heap_page = allocateHeapPage();

    void *buffer = nullptr;
    ASSERT_EQ(policy_pool_->pinPage(heap_page, &buffer, &ctx_), Status::OK) << ctx_.message;

    std::memset(buffer, 0, db_.page_size());
    auto *header = static_cast<PageHeader *>(buffer);
    header->magic = K_MAGIC_SBRD;
    header->page_type = PAGE_TYPE_DATABASE_HEADER;
    header->page_id = heap_page;

    ASSERT_EQ(policy_pool_->unpinPage(heap_page, true, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->flushAll(&ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(db_.sync(&ctx_), Status::OK) << ctx_.message;

    ASSERT_EQ(policy_pool_->pinPage(heap_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(heap_page, false, &ctx_), Status::OK) << ctx_.message;

    const auto reloaded = frameSnapshot(heap_page);
    EXPECT_TRUE(reloaded.resident);
    EXPECT_EQ(reloaded.page_class, BufferPool::MgaPageClass::Generic);
    EXPECT_EQ(reloaded.policy_domain, BufferPool::PolicyDomain::HotOltp);
    EXPECT_NE(reloaded.residency_tier, BufferPool::ResidencyTier::PinBiased);
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

TEST_F(BufferPoolMgaPolicyTest, RuntimeAccountingPublishesCanonicalDomainBudgetsAndAttribution)
{
    void *buffer = nullptr;

    ASSERT_EQ(policy_pool_->pinPage(0, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(0, false, &ctx_), Status::OK) << ctx_.message;

    const uint32_t version_page = allocateHeapPage();
    ASSERT_EQ(policy_pool_->pinPage(version_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(version_page, false, &ctx_), Status::OK) << ctx_.message;
    BufferPool::MgaFrameHints version_hints;
    version_hints.page_class = BufferPool::MgaPageClass::VERSION_ROOT;
    version_hints.workload_class = BufferPool::WorkloadClass::SweepGc;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(version_page),
                                                       version_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;

    const uint32_t hot_page = allocateHeapPage();
    ASSERT_EQ(policy_pool_->pinPage(hot_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(hot_page, false, &ctx_), Status::OK) << ctx_.message;

    BufferPool::MgaFrameHints hot_hints;
    hot_hints.page_class = BufferPool::MgaPageClass::INDEX_CHURN;
    hot_hints.workload_class = BufferPool::WorkloadClass::IndexProbe;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(hot_page),
                                                       hot_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;

    const uint32_t scan_page = allocateHeapPage();
    ASSERT_EQ(policy_pool_->pinPage(scan_page, &buffer, &ctx_, BufferPool::AccessStrategy::Sequential),
              Status::OK)
        << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(scan_page, false, &ctx_), Status::OK) << ctx_.message;

    const auto header_snapshot = frameSnapshot(0);
    const auto version_snapshot = frameSnapshot(version_page);
    const auto hot_snapshot = frameSnapshot(hot_page);
    const auto scan_snapshot = frameSnapshot(scan_page);

    EXPECT_EQ(header_snapshot.policy_domain, BufferPool::PolicyDomain::CriticalSystem);
    EXPECT_EQ(version_snapshot.policy_domain, BufferPool::PolicyDomain::VersionUndo);
    EXPECT_EQ(hot_snapshot.policy_domain, BufferPool::PolicyDomain::HotOltp);
    EXPECT_EQ(scan_snapshot.policy_domain, BufferPool::PolicyDomain::ScanBulkRing);

    const auto critical = domainSnapshot(BufferPool::PolicyDomain::CriticalSystem);
    const auto hot = domainSnapshot(BufferPool::PolicyDomain::HotOltp);
    const auto read = domainSnapshot(BufferPool::PolicyDomain::ReadMostly);
    const auto scan = domainSnapshot(BufferPool::PolicyDomain::ScanBulkRing);
    const auto version = domainSnapshot(BufferPool::PolicyDomain::VersionUndo);
    const auto temp = domainSnapshot(BufferPool::PolicyDomain::TemporaryWork);

    EXPECT_EQ(critical.budget.min_frames, 1u);
    EXPECT_EQ(hot.budget.target_frames, 3u);
    EXPECT_EQ(read.budget.target_frames, 2u);
    EXPECT_EQ(scan.budget.max_frames, 1u);
    EXPECT_EQ(version.budget.min_frames, 2u);
    EXPECT_EQ(temp.budget.max_frames, 1u);

    EXPECT_GE(critical.resident_pages, 1u);
    EXPECT_GE(hot.resident_pages, 1u);
    EXPECT_GE(scan.resident_pages, 1u);
    EXPECT_GE(version.resident_pages, 1u);
    EXPECT_EQ(temp.resident_pages, 0u);
    EXPECT_EQ(critical.reservation_breach_count, 0u);
    EXPECT_EQ(version.emergency_breach_count, 0u);
}

TEST_F(BufferPoolMgaPolicyTest,
       SweepGcVersionUndoPagesStayProtectedOutsideTheScanRing)
{
    const uint32_t version_page = allocateHeapPage();
    const uint32_t gc_page = allocateHeapPage();

    touchPage(version_page);
    touchPage(gc_page);

    BufferPool::MgaFrameHints version_hints;
    version_hints.page_class = BufferPool::MgaPageClass::VERSION_ROOT;
    version_hints.workload_class = BufferPool::WorkloadClass::SweepGc;
    version_hints.chain_depth_hint = 2;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(version_page),
                                                       version_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;

    BufferPool::MgaFrameHints gc_hints;
    gc_hints.page_class = BufferPool::MgaPageClass::GC_CANDIDATE;
    gc_hints.workload_class = BufferPool::WorkloadClass::SweepGc;
    gc_hints.dead_version_bytes = 512;
    gc_hints.prune_safe_horizon_hint = 900;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(gc_page),
                                                       gc_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;

    touchPage(version_page,
              BufferPool::AccessStrategy::Vacuum,
              BufferPool::WorkloadClass::SweepGc);
    touchPage(gc_page,
              BufferPool::AccessStrategy::Vacuum,
              BufferPool::WorkloadClass::SweepGc);

    const auto version_snapshot = frameSnapshot(version_page);
    const auto gc_snapshot = frameSnapshot(gc_page);

    EXPECT_EQ(version_snapshot.page_class, BufferPool::MgaPageClass::VERSION_ROOT);
    EXPECT_EQ(version_snapshot.workload_class, BufferPool::WorkloadClass::SweepGc);
    EXPECT_EQ(version_snapshot.policy_domain, BufferPool::PolicyDomain::VersionUndo);
    EXPECT_EQ(version_snapshot.residency_tier, BufferPool::ResidencyTier::Protected);

    EXPECT_EQ(gc_snapshot.page_class, BufferPool::MgaPageClass::GC_CANDIDATE);
    EXPECT_EQ(gc_snapshot.workload_class, BufferPool::WorkloadClass::SweepGc);
    EXPECT_EQ(gc_snapshot.policy_domain, BufferPool::PolicyDomain::VersionUndo);
    EXPECT_EQ(gc_snapshot.residency_tier, BufferPool::ResidencyTier::Protected);
}

TEST_F(BufferPoolMgaPolicyTest,
       TemporaryWorkHintsCannotMoveDurablePagesIntoTheTempDomain)
{
    const uint32_t durable_page = allocateHeapPage();
    touchPage(durable_page,
              BufferPool::AccessStrategy::Normal,
              BufferPool::WorkloadClass::PointLookup);

    BufferPool::MgaFrameHints hints;
    hints.page_class = BufferPool::MgaPageClass::Generic;
    hints.workload_class = BufferPool::WorkloadClass::TemporaryWork;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(durable_page),
                                                       hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;

    const auto durable_snapshot = frameSnapshot(durable_page);
    EXPECT_EQ(durable_snapshot.page_class, BufferPool::MgaPageClass::Generic);
    EXPECT_EQ(durable_snapshot.workload_class, BufferPool::WorkloadClass::PointLookup);
    EXPECT_EQ(durable_snapshot.policy_domain, BufferPool::PolicyDomain::HotOltp);
    EXPECT_NE(durable_snapshot.policy_domain, BufferPool::PolicyDomain::TemporaryWork);
}

TEST_F(BufferPoolMgaPolicyTest,
       AutomaticPageRoleMappingPromotesSystemMetaIndexRootsAndTempWork)
{
    const uint32_t system_root_page = allocateTypedPage(PAGE_TYPE_CATALOG_ROOT);
    const uint32_t system_leaf_page = allocateTypedPage(PAGE_TYPE_CATALOG_PAGE);
    const uint32_t index_root_page = allocateTypedPage(PAGE_TYPE_BTREE_INTERNAL);
    const uint32_t temp_page = allocateTypedPage(PAGE_TYPE_TEMP_HEAP);

    touchPage(system_root_page);
    touchPage(system_leaf_page);
    touchPage(system_leaf_page);
    touchPage(index_root_page,
              BufferPool::AccessStrategy::Normal,
              BufferPool::WorkloadClass::IndexProbe);
    touchPage(temp_page);

    const auto system_root_snapshot = frameSnapshot(system_root_page);
    const auto system_leaf_snapshot = frameSnapshot(system_leaf_page);
    const auto index_root_snapshot = frameSnapshot(index_root_page);
    const auto temp_snapshot = frameSnapshot(temp_page);

    EXPECT_EQ(system_root_snapshot.page_class, BufferPool::MgaPageClass::SYSTEM_META);
    EXPECT_EQ(system_root_snapshot.policy_domain, BufferPool::PolicyDomain::CriticalSystem);
    EXPECT_EQ(system_root_snapshot.residency_tier, BufferPool::ResidencyTier::Protected);

    EXPECT_EQ(system_leaf_snapshot.page_class, BufferPool::MgaPageClass::SYSTEM_META);
    EXPECT_EQ(system_leaf_snapshot.policy_domain, BufferPool::PolicyDomain::CriticalSystem);
    EXPECT_EQ(system_leaf_snapshot.residency_tier,
              BufferPool::ResidencyTier::Probationary);
    EXPECT_GE(system_leaf_snapshot.last_touch_generation,
              system_leaf_snapshot.admission_generation);

    EXPECT_EQ(index_root_snapshot.page_class, BufferPool::MgaPageClass::INDEX_ROOT_INTERNAL);
    EXPECT_EQ(index_root_snapshot.workload_class, BufferPool::WorkloadClass::IndexProbe);
    EXPECT_EQ(index_root_snapshot.policy_domain, BufferPool::PolicyDomain::HotOltp);
    EXPECT_EQ(index_root_snapshot.residency_tier, BufferPool::ResidencyTier::Protected);

    EXPECT_EQ(temp_snapshot.page_class, BufferPool::MgaPageClass::TEMP_WORK);
    EXPECT_EQ(temp_snapshot.workload_class, BufferPool::WorkloadClass::TemporaryWork);
    EXPECT_EQ(temp_snapshot.policy_domain, BufferPool::PolicyDomain::TemporaryWork);
}

TEST_F(BufferPoolMgaPolicyTest,
       WorkloadHintsRedirectGenericDataWithoutOverridingProtectedPageRoles)
{
    const uint32_t range_page = allocateHeapPage();
    const uint32_t seq_page = allocateHeapPage();
    const uint32_t index_root_page = allocateTypedPage(PAGE_TYPE_BTREE_INTERNAL);

    void *range_buffer = nullptr;
    ASSERT_EQ(policy_pool_->pinPage(range_page,
                                    &range_buffer,
                                    &ctx_,
                                    BufferPool::AccessStrategy::Normal,
                                    BufferPool::WorkloadClass::RangeScan),
              Status::OK)
        << ctx_.message;
    ASSERT_NE(range_buffer, nullptr);

    void *seq_buffer = nullptr;
    ASSERT_EQ(policy_pool_->pinPage(seq_page,
                                    &seq_buffer,
                                    &ctx_,
                                    BufferPool::AccessStrategy::Sequential),
              Status::OK)
        << ctx_.message;
    ASSERT_NE(seq_buffer, nullptr);

    void *index_root_buffer = nullptr;
    ASSERT_EQ(policy_pool_->pinPage(index_root_page,
                                    &index_root_buffer,
                                    &ctx_,
                                    BufferPool::AccessStrategy::Sequential,
                                    BufferPool::WorkloadClass::SequentialScan),
              Status::OK)
        << ctx_.message;
    ASSERT_NE(index_root_buffer, nullptr);

    const auto range_snapshot = frameSnapshot(range_page);
    const auto seq_snapshot = frameSnapshot(seq_page);
    const auto index_root_snapshot = frameSnapshot(index_root_page);

    EXPECT_TRUE(range_snapshot.resident);
    EXPECT_TRUE(seq_snapshot.resident);
    EXPECT_TRUE(index_root_snapshot.resident);

    EXPECT_EQ(range_snapshot.page_class, BufferPool::MgaPageClass::Generic);
    EXPECT_EQ(range_snapshot.workload_class, BufferPool::WorkloadClass::RangeScan);
    EXPECT_EQ(range_snapshot.policy_domain, BufferPool::PolicyDomain::ReadMostly);
    EXPECT_EQ(range_snapshot.residency_tier, BufferPool::ResidencyTier::Probationary);

    EXPECT_EQ(seq_snapshot.page_class, BufferPool::MgaPageClass::SCAN_PROBATION);
    EXPECT_EQ(seq_snapshot.workload_class, BufferPool::WorkloadClass::SequentialScan);
    EXPECT_EQ(seq_snapshot.policy_domain, BufferPool::PolicyDomain::ScanBulkRing);
    EXPECT_EQ(seq_snapshot.residency_tier, BufferPool::ResidencyTier::RingOnly);

    EXPECT_EQ(index_root_snapshot.page_class, BufferPool::MgaPageClass::INDEX_ROOT_INTERNAL);
    EXPECT_EQ(index_root_snapshot.workload_class, BufferPool::WorkloadClass::SequentialScan);
    EXPECT_EQ(index_root_snapshot.policy_domain, BufferPool::PolicyDomain::HotOltp);
    EXPECT_EQ(index_root_snapshot.residency_tier, BufferPool::ResidencyTier::Protected);

    ASSERT_EQ(policy_pool_->unpinPage(index_root_page, false, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(seq_page, false, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(range_page, false, &ctx_), Status::OK) << ctx_.message;
}

TEST_F(BufferPoolMgaPolicyTest, BulkWriteAndPrefetchPlumbCanonicalWorkloadHints)
{
    const uint32_t bulk_page = allocateHeapPage();
    const uint32_t prefetch_page = allocateHeapPage();

    ConnectionContext conn_ctx(&db_, 0);
    conn_ctx.setBulkWriteMode(true);
    ConnectionContext::setCurrent(&conn_ctx);
    touchPage(bulk_page);
    ConnectionContext::setCurrent(nullptr);

    ASSERT_EQ(policy_pool_->prefetchPages({prefetch_page}, &ctx_), Status::OK) << ctx_.message;

    const auto bulk_snapshot = frameSnapshot(bulk_page);
    const auto prefetch_snapshot = frameSnapshot(prefetch_page);

    EXPECT_EQ(bulk_snapshot.workload_class, BufferPool::WorkloadClass::BulkWrite);
    EXPECT_EQ(bulk_snapshot.page_class, BufferPool::MgaPageClass::SCAN_PROBATION);
    EXPECT_EQ(bulk_snapshot.policy_domain, BufferPool::PolicyDomain::ScanBulkRing);
    EXPECT_EQ(bulk_snapshot.residency_tier, BufferPool::ResidencyTier::RingOnly);

    EXPECT_EQ(prefetch_snapshot.workload_class, BufferPool::WorkloadClass::PrefetchSpeculative);
    EXPECT_EQ(prefetch_snapshot.page_class, BufferPool::MgaPageClass::SCAN_PROBATION);
    EXPECT_EQ(prefetch_snapshot.policy_domain, BufferPool::PolicyDomain::ScanBulkRing);
    EXPECT_EQ(prefetch_snapshot.residency_tier, BufferPool::ResidencyTier::RingOnly);
    EXPECT_TRUE(prefetch_snapshot.speculative_prefetch);
    EXPECT_FALSE(prefetch_snapshot.prefetch_consumed);
}

TEST_F(BufferPoolMgaPolicyTest,
       PrefetchDebtCapsSpeculativeAdmissionsAndCancelsExcessWork)
{
    auto config = makePolicyConfig();
    config.prefetch_max_debt_pages = 2;
    rebuildPolicyPool(config);

    std::vector<uint32_t> pages;
    for (int i = 0; i < 4; ++i)
    {
        pages.push_back(allocateHeapPage());
    }

    ASSERT_EQ(policy_pool_->prefetchPages(pages, &ctx_), Status::OK) << ctx_.message;

    size_t speculative_resident = 0;
    for (uint32_t page_id : pages)
    {
        const auto snapshot = frameSnapshot(page_id);
        if (snapshot.resident && snapshot.speculative_prefetch)
        {
            ++speculative_resident;
        }
    }

    const auto stats = policy_pool_->getStats();
    EXPECT_EQ(speculative_resident, 2u);
    EXPECT_EQ(stats.prefetch_debt_pages, 2u);
    EXPECT_EQ(stats.prefetch_cancelled_pages, 2u);
    EXPECT_EQ(stats.thrash_detector_state,
              BufferPool::ThrashDetectorState::GlobalDebtCap);
}

TEST_F(BufferPoolMgaPolicyTest,
       DemandHitConsumesPrefetchDebtAndCountsUsefulness)
{
    const uint32_t prefetched_page = allocateHeapPage();

    ASSERT_EQ(policy_pool_->prefetchPages({prefetched_page}, &ctx_), Status::OK) << ctx_.message;
    const auto prefetched_snapshot = frameSnapshot(prefetched_page);
    EXPECT_TRUE(prefetched_snapshot.speculative_prefetch);
    EXPECT_FALSE(prefetched_snapshot.prefetch_consumed);

    touchPage(prefetched_page);

    const auto consumed_snapshot = frameSnapshot(prefetched_page);
    EXPECT_FALSE(consumed_snapshot.speculative_prefetch);
    EXPECT_TRUE(consumed_snapshot.prefetch_consumed);
    EXPECT_EQ(consumed_snapshot.prefetch_session_key, 0u);

    const auto stats = policy_pool_->getStats();
    EXPECT_EQ(stats.prefetch_pages_useful, 1u);
    EXPECT_EQ(stats.prefetch_debt_pages, 0u);
}

TEST_F(BufferPoolMgaPolicyTest,
       UnusedPrefetchEvictionIsCountedSeparately)
{
    auto config = makePolicyConfig();
    config.pool_size = 6;
    config.prefetch_max_debt_pages = 2;
    rebuildPolicyPool(config);

    std::vector<uint32_t> prefetched_pages;
    for (int i = 0; i < 2; ++i)
    {
        prefetched_pages.push_back(allocateHeapPage());
    }
    ASSERT_EQ(policy_pool_->prefetchPages(prefetched_pages, &ctx_), Status::OK) << ctx_.message;

    std::vector<uint32_t> pressure_pages;
    for (int i = 0; i < 16; ++i)
    {
        pressure_pages.push_back(allocateHeapPage());
    }
    pressurePages(pressure_pages, BufferPool::AccessStrategy::Normal);

    const auto stats = policy_pool_->getStats();
    EXPECT_GE(stats.prefetch_pages_unused_evicted, 1u);
}

TEST_F(BufferPoolMgaPolicyTest,
       ObjectProtectionCapKeepsOveradmittedGenericPagesOutOfProtectedSet)
{
    auto config = makePolicyConfig();
    config.thrash_object_budget_pct = 25;
    rebuildPolicyPool(config);

    const std::vector<uint32_t> object_pages = {
        allocateTypedPage(PAGE_TYPE_HEAP, 0, 77),
        allocateTypedPage(PAGE_TYPE_HEAP, 0, 77),
        allocateTypedPage(PAGE_TYPE_HEAP, 0, 77),
    };

    for (uint32_t page_id : object_pages)
    {
        touchPage(page_id);
        touchPage(page_id);
    }

    size_t protected_count = 0;
    size_t probationary_count = 0;
    uint32_t observed_object_id = 0;
    for (uint32_t page_id : object_pages)
    {
        const auto snapshot = frameSnapshot(page_id);
        ASSERT_NE(snapshot.object_id, 0u);
        if (observed_object_id == 0)
        {
            observed_object_id = snapshot.object_id;
        }
        EXPECT_EQ(snapshot.object_id, observed_object_id);
        if (snapshot.residency_tier == BufferPool::ResidencyTier::Protected)
        {
            ++protected_count;
        }
        if (snapshot.residency_tier == BufferPool::ResidencyTier::Probationary)
        {
            ++probationary_count;
        }
    }

    const auto stats = policy_pool_->getStats();
    EXPECT_EQ(protected_count, 2u);
    EXPECT_EQ(probationary_count, 1u);
    EXPECT_GT(stats.fairness_object_budget_breaches, 0u);
}

TEST_F(BufferPoolMgaPolicyTest,
       UsefulnessCollapseCancelsFurtherSpeculativePrefetch)
{
    auto config = makePolicyConfig();
    config.prefetch_max_debt_pages = 4;
    config.prefetch_usefulness_floor_pct = 75;
    config.thrash_session_budget_pct = 100;
    rebuildPolicyPool(config);

    std::vector<uint32_t> first_wave;
    for (int i = 0; i < 4; ++i)
    {
        first_wave.push_back(allocateHeapPage());
    }
    ASSERT_EQ(policy_pool_->prefetchPages(first_wave, &ctx_), Status::OK) << ctx_.message;

    std::vector<uint32_t> pressure_pages;
    for (int i = 0; i < 32; ++i)
    {
        pressure_pages.push_back(allocateHeapPage());
    }
    pressurePages(pressure_pages, BufferPool::AccessStrategy::Normal);

    const auto pre_collapse_stats = policy_pool_->getStats();
    ASSERT_GE(pre_collapse_stats.prefetch_pages_unused_evicted, 4u);

    std::vector<uint32_t> second_wave;
    for (int i = 0; i < 2; ++i)
    {
        second_wave.push_back(allocateHeapPage());
    }
    ASSERT_EQ(policy_pool_->prefetchPages(second_wave, &ctx_), Status::OK) << ctx_.message;

    const auto stats = policy_pool_->getStats();
    EXPECT_EQ(stats.thrash_detector_state,
              BufferPool::ThrashDetectorState::UsefulnessCollapse);
    EXPECT_GE(stats.prefetch_cancelled_pages, 2u);
    for (uint32_t page_id : second_wave)
    {
        EXPECT_FALSE(frameSnapshot(page_id).resident);
    }
}

TEST_F(BufferPoolMgaPolicyTest,
       ScanPressureShrinksSpeculativePrefetchToOnePage)
{
    auto config = makePolicyConfig();
    config.prefetch_max_debt_pages = 8;
    config.thrash_session_budget_pct = 100;
    config.thrash_prefetch_pressure_pct = 25;
    rebuildPolicyPool(config);

    std::vector<uint32_t> first_wave;
    for (int i = 0; i < 2; ++i)
    {
        first_wave.push_back(allocateHeapPage());
    }
    ASSERT_EQ(policy_pool_->prefetchPages(first_wave, &ctx_), Status::OK) << ctx_.message;

    std::vector<uint32_t> second_wave;
    for (int i = 0; i < 4; ++i)
    {
        second_wave.push_back(allocateHeapPage());
    }
    ASSERT_EQ(policy_pool_->prefetchPages(second_wave, &ctx_), Status::OK) << ctx_.message;

    size_t speculative_resident = 0;
    for (uint32_t page_id : first_wave)
    {
        if (frameSnapshot(page_id).speculative_prefetch)
        {
            ++speculative_resident;
        }
    }
    for (uint32_t page_id : second_wave)
    {
        if (frameSnapshot(page_id).speculative_prefetch)
        {
            ++speculative_resident;
        }
    }

    const auto stats = policy_pool_->getStats();
    EXPECT_EQ(speculative_resident, 3u);
    EXPECT_EQ(stats.prefetch_cancelled_pages, 3u);
    EXPECT_EQ(stats.thrash_detector_state, BufferPool::ThrashDetectorState::ScanPressure);
}

TEST_F(BufferPoolMgaPolicyTest,
       DefaultDemandReadEntersProbationaryAndSecondTouchPromotesToProtected)
{
    void *buffer = nullptr;
    const uint32_t heap_page = allocateHeapPage();

    ASSERT_EQ(policy_pool_->pinPage(heap_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(heap_page, false, &ctx_), Status::OK) << ctx_.message;

    const auto first_touch = frameSnapshot(heap_page);
    EXPECT_EQ(first_touch.residency_tier, BufferPool::ResidencyTier::Probationary);
    ASSERT_GT(first_touch.admission_generation, 0u);
    EXPECT_EQ(first_touch.last_touch_generation, first_touch.admission_generation);

    ASSERT_EQ(policy_pool_->pinPage(heap_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(heap_page, false, &ctx_), Status::OK) << ctx_.message;

    const auto second_touch = frameSnapshot(heap_page);
    EXPECT_EQ(second_touch.residency_tier, BufferPool::ResidencyTier::Protected);
    EXPECT_GE(second_touch.last_touch_generation, second_touch.admission_generation);
    EXPECT_EQ(second_touch.temperature_generation, second_touch.last_touch_generation);

    const auto stats = policy_pool_->getStats();
    EXPECT_GT(stats.mga_admission_promotions, 0u);
}

TEST_F(BufferPoolMgaPolicyTest,
       GhostHistoryPromotesReloadedPageBackToProtectedResidency)
{
    void *buffer = nullptr;
    const uint32_t hot_page = allocateHeapPage();

    ASSERT_EQ(policy_pool_->pinPage(hot_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(hot_page, false, &ctx_), Status::OK) << ctx_.message;

    const auto admitted = frameSnapshot(hot_page);
    EXPECT_EQ(admitted.residency_tier, BufferPool::ResidencyTier::Probationary);

    std::vector<uint32_t> pressure_pages;
    for (int i = 0; i < 64; ++i)
    {
        pressure_pages.push_back(allocateHeapPage());
    }
    pressurePages(pressure_pages, BufferPool::AccessStrategy::Normal);

    const auto evicted = frameSnapshot(hot_page);
    EXPECT_FALSE(evicted.resident);

    auto stats = policy_pool_->getStats();
    EXPECT_GT(stats.mga_ghost_history_entries, 0u);

    ASSERT_EQ(policy_pool_->pinPage(hot_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(hot_page, false, &ctx_), Status::OK) << ctx_.message;

    const auto reloaded = frameSnapshot(hot_page);
    EXPECT_TRUE(reloaded.resident);
    EXPECT_EQ(reloaded.residency_tier, BufferPool::ResidencyTier::Protected);

    stats = policy_pool_->getStats();
    EXPECT_GT(stats.mga_ghost_history_hits, 0u);
    EXPECT_GT(stats.mga_admission_promotions, 0u);
}

TEST_F(BufferPoolMgaPolicyTest,
       FirstMutationPublishesDirtyGenerationAsUnscheduled)
{
    void *buffer = nullptr;

    const uint32_t scan_page = allocateHeapPage();
    ASSERT_EQ(policy_pool_->pinPage(scan_page, &buffer, &ctx_, BufferPool::AccessStrategy::Sequential),
              Status::OK)
        << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(scan_page, false, &ctx_), Status::OK) << ctx_.message;

    const auto scan_snapshot = frameSnapshot(scan_page);
    EXPECT_EQ(scan_snapshot.lifecycle_state, BufferPool::LifecycleState::Valid);
    EXPECT_EQ(scan_snapshot.dirty_state, BufferPool::DirtyState::Clean);
    EXPECT_EQ(scan_snapshot.residency_tier, BufferPool::ResidencyTier::RingOnly);
    EXPECT_EQ(scan_snapshot.last_flush_generation, 0u);
    EXPECT_EQ(scan_snapshot.checkpoint_target_generation, 0u);

    const uint32_t dirty_page = allocateHeapPage();
    ASSERT_EQ(policy_pool_->pinPage(dirty_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(dirty_page, true, &ctx_), Status::OK) << ctx_.message;

    const auto dirty_snapshot = frameSnapshot(dirty_page);
    ASSERT_TRUE(dirty_snapshot.resident);
    EXPECT_TRUE(dirty_snapshot.is_dirty);
    EXPECT_EQ(dirty_snapshot.lifecycle_state, BufferPool::LifecycleState::Valid);
    EXPECT_EQ(dirty_snapshot.dirty_state, BufferPool::DirtyState::DirtyUnscheduled);
    EXPECT_EQ(dirty_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::BACKGROUND_AGE);
    EXPECT_EQ(dirty_snapshot.residency_tier, BufferPool::ResidencyTier::Probationary);
    ASSERT_GT(dirty_snapshot.dirty_generation, 0u);
    EXPECT_EQ(dirty_snapshot.last_flush_generation, 0u);
    EXPECT_EQ(dirty_snapshot.checkpoint_target_generation, 0u);
}

TEST_F(BufferPoolMgaPolicyTest,
       CheckpointFlushTransitionsDirtyFrameToPendingFsyncUntilSync)
{
    void *buffer = nullptr;

    const uint32_t dirty_page = allocateHeapPage();
    ASSERT_EQ(policy_pool_->pinPage(dirty_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(dirty_page, true, &ctx_), Status::OK) << ctx_.message;

    const auto dirty_snapshot = frameSnapshot(dirty_page);
    ASSERT_GT(dirty_snapshot.dirty_generation, 0u);

    ASSERT_EQ(policy_pool_->flushDirtyCheckpointBoundary(dirty_snapshot.dirty_generation, &ctx_),
              Status::OK)
        << ctx_.message;

    const auto flushed_snapshot = frameSnapshot(dirty_page);
    EXPECT_TRUE(flushed_snapshot.is_dirty);
    EXPECT_EQ(flushed_snapshot.lifecycle_state, BufferPool::LifecycleState::Valid);
    EXPECT_EQ(flushed_snapshot.dirty_state,
              BufferPool::DirtyState::DirtyFlushedPendingFsync);
    EXPECT_EQ(flushed_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::CHECKPOINT);
    EXPECT_EQ(flushed_snapshot.last_flush_generation, dirty_snapshot.dirty_generation);
    EXPECT_EQ(flushed_snapshot.checkpoint_target_generation, dirty_snapshot.dirty_generation);

    ASSERT_EQ(db_.sync(&ctx_), Status::OK) << ctx_.message;
    // This fixture owns a standalone policy pool rather than the Database-owned
    // pool, so it completes the same post-fsync resident-frame transition
    // explicitly here.
    policy_pool_->completeFsyncFence();

    const auto synced_snapshot = frameSnapshot(dirty_page);
    EXPECT_FALSE(synced_snapshot.is_dirty);
    EXPECT_EQ(synced_snapshot.lifecycle_state, BufferPool::LifecycleState::Valid);
    EXPECT_EQ(synced_snapshot.dirty_state, BufferPool::DirtyState::Clean);
    EXPECT_EQ(synced_snapshot.writeback_queue_state, BufferPool::WritebackQueueState::NONE);
    EXPECT_EQ(synced_snapshot.last_flush_generation, dirty_snapshot.dirty_generation);
    EXPECT_EQ(synced_snapshot.checkpoint_target_generation, 0u);
}

TEST_F(BufferPoolMgaPolicyTest,
       RedirtyAfterSuccessfulFlushReturnsFrameToUnscheduled)
{
    void *buffer = nullptr;

    const uint32_t dirty_page = allocateHeapPage();
    ASSERT_EQ(policy_pool_->pinPage(dirty_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(dirty_page, true, &ctx_), Status::OK) << ctx_.message;

    const auto first_dirty_snapshot = frameSnapshot(dirty_page);
    ASSERT_GT(first_dirty_snapshot.dirty_generation, 0u);

    ASSERT_EQ(policy_pool_->flushAll(&ctx_), Status::OK) << ctx_.message;

    const auto pending_snapshot = frameSnapshot(dirty_page);
    EXPECT_TRUE(pending_snapshot.is_dirty);
    EXPECT_EQ(pending_snapshot.dirty_state,
              BufferPool::DirtyState::DirtyFlushedPendingFsync);
    EXPECT_EQ(pending_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::BACKGROUND_AGE);
    EXPECT_EQ(pending_snapshot.last_flush_generation, first_dirty_snapshot.dirty_generation);

    ASSERT_EQ(policy_pool_->pinPage(dirty_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(dirty_page, true, &ctx_), Status::OK) << ctx_.message;

    const auto redirtied_snapshot = frameSnapshot(dirty_page);
    EXPECT_TRUE(redirtied_snapshot.is_dirty);
    EXPECT_EQ(redirtied_snapshot.dirty_state, BufferPool::DirtyState::DirtyUnscheduled);
    EXPECT_EQ(redirtied_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::BACKGROUND_AGE);
    EXPECT_GT(redirtied_snapshot.dirty_generation, pending_snapshot.last_flush_generation);
    EXPECT_EQ(redirtied_snapshot.last_flush_generation, pending_snapshot.last_flush_generation);
    EXPECT_EQ(redirtied_snapshot.checkpoint_target_generation, 0u);
}

TEST_F(BufferPoolMgaPolicyTest,
       WritebackDebtMetricsTrackQueueKindsAndGenerations)
{
    void *buffer = nullptr;

    ASSERT_EQ(policy_pool_->pinPage(0, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(0, true, &ctx_), Status::OK) << ctx_.message;
    policy_pool_->beginCommitFence();

    const auto tx_snapshot = frameSnapshot(0);
    EXPECT_EQ(tx_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::FOREGROUND_HELP);

    const uint32_t generic_page = allocateHeapPage();
    ASSERT_EQ(policy_pool_->pinPage(generic_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(generic_page, true, &ctx_), Status::OK) << ctx_.message;
    const auto generic_snapshot = frameSnapshot(generic_page);
    EXPECT_EQ(generic_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::BACKGROUND_AGE);

    const uint32_t metadata_page = allocateTypedPage(PAGE_TYPE_SYSTEM_STATE);
    BufferPool::MgaFrameHints metadata_hints;
    metadata_hints.page_class = BufferPool::MgaPageClass::SYSTEM_META;
    ASSERT_EQ(policy_pool_->pinPage(metadata_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(metadata_page),
                                                       metadata_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(metadata_page, true, &ctx_), Status::OK) << ctx_.message;
    const auto metadata_snapshot = frameSnapshot(metadata_page);
    EXPECT_EQ(metadata_snapshot.page_class, BufferPool::MgaPageClass::SYSTEM_META);
    EXPECT_EQ(metadata_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::METADATA_PRIORITY);

    const uint32_t chain_page = allocateHeapPage();
    touchPage(chain_page);
    BufferPool::MgaFrameHints chain_hints;
    chain_hints.page_class = BufferPool::MgaPageClass::CHAIN_HEAVY;
    chain_hints.chain_depth_hint = 7;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(chain_page),
                                                       chain_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;
    ASSERT_EQ(policy_pool_->pinPage(chain_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(chain_page, true, &ctx_), Status::OK) << ctx_.message;
    const auto chain_snapshot = frameSnapshot(chain_page);
    EXPECT_EQ(chain_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::WRITE_COMBINE);

    const uint32_t checkpoint_page = allocateHeapPage();
    ASSERT_EQ(policy_pool_->pinPage(checkpoint_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(checkpoint_page, true, &ctx_), Status::OK) << ctx_.message;
    const auto checkpoint_dirty_snapshot = frameSnapshot(checkpoint_page);
    ASSERT_GT(checkpoint_dirty_snapshot.dirty_generation, 0u);
    const auto pre_checkpoint_stats = policy_pool_->getStats();
    EXPECT_GT(pre_checkpoint_stats.dirty_generation_low_watermark, 0u);
    EXPECT_GE(pre_checkpoint_stats.dirty_generation_high_watermark,
              pre_checkpoint_stats.dirty_generation_low_watermark);
    EXPECT_GE(pre_checkpoint_stats.queue_depth_foreground_help, 1u);
    EXPECT_GE(pre_checkpoint_stats.queue_depth_background_age, 1u);
    EXPECT_GE(pre_checkpoint_stats.queue_depth_metadata_priority, 1u);
    EXPECT_GE(pre_checkpoint_stats.queue_depth_write_combine, 1u);
    EXPECT_GE(pre_checkpoint_stats.foreground_help_backlog_pages, 1u);

    ASSERT_EQ(policy_pool_->flushDirtyCheckpointBoundary(
                  checkpoint_dirty_snapshot.dirty_generation,
                  &ctx_),
              Status::OK)
        << ctx_.message;
    const auto checkpoint_snapshot = frameSnapshot(checkpoint_page);
    EXPECT_EQ(checkpoint_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::CHECKPOINT);

    const auto post_checkpoint_stats = policy_pool_->getStats();
    EXPECT_GE(post_checkpoint_stats.queue_depth_checkpoint, 1u);
    EXPECT_GE(post_checkpoint_stats.checkpoint_bound_dirty_pages, 1u);

    policy_pool_->endCommitFence();
}

TEST_F(BufferPoolMgaPolicyTest,
       WritebackFailureMovesFrameIntoRepairRetryDebt)
{
    const uint32_t failed_page = allocateHeapPage();
    void *buffer = nullptr;
    ASSERT_EQ(policy_pool_->pinPage(failed_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(failed_page, true, &ctx_), Status::OK) << ctx_.message;

    auto *failpoints = db_.mga_failpoint_manager();
    ASSERT_NE(failpoints, nullptr);

    ErrorContext failpoint_ctx;
    MgaFailpointDefinition definition{};
    definition.trigger_name =
        std::string(MgaFailpointTriggers::kWritebackPageWriteFailure);
    definition.injected_status = Status::DISK_FULL;
    ASSERT_EQ(failpoints->installSeed("mmw008_repair_retry", {definition}, &failpoint_ctx),
              Status::OK)
        << failpoint_ctx.message;

    ErrorContext flush_ctx;
    EXPECT_EQ(policy_pool_->flushPage(failed_page, &flush_ctx), Status::DISK_FULL);
    ASSERT_EQ(failpoints->clear(&failpoint_ctx), Status::OK) << failpoint_ctx.message;

    const auto failed_snapshot = frameSnapshot(failed_page);
    EXPECT_TRUE(failed_snapshot.is_dirty);
    EXPECT_EQ(failed_snapshot.lifecycle_state, BufferPool::LifecycleState::Error);
    EXPECT_EQ(failed_snapshot.dirty_state, BufferPool::DirtyState::DirtyFailed);
    EXPECT_EQ(failed_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::REPAIR_RETRY);

    const auto stats = policy_pool_->getStats();
    EXPECT_GE(stats.queue_depth_repair_retry, 1u);
    EXPECT_TRUE(db_.write_admission_fenced());

    ASSERT_EQ(db_.clearWritebackFailureState(&ctx_), Status::OK) << ctx_.message;
}

TEST_F(BufferPoolMgaPolicyTest,
       GcCandidateWritebackFailurePreservesVersionDomainAndQueuesRepairRetry)
{
    const uint32_t candidate_page = allocateHeapPage();
    void *buffer = nullptr;
    ASSERT_EQ(policy_pool_->pinPage(candidate_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(candidate_page, true, &ctx_), Status::OK) << ctx_.message;

    BufferPool::MgaFrameHints hints;
    hints.page_class = BufferPool::MgaPageClass::GC_CANDIDATE;
    hints.workload_class = BufferPool::WorkloadClass::SweepGc;
    hints.dead_version_bytes = 512;
    hints.prune_safe_horizon_hint = 2048;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(candidate_page),
                                                       hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;

    auto *failpoints = db_.mga_failpoint_manager();
    ASSERT_NE(failpoints, nullptr);

    ErrorContext failpoint_ctx;
    MgaFailpointDefinition definition{};
    definition.trigger_name =
        std::string(MgaFailpointTriggers::kWritebackPageWriteFailure);
    definition.injected_status = Status::DISK_FULL;
    ASSERT_EQ(failpoints->installSeed("mmw010_gc_repair_retry", {definition}, &failpoint_ctx),
              Status::OK)
        << failpoint_ctx.message;

    ErrorContext flush_ctx;
    EXPECT_EQ(policy_pool_->flushPage(candidate_page, &flush_ctx), Status::DISK_FULL);
    ASSERT_EQ(failpoints->clear(&failpoint_ctx), Status::OK) << failpoint_ctx.message;

    const auto candidate_snapshot = frameSnapshot(candidate_page);
    EXPECT_EQ(candidate_snapshot.page_class, BufferPool::MgaPageClass::GC_CANDIDATE);
    EXPECT_EQ(candidate_snapshot.workload_class, BufferPool::WorkloadClass::SweepGc);
    EXPECT_EQ(candidate_snapshot.policy_domain, BufferPool::PolicyDomain::VersionUndo);
    EXPECT_EQ(candidate_snapshot.dirty_state, BufferPool::DirtyState::DirtyFailed);
    EXPECT_EQ(candidate_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::REPAIR_RETRY);

    ASSERT_EQ(db_.clearWritebackFailureState(&ctx_), Status::OK) << ctx_.message;
}

TEST_F(BufferPoolMgaPolicyTest,
       BackgroundWriterPrefersMetadataPriorityBeforeBackgroundAge)
{
    ASSERT_EQ(policy_pool_->shutdown(&ctx_), Status::OK) << ctx_.message;
    policy_pool_.reset();

    BufferPool::Config config;
    config.pool_size = 8;
    config.page_size = db_.page_size();
    config.enable_background_writer = true;
    config.bgwriter_delay_ms = 250;
    config.bgwriter_max_pages = 1;
    config.dirty_ratio_low = 0.10;
    config.dirty_ratio_high = 1.00;
    config.dirty_ratio_checkpoint = 1.00;

    policy_pool_ = std::make_unique<BufferPool>(&db_, config);
    ASSERT_EQ(policy_pool_->initialize(&ctx_), Status::OK) << ctx_.message;

    void *buffer = nullptr;
    const uint32_t generic_page = allocateHeapPage();
    ASSERT_EQ(policy_pool_->pinPage(generic_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(generic_page, true, &ctx_), Status::OK) << ctx_.message;

    const uint32_t metadata_page = allocateTypedPage(PAGE_TYPE_SYSTEM_STATE);
    BufferPool::MgaFrameHints metadata_hints;
    metadata_hints.page_class = BufferPool::MgaPageClass::SYSTEM_META;
    ASSERT_EQ(policy_pool_->pinPage(metadata_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(metadata_page),
                                                       metadata_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(metadata_page, true, &ctx_), Status::OK) << ctx_.message;

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (policy_pool_->getStats().bgwriter_pages_written >= 1u)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_GE(policy_pool_->getStats().bgwriter_pages_written, 1u);

    const auto metadata_snapshot = frameSnapshot(metadata_page);
    const auto generic_snapshot = frameSnapshot(generic_page);
    EXPECT_EQ(metadata_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::METADATA_PRIORITY);
    EXPECT_EQ(metadata_snapshot.dirty_state,
              BufferPool::DirtyState::DirtyFlushedPendingFsync);
    EXPECT_EQ(generic_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::BACKGROUND_AGE);
    EXPECT_EQ(generic_snapshot.dirty_state,
              BufferPool::DirtyState::DirtyUnscheduled);
}

TEST_F(BufferPoolMgaPolicyTest,
       BackgroundWriterSkipsTemporaryWorkPages)
{
    ASSERT_EQ(policy_pool_->shutdown(&ctx_), Status::OK) << ctx_.message;
    policy_pool_.reset();

    BufferPool::Config config = makePolicyConfig();
    config.enable_background_writer = true;
    config.bgwriter_delay_ms = 250;
    config.bgwriter_max_pages = 1;
    config.dirty_ratio_low = 0.10;
    config.dirty_ratio_high = 1.00;
    config.dirty_ratio_checkpoint = 1.00;

    policy_pool_ = std::make_unique<BufferPool>(&db_, config);
    ASSERT_EQ(policy_pool_->initialize(&ctx_), Status::OK) << ctx_.message;

    void *buffer = nullptr;
    const uint32_t temp_page = allocateTypedPage(PAGE_TYPE_TEMP_HEAP);
    ASSERT_EQ(policy_pool_->pinPage(temp_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_EQ(policy_pool_->unpinPage(temp_page, true, &ctx_), Status::OK) << ctx_.message;

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    EXPECT_EQ(policy_pool_->getStats().bgwriter_pages_written, 0u);

    const auto temp_snapshot = frameSnapshot(temp_page);
    EXPECT_TRUE(temp_snapshot.is_dirty);
    EXPECT_EQ(temp_snapshot.dirty_state, BufferPool::DirtyState::DirtyUnscheduled);
    EXPECT_EQ(temp_snapshot.writeback_queue_state,
              BufferPool::WritebackQueueState::BACKGROUND_AGE);
    EXPECT_EQ(temp_snapshot.last_flush_generation, 0u);
}

TEST_F(BufferPoolMgaPolicyTest,
       SessionTempHeapPagesSurviveEvictionReloadWithoutDurableGenerations)
{
    void *buffer = nullptr;
    const uint32_t temp_heap_page =
        allocateTypedPage(PAGE_TYPE_HEAP,
                          static_cast<uint32_t>(PAGE_FLAG_TEMPORARY_WORK),
                          91);

    ASSERT_EQ(policy_pool_->pinPage(temp_heap_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_NE(buffer, nullptr);
    auto *header = reinterpret_cast<PageHeader *>(buffer);
    header->version = 77;
    ASSERT_EQ(policy_pool_->unpinPage(temp_heap_page, true, &ctx_), Status::OK) << ctx_.message;

    std::vector<uint32_t> pressure_pages;
    for (int i = 0; i < 64; ++i)
    {
        pressure_pages.push_back(allocateHeapPage());
    }
    pressurePages(pressure_pages, BufferPool::AccessStrategy::Normal);

    const auto evicted_snapshot = frameSnapshot(temp_heap_page);
    EXPECT_FALSE(evicted_snapshot.resident);

    ASSERT_EQ(policy_pool_->pinPage(temp_heap_page, &buffer, &ctx_), Status::OK) << ctx_.message;
    ASSERT_NE(buffer, nullptr);
    header = reinterpret_cast<PageHeader *>(buffer);
    EXPECT_EQ(header->version, 77);
    EXPECT_NE(header->flags & static_cast<uint16_t>(PAGE_FLAG_TEMPORARY_WORK), 0u);
    EXPECT_EQ(header->flush_generation, 0u);
    EXPECT_EQ(header->checkpoint_generation, 0u);
    ASSERT_EQ(policy_pool_->unpinPage(temp_heap_page, false, &ctx_), Status::OK) << ctx_.message;
}

TEST_F(BufferPoolMgaPolicyTest,
       ScanBulkRingDomainSurrendersBeforeHotAndVersionDomains)
{
    touchPage(0);

    const uint32_t version_page_a = allocateHeapPage();
    const uint32_t version_page_b = allocateHeapPage();
    const uint32_t hot_page = allocateTypedPage(PAGE_TYPE_BTREE_INTERNAL);

    touchPage(version_page_a);
    touchPage(version_page_b);
    touchPage(hot_page,
              BufferPool::AccessStrategy::Normal,
              BufferPool::WorkloadClass::IndexProbe);

    BufferPool::MgaFrameHints version_hints;
    version_hints.page_class = BufferPool::MgaPageClass::VERSION_ROOT;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(version_page_a),
                                                       version_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(version_page_b),
                                                       version_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;

    touchPage(version_page_a);
    touchPage(version_page_b);

    std::vector<uint32_t> scan_pages;
    for (int i = 0; i < 3; ++i)
    {
        const uint32_t page_id = allocateHeapPage();
        scan_pages.push_back(page_id);
        touchPage(page_id,
                  BufferPool::AccessStrategy::Normal,
                  BufferPool::WorkloadClass::SequentialScan);
    }

    const auto scan_before = domainSnapshot(BufferPool::PolicyDomain::ScanBulkRing);
    ASSERT_GE(scan_before.resident_pages, 3u);
    EXPECT_GT(scan_before.borrowed_pages, 0u);

    std::vector<uint32_t> pressure_pages;
    for (int i = 0; i < 16; ++i)
    {
        pressure_pages.push_back(allocateHeapPage());
    }
    pressurePages(pressure_pages, BufferPool::AccessStrategy::Normal);

    size_t resident_scan_pages = 0;
    for (uint32_t page_id : scan_pages)
    {
        if (frameSnapshot(page_id).resident)
        {
            ++resident_scan_pages;
        }
    }

    const auto scan_after = domainSnapshot(BufferPool::PolicyDomain::ScanBulkRing);
    const auto version_after = domainSnapshot(BufferPool::PolicyDomain::VersionUndo);
    const auto hot_after = frameSnapshot(hot_page);

    EXPECT_LT(resident_scan_pages, scan_pages.size());
    EXPECT_LT(scan_after.borrowed_pages, scan_before.borrowed_pages);
    EXPECT_GE(version_after.resident_pages, 2u);
    EXPECT_TRUE(frameSnapshot(version_page_a).resident);
    EXPECT_TRUE(frameSnapshot(version_page_b).resident);
    EXPECT_TRUE(hot_after.resident);
    EXPECT_EQ(hot_after.policy_domain, BufferPool::PolicyDomain::HotOltp);
}

TEST_F(BufferPoolMgaPolicyTest,
       TemporaryWorkDomainSurrendersBeforeDurableDomains)
{
    touchPage(0);

    const uint32_t version_page_a = allocateHeapPage();
    const uint32_t version_page_b = allocateHeapPage();
    const uint32_t hot_page = allocateTypedPage(PAGE_TYPE_BTREE_INTERNAL);

    touchPage(version_page_a);
    touchPage(version_page_b);
    touchPage(hot_page,
              BufferPool::AccessStrategy::Normal,
              BufferPool::WorkloadClass::IndexProbe);

    BufferPool::MgaFrameHints version_hints;
    version_hints.page_class = BufferPool::MgaPageClass::VERSION_ROOT;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(version_page_a),
                                                       version_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(version_page_b),
                                                       version_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;

    touchPage(version_page_a);
    touchPage(version_page_b);

    std::vector<uint32_t> temp_pages;
    for (int i = 0; i < 3; ++i)
    {
        const uint32_t page_id = allocateTypedPage(PAGE_TYPE_TEMP_HEAP);
        temp_pages.push_back(page_id);
        touchPage(page_id);
    }

    const auto temp_before = domainSnapshot(BufferPool::PolicyDomain::TemporaryWork);
    ASSERT_GE(temp_before.resident_pages, 3u);
    EXPECT_GT(temp_before.borrowed_pages, 0u);

    std::vector<uint32_t> pressure_pages;
    for (int i = 0; i < 16; ++i)
    {
        pressure_pages.push_back(allocateHeapPage());
    }
    pressurePages(pressure_pages, BufferPool::AccessStrategy::Normal);

    size_t resident_temp_pages = 0;
    for (uint32_t page_id : temp_pages)
    {
        if (frameSnapshot(page_id).resident)
        {
            ++resident_temp_pages;
        }
    }

    const auto temp_after = domainSnapshot(BufferPool::PolicyDomain::TemporaryWork);
    EXPECT_LT(resident_temp_pages, temp_pages.size());
    EXPECT_LT(temp_after.borrowed_pages, temp_before.borrowed_pages);
    EXPECT_TRUE(frameSnapshot(version_page_a).resident);
    EXPECT_TRUE(frameSnapshot(version_page_b).resident);
    EXPECT_TRUE(frameSnapshot(hot_page).resident);
}

TEST_F(BufferPoolMgaPolicyTest,
       HardReservedDomainsDoNotEvictAtMinimumReservation)
{
    touchPage(0);

    const uint32_t version_page_a = allocateHeapPage();
    const uint32_t version_page_b = allocateHeapPage();
    touchPage(version_page_a);
    touchPage(version_page_b);

    BufferPool::MgaFrameHints version_hints;
    version_hints.page_class = BufferPool::MgaPageClass::VERSION_ROOT;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(version_page_a),
                                                       version_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;
    ASSERT_EQ(policy_pool_->publishMgaFrameHintsGlobal(convertPageIDtoGPID(version_page_b),
                                                       version_hints,
                                                       &ctx_),
              Status::OK)
        << ctx_.message;

    touchPage(version_page_a);
    touchPage(version_page_b);

    std::vector<uint32_t> filler_pages;
    for (int i = 0; i < 5; ++i)
    {
        const uint32_t page_id = (i < 2) ? allocateTypedPage(PAGE_TYPE_TEMP_HEAP)
                                         : allocateHeapPage();
        filler_pages.push_back(page_id);
        touchPage(page_id,
                  BufferPool::AccessStrategy::Normal,
                  (i == 2) ? BufferPool::WorkloadClass::SequentialScan
                           : BufferPool::WorkloadClass::Unspecified);
    }

    std::vector<uint32_t> pressure_pages;
    for (int i = 0; i < 24; ++i)
    {
        pressure_pages.push_back(allocateHeapPage());
    }
    pressurePages(pressure_pages, BufferPool::AccessStrategy::Normal);

    const auto critical = domainSnapshot(BufferPool::PolicyDomain::CriticalSystem);
    const auto version = domainSnapshot(BufferPool::PolicyDomain::VersionUndo);
    const auto stats = policy_pool_->getStats();

    // The critical-system floor is anchored by the TX_STATE hard-protect path.
    // The explicit reservation-breach counter is therefore most meaningful on
    // VersionUndo, whose minimum is enforced through domain-aware victim search.
    EXPECT_GE(critical.resident_pages, critical.budget.min_frames);
    EXPECT_GE(version.resident_pages, version.budget.min_frames);
    EXPECT_TRUE(frameSnapshot(0).resident);
    EXPECT_TRUE(frameSnapshot(version_page_a).resident);
    EXPECT_TRUE(frameSnapshot(version_page_b).resident);
    EXPECT_EQ(critical.reservation_breach_count, 0u);
    EXPECT_GT(version.reservation_breach_count, 0u);
    EXPECT_EQ(critical.emergency_breach_count, 0u);
    EXPECT_EQ(version.emergency_breach_count, 0u);
    EXPECT_EQ(stats.mga_protected_set_collapse_events, 0u);
}
