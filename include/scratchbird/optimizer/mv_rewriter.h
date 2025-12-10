/**
 * mv_rewriter.h - Materialized View Query Rewriter
 *
 * V2 MIGRATION STATUS: COMPLETE
 *
 * P3-15: Automatic query rewriting to use materialized views.
 * When a query could be answered using a materialized view instead of
 * scanning base tables, this optimizer substitutes the MV for better performance.
 *
 * Rewriting Algorithm:
 * 1. Extract query pattern (tables, columns, predicates, aggregates)
 * 2. Search for materialized views that match or subsume the pattern
 * 3. Check freshness requirements
 * 4. Estimate cost of using MV vs base tables
 * 5. If MV is cheaper and fresh enough, rewrite query to use MV
 *
 * Supported query patterns:
 * - Simple SELECT with WHERE predicates
 * - Aggregate queries with GROUP BY
 * - Join queries where MV covers the join
 *
 * Limitations:
 * - Only considers MVs that fully cover the query
 * - No partial MV usage (future enhancement)
 * - Staleness tolerance is configurable (default: infinite)
 */

#ifndef SCRATCHBIRD_OPTIMIZER_MV_REWRITER_H
#define SCRATCHBIRD_OPTIMIZER_MV_REWRITER_H

#include "scratchbird/optimizer/cost_model.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/parser/ast_v2.h"
#include "scratchbird/sblr/resolved_ast_v2.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_set>
#include <chrono>

namespace scratchbird::optimizer
{

/**
 * QueryPattern - Represents the structure of a query for MV matching
 */
struct QueryPattern
{
    // Tables referenced in FROM clause
    std::vector<core::ID> base_table_ids;
    std::vector<std::string> table_names;

    // Columns selected (including computed expressions)
    std::vector<std::string> select_columns;

    // Columns in WHERE predicates
    std::vector<std::string> predicate_columns;

    // Columns in GROUP BY (if aggregate query)
    std::vector<std::string> group_by_columns;

    // Aggregate functions used
    std::vector<std::string> aggregates;  // e.g., "SUM", "COUNT", "AVG"

    // Original WHERE clause expression (V2 type)
    parser::v2::ResolvedExpression* where_clause = nullptr;

    // Is this an aggregate query?
    bool is_aggregate = false;

    // Hash for quick comparison
    size_t pattern_hash = 0;

    void computeHash();
};

/**
 * MVCandidate - A materialized view that might satisfy a query
 */
struct MVCandidate
{
    core::CatalogManager::ViewInfo mv_info;
    QueryPattern mv_pattern;
    double estimated_cost;
    double staleness_seconds;  // Time since last refresh
    bool is_exact_match;       // True if MV exactly matches query pattern
    bool is_subsumption_match; // True if MV subsumes query (has more columns/data)
};

/**
 * MVRewriter - Rewrites queries to use materialized views when beneficial (V2 Complete)
 */
class MVRewriter
{
public:
    // Default maximum staleness tolerance (0 = infinite, i.e., use MV regardless of freshness)
    static constexpr double DEFAULT_MAX_STALENESS_SECONDS = 0.0;

    /**
     * Constructor
     * @param db Database instance for catalog access
     * @param cost_model Cost model for comparing plan costs
     * @param stats_manager Statistics manager for cardinality estimation
     */
    MVRewriter(core::Database* db,
               CostModel& cost_model,
               StatisticsManager* stats_manager);

    /**
     * Set maximum staleness tolerance
     * @param seconds Maximum age of MV data (0 = infinite)
     */
    void setMaxStaleness(double seconds) { max_staleness_seconds_ = seconds; }

    /**
     * Try to rewrite a SELECT query to use a materialized view (V2 API)
     *
     * @param select_stmt Original SELECT statement (V2 unresolved AST)
     * @param string_pool String pool for identifier resolution
     * @param arena AST arena for allocation
     * @param ctx Error context
     * @return Rewritten SELECT (using MV) or nullptr if no beneficial rewrite found
     */
    parser::v2::SelectStmt* tryRewrite(const parser::v2::SelectStmt* select_stmt,
                                       parser::v2::StringPool& string_pool,
                                       parser::v2::ASTArena& arena,
                                       core::ErrorContext* ctx = nullptr);

