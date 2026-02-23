/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/tid_resolver.h" // Sprint 4 Task 5.4.2
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/gc_manager.h"
#include "scratchbird/core/clog.h"
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/long_transaction_monitor.h"
#include "scratchbird/core/job_scheduler.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/encryption_key_manager.h"
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/table_stats_manager.h"
#include "scratchbird/core/charset_loader.h"
#include "scratchbird/core/timezone_loader.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/permission_cache.h" // Security Phase 3.2.3
#include "scratchbird/core/password_hash.h"
#include "scratchbird/core/portable_file_io.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/security/scram_auth.h"
#include <nlohmann/json.hpp>
#include <openssl/md5.h>
#include <fcntl.h>
#include <sys/stat.h>
#if !defined(_WIN32)
    #include <sys/file.h>
#endif
#include <cstring>
#include <cctype>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cerrno>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <climits>
#if defined(_WIN32)
    #include <io.h>
    #include <sys/locking.h>
#endif

#ifdef _WIN32
namespace {
constexpr int LOCK_EX = 0x2;
constexpr int LOCK_NB = 0x4;
inline int flock(int, int) { return 0; }
} // namespace
#endif

namespace scratchbird::core
{
    namespace
    {
        std::string toLowerAscii(std::string value)
        {
            for (char &ch : value)
            {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return value;
        }

        bool lockFileExclusiveNonBlocking(int fd, int* lock_errno = nullptr)
        {
#if defined(_WIN32)
            if (_lseeki64(fd, 0, SEEK_SET) < 0)
            {
                if (lock_errno != nullptr)
                {
                    *lock_errno = errno;
                }
                return false;
            }
            if (_locking(fd, _LK_NBLCK, LONG_MAX) != 0)
            {
                if (lock_errno != nullptr)
                {
                    *lock_errno = errno;
                }
                return false;
            }
            return true;
#else
            if (flock(fd, LOCK_EX | LOCK_NB) != 0)
            {
                if (lock_errno != nullptr)
                {
                    *lock_errno = errno;
                }
                return false;
            }
            return true;
#endif
        }

        bool preadFully(int fd, void* buffer, size_t size, off_t offset,
                        size_t* transferred_out = nullptr)
        {
            auto* dst = static_cast<uint8_t*>(buffer);
            size_t transferred = 0;
            while (transferred < size)
            {
                ssize_t rc = platform::readAt(fd,
                                     dst + transferred,
                                     size - transferred,
                                     offset + static_cast<off_t>(transferred));
                if (rc < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    if (transferred_out != nullptr)
                    {
                        *transferred_out = transferred;
                    }
                    return false;
                }
                if (rc == 0)
                {
                    if (transferred_out != nullptr)
                    {
                        *transferred_out = transferred;
                    }
                    return false;
                }
                transferred += static_cast<size_t>(rc);
            }
            if (transferred_out != nullptr)
            {
                *transferred_out = transferred;
            }
            return true;
        }

        bool pwriteFully(int fd, const void* buffer, size_t size, off_t offset,
                         size_t* transferred_out = nullptr)
        {
            const auto* src = static_cast<const uint8_t*>(buffer);
            size_t transferred = 0;
            while (transferred < size)
            {
                ssize_t rc = platform::writeAt(fd,
                                      src + transferred,
                                      size - transferred,
                                      offset + static_cast<off_t>(transferred));
                if (rc < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    if (transferred_out != nullptr)
                    {
                        *transferred_out = transferred;
                    }
                    return false;
                }
                if (rc == 0)
                {
                    if (transferred_out != nullptr)
                    {
                        *transferred_out = transferred;
                    }
                    return false;
                }
                transferred += static_cast<size_t>(rc);
            }
            if (transferred_out != nullptr)
            {
                *transferred_out = transferred;
            }
            return true;
        }

        BufferPool::PoolLayout parseBufferPoolLayout(const std::string &value, bool *recognized)
        {
            std::string normalized = toLowerAscii(value);
            if (normalized.empty() || normalized == "single" || normalized == "default")
            {
                if (recognized)
                {
                    *recognized = true;
                }
                return BufferPool::PoolLayout::Single;
            }
            if (normalized == "hot_cold" || normalized == "hot-cold" || normalized == "hotcold")
            {
                if (recognized)
                {
                    *recognized = true;
                }
                return BufferPool::PoolLayout::HotCold;
            }
            if (normalized == "tablespace" || normalized == "tablespaces")
            {
                if (recognized)
                {
                    *recognized = true;
                }
                return BufferPool::PoolLayout::Tablespace;
            }
            if (recognized)
            {
                *recognized = false;
            }
            return BufferPool::PoolLayout::Single;
        }

        bool readVersionFile(const std::string& path, std::string& out)
        {
            std::ifstream file(path);
            if (!file.is_open())
            {
                return false;
            }
            std::getline(file, out);
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
            {
                out.pop_back();
            }
            return !out.empty();
        }

        Status bootstrapI18nResources(Database* db, ErrorContext* ctx)
        {
            auto* catalog = db->catalog_manager();
            if (catalog == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
                return Status::INVALID_ARGUMENT;
            }

            bool need_charsets = false;
            bool need_collations = false;
            bool need_timezones = false;

            std::vector<CatalogManager::CharsetInfo> charsets;
            Status status = catalog->listCharsets(charsets, ctx);
            if (status == Status::NOT_FOUND || charsets.empty())
            {
                need_charsets = true;
            }
            else if (status != Status::OK)
            {
                return status;
            }

            std::vector<CatalogManager::CollationCatalogInfo> collations;
            status = catalog->listCollations(collations, ctx);
            if (status == Status::NOT_FOUND || collations.empty())
            {
                need_collations = true;
            }
            else if (status != Status::OK)
            {
                return status;
            }

            if (need_charsets || need_collations)
            {
                CharsetLoader loader(catalog, db);
                Status cs_status = loader.loadBuiltinCharsets(ctx);
                if (cs_status != Status::OK)
                {
                    return cs_status;
                }

                if (need_charsets && std::filesystem::exists("resources/charsets/charsets.json"))
                {
                    cs_status = loader.loadFromJSONFile("resources/charsets/charsets.json", ctx);
                    if (cs_status != Status::OK)
                    {
                        return cs_status;
                    }
                }
                else if (need_charsets)
                {
                    LOG_WARNING(GENERAL, "Charset resources not found; using built-in defaults");
                }

                if (need_collations && std::filesystem::exists("resources/collations/collations.json"))
                {
                    cs_status = loader.loadCollationsFromJSONFile("resources/collations/collations.json", ctx);
                    if (cs_status != Status::OK)
                    {
                        return cs_status;
                    }
                }
                else if (need_collations)
                {
                    LOG_WARNING(GENERAL, "Collation resources not found; using built-in defaults");
                }
            }

            std::vector<CatalogManager::TimezoneInfo> timezones;
            status = catalog->listTimezones(timezones, ctx);
            if (status == Status::NOT_FOUND || timezones.empty())
            {
                need_timezones = true;
            }
            else if (status != Status::OK)
            {
                return status;
            }

            if (need_timezones)
            {
                std::string zoneinfo_dir;
                if (std::filesystem::exists("resources/timezones"))
                {
                    zoneinfo_dir = "resources/timezones";
                }
                else if (std::filesystem::exists("/usr/share/zoneinfo"))
                {
                    zoneinfo_dir = "/usr/share/zoneinfo";
                }

                if (!zoneinfo_dir.empty())
                {
                    TimezoneLoader loader(catalog);
                    ErrorContext tz_ctx;
                    Status tz_status = loader.loadFromDirectory(zoneinfo_dir, &tz_ctx);
                    if (tz_status == Status::NOT_FOUND)
                    {
                        LOG_WARNING(GENERAL, "Timezone resources not found; using built-in defaults");
                    }
                    else if (tz_status != Status::OK)
                    {
                        if (ctx)
                        {
                            ctx->set(tz_status,
                                     tz_ctx.message.empty() ? "Failed to load timezones"
                                                            : tz_ctx.message.c_str(),
                                     tz_ctx.file ? tz_ctx.file : __FILE__,
                                     tz_ctx.line,
                                     tz_ctx.function ? tz_ctx.function : __func__);
                        }
                        return tz_status;
                    }

                    std::string tzdata_version;
                    if (readVersionFile(zoneinfo_dir + "/version", tzdata_version) ||
                        readVersionFile("resources/timezones/version", tzdata_version))
                    {
                        catalog->setTimezoneVersion(tzdata_version, ctx);
                    }
                }
                else
                {
                    LOG_WARNING(GENERAL, "Timezone resources not found; using built-in defaults");
                }
            }

            std::string resource_version;
            if (readVersionFile("resources/i18n/version", resource_version))
            {
                std::string catalog_version;
                Status vstatus = catalog->getI18nResourceVersion(catalog_version, ctx);
                if (vstatus == Status::NOT_FOUND)
                {
                    catalog->setI18nResourceVersion(resource_version, ctx);
                }
                else if (vstatus == Status::OK && catalog_version != resource_version)
                {
                    LOG_WARNING(GENERAL,
                                "i18n resource version mismatch: catalog=%s resources=%s",
                                catalog_version.c_str(), resource_version.c_str());
                }
            }

            return Status::OK;
        }
    }
    namespace {
    constexpr uint32_t kSysarchScramIterations = 4096;
    constexpr const char* kSysarchUser = "SYSARCH";
    constexpr const char* kSysarchPassword = "ScratchBirdBeta1!";

    std::string toHexLower(const unsigned char* data, size_t len)
    {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < len; ++i)
        {
            oss << std::setw(2) << static_cast<int>(data[i]);
        }
        return oss.str();
    }

