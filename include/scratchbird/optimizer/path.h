#pragma once

#include "scratchbird/core/types.h"
#include "scratchbird/optimizer/cost_model.h"
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
        SEQ_SCAN,    // Sequential table scan
        INDEX_SCAN   // Index scan with heap fetch
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

} // namespace scratchbird::optimizer
