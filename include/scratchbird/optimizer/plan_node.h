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

/**
 * ScratchBird Query Plan Nodes
 *
 * This module defines the execution plan node types used by the optimizer.
 * Plan nodes form a tree representing query execution strategy.
 *
 * Type Dependencies:
 * - Enum types (JoinType, WindowFunc, etc.) from shared_types.h
 * - Resolved expression types from sblr/resolved_ast_v2.h (V2 AST)
 *
 * MIGRATION COMPLETE: This file uses V2 resolved types exclusively.
 */

#include "scratchbird/core/types.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/parser/shared_types.h"      // For JoinType, WindowFunc, etc.
#include "scratchbird/sblr/resolved_ast_v2.h"     // For ResolvedExpression, ResolvedWindowSpec, etc. (V2 AST)
#include <memory>
#include <string>
#include <vector>
#include <cmath>  // For std::log2

namespace scratchbird::core {
    using ID = UuidV7Bytes;
}

namespace scratchbird::optimizer
{

    /**
     * PlanNodeType - Types of plan nodes in execution plan tree
     *
     * Phase 1, Task 1.3: Basic Query Planner
     */
    enum class PlanNodeType
    {
        SEQ_SCAN,      // Sequential table scan
        INDEX_SCAN,    // Index scan with heap fetch
        INDEX_ONLY_SCAN,   // Index-only scan (covering index, no heap access) - TASK-BYTECODE-4
        BITMAP_INDEX_SCAN, // Bitmap index scan (combine multiple indexes) - TASK-BYTECODE-4
        RTREE_SCAN,    // R-tree spatial index scan (Phase 2, Task 9.2)
        CTE_SCAN,      // CTE (Common Table Expression) scan (Phase 2 Wave 2)
        NESTED_LOOP_JOIN,  // Nested loop join (Phase 1, Task 3.2)
        HASH_JOIN,     // Hash join (Phase 1, Task 3.2)
        MERGE_JOIN,    // Merge join (future)
        SORT,          // Sort operation (Phase 1, Task 5.1)
        AGGREGATE,     // Aggregation (Phase 1, Task 4.2)
        LIMIT,         // Limit/offset (Phase 1, Task 5.2)
        WINDOW,        // Window function (Phase 1, Task 6.2)
        SUBPLAN        // Subquery execution (Phase 2 Wave 2 - Agent B)
    };

    /**
     * ScanDirection - Direction of scan through table/index
     */
    enum class ScanDirection
    {
        FORWARD,   // Scan in ascending order
        BACKWARD,  // Scan in descending order
        NO_MOVEMENT // Single tuple fetch (future)
    };

    /**
     * PlanNode - Base class for all plan nodes
     *
     * A plan node represents one step in query execution plan.
     * Plan nodes form a tree where:
     * - Leaf nodes are scans (SeqScan, IndexScan)
     * - Internal nodes are operations (Join, Sort, Aggregate)
     *
     * Each node has:
     * - Cost estimates (from cost model)
     * - Row estimates (cardinality)
     * - Child nodes (if any)
     *
     * Phase 1, Task 1.3.1
     */
    class PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param type Plan node type
         */
        explicit PlanNode(PlanNodeType type)
            : type_(type), startup_cost_(0.0), total_cost_(0.0), rows_(0)
        {
        }

        virtual ~PlanNode() = default;

        /**
         * Get plan node type
         */
        PlanNodeType type() const { return type_; }

        /**
         * Get startup cost (one-time setup cost)
         */
        double startupCost() const { return startup_cost_; }

        /**
         * Get total cost (startup + run cost)
         */
        double totalCost() const { return total_cost_; }

        /**
         * Get estimated rows returned
         */
        uint64_t rows() const { return rows_; }

        /**
         * Set cost estimates
         */
        void setCost(double startup_cost, double total_cost, uint64_t rows)
        {
            startup_cost_ = startup_cost;
            total_cost_ = total_cost;
            rows_ = rows;
        }

        /**
         * Get child nodes
         */
        const std::vector<std::shared_ptr<PlanNode>> &children() const
        {
            return children_;
        }

        /**
         * Add child node
         */
        void addChild(std::shared_ptr<PlanNode> child)
        {
            children_.push_back(std::move(child));
        }

        /**
         * Convert plan node to string for debugging/EXPLAIN
         *
         * @param indent Indentation level for tree display
         * @return String representation of plan node
         */
        virtual auto toString(int indent = 0) const -> std::string = 0;

