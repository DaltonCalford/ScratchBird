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
    // Forward declarations
    namespace core
    {
        class ConnectionContext;
    }

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

        // Task 17 MGA Phase 2.2: Index maintenance statistics
        struct IndexMaintenanceStats
        {
            uint64_t entries_added = 0;
            uint64_t entries_removed = 0;
            uint64_t entries_updated = 0;
            uint64_t expression_evaluations = 0;
            uint64_t predicate_evaluations = 0;
            uint64_t invisible_skipped = 0;
            uint64_t indexes_maintained = 0;

            double total_eval_time_ms = 0.0;
            double total_insert_time_ms = 0.0;
            double total_remove_time_ms = 0.0;

            void reset()
            {
                entries_added = 0;
                entries_removed = 0;
                entries_updated = 0;
                expression_evaluations = 0;
                predicate_evaluations = 0;
                invisible_skipped = 0;
                indexes_maintained = 0;
                total_eval_time_ms = 0.0;
                total_insert_time_ms = 0.0;
                total_remove_time_ms = 0.0;
            }
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

            // Set connection context for security and transaction state (Phase 2 - Security System)
            // Must be called before executing any security-related operations (CREATE USER, GRANT, etc.)
            void setConnectionContext(core::ConnectionContext *conn_ctx)
            {
                conn_ctx_ = conn_ctx;
            }

            // Task 17 MGA Phase 2.2: Access index maintenance statistics
            const IndexMaintenanceStats& getIndexStats() const { return index_stats_; }
            void resetIndexStats() { index_stats_.reset(); }

        private:
            core::Database *db_;
            core::CharsetManager charset_manager_;
            core::TimezoneManager timezone_manager_;

            // Connection context for security and transaction state (Phase 2 - Security System)
            // NOTE: This is a non-owning pointer that must be set before executing security-related operations
            core::ConnectionContext *conn_ctx_ = nullptr;

            // Execution state
            // Note: bytecode_ is a raw pointer that must remain valid during execute()
            const uint8_t *bytecode_;
            size_t bytecode_size_;
            size_t pc_; // Program counter
            std::stack<Value> stack_;

            // CTE (Common Table Expression) storage (Phase 2 Wave 2)
            // Maps CTE name -> materialized result rows
            std::unordered_map<std::string, std::vector<std::vector<Value>>> cte_results_;
            std::unordered_map<std::string, std::vector<std::string>> cte_column_names_;
            std::unordered_map<std::string, std::vector<core::DataType>> cte_column_types_;

            // Current statement context
            std::string current_table_;
            std::vector<std::string> current_columns_;
            std::unique_ptr<ResultSet> current_result_set_;

            // Row context for expression evaluation (during SELECT WHERE)
            const std::vector<Value> *current_row_values_ = nullptr;
            const std::vector<core::CatalogManager::ColumnInfo> *current_row_columns_ = nullptr;

            // Session state for CURRVAL (ALPHA Phase 1 - Sequences)
            std::unordered_map<core::ID, int64_t> session_sequence_currval_;

            // Task 17 MGA Phase 2.2: Index maintenance statistics
            IndexMaintenanceStats index_stats_;

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
            // Task 17 Phase 6: Build expression/filtered index
            // Task 17 MGA Phase 1.1: Added xid parameter for transaction context
            void buildExpressionIndex(uint64_t xid,
                                     const core::CatalogManager::TableInfo &table_info,
                                     const core::ID &index_id);

            // Task 17 Phase 7: Index maintenance helpers
            // Task 17 MGA Phase 1.1: Added xid parameter for transaction context
            void updateIndexesOnInsert(uint64_t xid,
                                      const core::ID &table_id,
                                      const core::CatalogManager::TableInfo &table_info,
                                      const std::vector<core::CatalogManager::ColumnInfo> &all_columns,
                                      uint32_t page_id,
                                      uint16_t item_id,
                                      const std::vector<Value> &row_values);

            // Task 17 MGA Phase 1.1: Added xid parameter for transaction context
            void updateIndexesOnUpdate(uint64_t xid,
                                      const core::ID &table_id,
                                      const core::CatalogManager::TableInfo &table_info,
                                      const std::vector<core::CatalogManager::ColumnInfo> &all_columns,
                                      const std::vector<Value> &old_values,
                                      const std::vector<Value> &new_values,
                                      core::TID old_tid,
                                      core::TID new_tid);

            // Task 17 MGA Phase 1.1: Added xid parameter for transaction context
            void updateIndexesOnDelete(uint64_t xid,
                                      const core::ID &table_id,
                                      const core::CatalogManager::TableInfo &table_info,
                                      const std::vector<core::CatalogManager::ColumnInfo> &all_columns,
                                      const std::vector<Value> &row_values,
                                      core::TID tid);

            void serializeIndexKey(const std::vector<Value> &key_values,
                                  std::vector<uint8_t> &key_bytes_out);

            void executeCreateTablespace();        // Phase 2 Task 2.1
            void executeAlterTablespace();         // Phase 2 Task 2.2
            void executeAlterTableSetTablespace(); // Phase 4 Task 4.1.6
            void executeDropTable();               // ALPHA Phase 1 - DDL Modifications
            void executeDropIndex();               // ALPHA Phase 1 - DDL Modifications
            void executeAlterTable();              // ALPHA Phase 1 - DDL Modifications
            void executeTruncateTable();           // ALPHA Phase 1 - DDL Modifications (TRUNCATE TABLE ASYNC)
            void executeDropTablespace();          // Phase 2 Task 2.1
            void executeAttachTablespace();        // Phase 6 Task 6.1
            void executeDetachTablespace();        // Phase 6 Task 6.2
            void executeInsert();
            void executeSelect();
            void executeViewQuery(const core::CatalogManager::ViewInfo& view_info,
                                 const std::vector<std::pair<std::string, std::string>>& select_items,
                                 bool is_select_star);  // ALPHA Phase 1 - Views
            void executeUpdate();           // Phase 1 Task 1.6.1
            void executeDelete();           // Phase 1 Task 1.6.2
            void executeNestedLoopJoin();   // Phase 1 Task 3.3
            void executeHashJoin();         // Phase 1 Task 3.3
            void executeSweep();            // Phase 3 Task 3.3
            void executeStartTransaction(); // Phase 2 Task 2.6, Phase 3 Task 3.6
            void executeSetTransaction();   // Phase 3 Task 3.6
            void executeCommit();           // Phase 2 Task 2.6
            void executeRollback();         // Phase 2 Task 2.6

            // Trigger execution (Wave 2)
            void executeCreateTrigger();    // CREATE TRIGGER
            void executeDropTrigger();      // DROP TRIGGER

            // Sequence execution (ALPHA Phase 1 - Sequences)
            void executeCreateSequence();
            void executeAlterSequence();
            void executeDropSequence();
            int64_t executeSequenceNextVal();  // Returns value
            int64_t executeSequenceCurrVal();  // Returns value
            int64_t executeSequenceSetVal();   // Returns value

            // View execution (ALPHA Phase 1 - Views)
            void executeCreateView();
            void executeDropView();
            void executeRefreshMaterializedView();  // ALPHA Phase 1 - Materialized Views

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

            // LIMIT/OFFSET execution helper (Phase 1 Task 1.6.5)
            void executeLimit(std::unique_ptr<ResultSet> input_result_set);

            // Window function execution helper (Phase 1 Task 6.5)
            void executeWindow(std::unique_ptr<ResultSet> input_result_set);

            // Expression evaluation
            void evaluateExpression();
            void executeBinaryOp(Opcode op);

            // Pattern matching helpers
            bool matchPattern(const std::string &text, const std::string &pattern,
                              bool case_insensitive);
            bool matchPatternRecursive(const std::string &text, size_t text_pos,
                                       const std::string &pattern, size_t pattern_pos);

            // Regex helpers (Phase 2 Task 13)
            bool matchRegex(const std::string &text, const std::string &pattern, bool case_insensitive);
            std::vector<std::string> regexMatches(const std::string &text, const std::string &pattern, const std::string &flags);
            std::string regexReplace(const std::string &text, const std::string &pattern, const std::string &replacement, const std::string &flags);
            std::vector<std::string> regexSplit(const std::string &text, const std::string &pattern, const std::string &flags);

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
                enum class AggFunc {
                    COUNT, SUM, AVG, MIN, MAX, ARRAY_AGG,
                    // Statistical functions (Nov 14, 2025)
                    STDDEV_SAMP, STDDEV_POP, VAR_SAMP, VAR_POP, CORR, COVAR_POP
                };

                AggFunc func;
                bool distinct;
                Value result;           // Final result
                int64_t count;          // For COUNT and AVG
                double sum;             // For SUM and AVG
                std::unordered_set<std::string> distinct_values; // For DISTINCT
                std::vector<Value> array_elements;  // For ARRAY_AGG

                // Statistical function state (Welford's algorithm for numerical stability)
                double mean;            // Running mean
                double m2;              // Sum of squared deviations from mean

                // For correlation/covariance (2-variable statistics)
                double sum_x;           // Σx
                double sum_y;           // Σy
                double sum_xy;          // Σ(xy)
                double sum_x2;          // Σ(x²)
                double sum_y2;          // Σ(y²)

                AggregateAccumulator(AggFunc f, bool d)
                    : func(f), distinct(d), count(0), sum(0.0),
                      mean(0.0), m2(0.0),
                      sum_x(0.0), sum_y(0.0), sum_xy(0.0), sum_x2(0.0), sum_y2(0.0) {}

                void accumulate(const Value& val);
                void accumulate2(const Value& val1, const Value& val2);  // For CORR, COVAR
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

            // Window function execution helpers (Phase 1 Task 6.5)
            struct WindowFunctionSpec
            {
                enum class FuncType {
                    ROW_NUMBER, RANK, DENSE_RANK,
                    LAG, LEAD,
                    FIRST_VALUE, LAST_VALUE, NTH_VALUE
                };

                FuncType func_type;
                std::vector<Value> args;  // Function arguments (already evaluated)
                std::vector<size_t> partition_cols;  // Column indices for PARTITION BY
                std::vector<size_t> order_cols;       // Column indices for ORDER BY
                std::vector<bool> order_asc;          // Sort directions
                bool has_frame;
                bool frame_is_rows;  // ROWS vs RANGE
                int64_t frame_start_offset;  // -1 = UNBOUNDED PRECEDING, 0 = CURRENT ROW
                int64_t frame_end_offset;    // -1 = UNBOUNDED FOLLOWING, 0 = CURRENT ROW
                std::string output_column;
            };

            struct Partition
            {
                std::vector<size_t> row_indices;  // Indices into input result set
                std::vector<Value> partition_key; // Partition BY values for this partition
            };

            // Helper to compute window function for a partition
            Value computeWindowFunction(const WindowFunctionSpec& spec,
                                       const Partition& partition,
                                       size_t current_row_in_partition,
                                       const ResultSet* input_result);

            // Helper to identify partition boundaries
            std::vector<Partition> identifyPartitions(const ResultSet* input_result,
                                                      const std::vector<size_t>& partition_cols);

            // Helper to sort rows within a partition
            void sortPartition(Partition& partition,
                             const ResultSet* input_result,
                             const std::vector<size_t>& order_cols,
                             const std::vector<bool>& order_asc);

            // Trigger execution helpers (Wave 2)
            // ===== PSQL - Stored Procedures and Functions (Phase 2 Task 10.2, Phase 4) =====

            // Variable stack frame for PSQL execution
            struct VariableFrame
            {
                std::unordered_map<std::string, Value> variables;
                VariableFrame* parent;  // For nested blocks

                VariableFrame() : parent(nullptr) {}
                explicit VariableFrame(VariableFrame* parent_frame) : parent(parent_frame) {}
            };

            // Variable stack management
            class VariableStack
            {
            public:
                VariableStack() { pushFrame(); }  // Always have at least one frame
                ~VariableStack() { while (!frames_.empty()) popFrame(); }

                void pushFrame();
                void popFrame();
                void declareVariable(const std::string& name, const Value& value);
                Value& getVariable(const std::string& name);
                void setVariable(const std::string& name, const Value& value);
                bool hasVariable(const std::string& name) const;

            private:
                std::vector<std::unique_ptr<VariableFrame>> frames_;
            };

            // Control flow state for loops
            struct LoopState
            {
                size_t loop_start_pc;      // PC at loop beginning
                size_t loop_end_pc;        // PC after loop end
                std::string label;         // Optional label for EXIT statement
                bool exit_requested;       // Set by EXIT statement

                LoopState(size_t start, size_t end, const std::string& lbl = "")
                    : loop_start_pc(start), loop_end_pc(end), label(lbl), exit_requested(false) {}
            };

            // Exception frame for PSQL exception handling
            struct ExceptionFrame
            {
                size_t try_start_pc;
                size_t try_end_pc;
                std::vector<std::pair<std::string, size_t>> handlers;  // (exception_name, handler_pc)

                ExceptionFrame(size_t start, size_t end) : try_start_pc(start), try_end_pc(end) {}
            };

            // PSQL execution state
            std::unique_ptr<VariableStack> variable_stack_;
            std::vector<LoopState> loop_stack_;
            std::vector<ExceptionFrame> exception_stack_;
            bool return_requested_ = false;
            Value return_value_;

            // PSQL statement execution methods
            void executeFunction();          // Execute CREATE FUNCTION
            void executeProcedure();         // Execute CREATE PROCEDURE
            void executeBlock();             // Execute BEGIN...END block
            void executeVarDeclaration();    // Execute variable declaration
            void executeAssignment();        // Execute variable assignment
            void executeIfStatement();       // Execute IF statement
            void executeLoopStatement();     // Execute LOOP statement
            void executeWhileStatement();    // Execute WHILE statement
            void executeExitStatement();     // Execute EXIT statement
            void executeReturnStatement();   // Execute RETURN statement
            void executeRaiseStatement();    // Execute RAISE exception

            // PSQL variable operations
            void executeVarLoad();           // Load variable onto stack
            void executeVarStore();          // Store stack value to variable

            // PSQL control flow helpers
            void executeJump();              // Unconditional jump
            void executeJumpIfTrue();        // Jump if stack top is true
            void executeJumpIfFalse();       // Jump if stack top is false

            // Security Statements (ALPHA Phase 1 - Security System Phase 2)
            void executeCreateUser();        // Execute CREATE USER
            void executeAlterUser();         // Execute ALTER USER
            void executeDropUser();          // Execute DROP USER
            void executeCreateRole();        // Execute CREATE ROLE
            void executeDropRole();          // Execute DROP ROLE
            void executeCreateGroup();       // Execute CREATE GROUP
            void executeDropGroup();         // Execute DROP GROUP
            void executeGrantPrivilege();    // Execute GRANT privilege
            void executeRevokePrivilege();   // Execute REVOKE privilege
            void executeGrantRole();         // Execute GRANT role
            void executeRevokeRole();        // Execute REVOKE role
            void executeSetRole();           // Execute SET ROLE / RESET ROLE
            void executeSetSessionAuth();    // Execute SET/RESET SESSION AUTHORIZATION
            void executeCreatePolicy();      // Execute CREATE POLICY (Security Phase 3.4.4)
            void executeDropPolicy();        // Execute DROP POLICY (Security Phase 3.4.4)
            void executeAlterTableRLS();     // Execute ALTER TABLE ... ROW LEVEL SECURITY (Security Phase 3.4.4)

            // Security context helpers (Phase 2 - Security System)
            // These wrap ConnectionContext methods for convenience
            const core::ID& getCurrentUserID() const;
            const core::ID& getActiveRoleID() const;
            bool isSuperuser() const;

            // Permission check helper (Phase 2 - Security System)
            // Returns true if the current user has the specified privilege on the object
            bool checkPermission(const core::ID& object_id,
                               core::CatalogManager::PermissionObjectType object_type,
                               uint32_t required_privilege);

            // Row-Level Security helpers (Phase 3.5 - RLS DML Enforcement)

            // Check if current user/role should be subject to RLS policies
            // Returns false if user is superuser or table owner (bypass RLS)
            bool shouldEnforceRLS(const core::ID& table_id);

            // Check if a row passes RLS policies for the given operation
            // Returns true if all applicable policies pass, false otherwise
            bool checkRLSPolicies(const core::ID& table_id,
                                const std::vector<Value>& row_values,
                                const std::vector<core::CatalogManager::ColumnInfo>& columns,
                                core::CatalogManager::PolicyType policy_type,
                                bool is_with_check);

            // Check if a specific policy applies to the current user
            bool policyAppliesToUser(const core::CatalogManager::PolicyInfo& policy);

            // Convert hex string to bytecode (for deserializing policy expressions)
            std::vector<uint8_t> hexToBytes(const std::string& hex_str);

            // Evaluate a policy expression bytecode with row context
            // Returns the boolean result of the expression
            bool evaluatePolicyExpression(const std::vector<uint8_t>& expr_bytecode,
                                        const std::vector<Value>& row_values,
                                        const std::vector<core::CatalogManager::ColumnInfo>& columns);

            // ALPHA Phase A: Evaluate DEFAULT value expression for a column
            // For now, supports simple constant defaults (numbers, strings, booleans, NULL)
            // Future: Support function calls like NOW(), CURRENT_USER, etc.
            Value evaluateDefaultValue(const core::CatalogManager::ColumnInfo& column);

            // ALPHA Phase A: Evaluate CHECK constraint for a column
            // Returns true if constraint passes, false if it fails
            bool evaluateCheckConstraint(const core::CatalogManager::ColumnInfo& column,
                                        const std::vector<Value>& row_values,
                                        const std::vector<core::CatalogManager::ColumnInfo>& columns);

            // ALPHA Phase A: Check for UNIQUE constraint violation
            // Returns true if a duplicate value exists (violation), false if value is unique
            bool checkUniqueViolation(const core::ID& table_id,
                                     const core::CatalogManager::ColumnInfo& column,
                                     const Value& value,
                                     const std::vector<core::CatalogManager::ColumnInfo>& all_columns);

            // ALPHA Phase A: Check for UNIQUE constraint violation during UPDATE
            // Similar to checkUniqueViolation, but excludes the row being updated (identified by TID)
            bool checkUniqueViolationForUpdate(const core::ID& table_id,
                                              const core::CatalogManager::ColumnInfo& column,
                                              const Value& value,
                                              const std::vector<core::CatalogManager::ColumnInfo>& all_columns,
                                              const core::TID& exclude_tid);

            // ALPHA Phase A: Compare two values for equality (for UNIQUE constraint checking)
            bool valuesEqual(const Value& a, const Value& b);

            // ALPHA Phase A: Foreign Key constraint enforcement
            // Check if FK constraint is satisfied on INSERT/UPDATE (child table)
            bool checkForeignKeyExists(const core::ID& parent_table_id,
                                      const std::vector<std::string>& parent_columns,
                                      const std::vector<Value>& fk_values,
                                      const std::vector<core::CatalogManager::ColumnInfo>& parent_cols);

            // Apply FK referential action on DELETE (parent table)
            void applyFKActionOnDelete(const core::ID& parent_table_id,
                                      const std::vector<Value>& deleted_key_values,
                                      const std::vector<core::CatalogManager::ColumnInfo>& parent_cols);

            // Apply FK referential action on UPDATE (parent table)
            void applyFKActionOnUpdate(const core::ID& parent_table_id,
                                      const std::vector<Value>& old_key_values,
                                      const std::vector<Value>& new_key_values,
                                      const std::vector<core::CatalogManager::ColumnInfo>& parent_cols);

            // Tuple modification helpers for FK actions (Phase B)
            // Serialize tuple from column values
            bool serializeTupleFromValues(const std::vector<Value>& values,
                                         const std::vector<core::CatalogManager::ColumnInfo>& columns,
                                         std::vector<uint8_t>& tuple_data_out);

            // Modify specific columns in a tuple and reserialize
            bool modifyTupleColumns(const uint8_t* original_tuple, uint32_t original_size,
                                   const std::vector<core::CatalogManager::ColumnInfo>& all_columns,
                                   const std::vector<size_t>& column_indices,
                                   const std::vector<Value>& new_values,
                                   std::vector<uint8_t>& new_tuple_out);

            // Bit Manipulation Functions (Nov 14, 2025)
            void executeGetByte();       // GET_BYTE(bytes, offset)
            void executeSetByte();       // SET_BYTE(bytes, offset, value)
            void executeGetBit();        // GET_BIT(bytes, bit_offset)
            void executeSetBit();        // SET_BIT(bytes, bit_offset, value)
            void executeBitAnd();        // BIT_AND(a, b) / a & b
            void executeBitOr();         // BIT_OR(a, b) / a | b
            void executeBitXor();        // BIT_XOR(a, b) / a ^ b
            void executeBitNot();        // BIT_NOT(a) / ~a
            void executeBitShiftLeft();  // a << n
            void executeBitShiftRight(); // a >> n (arithmetic)
            void executeBitShiftRightLogical(); // a >>> n (logical)
            void executeBitCount();      // BIT_COUNT(a) - popcount
            void executeBitLength();     // BIT_LENGTH(bytes)
            void executeBitMask();       // BIT_MASK(length)

            // Statistical Functions (Nov 14, 2025)
            void executeStdDevSamp();    // STDDEV / STDDEV_SAMP(expr)
            void executeStdDevPop();     // STDDEV_POP(expr)
            void executeVarSamp();       // VARIANCE / VAR_SAMP(expr)
            void executeVarPop();        // VAR_POP(expr)
            void executeCorr();          // CORR(y, x)
            void executeCovarPop();      // COVAR_POP(y, x)

            // Cryptographic Functions (Nov 14, 2025)
            void executeMD5();           // MD5(data)
            void executeSHA1();          // SHA1(data)
            void executeSHA256();        // SHA256(data)
            void executeSHA512();        // SHA512(data)
            void executeEncode();        // ENCODE(data, format)
            void executeDecode();        // DECODE(text, format)

            // XML Functions (Nov 14, 2025)
            void executeXMLParse();      // XMLPARSE(document_or_content, xml_text)
            void executeXMLSerialize();  // XMLSERIALIZE(content_or_document xml AS type)
            void executeXMLElement();    // XMLELEMENT(name, content)
            void executeXMLConcat();     // XMLCONCAT(xml, ...)
            void executeXMLForest();     // XMLFOREST(expr AS name, ...)
            void executeXMLComment();    // XMLCOMMENT(text)
            void executeXMLRoot();       // XMLROOT(xml, version, standalone)
            void executeXPath();         // XPATH(xpath_expr, xml)
            void executeXMLExists();     // XMLEXISTS(xpath_expr, xml)

        public:
            // Forward declaration
            class TriggerContext;

            // Trigger procedure type: takes TriggerContext, returns true to continue operation
            using TriggerProcedure = std::function<bool(const TriggerContext&)>;

            // Register a trigger procedure for testing
            void registerTriggerProcedure(const std::string& name, TriggerProcedure procedure);

        private:
            // Trigger procedure registry
            std::unordered_map<std::string, TriggerProcedure> trigger_procedures_;

            // Fire a trigger for the given context
            bool fireTrigger(const TriggerContext& ctx);
        };

    } // namespace sblr
} // namespace scratchbird