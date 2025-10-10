#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/config.h"
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
        , background_interval_ms_(5000)
        , cooperative_rate_(100)
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

            // Wake the background thread so it can exit
            bg_wake_cv_.notify_one();

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

        // Read configuration
        readConfiguration();

        LOG_INFO(VACUUM, "GarbageCollector initialized with policy: %s, interval: %lums, rate: 1/%u",
                 policy_ == GCPolicy::COOPERATIVE ? "COOPERATIVE" :
                 policy_ == GCPolicy::BACKGROUND ? "BACKGROUND" : "COMBINED",
                 background_interval_ms_, cooperative_rate_);

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
        uint64_t space_reclaimed = 0;
        uint64_t tuples_removed = cleanPage(page_id, &space_reclaimed, ctx);

        // Update statistics
        updateCooperativeStats(tuples_removed, 1, space_reclaimed);
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

        // Wake the background thread so it can exit
        bg_wake_cv_.notify_one();

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

        // Track garbage accumulation rate
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_dirty_pages_marked++;
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

            // Get dirty pages to clean
            std::vector<uint32_t> pages_to_clean;
            {
                std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
                pages_to_clean.assign(dirty_pages_.begin(), dirty_pages_.end());
            }

            uint64_t tuples_removed = 0;
            uint64_t pages_cleaned = 0;
            uint64_t space_reclaimed = 0;

            // Clean each dirty page
            for (uint32_t page_id : pages_to_clean)
            {
                if (shutdown_requested_.load(std::memory_order_acquire))
                {
                    break;
                }

                ErrorContext err_ctx;
                uint64_t page_space_reclaimed = 0;
                uint64_t tuples_found = cleanPage(page_id, &page_space_reclaimed, &err_ctx);
                tuples_removed += tuples_found;
                space_reclaimed += page_space_reclaimed;
                pages_cleaned++;
            }

            // Update statistics
            auto end_time = std::chrono::steady_clock::now();
            uint64_t duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count();

            updateBackgroundStats(tuples_removed, pages_cleaned, space_reclaimed, duration_ms);

            // Wait for wake signal or timeout
            // Use condition variable for responsive wake on sweep completion
            std::unique_lock<std::mutex> lock(bg_wake_mutex_);
            bg_wake_cv_.wait_for(lock, std::chrono::milliseconds(background_interval_ms_),
                                 [this] { return shutdown_requested_.load(std::memory_order_acquire); });
        }

        LOG_INFO(VACUUM, "Background GC loop stopped");
    }

    uint64_t GarbageCollector::cleanPage(uint32_t page_id, uint64_t* space_reclaimed_out, ErrorContext* ctx)
    {
        // Get current OIT from TransactionManager
        uint64_t oit = txn_manager_->getOldestXid();

        // Pin the page through buffer pool
        void* page_buffer;
        Status s = db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(VACUUM, "Failed to pin page %u for GC: %d", page_id, static_cast<int>(s));
            if (space_reclaimed_out != nullptr)
            {
                *space_reclaimed_out = 0;
            }
            return 0;
        }

        // Get page header to check page type
        auto* page_header = reinterpret_cast<PageHeader*>(page_buffer);

        // Only process heap pages
        if (page_header->page_type != PAGE_TYPE_HEAP)
        {
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            if (space_reclaimed_out != nullptr)
            {
                *space_reclaimed_out = 0;
            }
            return 0;
        }

        // Use HeapPage::prunePage() for physical tuple removal
        HeapPage heap_page(reinterpret_cast<uint8_t*>(page_buffer), page_header->page_size);

        uint32_t tuples_pruned = 0;
        uint32_t space_reclaimed = 0;

        // Prune garbage tuples and defragment page
        Status prune_status = heap_page.prunePage(oit, &tuples_pruned, &space_reclaimed, ctx);

        bool page_modified = (tuples_pruned > 0);

        // Log results if we pruned any tuples
        if (tuples_pruned > 0)
        {
            LOG_INFO(VACUUM, "Page %u: pruned %u tuples, reclaimed %u bytes (OIT=%lu)",
                    page_id, tuples_pruned, space_reclaimed, oit);
        }

        // Unpin page (mark as dirty if we modified it)
        db_->buffer_pool()->unpinPage(page_id, page_modified, ctx);

        uint64_t garbage_tuples_found = tuples_pruned;

        // Return space reclaimed
        if (space_reclaimed_out != nullptr)
        {
            *space_reclaimed_out = space_reclaimed;
        }

        // Remove from dirty pages
        {
            std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
            dirty_pages_.erase(page_id);
        }

        return garbage_tuples_found;
    }

    bool GarbageCollector::isTupleGarbage(uint64_t xmax, uint64_t oit)
    {
        // Tuple is garbage if:
        // 1. It has been deleted/updated (xmax != INVALID_XID)
        // 2. The deleting transaction is old (xmax < OIT)
        // 3. The transaction committed

        constexpr uint64_t INVALID_XID = 0;

        // Not deleted/updated - still visible
        if (xmax == INVALID_XID)
        {
            return false;
        }

        // Deleting transaction too new - not yet garbage
        if (xmax >= oit)
        {
            return false;
        }

        // Check if deleting transaction committed
        TransactionState state;
        Status s = txn_manager_->getTransactionState(xmax, state, nullptr);

        if (s != Status::OK)
        {
            // Can't determine state - assume not garbage (conservative)
            return false;
        }

        // Only garbage if the deleting transaction committed
        // If it aborted, the tuple is still visible
        return (state == TransactionState::COMMITTED);
    }

    void GarbageCollector::updateCooperativeStats(uint64_t tuples_removed, uint64_t pages_cleaned, uint64_t space_reclaimed)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.tuples_removed += tuples_removed;
        stats_.pages_cleaned += pages_cleaned;
        stats_.space_reclaimed_bytes += space_reclaimed;
        stats_.cooperative_runs++;

        // Track page efficiency metrics
        if (tuples_removed == 0)
        {
            stats_.pages_with_no_garbage++;
        }
        if (space_reclaimed > stats_.max_space_reclaimed_single_page)
        {
            stats_.max_space_reclaimed_single_page = space_reclaimed;
        }
    }

    void GarbageCollector::updateBackgroundStats(uint64_t tuples_removed, uint64_t pages_cleaned,
                                                   uint64_t space_reclaimed, uint64_t duration_ms)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.tuples_removed += tuples_removed;
        stats_.pages_cleaned += pages_cleaned;
        stats_.space_reclaimed_bytes += space_reclaimed;
        stats_.background_runs++;
        stats_.last_background_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        stats_.last_background_duration_ms = duration_ms;

        // Update duration histogram
        if (duration_ms < 10)
        {
            stats_.duration_0_10ms++;
        }
        else if (duration_ms < 50)
        {
            stats_.duration_10_50ms++;
        }
        else if (duration_ms < 100)
        {
            stats_.duration_50_100ms++;
        }
        else if (duration_ms < 500)
        {
            stats_.duration_100_500ms++;
        }
        else if (duration_ms < 1000)
        {
            stats_.duration_500_1000ms++;
        }
        else
        {
            stats_.duration_1000ms_plus++;
        }

        // Track page efficiency metrics (for background GC passes)
        if (tuples_removed == 0)
        {
            stats_.pages_with_no_garbage += pages_cleaned;
        }
        if (space_reclaimed > stats_.max_space_reclaimed_single_page)
        {
            stats_.max_space_reclaimed_single_page = space_reclaimed;
        }
    }

    void GarbageCollector::wakeBackgroundThread()
    {
        // Wake background GC thread immediately using condition variable
        bg_wake_cv_.notify_one();
    }

    bool GarbageCollector::shouldRunCooperativeGC()
    {
        // Simple rate limiting - run on ~1% of page reads
        // Use thread-local counter to avoid contention
        static thread_local uint32_t counter = 0;
        counter++;

        return (counter % cooperative_rate_) == 0;
    }

    void GarbageCollector::readConfiguration()
    {
        Config& cfg = Config::getInstance();

        // Read GC policy
        std::string policy_str = cfg.getString("garbage_collection", "policy", "COMBINED");
        if (policy_str == "COOPERATIVE")
        {
            policy_ = GCPolicy::COOPERATIVE;
        }
        else if (policy_str == "BACKGROUND")
        {
            policy_ = GCPolicy::BACKGROUND;
        }
        else if (policy_str == "COMBINED")
        {
            policy_ = GCPolicy::COMBINED;
        }
        else
        {
            LOG_WARNING(VACUUM, "Invalid GC policy '%s', using COMBINED", policy_str.c_str());
            policy_ = GCPolicy::COMBINED;
        }

        // Read background interval (default: 5000ms = 5 seconds)
        background_interval_ms_ = cfg.getUInt("garbage_collection", "background_interval_ms", 5000);

        // Validate interval (minimum 100ms, maximum 1 hour)
        if (background_interval_ms_ < 100)
        {
            LOG_WARNING(VACUUM, "Background interval %lums too low, using 100ms", background_interval_ms_);
            background_interval_ms_ = 100;
        }
        else if (background_interval_ms_ > 3600000)  // 1 hour
        {
            LOG_WARNING(VACUUM, "Background interval %lums too high, using 3600000ms (1 hour)", background_interval_ms_);
            background_interval_ms_ = 3600000;
        }

        // Read cooperative rate (default: 100 = 1% of page reads)
        cooperative_rate_ = cfg.getUInt("garbage_collection", "cooperative_rate", 100);

        // Validate rate (minimum 1, maximum 10000)
        if (cooperative_rate_ < 1)
        {
            LOG_WARNING(VACUUM, "Cooperative rate %u too low, using 1", cooperative_rate_);
            cooperative_rate_ = 1;
        }
        else if (cooperative_rate_ > 10000)
        {
            LOG_WARNING(VACUUM, "Cooperative rate %u too high, using 10000", cooperative_rate_);
            cooperative_rate_ = 10000;
        }

        // Read enabled flag (default: true)
        bool enabled = cfg.getBool("garbage_collection", "enabled", true);
        enabled_.store(enabled, std::memory_order_release);
    }

} // namespace scratchbird::core
