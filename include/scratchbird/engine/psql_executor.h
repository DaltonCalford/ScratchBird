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

    // PSQL execution scope for variable management
    struct PsqlScope {
        std::unordered_map<std::string, PsqlVariable> variables;
        std::shared_ptr<PsqlScope> parent_scope; // For nested blocks

        // Scope management
        bool has_variable(const std::string& name) const;
        PsqlVariable* get_variable(const std::string& name);
        const PsqlVariable* get_variable(const std::string& name) const;
        void declare_variable(const std::string& name, const PsqlVariableType& type,
                              const Value& default_value = Value{});
        bool assign_variable(const std::string& name, const Value& value);
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

        // Control flow state
        enum class ControlFlowState { Normal, Break, Continue, Return, Exception };

        ControlFlowState control_state{ControlFlowState::Normal};
        Value return_value;

        // Exception state
        std::string exception_name;
        std::string exception_message;

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
