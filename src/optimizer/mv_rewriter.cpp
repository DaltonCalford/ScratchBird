/**
 * mv_rewriter.cpp - Materialized View Query Rewriter Implementation
 *
 * P3-15: Automatic query rewriting to use materialized views.
 */

#include "scratchbird/optimizer/mv_rewriter.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/lexer.h"
#include <algorithm>
#include <functional>
#include <chrono>

namespace scratchbird::optimizer
{

// Hash combiner helper
template<typename T>
void hash_combine(size_t& seed, const T& val) {
    seed ^= std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

void QueryPattern::computeHash()
{
    pattern_hash = 0;

    // Hash table names
    for (const auto& name : table_names)
    {
        hash_combine(pattern_hash, name);
    }

    // Hash select columns (sorted for order independence)
    std::vector<std::string> sorted_select = select_columns;
    std::sort(sorted_select.begin(), sorted_select.end());
    for (const auto& col : sorted_select)
    {
        hash_combine(pattern_hash, col);
    }

    // Hash group by columns
    std::vector<std::string> sorted_group = group_by_columns;
    std::sort(sorted_group.begin(), sorted_group.end());
    for (const auto& col : sorted_group)
    {
        hash_combine(pattern_hash, col);
    }

    // Hash aggregates
    for (const auto& agg : aggregates)
    {
        hash_combine(pattern_hash, agg);
    }

    hash_combine(pattern_hash, is_aggregate);
}

MVRewriter::MVRewriter(core::Database* db,
                       CostModel& cost_model,
                       StatisticsManager* stats_manager)
    : db_(db)
    , cost_model_(cost_model)
    , stats_manager_(stats_manager)
    , max_staleness_seconds_(DEFAULT_MAX_STALENESS_SECONDS)
{
}

parser::SelectStmt* MVRewriter::tryRewrite(const parser::SelectStmt* select_stmt,
                                           parser::StringPool& string_pool,
                                           parser::ASTArena& arena,
                                           core::ErrorContext* ctx)
{
    rewrite_attempts_++;

    DEBUG_LOG_DB("MVRewriter: Attempting query rewrite");

    // Step 1: Extract query pattern
    QueryPattern query_pattern = extractPattern(select_stmt, string_pool, ctx);

    if (query_pattern.table_names.empty())
    {
        DEBUG_LOG_DB("MVRewriter: No tables in query, skipping rewrite");
        return nullptr;
    }

    // Step 2: Find candidate MVs
    std::vector<MVCandidate> candidates = findCandidates(query_pattern, ctx);

    if (candidates.empty())
    {
        DEBUG_LOG_DB("MVRewriter: No candidate MVs found");
        return nullptr;
    }

    // Step 3: Estimate original query cost
    double original_cost = estimateOriginalCost(query_pattern, ctx);

    DEBUG_LOG_DB("MVRewriter: Original query cost estimate: " + std::to_string(original_cost));

    // Step 4: Find best MV candidate
    MVCandidate* best_candidate = nullptr;
    double best_savings = 0.0;

    for (auto& candidate : candidates)
    {
        // Check staleness
        if (max_staleness_seconds_ > 0 && candidate.staleness_seconds > max_staleness_seconds_)
        {
            DEBUG_LOG_DB("MVRewriter: Skipping stale MV: " + candidate.mv_info.name);
            continue;
        }

        double savings = original_cost - candidate.estimated_cost;
        if (savings > best_savings)
        {
            best_savings = savings;
            best_candidate = &candidate;
        }
    }

    if (!best_candidate || best_savings <= 0.0)
    {
        DEBUG_LOG_DB("MVRewriter: No beneficial MV rewrite found");
        return nullptr;
    }

    DEBUG_LOG_DB("MVRewriter: Best MV candidate: " + best_candidate->mv_info.name +
                 ", cost=" + std::to_string(best_candidate->estimated_cost) +
                 ", savings=" + std::to_string(best_savings));

    // Step 5: Create rewritten query
    parser::SelectStmt* rewritten = createMVSelect(best_candidate->mv_info,
                                                   select_stmt,
                                                   string_pool,
                                                   arena,
                                                   ctx);

    if (rewritten)
    {
        rewrite_successes_++;
        total_cost_savings_ += best_savings;
        DEBUG_LOG_DB("MVRewriter: Successfully rewrote query using MV: " + best_candidate->mv_info.name);
    }

    return rewritten;
}

std::vector<MVCandidate> MVRewriter::findCandidates(const QueryPattern& pattern,
                                                    core::ErrorContext* ctx)
{
    std::vector<MVCandidate> candidates;

    // Get catalog manager
    core::CatalogManager* catalog = db_->catalog_manager();
    if (!catalog)
    {
        DEBUG_LOG_DB("MVRewriter: No catalog manager available");
        return candidates;
    }

    // Get current timestamp for staleness calculation
    auto now = std::chrono::system_clock::now();
    uint64_t now_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    // Get all views from catalog
    // Note: We need to iterate through the catalog to find materialized views
    // Since getAllViews() doesn't exist, we'll use a simpler approach:
    // Check if any of the queried tables have associated MVs

    for (const auto& table_id : pattern.base_table_ids)
    {
        // Look for views that reference this table
        core::CatalogManager::TableInfo table_info;
        auto status = catalog->getTable(table_id, table_info, ctx);
        if (status != core::Status::OK)
        {
            continue;
        }

        // Search for materialized views by schema
        // This is a simplified implementation - in production we'd have
        // a proper MV registry
        (void)table_info;  // Schema lookup would go here

        // For now, we'll check if there's a view with a matching pattern
        // A full implementation would maintain a registry of MVs indexed by
        // their base tables
    }

    // Fallback: Check pattern cache for any pre-registered MVs
    // In a full implementation, MVs would be registered when created
    // and indexed by the tables they reference

    for (const auto& [mv_id, mv_pattern] : mv_pattern_cache_)
    {
        if (checkSubsumption(mv_pattern, pattern))
        {
            // Get MV info by ID - we need to get the view using getViewIdByName or similar
            // Since we cached the pattern with the ID, use our default schema
            core::CatalogManager::ViewInfo mv_info;

            // Note: In a full implementation, we'd store the schema ID with the cached pattern
            // For now, skip if we can't retrieve the view info
            // This is a simplified implementation - MV info would be cached with pattern

            // Create a candidate with cached info
            MVCandidate candidate;
            candidate.mv_pattern = mv_pattern;

            // Calculate staleness (using a placeholder since we don't have full view info)
            candidate.staleness_seconds = 0.0;  // Assume fresh for cached patterns

            // Estimate cost using a default
            candidate.estimated_cost = 50.0;  // Default low cost for MVs

            // Check match type
            candidate.is_exact_match = columnSetsMatch(mv_pattern.select_columns, pattern.select_columns);
            candidate.is_subsumption_match = !candidate.is_exact_match &&
                columnSetContains(mv_pattern.select_columns, pattern.select_columns);

            candidates.push_back(candidate);
        }
    }

    // Sort by estimated cost (cheapest first)
    std::sort(candidates.begin(), candidates.end(),
              [](const MVCandidate& a, const MVCandidate& b) {
                  return a.estimated_cost < b.estimated_cost;
              });

    DEBUG_LOG_DB("MVRewriter: Found " + std::to_string(candidates.size()) + " candidate MVs");

    return candidates;
}

bool MVRewriter::checkSubsumption(const QueryPattern& mv_pattern,
                                  const QueryPattern& query_pattern)
{
    // MV must cover all tables in the query
    for (const auto& table : query_pattern.table_names)
    {
        bool found = false;
        for (const auto& mv_table : mv_pattern.table_names)
        {
            if (mv_table == table)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }

    // MV must contain all columns selected by query
    if (!columnSetContains(mv_pattern.select_columns, query_pattern.select_columns))
    {
        return false;
    }

    // If query has aggregates, MV must have compatible aggregates
    if (query_pattern.is_aggregate)
    {
        // For now, require exact aggregate match
        // Future: Support deriving aggregates (e.g., AVG from SUM/COUNT)
        if (!mv_pattern.is_aggregate)
        {
            return false;
        }

        // Group by columns must match
        if (!columnSetsMatch(mv_pattern.group_by_columns, query_pattern.group_by_columns))
        {
            return false;
        }
    }

    return true;
}

QueryPattern MVRewriter::extractPattern(const parser::SelectStmt* select_stmt,
                                        const parser::StringPool& string_pool,
                                        core::ErrorContext* ctx)
{
    QueryPattern pattern;

    if (!select_stmt)
    {
        return pattern;
    }

    // Extract table names from FROM clause
    const auto& from_clause = select_stmt->fromClause();
    std::string base_table(string_pool.get(from_clause.base_table.table_name));
    pattern.table_names.push_back(base_table);

    // Get table ID - use getTable with default schema
    core::CatalogManager* catalog = db_->catalog_manager();
    if (catalog)
    {
        // Use default schema (public schema, ID with all zeros except byte 1)
        core::ID default_schema_id{};
        default_schema_id.bytes[1] = 1;  // Public schema

        core::CatalogManager::TableInfo table_info;
        if (catalog->getTable(default_schema_id, base_table, table_info, ctx) == core::Status::OK)
        {
            pattern.base_table_ids.push_back(table_info.table_id);
        }
    }

    // Add joined tables
    for (const auto& join : from_clause.joins)
    {
        std::string joined_table(string_pool.get(join.right_table.table_name));
        pattern.table_names.push_back(joined_table);

        if (catalog)
        {
            core::ID default_schema_id{};
            default_schema_id.bytes[1] = 1;

            core::CatalogManager::TableInfo table_info;
            if (catalog->getTable(default_schema_id, joined_table, table_info, ctx) == core::Status::OK)
            {
                pattern.base_table_ids.push_back(table_info.table_id);
            }
        }
    }

    // Extract select columns
    extractSelectColumns(select_stmt, string_pool, pattern.select_columns);

    // Extract predicate columns from WHERE clause
    if (select_stmt->whereClause())
    {
        extractPredicateColumns(select_stmt->whereClause(), string_pool, pattern.predicate_columns);
        pattern.where_clause = const_cast<parser::Expression*>(select_stmt->whereClause());
    }

    // Extract GROUP BY columns
    const auto* group_by = select_stmt->groupByClause();
    if (group_by)
    {
        for (const auto* expr : group_by->grouping_exprs)
        {
            if (expr && expr->kind() == parser::ASTKind::IDENTIFIER)
            {
                auto* id_expr = static_cast<const parser::IdentifierExpr*>(expr);
                pattern.group_by_columns.push_back(std::string(string_pool.get(id_expr->name())));
            }
        }
    }

    // Check for aggregates
    pattern.is_aggregate = !pattern.group_by_columns.empty();

    // Look for aggregate functions in select list
    for (const auto& item : select_stmt->selectList())
    {
        if (item.expr && item.expr->kind() == parser::ASTKind::AGGREGATE_FUNC)
        {
            auto* agg_expr = static_cast<const parser::AggregateExpr*>(item.expr);
            // Convert aggregate function enum to string
            std::string agg_name;
            switch (agg_expr->func())
            {
                case parser::AggregateFunc::COUNT: agg_name = "COUNT"; break;
                case parser::AggregateFunc::SUM: agg_name = "SUM"; break;
                case parser::AggregateFunc::AVG: agg_name = "AVG"; break;
                case parser::AggregateFunc::MIN: agg_name = "MIN"; break;
                case parser::AggregateFunc::MAX: agg_name = "MAX"; break;
                case parser::AggregateFunc::ARRAY_AGG: agg_name = "ARRAY_AGG"; break;
            }
            pattern.aggregates.push_back(agg_name);
            pattern.is_aggregate = true;
        }
    }

    pattern.computeHash();

    return pattern;
}

void MVRewriter::extractSelectColumns(const parser::SelectStmt* stmt,
                                      const parser::StringPool& string_pool,
                                      std::vector<std::string>& columns)
{
    for (const auto& item : stmt->selectList())
    {
        if (item.is_star)
        {
            columns.push_back("*");
            continue;
        }

        if (!item.expr) continue;

        if (item.expr->kind() == parser::ASTKind::IDENTIFIER)
        {
            auto* id_expr = static_cast<const parser::IdentifierExpr*>(item.expr);
            columns.push_back(std::string(string_pool.get(id_expr->name())));
        }
        else if (item.expr->kind() == parser::ASTKind::AGGREGATE_FUNC)
        {
            auto* agg_expr = static_cast<const parser::AggregateExpr*>(item.expr);
            // Convert aggregate function enum to string
            std::string agg_name;
            switch (agg_expr->func())
            {
                case parser::AggregateFunc::COUNT: agg_name = "COUNT"; break;
                case parser::AggregateFunc::SUM: agg_name = "SUM"; break;
                case parser::AggregateFunc::AVG: agg_name = "AVG"; break;
                case parser::AggregateFunc::MIN: agg_name = "MIN"; break;
                case parser::AggregateFunc::MAX: agg_name = "MAX"; break;
                case parser::AggregateFunc::ARRAY_AGG: agg_name = "ARRAY_AGG"; break;
            }
            // Include aggregate with argument info
            if (agg_expr->arg() && agg_expr->arg()->kind() == parser::ASTKind::IDENTIFIER)
            {
                auto* arg_id = static_cast<const parser::IdentifierExpr*>(agg_expr->arg());
                columns.push_back(agg_name + "(" + std::string(string_pool.get(arg_id->name())) + ")");
            }
            else
            {
                columns.push_back(agg_name + "(*)");
            }
        }
        // Handle other expression types as needed
    }
}

void MVRewriter::extractPredicateColumns(const parser::Expression* expr,
                                         const parser::StringPool& string_pool,
                                         std::vector<std::string>& columns)
{
    if (!expr) return;

    if (expr->kind() == parser::ASTKind::IDENTIFIER)
    {
        auto* id_expr = static_cast<const parser::IdentifierExpr*>(expr);
        columns.push_back(std::string(string_pool.get(id_expr->name())));
    }
    else if (expr->kind() == parser::ASTKind::BINARY_OP)
    {
        auto* bin_expr = static_cast<const parser::BinaryOpExpr*>(expr);
        extractPredicateColumns(bin_expr->left(), string_pool, columns);
        extractPredicateColumns(bin_expr->right(), string_pool, columns);
    }
    // Note: No UnaryOpExpr in the parser - unary operations are handled differently
}

bool MVRewriter::columnSetsMatch(const std::vector<std::string>& set1,
                                  const std::vector<std::string>& set2)
{
    if (set1.size() != set2.size())
    {
        return false;
    }

    std::unordered_set<std::string> s1(set1.begin(), set1.end());
    for (const auto& col : set2)
    {
        if (s1.find(col) == s1.end())
        {
            return false;
        }
    }
    return true;
}

bool MVRewriter::columnSetContains(const std::vector<std::string>& set1,
                                    const std::vector<std::string>& set2)
{
    std::unordered_set<std::string> s1(set1.begin(), set1.end());
    for (const auto& col : set2)
    {
        if (s1.find(col) == s1.end())
        {
            return false;
        }
    }
    return true;
}

QueryPattern MVRewriter::parseMVPattern(const core::CatalogManager::ViewInfo& mv_info,
                                        core::ErrorContext* ctx)
{
    QueryPattern pattern;

    // Parse the MV definition SQL
    parser::Lexer lexer(mv_info.definition);
    parser::ASTArena arena;
    parser::Parser parser(lexer, arena);

    auto result = parser.parseStatement();
    if (result.statement() && result.statement()->kind() == parser::ASTKind::SELECT)
    {
        auto* select_stmt = static_cast<parser::SelectStmt*>(result.statement());
        pattern = extractPattern(select_stmt, parser.stringPool(), ctx);
    }

    return pattern;
}

double MVRewriter::estimateMVScanCost(const core::CatalogManager::ViewInfo& mv_info,
                                       core::ErrorContext* ctx)
{
    // If MV has a backing table, use its statistics
    // Check if ID is valid (non-zero bytes)
    bool has_valid_table = false;
    for (size_t i = 0; i < sizeof(mv_info.materialized_table_id.bytes); ++i)
    {
        if (mv_info.materialized_table_id.bytes[i] != 0)
        {
            has_valid_table = true;
            break;
        }
    }

    if (has_valid_table && stats_manager_)
    {
        TableStatistics stats;
        auto status = stats_manager_->getTableStatistics(mv_info.materialized_table_id, stats, ctx);
        if (status == core::Status::OK)
        {
            // Simple cost estimate: seq_page_cost * num_pages
            CostEstimate estimate = cost_model_.costSeqScan(
                stats.num_pages,
                stats.num_rows,
                0.0,  // No qual cost for simple MV scan
                ctx);
            return estimate.total_cost;
        }
    }

    // Default cost estimate if no statistics available
    return 100.0;  // Arbitrary default
}

double MVRewriter::estimateOriginalCost(const QueryPattern& pattern,
                                         core::ErrorContext* ctx)
{
    double total_cost = 0.0;

    if (stats_manager_)
    {
        for (const auto& table_id : pattern.base_table_ids)
        {
            TableStatistics stats;
            auto status = stats_manager_->getTableStatistics(table_id, stats, ctx);
            if (status == core::Status::OK)
            {
                CostEstimate estimate = cost_model_.costSeqScan(
                    stats.num_pages,
                    stats.num_rows,
                    0.01,  // Small qual cost
                    ctx);
                total_cost += estimate.total_cost;
            }
            else
            {
                total_cost += 1000.0;  // Default estimate for unknown table
            }
        }
    }
    else
    {
        total_cost = 1000.0 * static_cast<double>(pattern.base_table_ids.size());
    }

    // Add join cost estimate for multi-table queries
    if (pattern.base_table_ids.size() > 1)
    {
        // Rough estimate: each join multiplies cost
        total_cost *= static_cast<double>(pattern.base_table_ids.size() - 1) * 2.0;
    }

    // Add aggregation cost
    if (pattern.is_aggregate)
    {
        total_cost *= 1.5;  // Aggregation overhead
    }

    return total_cost;
}

parser::SelectStmt* MVRewriter::createMVSelect(const core::CatalogManager::ViewInfo& mv_info,
                                                const parser::SelectStmt* original,
                                                parser::StringPool& string_pool,
                                                parser::ASTArena& arena,
                                                core::ErrorContext* ctx)
{
    // Create a new SELECT that reads from the MV's backing table
    // This is a simplified implementation - full version would handle
    // column mapping and predicate pushdown

    // Create table reference to MV
    parser::StringPool::StringId mv_name_id = string_pool.intern(mv_info.name);

    parser::TableRef mv_table_ref;
    mv_table_ref.table_name = mv_name_id;
    mv_table_ref.alias = 0;  // No alias

    parser::FromClause from_clause;
    from_clause.base_table = mv_table_ref;

    // Create SELECT items from the original select list
    std::vector<parser::SelectItem> select_list;

    // Copy the original select list
    for (const auto& item : original->selectList())
    {
        select_list.push_back(item);
    }

    // Create new SELECT statement using the FromClause constructor
    auto* new_select = arena.make<parser::SelectStmt>(
        original->span(),
        select_list,
        from_clause);

    // Copy additional clauses
    if (original->whereClause())
    {
        new_select->setWhereClause(const_cast<parser::Expression*>(original->whereClause()));
    }

    // Copy GROUP BY clause
    if (original->groupByClause())
    {
        new_select->setGroupByClause(*original->groupByClause());
    }

    // Copy ORDER BY clause
    new_select->setOrderByClause(original->orderByClause());

    // Copy LIMIT/OFFSET
    if (original->hasLimit())
    {
        new_select->setLimitCount(original->limitCount());
    }
    if (original->hasOffset())
    {
        new_select->setOffsetCount(original->offsetCount());
    }

    return new_select;
}

} // namespace scratchbird::optimizer
