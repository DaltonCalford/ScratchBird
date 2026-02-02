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
#include "scratchbird/core/backup_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/page_manager.h"
#include <atomic>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <vector>
#include <unistd.h>

using namespace scratchbird::core;

class BackupTablespaceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static std::atomic<int> counter{0};
        test_id_ = std::to_string(getpid()) + "_" + std::to_string(counter++);
        work_dir_ = std::filesystem::temp_directory_path() / ("scratchbird_backup_ts_" + test_id_);
        std::filesystem::create_directories(work_dir_);
        db_path_ = (work_dir_ / "primary.db").string();
        ts_path_ = (work_dir_ / "ts1.sbts").string();
        backup_path_ = (work_dir_ / "backup.sbkp").string();
        restore_path_ = (work_dir_ / "restore.db").string();
    }

    void TearDown() override
    {
        std::filesystem::remove_all(work_dir_);
    }

    Status createBackupWithTablespace(uint16_t *tablespace_id_out)
    {
        ErrorContext ctx;
        Status status = Database::create(db_path_, 8192, &ctx);
        if (status != Status::OK)
        {
            return status;
        }

        Database db;
        status = db.open(db_path_, &ctx);
        if (status != Status::OK)
        {
            return status;
        }

        CatalogManager *catalog = db.catalog_manager();
        if (!catalog)
        {
            return Status::INVALID_ARGUMENT;
        }

        uint16_t tablespace_id = 0;
        status = catalog->createTablespace("ts1", ts_path_,
                                           true, 1, 8, 4,
                                           tablespace_id, &ctx);
        if (status != Status::OK)
        {
            return status;
        }

        PageManager *page_mgr = db.page_manager();
        if (!page_mgr)
        {
            return Status::INVALID_ARGUMENT;
        }

        GPID gpid = 0;
        status = page_mgr->allocatePageInTablespace(tablespace_id, &gpid, &ctx);
        if (status != Status::OK)
        {
            return status;
        }

        BackupManager backup_mgr(&db);
        BackupConfig config;
        config.type = BackupType::FULL;
        config.compression = CompressionType::NONE;
        status = backup_mgr.createBackup(backup_path_, config, nullptr, &ctx);
        if (status != Status::OK)
        {
            return status;
        }

        db.close();
        if (tablespace_id_out)
        {
            *tablespace_id_out = tablespace_id;
        }
        return Status::OK;
    }

    std::string test_id_;
    std::filesystem::path work_dir_;
    std::string db_path_;
    std::string ts_path_;
    std::string backup_path_;
    std::string restore_path_;
};

TEST_F(BackupTablespaceTest, ManifestIncludesTablespaceEntries)
{
    uint16_t tablespace_id = 0;
    ASSERT_EQ(createBackupWithTablespace(&tablespace_id), Status::OK);

    int fd = ::open(backup_path_.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    BackupManifestHeader header{};
    ssize_t read_bytes = ::read(fd, &header, sizeof(header));
    ASSERT_EQ(read_bytes, static_cast<ssize_t>(sizeof(header)));
    ASSERT_GT(header.tablespace_info_size, 0u);
    ASSERT_GT(header.tablespace_info_offset, 0u);

    off_t offset = static_cast<off_t>(header.tablespace_info_offset);
    ASSERT_EQ(::lseek(fd, offset, SEEK_SET), offset);

    uint32_t count = 0;
    read_bytes = ::read(fd, &count, sizeof(count));
    ASSERT_EQ(read_bytes, static_cast<ssize_t>(sizeof(count)));
    ASSERT_GE(count, 2u);

    bool found_custom = false;
    for (uint32_t i = 0; i < count; ++i)
    {
        BackupTablespaceEntryHeader entry{};
        read_bytes = ::read(fd, &entry, sizeof(entry));
        ASSERT_EQ(read_bytes, static_cast<ssize_t>(sizeof(entry)));

        std::vector<std::string> paths;
        for (uint16_t p = 0; p < entry.file_count; ++p)
        {
            uint32_t len = 0;
            read_bytes = ::read(fd, &len, sizeof(len));
            ASSERT_EQ(read_bytes, static_cast<ssize_t>(sizeof(len)));
            std::string path(len, '\0');
            if (len > 0)
            {
                read_bytes = ::read(fd, path.data(), len);
                ASSERT_EQ(read_bytes, static_cast<ssize_t>(len));
            }
            paths.push_back(path);
        }

        if (entry.tablespace_id == tablespace_id)
        {
            found_custom = true;
            EXPECT_GT(entry.total_pages, 0u);
            ASSERT_FALSE(paths.empty());
            EXPECT_EQ(paths.front(), ts_path_);
        }
    }

    ::close(fd);
    EXPECT_TRUE(found_custom);
}

TEST_F(BackupTablespaceTest, RestoreRequiresTablespaceUnlessAllowed)
{
    uint16_t tablespace_id = 0;
    ASSERT_EQ(createBackupWithTablespace(&tablespace_id), Status::OK);

    std::filesystem::remove(ts_path_);
    std::filesystem::remove(restore_path_);

    BackupManager restore_mgr(nullptr);
    RestoreConfig config;
    ErrorContext ctx;

    config.allow_tablespace_create = false;
    Status status = restore_mgr.restoreBackup(backup_path_, restore_path_, config, nullptr, &ctx);
    EXPECT_EQ(status, Status::FILE_NOT_FOUND);

    std::filesystem::remove(restore_path_);

    config.allow_tablespace_create = true;
    status = restore_mgr.restoreBackup(backup_path_, restore_path_, config, nullptr, &ctx);
    EXPECT_EQ(status, Status::OK);
    EXPECT_TRUE(std::filesystem::exists(ts_path_));
}
