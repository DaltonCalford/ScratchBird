#include "scratchbird/engine/trigger_engine.h"

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/expr.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace scratchbird::engine
{

    // ========== TriggerContext Implementation ==========

    std::unordered_map<std::string, std::size_t> TriggerContext::get_column_index_map() const
    {
        std::unordered_map<std::string, std::size_t> index_map;
        for (std::size_t i = 0; i < column_names.size(); ++i) {
            index_map[column_names[i]] = i;
        }
        return index_map;
    }

    Value TriggerContext::get_old_value(const std::string& column) const
    {
        auto index_map = get_column_index_map();
        auto it = index_map.find(column);
        if (it != index_map.end() && it->second < old_row.size()) {
            return old_row[it->second];
        }
        Value null_val;
        null_val.is_null = true;
        return null_val;
    }

    Value TriggerContext::get_new_value(const std::string& column) const
    {
        auto index_map = get_column_index_map();
        auto it = index_map.find(column);
        if (it != index_map.end() && it->second < new_row.size()) {
            return new_row[it->second];
        }
        Value null_val;
        null_val.is_null = true;
        return null_val;
    }

    void TriggerContext::set_new_value(const std::string& column, const Value& value)
    {
        auto index_map = get_column_index_map();
        auto it = index_map.find(column);
        if (it != index_map.end() && it->second < new_row.size()) {
            new_row[it->second] = value;
        }
    }

    // ========== TriggerWhenEvaluator Implementation ==========

    TriggerWhenEvaluator::TriggerWhenEvaluator() {}

    bool TriggerWhenEvaluator::compile_when_clause(const std::string& when_expr)
    {
        compiled_expression_ = when_expr;
        has_old_references_ = false;
        has_new_references_ = false;

        // Simple analysis for OLD/NEW references
        std::string lower_expr = when_expr;
        std::transform(lower_expr.begin(), lower_expr.end(), lower_expr.begin(), ::tolower);

        if (lower_expr.find("old.") != std::string::npos) {
            has_old_references_ = true;
        }
        if (lower_expr.find("new.") != std::string::npos) {
            has_new_references_ = true;
        }

        return true;
    }

    bool TriggerWhenEvaluator::evaluate_when_clause(const TriggerContext& context) const
    {
        if (compiled_expression_.empty()) {
            return true; // No WHEN clause means always execute
        }

        try {
            // Enhanced WHEN clause evaluation
            std::string expr = compiled_expression_;

            // Simple string replacement for OLD.column and NEW.column references
            // For a production system, a proper expression parser would be better

            // Replace OLD.column references
            std::size_t old_pos = 0;
            while ((old_pos = expr.find("OLD.", old_pos)) != std::string::npos) {
                std::size_t start_pos = old_pos + 4;
                std::size_t end_pos = start_pos;

                // Find end of column name (alphanumeric + underscore)
                while (end_pos < expr.length() &&
                       (std::isalnum(expr[end_pos]) || expr[end_pos] == '_')) {
                    end_pos++;
                }

                if (end_pos > start_pos) {
                    std::string column = expr.substr(start_pos, end_pos - start_pos);
                    Value val = context.get_old_value(column);
                    std::string replacement = val.is_null ? "NULL" : ("'" + val.bytes + "'");
                    expr.replace(old_pos, end_pos - old_pos, replacement);
                    old_pos += replacement.length();
                } else {
                    old_pos += 4;
                }
            }

            // Replace NEW.column references
            std::size_t new_pos = 0;
            while ((new_pos = expr.find("NEW.", new_pos)) != std::string::npos) {
                std::size_t start_pos = new_pos + 4;
                std::size_t end_pos = start_pos;

                // Find end of column name (alphanumeric + underscore)
                while (end_pos < expr.length() &&
                       (std::isalnum(expr[end_pos]) || expr[end_pos] == '_')) {
                    end_pos++;
                }

                if (end_pos > start_pos) {
                    std::string column = expr.substr(start_pos, end_pos - start_pos);
                    Value val = context.get_new_value(column);
                    std::string replacement = val.is_null ? "NULL" : ("'" + val.bytes + "'");
                    expr.replace(new_pos, end_pos - new_pos, replacement);
                    new_pos += replacement.length();
                } else {
                    new_pos += 4;
                }
            }

            // Handle special variables for statement-level triggers
            // Replace affected_rows
            std::size_t affected_pos = expr.find("affected_rows");
            if (affected_pos != std::string::npos) {
                expr.replace(affected_pos, 13, std::to_string(context.affected_rows));
            }

            // Replace old_count
            std::size_t old_count_pos = expr.find("old_count");
            if (old_count_pos != std::string::npos) {
                expr.replace(old_count_pos, 9, std::to_string(context.old_count));
            }

            // Replace new_count
            std::size_t new_count_pos = expr.find("new_count");
            if (new_count_pos != std::string::npos) {
                expr.replace(new_count_pos, 9, std::to_string(context.new_count));
            }

            // Use existing predicate evaluator for final evaluation
            std::unordered_map<std::string, std::size_t> dummy_columns;
            std::vector<Value> dummy_row;

            return evaluate_predicate(expr, dummy_columns, dummy_row);

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[TRIGGER] WHEN clause evaluation error: %s\n", e.what());
            return false; // Conservative: don't execute if WHEN clause fails
        }
    }

    Value TriggerWhenEvaluator::evaluate_column_reference(const std::string& ref,
                                                          const TriggerContext& context) const
    {
        if (ref.find("OLD.") == 0) {
            std::string column = ref.substr(4);
            return context.get_old_value(column);
        } else if (ref.find("NEW.") == 0) {
            std::string column = ref.substr(4);
            return context.get_new_value(column);
        }

        Value null_val;
        null_val.is_null = true;
        return null_val;
    }

    bool TriggerWhenEvaluator::evaluate_comparison(const std::string& op, const Value& left,
                                                   const Value& right) const
    {
        if (left.is_null || right.is_null) {
            if (op == "IS NULL" || op == "IS NOT NULL") {
                return (op == "IS NULL") ? left.is_null : !left.is_null;
            }
            return false; // NULL comparisons are false except for IS NULL
        }

        if (op == "=") {
            return left.bytes == right.bytes;
        } else if (op == "!=" || op == "<>") {
            return left.bytes != right.bytes;
        } else if (op == "<") {
            return left.bytes < right.bytes;
        } else if (op == "<=") {
            return left.bytes <= right.bytes;
        } else if (op == ">") {
            return left.bytes > right.bytes;
        } else if (op == ">=") {
            return left.bytes >= right.bytes;
        }

        return false;
    }

    // ========== TriggerActionInterpreter Implementation ==========

    TriggerActionInterpreter::TriggerActionInterpreter() {}

    std::vector<std::string> TriggerActionInterpreter::parse_trigger_body(const std::string& body)
    {
        std::vector<std::string> actions;

        std::istringstream iss(body);
        std::string line;
        std::string current_action;

        while (std::getline(iss, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty() || line[0] == '-' || line.find("--") == 0) {
                continue; // Skip empty lines and comments
            }

            // Handle multi-line statements
            if (!current_action.empty()) {
                current_action += " " + line;
            } else {
                current_action = line;
            }

            // Check if statement is complete (ends with semicolon)
            if (!current_action.empty() && current_action.back() == ';') {
                // Remove semicolon and add to actions
                current_action.pop_back();
                if (!current_action.empty()) {
                    actions.push_back(normalize_action(current_action));
                }
                current_action.clear();
            }
        }

        // Add any remaining action
        if (!current_action.empty()) {
            actions.push_back(normalize_action(current_action));
        }

        return actions;
    }

    bool TriggerActionInterpreter::execute_actions(const std::vector<std::string>& actions,
                                                   TriggerContext& context)
    {
        for (const auto& action : actions) {
            if (!execute_action(action, context)) {
                return false;
            }
        }
        return true;
    }

    bool TriggerActionInterpreter::execute_action(const std::string& action,
                                                  TriggerContext& context)
    {
        std::string lower_action = action;
        std::transform(lower_action.begin(), lower_action.end(), lower_action.begin(), ::tolower);

        try {
            if (lower_action.find("new.") == 0 && lower_action.find("=") != std::string::npos) {
                // Assignment to NEW column: NEW.column = expression
                return execute_assignment(action, context);
            } else if (lower_action.find("raise") == 0) {
                // RAISE statement
                return execute_raise_statement(action, context);
            } else {
                // General SQL statement (not yet implemented)
                return execute_sql_statement(action, context);
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[TRIGGER] Action execution error: %s\n", e.what());
            return false;
        }
    }

    bool TriggerActionInterpreter::execute_assignment(const std::string& assignment,
                                                      TriggerContext& context)
    {
        // Parse: NEW.column = expression
        auto eq_pos = assignment.find('=');
        if (eq_pos == std::string::npos) {
            return false;
        }

        std::string left = assignment.substr(0, eq_pos);
        std::string right = assignment.substr(eq_pos + 1);

        // Trim whitespace
        left.erase(0, left.find_first_not_of(" \t"));
        left.erase(left.find_last_not_of(" \t") + 1);
        right.erase(0, right.find_first_not_of(" \t"));
        right.erase(right.find_last_not_of(" \t") + 1);

        // Extract column name from NEW.column
        if (left.find("NEW.") != 0 && left.find("new.") != 0) {
            std::fprintf(stderr, "[TRIGGER] Invalid assignment target: %s\n", left.c_str());
            return false;
        }

        std::string column = left.substr(4); // Remove "NEW." or "new."

        // Evaluate right-hand side expression
        Value new_value = evaluate_expression(right, context);

        // Set the new value
        context.set_new_value(column, new_value);

        std::fprintf(stderr, "[TRIGGER] Set NEW.%s = '%s'\n", column.c_str(),
                     new_value.is_null ? "NULL" : new_value.bytes.c_str());

        return true;
    }

    bool TriggerActionInterpreter::execute_raise_statement(const std::string& raise_stmt,
                                                           TriggerContext& /* context */)
    {
        // Parse RAISE statements: RAISE 'message' or RAISE SQLSTATE 'code' 'message'
        std::string stmt = raise_stmt;
        std::transform(stmt.begin(), stmt.end(), stmt.begin(), ::tolower);

        std::string message = "Trigger raised error";
        std::string sqlstate = "P0001"; // Default SQLSTATE

        // Look for quoted message
        auto quote1 = raise_stmt.find('\'');
        if (quote1 != std::string::npos) {
            auto quote2 = raise_stmt.find('\'', quote1 + 1);
            if (quote2 != std::string::npos) {
                // Check if this is SQLSTATE format
                if (stmt.find("sqlstate") != std::string::npos && quote1 > stmt.find("sqlstate")) {
                    sqlstate = raise_stmt.substr(quote1 + 1, quote2 - quote1 - 1);

                    // Look for message after SQLSTATE
                    auto quote3 = raise_stmt.find('\'', quote2 + 1);
                    if (quote3 != std::string::npos) {
                        auto quote4 = raise_stmt.find('\'', quote3 + 1);
                        if (quote4 != std::string::npos) {
                            message = raise_stmt.substr(quote3 + 1, quote4 - quote3 - 1);
                        }
                    }
                } else {
                    message = raise_stmt.substr(quote1 + 1, quote2 - quote1 - 1);
                }
            }
        }

        std::fprintf(stderr, "[TRIGGER] RAISE: %s (SQLSTATE: %s)\n", message.c_str(),
                     sqlstate.c_str());
        throw std::runtime_error("Trigger raised: " + message);
    }

    bool TriggerActionInterpreter::execute_sql_statement(const std::string& sql_stmt,
                                                         TriggerContext& /* context */)
    {
        // For now, we don't support arbitrary SQL in triggers
        // This would require integrating with the main SQL executor
        std::fprintf(stderr, "[TRIGGER] SQL statements in triggers not yet supported: %s\n",
                     sql_stmt.c_str());
        return true; // Don't fail for unsupported features
    }

    Value TriggerActionInterpreter::evaluate_expression(const std::string& expr,
                                                        const TriggerContext& context)
    {
        std::string expression = expr;

        // Handle literal values
        if (expression.front() == '\'' && expression.back() == '\'') {
            // String literal
            Value val;
            val.bytes = expression.substr(1, expression.length() - 2);
            val.is_null = false;
            return val;
        }

        if (std::isdigit(expression[0]) ||
            (expression[0] == '-' && expression.length() > 1 && std::isdigit(expression[1]))) {
            // Numeric literal
            Value val;
            val.bytes = expression;
            val.is_null = false;
            return val;
        }

        // Handle OLD.column and NEW.column references
        std::string lower_expr = expression;
        std::transform(lower_expr.begin(), lower_expr.end(), lower_expr.begin(), ::tolower);

        if (lower_expr.find("old.") == 0) {
            std::string column = expression.substr(4);
            return context.get_old_value(column);
        } else if (lower_expr.find("new.") == 0) {
            std::string column = expression.substr(4);
            return context.get_new_value(column);
        }

        // For more complex expressions, we'd need a full expression parser
        // For now, return the expression as a string literal
        Value val;
        val.bytes = expression;
        val.is_null = false;
        return val;
    }

    std::string TriggerActionInterpreter::normalize_action(const std::string& action)
    {
        std::string normalized = action;

        // Trim whitespace
        normalized.erase(0, normalized.find_first_not_of(" \t\r\n"));
        normalized.erase(normalized.find_last_not_of(" \t\r\n") + 1);

        // Remove extra whitespace
        std::regex whitespace_regex(R"(\s+)");
        normalized = std::regex_replace(normalized, whitespace_regex, " ");

        return normalized;
    }

    // ========== TriggerEngine Implementation ==========

    TriggerEngine::TriggerEngine(const std::string& db_path) : db_path_(db_path) {}

    void TriggerEngine::execute_statement_triggers(const std::string& schema,
                                                   const std::string& relation,
                                                   const std::string& timing,
                                                   const std::string& event, std::size_t old_count,
                                                   std::size_t new_count,
                                                   const std::vector<std::vector<Value>>& old_table,
                                                   const std::vector<std::vector<Value>>& new_table)
    {
        if (!execution_enabled_) {
            return;
        }

        auto triggers = get_triggers_for_relation(schema, relation);

        // Filter for statement-level triggers
        std::vector<AdvancedTriggerInfo> statement_triggers;
        for (const auto& trigger : triggers) {
            if (trigger.for_each == "STATEMENT" && trigger.timing == timing &&
                trigger.event == event && trigger.active) {
                statement_triggers.push_back(trigger);
            }
        }

        // Sort by position
        std::sort(statement_triggers.begin(), statement_triggers.end(),
                  [](const AdvancedTriggerInfo& a, const AdvancedTriggerInfo& b) {
                      return a.position < b.position;
                  });

        // Build context
        TriggerContext context = build_statement_context(schema, relation, timing, event, old_count,
                                                         new_count, old_table, new_table);

        // Execute triggers
        for (const auto& trigger : statement_triggers) {
            try {
                if (should_execute_trigger(trigger, context)) {
                    execute_single_trigger(trigger, context);
                }
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[TRIGGER] Statement trigger '%s' error: %s\n",
                             trigger.name.c_str(), e.what());
                throw; // Re-throw to abort the statement
            }
        }
    }

    void TriggerEngine::execute_row_triggers(const std::string& schema, const std::string& relation,
                                             const std::string& timing, const std::string& event,
                                             const std::vector<std::string>& columns,
                                             const std::vector<Value>& old_row,
                                             const std::vector<Value>& new_row)
    {
        if (!execution_enabled_) {
            return;
        }

        auto triggers = get_triggers_for_relation(schema, relation);

        // Filter for row-level triggers
        std::vector<AdvancedTriggerInfo> row_triggers;
        for (const auto& trigger : triggers) {
            if (trigger.for_each == "ROW" && trigger.timing == timing && trigger.event == event &&
                trigger.active) {
                row_triggers.push_back(trigger);
            }
        }

        // Sort by position
        std::sort(row_triggers.begin(), row_triggers.end(),
                  [](const AdvancedTriggerInfo& a, const AdvancedTriggerInfo& b) {
                      return a.position < b.position;
                  });

        // Build context
        TriggerContext context =
            build_row_context(schema, relation, timing, event, columns, old_row, new_row);

        // Execute triggers
        for (const auto& trigger : row_triggers) {
            try {
                if (should_execute_trigger(trigger, context)) {
                    execute_single_trigger(trigger, context);
                }
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[TRIGGER] Row trigger '%s' error: %s\n", trigger.name.c_str(),
                             e.what());
                throw; // Re-throw to abort the statement
            }
        }
    }

    std::vector<AdvancedTriggerInfo>
    TriggerEngine::get_triggers_for_relation(const std::string& schema, const std::string& relation)
    {
        return load_triggers_from_catalog(schema, relation);
    }

    void TriggerEngine::execute_single_trigger(const AdvancedTriggerInfo& trigger,
                                               TriggerContext& context)
    {
        std::fprintf(stderr, "[TRIGGER] Executing %s trigger '%s' on %s.%s\n",
                     trigger.timing.c_str(), trigger.name.c_str(), context.schema_name.c_str(),
                     context.relation_name.c_str());

        // Parse and execute trigger actions
        auto actions = action_interpreter_.parse_trigger_body(trigger.trigger_body);
        if (!action_interpreter_.execute_actions(actions, context)) {
            throw std::runtime_error("Trigger execution failed: " + trigger.name);
        }
    }

    bool TriggerEngine::should_execute_trigger(const AdvancedTriggerInfo& trigger,
                                               const TriggerContext& context)
    {
        if (!trigger.active) {
            return false;
        }

        // Check WHEN clause if present
        if (trigger.has_when_clause && !trigger.when_clause.empty()) {
            TriggerWhenEvaluator when_eval;
            when_eval.compile_when_clause(trigger.when_clause);
            if (!when_eval.evaluate_when_clause(context)) {
                return false;
            }
        }

        // Check UPDATE OF columns if applicable
        if (context.event == "UPDATE" && !trigger.update_of_columns.empty()) {
            // For simplicity, assume any UPDATE matches UPDATE OF for now
            // A full implementation would check which columns were actually updated
        }

        return true;
    }

    std::vector<AdvancedTriggerInfo>
    TriggerEngine::load_triggers_from_catalog(const std::string& schema,
                                              const std::string& relation)
    {
        std::vector<AdvancedTriggerInfo> triggers;

        try {
            CatalogManager cm(db_path_);
            auto soid = cm.lookup_schema_oid_by_name(schema);
            if (!soid) {
                return triggers;
            }

            auto basic_triggers = cm.list_relation_triggers_by_name(soid, relation);

            for (const auto& basic_trigger : basic_triggers) {
                AdvancedTriggerInfo advanced_trigger;

                // Copy basic info
                advanced_trigger.oid = basic_trigger.oid;
                advanced_trigger.name = basic_trigger.name;
                advanced_trigger.timing = basic_trigger.timing;
                advanced_trigger.event =
                    basic_trigger.events; // Note: may need parsing for multiple events
                advanced_trigger.for_each = basic_trigger.for_each;
                advanced_trigger.position = basic_trigger.position;
                advanced_trigger.active = basic_trigger.active;
                advanced_trigger.update_of_columns = basic_trigger.update_of_cols;

                // Get trigger body from catalog
                auto body = cm.get_source_for_object(basic_trigger.oid);
                advanced_trigger.trigger_body = body;

                // Parse WHEN clause from body (basic implementation)
                std::string lower_body = body;
                std::transform(lower_body.begin(), lower_body.end(), lower_body.begin(), ::tolower);

                auto when_pos = lower_body.find("when ");
                if (when_pos != std::string::npos) {
                    auto line_end = body.find('\n', when_pos);
                    if (line_end == std::string::npos)
                        line_end = body.length();

                    advanced_trigger.when_clause =
                        body.substr(when_pos + 5, line_end - when_pos - 5);
                    advanced_trigger.has_when_clause = true;

                    // Trim whitespace
                    advanced_trigger.when_clause.erase(
                        0, advanced_trigger.when_clause.find_first_not_of(" \t"));
                    advanced_trigger.when_clause.erase(
                        advanced_trigger.when_clause.find_last_not_of(" \t") + 1);
                }

                // Analyze for OLD/NEW references
                if (lower_body.find("old.") != std::string::npos) {
                    advanced_trigger.has_old_references = true;
                }
                if (lower_body.find("new.") != std::string::npos) {
                    advanced_trigger.has_new_references = true;
                }

                triggers.push_back(advanced_trigger);
            }

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[TRIGGER] Error loading triggers for %s.%s: %s\n", schema.c_str(),
                         relation.c_str(), e.what());
        }

        return triggers;
    }

    TriggerContext
    TriggerEngine::build_statement_context(const std::string& schema, const std::string& relation,
                                           const std::string& timing, const std::string& event,
                                           std::size_t old_count, std::size_t new_count,
                                           const std::vector<std::vector<Value>>& old_table,
                                           const std::vector<std::vector<Value>>& new_table)
    {
        TriggerContext context;
        context.schema_name = schema;
        context.relation_name = relation;
        context.timing = timing;
        context.event = event;
        context.for_each = "STATEMENT";
        context.old_count = old_count;
        context.new_count = new_count;
        context.affected_rows =
            (new_count > old_count) ? (new_count - old_count) : (old_count - new_count);
        context.old_table = old_table;
        context.new_table = new_table;

        return context;
    }

    TriggerContext TriggerEngine::build_row_context(
        const std::string& schema, const std::string& relation, const std::string& timing,
        const std::string& event, const std::vector<std::string>& columns,
        const std::vector<Value>& old_row, const std::vector<Value>& new_row)
    {
        TriggerContext context;
        context.schema_name = schema;
        context.relation_name = relation;
        context.timing = timing;
        context.event = event;
        context.for_each = "ROW";
        context.column_names = columns;
        context.old_row = old_row;
        context.new_row = new_row;
        context.affected_rows = 1;

        return context;
    }

    // ========== High-level Utility Functions ==========

    bool compile_trigger_when_clause(const std::string& when_expr)
    {
        TriggerWhenEvaluator evaluator;
        return evaluator.compile_when_clause(when_expr);
    }

    bool evaluate_trigger_when_clause(const std::string& when_expr, const TriggerContext& context)
    {
        TriggerWhenEvaluator evaluator;
        evaluator.compile_when_clause(when_expr);
        return evaluator.evaluate_when_clause(context);
    }

    std::vector<std::string> parse_trigger_actions(const std::string& body)
    {
        TriggerActionInterpreter interpreter;
        return interpreter.parse_trigger_body(body);
    }

    bool validate_trigger_body(const std::string& body, const std::string& timing,
                               const std::string& for_each)
    {
        // Basic validation rules
        if (body.empty()) {
            return false;
        }

        std::string lower_body = body;
        std::transform(lower_body.begin(), lower_body.end(), lower_body.begin(), ::tolower);

        // AFTER triggers cannot modify NEW/OLD
        if (timing == "AFTER" && for_each == "ROW") {
            if (lower_body.find("new.") != std::string::npos &&
                lower_body.find("=") != std::string::npos) {
                return false; // Cannot assign to NEW in AFTER triggers
            }
        }

        // INSERT triggers cannot reference OLD
        if (lower_body.find("insert") != std::string::npos &&
            lower_body.find("old.") != std::string::npos) {
            return false; // Cannot reference OLD in INSERT triggers
        }

        // DELETE triggers cannot reference NEW
        if (lower_body.find("delete") != std::string::npos &&
            lower_body.find("new.") != std::string::npos) {
            return false; // Cannot reference NEW in DELETE triggers
        }

        return true;
    }

    Value evaluate_trigger_expression(const std::string& expr, const TriggerContext& context)
    {
        TriggerActionInterpreter interpreter;
        return interpreter.evaluate_expression(expr, context);
    }

    std::string format_trigger_error(const std::string& trigger_name, const std::string& error)
    {
        return "Trigger '" + trigger_name + "' error: " + error;
    }

} // namespace scratchbird::engine
