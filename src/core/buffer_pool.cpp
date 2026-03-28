/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/core/vnext_metrics_event_model.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/logger.h"
#include <cstring>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <chrono>
#include <functional>
#include <cstdlib>
#include <sstream>
#include <unordered_set>
#ifndef _WIN32
    #include "scratchbird/core/posix_compat.h"
#endif

namespace scratchbird::core
{
    namespace
    {
        constexpr uint32_t RING_DIVISOR = 8;
        constexpr uint64_t ANONYMOUS_PREFETCH_SESSION_KEY = 1;

        void eraseOwnedFrameIndex(std::vector<uint32_t> &owned_frames, uint32_t frame_index)
        {
            owned_frames.erase(std::remove(owned_frames.begin(),
                                           owned_frames.end(),
                                           frame_index),
                               owned_frames.end());
        }

        const char* poolLayoutToString(BufferPool::PoolLayout layout)
        {
            switch (layout)
            {
                case BufferPool::PoolLayout::Single:
                    return "single";
                case BufferPool::PoolLayout::Segmented:
                    return "segmented";
                case BufferPool::PoolLayout::HotCold:
                    return "hot_cold";
                case BufferPool::PoolLayout::Tablespace:
                    return "tablespace";
            }
            return "unknown";
        }

        const char* mgaPageClassToString(BufferPool::MgaPageClass page_class)
        {
            switch (page_class)
            {
                case BufferPool::MgaPageClass::Generic:
                    return "GENERIC";
                case BufferPool::MgaPageClass::TX_STATE:
                    return "TX_STATE";
                case BufferPool::MgaPageClass::SYSTEM_META:
                    return "SYSTEM_META";
                case BufferPool::MgaPageClass::INDEX_ROOT_INTERNAL:
                    return "INDEX_ROOT_INTERNAL";
                case BufferPool::MgaPageClass::VERSION_ROOT:
                    return "VERSION_ROOT";
                case BufferPool::MgaPageClass::CHAIN_HEAVY:
                    return "CHAIN_HEAVY";
                case BufferPool::MgaPageClass::GC_CANDIDATE:
                    return "GC_CANDIDATE";
                case BufferPool::MgaPageClass::SCAN_PROBATION:
                    return "SCAN_PROBATION";
                case BufferPool::MgaPageClass::INDEX_CHURN:
                    return "INDEX_CHURN";
                case BufferPool::MgaPageClass::TEMP_WORK:
                    return "TEMP_WORK";
            }
            return "GENERIC";
        }

        const char* writebackQueueStateToString(BufferPool::WritebackQueueState queue_state)
        {
            switch (queue_state)
            {
                case BufferPool::WritebackQueueState::NONE:
                    return "NONE";
                case BufferPool::WritebackQueueState::FOREGROUND_HELP:
                    return "FOREGROUND_HELP";
                case BufferPool::WritebackQueueState::BACKGROUND_AGE:
                    return "BACKGROUND_AGE";
                case BufferPool::WritebackQueueState::CHECKPOINT:
                    return "CHECKPOINT";
                case BufferPool::WritebackQueueState::METADATA_PRIORITY:
                    return "METADATA_PRIORITY";
                case BufferPool::WritebackQueueState::WRITE_COMBINE:
                    return "WRITE_COMBINE";
                case BufferPool::WritebackQueueState::REPAIR_RETRY:
                    return "REPAIR_RETRY";
            }
            return "NONE";
        }

        auto isTemporaryWorkPageBuffer(const uint8_t *buffer, uint32_t page_size) -> bool
        {
            if (buffer == nullptr || page_size < sizeof(PageHeader))
            {
                return false;
            }

            const auto *header = reinterpret_cast<const PageHeader *>(buffer);
            return pageIsTemporaryWork(*header);
        }

        auto deriveCurrentPrefetchSessionKey() -> uint64_t
        {
            auto *ctx = ConnectionContext::getCurrent();
            if (ctx == nullptr)
            {
                return ANONYMOUS_PREFETCH_SESSION_KEY;
            }

            const auto session_id = ctx->effectiveSessionId();
            if (session_id != ConnectionContext::ID{})
            {
                const uint64_t hashed = static_cast<uint64_t>(
                    std::hash<ConnectionContext::ID>{}(session_id));
                return hashed == 0 ? ANONYMOUS_PREFETCH_SESSION_KEY : hashed;
            }

            const uint64_t proc_key = static_cast<uint64_t>(ctx->getProcId()) + 2;
            return proc_key == 0 ? ANONYMOUS_PREFETCH_SESSION_KEY : proc_key;
        }

        auto ghostHistoryCapForConfig(const BufferPool::Config &config) -> size_t
        {
            const uint32_t pct_cap = BufferPool::Config::pctToFrames(
                config.replacement_ghost_history_pct, config.pool_size);
            return static_cast<size_t>(
                std::max<uint32_t>(pct_cap, std::max<uint32_t>(8u, config.pool_size * 8)));
        }
    }

    BufferPool::BufferPool(Database *db, const Config &config) : db_(db), config_(config)
    {
        config_.recomputeDomainFrames();
        frames_.resize(config.pool_size);
        ghost_history_capacity_ = ghostHistoryCapForConfig(config_);
        ghost_history_.assign(ghost_history_capacity_, GhostEntry{});
    }

    BufferPool::~BufferPool()
    {
        shutdown();
    }

    auto BufferPool::initialize(ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (config_.layout == PoolLayout::HotCold || config_.layout == PoolLayout::Tablespace)
        {
            LOG_WARNING(GENERAL, "Buffer pool layout '%s' requested; using segmented logical foundation",
                        poolLayoutToString(config_.layout));
        }

        if (frames_.size() != config_.pool_size)
        {
            frames_.clear();
            frames_.resize(config_.pool_size);
        }
        lru_list_.clear();
        clearOwnershipPartitionsLocked();
        ghost_history_capacity_ = ghostHistoryCapForConfig(config_);
        ghost_history_.assign(ghost_history_capacity_, GhostEntry{});
        ghost_history_head_ = 0;
        ghost_history_count_ = 0;

        // Ensure page table partitions have buckets to avoid modulo-by-zero in hashing.
        const size_t per_partition = std::max<size_t>(1, config_.pool_size / NUM_PAGE_TABLE_PARTITIONS);
        for (auto& partition : page_table_partitions_)
        {
            partition.table.reserve(per_partition);
        }

        // Allocate memory for each frame
        for (uint32_t i = 0; i < config_.pool_size; i++)
        {
            try
            {
                frames_[i].data = std::make_unique<uint8_t[]>(config_.page_size);
            }
            catch (const std::bad_alloc &)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer pool memory");
                return Status::OOM;
            }

            // Initialize frame
            // PHASE 1, TASK 1.2.3: Changed page_id to gpid
            frames_[i].gpid = INVALID_GPID;
            // CRITICAL FIX (CRITICAL-1): Use atomic store for thread-safe write
            frames_[i].pin_count.store(0, std::memory_order_relaxed);
            frames_[i].is_dirty.store(false, std::memory_order_relaxed);
            frames_[i].usage_count.store(0, std::memory_order_relaxed);
            frames_[i].owner_partition.store(
                static_cast<uint16_t>(getOwnershipHomePartition(i)),
                std::memory_order_relaxed);
            frames_[i].home_partition.store(
                static_cast<uint16_t>(getOwnershipHomePartition(i)),
                std::memory_order_relaxed);
            resetFrameScaffolding(frames_[i]);

            // Add to LRU list (all frames start as free)
            lru_list_.push_back(i);
        }

        initializeOwnershipPartitionsLocked();
        initializeRingBuffers();

        metrics_ = &ScratchBirdMetrics::getInstance();
        metrics_->initialize();
        updatePoolTelemetry();
        updateDirtyTelemetry();

        // Start background writer if enabled (Issue 2.20)
        const char* disable_bgwriter = std::getenv("SCRATCHBIRD_DISABLE_BGWRITER");
        if (disable_bgwriter && disable_bgwriter[0] != '\0' && disable_bgwriter[0] != '0')
        {
            config_.enable_background_writer = false;
        }
        if (config_.enable_background_writer)
        {
            startBackgroundWriter();
        }

