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
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/vector.h"
#include "scratchbird/core/gpid.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <random>

namespace scratchbird
{
    namespace core
    {

        // Forward declarations
        class Database;
        class BufferPool;
        class PageManager;
        class TransactionManager; // For TIP-based visibility checks (Firebird MGA)
        struct ErrorContext;

        /**
         * HNSW (Hierarchical Navigable Small World) Index - Vector similarity search
         *
         * HNSW provides efficient approximate nearest neighbor search for high-dimensional
         * vectors. It builds a hierarchical graph structure with multiple layers, where
         * higher layers provide coarse navigation and lower layers provide refinement.
         *
         * ## Use Cases
         * - Semantic search (text embeddings from OpenAI, Cohere, etc.)
         * - Image similarity search
         * - Recommendation systems
         * - Anomaly detection
         * - Document clustering
         *
         * ## Architecture (Firebird MGA with Stable TIDs)
         *
         * ```
         * Layer 2 (Top):    [Entry] -----> [Node A]
         *                      |              |
         * Layer 1:         [Node B] <---> [Node C]
         *                      |              |
         * Layer 0 (Base):  [Node D] <---> [Node E] <---> [Node F]
         *                      ↓              ↓              ↓
         *                    TID:1          TID:2          TID:3
         *                   xmin=10        xmin=11        xmin=12
         *                   xmax=0         xmax=0         xmax=15 (deleted)
         * ```
         *
         * ## MGA Compliance (Phase 6 - November 2025)
         *
         * - **xmin/xmax tracking**: Each node has xmin/xmax
         * - **TIP-based visibility**: TransactionId parameter in search/insert APIs (NOT snapshots)
         * - **Visibility filtering**: During graph traversal, skip deleted nodes using TIP
         * - **Garbage collection**: removeDeadEntries() for dead node removal
         * - **Stable TIDs**: Nodes reference stable tuple IDs (heap TIDs)
         *
         * ## Implementation Notes
         *
         * - Multi-layer graph (typical: 4-6 layers)
         * - Bi-directional links between nodes
         * - M parameter: Max connections per node (default: 16)
         * - ef_construction: Expansion factor during build (default: 200)
         * - ef_search: Expansion factor during search (default: 100)
         * - Distance metric: Configurable (EUCLIDEAN, COSINE, etc.)
         * - Recall@10: Target 95%+ for production workloads
         */

        // HNSW page flags
        enum class HnswFlags : uint16_t
        {
            ROOT = 0x0001,        // Root/entry point page
            HAS_GARBAGE = 0x0002, // Page has deleted nodes
            COMPRESSED = 0x0004   // Vectors compressed (future)
        };

        // HNSW node flags
        enum class HnswNodeFlags : uint16_t
        {
            DELETED = 0x0001,    // Logically deleted (xmax set)
            ENTRY_POINT = 0x0002 // Entry point for layer
        };

#pragma pack(push, 1)
        /**
         * HNSW page structure
         *
         * Stores HNSW graph nodes with their connections.
         * Each node represents a vector with links to its neighbors.
         */
        struct SBHnswPage
        {
            // Standard page header
            PageHeader hnsw_header; // Standard ScratchBird page header

            // Index identification
            ID hnsw_index_uuid; // Index UUID v7
            ID hnsw_table_uuid; // Table this index belongs to

            // HNSW metadata
            uint16_t hnsw_flags;      // Page flags (see HnswFlags)
            uint16_t hnsw_count;      // Number of nodes on page
            uint16_t hnsw_free_space; // Free space in bytes
            uint16_t hnsw_layer;      // Layer this page belongs to
            uint32_t hnsw_m;          // Max connections per node
            uint32_t hnsw_dimensions; // Vector dimensions

            // Sibling navigation
            uint64_t hnsw_left_sibling;  // Left sibling page number
            uint64_t hnsw_right_sibling; // Right sibling page number

            // MGA compliance (Phase 4A.2)
            uint64_t hnsw_xmin; // Page creation transaction
            uint64_t hnsw_xmax; // Page deletion transaction (0 if active)
            uint64_t hnsw_lsn;  // Last LSN that modified this page

