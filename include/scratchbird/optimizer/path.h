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

#include "scratchbird/core/types.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/parser/shared_types.h"      // For JoinType, WindowFunc, GroupingType, etc.
#include <memory>
#include <string>
#include <vector>

namespace scratchbird::parser::v3 {
    class Expression;
    class FunctionCallExpr;
    struct OrderByItem;
    struct WindowSpec;
}

namespace scratchbird::optimizer
{

    /**
     * PathType - Types of access paths considered by planner
     *
     * Phase 1, Task 1.3: Basic Query Planner
     */
    enum class PathType
    {
        SEQ_SCAN,          // Sequential table scan
        INDEX_SCAN,        // Index scan with heap fetch
        INDEX_ONLY_SCAN,   // Index-only scan (covering index, no heap access) - TASK-BYTECODE-4
        BITMAP_INDEX_SCAN, // Bitmap index scan (combine multiple indexes) - TASK-BYTECODE-4
        RTREE_SCAN,        // R-tree spatial index scan (Phase 2, Task 9.2)
        NESTED_LOOP_JOIN,  // Nested loop join (Phase 1, Task 3.2)
        HASH_JOIN,         // Hash join (Phase 1, Task 3.2)
        MERGE_JOIN,        // Merge join (NCW-040C)
        AGGREGATE,         // Aggregation (Phase 1, Task 4.1)
        SORT,              // Sort operation (Phase 1, Task 5.1)
        LIMIT,             // Limit/offset (Phase 1, Task 5.2)
        WINDOW             // Window functions (Phase 1, Task 6.2)
    };

    /**
     * Path - Represents a possible execution path for a query
     *
     * During query planning, we generate multiple Paths (different ways
     * to execute the query), estimate costs for each, and select the
     * cheapest Path. The chosen Path is then converted to a PlanNode.
     *
     * Path vs PlanNode:
     * - Path: lightweight, used during planning, many generated
     * - PlanNode: heavier, final execution plan, one selected
     *
     * Each Path contains:
     * - Cost estimates (from CostModel)
     * - Row estimates (from statistics)
     * - Information needed to generate PlanNode
     *
     * Phase 1, Task 1.3.2
     */
    class Path
    {
    public:
        /**
         * Constructor
         *
         * @param type Path type
         * @param cost Cost estimate for this path
         */
        Path(PathType type, const CostEstimate &cost)
            : type_(type), cost_(cost)
        {
        }

        virtual ~Path() = default;

        /**
         * Get path type
         */
        PathType type() const { return type_; }

        /**
         * Get cost estimate
         */
        const CostEstimate &cost() const { return cost_; }

        /**
         * Get startup cost
         */
        double startupCost() const { return cost_.startup_cost; }

        /**
         * Get total cost
         */
        double totalCost() const { return cost_.total_cost; }

        /**
         * Get estimated rows
         */
        uint64_t rows() const { return cost_.rows; }

        /**
         * Convert path to string for debugging
         */
        virtual auto toString() const -> std::string = 0;

    protected:
        PathType type_;
        CostEstimate cost_;
    };

    /**
     * SeqScanPath - Sequential scan access path
     *
     * Represents decision to scan table sequentially.
     *
     * Cost calculated by:
     *   CostModel::costSeqScan(num_pages, num_tuples, qual_cost)
     *
     * Example:
     *   SELECT * FROM users WHERE age > 25
     *   → SeqScanPath (cost=225.00, rows=6000)
     *
     * Phase 1, Task 1.3.2
     */
    class SeqScanPath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param table_id Table to scan
         * @param table_name Table name
         * @param num_pages Number of pages in table
         * @param num_tuples Estimated tuples to scan (after WHERE)
         * @param qual_cost Cost of WHERE clause evaluation
         * @param cost Cost estimate
         */
        SeqScanPath(const core::ID &table_id,
                    const std::string &table_name,
                    uint64_t num_pages,
                    uint64_t num_tuples,
                    double qual_cost,
                    const CostEstimate &cost)
            : Path(PathType::SEQ_SCAN, cost),
              table_id_(table_id),
              table_name_(table_name),
              num_pages_(num_pages),
              num_tuples_(num_tuples),
              qual_cost_(qual_cost)
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
         * Get number of pages
         */
        uint64_t numPages() const { return num_pages_; }

        /**
         * Get number of tuples
         */
        uint64_t numTuples() const { return num_tuples_; }

        /**
         * Get qualification cost
         */
        double qualCost() const { return qual_cost_; }

        /**
         * Set WHERE expression (OPT-L2: for EXPLAIN filter display)
         * @param expr WHERE clause expression (non-owning pointer) - V3 expression
         */
        void setWhereExpr(const parser::v3::Expression* expr) { where_expr_ = expr; }

