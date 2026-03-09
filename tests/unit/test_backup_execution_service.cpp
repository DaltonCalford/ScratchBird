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
#include <filesystem>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/backup_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/page_manager.h"

using namespace scratchbird::core;

namespace {

std::string normalizePath(const std::filesystem::path& path)
{
    return std::filesystem::absolute(path).lexically_normal().string();
}

} // namespace

class BackupExecutionServiceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static std::atomic<int> counter{0};
        const std::string suffix =
            std::to_string(getpid()) + "_" + std::to_string(counter++);
        work_dir_ = std::filesystem::temp_directory_path() / ("scratchbird_backup_job_" + suffix);
        backup_dir_ = work_dir_ / "backups";
        std::filesystem::create_directories(backup_dir_);
        db_path_ = (work_dir_ / "test.sbdb").string();

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);
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

    BackupConfig makeConfig(BackupType type, const std::string& label) const
    {
        BackupConfig config;
        config.type = type;
        config.compression = CompressionType::NONE;
        config.label = label;
        return config;
    }

    CatalogManager::BackupHistoryCatalogInfo lookupHistoryRow(const std::filesystem::path& path)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::BackupHistoryCatalogInfo> rows;
        EXPECT_EQ(catalog_->listBackupHistoryCatalogEntries(db_.uuid(), rows, &ctx), Status::OK)
            << ctx.message;

        const std::string normalized_path = normalizePath(path);
        auto it = std::find_if(rows.begin(), rows.end(), [&](const auto& row) {
            return row.storage_uri == normalized_path;
        });
        EXPECT_NE(it, rows.end());
        return it == rows.end() ? CatalogManager::BackupHistoryCatalogInfo{} : *it;
    }

    std::filesystem::path work_dir_;
    std::filesystem::path backup_dir_;
    std::string db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
};

TEST_F(BackupExecutionServiceTest, ResumeFullBackupFromCheckpoint)
{
    BackupManager backup_mgr(&db_);
    const auto backup_path = backup_dir_ / "resume_full.sbkp";

    BackupExecutionConfig chunked_exec;
    chunked_exec.max_pages_per_invocation = 1;

    BackupJobResult result;
    ErrorContext ctx;
    ASSERT_EQ(backup_mgr.executeBackupJob(backup_path.string(),
                                          makeConfig(BackupType::FULL, "resume-full"),
                                          chunked_exec,
                                          &result,
                                          nullptr,
                                          &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(result.state, BackupJobState::PAUSED);
    EXPECT_EQ(result.pages_processed, 1u);
    EXPECT_GT(result.pages_total, result.pages_processed);
    EXPECT_TRUE(std::filesystem::exists(backup_path.string() + ".part"));
    EXPECT_TRUE(std::filesystem::exists(backup_path.string() + ".sbkjob"));
    EXPECT_FALSE(std::filesystem::exists(backup_path));

    BackupJobResult paused_result;
    ErrorContext paused_ctx;
    ASSERT_EQ(backup_mgr.getBackupJobResult(backup_path.string(), &paused_result, &paused_ctx),
              Status::OK)
        << paused_ctx.message;
    EXPECT_EQ(paused_result.state, BackupJobState::PAUSED);
    EXPECT_EQ(paused_result.pages_processed, 1u);

    ErrorContext resume_ctx;
    ASSERT_EQ(backup_mgr.resumeBackupJob(backup_path.string(),
                                         BackupExecutionConfig{},
                                         &result,
                                         nullptr,
                                         &resume_ctx),
              Status::OK)
        << resume_ctx.message;
    EXPECT_EQ(result.state, BackupJobState::COMPLETED);
    EXPECT_EQ(result.resume_count, 1u);
    EXPECT_EQ(result.pages_processed, result.pages_total);
    EXPECT_TRUE(std::filesystem::exists(backup_path));
    EXPECT_FALSE(std::filesystem::exists(backup_path.string() + ".part"));

    RecordProperty("pages_total", std::to_string(result.pages_total));
    RecordProperty("bytes_written", std::to_string(result.bytes_written));
    RecordProperty("resume_count", std::to_string(result.resume_count));
    RecordProperty("elapsed_micros",
                   std::to_string(result.completed_time - result.started_time));

    BackupMetadata metadata;
    ErrorContext metadata_ctx;
    ASSERT_EQ(backup_mgr.getBackupMetadata(backup_path.string(), &metadata, &metadata_ctx),
              Status::OK)
        << metadata_ctx.message;
    EXPECT_EQ(metadata.backup_id, result.backup_id);
    EXPECT_EQ(metadata.total_pages, result.pages_total);

    const auto history_row = lookupHistoryRow(backup_path);
    EXPECT_EQ(history_row.backup_status, CatalogManager::BackupHistoryStatus::SUCCESS);
}

TEST_F(BackupExecutionServiceTest, ResumeIncrementalBackupPreservesParentLineage)
{
    BackupManager backup_mgr(&db_);
    const auto full_path = backup_dir_ / "base_full.sbkp";
    const auto incr_path = backup_dir_ / "resume_incr.sbkp";

    BackupJobResult full_result;
    ErrorContext ctx;
    ASSERT_EQ(backup_mgr.executeBackupJob(full_path.string(),
                                          makeConfig(BackupType::FULL, "base-full"),
                                          BackupExecutionConfig{},
                                          &full_result,
                                          nullptr,
                                          &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(full_result.state, BackupJobState::COMPLETED);

    BackupMetadata full_metadata;
    ErrorContext full_metadata_ctx;
    ASSERT_EQ(backup_mgr.getBackupMetadata(full_path.string(), &full_metadata, &full_metadata_ctx),
              Status::OK)
        << full_metadata_ctx.message;

    backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 0));
    backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 1));

    BackupConfig incr_config = makeConfig(BackupType::INCREMENTAL, "resume-incremental");
    incr_config.parent_backup_id = full_metadata.backup_id;
    incr_config.parent_backup_path = full_path.string();

    BackupExecutionConfig chunked_exec;
    chunked_exec.max_pages_per_invocation = 1;

    BackupJobResult incr_result;
    ErrorContext incr_start_ctx;
    ASSERT_EQ(backup_mgr.executeBackupJob(incr_path.string(),
                                          incr_config,
                                          chunked_exec,
                                          &incr_result,
                                          nullptr,
                                          &incr_start_ctx),
              Status::OK)
        << incr_start_ctx.message;
    ASSERT_EQ(incr_result.state, BackupJobState::PAUSED);
    EXPECT_EQ(incr_result.parent_backup_id, full_metadata.backup_id);

    ErrorContext incr_resume_ctx;
    ASSERT_EQ(backup_mgr.resumeBackupJob(incr_path.string(),
                                         BackupExecutionConfig{},
                                         &incr_result,
                                         nullptr,
                                         &incr_resume_ctx),
              Status::OK)
        << incr_resume_ctx.message;
    EXPECT_EQ(incr_result.state, BackupJobState::COMPLETED);
    EXPECT_EQ(incr_result.resume_count, 1u);

    RecordProperty("pages_total", std::to_string(incr_result.pages_total));
    RecordProperty("bytes_written", std::to_string(incr_result.bytes_written));
    RecordProperty("resume_count", std::to_string(incr_result.resume_count));
    RecordProperty("elapsed_micros",
                   std::to_string(incr_result.completed_time - incr_result.started_time));

    BackupMetadata incr_metadata;
    ErrorContext incr_metadata_ctx;
    ASSERT_EQ(backup_mgr.getBackupMetadata(incr_path.string(), &incr_metadata, &incr_metadata_ctx),
              Status::OK)
        << incr_metadata_ctx.message;
    EXPECT_EQ(incr_metadata.parent_id, full_metadata.backup_id);

    std::vector<std::string> chain;
    ErrorContext chain_ctx;
    ASSERT_EQ(backup_mgr.buildBackupChain(incr_path.string(), &chain, &chain_ctx), Status::OK)
        << chain_ctx.message;
    ASSERT_EQ(chain.size(), 2u);
    EXPECT_EQ(chain[0], normalizePath(full_path));
    EXPECT_EQ(chain[1], normalizePath(incr_path));
    EXPECT_TRUE(backup_mgr.getModifiedPages().empty());
}

