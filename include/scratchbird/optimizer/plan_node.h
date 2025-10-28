#pragma once

#include "scratchbird/core/types.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/parser/ast.h"  // For JoinType and Expression
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
        NESTED_LOOP_JOIN,  // Nested loop join (Phase 1, Task 3.2)
        HASH_JOIN,     // Hash join (Phase 1, Task 3.2)
        MERGE_JOIN,    // Merge join (future)
        SORT,          // Sort operation (Phase 1, Task 5.1)
        AGGREGATE,     // Aggregation (Phase 1, Task 4.2)
        LIMIT,         // Limit/offset (Phase 1, Task 5.2)
        WINDOW         // Window function (Phase 1, Task 6.2)
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
                          parser::Expression* join_condition)
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
        parser::Expression* joinCondition() const { return join_condition_; }

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
        parser::Expression* join_condition_;
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
                    parser::Expression* join_condition,
                    const std::vector<parser::Expression*>& hash_keys_outer,
                    const std::vector<parser::Expression*>& hash_keys_inner)
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
        parser::Expression* joinCondition() const { return join_condition_; }

        /**
         * Get hash keys from outer table
         */
        const std::vector<parser::Expression*>& hashKeysOuter() const
        {
            return hash_keys_outer_;
        }

        /**
         * Get hash keys from inner table
         */
        const std::vector<parser::Expression*>& hashKeysInner() const
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
        parser::Expression* join_condition_;
        std::vector<parser::Expression*> hash_keys_outer_;
        std::vector<parser::Expression*> hash_keys_inner_;
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
         * @param aggregates Aggregate expressions in SELECT list
         * @param having_clause Optional HAVING filter (nullptr if none)
         */
        AggregateNode(std::shared_ptr<PlanNode> child_plan,
                     const std::vector<parser::Expression*>& grouping_exprs,
                     const std::vector<parser::AggregateExpr*>& aggregates,
                     parser::Expression* having_clause = nullptr)
            : PlanNode(PlanNodeType::AGGREGATE),
              child_plan_(std::move(child_plan)),
              grouping_exprs_(grouping_exprs),
              aggregates_(aggregates),
              having_clause_(having_clause)
        {
        }

        /**
         * Get child plan
         */
        const std::shared_ptr<PlanNode>& childPlan() const { return child_plan_; }

        /**
         * Get grouping expressions
         */
        const std::vector<parser::Expression*>& groupingExprs() const { return grouping_exprs_; }

        /**
         * Get aggregate expressions
         */
        const std::vector<parser::AggregateExpr*>& aggregates() const { return aggregates_; }

        /**
         * Get HAVING clause
         */
        parser::Expression* havingClause() const { return having_clause_; }

        /**
         * Check if this is a simple aggregation (no GROUP BY)
         */
        bool isSimpleAggregation() const { return grouping_exprs_.empty(); }

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
        std::vector<parser::Expression*> grouping_exprs_;
        std::vector<parser::AggregateExpr*> aggregates_;
        parser::Expression* having_clause_;
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
         * @param order_by_items List of sort keys with direction
         */
        SortNode(std::shared_ptr<PlanNode> child_plan,
                const std::vector<parser::OrderByItem>& order_by_items)
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
        const std::vector<parser::OrderByItem>& orderByItems() const { return order_by_items_; }

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
        std::vector<parser::OrderByItem> order_by_items_;
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
    // Represents window function evaluation
    class WindowNode : public PlanNode
    {
    public:
        // Window function specification
        struct WindowFunction
        {
            parser::WindowFunc func;                    // Function type (ROW_NUMBER, RANK, etc.)
            std::vector<parser::Expression*> args;      // Function arguments
            parser::WindowSpec* window_spec;            // Window specification (OVER clause)
            std::string output_column;                  // Output column name

            WindowFunction(parser::WindowFunc f,
                          const std::vector<parser::Expression*>& a,
                          parser::WindowSpec* spec,
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
                if (wf.window_spec && !wf.window_spec->orderBy().empty())
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
                if (wf.window_spec && !wf.window_spec->partitionBy().empty())
                {
                    result += " PARTITION BY (";
                    for (size_t i = 0; i < wf.window_spec->partitionBy().size(); i++)
                    {
                        if (i > 0) result += ", ";
                        result += "expr";
                    }
                    result += ")";
                }

                // Show ORDER BY
                if (wf.window_spec && !wf.window_spec->orderBy().empty())
                {
                    result += " ORDER BY (";
                    for (size_t i = 0; i < wf.window_spec->orderBy().size(); i++)
                    {
                        if (i > 0) result += ", ";
                        result += "expr";
                    }
                    result += ")";
                }

                // Show frame clause
                if (wf.window_spec && wf.window_spec->hasFrame())
                {
                    result += " " + std::string(wf.window_spec->frameMode() == parser::FrameMode::ROWS ? "ROWS" : "RANGE");
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

} // namespace scratchbird::optimizer
