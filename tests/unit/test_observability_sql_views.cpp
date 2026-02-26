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

    TEST(SqlObservabilityViewBuilderTest, BuildsRuntimeAndHealthRowsWithDeterministicOrdering)
    {
        MetricsRegistry& registry = MetricsRegistry::getInstance();
        registry.clear();

        auto* queries = registry.registerCounter(
            "sb_engine_queries_total",
            "queries",
            {"db", "result"});
        auto* lag = registry.registerGauge(
            "sb_cluster_replication_lag_txn",
            "replication lag",
            {"db", "shard"});
        ASSERT_NE(queries, nullptr);
        ASSERT_NE(lag, nullptr);
        queries->inc(1.0, {"sb", "ok"});
        lag->set(4.0, {"sb", "010"});

        std::vector<SqlRuntimeMetricRow> runtime_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildRuntimeRows(registry, 1700000001, runtime_rows), Status::OK);
        ASSERT_GE(runtime_rows.size(), 2u);
        EXPECT_LE(runtime_rows[0].metric_name, runtime_rows[1].metric_name);

        bool saw_query_total = false;
        for (const SqlRuntimeMetricRow& row : runtime_rows)
        {
            if (row.metric_name == "sb_engine_queries_total")
            {
                saw_query_total = true;
                EXPECT_EQ(row.metric_type, "counter");
                EXPECT_NE(row.labels_json.find("\"db\":\"sb\""), std::string::npos);
                EXPECT_NE(row.labels_json.find("\"result\":\"ok\""), std::string::npos);
            }
        }
        EXPECT_TRUE(saw_query_total);

        std::vector<HealthComponentRow> health_in{
            {"listener_pool_available", HealthComponentStatus::OK, "listener healthy", 1700000001},
            {"catalog_available", HealthComponentStatus::FAIL, "catalog unavailable", 1700000001},
        };
        std::vector<HealthComponentRow> health_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildHealthRows(health_in, health_rows), Status::OK);
        ASSERT_EQ(health_rows.size(), 2u);
        EXPECT_EQ(health_rows[0].component, "catalog_available");
        EXPECT_EQ(health_rows[1].component, "listener_pool_available");
    }

    TEST(SqlObservabilityViewBuilderTest, BuildsClusterShardAndSnapshotRows)
    {
        std::vector<ClusterShardObservabilityInput> shard_in{
            {"db-b", "shard-2", "node-2", 3, 1701, 32, 30, 28, 28, 4, 0.9},
            {"db-a", "shard-1", "node-1", 2, 1700, 20, 19, 19, 19, 1, 0.2},
        };
        std::vector<SqlClusterShardMetricRow> shard_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildClusterShardRows(shard_in, shard_rows), Status::OK);
        ASSERT_EQ(shard_rows.size(), 2u);
        EXPECT_EQ(shard_rows[0].db_uuid, "db-a");
        EXPECT_EQ(shard_rows[0].shard_id, "shard-1");
        EXPECT_EQ(shard_rows[1].db_uuid, "db-b");
        EXPECT_EQ(shard_rows[1].replication_lag_txn, 4u);

        std::vector<ClusterSnapshotObservabilityInput> snap_in{
            {"sess-b", "db-a", "shard-2", 44, 1600, 1650},
            {"sess-a", "db-a", "shard-1", 12, 1500, 1550},
        };
        std::vector<SqlClusterSnapshotMetricRow> snap_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildClusterSnapshotRows(snap_in, snap_rows), Status::OK);
        ASSERT_EQ(snap_rows.size(), 2u);
        EXPECT_EQ(snap_rows[0].session_id, "sess-a");
        EXPECT_EQ(snap_rows[0].snapshot_boundary, 12u);
        EXPECT_EQ(snap_rows[1].session_id, "sess-b");
        EXPECT_EQ(snap_rows[1].snapshot_boundary, 44u);
    }

} // namespace scratchbird::core
