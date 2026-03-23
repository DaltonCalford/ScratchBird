/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/job_scheduler.h"

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/job_scheduler_utils.h"
#include "scratchbird/core/posix_compat.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/core/workload_governance.h"
#include "scratchbird/sblr/executor.h"

#include <chrono>
#include <algorithm>
#include <thread>
#include <vector>
#include <unordered_set>
#include <cstdlib>
#include <cctype>
#include <system_error>
#include <filesystem>

#ifndef _WIN32
#include <poll.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#endif

namespace scratchbird::core {

namespace {

struct CurrentContextGuard {
    ConnectionContext* previous = nullptr;
    bool changed = false;

    explicit CurrentContextGuard(ConnectionContext* current)
        : previous(ConnectionContext::getCurrent())
    {
        if (current && current != previous) {
            ConnectionContext::setCurrent(current);
            changed = true;
        }
    }

    ~CurrentContextGuard() {
        if (changed) {
            ConnectionContext::setCurrent(previous);
        }
    }
};

bool isZeroId(const ID& id) {
    for (uint8_t byte : id.bytes) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
}

void logJobRunAudit(Database* db,
                    const CatalogManager::JobInfo& job,
                    const CatalogManager::JobRunInfo& run,
                    bool success,
                    const std::string& message) {
    if (!db) {
        return;
    }
    auto* audit_logger = db->audit_logger();
    if (!audit_logger) {
        return;
    }

    AuditEvent event;
    event.event_type = success ? AuditEventType::JOB_EXECUTED : AuditEventType::JOB_FAILED;
    event.user_id = job.created_by_user_uuid;
    event.object_type = "JOB";
    event.object_name = job.job_name;
    event.object_id = job.job_id;
    event.success = success;

    if (!isZeroId(event.user_id)) {
        CatalogManager::UserInfo user_info;
        ErrorContext user_ctx;
        if (db->catalog_manager()->getUser(event.user_id, user_info, &user_ctx) == Status::OK) {
            event.username = user_info.username;
        }
    }

    std::string details = "job_run_uuid=" + run.job_run_id.toString();
    if (!message.empty()) {
        details += " message=" + message;
    }
    event.details = details;

    ErrorContext audit_ctx;
    audit_logger->logEvent(event, &audit_ctx);
}

std::vector<std::string> splitCommand(const std::string& command) {
    std::vector<std::string> out;
    std::string current;
    bool in_single = false;
    bool in_double = false;
    bool escape = false;

    for (char ch : command) {
        if (escape) {
            current.push_back(ch);
            escape = false;
            continue;
        }
        if (ch == '\\') {
            escape = true;
            continue;
        }
        if (ch == '\'' && !in_double) {
            in_single = !in_single;
            continue;
        }
        if (ch == '"' && !in_single) {
            in_double = !in_double;
            continue;
        }
        if (!in_single && !in_double && std::isspace(static_cast<unsigned char>(ch)) != 0) {
            if (!current.empty()) {
                out.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }

    if (!current.empty()) {
        out.push_back(current);
    }
    return out;
}

std::string basenameOf(const std::string& value) {
    size_t pos = value.find_last_of('/');
    if (pos == std::string::npos) {
        return value;
    }
    return value.substr(pos + 1);
}

bool commandAllowed(const std::vector<std::string>& args,
                    const std::vector<std::string>& allowed_commands,
                    const std::vector<std::string>& allowed_dirs) {
    if (allowed_commands.empty() && allowed_dirs.empty()) {
        return false;
    }
    if (args.empty()) {
        return false;
    }

    const std::string& command = args[0];
    if (command.find('/') == std::string::npos) {
        return false;
    }
    std::string base = basenameOf(command);
    for (const auto& allowed : allowed_commands) {
        if (allowed == command || allowed == base) {
            return true;
        }
    }
    for (const auto& dir : allowed_dirs) {
        if (dir.empty()) {
            continue;
        }
        if (command.rfind(dir, 0) != 0) {
            continue;
        }
        if (command.size() == dir.size() || command[dir.size()] == '/') {
            return true;
        }
    }
    return false;
}

bool validateWorkingDir(const std::string& working_dir, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::exists(working_dir, ec)) {
        error = "External working directory not found";
        return false;
    }
    if (!std::filesystem::is_directory(working_dir, ec)) {
        error = "External working directory is not a directory";
        return false;
    }
    return true;
}

bool buildExternalEnv(Database* db,
                      const CatalogManager::JobInfo& job,
                      const CatalogManager::JobRunInfo& run,
                      const std::vector<std::string>& allowlist,
                      std::vector<std::string>& env_out,
                      std::string& error) {
    if (allowlist.empty()) {
        error = "External environment allowlist not configured";
        return false;
    }

    std::unordered_set<std::string> seen;
    auto add_env = [&](const std::string& key, const std::string& value) {
        if (key.empty()) {
            return;
        }
        if (seen.insert(key).second) {
            env_out.push_back(key + "=" + value);
        }
    };

    add_env("SB_JOB_UUID", job.job_id.toString());
    add_env("SB_JOB_RUN_UUID", run.job_run_id.toString());
    add_env("SB_JOB_NAME", job.job_name);

    auto* catalog = db ? db->catalog_manager() : nullptr;
    for (const auto& entry : allowlist) {
        if (entry.rfind("SECRET:", 0) == 0) {
            std::string key = entry.substr(7);
            if (key.empty()) {
                error = "Job secret key is empty";
                return false;
            }
            if (!catalog) {
                error = "Catalog manager not available";
                return false;
            }
            std::string secret_value;
            ErrorContext ctx;
            Status status = catalog->getJobSecret(job.job_id, key, secret_value, &ctx);
            if (status != Status::OK) {
                error = "Job secret not found: " + key;
                return false;
            }
            add_env("SB_JOB_SECRET_" + key, secret_value);
            continue;
        }

        std::string env_key = entry;
        if (entry.rfind("ENV:", 0) == 0) {
            env_key = entry.substr(4);
        }
        if (env_key.empty()) {
            continue;
        }
        const char* val = std::getenv(env_key.c_str());
        if (val) {
            add_env(env_key, val);
        }
    }

    return true;
}

struct ExternalRunResult {
    int exit_code = -1;
    bool timed_out = false;
    bool cancelled = false;
    std::string output;
};

ExternalRunResult runExternalCommand(const std::vector<std::string>& args,
                                     const std::string& working_dir,
                                     const std::vector<std::string>& env_entries,
                                     uint32_t output_cap,
                                     uint32_t timeout_seconds,
                                     uint32_t kill_grace_ms,
                                     uint32_t cpu_limit_seconds,
                                     uint64_t memory_max_bytes,
                                     const std::shared_ptr<std::atomic<bool>>& cancel_flag,
                                     const ClockControl* clock_control) {
    ExternalRunResult result;

#ifdef _WIN32
    (void)args;
    (void)working_dir;
    (void)env_entries;
    (void)output_cap;
    (void)timeout_seconds;
    (void)kill_grace_ms;
    (void)cpu_limit_seconds;
    (void)memory_max_bytes;
    (void)cancel_flag;
    (void)clock_control;
    result.output = "External jobs are not supported on Windows in this cycle";
    return result;
#else
    if (args.empty()) {
        return result;
    }

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        result.output = "Failed to create output pipe";
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        result.output = "Failed to spawn external job";
        return result;
    }
    if (pid == 0) {
        if (!working_dir.empty()) {
            (void)chdir(working_dir.c_str());
        }

        if (cpu_limit_seconds > 0) {
            struct rlimit limit;
            limit.rlim_cur = cpu_limit_seconds;
            limit.rlim_max = cpu_limit_seconds;
            if (setrlimit(RLIMIT_CPU, &limit) != 0) {
                _exit(127);
            }
        }
        if (memory_max_bytes > 0) {
            struct rlimit limit;
            limit.rlim_cur = memory_max_bytes;
            limit.rlim_max = memory_max_bytes;
            if (setrlimit(RLIMIT_AS, &limit) != 0) {
                _exit(127);
            }
        }

        std::vector<std::string> env_store = env_entries;
        std::vector<char*> envp;
        envp.reserve(env_store.size() + 1);
        for (auto& entry : env_store) {
            envp.push_back(entry.data());
        }
        envp.push_back(nullptr);

        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execve(argv[0], argv.data(), envp.data());
        _exit(127);
    }

    close(pipefd[1]);
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);

