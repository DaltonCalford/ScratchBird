/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// Section 35 invariant: restore validation rehearsals anchor bounded restore
// legality and validation behavior. They do not imply PITR or donor-log
// recovery semantics.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

#include "scratchbird/core/backup_manager.h"
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

class RestoreValidationRehearsalTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static std::atomic<int> counter{0};
        const std::string suffix =
            std::to_string(getpid()) + "_" + std::to_string(counter++);
        work_dir_ =
            std::filesystem::temp_directory_path() / ("scratchbird_restore_validation_" + suffix);
        backup_dir_ = work_dir_ / "backups";
        std::filesystem::create_directories(backup_dir_);
        db_path_ = (work_dir_ / "test.sbdb").string();
        restore_path_ = work_dir_ / "restore.sbdb";
        chain_restore_path_ = work_dir_ / "restore_chain.sbdb";

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

    BackupConfig makeBackupConfig(BackupType type, const std::string& label) const
    {
        BackupConfig config;
        config.type = type;
        config.compression = CompressionType::NONE;
        config.label = label;
        return config;
    }

    BackupMetadata lookupMetadata(BackupManager& backup_mgr, const std::filesystem::path& backup_path)
    {
        BackupMetadata metadata;
        ErrorContext ctx;
        EXPECT_EQ(backup_mgr.getBackupMetadata(backup_path.string(), &metadata, &ctx), Status::OK)
            << ctx.message;
        return metadata;
    }

    std::filesystem::path work_dir_;
    std::filesystem::path backup_dir_;
    std::filesystem::path restore_path_;
    std::filesystem::path chain_restore_path_;
    std::string db_path_;
    Database db_;
    std::unique_ptr<ConnectionContext> conn_;
};

TEST_F(RestoreValidationRehearsalTest, FullBackupValidationPassesDeterministicChecks)
{
    BackupManager backup_mgr(&db_);
    const auto backup_path = backup_dir_ / "full.sbkp";

    ErrorContext backup_ctx;
    ASSERT_EQ(backup_mgr.createBackup(backup_path.string(),
                                      makeBackupConfig(BackupType::FULL, "validate-full"),
                                      nullptr,
                                      &backup_ctx),
              Status::OK)
        << backup_ctx.message;

    RestoreValidationConfig validation_config;
    validation_config.max_restore_micros = 5ULL * 1000ULL * 1000ULL;
    validation_config.max_rpo_micros = 60ULL * 1000ULL * 1000ULL;

    RestoreValidationResult result;
    ErrorContext validation_ctx;
    ASSERT_EQ(backup_mgr.runRestoreValidation(backup_path.string(),
                                              restore_path_.string(),
                                              validation_config,
                                              &result,
                                              nullptr,
                                              &validation_ctx),
              Status::OK)
        << validation_ctx.message;

    EXPECT_EQ(result.backup_chain.size(), 1u);
    EXPECT_EQ(result.backup_chain.front(), normalizePath(backup_path));
    EXPECT_EQ(result.target_path, normalizePath(restore_path_));
    EXPECT_EQ(result.applied_backup_count, 1u);
    EXPECT_EQ(result.verified_backup_count, 1u);
    EXPECT_TRUE(result.backup_verified);
    EXPECT_TRUE(result.restore_completed);
    EXPECT_TRUE(result.reopen_validated);
    EXPECT_TRUE(result.rollback_checkpoint_validated);
    EXPECT_TRUE(result.thresholds_passed);
    EXPECT_EQ(result.source_database_id, db_.uuid());
    EXPECT_EQ(result.restored_database_id, db_.uuid());
    EXPECT_EQ(result.rollback_checkpoint_version,
              BACKUP_ROLLBACK_CHECKPOINT_VERSION_CURRENT);
    EXPECT_EQ(result.chain_source_db_version, DB_VERSION_CURRENT);
    EXPECT_EQ(result.chain_source_db_compat_version, DB_COMPAT_VERSION_CURRENT);
    EXPECT_EQ(result.source_page_size, db_.page_size());
    EXPECT_EQ(result.restored_page_size, db_.page_size());
    EXPECT_GT(result.restored_total_pages, 0u);
    EXPECT_TRUE(std::filesystem::exists(restore_path_));

    RecordProperty("restore_elapsed_micros", std::to_string(result.restore_elapsed_micros));
    RecordProperty("measured_rpo_micros", std::to_string(result.measured_rpo_micros));
    RecordProperty("applied_backup_count", std::to_string(result.applied_backup_count));
}

