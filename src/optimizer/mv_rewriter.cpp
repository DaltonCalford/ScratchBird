/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * mv_rewriter.cpp - Materialized View Query Rewriter Implementation
 *
 * V2 MIGRATION STATUS: COMPLETE
 *
 * P3-15: Automatic query rewriting to use materialized views.
 */

#include "scratchbird/optimizer/mv_rewriter.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/parser/parser_v2.h"
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

parser::v2::SelectStmt* MVRewriter::tryRewrite(const parser::v2::SelectStmt* select_stmt,
                                               parser::v2::StringPool& string_pool,
                                               parser::v2::ASTArena& arena,
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
    parser::v2::SelectStmt* rewritten = createMVSelect(best_candidate->mv_info,
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

    // OPT-4: Get all materialized views from catalog
    std::vector<core::CatalogManager::ViewInfo> all_mvs;
    auto status = catalog->getAllMaterializedViews(all_mvs, ctx);
    if (status == core::Status::OK && !all_mvs.empty())
    {
        DEBUG_LOG_DB("MVRewriter: Found " + std::to_string(all_mvs.size()) + " materialized views in catalog");

        for (const auto& mv_info : all_mvs)
        {
            // Try to extract pattern from MV definition
            QueryPattern mv_pattern;
            if (extractPatternFromDefinition(mv_info.definition, mv_pattern))
            {
                if (checkSubsumption(mv_pattern, pattern))
                {
                    MVCandidate candidate;
                    candidate.mv_info = mv_info;
                    candidate.mv_pattern = mv_pattern;

                    // OPT-M9: Calculate staleness from last_refresh_time
                    if (mv_info.last_refresh_time > 0)
                    {
                        candidate.staleness_seconds = static_cast<double>(now_timestamp - mv_info.last_refresh_time);
                    }
                    else
                    {
                        candidate.staleness_seconds = 0.0;  // Never refreshed = treat as current
                    }

                    // OPT-M10: Estimate cost using statistics if available
                    if (stats_manager_ && !mv_pattern.base_table_ids.empty())
                    {
                        double original_cost = estimateOriginalCost(mv_pattern, ctx);
                        double mv_cost_factor = mv_pattern.is_aggregate ? 0.15 : 0.40;
                        candidate.estimated_cost = original_cost * mv_cost_factor;
                        if (candidate.estimated_cost < 10.0)
                        {
                            candidate.estimated_cost = 10.0;
                        }
                    }
                    else
                    {
                        candidate.estimated_cost = 50.0;
                    }

                    // Check match type
                    candidate.is_exact_match = columnSetsMatch(mv_pattern.select_columns, pattern.select_columns);
                    candidate.is_subsumption_match = !candidate.is_exact_match &&
                        columnSetContains(mv_pattern.select_columns, pattern.select_columns);

                    candidates.push_back(candidate);
                }
            }
        }
    }

    // Also check pattern cache for any pre-registered MVs
    for (const auto& [mv_id, mv_pattern] : mv_pattern_cache_)
    {
        if (checkSubsumption(mv_pattern, pattern))
        {
            // OPT-5: Get MV info by ID using getViewById
            core::CatalogManager::ViewInfo mv_info;
            status = catalog->getViewById(mv_id, mv_info, ctx);

            MVCandidate candidate;
            candidate.mv_pattern = mv_pattern;

            if (status == core::Status::OK)
            {
                candidate.mv_info = mv_info;

                // OPT-M9: Calculate staleness from last_refresh_time
                if (mv_info.last_refresh_time > 0)
                {
                    candidate.staleness_seconds = static_cast<double>(now_timestamp - mv_info.last_refresh_time);
                }
                else
                {
                    candidate.staleness_seconds = 0.0;
                }
            }
            else
            {
                // Fallback if view info not found - assume fresh
                candidate.staleness_seconds = 0.0;
            }

            // OPT-M10: Estimate cost using statistics if available
            if (stats_manager_ && !mv_pattern.base_table_ids.empty())
            {
                double original_cost = estimateOriginalCost(mv_pattern, ctx);
                double mv_cost_factor = mv_pattern.is_aggregate ? 0.15 : 0.40;
                candidate.estimated_cost = original_cost * mv_cost_factor;
                if (candidate.estimated_cost < 10.0)
                {
                    candidate.estimated_cost = 10.0;
                }
            }
            else
            {
                candidate.estimated_cost = 50.0;
            }

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

QueryPattern MVRewriter::extractPattern(const parser::v2::SelectStmt* select_stmt,
                                        const parser::v2::StringPool& string_pool,
                                        core::ErrorContext* ctx)
{
    QueryPattern pattern;

    if (!select_stmt)
    {
        return pattern;
    }

    // Helper lambda to build table name from SchemaPath
    auto pathToString = [&string_pool](const parser::v2::SchemaPath& path) -> std::string {
        std::string result;
        for (size_t i = 0; i < path.components.size(); ++i) {
            if (i > 0) result += ".";
            result += string_pool.get(path.components[i]);
        }
        return result;
    };

    // Extract table names from FROM clause
    if (select_stmt->from)
    {
        // V2 uses TableRefNode* for base table (not polymorphic)
        auto* table_ref = select_stmt->from;
        std::string base_table = pathToString(table_ref->table_path);
        pattern.table_names.push_back(base_table);

        // Get table ID from catalog
        core::CatalogManager* catalog = db_->catalog_manager();
        if (catalog)
        {
            core::ID default_schema_id{};
            default_schema_id.bytes[1] = 1;  // Public schema

            core::CatalogManager::TableInfo table_info;
            if (catalog->getTable(default_schema_id, base_table, table_info, ctx) == core::Status::OK)
            {
                pattern.base_table_ids.push_back(table_info.table_id);
            }
        }
    }

    // Add joined tables
    for (const auto* join : select_stmt->joins)
    {
        if (join && join->right)
        {
            auto* table_ref = join->right;
            std::string joined_table = pathToString(table_ref->table_path);
            pattern.table_names.push_back(joined_table);

            core::CatalogManager* catalog = db_->catalog_manager();
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
    }

    // Extract select columns
    extractSelectColumns(select_stmt, string_pool, pattern.select_columns);

    // Extract predicate columns from WHERE clause
    if (select_stmt->where)
    {
        extractPredicateColumns(select_stmt->where, string_pool, pattern.predicate_columns);
        // Note: V2 unresolved AST where clause, not resolved
    }

    // Extract GROUP BY columns
    for (const auto* expr : select_stmt->group_by)
    {
        if (expr && expr->kind() == parser::v2::ASTKind::ColumnRefExpr)
        {
            auto* col_ref = static_cast<const parser::v2::ColumnRefExpr*>(expr);
            pattern.group_by_columns.push_back(std::string(string_pool.get(col_ref->column.column_name)));
        }
    }

    // Check for aggregates
    pattern.is_aggregate = !pattern.group_by_columns.empty();

    // Look for aggregate functions in select list
    for (const auto* item : select_stmt->items)
    {
        if (item && item->expr && item->expr->kind() == parser::v2::ASTKind::FunctionCallExpr)
        {
            auto* func_expr = static_cast<const parser::v2::FunctionCallExpr*>(item->expr);
            // Get function name from path components
            std::string func_name;
            for (size_t i = 0; i < func_expr->function_path.components.size(); ++i) {
                if (i > 0) func_name += ".";
                func_name += string_pool.get(func_expr->function_path.components[i]);
            }
            // Convert to uppercase for matching
            std::transform(func_name.begin(), func_name.end(), func_name.begin(), ::toupper);

            // Check if it's an aggregate function
            if (func_name == "COUNT" || func_name == "SUM" || func_name == "AVG" ||
                func_name == "MIN" || func_name == "MAX" || func_name == "ARRAY_AGG")
            {
                pattern.aggregates.push_back(func_name);
                pattern.is_aggregate = true;
            }
        }
    }

    pattern.computeHash();

    return pattern;
}

void MVRewriter::extractSelectColumns(const parser::v2::SelectStmt* stmt,
                                      const parser::v2::StringPool& string_pool,
                                      std::vector<std::string>& columns)
{
    for (const auto* item : stmt->items)
    {
        if (!item) continue;

        if (item->item_type == parser::v2::SelectItem::Type::STAR)
        {
            columns.push_back("*");
            continue;
        }

        if (item->item_type == parser::v2::SelectItem::Type::TABLE_STAR)
        {
            // Build table path string from components
            std::string table_path_str;
            for (size_t i = 0; i < item->table_path.components.size(); ++i) {
                if (i > 0) table_path_str += ".";
                table_path_str += string_pool.get(item->table_path.components[i]);
            }
            columns.push_back(table_path_str + ".*");
            continue;
        }

        if (!item->expr) continue;

        if (item->expr->kind() == parser::v2::ASTKind::ColumnRefExpr)
        {
            auto* col_ref = static_cast<const parser::v2::ColumnRefExpr*>(item->expr);
            columns.push_back(std::string(string_pool.get(col_ref->column.column_name)));
        }
        else if (item->expr->kind() == parser::v2::ASTKind::FunctionCallExpr)
        {
            auto* func_expr = static_cast<const parser::v2::FunctionCallExpr*>(item->expr);
            // Build function name from path components
            std::string func_name;
            for (size_t i = 0; i < func_expr->function_path.components.size(); ++i) {
                if (i > 0) func_name += ".";
                func_name += string_pool.get(func_expr->function_path.components[i]);
            }

            // Include function with argument info
            if (!func_expr->arguments.empty() &&
                func_expr->arguments[0]->kind() == parser::v2::ASTKind::ColumnRefExpr)
            {
                auto* arg_col = static_cast<const parser::v2::ColumnRefExpr*>(func_expr->arguments[0]);
                columns.push_back(func_name + "(" + std::string(string_pool.get(arg_col->column.column_name)) + ")");
            }
            else
            {
                columns.push_back(func_name + "(*)");
            }
        }
        // Handle other expression types as needed
    }
}

void MVRewriter::extractPredicateColumns(const parser::v2::Expression* expr,
                                         const parser::v2::StringPool& string_pool,
                                         std::vector<std::string>& columns)
{
    if (!expr) return;

    if (expr->kind() == parser::v2::ASTKind::ColumnRefExpr)
    {
        auto* col_ref = static_cast<const parser::v2::ColumnRefExpr*>(expr);
        columns.push_back(std::string(string_pool.get(col_ref->column.column_name)));
    }
    else if (expr->kind() == parser::v2::ASTKind::BinaryExpr)
    {
        auto* bin_expr = static_cast<const parser::v2::BinaryExpr*>(expr);
        extractPredicateColumns(bin_expr->left, string_pool, columns);
        extractPredicateColumns(bin_expr->right, string_pool, columns);
    }
    else if (expr->kind() == parser::v2::ASTKind::UnaryExpr)
    {
        auto* unary_expr = static_cast<const parser::v2::UnaryExpr*>(expr);
        extractPredicateColumns(unary_expr->operand, string_pool, columns);
    }
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

    // Parse the MV definition SQL using V2 parser
    parser::v2::Parser parser(mv_info.definition);

    auto result = parser.parseStatement();
    if (result.statement() && result.statement()->kind() == parser::v2::ASTKind::SelectStmt)
    {
        auto* select_stmt = static_cast<parser::v2::SelectStmt*>(result.statement());
        pattern = extractPattern(select_stmt, parser.stringPool(), ctx);
    }

    return pattern;
}

bool MVRewriter::extractPatternFromDefinition(const std::string& definition,
                                               QueryPattern& pattern_out)
{
    // OPT-4: Parse MV definition string and extract query pattern using V2 parser
    if (definition.empty())
    {
        return false;
    }

    // Parse the definition SQL
    parser::v2::Parser parser(definition);

    auto result = parser.parseStatement();
    if (!result.statement())
    {
        DEBUG_LOG_DB("MVRewriter: Failed to parse MV definition: " + definition);
        return false;
    }

    if (result.statement()->kind() != parser::v2::ASTKind::SelectStmt)
    {
        DEBUG_LOG_DB("MVRewriter: MV definition is not a SELECT statement");
        return false;
    }

    auto* select_stmt = static_cast<parser::v2::SelectStmt*>(result.statement());
    pattern_out = extractPattern(select_stmt, parser.stringPool(), nullptr);

    // Verify we extracted something useful
    if (pattern_out.table_names.empty())
    {
        DEBUG_LOG_DB("MVRewriter: No tables found in MV definition");
        return false;
    }

    return true;
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

parser::v2::SelectStmt* MVRewriter::createMVSelect(const core::CatalogManager::ViewInfo& mv_info,
                                                    const parser::v2::SelectStmt* original,
                                                    parser::v2::StringPool& string_pool,
                                                    parser::v2::ASTArena& arena,
                                                    core::ErrorContext* ctx)
{
    // Create a new SELECT that reads from the MV's backing table
    // This is a simplified implementation - full version would handle
    // column mapping and predicate pushdown

    // Create new SelectStmt
    auto* new_select = arena.create<parser::v2::SelectStmt>();

    // Create table reference to MV
    auto* mv_table_ref = arena.create<parser::v2::TableRefNode>();
    // Create SchemaPath with proper constructor: (PathType, components, span)
    mv_table_ref->table_path = parser::v2::SchemaPath(
        parser::v2::PathType::UNQUALIFIED,
        std::vector<parser::v2::StringPool::StringId>{string_pool.intern(mv_info.name)},
        parser::v2::SourceSpan{});
    new_select->from = mv_table_ref;

    // Copy the original select list
    for (const auto* item : original->items)
    {
        // Note: We're sharing pointers here - in a production implementation
        // we might want to deep copy to avoid issues with arena lifetimes
        new_select->items.push_back(const_cast<parser::v2::SelectItem*>(item));
    }

    // Copy WHERE clause
    if (original->where)
    {
        new_select->where = const_cast<parser::v2::Expression*>(original->where);
    }

    // Copy GROUP BY clause
    for (const auto* expr : original->group_by)
    {
        new_select->group_by.push_back(const_cast<parser::v2::Expression*>(expr));
    }

    // Copy HAVING clause
    if (original->having)
    {
        new_select->having = const_cast<parser::v2::Expression*>(original->having);
    }

    // Copy ORDER BY clause
    for (const auto* item : original->order_by)
    {
        new_select->order_by.push_back(const_cast<parser::v2::OrderByItem*>(item));
    }

    // Copy LIMIT/OFFSET
    if (original->limit)
    {
        new_select->limit = const_cast<parser::v2::Expression*>(original->limit);
    }
    if (original->offset)
    {
        new_select->offset = const_cast<parser::v2::Expression*>(original->offset);
    }

    return new_select;
}

} // namespace scratchbird::optimizer
