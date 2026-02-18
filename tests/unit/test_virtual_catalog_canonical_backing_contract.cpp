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
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::catalog;
using namespace scratchbird::core;

namespace {

class VirtualCatalogCanonicalBackingContractTest : public ::testing::Test {
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    CatalogManager::SessionInfo session_{};

    void SetUp() override {
        db_path_ = "/tmp/test_virtual_catalog_canonical_backing_contract_" +
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

        const ID system_user_id = catalog_->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id, ID{});
        ASSERT_EQ(catalog_->createSession(system_user_id, ID{}, "native", session_, &ctx), Status::OK)
            << ctx.message;
        conn_->setSessionContext(session_.session_id, session_.authkey_id, session_.emulation_mode,
                                 session_.policy_epoch_global, session_.policy_epoch_table);
        conn_->beginStatementTracking("SELECT 1");
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

TEST_F(VirtualCatalogCanonicalBackingContractTest, CassandraLocalUsesCanonicalProfileAndCluster) {
    ErrorContext ctx;

    CatalogManager::EmulationProfileCatalogInfo profile{};
    profile.engine = CatalogManager::EmulationEngine::CASSANDRA;
    profile.enabled = true;
    profile.storage_profile = CatalogManager::StorageProfile::NATIVE_EMULATION;
    profile.requested_engine_version = "5.1.2";
    profile.installed_txid = 10;
    profile.last_modified_txid = 11;
    ID profile_id{};
    ASSERT_EQ(catalog_->upsertEmulationProfileCatalogEntry(profile, profile_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ClusterCatalogInfo cluster{};
    cluster.cluster_id = generateUuidV7();
    cluster.cluster_name = "ef035_cluster";
    cluster.cluster_mode = CatalogManager::ClusterMode::CLUSTER;
    cluster.cluster_state = CatalogManager::ClusterState::ONLINE;
    cluster.consensus_mode = CatalogManager::ConsensusMode::RAFT;
    cluster.config_version = 1;
    cluster.cluster_state_version = 1;
    ASSERT_EQ(catalog_->upsertClusterCatalogEntry(cluster, &ctx), Status::OK) << ctx.message;

    VirtualResultSet local;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::CASSANDRA, "system", "local", "", local, &ctx), Status::OK)
        << ctx.message;
    ASSERT_FALSE(local.empty());

    const auto* release = local.rows.front().getColumn("release_version");
    ASSERT_NE(release, nullptr);
    ASSERT_FALSE(release->isNull());
    EXPECT_EQ(release->toString(), "5.1.2");

    const auto* cluster_name = local.rows.front().getColumn("cluster_name");
    ASSERT_NE(cluster_name, nullptr);
    ASSERT_FALSE(cluster_name->isNull());
    EXPECT_EQ(cluster_name->toString(), "ef035_cluster");
}

TEST_F(VirtualCatalogCanonicalBackingContractTest, OpenSearchAnalyzerSettingsUseCanonicalTsConfig) {
    ErrorContext ctx;

    CatalogManager::TsParserCatalogInfo parser{};
    parser.parser_id = generateUuidV7();
    parser.parser_name = "ef035_parser";
    parser.start_proc_id = generateUuidV7();
    parser.gettoken_proc_id = generateUuidV7();
    parser.end_proc_id = generateUuidV7();
    parser.lextypes_proc_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertTsParserCatalogEntry(parser, &ctx), Status::OK) << ctx.message;

    CatalogManager::TsTemplateCatalogInfo templ{};
    templ.template_id = generateUuidV7();
    templ.template_name = "ef035_template";
    templ.init_proc_id = generateUuidV7();
    templ.lexize_proc_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertTsTemplateCatalogEntry(templ, &ctx), Status::OK) << ctx.message;

    CatalogManager::TsDictionaryCatalogInfo dict{};
    dict.dictionary_id = generateUuidV7();
    dict.dictionary_name = "ef035_dict";
    dict.template_id = templ.template_id;
    ASSERT_EQ(catalog_->upsertTsDictionaryCatalogEntry(dict, &ctx), Status::OK) << ctx.message;

    CatalogManager::TsConfigCatalogInfo cfg{};
    cfg.config_id = generateUuidV7();
    cfg.config_name = "ef035_cfg";
    cfg.parser_id = parser.parser_id;
    cfg.has_default_dictionary_id = true;
    cfg.default_dictionary_id = dict.dictionary_id;
    ASSERT_EQ(catalog_->upsertTsConfigCatalogEntry(cfg, &ctx), Status::OK) << ctx.message;

    CatalogManager::TsConfigMapCatalogInfo map{};
    map.map_id = generateUuidV7();
    map.config_id = cfg.config_id;
    map.token_type = "asciiword";
    map.dictionary_ids = {dict.dictionary_id};
    map.is_override = true;
    ASSERT_EQ(catalog_->upsertTsConfigMapCatalogEntry(map, &ctx), Status::OK) << ctx.message;

    VirtualResultSet settings;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::OPENSEARCH, "opensearch_meta", "analyzer_settings", "", settings,
                                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(settings.empty());

    bool found = false;
    for (const auto& row : settings.rows) {
        const auto* analyzer_name = row.getColumn("analyzer_name");
        const auto* config_json = row.getColumn("config_json");
        if (!analyzer_name || !config_json || analyzer_name->isNull() || config_json->isNull()) {
            continue;
        }
        if (analyzer_name->toString() != "ef035_cfg") {
            continue;
        }
        const std::string json = config_json->toString();
        if (json.find("\"source\":\"canonical_ts_config\"") == std::string::npos) {
            continue;
        }
        if (json.find("\"token_type\":\"asciiword\"") == std::string::npos) {
            continue;
        }
        found = true;
        break;
    }
    EXPECT_TRUE(found);
}

TEST_F(VirtualCatalogCanonicalBackingContractTest, RedisCommandsUseCanonicalParserCapabilities) {
    ErrorContext ctx;

    CatalogManager::ParserProfileCatalogInfo profile{};
    profile.profile_name = "ef035_redis_profile";
    profile.parser_engine = CatalogManager::EmulationEngine::REDIS;
    profile.version_major = 7;
    profile.version_minor = 0;
    profile.is_native = false;
    profile.is_default = true;
    profile.is_enabled = true;
    profile.profile_hash = std::string(63, 'r');
    ID profile_id{};
    ASSERT_EQ(catalog_->upsertParserProfileCatalogEntry(profile, profile_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ParserCapabilityCatalogInfo capability{};
    capability.parser_profile_id = profile_id;
    capability.feature_family = "redis_command";
    capability.feature_key = "xreadgroup";
    capability.capability_action = CatalogManager::ParserCapabilityAction::IMPLEMENT;
    capability.notes = "arity=-4;flags=readonly";
    ID capability_id{};
    ASSERT_EQ(catalog_->upsertParserCapabilityCatalogEntry(capability, capability_id, &ctx), Status::OK)
        << ctx.message;

    VirtualResultSet commands;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::REDIS, "redis_meta", "commands", "", commands, &ctx), Status::OK)
        << ctx.message;
    ASSERT_FALSE(commands.empty());

    bool found = false;
    for (const auto& row : commands.rows) {
        const auto* name = row.getColumn("command_name");
        const auto* arity = row.getColumn("arity");
        const auto* flags = row.getColumn("flags");
        if (!name || !arity || !flags || name->isNull() || arity->isNull() || flags->isNull()) {
            continue;
        }
        if (name->toString() == "XREADGROUP" && arity->toInt32() == -4 &&
            flags->toString() == "readonly") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

} // namespace
