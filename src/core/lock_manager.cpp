#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/logger.h"
#include <chrono>
#include <algorithm>
#include <cstring>

namespace scratchbird::core
{
    // Lock conflict matrix [held_mode][requested_mode]
    // true = conflict (must wait), false = no conflict (can grant)
    // Modes are 1-indexed (LOCK_ACCESS_SHARE=1), so subtract 1 for array index
    const bool LockManager::conflict_matrix_[8][8] = {
        //     AS  RS  RE  SUE  S  SRE  E  AE
        /* AS */ {0,  0,  0,  0,  0,  0,  0,  1},
        /* RS */ {0,  0,  0,  0,  0,  0,  1,  1},
        /* RE */ {0,  0,  0,  0,  1,  1,  1,  1},
        /* SUE*/ {0,  0,  0,  0,  1,  1,  1,  1},
        /* S  */ {0,  0,  1,  1,  0,  1,  1,  1},
        /* SRE*/ {0,  0,  1,  1,  1,  1,  1,  1},
        /* E  */ {0,  1,  1,  1,  1,  1,  1,  1},
        /* AE */ {1,  1,  1,  1,  1,  1,  1,  1}
    };

    LockManager::LockManager(Database* db)
        : db_(db)
        , max_locks_(config::DEFAULT_MAX_LOCKS)
        , deadlock_timeout_ms_(config::DEFAULT_DEADLOCK_TIMEOUT_MS)
    {
        std::memset(&stats_, 0, sizeof(stats_));
    }

    LockManager::~LockManager()
    {
        shutdown(nullptr);
    }

    auto LockManager::initialize(ErrorContext* ctx) -> Status
    {
        if (!db_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is null");
            return Status::INVALID_ARGUMENT;
        }

        // Create deadlock detector
        try {
            deadlock_detector_ = std::make_unique<DeadlockDetector>(this);
        } catch (const std::bad_alloc&) {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate DeadlockDetector");
            return Status::OOM;
        }

        return Status::OK;
    }

    auto LockManager::shutdown(ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);

        // RAII: unique_ptr automatically cleans up Lock objects and their wait_queues
        lock_table_.clear();
        proc_locks_.clear();

        deadlock_detector_.reset();