    std::string computePgMd5StoredHash(const std::string& username,
                                       const std::string& password)
    {
        std::string input = password + username;
        unsigned char hash[MD5_DIGEST_LENGTH];
        MD5(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
        return "md5" + toHexLower(hash, MD5_DIGEST_LENGTH);
    }

    core::Status buildPasswordHashPayload(const std::string& username,
                                          const std::string& password,
                                          std::string& out)
    {
        using json = nlohmann::json;
        json payload = json::object();
        payload["bcrypt"] = core::PasswordHash::hashPassword(password);
        payload["md5"] = computePgMd5StoredHash(username, password);

        json scram = json::object();
        auto add_scram = [&](security::ScramAlgorithm algo, const char* key) -> core::Status
        {
            std::vector<uint8_t> salt;
            std::vector<uint8_t> stored_key;
            std::vector<uint8_t> server_key;
            auto status = security::generateScramCredentials(
                password, algo, kSysarchScramIterations, salt, stored_key, server_key);
            if (status != core::Status::OK)
            {
                return status;
            }

            json entry = json::object();
            entry["iterations"] = kSysarchScramIterations;
            entry["salt"] = security::base64Encode(salt);
            entry["stored_key"] = security::base64Encode(stored_key);
            entry["server_key"] = security::base64Encode(server_key);
            scram[key] = entry;
            return core::Status::OK;
        };

        auto status = add_scram(security::ScramAlgorithm::SHA_256, "sha256");
        if (status != core::Status::OK)
        {
            return status;
        }
        status = add_scram(security::ScramAlgorithm::SHA_512, "sha512");
        if (status != core::Status::OK)
        {
            return status;
        }

        payload["scram"] = scram;
        out = payload.dump();
        return core::Status::OK;
    }

    bool isZeroId(const ID& id)
    {
        static const ID zero{};
        return id == zero;
    }

    void logReattachAuditEvent(AuditLogger* audit_logger,
                               AuditEventType event_type,
                               const ID& user_id,
                               const ID& session_id,
                               const ID& authkey_id,
                               const ID& dormant_id,
                               bool success,
                               const char* reason)
    {
        if (!audit_logger)
        {
            return;
        }

        AuditEvent event;
        event.event_type = event_type;
        event.user_id = user_id;
        event.session_id = session_id;
        event.authkey_id = authkey_id;
        event.success = success;

        std::ostringstream details;
        details << "{\"dormant_id\":\"" << dormant_id.toString() << "\"";
        if (reason && reason[0] != '\0')
        {
            details << ",\"reason\":\"" << reason << "\"";
        }
        details << "}";
        event.details = details.str();

        ErrorContext audit_ctx;
        audit_logger->logEvent(event, &audit_ctx);
    }

    core::Status ensureSysarchUser(CatalogManager* catalog, ErrorContext* ctx)
    {
        std::string password_hash;
        auto status = buildPasswordHashPayload(kSysarchUser, kSysarchPassword, password_hash);
        if (status != Status::OK)
        {
            if (ctx)
            {
                ctx->set(status, "Failed to build SYSARCH password hash",
                         __FILE__, __LINE__, __func__);
            }
            return status;
        }

        ID user_id;
        status = catalog->ensureUserExists(kSysarchUser, password_hash, ID(), true, user_id, ctx);
        return status;
    }
    } // namespace

    Database::Database()
    {
        table_stats_manager_ = std::make_unique<TableStatsManager>(this);
    }

    Database::~Database()
    {
        close();
    }

    // NOTE: Move operations deleted in header because Database contains std::mutex (non-movable)

    void Database::close()
    {
        // Stop long transaction monitor (before shutting down other components)
        if (long_transaction_monitor_ && long_transaction_monitor_->isMonitoring())
        {
            ErrorContext ctx;
            long_transaction_monitor_->stopMonitoring(&ctx);
        }
        long_transaction_monitor_.reset();

        {
            std::lock_guard<std::mutex> lock(scheduler_mutex_);
            if (job_scheduler_)
            {
                job_scheduler_->stop();
            }
            job_scheduler_.reset();
        }

        {
            std::lock_guard<std::mutex> lock(dormant_mutex_);
            // Tear down dormant contexts while lock/txn managers are still available.
            for (const auto& [dormant_id, entry] : dormant_contexts_)
            {
                if (catalog_manager_ && !isZeroId(entry.reattach_authkey_id))
                {
                    ErrorContext revoke_ctx;
                    const Status revoke_status =
                        catalog_manager_->revokeAuthKey(entry.reattach_authkey_id, &revoke_ctx);
                    logReattachAuditEvent(audit_logger_.get(),
                                          AuditEventType::REATTACH_TOKEN_REVOKED,
                                          entry.connection ? entry.connection->getCurrentUserId() : ID{},
                                          entry.connection ? entry.connection->protocolSessionId() : ID{},
                                          entry.reattach_authkey_id,
                                          dormant_id,
                                          revoke_status == Status::OK ||
                                              revoke_status == Status::INVALID_AUTHORIZATION,
                                          "database_close");
                }
            }
            dormant_contexts_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(connection_registry_mutex_);
            connection_registry_.clear();
        }

        // Shut down domain manager
        domain_manager_.reset();

        // Shut down encryption key manager
        encryption_key_manager_.reset();

        // Shut down audit logger before catalog manager
        audit_logger_.reset();

        // Shut down garbage collector first (before sweep manager)
        garbage_collector_.reset();

        // Shut down sweep manager (before transaction manager)
        sweep_manager_.reset();

        // Shut down lock manager (before transaction manager)
        lock_manager_.reset();

        // Shut down ProcArray (before transaction manager)
        if (header_ && header_->proc_array_initialized)
        {
            ErrorContext ctx;
            shutdownProcArray(&ctx);
        }

        // Shut down transaction manager
        transaction_manager_.reset();

        // Shut down storage engine
        storage_engine_.reset();

        // Shut down catalog manager
        catalog_manager_.reset();

        table_stats_manager_.reset();

        // Flush page manager before shutting down buffer pool
        if (page_manager_ != nullptr)
        {
            ErrorContext ctx;
            page_manager_->flush(&ctx);
        }

        // Sync header_buffer_ with latest header page before buffer pool shutdown
        if (buffer_pool_ != nullptr && header_buffer_ != nullptr)
        {
            void *header_page = nullptr;
            ErrorContext ctx;
            if (buffer_pool_->pinPage(0, &header_page, &ctx) == Status::OK && header_page != nullptr)
            {
                std::memcpy(header_buffer_.get(), header_page, page_size_);
                buffer_pool_->unpinPage(0, false, &ctx);
            }
        }

        // Shut down buffer pool (flushes dirty pages)
        if (buffer_pool_ != nullptr)
        {
            buffer_pool_->shutdown();
        }
        buffer_pool_.reset();

        // Now delete page manager
        page_manager_.reset();

        // Flush the database header page (page 0) before closing
        // The header is stored in header_buffer_ and may have been modified during operation
        // (e.g., next_xid, next_oid, catalog_initialized, etc.)
        // write_page() will recalculate the checksum before writing
        if (header_buffer_ && fd_ >= 0)
        {
            ErrorContext ctx;
            write_page(0, header_buffer_.get(), &ctx);
            // Sync to ensure data is persisted
            sync(&ctx);
        }

        // LOW-1 FIX: header_buffer_ automatically cleaned up by std::unique_ptr
        header_ = nullptr;        // Invalidate pointer (was pointing into header_buffer_)
        header_buffer_.reset();   // Release memory (automatic, but explicit for clarity)

        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
    }

    Status Database::applySchedulerConfig(ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(scheduler_mutex_);
        // Default disabled to keep database lifecycle deterministic unless explicitly enabled.
        bool scheduler_enabled =
            Config::getInstance().getBool("scheduler", "enabled", false);
        if (!scheduler_enabled)
        {
            if (job_scheduler_)
            {
                job_scheduler_->stop();
            }
            job_scheduler_.reset();
            return Status::OK;
        }

        JobScheduler::Config sched_config;
        sched_config.polling_interval_seconds =
            Config::getInstance().getUInt("scheduler", "polling_interval_seconds", 10);
        sched_config.max_jobs_per_tick =
            Config::getInstance().getUInt("scheduler", "max_jobs_per_tick", 16);
        sched_config.max_concurrent_jobs =
            Config::getInstance().getUInt("scheduler", "max_concurrent_jobs", 10);
        sched_config.job_timeout_seconds =
            Config::getInstance().getUInt("scheduler", "job_timeout_seconds", 3600);
        sched_config.cron_fallback_seconds =
            Config::getInstance().getUInt("scheduler", "cron_fallback_seconds", 60);
        sched_config.pre_execute_delay_ms =
            Config::getInstance().getUInt("scheduler", "pre_execute_delay_ms", 0);
        {
            std::string catch_up = Config::getInstance().getString("scheduler", "catch_up", "last");
            std::string normalized;
            normalized.reserve(catch_up.size());
            for (unsigned char c : catch_up)
            {
                normalized.push_back(static_cast<char>(std::tolower(c)));
            }
            if (normalized == "none")
            {
                sched_config.catch_up_policy = JobScheduler::CatchUpPolicy::NONE;
            }
            else if (normalized == "all")
            {
                sched_config.catch_up_policy = JobScheduler::CatchUpPolicy::ALL;
            }
            else
            {
                sched_config.catch_up_policy = JobScheduler::CatchUpPolicy::LAST;
            }
        }
        auto parseList = [](const std::string& value) {
            std::vector<std::string> items;
            std::string current;
            for (char ch : value) {
                if (ch == ',' || ch == ';') {
                    if (!current.empty()) {
                        items.push_back(current);
                        current.clear();
                    }
                    continue;
                }
                if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                    continue;
                }
                current.push_back(ch);
            }
            if (!current.empty()) {
                items.push_back(current);
            }
            return items;
        };

        sched_config.external_jobs_enabled =
            Config::getInstance().getBool("scheduler", "external_jobs_enabled", false);
        sched_config.external_working_dir =
            Config::getInstance().getString("scheduler", "external_working_dir", "");
        sched_config.external_allowed_commands = parseList(
            Config::getInstance().getString("scheduler", "external_allowed_commands", ""));
        sched_config.external_allowed_dirs = parseList(
            Config::getInstance().getString("scheduler", "external_allowed_dirs", ""));
        sched_config.external_env_allowlist = parseList(
            Config::getInstance().getString("scheduler", "external_env_allowlist", ""));
        sched_config.external_output_max_bytes =
            Config::getInstance().getUInt("scheduler", "external_output_max_bytes", 1024 * 1024);
        sched_config.external_kill_grace_ms =
            Config::getInstance().getUInt("scheduler", "external_kill_grace_ms", 2000);
        sched_config.external_cpu_time_limit_seconds =
            Config::getInstance().getUInt("scheduler", "external_cpu_time_limit_seconds", 0);
        sched_config.external_memory_max_bytes =
            Config::getInstance().getUInt("scheduler", "external_memory_max_bytes", 0);

        if (!job_scheduler_)
        {
            try
            {
                job_scheduler_ = std::make_unique<JobScheduler>(this, sched_config);
            }
            catch (const std::bad_alloc &)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate JobScheduler");
                return Status::OOM;
            }
            auto status = job_scheduler_->start(ctx);
            if (status != Status::OK)
            {
                job_scheduler_.reset();
                return status;
            }
        }
        else
        {
            job_scheduler_->updateConfig(sched_config);
        }

