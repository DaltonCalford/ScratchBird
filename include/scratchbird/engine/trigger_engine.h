#ifndef SCRATCHBIRD_ENGINE_TRIGGER_ENGINE_H
#define SCRATCHBIRD_ENGINE_TRIGGER_ENGINE_H

#include "scratchbird/engine/heap.h"
#include "scratchbird/engine/system_oids.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    // Enhanced trigger execution context
    struct TriggerContext {
        // Basic context
        std::string schema_name;
        std::string relation_name;
        std::string timing;   // BEFORE|AFTER
        std::string event;    // INSERT|UPDATE|DELETE
        std::string for_each; // ROW|STATEMENT

        // Row-level context (for FOR EACH ROW triggers)
        std::vector<std::string> column_names;
        std::vector<Value> old_row; // OLD.* values (empty for INSERT)
        std::vector<Value> new_row; // NEW.* values (empty for DELETE)

        // Statement-level context (for FOR EACH STATEMENT triggers)
        std::size_t affected_rows{0}; // Number of rows affected
        std::size_t old_count{0};     // Pre-statement row count
        std::size_t new_count{0};     // Post-statement row count

        // Transition tables (advanced feature)
        std::vector<std::vector<Value>> old_table; // OLD TABLE
        std::vector<std::vector<Value>> new_table; // NEW TABLE

        // Transaction context
        std::uint64_t transaction_id{0};
        std::uint64_t statement_id{0};

        // Helper methods
        std::unordered_map<std::string, std::size_t> get_column_index_map() const;
        Value get_old_value(const std::string& column) const;
        Value get_new_value(const std::string& column) const;
        void set_new_value(const std::string& column, const Value& value);
    };

    // Enhanced trigger metadata with WHEN clause support
    struct AdvancedTriggerInfo {
        // Basic trigger metadata
        UuidBytes oid{};
        std::string name;
        std::string timing;   // BEFORE|AFTER|INSTEAD OF
        std::string event;    // INSERT|UPDATE|DELETE|TRUNCATE
        std::string for_each; // ROW|STATEMENT
        int position{0};
        bool active{true};

        // Advanced features
        std::string when_clause;                    // WHEN condition expression
        std::vector<std::string> update_of_columns; // UPDATE OF column list
        std::string referencing_clause;             // REFERENCING OLD/NEW AS names

        // Trigger body and actions
        std::string trigger_body;
        std::vector<std::string> actions; // Parsed action statements

        // Execution flags
        bool has_when_clause{false};
        bool has_old_references{false};
        bool has_new_references{false};
        bool has_transition_tables{false};
        bool is_constraint_trigger{false};
    };

    // WHEN clause expression evaluator
    class TriggerWhenEvaluator
    {
      public:
        TriggerWhenEvaluator();

        // Parse and compile WHEN clause
        bool compile_when_clause(const std::string& when_expr);

        // Evaluate WHEN clause against trigger context
        bool evaluate_when_clause(const TriggerContext& context) const;

        // Check if WHEN clause references OLD/NEW
        bool references_old_values() const
        {
            return has_old_references_;
        }
        bool references_new_values() const
        {
            return has_new_references_;
        }

      private:
        std::string compiled_expression_;
        bool has_old_references_{false};
        bool has_new_references_{false};

        // Helper methods for evaluation
        Value evaluate_column_reference(const std::string& ref,
                                        const TriggerContext& context) const;
        Value evaluate_function_call(const std::string& func, const std::vector<Value>& args) const;
        bool evaluate_comparison(const std::string& op, const Value& left,
                                 const Value& right) const;
    };

    // Enhanced trigger action interpreter
    class TriggerActionInterpreter
    {
      public:
        TriggerActionInterpreter();

        // Parse trigger body into individual actions
        std::vector<std::string> parse_trigger_body(const std::string& body);

        // Execute trigger actions
        bool execute_actions(const std::vector<std::string>& actions, TriggerContext& context);

        // Execute individual action
        bool execute_action(const std::string& action, TriggerContext& context);

      private:
        // Action types
        bool execute_assignment(const std::string& assignment, TriggerContext& context);
        bool execute_raise_statement(const std::string& raise_stmt, TriggerContext& context);
        bool execute_sql_statement(const std::string& sql_stmt, TriggerContext& context);

        // Utility methods
        std::string normalize_action(const std::string& action);
        std::vector<std::string> tokenize_expression(const std::string& expr);

      public:
        // Expression evaluation (public for testing)
        Value evaluate_expression(const std::string& expr, const TriggerContext& context);
    };

    // Enhanced trigger execution engine
    class TriggerEngine
    {
      public:
        TriggerEngine(const std::string& db_path);

        // Main trigger execution entry points
        void execute_statement_triggers(const std::string& schema, const std::string& relation,
                                        const std::string& timing, const std::string& event,
                                        std::size_t old_count = 0, std::size_t new_count = 0,
                                        const std::vector<std::vector<Value>>& old_table = {},
                                        const std::vector<std::vector<Value>>& new_table = {});

        void execute_row_triggers(const std::string& schema, const std::string& relation,
                                  const std::string& timing, const std::string& event,
                                  const std::vector<std::string>& columns,
                                  const std::vector<Value>& old_row = {},
                                  const std::vector<Value>& new_row = {});

        // Trigger management
        std::vector<AdvancedTriggerInfo> get_triggers_for_relation(const std::string& schema,
                                                                   const std::string& relation);

        bool create_advanced_trigger(const std::string& schema, const std::string& relation,
                                     const AdvancedTriggerInfo& trigger_info);

        bool drop_trigger(const std::string& schema, const std::string& trigger_name);

        // Configuration
        void set_trigger_execution_enabled(bool enabled)
        {
            execution_enabled_ = enabled;
        }
        void set_constraint_triggers_immediate(bool immediate)
        {
            constraint_triggers_immediate_ = immediate;
        }

      private:
        std::string db_path_;
        bool execution_enabled_{true};
        bool constraint_triggers_immediate_{false};

        TriggerWhenEvaluator when_evaluator_;
        TriggerActionInterpreter action_interpreter_;

        // Internal execution methods
        void execute_single_trigger(const AdvancedTriggerInfo& trigger, TriggerContext& context);
        bool should_execute_trigger(const AdvancedTriggerInfo& trigger,
                                    const TriggerContext& context);

        // Catalog integration
        std::vector<AdvancedTriggerInfo> load_triggers_from_catalog(const std::string& schema,
                                                                    const std::string& relation);
        void store_trigger_to_catalog(const AdvancedTriggerInfo& trigger);

        // Context building
        TriggerContext build_statement_context(const std::string& schema,
                                               const std::string& relation,
                                               const std::string& timing, const std::string& event,
                                               std::size_t old_count, std::size_t new_count,
                                               const std::vector<std::vector<Value>>& old_table,
                                               const std::vector<std::vector<Value>>& new_table);

        TriggerContext build_row_context(const std::string& schema, const std::string& relation,
                                         const std::string& timing, const std::string& event,
                                         const std::vector<std::string>& columns,
                                         const std::vector<Value>& old_row,
                                         const std::vector<Value>& new_row);
    };

    // High-level trigger utility functions

    // Enhanced WHEN clause functions
    bool compile_trigger_when_clause(const std::string& when_expr);
    bool evaluate_trigger_when_clause(const std::string& when_expr, const TriggerContext& context);

    // Trigger body parsing and validation
    std::vector<std::string> parse_trigger_actions(const std::string& body);
    bool validate_trigger_body(const std::string& body, const std::string& timing,
                               const std::string& for_each);

    // Advanced trigger expression support
    Value evaluate_trigger_expression(const std::string& expr, const TriggerContext& context);
    std::string format_trigger_error(const std::string& trigger_name, const std::string& error);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_TRIGGER_ENGINE_H
