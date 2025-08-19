#include "scratchbird/engine/query_planner.h"

#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace scratchbird::engine
{

    // ========== QueryPlan Implementation ==========

    std::string QueryPlan::to_text(bool show_actuals) const
    {
        if (!root) {
            return "Empty Plan";
        }

        std::ostringstream ss;

        std::function<void(const PlanNode*, int, bool)> print_node =
            [&](const PlanNode* node, int depth, bool /* is_last */) {
                // Indentation
                for (int i = 0; i < depth; ++i) {
                    ss << "  ";
                }

                // Node type and details
                ss << node->node_type;
                if (!node->details.empty()) {
                    ss << " " << node->details;
                }

                // Cost estimates
                ss << " (cost=" << std::fixed << std::setprecision(2) << node->estimated_cost;
                ss << " rows=" << node->estimated_rows << ")";

                // Actual metrics if available
                if (show_actuals && node->has_actuals) {
                    const auto& instr = node->instrumentation;
                    ss << " [actual: rows=" << instr.output_rows;
                    ss << " time=" << instr.wall_time_ms << "ms";
                    if (instr.filtered_rows > 0) {
                        ss << " filtered=" << instr.filtered_rows;
                    }
                    if (instr.memory_bytes_peak > 0) {
                        ss << " memory=" << (instr.memory_bytes_peak / 1024) << "KB";
                    }
                    ss << "]";
                }

                ss << "\n";

                // Print children
                for (size_t i = 0; i < node->children.size(); ++i) {
                    print_node(node->children[i].get(), depth + 1, i == node->children.size() - 1);
                }
            };

        print_node(root.get(), 0, true);
        return ss.str();
    }

    std::vector<std::string> QueryPlan::to_multiline(bool show_actuals) const
    {
        std::vector<std::string> lines;
        std::string text = to_text(show_actuals);

        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }

        return lines;
    }

    std::string QueryPlan::to_json(bool show_actuals) const
    {
        if (!root) {
            return "{\"plan\": null}";
        }

        std::ostringstream ss;

        std::function<void(const PlanNode*)> print_node = [&](const PlanNode* node) {
            ss << "{";
            ss << "\"node_type\":\"" << node->node_type << "\",";
            ss << "\"details\":\"" << node->details << "\",";
            ss << "\"estimated_cost\":" << node->estimated_cost << ",";
            ss << "\"estimated_rows\":" << node->estimated_rows;

            if (show_actuals && node->has_actuals) {
                const auto& instr = node->instrumentation;
                ss << ",\"actual_rows\":" << instr.output_rows;
                ss << ",\"actual_time_ms\":" << instr.wall_time_ms;
                ss << ",\"filtered_rows\":" << instr.filtered_rows;
                ss << ",\"memory_bytes_peak\":" << instr.memory_bytes_peak;
            }

            if (!node->children.empty()) {
                ss << ",\"children\":[";
                for (size_t i = 0; i < node->children.size(); ++i) {
                    if (i > 0)
                        ss << ",";
                    print_node(node->children[i].get());
                }
                ss << "]";
            }

            ss << "}";
        };

        ss << "{\"plan\":";
        print_node(root.get());
        ss << "}";

        return ss.str();
    }

    // ========== QueryPlanner Implementation ==========

    std::unique_ptr<ExecutorNode> QueryPlanner::build_executor_plan(const SelectQuery& query)
    {
        // Start with base table scans
        std::vector<std::unique_ptr<ExecutorNode>> scan_nodes;

        for (const auto& from_item : query.from_items) {
            auto scan = build_scan_node(from_item);
            scan_nodes.push_back(std::move(scan));
        }

        if (scan_nodes.empty()) {
            throw std::runtime_error("No tables specified in FROM clause");
        }

        // Build join tree if multiple tables
        std::unique_ptr<ExecutorNode> result = std::move(scan_nodes[0]);
        for (size_t i = 1; i < scan_nodes.size(); ++i) {
            // Simple nested loop join for now
            // TODO: Choose optimal join algorithm based on cost estimates
            result = build_join_node(std::move(result), std::move(scan_nodes[i]), "");
        }

        // Apply WHERE clause as filter
        if (!query.where_expr.empty()) {
            result = build_filter_node(std::move(result), query.where_expr);
        }

        // Apply projections
        if (!query.projections.empty()) {
            result = build_project_node(std::move(result), query.projections);
        }

        // TODO: Add ORDER BY, GROUP BY, HAVING, LIMIT nodes

        return result;
    }

    std::unique_ptr<QueryPlan> QueryPlanner::build_query_plan(const SelectQuery& query)
    {
        auto plan = std::make_unique<QueryPlan>();

        // Build a corresponding plan tree for EXPLAIN
        // This mirrors the executor plan structure but focuses on planning details

        // For now, create a simple plan representation
        // TODO: Make this more sophisticated with proper cost modeling

        if (query.from_items.size() == 1) {
            // Single table query
            const auto& from_item = query.from_items[0];
            auto node = std::make_unique<QueryPlan::PlanNode>(
                "SeqScan",
                "on " + from_item.table + (!from_item.alias.empty() ? " " + from_item.alias : ""));

            node->estimated_cost = estimate_seq_scan_cost(from_item.table);
            node->estimated_rows = 1000; // Default estimate
            node->columns = {"*"};       // TODO: Get actual columns

            if (!query.where_expr.empty()) {
                auto filter_node = std::make_unique<QueryPlan::PlanNode>(
                    "Filter", "condition: " + query.where_expr);
                filter_node->estimated_cost = estimate_filter_cost(node->estimated_rows);
                filter_node->estimated_rows =
                    node->estimated_rows * 0.1; // 10% selectivity estimate
                filter_node->children.push_back(std::move(node));
                node = std::move(filter_node);
            }

            if (!query.projections.empty() &&
                !(query.projections.size() == 1 && query.projections[0] == "*")) {
                auto project_node = std::make_unique<QueryPlan::PlanNode>(
                    "Project", "columns: " + [&]() {
                        std::string cols;
                        for (size_t i = 0; i < query.projections.size(); ++i) {
                            if (i > 0)
                                cols += ", ";
                            cols += query.projections[i];
                        }
                        return cols;
                    }());
                project_node->estimated_cost = node->estimated_cost + 10.0;
                project_node->estimated_rows = node->estimated_rows;
                project_node->children.push_back(std::move(node));
                node = std::move(project_node);
            }

            plan->root = std::move(node);
        } else {
            // Multi-table join
            auto join_node = std::make_unique<QueryPlan::PlanNode>("HashJoin", "inner join");
            join_node->estimated_cost = estimate_hash_join_cost(1000, 1000);
            join_node->estimated_rows = 500; // Join selectivity estimate

            // Add child scans
            for (const auto& from_item : query.from_items) {
                auto scan_node = std::make_unique<QueryPlan::PlanNode>(
                    "SeqScan", "on " + from_item.table +
                                   (!from_item.alias.empty() ? " " + from_item.alias : ""));
                scan_node->estimated_cost = estimate_seq_scan_cost(from_item.table);
                scan_node->estimated_rows = 1000;
                join_node->children.push_back(std::move(scan_node));
            }

            plan->root = std::move(join_node);
        }

        return plan;
    }

    // Removed execute_with_instrumentation - implemented inline in explain_analyze_select_nodes

    // ========== Helper Methods ==========

    std::unique_ptr<ExecutorNode> QueryPlanner::build_scan_node(const FromItem& from_item)
    {
        // Default to "public" schema if not specified
        std::string schema = "public";
        return std::make_unique<SeqScanNode>(
            schema, from_item.table, from_item.alias.empty() ? from_item.table : from_item.alias);
    }

    std::unique_ptr<ExecutorNode>
    QueryPlanner::build_filter_node(std::unique_ptr<ExecutorNode> child,
                                    const std::string& predicate)
    {
        return std::make_unique<FilterNode>(std::move(child), predicate);
    }

    std::unique_ptr<ExecutorNode> QueryPlanner::build_join_node(std::unique_ptr<ExecutorNode> left,
                                                                std::unique_ptr<ExecutorNode> right,
                                                                const std::string& join_condition)
    {
        // For now, use nested loop join with the provided condition
        // TODO: Implement proper join condition parsing and key extraction for hash joins
        return std::make_unique<NestedLoopJoinNode>(std::move(left), std::move(right),
                                                    join_condition);
    }

    std::unique_ptr<ExecutorNode>
    QueryPlanner::build_project_node(std::unique_ptr<ExecutorNode> child,
                                     const std::vector<std::string>& projections)
    {
        return std::make_unique<ProjectNode>(std::move(child), projections);
    }

    void QueryPlanner::collect_instrumentation(QueryPlan::PlanNode* plan_node,
                                               const ExecutorNode* exec_node)
    {
        if (!plan_node || !exec_node) {
            return;
        }

        // Copy instrumentation from executor node to plan node
        plan_node->instrumentation = exec_node->get_instrumentation();
        plan_node->has_actuals = true;

        // TODO: Recursively collect from children when we have proper tree traversal
    }

    double QueryPlanner::estimate_seq_scan_cost(const std::string& /* table */)
    {
        // Simple cost estimation: assume 1000 rows, 1.0 cost per row
        return 1000.0;
    }

    double QueryPlanner::estimate_hash_join_cost(double left_rows, double right_rows)
    {
        // Hash join cost: build hash table (right_rows) + probe (left_rows)
        return right_rows * 1.5 + left_rows * 1.0;
    }

    double QueryPlanner::estimate_filter_cost(double input_rows, double /* selectivity */)
    {
        // Filter cost: evaluate predicate for each input row
        return input_rows * 0.1;
    }

    // ========== High-level Interface Functions ==========

    ExecutionResult explain_select_nodes(const std::string& sql)
    {
        try {
            // Parse the SELECT statement
            SelectQuery query = parse_select_minimal(sql);

            // Build query plan
            QueryPlanner planner;
            auto plan = planner.build_query_plan(query);

            // Generate EXPLAIN output
            ExecutionResult result;
            result.columns = {"Plan"};

            auto lines = plan->to_multiline(false);
            for (const auto& line : lines) {
                result.rows.push_back({line});
            }

            return result;
        } catch (const std::exception& e) {
            ExecutionResult result;
            result.columns = {"error"};
            result.rows = {{std::string("EXPLAIN failed: ") + e.what()}};
            return result;
        }
    }

    ExecutionResult explain_analyze_select_nodes(const std::string& sql)
    {
        try {
            // Parse the SELECT statement
            SelectQuery query = parse_select_minimal(sql);

            // Build executor plan and query plan
            QueryPlanner planner;
            auto exec_plan = planner.build_executor_plan(query);
            auto explain_plan = planner.build_query_plan(query);

            // Execute with instrumentation inline
            ExecutorContext ctx;
            ctx.db_path = get_executor_db_path();

            exec_plan->open(ctx);

            ExecutionResult query_result;
            query_result.columns = exec_plan->columns();

            Tuple tuple;
            while (exec_plan->next(tuple)) {
                std::vector<std::string> row;
                for (const auto& val : tuple) {
                    row.push_back(val.bytes);
                }
                query_result.rows.push_back(row);
            }

            exec_plan->close();

            // Collect instrumentation
            if (explain_plan && explain_plan->root) {
                planner.collect_instrumentation(explain_plan->root.get(), exec_plan.get());
            }

            // Generate EXPLAIN ANALYZE output
            ExecutionResult result;
            result.columns = {"Plan"};

            auto lines = explain_plan->to_multiline(true);
            for (const auto& line : lines) {
                result.rows.push_back({line});
            }

            // Add summary line
            std::vector<std::string> summary_row = {
                "Query returned " + std::to_string(query_result.rows.size()) + " rows"};
            result.rows.push_back(summary_row);

            return result;
        } catch (const std::exception& e) {
            ExecutionResult result;
            result.columns = {"error"};
            result.rows = {{std::string("EXPLAIN ANALYZE failed: ") + e.what()}};
            return result;
        }
    }

} // namespace scratchbird::engine
