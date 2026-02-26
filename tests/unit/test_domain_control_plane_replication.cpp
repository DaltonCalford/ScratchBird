/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/cluster_write_safety.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <unordered_map>

namespace scratchbird::core
{

    namespace
    {

        auto hasReason(const DomainJoinValidationResult& result, const std::string& reason) -> bool
        {
            return std::find(result.mismatch_reasons.begin(), result.mismatch_reasons.end(), reason) !=
                result.mismatch_reasons.end();
        }

        auto toHashMap(const std::vector<DomainJoinManifestEntry>& entries)
            -> std::unordered_map<ID, std::string, IDHash>
        {
            std::unordered_map<ID, std::string, IDHash> out;
            for (const DomainJoinManifestEntry& entry : entries)
            {
                out[entry.domain_id] = entry.definition_hash;
            }
            return out;
        }

    } // namespace

    TEST(DomainControlPlaneReplicaCatalogTest, AppliesEventsAndExportsJoinManifest)
    {
        DomainControlPlaneReplicaCatalog catalog;
        const ID domain_a = generateUuidV7();
        const ID domain_b = generateUuidV7();

        DomainControlPlaneEvent create_a{};
        create_a.cluster_config_epoch = 10;
        create_a.schema_epoch = 20;
        create_a.event_type = DomainControlPlaneEventType::CREATE;
        create_a.domain_id = domain_a;
        create_a.definition_hash = DomainControlPlaneReplicaCatalog::computeDefinitionHash(
            "domain_a:VARCHAR(32)");
        ASSERT_EQ(catalog.appendEvent(create_a), Status::OK);

        DomainControlPlaneEvent create_b{};
        create_b.cluster_config_epoch = 11;
        create_b.schema_epoch = 21;
        create_b.event_type = DomainControlPlaneEventType::CREATE;
        create_b.domain_id = domain_b;
        create_b.definition_hash = DomainControlPlaneReplicaCatalog::computeDefinitionHash(
            "domain_b:INTEGER");
        ASSERT_EQ(catalog.appendEvent(create_b), Status::OK);

        DomainControlPlaneEvent alter_a = create_a;
        alter_a.cluster_config_epoch = 12;
        alter_a.schema_epoch = 22;
        alter_a.event_type = DomainControlPlaneEventType::ALTER;
        alter_a.definition_hash = DomainControlPlaneReplicaCatalog::computeDefinitionHash(
            "domain_a:VARCHAR(64)");
        ASSERT_EQ(catalog.appendEvent(alter_a), Status::OK);

        DomainControlPlaneEvent drop_b = create_b;
        drop_b.cluster_config_epoch = 13;
        drop_b.schema_epoch = 23;
        drop_b.event_type = DomainControlPlaneEventType::DROP;
        drop_b.definition_hash.clear();
        ASSERT_EQ(catalog.appendEvent(drop_b), Status::OK);

        std::vector<DomainControlPlaneEvent> events;
        ASSERT_EQ(catalog.eventLog(events), Status::OK);
        ASSERT_EQ(events.size(), 4u);
        EXPECT_EQ(events[0].event_type, DomainControlPlaneEventType::CREATE);
        EXPECT_EQ(events[1].event_type, DomainControlPlaneEventType::CREATE);
        EXPECT_EQ(events[2].event_type, DomainControlPlaneEventType::ALTER);
        EXPECT_EQ(events[3].event_type, DomainControlPlaneEventType::DROP);

        std::vector<DomainJoinManifestEntry> manifest;
        ASSERT_EQ(catalog.exportManifest(manifest), Status::OK);
        ASSERT_EQ(manifest.size(), 1u);
        std::unordered_map<ID, std::string, IDHash> by_id = toHashMap(manifest);
        ASSERT_EQ(by_id.count(domain_a), 1u);
        EXPECT_EQ(by_id[domain_a], alter_a.definition_hash);

        DomainJoinValidationResult join{};
        ASSERT_EQ(catalog.validateJoinManifest(manifest, join), Status::OK);
        EXPECT_TRUE(join.valid);
        EXPECT_EQ(join.mismatch_count, 0u);
        EXPECT_EQ(join.local_domain_count, 1u);
        EXPECT_EQ(join.remote_domain_count, 1u);
    }

