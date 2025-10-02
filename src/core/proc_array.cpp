#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include <cstring>
#include <chrono>
#include <unistd.h>
#include <sys/mman.h>

namespace scratchbird::core
{
    // Static member initialization
    ProcArray* ProcArrayManager::proc_array_ = nullptr;
    Database* ProcArrayManager::database_ = nullptr;

    auto ProcArrayManager::initialize(Database* db, uint32_t max_backends,
                                     ErrorContext* ctx) -> Status
    {
        if (!db) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is null");
            return Status::INVALID_ARGUMENT;
        }

        if (proc_array_ != nullptr) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray already initialized");
            return Status::INVALID_ARGUMENT;
        }

        database_ = db;

        // Calculate total size needed
        size_t header_size = sizeof(ProcArray);
        size_t pcb_array_size = sizeof(ProcessControlBlock) * max_backends;
        size_t total_size = header_size + pcb_array_size;

        // Allocate shared memory (mmap with MAP_SHARED)
        void* shared_mem = mmap(nullptr, total_size,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        if (shared_mem == MAP_FAILED) {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate shared memory for ProcArray");
            return Status::OOM;
        }

        // Initialize ProcArray header
        proc_array_ = static_cast<ProcArray*>(shared_mem);
        std::memset(proc_array_, 0, total_size);

        proc_array_->max_backends = max_backends;
        proc_array_->latest_completed_xid = 0;
        proc_array_->oldest_xmin = 0;
        proc_array_->first_free = 0;
        proc_array_->num_active = 0;

        // Initialize read-write lock
        pthread_rwlockattr_t rwlock_attr;
        pthread_rwlockattr_init(&rwlock_attr);
        pthread_rwlockattr_setpshared(&rwlock_attr, PTHREAD_PROCESS_SHARED);
        pthread_rwlock_init(&proc_array_->array_lock, &rwlock_attr);
        pthread_rwlockattr_destroy(&rwlock_attr);

        // Initialize mutex
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&proc_array_->alloc_lock, &mutex_attr);
        pthread_mutexattr_destroy(&mutex_attr);

        // Initialize PCB array (comes after header)
        auto* pcbs = reinterpret_cast<ProcessControlBlock*>(
            reinterpret_cast<uint8_t*>(proc_array_) + sizeof(ProcArray));

        // Build free list
        for (uint32_t i = 0; i < max_backends; ++i) {
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

        return Status::OK;
    }

    auto ProcArrayManager::shutdown(ErrorContext* ctx) -> Status
    {
        if (proc_array_ == nullptr) {
            return Status::OK;
        }

        // Calculate total size
        size_t header_size = sizeof(ProcArray);
        size_t pcb_array_size = sizeof(ProcessControlBlock) * proc_array_->max_backends;
        size_t total_size = header_size + pcb_array_size;

        // Destroy synchronization primitives
        pthread_rwlock_destroy(&proc_array_->array_lock);
        pthread_mutex_destroy(&proc_array_->alloc_lock);

        // Unmap shared memory
        if (munmap(proc_array_, total_size) != 0) {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to unmap ProcArray");
            return Status::IO_ERROR;
        }

        proc_array_ = nullptr;
        database_ = nullptr;

        return Status::OK;
    }

    auto ProcArrayManager::registerBackend(uint32_t* proc_id_out,
                                          ErrorContext* ctx) -> Status
    {
        if (!proc_array_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (!proc_id_out) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "proc_id_out is null");
            return Status::INVALID_ARGUMENT;
        }

        pthread_mutex_lock(&proc_array_->alloc_lock);

        // Find free slot
        uint32_t proc_id = 0;
        bool found = false;

        auto* pcbs = reinterpret_cast<ProcessControlBlock*>(
            reinterpret_cast<uint8_t*>(proc_array_) + sizeof(ProcArray));

        for (uint32_t i = 0; i < proc_array_->max_backends; ++i) {
            if (!pcbs[i].is_active) {
                proc_id = i;
                found = true;
                break;
            }
        }

        if (!found) {
            pthread_mutex_unlock(&proc_array_->alloc_lock);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "No free backend slots");
            return Status::PAGE_FULL;
        }

        // Initialize PCB
        ProcessControlBlock* pcb = &pcbs[proc_id];
        pcb->is_active = true;
        pcb->backend_pid = getpid();
        pcb->xid = 0;
        pcb->backend_xmin = 0;
        pcb->xmin = 0;
        pcb->wait_lock_id = 0;
        pcb->deadlock_check_pending = false;
        pcb->start_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        pcb->query_start_time = 0;

        proc_array_->num_active++;

        pthread_mutex_unlock(&proc_array_->alloc_lock);

        *proc_id_out = proc_id;
        return Status::OK;
    }

    auto ProcArrayManager::unregisterBackend(uint32_t proc_id,
                                            ErrorContext* ctx) -> Status
    {
        if (!proc_array_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (proc_id >= proc_array_->max_backends) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid proc_id");
            return Status::INVALID_ARGUMENT;
        }

        pthread_mutex_lock(&proc_array_->alloc_lock);

        ProcessControlBlock* pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active) {
            pthread_mutex_unlock(&proc_array_->alloc_lock);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Backend not active");
            return Status::INVALID_ARGUMENT;
        }

        // Clear PCB
        std::memset(pcb, 0, sizeof(ProcessControlBlock));
        pcb->proc_id = proc_id;
        pcb->is_active = false;

        proc_array_->num_active--;

        pthread_mutex_unlock(&proc_array_->alloc_lock);

        return Status::OK;
    }

    auto ProcArrayManager::setTransactionId(uint32_t proc_id, uint64_t xid,
                                           ErrorContext* ctx) -> Status
    {
        if (!proc_array_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock* pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&proc_array_->array_lock);

        pcb->xid = xid;

        // Update global latest completed XID if this is higher
        if (xid > proc_array_->latest_completed_xid) {
            proc_array_->latest_completed_xid = xid;
        }

        pthread_rwlock_unlock(&proc_array_->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::clearTransactionId(uint32_t proc_id,
                                             ErrorContext* ctx) -> Status
    {
        if (!proc_array_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock* pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&proc_array_->array_lock);

        uint64_t xid = pcb->xid;
        pcb->xid = 0;

        // Update latest completed XID
        if (xid > proc_array_->latest_completed_xid) {
            proc_array_->latest_completed_xid = xid;
        }

        pthread_rwlock_unlock(&proc_array_->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::getActiveTransactions(std::vector<uint64_t>* xids_out,
                                                 uint64_t* oldest_xmin_out,
                                                 ErrorContext* ctx) -> Status
    {
        if (!proc_array_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (!xids_out || !oldest_xmin_out) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Output parameters are null");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_rdlock(&proc_array_->array_lock);

        xids_out->clear();
        uint64_t oldest_xmin = UINT64_MAX;

        auto* pcbs = reinterpret_cast<ProcessControlBlock*>(
            reinterpret_cast<uint8_t*>(proc_array_) + sizeof(ProcArray));

        // Scan all active backends
        for (uint32_t i = 0; i < proc_array_->max_backends; ++i) {
            if (!pcbs[i].is_active) {
                continue;
            }

            // Collect active transaction XIDs
            if (pcbs[i].xid != 0) {
                xids_out->push_back(pcbs[i].xid);

                if (pcbs[i].xid < oldest_xmin) {
                    oldest_xmin = pcbs[i].xid;
                }
            }

            // Consider backend's xmin (snapshot horizon)
            if (pcbs[i].backend_xmin != 0 && pcbs[i].backend_xmin < oldest_xmin) {
                oldest_xmin = pcbs[i].backend_xmin;
            }
        }

        pthread_rwlock_unlock(&proc_array_->array_lock);

        if (oldest_xmin == UINT64_MAX) {
            // No active transactions
            oldest_xmin = proc_array_->latest_completed_xid;
        }

        *oldest_xmin_out = oldest_xmin;

        return Status::OK;
    }

    auto ProcArrayManager::getVacuumHorizon(uint64_t* horizon_out,
                                           ErrorContext* ctx) -> Status
    {
        if (!proc_array_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (!horizon_out) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "horizon_out is null");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_rdlock(&proc_array_->array_lock);

        uint64_t oldest_xmin = UINT64_MAX;

        auto* pcbs = reinterpret_cast<ProcessControlBlock*>(
            reinterpret_cast<uint8_t*>(proc_array_) + sizeof(ProcArray));

        // Scan all active backends for oldest xmin
        for (uint32_t i = 0; i < proc_array_->max_backends; ++i) {
            if (!pcbs[i].is_active) {
                continue;
            }

            // Consider backend's xmin (snapshot horizon)
            if (pcbs[i].backend_xmin != 0 && pcbs[i].backend_xmin < oldest_xmin) {
                oldest_xmin = pcbs[i].backend_xmin;
            }

            // Consider active transaction
            if (pcbs[i].xid != 0 && pcbs[i].xid < oldest_xmin) {
                oldest_xmin = pcbs[i].xid;
            }
        }

        pthread_rwlock_unlock(&proc_array_->array_lock);

        if (oldest_xmin == UINT64_MAX) {
            // No active transactions, use latest completed
            oldest_xmin = proc_array_->latest_completed_xid;
        }

        *horizon_out = oldest_xmin;

        return Status::OK;
    }

    auto ProcArrayManager::getBackendXmin(uint32_t proc_id, uint64_t* xmin_out,
                                         ErrorContext* ctx) -> Status
    {
        if (!proc_array_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock* pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_rdlock(&proc_array_->array_lock);
        *xmin_out = pcb->backend_xmin;
        pthread_rwlock_unlock(&proc_array_->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::setBackendXmin(uint32_t proc_id, uint64_t xmin,
                                         ErrorContext* ctx) -> Status
    {
        if (!proc_array_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        ProcessControlBlock* pcb = getPCB(proc_id);
        if (!pcb || !pcb->is_active) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid or inactive backend");
            return Status::INVALID_ARGUMENT;
        }

        pthread_rwlock_wrlock(&proc_array_->array_lock);
        pcb->backend_xmin = xmin;
        pthread_rwlock_unlock(&proc_array_->array_lock);

        return Status::OK;
    }

    auto ProcArrayManager::getNumActiveBackends(uint32_t* count_out,
                                               ErrorContext* ctx) -> Status
    {
        if (!proc_array_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray not initialized");
            return Status::INVALID_ARGUMENT;
        }

        if (!count_out) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "count_out is null");
            return Status::INVALID_ARGUMENT;
        }

        pthread_mutex_lock(&proc_array_->alloc_lock);
        *count_out = proc_array_->num_active;
        pthread_mutex_unlock(&proc_array_->alloc_lock);

        return Status::OK;
    }

    auto ProcArrayManager::getInstance() -> ProcArray*
    {
        return proc_array_;
    }

    auto ProcArrayManager::getPCB(uint32_t proc_id) -> ProcessControlBlock*
    {
        if (!proc_array_ || proc_id >= proc_array_->max_backends) {
            return nullptr;
        }

        auto* pcbs = reinterpret_cast<ProcessControlBlock*>(
            reinterpret_cast<uint8_t*>(proc_array_) + sizeof(ProcArray));

        return &pcbs[proc_id];
    }

} // namespace scratchbird::core
