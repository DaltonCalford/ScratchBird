/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/tid.h"
#include <cstdint>
#include <vector>

namespace scratchbird::core
{
    enum class IndexGcLifecycleState : uint8_t
    {
        LOGICAL_DEAD_ROOT,
    };

    struct IndexGcCandidate
    {
        TID stable_root_tid = INVALID_TID;
        IndexGcLifecycleState lifecycle_state = IndexGcLifecycleState::LOGICAL_DEAD_ROOT;
    };

    struct IndexCleanupDebtSnapshot
    {
        uint64_t backlog_pages = 0;
        uint64_t backlog_bytes = 0;
        uint32_t first_locality_page_id = 0;
        bool repair_required = false;
    };

    /**
     * IndexGCInterface - Interface for index garbage collection
     *
     * This interface defines the contract between the garbage collector
     * and index implementations. When the heap maturity scan identifies
     * logical dead root versions, it calls the lifecycle-aware removal path
     * so indexes only act on stable root TIDs.
     *
     * ## Firebird MGA Design Pattern
     *
     * In Firebird MGA architecture:
     * 1. Indexes store stable TIDs pointing to primary tuple locations
     * 2. When tuples are deleted/updated, heap creates back versions
     * 3. Primary tuple location remains valid (TID never changes)
     * 4. Eventually, when all transactions can't see a deleted root version, the stable
     *    root becomes logically dead
     * 5. Heap maturity scan identifies stable root TIDs for those logical dead roots
     * 6. Index GC removes index entries pointing to those stable root TIDs
     *
     * ## Protocol
     *
     * 1. Heap maturity scan identifies logical dead root versions
     * 2. Sweep/GC collects stable root TIDs in lifecycle candidates
     * 3. For each index on the table, calls removeDeadEntriesWithLifecycle(candidates)
     * 4. Index removes entries pointing to those stable root TIDs
     * 5. Index returns statistics (entries removed, pages modified)
     *
     * ## Implementation Notes
     *
     * - Method must be safe to call with empty vector (no-op)
     * - Method should be atomic or handle partial failures gracefully
     * - Lifecycle candidates are guaranteed to be logical dead roots
     * - No need to re-verify tuple liveness - trust the shared maturity engine
     * - Performance: Bulk removal is more efficient than one-at-a-time
     *
     * ## Thread Safety
     *
     * - Implementation must handle concurrent index scans during GC
     * - Use appropriate locking (page-level or structure-specific)
     * - Dead entry removal is low priority, can yield to readers
     */
    class IndexGCInterface
    {
    public:
        virtual ~IndexGCInterface() = default;

        virtual Status removeDeadEntriesWithLifecycle(
            const std::vector<IndexGcCandidate> &dead_candidates,
            uint64_t *entries_removed_out = nullptr,
            uint64_t *pages_modified_out = nullptr,
            ErrorContext *ctx = nullptr)
        {
            std::vector<TID> dead_tids;
            dead_tids.reserve(dead_candidates.size());

            for (const auto &candidate : dead_candidates)
            {
                if (!candidate.stable_root_tid.isValid())
                {
                    SET_ERROR_CONTEXT(ctx,
                                      Status::INVALID_ARGUMENT,
                                      "Index GC candidate contains an invalid stable root TID");
                    return Status::INVALID_ARGUMENT;
                }

                if (candidate.lifecycle_state != IndexGcLifecycleState::LOGICAL_DEAD_ROOT)
                {
                    SET_ERROR_CONTEXT(
                        ctx,
                        Status::INVALID_ARGUMENT,
                        "Index GC candidate is not classified as a logical dead root");
                    return Status::INVALID_ARGUMENT;
                }

                dead_tids.push_back(candidate.stable_root_tid);
            }

            return removeDeadEntries(dead_tids, entries_removed_out, pages_modified_out, ctx);
        }

        /**
         * Remove index entries pointing to dead tuples
         *
         * Called by garbage collector after the lifecycle-aware path classifies
         * stable root versions as logical dead roots and collapses candidates to TIDs.
         * Implementation should remove all index entries that point to TIDs
         * in the dead_tids vector.
         *
         * PHASE 1.5 TASK 1.5.4: Migrated to TID struct API
         *
         * @param dead_tids Vector of stable root TIDs confirmed as logical dead roots
         * @param entries_removed_out [OUT] Number of index entries removed (optional)
         * @param pages_modified_out [OUT] Number of index pages modified (optional)
         * @param ctx Error context for diagnostics
         * @return Status::OK on success, error code on failure
         *
         * Error Handling:
         * - OK: All dead entries successfully removed
         * - PARTIAL_FAILURE: Some entries removed, but errors occurred (log warnings)
         * - IO_ERROR: Failed to access index pages
         * - INTERNAL_ERROR: Index structure corruption detected
         *
         * Thread Safety:
         * - Must be safe to call concurrently with index scans (readers)
         * - Should use appropriate locking to prevent corruption
         * - Can block or yield based on implementation strategy
         *
         * Performance:
         * - Bulk operations preferred over per-TID removal
         * - Can scan index once and remove all matching TIDs
         * - Should update statistics (entries_removed, pages_modified)
         */
        virtual Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                         uint64_t *entries_removed_out = nullptr,
                                         uint64_t *pages_modified_out = nullptr,
                                         ErrorContext *ctx = nullptr) = 0;

        virtual Status getCleanupDebtSnapshot(IndexCleanupDebtSnapshot *snapshot_out,
                                              ErrorContext *ctx = nullptr) const
        {
            (void)ctx;
            if (snapshot_out != nullptr)
            {
                *snapshot_out = IndexCleanupDebtSnapshot{};
            }
            return Status::OK;
        }

        /**
         * Get index type name (for logging/debugging)
         *
         * @return Human-readable index type name (e.g., "B-Tree", "Hash", "GIN")
         */
        virtual const char *indexTypeName() const = 0;
    };

    /**
     * GCStatistics - Statistics returned by index GC operations
     *
     * Used to track the effectiveness of garbage collection.
     */
    struct IndexGCStatistics
    {
        uint64_t entries_removed;   // Number of index entries removed
        uint64_t pages_modified;    // Number of index pages modified
        uint64_t pages_scanned;     // Number of index pages scanned
        uint64_t duration_ms;       // Time taken (milliseconds)

        IndexGCStatistics()
            : entries_removed(0), pages_modified(0), pages_scanned(0), duration_ms(0)
        {
        }
    };

} // namespace scratchbird::core
