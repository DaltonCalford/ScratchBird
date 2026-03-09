#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>

#include "scratchbird/core/backup_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace
{

bool readFullyAt(int fd, void *buffer, size_t size, off_t offset)
{
    auto *dst = static_cast<uint8_t *>(buffer);
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

bool writeFullyAt(int fd, const void *buffer, size_t size, off_t offset)
{
    const auto *src = static_cast<const uint8_t *>(buffer);
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

class DatabaseFormatCompatibilityTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = scratchbird::testing::uniqueTestDbPath(
            "test_database_format_compatibility", ".sbdb");
        backup_path_ = scratchbird::testing::uniqueTestDbPath(
            "test_database_format_compatibility_backup", ".sbkp");
        restore_path_ = scratchbird::testing::uniqueTestDbPath(
            "test_database_format_compatibility_restore", ".sbdb");
        std::filesystem::remove(db_path_);
        std::filesystem::remove(backup_path_);
        std::filesystem::remove(restore_path_);
    }

    void TearDown() override
    {
        std::filesystem::remove(db_path_);
        std::filesystem::remove(backup_path_);
        std::filesystem::remove(restore_path_);
    }

    void readPageZero(const std::string &path, std::vector<uint8_t> *page_out)
    {
        ASSERT_NE(page_out, nullptr);
        int fd = ::open(path.c_str(), O_RDONLY);
        ASSERT_GE(fd, 0);

        PageHeader page_header{};
        ASSERT_TRUE(readFullyAt(fd, &page_header, sizeof(page_header), 0));
        ASSERT_TRUE(isValidAlphaPageSize(page_header.page_size));

        page_out->assign(page_header.page_size, 0);
        ASSERT_TRUE(readFullyAt(fd, page_out->data(), page_out->size(), 0));
        ::close(fd);
    }

    void rewriteDatabaseHeaderVersions(const std::string &path,
                                       uint32_t db_version,
                                       uint32_t db_compat_version)
    {
        std::vector<uint8_t> page;
        readPageZero(path, &page);
        if (HasFatalFailure())
        {
            return;
        }

        auto *header = reinterpret_cast<DatabaseHeader *>(page.data());
        header->db_version = db_version;
        header->db_compat_version = db_compat_version;
        header->page_header.checksum = calculatePageChecksum(page.data(), page.size());

        int fd = ::open(path.c_str(), O_RDWR);
        ASSERT_GE(fd, 0);
        ASSERT_TRUE(writeFullyAt(fd, page.data(), page.size(), 0));
        ::close(fd);
    }

    DatabaseHeader readDatabaseHeader(const std::string &path)
    {
        std::vector<uint8_t> page;
        readPageZero(path, &page);
        if (HasFatalFailure())
        {
            return DatabaseHeader{};
        }
        return *reinterpret_cast<const DatabaseHeader *>(page.data());
    }

    void rewriteBackupPrimaryHeaderVersions(const std::string &backup_path,
                                            uint32_t db_version,
                                            uint32_t db_compat_version)
    {
        int fd = ::open(backup_path.c_str(), O_RDWR);
        ASSERT_GE(fd, 0);

        BackupManifestHeader manifest{};
        ASSERT_TRUE(readFullyAt(fd, &manifest, sizeof(manifest), 0));

        uint64_t index_offset = manifest.tablespace_info_offset + manifest.tablespace_info_size;
        if (index_offset == 0)
        {
            index_offset = sizeof(BackupManifestHeader);
        }

        std::vector<BackupPageEntry> entries(manifest.total_pages);
        ASSERT_TRUE(readFullyAt(fd,
                                entries.data(),
                                entries.size() * sizeof(BackupPageEntry),
                                static_cast<off_t>(index_offset)));

        size_t page0_index = entries.size();
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (getTablespaceID(entries[i].gpid) == PRIMARY_TABLESPACE_ID &&
                getPageNumber(entries[i].gpid) == 0)
            {
                page0_index = i;
                break;
            }
        }
        ASSERT_LT(page0_index, entries.size());

        BackupPageEntry &entry = entries[page0_index];
        ASSERT_EQ(entry.compressed_size, 0u);
        std::vector<uint8_t> page(entry.original_size, 0);
        ASSERT_TRUE(readFullyAt(fd,
                                page.data(),
                                page.size(),
                                static_cast<off_t>(entry.file_offset)));

        auto *header = reinterpret_cast<DatabaseHeader *>(page.data());
        header->db_version = db_version;
        header->db_compat_version = db_compat_version;
        header->page_header.checksum = calculatePageChecksum(page.data(), page.size());
        entry.checksum = static_cast<uint32_t>(
            crc32(0L, page.data(), static_cast<uInt>(page.size())));

        ASSERT_TRUE(writeFullyAt(fd,
                                 page.data(),
                                 page.size(),
                                 static_cast<off_t>(entry.file_offset)));
        ASSERT_TRUE(writeFullyAt(fd,
                                 &entry,
                                 sizeof(entry),
                                 static_cast<off_t>(index_offset +
                                                    (page0_index * sizeof(BackupPageEntry)))));
        ::close(fd);
    }

    std::string db_path_;
    std::string backup_path_;
    std::string restore_path_;
};

