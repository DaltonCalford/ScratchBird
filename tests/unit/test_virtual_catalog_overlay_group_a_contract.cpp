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

class VirtualCatalogOverlayGroupAContractTest : public ::testing::Test {
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    CatalogManager::SessionInfo session_{};
    ID schema_id_{};

    void SetUp() override {
        db_path_ = "/tmp/test_virtual_catalog_overlay_group_a_" + std::to_string(getpid()) + ".db";
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

        ASSERT_EQ(catalog_->createSchema("ef031_ks", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::ColumnInfo id_col{};
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT64);
        id_col.nullable = false;
        id_col.is_primary_key = true;

        CatalogManager::ColumnInfo amt_col{};
        amt_col.column_name = "amount";
        amt_col.data_type = static_cast<uint16_t>(DataType::DECIMAL);
        amt_col.type_precision = 18;
        amt_col.type_scale = 4;
        amt_col.nullable = false;

        ID table_id{};
        ASSERT_EQ(catalog_->createTable(schema_id_, "orders", {id_col, amt_col}, table_id, 0, &ctx), Status::OK)
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

TEST_F(VirtualCatalogOverlayGroupAContractTest, GroupAHandlersRegisteredAndQueryable) {
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();
    ASSERT_NE(router.getHandler(ProtocolType::POSTGRESQL), nullptr);
    ASSERT_NE(router.getHandler(ProtocolType::MYSQL), nullptr);
    ASSERT_NE(router.getHandler(ProtocolType::FIREBIRD), nullptr);
    ASSERT_NE(router.getHandler(ProtocolType::CASSANDRA), nullptr);

    ErrorContext ctx;

    VirtualResultSet pg_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::POSTGRESQL, "pg_catalog", "pg_namespace", "", pg_result, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(pg_result.empty());

    VirtualResultSet mysql_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema", "processlist", "", mysql_result,
                                  &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(mysql_result.empty());

    VirtualResultSet firebird_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::FIREBIRD, "rdb", "RDB$DATABASE", "", firebird_result, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(firebird_result.empty());

    VirtualResultSet cassandra_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::CASSANDRA, "system", "local", "", cassandra_result, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(cassandra_result.empty());
}

TEST_F(VirtualCatalogOverlayGroupAContractTest, CassandraSystemSchemaReflectsCatalogObjects) {
    ErrorContext ctx;

    VirtualResultSet keyspaces;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::CASSANDRA, "system_schema", "keyspaces", "", keyspaces, &ctx),
              Status::OK)
        << ctx.message;
    bool found_keyspace = false;
    for (const auto& row : keyspaces.rows) {
        const auto* ks = row.getColumn("keyspace_name");
        if (ks && !ks->isNull() && ks->toString() == "ef031_ks") {
            found_keyspace = true;
            break;
        }
    }
    EXPECT_TRUE(found_keyspace);

    VirtualResultSet tables;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::CASSANDRA, "system_schema", "tables", "", tables, &ctx),
              Status::OK)
        << ctx.message;
    bool found_table = false;
    for (const auto& row : tables.rows) {
        const auto* ks = row.getColumn("keyspace_name");
        const auto* tbl = row.getColumn("table_name");
        if (ks && tbl && !ks->isNull() && !tbl->isNull() &&
            ks->toString() == "ef031_ks" && tbl->toString() == "orders") {
            found_table = true;
            break;
        }
    }
    EXPECT_TRUE(found_table);

    VirtualResultSet columns;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::CASSANDRA, "system_schema", "columns", "", columns, &ctx),
              Status::OK)
        << ctx.message;
    bool found_column = false;
    for (const auto& row : columns.rows) {
        const auto* ks = row.getColumn("keyspace_name");
        const auto* tbl = row.getColumn("table_name");
        const auto* col = row.getColumn("column_name");
        if (ks && tbl && col && !ks->isNull() && !tbl->isNull() && !col->isNull() &&
            ks->toString() == "ef031_ks" && tbl->toString() == "orders" && col->toString() == "id") {
            found_column = true;
            break;
        }
    }
    EXPECT_TRUE(found_column);
}

} // namespace
