#ifndef SCRATCHBIRD_ENGINE_PARSER_SELECT_H
#define SCRATCHBIRD_ENGINE_PARSER_SELECT_H

#include "scratchbird/engine/ast.h"

#include <memory>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    enum class JoinType { Inner, Left, Right, Full, Cross, Natural };

    struct JoinClause {
        JoinType type{JoinType::Inner};
        std::string table;                   // joined table name
        std::string alias;                   // optional alias for joined source
        std::string on_raw;                  // ON expression (raw)
        std::vector<std::string> using_cols; // USING(col, ...)
        bool rhs_is_subquery{false};         // true when JOIN ( ... ) or table function
        std::string rhs_raw;                 // raw body for derived/join-group
    };

    struct CteDef {
        std::string name;
        std::vector<std::string> columns;
        std::string body_raw;
    };

    struct FromItem {
        std::string table;   // possibly qualified
        std::string alias;   // optional
        bool lateral{false}; // LATERAL
        bool is_subquery{false};
        std::string subquery; // raw if subquery
    };

    struct SelectQuery; // fwd

    struct JoinTree {
        // Leaf when both children are null
        FromItem leaf{}; // valid when this node represents a single source
        bool is_leaf{true};
        JoinType type{JoinType::Inner};      // join type for non-leaf
        std::string on_raw;                  // ON condition (raw)
        std::vector<std::string> using_cols; // USING(...) columns
        bool natural{false};                 // NATURAL join present
        bool has_on{false};                  // ON clause present
        bool has_using{false};               // USING clause present
        std::unique_ptr<JoinTree> left;      // left child for non-leaf
        std::unique_ptr<JoinTree> right;     // right child for non-leaf
        std::string alias;                   // alias applied to this node (when derived)
    };

    struct ForUpdateSpec {
        bool enabled{false};
        std::vector<std::string> of_columns;
        bool nowait{false};
        bool skip_locked{false};
    };

    enum class NullsOrder { None, First, Last };

    struct OrderItem {
        std::string expression; // normalized expression text (empty if ordinal used)
        int ordinal{0};         // 1-based positional reference; 0 if not used
        bool ascending{true};   // default ASC
        NullsOrder nulls{NullsOrder::None};
    };

    struct WindowFrame {
        std::string unit;       // ROWS or RANGE
        std::string between;    // raw between clause
        std::string start_kind; // unbounded|current|expr|none
        std::string start_expr; // when start_kind=expr
        std::string end_kind;   // unbounded|current|expr|none
        std::string end_expr;   // when end_kind=expr
        std::string start_dir;  // preceding|following|"" (for expr)
        std::string end_dir;    // preceding|following|"" (for expr)
        SourceSpan span{};
    };

    struct WindowSpec {
        std::string name;
        std::string ref_name;                       // named window reference
        std::string partition_by;                   // raw for now
        std::string order_by;                       // raw for now
        std::vector<std::string> partition_by_list; // structured list
        std::vector<std::string> order_by_list;     // structured list
        WindowFrame frame;                          // minimal parsed frame
        SourceSpan span{};
    };

    struct SetTree {
        std::string op; // empty for leaf
        bool all{false};
        std::unique_ptr<SetTree> left;
        std::unique_ptr<SetTree> right;
        std::unique_ptr<SelectQuery> leaf; // valid when leaf
        // Compound tail
        std::vector<OrderItem> order_by; // ORDER BY at compound level
        int fetch_n{0};                  // FETCH at compound level
    };

    struct PlanOp {
        std::string relation;            // table or alias
        std::string method;              // NATURAL, INDEX, ORDER
        std::string order_index;         // for ORDER <index_name>
        std::vector<std::string> args;   // e.g., index list or extra
        std::string index_type{"BTREE"}; // HASH, BITMAP, CLUSTERED, NON-CLUSTERED, FULLTEXT,
                                         // MULTIKEY, GEOSPATIAL, INVERTED, BTREE
        std::vector<std::string> hints;  // additional arbitrary hints
    };

    struct PlanNode {
        std::string kind;        // SINGLE, JOIN, NESTED, MERGE, SORT, HASH, UNION, SORT MERGE
        std::vector<PlanOp> ops; // leaf operations (relations)
        std::vector<PlanNode> subplans; // nested plan nodes (for JOIN/SORT/MERGE)
    };

    struct SelectQuery {
        // Projection
        std::vector<std::string> projection;  // kept for compatibility
        std::vector<std::string> projections; // used by implementation
        // FROM / JOIN
        std::vector<FromItem> from_items;
        std::vector<JoinClause> joins; // flat joins list (legacy path)
        std::unique_ptr<JoinTree> join_tree_root;
        std::string from_table; // legacy convenience
        std::string from_alias; // legacy convenience
        // WHERE/GROUP/HAVING
        std::string where_raw;
        std::string where_expr; // normalized expression
        std::vector<std::string> group_by;
        std::string having_raw;
        // ORDER/FETCH
        std::vector<OrderItem> order_by;
        int fetch_n{0};
        int skip{0};
        int first{0};
        int rows_from{0};
        int rows_to{0};
        // Modifiers
        bool distinct{false};
        std::vector<std::string> distinct_on;
        bool with_recursive{false};
        // WITH / WINDOW
        std::vector<CteDef> ctes;
        std::vector<WindowSpec> windows;
        std::vector<std::string> window_functions;
        // FOR UPDATE
        ForUpdateSpec for_update;   // structured
        std::string for_update_raw; // raw capture
        // SET operations / PLAN
        std::unique_ptr<SetTree> compound;
        std::unique_ptr<PlanNode> plan;
        std::string plan_raw;
        // Diagnostics
        bool ok{true};
        std::string error;
        std::vector<std::string> warnings;
        std::vector<SourceSpan> warning_spans;
    };

    // Minimal text-only SELECT parsing
    SelectQuery parse_select_minimal(const std::string& sql);

    std::string format_select(const SelectQuery& q);
    std::string format_set_tree(const SetTree* t, int indent = 0);
    std::string format_join_tree(const JoinTree* jt, int indent = 0);

    // New: return a single-line textual plan for EXPLAIN
    std::string explain_select_plan(const SelectQuery& q, bool analyze);

    // Phase 6: Optimizer outputs
    // Optimized textual plan (may differ from simple heuristic explain)
    std::string optimize_select_plan(const SelectQuery& q, bool analyze);
    // Multiline, indented tree plan
    std::vector<std::string> optimize_select_plan_multiline(const SelectQuery& q, bool analyze);

    // For two-relation joins, choose driving order: returns 0 if original order, 1 if swap
    int choose_two_relation_join_order(const SelectQuery& q);

    // Phase 6: allow executor/DDL/ANALYZE to invalidate cached plans
    void invalidate_optimizer_cache();
    void invalidate_optimizer_cache_for_relation(const std::string& relation);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_PARSER_SELECT_H
