#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/catalog_manager.h"
#include <algorithm>

namespace scratchbird::optimizer
{

    auto QueryPlanner::planQuery(const parser::SelectStmt *select_stmt,
                                  core::ErrorContext *ctx)
        -> std::shared_ptr<PlanNode>
    {
        DEBUG_LOG_DB("Planning query for table: " +
                     std::string(select_stmt->tableName().data()));

        // Phase 1: Get table ID from catalog
        std::string table_name(select_stmt->tableName().data());
        core::ID table_id;
        core::Status status = db_->catalog_manager()->getTableID(table_name, table_id, ctx);
        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("Failed to find table: " + table_name);
            if (ctx)
            {
                ctx->recordError(core::ErrorCode::TABLE_NOT_FOUND,
                                 "Table not found: " + table_name);
            }
            return nullptr;
        }

        DEBUG_LOG_DB("Table ID: " + table_id.toString());

        // Phase 2: Generate all feasible paths
        std::vector<std::shared_ptr<Path>> paths;
        status = generatePaths(select_stmt, table_id, paths, ctx);
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

        // Phase 4: Convert to PlanNode
        auto plan = pathToPlanNode(cheapest, ctx);
        if (!plan)
        {
            DEBUG_LOG_DB("Failed to convert path to plan node");
            return nullptr;
        }

        DEBUG_LOG_DB("Query planning complete: " + plan->toString());
        return plan;
    }

    auto QueryPlanner::planAnalyze(const parser::AnalyzeStmt *analyze_stmt,
                                    core::ErrorContext *ctx)
        -> std::shared_ptr<PlanNode>
    {
        // For ANALYZE, we don't need a complex plan
        // The executor will call StatisticsManager::analyzeTable() directly
        // This is a placeholder - actual ANALYZE execution happens in executor
        DEBUG_LOG_DB("Planning ANALYZE for table: " +
                     std::string(analyze_stmt->tableName().data()));

        // Return nullptr to signal special handling in executor
        // (In a real implementation, we might create an AnalyzeNode)
        return nullptr;
    }

    auto QueryPlanner::generatePaths(const parser::SelectStmt *select_stmt,
                                      const core::ID &table_id,
                                      std::vector<std::shared_ptr<Path>> &paths,
                                      core::ErrorContext *ctx)
        -> core::Status
    {
        std::string table_name(select_stmt->tableName().data());

        // Always generate sequential scan path (always feasible)
        auto seq_scan_path = generateSeqScanPath(select_stmt, table_id, table_name, ctx);
        if (seq_scan_path)
        {
            paths.push_back(seq_scan_path);
            DEBUG_LOG_DB("Generated SeqScanPath: cost=" +
                         std::to_string(seq_scan_path->totalCost()));
        }

        // Generate index scan paths
        core::Status status = generateIndexScanPaths(select_stmt, table_id, table_name, paths, ctx);
        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("Index scan path generation had errors (non-fatal)");
            // Non-fatal - we can still use sequential scan
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
                                               core::ErrorContext *ctx)
        -> core::Status
    {
        DEBUG_LOG_DB("Generating index scan paths for " + table_name);

        // Get all indexes on table
        std::vector<core::ID> index_ids;
        core::Status status = db_->catalog_manager()->getTableIndexes(table_id, index_ids, ctx);
        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("No indexes found for table " + table_name);
            return core::Status::OK;  // No indexes is not an error
        }

        DEBUG_LOG_DB("Found " + std::to_string(index_ids.size()) + " indexes");

        // Check each index for applicability
        for (const auto &index_id : index_ids)
        {
            if (!isIndexApplicable(index_id, select_stmt, ctx))
            {
                DEBUG_LOG_DB("Index " + index_id.toString() + " not applicable");
                continue;
            }

            DEBUG_LOG_DB("Index " + index_id.toString() + " is applicable");

            // Get index metadata
            std::string index_name;
            std::vector<core::ID> index_columns;
            status = db_->catalog_manager()->getIndexMetadata(
                index_id, index_name, index_columns, ctx);
            if (status != core::Status::OK)
            {
                DEBUG_LOG_DB("Failed to get index metadata");
                continue;
            }

            // Get table statistics
            TableStatistics table_stats;
            status = stats_manager_->getTableStatistics(table_id, table_stats, ctx);
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
            if (!index_columns.empty())
            {
                ColumnStatistics col_stats;
                status = stats_manager_->getColumnStatistics(
                    table_id, index_columns[0], col_stats, ctx);
                if (status == core::Status::OK)
                {
                    // Correlation would be stored in col_stats if we had it
                    // For now, use default
                    correlation = 0.0;
                }
            }

            // Calculate qualification cost
            double qual_cost = calculateQualCost(select_stmt);

            // Estimate index scan cost
            CostEstimate cost = cost_model_.costIndexScan(
                index_height,
                index_pages,
                index_tuples,
                heap_pages,
                heap_tuples,
                qual_cost,
                correlation,
                ctx);

            DEBUG_LOG_DB("IndexScan cost for " + index_name +
                         ": startup=" + std::to_string(cost.startup_cost) +
                         ", total=" + std::to_string(cost.total_cost) +
                         ", rows=" + std::to_string(cost.rows));

            // Create IndexScanPath
            auto index_path = std::make_shared<IndexScanPath>(
                table_id,
                table_name,
                index_id,
                index_name,
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

        DEBUG_LOG_DB("Unknown path type");
        return nullptr;
    }

    auto QueryPlanner::estimateSelectivity(const parser::SelectStmt *select_stmt,
                                            const core::ID &table_id,
                                            core::ErrorContext *ctx) const
        -> double
    {
        // Phase 1: Simple heuristics
        // Phase 2 (Task 1.4) will use histogram-based estimation

        if (!select_stmt->whereClause())
        {
            // No WHERE clause → all rows pass
            return 1.0;
        }

        // For now, use conservative default estimate
        // Typical WHERE clause selectivity: ~10-30%
        // We use 0.33 as a reasonable middle ground
        constexpr double DEFAULT_SELECTIVITY = 0.33;

        DEBUG_LOG_DB("Using default selectivity estimate: " +
                     std::to_string(DEFAULT_SELECTIVITY));

        return DEFAULT_SELECTIVITY;
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

} // namespace scratchbird::optimizer