    protected:
        PlanNodeType type_;
        double startup_cost_;
        double total_cost_;
        uint64_t rows_;
        std::vector<std::shared_ptr<PlanNode>> children_;
    };

    /**
     * SeqScanNode - Sequential table scan plan node
     *
     * Scans entire table sequentially from first to last page.
     *
     * Properties:
     * - No startup cost (just start reading)
     * - Cost proportional to table size
     * - Returns all rows that pass WHERE clause
     *
     * Example:
     *   SeqScan on users (cost=0.00..225.00 rows=10000)
     *     Filter: age > 25
     *
     * Phase 1, Task 1.3.1
     */
    class SeqScanNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param table_id Table to scan
         * @param table_name Table name (for EXPLAIN)
         * @param direction Scan direction (FORWARD/BACKWARD)
         */
        SeqScanNode(const core::ID &table_id,
                    const std::string &table_name,
                    ScanDirection direction = ScanDirection::FORWARD)
            : PlanNode(PlanNodeType::SEQ_SCAN),
              table_id_(table_id),
              table_name_(table_name),
              direction_(direction),
              qual_cost_(0.0)
        {
        }

        /**
         * Get table ID
         */
        const core::ID &tableId() const { return table_id_; }

        /**
         * Get table name
         */
        const std::string &tableName() const { return table_name_; }

        /**
         * Get scan direction
         */
        ScanDirection direction() const { return direction_; }

        /**
         * Set qualification (WHERE clause) cost
         */
        void setQualCost(double qual_cost) { qual_cost_ = qual_cost; }

        /**
         * Get qualification cost
         */
        double qualCost() const { return qual_cost_; }

        /**
         * Set filter expression (for EXPLAIN display)
         *
         * @param filter Filter expression string (e.g., "age > 25")
         */
        void setFilter(const std::string &filter) { filter_ = filter; }

        /**
         * Get filter expression
         */
        const std::string &filter() const { return filter_; }

        /**
         * Convert to string for EXPLAIN
         *
         * Format:
         *   SeqScan on <table> (cost=<startup>..<total> rows=<rows>)
         *     Filter: <filter>
         */
        auto toString(int indent = 0) const -> std::string override
        {
            std::string result(indent * 2, ' ');
            result += "SeqScan on " + table_name_;
            result += " (cost=" + std::to_string(startup_cost_);
            result += ".." + std::to_string(total_cost_);
            result += " rows=" + std::to_string(rows_) + ")";

            if (!filter_.empty())
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "Filter: " + filter_;
            }

            return result;
        }

    private:
        core::ID table_id_;
        std::string table_name_;
        ScanDirection direction_;
        double qual_cost_;
        std::string filter_;
    };

    /**
     * CTEScanNode - CTE (Common Table Expression) scan plan node
     *
     * Scans a CTE that was defined in a WITH clause.
     * The CTE is materialized (executed and stored) once, then scanned.
     *
     * Properties:
     * - Startup cost: cost of materializing the CTE
     * - Run cost: cost of scanning the materialized results
     * - CTEs can be referenced multiple times (shared scan)
     *
     * Example:
     *   CTEScan on high_value_orders (cost=100.00..125.00 rows=50)
     *
     * Phase 2 Wave 2 - Agent A
     */
    class CTEScanNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param cte_name CTE name (for EXPLAIN)
         * @param cte_query The CTE query plan
         */
        CTEScanNode(const std::string &cte_name,
                    std::shared_ptr<PlanNode> cte_query)
            : PlanNode(PlanNodeType::CTE_SCAN),
              cte_name_(cte_name),
              cte_query_(std::move(cte_query))
        {
        }

        /**
         * Get CTE name
         */
        const std::string &cteName() const { return cte_name_; }

        /**
         * Get CTE query plan
         */
        const std::shared_ptr<PlanNode> &cteQuery() const { return cte_query_; }

        /**
         * Convert to string for EXPLAIN
         *
         * Format:
         *   CTEScan on <cte_name> (cost=<startup>..<total> rows=<rows>)
         */
        auto toString(int indent = 0) const -> std::string override
        {
            std::string result(indent * 2, ' ');
            result += "CTEScan on " + cte_name_;
            result += " (cost=" + std::to_string(startup_cost_);
            result += ".." + std::to_string(total_cost_);
            result += " rows=" + std::to_string(rows_) + ")";
            return result;
        }

    private:
        std::string cte_name_;
        std::shared_ptr<PlanNode> cte_query_;
    };

    /**
     * IndexScanNode - Index scan plan node
     *
     * Uses index to find matching rows, then fetches from heap.
     *
     * Properties:
     * - Startup cost: B-tree traversal from root to first leaf
     * - Run cost: index I/O + heap fetch I/O + CPU
     * - Correlation affects heap fetch cost
     * - Can return rows in index order (useful for ORDER BY)
     *
     * Example:
     *   IndexScan on users using idx_users_id (cost=0.01..16.02 rows=1)
     *     Index Cond: id = 42
     *     Filter: age > 25
     *
     * Phase 1, Task 1.3.1
     */
    class IndexScanNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param table_id Table to scan
         * @param table_name Table name (for EXPLAIN)
         * @param index_id Index to use
         * @param index_name Index name (for EXPLAIN)
         * @param direction Scan direction (FORWARD/BACKWARD)
         */
        IndexScanNode(const core::ID &table_id,
                      const std::string &table_name,
                      const core::ID &index_id,
                      const std::string &index_name,
                      ScanDirection direction = ScanDirection::FORWARD)
            : PlanNode(PlanNodeType::INDEX_SCAN),
              table_id_(table_id),
              table_name_(table_name),
              index_id_(index_id),
              index_name_(index_name),
              direction_(direction),
              index_qual_cost_(0.0),
              heap_qual_cost_(0.0),
              correlation_(0.0)
        {
        }

        /**
         * Get table ID
         */
        const core::ID &tableId() const { return table_id_; }

        /**
         * Get table name
         */
        const std::string &tableName() const { return table_name_; }

        /**
         * Get index ID
         */
        const core::ID &indexId() const { return index_id_; }

        /**
         * Get index name
         */
        const std::string &indexName() const { return index_name_; }

        /**
         * Get scan direction
         */
        ScanDirection direction() const { return direction_; }

        /**
         * Set index qualification (index WHERE clause) cost
         */
        void setIndexQualCost(double qual_cost) { index_qual_cost_ = qual_cost; }

        /**
         * Get index qualification cost
         */
        double indexQualCost() const { return index_qual_cost_; }

        /**
         * Set heap qualification (additional heap WHERE clause) cost
         */
        void setHeapQualCost(double qual_cost) { heap_qual_cost_ = qual_cost; }

        /**
         * Get heap qualification cost
         */
        double heapQualCost() const { return heap_qual_cost_; }

        /**
         * Set physical ordering correlation
         */
        void setCorrelation(double correlation) { correlation_ = correlation; }

        /**
         * Get physical ordering correlation
         */
        double correlation() const { return correlation_; }

        /**
         * Set index condition (for EXPLAIN display)
         *
         * @param index_cond Index condition string (e.g., "id = 42")
         */
        void setIndexCond(const std::string &index_cond)
        {
            index_cond_ = index_cond;
        }

        /**
         * Get index condition
         */
        const std::string &indexCond() const { return index_cond_; }

        /**
         * Set filter expression (for EXPLAIN display)
         *
         * @param filter Filter expression string (e.g., "age > 25")
         */
        void setFilter(const std::string &filter) { filter_ = filter; }

        /**
         * Get filter expression
         */
        const std::string &filter() const { return filter_; }

        /**
         * Convert to string for EXPLAIN
         *
         * Format:
         *   IndexScan on <table> using <index> (cost=<startup>..<total> rows=<rows>)
         *     Index Cond: <index_cond>
         *     Filter: <filter>
         */
        auto toString(int indent = 0) const -> std::string override
        {
            std::string result(indent * 2, ' ');
            result += "IndexScan on " + table_name_;
            result += " using " + index_name_;
            result += " (cost=" + std::to_string(startup_cost_);
            result += ".." + std::to_string(total_cost_);
            result += " rows=" + std::to_string(rows_) + ")";

            if (!index_cond_.empty())
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "Index Cond: " + index_cond_;
            }

            if (!filter_.empty())
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "Filter: " + filter_;
            }

            return result;
        }

    private:
        core::ID table_id_;
        std::string table_name_;
        core::ID index_id_;
        std::string index_name_;
        ScanDirection direction_;
        double index_qual_cost_;
        double heap_qual_cost_;
        double correlation_;
        std::string index_cond_;
        std::string filter_;
    };

    /**
     * RTreeScanNode - R-tree spatial index scan plan node
     *
     * Scans R-tree index to find tuples matching spatial predicate.
     * R-trees index bounding boxes and support efficient spatial queries.
     *
     * Typical predicates:
     * - ST_Intersects(geometry_column, constant_bbox)
     * - ST_Contains(geometry_column, constant_point)
     * - ST_Within(geometry_column, constant_polygon)
     *
     * Phase 2, Task 9.2
     */
    class RTreeScanNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param table_id Table to scan
         * @param table_name Table name (for EXPLAIN)
         * @param index_id R-tree index to use
         * @param index_name Index name (for EXPLAIN)
         * @param predicate_type Type of spatial predicate
         */
        RTreeScanNode(const core::ID &table_id,
                      const std::string &table_name,
                      const core::ID &index_id,
                      const std::string &index_name,
                      const std::string &predicate_type)
            : PlanNode(PlanNodeType::RTREE_SCAN),
              table_id_(table_id),
              table_name_(table_name),
              index_id_(index_id),
              index_name_(index_name),
              predicate_type_(predicate_type)
        {
        }

        /**
         * Get table ID
         */
        const core::ID &tableId() const { return table_id_; }

        /**
         * Get table name
         */
        const std::string &tableName() const { return table_name_; }

        /**
         * Get index ID
         */
        const core::ID &indexId() const { return index_id_; }

        /**
         * Get index name
         */
        const std::string &indexName() const { return index_name_; }

        /**
         * Get predicate type
         */
        const std::string &predicateType() const { return predicate_type_; }

        /**
         * Set spatial condition (for EXPLAIN display)
         *
         * @param spatial_cond Spatial condition string (e.g., "ST_Intersects(location, bbox)")
         */
        void setSpatialCond(const std::string &spatial_cond)
        {
            spatial_cond_ = spatial_cond;
        }

        /**
         * Get spatial condition
         */
        const std::string &spatialCond() const { return spatial_cond_; }

        /**
         * Set filter (additional non-spatial WHERE conditions)
         *
         * @param filter Filter condition string
         */
        void setFilter(const std::string &filter)
        {
            filter_ = filter;
        }

        /**
         * Get filter
         */
        const std::string &filter() const { return filter_; }

        /**
         * Convert to human-readable string for EXPLAIN
         *
         * Format:
         *   RTreeScan on <table> using <index> (cost=<startup>..<total> rows=<rows>)
         *     Spatial Cond: <spatial_cond>
         *     Filter: <filter>
         */
        auto toString(int indent = 0) const -> std::string override
        {
            std::string result(indent * 2, ' ');
            result += "RTreeScan on " + table_name_;
            result += " using " + index_name_;
            result += " (cost=" + std::to_string(startup_cost_);
            result += ".." + std::to_string(total_cost_);
            result += " rows=" + std::to_string(rows_) + ")";

            if (!spatial_cond_.empty())
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "Spatial Cond: " + spatial_cond_;
            }

            if (!filter_.empty())
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "Filter: " + filter_;
            }

            return result;
        }

    private:
        core::ID table_id_;
        std::string table_name_;
        core::ID index_id_;
        std::string index_name_;
        std::string predicate_type_;
        std::string spatial_cond_;
        std::string filter_;
    };

    /**
     * IndexOnlyScanNode - Index-only scan plan node (covering index)
     *
     * Scans index WITHOUT fetching heap tuples. Only applicable when:
     * - Index contains all columns needed by SELECT + WHERE + ORDER BY
     * - Visibility information is available (via visibility map or TIP)
     *
     * Benefits vs IndexScan:
     * - No heap I/O (much faster for selective queries)
     * - Better cache utilization
     * - Lower latency for indexed column queries
     *
     * Example:
     *   CREATE INDEX idx_users_id_name ON users(id, name);
     *   SELECT id, name FROM users WHERE id > 100;
     *   → IndexOnlyScan on idx_users_id_name (cost=0.00..10.00 rows=50)
     *
     * TASK-BYTECODE-4: Query Planner Integration
     */
    class IndexOnlyScanNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param table_id Table to scan
         * @param table_name Table name (for EXPLAIN)
         * @param index_id Index to use
         * @param index_name Index name (for EXPLAIN)
         * @param direction Scan direction (FORWARD/BACKWARD)
         */
        IndexOnlyScanNode(const core::ID &table_id,
                          const std::string &table_name,
                          const core::ID &index_id,
                          const std::string &index_name,
                          ScanDirection direction = ScanDirection::FORWARD)
            : PlanNode(PlanNodeType::INDEX_ONLY_SCAN),
              table_id_(table_id),
              table_name_(table_name),
              index_id_(index_id),
              index_name_(index_name),
              direction_(direction)
        {
        }

        const core::ID &tableId() const { return table_id_; }
        const std::string &tableName() const { return table_name_; }
        const core::ID &indexId() const { return index_id_; }
        const std::string &indexName() const { return index_name_; }
        ScanDirection direction() const { return direction_; }

        void setIndexCond(const std::string &index_cond) { index_cond_ = index_cond; }
        const std::string &indexCond() const { return index_cond_; }

        void setFilter(const std::string &filter) { filter_ = filter; }
        const std::string &filter() const { return filter_; }

        auto toString(int indent = 0) const -> std::string override
        {
            std::string result(indent * 2, ' ');
            result += "IndexOnlyScan on " + table_name_;
            result += " using " + index_name_;
            result += " (cost=" + std::to_string(startup_cost_);
            result += ".." + std::to_string(total_cost_);
            result += " rows=" + std::to_string(rows_) + ")";

            if (!index_cond_.empty())
            {
                result += "\n" + std::string((indent + 1) * 2, ' ');
                result += "Index Cond: " + index_cond_;
            }
            if (!filter_.empty())
            {
                result += "\n" + std::string((indent + 1) * 2, ' ');
                result += "Filter: " + filter_;
            }
            return result;
        }

    private:
        core::ID table_id_;
        std::string table_name_;
        core::ID index_id_;
        std::string index_name_;
        ScanDirection direction_;
        std::string index_cond_;
        std::string filter_;
    };

    /**
     * BitmapIndexScanNode - Bitmap index scan plan node
     *
     * Combines multiple indexes using bitmap operations (AND/OR).
     * Useful for queries where multiple indexes are applicable but
     * none is perfectly selective.
     *
     * Algorithm:
     * 1. Scan each index to build bitmap of matching TIDs
     * 2. Combine bitmaps (AND/OR operations)
     * 3. Sort resulting TIDs by physical location
     * 4. Fetch heap tuples in sequential order
     *
     * Benefits:
     * - Single heap pass (vs multiple random accesses)
     * - Combines multiple indexes for better selectivity
     * - Eliminates duplicate fetches (AND automatically deduplicates)
     * - Better I/O pattern (sorted TIDs → sequential heap access)
     *
     * Example:
     *   SELECT * FROM users WHERE age > 25 AND city = 'Seattle';
     *   → BitmapIndexScan combining idx_users_age AND idx_users_city
     *
     * TASK-BYTECODE-4: Query Planner Integration
     */
    class BitmapIndexScanNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param table_id Table to scan
         * @param table_name Table name (for EXPLAIN)
         * @param index_ids Indexes to combine
         * @param index_names Index names (for EXPLAIN)
         * @param bitmap_op Bitmap operation ("AND" or "OR")
         */
        BitmapIndexScanNode(const core::ID &table_id,
                            const std::string &table_name,
                            const std::vector<core::ID> &index_ids,
                            const std::vector<std::string> &index_names,
                            const std::string &bitmap_op)
            : PlanNode(PlanNodeType::BITMAP_INDEX_SCAN),
              table_id_(table_id),
              table_name_(table_name),
              index_ids_(index_ids),
              index_names_(index_names),
              bitmap_op_(bitmap_op)
        {
        }

        const core::ID &tableId() const { return table_id_; }
        const std::string &tableName() const { return table_name_; }
        const std::vector<core::ID> &indexIds() const { return index_ids_; }
        const std::vector<std::string> &indexNames() const { return index_names_; }
        const std::string &bitmapOp() const { return bitmap_op_; }

        void setIndexConds(const std::vector<std::string> &index_conds) { index_conds_ = index_conds; }
        const std::vector<std::string> &indexConds() const { return index_conds_; }

        void setFilter(const std::string &filter) { filter_ = filter; }
        const std::string &filter() const { return filter_; }

        auto toString(int indent = 0) const -> std::string override
        {
            std::string result(indent * 2, ' ');
            result += "BitmapIndexScan on " + table_name_;
            result += " (cost=" + std::to_string(startup_cost_);
            result += ".." + std::to_string(total_cost_);
            result += " rows=" + std::to_string(rows_) + ")";

            result += "\n" + std::string((indent + 1) * 2, ' ');
            result += "Bitmap Op: " + bitmap_op_;

            result += "\n" + std::string((indent + 1) * 2, ' ');
            result += "Indexes: [";
            for (size_t i = 0; i < index_names_.size(); i++)
            {
                if (i > 0) result += ", ";
                result += index_names_[i];
            }
            result += "]";

            if (!index_conds_.empty())
            {
                result += "\n" + std::string((indent + 1) * 2, ' ');
                result += "Index Conds: [";
                for (size_t i = 0; i < index_conds_.size(); i++)
                {
                    if (i > 0) result += ", ";
                    result += index_conds_[i];
                }
                result += "]";
            }

            if (!filter_.empty())
            {
                result += "\n" + std::string((indent + 1) * 2, ' ');
                result += "Filter: " + filter_;
            }

            return result;
        }

    private:
        core::ID table_id_;
        std::string table_name_;
        std::vector<core::ID> index_ids_;
        std::vector<std::string> index_names_;
        std::string bitmap_op_;  // "AND" or "OR"
        std::vector<std::string> index_conds_;
        std::string filter_;
    };

    /**
     * NestedLoopJoinNode - Nested loop join plan node
     *
     * Joins two relations using nested loops:
     * - For each row in outer relation (left)
     *   - Scan inner relation (right) and check join condition
     *   - Emit matching rows
     *
     * Properties:
     * - Startup cost: outer startup cost
     * - Total cost: outer_cost + (outer_rows * inner_cost) + qual_cost
     * - Works for all join types (INNER, LEFT, RIGHT, FULL, CROSS)
     * - Inner can be parameterized (index scan on join key)
     *
     * Example:
     *   Nested Loop Join (cost=100.00..50,100.00 rows=100000)
     *     Join Cond: (u.id = o.user_id)
     *     -> SeqScan on users u (cost=0.00..100.00 rows=10000)
     *     -> IndexScan on orders o using idx_orders_user_id (cost=0.00..5.00 rows=10)
     *
     * Phase 1, Task 3.2
     */
    class NestedLoopJoinNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param join_type Type of join (INNER, LEFT, RIGHT, FULL, CROSS)
         * @param outer_plan Outer (left) side plan
         * @param inner_plan Inner (right) side plan
         * @param join_condition Join condition expression (ON clause, nullptr for CROSS)
         */
        NestedLoopJoinNode(parser::JoinType join_type,
                          std::shared_ptr<PlanNode> outer_plan,
                          std::shared_ptr<PlanNode> inner_plan,
                          parser::v2::ResolvedExpression* join_condition)
            : PlanNode(PlanNodeType::NESTED_LOOP_JOIN),
              join_type_(join_type),
              outer_plan_(std::move(outer_plan)),
              inner_plan_(std::move(inner_plan)),
              join_condition_(join_condition)
        {
        }

        /**
         * Get join type
         */
        parser::JoinType joinType() const { return join_type_; }

        /**
         * Get outer (left) plan
         */
        const std::shared_ptr<PlanNode>& outerPlan() const { return outer_plan_; }

        /**
         * Get inner (right) plan
         */
        const std::shared_ptr<PlanNode>& innerPlan() const { return inner_plan_; }

        /**
         * Get join condition
         */
        parser::v2::ResolvedExpression* joinCondition() const { return join_condition_; }

        /**
         * Set join condition string (for EXPLAIN)
         */
        void setJoinCondString(const std::string& join_cond_str)
        {
            join_cond_str_ = join_cond_str;
        }

        /**
         * Get join condition string
         */
        const std::string& joinCondString() const { return join_cond_str_; }

        /**
         * Convert to string for EXPLAIN
         *
         * Format:
         *   Nested Loop [Join Type] (cost=<startup>..<total> rows=<rows>)
         *     Join Cond: <condition>
         *     -> <outer plan>
         *     -> <inner plan>
         */
        auto toString(int indent = 0) const -> std::string override
        {
            std::string result(indent * 2, ' ');
            result += "Nested Loop ";

            // Add join type
            switch (join_type_)
            {
                case parser::JoinType::INNER:
                    result += "Join";
                    break;
                case parser::JoinType::LEFT:
                    result += "Left Join";
                    break;
                case parser::JoinType::RIGHT:
                    result += "Right Join";
                    break;
                case parser::JoinType::FULL:
                    result += "Full Outer Join";
                    break;
                case parser::JoinType::CROSS:
                    result += "Cross Join";
                    break;
            }

            result += " (cost=" + std::to_string(startup_cost_);
            result += ".." + std::to_string(total_cost_);
            result += " rows=" + std::to_string(rows_) + ")";

            // Add join condition (if not CROSS join)
            if (join_type_ != parser::JoinType::CROSS && !join_cond_str_.empty())
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "Join Cond: " + join_cond_str_;
            }

            // Add outer plan
            if (outer_plan_)
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "-> " + outer_plan_->toString(indent + 2);
            }

            // Add inner plan
            if (inner_plan_)
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "-> " + inner_plan_->toString(indent + 2);
            }

            return result;
        }

    private:
        parser::JoinType join_type_;
        std::shared_ptr<PlanNode> outer_plan_;
        std::shared_ptr<PlanNode> inner_plan_;
        parser::v2::ResolvedExpression* join_condition_;
        std::string join_cond_str_;  // For EXPLAIN display
    };

    /**
     * HashJoinNode - Hash join plan node
     *
     * Joins two relations using hash table:
     * - Phase 1 (Build): Build hash table from outer relation
     * - Phase 2 (Probe): Probe hash table with inner relation rows
     *
     * Properties:
     * - Startup cost: Build entire hash table (outer_cost + hash_build_cost)
     * - Total cost: startup + probe_cost
     * - Only works for equi-joins (join condition contains =)
     * - Memory usage: Hash table size = outer_rows * row_width
     * - Best for: Large tables with equi-join conditions
     *
     * Example:
     *   Hash Join (cost=300.00..3,075.00 rows=100000)
     *     Hash Cond: (u.id = o.user_id)
     *     -> SeqScan on users u (cost=0.00..100.00 rows=10000)
     *     -> SeqScan on orders o (cost=0.00..1000.00 rows=100000)
     *
     * Phase 1, Task 3.2
     */
    class HashJoinNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param join_type Type of join (INNER, LEFT, RIGHT, FULL)
         * @param outer_plan Outer (build side) plan - usually smaller table
         * @param inner_plan Inner (probe side) plan - usually larger table
         * @param join_condition Join condition expression (must contain =)
         * @param hash_keys_outer Hash key expressions from outer table
         * @param hash_keys_inner Hash key expressions from inner table
         */
        HashJoinNode(parser::JoinType join_type,
                    std::shared_ptr<PlanNode> outer_plan,
                    std::shared_ptr<PlanNode> inner_plan,
                    parser::v2::ResolvedExpression* join_condition,
                    const std::vector<parser::v2::ResolvedExpression*>& hash_keys_outer,
                    const std::vector<parser::v2::ResolvedExpression*>& hash_keys_inner)
            : PlanNode(PlanNodeType::HASH_JOIN),
              join_type_(join_type),
              outer_plan_(std::move(outer_plan)),
              inner_plan_(std::move(inner_plan)),
              join_condition_(join_condition),
              hash_keys_outer_(hash_keys_outer),
              hash_keys_inner_(hash_keys_inner)
        {
        }

        /**
         * Get join type
         */
        parser::JoinType joinType() const { return join_type_; }

        /**
         * Get outer (build side) plan
         */
        const std::shared_ptr<PlanNode>& outerPlan() const { return outer_plan_; }

        /**
         * Get inner (probe side) plan
         */
        const std::shared_ptr<PlanNode>& innerPlan() const { return inner_plan_; }

        /**
         * Get join condition
         */
        parser::v2::ResolvedExpression* joinCondition() const { return join_condition_; }

        /**
         * Get hash keys from outer table
         */
        const std::vector<parser::v2::ResolvedExpression*>& hashKeysOuter() const
        {
            return hash_keys_outer_;
        }

        /**
         * Get hash keys from inner table
         */
        const std::vector<parser::v2::ResolvedExpression*>& hashKeysInner() const
        {
            return hash_keys_inner_;
        }

        /**
         * Set join condition string (for EXPLAIN)
         */
        void setJoinCondString(const std::string& join_cond_str)
        {
            join_cond_str_ = join_cond_str;
        }

        /**
         * Get join condition string
         */
        const std::string& joinCondString() const { return join_cond_str_; }

        /**
         * Convert to string for EXPLAIN
         *
         * Format:
         *   Hash [Join Type] (cost=<startup>..<total> rows=<rows>)
         *     Hash Cond: <condition>
         *     -> <outer plan>
         *     -> <inner plan>
         */
        auto toString(int indent = 0) const -> std::string override
        {
            std::string result(indent * 2, ' ');
            result += "Hash ";

            // Add join type
            switch (join_type_)
            {
                case parser::JoinType::INNER:
                    result += "Join";
                    break;
                case parser::JoinType::LEFT:
                    result += "Left Join";
                    break;
                case parser::JoinType::RIGHT:
                    result += "Right Join";
                    break;
                case parser::JoinType::FULL:
                    result += "Full Outer Join";
                    break;
                case parser::JoinType::CROSS:
                    // Hash join not applicable for CROSS join
                    result += "Join (ERROR: CROSS join not applicable)";
                    break;
            }

            result += " (cost=" + std::to_string(startup_cost_);
            result += ".." + std::to_string(total_cost_);
            result += " rows=" + std::to_string(rows_) + ")";

            // Add hash condition
            if (!join_cond_str_.empty())
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "Hash Cond: " + join_cond_str_;
            }

            // Add outer plan (build side)
            if (outer_plan_)
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "-> " + outer_plan_->toString(indent + 2);
            }

            // Add inner plan (probe side)
            if (inner_plan_)
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "-> " + inner_plan_->toString(indent + 2);
            }

            return result;
        }

    private:
        parser::JoinType join_type_;
        std::shared_ptr<PlanNode> outer_plan_;
        std::shared_ptr<PlanNode> inner_plan_;
        parser::v2::ResolvedExpression* join_condition_;
        std::vector<parser::v2::ResolvedExpression*> hash_keys_outer_;
        std::vector<parser::v2::ResolvedExpression*> hash_keys_inner_;
        std::string join_cond_str_;  // For EXPLAIN display
    };

    /**
     * AggregateNode - Aggregation with optional grouping (Phase 1, Task 4.2)
     *
     * Implements GROUP BY and aggregate functions (COUNT, SUM, AVG, MIN, MAX).
     * Uses hash-based grouping for efficient aggregation.
     */
    class AggregateNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param child_plan Input plan to aggregate
         * @param grouping_exprs Expressions to group by (empty for no GROUP BY)
         * @param aggregates Aggregate function calls in SELECT list
         * @param having_clause Optional HAVING filter (nullptr if none)
         * @param grouping_type Type of grouping (STANDARD, ROLLUP, CUBE, GROUPING_SETS)
         * @param grouping_sets Explicit grouping sets (for GROUPING SETS)
         */
        AggregateNode(std::shared_ptr<PlanNode> child_plan,
                     const std::vector<parser::v2::ResolvedExpression*>& grouping_exprs,
                     const std::vector<parser::v2::ResolvedFunctionCall*>& aggregates,
                     parser::v2::ResolvedExpression* having_clause = nullptr,
                     parser::GroupingType grouping_type = parser::GroupingType::STANDARD,
                     const std::vector<std::vector<parser::v2::ResolvedExpression*>>& grouping_sets = {})
            : PlanNode(PlanNodeType::AGGREGATE),
              child_plan_(std::move(child_plan)),
              grouping_exprs_(grouping_exprs),
              aggregates_(aggregates),
              having_clause_(having_clause),
              grouping_type_(grouping_type),
              grouping_sets_(grouping_sets)
        {
        }

        /**
         * Get child plan
         */
        const std::shared_ptr<PlanNode>& childPlan() const { return child_plan_; }

        /**
         * Get grouping expressions
         */
        const std::vector<parser::v2::ResolvedExpression*>& groupingExprs() const { return grouping_exprs_; }

        /**
         * Get aggregate expressions (function calls with is_aggregate=true)
         */
        const std::vector<parser::v2::ResolvedFunctionCall*>& aggregates() const { return aggregates_; }

        /**
         * Get HAVING clause
         */
        parser::v2::ResolvedExpression* havingClause() const { return having_clause_; }

        /**
         * Check if this is a simple aggregation (no GROUP BY)
         */
        bool isSimpleAggregation() const { return grouping_exprs_.empty(); }

        /**
         * Get grouping type (STANDARD, ROLLUP, CUBE, GROUPING_SETS)
         */
        parser::GroupingType groupingType() const { return grouping_type_; }

        /**
         * Get explicit grouping sets (for GROUPING SETS)
         */
        const std::vector<std::vector<parser::v2::ResolvedExpression*>>& groupingSets() const
        {
            return grouping_sets_;
        }

        /**
         * Generate EXPLAIN output
         */
        std::string toString(int indent = 0) const override
        {
            std::string result(indent, ' ');
            result += "Aggregate";

            if (!grouping_exprs_.empty())
            {
                result += " (GROUP BY: " + std::to_string(grouping_exprs_.size()) + " expr(s))";
            }

            result += " (cost=" + std::to_string(total_cost_) +
                     " rows=" + std::to_string(rows_) + ")\n";

            if (child_plan_)
            {
                result += child_plan_->toString(indent + 2);
            }

            return result;
        }

    private:
        std::shared_ptr<PlanNode> child_plan_;
        std::vector<parser::v2::ResolvedExpression*> grouping_exprs_;
        std::vector<parser::v2::ResolvedFunctionCall*> aggregates_;
        parser::v2::ResolvedExpression* having_clause_;
        parser::GroupingType grouping_type_;
        std::vector<std::vector<parser::v2::ResolvedExpression*>> grouping_sets_;
    };

    /**
     * SortNode - Sorting operation (Phase 1, Task 5.1)
     *
     * Implements ORDER BY with ASC/DESC and NULLS FIRST/LAST.
     * Uses quicksort or external merge sort depending on data size.
     */
    class SortNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param child_plan Input plan to sort
         * @param order_by_items List of sort keys with direction (V2 resolved types)
         */
        SortNode(std::shared_ptr<PlanNode> child_plan,
                const std::vector<parser::v2::ResolvedOrderByItem*>& order_by_items)
            : PlanNode(PlanNodeType::SORT),
              child_plan_(std::move(child_plan)),
              order_by_items_(order_by_items)
        {
        }

        /**
         * Get child plan
         */
        const std::shared_ptr<PlanNode>& childPlan() const { return child_plan_; }

        /**
         * Get ORDER BY items
         */
        const std::vector<parser::v2::ResolvedOrderByItem*>& orderByItems() const { return order_by_items_; }

        /**
         * Generate EXPLAIN output
         */
        std::string toString(int indent = 0) const override
        {
            std::string result(indent, ' ');
            result += "Sort (keys=" + std::to_string(order_by_items_.size()) +
                     " cost=" + std::to_string(total_cost_) +
                     " rows=" + std::to_string(rows_) + ")\n";

            if (child_plan_)
            {
                result += child_plan_->toString(indent + 2);
            }

            return result;
        }

    private:
        std::shared_ptr<PlanNode> child_plan_;
        std::vector<parser::v2::ResolvedOrderByItem*> order_by_items_;
    };

    /**
     * LimitNode - LIMIT/OFFSET operation (Phase 1, Task 5.2)
     *
     * Implements result set limiting and offset for pagination.
     * Allows early termination of query execution.
     */
    class LimitNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param child_plan Input plan to limit
         * @param limit_count Maximum rows to return (-1 for unlimited)
         * @param offset_count Rows to skip (-1 for no offset)
         */
        LimitNode(std::shared_ptr<PlanNode> child_plan,
                 int64_t limit_count,
                 int64_t offset_count = -1)
            : PlanNode(PlanNodeType::LIMIT),
              child_plan_(std::move(child_plan)),
              limit_count_(limit_count),
              offset_count_(offset_count)
        {
        }

        /**
         * Get child plan
         */
        const std::shared_ptr<PlanNode>& childPlan() const { return child_plan_; }

        /**
         * Get limit count
         */
        int64_t limitCount() const { return limit_count_; }

        /**
         * Get offset count
         */
        int64_t offsetCount() const { return offset_count_; }

        /**
         * Generate EXPLAIN output
         */
        std::string toString(int indent = 0) const override
        {
            std::string result(indent, ' ');
            result += "Limit";

            if (limit_count_ >= 0)
            {
                result += " (count=" + std::to_string(limit_count_);
                if (offset_count_ >= 0)
                {
                    result += " offset=" + std::to_string(offset_count_);
                }
                result += ")";
            }

            result += " (cost=" + std::to_string(total_cost_) +
                     " rows=" + std::to_string(rows_) + ")\n";

            if (child_plan_)
            {
                result += child_plan_->toString(indent + 2);
            }

            return result;
        }

    private:
        std::shared_ptr<PlanNode> child_plan_;
        int64_t limit_count_;
        int64_t offset_count_;
    };

    // Window function node (Phase 1 Task 6.2)
    // Represents window function evaluation - Now using V2 resolved types
    class WindowNode : public PlanNode
    {
    public:
        // Window function specification (V2 resolved types)
        struct WindowFunction
        {
            parser::WindowFunc func;                              // Function type (ROW_NUMBER, RANK, etc.)
            std::vector<parser::v2::ResolvedExpression*> args;    // Function arguments
            parser::v2::ResolvedWindowSpec* window_spec;          // Window specification (OVER clause)
            std::string output_column;                            // Output column name

            WindowFunction(parser::WindowFunc f,
                          const std::vector<parser::v2::ResolvedExpression*>& a,
                          parser::v2::ResolvedWindowSpec* spec,
                          const std::string& col)
                : func(f), args(a), window_spec(spec), output_column(col) {}
        };

        WindowNode(std::shared_ptr<PlanNode> child,
                   const std::vector<WindowFunction>& window_funcs)
            : PlanNode(PlanNodeType::WINDOW),
              child_plan_(std::move(child)),
              window_functions_(window_funcs)
        {
            // Copy cost estimates from child
            startup_cost_ = child_plan_->startupCost();
            total_cost_ = child_plan_->totalCost();
            rows_ = child_plan_->rows();

            // Add window function processing cost
            // Base cost: sorting cost if ORDER BY present
            // Processing cost: O(n) per window function
            double window_cost = 0.0;
            for (const auto& wf : window_functions_)
            {
                // Check if window requires sorting
                if (wf.window_spec && !wf.window_spec->order_by.empty())
                {
                    // Sorting cost: O(n log n)
                    double n = static_cast<double>(rows_);
                    window_cost += n * std::log2(n + 1.0) * 0.01; // CPU cost per comparison
                }
                // Processing cost: O(n) per function
                window_cost += static_cast<double>(rows_) * 0.001;
            }

            total_cost_ += window_cost;

            // Window functions don't change row count
            // (they add columns but maintain same number of rows)
        }

        std::shared_ptr<PlanNode> child() const { return child_plan_; }
        const std::vector<WindowFunction>& windowFunctions() const { return window_functions_; }

        std::string toString(int indent = 0) const override
        {
            std::string result = std::string(indent, ' ') + "Window ";
            result += "(cost=" + std::to_string(total_cost_);
            result += " rows=" + std::to_string(rows_) + ")\n";

            // Show window functions
            for (const auto& wf : window_functions_)
            {
                result += std::string(indent + 2, ' ');
                result += windowFuncToString(wf.func) + " -> " + wf.output_column;

                // Show PARTITION BY
                if (wf.window_spec && !wf.window_spec->partition_by.empty())
                {
                    result += " PARTITION BY (";
                    for (size_t i = 0; i < wf.window_spec->partition_by.size(); i++)
                    {
                        if (i > 0) result += ", ";
                        result += "expr";
                    }
                    result += ")";
                }

                // Show ORDER BY
                if (wf.window_spec && !wf.window_spec->order_by.empty())
                {
                    result += " ORDER BY (";
                    for (size_t i = 0; i < wf.window_spec->order_by.size(); i++)
                    {
                        if (i > 0) result += ", ";
                        result += "expr";
                    }
                    result += ")";
                }

                // Show frame clause
                if (wf.window_spec && wf.window_spec->has_frame)
                {
                    result += " " + std::string(wf.window_spec->frame_mode == parser::FrameMode::ROWS ? "ROWS" : "RANGE");
                    result += " BETWEEN ... AND ...";
                }

                result += "\n";
            }

            // Show child plan
            result += child_plan_->toString(indent + 2);
            return result;
        }

    private:
        std::shared_ptr<PlanNode> child_plan_;
        std::vector<WindowFunction> window_functions_;

        static std::string windowFuncToString(parser::WindowFunc func)
        {
            switch (func)
            {
                case parser::WindowFunc::ROW_NUMBER: return "ROW_NUMBER";
                case parser::WindowFunc::RANK: return "RANK";
                case parser::WindowFunc::DENSE_RANK: return "DENSE_RANK";
                case parser::WindowFunc::LAG: return "LAG";
                case parser::WindowFunc::LEAD: return "LEAD";
                case parser::WindowFunc::FIRST_VALUE: return "FIRST_VALUE";
                case parser::WindowFunc::LAST_VALUE: return "LAST_VALUE";
                case parser::WindowFunc::NTH_VALUE: return "NTH_VALUE";
                default: return "UNKNOWN";
            }
        }
    };

    /**
     * SubplanNode - Subquery execution plan node
     *
     * Represents execution of a subquery within an expression context.
     * Supports all subquery types:
     * - SCALAR: Returns single value
     * - EXISTS: Returns boolean (true if any rows)
     * - IN/NOT IN: Membership test
     * - ARRAY: Returns array of values
     *
     * Properties:
     * - Startup cost: Cost of executing subquery
     * - For correlated subqueries: executed once per outer row
     * - For uncorrelated subqueries: executed once and cached
     *
     * Example:
     *   Scalar Subquery (cost=50.00..55.00 rows=1)
     *     -> SeqScan on employees (cost=0.00..50.00 rows=100)
     *
     * Phase 2 Wave 2 - Agent B: Subqueries
     */
    class SubplanNode : public PlanNode
    {
    public:
        /**
         * Constructor
         *
         * @param subquery_type Type of subquery (SCALAR, EXISTS, IN, etc.)
         * @param subplan The plan for executing the subquery
         */
        SubplanNode(parser::SubqueryType subquery_type,
                   std::shared_ptr<PlanNode> subplan)
            : PlanNode(PlanNodeType::SUBPLAN),
              subquery_type_(subquery_type),
              subplan_(std::move(subplan))
        {
        }

        /**
         * Get subquery type
         */
        parser::SubqueryType subqueryType() const { return subquery_type_; }

        /**
         * Get subquery plan
         */
        const std::shared_ptr<PlanNode>& subplan() const { return subplan_; }

        /**
         * Convert to string for EXPLAIN
         *
         * Format:
         *   <Type> Subquery (cost=<startup>..<total> rows=<rows>)
         *     -> <subplan>
         */
        std::string toString(int indent = 0) const override
        {
            std::string result(indent * 2, ' ');

            // Add subquery type
            switch (subquery_type_)
            {
                case parser::SubqueryType::SCALAR:
                    result += "Scalar Subquery";
                    break;
                case parser::SubqueryType::EXISTS:
                    result += "Exists Subquery";
                    break;
                case parser::SubqueryType::IN:
                    result += "In Subquery";
                    break;
                case parser::SubqueryType::NOT_IN:
                    result += "Not In Subquery";
                    break;
                case parser::SubqueryType::ARRAY:
                    result += "Array Subquery";
                    break;
            }

            result += " (cost=" + std::to_string(startup_cost_);
            result += ".." + std::to_string(total_cost_);
            result += " rows=" + std::to_string(rows_) + ")";

            // Add subplan
            if (subplan_)
            {
                result += "\n";
                result += std::string((indent + 1) * 2, ' ');
                result += "-> " + subplan_->toString(indent + 2);
            }

            return result;
        }

    private:
        parser::SubqueryType subquery_type_;
        std::shared_ptr<PlanNode> subplan_;
    };

} // namespace scratchbird::optimizer
