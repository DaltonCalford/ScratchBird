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

class CatalogSecurityIdentityAccountContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_security_identity_account_contract_" +
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

TEST_F(CatalogSecurityIdentityAccountContractTest, PrincipalTupleResolutionDeterministicRanking)
{
    ErrorContext ctx;

    CatalogManager::PrincipalAccountCatalogInfo any_account{};
    any_account.account_id = generateUuidV7();
    any_account.principal_name = "alice";
    any_account.principal_kind = CatalogManager::PrincipalKind::USER;
    any_account.source_scope_kind = CatalogManager::SourceScopeKind::ANY;
    any_account.auth_policy_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(any_account, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::PrincipalAccountCatalogInfo wildcard_account{};
    wildcard_account.account_id = generateUuidV7();
    wildcard_account.principal_name = "alice";
    wildcard_account.principal_kind = CatalogManager::PrincipalKind::USER;
    wildcard_account.source_scope_kind = CatalogManager::SourceScopeKind::HOST_WILDCARD;
    wildcard_account.has_source_scope_value = true;
    wildcard_account.source_scope_value = "*.example.com";
    wildcard_account.auth_policy_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(wildcard_account, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::PrincipalAccountCatalogInfo exact_account{};
    exact_account.account_id = generateUuidV7();
    exact_account.principal_name = "alice";
    exact_account.principal_kind = CatalogManager::PrincipalKind::USER;
    exact_account.source_scope_kind = CatalogManager::SourceScopeKind::HOST_EXACT;
    exact_account.has_source_scope_value = true;
    exact_account.source_scope_value = "db1.example.com";
    exact_account.has_auth_database = true;
    exact_account.auth_database = "sales";
    exact_account.has_tenant_scope = true;
    exact_account.tenant_scope = "tenant_a";
    exact_account.auth_policy_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(exact_account, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::PrincipalResolutionRequest req_exact{};
    req_exact.presented_principal_name = "alice";
    req_exact.source_host = "db1.example.com";
    req_exact.has_auth_database_context = true;
    req_exact.auth_database_context = "sales";
    req_exact.has_tenant_context = true;
    req_exact.tenant_context = "tenant_a";

    CatalogManager::PrincipalAccountCatalogInfo resolved{};
    ASSERT_EQ(catalog_->resolvePrincipalAccount(req_exact, resolved, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(resolved.account_id, exact_account.account_id);

    CatalogManager::PrincipalResolutionRequest req_wildcard{};
    req_wildcard.presented_principal_name = "alice";
    req_wildcard.source_host = "api.example.com";

    ASSERT_EQ(catalog_->resolvePrincipalAccount(req_wildcard, resolved, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(resolved.account_id, wildcard_account.account_id);

    CatalogManager::PrincipalResolutionRequest req_any{};
    req_any.presented_principal_name = "alice";
    req_any.source_host = "unmatched.invalid";

    ASSERT_EQ(catalog_->resolvePrincipalAccount(req_any, resolved, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(resolved.account_id, any_account.account_id);

    CatalogManager::PrincipalResolutionRequest req_missing{};
    req_missing.presented_principal_name = "missing";
    req_missing.source_host = "db1.example.com";

    EXPECT_EQ(catalog_->resolvePrincipalAccount(req_missing, resolved, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1201");
}

TEST_F(CatalogSecurityIdentityAccountContractTest, RejectsInvalidCidrAndDuplicateTuple)
{
    ErrorContext ctx;

    CatalogManager::PrincipalAccountCatalogInfo invalid_cidr{};
    invalid_cidr.account_id = generateUuidV7();
    invalid_cidr.principal_name = "cidr_user";
    invalid_cidr.principal_kind = CatalogManager::PrincipalKind::USER;
    invalid_cidr.source_scope_kind = CatalogManager::SourceScopeKind::CIDR;
    invalid_cidr.has_source_scope_value = true;
    invalid_cidr.source_scope_value = "10.0.0.0/99";
    invalid_cidr.auth_policy_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(invalid_cidr, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.vnext_code, "SEC_1203");

    CatalogManager::PrincipalAccountCatalogInfo base{};
    base.account_id = generateUuidV7();
    base.principal_name = "dup_user";
    base.principal_kind = CatalogManager::PrincipalKind::USER;
    base.source_scope_kind = CatalogManager::SourceScopeKind::HOST_EXACT;
    base.has_source_scope_value = true;
    base.source_scope_value = "db1.example.com";
    base.has_auth_database = true;
    base.auth_database = "app";
    base.has_tenant_scope = true;
    base.tenant_scope = "tenant_x";
    base.auth_policy_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(base, &ctx), Status::OK) << ctx.message;

    CatalogManager::PrincipalAccountCatalogInfo duplicate = base;
    duplicate.account_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(duplicate, &ctx),
              Status::CONSTRAINT_VIOLATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1205");
}

TEST_F(CatalogSecurityIdentityAccountContractTest, AmbiguousMatchIsRejected)
{
    ErrorContext ctx;

    CatalogManager::PrincipalAccountCatalogInfo a{};
    a.account_id = generateUuidV7();
    a.principal_name = "bob";
    a.principal_kind = CatalogManager::PrincipalKind::USER;
    a.source_scope_kind = CatalogManager::SourceScopeKind::HOST_WILDCARD;
    a.has_source_scope_value = true;
    a.source_scope_value = "*.example.com";
    a.auth_policy_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(a, &ctx), Status::OK) << ctx.message;

    CatalogManager::PrincipalAccountCatalogInfo b{};
    b.account_id = generateUuidV7();
    b.principal_name = "bob";
    b.principal_kind = CatalogManager::PrincipalKind::USER;
    b.source_scope_kind = CatalogManager::SourceScopeKind::HOST_WILDCARD;
    b.has_source_scope_value = true;
    b.source_scope_value = "db*.example.com";
    b.auth_policy_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(b, &ctx), Status::OK) << ctx.message;

    CatalogManager::PrincipalResolutionRequest req{};
    req.presented_principal_name = "bob";
    req.source_host = "db1.example.com";

    CatalogManager::PrincipalAccountCatalogInfo resolved{};
    EXPECT_EQ(catalog_->resolvePrincipalAccount(req, resolved, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1202");
}

TEST_F(CatalogSecurityIdentityAccountContractTest, CredentialAndProfileBindingContracts)
{
    ErrorContext ctx;

    CatalogManager::PrincipalAccountCatalogInfo account{};
    account.account_id = generateUuidV7();
    account.principal_name = "carol";
    account.principal_kind = CatalogManager::PrincipalKind::USER;
    account.source_scope_kind = CatalogManager::SourceScopeKind::ANY;
    account.auth_policy_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(account, &ctx), Status::OK) << ctx.message;

    CatalogManager::AccountCredentialCatalogInfo primary{};
    primary.credential_id = generateUuidV7();
    primary.account_id = account.account_id;
    primary.credential_kind = CatalogManager::CredentialKind::PASSWORD_ARGON2ID;
    primary.credential_payload_id = generateUuidV7();
    primary.is_active = true;
    ASSERT_EQ(catalog_->upsertAccountCredentialCatalogEntry(primary, &ctx), Status::OK) << ctx.message;

    CatalogManager::AccountCredentialCatalogInfo second_primary{};
    second_primary.credential_id = generateUuidV7();
    second_primary.account_id = account.account_id;
    second_primary.credential_kind = CatalogManager::CredentialKind::PASSWORD_SCRAM_SHA256;
    second_primary.credential_payload_id = generateUuidV7();
    second_primary.is_active = true;
    EXPECT_EQ(catalog_->upsertAccountCredentialCatalogEntry(second_primary, &ctx),
              Status::CONSTRAINT_VIOLATION);

    second_primary.is_active = false;
    ASSERT_EQ(catalog_->upsertAccountCredentialCatalogEntry(second_primary, &ctx), Status::OK)
        << ctx.message;

    std::vector<CatalogManager::AccountCredentialCatalogInfo> creds;
    ASSERT_EQ(catalog_->listAccountCredentialCatalogEntries(account.account_id, creds, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(creds.size(), 2u);

    CatalogManager::AccountProfileBindingCatalogInfo binding{};
    binding.binding_id = generateUuidV7();
    binding.account_id = account.account_id;
    binding.has_quota_profile = true;
    binding.quota_profile_id = generateUuidV7();
    binding.has_settings_profile = true;
    binding.settings_profile_id = generateUuidV7();
    binding.priority_u16 = 10;
    ASSERT_EQ(catalog_->upsertAccountProfileBindingCatalogEntry(binding, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::AccountProfileBindingCatalogInfo loaded_binding{};
    ASSERT_EQ(catalog_->getAccountProfileBindingCatalogEntry(binding.binding_id, loaded_binding, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(loaded_binding.account_id, binding.account_id);
    EXPECT_EQ(loaded_binding.priority_u16, 10u);

    std::vector<CatalogManager::AccountProfileBindingCatalogInfo> bindings;
    ASSERT_EQ(catalog_->listAccountProfileBindingCatalogEntries(account.account_id, bindings, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(bindings.size(), 1u);
}

