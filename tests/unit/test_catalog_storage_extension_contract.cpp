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

#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class CatalogStorageExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_storage_extension_contract_" +
                   std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
            catalog_ = nullptr;
        }
        std::remove(db_path_.c_str());
    }
};

TEST_F(CatalogStorageExtensionContractTest, FilespaceStatsAndBackupHistoryContracts)
{
    ErrorContext ctx;

    CatalogManager::FilespaceStatsCatalogInfo invalid_stats{};
    invalid_stats.filespace_id = 0;
    invalid_stats.total_pages = 100;
    invalid_stats.free_pages = 120;
    EXPECT_EQ(catalog_->upsertFilespaceStatsCatalogEntry(invalid_stats, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::FilespaceStatsCatalogInfo missing_filespace{};
    missing_filespace.filespace_id = 777;
    missing_filespace.total_pages = 100;
    missing_filespace.free_pages = 10;
    EXPECT_EQ(catalog_->upsertFilespaceStatsCatalogEntry(missing_filespace, &ctx), Status::NOT_FOUND);

    CatalogManager::FilespaceStatsCatalogInfo stats{};
    stats.filespace_id = 0;
    stats.total_pages = 1024;
    stats.free_pages = 256;
    stats.dirty_pages = 64;
    stats.read_iops = 500;
    stats.write_iops = 350;
    stats.last_scan_txid = 99;
    ASSERT_EQ(catalog_->upsertFilespaceStatsCatalogEntry(stats, &ctx), Status::OK) << ctx.message;

    CatalogManager::FilespaceStatsCatalogInfo loaded_stats{};
    ASSERT_EQ(catalog_->getFilespaceStatsCatalogEntry(0, loaded_stats, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(loaded_stats.total_pages, 1024u);
    EXPECT_EQ(loaded_stats.free_pages, 256u);
    EXPECT_EQ(loaded_stats.dirty_pages, 64u);

    std::vector<CatalogManager::FilespaceStatsCatalogInfo> stats_rows;
    ASSERT_EQ(catalog_->listFilespaceStatsCatalogEntries(stats_rows, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(stats_rows.size(), 1u);

    CatalogManager::BackupHistoryCatalogInfo invalid_success{};
    invalid_success.backup_id = generateUuidV7();
    invalid_success.database_id = generateUuidV7();
    invalid_success.backup_kind = CatalogManager::BackupHistoryKind::FULL;
    invalid_success.backup_status = CatalogManager::BackupHistoryStatus::SUCCESS;
    invalid_success.started_time = 1000;
    EXPECT_EQ(catalog_->upsertBackupHistoryCatalogEntry(invalid_success, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::BackupHistoryCatalogInfo invalid_failed{};
    invalid_failed.backup_id = generateUuidV7();
    invalid_failed.database_id = generateUuidV7();
    invalid_failed.backup_kind = CatalogManager::BackupHistoryKind::INCREMENTAL;
    invalid_failed.backup_status = CatalogManager::BackupHistoryStatus::FAILED;
    invalid_failed.started_time = 1001;
    invalid_failed.completed_time = 1002;
    EXPECT_EQ(catalog_->upsertBackupHistoryCatalogEntry(invalid_failed, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::BackupHistoryCatalogInfo backup{};
    backup.backup_id = generateUuidV7();
    backup.database_id = generateUuidV7();
    backup.backup_kind = CatalogManager::BackupHistoryKind::FULL;
    backup.backup_status = CatalogManager::BackupHistoryStatus::SUCCESS;
    backup.storage_profile = "local_fs";
    backup.storage_uri = "file:///var/lib/scratchbird/backups/full-001.sbk";
    backup.has_size_bytes = true;
    backup.size_bytes = 1024 * 1024;
    backup.has_checksum = true;
    backup.checksum = 0xA1B2C3D4u;
    backup.started_time = 2000;
    backup.completed_time = 2200;
    backup.created_by_user_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertBackupHistoryCatalogEntry(backup, &ctx), Status::OK) << ctx.message;

    CatalogManager::BackupHistoryCatalogInfo loaded_backup{};
    ASSERT_EQ(catalog_->getBackupHistoryCatalogEntry(backup.backup_id, loaded_backup, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded_backup.backup_status, CatalogManager::BackupHistoryStatus::SUCCESS);
    EXPECT_EQ(loaded_backup.storage_profile, "local_fs");
    EXPECT_EQ(loaded_backup.storage_uri, "file:///var/lib/scratchbird/backups/full-001.sbk");
    EXPECT_TRUE(loaded_backup.has_size_bytes);
    EXPECT_EQ(loaded_backup.size_bytes, 1024u * 1024u);

    std::vector<CatalogManager::BackupHistoryCatalogInfo> backup_rows;
    ASSERT_EQ(catalog_->listBackupHistoryCatalogEntries(ID{}, backup_rows, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(backup_rows.size(), 1u);

    ASSERT_EQ(catalog_->deleteBackupHistoryCatalogEntry(backup.backup_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->getBackupHistoryCatalogEntry(backup.backup_id, loaded_backup, &ctx), Status::NOT_FOUND);

    ASSERT_EQ(catalog_->deleteFilespaceStatsCatalogEntry(0, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->getFilespaceStatsCatalogEntry(0, loaded_stats, &ctx), Status::NOT_FOUND);
}

TEST_F(CatalogStorageExtensionContractTest, LobAndLobPageContracts)
{
    ErrorContext ctx;

    CatalogManager::LobCatalogInfo invalid_lob{};
    invalid_lob.lob_id = generateUuidV7();
    invalid_lob.database_id = generateUuidV7();
    invalid_lob.owner_id = generateUuidV7();
    invalid_lob.is_encrypted = true;
    EXPECT_EQ(catalog_->upsertLobCatalogEntry(invalid_lob, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::LobCatalogInfo lob{};
    lob.lob_id = generateUuidV7();
    lob.database_id = generateUuidV7();
    lob.owner_id = generateUuidV7();
    lob.data_length = 65536;
    lob.page_count = 4;
    lob.has_checksum = true;
    lob.checksum = 0xDEADBEEFu;
    lob.is_encrypted = false;
    ASSERT_EQ(catalog_->upsertLobCatalogEntry(lob, &ctx), Status::OK) << ctx.message;

    CatalogManager::LobCatalogInfo loaded_lob{};
    ASSERT_EQ(catalog_->getLobCatalogEntry(lob.lob_id, loaded_lob, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(loaded_lob.page_count, 4u);
    EXPECT_TRUE(loaded_lob.has_checksum);
    EXPECT_EQ(loaded_lob.checksum, 0xDEADBEEFu);

    CatalogManager::LobPageCatalogInfo missing_lob_page{};
    missing_lob_page.lob_page_id = generateUuidV7();
    missing_lob_page.lob_id = generateUuidV7();
    missing_lob_page.page_index = 0;
    missing_lob_page.page_gpid = 100;
    missing_lob_page.chunk_bytes = 128;
    EXPECT_EQ(catalog_->upsertLobPageCatalogEntry(missing_lob_page, &ctx), Status::NOT_FOUND);

    CatalogManager::LobPageCatalogInfo invalid_chunk{};
    invalid_chunk.lob_page_id = generateUuidV7();
    invalid_chunk.lob_id = lob.lob_id;
    invalid_chunk.page_index = 0;
    invalid_chunk.page_gpid = 200;
    invalid_chunk.chunk_bytes = static_cast<uint32_t>(db_->page_size() + 1);
    EXPECT_EQ(catalog_->upsertLobPageCatalogEntry(invalid_chunk, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::LobPageCatalogInfo page0{};
    page0.lob_page_id = generateUuidV7();
    page0.lob_id = lob.lob_id;
    page0.page_index = 0;
    page0.page_gpid = 300;
    page0.chunk_bytes = 256;
    page0.has_checksum = true;
    page0.checksum = 12345;
    ASSERT_EQ(catalog_->upsertLobPageCatalogEntry(page0, &ctx), Status::OK) << ctx.message;

    CatalogManager::LobPageCatalogInfo duplicate_index{};
    duplicate_index.lob_page_id = generateUuidV7();
    duplicate_index.lob_id = lob.lob_id;
    duplicate_index.page_index = 0;
    duplicate_index.page_gpid = 301;
    duplicate_index.chunk_bytes = 128;
    EXPECT_EQ(catalog_->upsertLobPageCatalogEntry(duplicate_index, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::LobPageCatalogInfo loaded_page{};
    ASSERT_EQ(catalog_->getLobPageCatalogEntry(page0.lob_page_id, loaded_page, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded_page.page_index, 0u);
    EXPECT_EQ(loaded_page.page_gpid, 300u);
    EXPECT_EQ(loaded_page.chunk_bytes, 256u);

    std::vector<CatalogManager::LobPageCatalogInfo> lob_pages;
    ASSERT_EQ(catalog_->listLobPageCatalogEntries(lob.lob_id, lob_pages, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(lob_pages.size(), 1u);

    std::vector<CatalogManager::LobCatalogInfo> lob_rows;
    ASSERT_EQ(catalog_->listLobCatalogEntries(lob_rows, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(lob_rows.size(), 1u);

    ASSERT_EQ(catalog_->deleteLobPageCatalogEntry(page0.lob_page_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->getLobPageCatalogEntry(page0.lob_page_id, loaded_page, &ctx), Status::NOT_FOUND);

    ASSERT_EQ(catalog_->deleteLobCatalogEntry(lob.lob_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->getLobCatalogEntry(lob.lob_id, loaded_lob, &ctx), Status::NOT_FOUND);
}
