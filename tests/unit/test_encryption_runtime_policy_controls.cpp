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

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/encryption_key_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class EncryptionRuntimePolicyControlsTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager *catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    static uint64_t nowTicks()
    {
        return static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    }

    static uint64_t daysAgoTicks(int days)
    {
        using ClockDuration = std::chrono::system_clock::duration;
        const uint64_t delta = static_cast<uint64_t>(
            std::chrono::duration_cast<ClockDuration>(std::chrono::hours(24 * days)).count());
        return nowTicks() - delta;
    }

    void connectCurrent()
    {
        ErrorContext ctx;
        ASSERT_NE(db_, nullptr);
        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);
    }

    void closeCurrent()
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
        }
        catalog_ = nullptr;
    }

    void SetUp() override
    {
        db_path_ = "/tmp/test_encryption_runtime_policy_controls_" +
                   std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        connectCurrent();
    }

    void TearDown() override
    {
        closeCurrent();
        std::remove(db_path_.c_str());
    }

    auto makeProfile(const std::string &name,
                     CatalogManager::EncryptionAlgorithm cipher,
                     CatalogManager::KeyRotationPolicy rotation_policy)
        -> CatalogManager::EncryptionProfileCatalogInfo
    {
        CatalogManager::EncryptionProfileCatalogInfo profile{};
        profile.profile_id = generateUuidV7();
        profile.profile_name = name;
        profile.cipher = cipher;
        profile.kdf_algorithm = CatalogManager::KdfAlgorithm::ARGON2ID;
        profile.kdf_params_id = generateUuidV7();
        profile.key_rotation_policy = rotation_policy;
        profile.min_shards_required = 1;
        profile.unlock_timeout_ms = 30000;
        return profile;
    }

    auto makeKey(const ID &profile_id,
                 const ID &key_id,
                 CatalogManager::EncryptionKeyStatus key_status,
                 uint32_t version,
                 uint64_t activated_time,
                 bool has_activated_time)
        -> CatalogManager::EncryptionKeyCatalogInfo
    {
        CatalogManager::EncryptionKeyCatalogInfo key{};
        key.key_id = key_id;
        key.profile_id = profile_id;
        key.key_kind = CatalogManager::KeyMaterialKind::SYMMETRIC;
        key.key_status = key_status;
        key.key_material_encrypted_id = generateUuidV7();
        key.key_version = version;
        key.created_time = activated_time;
        key.has_activated_time = has_activated_time;
        key.activated_time = has_activated_time ? activated_time : 0;
        key.key_material_hash.fill(static_cast<uint8_t>(version));
        return key;
    }

    void installBootstrap(const CatalogManager::EncryptionProfileCatalogInfo &profile,
                          const CatalogManager::EncryptionKeyCatalogInfo &active_key,
                          const std::string &unlock_policy)
    {
        ErrorContext ctx;
        ASSERT_EQ(catalog_->upsertEncryptionProfileCatalogEntry(profile, &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(catalog_->upsertEncryptionKeyCatalogEntry(active_key, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::EncryptionBootstrapInfoCatalogInfo bootstrap{};
        bootstrap.database_id = db_->uuid();
        bootstrap.profile_id = profile.profile_id;
        bootstrap.active_key_id = active_key.key_id;
        bootstrap.min_shards_required = profile.min_shards_required;
        bootstrap.unlock_timeout_ms = profile.unlock_timeout_ms;
        bootstrap.unlock_policy = unlock_policy;
        bootstrap.last_unlock_result = CatalogManager::UnlockResult::SUCCESS;
        bootstrap.has_last_unlock_time = true;
        bootstrap.last_unlock_time = nowTicks();
        bootstrap.policy_version = 1;
        ASSERT_EQ(catalog_->upsertEncryptionBootstrapInfoCatalogEntry(bootstrap, &ctx), Status::OK)
            << ctx.message;
    }

    auto reopen(ErrorContext *ctx) -> Status
    {
        closeCurrent();
        db_ = std::make_unique<Database>();
        Status status = db_->open(db_path_, ctx);
        if (status == Status::OK)
        {
            connectCurrent();
        }
        else
        {
            db_.reset();
            catalog_ = nullptr;
        }
        return status;
    }
};

TEST_F(EncryptionRuntimePolicyControlsTest, DatabaseOpenRejectsUnsupportedExternalUnlockPolicy)
{
    const auto profile = makeProfile(
        "en031_kms_policy",
        CatalogManager::EncryptionAlgorithm::AES_256_GCM,
        CatalogManager::KeyRotationPolicy::TIME_BASED);
    const auto active_key = makeKey(
        profile.profile_id,
        generateUuidV7(),
        CatalogManager::EncryptionKeyStatus::ACTIVE,
        1,
        nowTicks(),
        true);
    installBootstrap(profile, active_key, "kms_first");

    ErrorContext ctx;
    EXPECT_EQ(reopen(&ctx), Status::CONNECTION_FAILURE);
    EXPECT_EQ(ctx.vnext_code, "SEC_1293");
}

TEST_F(EncryptionRuntimePolicyControlsTest, DatabaseOpenRejectsRotationOverdueBootstrapKey)
{
    const auto profile = makeProfile(
        "en031_rotation_overdue",
        CatalogManager::EncryptionAlgorithm::AES_256_GCM,
        CatalogManager::KeyRotationPolicy::TIME_BASED);
    const auto active_key = makeKey(
        profile.profile_id,
        generateUuidV7(),
        CatalogManager::EncryptionKeyStatus::ACTIVE,
        1,
        daysAgoTicks(120),
        true);
    installBootstrap(profile, active_key, "os_keyring");

    ErrorContext ctx;
    EXPECT_EQ(reopen(&ctx), Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1294");
}

TEST_F(EncryptionRuntimePolicyControlsTest, RotateDatabaseKeyActivatesStagedKeyAndUpdatesBootstrap)
{
    const auto profile = makeProfile(
        "en031_rotation_success",
        CatalogManager::EncryptionAlgorithm::AES_256_GCM,
        CatalogManager::KeyRotationPolicy::TIME_BASED);
    const ID active_key_id = generateUuidV7();
    const ID staged_key_id = generateUuidV7();
    const auto active_key = makeKey(
        profile.profile_id,
        active_key_id,
        CatalogManager::EncryptionKeyStatus::ACTIVE,
        1,
        nowTicks(),
        true);
    installBootstrap(profile, active_key, "os_keyring");

    ErrorContext ctx;
    auto staged_key = makeKey(
        profile.profile_id,
        staged_key_id,
        CatalogManager::EncryptionKeyStatus::STAGED,
        2,
        nowTicks(),
        false);
    ASSERT_EQ(catalog_->upsertEncryptionKeyCatalogEntry(staged_key, &ctx), Status::OK) << ctx.message;

    ASSERT_NE(db_->encryption_key_manager(), nullptr);
    ASSERT_EQ(db_->encryption_key_manager()->rotateDatabaseKey(staged_key_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::EncryptionBootstrapInfoCatalogInfo bootstrap{};
    ASSERT_EQ(catalog_->getEncryptionBootstrapInfoCatalogEntry(db_->uuid(), bootstrap, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(bootstrap.active_key_id, staged_key_id);

    CatalogManager::EncryptionKeyCatalogInfo reloaded_active{};
    ASSERT_EQ(catalog_->getEncryptionKeyCatalogEntry(active_key_id, reloaded_active, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(reloaded_active.key_status, CatalogManager::EncryptionKeyStatus::RETIRED);
    EXPECT_TRUE(reloaded_active.has_retired_time);

    CatalogManager::EncryptionKeyCatalogInfo reloaded_staged{};
    ASSERT_EQ(catalog_->getEncryptionKeyCatalogEntry(staged_key_id, reloaded_staged, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(reloaded_staged.key_status, CatalogManager::EncryptionKeyStatus::ACTIVE);
    EXPECT_TRUE(reloaded_staged.has_activated_time);

    ErrorContext reopen_ctx;
    EXPECT_EQ(reopen(&reopen_ctx), Status::OK) << reopen_ctx.message;
}
