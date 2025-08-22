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
    struct PsqlCursor {
        std::string name;
        std::string query;             // SQL query for the cursor
        bool is_open{false};           // Cursor state
        size_t current_row{0};         // Current row position
        ExecutionResult cached_result; // Cached query results
        bool has_data{false};          // Has more data to fetch
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

      private:
        std::string db_path_;

        // Performance optimization state
        bool plan_caching_enabled_{true};
        std::unordered_map<std::string, CompiledProcedure> procedure_cache_;
        mutable std::mutex cache_mutex_; // Thread safety for cache access

        // Debugging state
        DebugState debug_state_;
        mutable std::mutex debug_mutex_; // Thread safety for debug operations

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
