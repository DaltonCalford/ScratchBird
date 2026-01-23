#include "scratchbird/core/job_scheduler.h"

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/job_scheduler_utils.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"

#include <chrono>
#include <thread>
#include <vector>

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

}  // namespace

JobScheduler::JobScheduler(Database* db)
    : JobScheduler(db, Config{})
{
}

JobScheduler::JobScheduler(Database* db, const Config& config)
    : db_(db)
    , config_(config)
{
}

JobScheduler::~JobScheduler() {
    stop();
}

Status JobScheduler::start(ErrorContext* ctx) {
    if (running_.load()) {
        return Status::OK;
    }

    if (!db_ || !db_->catalog_manager()) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "JobScheduler requires catalog manager");
        return Status::INVALID_ARGUMENT;
    }

    stop_requested_ = false;
    running_.store(true);
    worker_ = std::thread(&JobScheduler::runLoop, this);
    return Status::OK;
}

void JobScheduler::stop() {
    if (!running_.load()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    running_.store(false);
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
        cv_.wait_for(lock, std::chrono::seconds(config_.polling_interval_seconds), [&]() {
            return stop_requested_;
        });
        if (stop_requested_) {
            break;
        }
    }
}

void JobScheduler::processDueJobs() {
    auto now_ms = []() -> uint64_t {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    };

    uint64_t now = now_ms();
    std::vector<CatalogManager::JobInfo> due_jobs;
    ErrorContext ctx;
    auto status = db_->catalog_manager()->listDueJobs(now, due_jobs, &ctx);
    if (status != Status::OK) {
        return;
    }

    uint32_t handled = 0;
    for (auto& job : due_jobs) {
        if (handled >= config_.max_jobs_per_tick) {
            break;
        }

        if (job.schedule_kind == CatalogManager::ScheduleKind::CRON &&
            job.next_run_time == 0) {
            uint64_t next_cron = detail::computeNextCronRunMs(job.cron_expression, now);
            if (next_cron > now) {
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

        bool deps_ok = true;
        for (const auto& dep : deps) {
            std::vector<CatalogManager::JobRunInfo> runs;
            if (db_->catalog_manager()->listJobRuns(dep.depends_on_job_id, runs, &ctx) != Status::OK) {
                deps_ok = false;
                break;
            }
            if (!detail::dependencySatisfied(runs)) {
                deps_ok = false;
                break;
            }
        }

        if (!deps_ok) {
            continue;
        }

        handled++;

        std::vector<CatalogManager::JobRunInfo> previous_runs;
        db_->catalog_manager()->listJobRuns(job.job_id, previous_runs, &ctx);
        const CatalogManager::JobRunInfo* latest_run = nullptr;
        for (const auto& candidate : previous_runs) {
            if (!latest_run || candidate.completed_at > latest_run->completed_at) {
                latest_run = &candidate;
            }
        }

        CatalogManager::JobRunInfo run;
        run.job_id = job.job_id;
        run.scheduled_time = job.next_run_time == 0 ? now : job.next_run_time;
        run.started_at = now;
        run.state = CatalogManager::JobRunState::RUNNING;
        if (latest_run && latest_run->state == CatalogManager::JobRunState::FAILED) {
            run.retry_count = latest_run->retry_count + 1;
        }

        ID run_id;
        status = db_->catalog_manager()->createJobRun(run, run_id, &ctx);
        if (status != Status::OK) {
            continue;
        }

        run.job_run_id = run_id;

        bool run_success = true;
        bool run_cancelled = false;
        std::string run_message;
        int32_t run_error_code = 0;
        int64_t rows_affected = 0;

        if (config_.pre_execute_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.pre_execute_delay_ms));
        }

        CatalogManager::JobRunInfo current_run;
        if (db_->catalog_manager()->getJobRun(run_id, current_run, &ctx) == Status::OK &&
            current_run.state == CatalogManager::JobRunState::CANCELLED) {
            run_cancelled = true;
            run_message = "Cancelled";
        }

        if (!run_cancelled) {
            if (job.job_type == CatalogManager::JobType::EXTERNAL) {
                run_success = false;
                run_message = "External jobs are disabled in Alpha";
                run_error_code = -1;
            } else {
                std::unique_ptr<ConnectionContext> conn_ctx;
                Status conn_status = db_->connect(conn_ctx, &ctx);
                if (conn_status != Status::OK || !conn_ctx) {
                    run_success = false;
                    run_message = "Failed to create job connection context";
                    run_error_code = static_cast<int32_t>(conn_status);
                } else {
                    CurrentContextGuard ctx_guard(conn_ctx.get());

                    if (!isZeroId(job.created_by_user_uuid)) {
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

                    if (!isZeroId(job.run_as_role_uuid)) {
                        conn_ctx->setActiveRole(job.run_as_role_uuid);
                    }

                    std::string sql = job.job_sql;
                    if (sql.empty() && !isZeroId(job.procedure_uuid)) {
                        std::vector<CatalogManager::ProcedureInfo> procedures;
                        if (db_->catalog_manager()->listProcedures(procedures, &ctx) == Status::OK) {
                            for (const auto& proc : procedures) {
                            if (proc.procedure_id == job.procedure_uuid) {
                                sql = "CALL " + proc.name + "()";
                                break;
                            }
                            }
                        }
                    }

                    if (sql.empty()) {
                        run_success = false;
                        run_message = "Job has no SQL to execute";
                        run_error_code = -1;
                    } else {
                        sblr::QueryCompilerV2 compiler(db_);
                        if (!isZeroId(conn_ctx->getCurrentSchemaId())) {
                            compiler.setCurrentSchema(conn_ctx->getCurrentSchemaId());
                        }
                        std::vector<core::ID> path_ids;
                        const auto& paths = conn_ctx->search_path();
                        for (const auto& path : paths) {
                            CatalogManager::SchemaInfo schema_info;
                            ErrorContext path_ctx;
                            if (db_->catalog_manager()->getSchema(path, schema_info, &path_ctx) == Status::OK) {
                                path_ids.push_back(schema_info.schema_id);
                            }
                        }
                        compiler.setSearchPath(path_ids);

                        auto compile_result = compiler.compile(sql);
                        if (!compile_result.success()) {
                            run_success = false;
                            run_message = compile_result.errors().empty()
                                ? "Compilation error"
                                : compile_result.errors()[0];
                            run_error_code = static_cast<int32_t>(Status::INVALID_ARGUMENT);
                        } else {
                            sblr::Executor executor(db_);
                            executor.setConnectionContext(conn_ctx.get());
                            auto exec_result = executor.execute(compile_result.bytecode());
                            if (!exec_result.success()) {
                                run_success = false;
                                run_message = exec_result.error();
                                run_error_code = -1;
                            } else {
                                rows_affected = executor.getLastAffectedRows();
                            }
                        }
                    }
                }
            }
        }

        if (!run_cancelled) {
            run.completed_at = now_ms();
            run.rows_affected = rows_affected;
            run.error_code = run_error_code;
            run.result_message = run_message;

            if (run_success) {
                run.state = CatalogManager::JobRunState::COMPLETED;
            } else {
                run.state = CatalogManager::JobRunState::FAILED;
            }
            db_->catalog_manager()->updateJobRun(run, &ctx);
            logJobRunAudit(db_, job, run, run_success, run_message);
        }

        if (!run_cancelled && !run_success && run.retry_count < job.max_retries) {
            uint64_t backoff = static_cast<uint64_t>(job.retry_backoff_seconds);
            uint64_t delay = backoff * (1ULL << run.retry_count);
            job.next_run_time = now + delay * 1000;
        } else if (job.schedule_kind == CatalogManager::ScheduleKind::AT) {
            if (job.on_completion == CatalogManager::JobOnCompletion::DROP) {
                db_->catalog_manager()->deleteJob(job.job_id, true, &ctx);
                continue;
            }
            job.state = CatalogManager::JobState::DISABLED;
            job.next_run_time = 0;
        } else if (job.schedule_kind == CatalogManager::ScheduleKind::EVERY) {
            if (job.interval_seconds <= 0) {
                job.state = CatalogManager::JobState::DISABLED;
                job.next_run_time = 0;
            } else {
                job.next_run_time = now + static_cast<uint64_t>(job.interval_seconds) * 1000;
            }
        } else {
            uint64_t next_cron = detail::computeNextCronRunMs(job.cron_expression, now);
            if (next_cron == 0) {
                job.state = CatalogManager::JobState::DISABLED;
                job.next_run_time = 0;
            } else {
                job.next_run_time = next_cron;
            }
        }

        if (job.ends_at != 0 && job.next_run_time > job.ends_at) {
            job.state = CatalogManager::JobState::DISABLED;
            job.next_run_time = 0;
        }

        db_->catalog_manager()->updateJob(job, &ctx);
    }
}

}  // namespace scratchbird::core
