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
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/types.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class CatalogFamilyMigrationBackfillTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kPageSize = 16384;
    std::unique_ptr<scratchbird::testing::TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>(
            "catalog_family_migration_backfill");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), kPageSize, &ctx), Status::OK) << ctx.message;
        openExisting();
    }

    void TearDown() override
    {
        closeCurrent();
        db_file_.reset();
    }

    void openExisting()
    {
        ErrorContext ctx;
        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
    }

    void closeCurrent()
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
        }
    }

    auto catalog() const -> CatalogManager*
    {
        return db_->catalog_manager();
    }

    void createSmokeTable()
    {
        ErrorContext ctx;
        CatalogManager::SchemaInfo schema{};
        ASSERT_EQ(catalog()->getSchema("public", schema, &ctx), Status::OK) << ctx.message;

        CatalogManager::ColumnInfo id_col{};
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT64);
        id_col.nullable = false;

        std::vector<CatalogManager::ColumnInfo> columns{id_col};
        ID table_id{};
        ASSERT_EQ(catalog()->createTable(schema.schema_id, "en016_smoke_table", columns, table_id, 0, &ctx),
                  Status::OK)
            << ctx.message;
    }

    void forceRootPageFieldsToZeroOnDisk(const std::vector<uint32_t>& page_ids_to_zero)
    {
        const int fd = ::open(db_file_->path().c_str(), O_RDWR);
        ASSERT_GE(fd, 0);

        std::vector<uint8_t> page(kPageSize, 0);
        const off_t offset =
            static_cast<off_t>(BOOTSTRAP_PAGE_CATALOG_ROOT) * static_cast<off_t>(kPageSize);
        ASSERT_EQ(::pread(fd, page.data(), page.size(), offset),
                  static_cast<ssize_t>(page.size()));

        uint8_t* bytes = page.data();
        const size_t scan_limit = page.size();

        for (uint32_t page_id : page_ids_to_zero)
        {
            ASSERT_NE(page_id, 0u);
            size_t match_offset = 0;
            uint32_t matches = 0;

            for (size_t off = 0; off + sizeof(uint32_t) <= scan_limit; off += sizeof(uint32_t))
            {
                uint32_t value = 0;
                std::memcpy(&value, bytes + off, sizeof(value));
                if (value == page_id)
                {
                    ++matches;
                    match_offset = off;
                }
            }

            ASSERT_EQ(matches, 1u)
                << "expected exactly one root-page field match for page id " << page_id;

            const uint32_t zero = 0;
            std::memcpy(bytes + match_offset, &zero, sizeof(zero));
        }

        ASSERT_EQ(::pwrite(fd, page.data(), page.size(), offset),
                  static_cast<ssize_t>(page.size()));
        ASSERT_EQ(::fsync(fd), 0);
        ASSERT_EQ(::close(fd), 0);
    }
};

TEST_F(CatalogFamilyMigrationBackfillTest, ReopenBackfillsMissingCatalogFamilyPagePointersAndIsIdempotent)
{
    createSmokeTable();

    const uint32_t old_remote_connector = catalog()->remoteConnectorTablePage();
    const uint32_t old_workload_class = catalog()->workloadClassTablePage();
    const uint32_t old_cluster_fabric_link = catalog()->clusterFabricLinkTablePage();
    const uint32_t old_ts_parser = catalog()->tsParserTablePage();
    const uint32_t old_sblr_module = catalog()->sblrModuleTablePage();
    const uint32_t old_replication_channel = catalog()->replicationChannelTablePage();

    ASSERT_NE(old_remote_connector, 0u);
    ASSERT_NE(old_workload_class, 0u);
    ASSERT_NE(old_cluster_fabric_link, 0u);
    ASSERT_NE(old_ts_parser, 0u);
    ASSERT_NE(old_sblr_module, 0u);
    ASSERT_NE(old_replication_channel, 0u);

    closeCurrent();
    forceRootPageFieldsToZeroOnDisk({
        old_remote_connector,
        old_workload_class,
        old_cluster_fabric_link,
        old_ts_parser,
        old_sblr_module,
        old_replication_channel,
    });
    openExisting();

    const uint32_t new_remote_connector = catalog()->remoteConnectorTablePage();
    const uint32_t new_workload_class = catalog()->workloadClassTablePage();
    const uint32_t new_cluster_fabric_link = catalog()->clusterFabricLinkTablePage();
    const uint32_t new_ts_parser = catalog()->tsParserTablePage();
    const uint32_t new_sblr_module = catalog()->sblrModuleTablePage();
    const uint32_t new_replication_channel = catalog()->replicationChannelTablePage();

    EXPECT_NE(new_remote_connector, 0u);
    EXPECT_NE(new_workload_class, 0u);
    EXPECT_NE(new_cluster_fabric_link, 0u);
    EXPECT_NE(new_ts_parser, 0u);
    EXPECT_NE(new_sblr_module, 0u);
    EXPECT_NE(new_replication_channel, 0u);

    EXPECT_NE(new_remote_connector, old_remote_connector);
    EXPECT_NE(new_workload_class, old_workload_class);
    EXPECT_NE(new_cluster_fabric_link, old_cluster_fabric_link);
    EXPECT_NE(new_ts_parser, old_ts_parser);
    EXPECT_NE(new_sblr_module, old_sblr_module);
    EXPECT_NE(new_replication_channel, old_replication_channel);

    ErrorContext ctx;
    CatalogManager::SchemaInfo schema{};
    ASSERT_EQ(catalog()->getSchema("public", schema, &ctx), Status::OK) << ctx.message;
    CatalogManager::TableInfo table{};
    ASSERT_EQ(catalog()->getTable(schema.schema_id, "en016_smoke_table", table, &ctx), Status::OK)
        << ctx.message;

    closeCurrent();
    openExisting();

    EXPECT_EQ(catalog()->remoteConnectorTablePage(), new_remote_connector);
    EXPECT_EQ(catalog()->workloadClassTablePage(), new_workload_class);
    EXPECT_EQ(catalog()->clusterFabricLinkTablePage(), new_cluster_fabric_link);
    EXPECT_EQ(catalog()->tsParserTablePage(), new_ts_parser);
    EXPECT_EQ(catalog()->sblrModuleTablePage(), new_sblr_module);
    EXPECT_EQ(catalog()->replicationChannelTablePage(), new_replication_channel);
}
