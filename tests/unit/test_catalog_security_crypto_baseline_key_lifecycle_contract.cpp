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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class CatalogSecurityCryptoBaselineKeyLifecycleContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_security_crypto_baseline_key_lifecycle_contract_" +
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
};

TEST_F(CatalogSecurityCryptoBaselineKeyLifecycleContractTest, TransportSignatureAndPasswordRejects)
{
    ErrorContext ctx;
    CatalogManager::CryptoBaselineEvaluationRequest request{};
    request.crypto_profile_id = CatalogManager::CryptoProfileId::MODERN_BASELINE;
    request.security_tier = CatalogManager::SecurityTierId::TIER_2_STANDARD;
    request.is_network_session = true;
    request.tls_version = CatalogManager::TlsVersion::TLS_1_2;
    request.tls_cipher_suite = "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384";
    request.at_rest_algorithm = CatalogManager::EncryptionAlgorithm::AES_256_GCM;
    request.primary_provider = CatalogManager::KeyProviderKind::OS_KEYRING;
    request.primary_provider_available = true;
    request.primary_provider_authorized = true;
    request.artifact_signed = true;
    request.artifact_signature_algorithm = "Ed25519";

    CatalogManager::CryptoBaselineEvaluationDecision decision{};
    EXPECT_EQ(catalog_->evaluateCryptoBaselinePolicy(request, decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1289");

    request.tls_version = CatalogManager::TlsVersion::TLS_1_3;
    request.tls_cipher_suite = "TLS_AES_256_GCM_SHA384";
    request.credential_kind = CatalogManager::CredentialKind::PASSWORD_SCRAM_SHA256;
    request.is_privileged_account = true;
    EXPECT_EQ(catalog_->evaluateCryptoBaselinePolicy(request, decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1290");

    request.credential_kind = CatalogManager::CredentialKind::PASSWORD_ARGON2ID;
    request.is_privileged_account = false;
    request.artifact_signed = false;
    request.listener_enabled = true;
    request.debug_unsigned_override = false;
    EXPECT_EQ(catalog_->evaluateCryptoBaselinePolicy(request, decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1291");
}

TEST_F(CatalogSecurityCryptoBaselineKeyLifecycleContractTest, AtRestProviderAndRotationRejects)
{
    ErrorContext ctx;

    CatalogManager::EncryptionProfileCatalogInfo profile{};
    profile.profile_id = generateUuidV7();
    profile.profile_name = "en020_profile_rotation";
    profile.cipher = CatalogManager::EncryptionAlgorithm::AES_256_GCM;
    profile.kdf_algorithm = CatalogManager::KdfAlgorithm::ARGON2ID;
    profile.kdf_params_id = generateUuidV7();
    profile.key_rotation_policy = CatalogManager::KeyRotationPolicy::TIME_BASED;
    ASSERT_EQ(catalog_->upsertEncryptionProfileCatalogEntry(profile, &ctx), Status::OK) << ctx.message;

    CatalogManager::EncryptionKeyCatalogInfo active_key{};
    active_key.key_id = generateUuidV7();
    active_key.profile_id = profile.profile_id;
    active_key.key_kind = CatalogManager::KeyMaterialKind::SYMMETRIC;
    active_key.key_status = CatalogManager::EncryptionKeyStatus::ACTIVE;
    active_key.key_material_encrypted_id = generateUuidV7();
    active_key.key_version = 1;
    active_key.has_activated_time = true;
    active_key.activated_time = 100;
    active_key.created_time = 100;
    active_key.key_material_hash.fill(0x22);
    ASSERT_EQ(catalog_->upsertEncryptionKeyCatalogEntry(active_key, &ctx), Status::OK) << ctx.message;

    CatalogManager::CryptoBaselineEvaluationRequest request{};
    request.crypto_profile_id = CatalogManager::CryptoProfileId::COMPAT_EMULATION;
    request.security_tier = CatalogManager::SecurityTierId::TIER_1_BASIC;
    request.at_rest_algorithm = CatalogManager::EncryptionAlgorithm::CHACHA20_POLY1305;
    request.chacha_fallback_explicit = true;
    request.aes_gcm_hw_available = true;
    request.primary_provider = CatalogManager::KeyProviderKind::LOCAL_FILE_KEYSTORE;
    request.primary_provider_available = true;
    request.primary_provider_authorized = true;
    request.artifact_signed = true;
    request.artifact_signature_algorithm = "ECDSA_P256_SHA256";

    CatalogManager::CryptoBaselineEvaluationDecision decision{};
    EXPECT_EQ(catalog_->evaluateCryptoBaselinePolicy(request, decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1292");

    request.at_rest_algorithm = CatalogManager::EncryptionAlgorithm::AES_256_GCM;
    request.security_tier = CatalogManager::SecurityTierId::TIER_4_MILITARY_CLUSTER;
    request.primary_provider = CatalogManager::KeyProviderKind::EXTERNAL_KMS;
    request.has_escrow_provider = true;
    request.escrow_provider = CatalogManager::KeyProviderKind::EXTERNAL_KMS;
    request.escrow_provider_available = true;
    request.escrow_provider_authorized = true;
    EXPECT_EQ(catalog_->evaluateCryptoBaselinePolicy(request, decision, &ctx), Status::CONNECTION_FAILURE);
    EXPECT_EQ(ctx.vnext_code, "SEC_1293");

    request.primary_provider = CatalogManager::KeyProviderKind::PKCS11_HSM;
    request.primary_provider_available = true;
    request.primary_provider_authorized = true;
    request.evaluate_rotation_window = true;
    request.privileged_operation = true;
    request.has_encryption_profile_id = true;
    request.encryption_profile_id = profile.profile_id;
    request.now_utc = 1000000000000000000ULL;
    EXPECT_EQ(catalog_->evaluateCryptoBaselinePolicy(request, decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1294");
}

TEST_F(CatalogSecurityCryptoBaselineKeyLifecycleContractTest, LifecycleTransitionAndConformanceGate)
{
    ErrorContext ctx;

    CatalogManager::EncryptionProfileCatalogInfo profile{};
    profile.profile_id = generateUuidV7();
    profile.profile_name = "en020_lifecycle_profile";
    profile.cipher = CatalogManager::EncryptionAlgorithm::AES_256_GCM;
    profile.kdf_algorithm = CatalogManager::KdfAlgorithm::ARGON2ID;
    profile.kdf_params_id = generateUuidV7();
    profile.key_rotation_policy = CatalogManager::KeyRotationPolicy::TIME_BASED;
    ASSERT_EQ(catalog_->upsertEncryptionProfileCatalogEntry(profile, &ctx), Status::OK) << ctx.message;

    CatalogManager::EncryptionKeyCatalogInfo active_key{};
    active_key.key_id = generateUuidV7();
    active_key.profile_id = profile.profile_id;
    active_key.key_kind = CatalogManager::KeyMaterialKind::SYMMETRIC;
    active_key.key_status = CatalogManager::EncryptionKeyStatus::ACTIVE;
    active_key.key_material_encrypted_id = generateUuidV7();
    active_key.key_version = 1;
    active_key.has_activated_time = true;
    active_key.activated_time = 1000;
    active_key.key_material_hash.fill(0x44);
    ASSERT_EQ(catalog_->upsertEncryptionKeyCatalogEntry(active_key, &ctx), Status::OK) << ctx.message;

    CatalogManager::EncryptionKeyCatalogInfo staged_key{};
    staged_key.key_id = generateUuidV7();
    staged_key.profile_id = profile.profile_id;
    staged_key.key_kind = CatalogManager::KeyMaterialKind::SYMMETRIC;
    staged_key.key_status = CatalogManager::EncryptionKeyStatus::STAGED;
    staged_key.key_material_encrypted_id = generateUuidV7();
    staged_key.key_version = 2;
    staged_key.key_material_hash.fill(0x55);
    ASSERT_EQ(catalog_->upsertEncryptionKeyCatalogEntry(staged_key, &ctx), Status::OK) << ctx.message;

    CatalogManager::EncryptionKeyLifecycleTransitionRequest transition{};
    transition.key_id = staged_key.key_id;
    transition.target_status = CatalogManager::EncryptionKeyStatus::ACTIVE;
    transition.event_time_utc = 2000;
    transition.retire_existing_active = false;
    CatalogManager::EncryptionKeyLifecycleTransitionDecision transition_decision{};

    EXPECT_EQ(catalog_->transitionEncryptionKeyLifecycle(transition, transition_decision, &ctx),
              Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.vnext_code, "SEC_1294");

    transition.retire_existing_active = true;
    ASSERT_EQ(catalog_->transitionEncryptionKeyLifecycle(transition, transition_decision, &ctx), Status::OK)
        << ctx.message;
    EXPECT_TRUE(transition_decision.applied);
    EXPECT_TRUE(transition_decision.rotated_existing_active);

    CatalogManager::EncryptionKeyCatalogInfo reloaded_active{};
    ASSERT_EQ(catalog_->getEncryptionKeyCatalogEntry(active_key.key_id, reloaded_active, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(reloaded_active.key_status, CatalogManager::EncryptionKeyStatus::RETIRED);
    EXPECT_TRUE(reloaded_active.has_retired_time);

    CatalogManager::CryptoBaselineEvaluationRequest bad_gate{};
    bad_gate.crypto_profile_id = CatalogManager::CryptoProfileId::MODERN_BASELINE;
    bad_gate.security_tier = CatalogManager::SecurityTierId::TIER_2_STANDARD;
    bad_gate.is_network_session = true;
    bad_gate.tls_version = CatalogManager::TlsVersion::TLS_1_2;
    bad_gate.tls_cipher_suite = "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384";
    bad_gate.artifact_signed = true;
    bad_gate.artifact_signature_algorithm = "Ed25519";
    bad_gate.at_rest_algorithm = CatalogManager::EncryptionAlgorithm::AES_256_GCM;
    bad_gate.primary_provider = CatalogManager::KeyProviderKind::OS_KEYRING;

    CatalogManager::CryptoBaselineEvaluationDecision gate_decision{};
    EXPECT_EQ(catalog_->evaluateCryptoBaselineConformanceGate(bad_gate, gate_decision, &ctx), Status::NOT_SUPPORTED);
    EXPECT_EQ(ctx.vnext_code, "SEC_1295");

    CatalogManager::CryptoBaselineEvaluationRequest good_gate = bad_gate;
    good_gate.tls_version = CatalogManager::TlsVersion::TLS_1_3;
    good_gate.tls_cipher_suite = "TLS_AES_256_GCM_SHA384";
    ASSERT_EQ(catalog_->evaluateCryptoBaselineConformanceGate(good_gate, gate_decision, &ctx), Status::OK)
        << ctx.message;
    EXPECT_TRUE(gate_decision.gate_pass);
}

TEST_F(CatalogSecurityCryptoBaselineKeyLifecycleContractTest, Argon2RehashHintIsDeterministic)
{
    ErrorContext ctx;
    CatalogManager::CryptoBaselineEvaluationRequest request{};
    request.crypto_profile_id = CatalogManager::CryptoProfileId::MODERN_BASELINE;
    request.security_tier = CatalogManager::SecurityTierId::TIER_2_STANDARD;
    request.credential_kind = CatalogManager::CredentialKind::PASSWORD_ARGON2ID;
    request.require_password_rehash_check = true;
    request.argon2_memory_kib = 131072;
    request.argon2_iterations = 2;
    request.argon2_parallelism = 1;
    request.at_rest_algorithm = CatalogManager::EncryptionAlgorithm::AES_256_GCM;
    request.primary_provider = CatalogManager::KeyProviderKind::OS_KEYRING;
    request.primary_provider_available = true;
    request.primary_provider_authorized = true;
    request.artifact_signed = true;
    request.artifact_signature_algorithm = "Ed25519";

    CatalogManager::CryptoBaselineEvaluationDecision decision{};
    ASSERT_EQ(catalog_->evaluateCryptoBaselinePolicy(request, decision, &ctx), Status::OK)
        << ctx.message;
    EXPECT_TRUE(decision.allowed);
    EXPECT_TRUE(decision.require_password_rehash);
}
