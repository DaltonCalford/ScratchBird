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
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class CatalogResourceTimezoneExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_resource_timezone_extension_contract_" +
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

    ID createBundle(CatalogManager::ResourceBundleKind kind,
                    const std::string& name,
                    const std::string& version,
                    bool is_active)
    {
        CatalogManager::ResourceBundleCatalogInfo bundle{};
        bundle.bundle_kind = kind;
        bundle.bundle_name = name;
        bundle.bundle_version = version;
        bundle.source_uri = "spec://resources/" + name;
        bundle.manifest_json = "{\"bundle\":\"" + name + "\"}";
        bundle.content_hash = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        bundle.is_active = is_active;

        ErrorContext ctx;
        ID bundle_id{};
        EXPECT_EQ(catalog_->upsertResourceBundleCatalogEntry(bundle, bundle_id, &ctx), Status::OK)
            << ctx.message;
        return bundle_id;
    }

    ID createTimezone(uint16_t timezone_id, const std::string& name)
    {
        CatalogManager::TimezoneInfo tz{};
        tz.timezone_id = timezone_id;
        tz.name = name;
        tz.abbreviation = "UTC";
        tz.std_offset_minutes = 0;
        tz.observes_dst = false;

        ErrorContext ctx;
        EXPECT_EQ(catalog_->createTimezone(tz, &ctx), Status::OK) << ctx.message;

        CatalogManager::TimezoneInfo fetched{};
        EXPECT_EQ(catalog_->getTimezone(timezone_id, fetched, &ctx), Status::OK) << ctx.message;
        return fetched.timezone_uuid;
    }
};

