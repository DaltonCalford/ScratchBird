/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-11: TIP Compaction
//
// Consolidates old committed/aborted transactions in Transaction Inventory Pages
// to prevent unbounded TIP growth and reclaim space.
//
// Strategy:
// - Periodically scan TIP pages for compaction candidates
// - Compress ranges of identical transaction states (all committed/aborted)
// - Free unused TIP pages when possible
// - Maintain minimum retention period for active visibility checks
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>

namespace scratchbird::core {

// Forward declarations
class Database;
class TransactionManager;
class BufferPool;

// TIP compaction statistics
struct TIPCompactionStats {
    uint64_t pages_scanned = 0;           // Total pages scanned
    uint64_t pages_compacted = 0;         // Pages successfully compacted
    uint64_t pages_freed = 0;             // Pages returned to free list
    uint64_t entries_compacted = 0;       // Individual entries consolidated
    uint64_t bytes_saved = 0;             // Bytes saved by compaction
    uint64_t compaction_time_us = 0;      // Total compaction time
    uint64_t last_compaction_xid = 0;     // XID of last compaction run
    std::chrono::system_clock::time_point last_compaction_time;

    // Efficiency metrics
    double compressionRatio() const {
        return pages_scanned > 0 ?
               static_cast<double>(pages_freed) / pages_scanned : 0.0;
    }
};

// Compaction configuration
struct TIPCompactionConfig {
    // Minimum age (in transactions) before an entry is compactable
    uint64_t min_age_xids = 1000000;  // 1 million transactions

    // Minimum age (in time) before an entry is compactable
    std::chrono::seconds min_age_time{3600};  // 1 hour

    // Maximum entries to scan per compaction run
    uint64_t max_entries_per_run = 100000;

    // Trigger compaction when TIP pages exceed this count
    uint32_t trigger_page_count = 1000;

    // Target ratio of active entries per page (trigger compaction below this)
    double target_density = 0.5;  // 50% utilization

    // Enable/disable background compaction
    bool enable_background = true;

    // Background compaction interval
    std::chrono::seconds background_interval{60};  // 1 minute
};

// Compacted TIP entry range
struct CompactedRange {
    uint64_t start_xid;
    uint64_t end_xid;
    uint8_t state;  // TransactionState - all entries in range have this state
    uint32_t count;

    bool contains(uint64_t xid) const {
        return xid >= start_xid && xid <= end_xid;
    }
};

// Compacted TIP page format
#pragma pack(push, 1)
struct CompactedTIPPageHeader {
    uint32_t magic;                 // Magic number
    uint32_t version;               // Format version
    uint64_t min_xid;               // Minimum XID in page
    uint64_t max_xid;               // Maximum XID in page
    uint32_t num_ranges;            // Number of compacted ranges
    uint32_t original_entries;      // Original entry count before compaction
    uint64_t compaction_time;       // Timestamp of compaction
    uint32_t next_page;             // Next compacted page (or 0)
    uint8_t reserved[20];

    static constexpr uint32_t MAGIC = 0x54495043;  // "TIPC"
    static constexpr uint32_t VERSION = 1;
};
#pragma pack(pop)

// TIP compactor class
class TIPCompactor {
public:
    TIPCompactor(Database* db, TransactionManager* txn_mgr);
    ~TIPCompactor();

    // Initialize compactor
    Status initialize(const TIPCompactionConfig& config, ErrorContext* ctx = nullptr);

    // Run compaction (manual trigger)
    Status compact(ErrorContext* ctx = nullptr);

    // Check if compaction is needed
    bool needsCompaction() const;

    // Get current statistics
    const TIPCompactionStats& stats() const { return stats_; }

    // Get configuration
    const TIPCompactionConfig& config() const { return config_; }

    // Update configuration
    void setConfig(const TIPCompactionConfig& config);

    // Start background compaction thread
    Status startBackgroundCompaction(ErrorContext* ctx = nullptr);

    // Stop background compaction thread
    void stopBackgroundCompaction();

    // Check if background compaction is running
    bool isBackgroundRunning() const { return background_running_; }

    // Set progress callback
    using ProgressCallback = std::function<void(uint64_t entries_processed, uint64_t total_entries)>;
    void setProgressCallback(ProgressCallback callback);

    // Lookup XID in compacted data (returns true if found, sets state)
    bool lookupCompacted(uint64_t xid, uint8_t* state_out) const;

    // Get oldest active XID (transactions still visible to someone)
    uint64_t getOldestActiveXid() const;

private:
    Database* db_;
    TransactionManager* txn_mgr_;
    TIPCompactionConfig config_;
    TIPCompactionStats stats_;
    mutable std::mutex mutex_;

    // Background compaction
    std::atomic<bool> background_running_{false};
    std::atomic<bool> shutdown_requested_{false};

    // Compacted ranges cache
    std::vector<CompactedRange> compacted_ranges_;
    mutable std::mutex ranges_mutex_;

    // Progress callback
    ProgressCallback progress_callback_;

    // Internal methods
    Status scanAndCompact(ErrorContext* ctx);
    Status compactPage(uint32_t page_id, ErrorContext* ctx);
    Status writeCompactedPage(const std::vector<CompactedRange>& ranges,
                              uint64_t min_xid, uint64_t max_xid,
                              uint32_t* page_id_out, ErrorContext* ctx);
    bool canCompactEntry(uint64_t xid, uint8_t state) const;
    void backgroundCompactionLoop();
    void mergeAdjacentRanges(std::vector<CompactedRange>& ranges);
};

// TIP compaction manager (singleton)
class TIPCompactionManager {
public:
    static TIPCompactionManager& getInstance();

    // Register a compactor for a database
    void registerCompactor(Database* db, std::shared_ptr<TIPCompactor> compactor);

    // Unregister compactor
    void unregisterCompactor(Database* db);

    // Get compactor for database
    std::shared_ptr<TIPCompactor> getCompactor(Database* db);

    // Trigger compaction for all databases
    void compactAll(ErrorContext* ctx = nullptr);

    // Get aggregate statistics
    TIPCompactionStats aggregateStats() const;

private:
    TIPCompactionManager() = default;

    mutable std::mutex mutex_;
    std::unordered_map<Database*, std::shared_ptr<TIPCompactor>> compactors_;
};

} // namespace scratchbird::core
