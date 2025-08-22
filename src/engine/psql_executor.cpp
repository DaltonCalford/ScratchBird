#include "scratchbird/engine/psql_executor.h"

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/expr.h"
#include "scratchbird/engine/parser.h"
#include "scratchbird/engine/system_oids.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <stdexcept>

namespace scratchbird::engine
{
    // PsqlScope implementation
    bool PsqlScope::has_variable(const std::string& name) const
    {
        auto it = variables.find(name);
        if (it != variables.end()) {
            return true;
        }
        return parent_scope ? parent_scope->has_variable(name) : false;
    }

    PsqlVariable* PsqlScope::get_variable(const std::string& name)
    {
        auto it = variables.find(name);
        if (it != variables.end()) {
            return &it->second;
        }
        return parent_scope ? parent_scope->get_variable(name) : nullptr;
    }

    const PsqlVariable* PsqlScope::get_variable(const std::string& name) const
    {
        auto it = variables.find(name);
        if (it != variables.end()) {
            return &it->second;
        }
        return parent_scope ? parent_scope->get_variable(name) : nullptr;
    }

    void PsqlScope::declare_variable(const std::string& name, const PsqlVariableType& type,
                                     const Value& default_value)
    {
        PsqlVariable var;
        var.name = name;
        var.type = type;
        var.value =
            default_value.is_null ? PsqlTypeManager::get_default_value(type) : default_value;
        var.initialized = !default_value.is_null;
        variables[name] = var;
    }

    bool PsqlScope::assign_variable(const std::string& name, const Value& value)
    {
        auto* var = get_variable(name);
        if (!var) {
            return false;
        }

        // Type validation and coercion
        if (!PsqlTypeManager::validate_assignment(var->type, value)) {
            return false;
        }

        var->value = PsqlTypeManager::coerce_value(var->type, value);
        var->initialized = true;
        return true;
    }

    // PsqlExecutionContext implementation
    PsqlExecutionContext::PsqlExecutionContext()
    {
        // Create initial scope
        push_scope();
    }

    PsqlExecutionContext::~PsqlExecutionContext() = default;

    void PsqlExecutionContext::push_scope()
    {
        auto new_scope = std::make_shared<PsqlScope>();
        if (!scope_stack_.empty()) {
            new_scope->parent_scope = scope_stack_.top();
        }
        scope_stack_.push(new_scope);
    }

    void PsqlExecutionContext::pop_scope()
    {
        if (scope_stack_.size() > 1) { // Keep at least one scope
            scope_stack_.pop();
        }
    }

    PsqlScope& PsqlExecutionContext::current_scope()
    {
        return *scope_stack_.top();
    }

    const PsqlScope& PsqlExecutionContext::current_scope() const
    {
        return *scope_stack_.top();
    }

    void PsqlExecutionContext::declare_variable(const std::string& name,
                                                const PsqlVariableType& type,
                                                const Value& default_value)
    {
        current_scope().declare_variable(name, type, default_value);
        variables_declared++;
    }

    bool PsqlExecutionContext::assign_variable(const std::string& name, const Value& value)
    {
        return current_scope().assign_variable(name, value);
    }

    Value PsqlExecutionContext::get_variable_value(const std::string& name) const
    {
        const auto* var = current_scope().get_variable(name);
        return var ? var->value : Value{};
    }

    bool PsqlExecutionContext::has_variable(const std::string& name) const
    {
        return current_scope().has_variable(name);
    }

    void PsqlExecutionContext::bind_parameter(const std::string& name, const std::string& mode,
                                              const PsqlVariableType& type, const Value& value)
    {
        PsqlVariable param;
        param.name = name;
        param.type = type;
        param.value = value;
        param.initialized = true;
        param.is_parameter = true;
        param.param_mode = mode;

        current_scope().variables[name] = param;
    }