TEST_F(CatalogResourceTimezoneExtensionContractTest, ResourceBundleAndArtifactContracts)
{
    ErrorContext ctx;
    ID bundle_id{};

    CatalogManager::ResourceBundleCatalogInfo invalid_kind{};
    invalid_kind.bundle_kind = static_cast<CatalogManager::ResourceBundleKind>(99);
    invalid_kind.bundle_name = "invalid";
    invalid_kind.bundle_version = "v1";
    invalid_kind.content_hash = "hash";
    EXPECT_EQ(catalog_->upsertResourceBundleCatalogEntry(invalid_kind, bundle_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::ResourceBundleCatalogInfo bundle{};
    bundle.bundle_kind = CatalogManager::ResourceBundleKind::TIMEZONE;
    bundle.bundle_name = "tzdata";
    bundle.bundle_version = "2026a";
    bundle.source_uri = "file://tzdata";
    bundle.manifest_json = "{\"files\":3}";
    bundle.content_hash = "1111111111111111111111111111111111111111111111111111111111111111";
    bundle.is_active = true;
    ASSERT_EQ(catalog_->upsertResourceBundleCatalogEntry(bundle, bundle_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_NE(bundle_id, ID{});

    CatalogManager::ResourceBundleCatalogInfo duplicate_active = bundle;
    duplicate_active.bundle_id = generateUuidV7();
    duplicate_active.bundle_name = "tzdata-alt";
    duplicate_active.content_hash =
        "2222222222222222222222222222222222222222222222222222222222222222";
    ID duplicate_bundle_id{};
    EXPECT_EQ(catalog_->upsertResourceBundleCatalogEntry(duplicate_active, duplicate_bundle_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::ResourceBundleCatalogInfo active{};
    ASSERT_EQ(
        catalog_->getActiveResourceBundleByKind(CatalogManager::ResourceBundleKind::TIMEZONE, active, &ctx),
        Status::OK)
        << ctx.message;
    EXPECT_EQ(active.bundle_id, bundle_id);

    CatalogManager::ResourceArtifactCatalogInfo missing_bundle{};
    missing_bundle.bundle_id = generateUuidV7();
    missing_bundle.artifact_kind = CatalogManager::ResourceArtifactKind::TZ_SOURCE;
    missing_bundle.artifact_path = "timezones/asia";
    missing_bundle.content_blob = "zone data";
    missing_bundle.content_hash = "hash-missing-bundle";
    ID artifact_id{};
    EXPECT_EQ(catalog_->upsertResourceArtifactCatalogEntry(missing_bundle, artifact_id, &ctx),
              Status::NOT_FOUND);

    CatalogManager::ResourceArtifactCatalogInfo size_mismatch{};
    size_mismatch.bundle_id = bundle_id;
    size_mismatch.artifact_kind = CatalogManager::ResourceArtifactKind::TZ_SOURCE;
    size_mismatch.artifact_path = "timezones/europe";
    size_mismatch.content_blob = "abcdef";
    size_mismatch.content_hash = "hash-size-mismatch";
    size_mismatch.content_size_bytes = 10;
    EXPECT_EQ(catalog_->upsertResourceArtifactCatalogEntry(size_mismatch, artifact_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::ResourceArtifactCatalogInfo artifact{};
    artifact.bundle_id = bundle_id;
    artifact.artifact_kind = CatalogManager::ResourceArtifactKind::TZ_SOURCE;
    artifact.artifact_path = "timezones/africa";
    artifact.content_blob = "africa zone source";
    artifact.content_hash = "hash-africa";
    ASSERT_EQ(catalog_->upsertResourceArtifactCatalogEntry(artifact, artifact_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ResourceArtifactCatalogInfo duplicate_artifact = artifact;
    duplicate_artifact.artifact_id = generateUuidV7();
    duplicate_artifact.content_hash = "hash-africa-dup";
    ID duplicate_artifact_id{};
    EXPECT_EQ(catalog_->upsertResourceArtifactCatalogEntry(
                  duplicate_artifact, duplicate_artifact_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::ResourceArtifactCatalogInfo fetched{};
    ASSERT_EQ(catalog_->getResourceArtifactCatalogEntry(artifact_id, fetched, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched.bundle_id, bundle_id);
    EXPECT_EQ(fetched.artifact_path, "timezones/africa");
    EXPECT_EQ(fetched.content_blob, "africa zone source");

    std::vector<CatalogManager::ResourceArtifactCatalogInfo> artifacts;
    ASSERT_EQ(catalog_->listResourceArtifactCatalogEntries(bundle_id, artifacts, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(artifacts.size(), 1u);

    ASSERT_EQ(catalog_->deleteResourceArtifactCatalogEntry(artifact_id, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(catalog_->getResourceArtifactCatalogEntry(artifact_id, fetched, &ctx), Status::NOT_FOUND);

    ASSERT_EQ(catalog_->deleteResourceBundleCatalogEntry(bundle_id, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(catalog_->getResourceBundleCatalogEntry(bundle_id, active, &ctx), Status::NOT_FOUND);
}

TEST_F(CatalogResourceTimezoneExtensionContractTest, TimezoneTransitionContracts)
{
    ID bundle_id = createBundle(CatalogManager::ResourceBundleKind::TIMEZONE, "tz_bundle", "2026a", true);
    ID timezone_uuid = createTimezone(301, "cat013_timezone_transition");

    ErrorContext ctx;
    ID transition_id{};

    CatalogManager::TimezoneTransitionCatalogInfo invalid_missing_tz{};
    invalid_missing_tz.bundle_id = bundle_id;
    invalid_missing_tz.abbreviation = "EST";
    EXPECT_EQ(catalog_->upsertTimezoneTransitionCatalogEntry(invalid_missing_tz, transition_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::TimezoneTransitionCatalogInfo missing_bundle{};
    missing_bundle.timezone_id = timezone_uuid;
    missing_bundle.bundle_id = generateUuidV7();
    missing_bundle.effective_utc_epoch = 1000;
    missing_bundle.utc_offset_seconds = -18000;
    missing_bundle.is_dst = false;
    missing_bundle.abbreviation = "EST";
    missing_bundle.sequence_no = 1;
    EXPECT_EQ(catalog_->upsertTimezoneTransitionCatalogEntry(missing_bundle, transition_id, &ctx),
              Status::NOT_FOUND);

    CatalogManager::TimezoneTransitionCatalogInfo row{};
    row.timezone_id = timezone_uuid;
    row.bundle_id = bundle_id;
    row.effective_utc_epoch = 1700000000;
    row.utc_offset_seconds = -18000;
    row.is_dst = false;
    row.abbreviation = "EST";
    row.sequence_no = 1;
    ASSERT_EQ(catalog_->upsertTimezoneTransitionCatalogEntry(row, transition_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_NE(transition_id, ID{});

    CatalogManager::TimezoneTransitionCatalogInfo duplicate_seq = row;
    duplicate_seq.transition_id = generateUuidV7();
    duplicate_seq.effective_utc_epoch = 1700001000;
    ID duplicate_seq_id{};
    EXPECT_EQ(catalog_->upsertTimezoneTransitionCatalogEntry(duplicate_seq, duplicate_seq_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::TimezoneTransitionCatalogInfo duplicate_tuple = row;
    duplicate_tuple.transition_id = generateUuidV7();
    duplicate_tuple.sequence_no = 2;
    ID duplicate_tuple_id{};
    EXPECT_EQ(catalog_->upsertTimezoneTransitionCatalogEntry(duplicate_tuple, duplicate_tuple_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::TimezoneTransitionCatalogInfo fetched{};
    ASSERT_EQ(catalog_->getTimezoneTransitionCatalogEntry(transition_id, fetched, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched.sequence_no, 1u);
    EXPECT_EQ(fetched.abbreviation, "EST");

    std::vector<CatalogManager::TimezoneTransitionCatalogInfo> rows;
    ASSERT_EQ(catalog_->listTimezoneTransitionCatalogEntries(timezone_uuid, rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(rows.size(), 1u);

    ASSERT_EQ(catalog_->deleteTimezoneTransitionCatalogEntry(transition_id, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(catalog_->getTimezoneTransitionCatalogEntry(transition_id, fetched, &ctx), Status::NOT_FOUND);
}

TEST_F(CatalogResourceTimezoneExtensionContractTest, TimezoneLeapSecondContracts)
{
    ID bundle_id = createBundle(CatalogManager::ResourceBundleKind::TIMEZONE, "tz_bundle_ls", "2026b", false);

    ErrorContext ctx;
    ID leap_id{};

    CatalogManager::TimezoneLeapSecondCatalogInfo missing_bundle{};
    missing_bundle.bundle_id = generateUuidV7();
    missing_bundle.effective_utc_epoch = 100;
    missing_bundle.total_correction_seconds = 1;
    EXPECT_EQ(catalog_->upsertTimezoneLeapSecondCatalogEntry(missing_bundle, leap_id, &ctx),
              Status::NOT_FOUND);

    CatalogManager::TimezoneLeapSecondCatalogInfo row{};
    row.bundle_id = bundle_id;
    row.effective_utc_epoch = 1483228799;
    row.total_correction_seconds = 37;
    ASSERT_EQ(catalog_->upsertTimezoneLeapSecondCatalogEntry(row, leap_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_NE(leap_id, ID{});

    CatalogManager::TimezoneLeapSecondCatalogInfo duplicate = row;
    duplicate.leap_id = generateUuidV7();
    ID duplicate_id{};
    EXPECT_EQ(catalog_->upsertTimezoneLeapSecondCatalogEntry(duplicate, duplicate_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::TimezoneLeapSecondCatalogInfo fetched{};
    ASSERT_EQ(catalog_->getTimezoneLeapSecondCatalogEntry(leap_id, fetched, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched.total_correction_seconds, 37);

    std::vector<CatalogManager::TimezoneLeapSecondCatalogInfo> rows;
    ASSERT_EQ(catalog_->listTimezoneLeapSecondCatalogEntries(bundle_id, rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(rows.size(), 1u);

    ASSERT_EQ(catalog_->deleteTimezoneLeapSecondCatalogEntry(leap_id, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(catalog_->getTimezoneLeapSecondCatalogEntry(leap_id, fetched, &ctx), Status::NOT_FOUND);
}
