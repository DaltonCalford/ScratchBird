#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include <cstdint>
#include <vector>
#include <memory>

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
            RTreeIndex(Database *db, const UuidV7Bytes &index_uuid, uint32_t meta_page);

            // Create a new R-Tree index
            static Status create(Database *db, const UuidV7Bytes &index_uuid,
                                 uint32_t *meta_page_out, ErrorContext *ctx = nullptr);

            // Open an existing R-Tree index
            static std::unique_ptr<RTreeIndex> open(Database *db, const UuidV7Bytes &index_uuid,
                                                    uint32_t meta_page, ErrorContext *ctx = nullptr);

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

            // Vacuum the index
            Status vacuum(ErrorContext *ctx = nullptr);

            // IndexGCInterface implementation
            Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

            const char *indexTypeName() const override { return "RTree"; }

            // Get index metadata
            const UuidV7Bytes &getIndexUuid() const { return index_uuid_; }
            uint32_t getMetaPage() const { return meta_page_; }

        private:
            Database *db_;
            UuidV7Bytes index_uuid_;
            uint32_t meta_page_;
        };

    } // namespace core
} // namespace scratchbird
