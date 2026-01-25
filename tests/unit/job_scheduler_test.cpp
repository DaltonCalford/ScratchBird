#include "scratchbird/core/job_scheduler_utils.h"
#include "scratchbird/core/job_scheduler.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "test_helpers.h"

#include "gtest/gtest.h"

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

CatalogManager::JobInfo buildSimpleJob(const std::string& name,
                                       const ID& system_user,
                                       uint64_t scheduled_time) {
    CatalogManager::JobInfo job;
    job.job_name = name;
    job.job_type = CatalogManager::JobType::SQL;
    job.job_sql = "SELECT 1";
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

    ASSERT_EQ(catalog->getJobByName("update_stats", job, &ctx), Status::OK);
    EXPECT_EQ(job.state, CatalogManager::JobState::ENABLED);
    EXPECT_EQ(job.schedule_kind, CatalogManager::ScheduleKind::CRON);

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

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

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
    config.polling_interval_seconds = 10;

    JobScheduler scheduler(&db, config);
    ASSERT_EQ(scheduler.start(&ctx), Status::OK);

    ID run_id;
    ASSERT_EQ(scheduler.executeJobNow(job, run_id, &ctx), Status::OK);

    std::vector<CatalogManager::JobRunInfo> runs;
    ASSERT_TRUE(waitForJobRuns(catalog, job_id, 1, 2000, &runs));

    bool completed = false;
    for (const auto& run : runs) {
        if (run.job_run_id == run_id) {
            completed = (run.state == CatalogManager::JobRunState::COMPLETED);
            break;
        }
    }
    EXPECT_TRUE(completed);

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
    auto pending_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
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
    ASSERT_TRUE(waitForJobRuns(catalog, job_id, 1, 2500, &runs));

    CatalogManager::JobRunInfo final_run;
    auto transition_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
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
    ASSERT_TRUE(waitForJobRuns(catalog, job_id, 1, 2000, &runs));

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
    ASSERT_TRUE(waitForJobRuns(catalog, parent_id, 1, 2000, &parent_runs));
    CatalogManager::JobRunInfo parent_final;
    auto parent_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
    while (std::chrono::steady_clock::now() < parent_deadline) {
        ASSERT_EQ(catalog->getJobRun(parent_runs.front().job_run_id, parent_final, &ctx), Status::OK);
        if (parent_final.state == CatalogManager::JobRunState::COMPLETED) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_EQ(parent_final.state, CatalogManager::JobRunState::COMPLETED);

    bool child_ran_early = false;
    auto early_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (std::chrono::steady_clock::now() < early_deadline) {
        std::vector<CatalogManager::JobRunInfo> child_runs;
        if (catalog->listJobRuns(child_id, child_runs, &ctx) == Status::OK &&
            !child_runs.empty()) {
            child_ran_early = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_FALSE(child_ran_early);

    std::vector<CatalogManager::JobRunInfo> child_runs;
    ASSERT_TRUE(waitForJobRuns(catalog, child_id, 1, 2500, &child_runs));

    CatalogManager::JobRunInfo child_final;
    auto child_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
    while (std::chrono::steady_clock::now() < child_deadline) {
        ASSERT_EQ(catalog->getJobRun(child_runs.front().job_run_id, child_final, &ctx), Status::OK);
        if (child_final.state != CatalogManager::JobRunState::PENDING &&
            child_final.started_at != 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_NE(child_final.state, CatalogManager::JobRunState::PENDING);
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

    EXPECT_TRUE(waitForJobRuns(catalog, job_id, 1, 1500));

    cfg.set("scheduler", "enabled", "false");
    ASSERT_EQ(db.applySchedulerConfig(&ctx), Status::OK);

    auto job_disabled = buildSimpleJob("toggle_run_disabled", system_user, nowMs());
    ID disabled_id;
    ASSERT_EQ(catalog->createJob(job_disabled, disabled_id, &ctx), Status::OK);

    EXPECT_FALSE(waitForJobRuns(catalog, disabled_id, 1, 800));

    cfg.set("scheduler", "enabled", "true");
    ASSERT_EQ(db.applySchedulerConfig(&ctx), Status::OK);

    EXPECT_TRUE(waitForJobRuns(catalog, disabled_id, 1, 1500));

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
    job.timeout_seconds = 1;

    ID job_id;
    ASSERT_EQ(catalog->createJob(job, job_id, &ctx), Status::OK);

    JobScheduler::Config config;
    config.polling_interval_seconds = 1;
    config.pre_execute_delay_ms = 1500;

    JobScheduler scheduler(&db, config);
    ASSERT_EQ(scheduler.start(&ctx), Status::OK);

    std::vector<CatalogManager::JobRunInfo> runs;
    ASSERT_TRUE(waitForJobRuns(catalog, job_id, 1, 2500, &runs));

    CatalogManager::JobRunInfo updated;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
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

    sblr::QueryCompilerV2 compiler(&db);
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
