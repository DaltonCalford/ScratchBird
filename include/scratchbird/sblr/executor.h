/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/sblr/index_cache.h"
#include "scratchbird/sblr/query_limits.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/charset.h"
#include "scratchbird/core/timezone.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/permission_cache.h"  // For PermissionCheckMode
#include "scratchbird/core/function_invoker.h"
#include "scratchbird/core/lock_manager.h"
// Index headers needed for template implementation
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/rtree_index.h"
#include "scratchbird/core/columnstore_index.h"
#include "scratchbird/core/gist_index.h"
#include "scratchbird/core/spgist_index.h"
#include "scratchbird/core/lsm_tree_index.h"
#include <vector>
#include <memory>
#include <functional>
#include <variant>
#include <stack>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <type_traits>
#include <iosfwd>

namespace scratchbird
{
    // Forward declarations
    namespace core
    {
        class ConnectionContext;
        class HashIndex;
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
            using RowCallback = std::function<void(const std::vector<Value>&)>;
            void setRowCallback(RowCallback cb)
            {
                row_callback_ = std::move(cb);
            }
            void setStoreRows(bool store)
            {
                store_rows_ = store;
            }
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
            RowCallback row_callback_;
            bool store_rows_ = true;
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
        class Executor : public core::FunctionInvoker
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

            // Optional COPY STDIN/STDOUT streams (defaults to std::cin/std::cout when unset).
            void setCopyInputStream(std::istream* in) { copy_input_stream_ = in; }
            void setCopyOutputStream(std::ostream* out) { copy_output_stream_ = out; }

            // SECURITY ENHANCEMENT (MEDIUM-3): Query execution limits management
            void setQueryLimits(const QueryLimits& limits) { query_limits_ = limits; }
            const QueryLimits& getQueryLimits() const { return query_limits_; }

            // Set current schema for DDL operations (Firebird emulation support)
            // If not set, defaults to root schema
            void setCurrentSchema(const core::ID& schema_id) { current_schema_id_ = schema_id; current_schema_set_ = true; }
            void clearCurrentSchema() { current_schema_id_ = core::ID(); current_schema_set_ = false; }
            bool hasCurrentSchema() const { return current_schema_set_; }
            const core::ID& getCurrentSchema() const { return current_schema_id_; }

            // Bind parameters for placeholder evaluation (prepared statements)
            void setParameters(const std::vector<std::string>& values,
                               const std::vector<bool>& nulls);
            void clearParameters();

            // Task 17 MGA Phase 2.2: Access index maintenance statistics
            const IndexMaintenanceStats& getIndexStats() const { return index_stats_; }
            void resetIndexStats() { index_stats_.reset(); }

            int64_t getLastAffectedRows() const { return last_affected_rows_; }

            // NET-M1: Query cancellation support
            // Request cancellation of the currently executing query
            void requestCancellation() { cancel_requested_.store(true, std::memory_order_release); }

            // Check if cancellation has been requested
            bool isCancellationRequested() const { return cancel_requested_.load(std::memory_order_acquire); }

            // Reset cancellation flag (called before starting a new query)
            void resetCancellation() { cancel_requested_.store(false, std::memory_order_release); }

            // PSQL procedure invocation (for database triggers and CALL statement)
            // Executes a stored procedure by name with optional arguments
            // Returns success/error status
            ExecutionResult callProcedureByName(const std::string& procedure_name,
                                                const std::vector<Value>& args = {});

            // Function invocation for domain WITH blocks and custom validations
            auto callFunctionByName(const std::string& function_name,
                                    const std::vector<Value>& args,
                                    Value& result_out,
                                    core::ErrorContext* ctx = nullptr) -> core::Status override;

        private:
            auto callFunctionByInfo(const core::CatalogManager::FunctionInfo& function_info,
                                    const std::vector<Value>& args,
                                    Value& result_out,
                                    core::ErrorContext* ctx = nullptr) -> core::Status;
            auto callFunctionById(const core::ID& function_id,
                                  const std::vector<Value>& args,
                                  Value& result_out,
                                  core::ErrorContext* ctx = nullptr) -> core::Status;
            auto callUDRFunctionById(const core::ID& udr_id,
                                     const std::vector<Value>& args,
                                     Value& result_out,
                                     core::ErrorContext* ctx = nullptr) -> core::Status;
            bool isEffectiveSuperuser() const;
            void recordObjectDefinition(core::CatalogManager::ObjectType object_type,
                                         const core::ID& object_id);
            void deleteObjectDefinition(core::CatalogManager::ObjectType object_type,
                                         const core::ID& object_id);

            core::Database *db_;
            core::CharsetManager charset_manager_;
            core::TimezoneManager timezone_manager_;

            // Connection context for security and transaction state (Phase 2 - Security System)
            // NOTE: This is a non-owning pointer that must be set before executing security-related operations
            core::ConnectionContext *conn_ctx_ = nullptr;
            std::unique_ptr<core::ConnectionContext> owned_conn_ctx_;
            std::optional<core::ID> effective_user_override_;
            std::optional<bool> effective_superuser_override_;
            std::istream* copy_input_stream_ = nullptr;
            std::ostream* copy_output_stream_ = nullptr;