            // Statistics
            uint64_t hnsw_total_nodes;   // Total nodes in entire index
            uint64_t hnsw_deleted_nodes; // Deleted nodes (need GC compaction)

            uint8_t hnsw_distance_metric; // DistanceMetric enum value
            uint8_t hnsw_padding[63];     // Reserved for future use

            // Nodes follow immediately after header
            // Each node is a SBHnswNode structure
        };

        /**
         * HNSW node structure
         *
         * Represents a single vector in the graph with its connections.
         * Variable-size structure (neighbors and vector data follow).
         */
        struct SBHnswNode
        {
            // PHASE 1.5 TASK 1.5.2b: Migrated to GPID format for custom tablespace support
            GPID node_gpid;            // Heap GPID (8 bytes)
            uint16_t node_slot;        // Heap slot (2 bytes)
            uint16_t node_flags;       // Node flags (see HnswNodeFlags)
            uint16_t node_layer;       // Highest layer this node appears in
            uint16_t node_num_neighbors; // Number of neighbors
            uint16_t node_vector_len;  // Length of vector data in bytes

            // MGA compliance (Phase 4A.2)
            uint64_t node_xmin; // Transaction that created this node
            uint64_t node_xmax; // Transaction that deleted this node (0 if active)

            // Helper to get/set TID
            TID getTID() const { return TID(node_gpid, node_slot); }
            void setTID(const TID &tid) { node_gpid = tid.gpid; node_slot = tid.slot; }

            // Variable-length data follows:
            // - HnswNeighbor neighbors[node_num_neighbors]  // Neighbor TIDs (GPID + slot)
            // - uint8_t vector_data[node_vector_len]    // Encoded vector
            //
            // Access via:
            //   const HnswNeighbor* get_neighbors() const { return (HnswNeighbor*)(this + 1); }
            //   const uint8_t* get_vector_data() const {
            //       return (uint8_t*)(get_neighbors() + node_num_neighbors);
            //   }
        };

        struct HnswNeighbor
        {
            GPID neighbor_gpid;
            uint16_t neighbor_slot;
            uint8_t neighbor_padding[6];

            TID getTID() const { return TID(neighbor_gpid, neighbor_slot); }
            void setTID(const TID &tid) { neighbor_gpid = tid.gpid; neighbor_slot = tid.slot; }
        };

        static_assert(sizeof(HnswNeighbor) == 16, "HnswNeighbor must be 16 bytes");

#pragma pack(pop)

        /**
         * HNSW index metadata (stored in catalog)
         */
        struct SBHnswIndex
        {
            ID idx_uuid;                        // Index UUID v7
            ID idx_table_uuid;                  // Table UUID
            std::vector<ID> idx_column_uuids;   // Indexed columns (usually 1)
            uint32_t idx_root_page;             // Root page number
            uint16_t idx_tablespace_id = 0;
            uint32_t idx_m;                     // Max connections per node (default 16)
            uint32_t idx_ef_construction;       // Build expansion factor (default 200)
            uint32_t idx_ef_search;             // Search expansion factor (default 100)
            uint32_t idx_dimensions;            // Vector dimensions
            uint64_t idx_total_nodes;           // Total number of nodes
            uint64_t idx_creation_xid;          // Transaction that created index
            uint8_t idx_distance_metric;        // DistanceMetric enum value
            uint8_t idx_vector_type;            // VectorType enum value
        };

        /**
         * HNSW search result
         * PHASE 1.5 TASK 1.5.2d: Migrated to TID struct API
         */
        struct HnswSearchResult
        {
            TID tid;          // Heap TID
            double distance;  // Distance/similarity score

            bool operator<(const HnswSearchResult &other) const
            {
                return distance < other.distance; // Min-heap order
            }
        };

        /**
         * HNSW Index Implementation
         *
         * Implements IndexGCInterface for garbage collection integration.
         * Follows B-Tree MGA pattern for consistency.
         */
        class HnswIndex : public IndexGCInterface
        {
        public:
            HnswIndex(Database *db, SBHnswIndex index_info);
            ~HnswIndex();

