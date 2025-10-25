#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <cstdint>
#include <string>

namespace scratchbird::optimizer
{

    /**
     * Cost Parameters - Configuration for query cost estimation
     *
     * Following PostgreSQL's cost model where seq_page_cost = 1.0 is the baseline.
     * All costs are relative to the cost of reading one sequential page from disk.
     *
     * Phase 1, Task 1.2: Cost Model
     */
    struct CostParameters
    {
        // I/O costs (relative to seq_page_cost = 1.0 baseline)
        double seq_page_cost = 1.0;        // Cost of sequential page read
        double random_page_cost = 4.0;     // Cost of random page read (4x sequential)

        // CPU costs (relative to seq_page_cost)
        double cpu_tuple_cost = 0.01;       // Cost of processing one row
        double cpu_index_tuple_cost = 0.005; // Cost of processing one index entry
        double cpu_operator_cost = 0.0025;   // Cost of evaluating an operator

        // Memory/sorting costs
        double sort_mem_cost = 0.0001;      // Cost per byte of sort memory

        // Parallelism (future use)
        double parallel_setup_cost = 1000.0;  // Cost of starting parallel workers
        double parallel_tuple_cost = 0.1;     // Extra cost per tuple for parallelism

        // Cache effectiveness
        double effective_cache_size = 16384.0; // Estimated OS cache size in pages (128MB @ 8KB/page)
    };

    /**
     * Cost Estimate - Result of cost estimation for a query plan
     *
     * Contains startup cost (one-time), run cost (per-execution), and row estimate.
     */
    struct CostEstimate
    {
        double startup_cost = 0.0;  // One-time setup cost
        double run_cost = 0.0;      // Per-execution cost
        double total_cost = 0.0;    // startup_cost + run_cost
        uint64_t rows = 0;          // Estimated rows returned

        // Constructor for convenience
        CostEstimate() = default;
        CostEstimate(double startup, double run, uint64_t row_count)
            : startup_cost(startup), run_cost(run),
              total_cost(startup + run), rows(row_count)
        {
        }
    };

    /**
     * Cost Model - Query plan cost estimation
     *
     * Estimates execution costs for different access methods:
     * - Sequential scans
     * - Index scans
     * - Joins (future)
     * - Sorts (future)
     *
     * Phase 1, Task 1.2
     */
    class CostModel
    {
    public:
        /**
         * Constructor with optional custom parameters
         *
         * @param params Cost parameters (uses PostgreSQL defaults if not provided)
         */
        explicit CostModel(const CostParameters &params = CostParameters{});

