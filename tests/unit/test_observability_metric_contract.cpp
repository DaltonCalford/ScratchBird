/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/observability_contract.h"

#include <algorithm>

#include <gtest/gtest.h>

namespace scratchbird::core
{

    TEST(MetricContractPolicyTest, RegistryAuditFlagsLegacyNamesAndForbiddenLabels)
    {
        MetricsRegistry registry;

        auto* canonical = registry.registerCounter(
            "sb_cluster_replication_apply_total",
            "canonical cluster apply",
            {"db", "shard", "result"});
        ASSERT_NE(canonical, nullptr);
        canonical->inc(1.0, {"sb", "001", "ok"});

        auto* legacy = registry.registerCounter(
            "scratchbird_queries_total",
            "legacy metric",
            {"db", "result"});
        ASSERT_NE(legacy, nullptr);
        legacy->inc(1.0, {"sb", "ok"});

        auto* forbidden = registry.registerGauge(
            "sb_engine_connections_active",
            "forbidden label example",
            {"session_id"});
        ASSERT_NE(forbidden, nullptr);
        forbidden->set(3.0, {"sess-1"});

        std::vector<MetricPolicyViolation> violations;
        ASSERT_EQ(MetricContractPolicy::auditRegistry(registry, violations), Status::OK);
        ASSERT_FALSE(violations.empty());

        bool saw_legacy_name = false;
        bool saw_forbidden_label = false;
        for (const MetricPolicyViolation& violation : violations)
        {
            if (violation.metric_name == "scratchbird_queries_total" &&
                violation.reason == "metric_name_non_canonical")
            {
                saw_legacy_name = true;
            }
            if (violation.metric_name == "sb_engine_connections_active" &&
                violation.reason == "forbidden_label:session_id")
            {
                saw_forbidden_label = true;
            }
        }

        EXPECT_TRUE(saw_legacy_name);
        EXPECT_TRUE(saw_forbidden_label);
    }

    TEST(MetricContractPolicyTest, RegistersBaselineSbObsMetricsAndProducesNoPolicyViolations)
    {
        MetricsRegistry registry;

        ASSERT_EQ(MetricContractPolicy::registerSbObsBaselineMetrics(registry), Status::OK);

        EXPECT_NE(registry.get("sb_engine_queries_total"), nullptr);
        EXPECT_NE(registry.get("sb_engine_query_duration_seconds"), nullptr);
        EXPECT_NE(registry.get("sb_cluster_leader_term"), nullptr);
        EXPECT_NE(registry.get("sb_cluster_gc_safe_horizon_txn"), nullptr);

        auto* query_total = dynamic_cast<Counter*>(registry.get("sb_engine_queries_total"));
        auto* lag_txn = dynamic_cast<Gauge*>(registry.get("sb_cluster_replication_lag_txn"));
        ASSERT_NE(query_total, nullptr);
        ASSERT_NE(lag_txn, nullptr);
        query_total->inc(2.0, {"sb", "ok"});
        lag_txn->set(7.0, {"sb", "001"});

        std::vector<MetricPolicyViolation> violations;
        ASSERT_EQ(MetricContractPolicy::auditRegistry(registry, violations), Status::OK);
        EXPECT_TRUE(violations.empty());

        std::vector<std::pair<std::string, std::string>> mapping;
        ASSERT_EQ(MetricContractPolicy::buildLegacyNameMapping(mapping), Status::OK);
        ASSERT_FALSE(mapping.empty());
        EXPECT_EQ(mapping.front().first, "scratchbird_buffer_pool_hits_total");
    }

