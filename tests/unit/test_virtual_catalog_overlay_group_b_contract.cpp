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

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::catalog;
using namespace scratchbird::core;

namespace {

class VirtualCatalogOverlayGroupBContractTest : public ::testing::Test {
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    CatalogManager::SessionInfo session_{};
    ID schema_id_{};

    void SetUp() override {
        db_path_ = "/tmp/test_virtual_catalog_overlay_group_b_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        ID system_user_id = catalog_->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id, ID{}) << "System user id unexpectedly unset";
        ASSERT_EQ(catalog_->createSession(system_user_id, ID{}, "mysql", session_, &ctx), Status::OK)
            << ctx.message;
        conn_->setSessionContext(session_.session_id, session_.authkey_id, session_.emulation_mode,
                                 session_.policy_epoch_global, session_.policy_epoch_table);
        conn_->beginStatementTracking("SELECT 1");

        ASSERT_EQ(catalog_->createSchema("ef032_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::ColumnInfo id_col{};
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT64);
        id_col.nullable = false;
        id_col.is_primary_key = true;

        CatalogManager::ColumnInfo value_col{};
        value_col.column_name = "reading";
        value_col.data_type = static_cast<uint16_t>(DataType::FLOAT64);
        value_col.nullable = true;

        ID table_id{};
        ASSERT_EQ(catalog_->createTable(schema_id_, "sensor_points", {id_col, value_col}, table_id, 0, &ctx),
                  Status::OK)
            << ctx.message;
    }

    void TearDown() override {
        if (conn_) {
            conn_->endStatementTrackingSuccess(0);
        }
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();

        if (db_) {
            db_->close();
            db_.reset();
            catalog_ = nullptr;
        }
        std::remove(db_path_.c_str());
    }
};

TEST_F(VirtualCatalogOverlayGroupBContractTest, GroupBHandlersRegisteredAndQueryable) {
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();
    ASSERT_NE(router.getHandler(ProtocolType::MARIADB), nullptr);
    ASSERT_NE(router.getHandler(ProtocolType::CLICKHOUSE), nullptr);
    ASSERT_NE(router.getHandler(ProtocolType::DUCKDB), nullptr);
    ASSERT_NE(router.getHandler(ProtocolType::INFLUXDB), nullptr);

    ErrorContext ctx;

    VirtualResultSet mariadb_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MARIADB, "mysql", "user", "", mariadb_result, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(mariadb_result.empty());

    VirtualResultSet clickhouse_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::CLICKHOUSE, "system", "databases", "", clickhouse_result,
                                  &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(clickhouse_result.empty());

    VirtualResultSet duckdb_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::DUCKDB, "duckdb_catalog", "duckdb_tables", "", duckdb_result,
                                  &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(duckdb_result.empty());

    VirtualResultSet influx_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::INFLUXDB, "influxdb_meta", "measurements", "", influx_result,
                                  &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(influx_result.empty());
}

TEST_F(VirtualCatalogOverlayGroupBContractTest, GroupBCatalogsReflectEngineMetadata) {
    ErrorContext ctx;

    VirtualResultSet clickhouse_tables;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::CLICKHOUSE, "system", "tables", "", clickhouse_tables, &ctx),
              Status::OK)
        << ctx.message;
    bool found_clickhouse_table = false;
    for (const auto& row : clickhouse_tables.rows) {
        const auto* db_col = row.getColumn("database");
        const auto* table_col = row.getColumn("name");
        if (db_col && table_col && !db_col->isNull() && !table_col->isNull() &&
            db_col->toString() == "ef032_schema" && table_col->toString() == "sensor_points") {
            found_clickhouse_table = true;
            break;
        }
    }
    EXPECT_TRUE(found_clickhouse_table);

    VirtualResultSet duckdb_columns;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::DUCKDB, "duckdb_catalog", "duckdb_columns", "",
                                  duckdb_columns, &ctx),
              Status::OK)
        << ctx.message;
    bool found_duckdb_column = false;
    for (const auto& row : duckdb_columns.rows) {
        const auto* schema_col = row.getColumn("schema_name");
        const auto* table_col = row.getColumn("table_name");
        const auto* column_col = row.getColumn("column_name");
        if (schema_col && table_col && column_col &&
            !schema_col->isNull() && !table_col->isNull() && !column_col->isNull() &&
            schema_col->toString() == "ef032_schema" &&
            table_col->toString() == "sensor_points" &&
            column_col->toString() == "id") {
            found_duckdb_column = true;
            break;
        }
    }
    EXPECT_TRUE(found_duckdb_column);

    VirtualResultSet influx_measurements;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::INFLUXDB, "influxdb_meta", "measurements", "",
                                  influx_measurements, &ctx),
              Status::OK)
        << ctx.message;
    bool found_measurement = false;
    for (const auto& row : influx_measurements.rows) {
        const auto* measurement_col = row.getColumn("measurement_name");
        if (measurement_col && !measurement_col->isNull() &&
            measurement_col->toString() == "ef032_schema.sensor_points") {
            found_measurement = true;
            break;
        }
    }
    EXPECT_TRUE(found_measurement);
}

} // namespace

