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
 * QueryPlanner - Cost-based Query Planner Implementation
 *
 * V2 MIGRATION STATUS: COMPLETE
 *
 * This file implements the query planner using V2 resolved AST types.
 * The planner generates execution plans for SELECT and ANALYZE statements.
 *
 * NOTE: The main execution path is:
 *   ServerSession → QueryCompilerV2 → BytecodeGeneratorV2 → Executor
 *
 * QueryPlanner is primarily used for EXPLAIN functionality.
 */

#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/optimizer/join_ordering.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/permission_cache.h"
#include <algorithm>
#include <unordered_map>
#include <cmath>

namespace scratchbird::optimizer
{

namespace
{

// Estimate table row count (default if statistics unavailable)
constexpr uint64_t DEFAULT_TABLE_ROWS = 1000;
constexpr uint64_t DEFAULT_TABLE_PAGES = 100;
constexpr double SEQ_SCAN_COST_PER_PAGE = 1.0;
constexpr double CPU_TUPLE_COST = 0.01;

// Helper to get table name from ResolvedTableRef
// Returns table UUID as hex string if database unavailable
std::string getTableName(const parser::v2::ResolvedTableRef* ref, core::Database* db)
{
    if (!ref) return "unknown";
    // Use table UUID as fallback name
    return ref->table_uuid.toString();
}

// Helper to create a SeqScanNode for a table
std::shared_ptr<SeqScanNode> createSeqScanNode(
    const core::ID& table_id,
    const std::string& table_name,
    uint64_t estimated_rows,
    uint64_t estimated_pages)
{
    auto node = std::make_shared<SeqScanNode>(table_id, table_name);

    // Calculate cost: startup + (pages * seq_scan_cost) + (rows * cpu_cost)
    double startup_cost = 0.0;
    double total_cost = estimated_pages * SEQ_SCAN_COST_PER_PAGE +
                       estimated_rows * CPU_TUPLE_COST;

    node->setCost(startup_cost, total_cost, estimated_rows);
    return node;
}

// Helper to create a SeqScanPath for join planning
std::shared_ptr<SeqScanPath> createSeqScanPath(
    const core::ID& table_id,
    const std::string& table_name,
    uint64_t estimated_rows,
    uint64_t estimated_pages)
{
    CostEstimate estimate;
    estimate.startup_cost = 0.0;
    estimate.total_cost = estimated_pages * SEQ_SCAN_COST_PER_PAGE +
                         estimated_rows * CPU_TUPLE_COST;
    estimate.rows = estimated_rows;

    return std::make_shared<SeqScanPath>(
        table_id,
        table_name,
        estimated_pages,
        estimated_rows,
        0.0,  // qual_cost
        estimate
    );
}

// Convert optimizer Path to PlanNode
std::shared_ptr<PlanNode> pathToPlanNode(const std::shared_ptr<Path>& path)
{
    if (!path) return nullptr;

    switch (path->type())
    {
        case PathType::SEQ_SCAN:
        {
            auto seq_path = std::dynamic_pointer_cast<SeqScanPath>(path);
            if (!seq_path) return nullptr;

            auto node = std::make_shared<SeqScanNode>(
                seq_path->tableId(),
                seq_path->tableName()
            );
            node->setCost(path->cost().startup_cost,
                         path->cost().total_cost,
                         path->cost().rows);
            return node;
        }

        case PathType::NESTED_LOOP_JOIN:
        {
            auto nlj_path = std::dynamic_pointer_cast<NestedLoopJoinPath>(path);
            if (!nlj_path) return nullptr;

            auto outer_node = pathToPlanNode(nlj_path->outerPath());
            auto inner_node = pathToPlanNode(nlj_path->innerPath());

            auto node = std::make_shared<NestedLoopJoinNode>(
                nlj_path->joinType(),
                outer_node,
                inner_node,
                nlj_path->joinCondition()
            );
            node->setCost(path->cost().startup_cost,
                         path->cost().total_cost,
                         path->cost().rows);
            return node;
        }

        case PathType::HASH_JOIN:
        {
            auto hash_path = std::dynamic_pointer_cast<HashJoinPath>(path);
            if (!hash_path) return nullptr;

            auto outer_node = pathToPlanNode(hash_path->outerPath());
            auto inner_node = pathToPlanNode(hash_path->innerPath());

            std::vector<parser::v2::ResolvedExpression*> empty_keys;
            auto node = std::make_shared<HashJoinNode>(
                hash_path->joinType(),
                outer_node,
                inner_node,
                hash_path->joinCondition(),
                empty_keys,  // hash_keys_outer (not stored in path)
                empty_keys   // hash_keys_inner (not stored in path)
            );
            node->setCost(path->cost().startup_cost,
                         path->cost().total_cost,
                         path->cost().rows);
            return node;
        }

        default:
            DEBUG_LOG_DB("Unsupported path type for conversion to PlanNode");
            return nullptr;
    }
}

} // anonymous namespace

auto QueryPlanner::planQuery(const parser::v2::ResolvedSelectStmt* select_stmt,
                              core::ErrorContext* ctx,
                              core::ConnectionContext* conn_ctx)
    -> std::shared_ptr<PlanNode>
{
    if (!select_stmt)
    {
        DEBUG_LOG_DB("QueryPlanner::planQuery - null select_stmt");
        return nullptr;
    }

    conn_ctx_ = conn_ctx;

    DEBUG_LOG_DB("QueryPlanner::planQuery - planning query");

    if (!checkSelectPermissions(select_stmt, ctx))
    {
        return nullptr;
    }

    // Check if this is a join query
    if (!select_stmt->joins.empty())
    {
        DEBUG_LOG_DB("Planning JOIN query with " +
                    std::to_string(select_stmt->joins.size()) + " joins");
        return planJoinQuery(select_stmt, ctx);
    }

    // Single table or no-table query
    if (select_stmt->from_tables.empty())
    {
        // Query with no FROM clause (e.g., SELECT 1+1)
        DEBUG_LOG_DB("Query with no FROM clause - returning empty scan");
        auto node = std::make_shared<SeqScanNode>(core::ID{}, "");
        node->setCost(0.0, 0.01, 1);  // Minimal cost, single row
        return wrapWithClauses(node, select_stmt);
    }

    // Single table query
    auto* table_ref = select_stmt->from_tables[0];
    if (!table_ref)
    {
        DEBUG_LOG_DB("QueryPlanner::planQuery - null table reference");
        return nullptr;
    }

    // Create base scan node
    auto base_node = createSeqScanNode(
        table_ref->table_uuid,
        getTableName(table_ref, db_),
        DEFAULT_TABLE_ROWS,
        DEFAULT_TABLE_PAGES
    );

    // Apply WHERE filter if present
    if (select_stmt->where)
    {
        // Estimate selectivity (default 10% for now)
        double selectivity = 0.1;
        uint64_t filtered_rows = static_cast<uint64_t>(base_node->rows() * selectivity);
        if (filtered_rows == 0) filtered_rows = 1;

        base_node->setCost(
            base_node->startupCost(),
            base_node->totalCost(),
            filtered_rows
        );
        base_node->setFilter("(filter expression)");
    }

    return wrapWithClauses(base_node, select_stmt);
}

bool QueryPlanner::checkSelectPermissions(
    const parser::v2::ResolvedSelectStmt* select_stmt,
    core::ErrorContext* ctx)
{
    if (!select_stmt || select_stmt->from_tables.empty())
    {
        return true;
    }

    if (!conn_ctx_ || conn_ctx_->isSuperuser())
    {
        return true;
    }

    auto* catalog = db_ ? db_->catalog_manager() : nullptr;
    auto* cache = db_ ? db_->permission_cache() : nullptr;
    if (!catalog || !cache)
    {
        SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR,
                          "Permission checks unavailable");
        return false;
    }