            // Static factory methods
            static Status create(Database *db,
                                 const UuidV7Bytes &index_uuid,
                                 const UuidV7Bytes &table_uuid,
                                 const std::vector<UuidV7Bytes> &column_uuids,
                                 uint32_t dimensions,
                                 DistanceMetric distance_metric = DistanceMetric::EUCLIDEAN,
                                 uint32_t m = 16,                     // Max connections
                                 uint32_t ef_construction = 200,      // Build expansion
                                 uint32_t ef_search = 100,            // Search expansion
                                 GPID root_gpid = 0,
                                 ErrorContext *ctx = nullptr);

            static std::unique_ptr<HnswIndex> open(Database *db,
                                                   const UuidV7Bytes &index_uuid,
                                                   GPID root_gpid,
                                                   ErrorContext *ctx = nullptr);

            /**
             * Insert vector into HNSW graph
             *
             * Phase 4A.2.2: Graph insertion algorithm
             * - Selects layer for new node (probabilistic)
             * - Finds neighbors using greedy search
             * - Creates bi-directional links
             * - New nodes get xmin (current transaction)
             *
             * PHASE 1.5 TASK 1.5.2d: Migrated to TID struct API
             *
             * @param vector The vector to insert (encoded as bytes)
             * @param tid The heap TID for this vector
             * @param ctx Error context
             */
            Status insert(const VectorValue &vector,
                          const TID &tid,
                          ErrorContext *ctx = nullptr);

            /**
             * Delete vector from HNSW graph (soft deletion)
             *
             * Phase 4A.2.3: Graph deletion algorithm
             * - Marks node as deleted (sets xmax)
             * - Keeps links intact (needed for older snapshots)
             * - Actual removal done during GC compaction via removeDeadEntries()
             *
             * PHASE 1.5 TASK 1.5.2d: Migrated to TID struct API
             *
             * @param tid The heap TID to delete
             * @param ctx Error context
             */
            Status remove(const TID &tid,
                          ErrorContext *ctx = nullptr);

            /**
             * KNN search: Find k nearest neighbors
             *
             * Firebird MGA: KNN search with TIP-based visibility filtering
             * - Greedy search from top layer down
             * - Beam search for accuracy
             * - Uses TIP-based visibility filtering (NOT snapshots)
             * - Returns k nearest neighbors sorted by distance
             *
             * @param query_vector Query vector
             * @param k Number of neighbors to return
             * @param current_xid Current transaction ID for TIP-based visibility
             * @param results_out Output: k nearest neighbors with distances
             * @param ctx Error context
             * @return Status OK if successful
             *
             * Example:
             *   SELECT id, embedding <-> '[0.1, 0.2, ...]' AS distance
             *   FROM documents
             *   ORDER BY distance LIMIT 10
             *
             * Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
             */
            Status search(const VectorValue &query_vector,
                          uint32_t k,
                          uint64_t current_xid,
                          std::vector<HnswSearchResult> *results_out,
                          ErrorContext *ctx = nullptr);

            /**
             * GC compaction operations (ScratchBird MGA GC, not PostgreSQL VACUUM)
             */
            struct GcCompactionStats
            {
                uint64_t nodes_visited;
                uint64_t nodes_removed;
                uint64_t links_updated;
                uint64_t bytes_reclaimed;
            };

            Status gcCompact(GcCompactionStats *stats_out = nullptr,
                             ErrorContext *ctx = nullptr);

            /**
             * Phase 4A.2.5: Remove dead nodes
             * PHASE 1.5 TASK 1.5.2d: Migrated to TID struct API
             *
             * Called by garbage collector after heap sweep identifies dead tuples.
             * Removes nodes and updates graph links.
             *
             * @param dead_tids Tuple IDs that are dead
             * @param entries_removed_out Output: number of nodes removed
             * @param pages_modified_out Output: number of pages modified
             * @param ctx Error context
             * @return Status OK if successful
             */
            Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

            /**
             * Get index type name for logging
             */
            const char *indexTypeName() const override
            {
                return "HNSW";
            }

            /**
             * Get statistics
             */
            struct HnswStats
            {
                uint64_t total_nodes;
                uint64_t deleted_nodes;
                uint64_t total_pages;
                uint32_t max_layer;
                double avg_connections; // Average connections per node
                double avg_path_length; // Average search path length
            };

