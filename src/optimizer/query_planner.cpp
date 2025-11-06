#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/optimizer/expression_matcher.h"
#include "scratchbird/optimizer/predicate_matcher.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/expression_serializer.h"
#include <algorithm>
#include <unordered_map>
#include <cmath>  // LSM Integration: for std::ceil, std::log2

namespace scratchbird::optimizer
{

    auto QueryPlanner::planQuery(const parser::SelectStmt *select_stmt,
                                  const parser::StringPool &string_pool,
                                  core::ErrorContext *ctx)
        -> std::shared_ptr<PlanNode>
    {
        // Phase 2 Wave 2: Process WITH clause (CTEs) if present
        std::unordered_map<std::string, std::shared_ptr<PlanNode>> cte_plans;
        if (select_stmt->withClause())
        {
            DEBUG_LOG_DB("Planning WITH clause (CTEs)");
            for (const auto &cte : select_stmt->withClause()->ctes())
            {
                std::string cte_name(string_pool.get(cte.name));
                DEBUG_LOG_DB("Planning CTE: " + cte_name);

                // Recursively plan the CTE query
                auto cte_plan = planQuery(cte.query, string_pool, ctx);
                if (!cte_plan)
                {
                    DEBUG_LOG_DB("Failed to plan CTE: " + cte_name);
                    return nullptr;
                }

                cte_plans[cte_name] = cte_plan;
            }
        }

        // Check if this is a join query (Phase 1, Task 3.2)
        if (select_stmt->hasJoins())
        {
            DEBUG_LOG_DB("Planning JOIN query");
            return planJoinQuery(select_stmt, string_pool, ctx);
        }

        // Single-table query planning (original logic)
        std::string table_name(string_pool.get(select_stmt->tableName()));
        DEBUG_LOG_DB("Planning single-table query for: " + table_name);

        // Phase 2 Wave 2: Check if this is a CTE reference
        auto cte_it = cte_plans.find(table_name);
        if (cte_it != cte_plans.end())
        {
            DEBUG_LOG_DB("Table is a CTE: " + table_name);
            // Create a CTEScanNode to scan the materialized CTE
            auto cte_scan = std::make_shared<CTEScanNode>(table_name, cte_it->second);
            // Copy cost estimates from the CTE plan
            cte_scan->setCost(cte_it->second->startupCost(),
                             cte_it->second->totalCost(),
                             cte_it->second->rows());
            return cte_scan;
        }

        // Phase 1: Get table ID from catalog
        // Get default PUBLIC schema
        core::CatalogManager::SchemaInfo schema_info;
        core::Status status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, ctx);
        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("Failed to get PUBLIC schema");
            return nullptr;
        }

        // Look up table in catalog
        core::CatalogManager::TableInfo table_info;
        status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info, ctx);
        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("Failed to find table: " + table_name);
            return nullptr;
        }

        core::ID table_id = table_info.table_id;

        DEBUG_LOG_DB("Table ID: " + table_id.toString());

        // Phase 2: Generate all feasible paths
        std::vector<std::shared_ptr<Path>> paths;
        status = generatePaths(select_stmt, table_id, paths, string_pool, ctx);
        if (status != core::Status::OK || paths.empty())
        {
            DEBUG_LOG_DB("Failed to generate paths");
            return nullptr;
        }

        DEBUG_LOG_DB("Generated " + std::to_string(paths.size()) + " paths");

        // Phase 3: Select cheapest path
        auto cheapest = selectCheapestPath(paths);
        if (!cheapest)
        {
            DEBUG_LOG_DB("No valid path found");
            return nullptr;
        }

        DEBUG_LOG_DB("Selected cheapest path: " + cheapest->toString());

        // Phase 3.5: Layer aggregation, window functions, sorting, and limiting on top of base path
        // This transforms the execution plan to support:
        // - Aggregation (GROUP BY, HAVING, COUNT/SUM/AVG/MIN/MAX)
        // - Window functions (ROW_NUMBER, RANK, LAG, LEAD, etc.)
        // - Sorting (ORDER BY)
        // - Limiting (LIMIT/OFFSET)
        //
        // Order of layering matters (follows SQL evaluation order):
        // 1. Aggregation (reduces rows via GROUP BY)
        // 2. Window functions (evaluated after GROUP BY, before ORDER BY)
        // 3. Sorting (ORDER BY on results with window functions)
        // 4. Limiting (LIMIT on sorted results)

        std::shared_ptr<Path> final_path = cheapest;

        // Layer 1: Aggregation (Phase 1, Task 4.1)
        std::vector<parser::AggregateExpr*> aggregates;
        bool has_aggregates = detectAggregates(select_stmt, aggregates);
        bool has_group_by = (select_stmt->groupByClause() != nullptr);

        if (has_aggregates || has_group_by)
        {
            DEBUG_LOG_DB("Query has aggregation");
            final_path = addAggregatePath(final_path, select_stmt, aggregates, table_id, ctx);
            if (!final_path)
            {
                DEBUG_LOG_DB("Failed to add aggregate path");
                return nullptr;
            }
        }

        // Layer 2: Window functions (Phase 1, Task 6.2)
        std::vector<parser::WindowFuncExpr*> window_funcs;
        bool has_window_funcs = detectWindowFunctions(select_stmt, window_funcs);

        if (has_window_funcs)
        {
            DEBUG_LOG_DB("Query has window functions");
            final_path = addWindowPath(final_path, select_stmt, window_funcs, ctx);
            if (!final_path)
            {
                DEBUG_LOG_DB("Failed to add window path");
                return nullptr;
            }
        }

        // Layer 3: Sorting (Phase 1, Task 5.1)
        if (!select_stmt->orderByClause().empty())
        {
            DEBUG_LOG_DB("Query has ORDER BY");
            final_path = addSortPath(final_path, select_stmt, ctx);
            if (!final_path)
            {
                DEBUG_LOG_DB("Failed to add sort path");
                return nullptr;
            }
        }

        // Layer 4: Limiting (Phase 1, Task 5.2)
        if (select_stmt->hasLimit())
        {
            DEBUG_LOG_DB("Query has LIMIT");
            final_path = addLimitPath(final_path, select_stmt, ctx);
            if (!final_path)
            {
                DEBUG_LOG_DB("Failed to add limit path");
                return nullptr;
            }
        }

        // Phase 4: Convert to PlanNode
        auto plan = pathToPlanNode(final_path, ctx);
        if (!plan)
        {
            DEBUG_LOG_DB("Failed to convert path to plan node");
            return nullptr;
        }

        DEBUG_LOG_DB("Query planning complete: " + plan->toString());
        return plan;
    }

    auto QueryPlanner::planAnalyze(const parser::AnalyzeStmt *analyze_stmt,
                                    const parser::StringPool &string_pool,
                                    core::ErrorContext *ctx)
        -> std::shared_ptr<PlanNode>
    {
        // For ANALYZE, we don't need a complex plan
        // The executor will call StatisticsManager::analyzeTable() directly
        // This is a placeholder - actual ANALYZE execution happens in executor
        std::string table_name(string_pool.get(analyze_stmt->tableName()));
        DEBUG_LOG_DB("Planning ANALYZE for table: " + table_name);

        // Return nullptr to signal special handling in executor
        // (In a real implementation, we might create an AnalyzeNode)
        return nullptr;
    }

    auto QueryPlanner::generatePaths(const parser::SelectStmt *select_stmt,
                                      const core::ID &table_id,
                                      std::vector<std::shared_ptr<Path>> &paths,
                                      const parser::StringPool &string_pool,
                                      core::ErrorContext *ctx)
        -> core::Status
    {
        std::string table_name(string_pool.get(select_stmt->tableName()));

        // Always generate sequential scan path (always feasible)
        auto seq_scan_path = generateSeqScanPath(select_stmt, table_id, table_name, ctx);
        if (seq_scan_path)
        {
            paths.push_back(seq_scan_path);
            DEBUG_LOG_DB("Generated SeqScanPath: cost=" +
                         std::to_string(seq_scan_path->totalCost()));
        }

        // Generate index scan paths
        core::Status status = generateIndexScanPaths(select_stmt, table_id, table_name, paths, string_pool, ctx);
        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("Index scan path generation had errors (non-fatal)");
            // Non-fatal - we can still use sequential scan
        }

        // Generate R-tree scan paths for spatial queries (Phase 2, Task 9.2)
        status = generateRTreeScanPaths(select_stmt, table_id, table_name, paths, ctx);
        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("R-tree scan path generation had errors (non-fatal)");
            // Non-fatal - we can still use sequential scan or index scan
        }

        return paths.empty() ? core::Status::NOT_FOUND : core::Status::OK;
    }

    auto QueryPlanner::generateSeqScanPath(const parser::SelectStmt *select_stmt,
                                            const core::ID &table_id,
                                            const std::string &table_name,
                                            core::ErrorContext *ctx)
        -> std::shared_ptr<SeqScanPath>
    {
        DEBUG_LOG_DB("Generating sequential scan path for " + table_name);

        // Get table statistics
        TableStatistics table_stats;
        core::Status status = stats_manager_->getTableStatistics(table_id, table_stats, ctx);
        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("No statistics for table " + table_name + ", using defaults");
            // Use default estimates if no statistics available
            table_stats.num_rows = 1000;
            table_stats.num_pages = 10;
        }

        // Estimate selectivity (fraction of rows passing WHERE clause)
        double selectivity = estimateSelectivity(select_stmt, table_id, ctx);
        uint64_t estimated_rows = static_cast<uint64_t>(
            static_cast<double>(table_stats.num_rows) * selectivity);

        DEBUG_LOG_DB("Table stats: rows=" + std::to_string(table_stats.num_rows) +
                     ", pages=" + std::to_string(table_stats.num_pages) +
                     ", selectivity=" + std::to_string(selectivity) +
                     ", estimated_rows=" + std::to_string(estimated_rows));

        // Calculate qualification cost (WHERE clause evaluation)
        double qual_cost = calculateQualCost(select_stmt);

        DEBUG_LOG_DB("Qualification cost: " + std::to_string(qual_cost));

        // Estimate cost using cost model
        CostEstimate cost = cost_model_.costSeqScan(
            table_stats.num_pages,
            table_stats.num_rows,  // We scan all rows, but only 'estimated_rows' pass WHERE
            qual_cost,
            ctx);

        // Adjust row estimate to reflect selectivity
        cost.rows = estimated_rows;

        DEBUG_LOG_DB("SeqScan cost: startup=" + std::to_string(cost.startup_cost) +
                     ", total=" + std::to_string(cost.total_cost) +
                     ", rows=" + std::to_string(cost.rows));

        return std::make_shared<SeqScanPath>(
            table_id,
            table_name,
            table_stats.num_pages,
            table_stats.num_rows,
            qual_cost,
            cost);
    }

    auto QueryPlanner::generateIndexScanPaths(const parser::SelectStmt *select_stmt,
                                               const core::ID &table_id,
                                               const std::string &table_name,
                                               std::vector<std::shared_ptr<Path>> &paths,
                                               const parser::StringPool &string_pool,
                                               core::ErrorContext *ctx)
        -> core::Status
    {
        DEBUG_LOG_DB("Generating index scan paths for " + table_name);

        // Get all indexes on table
        auto indexes = db_->catalog_manager()->getTableIndexes(table_id, ctx);
        if (indexes.empty())
        {
            DEBUG_LOG_DB("No indexes found for table " + table_name);
            return core::Status::OK;  // No indexes is not an error
        }

        DEBUG_LOG_DB("Found " + std::to_string(indexes.size()) + " indexes");

        // Check each index for applicability
        for (const auto &index_info : indexes)
        {
            // Task 17 Phase 8: Check expression indexes
            bool is_applicable = false;
            if (index_info.is_expression_index)
            {
                // Use ExpressionMatcher for expression indexes
                is_applicable = isExpressionIndexApplicable(index_info, select_stmt->whereClause(),
                                                           string_pool, ctx);
            }
            else
            {
                // Use existing logic for regular column indexes
                is_applicable = isIndexApplicable(index_info.index_id, select_stmt, ctx);
            }

            if (!is_applicable)
            {
                DEBUG_LOG_DB("Index " + index_info.index_name + " not applicable");
                continue;
            }

            DEBUG_LOG_DB("Index " + index_info.index_name + " is applicable");

            // Get table statistics
            TableStatistics table_stats;
            core::Status status = stats_manager_->getTableStatistics(table_id, table_stats, ctx);
            if (status != core::Status::OK)
            {
                DEBUG_LOG_DB("No statistics available, using defaults");
                table_stats.num_rows = 1000;
                table_stats.num_pages = 10;
            }

            // Estimate selectivity
            double selectivity = estimateSelectivity(select_stmt, table_id, ctx);
            uint64_t estimated_rows = static_cast<uint64_t>(
                static_cast<double>(table_stats.num_rows) * selectivity);

            // Get index statistics (if available)
            // For now, use simple heuristics
            constexpr uint64_t DEFAULT_INDEX_HEIGHT = 3;
            uint64_t index_height = DEFAULT_INDEX_HEIGHT;
            uint64_t index_pages = static_cast<uint64_t>(
                static_cast<double>(table_stats.num_pages) * 0.1);  // Index is ~10% of table
            uint64_t index_tuples = estimated_rows;

            // Estimate heap pages to fetch
            // This depends on physical ordering correlation
            // For now, assume poor correlation (random heap access)
            uint64_t heap_pages = std::min(estimated_rows, table_stats.num_pages);
            uint64_t heap_tuples = estimated_rows;

            // Get correlation from column statistics (if available)
            double correlation = 0.0;  // Default: assume random ordering
            if (!index_info.column_ids.empty())
            {
                ColumnStatistics col_stats;
                core::Status status = stats_manager_->getColumnStatistics(
                    table_id, index_info.column_ids[0], col_stats, ctx);
                if (status == core::Status::OK)
                {
                    // Correlation would be stored in col_stats if we had it
                    // For now, use default
                    correlation = 0.0;
                }
            }

            // Calculate qualification cost
            double qual_cost = calculateQualCost(select_stmt);

            // LSM Integration Phase 5: Use appropriate cost model for index type
            CostEstimate cost;
            if (index_info.index_type == core::CatalogManager::IndexType::LSM)
            {
                // LSM-Tree cost estimation
                // Estimate number of levels and SSTables based on table size
                uint64_t num_levels = std::min(4UL, static_cast<uint64_t>(
                    std::ceil(std::log2(static_cast<double>(table_stats.num_rows) / 1000.0))));
                if (num_levels == 0) num_levels = 1;  // At least one level

                uint64_t avg_sstables_per_level = 2;  // Conservative estimate

                cost = cost_model_.costLSMScan(
                    num_levels,
                    avg_sstables_per_level,
                    index_tuples,
                    heap_pages,
                    heap_tuples,
                    qual_cost,
                    correlation,
                    ctx);

                DEBUG_LOG_DB("LSMScan cost for " + index_info.index_name +
                             ": startup=" + std::to_string(cost.startup_cost) +
                             ", total=" + std::to_string(cost.total_cost) +
                             ", rows=" + std::to_string(cost.rows) +
                             " (levels=" + std::to_string(num_levels) + ")");
            }
            else
            {
                // B-Tree or other index types: use traditional index scan cost
                cost = cost_model_.costIndexScan(
                    index_height,
                    index_pages,
                    index_tuples,
                    heap_pages,
                    heap_tuples,
                    qual_cost,
                    correlation,
                    ctx);

                DEBUG_LOG_DB("IndexScan cost for " + index_info.index_name +
                             ": startup=" + std::to_string(cost.startup_cost) +
                             ", total=" + std::to_string(cost.total_cost) +
                             ", rows=" + std::to_string(cost.rows));
            }

            // Create IndexScanPath
            auto index_path = std::make_shared<IndexScanPath>(
                table_id,
                table_name,
                index_info.index_id,
                index_info.index_name,
                index_height,
                index_pages,
                index_tuples,
                heap_pages,
                heap_tuples,
                qual_cost,
                correlation,
                cost);

            paths.push_back(index_path);
        }

        return core::Status::OK;
    }

    auto QueryPlanner::isIndexApplicable(const core::ID &index_id,
                                          const parser::SelectStmt *select_stmt,
                                          core::ErrorContext *ctx) const
        -> bool
    {
        // For Phase 1, use simple heuristic:
        // Index is applicable if query has a WHERE clause
        // (Detailed predicate analysis will be in Phase 2)

        // If no WHERE clause, index scan is not beneficial
        // (Sequential scan will be faster for full table scan)
        if (!select_stmt->whereClause())
        {
            DEBUG_LOG_DB("No WHERE clause - index not applicable");
            return false;
        }

        // TODO Phase 2: Analyze WHERE clause to check if index column is used
        // TODO Phase 2: Check operator compatibility with index type
        // For now, assume any index is applicable if WHERE exists
        return true;
    }

    auto QueryPlanner::isSpatialPredicate(const parser::Expression *expr,
                                          std::string &column_name,
                                          std::string &function_name) const
        -> bool
    {
        // Check if expression is a function call
        auto *func_call = dynamic_cast<const parser::FunctionCallExpr*>(expr);
        if (!func_call)
        {
            return false;
        }

        // Get function name - need to resolve StringId
        // For now, we can't easily compare StringIds without the string pool
        // This is a simplified implementation for Phase 2
        // TODO: Pass string pool to properly resolve function names

        // Check if first argument is an identifier (column reference)
        if (func_call->args().empty())
        {
            return false;
        }

        auto *col_ref = dynamic_cast<const parser::IdentifierExpr*>(func_call->args()[0]);
        if (!col_ref)
        {
            return false;
        }

        // For Phase 2, we'll mark this as a spatial predicate candidate
        // The actual function name checking will be done during execution
        // when we have access to the string pool
        column_name = "(spatial_column)";  // Placeholder
        function_name = "(spatial_function)";  // Placeholder

        DEBUG_LOG_DB("Found potential spatial predicate function call");
        return true;
    }

    auto QueryPlanner::generateRTreeScanPaths(const parser::SelectStmt *select_stmt,
                                              const core::ID &table_id,
                                              const std::string &table_name,
                                              std::vector<std::shared_ptr<Path>> &paths,
                                              core::ErrorContext *ctx)
        -> core::Status
    {
        // No WHERE clause means no spatial predicates
        if (!select_stmt->whereClause())
        {
            return core::Status::OK;
        }

        // Find spatial predicates in WHERE clause
        std::vector<std::pair<std::string, std::string>> spatial_predicates;  // column_name, function_name

        // Simple check: look at WHERE clause expression
        std::string column_name, function_name;
        if (isSpatialPredicate(select_stmt->whereClause(), column_name, function_name))
        {
            spatial_predicates.push_back({column_name, function_name});
        }

        // If no spatial predicates found, nothing to do
        if (spatial_predicates.empty())
        {
            DEBUG_LOG_DB("No spatial predicates found for R-tree optimization");
            return core::Status::OK;
        }

        // Get all indexes for this table
        auto indexes = db_->catalog_manager()->getTableIndexes(table_id, ctx);

        // For each R-tree index, create a scan path if spatial predicates exist
        for (const auto &index_info : indexes)
        {
            // Check if this is an R-tree index
            if (index_info.index_type != core::CatalogManager::IndexType::RTREE)
            {
                continue;
            }

            // If we found spatial predicates, assume this R-tree can be used
            // Phase 2 simplified approach - proper column matching will be in Phase 3
            std::string matching_function = spatial_predicates[0].second;

            DEBUG_LOG_DB("R-tree index " + index_info.index_name +
                        " applicable for spatial predicate " + matching_function);

            // Get table statistics
            TableStatistics table_stats;
            core::Status status = stats_manager_->getTableStatistics(table_id, table_stats, ctx);
            if (status != core::Status::OK)
            {
                // Use default statistics
                table_stats.num_rows = 1000;
                table_stats.num_pages = 100;
            }

            // Estimate spatial selectivity
            // For Phase 2, use simple heuristic: spatial predicates are typically selective
            double selectivity = 0.01;  // Assume 1% of rows match (conservative estimate)
            uint64_t estimated_rows = static_cast<uint64_t>(
                static_cast<double>(table_stats.num_rows) * selectivity);

            if (estimated_rows == 0)
            {
                estimated_rows = 1;
            }

            // R-tree cost estimation
            constexpr uint64_t DEFAULT_RTREE_HEIGHT = 3;  // Typical R-tree height
            uint64_t tree_height = DEFAULT_RTREE_HEIGHT;

            // R-tree pages accessed = tree height + matched entries
            uint64_t tree_pages = tree_height + (estimated_rows / 10);  // ~10 entries per page
            uint64_t matched_entries = estimated_rows;

            // Heap pages to fetch (spatial: correlation = 0.0, random access)
            uint64_t heap_pages = std::min(estimated_rows, table_stats.num_pages);
            uint64_t heap_tuples = estimated_rows;

            // Calculate qualification cost
            double qual_cost = calculateQualCost(select_stmt);

            // Estimate R-tree scan cost using same model as B-tree index scan
            CostEstimate cost = cost_model_.costIndexScan(
                tree_height,
                tree_pages,
                matched_entries,
                heap_pages,
                heap_tuples,
                qual_cost,
                0.0,  // correlation = 0 for spatial (random heap access)
                ctx);

            DEBUG_LOG_DB("R-tree scan cost for " + index_info.index_name +
                        ": startup=" + std::to_string(cost.startup_cost) +
                        ", total=" + std::to_string(cost.total_cost) +
                        ", rows=" + std::to_string(cost.rows));

            // Create RTreeScanPath
            auto rtree_path = std::make_shared<RTreeScanPath>(
                table_id,
                table_name,
                index_info.index_id,
                index_info.index_name,
                tree_height,
                tree_pages,
                matched_entries,
                heap_pages,
                heap_tuples,
                qual_cost,
                matching_function,
                cost);

            paths.push_back(rtree_path);
        }

        return core::Status::OK;
    }

    auto QueryPlanner::selectCheapestPath(const std::vector<std::shared_ptr<Path>> &paths) const
        -> std::shared_ptr<Path>
    {
        if (paths.empty())
        {
            return nullptr;
        }

        // Find path with minimum total cost
        auto cheapest = paths[0];
        for (size_t i = 1; i < paths.size(); i++)
        {
            if (paths[i]->totalCost() < cheapest->totalCost())
            {
                cheapest = paths[i];
            }
            else if (paths[i]->totalCost() == cheapest->totalCost())
            {
                // Tie-breaker: prefer index scan (provides ordering)
                if (paths[i]->type() == PathType::INDEX_SCAN &&
                    cheapest->type() == PathType::SEQ_SCAN)
                {
                    cheapest = paths[i];
                }
            }
        }

        DEBUG_LOG_DB("Selected cheapest path: " + cheapest->toString() +
                     " (cost=" + std::to_string(cheapest->totalCost()) + ")");

        return cheapest;
    }

    auto QueryPlanner::pathToPlanNode(const std::shared_ptr<Path> &path,
                                       core::ErrorContext *ctx)
        -> std::shared_ptr<PlanNode>
    {
        if (!path)
        {
            return nullptr;
        }

        if (path->type() == PathType::SEQ_SCAN)
        {
            auto seq_path = std::static_pointer_cast<SeqScanPath>(path);

            auto plan = std::make_shared<SeqScanNode>(
                seq_path->tableId(),
                seq_path->tableName(),
                ScanDirection::FORWARD);

            plan->setCost(
                path->startupCost(),
                path->totalCost(),
                path->rows());

            plan->setQualCost(seq_path->qualCost());

            // TODO: Set filter expression from WHERE clause
            // For now, just mark that there is a filter
            plan->setFilter("(WHERE clause)");

            return plan;
        }
        else if (path->type() == PathType::INDEX_SCAN)
        {
            auto index_path = std::static_pointer_cast<IndexScanPath>(path);

            auto plan = std::make_shared<IndexScanNode>(
                index_path->tableId(),
                index_path->tableName(),
                index_path->indexId(),
                index_path->indexName(),
                ScanDirection::FORWARD);

            plan->setCost(
                path->startupCost(),
                path->totalCost(),
                path->rows());

            plan->setIndexQualCost(index_path->qualCost());
            plan->setCorrelation(index_path->correlation());

            // TODO: Set index condition and filter from WHERE clause
            plan->setIndexCond("(index condition)");
            plan->setFilter("(additional filter)");

            return plan;
        }
        else if (path->type() == PathType::RTREE_SCAN)
        {
            auto rtree_path = std::static_pointer_cast<RTreeScanPath>(path);

            auto plan = std::make_shared<RTreeScanNode>(
                rtree_path->tableId(),
                rtree_path->tableName(),
                rtree_path->indexId(),
                rtree_path->indexName(),
                rtree_path->predicateType());

            plan->setCost(
                path->startupCost(),
                path->totalCost(),
                path->rows());

            // TODO: Set spatial condition and filter from WHERE clause
            // For now, just mark that there is a spatial condition
            plan->setSpatialCond("(" + rtree_path->predicateType() + " condition)");
            plan->setFilter("(additional filter)");

            return plan;
        }
        else if (path->type() == PathType::AGGREGATE)
        {
            auto agg_path = std::static_pointer_cast<AggregatePath>(path);

            // Recursively convert child path
            auto child_plan = pathToPlanNode(agg_path->inputPath(), ctx);
            if (!child_plan)
            {
                return nullptr;
            }

            // Create aggregate node
            auto plan = std::make_shared<AggregateNode>(
                child_plan,
                agg_path->groupingExprs(),
                agg_path->aggregates(),
                agg_path->havingClause());

            plan->setCost(
                path->startupCost(),
                path->totalCost(),
                path->rows());

            return plan;
        }
        else if (path->type() == PathType::SORT)
        {
            auto sort_path = std::static_pointer_cast<SortPath>(path);

            // Recursively convert child path
            auto child_plan = pathToPlanNode(sort_path->inputPath(), ctx);
            if (!child_plan)
            {
                return nullptr;
            }

            // Create sort node
            auto plan = std::make_shared<SortNode>(
                child_plan,
                sort_path->orderByItems());

            plan->setCost(
                path->startupCost(),
                path->totalCost(),
                path->rows());

            return plan;
        }
        else if (path->type() == PathType::LIMIT)
        {
            auto limit_path = std::static_pointer_cast<LimitPath>(path);

            // Recursively convert child path
            auto child_plan = pathToPlanNode(limit_path->inputPath(), ctx);
            if (!child_plan)
            {
                return nullptr;
            }

            // Create limit node
            auto plan = std::make_shared<LimitNode>(
                child_plan,
                limit_path->limitCount(),
                limit_path->offsetCount());

            plan->setCost(
                path->startupCost(),
                path->totalCost(),
                path->rows());

            return plan;
        }
        else if (path->type() == PathType::WINDOW)
        {
            auto win_path = std::static_pointer_cast<WindowPath>(path);

            // Recursively convert child path
            auto child_plan = pathToPlanNode(win_path->inputPath(), ctx);
            if (!child_plan)
            {
                return nullptr;
            }

            // Convert WindowFuncExpr list to WindowNode::WindowFunction list
            std::vector<WindowNode::WindowFunction> window_functions;
            for (const auto* win_expr : win_path->windowFuncs())
            {
                // Generate output column name (e.g., "row_number_1", "rank_2")
                std::string func_name;
                switch (win_expr->func())
                {
                    case parser::WindowFunc::ROW_NUMBER: func_name = "row_number"; break;
                    case parser::WindowFunc::RANK: func_name = "rank"; break;
                    case parser::WindowFunc::DENSE_RANK: func_name = "dense_rank"; break;
                    case parser::WindowFunc::LAG: func_name = "lag"; break;
                    case parser::WindowFunc::LEAD: func_name = "lead"; break;
                    case parser::WindowFunc::FIRST_VALUE: func_name = "first_value"; break;
                    case parser::WindowFunc::LAST_VALUE: func_name = "last_value"; break;
                    case parser::WindowFunc::NTH_VALUE: func_name = "nth_value"; break;
                }

                std::string output_col = func_name + "_" + std::to_string(window_functions.size() + 1);

                window_functions.emplace_back(
                    win_expr->func(),
                    win_expr->args(),
                    win_expr->windowSpec(),
                    output_col);
            }

            // Create window node
            auto plan = std::make_shared<WindowNode>(
                child_plan,
                window_functions);

            plan->setCost(
                path->startupCost(),
                path->totalCost(),
                path->rows());

            return plan;
        }

        DEBUG_LOG_DB("Unknown path type");
        return nullptr;
    }

    auto QueryPlanner::estimateSelectivity(const parser::SelectStmt *select_stmt,
                                            const core::ID &table_id,
                                            core::ErrorContext *ctx)
        -> double
    {
        // Use SelectivityEstimator for histogram-based estimation
        return selectivity_estimator_.estimateWhereClause(
            select_stmt->whereClause(), table_id, ctx);
    }

    auto QueryPlanner::calculateQualCost(const parser::SelectStmt *select_stmt) const
        -> double
    {
        if (!select_stmt->whereClause())
        {
            return 0.0;
        }

        // For Phase 1, estimate one comparison operator per WHERE clause
        // Phase 2 will traverse the expression tree to get exact count
        double qual_cost = cost_model_.operatorCost("=");

        DEBUG_LOG_DB("Qualification cost estimate: " + std::to_string(qual_cost));

        return qual_cost;
    }

    auto QueryPlanner::planJoinQuery(const parser::SelectStmt *select_stmt,
                                     const parser::StringPool &string_pool,
                                     core::ErrorContext *ctx)
        -> std::shared_ptr<PlanNode>
    {
        DEBUG_LOG_DB("Planning JOIN query with " +
                   std::to_string(select_stmt->fromClause().joins.size()) + " joins");

        // For Phase 1: Greedy join ordering (join in query order)
        // Phase 2 will implement dynamic programming for optimal ordering

        // Step 1: Generate path for base table
        auto base_paths = generateBaseRelationPaths(
            select_stmt->fromClause().base_table,
            select_stmt->whereClause(),
            string_pool,
            ctx);

        if (base_paths.empty())
        {
            DEBUG_LOG_DB("Failed to generate paths for base table");
            return nullptr;
        }

        // Use cheapest path for base table
        std::shared_ptr<Path> current_path = selectCheapestPath(base_paths);

        // Step 2: For each join, generate paths and select cheapest
        for (const auto &join : select_stmt->fromClause().joins)
        {
            // Generate paths for right table
            auto right_paths = generateBaseRelationPaths(
                join.right_table,
                nullptr,  // No WHERE clause for joined table
                string_pool,
                ctx);

            if (right_paths.empty())
            {
                DEBUG_LOG_DB("Failed to generate paths for joined table");
                continue;
            }

            auto right_path = selectCheapestPath(right_paths);

            // Generate join paths
            auto join_paths = generateJoinPaths(current_path, right_path, join, ctx);

            if (join_paths.empty())
            {
                DEBUG_LOG_DB("Failed to generate join paths");
                continue;
            }

            // Select cheapest join path
            current_path = selectCheapestPath(join_paths);

            DEBUG_LOG_DB("Selected cheapest join path: type=" +
                       std::to_string(static_cast<int>(current_path->type())) +
                       ", cost=" + std::to_string(current_path->totalCost()));
        }

        // Step 3: Convert final path to plan node
        return joinPathToPlanNode(current_path);
    }

    auto QueryPlanner::generateBaseRelationPaths(
        const parser::TableRef &table_ref,
        const parser::Expression *where_clause,
        const parser::StringPool &string_pool,
        core::ErrorContext *ctx)
        -> std::vector<std::shared_ptr<Path>>
    {
        std::vector<std::shared_ptr<Path>> paths;

        // Look up table in catalog
        std::string table_name(string_pool.get(table_ref.table_name));

        // Get default PUBLIC schema
        core::CatalogManager::SchemaInfo schema_info;
        core::Status status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, ctx);
        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("Failed to get PUBLIC schema");
            return paths;
        }

        // Look up table
        core::CatalogManager::TableInfo table_info;
        status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info, ctx);
        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("Table not found: " + table_name);
            return paths;
        }

        core::ID table_id = table_info.table_id;

        // Get table statistics
        uint64_t num_pages = 100;    // TODO: Get from catalog
        uint64_t num_tuples = 10000; // TODO: Get from catalog

        double selectivity = 1.0;
        if (where_clause)
        {
            selectivity = selectivity_estimator_.estimateWhereClause(
                where_clause, table_id, ctx);
        }

        uint64_t output_rows = static_cast<uint64_t>(num_tuples * selectivity);
        double qual_cost = where_clause ? 0.01 : 0.0;

        CostEstimate cost = cost_model_.costSeqScan(num_pages, num_tuples, qual_cost, ctx);

        auto seq_scan_path = std::make_shared<SeqScanPath>(
            table_id,
            table_name,
            num_pages,
            output_rows,
            qual_cost,
            cost);

        paths.push_back(seq_scan_path);

        DEBUG_LOG_DB("Generated SeqScan path for " + table_name +
                   ": cost=" + std::to_string(cost.total_cost) +
                   ", rows=" + std::to_string(output_rows));

        // TODO: Also generate index scan paths

        return paths;
    }

    auto QueryPlanner::generateJoinPaths(
        std::shared_ptr<Path> left_path,
        std::shared_ptr<Path> right_path,
        const parser::JoinClause &join_clause,
        core::ErrorContext *ctx)
        -> std::vector<std::shared_ptr<Path>>
    {
        std::vector<std::shared_ptr<Path>> join_paths;

        // Get table IDs for selectivity estimation
        core::ID left_table_id;
        core::ID right_table_id;

        // Extract table IDs from paths
        if (left_path->type() == PathType::SEQ_SCAN)
        {
            auto *seq_path = static_cast<SeqScanPath *>(left_path.get());
            left_table_id = seq_path->tableId();
        }
        if (right_path->type() == PathType::SEQ_SCAN)
        {
            auto *seq_path = static_cast<SeqScanPath *>(right_path.get());
            right_table_id = seq_path->tableId();
        }

        // Estimate join selectivity
        double selectivity = selectivity_estimator_.estimateJoinSelectivity(
            join_clause.on_condition,
            left_table_id,
            right_table_id,
            ctx);

        uint64_t left_rows = left_path->rows();
        uint64_t right_rows = right_path->rows();

        // 1. Generate Nested Loop Join Path (always applicable)
        {
            CostEstimate nl_cost = cost_model_.costNestedLoopJoin(
                left_path->cost(),
                right_path->cost(),
                left_rows,
                right_rows,
                selectivity,
                ctx);

            auto nl_path = std::make_shared<NestedLoopJoinPath>(
                join_clause.join_type,
                left_path,
                right_path,
                join_clause.on_condition,
                selectivity,
                nl_cost);

            join_paths.push_back(nl_path);

            DEBUG_LOG_DB("Generated Nested Loop Join path: cost=" +
                       std::to_string(nl_cost.total_cost) +
                       ", rows=" + std::to_string(nl_cost.rows));
        }

        // 2. Generate Hash Join Path (only if equi-join)
        if (isHashJoinApplicable(join_clause.on_condition))
        {
            std::vector<parser::Expression *> hash_keys_left;
            std::vector<parser::Expression *> hash_keys_right;

            if (extractHashKeys(join_clause.on_condition, hash_keys_left, hash_keys_right))
            {
                // Choose smaller relation as build side
                std::shared_ptr<Path> build_path = (left_rows < right_rows) ? left_path : right_path;
                std::shared_ptr<Path> probe_path = (left_rows < right_rows) ? right_path : left_path;
                uint64_t build_rows = build_path->rows();
                uint64_t probe_rows = probe_path->rows();

                CostEstimate hash_cost = cost_model_.costHashJoin(
                    build_path->cost(),
                    probe_path->cost(),
                    build_rows,
                    probe_rows,
                    selectivity,
                    ctx);

                auto hash_path = std::make_shared<HashJoinPath>(
                    join_clause.join_type,
                    build_path,
                    probe_path,
                    join_clause.on_condition,
                    hash_keys_left,
                    hash_keys_right,
                    selectivity,
                    hash_cost);

                join_paths.push_back(hash_path);

                DEBUG_LOG_DB("Generated Hash Join path: cost=" +
                           std::to_string(hash_cost.total_cost) +
                           ", rows=" + std::to_string(hash_cost.rows));
            }
        }

        return join_paths;
    }

    auto QueryPlanner::isHashJoinApplicable(const parser::Expression *join_condition) const
        -> bool
    {
        if (!join_condition)
            return false;

        // Check if condition contains equality
        if (join_condition->kind() == parser::ASTKind::BINARY_OP)
        {
            auto *binary_op = static_cast<const parser::BinaryOpExpr *>(join_condition);

            if (binary_op->op() == parser::BinaryOp::EQ)
            {
                // Simple equality - hash join applicable
                return true;
            }
            else if (binary_op->op() == parser::BinaryOp::AND)
            {
                // Compound condition - check if both sides have equality
                return isHashJoinApplicable(binary_op->left()) &&
                       isHashJoinApplicable(binary_op->right());
            }
        }

        return false;
    }

    auto QueryPlanner::extractHashKeys(
        const parser::Expression *join_condition,
        std::vector<parser::Expression *> &left_keys,
        std::vector<parser::Expression *> &right_keys) const
        -> bool
    {
        if (!join_condition)
            return false;

        if (join_condition->kind() == parser::ASTKind::BINARY_OP)
        {
            auto *binary_op = static_cast<const parser::BinaryOpExpr *>(join_condition);

            if (binary_op->op() == parser::BinaryOp::EQ)
            {
                // Extract left and right column references
                // Note: This is a simplified version for Phase 1
                // Phase 2 would verify which columns belong to which table
                left_keys.push_back(binary_op->left());
                right_keys.push_back(binary_op->right());
                return true;
            }
            else if (binary_op->op() == parser::BinaryOp::AND)
            {
                // Recursively extract from both sides
                bool left_ok = extractHashKeys(binary_op->left(), left_keys, right_keys);
                bool right_ok = extractHashKeys(binary_op->right(), left_keys, right_keys);
                return left_ok && right_ok;
            }
        }

        return false;
    }

    auto QueryPlanner::joinPathToPlanNode(std::shared_ptr<Path> path)
        -> std::shared_ptr<PlanNode>
    {
        if (path->type() == PathType::SEQ_SCAN)
        {
            // Leaf node - convert to SeqScanNode
            auto *seq_path = static_cast<SeqScanPath *>(path.get());
            auto scan_node = std::make_shared<SeqScanNode>(
                seq_path->tableId(),
                seq_path->tableName(),
                ScanDirection::FORWARD);
            scan_node->setCost(
                path->startupCost(),
                path->totalCost(),
                path->rows());
            return scan_node;
        }
        else if (path->type() == PathType::INDEX_SCAN)
        {
            // Index scan node
            auto *idx_path = static_cast<IndexScanPath *>(path.get());
            auto scan_node = std::make_shared<IndexScanNode>(
                idx_path->tableId(),
                idx_path->tableName(),
                idx_path->indexId(),
                idx_path->indexName(),
                ScanDirection::FORWARD);
            scan_node->setCost(
                path->startupCost(),
                path->totalCost(),
                path->rows());
            return scan_node;
        }
        else if (path->type() == PathType::NESTED_LOOP_JOIN)
        {
            auto *nl_path = static_cast<NestedLoopJoinPath *>(path.get());

            // Recursively convert children
            auto outer_node = joinPathToPlanNode(nl_path->outerPath());
            auto inner_node = joinPathToPlanNode(nl_path->innerPath());

            auto join_node = std::make_shared<NestedLoopJoinNode>(
                nl_path->joinType(),
                outer_node,
                inner_node,
                nl_path->joinCondition());

            join_node->setCost(
                path->startupCost(),
                path->totalCost(),
                path->rows());

            return join_node;
        }
        else if (path->type() == PathType::HASH_JOIN)
        {
            auto *hash_path = static_cast<HashJoinPath *>(path.get());

            // Recursively convert children
            auto outer_node = joinPathToPlanNode(hash_path->outerPath());
            auto inner_node = joinPathToPlanNode(hash_path->innerPath());

            auto join_node = std::make_shared<HashJoinNode>(
                hash_path->joinType(),
                outer_node,
                inner_node,
                hash_path->joinCondition(),
                hash_path->hashKeysOuter(),
                hash_path->hashKeysInner());

            join_node->setCost(
                path->startupCost(),
                path->totalCost(),
                path->rows());

            return join_node;
        }

        // Unknown path type
        DEBUG_LOG_DB("Unknown path type in joinPathToPlanNode");
        return nullptr;
    }

    // Phase 1, Task 4.1: Aggregation support

    auto QueryPlanner::detectAggregates(const parser::SelectStmt *select_stmt,
                                        std::vector<parser::AggregateExpr*>& aggregates) const
        -> bool
    {
        aggregates.clear();

        // Scan SELECT list for aggregate expressions
        for (const auto& select_item : select_stmt->selectList())
        {
            parser::Expression* expr = select_item.expr;

            // Check if expression is an aggregate
            if (auto agg_expr = dynamic_cast<parser::AggregateExpr*>(expr))
            {
                aggregates.push_back(agg_expr);
            }
        }

        return !aggregates.empty();
    }

    auto QueryPlanner::estimateNumGroups(const parser::SelectStmt *select_stmt,
                                         const core::ID& table_id,
                                         core::ErrorContext *ctx) const
        -> uint64_t
    {
        // If no GROUP BY, this is simple aggregation (1 group)
        const auto* group_by = select_stmt->groupByClause();
        if (!group_by)
        {
            return 1;
        }

        // Estimate based on n_distinct of grouping columns
        // For now, use a simple heuristic:
        // - Single column: use n_distinct from statistics
        // - Multiple columns: multiply n_distinct values (with cap)

        uint64_t num_groups = 1;

        for (const auto& expr : group_by->grouping_exprs)
        {
            // For simple case, assume each grouping expression has ~100 distinct values
            // In production, we would look up actual n_distinct from statistics
            constexpr uint64_t DEFAULT_DISTINCT = 100;
            num_groups *= DEFAULT_DISTINCT;

            // Cap at reasonable limit to avoid overflow
            constexpr uint64_t MAX_GROUPS = 1000000;
            if (num_groups > MAX_GROUPS)
            {
                num_groups = MAX_GROUPS;
                break;
            }
        }

        DEBUG_LOG_DB("Estimated " + std::to_string(num_groups) + " groups for GROUP BY");
        return num_groups;
    }

    auto QueryPlanner::estimateRowWidth(const parser::SelectStmt *select_stmt,
                                        core::ErrorContext *ctx) const
        -> uint64_t
    {
        // Estimate average row width for sort cost estimation
        // For now, use a simple heuristic based on number of columns

        size_t num_columns = select_stmt->selectList().size();

        // Assume average column width:
        // - 8 bytes for integers
        // - 8 bytes for float/double
        // - 32 bytes for strings (average)
        constexpr uint64_t AVG_COLUMN_WIDTH = 16;

        uint64_t row_width = num_columns * AVG_COLUMN_WIDTH;

        // Add some overhead for tuple header
        constexpr uint64_t TUPLE_HEADER_SIZE = 24;
        row_width += TUPLE_HEADER_SIZE;

        DEBUG_LOG_DB("Estimated row width: " + std::to_string(row_width) + " bytes");
        return row_width;
    }

    auto QueryPlanner::addAggregatePath(std::shared_ptr<Path> base_path,
                                        const parser::SelectStmt *select_stmt,
                                        const std::vector<parser::AggregateExpr*>& aggregates,
                                        const core::ID& table_id,
                                        core::ErrorContext *ctx)
        -> std::shared_ptr<Path>
    {
        DEBUG_LOG_DB("Adding aggregate path");

        // Get GROUP BY clause
        const auto* group_by = select_stmt->groupByClause();

        // Estimate number of groups
        uint64_t num_groups = estimateNumGroups(select_stmt, table_id, ctx);

        // Get input rows from base path
        uint64_t input_rows = base_path->rows();

        // Calculate aggregate cost
        CostEstimate agg_cost = cost_model_.costAggregate(
            input_rows,
            num_groups,
            aggregates.size(),
            ctx);

        // Add base path cost
        agg_cost.startup_cost += base_path->totalCost();
        agg_cost.total_cost = agg_cost.startup_cost + agg_cost.run_cost;

        // Extract GROUP BY expressions and HAVING clause
        std::vector<parser::Expression*> grouping_exprs;
        parser::Expression* having_clause = nullptr;
        if (group_by)
        {
            grouping_exprs = group_by->grouping_exprs;
            having_clause = group_by->having_clause;
        }

        // Create aggregate path
        auto agg_path = std::make_shared<AggregatePath>(
            base_path,
            grouping_exprs,
            aggregates,
            having_clause,
            num_groups,
            agg_cost);

        DEBUG_LOG_DB("Aggregate path: " + agg_path->toString());
        return agg_path;
    }

    auto QueryPlanner::addSortPath(std::shared_ptr<Path> base_path,
                                   const parser::SelectStmt *select_stmt,
                                   core::ErrorContext *ctx)
        -> std::shared_ptr<Path>
    {
        DEBUG_LOG_DB("Adding sort path");

        // Get ORDER BY clause
        const auto& order_by = select_stmt->orderByClause();

        // Get input rows from base path
        uint64_t input_rows = base_path->rows();

        // Estimate row width
        uint64_t row_width = estimateRowWidth(select_stmt, ctx);

        // Calculate sort cost
        CostEstimate sort_cost = cost_model_.costSort(
            input_rows,
            row_width,
            order_by.size(),
            ctx);

        // Add base path cost
        sort_cost.startup_cost += base_path->totalCost();
        sort_cost.total_cost = sort_cost.startup_cost + sort_cost.run_cost;

        // Create sort path
        auto sort_path = std::make_shared<SortPath>(
            base_path,
            order_by,
            row_width,
            sort_cost);

        DEBUG_LOG_DB("Sort path: " + sort_path->toString());
        return sort_path;
    }

    auto QueryPlanner::addLimitPath(std::shared_ptr<Path> base_path,
                                    const parser::SelectStmt *select_stmt,
                                    core::ErrorContext *ctx)
        -> std::shared_ptr<Path>
    {
        DEBUG_LOG_DB("Adding limit path");

        // Get LIMIT/OFFSET values
        int64_t limit_count = select_stmt->limitCount();
        int64_t offset_count = select_stmt->offsetCount();

        // Get input rows from base path
        uint64_t input_rows = base_path->rows();

        // Calculate limit cost
        CostEstimate limit_cost = cost_model_.costLimit(
            input_rows,
            limit_count,
            offset_count,
            ctx);

        // Add base path startup cost (LIMIT affects run cost, not startup)
        limit_cost.startup_cost += base_path->startupCost();
        limit_cost.total_cost = limit_cost.startup_cost + limit_cost.run_cost;

        // Create limit path
        auto limit_path = std::make_shared<LimitPath>(
            base_path,
            limit_count,
            offset_count,
            limit_cost);

        DEBUG_LOG_DB("Limit path: " + limit_path->toString());
        return limit_path;
    }

    // Phase 1, Task 6.2: Window function support

    auto QueryPlanner::detectWindowFunctions(const parser::SelectStmt *select_stmt,
                                            std::vector<parser::WindowFuncExpr*>& window_funcs) const
        -> bool
    {
        window_funcs.clear();

        // Scan SELECT list for window function expressions
        for (const auto& select_item : select_stmt->selectList())
        {
            parser::Expression* expr = select_item.expr;

            // Check if expression is a window function
            if (auto win_expr = dynamic_cast<parser::WindowFuncExpr*>(expr))
            {
                window_funcs.push_back(win_expr);
            }
        }

        return !window_funcs.empty();
    }

    auto QueryPlanner::addWindowPath(std::shared_ptr<Path> base_path,
                                    const parser::SelectStmt *select_stmt,
                                    const std::vector<parser::WindowFuncExpr*>& window_funcs,
                                    core::ErrorContext *ctx)
        -> std::shared_ptr<Path>
    {
        DEBUG_LOG_DB("Adding window path");

        // Get input rows from base path
        uint64_t input_rows = base_path->rows();

        // Calculate window function processing cost
        // Window functions require:
        // 1. Sorting by PARTITION BY + ORDER BY columns (if present)
        // 2. Frame window processing for each function
        // 3. Per-row evaluation of window functions

        double window_cost = 0.0;
        double startup_cost = base_path->startupCost();

        for (const auto* win_func : window_funcs)
        {
            const parser::WindowSpec* spec = win_func->windowSpec();

            // Cost 1: Sorting cost (if ORDER BY present)
            if (spec && !spec->orderBy().empty())
            {
                // Sorting cost: O(n log n) comparisons
                double n = static_cast<double>(input_rows);
                double sort_cost = n * std::log2(n + 1.0) * cost_model_.operatorCost("=");
                window_cost += sort_cost;
            }

            // Cost 2: Partition processing cost
            // For each partition, we need to:
            // - Identify partition boundaries: O(n) scan
            // - Process frame windows within partition
            double partition_cost = static_cast<double>(input_rows) * cost_model_.operatorCost("=");
            window_cost += partition_cost;

            // Cost 3: Frame window processing cost
            // Depends on frame specification and window function type
            if (spec && spec->hasFrame())
            {
                // Frame processing requires maintaining window state
                // Cost varies by frame mode:
                // - ROWS: O(1) per row (simple offset arithmetic)
                // - RANGE: O(log n) per row (need to find frame boundaries)
                double frame_cost = static_cast<double>(input_rows);
                if (spec->frameMode() == parser::FrameMode::RANGE)
                {
                    frame_cost *= std::log2(static_cast<double>(input_rows) + 1.0);
                }
                frame_cost *= cost_model_.operatorCost("=");
                window_cost += frame_cost;
            }
            else
            {
                // Default frame: RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
                // This requires accumulating state but no complex lookups
                double frame_cost = static_cast<double>(input_rows) * cost_model_.operatorCost("=");
                window_cost += frame_cost;
            }

            // Cost 4: Function evaluation cost
            // Depends on function type:
            // - ROW_NUMBER/RANK/DENSE_RANK: O(1) per row
            // - LAG/LEAD: O(1) per row (simple offset lookup)
            // - FIRST_VALUE/LAST_VALUE/NTH_VALUE: O(1) per row (frame boundary lookup)
            double eval_cost = static_cast<double>(input_rows) * cost_model_.operatorCost("=");
            window_cost += eval_cost;
        }

        // Create cost estimate
        CostEstimate win_cost;
        win_cost.startup_cost = startup_cost;
        win_cost.run_cost = base_path->cost().run_cost + window_cost;
        win_cost.total_cost = win_cost.startup_cost + win_cost.run_cost;
        win_cost.rows = input_rows; // Window functions don't change row count

        // Create window path
        auto window_path = std::make_shared<WindowPath>(
            base_path,
            window_funcs,
            win_cost);

        DEBUG_LOG_DB("Window path: " + window_path->toString());
        return window_path;
    }

    auto QueryPlanner::isExpressionIndexApplicable(const core::CatalogManager::IndexInfo &index_info,
                                                   const parser::Expression *where_clause,
                                                   const parser::StringPool &string_pool,
                                                   core::ErrorContext *ctx) const
        -> bool
    {
        // Task 17 Phase 8-9: Expression Index and Filtered Index Matching

        // No WHERE clause - expression/filtered index not beneficial for full table scan
        if (!where_clause)
        {
            DEBUG_LOG_DB("No WHERE clause - expression/filtered index not beneficial");
            return false;
        }

        // Deserialize index expressions and predicate
        parser::StringPool temp_pool;
        std::vector<parser::Expression *> index_expressions;
        parser::Expression *index_predicate = nullptr;

        // Task 17 Phase 8: Deserialize expression index columns
        if (index_info.is_expression_index)
        {
            try
            {
                index_expressions = core::ExpressionSerializer::deserializeList(
                    index_info.expression_data.data(),
                    index_info.expression_data.size(),
                    temp_pool);
            }
            catch (...)
            {
                DEBUG_LOG_DB("Failed to deserialize expression index");
                return false;
            }
        }

        // Task 17 Phase 9: Deserialize filtered index predicate
        if (index_info.is_partial_index)
        {
            try
            {
                index_predicate = core::ExpressionSerializer::deserialize(
                    index_info.predicate_data.data(),
                    index_info.predicate_data.size(),
                    temp_pool);
            }
            catch (...)
            {
                DEBUG_LOG_DB("Failed to deserialize filtered index predicate");
                // Cleanup expressions
                for (auto *expr : index_expressions)
                {
                    delete expr;
                }
                return false;
            }
        }

        // Task 17 Phase 9: Check if query predicate implies index predicate
        // If this is a filtered index, the query WHERE clause must imply the index WHERE clause
        if (index_info.is_partial_index && index_predicate)
        {
            bool predicate_satisfied = PredicateMatcher::implies(where_clause, index_predicate, &string_pool);

            if (!predicate_satisfied)
            {
                DEBUG_LOG_DB("Query predicate does not imply filtered index predicate - cannot use index");
                // Cleanup
                delete index_predicate;
                for (auto *expr : index_expressions)
                {
                    delete expr;
                }
                return false;
            }

            DEBUG_LOG_DB("Query predicate implies filtered index predicate - can use index");
        }

        // Task 17 Phase 8: Check if any index expression matches or can be used by WHERE clause
        bool applicable = false;

        // If this is a regular filtered index (not expression index), check column match
        if (!index_info.is_expression_index)
        {
            // For non-expression filtered indexes, we still need to check if the
            // indexed columns are used in the query - delegate to existing logic
            // This is handled by the caller's isIndexApplicable() check
            applicable = true; // Predicate already checked above
        }
        else
        {
            // Expression index - check if expressions match query
            for (auto *index_expr : index_expressions)
            {
                // Try exact match or range scan compatibility
                ExpressionMatchType match_type = ExpressionMatcher::canUse(where_clause, index_expr, &string_pool);

                if (match_type == ExpressionMatchType::EXACT_MATCH ||
                    match_type == ExpressionMatchType::RANGE_SCAN)
                {
                    DEBUG_LOG_DB("Expression index can be used: match type = " +
                               std::to_string(static_cast<int>(match_type)));
                    applicable = true;
                    break;
                }

                // For complex WHERE clauses (AND/OR), recursively check sub-expressions
                if (where_clause->kind() == parser::ASTKind::BINARY_OP)
                {
                    auto *bin_op = static_cast<const parser::BinaryOpExpr *>(where_clause);
                    parser::BinaryOp op = bin_op->op();

                    // For AND: check both sides
                    if (op == parser::BinaryOp::AND)
                    {
                        ExpressionMatchType left_match = ExpressionMatcher::canUse(bin_op->left(), index_expr, &string_pool);
                        ExpressionMatchType right_match = ExpressionMatcher::canUse(bin_op->right(), index_expr, &string_pool);

                        if (left_match != ExpressionMatchType::NO_MATCH ||
                            right_match != ExpressionMatchType::NO_MATCH)
                        {
                            DEBUG_LOG_DB("Expression index matches part of AND clause");
                            applicable = true;
                            break;
                        }
                    }
                    // For OR: both sides must be compatible (more restrictive)
                    else if (op == parser::BinaryOp::OR)
                    {
                        ExpressionMatchType left_match = ExpressionMatcher::canUse(bin_op->left(), index_expr, &string_pool);
                        ExpressionMatchType right_match = ExpressionMatcher::canUse(bin_op->right(), index_expr, &string_pool);

                        if (left_match != ExpressionMatchType::NO_MATCH &&
                            right_match != ExpressionMatchType::NO_MATCH)
                        {
                            DEBUG_LOG_DB("Expression index matches both sides of OR clause");
                            applicable = true;
                            break;
                        }
                    }
                }
            }
        }

        // Cleanup deserialized expressions and predicate
        if (index_predicate)
        {
            delete index_predicate;
        }
        for (auto *expr : index_expressions)
        {
            delete expr;
        }

        return applicable;
    }

} // namespace scratchbird::optimizer