    TEST(DomainControlPlaneReplicaCatalogTest, DetectsJoinManifestHashAndMembershipMismatch)
    {
        DomainControlPlaneReplicaCatalog catalog;
        const ID domain_a = generateUuidV7();
        const ID domain_b = generateUuidV7();
        const ID domain_c = generateUuidV7();

        DomainControlPlaneEvent create_a{};
        create_a.cluster_config_epoch = 100;
        create_a.schema_epoch = 200;
        create_a.event_type = DomainControlPlaneEventType::CREATE;
        create_a.domain_id = domain_a;
        create_a.definition_hash = DomainControlPlaneReplicaCatalog::computeDefinitionHash(
            "domain_a:VARCHAR(32)");
        ASSERT_EQ(catalog.appendEvent(create_a), Status::OK);

        DomainControlPlaneEvent create_b = create_a;
        create_b.cluster_config_epoch = 101;
        create_b.schema_epoch = 201;
        create_b.domain_id = domain_b;
        create_b.definition_hash = DomainControlPlaneReplicaCatalog::computeDefinitionHash(
            "domain_b:INTEGER");
        ASSERT_EQ(catalog.appendEvent(create_b), Status::OK);

        std::vector<DomainJoinManifestEntry> remote_manifest;
        remote_manifest.push_back({domain_a, "remote-wrong-hash"});
        remote_manifest.push_back({domain_a, "remote-duplicate-hash"});
        remote_manifest.push_back({ID{}, "zero-id"});
        remote_manifest.push_back(
            {domain_c, DomainControlPlaneReplicaCatalog::computeDefinitionHash("domain_c:BOOLEAN")});

        DomainJoinValidationResult join{};
        ASSERT_EQ(catalog.validateJoinManifest(remote_manifest, join), Status::OK);
        EXPECT_FALSE(join.valid);
        EXPECT_EQ(join.local_domain_count, 2u);
        EXPECT_EQ(join.remote_domain_count, remote_manifest.size());

        EXPECT_TRUE(hasReason(join, "remote_zero_domain_id"));
        EXPECT_TRUE(hasReason(join, "remote_duplicate_domain_id:" + domain_a.toString()));
        EXPECT_TRUE(hasReason(join, "hash_mismatch:" + domain_a.toString()));
        EXPECT_TRUE(hasReason(join, "missing_remote_domain:" + domain_b.toString()));
        EXPECT_TRUE(hasReason(join, "unexpected_remote_domain:" + domain_c.toString()));
        EXPECT_GE(join.mismatch_count, 5u);
    }

    TEST(DomainControlPlaneReplicaCatalogTest, EnforcesMonotonicEpochsAndHashRequirements)
    {
        DomainControlPlaneReplicaCatalog catalog;
        const ID domain_id = generateUuidV7();

        DomainControlPlaneEvent create{};
        create.cluster_config_epoch = 5;
        create.schema_epoch = 7;
        create.event_type = DomainControlPlaneEventType::CREATE;
        create.domain_id = domain_id;
        create.definition_hash = DomainControlPlaneReplicaCatalog::computeDefinitionHash(
            "domain_x:BIGINT");
        ASSERT_EQ(catalog.appendEvent(create), Status::OK);

        DomainControlPlaneEvent regressed_cluster = create;
        regressed_cluster.cluster_config_epoch = 4;
        regressed_cluster.schema_epoch = 8;
        regressed_cluster.event_type = DomainControlPlaneEventType::ALTER;
        regressed_cluster.definition_hash = DomainControlPlaneReplicaCatalog::computeDefinitionHash(
            "domain_x:NUMERIC");
        EXPECT_EQ(catalog.appendEvent(regressed_cluster), Status::INVALID_TRANSACTION_STATE);

        DomainControlPlaneEvent regressed_schema = create;
        regressed_schema.cluster_config_epoch = 8;
        regressed_schema.schema_epoch = 6;
        regressed_schema.event_type = DomainControlPlaneEventType::ALTER;
        regressed_schema.definition_hash = DomainControlPlaneReplicaCatalog::computeDefinitionHash(
            "domain_x:DOUBLE");
        EXPECT_EQ(catalog.appendEvent(regressed_schema), Status::INVALID_TRANSACTION_STATE);

        DomainControlPlaneEvent empty_hash = create;
        empty_hash.cluster_config_epoch = 8;
        empty_hash.schema_epoch = 8;
        empty_hash.event_type = DomainControlPlaneEventType::ALTER;
        empty_hash.definition_hash.clear();
        EXPECT_EQ(catalog.appendEvent(empty_hash), Status::INVALID_ARGUMENT);

        DomainControlPlaneEvent drop = create;
        drop.cluster_config_epoch = 8;
        drop.schema_epoch = 8;
        drop.event_type = DomainControlPlaneEventType::DROP;
        drop.definition_hash.clear();
        EXPECT_EQ(catalog.appendEvent(drop), Status::OK);
    }

} // namespace scratchbird::core
