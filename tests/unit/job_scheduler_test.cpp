#include "scratchbird/core/job_scheduler_utils.h"
#include "scratchbird/core/job_scheduler.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/testing/test_helpers.h"

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
    for (int i = 0; i < 200; ++i) {
        std::vector<CatalogManager::JobRunInfo> runs;
        if (catalog->listJobRuns(job_id, runs, &ctx) == Status::OK && !runs.empty()) {
            run = runs.front();
            found = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
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

}  // namespace scratchbird::core