    core::ID user_id = conn_ctx_->getCurrentUserId();
    if (user_id == core::ID{})
    {
        SET_ERROR_CONTEXT(ctx, core::Status::PERMISSION_DENIED,
                          "Permission denied: no active user");
        return false;
    }

    for (const auto* table_ref : select_stmt->from_tables)
    {
        if (!table_ref)
        {
            continue;
        }

        if (table_ref->object_type == parser::v2::ResolvedTableRef::ObjectType::CTE ||
            table_ref->object_type == parser::v2::ResolvedTableRef::ObjectType::SUBQUERY ||
            table_ref->object_type == parser::v2::ResolvedTableRef::ObjectType::FUNCTION)
        {
            continue;
        }

        if (table_ref->table_uuid == core::ID{})
        {
            continue;
        }

        core::CatalogManager::PermissionObjectType object_type =
            (table_ref->object_type == parser::v2::ResolvedTableRef::ObjectType::VIEW ||
             table_ref->object_type == parser::v2::ResolvedTableRef::ObjectType::MATERIALIZED_VIEW)
                ? core::CatalogManager::PermissionObjectType::VIEW
                : core::CatalogManager::PermissionObjectType::TABLE;

        core::PermissionCache::CacheKey key{
            user_id,
            table_ref->table_uuid,
            object_type,
            core::CatalogManager::Privilege::SELECT
        };

        core::ErrorContext perm_ctx;
        bool allowed = cache->checkPermission(
            catalog,
            key,
            core::PermissionCheckMode::CACHED,
            &perm_ctx);

        if (!allowed)
        {
            if (ctx)
            {
                if (perm_ctx.code != core::Status::OK &&
                    perm_ctx.code != core::Status::NOT_FOUND)
                {
                    SET_ERROR_CONTEXT(ctx, perm_ctx.code,
                                      perm_ctx.message.empty() ? "Permission check failed"
                                                               : perm_ctx.message.c_str());
                }
                else
                {
                    std::string object_label = getTableName(table_ref, db_);
                    SET_ERROR_CONTEXT(ctx, core::Status::PERMISSION_DENIED,
                                      ("Permission denied: SELECT on " + object_label).c_str());
                }
            }
            return false;
        }
    }