TEST_F(BackupExecutionServiceTest, MissingTempArtifactFailsClosedAndMarksHistoryFailed)
{
    BackupManager backup_mgr(&db_);
    const auto backup_path = backup_dir_ / "stale_resume.sbkp";

    BackupExecutionConfig chunked_exec;
    chunked_exec.max_pages_per_invocation = 1;

    BackupJobResult result;
    ErrorContext ctx;
    ASSERT_EQ(backup_mgr.executeBackupJob(backup_path.string(),
                                          makeConfig(BackupType::FULL, "stale-full"),
                                          chunked_exec,
                                          &result,
                                          nullptr,
                                          &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(result.state, BackupJobState::PAUSED);

    ASSERT_TRUE(std::filesystem::remove(backup_path.string() + ".part"));

    BackupJobResult failed_result;
    ErrorContext stale_resume_ctx;
    EXPECT_EQ(backup_mgr.resumeBackupJob(backup_path.string(),
                                         BackupExecutionConfig{},
                                         &failed_result,
                                         nullptr,
                                         &stale_resume_ctx),
              Status::FILE_NOT_FOUND);
    EXPECT_EQ(failed_result.state, BackupJobState::FAILED);
    EXPECT_FALSE(failed_result.error_message.empty());

    BackupJobResult queried_result;
    ErrorContext queried_ctx;
    ASSERT_EQ(backup_mgr.getBackupJobResult(backup_path.string(), &queried_result, &queried_ctx),
              Status::OK)
        << queried_ctx.message;
    EXPECT_EQ(queried_result.state, BackupJobState::FAILED);
    EXPECT_FALSE(std::filesystem::exists(backup_path));

    const auto history_row = lookupHistoryRow(backup_path);
    EXPECT_EQ(history_row.backup_status, CatalogManager::BackupHistoryStatus::FAILED);
}
