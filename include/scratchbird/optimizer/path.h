#pragma once

#include "scratchbird/core/types.h"
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/parser/ast.h"  // For JoinType and Expression
#include <memory>
#include <string>
#include <vector>

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
        RTREE_SCAN,        // R-tree spatial index scan (Phase 2, Task 9.2)
        NESTED_LOOP_JOIN,  // Nested loop join (Phase 1, Task 3.2)
        HASH_JOIN,         // Hash join (Phase 1, Task 3.2)
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
         * @param join_condition Join condition expression
         * @param selectivity Estimated selectivity of join condition
         * @param cost Cost estimate
         */
        NestedLoopJoinPath(parser::JoinType join_type,
                          std::shared_ptr<Path> outer_path,
                          std::shared_ptr<Path> inner_path,
                          parser::Expression* join_condition,
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
         * Get join condition
         */
        parser::Expression* joinCondition() const { return join_condition_; }

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
        parser::Expression* join_condition_;
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
         * @param join_condition Join condition expression
         * @param hash_keys_outer Hash key expressions from outer table
         * @param hash_keys_inner Hash key expressions from inner table
         * @param selectivity Estimated selectivity of join condition
         * @param cost Cost estimate
         */
        HashJoinPath(parser::JoinType join_type,
                    std::shared_ptr<Path> outer_path,
                    std::shared_ptr<Path> inner_path,
                    parser::Expression* join_condition,
                    const std::vector<parser::Expression*>& hash_keys_outer,
                    const std::vector<parser::Expression*>& hash_keys_inner,
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
        parser::Expression* join_condition_;
        std::vector<parser::Expression*> hash_keys_outer_;
        std::vector<parser::Expression*> hash_keys_inner_;
        double selectivity_;
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
         * @param grouping_exprs GROUP BY expressions (empty for simple aggregation)
         * @param aggregates Aggregate function expressions
         * @param having_clause HAVING clause expression (may be nullptr)
         * @param num_groups Estimated number of groups (1 for simple aggregation)
         * @param cost Cost estimate
         */
        AggregatePath(std::shared_ptr<Path> input_path,
                     const std::vector<parser::Expression*>& grouping_exprs,
                     const std::vector<parser::AggregateExpr*>& aggregates,
                     parser::Expression* having_clause,
                     uint64_t num_groups,
                     const CostEstimate& cost)
            : Path(PathType::AGGREGATE, cost),
              input_path_(std::move(input_path)),
              grouping_exprs_(grouping_exprs),
              aggregates_(aggregates),
              having_clause_(having_clause),
              num_groups_(num_groups)
        {
        }

        /**
         * Get input path
         */
        const std::shared_ptr<Path>& inputPath() const { return input_path_; }

        /**
         * Get GROUP BY expressions
         */
        const std::vector<parser::Expression*>& groupingExprs() const
        {
            return grouping_exprs_;
        }

        /**
         * Get aggregate functions
         */
        const std::vector<parser::AggregateExpr*>& aggregates() const
        {
            return aggregates_;
        }

        /**
         * Get HAVING clause
         */
        parser::Expression* havingClause() const { return having_clause_; }

        /**
         * Get number of groups
         */
        uint64_t numGroups() const { return num_groups_; }

        /**
         * Is this a simple aggregation (no GROUP BY)?
         */
        bool isSimpleAggregation() const { return grouping_exprs_.empty(); }

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
        std::vector<parser::Expression*> grouping_exprs_;
        std::vector<parser::AggregateExpr*> aggregates_;
        parser::Expression* having_clause_;
        uint64_t num_groups_;
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
         * @param order_by_items ORDER BY expressions with direction
         * @param row_width Estimated average row width
         * @param cost Cost estimate
         */
        SortPath(std::shared_ptr<Path> input_path,
                const std::vector<parser::OrderByItem>& order_by_items,
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
         * Get ORDER BY items
         */
        const std::vector<parser::OrderByItem>& orderByItems() const
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
        std::vector<parser::OrderByItem> order_by_items_;
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
         * @param window_funcs Window function expressions
         * @param cost Cost estimate
         */
        WindowPath(std::shared_ptr<Path> input_path,
                  const std::vector<parser::WindowFuncExpr*>& window_funcs,
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
         * Get window function expressions
         */
        const std::vector<parser::WindowFuncExpr*>& windowFuncs() const { return window_funcs_; }

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
        std::vector<parser::WindowFuncExpr*> window_funcs_;
    };

} // namespace scratchbird::optimizer
