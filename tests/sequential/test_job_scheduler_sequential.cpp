/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Job Scheduler Sequential Test Suite
 * 
 * This test wraps job scheduler tests that have timing dependencies
 * to run them sequentially, avoiding parallel execution conflicts.
 */

#include "scratchbird/core/job_scheduler_utils.h"
#include "scratchbird/core/job_scheduler.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/proc_array.h"
#include "test_helpers.h"

#include "gtest/gtest.h"

#include <chrono>
#include <thread>

using namespace scratchbird::core;
using scratchbird::testing::TestDatabaseFile;

namespace {

CatalogManager::JobInfo buildSimpleJob(const std::string& name,
                                       const ID& system_user,
                                       uint64_t scheduled_time) {
    CatalogManager::JobInfo job;
    job.job_name = name;
    // Engine SQL text execution is intentionally disabled for scheduler jobs.
    // Use a deterministic external no-op command for dependency ordering tests.
    job.job_type = CatalogManager::JobType::EXTERNAL;
    job.job_sql.clear();
    job.external_command = "/bin/true";
    job.schedule_kind = CatalogManager::ScheduleKind::AT;
    job.starts_at = scheduled_time;
    job.next_run_time = scheduled_time;
    return job;
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

} // namespace

/**
 * Sequential Job Scheduler Test Suite
 */
TEST(JobSchedulerSequentialSuite, DependentJobWaitsForCompletion)
{
    TestDatabaseFile db_file("test_scheduler_dependencies_seq");
    ErrorContext ctx;

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    ID system_user = catalog->getSystemUserId(&ctx);
    uint64_t scheduled_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto parent_job = buildSimpleJob("dep_parent_seq", system_user, scheduled_time);
    auto child_job = buildSimpleJob("dep_child_seq", system_user, scheduled_time);

    ID parent_id;
    ID child_id;
    ASSERT_EQ(catalog->createJob(parent_job, parent_id, &ctx), Status::OK);
    ASSERT_EQ(catalog->createJob(child_job, child_id, &ctx), Status::OK);
    ASSERT_EQ(catalog->addJobDependencies(child_id, {parent_id}, &ctx), Status::OK);

    JobScheduler::Config config;
    config.polling_interval_seconds = 1;
    config.pre_execute_delay_ms = 500;
    config.external_jobs_enabled = true;
    config.external_working_dir = "/tmp";
    config.external_allowed_commands = {"/bin/true"};
    config.external_env_allowlist = {"PATH"};

    JobScheduler scheduler(&db, config);
    ASSERT_EQ(scheduler.start(&ctx), Status::OK);

    std::vector<CatalogManager::JobRunInfo> parent_runs;
    ASSERT_TRUE(waitForJobRuns(catalog, parent_id, 1, 10000, &parent_runs));
    
    CatalogManager::JobRunInfo parent_final;
    auto parent_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(10000);
    while (std::chrono::steady_clock::now() < parent_deadline) {
        ASSERT_EQ(catalog->getJobRun(parent_runs.front().job_run_id, parent_final, &ctx), Status::OK);
        if (parent_final.state == CatalogManager::JobRunState::COMPLETED) {
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

TEST(JobSchedulerSequentialSuite, DISABLED_BatchCollectionTimeout)
{
    TestDatabaseFile db_file("test_scheduler_batch_timeout_seq");
    ErrorContext ctx;
    
    // Register backends first
    constexpr uint32_t kRequiredBackends = 10;
    uint32_t active_backends = 0;
    ProcArrayManager::getNumActiveBackends(&active_backends, &ctx);
    
    ProcArray* proc_array = ProcArrayManager::getInstance();
    uint32_t available = proc_array->max_backends - active_backends;
    std::vector<uint32_t> registered_proc_ids;
    
    for (uint32_t i = 0; i < std::min(kRequiredBackends, available); i++) {
        uint32_t proc_id;
        Status status = ProcArrayManager::registerBackend(&proc_id, &ctx);
        if (status == Status::OK) {
            registered_proc_ids.push_back(proc_id);
        }
    }

    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);

    TransactionManager* txn_mgr = db.transaction_manager();
    ASSERT_NE(txn_mgr, nullptr);

    txn_mgr->enableGroupCommit(true);
    txn_mgr->setGroupCommitTimeout(10000); // 10ms timeout

    const int num_commits = 5;
    std::vector<std::thread> threads;
    auto [start_group_commits, start_total_xids] = txn_mgr->getGroupCommitStats();

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_commits; i++)
    {
        threads.emplace_back([&, i]() {
            ErrorContext thread_ctx;
            uint32_t proc_id = registered_proc_ids[i % registered_proc_ids.size()];

            uint64_t xid;
            Status status = txn_mgr->beginTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);

            status = txn_mgr->commitTransaction(proc_id, xid, &thread_ctx);
            ASSERT_EQ(status, Status::OK);
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    EXPECT_LT(elapsed_ms, 500);

    auto [group_commits, total_xids] = txn_mgr->getGroupCommitStats();
    EXPECT_GT(group_commits - start_group_commits, 0);
    EXPECT_GE(total_xids - start_total_xids, num_commits);

    db.close();
    
    // Cleanup backends
    for (uint32_t proc_id : registered_proc_ids) {
        ProcArrayManager::unregisterBackend(proc_id, &ctx);
    }
}
