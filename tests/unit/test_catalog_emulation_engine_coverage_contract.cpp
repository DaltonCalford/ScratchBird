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
#include <set>
#include <string>
#include <vector>
#include <unistd.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

namespace {

class CatalogEmulationEngineCoverageContractTest : public ::testing::Test {
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override {
        db_path_ =
            "/tmp/test_catalog_emulation_engine_coverage_contract_" + std::to_string(getpid()) + ".db";
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

    void TearDown() override {
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

TEST_F(CatalogEmulationEngineCoverageContractTest, CanonicalVnextEngineSetAcceptedByCatalog) {
    const std::vector<CatalogManager::EmulationEngine> canonical_set{
        CatalogManager::EmulationEngine::CASSANDRA,
        CatalogManager::EmulationEngine::CLICKHOUSE,
        CatalogManager::EmulationEngine::DUCKDB,
        CatalogManager::EmulationEngine::FIREBIRD,
        CatalogManager::EmulationEngine::INFLUXDB,
        CatalogManager::EmulationEngine::MARIADB,
        CatalogManager::EmulationEngine::MILVUS,
        CatalogManager::EmulationEngine::MONGODB,
        CatalogManager::EmulationEngine::MYSQL,
        CatalogManager::EmulationEngine::NEO4J,
        CatalogManager::EmulationEngine::OPENSEARCH,
        CatalogManager::EmulationEngine::POSTGRESQL,
        CatalogManager::EmulationEngine::REDIS,
    };
    ASSERT_EQ(canonical_set.size(), 13u);

    ErrorContext ctx;
    std::set<uint8_t> inserted_engine_ids;
    for (size_t i = 0; i < canonical_set.size(); ++i) {
        CatalogManager::EmulationProfileCatalogInfo profile{};
        profile.engine = canonical_set[i];
        profile.enabled = true;
        profile.storage_profile = CatalogManager::StorageProfile::NATIVE_EMULATION;
        profile.requested_engine_version = "vnext-" + std::to_string(i + 1);
        profile.installed_txid = 100 + i;
        profile.last_modified_txid = 200 + i;
        profile.config_flags = static_cast<uint64_t>(1u << (i % 8));

        ID emulation_profile_id{};
        ASSERT_EQ(catalog_->upsertEmulationProfileCatalogEntry(profile, emulation_profile_id, &ctx), Status::OK)
            << ctx.message;
        ASSERT_NE(emulation_profile_id, ID{});

        CatalogManager::EmulationProfileCatalogInfo fetched{};
        ASSERT_EQ(catalog_->getEmulationProfileCatalogEntryByEngine(canonical_set[i], fetched, &ctx), Status::OK)
            << ctx.message;
        EXPECT_EQ(fetched.emulation_profile_id, emulation_profile_id);

        inserted_engine_ids.insert(static_cast<uint8_t>(canonical_set[i]));
    }

    EXPECT_EQ(inserted_engine_ids.size(), canonical_set.size());

    std::vector<CatalogManager::EmulationProfileCatalogInfo> rows;
    ASSERT_EQ(catalog_->listEmulationProfileCatalogEntries(rows, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(rows.size(), canonical_set.size());

    std::set<uint8_t> listed_engine_ids;
    for (const auto& row : rows) {
        listed_engine_ids.insert(static_cast<uint8_t>(row.engine));
    }
    EXPECT_EQ(listed_engine_ids, inserted_engine_ids);
}

TEST_F(CatalogEmulationEngineCoverageContractTest, InvalidEmulationEngineValueRejected) {
    CatalogManager::EmulationProfileCatalogInfo invalid{};
    invalid.engine = static_cast<CatalogManager::EmulationEngine>(254);
    invalid.enabled = true;
    invalid.storage_profile = CatalogManager::StorageProfile::RELATIONAL;
    invalid.requested_engine_version = "invalid";

    ErrorContext ctx;
    ID emulation_profile_id{};
    EXPECT_EQ(catalog_->upsertEmulationProfileCatalogEntry(invalid, emulation_profile_id, &ctx),
              Status::INVALID_ARGUMENT);
}

} // namespace
