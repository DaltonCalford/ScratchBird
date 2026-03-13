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
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>

#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/mga_failpoint_manager.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{

    class MgaObservabilityLiveViewsTest : public ::testing::Test
    {
    protected:
        std::string db_path_;
        std::unique_ptr<Database> db_;
        CatalogManager* catalog_ = nullptr;
        std::unique_ptr<ConnectionContext> conn_;
        ID schema_id_{};
        ID table_id_{};

        void SetUp() override
        {
            db_path_ = "/tmp/test_mga_observability_views_" + std::to_string(getpid()) + ".db";
            std::remove(db_path_.c_str());

            ErrorContext ctx;
            ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

            db_ = std::make_unique<Database>();
            ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
            catalog_ = db_->catalog_manager();
            ASSERT_NE(catalog_, nullptr);

            ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
            ConnectionContext::setCurrent(conn_.get());

            ASSERT_EQ(catalog_->createSchema("mga_obs", "system", schema_id_, &ctx), Status::OK)
                << ctx.message;

            CatalogManager::ColumnInfo col{};
            col.column_name = "id";
            col.data_type = static_cast<uint16_t>(DataType::INT64);
            col.nullable = false;
            std::vector<CatalogManager::ColumnInfo> cols{col};
            ASSERT_EQ(catalog_->createTable(schema_id_, "orders", cols, table_id_, 0, &ctx), Status::OK)
                << ctx.message;
        }

        void TearDown() override
        {
            ConnectionContext::setCurrent(nullptr);
            conn_.reset();
            if (db_)
            {
                db_->close();
                db_.reset();
            }
            std::remove(db_path_.c_str());
        }

        auto nowMicros() const -> uint64_t
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
        }
    };

    TEST(SqlObservabilityViewBuilderTest, BuildsRuntimeAndHealthRowsWithDeterministicOrdering)
    {
        MetricsRegistry registry;

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

    TEST_F(MgaObservabilityLiveViewsTest, BuildsLiveMgaRowsFromRuntimeCatalogAndFragmentationState)
    {
        MetricsRegistry& registry = MetricsRegistry::getInstance();

        const uint64_t now_us = nowMicros();

        CatalogManager::RuntimeTransactionCatalogInfo tx_active{};
        tx_active.txid = 41;
        tx_active.tx_uuid = generateUuidV7();
        tx_active.database_id = db_->uuid();
        tx_active.user_id = catalog_->getSystemUserId(nullptr);
        tx_active.isolation_level = static_cast<uint8_t>(IsolationLevel::SNAPSHOT);
        tx_active.state = CatalogManager::RuntimeTransactionState::IN_PROGRESS;
        tx_active.start_time = now_us - 6'000'000;
        tx_active.created_time = now_us - 6'000'000;
        tx_active.last_modified_time = now_us - 1'000'000;
        ASSERT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(tx_active, nullptr), Status::OK);

        CatalogManager::RuntimeTransactionCatalogInfo tx_prepared = tx_active;
        tx_prepared.txid = 42;
        tx_prepared.tx_uuid = generateUuidV7();
        tx_prepared.state = CatalogManager::RuntimeTransactionState::PREPARED;
        tx_prepared.start_time = now_us - 12'000'000;
        tx_prepared.created_time = now_us - 12'000'000;
        tx_prepared.has_end_time = true;
        tx_prepared.end_time = now_us - 500'000;
        ASSERT_EQ(catalog_->upsertRuntimeTransactionCatalogEntry(tx_prepared, nullptr), Status::OK);

        CatalogManager::TransactionHistoryEntry committed{};
        committed.thread_id = 7;
        committed.event_id = 100;
        committed.end_event_id = 101;
        committed.trx_id = 40;
        committed.start_oit = 30;
        committed.end_oit = 31;
        committed.start_oat = 40;
        committed.end_oat = 41;
        committed.start_ost = 40;
        committed.end_ost = 41;
        committed.timer_start = now_us - 20'000'000;
        committed.timer_end = now_us - 19'000'000;
        committed.timer_wait = 1'000'000;
        committed.committed = true;
        ASSERT_EQ(catalog_->recordTransactionHistory(committed, nullptr), Status::OK);

        CatalogManager::WaitHistoryEntry wait{};
        wait.thread_id = 17;
        wait.blocker_thread_id = 18;
        wait.event_id = 200;
        wait.timer_start = now_us - 4'000'000;
        wait.timer_end = now_us - 3'000'000;
        wait.timer_wait = 1'000'000;
        wait.has_blocker_txid = true;
        wait.blocker_txid = 41;
        wait.has_victim_txid = true;
        wait.victim_txid = 42;
        wait.requested_mode = static_cast<uint8_t>(LockMode::LOCK_EXCLUSIVE);
        wait.blocker_mode = static_cast<uint8_t>(LockMode::LOCK_SHARE);
        wait.outcome_code = "DEADLOCK_DETECTED";
        wait.victim_reason_code = "youngest_xid";
        wait.blocker_identity = tx_active.session_id.toString();
        wait.victim_identity = tx_prepared.session_id.toString();
        ASSERT_EQ(catalog_->recordWaitHistory(wait, nullptr), Status::OK);

        std::vector<MgaFailpointDefinition> failpoints{
            {std::string(MgaFailpointTriggers::kAfterTipTerminalBeforeClientAck),
             MgaFailpointAction::MARK_ONLY,
             1,
             Status::OK,
             0,
             "post_commit_marker"}};
        ASSERT_EQ(db_->mga_failpoint_manager()->installSeed("obs-seed", failpoints, nullptr),
                  Status::OK);
        ASSERT_EQ(db_->mga_failpoint_manager()->trip(
                      MgaFailpointTriggers::kAfterTipTerminalBeforeClientAck,
                      {},
                      nullptr),
                  Status::OK);

        StorageEngine::FragmentationAdvisory advisory{};
        advisory.page_id = 17;
        advisory.reclaimable_bytes = 512;
        advisory.dead_space_ratio = 0.40;
        advisory.rewrite_recommended = true;
        db_->storage_engine()->publishFragmentationAdvisory(table_id_, advisory.page_id, advisory);

        std::vector<SqlMgaActiveTransactionRow> active_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildMgaActiveTransactionRows(
                      *db_, now_us / 1000, active_rows),
                  Status::OK);
        ASSERT_EQ(active_rows.size(), 3u);
        EXPECT_EQ(active_rows.front().txid, 42u);
        EXPECT_GT(active_rows.front().age_seconds, 0.0);
        EXPECT_NE(std::find_if(active_rows.begin(),
                               active_rows.end(),
                               [this](const SqlMgaActiveTransactionRow& row) {
                                   return row.txid == conn_->getCurrentXid();
                               }),
                  active_rows.end());
        EXPECT_NE(std::find_if(active_rows.begin(),
                               active_rows.end(),
                               [](const SqlMgaActiveTransactionRow& row) {
                                   return row.txid == 41u;
                               }),
                  active_rows.end());
        EXPECT_NE(std::find_if(active_rows.begin(),
                               active_rows.end(),
                               [](const SqlMgaActiveTransactionRow& row) {
                                   return row.txid == 42u;
                               }),
                  active_rows.end());

        std::vector<SqlMgaCleanupDebtRow> cleanup_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildMgaCleanupDebtRows(
                      *db_, registry, now_us / 1000, cleanup_rows),
                  Status::OK);
        ASSERT_EQ(cleanup_rows.size(), 1u);
        EXPECT_EQ(cleanup_rows[0].relation_name, "orders");
        EXPECT_EQ(cleanup_rows[0].cleanup_debt_bytes, 512u);
        EXPECT_TRUE(cleanup_rows[0].rewrite_recommended);

        std::vector<SqlMgaWaitHistoryRow> wait_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildMgaWaitHistoryRows(*db_, wait_rows), Status::OK);
        ASSERT_EQ(wait_rows.size(), 1u);
        EXPECT_EQ(wait_rows[0].outcome, "DEADLOCK_DETECTED");
        EXPECT_TRUE(wait_rows[0].has_blocker_txid);
        EXPECT_EQ(wait_rows[0].blocker_txid, 41u);

        std::vector<SqlMgaFailpointEventRow> failpoint_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildMgaFailpointEventRows(*db_, failpoint_rows),
                  Status::OK);
        ASSERT_EQ(failpoint_rows.size(), 1u);
        EXPECT_EQ(failpoint_rows[0].seed_id, "obs-seed");
        EXPECT_EQ(failpoint_rows[0].trigger_name,
                  MgaFailpointTriggers::kAfterTipTerminalBeforeClientAck);
        EXPECT_EQ(failpoint_rows[0].outcome, "post_commit_marker");
        EXPECT_FALSE(failpoint_rows[0].has_txid);

        std::vector<SqlRuntimeMetricRow> runtime_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildMgaRuntimeRows(
                      *db_, registry, now_us / 1000, runtime_rows),
                  Status::OK);
        EXPECT_FALSE(runtime_rows.empty());
        const auto committed_metric = std::find_if(
            runtime_rows.begin(), runtime_rows.end(), [](const SqlRuntimeMetricRow& row) {
                return row.metric_name == "sb_tx_committed_total";
            });
        ASSERT_NE(committed_metric, runtime_rows.end());
        EXPECT_EQ(committed_metric->value, 1.0);
    }

} // namespace scratchbird::core
