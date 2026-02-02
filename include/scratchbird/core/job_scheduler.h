/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <thread>
#include <unordered_set>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::sblr {
class Executor;
}

namespace scratchbird::core {

class Database;
class ErrorContext;

class JobScheduler {
public:
    enum class CatchUpPolicy : uint8_t {
        NONE = 0,
        LAST = 1,
        ALL = 2
    };

    struct Config {
        uint32_t polling_interval_seconds = 10;
        uint32_t max_jobs_per_tick = 16;
        uint32_t max_concurrent_jobs = 10;
        uint32_t job_timeout_seconds = 3600;
        uint32_t cron_fallback_seconds = 60;
        uint32_t pre_execute_delay_ms = 0;
        CatchUpPolicy catch_up_policy = CatchUpPolicy::LAST;
        bool external_jobs_enabled = false;
        std::string external_working_dir;
        std::vector<std::string> external_allowed_commands;
        std::vector<std::string> external_allowed_dirs;
        std::vector<std::string> external_env_allowlist;
        uint32_t external_output_max_bytes = 1024 * 1024;
        uint32_t external_kill_grace_ms = 2000;
        uint32_t external_cpu_time_limit_seconds = 0;
        uint64_t external_memory_max_bytes = 0;
    };

    explicit JobScheduler(Database* db);
    explicit JobScheduler(Database* db, const Config& config);
    ~JobScheduler();

    JobScheduler(const JobScheduler&) = delete;
    JobScheduler& operator=(const JobScheduler&) = delete;

    Status start(ErrorContext* ctx = nullptr);
    void stop();
    bool isRunning() const { return running_.load(); }
    void updateConfig(const Config& config);
    Status executeJobNow(const CatalogManager::JobInfo& job, ID& run_id,
                         ErrorContext* ctx = nullptr);
    Status requestCancelRun(const ID& run_id, ErrorContext* ctx = nullptr);

private:
    struct ActiveRunState {
        ID job_id{};
        std::shared_ptr<std::atomic<bool>> cancel_requested;
        std::shared_ptr<scratchbird::sblr::Executor> executor;
    };

    void runLoop();
    void processDueJobs();
    Status startJobExecution(const CatalogManager::JobInfo& job, uint64_t scheduled_time,
                             ID& run_id, ErrorContext* ctx);
    void executeJobRun(const CatalogManager::JobInfo& job, CatalogManager::JobRunInfo run,
                       const ID& run_id, uint64_t now_ms);
    Status createJobRunRecord(const CatalogManager::JobInfo& job, uint64_t scheduled_time,
                              CatalogManager::JobRunInfo& run, ID& run_id, ErrorContext& ctx);

    Database* db_;
    Config config_;

    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_requested_ = false;
    std::thread worker_;

    std::mutex active_mutex_;
    std::condition_variable active_cv_;
    std::unordered_set<ID, IDHash> active_jobs_;

    std::mutex active_runs_mutex_;
    std::unordered_map<ID, ActiveRunState, IDHash> active_runs_;

    std::mutex job_threads_mutex_;
    std::vector<std::thread> job_threads_;
};

}  // namespace scratchbird::core
