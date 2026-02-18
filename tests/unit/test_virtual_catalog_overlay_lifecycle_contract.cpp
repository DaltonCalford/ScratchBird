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
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::catalog;
using namespace scratchbird::core;

namespace {

class VirtualCatalogOverlayLifecycleContractTest : public ::testing::Test {
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;

    void SetUp() override {
        db_path_ = "/tmp/test_virtual_catalog_overlay_lifecycle_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);
    }

    void TearDown() override {
        if (db_) {
            db_->close();
            db_.reset();
            catalog_ = nullptr;
        }
        std::remove(db_path_.c_str());
    }

    void upsertProfile(CatalogManager::EmulationEngine engine, bool enabled,
                       const char* version = "1.0") {
        ASSERT_NE(catalog_, nullptr);
        ErrorContext ctx;

        CatalogManager::EmulationProfileCatalogInfo info{};
        info.engine = engine;
        info.enabled = enabled;
        info.is_valid = true;

        CatalogManager::EmulationProfileCatalogInfo existing{};
        if (catalog_->getEmulationProfileCatalogEntryByEngine(engine, existing, &ctx) == Status::OK) {
            info.emulation_profile_id = existing.emulation_profile_id;
        }

        if (enabled) {
            info.storage_profile = CatalogManager::StorageProfile::NATIVE_EMULATION;
            info.requested_engine_version = version;
        }

        ID profile_id{};
        ASSERT_EQ(catalog_->upsertEmulationProfileCatalogEntry(info, profile_id, &ctx), Status::OK)
            << ctx.message;
    }
};

TEST_F(VirtualCatalogOverlayLifecycleContractTest, EmptyProfileTableFallsBackToFullOverlaySet) {
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();
    initializeVirtualCatalogs(catalog_);

    EXPECT_NE(router.getHandler(ProtocolType::SCRATCHBIRD), nullptr);
    EXPECT_NE(router.getHandler(ProtocolType::POSTGRESQL), nullptr);
    EXPECT_NE(router.getHandler(ProtocolType::MYSQL), nullptr);
    EXPECT_NE(router.getHandler(ProtocolType::MONGODB), nullptr);
    EXPECT_NE(router.getHandler(ProtocolType::OPENSEARCH), nullptr);
}

TEST_F(VirtualCatalogOverlayLifecycleContractTest, ProfileRowsConstrainOverlayLifecycleDeterministically) {
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();
    ErrorContext ctx;

    upsertProfile(CatalogManager::EmulationEngine::MYSQL, true, "8.4");
    upsertProfile(CatalogManager::EmulationEngine::POSTGRESQL, false);

    initializeVirtualCatalogs(catalog_);

    EXPECT_NE(router.getHandler(ProtocolType::SCRATCHBIRD), nullptr);
    EXPECT_NE(router.getHandler(ProtocolType::MYSQL), nullptr);
    EXPECT_EQ(router.getHandler(ProtocolType::POSTGRESQL), nullptr);

    VirtualResultSet mysql_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema", "processlist", "",
                                  mysql_result, &ctx), Status::OK)
        << ctx.message;

    VirtualResultSet pg_result;
    EXPECT_EQ(executeVirtualQuery(ProtocolType::POSTGRESQL, "pg_catalog", "pg_namespace", "",
                                  pg_result, &ctx), Status::NOT_FOUND);
}

TEST_F(VirtualCatalogOverlayLifecycleContractTest, ProfileUpdatesApplyOnReload) {
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();
    ErrorContext ctx;

    upsertProfile(CatalogManager::EmulationEngine::MYSQL, true, "8.4");
    upsertProfile(CatalogManager::EmulationEngine::POSTGRESQL, false);
    initializeVirtualCatalogs(catalog_);

    EXPECT_NE(router.getHandler(ProtocolType::MYSQL), nullptr);
    EXPECT_EQ(router.getHandler(ProtocolType::POSTGRESQL), nullptr);

    upsertProfile(CatalogManager::EmulationEngine::MYSQL, false);
    upsertProfile(CatalogManager::EmulationEngine::POSTGRESQL, true, "18.0");
    initializeVirtualCatalogs(catalog_);

    EXPECT_EQ(router.getHandler(ProtocolType::MYSQL), nullptr);
    EXPECT_NE(router.getHandler(ProtocolType::POSTGRESQL), nullptr);

    VirtualResultSet pg_result;
    ASSERT_EQ(executeVirtualQuery(ProtocolType::POSTGRESQL, "pg_catalog", "pg_namespace", "",
                                  pg_result, &ctx), Status::OK)
        << ctx.message;

    VirtualResultSet mysql_result;
    EXPECT_EQ(executeVirtualQuery(ProtocolType::MYSQL, "performance_schema", "processlist", "",
                                  mysql_result, &ctx), Status::NOT_FOUND);
}

} // namespace