    std::vector<Value> PsqlExecutionContext::get_output_parameters() const
    {
        std::vector<Value> outputs;
        for (const auto& [name, var] : current_scope().variables) {
            if (var.is_parameter && (var.param_mode == "OUT" || var.param_mode == "INOUT")) {
                outputs.push_back(var.value);
            }
        }
        return outputs;
    }

    // PsqlTypeManager implementation
    PsqlVariableType PsqlTypeManager::parse_type(const std::string& type_str)
    {
        PsqlVariableType type;
        std::string upper_type = type_str;
        std::transform(upper_type.begin(), upper_type.end(), upper_type.begin(),
                       [](unsigned char c) { return std::toupper(c); });

        // Handle basic types
        if (upper_type == "INTEGER" || upper_type == "INT") {
            type.type_name = "INTEGER";
        } else if (upper_type == "BIGINT") {
            type.type_name = "BIGINT";
        } else if (upper_type == "SMALLINT") {
            type.type_name = "SMALLINT";
        } else if (upper_type == "BOOLEAN" || upper_type == "BOOL") {
            type.type_name = "BOOLEAN";
        } else if (upper_type == "REAL" || upper_type == "FLOAT") {
            type.type_name = "REAL";
        } else if (upper_type == "DOUBLE" || upper_type == "DOUBLE PRECISION") {
            type.type_name = "DOUBLE";
        } else if (upper_type.substr(0, 7) == "VARCHAR") {
            type.type_name = "VARCHAR";
            // Extract size from VARCHAR(n)
            std::regex size_regex(R"(VARCHAR\((\d+)\))");
            std::smatch match;
            if (std::regex_search(upper_type, match, size_regex)) {
                type.size = std::stoi(match[1].str());
            } else {
                type.size = 255; // Default size
            }
        } else if (upper_type.substr(0, 4) == "CHAR") {
            type.type_name = "CHAR";
            std::regex size_regex(R"(CHAR\((\d+)\))");
            std::smatch match;
            if (std::regex_search(upper_type, match, size_regex)) {
                type.size = std::stoi(match[1].str());
            } else {
                type.size = 1; // Default size
            }
        } else if (upper_type == "TEXT") {
            type.type_name = "TEXT";
        } else if (upper_type == "DATE") {
            type.type_name = "DATE";
        } else if (upper_type == "TIME") {
            type.type_name = "TIME";
        } else if (upper_type == "TIMESTAMP") {
            type.type_name = "TIMESTAMP";
        } else {
            // Default to VARCHAR for unknown types
            type.type_name = "VARCHAR";
            type.size = 255;
        }

        return type;
    }

    bool PsqlTypeManager::validate_assignment(const PsqlVariableType& type, const Value& value)
    {
        if (value.is_null) {
            return type.nullable;
        }

        // Basic type compatibility checks
        if (type.type_name == "INTEGER" || type.type_name == "BIGINT" ||
            type.type_name == "SMALLINT") {
            return value.bytes.empty() ||
                   std::all_of(value.bytes.begin(), value.bytes.end(), [](unsigned char c) {
                       return std::isdigit(c) || c == '-' || c == '+';
                   });
        }
        if (type.type_name == "BOOLEAN") {
            std::string val = value.bytes;
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            return val == "true" || val == "false" || val == "1" || val == "0" || val == "t" ||
                   val == "f";
        }
        if (type.type_name == "VARCHAR" || type.type_name == "CHAR") {
            return value.bytes.size() <= static_cast<size_t>(type.size);
        }

        return true; // Allow assignment for other types
    }

