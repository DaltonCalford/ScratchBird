#pragma once

#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/parser/token.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/charset.h"
#include "scratchbird/core/timezone.h"
#include "scratchbird/core/types.h"
#include <vector>
#include <memory>
#include <variant>
#include <stack>
#include <unordered_set>
#include <unordered_map>

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
            core::CharsetManager charset_manager_;
            core::TimezoneManager timezone_manager_;

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
            uint16_t readInt16();
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
            void executeCreateIndex();             // Phase 2 Task 2.3
            void executeCreateTablespace();        // Phase 2 Task 2.1
            void executeAlterTablespace();         // Phase 2 Task 2.2
            void executeAlterTableSetTablespace(); // Phase 4 Task 4.1.6
            void executeDropTablespace();          // Phase 2 Task 2.1
            void executeAttachTablespace();        // Phase 6 Task 6.1
            void executeDetachTablespace();        // Phase 6 Task 6.2
            void executeInsert();
            void executeSelect();
            void executeUpdate();           // Phase 1 Task 1.6.1
            void executeDelete();           // Phase 1 Task 1.6.2
            void executeNestedLoopJoin();   // Phase 1 Task 3.3
            void executeHashJoin();         // Phase 1 Task 3.3
            void executeSweep();            // Phase 3 Task 3.3
            void executeStartTransaction(); // Phase 2 Task 2.6, Phase 3 Task 3.6
            void executeSetTransaction();   // Phase 3 Task 3.6
            void executeCommit();           // Phase 2 Task 2.6
            void executeRollback();         // Phase 2 Task 2.6

            // Monitoring/system table execution
            void executeMonitoringQuery(const std::string &table_name);

            // Aggregation execution helper (Phase 1 Task 1.6.3)
            void executeAggregate(const core::CatalogManager::TableInfo& table_info,
                                 const std::vector<core::CatalogManager::ColumnInfo>& all_columns,
                                 const std::vector<std::pair<std::string, std::string>>& select_items,
                                 bool is_select_star,
                                 bool has_where,
                                 size_t where_start_pc,
                                 size_t where_end_pc);

            // Sorting execution helper (Phase 1 Task 1.6.4)
            void executeSort(std::unique_ptr<ResultSet> input_result_set);

            // Expression evaluation
            void evaluateExpression();
            void executeBinaryOp(Opcode op);

            // Pattern matching helpers
            bool matchPattern(const std::string &text, const std::string &pattern,
                              bool case_insensitive);
            bool matchPatternRecursive(const std::string &text, size_t text_pos,
                                       const std::string &pattern, size_t pattern_pos);

            // Collation-aware string comparison helper
            int compareStrings(const std::string &left, const std::string &right,
                               uint32_t collation_id = 101) const;

            // Error handling
            void error(const std::string &msg);

            // Tuple deserialization helper
            bool deserializeTuple(const uint8_t *tuple_data, uint32_t tuple_size,
                                  const std::vector<core::CatalogManager::ColumnInfo> &columns,
                                  std::vector<Value> &values_out);

            // JOIN execution helpers (Phase 1 Task 3.3)
            std::unique_ptr<ResultSet> executeChildPlan();
            bool evaluateJoinCondition(const std::vector<Value> &outer_row,
                                       const std::vector<Value> &inner_row,
                                       const std::vector<core::CatalogManager::ColumnInfo> &outer_columns,
                                       const std::vector<core::CatalogManager::ColumnInfo> &inner_columns,
                                       size_t condition_start_pc, size_t condition_end_pc);
            std::vector<Value> combineRows(const std::vector<Value> &outer_row,
                                           const std::vector<Value> &inner_row);

            // Aggregation execution helpers (Phase 1 Task 1.6.3)
            struct AggregateAccumulator
            {
                enum class AggFunc { COUNT, SUM, AVG, MIN, MAX };

                AggFunc func;
                bool distinct;
                Value result;           // Final result
                int64_t count;          // For COUNT and AVG
                double sum;             // For SUM and AVG
                std::unordered_set<std::string> distinct_values; // For DISTINCT

                AggregateAccumulator(AggFunc f, bool d)
                    : func(f), distinct(d), count(0), sum(0.0) {}

                void accumulate(const Value& val);
                Value finalize();
            };

            struct GroupKey
            {
                std::vector<Value> values;

                bool operator==(const GroupKey& other) const;
                size_t hash() const;
            };

            struct GroupKeyHash
            {
                size_t operator()(const GroupKey& key) const { return key.hash(); }
            };

            using AggregateState = std::vector<AggregateAccumulator>;
            using GroupMap = std::unordered_map<GroupKey, AggregateState, GroupKeyHash>;
        };

    } // namespace sblr
} // namespace scratchbird