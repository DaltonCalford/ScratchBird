/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/core/debug.h"
#include <algorithm>
#include <cmath>

namespace scratchbird::optimizer
{

    CostModel::CostModel(const CostParameters &params)
        : params_(params)
    {
        DEBUG_LOG_DB("CostModel created with seq_page_cost=" +
                     std::to_string(params_.seq_page_cost) +
                     ", random_page_cost=" + std::to_string(params_.random_page_cost));
    }

    auto CostModel::costSeqScan(uint64_t num_pages, uint64_t num_tuples,
                                 double qual_cost, core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating sequential scan cost: pages=" + std::to_string(num_pages) +
                     ", tuples=" + std::to_string(num_tuples));

        CostEstimate cost;

        // Sequential scan has no startup cost (no index to traverse, no setup needed)
        cost.startup_cost = 0.0;

        // Disk cost: read all pages sequentially
        // This is the dominant cost for large tables
        double disk_cost = static_cast<double>(num_pages) * params_.seq_page_cost;

        // CPU cost: process all tuples + evaluate WHERE clause
        // cpu_tuple_cost = cost of materializing one tuple
        // qual_cost = cost of evaluating WHERE predicates per tuple
        double cpu_cost = static_cast<double>(num_tuples) * params_.cpu_tuple_cost +
                          static_cast<double>(num_tuples) * qual_cost;

        cost.run_cost = disk_cost + cpu_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = num_tuples;

        DEBUG_LOG_DB("SeqScan cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost));

        return cost;
    }

    auto CostModel::costIndexScan(uint64_t index_height, uint64_t index_pages,
                                   uint64_t index_tuples, uint64_t heap_pages,
                                   uint64_t heap_tuples, double qual_cost,
                                   double correlation, core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating index scan cost: index_height=" + std::to_string(index_height) +
                     ", index_pages=" + std::to_string(index_pages) +
                     ", heap_pages=" + std::to_string(heap_pages) +
                     ", correlation=" + std::to_string(correlation));

        CostEstimate cost;

        // Startup cost: traverse B-tree from root to first matching entry
        // For a B-tree of height H, we need to read H pages (root to leaf)
        cost.startup_cost = static_cast<double>(index_height) * params_.cpu_operator_cost;

        // Index scan cost: read index pages and process index tuples
        // Index pages are typically accessed randomly (not sequential)
        double index_io_cost = static_cast<double>(index_pages) * params_.random_page_cost;
        double index_cpu_cost = static_cast<double>(index_tuples) * params_.cpu_index_tuple_cost;

        // Heap fetch cost: depends on physical ordering correlation
        // If index and heap are well-correlated, heap access is more sequential
        // If poorly correlated, heap access is random

        double effective_heap_pages;

        if (std::abs(correlation) > 0.8)
        {
            // Well-correlated (correlation > 0.8 or < -0.8)
            // Heap accesses are mostly sequential
            // Estimate: one page per ~100 tuples (typical for 8KB pages)
            constexpr double TUPLES_PER_PAGE = 100.0;
            effective_heap_pages = std::ceil(static_cast<double>(heap_tuples) / TUPLES_PER_PAGE);
        }
        else
        {
            // Poorly correlated (|correlation| <= 0.8)
            // Assume worst case: random heap access
            // Each tuple might be on a different page
            effective_heap_pages = std::min(static_cast<double>(heap_tuples),
                                            static_cast<double>(heap_pages));
        }

        // Apply cache effect to heap I/O cost
        double effective_random_cost = effectiveRandomPageCost(heap_pages);
        double heap_io_cost = effective_heap_pages * effective_random_cost;

        // CPU cost for heap tuples
        double heap_cpu_cost = static_cast<double>(heap_tuples) * params_.cpu_tuple_cost +
                               static_cast<double>(heap_tuples) * qual_cost;

        cost.run_cost = index_io_cost + index_cpu_cost + heap_io_cost + heap_cpu_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = heap_tuples;

        DEBUG_LOG_DB("IndexScan cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     " (index_io=" + std::to_string(index_io_cost) +
                     ", heap_io=" + std::to_string(heap_io_cost) + ")");

        return cost;
    }

