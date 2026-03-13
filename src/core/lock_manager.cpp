/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/logger.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace scratchbird::core
{
    namespace
    {
        auto lockModeName(LockMode mode) -> const char *
        {
            switch (mode)
            {
                case LockMode::LOCK_ACCESS_SHARE: return "ACCESS_SHARE";
                case LockMode::LOCK_ROW_SHARE: return "ROW_SHARE";
                case LockMode::LOCK_ROW_EXCLUSIVE: return "ROW_EXCLUSIVE";
                case LockMode::LOCK_SHARE_UPDATE_EXCLUSIVE: return "SHARE_UPDATE_EXCLUSIVE";
                case LockMode::LOCK_SHARE: return "SHARE";
                case LockMode::LOCK_SHARE_ROW_EXCLUSIVE: return "SHARE_ROW_EXCLUSIVE";
                case LockMode::LOCK_EXCLUSIVE: return "EXCLUSIVE";
                case LockMode::LOCK_ACCESS_EXCLUSIVE: return "ACCESS_EXCLUSIVE";
            }
            return "UNKNOWN";
        }

        auto lockTargetName(LockTarget target) -> const char *
        {
            switch (target)
            {
                case LockTarget::LOCK_TARGET_DATABASE: return "database";
                case LockTarget::LOCK_TARGET_TABLE: return "table";
                case LockTarget::LOCK_TARGET_PAGE: return "page";
                case LockTarget::LOCK_TARGET_TUPLE: return "tuple";
            }
            return "unknown";
        }

        auto formatLockResourceId(const LockTag &tag) -> std::string
        {
            std::ostringstream oss;
            oss << tag.object_uuid.toString() << ":" << tag.page_num << ":" << tag.offset_num;
            return oss.str();
        }

        auto isZeroIdLocal(const ID& id) -> bool
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

        bool lockModesConflictForTag(const LockTag &tag, LockMode held_mode, LockMode requested_mode,
                                     const bool conflict_matrix[8][8])
        {
            if (tag.target_type == LockTarget::LOCK_TARGET_TUPLE)
            {
                return true;
            }

            const uint8_t held_idx = static_cast<uint8_t>(held_mode) - 1;
            const uint8_t req_idx = static_cast<uint8_t>(requested_mode) - 1;
            return conflict_matrix[held_idx][req_idx];
        }

        void recordLockWaitHistory(Database *db, uint32_t waiter_proc_id,
                                   uint32_t blocker_proc_id, const LockTag &tag,
                                   LockMode requested_mode, LockMode blocker_mode,
                                   uint64_t timer_start, uint64_t timer_end, bool timed_out,
                                   const char *outcome_code, const char *victim_reason_code,
                                   bool retry_eligible)
        {
            if (db == nullptr || db->catalog_manager() == nullptr)
            {
                return;
            }

            CatalogManager::WaitHistoryEntry entry;
            entry.thread_id = waiter_proc_id;
            entry.blocker_thread_id = blocker_proc_id;
            entry.event_id = timer_start != 0 ? timer_start : waiter_proc_id;
            entry.timer_start = timer_start;
            entry.timer_end = timer_end;
            entry.timer_wait = (timer_start != 0 && timer_end >= timer_start)
                ? (timer_end - timer_start)
                : 0;
            entry.object_instance_begin = timer_start;
            entry.resource_class = lockTargetName(tag.target_type);
            entry.resource_id = formatLockResourceId(tag);
            entry.requested_mode = static_cast<uint8_t>(requested_mode);
            entry.blocker_mode = static_cast<uint8_t>(blocker_mode);
            entry.outcome_code = outcome_code != nullptr ? outcome_code : "";
            entry.victim_reason_code = victim_reason_code != nullptr ? victim_reason_code : "";
            entry.retry_eligible = retry_eligible;
            entry.timed_out = timed_out;

            const std::vector<Database::ConnectionIoSnapshot> snapshots =
                db->snapshotConnectionIoStats();
            for (const Database::ConnectionIoSnapshot& snapshot : snapshots)
            {
                if (snapshot.proc_id == waiter_proc_id)
                {
                    if (snapshot.transaction_id != 0)
                    {
                        entry.has_victim_txid = true;
                        entry.victim_txid = snapshot.transaction_id;
                    }
                    if (!isZeroIdLocal(snapshot.session_id))
                    {
                        entry.victim_identity = snapshot.session_id.toString();
                    }
                }
                if (snapshot.proc_id == blocker_proc_id)
                {
                    if (snapshot.transaction_id != 0)
                    {
                        entry.has_blocker_txid = true;
                        entry.blocker_txid = snapshot.transaction_id;
                    }
                    if (!isZeroIdLocal(snapshot.session_id))
                    {
                        entry.blocker_identity = snapshot.session_id.toString();
                    }
                }
            }
            db->catalog_manager()->recordWaitHistory(entry, nullptr);
        }
    } // namespace

    // Lock conflict matrix [held_mode][requested_mode]
    // true = conflict (must wait), false = no conflict (can grant)
    // Modes are 1-indexed (LOCK_ACCESS_SHARE=1), so subtract 1 for array index
    const bool LockManager::conflict_matrix_[8][8] = {
        //     AS  RS  RE  SUE  S  SRE  E  AE
        /* AS */ {0, 0, 0, 0, 0, 0, 0, 1},
        /* RS */ {0, 0, 0, 0, 0, 0, 1, 1},
        /* RE */ {0, 0, 0, 0, 1, 1, 1, 1},
        /* SUE*/ {0, 0, 0, 1, 1, 1, 1, 1},
        /* S  */ {0, 0, 1, 1, 0, 1, 1, 1},
        /* SRE*/ {0, 0, 1, 1, 1, 1, 1, 1},
        /* E  */ {0, 1, 1, 1, 1, 1, 1, 1},
        /* AE */ {1, 1, 1, 1, 1, 1, 1, 1}};

    LockManager::LockManager(Database *db)
        : db_(db), max_locks_(config::DEFAULT_MAX_LOCKS),
          deadlock_timeout_ms_(config::DEFAULT_DEADLOCK_TIMEOUT_MS)
    {
        std::memset(&stats_, 0, sizeof(stats_));
    }

    LockManager::~LockManager()
    {
        shutdown(nullptr);
    }

    auto LockManager::initialize(ErrorContext *ctx) -> Status
    {
        if (!db_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is null");
            return Status::INVALID_ARGUMENT;
        }

        // Create deadlock detector
        try
        {
            deadlock_detector_ = std::make_unique<DeadlockDetector>(this);
        }
        catch (const std::bad_alloc &)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate DeadlockDetector");
            return Status::OOM;
        }

        return Status::OK;
    }

    auto LockManager::shutdown(ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);

        // RAII: unique_ptr automatically cleans up Lock objects and their wait_queues
        lock_table_.clear();
        proc_locks_.clear();

        deadlock_detector_.reset();

        return Status::OK;
    }

    auto LockManager::acquireLock(uint32_t proc_id, const LockTag &tag, LockMode mode, bool wait,
                                  uint32_t timeout_ms, ErrorContext *ctx) -> Status
    {
        // OPTIMIZATION: Check if this is a read-only transaction
        // Read-only transactions can benefit from fast-path lock acquisition
        bool is_readonly_txn = isReadOnlyTransaction(proc_id);

        std::unique_lock<std::mutex> lock(lock_table_mutex_);

        // Get or create lock object
        Lock *lock_obj = findOrCreateLock(tag);
        if (!lock_obj)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate lock");
            return Status::OOM;
        }

        uint8_t mode_idx = static_cast<uint8_t>(mode) - 1;
        if (mode_idx >= 8)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid lock mode");
            return Status::INVALID_ARGUMENT;
        }

        // NOTE: Removed buggy optimization that allowed ANY proc_id to increment
        // granted_counts if the mode was already granted. This was incorrect for
        // self-conflicting modes (SHARE_ROW_EXCLUSIVE, EXCLUSIVE, ACCESS_EXCLUSIVE).
        //
        // Phase 3 Enhancement: Implement proper per-proc-id lock tracking to support:
        //   1. Recursive locking (same proc_id acquiring same mode multiple times)
        //   2. Fast-path for non-conflicting modes (multiple ACCESS_SHARE holders)
        // Current implementation uses conflict checking for all requests (correct but slower)
        //
        // For now, all lock requests go through conflict checking below.

        // OPTIMIZATION: Fast-path for read-only transactions with SHARE locks
        // Read-only transactions typically use ACCESS_SHARE or SHARE locks
        // These don't conflict with each other, so we can check quickly
        bool is_share_lock = (mode == LockMode::LOCK_ACCESS_SHARE || mode == LockMode::LOCK_SHARE ||
                              mode == LockMode::LOCK_ROW_SHARE);

        if (is_readonly_txn && is_share_lock && !checkConflictInternal(lock_obj, mode, proc_id))
        {
            // Fast path: no conflicts, grant immediately
            lock_obj->granted_mask |= (1u << mode_idx);
            lock_obj->granted_counts[mode_idx]++;
            lock_obj->total_acquisitions++;
            stats_.locks_acquired++;
            stats_.readonly_locks_acquired++;
            stats_.readonly_fast_path++;
            stats_.current_locks++;
            if (stats_.current_locks > stats_.max_locks_used)
            {
                stats_.max_locks_used = stats_.current_locks;
            }
            proc_locks_.insert({proc_id, ProcLockEntry{lock_obj, mode}});
            return Status::OK;
        }

        // Check for conflicts with existing locks
        if (checkConflictInternal(lock_obj, mode, proc_id))
        {
            uint32_t blocker_proc_id = 0;
            LockMode blocker_mode = LockMode::LOCK_ACCESS_SHARE;
            (void)checkConflictInternal(lock_obj, mode, proc_id, &blocker_proc_id, &blocker_mode);

            if (!wait)
            {
                stats_.no_wait_rejections++;
                const uint64_t end_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                              std::chrono::system_clock::now().time_since_epoch())
                                              .count();
                const std::string message =
                    "LOCK_CONFLICT_NO_WAIT: blocker_proc_id=" +
                    std::to_string(blocker_proc_id) + " requested_mode=" + lockModeName(mode) +
                    " blocker_mode=" + lockModeName(blocker_mode) + " resource=" +
                    formatLockResourceId(tag);
                recordLockWaitHistory(db_, proc_id, blocker_proc_id, tag, mode, blocker_mode,
                                      end_time, end_time, false, "LOCK_CONFLICT_NO_WAIT", "",
                                      false);
                SET_ERROR_CONTEXT(ctx, Status::LOCK_CONFLICT, message.c_str());
                return Status::LOCK_CONFLICT;
            }

            // Must wait for lock - create request with RAII
            auto req_ptr = std::make_unique<LockRequest>();
            LockRequest *req = req_ptr.get();

            req->proc_id = proc_id;
            req->mode = mode;
            req->granted = false;
            req->request_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();
            req->blocker_proc_id = blocker_proc_id;
            req->blocker_mode = blocker_mode;

            // Add to wait queue
            lock_obj->wait_queue.push_back(std::move(req_ptr));
            stats_.lock_waits++;
            lock_obj->total_waits++;
            if (is_readonly_txn)
            {
                stats_.readonly_lock_waits++;
            }

            // Wait for lock to be granted
            auto timeout = (timeout_ms == 0) ? std::chrono::milliseconds::max()
                                             : std::chrono::milliseconds(timeout_ms);

            bool granted = lock_wait_cv_.wait_for(lock, timeout, [req]() {
                return req->granted || req->terminal_status != Status::OK;
            });
            uint64_t end_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();

            if (!granted)
            {
                const uint64_t request_time = req->request_time;
                // Timeout - remove from queue (RAII handles deletion)
                auto it = std::find_if(lock_obj->wait_queue.begin(), lock_obj->wait_queue.end(),
                                       [req](const std::unique_ptr<LockRequest> &r)
                                       { return r.get() == req; });
                if (it != lock_obj->wait_queue.end())
                {
                    lock_obj->wait_queue.erase(it);
                }
                uint32_t timeout_blocker_proc_id = 0;
                LockMode timeout_blocker_mode = LockMode::LOCK_ACCESS_SHARE;
                (void)checkConflictInternal(lock_obj, mode, proc_id, &timeout_blocker_proc_id,
                                            &timeout_blocker_mode);
                recordLockWaitHistory(db_, proc_id, timeout_blocker_proc_id, tag, mode,
                                      timeout_blocker_mode, request_time, end_time, true,
                                      "LOCK_TIMEOUT", "", false);
                stats_.lock_timeouts++;
                SET_ERROR_CONTEXT(ctx, Status::LOCK_TIMEOUT, "Lock acquisition timeout");
                return Status::LOCK_TIMEOUT;
            }

            if (req->terminal_status != Status::OK)
            {
                const Status terminal_status = req->terminal_status;
                const uint32_t blocker_proc_id = req->blocker_proc_id;
                const LockMode terminal_blocker_mode = req->blocker_mode;
                const uint64_t request_time = req->request_time;
                const bool retry_eligible = req->retry_eligible;
                auto it = std::find_if(lock_obj->wait_queue.begin(), lock_obj->wait_queue.end(),
                                       [req](const std::unique_ptr<LockRequest> &r)
                                       { return r.get() == req; });
                if (it != lock_obj->wait_queue.end())
                {
                    lock_obj->wait_queue.erase(it);
                }
                recordLockWaitHistory(db_, proc_id, blocker_proc_id, tag, mode,
                                      terminal_blocker_mode, request_time, end_time, false,
                                      "DEADLOCK_DETECTED", "youngest_xid",
                                      retry_eligible);
                const std::string message =
                    "DEADLOCK_DETECTED: blocker_proc_id=" + std::to_string(blocker_proc_id) +
                    " requested_mode=" + lockModeName(mode) +
                    " blocker_mode=" + lockModeName(terminal_blocker_mode) +
                    " resource=" + formatLockResourceId(tag);
                SET_ERROR_CONTEXT(ctx, terminal_status, message.c_str());
                return terminal_status;
            }

            // Lock granted, remove from queue (RAII handles deletion)
            const uint32_t granted_blocker_proc_id = req->blocker_proc_id;
            const LockMode granted_blocker_mode = req->blocker_mode;
            const uint64_t request_time = req->request_time;
            auto it = std::find_if(lock_obj->wait_queue.begin(), lock_obj->wait_queue.end(),
                                   [req](const std::unique_ptr<LockRequest> &r)
                                   { return r.get() == req; });
            if (it != lock_obj->wait_queue.end())
            {
                lock_obj->wait_queue.erase(it);
            }
            recordLockWaitHistory(db_, proc_id, granted_blocker_proc_id, tag, mode,
                                  granted_blocker_mode, request_time, end_time, false,
                                  "WAIT_GRANTED", "", false);
        }

        // Grant the lock
        lock_obj->granted_mask |= (1u << mode_idx);
        lock_obj->granted_counts[mode_idx]++;
        lock_obj->total_acquisitions++;
        stats_.locks_acquired++;
        if (is_readonly_txn)
        {
            stats_.readonly_locks_acquired++;
        }
        stats_.current_locks++;
        if (stats_.current_locks > stats_.max_locks_used)
        {
            stats_.max_locks_used = stats_.current_locks;
        }

        // Track lock by proc_id
        proc_locks_.insert({proc_id, ProcLockEntry{lock_obj, mode}});

        return Status::OK;
    }

    auto LockManager::releaseLock(uint32_t proc_id, const LockTag &tag, LockMode mode,
                                  ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);

        auto it = lock_table_.find(tag);
        if (it == lock_table_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Lock not found");
            return Status::NOT_FOUND;
        }

        Lock *lock_obj = it->second.get();
        uint8_t mode_idx = static_cast<uint8_t>(mode) - 1;

        if (mode_idx >= 8 || lock_obj->granted_counts[mode_idx] == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Lock not held");
            return Status::INVALID_ARGUMENT;
        }

        // Decrement count
        lock_obj->granted_counts[mode_idx]--;
        if (lock_obj->granted_counts[mode_idx] == 0)
        {
            lock_obj->granted_mask &= ~(1u << mode_idx);
        }

        stats_.locks_released++;
        stats_.current_locks--;

        // Remove from proc_locks_
        auto range = proc_locks_.equal_range(proc_id);
        for (auto it2 = range.first; it2 != range.second; ++it2)
        {
            if (it2->second.lock == lock_obj && it2->second.mode == mode)
            {
                proc_locks_.erase(it2);
                break;
            }
        }

        // Try to grant waiting locks
        grantWaitingLocks(lock_obj);

        // Remove lock if unused (RAII handles deletion)
        removeLockIfUnused(tag);

        return Status::OK;
    }

    auto LockManager::releaseAllLocks(uint32_t proc_id, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);

        auto range = proc_locks_.equal_range(proc_id);
        std::vector<std::pair<LockTag, LockMode>> locks_to_release;

        // Collect all locks held by this process (store tag instead of pointer)
        for (auto it = range.first; it != range.second; ++it)
        {
            Lock *lock_obj = it->second.lock;
            LockMode mode = it->second.mode;
            if (lock_obj)
            {
                locks_to_release.push_back({lock_obj->tag, mode});
            }
        }

        // Release all locks
        for (const auto &pair : locks_to_release)
        {
            const LockTag &tag = pair.first;
            LockMode mode = pair.second;
            uint8_t mode_idx = static_cast<uint8_t>(mode) - 1;

            // Find lock (might have been deleted in previous iteration)
            auto it = lock_table_.find(tag);
            if (it == lock_table_.end())
            {
                continue;
            }
            Lock *lock_obj = it->second.get();

            if (mode_idx >= 8 || lock_obj->granted_counts[mode_idx] == 0)
            {
                continue;
            }

            // Release one instance of this lock
            lock_obj->granted_counts[mode_idx]--;
            if (lock_obj->granted_counts[mode_idx] == 0)
            {
                lock_obj->granted_mask &= ~(1u << mode_idx);
            }

            stats_.locks_released++;
            stats_.current_locks--;

            // Try to grant waiting locks
            grantWaitingLocks(lock_obj);

            // Remove lock if unused (RAII handles deletion)
            removeLockIfUnused(tag);
        }

        // Clear all proc_locks entries
        proc_locks_.erase(proc_id);

        return Status::OK;
    }

    bool LockManager::checkConflict(const LockTag &tag, LockMode mode) const
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);

        auto it = lock_table_.find(tag);
        if (it == lock_table_.end())
        {
            return false; // No lock exists, no conflict
        }

        return checkConflictInternal(it->second.get(), mode, 0);
    }

    void LockManager::getStatistics(LockStats *stats_out) const
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);
        *stats_out = stats_;
    }

    auto LockManager::listLocks(std::vector<LockSnapshot>& locks_out) const -> Status
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);
        locks_out.clear();

        // Granted locks per backend
        for (const auto& entry : proc_locks_)
        {
            const ProcLockEntry& held = entry.second;
            if (!held.lock)
            {
                continue;
            }
            LockSnapshot snap;
            snap.tag = held.lock->tag;
            snap.mode = held.mode;
            snap.proc_id = entry.first;
            snap.granted = true;
            snap.request_time = 0;
            locks_out.push_back(std::move(snap));
        }

        // Waiting locks from queues
        for (const auto& pair : lock_table_)
        {
            const Lock* lock_obj = pair.second.get();
            for (const auto& req_ptr : lock_obj->wait_queue)
            {
                const LockRequest* req = req_ptr.get();
                LockSnapshot snap;
                snap.tag = lock_obj->tag;
                snap.mode = req->mode;
                snap.proc_id = req->proc_id;
                snap.granted = false;
                snap.request_time = req->request_time;
                locks_out.push_back(std::move(snap));
            }
        }

        return Status::OK;
    }

    auto LockManager::detectDeadlocks(ErrorContext *ctx) -> Status
    {
        if (!deadlock_detector_)
        {
            return Status::OK;
        }

        // OPTIMIZATION NOTE: When deadlock detection is fully implemented,
        // read-only transactions can be excluded from deadlock detection
        // since they only acquire SHARE locks and cannot create write-write
        // deadlock cycles. This will reduce deadlock detection overhead.
        //
        // Future implementation:
        //   - Skip adding read-only transactions to wait-for graph
        //   - Filter out read-only waiters/holders in buildWaitGraph()
        return deadlock_detector_->detectDeadlocks(ctx);
    }

    Lock *LockManager::findOrCreateLock(const LockTag &tag)
    {
        auto it = lock_table_.find(tag);
        if (it != lock_table_.end())
        {
            return it->second.get();
        }

        // Check lock limit
        if (lock_table_.size() >= max_locks_)
        {
            return nullptr;
        }

        // Allocate new lock with RAII
        auto lock_obj = std::make_unique<Lock>();

        lock_obj->tag = tag;
        lock_obj->granted_mask = 0;
        std::memset(lock_obj->granted_counts, 0, sizeof(lock_obj->granted_counts));
        // wait_queue is std::list, default-constructed as empty
        lock_obj->total_acquisitions = 0;
        lock_obj->total_waits = 0;

        Lock *lock_ptr = lock_obj.get();
        lock_table_[tag] = std::move(lock_obj);

        return lock_ptr;
    }

    void LockManager::removeLockIfUnused(const LockTag &tag)
    {
        auto it = lock_table_.find(tag);
        if (it != lock_table_.end())
        {
            Lock *lock_obj = it->second.get();
            if (lock_obj->granted_mask == 0 && lock_obj->wait_queue.empty())
            {
                // RAII: unique_ptr automatically deletes Lock when erased
                lock_table_.erase(it);
            }
        }
    }

    void LockManager::grantWaitingLocks(Lock *lock_obj)
    {
        if (lock_obj->wait_queue.empty())
        {
            return;
        }

        bool granted_any = true;
        while (granted_any && !lock_obj->wait_queue.empty())
        {
            granted_any = false;

            for (auto &req_ptr : lock_obj->wait_queue)
            {
                LockRequest *req = req_ptr.get();
                if (req->terminal_status != Status::OK)
                {
                    continue;
                }
                if (!checkConflictInternal(lock_obj, req->mode, req->proc_id))
                {
                    // Can grant this lock
                    uint8_t mode_idx = static_cast<uint8_t>(req->mode) - 1;
                    lock_obj->granted_mask |= (1u << mode_idx);
                    lock_obj->granted_counts[mode_idx]++;

                    req->granted = true;
                    granted_any = true;

                    // Wake up waiter
                    lock_wait_cv_.notify_all();
                }
            }
        }
    }

    bool LockManager::checkConflictInternal(const Lock *lock_obj, LockMode mode,
                                            uint32_t skip_proc_id,
                                            uint32_t *blocker_proc_id_out,
                                            LockMode *blocker_mode_out) const
    {
        for (const auto &proc_pair : proc_locks_)
        {
            const uint32_t holder_proc_id = proc_pair.first;
            const ProcLockEntry &entry = proc_pair.second;
            if (entry.lock != lock_obj)
            {
                continue;
            }
            if (skip_proc_id != 0 && holder_proc_id == skip_proc_id)
            {
                continue;
            }

            if (!lockModesConflictForTag(lock_obj->tag, entry.mode, mode, conflict_matrix_))
            {
                continue;
            }

            if (blocker_proc_id_out != nullptr)
            {
                *blocker_proc_id_out = holder_proc_id;
            }
            if (blocker_mode_out != nullptr)
            {
                *blocker_mode_out = entry.mode;
            }
            return true;
        }

        return false;
    }

    bool LockManager::isReadOnlyTransaction(uint32_t proc_id) const
    {
        // Check if this proc_id belongs to a read-only transaction
        // by examining the ProcArray
        ProcArray *proc_array = ProcArrayManager::getInstance();
        if (!proc_array)
        {
            return false; // Assume not read-only if ProcArray unavailable
        }

        pthread_rwlock_rdlock(&proc_array->array_lock);

        ProcessControlBlock *pcbs = reinterpret_cast<ProcessControlBlock *>(
            reinterpret_cast<uint8_t *>(proc_array) + sizeof(ProcArray));

        bool is_readonly = false;
        for (uint32_t i = 0; i < proc_array->max_backends; ++i)
        {
            if (pcbs[i].is_active && pcbs[i].proc_id == proc_id)
            {
                is_readonly = pcbs[i].is_read_only;
                break;
            }
        }

        pthread_rwlock_unlock(&proc_array->array_lock);

        return is_readonly;
    }

    // ============================================================================
    // DeadlockDetector Implementation
    // ============================================================================

    DeadlockDetector::DeadlockDetector(LockManager *lock_mgr) : lock_mgr_(lock_mgr) {}

    DeadlockDetector::~DeadlockDetector() = default;

    auto DeadlockDetector::detectDeadlocks(ErrorContext *ctx) -> Status
    {
        std::vector<uint32_t> victims;
        {
            std::lock_guard<std::mutex> lock(lock_mgr_->lock_table_mutex_);
            wait_graph_.clear();
            buildWaitGraph();
            auto cycles = findAllCycles();

            if (!cycles.empty())
            {
                victims.reserve(cycles.size());
                for (const auto &cycle : cycles)
                {
                    victims.push_back(selectVictim(cycle));
                }
            }
        }

        if (!victims.empty())
        {
            // Deadlock detected! Abort one transaction from each cycle
            std::sort(victims.begin(), victims.end());
            victims.erase(std::unique(victims.begin(), victims.end()), victims.end());
            for (uint32_t victim : victims)
            {
                Status status = abortTransaction(victim, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }
        }

        return Status::OK;
    }

    bool DeadlockDetector::wouldCreateCycle(uint32_t waiter, uint32_t holder)
    {
        // Check if holder is already waiting for waiter (direct or indirect)
        std::unordered_set<uint32_t> visited;
        std::vector<uint32_t> stack = {holder};

        while (!stack.empty())
        {
            uint32_t current = stack.back();
            stack.pop_back();

            if (current == waiter)
            {
                return true; // Cycle detected
            }

            if (visited.count(current))
            {
                continue;
            }
            visited.insert(current);

            auto it = wait_graph_.find(current);
            if (it != wait_graph_.end())
            {
                for (uint32_t next : it->second)
                {
                    stack.push_back(next);
                }
            }
        }

        return false;
    }

    void DeadlockDetector::buildWaitGraph()
    {
        // Build wait-for graph by scanning lock manager state
        // Edge from W -> H means "W is waiting for H"
        //
        // HIGH-2 FIX: CRITICAL - Caller MUST hold lock_mgr_->lock_table_mutex_
        // This method accesses lock_table_ and proc_locks_, which are protected
        // by lock_table_mutex_. The mutex is acquired in DeadlockDetector::detectDeadlocks()
        // before calling this method.

        wait_graph_.clear();

        // Iterate through all locks in the system
        for (const auto &pair : lock_mgr_->lock_table_)
        {
            const Lock *lock_obj = pair.second.get();

            // Skip locks with no waiters
            if (lock_obj->wait_queue.empty())
            {
                continue;
            }

            // For each waiter in the queue
            for (const auto &req_ptr : lock_obj->wait_queue)
            {
                const LockRequest *req = req_ptr.get();
                uint32_t waiter_proc_id = req->proc_id;
                LockMode waiter_mode = req->mode;
                // Determine which granted locks block this waiter
                // We need to check which holder proc_ids have locks that conflict

                // Check each granted lock mode to see if it conflicts
                for (uint8_t held_idx = 0; held_idx < 8; ++held_idx)
                {
                    if (lock_obj->granted_counts[held_idx] > 0)
                    {
                        // Check if this held mode conflicts with waiter's requested mode
                        if (lockModesConflictForTag(lock_obj->tag,
                                                    static_cast<LockMode>(held_idx + 1),
                                                    waiter_mode,
                                                    lock_mgr_->conflict_matrix_))
                        {
                            // This lock mode conflicts! Find which proc_ids hold it
                            // We need to scan proc_locks_ to find holders

                            LockMode held_mode = static_cast<LockMode>(held_idx + 1);

                            // Find all proc_ids holding this lock in this mode
                            for (const auto &proc_pair : lock_mgr_->proc_locks_)
                            {
                                uint32_t holder_proc_id = proc_pair.first;
                                const ProcLockEntry &entry = proc_pair.second;

                                // Check if this proc holds the same lock object and mode
                                if (entry.lock == lock_obj && entry.mode == held_mode)
                                {
                                    // Don't add self-edges
                                    if (holder_proc_id != waiter_proc_id)
                                    {
                                        // Add edge: waiter -> holder
                                        wait_graph_[waiter_proc_id].push_back(holder_proc_id);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Remove duplicate edges in wait graph
        for (auto &pair : wait_graph_)
        {
            auto &holders = pair.second;
            std::sort(holders.begin(), holders.end());
            holders.erase(std::unique(holders.begin(), holders.end()), holders.end());
        }
    }

    bool DeadlockDetector::hasCycle(uint32_t start_proc, std::unordered_set<uint32_t> *visited,
                                    std::unordered_set<uint32_t> *rec_stack)
    {
        visited->insert(start_proc);
        rec_stack->insert(start_proc);

        auto it = wait_graph_.find(start_proc);
        if (it != wait_graph_.end())
        {
            for (uint32_t neighbor : it->second)
            {
                if (visited->count(neighbor) == 0)
                {
                    if (hasCycle(neighbor, visited, rec_stack))
                    {
                        return true;
                    }
                }
                else if (rec_stack->count(neighbor) > 0)
                {
                    return true; // Cycle found
                }
            }
        }

        rec_stack->erase(start_proc);
        return false;
    }

    std::vector<std::vector<uint32_t>> DeadlockDetector::findAllCycles()
    {
        std::vector<std::vector<uint32_t>> cycles;
        std::unordered_set<uint32_t> visited;

        for (const auto &pair : wait_graph_)
        {
            uint32_t proc = pair.first;
            if (visited.count(proc) == 0)
            {
                std::unordered_set<uint32_t> rec_stack;
                if (hasCycle(proc, &visited, &rec_stack))
                {
                    // Extract cycle from rec_stack
                    std::vector<uint32_t> cycle(rec_stack.begin(), rec_stack.end());
                    cycles.push_back(cycle);
                }
            }
        }

        return cycles;
    }

    uint32_t DeadlockDetector::selectVictim(const std::vector<uint32_t> &cycle)
    {
        if (cycle.empty())
        {
            return 0;
        }

        // Select youngest transaction (highest XID) as victim
        // Younger transactions have done less work, so aborting them is cheaper

        ProcArray *proc_array = ProcArrayManager::getInstance();
        if (!proc_array)
        {
            // Fallback: return first process if ProcArray unavailable
            return cycle[0];
        }

        pthread_rwlock_rdlock(&proc_array->array_lock);

        ProcessControlBlock *pcbs = reinterpret_cast<ProcessControlBlock *>(
            reinterpret_cast<uint8_t *>(proc_array) + sizeof(ProcArray));

        // Find the process with the highest XID (youngest transaction)
        uint32_t victim_proc_id = cycle[0];
        uint64_t highest_xid = 0;

        for (uint32_t proc_id : cycle)
        {
            // Find this proc_id in the ProcArray
            for (uint32_t i = 0; i < proc_array->max_backends; ++i)
            {
                if (pcbs[i].is_active && pcbs[i].proc_id == proc_id)
                {
                    if (pcbs[i].xid > highest_xid)
                    {
                        highest_xid = pcbs[i].xid;
                        victim_proc_id = proc_id;
                    }
                    break;
                }
            }
        }

        pthread_rwlock_unlock(&proc_array->array_lock);

        return victim_proc_id;
    }

    auto DeadlockDetector::abortTransaction(uint32_t proc_id, ErrorContext *ctx) -> Status
    {
        // Abort transaction to break deadlock:
        // 1. Get XID from ProcArray
        // 2. Rollback transaction in TransactionManager
        // 3. Release all locks held by proc_id
        // 4. Update statistics

        // Get the XID for this proc_id
        uint64_t xid = 0;
        ProcArray *proc_array = ProcArrayManager::getInstance();
        if (proc_array)
        {
            pthread_rwlock_rdlock(&proc_array->array_lock);

            ProcessControlBlock *pcbs = reinterpret_cast<ProcessControlBlock *>(
                reinterpret_cast<uint8_t *>(proc_array) + sizeof(ProcArray));

            for (uint32_t i = 0; i < proc_array->max_backends; ++i)
            {
                if (pcbs[i].is_active && pcbs[i].proc_id == proc_id)
                {
                    xid = pcbs[i].xid;
                    break;
                }
            }

            pthread_rwlock_unlock(&proc_array->array_lock);
        }

        {
            std::lock_guard<std::mutex> lock(lock_mgr_->lock_table_mutex_);
            for (auto &pair : lock_mgr_->lock_table_)
            {
                Lock *lock_obj = pair.second.get();
                for (auto &req_ptr : lock_obj->wait_queue)
                {
                    LockRequest *req = req_ptr.get();
                    if (req->proc_id != proc_id)
                    {
                        continue;
                    }

                    req->terminal_status = Status::DEADLOCK;
                    req->retry_eligible = true;

                    auto waiters = wait_graph_.find(proc_id);
                    if (waiters != wait_graph_.end() && !waiters->second.empty())
                    {
                        req->blocker_proc_id = waiters->second.front();
                        for (const auto &proc_pair : lock_mgr_->proc_locks_)
                        {
                            if (proc_pair.first == req->blocker_proc_id &&
                                proc_pair.second.lock == lock_obj)
                            {
                                req->blocker_mode = proc_pair.second.mode;
                                break;
                            }
                        }
                    }
                }
            }
            lock_mgr_->lock_wait_cv_.notify_all();
        }

        // Rollback the transaction in TransactionManager after the victim has
        // already been marked, so the waiting backend can surface DEADLOCK
        // without sitting on the full timeout window.
        if (xid != 0 && lock_mgr_ && lock_mgr_->db_)
        {
            TransactionManager *txn_mgr = lock_mgr_->db_->transaction_manager();
            if (txn_mgr)
            {
                Status status = txn_mgr->rollbackTransaction(proc_id, xid, ctx);
                if (status != Status::OK)
                {
                    // Log error but continue with cleanup
                    LOG_ERROR(LOCK,
                              "Failed to rollback transaction XID=%lu during deadlock resolution: "
                              "status=%d",
                              xid, static_cast<int>(status));
                }
            }
        }

        // Release all locks held by this proc_id
        // This will also wake up any waiters via grantWaitingLocks()
        if (lock_mgr_)
        {
            Status status = lock_mgr_->releaseAllLocks(proc_id, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Update deadlock statistics
            lock_mgr_->stats_.deadlocks_detected++;
        }

        return Status::OK;
    }

} // namespace scratchbird::core
