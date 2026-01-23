#include "scratchbird/core/job_scheduler.h"

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace scratchbird::core {

namespace {

struct CronField {
    int min_value = 0;
    int max_value = 0;
    bool any = true;
    std::vector<bool> allowed;
};

struct CronExpression {
    CronField minute;
    CronField hour;
    CronField day_of_month;
    CronField month;
    CronField day_of_week;
};

std::vector<std::string> split(const std::string& input, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) {
            out.push_back(item);
        }
    }
    return out;
}

bool parseCronNumber(const std::string& token, int& value_out) {
    if (token.empty()) {
        return false;
    }
    char* end = nullptr;
    long val = std::strtol(token.c_str(), &end, 10);
    if (!end || *end != '\0') {
        return false;
    }
    value_out = static_cast<int>(val);
    return true;
}

bool parseCronField(const std::string& field, CronField& out) {
    out.allowed.assign(out.max_value - out.min_value + 1, false);
    out.any = false;

    if (field == "*") {
        out.any = true;
        std::fill(out.allowed.begin(), out.allowed.end(), true);
        return true;
    }

    auto parts = split(field, ',');
    if (parts.empty()) {
        return false;
    }

    for (const auto& part : parts) {
        std::string range_part = part;
        int step = 1;
        auto step_parts = split(part, '/');
        if (step_parts.size() == 2) {
            range_part = step_parts[0];
            if (!parseCronNumber(step_parts[1], step) || step <= 0) {
                return false;
            }
        } else if (step_parts.size() > 2) {
            return false;
        }

        int start = 0;
        int end = 0;
        if (range_part == "*") {
            start = out.min_value;
            end = out.max_value;
        } else {
            auto range_tokens = split(range_part, '-');
            if (range_tokens.size() == 1) {
                if (!parseCronNumber(range_tokens[0], start)) {
                    return false;
                }
                end = start;
            } else if (range_tokens.size() == 2) {
                if (!parseCronNumber(range_tokens[0], start) ||
                    !parseCronNumber(range_tokens[1], end)) {
                    return false;
                }
            } else {
                return false;
            }
        }

        if (start < out.min_value || end > out.max_value || start > end) {
            return false;
        }

        for (int value = start; value <= end; value += step) {
            out.allowed[value - out.min_value] = true;
        }
    }

    return true;
}

bool parseCronExpression(const std::string& expr, CronExpression& out) {
    auto fields = split(expr, ' ');
    if (fields.size() != 5) {
        return false;
    }

    out.minute = {0, 59, true, {}};
    out.hour = {0, 23, true, {}};
    out.day_of_month = {1, 31, true, {}};
    out.month = {1, 12, true, {}};
    out.day_of_week = {0, 6, true, {}};

    if (!parseCronField(fields[0], out.minute)) return false;
    if (!parseCronField(fields[1], out.hour)) return false;
    if (!parseCronField(fields[2], out.day_of_month)) return false;
    if (!parseCronField(fields[3], out.month)) return false;
    if (!parseCronField(fields[4], out.day_of_week)) return false;

    return true;
}

bool cronFieldMatches(const CronField& field, int value) {
    if (field.any) {
        return true;
    }
    if (value < field.min_value || value > field.max_value) {
        return false;
    }
    return field.allowed[value - field.min_value];
}

bool cronMatches(const CronExpression& expr, const std::tm& tm) {
    int minute = tm.tm_min;
    int hour = tm.tm_hour;
    int dom = tm.tm_mday;
    int month = tm.tm_mon + 1;
    int dow = tm.tm_wday;

    if (!cronFieldMatches(expr.minute, minute)) return false;
    if (!cronFieldMatches(expr.hour, hour)) return false;
    if (!cronFieldMatches(expr.month, month)) return false;

    bool dom_any = expr.day_of_month.any;
    bool dow_any = expr.day_of_week.any;
    bool dom_match = cronFieldMatches(expr.day_of_month, dom);
    bool dow_match = cronFieldMatches(expr.day_of_week, dow);

    if (!dom_any && !dow_any) {
        return dom_match || dow_match;
    }
    if (!dom_any && dow_any) {
        return dom_match;
    }
    if (dom_any && !dow_any) {
        return dow_match;
    }
    return true;
}

uint64_t computeNextCronRunMs(const std::string& expr, uint64_t after_ms) {
    CronExpression parsed{};
    if (!parseCronExpression(expr, parsed)) {
        return 0;
    }

    constexpr int64_t kMaxMinutes = 60 * 24 * 366;
    int64_t after_seconds = static_cast<int64_t>(after_ms / 1000);
    int64_t candidate_seconds = after_seconds - (after_seconds % 60) + 60;

    for (int64_t minute = 0; minute < kMaxMinutes; ++minute) {
        time_t candidate = static_cast<time_t>(candidate_seconds + minute * 60);
        std::tm tm{};
        gmtime_r(&candidate, &tm);
        if (cronMatches(parsed, tm)) {
            return static_cast<uint64_t>(candidate) * 1000;
        }
    }

    return 0;
}

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

}  // namespace

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
            uint64_t next_cron = computeNextCronRunMs(job.cron_expression, now);
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
            if (runs.empty()) {
                deps_ok = false;
                break;
            }
            const CatalogManager::JobRunInfo* latest = nullptr;
            for (const auto& run : runs) {
                if (!latest || run.completed_at > latest->completed_at) {
                    latest = &run;
                }
            }
            if (!latest || latest->state != CatalogManager::JobRunState::COMPLETED) {
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
        std::string run_message;
        int32_t run_error_code = 0;
        int64_t rows_affected = 0;

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
                                sql = "CALL " + proc.procedure_name + "()";
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

        if (!run_success && run.retry_count < job.max_retries) {
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
            uint64_t next_cron = computeNextCronRunMs(job.cron_expression, now);
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
