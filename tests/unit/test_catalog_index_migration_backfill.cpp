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

#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/types.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class CatalogIndexMigrationBackfillTest : public ::testing::Test
{
protected:
    std::unique_ptr<scratchbird::testing::TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_file_ = std::make_unique<scratchbird::testing::TestDatabaseFile>(
            "catalog_index_migration_backfill");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
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

    auto findPublicSchema(ErrorContext& ctx) const -> CatalogManager::SchemaInfo
    {
        CatalogManager::SchemaInfo schema{};
        EXPECT_EQ(catalog()->getSchema("public", schema, &ctx), Status::OK) << ctx.message;
        return schema;
    }

    void createTableAndIndex(const std::string& table_name,
                             const std::string& index_name,
                             ID& table_id_out,
                             ID& index_id_out) const
    {
        ErrorContext ctx;
        auto schema = findPublicSchema(ctx);

        CatalogManager::ColumnInfo id_col{};
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT64);
        id_col.nullable = false;

        CatalogManager::ColumnInfo payload_col{};
        payload_col.column_name = "payload";
        payload_col.data_type = static_cast<uint16_t>(DataType::VARCHAR);
        payload_col.type_precision = 64;
        payload_col.max_length = 64;
        payload_col.nullable = true;

        std::vector<CatalogManager::ColumnInfo> columns{id_col, payload_col};
        ASSERT_EQ(catalog()->createTable(schema.schema_id, table_name, columns, table_id_out, 0, &ctx),
                  Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog()->createIndex(
                      table_id_out,
                      index_name,
                      std::vector<std::string>{"id"},
                      std::vector<std::string>{"payload"},
                      index_id_out,
                      false,
                      CatalogManager::IndexType::BTREE,
                      0,
                      &ctx),
                  Status::OK)
            << ctx.message;
    }

    auto resolveIndexId(const std::string& table_name, const std::string& index_name) const -> ID
    {
        ErrorContext ctx;
        auto schema = findPublicSchema(ctx);
        CatalogManager::TableInfo table{};
        EXPECT_EQ(catalog()->getTable(schema.schema_id, table_name, table, &ctx), Status::OK)
            << ctx.message;
        CatalogManager::IndexInfo index{};
        EXPECT_EQ(catalog()->getIndex(table.table_id, index_name, index, &ctx), Status::OK)
            << ctx.message;
        return index.index_id;
    }
};

