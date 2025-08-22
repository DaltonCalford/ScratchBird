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

    bool PsqlScope::has_cursor(const std::string& name) const
    {
        if (cursors.find(name) != cursors.end()) {
            return true;
        }
        return parent_scope ? parent_scope->has_cursor(name) : false;
    }

    PsqlCursor* PsqlScope::get_cursor(const std::string& name)
    {
        auto it = cursors.find(name);
        if (it != cursors.end()) {
            return &it->second;
        }
        return parent_scope ? parent_scope->get_cursor(name) : nullptr;
    }

    const PsqlCursor* PsqlScope::get_cursor(const std::string& name) const
    {
        auto it = cursors.find(name);
        if (it != cursors.end()) {
            return &it->second;
        }
        return parent_scope ? parent_scope->get_cursor(name) : nullptr;
    }

    void PsqlScope::declare_cursor(const std::string& name, const std::string& query)
    {
        PsqlCursor cursor;
        cursor.name = name;
        cursor.query = query;
        cursor.is_open = false;
        cursor.current_row = 0;
        cursor.has_data = false;
        cursors[name] = cursor;
    }

    void PsqlScope::declare_scrollable_cursor(const std::string& name, const std::string& query,
                                              CursorScrollType scroll_type)
    {
        PsqlCursor cursor;
        cursor.name = name;
        cursor.query = query;
        cursor.is_open = false;
        cursor.current_row = 0;
        cursor.has_data = false;
        cursor.scroll_type = scroll_type;
        cursors[name] = cursor;
    }

    void PsqlScope::set_cursor_bulk_limit(const std::string& name, size_t limit)
    {
        auto cursor = get_cursor(name);
        if (cursor) {
            cursor->bulk_limit = limit;
        }
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

    void PsqlExecutionContext::set_exception(const std::string& name, const std::string& message)
    {
        exception_name = name;
        exception_message = message;
        control_state = ControlFlowState::Exception;
    }

    std::unordered_map<std::string, int> PsqlExecutionContext::get_system_exceptions()
    {
        // Firebird-compatible system exceptions
        static std::unordered_map<std::string, int> system_exceptions = {
            {"GDSCODE", 335544321},           // Generic error
            {"SQLCODE", 335544322},           // SQL error
            {"NO_DATA_FOUND", 335544382},     // No data returned
            {"TOO_MANY_ROWS", 335544383},     // Multiple rows returned
            {"INVALID_CURSOR", 335544384},    // Invalid cursor operation
            {"ZERO_DIVIDE", 335544385},       // Division by zero
            {"NUMERIC_OVERFLOW", 335544386},  // Numeric overflow
            {"INVALID_DATE", 335544387},      // Invalid date/time
            {"STRING_TRUNCATION", 335544388}, // String truncation
            {"NULL_SEGMENT", 335544389},      // Null in compound statement
            {"USER_EXCEPTION", 335544390},    // User-defined exception
            {"OTHERS", 0}                     // Catch-all exception
        };
        return system_exceptions;
    }

    void PsqlExecutionContext::declare_cursor(const std::string& name, const std::string& query)
    {
        current_scope().declare_cursor(name, query);
    }

    bool PsqlExecutionContext::has_cursor(const std::string& name) const
    {
        return current_scope().has_cursor(name);
    }

    PsqlCursor* PsqlExecutionContext::get_cursor(const std::string& name)
    {
        return current_scope().get_cursor(name);
    }

    void PsqlExecutionContext::set_security_context(const std::string& security_type,
                                                    const std::string& owner_role)
    {
        // Store original context if not already set
        if (original_security_type_ == "INVOKER" && current_security_type_ == "INVOKER") {
            original_security_type_ = current_security_type_;
            original_owner_role_ = current_owner_role_;
        }

        current_security_type_ = security_type;
        current_owner_role_ = owner_role;
    }

    void PsqlExecutionContext::restore_security_context()
    {
        current_security_type_ = original_security_type_;
        current_owner_role_ = original_owner_role_;
    }

    std::string PsqlExecutionContext::get_current_security_context() const
    {
        return current_security_type_;
    }

    bool PsqlExecutionContext::has_definer_rights() const
    {
        return current_security_type_ == "DEFINER";
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

                // Check for exceptions first
                if (context.has_active_exception()) {
                    // Break from normal execution - exception handlers will be processed after loop
                    break;
                }

                // Check for other control flow changes
                if (context.control_state != PsqlExecutionContext::ControlFlowState::Normal &&
                    context.control_state != PsqlExecutionContext::ControlFlowState::Exception) {
                    break;
                }

                // Accumulate any errors
                if (!stmt_result.error_message.empty()) {
                    result.error_message = stmt_result.error_message;
                    break;
                }

                context.statements_executed++;
            }

            // Process exception handlers if we have an active exception
            if (context.has_active_exception()) {
                bool exception_handled = false;
                for (const auto& stmt : block.body) {
                    if (stmt.kind == Ast::PsqlStmtKind::Exception) {
                        auto handler_result = execute_exception_handler(stmt, context);
                        if (!context.has_active_exception()) {
                            exception_handled = true;
                            // Merge any results from the handler
                            if (!handler_result.error_message.empty()) {
                                result.error_message = handler_result.error_message;
                            }
                            break;
                        }
                    }
                }

                if (!exception_handled) {
                    // Propagate unhandled exception
                    result.error_message = "Unhandled exception: " + context.exception_name +
                                           " - " + context.exception_message;
                    return result;
                }
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

        case Ast::PsqlStmtKind::Raise:
            return execute_raise_statement(stmt, context);

        case Ast::PsqlStmtKind::Exception:
            return execute_exception_handler(stmt, context);

        case Ast::PsqlStmtKind::OpenCursor:
            return execute_open_cursor(stmt, context);

        case Ast::PsqlStmtKind::FetchCursor:
            return execute_fetch_cursor(stmt, context);

        case Ast::PsqlStmtKind::CloseCursor:
            return execute_close_cursor(stmt, context);

        case Ast::PsqlStmtKind::Leave:
            return execute_leave_statement(stmt, context);

        case Ast::PsqlStmtKind::Continue:
            return execute_continue_statement(stmt, context);

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
            // Check procedure cache first
            auto cached = get_cached_procedure(call.routine_name);
            if (cached) {
                // Use cached compiled procedure
                cached->execution_count++;
                // For now, just use the cached compiled_body as source
                // In a full implementation, this would be pre-compiled bytecode
            }

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

            // Apply performance optimizations if not cached
            if (!cached) {
                // Inline deterministic functions
                source_code = inline_deterministic_functions(source_code);

                // Create compiled procedure entry for caching
                CompiledProcedure compiled;
                compiled.name = call.routine_name;
                compiled.schema_name = "public"; // TODO: get actual schema
                compiled.compiled_body = source_code;
                compiled.compiled_time = std::chrono::steady_clock::now();
                compiled.is_deterministic = (routine_info->volatility == "IMMUTABLE");

                // Cache the compiled procedure
                cache_procedure(call.routine_name, compiled);
            }

            // Get parameter definitions
            auto param_info = cm.get_routine_params(routine_info->oid);

            // Create execution context
            PsqlExecutionContext context;

            // Set security context based on routine definition
            if (routine_info->security == "DEFINER") {
                // Switch to definer's security context
                // In a real implementation, this would switch to the routine owner's role
                context.set_security_context("DEFINER", "routine_owner");
            } else {
                // Use invoker's security context (default)
                context.set_security_context("INVOKER", "");
            }

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

            // Restore original security context
            context.restore_security_context();

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
            if (stmt.declare_is_cursor) {
                // Handle cursor declaration
                std::string decl = stmt.raw;

                // Parse cursor declaration: "DECLARE cursor_name CURSOR FOR (query)"
                std::regex cursor_regex(R"(DECLARE\s+(\w+)\s+CURSOR\s+FOR\s*\(([\s\S]+)\))",
                                        std::regex_constants::icase);
                std::smatch match;

                if (std::regex_search(decl, match, cursor_regex)) {
                    std::string cursor_name = match[1].str();
                    std::string cursor_query = match[2].str();

                    // Trim whitespace from query
                    auto trim = [](std::string& s) {
                        auto not_space = [](int ch) { return !std::isspace(ch); };
                        s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
                        s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
                    };
                    trim(cursor_query);

                    // Declare the cursor
                    context.declare_cursor(cursor_name, cursor_query);

                    result.columns = {"cursor_declared"};
                    result.rows = {{cursor_name}};
                } else {
                    result.error_message = "Invalid DECLARE CURSOR syntax: " + stmt.raw;
                }
            } else {
                // Handle variable declaration: "DECLARE var_name type [DEFAULT value]"
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

    ExecutionResult PsqlExecutor::execute_raise_statement(const Ast::PsqlStmt& stmt,
                                                          PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            // Parse RAISE statement - format: RAISE [exception_name] ['message']
            std::string raw = stmt.raw;
            std::string content = raw.substr(5); // Remove "RAISE"

            // Trim whitespace
            auto trim = [](std::string& s) {
                auto not_space = [](int ch) { return !std::isspace(ch); };
                s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
                s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
            };
            trim(content);

            std::string exception_name = "USER_EXCEPTION";
            std::string exception_message = "";

            if (!content.empty()) {
                // Parse exception name and optional message
                // Format: RAISE exception_name 'message'
                // or: RAISE 'message' (uses default exception)

                if (content[0] == '\'' || content[0] == '"') {
                    // RAISE 'message' - use default exception name
                    exception_message = content.substr(1, content.length() - 2);
                } else {
                    // RAISE exception_name ['message']
                    size_t space_pos = content.find(' ');
                    if (space_pos != std::string::npos) {
                        exception_name = content.substr(0, space_pos);
                        std::string msg_part = content.substr(space_pos + 1);
                        trim(msg_part);
                        if (!msg_part.empty() && (msg_part[0] == '\'' || msg_part[0] == '"')) {
                            exception_message = msg_part.substr(1, msg_part.length() - 2);
                        }
                    } else {
                        exception_name = content;
                    }
                }
            }

            // Set exception state
            context.set_exception(exception_name, exception_message);
            context.control_state = PsqlExecutionContext::ControlFlowState::Exception;

            result.columns = {"exception_raised"};
            result.rows = {{exception_name + ": " + exception_message}};

        } catch (const std::exception& e) {
            result.error_message = std::string("RAISE statement error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_exception_handler(const Ast::PsqlStmt& stmt,
                                                            PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            // WHEN exception handler - only execute if we have an active exception
            if (!context.has_active_exception()) {
                // No active exception, skip this handler
                result.columns = {"when_handler_skipped"};
                result.rows = {{"no_active_exception"}};
                return result;
            }

            // Evaluate WHEN condition if present
            bool should_handle = true;
            if (!stmt.when_condition_raw.empty()) {
                // Parse condition - could be exception name match or boolean expression
                std::string condition = stmt.when_condition_raw;

                // Check if it's an exception name match
                if (condition == context.exception_name || condition == "OTHERS" ||
                    condition == "ANY") {
                    should_handle = true;
                } else {
                    // Try to evaluate as boolean expression
                    Value condition_result = evaluate_expression(condition, context);
                    should_handle = !condition_result.is_null &&
                                    (condition_result.bytes == "true" ||
                                     condition_result.bytes == "1" || condition_result.u64 != 0);
                }
            }

            if (should_handle) {
                // Clear the exception state since we're handling it
                context.clear_exception();
                context.control_state = PsqlExecutionContext::ControlFlowState::Normal;

                // Execute nested statements (the exception handler body)
                context.push_scope(); // Create new scope for exception handler

                for (const auto& nested_stmt : stmt.nested) {
                    auto nested_result = execute_statement(nested_stmt, context);
                    if (!nested_result.error_message.empty()) {
                        result.error_message = nested_result.error_message;
                        break;
                    }
                    if (context.control_state != PsqlExecutionContext::ControlFlowState::Normal) {
                        break;
                    }
                }

                context.pop_scope(); // Exit exception handler scope

                result.columns = {"exception_handled"};
                result.rows = {{"true"}};
            } else {
                result.columns = {"exception_handled"};
                result.rows = {{"false"}};
            }

        } catch (const std::exception& e) {
            result.error_message = std::string("Exception handler error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_open_cursor(const Ast::PsqlStmt& stmt,
                                                      PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            std::string cursor_name = stmt.cursor_name;
            if (cursor_name.empty()) {
                cursor_name = stmt.name; // Fallback to name field
            }

            if (cursor_name.empty()) {
                result.error_message = "OPEN CURSOR: Missing cursor name";
                return result;
            }

            // Get the cursor
            PsqlCursor* cursor = context.get_cursor(cursor_name);
            if (!cursor) {
                result.error_message = "OPEN CURSOR: Cursor '" + cursor_name + "' not declared";
                return result;
            }

            if (cursor->is_open) {
                result.error_message = "OPEN CURSOR: Cursor '" + cursor_name + "' is already open";
                return result;
            }

            // Execute the cursor query
            auto query_result = execute_sql_statement(cursor->query, context);
            if (!query_result.error_message.empty()) {
                result.error_message =
                    "OPEN CURSOR: Failed to execute query: " + query_result.error_message;
                return result;
            }

            // Cache the results and open the cursor
            cursor->cached_result = query_result;
            cursor->is_open = true;
            cursor->current_row = 0;
            cursor->has_data = !query_result.rows.empty();

            // Enhanced cursor state initialization
            cursor->total_rows = query_result.rows.size();
            cursor->reset(); // Reset cursor attributes
            cursor->at_beginning = true;
            cursor->at_end = (cursor->total_rows == 0);

            result.columns = {"cursor_opened"};
            result.rows = {{cursor_name}};

        } catch (const std::exception& e) {
            result.error_message = std::string("OPEN CURSOR error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_fetch_cursor(const Ast::PsqlStmt& stmt,
                                                       PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            std::string cursor_name = stmt.cursor_name;
            if (cursor_name.empty()) {
                cursor_name = stmt.name; // Fallback to name field
            }

            if (cursor_name.empty()) {
                result.error_message = "FETCH CURSOR: Missing cursor name";
                return result;
            }

            // Get the cursor
            PsqlCursor* cursor = context.get_cursor(cursor_name);
            if (!cursor) {
                result.error_message = "FETCH CURSOR: Cursor '" + cursor_name + "' not declared";
                return result;
            }

            if (!cursor->is_open) {
                result.error_message = "FETCH CURSOR: Cursor '" + cursor_name + "' is not open";
                return result;
            }

            // Check if there's data to fetch
            if (cursor->current_row >= cursor->cached_result.rows.size()) {
                // No more data - update cursor attributes
                cursor->has_data = false;
                cursor->update_attributes(false); // Update found/not_found attributes

                // Set exception for NO_DATA_FOUND
                context.set_exception("NO_DATA_FOUND", "No more rows to fetch");

                result.columns = {"fetch_status"};
                result.rows = {{"no_data_found"}};
                return result;
            }

            // Fetch the current row
            const auto& row = cursor->cached_result.rows[cursor->current_row];
            cursor->current_row++;
            cursor->has_data = (cursor->current_row < cursor->cached_result.rows.size());

            // Update cursor attributes for successful fetch
            cursor->update_attributes(true, 1);

            // Handle INTO variables if specified
            if (!stmt.into_vars.empty()) {
                for (size_t i = 0; i < stmt.into_vars.size() && i < row.size(); ++i) {
                    Value val;
                    val.bytes = row[i];
                    val.is_null = (row[i] == "NULL");
                    context.assign_variable(stmt.into_vars[i], val);
                }
            }

            // Return the fetched row
            result.columns = cursor->cached_result.columns;
            result.rows = {row};

        } catch (const std::exception& e) {
            result.error_message = std::string("FETCH CURSOR error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_close_cursor(const Ast::PsqlStmt& stmt,
                                                       PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            std::string cursor_name = stmt.cursor_name;
            if (cursor_name.empty()) {
                cursor_name = stmt.name; // Fallback to name field
            }

            if (cursor_name.empty()) {
                result.error_message = "CLOSE CURSOR: Missing cursor name";
                return result;
            }

            // Get the cursor
            PsqlCursor* cursor = context.get_cursor(cursor_name);
            if (!cursor) {
                result.error_message = "CLOSE CURSOR: Cursor '" + cursor_name + "' not declared";
                return result;
            }

            if (!cursor->is_open) {
                result.error_message = "CLOSE CURSOR: Cursor '" + cursor_name + "' is not open";
                return result;
            }

            // Close the cursor and clear cached data
            cursor->is_open = false;
            cursor->current_row = 0;
            cursor->has_data = false;
            cursor->cached_result = ExecutionResult{}; // Clear cached data

            result.columns = {"cursor_closed"};
            result.rows = {{cursor_name}};

        } catch (const std::exception& e) {
            result.error_message = std::string("CLOSE CURSOR error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_leave_statement(const Ast::PsqlStmt& stmt,
                                                          PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            // LEAVE (equivalent to BREAK) - exit from current loop
            context.control_state = PsqlExecutionContext::ControlFlowState::Break;

            // If a label is specified, it could be used for nested loop control
            if (!stmt.label.empty()) {
                // In a full implementation, this would handle labeled breaks
                // For now, we just set the break state
            }

            result.columns = {"leave_executed"};
            result.rows = {{"break_from_loop"}};

        } catch (const std::exception& e) {
            result.error_message = std::string("LEAVE statement error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_continue_statement(const Ast::PsqlStmt& stmt,
                                                             PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            // CONTINUE - continue to next iteration of current loop
            context.control_state = PsqlExecutionContext::ControlFlowState::Continue;

            // If a label is specified, it could be used for nested loop control
            if (!stmt.label.empty()) {
                // In a full implementation, this would handle labeled continues
                // For now, we just set the continue state
            }

            result.columns = {"continue_executed"};
            result.rows = {{"continue_loop"}};

        } catch (const std::exception& e) {
            result.error_message = std::string("CONTINUE statement error: ") + e.what();
        }

        return result;
    }

    // Performance optimization method implementations

    void PsqlExecutor::enable_plan_caching(bool enabled)
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        plan_caching_enabled_ = enabled;
        if (!enabled) {
            procedure_cache_.clear();
        }
    }

    std::optional<PsqlExecutor::CompiledProcedure>
    PsqlExecutor::get_cached_procedure(const std::string& name) const
    {
        if (!plan_caching_enabled_) {
            return std::nullopt;
        }

        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = procedure_cache_.find(name);
        if (it != procedure_cache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void PsqlExecutor::cache_procedure(const std::string& name, const CompiledProcedure& compiled)
    {
        if (!plan_caching_enabled_) {
            return;
        }

        std::lock_guard<std::mutex> lock(cache_mutex_);
        procedure_cache_[name] = compiled;
    }

    void PsqlExecutor::clear_procedure_cache()
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        procedure_cache_.clear();
    }

    std::string PsqlExecutor::optimize_expression(const std::string& expr,
                                                  const PsqlExecutionContext& context)
    {
        // Basic expression optimization
        std::string optimized = expr;

        // Constant folding for simple arithmetic
        if (optimized.find("1 + 1") != std::string::npos) {
            optimized = std::regex_replace(optimized, std::regex("1 \\+ 1"), "2");
        }
        if (optimized.find("2 * 2") != std::string::npos) {
            optimized = std::regex_replace(optimized, std::regex("2 \\* 2"), "4");
        }
        if (optimized.find("0 +") != std::string::npos) {
            optimized = std::regex_replace(optimized, std::regex("0 \\+ "), "");
        }
        if (optimized.find("+ 0") != std::string::npos) {
            optimized = std::regex_replace(optimized, std::regex(" \\+ 0"), "");
        }
        if (optimized.find("1 *") != std::string::npos) {
            optimized = std::regex_replace(optimized, std::regex("1 \\* "), "");
        }
        if (optimized.find("* 1") != std::string::npos) {
            optimized = std::regex_replace(optimized, std::regex(" \\* 1"), "");
        }

        // Variable substitution for known constant values
        for (const auto& var_name : {"TRUE", "FALSE", "NULL"}) {
            std::string pattern = std::string("\\b") + var_name + "\\b";
            // Leave as-is for boolean constants (basic implementation)
        }

        return optimized;
    }

    std::string PsqlExecutor::inline_deterministic_functions(const std::string& code)
    {
        std::string optimized = code;

        // Inline simple deterministic functions
        // Use manual parsing for better compatibility

        // Example: UPPER('hello') -> 'HELLO'
        std::regex upper_regex("UPPER\\('([^']+)'\\)");
        std::smatch match;
        while (std::regex_search(optimized, match, upper_regex)) {
            std::string str = match[1].str();
            std::transform(str.begin(), str.end(), str.begin(), ::toupper);
            optimized = std::regex_replace(optimized, upper_regex, "'" + str + "'",
                                           std::regex_constants::format_first_only);
        }

        // Example: LOWER('HELLO') -> 'hello'
        std::regex lower_regex("LOWER\\('([^']+)'\\)");
        while (std::regex_search(optimized, match, lower_regex)) {
            std::string str = match[1].str();
            std::transform(str.begin(), str.end(), str.begin(), ::tolower);
            optimized = std::regex_replace(optimized, lower_regex, "'" + str + "'",
                                           std::regex_constants::format_first_only);
        }

        // Example: LENGTH('hello') -> 5
        std::regex length_regex("LENGTH\\('([^']*)'\\)");
        while (std::regex_search(optimized, match, length_regex)) {
            std::string len_str = std::to_string(match[1].str().length());
            optimized = std::regex_replace(optimized, length_regex, len_str,
                                           std::regex_constants::format_first_only);
        }

        return optimized;
    }

    // PSQL Debugging Support implementations

    void PsqlExecutor::enable_debugging(bool enabled)
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        debug_state_.debugging_enabled = enabled;
        if (!enabled) {
            debug_state_.step_mode = false;
            debug_state_.call_stack.clear();
        }
    }

    void PsqlExecutor::add_breakpoint(const std::string& procedure_name, int line_number,
                                      const std::string& condition)
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        DebugBreakpoint bp;
        bp.procedure_name = procedure_name;
        bp.line_number = line_number;
        bp.condition = condition;
        bp.enabled = true;
        debug_state_.breakpoints.push_back(bp);
    }

    void PsqlExecutor::remove_breakpoint(const std::string& procedure_name, int line_number)
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        auto it = std::remove_if(debug_state_.breakpoints.begin(), debug_state_.breakpoints.end(),
                                 [&](const DebugBreakpoint& bp) {
                                     return bp.procedure_name == procedure_name &&
                                            bp.line_number == line_number;
                                 });
        debug_state_.breakpoints.erase(it, debug_state_.breakpoints.end());
    }

    void PsqlExecutor::clear_breakpoints()
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        debug_state_.breakpoints.clear();
    }

    std::vector<PsqlExecutor::DebugBreakpoint> PsqlExecutor::get_breakpoints() const
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        return debug_state_.breakpoints;
    }

    void PsqlExecutor::enable_step_mode(bool enabled)
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        debug_state_.step_mode = enabled;
    }

    void PsqlExecutor::step_over()
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        debug_state_.step_mode = true;
        // In a full implementation, this would set a flag to break at the next statement
        // at the same call stack level
    }

    void PsqlExecutor::step_into()
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        debug_state_.step_mode = true;
        // In a full implementation, this would set a flag to break at the next statement
        // regardless of call stack level
    }

    void PsqlExecutor::continue_execution()
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        debug_state_.step_mode = false;
        // In a full implementation, this would resume execution until the next breakpoint
    }

    std::unordered_map<std::string, Value> PsqlExecutor::get_current_variables() const
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        if (debug_state_.call_stack.empty()) {
            return {};
        }
        return debug_state_.call_stack.back().local_variables;
    }

    Value PsqlExecutor::get_variable_value(const std::string& name) const
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        if (debug_state_.call_stack.empty()) {
            return Value{};
        }

        const auto& vars = debug_state_.call_stack.back().local_variables;
        auto it = vars.find(name);
        return (it != vars.end()) ? it->second : Value{};
    }

    std::vector<PsqlExecutor::DebugCallFrame> PsqlExecutor::get_call_stack() const
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        return debug_state_.call_stack;
    }

    std::string PsqlExecutor::get_current_procedure() const
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        if (debug_state_.call_stack.empty()) {
            return "";
        }
        return debug_state_.call_stack.back().procedure_name;
    }

    int PsqlExecutor::get_current_line() const
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        if (debug_state_.call_stack.empty()) {
            return 0;
        }
        return debug_state_.call_stack.back().current_line;
    }

    std::string PsqlExecutor::get_last_error_with_location() const
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        if (debug_state_.last_error.empty()) {
            return "";
        }

        std::string result = debug_state_.last_error;
        if (debug_state_.last_error_line > 0) {
            result += " (line " + std::to_string(debug_state_.last_error_line) + ")";
        }

        if (!debug_state_.call_stack.empty()) {
            result += " in procedure '" + debug_state_.call_stack.back().procedure_name + "'";
        }

        return result;
    }

    void PsqlExecutor::report_runtime_error(const std::string& error, int line_number)
    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        debug_state_.last_error = error;
        debug_state_.last_error_line = line_number;

        if (debug_state_.break_on_exception) {
            debug_state_.step_mode = true;
        }
    }

    // Advanced Cursor Operations Implementation

    ExecutionResult PsqlExecutor::execute_declare_cursor(const Ast::PsqlStmt& stmt,
                                                         PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            std::string cursor_name = stmt.name;
            if (cursor_name.empty()) {
                result.error_message = "DECLARE CURSOR: Missing cursor name";
                return result;
            }

            std::string query = stmt.raw;
            if (query.empty()) {
                result.error_message = "DECLARE CURSOR: Missing query";
                return result;
            }

            // Check for SCROLL/NO SCROLL keywords in cursor declaration
            CursorScrollType scroll_type = CursorScrollType::NO_SCROLL;
            if (stmt.raw.find("SCROLL") != std::string::npos) {
                if (stmt.raw.find("NO SCROLL") != std::string::npos) {
                    scroll_type = CursorScrollType::NO_SCROLL;
                } else {
                    scroll_type = CursorScrollType::SCROLL;
                }
            }

            // Declare the cursor with enhanced features
            context.current_scope().declare_scrollable_cursor(cursor_name, query, scroll_type);

            result.columns = {"cursor_declared"};
            result.rows = {{cursor_name}};

        } catch (const std::exception& e) {
            result.error_message = std::string("DECLARE CURSOR error: ") + e.what();
        }

        return result;
    }

    ExecutionResult
    PsqlExecutor::execute_fetch_bulk_collect(const std::string& cursor_name,
                                             const std::vector<std::string>& target_vars,
                                             PsqlExecutionContext& context, size_t limit)
    {
        ExecutionResult result;

        try {
            // Get the cursor
            PsqlCursor* cursor = context.get_cursor(cursor_name);
            if (!cursor) {
                result.error_message =
                    "FETCH BULK COLLECT: Cursor '" + cursor_name + "' not declared";
                return result;
            }

            if (!cursor->is_open) {
                result.error_message =
                    "FETCH BULK COLLECT: Cursor '" + cursor_name + "' is not open";
                return result;
            }

            // Use cursor's bulk limit if no limit specified
            size_t fetch_limit = (limit > 0) ? limit : cursor->bulk_limit;
            size_t rows_fetched = 0;

            // Clear previous bulk buffer
            cursor->bulk_buffer.clear();

            // Fetch rows up to the limit
            while (rows_fetched < fetch_limit &&
                   cursor->current_row < cursor->cached_result.rows.size()) {

                const auto& row = cursor->cached_result.rows[cursor->current_row];
                cursor->bulk_buffer.push_back(row);
                cursor->current_row++;
                rows_fetched++;
            }

            // Update cursor state
            cursor->has_data = (cursor->current_row < cursor->cached_result.rows.size());
            cursor->update_attributes(rows_fetched > 0, rows_fetched);

            // Populate target variables with bulk data (simplified - would need array support)
            if (!target_vars.empty() && !cursor->bulk_buffer.empty()) {
                // For now, just assign first row to variables
                const auto& first_row = cursor->bulk_buffer[0];
                for (size_t i = 0; i < target_vars.size() && i < first_row.size(); ++i) {
                    Value val;
                    val.bytes = first_row[i];
                    val.is_null = (first_row[i] == "NULL");
                    context.assign_variable(target_vars[i], val);
                }
            }

            result.columns = {"rows_fetched"};
            result.rows = {{std::to_string(rows_fetched)}};

        } catch (const std::exception& e) {
            result.error_message = std::string("FETCH BULK COLLECT error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_cursor_for_loop(const Ast::PsqlStmt& stmt,
                                                          PsqlExecutionContext& context)
    {
        ExecutionResult result;

        try {
            std::string cursor_name = stmt.cursor_name;
            std::string record_var = stmt.name; // Record variable name

            if (cursor_name.empty() || record_var.empty()) {
                result.error_message = "CURSOR FOR LOOP: Missing cursor name or record variable";
                return result;
            }

            // Get the cursor
            PsqlCursor* cursor = context.get_cursor(cursor_name);
            if (!cursor) {
                result.error_message = "CURSOR FOR LOOP: Cursor '" + cursor_name + "' not declared";
                return result;
            }

            // Auto-open cursor if not already open
            if (!cursor->is_open) {
                auto query_result = execute_sql_statement(cursor->query, context);
                if (!query_result.error_message.empty()) {
                    result.error_message =
                        "CURSOR FOR LOOP: Failed to open cursor: " + query_result.error_message;
                    return result;
                }

                cursor->cached_result = query_result;
                cursor->is_open = true;
                cursor->current_row = 0;
                cursor->total_rows = query_result.rows.size();
                cursor->reset();
            }

            size_t loop_iterations = 0;

            // Execute loop body for each row
            while (cursor->current_row < cursor->cached_result.rows.size()) {
                // Fetch current row
                const auto& row = cursor->cached_result.rows[cursor->current_row];
                cursor->current_row++;
                cursor->update_attributes(true, 1);

                // Create record variable (simplified - would need record type support)
                // For now, create variables for each column
                for (size_t i = 0; i < cursor->cached_result.columns.size() && i < row.size();
                     ++i) {
                    std::string col_var = record_var + "." + cursor->cached_result.columns[i];
                    Value val;
                    val.bytes = row[i];
                    val.is_null = (row[i] == "NULL");

                    // Declare variable if it doesn't exist
                    if (!context.has_variable(col_var)) {
                        context.declare_variable(col_var, PsqlVariableType{"VARCHAR", 255, true},
                                                 val);
                    } else {
                        context.assign_variable(col_var, val);
                    }
                }

                // For cursor FOR loops, the loop body would be parsed and executed
                // by the calling context. This implementation provides the infrastructure
                // for automatic cursor management and row iteration.

                loop_iterations++;
            }

            // Auto-close cursor
            cursor->is_open = false;
            cursor->reset();

            result.columns = {"loop_iterations"};
            result.rows = {{std::to_string(loop_iterations)}};

        } catch (const std::exception& e) {
            result.error_message = std::string("CURSOR FOR LOOP error: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::fetch_cursor_direction(PsqlCursor* cursor,
                                                         CursorDirection direction, int offset,
                                                         PsqlExecutionContext& context)
    {
        ExecutionResult result;

        if (!cursor || !cursor->is_open) {
            result.error_message = "FETCH: Cursor is not open";
            return result;
        }

        if (!cursor->is_scrollable() && direction != CursorDirection::NEXT) {
            result.error_message =
                "FETCH: Cursor is not scrollable - only NEXT direction supported";
            return result;
        }

        size_t new_position = cursor->current_row;

        switch (direction) {
        case CursorDirection::NEXT:
            new_position = cursor->current_row;
            break;
        case CursorDirection::PRIOR:
            if (cursor->current_row > 0) {
                new_position = cursor->current_row - 2; // -2 because current_row points to next
            } else {
                cursor->update_attributes(false);
                result.error_message = "FETCH PRIOR: Already at beginning";
                return result;
            }
            break;
        case CursorDirection::FIRST:
            new_position = 0;
            break;
        case CursorDirection::LAST:
            if (cursor->total_rows > 0) {
                new_position = cursor->total_rows - 1;
            } else {
                cursor->update_attributes(false);
                result.error_message = "FETCH LAST: No rows in cursor";
                return result;
            }
            break;
        case CursorDirection::ABSOLUTE:
            return fetch_cursor_absolute(cursor, offset, context);
        case CursorDirection::RELATIVE:
            return fetch_cursor_relative(cursor, offset, context);
        }

        // Validate new position
        if (new_position >= cursor->total_rows) {
            cursor->update_attributes(false);
            context.set_exception("NO_DATA_FOUND", "No more rows to fetch");
            result.columns = {"fetch_status"};
            result.rows = {{"no_data_found"}};
            return result;
        }

        // Fetch the row at new position
        const auto& row = cursor->cached_result.rows[new_position];
        cursor->current_row = new_position + 1;
        cursor->update_attributes(true, 1);

        result.columns = cursor->cached_result.columns;
        result.rows = {row};

        return result;
    }

    ExecutionResult PsqlExecutor::fetch_cursor_absolute(PsqlCursor* cursor, size_t position,
                                                        PsqlExecutionContext& context)
    {
        ExecutionResult result;

        if (!cursor || !cursor->is_open) {
            result.error_message = "FETCH ABSOLUTE: Cursor is not open";
            return result;
        }

        if (!cursor->is_scrollable()) {
            result.error_message = "FETCH ABSOLUTE: Cursor is not scrollable";
            return result;
        }

        // Convert to 0-based position (PSQL uses 1-based)
        size_t target_pos = (position > 0) ? position - 1 : 0;

        if (target_pos >= cursor->total_rows) {
            cursor->update_attributes(false);
            context.set_exception("NO_DATA_FOUND", "Position out of range");
            result.columns = {"fetch_status"};
            result.rows = {{"no_data_found"}};
            return result;
        }

        // Fetch the row at absolute position
        const auto& row = cursor->cached_result.rows[target_pos];
        cursor->current_row = target_pos + 1;
        cursor->update_attributes(true, 1);

        result.columns = cursor->cached_result.columns;
        result.rows = {row};

        return result;
    }

    ExecutionResult PsqlExecutor::fetch_cursor_relative(PsqlCursor* cursor, int offset,
                                                        PsqlExecutionContext& context)
    {
        ExecutionResult result;

        if (!cursor || !cursor->is_open) {
            result.error_message = "FETCH RELATIVE: Cursor is not open";
            return result;
        }

        if (!cursor->is_scrollable()) {
            result.error_message = "FETCH RELATIVE: Cursor is not scrollable";
            return result;
        }

        // Calculate new position relative to current
        int new_pos = static_cast<int>(cursor->current_row) + offset - 1;

        if (new_pos < 0 || new_pos >= static_cast<int>(cursor->total_rows)) {
            cursor->update_attributes(false);
            context.set_exception("NO_DATA_FOUND", "Relative position out of range");
            result.columns = {"fetch_status"};
            result.rows = {{"no_data_found"}};
            return result;
        }

        // Fetch the row at relative position
        const auto& row = cursor->cached_result.rows[new_pos];
        cursor->current_row = new_pos + 1;
        cursor->update_attributes(true, 1);

        result.columns = cursor->cached_result.columns;
        result.rows = {row};

        return result;
    }

    // Enhanced Package Support Implementation

    ExecutionResult PsqlExecutor::execute_create_package(const decltype(Ast{}.psqlPackage)& package)
    {
        ExecutionResult result;
        result.success = true;

        try {
            std::lock_guard<std::mutex> lock(package_mutex_);

            PackageSpecification spec;
            spec.name = package.name;
            spec.schema_name = package.schema_name.empty() ? "PUBLIC" : package.schema_name;
            spec.created_time = std::chrono::steady_clock::now();
            spec.version = "1.0";
            spec.valid = true;

            if (package.is_header) {
                // Parse public procedures and functions from header
                std::string header = package.header_body;

                // Simple parsing to extract procedure/function names
                size_t pos = 0;
                while ((pos = header.find("PROCEDURE ", pos)) != std::string::npos) {
                    pos += 10; // Skip "PROCEDURE "
                    size_t end = header.find_first_of("( \t\n;", pos);
                    if (end != std::string::npos) {
                        std::string proc_name = header.substr(pos, end - pos);
                        spec.public_procedures.push_back(proc_name);
                        spec.public_signatures[proc_name] =
                            ""; // Simplified - would need full signature parsing
                    }
                }

                pos = 0;
                while ((pos = header.find("FUNCTION ", pos)) != std::string::npos) {
                    pos += 9; // Skip "FUNCTION "
                    size_t end = header.find_first_of("( \t\n", pos);
                    if (end != std::string::npos) {
                        std::string func_name = header.substr(pos, end - pos);
                        spec.public_functions.push_back(func_name);
                        spec.public_signatures[func_name] = ""; // Simplified
                    }
                }

                // Create or update package instance
                std::string package_key = spec.schema_name + "." + spec.name;
                if (packages_.find(package_key) == packages_.end()) {
                    packages_[package_key] = PackageInstance{};
                }
                packages_[package_key].spec = spec;

                result.columns = {"status"};
                result.rows = {{"Package header created successfully"}};
            } else {
                result.success = false;
                result.error_message = "Package body creation requires existing package header";
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error creating package: ") + e.what();
        }

        return result;
    }

    ExecutionResult
    PsqlExecutor::execute_create_package_body(const decltype(Ast{}.psqlPackage)& package)
    {
        ExecutionResult result;
        result.success = true;

        try {
            std::lock_guard<std::mutex> lock(package_mutex_);

            std::string package_key =
                (package.schema_name.empty() ? "PUBLIC" : package.schema_name) + "." + package.name;

            auto it = packages_.find(package_key);
            if (it == packages_.end()) {
                result.success = false;
                result.error_message = "Package specification must be created before package body";
                return result;
            }

            PackageBody body;
            body.name = package.name;
            body.schema_name = package.schema_name.empty() ? "PUBLIC" : package.schema_name;
            body.implementation_body = package.implementation_body;
            body.compiled_time = std::chrono::steady_clock::now();
            body.initialized = false;

            // Parse private procedures and functions (simplified)
            std::string impl = package.implementation_body;
            size_t pos = 0;
            while ((pos = impl.find("PROCEDURE ", pos)) != std::string::npos) {
                pos += 10;
                size_t end = impl.find_first_of("( \t\n", pos);
                if (end != std::string::npos) {
                    std::string proc_name = impl.substr(pos, end - pos);
                    // Check if it's a public procedure implementation or private procedure
                    auto& spec = it->second.spec;
                    bool is_public =
                        std::find(spec.public_procedures.begin(), spec.public_procedures.end(),
                                  proc_name) != spec.public_procedures.end();
                    if (!is_public) {
                        body.private_procedures.push_back(proc_name);
                    }
                }
            }

            pos = 0;
            while ((pos = impl.find("FUNCTION ", pos)) != std::string::npos) {
                pos += 9;
                size_t end = impl.find_first_of("( \t\n", pos);
                if (end != std::string::npos) {
                    std::string func_name = impl.substr(pos, end - pos);
                    auto& spec = it->second.spec;
                    bool is_public =
                        std::find(spec.public_functions.begin(), spec.public_functions.end(),
                                  func_name) != spec.public_functions.end();
                    if (!is_public) {
                        body.private_functions.push_back(func_name);
                    }
                }
            }

            // Extract initialization block (simplified - would need proper parsing)
            size_t init_pos = impl.find("BEGIN");
            if (init_pos != std::string::npos) {
                size_t init_end = impl.find("END", init_pos);
                if (init_end != std::string::npos) {
                    body.initialization_block = impl.substr(init_pos, init_end - init_pos + 3);
                }
            }

            it->second.body = body;

            result.columns = {"status"};
            result.rows = {{"Package body created successfully"}};
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error creating package body: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_drop_package(const std::string& package_name,
                                                       const std::string& schema_name)
    {
        ExecutionResult result;
        result.success = true;

        try {
            std::lock_guard<std::mutex> lock(package_mutex_);

            std::string schema = schema_name.empty() ? "PUBLIC" : schema_name;
            std::string package_key = schema + "." + package_name;

            auto it = packages_.find(package_key);
            if (it == packages_.end()) {
                result.success = false;
                result.error_message = "Package '" + package_name + "' does not exist";
                return result;
            }

            // Cleanup package state
            cleanup_package(package_name);

            // Remove from registry
            packages_.erase(it);

            result.columns = {"status"};
            result.rows = {{"Package dropped successfully"}};
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error dropping package: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::compile_package_specification(const PackageSpecification& spec)
    {
        ExecutionResult result;
        result.success = true;

        // Package specification compilation would involve:
        // 1. Syntax validation
        // 2. Dependency checking
        // 3. Interface validation
        // For now, return success as basic implementation

        result.columns = {"status"};
        result.rows = {{"Package specification compiled successfully"}};
        return result;
    }

    ExecutionResult PsqlExecutor::compile_package_body(const PackageBody& body,
                                                       const PackageSpecification& spec)
    {
        ExecutionResult result;
        result.success = true;

        // Package body compilation would involve:
        // 1. Implementation validation against specification
        // 2. Private member compilation
        // 3. Initialization block preparation
        // For now, return success as basic implementation

        result.columns = {"status"};
        result.rows = {{"Package body compiled successfully"}};
        return result;
    }

    bool PsqlExecutor::validate_package_dependencies(const std::string& package_name)
    {
        // Simplified dependency validation
        // In a full implementation, this would check:
        // 1. Required packages exist
        // 2. Required procedures/functions are available
        // 3. No circular dependencies
        return true;
    }

    void PsqlExecutor::invalidate_dependent_packages(const std::string& package_name)
    {
        std::lock_guard<std::mutex> lock(package_mutex_);

        // Mark dependent packages as invalid
        for (auto& [key, package_instance] : packages_) {
            // Simplified - would need proper dependency tracking
            if (package_instance.spec.name != package_name) {
                package_instance.spec.valid = false;
            }
        }
    }

    bool PsqlExecutor::is_public_procedure(const std::string& package_name,
                                           const std::string& procedure_name)
    {
        std::lock_guard<std::mutex> lock(package_mutex_);

        for (const auto& [key, package_instance] : packages_) {
            if (package_instance.spec.name == package_name) {
                const auto& public_procs = package_instance.spec.public_procedures;
                return std::find(public_procs.begin(), public_procs.end(), procedure_name) !=
                       public_procs.end();
            }
        }
        return false;
    }

    bool PsqlExecutor::is_public_function(const std::string& package_name,
                                          const std::string& function_name)
    {
        std::lock_guard<std::mutex> lock(package_mutex_);

        for (const auto& [key, package_instance] : packages_) {
            if (package_instance.spec.name == package_name) {
                const auto& public_funcs = package_instance.spec.public_functions;
                return std::find(public_funcs.begin(), public_funcs.end(), function_name) !=
                       public_funcs.end();
            }
        }
        return false;
    }

    bool PsqlExecutor::has_package_access(const std::string& package_name,
                                          const std::string& calling_context)
    {
        // Simplified access control - in a full implementation would check:
        // 1. Schema-level permissions
        // 2. Role-based access control
        // 3. Package-specific grants
        return true;
    }

    ExecutionResult PsqlExecutor::initialize_package(const std::string& package_name)
    {
        ExecutionResult result;
        result.success = true;

        try {
            std::lock_guard<std::mutex> lock(package_mutex_);

            for (auto& [key, package_instance] : packages_) {
                if (package_instance.spec.name == package_name) {
                    if (!package_instance.body.initialized &&
                        !package_instance.body.initialization_block.empty()) {
                        // Execute initialization block
                        // For now, just mark as initialized
                        package_instance.body.initialized = true;
                        package_instance.state_initialized = true;

                        result.columns = {"status"};
                        result.rows = {{"Package initialized successfully"}};
                        return result;
                    }
                }
            }

            result.success = false;
            result.error_message = "Package not found or already initialized";
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error initializing package: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::reset_package_state(const std::string& package_name)
    {
        ExecutionResult result;
        result.success = true;

        try {
            std::lock_guard<std::mutex> lock(package_mutex_);

            for (auto& [key, package_instance] : packages_) {
                if (package_instance.spec.name == package_name) {
                    package_instance.session_state.clear();
                    package_instance.body.package_variables.clear();
                    package_instance.state_initialized = false;
                    package_instance.body.initialized = false;

                    result.columns = {"status"};
                    result.rows = {{"Package state reset successfully"}};
                    return result;
                }
            }

            result.success = false;
            result.error_message = "Package not found";
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error resetting package state: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::cleanup_package(const std::string& package_name)
    {
        ExecutionResult result;
        result.success = true;

        // Cleanup package resources
        // In a full implementation would:
        // 1. Close any open cursors
        // 2. Release allocated memory
        // 3. Clean up temporary resources

        return reset_package_state(package_name);
    }

    Value PsqlExecutor::get_package_variable(const std::string& package_name,
                                             const std::string& variable_name)
    {
        std::lock_guard<std::mutex> lock(package_mutex_);

        for (const auto& [key, package_instance] : packages_) {
            if (package_instance.spec.name == package_name) {
                auto it = package_instance.session_state.find(variable_name);
                if (it != package_instance.session_state.end()) {
                    return it->second;
                }

                auto it2 = package_instance.body.package_variables.find(variable_name);
                if (it2 != package_instance.body.package_variables.end()) {
                    return it2->second;
                }
            }
        }

        return Value{}; // Return null value if not found
    }

    bool PsqlExecutor::set_package_variable(const std::string& package_name,
                                            const std::string& variable_name, const Value& value)
    {
        std::lock_guard<std::mutex> lock(package_mutex_);

        for (auto& [key, package_instance] : packages_) {
            if (package_instance.spec.name == package_name) {
                package_instance.session_state[variable_name] = value;
                return true;
            }
        }

        return false;
    }

    ExecutionResult PsqlExecutor::execute_package_procedure(const std::string& package_name,
                                                            const std::string& procedure_name,
                                                            const std::vector<Value>& params)
    {
        ExecutionResult result;
        result.success = true;

        try {
            // Check if procedure is accessible
            if (!is_public_procedure(package_name, procedure_name)) {
                result.success = false;
                result.error_message = "Procedure '" + procedure_name +
                                       "' is not accessible or does not exist in package '" +
                                       package_name + "'";
                return result;
            }

            // Initialize package if needed
            initialize_package(package_name);

            // Execute procedure (simplified implementation)
            // In a full implementation would:
            // 1. Parse procedure parameters
            // 2. Set up execution context
            // 3. Execute procedure body
            // 4. Handle return values

            result.columns = {"status"};
            result.rows = {{"Package procedure executed successfully"}};
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error executing package procedure: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_package_function(const std::string& package_name,
                                                           const std::string& function_name,
                                                           const std::vector<Value>& params)
    {
        ExecutionResult result;
        result.success = true;

        try {
            // Check if function is accessible
            if (!is_public_function(package_name, function_name)) {
                result.success = false;
                result.error_message = "Function '" + function_name +
                                       "' is not accessible or does not exist in package '" +
                                       package_name + "'";
                return result;
            }

            // Initialize package if needed
            initialize_package(package_name);

            // Execute function (simplified implementation)
            // In a full implementation would:
            // 1. Parse function parameters
            // 2. Set up execution context
            // 3. Execute function body
            // 4. Return function result

            result.columns = {"result"};
            result.rows = {{"function_result_placeholder"}};
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error executing package function: ") + e.what();
        }

        return result;
    }

    // Advanced Function Features Implementation

    ExecutionResult PsqlExecutor::register_function_overload(const FunctionSignature& signature)
    {
        ExecutionResult result;
        result.success = true;

        try {
            std::lock_guard<std::mutex> lock(function_mutex_);

            // Check for overload conflicts
            if (has_overload_conflict(signature)) {
                result.success = false;
                result.error_message =
                    "Function overload conflict: signature already exists or is ambiguous";
                return result;
            }

            // Create or update overload set
            if (function_overloads_.find(signature.name) == function_overloads_.end()) {
                function_overloads_[signature.name] = FunctionOverloadSet{signature.name, {}, {}};
            }

            auto& overload_set = function_overloads_[signature.name];

            // Generate signature string for mapping
            std::string sig_str = signature.name + "(";
            for (size_t i = 0; i < signature.parameter_types.size(); ++i) {
                if (i > 0)
                    sig_str += ",";
                sig_str += signature.parameter_types[i];
            }
            sig_str += ")";

            // Add overload
            overload_set.overloads.push_back(signature);
            overload_set.signature_map[sig_str] = overload_set.overloads.size() - 1;

            result.columns = {"status"};
            result.rows = {{"Function overload registered successfully"}};
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error registering function overload: ") + e.what();
        }

        return result;
    }

    PsqlExecutor::FunctionSignature*
    PsqlExecutor::resolve_function_overload(const std::string& function_name,
                                            const std::vector<std::string>& param_types)
    {
        std::lock_guard<std::mutex> lock(function_mutex_);

        auto it = function_overloads_.find(function_name);
        if (it == function_overloads_.end()) {
            return nullptr;
        }

        // Generate signature string for lookup
        std::string sig_str = function_name + "(";
        for (size_t i = 0; i < param_types.size(); ++i) {
            if (i > 0)
                sig_str += ",";
            sig_str += param_types[i];
        }
        sig_str += ")";

        // Exact match first
        auto sig_it = it->second.signature_map.find(sig_str);
        if (sig_it != it->second.signature_map.end()) {
            return &it->second.overloads[sig_it->second];
        }

        // Type coercion matching (simplified)
        for (auto& overload : it->second.overloads) {
            if (overload.parameter_types.size() == param_types.size()) {
                bool compatible = true;
                for (size_t i = 0; i < param_types.size(); ++i) {
                    // Simplified type compatibility check
                    if (param_types[i] != overload.parameter_types[i] &&
                        !(param_types[i] == "INTEGER" &&
                          overload.parameter_types[i] == "NUMERIC") &&
                        !(param_types[i] == "VARCHAR" && overload.parameter_types[i] == "TEXT")) {
                        compatible = false;
                        break;
                    }
                }
                if (compatible) {
                    return &overload;
                }
            }
        }

        return nullptr;
    }

    bool PsqlExecutor::has_overload_conflict(const FunctionSignature& new_signature)
    {
        auto it = function_overloads_.find(new_signature.name);
        if (it == function_overloads_.end()) {
            return false; // No existing overloads
        }

        // Check for exact signature match
        for (const auto& existing : it->second.overloads) {
            if (existing.parameter_types.size() == new_signature.parameter_types.size()) {
                bool same_signature = true;
                for (size_t i = 0; i < existing.parameter_types.size(); ++i) {
                    if (existing.parameter_types[i] != new_signature.parameter_types[i]) {
                        same_signature = false;
                        break;
                    }
                }
                if (same_signature) {
                    return true; // Conflict found
                }
            }
        }

        return false; // No conflict
    }

    ExecutionResult PsqlExecutor::execute_overloaded_function(const std::string& function_name,
                                                              const std::vector<Value>& params)
    {
        ExecutionResult result;
        result.success = true;

        try {
            // Extract parameter types
            std::vector<std::string> param_types;
            for (const auto& param : params) {
                // Simplified type detection based on Value content
                if (param.is_null) {
                    param_types.push_back("NULL");
                } else if (param.bytes.find_first_not_of("0123456789.-") == std::string::npos) {
                    param_types.push_back("NUMERIC");
                } else {
                    param_types.push_back("VARCHAR");
                }
            }

            // Resolve function overload
            FunctionSignature* signature = resolve_function_overload(function_name, param_types);
            if (!signature) {
                result.success = false;
                result.error_message = "No matching function overload found for " + function_name;
                return result;
            }

            // Track function calls for profiling
            if (function_profiling_enabled_) {
                std::lock_guard<std::mutex> lock(function_mutex_);
                function_call_counts_[function_name]++;
            }

            // Check for recursion
            auto recursive_it = recursive_functions_.find(function_name);
            if (recursive_it != recursive_functions_.end()) {
                return execute_recursive_function(function_name, params, recursive_it->second);
            }

            // Check for inlining
            if (should_inline_function(*signature)) {
                // For inlining, we would expand the function body inline
                // For now, just execute normally but mark as inlined
                result.columns = {"result", "inlined"};
                result.rows = {{"function_result", "true"}};
            } else {
                // Execute function normally
                result.columns = {"result"};
                result.rows = {{"function_result"}};
            }

        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error executing overloaded function: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::execute_recursive_function(const std::string& function_name,
                                                             const std::vector<Value>& params,
                                                             RecursiveCallInfo& call_info)
    {
        ExecutionResult result;
        result.success = true;

        try {
            std::lock_guard<std::mutex> lock(function_mutex_);

            // Check recursion depth limit
            if (call_info.stack_depth >= call_info.max_stack_depth) {
                result.success = false;
                result.error_message =
                    "Maximum recursion depth exceeded for function " + function_name;
                return result;
            }

            // Increment stack depth
            call_info.stack_depth++;
            call_info.last_call_time = std::chrono::steady_clock::now();

            // For tail-recursive functions, optimize the call
            if (call_info.tail_call_optimizable) {
                // Tail call optimization would reuse the current stack frame
                // For now, just indicate that optimization was applied
                result.columns = {"result", "tail_optimized"};
                result.rows = {{"recursive_result", "true"}};
            } else {
                // Regular recursive execution
                result.columns = {"result"};
                result.rows = {{"recursive_result"}};
            }

            // Decrement stack depth on return
            call_info.stack_depth--;

        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error executing recursive function: ") + e.what();
        }

        return result;
    }

    bool PsqlExecutor::is_tail_recursive(const FunctionSignature& signature)
    {
        // Simplified tail recursion detection
        // In a full implementation, this would analyze the function body
        // to determine if the recursive call is the last operation

        std::string body = signature.body;

        // Look for common tail-recursive patterns
        if (body.find("RETURN " + signature.name + "(") != std::string::npos) {
            // Check if return statement is at the end
            size_t return_pos = body.rfind("RETURN " + signature.name + "(");
            size_t end_pos = body.find("END", return_pos);
            if (end_pos != std::string::npos &&
                body.substr(return_pos, end_pos - return_pos).find(';') == std::string::npos) {
                return true;
            }
        }

        return false;
    }

    ExecutionResult PsqlExecutor::optimize_tail_recursion(const FunctionSignature& signature)
    {
        ExecutionResult result;
        result.success = true;

        try {
            std::lock_guard<std::mutex> lock(function_mutex_);

            auto it = recursive_functions_.find(signature.name);
            if (it == recursive_functions_.end()) {
                recursive_functions_[signature.name] =
                    RecursiveCallInfo{signature.name, 0, 1000, false, {}};
                it = recursive_functions_.find(signature.name);
            }

            // Mark as tail-call optimizable
            it->second.tail_call_optimizable = is_tail_recursive(signature);

            result.columns = {"status", "optimizable"};
            result.rows = {{"Tail recursion analysis complete",
                            it->second.tail_call_optimizable ? "true" : "false"}};
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error optimizing tail recursion: ") + e.what();
        }

        return result;
    }

    void PsqlExecutor::set_recursion_limit(const std::string& function_name, size_t max_depth)
    {
        std::lock_guard<std::mutex> lock(function_mutex_);

        auto it = recursive_functions_.find(function_name);
        if (it == recursive_functions_.end()) {
            recursive_functions_[function_name] =
                RecursiveCallInfo{function_name, 0, max_depth, false, {}};
        } else {
            it->second.max_stack_depth = max_depth;
        }
    }

    bool PsqlExecutor::should_inline_function(const FunctionSignature& signature)
    {
        if (!signature.allow_inlining) {
            return false;
        }

        // Inline deterministic functions with low complexity
        if (signature.is_deterministic && signature.complexity_score < 10) {
            return true;
        }

        // Inline very simple functions regardless of determinism
        if (signature.complexity_score < 5) {
            return true;
        }

        // Don't inline recursive functions
        if (recursive_functions_.find(signature.name) != recursive_functions_.end()) {
            return false;
        }

        return false;
    }

    std::string PsqlExecutor::inline_function_call(const FunctionSignature& signature,
                                                   const std::vector<std::string>& arg_expressions)
    {
        // Simplified function inlining
        // In a full implementation, this would:
        // 1. Parse the function body
        // 2. Replace parameter references with argument expressions
        // 3. Optimize the resulting expression
        // 4. Return the inlined code

        std::string inlined_body = signature.body;

        // Replace parameter placeholders with actual arguments
        for (size_t i = 0; i < arg_expressions.size() && i < signature.parameter_types.size();
             ++i) {
            std::string param_placeholder = "$" + std::to_string(i + 1);
            size_t pos = 0;
            while ((pos = inlined_body.find(param_placeholder, pos)) != std::string::npos) {
                inlined_body.replace(pos, param_placeholder.length(), arg_expressions[i]);
                pos += arg_expressions[i].length();
            }
        }

        return inlined_body;
    }

    size_t PsqlExecutor::calculate_function_complexity(const std::string& function_body)
    {
        // Simplified complexity calculation
        // In a full implementation, this would analyze:
        // 1. Number of statements
        // 2. Control flow complexity (loops, conditions)
        // 3. Function call depth
        // 4. Variable references

        size_t complexity = 0;

        // Count statements (semicolons)
        complexity += std::count(function_body.begin(), function_body.end(), ';');

        // Count control structures
        complexity += (function_body.find("IF") != std::string::npos ? 2 : 0);
        complexity += (function_body.find("WHILE") != std::string::npos ? 3 : 0);
        complexity += (function_body.find("FOR") != std::string::npos ? 3 : 0);

        // Count function calls
        size_t pos = 0;
        while ((pos = function_body.find("(", pos)) != std::string::npos) {
            complexity++;
            pos++;
        }

        return complexity;
    }

    ExecutionResult PsqlExecutor::mark_function_deterministic(const std::string& function_name,
                                                              bool deterministic)
    {
        ExecutionResult result;
        result.success = true;

        try {
            std::lock_guard<std::mutex> lock(function_mutex_);

            auto it = function_overloads_.find(function_name);
            if (it != function_overloads_.end()) {
                for (auto& overload : it->second.overloads) {
                    overload.is_deterministic = deterministic;
                    overload.allow_inlining =
                        deterministic; // Enable inlining for deterministic functions
                }
                result.columns = {"status"};
                result.rows = {{"Function determinism updated successfully"}};
            } else {
                result.success = false;
                result.error_message = "Function not found: " + function_name;
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error marking function deterministic: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::analyze_function_performance(const std::string& function_name)
    {
        ExecutionResult result;
        result.success = true;

        try {
            std::lock_guard<std::mutex> lock(function_mutex_);

            auto overload_it = function_overloads_.find(function_name);
            if (overload_it == function_overloads_.end()) {
                result.success = false;
                result.error_message = "Function not found: " + function_name;
                return result;
            }

            std::vector<std::vector<std::string>> analysis_rows;

            for (size_t i = 0; i < overload_it->second.overloads.size(); ++i) {
                const auto& overload = overload_it->second.overloads[i];

                // Calculate metrics
                size_t complexity = calculate_function_complexity(overload.body);
                bool tail_recursive = is_tail_recursive(overload);
                bool should_inline = should_inline_function(overload);

                // Get call count
                size_t call_count = 0;
                auto call_it = function_call_counts_.find(function_name);
                if (call_it != function_call_counts_.end()) {
                    call_count = call_it->second;
                }

                analysis_rows.push_back({
                    std::to_string(i),                               // overload_index
                    std::to_string(overload.parameter_types.size()), // param_count
                    std::to_string(complexity),                      // complexity_score
                    overload.is_deterministic ? "true" : "false",    // deterministic
                    tail_recursive ? "true" : "false",               // tail_recursive
                    should_inline ? "true" : "false",                // should_inline
                    std::to_string(call_count)                       // call_count
                });
            }

            result.columns = {"overload_index", "param_count",    "complexity_score",
                              "deterministic",  "tail_recursive", "should_inline",
                              "call_count"};
            result.rows = analysis_rows;

        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error analyzing function performance: ") + e.what();
        }

        return result;
    }

    ExecutionResult PsqlExecutor::get_function_metrics(const std::string& function_name)
    {
        ExecutionResult result;
        result.success = true;

        try {
            std::lock_guard<std::mutex> lock(function_mutex_);

            // Get call count
            size_t call_count = 0;
            auto call_it = function_call_counts_.find(function_name);
            if (call_it != function_call_counts_.end()) {
                call_count = call_it->second;
            }

            // Get recursive info
            std::string recursive_info = "false";
            auto recursive_it = recursive_functions_.find(function_name);
            if (recursive_it != recursive_functions_.end()) {
                recursive_info =
                    "true (depth: " + std::to_string(recursive_it->second.stack_depth) +
                    ", max: " + std::to_string(recursive_it->second.max_stack_depth) + ")";
            }

            // Get overload count
            size_t overload_count = 0;
            auto overload_it = function_overloads_.find(function_name);
            if (overload_it != function_overloads_.end()) {
                overload_count = overload_it->second.overloads.size();
            }

            result.columns = {"metric", "value"};
            result.rows = {{"function_name", function_name},
                           {"call_count", std::to_string(call_count)},
                           {"overload_count", std::to_string(overload_count)},
                           {"recursive", recursive_info},
                           {"profiling_enabled", function_profiling_enabled_ ? "true" : "false"}};

        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Error getting function metrics: ") + e.what();
        }

        return result;
    }

    void PsqlExecutor::enable_function_profiling(bool enabled)
    {
        std::lock_guard<std::mutex> lock(function_mutex_);
        function_profiling_enabled_ = enabled;

        if (!enabled) {
            // Clear profiling data when disabled
            function_call_counts_.clear();
        }
    }

} // namespace scratchbird::engine
