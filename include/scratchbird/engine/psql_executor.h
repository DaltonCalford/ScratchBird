#ifndef SCRATCHBIRD_ENGINE_PSQL_EXECUTOR_H
#define SCRATCHBIRD_ENGINE_PSQL_EXECUTOR_H

#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/heap.h"

#include <memory>
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

        // Expression evaluation in PSQL context
        Value evaluate_expression(const std::string& expr, const PsqlExecutionContext& context);

      private:
        std::string db_path_;

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
