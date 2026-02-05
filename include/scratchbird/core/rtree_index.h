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

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/rtree.h"
#include "scratchbird/core/gpid.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <algorithm>

namespace scratchbird
{
    namespace core
    {
        // Forward declarations
        class Database;

        // R-Tree Index - Spatial index for geometric data
        // Implements R-Tree algorithm for efficient spatial queries
        // November 19, 2025
        class RTreeIndex : public IndexGCInterface
        {
        public:
            // Constructor
            RTreeIndex(Database *db, const UuidV7Bytes &index_uuid, GPID meta_gpid);

            // Create a new R-Tree index
            static Status create(Database *db, const UuidV7Bytes &index_uuid,
                                 GPID meta_gpid, ErrorContext *ctx = nullptr);

            // Open an existing R-Tree index
            static std::unique_ptr<RTreeIndex> open(Database *db, const UuidV7Bytes &index_uuid,
                                                    GPID meta_gpid, ErrorContext *ctx = nullptr);

            // Destructor
            ~RTreeIndex();

            // Insert a spatial key (bounding box) with TID
            Status insert(const std::vector<uint8_t> &key, const TID &tid,
                          uint64_t xmin, ErrorContext *ctx = nullptr);

            // Search for overlapping entries
            Status search(const std::vector<uint8_t> &query_box, uint64_t current_xid,
                          std::vector<TID> *results_out, ErrorContext *ctx = nullptr);

            // Remove entry (MGA logical deletion)
            Status remove(const std::vector<uint8_t> &key, const TID &tid,
                          uint64_t xmax, ErrorContext *ctx = nullptr);

            // GC compaction (ScratchBird MGA GC, not PostgreSQL VACUUM)
            Status gcCompact(ErrorContext *ctx = nullptr);

            // IndexGCInterface implementation
            Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

            const char *indexTypeName() const override { return "RTree"; }

            // Get index metadata
            const UuidV7Bytes &getIndexUuid() const { return index_uuid_; }
            uint32_t getMetaPage() const { return static_cast<uint32_t>(getPageNumber(meta_gpid_)); }

        private:
            Database *db_;
            UuidV7Bytes index_uuid_;
            GPID meta_gpid_;
            uint32_t root_page_;

            // Delegate to the real RTree implementation
            std::unique_ptr<RTree> rtree_;

            // R-Tree parameters
            static constexpr uint32_t MAX_ENTRIES = 50;  // M
            static constexpr uint32_t MIN_ENTRIES = 20;  // m (40% fill)

            // Helper methods
            Status deserializeBoundingBox(const std::vector<uint8_t>& key, scratchbird::core::BoundingBox* bbox, ErrorContext* ctx);
            std::vector<uint8_t> serializeBoundingBox(const scratchbird::core::BoundingBox& bbox);
        };

    } // namespace core
} // namespace scratchbird
