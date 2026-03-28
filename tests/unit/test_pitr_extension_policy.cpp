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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/backup_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/page_manager.h"

using namespace scratchbird::core;

namespace {

std::string normalizePath(const std::filesystem::path& path)
{
    return std::filesystem::absolute(path).lexically_normal().string();
}

std::vector<uint8_t> readFileBytes(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    EXPECT_TRUE(file.is_open());
    if (!file.is_open())
    {
        return {};
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                                std::istreambuf_iterator<char>());
}

} // namespace

class PITRExtensionPolicyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static std::atomic<int> counter{0};
        const std::string suffix =
            std::to_string(getpid()) + "_" + std::to_string(counter++);
        work_dir_ =
            std::filesystem::temp_directory_path() / ("scratchbird_pitr_policy_" + suffix);
        backup_dir_ = work_dir_ / "backups";
        std::filesystem::create_directories(backup_dir_);
        db_path_ = (work_dir_ / "test.sbdb").string();
        restore_path_ = work_dir_ / "restore.sbdb";
        reference_path_ = work_dir_ / "reference.sbdb";
        latest_path_ = work_dir_ / "latest.sbdb";

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_path_, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        ensurePrimaryPages(4);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        db_.close();
        std::filesystem::remove_all(work_dir_);
    }

    void ensurePrimaryPages(uint32_t minimum_pages)
    {
        ErrorContext ctx;
        auto* page_mgr = db_.page_manager();
        ASSERT_NE(page_mgr, nullptr);

        while (db_.total_pages() < minimum_pages)
        {
            uint32_t page_id = 0;
            ASSERT_EQ(page_mgr->allocatePage(page_id, &ctx), Status::OK) << ctx.message;
        }

        ASSERT_EQ(db_.sync(&ctx), Status::OK) << ctx.message;
    }

    uint32_t allocatePrimaryPage()
    {
        ErrorContext ctx;
        auto* page_mgr = db_.page_manager();
        if (page_mgr == nullptr)
        {
            ADD_FAILURE() << "Page manager must be available";
            return 0;
        }
        uint32_t page_id = 0;
        if (page_mgr->allocatePage(page_id, &ctx) != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return 0;
        }
        if (db_.sync(&ctx) != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return 0;
        }
        return page_id;
    }

    void stampPrimaryPage(uint32_t page_id, uint8_t marker)
    {
        ErrorContext ctx;
        auto* buffer_pool = db_.buffer_pool();
        ASSERT_NE(buffer_pool, nullptr);

        void* page_buffer = nullptr;
        ASSERT_EQ(buffer_pool->pinPage(page_id, &page_buffer, &ctx), Status::OK) << ctx.message;
        auto* page_bytes = static_cast<uint8_t*>(page_buffer);
        {
            HeapPage heap_page(page_bytes, db_.page_size(), nullptr, &db_, ID{});
            ASSERT_EQ(heap_page.initialize(page_id, &ctx), Status::OK) << ctx.message;
        }
        auto* page_header = reinterpret_cast<PageHeader*>(page_bytes);
        ASSERT_EQ(page_header->page_type, PAGE_TYPE_HEAP);

        std::fill_n(page_bytes + sizeof(PageHeader), 32, marker);
        preparePageForWrite(page_bytes, db_.page_size(), page_id);

        ASSERT_EQ(buffer_pool->unpinPage(page_id, true, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.sync(&ctx), Status::OK) << ctx.message;
    }

    Status createFullBackup(const std::filesystem::path& path, const std::string& label)
    {
        BackupManager backup_mgr(&db_);
        BackupConfig config;
        config.type = BackupType::FULL;
        config.compression = CompressionType::NONE;
        config.label = label;
        ErrorContext ctx;
        return backup_mgr.createBackup(path.string(), config, nullptr, &ctx);
    }

    Status createIncrementalBackup(const std::filesystem::path& path,
                                   const std::filesystem::path& parent_path,
                                   const std::vector<uint32_t>& modified_pages)
    {
        BackupManager backup_mgr(&db_);
        backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 0));
        for (uint32_t page_id : modified_pages)
        {
            backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, page_id));
        }

        ErrorContext ctx;
        return backup_mgr.createIncrementalBackup(path.string(), parent_path.string(), nullptr, &ctx);
    }

    BackupMetadata lookupMetadata(const std::filesystem::path& backup_path)
    {
        BackupManager backup_mgr(&db_);
        ErrorContext ctx;
        BackupMetadata metadata;
        EXPECT_EQ(backup_mgr.getBackupMetadata(backup_path.string(), &metadata, &ctx), Status::OK)
            << ctx.message;
        return metadata;
    }

    std::vector<std::string> buildChain(const std::filesystem::path& backup_path)
    {
        BackupManager backup_mgr(&db_);
        ErrorContext ctx;
        std::vector<std::string> chain;
        EXPECT_EQ(backup_mgr.buildBackupChain(backup_path.string(), &chain, &ctx), Status::OK)
            << ctx.message;
        return chain;
    }

    PITRPolicy snapshotBoundaryPolicy(uint64_t retention_window_micros = 0) const
    {
        PITRPolicy policy;
        policy.schema_version = 1;
        policy.mode = PITRMode::SNAPSHOT_BOUNDARY;
        policy.retention_window_micros = retention_window_micros;
        return policy;
    }

    std::filesystem::path work_dir_;
    std::filesystem::path backup_dir_;
    std::filesystem::path restore_path_;
    std::filesystem::path reference_path_;
    std::filesystem::path latest_path_;
    std::string db_path_;
    Database db_;
    std::unique_ptr<ConnectionContext> conn_;
};

