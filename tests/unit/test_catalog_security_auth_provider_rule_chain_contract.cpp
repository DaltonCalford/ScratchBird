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

class CatalogSecurityAuthProviderRuleChainContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_security_auth_provider_rule_chain_contract_" +
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

TEST_F(CatalogSecurityAuthProviderRuleChainContractTest, ProviderChainEvaluatesDeterministically)
{
    ErrorContext ctx;

    CatalogManager::AuthProviderCatalogInfo p1{};
    p1.provider_id = generateUuidV7();
    p1.provider_name = "p1_internal_argon2";
    p1.provider_kind = CatalogManager::AuthProviderKind::INTERNAL_ARGON2ID;
    p1.provider_state = CatalogManager::AuthProviderState::ENABLED;
    p1.priority_rank = 1;
    p1.fail_mode = CatalogManager::AuthProviderFailMode::TRY_NEXT;
    ASSERT_EQ(catalog_->upsertAuthProviderCatalogEntry(p1, &ctx), Status::OK) << ctx.message;

    CatalogManager::AuthProviderCatalogInfo p2{};
    p2.provider_id = generateUuidV7();
    p2.provider_name = "p2_internal_scram";
    p2.provider_kind = CatalogManager::AuthProviderKind::INTERNAL_SCRAM_SHA256;
    p2.provider_state = CatalogManager::AuthProviderState::ENABLED;
    p2.priority_rank = 2;
    p2.fail_mode = CatalogManager::AuthProviderFailMode::TRY_NEXT;
    ASSERT_EQ(catalog_->upsertAuthProviderCatalogEntry(p2, &ctx), Status::OK) << ctx.message;

    CatalogManager::AuthPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.policy_name = "auth_chain_policy";
    policy.provider_chain = {p1.provider_id, p2.provider_id};
    policy.lockout_threshold = 5;
    policy.lockout_window_ms = 60000;
    policy.lockout_duration_ms = 60000;
    policy.allow_password_fallback = true;
    ASSERT_EQ(catalog_->upsertAuthPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::PrincipalAccountCatalogInfo account{};
    account.account_id = generateUuidV7();
    account.principal_name = "runtime_alice";
    account.principal_kind = CatalogManager::PrincipalKind::USER;
    account.source_scope_kind = CatalogManager::SourceScopeKind::ANY;
    account.auth_policy_id = policy.policy_id;
    ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(account, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::AuthProviderRuntimeRequest request{};
    request.account_id = account.account_id;
    request.connection_id = generateUuidV7();
    request.credential_kinds = {
        CatalogManager::CredentialKind::PASSWORD_ARGON2ID,
        CatalogManager::CredentialKind::PASSWORD_SCRAM_SHA256};
    request.client_capabilities = {
        CatalogManager::AuthProviderKind::INTERNAL_ARGON2ID,
        CatalogManager::AuthProviderKind::INTERNAL_SCRAM_SHA256};
    request.adapter_results = {
        {p1.provider_id, CatalogManager::AuthAdapterOutcome::REJECT},
        {p2.provider_id, CatalogManager::AuthAdapterOutcome::ACCEPT}};
    request.mfa_completed = true;

    CatalogManager::AuthProviderRuntimeDecision decision{};
    ASSERT_EQ(catalog_->evaluateAuthProviderRuntime(request, decision, &ctx), Status::OK)
        << ctx.message;
    EXPECT_TRUE(decision.success);
    EXPECT_EQ(decision.selected_provider_id, p2.provider_id);
    EXPECT_EQ(decision.attempted_provider_ids.size(), 2u);

    std::vector<CatalogManager::AuthAttemptLogCatalogInfo> attempts;
    ASSERT_EQ(catalog_->listAuthAttemptLogCatalogEntries(account.account_id, attempts, &ctx), Status::OK)
        << ctx.message;
    ASSERT_FALSE(attempts.empty());
    EXPECT_EQ(attempts.back().outcome, CatalogManager::AuthAttemptOutcome::SUCCESS);
}

TEST_F(CatalogSecurityAuthProviderRuleChainContractTest, ProviderValidationAndLockoutContracts)
{
    ErrorContext ctx;

    CatalogManager::AuthProviderCatalogInfo invalid_ldap{};
    invalid_ldap.provider_id = generateUuidV7();
    invalid_ldap.provider_name = "ldap_invalid_missing_keys";
    invalid_ldap.provider_kind = CatalogManager::AuthProviderKind::LDAP_SIMPLE_BIND;
    invalid_ldap.provider_state = CatalogManager::AuthProviderState::ENABLED;
    invalid_ldap.config_payload = "server_uri=ldap://example";
    EXPECT_EQ(catalog_->upsertAuthProviderCatalogEntry(invalid_ldap, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.vnext_code, "SEC_1216");

    CatalogManager::AuthProviderCatalogInfo invalid_ldap_plain{};
    invalid_ldap_plain.provider_id = generateUuidV7();
    invalid_ldap_plain.provider_name = "ldap_plaintext";
    invalid_ldap_plain.provider_kind = CatalogManager::AuthProviderKind::LDAP_SIMPLE_BIND;
    invalid_ldap_plain.provider_state = CatalogManager::AuthProviderState::ENABLED;
    invalid_ldap_plain.config_payload =
        "server_uri=ldap://example;bind_dn_template=uid={user};tls_mode=PLAINTEXT";
    EXPECT_EQ(catalog_->upsertAuthProviderCatalogEntry(invalid_ldap_plain, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.vnext_code, "SEC_1217");

    CatalogManager::AuthProviderCatalogInfo provider{};
    provider.provider_id = generateUuidV7();
    provider.provider_name = "lockout_provider";
    provider.provider_kind = CatalogManager::AuthProviderKind::INTERNAL_ARGON2ID;
    provider.provider_state = CatalogManager::AuthProviderState::ENABLED;
    provider.fail_mode = CatalogManager::AuthProviderFailMode::TRY_NEXT;
    ASSERT_EQ(catalog_->upsertAuthProviderCatalogEntry(provider, &ctx), Status::OK) << ctx.message;

    CatalogManager::AuthPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.policy_name = "lockout_policy";
    policy.provider_chain = {provider.provider_id};
    policy.lockout_threshold = 1;
    policy.lockout_window_ms = 120000;
    policy.lockout_duration_ms = 120000;
    policy.allow_password_fallback = false;
    ASSERT_EQ(catalog_->upsertAuthPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::PrincipalAccountCatalogInfo account{};
    account.account_id = generateUuidV7();
    account.principal_name = "runtime_bob";
    account.principal_kind = CatalogManager::PrincipalKind::USER;
    account.source_scope_kind = CatalogManager::SourceScopeKind::ANY;
    account.auth_policy_id = policy.policy_id;
    ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(account, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::AuthProviderRuntimeRequest request{};
    request.account_id = account.account_id;
    request.connection_id = generateUuidV7();
    request.credential_kinds = {CatalogManager::CredentialKind::PASSWORD_ARGON2ID};
    request.client_capabilities = {CatalogManager::AuthProviderKind::INTERNAL_ARGON2ID};
    request.adapter_results = {{provider.provider_id, CatalogManager::AuthAdapterOutcome::REJECT}};

    CatalogManager::AuthProviderRuntimeDecision decision{};
    EXPECT_EQ(catalog_->evaluateAuthProviderRuntime(request, decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1213");

    CatalogManager::PrincipalAccountCatalogInfo loaded{};
    ASSERT_EQ(catalog_->getPrincipalAccountCatalogEntry(account.account_id, loaded, &ctx), Status::OK)
        << ctx.message;
    EXPECT_TRUE(loaded.is_locked);

    EXPECT_EQ(catalog_->evaluateAuthProviderRuntime(request, decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1215");
}

TEST_F(CatalogSecurityAuthProviderRuleChainContractTest, ConnectionRuleChainDecisionAndIntegrity)
{
    ErrorContext ctx;

    CatalogManager::ConnectionRuleCatalogInfo allow{};
    allow.rule_id = generateUuidV7();
    allow.profile_scope = "native";
    allow.rule_order = 1;
    allow.transport_kind = CatalogManager::ConnectionRuleTransportKind::TLS;
    allow.has_source_host_pattern = true;
    allow.source_host_pattern = "*.example.com";
    allow.has_required_tls_mode = true;
    allow.required_tls_mode = CatalogManager::ConnectionRuleTlsMode::TLS;
    allow.action = CatalogManager::ConnectionRuleAction::ALLOW;
    allow.has_required_provider_kind = true;
    allow.required_provider_kind = CatalogManager::AuthProviderKind::INTERNAL_ARGON2ID;
    ASSERT_EQ(catalog_->upsertConnectionRuleCatalogEntry(allow, &ctx), Status::OK) << ctx.message;

    CatalogManager::ConnectionRuleCatalogInfo deny{};
    deny.rule_id = generateUuidV7();
    deny.profile_scope = "native";
    deny.rule_order = 2;
    deny.transport_kind = CatalogManager::ConnectionRuleTransportKind::TLS;
    deny.action = CatalogManager::ConnectionRuleAction::DENY;
    ASSERT_EQ(catalog_->upsertConnectionRuleCatalogEntry(deny, &ctx), Status::OK) << ctx.message;

    CatalogManager::ConnectionRuleEvaluationRequest request{};
    request.profile_scope = "native";
    request.transport_kind = CatalogManager::ConnectionRuleTransportKind::TLS;
    request.source_host = "db1.example.com";
    request.remote_address = "10.0.0.10";
    request.source_ip = "10.0.0.10";
    request.has_provider_kind = true;
    request.provider_kind = CatalogManager::AuthProviderKind::INTERNAL_ARGON2ID;

    CatalogManager::ConnectionRuleEvaluationDecision decision{};
    ASSERT_EQ(catalog_->evaluateConnectionRuleChain(request, decision, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(decision.matched);
    EXPECT_EQ(decision.matched_rule_id, allow.rule_id);
    EXPECT_EQ(decision.action, CatalogManager::ConnectionRuleAction::ALLOW);

    request.has_provider_kind = false;
    EXPECT_EQ(catalog_->evaluateConnectionRuleChain(request, decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1232");

    request.has_provider_kind = true;
    request.provider_kind = CatalogManager::AuthProviderKind::INTERNAL_ARGON2ID;
    request.source_ip = "10.0.0.11";
    EXPECT_EQ(catalog_->evaluateConnectionRuleChain(request, decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1233");
}

TEST_F(CatalogSecurityAuthProviderRuleChainContractTest, ConnectionRuleValidationAndEpochGuards)
{
    ErrorContext ctx;

    CatalogManager::ConnectionRuleCatalogInfo base{};
    base.rule_id = generateUuidV7();
    base.profile_scope = "epoch_profile";
    base.rule_order = 1;
    base.transport_kind = CatalogManager::ConnectionRuleTransportKind::TCP;
    base.action = CatalogManager::ConnectionRuleAction::DENY;
    ASSERT_EQ(catalog_->upsertConnectionRuleCatalogEntry(base, &ctx), Status::OK) << ctx.message;

    CatalogManager::ConnectionRuleEpochCatalogInfo epoch{};
    ASSERT_EQ(catalog_->getConnectionRuleEpochCatalogEntry("epoch_profile", epoch, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::ConnectionRuleCatalogInfo wrong_epoch = base;
    wrong_epoch.has_expected_epoch = true;
    wrong_epoch.expected_epoch_u64 = epoch.rule_epoch_u64 + 100;
    wrong_epoch.has_source_host_pattern = true;
    wrong_epoch.source_host_pattern = "*.example.com";
    EXPECT_EQ(catalog_->upsertConnectionRuleCatalogEntry(wrong_epoch, &ctx),
              Status::CONSTRAINT_VIOLATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1237");

    CatalogManager::ConnectionRuleCatalogInfo duplicate{};
    duplicate.rule_id = generateUuidV7();
    duplicate.profile_scope = "epoch_profile";
    duplicate.rule_order = 1;
    duplicate.transport_kind = CatalogManager::ConnectionRuleTransportKind::TCP;
    duplicate.action = CatalogManager::ConnectionRuleAction::DENY;
    EXPECT_EQ(catalog_->upsertConnectionRuleCatalogEntry(duplicate, &ctx),
              Status::CONSTRAINT_VIOLATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1234");

    CatalogManager::ConnectionRuleCatalogInfo invalid_cidr{};
    invalid_cidr.rule_id = generateUuidV7();
    invalid_cidr.profile_scope = "cidr_profile";
    invalid_cidr.rule_order = 1;
    invalid_cidr.transport_kind = CatalogManager::ConnectionRuleTransportKind::TCP;
    invalid_cidr.action = CatalogManager::ConnectionRuleAction::DENY;
    invalid_cidr.has_source_cidr = true;
    invalid_cidr.source_cidr = "10.0.0.0/99";
    EXPECT_EQ(catalog_->upsertConnectionRuleCatalogEntry(invalid_cidr, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.vnext_code, "SEC_1235");

    CatalogManager::ConnectionRuleCatalogInfo no_deny{};
    no_deny.rule_id = generateUuidV7();
    no_deny.profile_scope = "no_deny_profile";
    no_deny.rule_order = 1;
    no_deny.transport_kind = CatalogManager::ConnectionRuleTransportKind::TCP;
    no_deny.action = CatalogManager::ConnectionRuleAction::ALLOW;
    ASSERT_EQ(catalog_->upsertConnectionRuleCatalogEntry(no_deny, &ctx), Status::OK) << ctx.message;

    CatalogManager::ConnectionRuleEvaluationRequest req{};
    req.profile_scope = "no_deny_profile";
    req.transport_kind = CatalogManager::ConnectionRuleTransportKind::TCP;
    req.source_ip = "127.0.0.1";
    req.remote_address = "127.0.0.1";

    CatalogManager::ConnectionRuleEvaluationDecision decision{};
    EXPECT_EQ(catalog_->evaluateConnectionRuleChain(req, decision, &ctx),
              Status::CONSTRAINT_VIOLATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1236");
}