    Value PsqlTypeManager::coerce_value(const PsqlVariableType& type, const Value& value)
    {
        if (value.is_null) {
            return value;
        }

        Value result = value;

        // Perform type-specific coercion
        if (type.type_name == "INTEGER" || type.type_name == "BIGINT" ||
            type.type_name == "SMALLINT") {
            try {
                std::int64_t num = std::stoll(value.bytes);
                result.bytes = std::to_string(num);
                result.u64 = static_cast<std::uint64_t>(num);
            } catch (...) {
                result.bytes = "0";
                result.u64 = 0;
            }
        } else if (type.type_name == "BOOLEAN") {
            std::string val = value.bytes;
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            bool bool_val = (val == "true" || val == "1" || val == "t");
            result.bytes = bool_val ? "true" : "false";
            result.u64 = bool_val ? 1 : 0;
        } else if (type.type_name == "VARCHAR" || type.type_name == "CHAR") {
            if (value.bytes.size() > static_cast<size_t>(type.size)) {
                result.bytes = value.bytes.substr(0, type.size);
            }
        }

        return result;
    }

    Value PsqlTypeManager::get_default_value(const PsqlVariableType& type)
    {
        Value default_val;
        default_val.is_null = false;

        if (type.type_name == "INTEGER" || type.type_name == "BIGINT" ||
            type.type_name == "SMALLINT") {
            default_val.bytes = "0";
            default_val.u64 = 0;
        } else if (type.type_name == "BOOLEAN") {
            default_val.bytes = "false";
            default_val.u64 = 0;
        } else if (type.type_name == "REAL" || type.type_name == "DOUBLE") {
            default_val.bytes = "0.0";
        } else {
            // String types default to empty string
            default_val.bytes = "";
        }

        return default_val;
    }

    // PsqlExecutor implementation
    PsqlExecutor::PsqlExecutor(const std::string& db_path) : db_path_(db_path) {}

