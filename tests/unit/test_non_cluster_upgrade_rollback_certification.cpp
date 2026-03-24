#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "scratchbird/core/backup_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/page_manager.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace {

bool readFullyAt(int fd, void* buffer, size_t size, off_t offset)
{
    auto* dst = static_cast<uint8_t*>(buffer);
    size_t transferred = 0;
    while (transferred < size)
    {
        const ssize_t rc = ::pread(fd,
                                   dst + transferred,
                                   size - transferred,
                                   offset + static_cast<off_t>(transferred));
        if (rc <= 0)
        {
            return false;
        }
        transferred += static_cast<size_t>(rc);
    }
    return true;
}

bool writeFullyAt(int fd, const void* buffer, size_t size, off_t offset)
{
    const auto* src = static_cast<const uint8_t*>(buffer);
    size_t transferred = 0;
    while (transferred < size)
    {
        const ssize_t rc = ::pwrite(fd,
                                    src + transferred,
                                    size - transferred,
                                    offset + static_cast<off_t>(transferred));
        if (rc <= 0)
        {
            return false;
        }
        transferred += static_cast<size_t>(rc);
    }
    return true;
}

std::string normalizePath(const std::filesystem::path& path)
{
    return std::filesystem::absolute(path).lexically_normal().string();
}

class NonClusterUpgradeRollbackCertificationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        work_dir_ = scratchbird::testing::uniqueTestDbPath(
            "test_non_cluster_upgrade_rollback_certification", "");
        backup_dir_ = work_dir_ / "backups";
        std::filesystem::create_directories(backup_dir_);
        db_path_ = (work_dir_ / "cert.sbdb").string();
        restore_path_ = work_dir_ / "restore.sbdb";
        rehearsal_restore_path_ = work_dir_ / "restore_chain.sbdb";

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

    RestoreValidationConfig makeValidationConfig() const
    {
        RestoreValidationConfig config;
        config.max_restore_micros = 5ULL * 1000ULL * 1000ULL;
        config.max_rpo_micros = 60ULL * 1000ULL * 1000ULL;
        return config;
    }

    void rewriteDatabaseHeaderVersions(uint32_t db_version, uint32_t db_compat_version)
    {
        int fd = ::open(db_path_.c_str(), O_RDWR);
        ASSERT_GE(fd, 0);

        PageHeader page_header{};
        ASSERT_TRUE(readFullyAt(fd, &page_header, sizeof(page_header), 0));
        ASSERT_TRUE(isValidAlphaPageSize(page_header.page_size));

        std::vector<uint8_t> page(page_header.page_size, 0);
        ASSERT_TRUE(readFullyAt(fd, page.data(), page.size(), 0));

        auto* header = reinterpret_cast<DatabaseHeader*>(page.data());
        header->db_version = db_version;
        header->db_compat_version = db_compat_version;
        if (header->page_header.header_bytes == 0u)
        {
            header->page_header.header_bytes = CANONICAL_PAGE_HEADER_BYTES;
        }
        header->page_header.payload_checksum =
            calculatePageChecksum(page.data(), static_cast<uint32_t>(page.size()));
        header->page_header.header_checksum =
            calculatePageHeaderChecksum(page.data(), header->page_header.header_bytes);

        ASSERT_TRUE(writeFullyAt(fd, page.data(), page.size(), 0));
        ::close(fd);
    }

    void reopenDatabaseWithVersions(uint32_t db_version, uint32_t db_compat_version)
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        db_.close();

        rewriteDatabaseHeaderVersions(db_version, db_compat_version);

        ErrorContext ctx;
        ASSERT_EQ(db_.open(db_path_, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
    }

    void rewriteBackupRollbackCheckpoint(const std::filesystem::path& backup_path,
                                         uint32_t marker_version,
                                         uint32_t source_db_version,
                                         uint32_t source_db_compat_version,
                                         uint32_t rollback_flags)
    {
        int fd = ::open(backup_path.c_str(), O_RDWR);
        ASSERT_GE(fd, 0);

        BackupManifestHeader header{};
        ASSERT_TRUE(readFullyAt(fd, &header, sizeof(header), 0));
        header.rollback_checkpoint_version = marker_version;
        header.source_db_version = source_db_version;
        header.source_db_compat_version = source_db_compat_version;
        header.rollback_flags = rollback_flags;
        header.checksum = 0;
        header.checksum = static_cast<uint32_t>(
            crc32(0L, reinterpret_cast<const Bytef*>(&header), sizeof(header)));

        ASSERT_TRUE(writeFullyAt(fd, &header, sizeof(header), 0));
        ::close(fd);
    }

    std::filesystem::path work_dir_;
    std::filesystem::path backup_dir_;
    std::filesystem::path restore_path_;
    std::filesystem::path rehearsal_restore_path_;
    std::string db_path_;
    Database db_;
    std::unique_ptr<ConnectionContext> conn_;
};