        return Status::OK;
    }

    auto LockManager::acquireLock(
        uint32_t proc_id,
        const LockTag& tag,
        LockMode mode,
        bool wait,
        uint32_t timeout_ms,
        ErrorContext* ctx) -> Status
    {
        // OPTIMIZATION: Check if this is a read-only transaction
        // Read-only transactions can benefit from fast-path lock acquisition
        bool is_readonly_txn = isReadOnlyTransaction(proc_id);

        std::unique_lock<std::mutex> lock(lock_table_mutex_);

        // Get or create lock object
        Lock* lock_obj = findOrCreateLock(tag);
        if (!lock_obj) {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate lock");
            return Status::OOM;
        }

        uint8_t mode_idx = static_cast<uint8_t>(mode) - 1;
        if (mode_idx >= 8) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid lock mode");
            return Status::INVALID_ARGUMENT;
        }

        // NOTE: Removed buggy optimization that allowed ANY proc_id to increment
        // granted_counts if the mode was already granted. This was incorrect for
        // self-conflicting modes (SHARE_ROW_EXCLUSIVE, EXCLUSIVE, ACCESS_EXCLUSIVE).
        //
        // TODO: Implement proper per-proc-id lock tracking to support:
        //   1. Recursive locking (same proc_id acquiring same mode multiple times)
        //   2. Fast-path for non-conflicting modes (multiple ACCESS_SHARE holders)
        //
        // For now, all lock requests go through conflict checking below.

        // OPTIMIZATION: Fast-path for read-only transactions with SHARE locks
        // Read-only transactions typically use ACCESS_SHARE or SHARE locks
        // These don't conflict with each other, so we can check quickly
        bool is_share_lock = (mode == LockMode::LOCK_ACCESS_SHARE ||
                              mode == LockMode::LOCK_SHARE ||
                              mode == LockMode::LOCK_ROW_SHARE);

        if (is_readonly_txn && is_share_lock && !checkConflictInternal(lock_obj, mode, proc_id)) {
            // Fast path: no conflicts, grant immediately
            lock_obj->granted_mask |= (1u << mode_idx);
            lock_obj->granted_counts[mode_idx]++;
            lock_obj->total_acquisitions++;
            stats_.locks_acquired++;
            stats_.readonly_locks_acquired++;
            stats_.readonly_fast_path++;
            stats_.current_locks++;
            if (stats_.current_locks > stats_.max_locks_used) {
                stats_.max_locks_used = stats_.current_locks;
            }
            proc_locks_.insert({proc_id, lock_obj});
            return Status::OK;
        }

        // Check for conflicts with existing locks
        if (checkConflictInternal(lock_obj, mode, proc_id)) {
            if (!wait) {
                SET_ERROR_CONTEXT(ctx, Status::LOCK_CONFLICT, "Lock conflict, no wait requested");
                return Status::LOCK_CONFLICT;
            }

            // Must wait for lock - create request with RAII
            auto req_ptr = std::make_unique<LockRequest>();
            LockRequest* req = req_ptr.get();

            req->proc_id = proc_id;
            req->mode = mode;
            req->granted = false;
            req->request_time = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            // Add to wait queue
            lock_obj->wait_queue.push_back(std::move(req_ptr));
            stats_.lock_waits++;
            lock_obj->total_waits++;
            if (is_readonly_txn) {
                stats_.readonly_lock_waits++;
            }

            // Wait for lock to be granted
            auto timeout = (timeout_ms == 0)
                ? std::chrono::milliseconds::max()
                : std::chrono::milliseconds(timeout_ms);

            bool granted = lock_wait_cv_.wait_for(lock, timeout, [req]() {
                return req->granted;
            });

            if (!granted) {
                // Timeout - remove from queue (RAII handles deletion)
                auto it = std::find_if(lock_obj->wait_queue.begin(), lock_obj->wait_queue.end(),
                    [req](const std::unique_ptr<LockRequest>& r) { return r.get() == req; });
                if (it != lock_obj->wait_queue.end()) {
                    lock_obj->wait_queue.erase(it);
                }
                stats_.lock_timeouts++;
                SET_ERROR_CONTEXT(ctx, Status::LOCK_TIMEOUT, "Lock acquisition timeout");
                return Status::LOCK_TIMEOUT;
            }

            // Lock granted, remove from queue (RAII handles deletion)
            auto it = std::find_if(lock_obj->wait_queue.begin(), lock_obj->wait_queue.end(),
                [req](const std::unique_ptr<LockRequest>& r) { return r.get() == req; });
            if (it != lock_obj->wait_queue.end()) {
                lock_obj->wait_queue.erase(it);
            }
        }

        // Grant the lock
        lock_obj->granted_mask |= (1u << mode_idx);
        lock_obj->granted_counts[mode_idx]++;
        lock_obj->total_acquisitions++;
        stats_.locks_acquired++;
        if (is_readonly_txn) {
            stats_.readonly_locks_acquired++;
        }
        stats_.current_locks++;
        if (stats_.current_locks > stats_.max_locks_used) {
            stats_.max_locks_used = stats_.current_locks;
        }

        // Track lock by proc_id
        proc_locks_.insert({proc_id, lock_obj});

        return Status::OK;
    }

    auto LockManager::releaseLock(
        uint32_t proc_id,
        const LockTag& tag,
        LockMode mode,
        ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);

        auto it = lock_table_.find(tag);
        if (it == lock_table_.end()) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Lock not found");
            return Status::NOT_FOUND;
        }

        Lock* lock_obj = it->second.get();
        uint8_t mode_idx = static_cast<uint8_t>(mode) - 1;

        if (mode_idx >= 8 || lock_obj->granted_counts[mode_idx] == 0) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Lock not held");
            return Status::INVALID_ARGUMENT;
        }

        // Decrement count
        lock_obj->granted_counts[mode_idx]--;
        if (lock_obj->granted_counts[mode_idx] == 0) {
            lock_obj->granted_mask &= ~(1u << mode_idx);
        }

        stats_.locks_released++;
        stats_.current_locks--;

        // Remove from proc_locks_
        auto range = proc_locks_.equal_range(proc_id);
        for (auto it2 = range.first; it2 != range.second; ++it2) {
            if (it2->second == lock_obj) {
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

    auto LockManager::releaseAllLocks(uint32_t proc_id, ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);

        auto range = proc_locks_.equal_range(proc_id);
        std::vector<std::pair<LockTag, LockMode>> locks_to_release;

        // Collect all locks held by this process (store tag instead of pointer)
        for (auto it = range.first; it != range.second; ++it) {
            Lock* lock_obj = it->second;

            // Find which modes this proc holds
            for (uint8_t i = 0; i < 8; ++i) {
                if (lock_obj->granted_counts[i] > 0) {
                    LockMode mode = static_cast<LockMode>(i + 1);
                    locks_to_release.push_back({lock_obj->tag, mode});
                }
            }
        }

        // Release all locks
        for (const auto& pair : locks_to_release) {
            const LockTag& tag = pair.first;
            LockMode mode = pair.second;
            uint8_t mode_idx = static_cast<uint8_t>(mode) - 1;

            // Find lock (might have been deleted in previous iteration)
            auto it = lock_table_.find(tag);
            if (it == lock_table_.end()) {
                continue;
            }
            Lock* lock_obj = it->second.get();

            // Release all instances of this lock
            uint32_t count = lock_obj->granted_counts[mode_idx];
            lock_obj->granted_counts[mode_idx] = 0;
            lock_obj->granted_mask &= ~(1u << mode_idx);

            stats_.locks_released += count;
            stats_.current_locks -= count;

            // Try to grant waiting locks
            grantWaitingLocks(lock_obj);

            // Remove lock if unused (RAII handles deletion)
            removeLockIfUnused(tag);
        }

        // Clear all proc_locks entries
        proc_locks_.erase(proc_id);

        return Status::OK;
    }

    bool LockManager::checkConflict(const LockTag& tag, LockMode mode)
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);

        auto it = lock_table_.find(tag);
        if (it == lock_table_.end()) {
            return false;  // No lock exists, no conflict
        }

        return checkConflictInternal(it->second.get(), mode, 0);
    }

    void LockManager::getStatistics(LockStats* stats_out)
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);
        *stats_out = stats_;
    }

    auto LockManager::detectDeadlocks(ErrorContext* ctx) -> Status
    {
        if (!deadlock_detector_) {
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

    Lock* LockManager::findOrCreateLock(const LockTag& tag)
    {
        auto it = lock_table_.find(tag);
        if (it != lock_table_.end()) {
            return it->second.get();
        }

        // Check lock limit
        if (lock_table_.size() >= max_locks_) {
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

        Lock* lock_ptr = lock_obj.get();
        lock_table_[tag] = std::move(lock_obj);

        return lock_ptr;
    }

    void LockManager::removeLockIfUnused(const LockTag& tag)
    {
        auto it = lock_table_.find(tag);
        if (it != lock_table_.end()) {
            Lock* lock_obj = it->second.get();
            if (lock_obj->granted_mask == 0 && lock_obj->wait_queue.empty()) {
                // RAII: unique_ptr automatically deletes Lock when erased
                lock_table_.erase(it);
            }
        }
    }

    void LockManager::grantWaitingLocks(Lock* lock_obj)
    {
        if (lock_obj->wait_queue.empty()) {
            return;
        }

        bool granted_any = true;
        while (granted_any && !lock_obj->wait_queue.empty()) {
            granted_any = false;

            for (auto& req_ptr : lock_obj->wait_queue) {
                LockRequest* req = req_ptr.get();
                if (!checkConflictInternal(lock_obj, req->mode, req->proc_id)) {
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

    bool LockManager::checkConflictInternal(
        const Lock* lock_obj,
        LockMode mode,
        uint32_t skip_proc_id)
    {
        uint8_t req_mode_idx = static_cast<uint8_t>(mode) - 1;

        // Check conflict with each granted lock
        for (uint8_t held_idx = 0; held_idx < 8; ++held_idx) {
            if (lock_obj->granted_counts[held_idx] > 0) {
                if (conflict_matrix_[held_idx][req_mode_idx]) {
                    return true;  // Conflict
                }
            }
        }

        return false;
    }

    bool LockManager::isReadOnlyTransaction(uint32_t proc_id) const
    {
        // Check if this proc_id belongs to a read-only transaction
        // by examining the ProcArray
        ProcArray* proc_array = ProcArrayManager::getInstance();
        if (!proc_array) {
            return false;  // Assume not read-only if ProcArray unavailable
        }

        pthread_rwlock_rdlock(&proc_array->array_lock);

        ProcessControlBlock* pcbs = reinterpret_cast<ProcessControlBlock*>(
            reinterpret_cast<uint8_t*>(proc_array) + sizeof(ProcArray));

        bool is_readonly = false;
        for (uint32_t i = 0; i < proc_array->max_backends; ++i) {
            if (pcbs[i].is_active && pcbs[i].proc_id == proc_id) {
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

    DeadlockDetector::DeadlockDetector(LockManager* lock_mgr)
        : lock_mgr_(lock_mgr)
    {
    }

    DeadlockDetector::~DeadlockDetector() = default;

    auto DeadlockDetector::detectDeadlocks(ErrorContext* ctx) -> Status
    {
        wait_graph_.clear();
        buildWaitGraph();

        auto cycles = findAllCycles();

        if (!cycles.empty()) {
            // Deadlock detected! Abort one transaction from each cycle
            for (const auto& cycle : cycles) {
                uint32_t victim = selectVictim(cycle);
                Status status = abortTransaction(victim, ctx);
                if (status != Status::OK) {
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

        while (!stack.empty()) {
            uint32_t current = stack.back();
            stack.pop_back();

            if (current == waiter) {
                return true;  // Cycle detected
            }

            if (visited.count(current)) {
                continue;
            }
            visited.insert(current);

            auto it = wait_graph_.find(current);
            if (it != wait_graph_.end()) {
                for (uint32_t next : it->second) {
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

        wait_graph_.clear();

        // Iterate through all locks in the system
        for (const auto& pair : lock_mgr_->lock_table_) {
            const Lock* lock_obj = pair.second.get();

            // Skip locks with no waiters
            if (lock_obj->wait_queue.empty()) {
                continue;
            }

            // For each waiter in the queue
            for (const auto& req_ptr : lock_obj->wait_queue) {
                const LockRequest* req = req_ptr.get();
                uint32_t waiter_proc_id = req->proc_id;
                LockMode waiter_mode = req->mode;
                uint8_t waiter_mode_idx = static_cast<uint8_t>(waiter_mode) - 1;

                // Determine which granted locks block this waiter
                // We need to check which holder proc_ids have locks that conflict

                // Check each granted lock mode to see if it conflicts
                for (uint8_t held_idx = 0; held_idx < 8; ++held_idx) {
                    if (lock_obj->granted_counts[held_idx] > 0) {
                        // Check if this held mode conflicts with waiter's requested mode
                        if (lock_mgr_->conflict_matrix_[held_idx][waiter_mode_idx]) {
                            // This lock mode conflicts! Find which proc_ids hold it
                            // We need to scan proc_locks_ to find holders

                            LockMode held_mode = static_cast<LockMode>(held_idx + 1);

                            // Find all proc_ids holding this lock in this mode
                            for (const auto& proc_pair : lock_mgr_->proc_locks_) {
                                uint32_t holder_proc_id = proc_pair.first;
                                Lock* proc_lock = proc_pair.second;

                                // Check if this proc holds the same lock object
                                if (proc_lock == lock_obj) {
                                    // Verify this proc actually holds the conflicting mode
                                    // (we can't directly check from proc_locks_, so we assume
                                    // if they're in proc_locks_ for this lock, they hold it)

                                    // Don't add self-edges
                                    if (holder_proc_id != waiter_proc_id) {
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
        for (auto& pair : wait_graph_) {
            auto& holders = pair.second;
            std::sort(holders.begin(), holders.end());
            holders.erase(std::unique(holders.begin(), holders.end()), holders.end());
        }
    }

    bool DeadlockDetector::hasCycle(
        uint32_t start_proc,
        std::unordered_set<uint32_t>* visited,
        std::unordered_set<uint32_t>* rec_stack)
    {
        visited->insert(start_proc);
        rec_stack->insert(start_proc);

        auto it = wait_graph_.find(start_proc);
        if (it != wait_graph_.end()) {
            for (uint32_t neighbor : it->second) {
                if (visited->count(neighbor) == 0) {
                    if (hasCycle(neighbor, visited, rec_stack)) {
                        return true;
                    }
                } else if (rec_stack->count(neighbor) > 0) {
                    return true;  // Cycle found
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

        for (const auto& pair : wait_graph_) {
            uint32_t proc = pair.first;
            if (visited.count(proc) == 0) {
                std::unordered_set<uint32_t> rec_stack;
                if (hasCycle(proc, &visited, &rec_stack)) {
                    // Extract cycle from rec_stack
                    std::vector<uint32_t> cycle(rec_stack.begin(), rec_stack.end());
                    cycles.push_back(cycle);
                }
            }
        }

        return cycles;
    }

    uint32_t DeadlockDetector::selectVictim(const std::vector<uint32_t>& cycle)
    {
        if (cycle.empty()) {
            return 0;
        }

        // Select youngest transaction (highest XID) as victim
        // Younger transactions have done less work, so aborting them is cheaper

        ProcArray* proc_array = ProcArrayManager::getInstance();
        if (!proc_array) {
            // Fallback: return first process if ProcArray unavailable
            return cycle[0];
        }

        pthread_rwlock_rdlock(&proc_array->array_lock);

        ProcessControlBlock* pcbs = reinterpret_cast<ProcessControlBlock*>(
            reinterpret_cast<uint8_t*>(proc_array) + sizeof(ProcArray));

        // Find the process with the highest XID (youngest transaction)
        uint32_t victim_proc_id = cycle[0];
        uint64_t highest_xid = 0;

        for (uint32_t proc_id : cycle) {
            // Find this proc_id in the ProcArray
            for (uint32_t i = 0; i < proc_array->max_backends; ++i) {
                if (pcbs[i].is_active && pcbs[i].proc_id == proc_id) {
                    if (pcbs[i].xid > highest_xid) {
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

    auto DeadlockDetector::abortTransaction(uint32_t proc_id, ErrorContext* ctx) -> Status
    {
        // Abort transaction to break deadlock:
        // 1. Get XID from ProcArray
        // 2. Rollback transaction in TransactionManager
        // 3. Release all locks held by proc_id
        // 4. Update statistics

        // Get the XID for this proc_id
        uint64_t xid = 0;
        ProcArray* proc_array = ProcArrayManager::getInstance();
        if (proc_array) {
            pthread_rwlock_rdlock(&proc_array->array_lock);

            ProcessControlBlock* pcbs = reinterpret_cast<ProcessControlBlock*>(
                reinterpret_cast<uint8_t*>(proc_array) + sizeof(ProcArray));

            for (uint32_t i = 0; i < proc_array->max_backends; ++i) {
                if (pcbs[i].is_active && pcbs[i].proc_id == proc_id) {
                    xid = pcbs[i].xid;
                    break;
                }
            }

            pthread_rwlock_unlock(&proc_array->array_lock);
        }

        // Rollback the transaction in TransactionManager
        if (xid != 0 && lock_mgr_ && lock_mgr_->db_) {
            TransactionManager* txn_mgr = lock_mgr_->db_->transaction_manager();
            if (txn_mgr) {
                Status status = txn_mgr->rollbackTransaction(proc_id, xid, ctx);
                if (status != Status::OK) {
                    // Log error but continue with cleanup
                    LOG_ERROR(LOCK, "Failed to rollback transaction XID=%lu during deadlock resolution: status=%d",
                              xid, static_cast<int>(status));
                }
            }
        }

        // Release all locks held by this proc_id
        // This will also wake up any waiters via grantWaitingLocks()
        if (lock_mgr_) {
            Status status = lock_mgr_->releaseAllLocks(proc_id, ctx);
            if (status != Status::OK) {
                return status;
            }

            // Update deadlock statistics
            lock_mgr_->stats_.deadlocks_detected++;
        }

        return Status::OK;
    }

} // namespace scratchbird::core