            Status getStats(HnswStats *stats_out, ErrorContext *ctx = nullptr);

            /**
             * PHASE 5 TASK 5.3.2: Update TIDs after tablespace migration
             *
             * Traverses all layers of the HNSW graph and updates node_tuple_id fields
             * based on the provided TID mapping.
             *
             * @param tid_mapping Map from old TID (uint64_t) to new TID (uint64_t)
             * @param tids_updated_out Output: number of TIDs updated
             * @param pages_modified_out Output: number of pages modified
             * @param ctx Error context
             * @return Status::OK if successful
             */
            Status updateTIDsAfterMigration(
                const std::unordered_map<TID, TID> &tid_mapping,
                uint64_t *tids_updated_out = nullptr,
                uint64_t *pages_modified_out = nullptr,
                ErrorContext *ctx = nullptr);

        private:
            Database *db_;
            SBHnswIndex index_info_;

            // Helper methods

            /**
             * Select layer for new node (probabilistic)
             * Higher layers have exponentially fewer nodes
             */
            uint16_t select_layer();

            /**
             * Find nearest neighbors using greedy search
             */
            Status find_nearest(const VectorValue &query,
                                uint32_t k,
                                uint16_t layer,
                                const TID &entry_point,
                                uint64_t current_xid,
                                std::vector<HnswSearchResult> *results_out,
                                ErrorContext *ctx);

            /**
             * Compute distance between two vectors
             */
            double compute_distance(const VectorValue &a, const VectorValue &b) const;

            /**
             * Check if node is visible to current transaction
             * Firebird MGA: Uses TIP-based visibility checking (NOT snapshots)
             */
            bool is_node_visible(const SBHnswNode *node,
                                 uint64_t current_xid,
                                 ErrorContext *ctx) const;

            /**
             * Add bi-directional link between nodes
             */
            Status add_link(const TID &from_tid, const TID &to_tid,
                            uint16_t layer, ErrorContext *ctx);

            /**
             * Remove link between nodes
             */
            Status remove_link(const TID &from_tid, const TID &to_tid,
                               uint16_t layer, ErrorContext *ctx);

            /**
             * Find node by tuple ID
             */
            Status find_node(const TID &tuple_id,
                             SBHnswNode **node_out,
                             uint64_t *page_num_out,
                             ErrorContext *ctx);

            /**
             * Prune connections to maintain M limit
             */
            Status prune_connections(const TID &node_tid, uint16_t layer,
                                     ErrorContext *ctx);

            /**
             * Create a new node in the graph
             */
            Status create_node(const VectorValue &vector,
                              const TID &tuple_id,
                              uint16_t layer,
                              const std::vector<TID> &neighbors,
                              uint64_t current_xid,
                              ErrorContext *ctx);

            /**
             * Find entry point (node with highest layer)
             */
            TID find_entry_point(ErrorContext *ctx) const;

            /**
             * Get maximum layer in the index
             */
            uint16_t get_max_layer() const;

            /**
             * Calculate the size of a node in bytes
             */
            size_t calculate_node_size(const SBHnswNode *node) const;
            size_t calculate_node_size(uint16_t num_neighbors, uint16_t vector_len) const;

            /**
             * Get vector data from a node
             */
            Status get_node_vector(const TID &tuple_id,
                                   VectorValue *vector_out,
                                   ErrorContext *ctx);

            /**
             * Reorganize page when updating a node's neighbors
             * This handles variable-sized nodes and preserves MGA fields
             */
            Status reorganize_page_for_node_update(
                uint64_t page_num,
                const TID &target_tid,
                uint16_t new_num_neighbors,
                const std::vector<TID> &new_neighbors,
                ErrorContext *ctx);

            Status pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx = nullptr) const;
            Status unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx = nullptr) const;
            GPID indexGPID(uint64_t page_num) const;

            // Random number generator for layer selection
            mutable std::mt19937 rng_;

            // Probability decay for layer selection (default: 1/ln(2))
            static constexpr double ML = 1.0 / 0.693147180559945309417;
        };

    } // namespace core
} // namespace scratchbird
