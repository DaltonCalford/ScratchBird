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
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

namespace
{
auto metricCounterValue(const std::string& metric_name,
                        const std::vector<std::string>& labels) -> double
{
    auto* metric = MetricsRegistry::getInstance().get(metric_name);
    if (metric == nullptr)
    {
        return 0.0;
    }
    auto* counter = dynamic_cast<Counter*>(metric);
    if (counter == nullptr)
    {
        return 0.0;
    }
    return counter->get(labels);
}
} // namespace

class CatalogSecurityAclAbacGraphTokenQuotaSettingsContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_security_acl_abac_graph_token_quota_settings_contract_" +
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

TEST_F(CatalogSecurityAclAbacGraphTokenQuotaSettingsContractTest, AclContractsDenyPrecedenceAndAllow)
{
    ErrorContext ctx;
    const ID subject_id = generateUuidV7();
    const std::string metric = "scratchbird_vnext_security_events_total";
    const double allow_before =
        metricCounterValue(metric, {"acl_command_policy", "allow", "NONE"});
    const double reject_1240_before =
        metricCounterValue(metric, {"acl_command_policy", "reject", "SEC_1240"});
    const double reject_1241_before =
        metricCounterValue(metric, {"acl_command_policy", "reject", "SEC_1241"});

    CatalogManager::AclRuleCatalogInfo allow_rule{};
    allow_rule.acl_rule_id = generateUuidV7();
    allow_rule.subject_id = subject_id;
    allow_rule.subject_type = CatalogManager::AuthorizationSubjectType::USER;
    allow_rule.effect = CatalogManager::PolicyEffect::ALLOW;
    allow_rule.command_pattern = "GET";
    allow_rule.priority_u16 = 10;
    ASSERT_EQ(catalog_->upsertAclRuleCatalogEntry(allow_rule, &ctx), Status::OK) << ctx.message;

    CatalogManager::AclRuleCatalogInfo deny_rule{};
    deny_rule.acl_rule_id = generateUuidV7();
    deny_rule.subject_id = subject_id;
    deny_rule.subject_type = CatalogManager::AuthorizationSubjectType::USER;
    deny_rule.effect = CatalogManager::PolicyEffect::DENY;
    deny_rule.command_pattern = "GET";
    deny_rule.has_key_pattern = true;
    deny_rule.key_pattern = "cache:*";
    deny_rule.priority_u16 = 0;
    ASSERT_EQ(catalog_->upsertAclRuleCatalogEntry(deny_rule, &ctx), Status::OK) << ctx.message;

    CatalogManager::AclEvaluationRequest request{};
    request.command_name = "GET";
    request.keys = {"cache:item"};
    request.effective_subjects = {
        {subject_id, CatalogManager::AuthorizationSubjectType::USER}
    };

    CatalogManager::AclEvaluationDecision decision{};
    EXPECT_EQ(catalog_->evaluateAclCommandPolicy(request, decision, &ctx), Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1240");

    ASSERT_EQ(catalog_->deleteAclRuleCatalogEntry(deny_rule.acl_rule_id, &ctx), Status::OK) << ctx.message;

    EXPECT_EQ(catalog_->evaluateAclCommandPolicy(request, decision, &ctx), Status::OK);
    EXPECT_TRUE(decision.allowed);
    EXPECT_EQ(decision.matched_rule_id, allow_rule.acl_rule_id);

    request.command_name = "SET";
    EXPECT_EQ(catalog_->evaluateAclCommandPolicy(request, decision, &ctx), Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1241");

    EXPECT_EQ(allow_before + 1.0,
              metricCounterValue(metric, {"acl_command_policy", "allow", "NONE"}));
    EXPECT_EQ(reject_1240_before + 1.0,
              metricCounterValue(metric, {"acl_command_policy", "reject", "SEC_1240"}));
    EXPECT_EQ(reject_1241_before + 1.0,
              metricCounterValue(metric, {"acl_command_policy", "reject", "SEC_1241"}));
}

TEST_F(CatalogSecurityAclAbacGraphTokenQuotaSettingsContractTest, DocumentContractsReadWriteAndMissingModelPolicy)
{
    ErrorContext ctx;

    CatalogManager::DocumentPolicyCatalogInfo allow_policy{};
    allow_policy.policy_id = generateUuidV7();
    allow_policy.engine_tag = CatalogManager::DocumentEngineTag::MONGODB;
    allow_policy.resource_pattern = "docs_*";
    allow_policy.effect = CatalogManager::PolicyEffect::ALLOW;
    allow_policy.field_allowlist = {"id", "name"};
    allow_policy.field_denylist = {"name"};
    allow_policy.priority_u16 = 10;
    ASSERT_EQ(catalog_->upsertDocumentPolicyCatalogEntry(allow_policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::DocumentPolicyCatalogInfo deny_write_policy{};
    deny_write_policy.policy_id = generateUuidV7();
    deny_write_policy.engine_tag = CatalogManager::DocumentEngineTag::MONGODB;
    deny_write_policy.resource_pattern = "docs_*";
    deny_write_policy.effect = CatalogManager::PolicyEffect::DENY;
    deny_write_policy.has_doc_filter_sblr_uuid = true;
    deny_write_policy.doc_filter_sblr_uuid = generateUuidV7();
    deny_write_policy.priority_u16 = 0;
    ASSERT_EQ(catalog_->upsertDocumentPolicyCatalogEntry(deny_write_policy, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::DocumentAuthorizationRequest read_request{};
    read_request.engine_tag = CatalogManager::DocumentEngineTag::MONGODB;
    read_request.resource_name = "docs_users";
    read_request.candidate_fields = {"id", "name", "age"};
    read_request.require_non_empty_document = true;

    CatalogManager::DocumentAuthorizationDecision read_decision{};
    ASSERT_EQ(catalog_->evaluateDocumentReadPolicy(read_request, read_decision, &ctx), Status::OK)
        << ctx.message;
    ASSERT_TRUE(read_decision.allowed);
    ASSERT_EQ(read_decision.projected_fields.size(), 1u);
    EXPECT_EQ(read_decision.projected_fields.front(), "id");

    CatalogManager::DocumentAuthorizationRequest write_request = read_request;
    write_request.is_write = true;
    CatalogManager::DocumentAuthorizationDecision write_decision{};
    EXPECT_EQ(catalog_->evaluateDocumentWritePolicy(write_request, write_decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1243");

    CatalogManager::DocumentAuthorizationRequest missing_model{};
    missing_model.engine_tag = CatalogManager::DocumentEngineTag::OPENSEARCH;
    missing_model.resource_name = "idx_users";
    missing_model.require_model_policy = true;
    CatalogManager::DocumentAuthorizationDecision missing_decision{};
    EXPECT_EQ(catalog_->evaluateDocumentReadPolicy(missing_model, missing_decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1247");
}

TEST_F(CatalogSecurityAclAbacGraphTokenQuotaSettingsContractTest, GraphContractsValidateActionBitsAndDenyPrecedence)
{
    ErrorContext ctx;
    const ID subject_id = generateUuidV7();
    const std::string metric = "scratchbird_vnext_security_events_total";
    const double allow_before =
        metricCounterValue(metric, {"graph_privilege_policy", "allow", "NONE"});
    const double reject_1244_before =
        metricCounterValue(metric, {"graph_privilege_policy", "reject", "SEC_1244"});
    const double reject_1248_before =
        metricCounterValue(metric, {"graph_privilege_policy", "reject", "SEC_1248"});

    CatalogManager::GraphPrivilegeCatalogInfo allow{};
    allow.graph_priv_id = generateUuidV7();
    allow.subject_id = subject_id;
    allow.subject_type = CatalogManager::AuthorizationSubjectType::USER;
    allow.graph_scope = "graph_*";
    allow.action_bits = (1ULL << 0) | (1ULL << 1);
    allow.effect = CatalogManager::PolicyEffect::ALLOW;
    allow.priority_u16 = 10;
    ASSERT_EQ(catalog_->upsertGraphPrivilegeCatalogEntry(allow, &ctx), Status::OK) << ctx.message;

    CatalogManager::GraphAuthorizationRequest request{};
    request.graph_scope = "graph_sales";
    request.requested_action_bits = (1ULL << 0);
    request.effective_subjects = {
        {subject_id, CatalogManager::AuthorizationSubjectType::USER}
    };

    CatalogManager::GraphAuthorizationDecision decision{};
    ASSERT_EQ(catalog_->evaluateGraphPrivilegePolicy(request, decision, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(decision.allowed);

    CatalogManager::GraphPrivilegeCatalogInfo deny{};
    deny.graph_priv_id = generateUuidV7();
    deny.subject_id = subject_id;
    deny.subject_type = CatalogManager::AuthorizationSubjectType::USER;
    deny.graph_scope = "graph_*";
    deny.action_bits = (1ULL << 0);
    deny.effect = CatalogManager::PolicyEffect::DENY;
    deny.priority_u16 = 0;
    ASSERT_EQ(catalog_->upsertGraphPrivilegeCatalogEntry(deny, &ctx), Status::OK) << ctx.message;

    EXPECT_EQ(catalog_->evaluateGraphPrivilegePolicy(request, decision, &ctx), Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1244");

    request.requested_action_bits = (1ULL << 20);
    EXPECT_EQ(catalog_->evaluateGraphPrivilegePolicy(request, decision, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.vnext_code, "SEC_1248");

    EXPECT_EQ(allow_before + 1.0,
              metricCounterValue(metric, {"graph_privilege_policy", "allow", "NONE"}));
    EXPECT_EQ(reject_1244_before + 1.0,
              metricCounterValue(metric, {"graph_privilege_policy", "reject", "SEC_1244"}));
    EXPECT_EQ(reject_1248_before + 1.0,
              metricCounterValue(metric, {"graph_privilege_policy", "reject", "SEC_1248"}));
}

TEST_F(CatalogSecurityAclAbacGraphTokenQuotaSettingsContractTest, TokenQuotaAndSettingsContracts)
{
    ErrorContext ctx;
    const std::string metric = "scratchbird_vnext_security_events_total";
    const double token_allow_before =
        metricCounterValue(metric, {"token_scope_validation", "allow", "NONE"});
    const double token_reject_1253_before =
        metricCounterValue(metric, {"token_scope_validation", "reject", "SEC_1253"});
    const double token_reject_1251_before =
        metricCounterValue(metric, {"token_scope_validation", "reject", "SEC_1251"});
    const double quota_reject_1258_before =
        metricCounterValue(metric, {"quota_policy", "reject", "SEC_1258"});
    const double quota_reject_1254_before =
        metricCounterValue(metric, {"quota_policy", "reject", "SEC_1254"});
    const double settings_reject_1258_before =
        metricCounterValue(metric, {"settings_policy", "reject", "SEC_1258"});
    const double settings_allow_before =
        metricCounterValue(metric, {"settings_policy", "allow", "NONE"});
    const double settings_reject_1255_before =
        metricCounterValue(metric, {"settings_policy", "reject", "SEC_1255"});

    CatalogManager::TokenCatalogInfo token{};
    token.token_id = generateUuidV7();
    token.token_kind = CatalogManager::TokenKind::BEARER;
    token.token_hash.fill(0x11);
    token.issuer = "test_issuer";
    token.subject_account_id = generateUuidV7();
    token.scope_model = CatalogManager::TokenScopeModel::GENERIC;
    token.not_before_utc = 100;
    token.not_after_utc = 1000;
    ASSERT_EQ(catalog_->upsertTokenCatalogEntry(token, &ctx), Status::OK) << ctx.message;

    CatalogManager::TokenScopeEntryCatalogInfo allow_scope{};
    allow_scope.scope_id = generateUuidV7();
    allow_scope.token_id = token.token_id;
    allow_scope.effect = CatalogManager::PolicyEffect::ALLOW;
    allow_scope.resource_kind = CatalogManager::TokenResourceKind::DATABASE;
    allow_scope.resource_pattern = "db_*";
    allow_scope.action_bits = 0x3;
    allow_scope.priority_u16 = 10;
    ASSERT_EQ(catalog_->upsertTokenScopeEntryCatalogEntry(allow_scope, &ctx), Status::OK) << ctx.message;

    CatalogManager::TokenValidationRequest token_request{};
    token_request.presented_token_hash = token.token_hash;
    token_request.resource_kind = CatalogManager::TokenResourceKind::DATABASE;
    token_request.resource_name = "db_main";
    token_request.requested_action_bits = 0x1;
    token_request.now_utc = 500;

    CatalogManager::TokenValidationDecision token_decision{};
    ASSERT_EQ(catalog_->validateTokenScope(token_request, token_decision, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(token_decision.allowed);
    EXPECT_EQ(token_decision.token_id, token.token_id);

    CatalogManager::TokenScopeEntryCatalogInfo deny_scope{};
    deny_scope.scope_id = generateUuidV7();
    deny_scope.token_id = token.token_id;
    deny_scope.effect = CatalogManager::PolicyEffect::DENY;
    deny_scope.resource_kind = CatalogManager::TokenResourceKind::DATABASE;
    deny_scope.resource_pattern = "db_*";
    deny_scope.action_bits = 0x1;
    deny_scope.priority_u16 = 0;
    ASSERT_EQ(catalog_->upsertTokenScopeEntryCatalogEntry(deny_scope, &ctx), Status::OK) << ctx.message;

    EXPECT_EQ(catalog_->validateTokenScope(token_request, token_decision, &ctx), Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1253");

    ASSERT_EQ(catalog_->deleteTokenScopeEntryCatalogEntry(deny_scope.scope_id, &ctx), Status::OK) << ctx.message;

    CatalogManager::TokenCatalogInfo loaded_token{};
    ASSERT_EQ(catalog_->getTokenCatalogEntry(token.token_id, loaded_token, &ctx), Status::OK) << ctx.message;
    loaded_token.has_revoked_time_utc = true;
    loaded_token.revoked_time_utc = 700;
    ASSERT_EQ(catalog_->upsertTokenCatalogEntry(loaded_token, &ctx), Status::OK) << ctx.message;

    EXPECT_EQ(catalog_->validateTokenScope(token_request, token_decision, &ctx), Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1251");

    CatalogManager::QuotaEvaluationRequest quota_request{};
    quota_request.require_profile = true;
    quota_request.concurrent_requests = 3;
    CatalogManager::QuotaEvaluationDecision quota_decision{};
    EXPECT_EQ(catalog_->evaluateQuotaPolicy(quota_request, quota_decision, &ctx), Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1258");

    CatalogManager::QuotaProfileCatalogInfo quota_profile{};
    quota_profile.quota_profile_id = generateUuidV7();
    quota_profile.profile_name = "q_standard";
    quota_profile.max_concurrent_requests = 1;
    quota_profile.window_ms = 1000;
    ASSERT_EQ(catalog_->upsertQuotaProfileCatalogEntry(quota_profile, &ctx), Status::OK) << ctx.message;

    CatalogManager::QuotaBindingCatalogInfo quota_binding{};
    quota_binding.binding_id = generateUuidV7();
    quota_binding.subject_type = CatalogManager::BindingSubjectType::GLOBAL;
    quota_binding.has_subject_id = false;
    quota_binding.resource_scope_kind = CatalogManager::BindingResourceScopeKind::GLOBAL;
    quota_binding.has_resource_scope_value = false;
    quota_binding.quota_profile_id = quota_profile.quota_profile_id;
    ASSERT_EQ(catalog_->upsertQuotaBindingCatalogEntry(quota_binding, &ctx), Status::OK) << ctx.message;

    EXPECT_EQ(catalog_->evaluateQuotaPolicy(quota_request, quota_decision, &ctx), Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1254");

    CatalogManager::SettingsResolutionRequest settings_request{};
    settings_request.require_profile = true;
    settings_request.session_overrides = {{"allowed", "off"}};
    CatalogManager::SettingsResolutionDecision settings_decision{};
    EXPECT_EQ(catalog_->resolveSettingsPolicy(settings_request, settings_decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1258");

    CatalogManager::SettingsProfileCatalogInfo settings_profile{};
    settings_profile.settings_profile_id = generateUuidV7();
    settings_profile.profile_name = "s_strict";
    settings_profile.settings_payload = "allowed=on";
    settings_profile.strict_mode = true;
    ASSERT_EQ(catalog_->upsertSettingsProfileCatalogEntry(settings_profile, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::SettingsBindingCatalogInfo settings_binding{};
    settings_binding.binding_id = generateUuidV7();
    settings_binding.subject_type = CatalogManager::BindingSubjectType::GLOBAL;
    settings_binding.has_subject_id = false;
    settings_binding.resource_scope_kind = CatalogManager::BindingResourceScopeKind::GLOBAL;
    settings_binding.has_resource_scope_value = false;
    settings_binding.settings_profile_id = settings_profile.settings_profile_id;
    ASSERT_EQ(catalog_->upsertSettingsBindingCatalogEntry(settings_binding, &ctx), Status::OK)
        << ctx.message;

    ASSERT_EQ(catalog_->resolveSettingsPolicy(settings_request, settings_decision, &ctx), Status::OK)
        << ctx.message;
    ASSERT_TRUE(settings_decision.applied);
    EXPECT_EQ(settings_decision.merged_settings["allowed"], "off");

    settings_request.session_overrides = {{"unknown", "1"}};
    EXPECT_EQ(catalog_->resolveSettingsPolicy(settings_request, settings_decision, &ctx),
              Status::INVALID_AUTHORIZATION);
    EXPECT_EQ(ctx.vnext_code, "SEC_1255");

    EXPECT_EQ(token_allow_before + 1.0,
              metricCounterValue(metric, {"token_scope_validation", "allow", "NONE"}));
    EXPECT_EQ(token_reject_1253_before + 1.0,
              metricCounterValue(metric, {"token_scope_validation", "reject", "SEC_1253"}));
    EXPECT_EQ(token_reject_1251_before + 1.0,
              metricCounterValue(metric, {"token_scope_validation", "reject", "SEC_1251"}));
    EXPECT_EQ(quota_reject_1258_before + 1.0,
              metricCounterValue(metric, {"quota_policy", "reject", "SEC_1258"}));
    EXPECT_EQ(quota_reject_1254_before + 1.0,
              metricCounterValue(metric, {"quota_policy", "reject", "SEC_1254"}));
    EXPECT_EQ(settings_reject_1258_before + 1.0,
              metricCounterValue(metric, {"settings_policy", "reject", "SEC_1258"}));
    EXPECT_EQ(settings_allow_before + 1.0,
              metricCounterValue(metric, {"settings_policy", "allow", "NONE"}));
    EXPECT_EQ(settings_reject_1255_before + 1.0,
              metricCounterValue(metric, {"settings_policy", "reject", "SEC_1255"}));
}
