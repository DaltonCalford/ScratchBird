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

#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include <openssl/sha.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

namespace {

std::array<uint8_t, 32> sha256Bytes(const std::string& text)
{
    std::array<uint8_t, 32> out{};
    SHA256(reinterpret_cast<const unsigned char*>(text.data()),
           static_cast<unsigned long>(text.size()),
           out.data());
    return out;
}

bool fileContainsAscii(const std::string& path, const std::string& needle)
{
    if (needle.empty()) {
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return bytes.find(needle) != std::string::npos;
}

Status createIdentityAndPolicy(CatalogManager* catalog,
                               const std::string& principal_name,
                               const ID& mfa_policy_id,
                               ID& account_id_out,
                               ErrorContext* ctx)
{
    CatalogManager::AuthProviderCatalogInfo provider{};
    provider.provider_id = generateUuidV7();
    provider.provider_name = principal_name + "_provider";
    provider.provider_kind = CatalogManager::AuthProviderKind::INTERNAL_SCRAM_SHA256;
    provider.provider_state = CatalogManager::AuthProviderState::ENABLED;
    provider.priority_rank = 1;
    provider.fail_mode = CatalogManager::AuthProviderFailMode::TRY_NEXT;
    Status status = catalog->upsertAuthProviderCatalogEntry(provider, ctx);
    if (status != Status::OK) {
        return status;
    }

    CatalogManager::AuthPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.policy_name = principal_name + "_policy";
    policy.provider_chain = {provider.provider_id};
    policy.mfa_required = true;
    policy.has_mfa_policy = (mfa_policy_id != ID{});
    policy.mfa_policy_id = mfa_policy_id;
    policy.allow_password_fallback = false;
    policy.allowed_auth_method_mask = CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256;
    policy.has_required_auth_method = true;
    policy.required_auth_method = CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256;
    policy.allowed_transport_mask = CatalogManager::AUTH_POLICY_TRANSPORT_ALL;
    status = catalog->upsertAuthPolicyCatalogEntry(policy, ctx);
    if (status != Status::OK) {
        return status;
    }

    CatalogManager::PrincipalAccountCatalogInfo account{};
    account.account_id = generateUuidV7();
    account.principal_name = principal_name;
    account.principal_kind = CatalogManager::PrincipalKind::USER;
    account.source_scope_kind = CatalogManager::SourceScopeKind::ANY;
    account.auth_policy_id = policy.policy_id;
    status = catalog->upsertPrincipalAccountCatalogEntry(account, ctx);
    if (status != Status::OK) {
        return status;
    }

    account_id_out = account.account_id;
    return Status::OK;
}

}  // namespace

class CatalogSecurityMfaPolicyEnrollmentRecoveryContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        db_path_ = "/tmp/test_catalog_security_mfa_contract_" +
                   std::to_string(getpid()) + "_" + std::to_string(now) + ".db";
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
        if (db_) {
            db_->close();
            db_.reset();
            catalog_ = nullptr;
        }
        std::remove(db_path_.c_str());
    }
};

