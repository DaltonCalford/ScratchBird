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

class CatalogOlapCubeExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID schema_id_{};
    ID system_user_id_{};
    ID base_table_id_{};
    ID storage_table_id_{};
    ID id_column_id_{};
    ID value_column_id_{};
    ID type_id_{};

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_olap_cube_extension_contract_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        ASSERT_EQ(catalog_->createSchema("cat030_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;
        system_user_id_ = catalog_->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id_, ID{});

        createTableWithColumns("cat030_base_table", base_table_id_);
        createTableWithColumns("cat030_cube_storage", storage_table_id_);

        CatalogManager::TypeCatalogInfo type_info{};
        type_info.schema_id = schema_id_;
        type_info.type_name = "cat030_numeric_type";
        type_info.type_kind = CatalogManager::TypeKind::SCALAR;
        ASSERT_EQ(catalog_->upsertTypeCatalogEntry(type_info, type_id_, &ctx), Status::OK) << ctx.message;
        ASSERT_NE(type_id_, ID{});
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

    void createTableWithColumns(const std::string& name, ID& table_id_out)
    {
        CatalogManager::ColumnInfo id_col{};
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT64);
        id_col.nullable = false;

        CatalogManager::ColumnInfo value_col{};
        value_col.column_name = "value_col";
        value_col.data_type = static_cast<uint16_t>(DataType::INT64);
        value_col.nullable = true;

        std::vector<CatalogManager::ColumnInfo> columns{id_col, value_col};
        ErrorContext ctx;
        ASSERT_EQ(catalog_->createTable(schema_id_, name, columns, table_id_out, 0, &ctx), Status::OK)
            << ctx.message;

        std::vector<CatalogManager::ColumnInfo> fetched_columns;
        ASSERT_EQ(catalog_->getColumns(table_id_out, fetched_columns, &ctx), Status::OK) << ctx.message;
        for (const auto& col : fetched_columns)
        {
            if (col.column_name == "id")
            {
                id_column_id_ = col.column_id;
            }
            else if (col.column_name == "value_col")
            {
                value_column_id_ = col.column_id;
            }
        }
    }
};

