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
#include "scratchbird/core/error_context.h"
#include "scratchbird/parser/shared_types.h"
#include <cstdint>
#include <string>
#include <vector>

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
        uint64_t work_mem_bytes = 4 * 1024 * 1024; // Memory budget per operator
        uint64_t planner_page_size_bytes = 8192;   // Spill planning page size
        double spill_page_cost = 1.5;              // Cost per spilled temp page
        double spill_cpu_tuple_cost = 0.02;        // Extra CPU cost per spilled tuple
        double hash_mem_multiplier = 1.25;         // Hash joins may use a larger grant
        double merge_mem_multiplier = 1.0;         // Merge/sort operators use work_mem
        double aggregate_mem_multiplier = 1.0;     // Hash aggregate memory multiplier
        uint64_t default_row_width_bytes = 64;     // Fallback row width when unknown
        uint64_t hash_tuple_overhead_bytes = 24;   // Hash table bucket/entry overhead
        uint64_t sort_tuple_overhead_bytes = 16;   // Sort tuple pointer/metadata overhead

        // Parallelism (future use)
        double parallel_setup_cost = 1000.0;  // Cost of starting parallel workers
        double parallel_tuple_cost = 0.1;     // Extra cost per tuple for parallelism

        // Cache effectiveness
        double effective_cache_size = 16384.0; // Estimated OS cache size in pages (128MB @ 8KB/page)
    };

    struct CostInputEstimate
    {
        std::string name;
        double value = 0.0;
        std::string unit;
    };

    struct CostFormulaTerm
    {
        std::string name;
        double coefficient = 0.0;
        double input_value = 0.0;
        double contribution = 0.0;
        std::string unit;
    };

    struct CostFormulaProfile
    {
        std::string profile_id = "sb_cost_formula/default";
        uint32_t profile_version = 1;
        std::string calibration_profile_id = "sb_cost_calibration/default";
        std::string storage_profile = "heap_btree";
        std::string workload_profile = "mixed_oltp";
        CostParameters parameters;
    };

    struct IndexFamilyCostCalibrationInput
    {
        std::string planner_family;
        std::string metrics_type_name;
        uint32_t family_metrics_version = 0;
        std::string metrics_confidence_class;
        double correlation = 0.0;
        double bloat_ratio = 0.0;
        double recheck_ratio_est = 0.0;
        double coverage_fraction = 0.0;
        bool ordered_output = false;
        bool covering_index = false;
        bool requires_recheck = false;
    };

    auto deriveIndexFamilyFormulaProfile(
        const CostParameters &params,
        const IndexFamilyCostCalibrationInput &input) -> CostFormulaProfile;

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
        uint64_t memory_bytes = 0;  // Estimated working-set memory
        uint64_t memory_budget_bytes = 0; // Memory budget used for this operator
        bool spill_expected = false; // Whether temp spill is expected
        uint32_t spill_passes = 0;   // Estimated spill/repartition passes
        uint64_t spill_bytes = 0;    // Estimated temp footprint written to disk
        std::string operator_name;
        std::string formula_profile_id;
        uint32_t formula_profile_version = 0;
        std::string calibration_profile_id;
        std::string storage_profile;
        std::string workload_profile;
        std::string resource_governance_outcome;
        std::vector<CostInputEstimate> input_estimates;
        std::vector<CostFormulaTerm> expanded_terms;

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
        explicit CostModel(const CostFormulaProfile &profile);

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
         * costIndexOnlyScan - Estimate cost of covering index scan
         *
         * Similar to costIndexScan, but avoids heap fetches because every
         * required value can be read directly from the index payload.
         */
        auto costIndexOnlyScan(uint64_t index_height,
                               uint64_t index_pages,
                               uint64_t index_tuples,
                               double qual_cost,
                               double correlation = 0.0,
                               core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * costBitmapScan - Estimate cost of bitmap index scan
         *
         * Combines multiple index probes, materializes a bitmap of tuple IDs,
         * then visits heap pages in physical order.
         */
        auto costBitmapScan(uint64_t num_indexes,
                            uint64_t total_index_pages,
                            uint64_t heap_pages,
                            uint64_t heap_tuples,
                            double qual_cost,
                            const std::string &bitmap_op = "AND",
                            core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * costSummaryScan - Estimate cost of summary-family scans
         *
         * Used for BRIN and summary-filter families that narrow heap work down
         * to candidate ranges and require residual recheck.
         */
        auto costSummaryScan(uint64_t summary_pages_read,
                             uint64_t candidate_heap_pages,
                             uint64_t candidate_rows,
                             double qual_cost,
                             double unsummarized_range_fraction = 0.0,
                             double summary_staleness_fraction = 0.0,
                             core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * costBitmapStorageScan - Estimate cost of physical bitmap-family scans
         *
         * Distinct from synthetic bitmap combination. Models a stored bitmap
         * candidate path that may be lossy and require residual recheck.
         */
        auto costBitmapStorageScan(uint64_t bitmap_pages_read,
                                   uint64_t candidate_heap_pages,
                                   uint64_t candidate_rows,
                                   double qual_cost,
                                   double lossy_container_fraction = 0.0,
                                   double false_positive_ratio = 0.0,
                                   core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * costColumnstoreScan - Estimate cost of columnstore-family scans
         *
         * Models projected column reads, late materialization, and delta
         * penalties for candidate-region columnar access.
         */
        auto costColumnstoreScan(uint64_t bytes_read_est,
                                 uint64_t rows_materialized,
                                 double qual_cost,
                                 double column_bytes_pruned_ratio = 0.0,
                                 double late_materialization_gain_est = 0.0,
                                 double delta_fraction = 0.0,
                                 core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * costLSMScan - Estimate cost of LSM-Tree index scan
         *
         * LSM-Tree characteristics:
         * - Read path: Check memtable → immutable memtable → L0-L3 SSTables
         * - Point lookup: O(log N) memtable + up to 4 SSTable lookups (with Bloom filters)
         * - Range scan: K-way merge across memtable + SSTables
         * - Trade-off: Slower reads than B-Tree, but better write performance
         *
         * Cost formula:
         *   startup = memtable_cpu_cost + bloom_filter_checks
         *   index_cost = sstable_reads * random_page_cost + index_tuples * cpu_index_tuple_cost
         *   heap_cost = heap_pages * effective_random_page_cost + heap_tuples * (cpu_tuple_cost + qual_cost)
         *   total = startup + index_cost + heap_cost
         *
         * Key differences from B-Tree:
         * - No index_height (LSM is flat within each level)
         * - More random reads (multiple SSTables)
         * - Bloom filters reduce disk I/O for non-existent keys
         *
         * @param num_levels Number of LSM levels with data (0-4 typically)
         * @param avg_sstables_per_level Average SSTables per level to scan
         * @param index_tuples Number of index entries scanned
         * @param heap_pages Number of heap pages accessed
         * @param heap_tuples Number of heap tuples fetched
         * @param qual_cost Cost of evaluating WHERE clause per tuple
         * @param correlation Physical ordering correlation (-1.0 to 1.0, 0.0 = random)
         * @param ctx Error context
         * @return Cost estimate with startup, run, and total costs
         *
         * LSM Integration Phase 5, Task 5.2
         */
        auto costLSMScan(uint64_t num_levels, uint64_t avg_sstables_per_level,
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
                               parser::JoinType join_type = parser::JoinType::INNER,
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
                         parser::JoinType join_type = parser::JoinType::INNER,
                         core::ErrorContext* ctx = nullptr)
            -> CostEstimate;

        /**
         * costMergeJoin - Estimate cost of merge join
         *
         * Accounts for optional input sorting plus a linear merge pass.
         *
         * @param outer_cost Cost of scanning the outer relation
         * @param inner_cost Cost of scanning the inner relation
         * @param outer_rows Number of outer rows
         * @param inner_rows Number of inner rows
         * @param selectivity Selectivity of join condition (0.0-1.0)
         * @param outer_presorted True when outer input is already ordered on the join key
         * @param inner_presorted True when inner input is already ordered on the join key
         * @param join_type Type of join (currently INNER or LEFT)
         * @param ctx Error context
         * @return Cost estimate with startup, run, and total costs
         */
        auto costMergeJoin(const CostEstimate& outer_cost,
                           const CostEstimate& inner_cost,
                           uint64_t outer_rows,
                           uint64_t inner_rows,
                           double selectivity,
                           bool outer_presorted,
                           bool inner_presorted,
                           parser::JoinType join_type = parser::JoinType::INNER,
                           core::ErrorContext* ctx = nullptr)
            -> CostEstimate;

        /**
         * costAggregate - Estimate cost of aggregation with GROUP BY
         *
         * Cost formula:
         *   startup = input_cost.total + hash_build_cost
         *   hash_build_cost = input_rows * cpu_tuple_cost * 2.0 (hash computation + insertion)
         *   run = num_groups * (num_aggregates * cpu_operator_cost)
         *   total = startup + run
         *   output_rows = num_groups
         *
         * For simple aggregation (no GROUP BY), num_groups = 1.
         * For hash-based grouping, assumes hash table fits in memory.
         *
         * @param input_rows Number of input rows
         * @param num_groups Estimated number of groups (1 for simple aggregation)
         * @param num_aggregates Number of aggregate functions (COUNT, SUM, etc.)
         * @param ctx Error context
         * @return Cost estimate with startup, run, and total costs
         *
         * Phase 1, Task 4.1
         */
        auto costAggregate(uint64_t input_rows,
                          uint64_t num_groups,
                          uint64_t num_aggregates,
                          core::ErrorContext* ctx = nullptr)
            -> CostEstimate;

        /**
         * costGroupAggregate - Estimate cost of ordered/group aggregate
         *
         * Models streaming aggregation on presorted input, avoiding hash-table
         * build cost and large aggregation spill state.
         */
        auto costGroupAggregate(uint64_t input_rows,
                                uint64_t num_groups,
                                uint64_t num_aggregates,
                                core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * costDistinct - Estimate cost of DISTINCT elimination
         *
         * `presorted=true` models streaming duplicate elimination on already
         * ordered input. `presorted=false` models hash-based duplicate
         * elimination with spill estimation.
         */
        auto costDistinct(uint64_t input_rows,
                          uint64_t num_distinct_rows,
                          uint64_t num_distinct_keys,
                          bool presorted,
                          core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * costSort - Estimate cost of sorting
         *
         * Cost formula:
         *   startup = input_cost.total + sort_cost
         *   sort_cost = num_rows * log2(num_rows) * comparison_cost
         *   comparison_cost = num_sort_keys * cpu_operator_cost
         *   run = num_rows * cpu_tuple_cost (output)
         *   total = startup + run
         *   output_rows = num_rows
         *
         * Assumes in-memory quicksort. For external sorts (disk-based),
         * cost would be higher based on sort_mem_cost.
         *
         * @param num_rows Number of rows to sort
         * @param row_width Average row width in bytes
         * @param num_sort_keys Number of sort keys (ORDER BY expressions)
         * @param ctx Error context
         * @return Cost estimate with startup, run, and total costs
         *
         * Phase 1, Task 5.1
         */
        auto costSort(uint64_t num_rows,
                     uint64_t row_width,
                     uint64_t num_sort_keys,
                     core::ErrorContext* ctx = nullptr)
            -> CostEstimate;

        /**
         * costSort - Estimate cost of bounded top-N sort
         *
         * Uses a heap-style top-N model when a LIMIT/OFFSET bound is known.
         * `top_n_count` should be `limit + offset`.
         */
        auto costSort(uint64_t num_rows,
                      uint64_t row_width,
                      uint64_t num_sort_keys,
                      uint64_t top_n_count,
                      core::ErrorContext *ctx)
            -> CostEstimate;

        /**
         * costGather - Estimate cost of parallel gather
         *
         * Models a guarded parallel wrapper over an existing serial child path.
         * Worker startup penalty and tuple transfer cost are charged here.
         */
        auto costGather(const CostEstimate &input_cost,
                        uint64_t input_rows,
                        uint32_t workers_planned,
                        bool leader_participates = true,
                        core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * costGatherMerge - Estimate cost of parallel gather merge
         *
         * Extends gather costing with an ordered merge fan-in across workers.
         */
        auto costGatherMerge(const CostEstimate &input_cost,
                             uint64_t input_rows,
                             uint64_t num_order_keys,
                             uint32_t workers_planned,
                             bool leader_participates = true,
                             core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * costLimit - Estimate cost of LIMIT/OFFSET
         *
         * Cost formula:
         *   startup = input_cost.startup + (offset_count * cpu_tuple_cost)
         *   run = limit_count * cpu_tuple_cost
         *   total = startup + run
         *   output_rows = min(limit_count, input_rows - offset_count)
         *
         * LIMIT allows early termination of query execution.
         * OFFSET requires skipping rows but doesn't reduce input processing.
         *
         * @param input_rows Number of input rows
         * @param limit_count LIMIT value (-1 for no limit)
         * @param offset_count OFFSET value (-1 for no offset)
         * @param ctx Error context
         * @return Cost estimate with startup, run, and total costs
         *
         * Phase 1, Task 5.2
         */
        auto costLimit(uint64_t input_rows,
                      int64_t limit_count,
                      int64_t offset_count,
                      core::ErrorContext* ctx = nullptr)
            -> CostEstimate;

        /**
         * costWindow - Estimate cost of window function evaluation
         *
         * Accounts for required partition/order sorting plus per-row window
         * state maintenance.
         */
        auto costWindow(uint64_t input_rows,
                        uint64_t row_width,
                        uint64_t num_partition_keys,
                        uint64_t num_order_keys,
                        uint64_t num_window_functions,
                        bool input_presorted,
                        core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        auto costWindow(uint64_t input_rows,
                        uint64_t row_width,
                        uint64_t num_partition_keys,
                        uint64_t num_order_keys,
                        uint64_t num_window_functions,
                        core::ErrorContext *ctx = nullptr)
            -> CostEstimate;

        /**
         * Get current cost parameters
         */
        const CostParameters &parameters() const { return params_; }
        const CostFormulaProfile &formulaProfile() const { return formula_profile_; }

    private:
        CostParameters params_;
        CostFormulaProfile formula_profile_;
    };

} // namespace scratchbird::optimizer