            // Execution state
            // Note: bytecode_ is a raw pointer that must remain valid during execute()
            const uint8_t *bytecode_;
            const std::vector<uint8_t> *current_bytecode_vec_ = nullptr;
            size_t bytecode_size_;
            size_t pc_; // Program counter
            std::stack<Value> stack_;

            // CTE (Common Table Expression) storage (Phase 2 Wave 2)
            // Maps CTE name -> materialized result rows
            std::unordered_map<std::string, std::vector<std::vector<Value>>> cte_results_;
            std::unordered_map<std::string, std::vector<std::string>> cte_column_names_;
            std::unordered_map<std::string, std::vector<core::DataType>> cte_column_types_;
            bool cte_is_recursive_; // True if the current WITH clause is recursive

            // Current statement context
            std::string current_table_;
            std::vector<std::string> current_columns_;
            std::unique_ptr<ResultSet> current_result_set_;

            // Row context for expression evaluation (during SELECT WHERE)
            const std::vector<Value> *current_row_values_ = nullptr;
            const std::vector<core::CatalogManager::ColumnInfo> *current_row_columns_ = nullptr;
            bool current_row_case_insensitive_ = false;
            std::unordered_map<std::string, size_t> current_row_alias_map_;
            // Insert-row context (for VALUES(col) in ON CONFLICT updates)
            const std::vector<Value> *current_insert_values_ = nullptr;
            const std::vector<core::CatalogManager::ColumnInfo> *current_insert_columns_ = nullptr;
            bool aggregate_scan_active_ = false;
            bool aggregate_scan_found_ = false;
            bool skip_expression_disallow_aggregates_ = false;
            bool scalar_aggregate_filter_active_ = false;
            size_t scalar_aggregate_filter_start_ = 0;
            size_t scalar_aggregate_filter_end_ = 0;

            // Bound parameters for placeholders (prepared statements)
            std::vector<std::string> parameter_values_;
            std::vector<bool> parameter_nulls_;
            std::unordered_set<core::ID, core::IDHash> last_select_table_ids_;
            bool last_select_cacheable_ = true;

            struct MySqlTableLock {
                core::LockTag tag{};
                core::LockMode mode = core::LockMode::LOCK_ACCESS_SHARE;
            };
            std::vector<MySqlTableLock> mysql_table_locks_;

            // BLR savepoint scope tracking (implicit savepoints)
            std::vector<std::string> blr_savepoint_stack_;
            uint64_t blr_savepoint_counter_ = 0;

            // Grouping context for ROLLUP/CUBE/GROUPING SETS (Phase 3: Advanced Grouping)
            size_t current_grouping_set_index_ = 0;
            std::vector<size_t> current_grouping_set_column_pcs_;  // Expression PCs in current set
            size_t total_grouping_columns_ = 0;                     // Total columns across all sets

            // Session state for CURRVAL (ALPHA Phase 1 - Sequences)
            std::unordered_map<core::ID, int64_t> session_sequence_currval_;

            // Task 17 MGA Phase 2.2: Index maintenance statistics
            IndexMaintenanceStats index_stats_;

            // Index cache for performance (November 19, 2025)
            // LRU cache of frequently accessed index instances
            IndexCache index_cache_;

            // SECURITY ENHANCEMENT (MEDIUM-3): Query execution limits for DoS protection
            QueryLimits query_limits_;
            std::chrono::steady_clock::time_point query_start_time_;
            uint32_t cte_recursion_depth_ = 0;
            uint64_t rows_processed_ = 0;
            int64_t last_affected_rows_ = 0;

            // NET-M1: Query cancellation flag (atomic for thread-safe access)
            std::atomic<bool> cancel_requested_{false};

            // Current schema for DDL operations (Firebird emulation support)
            core::ID current_schema_id_;
            bool current_schema_set_ = false;

