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
// P3-11: TIP Compaction Implementation
//
// November 25, 2025

#include "scratchbird/core/tip_compaction.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include <algorithm>
#include <thread>

namespace scratchbird::core {

// ============================================================================
// TIPCompactor Implementation
// ============================================================================

TIPCompactor::TIPCompactor(Database* db, TransactionManager* txn_mgr)
    : db_(db), txn_mgr_(txn_mgr) {
}

TIPCompactor::~TIPCompactor() {
    stopBackgroundCompaction();
}

Status TIPCompactor::initialize(const TIPCompactionConfig& config, ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    return Status::OK;
}

bool TIPCompactor::needsCompaction() const {
    // Check if we have too many TIP pages
    // This would query the transaction manager for TIP page count
    // For now, return based on configuration threshold
    return false;  // Placeholder - would check actual TIP page count
}

Status TIPCompactor::compact(ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto start_time = std::chrono::high_resolution_clock::now();

    Status status = scanAndCompact(ctx);

    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.compaction_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();
    stats_.last_compaction_time = std::chrono::system_clock::now();

    return status;
}

Status TIPCompactor::scanAndCompact(ErrorContext* ctx) {
    // Get the oldest active XID - we can't compact anything >= this
    uint64_t oldest_active = getOldestActiveXid();

    std::vector<CompactedRange> new_ranges;
    CompactedRange current_range{0, 0, 0, 0};
    bool in_range = false;

    uint64_t entries_processed = 0;
    uint64_t total_entries = config_.max_entries_per_run;

    // Scan TIP entries looking for compactable ranges
    // This is a simplified implementation - actual implementation would
    // iterate through TIP pages

    // For each XID from oldest to oldest_active - 1
    uint64_t scan_start = stats_.last_compaction_xid > 0 ? stats_.last_compaction_xid : 1;
    uint64_t scan_end = oldest_active > config_.min_age_xids ?
                        oldest_active - config_.min_age_xids : scan_start;

    for (uint64_t xid = scan_start; xid < scan_end && entries_processed < config_.max_entries_per_run; ++xid) {
        // Get transaction state
        TransactionState state;
        Status status = txn_mgr_->getTransactionState(xid, state);
        if (status != Status::OK) {
            continue;  // Skip if we can't get state
        }

        uint8_t state_byte = static_cast<uint8_t>(state);

        // Only compact committed or aborted transactions
        if (state != TransactionState::COMMITTED && state != TransactionState::ABORTED) {
            // End current range if we hit an active/prepared transaction
            if (in_range && current_range.count > 0) {
                new_ranges.push_back(current_range);
                in_range = false;
            }
            continue;
        }

        if (!in_range) {
            // Start new range
            current_range = {xid, xid, state_byte, 1};
            in_range = true;
        } else if (current_range.state == state_byte) {
            // Extend current range
            current_range.end_xid = xid;
            current_range.count++;
        } else {
            // State changed, end current range and start new one
            new_ranges.push_back(current_range);
            current_range = {xid, xid, state_byte, 1};
        }

        entries_processed++;
        stats_.entries_compacted++;

        // Report progress
        if (progress_callback_ && entries_processed % 10000 == 0) {
            progress_callback_(entries_processed, total_entries);
        }
    }

    // Don't forget the last range
    if (in_range && current_range.count > 0) {
        new_ranges.push_back(current_range);
    }

    // Merge adjacent ranges with same state
    mergeAdjacentRanges(new_ranges);

    // Store compacted ranges
    {
        std::lock_guard<std::mutex> ranges_lock(ranges_mutex_);
        // Merge with existing ranges
        for (const auto& range : new_ranges) {
            compacted_ranges_.push_back(range);
        }
        // Sort and merge again
        std::sort(compacted_ranges_.begin(), compacted_ranges_.end(),
                  [](const CompactedRange& a, const CompactedRange& b) {
                      return a.start_xid < b.start_xid;
                  });
        mergeAdjacentRanges(compacted_ranges_);
    }

    stats_.last_compaction_xid = scan_end;
    stats_.pages_scanned++;  // Simplified - would track actual pages

    return Status::OK;
}

void TIPCompactor::mergeAdjacentRanges(std::vector<CompactedRange>& ranges) {
    if (ranges.size() < 2) return;

    std::vector<CompactedRange> merged;
    merged.push_back(ranges[0]);

    for (size_t i = 1; i < ranges.size(); ++i) {
        CompactedRange& last = merged.back();
        const CompactedRange& current = ranges[i];

        // Check if ranges are adjacent and have same state
        if (last.end_xid + 1 == current.start_xid && last.state == current.state) {
            // Merge
            last.end_xid = current.end_xid;
            last.count += current.count;
        } else {
            // Add new range
            merged.push_back(current);
        }
    }

    ranges = std::move(merged);
}

bool TIPCompactor::canCompactEntry(uint64_t xid, uint8_t state) const {
    // Can only compact committed or aborted transactions
    if (state != static_cast<uint8_t>(TransactionState::COMMITTED) &&
        state != static_cast<uint8_t>(TransactionState::ABORTED)) {
        return false;
    }

    // Check age requirements
    uint64_t oldest_active = getOldestActiveXid();
    if (xid >= oldest_active) {
        return false;  // Still potentially visible
    }

    // Check minimum age
    if (oldest_active - xid < config_.min_age_xids) {
        return false;
    }

    return true;
}

uint64_t TIPCompactor::getOldestActiveXid() const {
    // This would query the transaction manager for the oldest active XID
    // Placeholder implementation
    return 0;
}

bool TIPCompactor::lookupCompacted(uint64_t xid, uint8_t* state_out) const {
    std::lock_guard<std::mutex> lock(ranges_mutex_);

    // Binary search for the range containing this XID
    auto it = std::lower_bound(compacted_ranges_.begin(), compacted_ranges_.end(), xid,
                               [](const CompactedRange& range, uint64_t x) {
                                   return range.end_xid < x;
                               });

    if (it != compacted_ranges_.end() && it->contains(xid)) {
        if (state_out) {
            *state_out = it->state;
        }
        return true;
    }

    return false;
}

void TIPCompactor::setConfig(const TIPCompactionConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

Status TIPCompactor::startBackgroundCompaction(ErrorContext* ctx) {
    if (background_running_) {
        return Status::OK;  // Already running
    }

    shutdown_requested_ = false;
    background_running_ = true;

    std::thread([this]() {
        backgroundCompactionLoop();
    }).detach();

    return Status::OK;
}

void TIPCompactor::stopBackgroundCompaction() {
    shutdown_requested_ = true;
    background_running_ = false;
}

void TIPCompactor::backgroundCompactionLoop() {
    while (!shutdown_requested_) {
        // Sleep for configured interval
        std::this_thread::sleep_for(config_.background_interval);

        if (shutdown_requested_) break;

        // Check if compaction is needed
        if (needsCompaction()) {
            ErrorContext ctx;
            compact(&ctx);
        }
    }

    background_running_ = false;
}

void TIPCompactor::setProgressCallback(ProgressCallback callback) {
    progress_callback_ = std::move(callback);
}

Status TIPCompactor::compactPage(uint32_t page_id, ErrorContext* ctx) {
    // This would compact a specific TIP page
    // Implementation would read the page, compress entries, write back
    stats_.pages_compacted++;
    return Status::OK;
}

Status TIPCompactor::writeCompactedPage(const std::vector<CompactedRange>& ranges,
                                        uint64_t min_xid, uint64_t max_xid,
                                        uint32_t* page_id_out, ErrorContext* ctx) {
    // This would write compacted ranges to a new page format
    // Implementation would allocate page, write header and ranges
    return Status::OK;
}

// ============================================================================
// TIPCompactionManager Implementation
// ============================================================================

TIPCompactionManager& TIPCompactionManager::getInstance() {
    static TIPCompactionManager instance;
    return instance;
}

void TIPCompactionManager::registerCompactor(Database* db, std::shared_ptr<TIPCompactor> compactor) {
    std::lock_guard<std::mutex> lock(mutex_);
    compactors_[db] = std::move(compactor);
}

void TIPCompactionManager::unregisterCompactor(Database* db) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = compactors_.find(db);
    if (it != compactors_.end()) {
        it->second->stopBackgroundCompaction();
        compactors_.erase(it);
    }
}

std::shared_ptr<TIPCompactor> TIPCompactionManager::getCompactor(Database* db) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = compactors_.find(db);
    return it != compactors_.end() ? it->second : nullptr;
}

void TIPCompactionManager::compactAll(ErrorContext* ctx) {
    std::vector<std::shared_ptr<TIPCompactor>> compactors_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [db, compactor] : compactors_) {
            compactors_copy.push_back(compactor);
        }
    }

    for (auto& compactor : compactors_copy) {
        compactor->compact(ctx);
    }
}

TIPCompactionStats TIPCompactionManager::aggregateStats() const {
    TIPCompactionStats aggregate;

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [db, compactor] : compactors_) {
        const auto& stats = compactor->stats();
        aggregate.pages_scanned += stats.pages_scanned;
        aggregate.pages_compacted += stats.pages_compacted;
        aggregate.pages_freed += stats.pages_freed;
        aggregate.entries_compacted += stats.entries_compacted;
        aggregate.bytes_saved += stats.bytes_saved;
        aggregate.compaction_time_us += stats.compaction_time_us;
    }

    return aggregate;
}

} // namespace scratchbird::core
