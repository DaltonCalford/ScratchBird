/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/job_scheduler_utils.h"
#include "scratchbird/core/job_scheduler.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/workload_governance.h"
#include "scratchbird/parser/v3_compiler.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace scratchbird::core::detail {
namespace {

CatalogManager::JobRunInfo makeRun(CatalogManager::JobRunState state, uint64_t completed_at) {
    CatalogManager::JobRunInfo run;
    run.state = state;
    run.completed_at = completed_at;
    return run;
}

}  // namespace

TEST(JobSchedulerCronParsing, RejectsInvalidFields) {
    CronExpression expr{};
    EXPECT_FALSE(parseCronExpression("*/5 * * *", expr));
    EXPECT_FALSE(parseCronExpression("61 * * * *", expr));
    EXPECT_FALSE(parseCronExpression("* * * 13 *", expr));
}

TEST(JobSchedulerCronParsing, ComputesNextRunFromEpoch) {
    uint64_t next = computeNextCronRunMs("2 0 1 1 *", 0);
    EXPECT_EQ(next, 120000u);
}

TEST(JobSchedulerCronParsing, ComputesNextRunWithStep) {
    uint64_t after_ms = 0;
    uint64_t next = computeNextCronRunMs("*/15 * * * *", after_ms);
    EXPECT_EQ(next, 900000u);
}

TEST(JobSchedulerDependencyGating, RequiresCompletedLatestRun) {
    std::vector<CatalogManager::JobRunInfo> runs;
    EXPECT_FALSE(dependencySatisfied(runs));

    runs = {
        makeRun(CatalogManager::JobRunState::FAILED, 100),
        makeRun(CatalogManager::JobRunState::COMPLETED, 200),
    };
    EXPECT_TRUE(dependencySatisfied(runs));

    runs = {
        makeRun(CatalogManager::JobRunState::COMPLETED, 100),
        makeRun(CatalogManager::JobRunState::FAILED, 200),
    };
    EXPECT_FALSE(dependencySatisfied(runs));
}

}  // namespace scratchbird::core::detail

