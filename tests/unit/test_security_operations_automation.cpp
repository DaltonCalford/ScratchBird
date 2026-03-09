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

class SecurityOperationsAutomationTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_security_operations_automation_" + std::to_string(getpid()) + ".db";
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

TEST_F(SecurityOperationsAutomationTest, AckOverdueIncidentProducesActionableEscalation)
{
    ErrorContext ctx;

    CatalogManager::AlertRuleCatalogInfo rule{};
    rule.rule_id = generateUuidV7();
    rule.rule_name = "cpu_warning";
    rule.rule_kind = CatalogManager::AlertRuleKind::METRIC;
    rule.severity = CatalogManager::AlertSeverity::WARNING;
    rule.has_condition_text = true;
    rule.condition_text = "cpu > 90";
    rule.throttle_interval_ms = 60000;
    ASSERT_EQ(catalog_->upsertAlertRuleCatalogEntry(rule, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertTargetCatalogInfo target{};
    target.target_id = generateUuidV7();
    target.target_name = "ops_webhook";
    target.target_kind = CatalogManager::AlertTargetKind::WEBHOOK;
    target.endpoint = "https://ops.example/alert";
    ASSERT_EQ(catalog_->upsertAlertTargetCatalogEntry(target, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertRouteCatalogInfo route{};
    route.route_id = generateUuidV7();
    route.rule_id = rule.rule_id;
    route.target_id = target.target_id;
    route.route_kind = CatalogManager::AlertRouteKind::ESCALATION;
    route.severity_min = CatalogManager::AlertSeverity::WARNING;
    route.severity_max = CatalogManager::AlertSeverity::CRITICAL;
    ASSERT_EQ(catalog_->upsertAlertRouteCatalogEntry(route, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertEventCatalogInfo event{};
    event.event_id = generateUuidV7();
    event.rule_id = rule.rule_id;
    event.severity = CatalogManager::AlertSeverity::WARNING;
    event.event_state = CatalogManager::AlertEventState::OPEN;
    event.event_time = 1000;
    ASSERT_EQ(catalog_->upsertAlertEventCatalogEntry(event, &ctx), Status::OK) << ctx.message;

    CatalogManager::SecurityOperationsAutomationRequest request{};
    request.now_time = 1200;
    request.warning_ack_sla_ms = 100;

    CatalogManager::SecurityOperationsAutomationResult result;
    ASSERT_EQ(catalog_->runSecurityOperationsAutomation(request, result, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(1u, result.open_event_count);
    ASSERT_EQ(1u, result.actionable_event_count);
    ASSERT_EQ(0u, result.healing_run_count);
    ASSERT_EQ(1u, result.actions.size());
    EXPECT_TRUE(result.actions.front().ack_overdue);
    EXPECT_FALSE(result.actions.front().remediation_overdue);
    EXPECT_EQ("SECOPS_ACK", result.actions.front().action_code);
    EXPECT_EQ(1u, result.actions.front().route_ids.size());
    EXPECT_EQ(1u, result.actions.front().target_ids.size());
}

TEST_F(SecurityOperationsAutomationTest, VulnerabilitySignalQueuesHealingRunOnce)
{
    ErrorContext ctx;

    CatalogManager::AlertRuleCatalogInfo rule{};
    rule.rule_id = generateUuidV7();
    rule.rule_name = "vuln_openssl";
    rule.rule_kind = CatalogManager::AlertRuleKind::EVENT;
    rule.severity = CatalogManager::AlertSeverity::CRITICAL;
    rule.has_condition_text = true;
    rule.condition_text = "cve backlog";
    rule.throttle_interval_ms = 60000;
    ASSERT_EQ(catalog_->upsertAlertRuleCatalogEntry(rule, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertTargetCatalogInfo target{};
    target.target_id = generateUuidV7();
    target.target_name = "secops_pager";
    target.target_kind = CatalogManager::AlertTargetKind::PAGER;
    target.endpoint = "pager://secops";
    ASSERT_EQ(catalog_->upsertAlertTargetCatalogEntry(target, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertRouteCatalogInfo route{};
    route.route_id = generateUuidV7();
    route.rule_id = rule.rule_id;
    route.target_id = target.target_id;
    route.route_kind = CatalogManager::AlertRouteKind::ESCALATION;
    route.severity_min = CatalogManager::AlertSeverity::CRITICAL;
    route.severity_max = CatalogManager::AlertSeverity::CRITICAL;
    ASSERT_EQ(catalog_->upsertAlertRouteCatalogEntry(route, &ctx), Status::OK) << ctx.message;

    CatalogManager::AlertEventCatalogInfo event{};
    event.event_id = generateUuidV7();
    event.rule_id = rule.rule_id;
    event.severity = CatalogManager::AlertSeverity::CRITICAL;
    event.event_state = CatalogManager::AlertEventState::OPEN;
    event.event_time = 1000;
    ASSERT_EQ(catalog_->upsertAlertEventCatalogEntry(event, &ctx), Status::OK) << ctx.message;

    CatalogManager::HealingPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.policy_name = "manual_vuln_remediation";
    policy.trigger_kind = CatalogManager::HealingTriggerKind::MANUAL;
    policy.min_severity = CatalogManager::AlertSeverity::WARNING;
    ASSERT_EQ(catalog_->upsertHealingPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::HealingActionCatalogInfo action{};
    action.action_id = generateUuidV7();
    action.policy_id = policy.policy_id;
    action.action_kind = CatalogManager::HealingActionKind::NOTIFY;
    action.action_order = 1;
    action.is_blocking = true;
    ASSERT_EQ(catalog_->upsertHealingActionCatalogEntry(action, &ctx), Status::OK) << ctx.message;

    CatalogManager::SecurityOperationsAutomationRequest request{};
    request.now_time = 1200;
    request.critical_ack_sla_ms = 50;
    request.critical_vulnerability_sla_ms = 100;

    CatalogManager::SecurityOperationsAutomationResult first_result;
    ASSERT_EQ(catalog_->runSecurityOperationsAutomation(request, first_result, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(1u, first_result.healing_run_count);
    ASSERT_EQ(1u, first_result.actions.size());
    EXPECT_TRUE(first_result.actions.front().vulnerability_signal);
    EXPECT_TRUE(first_result.actions.front().remediation_overdue);
    EXPECT_TRUE(first_result.actions.front().healing_run_created);

    std::vector<CatalogManager::HealingRunCatalogInfo> runs;
    ASSERT_EQ(catalog_->listHealingRunCatalogEntries(policy.policy_id, runs, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(1u, runs.size());

    std::vector<CatalogManager::HealingStepCatalogInfo> steps;
    ASSERT_EQ(catalog_->listHealingStepCatalogEntries(runs.front().run_id, steps, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(1u, steps.size());

    CatalogManager::SecurityOperationsAutomationResult second_result;
    ASSERT_EQ(catalog_->runSecurityOperationsAutomation(request, second_result, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(0u, second_result.healing_run_count);
    ASSERT_EQ(1u, second_result.actions.size());
    EXPECT_EQ("SECOPS_EXISTING_RUN", second_result.actions.front().action_code);
    EXPECT_EQ(runs.front().run_id, second_result.actions.front().healing_run_id);
}