            // Execution helpers
            uint8_t readByte();
            uint16_t readExtendedOpcode();
            uint16_t readInt16();
            uint32_t readInt32();
            uint64_t readInt64();
            uint64_t readUVarint();
            double readDouble();
            std::string readString();
            std::string readString16();
            core::ObjectPath readObjectPath();
            core::ID readId();
            void readTableRefPayload(core::ID& table_id_out,
                                     std::string& name_out,
                                     std::string& alias_out,
                                     bool& has_uuid_out);
            size_t skipExpressionRange(size_t start_pc);
            size_t skipExpressionRangeNoAggregates(size_t start_pc);
            void skipSelectStatement();
            core::DataType readDataTypeWithModifiers(uint32_t& precision_out,
                                                     uint32_t& scale_out,
                                                     bool* with_timezone_out = nullptr,
                                                     uint16_t* timezone_hint_out = nullptr);
            core::DataType readColumnTypeWithDomain(core::ID& domain_id_out,
                                                    uint32_t& precision_out,
                                                    uint32_t& scale_out,
                                                    bool& is_array_out,
                                                    uint32_t& array_size_out,
                                                    bool* with_timezone_out = nullptr,
                                                    uint16_t* timezone_hint_out = nullptr);
            void readDependencies(std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>>& deps);
            core::Status resolveSchemaIdForName(const std::string& schema_path,
                                                core::ID& schema_id_out,
                                                core::ErrorContext* ctx,
                                                bool allow_search_path = true);
            core::Status resolveSchemaIdForQualifiedName(const std::string& qualified_name,
                                                         std::string& object_name_out,
                                                         core::ID& schema_id_out,
                                                         core::ErrorContext* ctx,
                                                         bool allow_search_path = true);
            core::Status resolveObjectIdForQualifiedName(
                const std::string& qualified_name,
                core::CatalogManager::ObjectType expected_type,
                core::ID& object_id_out,
                core::CatalogManager::ObjectType& resolved_type_out,
                core::CatalogManager::ResolvedObject* resolved_out,
                core::ErrorContext* ctx,
                bool allow_search_path = true);

            void push(const Value &v)
            {
                stack_.push(v);
            }
            Value pop();
            Value getStackValueAtOffset(uint16_t offset);
            void setStackValueAtOffset(uint16_t offset, const Value& value);
            Value resolvePlaceholderValue(uint16_t position, uint16_t type_hint);
            Value parsePlaceholderValue(const std::string& raw, uint16_t type_hint) const;

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
            void executeAlterIndex();
            void executeAlterTable();              // ALPHA Phase 1 - DDL Modifications
            void executeRenameObject();
            void executeMoveObject();
            void executeCreateSchema();
            void executeDropSchema();
            void executeAlterSchema();
            void executeCreateDatabase();
            void executeCreateDomain();
            void executeAlterDomain();
            void executeDropDomain();
            void executeDropDatabase();
            void executeAlterDatabase();
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
            void executeMerge();            // Alpha 1 - Advanced SQL
            void executeAnalyze();          // P1-10: Statistics & ANALYZE
            void executeExplainPlan();      // EXPLAIN statement
            void executeCopy();             // COPY statement (placeholder)
            void executeNestedLoopJoin();   // Phase 1 Task 3.3
            void executeHashJoin();         // Phase 1 Task 3.3
            void executeSweep();            // Phase 3 Task 3.3
            void executeStartTransaction(); // Phase 2 Task 2.6, Phase 3 Task 3.6
            void executeSetTransaction();   // Phase 3 Task 3.6
            void executeStartOrSetTransaction();
            void executeCommit();           // Phase 2 Task 2.6
            void executeRollback();         // Phase 2 Task 2.6
            void executeCommitFlags(uint8_t flags);
            void executeRollbackFlags(uint8_t flags);
            void executeSetAutocommitOpcode();
            void executePrepareTransaction();
            void executeCommitPrepared();
            void executeRollbackPrepared();
            void executeSavepoint();
            void executeReleaseSavepoint();
            void executeRollbackToSavepoint();
            void executeBlrSavepointBegin();
            void executeBlrSavepointEnd();

            // Set operations (UNION, INTERSECT, EXCEPT)
            void executeUnionAll();         // UNION ALL - concatenate with duplicates
            void executeUnion();            // UNION - concatenate without duplicates
            void executeIntersectAll();     // INTERSECT ALL - common rows with duplicates
            void executeIntersect();        // INTERSECT - common rows without duplicates
            void executeExceptAll();        // EXCEPT ALL - left minus right with duplicates
            void executeExcept();           // EXCEPT - left minus right without duplicates

            // Recursive CTE execution
            void executeRecursiveCTE(const std::string& cte_name, size_t base_pc);

            // Trigger execution (Wave 2)
            void executeCreateTrigger();    // CREATE TRIGGER
            void executeDropTrigger();      // DROP TRIGGER

            // Database trigger execution (Firebird-style: ON CONNECT/DISCONNECT/TRANSACTION events)
            void executeCreateDatabaseTrigger();
            void executeDropDatabaseTrigger();

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

            // Stored code DDL (Alpha Phase 3)
            void executeCreateFunctionStatement();
            void executeCreateProcedureStatement();
            void executeCreatePackageStatement();
            void executeCreateExceptionStatement();
            void executeDropFunctionStatement();
            void executeDropProcedureStatement();
            void executeDropPackageStatement();
            void executeDropExceptionStatement();

            // Monitoring/system table execution
            void executeMonitoringQuery(const std::string &table_name);

            struct SelectItemInfo
            {
                enum class Kind
                {
                    STAR,
                    TABLE_STAR,
                    EXPR
                };
                Kind kind = Kind::EXPR;
                size_t expr_start = 0;
                size_t expr_end = 0;
                bool has_aggregate = false;
                std::string alias;
                core::ID table_id{};
                std::string table_name;
                std::string table_alias;
            };

