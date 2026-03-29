/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// Section 35 invariant: database.cpp is a primary authority for native startup
// legality, recovery classification, and persistence coordination. It must not
// be read as evidence of WAL-style replay or donor-log failover machinery.
// Section 37 invariant: database.cpp is adjacent to bootstrap materialization
// and catalog-root ownership, but it is not the primary owner of durable schema
// metadata beyond those bounded bootstrap and startup surfaces.

#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/mga_backout_engine.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/tid_resolver.h" // Sprint 4 Task 5.4.2
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/gc_manager.h"
#include "scratchbird/core/clog.h"
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/long_transaction_monitor.h"
#include "scratchbird/core/mga_failpoint_manager.h"
#include "scratchbird/core/job_scheduler.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/encryption_key_manager.h"
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/table_stats_manager.h"
#include "scratchbird/core/workload_governance.h"
#include "scratchbird/core/charset_loader.h"
#include "scratchbird/core/timezone_loader.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/permission_cache.h" // Security Phase 3.2.3
#include "scratchbird/core/password_hash.h"
#include "scratchbird/core/portable_file_io.h"
#include "scratchbird/core/storage_lock_provider.h"
#include "scratchbird/core/time_source.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/security/scram_auth.h"
#include <nlohmann/json.hpp>
#include <openssl/md5.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cerrno>
#include <exception>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <climits>
#include <limits>
#include <unordered_set>
#include <cstdlib>
#if defined(_WIN32)
    #include <io.h>
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

        std::string trimAscii(std::string value)
        {
            const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
            value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), is_space));
            value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(), value.end());
            return value;
        }

        auto isZeroUuidLocal(const ID& id) -> bool
        {
            for (uint8_t byte : id.bytes)
            {
                if (byte != 0)
                {
                    return false;
                }
            }
            return true;
        }

        void parseDormantSessionSettings(const std::string& payload,
                                         std::vector<std::string>& search_path_out,
                                         std::string& dialect_tag_out,
                                         uint8_t& sql_dialect_out,
                                         std::string& charset_out,
                                         uint32_t& statement_timeout_out)
        {
            if (payload.empty())
            {
                return;
            }

            try
            {
                const nlohmann::json doc = nlohmann::json::parse(payload);
                if (doc.contains("search_path") && doc["search_path"].is_array())
                {
                    search_path_out.clear();
                    for (const auto& entry : doc["search_path"])
                    {
                        if (entry.is_string())
                        {
                            search_path_out.push_back(entry.get<std::string>());
                        }
                    }
                }

                if (doc.contains("dialect_tag") && doc["dialect_tag"].is_string())
                {
                    dialect_tag_out = doc["dialect_tag"].get<std::string>();
                }

                if (doc.contains("sql_dialect") && doc["sql_dialect"].is_number_unsigned())
                {
                    sql_dialect_out = static_cast<uint8_t>(doc["sql_dialect"].get<uint32_t>());
                }

                if (doc.contains("charset") && doc["charset"].is_string())
                {
                    charset_out = doc["charset"].get<std::string>();
                }

                if (doc.contains("statement_timeout") &&
                    doc["statement_timeout"].is_number_unsigned())
                {
                    statement_timeout_out = doc["statement_timeout"].get<uint32_t>();
                }
            }
            catch (const std::exception&)
            {
                // Dormant session settings are advisory for replacement transaction
                // recovery. Invalid JSON must not block restart recovery.
            }
        }

        auto parseDormantRestartReattachPolicy(const std::string& value)
            -> Database::DormantRestartReattachPolicy
        {
            const std::string normalized = toLowerAscii(trimAscii(value));
            if (normalized == "deny" ||
                normalized == "deny_after_restart" ||
                normalized == "deny-after-restart" ||
                normalized == "no_restart_reattach")
            {
                return Database::DormantRestartReattachPolicy::DENY_AFTER_RESTART;
            }
            return Database::DormantRestartReattachPolicy::ALLOW_REPLACEMENT;
        }

        auto parseDormantCleanupPolicy(const std::string& value)
            -> Database::DormantCleanupPolicy
        {
            const std::string normalized = toLowerAscii(trimAscii(value));
            if (normalized == "keep")
            {
                return Database::DormantCleanupPolicy::KEEP;
            }
            if (normalized == "expire_only" || normalized == "expire-only")
            {
                return Database::DormantCleanupPolicy::EXPIRE_ONLY;
            }
            if (normalized == "rollback_expired_and_purge" ||
                normalized == "rollback-expired-and-purge")
            {
                return Database::DormantCleanupPolicy::ROLLBACK_EXPIRED_AND_PURGE;
            }
            return Database::DormantCleanupPolicy::ROLLBACK_EXPIRED;
        }

        auto isDormantTerminalState(CatalogManager::DormantTransactionState state) -> bool
        {
            using DTS = CatalogManager::DormantTransactionState;
            return state == DTS::REATTACHED ||
                   state == DTS::ROLLED_BACK ||
                   state == DTS::EXPIRED;
        }

        auto recoverDormantAfterRestart(Database *db,
                                        const CatalogManager::DormantTransactionInfo& info,
                                        const ID& reattach_authkey,
                                        std::unique_ptr<ConnectionContext>& connection_out,
                                        ErrorContext *ctx) -> Status
        {
            if (db == nullptr || db->catalog_manager() == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Catalog manager not available for dormant recovery");
                return Status::INVALID_ARGUMENT;
            }

            std::unique_ptr<ConnectionContext> recovered;
            Status status = db->connect(recovered, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status,
                                  "Failed to create replacement connection for dormant recovery");
                return status;
            }

            recovered->setAttachmentId(info.attachment_id);
            recovered->setProtocolSessionId(info.session_id);

            std::vector<std::string> search_path;
            std::string dialect_tag = "native";
            uint8_t sql_dialect = recovered->sql_dialect();
            std::string charset = recovered->charset();
            uint32_t statement_timeout = recovered->statement_timeout();
            parseDormantSessionSettings(info.session_settings,
                                        search_path,
                                        dialect_tag,
                                        sql_dialect,
                                        charset,
                                        statement_timeout);

            recovered->setSessionContext(info.session_id,
                                         reattach_authkey,
                                         dialect_tag.empty() ? "native" : dialect_tag,
                                         0,
                                         0);
            if (!dialect_tag.empty())
            {
                recovered->set_dialect_tag(dialect_tag);
            }
            recovered->set_sql_dialect(sql_dialect);
            recovered->set_charset(charset);
            recovered->set_statement_timeout(statement_timeout);
            if (!search_path.empty())
            {
                recovered->set_search_path(search_path);
            }

            const bool wait_for_locks = info.wait_mode == CatalogManager::DormantWaitMode::WAIT;
            recovered->setAutocommitMode(info.autocommit_mode);
            recovered->setWaitForLocks(wait_for_locks);
            recovered->setLockTimeout(info.lock_timeout_seconds);

            CatalogManager *catalog = db->catalog_manager();
            if (!isZeroUuidLocal(info.session_user_id))
            {
                CatalogManager::UserInfo session_user{};
                status = catalog->getUser(info.session_user_id, session_user, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status,
                                      "Failed to restore dormant session user during restart recovery");
                    return status;
                }
                recovered->setCurrentUser(info.session_user_id, session_user.is_superuser);
            }

            if (!isZeroUuidLocal(info.user_id) && info.user_id != info.session_user_id)
            {
                CatalogManager::UserInfo effective_user{};
                status = catalog->getUser(info.user_id, effective_user, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status,
                                      "Failed to restore dormant effective user during restart recovery");
                    return status;
                }
                recovered->setCurrentUser(info.user_id, effective_user.is_superuser);
            }
            else if (!isZeroUuidLocal(info.user_id) && isZeroUuidLocal(info.session_user_id))
            {
                CatalogManager::UserInfo effective_user{};
                status = catalog->getUser(info.user_id, effective_user, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status,
                                      "Failed to restore dormant user during restart recovery");
                    return status;
                }
                recovered->setCurrentUser(info.user_id, effective_user.is_superuser);
            }

            if (!isZeroUuidLocal(info.role_id))
            {
                recovered->setActiveRole(info.role_id);
            }

            if (!isZeroUuidLocal(info.current_schema_id))
            {
                recovered->setCurrentSchemaId(info.current_schema_id);
                CatalogManager::SchemaInfo schema{};
                if (catalog->getSchema(info.current_schema_id, schema, nullptr) == Status::OK)
                {
                    recovered->set_current_schema(schema.full_path.empty()
                                                      ? schema.schema_name
                                                      : schema.full_path);
                }
            }

            IsolationLevel recovered_isolation =
                static_cast<IsolationLevel>(info.isolation_level);
            ReadCommittedMode recovered_rc_mode = ReadCommittedMode::DEFAULT;
            if (recovered_isolation == IsolationLevel::READ_COMMITTED_READ_CONSISTENCY)
            {
                recovered_rc_mode = ReadCommittedMode::READ_CONSISTENCY;
            }
            recovered->setReadCommittedMode(recovered_rc_mode);
            recovered->stageTransactionSettings(
                recovered_isolation,
                info.access_mode == CatalogManager::DormantAccessMode::READ_ONLY,
                wait_for_locks,
                info.lock_timeout_seconds,
                recovered_rc_mode);

            ErrorContext restart_ctx;
            status = recovered->rollback(&restart_ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx,
                                  status,
                                  restart_ctx.message.empty()
                                      ? "Failed to open replacement transaction during dormant restart recovery"
                                      : restart_ctx.message.c_str());
                return status;
            }

            recovered->pushNotice(
                "Dormant transaction recovered after server restart; prior active transaction "
                "was closed and a replacement transaction was opened.");

            connection_out = std::move(recovered);
            return Status::OK;
        }

        bool preadFully(int fd, void *buffer, size_t size, off_t offset,
                        size_t *bytes_read_out = nullptr);
        bool pwriteFully(int fd, const void *buffer, size_t size, off_t offset,
                         size_t *bytes_written_out = nullptr);

        auto formatStartupRepairPlanMask(uint64_t repair_plan_mask) -> std::string
        {
            if (repair_plan_mask == Database::STARTUP_REPAIR_PLAN_NONE)
            {
                return "none";
            }

            std::string result;
            auto append = [&](Database::StartupRepairPlan flag, const char *token)
            {
                if ((repair_plan_mask & static_cast<uint64_t>(flag)) == 0)
                {
                    return;
                }
                if (!result.empty())
                {
                    result += "|";
                }
                result += token;
            };

            append(Database::STARTUP_REPAIR_PLAN_DIAGNOSTIC_SCAN, "diagnostic_scan");
            append(Database::STARTUP_REPAIR_PLAN_RELINKABLE_CHAIN_REPAIR,
                   "relinkable_chain_repair");
            append(Database::STARTUP_REPAIR_PLAN_CHAIN_REWRITE_REVIEW, "chain_rewrite_review");
            append(Database::STARTUP_REPAIR_PLAN_READ_ONLY_QUARANTINE, "read_only_quarantine");
            append(Database::STARTUP_REPAIR_PLAN_REBUILD_FSM, "rebuild_fsm");
            append(Database::STARTUP_REPAIR_PLAN_RESTORE_FROM_BACKUP, "restore_from_backup");
            return result;
        }

        auto startupCorruptionClassName(Database::StartupCorruptionClass value) -> const char *
        {
            switch (value)
            {
                case Database::StartupCorruptionClass::NONE:
                    return "none";
                case Database::StartupCorruptionClass::RELINKABLE_ONLY:
                    return "relinkable_only";
                case Database::StartupCorruptionClass::REPAIR_REQUIRED:
                    return "repair_required";
                case Database::StartupCorruptionClass::QUARANTINE_REQUIRED:
                    return "quarantine_required";
                case Database::StartupCorruptionClass::STARTUP_REFUSAL:
                    return "startup_refusal";
            }
            return "unknown";
        }

        auto startupQuarantineActionName(Database::StartupQuarantineAction value) -> const char *
        {
            switch (value)
            {
                case Database::StartupQuarantineAction::NONE:
                    return "none";
                case Database::StartupQuarantineAction::READ_ONLY:
                    return "read_only";
                case Database::StartupQuarantineAction::REFUSE_OPEN:
                    return "refuse_open";
            }
            return "unknown";
        }

        auto startupRecoveryClassificationName(
            Database::StartupRecoveryClassification value) -> const char *
        {
            switch (value)
            {
                case Database::StartupRecoveryClassification::NOT_CLASSIFIED:
                    return "not_classified";
                case Database::StartupRecoveryClassification::CLEAN_SHUTDOWN_FAST_PATH:
                    return "clean_shutdown_fast_path";
                case Database::StartupRecoveryClassification::
                    DIRTY_SHUTDOWN_NORMALIZATION_REQUIRED:
                    return "dirty_shutdown_normalization_required";
                case Database::StartupRecoveryClassification::REPAIRABLE_PAGE_DAMAGE:
                    return "repairable_page_damage";
                case Database::StartupRecoveryClassification::WRITEBACK_FAILURE_RESUME:
                    return "writeback_failure_resume";
                case Database::StartupRecoveryClassification::CATALOG_OR_CONTROL_DAMAGE_FATAL:
                    return "catalog_or_control_damage_fatal";
            }
            return "unknown";
        }

        auto startupServiceStateName(Database::StartupServiceState value) -> const char *
        {
            switch (value)
            {
                case Database::StartupServiceState::NORMAL:
                    return "normal";
                case Database::StartupServiceState::DEGRADED_READ_WRITE:
                    return "degraded_read_write";
                case Database::StartupServiceState::WRITE_FENCED:
                    return "write_fenced";
                case Database::StartupServiceState::FATAL:
                    return "fatal";
            }
            return "unknown";
        }

        struct CheckpointControlState
        {
            uint64_t version = SYSTEM_STATE_CHECKPOINT_VERSION;
            uint64_t checkpoint_generation = 0;
            CheckpointLifecycleState checkpoint_state = CheckpointLifecycleState::IDLE;
            uint64_t checkpoint_start_time = 0;
            uint64_t captured_oit = 0;
            uint64_t captured_oat = 0;
            uint64_t captured_ost = 0;
            uint64_t dirty_generation_low_watermark = 0;
            uint64_t dirty_generation_high_watermark = 0;
            uint64_t captured_flush_debt_pages = 0;
            uint64_t sweep_generation_seen = 0;
            bool queue_rebuild_required = false;
            CheckpointShutdownIntent shutdown_intent = CheckpointShutdownIntent::NONE;
            Status checkpoint_failure_reason = Status::OK;
        };

        void loadCheckpointControlState(const BootstrapSystemStatePage &state_page,
                                        CheckpointControlState *control_out)
        {
            if (control_out == nullptr)
            {
                return;
            }

            CheckpointControlState control{};
            control.version = state_page.reserved[SYSTEM_STATE_CHECKPOINT_VERSION_SLOT];
            if (control.version == 0)
            {
                control.version = SYSTEM_STATE_CHECKPOINT_VERSION;
                control.checkpoint_generation = state_page.last_clean_shutdown_generation;
                control.shutdown_intent = state_page.clean_shutdown != 0
                    ? CheckpointShutdownIntent::CLEAN
                    : CheckpointShutdownIntent::NONE;
                *control_out = control;
                return;
            }

            control.checkpoint_generation =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_GENERATION_SLOT];
            control.checkpoint_state = static_cast<CheckpointLifecycleState>(
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_STATE_SLOT]);
            control.checkpoint_start_time =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_START_TIME_SLOT];
            control.captured_oit =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_CAPTURED_OIT_SLOT];
            control.captured_oat =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_CAPTURED_OAT_SLOT];
            control.captured_ost =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_CAPTURED_OST_SLOT];
            control.dirty_generation_low_watermark =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_DIRTY_LOW_SLOT];
            control.dirty_generation_high_watermark =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_DIRTY_HIGH_SLOT];
            control.captured_flush_debt_pages =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_FLUSH_DEBT_SLOT];
            control.sweep_generation_seen =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_SWEEP_GENERATION_SLOT];
            control.queue_rebuild_required =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_QUEUE_REBUILD_SLOT] != 0;
            control.shutdown_intent = static_cast<CheckpointShutdownIntent>(
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_SHUTDOWN_INTENT_SLOT]);
            control.checkpoint_failure_reason = static_cast<Status>(
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_FAILURE_REASON_SLOT]);
            *control_out = control;
        }

        void storeCheckpointControlState(BootstrapSystemStatePage *state_page,
                                         const CheckpointControlState &control)
        {
            if (state_page == nullptr)
            {
                return;
            }

            state_page->reserved[SYSTEM_STATE_CHECKPOINT_VERSION_SLOT] = control.version;
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_GENERATION_SLOT] =
                control.checkpoint_generation;
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_STATE_SLOT] =
                static_cast<uint64_t>(control.checkpoint_state);
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_START_TIME_SLOT] =
                control.checkpoint_start_time;
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_CAPTURED_OIT_SLOT] =
                control.captured_oit;
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_CAPTURED_OAT_SLOT] =
                control.captured_oat;
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_CAPTURED_OST_SLOT] =
                control.captured_ost;
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_DIRTY_LOW_SLOT] =
                control.dirty_generation_low_watermark;
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_DIRTY_HIGH_SLOT] =
                control.dirty_generation_high_watermark;
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_FLUSH_DEBT_SLOT] =
                control.captured_flush_debt_pages;
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_SWEEP_GENERATION_SLOT] =
                control.sweep_generation_seen;
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_QUEUE_REBUILD_SLOT] =
                control.queue_rebuild_required ? 1 : 0;
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_SHUTDOWN_INTENT_SLOT] =
                static_cast<uint64_t>(control.shutdown_intent);
            state_page->reserved[SYSTEM_STATE_CHECKPOINT_FAILURE_REASON_SLOT] =
                static_cast<uint64_t>(control.checkpoint_failure_reason);
        }

        struct WritebackIncidentControlState
        {
            uint64_t version = SYSTEM_STATE_WRITEBACK_INCIDENT_VERSION;
            bool incident_open = false;
            uint64_t filespace_id = 0;
            WritebackQueueKind queue_kind = WritebackQueueKind::UNKNOWN;
            WritebackPolicyDomain policy_domain = WritebackPolicyDomain::UNKNOWN;
            uint64_t page_class = 0;
            uint64_t dirty_generation = 0;
            uint64_t first_seen_time = 0;
            uint64_t last_retry_time = 0;
            uint64_t retry_count = 0;
            WritebackFailureClass failure_class = WritebackFailureClass::NONE;
            WritebackDegradedState degraded_state = WritebackDegradedState::NORMAL;
            Status last_error_status = Status::OK;
        };

        void loadWritebackIncidentControlState(const BootstrapSystemStatePage &state_page,
                                               WritebackIncidentControlState *control_out)
        {
            if (control_out == nullptr)
            {
                return;
            }

            WritebackIncidentControlState control{};
            control.version =
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_VERSION_SLOT];
            if (control.version == 0)
            {
                *control_out = control;
                return;
            }

            control.incident_open =
                (state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_FLAGS_SLOT] &
                 SYSTEM_STATE_WRITEBACK_INCIDENT_FLAG_OPEN) != 0;
            control.filespace_id =
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_FILESPACE_SLOT];
            control.queue_kind = static_cast<WritebackQueueKind>(
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_QUEUE_KIND_SLOT]);
            control.policy_domain = static_cast<WritebackPolicyDomain>(
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_POLICY_DOMAIN_SLOT]);
            control.page_class =
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_PAGE_CLASS_SLOT];
            control.dirty_generation =
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_DIRTY_GENERATION_SLOT];
            control.first_seen_time =
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_FIRST_SEEN_SLOT];
            control.last_retry_time =
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_LAST_RETRY_SLOT];
            control.retry_count =
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_RETRY_COUNT_SLOT];
            control.failure_class = static_cast<WritebackFailureClass>(
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_FAILURE_CLASS_SLOT]);
            control.degraded_state = static_cast<WritebackDegradedState>(
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_DEGRADED_STATE_SLOT]);
            control.last_error_status = static_cast<Status>(
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_LAST_ERROR_STATUS_SLOT]);
            *control_out = control;
        }

        void storeWritebackIncidentControlState(BootstrapSystemStatePage *state_page,
                                                const WritebackIncidentControlState &control)
        {
            if (state_page == nullptr)
            {
                return;
            }

            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_VERSION_SLOT] =
                control.version;
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_FLAGS_SLOT] =
                control.incident_open ? SYSTEM_STATE_WRITEBACK_INCIDENT_FLAG_OPEN : 0;
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_FILESPACE_SLOT] =
                control.filespace_id;
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_QUEUE_KIND_SLOT] =
                static_cast<uint64_t>(control.queue_kind);
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_POLICY_DOMAIN_SLOT] =
                static_cast<uint64_t>(control.policy_domain);
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_PAGE_CLASS_SLOT] =
                control.page_class;
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_DIRTY_GENERATION_SLOT] =
                control.dirty_generation;
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_FIRST_SEEN_SLOT] =
                control.first_seen_time;
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_LAST_RETRY_SLOT] =
                control.last_retry_time;
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_RETRY_COUNT_SLOT] =
                control.retry_count;
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_FAILURE_CLASS_SLOT] =
                static_cast<uint64_t>(control.failure_class);
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_DEGRADED_STATE_SLOT] =
                static_cast<uint64_t>(control.degraded_state);
            state_page->reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_LAST_ERROR_STATUS_SLOT] =
                static_cast<uint64_t>(control.last_error_status);
        }

        auto startupStateHasTxnNormalizationWork(
            const Database::StartupReconciliationState &state) -> bool
        {
            return state.startup_repair ||
                   state.tip_active_to_aborted > 0 ||
                   state.tip_active_to_prepared > 0 ||
                   state.stale_prepared_records_removed > 0 ||
                   state.clog_states_synchronized > 0;
        }

        auto startupOutcomeIsFatal(Database::StartupReconciliationOutcome outcome) -> bool
        {
            switch (outcome)
            {
                case Database::StartupReconciliationOutcome::FAILED_PAGE_SCAN:
                case Database::StartupReconciliationOutcome::FAILED_TXN_RECONCILIATION:
                case Database::StartupReconciliationOutcome::FAILED_CORRUPTION_POLICY:
                    return true;
                case Database::StartupReconciliationOutcome::NOT_RUN:
                case Database::StartupReconciliationOutcome::CLEAN:
                case Database::StartupReconciliationOutcome::CLEAN_WITH_FINDINGS:
                case Database::StartupReconciliationOutcome::RECOVERY_WITH_FINDINGS:
                    return false;
            }
            return false;
        }

        auto escalateStartupServiceState(Database::StartupServiceState current,
                                         Database::StartupServiceState candidate)
            -> Database::StartupServiceState
        {
            return static_cast<uint8_t>(candidate) > static_cast<uint8_t>(current)
                ? candidate
                : current;
        }

        auto startupServiceStateFromWriteback(WritebackDegradedState degraded_state)
            -> Database::StartupServiceState
        {
            switch (degraded_state)
            {
                case WritebackDegradedState::NORMAL:
                    return Database::StartupServiceState::DEGRADED_READ_WRITE;
                case WritebackDegradedState::DEGRADED_READ_WRITE:
                    return Database::StartupServiceState::DEGRADED_READ_WRITE;
                case WritebackDegradedState::WRITE_FENCED:
                    return Database::StartupServiceState::WRITE_FENCED;
                case WritebackDegradedState::FATAL:
                    return Database::StartupServiceState::FATAL;
            }
            return Database::StartupServiceState::DEGRADED_READ_WRITE;
        }

        auto finalizeStartupRecoveryClassification(
            Database::StartupReconciliationState *state,
            bool writeback_incident_open,
            WritebackDegradedState writeback_degraded_state) -> void
        {
            if (state == nullptr)
            {
                return;
            }

            const bool has_tx_findings = startupStateHasTxnNormalizationWork(*state);
            const bool has_checkpoint_queue_rebuild =
                state->checkpoint_queue_rebuild_required ||
                state->checkpoint_queue_rebuild_pages > 0;
            const bool has_repairable_page_damage =
                state->has_page_scan_findings ||
                state->has_corrupt_pages ||
                state->relinkable_chain_pages > 0 ||
                state->cleanup_blocked_chain_pages > 0 ||
                state->quarantinable_chain_pages > 0 ||
                state->corruption_class != Database::StartupCorruptionClass::NONE;
            const bool has_fatal_corruption =
                state->corruption_class == Database::StartupCorruptionClass::STARTUP_REFUSAL ||
                state->quarantine_action == Database::StartupQuarantineAction::REFUSE_OPEN ||
                startupOutcomeIsFatal(state->outcome);

            if (has_fatal_corruption)
            {
                state->classification =
                    Database::StartupRecoveryClassification::CATALOG_OR_CONTROL_DAMAGE_FATAL;
                state->service_state = Database::StartupServiceState::FATAL;
                return;
            }

            if (writeback_incident_open)
            {
                state->classification =
                    Database::StartupRecoveryClassification::WRITEBACK_FAILURE_RESUME;
                state->service_state = startupServiceStateFromWriteback(writeback_degraded_state);
            }
            else if (has_repairable_page_damage)
            {
                state->classification =
                    Database::StartupRecoveryClassification::REPAIRABLE_PAGE_DAMAGE;
                state->service_state = Database::StartupServiceState::DEGRADED_READ_WRITE;
            }
            else if (!state->clean_shutdown_marker || has_tx_findings ||
                     has_checkpoint_queue_rebuild)
            {
                state->classification =
                    Database::StartupRecoveryClassification::
                        DIRTY_SHUTDOWN_NORMALIZATION_REQUIRED;
                state->service_state = Database::StartupServiceState::NORMAL;
            }
            else
            {
                state->classification =
                    Database::StartupRecoveryClassification::CLEAN_SHUTDOWN_FAST_PATH;
                state->service_state = Database::StartupServiceState::NORMAL;
            }

            if (state->has_page_scan_findings || state->has_corrupt_pages ||
                state->corruption_class == Database::StartupCorruptionClass::REPAIR_REQUIRED ||
                state->corruption_class == Database::StartupCorruptionClass::RELINKABLE_ONLY)
            {
                state->service_state = escalateStartupServiceState(
                    state->service_state,
                    Database::StartupServiceState::DEGRADED_READ_WRITE);
            }
            if (state->quarantine_active ||
                state->quarantine_action == Database::StartupQuarantineAction::READ_ONLY)
            {
                state->service_state = escalateStartupServiceState(
                    state->service_state,
                    Database::StartupServiceState::WRITE_FENCED);
            }
        }

        template <typename Mutator>
        auto mutateAndFlushSystemStatePage(Database *db,
                                           BufferPool *buffer_pool,
                                           uint32_t page_size,
                                           ErrorContext *ctx,
                                           Mutator &&mutator) -> Status
        {
            if (db == nullptr || buffer_pool == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "System state mutation requires database and buffer pool");
                return Status::INVALID_ARGUMENT;
            }

            void *page_buffer = nullptr;
            Status status =
                buffer_pool->pinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, &page_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *state_page = static_cast<BootstrapSystemStatePage *>(page_buffer);
            if (state_page->page_header.page_type != PAGE_TYPE_SYSTEM_STATE)
            {
                buffer_pool->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Invalid system state bootstrap page");
                return Status::PAGE_CORRUPT;
            }
            if (!validatePageChecksum(reinterpret_cast<uint8_t *>(state_page), page_size))
            {
                buffer_pool->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH,
                                  "System state page checksum validation failed");
                return Status::CHECKSUM_MISMATCH;
            }

            mutator(state_page);
            state_page->page_header.checksum =
                calculatePageChecksum(reinterpret_cast<uint8_t *>(state_page), page_size);
            buffer_pool->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, true, ctx);

            status = buffer_pool->flushPage(BOOTSTRAP_PAGE_SYSTEM_STATE, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            WritebackAttribution attribution{};
            attribution.queue_kind = WritebackQueueKind::METADATA_PRIORITY;
            attribution.policy_domain = WritebackPolicyDomain::SYSTEM_STATE;
            attribution.page_class = PAGE_TYPE_SYSTEM_STATE;
            return db->sync(ctx, attribution);
        }

        auto classifyWritebackFailure(Status status,
                                      int err,
                                      bool sync_failure) -> WritebackFailureClass
        {
            if (status == Status::DISK_FULL || err == ENOSPC)
            {
                return WritebackFailureClass::DISK_FULL;
            }
            if (err == EROFS || err == ENODEV || err == EIO)
            {
                return WritebackFailureClass::FILESYSTEM_OFFLINE;
            }
            return sync_failure
                ? WritebackFailureClass::RETRYABLE_FSYNC_IO
                : WritebackFailureClass::RETRYABLE_WRITEBACK_IO;
        }

        auto finalizeWritebackAttribution(const void *buffer,
                                          WritebackAttribution attribution,
                                          uint64_t default_filespace_id) -> WritebackAttribution
        {
            attribution.filespace_id =
                attribution.filespace_id == 0 ? default_filespace_id : attribution.filespace_id;
            if (buffer == nullptr)
            {
                return attribution;
            }

            const auto *header = reinterpret_cast<const PageHeader *>(buffer);
            if (header->magic != K_MAGIC_SBRD)
            {
                return attribution;
            }
            if (attribution.page_class == 0)
            {
                attribution.page_class = header->page_type;
            }
            if (attribution.dirty_generation == 0)
            {
                attribution.dirty_generation = header->generation;
            }
            return attribution;
        }

        template <typename Mutator>
        auto mutateAndPersistSystemStatePageDirect(Database *db,
                                                   bool sync_after_write,
                                                   ErrorContext *ctx,
                                                   Mutator &&mutator) -> Status
        {
            if (db == nullptr || db->fd() < 0 || db->page_size() == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "System state direct persistence requires an open database");
                return Status::INVALID_ARGUMENT;
            }

            std::vector<uint8_t> buffer(db->page_size(), 0);
            const off_t offset =
                static_cast<off_t>(BOOTSTRAP_PAGE_SYSTEM_STATE) *
                static_cast<off_t>(db->page_size());
            size_t bytes_read = 0;
            errno = 0;
            if (!preadFully(db->fd(), buffer.data(), db->page_size(), offset, &bytes_read))
            {
                if (errno != 0)
                {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "Failed to load system state page for direct persistence: %s",
                             std::strerror(errno));
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                                      "Short read loading system state page for direct persistence");
                }
                return Status::IO_ERROR;
            }

            auto *state_page =
                reinterpret_cast<BootstrapSystemStatePage *>(buffer.data());
            if (state_page->page_header.page_type != PAGE_TYPE_SYSTEM_STATE)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Invalid system state page during direct persistence");
                return Status::PAGE_CORRUPT;
            }
            if (!validatePageChecksum(buffer.data(), db->page_size()))
            {
                SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH,
                                  "System state page checksum validation failed");
                return Status::CHECKSUM_MISMATCH;
            }

            mutator(state_page);
            preparePageForWrite(buffer.data(),
                                db->page_size(),
                                BOOTSTRAP_PAGE_SYSTEM_STATE);

            size_t bytes_written = 0;
            errno = 0;
            if (!pwriteFully(db->fd(), buffer.data(), db->page_size(), offset, &bytes_written))
            {
                if (errno != 0)
                {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "Failed to persist system state page directly: %s",
                             std::strerror(errno));
                    SET_ERROR_CONTEXT(ctx, classifyWritebackFailure(Status::IO_ERROR,
                                                                    errno,
                                                                    false) ==
                                              WritebackFailureClass::DISK_FULL
                                          ? Status::DISK_FULL
                                          : Status::IO_ERROR,
                                      msg);
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                                      "Short write persisting system state page directly");
                }
                return (errno == ENOSPC) ? Status::DISK_FULL : Status::IO_ERROR;
            }

            if (sync_after_write && platform::syncFd(db->fd()) != 0)
            {
                const int sync_errno = errno;
                if (sync_errno == ENOSPC)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DISK_FULL,
                                      "Failed to sync persisted system state page: disk full");
                    return Status::DISK_FULL;
                }
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                                  "Failed to sync persisted system state page");
                return Status::IO_ERROR;
            }

            return Status::OK;
        }

        auto classifyStartupCorruptionPolicy(Database::StartupReconciliationState *state) -> void
        {
            if (state == nullptr)
            {
                return;
            }

            state->corruption_class = Database::StartupCorruptionClass::NONE;
            state->quarantine_action = Database::StartupQuarantineAction::NONE;
            state->quarantine_active = false;
            state->repair_plan_mask = Database::STARTUP_REPAIR_PLAN_NONE;

            if (state->relinkable_chain_pages > 0)
            {
                state->corruption_class = Database::StartupCorruptionClass::RELINKABLE_ONLY;
                state->repair_plan_mask |=
                    Database::STARTUP_REPAIR_PLAN_DIAGNOSTIC_SCAN |
                    Database::STARTUP_REPAIR_PLAN_RELINKABLE_CHAIN_REPAIR;
            }

            if (state->cleanup_blocked_chain_pages > 0)
            {
                state->corruption_class = Database::StartupCorruptionClass::REPAIR_REQUIRED;
                state->repair_plan_mask |=
                    Database::STARTUP_REPAIR_PLAN_DIAGNOSTIC_SCAN |
                    Database::STARTUP_REPAIR_PLAN_CHAIN_REWRITE_REVIEW;
            }

            if (state->quarantinable_chain_pages > 0)
            {
                state->corruption_class = Database::StartupCorruptionClass::QUARANTINE_REQUIRED;
                state->quarantine_action = Database::StartupQuarantineAction::READ_ONLY;
                state->quarantine_active = true;
                state->repair_plan_mask |=
                    Database::STARTUP_REPAIR_PLAN_DIAGNOSTIC_SCAN |
                    Database::STARTUP_REPAIR_PLAN_CHAIN_REWRITE_REVIEW |
                    Database::STARTUP_REPAIR_PLAN_READ_ONLY_QUARANTINE;
            }

            if (state->has_corrupt_pages)
            {
                state->corruption_class = Database::StartupCorruptionClass::QUARANTINE_REQUIRED;
                state->quarantine_action = Database::StartupQuarantineAction::READ_ONLY;
                state->quarantine_active = true;
                state->repair_plan_mask |=
                    Database::STARTUP_REPAIR_PLAN_DIAGNOSTIC_SCAN |
                    Database::STARTUP_REPAIR_PLAN_REBUILD_FSM |
                    Database::STARTUP_REPAIR_PLAN_READ_ONLY_QUARANTINE;
            }

            if (state->unrecoverable_chain_pages > 0)
            {
                state->corruption_class = Database::StartupCorruptionClass::STARTUP_REFUSAL;
                state->quarantine_action = Database::StartupQuarantineAction::REFUSE_OPEN;
                state->quarantine_active = false;
                state->repair_plan_mask |=
                    Database::STARTUP_REPAIR_PLAN_DIAGNOSTIC_SCAN |
                    Database::STARTUP_REPAIR_PLAN_RESTORE_FROM_BACKUP;
            }

            if (state->corruption_class == Database::StartupCorruptionClass::STARTUP_REFUSAL)
            {
                state->repair_plan_mask &=
                    ~static_cast<uint64_t>(Database::STARTUP_REPAIR_PLAN_READ_ONLY_QUARANTINE);
            }
        }

        auto buildStartupCorruptionPolicyMessage(const Database::StartupReconciliationState &state)
            -> std::string
        {
            std::ostringstream oss;
            oss << "STARTUP_CORRUPTION_POLICY[class="
                << startupCorruptionClassName(state.corruption_class)
                << ",action="
                << startupQuarantineActionName(state.quarantine_action)
                << ",recovery_class="
                << startupRecoveryClassificationName(state.classification)
                << ",service_state="
                << startupServiceStateName(state.service_state)
                << ",repair_plan="
                << formatStartupRepairPlanMask(state.repair_plan_mask)
                << ",corrupt_pages=" << state.has_corrupt_pages
                << ",relinkable=" << state.relinkable_chain_pages
                << ",cleanup_blocked=" << state.cleanup_blocked_chain_pages
                << ",quarantinable=" << state.quarantinable_chain_pages
                << ",unrecoverable=" << state.unrecoverable_chain_pages
                << ",checkpoint_queue_rebuild=" << state.checkpoint_queue_rebuild_required
                << ",checkpoint_queue_pages=" << state.checkpoint_queue_rebuild_pages
                << ",checkpoint_dirty_low=" << state.checkpoint_dirty_generation_low_watermark
                << ",checkpoint_dirty_high=" << state.checkpoint_dirty_generation_high_watermark
                << "]";
            return oss.str();
        }

        bool parseUnsignedIntegerStrict(const std::string &value, uint64_t *out_value)
        {
            if (out_value == nullptr)
            {
                return false;
            }

            const std::string trimmed = trimAscii(value);
            if (trimmed.empty())
            {
                return false;
            }

            size_t consumed = 0;
            try
            {
                const auto parsed = std::stoull(trimmed, &consumed, 10);
                if (consumed != trimmed.size())
                {
                    return false;
                }
                *out_value = parsed;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool parseDoubleStrict(const std::string &value, double *out_value)
        {
            if (out_value == nullptr)
            {
                return false;
            }

            const std::string trimmed = trimAscii(value);
            if (trimmed.empty())
            {
                return false;
            }

            size_t consumed = 0;
            try
            {
                const auto parsed = std::stod(trimmed, &consumed);
                if (consumed != trimmed.size())
                {
                    return false;
                }
                *out_value = parsed;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool parseBinarySizeBytes(const std::string &value, uint64_t *out_bytes)
        {
            if (out_bytes == nullptr)
            {
                return false;
            }

            const std::string trimmed = trimAscii(value);
            if (trimmed.empty())
            {
                return false;
            }

            size_t suffix_start = 0;
            while (suffix_start < trimmed.size() &&
                   std::isdigit(static_cast<unsigned char>(trimmed[suffix_start])) != 0)
            {
                ++suffix_start;
            }
            if (suffix_start == 0 || suffix_start == trimmed.size())
            {
                return false;
            }

            uint64_t magnitude = 0;
            if (!parseUnsignedIntegerStrict(trimmed.substr(0, suffix_start), &magnitude))
            {
                return false;
            }

            const std::string suffix = toLowerAscii(trimAscii(trimmed.substr(suffix_start)));
            uint64_t multiplier = 0;
            if (suffix == "b")
            {
                multiplier = 1;
            }
            else if (suffix == "k" || suffix == "kb")
            {
                multiplier = 1024ULL;
            }
            else if (suffix == "m" || suffix == "mb")
            {
                multiplier = 1024ULL * 1024ULL;
            }
            else if (suffix == "g" || suffix == "gb")
            {
                multiplier = 1024ULL * 1024ULL * 1024ULL;
            }
            else if (suffix == "t" || suffix == "tb")
            {
                multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
            }
            else
            {
                return false;
            }

            if (magnitude > (std::numeric_limits<uint64_t>::max() / multiplier))
            {
                return false;
            }

            *out_bytes = magnitude * multiplier;
            return true;
        }

        bool preadFully(int fd, void* buffer, size_t size, off_t offset,
                        size_t* transferred_out)
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
                         size_t* transferred_out)
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

        struct ConfigLookupKey
        {
            const char *section = nullptr;
            const char *key = nullptr;
        };

        auto lookupConfigString(Config &settings,
                                std::initializer_list<ConfigLookupKey> lookup_keys)
            -> std::optional<std::string>
        {
            for (const auto &lookup_key : lookup_keys)
            {
                if (lookup_key.section == nullptr || lookup_key.key == nullptr)
                {
                    continue;
                }
                if (settings.hasKey(lookup_key.section, lookup_key.key))
                {
                    return settings.getString(lookup_key.section, lookup_key.key, "");
                }
            }
            return std::nullopt;
        }

        BufferPool::PoolLayout parseBufferPoolLayout(const std::string &value, bool *recognized)
        {
            std::string normalized = toLowerAscii(value);
            if (normalized.empty() || normalized == "default" || normalized == "segmented")
            {
                if (recognized)
                {
                    *recognized = true;
                }
                return BufferPool::PoolLayout::Segmented;
            }
            if (normalized == "single")
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

        BufferPool::BufferProfile parseBufferProfile(const std::string &value, bool *recognized)
        {
            const std::string normalized = toLowerAscii(value);
            if (normalized.empty() || normalized == "mixed" || normalized == "default")
            {
                if (recognized)
                {
                    *recognized = true;
                }
                return BufferPool::BufferProfile::Mixed;
            }
            if (normalized == "dev")
            {
                if (recognized)
                {
                    *recognized = true;
                }
                return BufferPool::BufferProfile::Dev;
            }
            if (normalized == "oltp")
            {
                if (recognized)
                {
                    *recognized = true;
                }
                return BufferPool::BufferProfile::Oltp;
            }
            if (normalized == "analytics")
            {
                if (recognized)
                {
                    *recognized = true;
                }
                return BufferPool::BufferProfile::Analytics;
            }
            if (normalized == "maintenance_recovery" || normalized == "maintenance-recovery")
            {
                if (recognized)
                {
                    *recognized = true;
                }
                return BufferPool::BufferProfile::MaintenanceRecovery;
            }
            if (recognized)
            {
                *recognized = false;
            }
            return BufferPool::BufferProfile::Mixed;
        }

        Status loadBufferPoolConfig(uint32_t database_page_size,
                                    BufferPool::Config *config_out,
                                    ErrorContext *ctx)
        {
            if (config_out == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Buffer pool config output cannot be null");
                return Status::INVALID_ARGUMENT;
            }

            BufferPool::Config config;
            Config &settings = Config::getInstance();

            // AUDIT CONTRACT (MMW-002):
            // - Canonical `storage.buffer.*` keys are loaded here; legacy `memory.buffer_pool_*`
            //   keys remain compatibility aliases only.
            // - `segmented` means logical policy domains and budget accounting over the current
            //   shared frame array. It does not yet claim segmented eviction behavior.
            // - This loader validates domain budget legality now so later residency tickets can
            //   build on one canonical foundation instead of retrofitting config meaning.

            if (const auto profile_value =
                    lookupConfigString(settings, {{"storage.buffer", "profile"}});
                profile_value.has_value())
            {
                bool recognized = false;
                const auto parsed_profile =
                    parseBufferProfile(profile_value.value(), &recognized);
                if (!recognized)
                {
                    const std::string message =
                        "Unknown storage.buffer.profile '" + profile_value.value() + "'";
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
                    return Status::INVALID_ARGUMENT;
                }
                config.applyProfileDefaults(parsed_profile);
            }

            uint64_t pool_pages = 0;
            if (const auto pool_size_mb_value =
                    lookupConfigString(settings, {{"storage.buffer", "pool_size_mb"}});
                pool_size_mb_value.has_value())
            {
                uint64_t pool_size_mb = 0;
                if (!parseUnsignedIntegerStrict(pool_size_mb_value.value(), &pool_size_mb) ||
                    pool_size_mb == 0)
                {
                    const std::string message =
                        "Invalid storage.buffer.pool_size_mb '" + pool_size_mb_value.value() +
                        "': use a positive integer megabyte count";
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
                    return Status::INVALID_ARGUMENT;
                }
                const uint64_t pool_bytes = pool_size_mb * 1024ULL * 1024ULL;
                pool_pages =
                    std::max<uint64_t>(1, (pool_bytes + database_page_size - 1) / database_page_size);
            }
            else
            {
                const std::string pool_size_value = settings.getString(
                    "memory", "buffer_pool_size", std::to_string(config.pool_size));
                if (!parseUnsignedIntegerStrict(pool_size_value, &pool_pages))
                {
                    uint64_t pool_bytes = 0;
                    if (!parseBinarySizeBytes(pool_size_value, &pool_bytes))
                    {
                        const std::string message =
                            "Invalid buffer_pool_size '" + pool_size_value +
                            "': use a page count or a size suffix such as 128MB";
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
                        return Status::INVALID_ARGUMENT;
                    }
                    pool_pages = std::max<uint64_t>(
                        1, (pool_bytes + database_page_size - 1) / database_page_size);
                }
            }
            if (pool_pages == 0 || pool_pages > std::numeric_limits<uint32_t>::max())
            {
                const std::string message =
                    "buffer_pool_size resolves to an out-of-range page count";
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
                return Status::INVALID_ARGUMENT;
            }

            config.pool_size = static_cast<uint32_t>(pool_pages);
            config.page_size = database_page_size;
            config.recomputeDomainFrames();

            if (settings.hasKey("memory", "buffer_pool_page_size"))
            {
                const std::string configured_page_size =
                    settings.getString("memory", "buffer_pool_page_size", "");
                uint64_t page_size_value = 0;
                if (!parseUnsignedIntegerStrict(configured_page_size, &page_size_value))
                {
                    const std::string message =
                        "Invalid buffer_pool_page_size '" + configured_page_size +
                        "': page size overrides must be numeric";
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
                    return Status::INVALID_ARGUMENT;
                }
                if (page_size_value != database_page_size)
                {
                    const std::string message =
                        "buffer_pool_page_size is fixed by the database page size (" +
                        std::to_string(database_page_size) + ")";
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
                    return Status::INVALID_ARGUMENT;
                }
            }

            const std::string layout_value = lookupConfigString(
                                                 settings,
                                                 {{"storage.buffer", "layout"},
                                                  {"memory", "buffer_pool_layout"}})
                                                 .value_or("segmented");
            bool layout_recognized = false;
            config.layout = parseBufferPoolLayout(layout_value, &layout_recognized);
            if (!layout_recognized)
            {
                const std::string message =
                    "Unknown buffer_pool_layout '" + layout_value + "'";
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
                return Status::INVALID_ARGUMENT;
            }
            if (config.layout == BufferPool::PoolLayout::HotCold ||
                config.layout == BufferPool::PoolLayout::Tablespace)
            {
                const std::string message =
                    "buffer_pool_layout '" + layout_value +
                    "' is not supported; canonical 'segmented' and legacy 'single' are implemented";
                SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, message.c_str());
                return Status::NOT_IMPLEMENTED;
            }

            if (settings.hasKey("storage.buffer.writeback", "enabled"))
            {
                config.enable_background_writer = settings.getBool(
                    "storage.buffer.writeback", "enabled", config.enable_background_writer);
            }
            else
            {
                config.enable_background_writer = settings.getBool(
                    "memory", "buffer_pool_bgwriter_enabled", config.enable_background_writer);
            }

            const uint64_t bgwriter_delay_ms = settings.getUInt(
                "memory", "buffer_pool_bgwriter_delay_ms", config.bgwriter_delay_ms);
            const uint64_t bgwriter_max_pages =
                lookupConfigString(settings,
                                   {{"storage.buffer.writeback", "batch_pages"},
                                    {"memory", "buffer_pool_bgwriter_max_pages"}})
                    .has_value()
                ? settings.getUInt("storage.buffer.writeback", "batch_pages",
                                   settings.getUInt("memory",
                                                    "buffer_pool_bgwriter_max_pages",
                                                    config.bgwriter_max_pages))
                : config.bgwriter_max_pages;
            if (bgwriter_delay_ms > std::numeric_limits<uint32_t>::max() ||
                bgwriter_max_pages > std::numeric_limits<uint32_t>::max())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "buffer_pool_bgwriter settings exceed supported range");
                return Status::INVALID_ARGUMENT;
            }
            config.bgwriter_delay_ms = static_cast<uint32_t>(bgwriter_delay_ms);
            config.bgwriter_max_pages = static_cast<uint32_t>(bgwriter_max_pages);

            if (const auto low_pct = lookupConfigString(
                    settings,
                    {{"storage.buffer.writeback", "low_dirty_pct"},
                     {"memory", "buffer_pool_dirty_ratio_low"}});
                low_pct.has_value())
            {
                if (settings.hasKey("storage.buffer.writeback", "low_dirty_pct"))
                {
                    double canonical_pct = 0.0;
                    if (!parseDoubleStrict(low_pct.value(), &canonical_pct) ||
                        canonical_pct < 0.0 || canonical_pct > 100.0)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "storage.buffer.writeback.low_dirty_pct must be between 0 and 100");
                        return Status::INVALID_ARGUMENT;
                    }
                    config.dirty_ratio_low = canonical_pct / 100.0;
                }
                else
                {
                    config.dirty_ratio_low = settings.getDouble(
                        "memory", "buffer_pool_dirty_ratio_low", config.dirty_ratio_low);
                }
            }
            if (const auto high_pct = lookupConfigString(
                    settings,
                    {{"storage.buffer.writeback", "high_dirty_pct"},
                     {"memory", "buffer_pool_dirty_ratio_high"}});
                high_pct.has_value())
            {
                if (settings.hasKey("storage.buffer.writeback", "high_dirty_pct"))
                {
                    double canonical_pct = 0.0;
                    if (!parseDoubleStrict(high_pct.value(), &canonical_pct) ||
                        canonical_pct < 0.0 || canonical_pct > 100.0)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "storage.buffer.writeback.high_dirty_pct must be between 0 and 100");
                        return Status::INVALID_ARGUMENT;
                    }
                    config.dirty_ratio_high = canonical_pct / 100.0;
                }
                else
                {
                    config.dirty_ratio_high = settings.getDouble(
                        "memory", "buffer_pool_dirty_ratio_high", config.dirty_ratio_high);
                }
            }
            if (const auto checkpoint_pct = lookupConfigString(
                    settings,
                    {{"storage.buffer.writeback", "checkpoint_target_pct"},
                     {"memory", "buffer_pool_dirty_ratio_checkpoint"}});
                checkpoint_pct.has_value())
            {
                if (settings.hasKey("storage.buffer.writeback", "checkpoint_target_pct"))
                {
                    double canonical_pct = 0.0;
                    if (!parseDoubleStrict(checkpoint_pct.value(), &canonical_pct) ||
                        canonical_pct < 0.0 || canonical_pct > 100.0)
                    {
                        SET_ERROR_CONTEXT(
                            ctx,
                            Status::INVALID_ARGUMENT,
                            "storage.buffer.writeback.checkpoint_target_pct must be between 0 and 100");
                        return Status::INVALID_ARGUMENT;
                    }
                    config.dirty_ratio_checkpoint = canonical_pct / 100.0;
                }
                else
                {
                    config.dirty_ratio_checkpoint = settings.getDouble(
                        "memory", "buffer_pool_dirty_ratio_checkpoint",
                        config.dirty_ratio_checkpoint);
                }
            }

            auto apply_domain_override = [&](BufferPool::PolicyDomain domain,
                                             const char *domain_name,
                                             const char *key_name,
                                             double BufferPool::DomainBudgetConfig::*member)
                -> Status
            {
                const std::string section =
                    std::string("storage.buffer.domain.") + domain_name;
                const auto configured_value =
                    lookupConfigString(settings, {{section.c_str(), key_name}});
                if (!configured_value.has_value())
                {
                    return Status::OK;
                }

                double pct = 0.0;
                if (!parseDoubleStrict(configured_value.value(), &pct) ||
                    pct < 0.0 || pct > 100.0)
                {
                    const std::string message =
                        "storage.buffer.domain." + std::string(domain_name) + "." + key_name +
                        " must be between 0 and 100";
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
                    return Status::INVALID_ARGUMENT;
                }

                config.domainBudget(domain).*member = pct;
                return Status::OK;
            };

            Status status = Status::OK;
            status = apply_domain_override(BufferPool::PolicyDomain::CriticalSystem,
                                           "critical_system",
                                           "min_pct",
                                           &BufferPool::DomainBudgetConfig::min_pct);
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_domain_override(BufferPool::PolicyDomain::HotOltp,
                                           "hot_oltp",
                                           "target_pct",
                                           &BufferPool::DomainBudgetConfig::target_pct);
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_domain_override(BufferPool::PolicyDomain::ReadMostly,
                                           "read_mostly",
                                           "target_pct",
                                           &BufferPool::DomainBudgetConfig::target_pct);
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_domain_override(BufferPool::PolicyDomain::ScanBulkRing,
                                           "scan_bulk_ring",
                                           "max_pct",
                                           &BufferPool::DomainBudgetConfig::max_pct);
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_domain_override(BufferPool::PolicyDomain::VersionUndo,
                                           "version_undo",
                                           "min_pct",
                                           &BufferPool::DomainBudgetConfig::min_pct);
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_domain_override(BufferPool::PolicyDomain::TemporaryWork,
                                           "temporary_work",
                                           "max_pct",
                                           &BufferPool::DomainBudgetConfig::max_pct);
            if (status != Status::OK)
            {
                return status;
            }

            if (const auto protected_pct =
                    lookupConfigString(settings, {{"storage.buffer.replacement", "protected_pct"}});
                protected_pct.has_value())
            {
                uint64_t parsed = 0;
                if (!parseUnsignedIntegerStrict(protected_pct.value(), &parsed) || parsed > 100)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "storage.buffer.replacement.protected_pct must be between 0 and 100");
                    return Status::INVALID_ARGUMENT;
                }
                config.replacement_protected_pct = static_cast<uint32_t>(parsed);
            }

            if (const auto ghost_pct = lookupConfigString(
                    settings, {{"storage.buffer.replacement", "ghost_history_pct"}});
                ghost_pct.has_value())
            {
                uint64_t parsed = 0;
                if (!parseUnsignedIntegerStrict(ghost_pct.value(), &parsed) || parsed > 100)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "storage.buffer.replacement.ghost_history_pct must be between 0 and 100");
                    return Status::INVALID_ARGUMENT;
                }
                config.replacement_ghost_history_pct = static_cast<uint32_t>(parsed);
            }

            if (const auto second_touch = lookupConfigString(
                    settings, {{"storage.buffer.admission", "second_touch_generations"}});
                second_touch.has_value())
            {
                uint64_t parsed = 0;
                if (!parseUnsignedIntegerStrict(second_touch.value(), &parsed) || parsed == 0 ||
                    parsed > std::numeric_limits<uint32_t>::max())
                {
                    SET_ERROR_CONTEXT(
                        ctx,
                        Status::INVALID_ARGUMENT,
                        "storage.buffer.admission.second_touch_generations must be a positive integer");
                    return Status::INVALID_ARGUMENT;
                }
                config.admission_second_touch_generations = static_cast<uint32_t>(parsed);
            }

            if (settings.hasKey("storage.buffer.admission", "direct_protect_roots"))
            {
                config.admission_direct_protect_roots = settings.getBool(
                    "storage.buffer.admission",
                    "direct_protect_roots",
                    config.admission_direct_protect_roots);
            }

            if (settings.hasKey("storage.buffer.prefetch", "enabled"))
            {
                config.prefetch_enabled = settings.getBool("storage.buffer.prefetch",
                                                           "enabled",
                                                           config.prefetch_enabled);
            }

            auto apply_prefetch_uint = [&](const char *key_name,
                                           uint32_t *target,
                                           bool require_positive,
                                           const char *message_suffix) -> Status
            {
                const auto configured_value =
                    lookupConfigString(settings, {{"storage.buffer.prefetch", key_name}});
                if (!configured_value.has_value())
                {
                    return Status::OK;
                }

                uint64_t parsed = 0;
                if (!parseUnsignedIntegerStrict(configured_value.value(), &parsed) ||
                    parsed > std::numeric_limits<uint32_t>::max() ||
                    (require_positive && parsed == 0))
                {
                    std::string message = "storage.buffer.prefetch.";
                    message += key_name;
                    message += message_suffix;
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
                    return Status::INVALID_ARGUMENT;
                }

                *target = static_cast<uint32_t>(parsed);
                return Status::OK;
            };

            status = apply_prefetch_uint("workers",
                                         &config.prefetch_workers,
                                         false,
                                         " must be a non-negative integer");
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_prefetch_uint("scan_window_pages",
                                         &config.prefetch_scan_window_pages,
                                         true,
                                         " must be a positive integer");
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_prefetch_uint("index_window_pages",
                                         &config.prefetch_index_window_pages,
                                         true,
                                         " must be a positive integer");
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_prefetch_uint("chain_window_pages",
                                         &config.prefetch_chain_window_pages,
                                         true,
                                         " must be a positive integer");
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_prefetch_uint("max_debt_pages",
                                         &config.prefetch_max_debt_pages,
                                         false,
                                         " must be a non-negative integer");
            if (status != Status::OK)
            {
                return status;
            }

            auto apply_prefetch_pct = [&](const char *section,
                                          const char *key_name,
                                          uint32_t *target) -> Status
            {
                const auto configured_value =
                    lookupConfigString(settings, {{section, key_name}});
                if (!configured_value.has_value())
                {
                    return Status::OK;
                }

                uint64_t parsed = 0;
                const std::string full_name = std::string(section) + "." + key_name;
                if (!parseUnsignedIntegerStrict(configured_value.value(), &parsed) ||
                    parsed > 100)
                {
                    const std::string message = full_name + " must be between 0 and 100";
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
                    return Status::INVALID_ARGUMENT;
                }

                *target = static_cast<uint32_t>(parsed);
                return Status::OK;
            };

            status = apply_prefetch_pct("storage.buffer.prefetch",
                                        "usefulness_floor_pct",
                                        &config.prefetch_usefulness_floor_pct);
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_prefetch_pct("storage.buffer.thrash",
                                        "session_budget_pct",
                                        &config.thrash_session_budget_pct);
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_prefetch_pct("storage.buffer.thrash",
                                        "object_budget_pct",
                                        &config.thrash_object_budget_pct);
            if (status != Status::OK)
            {
                return status;
            }
            status = apply_prefetch_pct("storage.buffer.thrash",
                                        "prefetch_pressure_pct",
                                        &config.thrash_prefetch_pressure_pct);
            if (status != Status::OK)
            {
                return status;
            }

            if (config.prefetch_enabled)
            {
                if (config.prefetch_workers == 0)
                {
                    SET_ERROR_CONTEXT(
                        ctx,
                        Status::INVALID_ARGUMENT,
                        "storage.buffer.prefetch.workers must be positive when prefetch is enabled");
                    return Status::INVALID_ARGUMENT;
                }
                if (config.prefetch_max_debt_pages == 0)
                {
                    SET_ERROR_CONTEXT(
                        ctx,
                        Status::INVALID_ARGUMENT,
                        "storage.buffer.prefetch.max_debt_pages must be positive when prefetch is enabled");
                    return Status::INVALID_ARGUMENT;
                }
            }

            config.recomputeDomainFrames();
            const auto &critical_budget =
                config.domainBudget(BufferPool::PolicyDomain::CriticalSystem);
            const auto &version_budget =
                config.domainBudget(BufferPool::PolicyDomain::VersionUndo);
            if (critical_budget.min_frames + version_budget.min_frames >= config.pool_size)
            {
                SET_ERROR_CONTEXT(
                    ctx,
                    Status::INVALID_ARGUMENT,
                    "storage.buffer domain minimums leave no shared probationary capacity");
                return Status::INVALID_ARGUMENT;
            }

            const bool low_valid =
                (config.dirty_ratio_low >= 0.0 && config.dirty_ratio_low <= 1.0);
            const bool high_valid =
                (config.dirty_ratio_high >= 0.0 && config.dirty_ratio_high <= 1.0);
            const bool checkpoint_valid =
                (config.dirty_ratio_checkpoint >= 0.0 && config.dirty_ratio_checkpoint <= 1.0);
            if (!low_valid || !high_valid || !checkpoint_valid ||
                config.dirty_ratio_low > config.dirty_ratio_high ||
                config.dirty_ratio_high > config.dirty_ratio_checkpoint)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "buffer_pool dirty ratios must satisfy 0 <= low <= high <= checkpoint <= 1");
                return Status::INVALID_ARGUMENT;
            }

            *config_out = config;
            return Status::OK;
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

        void appendResourceCandidate(const std::filesystem::path& candidate,
                                     std::vector<std::filesystem::path>& out,
                                     std::unordered_set<std::string>& seen)
        {
            if (candidate.empty())
            {
                return;
            }
            std::error_code ec;
            std::filesystem::path normalized = std::filesystem::absolute(candidate, ec);
            if (ec || normalized.empty())
            {
                normalized = candidate;
                ec.clear();
            }
            normalized = normalized.lexically_normal();
            if (!std::filesystem::exists(normalized, ec) || ec)
            {
                return;
            }
            ec.clear();
            if (!std::filesystem::is_directory(normalized, ec) || ec)
            {
                return;
            }
            const std::string key = normalized.string();
            if (!seen.insert(key).second)
            {
                return;
            }
            out.push_back(normalized);
        }

        std::vector<std::filesystem::path> discoverResourceRoots()
        {
            std::vector<std::filesystem::path> roots;
            std::unordered_set<std::string> seen;

#ifdef SCRATCHBIRD_INSTALL_RESOURCE_ROOT
            appendResourceCandidate(std::filesystem::path(SCRATCHBIRD_INSTALL_RESOURCE_ROOT), roots, seen);
#endif
#ifndef _WIN32
            {
                std::error_code exe_ec;
                const std::filesystem::path self_path = std::filesystem::read_symlink("/proc/self/exe", exe_ec);
                if (!exe_ec && !self_path.empty())
                {
                    appendResourceCandidate(
                        (self_path.parent_path() / ".." / "share" / "scratchbird" / "resources").lexically_normal(),
                        roots,
                        seen);
                }
            }
#endif
            if (const char* env_root = std::getenv("SCRATCHBIRD_RESOURCE_ROOT"))
            {
                appendResourceCandidate(std::filesystem::path(env_root), roots, seen);
            }
            if (const char* env_project_root = std::getenv("SCRATCHBIRD_PROJECT_ROOT"))
            {
                appendResourceCandidate(std::filesystem::path(env_project_root) / "resources",
                                        roots, seen);
            }

            std::error_code ec;
            std::filesystem::path current = std::filesystem::current_path(ec);
            if (!ec && !current.empty())
            {
                current = current.lexically_normal();
                while (!current.empty())
                {
                    appendResourceCandidate(current / "resources", roots, seen);
                    const std::filesystem::path parent = current.parent_path();
                    if (parent == current)
                    {
                        break;
                    }
                    current = parent;
                }
            }

            std::filesystem::path source_path(__FILE__);
            std::filesystem::path source_dir = source_path.parent_path();
            if (!source_dir.empty())
            {
                source_dir = source_dir.lexically_normal();
                while (!source_dir.empty())
                {
                    appendResourceCandidate(source_dir / "resources", roots, seen);
                    const std::filesystem::path parent = source_dir.parent_path();
                    if (parent == source_dir)
                    {
                        break;
                    }
                    source_dir = parent;
                }
            }

            if (const char* home = std::getenv("HOME"))
            {
                appendResourceCandidate(
                    std::filesystem::path(home) / "CliWork" / "ScratchBird" / "resources",
                    roots, seen);
            }

            return roots;
        }

        std::filesystem::path resolveResourceEntry(const std::filesystem::path& relative)
        {
            const auto roots = discoverResourceRoots();
            std::error_code ec;
            for (const auto& root : roots)
            {
                std::filesystem::path candidate = (root / relative).lexically_normal();
                if (std::filesystem::exists(candidate, ec) && !ec)
                {
                    return candidate;
                }
                ec.clear();
            }
            return {};
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

                const std::filesystem::path charsets_json =
                    resolveResourceEntry(std::filesystem::path("charsets") / "charsets.json");
                if (need_charsets && !charsets_json.empty())
                {
                    cs_status = loader.loadFromJSONFile(charsets_json.string(), ctx);
                    if (cs_status != Status::OK)
                    {
                        return cs_status;
                    }
                }
                else if (need_charsets)
                {
                    LOG_WARNING(GENERAL, "Charset resources not found; using built-in defaults");
                }

                const std::filesystem::path collations_json =
                    resolveResourceEntry(std::filesystem::path("collations") / "collations.json");
                if (need_collations && !collations_json.empty())
                {
                    cs_status = loader.loadCollationsFromJSONFile(collations_json.string(), ctx);
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
                const std::filesystem::path timezone_dir =
                    resolveResourceEntry(std::filesystem::path("timezones"));
                if (!timezone_dir.empty())
                {
                    zoneinfo_dir = timezone_dir.string();
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
                    const std::filesystem::path timezone_version_file =
                        resolveResourceEntry(std::filesystem::path("timezones") / "version");
                    if (readVersionFile(zoneinfo_dir + "/version", tzdata_version) ||
                        (!timezone_version_file.empty() &&
                         readVersionFile(timezone_version_file.string(), tzdata_version)))
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
            const std::filesystem::path i18n_version_file =
                resolveResourceEntry(std::filesystem::path("i18n") / "version");
            if (!i18n_version_file.empty() &&
                readVersionFile(i18n_version_file.string(), resource_version))
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
    constexpr uint32_t kBootstrapScramIterations = 4096;
    constexpr const char* kBootstrapAuthManifestRelativePath = "bootstrap/default_auth_manifest.json";

    struct BootstrapUserSpec
    {
        std::string username;
        std::string password;
        bool is_superuser = false;
        bool seed_on_database_bootstrap = false;
    };

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
        try
        {
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
                    password, algo, kBootstrapScramIterations, salt, stored_key, server_key);
                if (status != core::Status::OK)
                {
                    return status;
                }

                json entry = json::object();
                entry["iterations"] = kBootstrapScramIterations;
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
        catch (const std::exception&)
        {
            out.clear();
            return core::Status::INTERNAL_ERROR;
        }
    }

    core::Status loadBootstrapAuthManifest(std::vector<BootstrapUserSpec>& users_out,
                                           ErrorContext* ctx)
    {
        using json = nlohmann::json;
        users_out.clear();

        const std::filesystem::path manifest_path =
            resolveResourceEntry(std::filesystem::path(kBootstrapAuthManifestRelativePath));
        if (manifest_path.empty())
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::NOT_FOUND,
                              "Bootstrap auth manifest not found in runtime resources");
            return Status::NOT_FOUND;
        }

        std::ifstream manifest_stream(manifest_path);
        if (!manifest_stream.is_open())
        {
            if (ctx)
            {
                const std::string message =
                    std::string("Failed to open bootstrap auth manifest: ") +
                    manifest_path.string();
                ctx->set(Status::IO_ERROR,
                         message.c_str(),
                         __FILE__, __LINE__, __func__);
            }
            return Status::IO_ERROR;
        }

        try
        {
            const json doc = json::parse(manifest_stream);
            if (!doc.is_object())
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::INVALID_ARGUMENT,
                                  "Bootstrap auth manifest must be a JSON object");
                return Status::INVALID_ARGUMENT;
            }

            const int format_version = doc.value("format_version", 0);
            if (format_version != 1)
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::INVALID_ARGUMENT,
                                  "Unsupported bootstrap auth manifest format_version");
                return Status::INVALID_ARGUMENT;
            }

            auto defaults_it = doc.find("defaults");
            if (defaults_it == doc.end() || !defaults_it->is_object())
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::INVALID_ARGUMENT,
                                  "Bootstrap auth manifest missing defaults object");
                return Status::INVALID_ARGUMENT;
            }

            auto scratchbird_it = defaults_it->find("scratchbird");
            if (scratchbird_it == defaults_it->end() || !scratchbird_it->is_array())
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::INVALID_ARGUMENT,
                                  "Bootstrap auth manifest missing defaults.scratchbird array");
                return Status::INVALID_ARGUMENT;
            }

            for (const auto& entry : *scratchbird_it)
            {
                if (!entry.is_object())
                {
                    SET_ERROR_CONTEXT(ctx,
                                      Status::INVALID_ARGUMENT,
                                      "Bootstrap auth manifest entries must be JSON objects");
                    return Status::INVALID_ARGUMENT;
                }

                auto username_it = entry.find("username");
                auto password_it = entry.find("password");
                auto superuser_it = entry.find("is_superuser");
                auto seed_it = entry.find("seed_on_database_bootstrap");
                if (username_it == entry.end() || !username_it->is_string() ||
                    password_it == entry.end() || !password_it->is_string() ||
                    superuser_it == entry.end() || !superuser_it->is_boolean() ||
                    seed_it == entry.end() || !seed_it->is_boolean())
                {
                    SET_ERROR_CONTEXT(ctx,
                                      Status::INVALID_ARGUMENT,
                                      "Bootstrap auth manifest scratchbird entries require username, password, is_superuser, and seed_on_database_bootstrap");
                    return Status::INVALID_ARGUMENT;
                }

                BootstrapUserSpec spec;
                spec.username = username_it->get<std::string>();
                spec.password = password_it->get<std::string>();
                spec.is_superuser = superuser_it->get<bool>();
                spec.seed_on_database_bootstrap = seed_it->get<bool>();
                if (spec.username.empty() || spec.password.empty())
                {
                    SET_ERROR_CONTEXT(ctx,
                                      Status::INVALID_ARGUMENT,
                                      "Bootstrap auth manifest entries may not use empty username or password");
                    return Status::INVALID_ARGUMENT;
                }

                users_out.push_back(std::move(spec));
            }
        }
        catch (const json::exception& ex)
        {
            if (ctx)
            {
                const std::string message =
                    std::string("Invalid bootstrap auth manifest JSON: ") + ex.what();
                ctx->set(Status::INVALID_ARGUMENT,
                         message.c_str(),
                         __FILE__, __LINE__, __func__);
            }
            return Status::INVALID_ARGUMENT;
        }

        if (users_out.empty())
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::INVALID_ARGUMENT,
                              "Bootstrap auth manifest contains no ScratchBird credential entries");
            return Status::INVALID_ARGUMENT;
        }

        return Status::OK;
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

    core::Status ensureBootstrapUsersFromManifest(CatalogManager* catalog, ErrorContext* ctx)
    {
        if (catalog == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<BootstrapUserSpec> users;
        auto status = loadBootstrapAuthManifest(users, ctx);
        if (status != Status::OK)
        {
            if (status == Status::NOT_FOUND)
            {
                CatalogManager::BootstrapState bootstrap_state =
                    CatalogManager::BootstrapState::UNINITIALIZED;
                ErrorContext bootstrap_ctx;
                const Status bootstrap_status =
                    catalog->getBootstrapState(bootstrap_state, &bootstrap_ctx);
                if (bootstrap_status == Status::OK &&
                    bootstrap_state != CatalogManager::BootstrapState::UNINITIALIZED)
                {
                    LOG_WARNING(GENERAL,
                                "Bootstrap auth manifest unavailable; preserving existing "
                                "catalog principals (state=%d)",
                                static_cast<int>(bootstrap_state));
                    if (ctx)
                    {
                        ctx->set(Status::OK, "", __FILE__, __LINE__, __func__);
                    }
                    return Status::OK;
                }
            }
            return status;
        }

        bool seeded_any = false;
        for (const auto& user : users)
        {
            if (!user.seed_on_database_bootstrap)
            {
                continue;
            }

            std::string password_hash;
            status = buildPasswordHashPayload(user.username, user.password, password_hash);
            if (status != Status::OK)
            {
                if (ctx)
                {
                    const std::string message =
                        std::string("Failed to build bootstrap password hash for user ") +
                        user.username;
                    ctx->set(status,
                             message.c_str(),
                             __FILE__, __LINE__, __func__);
                }
                return status;
            }

            ID user_id;
            status = catalog->ensureUserExists(user.username,
                                               password_hash,
                                               ID(),
                                               user.is_superuser,
                                               user_id,
                                               ctx);
            if (status != Status::OK)
            {
                return status;
            }
            seeded_any = true;
        }

        if (!seeded_any)
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::INVALID_ARGUMENT,
                              "Bootstrap auth manifest does not mark any ScratchBird users for database bootstrap");
            return Status::INVALID_ARGUMENT;
        }

        return Status::OK;
    }
    } // namespace

    auto databaseFormatVersionToString(uint32_t version) -> std::string
    {
        std::ostringstream out;
        out << ((version >> 24) & 0xFFu) << '.'
            << ((version >> 16) & 0xFFu) << '.'
            << ((version >> 8) & 0xFFu) << '.'
            << (version & 0xFFu);
        return out.str();
    }

    auto validateDatabaseFormatCompatibility(uint32_t db_version,
                                             uint32_t db_compat_version,
                                             ErrorContext *ctx) -> Status
    {
        if (db_version == 0u || db_compat_version == 0u)
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::PAGE_CORRUPT,
                              "Database format version fields must be non-zero");
            return Status::PAGE_CORRUPT;
        }

        const auto is_known_version = [](uint32_t version) {
            return version == DB_VERSION_ALPHA_1_0_1 || version == DB_VERSION_CURRENT;
        };

        if (db_version > DB_VERSION_CURRENT)
        {
            const std::string message =
                "Database on-disk format version " + databaseFormatVersionToString(db_version) +
                " is newer than engine baseline " +
                databaseFormatVersionToString(DB_VERSION_CURRENT);
            SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, message.c_str());
            return Status::NOT_SUPPORTED;
        }

        if (!is_known_version(db_version))
        {
            const std::string message =
                "Database on-disk format version " + databaseFormatVersionToString(db_version) +
                " is not recognized in the compatibility matrix";
            SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, message.c_str());
            return Status::NOT_SUPPORTED;
        }

        if (db_compat_version > DB_VERSION_CURRENT)
        {
            const std::string message =
                "Database requires engine version at least " +
                databaseFormatVersionToString(db_compat_version) +
                "; current on-disk baseline is " +
                databaseFormatVersionToString(DB_VERSION_CURRENT);
            SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, message.c_str());
            return Status::NOT_SUPPORTED;
        }

        if (!is_known_version(db_compat_version))
        {
            const std::string message =
                "Database compatibility floor " + databaseFormatVersionToString(db_compat_version) +
                " is not recognized in the compatibility matrix";
            SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, message.c_str());
            return Status::NOT_SUPPORTED;
        }

        return Status::OK;
    }

    Database::Database()
    {
        table_stats_manager_ = std::make_unique<TableStatsManager>(this);
        mga_failpoint_manager_ = std::make_unique<MgaFailpointManager>(this);
    }

    Database::~Database()
    {
        close();
    }

    auto Database::write_admission_fenced() const -> bool
    {
        std::lock_guard<std::mutex> lock(writeback_failure_mutex_);
        return write_admission_fenced_;
    }

    auto Database::write_admission_status() const -> Status
    {
        std::lock_guard<std::mutex> lock(writeback_failure_mutex_);
        return write_admission_failure_status_;
    }

    auto Database::clearWritebackFailureState(ErrorContext *ctx) -> Status
    {
        WritebackIncidentControlState cleared{};
        {
            std::lock_guard<std::mutex> lock(writeback_failure_mutex_);
            writeback_incident_open_ = false;
            writeback_degraded_state_ = WritebackDegradedState::NORMAL;
            write_admission_fenced_ = false;
            write_admission_failure_status_ = Status::OK;
            writeback_incident_persist_in_progress_ = true;
        }

        Status status = mutateAndPersistSystemStatePageDirect(
            this,
            true,
            ctx,
            [&](BootstrapSystemStatePage *state_page) {
                storeWritebackIncidentControlState(state_page, cleared);
            });

        {
            std::lock_guard<std::mutex> lock(writeback_failure_mutex_);
            writeback_incident_persist_in_progress_ = false;
            if (status != Status::OK)
            {
                writeback_incident_open_ = true;
                writeback_degraded_state_ = WritebackDegradedState::WRITE_FENCED;
                write_admission_fenced_ = true;
                write_admission_failure_status_ =
                    status == Status::OK ? Status::IO_ERROR : status;
            }
            else
            {
                clean_shutdown_eligible_ = startup_state_loaded_;
            }
        }

        if (status == Status::OK && catalog_manager_ != nullptr)
        {
            ErrorContext history_ctx;
            CatalogManager::WritebackIncidentCatalogInfo incident{};
            if (catalog_manager_->getOpenWritebackIncidentCatalogEntry(incident, &history_ctx) ==
                Status::OK)
            {
                incident.is_open = false;
                incident.degraded_state = WritebackDegradedState::NORMAL;
                incident.last_seen_time = defaultTimeSource().nowMicros();
                incident.last_error_status = Status::OK;
                if (!incident.has_clearance_condition_uuid)
                {
                    incident.has_clearance_condition_uuid = true;
                    incident.clearance_condition_uuid = generateUuidV7();
                }
                ErrorContext update_ctx;
                const Status update_status =
                    catalog_manager_->upsertWritebackIncidentCatalogEntry(incident, &update_ctx);
                if (update_status != Status::OK)
                {
                    LOG_WARNING(STORAGE,
                                "Failed to close writeback incident history row: %d (%s)",
                                static_cast<int>(update_status),
                                update_ctx.message.c_str());
                }
            }
        }
        return status;
    }

    void Database::noteWritebackFailure(Status status,
                                        WritebackFailureClass failure_class,
                                        const WritebackAttribution &attribution,
                                        bool skip_sync,
                                        ErrorContext *ctx)
    {
        // AUDIT CONTRACT:
        // A persisted writeback incident fences new durability claims. Once this
        // path runs, commit/publication code must fail closed until the incident is
        // explicitly cleared or enforcement is suspended for controlled repair.
        WritebackIncidentControlState incident{};
        bool should_persist = false;

        {
            std::lock_guard<std::mutex> lock(writeback_failure_mutex_);
            const uint64_t now = defaultTimeSource().nowMicros();
            write_admission_fenced_ = true;
            write_admission_failure_status_ =
                status == Status::OK ? Status::IO_ERROR : status;
            clean_shutdown_eligible_ = false;
            writeback_incident_open_ = true;
            writeback_degraded_state_ = WritebackDegradedState::WRITE_FENCED;

            incident.version = SYSTEM_STATE_WRITEBACK_INCIDENT_VERSION;
            incident.incident_open = true;
            incident.filespace_id = attribution.filespace_id;
            incident.queue_kind = attribution.queue_kind;
            incident.policy_domain = attribution.policy_domain;
            incident.page_class = attribution.page_class;
            incident.dirty_generation = attribution.dirty_generation;
            incident.failure_class = failure_class;
            incident.degraded_state = WritebackDegradedState::WRITE_FENCED;
            incident.last_error_status = write_admission_failure_status_;
            incident.first_seen_time = now;
            incident.last_retry_time = now;
            incident.retry_count = 1;

            if (!writeback_incident_persist_in_progress_)
            {
                writeback_incident_persist_in_progress_ = true;
                should_persist = true;
            }
        }

        if (!should_persist)
        {
            return;
        }

        ErrorContext persist_ctx;
        const Status persist_status = mutateAndPersistSystemStatePageDirect(
            this,
            !skip_sync,
            &persist_ctx,
            [&](BootstrapSystemStatePage *state_page) {
                WritebackIncidentControlState existing{};
                loadWritebackIncidentControlState(*state_page, &existing);
                if (existing.version != SYSTEM_STATE_WRITEBACK_INCIDENT_VERSION ||
                    !existing.incident_open)
                {
                    existing = incident;
                }
                else
                {
                    existing.incident_open = true;
                    existing.filespace_id = incident.filespace_id;
                    existing.queue_kind = incident.queue_kind;
                    existing.policy_domain = incident.policy_domain;
                    existing.page_class = incident.page_class;
                    existing.dirty_generation = incident.dirty_generation;
                    existing.last_retry_time = incident.last_retry_time;
                    existing.retry_count += 1;
                    existing.failure_class = incident.failure_class;
                    existing.degraded_state = incident.degraded_state;
                    existing.last_error_status = incident.last_error_status;
                }
                storeWritebackIncidentControlState(state_page, existing);
            });

        {
            std::lock_guard<std::mutex> lock(writeback_failure_mutex_);
            writeback_incident_persist_in_progress_ = false;
        }

        if (persist_status != Status::OK)
        {
            LOG_WARNING(STORAGE,
                        "Failed to persist writeback incident state: %d (%s)",
                        static_cast<int>(persist_status),
                        persist_ctx.message.c_str());
            if (ctx != nullptr && ctx->message.empty())
            {
                ctx->set(persist_status,
                         persist_ctx.message.c_str(),
                         __FILE__,
                         __LINE__,
                         __func__);
            }
        }
        else if (catalog_manager_ != nullptr)
        {
            ErrorContext history_ctx;
            CatalogManager::WritebackIncidentCatalogInfo history{};
            if (catalog_manager_->getOpenWritebackIncidentCatalogEntry(history, &history_ctx) ==
                Status::OK)
            {
                history.queue_kind = attribution.queue_kind;
                history.policy_domain = attribution.policy_domain;
                history.page_class = attribution.page_class;
                history.failure_class = failure_class;
                history.last_seen_time = incident.last_retry_time;
                history.retry_count = std::max<uint64_t>(history.retry_count + 1, 1);
                history.degraded_state = WritebackDegradedState::WRITE_FENCED;
                history.is_open = true;
                history.last_error_status = write_admission_failure_status_;
            }
            else
            {
                history = CatalogManager::WritebackIncidentCatalogInfo{};
                history.writeback_incident_uuid = generateUuidV7();
                history.queue_kind = attribution.queue_kind;
                history.policy_domain = attribution.policy_domain;
                history.page_class = attribution.page_class;
                history.failure_class = failure_class;
                history.first_seen_time = incident.first_seen_time;
                history.last_seen_time = incident.last_retry_time;
                history.retry_count = incident.retry_count;
                history.degraded_state = WritebackDegradedState::WRITE_FENCED;
                history.is_open = true;
                history.is_valid = true;
                history.last_error_status = write_admission_failure_status_;
            }

            ErrorContext update_ctx;
            const bool previous_enforcement_state = write_admission_enforcement_suspended_;
            write_admission_enforcement_suspended_ = true;
            const Status update_status =
                catalog_manager_->upsertWritebackIncidentCatalogEntry(history, &update_ctx);
            write_admission_enforcement_suspended_ = previous_enforcement_state;
            if (update_status != Status::OK)
            {
                LOG_WARNING(STORAGE,
                            "Failed to persist writeback incident history row: %d (%s)",
                            static_cast<int>(update_status),
                            update_ctx.message.c_str());
            }
        }
    }

    // NOTE: Move operations deleted in header because Database contains std::mutex (non-movable)

    void Database::close()
    {
        clean_shutdown_eligible_ = clean_shutdown_eligible_ && startup_state_loaded_;

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
            // Preserve dormant catalog records and their reattach AuthKeys across
            // close/open so restart can reconstruct a replacement transaction
            // instead of silently discarding the engine-owned recovery token.
            dormant_contexts_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(connection_registry_mutex_);
            connection_registry_.clear();
        }

        // Shut down domain manager
        domain_manager_.reset();
        clearDomainControlPlaneReplicaCatalog(db_uuid_);

        // Shut down encryption key manager
        encryption_key_manager_.reset();

        // Shut down audit logger before catalog manager
        audit_logger_.reset();

        // Shut down local workload governance before the catalog manager it queries.
        workload_governance_.reset();
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

        // Shut down MGA backout engine before the storage engine it drives.
        mga_backout_engine_.reset();

        // Shut down storage engine
        storage_engine_.reset();

        table_stats_manager_.reset();

        if (buffer_pool_ != nullptr)
        {
            // Quiesce the background writer before clean-shutdown publication so
            // checkpoint-boundary draining does not race a concurrent writeback path.
            buffer_pool_->quiesceBackgroundWriterForShutdown();

            ErrorContext ctx;
            if (clean_shutdown_eligible_)
            {
                markCleanShutdown(&ctx);
            }
            else
            {
                buffer_pool_->flushAll(&ctx);
                sync(&ctx);
            }
        }

        // Flush page-manager state after clean-shutdown publication so any
        // catalog pages allocated for runtime-history rows are reflected in the
        // persisted FSM state seen on the next open.
        if (page_manager_ != nullptr)
        {
            ErrorContext ctx;
            page_manager_->flush(&ctx);
        }

        if (buffer_pool_ != nullptr && clean_shutdown_eligible_)
        {
            ErrorContext ctx;
            const uint64_t shutdown_dirty_boundary =
                buffer_pool_->currentDirtyGeneration();
            (void)buffer_pool_->flushDirtyCheckpointBoundary(shutdown_dirty_boundary, &ctx);
            (void)sync(&ctx);
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

        // Shut down catalog manager after clean-shutdown publication, since
        // markCleanShutdown() persists runtime history through the live
        // catalog/buffer/page-manager stack.
        catalog_manager_.reset();

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
        {
            std::lock_guard<std::mutex> lock(shadow_filespace_mutex_);
            for (auto& [shadow_id, entry] : shadow_filespaces_)
            {
                (void) shadow_id;
                if (entry.fd >= 0)
                {
                    ::close(entry.fd);
                    entry.fd = -1;
                }
            }
            shadow_filespaces_.clear();
            shadow_filespace_by_source_.clear();
        }

        startup_state_loaded_ = false;
        clean_shutdown_eligible_ = false;
        startup_generation_ = 0;
        restart_generation_ = 0;
        last_clean_shutdown_generation_ = 0;
        last_shutdown_was_clean_ = true;
        write_admission_enforcement_suspended_ = true;
        startup_reconciliation_state_ = {};
        startup_quarantine_active_ = false;
        write_admission_enforcement_suspended_ = false;
        {
            std::lock_guard<std::mutex> lock(writeback_failure_mutex_);
            writeback_incident_open_ = false;
            writeback_degraded_state_ = WritebackDegradedState::NORMAL;
            write_admission_fenced_ = false;
            write_admission_failure_status_ = Status::OK;
            writeback_incident_persist_in_progress_ = false;
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

    Status Database::applyDormantTransactionPolicyConfig(ErrorContext* /*ctx*/)
    {
        refreshDormantTransactionPolicyFromConfig();
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
                const std::string detail =
                    (ctx != nullptr && !ctx->message.empty()) ? ctx->message : "no detail";
                SET_ERROR_CONTEXT(ctx,
                                  s,
                                  ("Failed to initialize connection context: " + detail).c_str());
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
        for (auto it = connection_registry_.begin(); it != connection_registry_.end();)
        {
            if (it->second == ctx)
            {
                it = connection_registry_.erase(it);
                continue;
            }
            ++it;
        }
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

    void Database::refreshDormantTransactionPolicyFromConfig()
    {
        Config& cfg = Config::getInstance();
        dormant_transaction_policy_.restart_reattach_policy =
            parseDormantRestartReattachPolicy(
                cfg.getString("transactions",
                              "dormant_restart_reattach_policy",
                              "allow_replacement"));
        dormant_transaction_policy_.cleanup_policy =
            parseDormantCleanupPolicy(
                cfg.getString("transactions",
                              "dormant_cleanup_policy",
                              "rollback_expired"));
        dormant_transaction_policy_.lease_seconds =
            cfg.getUInt("transactions",
                        "dormant_lease_seconds",
                        config::DEFAULT_DORMANT_TXN_LEASE_SECONDS);
        dormant_transaction_policy_.terminal_retention_seconds =
            cfg.getUInt("transactions",
                        "dormant_terminal_retention_seconds",
                        86400);
    }

    auto Database::dormantTransactionPolicy() const -> DormantTransactionPolicySnapshot
    {
        DormantTransactionPolicySnapshot snapshot = dormant_transaction_policy_;
        Config& cfg = Config::getInstance();
        snapshot.restart_reattach_policy =
            parseDormantRestartReattachPolicy(
                cfg.getString("transactions",
                              "dormant_restart_reattach_policy",
                              "allow_replacement"));
        snapshot.cleanup_policy =
            parseDormantCleanupPolicy(
                cfg.getString("transactions",
                              "dormant_cleanup_policy",
                              "rollback_expired"));
        snapshot.lease_seconds =
            cfg.getUInt("transactions",
                        "dormant_lease_seconds",
                        config::DEFAULT_DORMANT_TXN_LEASE_SECONDS);
        snapshot.terminal_retention_seconds =
            cfg.getUInt("transactions",
                        "dormant_terminal_retention_seconds",
                        86400);
        return snapshot;
    }

    auto Database::snapshotDormantTransactions(
        std::vector<DormantTransactionSnapshot>& dormants_out,
        ErrorContext* ctx) const -> Status
    {
        dormants_out.clear();
        if (catalog_manager_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<CatalogManager::DormantTransactionInfo> rows;
        Status status = catalog_manager_->listDormantTransactions(rows, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        dormants_out.reserve(rows.size());
        for (const CatalogManager::DormantTransactionInfo& info : rows)
        {
            DormantTransactionSnapshot row{};
            row.dormant_id = info.dormant_id;
            row.attachment_id = info.attachment_id;
            row.session_id = info.session_id;
            row.user_id = info.user_id;
            row.session_user_id = info.session_user_id;
            row.role_id = info.role_id;
            row.current_schema_id = info.current_schema_id;
            row.server_instance_id = info.server_instance_id;
            row.proc_id = info.proc_id;
            row.txn_id = info.txn_id;
            row.isolation_level = info.isolation_level;
            row.read_only = info.access_mode == CatalogManager::DormantAccessMode::READ_ONLY;
            row.autocommit_mode = info.autocommit_mode;
            row.wait_for_locks = info.wait_mode == CatalogManager::DormantWaitMode::WAIT;
            row.lock_timeout_seconds = info.lock_timeout_seconds;
            row.state = static_cast<uint8_t>(info.state);
            row.start_time = info.start_time;
            row.last_activity_time = info.last_activity_time;
            row.dormant_since = info.dormant_since;
            row.lease_expires_at = info.lease_expires_at;
            row.last_statement_time = info.last_statement_time;
            row.last_statement_hash = info.last_statement_hash;
            row.last_rows_affected = info.last_rows_affected;
            row.last_error_code = info.last_error_code;
            row.last_sqlstate = info.last_sqlstate;
            row.last_statement_text = info.last_statement_text;
            row.session_settings = info.session_settings;
            dormants_out.push_back(std::move(row));
        }

        return Status::OK;
    }

    auto Database::maintainDormantTransactions(uint32_t* normalized_out,
                                               ErrorContext* ctx) -> Status
    {
        if (normalized_out != nullptr)
        {
            *normalized_out = 0;
        }
        if (catalog_manager_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        refreshDormantTransactionPolicyFromConfig();

        std::vector<CatalogManager::DormantTransactionInfo> rows;
        Status status = catalog_manager_->listDormantTransactions(rows, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        const uint64_t now_micros = std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count();
        const bool rollback_expired =
            dormant_transaction_policy_.cleanup_policy ==
                DormantCleanupPolicy::ROLLBACK_EXPIRED ||
            dormant_transaction_policy_.cleanup_policy ==
                DormantCleanupPolicy::ROLLBACK_EXPIRED_AND_PURGE;
        const bool purge_terminal =
            dormant_transaction_policy_.cleanup_policy ==
            DormantCleanupPolicy::ROLLBACK_EXPIRED_AND_PURGE;

        uint32_t normalized = 0;
        for (CatalogManager::DormantTransactionInfo& info : rows)
        {
            const bool lease_expired =
                info.lease_expires_at != 0 && now_micros > info.lease_expires_at;
            const bool restart_stale =
                !isZeroId(info.server_instance_id) &&
                info.server_instance_id != server_instance_id_;

            std::unique_ptr<ConnectionContext> released_connection;
            ID live_authkey{};

            if (info.state == CatalogManager::DormantTransactionState::DORMANT &&
                lease_expired &&
                rollback_expired)
            {
                std::lock_guard<std::mutex> lock(dormant_mutex_);
                auto it = dormant_contexts_.find(info.dormant_id);
                if (it != dormant_contexts_.end())
                {
                    live_authkey = it->second.reattach_authkey_id;
                    released_connection = std::move(it->second.connection);
                    dormant_contexts_.erase(it);
                }
            }

            bool updated = false;
            if (info.state == CatalogManager::DormantTransactionState::DORMANT)
            {
                if (lease_expired &&
                    dormant_transaction_policy_.cleanup_policy != DormantCleanupPolicy::KEEP)
                {
                    info.state = released_connection
                        ? CatalogManager::DormantTransactionState::ROLLED_BACK
                        : CatalogManager::DormantTransactionState::EXPIRED;
                    info.last_activity_time = now_micros;
                    updated = true;
                }
                else if (restart_stale &&
                         dormant_transaction_policy_.restart_reattach_policy ==
                             DormantRestartReattachPolicy::DENY_AFTER_RESTART)
                {
                    info.state = CatalogManager::DormantTransactionState::EXPIRED;
                    info.last_activity_time = now_micros;
                    updated = true;
                }
            }

            if (released_connection)
            {
                released_connection.reset();
                if (!isZeroId(live_authkey))
                {
                    ErrorContext revoke_ctx;
                    catalog_manager_->revokeAuthKey(live_authkey, &revoke_ctx);
                    logReattachAuditEvent(audit_logger_.get(),
                                          AuditEventType::REATTACH_TOKEN_REVOKED,
                                          info.user_id,
                                          info.session_id,
                                          live_authkey,
                                          info.dormant_id,
                                          true,
                                          "dormant_cleanup_rollback");
                }
            }

            if (updated)
            {
                status = catalog_manager_->updateDormantTransaction(info, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                ++normalized;
            }

            const uint64_t reference_time = info.last_activity_time != 0
                ? info.last_activity_time
                : info.dormant_since;
            if (purge_terminal &&
                isDormantTerminalState(info.state) &&
                dormant_transaction_policy_.terminal_retention_seconds != 0 &&
                reference_time != 0 &&
                now_micros >=
                    reference_time +
                        (dormant_transaction_policy_.terminal_retention_seconds * 1000000ULL))
            {
                status = catalog_manager_->deleteDormantTransaction(info.dormant_id, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                ++normalized;
            }
        }

        if (normalized_out != nullptr)
        {
            *normalized_out = normalized;
        }
        return Status::OK;
    }

    Status Database::detachToDormant(std::unique_ptr<ConnectionContext> &connection,
                                     ID &dormant_id_out,
                                     ErrorContext *ctx,
                                     ID *reattach_authkey_out)
    {
        refreshDormantTransactionPolicyFromConfig();

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

        uint64_t lease_seconds = dormant_transaction_policy_.lease_seconds;
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
        refreshDormantTransactionPolicyFromConfig();
        ErrorContext maintenance_ctx;
        Status maintenance_status = maintainDormantTransactions(nullptr, &maintenance_ctx);
        if (maintenance_status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx,
                              maintenance_status,
                              maintenance_ctx.message.empty()
                                  ? "Failed to apply dormant transaction maintenance"
                                  : maintenance_ctx.message.c_str());
            return maintenance_status;
        }

        if (!reattach_authkey || isZeroId(*reattach_authkey))
        {
            logReattachAuditEvent(audit_logger_.get(),
                                  AuditEventType::REATTACH_FAILURE,
                                  ID{},
                                  ID{},
                                  ID{},
                                  dormant_id,
                                  false,
                                  "missing_reattach_authkey");
            SET_ERROR_CONTEXT(ctx, Status::INVALID_AUTHORIZATION, "Reattach AuthKey is required");
            return Status::INVALID_AUTHORIZATION;
        }

        if (!catalog_manager_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager::DormantTransactionInfo info;
        Status info_status = catalog_manager_->getDormantTransaction(dormant_id, info, ctx);
        if (info_status != Status::OK)
        {
            logReattachAuditEvent(audit_logger_.get(),
                                  AuditEventType::REATTACH_FAILURE,
                                  ID{},
                                  ID{},
                                  *reattach_authkey,
                                  dormant_id,
                                  false,
                                  "dormant_not_found");
            return info_status;
        }

        if (info.state != CatalogManager::DormantTransactionState::DORMANT)
        {
            logReattachAuditEvent(audit_logger_.get(),
                                  AuditEventType::REATTACH_FAILURE,
                                  info.user_id,
                                  info.session_id,
                                  *reattach_authkey,
                                  dormant_id,
                                  false,
                                  "dormant_not_reattachable");
            SET_ERROR_CONTEXT(ctx, Status::INVALID_AUTHORIZATION,
                              "Dormant transaction is not in a reattachable state");
            return Status::INVALID_AUTHORIZATION;
        }

        if (info.lease_expires_at != 0)
        {
            const uint64_t now_micros = std::chrono::duration_cast<std::chrono::microseconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count();
            if (now_micros > info.lease_expires_at)
            {
                info.state = CatalogManager::DormantTransactionState::EXPIRED;
                info.last_activity_time = now_micros;
                catalog_manager_->updateDormantTransaction(info, nullptr);
                logReattachAuditEvent(audit_logger_.get(),
                                      AuditEventType::REATTACH_FAILURE,
                                      info.user_id,
                                      info.session_id,
                                      *reattach_authkey,
                                      dormant_id,
                                      false,
                                      "dormant_lease_expired");
                SET_ERROR_CONTEXT(ctx, Status::INVALID_AUTHORIZATION,
                                  "Dormant reattach token has expired");
                return Status::INVALID_AUTHORIZATION;
            }
        }

        CatalogManager::AuthKeyInfo authkey_info;
        Status authkey_status = catalog_manager_->getAuthKey(*reattach_authkey, authkey_info, ctx);
        if (authkey_status != Status::OK ||
            authkey_info.status != CatalogManager::AuthKeyStatus::ACTIVE ||
            authkey_info.scope != CatalogManager::AuthKeyScope::REATTACH)
        {
            logReattachAuditEvent(audit_logger_.get(),
                                  AuditEventType::REATTACH_FAILURE,
                                  info.user_id,
                                  info.session_id,
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
                                  info.user_id,
                                  info.session_id,
                                  *reattach_authkey,
                                  dormant_id,
                                  false,
                                  "reattach_authkey_consume_failed");
            return consume_status;
        }

        bool recovered_after_restart = false;
        {
            std::lock_guard<std::mutex> lock(dormant_mutex_);
            auto it = dormant_contexts_.find(dormant_id);
            if (it != dormant_contexts_.end())
            {
                if (*reattach_authkey != it->second.reattach_authkey_id)
                {
                    logReattachAuditEvent(audit_logger_.get(),
                                          AuditEventType::REATTACH_FAILURE,
                                          info.user_id,
                                          info.session_id,
                                          *reattach_authkey,
                                          dormant_id,
                                          false,
                                          "reattach_authkey_mismatch");
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_AUTHORIZATION,
                                      "Reattach AuthKey mismatch");
                    return Status::INVALID_AUTHORIZATION;
                }
                connection_out = std::move(it->second.connection);
                dormant_contexts_.erase(it);
            }
        }

        if (!connection_out)
        {
            if (info.server_instance_id != server_instance_id_ &&
                dormant_transaction_policy_.restart_reattach_policy ==
                    DormantRestartReattachPolicy::DENY_AFTER_RESTART)
            {
                info.state = CatalogManager::DormantTransactionState::EXPIRED;
                info.last_activity_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                              std::chrono::system_clock::now().time_since_epoch())
                                              .count();
                catalog_manager_->updateDormantTransaction(info, nullptr);
                logReattachAuditEvent(audit_logger_.get(),
                                      AuditEventType::REATTACH_FAILURE,
                                      info.user_id,
                                      info.session_id,
                                      *reattach_authkey,
                                      dormant_id,
                                      false,
                                      "restart_policy_denied");
                SET_ERROR_CONTEXT(ctx, Status::INVALID_AUTHORIZATION,
                                  "Restart-time dormant reattach is disabled by policy");
                return Status::INVALID_AUTHORIZATION;
            }

            Status recover_status =
                recoverDormantAfterRestart(this, info, *reattach_authkey, connection_out, ctx);
            if (recover_status != Status::OK)
            {
                logReattachAuditEvent(audit_logger_.get(),
                                      AuditEventType::REATTACH_FAILURE,
                                      info.user_id,
                                      info.session_id,
                                      *reattach_authkey,
                                      dormant_id,
                                      false,
                                      "restart_recovery_failed");
                return recover_status;
            }
            recovered_after_restart = true;
        }

        info.state = CatalogManager::DormantTransactionState::REATTACHED;
        info.last_activity_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count();
        info.server_instance_id = server_instance_id_;
        catalog_manager_->updateDormantTransaction(info, nullptr);

        logReattachAuditEvent(audit_logger_.get(),
                              AuditEventType::REATTACH_SUCCESS,
                              info.user_id,
                              info.session_id,
                              *reattach_authkey,
                              dormant_id,
                              true,
                              recovered_after_restart ? "recovered_after_restart" : "reattached");
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
        state_page->restart_generation = 0;
        state_page->last_clean_shutdown_generation = 1;
        state_page->last_checkpoint_txid = 0;
        state_page->last_checkpoint_time = 0;
        state_page->config_flags = 0;
        CheckpointControlState checkpoint{};
        checkpoint.checkpoint_generation = 1;
        checkpoint.shutdown_intent = CheckpointShutdownIntent::CLEAN;
        storeCheckpointControlState(state_page, checkpoint);
        storeWritebackIncidentControlState(state_page, WritebackIncidentControlState{});

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
        header->cluster_id = ID{};
        header->node_id = ID{};
        header->cluster_config_epoch = 0;
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
        header->db_version = DB_VERSION_CURRENT;
        header->db_compat_version = DB_COMPAT_VERSION_CURRENT;

        // Get current time in microseconds
        uint64_t micros = defaultTimeSource().nowMicros();
        header->creation_time = micros;
        header->last_checkpoint = 0;
        header->latest_commit_seqno = 0;

        // Set configuration
        header->block_size = page_size;
        // Reserved compatibility slot only. Alpha durability/recovery uses MGA
        // state reconstruction and never write-ahead redo.
        header->reserved_wal_level_compat = 0;
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
        header->oldest_active_xid = header->next_transaction_id;
        header->oldest_snapshot = header->next_transaction_id;
        header->latest_completed_xid = 0;
        header->tip_root_page = BOOTSTRAP_PAGE_TX_MAP_ROOT;
        header->inventory_generation = 1;
        header->oldest_snapshot_serial = 0;

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
        const StorageLockResult lock_result =
            getStorageLockProvider().tryLockExclusive(fd, &lock_errno);
        if (lock_result != StorageLockResult::LOCKED)
        {
            ::close(fd);
            if (lock_result == StorageLockResult::LOCK_CONFLICT) {
                SET_ERROR_CONTEXT(ctx, Status::LOCK_CONFLICT,
                    "Database file is already in use by another process");
            } else {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to lock database file");
            }
            return (lock_result == StorageLockResult::LOCK_CONFLICT)
                ? Status::LOCK_CONFLICT
                : Status::IO_ERROR;
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
        auto close_with_stage_error = [this, ctx](Status stage_status,
                                                  const char* stage_name) -> Status
        {
            if (ctx)
            {
                if (!ctx->message.empty())
                {
                    ctx->message = std::string(stage_name) + ": " + ctx->message;
                }
                else
                {
                    ctx->set(stage_status, stage_name, __FILE__, __LINE__, __func__);
                }
            }
            close();
            return stage_status;
        };

        std::string canonical_path;
        Status status = Database::validate_db_path(path, canonical_path, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Close if already open
        close();

        startup_state_loaded_ = false;
        clean_shutdown_eligible_ = false;
        startup_generation_ = 0;
        restart_generation_ = 0;
        last_clean_shutdown_generation_ = 0;
        last_shutdown_was_clean_ = true;
        write_admission_enforcement_suspended_ = true;
        auto release_startup_write_admission_suspension = [this](void *) {
            write_admission_enforcement_suspended_ = false;
        };
        std::unique_ptr<void, decltype(release_startup_write_admission_suspension)>
            startup_write_admission_guard(
                reinterpret_cast<void *>(1),
                release_startup_write_admission_suspension);

        // Open file
        fd_ = platform::openFd(canonical_path.c_str(), O_RDWR);
        if (fd_ < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "Database file not found");
            return Status::FILE_NOT_FOUND;
        }

        // Lock file for exclusive access
        int lock_errno = 0;
        const StorageLockResult lock_result =
            getStorageLockProvider().tryLockExclusive(fd_, &lock_errno);
        if (lock_result != StorageLockResult::LOCKED)
        {
            ::close(fd_);
            fd_ = -1;
            if (lock_result == StorageLockResult::LOCK_CONFLICT) {
                SET_ERROR_CONTEXT(ctx, Status::LOCK_CONFLICT,
                    "Database file is already in use by another process");
            } else {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to lock database file");
            }
            return (lock_result == StorageLockResult::LOCK_CONFLICT)
                ? Status::LOCK_CONFLICT
                : Status::IO_ERROR;
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
        refreshDormantTransactionPolicyFromConfig();

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

        // Initialize buffer pool
        BufferPool::Config bp_config;
        status = loadBufferPoolConfig(page_size_, &bp_config, ctx);
        if (status != Status::OK)
        {
            close();
            return status;
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

        status = markStartupOpen(ctx);
        if (status != Status::OK)
        {
            return close_with_stage_error(status, "database.markStartupOpen");
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
            // Dormant records survive restart. Reattach will reconstruct a
            // replacement transaction from persisted session state instead of
            // replaying prior work or purging the token on open.
            status = catalog_manager_->purgeStaleSessionTemporaryTables(ctx);
            if (status != Status::OK)
            {
                return close_with_stage_error(
                    status,
                    "catalog_manager.purgeStaleSessionTemporaryTables");
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

        if (catalog_manager_ != nullptr)
        {
            status = maintainDormantTransactions(nullptr, ctx);
            if (status != Status::OK)
            {
                return close_with_stage_error(status, "database.maintainDormantTransactions");
            }
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

        try
        {
            mga_backout_engine_ = std::make_unique<MgaBackoutEngine>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate MgaBackoutEngine");
            return Status::OOM;
        }

        // Initialize CLOG manager before startup reconciliation so the startup
        // pass can republish terminal transaction state from TIP into CLOG.
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
            return close_with_stage_error(status, "clog.initialize");
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
        startup_recovery_start_time_ = defaultTimeSource().nowMicros();
        status = runStartupReconciliation(ctx);
        if (status != Status::OK)
        {
            return close_with_stage_error(status, "database.runStartupReconciliation");
        }

        status = catalog_manager_->initializePolicyToastIfNeeded(ctx);
        if (status != Status::OK)
        {
            return close_with_stage_error(status, "catalog_manager.initializePolicyToastIfNeeded");
        }
        status = ensureBootstrapUsersFromManifest(catalog_manager_.get(), ctx);
        if (status != Status::OK)
        {
            return close_with_stage_error(status, "ensureBootstrapUsersFromManifest");
        }

        status = bootstrapI18nResources(this, ctx);
        if (status != Status::OK)
        {
            return close_with_stage_error(status, "bootstrapI18nResources");
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
            return close_with_stage_error(status, "lock_manager.initialize");
        }
        status = initializeProcArray(config::DEFAULT_MAX_BACKENDS, ctx);
        if (status != Status::OK)
        {
            return close_with_stage_error(status, "database.initializeProcArray");
        }
        status = transaction_manager_->restorePreparedLockOwners(ctx);
        if (status != Status::OK)
        {
            return close_with_stage_error(status, "transaction_manager.restorePreparedLockOwners");
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
            return close_with_stage_error(status, "sweep_manager.initialize");
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
            return close_with_stage_error(status, "garbage_collector.initialize");
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
            return close_with_stage_error(status, "long_transaction_monitor.initialize");
        }

        // Start long transaction monitoring thread
        status = long_transaction_monitor_->startMonitoring(ctx);
        if (status != Status::OK)
        {
            return close_with_stage_error(status, "long_transaction_monitor.startMonitoring");
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
                return close_with_stage_error(status, "domain_manager.initialize");
            }
        }
        else
        {
            status = domain_manager_->load(ctx);
            if (status != Status::OK)
            {
                return close_with_stage_error(status, "domain_manager.load");
            }
            status = domain_manager_->ensureSystemDomains(ctx);
            if (status != Status::OK)
            {
                return close_with_stage_error(status, "domain_manager.ensureSystemDomains");
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
            return close_with_stage_error(status, "encryption_key_manager.initialize");
        }
        status = encryption_key_manager_->validateDatabaseEncryptionPolicy(ctx);
        if (status != Status::OK)
        {
            return close_with_stage_error(status, "encryption_key_manager.validateDatabaseEncryptionPolicy");
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

        try
        {
            workload_governance_ = std::make_unique<WorkloadGovernance>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate WorkloadGovernance");
            return Status::OOM;
        }

        // Initialize job scheduler (WS-4 Scheduler)
        status = applySchedulerConfig(ctx);
        if (status != Status::OK)
        {
            return close_with_stage_error(status, "applySchedulerConfig");
        }

        // Initialize virtual catalog handlers (information_schema, pg_catalog, mysql, firebird).
        scratchbird::catalog::initializeVirtualCatalogs(catalog_manager_.get());

        write_admission_enforcement_suspended_ = false;
        startup_write_admission_guard.release();
        clean_shutdown_eligible_ = !write_admission_fenced();
        DEBUG_LOG_DB("Database opened successfully");
        return Status::OK;
    }

    auto Database::persistStartupReconciliationState(
        const StartupReconciliationState &state,
        ErrorContext *ctx) -> Status
    {
        if (buffer_pool_ == nullptr || !startup_state_loaded_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Startup reconciliation requires initialized system state");
            return Status::INVALID_ARGUMENT;
        }

        Status status = mutateAndFlushSystemStatePage(
            this,
            buffer_pool_.get(),
            page_size_,
            ctx,
            [&](BootstrapSystemStatePage *state_page) {
                uint64_t flags = 0;
                if (state.clean_shutdown_marker)
                {
                    flags |= SYSTEM_STATE_STARTUP_RECON_FLAG_CLEAN_MARKER;
                }
                if (state.startup_repair)
                {
                    flags |= SYSTEM_STATE_STARTUP_RECON_FLAG_STARTUP_REPAIR;
                }
                if (state.has_page_scan_findings)
                {
                    flags |= SYSTEM_STATE_STARTUP_RECON_FLAG_PAGE_SCAN_FINDINGS;
                }
                if (state.has_corrupt_pages)
                {
                    flags |= SYSTEM_STATE_STARTUP_RECON_FLAG_CORRUPT_PAGES;
                }
                if (state.quarantine_active)
                {
                    flags |= SYSTEM_STATE_STARTUP_RECON_FLAG_QUARANTINE_ACTIVE;
                }

                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_VERSION_SLOT] =
                    SYSTEM_STATE_STARTUP_RECON_VERSION;
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_OUTCOME_SLOT] =
                    static_cast<uint64_t>(state.outcome);
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_STATUS_SLOT] =
                    static_cast<uint64_t>(state.failure_status);
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_TIP_ABORTED_SLOT] =
                    state.tip_active_to_aborted;
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_TIP_PREPARED_SLOT] =
                    state.tip_active_to_prepared;
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_STALE_PREPARED_SLOT] =
                    state.stale_prepared_records_removed;
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_CLOG_SYNC_SLOT] =
                    state.clog_states_synchronized;
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_RELINKABLE_SLOT] =
                    state.relinkable_chain_pages;
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_BLOCKED_SLOT] =
                    state.cleanup_blocked_chain_pages;
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_QUARANTINABLE_SLOT] =
                    state.quarantinable_chain_pages;
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_UNRECOVERABLE_SLOT] =
                    state.unrecoverable_chain_pages;
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_CLASS_SLOT] =
                    static_cast<uint64_t>(state.corruption_class);
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_ACTION_SLOT] =
                    static_cast<uint64_t>(state.quarantine_action);
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_REPAIR_PLAN_SLOT] =
                    state.repair_plan_mask;
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_CLASSIFICATION_SLOT] =
                    static_cast<uint64_t>(state.classification);
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_SERVICE_STATE_SLOT] =
                    static_cast<uint64_t>(state.service_state);
                state_page->reserved[SYSTEM_STATE_STARTUP_RECON_FLAGS_SLOT] = flags;
            });
        if (status != Status::OK)
        {
            return status;
        }

        startup_reconciliation_state_ = state;
        if (catalog_manager_ != nullptr)
        {
            CatalogManager::RecoveryRunCatalogInfo recovery_run{};
            recovery_run.recovery_generation = startup_generation_;
            recovery_run.classification = state.classification;
            recovery_run.start_time = startup_recovery_start_time_ == 0
                ? defaultTimeSource().nowMicros()
                : startup_recovery_start_time_;
            recovery_run.has_end_time = true;
            recovery_run.end_time = defaultTimeSource().nowMicros();
            recovery_run.normalized_transactions = state.tip_active_to_aborted +
                state.tip_active_to_prepared + state.stale_prepared_records_removed;
            recovery_run.repair_required_pages = static_cast<uint64_t>(state.relinkable_chain_pages) +
                static_cast<uint64_t>(state.cleanup_blocked_chain_pages) +
                static_cast<uint64_t>(state.quarantinable_chain_pages) +
                static_cast<uint64_t>(state.unrecoverable_chain_pages);
            recovery_run.degraded_state = state.service_state;

            ErrorContext history_ctx;
            const Status history_status =
                catalog_manager_->upsertRecoveryRunCatalogEntry(recovery_run, &history_ctx);
            if (history_status != Status::OK)
            {
                LOG_WARNING(STORAGE,
                            "Failed to persist recovery run history row: %d (%s)",
                            static_cast<int>(history_status),
                            history_ctx.message.c_str());
            }

            if (state.classification != StartupRecoveryClassification::CLEAN_SHUTDOWN_FAST_PATH)
            {
                CatalogManager::RecoveryIncidentCatalogInfo incident{};
                incident.recovery_generation = startup_generation_;
                incident.classification = state.classification;
                incident.has_checkpoint_generation = true;
                incident.checkpoint_generation = last_clean_shutdown_generation_;
                incident.created_time = recovery_run.end_time;
                incident.has_details = true;
                incident.details_json = buildStartupCorruptionPolicyMessage(state);
                ErrorContext incident_ctx;
                const Status incident_status =
                    catalog_manager_->appendRecoveryIncidentCatalogEntry(incident, &incident_ctx);
                if (incident_status != Status::OK)
                {
                    LOG_WARNING(STORAGE,
                                "Failed to persist recovery incident row: %d (%s)",
                                static_cast<int>(incident_status),
                                incident_ctx.message.c_str());
                }
            }
        }
        return Status::OK;
    }

    auto Database::runStartupReconciliation(ErrorContext *ctx) -> Status
    {
        if (page_manager_ == nullptr || transaction_manager_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Startup reconciliation requires page and transaction managers");
            return Status::INVALID_ARGUMENT;
        }

        StartupReconciliationState state{};
        startup_quarantine_active_ = false;
        state.clean_shutdown_marker = last_shutdown_was_clean_;
        auto load_current_checkpoint_state =
            [this, ctx](CheckpointControlState *checkpoint_out) -> Status {
                if (buffer_pool_ == nullptr || checkpoint_out == nullptr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Checkpoint control read requires initialized buffer pool");
                    return Status::INVALID_ARGUMENT;
                }

                void *page_buffer = nullptr;
                Status local_status = buffer_pool_->pinPage(
                    BOOTSTRAP_PAGE_SYSTEM_STATE, &page_buffer, ctx);
                if (local_status != Status::OK)
                {
                    return local_status;
                }

                auto *state_page = static_cast<BootstrapSystemStatePage *>(page_buffer);
                if (state_page->page_header.page_type != PAGE_TYPE_SYSTEM_STATE)
                {
                    buffer_pool_->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);
                    SET_ERROR_CONTEXT(
                        ctx,
                        Status::PAGE_CORRUPT,
                        "Invalid system state bootstrap page during startup reconciliation");
                    return Status::PAGE_CORRUPT;
                }
                if (!validatePageChecksum(reinterpret_cast<uint8_t *>(state_page), page_size_))
                {
                    buffer_pool_->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);
                    SET_ERROR_CONTEXT(
                        ctx,
                        Status::CHECKSUM_MISMATCH,
                        "System state page checksum validation failed during startup reconciliation");
                    return Status::CHECKSUM_MISMATCH;
                }

                loadCheckpointControlState(*state_page, checkpoint_out);
                buffer_pool_->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);
                return Status::OK;
            };
        CheckpointControlState checkpoint{};
        Status status = load_current_checkpoint_state(&checkpoint);
        if (status != Status::OK)
        {
            return status;
        }
        bool writeback_incident_open = false;
        WritebackDegradedState writeback_degraded_state = WritebackDegradedState::NORMAL;
        {
            std::lock_guard<std::mutex> lock(writeback_failure_mutex_);
            writeback_incident_open = writeback_incident_open_;
            writeback_degraded_state = writeback_degraded_state_;
        }

        PageManager::ReconstructionSummary page_summary{};
        status = page_manager_->reconstructFromPages(&page_summary,
                                                     ctx,
                                                     state.clean_shutdown_marker);
        state.relinkable_chain_pages = page_summary.relinkable_chain_pages;
        state.cleanup_blocked_chain_pages = page_summary.cleanup_blocked_chain_pages;
        state.quarantinable_chain_pages = page_summary.quarantinable_chain_pages;
        state.unrecoverable_chain_pages = page_summary.unrecoverable_chain_pages;
        state.checkpoint_queue_rebuild_required =
            checkpoint.queue_rebuild_required || page_summary.checkpoint_queue_rebuild_pages > 0;
        state.checkpoint_queue_rebuild_pages = page_summary.checkpoint_queue_rebuild_pages;
        state.checkpoint_dirty_generation_low_watermark =
            checkpoint.dirty_generation_low_watermark;
        state.checkpoint_dirty_generation_high_watermark =
            checkpoint.dirty_generation_high_watermark;
        state.has_corrupt_pages = page_summary.corrupt_pages > 0;
        state.has_page_scan_findings =
            state.has_corrupt_pages ||
            state.relinkable_chain_pages > 0 ||
            state.cleanup_blocked_chain_pages > 0 ||
            state.quarantinable_chain_pages > 0 ||
            state.unrecoverable_chain_pages > 0;
        if (status != Status::OK)
        {
            state.outcome = StartupReconciliationOutcome::FAILED_PAGE_SCAN;
            state.failure_status = status;
            finalizeStartupRecoveryClassification(
                &state, writeback_incident_open, writeback_degraded_state);
            ErrorContext persist_ctx;
            Status persist_status = persistStartupReconciliationState(state, &persist_ctx);
            if (persist_status != Status::OK)
            {
                LOG_WARNING(STORAGE,
                            "Failed to persist startup reconciliation failure state: %d",
                            static_cast<int>(persist_status));
            }
            return status;
        }

        if (state.checkpoint_queue_rebuild_required ||
            !page_summary.checkpoint_queue_candidates.empty())
        {
            buffer_pool_->restoreCheckpointQueueState(
                page_summary.checkpoint_queue_candidates,
                checkpoint.dirty_generation_high_watermark);
        }

        StartupReconciliationSummary txn_summary{};
        status = transaction_manager_->load(&txn_summary, ctx);
        state.startup_repair = txn_summary.startup_repair;
        state.tip_active_to_aborted = txn_summary.tip_active_to_aborted;
        state.tip_active_to_prepared = txn_summary.tip_active_to_prepared;
        state.stale_prepared_records_removed = txn_summary.stale_prepared_records_removed;
        state.prepared_tip_without_catalog = txn_summary.prepared_tip_without_catalog;
        state.clog_states_synchronized = txn_summary.clog_states_synchronized;
        if (status != Status::OK)
        {
            state.outcome = StartupReconciliationOutcome::FAILED_TXN_RECONCILIATION;
            state.failure_status = status;
            finalizeStartupRecoveryClassification(
                &state, writeback_incident_open, writeback_degraded_state);
            ErrorContext persist_ctx;
            Status persist_status = persistStartupReconciliationState(state, &persist_ctx);
            if (persist_status != Status::OK)
            {
                LOG_WARNING(STORAGE,
                            "Failed to persist startup reconciliation transaction failure state: %d",
                            static_cast<int>(persist_status));
            }
            return status;
        }

        const bool has_tx_findings = startupStateHasTxnNormalizationWork(state);
        const bool has_checkpoint_queue_rebuild =
            state.checkpoint_queue_rebuild_required ||
            state.checkpoint_queue_rebuild_pages > 0;
        if (!state.clean_shutdown_marker && !state.has_page_scan_findings &&
            !has_tx_findings && !has_checkpoint_queue_rebuild)
        {
            state.outcome = StartupReconciliationOutcome::RECOVERY_WITH_FINDINGS;
        }
        else if (!state.clean_shutdown_marker || state.has_page_scan_findings ||
                 has_tx_findings || has_checkpoint_queue_rebuild)
        {
            state.outcome = state.clean_shutdown_marker
                ? StartupReconciliationOutcome::CLEAN_WITH_FINDINGS
                : StartupReconciliationOutcome::RECOVERY_WITH_FINDINGS;
        }
        else
        {
            state.outcome = StartupReconciliationOutcome::CLEAN;
        }

        classifyStartupCorruptionPolicy(&state);
        finalizeStartupRecoveryClassification(
            &state, writeback_incident_open, writeback_degraded_state);
        startup_quarantine_active_ = state.quarantine_active;
        if (state.corruption_class == Database::StartupCorruptionClass::STARTUP_REFUSAL)
        {
            state.outcome = StartupReconciliationOutcome::FAILED_CORRUPTION_POLICY;
            state.failure_status = Status::PAGE_CORRUPT;
            finalizeStartupRecoveryClassification(
                &state, writeback_incident_open, writeback_degraded_state);
            const std::string policy_message = buildStartupCorruptionPolicyMessage(state);
            Status persist_status = persistStartupReconciliationState(state, ctx);
            if (persist_status != Status::OK)
            {
                LOG_WARNING(STORAGE,
                            "Failed to persist startup corruption refusal state: %d",
                            static_cast<int>(persist_status));
            }
            startup_quarantine_active_ = false;
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, policy_message.c_str());
            return Status::PAGE_CORRUPT;
        }

        if (state.quarantine_active)
        {
            LOG_WARNING(STORAGE,
                        "Startup opened in read-only quarantine: %s",
                        buildStartupCorruptionPolicyMessage(state).c_str());
        }

        return persistStartupReconciliationState(state, ctx);
    }

    auto Database::markStartupOpen(ErrorContext *ctx) -> Status
    {
        if (buffer_pool_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool not initialized");
            return Status::INVALID_ARGUMENT;
        }

        void *page_buffer = nullptr;
        Status status = buffer_pool_->pinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *state_page = static_cast<BootstrapSystemStatePage *>(page_buffer);
        if (state_page->page_header.page_type != PAGE_TYPE_SYSTEM_STATE)
        {
            buffer_pool_->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid system state bootstrap page");
            return Status::PAGE_CORRUPT;
        }
        if (!validatePageChecksum(reinterpret_cast<uint8_t *>(state_page), page_size_))
        {
            buffer_pool_->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH,
                              "System state page checksum validation failed");
            return Status::CHECKSUM_MISMATCH;
        }

        CheckpointControlState checkpoint{};
        loadCheckpointControlState(*state_page, &checkpoint);
        WritebackIncidentControlState writeback_incident{};
        loadWritebackIncidentControlState(*state_page, &writeback_incident);

        const bool checkpoint_state_completed =
            checkpoint.checkpoint_state == CheckpointLifecycleState::IDLE ||
            checkpoint.checkpoint_state == CheckpointLifecycleState::COMPLETE;
        const bool checkpoint_clean_valid =
            checkpoint_state_completed &&
            checkpoint.checkpoint_failure_reason == Status::OK &&
            !checkpoint.queue_rebuild_required &&
            checkpoint.shutdown_intent == CheckpointShutdownIntent::CLEAN &&
            state_page->last_clean_shutdown_generation != 0 &&
            state_page->last_clean_shutdown_generation == checkpoint.checkpoint_generation;

        last_shutdown_was_clean_ =
            state_page->clean_shutdown != 0 && checkpoint_clean_valid;
        const uint64_t persisted_startup =
            state_page->startup_counter == 0 ? 1 : state_page->startup_counter;
        startup_generation_ = persisted_startup + 1;
        restart_generation_ = state_page->restart_generation;
        if (!last_shutdown_was_clean_)
        {
            ++restart_generation_;
        }
        last_clean_shutdown_generation_ = state_page->last_clean_shutdown_generation;
        startup_quarantine_active_ = false;
        {
            std::lock_guard<std::mutex> lock(writeback_failure_mutex_);
            writeback_incident_open_ = writeback_incident.incident_open;
            writeback_degraded_state_ = writeback_incident.degraded_state;
            write_admission_fenced_ =
                writeback_incident.incident_open &&
                writeback_incident.degraded_state == WritebackDegradedState::WRITE_FENCED;
            write_admission_failure_status_ = writeback_incident.incident_open &&
                                              writeback_incident.last_error_status != Status::OK
                ? writeback_incident.last_error_status
                : Status::OK;
        }

        buffer_pool_->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);

        checkpoint.shutdown_intent = CheckpointShutdownIntent::NONE;
        status = mutateAndFlushSystemStatePage(
            this,
            buffer_pool_.get(),
            page_size_,
            ctx,
            [&](BootstrapSystemStatePage *mutable_state_page) {
                mutable_state_page->clean_shutdown = 0;
                mutable_state_page->startup_counter = startup_generation_;
                mutable_state_page->restart_generation = restart_generation_;
                storeCheckpointControlState(mutable_state_page, checkpoint);
            });
        if (status != Status::OK)
        {
            return status;
        }

        startup_state_loaded_ = true;
        return Status::OK;
    }

    auto Database::markCleanShutdown(ErrorContext *ctx) -> Status
    {
        if (buffer_pool_ == nullptr || !startup_state_loaded_)
        {
            return Status::OK;
        }

        CheckpointControlState checkpoint{};
        checkpoint.checkpoint_generation = std::max<uint64_t>(startup_generation_, 1);
        checkpoint.checkpoint_start_time = defaultTimeSource().nowMicros();
        checkpoint.captured_oit = header_ != nullptr ? header_->oldest_transaction_id : 0;
        checkpoint.captured_oat = header_ != nullptr ? header_->oldest_active_xid : 0;
        checkpoint.captured_ost = header_ != nullptr ? header_->oldest_snapshot : 0;
        checkpoint.shutdown_intent = CheckpointShutdownIntent::CLEAN;
        checkpoint.checkpoint_failure_reason = Status::OK;
        checkpoint.queue_rebuild_required = false;
        const auto pre_checkpoint_stats = buffer_pool_->getStats();
        const uint64_t dirty_generation_boundary =
            buffer_pool_->currentDirtyGeneration();
        checkpoint.dirty_generation_low_watermark =
            pre_checkpoint_stats.dirty_generation_low_watermark == 0
            ? dirty_generation_boundary
            : pre_checkpoint_stats.dirty_generation_low_watermark;
        checkpoint.dirty_generation_high_watermark =
            std::max<uint64_t>(dirty_generation_boundary,
                               pre_checkpoint_stats.dirty_generation_high_watermark);
        checkpoint.captured_flush_debt_pages =
            std::max<uint64_t>(buffer_pool_->checkpointDebtCandidateCount(),
                               buffer_pool_->currentDirtyPageCount());
        const uint64_t checkpoint_flushes_before =
            buffer_pool_->getStats().checkpoint_flushes;
        CatalogManager::CheckpointRunCatalogInfo checkpoint_run{};
        checkpoint_run.checkpoint_generation = checkpoint.checkpoint_generation;
        checkpoint_run.checkpoint_state = CheckpointLifecycleState::CAPTURING_HORIZONS;
        checkpoint_run.start_time = checkpoint.checkpoint_start_time;
        checkpoint_run.dirty_generation_low_watermark =
            checkpoint.dirty_generation_low_watermark;
        checkpoint_run.pages_target = checkpoint.captured_flush_debt_pages;
        checkpoint_run.pages_flushed = 0;
        checkpoint_run.is_valid = true;
        auto persist_checkpoint_history =
            [&](CheckpointLifecycleState phase, Status failure_reason, bool terminal) {
                if (catalog_manager_ == nullptr)
                {
                    return;
                }
                checkpoint_run.checkpoint_state = phase;
                checkpoint_run.pages_flushed =
                    buffer_pool_->getStats().checkpoint_flushes - checkpoint_flushes_before;
                checkpoint_run.has_failure_reason = failure_reason != Status::OK;
                checkpoint_run.failure_reason = failure_reason;
                checkpoint_run.has_end_time = terminal;
                checkpoint_run.end_time = terminal ? defaultTimeSource().nowMicros() : 0;
                ErrorContext history_ctx;
                const Status history_status =
                    catalog_manager_->upsertCheckpointRunCatalogEntry(
                        checkpoint_run, &history_ctx);
                if (history_status != Status::OK)
                {
                    LOG_WARNING(STORAGE,
                                "Failed to persist checkpoint history row: %d (%s)",
                                static_cast<int>(history_status),
                                history_ctx.message.c_str());
                }
            };

        auto persist_checkpoint_phase = [&](CheckpointLifecycleState phase,
                                            bool clean_shutdown,
                                            Status failure_reason) -> Status {
            checkpoint.checkpoint_state = phase;
            checkpoint.checkpoint_failure_reason = failure_reason;
            checkpoint.queue_rebuild_required =
                (phase == CheckpointLifecycleState::FAILED) ||
                ((phase == CheckpointLifecycleState::CAPTURING_HORIZONS ||
                  phase == CheckpointLifecycleState::DRAINING_DIRTY_SET) &&
                 checkpoint.captured_flush_debt_pages > 0);

            return mutateAndFlushSystemStatePage(
                this,
                buffer_pool_.get(),
                page_size_,
                ctx,
                [&](BootstrapSystemStatePage *state_page) {
                    state_page->clean_shutdown = clean_shutdown ? 1 : 0;
                    state_page->startup_counter = startup_generation_;
                    state_page->restart_generation = restart_generation_;
                    if (clean_shutdown)
                    {
                        state_page->last_clean_shutdown_generation =
                            checkpoint.checkpoint_generation;
                    }
                    state_page->last_checkpoint_txid =
                        header_ != nullptr && header_->next_transaction_id > 0
                            ? header_->next_transaction_id - 1
                            : 0;
                    state_page->last_checkpoint_time = defaultTimeSource().nowMicros();
                    storeCheckpointControlState(state_page, checkpoint);
                });
        };

        persist_checkpoint_history(CheckpointLifecycleState::CAPTURING_HORIZONS,
                                   Status::OK,
                                   false);

        Status status = persist_checkpoint_phase(CheckpointLifecycleState::CAPTURING_HORIZONS,
                                                 false,
                                                 Status::OK);
        if (status != Status::OK)
        {
            return status;
        }

        status = persist_checkpoint_phase(CheckpointLifecycleState::DRAINING_DIRTY_SET,
                                          false,
                                          Status::OK);
        if (status != Status::OK)
        {
            return status;
        }
        status = buffer_pool_->flushDirtyCheckpointBoundary(dirty_generation_boundary, ctx);
        if (status != Status::OK)
        {
            (void)persist_checkpoint_phase(CheckpointLifecycleState::FAILED, false, status);
            persist_checkpoint_history(CheckpointLifecycleState::FAILED, status, true);
            return status;
        }

        WritebackAttribution attribution{};
        attribution.queue_kind = WritebackQueueKind::CHECKPOINT;
        attribution.policy_domain = WritebackPolicyDomain::CHECKPOINT;
        attribution.page_class = PAGE_TYPE_SYSTEM_STATE;
        status = sync(ctx, attribution);
        if (status != Status::OK)
        {
            (void)persist_checkpoint_phase(CheckpointLifecycleState::FAILED, false, status);
            persist_checkpoint_history(CheckpointLifecycleState::FAILED, status, true);
            return status;
        }

        status = persist_checkpoint_phase(CheckpointLifecycleState::PERSISTING_CHECKPOINT_MARKER,
                                          false,
                                          Status::OK);
        if (status != Status::OK)
        {
            return status;
        }

        status = persist_checkpoint_phase(CheckpointLifecycleState::CLEAN_SHUTDOWN_ARMED,
                                          true,
                                          Status::OK);
        if (status != Status::OK)
        {
            return status;
        }

        status = persist_checkpoint_phase(CheckpointLifecycleState::COMPLETE,
                                          true,
                                          Status::OK);
        if (status != Status::OK)
        {
            return status;
        }
        persist_checkpoint_history(CheckpointLifecycleState::COMPLETE, Status::OK, true);

        status = persist_checkpoint_phase(CheckpointLifecycleState::IDLE, true, Status::OK);
        if (status != Status::OK)
        {
            return status;
        }

        last_shutdown_was_clean_ = true;
        last_clean_shutdown_generation_ = checkpoint.checkpoint_generation;
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

        const Status format_status =
            validateDatabaseFormatCompatibility(header_->db_version,
                                                header_->db_compat_version,
                                                ctx);
        if (format_status != Status::OK)
        {
            return format_status;
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

    auto Database::write_page(uint32_t page_id,
                              const void *buffer,
                              ErrorContext *ctx,
                              WritebackAttribution attribution) -> Status
    {
        if (fd_ < 0 || (buffer == nullptr))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to write_page");
            return Status::INVALID_ARGUMENT;
        }

        attribution = finalizeWritebackAttribution(buffer, attribution, 0);

        MgaFailpointManager *failpoints = mga_failpoint_manager_.get();
        if (failpoints != nullptr)
        {
            Status failpoint_status = failpoints->trip(
                MgaFailpointTriggers::kWritebackPageWriteFailure,
                {},
                ctx);
            if (failpoint_status != Status::OK)
            {
                noteWritebackFailure(failpoint_status,
                                     classifyWritebackFailure(failpoint_status, 0, false),
                                     attribution,
                                     false,
                                     ctx);
                return failpoint_status;
            }
        }

        // AUDIT CONTRACT:
        // Finalize the canonical page contract before durable write. The primary
        // page image is authoritative MGA truth; shadow mirroring below is a
        // derivative copy of that already-produced truth, not a recovery log.
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
            const Status failure_status = (errno == ENOSPC) ? Status::DISK_FULL : Status::IO_ERROR;
            SET_ERROR_CONTEXT(ctx, failure_status, msg);
            noteWritebackFailure(failure_status,
                                 classifyWritebackFailure(failure_status, errno, false),
                                 attribution,
                                 false,
                                 ctx);
            return failure_status;
        }

        Status mirror_status =
            mirrorShadowFilespaceWrite(PRIMARY_TABLESPACE_ID,
                                       static_cast<uint64_t>(offset),
                                       buffer,
                                       page_size_,
                                       ctx);
        if (mirror_status != Status::OK)
        {
            return mirror_status;
        }

        return Status::OK;
    }

    auto Database::sync(ErrorContext *ctx, WritebackAttribution attribution) const -> Status
    {
        // AUDIT CONTRACT:
        // sync() is the engine-wide forced-write fence. When it returns OK, the
        // primary database file, every registered durable tablespace, and every
        // active shadow filespace participating in durability have been forced to
        // stable storage. This is conservative MGA publication, not WAL replay.
        if (fd_ < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
            return Status::INVALID_ARGUMENT;
        }

        MgaFailpointManager *failpoints = mga_failpoint_manager_.get();
        if (failpoints != nullptr)
        {
            Status failpoint_status = failpoints->trip(
                MgaFailpointTriggers::kWritebackSyncFailure,
                {},
                ctx);
            if (failpoint_status != Status::OK)
            {
                const_cast<Database *>(this)->noteWritebackFailure(
                    failpoint_status,
                    classifyWritebackFailure(failpoint_status, 0, true),
                    attribution,
                    true,
                    ctx);
                return failpoint_status;
            }
        }

        auto sync_target = [&](int fd,
                               uint16_t filespace_id,
                               const char *target_name) -> Status
        {
            errno = 0;
            if (platform::syncFd(fd) == 0)
            {
                return Status::OK;
            }

            const int sync_errno = errno;
            const Status failure_status =
                (sync_errno == ENOSPC) ? Status::DISK_FULL : Status::IO_ERROR;
            if (sync_errno == ENOSPC)
            {
                if (filespace_id == PRIMARY_TABLESPACE_ID)
                {
                    SET_ERROR_CONTEXT(ctx,
                                      failure_status,
                                      "Failed to sync database file: disk full");
                }
                else
                {
                    char msg[256];
                    snprintf(msg,
                             sizeof(msg),
                             "Failed to sync %s %u: disk full",
                             target_name,
                             filespace_id);
                    SET_ERROR_CONTEXT(ctx, failure_status, msg);
                }
            }
            else
            {
                if (filespace_id == PRIMARY_TABLESPACE_ID)
                {
                    SET_ERROR_CONTEXT(ctx, failure_status, "Failed to sync database file");
                }
                else
                {
                    char msg[256];
                    snprintf(msg,
                             sizeof(msg),
                             "Failed to sync %s %u",
                             target_name,
                             filespace_id);
                    SET_ERROR_CONTEXT(ctx, failure_status, msg);
                }
            }

            WritebackAttribution failure_attribution = attribution;
            failure_attribution.filespace_id = filespace_id;
            const_cast<Database *>(this)->noteWritebackFailure(
                failure_status,
                classifyWritebackFailure(failure_status, sync_errno, true),
                failure_attribution,
                true,
                ctx);
            return failure_status;
        };

        if (Status status = sync_target(fd_, PRIMARY_TABLESPACE_ID, "database file");
            status != Status::OK)
        {
            return status;
        }
        if (Status status = syncShadowFilespacesForSource(PRIMARY_TABLESPACE_ID, ctx);
            status != Status::OK)
        {
            return status;
        }

        // Firebird-style forced-write durability requires the durable fence to
        // reach every registered filespace, not only the primary database file.
        // Until touched-filespace tracking is wired into the foreground commit
        // path, Alpha uses the conservative fence and syncs every registered
        // tablespace descriptor here.
        std::vector<std::pair<uint16_t, int>> registered_tablespaces;
        {
            std::lock_guard<std::mutex> lock(tablespace_mutex_);
            registered_tablespaces.reserve(tablespace_fds_.size());
            for (const auto& [tablespace_id, tablespace_fd] : tablespace_fds_)
            {
                registered_tablespaces.emplace_back(tablespace_id, tablespace_fd);
            }
        }
        for (const auto &[tablespace_id, tablespace_fd] : registered_tablespaces)
        {
            if (tablespace_fd < 0)
            {
                continue;
            }

            if (Status status = sync_target(tablespace_fd, tablespace_id, "tablespace");
                status != Status::OK)
            {
                return status;
            }
            if (Status status = syncShadowFilespacesForSource(tablespace_id, ctx);
                status != Status::OK)
            {
                return status;
            }
        }

        // Resident frames are only clean after the engine-wide forced-write fence
        // completes. The page image was written earlier; this clears the
        // MGA-local `DirtyFlushedPendingFsync` staging state after fsync.
        if (buffer_pool_ != nullptr)
        {
            buffer_pool_->completeFsyncFence();
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

    auto Database::write_page_global(GPID gpid,
                                     const void *buffer,
                                     ErrorContext *ctx,
                                     WritebackAttribution attribution) -> Status
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

        attribution = finalizeWritebackAttribution(buffer, attribution, tablespace_id);

        MgaFailpointManager *failpoints = mga_failpoint_manager_.get();
        if (failpoints != nullptr)
        {
            Status failpoint_status = failpoints->trip(
                MgaFailpointTriggers::kWritebackPageWriteFailure,
                {},
                ctx);
            if (failpoint_status != Status::OK)
            {
                noteWritebackFailure(failpoint_status,
                                     classifyWritebackFailure(failpoint_status, 0, false),
                                     attribution,
                                     false,
                                     ctx);
                return failpoint_status;
            }
        }

        // Calculate offset in file (page_number * page_size_)
        off_t offset = static_cast<off_t>(page_number) * static_cast<off_t>(page_size_);

        // Finalize page header contract before durable write for both primary and custom
        // tablespaces so GPID-based buffer-pool flushes publish canonical checksum state.
        auto *page = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(buffer));
        preparePageForWrite(page, page_size_, page_number);

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
            const Status failure_status = (errno == ENOSPC) ? Status::DISK_FULL : Status::IO_ERROR;
            SET_ERROR_CONTEXT(ctx, failure_status, msg);
            noteWritebackFailure(failure_status,
                                 classifyWritebackFailure(failure_status, errno, false),
                                 attribution,
                                 false,
                                 ctx);
            return failure_status;
        }

        Status mirror_status =
            mirrorShadowFilespaceWrite(tablespace_id,
                                       static_cast<uint64_t>(offset),
                                       buffer,
                                       page_size_,
                                       ctx);
        if (mirror_status != Status::OK)
        {
            return mirror_status;
        }

        return Status::OK;
    }

    auto Database::write_free_page_image_global(GPID gpid,
                                                const void *buffer,
                                                ErrorContext *ctx,
                                                WritebackAttribution attribution) -> Status
    {
        if (buffer == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        int tablespace_fd = -1;
        std::string unused_path;
        const uint16_t tablespace_id = getTablespaceID(gpid);
        const uint64_t page_number = getPageNumber(gpid);
        Status route_status = resolveFilespaceRoute(tablespace_id,
                                                    &tablespace_fd,
                                                    &unused_path,
                                                    ctx);
        if (route_status != Status::OK)
        {
            return route_status;
        }

        attribution.filespace_id =
            attribution.filespace_id == 0 ? tablespace_id : attribution.filespace_id;

        MgaFailpointManager *failpoints = mga_failpoint_manager_.get();
        if (failpoints != nullptr)
        {
            Status failpoint_status = failpoints->trip(
                MgaFailpointTriggers::kWritebackPageWriteFailure,
                {},
                ctx);
            if (failpoint_status != Status::OK)
            {
                noteWritebackFailure(failpoint_status,
                                     classifyWritebackFailure(failpoint_status, 0, false),
                                     attribution,
                                     false,
                                     ctx);
                return failpoint_status;
            }
        }

        const off_t offset = static_cast<off_t>(page_number) * static_cast<off_t>(page_size_);
        size_t bytes_written = 0;
        errno = 0;
        if (!pwriteFully(tablespace_fd, buffer, page_size_, offset, &bytes_written))
        {
            char msg[256];
            if (errno != 0)
            {
                snprintf(msg, sizeof(msg),
                         "Write failed for free page image %lu in tablespace %u after %zu/%u bytes: %s",
                         static_cast<unsigned long>(page_number),
                         static_cast<unsigned int>(tablespace_id),
                         bytes_written,
                         page_size_,
                         std::strerror(errno));
            }
            else
            {
                snprintf(msg,
                         sizeof(msg),
                         "Short write on free page image (%zu/%u bytes)",
                         bytes_written,
                         page_size_);
            }
            const Status failure_status = (errno == ENOSPC) ? Status::DISK_FULL : Status::IO_ERROR;
            SET_ERROR_CONTEXT(ctx, failure_status, msg);
            noteWritebackFailure(failure_status,
                                 classifyWritebackFailure(failure_status, errno, false),
                                 attribution,
                                 false,
                                 ctx);
            return failure_status;
        }

        return mirrorShadowFilespaceWrite(tablespace_id,
                                          static_cast<uint64_t>(offset),
                                          buffer,
                                          page_size_,
                                          ctx);
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

    auto Database::set_cluster_identity(const ID &cluster_id,
                                        const ID &node_id,
                                        uint64_t cluster_config_epoch,
                                        ErrorContext *ctx) -> Status
    {
        if (fd_ < 0 || !is_open())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
            return Status::INVALID_ARGUMENT;
        }

        if (header_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database header not loaded");
            return Status::INVALID_ARGUMENT;
        }

        header_->cluster_id = cluster_id;
        header_->node_id = node_id;
        header_->cluster_config_epoch = cluster_config_epoch;
        header_->page_header.flags |= PAGE_FLAG_CHECKSUM_VALID;
        header_->page_header.checksum =
            calculatePageChecksum(reinterpret_cast<uint8_t *>(header_), page_size_);

        if (buffer_pool_ != nullptr)
        {
            void *header_buffer = nullptr;
            const Status status = buffer_pool_->pinPage(0, &header_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
            db_header->cluster_id = cluster_id;
            db_header->node_id = node_id;
            db_header->cluster_config_epoch = cluster_config_epoch;
            db_header->page_header.flags |= PAGE_FLAG_CHECKSUM_VALID;
            db_header->page_header.checksum =
                calculatePageChecksum(reinterpret_cast<uint8_t *>(db_header), page_size_);
            buffer_pool_->unpinPage(0, true, ctx);
        }

        return sync(ctx);
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

    Status Database::resolveFilespaceRoute(uint16_t source_tablespace_id,
                                           int* fd_out,
                                           std::string* path_out,
                                           ErrorContext* ctx) const
    {
        if (source_tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            if (fd_ < 0 || path_.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Primary database filespace is not open");
                return Status::INVALID_ARGUMENT;
            }
            if (fd_out != nullptr)
            {
                *fd_out = fd_;
            }
            if (path_out != nullptr)
            {
                *path_out = path_;
            }
            return Status::OK;
        }

        int tablespace_fd = -1;
        {
            std::lock_guard<std::mutex> lock(tablespace_mutex_);
            auto it = tablespace_fds_.find(source_tablespace_id);
            if (it == tablespace_fds_.end() || it->second < 0)
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::NOT_FOUND,
                                  ("Tablespace " + std::to_string(source_tablespace_id) +
                                   " is not registered").c_str());
                return Status::NOT_FOUND;
            }
            tablespace_fd = it->second;
        }

        if (fd_out != nullptr)
        {
            *fd_out = tablespace_fd;
        }

        if (path_out != nullptr)
        {
            {
                std::lock_guard<std::mutex> shadow_lock(shadow_filespace_mutex_);
                const auto shadow_range =
                    shadow_filespace_by_source_.equal_range(source_tablespace_id);
                for (auto shadow_it = shadow_range.first;
                     shadow_it != shadow_range.second;
                     ++shadow_it)
                {
                    const auto entry_it = shadow_filespaces_.find(shadow_it->second);
                    if (entry_it == shadow_filespaces_.end())
                    {
                        continue;
                    }
                    const auto& entry = entry_it->second;
                    if (entry.snapshot.promoted && !entry.snapshot.shadow_path.empty())
                    {
                        *path_out = entry.snapshot.shadow_path;
                        return Status::OK;
                    }
                }
            }

            if (catalog_manager_ == nullptr)
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::INVALID_ARGUMENT,
                                  "Catalog manager unavailable for tablespace route lookup");
                return Status::INVALID_ARGUMENT;
            }

            TablespaceInfo info{};
            Status status = catalog_manager_->getTablespace(source_tablespace_id, info, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            if (info.file_paths.empty())
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::NOT_FOUND,
                                  "Tablespace route is missing file_paths");
                return Status::NOT_FOUND;
            }
            *path_out = info.file_paths.front();
        }

        return Status::OK;
    }

    Status Database::backfillShadowFilespace(ShadowFilespaceEntry& entry,
                                             ErrorContext* ctx)
    {
        int source_fd = -1;
        std::string source_path;
        Status status = resolveFilespaceRoute(entry.snapshot.source_tablespace_id,
                                              &source_fd,
                                              &source_path,
                                              ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::error_code ec;
        const auto source_size = std::filesystem::file_size(source_path, ec);
        if (ec)
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::IO_ERROR,
                              "Failed to inspect source filespace before shadow backfill");
            return Status::IO_ERROR;
        }

        entry.snapshot.source_path = source_path;
        entry.snapshot.copied_pages = 0;
        entry.snapshot.last_sync_time = 0;

        if (source_size == 0)
        {
            if (platform::syncFd(entry.fd) != 0)
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::IO_ERROR,
                                  "Failed to sync empty shadow filespace after create");
                return Status::IO_ERROR;
            }
            entry.snapshot.last_sync_time = defaultTimeSource().nowMicros();
            return Status::OK;
        }

        std::vector<uint8_t> copy_buffer(page_size_);
        uint64_t copied_pages = 0;
        for (uint64_t offset = 0; offset < source_size; offset += page_size_)
        {
            const size_t chunk_size = static_cast<size_t>(
                std::min<uint64_t>(page_size_, source_size - offset));
            size_t bytes_read = 0;
            errno = 0;
            if (!preadFully(source_fd, copy_buffer.data(), chunk_size,
                            static_cast<off_t>(offset), &bytes_read) ||
                bytes_read != chunk_size)
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::IO_ERROR,
                                  "Failed to read source filespace during shadow backfill");
                return Status::IO_ERROR;
            }

            size_t bytes_written = 0;
            errno = 0;
            if (!pwriteFully(entry.fd, copy_buffer.data(), chunk_size,
                             static_cast<off_t>(offset), &bytes_written) ||
                bytes_written != chunk_size)
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::IO_ERROR,
                                  "Failed to write shadow filespace during backfill");
                return Status::IO_ERROR;
            }
            ++copied_pages;
        }

        if (platform::syncFd(entry.fd) != 0)
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::IO_ERROR,
                              "Failed to sync shadow filespace after backfill");
            return Status::IO_ERROR;
        }

        entry.snapshot.copied_pages = copied_pages;
        entry.snapshot.last_sync_time = defaultTimeSource().nowMicros();
        return Status::OK;
    }

    Status Database::mirrorShadowFilespaceWrite(uint16_t source_tablespace_id,
                                                uint64_t offset,
                                                const void* buffer,
                                                size_t size,
                                                ErrorContext* ctx) const
    {
        // Derivative page-for-page copy only. Failure blocks the caller because the
        // configured shadow durability contract was not met, but the main recovery
        // authority remains the primary MGA page state.
        if (buffer == nullptr || size == 0)
        {
            return Status::OK;
        }

        auto* self = const_cast<Database*>(this);
        std::lock_guard<std::mutex> lock(self->shadow_filespace_mutex_);
        const auto range = self->shadow_filespace_by_source_.equal_range(source_tablespace_id);
        for (auto it = range.first; it != range.second; ++it)
        {
            auto entry_it = self->shadow_filespaces_.find(it->second);
            if (entry_it == self->shadow_filespaces_.end())
            {
                continue;
            }

            auto& entry = entry_it->second;
            if (!entry.snapshot.active || entry.snapshot.promoted || entry.fd < 0)
            {
                continue;
            }

            size_t bytes_written = 0;
            errno = 0;
            if (!pwriteFully(entry.fd, buffer, size, static_cast<off_t>(offset), &bytes_written) ||
                bytes_written != size)
            {
                const Status failure_status =
                    (errno == ENOSPC) ? Status::DISK_FULL : Status::IO_ERROR;
                SET_ERROR_CONTEXT(ctx,
                                  failure_status,
                                  "Failed to mirror write into shadow filespace");
                return failure_status;
            }

            entry.snapshot.mirrored_writes += 1;
            entry.snapshot.last_sync_time = defaultTimeSource().nowMicros();
        }

        return Status::OK;
    }

    Status Database::syncShadowFilespacesForSource(uint16_t source_tablespace_id,
                                                   ErrorContext* ctx) const
    {
        auto* self = const_cast<Database*>(this);
        std::lock_guard<std::mutex> lock(self->shadow_filespace_mutex_);
        const auto range = self->shadow_filespace_by_source_.equal_range(source_tablespace_id);
        for (auto it = range.first; it != range.second; ++it)
        {
            auto entry_it = self->shadow_filespaces_.find(it->second);
            if (entry_it == self->shadow_filespaces_.end())
            {
                continue;
            }

            auto& entry = entry_it->second;
            if (!entry.snapshot.active || entry.snapshot.promoted || entry.fd < 0)
            {
                continue;
            }

            errno = 0;
            if (platform::syncFd(entry.fd) != 0)
            {
                const Status failure_status =
                    (errno == ENOSPC) ? Status::DISK_FULL : Status::IO_ERROR;
                SET_ERROR_CONTEXT(ctx,
                                  failure_status,
                                  "Failed to sync shadow filespace");
                return failure_status;
            }
            entry.snapshot.last_sync_time = defaultTimeSource().nowMicros();
        }

        return Status::OK;
    }

    Status Database::createShadowFilespace(uint16_t source_tablespace_id,
                                           const std::string& shadow_path,
                                           ID* shadow_id_out,
                                           ErrorContext* ctx)
    {
        if (shadow_path.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Shadow filespace path is required");
            return Status::INVALID_ARGUMENT;
        }
        if (page_size_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is not open");
            return Status::INVALID_ARGUMENT;
        }

        std::string source_path;
        Status status = resolveFilespaceRoute(source_tablespace_id, nullptr, &source_path, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::error_code ec;
        const std::filesystem::path normalized_shadow =
            std::filesystem::absolute(std::filesystem::path(shadow_path), ec);
        if (ec)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid shadow filespace path");
            return Status::INVALID_ARGUMENT;
        }
        if (normalized_shadow.string() == source_path)
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::INVALID_ARGUMENT,
                              "Shadow filespace path must differ from the source filespace");
            return Status::INVALID_ARGUMENT;
        }
        if (normalized_shadow.has_parent_path())
        {
            std::filesystem::create_directories(normalized_shadow.parent_path(), ec);
            if (ec)
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::IO_ERROR,
                                  "Failed to create shadow filespace directory");
                return Status::IO_ERROR;
            }
        }

        int shadow_fd = platform::openFd(normalized_shadow.string().c_str(),
                                         O_RDWR | O_CREAT | O_EXCL,
                                         0644);
        if (shadow_fd < 0)
        {
            const Status failure_status =
                (errno == EEXIST) ? Status::FILE_EXISTS : Status::IO_ERROR;
            SET_ERROR_CONTEXT(ctx, failure_status, "Failed to create shadow filespace");
            return failure_status;
        }

        ShadowFilespaceEntry entry{};
        entry.snapshot.shadow_id = generateUuidV7();
        entry.snapshot.source_tablespace_id = source_tablespace_id;
        entry.snapshot.source_path = source_path;
        entry.snapshot.shadow_path = normalized_shadow.string();
        entry.snapshot.created_time = defaultTimeSource().nowMicros();
        entry.snapshot.active = true;
        entry.snapshot.promoted = false;
        entry.fd = shadow_fd;
        const ID created_shadow_id = entry.snapshot.shadow_id;

        {
            std::lock_guard<std::mutex> lock(shadow_filespace_mutex_);
            for (const auto& [existing_id, existing] : shadow_filespaces_)
            {
                (void) existing_id;
                if (existing.snapshot.shadow_path == entry.snapshot.shadow_path)
                {
                    ::close(shadow_fd);
                    std::filesystem::remove(normalized_shadow, ec);
                    SET_ERROR_CONTEXT(ctx,
                                      Status::FILE_EXISTS,
                                      "Shadow filespace path is already registered");
                    return Status::FILE_EXISTS;
                }
            }

        }

        status = backfillShadowFilespace(entry, ctx);
        if (status != Status::OK)
        {
            ::close(shadow_fd);
            std::filesystem::remove(normalized_shadow, ec);
            return status;
        }

        {
            std::lock_guard<std::mutex> lock(shadow_filespace_mutex_);
            for (const auto& [existing_id, existing] : shadow_filespaces_)
            {
                (void) existing_id;
                if (existing.snapshot.shadow_path == entry.snapshot.shadow_path)
                {
                    ::close(shadow_fd);
                    std::filesystem::remove(normalized_shadow, ec);
                    SET_ERROR_CONTEXT(ctx,
                                      Status::FILE_EXISTS,
                                      "Shadow filespace path is already registered");
                    return Status::FILE_EXISTS;
                }
            }

            shadow_filespace_by_source_.emplace(source_tablespace_id, entry.snapshot.shadow_id);
            shadow_filespaces_.emplace(entry.snapshot.shadow_id, std::move(entry));
        }

        if (shadow_id_out != nullptr)
        {
            *shadow_id_out = created_shadow_id;
        }
        return Status::OK;
    }

    Status Database::dropShadowFilespace(const ID& shadow_id,
                                         bool keep_file,
                                         ErrorContext* ctx)
    {
        ShadowFilespaceEntry removed{};
        {
            std::lock_guard<std::mutex> lock(shadow_filespace_mutex_);
            auto it = shadow_filespaces_.find(shadow_id);
            if (it == shadow_filespaces_.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Shadow filespace not found");
                return Status::NOT_FOUND;
            }
            if (it->second.snapshot.promoted)
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::CONSTRAINT_VIOLATION,
                                  "Cannot drop a promoted shadow filespace while it is the live route");
                return Status::CONSTRAINT_VIOLATION;
            }

            removed = it->second;
            auto range = shadow_filespace_by_source_.equal_range(
                removed.snapshot.source_tablespace_id);
            for (auto src_it = range.first; src_it != range.second; )
            {
                if (src_it->second == shadow_id)
                {
                    src_it = shadow_filespace_by_source_.erase(src_it);
                }
                else
                {
                    ++src_it;
                }
            }
            shadow_filespaces_.erase(it);
        }

        if (removed.fd >= 0)
        {
            ::close(removed.fd);
        }
        if (!keep_file)
        {
            std::error_code ec;
            std::filesystem::remove(removed.snapshot.shadow_path, ec);
            if (ec)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to remove shadow filespace");
                return Status::IO_ERROR;
            }
        }
        return Status::OK;
    }

    Status Database::promoteShadowFilespace(const ID& shadow_id,
                                            ErrorContext* ctx)
    {
        ShadowFilespaceSnapshot snapshot{};
        {
            std::lock_guard<std::mutex> lock(shadow_filespace_mutex_);
            auto it = shadow_filespaces_.find(shadow_id);
            if (it == shadow_filespaces_.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Shadow filespace not found");
                return Status::NOT_FOUND;
            }
            if (it->second.snapshot.promoted)
            {
                return Status::OK;
            }
            it->second.snapshot.active = false;
            snapshot = it->second.snapshot;
        }

        int replacement_fd = platform::openFd(snapshot.shadow_path.c_str(), O_RDWR);
        if (replacement_fd < 0)
        {
            std::lock_guard<std::mutex> relock(shadow_filespace_mutex_);
            auto it = shadow_filespaces_.find(shadow_id);
            if (it != shadow_filespaces_.end())
            {
                it->second.snapshot.active = true;
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to reopen shadow filespace for promotion");
            return Status::IO_ERROR;
        }

        if (snapshot.source_tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            if (fd_ >= 0)
            {
                ::close(fd_);
            }
            fd_ = replacement_fd;
            path_ = snapshot.shadow_path;
        }
        else
        {
            std::lock_guard<std::mutex> lock(tablespace_mutex_);
            auto it = tablespace_fds_.find(snapshot.source_tablespace_id);
            if (it == tablespace_fds_.end())
            {
                ::close(replacement_fd);
                std::lock_guard<std::mutex> relock(shadow_filespace_mutex_);
                auto shadow_it = shadow_filespaces_.find(shadow_id);
                if (shadow_it != shadow_filespaces_.end())
                {
                    shadow_it->second.snapshot.active = true;
                }
                SET_ERROR_CONTEXT(ctx,
                                  Status::NOT_FOUND,
                                  "Source tablespace route not found during shadow promotion");
                return Status::NOT_FOUND;
            }

            if (it->second >= 0)
            {
                ::close(it->second);
            }
            it->second = replacement_fd;
        }

        std::lock_guard<std::mutex> relock(shadow_filespace_mutex_);
        auto it = shadow_filespaces_.find(shadow_id);
        if (it == shadow_filespaces_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Shadow filespace disappeared during promotion");
            return Status::NOT_FOUND;
        }
        if (it->second.fd >= 0)
        {
            ::close(it->second.fd);
            it->second.fd = -1;
        }
        it->second.snapshot.promoted = true;
        it->second.snapshot.active = false;
        it->second.snapshot.last_sync_time = defaultTimeSource().nowMicros();
        return Status::OK;
    }

    Status Database::listShadowFilespaces(std::vector<ShadowFilespaceSnapshot>& shadows_out,
                                          ErrorContext* ctx) const
    {
        (void) ctx;
        shadows_out.clear();
        std::lock_guard<std::mutex> lock(shadow_filespace_mutex_);
        shadows_out.reserve(shadow_filespaces_.size());
        for (const auto& [shadow_id, entry] : shadow_filespaces_)
        {
            (void) shadow_id;
            shadows_out.push_back(entry.snapshot);
        }
        std::sort(shadows_out.begin(),
                  shadows_out.end(),
                  [](const ShadowFilespaceSnapshot& left,
                     const ShadowFilespaceSnapshot& right) {
                      if (left.source_tablespace_id != right.source_tablespace_id)
                      {
                          return left.source_tablespace_id < right.source_tablespace_id;
                      }
                      return left.created_time < right.created_time;
                  });
        return Status::OK;
    }

} // namespace scratchbird::core
