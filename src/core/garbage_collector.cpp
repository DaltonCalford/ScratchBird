/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/index_factory.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/mga_failpoint_manager.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/gist_index.h"
#include "scratchbird/core/spgist_index.h"
#include "scratchbird/core/rtree.h"
#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/inverted_index.h"
#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/toast.h" // Phase 4: TOAST GC
#include "scratchbird/core/toast_visibility.h"
#include "scratchbird/core/plain_value_reader.h"
#include "scratchbird/core/gpid.h"
#include <chrono>
#include <functional>
#include <thread>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace scratchbird::core
{
    namespace
    {
        bool parseUuidFromString(const std::string& text, ID& out)
        {
            // Expect canonical UUID with dashes; tolerate missing dashes.
            std::string hex;
            hex.reserve(32);
            for (char c : text)
            {
                if (c == '-')
                {
                    continue;
                }
                if ((c >= '0' && c <= '9') ||
                    (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F'))
                {
                    hex.push_back(c);
                }
                else
                {
                    return false;
                }
            }

            if (hex.size() != 32)
            {
                return false;
            }

            auto hexToNibble = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
                if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
                return static_cast<uint8_t>(10 + (c - 'A'));
            };

            for (size_t i = 0; i < 16; ++i)
            {
                uint8_t hi = hexToNibble(hex[i * 2]);
                uint8_t lo = hexToNibble(hex[i * 2 + 1]);
                out.bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
            }
            return true;
        }
    } // namespace
    namespace
    {
        IndexGCInterface *asIndexGcInterface(void *index_ptr,
                                             IndexFactory::IndexRuntimeClass runtime_class)
        {
            if (!index_ptr)
            {
                return nullptr;
            }

            switch (runtime_class)
            {
                case IndexFactory::IndexRuntimeClass::BTREE:
                    return static_cast<BTree *>(index_ptr);
                case IndexFactory::IndexRuntimeClass::HASH:
                    return static_cast<HashIndex *>(index_ptr);
                case IndexFactory::IndexRuntimeClass::LSM:
                    return static_cast<LSMTreeIndex *>(index_ptr);
                case IndexFactory::IndexRuntimeClass::GIN:
                    return static_cast<GinIndex *>(index_ptr);
                case IndexFactory::IndexRuntimeClass::GIST:
                    return static_cast<GiSTIndex *>(index_ptr);
                case IndexFactory::IndexRuntimeClass::BRIN:
                    return static_cast<BrinIndex *>(index_ptr);
                case IndexFactory::IndexRuntimeClass::RTREE:
                    return static_cast<RTree *>(index_ptr);
                case IndexFactory::IndexRuntimeClass::SPGIST:
                    return static_cast<SPGiSTIndex *>(index_ptr);
                case IndexFactory::IndexRuntimeClass::BITMAP:
                    return static_cast<BitmapIndex *>(index_ptr);
                case IndexFactory::IndexRuntimeClass::COLUMNSTORE:
                    return static_cast<ColumnstoreIndex *>(index_ptr);
                case IndexFactory::IndexRuntimeClass::HNSW:
                    return static_cast<HnswIndex *>(index_ptr);
                case IndexFactory::IndexRuntimeClass::INVERTED:
                    return static_cast<InvertedIndex *>(index_ptr);
                default:
                    return nullptr;
            }
        }
    } // namespace
    namespace
    {
        bool isZeroId(const ID &id)
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
    }

    GarbageCollector::GarbageCollector(Database *db)
        : db_(db), txn_manager_(nullptr), storage_engine_(nullptr), policy_(GCPolicy::COMBINED),
          enabled_(true), background_interval_ms_(5000), cooperative_rate_(100),
          adaptive_tuning_enabled_(true), tuning_check_interval_(10), background_running_(false),
          shutdown_requested_(false)
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

    Status GarbageCollector::initialize(ErrorContext *ctx)
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
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "TransactionManager or StorageEngine not available");
            return Status::IO_ERROR;
        }

        // Read configuration
        readConfiguration();

        LOG_INFO(VACUUM,
                 "GarbageCollector initialized with policy: %s, interval: %lums, rate: 1/%u",
                 policy_ == GCPolicy::COOPERATIVE  ? "COOPERATIVE"
                 : policy_ == GCPolicy::BACKGROUND ? "BACKGROUND"
                                                   : "COMBINED",
                 background_interval_ms_, cooperative_rate_);

        return Status::OK;
    }

    void GarbageCollector::processPageCooperative(uint32_t page_id, ErrorContext *ctx)
    {
        // Check if cooperative GC is enabled
        if (!enabled_.load(std::memory_order_acquire))
        {
            return;
        }

        if (policy_ == GCPolicy::BACKGROUND)
        {
            return; // Cooperative disabled in BACKGROUND-only mode
        }

        if (sweep_prune_blocked_.load(std::memory_order_acquire))
        {
            LOG_DEBUG(VACUUM,
                      "Cooperative GC skipped because sweep prune is blocked pending local evidence");
            return;
        }

        bool explicitly_dirty = false;
        {
            std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
            explicitly_dirty = dirty_pages_.find(page_id) != dirty_pages_.end();
        }

        // Rate limiting is only for opportunistic page reads. If a caller
        // explicitly marked a page dirty and then asked for cooperative GC on
        // that page, honor the request immediately.
        if (!explicitly_dirty && !shouldRunCooperativeGC())
        {
            return;
        }

        // Perform cooperative cleanup
        uint64_t space_reclaimed = 0;
        uint64_t tuples_removed = cleanPage(page_id, &space_reclaimed, ctx);

        // Update statistics
        updateCooperativeStats(tuples_removed, 1, space_reclaimed);
    }

    Status GarbageCollector::startBackgroundGC(ErrorContext *ctx)
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

        uint64_t tid = std::hash<std::thread::id>{}(background_thread_.get_id());
        LOG_INFO(VACUUM, "Background GC thread started (tid=%lu)",
                 static_cast<unsigned long>(tid));
        return Status::OK;
    }

    Status GarbageCollector::stopBackgroundGC(ErrorContext *ctx)
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

        // Get current timestamp
        uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();

        // Check if page is already dirty
        auto it = dirty_pages_.find(page_id);
        if (it != dirty_pages_.end())
        {
            // Page already dirty - increment mark count and recalculate priority
            DirtyPageInfo &info = it->second;
            info.mark_count++;

            // Recalculate priority based on mark count and age
            uint64_t age = now - info.marked_timestamp;
            info.priority = calculatePagePriority(info.mark_count, age);
        }
        else
        {
            // New dirty page - add to map with initial priority
            double initial_priority = 1.0; // Base priority
            dirty_pages_[page_id] = DirtyPageInfo(page_id, initial_priority, now);
        }

        // Track garbage accumulation rate (counts every mark, including re-marks)
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
                 policy == GCPolicy::COOPERATIVE  ? "COOPERATIVE"
                 : policy == GCPolicy::BACKGROUND ? "BACKGROUND"
                                                  : "COMBINED");
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

        // Include current tuning parameters
        stats.current_cooperative_rate = cooperative_rate_;
        stats.current_background_interval_ms = background_interval_ms_;

        return stats;
    }

    void GarbageCollector::notifySweepComplete(uint64_t old_oit, uint64_t new_oit)
    {
        sweep_prune_blocked_.store(false, std::memory_order_release);

        // Sweep has advanced OIT - more tuples may now be garbage
        LOG_INFO(VACUUM, "Sweep completed: OIT advanced from %lu to %lu", old_oit, new_oit);

        // If background GC is enabled, wake it to process newly-identified garbage
        if (policy_ == GCPolicy::BACKGROUND || policy_ == GCPolicy::COMBINED)
        {
            wakeBackgroundThread();
        }
    }

    void GarbageCollector::notifySweepEvidenceBlocked(uint64_t old_oit, uint64_t new_oit)
    {
        sweep_prune_blocked_.store(true, std::memory_order_release);
        LOG_WARNING(VACUUM,
                    "Sweep evidence handoff blocked prune after OIT advance from %lu to %lu",
                    old_oit, new_oit);
        wakeBackgroundThread();
    }

    void GarbageCollector::setSweepPruneBlocked(bool blocked)
    {
        sweep_prune_blocked_.store(blocked, std::memory_order_release);
        if (blocked)
        {
            wakeBackgroundThread();
        }
    }

    bool GarbageCollector::isSweepPruneBlocked() const
    {
        return sweep_prune_blocked_.load(std::memory_order_acquire);
    }

    // Private methods

    void GarbageCollector::backgroundGCLoop()
    {
        LOG_INFO(VACUUM, "Background GC loop started");

        while (!shutdown_requested_.load(std::memory_order_acquire))
        {
            auto start_time = std::chrono::steady_clock::now();

            if (sweep_prune_blocked_.load(std::memory_order_acquire))
            {
                std::unique_lock<std::mutex> wake_lock(bg_wake_mutex_);
                bg_wake_cv_.wait_for(
                    wake_lock,
                    std::chrono::milliseconds(background_interval_ms_),
                    [this] {
                        return shutdown_requested_.load(std::memory_order_acquire) ||
                               !sweep_prune_blocked_.load(std::memory_order_acquire);
                    });
                continue;
            }

            // Get dirty pages to clean (sorted by priority)
            std::vector<DirtyPageInfo> pages_to_clean;
            {
                std::lock_guard<std::mutex> lock(dirty_pages_mutex_);

                // Extract all dirty page info
                pages_to_clean.reserve(dirty_pages_.size());
                for (const auto &pair : dirty_pages_)
                {
                    pages_to_clean.push_back(pair.second);
                }

                // Sort by priority (highest first)
                std::sort(pages_to_clean.begin(), pages_to_clean.end(),
                          [](const DirtyPageInfo &a, const DirtyPageInfo &b)
                          {
                              return a < b; // Uses operator< which orders by priority
                          });
            }

            uint64_t tuples_removed = 0;
            uint64_t pages_cleaned = 0;
            uint64_t space_reclaimed = 0;

            // Clean each dirty page (in priority order - highest first)
            for (const auto &page_info : pages_to_clean)
            {
                if (shutdown_requested_.load(std::memory_order_acquire))
                {
                    break;
                }

                ErrorContext err_ctx;
                uint64_t page_space_reclaimed = 0;
                uint64_t tuples_found =
                    cleanPage(page_info.page_id, &page_space_reclaimed, &err_ctx);
                tuples_removed += tuples_found;
                space_reclaimed += page_space_reclaimed;
                pages_cleaned++;
            }

            // Update statistics
            auto end_time = std::chrono::steady_clock::now();
            uint64_t duration_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
                    .count();

            updateBackgroundStats(tuples_removed, pages_cleaned, space_reclaimed, duration_ms);

            // Perform adaptive tuning (if enabled)
            performAdaptiveTuning();

            // Wait for wake signal or timeout
            // Use condition variable for responsive wake on sweep completion
            std::unique_lock<std::mutex> lock(bg_wake_mutex_);
            bg_wake_cv_.wait_for(lock, std::chrono::milliseconds(background_interval_ms_), [this]
                                 { return shutdown_requested_.load(std::memory_order_acquire); });
        }

        LOG_INFO(VACUUM, "Background GC loop stopped");
    }

    uint64_t GarbageCollector::cleanPage(uint32_t page_id, uint64_t *space_reclaimed_out,
                                         ErrorContext *ctx)
    {
        ReclaimHorizonSnapshot horizons{};
        Status horizon_status = txn_manager_->captureReclaimHorizons(horizons, ctx);
        if (horizon_status != Status::OK)
        {
            if (space_reclaimed_out != nullptr)
            {
                *space_reclaimed_out = 0;
            }
            return 0;
        }
        uint64_t reclaim_horizon = horizons.heap_reclaim_horizon;

        // Pin the page through buffer pool
        void *page_buffer;
        Status s = db_->buffer_pool()->pinPage(
            page_id, &page_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
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
        auto *page_header = reinterpret_cast<PageHeader *>(page_buffer);

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

        // Reject pages with a mismatched runtime page size before interpreting heap layout.
        if (page_header->page_size != db_->page_size())
        {
            LOG_WARNING(VACUUM,
                        "Skipping page %u for GC due to page-size mismatch (header=%u, db=%u)",
                        page_id, page_header->page_size, db_->page_size());
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            if (space_reclaimed_out != nullptr)
            {
                *space_reclaimed_out = 0;
            }
            return 0;
        }

        const auto *special = reinterpret_cast<const HeapPageSpecial *>(
            reinterpret_cast<const uint8_t *>(page_buffer) + page_header->page_size - sizeof(HeapPageSpecial));
        const ID table_id = (special != nullptr) ? special->table_id : ID{};

        // Use HeapPage for garbage collection.
        HeapPage heap_page(reinterpret_cast<uint8_t *>(page_buffer),
                           page_header->page_size,
                           nullptr,
                           db_,
                           table_id);

        // Validate heap page boundaries/item pointers before pruning; skip corrupt/incompatible pages.
        Status validate_status = heap_page.validate(ctx);
        if (validate_status != Status::OK)
        {
            LOG_WARNING(VACUUM,
                        "Skipping page %u for GC due to invalid heap-page layout: %d",
                        page_id, static_cast<int>(validate_status));
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            if (space_reclaimed_out != nullptr)
            {
                *space_reclaimed_out = 0;
            }
            {
                std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
                dirty_pages_.erase(page_id);
            }
            return 0;
        }

        HeapPage::VersionChainAuditResult chain_audit{};
        Status audit_status = heap_page.auditVersionChainMetadata(
            HeapPage::VersionChainAuditMode::READ_ONLY, &chain_audit, ctx);
        if (audit_status != Status::OK)
        {
            LOG_WARNING(VACUUM,
                        "Skipping page %u for GC because version chain audit failed: %d",
                        page_id,
                        static_cast<int>(audit_status));
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            if (space_reclaimed_out != nullptr)
            {
                *space_reclaimed_out = 0;
            }
            {
                std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
                dirty_pages_.erase(page_id);
            }
            return 0;
        }
        if (chain_audit.cleanup_blocked)
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, chain_audit.summary.c_str());
            LOG_WARNING(VACUUM,
                        "Skipping page %u for GC because version chain cleanup is blocked: %s",
                        page_id,
                        chain_audit.summary.c_str());
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            if (space_reclaimed_out != nullptr)
            {
                *space_reclaimed_out = 0;
            }
            {
                std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
                dirty_pages_.erase(page_id);
            }
            return 0;
        }
        if (chain_audit.strongest_class == HeapPage::VersionChainAnomalyClass::RELINKABLE)
        {
            LOG_WARNING(VACUUM,
                        "Continuing GC on page %u with relinkable version chain anomalies: %s",
                        page_id,
                        chain_audit.summary.c_str());
        }

        uint32_t tuples_pruned = 0;
        uint32_t space_reclaimed = 0;

        HeapPage::VersionMaturityScan maturity_scan{};
        std::vector<uint16_t> reclaimable_item_ids;
        std::vector<TID> dead_tids;
        Status collect_status = heap_page.scanVersionMaturity(reclaim_horizon,
                                                              &maturity_scan,
                                                              &reclaimable_item_ids,
                                                              &dead_tids,
                                                              ctx);
        if (collect_status != Status::OK)
        {
            LOG_WARNING(VACUUM, "Failed to classify version maturity on page %u: %d", page_id,
                       static_cast<int>(collect_status));
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            if (space_reclaimed_out != nullptr)
            {
                *space_reclaimed_out = 0;
            }
            {
                std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
                dirty_pages_.erase(page_id);
            }
            return 0;
        }

        uint16_t chain_depth_hint = 0;
        uint64_t oldest_interesting_txid = 0;
        for (uint16_t item_id = 0; item_id < heap_page.getItemCount(); ++item_id)
        {
            const uint8_t *tuple_data = nullptr;
            uint32_t tuple_size = 0;
            if (heap_page.getTuple(item_id, &tuple_data, &tuple_size, ctx) != Status::OK)
            {
                continue;
            }

            const auto *tuple_hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
            if (tuple_hdr->hasBackVersion())
            {
                ++chain_depth_hint;
            }
            if (tuple_hdr->xmin != 0 &&
                (oldest_interesting_txid == 0 || tuple_hdr->xmin < oldest_interesting_txid))
            {
                oldest_interesting_txid = tuple_hdr->xmin;
            }
            if (tuple_hdr->xmax != 0 &&
                (oldest_interesting_txid == 0 || tuple_hdr->xmax < oldest_interesting_txid))
            {
                oldest_interesting_txid = tuple_hdr->xmax;
            }
        }

        BufferPool::MgaFrameHints page_hints{};
        page_hints.page_class = (maturity_scan.reclaimable_item_count != 0)
                                    ? BufferPool::MgaPageClass::GC_CANDIDATE
                                    : (chain_depth_hint >= 4
                                           ? BufferPool::MgaPageClass::CHAIN_HEAVY
                                           : BufferPool::MgaPageClass::VERSION_ROOT);
        page_hints.oldest_interesting_txid = oldest_interesting_txid;
        page_hints.prune_safe_horizon_hint = reclaim_horizon;
        page_hints.dead_version_bytes = maturity_scan.reclaimable_bytes;
        page_hints.chain_depth_hint = chain_depth_hint;
        (void)db_->buffer_pool()->publishMgaFrameHintsGlobal(convertPageIDtoGPID(page_id),
                                                             page_hints,
                                                             nullptr);

        // Prune garbage tuples and defragment page
        Status prune_status =
            heap_page.prunePage(reclaim_horizon, &tuples_pruned, &space_reclaimed, ctx);
        if (prune_status != Status::OK)
        {
            LOG_WARNING(VACUUM, "Failed to prune page %u during GC: %d", page_id,
                        static_cast<int>(prune_status));
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            if (space_reclaimed_out != nullptr)
            {
                *space_reclaimed_out = 0;
            }
            {
                std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
                dirty_pages_.erase(page_id);
            }
            return 0;
        }

        bool page_modified = tuples_pruned > 0;

        MgaFailpointManager* failpoints = db_ ? db_->mga_failpoint_manager() : nullptr;
        if (failpoints != nullptr)
        {
            ErrorContext failpoint_ctx;
            Status failpoint_status = failpoints->trip(
                MgaFailpointTriggers::kAfterChainUnlinkBeforeCompactionPublish,
                {},
                &failpoint_ctx);
            if (failpoint_status != Status::OK)
            {
                LOG_WARNING(VACUUM,
                            "Injected MGA failpoint after chain unlink on page %u: %s",
                            page_id,
                            failpoint_ctx.message.c_str());
                db_->buffer_pool()->unpinPage(page_id, page_modified, ctx);
                if (space_reclaimed_out != nullptr)
                {
                    *space_reclaimed_out = space_reclaimed;
                }
                {
                    std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
                    dirty_pages_.erase(page_id);
                }
                return tuples_pruned;
            }
        }

        // Log results if we pruned any tuples
        if (tuples_pruned > 0)
        {
            LOG_INFO(VACUUM,
                     "Page %u: pruned %u tuples, reclaimed %u bytes (safe_horizon=%lu)",
                     page_id,
                     tuples_pruned,
                     space_reclaimed,
                     reclaim_horizon);
        }

        // Unpin page (mark as dirty if we modified it)
        db_->buffer_pool()->unpinPage(page_id, page_modified, ctx);

        // PHASE 2 TASK 2.6: Clean indexes after pruning heap
        uint64_t index_entries_removed = 0;
        if (!dead_tids.empty())
        {
            bool skip_index_cleanup = false;
            if (failpoints != nullptr)
            {
                ErrorContext failpoint_ctx;
                Status failpoint_status = failpoints->trip(
                    MgaFailpointTriggers::kAfterHeapReclaimBeforeDeadEntryDelete,
                    {},
                    &failpoint_ctx);
                if (failpoint_status != Status::OK)
                {
                    skip_index_cleanup = true;
                    LOG_WARNING(VACUUM,
                                "Injected MGA failpoint before dead-index cleanup on page %u: %s",
                                page_id,
                                failpoint_ctx.message.c_str());
                }
            }

            if (!skip_index_cleanup)
            {
                index_entries_removed = cleanIndexes(page_id, table_id, dead_tids, ctx);
                LOG_DEBUG(VACUUM, "Page %u: removed %lu index entries for %zu dead tuples",
                          page_id, index_entries_removed, dead_tids.size());
            }
        }

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
    void GarbageCollector::updateCooperativeStats(uint64_t tuples_removed, uint64_t pages_cleaned,
                                                  uint64_t space_reclaimed)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.tuples_removed += tuples_removed;
        stats_.pages_cleaned += pages_cleaned;
        stats_.space_reclaimed_bytes += space_reclaimed;
        stats_.cooperative_reclaimed_bytes += space_reclaimed;
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
        stats_.background_reclaimed_bytes += space_reclaimed;
        stats_.background_runs++;
        stats_.last_background_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
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

    bool GarbageCollector::shouldRunCooperativeGC() const
    {
        // Simple rate limiting - run on ~1% of page reads
        // Use thread-local counter to avoid contention
        static thread_local uint32_t counter = 0;
        counter++;

        return (counter % cooperative_rate_) == 0;
    }

    void GarbageCollector::readConfiguration()
    {
        Config &cfg = Config::getInstance();

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
            LOG_WARNING(VACUUM, "Background interval %lums too low, using 100ms",
                        background_interval_ms_);
            background_interval_ms_ = 100;
        }
        else if (background_interval_ms_ > 3600000) // 1 hour
        {
            LOG_WARNING(VACUUM, "Background interval %lums too high, using 3600000ms (1 hour)",
                        background_interval_ms_);
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

        // Read adaptive tuning flag (default: true)
        bool adaptive_enabled = cfg.getBool("garbage_collection", "adaptive_tuning", true);
        adaptive_tuning_enabled_.store(adaptive_enabled, std::memory_order_release);

        // Read tuning check interval (default: 10 background runs)
        tuning_check_interval_ = cfg.getUInt("garbage_collection", "tuning_check_interval", 10);
        if (tuning_check_interval_ < 1)
        {
            LOG_WARNING(VACUUM, "Tuning check interval %u too low, using 1",
                        tuning_check_interval_);
            tuning_check_interval_ = 1;
        }
    }

    void GarbageCollector::setAdaptiveTuning(bool enabled)
    {
        adaptive_tuning_enabled_.store(enabled, std::memory_order_release);
        LOG_INFO(VACUUM, "Adaptive tuning %s", enabled ? "enabled" : "disabled");
    }

    bool GarbageCollector::isAdaptiveTuningEnabled() const
    {
        return adaptive_tuning_enabled_.load(std::memory_order_acquire);
    }

    void GarbageCollector::performAdaptiveTuning()
    {
        // Only perform tuning if enabled
        if (!adaptive_tuning_enabled_.load(std::memory_order_acquire))
        {
            return;
        }

        // Check if it's time to tune (every N background runs)
        uint64_t background_runs = 0;
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            background_runs = stats_.background_runs;
        }

        if (background_runs % tuning_check_interval_ != 0)
        {
            return; // Not yet time to tune
        }

        // Get current statistics
        GCStatistics stats = getStatistics();

        // Adaptive Cooperative Rate Tuning
        // Goal: Minimize wasted effort (pages scanned with no garbage)
        if (stats.pages_cleaned > 0)
        {
            double waste_ratio = static_cast<double>(stats.pages_with_no_garbage) /
                                 static_cast<double>(stats.pages_cleaned);

            uint32_t old_rate = cooperative_rate_;
            uint32_t new_rate = old_rate;

            if (waste_ratio > HIGH_WASTE_THRESHOLD)
            {
                // Too much wasted effort - reduce frequency (increase rate)
                new_rate = static_cast<uint32_t>(old_rate * 1.1); // Increase by 10%
                if (new_rate > MAX_COOPERATIVE_RATE)
                {
                    new_rate = MAX_COOPERATIVE_RATE;
                }
            }
            else if (waste_ratio < LOW_WASTE_THRESHOLD)
            {
                // Low wasted effort - can increase frequency (decrease rate)
                new_rate = static_cast<uint32_t>(old_rate * 0.9); // Decrease by 10%
                if (new_rate < MIN_COOPERATIVE_RATE)
                {
                    new_rate = MIN_COOPERATIVE_RATE;
                }
            }

            if (new_rate != old_rate)
            {
                cooperative_rate_ = new_rate;
                LOG_INFO(VACUUM, "Adaptive tuning: cooperative_rate %u -> %u (waste ratio: %.2f%%)",
                         old_rate, new_rate, waste_ratio * 100.0);
            }
        }

        // Adaptive Background Interval Tuning
        // Goal: Balance responsiveness with CPU overhead
        uint64_t total_runs = stats.duration_0_10ms + stats.duration_10_50ms +
                              stats.duration_50_100ms + stats.duration_100_500ms +
                              stats.duration_500_1000ms + stats.duration_1000ms_plus;

        if (total_runs > 0)
        {
            // Calculate percentage of fast runs (0-50ms)
            double fast_ratio =
                static_cast<double>(stats.duration_0_10ms + stats.duration_10_50ms) /
                static_cast<double>(total_runs);

            // Calculate percentage of slow runs (100ms+)
            double slow_ratio =
                static_cast<double>(stats.duration_100_500ms + stats.duration_500_1000ms +
                                    stats.duration_1000ms_plus) /
                static_cast<double>(total_runs);

            uint64_t old_interval = background_interval_ms_;
            uint64_t new_interval = old_interval;

            // If mostly fast runs AND low dirty page count, increase interval (less frequent)
            if (fast_ratio > 0.8 && stats.dirty_page_count < LOW_DIRTY_THRESHOLD)
            {
                new_interval = static_cast<uint64_t>(old_interval * 1.1); // Increase by 10%
                if (new_interval > MAX_BACKGROUND_INTERVAL_MS)
                {
                    new_interval = MAX_BACKGROUND_INTERVAL_MS;
                }
            }
            // If many slow runs OR high dirty page count, decrease interval (more frequent)
            else if (slow_ratio > 0.2 || stats.dirty_page_count > HIGH_DIRTY_THRESHOLD)
            {
                new_interval = static_cast<uint64_t>(old_interval * 0.9); // Decrease by 10%
                if (new_interval < MIN_BACKGROUND_INTERVAL_MS)
                {
                    new_interval = MIN_BACKGROUND_INTERVAL_MS;
                }
            }

            if (new_interval != old_interval)
            {
                background_interval_ms_ = new_interval;
                LOG_INFO(VACUUM,
                         "Adaptive tuning: background_interval_ms %lu -> %lu (fast: %.1f%%, slow: "
                         "%.1f%%, dirty: %lu)",
                         old_interval, new_interval, fast_ratio * 100.0, slow_ratio * 100.0,
                         stats.dirty_page_count);
            }
        }
    }

    double GarbageCollector::calculatePagePriority(uint32_t mark_count, uint64_t age_microseconds) const
    {
        // Priority calculation strategy:
        // 1. Pages marked multiple times (high churn) = higher priority
        // 2. Older pages (accumulated more garbage) = higher priority
        // 3. Combine both factors with weighted scoring
        //
        // Formula: priority = mark_count_score + age_score
        //   mark_count_score = log2(mark_count + 1) * 2.0
        //   age_score = age_seconds * 0.1
        //
        // This gives:
        // - mark_count=1, age=10s  -> priority = 2.0 + 1.0 = 3.0
        // - mark_count=5, age=10s  -> priority = 5.17 + 1.0 = 6.17
        // - mark_count=1, age=100s -> priority = 2.0 + 10.0 = 12.0
        // - mark_count=10, age=100s -> priority = 7.02 + 10.0 = 17.02

        // Convert age to seconds
        double age_seconds = age_microseconds / 1000000.0;

        // Mark count contribution (logarithmic to avoid extreme values)
        double mark_count_score = std::log2(mark_count + 1) * 2.0;

        // Age contribution (linear)
        double age_score = age_seconds * 0.1;

        // Combined priority
        double priority = mark_count_score + age_score;

        return priority;
    }

    // PHASE 2 TASK 2.6: Clean indexes for dead tuples
    // PHASE 1.5 TASK 1.5.3: Migrated to TID struct API
    uint64_t GarbageCollector::cleanIndexes(uint32_t page_id, const ID &table_id,
                                            const std::vector<TID> &dead_tids,
                                            ErrorContext *ctx)
    {
        if (dead_tids.empty())
        {
            return 0;
        }

        uint64_t total_entries_removed = 0;

        // PHASE 2 TASK 2.6: Full implementation of index cleanup
        //
        // Strategy: Since we don't have a direct page_id → table_id mapping,
        // we use catalog metadata to find which table owns this page.
        // This is acceptable for GC (background operation, not performance-critical).

        auto *catalog = db_->catalog_manager();
        if (!catalog)
        {
            LOG_WARNING(VACUUM, "Cannot clean indexes for page %u: catalog manager not available",
                       page_id);
            return 0;
        }

        std::vector<CatalogManager::TableInfo> tables;
        Status status = Status::OK;

        if (!isZeroId(table_id))
        {
            CatalogManager::TableInfo table_info;
            status = catalog->getTable(table_id, table_info, ctx);
            if (status != Status::OK)
            {
                LOG_WARNING(VACUUM, "Cannot clean indexes for page %u: failed to resolve table ID %s (status %d)",
                           page_id, table_id.toString().c_str(), static_cast<int>(status));
                return 0;
            }
            tables.push_back(table_info);
        }
        else
        {
            // Fallback: scan all tables when table_id isn't populated on the page.
            std::vector<CatalogManager::SchemaInfo> schemas;
            status = catalog->listSchemas(schemas, ctx);
            if (status != Status::OK)
            {
                LOG_WARNING(VACUUM, "Cannot clean indexes for page %u: failed to list schemas (status %d)",
                           page_id, static_cast<int>(status));
                return 0;
            }

            for (const auto &schema : schemas)
            {
                std::vector<CatalogManager::TableInfo> schema_tables;
                status = catalog->listTables(schema.schema_id, schema_tables, ctx);
                if (status == Status::OK)
                {
                    tables.insert(tables.end(), schema_tables.begin(), schema_tables.end());
                }
            }
        }

        bool found_owner = false;

        for (const auto &table : tables)
        {
            // Get indexes for this table
            std::vector<CatalogManager::IndexInfo> indexes;
            status = catalog->listIndexesForTable(table.table_id, indexes, ctx, false);
            if (status != Status::OK)
            {
                LOG_WARNING(VACUUM, "Failed to list indexes for table %s (status %d)",
                           table.table_name.c_str(), static_cast<int>(status));
                continue;
            }

            if (indexes.empty())
            {
                continue; // No indexes for this table
            }

            // Try to clean each index
            // Note: removeDeadEntries() is idempotent, so it's safe to call
            // even if the TIDs don't belong to this table's indexes
            for (const auto &index_info : indexes)
            {
                void *index_handle = nullptr;
                IndexGCInterface *index = nullptr;

                auto closeIndexHandle = [&]() {
                    if (!index_handle)
                    {
                        return;
                    }

                    ErrorContext close_ctx;
                    Status close_status =
                        IndexFactory::closeIndex(index_info.index_type, index_handle, &close_ctx);
                    if (close_status != Status::OK)
                    {
                        LOG_WARNING(VACUUM, "Failed to close index %s after cleanup (status %d): %s",
                                    index_info.index_name.c_str(), static_cast<int>(close_status),
                                    close_ctx.message.c_str());
                    }
                    index_handle = nullptr;
                };

                Status open_status = IndexFactory::openIndex(
                    index_info.index_type, db_, index_info, &index_handle, ctx);
                if (open_status != Status::OK || !index_handle)
                {
                    LOG_WARNING(VACUUM, "Failed to open index %s for cleanup (status %d)",
                                index_info.index_name.c_str(), static_cast<int>(open_status));
                    continue;
                }

                const auto *caps = IndexFactory::lookupCapabilities(index_info.index_type);
                if (!caps)
                {
                    LOG_WARNING(VACUUM, "Missing capability metadata for index %s (type %d)",
                                index_info.index_name.c_str(),
                                static_cast<int>(index_info.index_type));
                    closeIndexHandle();
                    continue;
                }

                index = asIndexGcInterface(index_handle, caps->runtime_class);
                if (!index)
                {
                    LOG_WARNING(VACUUM,
                                "Runtime class %d for index %s does not implement GC cleanup",
                                static_cast<int>(caps->runtime_class),
                                index_info.index_name.c_str());
                    closeIndexHandle();
                    continue;
                }

                std::vector<IndexGcCandidate> dead_candidates;
                dead_candidates.reserve(dead_tids.size());
                for (const auto &dead_tid : dead_tids)
                {
                    dead_candidates.push_back(
                        IndexGcCandidate{dead_tid, IndexGcLifecycleState::LOGICAL_DEAD_ROOT});
                }

                // Call lifecycle-aware index cleanup on this index.
                uint64_t entries_removed = 0;
                uint64_t pages_modified = 0;

                Status remove_status = index->removeDeadEntriesWithLifecycle(
                    dead_candidates, &entries_removed, &pages_modified, ctx);

                if (remove_status == Status::OK)
                {
                    if (entries_removed > 0)
                    {
                        total_entries_removed += entries_removed;
                        found_owner = true; // Found at least one index that had these TIDs

                        LOG_INFO(VACUUM, "Index %s (table %s): removed %lu entries from %lu pages",
                                index->indexTypeName(), table.table_name.c_str(),
                                entries_removed, pages_modified);
                    }
                }
                else
                {
                    LOG_WARNING(VACUUM, "Index %s (table %s): GC failed with status %d",
                               index->indexTypeName(), table.table_name.c_str(),
                               static_cast<int>(remove_status));
                }

                closeIndexHandle();
            }
        }

        if (!found_owner && !tables.empty())
        {
            // This is expected for non-heap pages (index pages, metadata pages, etc.)
            LOG_DEBUG(VACUUM, "Page %u: no indexes cleaned (may not be a heap page)", page_id);
        }

        return total_entries_removed;
    }

    // =============================================================================
    // Phase 4: TOAST Garbage Collection Implementation
    // =============================================================================

    Status GarbageCollector::detectOrphanedToastChunks(
        const ID& toast_table_id,
        std::unordered_set<ID, IDHash>* orphaned_value_ids,
        ErrorContext* ctx)
    {
        // Phase 4 Task 4.1: TOAST Orphan Detection
        //
        // Algorithm:
        // 1. Collect all TOAST value IDs referenced by heap tuples
        // 2. Scan TOAST table for all value IDs
        // 3. Find orphans (in TOAST but not referenced by any heap tuple)
        //
        // MGA Compliance: Only considers visible heap tuples for references

        if (!orphaned_value_ids)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "orphaned_value_ids is null");
            return Status::INVALID_ARGUMENT;
        }

        orphaned_value_ids->clear();

        // Get catalog manager to find parent table
        auto* catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        // Find the parent table for this TOAST table
        // TOAST table name format: "toast_<parent_table_id>"
        ID parent_table_id;
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog->listSchemas(schemas, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        bool found_parent = false;
        for (const auto& schema : schemas)
        {
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK)
            {
                continue;
            }

            for (const auto& table : tables)
            {
                if (table.has_toast && table.toast_table_id == toast_table_id)
                {
                    parent_table_id = table.table_id;
                    found_parent = true;
                    break;
                }
            }
            if (found_parent) break;
        }

        if (!found_parent)
        {
            // Fallback: parse parent UUID from TOAST table name (sb_toast_<uuid>)
            CatalogManager::TableInfo toast_info;
            Status toast_status = catalog->getTable(toast_table_id, toast_info, ctx);
            if (toast_status == Status::OK)
            {
                static const std::string kPrefix = "sb_toast_";
                if (toast_info.table_name.rfind(kPrefix, 0) == 0)
                {
                    std::string uuid_text = toast_info.table_name.substr(kPrefix.size());
                    ID parsed_parent{};
                    if (parseUuidFromString(uuid_text, parsed_parent))
                    {
                        CatalogManager::TableInfo parent_info;
                        if (catalog->getTable(parsed_parent, parent_info, ctx) == Status::OK)
                        {
                            parent_table_id = parsed_parent;
                            found_parent = true;
                        }
                    }
                }
            }
        }

        if (!found_parent)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Parent table not found for TOAST table");
            return Status::NOT_FOUND;
        }

        // Get column information to parse tuples
        std::vector<CatalogManager::ColumnInfo> columns;
        status = catalog->getColumns(parent_table_id, columns, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::unordered_set<ID, IDHash> referenced_value_ids;
        auto collectToastPointers = [&](const uint8_t* tuple_data,
                                        uint32_t tuple_size) -> void
        {
            if (tuple_size < sizeof(TupleHeader))
            {
                return;
            }

            const auto* header = reinterpret_cast<const TupleHeader*>(tuple_data);
            const uint8_t* payload = tuple_data + sizeof(TupleHeader);
            const size_t payload_size = tuple_size - sizeof(TupleHeader);

            // Storage-level heap versions can legitimately store a raw canonical
            // TOAST pointer instead of a logical column-encoded payload. Detect
            // that shape first so back versions created by low-level update paths
            // are not misclassified as TOAST orphans.
            if (payload_size == sizeof(ToastPointer) &&
                (header->hasRecordFlag(TupleHeader::RHD_TOAST_PTR) ||
                 ToastManager::isToastPointer(payload, payload_size)))
            {
                const auto* toast_ptr = reinterpret_cast<const ToastPointer*>(payload);
                referenced_value_ids.insert(toast_ptr->lob_uuid);
                return;
            }

            // Check for null bitmap
            const uint8_t* null_bitmap = nullptr;
            size_t data_offset = sizeof(TupleHeader);

            if (header->hasNulls() && header->null_bitmap_offset > 0 &&
                header->null_bitmap_offset < tuple_size)
            {
                null_bitmap = tuple_data + header->null_bitmap_offset;
                size_t bitmap_bytes = (columns.size() + 7) / 8;
                data_offset = header->null_bitmap_offset + bitmap_bytes;
            }

            // Parse each column looking for TOAST pointers
            size_t current_offset = data_offset;
            for (size_t i = 0; i < columns.size(); i++)
            {
                // Check if column is NULL
                bool is_null = false;
                if (null_bitmap)
                {
                    size_t byte_offset = i / 8;
                    size_t bit_pos = i % 8;
                    is_null = (null_bitmap[byte_offset] & (1 << bit_pos)) != 0;
                }

                if (is_null)
                {
                    continue;
                }

                DataType col_type = static_cast<DataType>(columns[i].data_type);
                size_t col_size = 0;

                bool is_encrypted = false;
                if (columns[i].domain_id != ID{} && db_ && db_->domain_manager())
                {
                    DomainInfo domain;
                    Status domain_status = db_->domain_manager()->getDomain(columns[i].domain_id, domain, ctx);
                    if (domain_status != Status::OK && domain_status != Status::NOT_FOUND)
                    {
                        continue;
                    }
                    is_encrypted = (domain_status == Status::OK) && domain.security.encryption_enabled;
                }

                if (is_encrypted)
                {
                    size_t len_offset = current_offset;
                    uint32_t len = 0;
                    if (!readUint32LE(tuple_data, tuple_size, len_offset, len))
                    {
                        continue;
                    }
                    col_size = sizeof(uint32_t) + len;
                }
                else
                {
                    TypeInfo type_info(static_cast<DataType>(columns[i].data_type));
                    uint32_t precision = columns[i].type_precision != 0 ? columns[i].type_precision
                                                                         : columns[i].max_length;
                    type_info.precision = precision;
                    type_info.scale = columns[i].type_scale;
                    type_info.with_timezone = columns[i].with_timezone;
                    type_info.timezone_hint = columns[i].timezone_hint;

                    ErrorContext size_ctx;
                    Status size_status = computePlainValueSize(type_info.type,
                                                               type_info,
                                                               tuple_data + current_offset,
                                                               tuple_size - current_offset,
                                                               col_size,
                                                               &size_ctx);
                    if (size_status != Status::OK)
                    {
                        continue;
                    }

                    bool length_prefixed = (col_type == DataType::CHAR ||
                                            col_type == DataType::VARCHAR ||
                                            col_type == DataType::TEXT ||
                                            col_type == DataType::JSON ||
                                            col_type == DataType::JSONB ||
                                            col_type == DataType::XML ||
                                            col_type == DataType::BINARY ||
                                            col_type == DataType::VARBINARY ||
                                            col_type == DataType::BLOB ||
                                            col_type == DataType::BYTEA ||
                                            col_type == DataType::VECTOR);

                    if (length_prefixed && col_size >= sizeof(uint32_t) + sizeof(ToastPointer))
                    {
                        size_t len_offset = current_offset;
                        uint32_t len = 0;
                        if (readUint32LE(tuple_data, tuple_size, len_offset, len))
                        {
                            const uint8_t* col_data = tuple_data + current_offset + sizeof(uint32_t);
                            if (len == sizeof(ToastPointer))
                            {
                                if (ToastManager::isToastPointer(col_data, len))
                                {
                                    const auto* toast_ptr =
                                        reinterpret_cast<const ToastPointer*>(col_data);
                                    referenced_value_ids.insert(toast_ptr->lob_uuid);
                                }
                            }
                        }
                    }
                }

                current_offset += col_size;
                if (current_offset > tuple_size)
                {
                    break;
                }
            }
        };

        ReclaimHorizonSnapshot horizons{};
        status = txn_manager_->captureReclaimHorizons(horizons, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        uint64_t reclaim_horizon = horizons.toast_reclaim_horizon;

        std::vector<GPID> parent_pages;
        status = catalog->enumerateTablePages(parent_table_id, parent_pages, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (GPID gpid : parent_pages)
        {
            void* page_buffer = nullptr;
            Status pin_status = db_->buffer_pool()->pinPageGlobal(
                gpid, &page_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
            if (pin_status != Status::OK)
            {
                continue;
            }

            auto* page_header = static_cast<PageHeader*>(page_buffer);
            if (page_header->page_type != PAGE_TYPE_HEAP)
            {
                db_->buffer_pool()->unpinPageGlobal(gpid, false, ctx);
                continue;
            }

            auto* page_data = static_cast<uint8_t*>(page_buffer);
            HeapPage heap_page(page_data, db_->page_size(), nullptr, db_, parent_table_id);
            ErrorContext validate_ctx;
            if (heap_page.validate(&validate_ctx) != Status::OK)
            {
                db_->buffer_pool()->unpinPageGlobal(gpid, false, ctx);
                continue;
            }

            for (uint16_t item_id = 0; item_id < heap_page.getItemCount(); ++item_id)
            {
                const uint8_t* tuple_data = nullptr;
                uint32_t tuple_size = 0;
                Status tuple_status = heap_page.getTuple(item_id, &tuple_data, &tuple_size, nullptr);
                if (tuple_status != Status::OK || tuple_data == nullptr)
                {
                    continue;
                }

                // Keep TOAST values pinned until the owning heap version is physically
                // removed. Eligibility for heap prune alone is not enough to treat the
                // chunk as orphaned because TOAST GC must not outrun version-chain GC.
                collectToastPointers(tuple_data, tuple_size);
            }

            db_->buffer_pool()->unpinPageGlobal(gpid, false, ctx);
        }

        // Step 2: Scan TOAST table for all value IDs
        std::unordered_set<ID, IDHash> toast_value_ids;

        auto* storage = db_->storage_engine();
        if (!storage)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Storage engine not available");
            return Status::INVALID_ARGUMENT;
        }

        auto toast_scan = storage->createScanAll(toast_table_id, ctx);
        if (!toast_scan)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Failed to create TOAST scan iterator");
            return Status::INVALID_ARGUMENT;
        }

        Tuple toast_tuple;
        while (toast_scan->next(&toast_tuple, ctx) == Status::OK)
        {
            // Parse TOAST chunk to extract value_id
            // Format: [TupleHeader][chunk_id:16][chunk_seq:4][chunk_data_len:4][data...]
            const uint8_t* chunk_data = toast_tuple.data;
            uint32_t chunk_size = toast_tuple.data_size;

            if (chunk_size < sizeof(TupleHeader) + sizeof(ID))
            {
                continue; // Malformed chunk
            }

            // Skip TupleHeader, read chunk_id (value_id)
            size_t chunk_id_offset = sizeof(TupleHeader);
            ID value_id{};
            std::memcpy(value_id.bytes.data(), chunk_data + chunk_id_offset, value_id.bytes.size());

            toast_value_ids.insert(value_id);
        }

        // Step 3: Find orphans (in TOAST but not referenced)
        for (const auto& value_id : toast_value_ids)
        {
            if (referenced_value_ids.find(value_id) == referenced_value_ids.end())
            {
                // Orphan detected
                orphaned_value_ids->insert(value_id);
            }
        }

        LOG_INFO(VACUUM, "TOAST orphan detection: found %zu orphaned value IDs (out of %zu total)",
                 orphaned_value_ids->size(), toast_value_ids.size());

        return Status::OK;
    }

    Status GarbageCollector::cleanOrphanedToastChunks(
        const ID& toast_table_id,
        const std::unordered_set<ID, IDHash>& orphaned_value_ids,
        uint64_t* chunks_deleted,
        ErrorContext* ctx)
    {
        // Phase 4 Task 4.2: TOAST Chunk Cleanup
        //
        // Deletes all chunks for orphaned TOAST values
        // Since these have no parent tuples, safe to delete physically

        if (!chunks_deleted)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "chunks_deleted is null");
            return Status::INVALID_ARGUMENT;
        }

        *chunks_deleted = 0;

        if (orphaned_value_ids.empty())
        {
            return Status::OK; // Nothing to clean
        }

        auto* storage = db_->storage_engine();
        if (!storage)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Storage engine not available");
            return Status::INVALID_ARGUMENT;
        }

        // Scan TOAST table and delete chunks with matching value_ids
        auto toast_scan = storage->createScanAll(toast_table_id, ctx);
        if (!toast_scan)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Failed to create TOAST scan iterator");
            return Status::INVALID_ARGUMENT;
        }

        // Collect TIDs of chunks to delete
        std::vector<TID> chunks_to_delete;

        Tuple toast_tuple;
        while (toast_scan->next(&toast_tuple, ctx) == Status::OK)
        {
            // Parse chunk to get value_id
            const uint8_t* chunk_data = toast_tuple.data;
            uint32_t chunk_size = toast_tuple.data_size;

            if (chunk_size < sizeof(TupleHeader) + sizeof(ID))
            {
                continue;
            }

            // Extract value_id (chunk_id)
            size_t chunk_id_offset = sizeof(TupleHeader);
            ID value_id{};
            std::memcpy(value_id.bytes.data(), chunk_data + chunk_id_offset, value_id.bytes.size());

            // Check if this value_id is orphaned
            if (orphaned_value_ids.find(value_id) != orphaned_value_ids.end())
            {
                // Mark for deletion
                chunks_to_delete.push_back(toast_tuple.tid);
            }
        }

        auto forceDeleteChunk = [&](const TID& tid) -> Status {
            void* page_buffer = nullptr;
            Status pin_status = db_->buffer_pool()->pinPageGlobal(
                tid.gpid, &page_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
            if (pin_status != Status::OK)
            {
                return pin_status;
            }

            auto* page_data = static_cast<uint8_t*>(page_buffer);
            HeapPage heap_page(page_data, db_->page_size(), nullptr, db_, toast_table_id);
            Status delete_status = heap_page.deleteTuple(tid.slot, UINT64_MAX, ctx);

            db_->buffer_pool()->unpinPageGlobal(tid.gpid, delete_status == Status::OK, ctx);
            return delete_status;
        };

        // Delete the chunks
        for (const auto& tid : chunks_to_delete)
        {
            Status delete_status = forceDeleteChunk(tid);
            if (delete_status == Status::OK)
            {
                (*chunks_deleted)++;
            }
            else
            {
                LOG_WARNING(VACUUM, "Failed to delete orphaned TOAST chunk: gpid=%lu, item=%u",
                           static_cast<unsigned long>(tid.gpid), tid.slot);
            }
        }

        LOG_INFO(VACUUM, "Cleaned %lu orphaned TOAST chunks", *chunks_deleted);

        return Status::OK;
    }

    Status GarbageCollector::cleanToastChunksByTIP(
        const ID& toast_table_id,
        uint64_t* chunks_deleted,
        ErrorContext* ctx)
    {
        // Phase 4 Task 4.4: TIP-Based TOAST Garbage Collection
        //
        // Deletes TOAST chunks where xmax is set and committed (via TIP)
        // Clears xmax for chunks where deleting transaction aborted
        //
        // MGA Compliance: Uses TIP (Transaction Inventory Pages) for visibility

        if (!chunks_deleted)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "chunks_deleted is null");
            return Status::INVALID_ARGUMENT;
        }

        *chunks_deleted = 0;

        if (!txn_manager_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Transaction manager not available");
            return Status::INVALID_ARGUMENT;
        }

        auto* catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        ReclaimHorizonSnapshot horizons{};
        Status horizon_status = txn_manager_->captureReclaimHorizons(horizons, ctx);
        if (horizon_status != Status::OK)
        {
            return horizon_status;
        }

        const uint64_t reclaim_horizon = horizons.toast_reclaim_horizon;

        uint64_t visibility_xid = horizons.current_xid;
        if (visibility_xid == 0)
        {
            visibility_xid = (reclaim_horizon == UINT64_MAX) ? 1 : reclaim_horizon;
        }

        std::vector<GPID> toast_pages;
        Status page_status = catalog->enumerateTablePages(toast_table_id, toast_pages, ctx);
        if (page_status != Status::OK)
        {
            return page_status;
        }

        uint64_t xmax_cleared = 0;
        for (GPID gpid : toast_pages)
        {
            void* page_buffer = nullptr;
            Status pin_status = db_->buffer_pool()->pinPageGlobal(
                gpid, &page_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
            if (pin_status != Status::OK)
            {
                continue;
            }

            bool dirty = false;
            auto* page_header = static_cast<PageHeader*>(page_buffer);
            if (page_header->page_type != PAGE_TYPE_HEAP)
            {
                db_->buffer_pool()->unpinPageGlobal(gpid, false, ctx);
                continue;
            }

            auto* page_data = static_cast<uint8_t*>(page_buffer);
            HeapPage heap_page(page_data, db_->page_size(), nullptr, db_, toast_table_id);
            ErrorContext validate_ctx;
            if (heap_page.validate(&validate_ctx) != Status::OK)
            {
                db_->buffer_pool()->unpinPageGlobal(gpid, false, ctx);
                continue;
            }

            for (uint16_t item_id = 0; item_id < heap_page.getItemCount(); ++item_id)
            {
                const uint8_t* tuple_data = nullptr;
                uint32_t tuple_size = 0;
                Status tuple_status = heap_page.getTuple(item_id, &tuple_data, &tuple_size, nullptr);
                if (tuple_status != Status::OK || tuple_data == nullptr ||
                    tuple_size < sizeof(TupleHeader))
                {
                    continue;
                }

                auto* tuple_hdr =
                    const_cast<TupleHeader*>(reinterpret_cast<const TupleHeader*>(tuple_data));
                if (tuple_hdr->xmax == 0)
                {
                    continue;
                }

                ToastChunkLifecycleDecision lifecycle =
                    ToastVisibility::evaluateChunkLifecycle(tuple_hdr->xmin,
                                                            tuple_hdr->xmax,
                                                            visibility_xid,
                                                            reclaim_horizon,
                                                            txn_manager_);
                if (!lifecycle.clear_delete_marker)
                {
                    continue;
                }

                tuple_hdr->xmax = 0;
                tuple_hdr->infomask &= static_cast<uint16_t>(
                    ~(TupleHeader::HEAP_XMAX_COMMITTED | TupleHeader::HEAP_XMAX_INVALID));
                tuple_hdr->setRecordFlag(TupleHeader::RHD_DELETED, false);
                dirty = true;
                ++xmax_cleared;
            }

            uint32_t tuples_pruned = 0;
            uint32_t space_reclaimed = 0;
            Status prune_status =
                heap_page.prunePage(reclaim_horizon, &tuples_pruned, &space_reclaimed, ctx);
            if (prune_status != Status::OK)
            {
                db_->buffer_pool()->unpinPageGlobal(gpid, dirty, ctx);
                return prune_status;
            }
            if (tuples_pruned != 0)
            {
                dirty = true;
                *chunks_deleted += tuples_pruned;
            }

            db_->buffer_pool()->unpinPageGlobal(gpid, dirty, ctx);
        }

        LOG_INFO(VACUUM,
                 "TIP-based TOAST GC: deleted %lu chunks, cleared %lu aborted/out-of-range xmax markers",
                 *chunks_deleted, xmax_cleared);

        return Status::OK;
    }

} // namespace scratchbird::core
