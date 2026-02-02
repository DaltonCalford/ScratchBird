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
    /**
     * IndexGCInterface - Interface for index garbage collection
     *
     * This interface defines the contract between the garbage collector
     * and index implementations. When the heap sweep identifies dead tuples,
     * it calls removeDeadEntries() on each index to clean up references.
     *
     * ## Firebird MGA Design Pattern
     *
     * In Firebird MGA architecture:
     * 1. Indexes store stable TIDs pointing to primary tuple locations
     * 2. When tuples are deleted/updated, heap creates back versions
     * 3. Primary tuple location remains valid (TID never changes)
     * 4. Eventually, when all transactions can't see old versions, tuples become "dead"
     * 5. Heap sweep identifies dead TIDs (those with xmax < OIT)
     * 6. Index GC removes index entries pointing to dead TIDs
     *
     * ## Protocol
     *
     * 1. Heap sweep (GarbageCollector::cleanPage) identifies dead tuples via OIT
     * 2. Sweep collects dead TIDs in a vector
     * 3. For each index on the table, calls removeDeadEntries(dead_tids)
     * 4. Index removes entries pointing to dead TIDs
     * 5. Index returns statistics (entries removed, pages modified)
     *
     * ## Implementation Notes
     *
     * - Method must be safe to call with empty vector (no-op)
     * - Method should be atomic or handle partial failures gracefully
     * - TIDs in the vector are guaranteed to be truly dead (OIT-based)
     * - No need to re-verify tuple liveness - trust the OIT
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

        /**
         * Remove index entries pointing to dead tuples
         *
         * Called by garbage collector after heap sweep identifies dead tuples.
         * Implementation should remove all index entries that point to TIDs
         * in the dead_tids vector.
         *
         * PHASE 1.5 TASK 1.5.4: Migrated to TID struct API
         *
         * @param dead_tids Vector of TIDs that have been confirmed dead by OIT check
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
