/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <unistd.h>
#include <mutex>
#if !defined(_WIN32)
    #include <sys/mman.h>
#endif

namespace scratchbird::core
{
    namespace
    {
#if defined(_WIN32)
        void* allocateProcArraySharedMemory(size_t size)
        {
            return std::malloc(size);
        }

        int releaseProcArraySharedMemory(void* ptr, size_t /*size*/)
        {
            std::free(ptr);
            return 0;
        }
#else
        void* allocateProcArraySharedMemory(size_t size)
        {
            return mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        }

        int releaseProcArraySharedMemory(void* ptr, size_t size)
        {
            return munmap(ptr, size);
        }
#endif
    } // namespace

    // Static member initialization
    std::atomic<ProcArray *> ProcArrayManager::proc_array_{nullptr};
    Database *ProcArrayManager::database_ = nullptr;

    // Thread-safe initialization guard for static singleton
    static std::mutex init_mutex_;

    auto ProcArrayManager::initialize(Database *db, uint32_t max_backends, ErrorContext *ctx)
        -> Status
    {
        if (!db)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is null");
            return Status::INVALID_ARGUMENT;
        }

        // Thread-safe initialization guard
        // Prevents concurrent initialization race during test setup
        std::lock_guard<std::mutex> guard(init_mutex_);

        if (proc_array_.load(std::memory_order_acquire) != nullptr)
        {
            // Already initialized by another thread, reuse existing instance
            // This is safe for testing where multiple Database instances may exist
            return Status::OK;
        }

        database_ = db;

        // Calculate total size needed
        size_t header_size = sizeof(ProcArray);
        size_t pcb_array_size = sizeof(ProcessControlBlock) * max_backends;
        size_t total_size = header_size + pcb_array_size;