        /**
         * costSeqScan - Estimate cost of sequential table scan
         *
         * Cost formula:
         *   disk_cost = num_pages * seq_page_cost
         *   cpu_cost = num_tuples * (cpu_tuple_cost + qual_cost)
         *   total = disk_cost + cpu_cost
         *
         * @param num_pages Number of pages in table
         * @param num_tuples Number of tuples to process
         * @param qual_cost Cost of evaluating WHERE clause per tuple
         * @param ctx Error context
         * @return Cost estimate with startup, run, and total costs
         *
         * Phase 1, Task 1.2.2
         */
        auto costSeqScan(uint64_t num_pages, uint64_t num_tuples,
                         double qual_cost, core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * costIndexScan - Estimate cost of index scan
         *
         * Cost formula:
         *   startup = index_height * cpu_operator_cost (B-tree traversal)
         *   index_cost = index_pages * random_page_cost + index_tuples * cpu_index_tuple_cost
         *   heap_cost = heap_pages * effective_random_page_cost + heap_tuples * (cpu_tuple_cost + qual_cost)
         *   total = startup + index_cost + heap_cost
         *
         * @param index_height Height of B-tree index (2-4 typically)
         * @param index_pages Number of index pages accessed
         * @param index_tuples Number of index entries scanned
         * @param heap_pages Number of heap pages accessed
         * @param heap_tuples Number of heap tuples fetched
         * @param qual_cost Cost of evaluating WHERE clause per tuple
         * @param correlation Physical ordering correlation (-1.0 to 1.0, 0.0 = random)
         * @param ctx Error context
         * @return Cost estimate with startup, run, and total costs
         *
         * Phase 1, Task 1.2.3
         */
        auto costIndexScan(uint64_t index_height, uint64_t index_pages,
                           uint64_t index_tuples, uint64_t heap_pages,
                           uint64_t heap_tuples, double qual_cost,
                           double correlation = 0.0, core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * effectiveRandomPageCost - Calculate cache-adjusted random page cost
         *
         * When a table fits in cache, random access is nearly as cheap as sequential.
         *
         * Formula:
         *   cache_hit_ratio = min(1.0, effective_cache_size / table_pages)
         *   effective_cost = random_page_cost * (1 - cache_hit_ratio) + seq_page_cost * cache_hit_ratio
         *
         * @param table_pages Number of pages in table
         * @return Effective random page cost (between seq_page_cost and random_page_cost)
         *
         * Phase 1, Task 1.2.4
         */
        auto effectiveRandomPageCost(uint64_t table_pages) const -> double;

        /**
         * operatorCost - Get CPU cost for evaluating an operator
         *
         * Different operators have different costs:
         * - Simple comparisons (=, <, >, etc): cpu_operator_cost
         * - Arithmetic (*, /): 2x cpu_operator_cost
         * - String ops (LIKE): 10x cpu_operator_cost
         * - Functions (substr, concat): 5x cpu_operator_cost
         *
         * @param op Operator name (=, <, >, LIKE, etc.)
         * @return CPU cost for one evaluation
         *
         * Phase 1, Task 1.2.5
         */
        auto operatorCost(const std::string &op) const -> double;

        /**
         * costNestedLoopJoin - Estimate cost of nested loop join
         *
         * Cost formula:
         *   startup = outer_startup
         *   run = outer_run + (outer_rows * inner_total) + qual_cost
         *   qual_cost = outer_rows * inner_rows * selectivity * cpu_tuple_cost
         *   total = startup + run
         *   output_rows = outer_rows * inner_rows * selectivity
         *
         * For outer joins (LEFT, RIGHT, FULL), output_rows may be higher due to NULL padding.
         *
         * @param outer_cost Cost of scanning outer (left) relation
         * @param inner_cost Cost of scanning inner (right) relation (per outer row)
         * @param outer_rows Number of rows from outer relation
         * @param inner_rows Number of rows from inner relation
         * @param selectivity Selectivity of join condition (0.0-1.0)
         * @param ctx Error context
         * @return Cost estimate with startup, run, and total costs
         *
         * Phase 1, Task 3.2
         */
        auto costNestedLoopJoin(const CostEstimate& outer_cost,
                               const CostEstimate& inner_cost,
                               uint64_t outer_rows,
                               uint64_t inner_rows,
                               double selectivity,
                               core::ErrorContext* ctx = nullptr)
            -> CostEstimate;

        /**
         * costHashJoin - Estimate cost of hash join
         *
         * Cost formula:
         *   startup = outer_total + hash_build_cost
         *   hash_build_cost = outer_rows * cpu_tuple_cost * hash_build_factor
         *   run = inner_total + hash_probe_cost + qual_cost
         *   hash_probe_cost = inner_rows * cpu_tuple_cost * hash_probe_factor
         *   qual_cost = output_rows * cpu_tuple_cost
         *   total = startup + run
         *   output_rows = outer_rows * inner_rows * selectivity
         *
         * Hash join is only applicable for equi-joins (join condition contains =).
         * Memory usage: hash table size = outer_rows * row_width
         *
         * @param outer_cost Cost of scanning outer (build side) relation
         * @param inner_cost Cost of scanning inner (probe side) relation
         * @param outer_rows Number of rows from outer relation (build side)
         * @param inner_rows Number of rows from inner relation (probe side)
         * @param selectivity Selectivity of join condition (0.0-1.0)
         * @param ctx Error context
         * @return Cost estimate with startup, run, and total costs
         *
         * Phase 1, Task 3.2
         */
        auto costHashJoin(const CostEstimate& outer_cost,
                         const CostEstimate& inner_cost,
                         uint64_t outer_rows,
                         uint64_t inner_rows,
                         double selectivity,
                         core::ErrorContext* ctx = nullptr)
            -> CostEstimate;

        /**
         * Get current cost parameters
         */
        const CostParameters &parameters() const { return params_; }

    private:
        CostParameters params_;
    };

} // namespace scratchbird::optimizer