TEST_F(CatalogOlapCubeExtensionContractTest, OlapCubeCatalogContracts)
{
    ErrorContext ctx;

    CatalogManager::OlapWatermarkCatalogInfo watermark{};
    watermark.watermark_id = generateUuidV7();
    watermark.table_id = base_table_id_;
    watermark.last_ingested_txid = 10;
    watermark.last_ingested_time = 1000;
    ASSERT_EQ(catalog_->upsertOlapWatermarkCatalogEntry(watermark, &ctx), Status::OK) << ctx.message;

    CatalogManager::OlapWatermarkCatalogInfo dup_watermark = watermark;
    dup_watermark.watermark_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertOlapWatermarkCatalogEntry(dup_watermark, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::OlapPartitionCatalogInfo partition{};
    partition.partition_id = generateUuidV7();
    partition.table_id = base_table_id_;
    partition.range_kind = CatalogManager::CubeRangeKind::TIME;
    partition.has_range_min_bytes = true;
    partition.range_min_bytes = "2026-01-01";
    partition.has_range_max_bytes = true;
    partition.range_max_bytes = "2026-02-01";
    partition.row_count = 123;
    partition.size_bytes = 4096;
    partition.compression = CatalogManager::OlapCompression::LZ4;
    partition.tier = CatalogManager::OlapTier::HOT;
    ASSERT_EQ(catalog_->upsertOlapPartitionCatalogEntry(partition, &ctx), Status::OK) << ctx.message;

    CatalogManager::OlapSegmentCatalogInfo segment{};
    segment.segment_id = generateUuidV7();
    segment.partition_id = partition.partition_id;
    segment.segment_index = 0;
    segment.row_count = 100;
    segment.size_bytes = 1024;
    segment.has_min_key_bytes = true;
    segment.min_key_bytes = "1";
    segment.has_max_key_bytes = true;
    segment.max_key_bytes = "100";
    ASSERT_EQ(catalog_->upsertOlapSegmentCatalogEntry(segment, &ctx), Status::OK) << ctx.message;

    CatalogManager::OlapSegmentCatalogInfo dup_segment = segment;
    dup_segment.segment_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertOlapSegmentCatalogEntry(dup_segment, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::OlapIngestLogCatalogInfo ingest{};
    ingest.batch_id = generateUuidV7();
    ingest.table_id = base_table_id_;
    ingest.min_txid = 11;
    ingest.max_txid = 20;
    ingest.row_count = 100;
    ingest.size_bytes = 2048;
    ingest.ingest_state = CatalogManager::OlapIngestState::COMMITTED;
    ingest.created_time = 1001;
    ingest.has_completed_time = true;
    ingest.completed_time = 1002;
    ASSERT_EQ(catalog_->upsertOlapIngestLogCatalogEntry(ingest, &ctx), Status::OK) << ctx.message;

    CatalogManager::OlapIngestLogCatalogInfo dup_ingest = ingest;
    dup_ingest.batch_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertOlapIngestLogCatalogEntry(dup_ingest, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::CubeCatalogInfo cube{};
    cube.cube_id = generateUuidV7();
    cube.schema_id = schema_id_;
    cube.cube_name = "cat030_cube";
    cube.base_table_id = base_table_id_;
    cube.status = CatalogManager::CubeStatus::ACTIVE;
    cube.owner_id = system_user_id_;
    ASSERT_EQ(catalog_->upsertCubeCatalogEntry(cube, &ctx), Status::OK) << ctx.message;

    CatalogManager::CubeCatalogInfo dup_cube = cube;
    dup_cube.cube_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertCubeCatalogEntry(dup_cube, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::CubeDimensionCatalogInfo bad_dimension{};
    bad_dimension.dimension_id = generateUuidV7();
    bad_dimension.cube_id = cube.cube_id;
    bad_dimension.dimension_name = "bad_dim";
    bad_dimension.source_kind = CatalogManager::CubeSourceKind::COLUMN;
    bad_dimension.has_source_column_id = true;
    bad_dimension.source_column_id = id_column_id_;
    bad_dimension.has_source_expr_sblr_id = true;
    bad_dimension.source_expr_sblr_id = generateUuidV7();
    bad_dimension.data_type_id = type_id_;
    EXPECT_EQ(catalog_->upsertCubeDimensionCatalogEntry(bad_dimension, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::CubeDimensionCatalogInfo dimension{};
    dimension.dimension_id = generateUuidV7();
    dimension.cube_id = cube.cube_id;
    dimension.dimension_name = "dim_time";
    dimension.source_kind = CatalogManager::CubeSourceKind::COLUMN;
    dimension.has_source_column_id = true;
    dimension.source_column_id = id_column_id_;
    dimension.data_type_id = type_id_;
    dimension.is_time_dimension = true;
    ASSERT_EQ(catalog_->upsertCubeDimensionCatalogEntry(dimension, &ctx), Status::OK) << ctx.message;

    CatalogManager::CubeLevelCatalogInfo level{};
    level.level_id = generateUuidV7();
    level.dimension_id = dimension.dimension_id;
    level.level_name = "day";
    level.key_expr_sblr_id = generateUuidV7();
    level.has_label_expr_sblr_id = true;
    level.label_expr_sblr_id = generateUuidV7();
    level.level_order = 1;
    ASSERT_EQ(catalog_->upsertCubeLevelCatalogEntry(level, &ctx), Status::OK) << ctx.message;

    CatalogManager::CubeLevelCatalogInfo dup_level = level;
    dup_level.level_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertCubeLevelCatalogEntry(dup_level, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::CubeHierarchyCatalogInfo hierarchy{};
    hierarchy.hierarchy_id = generateUuidV7();
    hierarchy.dimension_id = dimension.dimension_id;
    hierarchy.hierarchy_name = "default_hierarchy";
    hierarchy.is_default = true;
    ASSERT_EQ(catalog_->upsertCubeHierarchyCatalogEntry(hierarchy, &ctx), Status::OK) << ctx.message;

    CatalogManager::CubeHierarchyCatalogInfo dup_hierarchy = hierarchy;
    dup_hierarchy.hierarchy_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertCubeHierarchyCatalogEntry(dup_hierarchy, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::CubeHierarchyLevelCatalogInfo hierarchy_level{};
    hierarchy_level.hierarchy_level_id = generateUuidV7();
    hierarchy_level.hierarchy_id = hierarchy.hierarchy_id;
    hierarchy_level.level_id = level.level_id;
    hierarchy_level.position = 1;
    ASSERT_EQ(catalog_->upsertCubeHierarchyLevelCatalogEntry(hierarchy_level, &ctx), Status::OK) << ctx.message;

    CatalogManager::CubeHierarchyLevelCatalogInfo dup_hierarchy_level = hierarchy_level;
    dup_hierarchy_level.hierarchy_level_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertCubeHierarchyLevelCatalogEntry(dup_hierarchy_level, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::CubeMeasureCatalogInfo measure{};
    measure.measure_id = generateUuidV7();
    measure.cube_id = cube.cube_id;
    measure.measure_name = "total_value";
    measure.agg_function = CatalogManager::CubeAggFunction::SUM;
    measure.source_expr_sblr_id = generateUuidV7();
    measure.data_type_id = type_id_;
    measure.null_handling = CatalogManager::CubeNullHandling::IGNORE_NULLS;
    ASSERT_EQ(catalog_->upsertCubeMeasureCatalogEntry(measure, &ctx), Status::OK) << ctx.message;

    CatalogManager::CubeMeasureCatalogInfo dup_measure = measure;
    dup_measure.measure_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertCubeMeasureCatalogEntry(dup_measure, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::CubeMaterializationCatalogInfo materialization{};
    materialization.materialization_id = generateUuidV7();
    materialization.cube_id = cube.cube_id;
    materialization.storage_table_id = storage_table_id_;
    materialization.dimension_set_hash = 101;
    materialization.measure_set_hash = 202;
    materialization.state = CatalogManager::CubeMaterializationState::ACTIVE;
    materialization.row_count = 100;
    materialization.size_bytes = 2048;
    materialization.has_last_refresh_time = true;
    materialization.last_refresh_time = 1100;
    ASSERT_EQ(catalog_->upsertCubeMaterializationCatalogEntry(materialization, &ctx), Status::OK) << ctx.message;

    CatalogManager::CubeRefreshPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.cube_id = cube.cube_id;
    policy.refresh_mode = CatalogManager::CubeRefreshMode::INTERVAL;
    policy.has_interval_ms = true;
    policy.interval_ms = 60000;
    policy.has_watermark_column_id = true;
    policy.watermark_column_id = value_column_id_;
    policy.has_max_staleness_ms = true;
    policy.max_staleness_ms = 120000;
    policy.is_enabled = true;
    ASSERT_EQ(catalog_->upsertCubeRefreshPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::CubeRefreshPolicyCatalogInfo dup_policy = policy;
    dup_policy.policy_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertCubeRefreshPolicyCatalogEntry(dup_policy, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::CubeJobCatalogInfo bad_job{};
    bad_job.job_id = generateUuidV7();
    bad_job.cube_id = cube.cube_id;
    bad_job.job_type = CatalogManager::CubeJobType::REFRESH;
    bad_job.state = CatalogManager::CubeJobState::COMPLETED;
    EXPECT_EQ(catalog_->upsertCubeJobCatalogEntry(bad_job, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::CubeJobCatalogInfo job{};
    job.job_id = generateUuidV7();
    job.cube_id = cube.cube_id;
    job.job_type = CatalogManager::CubeJobType::REFRESH;
    job.state = CatalogManager::CubeJobState::COMPLETED;
    job.created_time = 1200;
    job.has_started_time = true;
    job.started_time = 1201;
    job.has_completed_time = true;
    job.completed_time = 1202;
    job.has_error_code = true;
    job.error_code = "WARN_LATE";
    job.has_error_message = true;
    job.error_message = "late refresh";
    ASSERT_EQ(catalog_->upsertCubeJobCatalogEntry(job, &ctx), Status::OK) << ctx.message;

    CatalogManager::CubeJobStepCatalogInfo step{};
    step.step_id = generateUuidV7();
    step.job_id = job.job_id;
    step.step_index = 1;
    step.step_name = "scan_base";
    step.state = CatalogManager::CubeJobState::COMPLETED;
    step.has_started_time = true;
    step.started_time = 1201;
    step.has_completed_time = true;
    step.completed_time = 1202;
    ASSERT_EQ(catalog_->upsertCubeJobStepCatalogEntry(step, &ctx), Status::OK) << ctx.message;

    CatalogManager::CubeJobStepCatalogInfo dup_step = step;
    dup_step.step_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertCubeJobStepCatalogEntry(dup_step, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::CubeStatsCatalogInfo bad_stats{};
    bad_stats.cube_id = cube.cube_id;
    bad_stats.cache_hit_rate = 1.2f;
    EXPECT_EQ(catalog_->upsertCubeStatsCatalogEntry(bad_stats, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::CubeStatsCatalogInfo stats{};
    stats.cube_id = cube.cube_id;
    stats.row_count = 100;
    stats.size_bytes = 2048;
    stats.last_refresh_time = 1202;
    stats.avg_query_latency_ms = 42;
    stats.cache_hit_rate = 0.95f;
    ASSERT_EQ(catalog_->upsertCubeStatsCatalogEntry(stats, &ctx), Status::OK) << ctx.message;

    CatalogManager::CubeCatalogInfo cube_out{};
    ASSERT_EQ(catalog_->getCubeCatalogEntry(cube.cube_id, cube_out, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(cube_out.cube_name, "cat030_cube");

    CatalogManager::CubeStatsCatalogInfo stats_out{};
    ASSERT_EQ(catalog_->getCubeStatsCatalogEntry(cube.cube_id, stats_out, &ctx), Status::OK) << ctx.message;
    EXPECT_NEAR(stats_out.cache_hit_rate, 0.95f, 0.0001f);

    std::vector<CatalogManager::OlapPartitionCatalogInfo> partitions;
    ASSERT_EQ(catalog_->listOlapPartitionCatalogEntries(base_table_id_, partitions, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(partitions.size(), 1u);

    std::vector<CatalogManager::CubeMeasureCatalogInfo> measures;
    ASSERT_EQ(catalog_->listCubeMeasureCatalogEntries(cube.cube_id, measures, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(measures.size(), 1u);

    std::vector<CatalogManager::CubeJobStepCatalogInfo> steps;
    ASSERT_EQ(catalog_->listCubeJobStepCatalogEntries(job.job_id, steps, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(steps.size(), 1u);
}
