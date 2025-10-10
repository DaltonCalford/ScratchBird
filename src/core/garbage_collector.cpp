#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/logger.h"
#include <chrono>
#include <thread>

namespace scratchbird::core
{
    GarbageCollector::GarbageCollector(Database* db)
        : db_(db)
        , txn_manager_(nullptr)
        , storage_engine_(nullptr)
        , policy_(GCPolicy::COMBINED)
        , enabled_(true)
        , background_running_(false)
        , shutdown_requested_(false)
    {
    }

    GarbageCollector::~GarbageCollector()
    {
        // Stop background GC if running
        if (background_running_.load(std::memory_order_acquire))
        {
            shutdown_requested_.store(true, std::memory_order_release);

            if (background_thread_.joinable())
            {
                background_thread_.join();
            }
        }
    }

    Status GarbageCollector::initialize(ErrorContext* ctx)
    {
        if (!db_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is null");
            return Status::INVALID_ARGUMENT;
        }

        txn_manager_ = db_->transaction_manager();
        storage_engine_ = db_->storage_engine();

        if (!txn_manager_ || !storage_engine_)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "TransactionManager or StorageEngine not available");
            return Status::IO_ERROR;
        }

        LOG_INFO(VACUUM, "GarbageCollector initialized with policy: %s",
                 policy_ == GCPolicy::COOPERATIVE ? "COOPERATIVE" :
                 policy_ == GCPolicy::BACKGROUND ? "BACKGROUND" : "COMBINED");

        return Status::OK;
    }

    void GarbageCollector::processPageCooperative(uint32_t page_id, ErrorContext* ctx)
    {
        // Check if cooperative GC is enabled
        if (!enabled_.load(std::memory_order_acquire))
        {
            return;
        }

        if (policy_ == GCPolicy::BACKGROUND)
        {
            return;  // Cooperative disabled in BACKGROUND-only mode
        }

        // Rate limiting - don't run on every page read
        if (!shouldRunCooperativeGC())
        {
            return;
        }

        // Perform cooperative cleanup
        cleanPage(page_id, ctx);

        // Update statistics
        updateCooperativeStats(0, 1);  // Will be updated with actual counts later
    }

    Status GarbageCollector::startBackgroundGC(ErrorContext* ctx)
    {
        // Check if already running
        if (background_running_.load(std::memory_order_acquire))
        {
            LOG_WARNING(VACUUM, "Background GC already running");
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Background GC already running");
            return Status::IO_ERROR;
        }

        // Check if background GC is enabled by policy
        if (policy_ == GCPolicy::COOPERATIVE)
        {
            LOG_WARNING(VACUUM, "Cannot start background GC in COOPERATIVE-only mode");
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Background GC not enabled by policy");
            return Status::IO_ERROR;
        }

        // Start background thread
        shutdown_requested_.store(false, std::memory_order_release);
        background_running_.store(true, std::memory_order_release);

        background_thread_ = std::thread(&GarbageCollector::backgroundGCLoop, this);

        LOG_INFO(VACUUM, "Background GC thread started");
        return Status::OK;
    }

    Status GarbageCollector::stopBackgroundGC(ErrorContext* ctx)
    {
        if (!background_running_.load(std::memory_order_acquire))
        {
            LOG_WARNING(VACUUM, "Background GC not running");
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Background GC not running");
            return Status::IO_ERROR;
        }

        // Signal shutdown
        shutdown_requested_.store(true, std::memory_order_release);

        // Wait for thread to finish
        if (background_thread_.joinable())
        {
            background_thread_.join();
        }

        background_running_.store(false, std::memory_order_release);

        LOG_INFO(VACUUM, "Background GC thread stopped");
        return Status::OK;
    }

    bool GarbageCollector::isBackgroundGCRunning() const
    {
        return background_running_.load(std::memory_order_acquire);
    }

    void GarbageCollector::markPageDirty(uint32_t page_id)
    {
        std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
        dirty_pages_.insert(page_id);
    }

    size_t GarbageCollector::getDirtyPageCount() const
    {
        std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
        return dirty_pages_.size();
    }

    void GarbageCollector::setPolicy(GCPolicy policy)
    {
        policy_ = policy;
        LOG_INFO(VACUUM, "GC policy changed to: %s",
                 policy == GCPolicy::COOPERATIVE ? "COOPERATIVE" :
                 policy == GCPolicy::BACKGROUND ? "BACKGROUND" : "COMBINED");
    }

    GCPolicy GarbageCollector::getPolicy() const
    {
        return policy_;
    }

    void GarbageCollector::enable()
    {
        enabled_.store(true, std::memory_order_release);
        LOG_INFO(VACUUM, "GarbageCollector enabled");
    }

    void GarbageCollector::disable()
    {
        enabled_.store(false, std::memory_order_release);
        LOG_INFO(VACUUM, "GarbageCollector disabled");
    }

    bool GarbageCollector::isEnabled() const
    {
        return enabled_.load(std::memory_order_acquire);
    }

    GCStatistics GarbageCollector::getStatistics() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);

        // Update dirty page count in statistics
        GCStatistics stats = stats_;
        stats.dirty_page_count = getDirtyPageCount();

        return stats;
    }

    void GarbageCollector::notifySweepComplete(uint64_t old_oit, uint64_t new_oit)
    {
        // Sweep has advanced OIT - more tuples may now be garbage
        LOG_INFO(VACUUM, "Sweep completed: OIT advanced from %lu to %lu", old_oit, new_oit);

        // If background GC is enabled, wake it to process newly-identified garbage
        if (policy_ == GCPolicy::BACKGROUND || policy_ == GCPolicy::COMBINED)
        {
            wakeBackgroundThread();
        }
    }

    // Private methods

    void GarbageCollector::backgroundGCLoop()
    {
        LOG_INFO(VACUUM, "Background GC loop started");

        while (!shutdown_requested_.load(std::memory_order_acquire))
        {
            auto start_time = std::chrono::steady_clock::now();

            // TODO: Implement actual background GC logic
            // For now, just sleep

            // Get dirty pages to clean
            std::vector<uint32_t> pages_to_clean;
            {
                std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
                pages_to_clean.assign(dirty_pages_.begin(), dirty_pages_.end());
            }

            uint64_t tuples_removed = 0;
            uint64_t pages_cleaned = 0;

            // Clean each dirty page
            for (uint32_t page_id : pages_to_clean)
            {
                if (shutdown_requested_.load(std::memory_order_acquire))
                {
                    break;
                }

                ErrorContext err_ctx;
                cleanPage(page_id, &err_ctx);
                pages_cleaned++;
            }

            // Update statistics
            auto end_time = std::chrono::steady_clock::now();
            uint64_t duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count();

            updateBackgroundStats(tuples_removed, pages_cleaned, duration_ms);

            // Sleep before next pass (5 seconds default)
            // TODO: Read from config
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }

        LOG_INFO(VACUUM, "Background GC loop stopped");
    }

    void GarbageCollector::cleanPage(uint32_t page_id, ErrorContext* ctx)
    {
        // TODO: Implement actual page cleaning logic
        // For now, just mark as no longer dirty

        std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
        dirty_pages_.erase(page_id);

        (void)ctx;  // Suppress unused parameter warning
    }

    bool GarbageCollector::isTupleGarbage(uint64_t xmax, uint64_t oit)
    {
        // TODO: Implement actual garbage detection
        // Tuple is garbage if:
        // 1. It has been deleted/updated (xmax != INVALID_XID)
        // 2. The deleting transaction is old (xmax < OIT)
        // 3. The transaction committed

        constexpr uint64_t INVALID_XID = 0;

        if (xmax == INVALID_XID)
        {
            return false;  // Still visible
        }

        if (xmax >= oit)
        {
            return false;  // Deleting transaction too new
        }

        // TODO: Check CLOG to verify transaction committed

        return true;  // Potentially garbage
    }

    void GarbageCollector::updateCooperativeStats(uint64_t tuples_removed, uint64_t pages_cleaned)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.tuples_removed += tuples_removed;
        stats_.pages_cleaned += pages_cleaned;
        stats_.cooperative_runs++;
    }

    void GarbageCollector::updateBackgroundStats(uint64_t tuples_removed, uint64_t pages_cleaned,
                                                   uint64_t duration_ms)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.tuples_removed += tuples_removed;
        stats_.pages_cleaned += pages_cleaned;
        stats_.background_runs++;
        stats_.last_background_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        stats_.last_background_duration_ms = duration_ms;
    }

    void GarbageCollector::wakeBackgroundThread()
    {
        // TODO: Implement proper wake mechanism (condition variable)
        // For now, the thread will wake on its own periodic interval
    }

    bool GarbageCollector::shouldRunCooperativeGC()
    {
        // Simple rate limiting - run on ~1% of page reads
        // Use thread-local counter to avoid contention
        static thread_local uint32_t counter = 0;
        counter++;

        // TODO: Read rate from config
        const uint32_t cooperative_rate = 100;  // 1 in 100 page reads

        return (counter % cooperative_rate) == 0;
    }

} // namespace scratchbird::core