TEST_F(NonClusterUpgradeRollbackCertificationTest,
       CurrentBaselineRestoreValidationPassesSingleNodeCertification)
{
    BackupManager backup_mgr(&db_);
    const auto backup_path = backup_dir_ / "current_full.sbkp";

    ErrorContext backup_ctx;
    ASSERT_EQ(backup_mgr.createBackup(backup_path.string(),
                                      makeBackupConfig(BackupType::FULL, "cert-current-full"),
                                      nullptr,
                                      &backup_ctx),
              Status::OK)
        << backup_ctx.message;

    RestoreValidationResult result;
    ErrorContext validation_ctx;
    ASSERT_EQ(backup_mgr.runRestoreValidation(backup_path.string(),
                                              restore_path_.string(),
                                              makeValidationConfig(),
                                              &result,
                                              nullptr,
                                              &validation_ctx),
              Status::OK)
        << validation_ctx.message;

    EXPECT_EQ(result.backup_chain.size(), 1u);
    EXPECT_EQ(result.backup_chain.front(), normalizePath(backup_path));
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
}

TEST_F(NonClusterUpgradeRollbackCertificationTest,
       LegacyStandaloneBackupRemainsAdmissibleForSingleNodeCertification)
{
    reopenDatabaseWithVersions(DB_VERSION_ALPHA_1_0_1, DB_COMPAT_VERSION_ALPHA_1_0_1);

    BackupManager backup_mgr(&db_);
    const auto backup_path = backup_dir_ / "legacy_standalone.sbkp";

    ErrorContext backup_ctx;
    ASSERT_EQ(backup_mgr.createBackup(backup_path.string(),
                                      makeBackupConfig(BackupType::FULL, "cert-legacy-standalone"),
                                      nullptr,
                                      &backup_ctx),
              Status::OK)
        << backup_ctx.message;

    rewriteBackupRollbackCheckpoint(backup_path, 0, 0, 0, 0);

    RestoreValidationResult result;
    ErrorContext validation_ctx;
    ASSERT_EQ(backup_mgr.runRestoreValidation(backup_path.string(),
                                              restore_path_.string(),
                                              makeValidationConfig(),
                                              &result,
                                              nullptr,
                                              &validation_ctx),
              Status::OK)
        << validation_ctx.message;

    EXPECT_EQ(result.backup_chain.size(), 1u);
    EXPECT_TRUE(result.backup_verified);
    EXPECT_TRUE(result.restore_completed);
    EXPECT_TRUE(result.reopen_validated);
    EXPECT_FALSE(result.rollback_checkpoint_validated);
    EXPECT_TRUE(result.thresholds_passed);
    EXPECT_EQ(result.source_database_id, db_.uuid());
    EXPECT_EQ(result.restored_database_id, db_.uuid());
    EXPECT_EQ(result.chain_source_db_version, 0u);
    EXPECT_EQ(result.chain_source_db_compat_version, 0u);
}