TEST_F(CatalogSecurityMfaPolicyEnrollmentRecoveryContractTest,
       MfaPolicyEnrollmentCrudAndEncryptedSecretRoundTrip)
{
    ErrorContext ctx;

    CatalogManager::MfaPolicyCatalogInfo policy{};
    policy.mfa_policy_id = generateUuidV7();
    policy.policy_name = "mfa_policy_roundtrip";
    policy.primary_factor = CatalogManager::MfaFactorType::TOTP;
    policy.allow_recovery_codes = true;
    policy.allow_break_glass = true;
    policy.max_challenge_attempts = 5;
    policy.challenge_ttl_ms = 60000;
    policy.step_up_ttl_ms = 120000;
    ASSERT_EQ(catalog_->upsertMfaPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    ID account_id{};
    ASSERT_EQ(createIdentityAndPolicy(catalog_, "mfa_contract_user", policy.mfa_policy_id,
                                      account_id, &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::MfaPolicyCatalogInfo loaded_policy{};
    ASSERT_EQ(catalog_->getMfaPolicyCatalogEntry(policy.mfa_policy_id, loaded_policy, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(loaded_policy.policy_name, policy.policy_name);
    EXPECT_EQ(loaded_policy.max_challenge_attempts, 5);
    EXPECT_EQ(loaded_policy.step_up_ttl_ms, 120000u);

    std::vector<CatalogManager::MfaPolicyCatalogInfo> listed_policies;
    ASSERT_EQ(catalog_->listMfaPolicyCatalogEntries(listed_policies, &ctx), Status::OK) << ctx.message;
    ASSERT_FALSE(listed_policies.empty());

    static const std::string kSecret = "JBSWY3DPEHPK3PXP";
    CatalogManager::MfaEnrollmentCatalogInfo enrollment{};
    enrollment.enrollment_id = generateUuidV7();
    enrollment.account_id = account_id;
    enrollment.mfa_policy_id = policy.mfa_policy_id;
    enrollment.factor_type = CatalogManager::MfaFactorType::TOTP;
    enrollment.is_primary = true;
    enrollment.is_enrolled = true;
    enrollment.has_secret = true;
    enrollment.secret_base32 = kSecret;
    enrollment.totp_digits = 6;
    enrollment.totp_period = 30;
    enrollment.totp_look_ahead = 1;
    enrollment.totp_look_behind = 1;
    enrollment.enrolled_time_utc = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());

    ASSERT_EQ(catalog_->upsertMfaEnrollmentCatalogEntry(enrollment, &ctx), Status::OK) << ctx.message;

    CatalogManager::MfaEnrollmentCatalogInfo loaded_enrollment{};
    ASSERT_EQ(catalog_->getMfaEnrollmentCatalogEntry(enrollment.enrollment_id, loaded_enrollment, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_TRUE(loaded_enrollment.has_secret);
    EXPECT_EQ(loaded_enrollment.secret_base32, kSecret);
    EXPECT_EQ(loaded_enrollment.account_id, account_id);

    std::vector<CatalogManager::MfaEnrollmentCatalogInfo> listed_enrollments;
    ASSERT_EQ(catalog_->listMfaEnrollmentCatalogEntries(account_id, listed_enrollments, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(listed_enrollments.size(), 1u);
    EXPECT_EQ(listed_enrollments.front().secret_base32, kSecret);

    EXPECT_FALSE(fileContainsAscii(db_path_, kSecret))
        << "MFA secret appeared in plaintext on disk";

    ASSERT_EQ(catalog_->deleteMfaEnrollmentCatalogEntry(enrollment.enrollment_id, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(catalog_->getMfaEnrollmentCatalogEntry(enrollment.enrollment_id, loaded_enrollment, &ctx),
              Status::NOT_FOUND);

    ASSERT_EQ(catalog_->deleteMfaPolicyCatalogEntry(policy.mfa_policy_id, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(catalog_->getMfaPolicyCatalogEntry(policy.mfa_policy_id, loaded_policy, &ctx),
              Status::NOT_FOUND);
}

TEST_F(CatalogSecurityMfaPolicyEnrollmentRecoveryContractTest,
       RecoveryCodeConsumeEnforcesBreakGlassExhaustionAndCooldown)
{
    ErrorContext ctx;

    CatalogManager::MfaPolicyCatalogInfo policy{};
    policy.mfa_policy_id = generateUuidV7();
    policy.policy_name = "mfa_recovery_policy";
    policy.primary_factor = CatalogManager::MfaFactorType::TOTP;
    policy.allow_recovery_codes = true;
    policy.allow_break_glass = true;
    policy.max_challenge_attempts = 3;
    policy.challenge_ttl_ms = 60000;
    policy.step_up_ttl_ms = 60000;
    ASSERT_EQ(catalog_->upsertMfaPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    ID account_id{};
    ASSERT_EQ(createIdentityAndPolicy(catalog_, "mfa_recovery_user", policy.mfa_policy_id,
                                      account_id, &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::MfaRecoveryCodeCatalogInfo normal{};
    normal.recovery_id = generateUuidV7();
    normal.account_id = account_id;
    normal.mfa_policy_id = policy.mfa_policy_id;
    normal.is_break_glass = false;
    normal.code_hash = sha256Bytes("normal-code");
    normal.max_uses = 2;
    normal.uses = 0;
    normal.cooldown_ms = 0;
    ASSERT_EQ(catalog_->upsertMfaRecoveryCodeCatalogEntry(normal, &ctx), Status::OK) << ctx.message;

    CatalogManager::MfaRecoveryCodeCatalogInfo consumed{};
    ASSERT_EQ(catalog_->consumeMfaRecoveryCode(account_id, normal.code_hash, false, consumed, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(consumed.uses, 1u);
    ASSERT_EQ(catalog_->consumeMfaRecoveryCode(account_id, normal.code_hash, false, consumed, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(consumed.uses, 2u);

    EXPECT_EQ(catalog_->consumeMfaRecoveryCode(account_id, normal.code_hash, false, consumed, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1220");

    CatalogManager::MfaRecoveryCodeCatalogInfo break_glass{};
    break_glass.recovery_id = generateUuidV7();
    break_glass.account_id = account_id;
    break_glass.mfa_policy_id = policy.mfa_policy_id;
    break_glass.is_break_glass = true;
    break_glass.code_hash = sha256Bytes("break-glass-code");
    break_glass.max_uses = 1;
    break_glass.uses = 0;
    break_glass.cooldown_ms = 0;
    ASSERT_EQ(catalog_->upsertMfaRecoveryCodeCatalogEntry(break_glass, &ctx), Status::OK) << ctx.message;

    EXPECT_EQ(catalog_->consumeMfaRecoveryCode(account_id,
                                               break_glass.code_hash,
                                               false,
                                               consumed,
                                               &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1222");

    ASSERT_EQ(catalog_->consumeMfaRecoveryCode(account_id,
                                               break_glass.code_hash,
                                               true,
                                               consumed,
                                               &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_TRUE(consumed.is_break_glass);

    CatalogManager::MfaRecoveryCodeCatalogInfo throttled{};
    throttled.recovery_id = generateUuidV7();
    throttled.account_id = account_id;
    throttled.mfa_policy_id = policy.mfa_policy_id;
    throttled.is_break_glass = false;
    throttled.code_hash = sha256Bytes("cooldown-code");
    throttled.max_uses = 2;
    throttled.uses = 0;
    throttled.cooldown_ms = 600000;
    ASSERT_EQ(catalog_->upsertMfaRecoveryCodeCatalogEntry(throttled, &ctx), Status::OK) << ctx.message;

    ASSERT_EQ(catalog_->consumeMfaRecoveryCode(account_id,
                                               throttled.code_hash,
                                               true,
                                               consumed,
                                               &ctx),
              Status::OK)
        << ctx.message;

    EXPECT_EQ(catalog_->consumeMfaRecoveryCode(account_id,
                                               throttled.code_hash,
                                               true,
                                               consumed,
                                               &ctx),
              Status::PERMISSION_DENIED);
    EXPECT_EQ(ctx.vnext_code, "SEC_1224");
}
