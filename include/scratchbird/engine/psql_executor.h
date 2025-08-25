#ifndef SCRATCHBIRD_ENGINE_PSQL_EXECUTOR_H
#define SCRATCHBIRD_ENGINE_PSQL_EXECUTOR_H

#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/heap.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace scratchbird::engine
{
    // Forward declarations
    struct ExecutionResult;

    // PSQL variable type information
    struct PsqlVariableType {
        std::string type_name;   // INT, VARCHAR, etc.
        int size{0};             // For VARCHAR(n), etc.
        bool nullable{true};     // NULL allowed
        bool array{false};       // Array type
        int array_dimensions{0}; // Number of array dimensions
    };

    // PSQL variable storage
    struct PsqlVariable {
        std::string name;
        PsqlVariableType type;
        Value value;              // Current value
        bool initialized{false};  // Has been assigned
        bool is_parameter{false}; // Is a procedure/function parameter
        std::string param_mode;   // IN, OUT, INOUT for parameters
    };

    // PSQL cursor state
    // Cursor navigation directions for scrollable cursors
    enum class CursorDirection {
        NEXT,     // Default forward direction
        PRIOR,    // Move backward one row
        FIRST,    // Move to first row
        LAST,     // Move to last row
        ABSOLUTE, // Move to absolute position
        RELATIVE  // Move relative to current position
    };

    // Cursor scroll type
    enum class CursorScrollType {
        NO_SCROLL, // Forward-only cursor (default)
        SCROLL     // Bidirectional scrollable cursor
    };

    struct PsqlCursor {
        std::string name;
        std::string query;             // SQL query for the cursor
        bool is_open{false};           // Cursor state
        size_t current_row{0};         // Current row position (0-based)
        ExecutionResult cached_result; // Cached query results
        bool has_data{false};          // Has more data to fetch

        // Advanced cursor features
        CursorScrollType scroll_type{CursorScrollType::NO_SCROLL};
        bool is_scrollable() const
        {
            return scroll_type == CursorScrollType::SCROLL;
        }

        // Cursor attributes (Firebird/Oracle compatible)
        bool found{false};    // %FOUND - true if last fetch returned a row
        bool not_found{true}; // %NOTFOUND - opposite of %FOUND
        size_t row_count{0};  // %ROWCOUNT - number of rows fetched so far

        // Scrollable cursor state
        bool at_beginning{true}; // At beginning of result set
        bool at_end{false};      // At end of result set
        size_t total_rows{0};    // Total rows in result set (for scrollable cursors)

        // Bulk operation support
        std::vector<std::vector<std::string>>
            bulk_buffer; // Buffer for bulk collect operations (same format as ExecutionResult.rows)
        size_t bulk_limit{100}; // Default bulk collect limit

        // Update cursor attributes after fetch operation
        void update_attributes(bool fetch_successful, size_t rows_fetched = 1)
        {
            found = fetch_successful;
            not_found = !fetch_successful;
            if (fetch_successful) {
                row_count += rows_fetched;
                at_beginning = (current_row == 0);
                at_end = (current_row >= total_rows - 1);
            } else {
                at_end = true;
            }
        }

        // Reset cursor to initial state
        void reset()
        {
            current_row = 0;
            row_count = 0;
            found = false;
            not_found = true;
            at_beginning = true;
            at_end = false;
            bulk_buffer.clear();
        }
    };

    // PSQL execution scope for variable management
    struct PsqlScope {
        std::unordered_map<std::string, PsqlVariable> variables;
        std::unordered_map<std::string, PsqlCursor> cursors;
        std::shared_ptr<PsqlScope> parent_scope; // For nested blocks

        // Scope management
        bool has_variable(const std::string& name) const;
        PsqlVariable* get_variable(const std::string& name);
        const PsqlVariable* get_variable(const std::string& name) const;
        void declare_variable(const std::string& name, const PsqlVariableType& type,
                              const Value& default_value = Value{});
        bool assign_variable(const std::string& name, const Value& value);

        // Cursor management
        bool has_cursor(const std::string& name) const;
        PsqlCursor* get_cursor(const std::string& name);
        const PsqlCursor* get_cursor(const std::string& name) const;
        void declare_cursor(const std::string& name, const std::string& query);

        // Advanced cursor management
        void declare_scrollable_cursor(const std::string& name, const std::string& query,
                                       CursorScrollType scroll_type = CursorScrollType::SCROLL);
        void set_cursor_bulk_limit(const std::string& name, size_t limit);
    };

    // PSQL execution context
    class PsqlExecutionContext
    {
      public:
        PsqlExecutionContext();
        ~PsqlExecutionContext();

        // Scope management
        void push_scope(); // Enter new block scope
        void pop_scope();  // Exit current block scope
        PsqlScope& current_scope();
        const PsqlScope& current_scope() const;

        // Variable operations
        void declare_variable(const std::string& name, const PsqlVariableType& type,
                              const Value& default_value = Value{});
        bool assign_variable(const std::string& name, const Value& value);
        Value get_variable_value(const std::string& name) const;
        bool has_variable(const std::string& name) const;

        // Parameter management
        void bind_parameter(const std::string& name, const std::string& mode,
                            const PsqlVariableType& type, const Value& value);
        std::vector<Value> get_output_parameters() const;

        // Cursor management
        void declare_cursor(const std::string& name, const std::string& query);
        bool has_cursor(const std::string& name) const;
        PsqlCursor* get_cursor(const std::string& name);

        // Security context management
        void set_security_context(const std::string& security_type,
                                  const std::string& owner_role = "");
        void restore_security_context();
        std::string get_current_security_context() const;
        bool has_definer_rights() const;

        // Control flow state
        enum class ControlFlowState { Normal, Break, Continue, Return, Exception };

        ControlFlowState control_state{ControlFlowState::Normal};
        Value return_value;

        // Exception state
        std::string exception_name;
        std::string exception_message;
        bool has_active_exception() const
        {
            return !exception_name.empty();
        }
        void clear_exception()
        {
            exception_name.clear();
            exception_message.clear();
        }
        void set_exception(const std::string& name, const std::string& message);

        // System exception definitions (Firebird-compatible)
        static std::unordered_map<std::string, int> get_system_exceptions();

        // Execution statistics
        size_t statements_executed{0};
        size_t variables_declared{0};

      private:
        std::stack<std::shared_ptr<PsqlScope>> scope_stack_;

        // Security context state
        std::string current_security_type_{"INVOKER"}; // Default to INVOKER
        std::string current_owner_role_;
        std::string original_security_type_{"INVOKER"};
        std::string original_owner_role_;
    };

    // PSQL type parser and validator
    class PsqlTypeManager
    {
      public:
        // Parse type from PSQL declaration string (e.g., "VARCHAR(50)", "INTEGER")
        static PsqlVariableType parse_type(const std::string& type_str);

        // Validate value assignment to type
        static bool validate_assignment(const PsqlVariableType& type, const Value& value);

        // Convert value to match type requirements
        static Value coerce_value(const PsqlVariableType& type, const Value& value);

        // Get default value for type
        static Value get_default_value(const PsqlVariableType& type);
    };

    // Main PSQL executor
    class PsqlExecutor
    {
      public:
        explicit PsqlExecutor(const std::string& db_path = "");

        // Execute PSQL block
        ExecutionResult execute_block(const decltype(Ast{}.psqlBlock)& block);

        // Execute stored procedure/function call
        ExecutionResult execute_call(const decltype(Ast{}.psqlCall)& call);

        // Execute individual PSQL statement
        ExecutionResult execute_statement(const Ast::PsqlStmt& stmt, PsqlExecutionContext& context);

        // Control flow execution
        ExecutionResult execute_if_statement(const Ast::PsqlStmt& stmt,
                                             PsqlExecutionContext& context);
        ExecutionResult execute_while_loop(const Ast::PsqlStmt& stmt,
                                           PsqlExecutionContext& context);
        ExecutionResult execute_for_loop(const Ast::PsqlStmt& stmt, PsqlExecutionContext& context);

        // Variable operations
        ExecutionResult execute_declare(const Ast::PsqlStmt& stmt, PsqlExecutionContext& context);
        ExecutionResult execute_assignment(const Ast::PsqlStmt& stmt,
                                           PsqlExecutionContext& context);

        // SQL execution within PSQL context
        ExecutionResult execute_sql_statement(const std::string& sql,
                                              PsqlExecutionContext& context);

        // Exception handling
        ExecutionResult execute_raise_statement(const Ast::PsqlStmt& stmt,
                                                PsqlExecutionContext& context);
        ExecutionResult execute_exception_handler(const Ast::PsqlStmt& stmt,
                                                  PsqlExecutionContext& context);

        // Cursor operations
        ExecutionResult execute_open_cursor(const Ast::PsqlStmt& stmt,
                                            PsqlExecutionContext& context);
        ExecutionResult execute_fetch_cursor(const Ast::PsqlStmt& stmt,
                                             PsqlExecutionContext& context);
        ExecutionResult execute_close_cursor(const Ast::PsqlStmt& stmt,
                                             PsqlExecutionContext& context);

        // Advanced cursor operations
        ExecutionResult execute_declare_cursor(const Ast::PsqlStmt& stmt,
                                               PsqlExecutionContext& context);
        ExecutionResult execute_fetch_bulk_collect(const std::string& cursor_name,
                                                   const std::vector<std::string>& target_vars,
                                                   PsqlExecutionContext& context, size_t limit = 0);
        ExecutionResult execute_cursor_for_loop(const Ast::PsqlStmt& stmt,
                                                PsqlExecutionContext& context);

        // Scrollable cursor navigation
        ExecutionResult fetch_cursor_direction(PsqlCursor* cursor, CursorDirection direction,
                                               int offset, PsqlExecutionContext& context);
        ExecutionResult fetch_cursor_absolute(PsqlCursor* cursor, size_t position,
                                              PsqlExecutionContext& context);
        ExecutionResult fetch_cursor_relative(PsqlCursor* cursor, int offset,
                                              PsqlExecutionContext& context);

        // Advanced control flow
        ExecutionResult execute_leave_statement(const Ast::PsqlStmt& stmt,
                                                PsqlExecutionContext& context);
        ExecutionResult execute_continue_statement(const Ast::PsqlStmt& stmt,
                                                   PsqlExecutionContext& context);

        // Expression evaluation in PSQL context
        Value evaluate_expression(const std::string& expr, const PsqlExecutionContext& context);

        // Performance optimization methods
        struct CompiledProcedure {
            std::string name;
            std::string schema_name;
            std::string compiled_body; // Optimized PSQL body
            std::chrono::steady_clock::time_point compiled_time;
            size_t execution_count{0};
            bool is_deterministic{false};
            std::unordered_map<std::string, Value> constant_values; // Constant folding results
        };

        // Enable procedure plan caching
        void enable_plan_caching(bool enabled = true);

        // Get compiled procedure from cache
        std::optional<CompiledProcedure> get_cached_procedure(const std::string& name) const;

        // Cache compiled procedure
        void cache_procedure(const std::string& name, const CompiledProcedure& compiled);

        // Clear procedure cache
        void clear_procedure_cache();

        // Expression optimization
        std::string optimize_expression(const std::string& expr,
                                        const PsqlExecutionContext& context);

        // Function inlining for deterministic functions
        std::string inline_deterministic_functions(const std::string& code);

        // Advanced Performance Optimizations
        std::string eliminate_dead_code(const std::string& code);
        std::string optimize_constant_expressions(const std::string& code);
        std::string optimize_algebraic_expressions(const std::string& code);
        std::string optimize_loop_invariants(const std::string& code);

        // PSQL Debugging Support
        struct DebugBreakpoint {
            std::string procedure_name;
            int line_number{0};
            bool enabled{true};
            std::string condition; // Optional conditional breakpoint
        };

        struct DebugCallFrame {
            std::string procedure_name;
            std::string source_code;
            int current_line{0};
            std::unordered_map<std::string, Value> local_variables;
            std::chrono::steady_clock::time_point start_time;
        };

        struct DebugState {
            bool debugging_enabled{false};
            bool step_mode{false};
            bool break_on_exception{true};
            std::vector<DebugBreakpoint> breakpoints;
            std::vector<DebugCallFrame> call_stack;
            std::string last_error;
            int last_error_line{0};
        };

        // Enable/disable debugging
        void enable_debugging(bool enabled = true);

        // Breakpoint management
        void add_breakpoint(const std::string& procedure_name, int line_number,
                            const std::string& condition = "");
        void remove_breakpoint(const std::string& procedure_name, int line_number);
        void clear_breakpoints();
        std::vector<DebugBreakpoint> get_breakpoints() const;

        // Execution control
        void enable_step_mode(bool enabled = true);
        void step_over();
        void step_into();
        void continue_execution();

        // Variable inspection
        std::unordered_map<std::string, Value> get_current_variables() const;
        Value get_variable_value(const std::string& name) const;

        // Call stack inspection
        std::vector<DebugCallFrame> get_call_stack() const;
        std::string get_current_procedure() const;
        int get_current_line() const;

        // Error reporting
        std::string get_last_error_with_location() const;
        void report_runtime_error(const std::string& error, int line_number = 0);

        // Enhanced Package Support structures
        struct PackageSpecification {
            std::string name;
            std::string schema_name;
            std::vector<std::string> public_procedures;
            std::vector<std::string> public_functions;
            std::unordered_map<std::string, std::string> public_signatures; // name -> signature
            std::chrono::steady_clock::time_point created_time;
            std::string version;
            bool valid{true};
        };

        struct PackageBody {
            std::string name;
            std::string schema_name;
            std::string implementation_body;
            std::vector<std::string> private_procedures;
            std::vector<std::string> private_functions;
            std::unordered_map<std::string, std::string> private_signatures;
            std::unordered_map<std::string, Value> package_variables; // Package-level state
            std::string initialization_block;                         // Package initialization code
            std::chrono::steady_clock::time_point compiled_time;
            bool initialized{false};
        };

        struct PackageInstance {
            PackageSpecification spec;
            PackageBody body;
            std::unordered_map<std::string, Value> session_state; // Session-level package state
            bool state_initialized{false};
        };

        // Enhanced Package Support methods
        ExecutionResult execute_create_package(const decltype(Ast{}.psqlPackage)& package);
        ExecutionResult execute_create_package_body(const decltype(Ast{}.psqlPackage)& package);
        ExecutionResult execute_drop_package(const std::string& package_name,
                                             const std::string& schema_name = "");

        // Package compilation and validation
        ExecutionResult compile_package_specification(const PackageSpecification& spec);
        ExecutionResult compile_package_body(const PackageBody& body,
                                             const PackageSpecification& spec);
        bool validate_package_dependencies(const std::string& package_name);
        void invalidate_dependent_packages(const std::string& package_name);

        // Package visibility and access control
        bool is_public_procedure(const std::string& package_name,
                                 const std::string& procedure_name);
        bool is_public_function(const std::string& package_name, const std::string& function_name);
        bool has_package_access(const std::string& package_name,
                                const std::string& calling_context);

        // Package state management
        ExecutionResult initialize_package(const std::string& package_name);
        ExecutionResult reset_package_state(const std::string& package_name);
        ExecutionResult cleanup_package(const std::string& package_name);
        Value get_package_variable(const std::string& package_name,
                                   const std::string& variable_name);
        bool set_package_variable(const std::string& package_name, const std::string& variable_name,
                                  const Value& value);

        // Package execution context
        ExecutionResult execute_package_procedure(const std::string& package_name,
                                                  const std::string& procedure_name,
                                                  const std::vector<Value>& params);
        ExecutionResult execute_package_function(const std::string& package_name,
                                                 const std::string& function_name,
                                                 const std::vector<Value>& params);

        // Advanced Function Features
        struct FunctionSignature {
            std::string name;
            std::vector<std::string> parameter_types;
            std::string return_type;
            std::string body;
            bool is_deterministic{false};
            bool allow_inlining{false};
            size_t complexity_score{0}; // For inlining decisions
            std::chrono::steady_clock::time_point compiled_time;
        };

        struct FunctionOverloadSet {
            std::string function_name;
            std::vector<FunctionSignature> overloads;
            std::unordered_map<std::string, size_t> signature_map; // signature -> overload index
        };

        struct RecursiveCallInfo {
            std::string function_name;
            size_t stack_depth{0};
            size_t max_stack_depth{1000}; // Configurable limit
            bool tail_call_optimizable{false};
            std::chrono::steady_clock::time_point last_call_time;
        };

        // Function overloading support
        ExecutionResult register_function_overload(const FunctionSignature& signature);
        FunctionSignature* resolve_function_overload(const std::string& function_name,
                                                     const std::vector<std::string>& param_types);
        bool has_overload_conflict(const FunctionSignature& new_signature);
        ExecutionResult execute_overloaded_function(const std::string& function_name,
                                                    const std::vector<Value>& params);

        // Recursive function optimization
        ExecutionResult execute_recursive_function(const std::string& function_name,
                                                   const std::vector<Value>& params,
                                                   RecursiveCallInfo& call_info);
        bool is_tail_recursive(const FunctionSignature& signature);
        ExecutionResult optimize_tail_recursion(const FunctionSignature& signature);
        void set_recursion_limit(const std::string& function_name, size_t max_depth);

        // Function inlining support
        bool should_inline_function(const FunctionSignature& signature);
        std::string inline_function_call(const FunctionSignature& signature,
                                         const std::vector<std::string>& arg_expressions);
        size_t calculate_function_complexity(const std::string& function_body);
        ExecutionResult mark_function_deterministic(const std::string& function_name,
                                                    bool deterministic);

        // Function analysis and optimization
        ExecutionResult analyze_function_performance(const std::string& function_name);
        ExecutionResult get_function_metrics(const std::string& function_name);
        void enable_function_profiling(bool enabled = true);

      private:
        std::string db_path_;

        // Performance optimization state
        bool plan_caching_enabled_{true};
        std::unordered_map<std::string, CompiledProcedure> procedure_cache_;
        mutable std::mutex cache_mutex_; // Thread safety for cache access

        // Debugging state
        DebugState debug_state_;
        mutable std::mutex debug_mutex_; // Thread safety for debug operations

        // Package registry
        std::unordered_map<std::string, PackageInstance> packages_;
        mutable std::mutex package_mutex_; // Thread safety for package operations

        // Advanced function features registry
        std::unordered_map<std::string, FunctionOverloadSet> function_overloads_;
        std::unordered_map<std::string, RecursiveCallInfo> recursive_functions_;
        std::unordered_map<std::string, size_t> function_call_counts_;
        mutable std::mutex function_mutex_; // Thread safety for function operations
        bool function_profiling_enabled_{false};

        // Helper methods
        void process_declarations(const decltype(Ast{}.psqlBlock)& block,
                                  PsqlExecutionContext& context);
        void bind_parameters(const decltype(Ast{}.psqlBlock)& block,
                             const std::vector<Value>& params, PsqlExecutionContext& context);
        std::vector<Value> extract_return_values(const decltype(Ast{}.psqlBlock)& block,
                                                 const PsqlExecutionContext& context);
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_PSQL_EXECUTOR_H