TEST_F(NonClusterUpgradeRollbackCertificationTest,
       CurrentIncrementalChainPassesSingleNodeRollbackCertification)
{
    BackupManager backup_mgr(&db_);
    const auto full_path = backup_dir_ / "base_full.sbkp";
    const auto incremental_path = backup_dir_ / "delta.sbkp";

    ErrorContext full_ctx;
    ASSERT_EQ(backup_mgr.createBackup(full_path.string(),
                                      makeBackupConfig(BackupType::FULL, "cert-base-full"),
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

    RestoreValidationResult result;
    ErrorContext rehearsal_ctx;
    ASSERT_EQ(backup_mgr.runDisasterRecoveryRehearsal({full_path.string(), incremental_path.string()},
                                                      rehearsal_restore_path_.string(),
                                                      makeValidationConfig(),
                                                      &result,
                                                      nullptr,
                                                      &rehearsal_ctx),
              Status::OK)
        << rehearsal_ctx.message;

    EXPECT_EQ(result.backup_chain.size(), 2u);
    EXPECT_EQ(result.backup_chain.front(), normalizePath(full_path));
    EXPECT_EQ(result.backup_chain.back(), normalizePath(incremental_path));
    EXPECT_EQ(result.applied_backup_count, 2u);
    EXPECT_EQ(result.verified_backup_count, 2u);
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
}

TEST_F(NonClusterUpgradeRollbackCertificationTest,
       MissingCheckpointMarkersFailSingleNodeRollbackCertification)
{
    BackupManager backup_mgr(&db_);
    const auto full_path = backup_dir_ / "missing_marker_full.sbkp";
    const auto incremental_path = backup_dir_ / "missing_marker_delta.sbkp";

    ErrorContext full_ctx;
    ASSERT_EQ(backup_mgr.createBackup(full_path.string(),
                                      makeBackupConfig(BackupType::FULL, "cert-missing-marker-full"),
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

    rewriteBackupRollbackCheckpoint(full_path, 0, 0, 0, 0);

    RestoreValidationResult result;
    ErrorContext rehearsal_ctx;
    EXPECT_EQ(backup_mgr.runDisasterRecoveryRehearsal({full_path.string(), incremental_path.string()},
                                                      rehearsal_restore_path_.string(),
                                                      makeValidationConfig(),
                                                      &result,
                                                      nullptr,
                                                      &rehearsal_ctx),
              Status::NOT_SUPPORTED);
    EXPECT_FALSE(result.rollback_checkpoint_validated);
    EXPECT_NE(result.failure_reason.find("rollback checkpoint"), std::string::npos);
}

TEST_F(NonClusterUpgradeRollbackCertificationTest,
       CrossFormatAppliedChainFailsSingleNodeRollbackCertification)
{
    BackupManager backup_mgr(&db_);
    const auto full_path = backup_dir_ / "legacy_full.sbkp";
    const auto incremental_path = backup_dir_ / "current_delta.sbkp";

    reopenDatabaseWithVersions(DB_VERSION_ALPHA_1_0_1, DB_COMPAT_VERSION_ALPHA_1_0_1);
    ASSERT_EQ(backup_mgr.createBackup(full_path.string(),
                                      makeBackupConfig(BackupType::FULL, "cert-legacy-full"),
                                      nullptr,
                                      nullptr),
              Status::OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    reopenDatabaseWithVersions(DB_VERSION_CURRENT, DB_COMPAT_VERSION_CURRENT);
    backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 0));
    backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 1));

    ErrorContext incremental_ctx;
    ASSERT_EQ(backup_mgr.createIncrementalBackup(incremental_path.string(),
                                                 full_path.string(),
                                                 nullptr,
                                                 &incremental_ctx),
              Status::OK)
        << incremental_ctx.message;

    RestoreValidationResult result;
    ErrorContext rehearsal_ctx;
    EXPECT_EQ(backup_mgr.runDisasterRecoveryRehearsal({full_path.string(), incremental_path.string()},
                                                      rehearsal_restore_path_.string(),
                                                      makeValidationConfig(),
                                                      &result,
                                                      nullptr,
                                                      &rehearsal_ctx),
              Status::NOT_SUPPORTED);
    EXPECT_FALSE(result.rollback_checkpoint_validated);
    EXPECT_NE(rehearsal_ctx.message.find("format boundary"), std::string::npos);
}

} // namespace
