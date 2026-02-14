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

class CatalogSecurityExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_security_extension_contract_" +
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

TEST_F(CatalogSecurityExtensionContractTest, AuthMappingAndSecurityClassContracts)
{
    ErrorContext ctx;

    ID system_user_id = catalog_->getSystemUserId(&ctx);
    ASSERT_NE(system_user_id, ID{}) << ctx.message;

    CatalogManager::AuthMappingCatalogInfo missing_target{};
    missing_target.mapping_id = generateUuidV7();
    missing_target.auth_method = CatalogManager::AuthMethod::LDAP;
    missing_target.auth_source = "ldap";
    missing_target.external_subject = "uid=test,dc=example,dc=com";
    EXPECT_EQ(catalog_->upsertAuthMappingCatalogEntry(missing_target, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::AuthMappingCatalogInfo mapping{};
    mapping.mapping_id = generateUuidV7();
    mapping.auth_method = CatalogManager::AuthMethod::LDAP;
    mapping.auth_source = "ldap";
    mapping.external_subject = "uid=alice,dc=example,dc=com";
    mapping.external_group = "cn=developers,ou=groups,dc=example,dc=com";
    mapping.database_id = db_->uuid();
    mapping.user_id = system_user_id;
    mapping.priority = 10;
    ASSERT_EQ(catalog_->upsertAuthMappingCatalogEntry(mapping, &ctx), Status::OK) << ctx.message;

    CatalogManager::AuthMappingCatalogInfo dup_mapping{};
    dup_mapping.mapping_id = generateUuidV7();
    dup_mapping.auth_method = mapping.auth_method;
    dup_mapping.auth_source = mapping.auth_source;
    dup_mapping.external_subject = mapping.external_subject;
    dup_mapping.database_id = mapping.database_id;
    dup_mapping.user_id = system_user_id;
    EXPECT_EQ(catalog_->upsertAuthMappingCatalogEntry(dup_mapping, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::AuthMappingCatalogInfo loaded_mapping{};
    ASSERT_EQ(catalog_->getAuthMappingCatalogEntry(mapping.mapping_id, loaded_mapping, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(loaded_mapping.external_subject, mapping.external_subject);
    EXPECT_EQ(loaded_mapping.priority, 10u);

    std::vector<CatalogManager::AuthMappingCatalogInfo> mappings;
    ASSERT_EQ(catalog_->listAuthMappingCatalogEntries(mappings, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(mappings.size(), 1u);

    CatalogManager::RoleSettingCatalogInfo role_setting{};
    role_setting.role_setting_id = generateUuidV7();
    role_setting.role_id = generateUuidV7();
    role_setting.database_id = db_->uuid();
    role_setting.setting_key = "work_mem";
    role_setting.setting_value = "128MB";
    ASSERT_EQ(catalog_->upsertRoleSettingCatalogEntry(role_setting, &ctx), Status::OK) << ctx.message;

    CatalogManager::RoleSettingCatalogInfo duplicate_role_setting{};
    duplicate_role_setting.role_setting_id = generateUuidV7();
    duplicate_role_setting.role_id = role_setting.role_id;
    duplicate_role_setting.database_id = role_setting.database_id;
    duplicate_role_setting.setting_key = role_setting.setting_key;
    duplicate_role_setting.setting_value = "256MB";
    EXPECT_EQ(catalog_->upsertRoleSettingCatalogEntry(duplicate_role_setting, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::SecurityLabelCatalogInfo label{};
    label.security_label_id = generateUuidV7();
    label.object_id = generateUuidV7();
    label.provider_name = "sepgsql";
    label.label_text = "s0:c123,c456";
    label.created_by_id = system_user_id;
    ASSERT_EQ(catalog_->upsertSecurityLabelCatalogEntry(label, &ctx), Status::OK) << ctx.message;

    CatalogManager::SecurityLabelCatalogInfo duplicate_label{};
    duplicate_label.security_label_id = generateUuidV7();
    duplicate_label.object_id = label.object_id;
    duplicate_label.provider_name = label.provider_name;
    duplicate_label.label_text = "different";
    duplicate_label.created_by_id = system_user_id;
    EXPECT_EQ(catalog_->upsertSecurityLabelCatalogEntry(duplicate_label, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::SecurityClassCatalogInfo sec_class{};
    sec_class.security_class_id = generateUuidV7();
    sec_class.class_name = "RDB$SEC_CLASS_0001";
    sec_class.acl_payload_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertSecurityClassCatalogEntry(sec_class, &ctx), Status::OK) << ctx.message;

    CatalogManager::SecurityClassCatalogInfo duplicate_class{};
    duplicate_class.security_class_id = generateUuidV7();
    duplicate_class.class_name = sec_class.class_name;
    duplicate_class.acl_payload_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertSecurityClassCatalogEntry(duplicate_class, &ctx),
              Status::CONSTRAINT_VIOLATION);

    ASSERT_EQ(catalog_->deleteSecurityClassCatalogEntry(sec_class.security_class_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteSecurityLabelCatalogEntry(label.security_label_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteRoleSettingCatalogEntry(role_setting.role_setting_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteAuthMappingCatalogEntry(mapping.mapping_id, &ctx), Status::OK)
        << ctx.message;
}

TEST_F(CatalogSecurityExtensionContractTest, PkiAndCryptoContracts)
{
    ErrorContext ctx;

    CatalogManager::CertRegistryCatalogInfo invalid_cert{};
    invalid_cert.cert_id = generateUuidV7();
    invalid_cert.cert_kind = CatalogManager::CertKind::SERVER;
    invalid_cert.subject_name = "CN=server.example";
    invalid_cert.issuer_name = "CN=example-ca";
    invalid_cert.serial_number = "0x1001";
    invalid_cert.not_before = 1000;
    invalid_cert.not_after = 2000;
    invalid_cert.public_key_id = generateUuidV7();
    invalid_cert.cert_der_id = generateUuidV7();
    invalid_cert.signature_algorithm = "sha256WithRSAEncryption";
    invalid_cert.status = CatalogManager::CertStatus::REVOKED;
    EXPECT_EQ(catalog_->upsertCertRegistryCatalogEntry(invalid_cert, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::CertRegistryCatalogInfo cert{};
    cert.cert_id = generateUuidV7();
    cert.cert_kind = CatalogManager::CertKind::SERVER;
    cert.subject_name = "CN=server.example";
    cert.issuer_name = "CN=example-ca";
    cert.serial_number = "0x1001";
    cert.not_before = 1000;
    cert.not_after = 2000;
    cert.public_key_id = generateUuidV7();
    cert.cert_der_id = generateUuidV7();
    cert.signature_algorithm = "sha256WithRSAEncryption";
    cert.thumbprint_sha256.fill(0x11);
    ASSERT_EQ(catalog_->upsertCertRegistryCatalogEntry(cert, &ctx), Status::OK) << ctx.message;

    CatalogManager::TrustAnchorCatalogInfo anchor{};
    anchor.anchor_id = generateUuidV7();
    anchor.cert_id = cert.cert_id;
    anchor.thumbprint_sha256 = cert.thumbprint_sha256;
    anchor.state = CatalogManager::TrustAnchorState::ACTIVE;
    anchor.activated_time = 1100;
    ASSERT_EQ(catalog_->upsertTrustAnchorCatalogEntry(anchor, &ctx), Status::OK) << ctx.message;

    CatalogManager::TrustAnchorCatalogInfo duplicate_anchor{};
    duplicate_anchor.anchor_id = generateUuidV7();
    duplicate_anchor.cert_id = cert.cert_id;
    duplicate_anchor.thumbprint_sha256 = cert.thumbprint_sha256;
    duplicate_anchor.state = CatalogManager::TrustAnchorState::ACTIVE;
    duplicate_anchor.activated_time = 1101;
    EXPECT_EQ(catalog_->upsertTrustAnchorCatalogEntry(duplicate_anchor, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::CertRevocationCatalogInfo invalid_revocation{};
    invalid_revocation.revocation_id = generateUuidV7();
    invalid_revocation.cert_id = cert.cert_id;
    invalid_revocation.source_kind = CatalogManager::RevocationSource::LOCAL;
    invalid_revocation.reason_code = CatalogManager::RevocationReason::KEY_COMPROMISE;
    invalid_revocation.revoked_time = 2200;
    invalid_revocation.has_expiry_time = true;
    invalid_revocation.expiry_time = 2100;
    EXPECT_EQ(catalog_->upsertCertRevocationCatalogEntry(invalid_revocation, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::CertRevocationCatalogInfo revocation{};
    revocation.revocation_id = generateUuidV7();
    revocation.cert_id = cert.cert_id;
    revocation.source_kind = CatalogManager::RevocationSource::OCSP;
    revocation.reason_code = CatalogManager::RevocationReason::KEY_COMPROMISE;
    revocation.revoked_time = 2200;
    revocation.has_expiry_time = true;
    revocation.expiry_time = 3200;
    revocation.evidence_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertCertRevocationCatalogEntry(revocation, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::CertRevocationCatalogInfo loaded_revocation{};
    ASSERT_EQ(catalog_->getCertRevocationCatalogEntry(revocation.revocation_id, loaded_revocation, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(loaded_revocation.source_kind, CatalogManager::RevocationSource::OCSP);
    EXPECT_EQ(loaded_revocation.reason_code, CatalogManager::RevocationReason::KEY_COMPROMISE);

    CatalogManager::EncryptionProfileCatalogInfo profile{};
    profile.profile_id = generateUuidV7();
    profile.profile_name = "cluster_default";
    profile.cipher = CatalogManager::EncryptionAlgorithm::AES_256_GCM;
    profile.kdf_algorithm = CatalogManager::KdfAlgorithm::ARGON2ID;
    profile.kdf_params_id = generateUuidV7();
    profile.key_rotation_policy = CatalogManager::KeyRotationPolicy::TIME_BASED;
    profile.min_shards_required = 2;
    profile.unlock_timeout_ms = 30000;
    ASSERT_EQ(catalog_->upsertEncryptionProfileCatalogEntry(profile, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::PrivateKeyStoreCatalogInfo private_key{};
    private_key.key_id = generateUuidV7();
    private_key.cert_id = cert.cert_id;
    private_key.key_kind = CatalogManager::KeyMaterialKind::ASYMMETRIC_PRIVATE;
    private_key.key_material_encrypted_id = generateUuidV7();
    private_key.kek_profile_id = profile.profile_id;
    ASSERT_EQ(catalog_->upsertPrivateKeyStoreCatalogEntry(private_key, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::PrivateKeyStoreCatalogInfo duplicate_private_key{};
    duplicate_private_key.key_id = generateUuidV7();
    duplicate_private_key.cert_id = cert.cert_id;
    duplicate_private_key.key_kind = CatalogManager::KeyMaterialKind::ASYMMETRIC_PRIVATE;
    duplicate_private_key.key_material_encrypted_id = generateUuidV7();
    duplicate_private_key.kek_profile_id = profile.profile_id;
    EXPECT_EQ(catalog_->upsertPrivateKeyStoreCatalogEntry(duplicate_private_key, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::ChannelCertBindingCatalogInfo channel_binding{};
    channel_binding.binding_id = generateUuidV7();
    channel_binding.channel_name = "cluster_control";
    channel_binding.cert_kind = CatalogManager::CertKind::SERVER;
    channel_binding.cert_id = cert.cert_id;
    channel_binding.encryption_profile_id = profile.profile_id;
    channel_binding.enforce_mtls = true;
    channel_binding.min_tls_version = CatalogManager::TlsVersion::TLS_1_3;
    ASSERT_EQ(catalog_->upsertChannelCertBindingCatalogEntry(channel_binding, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ChannelCertBindingCatalogInfo duplicate_channel_binding{};
    duplicate_channel_binding.binding_id = generateUuidV7();
    duplicate_channel_binding.channel_name = channel_binding.channel_name;
    duplicate_channel_binding.cert_kind = channel_binding.cert_kind;
    duplicate_channel_binding.cert_id = cert.cert_id;
    duplicate_channel_binding.encryption_profile_id = profile.profile_id;
    duplicate_channel_binding.enforce_mtls = false;
    duplicate_channel_binding.min_tls_version = CatalogManager::TlsVersion::TLS_1_2;
    EXPECT_EQ(catalog_->upsertChannelCertBindingCatalogEntry(duplicate_channel_binding, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::PkiDistributionStateCatalogInfo distribution_state{};
    distribution_state.distribution_id = generateUuidV7();
    distribution_state.member_id = generateUuidV7();
    distribution_state.artifact_kind = CatalogManager::PkiArtifactKind::CERT;
    distribution_state.artifact_id = cert.cert_id;
    distribution_state.artifact_hash.fill(0x3a);
    distribution_state.distribution_state = CatalogManager::DistributionState::PENDING;
    ASSERT_EQ(catalog_->upsertPkiDistributionStateCatalogEntry(distribution_state, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::PkiDistributionStateCatalogInfo duplicate_distribution_state{};
    duplicate_distribution_state.distribution_id = generateUuidV7();
    duplicate_distribution_state.member_id = distribution_state.member_id;
    duplicate_distribution_state.artifact_kind = distribution_state.artifact_kind;
    duplicate_distribution_state.artifact_id = distribution_state.artifact_id;
    duplicate_distribution_state.artifact_hash.fill(0x55);
    duplicate_distribution_state.distribution_state = CatalogManager::DistributionState::FAILED;
    EXPECT_EQ(catalog_->upsertPkiDistributionStateCatalogEntry(duplicate_distribution_state, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::PkiDistributionStateCatalogInfo loaded_distribution_state{};
    ASSERT_EQ(catalog_->getPkiDistributionStateCatalogEntry(distribution_state.distribution_id,
                                                            loaded_distribution_state, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(loaded_distribution_state.distribution_state, CatalogManager::DistributionState::PENDING);

    CatalogManager::TrustAnchorRolloverCatalogInfo rollover{};
    rollover.rollover_id = generateUuidV7();
    rollover.rollover_group_id = generateUuidV7();
    rollover.old_anchor_id = anchor.anchor_id;
    rollover.new_anchor_id = generateUuidV7();
    rollover.phase = CatalogManager::RolloverPhase::PREPARE;
    rollover.quorum_required = 2;
    rollover.quorum_acked = 1;
    rollover.started_time = 2300;
    ASSERT_EQ(catalog_->upsertTrustAnchorRolloverCatalogEntry(rollover, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::TrustAnchorRolloverCatalogInfo invalid_rollover{};
    invalid_rollover.rollover_id = generateUuidV7();
    invalid_rollover.rollover_group_id = generateUuidV7();
    invalid_rollover.old_anchor_id = anchor.anchor_id;
    invalid_rollover.new_anchor_id = generateUuidV7();
    invalid_rollover.phase = CatalogManager::RolloverPhase::PREPARE;
    invalid_rollover.quorum_required = 1;
    invalid_rollover.quorum_acked = 2;
    invalid_rollover.started_time = 2400;
    EXPECT_EQ(catalog_->upsertTrustAnchorRolloverCatalogEntry(invalid_rollover, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::EncryptionKeyCatalogInfo invalid_active_key{};
    invalid_active_key.key_id = generateUuidV7();
    invalid_active_key.profile_id = profile.profile_id;
    invalid_active_key.key_kind = CatalogManager::KeyMaterialKind::SYMMETRIC;
    invalid_active_key.key_status = CatalogManager::EncryptionKeyStatus::ACTIVE;
    invalid_active_key.key_material_encrypted_id = generateUuidV7();
    invalid_active_key.key_version = 1;
    EXPECT_EQ(catalog_->upsertEncryptionKeyCatalogEntry(invalid_active_key, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::EncryptionKeyCatalogInfo key{};
    key.key_id = generateUuidV7();
    key.profile_id = profile.profile_id;
    key.key_kind = CatalogManager::KeyMaterialKind::SYMMETRIC;
    key.key_status = CatalogManager::EncryptionKeyStatus::ACTIVE;
    key.key_material_encrypted_id = generateUuidV7();
    key.key_version = 1;
    key.has_activated_time = true;
    key.activated_time = 1200;
    key.key_material_hash.fill(0x22);
    ASSERT_EQ(catalog_->upsertEncryptionKeyCatalogEntry(key, &ctx), Status::OK) << ctx.message;

    CatalogManager::EncryptionKeyShardCatalogInfo shard{};
    shard.shard_id = generateUuidV7();
    shard.key_id = key.key_id;
    shard.shard_index = 0;
    shard.shard_total = 3;
    shard.shard_material_encrypted_id = generateUuidV7();
    shard.holder_identity = "node-a";
    ASSERT_EQ(catalog_->upsertEncryptionKeyShardCatalogEntry(shard, &ctx), Status::OK) << ctx.message;

    CatalogManager::EncryptionKeyShardCatalogInfo duplicate_shard{};
    duplicate_shard.shard_id = generateUuidV7();
    duplicate_shard.key_id = key.key_id;
    duplicate_shard.shard_index = 0;
    duplicate_shard.shard_total = 3;
    duplicate_shard.shard_material_encrypted_id = generateUuidV7();
    duplicate_shard.holder_identity = "node-b";
    EXPECT_EQ(catalog_->upsertEncryptionKeyShardCatalogEntry(duplicate_shard, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::EncryptionBootstrapInfoCatalogInfo bootstrap{};
    bootstrap.database_id = db_->uuid();
    bootstrap.profile_id = profile.profile_id;
    bootstrap.active_key_id = key.key_id;
    bootstrap.min_shards_required = 2;
    bootstrap.unlock_timeout_ms = 45000;
    bootstrap.unlock_policy = "kms_first";
    bootstrap.last_unlock_result = CatalogManager::UnlockResult::NOT_ATTEMPTED;
    ASSERT_EQ(catalog_->upsertEncryptionBootstrapInfoCatalogEntry(bootstrap, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::EncryptionBootstrapInfoCatalogInfo loaded_bootstrap{};
    ASSERT_EQ(catalog_->getEncryptionBootstrapInfoCatalogEntry(db_->uuid(), loaded_bootstrap, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(loaded_bootstrap.profile_id, profile.profile_id);
    EXPECT_EQ(loaded_bootstrap.unlock_policy, "kms_first");

    ASSERT_EQ(catalog_->deleteEncryptionBootstrapInfoCatalogEntry(db_->uuid(), &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteEncryptionKeyShardCatalogEntry(shard.shard_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteEncryptionKeyCatalogEntry(key.key_id, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteTrustAnchorRolloverCatalogEntry(rollover.rollover_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deletePkiDistributionStateCatalogEntry(distribution_state.distribution_id, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->deleteChannelCertBindingCatalogEntry(channel_binding.binding_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deletePrivateKeyStoreCatalogEntry(private_key.key_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteEncryptionProfileCatalogEntry(profile.profile_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteCertRevocationCatalogEntry(revocation.revocation_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteTrustAnchorCatalogEntry(anchor.anchor_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->deleteCertRegistryCatalogEntry(cert.cert_id, &ctx), Status::OK) << ctx.message;
}
