#ifndef SCRATCHBIRD_ENGINE_QUERY_PLANNER_H
#define SCRATCHBIRD_ENGINE_QUERY_PLANNER_H

#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/executor_nodes.h"
#include "scratchbird/engine/parser_select.h"

#include <memory>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    // Query plan representation for EXPLAIN
    struct QueryPlan {
        struct PlanNode {
            std::string node_type; // "SeqScan", "HashJoin", "Filter", etc.
            std::string details;   // Additional details (table name, predicate, etc.)
            std::vector<std::string> columns;

            // Cost estimates
            double estimated_cost{0.0};
            std::uint64_t estimated_rows{0};

            // Actual metrics (filled during EXPLAIN ANALYZE)
            Instrumentation instrumentation;
            bool has_actuals{false};

            std::vector<std::unique_ptr<PlanNode>> children;

            PlanNode(const std::string& type, const std::string& details = "")
                : node_type(type), details(details)
            {
            }
        };

        std::unique_ptr<PlanNode> root;

        // Generate text representation for EXPLAIN
        std::string to_text(bool show_actuals = false) const;
        std::vector<std::string> to_multiline(bool show_actuals = false) const;
        std::string to_json(bool show_actuals = false) const;
    };

    // Query planner that builds executor node trees
    class QueryPlanner
    {
      public:
        // Build executor node tree from SELECT query
        std::unique_ptr<ExecutorNode> build_executor_plan(const SelectQuery& query);

        // Build plan representation for EXPLAIN
        std::unique_ptr<QueryPlan> build_query_plan(const SelectQuery& query);

        // Execute plan with instrumentation for EXPLAIN ANALYZE
        // ExecutionResult execute_with_instrumentation(std::unique_ptr<ExecutorNode> plan,
        //                                             std::unique_ptr<QueryPlan> explain_plan);

        // Helper methods for plan representation
        void collect_instrumentation(QueryPlan::PlanNode* plan_node, const ExecutorNode* exec_node);

        // Cost estimation (basic implementation) - public for testing
        double estimate_seq_scan_cost(const std::string& table);
        double estimate_hash_join_cost(double left_rows, double right_rows);
        double estimate_filter_cost(double input_rows, double selectivity = 0.1);

      private:
        // Helper methods for building plans
        std::unique_ptr<ExecutorNode> build_scan_node(const FromItem& from_item);
        std::unique_ptr<ExecutorNode> build_filter_node(std::unique_ptr<ExecutorNode> child,
                                                        const std::string& predicate);
        std::unique_ptr<ExecutorNode> build_join_node(std::unique_ptr<ExecutorNode> left,
                                                      std::unique_ptr<ExecutorNode> right,
                                                      const std::string& join_condition);
        std::unique_ptr<ExecutorNode>
        build_project_node(std::unique_ptr<ExecutorNode> child,
                           const std::vector<std::string>& projections);

        // Helper methods for plan representation
        std::unique_ptr<QueryPlan::PlanNode> build_plan_node(const ExecutorNode* exec_node);
    };

    // High-level interface functions
    ExecutionResult explain_select_nodes(const std::string& sql);
    ExecutionResult explain_analyze_select_nodes(const std::string& sql);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_QUERY_PLANNER_H