            // Aggregation execution helper (Phase 1 Task 1.6.3)
            void executeAggregate(const core::CatalogManager::TableInfo& table_info,
                                 const std::vector<core::CatalogManager::ColumnInfo>& all_columns,
                                 const std::vector<SelectItemInfo>& select_items,
                                 bool is_select_star,
                                 bool has_where,
                                 size_t where_start_pc,
                                 size_t where_end_pc);

            // Advanced grouping (ROLLUP/CUBE/GROUPING SETS) execution helper (Phase 3: Missing Functions)
            void executeAdvancedGrouping(const core::CatalogManager::TableInfo& table_info,
                                        const std::vector<core::CatalogManager::ColumnInfo>& all_columns,
                                        const std::vector<SelectItemInfo>& select_items,
                                        bool is_select_star,
                                        bool has_where,
                                        size_t where_start_pc,
                                        size_t where_end_pc);

            // EXEC-14: Scalar aggregate execution helper (for aggregates in expression context)
            // When an aggregate opcode is encountered in expression evaluation without
            // top-level AGG_INIT, this executes the aggregate inline by scanning
            // the current table context and returning a single scalar value.
            // func_type: 0=COUNT, 1=SUM, 2=AVG, 3=MIN, 4=MAX, 5=ARRAY_AGG
            Value executeScalarAggregate(uint8_t func_type, size_t arg_expr_start, size_t arg_expr_end);

            // Sorting execution helper (Phase 1 Task 1.6.4)
            void executeSort(std::unique_ptr<ResultSet> input_result_set, bool apply_limit = true);

            // LIMIT/OFFSET execution helper (Phase 1 Task 1.6.5)
            void executeLimit(std::unique_ptr<ResultSet> input_result_set);

            // Window function execution helper (Phase 1 Task 6.5)
            void executeWindow(std::unique_ptr<ResultSet> input_result_set);

            // Expression evaluation
            void evaluateExpression();
            Value evaluateExpressionRange(size_t end_pc);
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
                    STDDEV_SAMP, STDDEV_POP, VAR_SAMP, VAR_POP, CORR, COVAR_POP,
                    // Regression functions (Alpha 1 - Missing Functions)
                    REGR_SLOPE, REGR_INTERCEPT, REGR_COUNT, REGR_R2,
                    REGR_AVGX, REGR_AVGY, REGR_SXX, REGR_SYY, REGR_SXY
                };