        // Allocate shared state backing memory.
        void *shared_mem = allocateProcArraySharedMemory(total_size);

#if defined(_WIN32)
        if (shared_mem == nullptr)
#else
        if (shared_mem == MAP_FAILED)
#endif
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate shared memory for ProcArray");
            return Status::OOM;
        }

        // Use temporary pointer for initialization to prevent race
        // Other threads must not see proc_array_ != nullptr until fully initialized
        ProcArray *temp_array = static_cast<ProcArray *>(shared_mem);
        std::memset(temp_array, 0, total_size);

        temp_array->max_backends = max_backends;
        temp_array->latest_completed_xid = 0;
        temp_array->oldest_xmin = 0;
        temp_array->first_free = 0;
        temp_array->num_active = 0;

        // Initialize read-write lock
        pthread_rwlockattr_t rwlock_attr;
        pthread_rwlockattr_init(&rwlock_attr);
        pthread_rwlockattr_setpshared(&rwlock_attr, PTHREAD_PROCESS_SHARED);
        pthread_rwlock_init(&temp_array->array_lock, &rwlock_attr);
        pthread_rwlockattr_destroy(&rwlock_attr);

        // Initialize mutex
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&temp_array->alloc_lock, &mutex_attr);
        pthread_mutexattr_destroy(&mutex_attr);

        // Initialize PCB array (comes after header)
        auto *pcbs = reinterpret_cast<ProcessControlBlock *>(
            reinterpret_cast<uint8_t *>(temp_array) + sizeof(ProcArray));

        // Build free list
        for (uint32_t i = 0; i < max_backends; ++i)
        {
            std::memset(&pcbs[i], 0, sizeof(ProcessControlBlock));
            pcbs[i].proc_id = i;
            pcbs[i].is_active = false;
            pcbs[i].backend_pid = 0;
            pcbs[i].xid = 0;
            pcbs[i].backend_xmin = 0;
            pcbs[i].xmin = 0;
            pcbs[i].wait_lock_id = 0;
            pcbs[i].deadlock_check_pending = false;
            pcbs[i].start_time = 0;
            pcbs[i].query_start_time = 0;
        }

        // CRITICAL: Only publish proc_array_ after full initialization
        // This prevents other threads from seeing partially initialized state
        // Use release ordering to ensure all writes above are visible before publishing
        proc_array_.store(temp_array, std::memory_order_release);

        return Status::OK;
    }

    auto ProcArrayManager::shutdown(ErrorContext *ctx) -> Status
    {
        // Thread-safe shutdown guard
        std::lock_guard<std::mutex> guard(init_mutex_);

        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (array == nullptr)
        {
            return Status::OK;
        }

        // Calculate total size
        size_t header_size = sizeof(ProcArray);
        size_t pcb_array_size = sizeof(ProcessControlBlock) * array->max_backends;
        size_t total_size = header_size + pcb_array_size;

        // Destroy synchronization primitives
        pthread_rwlock_destroy(&array->array_lock);
        pthread_mutex_destroy(&array->alloc_lock);

        // Release shared memory
        if (releaseProcArraySharedMemory(array, total_size) != 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to unmap ProcArray");
            return Status::IO_ERROR;
        }

        proc_array_.store(nullptr, std::memory_order_release);
        database_ = nullptr;

        return Status::OK;
    }

    auto ProcArrayManager::registerBackend(uint32_t *proc_id_out, ErrorContext *ctx) -> Status
    {
        // Use acquire ordering to synchronize with the release in initialize()
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (!proc_id_out)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "proc_id_out is null");
            return Status::INVALID_ARGUMENT;
        }

        pthread_mutex_lock(&array->alloc_lock);

        // Find free slot
        uint32_t proc_id = 0;
        bool found = false;

        auto *pcbs = reinterpret_cast<ProcessControlBlock *>(
            reinterpret_cast<uint8_t *>(array) + sizeof(ProcArray));

        for (uint32_t i = 0; i < array->max_backends; ++i)
        {
            if (!pcbs[i].is_active)
            {
                proc_id = i;
                found = true;
                break;
            }
        }

        if (!found)
        {
            pthread_mutex_unlock(&array->alloc_lock);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "No free backend slots");
            return Status::PAGE_FULL;
        }

        pthread_rwlock_wrlock(&array->array_lock);
        // Initialize PCB
        ProcessControlBlock *pcb = &pcbs[proc_id];
        pcb->is_active = true;
        pcb->backend_pid = currentProcessId();
        pcb->xid = 0;
        pcb->backend_xmin = 0;
        pcb->xmin = 0;
        pcb->wait_lock_id = 0;
        pcb->deadlock_check_pending = false;
        pcb->start_time = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
        pcb->query_start_time = 0;
        pcb->state_change_time = pcb->start_time;
        pcb->termination_requested = false;
        pcb->session_id = ID{};
        std::memset(pcb->query_text, 0, sizeof(pcb->query_text));

        array->num_active++;
        pthread_rwlock_unlock(&array->array_lock);

        pthread_mutex_unlock(&array->alloc_lock);

        *proc_id_out = proc_id;
        return Status::OK;
    }

    auto ProcArrayManager::unregisterBackend(uint32_t proc_id, ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (proc_id >= array->max_backends)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid proc_id");
            return Status::INVALID_ARGUMENT;
        }

        pthread_mutex_lock(&array->alloc_lock);

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            pthread_mutex_unlock(&array->alloc_lock);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Backend not active");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);
        // Clear PCB
        std::memset(pcb, 0, sizeof(ProcessControlBlock));
        pcb->proc_id = proc_id;
        pcb->is_active = false;

        array->num_active--;
        pthread_rwlock_unlock(&array->array_lock);

        pthread_mutex_unlock(&array->alloc_lock);

        return Status::OK;
    }

    auto ProcArrayManager::setTransactionId(uint32_t proc_id, uint64_t xid, ErrorContext *ctx)
        -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);

        pcb->xid = xid;

        // Update global latest completed XID if this is higher
        if (xid > array->latest_completed_xid)
        {
            array->latest_completed_xid = xid;
        }

        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::clearTransactionId(uint32_t proc_id, ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);

        uint64_t xid = pcb->xid;
        pcb->xid = 0;

        // Update latest completed XID
        if (xid > array->latest_completed_xid)
        {
            array->latest_completed_xid = xid;
        }

        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::setIsolationLevel(uint32_t proc_id, uint8_t isolation_level,
                                             ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);

        pcb->isolation_level = isolation_level;
        // IsolationLevel: 0=READ_COMMITTED, 1=READ_COMMITTED_READ_CONSISTENCY, 2=SNAPSHOT,
        // 3=SNAPSHOT_TABLE_STABILITY SNAPSHOT modes are 2 and 3
        pcb->is_snapshot_txn = (isolation_level >= 2);

        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::setTransactionReadOnly(uint32_t proc_id, bool is_read_only,
                                                  ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);
        pcb->is_read_only = is_read_only;
        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::setTransactionStartTime(uint32_t proc_id, uint64_t start_time,
                                                   ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);
        pcb->xact_start_time = start_time;
        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::setSessionId(uint32_t proc_id, const ID& session_id,
                                        ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);
        pcb->session_id = session_id;
        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::setQueryInfo(uint32_t proc_id, uint64_t start_time,
                                        const std::string& query_text,
                                        ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);
        pcb->query_start_time = start_time;
        pcb->state_change_time = start_time;
        std::memset(pcb->query_text, 0, sizeof(pcb->query_text));
        if (!query_text.empty())
        {
            std::strncpy(pcb->query_text, query_text.c_str(), sizeof(pcb->query_text) - 1);
            pcb->query_text[sizeof(pcb->query_text) - 1] = '\0';
        }
        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::clearQueryInfo(uint32_t proc_id, uint64_t state_change_time,
                                          ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);
        pcb->query_start_time = 0;
        pcb->state_change_time = state_change_time;
        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::getActiveTransactions(std::vector<uint64_t> *xids_out,
                                                 uint64_t *oldest_xmin_out, ErrorContext *ctx)
        -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (!xids_out || !oldest_xmin_out)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Output parameters are null");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_rdlock(&array->array_lock);

        xids_out->clear();
        uint64_t oldest_xmin = UINT64_MAX;

        auto *pcbs = reinterpret_cast<ProcessControlBlock *>(
            reinterpret_cast<uint8_t *>(array) + sizeof(ProcArray));

        // Scan all active backends
        for (uint32_t i = 0; i < array->max_backends; ++i)
        {
            if (!pcbs[i].is_active)
            {
                continue;
            }

            // Collect active transaction XIDs
            if (pcbs[i].xid != 0)
            {
                xids_out->push_back(pcbs[i].xid);

                if (pcbs[i].xid < oldest_xmin)
                {
                    oldest_xmin = pcbs[i].xid;
                }
            }

            // Consider backend's xmin (snapshot horizon)
            if (pcbs[i].backend_xmin != 0 && pcbs[i].backend_xmin < oldest_xmin)
            {
                oldest_xmin = pcbs[i].backend_xmin;
            }
        }

        pthread_rwlock_unlock(&array->array_lock);

        if (oldest_xmin == UINT64_MAX)
        {
            // No active transactions
            oldest_xmin = array->latest_completed_xid;
        }

        *oldest_xmin_out = oldest_xmin;

        return Status::OK;
    }

    auto ProcArrayManager::getGcHorizon(uint64_t *horizon_out, ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (!horizon_out)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "horizon_out is null");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_rdlock(&array->array_lock);

        uint64_t oldest_xmin = UINT64_MAX;

        auto *pcbs = reinterpret_cast<ProcessControlBlock *>(
            reinterpret_cast<uint8_t *>(array) + sizeof(ProcArray));

        // Scan all active backends for oldest xmin
        for (uint32_t i = 0; i < array->max_backends; ++i)
        {
            if (!pcbs[i].is_active)
            {
                continue;
            }

            // Consider backend's xmin (snapshot horizon)
            if (pcbs[i].backend_xmin != 0 && pcbs[i].backend_xmin < oldest_xmin)
            {
                oldest_xmin = pcbs[i].backend_xmin;
            }

            // Consider active transaction
            if (pcbs[i].xid != 0 && pcbs[i].xid < oldest_xmin)
            {
                oldest_xmin = pcbs[i].xid;
            }
        }

        pthread_rwlock_unlock(&array->array_lock);

        if (oldest_xmin == UINT64_MAX)
        {
            // No active transactions, use latest completed
            oldest_xmin = array->latest_completed_xid;
        }

        *horizon_out = oldest_xmin;

        return Status::OK;
    }

    auto ProcArrayManager::getBackendXmin(uint32_t proc_id, uint64_t *xmin_out, ErrorContext *ctx)
        -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_rdlock(&array->array_lock);
        *xmin_out = pcb->backend_xmin;
        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::setBackendXmin(uint32_t proc_id, uint64_t xmin, ErrorContext *ctx)
        -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);
        pcb->backend_xmin = xmin;
        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::getBackendXid(uint32_t proc_id, uint64_t *xid_out, ErrorContext *ctx)
        -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (!xid_out)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "xid_out is null");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            // For inactive backends, return 0 (no transaction)
            *xid_out = 0;
            return Status::OK;
        }

        pthread_rwlock_rdlock(&array->array_lock);
        *xid_out = pcb->xid;
        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::getNumActiveBackends(uint32_t *count_out, ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (!count_out)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "count_out is null");
            return Status::INVALID_ARGUMENT;
        }

        pthread_mutex_lock(&array->alloc_lock);
        *count_out = array->num_active;
        pthread_mutex_unlock(&array->alloc_lock);

        return Status::OK;
    }

    auto ProcArrayManager::getAllActiveBackends(std::vector<ProcessControlBlock> *backends_out,
                                                ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (!backends_out)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "backends_out is null");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_rdlock(&array->array_lock);

        backends_out->clear();

        auto *pcbs = reinterpret_cast<ProcessControlBlock *>(
            reinterpret_cast<uint8_t *>(array) + sizeof(ProcArray));

        // Collect all active backends
        for (uint32_t i = 0; i < array->max_backends; ++i)
        {
            if (pcbs[i].is_active)
            {
                backends_out->push_back(pcbs[i]);
            }
        }

        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::getInstance() -> ProcArray *
    {
        return proc_array_.load(std::memory_order_acquire);
    }

    auto ProcArrayManager::getPCB(uint32_t proc_id) -> ProcessControlBlock *
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array || proc_id >= array->max_backends)
        {
            return nullptr;
        }

        auto *pcbs = reinterpret_cast<ProcessControlBlock *>(
            reinterpret_cast<uint8_t *>(array) + sizeof(ProcArray));

        return &pcbs[proc_id];
    }

    auto ProcArrayManager::requestBackendTermination(uint32_t proc_id, ErrorContext *ctx)
        -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);
        pcb->termination_requested = true;
        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::isTerminationRequested(uint32_t proc_id, bool *requested_out,
                                                  ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (!requested_out)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "requested_out is null");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_rdlock(&array->array_lock);
        *requested_out = pcb->termination_requested;
        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::clearTerminationRequest(uint32_t proc_id, ErrorContext *ctx) -> Status
    {
        ProcArray *array = proc_array_.load(std::memory_order_acquire);
        if (!array)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock *pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&array->array_lock);
        pcb->termination_requested = false;
        pthread_rwlock_unlock(&array->array_lock);

        return Status::OK;
    }

} // namespace scratchbird::core