        return Status::OK;
    }

    auto BufferPool::shutdown(ErrorContext *ctx) -> Status
    {
        // Stop background writer first (before acquiring mutex to avoid deadlock)
        stopBackgroundWriter();

        // Flush all dirty pages
        Status status = flushAll(ctx);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            // Memory is freed automatically by unique_ptr
            // Clear data structures
            lru_list_.clear();
            clearOwnershipPartitionsLocked();
            ghost_history_.clear();
            ghost_history_head_ = 0;
            ghost_history_count_ = 0;
            ghost_history_capacity_ = 0;
        }

        // Clear page table partitions without nesting partition locks under mutex_.
        for (size_t i = 0; i < NUM_PAGE_TABLE_PARTITIONS; ++i)
        {
            std::lock_guard<std::mutex> partition_lock(page_table_partitions_[i].mutex);
            page_table_partitions_[i].table.clear();
        }

        disableStatsDebug();

        return status;
    }

    auto BufferPool::enableStatsDebug(const std::string &path, ErrorContext *ctx) -> bool
    {
        std::lock_guard<std::mutex> lock(stats_debug_mutex_);
        if (stats_debug_fp_ != nullptr)
        {
            std::fclose(stats_debug_fp_);
            stats_debug_fp_ = nullptr;
        }

        stats_debug_fp_ = std::fopen(path.c_str(), "a");
        if (!stats_debug_fp_)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open stats debug log");
            return false;
        }

        std::setvbuf(stats_debug_fp_, nullptr, _IOLBF, 0);
        stats_debug_enabled_.store(true, std::memory_order_release);
        stats_debug_seq_ = 0;
        std::fprintf(stats_debug_fp_, "seq,ts_us,tid,event,ctx,gpid,tablespace,page,hits,misses\n");
        return true;
    }

    void BufferPool::disableStatsDebug()
    {
        std::lock_guard<std::mutex> lock(stats_debug_mutex_);
        stats_debug_enabled_.store(false, std::memory_order_release);
        if (stats_debug_fp_)
        {
            std::fclose(stats_debug_fp_);
            stats_debug_fp_ = nullptr;
        }
    }

    void BufferPool::logStatsEvent(const char *event, GPID gpid)
    {
        if (!stats_debug_enabled_.load(std::memory_order_acquire))
        {
            return;
        }

        std::lock_guard<std::mutex> lock(stats_debug_mutex_);
        if (!stats_debug_fp_)
        {
            return;
        }

        auto now = std::chrono::system_clock::now();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
                          now.time_since_epoch())
                          .count();
        uint64_t tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        uint16_t tablespace_id = getTablespaceID(gpid);
        uint64_t page_number = getPageNumber(gpid);
        uint64_t hits = stats_.hits.load(std::memory_order_relaxed);
        uint64_t misses = stats_.misses.load(std::memory_order_relaxed);

        const char *ctx_label = ConnectionContext::getCurrent() ? "conn" : "sys";
        std::fprintf(stats_debug_fp_, "%lu,%ld,%lu,%s,%s,%lu,%u,%lu,%lu,%lu\n",
                     static_cast<unsigned long>(stats_debug_seq_++),
                     static_cast<long>(micros),
                     static_cast<unsigned long>(tid),
                     event,
                     ctx_label,
                     static_cast<unsigned long>(gpid),
                     static_cast<unsigned int>(tablespace_id),
                     static_cast<unsigned long>(page_number),
                     static_cast<unsigned long>(hits),
                     static_cast<unsigned long>(misses));
    }

    auto BufferPool::isIndexLikePageType(uint16_t page_type) -> bool
    {
        return page_type >= PAGE_TYPE_BTREE_META && page_type < PAGE_TYPE_COLUMNSTORE_META;
    }

    auto BufferPool::isSystemMetaPageType(uint16_t page_type) -> bool
    {
        switch (page_type)
        {
            case PAGE_TYPE_CATALOG_ROOT:
            case PAGE_TYPE_CATALOG_PAGE:
            case PAGE_TYPE_FSM_ROOT:
            case PAGE_TYPE_FSM_PAGE:
            case PAGE_TYPE_TOAST_META:
            case PAGE_TYPE_LOB_META:
            case PAGE_TYPE_NAME_REGISTRY:
            case PAGE_TYPE_BOOTSTRAP_RESERVED:
            case PAGE_TYPE_FILESPACE_HEADER:
                return true;
            default:
                return false;
        }
    }

    auto BufferPool::isIndexRootInternalPageType(uint16_t page_type) -> bool
    {
        switch (page_type)
        {
            case PAGE_TYPE_BTREE_META:
            case PAGE_TYPE_BTREE_INTERNAL:
            case PAGE_TYPE_HASH_META:
            case PAGE_TYPE_GIN_META:
            case PAGE_TYPE_GIST_INTERNAL:
            case PAGE_TYPE_SPGIST_META:
            case PAGE_TYPE_SPGIST_INNER:
            case PAGE_TYPE_BRIN_META:
            case PAGE_TYPE_BRIN_REVMAP:
            case PAGE_TYPE_BITMAP_META:
            case PAGE_TYPE_BITMAP_DICT:
            case PAGE_TYPE_INVERTED_META:
            case PAGE_TYPE_INVERTED_DICT:
            case PAGE_TYPE_SPARSE_META:
            case PAGE_TYPE_SPARSE_DICT:
            case PAGE_TYPE_FTS_META:
            case PAGE_TYPE_FTS_DICT:
            case PAGE_TYPE_TRIE_META:
            case PAGE_TYPE_SPATIAL_META:
            case PAGE_TYPE_MINHASH_META:
            case PAGE_TYPE_BLOOM_META:
            case PAGE_TYPE_SAI_META:
            case PAGE_TYPE_SAI_TERM_DICT:
            case PAGE_TYPE_SASI_META:
            case PAGE_TYPE_SASI_TERM_DICT:
            case PAGE_TYPE_COLUMNSTORE_META:
            case PAGE_TYPE_LSM_META:
            case PAGE_TYPE_DOC_PATH_DICTIONARY:
            case PAGE_TYPE_TS_MEASUREMENT_ROOT:
            case PAGE_TYPE_TS_SERIES_INDEX:
            case PAGE_TYPE_COL_SEGMENT_HEADER:
            case PAGE_TYPE_COL_DICTIONARY:
            case PAGE_TYPE_SEARCH_TERM_DICT:
            case PAGE_TYPE_VECTOR_QUANTIZER:
            case PAGE_TYPE_LSM_RUN_MANIFEST:
            case PAGE_TYPE_RETENTION_MANIFEST:
                return true;
            default:
                return false;
        }
    }

    auto BufferPool::defaultWorkloadClass(AccessStrategy strategy,
                                          bool prefetch_request) -> WorkloadClass
    {
        if (prefetch_request)
        {
            return WorkloadClass::PrefetchSpeculative;
        }

        switch (strategy)
        {
            case AccessStrategy::Sequential:
                return WorkloadClass::SequentialScan;
            case AccessStrategy::Vacuum:
                return WorkloadClass::SweepGc;
            case AccessStrategy::BulkWrite:
                return WorkloadClass::BulkWrite;
            case AccessStrategy::Normal:
            default:
                return WorkloadClass::PointLookup;
        }
    }

    auto BufferPool::classifyPageType(const uint8_t *buffer,
                                      uint32_t page_size,
                                      WorkloadClass workload_class) -> MgaPageClass
    {
        const bool scan_overlay =
            workload_class == WorkloadClass::SequentialScan ||
            workload_class == WorkloadClass::SweepGc ||
            workload_class == WorkloadClass::BulkWrite ||
            workload_class == WorkloadClass::PrefetchSpeculative;

        if (buffer == nullptr || page_size < sizeof(PageHeader))
        {
            return scan_overlay ? MgaPageClass::SCAN_PROBATION : MgaPageClass::Generic;
        }

        const auto *header = reinterpret_cast<const PageHeader *>(buffer);
        if (pageIsTemporaryWork(*header))
        {
            return MgaPageClass::TEMP_WORK;
        }

        if (header->magic != K_MAGIC_SBRD)
        {
            return scan_overlay ? MgaPageClass::SCAN_PROBATION : MgaPageClass::Generic;
        }

        switch (header->page_type)
        {
            case PAGE_TYPE_DATABASE_HEADER:
                // Only the bootstrap header page is authoritative transaction state.
                // A non-bootstrap page with type 0 most commonly means a formerly raw
                // page image was canonicalized before its owning structure assigned a
                // concrete non-bootstrap page type.
                if (header->page_id == BOOTSTRAP_PAGE_DATABASE_HEADER)
                {
                    return MgaPageClass::TX_STATE;
                }
                break;
            case PAGE_TYPE_SYSTEM_STATE:
                // Same rule as PAGE_TYPE_DATABASE_HEADER: only the fixed bootstrap
                // page carries transaction-state authority.
                if (header->page_id == BOOTSTRAP_PAGE_SYSTEM_STATE)
                {
                    return MgaPageClass::TX_STATE;
                }
                break;
            case PAGE_TYPE_TRANSACTION_MAP:
                return MgaPageClass::TX_STATE;
            default:
                break;
        }

        if (isSystemMetaPageType(header->page_type))
        {
            return MgaPageClass::SYSTEM_META;
        }
        if (isIndexRootInternalPageType(header->page_type))
        {
            return MgaPageClass::INDEX_ROOT_INTERNAL;
        }
        if (scan_overlay && !isIndexLikePageType(header->page_type))
        {
            return MgaPageClass::SCAN_PROBATION;
        }
        if (isIndexLikePageType(header->page_type))
        {
            return MgaPageClass::INDEX_CHURN;
        }
        return MgaPageClass::Generic;
    }

    auto BufferPool::classifyPolicyDomain(const uint8_t *buffer,
                                          uint32_t page_size,
                                          MgaPageClass page_class,
                                          WorkloadClass workload_class) -> PolicyDomain
    {
        // Page role outranks workload hint for temp and version-support pages.
        // A non-temp durable page may not be moved into the temporary domain
        // merely because the caller supplied a TemporaryWork hint.
        if (page_class == MgaPageClass::TEMP_WORK || isTemporaryWorkPageBuffer(buffer, page_size))
        {
            return PolicyDomain::TemporaryWork;
        }

        switch (page_class)
        {
            case MgaPageClass::TX_STATE:
            case MgaPageClass::SYSTEM_META:
                return PolicyDomain::CriticalSystem;
            case MgaPageClass::INDEX_ROOT_INTERNAL:
                return PolicyDomain::HotOltp;
            case MgaPageClass::VERSION_ROOT:
            case MgaPageClass::CHAIN_HEAVY:
            case MgaPageClass::GC_CANDIDATE:
                return PolicyDomain::VersionUndo;
            case MgaPageClass::SCAN_PROBATION:
                return PolicyDomain::ScanBulkRing;
            case MgaPageClass::INDEX_CHURN:
            case MgaPageClass::Generic:
            case MgaPageClass::TEMP_WORK:
                break;
        }

        switch (workload_class)
        {
            case WorkloadClass::RangeScan:
                return PolicyDomain::ReadMostly;
            case WorkloadClass::SequentialScan:
            case WorkloadClass::SweepGc:
            case WorkloadClass::BulkWrite:
            case WorkloadClass::PrefetchSpeculative:
                return PolicyDomain::ScanBulkRing;
            case WorkloadClass::PointLookup:
            case WorkloadClass::IndexProbe:
            case WorkloadClass::NestedLoopReread:
                return PolicyDomain::HotOltp;
            case WorkloadClass::CheckpointCleaner:
            case WorkloadClass::RecoveryReplay:
            case WorkloadClass::TemporaryWork:
            case WorkloadClass::Unspecified:
                break;
        }

        return PolicyDomain::HotOltp;
    }

    auto BufferPool::classifyResidencyTier(WorkloadClass workload_class,
                                           MgaPageClass page_class) -> ResidencyTier
    {
        if (isVersionUndoClass(page_class))
        {
            return ResidencyTier::Protected;
        }
        if (page_class == MgaPageClass::SCAN_PROBATION ||
            workload_class == WorkloadClass::SequentialScan ||
            workload_class == WorkloadClass::SweepGc ||
            workload_class == WorkloadClass::BulkWrite ||
            workload_class == WorkloadClass::PrefetchSpeculative)
        {
            return ResidencyTier::RingOnly;
        }

        return ResidencyTier::LegacyShared;
    }

    auto BufferPool::evictionRankForTier(ResidencyTier tier) -> uint32_t
    {
        switch (tier)
        {
            case ResidencyTier::RingOnly:
                return 0;
            case ResidencyTier::Probationary:
                return 1;
            case ResidencyTier::LegacyShared:
                return 2;
            case ResidencyTier::Protected:
                return 3;
            case ResidencyTier::PinBiased:
                return 4;
        }
        return 4;
    }

    auto BufferPool::isVersionUndoClass(MgaPageClass page_class) -> bool
    {
        return page_class == MgaPageClass::VERSION_ROOT ||
               page_class == MgaPageClass::CHAIN_HEAVY ||
               page_class == MgaPageClass::GC_CANDIDATE;
    }

    auto BufferPool::isDirectProtectClass(MgaPageClass page_class) -> bool
    {
        return page_class == MgaPageClass::INDEX_ROOT_INTERNAL;
    }

    auto BufferPool::isDirectProtectSystemMetaPageType(uint16_t page_type) -> bool
    {
        switch (page_type)
        {
            case PAGE_TYPE_CATALOG_ROOT:
            case PAGE_TYPE_FSM_ROOT:
            case PAGE_TYPE_NAME_REGISTRY:
            case PAGE_TYPE_BOOTSTRAP_RESERVED:
            case PAGE_TYPE_FILESPACE_HEADER:
            case PAGE_TYPE_TOAST_META:
            case PAGE_TYPE_LOB_META:
                return true;
            default:
                return false;
        }
    }

    auto BufferPool::isHardReservedDomain(PolicyDomain domain) -> bool
    {
        return domain == PolicyDomain::CriticalSystem ||
               domain == PolicyDomain::VersionUndo;
    }

    auto BufferPool::protectedFrameBudget() const -> uint32_t
    {
        if (config_.pool_size == 0)
        {
            return 0;
        }
        return std::min(config_.pool_size,
                        std::max(1u, Config::pctToFrames(config_.replacement_protected_pct,
                                                         config_.pool_size)));
    }

    auto BufferPool::objectIdForBuffer(const uint8_t *buffer, uint32_t page_size) -> uint32_t
    {
        if (buffer == nullptr || page_size < sizeof(PageHeader))
        {
            return 0;
        }

        const auto *header = reinterpret_cast<const PageHeader *>(buffer);
        if (header->magic != K_MAGIC_SBRD)
        {
            return 0;
        }

        bool all_zero = true;
        uint32_t hash = 2166136261u;
        for (uint8_t byte : header->object_uuid)
        {
            if (byte != 0)
            {
                all_zero = false;
            }
            hash ^= static_cast<uint32_t>(byte);
            hash *= 16777619u;
        }

        if (all_zero)
        {
            return 0;
        }
        return hash == 0 ? 1u : hash;
    }

    auto BufferPool::objectProtectionBudget() const -> uint32_t
    {
        if (config_.pool_size == 0)
        {
            return 0;
        }
        return std::min(config_.pool_size,
                        std::max(1u, Config::pctToFrames(config_.thrash_object_budget_pct,
                                                         config_.pool_size)));
    }

    auto BufferPool::sessionPrefetchBudget() const -> uint32_t
    {
        if (config_.pool_size == 0)
        {
            return 0;
        }
        return std::min(config_.pool_size,
                        std::max(1u, Config::pctToFrames(config_.thrash_session_budget_pct,
                                                         config_.pool_size)));
    }

    auto BufferPool::prefetchPressureBudget() const -> uint32_t
    {
        if (config_.pool_size == 0)
        {
            return 0;
        }
        return std::min(config_.pool_size,
                        std::max(1u, Config::pctToFrames(config_.thrash_prefetch_pressure_pct,
                                                         config_.pool_size)));
    }

    auto BufferPool::currentProtectedFrameCount() const -> uint32_t
    {
        uint32_t count = 0;
        for (const auto &frame : frames_)
        {
            const auto lifecycle = static_cast<LifecycleState>(
                frame.lifecycle_state.load(std::memory_order_relaxed));
            if (lifecycle == LifecycleState::Free)
            {
                continue;
            }

            const auto tier = static_cast<ResidencyTier>(
                frame.residency_tier.load(std::memory_order_relaxed));
            // Pin-biased frames are a separate hard reservation and must not
            // consume the soft protected budget used for probationary
            // promotion/demotion decisions.
            if (tier == ResidencyTier::Protected)
            {
                ++count;
            }
        }
        return count;
    }

    auto BufferPool::currentProtectedFrameCountForObject(uint32_t object_id,
                                                         uint32_t excluding_frame) const
        -> uint32_t
    {
        if (object_id == 0)
        {
            return 0;
        }

        uint32_t count = 0;
        for (uint32_t frame_index = 0; frame_index < frames_.size(); ++frame_index)
        {
            if (frame_index == excluding_frame)
            {
                continue;
            }

            const auto &frame = frames_[frame_index];
            const auto lifecycle = static_cast<LifecycleState>(
                frame.lifecycle_state.load(std::memory_order_relaxed));
            if (lifecycle == LifecycleState::Free || frame.gpid == INVALID_GPID)
            {
                continue;
            }

            const auto tier = static_cast<ResidencyTier>(
                frame.residency_tier.load(std::memory_order_relaxed));
            if (tier != ResidencyTier::Protected)
            {
                continue;
            }

            if (frame.object_id.load(std::memory_order_relaxed) == object_id)
            {
                ++count;
            }
        }
        return count;
    }

    auto BufferPool::collectDomainResidentCountsLocked() const -> DomainCounterArray
    {
        DomainCounterArray counts{};
        for (const auto &frame : frames_)
        {
            const auto lifecycle = static_cast<LifecycleState>(
                frame.lifecycle_state.load(std::memory_order_relaxed));
            if (lifecycle == LifecycleState::Free)
            {
                continue;
            }

            const auto domain = static_cast<PolicyDomain>(
                frame.policy_domain.load(std::memory_order_relaxed));
            counts[static_cast<size_t>(domain)]++;
        }
        return counts;
    }

    auto BufferPool::domainBorrowedPagesLocked(PolicyDomain domain,
                                               const DomainCounterArray &resident_counts) const
        -> uint32_t
    {
        const auto &budget = config_.domainBudget(domain);
        const uint32_t resident = resident_counts[static_cast<size_t>(domain)];

        if (budget.max_frames > 0 && resident > budget.max_frames)
        {
            return resident - budget.max_frames;
        }
        if (budget.target_frames > 0 && resident > budget.target_frames)
        {
            return resident - budget.target_frames;
        }
        return 0;
    }

    auto BufferPool::canEvictFromDomainLocked(PolicyDomain domain,
                                              const DomainCounterArray &resident_counts) const
        -> bool
    {
        if (!isHardReservedDomain(domain))
        {
            return true;
        }

        const auto &budget = config_.domainBudget(domain);
        const uint32_t resident = resident_counts[static_cast<size_t>(domain)];
        if (budget.min_frames == 0)
        {
            return true;
        }
        return resident > budget.min_frames;
    }

    auto BufferPool::domainEvictionPriorityLocked(PolicyDomain domain,
                                                  const DomainCounterArray &resident_counts) const
        -> uint32_t
    {
        const auto &budget = config_.domainBudget(domain);
        const uint32_t resident = resident_counts[static_cast<size_t>(domain)];

        const bool above_max = budget.max_frames > 0 && resident > budget.max_frames;
        const bool above_target = budget.target_frames > 0 && resident > budget.target_frames;
        const bool above_min = budget.min_frames == 0 || resident > budget.min_frames;

        switch (domain)
        {
            case PolicyDomain::ScanBulkRing:
                return above_max ? 0u : 2u;
            case PolicyDomain::TemporaryWork:
                return above_max ? 1u : 3u;
            case PolicyDomain::ReadMostly:
                return above_target ? 4u : 6u;
            case PolicyDomain::HotOltp:
                return above_target ? 5u : 6u;
            case PolicyDomain::VersionUndo:
                return above_min ? 7u : 9u;
            case PolicyDomain::CriticalSystem:
                return above_min ? 8u : 10u;
            case PolicyDomain::Count:
                break;
        }
        return 10u;
    }

    auto BufferPool::consumeGhostHistory(GPID gpid) -> bool
    {
        std::lock_guard<std::mutex> ghost_lock(ghost_history_mutex_);
        if (ghost_history_count_ == 0 || ghost_history_capacity_ == 0)
        {
            return false;
        }

        auto entryIndex = [&](size_t logical_offset) -> size_t {
            return (ghost_history_head_ + logical_offset) % ghost_history_capacity_;
        };

        auto removeEntryAt = [&](size_t logical_offset) {
            for (size_t move = logical_offset + 1; move < ghost_history_count_; ++move)
            {
                ghost_history_[entryIndex(move - 1)] = ghost_history_[entryIndex(move)];
            }
            ghost_history_[entryIndex(ghost_history_count_ - 1)] = GhostEntry{};
            --ghost_history_count_;
            if (ghost_history_count_ == 0)
            {
                ghost_history_head_ = 0;
            }
        };

        for (size_t logical_offset = 0; logical_offset < ghost_history_count_; ++logical_offset)
        {
            if (ghost_history_[entryIndex(logical_offset)].gpid != gpid)
            {
                continue;
            }

            removeEntryAt(logical_offset);
            stats_.mga_ghost_history_hits.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    void BufferPool::recordGhostHistory(GPID gpid,
                                        PolicyDomain domain,
                                        MgaPageClass page_class,
                                        ResidencyTier residency_tier,
                                        uint64_t eviction_generation,
                                        GhostEvictionReason reason)
    {
        const size_t cap = ghostHistoryCapForConfig(config_);
        if (cap == 0 || gpid == INVALID_GPID)
        {
            return;
        }

        std::lock_guard<std::mutex> ghost_lock(ghost_history_mutex_);
        if (ghost_history_capacity_ != cap || ghost_history_.size() != cap)
        {
            ghost_history_.assign(cap, GhostEntry{});
            ghost_history_capacity_ = cap;
            ghost_history_head_ = 0;
            ghost_history_count_ = 0;
        }

        auto entryIndex = [&](size_t logical_offset) -> size_t {
            return (ghost_history_head_ + logical_offset) % ghost_history_capacity_;
        };

        auto removeEntryAt = [&](size_t logical_offset) {
            for (size_t move = logical_offset + 1; move < ghost_history_count_; ++move)
            {
                ghost_history_[entryIndex(move - 1)] = ghost_history_[entryIndex(move)];
            }
            ghost_history_[entryIndex(ghost_history_count_ - 1)] = GhostEntry{};
            --ghost_history_count_;
            if (ghost_history_count_ == 0)
            {
                ghost_history_head_ = 0;
            }
        };

        for (size_t logical_offset = 0; logical_offset < ghost_history_count_;)
        {
            if (ghost_history_[entryIndex(logical_offset)].gpid == gpid)
            {
                removeEntryAt(logical_offset);
                continue;
            }
            ++logical_offset;
        }

        const GhostEntry entry{
            gpid, domain, page_class, residency_tier, eviction_generation, reason};
        if (ghost_history_count_ < ghost_history_capacity_)
        {
            ghost_history_[entryIndex(ghost_history_count_)] = entry;
            ++ghost_history_count_;
        }
        else
        {
            ghost_history_[ghost_history_head_] = entry;
            ghost_history_head_ = (ghost_history_head_ + 1) % ghost_history_capacity_;
        }
    }

    void BufferPool::resetFrameScaffolding(Frame &frame)
    {
        frame.object_id.store(0, std::memory_order_relaxed);
        frame.mga_page_class.store(static_cast<uint8_t>(MgaPageClass::Generic),
                                   std::memory_order_relaxed);
        frame.policy_domain.store(static_cast<uint8_t>(PolicyDomain::HotOltp),
                                  std::memory_order_relaxed);
        frame.workload_class.store(static_cast<uint8_t>(WorkloadClass::Unspecified),
                                   std::memory_order_relaxed);
        frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::LegacyShared),
                                   std::memory_order_relaxed);
        frame.lifecycle_state.store(static_cast<uint8_t>(LifecycleState::Free),
                                    std::memory_order_relaxed);
        frame.dirty_state.store(static_cast<uint8_t>(DirtyState::Clean),
                                std::memory_order_relaxed);
        frame.oldest_interesting_txid.store(0, std::memory_order_relaxed);
        frame.prune_safe_horizon_hint.store(0, std::memory_order_relaxed);
        frame.dead_version_bytes.store(0, std::memory_order_relaxed);
        frame.chain_depth_hint.store(0, std::memory_order_relaxed);
        frame.prefetch_session_key.store(0, std::memory_order_relaxed);
        frame.last_gc_touch_generation.store(0, std::memory_order_relaxed);
        frame.scan_probation_generation.store(0, std::memory_order_relaxed);
        frame.state_generation.store(0, std::memory_order_relaxed);
        frame.io_generation.store(0, std::memory_order_relaxed);
        frame.dirty_generation.store(0, std::memory_order_relaxed);
        frame.last_flush_generation.store(0, std::memory_order_relaxed);
        frame.checkpoint_target_generation.store(0, std::memory_order_relaxed);
        frame.admission_generation.store(0, std::memory_order_relaxed);
        frame.last_touch_generation.store(0, std::memory_order_relaxed);
        frame.temperature_generation.store(0, std::memory_order_relaxed);
        frame.writeback_queue_state.store(static_cast<uint8_t>(WritebackQueueState::NONE),
                                          std::memory_order_relaxed);
        frame.speculative_prefetch.store(false, std::memory_order_relaxed);
        frame.prefetch_consumed.store(false, std::memory_order_relaxed);
        frame.commit_fence_member.store(false, std::memory_order_relaxed);
    }

    void BufferPool::purgeFramePageTableMappings(uint32_t frame_index)
    {
        if (frame_index >= frames_.size())
        {
            return;
        }

        uint32_t purged_entries = 0;
        for (auto& partition : page_table_partitions_)
        {
            std::lock_guard<std::mutex> partition_lock(partition.mutex);
            for (auto it = partition.table.begin(); it != partition.table.end();)
            {
                if (it->second == frame_index)
                {
                    it = partition.table.erase(it);
                    ++purged_entries;
                    continue;
                }
                ++it;
            }
        }

        if (purged_entries != 0)
        {
            LOG_WARNING(BUFFER,
                        "Recovered %u stale page-table entr%s before reusing frame %u",
                        purged_entries,
                        purged_entries == 1 ? "y" : "ies",
                        frame_index);
        }
    }

    void BufferPool::clearOwnershipPartitionsLocked()
    {
        for (auto &partition : ownership_partitions_)
        {
            partition.owned_frames.clear();
            partition.free_frames.clear();
            partition.victim_cursor = 0;
        }
    }

    void BufferPool::initializeOwnershipPartitionsLocked()
    {
        clearOwnershipPartitionsLocked();
        for (uint32_t frame_index = 0; frame_index < config_.pool_size; ++frame_index)
        {
            const size_t owner_partition = getOwnershipHomePartition(frame_index);
            Frame &frame = frames_[frame_index];
            frame.owner_partition.store(static_cast<uint16_t>(owner_partition),
                                        std::memory_order_relaxed);
            frame.home_partition.store(static_cast<uint16_t>(owner_partition),
                                       std::memory_order_relaxed);
            ownership_partitions_[owner_partition].owned_frames.push_back(frame_index);
            ownership_partitions_[owner_partition].free_frames.push_back(frame_index);
        }
    }

    void BufferPool::resetFrameIdentity(Frame &frame,
                                        size_t owner_partition,
                                        bool keep_home_partition)
    {
        frame.gpid = INVALID_GPID;
        frame.pin_count.store(0, std::memory_order_relaxed);
        frame.is_dirty.store(false, std::memory_order_relaxed);
        frame.usage_count.store(0, std::memory_order_relaxed);
        frame.owner_partition.store(static_cast<uint16_t>(owner_partition),
                                    std::memory_order_relaxed);
        if (!keep_home_partition)
        {
            frame.home_partition.store(static_cast<uint16_t>(owner_partition),
                                       std::memory_order_relaxed);
        }
        resetFrameScaffolding(frame);
    }

    void BufferPool::stageFrameForOwnership(Frame &frame,
                                            size_t owner_partition,
                                            LifecycleState lifecycle_state)
    {
        frame.owner_partition.store(static_cast<uint16_t>(owner_partition),
                                    std::memory_order_relaxed);
        frame.lifecycle_state.store(static_cast<uint8_t>(lifecycle_state),
                                    std::memory_order_relaxed);
        frame.state_generation.fetch_add(1, std::memory_order_relaxed);
    }

    void BufferPool::releaseFrameToOwnershipFreeListLocked(uint32_t frame_index,
                                                           size_t owner_partition)
    {
        OwnershipPartition &partition = ownership_partitions_[owner_partition];
        if (std::find(partition.free_frames.begin(),
                      partition.free_frames.end(),
                      frame_index) == partition.free_frames.end())
        {
            partition.free_frames.push_back(frame_index);
        }
    }

    void BufferPool::transferFrameOwnershipLocked(uint32_t frame_index,
                                                  size_t from_partition,
                                                  size_t to_partition)
    {
        if (from_partition == to_partition)
        {
            frames_[frame_index].owner_partition.store(static_cast<uint16_t>(to_partition),
                                                       std::memory_order_relaxed);
            return;
        }

        eraseOwnedFrameIndex(ownership_partitions_[from_partition].owned_frames, frame_index);
        auto &donor_free = ownership_partitions_[from_partition].free_frames;
        donor_free.erase(std::remove(donor_free.begin(),
                                     donor_free.end(),
                                     frame_index),
                         donor_free.end());
        ownership_partitions_[to_partition].owned_frames.push_back(frame_index);
        frames_[frame_index].owner_partition.store(static_cast<uint16_t>(to_partition),
                                                   std::memory_order_relaxed);
        frames_[frame_index].state_generation.fetch_add(1, std::memory_order_relaxed);
    }

    auto BufferPool::tryClaimFreeFrameLocked(size_t owner_partition,
                                             uint32_t &frame_index) -> bool
    {
        OwnershipPartition &partition = ownership_partitions_[owner_partition];
        while (!partition.free_frames.empty())
        {
            frame_index = partition.free_frames.front();
            partition.free_frames.pop_front();

            Frame &frame = frames_[frame_index];
            if (frame.gpid != INVALID_GPID ||
                frame.pin_count.load(std::memory_order_relaxed) != 0)
            {
                continue;
            }

            const auto lifecycle = static_cast<LifecycleState>(
                frame.lifecycle_state.load(std::memory_order_relaxed));
            if (lifecycle != LifecycleState::Free && lifecycle != LifecycleState::Error)
            {
                continue;
            }

            resetFrameIdentity(frame, owner_partition, true);
            stageFrameForOwnership(frame, owner_partition, LifecycleState::Loading);
            return true;
        }

        return false;
    }

    auto BufferPool::tryReclaimHomeFreeFrameLocked(size_t owner_partition,
                                                   uint32_t &frame_index) -> bool
    {
        for (size_t offset = 1; offset < NUM_PAGE_TABLE_PARTITIONS; ++offset)
        {
            const size_t donor_partition =
                (owner_partition + offset) % NUM_PAGE_TABLE_PARTITIONS;
            if (donor_partition == owner_partition)
            {
                continue;
            }

            std::unique_lock<std::mutex> donor_lock(
                ownership_partitions_[donor_partition].mutex,
                std::try_to_lock);
            if (!donor_lock.owns_lock())
            {
                continue;
            }

            auto &donor = ownership_partitions_[donor_partition];
            auto &donor_free = donor.free_frames;
            for (auto it = donor_free.begin(); it != donor_free.end();)
            {
                frame_index = *it;
                Frame &frame = frames_[frame_index];
                if (frame.home_partition.load(std::memory_order_relaxed) != owner_partition)
                {
                    ++it;
                    continue;
                }
                it = donor_free.erase(it);

                if (frame.gpid != INVALID_GPID ||
                    frame.pin_count.load(std::memory_order_relaxed) != 0)
                {
                    continue;
                }

                const auto lifecycle = static_cast<LifecycleState>(
                    frame.lifecycle_state.load(std::memory_order_relaxed));
                if (lifecycle != LifecycleState::Free &&
                    lifecycle != LifecycleState::Error)
                {
                    continue;
                }

                transferFrameOwnershipLocked(frame_index,
                                             donor_partition,
                                             owner_partition);
                resetFrameIdentity(frame, owner_partition, true);
                stageFrameForOwnership(frame, owner_partition, LifecycleState::Loading);
                return true;
            }
        }

        return false;
    }

    auto BufferPool::tryStealFreeFrameLocked(size_t owner_partition,
                                             uint32_t &frame_index) -> bool
    {
        for (size_t offset = 1; offset < NUM_PAGE_TABLE_PARTITIONS; ++offset)
        {
            const size_t donor_partition =
                (owner_partition + offset) % NUM_PAGE_TABLE_PARTITIONS;
            if (donor_partition == owner_partition)
            {
                continue;
            }

            std::unique_lock<std::mutex> donor_lock(
                ownership_partitions_[donor_partition].mutex,
                std::try_to_lock);
            if (!donor_lock.owns_lock())
            {
                continue;
            }

            auto &donor = ownership_partitions_[donor_partition];
            while (!donor.free_frames.empty())
            {
                frame_index = donor.free_frames.front();
                donor.free_frames.pop_front();

                Frame &frame = frames_[frame_index];
                if (frame.gpid != INVALID_GPID ||
                    frame.pin_count.load(std::memory_order_relaxed) != 0)
                {
                    continue;
                }

                const auto lifecycle = static_cast<LifecycleState>(
                    frame.lifecycle_state.load(std::memory_order_relaxed));
                if (lifecycle != LifecycleState::Free &&
                    lifecycle != LifecycleState::Error)
                {
                    continue;
                }

                transferFrameOwnershipLocked(frame_index,
                                             donor_partition,
                                             owner_partition);
                resetFrameIdentity(frame, owner_partition, true);
                stageFrameForOwnership(frame, owner_partition, LifecycleState::Loading);
                return true;
            }
        }

        return false;
    }

    auto BufferPool::selectOwnedVictimFrameLocked(size_t owner_partition,
                                                  uint32_t &candidate_frame,
                                                  uint16_t required_home_partition,
                                                  uint32_t *candidate_rank_out,
                                                  bool *protected_collapse_out,
                                                  bool *aging_progress_out) -> bool
    {
        if (candidate_rank_out != nullptr)
        {
            *candidate_rank_out = UINT32_MAX;
        }
        if (protected_collapse_out != nullptr)
        {
            *protected_collapse_out = false;
        }
        if (aging_progress_out != nullptr)
        {
            *aging_progress_out = false;
        }

        OwnershipPartition &partition = ownership_partitions_[owner_partition];
        if (partition.owned_frames.empty())
        {
            return false;
        }

        constexpr uint32_t MAX_PASSES = 2;
        candidate_frame = UINT32_MAX;
        uint32_t candidate_rank = UINT32_MAX;
        bool candidate_dirty = true;
        uint32_t protected_candidate_frame = UINT32_MAX;
        const uint64_t current_generation =
            residency_generation_clock_.load(std::memory_order_relaxed);
        uint32_t protected_frames = currentProtectedFrameCount();
        const uint32_t protected_budget = protectedFrameBudget();
        const DomainCounterArray resident_counts = collectDomainResidentCountsLocked();
        std::array<bool, static_cast<size_t>(PolicyDomain::Count)>
            reservation_skip_recorded{};

        for (uint32_t pass = 0; pass < MAX_PASSES; ++pass)
        {
            if (partition.owned_frames.empty())
            {
                break;
            }

            const size_t owned_count = partition.owned_frames.size();
            for (size_t scanned = 0; scanned < owned_count; ++scanned)
            {
                if (partition.victim_cursor >= partition.owned_frames.size())
                {
                    partition.victim_cursor = 0;
                }

                const uint32_t frame_index =
                    partition.owned_frames[partition.victim_cursor++];
                Frame &frame = frames_[frame_index];
                const auto lifecycle = static_cast<LifecycleState>(
                    frame.lifecycle_state.load(std::memory_order_relaxed));
                if (lifecycle != LifecycleState::Valid &&
                    lifecycle != LifecycleState::Error)
                {
                    continue;
                }
                if (frame.pin_count.load(std::memory_order_relaxed) != 0 ||
                    frame.gpid == INVALID_GPID)
                {
                    continue;
                }
                if (required_home_partition != UINT16_MAX &&
                    frame.home_partition.load(std::memory_order_relaxed) !=
                        required_home_partition)
                {
                    continue;
                }

                const MgaPageClass page_class = static_cast<MgaPageClass>(
                    frame.mga_page_class.load(std::memory_order_relaxed));
                ResidencyTier residency_tier = static_cast<ResidencyTier>(
                    frame.residency_tier.load(std::memory_order_relaxed));
                const PolicyDomain domain = static_cast<PolicyDomain>(
                    frame.policy_domain.load(std::memory_order_relaxed));
                const bool commit_fence_member =
                    frame.commit_fence_member.load(std::memory_order_relaxed);
                const bool hard_protected = isHardProtectedFrame(frame);

                if (commit_fence_member || hard_protected ||
                    residency_tier == ResidencyTier::PinBiased)
                {
                    continue;
                }

                if (!canEvictFromDomainLocked(domain, resident_counts))
                {
                    const size_t domain_index = static_cast<size_t>(domain);
                    if (!reservation_skip_recorded[domain_index])
                    {
                        domain_reservation_breach_counts_[domain_index].fetch_add(
                            1, std::memory_order_relaxed);
                        reservation_skip_recorded[domain_index] = true;
                    }
                    continue;
                }

                const uint32_t usage_count =
                    frame.usage_count.load(std::memory_order_relaxed);
                if (usage_count != 0)
                {
                    frame.usage_count.fetch_sub(1, std::memory_order_relaxed);
                    if (aging_progress_out != nullptr)
                    {
                        *aging_progress_out = true;
                    }
                    continue;
                }

                if (residency_tier == ResidencyTier::Protected)
                {
                    const uint64_t last_touch_generation =
                        frame.last_touch_generation.load(std::memory_order_relaxed);
                    const uint64_t age = (current_generation > last_touch_generation)
                                             ? (current_generation - last_touch_generation)
                                             : 0;
                    const uint64_t max_protected_age = std::max<uint64_t>(
                        4, static_cast<uint64_t>(
                               config_.admission_second_touch_generations) * 2);
                    if (protected_frames > protected_budget || age > max_protected_age)
                    {
                        frame.residency_tier.store(
                            static_cast<uint8_t>(ResidencyTier::Probationary),
                            std::memory_order_relaxed);
                        stats_.mga_residency_demotions.fetch_add(
                            1, std::memory_order_relaxed);
                        residency_tier = ResidencyTier::Probationary;
                        if (aging_progress_out != nullptr)
                        {
                            *aging_progress_out = true;
                        }
                        if (protected_frames > 0)
                        {
                            --protected_frames;
                        }
                    }
                    else
                    {
                        if (protected_candidate_frame == UINT32_MAX)
                        {
                            protected_candidate_frame = frame_index;
                        }
                        continue;
                    }
                }

                const bool dirty = frame.is_dirty.load(std::memory_order_relaxed);
                const uint32_t rank =
                    (domainEvictionPriorityLocked(domain, resident_counts) << 8) |
                    (evictionRankForTier(residency_tier) << 4) |
                    evictionRankForClass(page_class);
                if (candidate_frame == UINT32_MAX || rank < candidate_rank ||
                    (rank == candidate_rank && candidate_dirty && !dirty))
                {
                    candidate_frame = frame_index;
                    candidate_rank = rank;
                    candidate_dirty = dirty;
                }
            }

            if (candidate_frame != UINT32_MAX)
            {
                if (candidate_rank_out != nullptr)
                {
                    *candidate_rank_out = candidate_rank;
                }
                return true;
            }
        }

        if (protected_candidate_frame != UINT32_MAX &&
            commit_fence_depth_.load(std::memory_order_acquire) == 0)
        {
            candidate_frame = protected_candidate_frame;
            if (candidate_rank_out != nullptr)
            {
                *candidate_rank_out = UINT32_MAX - 1;
            }
            if (protected_collapse_out != nullptr)
            {
                *protected_collapse_out = true;
            }
            stats_.mga_protected_set_collapse_events.fetch_add(
                1, std::memory_order_relaxed);
            return true;
        }

        return false;
    }

    auto BufferPool::selectBestDonorVictimFrameLocked(size_t owner_partition,
                                                      uint32_t &frame_index,
                                                      size_t &donor_partition,
                                                      uint32_t &candidate_rank,
                                                      uint16_t required_home_partition,
                                                      bool *aging_progress_out) -> bool
    {
        frame_index = UINT32_MAX;
        donor_partition = owner_partition;
        candidate_rank = UINT32_MAX;
        bool found_candidate = false;
        if (aging_progress_out != nullptr)
        {
            *aging_progress_out = false;
        }

        for (size_t offset = 1; offset < NUM_PAGE_TABLE_PARTITIONS; ++offset)
        {
            const size_t current_donor_partition =
                (owner_partition + offset) % NUM_PAGE_TABLE_PARTITIONS;
            if (current_donor_partition == owner_partition)
            {
                continue;
            }

            std::unique_lock<std::mutex> donor_lock(
                ownership_partitions_[current_donor_partition].mutex,
                std::try_to_lock);
            if (!donor_lock.owns_lock())
            {
                continue;
            }

            uint32_t donor_candidate = UINT32_MAX;
            uint32_t donor_rank = UINT32_MAX;
            bool protected_collapse = false;
            bool donor_aging_progress = false;
            if (!selectOwnedVictimFrameLocked(current_donor_partition,
                                              donor_candidate,
                                              required_home_partition,
                                              &donor_rank,
                                              &protected_collapse,
                                              &donor_aging_progress))
            {
                if (donor_aging_progress && aging_progress_out != nullptr)
                {
                    *aging_progress_out = true;
                }
                continue;
            }
            if (donor_aging_progress && aging_progress_out != nullptr)
            {
                *aging_progress_out = true;
            }

            if (!found_candidate || donor_rank < candidate_rank)
            {
                frame_index = donor_candidate;
                donor_partition = current_donor_partition;
                candidate_rank = donor_rank;
                found_candidate = true;
            }
        }

        return found_candidate;
    }

    auto BufferPool::tryReclaimHomeVictimFrameLocked(size_t owner_partition,
                                                     uint32_t &frame_index,
                                                     ErrorContext *ctx) -> Status
    {
        for (size_t attempt = 0; attempt < Frame::MAX_USAGE_COUNT + 1; ++attempt)
        {
            size_t donor_partition = owner_partition;
            uint32_t donor_candidate = UINT32_MAX;
            uint32_t donor_rank = UINT32_MAX;
            bool aging_progress = false;
            if (!selectBestDonorVictimFrameLocked(owner_partition,
                                                  donor_candidate,
                                                  donor_partition,
                                                  donor_rank,
                                                  static_cast<uint16_t>(owner_partition),
                                                  &aging_progress))
            {
                if (aging_progress)
                {
                    continue;
                }
                return Status::INVALID_ARGUMENT;
            }

            const Status status = evictSpecificFrame(donor_candidate, ctx);
            if (status == Status::OK)
            {
                transferFrameOwnershipLocked(donor_candidate,
                                             donor_partition,
                                             owner_partition);
                resetFrameIdentity(frames_[donor_candidate],
                                   owner_partition,
                                   true);
                stageFrameForOwnership(frames_[donor_candidate],
                                       owner_partition,
                                       LifecycleState::Loading);
                frame_index = donor_candidate;
                return Status::OK;
            }
            if (status != Status::INVALID_ARGUMENT)
            {
                return status;
            }
        }

        return Status::INVALID_ARGUMENT;
    }

    auto BufferPool::tryClaimOwnedVictimFrameLocked(size_t owner_partition,
                                                    uint32_t &frame_index,
                                                    ErrorContext *ctx) -> Status
    {
        for (size_t attempt = 0; attempt < Frame::MAX_USAGE_COUNT + 1; ++attempt)
        {
            bool aging_progress = false;
            if (!selectOwnedVictimFrameLocked(owner_partition,
                                              frame_index,
                                              UINT16_MAX,
                                              nullptr,
                                              nullptr,
                                              &aging_progress))
            {
                if (aging_progress)
                {
                    continue;
                }
                return Status::INVALID_ARGUMENT;
            }

            const Status status = evictSpecificFrame(frame_index, ctx);
            if (status == Status::OK)
            {
                resetFrameIdentity(frames_[frame_index], owner_partition, true);
                stageFrameForOwnership(frames_[frame_index],
                                       owner_partition,
                                       LifecycleState::Loading);
                return Status::OK;
            }
            if (status != Status::INVALID_ARGUMENT)
            {
                return status;
            }
        }

        return Status::INVALID_ARGUMENT;
    }

    auto BufferPool::tryStealOwnedVictimFrameLocked(size_t owner_partition,
                                                    uint32_t &frame_index,
                                                    ErrorContext *ctx) -> Status
    {
        for (size_t attempt = 0; attempt < Frame::MAX_USAGE_COUNT + 1; ++attempt)
        {
            size_t donor_partition = owner_partition;
            uint32_t donor_candidate = UINT32_MAX;
            uint32_t donor_rank = UINT32_MAX;
            bool aging_progress = false;
            if (!selectBestDonorVictimFrameLocked(owner_partition,
                                                  donor_candidate,
                                                  donor_partition,
                                                  donor_rank,
                                                  UINT16_MAX,
                                                  &aging_progress))
            {
                if (aging_progress)
                {
                    continue;
                }
                return Status::INVALID_ARGUMENT;
            }

            const Status status = evictSpecificFrame(donor_candidate, ctx);
            if (status == Status::OK)
            {
                transferFrameOwnershipLocked(donor_candidate,
                                             donor_partition,
                                             owner_partition);
                resetFrameIdentity(frames_[donor_candidate],
                                   owner_partition,
                                   true);
                stageFrameForOwnership(frames_[donor_candidate],
                                       owner_partition,
                                       LifecycleState::Loading);
                frame_index = donor_candidate;
                return Status::OK;
            }
            if (status != Status::INVALID_ARGUMENT)
            {
                return status;
            }
        }

        return Status::INVALID_ARGUMENT;
    }

    auto BufferPool::claimFrameForSegmentedMiss(size_t owner_partition,
                                                uint32_t &frame_index,
                                                ErrorContext *ctx) -> Status
    {
        if (tryClaimFreeFrameLocked(owner_partition, frame_index))
        {
            return Status::OK;
        }
        if (tryReclaimHomeFreeFrameLocked(owner_partition, frame_index))
        {
            return Status::OK;
        }
        if (tryStealFreeFrameLocked(owner_partition, frame_index))
        {
            return Status::OK;
        }

        const Status home_reclaim_status =
            tryReclaimHomeVictimFrameLocked(owner_partition, frame_index, ctx);
        if (home_reclaim_status == Status::OK)
        {
            return Status::OK;
        }
        if (home_reclaim_status != Status::INVALID_ARGUMENT)
        {
            return home_reclaim_status;
        }

        for (size_t attempt = 0; attempt < Frame::MAX_USAGE_COUNT + 1; ++attempt)
        {
            uint32_t local_candidate = UINT32_MAX;
            uint32_t local_rank = UINT32_MAX;
            bool local_aging_progress = false;
            const bool have_local = selectOwnedVictimFrameLocked(owner_partition,
                                                                 local_candidate,
                                                                 UINT16_MAX,
                                                                 &local_rank,
                                                                 nullptr,
                                                                 &local_aging_progress);

            size_t donor_partition = owner_partition;
            uint32_t donor_candidate = UINT32_MAX;
            uint32_t donor_rank = UINT32_MAX;
            bool donor_aging_progress = false;
            const bool have_donor = selectBestDonorVictimFrameLocked(owner_partition,
                                                                     donor_candidate,
                                                                     donor_partition,
                                                                     donor_rank,
                                                                     UINT16_MAX,
                                                                     &donor_aging_progress);

            if (!have_local && !have_donor)
            {
                if (local_aging_progress || donor_aging_progress)
                {
                    continue;
                }
                break;
            }

            const bool choose_donor =
                have_donor && (!have_local || donor_rank <= local_rank);
            if (choose_donor)
            {
                const Status donor_status = evictSpecificFrame(donor_candidate, ctx);
                if (donor_status == Status::OK)
                {
                    transferFrameOwnershipLocked(donor_candidate,
                                                 donor_partition,
                                                 owner_partition);
                    resetFrameIdentity(frames_[donor_candidate],
                                       owner_partition,
                                       true);
                    stageFrameForOwnership(frames_[donor_candidate],
                                           owner_partition,
                                           LifecycleState::Loading);
                    frame_index = donor_candidate;
                    return Status::OK;
                }
                if (donor_status != Status::INVALID_ARGUMENT)
                {
                    return donor_status;
                }
                continue;
            }

            const Status local_status = evictSpecificFrame(local_candidate, ctx);
            if (local_status == Status::OK)
            {
                resetFrameIdentity(frames_[local_candidate], owner_partition, true);
                stageFrameForOwnership(frames_[local_candidate],
                                       owner_partition,
                                       LifecycleState::Loading);
                frame_index = local_candidate;
                return Status::OK;
            }
            if (local_status != Status::INVALID_ARGUMENT)
            {
                return local_status;
            }
        }

        uint32_t resident_frames = 0;
        uint32_t pinned_frames = 0;
        uint32_t loading_frames = 0;
        uint32_t protected_frames = 0;
        uint32_t pin_biased_frames = 0;
        uint32_t critical_system_frames = 0;
        uint32_t version_undo_frames = 0;
        uint32_t owned_resident_frames = 0;
        uint32_t owned_pinned_frames = 0;
        uint32_t owned_loading_frames = 0;
        const DomainCounterArray resident_counts = collectDomainResidentCountsLocked();

        for (uint32_t candidate = 0; candidate < frames_.size(); ++candidate)
        {
            Frame &frame = frames_[candidate];
            const auto lifecycle = static_cast<LifecycleState>(
                frame.lifecycle_state.load(std::memory_order_relaxed));
            if (lifecycle == LifecycleState::Free)
            {
                continue;
            }

            ++resident_frames;
            if (frame.pin_count.load(std::memory_order_relaxed) != 0)
            {
                ++pinned_frames;
            }
            if (lifecycle == LifecycleState::Loading)
            {
                ++loading_frames;
            }

            const auto tier = static_cast<ResidencyTier>(
                frame.residency_tier.load(std::memory_order_relaxed));
            if (tier == ResidencyTier::Protected)
            {
                ++protected_frames;
            }
            else if (tier == ResidencyTier::PinBiased)
            {
                ++pin_biased_frames;
            }

            const auto domain = static_cast<PolicyDomain>(
                frame.policy_domain.load(std::memory_order_relaxed));
            if (domain == PolicyDomain::CriticalSystem)
            {
                ++critical_system_frames;
            }
            else if (domain == PolicyDomain::VersionUndo)
            {
                ++version_undo_frames;
            }

            if (frame.owner_partition.load(std::memory_order_relaxed) != owner_partition)
            {
                continue;
            }

            ++owned_resident_frames;
            if (frame.pin_count.load(std::memory_order_relaxed) != 0)
            {
                ++owned_pinned_frames;
            }
            if (lifecycle == LifecycleState::Loading)
            {
                ++owned_loading_frames;
            }
        }

        std::ostringstream summary;
        summary << "Buffer pool partition has no evictable or transferrable frame"
                << " owner=" << owner_partition
                << " owned=" << ownership_partitions_[owner_partition].owned_frames.size()
                << " free=" << ownership_partitions_[owner_partition].free_frames.size()
                << " owned_resident=" << owned_resident_frames
                << " owned_pinned=" << owned_pinned_frames
                << " owned_loading=" << owned_loading_frames
                << " resident=" << resident_frames
                << " pinned=" << pinned_frames
                << " loading=" << loading_frames
                << " protected=" << protected_frames
                << " pin_biased=" << pin_biased_frames
                << " critical_system=" << critical_system_frames
                << " version_undo=" << version_undo_frames
                << " critical_system_resident="
                << resident_counts[static_cast<size_t>(PolicyDomain::CriticalSystem)]
                << " critical_system_min="
                << config_.domainBudget(PolicyDomain::CriticalSystem).min_frames
                << " version_undo_resident="
                << resident_counts[static_cast<size_t>(PolicyDomain::VersionUndo)]
                << " version_undo_min="
                << config_.domainBudget(PolicyDomain::VersionUndo).min_frames;
        LOG_ERROR(BUFFER, "%s", summary.str().c_str());
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, summary.str().c_str());
        return Status::INVALID_ARGUMENT;
    }

    auto BufferPool::classifyWritebackQueueState(const Frame &frame) -> WritebackQueueState
    {
        const DirtyState dirty_state = static_cast<DirtyState>(
            frame.dirty_state.load(std::memory_order_relaxed));
        if (dirty_state == DirtyState::DirtyFailed)
        {
            return WritebackQueueState::REPAIR_RETRY;
        }
        if (frame.checkpoint_target_generation.load(std::memory_order_relaxed) != 0)
        {
            return WritebackQueueState::CHECKPOINT;
        }
        if (frame.commit_fence_member.load(std::memory_order_relaxed))
        {
            return WritebackQueueState::FOREGROUND_HELP;
        }

        const MgaPageClass page_class = static_cast<MgaPageClass>(
            frame.mga_page_class.load(std::memory_order_relaxed));
        switch (page_class)
        {
            case MgaPageClass::TX_STATE:
            case MgaPageClass::SYSTEM_META:
            case MgaPageClass::INDEX_ROOT_INTERNAL:
            case MgaPageClass::VERSION_ROOT:
                return WritebackQueueState::METADATA_PRIORITY;
            case MgaPageClass::CHAIN_HEAVY:
            case MgaPageClass::GC_CANDIDATE:
            case MgaPageClass::INDEX_CHURN:
                return WritebackQueueState::WRITE_COMBINE;
            case MgaPageClass::Generic:
            case MgaPageClass::SCAN_PROBATION:
            case MgaPageClass::TEMP_WORK:
            default:
                return WritebackQueueState::BACKGROUND_AGE;
        }
    }

    void BufferPool::refreshFrameObjectId(uint32_t frame_index)
    {
        if (frame_index >= frames_.size())
        {
            return;
        }
        frames_[frame_index].object_id.store(
            objectIdForBuffer(frames_[frame_index].data.get(), config_.page_size),
            std::memory_order_relaxed);
    }

    void BufferPool::markFramePrefetched(uint32_t frame_index, uint64_t session_key)
    {
        if (frame_index >= frames_.size())
        {
            return;
        }

        Frame &frame = frames_[frame_index];
        const bool already_prefetched =
            frame.speculative_prefetch.exchange(true, std::memory_order_relaxed);
        frame.prefetch_session_key.store(session_key, std::memory_order_relaxed);
        frame.prefetch_consumed.store(false, std::memory_order_relaxed);
        if (!already_prefetched)
        {
            stats_.prefetch_pages_total.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void BufferPool::consumePrefetchDebt(uint32_t frame_index, WorkloadClass workload_class)
    {
        if (frame_index >= frames_.size() ||
            workload_class == WorkloadClass::PrefetchSpeculative)
        {
            return;
        }

        Frame &frame = frames_[frame_index];
        if (!frame.speculative_prefetch.exchange(false, std::memory_order_relaxed))
        {
            return;
        }

        frame.prefetch_consumed.store(true, std::memory_order_relaxed);
        frame.prefetch_session_key.store(0, std::memory_order_relaxed);
        stats_.prefetch_pages_useful.fetch_add(1, std::memory_order_relaxed);
    }

    void BufferPool::recordUnusedPrefetchEviction(const Frame &frame)
    {
        if (!frame.speculative_prefetch.load(std::memory_order_relaxed) ||
            frame.prefetch_consumed.load(std::memory_order_relaxed))
        {
            return;
        }

        stats_.prefetch_pages_unused_evicted.fetch_add(1, std::memory_order_relaxed);
    }

    auto BufferPool::currentOutstandingPrefetchDebtPages(uint64_t session_key) const -> uint32_t
    {
        uint32_t count = 0;
        for (const auto &frame : frames_)
        {
            if (frame.gpid == INVALID_GPID ||
                !frame.speculative_prefetch.load(std::memory_order_relaxed))
            {
                continue;
            }

            if (session_key != 0 &&
                frame.prefetch_session_key.load(std::memory_order_relaxed) != session_key)
            {
                continue;
            }
            ++count;
        }
        return count;
    }

    auto BufferPool::currentOutstandingPrefetchDebtPages(PolicyDomain domain) const -> uint32_t
    {
        uint32_t count = 0;
        for (const auto &frame : frames_)
        {
            if (frame.gpid == INVALID_GPID ||
                !frame.speculative_prefetch.load(std::memory_order_relaxed))
            {
                continue;
            }

            if (static_cast<PolicyDomain>(frame.policy_domain.load(std::memory_order_relaxed)) !=
                domain)
            {
                continue;
            }
            ++count;
        }
        return count;
    }

    auto BufferPool::evaluateThrashState(uint64_t session_key) const -> ThrashDetectorState
    {
        const uint32_t global_debt = currentOutstandingPrefetchDebtPages();
        if (config_.prefetch_max_debt_pages != 0 &&
            global_debt >= config_.prefetch_max_debt_pages)
        {
            return ThrashDetectorState::GlobalDebtCap;
        }

        if (session_key != 0 && currentOutstandingPrefetchDebtPages(session_key) >=
                                   sessionPrefetchBudget())
        {
            return ThrashDetectorState::SessionBudgetCap;
        }

        const auto scan_domain = currentOutstandingPrefetchDebtPages(PolicyDomain::ScanBulkRing);
        if (scan_domain >= prefetchPressureBudget())
        {
            return ThrashDetectorState::ScanPressure;
        }

        const uint64_t useful = stats_.prefetch_pages_useful.load(std::memory_order_relaxed);
        const uint64_t unused =
            stats_.prefetch_pages_unused_evicted.load(std::memory_order_relaxed);
        const uint64_t denominator = useful + unused;
        if (denominator >= 4)
        {
            const double usefulness_pct =
                (static_cast<double>(useful) * 100.0) / static_cast<double>(denominator);
            if (usefulness_pct < static_cast<double>(config_.prefetch_usefulness_floor_pct))
            {
                return ThrashDetectorState::UsefulnessCollapse;
            }
        }

        return ThrashDetectorState::None;
    }

    void BufferPool::publishThrashState(ThrashDetectorState state)
    {
        const uint8_t encoded = static_cast<uint8_t>(state);
        const uint8_t prior = thrash_detector_state_.exchange(encoded, std::memory_order_relaxed);
        if (prior != encoded && state != ThrashDetectorState::None)
        {
            stats_.thrash_policy_shift_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    auto BufferPool::canPromoteFrameToProtected(uint32_t frame_index) const -> bool
    {
        if (frame_index >= frames_.size())
        {
            return false;
        }

        const Frame &frame = frames_[frame_index];
        const MgaPageClass page_class = static_cast<MgaPageClass>(
            frame.mga_page_class.load(std::memory_order_relaxed));
        if (isHardProtectedFrame(frame) || isVersionUndoClass(page_class) ||
            page_class == MgaPageClass::SYSTEM_META ||
            page_class == MgaPageClass::INDEX_ROOT_INTERNAL)
        {
            return true;
        }

        const uint32_t object_id = frame.object_id.load(std::memory_order_relaxed);
        if (object_id == 0)
        {
            return true;
        }

        return currentProtectedFrameCountForObject(object_id, frame_index) <
               objectProtectionBudget();
    }

    void BufferPool::noteFrameAccess(uint32_t frame_index,
                                     WorkloadClass workload_class,
                                     bool ghost_hit)
    {
        Frame &frame = frames_[frame_index];
        const MgaPageClass page_class = static_cast<MgaPageClass>(
            frame.mga_page_class.load(std::memory_order_relaxed));
        const bool hard_protected_tx_state =
            page_class == MgaPageClass::TX_STATE &&
            isHardProtectedFrame(frame);
        const uint64_t generation =
            residency_generation_clock_.fetch_add(1, std::memory_order_relaxed) + 1;

        frame.last_touch_generation.store(generation, std::memory_order_relaxed);
        frame.temperature_generation.store(generation, std::memory_order_relaxed);
        frame.lifecycle_state.store(static_cast<uint8_t>(LifecycleState::Valid),
                                    std::memory_order_relaxed);
        consumePrefetchDebt(frame_index, workload_class);
        if (!frame.is_dirty.load(std::memory_order_relaxed))
        {
            frame.dirty_state.store(static_cast<uint8_t>(DirtyState::Clean),
                                    std::memory_order_relaxed);
        }

        const bool ring_only = page_class == MgaPageClass::SCAN_PROBATION ||
                               (!isVersionUndoClass(page_class) &&
                                (workload_class == WorkloadClass::SequentialScan ||
                                 workload_class == WorkloadClass::SweepGc ||
                                 workload_class == WorkloadClass::BulkWrite ||
                                 workload_class == WorkloadClass::PrefetchSpeculative));
        if (config_.layout != PoolLayout::Segmented)
        {
            if (frame.admission_generation.load(std::memory_order_relaxed) == 0)
            {
                frame.admission_generation.store(generation, std::memory_order_relaxed);
            }

            if (page_class == MgaPageClass::TX_STATE)
            {
                frame.usage_count.store(hard_protected_tx_state ? Frame::MAX_USAGE_COUNT : 1u,
                                        std::memory_order_relaxed);
                frame.residency_tier.store(
                    static_cast<uint8_t>(hard_protected_tx_state
                                             ? ResidencyTier::PinBiased
                                             : ResidencyTier::LegacyShared),
                    std::memory_order_relaxed);
                return;
            }

            if (ring_only)
            {
                frame.usage_count.store(1, std::memory_order_relaxed);
                frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::RingOnly),
                                           std::memory_order_relaxed);
                return;
            }

            // The legacy `single` layout must remain a simple shared cache.
            // MMW-004's probationary/protected tiers are only authoritative for
            // the canonical segmented layout.
            frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::LegacyShared),
                                       std::memory_order_relaxed);
            return;
        }

        if (page_class == MgaPageClass::TX_STATE)
        {
            if (frame.admission_generation.load(std::memory_order_relaxed) == 0)
            {
                frame.admission_generation.store(generation, std::memory_order_relaxed);
            }
            if (hard_protected_tx_state)
            {
                frame.usage_count.store(Frame::MAX_USAGE_COUNT, std::memory_order_relaxed);
                frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::PinBiased),
                                           std::memory_order_relaxed);
            }
            else
            {
                const uint32_t current_usage =
                    frame.usage_count.load(std::memory_order_relaxed);
                // Audit contract: only bootstrap-critical transaction-state pages remain
                // pin-biased. Ordinary transaction-map leaves still live in the
                // CriticalSystem domain, but they must remain evictable above the
                // reservation floor so user-page admission cannot starve behind TIP/CLOG churn.
                frame.usage_count.store(
                    std::max<uint32_t>(1u,
                                       std::min<uint32_t>(2u, current_usage + 1u)),
                    std::memory_order_relaxed);
                frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::Probationary),
                                           std::memory_order_relaxed);
            }
            return;
        }

        bool direct_protect_system_meta = false;
        if (page_class == MgaPageClass::SYSTEM_META && frame.data != nullptr)
        {
            const auto *header = reinterpret_cast<const PageHeader *>(frame.data.get());
            if (header->magic == K_MAGIC_SBRD &&
                isDirectProtectSystemMetaPageType(header->page_type))
            {
                direct_protect_system_meta = true;
            }
        }

        if (config_.admission_direct_protect_roots &&
            (isDirectProtectClass(page_class) || direct_protect_system_meta))
        {
            if (frame.admission_generation.load(std::memory_order_relaxed) == 0)
            {
                frame.admission_generation.store(generation, std::memory_order_relaxed);
            }
            frame.usage_count.store(Frame::MAX_USAGE_COUNT, std::memory_order_relaxed);
            frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::Protected),
                                       std::memory_order_relaxed);
            return;
        }

        if (ring_only)
        {
            if (frame.admission_generation.load(std::memory_order_relaxed) == 0)
            {
                frame.admission_generation.store(generation, std::memory_order_relaxed);
            }
            frame.usage_count.store(1, std::memory_order_relaxed);
            frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::RingOnly),
                                       std::memory_order_relaxed);
            return;
        }

        if (page_class == MgaPageClass::SYSTEM_META)
        {
            if (frame.admission_generation.load(std::memory_order_relaxed) == 0)
            {
                frame.admission_generation.store(generation, std::memory_order_relaxed);
            }
            const uint32_t current_usage =
                frame.usage_count.load(std::memory_order_relaxed);
            frame.usage_count.store(std::max<uint32_t>(1u, std::min<uint32_t>(2u,
                                                                              current_usage + 1u)),
                                   std::memory_order_relaxed);
            // Critical catalog/FSM lineage stays in the CriticalSystem domain,
            // but only explicit root metadata pages consume the protected set.
            // Ordinary system pages must not promote on second touch during
            // bootstrap or policy reload churn.
            frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::Probationary),
                                       std::memory_order_relaxed);
            return;
        }

        if (isVersionUndoClass(page_class))
        {
            if (frame.admission_generation.load(std::memory_order_relaxed) == 0)
            {
                frame.admission_generation.store(generation, std::memory_order_relaxed);
            }
            const uint32_t current_usage =
                frame.usage_count.load(std::memory_order_relaxed);
            frame.usage_count.store(
                std::max<uint32_t>(2u, std::min<uint32_t>(Frame::MAX_USAGE_COUNT,
                                                          current_usage + 1u)),
                std::memory_order_relaxed);
            // Sweep/GC may touch version-support pages with scan-like workload
            // hints, but those pages still belong to the VersionUndo
            // reservation and must not collapse into the scan ring.
            frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::Protected),
                                       std::memory_order_relaxed);
            return;
        }

        const ResidencyTier current_tier = static_cast<ResidencyTier>(
            frame.residency_tier.load(std::memory_order_relaxed));
        uint64_t admission_generation =
            frame.admission_generation.load(std::memory_order_relaxed);
        const bool first_touch = (admission_generation == 0);

        if (first_touch)
        {
            admission_generation = generation;
            frame.admission_generation.store(generation, std::memory_order_relaxed);
        }

        if (ghost_hit)
        {
            frame.usage_count.store(Frame::MAX_USAGE_COUNT, std::memory_order_relaxed);
            if (canPromoteFrameToProtected(frame_index))
            {
                frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::Protected),
                                           std::memory_order_relaxed);
                stats_.mga_admission_promotions.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::Probationary),
                                           std::memory_order_relaxed);
                stats_.fairness_object_budget_breaches.fetch_add(1,
                                                                 std::memory_order_relaxed);
            }
            return;
        }

        if (current_tier == ResidencyTier::Protected)
        {
            return;
        }

        const uint64_t admission_delta =
            (generation > admission_generation) ? (generation - admission_generation) : 0;
        if (!first_touch &&
            (current_tier == ResidencyTier::Probationary ||
             current_tier == ResidencyTier::LegacyShared) &&
            admission_delta <= std::max<uint32_t>(1, config_.admission_second_touch_generations))
        {
            frame.usage_count.store(Frame::MAX_USAGE_COUNT, std::memory_order_relaxed);
            if (canPromoteFrameToProtected(frame_index))
            {
                frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::Protected),
                                           std::memory_order_relaxed);
                stats_.mga_admission_promotions.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::Probationary),
                                           std::memory_order_relaxed);
                stats_.fairness_object_budget_breaches.fetch_add(1,
                                                                 std::memory_order_relaxed);
            }
            return;
        }

        frame.admission_generation.store(generation, std::memory_order_relaxed);
        frame.usage_count.store(1, std::memory_order_relaxed);
        frame.residency_tier.store(static_cast<uint8_t>(ResidencyTier::Probationary),
                                   std::memory_order_relaxed);
    }

    void BufferPool::beginFrameWriteback(uint32_t frame_index,
                                         WritebackQueueState queue_state,
                                         uint64_t checkpoint_target_generation)
    {
        frames_[frame_index].lifecycle_state.store(static_cast<uint8_t>(LifecycleState::Flushing),
                                                   std::memory_order_relaxed);
        frames_[frame_index].dirty_state.store(static_cast<uint8_t>(DirtyState::DirtyInFlight),
                                               std::memory_order_relaxed);
        frames_[frame_index].checkpoint_target_generation.store(checkpoint_target_generation,
                                                                std::memory_order_relaxed);
        frames_[frame_index].writeback_queue_state.store(static_cast<uint8_t>(queue_state),
                                                         std::memory_order_relaxed);
    }

    void BufferPool::markFrameWritebackFailure(uint32_t frame_index,
                                               WritebackQueueState queue_state)
    {
        frames_[frame_index].lifecycle_state.store(static_cast<uint8_t>(LifecycleState::Error),
                                                   std::memory_order_relaxed);
        frames_[frame_index].dirty_state.store(static_cast<uint8_t>(DirtyState::DirtyFailed),
                                               std::memory_order_relaxed);
        (void)queue_state;
        frames_[frame_index].writeback_queue_state.store(
                                                         static_cast<uint8_t>(WritebackQueueState::REPAIR_RETRY),
                                                         std::memory_order_relaxed);
    }

    auto BufferPool::evictionRankForClass(MgaPageClass page_class) -> uint32_t
    {
        switch (page_class)
        {
            case MgaPageClass::SCAN_PROBATION:
                return 0;
            case MgaPageClass::TEMP_WORK:
                return 1;
            case MgaPageClass::Generic:
                return 2;
            case MgaPageClass::GC_CANDIDATE:
                return 3;
            case MgaPageClass::INDEX_CHURN:
                return 4;
            case MgaPageClass::VERSION_ROOT:
                return 5;
            case MgaPageClass::CHAIN_HEAVY:
                return 6;
            case MgaPageClass::INDEX_ROOT_INTERNAL:
                return 7;
            case MgaPageClass::SYSTEM_META:
                return 8;
            case MgaPageClass::TX_STATE:
                return 9;
        }
        return 9;
    }

    auto BufferPool::isBootstrapCriticalTxStateFrame(const Frame &frame) -> bool
    {
        if (frame.gpid == INVALID_GPID || frame.data == nullptr)
        {
            return false;
        }

        const auto *header = reinterpret_cast<const PageHeader *>(frame.data.get());
        if (header->magic != K_MAGIC_SBRD)
        {
            return false;
        }

        switch (header->page_type)
        {
            case PAGE_TYPE_DATABASE_HEADER:
                return getTablespaceID(frame.gpid) == PRIMARY_TABLESPACE_ID &&
                       getPageNumber(frame.gpid) == BOOTSTRAP_PAGE_DATABASE_HEADER;
            case PAGE_TYPE_SYSTEM_STATE:
                return getTablespaceID(frame.gpid) == PRIMARY_TABLESPACE_ID &&
                       getPageNumber(frame.gpid) == BOOTSTRAP_PAGE_SYSTEM_STATE;
            case PAGE_TYPE_TRANSACTION_MAP:
                return getTablespaceID(frame.gpid) == PRIMARY_TABLESPACE_ID &&
                       getPageNumber(frame.gpid) == BOOTSTRAP_PAGE_TX_MAP_ROOT;
            default:
                return false;
        }
    }

    auto BufferPool::isHardProtectedFrame(const Frame &frame) -> bool
    {
        const MgaPageClass page_class = static_cast<MgaPageClass>(
            frame.mga_page_class.load(std::memory_order_relaxed));
        return page_class == MgaPageClass::TX_STATE &&
               isBootstrapCriticalTxStateFrame(frame);
    }

    void BufferPool::applyAutomaticMgaClassification(uint32_t frame_index,
                                                     WorkloadClass workload_class)
    {
        if (frame_index >= frames_.size())
        {
            return;
        }

        Frame &frame = frames_[frame_index];
        refreshFrameObjectId(frame_index);
        const MgaPageClass current_class =
            static_cast<MgaPageClass>(frame.mga_page_class.load(std::memory_order_relaxed));
        const bool temporary_page =
            current_class == MgaPageClass::TEMP_WORK ||
            isTemporaryWorkPageBuffer(frame.data.get(), config_.page_size);
        WorkloadClass effective_workload = workload_class;
        if (temporary_page)
        {
            effective_workload = WorkloadClass::TemporaryWork;
        }
        else if (effective_workload == WorkloadClass::TemporaryWork)
        {
            effective_workload = WorkloadClass::Unspecified;
        }
        const MgaPageClass detected_class =
            classifyPageType(frame.data.get(), config_.page_size, effective_workload);
        frame.workload_class.store(static_cast<uint8_t>(effective_workload),
                                   std::memory_order_relaxed);

        if (detected_class == MgaPageClass::SCAN_PROBATION && !isVersionUndoClass(current_class))
        {
            // Audit contract: sweep/GC traffic may discover generic scan pages, but it must not
            // overwrite an existing VersionUndo classification. VERSION_ROOT / CHAIN_HEAVY /
            // GC_CANDIDATE frames stay in the protected version-support domain even when later
            // accesses arrive through AccessStrategy::Vacuum or other scan-like workload hints.
            frame.mga_page_class.store(static_cast<uint8_t>(MgaPageClass::SCAN_PROBATION),
                                       std::memory_order_relaxed);
            frame.policy_domain.store(
                static_cast<uint8_t>(classifyPolicyDomain(frame.data.get(),
                                                          config_.page_size,
                                                          MgaPageClass::SCAN_PROBATION,
                                                          effective_workload)),
                std::memory_order_relaxed);
            frame.scan_probation_generation.store(
                mga_scan_generation_.fetch_add(1, std::memory_order_relaxed) + 1,
                std::memory_order_relaxed);
            return;
        }

        const bool detected_structural_class =
            detected_class == MgaPageClass::TX_STATE ||
            detected_class == MgaPageClass::SYSTEM_META ||
            detected_class == MgaPageClass::INDEX_ROOT_INTERNAL ||
            detected_class == MgaPageClass::INDEX_CHURN ||
            detected_class == MgaPageClass::TEMP_WORK;
        if (current_class == MgaPageClass::SCAN_PROBATION || current_class == MgaPageClass::Generic ||
            detected_structural_class)
        {
            frame.mga_page_class.store(static_cast<uint8_t>(detected_class),
                                       std::memory_order_relaxed);
            frame.policy_domain.store(
                static_cast<uint8_t>(classifyPolicyDomain(frame.data.get(),
                                                          config_.page_size,
                                                          detected_class,
                                                          effective_workload)),
                std::memory_order_relaxed);
            if (detected_class != MgaPageClass::SCAN_PROBATION)
            {
                frame.scan_probation_generation.store(0, std::memory_order_relaxed);
            }
        }
    }

    void BufferPool::recordMgaEviction(MgaPageClass page_class)
    {
        switch (page_class)
        {
            case MgaPageClass::TX_STATE:
                stats_.mga_evictions_tx_state.fetch_add(1, std::memory_order_relaxed);
                break;
            case MgaPageClass::SYSTEM_META:
                stats_.mga_evictions_system_meta.fetch_add(1, std::memory_order_relaxed);
                break;
            case MgaPageClass::INDEX_ROOT_INTERNAL:
                stats_.mga_evictions_index_root_internal.fetch_add(1, std::memory_order_relaxed);
                break;
            case MgaPageClass::VERSION_ROOT:
                stats_.mga_evictions_version_root.fetch_add(1, std::memory_order_relaxed);
                break;
            case MgaPageClass::CHAIN_HEAVY:
                stats_.mga_evictions_chain_heavy.fetch_add(1, std::memory_order_relaxed);
                break;
            case MgaPageClass::GC_CANDIDATE:
                stats_.mga_evictions_gc_candidate.fetch_add(1, std::memory_order_relaxed);
                break;
            case MgaPageClass::SCAN_PROBATION:
                stats_.mga_evictions_scan_probation.fetch_add(1, std::memory_order_relaxed);
                break;
            case MgaPageClass::INDEX_CHURN:
                stats_.mga_evictions_index_churn.fetch_add(1, std::memory_order_relaxed);
                break;
            case MgaPageClass::TEMP_WORK:
                stats_.mga_evictions_temp_work.fetch_add(1, std::memory_order_relaxed);
                break;
            case MgaPageClass::Generic:
            default:
                break;
        }
    }

    void BufferPool::offerGcCandidate(uint32_t frame_index)
    {
        if (frame_index >= frames_.size())
        {
            return;
        }

        const GPID gpid = frames_[frame_index].gpid;
        if (gpid == INVALID_GPID || db_ == nullptr)
        {
            return;
        }

        auto *gc = db_->garbage_collector();
        if (gc != nullptr && getTablespaceID(gpid) == PRIMARY_TABLESPACE_ID)
        {
            gc->markPageDirty(static_cast<uint32_t>(getPageNumber(gpid)));
            stats_.mga_gc_handoff_offers.fetch_add(1, std::memory_order_relaxed);
        }
    }

    auto BufferPool::publishMgaFrameHintsGlobal(GPID gpid,
                                                const MgaFrameHints &hints,
                                                ErrorContext *ctx) -> Status
    {
        size_t partition_idx = getPartitionIndex(gpid);
        auto &partition = page_table_partitions_[partition_idx];
        std::lock_guard<std::mutex> partition_lock(partition.mutex);
        auto it = partition.table.find(gpid);
        if (it == partition.table.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Page not in buffer pool");
            return Status::NOT_FOUND;
        }

        Frame &frame = frames_[it->second];
        WorkloadClass effective_workload = hints.workload_class;
        if (effective_workload == WorkloadClass::Unspecified)
        {
            effective_workload = static_cast<WorkloadClass>(
                frame.workload_class.load(std::memory_order_relaxed));
        }
        if (effective_workload == WorkloadClass::Unspecified)
        {
            effective_workload = defaultWorkloadClass(AccessStrategy::Normal);
        }
        const bool temporary_page =
            hints.page_class == MgaPageClass::TEMP_WORK ||
            isTemporaryWorkPageBuffer(frame.data.get(), config_.page_size);
        if (temporary_page)
        {
            effective_workload = WorkloadClass::TemporaryWork;
        }
        else if (effective_workload == WorkloadClass::TemporaryWork)
        {
            const WorkloadClass current_workload = static_cast<WorkloadClass>(
                frame.workload_class.load(std::memory_order_relaxed));
            effective_workload = current_workload == WorkloadClass::TemporaryWork
                                     ? defaultWorkloadClass(AccessStrategy::Normal)
                                     : current_workload;
        }

        frame.mga_page_class.store(static_cast<uint8_t>(hints.page_class),
                                   std::memory_order_relaxed);
        frame.policy_domain.store(
            static_cast<uint8_t>(
                classifyPolicyDomain(frame.data.get(), config_.page_size, hints.page_class,
                                     effective_workload)),
            std::memory_order_relaxed);
        frame.workload_class.store(static_cast<uint8_t>(effective_workload),
                                   std::memory_order_relaxed);
        frame.oldest_interesting_txid.store(hints.oldest_interesting_txid,
                                            std::memory_order_relaxed);
        frame.prune_safe_horizon_hint.store(hints.prune_safe_horizon_hint,
                                            std::memory_order_relaxed);
        frame.dead_version_bytes.store(hints.dead_version_bytes, std::memory_order_relaxed);
        frame.chain_depth_hint.store(hints.chain_depth_hint, std::memory_order_relaxed);
        uint64_t gc_touch = hints.last_gc_touch_generation;
        if (gc_touch == 0 && (hints.dead_version_bytes > 0 || hints.chain_depth_hint > 0 ||
                              hints.page_class == MgaPageClass::GC_CANDIDATE ||
                              hints.page_class == MgaPageClass::CHAIN_HEAVY))
        {
            gc_touch = mga_gc_touch_generation_.fetch_add(1, std::memory_order_relaxed) + 1;
        }
        frame.last_gc_touch_generation.store(gc_touch, std::memory_order_relaxed);
        frame.scan_probation_generation.store(hints.scan_probation_generation,
                                              std::memory_order_relaxed);
        frame.commit_fence_member.store(hints.commit_fence_member, std::memory_order_relaxed);
        ResidencyTier residency_tier =
            classifyResidencyTier(effective_workload, hints.page_class);
        if (hints.page_class == MgaPageClass::TX_STATE)
        {
            residency_tier = isHardProtectedFrame(frame)
                                 ? ResidencyTier::PinBiased
                                 : ResidencyTier::Probationary;
        }
        frame.residency_tier.store(static_cast<uint8_t>(residency_tier),
                                   std::memory_order_relaxed);
        if (frame.is_dirty.load(std::memory_order_relaxed))
        {
            frame.writeback_queue_state.store(
                static_cast<uint8_t>(classifyWritebackQueueState(frame)),
                std::memory_order_relaxed);
        }
        if (hints.page_class == MgaPageClass::GC_CANDIDATE)
        {
            offerGcCandidate(it->second);
        }
        return Status::OK;
    }

    auto BufferPool::getMgaFrameSnapshotGlobal(GPID gpid,
                                               MgaFrameSnapshot *snapshot_out,
                                               ErrorContext *ctx) const -> Status
    {
        if (snapshot_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "snapshot_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        *snapshot_out = MgaFrameSnapshot{};
        snapshot_out->gpid = gpid;

        size_t partition_idx = getPartitionIndex(gpid);
        auto &partition = page_table_partitions_[partition_idx];
        std::lock_guard<std::mutex> partition_lock(partition.mutex);
        auto it = partition.table.find(gpid);
        if (it == partition.table.end())
        {
            return Status::OK;
        }

        const Frame &frame = frames_[it->second];
        snapshot_out->resident = true;
        snapshot_out->owner_partition =
            frame.owner_partition.load(std::memory_order_relaxed);
        snapshot_out->home_partition =
            frame.home_partition.load(std::memory_order_relaxed);
        snapshot_out->pin_count = frame.pin_count.load(std::memory_order_relaxed);
        snapshot_out->is_dirty = frame.is_dirty.load(std::memory_order_relaxed);
        snapshot_out->object_id = frame.object_id.load(std::memory_order_relaxed);
        snapshot_out->state_generation =
            frame.state_generation.load(std::memory_order_relaxed);
        snapshot_out->io_generation =
            frame.io_generation.load(std::memory_order_relaxed);
        snapshot_out->dirty_generation =
            frame.dirty_generation.load(std::memory_order_relaxed);
        snapshot_out->page_class = static_cast<MgaPageClass>(
            frame.mga_page_class.load(std::memory_order_relaxed));
        snapshot_out->policy_domain = static_cast<PolicyDomain>(
            frame.policy_domain.load(std::memory_order_relaxed));
        snapshot_out->workload_class = static_cast<WorkloadClass>(
            frame.workload_class.load(std::memory_order_relaxed));
        snapshot_out->residency_tier = static_cast<ResidencyTier>(
            frame.residency_tier.load(std::memory_order_relaxed));
        snapshot_out->lifecycle_state = static_cast<LifecycleState>(
            frame.lifecycle_state.load(std::memory_order_relaxed));
        snapshot_out->dirty_state = static_cast<DirtyState>(
            frame.dirty_state.load(std::memory_order_relaxed));
        snapshot_out->writeback_queue_state = static_cast<WritebackQueueState>(
            frame.writeback_queue_state.load(std::memory_order_relaxed));
        snapshot_out->last_flush_generation =
            frame.last_flush_generation.load(std::memory_order_relaxed);
        snapshot_out->checkpoint_target_generation =
            frame.checkpoint_target_generation.load(std::memory_order_relaxed);
        snapshot_out->admission_generation =
            frame.admission_generation.load(std::memory_order_relaxed);
        snapshot_out->last_touch_generation =
            frame.last_touch_generation.load(std::memory_order_relaxed);
        snapshot_out->temperature_generation =
            frame.temperature_generation.load(std::memory_order_relaxed);
        snapshot_out->oldest_interesting_txid =
            frame.oldest_interesting_txid.load(std::memory_order_relaxed);
        snapshot_out->prune_safe_horizon_hint =
            frame.prune_safe_horizon_hint.load(std::memory_order_relaxed);
        snapshot_out->dead_version_bytes =
            frame.dead_version_bytes.load(std::memory_order_relaxed);
        snapshot_out->chain_depth_hint =
            frame.chain_depth_hint.load(std::memory_order_relaxed);
        snapshot_out->prefetch_session_key =
            frame.prefetch_session_key.load(std::memory_order_relaxed);
        snapshot_out->last_gc_touch_generation =
            frame.last_gc_touch_generation.load(std::memory_order_relaxed);
        snapshot_out->scan_probation_generation =
            frame.scan_probation_generation.load(std::memory_order_relaxed);
        snapshot_out->commit_fence_member =
            frame.commit_fence_member.load(std::memory_order_relaxed);
        snapshot_out->speculative_prefetch =
            frame.speculative_prefetch.load(std::memory_order_relaxed);
        snapshot_out->prefetch_consumed =
            frame.prefetch_consumed.load(std::memory_order_relaxed);
        return Status::OK;
    }

    void BufferPool::restoreCheckpointQueueState(const std::vector<GPID> &checkpoint_marker_gpids,
                                                 uint64_t dirty_generation_floor)
    {
        uint64_t seeded_floor = dirty_generation_floor;
        if (seeded_floor == 0 && !checkpoint_marker_gpids.empty())
        {
            seeded_floor = 1;
        }

        uint64_t observed_generation = dirty_generation_clock_.load(std::memory_order_relaxed);
        while (observed_generation < seeded_floor &&
               !dirty_generation_clock_.compare_exchange_weak(
                   observed_generation,
                   seeded_floor,
                   std::memory_order_relaxed,
                   std::memory_order_relaxed))
        {
        }

        if (checkpoint_marker_gpids.empty())
        {
            return;
        }

        std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
        checkpoint_marker_candidates_.insert(checkpoint_marker_gpids.begin(),
                                            checkpoint_marker_gpids.end());
    }

    auto BufferPool::checkpointDebtCandidateCount() -> uint64_t
    {
        std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
        std::unordered_set<GPID> unioned_candidates;
        unioned_candidates.reserve(dirty_checkpoint_candidates_.size() +
                                   checkpoint_marker_candidates_.size());
        for (const auto &entry : dirty_checkpoint_candidates_)
        {
            unioned_candidates.insert(entry.first);
        }
        for (const GPID gpid : checkpoint_marker_candidates_)
        {
            unioned_candidates.insert(gpid);
        }
        return static_cast<uint64_t>(unioned_candidates.size());
    }

    void BufferPool::restoreCheckpointDebtForFrame(uint32_t frame_index)
    {
        if (frame_index >= frames_.size() || frames_[frame_index].gpid == INVALID_GPID ||
            frames_[frame_index].data == nullptr)
        {
            return;
        }

        const auto *header = reinterpret_cast<const PageHeader *>(frames_[frame_index].data.get());
        if (header->magic != K_MAGIC_SBRD || pageIsTemporaryWork(*header) ||
            header->flush_generation <= header->checkpoint_generation)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
            checkpoint_marker_candidates_.insert(frames_[frame_index].gpid);
        }

        const uint64_t generation_floor = std::max<uint64_t>(
            1,
            dirty_generation_clock_.load(std::memory_order_relaxed));
        frames_[frame_index].last_flush_generation.store(header->flush_generation,
                                                         std::memory_order_relaxed);
        frames_[frame_index].checkpoint_target_generation.store(generation_floor,
                                                                std::memory_order_relaxed);
        frames_[frame_index].writeback_queue_state.store(
            static_cast<uint8_t>(WritebackQueueState::CHECKPOINT),
            std::memory_order_relaxed);
    }

    void BufferPool::beginCommitFence()
    {
        const uint32_t prior_depth = commit_fence_depth_.fetch_add(1, std::memory_order_acq_rel);
        if (prior_depth != 0)
        {
            return;
        }

        uint64_t backlog = 0;
        std::lock_guard<std::mutex> lock(mutex_);
        for (Frame &frame : frames_)
        {
            if (frame.gpid == INVALID_GPID)
            {
                continue;
            }

            const MgaPageClass page_class = static_cast<MgaPageClass>(
                frame.mga_page_class.load(std::memory_order_relaxed));
            const bool member = frame.is_dirty.load(std::memory_order_relaxed) ||
                                page_class == MgaPageClass::TX_STATE;
            frame.commit_fence_member.store(member, std::memory_order_relaxed);
            if (member)
            {
                ++backlog;
                if (frame.is_dirty.load(std::memory_order_relaxed))
                {
                    frame.writeback_queue_state.store(
                        static_cast<uint8_t>(classifyWritebackQueueState(frame)),
                        std::memory_order_relaxed);
                }
            }
        }
        commit_fence_backlog_.store(backlog, std::memory_order_release);
    }

    void BufferPool::endCommitFence()
    {
        const uint32_t prior_depth = commit_fence_depth_.load(std::memory_order_acquire);
        if (prior_depth == 0)
        {
            return;
        }

        if (commit_fence_depth_.fetch_sub(1, std::memory_order_acq_rel) != 1)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        for (Frame &frame : frames_)
        {
            frame.commit_fence_member.store(false, std::memory_order_relaxed);
            if (frame.is_dirty.load(std::memory_order_relaxed))
            {
                frame.writeback_queue_state.store(
                    static_cast<uint8_t>(classifyWritebackQueueState(frame)),
                    std::memory_order_relaxed);
            }
        }
        commit_fence_backlog_.store(0, std::memory_order_release);
    }

    void BufferPool::completeFsyncFence()
    {
        for (uint32_t frame_index = 0; frame_index < config_.pool_size; ++frame_index)
        {
            const DirtyState dirty_state = static_cast<DirtyState>(
                frames_[frame_index].dirty_state.load(std::memory_order_acquire));
            if (dirty_state != DirtyState::DirtyFlushedPendingFsync)
            {
                continue;
            }

            const uint64_t dirty_generation =
                frames_[frame_index].dirty_generation.load(std::memory_order_acquire);
            const uint64_t flushed_generation =
                frames_[frame_index].last_flush_generation.load(std::memory_order_relaxed);
            if (dirty_generation == 0 || dirty_generation != flushed_generation)
            {
                continue;
            }

            if (!tryClearFrameDirty(frame_index))
            {
                continue;
            }

            frames_[frame_index].writeback_queue_state.store(
                static_cast<uint8_t>(WritebackQueueState::NONE),
                std::memory_order_relaxed);
            frames_[frame_index].checkpoint_target_generation.store(0,
                                                                    std::memory_order_relaxed);

            const GPID gpid = frames_[frame_index].gpid;
            if (gpid == INVALID_GPID)
            {
                continue;
            }

            std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
            auto it = dirty_checkpoint_candidates_.find(gpid);
            if (it != dirty_checkpoint_candidates_.end() && it->second <= flushed_generation)
            {
                dirty_checkpoint_candidates_.erase(it);
            }
        }
    }

    // PHASE 1, TASK 1.2.3: LEGACY API - Convert page_id to GPID and call pinPageGlobal
    auto BufferPool::pinPage(uint32_t page_id, void **buffer, ErrorContext *ctx,
                             AccessStrategy strategy,
                             WorkloadClass workload_class) -> Status
    {
        GPID gpid = convertPageIDtoGPID(page_id);
        return pinPageGlobal(gpid, buffer, ctx, strategy, workload_class);
    }

    // PHASE 1, TASK 1.2.3: NEW GPID-based implementation
    // P2-1: Updated to use partitioned page table locks
    auto BufferPool::pinPageGlobal(GPID gpid, void **buffer, ErrorContext *ctx,
                                   AccessStrategy strategy,
                                   WorkloadClass workload_class) -> Status
    {
        if (buffer == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null buffer pointer");
            return Status::INVALID_ARGUMENT;
        }

        AccessStrategy effective_strategy = strategy;
        if (effective_strategy == AccessStrategy::Normal)
        {
            if (auto* conn_ctx = ConnectionContext::getCurrent())
            {
                if (conn_ctx->isBulkWriteMode())
                {
                    effective_strategy = AccessStrategy::BulkWrite;
                }
            }
        }
        WorkloadClass effective_workload = workload_class;
        if (effective_workload == WorkloadClass::Unspecified)
        {
            effective_workload = defaultWorkloadClass(effective_strategy);
        }

        if (frames_.size() != config_.pool_size)
        {
            std::lock_guard<std::mutex> init_lock(mutex_);
            if (frames_.size() != config_.pool_size)
            {
                frames_.clear();
                frames_.resize(config_.pool_size);
                lru_list_.clear();
                clearOwnershipPartitionsLocked();
                for (uint32_t i = 0; i < config_.pool_size; i++)
                {
                    try
                    {
                        frames_[i].data = std::make_unique<uint8_t[]>(config_.page_size);
                    }
                    catch (const std::bad_alloc &)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::OOM,
                                          "Failed to allocate buffer pool memory");
                        return Status::OOM;
                    }
                    frames_[i].gpid = INVALID_GPID;
                    frames_[i].pin_count.store(0, std::memory_order_relaxed);
                    frames_[i].is_dirty.store(false, std::memory_order_relaxed);
                    frames_[i].usage_count.store(0, std::memory_order_relaxed);
                    frames_[i].owner_partition.store(
                        static_cast<uint16_t>(getOwnershipHomePartition(i)),
                        std::memory_order_relaxed);
                    frames_[i].home_partition.store(
                        static_cast<uint16_t>(getOwnershipHomePartition(i)),
                        std::memory_order_relaxed);
                    resetFrameScaffolding(frames_[i]);
                    lru_list_.push_back(i);
                }

                initializeOwnershipPartitionsLocked();
                initializeRingBuffers();
            }
        }

        // P2-1: Get partition for this GPID
        size_t partition_idx = getPartitionIndex(gpid);
        auto& partition = page_table_partitions_[partition_idx];

        auto recover_resident_frame_mapping_locked = [&](uint32_t &recovered_frame_index) -> bool
        {
            for (uint32_t candidate = 0; candidate < frames_.size(); ++candidate)
            {
                const Frame &candidate_frame = frames_[candidate];
                if (candidate_frame.gpid != gpid)
                {
                    continue;
                }

                const auto lifecycle = static_cast<LifecycleState>(
                    candidate_frame.lifecycle_state.load(std::memory_order_relaxed));
                if (lifecycle == LifecycleState::Free ||
                    lifecycle == LifecycleState::Evicting)
                {
                    continue;
                }

                recovered_frame_index = candidate;
                partition.table[gpid] = recovered_frame_index;
                return true;
            }

            return false;
        };

        // First, try to find page with just the partition lock (fast path for cache hit)
        {
            std::lock_guard<std::mutex> partition_lock(partition.mutex);
            if (partition.table.bucket_count() == 0)
            {
                partition.table.rehash(1);
            }

            auto it = partition.table.find(gpid);
            if (it == partition.table.end())
            {
                uint32_t recovered_frame_index = UINT32_MAX;
                if (recover_resident_frame_mapping_locked(recovered_frame_index))
                {
                    it = partition.table.find(gpid);
                }
            }
            if (it != partition.table.end())
            {
                // Cache hit
                uint32_t frame_index = it->second;
                if (frame_index >= frames_.size() || frames_[frame_index].gpid != gpid)
                {
                    partition.table.erase(it);
                    uint32_t recovered_frame_index = UINT32_MAX;
                    if (recover_resident_frame_mapping_locked(recovered_frame_index))
                    {
                        frame_index = recovered_frame_index;
                    }
                    else
                    {
                        goto pin_fast_path_miss;
                    }
                }

                // CRITICAL FIX (Issue 1.13): Check for pin count overflow BEFORE incrementing
                if (frames_[frame_index].pin_count.load(std::memory_order_relaxed) ==
                    UINT32_MAX)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Pin count overflow - page pinned too many times");
                    return Status::INVALID_ARGUMENT;
                }

                // CRITICAL FIX (CRITICAL-1): Use atomic fetch_add for thread-safe increment
                frames_[frame_index].pin_count.fetch_add(1, std::memory_order_relaxed);

                // Clock Sweep: Increment usage count (capped at MAX_USAGE_COUNT)
                uint32_t current_usage =
                    frames_[frame_index].usage_count.load(std::memory_order_relaxed);
                if (current_usage < Frame::MAX_USAGE_COUNT)
                {
                    frames_[frame_index].usage_count.fetch_add(1,
                                                               std::memory_order_relaxed);
                }
                applyAutomaticMgaClassification(frame_index, effective_workload);
                noteFrameAccess(frame_index,
                                static_cast<WorkloadClass>(
                                    frames_[frame_index].workload_class.load(
                                        std::memory_order_relaxed)));
                if (static_cast<MgaPageClass>(
                        frames_[frame_index].mga_page_class.load(
                            std::memory_order_relaxed)) ==
                    MgaPageClass::CHAIN_HEAVY)
                {
                    stats_.mga_chain_heavy_hits.fetch_add(1,
                                                          std::memory_order_relaxed);
                }

                *buffer = frames_[frame_index].data.get();

                // MEDIUM-1 FIX: Use relaxed atomic increment for stats
                stats_.hits.fetch_add(1, std::memory_order_relaxed);
                logStatsEvent("HIT", gpid);
                if (metrics_ && metrics_->buffer_pool_hits_total)
                {
                    metrics_->buffer_pool_hits_total->inc();
                }
                VNextMetricsEventModel::recordStorageEvent(
                    "buffer_pool_pin", "hit", "NONE");
                if (auto* conn_ctx = ConnectionContext::getCurrent())
                {
                    conn_ctx->recordPageFetch();
                }
                return Status::OK;
            }
        }