    /**
     * Get candidate MVs for a query pattern
     *
     * @param pattern Query pattern to match
     * @param ctx Error context
     * @return List of candidate MVs, sorted by estimated cost
     */
    std::vector<MVCandidate> findCandidates(const QueryPattern& pattern,
                                            core::ErrorContext* ctx = nullptr);

    /**
     * Check if a materialized view pattern subsumes a query pattern
     * (i.e., the MV contains all the data needed to answer the query)
     *
     * @param mv_pattern MV pattern
     * @param query_pattern Query pattern
     * @return true if MV subsumes query
     */
    bool checkSubsumption(const QueryPattern& mv_pattern,
                          const QueryPattern& query_pattern);

    /**
     * Extract query pattern from a SELECT statement (V2 API)
     *
     * @param select_stmt SELECT statement (V2 unresolved AST)
     * @param string_pool String pool
     * @param ctx Error context
     * @return Query pattern
     */
    QueryPattern extractPattern(const parser::v2::SelectStmt* select_stmt,
                               const parser::v2::StringPool& string_pool,
                               core::ErrorContext* ctx = nullptr);

    /**
     * Get statistics about rewriting attempts
     */
    size_t rewriteAttempts() const { return rewrite_attempts_; }
    size_t rewriteSuccesses() const { return rewrite_successes_; }
    double avgCostSavings() const { return total_cost_savings_ / std::max(rewrite_successes_, size_t(1)); }

private:
    core::Database* db_;
    CostModel& cost_model_;
    StatisticsManager* stats_manager_;
    double max_staleness_seconds_;

    // Statistics
    size_t rewrite_attempts_ = 0;
    size_t rewrite_successes_ = 0;
    double total_cost_savings_ = 0.0;

    // Cache of parsed MV patterns (MV ID -> pattern)
    std::unordered_map<core::ID, QueryPattern> mv_pattern_cache_;

    /**
     * Parse MV definition to extract its pattern
     */
    QueryPattern parseMVPattern(const core::CatalogManager::ViewInfo& mv_info,
                               core::ErrorContext* ctx);

    /**
     * OPT-4: Extract pattern from MV definition string using V2 parser
     */
    bool extractPatternFromDefinition(const std::string& definition,
                                      QueryPattern& pattern_out);

    /**
     * Estimate cost of scanning a materialized view
     */
    double estimateMVScanCost(const core::CatalogManager::ViewInfo& mv_info,
                             core::ErrorContext* ctx);

    /**
     * Estimate cost of original query plan
     */
    double estimateOriginalCost(const QueryPattern& pattern,
                               core::ErrorContext* ctx);

    /**
     * Create rewritten SELECT that scans MV instead of base tables (V2 API)
     */
    parser::v2::SelectStmt* createMVSelect(const core::CatalogManager::ViewInfo& mv_info,
                                           const parser::v2::SelectStmt* original,
                                           parser::v2::StringPool& string_pool,
                                           parser::v2::ASTArena& arena,
                                           core::ErrorContext* ctx);

    /**
     * Extract column names from SELECT list (V2 API)
     */
    void extractSelectColumns(const parser::v2::SelectStmt* stmt,
                             const parser::v2::StringPool& string_pool,
                             std::vector<std::string>& columns);

    /**
     * Extract column names from expression (WHERE clause) (V2 API)
     */
    void extractPredicateColumns(const parser::v2::Expression* expr,
                                const parser::v2::StringPool& string_pool,
                                std::vector<std::string>& columns);

    /**
     * Check if two column sets match (ignoring order)
     */
    bool columnSetsMatch(const std::vector<std::string>& set1,
                        const std::vector<std::string>& set2);

    /**
     * Check if set1 contains all columns in set2
     */
    bool columnSetContains(const std::vector<std::string>& set1,
                          const std::vector<std::string>& set2);
};

} // namespace scratchbird::optimizer

#endif // SCRATCHBIRD_OPTIMIZER_MV_REWRITER_H