TEST_F(DatabaseFormatCompatibilityTest, CreateStampsCurrentDatabaseFormatVersions)
{
    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;

    const DatabaseHeader header = readDatabaseHeader(db_path_);
    EXPECT_EQ(header.db_version, DB_VERSION_CURRENT);
    EXPECT_EQ(header.db_compat_version, DB_COMPAT_VERSION_CURRENT);
}

TEST_F(DatabaseFormatCompatibilityTest, OpenAcceptsLegacyDatabaseFormatInCompatibilityMatrix)
{
    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;
    rewriteDatabaseHeaderVersions(db_path_,
                                  DB_VERSION_ALPHA_1_0_1,
                                  DB_COMPAT_VERSION_ALPHA_1_0_1);

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK) << ctx.message;
    db.close();
}

TEST_F(DatabaseFormatCompatibilityTest, OpenRejectsUnknownHistoricalDatabaseFormat)
{
    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;
    rewriteDatabaseHeaderVersions(db_path_, 0x00010701u, 0x00010701u);

    Database db;
    EXPECT_EQ(db.open(db_path_, &ctx), Status::NOT_SUPPORTED);
    EXPECT_NE(ctx.message.find("compatibility matrix"), std::string::npos);
}

TEST_F(DatabaseFormatCompatibilityTest, OpenRejectsFutureCompatibilityFloor)
{
    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;
    rewriteDatabaseHeaderVersions(db_path_, DB_VERSION_CURRENT, 0x00020000u);

    Database db;
    EXPECT_EQ(db.open(db_path_, &ctx), Status::NOT_SUPPORTED);
    EXPECT_NE(ctx.message.find("requires engine version at least"), std::string::npos);
}

TEST_F(DatabaseFormatCompatibilityTest, RestoreFailsClosedWhenBackupRequiresNewerEngine)
{
    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path_, 8192, &ctx), Status::OK) << ctx.message;

    Database db;
    ASSERT_EQ(db.open(db_path_, &ctx), Status::OK) << ctx.message;

    BackupManager backup_manager(&db);
    BackupConfig backup_config;
    backup_config.type = BackupType::FULL;
    backup_config.compression = CompressionType::NONE;
    backup_config.label = "format-compat";

    ASSERT_EQ(backup_manager.createBackup(backup_path_, backup_config, nullptr, &ctx),
              Status::OK)
        << ctx.message;

    rewriteBackupPrimaryHeaderVersions(backup_path_, DB_VERSION_CURRENT, 0x00020000u);

    RestoreConfig restore_config;
    restore_config.verify_checksums = true;

    EXPECT_EQ(backup_manager.restoreBackup(backup_path_,
                                           restore_path_,
                                           restore_config,
                                           nullptr,
                                           &ctx),
              Status::NOT_SUPPORTED);
    EXPECT_NE(ctx.message.find("requires engine version at least"), std::string::npos);

    db.close();
}

} // namespace