pin_fast_path_miss:
        // Partition lock released here

        // MEDIUM-1 FIX: Use relaxed atomic increment for stats
        stats_.misses.fetch_add(1, std::memory_order_relaxed);
        logStatsEvent("MISS", gpid);
        if (metrics_ && metrics_->buffer_pool_misses_total)
        {
            metrics_->buffer_pool_misses_total->inc();
        }
        VNextMetricsEventModel::recordStorageEvent(
            "buffer_pool_pin", "miss", "NONE");

        if (config_.layout == PoolLayout::Segmented)
        {
            auto &owner_partition = ownership_partitions_[partition_idx];
            std::unique_lock<std::mutex> owner_lock(owner_partition.mutex);
            RingBuffer *ring = getRingBuffer(effective_strategy);
            uint32_t ring_slot = 0;

            auto tryClaimSegmentedRingFrame =
                [&](uint32_t &claimed_frame) -> Status
            {
                if (ring == nullptr || ring->frames.empty())
                {
                    return Status::INVALID_ARGUMENT;
                }

                const uint32_t ring_frame = ring->frames[ring_slot];
                if (ring_frame == UINT32_MAX)
                {
                    return Status::INVALID_ARGUMENT;
                }
                if (ring_frame >= frames_.size())
                {
                    ring->frames[ring_slot] = UINT32_MAX;
                    return Status::INVALID_ARGUMENT;
                }

                size_t donor_partition = static_cast<size_t>(
                    frames_[ring_frame].owner_partition.load(std::memory_order_relaxed));
                if (donor_partition >= NUM_PAGE_TABLE_PARTITIONS)
                {
                    ring->frames[ring_slot] = UINT32_MAX;
                    return Status::INVALID_ARGUMENT;
                }

                std::unique_lock<std::mutex> donor_lock;
                if (donor_partition != partition_idx)
                {
                    donor_lock = std::unique_lock<std::mutex>(
                        ownership_partitions_[donor_partition].mutex,
                        std::try_to_lock);
                    if (!donor_lock.owns_lock())
                    {
                        return Status::INVALID_ARGUMENT;
                    }
                }

                const Status ring_status = evictSpecificFrame(ring_frame, ctx);
                if (ring_status != Status::OK)
                {
                    if (ring_status == Status::INVALID_ARGUMENT &&
                        frames_[ring_frame].gpid == INVALID_GPID)
                    {
                        ring->frames[ring_slot] = UINT32_MAX;
                    }
                    return ring_status;
                }

                transferFrameOwnershipLocked(ring_frame,
                                             donor_partition,
                                             partition_idx);
                resetFrameIdentity(frames_[ring_frame], partition_idx, true);
                stageFrameForOwnership(frames_[ring_frame],
                                       partition_idx,
                                       LifecycleState::Loading);
                claimed_frame = ring_frame;
                return Status::OK;
            };

            // Re-check after joining the owner partition. This prevents duplicate
            // loads for the same GPID from competing across one global miss lock.
            {
                std::lock_guard<std::mutex> partition_lock(partition.mutex);
                auto it = partition.table.find(gpid);
                if (it == partition.table.end())
                {
                    uint32_t recovered_frame_index = UINT32_MAX;
                    if (recover_resident_frame_mapping_locked(recovered_frame_index))
                    {
                        it = partition.table.find(gpid);
                    }
                }
                if (it != partition.table.end())
                {
                    uint32_t frame_index = it->second;
                    if (frame_index >= frames_.size() || frames_[frame_index].gpid != gpid)
                    {
                        partition.table.erase(it);
                        uint32_t recovered_frame_index = UINT32_MAX;
                        if (recover_resident_frame_mapping_locked(recovered_frame_index))
                        {
                            frame_index = recovered_frame_index;
                        }
                        else
                        {
                            goto segmented_owner_recheck_miss;
                        }
                    }

                    if (frames_[frame_index].pin_count.load(std::memory_order_relaxed) ==
                        UINT32_MAX)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Pin count overflow - page pinned too many times");
                        return Status::INVALID_ARGUMENT;
                    }
                    frames_[frame_index].pin_count.fetch_add(1, std::memory_order_relaxed);

                    const uint32_t current_usage =
                        frames_[frame_index].usage_count.load(std::memory_order_relaxed);
                    if (current_usage < Frame::MAX_USAGE_COUNT)
                    {
                        frames_[frame_index].usage_count.fetch_add(
                            1,
                            std::memory_order_relaxed);
                    }
                    applyAutomaticMgaClassification(frame_index, effective_workload);
                    noteFrameAccess(frame_index,
                                    static_cast<WorkloadClass>(
                                        frames_[frame_index].workload_class.load(
                                            std::memory_order_relaxed)));
                    if (static_cast<MgaPageClass>(
                            frames_[frame_index].mga_page_class.load(
                                std::memory_order_relaxed)) ==
                        MgaPageClass::CHAIN_HEAVY)
                    {
                        stats_.mga_chain_heavy_hits.fetch_add(
                            1,
                            std::memory_order_relaxed);
                    }

                    *buffer = frames_[frame_index].data.get();
                    stats_.hits.fetch_add(1, std::memory_order_relaxed);
                    logStatsEvent("HIT", gpid);
                    if (metrics_ && metrics_->buffer_pool_hits_total)
                    {
                        metrics_->buffer_pool_hits_total->inc();
                    }
                    VNextMetricsEventModel::recordStorageEvent(
                        "buffer_pool_pin", "hit", "NONE");
                    if (auto *conn_ctx = ConnectionContext::getCurrent())
                    {
                        conn_ctx->recordPageFetch();
                    }
                    return Status::OK;
                }
            }
