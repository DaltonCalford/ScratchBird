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
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class CatalogIndexMetricsExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID schema_id_{};

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_index_metrics_extension_contract_" +
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

        ASSERT_EQ(catalog_->createSchema("cat017_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;
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

    ID createIndexAndReturnFilespace(ID& filespace_id_out)
    {
        CatalogManager::ColumnInfo id_col{};
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT64);
        id_col.nullable = false;

        std::vector<CatalogManager::ColumnInfo> columns{id_col};

        ErrorContext ctx;
        ID table_id{};
        Status status = catalog_->createTable(schema_id_, "cat017_table", columns, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        if (status != Status::OK)
        {
            return ID{};
        }

        ID index_id{};
        status = catalog_->createIndex(table_id,
                                       "cat017_idx",
                                       std::vector<std::string>{"id"},
                                       index_id,
                                       false,
                                       CatalogManager::IndexType::BTREE,
                                       0,
                                       &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        if (status != Status::OK)
        {
            return ID{};
        }

        CatalogManager::IndexInfo index_info{};
        status = catalog_->getIndex(index_id, index_info, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        if (status != Status::OK)
        {
            return ID{};
        }
        filespace_id_out = index_info.tablespace_uuid;
        if (filespace_id_out == ID{})
        {
            std::vector<TablespaceInfo> tablespaces;
            status = catalog_->listTablespaces(tablespaces, &ctx);
            EXPECT_EQ(status, Status::OK) << ctx.message;
            if (status != Status::OK)
            {
                return ID{};
            }
            for (const auto& ts : tablespaces)
            {
                if (ts.tablespace_uuid != ID{})
                {
                    filespace_id_out = ts.tablespace_uuid;
                    break;
                }
            }
        }
        // Primary/default filespace can be represented by zero UUID.
        // If no explicit non-zero tablespace exists yet, zero remains valid.
        return index_id;
    }
};

TEST_F(CatalogIndexMetricsExtensionContractTest, StatsUsageAndContentionContracts)
{
    ErrorContext ctx;
    ID filespace_id{};
    ID index_id = createIndexAndReturnFilespace(filespace_id);
    ASSERT_NE(index_id, ID{});

    CatalogManager::IndexStatsCatalogInfo invalid_stats{};
    invalid_stats.index_id = index_id;
    invalid_stats.null_frac = 1.25f;
    EXPECT_EQ(catalog_->upsertIndexStatsCatalogEntry(invalid_stats, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::IndexStatsCatalogInfo stats{};
    stats.index_id = index_id;
    stats.stats_version = 3;
    stats.row_count_est = 100;
    stats.distinct_count_est = 80;
    stats.null_frac = 0.05f;
    stats.bloat_ratio = 0.10f;
    stats.correlation = 0.90f;
    stats.avg_key_len = 16;
    stats.avg_entry_len = 24;
    stats.leaf_pages = 8;
    stats.height = 3;
    stats.metrics_last_refresh_xid = 77;
    stats.family_metrics_version = 4;
    stats.family_metrics_type = scratchbird::optimizer::IndexFamilyMetricsType::ORDERED_EXACT;
    stats.metrics_confidence_class =
        scratchbird::optimizer::IndexMetricsConfidenceClass::HIGH;
    stats.queryability_state =
        scratchbird::optimizer::IndexMetricsQueryabilityState::QUERYABLE;
    stats.family_metrics_payload =
        R"({"family_metrics_type":"ORDERED_EXACT","family_metrics":{"avg_probe_pages":3.0}})";
    ASSERT_EQ(catalog_->upsertIndexStatsCatalogEntry(stats, &ctx), Status::OK) << ctx.message;

    CatalogManager::IndexStatsCatalogInfo loaded_stats{};
    ASSERT_EQ(catalog_->getIndexStatsCatalogEntry(index_id, loaded_stats, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(loaded_stats.stats_version, 3u);
    EXPECT_EQ(loaded_stats.row_count_est, 100u);
    EXPECT_EQ(loaded_stats.metrics_last_refresh_xid, 77u);
    EXPECT_EQ(loaded_stats.family_metrics_version, 4u);
    EXPECT_EQ(loaded_stats.family_metrics_type,
              scratchbird::optimizer::IndexFamilyMetricsType::ORDERED_EXACT);
    EXPECT_EQ(loaded_stats.metrics_confidence_class,
              scratchbird::optimizer::IndexMetricsConfidenceClass::HIGH);
    EXPECT_EQ(loaded_stats.queryability_state,
              scratchbird::optimizer::IndexMetricsQueryabilityState::QUERYABLE);
    EXPECT_NE(loaded_stats.family_metrics_payload.find("avg_probe_pages"), std::string::npos);

    CatalogManager::IndexUsageCatalogInfo usage{};
    usage.index_id = index_id;
    usage.scan_count = 12;
    usage.tuple_read = 400;
    usage.tuple_returned = 390;
    usage.blocks_read = 25;
    usage.blocks_hit = 300;
    usage.total_time_ns = 90000;
    ASSERT_EQ(catalog_->upsertIndexUsageCatalogEntry(usage, &ctx), Status::OK) << ctx.message;

    CatalogManager::IndexUsageCatalogInfo loaded_usage{};
    ASSERT_EQ(catalog_->getIndexUsageCatalogEntry(index_id, loaded_usage, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(loaded_usage.scan_count, 12u);
    EXPECT_EQ(loaded_usage.blocks_hit, 300u);

    CatalogManager::IndexContentionCatalogInfo contention{};
    contention.index_id = index_id;
    contention.lock_wait_count = 2;
    contention.lock_wait_time_ns = 1500;
    contention.deadlock_count = 1;
    ASSERT_EQ(catalog_->upsertIndexContentionCatalogEntry(contention, &ctx), Status::OK) << ctx.message;

    CatalogManager::IndexContentionCatalogInfo loaded_contention{};
    ASSERT_EQ(catalog_->getIndexContentionCatalogEntry(index_id, loaded_contention, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded_contention.lock_wait_count, 2u);
    EXPECT_EQ(loaded_contention.deadlock_count, 1u);

    std::vector<CatalogManager::IndexStatsCatalogInfo> stats_rows;
    ASSERT_EQ(catalog_->listIndexStatsCatalogEntries(stats_rows, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(stats_rows.size(), 1u);

    std::vector<CatalogManager::IndexUsageCatalogInfo> usage_rows;
    ASSERT_EQ(catalog_->listIndexUsageCatalogEntries(usage_rows, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(usage_rows.size(), 1u);

    std::vector<CatalogManager::IndexContentionCatalogInfo> contention_rows;
    ASSERT_EQ(catalog_->listIndexContentionCatalogEntries(contention_rows, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(contention_rows.size(), 1u);
}

TEST_F(CatalogIndexMetricsExtensionContractTest, StorageAndHealthContracts)
{
    ErrorContext ctx;
    ID filespace_id{};
    ID index_id = createIndexAndReturnFilespace(filespace_id);
    ASSERT_NE(index_id, ID{});

    CatalogManager::IndexStorageCatalogInfo invalid_storage{};
    invalid_storage.index_id = index_id;
    invalid_storage.filespace_id = filespace_id;
    invalid_storage.bytes_used = 2048;
    invalid_storage.bytes_allocated = 1024;
    EXPECT_EQ(catalog_->upsertIndexStorageCatalogEntry(invalid_storage, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::IndexStorageCatalogInfo missing_filespace{};
    missing_filespace.index_id = index_id;
    missing_filespace.filespace_id = generateUuidV7();
    missing_filespace.bytes_used = 1024;
    missing_filespace.bytes_allocated = 4096;
    EXPECT_EQ(catalog_->upsertIndexStorageCatalogEntry(missing_filespace, &ctx), Status::NOT_FOUND);

    CatalogManager::IndexStorageCatalogInfo storage{};
    storage.index_id = index_id;
    storage.filespace_id = filespace_id;
    storage.page_count = 32;
    storage.bytes_used = 4096;
    storage.bytes_allocated = 8192;
    storage.fragmentation_ratio = 0.25f;
    ASSERT_EQ(catalog_->upsertIndexStorageCatalogEntry(storage, &ctx), Status::OK) << ctx.message;

    CatalogManager::IndexStorageCatalogInfo loaded_storage{};
    ASSERT_EQ(catalog_->getIndexStorageCatalogEntry(index_id, loaded_storage, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded_storage.page_count, 32u);
    EXPECT_EQ(loaded_storage.bytes_allocated, 8192u);

    CatalogManager::IndexHealthCatalogInfo invalid_health{};
    invalid_health.index_id = index_id;
    invalid_health.light_status = CatalogManager::IndexHealthStatus::CORRUPT;
    invalid_health.diagnostic_status = CatalogManager::IndexHealthStatus::ERROR;
    EXPECT_EQ(catalog_->upsertIndexHealthCatalogEntry(invalid_health, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::IndexHealthCatalogInfo health{};
    health.index_id = index_id;
    health.light_status = CatalogManager::IndexHealthStatus::WARNING;
    health.diagnostic_status = CatalogManager::IndexHealthStatus::CORRUPT;
    health.light_error_count = 1;
    health.diagnostic_error_count = 2;
    health.pages_scanned = 2048;
    health.bytes_scanned = 65536;
    health.cleanup_backlog_count = 9;
    health.cleanup_backlog_pages = 3;
    health.cleanup_backlog_bytes = 768;
    health.cleanup_sweep_generation = 14;
    health.cleanup_checkpoint_generation = 7;
    health.cleanup_last_published_time = 123456789u;
    health.cleanup_repair_required = true;
    ASSERT_EQ(catalog_->upsertIndexHealthCatalogEntry(health, &ctx), Status::OK) << ctx.message;

    CatalogManager::IndexHealthCatalogInfo loaded_health{};
    ASSERT_EQ(catalog_->getIndexHealthCatalogEntry(index_id, loaded_health, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded_health.light_status, CatalogManager::IndexHealthStatus::WARNING);
    EXPECT_EQ(loaded_health.diagnostic_status, CatalogManager::IndexHealthStatus::CORRUPT);
    EXPECT_EQ(loaded_health.pages_scanned, 2048u);
    EXPECT_EQ(loaded_health.cleanup_backlog_count, 9u);
    EXPECT_EQ(loaded_health.cleanup_backlog_pages, 3u);
    EXPECT_EQ(loaded_health.cleanup_backlog_bytes, 768u);
    EXPECT_EQ(loaded_health.cleanup_sweep_generation, 14u);
    EXPECT_EQ(loaded_health.cleanup_checkpoint_generation, 7u);
    EXPECT_EQ(loaded_health.cleanup_last_published_time, 123456789u);
    EXPECT_TRUE(loaded_health.cleanup_repair_required);

    std::vector<CatalogManager::IndexStorageCatalogInfo> storage_rows;
    ASSERT_EQ(catalog_->listIndexStorageCatalogEntries(storage_rows, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(storage_rows.size(), 1u);

    std::vector<CatalogManager::IndexHealthCatalogInfo> health_rows;
    ASSERT_EQ(catalog_->listIndexHealthCatalogEntries(health_rows, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(health_rows.size(), 1u);

    ASSERT_EQ(catalog_->deleteIndexHealthCatalogEntry(index_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->getIndexHealthCatalogEntry(index_id, loaded_health, &ctx), Status::NOT_FOUND);
}