TEST_F(RestoreValidationRehearsalTest, DisasterRecoveryRehearsalAppliesIncrementalChain)
{
    BackupManager backup_mgr(&db_);
    const auto full_path = backup_dir_ / "base_full.sbkp";
    const auto incremental_path = backup_dir_ / "delta.sbkp";

    ErrorContext full_ctx;
    ASSERT_EQ(backup_mgr.createBackup(full_path.string(),
                                      makeBackupConfig(BackupType::FULL, "base-full"),
                                      nullptr,
                                      &full_ctx),
              Status::OK)
        << full_ctx.message;

    backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 0));
    backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 1));

    ErrorContext incremental_ctx;
    ASSERT_EQ(backup_mgr.createIncrementalBackup(incremental_path.string(),
                                                 full_path.string(),
                                                 nullptr,
                                                 &incremental_ctx),
              Status::OK)
        << incremental_ctx.message;

    const BackupMetadata incremental_metadata = lookupMetadata(backup_mgr, incremental_path);

    RestoreValidationConfig validation_config;
    validation_config.max_restore_micros = 5ULL * 1000ULL * 1000ULL;
    validation_config.max_rpo_micros = 60ULL * 1000ULL * 1000ULL;

    RestoreValidationResult result;
    ErrorContext rehearsal_ctx;
    ASSERT_EQ(backup_mgr.runDisasterRecoveryRehearsal({full_path.string(), incremental_path.string()},
                                                      chain_restore_path_.string(),
                                                      validation_config,
                                                      &result,
                                                      nullptr,
                                                      &rehearsal_ctx),
              Status::OK)
        << rehearsal_ctx.message;

    EXPECT_EQ(result.backup_chain.size(), 2u);
    EXPECT_EQ(result.applied_backup_count, 2u);
    EXPECT_EQ(result.verified_backup_count, 2u);
    EXPECT_TRUE(result.backup_verified);
    EXPECT_TRUE(result.restore_completed);
    EXPECT_TRUE(result.reopen_validated);
    EXPECT_TRUE(result.rollback_checkpoint_validated);
    EXPECT_TRUE(result.thresholds_passed);
    EXPECT_EQ(result.recovered_until_time, incremental_metadata.end_time);
    EXPECT_EQ(result.restored_database_id, db_.uuid());
    EXPECT_EQ(result.rollback_checkpoint_version,
              BACKUP_ROLLBACK_CHECKPOINT_VERSION_CURRENT);
    EXPECT_EQ(result.chain_source_db_version, DB_VERSION_CURRENT);
    EXPECT_EQ(result.chain_source_db_compat_version, DB_COMPAT_VERSION_CURRENT);
    EXPECT_TRUE(std::filesystem::exists(chain_restore_path_));

    RecordProperty("restore_elapsed_micros", std::to_string(result.restore_elapsed_micros));
    RecordProperty("measured_rpo_micros", std::to_string(result.measured_rpo_micros));
    RecordProperty("applied_backup_count", std::to_string(result.applied_backup_count));
}

TEST_F(RestoreValidationRehearsalTest, ValidationFailsConfiguredRpoThreshold)
{
    BackupManager backup_mgr(&db_);
    const auto backup_path = backup_dir_ / "rpo_threshold.sbkp";

    ErrorContext backup_ctx;
    ASSERT_EQ(backup_mgr.createBackup(backup_path.string(),
                                      makeBackupConfig(BackupType::FULL, "rpo-threshold"),
                                      nullptr,
                                      &backup_ctx),
              Status::OK)
        << backup_ctx.message;

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    RestoreValidationConfig validation_config;
    validation_config.max_restore_micros = 5ULL * 1000ULL * 1000ULL;
    validation_config.max_rpo_micros = 1000;

    RestoreValidationResult result;
    ErrorContext validation_ctx;
    EXPECT_EQ(backup_mgr.runRestoreValidation(backup_path.string(),
                                              restore_path_.string(),
                                              validation_config,
                                              &result,
                                              nullptr,
                                              &validation_ctx),
              Status::CONFIGURATION_LIMIT_EXCEEDED);
    EXPECT_TRUE(result.restore_completed);
    EXPECT_TRUE(result.reopen_validated);
    EXPECT_FALSE(result.thresholds_passed);
    EXPECT_GT(result.measured_rpo_micros, validation_config.max_rpo_micros);
    EXPECT_NE(result.failure_reason.find("RPO"), std::string::npos);
}

TEST_F(RestoreValidationRehearsalTest, ValidationFailsClosedOnCorruptBackup)
{
    BackupManager backup_mgr(&db_);
    const auto backup_path = backup_dir_ / "corrupt.sbkp";

    ErrorContext backup_ctx;
    ASSERT_EQ(backup_mgr.createBackup(backup_path.string(),
                                      makeBackupConfig(BackupType::FULL, "corrupt-full"),
                                      nullptr,
                                      &backup_ctx),
              Status::OK)
        << backup_ctx.message;

    std::fstream backup_stream(backup_path,
                               std::ios::in | std::ios::out | std::ios::binary);
    ASSERT_TRUE(backup_stream.is_open());
    backup_stream.seekp(0);
    backup_stream.put('X');
    backup_stream.flush();
    backup_stream.close();

    RestoreValidationConfig validation_config;
    RestoreValidationResult result;
    ErrorContext validation_ctx;
    EXPECT_EQ(backup_mgr.runRestoreValidation(backup_path.string(),
                                              restore_path_.string(),
                                              validation_config,
                                              &result,
                                              nullptr,
                                              &validation_ctx),
              Status::DATA_CORRUPTED);
    EXPECT_FALSE(result.backup_verified);
    EXPECT_FALSE(result.restore_completed);
    EXPECT_FALSE(result.thresholds_passed);
    EXPECT_FALSE(result.failure_reason.empty());
    EXPECT_FALSE(std::filesystem::exists(restore_path_));
}