    auto monotonic_now = [&]() {
        return clock_control ? clock_control->monotonicNow() : std::chrono::steady_clock::now();
    };

    auto start = monotonic_now();
    bool done = false;
    bool sent_term = false;
    auto term_time = start;

    while (!done) {
        if (cancel_flag && cancel_flag->load(std::memory_order_acquire)) {
            result.cancelled = true;
        }

        if (!result.cancelled && timeout_seconds > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                monotonic_now() - start).count();
            if (elapsed > static_cast<int64_t>(timeout_seconds)) {
                result.timed_out = true;
            }
        }

        if ((result.cancelled || result.timed_out) && !sent_term) {
            kill(pid, SIGTERM);
            sent_term = true;
            term_time = monotonic_now();
        }

        if (sent_term) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                monotonic_now() - term_time).count();
            if (elapsed > static_cast<int64_t>(kill_grace_ms)) {
                kill(pid, SIGKILL);
            }
        }

        pollfd pfd{};
        pfd.fd = pipefd[0];
        pfd.events = POLLIN;
        int poll_res = poll(&pfd, 1, 50);
        if (poll_res > 0 && (pfd.revents & POLLIN)) {
            char buf[4096];
            ssize_t n = read(pipefd[0], buf, sizeof(buf));
            if (n > 0) {
                size_t to_copy = static_cast<size_t>(n);
                if (result.output.size() + to_copy > output_cap) {
                    to_copy = output_cap > result.output.size()
                        ? output_cap - result.output.size()
                        : 0;
                }
                if (to_copy > 0) {
                    result.output.append(buf, buf + to_copy);
                }
                if (result.output.size() >= output_cap && !sent_term) {
                    kill(pid, SIGTERM);
                    sent_term = true;
                    term_time = monotonic_now();
                }
            }
        }

        int status = 0;
        pid_t wait_res = waitpid(pid, &status, WNOHANG);
        if (wait_res == pid) {
            if (WIFEXITED(status)) {
                result.exit_code = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                result.exit_code = 128 + WTERMSIG(status);
            }
            done = true;
        }
    }

    close(pipefd[0]);
    return result;
