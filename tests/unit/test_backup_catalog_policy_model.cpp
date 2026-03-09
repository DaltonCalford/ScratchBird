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
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/backup_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"

using namespace scratchbird::core;

namespace {

std::string normalizePath(const std::filesystem::path& path)
{
    return std::filesystem::absolute(path).lexically_normal().string();
}

bool isZeroId(const ID& id)
{
    return id == ID{};
}

} // namespace

class BackupCatalogPolicyModelTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static std::atomic<int> counter{0};
        const std::string suffix =
            std::to_string(getpid()) + "_" + std::to_string(counter++);
        work_dir_ = std::filesystem::temp_directory_path() / ("scratchbird_backup_model_" + suffix);
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
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        db_.close();
        std::filesystem::remove_all(work_dir_);
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
                                   const std::filesystem::path& parent_path)
    {
        BackupManager backup_mgr(&db_);
        backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 0));
        ErrorContext ctx;
        return backup_mgr.createIncrementalBackup(path.string(), parent_path.string(), nullptr, &ctx);
    }

    std::filesystem::path work_dir_;
    std::filesystem::path backup_dir_;
    std::string db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
};

TEST_F(BackupCatalogPolicyModelTest, CreateBackupPersistsCatalogHistoryAndChainMetadata)
{
    const auto full_path = backup_dir_ / "full_01.sbkp";
    const auto incr_path = backup_dir_ / "incr_01.sbkp";

    ASSERT_EQ(createFullBackup(full_path, "full-01"), Status::OK);
    ASSERT_EQ(createIncrementalBackup(incr_path, full_path), Status::OK);

    BackupManager backup_mgr(&db_);
    ErrorContext ctx;
    BackupMetadata full_metadata;
    BackupMetadata incr_metadata;
    ASSERT_EQ(backup_mgr.getBackupMetadata(full_path.string(), &full_metadata, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(backup_mgr.getBackupMetadata(incr_path.string(), &incr_metadata, &ctx), Status::OK)
        << ctx.message;

    EXPECT_FALSE(isZeroId(full_metadata.backup_id));
    EXPECT_FALSE(isZeroId(incr_metadata.backup_id));
    EXPECT_EQ(incr_metadata.parent_id, full_metadata.backup_id);
    EXPECT_EQ(full_metadata.rollback_checkpoint_version,
              BACKUP_ROLLBACK_CHECKPOINT_VERSION_CURRENT);
    EXPECT_EQ(full_metadata.source_database_id, db_.uuid());
    EXPECT_EQ(full_metadata.source_db_version, DB_VERSION_CURRENT);
    EXPECT_EQ(full_metadata.source_db_compat_version, DB_COMPAT_VERSION_CURRENT);
    EXPECT_EQ(full_metadata.rollback_flags, BACKUP_ROLLBACK_REQUIRED_FLAGS);
    EXPECT_EQ(incr_metadata.rollback_checkpoint_version,
              BACKUP_ROLLBACK_CHECKPOINT_VERSION_CURRENT);
    EXPECT_EQ(incr_metadata.source_database_id, db_.uuid());
    EXPECT_EQ(incr_metadata.source_db_version, DB_VERSION_CURRENT);
    EXPECT_EQ(incr_metadata.source_db_compat_version, DB_COMPAT_VERSION_CURRENT);
    EXPECT_EQ(incr_metadata.rollback_flags, BACKUP_ROLLBACK_REQUIRED_FLAGS);

    std::vector<std::string> chain;
    ASSERT_EQ(backup_mgr.buildBackupChain(incr_path.string(), &chain, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(chain.size(), 2u);
    EXPECT_EQ(chain[0], normalizePath(full_path));
    EXPECT_EQ(chain[1], normalizePath(incr_path));

    std::vector<BackupMetadata> backups;
    ASSERT_EQ(backup_mgr.listBackups(backup_dir_.string(), &backups, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(backups.size(), 2u);

    const auto catalog_path = backup_dir_ / ".scratchbird_backup_catalog.sbcat";
    ASSERT_TRUE(std::filesystem::exists(catalog_path));
    std::ifstream catalog_file(catalog_path, std::ios::binary);
    ASSERT_TRUE(catalog_file.is_open());

    char magic[8]{};
    uint32_t version = 0;
    catalog_file.read(magic, sizeof(magic));
    catalog_file.read(reinterpret_cast<char*>(&version), sizeof(version));
    EXPECT_EQ(std::string(magic, sizeof(magic)), "SBCAT001");
    EXPECT_EQ(version, 2u);

    std::vector<CatalogManager::BackupHistoryCatalogInfo> backup_rows;
    ASSERT_EQ(catalog_->listBackupHistoryCatalogEntries(db_.uuid(), backup_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(backup_rows.size(), 2u);

    auto full_row = std::find_if(backup_rows.begin(), backup_rows.end(),
                                 [&](const auto& row) {
                                     return row.storage_uri == normalizePath(full_path);
                                 });
    auto incr_row = std::find_if(backup_rows.begin(), backup_rows.end(),
                                 [&](const auto& row) {
                                     return row.storage_uri == normalizePath(incr_path);
                                 });
    ASSERT_NE(full_row, backup_rows.end());
    ASSERT_NE(incr_row, backup_rows.end());
    EXPECT_EQ(full_row->backup_status, CatalogManager::BackupHistoryStatus::SUCCESS);
    EXPECT_EQ(incr_row->backup_status, CatalogManager::BackupHistoryStatus::SUCCESS);
}

TEST_F(BackupCatalogPolicyModelTest, PolicyPersistsAndEvaluatesRetentionDeterministically)
{
    const auto full_01 = backup_dir_ / "full_01.sbkp";
    const auto full_02 = backup_dir_ / "full_02.sbkp";
    const auto incr_02 = backup_dir_ / "incr_02.sbkp";

    ASSERT_EQ(createFullBackup(full_01, "full-01"), Status::OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ASSERT_EQ(createFullBackup(full_02, "full-02"), Status::OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ASSERT_EQ(createIncrementalBackup(incr_02, full_02), Status::OK);

    BackupManager backup_mgr(&db_);
    BackupPolicy policy;
    policy.schema_version = 1;
    policy.enabled = true;
    policy.policy_name = "tight-local";
    policy.storage_profile = "local_fs";
    policy.retention_rules = {
        BackupRetentionRule{BackupType::FULL, 1, false, 0},
        BackupRetentionRule{BackupType::INCREMENTAL, 1, true, 0},
        BackupRetentionRule{BackupType::DIFFERENTIAL, 0, true, 0},
    };

    ErrorContext ctx;
    ASSERT_EQ(backup_mgr.setBackupPolicy(backup_dir_.string(), policy, &ctx), Status::OK)
        << ctx.message;

    BackupPolicy loaded_policy;
    ASSERT_EQ(backup_mgr.getBackupPolicy(backup_dir_.string(), &loaded_policy, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded_policy.schema_version, 1u);
    EXPECT_EQ(loaded_policy.policy_name, "tight-local");
    ASSERT_EQ(loaded_policy.retention_rules.size(), 3u);

    std::vector<BackupRetentionDecision> decisions;
    ASSERT_EQ(backup_mgr.evaluateRetentionPolicy(backup_dir_.string(), &decisions, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(decisions.size(), 3u);

    auto lookup_decision = [&](const std::filesystem::path& path) {
        return std::find_if(decisions.begin(), decisions.end(),
                            [&](const BackupRetentionDecision& decision) {
                                return decision.backup_path == normalizePath(path);
                            });
    };

    const auto full_01_decision = lookup_decision(full_01);
    const auto full_02_decision = lookup_decision(full_02);
    const auto incr_02_decision = lookup_decision(incr_02);

    ASSERT_NE(full_01_decision, decisions.end());
    ASSERT_NE(full_02_decision, decisions.end());
    ASSERT_NE(incr_02_decision, decisions.end());

    EXPECT_FALSE(full_01_decision->retain);
    EXPECT_EQ(full_01_decision->reason, "retention_limit_exceeded");
    EXPECT_TRUE(full_02_decision->retain);
    EXPECT_TRUE(incr_02_decision->retain);
}