segmented_owner_recheck_miss:

            uint32_t frame_index = UINT32_MAX;
            Status status = Status::INVALID_ARGUMENT;
            if (ring != nullptr && !ring->frames.empty())
            {
                ring_slot = nextRingSlot(*ring);
                status = tryClaimSegmentedRingFrame(frame_index);
                if (status != Status::OK && status != Status::INVALID_ARGUMENT)
                {
                    return status;
                }
            }

            if (status != Status::OK)
            {
                status = claimFrameForSegmentedMiss(partition_idx, frame_index, ctx);
            }
            if (status != Status::OK)
            {
                return status;
            }

            purgeFramePageTableMappings(frame_index);
            frames_[frame_index].io_generation.fetch_add(1, std::memory_order_relaxed);
            status = readPageFromDisk(gpid, frames_[frame_index].data.get(), ctx);
            if (status != Status::OK)
            {
                resetFrameIdentity(frames_[frame_index], partition_idx, true);
                releaseFrameToOwnershipFreeListLocked(frame_index, partition_idx);
                return status;
            }
            if (auto *conn_ctx = ConnectionContext::getCurrent())
            {
                conn_ctx->recordPageRead();
            }

            frames_[frame_index].gpid = gpid;
            frames_[frame_index].pin_count.store(1, std::memory_order_relaxed);
            frames_[frame_index].is_dirty.store(false, std::memory_order_relaxed);
            frames_[frame_index].usage_count.store(1, std::memory_order_relaxed);
            resetFrameScaffolding(frames_[frame_index]);
            frames_[frame_index].owner_partition.store(
                static_cast<uint16_t>(partition_idx),
                std::memory_order_relaxed);
            applyAutomaticMgaClassification(frame_index, effective_workload);
            const bool ghost_hit = consumeGhostHistory(gpid);
            noteFrameAccess(frame_index,
                            static_cast<WorkloadClass>(
                                frames_[frame_index].workload_class.load(
                                    std::memory_order_relaxed)),
                            ghost_hit);
            restoreCheckpointDebtForFrame(frame_index);

            {
                std::lock_guard<std::mutex> partition_lock(partition.mutex);
                auto it = partition.table.find(gpid);
                if (it != partition.table.end())
                {
                    uint32_t existing_frame = it->second;
                    if (existing_frame >= frames_.size() ||
                        frames_[existing_frame].gpid != gpid)
                    {
                        partition.table.erase(it);
                    }
                    else
                    {
                        frames_[existing_frame].pin_count.fetch_add(
                            1,
                            std::memory_order_relaxed);
                        resetFrameIdentity(frames_[frame_index], partition_idx, true);
                        releaseFrameToOwnershipFreeListLocked(frame_index, partition_idx);
                        *buffer = frames_[existing_frame].data.get();
                        stats_.hits.fetch_add(1, std::memory_order_relaxed);
                        logStatsEvent("HIT", gpid);
                        if (metrics_ && metrics_->buffer_pool_hits_total)
                        {
                            metrics_->buffer_pool_hits_total->inc();
                        }
                        VNextMetricsEventModel::recordStorageEvent(
                            "buffer_pool_pin", "hit", "NONE");
                        if (auto *conn_ctx = ConnectionContext::getCurrent())
                        {
                            conn_ctx->recordPageFetch();
                        }
                        return Status::OK;
                    }
                }
                partition.table[gpid] = frame_index;
            }

            if (ring != nullptr && !ring->frames.empty())
            {
                ring->frames[ring_slot] = frame_index;
            }

            *buffer = frames_[frame_index].data.get();
            return Status::OK;
        }

        // Legacy layouts still route misses through the shared allocator and
        // eviction path. The segmented memory-model program overrides that path
        // above with partition-local ownership.
        std::lock_guard<std::mutex> global_lock(mutex_);

        // Re-check partition in case another thread loaded the page while we waited
        {
            std::lock_guard<std::mutex> partition_lock(partition.mutex);
            auto it = partition.table.find(gpid);
            if (it != partition.table.end())
            {
                // Another thread loaded it - handle as cache hit
                uint32_t frame_index = it->second;
                if (frame_index >= frames_.size() || frames_[frame_index].gpid != gpid)
                {
                    partition.table.erase(it);
                }
                else
                {
                    frames_[frame_index].pin_count.fetch_add(1, std::memory_order_relaxed);

                    uint32_t current_usage =
                        frames_[frame_index].usage_count.load(std::memory_order_relaxed);
                    if (current_usage < Frame::MAX_USAGE_COUNT)
                    {
                        frames_[frame_index].usage_count.fetch_add(
                            1,
                            std::memory_order_relaxed);
                    }
                    applyAutomaticMgaClassification(frame_index, effective_workload);
                    noteFrameAccess(frame_index,
                                    static_cast<WorkloadClass>(
                                        frames_[frame_index].workload_class.load(
                                            std::memory_order_relaxed)));
                    if (static_cast<MgaPageClass>(
                            frames_[frame_index].mga_page_class.load(
                                std::memory_order_relaxed)) ==
                        MgaPageClass::CHAIN_HEAVY)
                    {
                        stats_.mga_chain_heavy_hits.fetch_add(
                            1,
                            std::memory_order_relaxed);
                    }

                    *buffer = frames_[frame_index].data.get();
                    if (effective_strategy == AccessStrategy::Normal)
                    {
                        updateLru(frame_index);
                    }
                    stats_.hits.fetch_add(1, std::memory_order_relaxed);
                    logStatsEvent("HIT", gpid);
                    VNextMetricsEventModel::recordStorageEvent(
                        "buffer_pool_pin", "hit", "NONE");
                    if (auto* conn_ctx = ConnectionContext::getCurrent())
                    {
                        conn_ctx->recordPageFetch();
                    }
                    return Status::OK;
                }
            }
        }

        // Find a frame to use
        uint32_t frame_index;

        // First try to find an unpinned frame
        bool found_free = false;
        for (uint32_t i = 0; i < config_.pool_size; i++)
        {
            if (frames_[i].gpid == INVALID_GPID)
            {
                frame_index = i;
                found_free = true;
                break;
            }
        }

        RingBuffer *ring = getRingBuffer(effective_strategy);
        uint32_t ring_slot = 0;

        if (!found_free)
        {
            bool used_ring_frame = false;
            if (ring && !ring->frames.empty())
            {
                ring_slot = nextRingSlot(*ring);
                uint32_t ring_frame = ring->frames[ring_slot];

                if (ring_frame != UINT32_MAX)
                {
                    Status ring_status = evictSpecificFrame(ring_frame, ctx);
                    if (ring_status == Status::OK)
                    {
                        frame_index = ring_frame;
                        used_ring_frame = true;
                    }
                    else if (ring_status != Status::INVALID_ARGUMENT)
                    {
                        return ring_status;
                    }
                }
            }

            if (!used_ring_frame)
            {
                // Need to evict a page
                Status status = evictPage(frame_index, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }
        }
        else if (ring && !ring->frames.empty())
        {
            ring_slot = nextRingSlot(*ring);
        }

        frames_[frame_index].lifecycle_state.store(static_cast<uint8_t>(LifecycleState::Loading),
                                                   std::memory_order_relaxed);
        purgeFramePageTableMappings(frame_index);
        frames_[frame_index].io_generation.fetch_add(1, std::memory_order_relaxed);

        // Read page from disk
        Status status = readPageFromDisk(gpid, frames_[frame_index].data.get(), ctx);
        if (status != Status::OK)
        {
            frames_[frame_index].lifecycle_state.store(static_cast<uint8_t>(LifecycleState::Error),
                                                       std::memory_order_relaxed);
            return status;
        }
        if (auto* conn_ctx = ConnectionContext::getCurrent())
        {
            conn_ctx->recordPageRead();
        }

        // Initialize frame metadata before publishing mapping to avoid lost pin_count on cache hits.
        frames_[frame_index].gpid = gpid;
        frames_[frame_index].pin_count.store(1, std::memory_order_relaxed);
        frames_[frame_index].is_dirty.store(false, std::memory_order_relaxed);

        // First touch now enters the auditable admission path with minimal protection.
        // `noteFrameAccess()` upgrades the frame to `Protected` or `PinBiased` only for
        // direct-protect classes, ghost hits, or qualifying second touches.
        frames_[frame_index].usage_count.store(1, std::memory_order_relaxed);
        resetFrameScaffolding(frames_[frame_index]);
        applyAutomaticMgaClassification(frame_index, effective_workload);
        const bool ghost_hit = consumeGhostHistory(gpid);
        noteFrameAccess(frame_index,
                        static_cast<WorkloadClass>(
                            frames_[frame_index].workload_class.load(
                                std::memory_order_relaxed)),
                        ghost_hit);
        restoreCheckpointDebtForFrame(frame_index);

        // P2-1: Insert into partitioned page table
        {
            std::lock_guard<std::mutex> partition_lock(partition.mutex);
            partition.table[gpid] = frame_index;
        }

        // Update LRU (still maintained for fallback)
        if (effective_strategy == AccessStrategy::Normal)
        {
            insertLruMidpoint(frame_index);
        }

        if (ring && !ring->frames.empty())
        {
            ring->frames[ring_slot] = frame_index;
        }

        *buffer = frames_[frame_index].data.get();
        return Status::OK;
    }

    // PHASE 1, TASK 1.2.3: LEGACY API - Convert page_id to GPID and call unpinPageGlobal
    auto BufferPool::unpinPage(uint32_t page_id, bool is_dirty, ErrorContext *ctx) -> Status
    {
        GPID gpid = convertPageIDtoGPID(page_id);
        return unpinPageGlobal(gpid, is_dirty, ctx);
    }

    // PHASE 1, TASK 1.2.3: NEW GPID-based implementation
    // P2-1: Updated to use partitioned page table locks
    auto BufferPool::unpinPageGlobal(GPID gpid, bool is_dirty, ErrorContext *ctx) -> Status
    {
        // P2-1: Only need partition lock for unpin (no global lock needed)
        size_t partition_idx = getPartitionIndex(gpid);
        auto& partition = page_table_partitions_[partition_idx];

        std::lock_guard<std::mutex> partition_lock(partition.mutex);

        auto recover_live_pinned_frame = [&](uint32_t &recovered_frame_index) -> bool
        {
            for (uint32_t candidate = 0; candidate < frames_.size(); ++candidate)
            {
                const Frame &candidate_frame = frames_[candidate];
                if (candidate_frame.gpid != gpid)
                {
                    continue;
                }
                if (candidate_frame.pin_count.load(std::memory_order_relaxed) == 0)
                {
                    continue;
                }

                recovered_frame_index = candidate;
                partition.table[gpid] = recovered_frame_index;
                return true;
            }

            return false;
        };

        // Find the page in buffer pool
        auto it = partition.table.find(gpid);
        if (it == partition.table.end())
        {
            uint32_t recovered_frame_index = UINT32_MAX;
            if (!recover_live_pinned_frame(recovered_frame_index))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page not in buffer pool");
                return Status::INVALID_ARGUMENT;
            }
            it = partition.table.find(gpid);
        }

        uint32_t frame_index = it->second;
        if (frame_index >= frames_.size() || frames_[frame_index].gpid != gpid)
        {
            partition.table.erase(it);
            frame_index = UINT32_MAX;
            if (!recover_live_pinned_frame(frame_index))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page not in buffer pool");
                return Status::INVALID_ARGUMENT;
            }
        }

        // Check pin count
        // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
        if (frames_[frame_index].pin_count.load(std::memory_order_relaxed) == 0)
        {
            // Under heavy concurrent miss/evict/reload pressure the page-table entry can
            // lag behind the caller's still-pinned frame for the same GPID. Recover by
            // locating the live pinned frame instead of failing the logical unpin.
            uint32_t alternate_frame = UINT32_MAX;
            for (uint32_t candidate = 0; candidate < frames_.size(); ++candidate)
            {
                if (candidate == frame_index)
                {
                    continue;
                }

                const Frame &candidate_frame = frames_[candidate];
                if (candidate_frame.gpid != gpid)
                {
                    continue;
                }
                if (candidate_frame.pin_count.load(std::memory_order_relaxed) == 0)
                {
                    continue;
                }

                alternate_frame = candidate;
                break;
            }

            if (alternate_frame == UINT32_MAX)
            {
                const Frame& mapped_frame = frames_[frame_index];
                std::ostringstream detail;
                detail << "Page is not pinned"
                       << " gpid=" << gpid
                       << " page=" << getPageNumber(gpid)
                       << " tablespace=" << getTablespaceID(gpid)
                       << " mapped_frame=" << frame_index
                       << " frame_gpid=" << mapped_frame.gpid
                       << " lifecycle="
                       << static_cast<unsigned>(
                              mapped_frame.lifecycle_state.load(std::memory_order_relaxed))
                       << " dirty="
                       << mapped_frame.is_dirty.load(std::memory_order_relaxed)
                       << " usage="
                       << mapped_frame.usage_count.load(std::memory_order_relaxed)
                       << " owner_partition="
                       << mapped_frame.owner_partition.load(std::memory_order_relaxed)
                       << " home_partition="
                       << mapped_frame.home_partition.load(std::memory_order_relaxed)
                       << " state_generation="
                       << mapped_frame.state_generation.load(std::memory_order_relaxed)
                       << " io_generation="
                       << mapped_frame.io_generation.load(std::memory_order_relaxed);
                SET_ERROR_CONTEXT(ctx,
                                  Status::INVALID_ARGUMENT,
                                  detail.str().c_str());
                return Status::INVALID_ARGUMENT;
            }

            frame_index = alternate_frame;
        }

        // Update dirty flag and publish the latest dirty generation so checkpoint
        // drains can distinguish pages dirtied before vs. after their boundary.
        if (is_dirty)
        {
            publishDirtyGeneration(frame_index);
            if (tryMarkFrameDirty(frame_index))
            {
                if (auto* conn_ctx = ConnectionContext::getCurrent())
                {
                    conn_ctx->recordPageMark();
                }
            }
            frames_[frame_index].lifecycle_state.store(
                static_cast<uint8_t>(LifecycleState::Valid),
                std::memory_order_relaxed);

            auto *header = reinterpret_cast<PageHeader *>(frames_[frame_index].data.get());
            const bool needs_bootstrap_publish =
                frames_[frame_index].pin_count.load(std::memory_order_relaxed) == 1 &&
                header != nullptr &&
                header->page_type != 0 &&
                !pageIsTemporaryWork(*header) &&
                (header->flags & PAGE_FLAG_CHECKSUM_VALID) == 0u;
            if (needs_bootstrap_publish)
            {
                std::vector<uint8_t> bootstrap_image(config_.page_size);
                std::memcpy(bootstrap_image.data(), header, config_.page_size);
                auto *bootstrap_header =
                    reinterpret_cast<PageHeader *>(bootstrap_image.data());
                bootstrap_header->generation = 0;
                bootstrap_header->flush_generation = 0;
                bootstrap_header->checkpoint_generation = 0;
                bootstrap_header->header_checksum = 0;
                bootstrap_header->payload_checksum = 0;
                bootstrap_header->flags &= ~PAGE_FLAG_CHECKSUM_VALID;

                WritebackAttribution bootstrap_attribution{};
                bootstrap_attribution.page_class = bootstrap_header->page_type;
                const Status bootstrap_status =
                    db_->write_free_page_image_global(gpid,
                                                      bootstrap_image.data(),
                                                      ctx,
                                                      bootstrap_attribution);
                if (bootstrap_status != Status::OK)
                {
                    return bootstrap_status;
                }

                refreshPageChecksumsWithoutGenerationAdvance(
                    reinterpret_cast<uint8_t *>(header),
                    config_.page_size,
                    getPageNumber(gpid));
            }
        }

        // Decrement pin count
        // CRITICAL FIX (CRITICAL-1): Use atomic fetch_sub for thread-safe decrement
        frames_[frame_index].pin_count.fetch_sub(1, std::memory_order_relaxed);

        return Status::OK;
    }

    // PHASE 1, TASK 1.2.3: LEGACY API - Convert page_id to GPID and call flushPageGlobal
    auto BufferPool::flushPage(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        GPID gpid = convertPageIDtoGPID(page_id);
        return flushPageGlobal(gpid, ctx);
    }

    // PHASE 1, TASK 1.2.3: NEW GPID-based implementation
    // P2-1: Updated to use partitioned page table locks
    auto BufferPool::flushPageGlobal(GPID gpid, ErrorContext *ctx) -> Status
    {
        // Lookup under partition lock, then release before frame content lock.
        size_t partition_idx = getPartitionIndex(gpid);
        auto& partition = page_table_partitions_[partition_idx];
        uint32_t frame_index = 0;

        {
            std::lock_guard<std::mutex> partition_lock(partition.mutex);
            auto it = partition.table.find(gpid);
            if (it == partition.table.end())
            {
                // Page not in buffer pool, nothing to flush
                return Status::OK;
            }
            frame_index = it->second;
            if (frame_index >= frames_.size() || frames_[frame_index].gpid != gpid)
            {
                partition.table.erase(it);
                return Status::OK;
            }
        }

        // Frame may have been reassigned after partition unlock; validate again before flushing.
        if (frame_index >= frames_.size() || frames_[frame_index].gpid != gpid)
        {
            return Status::OK;
        }

        // Check if dirty
        if (!frames_[frame_index].is_dirty.load(std::memory_order_acquire))
        {
            // Not dirty, nothing to flush
            return Status::OK;
        }
        if (static_cast<DirtyState>(
                frames_[frame_index].dirty_state.load(std::memory_order_relaxed)) ==
            DirtyState::DirtyFlushedPendingFsync)
        {
            return Status::OK;
        }

        // Avoid flushing a page that is currently pinned (may be actively modified)
        if (frames_[frame_index].pin_count.load(std::memory_order_relaxed) > 0)
        {
            return Status::OK;
        }

        std::unique_lock<std::mutex> content_lock(*frames_[frame_index].content_mutex,
                                                  std::try_to_lock);
        if (!content_lock.owns_lock())
        {
            return Status::OK;
        }

        // Re-validate after content lock acquisition.
        if (frames_[frame_index].gpid != gpid ||
            !frames_[frame_index].is_dirty.load(std::memory_order_acquire) ||
            frames_[frame_index].pin_count.load(std::memory_order_relaxed) > 0)
        {
            return Status::OK;
        }

        auto *header = reinterpret_cast<PageHeader *>(frames_[frame_index].data.get());
        if (header != nullptr && pageIsTemporaryWork(*header))
        {
            return Status::OK;
        }

        // Write to disk
        const uint64_t flushed_generation =
            frames_[frame_index].dirty_generation.load(std::memory_order_acquire);
        const WritebackQueueState queue_state = classifyWritebackQueueState(frames_[frame_index]);
        beginFrameWriteback(frame_index, queue_state);
        Status status = writePageToDisk(frame_index, ctx, queue_state);
        if (status == Status::OK)
        {
            finishFrameWriteback(frame_index,
                                 flushed_generation,
                                 queue_state);
            // MEDIUM-1 FIX: Use relaxed atomic increment for stats
            stats_.flushes.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            markFrameWritebackFailure(frame_index, queue_state);
        }

        return status;
    }

    auto BufferPool::flushAll(ErrorContext *ctx) -> Status
    {
        for (uint32_t i = 0; i < config_.pool_size; i++)
        {
            std::unique_lock<std::mutex> content_lock(*frames_[i].content_mutex,
                                                      std::try_to_lock);
            if (!content_lock.owns_lock())
            {
                continue;
            }

            // PHASE 1, TASK 1.2.3: Changed page_id to gpid
            if (frames_[i].gpid == INVALID_GPID ||
                !frames_[i].is_dirty.load(std::memory_order_acquire) ||
                static_cast<DirtyState>(frames_[i].dirty_state.load(std::memory_order_relaxed)) ==
                    DirtyState::DirtyFlushedPendingFsync)
            {
                continue;
            }
            if (frames_[i].pin_count.load(std::memory_order_relaxed) > 0)
            {
                continue;
            }
            if (isTemporaryWorkPageBuffer(frames_[i].data.get(), config_.page_size))
            {
                continue;
            }

            const uint64_t flushed_generation =
                frames_[i].dirty_generation.load(std::memory_order_acquire);
            const WritebackQueueState queue_state = classifyWritebackQueueState(frames_[i]);
            beginFrameWriteback(i, queue_state);
            Status status = writePageToDisk(i, ctx, queue_state);
            if (status != Status::OK)
            {
                markFrameWritebackFailure(i, queue_state);
                return status;
            }
            finishFrameWriteback(i,
                                 flushed_generation,
                                 queue_state);
            // MEDIUM-1 FIX: Use relaxed atomic increment for stats
            stats_.flushes.fetch_add(1, std::memory_order_relaxed);
        }

        return Status::OK;
    }

    // P2-3: TOAST Chunk Prefetching - LEGACY API
    auto BufferPool::prefetchPages(const std::vector<uint32_t> &page_ids, ErrorContext *ctx,
                                   AccessStrategy strategy,
                                   WorkloadClass workload_class) -> Status
    {
        // Convert to GPIDs and call GPID version
        std::vector<GPID> gpids;
        gpids.reserve(page_ids.size());
        for (uint32_t page_id : page_ids)
        {
            gpids.push_back(convertPageIDtoGPID(page_id));
        }
        return prefetchPagesGlobal(gpids, ctx, strategy, workload_class);
    }

    // P2-3: TOAST Chunk Prefetching - Batch read pages into buffer pool
    auto BufferPool::prefetchPagesGlobal(const std::vector<GPID> &gpids, ErrorContext *ctx,
                                         AccessStrategy strategy,
                                         WorkloadClass workload_class) -> Status
    {
        if (gpids.empty())
        {
            return Status::OK;
        }

        WorkloadClass effective_workload = workload_class;
        if (effective_workload == WorkloadClass::Unspecified)
        {
            effective_workload = defaultWorkloadClass(strategy, true);
        }

        if (!config_.prefetch_enabled)
        {
            publishThrashState(ThrashDetectorState::None);
            return Status::OK;
        }

        // Remove duplicates and pages already in cache
        std::vector<GPID> pages_to_fetch;
        pages_to_fetch.reserve(gpids.size());

        for (GPID gpid : gpids)
        {
            // Check if already in buffer pool using partition lock
            size_t partition_idx = getPartitionIndex(gpid);
            auto& partition = page_table_partitions_[partition_idx];

            bool in_cache = false;
            {
                std::lock_guard<std::mutex> partition_lock(partition.mutex);
                in_cache = (partition.table.find(gpid) != partition.table.end());
            }

            if (!in_cache)
            {
                // Check for duplicates in our fetch list
                bool duplicate = false;
                for (GPID existing : pages_to_fetch)
                {
                    if (existing == gpid)
                    {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate)
                {
                    pages_to_fetch.push_back(gpid);
                }
            }
        }

        if (pages_to_fetch.empty())
        {
            publishThrashState(ThrashDetectorState::None);
            return Status::OK;
        }

        const uint64_t session_key = deriveCurrentPrefetchSessionKey();
        const size_t original_prefetch_count = pages_to_fetch.size();
        const uint32_t global_outstanding = currentOutstandingPrefetchDebtPages();
        const uint32_t session_outstanding = currentOutstandingPrefetchDebtPages(session_key);
        const uint32_t global_remaining =
            (config_.prefetch_max_debt_pages > global_outstanding)
                ? (config_.prefetch_max_debt_pages - global_outstanding)
                : 0u;
        const uint32_t session_budget = sessionPrefetchBudget();
        const uint32_t session_remaining =
            (session_budget > session_outstanding) ? (session_budget - session_outstanding) : 0u;

        ThrashDetectorState thrash_state = evaluateThrashState(session_key);
        size_t allowed_prefetch_count = pages_to_fetch.size();
        if (global_remaining < allowed_prefetch_count)
        {
            allowed_prefetch_count = global_remaining;
            thrash_state = ThrashDetectorState::GlobalDebtCap;
        }
        if (session_remaining < allowed_prefetch_count)
        {
            allowed_prefetch_count = session_remaining;
            thrash_state = ThrashDetectorState::SessionBudgetCap;
            stats_.fairness_session_budget_breaches.fetch_add(1, std::memory_order_relaxed);
        }

        switch (thrash_state)
        {
            case ThrashDetectorState::UsefulnessCollapse:
                allowed_prefetch_count = 0;
                break;
            case ThrashDetectorState::ScanPressure:
                allowed_prefetch_count = std::min<size_t>(allowed_prefetch_count, 1u);
                break;
            case ThrashDetectorState::GlobalDebtCap:
            case ThrashDetectorState::SessionBudgetCap:
            case ThrashDetectorState::None:
                break;
        }

        publishThrashState(thrash_state);
        if (allowed_prefetch_count < pages_to_fetch.size())
        {
            stats_.prefetch_cancelled_pages.fetch_add(
                static_cast<uint64_t>(pages_to_fetch.size() - allowed_prefetch_count),
                std::memory_order_relaxed);
            pages_to_fetch.resize(allowed_prefetch_count);
        }

        // Prefetch pages by pinning then immediately unpinning
        // The pages stay in the buffer pool cache for subsequent access
        for (GPID gpid : pages_to_fetch)
        {
            void *buffer = nullptr;
            Status status = pinPageGlobal(gpid, &buffer, ctx, strategy, effective_workload);
            if (status == Status::OK)
            {
                // Immediately unpin - page stays in cache
                unpinPageGlobal(gpid, false, ctx);

                size_t partition_idx = getPartitionIndex(gpid);
                auto &partition = page_table_partitions_[partition_idx];
                std::lock_guard<std::mutex> partition_lock(partition.mutex);
                auto frame_it = partition.table.find(gpid);
                if (frame_it != partition.table.end())
                {
                    markFramePrefetched(frame_it->second, session_key);
                }
            }
            // Ignore errors - partial prefetch is still useful
        }

        if (pages_to_fetch.empty() && original_prefetch_count != 0 &&
            thrash_state == ThrashDetectorState::None)
        {
            publishThrashState(ThrashDetectorState::GlobalDebtCap);
        }

        return Status::OK;
    }

    // PHASE 6, TASK 6.2: Flush all dirty pages for a specific tablespace
    auto BufferPool::flushTablespace(uint16_t tablespace_id, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        uint32_t flushed_count = 0;

        // Iterate through all frames in buffer pool
        for (uint32_t i = 0; i < config_.pool_size; i++)
        {
            // Skip invalid or clean frames
            if (frames_[i].gpid == INVALID_GPID ||
                !frames_[i].is_dirty.load(std::memory_order_acquire))
            {
                continue;
            }

            // Extract tablespace_id from GPID
            uint16_t frame_tablespace_id = getTablespaceID(frames_[i].gpid);

            // Check if this frame belongs to the target tablespace
            if (frame_tablespace_id == tablespace_id)
            {
                std::unique_lock<std::mutex> content_lock(*frames_[i].content_mutex,
                                                          std::try_to_lock);
                if (!content_lock.owns_lock())
                {
                    continue;
                }
                if (frames_[i].pin_count.load(std::memory_order_relaxed) > 0)
                {
                    continue;
                }
                if (frames_[i].gpid == INVALID_GPID ||
                    !frames_[i].is_dirty.load(std::memory_order_acquire) ||
                    static_cast<DirtyState>(frames_[i].dirty_state.load(std::memory_order_relaxed)) ==
                        DirtyState::DirtyFlushedPendingFsync)
                {
                    continue;
                }
                auto *header = reinterpret_cast<PageHeader *>(frames_[i].data.get());
                if (header != nullptr && pageIsTemporaryWork(*header))
                {
                    continue;
                }

                // Flush this dirty page
                const uint64_t flushed_generation =
                    frames_[i].dirty_generation.load(std::memory_order_acquire);
                const WritebackQueueState queue_state = classifyWritebackQueueState(frames_[i]);
                beginFrameWriteback(i, queue_state);
                Status status = writePageToDisk(i, ctx, queue_state);
                if (status != Status::OK)
                {
                    markFrameWritebackFailure(i, queue_state);
                    LOG_ERROR(BUFFER,
                             "Failed to flush page in tablespace %u during flushTablespace()",
                             tablespace_id);
                    return status;
                }

                finishFrameWriteback(i,
                                     flushed_generation,
                                     queue_state);
                stats_.flushes.fetch_add(1, std::memory_order_relaxed);
                flushed_count++;
            }
        }

        LOG_DEBUG(BUFFER,
                 "Flushed %u dirty pages from tablespace %u",
                 flushed_count, tablespace_id);

        return Status::OK;
    }

    // P2-1: Updated to use partitioned page table locks
    auto BufferPool::lockPage(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        GPID gpid = convertPageIDtoGPID(page_id);
        return lockPageGlobal(gpid, ctx);
    }

    auto BufferPool::lockPageGlobal(GPID gpid, ErrorContext *ctx) -> Status
    {
        uint32_t frame_index;
        size_t partition_idx = getPartitionIndex(gpid);
        auto& partition = page_table_partitions_[partition_idx];

        // Find the frame index while holding partition mutex
        {
            std::lock_guard<std::mutex> partition_lock(partition.mutex);

            auto it = partition.table.find(gpid);
            if (it == partition.table.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                                  "Page not in buffer pool - must pin first");
                return Status::NOT_FOUND;
            }

            frame_index = it->second;

            // Page must be pinned before locking
            if (frames_[frame_index].pin_count.load(std::memory_order_relaxed) == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Cannot lock unpinned page");
                return Status::INVALID_ARGUMENT;
            }
        }

        // Acquire the content mutex for this page (outside partition mutex to avoid deadlock)
        frames_[frame_index].content_mutex->lock();

        return Status::OK;
    }

    // P2-1: Updated to use partitioned page table locks
    auto BufferPool::unlockPage(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        GPID gpid = convertPageIDtoGPID(page_id);
        return unlockPageGlobal(gpid, ctx);
    }

    auto BufferPool::unlockPageGlobal(GPID gpid, ErrorContext *ctx) -> Status
    {
        uint32_t frame_index;
        size_t partition_idx = getPartitionIndex(gpid);
        auto& partition = page_table_partitions_[partition_idx];

        // Find the frame index while holding partition mutex
        {
            std::lock_guard<std::mutex> partition_lock(partition.mutex);

            auto it = partition.table.find(gpid);
            if (it == partition.table.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Page not in buffer pool");
                return Status::NOT_FOUND;
            }

            frame_index = it->second;
        }

        // Release the content mutex for this page
        frames_[frame_index].content_mutex->unlock();

        return Status::OK;
    }

    void BufferPool::initializeRingBuffers()
    {
        uint32_t ring_size = std::max<uint32_t>(1, config_.pool_size / RING_DIVISOR);
        seq_ring_.reset(ring_size);
        vacuum_ring_.reset(ring_size);
        bulk_write_ring_.reset(ring_size);
    }

    auto BufferPool::getRingBuffer(AccessStrategy strategy) -> RingBuffer*
    {
        switch (strategy)
        {
            case AccessStrategy::Sequential:
                return &seq_ring_;
            case AccessStrategy::Vacuum:
                return &vacuum_ring_;
            case AccessStrategy::BulkWrite:
                return &bulk_write_ring_;
            case AccessStrategy::Normal:
            default:
                return nullptr;
        }
    }

    auto BufferPool::nextRingSlot(RingBuffer &ring) -> uint32_t
    {
        uint32_t slot = ring.next;
        ring.next = (ring.next + 1) % ring.frames.size();
        return slot;
    }

    auto BufferPool::evictSpecificFrame(uint32_t frame_index, ErrorContext *ctx) -> Status
    {
        if (frame_index >= config_.pool_size)
        {
            return Status::INVALID_ARGUMENT;
        }

        Frame &frame = frames_[frame_index];
        if (frame.pin_count.load(std::memory_order_relaxed) > 0)
        {
            return Status::INVALID_ARGUMENT;
        }

        if (frame.gpid == INVALID_GPID)
        {
            frame.is_dirty.store(false, std::memory_order_relaxed);
            frame.pin_count.store(0, std::memory_order_relaxed);
            frame.usage_count.store(0, std::memory_order_relaxed);
            resetFrameScaffolding(frame);
            return Status::OK;
        }

        std::unique_lock<std::mutex> content_lock(*frame.content_mutex, std::try_to_lock);
        if (!content_lock.owns_lock())
        {
            return Status::INVALID_ARGUMENT;
        }

        size_t partition_idx = getPartitionIndex(frame.gpid);
        auto &partition = page_table_partitions_[partition_idx];
        std::lock_guard<std::mutex> partition_lock(partition.mutex);

        auto page_table_it = partition.table.find(frame.gpid);
        if (page_table_it == partition.table.end() || page_table_it->second != frame_index)
        {
            return Status::INVALID_ARGUMENT;
        }

        uint32_t pin_count = frame.pin_count.load(std::memory_order_relaxed);
        if (pin_count != 0)
        {
            return Status::INVALID_ARGUMENT;
        }

        const ResidencyTier residency_tier = static_cast<ResidencyTier>(
            frame.residency_tier.load(std::memory_order_relaxed));
        if (residency_tier == ResidencyTier::Protected ||
            residency_tier == ResidencyTier::PinBiased)
        {
            return Status::INVALID_ARGUMENT;
        }

        const bool was_dirty = frame.is_dirty.load(std::memory_order_acquire);
        if (was_dirty)
        {
            auto *header = reinterpret_cast<PageHeader *>(frame.data.get());
            if (header != nullptr && pageIsTemporaryWork(*header) &&
                header->page_type == PAGE_TYPE_TEMP_HEAP)
            {
                // Scratch temp-heap/workfile pages are intentionally lossy across
                // eviction. Session temp tables also carry the temporary-work
                // flag, but they must remain reloadable within the session, so
                // ordinary heap pages still go through writePageToDisk().
                discardTemporaryWorkFrameDirtyState(frame_index);
            }
            else
            {
                const uint64_t flushed_generation =
                    frame.dirty_generation.load(std::memory_order_acquire);
                beginFrameWriteback(frame_index, WritebackQueueState::NONE);
                Status status = writePageToDisk(frame_index, ctx, WritebackQueueState::NONE);
                if (status != Status::OK)
                {
                    markFrameWritebackFailure(frame_index, WritebackQueueState::NONE);
                    return status;
                }
                finishFrameWriteback(frame_index,
                                     flushed_generation,
                                     WritebackQueueState::NONE);
                stats_.flushes.fetch_add(1, std::memory_order_relaxed);
                stats_.evictions_dirty.fetch_add(1, std::memory_order_relaxed);
                goto evict_specific_frame_stats_done;
            }
        }
        {
            stats_.evictions_clean.fetch_add(1, std::memory_order_relaxed);
        }
evict_specific_frame_stats_done:

        const MgaPageClass page_class = static_cast<MgaPageClass>(
            frame.mga_page_class.load(std::memory_order_relaxed));
        const PolicyDomain domain = static_cast<PolicyDomain>(
            frame.policy_domain.load(std::memory_order_relaxed));
        const uint64_t eviction_generation =
            residency_generation_clock_.fetch_add(1, std::memory_order_relaxed) + 1;
        if (page_class == MgaPageClass::SCAN_PROBATION)
        {
            stats_.mga_scan_probation_churn.fetch_add(1, std::memory_order_relaxed);
        }
        recordGhostHistory(frame.gpid,
                           domain,
                           page_class,
                           residency_tier,
                           eviction_generation,
                           was_dirty ? GhostEvictionReason::DirtyForeground
                                     : (residency_tier == ResidencyTier::RingOnly
                                            ? GhostEvictionReason::RingChurn
                                            : GhostEvictionReason::ProbationaryAging));
        recordUnusedPrefetchEviction(frame);

        partition.table.erase(page_table_it);
        frame.lifecycle_state.store(static_cast<uint8_t>(LifecycleState::Evicting),
                                    std::memory_order_relaxed);
        frame.gpid = INVALID_GPID;
        frame.is_dirty.store(false, std::memory_order_relaxed);
        frame.pin_count.store(0, std::memory_order_relaxed);
        frame.usage_count.store(0, std::memory_order_relaxed);
        resetFrameScaffolding(frame);
        stats_.evictions.fetch_add(1, std::memory_order_relaxed);
        recordMgaEviction(page_class);
        return Status::OK;
    }

    auto BufferPool::evictPage(uint32_t &evicted_frame, ErrorContext *ctx) -> Status
    {
        // CLOCK SWEEP ALGORITHM (Issue 2.14)
        // This algorithm provides better eviction decisions than pure LRU by:
        // 1. Avoiding sequential scan pollution (frequently accessed pages stay in cache)
        // 2. Giving recently accessed pages a second chance (usage_count mechanism)
        // 3. Preferring clean pages over dirty pages for faster eviction
        //
        // Algorithm:
        // - Each frame has a usage_count (0-MAX_USAGE_COUNT)
        // - On access, usage_count is incremented (capped at MAX_USAGE_COUNT)
        // - Clock hand sweeps through frames circularly
        // - For each frame:
        //   * Skip if pinned (in use)
        //   * If usage_count == 0 and unpinned, evict it
        //   * Otherwise decrement usage_count (give it another chance)
        //
        // Spec: docs/specifications/STORAGE_ENGINE_BUFFER_POOL.md:402-465

        constexpr uint32_t MAX_PASSES = 2; // Maximum passes before forcing eviction
        constexpr uint32_t MAX_RETRIES = 16; // Retry if candidate mapping changes while selecting

        for (uint32_t attempt = 0; attempt < MAX_RETRIES; ++attempt)
        {
            if (config_.pool_size == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Buffer pool has zero-sized frame array");
                return Status::INVALID_ARGUMENT;
            }

            if (clock_hand_ >= config_.pool_size)
            {
                DEBUG_LOG_BP("Clock hand out of bounds at sweep start: " << clock_hand_
                                                                          << " >= pool_size: " << config_.pool_size);
                clock_hand_ = 0;
            }

            uint32_t candidate_frame = UINT32_MAX;
            uint32_t candidate_rank = UINT32_MAX;
            bool candidate_dirty = true;
            uint32_t protected_candidate_frame = UINT32_MAX;
            uint32_t hard_emergency_candidate_frame = UINT32_MAX;
            const uint64_t current_generation =
                residency_generation_clock_.load(std::memory_order_relaxed);
            const uint32_t protected_budget = protectedFrameBudget();
            uint32_t protected_frames = currentProtectedFrameCount();
            const DomainCounterArray resident_counts = collectDomainResidentCountsLocked();
            std::array<bool, static_cast<size_t>(PolicyDomain::Count)>
                reservation_skip_recorded{};
            uint32_t passes = 0;
            uint32_t scanned_in_pass = 0;

            // Clock sweep: search for victim page
            while (passes < MAX_PASSES)
            {
                // MEDIUM-1 FIX: Use relaxed atomic increment for stats
                stats_.clock_sweeps.fetch_add(1, std::memory_order_relaxed);

                // SAFETY: Bounds check for clock_hand_
                if (clock_hand_ >= config_.pool_size)
                {
                    DEBUG_LOG_BP("Clock hand out of bounds: " << clock_hand_
                                                              << " >= pool_size: " << config_.pool_size);
                    clock_hand_ = 0; // Reset to safe value
                }

                Frame &frame = frames_[clock_hand_];

                // Move clock hand forward (circular)
                uint32_t current_hand = clock_hand_;
                clock_hand_ = (clock_hand_ + 1) % config_.pool_size;

                // Track when we wrap around
                if (clock_hand_ == 0)
                {
                    // MEDIUM-1 FIX: Use relaxed atomic increment for stats
                    stats_.clock_hand_resets.fetch_add(1, std::memory_order_relaxed);
                }

                // Count every examined frame, even if it is pinned, so the sweep
                // can terminate cleanly when no frame is evictable.
                scanned_in_pass++;

                // Skip pinned frames (in use)
                // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
                if (frame.pin_count.load(std::memory_order_relaxed) > 0)
                {
                    if (scanned_in_pass >= config_.pool_size)
                    {
                        passes++;
                        scanned_in_pass = 0;
                    }
                    continue;
                }

                // Skip empty frames (these should be allocated first, not evicted)
                // PHASE 1, TASK 1.2.3: Changed page_id to gpid
                if (frame.gpid == INVALID_GPID)
                {
                    if (scanned_in_pass >= config_.pool_size)
                    {
                        passes++;
                        scanned_in_pass = 0;
                    }
                    continue;
                }

                const MgaPageClass page_class = static_cast<MgaPageClass>(
                    frame.mga_page_class.load(std::memory_order_relaxed));
                const PolicyDomain domain = static_cast<PolicyDomain>(
                    frame.policy_domain.load(std::memory_order_relaxed));
                ResidencyTier residency_tier = static_cast<ResidencyTier>(
                    frame.residency_tier.load(std::memory_order_relaxed));
                const bool commit_fence_member =
                    frame.commit_fence_member.load(std::memory_order_relaxed);
                const bool hard_protected = isHardProtectedFrame(frame);

                if (commit_fence_member || hard_protected ||
                    residency_tier == ResidencyTier::PinBiased)
                {
                    if (config_.layout == PoolLayout::Single && !commit_fence_member &&
                        hard_protected &&
                        hard_emergency_candidate_frame == UINT32_MAX)
                    {
                        hard_emergency_candidate_frame = current_hand;
                    }
                    if (scanned_in_pass >= config_.pool_size)
                    {
                        passes++;
                        scanned_in_pass = 0;
                    }
                    continue;
                }

                if (!canEvictFromDomainLocked(domain, resident_counts))
                {
                    const size_t domain_index = static_cast<size_t>(domain);
                    if (!reservation_skip_recorded[domain_index])
                    {
                        domain_reservation_breach_counts_[domain_index].fetch_add(
                            1, std::memory_order_relaxed);
                        reservation_skip_recorded[domain_index] = true;
                    }
                    if (scanned_in_pass >= config_.pool_size)
                    {
                        passes++;
                        scanned_in_pass = 0;
                    }
                    continue;
                }

                // Check usage count
                // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
                uint32_t current_usage_count = frame.usage_count.load(std::memory_order_relaxed);
                if (current_usage_count == 0)
                {
                    if (residency_tier == ResidencyTier::Protected)
                    {
                        const uint64_t last_touch_generation =
                            frame.last_touch_generation.load(std::memory_order_relaxed);
                        const uint64_t age = (current_generation > last_touch_generation)
                                                 ? (current_generation - last_touch_generation)
                                                 : 0;
                        const uint64_t max_protected_age = std::max<uint64_t>(
                            4, static_cast<uint64_t>(config_.admission_second_touch_generations) * 2);
                        if (protected_frames > protected_budget || age > max_protected_age)
                        {
                            frame.residency_tier.store(
                                static_cast<uint8_t>(ResidencyTier::Probationary),
                                std::memory_order_relaxed);
                            stats_.mga_residency_demotions.fetch_add(
                                1, std::memory_order_relaxed);
                            residency_tier = ResidencyTier::Probationary;
                            if (protected_frames > 0)
                            {
                                --protected_frames;
                            }
                        }
                        else
                        {
                            if (protected_candidate_frame == UINT32_MAX)
                            {
                                protected_candidate_frame = current_hand;
                            }
                            if (scanned_in_pass >= config_.pool_size)
                            {
                                passes++;
                                scanned_in_pass = 0;
                            }
                            continue;
                        }
                    }

                    if (page_class == MgaPageClass::GC_CANDIDATE)
                    {
                        offerGcCandidate(current_hand);
                    }

                    const bool is_dirty = frame.is_dirty.load(std::memory_order_acquire);
                    const uint32_t rank =
                        domainEvictionPriorityLocked(domain, resident_counts) * 128 +
                        evictionRankForTier(residency_tier) * 16 +
                                          evictionRankForClass(page_class);
                    if (candidate_frame == UINT32_MAX ||
                        rank < candidate_rank ||
                        (rank == candidate_rank && candidate_dirty && !is_dirty))
                    {
                        candidate_frame = current_hand;
                        candidate_rank = rank;
                        candidate_dirty = is_dirty;
                        if (rank == 0 && !is_dirty)
                        {
                            break;
                        }
                    }
                }
                else
                {
                    if (page_class == MgaPageClass::SCAN_PROBATION)
                    {
                        frame.usage_count.store(0, std::memory_order_relaxed);
                    }
                    else if (residency_tier == ResidencyTier::Protected)
                    {
                        if (current_usage_count > 1)
                        {
                            frame.usage_count.fetch_sub(1, std::memory_order_relaxed);
                        }
                    }
                    else if (page_class == MgaPageClass::CHAIN_HEAVY ||
                             page_class == MgaPageClass::INDEX_CHURN)
                    {
                        if (current_usage_count > 1)
                        {
                            frame.usage_count.fetch_sub(1, std::memory_order_relaxed);
                        }
                    }
                    else
                    {
                        // Give page another chance - decrement usage count
                        // CRITICAL FIX (CRITICAL-1): Use atomic fetch_sub for thread-safe decrement
                        frame.usage_count.fetch_sub(1, std::memory_order_relaxed);
                    }
                }

                // Count scanned frames deterministically. This avoids relying on start-hand
                // comparisons that can fail when the hand was previously out of range.
                if (scanned_in_pass >= config_.pool_size)
                {
                    passes++;
                    scanned_in_pass = 0;
                    if (candidate_frame != UINT32_MAX)
                    {
                        // We found a dirty page candidate, use it
                        break;
                    }
                    // Otherwise continue for another pass
                }
            }

            // Emergency fallback: force evict the least recently used dirty page
            // This should rarely happen - only if all pages have high usage counts
            if (candidate_frame == UINT32_MAX)
            {
                DEBUG_LOG_BP("Clock sweep failed after " << MAX_PASSES
                                                         << " passes, using LRU fallback");

                // Fallback to LRU for emergency eviction
                for (unsigned int frame_index : lru_list_)
                {
                    // DEFENSIVE CHECK (Issue 3.2): Validate LRU list entries
                    // This is NOT redundant - it validates data from lru_list_ which could be corrupted
                    if (frame_index >= config_.pool_size)
                    {
                        continue; // Skip invalid entries
                    }

                    // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
                    // PHASE 1, TASK 1.2.3: Changed page_id to gpid
                    const MgaPageClass page_class = static_cast<MgaPageClass>(
                        frames_[frame_index].mga_page_class.load(std::memory_order_relaxed));
                    const ResidencyTier residency_tier = static_cast<ResidencyTier>(
                        frames_[frame_index].residency_tier.load(std::memory_order_relaxed));
                    const bool hard_protected =
                        isHardProtectedFrame(frames_[frame_index]);
                    if (residency_tier == ResidencyTier::PinBiased ||
                        hard_protected)
                    {
                        if (config_.layout == PoolLayout::Single &&
                            !frames_[frame_index].commit_fence_member.load(
                                std::memory_order_relaxed) &&
                            hard_protected &&
                            hard_emergency_candidate_frame == UINT32_MAX)
                        {
                            hard_emergency_candidate_frame = frame_index;
                        }
                        continue;
                    }
                    const PolicyDomain domain = static_cast<PolicyDomain>(
                        frames_[frame_index].policy_domain.load(std::memory_order_relaxed));
                    if (!canEvictFromDomainLocked(domain, resident_counts))
                    {
                        const size_t domain_index = static_cast<size_t>(domain);
                        if (!reservation_skip_recorded[domain_index])
                        {
                            domain_reservation_breach_counts_[domain_index].fetch_add(
                                1, std::memory_order_relaxed);
                            reservation_skip_recorded[domain_index] = true;
                        }
                        continue;
                    }
                    if (residency_tier == ResidencyTier::Protected)
                    {
                        if (protected_candidate_frame == UINT32_MAX)
                        {
                            protected_candidate_frame = frame_index;
                        }
                        continue;
                    }
                    if (frames_[frame_index].pin_count.load(std::memory_order_relaxed) == 0 &&
                        frames_[frame_index].gpid != INVALID_GPID &&
                        !frames_[frame_index].commit_fence_member.load(std::memory_order_relaxed) &&
                        !hard_protected)
                    {
                        candidate_frame = frame_index;
                        break;
                    }
                }

                if (candidate_frame == UINT32_MAX && protected_candidate_frame != UINT32_MAX &&
                    commit_fence_depth_.load(std::memory_order_acquire) == 0)
                {
                    candidate_frame = protected_candidate_frame;
                    stats_.mga_protected_set_collapse_events.fetch_add(
                        1, std::memory_order_relaxed);
                    LOG_WARNING(BUFFER,
                                "MGA protected-set collapse: evicting protected class %s",
                                mgaPageClassToString(static_cast<MgaPageClass>(
                                    frames_[candidate_frame].mga_page_class.load(
                                        std::memory_order_relaxed))));
                }

                if (candidate_frame == UINT32_MAX &&
                    hard_emergency_candidate_frame != UINT32_MAX &&
                    commit_fence_depth_.load(std::memory_order_acquire) == 0 &&
                    config_.layout == PoolLayout::Single)
                {
                    // Legacy single-layout pools still need a last-resort
                    // bootstrap escape hatch. The canonical segmented policy
                    // does not use hard-protected collapse for TX_STATE.
                    candidate_frame = hard_emergency_candidate_frame;
                    domain_emergency_breach_counts_[static_cast<size_t>(
                        PolicyDomain::CriticalSystem)].fetch_add(1, std::memory_order_relaxed);
                    stats_.mga_protected_set_collapse_events.fetch_add(
                        1, std::memory_order_relaxed);
                    LOG_WARNING(BUFFER,
                                "MGA legacy-single emergency collapse: evicting hard-protected class %s",
                                mgaPageClassToString(static_cast<MgaPageClass>(
                                    frames_[candidate_frame].mga_page_class.load(
                                        std::memory_order_relaxed))));
                }
            }

            // Final check: did we find any evictable page?
            if (candidate_frame == UINT32_MAX)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Buffer pool full - all pages are pinned");
                return Status::INVALID_ARGUMENT;
            }

            // ALGORITHM OUTPUT VALIDATION (Issue 3.2): Final safety check
            // This is NOT redundant - it validates the algorithm's output (candidate_frame) which is
            // computed from clock sweep or LRU fallback logic. Different variable than internal checks.
            if (candidate_frame >= config_.pool_size)
            {
                DEBUG_LOG_BP("Invalid candidate_frame: " << candidate_frame
                                                         << " >= pool_size: " << config_.pool_size);
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Invalid frame index selected for eviction");
                return Status::IO_ERROR;
            }

            // Lock the partition to serialize against pin/unpin for this page.
            // This prevents evicting a frame that becomes pinned concurrently.
            // PHASE 1, TASK 1.2.3: Changed page_id to gpid
            GPID evicted_gpid = frames_[candidate_frame].gpid;
            if (evicted_gpid == INVALID_GPID)
            {
                DEBUG_LOG_BP("Evicting frame with INVALID_GPID at index " << candidate_frame);
                frames_[candidate_frame].lifecycle_state.store(
                    static_cast<uint8_t>(LifecycleState::Free),
                    std::memory_order_relaxed);
                evicted_frame = candidate_frame;
                return Status::OK;
            }

            size_t partition_idx = getPartitionIndex(evicted_gpid);
            auto& partition = page_table_partitions_[partition_idx];
            std::lock_guard<std::mutex> partition_lock(partition.mutex);

            auto page_table_it = partition.table.find(evicted_gpid);
            if (page_table_it == partition.table.end() || page_table_it->second != candidate_frame)
            {
                std::this_thread::yield();
                continue;
            }

            // Wait for an in-flight writer to finish publishing this frame rather than
            // surfacing a false "all pages pinned" exhaustion to the caller.
            std::unique_lock<std::mutex> content_lock(*frames_[candidate_frame].content_mutex);

            // CRITICAL FIX (Issue 2.2): Consistency check - verify frame is unpinned
            // This MUST be fatal in ALL builds (not just debug) to prevent corruption
            // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
            uint32_t evicted_pin_count =
                frames_[candidate_frame].pin_count.load(std::memory_order_relaxed);
            if (evicted_pin_count != 0)
            {
                std::this_thread::yield();
                continue;
            }
            if (frames_[candidate_frame].gpid != evicted_gpid)
            {
                std::this_thread::yield();
                continue;
            }

            evicted_frame = candidate_frame;
            const MgaPageClass evicted_page_class = static_cast<MgaPageClass>(
                frames_[evicted_frame].mga_page_class.load(std::memory_order_relaxed));
            const PolicyDomain evicted_domain = static_cast<PolicyDomain>(
                frames_[evicted_frame].policy_domain.load(std::memory_order_relaxed));
            const ResidencyTier evicted_residency_tier = static_cast<ResidencyTier>(
                frames_[evicted_frame].residency_tier.load(std::memory_order_relaxed));
            if (evicted_page_class == MgaPageClass::GC_CANDIDATE)
            {
                offerGcCandidate(evicted_frame);
            }

            // Track whether this is a clean or dirty eviction
            bool was_dirty = frames_[evicted_frame].is_dirty.load(std::memory_order_acquire);

            // If dirty, flush first
            if (was_dirty)
            {
                auto *header =
                    reinterpret_cast<PageHeader *>(frames_[evicted_frame].data.get());
                if (header != nullptr && pageIsTemporaryWork(*header) &&
                    header->page_type == PAGE_TYPE_TEMP_HEAP)
                {
                    // Scratch temp-heap/workfile pages are intentionally lossy across
                    // eviction. Session temp tables also carry the temporary-work
                    // flag, but they must remain reloadable within the session, so
                    // ordinary heap pages still go through writePageToDisk().
                    discardTemporaryWorkFrameDirtyState(evicted_frame);
                }
                else
                {
                    const uint64_t flushed_generation =
                        frames_[evicted_frame].dirty_generation.load(std::memory_order_acquire);
                    beginFrameWriteback(evicted_frame, WritebackQueueState::NONE);
                    Status status = writePageToDisk(evicted_frame,
                                                    ctx,
                                                    WritebackQueueState::NONE);
                    if (status != Status::OK)
                    {
                        markFrameWritebackFailure(evicted_frame,
                                                  WritebackQueueState::NONE);
                        return status;
                    }
                    finishFrameWriteback(evicted_frame,
                                         flushed_generation,
                                         WritebackQueueState::NONE);
                    // MEDIUM-1 FIX: Use relaxed atomic increment for stats
                    stats_.flushes.fetch_add(1, std::memory_order_relaxed);
                    stats_.evictions_dirty.fetch_add(1, std::memory_order_relaxed);
                    goto evict_page_stats_done;
                }
            }
            {
                // MEDIUM-1 FIX: Use relaxed atomic increment for stats
                stats_.evictions_clean.fetch_add(1, std::memory_order_relaxed);
            }
evict_page_stats_done:

            if (evicted_page_class == MgaPageClass::SCAN_PROBATION)
            {
                stats_.mga_scan_probation_churn.fetch_add(1, std::memory_order_relaxed);
            }

            const uint64_t eviction_generation =
                residency_generation_clock_.fetch_add(1, std::memory_order_relaxed) + 1;
            recordGhostHistory(evicted_gpid,
                               evicted_domain,
                               evicted_page_class,
                               evicted_residency_tier,
                               eviction_generation,
                               (evicted_residency_tier == ResidencyTier::Protected ||
                                evicted_residency_tier == ResidencyTier::PinBiased)
                                   ? GhostEvictionReason::EmergencyCollapse
                                   : (was_dirty ? GhostEvictionReason::DirtyForeground
                                                   : (evicted_residency_tier == ResidencyTier::RingOnly
                                                       ? GhostEvictionReason::RingChurn
                                                       : GhostEvictionReason::ProbationaryAging)));
            recordUnusedPrefetchEviction(frames_[evicted_frame]);

            // Remove from page table
            partition.table.erase(page_table_it);

            // Reset frame (including Clock Sweep usage_count)
            // PHASE 1, TASK 1.2.3: Changed page_id to gpid
            frames_[evicted_frame].lifecycle_state.store(
                static_cast<uint8_t>(LifecycleState::Evicting),
                std::memory_order_relaxed);
            frames_[evicted_frame].gpid = INVALID_GPID;
            frames_[evicted_frame].is_dirty.store(false, std::memory_order_relaxed);
            frames_[evicted_frame].pin_count.store(0, std::memory_order_relaxed);
            // CRITICAL FIX (CRITICAL-1): Use atomic store for thread-safe write
            frames_[evicted_frame].usage_count.store(0, std::memory_order_relaxed); // Reset usage count for next page
            resetFrameScaffolding(frames_[evicted_frame]);

            // MEDIUM-1 FIX: Use relaxed atomic increment for stats
            stats_.evictions.fetch_add(1, std::memory_order_relaxed);
            recordMgaEviction(evicted_page_class);
            return Status::OK;
        }

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Buffer pool full - all pages are pinned");
        return Status::INVALID_ARGUMENT;
    }

    // PHASE 1, TASK 1.2.4: Use Database::read_page_global() for GPID-based I/O
    auto BufferPool::readPageFromDisk(GPID gpid, uint8_t *buffer, ErrorContext *ctx)
        -> Status
    {
        // Call new Database GPID method (supports multi-tablespace addressing)
        // Database class handles tablespace validation and routing for Phase 1
        Status status = db_->read_page_global(gpid, buffer, ctx);
        if (status == Status::OK && metrics_ && metrics_->buffer_pool_reads_total)
        {
            metrics_->buffer_pool_reads_total->inc();
        }
        if (status == Status::OK)
        {
            VNextMetricsEventModel::recordStorageEvent(
                "buffer_pool_io_read", "ok", "NONE");
        }
        else
        {
            VNextMetricsEventModel::recordStorageEvent(
                "buffer_pool_io_read", "error",
                std::to_string(static_cast<int>(status)));
        }
        return status;
    }

    // PHASE 1, TASK 1.2.4: Use Database::write_page_global() for GPID-based I/O
    auto BufferPool::writePageToDisk(uint32_t frame_index,
                                     ErrorContext *ctx,
                                     WritebackQueueState queue_state,
                                     bool checkpoint_flush)
        -> Status
    {
        if (frame_index >= frames_.size())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid frame index for writeback");
            return Status::INVALID_ARGUMENT;
        }

        const GPID gpid = frames_[frame_index].gpid;
        auto *mutable_buffer = frames_[frame_index].data.get();
        if (gpid == INVALID_GPID || mutable_buffer == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Invalid frame selected for writeback");
            return Status::INVALID_ARGUMENT;
        }

        auto *header = reinterpret_cast<PageHeader *>(mutable_buffer);
        const bool temporary_work = pageIsTemporaryWork(*header);
        if (temporary_work)
        {
            header->flush_generation = 0;
            header->checkpoint_generation = 0;
        }
        else
        {
            if (header->generation == 0)
            {
                header->generation = 1;
            }
            header->flush_generation = header->generation;
            if (checkpoint_flush)
            {
                header->checkpoint_generation = header->flush_generation;
            }
            else if (header->checkpoint_generation > header->flush_generation)
            {
                header->checkpoint_generation = header->flush_generation;
            }
        }

        WritebackAttribution attribution{};
        switch (queue_state)
        {
            case WritebackQueueState::FOREGROUND_HELP:
                attribution.queue_kind = WritebackQueueKind::FOREGROUND_HELP;
                break;
            case WritebackQueueState::BACKGROUND_AGE:
                attribution.queue_kind = WritebackQueueKind::BACKGROUND_AGE;
                break;
            case WritebackQueueState::CHECKPOINT:
                attribution.queue_kind = WritebackQueueKind::CHECKPOINT;
                break;
            case WritebackQueueState::METADATA_PRIORITY:
                attribution.queue_kind = WritebackQueueKind::METADATA_PRIORITY;
                break;
            case WritebackQueueState::WRITE_COMBINE:
                attribution.queue_kind = WritebackQueueKind::WRITE_COMBINE;
                break;
            case WritebackQueueState::REPAIR_RETRY:
                attribution.queue_kind = WritebackQueueKind::REPAIR_RETRY;
                break;
            case WritebackQueueState::NONE:
            default:
                attribution.queue_kind = WritebackQueueKind::UNKNOWN;
                break;
        }

        const MgaPageClass page_class = static_cast<MgaPageClass>(
            frames_[frame_index].mga_page_class.load(std::memory_order_relaxed));
        switch (page_class)
        {
            case MgaPageClass::TX_STATE:
            case MgaPageClass::VERSION_ROOT:
            case MgaPageClass::CHAIN_HEAVY:
            case MgaPageClass::GC_CANDIDATE:
                attribution.policy_domain = WritebackPolicyDomain::TRANSACTION;
                break;
            case MgaPageClass::SYSTEM_META:
            case MgaPageClass::INDEX_ROOT_INTERNAL:
                attribution.policy_domain = WritebackPolicyDomain::SYSTEM_STATE;
                break;
            case MgaPageClass::TEMP_WORK:
                attribution.policy_domain = WritebackPolicyDomain::ALLOCATOR;
                break;
            case MgaPageClass::Generic:
            case MgaPageClass::SCAN_PROBATION:
            case MgaPageClass::INDEX_CHURN:
            default:
                attribution.policy_domain =
                    queue_state == WritebackQueueState::CHECKPOINT
                    ? WritebackPolicyDomain::CHECKPOINT
                    : WritebackPolicyDomain::UNKNOWN;
                break;
        }
        if (queue_state == WritebackQueueState::CHECKPOINT)
        {
            attribution.policy_domain = WritebackPolicyDomain::CHECKPOINT;
        }
        attribution.dirty_generation =
            frames_[frame_index].dirty_generation.load(std::memory_order_relaxed);

        // Call new Database GPID method (supports multi-tablespace addressing)
        // Database class handles tablespace validation and routing for Phase 1
        frames_[frame_index].io_generation.fetch_add(1, std::memory_order_relaxed);
        Status status = db_->write_page_global(gpid, mutable_buffer, ctx, attribution);
        if (status == Status::OK)
        {
            {
                std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
                if (temporary_work ||
                    header->flush_generation <= header->checkpoint_generation)
                {
                    checkpoint_marker_candidates_.erase(gpid);
                }
                else
                {
                    checkpoint_marker_candidates_.insert(gpid);
                }
            }
            if (metrics_ && metrics_->buffer_pool_writes_total)
            {
                metrics_->buffer_pool_writes_total->inc();
            }
            if (auto* conn_ctx = ConnectionContext::getCurrent())
            {
                conn_ctx->recordPageWrite();
            }
            VNextMetricsEventModel::recordStorageEvent(
                "buffer_pool_io_write", "ok", "NONE");
        }
        else
        {
            VNextMetricsEventModel::recordStorageEvent(
                "buffer_pool_io_write", "error",
                std::to_string(static_cast<int>(status)));
        }
        return status;
    }

    void BufferPool::updateLru(uint32_t frame_index)
    {
        // CRITICAL: This method MUST be called with mutex_ held
        // The LRU list is shared state and concurrent modification will cause corruption
        // We use assert() because this is an internal consistency requirement
        // NOTE: There's no portable way to assert a mutex is locked, so we document the requirement
        // and rely on correct usage patterns. All callers (pinPage) do hold the lock.

        // INTERNAL CONSISTENCY CHECK (Issue 3.2 consolidation):
        // This is an internal method - callers must provide valid frame_index
        // Use assertion instead of runtime check since this indicates a programming error
        assert(frame_index < config_.pool_size && "updateLru called with invalid frame_index");

        // Remove from current position in LRU list
        lru_list_.remove(frame_index);

        // Add to end of LRU list (most recently used)
        lru_list_.push_back(frame_index);
    }

    void BufferPool::insertLruMidpoint(uint32_t frame_index)
    {
        // CRITICAL: This method MUST be called with mutex_ held
        assert(frame_index < config_.pool_size && "insertLruMidpoint called with invalid frame_index");

        // Remove any stale entry, then insert at midpoint to avoid scan pollution.
        lru_list_.remove(frame_index);
        if (lru_list_.empty())
        {
            lru_list_.push_back(frame_index);
            return;
        }

        size_t midpoint = lru_list_.size() / 2;
        auto it = lru_list_.begin();
        std::advance(it, static_cast<long>(midpoint));
        lru_list_.insert(it, frame_index);
    }

    // PHASE 1, TASK 1.2.3: LEGACY API - Convert result GPID to page_id
    auto BufferPool::allocatePage(uint32_t *page_id_out, void **buffer, ErrorContext *ctx) -> Status
    {
        if (page_id_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null page_id_out pointer");
            return Status::INVALID_ARGUMENT;
        }

        GPID gpid;
        Status status = allocatePageGlobal(PRIMARY_TABLESPACE_ID, &gpid, buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Convert GPID to page_id (safe since we're using PRIMARY_TABLESPACE_ID)
        uint32_t page_id;
        if (!convertGPIDtoPageID(gpid, &page_id))
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to convert GPID to page_id");
            return Status::IO_ERROR;
        }

        *page_id_out = page_id;
        return Status::OK;
    }

    // PHASE 1, TASK 1.2.3: NEW GPID-based implementation
    auto BufferPool::allocatePageGlobal(uint16_t tablespace_id, GPID *gpid_out, void **buffer,
                                       ErrorContext *ctx) -> Status
    {
        if (gpid_out == nullptr || buffer == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null output parameters");
            return Status::INVALID_ARGUMENT;
        }

        // Use Database::allocate_page_id_global() for GPID allocation
        // Supports both primary (0) and custom (1-65535) tablespaces
        GPID new_gpid;
        Status status = db_->allocate_page_id_global(tablespace_id, &new_gpid, ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(GENERAL,
                        "BufferPool::allocatePageGlobal allocate_page_id_global failed: tablespace=%u status=%d msg=%s",
                        static_cast<unsigned int>(tablespace_id),
                        static_cast<int>(status),
                        ctx ? ctx->message.c_str() : "");
            return status;
        }

        // Pin the new page (this will allocate a frame and initialize it)
        status = pinPageGlobal(new_gpid, buffer, ctx);
        if (status != Status::OK)
        {
            // Failed to pin - the gpid has been allocated but not used
            // For simplicity, we don't reclaim it (would require free list)
            return status;
        }

        // Mark the new page as dirty since it needs to be written
        status = markDirtyGlobal(new_gpid, ctx);
        if (status != Status::OK)
        {
            // Unpin on failure
            unpinPageGlobal(new_gpid, false, ctx);
            return status;
        }

        *gpid_out = new_gpid;
        return Status::OK;
    }

    bool BufferPool::tryMarkFrameDirty(uint32_t frame_index)
    {
        bool expected = false;
        if (frames_[frame_index].is_dirty.compare_exchange_strong(expected, true,
                                                                  std::memory_order_acq_rel,
                                                                  std::memory_order_acquire))
        {
            frames_[frame_index].dirty_state.store(
                static_cast<uint8_t>(DirtyState::DirtyUnscheduled),
                std::memory_order_relaxed);
            dirty_page_count_.fetch_add(1, std::memory_order_relaxed);
            updateDirtyTelemetry();
            return true;
        }
        return false;
    }

    bool BufferPool::tryClearFrameDirty(uint32_t frame_index)
    {
        bool expected = true;
        if (frames_[frame_index].is_dirty.compare_exchange_strong(expected, false,
                                                                  std::memory_order_acq_rel,
                                                                  std::memory_order_acquire))
        {
            frames_[frame_index].dirty_state.store(static_cast<uint8_t>(DirtyState::Clean),
                                                   std::memory_order_relaxed);
            dirty_page_count_.fetch_sub(1, std::memory_order_relaxed);
            updateDirtyTelemetry();
            return true;
        }
        return false;
    }

    void BufferPool::discardTemporaryWorkFrameDirtyState(uint32_t frame_index)
    {
        const GPID gpid = frames_[frame_index].gpid;
        (void)tryClearFrameDirty(frame_index);
        frames_[frame_index].dirty_generation.store(0, std::memory_order_relaxed);
        frames_[frame_index].last_flush_generation.store(0, std::memory_order_relaxed);
        frames_[frame_index].checkpoint_target_generation.store(0, std::memory_order_relaxed);
        frames_[frame_index].writeback_queue_state.store(
            static_cast<uint8_t>(WritebackQueueState::NONE),
            std::memory_order_relaxed);

        if (gpid == INVALID_GPID)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
        dirty_checkpoint_candidates_.erase(gpid);
        checkpoint_marker_candidates_.erase(gpid);
    }

    bool BufferPool::finishFrameWriteback(uint32_t frame_index,
                                          uint64_t flushed_generation,
                                          WritebackQueueState dirty_queue_state)
    {
        const uint64_t current_generation =
            frames_[frame_index].dirty_generation.load(std::memory_order_acquire);
        frames_[frame_index].last_flush_generation.store(flushed_generation,
                                                         std::memory_order_relaxed);
        frames_[frame_index].lifecycle_state.store(static_cast<uint8_t>(LifecycleState::Valid),
                                                   std::memory_order_relaxed);
        if (current_generation != flushed_generation)
        {
            frames_[frame_index].writeback_queue_state.store(
                static_cast<uint8_t>(classifyWritebackQueueState(frames_[frame_index])),
                std::memory_order_relaxed);
            frames_[frame_index].dirty_state.store(
                static_cast<uint8_t>(DirtyState::DirtyUnscheduled),
                std::memory_order_relaxed);
            return false;
        }

        if (dirty_queue_state != WritebackQueueState::NONE)
        {
            frames_[frame_index].writeback_queue_state.store(
                static_cast<uint8_t>(dirty_queue_state),
                std::memory_order_relaxed);
            frames_[frame_index].dirty_state.store(
                static_cast<uint8_t>(DirtyState::DirtyFlushedPendingFsync),
                std::memory_order_relaxed);
            return true;
        }

        const bool cleared = tryClearFrameDirty(frame_index);
        frames_[frame_index].writeback_queue_state.store(
            static_cast<uint8_t>(cleared ? WritebackQueueState::NONE : dirty_queue_state),
            std::memory_order_relaxed);
        return cleared;
    }

    uint64_t BufferPool::publishDirtyGeneration(uint32_t frame_index)
    {
        const uint64_t dirty_generation =
            dirty_generation_clock_.fetch_add(1, std::memory_order_relaxed) + 1;
        frames_[frame_index].dirty_generation.store(dirty_generation,
                                                    std::memory_order_relaxed);
        frames_[frame_index].checkpoint_target_generation.store(
            0, std::memory_order_relaxed);
        frames_[frame_index].dirty_state.store(
            static_cast<uint8_t>(DirtyState::DirtyUnscheduled),
            std::memory_order_relaxed);
        frames_[frame_index].writeback_queue_state.store(
            static_cast<uint8_t>(classifyWritebackQueueState(frames_[frame_index])),
            std::memory_order_relaxed);
        const GPID gpid = frames_[frame_index].gpid;
        if (gpid != INVALID_GPID)
        {
            std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
            dirty_checkpoint_candidates_[gpid] = dirty_generation;
        }
        return dirty_generation;
    }

    // PHASE 1, TASK 1.2.3: LEGACY API - Convert page_id to GPID and call markDirtyGlobal
    auto BufferPool::markDirty(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        GPID gpid = convertPageIDtoGPID(page_id);
        return markDirtyGlobal(gpid, ctx);
    }

    // PHASE 1, TASK 1.2.3: NEW GPID-based implementation
    // P2-1: Updated to use partitioned page table locks
    auto BufferPool::markDirtyGlobal(GPID gpid, ErrorContext *ctx) -> Status
    {
        // P2-1: Only need partition lock (no global lock needed)
        size_t partition_idx = getPartitionIndex(gpid);
        auto& partition = page_table_partitions_[partition_idx];

        std::lock_guard<std::mutex> partition_lock(partition.mutex);

        // Find the page in buffer pool
        auto it = partition.table.find(gpid);
        if (it == partition.table.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page not in buffer pool");
            return Status::INVALID_ARGUMENT;
        }

        uint32_t frame_index = it->second;

        publishDirtyGeneration(frame_index);

        // P2-2: Update atomic dirty page counter when transitioning to dirty
        if (tryMarkFrameDirty(frame_index))
        {
            if (auto* conn_ctx = ConnectionContext::getCurrent())
            {
                conn_ctx->recordPageMark();
            }
        }

        return Status::OK;
    }

    // ===========================================================================================
    // ISSUE 2.20: ADAPTIVE FLUSHING - BACKGROUND WRITER IMPLEMENTATION
    // ===========================================================================================
    //
    // This implementation addresses the audit finding:
    // "Buffer Pool - No Adaptive Flushing"
    // File: src/core/buffer_pool.cpp:438-449
    // Severity: MAJOR
    //
    // Problem:
    // - Flushing only occurs when evicting dirty pages
    // - Causes checkpoint storms (unpredictable I/O spikes)
    // - Long checkpoint times block transactions
    // - No proactive dirty page management
    //
    // Solution:
    // - Background writer thread with adaptive flushing
    // - Dirty ratio monitoring (percentage of dirty pages)
    // - Three-tier flushing strategy:
    //   * Low threshold (25%): Start gentle flushing
    //   * High threshold (50%): Aggressive flushing
    //   * Checkpoint threshold (75%): Emergency flushing
    // - Smooths I/O load over time
    // - Prevents checkpoint storms
    //
    // Benefits:
    // - Predictable I/O patterns
    // - Shorter checkpoint times (less dirty pages to flush)
    // - Better transaction throughput (fewer eviction stalls)
    // - Configurable flushing behavior
    //
    // Algorithm based on PostgreSQL's bgwriter and MySQL InnoDB's adaptive flushing
    // Spec: docs/specifications/STORAGE_ENGINE_BUFFER_POOL.md (background writer)

    void BufferPool::startBackgroundWriter()
    {
        // CRITICAL: This method is called while holding mutex_ in initialize()
        // Do NOT acquire mutex_ here to avoid deadlock

        {
            std::lock_guard<std::mutex> lock(bgwriter_mutex_);
            bgwriter_shutdown_ = false;
        }
        bgwriter_thread_ = std::make_unique<std::thread>(&BufferPool::backgroundWriterMain, this);
        uint64_t tid = std::hash<std::thread::id>{}(bgwriter_thread_->get_id());
        LOG_INFO(GENERAL, "Background writer thread started (tid=%lu)",
                 static_cast<unsigned long>(tid));
    }

    void BufferPool::stopBackgroundWriter()
    {
        // Signal shutdown under bgwriter coordination lock.
        {
            std::lock_guard<std::mutex> lock(bgwriter_mutex_);
            bgwriter_shutdown_ = true;
            bgwriter_cv_.notify_one();
        }

        // Wait for thread to finish
        if (bgwriter_thread_ && bgwriter_thread_->joinable())
        {
            bgwriter_thread_->join();
        }
    }

    void BufferPool::backgroundWriterMain()
    {
        // Background writer thread main loop
        // This runs continuously until shutdown is requested

        ErrorContext ctx;

        while (true)
        {
            // Sleep for configured delay (using condition variable for interruptible sleep)
            {
                std::unique_lock<std::mutex> lock(bgwriter_mutex_);
                if (bgwriter_shutdown_)
                {
                    break;
                }
                bgwriter_cv_.wait_for(lock,
                                       std::chrono::milliseconds(config_.bgwriter_delay_ms),
                                       [this] { return bgwriter_shutdown_; });
                if (bgwriter_shutdown_)
                {
                    break;
                }
            }

            // Perform one cycle of adaptive flushing
            backgroundWriterFlush(&ctx);

            // Update statistics (dirty ratio tracking)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.dirty_ratio_current = calculateDirtyRatio();
                if (stats_.dirty_ratio_current > stats_.dirty_ratio_max)
                {
                    stats_.dirty_ratio_max = stats_.dirty_ratio_current;
                }
            }
        }
    }

    void BufferPool::backgroundWriterFlush(ErrorContext *ctx)
    {
        // Perform one cycle of adaptive flushing
        // This implements the three-tier flushing strategy based on dirty ratio

        uint32_t pages_written = 0;
        uint32_t pages_to_write = 0;
        double dirty_ratio = 0.0;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Calculate current dirty ratio
            dirty_ratio = calculateDirtyRatio();

            // Determine how many pages to write based on dirty ratio (adaptive algorithm)
            if (dirty_ratio >= config_.dirty_ratio_checkpoint)
            {
                // EMERGENCY: Checkpoint threshold exceeded
                // Write maximum pages to prevent checkpoint storm
                pages_to_write = config_.bgwriter_max_pages;
            }
            else if (dirty_ratio >= config_.dirty_ratio_high)
            {
                // AGGRESSIVE: High threshold exceeded
                // Write 75% of maximum pages
                pages_to_write = static_cast<uint32_t>(config_.bgwriter_max_pages * 0.75);
            }
            else if (dirty_ratio >= config_.dirty_ratio_low)
            {
                // GENTLE: Low threshold exceeded
                // Write scaled based on how far above low threshold
                // Scale linearly from 25% to 75% of max_pages
                double scale = (dirty_ratio - config_.dirty_ratio_low) /
                               (config_.dirty_ratio_high - config_.dirty_ratio_low);
                pages_to_write = static_cast<uint32_t>(config_.bgwriter_max_pages * (0.25 + scale * 0.50));
            }
            else
            {
                // Below low threshold - no flushing needed
                // MEDIUM-1 FIX: Use relaxed atomic increment for stats
                stats_.bgwriter_runs.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }

        if (pages_to_write == 0)
        {
            pages_to_write = 1;
        }

        struct WritebackCandidate
        {
            uint32_t frame_index = UINT32_MAX;
            WritebackQueueState queue_state = WritebackQueueState::NONE;
            uint32_t priority = UINT32_MAX;
            uint64_t dirty_generation = 0;
            uint32_t usage_count = 0;
        };

        auto queuePriority = [](WritebackQueueState queue_state) -> uint32_t
        {
            switch (queue_state)
            {
                case WritebackQueueState::FOREGROUND_HELP:
                    return 0;
                case WritebackQueueState::CHECKPOINT:
                    return 1;
                case WritebackQueueState::METADATA_PRIORITY:
                    return 2;
                case WritebackQueueState::BACKGROUND_AGE:
                    return 3;
                case WritebackQueueState::WRITE_COMBINE:
                    return 4;
                case WritebackQueueState::REPAIR_RETRY:
                    return 5;
                case WritebackQueueState::NONE:
                default:
                    return 6;
            }
        };

        while (pages_written < pages_to_write)
        {
            std::vector<WritebackCandidate> candidates;
            candidates.reserve(config_.pool_size);

            for (uint32_t i = 0; i < config_.pool_size; ++i)
            {
                Frame &frame = frames_[i];
                std::unique_lock<std::mutex> content_lock(*frame.content_mutex, std::try_to_lock);
                if (!content_lock.owns_lock())
                {
                    continue;
                }

                const DirtyState dirty_state = static_cast<DirtyState>(
                    frame.dirty_state.load(std::memory_order_relaxed));
                if (!frame.is_dirty.load(std::memory_order_acquire) ||
                    dirty_state == DirtyState::DirtyInFlight ||
                    dirty_state == DirtyState::DirtyFlushedPendingFsync ||
                    frame.pin_count.load(std::memory_order_relaxed) > 0 ||
                    frame.gpid == INVALID_GPID)
                {
                    continue;
                }
                auto *header = reinterpret_cast<PageHeader *>(frame.data.get());
                if (header != nullptr && pageIsTemporaryWork(*header))
                {
                    continue;
                }

                const MgaPageClass page_class = static_cast<MgaPageClass>(
                    frame.mga_page_class.load(std::memory_order_relaxed));
                WritebackQueueState queue_state = static_cast<WritebackQueueState>(
                    frame.writeback_queue_state.load(std::memory_order_relaxed));
                const WritebackQueueState derived_queue_state =
                    classifyWritebackQueueState(frame);
                if (queue_state == WritebackQueueState::NONE ||
                    (queue_state != WritebackQueueState::CHECKPOINT &&
                     queue_state != WritebackQueueState::REPAIR_RETRY &&
                     queue_state != derived_queue_state))
                {
                    queue_state = derived_queue_state;
                    frame.writeback_queue_state.store(
                        static_cast<uint8_t>(queue_state),
                        std::memory_order_relaxed);
                }

                if (queue_state == WritebackQueueState::NONE)
                {
                    continue;
                }
                if (page_class == MgaPageClass::SCAN_PROBATION &&
                    queue_state == WritebackQueueState::BACKGROUND_AGE &&
                    dirty_ratio < config_.dirty_ratio_checkpoint)
                {
                    continue;
                }

                candidates.push_back(WritebackCandidate{
                    i,
                    queue_state,
                    queuePriority(queue_state),
                    frame.dirty_generation.load(std::memory_order_relaxed),
                    frame.usage_count.load(std::memory_order_relaxed)});
            }

            if (candidates.empty())
            {
                break;
            }

            std::sort(candidates.begin(),
                      candidates.end(),
                      [](const WritebackCandidate &lhs, const WritebackCandidate &rhs)
                      {
                          if (lhs.priority != rhs.priority)
                          {
                              return lhs.priority < rhs.priority;
                          }
                          if (lhs.dirty_generation != rhs.dirty_generation)
                          {
                              if (lhs.dirty_generation == 0)
                              {
                                  return false;
                              }
                              if (rhs.dirty_generation == 0)
                              {
                                  return true;
                              }
                              return lhs.dirty_generation < rhs.dirty_generation;
                          }
                          if (lhs.usage_count != rhs.usage_count)
                          {
                              return lhs.usage_count < rhs.usage_count;
                          }
                          return lhs.frame_index < rhs.frame_index;
                      });

            const WritebackCandidate candidate = candidates.front();
            Frame &frame = frames_[candidate.frame_index];
            std::unique_lock<std::mutex> content_lock(*frame.content_mutex, std::try_to_lock);
            if (!content_lock.owns_lock())
            {
                continue;
            }

            const DirtyState dirty_state = static_cast<DirtyState>(
                frame.dirty_state.load(std::memory_order_relaxed));
            if (!frame.is_dirty.load(std::memory_order_acquire) ||
                dirty_state == DirtyState::DirtyInFlight ||
                dirty_state == DirtyState::DirtyFlushedPendingFsync ||
                frame.pin_count.load(std::memory_order_relaxed) > 0 ||
                frame.gpid == INVALID_GPID)
            {
                continue;
            }
            auto *header = reinterpret_cast<PageHeader *>(frame.data.get());
            if (header != nullptr && pageIsTemporaryWork(*header))
            {
                continue;
            }

            const uint64_t flushed_generation =
                frame.dirty_generation.load(std::memory_order_acquire);
            const uint64_t checkpoint_target_generation =
                candidate.queue_state == WritebackQueueState::CHECKPOINT
                ? frame.checkpoint_target_generation.load(std::memory_order_relaxed)
                : 0;
            beginFrameWriteback(candidate.frame_index,
                                candidate.queue_state,
                                checkpoint_target_generation);
            Status status = writePageToDisk(candidate.frame_index,
                                            ctx,
                                            candidate.queue_state,
                                            candidate.queue_state ==
                                                WritebackQueueState::CHECKPOINT);
            if (status == Status::OK)
            {
                finishFrameWriteback(candidate.frame_index,
                                     flushed_generation,
                                     candidate.queue_state);
                pages_written++;
                stats_.bgwriter_pages_written.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            markFrameWritebackFailure(candidate.frame_index, candidate.queue_state);
            DEBUG_LOG_BP("Background writer failed to flush page "
                         << gpidToString(frame.gpid)
                         << " queue="
                         << writebackQueueStateToString(candidate.queue_state)
                         << ": " << static_cast<int>(status));
        }

        // Update statistics
        // MEDIUM-1 FIX: Use relaxed atomic increment for stats
        stats_.bgwriter_runs.fetch_add(1, std::memory_order_relaxed);
        if (pages_written >= pages_to_write)
        {
            // MEDIUM-1 FIX: Use relaxed atomic increment for stats
            stats_.bgwriter_maxwritten.fetch_add(1, std::memory_order_relaxed);
        }
    }

    auto BufferPool::flushDirtyCheckpointBoundary(uint64_t dirty_generation_boundary,
                                                  ErrorContext *ctx) -> Status
    {
        if (dirty_generation_boundary == 0)
        {
            return dirty_page_count_.load(std::memory_order_relaxed) == 0
                ? Status::OK
                : flushAll(ctx);
        }

        auto republishNonResidentCheckpointMarker =
            [&](GPID checkpoint_gpid) -> Status
        {
            std::vector<uint8_t> page_image(config_.page_size);
            Status read_status = db_->read_page_global(checkpoint_gpid,
                                                       page_image.data(),
                                                       ctx);
            if (read_status != Status::OK)
            {
                return read_status;
            }

            auto *header = reinterpret_cast<PageHeader *>(page_image.data());
            const uint64_t page_number = getPageNumber(checkpoint_gpid);
            if (header->magic != K_MAGIC_SBRD ||
                header->page_id != page_number ||
                header->page_size != config_.page_size ||
                !validatePageChecksum(page_image.data(), config_.page_size))
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::PAGE_CORRUPT,
                                  "Checkpoint queue rebuild encountered a corrupt nonresident page");
                return Status::PAGE_CORRUPT;
            }

            if (pageIsTemporaryWork(*header) ||
                header->flush_generation <= header->checkpoint_generation)
            {
                return Status::OK;
            }

            const uint64_t published_generation =
                header->generation == 0 ? 1 : header->generation + 1;
            header->flush_generation = published_generation;
            header->checkpoint_generation = published_generation;

            WritebackAttribution attribution{};
            attribution.queue_kind = WritebackQueueKind::CHECKPOINT;
            attribution.policy_domain = WritebackPolicyDomain::CHECKPOINT;
            attribution.page_class = header->page_type;
            const Status write_status = db_->write_page_global(checkpoint_gpid,
                                                               page_image.data(),
                                                               ctx,
                                                               attribution);
            if (write_status == Status::OK)
            {
                stats_.checkpoint_flushes.fetch_add(1, std::memory_order_relaxed);
            }
            return write_status;
        };

        std::vector<std::pair<GPID, uint64_t>> candidates;
        std::unordered_set<GPID> seen_gpid;
        {
            std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
            candidates.reserve(dirty_checkpoint_candidates_.size() +
                               checkpoint_marker_candidates_.size());
            for (const auto &entry : dirty_checkpoint_candidates_)
            {
                if (entry.second != 0 && entry.second <= dirty_generation_boundary)
                {
                    candidates.push_back(entry);
                    seen_gpid.insert(entry.first);
                }
            }
            for (const GPID gpid : checkpoint_marker_candidates_)
            {
                if (seen_gpid.find(gpid) == seen_gpid.end())
                {
                    candidates.emplace_back(gpid, 0);
                    seen_gpid.insert(gpid);
                }
            }
        }

        for (const Frame &frame : frames_)
        {
            const GPID gpid = frame.gpid;
            if (gpid == INVALID_GPID || seen_gpid.find(gpid) != seen_gpid.end())
            {
                continue;
            }

            auto *header = reinterpret_cast<PageHeader *>(frame.data.get());
            if (header == nullptr || header->magic != K_MAGIC_SBRD || pageIsTemporaryWork(*header))
            {
                continue;
            }

            const bool needs_checkpoint_marker =
                header->flush_generation > header->checkpoint_generation;
            if (!needs_checkpoint_marker)
            {
                continue;
            }

            const DirtyState dirty_state = static_cast<DirtyState>(
                frame.dirty_state.load(std::memory_order_relaxed));
            if (dirty_state == DirtyState::DirtyInFlight)
            {
                continue;
            }

            candidates.emplace_back(gpid, 0);
            seen_gpid.insert(gpid);
        }

        for (const auto &[gpid, queued_generation] : candidates)
        {
            size_t partition_idx = getPartitionIndex(gpid);
            auto &partition = page_table_partitions_[partition_idx];
            uint32_t frame_index = UINT32_MAX;
            void *buffer = nullptr;
            {
                std::lock_guard<std::mutex> partition_lock(partition.mutex);
                auto it = partition.table.find(gpid);
                if (it != partition.table.end())
                {
                    frame_index = it->second;
                    frames_[frame_index].pin_count.fetch_add(1, std::memory_order_relaxed);
                    buffer = frames_[frame_index].data.get();
                }
            }

            if (frame_index == UINT32_MAX)
            {
                if (queued_generation == 0)
                {
                    Status republish_status = republishNonResidentCheckpointMarker(gpid);
                    if (republish_status != Status::OK)
                    {
                        return republish_status;
                    }

                    std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
                    auto it = dirty_checkpoint_candidates_.find(gpid);
                    if (it != dirty_checkpoint_candidates_.end() &&
                        it->second <= dirty_generation_boundary)
                    {
                        dirty_checkpoint_candidates_.erase(it);
                    }
                    checkpoint_marker_candidates_.erase(gpid);
                    continue;
                }

                std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
                auto it = dirty_checkpoint_candidates_.find(gpid);
                if (it != dirty_checkpoint_candidates_.end() &&
                    (queued_generation == 0 || it->second == queued_generation))
                {
                    dirty_checkpoint_candidates_.erase(it);
                }
                checkpoint_marker_candidates_.erase(gpid);
                continue;
            }

            Frame &frame = frames_[frame_index];
            if (buffer == nullptr)
            {
                (void)unpinPageGlobal(gpid, false, ctx);
                if (ctx != nullptr)
                {
                    ctx->code = Status::IO_ERROR;
                    ctx->message =
                        "Checkpoint dirty-set drain resolved a resident frame without a buffer";
                }
                return Status::IO_ERROR;
            }
            std::unique_lock<std::mutex> content_lock(*frame.content_mutex);

            uint64_t live_generation = 0;
            {
                std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
                auto it = dirty_checkpoint_candidates_.find(gpid);
                if (it != dirty_checkpoint_candidates_.end())
                {
                    live_generation = it->second;
                }
            }
            const bool checkpoint_only_candidate = queued_generation == 0;
            if (!checkpoint_only_candidate &&
                (live_generation == 0 || live_generation > dirty_generation_boundary))
            {
                frame.checkpoint_target_generation.store(0, std::memory_order_relaxed);
                frame.writeback_queue_state.store(
                    static_cast<uint8_t>(classifyWritebackQueueState(frame)),
                    std::memory_order_relaxed);
                (void)unpinPageGlobal(gpid, false, ctx);
                continue;
            }

            auto *header = reinterpret_cast<PageHeader *>(buffer);
            if (pageIsTemporaryWork(*header))
            {
                frame.checkpoint_target_generation.store(0, std::memory_order_relaxed);
                frame.writeback_queue_state.store(
                    static_cast<uint8_t>(classifyWritebackQueueState(frame)),
                    std::memory_order_relaxed);
                {
                    std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
                    checkpoint_marker_candidates_.erase(gpid);
                }
                (void)unpinPageGlobal(gpid, false, ctx);
                continue;
            }
            const bool dirty = frame.is_dirty.load(std::memory_order_acquire);
            const DirtyState dirty_state = static_cast<DirtyState>(
                frame.dirty_state.load(std::memory_order_relaxed));
            const bool needs_checkpoint_marker =
                header->magic == K_MAGIC_SBRD &&
                header->flush_generation > header->checkpoint_generation;
            if (dirty_state == DirtyState::DirtyFlushedPendingFsync &&
                frame.last_flush_generation.load(std::memory_order_relaxed) >= live_generation &&
                !needs_checkpoint_marker)
            {
                frame.checkpoint_target_generation.store(dirty_generation_boundary,
                                                         std::memory_order_relaxed);
                frame.writeback_queue_state.store(
                    static_cast<uint8_t>(WritebackQueueState::CHECKPOINT),
                    std::memory_order_relaxed);
                (void)unpinPageGlobal(gpid, false, ctx);
                continue;
            }
            if (dirty || needs_checkpoint_marker)
            {
                uint64_t flushed_generation =
                    frame.dirty_generation.load(std::memory_order_acquire);
                if (!dirty && needs_checkpoint_marker)
                {
                    (void)tryMarkFrameDirty(frame_index);
                    flushed_generation =
                        frame.dirty_generation.load(std::memory_order_acquire);
                    if (flushed_generation == 0)
                    {
                        flushed_generation = publishDirtyGeneration(frame_index);
                    }
                }
                beginFrameWriteback(frame_index,
                                    WritebackQueueState::CHECKPOINT,
                                    dirty_generation_boundary);
                Status status = writePageToDisk(frame_index,
                                                ctx,
                                                WritebackQueueState::CHECKPOINT,
                                                true);
                if (status != Status::OK)
                {
                    markFrameWritebackFailure(frame_index, WritebackQueueState::CHECKPOINT);
                    (void)unpinPageGlobal(gpid, false, ctx);
                    return status;
                }
                finishFrameWriteback(frame_index,
                                     flushed_generation,
                                     WritebackQueueState::CHECKPOINT);
                stats_.checkpoint_flushes.fetch_add(1, std::memory_order_relaxed);
            }

            {
                std::lock_guard<std::mutex> lock(dirty_tracking_mutex_);
                auto it = dirty_checkpoint_candidates_.find(gpid);
                if (it != dirty_checkpoint_candidates_.end() &&
                    it->second <= dirty_generation_boundary &&
                    (queued_generation == 0 || it->second == queued_generation))
                {
                    dirty_checkpoint_candidates_.erase(it);
                }
                if (header->magic != K_MAGIC_SBRD ||
                    pageIsTemporaryWork(*header) ||
                    header->flush_generation <= header->checkpoint_generation)
                {
                    checkpoint_marker_candidates_.erase(gpid);
                }
            }

            (void)unpinPageGlobal(gpid, false, ctx);
        }

        return Status::OK;
    }

    void BufferPool::quiesceBackgroundWriterForShutdown()
    {
        stopBackgroundWriter();
    }

    double BufferPool::calculateDirtyRatio() const
    {
        // CRITICAL: Caller must hold mutex_
        // Calculate the ratio of dirty pages to total pages

        uint32_t dirty_count = getDirtyPageCount();
        uint32_t total_pages = config_.pool_size;

        if (total_pages == 0)
        {
            return 0.0;
        }

        return static_cast<double>(dirty_count) / static_cast<double>(total_pages);
    }

    uint32_t BufferPool::getDirtyPageCount() const
    {
        // P2-2: O(1) dirty page count using atomic counter
        // No longer requires mutex or O(N) scan through frames
        return dirty_page_count_.load(std::memory_order_relaxed);
    }

    void BufferPool::updateDirtyTelemetry()
    {
        if (!metrics_ || !metrics_->buffer_pool_pages_dirty)
        {
            return;
        }
        metrics_->buffer_pool_pages_dirty->set(getDirtyPageCount());
    }

    void BufferPool::updatePoolTelemetry()
    {
        if (!metrics_)
        {
            return;
        }
        if (metrics_->buffer_pool_size_bytes)
        {
            metrics_->buffer_pool_size_bytes->set(
                static_cast<double>(config_.pool_size) * static_cast<double>(config_.page_size));
        }
        if (metrics_->buffer_pool_pages_total)
        {
            metrics_->buffer_pool_pages_total->set(config_.pool_size);
        }
    }

} // namespace scratchbird::core