#endif
}

}  // namespace

JobScheduler::JobScheduler(Database* db)
    : JobScheduler(db, Config{})
{
}

JobScheduler::JobScheduler(Database* db, const Config& config)
    : db_(db)
    , config_(config)
    , clock_control_(createDefaultClockControl())
{
}

JobScheduler::~JobScheduler() {
    stop();
}

Status JobScheduler::start(ErrorContext* ctx) {
    if (!db_ || !db_->catalog_manager()) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "JobScheduler requires catalog manager");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (running_.load(std::memory_order_acquire)) {
        return Status::OK;
    }

    stop_requested_ = false;
    running_.store(true, std::memory_order_release);
    try {
        worker_ = std::thread(&JobScheduler::runLoop, this);
    } catch (const std::system_error& e) {
        running_.store(false, std::memory_order_release);
        stop_requested_ = true;
        std::string msg = "Failed to start scheduler thread: ";
        msg += e.what();
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, msg.c_str());
        return Status::INTERNAL_ERROR;
    }
    return Status::OK;
}

void JobScheduler::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_.load(std::memory_order_acquire)) {
            return;
        }
        stop_requested_ = true;
        cv_.notify_all();
    }

    if (worker_.joinable()) {
        worker_.join();
    }
    running_.store(false, std::memory_order_release);

    std::unique_lock<std::mutex> lock(active_mutex_);
    active_cv_.wait(lock, [&]() { return active_jobs_.empty(); });

    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> thread_lock(job_threads_mutex_);
        threads.swap(job_threads_);
    }
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void JobScheduler::updateConfig(const Config& config) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        cv_.notify_all();
    }
}

void JobScheduler::runLoop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_requested_) {
                break;
            }
        }

        processDueJobs();

        std::unique_lock<std::mutex> lock(mutex_);
        const uint32_t polling_interval_seconds = config_.polling_interval_seconds;
        auto deadline = (clock_control_ ? clock_control_->monotonicNow() : std::chrono::steady_clock::now()) +
                        std::chrono::seconds(polling_interval_seconds);
        cv_.wait_until(lock, deadline, [&]() {
            return stop_requested_;
        });
        if (stop_requested_) {
            break;
        }
    }
}

void JobScheduler::processDueJobs() {
    auto now_ms = [&]() -> uint64_t {
        if (clock_control_) {
            return clock_control_->realtimeNowMs();
        }
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    };

    Config cfg;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_requested_) {
            return;
        }
        cfg = config_;
    }

    uint64_t now = now_ms();
    std::vector<CatalogManager::JobInfo> due_jobs;
    ErrorContext ctx;
    auto status = db_->catalog_manager()->listDueJobs(now, due_jobs, &ctx);
    if (status != Status::OK) {
        return;
    }

    auto& metrics = ScratchBirdMetrics::getInstance();
    metrics.initialize();
    if (metrics.scheduler_queue_depth) {
        metrics.scheduler_queue_depth->set(static_cast<double>(due_jobs.size()));
    }

    uint32_t handled = 0;
    uint32_t max_jobs = cfg.max_jobs_per_tick;
    if (cfg.max_concurrent_jobs > 0) {
        max_jobs = std::min(max_jobs, cfg.max_concurrent_jobs);
    }
    for (auto& job : due_jobs) {
        if (handled >= max_jobs) {
            break;
        }

        if (job.schedule_kind == CatalogManager::ScheduleKind::CRON &&
            job.next_run_time == 0) {
            uint64_t next_cron = detail::computeNextCronRunMsWithTimezone(
                job.cron_expression, now, job.schedule_tz);
            if (next_cron > now) {
                job.next_run_time = next_cron;
                db_->catalog_manager()->updateJob(job, &ctx);
                continue;
            }
        }

        if (cfg.catch_up_policy == CatchUpPolicy::NONE &&
            job.next_run_time < now &&
            job.schedule_kind != CatalogManager::ScheduleKind::AT) {
            if (job.schedule_kind == CatalogManager::ScheduleKind::EVERY &&
                job.interval_seconds > 0) {
                job.next_run_time = now + static_cast<uint64_t>(job.interval_seconds) * 1000;
                db_->catalog_manager()->updateJob(job, &ctx);
                continue;
            }
            if (job.schedule_kind == CatalogManager::ScheduleKind::CRON) {
                uint64_t next_cron = detail::computeNextCronRunMsWithTimezone(
                    job.cron_expression, now, job.schedule_tz);
                job.next_run_time = next_cron;
                db_->catalog_manager()->updateJob(job, &ctx);
                continue;
            }
        }

        std::vector<CatalogManager::JobDependencyInfo> deps;
        status = db_->catalog_manager()->listJobDependencies(job.job_id, deps, &ctx);
        if (status != Status::OK) {
            continue;
        }

        uint64_t scheduled_time = job.next_run_time == 0 ? now : job.next_run_time;
        uint64_t window_start = 0;
        uint64_t window_end = scheduled_time;
        if (job.schedule_kind == CatalogManager::ScheduleKind::EVERY &&
            job.interval_seconds > 0) {
            uint64_t interval_ms = static_cast<uint64_t>(job.interval_seconds) * 1000;
            window_start = scheduled_time > interval_ms ? scheduled_time - interval_ms : 0;
        } else if (job.schedule_kind == CatalogManager::ScheduleKind::CRON) {
            window_start = detail::computePreviousCronRunMsWithTimezone(
                job.cron_expression, scheduled_time, job.schedule_tz);
        } else if (job.schedule_kind == CatalogManager::ScheduleKind::AT) {
            if (now > window_end) {
                window_end = now;
            }
        }

        bool deps_ok = true;
        for (const auto& dep : deps) {
            std::vector<CatalogManager::JobRunInfo> runs;
            if (db_->catalog_manager()->listJobRuns(dep.depends_on_job_id, runs, &ctx) != Status::OK) {
                deps_ok = false;
                break;
            }
            if (!detail::dependencySatisfiedForWindow(runs, window_start, window_end)) {
                deps_ok = false;
                break;
            }
        }

        if (!deps_ok) {
            continue;
        }

        ID run_id{};
        ErrorContext run_ctx;
        if (startJobExecution(job, scheduled_time, run_id, &run_ctx) != Status::OK) {
            continue;
        }

        if (cfg.catch_up_policy == CatchUpPolicy::ALL &&
            job.schedule_kind != CatalogManager::ScheduleKind::AT) {
            uint64_t next_time = 0;
            if (job.schedule_kind == CatalogManager::ScheduleKind::EVERY &&
                job.interval_seconds > 0) {
                next_time = scheduled_time +
                    static_cast<uint64_t>(job.interval_seconds) * 1000;
            } else if (job.schedule_kind == CatalogManager::ScheduleKind::CRON) {
                next_time = detail::computeNextCronRunMsWithTimezone(
                    job.cron_expression, scheduled_time, job.schedule_tz);
            }

            if (next_time > 0 && next_time != job.next_run_time) {
                job.next_run_time = next_time;
                db_->catalog_manager()->updateJob(job, &ctx);
            }
        }

        handled++;
    }
}

