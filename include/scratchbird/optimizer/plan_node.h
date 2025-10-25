#pragma once

#include "scratchbird/core/types.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/optimizer/cost_model.h"
#include <memory>
#include <string>
#include <vector>

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
        NESTED_LOOP,   // Nested loop join (future)
        HASH_JOIN,     // Hash join (future)
        MERGE_JOIN,    // Merge join (future)
        SORT,          // Sort operation (future)
        AGGREGATE,     // Aggregation (future)
        LIMIT          // Limit/offset (future)
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

} // namespace scratchbird::optimizer