    return true;
}

std::shared_ptr<PlanNode> QueryPlanner::planJoinQuery(
    const parser::v2::ResolvedSelectStmt* select_stmt,
    core::ErrorContext* ctx)
{
    DEBUG_LOG_DB("planJoinQuery - starting");

    // Use JoinOrderingOptimizer to find optimal join order
    JoinOrderingOptimizer optimizer(cost_model_, selectivity_estimator_);

    // Add base tables as relations
    std::unordered_map<const parser::v2::ResolvedTableRef*, size_t> table_to_idx;

    for (auto* table_ref : select_stmt->from_tables)
    {
        if (!table_ref) continue;

        std::string table_name = getTableName(table_ref, db_);

        auto path = createSeqScanPath(
            table_ref->table_uuid,
            table_name,
            DEFAULT_TABLE_ROWS,
            DEFAULT_TABLE_PAGES
        );

        size_t idx = optimizer.addRelation(
            table_ref->table_uuid,
            table_name,
            "",  // alias - not needed for cost calculation
            path
        );

        table_to_idx[table_ref] = idx;
    }

    // Add join edges
    for (auto* join : select_stmt->joins)
    {
        if (!join) continue;

        auto left_it = table_to_idx.find(join->left);
        auto right_it = table_to_idx.find(join->right);

        if (left_it == table_to_idx.end() || right_it == table_to_idx.end())
        {
            DEBUG_LOG_DB("Join references unknown table");
            continue;
        }

        optimizer.addJoinEdge(
            left_it->second,
            right_it->second,
            join->join_type,
            join->on_condition
        );
    }

    // Optimize join order
    auto optimal_path = optimizer.optimize();
    if (!optimal_path)
    {
        DEBUG_LOG_DB("JoinOrderingOptimizer returned null path");
        return nullptr;
    }

    // Convert Path to PlanNode
    auto join_node = pathToPlanNode(optimal_path);
    if (!join_node)
    {
        DEBUG_LOG_DB("Failed to convert join path to plan node");
        return nullptr;
    }

    return wrapWithClauses(join_node, select_stmt);
}

