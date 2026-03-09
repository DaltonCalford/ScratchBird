#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

#include <atomic>
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

using namespace scratchbird::core;

namespace
{

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

class BackupRollbackCheckpointTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static std::atomic<int> counter{0};
        const std::string suffix =
            std::to_string(getpid()) + "_" + std::to_string(counter++);
        work_dir_ =
            std::filesystem::temp_directory_path() / ("scratchbird_rollback_checkpoint_" + suffix);
        backup_dir_ = work_dir_ / "backups";
        std::filesystem::create_directories(backup_dir_);
        db_path_ = (work_dir_ / "test.sbdb").string();
        restore_path_ = work_dir_ / "restore.sbdb";

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
        header->page_header.checksum = calculatePageChecksum(page.data(), page.size());

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
    std::string db_path_;
    Database db_;
    std::unique_ptr<ConnectionContext> conn_;
};

TEST_F(BackupRollbackCheckpointTest, CreateBackupStampsExplicitRollbackCheckpointMarkers)
{
    BackupManager backup_mgr(&db_);
    const auto backup_path = backup_dir_ / "full.sbkp";

    ErrorContext ctx;
    ASSERT_EQ(backup_mgr.createBackup(backup_path.string(),
                                      makeBackupConfig(BackupType::FULL, "full"),
                                      nullptr,
                                      &ctx),
              Status::OK)
        << ctx.message;

    BackupMetadata metadata;
    ASSERT_EQ(backup_mgr.getBackupMetadata(backup_path.string(), &metadata, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(metadata.rollback_checkpoint_version,
              BACKUP_ROLLBACK_CHECKPOINT_VERSION_CURRENT);
    EXPECT_EQ(metadata.source_database_id, db_.uuid());
    EXPECT_EQ(metadata.source_db_version, DB_VERSION_CURRENT);
    EXPECT_EQ(metadata.source_db_compat_version, DB_COMPAT_VERSION_CURRENT);
    EXPECT_EQ(metadata.rollback_flags, BACKUP_ROLLBACK_REQUIRED_FLAGS);
}

TEST_F(BackupRollbackCheckpointTest, VerifyBackupRejectsFutureCheckpointSchemaVersion)
{
    BackupManager backup_mgr(&db_);
    const auto backup_path = backup_dir_ / "future_marker.sbkp";

    ErrorContext ctx;
    ASSERT_EQ(backup_mgr.createBackup(backup_path.string(),
                                      makeBackupConfig(BackupType::FULL, "future-marker"),
                                      nullptr,
                                      &ctx),
              Status::OK)
        << ctx.message;

    rewriteBackupRollbackCheckpoint(backup_path,
                                    BACKUP_ROLLBACK_CHECKPOINT_VERSION_CURRENT + 1,
                                    DB_VERSION_CURRENT,
                                    DB_COMPAT_VERSION_CURRENT,
                                    BACKUP_ROLLBACK_REQUIRED_FLAGS);

    EXPECT_EQ(backup_mgr.verifyBackup(backup_path.string(), nullptr, &ctx), Status::NOT_SUPPORTED);
    EXPECT_NE(ctx.message.find("marker version too new"), std::string::npos);
}

TEST_F(BackupRollbackCheckpointTest, RehearsalRejectsChainWithoutExplicitCheckpointMarkers)
{
    BackupManager backup_mgr(&db_);
    const auto full_path = backup_dir_ / "base_full.sbkp";
    const auto incremental_path = backup_dir_ / "delta.sbkp";

    ErrorContext ctx;
    ASSERT_EQ(backup_mgr.createBackup(full_path.string(),
                                      makeBackupConfig(BackupType::FULL, "base-full"),
                                      nullptr,
                                      &ctx),
              Status::OK)
        << ctx.message;

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 0));
    backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 1));
    ASSERT_EQ(backup_mgr.createIncrementalBackup(incremental_path.string(),
                                                 full_path.string(),
                                                 nullptr,
                                                 &ctx),
              Status::OK)
        << ctx.message;

    rewriteBackupRollbackCheckpoint(full_path, 0, 0, 0, 0);

    RestoreValidationResult result;
    EXPECT_EQ(backup_mgr.runDisasterRecoveryRehearsal({full_path.string(), incremental_path.string()},
                                                      restore_path_.string(),
                                                      makeValidationConfig(),
                                                      &result,
                                                      nullptr,
                                                      &ctx),
              Status::NOT_SUPPORTED);
    EXPECT_FALSE(result.rollback_checkpoint_validated);
    EXPECT_NE(result.failure_reason.find("rollback checkpoint"), std::string::npos);
}

TEST_F(BackupRollbackCheckpointTest, RehearsalRejectsChainThatCrossesFormatBoundary)
{
    BackupManager backup_mgr(&db_);
    const auto full_path = backup_dir_ / "legacy_full.sbkp";
    const auto incremental_path = backup_dir_ / "current_delta.sbkp";

    reopenDatabaseWithVersions(DB_VERSION_ALPHA_1_0_1, DB_COMPAT_VERSION_ALPHA_1_0_1);
    ASSERT_EQ(backup_mgr.createBackup(full_path.string(),
                                      makeBackupConfig(BackupType::FULL, "legacy-full"),
                                      nullptr,
                                      nullptr),
              Status::OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    reopenDatabaseWithVersions(DB_VERSION_CURRENT, DB_COMPAT_VERSION_CURRENT);
    backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 0));
    backup_mgr.markPageModified(makeGPID(PRIMARY_TABLESPACE_ID, 1));

    ErrorContext ctx;
    ASSERT_EQ(backup_mgr.createIncrementalBackup(incremental_path.string(),
                                                 full_path.string(),
                                                 nullptr,
                                                 &ctx),
              Status::OK)
        << ctx.message;

    RestoreValidationResult result;
    EXPECT_EQ(backup_mgr.runDisasterRecoveryRehearsal({full_path.string(), incremental_path.string()},
                                                      restore_path_.string(),
                                                      makeValidationConfig(),
                                                      &result,
                                                      nullptr,
                                                      &ctx),
              Status::NOT_SUPPORTED);
    EXPECT_FALSE(result.rollback_checkpoint_validated);
    EXPECT_NE(ctx.message.find("format boundary"), std::string::npos);
}

TEST_F(BackupRollbackCheckpointTest, RestoreValidationAllowsStandaloneLegacyBackupWithoutMarkers)
{
    BackupManager backup_mgr(&db_);
    const auto backup_path = backup_dir_ / "legacy_full.sbkp";

    ErrorContext ctx;
    ASSERT_EQ(backup_mgr.createBackup(backup_path.string(),
                                      makeBackupConfig(BackupType::FULL, "legacy-full"),
                                      nullptr,
                                      &ctx),
              Status::OK)
        << ctx.message;

    rewriteBackupRollbackCheckpoint(backup_path, 0, 0, 0, 0);

    RestoreValidationResult result;
    ASSERT_EQ(backup_mgr.runRestoreValidation(backup_path.string(),
                                              restore_path_.string(),
                                              makeValidationConfig(),
                                              &result,
                                              nullptr,
                                              &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(result.rollback_checkpoint_validated);
    EXPECT_EQ(result.rollback_checkpoint_version, 0u);
    EXPECT_EQ(result.chain_source_db_version, 0u);
    EXPECT_EQ(result.chain_source_db_compat_version, 0u);
}

} // namespace