        return Status::OK;
    }

    auto Database::connect(std::unique_ptr<ConnectionContext> &connection_out, ErrorContext *ctx)
        -> Status
    {
        if (!is_open())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Database is not open");
            return Status::NOT_FOUND;
        }

        // Initialize ProcArray if not already done
        if (header_)
        {
            Status s = initializeProcArray(config::DEFAULT_MAX_BACKENDS, ctx);
            if (s != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, s, "Failed to initialize ProcArray");
                return s;
            }
        }

        // Register this backend and get a proc_id
        uint32_t proc_id = 0;
        Status s = ProcArrayManager::registerBackend(&proc_id, ctx);
        if (s != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, s, "Failed to register backend in ProcArray");
            return s;
        }

        // Create connection context
        try
        {
            auto connection = std::make_unique<ConnectionContext>(this, proc_id);

            // Initialize the connection (starts initial transaction)
            s = connection->initialize(ctx);
            if (s != Status::OK)
            {
                // Unregister backend on failure
                ProcArrayManager::unregisterBackend(proc_id, nullptr);
                SET_ERROR_CONTEXT(ctx, s, "Failed to initialize connection context");
                return s;
            }

            connection->setRoleSwitchPolicy(role_switch_policy_);

            connection_out = std::move(connection);
            return Status::OK;
        }
        catch (const std::bad_alloc &)
        {
            // Unregister backend on OOM
            ProcArrayManager::unregisterBackend(proc_id, nullptr);
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate ConnectionContext");
            return Status::OOM;
        }
    }

    void Database::registerConnectionContext(ConnectionContext* ctx)
    {
        if (!ctx)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(connection_registry_mutex_);
        connection_registry_[ctx->getProcId()] = ctx;
    }

    void Database::unregisterConnectionContext(ConnectionContext* ctx)
    {
        if (!ctx)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(connection_registry_mutex_);
        auto it = connection_registry_.find(ctx->getProcId());
        if (it != connection_registry_.end() && it->second == ctx)
        {
            connection_registry_.erase(it);
        }
    }

    void Database::rebindConnectionContext(uint32_t proc_id, ConnectionContext* ctx)
    {
        if (!ctx)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(connection_registry_mutex_);
        connection_registry_[proc_id] = ctx;
    }

    std::vector<Database::ConnectionIoSnapshot> Database::snapshotConnectionIoStats() const
    {
        std::vector<ConnectionIoSnapshot> out;
        std::lock_guard<std::mutex> lock(connection_registry_mutex_);
        out.reserve(connection_registry_.size());
        for (const auto& [proc_id, ctx] : connection_registry_)
        {
            if (!ctx)
            {
                continue;
            }
            ConnectionIoSnapshot snap;
            snap.proc_id = proc_id;
            snap.session_id = ctx->effectiveSessionId();
            snap.transaction_id = ctx->getCurrentXid();
            snap.statement_id = ctx->currentStatementId();
            snap.statement_active = ctx->statementIoActive();
            snap.connection_io = ctx->snapshotConnectionIoStats();
            snap.transaction_io = ctx->snapshotTransactionIoStats();
            snap.statement_io = ctx->snapshotStatementIoStats();
            out.push_back(snap);
        }
        return out;
    }

    std::vector<Database::ConnectionSecuritySnapshot> Database::snapshotConnectionSecurityStacks() const
    {
        std::vector<ConnectionSecuritySnapshot> out;
        std::lock_guard<std::mutex> lock(connection_registry_mutex_);
        out.reserve(connection_registry_.size());
        for (const auto& [proc_id, ctx] : connection_registry_)
        {
            if (!ctx)
            {
                continue;
            }
            ConnectionSecuritySnapshot snap;
            snap.proc_id = proc_id;
            snap.session_id = ctx->effectiveSessionId();
            snap.statement_id = ctx->currentStatementId();
            snap.statement_time = ctx->lastStatementTime();
            snap.statement_line = ctx->lastStatementLine();
            snap.statement_column = ctx->lastStatementColumn();
            snap.security_stack = ctx->listSecurityContextStack();
            out.push_back(std::move(snap));
        }
        return out;
    }

    Status Database::detachToDormant(std::unique_ptr<ConnectionContext> &connection,
                                     ID &dormant_id_out,
                                     ErrorContext *ctx,
                                     ID *reattach_authkey_out)
    {
        if (reattach_authkey_out)
        {
            *reattach_authkey_out = ID{};
        }

        if (!connection)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Connection context is null");
            return Status::INVALID_ARGUMENT;
        }

        auto *catalog = catalog_manager_.get();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager::DormantTransactionInfo info;
        info.attachment_id = connection->attachmentId();
        info.proc_id = connection->getProcId();
        info.txn_id = connection->getCurrentXid();
        info.session_id = connection->protocolSessionId();
        info.user_id = connection->getCurrentUserId();
        info.session_user_id = connection->getSessionUserId();
        info.role_id = connection->getActiveRoleId();
        info.isolation_level = static_cast<uint8_t>(connection->getIsolationLevel());
        info.access_mode = connection->isReadOnly()
            ? CatalogManager::DormantAccessMode::READ_ONLY
            : CatalogManager::DormantAccessMode::READ_WRITE;
        info.wait_mode = connection->getWaitForLocks()
            ? CatalogManager::DormantWaitMode::WAIT
            : CatalogManager::DormantWaitMode::NO_WAIT;
        info.autocommit_mode = connection->autocommitMode();
        info.lock_timeout_seconds = connection->getLockTimeout();
        info.current_schema_id = connection->getCurrentSchemaId();
        info.session_settings = connection->sessionSettingsJson();
        info.last_statement_text = connection->lastStatementText();
        info.last_statement_hash = connection->lastStatementHash();
        info.last_statement_type = static_cast<CatalogManager::DormantStatementType>(
            connection->lastStatementType());
        info.last_statement_status = static_cast<CatalogManager::DormantStatementStatus>(
            connection->lastStatementStatus());
        info.state = CatalogManager::DormantTransactionState::DORMANT;
        info.start_time = static_cast<uint64_t>(connection->getTransactionStartTime().count());

        uint64_t now_micros = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
        info.last_activity_time = connection->lastActivityTime();
        if (info.last_activity_time == 0)
        {
            info.last_activity_time = now_micros;
        }
        info.dormant_since = now_micros;

        uint64_t lease_seconds = Config::getInstance().getUInt(
            "transactions", "dormant_lease_seconds",
            config::DEFAULT_DORMANT_TXN_LEASE_SECONDS);
        if (lease_seconds == 0)
        {
            info.lease_expires_at = 0;
        }
        else
        {
            info.lease_expires_at = now_micros + lease_seconds * 1000000ULL;
        }

        CatalogManager::AuthKeyInfo reattach_authkey;
        reattach_authkey.issuer = "dormant_reattach";
        reattach_authkey.status = CatalogManager::AuthKeyStatus::ACTIVE;
        reattach_authkey.usage_type = CatalogManager::AuthKeyUsage::SINGLE_USE;
        reattach_authkey.usage_limit = 1;
        reattach_authkey.scope = CatalogManager::AuthKeyScope::REATTACH;
        if (lease_seconds != 0)
        {
            const auto now_tp = std::chrono::system_clock::now().time_since_epoch();
            const auto ttl = std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::seconds(lease_seconds));
            reattach_authkey.valid_to = static_cast<uint64_t>((now_tp + ttl).count());
        }

        ID reattach_authkey_id;
        Status key_status = catalog->createAuthKey(reattach_authkey, reattach_authkey_id, ctx);
        if (key_status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, key_status, "Failed to create dormant reattach AuthKey");
            return key_status;
        }

        info.last_statement_time = connection->lastStatementTime();
        info.last_rows_affected = connection->lastRowsAffected();
        info.last_error_code = connection->lastErrorCode();
        info.last_sqlstate = connection->lastSqlstate();
        info.server_instance_id = server_instance_id_;
        info.is_valid = true;

        Status status = catalog->createDormantTransaction(info, ctx);
        if (status != Status::OK)
        {
            ErrorContext revoke_ctx;
            catalog->revokeAuthKey(reattach_authkey_id, &revoke_ctx);
            logReattachAuditEvent(audit_logger_.get(),
                                  AuditEventType::REATTACH_TOKEN_REVOKED,
                                  connection->getCurrentUserId(),
                                  connection->protocolSessionId(),
                                  reattach_authkey_id,
                                  info.dormant_id,
                                  true,
                                  "detach_failed");
            return status;
        }

        dormant_id_out = info.dormant_id;
        if (reattach_authkey_out)
        {
            *reattach_authkey_out = reattach_authkey_id;
        }

        DormantContextEntry entry;
        entry.dormant_id = info.dormant_id;
        entry.reattach_authkey_id = reattach_authkey_id;
        entry.lease_expires_at = info.lease_expires_at;
        entry.connection = std::move(connection);

        {
            std::lock_guard<std::mutex> lock(dormant_mutex_);
            // Keep the ConnectionContext alive so locks + ProcArray visibility remain intact.
            dormant_contexts_.emplace(entry.dormant_id, std::move(entry));
        }

        logReattachAuditEvent(audit_logger_.get(),
                              AuditEventType::REATTACH_TOKEN_ISSUED,
                              info.user_id,
                              info.session_id,
                              reattach_authkey_id,
                              info.dormant_id,
                              true,
                              "detached");

        return Status::OK;
    }

    Status Database::reattachDormant(const ID &dormant_id,
                                     std::unique_ptr<ConnectionContext> &connection_out,
                                     ErrorContext *ctx,
                                     const ID *reattach_authkey)
    {
        std::lock_guard<std::mutex> lock(dormant_mutex_);

        auto it = dormant_contexts_.find(dormant_id);
        if (it == dormant_contexts_.end())
        {
            logReattachAuditEvent(audit_logger_.get(),
                                  AuditEventType::REATTACH_FAILURE,
                                  ID{},
                                  ID{},
                                  reattach_authkey ? *reattach_authkey : ID{},
                                  dormant_id,
                                  false,
                                  "dormant_not_found");
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Dormant transaction not found");
            return Status::NOT_FOUND;
        }

        if (!reattach_authkey || isZeroId(*reattach_authkey))
        {
            logReattachAuditEvent(audit_logger_.get(),
                                  AuditEventType::REATTACH_FAILURE,
                                  it->second.connection ? it->second.connection->getCurrentUserId() : ID{},
                                  it->second.connection ? it->second.connection->protocolSessionId() : ID{},
                                  ID{},
                                  dormant_id,
                                  false,
                                  "missing_reattach_authkey");
            SET_ERROR_CONTEXT(ctx, Status::INVALID_AUTHORIZATION, "Reattach AuthKey is required");
            return Status::INVALID_AUTHORIZATION;
        }

        if (*reattach_authkey != it->second.reattach_authkey_id)
        {
            logReattachAuditEvent(audit_logger_.get(),
                                  AuditEventType::REATTACH_FAILURE,
                                  it->second.connection ? it->second.connection->getCurrentUserId() : ID{},
                                  it->second.connection ? it->second.connection->protocolSessionId() : ID{},
                                  *reattach_authkey,
                                  dormant_id,
                                  false,
                                  "reattach_authkey_mismatch");
            SET_ERROR_CONTEXT(ctx, Status::INVALID_AUTHORIZATION, "Reattach AuthKey mismatch");
            return Status::INVALID_AUTHORIZATION;
        }

        if (catalog_manager_)
        {
            CatalogManager::AuthKeyInfo authkey_info;
            Status authkey_status = catalog_manager_->getAuthKey(*reattach_authkey,
                                                                 authkey_info,
                                                                 ctx);
            if (authkey_status != Status::OK ||
                authkey_info.status != CatalogManager::AuthKeyStatus::ACTIVE ||
                authkey_info.scope != CatalogManager::AuthKeyScope::REATTACH)
            {
                logReattachAuditEvent(audit_logger_.get(),
                                      AuditEventType::REATTACH_FAILURE,
                                      it->second.connection ? it->second.connection->getCurrentUserId() : ID{},
                                      it->second.connection ? it->second.connection->protocolSessionId() : ID{},
                                      *reattach_authkey,
                                      dormant_id,
                                      false,
                                      "reattach_authkey_invalid");
                SET_ERROR_CONTEXT(ctx, Status::INVALID_AUTHORIZATION,
                                  "Reattach AuthKey is invalid or not active");
                return Status::INVALID_AUTHORIZATION;
            }

            Status consume_status = catalog_manager_->consumeAuthKey(*reattach_authkey, 1, ctx);
            if (consume_status != Status::OK)
            {
                logReattachAuditEvent(audit_logger_.get(),
                                      AuditEventType::REATTACH_FAILURE,
                                      it->second.connection ? it->second.connection->getCurrentUserId() : ID{},
                                      it->second.connection ? it->second.connection->protocolSessionId() : ID{},
                                      *reattach_authkey,
                                      dormant_id,
                                      false,
                                      "reattach_authkey_consume_failed");
                return consume_status;
            }

            CatalogManager::DormantTransactionInfo info;
            Status status = catalog_manager_->getDormantTransaction(dormant_id, info, ctx);
            if (status == Status::OK)
            {
                if (info.server_instance_id != server_instance_id_)
                {
                    logReattachAuditEvent(audit_logger_.get(),
                                          AuditEventType::REATTACH_FAILURE,
                                          it->second.connection ? it->second.connection->getCurrentUserId() : ID{},
                                          it->second.connection ? it->second.connection->protocolSessionId() : ID{},
                                          *reattach_authkey,
                                          dormant_id,
                                          false,
                                          "reattach_wrong_server_instance");
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Dormant transaction belongs to a different server instance");
                    return Status::INVALID_ARGUMENT;
                }

                info.state = CatalogManager::DormantTransactionState::REATTACHED;
                info.last_activity_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                              std::chrono::system_clock::now().time_since_epoch())
                                              .count();
                catalog_manager_->updateDormantTransaction(info, ctx);
            }
        }

        connection_out = std::move(it->second.connection);
        logReattachAuditEvent(audit_logger_.get(),
                              AuditEventType::REATTACH_SUCCESS,
                              connection_out ? connection_out->getCurrentUserId() : ID{},
                              connection_out ? connection_out->protocolSessionId() : ID{},
                              *reattach_authkey,
                              dormant_id,
                              true,
                              "reattached");
        dormant_contexts_.erase(it);
        return Status::OK;
    }

    auto Database::create_catalog_page(int fd, uint32_t page_size, uint8_t *page_buffer,
                                       const ID &db_uuid, uint64_t /*micros*/, ErrorContext *ctx)
        -> Status
    {
        memset(page_buffer, 0, page_size);
        auto *catalog_header = reinterpret_cast<PageHeader *>(page_buffer);

        catalog_header->magic = K_MAGIC_SBRD;
        catalog_header->version = 1;
        catalog_header->page_type = PAGE_TYPE_CATALOG_ROOT;
        catalog_header->page_size = page_size;
        catalog_header->page_id = BOOTSTRAP_PAGE_CATALOG_ROOT;
        catalog_header->flags = 0;
        catalog_header->lsn = 0;
        catalog_header->generation = 1;
        setDatabaseUuid(*catalog_header, db_uuid);
        setObjectUuid(*catalog_header, ID{});
        catalog_header->item_count = 0;
        pageSetLower(*catalog_header, sizeof(PageHeader));
        pageSetUpper(*catalog_header, page_size);
        pageSetSpecial(*catalog_header, page_size);

        // Bootstrap pages are written directly and must carry valid checksum metadata.
        catalog_header->flags |= PAGE_FLAG_CHECKSUM_VALID;
        catalog_header->checksum = calculatePageChecksum(page_buffer, page_size);

        // Write catalog page with full-transfer semantics to avoid partial write races.
        const off_t offset =
            static_cast<off_t>(BOOTSTRAP_PAGE_CATALOG_ROOT) * static_cast<off_t>(page_size);
        size_t bytes_written = 0;
        errno = 0;
        if (!pwriteFully(fd, page_buffer, page_size, offset, &bytes_written))
        {
            char msg[256];
            if (errno != 0)
            {
                snprintf(msg, sizeof(msg), "Failed to write catalog page: %s", std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg),
                         "Failed to write catalog page (partial write %zu/%u bytes)",
                         bytes_written, page_size);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::create_fsm_page(int fd, uint32_t page_size, uint8_t *page_buffer,
                                   const ID &db_uuid, ErrorContext *ctx) -> Status
    {
        memset(page_buffer, 0, page_size);
        auto *fsm_header = reinterpret_cast<PageHeader *>(page_buffer);

        fsm_header->magic = K_MAGIC_SBRD;
        fsm_header->version = 1;
        fsm_header->page_type = PAGE_TYPE_FSM_ROOT;
        fsm_header->page_size = page_size;
        fsm_header->page_id = BOOTSTRAP_PAGE_FSM_ROOT;
        fsm_header->flags = 0;
        fsm_header->lsn = 0;
        fsm_header->generation = 1;
        setDatabaseUuid(*fsm_header, db_uuid);
        setObjectUuid(*fsm_header, ID{});
        fsm_header->item_count = 0;

        // Initialize FSM data
        struct
        {
            uint32_t total_pages;
            uint32_t free_pages;
            uint32_t next_fsm_page;
            uint8_t bitmap[1]; // First byte of bitmap
        } *fsm_data = reinterpret_cast<decltype(fsm_data)>(page_buffer + sizeof(PageHeader));

        fsm_data->total_pages = BOOTSTRAP_FIXED_PAGE_COUNT;
        fsm_data->free_pages = 0;  // All system pages allocated
        fsm_data->next_fsm_page = 0;
        fsm_data->bitmap[0] = 0x3F; // First 6 bits set (pages 0..5 allocated)

        // Update header fields
        pageSetLower(*fsm_header, sizeof(PageHeader) + sizeof(uint32_t) * 3 + 1);
        pageSetUpper(*fsm_header, page_size);
        pageSetSpecial(*fsm_header, page_size);

        // Bootstrap pages are written directly and must carry valid checksum metadata.
        fsm_header->flags |= PAGE_FLAG_CHECKSUM_VALID;
        fsm_header->checksum = calculatePageChecksum(page_buffer, page_size);

        const off_t offset =
            static_cast<off_t>(BOOTSTRAP_PAGE_FSM_ROOT) * static_cast<off_t>(page_size);
        size_t bytes_written = 0;
        errno = 0;
        if (!pwriteFully(fd, page_buffer, page_size, offset, &bytes_written))
        {
            char msg[256];
            if (errno != 0)
            {
                snprintf(msg, sizeof(msg), "Failed to write FSM page: %s", std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg), "Failed to write FSM page (partial write %zu/%u bytes)",
                         bytes_written, page_size);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::create_system_state_page(int fd, uint32_t page_size, uint8_t *page_buffer,
                                            const ID &db_uuid, ErrorContext *ctx) -> Status
    {
        memset(page_buffer, 0, page_size);
        auto *state_page = reinterpret_cast<BootstrapSystemStatePage *>(page_buffer);
        auto &header = state_page->page_header;
        header.magic = K_MAGIC_SBRD;
        header.version = 1;
        header.page_type = PAGE_TYPE_SYSTEM_STATE;
        header.page_size = page_size;
        header.page_id = BOOTSTRAP_PAGE_SYSTEM_STATE;
        header.flags = 0;
        header.lsn = 0;
        header.generation = 1;
        setDatabaseUuid(header, db_uuid);
        setObjectUuid(header, ID{});
        header.item_count = 0;
        pageSetLower(header, sizeof(BootstrapSystemStatePage));
        pageSetUpper(header, page_size);
        pageSetSpecial(header, page_size);

        state_page->clean_shutdown = 1;
        state_page->engine_mode = 0;
        state_page->cluster_state = 0;
        state_page->startup_counter = 1;

        header.flags |= PAGE_FLAG_CHECKSUM_VALID;
        header.checksum = calculatePageChecksum(page_buffer, page_size);

        const off_t offset =
            static_cast<off_t>(BOOTSTRAP_PAGE_SYSTEM_STATE) * static_cast<off_t>(page_size);
        size_t bytes_written = 0;
        errno = 0;
        if (!pwriteFully(fd, page_buffer, page_size, offset, &bytes_written))
        {
            char msg[256];
            if (errno != 0)
            {
                snprintf(msg, sizeof(msg), "Failed to write system state page: %s",
                         std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg),
                         "Failed to write system state page (partial write %zu/%u bytes)",
                         bytes_written, page_size);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::create_tx_map_root_page(int fd, uint32_t page_size, uint8_t *page_buffer,
                                           const ID &db_uuid, ErrorContext *ctx) -> Status
    {
        memset(page_buffer, 0, page_size);
        auto *tx_root = reinterpret_cast<TIPPageHeader *>(page_buffer);
        auto &header = tx_root->page_header;
        header.magic = K_MAGIC_SBRD;
        header.version = 1;
        header.page_type = PAGE_TYPE_TRANSACTION_MAP;
        header.page_size = page_size;
        header.page_id = BOOTSTRAP_PAGE_TX_MAP_ROOT;
        header.flags = 0;
        header.lsn = 0;
        header.generation = 1;
        setDatabaseUuid(header, db_uuid);
        setObjectUuid(header, ID{});
        header.item_count = 0;
        pageSetLower(header, sizeof(TIPPageHeader) + (2 * sizeof(TIPEntry)));
        pageSetUpper(header, page_size);
        pageSetSpecial(header, page_size);

        tx_root->min_xid = config::DEFAULT_INITIAL_XID;
        tx_root->max_xid = config::DEFAULT_INITIAL_XID + 1;
        tx_root->num_transactions = 2;
        tx_root->next_tip_page = 0;

        auto *entries =
            reinterpret_cast<TIPEntry *>(page_buffer + sizeof(TIPPageHeader));
        entries[0].xid = config::DEFAULT_INITIAL_XID;
        entries[0].state = static_cast<uint8_t>(TransactionState::COMMITTED);
        entries[0].flags = 0;
        entries[0].reserved = 0;
        entries[0].commit_time = 0;
        entries[1].xid = config::DEFAULT_INITIAL_XID + 1;
        entries[1].state = static_cast<uint8_t>(TransactionState::COMMITTED);
        entries[1].flags = 0;
        entries[1].reserved = 0;
        entries[1].commit_time = 0;

        header.flags |= PAGE_FLAG_CHECKSUM_VALID;
        header.checksum = calculatePageChecksum(page_buffer, page_size);

        const off_t offset =
            static_cast<off_t>(BOOTSTRAP_PAGE_TX_MAP_ROOT) * static_cast<off_t>(page_size);
        size_t bytes_written = 0;
        errno = 0;
        if (!pwriteFully(fd, page_buffer, page_size, offset, &bytes_written))
        {
            char msg[256];
            if (errno != 0)
            {
                snprintf(msg, sizeof(msg), "Failed to write transaction map root page: %s",
                         std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg),
                         "Failed to write transaction map root page (partial write %zu/%u bytes)",
                         bytes_written, page_size);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::create_reserved_bootstrap_page(int fd, uint32_t page_size, uint8_t *page_buffer,
                                                  const ID &db_uuid, ErrorContext *ctx) -> Status
    {
        memset(page_buffer, 0, page_size);
        auto *header = reinterpret_cast<PageHeader *>(page_buffer);
        header->magic = K_MAGIC_SBRD;
        header->version = 1;
        header->page_type = PAGE_TYPE_BOOTSTRAP_RESERVED;
        header->page_size = page_size;
        header->page_id = BOOTSTRAP_PAGE_RESERVED;
        header->flags = 0;
        header->lsn = 0;
        header->generation = 1;
        setDatabaseUuid(*header, db_uuid);
        setObjectUuid(*header, ID{});
        header->item_count = 0;
        pageSetLower(*header, sizeof(PageHeader));
        pageSetUpper(*header, page_size);
        pageSetSpecial(*header, page_size);
        header->flags |= PAGE_FLAG_CHECKSUM_VALID;
        header->checksum = calculatePageChecksum(page_buffer, page_size);

        const off_t offset =
            static_cast<off_t>(BOOTSTRAP_PAGE_RESERVED) * static_cast<off_t>(page_size);
        size_t bytes_written = 0;
        errno = 0;
        if (!pwriteFully(fd, page_buffer, page_size, offset, &bytes_written))
        {
            char msg[256];
            if (errno != 0)
            {
                snprintf(msg, sizeof(msg), "Failed to write reserved bootstrap page: %s",
                         std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg),
                         "Failed to write reserved bootstrap page (partial write %zu/%u bytes)",
                         bytes_written, page_size);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::init_header_page(int fd, const std::string &path, uint32_t page_size,
                                    uint8_t *page_buffer, ErrorContext *ctx) -> Status
    {
        // Create database header
        auto *header = reinterpret_cast<DatabaseHeader *>(page_buffer);

        // Initialize page header
        header->page_header.magic = K_MAGIC_SBRD;
        header->page_header.version = 1;
        header->page_header.page_type = PAGE_TYPE_DATABASE_HEADER;
        header->page_header.page_size = page_size;
        header->page_header.page_id = 0;
        header->page_header.flags = 0;
        header->page_header.lsn = 0;

        // Generate and set database UUID
        ID db_uuid = generateUuidV7();
        header->database_uuid = db_uuid;
        setDatabaseUuid(header->page_header, db_uuid);
        setObjectUuid(header->page_header, ID{});

        // Set MVCC fields
        header->page_header.generation = 1;
        header->page_header.item_count = 0;
        pageSetLower(header->page_header, sizeof(DatabaseHeader));
        pageSetUpper(header->page_header, page_size);
        pageSetSpecial(header->page_header, page_size);

        // Initialize database identification with actual filename
        // EXCEPTION SAFETY (ERROR-CRITICAL-2 Priority 3): Protect string operations
        try
        {
            size_t last_slash = path.find_last_of("/\\\\");
            std::string db_name =
                (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;
            strncpy(header->db_name, db_name.c_str(), 31);
            header->db_name[31] = '\0';
        }
        catch (const std::bad_alloc &)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM,
                              "Out of memory extracting database name from path");
            return Status::OOM;
        }
        header->db_version = DB_VERSION_ALPHA_1_0_1;
        header->db_compat_version = DB_COMPAT_VERSION_ALPHA_1_0_1;

        // Get current time in microseconds
        uint64_t micros = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
        header->creation_time = micros;
        header->last_checkpoint = 0;

        // Set configuration
        header->block_size = page_size;
        header->wal_level = 0; // No WAL in Alpha
        header->max_connections = 1;
        header->encoding = 1; // UTF-8
        header->locale = 0;
        header->timezone = 0;

        // Initialize file layout
        header->total_pages = BOOTSTRAP_FIXED_PAGE_COUNT;
        header->free_pages = 0;
        header->next_page_id = BOOTSTRAP_FIXED_PAGE_COUNT;
        header->system_catalog_page = BOOTSTRAP_PAGE_CATALOG_ROOT;

        // Initialize transaction info
        // Use DEFAULT_INITIAL_XID + 1 for next_xid so that DEFAULT_INITIAL_XID is a valid fallback
        // This ensures tuples inserted without a proper transaction (fallback to DEFAULT_INITIAL_XID)
        // will pass visibility checks (xid < next_xid)
        header->next_transaction_id = config::DEFAULT_INITIAL_XID + 1;
        header->oldest_transaction_id = config::DEFAULT_INITIAL_XID; // OIT - lowest valid user XID
        header->oldest_active_xid = 0;     // OAT - 0 means no active transactions
        header->oldest_snapshot = 0;       // OST - 0 means no snapshot transactions
        header->latest_completed_xid = 0;
        header->tip_root_page = BOOTSTRAP_PAGE_TX_MAP_ROOT;

        // Bootstrap pages are written directly and must carry valid checksum metadata.
        header->page_header.flags |= PAGE_FLAG_CHECKSUM_VALID;
        header->page_header.checksum = calculatePageChecksum(page_buffer, page_size);

        // Write header page with full-transfer semantics.
        size_t bytes_written = 0;
        errno = 0;
        if (!pwriteFully(fd, page_buffer, page_size, 0, &bytes_written))
        {
            char msg[256];
            if (errno != 0)
            {
                snprintf(msg, sizeof(msg), "Failed to write header page: %s", std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg), "Failed to write header page (partial write %zu/%u bytes)",
                         bytes_written, page_size);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::create(const std::string &path, uint32_t page_size, ErrorContext *ctx) -> Status
    {
        std::string canonical_path;
        Status status = Database::validate_db_path(path, canonical_path, ctx);
        if (status != Status::OK)
        {
            // context is already set by validate_db_path
            return status;
        }

        // Validate page size
        if (!isValidAlphaPageSize(page_size))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Invalid page size: must be 8192, 16384, 32768, 65536, or 131072");
            return Status::INVALID_ARGUMENT;
        }

        // Check if file already exists
        struct stat st;
        if (stat(canonical_path.c_str(), &st) == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, "Database file already exists");
            return Status::FILE_EXISTS;
        }

        // Create and open file with exclusive create
        int fd = platform::openFd(canonical_path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
        if (fd < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to create database file");
            return Status::IO_ERROR;
        }

        // Lock file for exclusive access
        int lock_errno = 0;
        if (!lockFileExclusiveNonBlocking(fd, &lock_errno))
        {
            ::close(fd);
            if (lock_errno == EWOULDBLOCK || lock_errno == EAGAIN || lock_errno == EACCES) {
                SET_ERROR_CONTEXT(ctx, Status::LOCK_CONFLICT,
                    "Database file is already in use by another process");
            } else {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to lock database file");
            }
            return Status::LOCK_CONFLICT;
        }

        // Allocate buffer for header page with OOM check
        auto page_buffer = std::make_unique<uint8_t[]>(page_size);
        if (!page_buffer)
        {
            ::close(fd);
            unlink(canonical_path.c_str()); // Clean up file on failure
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate memory for page buffer");
            return Status::OOM;
        }
        memset(page_buffer.get(), 0, page_size);

        status = init_header_page(fd, canonical_path, page_size, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            ::close(fd);
            unlink(canonical_path.c_str());
            return status;
        }

        auto *header = reinterpret_cast<DatabaseHeader *>(page_buffer.get());
        ID db_uuid = header->database_uuid;
        uint64_t micros = header->creation_time;

        status = create_system_state_page(fd, page_size, page_buffer.get(), db_uuid, ctx);
        if (status != Status::OK)
        {
            ::close(fd);
            unlink(canonical_path.c_str());
            return status;
        }

        status = create_catalog_page(fd, page_size, page_buffer.get(), db_uuid, micros, ctx);
        if (status != Status::OK)
        {
            ::close(fd);
            unlink(canonical_path.c_str());
            return status;
        }

        // Create FSM root page (Page 3)
        status = create_fsm_page(fd, page_size, page_buffer.get(), db_uuid, ctx);
        if (status != Status::OK)
        {
            ::close(fd);
            unlink(canonical_path.c_str());
            return status;
        }

        status = create_tx_map_root_page(fd, page_size, page_buffer.get(), db_uuid, ctx);
        if (status != Status::OK)
        {
            ::close(fd);
            unlink(canonical_path.c_str());
            return status;
        }

        status = create_reserved_bootstrap_page(fd, page_size, page_buffer.get(), db_uuid, ctx);
        if (status != Status::OK)
        {
            ::close(fd);
            unlink(canonical_path.c_str());
            return status;
        }

        // Update database header with canonical bootstrap page count.
        size_t bytes_read = 0;
        errno = 0;
        if (!preadFully(fd, page_buffer.get(), page_size, 0, &bytes_read))
        {
            ::close(fd);
            unlink(canonical_path.c_str());
            if (errno != 0)
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Failed to read database header: %s", std::strerror(errno));
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            }
            else
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read database header");
            }
            return Status::IO_ERROR;
        }
        header = reinterpret_cast<DatabaseHeader *>(page_buffer.get());
        header->total_pages = BOOTSTRAP_FIXED_PAGE_COUNT;
        header->next_page_id = BOOTSTRAP_FIXED_PAGE_COUNT;
        header->system_catalog_page = BOOTSTRAP_PAGE_CATALOG_ROOT;
        header->tip_root_page = BOOTSTRAP_PAGE_TX_MAP_ROOT;
        header->page_header.flags |= PAGE_FLAG_CHECKSUM_VALID;
        header->page_header.checksum = calculatePageChecksum(page_buffer.get(), page_size);
        size_t bytes_written = 0;
        errno = 0;
        if (!pwriteFully(fd, page_buffer.get(), page_size, 0, &bytes_written))
        {
            ::close(fd);
            unlink(canonical_path.c_str());
            if (errno != 0)
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Failed to write database header: %s", std::strerror(errno));
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            }
            else
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write database header");
            }
            return Status::IO_ERROR;
        }

        // Sync to disk
        platform::syncFd(fd);

        ::close(fd);

        return Status::OK;
    }

    auto Database::open(const std::string &path, ErrorContext *ctx) -> Status
    {
        std::string canonical_path;
        Status status = Database::validate_db_path(path, canonical_path, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Close if already open
        close();

        // Open file
        fd_ = platform::openFd(canonical_path.c_str(), O_RDWR);
        if (fd_ < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "Database file not found");
            return Status::FILE_NOT_FOUND;
        }

        // Lock file for exclusive access
        int lock_errno = 0;
        if (!lockFileExclusiveNonBlocking(fd_, &lock_errno))
        {
            ::close(fd_);
            fd_ = -1;
            if (lock_errno == EWOULDBLOCK || lock_errno == EAGAIN || lock_errno == EACCES) {
                SET_ERROR_CONTEXT(ctx, Status::LOCK_CONFLICT,
                    "Database file is already in use by another process");
            } else {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to lock database file");
            }
            return Status::LOCK_CONFLICT;
        }

        // Read header to determine page size
        uint8_t temp_header[sizeof(PageHeader)];
        size_t bytes_read = 0;
        errno = 0;
        if (!preadFully(fd_, temp_header, sizeof(temp_header), 0, &bytes_read))
        {
            ::close(fd_);
            fd_ = -1;
            if (errno != 0)
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Failed to read database header: %s", std::strerror(errno));
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            }
            else
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Short read: database file truncated");
            }
            return Status::IO_ERROR;
        }

        auto *ph = reinterpret_cast<PageHeader *>(temp_header);

        // Validate magic
        if (ph->magic != K_MAGIC_SBRD)
        {
            ::close(fd_);
            fd_ = -1;
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid magic number in database header");
            return Status::PAGE_CORRUPT;
        }

        // Validate page size
        if (!isValidAlphaPageSize(ph->page_size))
        {
            ::close(fd_);
            fd_ = -1;
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid page size in database header");
            return Status::PAGE_CORRUPT;
        }

        page_size_ = ph->page_size;

        // LOW-1 FIX: Allocate full header buffer using std::unique_ptr for RAII
        try
        {
            header_buffer_ = std::make_unique<uint8_t[]>(page_size_);
        }
        catch (const std::bad_alloc &)
        {
            ::close(fd_);
            fd_ = -1;
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate memory for database header");
            return Status::OOM;
        }
        header_ = reinterpret_cast<DatabaseHeader *>(header_buffer_.get());

        // Read full header page
        bytes_read = 0;
        errno = 0;
        if (!preadFully(fd_, header_, page_size_, 0, &bytes_read))
        {
            close();
            if (errno != 0)
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Failed to read full header: %s", std::strerror(errno));
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            }
            else
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Short read on full header");
            }
            return Status::IO_ERROR;
        }

        // Validate header
        status = validate_header(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        status = validate_bootstrap_page_map(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Store database UUID
        db_uuid_ = header_->database_uuid;
        // Per-process instance identifier (used to invalidate stale dormant records on restart).
        server_instance_id_ = generateUuidV7();
        path_ = canonical_path;

        // Initialize logger
        Logger::getInstance().initialize();

        // Initialize page manager
        try
        {
            page_manager_ = std::make_unique<PageManager>(this, page_size_);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate PageManager");
            return Status::OOM;
        }
        status = page_manager_->load(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Reconstruct FSM from actual pages (MGA-style recovery)
        // This ensures FSM is always consistent with actual page state,
        // supporting full transaction recovery without WAL
        status = page_manager_->reconstructFromPages(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize buffer pool
        BufferPool::Config bp_config;
        bp_config.pool_size = Config::getInstance().getUInt("memory", "buffer_pool_size", 128);
        bp_config.page_size = page_size_;
        std::string layout_value = Config::getInstance().getString("memory", "buffer_pool_layout",
                                                                   "single");
        bool layout_recognized = false;
        bp_config.layout = parseBufferPoolLayout(layout_value, &layout_recognized);
        if (!layout_recognized && !layout_value.empty())
        {
            LOG_WARNING(GENERAL, "Unknown buffer_pool_layout '%s'; using single pool",
                        layout_value.c_str());
        }
        try
        {
            buffer_pool_ = std::make_unique<BufferPool>(this, bp_config);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate BufferPool");
            return Status::OOM;
        }
        status = buffer_pool_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize catalog manager
        try
        {
            catalog_manager_ = std::make_unique<CatalogManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate CatalogManager");
            return Status::OOM;
        }
        status = catalog_manager_->load(ctx);
        if (status != Status::OK && status != Status::PAGE_CORRUPT)
        {
            // PageCorrupt means catalog not initialized yet, which is OK
            close();
            return status;
        }
        if (status == Status::OK)
        {
            // On restart we cannot reattach previous in-memory contexts, so purge stale records.
            std::vector<CatalogManager::DormantTransactionInfo> dormants;
            Status list_status = catalog_manager_->listDormantTransactions(dormants, ctx);
            if (list_status == Status::OK)
            {
                for (const auto &entry : dormants)
                {
                    if (entry.server_instance_id != server_instance_id_)
                    {
                        catalog_manager_->deleteDormantTransaction(entry.dormant_id, ctx);
                    }
                }
            }
        }

        // Initialize audit logger
        try
        {
            audit_logger_ = std::make_unique<AuditLogger>(catalog_manager_.get());
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate AuditLogger");
            return Status::OOM;
        }

        // Initialize storage engine
        try
        {
            storage_engine_ = std::make_unique<StorageEngine>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate StorageEngine");
            return Status::OOM;
        }

        // Initialize transaction manager
        try
        {
            transaction_manager_ = std::make_unique<TransactionManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate TransactionManager");
            return Status::OOM;
        }
        status = transaction_manager_->load(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        status = catalog_manager_->initializePolicyToastIfNeeded(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }
        status = ensureSysarchUser(catalog_manager_.get(), ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        status = bootstrapI18nResources(this, ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize TID resolver (Sprint 4 Task 5.4.2)
        try
        {
            tid_resolver_ = std::make_unique<TIDResolver>();
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate TIDResolver");
            return Status::OOM;
        }

        // Initialize lock manager
        try
        {
            lock_manager_ = std::make_unique<LockManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate LockManager");
            return Status::OOM;
        }
        status = lock_manager_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize GC manager
        try
        {
            gc_manager_ = std::make_unique<GcManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate GC manager");
            return Status::OOM;
        }

        // Initialize CLOG manager
        try
        {
            clog_ = std::make_unique<Clog>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate Clog");
            return Status::OOM;
        }
        status = clog_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Mark DEFAULT_INITIAL_XID as committed in CLOG
        // This allows tuples inserted without a proper transaction (using fallback XID)
        // to be visible after recovery/restart
        status = clog_->setStatus(config::DEFAULT_INITIAL_XID, ClogStatus::COMMITTED, ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(STORAGE, "Failed to mark DEFAULT_INITIAL_XID as committed in CLOG");
            // Non-fatal - continue initialization
        }

        // Initialize sweep manager
        try
        {
            sweep_manager_ = std::make_unique<SweepManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate SweepManager");
            return Status::OOM;
        }
        status = sweep_manager_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize garbage collector
        try
        {
            garbage_collector_ = std::make_unique<GarbageCollector>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate GarbageCollector");
            return Status::OOM;
        }
        status = garbage_collector_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize long transaction monitor
        try
        {
            long_transaction_monitor_ = std::make_unique<LongTransactionMonitor>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate LongTransactionMonitor");
            return Status::OOM;
        }
        status = long_transaction_monitor_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Start long transaction monitoring thread
        status = long_transaction_monitor_->startMonitoring(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize domain manager
        try
        {
            domain_manager_ = std::make_unique<DomainManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate DomainManager");
            return Status::OOM;
        }
        status = domain_manager_->initialize(ctx);
        if (status != Status::OK)
        {
            // Domain catalog not initialized yet is OK
            if (status != Status::NOT_FOUND && status != Status::PAGE_CORRUPT)
            {
                close();
                return status;
            }
        }
        else
        {
            status = domain_manager_->load(ctx);
            if (status != Status::OK)
            {
                close();
                return status;
            }
            status = domain_manager_->ensureSystemDomains(ctx);
            if (status != Status::OK)
            {
                close();
                return status;
            }
        }

        // Initialize encryption key manager
        try
        {
            encryption_key_manager_ = std::make_unique<EncryptionKeyManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate EncryptionKeyManager");
            return Status::OOM;
        }
        status = encryption_key_manager_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize optimizer runtime components (Phase 1, Task 1.3)
        try
        {
            statistics_manager_ = std::make_unique<optimizer::StatisticsManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate StatisticsManager");
            return Status::OOM;
        }

        // Initialize permission cache (Security Phase 3.2.3)
        try
        {
            permission_cache_ = std::make_unique<PermissionCache>(
                1000,                           // max_entries
                std::chrono::seconds(60));      // TTL
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate PermissionCache");
            return Status::OOM;
        }

        // Initialize job scheduler (WS-4 Scheduler)
        status = applySchedulerConfig(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize virtual catalog handlers (information_schema, pg_catalog, mysql, firebird).
        scratchbird::catalog::initializeVirtualCatalogs(catalog_manager_.get());

        DEBUG_LOG_DB("Database opened successfully");
        return Status::OK;
    }

    auto Database::validate_db_path(const std::string &path, std::string &canonical_path,
                                    ErrorContext *ctx) -> Status
    {
        if (path.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_PATH, "Database path cannot be empty.");
            return Status::INVALID_PATH;
        }

        // EXCEPTION SAFETY (ERROR-CRITICAL-2 Priority 3): Protect path string operations
        try
        {
            std::filesystem::path requested(path);

            // Reject raw traversal segments before canonicalization. Canonicalization can
            // normalize ".." away, but the input still represents a traversal attempt.
            for (const std::filesystem::path& component : requested)
            {
                if (component == "..")
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_PATH,
                                      "Database path cannot contain parent traversal segments.");
                    return Status::INVALID_PATH;
                }
            }

            std::error_code ec;
            std::filesystem::path resolved;

            if (std::filesystem::exists(requested, ec) && !ec)
            {
                resolved = std::filesystem::weakly_canonical(requested, ec);
            }
            else
            {
                // File may not exist yet (create flow): resolve the parent and append filename.
                std::filesystem::path parent = requested.parent_path();
                if (parent.empty())
                {
                    parent = ".";
                }
                std::filesystem::path canonical_parent =
                    std::filesystem::weakly_canonical(parent, ec);
                if (ec)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_PATH,
                                      "Could not resolve database path directory.");
                    return Status::INVALID_PATH;
                }
                resolved = canonical_parent / requested.filename();
            }

            if (ec)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_PATH,
                                  "Could not canonicalize database path.");
                return Status::INVALID_PATH;
            }

            canonical_path = resolved.lexically_normal().string();

            // Note: We no longer restrict paths to current working directory
            // The server process needs to open databases in any location specified by the user
            // Security is enforced at the OS level through file permissions
        }
        catch (const std::bad_alloc &)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM,
                              "Out of memory during path validation");
            return Status::OOM;
        }

        // Basic validation: ensure path is absolute after resolution
        // and doesn't contain suspicious patterns
        if (canonical_path.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_PATH,
                              "Database path resolved to empty string.");
            return Status::INVALID_PATH;
        }

        // Ensure the parent directory exists and is writable
        std::filesystem::path p(canonical_path);
        std::filesystem::path parent_dir = p.parent_path();
        if (!parent_dir.empty() && !std::filesystem::exists(parent_dir))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_PATH,
                              "Parent directory does not exist.");
            return Status::INVALID_PATH;
        }

        return Status::OK;
    }

    auto Database::validate_header(ErrorContext *ctx) -> Status
    {
        if (header_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Database header is null");
            return Status::PAGE_CORRUPT;
        }

        // Check magic
        if (header_->page_header.magic != K_MAGIC_SBRD)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid magic number in database header");
            return Status::PAGE_CORRUPT;
        }

        // Check page type
        if (header_->page_header.page_type != PAGE_TYPE_DATABASE_HEADER)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid page type in database header");
            return Status::PAGE_CORRUPT;
        }

        // Check page ID
        if (header_->page_header.page_id != 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Database header page ID must be 0");
            return Status::PAGE_CORRUPT;
        }

        // Check block size consistency
        if (header_->block_size != header_->page_header.page_size)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Block size inconsistency in database header");
            return Status::PAGE_CORRUPT;
        }

        if (header_->system_catalog_page != BOOTSTRAP_PAGE_CATALOG_ROOT)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "Database header system catalog root is not canonical page 2");
            return Status::PAGE_CORRUPT;
        }

        if (header_->tip_root_page != BOOTSTRAP_PAGE_TX_MAP_ROOT)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "Database header transaction map root is not canonical page 4");
            return Status::PAGE_CORRUPT;
        }

        if (header_->total_pages < BOOTSTRAP_FIXED_PAGE_COUNT)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "Database header total_pages below fixed bootstrap map");
            return Status::PAGE_CORRUPT;
        }

        // Validate checksum
        if (!validatePageChecksum(reinterpret_cast<uint8_t *>(header_), page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH, "Database header checksum validation failed");
            return Status::CHECKSUM_MISMATCH;
        }

        return Status::OK;
    }

    auto Database::validate_bootstrap_page_map(ErrorContext *ctx) const -> Status
    {
        struct ExpectedPage
        {
            uint32_t page_id;
            uint16_t page_type;
        };

        constexpr ExpectedPage kExpected[] = {
            {BOOTSTRAP_PAGE_SYSTEM_STATE, PAGE_TYPE_SYSTEM_STATE},
            {BOOTSTRAP_PAGE_CATALOG_ROOT, PAGE_TYPE_CATALOG_ROOT},
            {BOOTSTRAP_PAGE_FSM_ROOT, PAGE_TYPE_FSM_ROOT},
            {BOOTSTRAP_PAGE_TX_MAP_ROOT, PAGE_TYPE_TRANSACTION_MAP},
            {BOOTSTRAP_PAGE_RESERVED, PAGE_TYPE_BOOTSTRAP_RESERVED},
        };

        auto page_buffer = std::make_unique<uint8_t[]>(page_size_);
        if (!page_buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM,
                              "Failed to allocate bootstrap validation buffer");
            return Status::OOM;
        }

        for (const auto &expected : kExpected)
        {
            off_t offset = static_cast<off_t>(expected.page_id) * page_size_;
            size_t bytes_read = 0;
            if (!preadFully(fd_, page_buffer.get(), page_size_, offset, &bytes_read))
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Failed to read bootstrap page %u (%zu/%u bytes)",
                         expected.page_id,
                         bytes_read,
                         page_size_);
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
                return Status::IO_ERROR;
            }

            auto *header = reinterpret_cast<PageHeader *>(page_buffer.get());
            if (header->magic != K_MAGIC_SBRD)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Bootstrap page %u has invalid magic", expected.page_id);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, msg);
                return Status::PAGE_CORRUPT;
            }
            if (header->page_size != page_size_)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Bootstrap page %u has invalid page_size", expected.page_id);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, msg);
                return Status::PAGE_CORRUPT;
            }
            if (header->page_id != expected.page_id)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Bootstrap page id mismatch for page %u", expected.page_id);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, msg);
                return Status::PAGE_CORRUPT;
            }
            if (header->page_type != expected.page_type)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Bootstrap page %u has wrong type %u",
                         expected.page_id, static_cast<unsigned>(header->page_type));
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, msg);
                return Status::PAGE_CORRUPT;
            }
            if (!validatePageChecksum(page_buffer.get(), page_size_))
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Bootstrap page %u checksum validation failed", expected.page_id);
                SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH, msg);
                return Status::CHECKSUM_MISMATCH;
            }
        }

        return Status::OK;
    }

    auto Database::read_page(uint32_t page_id, void *buffer, ErrorContext *ctx) const -> Status
    {
        if (fd_ < 0 || (buffer == nullptr))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to read_page");
            return Status::INVALID_ARGUMENT;
        }

        // NEW ISSUE FIX: Use pread() instead of lseek()+read() for thread-safe I/O
        // pread() is atomic and doesn't modify the file offset, preventing race conditions
        // when multiple threads access different pages concurrently
        off_t offset = static_cast<off_t>(page_id) * page_size_;
        size_t bytes_read = 0;
        errno = 0;
        if (!preadFully(fd_, buffer, page_size_, offset, &bytes_read))
        {
            char msg[256];
            if (errno != 0)
            {
                snprintf(msg, sizeof(msg),
                         "Read failed for page %u after %zu/%u bytes: %s",
                         page_id, bytes_read, page_size_, std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg), "Short read on page (%zu/%u bytes)",
                         bytes_read, page_size_);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        // Validate page
        auto *header = reinterpret_cast<PageHeader *>(buffer);

        if (header->magic != K_MAGIC_SBRD)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid magic number");
            return Status::PAGE_CORRUPT;
        }

        if (header->page_id != page_id)
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Page ID mismatch: expected %u, got %lu", page_id,
                     static_cast<unsigned long>(header->page_id));
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, msg);
            return Status::PAGE_CORRUPT;
        }

        if (!validatePageChecksum(reinterpret_cast<uint8_t *>(buffer), page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH, "Checksum validation failed");
            return Status::CHECKSUM_MISMATCH;
        }

        return Status::OK;
    }

    auto Database::write_page(uint32_t page_id, const void *buffer, ErrorContext *ctx) -> Status
    {
        if (fd_ < 0 || (buffer == nullptr))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to write_page");
            return Status::INVALID_ARGUMENT;
        }

        // Finalize page header contract before durable write.
        auto *page = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(buffer));
        preparePageForWrite(page, page_size_, page_id);

        // NEW ISSUE FIX: Use pwrite() instead of lseek()+write() for thread-safe I/O
        // pwrite() is atomic and doesn't modify the file offset, preventing race conditions
        // when multiple threads write different pages concurrently (e.g., background flush + active writes)
        off_t offset = static_cast<off_t>(page_id) * page_size_;
        size_t bytes_written = 0;
        errno = 0;
        if (!pwriteFully(fd_, buffer, page_size_, offset, &bytes_written))
        {
            char msg[256];
            if (errno != 0)
            {
                snprintf(msg, sizeof(msg),
                         "Write failed for page %u after %zu/%u bytes: %s",
                         page_id, bytes_written, page_size_, std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg), "Short write on page (%zu/%u bytes)",
                         bytes_written, page_size_);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::sync(ErrorContext *ctx) const -> Status
    {
        if (fd_ < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
            return Status::INVALID_ARGUMENT;
        }

        if (platform::syncFd(fd_) != 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to sync database file");
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::read_page_partial(uint32_t page_id, void *buffer, uint32_t size, uint32_t offset,
                                     ErrorContext *ctx) const -> Status
    {
        if (fd_ < 0 || (buffer == nullptr))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Invalid arguments to read_page_partial");
            return Status::INVALID_ARGUMENT;
        }

        if (offset + size > page_size_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Partial read exceeds page boundary");
            return Status::INVALID_ARGUMENT;
        }

        off_t file_offset = (static_cast<off_t>(page_id) * page_size_) + offset;
        size_t bytes_read = 0;
        errno = 0;
        if (!preadFully(fd_, buffer, size, file_offset, &bytes_read))
        {
            if (errno != 0)
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Failed partial read for page %u: %s",
                         page_id, std::strerror(errno));
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            }
            else
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Short read on partial page");
            }
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    // === PHASE 1, TASK 1.2.4: GPID-based I/O methods ===

    auto Database::read_page_global(GPID gpid, void *buffer, ErrorContext *ctx) const -> Status
    {
        if (fd_ < 0 || (buffer == nullptr))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to read_page_global");
            return Status::INVALID_ARGUMENT;
        }

        // Extract tablespace_id and page_number from GPID
        uint16_t tablespace_id = getTablespaceID(gpid);
        uint64_t page_number = getPageNumber(gpid);

        // Get file descriptor for the tablespace
        int tablespace_fd = -1;
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            // Primary tablespace uses main database file descriptor
            tablespace_fd = fd_;

            // Validate page_number fits in uint32_t for primary tablespace
            if (page_number > UINT32_MAX)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Page number %lu exceeds uint32_t maximum for primary tablespace",
                         page_number);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, msg);
                return Status::INVALID_ARGUMENT;
            }
        }
        else
        {
            // Custom tablespace - get file descriptor from registry
            tablespace_fd = getTablespaceFd(tablespace_id);
            if (tablespace_fd < 0)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Tablespace %u not found or not open", tablespace_id);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, msg);
                return Status::INVALID_ARGUMENT;
            }
        }

        // Calculate offset in file (page_number * page_size_)
        off_t offset = static_cast<off_t>(page_number) * static_cast<off_t>(page_size_);

        // Read page from tablespace file
        errno = 0;
        size_t bytes_read = 0;
        if (!preadFully(tablespace_fd, buffer, page_size_, offset, &bytes_read))
        {
            char msg[256];
            if (errno != 0)
            {
                snprintf(msg, sizeof(msg),
                         "Failed to read page %lu from tablespace %u: %s",
                         page_number, tablespace_id, std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg),
                         "Partial read: expected %u bytes, got %zu bytes for page %lu in tablespace %u",
                         page_size_, bytes_read, page_number, tablespace_id);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::write_page_global(GPID gpid, const void *buffer, ErrorContext *ctx) -> Status
    {
        if (buffer == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        // Extract tablespace_id and page_number from GPID
        uint16_t tablespace_id = getTablespaceID(gpid);
        uint64_t page_number = getPageNumber(gpid);

        // Get file descriptor for the tablespace
        int tablespace_fd = -1;
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            // Primary tablespace uses main database file descriptor
            if (fd_ < 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
                return Status::INVALID_ARGUMENT;
            }
            tablespace_fd = fd_;

            // Validate page_number fits in uint32_t for primary tablespace
            if (page_number > UINT32_MAX)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Page number %lu exceeds uint32_t maximum for primary tablespace",
                         page_number);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, msg);
                return Status::INVALID_ARGUMENT;
            }
        }
        else
        {
            // Custom tablespace - get file descriptor from registry
            tablespace_fd = getTablespaceFd(tablespace_id);
            if (tablespace_fd < 0)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Tablespace %u not found or not open", tablespace_id);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, msg);
                return Status::INVALID_ARGUMENT;
            }
        }

        // Calculate offset in file (page_number * page_size_)
        off_t offset = static_cast<off_t>(page_number) * static_cast<off_t>(page_size_);

        // Write page to tablespace file
        errno = 0;
        size_t bytes_written = 0;
        if (!pwriteFully(tablespace_fd, buffer, page_size_, offset, &bytes_written))
        {
            char msg[256];
            if (errno != 0)
            {
                snprintf(msg, sizeof(msg),
                         "Failed to write page %lu to tablespace %u: %s",
                         page_number, tablespace_id, std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg),
                         "Partial write: expected %u bytes, wrote %zu bytes for page %lu in tablespace %u",
                         page_size_, bytes_written, page_number, tablespace_id);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::allocate_page_id_global(uint16_t tablespace_id, GPID *gpid_out,
                                          ErrorContext *ctx) -> Status
    {
        if (!is_open())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
            return Status::INVALID_ARGUMENT;
        }

        if (gpid_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "gpid_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        // Delegate to PageManager for GPID allocation (handles both primary and custom tablespaces)
        return page_manager_->allocatePageInTablespace(tablespace_id, gpid_out, ctx);
    }

    auto Database::update_header_total_pages(uint32_t total_pages, ErrorContext *ctx) -> Status
    {
        if (fd_ < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
            return Status::INVALID_ARGUMENT;
        }

        // Update in-memory header
        if (header_ != nullptr)
        {
            header_->total_pages = total_pages;
        }

        // Pin header page through buffer pool to update it
        if (buffer_pool_ != nullptr)
        {
            void *header_buffer;
            Status status = buffer_pool_->pinPage(0, &header_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
            db_header->total_pages = total_pages;
            db_header->page_header.checksum =
                calculatePageChecksum(reinterpret_cast<uint8_t *>(db_header), page_size_);

            // Unpin as dirty
            buffer_pool_->unpinPage(0, true, ctx);
        }

        return Status::OK;
    }

    auto Database::update_header_next_xid(uint64_t next_xid, ErrorContext *ctx) -> Status
    {
        if (fd_ < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
            return Status::INVALID_ARGUMENT;
        }

        if (header_ != nullptr)
        {
            header_->next_transaction_id = next_xid;
        }

        if (buffer_pool_ != nullptr)
        {
            void *header_buffer;
            Status status = buffer_pool_->pinPage(0, &header_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
            if (db_header->next_transaction_id < next_xid)
            {
                db_header->next_transaction_id = next_xid;
                db_header->page_header.checksum =
                    calculatePageChecksum(reinterpret_cast<uint8_t *>(db_header), page_size_);
                buffer_pool_->unpinPage(0, true, ctx);
            }
            else
            {
                buffer_pool_->unpinPage(0, false, ctx);
            }
        }

        return Status::OK;
    }

    auto Database::initializeProcArray(uint32_t max_backends, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> init_lock(proc_array_init_mutex_);

        if (!is_open())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
            return Status::INVALID_ARGUMENT;
        }

        if (!header_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database header not loaded");
            return Status::INVALID_ARGUMENT;
        }

        uint32_t requested_max = header_->max_backends ? header_->max_backends : max_backends;
        if (requested_max == 0)
        {
            requested_max = config::DEFAULT_MAX_BACKENDS;
        }

        if (header_->proc_array_initialized)
        {
            return ProcArrayManager::initialize(this, requested_max, ctx);
        }

        // Initialize ProcArray
        Status status = ProcArrayManager::initialize(this, requested_max, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Update database header
        header_->max_backends = requested_max;
        header_->proc_array_initialized = 1;

        // Persist header changes
        void *header_buffer;
        status = buffer_pool_->pinPage(0, &header_buffer, ctx);
        if (status != Status::OK)
        {
            ProcArrayManager::shutdown(ctx);
            return status;
        }

        auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
        db_header->max_backends = requested_max;
        db_header->proc_array_initialized = 1;
        db_header->page_header.checksum =
            calculatePageChecksum(reinterpret_cast<uint8_t *>(db_header), page_size_);

        buffer_pool_->unpinPage(0, true, ctx);

        return sync(ctx);
    }

    auto Database::shutdownProcArray(ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> init_lock(proc_array_init_mutex_);

        if (!header_)
        {
            return Status::OK;
        }

        if (!header_->proc_array_initialized)
        {
            return Status::OK;
        }

        Status status = ProcArrayManager::shutdown(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Update header
        header_->proc_array_initialized = 0;

        return Status::OK;
    }

    auto Database::allocate_page_id(uint32_t *page_id_out, ErrorContext *ctx) -> Status
    {
        if (!is_open())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
            return Status::INVALID_ARGUMENT;
        }

        if (page_id_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "page_id_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        if (header_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database header not loaded");
            return Status::INVALID_ARGUMENT;
        }

        // Atomically allocate a new page ID by incrementing next_page_id
        // NOTE: This is a simple sequential allocation strategy
        // Future enhancements: free list, page recycling, etc.
        uint32_t new_page_id = static_cast<uint32_t>(header_->next_page_id);

        // Check for overflow (page_id is uint32_t, max 4 billion pages)
        if (new_page_id == UINT32_MAX)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Page ID overflow: maximum number of pages reached");
            return Status::INVALID_ARGUMENT;
        }

        // Increment next_page_id for next allocation
        header_->next_page_id++;
        header_->total_pages++;

        // Persist the updated header to disk
        // Pin header page through buffer pool to update it
        if (buffer_pool_ != nullptr)
        {
            void *header_buffer;
            Status status = buffer_pool_->pinPage(0, &header_buffer, ctx);
            if (status != Status::OK)
            {
                // Rollback on failure
                header_->next_page_id--;
                header_->total_pages--;
                return status;
            }

            auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
            db_header->total_pages = header_->total_pages;
            db_header->page_header.checksum =
                calculatePageChecksum(reinterpret_cast<uint8_t *>(db_header), page_size_);

            // Unpin as dirty
            buffer_pool_->unpinPage(0, true, ctx);
        }

        *page_id_out = new_page_id;
        return Status::OK;
    }

    // ========================================================================
    // PHASE 1, TASK 1.3.4: Tablespace File Descriptor Management
    // ========================================================================

    Status Database::registerTablespaceFile(uint16_t tablespace_id, int fd, ErrorContext *ctx)
    {
        // Validate tablespace ID
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Cannot register tablespace 0 (primary database file)");
            return Status::INVALID_ARGUMENT;
        }

        if (fd < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Invalid file descriptor: " + std::to_string(fd)).c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Thread-safe insertion
        std::lock_guard<std::mutex> lock(tablespace_mutex_);

        // Check if already registered
        if (tablespace_fds_.find(tablespace_id) != tablespace_fds_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS,
                             ("Tablespace " + std::to_string(tablespace_id) +
                              " is already registered").c_str());
            return Status::FILE_EXISTS;
        }

        // Register file descriptor
        tablespace_fds_[tablespace_id] = fd;
        return Status::OK;
    }

    Status Database::unregisterTablespaceFile(uint16_t tablespace_id, ErrorContext *ctx)
    {
        // Validate tablespace ID
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Cannot unregister tablespace 0 (primary database file)");
            return Status::INVALID_ARGUMENT;
        }

        // Thread-safe removal
        std::lock_guard<std::mutex> lock(tablespace_mutex_);

        auto it = tablespace_fds_.find(tablespace_id);
        if (it == tablespace_fds_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                             ("Tablespace " + std::to_string(tablespace_id) + " not found").c_str());
            return Status::NOT_FOUND;
        }

        // Close file descriptor
        int fd = it->second;
        if (fd >= 0)
        {
            ::close(fd);
        }

        // Remove from map
        tablespace_fds_.erase(it);
        return Status::OK;
    }

    int Database::getTablespaceFd(uint16_t tablespace_id) const
    {
        // PRIMARY_TABLESPACE_ID (0) uses primary database fd_
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            return fd_;
        }

        // Thread-safe lookup
        std::lock_guard<std::mutex> lock(tablespace_mutex_);

        auto it = tablespace_fds_.find(tablespace_id);
        if (it == tablespace_fds_.end())
        {
            return -1; // Not found
        }

        return it->second;
    }

} // namespace scratchbird::core