TEST_F(PITRExtensionPolicyTest, TimestampRestoreRequiresEnabledExtension)
{
    const auto full_path = backup_dir_ / "full_01.sbkp";
    const auto incr_path = backup_dir_ / "incr_01.sbkp";

    ASSERT_EQ(createFullBackup(full_path, "full-01"), Status::OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const uint32_t new_page = allocatePrimaryPage();
    ASSERT_EQ(createIncrementalBackup(incr_path, full_path, {new_page}), Status::OK);

    const BackupMetadata incr_meta = lookupMetadata(incr_path);
    const std::vector<std::string> chain = buildChain(incr_path);

    BackupManager backup_mgr(&db_);
    ErrorContext ctx;
    EXPECT_EQ(backup_mgr.restoreToPointInTime(chain,
                                              restore_path_.string(),
                                              incr_meta.end_time,
                                              nullptr,
                                              &ctx),
              Status::NOT_IMPLEMENTED);
    EXPECT_NE(ctx.message.find("disabled"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(restore_path_));
}

TEST_F(PITRExtensionPolicyTest, TimestampRestoreUsesSnapshotBoundaryAtRequestedTime)
{
    const auto full_path = backup_dir_ / "full_01.sbkp";
    const auto incr_01 = backup_dir_ / "incr_01.sbkp";
    const auto incr_02 = backup_dir_ / "incr_02.sbkp";
    const uint32_t tracked_page = allocatePrimaryPage();

    ASSERT_EQ(createFullBackup(full_path, "full-01"), Status::OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    stampPrimaryPage(tracked_page, 0x11);
    ASSERT_EQ(createIncrementalBackup(incr_01, full_path, {tracked_page}), Status::OK);
    const BackupMetadata incr_01_meta = lookupMetadata(incr_01);

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    stampPrimaryPage(tracked_page, 0x22);
    ASSERT_EQ(createIncrementalBackup(incr_02, incr_01, {tracked_page}), Status::OK);

    BackupManager backup_mgr(&db_);
    ErrorContext ctx;
    ASSERT_EQ(backup_mgr.setPITRPolicy(backup_dir_.string(), snapshotBoundaryPolicy(), &ctx), Status::OK)
        << ctx.message;

    const std::vector<std::string> latest_chain = buildChain(incr_02);
    const std::vector<std::string> reference_chain = buildChain(incr_01);

    ASSERT_EQ(backup_mgr.restoreToPointInTime(latest_chain,
                                              restore_path_.string(),
                                              incr_01_meta.end_time,
                                              nullptr,
                                              &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(backup_mgr.restoreToPointInTime(reference_chain,
                                              reference_path_.string(),
                                              0,
                                              nullptr,
                                              &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(backup_mgr.restoreToPointInTime(latest_chain,
                                              latest_path_.string(),
                                              0,
                                              nullptr,
                                              &ctx),
              Status::OK)
        << ctx.message;

    const auto time_target_bytes = readFileBytes(restore_path_);
    const auto reference_bytes = readFileBytes(reference_path_);
    const auto latest_bytes = readFileBytes(latest_path_);
    EXPECT_EQ(time_target_bytes, reference_bytes);
    EXPECT_NE(time_target_bytes, latest_bytes);
}

TEST_F(PITRExtensionPolicyTest, PITRPolicyPersistsAndAuditsSnapshotCoverage)
{
    const auto full_path = backup_dir_ / "full_01.sbkp";
    const auto incr_path = backup_dir_ / "incr_01.sbkp";

    ASSERT_EQ(createFullBackup(full_path, "full-01"), Status::OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const uint32_t new_page = allocatePrimaryPage();
    ASSERT_EQ(createIncrementalBackup(incr_path, full_path, {new_page}), Status::OK);

    BackupManager backup_mgr(&db_);
    ErrorContext ctx;
    const PITRPolicy expected_policy = snapshotBoundaryPolicy(60ULL * 1000ULL * 1000ULL);
    ASSERT_EQ(backup_mgr.setPITRPolicy(backup_dir_.string(), expected_policy, &ctx), Status::OK)
        << ctx.message;

    PITRPolicy loaded_policy;
    ASSERT_EQ(backup_mgr.getPITRPolicy(backup_dir_.string(), &loaded_policy, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded_policy.schema_version, 1u);
    EXPECT_EQ(loaded_policy.mode, PITRMode::SNAPSHOT_BOUNDARY);
    EXPECT_EQ(loaded_policy.retention_window_micros, expected_policy.retention_window_micros);

    std::vector<PITRRetentionDecision> decisions;
    ASSERT_EQ(backup_mgr.evaluatePITRRetention(backup_dir_.string(), &decisions, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(decisions.size(), 2u);

    auto full_decision = std::find_if(decisions.begin(), decisions.end(),
                                      [&](const PITRRetentionDecision& decision) {
                                          return decision.backup_path == normalizePath(full_path);
                                      });
    auto incr_decision = std::find_if(decisions.begin(), decisions.end(),
                                      [&](const PITRRetentionDecision& decision) {
                                          return decision.backup_path == normalizePath(incr_path);
                                      });
    ASSERT_NE(full_decision, decisions.end());
    ASSERT_NE(incr_decision, decisions.end());
    EXPECT_TRUE(full_decision->recoverable);
    EXPECT_TRUE(incr_decision->recoverable);
    EXPECT_FALSE(full_decision->exact_replay_available);
    EXPECT_FALSE(incr_decision->exact_replay_available);
    EXPECT_EQ(full_decision->reason, "snapshot_restore_point");
    EXPECT_EQ(incr_decision->reason, "snapshot_restore_point");
}

TEST_F(PITRExtensionPolicyTest, TimestampRestoreHonorsRetentionWindow)
{
    const auto full_path = backup_dir_ / "full_01.sbkp";
    const auto incr_01 = backup_dir_ / "incr_01.sbkp";
    const auto incr_02 = backup_dir_ / "incr_02.sbkp";

    ASSERT_EQ(createFullBackup(full_path, "full-01"), Status::OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    const uint32_t first_new_page = allocatePrimaryPage();
    ASSERT_EQ(createIncrementalBackup(incr_01, full_path, {first_new_page}), Status::OK);
    const BackupMetadata incr_01_meta = lookupMetadata(incr_01);

    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    const uint32_t second_new_page = allocatePrimaryPage();
    ASSERT_EQ(createIncrementalBackup(incr_02, incr_01, {second_new_page}), Status::OK);

    BackupManager backup_mgr(&db_);
    ErrorContext ctx;
    ASSERT_EQ(backup_mgr.setPITRPolicy(backup_dir_.string(), snapshotBoundaryPolicy(1000), &ctx), Status::OK)
        << ctx.message;

    const std::vector<std::string> chain = buildChain(incr_02);
    EXPECT_EQ(backup_mgr.restoreToPointInTime(chain,
                                              restore_path_.string(),
                                              incr_01_meta.end_time,
                                              nullptr,
                                              &ctx),
              Status::CONFIGURATION_LIMIT_EXCEEDED);
    EXPECT_NE(ctx.message.find("retention window"), std::string::npos);
}

TEST_F(PITRExtensionPolicyTest, DirectRestoreRejectsTargetLsnRequests)
{
    const auto full_path = backup_dir_ / "full_01.sbkp";

    ASSERT_EQ(createFullBackup(full_path, "full-01"), Status::OK);

    BackupManager backup_mgr(&db_);
    RestoreConfig config;
    config.target_lsn = "00000001/00000001";
    ErrorContext ctx;
    EXPECT_EQ(backup_mgr.restoreBackup(full_path.string(),
                                       restore_path_.string(),
                                       config,
                                       nullptr,
                                       &ctx),
              Status::NOT_IMPLEMENTED);
    EXPECT_NE(ctx.message.find("LSN-based PITR"), std::string::npos);
}