    TEST(MetricContractPolicyTest, MgaContractIsVersionedAndRegistryAuditPasses)
    {
        MetricsRegistry registry;

        EXPECT_STREQ(MgaObservabilityContract::contract_id(), "sb_mga_observability/v1");
        EXPECT_EQ(MgaObservabilityContract::metric_schema_version(), 1u);
        EXPECT_EQ(MgaObservabilityContract::sql_view_schema_version(), 1u);
        EXPECT_EQ(MgaObservabilityContract::dashboard_schema_version(), 1u);

        std::vector<MetricSchemaDefinition> definitions;
        ASSERT_EQ(MgaObservabilityContract::appendMetricDefinitions(definitions), Status::OK);
        ASSERT_EQ(definitions.size(), 74u);
        EXPECT_EQ(definitions.front().metric_name, "sb_buf_commit_fence_backlog");
        EXPECT_EQ(definitions.back().metric_name, "sb_writeback_incidents_open");
        for (const MetricSchemaDefinition& definition : definitions)
        {
            EXPECT_TRUE(MetricContractPolicy::isCanonicalMetricName(definition.metric_name))
                << definition.metric_name;
        }

        ASSERT_EQ(MgaObservabilityContract::registerRequiredMetrics(registry), Status::OK);
        std::vector<std::string> missing_metrics;
        ASSERT_EQ(
            MgaObservabilityContract::verifyRegistryContainsRequiredMetrics(registry, missing_metrics),
            Status::OK);
        EXPECT_TRUE(missing_metrics.empty());

        auto* tx_active = dynamic_cast<Gauge*>(registry.get("sb_tx_active"));
        auto* tx_limbo = dynamic_cast<Gauge*>(registry.get("sb_tx_limbo"));
        auto* commit_fence =
            dynamic_cast<Histogram*>(registry.get("sb_tx_commit_fence_flush_seconds"));
        auto* chain_depth = dynamic_cast<Gauge*>(registry.get("sb_mga_chain_depth_bucket"));
        auto* buf_frames = dynamic_cast<Gauge*>(registry.get("sb_buf_frames_by_class"));
        auto* buf_domain_resident =
            dynamic_cast<Gauge*>(registry.get("sb_buf_domain_resident_pages"));
        auto* buf_prefetch_usefulness =
            dynamic_cast<Gauge*>(registry.get("sb_buf_prefetch_usefulness_pct"));
        auto* buf_queue_depth =
            dynamic_cast<Gauge*>(registry.get("sb_buf_writeback_queue_depth"));
        auto* buf_promotions =
            dynamic_cast<Counter*>(registry.get("sb_buf_promotions_total"));
        auto* lock_wait = dynamic_cast<Counter*>(registry.get("sb_lock_wait_seconds_total"));
        auto* checkpoint_generation =
            dynamic_cast<Gauge*>(registry.get("sb_checkpoint_generation_current"));
        auto* recovery_generation =
            dynamic_cast<Gauge*>(registry.get("sb_recovery_generation_current"));
        auto* writeback_open =
            dynamic_cast<Gauge*>(registry.get("sb_writeback_incidents_open"));
        ASSERT_NE(tx_active, nullptr);
        ASSERT_NE(tx_limbo, nullptr);
        ASSERT_NE(commit_fence, nullptr);
        ASSERT_NE(chain_depth, nullptr);
        ASSERT_NE(buf_frames, nullptr);
        ASSERT_NE(buf_domain_resident, nullptr);
        ASSERT_NE(buf_prefetch_usefulness, nullptr);
        ASSERT_NE(buf_queue_depth, nullptr);
        ASSERT_NE(buf_promotions, nullptr);
        ASSERT_NE(lock_wait, nullptr);
        ASSERT_NE(checkpoint_generation, nullptr);
        ASSERT_NE(recovery_generation, nullptr);
        ASSERT_NE(writeback_open, nullptr);

        tx_active->set(7.0, {"sb"});
        tx_limbo->set(1.0, {"sb", "prepared"});
        commit_fence->observe(0.012, {"sb", "ok"});
        chain_depth->set(4.0, {"sb", "orders", "depth_4_7"});
        buf_frames->set(128.0, {"sb", "commit_fence"});
        buf_domain_resident->set(32.0, {"sb", "hot_oltp"});
        buf_prefetch_usefulness->set(75.0, {"sb"});
        buf_queue_depth->set(6.0, {"sb", "checkpoint"});
        buf_promotions->inc(2.0, {"sb"});
        lock_wait->inc(0.50, {"sb", "WAIT"});
        checkpoint_generation->set(3.0, {"sb"});
        recovery_generation->set(4.0, {"sb"});
        writeback_open->set(1.0, {"sb", "write_fenced"});

        std::vector<MetricPolicyViolation> violations;
        ASSERT_EQ(MetricContractPolicy::auditRegistry(registry, violations), Status::OK);
        EXPECT_TRUE(violations.empty());
    }

} // namespace scratchbird::core