Status JobScheduler::createJobRunRecord(const CatalogManager::JobInfo& job,
                                        uint64_t scheduled_time,
                                        CatalogManager::JobRunInfo& run,
                                        ID& run_id,
                                        ErrorContext& ctx) {
    auto now_ms = []() -> uint64_t {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    };

    std::vector<CatalogManager::JobRunInfo> previous_runs;
    db_->catalog_manager()->listJobRuns(job.job_id, previous_runs, &ctx);
    const CatalogManager::JobRunInfo* latest_run = nullptr;
    for (const auto& candidate : previous_runs) {
        if (!latest_run || candidate.completed_at > latest_run->completed_at) {
            latest_run = &candidate;
        }
    }

    run = CatalogManager::JobRunInfo{};
    run.job_id = job.job_id;
    run.scheduled_time = scheduled_time;
    run.started_at = 0;
    run.state = CatalogManager::JobRunState::PENDING;
    if (latest_run && latest_run->state == CatalogManager::JobRunState::FAILED) {
        run.retry_count = latest_run->retry_count + 1;
    }

    auto status = db_->catalog_manager()->createJobRun(run, run_id, &ctx);
    if (status != Status::OK) {
        return status;
    }

    run.job_run_id = run_id;
    return Status::OK;
}

Status JobScheduler::startJobExecution(const CatalogManager::JobInfo& job,
                                       uint64_t scheduled_time,
                                       ID& run_id,
                                       ErrorContext* ctx) {
    ErrorContext local_ctx;
    ErrorContext& err_ctx = ctx ? *ctx : local_ctx;

    if (!running_.load()) {
        SET_ERROR_CONTEXT(&err_ctx, Status::INVALID_ARGUMENT, "Scheduler is not running");
        return Status::INVALID_ARGUMENT;
    }

    Config cfg;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cfg = config_;
    }

    size_t running_jobs_count = 0;
    {
        std::lock_guard<std::mutex> lock(active_mutex_);
        if (cfg.max_concurrent_jobs > 0 &&
            active_jobs_.size() >= cfg.max_concurrent_jobs) {
            SET_ERROR_CONTEXT(&err_ctx, Status::LOCK_CONFLICT, "Scheduler is at max concurrency");
            return Status::LOCK_CONFLICT;
        }
        if (active_jobs_.find(job.job_id) != active_jobs_.end()) {
            SET_ERROR_CONTEXT(&err_ctx, Status::LOCK_CONFLICT, "Job is already running");
            return Status::LOCK_CONFLICT;
        }
        active_jobs_.insert(job.job_id);
        running_jobs_count = active_jobs_.size();
    }

    auto& metrics = ScratchBirdMetrics::getInstance();
    metrics.initialize();
    if (metrics.scheduler_jobs_running) {
        metrics.scheduler_jobs_running->set(static_cast<double>(running_jobs_count));
    }

    CatalogManager::JobRunInfo run;
    auto status = createJobRunRecord(job, scheduled_time, run, run_id, err_ctx);
    if (status != Status::OK) {
        std::lock_guard<std::mutex> lock(active_mutex_);
        active_jobs_.erase(job.job_id);
        active_cv_.notify_all();
        return status;
    }

    {
        std::lock_guard<std::mutex> lock(active_runs_mutex_);
        ActiveRunState state;
        state.job_id = job.job_id;
        state.cancel_requested = std::make_shared<std::atomic<bool>>(false);
        active_runs_[run_id] = std::move(state);
    }

    std::thread job_thread([this, job, run, run_id, scheduled_time]() {
        executeJobRun(job, run, run_id, scheduled_time);
    });
    {
        std::lock_guard<std::mutex> lock(job_threads_mutex_);
        job_threads_.emplace_back(std::move(job_thread));
    }

    return Status::OK;
}

