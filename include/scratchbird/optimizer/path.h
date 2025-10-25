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
        NESTED_LOOP_JOIN,  // Nested loop join (Phase 1, Task 3.2)
        HASH_JOIN          // Hash join (Phase 1, Task 3.2)
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

} // namespace scratchbird::optimizer
