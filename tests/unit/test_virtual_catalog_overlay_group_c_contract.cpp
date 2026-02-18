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

class VirtualCatalogOverlayGroupCContractTest : public ::testing::Test {
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    CatalogManager::SessionInfo session_{};
    ID schema_id_{};

    void SetUp() override {
        db_path_ = "/tmp/test_virtual_catalog_overlay_group_c_" + std::to_string(getpid()) + ".db";
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
        ASSERT_EQ(catalog_->createSession(system_user_id, ID{}, "mongo", session_, &ctx), Status::OK)
            << ctx.message;
        conn_->setSessionContext(session_.session_id, session_.authkey_id, session_.emulation_mode,
                                 session_.policy_epoch_global, session_.policy_epoch_table);
        conn_->beginStatementTracking("SELECT 1");

        ASSERT_EQ(catalog_->createSchema("ef033_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::ColumnInfo id_col{};
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT64);
        id_col.nullable = false;
        id_col.is_primary_key = true;

        CatalogManager::ColumnInfo payload_col{};
        payload_col.column_name = "payload";
        payload_col.data_type = static_cast<uint16_t>(DataType::VARCHAR);
        payload_col.type_precision = 128;
        payload_col.nullable = true;

        ID table_id{};
        ASSERT_EQ(catalog_->createTable(schema_id_, "events", {id_col, payload_col}, table_id, 0, &ctx),
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

TEST_F(VirtualCatalogOverlayGroupCContractTest, GroupCHandlersRegisteredAndQueryable) {
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();
    ASSERT_NE(router.getHandler(ProtocolType::MONGODB), nullptr);
    ASSERT_NE(router.getHandler(ProtocolType::REDIS), nullptr);
    ASSERT_NE(router.getHandler(ProtocolType::NEO4J), nullptr);
    ASSERT_NE(router.getHandler(ProtocolType::MILVUS), nullptr);
    ASSERT_NE(router.getHandler(ProtocolType::OPENSEARCH), nullptr);

    ErrorContext ctx;

    VirtualResultSet mongo_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MONGODB, "mongo_meta", "list_databases", "",
                                  mongo_result, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(mongo_result.empty());

    VirtualResultSet redis_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::REDIS, "redis_meta", "commands", "", redis_result, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(redis_result.empty());

    VirtualResultSet neo4j_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::NEO4J, "neo4j_meta", "databases", "", neo4j_result, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(neo4j_result.empty());

    VirtualResultSet milvus_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MILVUS, "milvus_meta", "collections", "",
                                  milvus_result, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(milvus_result.empty());

    VirtualResultSet opensearch_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::OPENSEARCH, "opensearch_meta", "index_metadata", "",
                                  opensearch_result, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_FALSE(opensearch_result.empty());
}

TEST_F(VirtualCatalogOverlayGroupCContractTest, GroupCCatalogsReflectEngineMetadata) {
    ErrorContext ctx;

    VirtualResultSet mongo_collections;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MONGODB, "mongo_meta", "list_collections", "",
                                  mongo_collections, &ctx),
              Status::OK)
        << ctx.message;
    bool found_mongo_collection = false;
    for (const auto& row : mongo_collections.rows) {
        const auto* db_col = row.getColumn("db_name");
        const auto* collection_col = row.getColumn("collection_name");
        if (db_col && collection_col && !db_col->isNull() && !collection_col->isNull() &&
            db_col->toString() == "ef033_schema" && collection_col->toString() == "events") {
            found_mongo_collection = true;
            break;
        }
    }
    EXPECT_TRUE(found_mongo_collection);

    VirtualResultSet milvus_collections;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MILVUS, "milvus_meta", "collections", "",
                                  milvus_collections, &ctx),
              Status::OK)
        << ctx.message;
    bool found_milvus_collection = false;
    for (const auto& row : milvus_collections.rows) {
        const auto* db_col = row.getColumn("db_name");
        const auto* collection_col = row.getColumn("collection_name");
        if (db_col && collection_col && !db_col->isNull() && !collection_col->isNull() &&
            db_col->toString() == "ef033_schema" && collection_col->toString() == "events") {
            found_milvus_collection = true;
            break;
        }
    }
    EXPECT_TRUE(found_milvus_collection);

    VirtualResultSet opensearch_indices;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::OPENSEARCH, "opensearch_meta", "index_metadata", "",
                                  opensearch_indices, &ctx),
              Status::OK)
        << ctx.message;
    bool found_opensearch_index = false;
    for (const auto& row : opensearch_indices.rows) {
        const auto* index_col = row.getColumn("index_name");
        if (index_col && !index_col->isNull() &&
            index_col->toString() == "ef033_schema.events") {
            found_opensearch_index = true;
            break;
        }
    }
    EXPECT_TRUE(found_opensearch_index);
}

} // namespace