Status JobScheduler::executeJobNow(const CatalogManager::JobInfo& job, ID& run_id,
                                   ErrorContext* ctx) {
    auto now_ms = [&]() -> uint64_t {
        if (clock_control_) {
            return clock_control_->realtimeNowMs();
        }
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    };
    Status status = startJobExecution(job, now_ms(), run_id, ctx);
    if (status != Status::OK) {
        return status;
    }

    Config cfg;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cfg = config_;
    }

    if (job.job_type == CatalogManager::JobType::SQL && cfg.pre_execute_delay_ms == 0) {
        auto monotonic_now = [&]() {
            return clock_control_ ? clock_control_->monotonicNow() : std::chrono::steady_clock::now();
        };
        auto deadline = monotonic_now() + std::chrono::milliseconds(1500);
        while (monotonic_now() < deadline) {
            CatalogManager::JobRunInfo run;
            ErrorContext poll_ctx;
            if (db_ && db_->catalog_manager() &&
                db_->catalog_manager()->getJobRun(run_id, run, &poll_ctx) == Status::OK) {
                if (run.state != CatalogManager::JobRunState::RUNNING &&
                    run.state != CatalogManager::JobRunState::PENDING) {
                    break;
                }
            }
            if (clock_control_) {
                clock_control_->sleepFor(std::chrono::milliseconds(10));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    return Status::OK;
}

Status JobScheduler::requestCancelRun(const ID& run_id, ErrorContext* ctx) {
    ErrorContext local_ctx;
    ErrorContext& err_ctx = ctx ? *ctx : local_ctx;

    std::shared_ptr<scratchbird::sblr::Executor> executor;
    {
        std::lock_guard<std::mutex> lock(active_runs_mutex_);
        auto it = active_runs_.find(run_id);
        if (it == active_runs_.end()) {
            SET_ERROR_CONTEXT(&err_ctx, Status::NOT_FOUND, "Job run not active");
            return Status::NOT_FOUND;
        }
        if (it->second.cancel_requested) {
            it->second.cancel_requested->store(true, std::memory_order_release);
        }
        executor = it->second.executor;
    }

    if (executor) {
        executor->requestCancellation();
    }

    if (db_ && db_->catalog_manager()) {
        CatalogManager::JobRunInfo run;
        ErrorContext run_ctx;
        if (db_->catalog_manager()->getJobRun(run_id, run, &run_ctx) == Status::OK) {
            if (run.state == CatalogManager::JobRunState::RUNNING ||
                run.state == CatalogManager::JobRunState::PENDING) {
                run.state = CatalogManager::JobRunState::CANCELLED;
                run.completed_at = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                if (run.result_message.empty()) {
                    run.result_message = "Cancelled by user";
                }
                db_->catalog_manager()->updateJobRun(run, &run_ctx);
            }
        }
    }
    return Status::OK;
}

void JobScheduler::executeJobRun(const CatalogManager::JobInfo& job,
                                 CatalogManager::JobRunInfo run,
                                 const ID& run_id,
                                 uint64_t now) {
    auto now_ms = [&]() -> uint64_t {
        if (clock_control_) {
            return clock_control_->realtimeNowMs();
        }
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    };
    auto monotonic_now = [&]() {
        return clock_control_ ? clock_control_->monotonicNow() : std::chrono::steady_clock::now();
    };

    ErrorContext ctx;
    run.job_run_id = run_id;
    run.job_id = job.job_id;

    bool run_success = true;
    bool run_cancelled = false;
    std::string run_message;
    int32_t run_error_code = 0;
    int64_t rows_affected = 0;
    auto execution_start = monotonic_now();

    Config cfg;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cfg = config_;
    }

    if (cfg.pre_execute_delay_ms > 0) {
        if (clock_control_) {
            clock_control_->sleepFor(std::chrono::milliseconds(cfg.pre_execute_delay_ms));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg.pre_execute_delay_ms));
        }
    }

    std::shared_ptr<std::atomic<bool>> cancel_requested;
    {
        std::lock_guard<std::mutex> lock(active_runs_mutex_);
        auto it = active_runs_.find(run_id);
        if (it != active_runs_.end()) {
            cancel_requested = it->second.cancel_requested;
        }
    }

    CatalogManager::JobRunInfo current_run;
    if (db_->catalog_manager()->getJobRun(run_id, current_run, &ctx) == Status::OK &&
        current_run.state == CatalogManager::JobRunState::CANCELLED) {
        run_cancelled = true;
        run_message = "Cancelled by user";
    }
    if (cancel_requested && cancel_requested->load(std::memory_order_acquire)) {
        run_cancelled = true;
        run_message = "Cancelled by user";
    }

    if (!run_cancelled) {
        run.started_at = now_ms();
        run.state = CatalogManager::JobRunState::RUNNING;
        db_->catalog_manager()->updateJobRun(run, &ctx);
    }

    if (!run_cancelled) {
        if (job.job_type == CatalogManager::JobType::EXTERNAL) {
            if (!cfg.external_jobs_enabled) {
                run_success = false;
                run_message = "External jobs are disabled";
                run_error_code = -1;
            } else if (job.external_command.empty()) {
                run_success = false;
                run_message = "External job command is empty";
                run_error_code = -1;
            } else {
                std::vector<std::string> args = splitCommand(job.external_command);
                if (args.empty()) {
                    run_success = false;
                    run_message = "External job command is empty";
                    run_error_code = -1;
                } else if (!commandAllowed(args,
                                           cfg.external_allowed_commands,
                                           cfg.external_allowed_dirs)) {
                    run_success = false;
                    run_message = "External command not allowed or not absolute";
                    run_error_code = -1;
                } else if (cfg.external_working_dir.empty()) {
                    run_success = false;
                    run_message = "External working directory not configured";
                    run_error_code = -1;
                } else {
                    std::string dir_error;
                    if (!validateWorkingDir(cfg.external_working_dir, dir_error)) {
                        run_success = false;
                        run_message = dir_error;
                        run_error_code = -1;
                    } else {
                        std::vector<std::string> env_entries;
                        std::string env_error;
                        if (!buildExternalEnv(db_, job, run,
                                              cfg.external_env_allowlist,
                                              env_entries, env_error)) {
                            run_success = false;
                            run_message = env_error;
                            run_error_code = -1;
                        } else {
                            uint32_t timeout_seconds = job.timeout_seconds;
                            if (timeout_seconds == 0) {
                                timeout_seconds = cfg.job_timeout_seconds;
                            }
                        auto run_result = runExternalCommand(args,
                                                             cfg.external_working_dir,
                                                             env_entries,
                                                             cfg.external_output_max_bytes,
                                                             timeout_seconds,
                                                             cfg.external_kill_grace_ms,
                                                             cfg.external_cpu_time_limit_seconds,
                                                             cfg.external_memory_max_bytes,
                                                             cancel_requested,
                                                             clock_control_.get());
                            if (run_result.cancelled) {
                                run_cancelled = true;
                                run_message = "External job cancelled";
                                run_error_code = static_cast<int32_t>(Status::QUERY_CANCELED);
                            } else if (run_result.timed_out) {
                                run_success = false;
                                run_message = "External job timed out";
                                run_error_code = static_cast<int32_t>(Status::QUERY_CANCELED);
                            } else if (run_result.exit_code != 0) {
                                run_success = false;
                                run_message = "External job failed (exit " +
                                    std::to_string(run_result.exit_code) + ")";
                                if (!run_result.output.empty()) {
                                    run_message += ": " + run_result.output;
                                }
                                run_error_code = run_result.exit_code;
                            } else {
                                run_message = run_result.output;
                            }
                        }
                    }
                }
            }
        } else {
            std::unique_ptr<ConnectionContext> conn_ctx;
            Status conn_status = db_->connect(conn_ctx, &ctx);
            if (conn_status != Status::OK || !conn_ctx) {
                run_success = false;
                run_message = "Failed to create job connection context";
                run_error_code = static_cast<int32_t>(conn_status);
            } else {
                CurrentContextGuard ctx_guard(conn_ctx.get());

                conn_status = conn_ctx->initialize(&ctx);
                if (conn_status != Status::OK) {
                    run_success = false;
                    run_message = "Failed to initialize job connection context";
                    run_error_code = static_cast<int32_t>(conn_status);
                }

                if (run_success && !isZeroId(job.created_by_user_uuid)) {
                    CatalogManager::UserInfo user_info;
                    ErrorContext user_ctx;
                    if (db_->catalog_manager()->getUser(job.created_by_user_uuid, user_info, &user_ctx) == Status::OK) {
                        conn_ctx->setCurrentUser(user_info.user_id, user_info.is_superuser);
                        if (!isZeroId(user_info.default_schema_id)) {
                            conn_ctx->setCurrentSchemaId(user_info.default_schema_id);
                            CatalogManager::SchemaInfo schema_info;
                            if (db_->catalog_manager()->getSchema(user_info.default_schema_id,
                                                                  schema_info, &user_ctx) == Status::OK) {
                                conn_ctx->set_current_schema(schema_info.schema_name);
                                conn_ctx->set_search_path({schema_info.schema_name});
                            }
                        }
                    }
                }

                if (run_success && !isZeroId(job.run_as_role_uuid)) {
                    conn_ctx->setActiveRole(job.run_as_role_uuid);
                }

                if (run_success) {
                    conn_ctx->setSessionVariable("APPLICATION_NAME", "job_scheduler");
                    conn_ctx->setSessionVariable("RESOURCE_TAG", "scheduler");
                    conn_ctx->setSessionVariable("job_uuid", job.job_id.toString());
                    conn_ctx->setSessionVariable("job_run_uuid", run.job_run_id.toString());
                }

                std::string procedure_name;
                if (run_success && !isZeroId(job.procedure_uuid)) {
                    std::vector<CatalogManager::ProcedureInfo> procedures;
                    if (db_->catalog_manager()->listProcedures(procedures, &ctx) == Status::OK) {
                        for (const auto& proc : procedures) {
                            if (proc.procedure_id == job.procedure_uuid) {
                                procedure_name = proc.name;
                                break;
                            }
                        }
                    }
                }

                auto executor = std::make_shared<scratchbird::sblr::Executor>(db_);
                executor->setConnectionContext(conn_ctx.get());
                uint32_t timeout_seconds = job.timeout_seconds;
                if (timeout_seconds == 0) {
                    timeout_seconds = cfg.job_timeout_seconds;
                }
                if (timeout_seconds > 0) {
                    conn_ctx->set_statement_timeout(timeout_seconds);
                    auto limits = executor->getQueryLimits();
                    limits.max_execution_time_ms =
                        static_cast<uint64_t>(timeout_seconds) * 1000ULL;
                    executor->setQueryLimits(limits);
                }

                std::string governance_sql;
                if (!procedure_name.empty()) {
                    governance_sql = "CALL " + procedure_name;
                } else if (!job.job_sql.empty()) {
                    governance_sql = job.job_sql;
                } else if (!job.bytecode.empty()) {
                    governance_sql = "<stored-sblr-job>";
                }

                WorkloadGovernance::AdmissionLease admission_lease;
                if (db_->workload_governance() != nullptr) {
                    WorkloadGovernance::QueryDescriptor descriptor;
                    descriptor.connection = conn_ctx.get();
                    descriptor.sql = governance_sql;
                    descriptor.schema_name = conn_ctx->current_schema();
                    descriptor.client_app = "job_scheduler";
                    descriptor.resource_tag = "scheduler";

                    ErrorContext governance_ctx;
                    auto decision = db_->workload_governance()->acquire(
                        descriptor, admission_lease, &governance_ctx);
                    if (!decision.admitted) {
                        run_success = false;
                        run_message = decision.detail.empty()
                            ? (governance_ctx.message.empty()
                                ? "Workload governance rejected scheduled job"
                                : governance_ctx.message)
                            : decision.detail;
                        run_error_code = static_cast<int32_t>(decision.status);
                    }
                }

                if (run_success) {
                    std::lock_guard<std::mutex> lock(active_runs_mutex_);
                    auto it = active_runs_.find(run_id);
                    if (it != active_runs_.end()) {
                        it->second.executor = executor;
                    }
                }

                if (run_success) {
                    if (!procedure_name.empty()) {
                        auto exec_result = executor->callProcedureByName(procedure_name);
                        if (!exec_result.success()) {
                            run_success = false;
                            run_message = exec_result.error();
                            run_error_code = -1;
                        } else {
                            rows_affected = executor->getLastAffectedRows();
                        }
                    } else if (!job.bytecode.empty()) {
                        auto exec_result = executor->execute(job.bytecode);
                        if (!exec_result.success()) {
                            run_success = false;
                            run_message = exec_result.error();
                            run_error_code = static_cast<int32_t>(exec_result.status());
                        } else {
                            rows_affected = executor->getLastAffectedRows();
                        }
                    } else if (!job.job_sql.empty()) {
                        run_success = false;
                        run_message =
                            "SQL job requires stored SBLR bytecode; recreate job via the V3 parser";
                        run_error_code = static_cast<int32_t>(Status::NOT_SUPPORTED);
                    } else {
                        run_success = false;
                        run_message = "Job has no SQL to execute";
                        run_error_code = -1;
                    }
                }
            }
        }
    }

    if (!run_cancelled && cancel_requested &&
        cancel_requested->load(std::memory_order_acquire)) {
        run_cancelled = true;
        run_success = false;
        run_error_code = static_cast<int32_t>(Status::QUERY_CANCELED);
        if (run_message.empty()) {
            run_message = "Cancelled by user";
        }
    }

    uint32_t timeout_seconds = job.timeout_seconds;
    if (timeout_seconds == 0) {
        timeout_seconds = cfg.job_timeout_seconds;
    }
    if (!run_cancelled && run_success && timeout_seconds > 0) {
        auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                monotonic_now() - execution_start).count();
        if (elapsed_ms >= static_cast<int64_t>(timeout_seconds) * 1000) {
            run_success = false;
            run_error_code = static_cast<int32_t>(Status::QUERY_CANCELED);
            auto elapsed_sec = elapsed_ms / 1000;
            run_message = "Job timed out after " + std::to_string(elapsed_sec) + " seconds";
        }
    }

    CatalogManager::JobRunInfo refreshed_run;
    if (!run_cancelled &&
        db_->catalog_manager()->getJobRun(run_id, refreshed_run, &ctx) == Status::OK &&
        refreshed_run.state == CatalogManager::JobRunState::CANCELLED) {
        run_cancelled = true;
        if (run_message.empty()) {
            run_message = "Cancelled by user";
        }
    }

    run.completed_at = now_ms();
    run.rows_affected = rows_affected;
    if (run_cancelled && run_error_code == 0) {
        run_error_code = static_cast<int32_t>(Status::QUERY_CANCELED);
    }
    run.error_code = run_error_code;
    run.result_message = run_message;

    if (run_cancelled) {
        run.state = CatalogManager::JobRunState::CANCELLED;
    } else if (run_success) {
        run.state = CatalogManager::JobRunState::COMPLETED;
    } else {
        run.state = CatalogManager::JobRunState::FAILED;
    }
    db_->catalog_manager()->updateJobRun(run, &ctx);
    logJobRunAudit(db_, job, run, run_success && !run_cancelled, run_message);

    {
        auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(
            monotonic_now() - execution_start).count();
        auto& metrics = ScratchBirdMetrics::getInstance();
        metrics.initialize();
        if (metrics.scheduler_job_run_latency_seconds) {
            metrics.scheduler_job_run_latency_seconds->observe(duration);
        }
    }

    if (!run_success && !run_cancelled) {
        auto& metrics = ScratchBirdMetrics::getInstance();
        metrics.initialize();
        if (metrics.scheduler_jobs_failed_total) {
            metrics.scheduler_jobs_failed_total->inc();
        }
    }

    CatalogManager::JobInfo updated_job = job;
    uint64_t now_actual = now_ms();
    uint64_t reference_time = now;
    if (cfg.catch_up_policy == CatchUpPolicy::LAST && reference_time < now_actual) {
        reference_time = now_actual;
    }
    if (!run_cancelled && !run_success && run.retry_count < job.max_retries) {
        uint64_t backoff = static_cast<uint64_t>(job.retry_backoff_seconds);
        uint64_t delay = backoff * (1ULL << run.retry_count);
        updated_job.next_run_time = reference_time + delay * 1000;
    } else if (job.schedule_kind == CatalogManager::ScheduleKind::AT) {
        if (job.on_completion == CatalogManager::JobOnCompletion::DROP) {
            db_->catalog_manager()->deleteJob(job.job_id, true, &ctx);
            {
                std::lock_guard<std::mutex> lock(active_mutex_);
                active_jobs_.erase(job.job_id);
                active_cv_.notify_all();
            }
            {
                std::lock_guard<std::mutex> lock(active_runs_mutex_);
                active_runs_.erase(run_id);
            }
            return;
        }
        updated_job.state = CatalogManager::JobState::DISABLED;
        updated_job.next_run_time = 0;
    } else if (job.schedule_kind == CatalogManager::ScheduleKind::EVERY) {
        if (job.interval_seconds <= 0) {
            updated_job.state = CatalogManager::JobState::DISABLED;
            updated_job.next_run_time = 0;
        } else {
            updated_job.next_run_time = reference_time +
                static_cast<uint64_t>(job.interval_seconds) * 1000;
        }
    } else {
        uint64_t next_cron = detail::computeNextCronRunMsWithTimezone(
            job.cron_expression, reference_time, job.schedule_tz);
        if (next_cron == 0) {
            updated_job.state = CatalogManager::JobState::DISABLED;
            updated_job.next_run_time = 0;
        } else {
            updated_job.next_run_time = next_cron;
        }
    }

    if (updated_job.ends_at != 0 && updated_job.next_run_time > updated_job.ends_at) {
        updated_job.state = CatalogManager::JobState::DISABLED;
        updated_job.next_run_time = 0;
    }

    if (cfg.catch_up_policy == CatchUpPolicy::ALL) {
        CatalogManager::JobInfo latest_job;
        ErrorContext latest_ctx;
        if (db_->catalog_manager()->getJob(job.job_id, latest_job, &latest_ctx) == Status::OK) {
            if (latest_job.next_run_time > updated_job.next_run_time) {
                updated_job.next_run_time = latest_job.next_run_time;
            }
        }
    }

    db_->catalog_manager()->updateJob(updated_job, &ctx);

    size_t remaining_jobs_count = 0;
    {
        std::lock_guard<std::mutex> lock(active_mutex_);
        active_jobs_.erase(job.job_id);
        remaining_jobs_count = active_jobs_.size();
        active_cv_.notify_all();
    }
    {
        auto& metrics = ScratchBirdMetrics::getInstance();
        metrics.initialize();
        if (metrics.scheduler_jobs_running) {
            metrics.scheduler_jobs_running->set(static_cast<double>(remaining_jobs_count));
        }
    }

    {
        std::lock_guard<std::mutex> lock(active_runs_mutex_);
        active_runs_.erase(run_id);
    }
}

}  // namespace scratchbird::core
