#pragma once

#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/parser/token.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/types.h"
#include <vector>
#include <memory>
#include <variant>
#include <stack>

namespace scratchbird
{
    namespace sblr
    {

        // Value types that can be on the execution stack
        // Now using the unified type system
        using Value = core::TypedValue;

        // Result of executing a SELECT statement
        class ResultSet
        {
        public:
            ResultSet() = default;

            // Column information
            void addColumn(const std::string &name, core::DataType type);
            size_t columnCount() const
            {
                return column_names_.size();
            }
            const std::string &columnName(size_t idx) const
            {
                return column_names_[idx];
            }
            core::DataType columnType(size_t idx) const
            {
                return column_types_[idx];
            }

            // Row data
            void addRow(std::vector<Value> row);
            size_t rowCount() const
            {
                return rows_.size();
            }
            const Value &getValue(size_t row, size_t col) const
            {
                return rows_[row][col];
            }

            // Debug output
            void print(std::ostream &out) const;

        private:
            std::vector<std::string> column_names_;
            std::vector<core::DataType> column_types_;
            std::vector<std::vector<Value>> rows_;
        };

        // Execution result
        class ExecutionResult
        {
        public:
            enum ResultType
            {
                SUCCESS,
                ERROR,
                RESULT_SET
            };

            ExecutionResult() : type_(SUCCESS) {}
            ExecutionResult(const std::string &error) : type_(ERROR), error_(error) {}
            ExecutionResult(std::unique_ptr<ResultSet> results)
                : type_(RESULT_SET), result_set_(std::move(results))
            {
            }

            bool success() const
            {
                return type_ != ERROR;
            }
            bool hasResultSet() const
            {
                return type_ == RESULT_SET;
            }

            const std::string &error() const
            {
                return error_;
            }
            ResultSet *resultSet() const
            {
                return result_set_.get();
            }

            // For DDL statements, return number of affected objects
            void setAffectedCount(int count)
            {
                affected_count_ = count;
            }
            int affectedCount() const
            {
                return affected_count_;
            }

        private:
            ResultType type_;
            std::string error_;
            std::unique_ptr<ResultSet> result_set_;
            int affected_count_ = 0;
        };

        // SBLR Bytecode Executor
        // NOTE: The executor does not take ownership of the Database pointer.
        // The caller must ensure the Database remains valid for the executor's lifetime.
        class Executor
        {
        public:
            // Constructor takes a non-null Database pointer
            // The Database must remain valid for the lifetime of this Executor
            Executor(core::Database *db);
            ~Executor();

            // Execute bytecode
            ExecutionResult execute(const std::vector<uint8_t> &bytecode);

        private:
            core::Database *db_;

            // Execution state
            // Note: bytecode_ is a raw pointer that must remain valid during execute()
            const uint8_t *bytecode_;
            size_t bytecode_size_;
            size_t pc_; // Program counter
            std::stack<Value> stack_;

            // Current statement context
            std::string current_table_;
            std::vector<std::string> current_columns_;
            std::unique_ptr<ResultSet> current_result_set_;

            // Row context for expression evaluation (during SELECT WHERE)
            const std::vector<Value> *current_row_values_ = nullptr;
            const std::vector<core::CatalogManager::ColumnInfo> *current_row_columns_ = nullptr;

            // Execution helpers
            uint8_t readByte();
            uint32_t readInt32();
            uint64_t readInt64();
            double readDouble();
            std::string readString();

            void push(const Value &v)
            {
                stack_.push(v);
            }
            Value pop();

            // Statement execution
            void executeCreateTable();
            void executeInsert();
            void executeSelect();

            // Expression evaluation
            void evaluateExpression();
            void executeBinaryOp(Opcode op);

            // Error handling
            void error(const std::string &msg);

            // Tuple deserialization helper
            bool deserializeTuple(const uint8_t *tuple_data, uint32_t tuple_size,
                                   const std::vector<core::CatalogManager::ColumnInfo> &columns,
                                   std::vector<Value> &values_out);
        };

    } // namespace sblr
} // namespace scratchbird