TEST_F(CatalogIndexMigrationBackfillTest, ReopenBackfillsMissingCanonicalRowsAndRemainsIdempotent)
{
    ID table_id{};
    ID index_id{};
    createTableAndIndex("wf021_table", "wf021_idx", table_id, index_id);

    ErrorContext ctx;
    std::vector<CatalogManager::IndexColumnCatalogInfo> columns;
    ASSERT_EQ(catalog()->listIndexColumnCatalogEntries(index_id, columns, &ctx), Status::OK) << ctx.message;
    for (const auto& column : columns)
    {
        ASSERT_EQ(catalog()->deleteIndexColumnCatalogEntry(column.index_column_id, &ctx), Status::OK)
            << ctx.message;
    }

    Status status = catalog()->deleteIndexStatsCatalogEntry(index_id, &ctx);
    ASSERT_TRUE(status == Status::OK || status == Status::NOT_FOUND) << ctx.message;
    status = catalog()->deleteIndexUsageCatalogEntry(index_id, &ctx);
    ASSERT_TRUE(status == Status::OK || status == Status::NOT_FOUND) << ctx.message;
    status = catalog()->deleteIndexContentionCatalogEntry(index_id, &ctx);
    ASSERT_TRUE(status == Status::OK || status == Status::NOT_FOUND) << ctx.message;
    status = catalog()->deleteIndexStorageCatalogEntry(index_id, &ctx);
    ASSERT_TRUE(status == Status::OK || status == Status::NOT_FOUND) << ctx.message;
    status = catalog()->deleteIndexHealthCatalogEntry(index_id, &ctx);
    ASSERT_TRUE(status == Status::OK || status == Status::NOT_FOUND) << ctx.message;

    closeCurrent();
    openExisting();

    index_id = resolveIndexId("wf021_table", "wf021_idx");

    CatalogManager::SchemaInfo schema{};
    ASSERT_EQ(catalog()->getSchema("public", schema, &ctx), Status::OK) << ctx.message;
    CatalogManager::TableInfo table{};
    ASSERT_EQ(catalog()->getTable(schema.schema_id, "wf021_table", table, &ctx), Status::OK)
        << ctx.message;

    std::vector<CatalogManager::ColumnInfo> table_columns;
    ASSERT_EQ(catalog()->getColumns(table.table_id, table_columns, &ctx), Status::OK) << ctx.message;
    ID id_column{};
    ID payload_column{};
    for (const auto& col : table_columns)
    {
        if (col.column_name == "id")
        {
            id_column = col.column_id;
        }
        else if (col.column_name == "payload")
        {
            payload_column = col.column_id;
        }
    }
    ASSERT_NE(id_column, ID{});
    ASSERT_NE(payload_column, ID{});

    columns.clear();
    ASSERT_EQ(catalog()->listIndexColumnCatalogEntries(index_id, columns, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(columns.size(), 2u);
    EXPECT_EQ(columns[0].position, 1u);
    EXPECT_EQ(columns[0].column_id, id_column);
    EXPECT_FALSE(columns[0].is_include);
    EXPECT_EQ(columns[1].position, 2u);
    EXPECT_EQ(columns[1].column_id, payload_column);
    EXPECT_TRUE(columns[1].is_include);

    CatalogManager::IndexStatsCatalogInfo stats{};
    ASSERT_EQ(catalog()->getIndexStatsCatalogEntry(index_id, stats, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(stats.stats_version, 1u);

    CatalogManager::IndexUsageCatalogInfo usage{};
    ASSERT_EQ(catalog()->getIndexUsageCatalogEntry(index_id, usage, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(usage.scan_count, 0u);

    CatalogManager::IndexContentionCatalogInfo contention{};
    ASSERT_EQ(catalog()->getIndexContentionCatalogEntry(index_id, contention, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(contention.lock_wait_count, 0u);

    CatalogManager::IndexStorageCatalogInfo storage{};
    ASSERT_EQ(catalog()->getIndexStorageCatalogEntry(index_id, storage, &ctx), Status::OK) << ctx.message;
    EXPECT_GE(storage.page_count, 1u);
    EXPECT_GE(storage.bytes_allocated, static_cast<uint64_t>(db_->page_size()));
    EXPECT_LE(storage.bytes_used, storage.bytes_allocated);

    CatalogManager::IndexHealthCatalogInfo health{};
    ASSERT_EQ(catalog()->getIndexHealthCatalogEntry(index_id, health, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(health.light_status, CatalogManager::IndexHealthStatus::HEALTHY);
    EXPECT_EQ(health.diagnostic_status, CatalogManager::IndexHealthStatus::HEALTHY);

    closeCurrent();
    openExisting();

    index_id = resolveIndexId("wf021_table", "wf021_idx");
    columns.clear();
    ASSERT_EQ(catalog()->listIndexColumnCatalogEntries(index_id, columns, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(columns.size(), 2u);
    EXPECT_EQ(columns[0].position, 1u);
    EXPECT_EQ(columns[1].position, 2u);
}

TEST_F(CatalogIndexMigrationBackfillTest, ReopenBackfillPreservesExistingTelemetryRows)
{
    ID table_id{};
    ID index_id{};
    createTableAndIndex("wf021_preserve_table", "wf021_preserve_idx", table_id, index_id);

    ErrorContext ctx;
    CatalogManager::IndexStatsCatalogInfo stats{};
    stats.index_id = index_id;
    stats.stats_version = 7;
    stats.row_count_est = 123;
    ASSERT_EQ(catalog()->upsertIndexStatsCatalogEntry(stats, &ctx), Status::OK) << ctx.message;

    CatalogManager::IndexUsageCatalogInfo usage{};
    usage.index_id = index_id;
    usage.scan_count = 42;
    usage.blocks_hit = 900;
    ASSERT_EQ(catalog()->upsertIndexUsageCatalogEntry(usage, &ctx), Status::OK) << ctx.message;

    Status status = catalog()->deleteIndexHealthCatalogEntry(index_id, &ctx);
    ASSERT_TRUE(status == Status::OK || status == Status::NOT_FOUND) << ctx.message;

    closeCurrent();
    openExisting();

    index_id = resolveIndexId("wf021_preserve_table", "wf021_preserve_idx");

    CatalogManager::IndexStatsCatalogInfo loaded_stats{};
    ASSERT_EQ(catalog()->getIndexStatsCatalogEntry(index_id, loaded_stats, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(loaded_stats.stats_version, 7u);
    EXPECT_EQ(loaded_stats.row_count_est, 123u);

    CatalogManager::IndexUsageCatalogInfo loaded_usage{};
    ASSERT_EQ(catalog()->getIndexUsageCatalogEntry(index_id, loaded_usage, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded_usage.scan_count, 42u);
    EXPECT_EQ(loaded_usage.blocks_hit, 900u);

    CatalogManager::IndexHealthCatalogInfo loaded_health{};
    ASSERT_EQ(catalog()->getIndexHealthCatalogEntry(index_id, loaded_health, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded_health.light_status, CatalogManager::IndexHealthStatus::HEALTHY);
    EXPECT_EQ(loaded_health.diagnostic_status, CatalogManager::IndexHealthStatus::HEALTHY);
}