    ExecutionResult PsqlExecutor::execute_block(const decltype(Ast{}.psqlBlock)& block)
    {
        ExecutionResult result;
        PsqlExecutionContext context;

        try {
            // Process parameter declarations if any
            // TODO: Parse block.params_raw to extract parameter declarations

            // Process variable declarations from the block body
            process_declarations(block, context);

            // Execute statements in the block
            for (const auto& stmt : block.body) {
                auto stmt_result = execute_statement(stmt, context);

                // Check for control flow changes
                if (context.control_state != PsqlExecutionContext::ControlFlowState::Normal) {
                    break;
                }

                // Accumulate any errors
                if (!stmt_result.error_message.empty()) {
                    result.error_message = stmt_result.error_message;
                    break;
                }

                context.statements_executed++;
            }

            // Handle return values if this is a function-like block
            if (!block.returns_raw.empty()) {
                auto return_values = extract_return_values(block, context);
                std::vector<std::string> string_values;
                for (const auto& val : return_values) {
                    if (val.is_null) {
                        string_values.push_back("NULL");
                    } else {
                        string_values.push_back(val.bytes.empty() ? std::to_string(val.u64)
                                                                  : val.bytes);
                    }
                }
                result.rows = {string_values};
                result.columns = {"return_value"}; // TODO: Parse returns_raw for column names
            } else {
                // For procedures, return execution status
                result.columns = {"statements_executed", "variables_declared"};
                result.rows = {{std::to_string(context.statements_executed),
                                std::to_string(context.variables_declared)}};
            }

        } catch (const std::exception& e) {
            result.error_message = std::string("PSQL execution error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_statement(const Ast::PsqlStmt& stmt,
                                                    PsqlExecutionContext& context)
    {
        ExecutionResult result;

        switch (stmt.kind) {
        case Ast::PsqlStmtKind::Declare:
            return execute_declare(stmt, context);

        case Ast::PsqlStmtKind::Assign:
            return execute_assignment(stmt, context);

        case Ast::PsqlStmtKind::If:
            return execute_if_statement(stmt, context);

        case Ast::PsqlStmtKind::While:
            return execute_while_loop(stmt, context);

        case Ast::PsqlStmtKind::ForSelect:
            return execute_for_loop(stmt, context);

        case Ast::PsqlStmtKind::ExecStmt:
            return execute_sql_statement(stmt.raw, context);

        default:
            result.error_message = "Unsupported PSQL statement type";
            break;
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_call(const decltype(Ast{}.psqlCall)& call)
    {
        ExecutionResult result;

        try {
            // Use catalog manager to find the procedure/function
            CatalogManager cm(db_path_);
            auto schema_oid = oid_public_schema(); // Default to public schema

            auto routine_info = cm.get_routine_by_name(schema_oid, call.routine_name);
            if (!routine_info) {
                result.error_message = "Procedure/function '" + call.routine_name + "' not found";
                return result;
            }

            // Get the stored procedure/function source code
            std::string source_code = routine_info->source_code;
            if (source_code.empty()) {
                result.error_message = "No source code found for '" + call.routine_name + "'";
                return result;
            }

            // Get parameter definitions
            auto param_info = cm.get_routine_params(routine_info->oid);

            // Create execution context
            PsqlExecutionContext context;

            // Bind input parameters from call arguments
            if (call.arguments.size() > param_info.size()) {
                result.error_message =
                    "Too many arguments for procedure '" + call.routine_name + "'";
                return result;
            }

            for (size_t i = 0; i < param_info.size() && i < call.arguments.size(); ++i) {
                const auto& param = param_info[i];
                const auto& arg_expr = call.arguments[i];

                // Evaluate argument expression (simplified for now)
                Value arg_value = evaluate_expression(arg_expr, context);

                // Create parameter type (simplified parsing)
                PsqlVariableType param_type = PsqlTypeManager::parse_type(param.type_json);

                // Bind the parameter
                context.bind_parameter(param.name, param.mode, param_type, arg_value);
            }

            // Parse the stored procedure source as an EXECUTE BLOCK
            // Wrap the source code in an EXECUTE BLOCK structure
            std::string block_sql = "EXECUTE BLOCK AS BEGIN " + source_code + " END";
            Ast block_ast = parse_sql(block_sql);

            ExecutionResult proc_result;
            if (block_ast.kind == NodeKind::PsqlBlock) {
                // Execute the procedure body
                proc_result = execute_block(block_ast.psqlBlock);
            } else {
                proc_result.error_message = "Failed to parse procedure body as PSQL block";
            }

            if (routine_info->kind == "FUNCTION") {
                // For functions, return the function result
                if (context.control_state == PsqlExecutionContext::ControlFlowState::Return) {
                    result.columns = {"function_result"};
                    result.rows = {{context.return_value.bytes}};
                } else {
                    result.columns = {"function_result"};
                    result.rows = {{"NULL"}};
                }
            } else {
                // For procedures, return execution status
                result.columns = {"procedure_status"};
                result.rows = {{"Procedure executed successfully"}};
            }

        } catch (const std::exception& e) {
            result.error_message = std::string("CALL execution error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_declare(const Ast::PsqlStmt& stmt,
                                                  PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            // Parse variable declaration: "DECLARE var_name type [DEFAULT value]"
            std::string decl = stmt.raw;

            // Simple regex-based parsing for now
            std::regex decl_regex(
                R"(DECLARE\s+(\w+)\s+(\w+(?:\(\d+\))?)\s*(?:DEFAULT\s+(.+?))?(?:\s*;|$))",
                std::regex_constants::icase);
            std::smatch match;

            if (std::regex_search(decl, match, decl_regex)) {
                std::string var_name = match[1].str();
                std::string type_str = match[2].str();
                std::string default_expr = match[3].str();

                PsqlVariableType type = PsqlTypeManager::parse_type(type_str);

                Value default_value;
                if (!default_expr.empty()) {
                    default_value = evaluate_expression(default_expr, context);
                } else {
                    default_value = PsqlTypeManager::get_default_value(type);
                }

                context.declare_variable(var_name, type, default_value);

                result.columns = {"variable_declared"};
                result.rows = {{var_name}};
            } else {
                result.error_message = "Invalid DECLARE syntax: " + stmt.raw;
            }

        } catch (const std::exception& e) {
            result.error_message = std::string("DECLARE error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_assignment(const Ast::PsqlStmt& stmt,
                                                     PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            // Parse assignment: "var_name = expression" or ":var_name = expression"
            std::string assign = stmt.raw;

            std::regex assign_regex(R"((?::)?(\w+)\s*=\s*(.+))", std::regex_constants::icase);
            std::smatch match;

            if (std::regex_search(assign, match, assign_regex)) {
                std::string var_name = match[1].str();
                std::string expr = match[2].str();

                // Remove trailing semicolon if present
                if (!expr.empty() && expr.back() == ';') {
                    expr.pop_back();
                }

                if (!context.has_variable(var_name)) {
                    result.error_message = "Undefined variable: " + var_name;
                    return result;
                }

                Value value = evaluate_expression(expr, context);

                if (!context.assign_variable(var_name, value)) {
                    result.error_message = "Type mismatch in assignment to variable: " + var_name;
                    return result;
                }

                result.columns = {"variable_assigned", "value"};
                result.rows = {{var_name, value.bytes}};
            } else {
                result.error_message = "Invalid assignment syntax: " + stmt.raw;
            }

        } catch (const std::exception& e) {
            result.error_message = std::string("Assignment error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_if_statement(const Ast::PsqlStmt& stmt,
                                                       PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            // Evaluate IF condition
            Value condition = evaluate_expression(stmt.when_condition_raw, context);

            bool is_true = false;
            if (!condition.is_null) {
                if (condition.bytes == "true" || condition.bytes == "1" || condition.u64 != 0) {
                    is_true = true;
                }
            }

            // Execute appropriate branch - for now, only handle the main nested block
            // TODO: Implement proper ELSE handling by parsing statement structure
            const auto& statements_to_execute =
                is_true ? stmt.nested : std::vector<Ast::PsqlStmt>{};

            context.push_scope(); // Create new scope for IF block

            for (const auto& nested_stmt : statements_to_execute) {
                auto nested_result = execute_statement(nested_stmt, context);
                if (!nested_result.error_message.empty()) {
                    result.error_message = nested_result.error_message;
                    break;
                }
                if (context.control_state != PsqlExecutionContext::ControlFlowState::Normal) {
                    break;
                }
            }

            context.pop_scope(); // Exit IF block scope

            result.columns = {"if_branch_executed"};
            result.rows = {{is_true ? "then" : "else"}};

        } catch (const std::exception& e) {
            result.error_message = std::string("IF statement error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_while_loop(const Ast::PsqlStmt& stmt,
                                                     PsqlExecutionContext& context)
    {
        ExecutionResult result;
        int iterations = 0;
        const int max_iterations = 10000; // Safety limit

        try {
            context.push_scope(); // Create new scope for loop

            while (iterations < max_iterations) {
                // Evaluate loop condition
                Value condition = evaluate_expression(stmt.when_condition_raw, context);

                bool continue_loop = false;
                if (!condition.is_null) {
                    if (condition.bytes == "true" || condition.bytes == "1" || condition.u64 != 0) {
                        continue_loop = true;
                    }
                }

                if (!continue_loop) {
                    break;
                }

                // Execute loop body
                for (const auto& nested_stmt : stmt.nested) {
                    auto nested_result = execute_statement(nested_stmt, context);
                    if (!nested_result.error_message.empty()) {
                        result.error_message = nested_result.error_message;
                        context.pop_scope();
                        return result;
                    }

                    // Handle control flow
                    if (context.control_state == PsqlExecutionContext::ControlFlowState::Break) {
                        context.control_state = PsqlExecutionContext::ControlFlowState::Normal;
                        goto exit_loop;
                    }
                    if (context.control_state == PsqlExecutionContext::ControlFlowState::Continue) {
                        context.control_state = PsqlExecutionContext::ControlFlowState::Normal;
                        break; // Continue to next iteration
                    }
                    if (context.control_state != PsqlExecutionContext::ControlFlowState::Normal) {
                        goto exit_loop;
                    }
                }

                iterations++;
            }

        exit_loop:
            context.pop_scope(); // Exit loop scope

            result.columns = {"loop_iterations"};
            result.rows = {{std::to_string(iterations)}};

            if (iterations >= max_iterations) {
                result.error_message = "WHILE loop exceeded maximum iterations limit";
            }

        } catch (const std::exception& e) {
            result.error_message = std::string("WHILE loop error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_for_loop(const Ast::PsqlStmt& stmt,
                                                   PsqlExecutionContext& context)
    {
        // Basic FOR loop implementation - placeholder for now
        ExecutionResult result;
        result.error_message = "FOR loops not yet implemented";
        return result;
    }

    ExecutionResult PsqlExecutor::execute_sql_statement(const std::string& sql,
                                                        PsqlExecutionContext& context)
    {
        // Execute SQL statement using the main executor
        // For now, this is a simplified implementation
        ExecutionResult result;

        try {
            // TODO: Integrate with main SQL executor, substituting variables
            result.columns = {"sql_executed"};
            result.rows = {{"SQL: " + sql}};

        } catch (const std::exception& e) {
            result.error_message = std::string("SQL execution error: ") + e.what();
        }

        return result;
    }

    Value PsqlExecutor::evaluate_expression(const std::string& expr,
                                            const PsqlExecutionContext& context)
    {
        Value result;

        try {
            // Simple expression evaluation - check if it's a variable reference first
            std::string trimmed_expr = expr;
            // Trim whitespace
            trimmed_expr.erase(0, trimmed_expr.find_first_not_of(" \t\n\r"));
            trimmed_expr.erase(trimmed_expr.find_last_not_of(" \t\n\r") + 1);

            // Check if it's a variable reference (possibly with : prefix)
            std::string var_name = trimmed_expr;
            if (!var_name.empty() && var_name[0] == ':') {
                var_name = var_name.substr(1);
            }

            if (context.has_variable(var_name)) {
                return context.get_variable_value(var_name);
            }

            // Check if it's a literal value
            if (trimmed_expr.empty()) {
                result.is_null = true;
                return result;
            }

            // Try to parse as integer
            if (std::all_of(trimmed_expr.begin(), trimmed_expr.end(),
                            [](char c) { return std::isdigit(c) || c == '-' || c == '+'; })) {
                try {
                    std::int64_t num = std::stoll(trimmed_expr);
                    result.bytes = trimmed_expr;
                    result.u64 = static_cast<std::uint64_t>(num);
                    result.is_null = false;
                    return result;
                } catch (...) {
                    // Fall through to string literal
                }
            }

            // Check if it's a quoted string literal
            if (trimmed_expr.size() >= 2 && trimmed_expr.front() == '\'' &&
                trimmed_expr.back() == '\'') {
                result.bytes = trimmed_expr.substr(1, trimmed_expr.size() - 2);
                result.is_null = false;
                return result;
            }

            // Default to string literal
            result.bytes = trimmed_expr;
            result.is_null = false;

        } catch (const std::exception& e) {
            result.is_null = true;
            result.bytes = std::string("Error: ") + e.what();
        }

        return result;
    }

    void PsqlExecutor::process_declarations(const decltype(Ast{}.psqlBlock)& block,
                                            PsqlExecutionContext& context)
    {
        // Process any DECLARE statements in the block body
        for (const auto& stmt : block.body) {
            if (stmt.kind == Ast::PsqlStmtKind::Declare) {
                execute_declare(stmt, context);
            }
        }
    }

    void PsqlExecutor::bind_parameters(const decltype(Ast{}.psqlBlock)& block,
                                       const std::vector<Value>& params,
                                       PsqlExecutionContext& context)
    {
        // TODO: Parse block.params_raw and bind input parameters
        // This is a placeholder implementation
    }

    std::vector<Value> PsqlExecutor::extract_return_values(const decltype(Ast{}.psqlBlock)& block,
                                                           const PsqlExecutionContext& context)
    {
        // TODO: Parse block.returns_raw and extract return values
        // For now, return empty vector
        return {};
    }

} // namespace scratchbird::engine
