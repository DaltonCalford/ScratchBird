#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
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
        , max_locks_(10000)
        , deadlock_timeout_ms_(1000)
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

        // Free all locks
        for (auto& pair : lock_table_) {
            Lock* lock_obj = pair.second;

            // Free all requests in wait queue
            LockRequest* req = lock_obj->wait_queue_head;
            while (req) {
                LockRequest* next = req->next;
                delete req;
                req = next;
            }

            delete lock_obj;
        }

        lock_table_.clear();
        proc_locks_.clear();

        // Free lock pool
        for (Lock* lock_obj : lock_pool_) {
            delete lock_obj;
        }
        lock_pool_.clear();

        // Free request pool
        for (LockRequest* req : request_pool_) {
            delete req;
        }
        request_pool_.clear();

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

        // Check if we already hold this lock
        if (lock_obj->granted_counts[mode_idx] > 0) {
            // Already granted, just increment count
            lock_obj->granted_counts[mode_idx]++;
            stats_.locks_acquired++;
            return Status::OK;
        }

        // Check for conflicts with existing locks
        if (checkConflictInternal(lock_obj, mode, proc_id)) {
            if (!wait) {
                SET_ERROR_CONTEXT(ctx, Status::LOCK_CONFLICT, "Lock conflict, no wait requested");
                return Status::LOCK_CONFLICT;
            }

            // Must wait for lock
            LockRequest* req = allocateRequest();
            if (!req) {
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate lock request");
                return Status::OOM;
            }

            req->proc_id = proc_id;
            req->mode = mode;
            req->granted = false;
            req->request_time = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            enqueueRequest(lock_obj, req);
            stats_.lock_waits++;
            lock_obj->total_waits++;

            // Wait for lock to be granted
            auto timeout = (timeout_ms == 0)
                ? std::chrono::milliseconds::max()
                : std::chrono::milliseconds(timeout_ms);

            bool granted = lock_wait_cv_.wait_for(lock, timeout, [req]() {
                return req->granted;
            });

            if (!granted) {
                // Timeout
                dequeueRequest(lock_obj, req);
                freeRequest(req);
                stats_.lock_timeouts++;
                SET_ERROR_CONTEXT(ctx, Status::LOCK_TIMEOUT, "Lock acquisition timeout");
                return Status::LOCK_TIMEOUT;
            }

            // Lock granted, remove from queue
            dequeueRequest(lock_obj, req);
            freeRequest(req);
        }

        // Grant the lock
        lock_obj->granted_mask |= (1u << mode_idx);
        lock_obj->granted_counts[mode_idx]++;
        lock_obj->total_acquisitions++;
        stats_.locks_acquired++;
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

        Lock* lock_obj = it->second;
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

        // Remove lock if unused
        removeLockIfUnused(lock_obj);

        return Status::OK;
    }

    auto LockManager::releaseAllLocks(uint32_t proc_id, ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(lock_table_mutex_);

        auto range = proc_locks_.equal_range(proc_id);
        std::vector<std::pair<Lock*, LockMode>> locks_to_release;

        // Collect all locks held by this process
        for (auto it = range.first; it != range.second; ++it) {
            Lock* lock_obj = it->second;

            // Find which modes this proc holds
            for (uint8_t i = 0; i < 8; ++i) {
                if (lock_obj->granted_counts[i] > 0) {
                    LockMode mode = static_cast<LockMode>(i + 1);
                    locks_to_release.push_back({lock_obj, mode});
                }
            }
        }

        // Release all locks
        for (const auto& pair : locks_to_release) {
            Lock* lock_obj = pair.first;
            LockMode mode = pair.second;
            uint8_t mode_idx = static_cast<uint8_t>(mode) - 1;

            // Release all instances of this lock
            uint32_t count = lock_obj->granted_counts[mode_idx];
            lock_obj->granted_counts[mode_idx] = 0;
            lock_obj->granted_mask &= ~(1u << mode_idx);

            stats_.locks_released += count;
            stats_.current_locks -= count;

            // Try to grant waiting locks
            grantWaitingLocks(lock_obj);

            // Remove lock if unused
            removeLockIfUnused(lock_obj);
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

        return checkConflictInternal(it->second, mode, 0);
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

        return deadlock_detector_->detectDeadlocks(ctx);
    }

    Lock* LockManager::findOrCreateLock(const LockTag& tag)
    {
        auto it = lock_table_.find(tag);
        if (it != lock_table_.end()) {
            return it->second;
        }

        // Check lock limit
        if (lock_table_.size() >= max_locks_) {
            return nullptr;
        }

        // Allocate new lock
        Lock* lock_obj = allocateLock();
        if (!lock_obj) {
            return nullptr;
        }

        lock_obj->tag = tag;
        lock_obj->granted_mask = 0;
        std::memset(lock_obj->granted_counts, 0, sizeof(lock_obj->granted_counts));
        lock_obj->wait_queue_head = nullptr;
        lock_obj->wait_queue_tail = nullptr;
        lock_obj->wait_queue_size = 0;
        lock_obj->total_acquisitions = 0;
        lock_obj->total_waits = 0;

        lock_table_[tag] = lock_obj;

        return lock_obj;
    }

    void LockManager::removeLockIfUnused(Lock* lock_obj)
    {
        if (lock_obj->granted_mask == 0 && lock_obj->wait_queue_size == 0) {
            lock_table_.erase(lock_obj->tag);
            freeLock(lock_obj);
        }
    }

    void LockManager::grantWaitingLocks(Lock* lock_obj)
    {
        if (!lock_obj->wait_queue_head) {
            return;
        }

        bool granted_any = true;
        while (granted_any && lock_obj->wait_queue_head) {
            granted_any = false;

            LockRequest* req = lock_obj->wait_queue_head;
            LockRequest* prev = nullptr;

            while (req) {
                if (!checkConflictInternal(lock_obj, req->mode, req->proc_id)) {
                    // Can grant this lock
                    uint8_t mode_idx = static_cast<uint8_t>(req->mode) - 1;
                    lock_obj->granted_mask |= (1u << mode_idx);
                    lock_obj->granted_counts[mode_idx]++;

                    req->granted = true;
                    granted_any = true;

                    // Wake up waiter
                    lock_wait_cv_.notify_all();

                    prev = req;
                    req = req->next;
                } else {
                    prev = req;
                    req = req->next;
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

    LockRequest* LockManager::allocateRequest()
    {
        if (!request_pool_.empty()) {
            LockRequest* req = request_pool_.back();
            request_pool_.pop_back();
            return req;
        }

        try {
            return new LockRequest();
        } catch (const std::bad_alloc&) {
            return nullptr;
        }
    }

    void LockManager::freeRequest(LockRequest* req)
    {
        if (request_pool_.size() < 1000) {
            request_pool_.push_back(req);
        } else {
            delete req;
        }
    }

    void LockManager::enqueueRequest(Lock* lock_obj, LockRequest* req)
    {
        req->next = nullptr;
        req->prev = lock_obj->wait_queue_tail;

        if (lock_obj->wait_queue_tail) {
            lock_obj->wait_queue_tail->next = req;
        } else {
            lock_obj->wait_queue_head = req;
        }

        lock_obj->wait_queue_tail = req;
        lock_obj->wait_queue_size++;
    }

    void LockManager::dequeueRequest(Lock* lock_obj, LockRequest* req)
    {
        if (req->prev) {
            req->prev->next = req->next;
        } else {
            lock_obj->wait_queue_head = req->next;
        }

        if (req->next) {
            req->next->prev = req->prev;
        } else {
            lock_obj->wait_queue_tail = req->prev;
        }

        lock_obj->wait_queue_size--;
    }

    Lock* LockManager::allocateLock()
    {
        if (!lock_pool_.empty()) {
            Lock* lock_obj = lock_pool_.back();
            lock_pool_.pop_back();
            return lock_obj;
        }

        try {
            return new Lock();
        } catch (const std::bad_alloc&) {
            return nullptr;
        }
    }

    void LockManager::freeLock(Lock* lock_obj)
    {
        if (lock_pool_.size() < 1000) {
            lock_pool_.push_back(lock_obj);
        } else {
            delete lock_obj;
        }
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
        // TODO: Build wait-for graph by scanning lock manager state
        // For each lock with waiters:
        //   For each waiter W:
        //     For each holder H:
        //       Add edge W -> H
        // This requires access to lock manager internals
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
        // Select youngest transaction (highest XID) as victim
        // TODO: Get XIDs from ProcArray
        // For now, just return first process
        return cycle.empty() ? 0 : cycle[0];
    }

    auto DeadlockDetector::abortTransaction(uint32_t proc_id, ErrorContext* ctx) -> Status
    {
        // TODO: Abort transaction by:
        // 1. Release all locks held by proc_id
        // 2. Mark transaction as aborted in TransactionManager
        // 3. Wake up any waiters

        // For now, just release locks
        if (lock_mgr_) {
            return lock_mgr_->releaseAllLocks(proc_id, ctx);
        }

        return Status::OK;
    }

} // namespace scratchbird::core
