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

} // namespace scratchbird::optimizer