                AggFunc func;
                bool distinct;
                core::DataType input_type;
                Value result;           // Final result
                int64_t count;          // For COUNT and AVG
                double sum;             // For SUM and AVG
                core::int128_t int_sum; // For integer SUM/AVG
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
                    : func(f), distinct(d), input_type(core::DataType::UNKNOWN), count(0),
                      sum(0.0), int_sum(0),
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
                    FIRST_VALUE, LAST_VALUE, NTH_VALUE,
                    CUME_DIST, PERCENT_RANK,
                    AGG_COUNT, AGG_SUM, AGG_AVG, AGG_MIN, AGG_MAX
                };

                FuncType func_type;
                std::vector<Value> args;  // Function arguments (already evaluated)
                std::vector<uint8_t> aggregate_expr;  // Raw expression bytecode for aggregates
                bool aggregate_count_star = false;
                std::vector<size_t> partition_cols;  // Column indices for PARTITION BY
                std::vector<size_t> order_cols;       // Column indices for ORDER BY
                std::vector<bool> order_asc;          // Sort directions
                bool has_frame;

                // P2-9: Frame mode enum instead of bool
                enum class FrameMode { ROWS, RANGE, GROUPS };
                FrameMode frame_mode = FrameMode::RANGE;  // Default per SQL spec

                int64_t frame_start_offset;  // -1 = UNBOUNDED PRECEDING, 0 = CURRENT ROW
                int64_t frame_end_offset;    // -1 = UNBOUNDED FOLLOWING, 0 = CURRENT ROW
                std::string output_column;

                // P2-9: For LAG/LEAD offset and default value
                int64_t lag_lead_offset = 1;  // Default offset for LAG/LEAD
                Value lag_lead_default;       // Default value for LAG/LEAD
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
                bool has_condition = false;
                std::vector<uint8_t> condition_bytecode;

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

            // Cursor state for PSQL cursors
            struct CursorState
            {
                std::string name;                          // Cursor name
                std::vector<uint8_t> query_bytecode;       // SELECT query bytecode
                std::vector<std::vector<Value>> result_set; // Materialized query results
                size_t current_row;                        // Current fetch position
                bool is_open;                              // Whether cursor is open
                std::vector<std::string> column_names;     // Result column names
                std::vector<core::DataType> column_types;  // Result column types

                CursorState() : current_row(0), is_open(false) {}
                explicit CursorState(const std::string& cursor_name)
                    : name(cursor_name), current_row(0), is_open(false) {}
            };

            // PSQL execution state
            std::unique_ptr<VariableStack> variable_stack_;
            std::vector<LoopState> loop_stack_;
            std::vector<ExceptionFrame> exception_stack_;
            std::unordered_map<std::string, CursorState> cursors_;  // Active cursors
            std::vector<std::string> psql_output_vars_;
            std::vector<core::DataType> psql_output_types_;
            bool psql_output_active_ = false;
            bool return_requested_ = false;
            Value return_value_;

            // Current exception state (for re-raise)
            bool has_current_exception_ = false;
            std::string current_exception_message_;

            // PSQL statement execution methods
            void executeFunction();          // Execute CREATE FUNCTION
            void executeProcedure();         // Execute CREATE PROCEDURE
            void executeBlock();             // Execute BEGIN...END block
            void executeVarDeclaration(std::string* out_name = nullptr,
                                       core::DataType* out_type = nullptr);    // Execute variable declaration
            void executeAssignment();        // Execute variable assignment
            void executeIfStatement();       // Execute IF statement
            void executeLoopStatement();     // Execute LOOP statement
            void executeWhileStatement();    // Execute WHILE statement
            void executeExitStatement();     // Execute EXIT statement
            void executeReturnStatement();   // Execute RETURN statement
            void executeSuspendStatement();  // Execute SUSPEND statement
            void executeRaiseStatement();    // Execute RAISE exception
            void executeLoopEnd();           // Execute loop end marker
            void executeExecuteStatement();  // Execute EXECUTE STATEMENT (dynamic SQL)
            void executeTryStatement();      // Execute TRY block with exception handlers
            void executeExceptHandler();     // Execute EXCEPT handler block

            // PSQL variable operations
            void executeVarLoad();           // Load variable onto stack
            void executeVarStore();          // Store stack value to variable

            // PSQL control flow helpers
            void executeJump();              // Unconditional jump
            void executeJumpIfTrue();        // Jump if stack top is true
            void executeJumpIfFalse();       // Jump if stack top is false

            Value evaluateExpressionFromBuffer(const std::vector<uint8_t>& buffer);

            // PSQL cursor operations
            void executeCursorDeclare();     // DECLARE cursor_name CURSOR FOR select_statement
            void executeCursorOpen();        // OPEN cursor_name
            void executeCursorFetch();       // FETCH [direction] FROM cursor_name INTO variables
            void executeCursorClose();       // CLOSE cursor_name

            // Security Statements (ALPHA Phase 1 - Security System Phase 2)
            void executeCreateUser();        // Execute CREATE USER
            void executeAlterUser();         // Execute ALTER USER
            void executeDropUser();          // Execute DROP USER
            void executeCreateRole();        // Execute CREATE ROLE
            void executeAlterRole();         // Execute ALTER ROLE
            void executeDropRole();          // Execute DROP ROLE
            void executeCreateJob();         // Execute CREATE JOB
            void executeAlterJob();          // Execute ALTER JOB
            void executeDropJob();           // Execute DROP JOB
            void executeExecuteJob();        // Execute EXECUTE JOB
            void executeCancelJobRun();      // Execute CANCEL JOB RUN
            void executeCreateGroup();       // Execute CREATE GROUP
            void executeDropGroup();         // Execute DROP GROUP
            void executeCreateForeignServer(); // Execute CREATE SERVER (FDW)
            void executeDropForeignServer();   // Execute DROP SERVER (FDW)
            void executeCreateForeignTable();  // Execute CREATE FOREIGN TABLE
            void executeDropForeignTable();    // Execute DROP FOREIGN TABLE
            void executeCreateUserMapping();   // Execute CREATE USER MAPPING
            void executeDropUserMapping();     // Execute DROP USER MAPPING
            void executeCreateSynonym();       // Execute CREATE SYNONYM
            void executeDropSynonym();         // Execute DROP SYNONYM
            void executeCreateUdr();           // Execute CREATE UDR
            void executeDropUdr();             // Execute DROP UDR
            void executeComment();             // Execute COMMENT
            void executeGrantPrivilege();    // Execute GRANT privilege
            void executeRevokePrivilege();   // Execute REVOKE privilege
            void executeAlterDefaultPrivileges(); // Execute ALTER DEFAULT PRIVILEGES
            void executeGrantRole();         // Execute GRANT role
            void executeRevokeRole();        // Execute REVOKE role
            void executeSetRole();           // Execute SET ROLE / RESET ROLE
            void executeSetSessionAuth();    // Execute SET/RESET SESSION AUTHORIZATION
            void executeSetConstraints();    // P2-7: Execute SET CONSTRAINTS
            void executeSetVariable();       // Execute SET variable (EXT_SET_VARIABLE)
            void executeAlterSystem();       // Execute ALTER SYSTEM SET (EXT_ALTER_SYSTEM)
            void executeConnect();           // Execute CONNECT
            void executeDisconnect();        // Execute DISCONNECT
            void executeCreatePolicy();      // Execute CREATE POLICY (Security Phase 3.4.4)
            void executeDropPolicy();        // Execute DROP POLICY (Security Phase 3.4.4)
            void executeAlterPolicy();       // Execute ALTER POLICY (Security Phase 3.4.4)
            void executeAlterTableRLS();     // Execute ALTER TABLE ... ROW LEVEL SECURITY (Security Phase 3.4.4)

            // SQL Engine Commands (ALPHA Phase 1 - Developer Experience)
            void executeShowTables();        // Execute SHOW TABLES
            void executeShowDatabases();     // Execute SHOW DATABASES/SCHEMAS
            void executeShowColumns();       // Execute SHOW COLUMNS FROM table
            void executeShowIndexes();       // Execute SHOW INDEXES FROM table
            void executeShowCreateTable();   // Execute SHOW CREATE TABLE
            void executeDescribeTable();     // Execute DESCRIBE table

            // Extended SHOW Commands (Firebird ISQL compatibility)
            void executeShowTable();         // Execute SHOW TABLE object_name
            void executeShowIndex();         // Execute SHOW INDEX object_name
            void executeShowTrigger();       // Execute SHOW TRIGGER object_name
            void executeShowProcedure();     // Execute SHOW PROCEDURE object_name
            void executeShowFunction();      // Execute SHOW FUNCTION object_name
            void executeShowView();          // Execute SHOW VIEW object_name
            void executeShowDomain();        // Execute SHOW DOMAIN object_name
            void executeShowGenerator();     // Execute SHOW GENERATOR object_name (sequence)
            void executeShowSchema();        // Execute SHOW SCHEMA [object_name]
            void executeShowRole();          // Execute SHOW ROLE object_name
            void executeShowGrants();        // Execute SHOW GRANTS [FOR object_name]
            void executeShowJobs();          // Execute SHOW JOBS
            void executeShowJob();           // Execute SHOW JOB job_name
            void executeShowJobRuns();       // Execute SHOW JOB RUNS job_name
            void executeShowChecks();        // Execute SHOW CHECKS object_name
            void executeShowCollations();    // Execute SHOW COLLATIONS [LIKE pattern]
            void executeShowComments();      // Execute SHOW COMMENTS object_name
            void executeShowDependencies();  // Execute SHOW DEPENDENCIES object_name
            void executeShowPackage();       // Execute SHOW PACKAGE object_name
            void executeShowSystem();        // Execute SHOW SYSTEM
            void executeShowMetrics();       // Execute SHOW METRICS
            void executeShowSqlDialect();    // Execute SHOW SQL DIALECT
            void executeShowVersion();       // Execute SHOW VERSION
            void executeShowDatabase();      // Execute SHOW DATABASE
            void executeShowVariable();      // Execute SHOW variable_name
            void executeShowAll();           // Execute SHOW ALL
            void executeShowTransactionLevel(); // Execute SHOW TRANSACTION ISOLATION LEVEL

            // Schema Navigation Commands
            void executeShowSchemaPath();    // Execute SHOW SCHEMA PATH
            void executeShowSchemaTree();    // Execute SHOW SCHEMA TREE [DEPTH n]
            void executeShowSearchPath();    // Execute SHOW SEARCH PATH
            void executeShowLocation();      // Execute SHOW LOCATION OF [type] name
            void executeShowResolved();      // Execute SHOW RESOLVED name
            void executeShowObjects();       // Execute SHOW OBJECTS
            void executePrepareStatement();  // Execute PREPARE statement
            ExecutionResult executeExecutePrepared();  // Execute prepared statement
            void executeDeallocatePrepared();  // Execute DEALLOCATE PREPARE
            void executeAlterFunctionStatement();  // Execute ALTER FUNCTION
            void executeAlterProcedureStatement();  // Execute ALTER PROCEDURE
            void executeMySqlKill();  // Execute MySQL KILL
            void executeMySqlFlush();  // Execute MySQL FLUSH
            void executeMySqlLockTables();  // Execute MySQL LOCK TABLES
            void executeMySqlUnlockTables();  // Execute MySQL UNLOCK TABLES
            void executeObjectResolverQuery(
                const std::vector<std::pair<std::string, std::string>>& select_items,
                bool is_select_star,
                bool has_where,
                size_t where_start_pc,
                size_t where_end_pc);
            bool executeVirtualCatalogQuery(
                const std::string& table_name,
                const std::vector<std::pair<std::string, std::string>>& select_items,
                bool is_select_star,
                bool has_where,
                size_t where_start_pc,
                size_t where_end_pc);

            // Session SET Commands (Firebird ISQL compatibility)
            void executeSetSqlDialect();     // Execute SET SQL DIALECT n
            void executeSetNames();          // Execute SET NAMES charset_name
            void executeSetLocalTimeout();   // Execute SET LOCAL_TIMEOUT n

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

            // SECURITY ENHANCEMENT (MEDIUM-1): Permission check with cache mode control
            // Use VERIFIED mode for security-critical operations (DROP, DELETE, GRANT, REVOKE)
            bool checkPermission(const core::ID& object_id,
                               core::CatalogManager::PermissionObjectType object_type,
                               uint32_t required_privilege,
                               core::PermissionCheckMode mode);

            // Metadata visibility helper (Plan 03): determine if details should be redacted.
            bool shouldRedactMetadata(const core::ID& object_id,
                                      core::CatalogManager::PermissionObjectType object_type,
                                      const core::ID& owner_id,
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

            // Evaluate expression bytecode with row context and return Value
            Value evaluateExpressionBytecode(const std::vector<uint8_t>& expr_bytecode,
                                             const std::vector<Value>& row_values,
                                             const std::vector<core::CatalogManager::ColumnInfo>& columns,
                                             bool& ok);

            // SECURITY ENHANCEMENT (MEDIUM-3): Query execution limit checks
            void checkQueryLimits();              // Check all query limits, throw if exceeded
            void checkTimeout();                  // Check query timeout
            void checkCancellation();             // NET-M1: Check if query was cancelled, throw if so
            void checkCTEDepth();                 // Check CTE recursion depth
            void incrementCTEDepth();             // Increment CTE depth counter
            void decrementCTEDepth();             // Decrement CTE depth counter
            void trackRowsProcessed(uint64_t count); // Track rows for limit checking

            // ALPHA Phase A: Evaluate DEFAULT value expression for a column
            // For now, supports simple constant defaults (numbers, strings, booleans, NULL)
            // Future: Support function calls like NOW(), CURRENT_USER, etc.
            Value evaluateDefaultValue(const core::CatalogManager::ColumnInfo& column);

            // ALPHA Phase A: Evaluate CHECK constraint for a column
            // Returns true if constraint passes, false if it fails
            bool evaluateCheckConstraint(const core::CatalogManager::ColumnInfo& column,
                                        const std::vector<Value>& row_values,
                                        const std::vector<core::CatalogManager::ColumnInfo>& columns);

            // Table-level CHECK constraint evaluation
            bool evaluateTableCheckConstraint(const core::CatalogManager::ConstraintInfo& constraint,
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

            void enforceUniqueIndexes(const core::ID& table_id,
                                     const std::vector<Value>& row_values,
                                     const std::vector<core::CatalogManager::ColumnInfo>& all_columns,
                                     const std::vector<core::CatalogManager::ConstraintInfo>& table_constraints,
                                     const core::TID* exclude_tid);

            void validateForeignKeyDefinition(const core::ID& child_table_id,
                                             const std::vector<std::string>& child_columns,
                                             const core::ID& parent_table_id,
                                             const std::vector<std::string>& parent_columns,
                                             const std::string& fk_name);

            // ALPHA Phase A: Compare two values for equality (for UNIQUE constraint checking)
            bool valuesEqual(const Value& a, const Value& b);

            // ALPHA Phase A+: Index-based constraint optimization helpers (Nov 19, 2025)
            // Find an index that covers the specified columns (for constraint checking)
            // Returns true if suitable index found, false otherwise
            bool findIndexForColumns(const core::ID& table_id,
                                    const std::vector<core::ID>& column_ids,
                                    core::CatalogManager::IndexInfo& index_out);

            // Fulltext helper: find FULLTEXT index for a single column
            bool findFullTextIndexForColumn(const core::ID& table_id,
                                           const core::ID& column_id,
                                           core::CatalogManager::IndexInfo& index_out);

            // Fulltext helper: extract simple "column @@ query" predicate from bytecode
            bool extractFullTextPredicate(size_t expr_start,
                                         size_t expr_end,
                                         std::string& column_name_out,
                                         size_t& query_start_out,
                                         size_t& query_end_out);

            // Fulltext helper: build FULLTEXT search key from query expression
            bool buildFullTextQueryKey(size_t query_start,
                                      size_t query_end,
                                      std::vector<uint8_t>& key_out);

            // Search an index for matching values (returns TIDs of matching rows)
            // Returns Status::OK if search succeeded, error otherwise
            core::Status searchIndexForValues(const core::CatalogManager::IndexInfo& index_info,
                                            const std::vector<Value>& values,
                                            uint64_t current_xid,
                                            std::vector<core::TID>& tids_out);

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
                                         std::vector<uint8_t>& tuple_data_out,
                                         core::ErrorContext* ctx = nullptr);

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

            // Extraction function
            void executeExtract();       // EXTRACT(field FROM value)
            void executeAlterElement();  // ALTER_ELEMENT(selector IN value TO new_value)

            // Index Operation Executors (November 19, 2025)
            // These methods execute index operations from bytecode
            void executeIndexInsert();   // EXT_INDEX_INSERT - Insert entry into index
            void executeIndexSearch();   // EXT_INDEX_SEARCH - Search index for key
            void executeIndexScan();     // EXT_INDEX_SCAN - Range scan index
            void executeIndexDelete();   // EXT_INDEX_DELETE - Delete from index (MGA logical)

            // Specialized index operations (for indexes with unique APIs)
            void executeGinInsert();     // EXT_GIN_INSERT - GIN insert with key extractor
            void executeGinSearch();     // EXT_GIN_SEARCH - GIN search with key extractor
            void executeHnswInsert();    // EXT_HNSW_INSERT - HNSW insert with vector
            void executeHnswSearch();    // EXT_HNSW_SEARCH - HNSW k-NN search
            void executeColumnstoreInsert();  // EXT_COLUMNSTORE_INSERT - Insert column
            void executeColumnstoreScan();    // EXT_COLUMNSTORE_SCAN - Scan column

            // Index cache helpers (November 19, 2025)
            // Get index from cache or open it. Returns raw pointer (owned by cache if cached).
            // If not in cache, opens index and adds to cache.
            template<typename IndexT>
            IndexT* getOrOpenIndex(const core::ID& index_uuid, IndexType type,
                                  uint64_t root_handle, core::ErrorContext* ctx);

            // Index operation helpers
            core::Status routeIndexInsert(IndexType type, const core::ID& index_uuid,
                                        const std::vector<uint8_t>& key,
                                        const core::TID& tid, uint64_t xmin,
                                        core::ErrorContext* ctx);
            core::Status routeIndexSearch(IndexType type, const core::ID& index_uuid,
                                        const std::vector<uint8_t>& key,
                                        uint64_t current_xid,
                                        std::vector<core::TID>* results_out,
                                        core::ErrorContext* ctx);
            core::Status routeIndexDelete(IndexType type, const core::ID& index_uuid,
                                        const std::vector<uint8_t>& key,
                                        const core::TID& tid, uint64_t xmax,
                                        core::ErrorContext* ctx);
            core::Status routeIndexScan(IndexType type, const core::ID& index_uuid,
                                        const std::vector<uint8_t>* start_key,
                                        const std::vector<uint8_t>* end_key,
                                        bool start_inclusive, bool end_inclusive,
                                        uint64_t current_xid,
                                        std::vector<core::TID>* results_out,
                                        core::ErrorContext* ctx);

        public:
            // Forward declaration
            class TriggerContext;
            class StatementTriggerContext;  // P2-8: Statement-level triggers

            // Trigger procedure type: takes TriggerContext, returns true to continue operation
            using TriggerProcedure = std::function<bool(const TriggerContext&)>;

            // P2-8: Statement-level trigger procedure type
            using StatementTriggerProcedure = std::function<bool(const StatementTriggerContext&)>;

            // Register a trigger procedure for testing
            void registerTriggerProcedure(const std::string& name, TriggerProcedure procedure);

            // P2-8: Register a statement-level trigger procedure
            void registerStatementTriggerProcedure(const std::string& name, StatementTriggerProcedure procedure);

        private:
            // Trigger procedure registry
            std::unordered_map<std::string, TriggerProcedure> trigger_procedures_;

            // P2-8: Statement-level trigger procedure registry
            std::unordered_map<std::string, StatementTriggerProcedure> statement_trigger_procedures_;

            // Fire a trigger for the given context
            bool fireTrigger(const TriggerContext& ctx);

            // P2-8: Fire a statement-level trigger
            bool fireStatementTrigger(const StatementTriggerContext& ctx);
        };

        // ============================================================================
        // Template Implementation: Index Cache Helper
        // ============================================================================

        template<typename IndexT>
        IndexT* Executor::getOrOpenIndex(const core::ID& index_uuid, IndexType type,
                                        uint64_t root_handle, core::ErrorContext* ctx)
        {
            // Try to get from cache first
            void* cached_ptr = index_cache_.get(index_uuid, type);
            if (cached_ptr != nullptr)
            {
                // Cache hit - return cached instance
                return static_cast<IndexT*>(cached_ptr);
            }

            // Cache miss - open index
            std::unique_ptr<IndexT> index_ptr;
            if constexpr (std::is_same_v<IndexT, core::BTree> ||
                          std::is_same_v<IndexT, core::HashIndex> ||
                          std::is_same_v<IndexT, core::GinIndex> ||
                          std::is_same_v<IndexT, core::HnswIndex> ||
                          std::is_same_v<IndexT, core::BitmapIndex> ||
                          std::is_same_v<IndexT, core::BrinIndex> ||
                          std::is_same_v<IndexT, core::GiSTIndex> ||
                          std::is_same_v<IndexT, core::SPGiSTIndex> ||
                          std::is_same_v<IndexT, core::ColumnstoreIndexSimple> ||
                          std::is_same_v<IndexT, core::RTreeIndex>)
            {
                index_ptr = IndexT::open(db_, index_uuid, static_cast<core::GPID>(root_handle), ctx);
            }
            else
            {
                index_ptr = IndexT::open(db_, index_uuid, static_cast<uint32_t>(root_handle), ctx);
            }
            if (!index_ptr)
            {
                return nullptr;
            }

            // Take ownership and add to cache
            IndexT* raw_ptr = index_ptr.release();
            index_cache_.put(index_uuid, type, raw_ptr);

            return raw_ptr;
        }

    } // namespace sblr
} // namespace scratchbird
