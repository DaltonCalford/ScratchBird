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

#include <gtest/gtest.h>

namespace scratchbird::core
{

    TEST(MetricContractPolicyTest, RegistryAuditFlagsLegacyNamesAndForbiddenLabels)
    {
        MetricsRegistry& registry = MetricsRegistry::getInstance();
        registry.clear();

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
        MetricsRegistry& registry = MetricsRegistry::getInstance();
        registry.clear();

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

} // namespace scratchbird::core