        /**
         * Get WHERE expression
         * @return WHERE clause expression or nullptr if none
         */
        const parser::v3::Expression* whereExpr() const { return where_expr_; }

        /**
         * Convert to string for debugging
         */
        auto toString() const -> std::string override
        {
            return "SeqScanPath(table=" + table_name_ +
                   ", cost=" + std::to_string(cost_.total_cost) +
                   ", rows=" + std::to_string(cost_.rows) + ")";
        }

    private:
        core::ID table_id_;
        std::string table_name_;
        uint64_t num_pages_;
        uint64_t num_tuples_;
        double qual_cost_;
        const parser::v3::Expression* where_expr_ = nullptr;  // OPT-L2: WHERE clause for EXPLAIN (V3)
    };

    /**
     * IndexScanPath - Index scan access path
     *
     * Represents decision to use specific index.
     *
     * Cost calculated by:
     *   CostModel::costIndexScan(index_height, index_pages, index_tuples,
     *                            heap_pages, heap_tuples, qual_cost, correlation)
     *
     * Example:
     *   SELECT * FROM users WHERE id = 42
     *   → IndexScanPath(idx_users_id, cost=16.02, rows=1)
     *
     * Phase 1, Task 1.3.2
     */
    class IndexScanPath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param table_id Table to scan
         * @param table_name Table name
         * @param index_id Index to use
         * @param index_name Index name
         * @param index_height B-tree height
         * @param index_pages Index pages to access
         * @param index_tuples Index tuples to scan
         * @param heap_pages Heap pages to access
         * @param heap_tuples Heap tuples to fetch
         * @param qual_cost Cost of WHERE clause evaluation
         * @param correlation Physical ordering correlation
         * @param cost Cost estimate
         */
        IndexScanPath(const core::ID &table_id,
                      const std::string &table_name,
                      const core::ID &index_id,
                      const std::string &index_name,
                      uint64_t index_height,
                      uint64_t index_pages,
                      uint64_t index_tuples,
                      uint64_t heap_pages,
                      uint64_t heap_tuples,
                      double qual_cost,
                      double correlation,
                      const CostEstimate &cost)
            : Path(PathType::INDEX_SCAN, cost),
              table_id_(table_id),
              table_name_(table_name),
              index_id_(index_id),
              index_name_(index_name),
              index_height_(index_height),
              index_pages_(index_pages),
              index_tuples_(index_tuples),
              heap_pages_(heap_pages),
              heap_tuples_(heap_tuples),
              qual_cost_(qual_cost),
              correlation_(correlation)
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
         * Get index height
         */
        uint64_t indexHeight() const { return index_height_; }

        /**
         * Get index pages
         */
        uint64_t indexPages() const { return index_pages_; }

        /**
         * Get index tuples
         */
        uint64_t indexTuples() const { return index_tuples_; }

        /**
         * Get heap pages
         */
        uint64_t heapPages() const { return heap_pages_; }

        /**
         * Get heap tuples
         */
        uint64_t heapTuples() const { return heap_tuples_; }

        /**
         * Get qualification cost
         */
        double qualCost() const { return qual_cost_; }

        /**
         * Get correlation
         */
        double correlation() const { return correlation_; }

        /**
         * Set WHERE expression (OPT-L2: for EXPLAIN filter display)
         * @param expr WHERE clause expression (non-owning pointer) - V3 expression
         */
        void setWhereExpr(const parser::v3::Expression* expr) { where_expr_ = expr; }

        /**
         * Get WHERE expression
         * @return WHERE clause expression or nullptr if none
         */
        const parser::v3::Expression* whereExpr() const { return where_expr_; }

        /**
         * Convert to string for debugging
         */
        auto toString() const -> std::string override
        {
            return "IndexScanPath(table=" + table_name_ +
                   ", index=" + index_name_ +
                   ", cost=" + std::to_string(cost_.total_cost) +
                   ", rows=" + std::to_string(cost_.rows) + ")";
        }

    private:
        core::ID table_id_;
        std::string table_name_;
        core::ID index_id_;
        std::string index_name_;
        uint64_t index_height_;
        uint64_t index_pages_;
        uint64_t index_tuples_;
        uint64_t heap_pages_;
        uint64_t heap_tuples_;
        double qual_cost_;
        double correlation_;
        const parser::v3::Expression* where_expr_ = nullptr;  // OPT-L2: WHERE clause for EXPLAIN (V3)
    };

    /**
     * RTreeScanPath - R-tree spatial index scan access path
     *
     * Represents decision to use R-tree index for spatial queries.
     * R-trees are optimized for bounding box queries and spatial predicates.
     *
     * Cost calculated by:
     *   CostModel::costIndexScan(tree_height, tree_pages, matched_entries,
     *                            heap_pages, heap_tuples, qual_cost, 0.0)
     *
     * Applicable for spatial predicates:
     *   - ST_Intersects(geom, constant)
     *   - ST_Contains(geom, constant)
     *   - ST_Within(geom, constant)
     *   - ST_DWithin(geom, constant, distance)
     *
     * Example:
     *   SELECT * FROM buildings WHERE ST_Intersects(location, bbox)
     *   → RTreeScanPath(idx_buildings_location, cost=25.5, rows=100)
     *
     * Phase 2, Task 9.2
     */
    class RTreeScanPath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param table_id Table to scan
         * @param table_name Table name
         * @param index_id R-tree index to use
         * @param index_name Index name
         * @param tree_height R-tree height
         * @param tree_pages R-tree pages to access
         * @param matched_entries Estimated matching entries
         * @param heap_pages Heap pages to access
         * @param heap_tuples Heap tuples to fetch
         * @param qual_cost Cost of WHERE clause evaluation
         * @param predicate_type Type of spatial predicate (for optimization)
         * @param cost Cost estimate
         */
        RTreeScanPath(const core::ID &table_id,
                      const std::string &table_name,
                      const core::ID &index_id,
                      const std::string &index_name,
                      uint64_t tree_height,
                      uint64_t tree_pages,
                      uint64_t matched_entries,
                      uint64_t heap_pages,
                      uint64_t heap_tuples,
                      double qual_cost,
                      const std::string &predicate_type,
                      const CostEstimate &cost)
            : Path(PathType::RTREE_SCAN, cost),
              table_id_(table_id),
              table_name_(table_name),
              index_id_(index_id),
              index_name_(index_name),
              tree_height_(tree_height),
              tree_pages_(tree_pages),
              matched_entries_(matched_entries),
              heap_pages_(heap_pages),
              heap_tuples_(heap_tuples),
              qual_cost_(qual_cost),
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
         * Get tree height
         */
        uint64_t treeHeight() const { return tree_height_; }

        /**
         * Get tree pages
         */
        uint64_t treePages() const { return tree_pages_; }

        /**
         * Get matched entries
         */
        uint64_t matchedEntries() const { return matched_entries_; }

        /**
         * Get heap pages
         */
        uint64_t heapPages() const { return heap_pages_; }

        /**
         * Get heap tuples
         */
        uint64_t heapTuples() const { return heap_tuples_; }

        /**
         * Get qualification cost
         */
        double qualCost() const { return qual_cost_; }

        /**
         * Get predicate type
         */
        const std::string &predicateType() const { return predicate_type_; }

        /**
         * Convert to string for debugging
         */
        auto toString() const -> std::string override
        {
            return "RTreeScanPath(table=" + table_name_ +
                   ", index=" + index_name_ +
                   ", predicate=" + predicate_type_ +
                   ", cost=" + std::to_string(cost_.total_cost) +
                   ", rows=" + std::to_string(cost_.rows) + ")";
        }

    private:
        core::ID table_id_;
        std::string table_name_;
        core::ID index_id_;
        std::string index_name_;
        uint64_t tree_height_;
        uint64_t tree_pages_;
        uint64_t matched_entries_;
        uint64_t heap_pages_;
        uint64_t heap_tuples_;
        double qual_cost_;
        std::string predicate_type_;
    };

    /**
     * NestedLoopJoinPath - Nested loop join access path
     *
     * Represents decision to join two relations using nested loops.
     *
     * Cost calculated by:
     *   startup = outer_startup
     *   total = outer_total + (outer_rows * inner_total) + qual_cost
     *   where qual_cost = outer_rows * inner_rows * selectivity * cpu_tuple_cost
     *
     * Example:
     *   SELECT * FROM users u JOIN orders o ON u.id = o.user_id
     *   → NestedLoopJoinPath (outer=users, inner=orders_idx, cost=50,100)
     *
     * Phase 1, Task 3.2
     */
    class NestedLoopJoinPath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param join_type Type of join (INNER, LEFT, RIGHT, FULL, CROSS)
         * @param outer_path Outer (left) relation path
         * @param inner_path Inner (right) relation path
         * @param join_condition Join condition expression (V3)
         * @param selectivity Estimated selectivity of join condition
         * @param cost Cost estimate
         */
        NestedLoopJoinPath(parser::JoinType join_type,
                          std::shared_ptr<Path> outer_path,
                          std::shared_ptr<Path> inner_path,
                          parser::v3::Expression* join_condition,
                          double selectivity,
                          const CostEstimate& cost)
            : Path(PathType::NESTED_LOOP_JOIN, cost),
              join_type_(join_type),
              outer_path_(std::move(outer_path)),
              inner_path_(std::move(inner_path)),
              join_condition_(join_condition),
              selectivity_(selectivity)
        {
        }

        /**
         * Get join type
         */
        parser::JoinType joinType() const { return join_type_; }

        /**
         * Get outer path
         */
        const std::shared_ptr<Path>& outerPath() const { return outer_path_; }

        /**
         * Get inner path
         */
        const std::shared_ptr<Path>& innerPath() const { return inner_path_; }

        /**
         * Get join condition (V3 expression)
         */
        parser::v3::Expression* joinCondition() const { return join_condition_; }

        /**
         * Get join selectivity
         */
        double selectivity() const { return selectivity_; }

        /**
         * Convert to string for debugging
         */
        auto toString() const -> std::string override
        {
            return "NestedLoopJoinPath(cost=" + std::to_string(cost_.total_cost) +
                   ", rows=" + std::to_string(cost_.rows) +
                   ", selectivity=" + std::to_string(selectivity_) + ")";
        }

    private:
        parser::JoinType join_type_;
        std::shared_ptr<Path> outer_path_;
        std::shared_ptr<Path> inner_path_;
        parser::v3::Expression* join_condition_;
        double selectivity_;
    };

    /**
     * HashJoinPath - Hash join access path
     *
     * Represents decision to join two relations using hash table.
     *
     * Cost calculated by:
     *   startup = outer_total + hash_build_cost
     *   total = startup + inner_total + hash_probe_cost
     *
     * Only applicable for equi-joins (join condition contains =).
     *
     * Example:
     *   SELECT * FROM users u JOIN orders o ON u.id = o.user_id
     *   → HashJoinPath (outer=users, inner=orders, cost=3,075)
     *
     * Phase 1, Task 3.2
     */
    class HashJoinPath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param join_type Type of join (INNER, LEFT, RIGHT, FULL)
         * @param outer_path Outer (build side) relation path
         * @param inner_path Inner (probe side) relation path
         * @param join_condition Join condition expression (V3)
         * @param hash_keys_outer Hash key expressions from outer table (V3)
         * @param hash_keys_inner Hash key expressions from inner table (V3)
         * @param selectivity Estimated selectivity of join condition
         * @param cost Cost estimate
         */
        HashJoinPath(parser::JoinType join_type,
                    std::shared_ptr<Path> outer_path,
                    std::shared_ptr<Path> inner_path,
                    parser::v3::Expression* join_condition,
                    const std::vector<parser::v3::Expression*>& hash_keys_outer,
                    const std::vector<parser::v3::Expression*>& hash_keys_inner,
                    double selectivity,
                    const CostEstimate& cost)
            : Path(PathType::HASH_JOIN, cost),
              join_type_(join_type),
              outer_path_(std::move(outer_path)),
              inner_path_(std::move(inner_path)),
              join_condition_(join_condition),
              hash_keys_outer_(hash_keys_outer),
              hash_keys_inner_(hash_keys_inner),
              selectivity_(selectivity)
        {
        }

        /**
         * Get join type
         */
        parser::JoinType joinType() const { return join_type_; }

        /**
         * Get outer (build side) path
         */
        const std::shared_ptr<Path>& outerPath() const { return outer_path_; }

        /**
         * Get inner (probe side) path
         */
        const std::shared_ptr<Path>& innerPath() const { return inner_path_; }

        /**
         * Get join condition (V3 expression)
         */
        parser::v3::Expression* joinCondition() const { return join_condition_; }

        /**
         * Get hash keys from outer table (V3 expressions)
         */
        const std::vector<parser::v3::Expression*>& hashKeysOuter() const
        {
            return hash_keys_outer_;
        }

        /**
         * Get hash keys from inner table (V3 expressions)
         */
        const std::vector<parser::v3::Expression*>& hashKeysInner() const
        {
            return hash_keys_inner_;
        }

        /**
         * Get join selectivity
         */
        double selectivity() const { return selectivity_; }

        /**
         * Convert to string for debugging
         */
        auto toString() const -> std::string override
        {
            return "HashJoinPath(cost=" + std::to_string(cost_.total_cost) +
                   ", rows=" + std::to_string(cost_.rows) +
                   ", selectivity=" + std::to_string(selectivity_) + ")";
        }

    private:
        parser::JoinType join_type_;
        std::shared_ptr<Path> outer_path_;
        std::shared_ptr<Path> inner_path_;
        parser::v3::Expression* join_condition_;
        std::vector<parser::v3::Expression*> hash_keys_outer_;
        std::vector<parser::v3::Expression*> hash_keys_inner_;
        double selectivity_;
    };

    /**
     * MergeJoinPath - Merge join access path
     *
     * Represents decision to join two relations using sorted merge.
     *
     * Merge joins are only considered for equi-joins. Inputs may already be
     * ordered or may require an implicit sort that is accounted for in cost.
     */
    class MergeJoinPath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param join_type Type of join (INNER, LEFT)
         * @param outer_path Outer (left) relation path
         * @param inner_path Inner (right) relation path
         * @param join_condition Join condition expression (V3)
         * @param merge_keys_outer Merge key expressions from outer relation
         * @param merge_keys_inner Merge key expressions from inner relation
         * @param selectivity Estimated selectivity of join condition
         * @param outer_presorted True when the outer input is already ordered
         * @param inner_presorted True when the inner input is already ordered
         * @param cost Cost estimate
         */
        MergeJoinPath(parser::JoinType join_type,
                      std::shared_ptr<Path> outer_path,
                      std::shared_ptr<Path> inner_path,
                      parser::v3::Expression* join_condition,
                      const std::vector<parser::v3::Expression*>& merge_keys_outer,
                      const std::vector<parser::v3::Expression*>& merge_keys_inner,
                      double selectivity,
                      bool outer_presorted,
                      bool inner_presorted,
                      const CostEstimate& cost)
            : Path(PathType::MERGE_JOIN, cost),
              join_type_(join_type),
              outer_path_(std::move(outer_path)),
              inner_path_(std::move(inner_path)),
              join_condition_(join_condition),
              merge_keys_outer_(merge_keys_outer),
              merge_keys_inner_(merge_keys_inner),
              selectivity_(selectivity),
              outer_presorted_(outer_presorted),
              inner_presorted_(inner_presorted)
        {
        }

        parser::JoinType joinType() const { return join_type_; }
        const std::shared_ptr<Path>& outerPath() const { return outer_path_; }
        const std::shared_ptr<Path>& innerPath() const { return inner_path_; }
        parser::v3::Expression* joinCondition() const { return join_condition_; }
        const std::vector<parser::v3::Expression*>& mergeKeysOuter() const
        {
            return merge_keys_outer_;
        }
        const std::vector<parser::v3::Expression*>& mergeKeysInner() const
        {
            return merge_keys_inner_;
        }
        double selectivity() const { return selectivity_; }
        bool outerPresorted() const { return outer_presorted_; }
        bool innerPresorted() const { return inner_presorted_; }

        auto toString() const -> std::string override
        {
            return "MergeJoinPath(cost=" + std::to_string(cost_.total_cost) +
                   ", rows=" + std::to_string(cost_.rows) +
                   ", presorted=" +
                   std::string(outer_presorted_ ? "outer" : "sort_outer") + "/" +
                   std::string(inner_presorted_ ? "inner" : "sort_inner") + ")";
        }

    private:
        parser::JoinType join_type_;
        std::shared_ptr<Path> outer_path_;
        std::shared_ptr<Path> inner_path_;
        parser::v3::Expression* join_condition_;
        std::vector<parser::v3::Expression*> merge_keys_outer_;
        std::vector<parser::v3::Expression*> merge_keys_inner_;
        double selectivity_;
        bool outer_presorted_;
        bool inner_presorted_;
    };

    /**
     * AggregatePath - Aggregation access path
     *
     * Represents decision to perform aggregation with optional GROUP BY.
     *
     * Cost calculated by:
     *   startup = input_cost + hash_build_cost
     *   total = startup + (num_groups * num_aggregates * cpu_operator_cost)
     *
     * Example:
     *   SELECT dept, COUNT(*) FROM employees GROUP BY dept
     *   → AggregatePath (cost=1,550, rows=20)
     *
     * Phase 1, Task 4.1
     */
    class AggregatePath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param input_path Input path to aggregate
         * @param grouping_exprs GROUP BY expressions (V3, empty for simple aggregation)
         * @param aggregates Aggregate function expressions (V3)
         * @param having_clause HAVING clause expression (V3, may be nullptr)
         * @param num_groups Estimated number of groups (1 for simple aggregation)
         * @param cost Cost estimate
         * @param grouping_type Type of grouping (STANDARD, ROLLUP, CUBE, GROUPING_SETS)
         * @param grouping_sets Explicit grouping sets (V3, for GROUPING SETS)
         */
        AggregatePath(std::shared_ptr<Path> input_path,
                     const std::vector<parser::v3::Expression*>& grouping_exprs,
                     const std::vector<parser::v3::FunctionCallExpr*>& aggregates,
                     parser::v3::Expression* having_clause,
                     uint64_t num_groups,
                     const CostEstimate& cost,
                     parser::GroupingType grouping_type = parser::GroupingType::STANDARD,
                     const std::vector<std::vector<parser::v3::Expression*>>& grouping_sets = {})
            : Path(PathType::AGGREGATE, cost),
              input_path_(std::move(input_path)),
              grouping_exprs_(grouping_exprs),
              aggregates_(aggregates),
              having_clause_(having_clause),
              num_groups_(num_groups),
              grouping_type_(grouping_type),
              grouping_sets_(grouping_sets)
        {
        }

        /**
         * Get input path
         */
        const std::shared_ptr<Path>& inputPath() const { return input_path_; }

        /**
         * Get GROUP BY expressions (V3)
         */
        const std::vector<parser::v3::Expression*>& groupingExprs() const
        {
            return grouping_exprs_;
        }

        /**
         * Get aggregate functions (V3 function calls)
         */
        const std::vector<parser::v3::FunctionCallExpr*>& aggregates() const
        {
            return aggregates_;
        }

        /**
         * Get HAVING clause (V3 expression)
         */
        parser::v3::Expression* havingClause() const { return having_clause_; }

        /**
         * Get number of groups
         */
        uint64_t numGroups() const { return num_groups_; }

        /**
         * Is this a simple aggregation (no GROUP BY)?
         */
        bool isSimpleAggregation() const { return grouping_exprs_.empty(); }

        /**
         * Get grouping type (STANDARD, ROLLUP, CUBE, GROUPING_SETS)
         */
        parser::GroupingType groupingType() const { return grouping_type_; }

        /**
         * Get explicit grouping sets (V3, for GROUPING SETS)
         */
        const std::vector<std::vector<parser::v3::Expression*>>& groupingSets() const
        {
            return grouping_sets_;
        }

        /**
         * Convert to string for debugging
         */
        auto toString() const -> std::string override
        {
            std::string result = "AggregatePath(";
            if (!grouping_exprs_.empty())
            {
                result += "groups=" + std::to_string(num_groups_) + ", ";
            }
            result += "aggregates=" + std::to_string(aggregates_.size()) +
                     ", cost=" + std::to_string(cost_.total_cost) +
                     ", rows=" + std::to_string(cost_.rows) + ")";
            return result;
        }

    private:
        std::shared_ptr<Path> input_path_;
        std::vector<parser::v3::Expression*> grouping_exprs_;
        std::vector<parser::v3::FunctionCallExpr*> aggregates_;
        parser::v3::Expression* having_clause_;
        uint64_t num_groups_;
        parser::GroupingType grouping_type_;
        std::vector<std::vector<parser::v3::Expression*>> grouping_sets_;
    };

    /**
     * SortPath - Sorting access path
     *
     * Represents decision to sort the result set.
     *
     * Cost calculated by:
     *   startup = input_cost + sort_cost
     *   sort_cost = n * log2(n) * comparison_cost
     *
     * Example:
     *   SELECT * FROM users ORDER BY age DESC, name ASC
     *   → SortPath (cost=8,250, rows=10000)
     *
     * Phase 1, Task 5.1
     */
    class SortPath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param input_path Input path to sort
         * @param order_by_items ORDER BY expressions with direction (V3)
         * @param row_width Estimated average row width
         * @param cost Cost estimate
         */
        SortPath(std::shared_ptr<Path> input_path,
                const std::vector<parser::v3::OrderByItem*>& order_by_items,
                uint64_t row_width,
                const CostEstimate& cost)
            : Path(PathType::SORT, cost),
              input_path_(std::move(input_path)),
              order_by_items_(order_by_items),
              row_width_(row_width)
        {
        }

        /**
         * Get input path
         */
        const std::shared_ptr<Path>& inputPath() const { return input_path_; }

        /**
         * Get ORDER BY items (V3)
         */
        const std::vector<parser::v3::OrderByItem*>& orderByItems() const
        {
            return order_by_items_;
        }

        /**
         * Get row width
         */
        uint64_t rowWidth() const { return row_width_; }

        /**
         * Convert to string for debugging
         */
        auto toString() const -> std::string override
        {
            return "SortPath(keys=" + std::to_string(order_by_items_.size()) +
                   ", cost=" + std::to_string(cost_.total_cost) +
                   ", rows=" + std::to_string(cost_.rows) + ")";
        }

    private:
        std::shared_ptr<Path> input_path_;
        std::vector<parser::v3::OrderByItem*> order_by_items_;
        uint64_t row_width_;
    };

    /**
     * LimitPath - Limit/offset access path
     *
     * Represents decision to limit result set with optional offset.
     *
     * Cost calculated by:
     *   startup = input_startup + (offset_count * cpu_tuple_cost)
     *   total = startup + (limit_count * cpu_tuple_cost)
     *
     * Example:
     *   SELECT * FROM users LIMIT 10 OFFSET 100
     *   → LimitPath (cost=112, rows=10)
     *
     * Phase 1, Task 5.2
     */
    class LimitPath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param input_path Input path to limit
         * @param limit_count LIMIT value (-1 for no limit)
         * @param offset_count OFFSET value (-1 for no offset)
         * @param cost Cost estimate
         */
        LimitPath(std::shared_ptr<Path> input_path,
                 int64_t limit_count,
                 int64_t offset_count,
                 const CostEstimate& cost)
            : Path(PathType::LIMIT, cost),
              input_path_(std::move(input_path)),
              limit_count_(limit_count),
              offset_count_(offset_count)
        {
        }

        /**
         * Get input path
         */
        const std::shared_ptr<Path>& inputPath() const { return input_path_; }

        /**
         * Get limit count
         */
        int64_t limitCount() const { return limit_count_; }

        /**
         * Get offset count
         */
        int64_t offsetCount() const { return offset_count_; }

        /**
         * Convert to string for debugging
         */
        auto toString() const -> std::string override
        {
            std::string result = "LimitPath(";
            if (limit_count_ >= 0)
            {
                result += "limit=" + std::to_string(limit_count_);
                if (offset_count_ >= 0)
                {
                    result += ", offset=" + std::to_string(offset_count_);
                }
            }
            result += ", cost=" + std::to_string(cost_.total_cost) +
                     ", rows=" + std::to_string(cost_.rows) + ")";
            return result;
        }

    private:
        std::shared_ptr<Path> input_path_;
        int64_t limit_count_;
        int64_t offset_count_;
    };

    /**
     * WindowPath - Window function evaluation path
     *
     * Represents evaluation of window functions (ROW_NUMBER, RANK, LAG, etc.)
     * over partitioned and ordered data.
     *
     * Phase 1, Task 6.2
     */
    class WindowPath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param input_path Input path to evaluate window functions over
         * @param window_funcs Window function expressions (V3 function calls with window specs)
         * @param cost Cost estimate
         */
        WindowPath(std::shared_ptr<Path> input_path,
                  const std::vector<parser::v3::FunctionCallExpr*>& window_funcs,
                  const CostEstimate& cost)
            : Path(PathType::WINDOW, cost),
              input_path_(std::move(input_path)),
              window_funcs_(window_funcs)
        {
        }

        /**
         * Get input path
         */
        const std::shared_ptr<Path>& inputPath() const { return input_path_; }

        /**
         * Get window function expressions (V3, with window specs)
         */
        const std::vector<parser::v3::FunctionCallExpr*>& windowFuncs() const { return window_funcs_; }

        /**
         * Convert to string for debugging
         */
        auto toString() const -> std::string override
        {
            std::string result = "WindowPath(";
            result += "funcs=" + std::to_string(window_funcs_.size());
            result += ", cost=" + std::to_string(cost_.total_cost);
            result += ", rows=" + std::to_string(cost_.rows) + ")";
            return result;
        }

    private:
        std::shared_ptr<Path> input_path_;
        std::vector<parser::v3::FunctionCallExpr*> window_funcs_;
    };

    /**
     * IndexOnlyScanPath - Index-only scan access path (covering index)
     *
     * Represents decision to scan index WITHOUT fetching heap tuples.
     * Only applicable when index contains all columns needed by query.
     *
     * Benefits:
     * - No heap I/O (much faster for selective queries)
     * - Reduced cache pressure
     * - Better performance for queries on indexed columns only
     *
     * Requirements:
     * - Index must cover all columns in SELECT + WHERE + ORDER BY
     * - Visibility information available (via visibility map or TIP)
     *
     * Example:
     *   CREATE INDEX idx_users_id_name ON users(id, name);
     *   SELECT id, name FROM users WHERE id > 100;
     *   → IndexOnlyScanPath (no heap access needed!)
     *
     * TASK-BYTECODE-4: Query Planner Integration
     */
    class IndexOnlyScanPath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param table_id Table to scan
         * @param table_name Table name
         * @param index_id Index to use
         * @param index_name Index name
         * @param index_height B-tree height
         * @param index_pages Index pages to access
         * @param index_tuples Index tuples to scan
         * @param qual_cost Cost of WHERE clause evaluation
         * @param correlation Physical ordering correlation
         * @param cost Cost estimate
         */
        IndexOnlyScanPath(const core::ID &table_id,
                          const std::string &table_name,
                          const core::ID &index_id,
                          const std::string &index_name,
                          uint64_t index_height,
                          uint64_t index_pages,
                          uint64_t index_tuples,
                          double qual_cost,
                          double correlation,
                          const CostEstimate &cost)
            : Path(PathType::INDEX_ONLY_SCAN, cost),
              table_id_(table_id),
              table_name_(table_name),
              index_id_(index_id),
              index_name_(index_name),
              index_height_(index_height),
              index_pages_(index_pages),
              index_tuples_(index_tuples),
              qual_cost_(qual_cost),
              correlation_(correlation)
        {
        }

        // Accessors
        const core::ID &tableId() const { return table_id_; }
        const std::string &tableName() const { return table_name_; }
        const core::ID &indexId() const { return index_id_; }
        const std::string &indexName() const { return index_name_; }
        uint64_t indexHeight() const { return index_height_; }
        uint64_t indexPages() const { return index_pages_; }
        uint64_t indexTuples() const { return index_tuples_; }
        double qualCost() const { return qual_cost_; }
        double correlation() const { return correlation_; }

        auto toString() const -> std::string override
        {
            return "IndexOnlyScanPath(index=" + index_name_ +
                   ", cost=" + std::to_string(cost_.total_cost) +
                   ", rows=" + std::to_string(cost_.rows) + ")";
        }

    private:
        core::ID table_id_;
        std::string table_name_;
        core::ID index_id_;
        std::string index_name_;
        uint64_t index_height_;
        uint64_t index_pages_;
        uint64_t index_tuples_;
        double qual_cost_;
        double correlation_;
    };

    /**
     * BitmapIndexScanPath - Bitmap index scan access path
     *
     * Represents decision to combine multiple indexes using bitmap operations.
     * Useful for multi-column queries where no single index is perfect.
     *
     * How it works:
     * 1. Scan each applicable index to build a bitmap of matching TIDs
     * 2. Combine bitmaps using AND/OR operations
     * 3. Sort TIDs to enable sequential heap access
     * 4. Fetch heap tuples in physical order (better I/O pattern)
     *
     * Example:
     *   SELECT * FROM users WHERE age > 25 AND city = 'Seattle';
     *   → BitmapIndexScan combining:
     *     - idx_users_age (bitmap1)
     *     - idx_users_city (bitmap2)
     *     - Result: bitmap1 AND bitmap2
     *
     * Benefits vs multiple IndexScans:
     * - Single heap pass (vs random access per index)
     * - Combines indexes for better selectivity
     * - Eliminates duplicate heap fetches
     *
     * TASK-BYTECODE-4: Query Planner Integration
     */
    class BitmapIndexScanPath : public Path
    {
    public:
        /**
         * Constructor
         *
         * @param table_id Table to scan
         * @param table_name Table name
         * @param index_ids Indexes to combine
         * @param index_names Index names
         * @param bitmap_op Operation to combine bitmaps (AND/OR)
         * @param total_index_pages Total index pages across all indexes
         * @param estimated_heap_pages Estimated heap pages (after bitmap combination)
         * @param estimated_tuples Estimated matching tuples
         * @param qual_cost Cost of WHERE clause evaluation
         * @param cost Cost estimate
         */
        BitmapIndexScanPath(const core::ID &table_id,
                            const std::string &table_name,
                            const std::vector<core::ID> &index_ids,
                            const std::vector<std::string> &index_names,
                            const std::string &bitmap_op,  // "AND" or "OR"
                            uint64_t total_index_pages,
                            uint64_t estimated_heap_pages,
                            uint64_t estimated_tuples,
                            double qual_cost,
                            const CostEstimate &cost)
            : Path(PathType::BITMAP_INDEX_SCAN, cost),
              table_id_(table_id),
              table_name_(table_name),
              index_ids_(index_ids),
              index_names_(index_names),
              bitmap_op_(bitmap_op),
              total_index_pages_(total_index_pages),
              estimated_heap_pages_(estimated_heap_pages),
              estimated_tuples_(estimated_tuples),
              qual_cost_(qual_cost)
        {
        }

        // Accessors
        const core::ID &tableId() const { return table_id_; }
        const std::string &tableName() const { return table_name_; }
        const std::vector<core::ID> &indexIds() const { return index_ids_; }
        const std::vector<std::string> &indexNames() const { return index_names_; }
        const std::string &bitmapOp() const { return bitmap_op_; }
        uint64_t totalIndexPages() const { return total_index_pages_; }
        uint64_t estimatedHeapPages() const { return estimated_heap_pages_; }
        uint64_t estimatedTuples() const { return estimated_tuples_; }
        double qualCost() const { return qual_cost_; }

        auto toString() const -> std::string override
        {
            std::string idx_list;
            for (size_t i = 0; i < index_names_.size(); i++)
            {
                if (i > 0) idx_list += " " + bitmap_op_ + " ";
                idx_list += index_names_[i];
            }
            return "BitmapIndexScanPath(indexes=[" + idx_list + "]" +
                   ", cost=" + std::to_string(cost_.total_cost) +
                   ", rows=" + std::to_string(cost_.rows) + ")";
        }

    private:
        core::ID table_id_;
        std::string table_name_;
        std::vector<core::ID> index_ids_;
        std::vector<std::string> index_names_;
        std::string bitmap_op_;  // "AND" or "OR"
        uint64_t total_index_pages_;
        uint64_t estimated_heap_pages_;
        uint64_t estimated_tuples_;
        double qual_cost_;
    };

} // namespace scratchbird::optimizer
