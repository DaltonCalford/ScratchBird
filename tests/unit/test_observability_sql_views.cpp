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
#include "scratchbird/core/page_manager.h"
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
            conn_->setCurrentUser(catalog_->getSystemUserId(&ctx), true);

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
        ASSERT_EQ(views.size(), 21u);
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

        const auto dormant_policy_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_mga_dormant_policy";
            });
        ASSERT_NE(dormant_policy_view, views.end());
        EXPECT_EQ(dormant_policy_view->columns[1].column_name, "restart_reattach_policy");
        EXPECT_EQ(dormant_policy_view->columns[2].column_name, "cleanup_policy");
        EXPECT_EQ(dormant_policy_view->columns[7].column_name, "restart_stale_rows");

        const auto dormant_transactions_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_mga_dormant_transactions";
            });
        ASSERT_NE(dormant_transactions_view, views.end());
        EXPECT_EQ(dormant_transactions_view->columns[1].column_name, "dormant_id");
        EXPECT_EQ(dormant_transactions_view->columns[12].column_name, "lease_expires_at_ms");
        EXPECT_EQ(dormant_transactions_view->columns[19].column_name, "last_statement_text");

        const auto history_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_mga_transaction_history";
            });
        ASSERT_NE(history_view, views.end());
        EXPECT_EQ(history_view->columns[10].column_name, "publication_fence_seconds");
        EXPECT_TRUE(history_view->columns[10].nullable);
        EXPECT_EQ(history_view->columns[11].column_name, "limbo_state");

        const auto checkpoint_status_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_checkpoint_status";
            });
        ASSERT_NE(checkpoint_status_view, views.end());
        EXPECT_EQ(checkpoint_status_view->columns[0].column_name, "checkpoint_generation");
        EXPECT_EQ(checkpoint_status_view->columns[5].column_name, "captured_flush_debt_pages");
        EXPECT_EQ(checkpoint_status_view->columns[9].column_name, "queue_rebuild_required");
        EXPECT_TRUE(checkpoint_status_view->columns[10].nullable);

        const auto recovery_status_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_recovery_status";
            });
        ASSERT_NE(recovery_status_view, views.end());
        EXPECT_EQ(recovery_status_view->columns[0].column_name, "recovery_generation");
        EXPECT_EQ(recovery_status_view->columns[4].column_name, "repair_required_pages");
        EXPECT_EQ(recovery_status_view->columns[7].column_name, "warmup_mode");

        const auto writeback_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_writeback_incidents";
            });
        ASSERT_NE(writeback_view, views.end());
        EXPECT_EQ(writeback_view->columns[0].column_name, "incident_uuid");
        EXPECT_EQ(writeback_view->columns[10].column_name, "clearance_condition");
        EXPECT_EQ(writeback_view->columns[12].column_type, "BIGINT");

        const auto sweep_resume_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_sweep_resume_status";
            });
        ASSERT_NE(sweep_resume_view, views.end());
        EXPECT_EQ(sweep_resume_view->columns[0].column_name, "sweep_generation");
        EXPECT_EQ(sweep_resume_view->columns[16].column_name, "resume_outcome");

        const auto buffer_pool_stats_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_buffer_pool_stats";
            });
        ASSERT_NE(buffer_pool_stats_view, views.end());
        EXPECT_EQ(buffer_pool_stats_view->columns[1].column_name, "profile");
        EXPECT_EQ(buffer_pool_stats_view->columns[2].column_name, "layout");
        EXPECT_EQ(buffer_pool_stats_view->columns[13].column_name, "prefetch_enabled");
        EXPECT_EQ(buffer_pool_stats_view->columns[24].column_name, "observed_at_ms");

        const auto buffer_domain_stats_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_buffer_domain_stats";
            });
        ASSERT_NE(buffer_domain_stats_view, views.end());
        EXPECT_EQ(buffer_domain_stats_view->columns[1].column_name, "domain_id");
        EXPECT_EQ(buffer_domain_stats_view->columns[6].column_name, "protected_pages");
        EXPECT_EQ(buffer_domain_stats_view->columns[13].column_name, "borrowed_pages");
        EXPECT_EQ(buffer_domain_stats_view->columns[16].column_name, "observed_at_ms");

        const auto buffer_policy_health_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_buffer_policy_health";
            });
        ASSERT_NE(buffer_policy_health_view, views.end());
        EXPECT_EQ(buffer_policy_health_view->columns[1].column_name, "ghost_hits");
        EXPECT_EQ(buffer_policy_health_view->columns[8].column_name, "prefetch_usefulness_pct");
        EXPECT_EQ(buffer_policy_health_view->columns[9].column_name, "thrash_detector_state");

        const auto buffer_prefetch_health_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_buffer_prefetch_health";
            });
        ASSERT_NE(buffer_prefetch_health_view, views.end());
        EXPECT_EQ(buffer_prefetch_health_view->columns[1].column_name, "prefetch_pages_total");
        EXPECT_EQ(buffer_prefetch_health_view->columns[6].column_name, "prefetch_scan_debt_pages");
        EXPECT_EQ(buffer_prefetch_health_view->columns[8].column_name, "thrash_detector_state");

        const auto checkpoint_writeback_pressure_view = std::find_if(
            views.begin(), views.end(), [](const SqlViewSchemaDefinition& view) {
                return view.view_name == "sb_checkpoint_writeback_pressure";
            });
        ASSERT_NE(checkpoint_writeback_pressure_view, views.end());
        EXPECT_EQ(checkpoint_writeback_pressure_view->columns[7].column_name,
                  "queue_depth_foreground_help");
        EXPECT_EQ(checkpoint_writeback_pressure_view->columns[12].column_name,
                  "queue_depth_repair_retry");
        EXPECT_EQ(checkpoint_writeback_pressure_view->columns[17].column_name, "observed_at_ms");

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

    TEST_F(MgaObservabilityLiveViewsTest, BuildsLiveBufferPolicyRowsFromSegmentedSnapshots)
    {
        ErrorContext ctx;
        ASSERT_NE(db_->buffer_pool(), nullptr);
        ASSERT_NE(db_->page_manager(), nullptr);

        uint32_t prefetched_page = 0;
        ASSERT_EQ(db_->page_manager()->allocatePage(prefetched_page, &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(db_->buffer_pool()->prefetchPages({prefetched_page}, &ctx), Status::OK)
            << ctx.message;

        BufferPool::MgaFrameSnapshot prefetch_snapshot{};
        ASSERT_EQ(db_->buffer_pool()->getMgaFrameSnapshotGlobal(
                      convertPageIDtoGPID(prefetched_page), &prefetch_snapshot, &ctx),
                  Status::OK)
            << ctx.message;
        ASSERT_TRUE(prefetch_snapshot.resident);
        EXPECT_EQ(prefetch_snapshot.policy_domain, BufferPool::PolicyDomain::ScanBulkRing);
        EXPECT_TRUE(prefetch_snapshot.speculative_prefetch);
        EXPECT_FALSE(prefetch_snapshot.prefetch_consumed);
        EXPECT_TRUE(prefetch_snapshot.residency_tier == BufferPool::ResidencyTier::RingOnly ||
                    prefetch_snapshot.residency_tier ==
                        BufferPool::ResidencyTier::Probationary);

        const uint64_t observed_at_ms = nowMicros() / 1000;

        std::vector<SqlBufferPoolStatsRow> pool_rows;
        ASSERT_EQ(
            SqlObservabilityViewBuilder::buildBufferPoolStatsRows(
                *db_, observed_at_ms, pool_rows),
            Status::OK);
        ASSERT_EQ(pool_rows.size(), 1u);
        EXPECT_EQ(pool_rows.front().layout, "segmented");
        EXPECT_TRUE(pool_rows.front().prefetch_enabled);
        EXPECT_GT(pool_rows.front().pool_pages, 0u);

        std::vector<SqlBufferDomainStatsRow> domain_rows;
        ASSERT_EQ(
            SqlObservabilityViewBuilder::buildBufferDomainStatsRows(
                *db_, observed_at_ms, domain_rows),
            Status::OK);
        ASSERT_EQ(domain_rows.size(),
                  static_cast<size_t>(BufferPool::PolicyDomain::Count));
        const auto scan_row = std::find_if(
            domain_rows.begin(), domain_rows.end(), [](const SqlBufferDomainStatsRow& row) {
                return row.domain_id == "scan_bulk_ring";
            });
        ASSERT_NE(scan_row, domain_rows.end());
        EXPECT_GT(scan_row->resident_pages, 0u);
        EXPECT_GE(scan_row->ring_only_pages + scan_row->probationary_pages, 1u);

        std::vector<SqlBufferPolicyHealthRow> policy_rows;
        ASSERT_EQ(
            SqlObservabilityViewBuilder::buildBufferPolicyHealthRows(
                *db_, observed_at_ms, policy_rows),
            Status::OK);
        ASSERT_EQ(policy_rows.size(), 1u);
        EXPECT_GE(policy_rows.front().promotions, 0u);
        EXPECT_FALSE(policy_rows.front().thrash_detector_state.empty());

        std::vector<SqlBufferPrefetchHealthRow> prefetch_rows;
        ASSERT_EQ(
            SqlObservabilityViewBuilder::buildBufferPrefetchHealthRows(
                *db_, observed_at_ms, prefetch_rows),
            Status::OK);
        ASSERT_EQ(prefetch_rows.size(), 1u);
        EXPECT_EQ(prefetch_rows.front().prefetch_pages_total, 1u);
        EXPECT_EQ(prefetch_rows.front().prefetch_pages_useful, 0u);
        EXPECT_GE(prefetch_rows.front().prefetch_debt_pages, 1u);
        EXPECT_FALSE(prefetch_rows.front().thrash_detector_state.empty());

        std::vector<SqlCheckpointWritebackPressureRow> pressure_rows;
        ASSERT_EQ(
            SqlObservabilityViewBuilder::buildCheckpointWritebackPressureRows(
                *db_, observed_at_ms, pressure_rows),
            Status::OK);
        ASSERT_EQ(pressure_rows.size(), 1u);
        EXPECT_EQ(pressure_rows.front().db_uuid, db_->uuid().toString());
        EXPECT_GE(pressure_rows.front().queue_depth_foreground_help, 0u);
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
        advisory.deleted_slots = 7;
        advisory.chain_depth_hint = 5;
        advisory.same_page_back_versions = 1;
        advisory.same_page_update_ratio = 0.25;
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
        auto find_metric = [&runtime_rows](const std::string& metric_name,
                                           const std::string& labels_fragment)
            -> std::vector<SqlRuntimeMetricRow>::const_iterator {
            return std::find_if(
                runtime_rows.begin(), runtime_rows.end(),
                [&](const SqlRuntimeMetricRow& row) {
                    return row.metric_name == metric_name &&
                           row.labels_json.find(labels_fragment) != std::string::npos;
                });
        };

        const auto committed_metric = find_metric("sb_tx_committed_total", "\"db\":");
        ASSERT_NE(committed_metric, runtime_rows.end());
        EXPECT_EQ(committed_metric->value, 1.0);

        const auto commit_fence_metric =
            find_metric("sb_buf_commit_fence_backlog", "\"db\":");
        ASSERT_NE(commit_fence_metric, runtime_rows.end());

        const auto background_reclaim_metric =
            find_metric("sb_gc_background_reclaim_bytes_total",
                        "\"relation\":\"__database__\"");
        ASSERT_NE(background_reclaim_metric, runtime_rows.end());

        const auto chain_depth_metric =
            find_metric("sb_mga_chain_depth_bucket",
                        "\"relation\":\"orders\"");
        ASSERT_NE(chain_depth_metric, runtime_rows.end());
        EXPECT_NE(chain_depth_metric->labels_json.find("\"bucket\":\"depth_4_7\""),
                  std::string::npos);

        const auto same_page_ratio_metric =
            find_metric("sb_mga_same_page_update_ratio",
                        "\"relation\":\"orders\"");
        ASSERT_NE(same_page_ratio_metric, runtime_rows.end());
        EXPECT_DOUBLE_EQ(same_page_ratio_metric->value, 0.25);

        const auto index_backlog_metric =
            find_metric("sb_gc_index_backlog_entries",
                        "\"relation\":\"orders\"");
        ASSERT_NE(index_backlog_metric, runtime_rows.end());
        EXPECT_DOUBLE_EQ(index_backlog_metric->value, 7.0);
    }

    TEST_F(MgaObservabilityLiveViewsTest, BuildsDormantPolicyAndDormantTransactionRows)
    {
        ErrorContext ctx;

        std::unique_ptr<ConnectionContext> dormant_conn;
        ASSERT_EQ(db_->connect(dormant_conn, &ctx), Status::OK) << ctx.message;
        ASSERT_NE(dormant_conn, nullptr);
        dormant_conn->setCurrentUser(catalog_->getSystemUserId(&ctx), true);
        dormant_conn->setProtocolSessionId(generateUuidV7());
        dormant_conn->setWaitForLocks(false);
        dormant_conn->setLockTimeout(15);
        ASSERT_EQ(dormant_conn->beginStatementTracking(
                      "UPDATE mga_obs.orders SET id = 1",
                      &ctx),
                  Status::OK)
            << ctx.message;
        dormant_conn->endStatementTrackingSuccess(3);

        ID dormant_id{};
        ID reattach_authkey_id{};
        ASSERT_EQ(db_->detachToDormant(dormant_conn, dormant_id, &ctx, &reattach_authkey_id),
                  Status::OK)
            << ctx.message;
        EXPECT_EQ(dormant_conn, nullptr);

        const uint64_t now_ms = nowMicros() / 1000;

        std::vector<SqlDormantTransactionPolicyRow> policy_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildDormantTransactionPolicyRows(
                      *db_, now_ms, policy_rows),
                  Status::OK);
        ASSERT_EQ(policy_rows.size(), 1u);
        EXPECT_EQ(policy_rows[0].db_uuid, db_->uuid().toString());
        EXPECT_FALSE(policy_rows[0].restart_reattach_policy.empty());
        EXPECT_FALSE(policy_rows[0].cleanup_policy.empty());
        EXPECT_GE(policy_rows[0].total_rows, 1u);
        EXPECT_GE(policy_rows[0].dormant_rows, 1u);

        std::vector<SqlDormantTransactionRow> dormant_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildDormantTransactionRows(
                      *db_, now_ms, dormant_rows),
                  Status::OK);
        auto it = std::find_if(
            dormant_rows.begin(), dormant_rows.end(),
            [&](const SqlDormantTransactionRow& row) {
                return row.dormant_id == dormant_id;
            });
        ASSERT_NE(it, dormant_rows.end());
        EXPECT_EQ(it->state, "DORMANT");
        EXPECT_EQ(it->wait_mode, "NO_WAIT");
        EXPECT_EQ(it->lock_timeout_seconds, 15u);
        EXPECT_TRUE(it->has_lease_expires_at_ms);
        EXPECT_TRUE(it->has_last_statement_time_ms);
        EXPECT_EQ(it->last_rows_affected, 3);
        EXPECT_TRUE(it->has_last_statement_text);
        EXPECT_EQ(it->last_statement_text, "UPDATE mga_obs.orders SET id = 1");
        EXPECT_GT(it->last_statement_hash, 0u);
        EXPECT_FALSE(it->restart_stale);
    }

    TEST_F(MgaObservabilityLiveViewsTest, BuildsDurabilityRowsFromCatalogHistoryAndRuntimeState)
    {
        const uint64_t now_us = nowMicros();
        MetricsRegistry registry;

        CatalogManager::CheckpointRunCatalogInfo checkpoint{};
        checkpoint.checkpoint_run_uuid = generateUuidV7();
        checkpoint.checkpoint_generation = 7;
        checkpoint.checkpoint_state = CheckpointLifecycleState::FAILED;
        checkpoint.start_time = now_us - 9'000'000;
        checkpoint.has_end_time = true;
        checkpoint.end_time = now_us - 8'000'000;
        checkpoint.dirty_generation_low_watermark = 42;
        checkpoint.pages_target = 17;
        checkpoint.pages_flushed = 9;
        checkpoint.has_failure_reason = true;
        checkpoint.failure_reason = Status::DISK_FULL;
        ASSERT_EQ(catalog_->upsertCheckpointRunCatalogEntry(checkpoint, nullptr), Status::OK);

        CatalogManager::RecoveryRunCatalogInfo recovery{};
        recovery.recovery_run_uuid = generateUuidV7();
        recovery.recovery_generation = 11;
        recovery.classification =
            Database::StartupRecoveryClassification::WRITEBACK_FAILURE_RESUME;
        recovery.start_time = now_us - 7'000'000;
        recovery.has_end_time = true;
        recovery.end_time = now_us - 6'500'000;
        recovery.normalized_transactions = 3;
        recovery.repair_required_pages = 5;
        recovery.degraded_state = Database::StartupServiceState::WRITE_FENCED;
        ASSERT_EQ(catalog_->upsertRecoveryRunCatalogEntry(recovery, nullptr), Status::OK);

        CatalogManager::RecoveryIncidentCatalogInfo recovery_incident{};
        recovery_incident.recovery_incident_uuid = generateUuidV7();
        recovery_incident.recovery_generation = recovery.recovery_generation;
        recovery_incident.classification = recovery.classification;
        recovery_incident.has_checkpoint_generation = true;
        recovery_incident.checkpoint_generation = checkpoint.checkpoint_generation;
        recovery_incident.has_object_uuid = true;
        recovery_incident.object_uuid = table_id_;
        recovery_incident.has_details = true;
        recovery_incident.details_json = "{\"reason\":\"writeback_resume\"}";
        recovery_incident.created_time = now_us - 6'400'000;
        ASSERT_EQ(catalog_->appendRecoveryIncidentCatalogEntry(recovery_incident, nullptr),
                  Status::OK);

        CatalogManager::WritebackIncidentCatalogInfo incident{};
        incident.writeback_incident_uuid = generateUuidV7();
        incident.queue_kind = WritebackQueueKind::CHECKPOINT;
        incident.policy_domain = WritebackPolicyDomain::CHECKPOINT;
        incident.page_class = PAGE_TYPE_SYSTEM_STATE;
        incident.failure_class = WritebackFailureClass::DISK_FULL;
        incident.first_seen_time = now_us - 5'000'000;
        incident.last_seen_time = now_us - 4'000'000;
        incident.retry_count = 2;
        incident.degraded_state = WritebackDegradedState::WRITE_FENCED;
        incident.is_open = true;
        incident.last_error_status = Status::DISK_FULL;
        ASSERT_EQ(catalog_->upsertWritebackIncidentCatalogEntry(incident, nullptr), Status::OK);

        CatalogManager::SweepCursorStateCatalogInfo sweep{};
        sweep.sweep_cursor_state_uuid = generateUuidV7();
        sweep.sweep_generation = 13;
        sweep.relation_uuid = table_id_;
        sweep.page_id = 91;
        sweep.slot_id = 4;
        sweep.checkpoint_generation_seen = checkpoint.checkpoint_generation;
        sweep.persist_time = now_us - 2'000'000;
        sweep.active = true;
        sweep.stage = 2;
        sweep.resume_lane_mask = 3;
        sweep.resume_strict_audit = true;
        sweep.start_horizon = 77;
        sweep.reclaimed_version_count = 12;
        sweep.reclaimed_bytes = 2048;
        sweep.index_backlog_count = 6;
        sweep.cursor_crc32c = 0x1234u;
        ASSERT_EQ(catalog_->appendSweepCursorStateCatalogEntry(sweep, nullptr), Status::OK);

        std::vector<SqlCheckpointStatusRow> checkpoint_status_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildCheckpointStatusRows(
                      *db_, checkpoint_status_rows),
                  Status::OK);
        ASSERT_EQ(checkpoint_status_rows.size(), 1u);
        EXPECT_EQ(checkpoint_status_rows.front().checkpoint_generation, 7u);
        EXPECT_EQ(checkpoint_status_rows.front().checkpoint_state, "FAILED");
        EXPECT_EQ(checkpoint_status_rows.front().pages_remaining, 8u);
        EXPECT_TRUE(checkpoint_status_rows.front().has_failure_reason);
        EXPECT_EQ(checkpoint_status_rows.front().failure_reason, "DISK_FULL");

        std::vector<SqlCheckpointHistoryRow> checkpoint_history_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildCheckpointHistoryRows(
                      *db_, checkpoint_history_rows),
                  Status::OK);
        ASSERT_EQ(checkpoint_history_rows.size(), 1u);
        EXPECT_EQ(checkpoint_history_rows.front().pages_flushed, 9u);

        std::vector<SqlRecoveryStatusRow> recovery_status_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildRecoveryStatusRows(
                      *db_, recovery_status_rows),
                  Status::OK);
        ASSERT_EQ(recovery_status_rows.size(), 1u);
        EXPECT_EQ(recovery_status_rows.front().recovery_generation, 11u);
        EXPECT_EQ(recovery_status_rows.front().classification, "writeback_failure_resume");
        EXPECT_EQ(recovery_status_rows.front().startup_state, "write_fenced");
        EXPECT_EQ(recovery_status_rows.front().normalized_transactions, 3u);
        EXPECT_TRUE(recovery_status_rows.front().write_fenced);

        std::vector<SqlRecoveryIncidentRow> recovery_incident_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildRecoveryIncidentRows(
                      *db_, recovery_incident_rows),
                  Status::OK);
        ASSERT_GE(recovery_incident_rows.size(), 1u);
        const auto recovery_incident_it = std::find_if(
            recovery_incident_rows.begin(),
            recovery_incident_rows.end(),
            [&](const SqlRecoveryIncidentRow& row) {
                return row.object_uuid == table_id_.toString() &&
                    row.recovery_generation == recovery.recovery_generation &&
                    row.has_checkpoint_generation &&
                    row.checkpoint_generation == checkpoint.checkpoint_generation;
            });
        ASSERT_NE(recovery_incident_it, recovery_incident_rows.end());
        EXPECT_EQ(recovery_incident_it->object_uuid, table_id_.toString());

        std::vector<SqlWritebackIncidentRow> incident_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildWritebackIncidentRows(
                      *db_, incident_rows),
                  Status::OK);
        ASSERT_EQ(incident_rows.size(), 1u);
        EXPECT_EQ(incident_rows.front().queue_kind, "checkpoint");
        EXPECT_EQ(incident_rows.front().policy_domain, "checkpoint");
        EXPECT_EQ(incident_rows.front().degraded_state, "write_fenced");
        EXPECT_TRUE(incident_rows.front().is_open);

        std::vector<SqlBufferWritebackDebtRow> debt_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildBufferWritebackDebtRows(
                      *db_, debt_rows),
                  Status::OK);
        ASSERT_EQ(debt_rows.size(), 1u);
        EXPECT_EQ(debt_rows.front().checkpoint_pages_remaining, 8u);
        EXPECT_TRUE(debt_rows.front().incident_open);
        EXPECT_EQ(debt_rows.front().retry_count, 2u);

        std::vector<SqlSweepResumeStatusRow> sweep_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildSweepResumeStatusRows(
                      *db_, sweep_rows),
                  Status::OK);
        ASSERT_EQ(sweep_rows.size(), 1u);
        EXPECT_EQ(sweep_rows.front().sweep_generation, 13u);
        EXPECT_EQ(sweep_rows.front().relation_uuid, table_id_.toString());
        EXPECT_EQ(sweep_rows.front().resume_outcome, "rewind_required");

        std::vector<SqlRuntimeMetricRow> runtime_rows;
        ASSERT_EQ(SqlObservabilityViewBuilder::buildMgaRuntimeRows(
                      *db_, registry, now_us / 1000, runtime_rows),
                  Status::OK);

        auto find_metric = [&runtime_rows](const std::string& metric_name,
                                           const std::string& labels_fragment)
            -> std::vector<SqlRuntimeMetricRow>::const_iterator {
            return std::find_if(
                runtime_rows.begin(), runtime_rows.end(),
                [&](const SqlRuntimeMetricRow& row) {
                    return row.metric_name == metric_name &&
                           row.labels_json.find(labels_fragment) != std::string::npos;
                });
        };

        const auto checkpoint_generation_metric =
            find_metric("sb_checkpoint_generation_current", "\"db\":");
        ASSERT_NE(checkpoint_generation_metric, runtime_rows.end());
        EXPECT_DOUBLE_EQ(checkpoint_generation_metric->value, 7.0);

        const auto checkpoint_failures_metric =
            find_metric("sb_checkpoint_failed_total", "\"reason\":\"DISK_FULL\"");
        ASSERT_NE(checkpoint_failures_metric, runtime_rows.end());
        EXPECT_DOUBLE_EQ(checkpoint_failures_metric->value, 1.0);

        const auto recovery_generation_metric =
            find_metric("sb_recovery_generation_current", "\"db\":");
        ASSERT_NE(recovery_generation_metric, runtime_rows.end());
        EXPECT_DOUBLE_EQ(recovery_generation_metric->value, 11.0);

        const auto writeback_open_metric =
            find_metric("sb_writeback_incidents_open",
                        "\"degraded_state\":\"write_fenced\"");
        ASSERT_NE(writeback_open_metric, runtime_rows.end());
        EXPECT_DOUBLE_EQ(writeback_open_metric->value, 1.0);
    }

} // namespace scratchbird::core