namespace scratchbird::core {
namespace {

std::vector<uint8_t> compileSimpleSqlBytecode(const std::string& sql) {
    scratchbird::parser::v3::Compiler compiler;
    auto compiled = compiler.compile(sql);
    EXPECT_TRUE(compiled.ok) << compiled.error;
    if (!compiled.ok) {
        return {};
    }
    return compiled.bytecode;
}

uint64_t nowMs() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

bool waitForJobRuns(CatalogManager* catalog,
                    const ID& job_id,
                    size_t min_count,
                    uint32_t timeout_ms,
                    std::vector<CatalogManager::JobRunInfo>* runs_out = nullptr) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        std::vector<CatalogManager::JobRunInfo> runs;
        ErrorContext ctx;
        if (catalog->listJobRuns(job_id, runs, &ctx) == Status::OK &&
            runs.size() >= min_count) {
            if (runs_out) {
                *runs_out = runs;
            }
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool waitForJobRunState(CatalogManager* catalog,
                        const ID& job_run_id,
                        CatalogManager::JobRunState expected_state,
                        uint32_t timeout_ms,
                        CatalogManager::JobRunInfo* run_out = nullptr) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        CatalogManager::JobRunInfo run;
        ErrorContext ctx;
        if (catalog->getJobRun(job_run_id, run, &ctx) == Status::OK &&
            run.state == expected_state) {
            if (run_out) {
                *run_out = run;
            }
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

CatalogManager::JobInfo buildSimpleJob(const std::string& name,
                                       const ID& system_user,
                                       uint64_t scheduled_time) {
    CatalogManager::JobInfo job;
    job.job_name = name;
    job.job_type = CatalogManager::JobType::SQL;
    job.job_sql = "SELECT 1";
    job.bytecode = compileSimpleSqlBytecode("SELECT 1");
    job.source_dialect = "scratchbird_v3";
    job.schedule_kind = CatalogManager::ScheduleKind::AT;
    job.starts_at = scheduled_time;
    job.next_run_time = scheduled_time;
    job.created_at = scheduled_time;
    job.created_by_user_uuid = system_user;
    job.state = CatalogManager::JobState::ENABLED;
    return job;
}

}  // namespace

TEST(JobSchedulerMaintenanceSeed, JobsPresentAndEnabled) {
    testing::TestDatabaseFile db_file("test_scheduler_seed");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    CatalogManager::JobInfo job;
    ASSERT_EQ(catalog->getJobByName("daily_sweep", job, &ctx), Status::OK);
    EXPECT_EQ(job.state, CatalogManager::JobState::ENABLED);
    EXPECT_EQ(job.schedule_kind, CatalogManager::ScheduleKind::CRON);
    EXPECT_EQ(job.job_sql, "SWEEP DATABASE");
    EXPECT_EQ(job.source_dialect, "scratchbird_v3");
    EXPECT_FALSE(job.bytecode.empty());
    EXPECT_GT(job.next_run_time, job.created_at);

    EXPECT_EQ(catalog->getJobByName("update_stats", job, &ctx), Status::NOT_FOUND);
    EXPECT_EQ(catalog->getJobByName("rebuild_indexes", job, &ctx), Status::NOT_FOUND);

    db.close();
}

TEST(JobSchedulerCancellation, CancelledRunSkippedBeforeExecution) {
    testing::TestDatabaseFile db_file("test_scheduler_cancel");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    CatalogManager::JobInfo job;
    job.job_name = "cancel_me";
    job.job_type = CatalogManager::JobType::SQL;
    job.job_sql = "SELECT 1";
    job.bytecode = compileSimpleSqlBytecode(job.job_sql);
    job.source_dialect = "scratchbird_v3";
    job.schedule_kind = CatalogManager::ScheduleKind::AT;
    job.starts_at = nowMs();
    job.next_run_time = job.starts_at;
    job.created_at = job.starts_at;
    job.created_by_user_uuid = catalog->getSystemUserId(&ctx);
    job.state = CatalogManager::JobState::ENABLED;

    ID job_id;
    ASSERT_EQ(catalog->createJob(job, job_id, &ctx), Status::OK);

    JobScheduler::Config config;
    config.polling_interval_seconds = 1;
    config.pre_execute_delay_ms = 500;

    JobScheduler scheduler(&db, config);
    ASSERT_EQ(scheduler.start(&ctx), Status::OK);

    CatalogManager::JobRunInfo run;
    bool found = false;
    auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(config.polling_interval_seconds * 1000 +
                                  config.pre_execute_delay_ms + 1000);
    while (std::chrono::steady_clock::now() < deadline) {
        std::vector<CatalogManager::JobRunInfo> runs;
        if (catalog->listJobRuns(job_id, runs, &ctx) == Status::OK && !runs.empty()) {
            run = runs.front();
            found = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(found);

    run.state = CatalogManager::JobRunState::CANCELLED;
    run.completed_at = nowMs();
    run.result_message = "Cancelled by test";
    ASSERT_EQ(catalog->updateJobRun(run, &ctx), Status::OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    CatalogManager::JobRunInfo updated;
    ASSERT_EQ(catalog->getJobRun(run.job_run_id, updated, &ctx), Status::OK);
    EXPECT_EQ(updated.state, CatalogManager::JobRunState::CANCELLED);

    scheduler.stop();
    db.close();
}

TEST(JobSchedulerExecuteNow, ManualExecutionCreatesRun) {
    testing::TestDatabaseFile db_file("test_scheduler_execute_now");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    uint64_t scheduled_time = nowMs();
    CatalogManager::JobInfo job = buildSimpleJob(
        "manual_job", catalog->getSystemUserId(&ctx), scheduled_time);

    ID job_id;
    ASSERT_EQ(catalog->createJob(job, job_id, &ctx), Status::OK);
    job.job_id = job_id;

    JobScheduler::Config config;
    // Keep manual execution deterministic in CI by avoiding long poll windows.
    config.polling_interval_seconds = 1;

    JobScheduler scheduler(&db, config);
    ASSERT_EQ(scheduler.start(&ctx), Status::OK);

    ID run_id;
    ASSERT_EQ(scheduler.executeJobNow(job, run_id, &ctx), Status::OK);

    CatalogManager::JobRunInfo run;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(10000);
    while (std::chrono::steady_clock::now() < deadline) {
        ASSERT_EQ(catalog->getJobRun(run_id, run, &ctx), Status::OK);
        if (run.state != CatalogManager::JobRunState::RUNNING &&
            run.state != CatalogManager::JobRunState::PENDING) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_NE(run.state, CatalogManager::JobRunState::RUNNING);
    EXPECT_NE(run.state, CatalogManager::JobRunState::PENDING);
    EXPECT_EQ(run.state, CatalogManager::JobRunState::COMPLETED);

    scheduler.stop();
    db.close();
}

TEST(JobSchedulerPendingState, RunTransitionsFromPendingToRunning) {
    testing::TestDatabaseFile db_file("test_scheduler_pending_state");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    uint64_t scheduled_time = nowMs();
    CatalogManager::JobInfo job = buildSimpleJob(
        "pending_state_job", catalog->getSystemUserId(&ctx), scheduled_time);

    ID job_id;
    ASSERT_EQ(catalog->createJob(job, job_id, &ctx), Status::OK);
    job.job_id = job_id;

    JobScheduler::Config config;
    config.polling_interval_seconds = 10;
    config.pre_execute_delay_ms = 500;

    JobScheduler scheduler(&db, config);
    ASSERT_EQ(scheduler.start(&ctx), Status::OK);

    ID run_id;
    ASSERT_EQ(scheduler.executeJobNow(job, run_id, &ctx), Status::OK);

    bool saw_pending = false;
    auto pending_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    while (std::chrono::steady_clock::now() < pending_deadline) {
        CatalogManager::JobRunInfo run;
        if (catalog->getJobRun(run_id, run, &ctx) == Status::OK) {
            if (run.state == CatalogManager::JobRunState::PENDING) {
                saw_pending = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(saw_pending);

    std::vector<CatalogManager::JobRunInfo> runs;
    ASSERT_TRUE(waitForJobRuns(catalog, job_id, 1, 10000, &runs));

    CatalogManager::JobRunInfo final_run;
    auto transition_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(10000);
    while (std::chrono::steady_clock::now() < transition_deadline) {
        ASSERT_EQ(catalog->getJobRun(run_id, final_run, &ctx), Status::OK);
        if (final_run.state != CatalogManager::JobRunState::PENDING) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_NE(final_run.state, CatalogManager::JobRunState::PENDING);

    scheduler.stop();
    db.close();
}

TEST(JobSchedulerCancellation, RequestCancelInterruptsRun) {
    testing::TestDatabaseFile db_file("test_scheduler_cancel_request");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    uint64_t scheduled_time = nowMs();
    CatalogManager::JobInfo job = buildSimpleJob(
        "cancel_request", catalog->getSystemUserId(&ctx), scheduled_time);

    ID job_id;
    ASSERT_EQ(catalog->createJob(job, job_id, &ctx), Status::OK);
    job.job_id = job_id;

    JobScheduler::Config config;
    config.polling_interval_seconds = 10;
    config.pre_execute_delay_ms = 500;

    JobScheduler scheduler(&db, config);
    ASSERT_EQ(scheduler.start(&ctx), Status::OK);

    ID run_id;
    ASSERT_EQ(scheduler.executeJobNow(job, run_id, &ctx), Status::OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(scheduler.requestCancelRun(run_id, &ctx), Status::OK);

    std::vector<CatalogManager::JobRunInfo> runs;
    ASSERT_TRUE(waitForJobRuns(catalog, job_id, 1, 10000, &runs));

    CatalogManager::JobRunInfo latest;
    ASSERT_EQ(catalog->getJobRun(run_id, latest, &ctx), Status::OK);
    EXPECT_EQ(latest.state, CatalogManager::JobRunState::CANCELLED);

    scheduler.stop();
    db.close();
}

TEST(JobSchedulerResultData, PersistsResultPayloads) {
    testing::TestDatabaseFile db_file("test_scheduler_result_data");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    CatalogManager::JobInfo job = buildSimpleJob(
        "result_data_job", catalog->getSystemUserId(&ctx), nowMs());
    ID job_id;
    ASSERT_EQ(catalog->createJob(job, job_id, &ctx), Status::OK);

    CatalogManager::JobRunInfo run;
    run.job_id = job_id;
    run.scheduled_time = nowMs();
    run.started_at = run.scheduled_time;
    run.completed_at = run.scheduled_time;
    run.state = CatalogManager::JobRunState::COMPLETED;
    run.result_message = "ok";
    run.result_data = {0x01, 0x02, 0x03};

    ID run_id;
    ASSERT_EQ(catalog->createJobRun(run, run_id, &ctx), Status::OK);

    CatalogManager::JobRunInfo loaded;
    ASSERT_EQ(catalog->getJobRun(run_id, loaded, &ctx), Status::OK);
    EXPECT_EQ(loaded.result_data, run.result_data);

    run.job_run_id = run_id;
    run.result_data = {0x10, 0x20};
    ASSERT_EQ(catalog->updateJobRun(run, &ctx), Status::OK);

    CatalogManager::JobRunInfo updated;
    ASSERT_EQ(catalog->getJobRun(run_id, updated, &ctx), Status::OK);
    EXPECT_EQ(updated.result_data, run.result_data);

    db.close();
}

TEST(JobSchedulerDependencies, DependentJobWaitsForCompletion) {
    testing::TestDatabaseFile db_file("test_scheduler_dependencies");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    ID system_user = catalog->getSystemUserId(&ctx);
    uint64_t scheduled_time = nowMs();

    auto parent_job = buildSimpleJob("dep_parent", system_user, scheduled_time);
    auto child_job = buildSimpleJob("dep_child", system_user, scheduled_time);

    ID parent_id;
    ID child_id;
    ASSERT_EQ(catalog->createJob(parent_job, parent_id, &ctx), Status::OK);
    ASSERT_EQ(catalog->createJob(child_job, child_id, &ctx), Status::OK);
    ASSERT_EQ(catalog->addJobDependencies(child_id, {parent_id}, &ctx), Status::OK);

    JobScheduler::Config config;
    config.polling_interval_seconds = 1;
    config.pre_execute_delay_ms = 500;

    JobScheduler scheduler(&db, config);
    ASSERT_EQ(scheduler.start(&ctx), Status::OK);

    std::vector<CatalogManager::JobRunInfo> parent_runs;
    ASSERT_TRUE(waitForJobRuns(catalog, parent_id, 1, 10000, &parent_runs));
    CatalogManager::JobRunInfo parent_final;
    auto parent_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(10000);
    while (std::chrono::steady_clock::now() < parent_deadline) {
        ASSERT_EQ(catalog->getJobRun(parent_runs.front().job_run_id, parent_final, &ctx), Status::OK);
        if (parent_final.state != CatalogManager::JobRunState::PENDING &&
            parent_final.state != CatalogManager::JobRunState::RUNNING) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_EQ(parent_final.state, CatalogManager::JobRunState::COMPLETED);

    std::vector<CatalogManager::JobRunInfo> child_runs;
    ASSERT_TRUE(waitForJobRuns(catalog, child_id, 1, 10000, &child_runs));
    CatalogManager::JobRunInfo child_final;
    auto child_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(10000);
    while (std::chrono::steady_clock::now() < child_deadline) {
        ASSERT_EQ(catalog->getJobRun(child_runs.front().job_run_id, child_final, &ctx), Status::OK);
        if (child_final.state != CatalogManager::JobRunState::PENDING &&
            child_final.state != CatalogManager::JobRunState::RUNNING) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_EQ(child_final.state, CatalogManager::JobRunState::COMPLETED);
    EXPECT_GE(child_final.started_at, parent_final.completed_at);

    scheduler.stop();
    db.close();
}

TEST(JobSchedulerRuntimeToggle, DisableStopsAndEnableResumesRuns) {
    auto& cfg = Config::getInstance();
    std::string prev_enabled = cfg.getString("scheduler", "enabled", "true");
    std::string prev_poll = cfg.getString("scheduler", "polling_interval_seconds", "10");
    std::string prev_max = cfg.getString("scheduler", "max_jobs_per_tick", "16");

    cfg.set("scheduler", "enabled", "true");
    cfg.set("scheduler", "polling_interval_seconds", "1");
    cfg.set("scheduler", "max_jobs_per_tick", "8");

    testing::TestDatabaseFile db_file("test_scheduler_toggle");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    ID system_user = catalog->getSystemUserId(&ctx);
    auto job = buildSimpleJob("toggle_run_enabled", system_user, nowMs());
    ID job_id;
    ASSERT_EQ(catalog->createJob(job, job_id, &ctx), Status::OK);

    EXPECT_TRUE(waitForJobRuns(catalog, job_id, 1, 10000));

    cfg.set("scheduler", "enabled", "false");
    ASSERT_EQ(db.applySchedulerConfig(&ctx), Status::OK);

    auto job_disabled = buildSimpleJob("toggle_run_disabled", system_user, nowMs());
    ID disabled_id;
    ASSERT_EQ(catalog->createJob(job_disabled, disabled_id, &ctx), Status::OK);

    EXPECT_FALSE(waitForJobRuns(catalog, disabled_id, 1, 2000));

    cfg.set("scheduler", "enabled", "true");
    ASSERT_EQ(db.applySchedulerConfig(&ctx), Status::OK);

    EXPECT_TRUE(waitForJobRuns(catalog, disabled_id, 1, 10000));

    db.close();

    cfg.set("scheduler", "enabled", prev_enabled);
    cfg.set("scheduler", "polling_interval_seconds", prev_poll);
    cfg.set("scheduler", "max_jobs_per_tick", prev_max);
}

TEST(JobSchedulerTimeout, MarksRunFailedAfterTimeout) {
    auto& cfg = Config::getInstance();
    std::string prev_enabled = cfg.getString("scheduler", "enabled", "true");
    cfg.set("scheduler", "enabled", "false");

    testing::TestDatabaseFile db_file("test_scheduler_timeout");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    ID system_user = catalog->getSystemUserId(&ctx);
    auto job = buildSimpleJob("timeout_job", system_user, nowMs());
    job.job_type = CatalogManager::JobType::EXTERNAL;
    job.job_sql.clear();
    job.external_command = "/bin/sleep 2";
    job.timeout_seconds = 1;

    ID job_id;
    ASSERT_EQ(catalog->createJob(job, job_id, &ctx), Status::OK);

    JobScheduler::Config config;
    config.polling_interval_seconds = 1;
    config.pre_execute_delay_ms = 1500;
    config.external_jobs_enabled = true;
    config.external_working_dir = "/tmp";
    config.external_allowed_commands = {"/bin/sleep"};
    config.external_env_allowlist = {"PATH"};

    JobScheduler scheduler(&db, config);
    ASSERT_EQ(scheduler.start(&ctx), Status::OK);

    std::vector<CatalogManager::JobRunInfo> runs;
    ASSERT_TRUE(waitForJobRuns(catalog, job_id, 1, 10000, &runs));

    CatalogManager::JobRunInfo updated;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(10000);
    while (std::chrono::steady_clock::now() < deadline) {
        ASSERT_EQ(catalog->getJobRun(runs.front().job_run_id, updated, &ctx), Status::OK);
        if (updated.state != CatalogManager::JobRunState::RUNNING &&
            updated.state != CatalogManager::JobRunState::PENDING) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_NE(updated.state, CatalogManager::JobRunState::RUNNING);
    EXPECT_NE(updated.state, CatalogManager::JobRunState::PENDING);
    EXPECT_EQ(updated.state, CatalogManager::JobRunState::FAILED);
    EXPECT_NE(updated.result_message.find("timed out"), std::string::npos);

    scheduler.stop();
    db.close();

    cfg.set("scheduler", "enabled", prev_enabled);
}

TEST(JobSchedulerGovernance, ProcedureRunRespectsWorkloadAdmissionPolicy) {
    auto& cfg = Config::getInstance();
    std::string prev_enabled = cfg.getString("scheduler", "enabled", "true");
    cfg.set("scheduler", "enabled", "false");

    testing::TestDatabaseFile db_file("test_scheduler_governance");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(catalog->getSchema("PUBLIC", schema_info, &ctx), Status::OK) << ctx.message;
    const ID system_user = catalog->getSystemUserId(&ctx);

    std::unique_ptr<ConnectionContext> ddl_conn;
    ASSERT_EQ(db.connect(ddl_conn, &ctx), Status::OK) << ctx.message;
    ASSERT_NE(ddl_conn, nullptr);
    ddl_conn->setCurrentUser(system_user, true);
    ddl_conn->setCurrentSchemaId(schema_info.schema_id);
    ddl_conn->set_current_schema("PUBLIC");
    ddl_conn->set_search_path({"PUBLIC"});
    ConnectionContext::setCurrent(ddl_conn.get());

    sblr::QueryCompilerV3 compiler(&db);
    sblr::Executor executor(&db);
    executor.setConnectionContext(ddl_conn.get());

    auto create_procedure = compiler.compile(
        "CREATE PROCEDURE proc_scheduler_governed AS "
        "BEGIN "
        "RETURN; "
        "END");
    ASSERT_TRUE(create_procedure.success())
        << (create_procedure.errors().empty()
                ? "Compilation failed"
                : create_procedure.errors().front());
    auto create_result = executor.execute(create_procedure.bytecode());
    ASSERT_TRUE(create_result.success()) << create_result.error();

    CatalogManager::ProcedureInfo proc{};
    std::vector<CatalogManager::ProcedureInfo> procedures;
    ASSERT_EQ(catalog->listProcedures(procedures, &ctx), Status::OK) << ctx.message;
    auto proc_it = std::find_if(
        procedures.begin(), procedures.end(), [](const CatalogManager::ProcedureInfo& candidate) {
            return candidate.name == "proc_scheduler_governed";
        });
    ASSERT_NE(proc_it, procedures.end());
    proc = *proc_it;

    ConnectionContext::setCurrent(nullptr);
    ddl_conn.reset();

    CatalogManager::WorkloadClassCatalogInfo klass{};
    klass.class_id = generateUuidV7();
    klass.class_name = "wl_scheduler";
    klass.match_kind = CatalogManager::WorkloadMatchKind::RESOURCE_TAG;
    klass.match_text = "scheduler";
    klass.priority = 5;
    ASSERT_EQ(catalog->upsertWorkloadClassCatalogEntry(klass, &ctx), Status::OK) << ctx.message;

    CatalogManager::AdmissionPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.policy_name = "ap_scheduler";
    policy.max_concurrent_sessions = 8;
    policy.max_concurrent_queries = 1;
    policy.max_queue_depth = 0;
    policy.reject_mode = CatalogManager::AdmissionRejectMode::REJECT;
    policy.queue_timeout_ms = 0;
    ASSERT_EQ(catalog->upsertAdmissionPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::AdmissionBindingCatalogInfo binding{};
    binding.binding_id = generateUuidV7();
    binding.policy_id = policy.policy_id;
    binding.target_kind = CatalogManager::AdmissionTargetKind::WORKLOAD_CLASS;
    binding.class_id = klass.class_id;
    binding.priority = 1;
    ASSERT_EQ(catalog->upsertAdmissionBindingCatalogEntry(binding, &ctx), Status::OK) << ctx.message;

    std::unique_ptr<ConnectionContext> gate_conn;
    ASSERT_EQ(db.connect(gate_conn, &ctx), Status::OK) << ctx.message;
    ASSERT_NE(gate_conn, nullptr);
    ConnectionContext::setCurrent(gate_conn.get());
    ASSERT_EQ(gate_conn->initialize(&ctx), Status::OK) << ctx.message;
    ConnectionContext::setCurrent(nullptr);
    gate_conn->setCurrentUser(system_user, true);
    gate_conn->setCurrentSchemaId(schema_info.schema_id);
    gate_conn->set_current_schema("PUBLIC");
    gate_conn->set_search_path({"PUBLIC"});
    gate_conn->setSessionVariable("RESOURCE_TAG", "scheduler");

    WorkloadGovernance::QueryDescriptor descriptor;
    descriptor.connection = gate_conn.get();
    descriptor.sql = "CALL proc_scheduler_governed";
    descriptor.schema_name = "PUBLIC";
    descriptor.resource_tag = "scheduler";

    WorkloadGovernance::AdmissionLease gate_lease;
    auto gate_decision = db.workload_governance()->acquire(descriptor, gate_lease, &ctx);
    ASSERT_TRUE(gate_decision.admitted) << gate_decision.detail;
    ASSERT_TRUE(gate_lease.active());

    std::vector<WorkloadGovernance::AdmissionStatusRow> rows;
    ASSERT_EQ(db.workload_governance()->snapshotAdmissionStatus(rows, &ctx), Status::OK)
        << ctx.message;
    auto row_it = std::find_if(
        rows.begin(), rows.end(), [](const WorkloadGovernance::AdmissionStatusRow& row) {
            return row.policy_name == "ap_scheduler";
        });
    ASSERT_NE(row_it, rows.end());
    EXPECT_EQ(row_it->active_queries, 1u);

    auto job = buildSimpleJob("governed_proc_job", system_user, nowMs());
    job.job_type = CatalogManager::JobType::PROCEDURE;
    job.job_sql.clear();
    job.procedure_uuid = proc.procedure_id;

    ID job_id;
    ASSERT_EQ(catalog->createJob(job, job_id, &ctx), Status::OK);
    job.job_id = job_id;

    JobScheduler::Config config;
    config.polling_interval_seconds = 1;
    config.pre_execute_delay_ms = 0;

    JobScheduler scheduler(&db, config);
    ASSERT_EQ(scheduler.start(&ctx), Status::OK);

    ID run_id;
    ASSERT_EQ(scheduler.executeJobNow(job, run_id, &ctx), Status::OK);

    CatalogManager::JobRunInfo run;
    ASSERT_TRUE(waitForJobRunState(catalog, run_id, CatalogManager::JobRunState::FAILED, 10000, &run));
    EXPECT_EQ(run.error_code, static_cast<int32_t>(Status::CONFIGURATION_LIMIT_EXCEEDED))
        << run.result_message;
    EXPECT_NE(run.result_message.find("Admission rejected by max_concurrent_queries"),
              std::string::npos)
        << run.result_message;

    scheduler.stop();
    gate_lease.release();
    gate_conn.reset();
    db.close();

    cfg.set("scheduler", "enabled", prev_enabled);
}

TEST(JobSchedulerRuntimeSql, AlterSystemAppliesSchedulerConfig) {
    auto& cfg = Config::getInstance();
    std::string prev_enabled = cfg.getString("scheduler", "enabled", "true");

    testing::TestDatabaseFile db_file("test_scheduler_alter_system");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    std::unique_ptr<ConnectionContext> conn_ctx;
    ASSERT_EQ(db.connect(conn_ctx, &ctx), Status::OK);
    ID system_user = db.catalog_manager()->getSystemUserId(&ctx);
    conn_ctx->setCurrentUser(system_user, true);
    ConnectionContext::setCurrent(conn_ctx.get());

    sblr::QueryCompilerV3 compiler(&db);
    sblr::Executor executor(&db);
    executor.setConnectionContext(conn_ctx.get());

    auto apply_sql = [&](const std::string& sql) {
        auto result = compiler.compile(sql);
        ASSERT_TRUE(result.success())
            << (result.errors().empty() ? "Compilation failed" : result.errors().front());
        auto exec_result = executor.execute(result.bytecode());
        ASSERT_TRUE(exec_result.success()) << exec_result.error();
    };

    apply_sql("ALTER SYSTEM SET scheduler.enabled = false");
    EXPECT_FALSE(cfg.getBool("scheduler", "enabled", true));

    apply_sql("ALTER SYSTEM SET scheduler.enabled = true");
    EXPECT_TRUE(cfg.getBool("scheduler", "enabled", false));

    ConnectionContext::setCurrent(nullptr);
    conn_ctx.reset();
    db.close();

    cfg.set("scheduler", "enabled", prev_enabled);
}

}  // namespace scratchbird::core
