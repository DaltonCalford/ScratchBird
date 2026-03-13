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

    TEST(SqlObservabilityViewBuilderTest, FreezesMgaSqlViewsAndDashboardContracts)
    {
        std::vector<SqlViewSchemaDefinition> views;
        ASSERT_EQ(MgaObservabilityContract::appendSqlViewDefinitions(views), Status::OK);
        ASSERT_EQ(views.size(), 7u);
        EXPECT_EQ(views.front().view_name, "sb_mga_active_transactions");
        EXPECT_EQ(views.back().view_name, "sb_mga_wait_history");
        for (const SqlViewSchemaDefinition& view : views)
        {
            EXPECT_EQ(view.schema_version, MgaObservabilityContract::sql_view_schema_version());
            EXPECT_FALSE(view.columns.empty()) << view.view_name;
        }

        const auto runtime_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_mga_runtime_metrics";
            });
        ASSERT_NE(runtime_view, views.end());
        ASSERT_EQ(runtime_view->columns.size(), 5u);
        EXPECT_EQ(runtime_view->columns[0].column_name, "metric_name");
        EXPECT_EQ(runtime_view->columns[0].column_type, "VARCHAR");
        EXPECT_EQ(runtime_view->columns[3].column_name, "labels_json");
        EXPECT_EQ(runtime_view->columns[3].column_type, "JSON");

        const auto history_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_mga_transaction_history";
            });
        ASSERT_NE(history_view, views.end());
        EXPECT_EQ(history_view->columns[10].column_name, "publication_fence_seconds");
        EXPECT_TRUE(history_view->columns[10].nullable);
        EXPECT_EQ(history_view->columns[11].column_name, "limbo_state");

        std::vector<DashboardSchemaDefinition> dashboards;
        ASSERT_EQ(MgaObservabilityContract::appendDashboardDefinitions(dashboards), Status::OK);
        ASSERT_EQ(dashboards.size(), 5u);
        EXPECT_EQ(dashboards.front().dashboard_id, "sb_mga_chain_locality_fragmentation");
        EXPECT_EQ(dashboards.back().dashboard_id, "sb_mga_restart_crash_window_anomalies");
        for (const DashboardSchemaDefinition& dashboard : dashboards)
        {
            EXPECT_EQ(dashboard.schema_version,
                      MgaObservabilityContract::dashboard_schema_version());
            EXPECT_FALSE(dashboard.panels.empty()) << dashboard.dashboard_id;
            EXPECT_FALSE(dashboard.alerts.empty()) << dashboard.dashboard_id;
        }

        const auto long_snapshot_dashboard = std::find_if(
            dashboards.begin(), dashboards.end(), [](const DashboardSchemaDefinition& dashboard) {
                return dashboard.dashboard_id == "sb_mga_long_snapshot_blockers";
            });
        ASSERT_NE(long_snapshot_dashboard, dashboards.end());
        ASSERT_EQ(long_snapshot_dashboard->panels.size(), 2u);
        EXPECT_EQ(long_snapshot_dashboard->panels[0].source_view, "sb_mga_snapshot_blockers");
        EXPECT_EQ(long_snapshot_dashboard->panels[0].required_fields[2], "retained_bytes");
        EXPECT_EQ(long_snapshot_dashboard->alerts[0].predicate, "snapshot_age_seconds > 300");

        const auto restart_dashboard = std::find_if(
            dashboards.begin(), dashboards.end(), [](const DashboardSchemaDefinition& dashboard) {
                return dashboard.dashboard_id == "sb_mga_restart_crash_window_anomalies";
            });
        ASSERT_NE(restart_dashboard, dashboards.end());
        EXPECT_EQ(restart_dashboard->panels[1].source_view, "sb_mga_failpoint_events");
        EXPECT_EQ(restart_dashboard->alerts[0].predicate, "commit fence backlog older than 2 s");
    }

} // namespace scratchbird::core