std::shared_ptr<PlanNode> QueryPlanner::wrapWithClauses(
    std::shared_ptr<PlanNode> base_plan,
    const parser::v2::ResolvedSelectStmt* select_stmt)
{
    if (!base_plan || !select_stmt) return base_plan;

    auto current = base_plan;

    // Add GROUP BY / aggregation if present
    if (!select_stmt->group_by.empty())
    {
        DEBUG_LOG_DB("Adding AggregateNode for GROUP BY");

        // Collect aggregate functions from select list
        std::vector<parser::v2::ResolvedFunctionCall*> aggregates;
        for (const auto& item : select_stmt->select_list)
        {
            if (item.expr)
            {
                // Check if it's an aggregate function call
                if (auto* func = dynamic_cast<parser::v2::ResolvedFunctionCall*>(item.expr))
                {
                    if (func->function.is_aggregate)
                    {
                        aggregates.push_back(func);
                    }
                }
            }
        }

        auto agg_node = std::make_shared<AggregateNode>(
            current,
            select_stmt->group_by,
            aggregates,
            select_stmt->having
        );

        // Estimate group count (rough: sqrt of input rows)
        uint64_t groups = static_cast<uint64_t>(std::sqrt(static_cast<double>(current->rows())));
        if (groups == 0) groups = 1;

        double agg_cost = current->totalCost() +
                         current->rows() * CPU_TUPLE_COST * 2.0;  // Hash insert + lookup

        agg_node->setCost(current->startupCost() + agg_cost * 0.1, agg_cost, groups);
        current = agg_node;
    }

    // Add ORDER BY if present
    if (!select_stmt->order_by.empty())
    {
        DEBUG_LOG_DB("Adding SortNode for ORDER BY");

        auto sort_node = std::make_shared<SortNode>(
            current,
            select_stmt->order_by
        );

        // Sort cost: O(n log n)
        double n = static_cast<double>(current->rows());
        double sort_cost = current->totalCost() +
                          n * std::log2(n + 1.0) * CPU_TUPLE_COST;

        sort_node->setCost(sort_cost * 0.9, sort_cost, current->rows());
        current = sort_node;
    }

    // Add LIMIT/OFFSET if present
    if (select_stmt->limit || select_stmt->offset)
    {
        DEBUG_LOG_DB("Adding LimitNode for LIMIT/OFFSET");

        int64_t limit_val = -1;
        int64_t offset_val = -1;

        // Extract constant values if possible
        if (select_stmt->limit)
        {
            if (auto* lit = dynamic_cast<parser::v2::ResolvedLiteral*>(select_stmt->limit))
            {
                limit_val = lit->int_value;
            }
        }
        if (select_stmt->offset)
        {
            if (auto* lit = dynamic_cast<parser::v2::ResolvedLiteral*>(select_stmt->offset))
            {
                offset_val = lit->int_value;
            }
        }

        auto limit_node = std::make_shared<LimitNode>(
            current,
            limit_val,
            offset_val
        );

        // Calculate output rows
        uint64_t output_rows = current->rows();
        if (offset_val > 0)
        {
            output_rows = (output_rows > static_cast<uint64_t>(offset_val)) ?
                         output_rows - offset_val : 0;
        }
        if (limit_val >= 0)
        {
            output_rows = std::min(output_rows, static_cast<uint64_t>(limit_val));
        }

        // Limit reduces effective cost
        double fraction = (current->rows() > 0) ?
            static_cast<double>(output_rows + (offset_val > 0 ? offset_val : 0)) /
            static_cast<double>(current->rows()) : 1.0;
        double limit_cost = current->totalCost() * fraction;

        limit_node->setCost(current->startupCost(), limit_cost, output_rows);
        current = limit_node;
    }

    return current;
}

auto QueryPlanner::planAnalyze(const parser::v2::ResolvedAnalyzeStmt* analyze_stmt,
                                core::ErrorContext* ctx)
    -> std::shared_ptr<PlanNode>
{
    if (!analyze_stmt)
    {
        DEBUG_LOG_DB("QueryPlanner::planAnalyze - null analyze_stmt");
        return nullptr;
    }

    DEBUG_LOG_DB("QueryPlanner::planAnalyze - planning ANALYZE");

    // ANALYZE scans the target table(s) to gather statistics
    if (!analyze_stmt->table_path.isEmpty())
    {
        // Analyze specific table
        std::string table_name = "ANALYZE_TARGET";

        auto node = createSeqScanNode(
            core::ID{},
            table_name,
            DEFAULT_TABLE_ROWS,
            DEFAULT_TABLE_PAGES
        );

        // ANALYZE has higher CPU cost (computing statistics)
        double analyze_cost = node->totalCost() * 2.0;  // 2x for stat computation
        node->setCost(0.0, analyze_cost, 0);  // Returns 0 rows

        return node;
    }
    else
    {
        // Analyze all tables - return a placeholder
        auto node = std::make_shared<SeqScanNode>(core::ID{}, "ALL TABLES");
        node->setCost(0.0, 1000.0, 0);  // Estimated cost
        return node;
    }
}

} // namespace scratchbird::optimizer
