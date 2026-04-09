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
#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace {

template <typename Row, typename Predicate>
const Row* findRow(const std::vector<Row>& rows, Predicate predicate) {
    const auto it = std::find_if(rows.begin(), rows.end(), predicate);
    return it == rows.end() ? nullptr : &(*it);
}

}  // namespace

TEST(CatalogListenerTopologyBootstrapContractTest,
     BootstrapConfigurationSeedsListenerTopologyAndGenerationRows) {
    const std::string db_path =
        scratchbird::testing::uniqueTestDbPath("listener_topology_bootstrap_contract", ".sbdb");

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 16384, &ctx), Status::OK) << ctx.message;

    Database db;
    ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;

    std::unique_ptr<ConnectionContext> conn;
    ASSERT_EQ(db.connect(conn, &ctx), Status::OK) << ctx.message;
    ConnectionContext::setCurrent(conn.get());

    ASSERT_EQ(db.bootstrapConfigurationCatalog(&ctx), Status::OK) << ctx.message;
    ASSERT_EQ(db.bootstrapConfigurationCatalog(&ctx), Status::OK) << ctx.message;

    CatalogManager* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    std::vector<CatalogManager::ParserPoolPolicyCatalogInfo> parser_policies;
    ASSERT_EQ(catalog->listParserPoolPolicyCatalogEntries(parser_policies, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(parser_policies.size(), 1u);
    EXPECT_EQ(parser_policies.front().policy_name, "default");
    EXPECT_EQ(parser_policies.front().parser_library_family, "scratchbird_sql");
    EXPECT_GE(parser_policies.front().preferred_workers, parser_policies.front().min_workers);
    EXPECT_GE(parser_policies.front().max_workers, parser_policies.front().preferred_workers);

    std::vector<CatalogManager::ListenerProfileCatalogInfo> profiles;
    ASSERT_EQ(catalog->listListenerProfileCatalogEntries(profiles, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(profiles.size(), 10u);

    std::vector<CatalogManager::ListenerBindingCatalogInfo> bindings;
    ASSERT_EQ(catalog->listListenerBindingCatalogEntries(bindings, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(bindings.size(), profiles.size());

    std::vector<CatalogManager::ListenerEmulationBindingCatalogInfo> emulation_bindings;
    ASSERT_EQ(catalog->listListenerEmulationBindingCatalogEntries(emulation_bindings, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(emulation_bindings.size(), 8u);

    std::vector<CatalogManager::ListenerRuntimeTargetCatalogInfo> runtime_targets;
    ASSERT_EQ(catalog->listListenerRuntimeTargetCatalogEntries(runtime_targets, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(runtime_targets.size(), profiles.size());

    std::vector<CatalogManager::ListenerGenerationRecordCatalogInfo> generation_rows;
    ASSERT_EQ(catalog->listListenerGenerationRecordCatalogEntries(generation_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(generation_rows.size(), profiles.size());

    const auto* native_profile = findRow(
        profiles, [](const auto& row) { return row.profile_name == "native"; });
    ASSERT_NE(native_profile, nullptr);
    EXPECT_EQ(native_profile->protocol_family, "native");
    EXPECT_FALSE(native_profile->manager_fronted);
    EXPECT_TRUE(native_profile->has_owner_database_uuid);
    EXPECT_EQ(native_profile->owner_database_uuid, db.uuid());

    const auto* pg_profile = findRow(
        profiles, [](const auto& row) { return row.profile_name == "postgresql"; });
    ASSERT_NE(pg_profile, nullptr);
    EXPECT_TRUE(pg_profile->manager_fronted);
    EXPECT_TRUE(pg_profile->has_owner_database_uuid);
    EXPECT_EQ(pg_profile->owner_database_uuid, db.uuid());

    const auto* mgmt_profile = findRow(
        profiles, [](const auto& row) { return row.profile_name == "mgmt_ipc"; });
    ASSERT_NE(mgmt_profile, nullptr);
    EXPECT_EQ(mgmt_profile->protocol_family, "mgmt_ipc");

    const auto* pg_emulation = findRow(
        emulation_bindings, [pg_profile](const auto& row) {
            return pg_profile != nullptr && row.listener_profile_id == pg_profile->listener_profile_id;
        });
    ASSERT_NE(pg_emulation, nullptr);
    EXPECT_EQ(pg_emulation->emulation_family, "postgresql");
    EXPECT_EQ(pg_emulation->protocol_surface, "postgresql");
    EXPECT_TRUE(pg_emulation->has_parser_pool_policy_uuid);
    EXPECT_EQ(pg_emulation->parser_pool_policy_uuid, parser_policies.front().parser_pool_policy_id);

    const auto* native_target = findRow(
        runtime_targets, [native_profile](const auto& row) {
            return native_profile != nullptr && row.listener_profile_id == native_profile->listener_profile_id;
        });
    ASSERT_NE(native_target, nullptr);
    EXPECT_EQ(native_target->target_kind, "database");
    EXPECT_TRUE(native_target->has_target_database_uuid);
    EXPECT_EQ(native_target->target_database_uuid, db.uuid());
    EXPECT_TRUE(native_target->has_last_applied_generation);
    EXPECT_EQ(native_target->current_generation, native_target->last_applied_generation);

    for (const auto& row : generation_rows) {
        EXPECT_EQ(row.target_database_uuid, db.uuid());
        EXPECT_EQ(row.committed_generation, row.applied_generation);
        EXPECT_EQ(row.drift_state, "CONSISTENT");
    }

    ConnectionContext::setCurrent(nullptr);
    conn.reset();
    db.close();
}