    auto CostModel::costIndexOnlyScan(uint64_t index_height,
                                      uint64_t index_pages,
                                      uint64_t index_tuples,
                                      double qual_cost,
                                      double correlation,
                                      core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating index-only scan cost: index_height=" +
                     std::to_string(index_height) +
                     ", index_pages=" + std::to_string(index_pages) +
                     ", index_tuples=" + std::to_string(index_tuples) +
                     ", correlation=" + std::to_string(correlation));

        CostEstimate cost;
        cost.startup_cost = static_cast<double>(index_height) * params_.cpu_operator_cost;

        const double locality_discount =
            std::abs(correlation) > 0.8 ? 0.65 : (std::abs(correlation) > 0.4 ? 0.8 : 1.0);
        const double index_io_cost =
            static_cast<double>(index_pages) * params_.random_page_cost * locality_discount;
        const double index_cpu_cost =
            static_cast<double>(index_tuples) * params_.cpu_index_tuple_cost;
        const double visibility_cost =
            static_cast<double>(index_tuples) * params_.cpu_tuple_cost * 0.25;
        const double qual_cpu_cost =
            static_cast<double>(index_tuples) * qual_cost;

        cost.run_cost = index_io_cost + index_cpu_cost + visibility_cost + qual_cpu_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = index_tuples;
        return cost;
    }

    auto CostModel::costBitmapScan(uint64_t num_indexes,
                                   uint64_t total_index_pages,
                                   uint64_t heap_pages,
                                   uint64_t heap_tuples,
                                   double qual_cost,
                                   const std::string &bitmap_op,
                                   core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating bitmap scan cost: indexes=" + std::to_string(num_indexes) +
                     ", total_index_pages=" + std::to_string(total_index_pages) +
                     ", heap_pages=" + std::to_string(heap_pages) +
                     ", tuples=" + std::to_string(heap_tuples) +
                     ", bitmap_op=" + bitmap_op);

        CostEstimate cost;
        const double bitmap_build_cost =
            static_cast<double>(heap_tuples) *
            (params_.cpu_index_tuple_cost + params_.cpu_operator_cost * 0.5);
        const double bitmap_merge_cost =
            static_cast<double>(std::max<uint64_t>(1, num_indexes - 1)) *
            static_cast<double>(heap_tuples) * params_.cpu_operator_cost * 0.35;

        cost.startup_cost = bitmap_build_cost + bitmap_merge_cost;

        const double index_io_cost =
            static_cast<double>(total_index_pages) * params_.random_page_cost;
        const double heap_io_cost =
            static_cast<double>(heap_pages) *
            std::min(params_.random_page_cost,
                     params_.seq_page_cost + effectiveRandomPageCost(heap_pages) * 0.5);
        const double qual_cpu_cost =
            static_cast<double>(heap_tuples) * (params_.cpu_tuple_cost + qual_cost);

        cost.run_cost = index_io_cost + heap_io_cost + qual_cpu_cost;
        if (bitmap_op == "OR")
        {
            cost.run_cost *= 1.08;
        }
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = heap_tuples;
        return cost;
    }

    auto CostModel::costLSMScan(uint64_t num_levels, uint64_t avg_sstables_per_level,
                                 uint64_t index_tuples, uint64_t heap_pages,
                                 uint64_t heap_tuples, double qual_cost,
                                 double correlation, core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating LSM scan cost: num_levels=" + std::to_string(num_levels) +
                     ", avg_sstables_per_level=" + std::to_string(avg_sstables_per_level) +
                     ", heap_pages=" + std::to_string(heap_pages) +
                     ", correlation=" + std::to_string(correlation));

        CostEstimate cost;

        // Startup cost: LSM-Tree read path
        // 1. Check memtable (in-memory, very cheap)
        double memtable_cost = 10.0 * params_.cpu_operator_cost;  // ~10 comparisons for RB-tree lookup

        // 2. Check immutable memtable (in-memory, very cheap)
        double immutable_memtable_cost = 10.0 * params_.cpu_operator_cost;

        // 3. Bloom filter checks for each SSTable level (CPU only, no I/O if negative)
        // Each Bloom filter check is ~3-5 hash computations
        uint64_t total_sstables = num_levels * avg_sstables_per_level;
        double bloom_filter_cost = static_cast<double>(total_sstables) * 4.0 * params_.cpu_operator_cost;

        cost.startup_cost = memtable_cost + immutable_memtable_cost + bloom_filter_cost;

        // Index scan cost: read SSTable pages
        // Key insight: LSM-Tree may need to read from multiple SSTables per level
        // Each SSTable requires random page reads (B-Tree within SSTable)
        // Bloom filters reduce false positives, assume 30% false positive rate
        constexpr double BLOOM_FALSE_POSITIVE_RATE = 0.30;
        double expected_sstable_reads = static_cast<double>(total_sstables) * BLOOM_FALSE_POSITIVE_RATE;

        // Each SSTable read: 1-2 pages for internal B-Tree (small index within SSTable)
        constexpr double AVG_PAGES_PER_SSTABLE_READ = 1.5;
        uint64_t estimated_index_pages = static_cast<uint64_t>(expected_sstable_reads * AVG_PAGES_PER_SSTABLE_READ);

        double index_io_cost = static_cast<double>(estimated_index_pages) * params_.random_page_cost;
        double index_cpu_cost = static_cast<double>(index_tuples) * params_.cpu_index_tuple_cost;

        // OPT-M7: Additional CPU cost for k-way merge if range scan
        // K-way merge is needed when range scans must merge results from multiple SSTables
        // Cost: O(n * log(k)) where n = tuples, k = number of SSTables to merge
        // For point lookups (index_tuples <= 1), no merge needed
        double merge_cost = 0.0;
        if (index_tuples > 1 && total_sstables > 1)
        {
            // K-way merge CPU cost: each tuple requires log(k) comparisons to find next
            // in the merge heap, where k = number of overlapping SSTables
            double log_k = std::log2(static_cast<double>(total_sstables));
            merge_cost = static_cast<double>(index_tuples) * log_k * params_.cpu_operator_cost;

            DEBUG_LOG_DB("LSM merge cost for " + std::to_string(index_tuples) +
                         " tuples across " + std::to_string(total_sstables) +
                         " SSTables: " + std::to_string(merge_cost));
        }

        // Heap fetch cost: same as B-Tree
        double effective_heap_pages;
        if (std::abs(correlation) > 0.8)
        {
            constexpr double TUPLES_PER_PAGE = 100.0;
            effective_heap_pages = std::ceil(static_cast<double>(heap_tuples) / TUPLES_PER_PAGE);
        }
        else
        {
            effective_heap_pages = std::min(static_cast<double>(heap_tuples),
                                            static_cast<double>(heap_pages));
        }

        double effective_random_cost = effectiveRandomPageCost(heap_pages);
        double heap_io_cost = effective_heap_pages * effective_random_cost;
        double heap_cpu_cost = static_cast<double>(heap_tuples) * params_.cpu_tuple_cost +
                               static_cast<double>(heap_tuples) * qual_cost;

        cost.run_cost = index_io_cost + index_cpu_cost + merge_cost + heap_io_cost + heap_cpu_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = heap_tuples;

        DEBUG_LOG_DB("LSMScan cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     " (index_io=" + std::to_string(index_io_cost) +
                     ", merge=" + std::to_string(merge_cost) +
                     ", heap_io=" + std::to_string(heap_io_cost) +
                     ", bloom_checks=" + std::to_string(bloom_filter_cost) + ")");

        return cost;
    }

    auto CostModel::effectiveRandomPageCost(uint64_t table_pages) const -> double
    {
        if (table_pages == 0)
        {
            return params_.random_page_cost;
        }

        // Calculate cache hit ratio
        // If entire table fits in cache (effective_cache_size > table_pages),
        // cache hit ratio = 1.0 (all accesses are cache hits)
        double cache_hit_ratio = std::min(1.0, params_.effective_cache_size /
                                                   static_cast<double>(table_pages));

        // Blend random and sequential costs based on cache hit ratio
        // When cache_hit_ratio = 1.0 (fully cached), cost = seq_page_cost (fast)
        // When cache_hit_ratio = 0.0 (not cached), cost = random_page_cost (slow)
        double effective_cost = params_.random_page_cost * (1.0 - cache_hit_ratio) +
                                params_.seq_page_cost * cache_hit_ratio;

        DEBUG_LOG_DB("Effective random page cost for " + std::to_string(table_pages) +
                     " pages: " + std::to_string(effective_cost) +
                     " (cache_hit_ratio=" + std::to_string(cache_hit_ratio) + ")");

        return effective_cost;
    }

    auto CostModel::operatorCost(const std::string &op) const -> double
    {
        // Simple comparison operators: cheapest
        if (op == "=" || op == "!=" || op == "<" || op == ">" ||
            op == "<=" || op == ">=")
        {
            return params_.cpu_operator_cost;
        }

        // Arithmetic operators: cheap
        if (op == "+" || op == "-")
        {
            return params_.cpu_operator_cost;
        }

        // Multiplication and division: slightly more expensive
        if (op == "*" || op == "/" || op == "%")
        {
            return params_.cpu_operator_cost * 2.0;
        }

        // String operations: expensive (pattern matching)
        if (op == "LIKE" || op == "ILIKE" || op == "~" || op == "~*")
        {
            return params_.cpu_operator_cost * 10.0;
        }

        // IN operator: cost depends on list size, but estimate moderate cost
        if (op == "IN" || op == "NOT IN")
        {
            return params_.cpu_operator_cost * 5.0;
        }

        // BETWEEN: two comparisons
        if (op == "BETWEEN")
        {
            return params_.cpu_operator_cost * 2.0;
        }

        // IS NULL / IS NOT NULL: very cheap (just a flag check)
        if (op == "IS NULL" || op == "IS NOT NULL")
        {
            return params_.cpu_operator_cost * 0.5;
        }

        // Logical operators: combine costs of subexpressions
        if (op == "AND" || op == "OR" || op == "NOT")
        {
            return params_.cpu_operator_cost * 0.5;
        }

        // String functions: moderately expensive
        if (op == "substr" || op == "substring" || op == "concat" ||
            op == "lower" || op == "upper" || op == "trim")
        {
            return params_.cpu_operator_cost * 5.0;
        }

        // Aggregate functions: expensive (but amortized over groups)
        if (op == "sum" || op == "avg" || op == "count" ||
            op == "min" || op == "max")
        {
            return params_.cpu_operator_cost * 3.0;
        }

        // Math functions: moderately expensive
        if (op == "abs" || op == "round" || op == "floor" || op == "ceil" ||
            op == "sqrt" || op == "exp" || op == "log" || op == "pow")
        {
            return params_.cpu_operator_cost * 3.0;
        }

        // Default: use base cpu_operator_cost
        DEBUG_LOG_DB("Unknown operator '" + op + "', using default cost");
        return params_.cpu_operator_cost;
    }

    auto CostModel::costNestedLoopJoin(const CostEstimate& outer_cost,
                                       const CostEstimate& inner_cost,
                                       uint64_t outer_rows,
                                       uint64_t inner_rows,
                                       double selectivity,
                                       parser::JoinType join_type,
                                       core::ErrorContext* ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating nested loop join cost: outer_rows=" + std::to_string(outer_rows) +
                     ", inner_rows=" + std::to_string(inner_rows) +
                     ", selectivity=" + std::to_string(selectivity));

        CostEstimate cost;

        // Startup cost: just the outer relation startup
        // Inner relation is re-scanned for each outer row, so no one-time startup
        cost.startup_cost = outer_cost.startup_cost;

        // Run cost breakdown:
        // 1. Scan outer relation completely
        double outer_scan_cost = outer_cost.run_cost;

        // 2. For each outer row, scan inner relation (inner is re-scanned outer_rows times)
        //    Note: inner_cost.total_cost includes both startup and run for each inner scan
        double inner_scan_cost = static_cast<double>(outer_rows) * inner_cost.total_cost;

        // 3. CPU cost of evaluating join condition and materializing output
        //    Join produces: outer_rows * inner_rows * selectivity output rows
        uint64_t output_rows = 0;
        if (join_type == parser::JoinType::CROSS)
        {
            output_rows = outer_rows * inner_rows;
        }
        else
        {
            output_rows = static_cast<uint64_t>(
                static_cast<double>(outer_rows) * static_cast<double>(inner_rows) * selectivity);
        }
        if (outer_rows > 0 && inner_rows > 0 && output_rows == 0 &&
            join_type != parser::JoinType::LEFT &&
            join_type != parser::JoinType::RIGHT &&
            join_type != parser::JoinType::FULL)
        {
            output_rows = 1;
        }
        if (join_type == parser::JoinType::LEFT)
        {
            output_rows = std::max<uint64_t>(outer_rows, output_rows);
        }
        else if (join_type == parser::JoinType::RIGHT)
        {
            output_rows = std::max<uint64_t>(inner_rows, output_rows);
        }
        else if (join_type == parser::JoinType::FULL)
        {
            output_rows = std::max<uint64_t>(std::max<uint64_t>(outer_rows, inner_rows),
                                             output_rows);
        }

        // Cost of evaluating join condition for each combination
        // (before selectivity filtering)
        double join_qual_cost = static_cast<double>(outer_rows) *
                               static_cast<double>(inner_rows) *
                               params_.cpu_operator_cost;

        // Cost of materializing output tuples (after selectivity filtering)
        double output_cost = static_cast<double>(output_rows) * params_.cpu_tuple_cost;

        cost.run_cost = outer_scan_cost + inner_scan_cost + join_qual_cost + output_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = output_rows;

        DEBUG_LOG_DB("NestedLoopJoin cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", output_rows=" + std::to_string(cost.rows) +
                     " (outer_scan=" + std::to_string(outer_scan_cost) +
                     ", inner_scan=" + std::to_string(inner_scan_cost) +
                     ", join_qual=" + std::to_string(join_qual_cost) + ")");

        return cost;
    }

    auto CostModel::costHashJoin(const CostEstimate& outer_cost,
                                 const CostEstimate& inner_cost,
                                 uint64_t outer_rows,
                                 uint64_t inner_rows,
                                 double selectivity,
                                 parser::JoinType join_type,
                                 core::ErrorContext* ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating hash join cost: outer_rows=" + std::to_string(outer_rows) +
                     ", inner_rows=" + std::to_string(inner_rows) +
                     ", selectivity=" + std::to_string(selectivity));

        CostEstimate cost;

        // Hash join phases:
        // Phase 1 (Build): Scan outer relation and build hash table
        // Phase 2 (Probe): Scan inner relation and probe hash table

        // Build phase cost:
        // 1. Scan outer relation completely
        double outer_scan_cost = outer_cost.total_cost;

        // 2. Build hash table from outer rows
        //    Hash insertion: compute hash + insert into bucket
        //    Factor of 2.0 represents hash function computation + insertion overhead
        constexpr double HASH_BUILD_FACTOR = 2.0;
        double hash_build_cost = static_cast<double>(outer_rows) *
                                params_.cpu_tuple_cost * HASH_BUILD_FACTOR;

        // Startup cost = build entire hash table
        cost.startup_cost = outer_scan_cost + hash_build_cost;

        // Probe phase cost:
        // 1. Scan inner relation completely
        double inner_scan_cost = inner_cost.total_cost;

        // 2. Probe hash table for each inner row
        //    Hash lookup: compute hash + traverse bucket chain
        //    Factor of 1.5 represents hash function computation + lookup overhead
        constexpr double HASH_PROBE_FACTOR = 1.5;
        double hash_probe_cost = static_cast<double>(inner_rows) *
                                params_.cpu_tuple_cost * HASH_PROBE_FACTOR;

        // 3. CPU cost of evaluating join condition and materializing output
        uint64_t output_rows = 0;
        if (join_type == parser::JoinType::CROSS)
        {
            output_rows = outer_rows * inner_rows;
        }
        else
        {
            output_rows = static_cast<uint64_t>(
                static_cast<double>(outer_rows) * static_cast<double>(inner_rows) * selectivity);
        }
        if (outer_rows > 0 && inner_rows > 0 && output_rows == 0 &&
            join_type != parser::JoinType::LEFT &&
            join_type != parser::JoinType::RIGHT &&
            join_type != parser::JoinType::FULL)
        {
            output_rows = 1;
        }
        if (join_type == parser::JoinType::LEFT)
        {
            output_rows = std::max<uint64_t>(outer_rows, output_rows);
        }
        else if (join_type == parser::JoinType::RIGHT)
        {
            output_rows = std::max<uint64_t>(inner_rows, output_rows);
        }
        else if (join_type == parser::JoinType::FULL)
        {
            output_rows = std::max<uint64_t>(std::max<uint64_t>(outer_rows, inner_rows),
                                             output_rows);
        }

        // For hash join, we only evaluate join condition for matching hash buckets
        // (much cheaper than nested loop which evaluates for all combinations)
        // Estimate: evaluate for ~10% of combinations (hash collisions + matches)
        double join_qual_cost = static_cast<double>(outer_rows) *
                               static_cast<double>(inner_rows) *
                               selectivity * 10.0 *  // hash collision factor
                               params_.cpu_operator_cost;

        // Cost of materializing output tuples
        double output_cost = static_cast<double>(output_rows) * params_.cpu_tuple_cost;

        cost.run_cost = inner_scan_cost + hash_probe_cost + join_qual_cost + output_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = output_rows;

        DEBUG_LOG_DB("HashJoin cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", output_rows=" + std::to_string(cost.rows) +
                     " (hash_build=" + std::to_string(hash_build_cost) +
                     ", hash_probe=" + std::to_string(hash_probe_cost) +
                     ", join_qual=" + std::to_string(join_qual_cost) + ")");

        return cost;
    }

    auto CostModel::costAggregate(uint64_t input_rows,
                                  uint64_t num_groups,
                                  uint64_t num_aggregates,
                                  core::ErrorContext* ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating aggregate cost: input_rows=" + std::to_string(input_rows) +
                     ", num_groups=" + std::to_string(num_groups) +
                     ", num_aggregates=" + std::to_string(num_aggregates));

        CostEstimate cost;

        // Hash-based aggregation uses a hash table to group rows
        // For simple aggregation (no GROUP BY), num_groups = 1

        // Startup cost: build hash table and compute aggregates
        // Each input row is hashed and inserted into hash table
        // Factor of 2.0 represents hash computation + hash table insertion
        constexpr double HASH_AGG_FACTOR = 2.0;
        double hash_build_cost = static_cast<double>(input_rows) *
                                params_.cpu_tuple_cost * HASH_AGG_FACTOR;

        cost.startup_cost = hash_build_cost;

        // Run cost: finalize aggregates for each group
        // For each group, we need to compute final aggregate values
        // (e.g., AVG = sum/count, finalize accumulators)
        double finalize_cost = static_cast<double>(num_groups) *
                              static_cast<double>(num_aggregates) *
                              params_.cpu_operator_cost;

        // Output cost: materialize result rows
        double output_cost = static_cast<double>(num_groups) * params_.cpu_tuple_cost;

        cost.run_cost = finalize_cost + output_cost;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = num_groups;

        DEBUG_LOG_DB("Aggregate cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", output_rows=" + std::to_string(cost.rows) +
                     " (hash_build=" + std::to_string(hash_build_cost) +
                     ", finalize=" + std::to_string(finalize_cost) + ")");

        return cost;
    }

    auto CostModel::costSort(uint64_t num_rows,
                            uint64_t row_width,
                            uint64_t num_sort_keys,
                            core::ErrorContext* ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating sort cost: num_rows=" + std::to_string(num_rows) +
                     ", row_width=" + std::to_string(row_width) +
                     ", num_sort_keys=" + std::to_string(num_sort_keys));

        CostEstimate cost;

        if (num_rows == 0)
        {
            // Empty input, no cost
            cost.startup_cost = 0.0;
            cost.run_cost = 0.0;
            cost.total_cost = 0.0;
            cost.rows = 0;
            return cost;
        }

        // Sort algorithm: in-memory quicksort
        // Time complexity: O(n log n) comparisons
        // Each comparison evaluates num_sort_keys expressions

        // Number of comparisons for quicksort: n * log2(n)
        double num_comparisons = static_cast<double>(num_rows) *
                                std::log2(static_cast<double>(num_rows));

        // Cost per comparison: evaluate all sort key expressions
        double comparison_cost = static_cast<double>(num_sort_keys) * params_.cpu_operator_cost;

        // Total sort cost
        double sort_cost = num_comparisons * comparison_cost;

        // Memory cost: if data doesn't fit in memory, external sort is needed
        // For now, assume in-memory sort (external sort would add I/O cost)
        uint64_t memory_bytes = num_rows * row_width;
        double memory_cost = static_cast<double>(memory_bytes) * params_.sort_mem_cost;

        // Startup cost: perform the sort
        cost.startup_cost = sort_cost + memory_cost;

        // Run cost: output sorted rows (sequential scan)
        cost.run_cost = static_cast<double>(num_rows) * params_.cpu_tuple_cost;

        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = num_rows;

        DEBUG_LOG_DB("Sort cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", output_rows=" + std::to_string(cost.rows) +
                     " (sort=" + std::to_string(sort_cost) +
                     ", comparisons=" + std::to_string(num_comparisons) +
                     ", memory=" + std::to_string(memory_cost) + ")");

        return cost;
    }

    auto CostModel::costLimit(uint64_t input_rows,
                             int64_t limit_count,
                             int64_t offset_count,
                             core::ErrorContext* ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating limit cost: input_rows=" + std::to_string(input_rows) +
                     ", limit=" + std::to_string(limit_count) +
                     ", offset=" + std::to_string(offset_count));

        CostEstimate cost;

        // Handle offset
        uint64_t offset = 0;
        if (offset_count > 0)
        {
            offset = static_cast<uint64_t>(offset_count);
        }

        // OFFSET requires scanning and discarding rows
        double offset_cost = static_cast<double>(offset) * params_.cpu_tuple_cost;
        cost.startup_cost = offset_cost;

        // Calculate output rows after offset
        uint64_t rows_after_offset = (offset >= input_rows) ? 0 : (input_rows - offset);

        // Handle limit
        uint64_t output_rows = rows_after_offset;
        if (limit_count >= 0)
        {
            output_rows = std::min(rows_after_offset, static_cast<uint64_t>(limit_count));
        }

        // Run cost: materialize limited output rows
        // LIMIT allows early termination, so we only process output_rows
        cost.run_cost = static_cast<double>(output_rows) * params_.cpu_tuple_cost;

        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = output_rows;

        DEBUG_LOG_DB("Limit cost: startup=" + std::to_string(cost.startup_cost) +
                     ", run=" + std::to_string(cost.run_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", output_rows=" + std::to_string(cost.rows) +
                     " (offset_rows=" + std::to_string(offset) + ")");

        return cost;
    }

    auto CostModel::costWindow(uint64_t input_rows,
                               uint64_t row_width,
                               uint64_t num_partition_keys,
                               uint64_t num_order_keys,
                               uint64_t num_window_functions,
                               core::ErrorContext *ctx)
        -> CostEstimate
    {
        DEBUG_LOG_DB("Estimating window cost: input_rows=" + std::to_string(input_rows) +
                     ", row_width=" + std::to_string(row_width) +
                     ", partition_keys=" + std::to_string(num_partition_keys) +
                     ", order_keys=" + std::to_string(num_order_keys) +
                     ", funcs=" + std::to_string(num_window_functions));

        CostEstimate cost;
        if (input_rows == 0)
        {
            return cost;
        }

        const bool requires_sort = num_partition_keys > 0 || num_order_keys > 0;
        double sort_cost = 0.0;
        if (requires_sort)
        {
            const auto sort = costSort(input_rows,
                                       row_width,
                                       std::max<uint64_t>(1, num_partition_keys + num_order_keys),
                                       ctx);
            sort_cost = sort.total_cost;
        }

        const double partition_cpu =
            static_cast<double>(input_rows) *
            static_cast<double>(std::max<uint64_t>(1, num_partition_keys)) *
            params_.cpu_operator_cost * 0.5;
        const double function_cpu =
            static_cast<double>(input_rows) *
            static_cast<double>(std::max<uint64_t>(1, num_window_functions)) *
            (params_.cpu_operator_cost + params_.cpu_tuple_cost * 0.5);

        cost.startup_cost = sort_cost;
        cost.run_cost = partition_cpu + function_cpu;
        cost.total_cost = cost.startup_cost + cost.run_cost;
        cost.rows = input_rows;
        return cost;
    }

} // namespace scratchbird::optimizer
