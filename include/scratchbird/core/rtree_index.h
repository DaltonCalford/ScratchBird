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
            // Bounding box for spatial queries (2D for now)
            struct BoundingBox
            {
                double min_x, min_y;
                double max_x, max_y;

                BoundingBox() : min_x(0), min_y(0), max_x(0), max_y(0) {}

                BoundingBox(double minx, double miny, double maxx, double maxy)
                    : min_x(minx), min_y(miny), max_x(maxx), max_y(maxy) {}

                // Calculate area
                double area() const { return (max_x - min_x) * (max_y - min_y); }

                // Check if boxes overlap
                bool overlaps(const BoundingBox& other) const {
                    return !(max_x < other.min_x || min_x > other.max_x ||
                             max_y < other.min_y || min_y > other.max_y);
                }

                // Check if this box contains another
                bool contains(const BoundingBox& other) const {
                    return min_x <= other.min_x && max_x >= other.max_x &&
                           min_y <= other.min_y && max_y >= other.max_y;
                }

                // Compute minimum bounding box that contains both
                BoundingBox union_box(const BoundingBox& other) const {
                    return BoundingBox(
                        std::min(min_x, other.min_x),
                        std::min(min_y, other.min_y),
                        std::max(max_x, other.max_x),
                        std::max(max_y, other.max_y)
                    );
                }

                // Calculate area increase if other box is added
                double area_increase(const BoundingBox& other) const {
                    BoundingBox combined = union_box(other);
                    return combined.area() - area();
                }
            };

            // Entry in R-Tree (leaf or internal node)
            struct RTreeEntry
            {
                BoundingBox bbox;
                uint64_t child_page;  // For internal nodes
                TID tid;              // For leaf nodes
                uint64_t xmin;        // MGA: transaction that created this entry
                uint64_t xmax;        // MGA: transaction that deleted this entry (0 if active)
                bool is_leaf;
            };

            Database *db_;
            UuidV7Bytes index_uuid_;
            uint32_t meta_page_;
            uint32_t root_page_;

            // R-Tree parameters
            static constexpr uint32_t MAX_ENTRIES = 50;  // M
            static constexpr uint32_t MIN_ENTRIES = 20;  // m (40% fill)

            // Helper methods
            Status deserializeBoundingBox(const std::vector<uint8_t>& key, BoundingBox* bbox, ErrorContext* ctx);
            std::vector<uint8_t> serializeBoundingBox(const BoundingBox& bbox);

            Status chooseLeaf(const BoundingBox& bbox, uint32_t* leaf_page, ErrorContext* ctx);
            Status chooseSubtree(uint32_t node_page, const BoundingBox& bbox, uint32_t* child_page, ErrorContext* ctx);
            Status splitNode(uint32_t page_num, RTreeEntry* new_entry, uint32_t* new_sibling, BoundingBox* separator_bbox, ErrorContext* ctx);
            Status adjustTree(uint32_t leaf_page, uint32_t* split_page, BoundingBox* split_bbox, ErrorContext* ctx);
            Status insertEntry(uint32_t page_num, const RTreeEntry& entry, ErrorContext* ctx);
            Status searchNode(uint32_t page_num, const BoundingBox& query, uint64_t current_xid, std::vector<TID>* results, ErrorContext* ctx);
        };

    } // namespace core
} // namespace scratchbird
