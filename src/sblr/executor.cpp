#include "scratchbird/sblr/executor.h"
#include "scratchbird/parser/ast.h"
#include "scratchbird/parser/lexer.h"      // ALPHA Phase 1 - Views: For parsing view definitions
#include "scratchbird/parser/parser.h"     // ALPHA Phase 1 - Views: For parsing view definitions
#include "scratchbird/sblr/bytecode_generator.h"  // ALPHA Phase 1 - Views: For generating view bytecode
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/charset.h"
#include "scratchbird/core/timezone.h"
#include "scratchbird/core/array.h"  // For ARRAY extraction
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/password_hash.h"
#include "scratchbird/core/permission_cache.h"  // Security Phase 3.2.3: Global cache
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/spatial/wkt_parser.h"
#include "scratchbird/spatial/wkb.h"
#include "scratchbird/spatial/geos_wrapper.h"
#include "scratchbird/geo/geodetic.h"
#include "scratchbird/geo/proj_wrapper.h"
#include "scratchbird/geo/srid.h"
#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
#include "scratchbird/core/ts_functions.h"
#include "scratchbird/core/ts_operations.h"
#include "scratchbird/core/expression_serializer.h"
#include "scratchbird/sblr/expression_evaluator.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/gin_index.h"
#include "scratchbird/sblr/gin_extractors.h"  // GIN key extractor registry
#include "scratchbird/core/gist_index.h"
#include "scratchbird/core/spgist_index.h"
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/rtree_index.h"
#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/columnstore_index.h"
#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/logger.h"  // For LOG_ERROR macro
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <map>
#include <unordered_set>
#include <regex>
#include <cctype>
#include <cmath>
#include <openssl/md5.h>
#include <openssl/sha.h>

// libxml2 for full XML/XPath support (Nov 14, 2025)
#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/xmlstring.h>
#endif

using json = nlohmann::json;

namespace scratchbird
{
    namespace sblr
    {
        // ===== JSON Helper Functions =====

        // Parse JSONPath expression ($.field.subfield[0].nested)
        // Returns a list of path components
        static std::vector<std::string> parseJSONPath(const std::string& path) {
            std::vector<std::string> components;

            // Handle empty path
            if (path.empty()) {
                return components;
            }

            // Handle MySQL-style paths: $.field.subfield
            if (path[0] == '$') {
                std::string current;
                bool in_bracket = false;

                for (size_t i = 1; i < path.length(); i++) {
                    char c = path[i];

                    if (c == '.' && !in_bracket) {
                        if (!current.empty()) {
                            components.push_back(current);
                            current.clear();
                        }
                    } else if (c == '[') {
                        if (!current.empty()) {
                            components.push_back(current);
                            current.clear();
                        }
                        in_bracket = true;
                    } else if (c == ']') {
                        if (!current.empty()) {
                            components.push_back(current);
                            current.clear();
                        }
                        in_bracket = false;
                    } else {
                        current += c;
                    }
                }

                if (!current.empty()) {
                    components.push_back(current);
                }
            } else {
                // Simple field name
                components.push_back(path);
            }

            return components;
        }

        // Extract value from JSON using path components
        static json extractJSONValue(const json& j, const std::vector<std::string>& path) {
            json current = j;

            for (const auto& component : path) {
                // Try as array index first
                try {
                    size_t idx = std::stoull(component);
                    if (current.is_array() && idx < current.size()) {
                        current = current[idx];
                        continue;
                    }
                } catch (...) {
                    // Not a number, treat as object key
                }

                // Try as object key
                if (current.is_object() && current.contains(component)) {
                    current = current[component];
                } else {
                    // Path not found, return null
                    return json();
                }
            }

            return current;
        }

        // Convert Value to json
        static json valueToJSON(const Value& val) {
            if (val.isNull()) {
                return json();
            }

            switch (val.type()) {
                case core::DataType::INT8:
                case core::DataType::INT16:
                case core::DataType::INT32:
                case core::DataType::INT64:
                    return json(val.toInt64());

                case core::DataType::UINT8:
                case core::DataType::UINT16:
                case core::DataType::UINT32:
                case core::DataType::UINT64:
                    return json(static_cast<uint64_t>(val.toInt64()));

                case core::DataType::FLOAT32:
                case core::DataType::FLOAT64:
                    return json(val.toDouble());

                case core::DataType::BOOLEAN:
                    return json(val.toInt64() != 0);

                case core::DataType::VARCHAR:
                case core::DataType::TEXT:
                case core::DataType::CHAR:
                    return json(val.toString());

                case core::DataType::JSON:
                    // Parse existing JSON
                    try {
                        return json::parse(val.toString());
                    } catch (...) {
                        return json();
                    }

                default:
                    // For other types, convert to string
                    return json(val.toString());
            }
        }

        // Convert json to Value
        static Value jsonToValue(const json& j, bool as_text = false) {
            if (j.is_null()) {
                return Value::makeNull();
            }

            if (as_text) {
                // Return as text string
                return Value::makeText(j.dump());
            } else {
                // Return as JSON type
                return Value::makeJSON(j.dump());
            }
        }

        // ===== Value Implementation =====
        // Value is now an alias for core::TypedValue, so no implementation needed here

        // ===== TriggerContext Implementation =====
        // Wave 2: Trigger Executor Implementation

        class Executor::TriggerContext
        {
        public:
            TriggerContext(
                const core::CatalogManager::TriggerInfo& trigger,
                const std::vector<Value>* old_row,
                const std::vector<Value>* new_row,
                const core::CatalogManager::TableInfo& table_info,
                const std::vector<core::CatalogManager::ColumnInfo>& columns
            ) : trigger_(trigger), old_row_(old_row), new_row_(new_row),
                table_info_(table_info), columns_(columns) {}

            const core::CatalogManager::TriggerInfo& trigger() const { return trigger_; }
            const core::CatalogManager::TableInfo& tableInfo() const { return table_info_; }

            // Access OLD.column_name
            Value getOldValue(const std::string& column_name) const
            {
                if (!old_row_) return Value::makeNull();
                size_t col_idx = findColumnIndex(column_name);
                if (col_idx == static_cast<size_t>(-1)) return Value::makeNull();
                return (*old_row_)[col_idx];
            }

            // Access NEW.column_name
            Value getNewValue(const std::string& column_name) const
            {
                if (!new_row_) return Value::makeNull();
                size_t col_idx = findColumnIndex(column_name);
                if (col_idx == static_cast<size_t>(-1)) return Value::makeNull();
                return (*new_row_)[col_idx];
            }

        private:
            const core::CatalogManager::TriggerInfo& trigger_;
            const std::vector<Value>* old_row_;
            const std::vector<Value>* new_row_;
            const core::CatalogManager::TableInfo& table_info_;
            const std::vector<core::CatalogManager::ColumnInfo>& columns_;

            size_t findColumnIndex(const std::string& column_name) const
            {
                for (size_t i = 0; i < columns_.size(); i++)
                {
                    if (columns_[i].column_name == column_name)
                    {
                        return i;
                    }
                }
                return static_cast<size_t>(-1);  // Not found
            }
        };

        // ===== ResultSet Implementation =====

        void ResultSet::addColumn(const std::string &name, core::DataType type)
        {
            column_names_.push_back(name);
            column_types_.push_back(type);
        }

        void ResultSet::addRow(std::vector<Value> row)
        {
            if (row.size() != column_names_.size())
            {
                throw std::runtime_error("Row column count mismatch");
            }
            rows_.push_back(std::move(row));
        }

        void ResultSet::print(std::ostream &out) const
        {
            // Print column headers
            for (size_t i = 0; i < column_names_.size(); i++)
            {
                if (i > 0)
                    out << " | ";
                out << std::setw(15) << column_names_[i];
            }
            out << "\n";

            // Print separator
            for (size_t i = 0; i < column_names_.size(); i++)
            {
                if (i > 0)
                    out << "-+-";
                out << std::string(15, '-');
            }
            out << "\n";

            // Print rows
            for (const auto &row : rows_)
            {
                for (size_t i = 0; i < row.size(); i++)
                {
                    if (i > 0)
                        out << " | ";
                    out << std::setw(15) << row[i].toString();
                }
                out << "\n";
            }

            out << "(" << rows_.size() << " rows)\n";
        }

        // ===== Executor Implementation =====

        Executor::Executor(core::Database *db) : db_(db), pc_(0)
        {
            if (!db_)
            {
                throw std::invalid_argument("Database pointer cannot be null");
            }

            // SECURITY ENHANCEMENT (MEDIUM-3): Initialize query limits with defaults
            query_limits_ = QueryLimits::defaults();
            query_start_time_ = std::chrono::steady_clock::now();
            cte_recursion_depth_ = 0;
            rows_processed_ = 0;
        }

        Executor::~Executor() = default;

        ExecutionResult Executor::execute(const std::vector<uint8_t> &bytecode)
        {
            // Reset execution state
            // IMPORTANT: bytecode_ stores a raw pointer to the input vector's data.
            // The caller MUST ensure the bytecode vector remains valid for the
            // duration of this execute() call. This is safe because we take a const
            // reference and complete execution within this function.
            bytecode_ = bytecode.data();
            bytecode_size_ = bytecode.size();
            pc_ = 0;

            // Clear stack efficiently by replacing with empty stack
            stack_ = std::stack<Value>();

            current_table_.clear();
            current_columns_.clear();
            current_result_set_.reset();

            // SECURITY ENHANCEMENT (MEDIUM-3): Reset query limits for new execution
            query_start_time_ = std::chrono::steady_clock::now();
            cte_recursion_depth_ = 0;
            rows_processed_ = 0;

            // Statement snapshot management for READ_COMMITTED_READ_CONSISTENCY
            core::ConnectionContext *conn_ctx = core::ConnectionContext::getCurrent();
            bool created_stmt_snapshot = false;

            try
            {
                // Check version
                if (readByte() != static_cast<uint8_t>(Opcode::VERSION))
                {
                    return ExecutionResult("Invalid bytecode: missing version");
                }

                uint8_t version = readByte();
                if (version != SBLR_VERSION)
                {
                    return ExecutionResult("Unsupported bytecode version: " +
                                           std::to_string(version));
                }

                // Create statement snapshot for READ_COMMITTED_READ_CONSISTENCY
                if (conn_ctx && conn_ctx->getIsolationLevel() ==
                                    core::IsolationLevel::READ_COMMITTED_READ_CONSISTENCY)
                {
                    // Create statement XID for consistent reads within this statement
                    conn_ctx->createStatementXID();
                    created_stmt_snapshot = true;
                    // Non-fatal if snapshot creation fails - fall back to READ COMMITTED semantics
                }

                // Execute main statement
                Opcode op = static_cast<Opcode>(readByte());
                ExecutionResult result;

                switch (op)
                {
                    case Opcode::CREATE_TABLE:
                        executeCreateTable();
                        result = ExecutionResult();
                        break;

                    case Opcode::CREATE_INDEX:
                        executeCreateIndex();
                        result = ExecutionResult();
                        break;

                    case Opcode::CREATE_TABLESPACE:
                        executeCreateTablespace();
                        result = ExecutionResult();
                        break;

                    case Opcode::ALTER_TABLESPACE:
                        executeAlterTablespace();
                        result = ExecutionResult();
                        break;

                    case Opcode::ALTER_TABLE_SET_TABLESPACE:
                        executeAlterTableSetTablespace();
                        result = ExecutionResult();
                        break;

                    case Opcode::DROP_TABLE:
                        executeDropTable();
                        result = ExecutionResult();
                        break;

                    case Opcode::DROP_INDEX:
                        executeDropIndex();
                        result = ExecutionResult();
                        break;

                    case Opcode::ALTER_TABLE:
                        executeAlterTable();
                        result = ExecutionResult();
                        break;

                    case Opcode::TRUNCATE_TABLE:
                        executeTruncateTable();
                        result = ExecutionResult();
                        break;

                    case Opcode::CREATE_SEQUENCE:
                        executeCreateSequence();
                        result = ExecutionResult();
                        break;

                    case Opcode::ALTER_SEQUENCE:
                        executeAlterSequence();
                        result = ExecutionResult();
                        break;

                    case Opcode::DROP_SEQUENCE:
                        executeDropSequence();
                        result = ExecutionResult();
                        break;

                    case Opcode::CREATE_VIEW:
                        executeCreateView();
                        result = ExecutionResult();
                        break;

                    case Opcode::DROP_VIEW:
                        executeDropView();
                        result = ExecutionResult();
                        break;

                    case Opcode::REFRESH_MATERIALIZED_VIEW:  // ALPHA Phase 1 - Materialized Views
                        executeRefreshMaterializedView();
                        result = ExecutionResult();
                        break;

                    case Opcode::DROP_TABLESPACE:
                        executeDropTablespace();
                        result = ExecutionResult();
                        break;

                    case Opcode::ATTACH_TABLESPACE:
                        executeAttachTablespace();
                        result = ExecutionResult();
                        break;

                    case Opcode::DETACH_TABLESPACE:
                        executeDetachTablespace();
                        result = ExecutionResult();
                        break;

                    case Opcode::INSERT:
                        executeInsert();
                        result = ExecutionResult();
                        break;

                    case Opcode::UPDATE:
                        executeUpdate();
                        result = ExecutionResult();
                        break;

                    case Opcode::DELETE:
                        executeDelete();
                        result = ExecutionResult();
                        break;

                    case Opcode::SELECT:
                        executeSelect();
                        result = ExecutionResult(std::move(current_result_set_));
                        break;

                    case Opcode::NESTED_LOOP_JOIN:
                        executeNestedLoopJoin();
                        result = ExecutionResult(std::move(current_result_set_));
                        break;

                    case Opcode::HASH_JOIN:
                        executeHashJoin();
                        result = ExecutionResult(std::move(current_result_set_));
                        break;

                    case Opcode::SWEEP:
                        executeSweep();
                        result = ExecutionResult();
                        break;

                    case Opcode::START_TRANSACTION:
                        executeStartTransaction();
                        result = ExecutionResult();
                        break;

                    case Opcode::SET_TRANSACTION:
                        executeSetTransaction();
                        result = ExecutionResult();
                        break;

                    case Opcode::COMMIT:
                        executeCommit();
                        result = ExecutionResult();
                        break;

                    case Opcode::ROLLBACK:
                        executeRollback();
                        result = ExecutionResult();
                        break;

                    case Opcode::EXTENDED_OPCODE:
                    {
                        uint8_t ext_op = readByte();

                        if (ext_op == static_cast<uint8_t>(Opcode::EXT_WITH_CLAUSE))
                        {
                            // Phase 2 Wave 2: Handle WITH clause (CTEs)
                            uint16_t cte_count = readInt16();
                            // Read recursive flag
                            uint8_t is_recursive = readByte();
                            cte_is_recursive_ = (is_recursive != 0);
                            // Clear any previous CTE results
                            cte_results_.clear();
                            cte_column_names_.clear();
                            cte_column_types_.clear();
                            // CTEs will be materialized by subsequent EXT_CTE_DEF opcodes
                            // Continue execution without breaking
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_CTE_DEF))
                        {
                            // Phase 2 Wave 2: Define and materialize a CTE
                            std::string cte_name = readString();

                            // Save current state
                            auto saved_result_set = std::move(current_result_set_);
                            auto saved_table = current_table_;
                            size_t saved_pc = pc_;  // Save program counter for recursive execution

                            // Execute the CTE query (next opcodes)
                            current_result_set_ = std::make_unique<ResultSet>();

                            // Read and execute the nested SELECT or UNION ALL
                            Opcode cte_query_op = static_cast<Opcode>(readByte());

                            // Check if this is a recursive CTE with UNION ALL
                            if (cte_is_recursive_ && cte_query_op == Opcode::EXTENDED_OPCODE)
                            {
                                uint8_t ext_opcode = readByte();
                                if (ext_opcode == static_cast<uint8_t>(Opcode::EXT_UNION_ALL))
                                {
                                    // Recursive CTE: Execute iteratively
                                    executeRecursiveCTE(cte_name, saved_pc);
                                }
                                else
                                {
                                    error("Recursive CTE must use UNION ALL");
                                }
                            }
                            else if (cte_query_op == Opcode::SELECT)
                            {
                                // Non-recursive CTE: Execute once
                                executeSelect();

                                // Store the materialized CTE results
                                if (current_result_set_)
                                {
                                    // Extract column metadata
                                    std::vector<std::string> col_names;
                                    std::vector<core::DataType> col_types;
                                    for (size_t i = 0; i < current_result_set_->columnCount(); ++i)
                                    {
                                        col_names.push_back(current_result_set_->columnName(i));
                                        col_types.push_back(current_result_set_->columnType(i));
                                    }

                                    // Extract all rows
                                    std::vector<std::vector<Value>> rows;
                                    for (size_t r = 0; r < current_result_set_->rowCount(); ++r)
                                    {
                                        std::vector<Value> row;
                                        for (size_t c = 0; c < current_result_set_->columnCount(); ++c)
                                        {
                                            row.push_back(current_result_set_->getValue(r, c));
                                        }
                                        rows.push_back(std::move(row));
                                    }

                                    // Store CTE results
                                    cte_results_[cte_name] = std::move(rows);
                                    cte_column_names_[cte_name] = std::move(col_names);
                                    cte_column_types_[cte_name] = std::move(col_types);
                                }
                            }
                            else
                            {
                                result = ExecutionResult("CTE query must be a SELECT statement or UNION ALL");
                                break;
                            }

                            // Restore state
                            current_result_set_ = std::move(saved_result_set);
                            current_table_ = saved_table;
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_CTE_SCAN))
                        {
                            // Phase 2 Wave 2: Scan a CTE (replaces table scan)
                            std::string cte_name = readString();

                            // Look up CTE results
                            auto it = cte_results_.find(cte_name);
                            if (it == cte_results_.end())
                            {
                                result = ExecutionResult("CTE '" + cte_name + "' not found");
                                break;
                            }

                            // Set up result set with CTE data
                            if (!current_result_set_)
                            {
                                current_result_set_ = std::make_unique<ResultSet>();
                            }

                            // Add columns from CTE
                            const auto& col_names = cte_column_names_[cte_name];
                            const auto& col_types = cte_column_types_[cte_name];
                            for (size_t i = 0; i < col_names.size(); ++i)
                            {
                                current_result_set_->addColumn(col_names[i], col_types[i]);
                            }

                            // Add all rows from CTE
                            for (const auto& row : it->second)
                            {
                                current_result_set_->addRow(row);
                            }
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_UNION_ALL))
                        {
                            // Execute UNION ALL: concatenate results from left and right
                            executeUnionAll();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_UNION))
                        {
                            // Execute UNION: concatenate and remove duplicates
                            executeUnion();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_INTERSECT_ALL))
                        {
                            // Execute INTERSECT ALL: keep common rows with duplicates
                            executeIntersectAll();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_INTERSECT))
                        {
                            // Execute INTERSECT: keep common rows without duplicates
                            executeIntersect();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_EXCEPT_ALL))
                        {
                            // Execute EXCEPT ALL: rows in left but not in right, with duplicates
                            executeExceptAll();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_EXCEPT))
                        {
                            // Execute EXCEPT: rows in left but not in right, without duplicates
                            executeExcept();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_CREATE_TRIGGER))
                        {
                            executeCreateTrigger();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_DROP_TRIGGER))
                        {
                            executeDropTrigger();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_SCALAR) ||
                                 ext_op == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_EXISTS) ||
                                 ext_op == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_IN) ||
                                 ext_op == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_NOT_IN))
                        {
                            // Phase 2 Wave 2 - Agent B: Subquery execution
                            // These opcodes should not appear at statement level - they are expression-level
                            result = ExecutionResult("Subquery opcodes must appear within expressions, not at statement level");
                        }
                        // ===== PSQL - Stored Procedures and Functions (Phase 2 Task 10.2, Phase 4-5) =====
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNCTION))
                        {
                            executeFunction();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_PROCEDURE))
                        {
                            executeProcedure();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_BLOCK))
                        {
                            executeBlock();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_DECLARE))
                        {
                            executeVarDeclaration();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ASSIGN))
                        {
                            executeAssignment();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_IF))
                        {
                            executeIfStatement();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_LOOP))
                        {
                            executeLoopStatement();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_WHILE))
                        {
                            executeWhileStatement();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_EXIT))
                        {
                            executeExitStatement();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_RETURN))
                        {
                            executeReturnStatement();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_RAISE))
                        {
                            executeRaiseStatement();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_TRY))
                        {
                            executeTryStatement();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_EXCEPT_HANDLER))
                        {
                            executeExceptHandler();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_VAR_LOAD))
                        {
                            executeVarLoad();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_VAR_STORE))
                        {
                            executeVarStore();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_JUMP))
                        {
                            executeJump();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_JUMP_IF_TRUE))
                        {
                            executeJumpIfTrue();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_JUMP_IF_FALSE))
                        {
                            executeJumpIfFalse();
                            result = ExecutionResult();
                        }
                        // ===== Spatial SRID Functions (Phase 2 Task 9.5) =====
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_SRID))
                        {
                            // ST_SRID(geom) - get SRID of geometry
                            Value geom = pop();

                            if (geom.isNull())
                            {
                                push(Value::makeNull());
                            }
                            else
                            {
                                int32_t srid = 0;
                                if (geom.type() == core::DataType::POINT)
                                {
                                    srid = geom.getPoint().getSRID();
                                }
                                else if (geom.type() == core::DataType::LINESTRING)
                                {
                                    srid = geom.getLineString().getSRID();
                                }
                                else if (geom.type() == core::DataType::POLYGON)
                                {
                                    srid = geom.getPolygon().getSRID();
                                }
                                else
                                {
                                    push(Value::makeNull());
                                    result = ExecutionResult();
                                    break;
                                }

                                push(Value::makeInt32(srid));
                            }
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_SETSRID))
                        {
                            // ST_SetSRID(geom, srid) - set SRID of geometry
                            Value srid_val = pop();
                            Value geom = pop();

                            if (geom.isNull() || srid_val.isNull())
                            {
                                push(Value::makeNull());
                            }
                            else
                            {
                                int32_t new_srid = static_cast<int32_t>(srid_val.toInt64());

                                // Create copy with new SRID (don't transform coordinates!)
                                if (geom.type() == core::DataType::POINT)
                                {
                                    core::Point pt = geom.getPoint();
                                    pt.setSRID(new_srid);
                                    push(Value::makePoint(pt));
                                }
                                else if (geom.type() == core::DataType::LINESTRING)
                                {
                                    core::LineString line = geom.getLineString();
                                    line.setSRID(new_srid);
                                    push(Value::makeLineString(line));
                                }
                                else if (geom.type() == core::DataType::POLYGON)
                                {
                                    core::Polygon poly = geom.getPolygon();
                                    poly.setSRID(new_srid);
                                    push(Value::makePolygon(poly));
                                }
                                else
                                {
                                    push(Value::makeNull());
                                }
                            }
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_TRANSFORM))
                        {
                            // ST_Transform(geom, target_srid) - transform to different SRID
                            Value target_srid_val = pop();
                            Value geom = pop();

                            if (geom.isNull() || target_srid_val.isNull())
                            {
                                push(Value::makeNull());
                                result = ExecutionResult();
                            }
                            else
                            {
                                int32_t target_srid = static_cast<int32_t>(target_srid_val.toInt64());

#ifdef HAVE_PROJ
                                try
                                {
                                    if (geom.type() == core::DataType::POINT)
                                    {
                                        core::Point pt = geom.getPoint();
                                        int32_t source_srid = pt.getSRID();

                                        if (source_srid == 0)
                                        {
                                            error("ST_Transform: geometry must have SRID");
                                        }
                                        if (source_srid == target_srid)
                                        {
                                            // No transformation needed
                                            push(geom);
                                        }
                                        else
                                        {
                                            // Transform coordinates
                                            geo::PROJTransform transform(source_srid, target_srid);
                                            double x = pt.x;
                                            double y = pt.y;
                                            transform.transform(x, y);

                                            // Create new point with transformed coordinates
                                            core::Point transformed(x, y, target_srid);
                                            push(Value::makePoint(transformed));
                                        }
                                    }
                                    else if (geom.type() == core::DataType::LINESTRING)
                                    {
                                        core::LineString line = geom.getLineString();
                                        int32_t source_srid = line.getSRID();

                                        if (source_srid == 0)
                                        {
                                            error("ST_Transform: geometry must have SRID");
                                        }
                                        if (source_srid == target_srid)
                                        {
                                            push(geom);
                                        }
                                        else
                                        {
                                            // Transform all points
                                            geo::PROJTransform transform(source_srid, target_srid);
                                            std::vector<core::Point> transformed_points;

                                            for (const auto &pt : line.points)
                                            {
                                                double x = pt.x;
                                                double y = pt.y;
                                                transform.transform(x, y);
                                                transformed_points.emplace_back(x, y, target_srid);
                                            }

                                            core::LineString transformed(std::move(transformed_points), target_srid);
                                            push(Value::makeLineString(transformed));
                                        }
                                    }
                                    else if (geom.type() == core::DataType::POLYGON)
                                    {
                                        core::Polygon poly = geom.getPolygon();
                                        int32_t source_srid = poly.getSRID();

                                        if (source_srid == 0)
                                        {
                                            error("ST_Transform: geometry must have SRID");
                                        }
                                        if (source_srid == target_srid)
                                        {
                                            push(geom);
                                        }
                                        else
                                        {
                                            // Transform all rings
                                            geo::PROJTransform transform(source_srid, target_srid);
                                            std::vector<std::vector<core::Point>> transformed_rings;

                                            for (const auto &ring : poly.rings)
                                            {
                                                std::vector<core::Point> transformed_ring;
                                                for (const auto &pt : ring)
                                                {
                                                    double x = pt.x;
                                                    double y = pt.y;
                                                    transform.transform(x, y);
                                                    transformed_ring.emplace_back(x, y, target_srid);
                                                }
                                                transformed_rings.push_back(std::move(transformed_ring));
                                            }

                                            core::Polygon transformed(std::move(transformed_rings), target_srid);
                                            push(Value::makePolygon(transformed));
                                        }
                                    }
                                    else
                                    {
                                        push(Value::makeNull());
                                    }
                                }
                                catch (const geo::PROJException &e)
                                {
                                    // SECURITY FIX (HIGH-1): Log detailed error internally, return generic error to client
                                    LOG_ERROR(EXECUTOR, "ST_Transform failed: %s", e.what());
                                    error("Spatial transformation failed");
                                }
#else
                                error("ST_Transform requires PROJ library (not available)");
#endif
                                result = ExecutionResult();
                            }
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_DISTANCE_SPHERE))
                        {
                            // ST_Distance_Sphere(geom1, geom2) - geodetic distance (always uses Haversine)
                            Value geom2 = pop();
                            Value geom1 = pop();

                            if (geom1.isNull() || geom2.isNull())
                            {
                                push(Value::makeNull());
                            }
                            else
                            {
                                // Only works with points
                                if (geom1.type() != core::DataType::POINT || geom2.type() != core::DataType::POINT)
                                {
                                    error("ST_Distance_Sphere only works with POINT geometries");
                                }

                                core::Point pt1 = geom1.getPoint();
                                core::Point pt2 = geom2.getPoint();

                                // Use Haversine formula (assumes geographic coordinates in degrees)
                                double distance = geo::Geodetic::haversineDistance(pt1.x, pt1.y, pt2.x, pt2.y);

                                push(Value::makeFloat64(distance));
                            }
                            result = ExecutionResult();
                        }
                        // ===== Security Statements (ALPHA Phase 1 - Security System Phase 2) =====
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_CREATE_USER))
                        {
                            executeCreateUser();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ALTER_USER))
                        {
                            executeAlterUser();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_DROP_USER))
                        {
                            executeDropUser();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_CREATE_ROLE))
                        {
                            executeCreateRole();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_DROP_ROLE))
                        {
                            executeDropRole();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_CREATE_GROUP))
                        {
                            executeCreateGroup();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_DROP_GROUP))
                        {
                            executeDropGroup();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_GRANT_PRIVILEGE))
                        {
                            executeGrantPrivilege();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REVOKE_PRIVILEGE))
                        {
                            executeRevokePrivilege();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_GRANT_ROLE))
                        {
                            executeGrantRole();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REVOKE_ROLE))
                        {
                            executeRevokeRole();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SET_ROLE))
                        {
                            executeSetRole();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SET_SESSION_AUTH))
                        {
                            executeSetSessionAuth();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_CREATE_POLICY))
                        {
                            executeCreatePolicy();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_DROP_POLICY))
                        {
                            executeDropPolicy();
                            result = ExecutionResult();
                        }
                        else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ALTER_TABLE_RLS))
                        {
                            executeAlterTableRLS();
                            result = ExecutionResult();
                        }
                        else
                        {
                            result = ExecutionResult("Unknown extended opcode: " +
                                                     std::to_string(static_cast<int>(ext_op)));
                        }
                        break;
                    }

                    default:
                        result = ExecutionResult("Unknown statement opcode: " +
                                                 std::to_string(static_cast<int>(op)));
                        break;
                }

                // Clear statement snapshot after successful execution
                if (created_stmt_snapshot && conn_ctx)
                {
                    conn_ctx->clearStatementXID();
                }

                return result;
            }
            catch (const std::exception &e)
            {
                // Clear statement snapshot on error
                if (created_stmt_snapshot && conn_ctx)
                {
                    conn_ctx->clearStatementXID();
                }
                return ExecutionResult(std::string("Execution error: ") + e.what());
            }
        }

        uint8_t Executor::readByte()
        {
            if (pc_ >= bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            return bytecode_[pc_++];
        }

        uint16_t Executor::readInt16()
        {
            if (pc_ + 2 > bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            uint16_t value = sblr::readInt16(&bytecode_[pc_]);
            pc_ += 2;
            return value;
        }

        uint32_t Executor::readInt32()
        {
            if (pc_ + 4 > bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            uint32_t value = sblr::readInt32(&bytecode_[pc_]);
            pc_ += 4;
            return value;
        }

        uint64_t Executor::readInt64()
        {
            if (pc_ + 8 > bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            uint64_t value = sblr::readInt64(&bytecode_[pc_]);
            pc_ += 8;
            return value;
        }

        double Executor::readDouble()
        {
            if (pc_ + 8 > bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            // Read as little-endian 64-bit value, then convert to double
            uint64_t bits = sblr::readInt64(&bytecode_[pc_]);
            double value;
            std::memcpy(&value, &bits, sizeof(double));
            pc_ += 8;
            return value;
        }

        std::string Executor::readString()
        {
            uint32_t length = readInt32();

            // Validate reasonable string length (prevent malicious huge allocations)
            // Maximum reasonable string: 16MB
            constexpr uint32_t MAX_STRING_LENGTH = 16 * 1024 * 1024;
            if (length > MAX_STRING_LENGTH)
            {
                throw std::runtime_error("String length exceeds maximum allowed (16MB)");
            }

            if (pc_ + length > bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            std::string str(reinterpret_cast<const char *>(&bytecode_[pc_]), length);
            pc_ += length;
            return str;
        }

        Value Executor::pop()
        {
            if (stack_.empty())
            {
                throw std::runtime_error("Stack underflow");
            }
            Value v = stack_.top();
            stack_.pop();
            return v;
        }

        // Helper: Convert parser::DataType to core::DataType
        static core::DataType convertDataType(Opcode type_opcode, uint32_t precision = 0)
        {
            switch (type_opcode)
            {
                // Integer types
                case Opcode::TYPE_INT8:
                    return core::DataType::INT8;
                case Opcode::TYPE_INT16:
                    return core::DataType::INT16;
                case Opcode::TYPE_INTEGER:
                    return core::DataType::INT32;
                case Opcode::TYPE_BIGINT:
                    return core::DataType::INT64;

                // Floating point types
                case Opcode::TYPE_FLOAT32:
                    return core::DataType::FLOAT32;
                case Opcode::TYPE_DOUBLE:
                    return core::DataType::FLOAT64;

                // Boolean
                case Opcode::TYPE_BOOLEAN:
                    return core::DataType::BOOLEAN;

                // String types
                case Opcode::TYPE_CHAR:
                    return core::DataType::CHAR;
                case Opcode::TYPE_VARCHAR:
                    return core::DataType::VARCHAR;
                case Opcode::TYPE_TEXT:
                    return core::DataType::TEXT;

                // Date/Time types
                case Opcode::TYPE_DATE:
                    return core::DataType::DATE;
                case Opcode::TYPE_TIME:
                    return core::DataType::TIME;
                case Opcode::TYPE_TIMESTAMP:
                    return core::DataType::TIMESTAMP;

                // Binary types
                case Opcode::TYPE_BINARY:
                    return core::DataType::BINARY;
                case Opcode::TYPE_VARBINARY:
                    return core::DataType::VARBINARY;
                case Opcode::TYPE_BLOB:
                    return core::DataType::BLOB;
                case Opcode::TYPE_BYTEA:
                    return core::DataType::BYTEA;

                // Other types
                case Opcode::TYPE_UUID:
                    return core::DataType::UUID;
                case Opcode::TYPE_DECIMAL:
                    return core::DataType::DECIMAL;
                case Opcode::TYPE_JSON:
                    return core::DataType::JSON;

                default:
                    throw std::runtime_error("Unknown data type opcode");
            }
        }

        void Executor::executeCreateTable()
        {
            // Read TABLE_REF opcode
            if (readByte() != static_cast<uint8_t>(Opcode::TABLE_REF))
            {
                error("Expected TABLE_REF in CREATE TABLE");
            }

            std::string table_name = readString();

            // Read BEGIN_LIST opcode for columns
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for columns");
            }

            uint32_t column_count = readInt32();

            // Read column definitions
            std::vector<core::CatalogManager::ColumnInfo> columns;

            // Track FK constraints to create after table (ALPHA Phase A - FK Constraints)
            struct PendingFK {
                std::vector<std::string> child_columns;  // Changed to vector for composite FK support (Phase C)
                std::string parent_table;
                std::vector<std::string> parent_columns;
                std::string on_delete_action;
                std::string on_update_action;
            };
            std::vector<PendingFK> pending_fks;

            for (uint32_t i = 0; i < column_count; i++)
            {
                // Read COLUMN_DEF opcode
                if (readByte() != static_cast<uint8_t>(Opcode::COLUMN_DEF))
                {
                    error("Expected COLUMN_DEF");
                }

                // Read COLUMN_REF (column name)
                if (readByte() != static_cast<uint8_t>(Opcode::COLUMN_REF))
                {
                    error("Expected COLUMN_REF in column definition");
                }
                std::string col_name = readString();

                // Read data type
                Opcode type_op = static_cast<Opcode>(readByte());
                uint32_t precision = 0;
                if (type_op == Opcode::TYPE_VARCHAR)
                {
                    precision = readInt32();
                }

                core::DataType col_type = convertDataType(type_op, precision);

                // Check for NOT_NULL constraint
                bool nullable = true;
                if (pc_ < bytecode_size_ &&
                    bytecode_[pc_] == static_cast<uint8_t>(Opcode::NOT_NULL))
                {
                    nullable = false;
                    readByte(); // Consume NOT_NULL opcode
                }

                // Check for DEFAULT expression (ALPHA Phase A - Constraint Enforcement)
                std::string default_expr_hex;
                if (pc_ < bytecode_size_ &&
                    bytecode_[pc_] == static_cast<uint8_t>(Opcode::DEFAULT_VALUE))
                {
                    readByte(); // Consume DEFAULT_VALUE opcode

                    // Read bytecode length
                    uint32_t bytecode_len = readInt32();

                    // Read bytecode and convert to hex string
                    std::vector<uint8_t> bytecode_data;
                    bytecode_data.reserve(bytecode_len);
                    for (uint32_t j = 0; j < bytecode_len; j++)
                    {
                        bytecode_data.push_back(readByte());
                    }

                    // Convert bytecode to hex string for storage
                    std::stringstream ss;
                    for (uint8_t byte : bytecode_data)
                    {
                        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                    }
                    default_expr_hex = ss.str();
                }

                // Check for CHECK constraint expression (ALPHA Phase A - Constraint Enforcement)
                std::string check_expr_hex;
                if (pc_ < bytecode_size_ &&
                    bytecode_[pc_] == static_cast<uint8_t>(Opcode::CHECK_CONSTRAINT))
                {
                    readByte(); // Consume CHECK_CONSTRAINT opcode

                    // Read bytecode length
                    uint32_t bytecode_len = readInt32();

                    // Read bytecode and convert to hex string
                    std::vector<uint8_t> bytecode_data;
                    bytecode_data.reserve(bytecode_len);
                    for (uint32_t j = 0; j < bytecode_len; j++)
                    {
                        bytecode_data.push_back(readByte());
                    }

                    // Convert bytecode to hex string for storage
                    std::stringstream ss;
                    for (uint8_t byte : bytecode_data)
                    {
                        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                    }
                    check_expr_hex = ss.str();
                }

                // Check for FOREIGN KEY constraint (ALPHA Phase A - FK Constraints)
                if (pc_ < bytecode_size_ &&
                    bytecode_[pc_] == static_cast<uint8_t>(Opcode::FOREIGN_KEY))
                {
                    readByte(); // Consume FOREIGN_KEY opcode

                    PendingFK fk;
                    fk.child_columns.push_back(col_name);  // Single column FK (Phase C: use vector)

                    // Read parent table name
                    fk.parent_table = readString();

                    // Read parent column count
                    uint8_t parent_col_count = readByte();

                    // Read parent column names
                    for (uint8_t j = 0; j < parent_col_count; j++)
                    {
                        fk.parent_columns.push_back(readString());
                    }

                    // If no parent columns specified, use same column name as child
                    if (fk.parent_columns.empty())
                    {
                        fk.parent_columns.push_back(col_name);
                    }

                    // Read ON DELETE action
                    fk.on_delete_action = readString();

                    // Read ON UPDATE action
                    fk.on_update_action = readString();

                    pending_fks.push_back(fk);
                }

                // Build ColumnInfo (table_id and column_id will be set by catalog)
                core::CatalogManager::ColumnInfo col_info;
                col_info.column_name = col_name;
                col_info.data_type = static_cast<uint16_t>(col_type);
                col_info.max_length = precision;
                col_info.nullable = nullable;
                col_info.has_default = !default_expr_hex.empty();
                col_info.default_expr = default_expr_hex; // Store DEFAULT expression hex bytecode
                col_info.check_expr = check_expr_hex;     // Store CHECK expression hex bytecode
                columns.push_back(col_info);
            }

            // Read END_LIST opcode
            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST after columns");
            }

            // Check for table-level FK constraints (ALPHA Phase C - Composite FK Support)
            while (pc_ < bytecode_size_ &&
                   bytecode_[pc_] == static_cast<uint8_t>(Opcode::TABLE_FK))
            {
                readByte(); // Consume TABLE_FK opcode

                PendingFK fk;

                // Read child column count and names
                uint8_t child_col_count = readByte();
                for (uint8_t j = 0; j < child_col_count; j++)
                {
                    fk.child_columns.push_back(readString());
                }

                // Read parent table name
                fk.parent_table = readString();

                // Read parent column count and names
                uint8_t parent_col_count = readByte();
                for (uint8_t j = 0; j < parent_col_count; j++)
                {
                    fk.parent_columns.push_back(readString());
                }

                // Read ON DELETE action
                fk.on_delete_action = readString();

                // Read ON UPDATE action
                fk.on_update_action = readString();

                // Read constraint name (optional)
                std::string constraint_name = readString();
                (void)constraint_name; // TODO: Use constraint name instead of auto-generated

                pending_fks.push_back(fk);
            }

            // Read tablespace name (Phase 2 Task 2.3)
            std::string tablespace_name = readString();
            uint16_t tablespace_id = 0; // Default tablespace

            // If tablespace name is provided, resolve it to tablespace_id
            if (!tablespace_name.empty())
            {
                core::TablespaceInfo ts_info;
                auto ts_status = db_->catalog_manager()->getTablespaceByName(tablespace_name, ts_info, nullptr);
                if (ts_status != core::Status::OK)
                {
                    error("Tablespace not found: " + tablespace_name);
                }
                tablespace_id = ts_info.tablespace_id;
            }

            // Get default schema (PUBLIC)
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get default schema");
            }

            // Check CREATE permission on schema
            if (!checkPermission(schema_info.schema_id,
                               core::CatalogManager::PermissionObjectType::SCHEMA,
                               static_cast<uint32_t>(core::CatalogManager::Privilege::CREATE)))
            {
                error("Permission denied: CREATE on schema PUBLIC");
            }

            // Create table in catalog
            core::ID table_id;
            status = db_->catalog_manager()->createTable(schema_info.schema_id, table_name, columns,
                                                         table_id, tablespace_id, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to create table");
            }

            // Create pending FK constraints (ALPHA Phase A - FK Constraints)
            for (const auto& fk : pending_fks)
            {
                // Look up parent table (assume same schema)
                core::CatalogManager::TableInfo parent_table;
                status = db_->catalog_manager()->getTable(schema_info.schema_id, fk.parent_table, parent_table, nullptr);
                if (status != core::Status::OK)
                {
                    error("FK parent table not found: " + fk.parent_table);
                }

                // Convert action strings to FKAction enums
                auto parseAction = [](const std::string& action_str) -> core::CatalogManager::FKAction {
                    if (action_str == "CASCADE") return core::CatalogManager::FKAction::CASCADE;
                    if (action_str == "SET NULL") return core::CatalogManager::FKAction::SET_NULL;
                    if (action_str == "SET DEFAULT") return core::CatalogManager::FKAction::SET_DEFAULT;
                    if (action_str == "RESTRICT") return core::CatalogManager::FKAction::RESTRICT;
                    return core::CatalogManager::FKAction::NO_ACTION; // Default
                };

                core::CatalogManager::FKAction on_delete = parseAction(fk.on_delete_action);
                core::CatalogManager::FKAction on_update = parseAction(fk.on_update_action);

                // Generate FK name: table_fk_col1_col2_..._ref (Phase C: composite FK support)
                std::string fk_name = table_name + "_fk";
                for (const auto& col : fk.child_columns)
                {
                    fk_name += "_" + col;
                }
                fk_name += "_ref";

                // Create FK constraint (Phase C: use child_columns vector)
                core::ID fk_id;
                status = db_->catalog_manager()->createForeignKey(
                    fk_name,
                    table_id,
                    parent_table.table_id,
                    fk.child_columns,
                    fk.parent_columns,
                    on_delete,
                    on_update,
                    core::CatalogManager::FKMatchType::SIMPLE,
                    fk_id,
                    nullptr
                );

                if (status != core::Status::OK)
                {
                    error("Failed to create foreign key constraint: " + fk_name);
                }
            }
        }

        void Executor::executeCreateIndex()
        {
            // Read index name
            std::string index_name = readString();

            // Read table name
            std::string table_name = readString();

            // Read is_unique flag
            bool is_unique = (readByte() != 0);

            // Read column count
            uint32_t column_count = readInt32();

            // Read column names
            std::vector<std::string> column_names;
            for (uint32_t i = 0; i < column_count; i++)
            {
                column_names.push_back(readString());
            }

            // Read tablespace name (Phase 2 Task 2.3)
            std::string tablespace_name = readString();
            uint16_t tablespace_id = 0;

            if (!tablespace_name.empty())
            {
                core::TablespaceInfo ts_info;
                auto ts_status = db_->catalog_manager()->getTablespaceByName(tablespace_name, ts_info, nullptr);
                if (ts_status != core::Status::OK)
                {
                    error("Tablespace not found: " + tablespace_name);
                }
                tablespace_id = ts_info.tablespace_id;
            }

            // LSM Integration Phase 2 Task 2.3: Read index type from bytecode
            uint8_t index_type_byte = readByte();
            core::CatalogManager::IndexType index_type = core::CatalogManager::IndexType::BTREE;  // Default
            if (index_type_byte != 0xFF)
            {
                // Convert byte to enum (0xFF means use default BTREE)
                index_type = static_cast<core::CatalogManager::IndexType>(index_type_byte);
            }

            // Task 17 Phase 6: Read expression/predicate flags
            bool has_expressions = (readByte() != 0);
            bool has_predicate = (readByte() != 0);

            std::vector<uint8_t> expression_data;
            std::vector<std::string> expression_strings;

            if (has_expressions)
            {
                uint32_t expr_data_len = readInt32();
                expression_data.resize(expr_data_len);
                for (uint32_t i = 0; i < expr_data_len; i++)
                {
                    expression_data[i] = readByte();
                }

                uint32_t expr_string_count = readInt32();
                for (uint32_t i = 0; i < expr_string_count; i++)
                {
                    expression_strings.push_back(readString());
                }
            }

            std::vector<uint8_t> predicate_data;
            std::string predicate_string;

            if (has_predicate)
            {
                uint32_t pred_data_len = readInt32();
                predicate_data.resize(pred_data_len);
                for (uint32_t i = 0; i < pred_data_len; i++)
                {
                    predicate_data[i] = readByte();
                }
                predicate_string = readString();
            }

            // Get default schema (PUBLIC)
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get default schema");
            }

            // Get table ID by name
            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Table not found: " + table_name);
            }

            // Create index in catalog
            core::ID index_id;

            if (has_expressions || has_predicate)
            {
                // Use new createIndex() overload with expression/predicate support
                status = db_->catalog_manager()->createIndex(
                    table_info.table_id, index_name, column_names,
                    expression_data, predicate_data,
                    expression_strings, predicate_string,
                    index_id, is_unique, index_type,  // LSM Integration: Use parsed index type
                    tablespace_id, nullptr);
            }
            else
            {
                // Use original createIndex() for simple indexes
                status = db_->catalog_manager()->createIndex(
                    table_info.table_id, index_name, column_names,
                    index_id, is_unique, index_type,  // LSM Integration: Use parsed index type
                    tablespace_id, nullptr);
            }

            if (status != core::Status::OK)
            {
                error("Failed to create index");
            }

            // Task 17 Phase 6: Build index immediately if it has expressions or predicate
            if (has_expressions || has_predicate)
            {
                // Task 17 MGA Phase 1.1: Pass current transaction ID
                uint64_t xid = db_->storage_engine()->getCurrentXid();
                buildExpressionIndex(xid, table_info, index_id);
            }
        }

        // Task 17 MGA Phase 1.1: Added xid parameter for transaction context
        void Executor::buildExpressionIndex(
            uint64_t xid,
            const core::CatalogManager::TableInfo &table_info,
            const core::ID &index_id)
        {
            // 1. Get index info from catalog
            core::CatalogManager::IndexInfo index_info;
            auto status = db_->catalog_manager()->getIndex(index_id, index_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get index info for building");
            }

            // 2. Get table columns
            std::vector<core::CatalogManager::ColumnInfo> columns;
            status = db_->catalog_manager()->getColumns(table_info.table_id, columns, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get table columns");
            }

            // 3. Deserialize expressions and predicate
            parser::StringPool temp_pool;
            auto expressions_unique = std::vector<std::unique_ptr<parser::Expression>>();
            auto predicate_unique = std::unique_ptr<parser::Expression>();

            // Create raw pointer vectors for evaluator (will be cleaned up automatically)
            std::vector<parser::Expression *> expressions;
            parser::Expression *predicate = nullptr;

            if (index_info.is_expression_index)
            {
                expressions_unique = core::ExpressionSerializer::deserializeList(
                    index_info.expression_data.data(),
                    index_info.expression_data.size(),
                    temp_pool);
                for (auto& expr : expressions_unique)
                {
                    expressions.push_back(expr.get());
                }
            }

            if (index_info.is_partial_index)
            {
                predicate_unique = core::ExpressionSerializer::deserialize(
                    index_info.predicate_data.data(),
                    index_info.predicate_data.size(),
                    temp_pool);
                predicate = predicate_unique.get();
            }

            // 4. Create expression evaluator
            // Task 17 MGA Phase 1.4: Pass database and transaction ID for visibility checks
            ExpressionEvaluator evaluator(columns, &temp_pool, db_, xid);

            // 5. Open B-tree for this index
            auto btree = core::BTree::open(db_, index_info.index_id, index_info.root_page, nullptr);
            if (!btree)
            {
                error("Failed to open B-tree for index building");
            }

            // 6. Scan table and build index
            auto scan = db_->storage_engine()->createScan(table_info.table_id, nullptr);
            if (!scan)
            {
                error("Failed to create table scan for index building");
            }

            size_t rows_indexed = 0;
            size_t rows_skipped = 0;

            core::Tuple tuple;
            while (scan->next(&tuple, nullptr) == core::Status::OK)
            {
                // Task 17 MGA Phase 1.2: Check tuple visibility BEFORE indexing
                // Extract xmin/xmax from tuple header
                auto* hdr = reinterpret_cast<const core::TupleHeader*>(tuple.data);

                // Check if tuple is visible to current transaction
                if (!db_->storage_engine()->isVisible(hdr->xmin, hdr->xmax, xid))
                {
                    rows_skipped++;
                    index_stats_.invisible_skipped++;  // Task 17 MGA Phase 2.2
                    continue;  // Skip invisible tuple (uncommitted or deleted)
                }

                // Deserialize row into values
                std::vector<Value> row_values;
                if (!deserializeTuple(tuple.data, tuple.data_size, columns, row_values))
                {
                    rows_skipped++;
                    continue;
                }

                // Check predicate (if partial index)
                if (predicate)
                {
                    try
                    {
                        index_stats_.predicate_evaluations++;  // Task 17 MGA Phase 2.2
                        bool matches = evaluator.evaluatePredicate(predicate, row_values);
                        if (!matches)
                        {
                            rows_skipped++;
                            continue; // Skip row not matching WHERE clause
                        }
                    }
                    catch (const std::exception &e)
                    {
                        // Predicate evaluation error - skip row
                        rows_skipped++;
                        continue;
                    }
                }

                // Compute index key
                std::vector<Value> key_values;
                bool expr_error = false;

                if (index_info.is_expression_index)
                {
                    // Expression index - evaluate expressions
                    for (auto *expr : expressions)
                    {
                        try
                        {
                            index_stats_.expression_evaluations++;  // Task 17 MGA Phase 2.2
                            Value key_val = evaluator.evaluate(expr, row_values);
                            key_values.push_back(key_val);
                        }
                        catch (const std::exception &e)
                        {
                            // Expression evaluation error - skip row
                            expr_error = true;
                            break;
                        }
                    }
                }
                else
                {
                    // Regular column index (shouldn't reach here, but handle it)
                    for (const auto &col_id : index_info.column_ids)
                    {
                        for (size_t i = 0; i < columns.size(); i++)
                        {
                            if (columns[i].column_id == col_id)
                            {
                                key_values.push_back(row_values[i]);
                                break;
                            }
                        }
                    }
                }

                if (expr_error)
                {
                    rows_skipped++;
                    continue;
                }

                // Serialize key for B-tree insertion
                std::vector<uint8_t> key_bytes;
                bool skip_row = false;
                for (const auto &val : key_values)
                {
                    // Simple serialization - extend for all types
                    if (val.isNull())
                    {
                        key_bytes.push_back(0xFF); // NULL marker
                    }
                    else
                    {
                        switch (val.type())
                        {
                        case core::DataType::INT32:
                        case core::DataType::INT64:
                        {
                            int64_t i = val.getInt64();
                            key_bytes.push_back(0x01); // INT marker
                            for (int j = 7; j >= 0; j--)
                            {
                                key_bytes.push_back((i >> (j * 8)) & 0xFF);
                            }
                            break;
                        }
                        case core::DataType::VARCHAR:
                        {
                            std::string s = val.toString();
                            key_bytes.push_back(0x02); // STRING marker
                            uint32_t len = static_cast<uint32_t>(s.length());
                            for (int j = 3; j >= 0; j--)
                            {
                                key_bytes.push_back((len >> (j * 8)) & 0xFF);
                            }
                            key_bytes.insert(key_bytes.end(), s.begin(), s.end());
                            break;
                        }
                        case core::DataType::FLOAT64:
                        {
                            double d = val.toDouble();
                            key_bytes.push_back(0x03); // DOUBLE marker
                            uint64_t bits;
                            std::memcpy(&bits, &d, sizeof(double));
                            for (int j = 7; j >= 0; j--)
                            {
                                key_bytes.push_back((bits >> (j * 8)) & 0xFF);
                            }
                            break;
                        }
                        case core::DataType::BOOLEAN:
                        {
                            key_bytes.push_back(0x04); // BOOLEAN marker
                            key_bytes.push_back(val.getBoolean() ? 1 : 0);
                            break;
                        }
                        default:
                            // Unsupported type - skip
                            skip_row = true;
                            break;
                        }

                        if (skip_row)
                        {
                            break;
                        }
                    }
                }

                if (skip_row)
                {
                    rows_skipped++;
                    continue;
                }

                // Insert into B-tree
                // Task 17 MGA Phase 3.1: Pass xid for btn_xmin tracking
                status = btree->insert(key_bytes, tuple.tid, xid, nullptr);
                if (status != core::Status::OK)
                {
                    // Log error but continue
                    rows_skipped++;
                }
                else
                {
                    rows_indexed++;
                    index_stats_.entries_added++;  // Task 17 MGA Phase 2.2
                }
            }

            // Task 17 MGA Phase 2.1: Enhanced debug logging for index maintenance
            std::string log_msg = "Built ";
            if (index_info.is_expression_index) log_msg += "expression ";
            if (index_info.is_partial_index) log_msg += "partial ";
            log_msg += "index '" + index_info.index_name + "' on table '" +
                       table_info.table_name + "' with " +
                       std::to_string(rows_indexed) + " rows indexed, " +
                       std::to_string(rows_skipped) + " rows skipped (xid=" +
                       std::to_string(xid) + ")";
            DEBUG_LOG_INDEX(log_msg);

            // Task 17 MGA Phase 2.2: Track index maintenance
            index_stats_.indexes_maintained++;

            // No manual cleanup needed - unique_ptr handles it automatically
        }

        // Task 17 Phase 7: Index maintenance helpers
        // Task 17 MGA Phase 1.1: Added xid parameter for transaction context
        void Executor::updateIndexesOnInsert(
            uint64_t xid,
            const core::ID &table_id,
            const core::CatalogManager::TableInfo &table_info,
            const std::vector<core::CatalogManager::ColumnInfo> &all_columns,
            uint32_t page_id,
            uint16_t item_id,
            const std::vector<Value> &row_values)
        {
            // Get all indexes for this table
            std::vector<core::CatalogManager::IndexInfo> indexes;
            auto status = db_->catalog_manager()->listIndexesForTable(table_id, indexes, nullptr);
            if (status != core::Status::OK)
            {
                return; // No indexes or error - continue
            }

            core::TID tid(page_id, item_id);

            for (const auto &index_info : indexes)
            {
                // CRITICAL FIX (Nov 20, 2025): Maintain ALL indexes, not just expression/partial
                // Previous bug: Basic indexes were skipped, causing data integrity violations

                // Deserialize expression/predicate (only if needed)
                parser::StringPool temp_pool;
                auto expressions_unique = std::vector<std::unique_ptr<parser::Expression>>();
                auto predicate_unique = std::unique_ptr<parser::Expression>();

                // Create raw pointer vectors for evaluator (will be cleaned up automatically)
                std::vector<parser::Expression *> expressions;
                parser::Expression *predicate = nullptr;

                if (index_info.is_expression_index)
                {
                    expressions_unique = core::ExpressionSerializer::deserializeList(
                        index_info.expression_data.data(),
                        index_info.expression_data.size(),
                        temp_pool);
                    for (auto& expr : expressions_unique)
                    {
                        expressions.push_back(expr.get());
                    }
                }

                if (index_info.is_partial_index)
                {
                    predicate_unique = core::ExpressionSerializer::deserialize(
                        index_info.predicate_data.data(),
                        index_info.predicate_data.size(),
                        temp_pool);
                    predicate = predicate_unique.get();
                }

                // Create evaluator
                // Task 17 MGA Phase 1.4: Pass database and transaction ID for visibility checks
                ExpressionEvaluator evaluator(all_columns, &temp_pool, db_, xid);

                // Check predicate
                if (predicate)
                {
                    try
                    {
                        index_stats_.predicate_evaluations++;  // Task 17 MGA Phase 2.2
                        bool matches = evaluator.evaluatePredicate(predicate, row_values);
                        if (!matches)
                        {
                            // Row doesn't match filter - skip this index (cleanup automatic)
                            continue;
                        }
                    }
                    catch (...)
                    {
                        // Error evaluating - skip (cleanup automatic)
                        continue;
                    }
                }

                // Compute key
                std::vector<Value> key_values;
                bool skip_index = false;
                if (index_info.is_expression_index)
                {
                    for (auto *expr : expressions)
                    {
                        try
                        {
                            index_stats_.expression_evaluations++;  // Task 17 MGA Phase 2.2
                            key_values.push_back(evaluator.evaluate(expr, row_values));
                        }
                        catch (...)
                        {
                            // Error - skip this index
                            skip_index = true;
                            break;
                        }
                    }
                }
                else
                {
                    // Regular columns
                    for (const auto &col_id : index_info.column_ids)
                    {
                        for (size_t i = 0; i < all_columns.size(); i++)
                        {
                            if (all_columns[i].column_id == col_id)
                            {
                                key_values.push_back(row_values[i]);
                                break;
                            }
                        }
                    }
                }

                if (skip_index)
                {
                    // Skip this index (cleanup automatic)
                    continue;
                }

                // Serialize key
                std::vector<uint8_t> key_bytes;
                serializeIndexKey(key_values, key_bytes);

                // Insert into B-tree
                auto btree = core::BTree::open(db_, index_info.index_id, index_info.root_page, nullptr);
                if (btree)
                {
                    // Task 17 MGA Phase 3.1: Pass xid for btn_xmin tracking
                    btree->insert(key_bytes, tid, xid, nullptr);

                    // Task 17 MGA Phase 2.1: Debug logging for insert
                    DEBUG_LOG_INDEX("Index '" + index_info.index_name + "': added entry for tid=" +
                                   std::to_string(tid.value()) + " (xid=" + std::to_string(xid) + ")");

                    // Task 17 MGA Phase 2.2: Track statistics
                    index_stats_.entries_added++;
                    index_stats_.indexes_maintained++;
                }

                // No manual cleanup needed - unique_ptr handles it automatically
            }
        }

        // Task 17 MGA Phase 1.1: Added xid parameter for transaction context
        void Executor::updateIndexesOnUpdate(
            uint64_t xid,
            const core::ID &table_id,
            const core::CatalogManager::TableInfo &table_info,
            const std::vector<core::CatalogManager::ColumnInfo> &all_columns,
            const std::vector<Value> &old_values,
            const std::vector<Value> &new_values,
            core::TID old_tid,
            core::TID new_tid)
        {
            std::vector<core::CatalogManager::IndexInfo> indexes;
            auto status = db_->catalog_manager()->listIndexesForTable(table_id, indexes, nullptr);
            if (status != core::Status::OK)
            {
                return;
            }

            for (const auto &index_info : indexes)
            {
                // CRITICAL FIX (Nov 20, 2025): Maintain ALL indexes, not just expression/partial
                // Previous bug: Basic indexes were skipped during UPDATE, causing stale entries

                parser::StringPool temp_pool;
                auto expressions_unique = std::vector<std::unique_ptr<parser::Expression>>();
                auto predicate_unique = std::unique_ptr<parser::Expression>();

                // Create raw pointer vectors for evaluator (will be cleaned up automatically)
                std::vector<parser::Expression *> expressions;
                parser::Expression *predicate = nullptr;

                if (index_info.is_expression_index)
                {
                    expressions_unique = core::ExpressionSerializer::deserializeList(
                        index_info.expression_data.data(),
                        index_info.expression_data.size(),
                        temp_pool);
                    for (auto& expr : expressions_unique)
                    {
                        expressions.push_back(expr.get());
                    }
                }

                if (index_info.is_partial_index)
                {
                    predicate_unique = core::ExpressionSerializer::deserialize(
                        index_info.predicate_data.data(),
                        index_info.predicate_data.size(),
                        temp_pool);
                    predicate = predicate_unique.get();
                }

                // Task 17 MGA Phase 1.4: Pass database and transaction ID for visibility checks
                ExpressionEvaluator evaluator(all_columns, &temp_pool, db_, xid);

                // Check predicate for both old and new
                bool in_old = true, in_new = true;

                if (predicate)
                {
                    try
                    {
                        in_old = evaluator.evaluatePredicate(predicate, old_values);
                        in_new = evaluator.evaluatePredicate(predicate, new_values);
                    }
                    catch (...)
                    {
                        // Error - assume not in index
                        in_old = in_new = false;
                    }
                }

                // Compute old and new keys
                std::vector<uint8_t> old_key, new_key;

                if (in_old)
                {
                    std::vector<Value> old_key_vals;
                    if (index_info.is_expression_index)
                    {
                        for (auto *expr : expressions)
                        {
                            try
                            {
                                old_key_vals.push_back(evaluator.evaluate(expr, old_values));
                            }
                            catch (...)
                            {
                                in_old = false;
                                break;
                            }
                        }
                    }
                    else
                    {
                        for (const auto &col_id : index_info.column_ids)
                        {
                            for (size_t i = 0; i < all_columns.size(); i++)
                            {
                                if (all_columns[i].column_id == col_id)
                                {
                                    old_key_vals.push_back(old_values[i]);
                                    break;
                                }
                            }
                        }
                    }

                    if (in_old)
                    {
                        serializeIndexKey(old_key_vals, old_key);
                    }
                }

                if (in_new)
                {
                    std::vector<Value> new_key_vals;
                    if (index_info.is_expression_index)
                    {
                        for (auto *expr : expressions)
                        {
                            try
                            {
                                new_key_vals.push_back(evaluator.evaluate(expr, new_values));
                            }
                            catch (...)
                            {
                                in_new = false;
                                break;
                            }
                        }
                    }
                    else
                    {
                        for (const auto &col_id : index_info.column_ids)
                        {
                            for (size_t i = 0; i < all_columns.size(); i++)
                            {
                                if (all_columns[i].column_id == col_id)
                                {
                                    new_key_vals.push_back(new_values[i]);
                                    break;
                                }
                            }
                        }
                    }

                    if (in_new)
                    {
                        serializeIndexKey(new_key_vals, new_key);
                    }
                }

                // Open B-tree
                auto btree = core::BTree::open(db_, index_info.index_id, index_info.root_page, nullptr);
                if (!btree)
                {
                    // Skip this index (cleanup automatic)
                    continue;
                }

                // Handle four cases (Task 17 MGA Phase 2.1: Added debug logging)
                if (in_old && in_new)
                {
                    // Both in index - delete old, insert new
                    // Task 17 MGA Phase 3.1: Pass xid for transaction tracking
                    btree->remove(old_key, old_tid, xid, nullptr);
                    btree->insert(new_key, new_tid, xid, nullptr);
                    DEBUG_LOG_INDEX("Index '" + index_info.index_name + "': predicate transition UPDATE " +
                                   "(old_tid=" + std::to_string(old_tid.value()) + " → new_tid=" +
                                   std::to_string(new_tid.value()) + ", xid=" + std::to_string(xid) + ")");
                    // Task 17 MGA Phase 2.2: Track statistics
                    index_stats_.entries_updated++;
                    index_stats_.indexes_maintained++;
                }
                else if (in_old && !in_new)
                {
                    // Was in index, now not - delete
                    // Task 17 MGA Phase 3.1: Pass xid for transaction tracking
                    btree->remove(old_key, old_tid, xid, nullptr);
                    DEBUG_LOG_INDEX("Index '" + index_info.index_name + "': predicate transition DELETE " +
                                   "(was in index, now not, tid=" + std::to_string(old_tid.value()) +
                                   ", xid=" + std::to_string(xid) + ")");
                    // Task 17 MGA Phase 2.2: Track statistics
                    index_stats_.entries_removed++;
                    index_stats_.indexes_maintained++;
                }
                else if (!in_old && in_new)
                {
                    // Wasn't in index, now is - insert
                    // Task 17 MGA Phase 3.1: Pass xid for transaction tracking
                    btree->insert(new_key, new_tid, xid, nullptr);
                    DEBUG_LOG_INDEX("Index '" + index_info.index_name + "': predicate transition INSERT " +
                                   "(wasn't in index, now is, tid=" + std::to_string(new_tid.value()) +
                                   ", xid=" + std::to_string(xid) + ")");
                    // Task 17 MGA Phase 2.2: Track statistics
                    index_stats_.entries_added++;
                    index_stats_.indexes_maintained++;
                }
                // else: neither in index - no change

                // No manual cleanup needed - unique_ptr handles it automatically
            }
        }

        // Task 17 MGA Phase 1.1: Added xid parameter for transaction context
        void Executor::updateIndexesOnDelete(
            uint64_t xid,
            const core::ID &table_id,
            const core::CatalogManager::TableInfo &table_info,
            const std::vector<core::CatalogManager::ColumnInfo> &all_columns,
            const std::vector<Value> &row_values,
            core::TID tid)
        {
            std::vector<core::CatalogManager::IndexInfo> indexes;
            auto status = db_->catalog_manager()->listIndexesForTable(table_id, indexes, nullptr);
            if (status != core::Status::OK)
            {
                return;
            }

            for (const auto &index_info : indexes)
            {
                // CRITICAL FIX (Nov 20, 2025): Maintain ALL indexes, not just expression/partial
                // Previous bug: Basic indexes were skipped during DELETE, causing orphaned entries

                parser::StringPool temp_pool;
                auto expressions_unique = std::vector<std::unique_ptr<parser::Expression>>();
                auto predicate_unique = std::unique_ptr<parser::Expression>();

                // Create raw pointer vectors for evaluator (will be cleaned up automatically)
                std::vector<parser::Expression *> expressions;
                parser::Expression *predicate = nullptr;

                if (index_info.is_expression_index)
                {
                    expressions_unique = core::ExpressionSerializer::deserializeList(
                        index_info.expression_data.data(),
                        index_info.expression_data.size(),
                        temp_pool);
                    for (auto& expr : expressions_unique)
                    {
                        expressions.push_back(expr.get());
                    }
                }

                if (index_info.is_partial_index)
                {
                    predicate_unique = core::ExpressionSerializer::deserialize(
                        index_info.predicate_data.data(),
                        index_info.predicate_data.size(),
                        temp_pool);
                    predicate = predicate_unique.get();
                }

                // Task 17 MGA Phase 1.4: Pass database and transaction ID for visibility checks
                ExpressionEvaluator evaluator(all_columns, &temp_pool, db_, xid);

                // Check if row was in index
                bool in_index = true;
                if (predicate)
                {
                    try
                    {
                        in_index = evaluator.evaluatePredicate(predicate, row_values);
                    }
                    catch (...)
                    {
                        in_index = false;
                    }
                }

                if (!in_index)
                {
                    // Not in index - nothing to delete (cleanup automatic)
                    continue;
                }

                // Compute key
                std::vector<Value> key_values;
                bool skip_index = false;
                if (index_info.is_expression_index)
                {
                    for (auto *expr : expressions)
                    {
                        try
                        {
                            key_values.push_back(evaluator.evaluate(expr, row_values));
                        }
                        catch (...)
                        {
                            skip_index = true;
                            break;
                        }
                    }
                }
                else
                {
                    for (const auto &col_id : index_info.column_ids)
                    {
                        for (size_t i = 0; i < all_columns.size(); i++)
                        {
                            if (all_columns[i].column_id == col_id)
                            {
                                key_values.push_back(row_values[i]);
                                break;
                            }
                        }
                    }
                }

                if (skip_index)
                {
                    // Skip this index (cleanup automatic)
                    continue;
                }

                // Serialize and delete
                std::vector<uint8_t> key_bytes;
                serializeIndexKey(key_values, key_bytes);

                // TASK-DML-2 Enhancement: Use storage engine helpers for all index types
                // Get index pointer from catalog (supports all 11 index types)
                core::CatalogManager::IndexType actual_index_type;
                void *index_ptr = db_->catalog_manager()->getIndexPtr(index_info.index_id, &actual_index_type);

                if (index_ptr)
                {
                    // Use removeFromIndex helper (handles B-Tree, LSM, Hash, HNSW, etc.)
                    // Note: Cast IndexType enum to uint8_t to avoid circular include dependency
                    core::Status remove_status = db_->storage_engine()->removeFromIndexHelper(
                        static_cast<uint8_t>(actual_index_type), index_ptr, key_bytes, tid, xid, nullptr);

                    if (remove_status == core::Status::OK)
                    {
                        // Task 17 MGA Phase 2.1: Debug logging for delete
                        DEBUG_LOG_INDEX("Index '" + index_info.index_name + "': removed entry for tid=" +
                                       std::to_string(tid.value()) + " (xid=" + std::to_string(xid) + ")");

                        // Task 17 MGA Phase 2.2: Track statistics
                        index_stats_.entries_removed++;
                        index_stats_.indexes_maintained++;
                    }
                }
                else
                {
                    // Fallback to direct B-Tree access for compatibility
                    auto btree = core::BTree::open(db_, index_info.index_id, index_info.root_page, nullptr);
                    if (btree)
                    {
                        // Task 17 MGA Phase 3.1: Pass xid for transaction tracking
                        btree->remove(key_bytes, tid, xid, nullptr);

                        // Task 17 MGA Phase 2.1: Debug logging for delete
                        DEBUG_LOG_INDEX("Index '" + index_info.index_name + "': removed entry for tid=" +
                                       std::to_string(tid.value()) + " (xid=" + std::to_string(xid) + ")");

                        // Task 17 MGA Phase 2.2: Track statistics
                        index_stats_.entries_removed++;
                        index_stats_.indexes_maintained++;
                    }
                }

                // No manual cleanup needed - unique_ptr handles it automatically
            }
        }

        void Executor::serializeIndexKey(const std::vector<Value> &key_values,
                                          std::vector<uint8_t> &key_bytes_out)
        {
            key_bytes_out.clear();

            for (const auto &val : key_values)
            {
                if (val.isNull())
                {
                    key_bytes_out.push_back(0xFF);
                }
                else
                {
                    switch (val.type())
                    {
                    case core::DataType::INT32:
                    case core::DataType::INT64:
                    {
                        int64_t i = val.getInt64();
                        key_bytes_out.push_back(0x01);
                        for (int j = 7; j >= 0; j--)
                        {
                            key_bytes_out.push_back((i >> (j * 8)) & 0xFF);
                        }
                        break;
                    }
                    case core::DataType::VARCHAR:
                    {
                        std::string s = val.toString();
                        key_bytes_out.push_back(0x02);
                        uint32_t len = s.length();
                        for (int j = 3; j >= 0; j--)
                        {
                            key_bytes_out.push_back((len >> (j * 8)) & 0xFF);
                        }
                        key_bytes_out.insert(key_bytes_out.end(), s.begin(), s.end());
                        break;
                    }
                    case core::DataType::FLOAT64:
                    {
                        double d = val.toDouble();
                        key_bytes_out.push_back(0x03);
                        uint64_t bits;
                        std::memcpy(&bits, &d, sizeof(double));
                        for (int j = 7; j >= 0; j--)
                        {
                            key_bytes_out.push_back((bits >> (j * 8)) & 0xFF);
                        }
                        break;
                    }
                    case core::DataType::BOOLEAN:
                    {
                        key_bytes_out.push_back(0x04);
                        key_bytes_out.push_back(val.getBoolean() ? 1 : 0);
                        break;
                    }
                    default:
                        // Unsupported - use NULL
                        key_bytes_out.push_back(0xFF);
                    }
                }
            }
        }

        void Executor::executeCreateTablespace()
        {
            // Read tablespace name
            std::string tablespace_name = readString();

            // Read location path
            std::string location = readString();

            // Read autoextend_enabled (1 byte)
            bool autoextend_enabled = (readByte() != 0);

            // Read autoextend_size_mb (uint32)
            uint32_t autoextend_size_mb = readInt32();

            // Read max_size_mb (uint32, 0 = UNLIMITED)
            uint32_t max_size_mb = readInt32();

            // Read prealloc_pages (uint32)
            uint32_t prealloc_pages = readInt32();

            // Create tablespace via CatalogManager
            core::ErrorContext err_ctx;
            uint16_t tablespace_id;
            core::Status status = db_->catalog_manager()->createTablespace(
                tablespace_name, location, autoextend_enabled, autoextend_size_mb, max_size_mb,
                prealloc_pages, tablespace_id, &err_ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to create tablespace '" + tablespace_name + "'";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
            }
        }

        void Executor::executeAlterTablespace()
        {
            // Read tablespace name
            std::string tablespace_name = readString();

            // Read number of alterations
            uint32_t alteration_count = readInt32();

            // Process each alteration
            for (uint32_t i = 0; i < alteration_count; i++)
            {
                // Read alteration type
                uint8_t alt_type = readByte();

                core::ErrorContext err_ctx;
                core::Status status = core::Status::OK;

                switch (alt_type)
                {
                    case 0: // SET_AUTOEXTEND
                    {
                        bool autoextend_enabled = (readByte() != 0);
                        // Get current tablespace info to preserve other parameters
                        core::TablespaceInfo info;
                        status = db_->catalog_manager()->getTablespaceByName(tablespace_name, info,
                                                                              &err_ctx);
                        if (status == core::Status::OK)
                        {
                            status = db_->catalog_manager()->updateTablespace(
                                tablespace_name, autoextend_enabled, info.autoextend_size_mb,
                                info.max_size_mb, &err_ctx);
                        }
                        break;
                    }

                    case 1: // SET_AUTOEXTEND_SIZE
                    {
                        uint32_t autoextend_size_mb = readInt32();
                        // Get current tablespace info to preserve other parameters
                        core::TablespaceInfo info;
                        status = db_->catalog_manager()->getTablespaceByName(tablespace_name, info,
                                                                              &err_ctx);
                        if (status == core::Status::OK)
                        {
                            status = db_->catalog_manager()->updateTablespace(
                                tablespace_name, info.autoextend_enabled, autoextend_size_mb,
                                info.max_size_mb, &err_ctx);
                        }
                        break;
                    }

                    case 2: // SET_MAXSIZE
                    {
                        uint32_t max_size_mb = readInt32();
                        // Get current tablespace info to preserve other parameters
                        core::TablespaceInfo info;
                        status = db_->catalog_manager()->getTablespaceByName(tablespace_name, info,
                                                                              &err_ctx);
                        if (status == core::Status::OK)
                        {
                            status = db_->catalog_manager()->updateTablespace(
                                tablespace_name, info.autoextend_enabled,
                                info.autoextend_size_mb, max_size_mb, &err_ctx);
                        }
                        break;
                    }

                    case 3: // RENAME_TO
                    {
                        std::string new_name = readString();
                        status = db_->catalog_manager()->renameTablespace(tablespace_name, new_name,
                                                                           &err_ctx);
                        // If rename succeeded, update tablespace_name for subsequent alterations
                        if (status == core::Status::OK)
                        {
                            tablespace_name = new_name;
                        }
                        break;
                    }

                    default:
                        error("Unknown alteration type: " + std::to_string(alt_type));
                        return;
                }

                if (status != core::Status::OK)
                {
                    std::string err_msg = "Failed to alter tablespace '" + tablespace_name + "'";
                    if (!err_ctx.message.empty())
                    {
                        err_msg += ": " + err_ctx.message;
                    }
                    error(err_msg);
                    return;
                }
            }
        }

        void Executor::executeDropTable()
        {
            // DROP TABLE [IF EXISTS] name [CASCADE | RESTRICT]

            // Read table name (string)
            std::string table_name = readString();

            // Read flags byte
            uint8_t flags = bytecode_[pc_++];
            bool if_exists = (flags & 0x01) != 0;
            bool cascade = (flags & 0x02) != 0;

            // Get current schema (default to 'PUBLIC')
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != Status::OK)
            {
                throw std::runtime_error("Failed to get schema PUBLIC");
            }

            // Check if table exists
            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name.c_str(), table_info, nullptr);
            if (status != Status::OK)
            {
                if (if_exists)
                {
                    // IF EXISTS specified, silently succeed
                    return;
                }
                else
                {
                    throw std::runtime_error("Table does not exist: " + table_name);
                }
            }

            // Check DROP permission on table (table owner or superuser)
            // SECURITY ENHANCEMENT (MEDIUM-1): Use VERIFIED mode for DROP TABLE (irreversible operation)
            if (!checkPermission(table_info.table_id,
                               core::CatalogManager::PermissionObjectType::TABLE,
                               static_cast<uint32_t>(core::CatalogManager::Privilege::DELETE),
                               core::PermissionCheckMode::VERIFIED))
            {
                throw std::runtime_error("Permission denied: DROP TABLE " + table_name);
            }

            // Drop the table using catalog manager
            ErrorContext ctx;
            status = db_->catalog_manager()->dropTable(table_info.table_id, cascade, &ctx);
            if (status != Status::OK)
            {
                throw std::runtime_error("Failed to drop table: " + ctx.message);
            }
        }

        void Executor::executeDropIndex()
        {
            // DROP INDEX [IF EXISTS] name

            // Read index name (string)
            std::string index_name = readString();

            // Read IF EXISTS flag
            uint8_t if_exists = bytecode_[pc_++];

            // Note: CatalogManager requires table_id + index_name or index_id
            // For DROP INDEX by name only, we need to search through all tables
            // This is a known limitation - for now we search all schemas

            // Try to find the index by searching all schemas and tables
            ErrorContext ctx;
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != Status::OK)
            {
                if (if_exists)
                {
                    // IF EXISTS specified and schema doesn't exist
                    return;
                }
                throw std::runtime_error("Failed to get schema PUBLIC");
            }

            // Get all tables in schema
            std::vector<core::CatalogManager::TableInfo> tables;
            status = db_->catalog_manager()->listTables(schema_info.schema_id, tables, nullptr);
            if (status != Status::OK)
            {
                throw std::runtime_error("Failed to list tables");
            }

            // Search all tables for the index
            bool found = false;
            core::ID found_index_id;
            for (const auto &table : tables)
            {
                // Try to get the index from this table
                core::CatalogManager::IndexInfo index_info;
                status = db_->catalog_manager()->getIndex(table.table_id, index_name, index_info, nullptr);
                if (status == Status::OK)
                {
                    // Found it!
                    found = true;
                    found_index_id = index_info.index_id;
                    break;
                }
            }

            if (!found)
            {
                if (if_exists)
                {
                    // IF EXISTS specified, silently succeed
                    return;
                }
                throw std::runtime_error("Index does not exist: " + index_name);
            }

            // Drop the index using catalog manager
            status = db_->catalog_manager()->dropIndex(found_index_id, &ctx);
            if (status != Status::OK)
            {
                throw std::runtime_error("Failed to drop index: " + ctx.message);
            }
        }

        void Executor::executeAlterTable()
        {
            // ALTER TABLE implementation (ALPHA Phase 1 - DDL Modifications)
            // Supports: ADD COLUMN, DROP COLUMN, RENAME COLUMN, ALTER COLUMN TYPE

            // Read table name (string)
            std::string table_name = readString();

            // Read action type
            uint8_t action = bytecode_[pc_++];

            // Get table from catalog
            core::ErrorContext ctx;
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != Status::OK)
            {
                throw std::runtime_error("Failed to get schema PUBLIC");
            }

            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name,
                                                       table_info, &ctx);
            if (status != Status::OK)
            {
                throw std::runtime_error("Table not found: " + table_name);
            }

            // Check ALTER permission on table (requires table owner or superuser)
            // For DDL operations, we typically require ownership rather than just UPDATE
            if (!checkPermission(table_info.table_id,
                               core::CatalogManager::PermissionObjectType::TABLE,
                               static_cast<uint32_t>(core::CatalogManager::Privilege::UPDATE)))
            {
                throw std::runtime_error("Permission denied: ALTER TABLE " + table_name);
            }

            // Dispatch based on action
            switch (action)
            {
                case 0: // ADD_COLUMN
                {
                    std::string col_name = readString();
                    uint16_t data_type = readInt16();
                    uint32_t precision = readInt32();
                    uint32_t scale = readInt32();
                    bool nullable = readByte() != 0;

                    core::CatalogManager::ColumnInfo col_info;
                    col_info.column_name = col_name;
                    col_info.data_type = data_type;
                    col_info.type_precision = precision;
                    col_info.type_scale = scale;
                    col_info.nullable = nullable;

                    status = db_->catalog_manager()->addColumn(table_info.table_id, col_info, &ctx);
                    if (status != Status::OK)
                    {
                        throw std::runtime_error("Failed to add column: " + ctx.message);
                    }
                    break;
                }

                case 1: // DROP_COLUMN
                {
                    std::string col_name = readString();
                    bool if_exists = readByte() != 0;
                    bool cascade = readByte() != 0;

                    status = db_->catalog_manager()->dropColumn(table_info.table_id, col_name,
                                                                 if_exists, cascade, &ctx);

                    if (status != Status::OK && !(status == Status::NOT_FOUND && if_exists))
                    {
                        throw std::runtime_error("Failed to drop column: " + ctx.message);
                    }
                    break;
                }

                case 5: // RENAME_COLUMN
                {
                    std::string old_name = readString();
                    std::string new_name = readString();

                    status = db_->catalog_manager()->renameColumn(table_info.table_id, old_name,
                                                                   new_name, &ctx);

                    if (status != Status::OK)
                    {
                        throw std::runtime_error("Failed to rename column: " + ctx.message);
                    }
                    break;
                }

                case 2: // ALTER_COLUMN_TYPE
                {
                    std::string col_name = readString();
                    uint16_t new_type = readInt16();
                    uint32_t new_precision = readInt32();
                    uint32_t new_scale = readInt32();

                    status = db_->catalog_manager()->alterColumnType(
                        table_info.table_id, col_name, static_cast<core::DataType>(new_type),
                        new_precision, new_scale, &ctx);

                    if (status != Status::OK)
                    {
                        throw std::runtime_error("Failed to alter column type: " + ctx.message);
                    }
                    break;
                }

                default:
                    throw std::runtime_error(
                        "ALTER TABLE action not implemented: " + std::to_string(action));
            }
        }

        void Executor::executeTruncateTable()
        {
            // TRUNCATE TABLE implementation (ALPHA Phase 1 - DDL Modifications)
            // Supports ASYNC (default) and SYNC modes

            // Read table name from bytecode
            std::string table_name = readString();

            // Read mode (0=ASYNC, 1=SYNC)
            uint8_t mode_byte = bytecode_[pc_++];
            bool is_sync = (mode_byte == 1);

            // Get default schema (PUBLIC)
            core::CatalogManager::SchemaInfo schema_info;
            ErrorContext ctx;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, &ctx);
            if (status != Status::OK)
            {
                throw std::runtime_error("Schema not found: PUBLIC");
            }

            // Get table info
            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info, &ctx);
            if (status != Status::OK)
            {
                throw std::runtime_error("Table not found: " + table_name);
            }

            // Get current transaction ID (for now, use 1 as default)
            uint64_t xid = 1;

            if (is_sync)
            {
                // Synchronous mode - blocks until complete
                status = db_->catalog_manager()->truncateTableSync(table_info.table_id, table_name, xid, &ctx);
                if (status != Status::OK)
                {
                    throw std::runtime_error("TRUNCATE TABLE SYNC failed");
                }
                std::cout << "TRUNCATE TABLE completed" << std::endl;
            }
            else
            {
                // Asynchronous mode - returns job ID
                uint64_t job_id = db_->catalog_manager()->truncateTableAsync(table_info.table_id, table_name, xid, &ctx);
                std::cout << "TRUNCATE TABLE job started (ID: " << job_id << ")" << std::endl;
            }
        }

        // ========================================================================
        // Sequence Operations (ALPHA Phase 1 - Sequences)
        // ========================================================================

        void Executor::executeCreateSequence()
        {
            // Read sequence name
            std::string sequence_name = readString();

            // Read optional parameters (flags byte indicates which are present)
            uint8_t flags = readByte();
            bool has_increment = (flags & 0x01) != 0;
            bool has_minvalue = (flags & 0x02) != 0;
            bool has_maxvalue = (flags & 0x04) != 0;
            bool has_start = (flags & 0x08) != 0;
            bool has_cache = (flags & 0x10) != 0;
            bool has_cycle = (flags & 0x20) != 0;

            // Apply defaults
            int64_t increment_by = 1;
            int64_t min_value = 1;
            int64_t max_value = INT64_MAX;
            int64_t start_value = min_value;
            int64_t cache_size = 1;
            bool cycle = false;

            // Read provided values
            if (has_increment) increment_by = readInt64();
            if (has_minvalue) min_value = readInt64();
            if (has_maxvalue) max_value = readInt64();
            if (has_start) start_value = readInt64();
            else start_value = min_value;  // Default start is min_value
            if (has_cache) cache_size = readInt64();
            if (has_cycle) cycle = (readByte() != 0);

            // Get default schema (PUBLIC)
            core::CatalogManager::SchemaInfo schema_info;
            ErrorContext ctx;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, &ctx);
            if (status != Status::OK)
            {
                throw std::runtime_error("Schema not found: PUBLIC");
            }

            // Create sequence
            status = db_->catalog_manager()->createSequence(
                schema_info.schema_id, sequence_name,
                increment_by, min_value, max_value, start_value, cache_size, cycle, &ctx);

            if (status != Status::OK)
            {
                std::string err_msg = "Failed to create sequence '" + sequence_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }
        }

        void Executor::executeAlterSequence()
        {
            // Read sequence name
            std::string sequence_name = readString();

            // Read optional parameters (flags byte indicates which are present)
            uint8_t flags = readByte();
            bool has_increment = (flags & 0x01) != 0;
            bool has_minvalue = (flags & 0x02) != 0;
            bool has_maxvalue = (flags & 0x04) != 0;
            bool has_restart = (flags & 0x08) != 0;
            bool has_cache = (flags & 0x10) != 0;
            bool has_cycle = (flags & 0x20) != 0;

            std::optional<int64_t> increment_by;
            std::optional<int64_t> min_value;
            std::optional<int64_t> max_value;
            std::optional<int64_t> restart;
            std::optional<int64_t> cache_size;
            std::optional<bool> cycle;

            // Read provided values
            if (has_increment) increment_by = readInt64();
            if (has_minvalue) min_value = readInt64();
            if (has_maxvalue) max_value = readInt64();
            if (has_restart) restart = readInt64();
            if (has_cache) cache_size = readInt64();
            if (has_cycle) cycle = (readByte() != 0);

            // Look up sequence ID by name
            core::ID sequence_id;
            core::ErrorContext ctx;
            auto status = db_->catalog_manager()->getSequenceIdByName(sequence_name, sequence_id, &ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Sequence not found: '" + sequence_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }

            // Call alterSequence on the catalog manager
            status = db_->catalog_manager()->alterSequence(sequence_id, increment_by, min_value,
                                                           max_value, restart, cache_size, cycle, &ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to alter sequence '" + sequence_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }
        }

        void Executor::executeDropSequence()
        {
            // Read sequence name
            std::string sequence_name = readString();

            // Read flags
            uint8_t flags = readByte();
            bool cascade = (flags & 0x01) != 0;

            // Look up sequence ID by name
            core::ID sequence_id;
            core::ErrorContext ctx;
            auto status = db_->catalog_manager()->getSequenceIdByName(sequence_name, sequence_id, &ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Sequence not found: '" + sequence_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }

            // Call dropSequence on the catalog manager
            status = db_->catalog_manager()->dropSequence(sequence_id, cascade, &ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to drop sequence '" + sequence_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }
        }

        void Executor::executeCreateView()
        {
            // Read view name
            std::string view_name = readString();

            // Read flags
            uint8_t flags = readByte();
            bool or_replace = (flags & 0x01) != 0;
            bool check_option = (flags & 0x02) != 0;
            bool has_column_names = (flags & 0x04) != 0;
            bool materialized = (flags & 0x08) != 0;  // ALPHA Phase 1 - Materialized Views

            // Read column names if present
            std::vector<std::string> column_names;
            if (has_column_names)
            {
                uint8_t count = readByte();
                for (uint8_t i = 0; i < count; i++)
                {
                    column_names.push_back(readString());
                }
            }

            // Read query definition
            std::string definition = readString();

            // Get default schema (PUBLIC)
            core::ErrorContext ctx;
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, &ctx);
            if (status != core::Status::OK)
            {
                error("Schema not found: PUBLIC");
            }

            // Create view
            status = db_->catalog_manager()->createView(
                schema_info.schema_id, view_name, definition,
                or_replace, check_option, materialized, column_names, &ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to create view '" + view_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }

            std::cout << "CREATE " << (materialized ? "MATERIALIZED " : "") << "VIEW" << std::endl;
        }

        void Executor::executeDropView()
        {
            // Read view name
            std::string view_name = readString();

            // Read flags
            uint8_t flags = readByte();
            bool if_exists = (flags & 0x01) != 0;
            bool cascade = (flags & 0x02) != 0;

            // Look up view ID
            core::ID view_id;
            core::ErrorContext ctx;
            auto status = db_->catalog_manager()->getViewIdByName(view_name, view_id, &ctx);

            if (status == core::Status::NOT_FOUND)
            {
                if (if_exists)
                {
                    std::cout << "NOTICE: view \"" << view_name << "\" does not exist, skipping" << std::endl;
                    return;
                }
                error("View not found: " + view_name);
            }

            // Drop view
            status = db_->catalog_manager()->dropView(view_id, cascade, &ctx);
            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to drop view '" + view_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }

            std::cout << "DROP VIEW" << std::endl;
        }

        void Executor::executeRefreshMaterializedView()
        {
            // ALPHA Phase 1 - Materialized Views: REFRESH MATERIALIZED VIEW

            // SECURITY NOTE (MEDIUM-2): RLS enforcement for materialized views
            // When refreshing a materialized view that queries RLS-protected tables:
            // 1. RLS policies MUST be enforced during view query execution
            // 2. Only users with BYPASSRLS privilege can refresh views over RLS tables
            // 3. Materialized data should respect the refreshing user's permissions
            // Current implementation delegates to catalog_manager->refreshMaterializedView()
            // which should enforce RLS through the query planner. Verify this is working correctly.

            // Read view name
            std::string view_name = readString();

            // Read flags
            uint8_t flags = readByte();
            bool concurrently = (flags & 0x01) != 0;

            // Look up view ID and verify it's materialized
            core::ID view_id;
            core::ErrorContext ctx;
            auto status = db_->catalog_manager()->getViewIdByName(view_name, view_id, &ctx);

            if (status == core::Status::NOT_FOUND)
            {
                error("Materialized view not found: " + view_name);
            }

            // Refresh the materialized view
            status = db_->catalog_manager()->refreshMaterializedView(view_id, concurrently, &ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to refresh materialized view '" + view_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }

            std::cout << "REFRESH " << (concurrently ? "CONCURRENTLY " : "") << "MATERIALIZED VIEW" << std::endl;
        }

        int64_t Executor::executeSequenceNextVal()
        {
            // Read sequence name
            std::string sequence_name = readString();

            // Look up sequence ID by name
            core::ID sequence_id;
            core::ErrorContext ctx;
            auto status = db_->catalog_manager()->getSequenceIdByName(sequence_name, sequence_id, &ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Sequence not found: '" + sequence_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }

            // Call sequenceNextVal to get the next value
            int64_t value = 0;
            status = db_->catalog_manager()->sequenceNextVal(sequence_id, value, &ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to get next value for sequence '" + sequence_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }

            // Store the value in session state for CURRVAL
            session_sequence_currval_[sequence_id] = value;

            // Push INT64 value onto stack
            push(core::TypedValue::makeInt64(value));

            return value;
        }

        int64_t Executor::executeSequenceCurrVal()
        {
            // Read sequence name
            std::string sequence_name = readString();

            // Look up sequence ID by name
            core::ID sequence_id;
            core::ErrorContext ctx;
            auto status = db_->catalog_manager()->getSequenceIdByName(sequence_name, sequence_id, &ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Sequence not found: '" + sequence_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }

            // Check if this sequence has been called in this session
            auto it = session_sequence_currval_.find(sequence_id);
            if (it == session_sequence_currval_.end())
            {
                error("CURRVAL of sequence '" + sequence_name + "' is not yet defined in this session");
            }

            int64_t value = it->second;

            // Push INT64 value onto stack
            push(core::TypedValue::makeInt64(value));

            return value;
        }

        int64_t Executor::executeSequenceSetVal()
        {
            // Read sequence name
            std::string sequence_name = readString();

            // Read value
            int64_t value = readInt64();

            // Read is_called flag
            bool is_called = (readByte() != 0);

            // Look up sequence ID by name
            core::ID sequence_id;
            core::ErrorContext ctx;
            auto status = db_->catalog_manager()->getSequenceIdByName(sequence_name, sequence_id, &ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Sequence not found: '" + sequence_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }

            // Call sequenceSetVal to set the value
            status = db_->catalog_manager()->sequenceSetVal(sequence_id, value, is_called, &ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to set value for sequence '" + sequence_name + "'";
                if (!ctx.message.empty())
                {
                    err_msg += ": " + ctx.message;
                }
                error(err_msg);
            }

            // Store the value in session state for CURRVAL
            session_sequence_currval_[sequence_id] = value;

            // Push INT64 value onto stack (SETVAL returns the value set)
            push(core::TypedValue::makeInt64(value));

            return value;
        }

        void Executor::executeDropTablespace()
        {
            // Read tablespace name
            std::string tablespace_name = readString();

            // Read force flag (1 byte)
            bool force = (readByte() != 0);

            // Drop tablespace via CatalogManager
            core::ErrorContext err_ctx;
            core::Status status =
                db_->catalog_manager()->dropTablespace(tablespace_name, force, &err_ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to drop tablespace '" + tablespace_name + "'";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
            }
        }

        void Executor::executeAttachTablespace()
        {
            // Phase 6 Task 6.1: Execute ATTACH TABLESPACE

            // Read file path
            std::string file_path = readString();

            // Read optional tablespace name
            std::string tablespace_name = readString();

            // Attach tablespace via CatalogManager
            core::ErrorContext err_ctx;
            uint16_t tablespace_id_out;
            core::Status status =
                db_->catalog_manager()->attachTablespace(file_path, tablespace_name, tablespace_id_out, &err_ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to attach tablespace '" + file_path + "'";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
            }
        }

        void Executor::executeDetachTablespace()
        {
            // Phase 6 Task 6.2: Execute DETACH TABLESPACE

            // Read tablespace name
            std::string tablespace_name = readString();

            // Read force flag (1 byte)
            bool force = (readByte() != 0);

            // Detach tablespace via CatalogManager
            core::ErrorContext err_ctx;
            core::Status status =
                db_->catalog_manager()->detachTablespace(tablespace_name, force, &err_ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to detach tablespace '" + tablespace_name + "'";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
            }
        }

        void Executor::executeAlterTableSetTablespace()
        {
            // Phase 4 Task 4.1.6: Execute ALTER TABLE ... SET TABLESPACE

            // Read table name
            std::string table_name = readString();

            // Read tablespace name
            std::string tablespace_name = readString();

            // Read online flag (1 byte)
            bool online = (readByte() != 0);

            // Get default schema (PUBLIC)
            core::CatalogManager::SchemaInfo schema_info;
            core::ErrorContext err_ctx;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, &err_ctx);
            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to get default schema";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
                return;
            }

            // Resolve table name to table ID
            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info,
                                                       &err_ctx);
            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to find table '" + table_name + "'";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
                return;
            }

            // Resolve tablespace name to tablespace ID
            core::TablespaceInfo ts_info;
            status = db_->catalog_manager()->getTablespaceByName(tablespace_name, ts_info, &err_ctx);
            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to find tablespace '" + tablespace_name + "'";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
                return;
            }

            // Call CatalogManager::moveTableToTablespace()
            // Phase 4 Task 4.1.3: Pass nullptr for progress_callback (no progress tracking in executor yet)
            status = db_->catalog_manager()->moveTableToTablespace(table_info.table_id,
                                                                    ts_info.tablespace_id, online,
                                                                    nullptr, // progress_callback
                                                                    &err_ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to move table '" + table_name + "' to tablespace '" +
                                      tablespace_name + "'";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
                return;
            }

            // Success - no result set to return for DDL
        }

        void Executor::executeInsert()
        {
            // Read TABLE_REF opcode
            if (readByte() != static_cast<uint8_t>(Opcode::TABLE_REF))
            {
                error("Expected TABLE_REF in INSERT");
            }

            std::string table_name = readString();

            // Get default schema (PUBLIC)
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get default schema");
            }

            // Get table from catalog
            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info,
                                                      nullptr);
            if (status != core::Status::OK)
            {
                error("Table not found: " + table_name);
            }
            core::ID table_id = table_info.table_id;

            // Check INSERT permission on table
            bool has_table_insert = checkPermission(table_info.table_id,
                               core::CatalogManager::PermissionObjectType::TABLE,
                               static_cast<uint32_t>(core::CatalogManager::Privilege::INSERT));

            // Security Phase 3.3.5: Get accessible columns for INSERT if no table-level permission
            std::vector<std::string> accessible_insert_columns;
            if (!has_table_insert)
            {
                // Check column-level INSERT permissions
                core::ErrorContext err_ctx;
                const auto& user_id = getCurrentUserID();
                status = db_->catalog_manager()->getAccessibleColumns(
                    user_id, table_info.table_id,
                    core::CatalogManager::Privilege::INSERT,
                    accessible_insert_columns, &err_ctx);

                if (status != core::Status::OK || accessible_insert_columns.empty())
                {
                    // No table-level and no column-level INSERT permissions
                    error("Permission denied: INSERT on table " + table_name);
                }
            }
            // If has_table_insert is true, accessible_insert_columns remains empty = all columns insertable

            // Read column list
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for columns");
            }

            uint32_t col_count = readInt32();
            std::vector<std::string> col_names;

            for (uint32_t i = 0; i < col_count; i++)
            {
                if (readByte() != static_cast<uint8_t>(Opcode::COLUMN_REF))
                {
                    error("Expected COLUMN_REF in column list");
                }
                std::string col_name = readString();

                // Security Phase 3.3.5: Check INSERT permission on this column
                if (!accessible_insert_columns.empty() &&
                    std::find(accessible_insert_columns.begin(), accessible_insert_columns.end(), col_name)
                        == accessible_insert_columns.end())
                {
                    error("Permission denied: INSERT on column " + col_name + " of table " + table_name);
                }

                col_names.push_back(col_name);
            }

            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST after column list");
            }

            // Read value list
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for values");
            }

            uint32_t value_count = readInt32();
            if (value_count != col_count)
            {
                error("Column count doesn't match value count");
            }

            // Evaluate each expression and push to stack
            for (uint32_t i = 0; i < value_count; i++)
            {
                evaluateExpression();
            }

            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST after value list");
            }

            // Pop values from stack in reverse order
            std::vector<Value> values;
            for (uint32_t i = 0; i < value_count; i++)
            {
                values.push_back(pop());
            }
            std::reverse(values.begin(), values.end());

            // Get column information to validate and serialize properly
            std::vector<core::CatalogManager::ColumnInfo> all_columns;
            auto status2 = db_->catalog_manager()->getColumns(table_id, all_columns, nullptr);
            if (status2 != core::Status::OK)
            {
                error("Failed to get table columns");
            }

            // Validate that columns exist and build column index map
            std::vector<size_t> col_indices;
            for (const auto &col_name_str : col_names)
            {
                auto it = std::find_if(all_columns.begin(), all_columns.end(),
                                       [&col_name_str](const auto &c)
                                       { return c.column_name == col_name_str; });

                if (it == all_columns.end())
                {
                    error("Column not found: " + col_name_str);
                }

                col_indices.push_back(std::distance(all_columns.begin(), it));
            }

            // Build tuple in binary format
            // Format: TupleHeader + null bitmap (if needed) + column data
            // HeapPage will overwrite some TupleHeader fields (xmin, xmax, ctid, etc.)
            std::vector<uint8_t> tuple_data;

            // Reserve space for TupleHeader (HeapPage expects it)
            size_t header_offset = tuple_data.size();
            tuple_data.resize(tuple_data.size() + sizeof(core::TupleHeader));

            // Determine if we need a null bitmap
            bool has_nulls = false;
            for (const auto &val : values)
            {
                if (val.isNull())
                {
                    has_nulls = true;
                    break;
                }
            }

            // Add null bitmap if needed (one bit per column)
            size_t null_bitmap_offset = 0;
            if (has_nulls)
            {
                null_bitmap_offset = tuple_data.size();
                size_t bitmap_bytes = (all_columns.size() + 7) / 8;
                tuple_data.resize(tuple_data.size() + bitmap_bytes);
                // Initialize bitmap to zero
                std::fill(tuple_data.begin() + null_bitmap_offset,
                          tuple_data.begin() + null_bitmap_offset + bitmap_bytes, 0);
            }

            // Serialize each column value
            for (size_t i = 0; i < values.size(); i++)
            {
                const auto &value = values[i];
                size_t col_idx = col_indices[i];
                const auto &col_info = all_columns[col_idx];

                if (value.isNull())
                {
                    // Set null bit in bitmap
                    size_t bit_offset = col_idx;
                    size_t byte_offset = null_bitmap_offset + (bit_offset / 8);
                    size_t bit_pos = bit_offset % 8;
                    tuple_data[byte_offset] |= (1 << bit_pos);
                    // Don't write any data for null values
                    continue;
                }

                // Serialize value based on column type
                core::DataType col_type = static_cast<core::DataType>(col_info.data_type);

                switch (col_type)
                {
                    case core::DataType::INT32:
                    {
                        int32_t val = static_cast<int32_t>(value.toInt64());
                        size_t offset = tuple_data.size();
                        tuple_data.resize(offset + sizeof(int32_t));
                        std::memcpy(&tuple_data[offset], &val, sizeof(int32_t));
                        break;
                    }
                    case core::DataType::INT64:
                    {
                        int64_t val = value.toInt64();
                        size_t offset = tuple_data.size();
                        tuple_data.resize(offset + sizeof(int64_t));
                        std::memcpy(&tuple_data[offset], &val, sizeof(int64_t));
                        break;
                    }
                    case core::DataType::FLOAT64:
                    {
                        double val = value.toDouble();
                        size_t offset = tuple_data.size();
                        tuple_data.resize(offset + sizeof(double));
                        std::memcpy(&tuple_data[offset], &val, sizeof(double));
                        break;
                    }
                    case core::DataType::VARCHAR:
                    {
                        std::string str = value.toString();
                        // Write length prefix (4 bytes) then data
                        uint32_t len = static_cast<uint32_t>(str.size());
                        size_t offset = tuple_data.size();
                        tuple_data.resize(offset + sizeof(uint32_t) + len);
                        std::memcpy(&tuple_data[offset], &len, sizeof(uint32_t));
                        std::memcpy(&tuple_data[offset + sizeof(uint32_t)], str.data(), len);
                        break;
                    }
                    default:
                        error("Unsupported column type for serialization");
                }
            }

            // Initialize TupleHeader (HeapPage will overwrite xmin, xmax, ctid later)
            auto *header = reinterpret_cast<core::TupleHeader *>(&tuple_data[header_offset]);
            // Initialize all fields to zero first
            std::memset(header, 0, sizeof(core::TupleHeader));

            // Set the fields we know about
            header->infomask = has_nulls ? core::TupleHeader::HEAP_HAS_NULLS : 0;
            header->null_bitmap_offset = has_nulls ? static_cast<uint16_t>(null_bitmap_offset) : 0;

            // HeapPage::insertTuple() will set:
            // - xmin (from transaction manager)
            // - xmax = 0
            // - back_version_tid = 0 (no back version for new insert)
            // - ctid_page, ctid_item (from final item position)

            // Wave 2: Fire BEFORE INSERT triggers
            std::vector<core::CatalogManager::TriggerInfo> before_triggers;
            core::ErrorContext err_ctx;
            auto trigger_status = db_->catalog_manager()->listTriggersForTable(
                table_id,
                core::CatalogManager::TriggerEvent::INSERT,
                core::CatalogManager::TriggerTiming::BEFORE,
                before_triggers,
                &err_ctx
            );

            if (trigger_status == core::Status::OK)
            {
                for (const auto& trigger : before_triggers)
                {
                    if (!trigger.enabled) continue;

                    TriggerContext ctx(trigger, nullptr, &values, table_info, all_columns);
                    bool should_continue = fireTrigger(ctx);

                    if (!should_continue)
                    {
                        // BEFORE trigger prevented operation
                        return;  // Don't insert
                    }
                }
            }

            // Security Phase 3.5: Row-Level Security WITH CHECK enforcement for INSERT
            // Build row values for RLS check (before actual insert)
            std::vector<Value> rls_row_values(all_columns.size());
            for (size_t i = 0; i < values.size(); i++)
            {
                rls_row_values[col_indices[i]] = values[i];
            }
            // ALPHA Phase A: Fill in DEFAULT values or NULL for columns not specified in INSERT
            for (size_t i = 0; i < all_columns.size(); i++)
            {
                bool found = false;
                for (size_t j = 0; j < col_indices.size(); j++)
                {
                    if (col_indices[j] == i)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    // Check if column has DEFAULT value
                    if (all_columns[i].has_default && !all_columns[i].default_value.empty())
                    {
                        // Try to evaluate DEFAULT value
                        Value default_val = evaluateDefaultValue(all_columns[i]);
                        rls_row_values[i] = default_val;
                    }
                    else
                    {
                        // No DEFAULT - use NULL
                        rls_row_values[i] = Value(); // NULL
                    }
                }
            }

            // Check RLS policies with WITH CHECK
            if (!checkRLSPolicies(table_id, rls_row_values, all_columns,
                                 core::CatalogManager::PolicyType::INSERT,
                                 true /* is_with_check */))
            {
                error("Row-level security policy violation: INSERT WITH CHECK constraint failed");
            }

            // ALPHA Phase A+: Enforce NOT NULL constraints (Nov 19, 2025)
            for (size_t i = 0; i < all_columns.size(); i++)
            {
                const auto& col = all_columns[i];
                if (!col.nullable && rls_row_values[i].isNull())
                {
                    error("NOT NULL constraint violation: NULL value in column '" + col.column_name + "'");
                }
            }

            // ALPHA Phase A+: Enforce data type validation (Nov 19, 2025)
            // Note: Type coercion happens during expression evaluation, but we validate
            // that the final values match the column types
            for (size_t i = 0; i < all_columns.size(); i++)
            {
                const auto& col = all_columns[i];
                const auto& val = rls_row_values[i];

                // Skip NULL values (already handled by NOT NULL check above)
                if (val.isNull())
                {
                    continue;
                }

                // Check type compatibility
                bool type_compatible = false;
                switch (static_cast<core::DataType>(col.data_type))
                {
                    case core::DataType::INT32:
                        type_compatible = (val.type() == core::DataType::INT32 ||
                                         val.type() == core::DataType::INT64);  // Allow implicit INT64->INT32 if in range
                        break;
                    case core::DataType::INT64:
                        type_compatible = (val.type() == core::DataType::INT32 ||
                                         val.type() == core::DataType::INT64);
                        break;
                    case core::DataType::FLOAT64:
                        type_compatible = (val.type() == core::DataType::FLOAT64 ||
                                         val.type() == core::DataType::INT32 ||
                                         val.type() == core::DataType::INT64);  // Allow numeric -> float
                        break;
                    case core::DataType::VARCHAR:
                    case core::DataType::TEXT:
                        type_compatible = (val.type() == core::DataType::VARCHAR ||
                                         val.type() == core::DataType::TEXT);
                        break;
                    case core::DataType::BOOLEAN:
                        type_compatible = (val.type() == core::DataType::BOOLEAN);
                        break;
                    // Add more types as needed
                    default:
                        // For other types, require exact match
                        type_compatible = (val.type() == static_cast<core::DataType>(col.data_type));
                        break;
                }

                if (!type_compatible)
                {
                    error("Type mismatch: cannot insert value of type " +
                          std::to_string(static_cast<int>(val.type())) +
                          " into column '" + col.column_name + "' of type " +
                          std::to_string(static_cast<int>(col.data_type)));
                }
            }

            // ALPHA Phase A+: Enforce PRIMARY KEY constraints (Nov 19, 2025)
            for (size_t i = 0; i < all_columns.size(); i++)
            {
                const auto& col = all_columns[i];
                if (col.is_primary_key)
                {
                    // PRIMARY KEY = NOT NULL + UNIQUE
                    if (rls_row_values[i].isNull())
                    {
                        error("PRIMARY KEY constraint violation: NULL value in column '" + col.column_name + "'");
                    }
                    // Check uniqueness (already handled by UNIQUE check below, but we ensure it's checked)
                    if (checkUniqueViolation(table_id, col, rls_row_values[i], all_columns))
                    {
                        error("PRIMARY KEY constraint violation: duplicate value in column '" + col.column_name + "'");
                    }
                }
            }

            // ALPHA Phase A: Enforce CHECK constraints on columns
            for (size_t i = 0; i < all_columns.size(); i++)
            {
                const auto& col = all_columns[i];
                if (col.check_expr_oid != 0)
                {
                    // Column has a CHECK constraint - evaluate it
                    if (!evaluateCheckConstraint(col, rls_row_values, all_columns))
                    {
                        error("CHECK constraint violation on column '" + col.column_name + "'");
                    }
                }
            }

            // ALPHA Phase A: Enforce UNIQUE constraints on columns
            for (size_t i = 0; i < all_columns.size(); i++)
            {
                const auto& col = all_columns[i];
                // Skip if already checked as PRIMARY KEY
                if (col.is_primary_key) continue;

                if (col.is_unique && !rls_row_values[i].isNull())
                {
                    // Column has a UNIQUE constraint and value is not NULL
                    // Check if value already exists in table
                    if (checkUniqueViolation(table_id, col, rls_row_values[i], all_columns))
                    {
                        error("UNIQUE constraint violation on column '" + col.column_name + "'");
                    }
                }
            }

            // ALPHA Phase A: Enforce FOREIGN KEY constraints on columns
            // Get all FKs for this table (where this table is the child)
            std::vector<core::CatalogManager::ForeignKeyInfo> fks;
            auto fk_status = db_->catalog_manager()->getForeignKeysForTable(table_id, fks, nullptr);

            if (fk_status == core::Status::OK)
            {
                for (const auto& fk : fks)
                {
                    if (!fk.is_enabled) continue;

                    // Collect FK column values from the row being inserted
                    std::vector<Value> fk_values;
                    for (const auto& col_name : fk.child_columns)
                    {
                        // Find column index
                        for (size_t i = 0; i < all_columns.size(); i++)
                        {
                            if (all_columns[i].column_name == col_name)
                            {
                                fk_values.push_back(rls_row_values[i]);
                                break;
                            }
                        }
                    }

                    // Get parent table columns
                    std::vector<core::CatalogManager::ColumnInfo> parent_cols;
                    auto col_status = db_->catalog_manager()->getColumns(fk.parent_table_id, parent_cols, nullptr);

                    if (col_status == core::Status::OK)
                    {
                        // Check if FK values exist in parent table (MATCH SIMPLE semantics)
                        if (!checkForeignKeyExists(fk.parent_table_id, fk.parent_columns, fk_values, parent_cols))
                        {
                            error("Foreign key constraint violation: no matching row in parent table for FK '" + fk.fk_name + "'");
                        }
                    }
                }
            }

            // Insert tuple via storage engine
            uint32_t page_id;
            uint16_t item_id;
            auto insert_status = db_->storage_engine()->insertTuple(
                table_id, tuple_data.data(), static_cast<uint32_t>(tuple_data.size()), &page_id,
                &item_id, nullptr);

            if (insert_status != core::Status::OK)
            {
                error("Failed to insert tuple into storage");
            }

            // Task 17 Phase 7: Update expression/filtered indexes
            // Build full row values in column order (all_columns)
            std::vector<Value> row_values(all_columns.size());
            for (size_t i = 0; i < values.size(); i++)
            {
                row_values[col_indices[i]] = values[i];
            }
            // Fill in default/NULL for columns not specified
            for (size_t i = 0; i < all_columns.size(); i++)
            {
                bool found = false;
                for (size_t j = 0; j < col_indices.size(); j++)
                {
                    if (col_indices[j] == i)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    row_values[i] = Value(); // NULL
                }
            }
            // Task 17 MGA Phase 1.1: Pass current transaction ID
            uint64_t xid = db_->storage_engine()->getCurrentXid();
            updateIndexesOnInsert(xid, table_id, table_info, all_columns, page_id, item_id, row_values);

            // Wave 2: Fire AFTER INSERT triggers
            std::vector<core::CatalogManager::TriggerInfo> after_triggers;
            trigger_status = db_->catalog_manager()->listTriggersForTable(
                table_id,
                core::CatalogManager::TriggerEvent::INSERT,
                core::CatalogManager::TriggerTiming::AFTER,
                after_triggers,
                &err_ctx
            );

            if (trigger_status == core::Status::OK)
            {
                for (const auto& trigger : after_triggers)
                {
                    if (!trigger.enabled) continue;

                    TriggerContext ctx(trigger, nullptr, &values, table_info, all_columns);
                    fireTrigger(ctx);  // AFTER triggers don't prevent operation
                }
            }

            // Success - tuple inserted
        }

        void Executor::executeUpdate()
        {
            // Phase 1 Task 1.6.1: UPDATE executor implementation
            // UPDATE table_name SET assignments WHERE condition

            // SECURITY ENHANCEMENT (MEDIUM-3): Check query limits before expensive UPDATE operation
            checkQueryLimits();

            // Read TABLE_REF opcode
            if (readByte() != static_cast<uint8_t>(Opcode::TABLE_REF))
            {
                error("Expected TABLE_REF in UPDATE");
            }

            std::string table_name = readString();

            // Get default schema (PUBLIC)
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get default schema");
            }

            // Get table from catalog
            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info,
                                                      nullptr);
            if (status != core::Status::OK)
            {
                error("Table not found: " + table_name);
            }
            core::ID table_id = table_info.table_id;

            // Check UPDATE permission on table (VERIFIED mode - security-critical)
            // SECURITY ENHANCEMENT (MEDIUM-1): Use VERIFIED mode for UPDATE (data modification operation)
            bool has_table_update = checkPermission(table_info.table_id,
                               core::CatalogManager::PermissionObjectType::TABLE,
                               static_cast<uint32_t>(core::CatalogManager::Privilege::UPDATE),
                               core::PermissionCheckMode::VERIFIED);

            // Security Phase 3.3.5: Get accessible columns for UPDATE if no table-level permission
            std::vector<std::string> accessible_update_columns;
            if (!has_table_update)
            {
                // Check column-level UPDATE permissions
                core::ErrorContext err_ctx;
                const auto& user_id = getCurrentUserID();
                status = db_->catalog_manager()->getAccessibleColumns(
                    user_id, table_info.table_id,
                    core::CatalogManager::Privilege::UPDATE,
                    accessible_update_columns, &err_ctx);

                if (status != core::Status::OK || accessible_update_columns.empty())
                {
                    // No table-level and no column-level UPDATE permissions
                    error("Permission denied: UPDATE on table " + table_name);
                }
            }
            // If has_table_update is true, accessible_update_columns remains empty = all columns updatable

            // Get column information
            std::vector<core::CatalogManager::ColumnInfo> all_columns;
            status = db_->catalog_manager()->getColumns(table_id, all_columns, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get table columns");
            }

            // Read assignments list
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for assignments");
            }

            uint32_t assignment_count = readInt32();

            // Parse assignments: save bytecode positions for later evaluation
            struct AssignmentInfo {
                std::string column_name;
                size_t expr_start_pc;
                size_t expr_end_pc;
                size_t column_index; // Index into all_columns
            };
            std::vector<AssignmentInfo> assignments;

            for (uint32_t i = 0; i < assignment_count; i++)
            {
                if (readByte() != static_cast<uint8_t>(Opcode::ASSIGNMENT))
                {
                    error("Expected ASSIGNMENT in assignments list");
                }

                // Read column reference
                if (readByte() != static_cast<uint8_t>(Opcode::COLUMN_REF))
                {
                    error("Expected COLUMN_REF in ASSIGNMENT");
                }

                std::string col_name = readString();

                // Find column index
                auto it = std::find_if(all_columns.begin(), all_columns.end(),
                                       [&col_name](const auto &c)
                                       { return c.column_name == col_name; });

                if (it == all_columns.end())
                {
                    error("Column not found in UPDATE: " + col_name);
                }

                // Security Phase 3.3.5: Check UPDATE permission on this column
                if (!accessible_update_columns.empty() &&
                    std::find(accessible_update_columns.begin(), accessible_update_columns.end(), col_name)
                        == accessible_update_columns.end())
                {
                    error("Permission denied: UPDATE on column " + col_name + " of table " + table_name);
                }

                size_t col_idx = std::distance(all_columns.begin(), it);

                // Save start of value expression
                size_t expr_start = pc_;

                // Skip over the expression to find its end
                int depth = 0;
                while (pc_ < bytecode_size_)
                {
                    Opcode op = static_cast<Opcode>(bytecode_[pc_]);

                    // Check if we've reached the next ASSIGNMENT or END_LIST
                    if (depth == 0 && (op == Opcode::ASSIGNMENT || op == Opcode::END_LIST))
                    {
                        break;
                    }

                    readByte(); // consume opcode

                    // Handle opcodes that affect depth or have data
                    if (op == Opcode::LITERAL_INT32)
                    {
                        pc_ += 4;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_INT64)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_DOUBLE)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_STRING || op == Opcode::COLUMN_REF)
                    {
                        uint32_t len = readInt32();
                        pc_ += len;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_NULL)
                    {
                        depth++;
                    }
                    else if (op >= Opcode::EXPR_ADD && op <= Opcode::EXPR_OR)
                    {
                        depth--; // Binary operators: consume 2, produce 1
                    }
                }

                size_t expr_end = pc_;

                assignments.push_back({col_name, expr_start, expr_end, col_idx});
            }

            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST after assignments");
            }

            // Check for WHERE clause
            size_t where_start_pc = 0;
            size_t where_end_pc = 0;
            bool has_where = false;

            if (pc_ < bytecode_size_ &&
                bytecode_[pc_] == static_cast<uint8_t>(Opcode::WHERE_CLAUSE))
            {
                has_where = true;
                readByte(); // Consume WHERE_CLAUSE opcode
                where_start_pc = pc_;

                // Skip over WHERE expression (same logic as SELECT)
                int depth = 1;
                while (pc_ < bytecode_size_ && depth > 0)
                {
                    Opcode op = static_cast<Opcode>(readByte());

                    if (op == Opcode::LITERAL_INT32)
                    {
                        pc_ += 4;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_INT64)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_DOUBLE)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_STRING || op == Opcode::COLUMN_REF)
                    {
                        uint32_t len = readInt32();
                        pc_ += len;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_NULL)
                    {
                        depth++;
                    }
                    else if (op >= Opcode::EXPR_ADD && op <= Opcode::EXPR_OR)
                    {
                        depth--;
                    }
                }
                where_end_pc = pc_;
            }

            // Create table scan iterator
            auto scan_iter = db_->storage_engine()->createScan(table_id, nullptr);
            if (!scan_iter)
            {
                error("Failed to create table scan iterator");
            }

            // Scan all tuples and update matching ones
            int affected_count = 0;
            core::Tuple tuple;

            while (scan_iter->next(&tuple, nullptr) == core::Status::OK)
            {
                // Deserialize current tuple data
                std::vector<Value> row_values;
                if (!deserializeTuple(tuple.data, tuple.data_size, all_columns, row_values))
                {
                    continue; // Skip malformed tuples
                }

                // Evaluate WHERE clause if present
                bool should_update = true;
                if (has_where)
                {
                    size_t saved_pc = pc_;
                    pc_ = where_start_pc;

                    // Set up row context for column references
                    current_row_values_ = &row_values;
                    current_row_columns_ = &all_columns;

                    try
                    {
                        evaluateExpression();
                        Value where_result = pop();

                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;

                        pc_ = saved_pc;

                        should_update = where_result.toBoolean();
                    }
                    catch (...)
                    {
                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;
                        pc_ = saved_pc;
                        throw;
                    }
                }

                if (!should_update)
                {
                    continue;
                }

                // Security Phase 3.5: Row-Level Security USING enforcement for UPDATE
                // Check if user can see/modify this row (old row values)
                if (!checkRLSPolicies(table_id, row_values, all_columns,
                                     core::CatalogManager::PolicyType::UPDATE,
                                     false /* is_using, not with_check */))
                {
                    // Row not visible to user - skip update
                    continue;
                }

                // Wave 2: Save old row values for triggers
                std::vector<Value> old_row_values = row_values;

                // Evaluate assignments and update row values
                for (const auto &assign : assignments)
                {
                    size_t saved_pc = pc_;
                    pc_ = assign.expr_start_pc;

                    // Set up row context for column references in assignment expression
                    current_row_values_ = &row_values;
                    current_row_columns_ = &all_columns;

                    try
                    {
                        evaluateExpression();
                        Value new_value = pop();

                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;

                        pc_ = saved_pc;

                        // Update the row value
                        row_values[assign.column_index] = new_value;
                    }
                    catch (...)
                    {
                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;
                        pc_ = saved_pc;
                        throw;
                    }
                }

                // Security Phase 3.5: Row-Level Security WITH CHECK enforcement for UPDATE
                // Check if new row values pass policies (after assignments)
                if (!checkRLSPolicies(table_id, row_values, all_columns,
                                     core::CatalogManager::PolicyType::UPDATE,
                                     true /* is_with_check */))
                {
                    error("Row-level security policy violation: UPDATE WITH CHECK constraint failed");
                }

                // ALPHA Phase A+: Enforce NOT NULL constraints on updated columns (Nov 19, 2025)
                for (const auto& assign : assignments)
                {
                    const auto& col = all_columns[assign.column_index];
                    if (!col.nullable && row_values[assign.column_index].isNull())
                    {
                        error("NOT NULL constraint violation: cannot set NULL value in column '" + col.column_name + "'");
                    }
                }

                // ALPHA Phase A+: Enforce data type validation on updated columns (Nov 19, 2025)
                for (const auto& assign : assignments)
                {
                    const auto& col = all_columns[assign.column_index];
                    const auto& val = row_values[assign.column_index];

                    // Skip NULL values (already handled by NOT NULL check above)
                    if (val.isNull())
                    {
                        continue;
                    }

                    // Check type compatibility
                    bool type_compatible = false;
                    switch (static_cast<core::DataType>(col.data_type))
                    {
                        case core::DataType::INT32:
                            type_compatible = (val.type() == core::DataType::INT32 ||
                                             val.type() == core::DataType::INT64);
                            break;
                        case core::DataType::INT64:
                            type_compatible = (val.type() == core::DataType::INT32 ||
                                             val.type() == core::DataType::INT64);
                            break;
                        case core::DataType::FLOAT64:
                            type_compatible = (val.type() == core::DataType::FLOAT64 ||
                                             val.type() == core::DataType::INT32 ||
                                             val.type() == core::DataType::INT64);
                            break;
                        case core::DataType::VARCHAR:
                        case core::DataType::TEXT:
                            type_compatible = (val.type() == core::DataType::VARCHAR ||
                                             val.type() == core::DataType::TEXT);
                            break;
                        case core::DataType::BOOLEAN:
                            type_compatible = (val.type() == core::DataType::BOOLEAN);
                            break;
                        default:
                            type_compatible = (val.type() == static_cast<core::DataType>(col.data_type));
                            break;
                    }

                    if (!type_compatible)
                    {
                        error("Type mismatch: cannot update column '" + col.column_name +
                              "' of type " + std::to_string(static_cast<int>(col.data_type)) +
                              " with value of type " + std::to_string(static_cast<int>(val.type())));
                    }
                }

                // ALPHA Phase A+: Enforce PRIMARY KEY constraints on updated columns (Nov 19, 2025)
                for (const auto& assign : assignments)
                {
                    const auto& col = all_columns[assign.column_index];
                    if (col.is_primary_key)
                    {
                        // PRIMARY KEY = NOT NULL + UNIQUE
                        if (row_values[assign.column_index].isNull())
                        {
                            error("PRIMARY KEY constraint violation: cannot set NULL value in PRIMARY KEY column '" + col.column_name + "'");
                        }
                        // Check uniqueness
                        if (checkUniqueViolationForUpdate(table_id, col, row_values[assign.column_index],
                                                         all_columns, tuple.tid))
                        {
                            error("PRIMARY KEY constraint violation: duplicate value in column '" + col.column_name + "'");
                        }
                    }
                }

                // ALPHA Phase A: Enforce CHECK constraints on updated columns
                for (const auto& assign : assignments)
                {
                    const auto& col = all_columns[assign.column_index];
                    if (col.check_expr_oid != 0)
                    {
                        // Column has a CHECK constraint - evaluate it
                        if (!evaluateCheckConstraint(col, row_values, all_columns))
                        {
                            error("CHECK constraint violation on column '" + col.column_name + "'");
                        }
                    }
                }

                // ALPHA Phase A: Enforce UNIQUE constraints on updated columns
                for (const auto& assign : assignments)
                {
                    const auto& col = all_columns[assign.column_index];
                    // Skip if already checked as PRIMARY KEY
                    if (col.is_primary_key) continue;

                    if (col.is_unique && !row_values[assign.column_index].isNull())
                    {
                        // Column has a UNIQUE constraint and new value is not NULL
                        // Check if the new value already exists in another row
                        // Note: We need to exclude the current row from the check
                        if (checkUniqueViolationForUpdate(table_id, col, row_values[assign.column_index],
                                                         all_columns, tuple.tid))
                        {
                            error("UNIQUE constraint violation on column '" + col.column_name + "'");
                        }
                    }
                }

                // ALPHA Phase A: Enforce FOREIGN KEY constraints on updated columns
                // Get all FKs for this table (where this table is the child)
                std::vector<core::CatalogManager::ForeignKeyInfo> fks;
                auto fk_status = db_->catalog_manager()->getForeignKeysForTable(table_id, fks, nullptr);

                if (fk_status == core::Status::OK)
                {
                    for (const auto& fk : fks)
                    {
                        if (!fk.is_enabled) continue;

                        // Check if any FK column was updated
                        bool fk_updated = false;
                        for (const auto& assign : assignments)
                        {
                            const auto& col_name = all_columns[assign.column_index].column_name;
                            if (std::find(fk.child_columns.begin(), fk.child_columns.end(), col_name)
                                != fk.child_columns.end())
                            {
                                fk_updated = true;
                                break;
                            }
                        }

                        if (fk_updated)
                        {
                            // Collect new FK column values from the updated row
                            std::vector<Value> fk_values;
                            for (const auto& col_name : fk.child_columns)
                            {
                                for (size_t i = 0; i < all_columns.size(); i++)
                                {
                                    if (all_columns[i].column_name == col_name)
                                    {
                                        fk_values.push_back(row_values[i]);
                                        break;
                                    }
                                }
                            }

                            // Get parent table columns
                            std::vector<core::CatalogManager::ColumnInfo> parent_cols;
                            auto col_status = db_->catalog_manager()->getColumns(fk.parent_table_id, parent_cols, nullptr);

                            if (col_status == core::Status::OK)
                            {
                                // Check if new FK values exist in parent table (MATCH SIMPLE semantics)
                                if (!checkForeignKeyExists(fk.parent_table_id, fk.parent_columns, fk_values, parent_cols))
                                {
                                    error("Foreign key constraint violation: no matching row in parent table for FK '" + fk.fk_name + "'");
                                }
                            }
                        }
                    }
                }

                // Serialize updated tuple data (same format as INSERT)
                std::vector<uint8_t> new_tuple_data;

                // Reserve space for TupleHeader
                new_tuple_data.resize(sizeof(core::TupleHeader));

                // Determine if we need a null bitmap
                bool has_nulls = false;
                for (const auto &val : row_values)
                {
                    if (val.isNull())
                    {
                        has_nulls = true;
                        break;
                    }
                }

                // Add null bitmap if needed
                if (has_nulls)
                {
                    size_t num_bytes = (all_columns.size() + 7) / 8;
                    size_t bitmap_offset = new_tuple_data.size();
                    new_tuple_data.resize(new_tuple_data.size() + num_bytes, 0);

                    for (size_t i = 0; i < row_values.size(); i++)
                    {
                        if (row_values[i].isNull())
                        {
                            size_t byte_idx = i / 8;
                            size_t bit_idx = i % 8;
                            new_tuple_data[bitmap_offset + byte_idx] |= (1 << bit_idx);
                        }
                    }
                }

                // Serialize column data
                for (size_t i = 0; i < row_values.size(); i++)
                {
                    if (row_values[i].isNull())
                    {
                        continue; // NULL values already marked in bitmap
                    }

                    const auto &val = row_values[i];
                    const auto &col = all_columns[i];

                    // Serialize based on column data type
                    switch (static_cast<core::DataType>(col.data_type))
                    {
                        case core::DataType::INT32:
                        {
                            int32_t int_val = static_cast<int32_t>(val.toInt64());
                            new_tuple_data.insert(new_tuple_data.end(),
                                                  reinterpret_cast<const uint8_t *>(&int_val),
                                                  reinterpret_cast<const uint8_t *>(&int_val) + 4);
                            break;
                        }
                        case core::DataType::INT64:
                        {
                            int64_t long_val = val.toInt64();
                            new_tuple_data.insert(new_tuple_data.end(),
                                                  reinterpret_cast<const uint8_t *>(&long_val),
                                                  reinterpret_cast<const uint8_t *>(&long_val) + 8);
                            break;
                        }
                        case core::DataType::FLOAT64:
                        {
                            double dbl_val = val.toDouble();
                            new_tuple_data.insert(new_tuple_data.end(),
                                                  reinterpret_cast<const uint8_t *>(&dbl_val),
                                                  reinterpret_cast<const uint8_t *>(&dbl_val) + 8);
                            break;
                        }
                        case core::DataType::BOOLEAN:
                        {
                            bool bool_val = val.toBoolean();
                            new_tuple_data.push_back(bool_val ? 1 : 0);
                            break;
                        }
                        case core::DataType::VARCHAR:
                        {
                            std::string str_val = val.toString();
                            uint32_t str_len = static_cast<uint32_t>(str_val.size());
                            new_tuple_data.insert(new_tuple_data.end(),
                                                  reinterpret_cast<const uint8_t *>(&str_len),
                                                  reinterpret_cast<const uint8_t *>(&str_len) + 4);
                            new_tuple_data.insert(new_tuple_data.end(), str_val.begin(), str_val.end());
                            break;
                        }
                        default:
                            error("Unsupported data type in UPDATE");
                    }
                }

                // Wave 2: Fire BEFORE UPDATE triggers
                std::vector<core::CatalogManager::TriggerInfo> before_triggers;
                core::ErrorContext err_ctx;
                auto trigger_status = db_->catalog_manager()->listTriggersForTable(
                    table_id,
                    core::CatalogManager::TriggerEvent::UPDATE,
                    core::CatalogManager::TriggerTiming::BEFORE,
                    before_triggers,
                    &err_ctx
                );

                bool should_continue = true;
                if (trigger_status == core::Status::OK)
                {
                    for (const auto& trig : before_triggers)
                    {
                        if (!trig.enabled) continue;

                        TriggerContext ctx(trig, &old_row_values, &row_values, table_info, all_columns);
                        should_continue = fireTrigger(ctx);

                        if (!should_continue)
                        {
                            // BEFORE trigger prevented operation
                            continue;  // Skip this row
                        }
                    }
                }

                if (!should_continue)
                {
                    continue;  // Skip this row if trigger prevented update
                }

                // Call StorageEngine::updateTuple with MGA versioning
                uint32_t page_id = static_cast<uint32_t>(core::getPageNumber(tuple.tid));
                uint16_t item_id = core::getSlot(tuple.tid);
                uint32_t new_page_id;
                uint16_t new_item_id;

                auto update_status = db_->storage_engine()->updateTuple(
                    table_id, page_id, item_id,
                    new_tuple_data.data(), static_cast<uint32_t>(new_tuple_data.size()),
                    &new_page_id, &new_item_id, nullptr);

                if (update_status != core::Status::OK)
                {
                    error("Failed to update tuple in storage");
                }

                // Task 17 Phase 7: Update expression/filtered indexes
                core::TID old_tid(page_id, item_id);
                core::TID new_tid(new_page_id, new_item_id);
                // Task 17 MGA Phase 1.1: Pass current transaction ID
                uint64_t xid = db_->storage_engine()->getCurrentXid();
                updateIndexesOnUpdate(xid, table_id, table_info, all_columns, old_row_values, row_values, old_tid, new_tid);

                // Wave 2: Fire AFTER UPDATE triggers
                std::vector<core::CatalogManager::TriggerInfo> after_triggers;
                trigger_status = db_->catalog_manager()->listTriggersForTable(
                    table_id,
                    core::CatalogManager::TriggerEvent::UPDATE,
                    core::CatalogManager::TriggerTiming::AFTER,
                    after_triggers,
                    &err_ctx
                );

                if (trigger_status == core::Status::OK)
                {
                    for (const auto& trig : after_triggers)
                    {
                        if (!trig.enabled) continue;

                        TriggerContext ctx(trig, &old_row_values, &row_values, table_info, all_columns);
                        fireTrigger(ctx);  // AFTER triggers don't prevent operation
                    }
                }

                affected_count++;
            }

            // Note: Index updates are handled automatically by StorageEngine
            // in the updateTuple() method for MGA architecture
        }

        void Executor::executeDelete()
        {
            // Phase 1 Task 1.6.2: DELETE executor implementation
            // DELETE FROM table_name WHERE condition

            // SECURITY ENHANCEMENT (MEDIUM-3): Check query limits before expensive DELETE operation
            checkQueryLimits();

            // Read TABLE_REF opcode
            if (readByte() != static_cast<uint8_t>(Opcode::TABLE_REF))
            {
                error("Expected TABLE_REF in DELETE");
            }

            std::string table_name = readString();

            // Get default schema (PUBLIC)
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get default schema");
            }

            // Get table from catalog
            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info,
                                                      nullptr);
            if (status != core::Status::OK)
            {
                error("Table not found: " + table_name);
            }
            core::ID table_id = table_info.table_id;

            // Check DELETE permission on table (VERIFIED mode - security-critical)
            // SECURITY ENHANCEMENT (MEDIUM-1): Use VERIFIED mode for DELETE (data loss operation)
            if (!checkPermission(table_info.table_id,
                               core::CatalogManager::PermissionObjectType::TABLE,
                               static_cast<uint32_t>(core::CatalogManager::Privilege::DELETE),
                               core::PermissionCheckMode::VERIFIED))
            {
                error("Permission denied: DELETE on table " + table_name);
            }

            // Get column information for WHERE clause evaluation
            std::vector<core::CatalogManager::ColumnInfo> all_columns;
            status = db_->catalog_manager()->getColumns(table_id, all_columns, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get table columns");
            }

            // Check for WHERE clause
            size_t where_start_pc = 0;
            size_t where_end_pc = 0;
            bool has_where = false;

            if (pc_ < bytecode_size_ &&
                bytecode_[pc_] == static_cast<uint8_t>(Opcode::WHERE_CLAUSE))
            {
                has_where = true;
                readByte(); // Consume WHERE_CLAUSE opcode
                where_start_pc = pc_;

                // Skip over WHERE expression
                int depth = 1;
                while (pc_ < bytecode_size_ && depth > 0)
                {
                    Opcode op = static_cast<Opcode>(readByte());

                    if (op == Opcode::LITERAL_INT32)
                    {
                        pc_ += 4;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_INT64)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_DOUBLE)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_STRING || op == Opcode::COLUMN_REF)
                    {
                        uint32_t len = readInt32();
                        pc_ += len;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_NULL)
                    {
                        depth++;
                    }
                    else if (op >= Opcode::EXPR_ADD && op <= Opcode::EXPR_OR)
                    {
                        depth--;
                    }
                }
                where_end_pc = pc_;
            }

            // Create table scan iterator
            auto scan_iter = db_->storage_engine()->createScan(table_id, nullptr);
            if (!scan_iter)
            {
                error("Failed to create table scan iterator");
            }

            // Scan all tuples and delete matching ones
            int affected_count = 0;
            core::Tuple tuple;

            while (scan_iter->next(&tuple, nullptr) == core::Status::OK)
            {
                // Deserialize tuple data for WHERE evaluation
                std::vector<Value> row_values;
                if (!deserializeTuple(tuple.data, tuple.data_size, all_columns, row_values))
                {
                    continue; // Skip malformed tuples
                }

                // Evaluate WHERE clause if present
                bool should_delete = true;
                if (has_where)
                {
                    size_t saved_pc = pc_;
                    pc_ = where_start_pc;

                    // Set up row context for column references
                    current_row_values_ = &row_values;
                    current_row_columns_ = &all_columns;

                    try
                    {
                        evaluateExpression();
                        Value where_result = pop();

                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;

                        pc_ = saved_pc;

                        should_delete = where_result.toBoolean();
                    }
                    catch (...)
                    {
                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;
                        pc_ = saved_pc;
                        throw;
                    }
                }

                if (!should_delete)
                {
                    continue;
                }

                // Security Phase 3.5: Row-Level Security USING enforcement for DELETE
                // Check if user can see/delete this row
                if (!checkRLSPolicies(table_id, row_values, all_columns,
                                     core::CatalogManager::PolicyType::DELETE,
                                     false /* is_using, not with_check */))
                {
                    // Row not visible to user - skip deletion
                    continue;
                }

                // Wave 2: Fire BEFORE DELETE triggers
                std::vector<core::CatalogManager::TriggerInfo> before_triggers;
                core::ErrorContext err_ctx;
                auto trigger_status = db_->catalog_manager()->listTriggersForTable(
                    table_id,
                    core::CatalogManager::TriggerEvent::DELETE,
                    core::CatalogManager::TriggerTiming::BEFORE,
                    before_triggers,
                    &err_ctx
                );

                bool should_continue = true;
                if (trigger_status == core::Status::OK)
                {
                    for (const auto& trig : before_triggers)
                    {
                        if (!trig.enabled) continue;

                        TriggerContext ctx(trig, &row_values, nullptr, table_info, all_columns);
                        should_continue = fireTrigger(ctx);

                        if (!should_continue)
                        {
                            // BEFORE trigger prevented operation
                            continue;  // Skip this row
                        }
                    }
                }

                if (!should_continue)
                {
                    continue;  // Skip this row if trigger prevented delete
                }

                // Task 17 Phase 7: Update expression/filtered indexes BEFORE deletion
                // (indexes need row values, and deletion is a soft delete that marks xmax)
                uint32_t page_id = static_cast<uint32_t>(core::getPageNumber(tuple.tid));
                uint16_t item_id = core::getSlot(tuple.tid);
                core::TID tid(page_id, item_id);
                // Task 17 MGA Phase 1.1: Pass current transaction ID
                uint64_t xid = db_->storage_engine()->getCurrentXid();
                updateIndexesOnDelete(xid, table_id, table_info, all_columns, row_values, tid);

                // Call StorageEngine::deleteTuple with MGA soft delete
                // This sets xmax = current transaction ID
                core::ConnectionContext *conn_ctx = core::ConnectionContext::getCurrent();
                uint64_t xmax = conn_ctx ? conn_ctx->getCurrentTransactionId() : 0;

                auto delete_status = db_->storage_engine()->deleteTuple(
                    table_id, page_id, item_id, nullptr);

                if (delete_status != core::Status::OK)
                {
                    error("Failed to delete tuple from storage");
                }

                // Wave 2: Fire AFTER DELETE triggers
                std::vector<core::CatalogManager::TriggerInfo> after_triggers;
                trigger_status = db_->catalog_manager()->listTriggersForTable(
                    table_id,
                    core::CatalogManager::TriggerEvent::DELETE,
                    core::CatalogManager::TriggerTiming::AFTER,
                    after_triggers,
                    &err_ctx
                );

                if (trigger_status == core::Status::OK)
                {
                    for (const auto& trig : after_triggers)
                    {
                        if (!trig.enabled) continue;

                        TriggerContext ctx(trig, &row_values, nullptr, table_info, all_columns);
                        fireTrigger(ctx);  // AFTER triggers don't prevent operation
                    }
                }

                affected_count++;
            }

            // Note: Index cleanup is handled automatically by StorageEngine
            // in the deleteTuple() method for MGA architecture
        }

        // Aggregation helper implementations (Phase 1 Task 1.6.3)
        void Executor::AggregateAccumulator::accumulate(const Value& val)
        {
            // Skip NULLs for all aggregate functions except COUNT(*) and ARRAY_AGG
            if (val.isNull() && func != AggFunc::COUNT && func != AggFunc::ARRAY_AGG)
                return;

            // Handle DISTINCT
            if (distinct)
            {
                std::string key = val.toString();
                if (distinct_values.find(key) != distinct_values.end())
                    return; // Already seen this value
                distinct_values.insert(key);
            }

            switch (func)
            {
                case AggFunc::COUNT:
                    // COUNT(*) counts all rows including NULLs
                    // COUNT(col) only counts non-NULL values (handled above)
                    count++;
                    break;

                case AggFunc::SUM:
                    sum += val.toDouble();
                    count++;
                    break;

                case AggFunc::AVG:
                    sum += val.toDouble();
                    count++;
                    break;

                case AggFunc::MIN:
                    if (count == 0 || val.toDouble() < result.toDouble())
                        result = val;
                    count++;
                    break;

                case AggFunc::MAX:
                    if (count == 0 || val.toDouble() > result.toDouble())
                        result = val;
                    count++;
                    break;

                case AggFunc::ARRAY_AGG:
                    // Collect all values into an array (including NULLs unless filtered)
                    array_elements.push_back(val);
                    count++;
                    break;

                // Statistical functions (Nov 14, 2025)
                // Use Welford's online algorithm for numerical stability
                case AggFunc::STDDEV_SAMP:
                case AggFunc::STDDEV_POP:
                case AggFunc::VAR_SAMP:
                case AggFunc::VAR_POP:
                {
                    double value = val.toDouble();
                    count++;
                    double delta = value - mean;
                    mean += delta / count;
                    double delta2 = value - mean;
                    m2 += delta * delta2;
                    break;
                }

                // Note: CORR and COVAR_POP are 2-argument functions
                // They will use accumulate2() instead of accumulate()
                case AggFunc::CORR:
                case AggFunc::COVAR_POP:
                    // These should not reach here - they use accumulate2()
                    // If they do, it's a programming error
                    break;
            }
        }

        // Accumulate for 2-argument statistical functions (CORR, COVAR_POP)
        void Executor::AggregateAccumulator::accumulate2(const Value& val_y, const Value& val_x)
        {
            // Skip NULLs
            if (val_y.isNull() || val_x.isNull())
                return;

            // TODO: Handle DISTINCT for 2-variable functions if needed
            // For now, DISTINCT doesn't make much sense for CORR/COVAR

            double x = val_x.toDouble();
            double y = val_y.toDouble();

            count++;
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_x2 += x * x;
            sum_y2 += y * y;
        }

        Value Executor::AggregateAccumulator::finalize()
        {
            switch (func)
            {
                case AggFunc::COUNT:
                    return Value::makeInt64(count);

                case AggFunc::SUM:
                    return count > 0 ? Value::makeFloat64(sum) : Value::makeNull();

                case AggFunc::AVG:
                    return count > 0 ? Value::makeFloat64(sum / count) : Value::makeNull();

                case AggFunc::MIN:
                case AggFunc::MAX:
                    return count > 0 ? result : Value::makeNull();

                case AggFunc::ARRAY_AGG:
                {
                    // Convert accumulated values to JSON array format
                    json j_array = json::array();
                    for (const auto& elem : array_elements)
                    {
                        if (elem.isNull())
                        {
                            j_array.push_back(nullptr);
                        }
                        else
                        {
                            // Convert Value to appropriate JSON type
                            switch (elem.type())
                            {
                                case core::DataType::INT8:
                                case core::DataType::INT16:
                                case core::DataType::INT32:
                                case core::DataType::INT64:
                                    j_array.push_back(elem.toInt64());
                                    break;
                                case core::DataType::FLOAT32:
                                case core::DataType::FLOAT64:
                                    j_array.push_back(elem.toDouble());
                                    break;
                                case core::DataType::BOOLEAN:
                                    j_array.push_back(elem.toBoolean());
                                    break;
                                case core::DataType::VARCHAR:
                                case core::DataType::TEXT:
                                case core::DataType::CHAR:
                                    j_array.push_back(elem.toString());
                                    break;
                                case core::DataType::JSON:
                                    // Parse existing JSON and add to array
                                    try {
                                        j_array.push_back(json::parse(elem.toString()));
                                    } catch (...) {
                                        j_array.push_back(elem.toString());
                                    }
                                    break;
                                default:
                                    j_array.push_back(elem.toString());
                                    break;
                            }
                        }
                    }
                    return Value::makeJSON(j_array.dump());
                }

                // Statistical functions (Nov 14, 2025)
                case AggFunc::VAR_SAMP:
                    // Sample variance = m2 / (n-1)
                    // Requires at least 2 values
                    return count > 1 ? Value::makeFloat64(m2 / (count - 1)) : Value::makeNull();

                case AggFunc::VAR_POP:
                    // Population variance = m2 / n
                    return count > 0 ? Value::makeFloat64(m2 / count) : Value::makeNull();

                case AggFunc::STDDEV_SAMP:
                    // Sample standard deviation = sqrt(variance)
                    // Requires at least 2 values
                    return count > 1 ? Value::makeFloat64(std::sqrt(m2 / (count - 1))) : Value::makeNull();

                case AggFunc::STDDEV_POP:
                    // Population standard deviation = sqrt(variance)
                    return count > 0 ? Value::makeFloat64(std::sqrt(m2 / count)) : Value::makeNull();

                case AggFunc::CORR:
                {
                    // Pearson correlation coefficient
                    // r = (n*Σxy - Σx*Σy) / sqrt((n*Σx² - (Σx)²) * (n*Σy² - (Σy)²))
                    if (count < 2)
                        return Value::makeNull();

                    double numerator = count * sum_xy - sum_x * sum_y;
                    double denom_x = count * sum_x2 - sum_x * sum_x;
                    double denom_y = count * sum_y2 - sum_y * sum_y;

                    // Check for zero denominator (no variance)
                    if (denom_x <= 0 || denom_y <= 0)
                        return Value::makeNull();

                    double denominator = std::sqrt(denom_x * denom_y);
                    if (denominator == 0.0)
                        return Value::makeNull();

                    return Value::makeFloat64(numerator / denominator);
                }

                case AggFunc::COVAR_POP:
                {
                    // Population covariance
                    // cov(X,Y) = (Σxy)/n - (Σx/n)*(Σy/n)
                    if (count == 0)
                        return Value::makeNull();

                    double mean_x = sum_x / count;
                    double mean_y = sum_y / count;
                    double covariance = (sum_xy / count) - (mean_x * mean_y);

                    return Value::makeFloat64(covariance);
                }
            }

            // Should never reach here
            return Value::makeNull();
        }

        bool Executor::GroupKey::operator==(const GroupKey& other) const
        {
            if (values.size() != other.values.size())
                return false;

            for (size_t i = 0; i < values.size(); i++)
            {
                // Use toString() for comparison to handle different types uniformly
                if (values[i].toString() != other.values[i].toString())
                    return false;
            }

            return true;
        }

        size_t Executor::GroupKey::hash() const
        {
            // Simple hash combining algorithm
            size_t h = 0;
            for (const auto& v : values)
            {
                // Combine hash using boost::hash_combine algorithm
                h ^= std::hash<std::string>{}(v.toString()) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
            return h;
        }

        void Executor::executeAggregate(
            const core::CatalogManager::TableInfo& table_info,
            const std::vector<core::CatalogManager::ColumnInfo>& all_columns,
            const std::vector<std::pair<std::string, std::string>>& select_items,
            bool is_select_star,
            bool has_where,
            size_t where_start_pc,
            size_t where_end_pc)
        {
            // Phase 1 Task 1.6.3: Aggregation execution
            // Parse GROUP BY clause if present
            std::vector<size_t> group_by_expr_pcs;
            bool has_group_by = false;

            if (pc_ < bytecode_size_ && bytecode_[pc_] == static_cast<uint8_t>(Opcode::GROUP_BY))
            {
                has_group_by = true;
                readByte(); // Consume GROUP_BY opcode
                uint32_t group_count = readInt32();

                // Save bytecode positions for each grouping expression
                for (uint32_t i = 0; i < group_count; i++)
                {
                    size_t expr_start = pc_;
                    group_by_expr_pcs.push_back(expr_start);

                    // Skip over expression to find next one
                    int depth = 0;
                    while (pc_ < bytecode_size_)
                    {
                        Opcode op = static_cast<Opcode>(readByte());

                        if (op == Opcode::LITERAL_INT32)
                        {
                            pc_ += 4;
                            depth++;
                        }
                        else if (op == Opcode::LITERAL_INT64)
                        {
                            pc_ += 8;
                            depth++;
                        }
                        else if (op == Opcode::LITERAL_DOUBLE)
                        {
                            pc_ += 8;
                            depth++;
                        }
                        else if (op == Opcode::LITERAL_STRING || op == Opcode::COLUMN_REF)
                        {
                            uint32_t len = readInt32();
                            pc_ += len;
                            depth++;
                        }
                        else if (op == Opcode::LITERAL_NULL)
                        {
                            depth++;
                        }
                        else if (op >= Opcode::EXPR_ADD && op <= Opcode::EXPR_OR)
                        {
                            depth--;
                        }

                        if (depth == 1)
                            break; // Found complete expression
                    }
                }
            }

            // Parse AGG_INIT
            if (readByte() != static_cast<uint8_t>(Opcode::AGG_INIT))
            {
                error("Expected AGG_INIT for aggregation query");
            }

            uint32_t agg_count = readInt32();

            // Parse aggregate function definitions
            struct AggDef
            {
                AggregateAccumulator::AggFunc func;
                bool distinct;
                size_t expr_start_pc;
                size_t expr_end_pc;
            };
            std::vector<AggDef> agg_defs;

            for (uint32_t i = 0; i < agg_count; i++)
            {
                Opcode agg_op = static_cast<Opcode>(readByte());
                AggregateAccumulator::AggFunc func;

                switch (agg_op)
                {
                    case Opcode::AGG_COUNT:
                        func = AggregateAccumulator::AggFunc::COUNT;
                        break;
                    case Opcode::AGG_SUM:
                        func = AggregateAccumulator::AggFunc::SUM;
                        break;
                    case Opcode::AGG_AVG:
                        func = AggregateAccumulator::AggFunc::AVG;
                        break;
                    case Opcode::AGG_MIN:
                        func = AggregateAccumulator::AggFunc::MIN;
                        break;
                    case Opcode::AGG_MAX:
                        func = AggregateAccumulator::AggFunc::MAX;
                        break;
                    case Opcode::ARRAY_AGG:
                        func = AggregateAccumulator::AggFunc::ARRAY_AGG;
                        break;
                    case Opcode::AGG_STDDEV_SAMP:
                        func = AggregateAccumulator::AggFunc::STDDEV_SAMP;
                        break;
                    case Opcode::AGG_STDDEV_POP:
                        func = AggregateAccumulator::AggFunc::STDDEV_POP;
                        break;
                    case Opcode::AGG_VAR_SAMP:
                        func = AggregateAccumulator::AggFunc::VAR_SAMP;
                        break;
                    case Opcode::AGG_VAR_POP:
                        func = AggregateAccumulator::AggFunc::VAR_POP;
                        break;
                    case Opcode::AGG_CORR:
                        func = AggregateAccumulator::AggFunc::CORR;
                        break;
                    case Opcode::AGG_COVAR_POP:
                        func = AggregateAccumulator::AggFunc::COVAR_POP;
                        break;
                    default:
                        error("Unknown aggregate function opcode");
                }

                // Read DISTINCT flag (single byte: 0 or 1)
                uint8_t distinct_flag = readByte();
                bool distinct = (distinct_flag != 0);

                // Save expression bytecode position
                size_t expr_start = pc_;

                // Skip over aggregate expression
                int depth = 0;
                while (pc_ < bytecode_size_)
                {
                    Opcode op = static_cast<Opcode>(readByte());

                    if (op == Opcode::LITERAL_INT32)
                    {
                        pc_ += 4;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_INT64)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_DOUBLE)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_STRING || op == Opcode::COLUMN_REF)
                    {
                        uint32_t len = readInt32();
                        pc_ += len;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_NULL || op == Opcode::SELECT_STAR)
                    {
                        depth++;
                    }
                    else if (op >= Opcode::EXPR_ADD && op <= Opcode::EXPR_OR)
                    {
                        depth--;
                    }

                    if (depth == 1)
                        break;
                }

                size_t expr_end = pc_;
                agg_defs.push_back({func, distinct, expr_start, expr_end});
            }

            // Check for HAVING clause
            bool has_having = false;
            size_t having_start_pc = 0;
            size_t having_end_pc = 0;

            if (pc_ < bytecode_size_ && bytecode_[pc_] == static_cast<uint8_t>(Opcode::HAVING))
            {
                has_having = true;
                readByte(); // Consume HAVING opcode
                having_start_pc = pc_;

                // Skip HAVING expression
                int depth = 1;
                while (pc_ < bytecode_size_ && depth > 0)
                {
                    Opcode op = static_cast<Opcode>(readByte());

                    if (op == Opcode::LITERAL_INT32)
                    {
                        pc_ += 4;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_INT64)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_DOUBLE)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_STRING || op == Opcode::COLUMN_REF)
                    {
                        uint32_t len = readInt32();
                        pc_ += len;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_NULL)
                    {
                        depth++;
                    }
                    else if (op >= Opcode::EXPR_ADD && op <= Opcode::EXPR_OR)
                    {
                        depth--;
                    }
                }
                having_end_pc = pc_;
            }

            // Consume AGG_FINALIZE opcode
            if (readByte() != static_cast<uint8_t>(Opcode::AGG_FINALIZE))
            {
                error("Expected AGG_FINALIZE");
            }

            // Now scan the table and build groups
            GroupMap groups;

            auto scan_iter = db_->storage_engine()->createScan(table_info.table_id, nullptr);
            if (!scan_iter)
            {
                error("Failed to create table scan iterator");
            }

            core::Tuple tuple;
            while (scan_iter->next(&tuple, nullptr) == core::Status::OK)
            {
                // Deserialize tuple
                std::vector<Value> row_values;
                if (!deserializeTuple(tuple.data, tuple.data_size, all_columns, row_values))
                {
                    continue;
                }

                // Evaluate WHERE clause if present
                if (has_where)
                {
                    size_t saved_pc = pc_;
                    pc_ = where_start_pc;
                    current_row_values_ = &row_values;
                    current_row_columns_ = &all_columns;

                    try
                    {
                        evaluateExpression();
                        Value where_result = pop();
                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;
                        pc_ = saved_pc;

                        if (!where_result.toBoolean())
                        {
                            continue; // Skip this row
                        }
                    }
                    catch (...)
                    {
                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;
                        pc_ = saved_pc;
                        throw;
                    }
                }

                // Evaluate GROUP BY expressions to build group key
                GroupKey group_key;

                if (has_group_by)
                {
                    for (size_t expr_pc : group_by_expr_pcs)
                    {
                        size_t saved_pc = pc_;
                        pc_ = expr_pc;
                        current_row_values_ = &row_values;
                        current_row_columns_ = &all_columns;

                        try
                        {
                            evaluateExpression();
                            Value group_val = pop();
                            current_row_values_ = nullptr;
                            current_row_columns_ = nullptr;
                            pc_ = saved_pc;

                            group_key.values.push_back(group_val);
                        }
                        catch (...)
                        {
                            current_row_values_ = nullptr;
                            current_row_columns_ = nullptr;
                            pc_ = saved_pc;
                            throw;
                        }
                    }
                }
                // else: no GROUP BY means single group with empty key

                // Find or create group in hash table
                auto& group_state = groups[group_key];

                // Initialize accumulators if this is first row in group
                if (group_state.empty())
                {
                    for (const auto& agg_def : agg_defs)
                    {
                        group_state.emplace_back(agg_def.func, agg_def.distinct);
                    }
                }

                // Accumulate values for each aggregate
                for (size_t i = 0; i < agg_defs.size(); i++)
                {
                    const auto& agg_def = agg_defs[i];

                    // Check if this is a 2-argument function (CORR, COVAR_POP)
                    bool is_two_arg = (agg_def.func == AggregateAccumulator::AggFunc::CORR ||
                                       agg_def.func == AggregateAccumulator::AggFunc::COVAR_POP);

                    // Evaluate aggregate expression(s)
                    size_t saved_pc = pc_;
                    pc_ = agg_def.expr_start_pc;
                    current_row_values_ = &row_values;
                    current_row_columns_ = &all_columns;

                    try
                    {
                        if (is_two_arg)
                        {
                            // For 2-argument functions, expressions are already on bytecode
                            // We need to evaluate them and get 2 values from the stack
                            // The bytecode generator wrote both args as expressions
                            evaluateExpression(); // First arg (y)
                            Value val1 = pop();
                            evaluateExpression(); // Second arg (x)
                            Value val2 = pop();
                            current_row_values_ = nullptr;
                            current_row_columns_ = nullptr;
                            pc_ = saved_pc;

                            group_state[i].accumulate2(val1, val2);
                        }
                        else
                        {
                            evaluateExpression();
                            Value agg_val = pop();
                            current_row_values_ = nullptr;
                            current_row_columns_ = nullptr;
                            pc_ = saved_pc;

                            group_state[i].accumulate(agg_val);
                        }
                    }
                    catch (...)
                    {
                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;
                        pc_ = saved_pc;
                        throw;
                    }
                }
            }

            // Build result set from groups
            current_result_set_ = std::make_unique<ResultSet>();

            // Add columns: GROUP BY columns followed by aggregate columns
            if (has_group_by)
            {
                for (size_t i = 0; i < group_by_expr_pcs.size(); i++)
                {
                    std::string col_name = "group_" + std::to_string(i);
                    current_result_set_->addColumn(col_name, core::DataType::VARCHAR);
                }
            }

            for (size_t i = 0; i < agg_defs.size(); i++)
            {
                std::string agg_name;
                core::DataType agg_type = core::DataType::FLOAT64;
                switch (agg_defs[i].func)
                {
                    case AggregateAccumulator::AggFunc::COUNT: agg_name = "COUNT"; break;
                    case AggregateAccumulator::AggFunc::SUM: agg_name = "SUM"; break;
                    case AggregateAccumulator::AggFunc::AVG: agg_name = "AVG"; break;
                    case AggregateAccumulator::AggFunc::MIN: agg_name = "MIN"; break;
                    case AggregateAccumulator::AggFunc::MAX: agg_name = "MAX"; break;
                    case AggregateAccumulator::AggFunc::ARRAY_AGG:
                        agg_name = "ARRAY_AGG";
                        agg_type = core::DataType::JSON;
                        break;
                    case AggregateAccumulator::AggFunc::STDDEV_SAMP: agg_name = "STDDEV_SAMP"; break;
                    case AggregateAccumulator::AggFunc::STDDEV_POP: agg_name = "STDDEV_POP"; break;
                    case AggregateAccumulator::AggFunc::VAR_SAMP: agg_name = "VAR_SAMP"; break;
                    case AggregateAccumulator::AggFunc::VAR_POP: agg_name = "VAR_POP"; break;
                    case AggregateAccumulator::AggFunc::CORR: agg_name = "CORR"; break;
                    case AggregateAccumulator::AggFunc::COVAR_POP: agg_name = "COVAR_POP"; break;
                }
                current_result_set_->addColumn(agg_name, agg_type);
            }

            // Add rows from groups
            for (auto& [group_key, group_state] : groups)
            {
                std::vector<Value> result_row;

                // Add GROUP BY values
                for (const auto& group_val : group_key.values)
                {
                    result_row.push_back(group_val);
                }

                // Add aggregate results
                for (auto& accumulator : group_state)
                {
                    result_row.push_back(accumulator.finalize());
                }

                // Evaluate HAVING clause if present
                if (has_having)
                {
                    // Set up context for HAVING evaluation
                    // For HAVING clause, the "row" is the aggregate result row
                    // We need to create a temporary column list that matches the result row
                    std::vector<core::CatalogManager::ColumnInfo> result_columns;

                    // Add GROUP BY columns
                    if (has_group_by)
                    {
                        for (size_t i = 0; i < group_by_expr_pcs.size(); i++)
                        {
                            core::CatalogManager::ColumnInfo col_info;
                            col_info.column_name = "group_" + std::to_string(i);
                            col_info.data_type = static_cast<uint16_t>(core::DataType::VARCHAR);
                            result_columns.push_back(col_info);
                        }
                    }

                    // Add aggregate columns
                    for (size_t i = 0; i < agg_defs.size(); i++)
                    {
                        core::CatalogManager::ColumnInfo col_info;
                        col_info.data_type = static_cast<uint16_t>(core::DataType::FLOAT64);
                        switch (agg_defs[i].func)
                        {
                            case AggregateAccumulator::AggFunc::COUNT: col_info.column_name = "COUNT"; break;
                            case AggregateAccumulator::AggFunc::SUM: col_info.column_name = "SUM"; break;
                            case AggregateAccumulator::AggFunc::AVG: col_info.column_name = "AVG"; break;
                            case AggregateAccumulator::AggFunc::MIN: col_info.column_name = "MIN"; break;
                            case AggregateAccumulator::AggFunc::MAX: col_info.column_name = "MAX"; break;
                            case AggregateAccumulator::AggFunc::ARRAY_AGG:
                                col_info.column_name = "ARRAY_AGG";
                                col_info.data_type = static_cast<uint16_t>(core::DataType::JSON);
                                break;
                            case AggregateAccumulator::AggFunc::STDDEV_SAMP: col_info.column_name = "STDDEV_SAMP"; break;
                            case AggregateAccumulator::AggFunc::STDDEV_POP: col_info.column_name = "STDDEV_POP"; break;
                            case AggregateAccumulator::AggFunc::VAR_SAMP: col_info.column_name = "VAR_SAMP"; break;
                            case AggregateAccumulator::AggFunc::VAR_POP: col_info.column_name = "VAR_POP"; break;
                            case AggregateAccumulator::AggFunc::CORR: col_info.column_name = "CORR"; break;
                            case AggregateAccumulator::AggFunc::COVAR_POP: col_info.column_name = "COVAR_POP"; break;
                        }
                        result_columns.push_back(col_info);
                    }

                    // Evaluate HAVING expression
                    size_t saved_pc = pc_;
                    pc_ = having_start_pc;
                    current_row_values_ = &result_row;
                    current_row_columns_ = &result_columns;

                    try
                    {
                        evaluateExpression();
                        Value having_result = pop();
                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;
                        pc_ = saved_pc;

                        if (!having_result.toBoolean())
                        {
                            continue; // Skip this group
                        }
                    }
                    catch (...)
                    {
                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;
                        pc_ = saved_pc;
                        throw;
                    }
                }

                current_result_set_->addRow(std::move(result_row));
            }

            // Check for WINDOW functions after aggregation (Phase 1 Task 6.5)
            if (pc_ < bytecode_size_ && bytecode_[pc_] == static_cast<uint8_t>(Opcode::WINDOW))
            {
                // Move result set and evaluate window functions
                executeWindow(std::move(current_result_set_));
                return; // executeWindow handles ORDER BY and LIMIT/OFFSET detection
            }

            // Check for ORDER BY after aggregation
            if (pc_ < bytecode_size_ && bytecode_[pc_] == static_cast<uint8_t>(Opcode::ORDER_BY))
            {
                // Move result set and sort it
                executeSort(std::move(current_result_set_));
                return; // executeSort handles LIMIT/OFFSET detection
            }

            // Check for LIMIT/OFFSET (if no ORDER BY)
            if (pc_ < bytecode_size_ &&
                (bytecode_[pc_] == static_cast<uint8_t>(Opcode::LIMIT) ||
                 bytecode_[pc_] == static_cast<uint8_t>(Opcode::OFFSET)))
            {
                // Move result set and apply limit/offset
                executeLimit(std::move(current_result_set_));
            }
        }

        void Executor::executeSort(std::unique_ptr<ResultSet> input_result_set)
        {
            // Phase 1 Task 1.6.4: Sorting execution

            // Parse ORDER BY opcode
            if (readByte() != static_cast<uint8_t>(Opcode::ORDER_BY))
            {
                error("Expected ORDER_BY opcode for sorting");
            }

            uint32_t sort_key_count = readInt32();

            // Parse sort key definitions
            struct SortKeyDef
            {
                size_t column_index;  // Index in result set
                bool ascending;       // ASC vs DESC
                bool nulls_first;     // NULLS FIRST vs NULLS LAST
                bool nulls_specified; // Whether NULLS ordering was specified
            };
            std::vector<SortKeyDef> sort_keys;

            for (uint32_t i = 0; i < sort_key_count; i++)
            {
                // Read SORT_KEY marker
                if (readByte() != static_cast<uint8_t>(Opcode::SORT_KEY))
                {
                    error("Expected SORT_KEY marker");
                }

                // Read sort expression (should be COLUMN_REF for result set column)
                Opcode expr_op = static_cast<Opcode>(readByte());
                if (expr_op != Opcode::COLUMN_REF)
                {
                    error("Only column references supported in ORDER BY for now");
                }

                std::string col_name = readString();

                // Find column index in result set
                size_t col_idx = 0;
                bool found = false;
                for (size_t j = 0; j < input_result_set->columnCount(); j++)
                {
                    if (input_result_set->columnName(j) == col_name)
                    {
                        col_idx = j;
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    error("Sort column not found in result set: " + col_name);
                }

                // Read sort direction
                Opcode dir_op = static_cast<Opcode>(readByte());
                bool ascending = (dir_op == Opcode::SORT_ASC);

                // Check for NULLS ordering
                bool nulls_first = false;
                bool nulls_specified = false;

                if (pc_ < bytecode_size_)
                {
                    Opcode next_op = static_cast<Opcode>(bytecode_[pc_]);
                    if (next_op == Opcode::NULLS_FIRST)
                    {
                        readByte(); // Consume
                        nulls_first = true;
                        nulls_specified = true;
                    }
                    else if (next_op == Opcode::NULLS_LAST)
                    {
                        readByte(); // Consume
                        nulls_first = false;
                        nulls_specified = true;
                    }
                }

                sort_keys.push_back({col_idx, ascending, nulls_first, nulls_specified});
            }

            // Collect all rows from input result set into a vector
            std::vector<std::vector<Value>> rows;
            for (size_t i = 0; i < input_result_set->rowCount(); i++)
            {
                std::vector<Value> row;
                for (size_t j = 0; j < input_result_set->columnCount(); j++)
                {
                    row.push_back(input_result_set->getValue(i, j));
                }
                rows.push_back(std::move(row));
            }

            // Define comparison function for multi-key sorting
            auto compare_rows = [&](const std::vector<Value>& row1, const std::vector<Value>& row2) -> bool
            {
                for (const auto& key : sort_keys)
                {
                    const Value& val1 = row1[key.column_index];
                    const Value& val2 = row2[key.column_index];

                    // Handle NULL values
                    bool val1_is_null = val1.isNull();
                    bool val2_is_null = val2.isNull();

                    if (val1_is_null && val2_is_null)
                    {
                        continue; // Both NULL, equal for this key
                    }

                    if (val1_is_null || val2_is_null)
                    {
                        // Determine NULL ordering
                        bool nulls_first_for_key;
                        if (key.nulls_specified)
                        {
                            nulls_first_for_key = key.nulls_first;
                        }
                        else
                        {
                            // Default: NULLS LAST for ASC, NULLS FIRST for DESC
                            nulls_first_for_key = !key.ascending;
                        }

                        if (val1_is_null)
                        {
                            return nulls_first_for_key;
                        }
                        else
                        {
                            return !nulls_first_for_key;
                        }
                    }

                    // Both values are non-NULL, compare them
                    int cmp = 0;

                    // Compare based on type
                    core::DataType type1 = val1.type();
                    core::DataType type2 = val2.type();

                    if (type1 == core::DataType::VARCHAR || type2 == core::DataType::VARCHAR)
                    {
                        // String comparison (use collation-aware comparison)
                        std::string str1 = val1.toString();
                        std::string str2 = val2.toString();
                        cmp = compareStrings(str1, str2);
                    }
                    else if (type1 == core::DataType::BOOLEAN || type2 == core::DataType::BOOLEAN)
                    {
                        // Boolean comparison
                        bool b1 = val1.toBoolean();
                        bool b2 = val2.toBoolean();
                        if (b1 < b2) cmp = -1;
                        else if (b1 > b2) cmp = 1;
                        else cmp = 0;
                    }
                    else
                    {
                        // Numeric comparison (convert to double)
                        double d1 = val1.toDouble();
                        double d2 = val2.toDouble();
                        if (d1 < d2) cmp = -1;
                        else if (d1 > d2) cmp = 1;
                        else cmp = 0;
                    }

                    if (cmp != 0)
                    {
                        // Apply sort direction
                        if (key.ascending)
                        {
                            return cmp < 0;
                        }
                        else
                        {
                            return cmp > 0;
                        }
                    }

                    // Values are equal for this key, continue to next key
                }

                // All keys are equal
                return false;
            };

            // Sort rows using std::sort with custom comparator
            std::sort(rows.begin(), rows.end(), compare_rows);

            // Build sorted result set
            current_result_set_ = std::make_unique<ResultSet>();

            // Copy column definitions from input
            for (size_t i = 0; i < input_result_set->columnCount(); i++)
            {
                current_result_set_->addColumn(
                    input_result_set->columnName(i),
                    input_result_set->columnType(i)
                );
            }

            // Add sorted rows
            for (auto& row : rows)
            {
                current_result_set_->addRow(std::move(row));
            }

            // Check for LIMIT/OFFSET after sorting
            if (pc_ < bytecode_size_ &&
                (bytecode_[pc_] == static_cast<uint8_t>(Opcode::LIMIT) ||
                 bytecode_[pc_] == static_cast<uint8_t>(Opcode::OFFSET)))
            {
                // Move result set and apply limit/offset
                executeLimit(std::move(current_result_set_));
            }
        }

        void Executor::executeLimit(std::unique_ptr<ResultSet> input_result_set)
        {
            // Phase 1 Task 1.6.5: LIMIT/OFFSET execution

            // Parse LIMIT and OFFSET opcodes
            int64_t limit_count = -1;  // -1 means no limit
            int64_t offset_count = 0;  // Default: no offset

            // Check for LIMIT opcode
            if (pc_ < bytecode_size_ && bytecode_[pc_] == static_cast<uint8_t>(Opcode::LIMIT))
            {
                readByte(); // Consume LIMIT opcode
                limit_count = static_cast<int64_t>(readInt64());
            }

            // Check for OFFSET opcode
            if (pc_ < bytecode_size_ && bytecode_[pc_] == static_cast<uint8_t>(Opcode::OFFSET))
            {
                readByte(); // Consume OFFSET opcode
                offset_count = static_cast<int64_t>(readInt64());
            }

            // Build limited result set
            current_result_set_ = std::make_unique<ResultSet>();

            // Copy column definitions from input
            for (size_t i = 0; i < input_result_set->columnCount(); i++)
            {
                current_result_set_->addColumn(
                    input_result_set->columnName(i),
                    input_result_set->columnType(i)
                );
            }

            // Skip OFFSET rows and collect up to LIMIT rows
            size_t input_row_count = input_result_set->rowCount();
            size_t start_row = static_cast<size_t>(offset_count);
            size_t rows_collected = 0;

            for (size_t i = start_row; i < input_row_count; i++)
            {
                // Check if we've reached the limit
                if (limit_count >= 0 && rows_collected >= static_cast<size_t>(limit_count))
                {
                    break; // Early termination optimization
                }

                // Collect row
                std::vector<Value> row;
                for (size_t j = 0; j < input_result_set->columnCount(); j++)
                {
                    row.push_back(input_result_set->getValue(i, j));
                }
                current_result_set_->addRow(std::move(row));
                rows_collected++;
            }
        }

        // Phase 1, Task 6.5: Window function execution

        void Executor::executeWindow(std::unique_ptr<ResultSet> input_result_set)
        {
            // Parse WINDOW opcode
            if (readByte() != static_cast<uint8_t>(Opcode::WINDOW))
            {
                error("Expected WINDOW opcode");
            }

            uint32_t func_count = readInt32();
            std::vector<WindowFunctionSpec> window_specs;

            // Parse each window function specification
            for (uint32_t i = 0; i < func_count; i++)
            {
                WindowFunctionSpec spec;

                // Read function type
                Opcode func_op = static_cast<Opcode>(readByte());
                switch (func_op)
                {
                    case Opcode::WIN_ROW_NUMBER:
                        spec.func_type = WindowFunctionSpec::FuncType::ROW_NUMBER;
                        break;
                    case Opcode::WIN_RANK:
                        spec.func_type = WindowFunctionSpec::FuncType::RANK;
                        break;
                    case Opcode::WIN_DENSE_RANK:
                        spec.func_type = WindowFunctionSpec::FuncType::DENSE_RANK;
                        break;
                    case Opcode::WIN_LAG:
                        spec.func_type = WindowFunctionSpec::FuncType::LAG;
                        break;
                    case Opcode::WIN_LEAD:
                        spec.func_type = WindowFunctionSpec::FuncType::LEAD;
                        break;
                    case Opcode::WIN_FIRST_VALUE:
                        spec.func_type = WindowFunctionSpec::FuncType::FIRST_VALUE;
                        break;
                    case Opcode::WIN_LAST_VALUE:
                        spec.func_type = WindowFunctionSpec::FuncType::LAST_VALUE;
                        break;
                    case Opcode::WIN_NTH_VALUE:
                        spec.func_type = WindowFunctionSpec::FuncType::NTH_VALUE;
                        break;
                    default:
                        error("Unknown window function opcode");
                }

                // Read function arguments (these would need to be evaluated per row)
                // For now, we'll skip detailed parsing and just note the structure
                uint32_t arg_count = readInt32();
                for (uint32_t a = 0; a < arg_count; a++)
                {
                    // Skip argument expressions for now
                    // TODO: Parse and store argument expressions
                    error("Window function argument parsing not fully implemented");
                }

                // Parse window specification
                if (readByte() != static_cast<uint8_t>(Opcode::WINDOW_SPEC))
                {
                    error("Expected WINDOW_SPEC opcode");
                }

                // Read PARTITION BY count
                uint32_t partition_count = readInt32();
                if (partition_count > 0)
                {
                    if (readByte() != static_cast<uint8_t>(Opcode::PARTITION_BY))
                    {
                        error("Expected PARTITION_BY opcode");
                    }
                    // For now, assume column references
                    // TODO: Full expression support
                    for (uint32_t p = 0; p < partition_count; p++)
                    {
                        spec.partition_cols.push_back(p); // Placeholder
                    }
                }

                // Read ORDER BY count
                uint32_t order_count = readInt32();
                if (order_count > 0)
                {
                    if (readByte() != static_cast<uint8_t>(Opcode::WINDOW_ORDER_BY))
                    {
                        error("Expected WINDOW_ORDER_BY opcode");
                    }
                    for (uint32_t o = 0; o < order_count; o++)
                    {
                        spec.order_cols.push_back(o); // Placeholder
                        spec.order_asc.push_back(true); // Placeholder
                    }
                }

                // Read frame clause
                uint32_t has_frame = readInt32();
                spec.has_frame = (has_frame != 0);
                if (spec.has_frame)
                {
                    if (readByte() != static_cast<uint8_t>(Opcode::FRAME_CLAUSE))
                    {
                        error("Expected FRAME_CLAUSE opcode");
                    }

                    // Read frame mode
                    Opcode frame_mode_op = static_cast<Opcode>(readByte());
                    spec.frame_is_rows = (frame_mode_op == Opcode::FRAME_ROWS);

                    // Read frame boundaries (simplified for now)
                    readByte(); // Frame start boundary type
                    readByte(); // Frame end boundary type
                }

                // Read output column name
                spec.output_column = readString();

                window_specs.push_back(spec);
            }

            // Create output result set with window function columns
            current_result_set_ = std::make_unique<ResultSet>();

            // Copy input columns
            for (size_t i = 0; i < input_result_set->columnCount(); i++)
            {
                current_result_set_->addColumn(
                    input_result_set->columnName(i),
                    input_result_set->columnType(i)
                );
            }

            // Add window function output columns
            for (const auto& spec : window_specs)
            {
                current_result_set_->addColumn(spec.output_column, core::DataType::INT64);
            }

            // For each row in input, compute window functions
            // This is a simplified implementation - full implementation would:
            // 1. Partition rows by PARTITION BY columns
            // 2. Sort partitions by ORDER BY columns
            // 3. Compute window functions with frame windows

            for (size_t row_idx = 0; row_idx < input_result_set->rowCount(); row_idx++)
            {
                std::vector<Value> output_row;

                // Copy input columns
                for (size_t col = 0; col < input_result_set->columnCount(); col++)
                {
                    output_row.push_back(input_result_set->getValue(row_idx, col));
                }

                // Compute window functions (simplified - just ROW_NUMBER for now)
                for (const auto& spec : window_specs)
                {
                    Value result;

                    if (spec.func_type == WindowFunctionSpec::FuncType::ROW_NUMBER)
                    {
                        // ROW_NUMBER() is 1-indexed
                        result = core::TypedValue::makeInt64(static_cast<int64_t>(row_idx + 1));
                    }
                    else
                    {
                        // Placeholder for other window functions
                        result = core::TypedValue::makeInt64(0);
                    }

                    output_row.push_back(result);
                }

                current_result_set_->addRow(std::move(output_row));
            }

            // Check for ORDER BY after window functions
            if (pc_ < bytecode_size_ && bytecode_[pc_] == static_cast<uint8_t>(Opcode::ORDER_BY))
            {
                executeSort(std::move(current_result_set_));
                return;
            }

            // Check for LIMIT/OFFSET
            if (pc_ < bytecode_size_ &&
                (bytecode_[pc_] == static_cast<uint8_t>(Opcode::LIMIT) ||
                 bytecode_[pc_] == static_cast<uint8_t>(Opcode::OFFSET)))
            {
                executeLimit(std::move(current_result_set_));
            }
        }

        // ALPHA Phase 1 - Views: Execute a view's underlying SELECT query
        void Executor::executeViewQuery(const core::CatalogManager::ViewInfo& view_info,
                                       const std::vector<std::pair<std::string, std::string>>& select_items,
                                       bool is_select_star)
        {
            // Parse the view's SELECT query definition
            parser::Lexer lexer(view_info.definition);
            parser::ASTArena arena;
            parser::Parser parser(lexer, arena);

            auto parse_result = parser.parseStatement();
            if (!parse_result.success())
            {
                error("Failed to parse view definition for view: " + view_info.name);
            }

            auto* select_stmt = dynamic_cast<parser::SelectStmt*>(parse_result.statement());
            if (!select_stmt)
            {
                error("View definition is not a SELECT statement: " + view_info.name);
            }

            // Generate bytecode for the view's SELECT query
            BytecodeGenerator gen(lexer.stringPool(), db_);
            auto bytecode_result = gen.generate(select_stmt);
            if (!bytecode_result.success())
            {
                std::string error_msg = "Failed to generate bytecode for view: " + view_info.name;
                if (!bytecode_result.errors().empty())
                {
                    error_msg += " - " + bytecode_result.errors()[0];
                }
                error(error_msg);
            }

            // Execute the view's bytecode
            Executor view_executor(db_);
            view_executor.setConnectionContext(conn_ctx_);  // Preserve security context
            auto exec_result = view_executor.execute(bytecode_result.bytecode());

            if (!exec_result.success())
            {
                error("Failed to execute view query: " + view_info.name + " - " + exec_result.error());
            }

            // Get the result set from the view execution
            auto* view_result_set = exec_result.resultSet();
            if (!view_result_set)
            {
                error("View query did not return a result set: " + view_info.name);
            }

            // Copy the result set from the view execution to the current result set
            // Apply column projection if specific columns were requested
            current_result_set_ = std::make_unique<ResultSet>();

            if (is_select_star)
            {
                // SELECT * FROM view - return all columns
                for (size_t i = 0; i < view_result_set->columnCount(); i++)
                {
                    current_result_set_->addColumn(view_result_set->columnName(i),
                                                   view_result_set->columnType(i));
                }

                // Copy all rows with all columns
                for (size_t row_idx = 0; row_idx < view_result_set->rowCount(); row_idx++)
                {
                    std::vector<Value> row;
                    for (size_t col_idx = 0; col_idx < view_result_set->columnCount(); col_idx++)
                    {
                        row.push_back(view_result_set->getValue(row_idx, col_idx));
                    }
                    current_result_set_->addRow(std::move(row));
                }
            }
            else
            {
                // SELECT col1, col2, ... FROM view - project specific columns
                // Build a mapping of requested column names to view column indices
                std::vector<size_t> column_indices;
                std::vector<std::string> column_aliases;

                for (const auto& [col_name, alias] : select_items)
                {
                    // Find this column in the view's result set
                    bool found = false;
                    for (size_t i = 0; i < view_result_set->columnCount(); i++)
                    {
                        if (view_result_set->columnName(i) == col_name)
                        {
                            column_indices.push_back(i);
                            column_aliases.push_back(alias.empty() ? col_name : alias);
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        error("Column not found in view " + view_info.name + ": " + col_name);
                    }
                }

                // Add projected columns to result set
                for (size_t i = 0; i < column_indices.size(); i++)
                {
                    size_t view_col_idx = column_indices[i];
                    current_result_set_->addColumn(column_aliases[i],
                                                   view_result_set->columnType(view_col_idx));
                }

                // Copy rows with only the projected columns
                for (size_t row_idx = 0; row_idx < view_result_set->rowCount(); row_idx++)
                {
                    std::vector<Value> row;
                    for (size_t proj_idx : column_indices)
                    {
                        row.push_back(view_result_set->getValue(row_idx, proj_idx));
                    }
                    current_result_set_->addRow(std::move(row));
                }
            }
        }

        void Executor::executeSelect()
        {
            // SECURITY ENHANCEMENT (MEDIUM-3): Check query limits before expensive SELECT operation
            checkQueryLimits();

            // Read select list
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for select items");
            }

            uint32_t select_count = readInt32();
            bool is_select_star = false;
            std::vector<std::pair<std::string, std::string>> select_items; // (column_name, alias)

            for (uint32_t i = 0; i < select_count; i++)
            {
                Opcode op = static_cast<Opcode>(readByte());

                if (op == Opcode::SELECT_STAR)
                {
                    is_select_star = true;
                }
                else
                {
                    // For now, we only support simple column references
                    // Full expression evaluation would require row context
                    if (op != Opcode::COLUMN_REF)
                    {
                        error("Complex expressions in SELECT not yet supported");
                    }

                    std::string col_name = readString();
                    std::string alias;

                    // Check for optional alias
                    if (pc_ < bytecode_size_ &&
                        bytecode_[pc_] == static_cast<uint8_t>(Opcode::COLUMN_REF))
                    {
                        readByte(); // Consume COLUMN_REF
                        alias = readString();
                    }
                    else
                    {
                        alias = col_name; // Use column name as default
                    }

                    select_items.push_back({col_name, alias});
                }
            }

            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST after select items");
            }

            // Read table reference
            if (readByte() != static_cast<uint8_t>(Opcode::TABLE_REF))
            {
                error("Expected TABLE_REF");
            }

            std::string table_name = readString();

            // Check if this is a monitoring/system table (MON_ prefix)
            // Note: Using MON_ instead of MON$ because $ is not supported in identifiers yet
            if (table_name.size() >= 4 && table_name.substr(0, 4) == "MON_")
            {
                executeMonitoringQuery(table_name);
                return;
            }

            // Get default schema and table info
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get default schema");
            }

            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info,
                                                      nullptr);
            if (status != core::Status::OK)
            {
                // ALPHA Phase 1 - Views: Check if this is a view instead of a table
                core::CatalogManager::ViewInfo view_info;
                core::ErrorContext view_ctx;
                auto view_status = db_->catalog_manager()->getView(schema_info.schema_id, table_name, view_info, &view_ctx);

                if (view_status == core::Status::OK)
                {
                    // This is a view - execute the view's SELECT query
                    executeViewQuery(view_info, select_items, is_select_star);
                    return;
                }

                error("Table or view not found: " + table_name);
            }

            // Check SELECT permission on table
            bool has_table_select = checkPermission(table_info.table_id,
                               core::CatalogManager::PermissionObjectType::TABLE,
                               static_cast<uint32_t>(core::CatalogManager::Privilege::SELECT));

            // Security Phase 3.3.5: Get accessible columns if no table-level permission
            std::vector<std::string> accessible_columns;
            if (!has_table_select)
            {
                // Check column-level permissions
                core::ErrorContext err_ctx;
                const auto& user_id = getCurrentUserID();
                status = db_->catalog_manager()->getAccessibleColumns(
                    user_id, table_info.table_id,
                    core::CatalogManager::Privilege::SELECT,
                    accessible_columns, &err_ctx);

                if (status != core::Status::OK || accessible_columns.empty())
                {
                    // No table-level and no column-level permissions
                    error("Permission denied: SELECT on table " + table_name);
                }
                // If accessible_columns is non-empty, user has some column permissions
            }
            // If has_table_select is true, accessible_columns remains empty = all columns accessible

            // Get column information
            std::vector<core::CatalogManager::ColumnInfo> all_columns;
            status = db_->catalog_manager()->getColumns(table_info.table_id, all_columns, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get table columns");
            }

            // Build result set structure
            current_result_set_ = std::make_unique<ResultSet>();

            if (is_select_star)
            {
                // SELECT * - add all accessible columns
                // Security Phase 3.3.5: Filter by column-level permissions
                for (const auto &col : all_columns)
                {
                    // If accessible_columns is empty, user has table-level SELECT (all columns accessible)
                    // If non-empty, check if this column is in the accessible list
                    if (accessible_columns.empty() ||
                        std::find(accessible_columns.begin(), accessible_columns.end(), col.column_name)
                            != accessible_columns.end())
                    {
                        current_result_set_->addColumn(col.column_name,
                                                       static_cast<core::DataType>(col.data_type));
                    }
                }

                // Security Phase 3.3.5: If no columns were accessible, error
                if (current_result_set_->columnCount() == 0)
                {
                    error("Permission denied: No accessible columns in table " + table_name);
                }
            }
            else
            {
                // Add selected columns
                // Security Phase 3.3.5: Check permission for each requested column
                for (const auto &[col_name, alias] : select_items)
                {
                    // Find column in table
                    auto it = std::find_if(all_columns.begin(), all_columns.end(),
                                           [&col_name](const auto &c)
                                           { return c.column_name == col_name; });

                    if (it == all_columns.end())
                    {
                        error("Column not found: " + col_name);
                    }

                    // Security Phase 3.3.5: Check if user has access to this column
                    if (!accessible_columns.empty() &&
                        std::find(accessible_columns.begin(), accessible_columns.end(), col_name)
                            == accessible_columns.end())
                    {
                        error("Permission denied: SELECT on column " + col_name + " of table " + table_name);
                    }

                    current_result_set_->addColumn(alias,
                                                   static_cast<core::DataType>(it->data_type));
                }
            }

            // Check for WHERE clause and save bytecode position
            size_t where_start_pc = 0;
            size_t where_end_pc = 0;
            bool has_where = false;

            if (pc_ < bytecode_size_ &&
                bytecode_[pc_] == static_cast<uint8_t>(Opcode::WHERE_CLAUSE))
            {
                has_where = true;
                readByte(); // Consume WHERE_CLAUSE opcode
                where_start_pc = pc_;

                // Skip over WHERE expression to find end
                // We'll re-parse it for each row
                int depth = 1; // Track expression nesting
                while (pc_ < bytecode_size_ && depth > 0)
                {
                    Opcode op = static_cast<Opcode>(readByte());

                    // Literals push one value
                    if (op == Opcode::LITERAL_INT32)
                    {
                        pc_ += 4;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_INT64)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_DOUBLE)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_STRING || op == Opcode::COLUMN_REF)
                    {
                        uint32_t len = readInt32();
                        pc_ += len;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_NULL)
                    {
                        depth++;
                    }
                    // Binary operators consume 2, produce 1
                    else if (op >= Opcode::EXPR_ADD && op <= Opcode::EXPR_OR)
                    {
                        depth--; // Net effect: consume 2, produce 1 = -1
                    }
                    // CAST consumes 1, produces 1, plus reads try_cast flag and type
                    else if (op == Opcode::EXPR_CAST)
                    {
                        // Skip try_cast flag (1 byte)
                        readByte();
                        // Read and skip type opcode
                        Opcode type_op = static_cast<Opcode>(readByte());
                        if (type_op == Opcode::TYPE_VARCHAR)
                        {
                            pc_ += 4; // Skip precision
                        }
                        // depth unchanged (consume 1, produce 1)
                    }
                }
                where_end_pc = pc_;
            }

            // Check for aggregation opcodes (GROUP BY or AGG_INIT)
            bool has_aggregation = false;
            size_t group_by_start_pc = 0;
            uint32_t group_by_count = 0;
            size_t agg_init_pc = 0;

            if (pc_ < bytecode_size_)
            {
                Opcode next_op = static_cast<Opcode>(bytecode_[pc_]);
                if (next_op == Opcode::GROUP_BY || next_op == Opcode::AGG_INIT)
                {
                    has_aggregation = true;
                }
            }

            // If we have aggregation, handle it differently
            if (has_aggregation)
            {
                executeAggregate(table_info, all_columns, select_items, is_select_star,
                                has_where, where_start_pc, where_end_pc);
                return;
            }

            // Non-aggregation path (existing code)
            // Create table scan iterator
            auto scan_iter = db_->storage_engine()->createScan(table_info.table_id, nullptr);
            if (!scan_iter)
            {
                error("Failed to create table scan iterator");
            }

            // Scan all tuples
            core::Tuple tuple;
            while (scan_iter->next(&tuple, nullptr) == core::Status::OK)
            {
                // Deserialize tuple data
                std::vector<Value> row_values;
                if (!deserializeTuple(tuple.data, tuple.data_size, all_columns, row_values))
                {
                    continue; // Skip malformed tuples
                }

                // Evaluate WHERE clause if present
                if (has_where)
                {
                    // Save current PC and evaluate WHERE expression
                    size_t saved_pc = pc_;
                    pc_ = where_start_pc;

                    // Set up row context for column references
                    current_row_values_ = &row_values;
                    current_row_columns_ = &all_columns;

                    try
                    {
                        evaluateExpression();
                        Value where_result = pop();

                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;

                        // Restore PC
                        pc_ = saved_pc;

                        // Check if WHERE clause evaluated to true
                        if (!where_result.toBoolean())
                        {
                            continue; // Skip this row
                        }
                    }
                    catch (...)
                    {
                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;
                        pc_ = saved_pc;
                        throw;
                    }
                }

                // Project selected columns
                std::vector<Value> result_row;

                if (is_select_star)
                {
                    result_row = row_values;
                }
                else
                {
                    for (const auto &[col_name, alias] : select_items)
                    {
                        // Find column index
                        auto it = std::find_if(all_columns.begin(), all_columns.end(),
                                               [&col_name](const auto &c)
                                               { return c.column_name == col_name; });

                        if (it != all_columns.end())
                        {
                            size_t col_idx = std::distance(all_columns.begin(), it);
                            result_row.push_back(row_values[col_idx]);
                        }
                    }
                }

                current_result_set_->addRow(std::move(result_row));
            }

            // Check for WINDOW functions after SELECT execution (Phase 1 Task 6.5)
            if (pc_ < bytecode_size_ && bytecode_[pc_] == static_cast<uint8_t>(Opcode::WINDOW))
            {
                // Move result set and evaluate window functions
                executeWindow(std::move(current_result_set_));
                return; // executeWindow handles ORDER BY and LIMIT/OFFSET detection
            }

            // Check for ORDER BY after SELECT execution
            if (pc_ < bytecode_size_ && bytecode_[pc_] == static_cast<uint8_t>(Opcode::ORDER_BY))
            {
                // Move result set and sort it
                executeSort(std::move(current_result_set_));
                return; // executeSort handles LIMIT/OFFSET detection
            }

            // Check for LIMIT/OFFSET (if no ORDER BY)
            if (pc_ < bytecode_size_ &&
                (bytecode_[pc_] == static_cast<uint8_t>(Opcode::LIMIT) ||
                 bytecode_[pc_] == static_cast<uint8_t>(Opcode::OFFSET)))
            {
                // Move result set and apply limit/offset
                executeLimit(std::move(current_result_set_));
            }
        }

        // ===== Set Operations (UNION, INTERSECT, EXCEPT) =====

        void Executor::executeUnionAll()
        {
            // UNION ALL: Concatenate results from left and right queries (keep duplicates)
            // Bytecode format: EXT_UNION_ALL <left_query> <right_query> [ORDER BY] [LIMIT]

            // Execute left query (next opcode)
            Opcode left_op = static_cast<Opcode>(readByte());
            auto left_result = std::make_unique<ResultSet>();

            if (left_op == Opcode::SELECT)
            {
                executeSelect();
                left_result = std::move(current_result_set_);
            }
            else
            {
                error("UNION ALL: Left side must be a SELECT statement");
            }

            // Execute right query
            Opcode right_op = static_cast<Opcode>(readByte());
            auto right_result = std::make_unique<ResultSet>();

            if (right_op == Opcode::SELECT)
            {
                executeSelect();
                right_result = std::move(current_result_set_);
            }
            else
            {
                error("UNION ALL: Right side must be a SELECT statement");
            }

            // Verify schema compatibility
            if (left_result->columnCount() != right_result->columnCount())
            {
                error("UNION ALL: Column count mismatch between left and right queries");
            }

            // Create combined result set (use left schema)
            current_result_set_ = std::make_unique<ResultSet>();
            for (size_t i = 0; i < left_result->columnCount(); ++i)
            {
                current_result_set_->addColumn(
                    left_result->columnName(i),
                    left_result->columnType(i)
                );
            }

            // Add all rows from left
            for (size_t r = 0; r < left_result->rowCount(); ++r)
            {
                std::vector<Value> row;
                for (size_t c = 0; c < left_result->columnCount(); ++c)
                {
                    row.push_back(left_result->getValue(r, c));
                }
                current_result_set_->addRow(row);
            }

            // Add all rows from right
            for (size_t r = 0; r < right_result->rowCount(); ++r)
            {
                std::vector<Value> row;
                for (size_t c = 0; c < right_result->columnCount(); ++c)
                {
                    row.push_back(right_result->getValue(r, c));
                }
                current_result_set_->addRow(row);
            }
        }

        void Executor::executeUnion()
        {
            // UNION: Concatenate results and remove duplicates
            // First execute as UNION ALL, then deduplicate

            executeUnionAll(); // Get combined result

            // Deduplicate rows
            std::set<std::vector<std::string>> unique_rows; // Use string representation for comparison
            auto original_result = std::move(current_result_set_);
            current_result_set_ = std::make_unique<ResultSet>();

            // Copy schema
            for (size_t i = 0; i < original_result->columnCount(); ++i)
            {
                current_result_set_->addColumn(
                    original_result->columnName(i),
                    original_result->columnType(i)
                );
            }

            // Add only unique rows
            for (size_t r = 0; r < original_result->rowCount(); ++r)
            {
                std::vector<Value> row;
                std::vector<std::string> row_key;

                for (size_t c = 0; c < original_result->columnCount(); ++c)
                {
                    Value val = original_result->getValue(r, c);
                    row.push_back(val);
                    // Create a comparable key
                    if (val.isNull())
                    {
                        row_key.push_back("\0NULL\0");
                    }
                    else
                    {
                        row_key.push_back(val.toString());
                    }
                }

                // Only add if not seen before
                if (unique_rows.insert(row_key).second)
                {
                    current_result_set_->addRow(row);
                }
            }
        }

        void Executor::executeIntersectAll()
        {
            // INTERSECT ALL: Keep only rows that appear in both sides (with duplicates)
            // For each row in left, if it appears in right, include it (decrement right count)

            // Execute left query
            Opcode left_op = static_cast<Opcode>(readByte());
            auto left_result = std::make_unique<ResultSet>();

            if (left_op == Opcode::SELECT)
            {
                executeSelect();
                left_result = std::move(current_result_set_);
            }
            else
            {
                error("INTERSECT ALL: Left side must be a SELECT statement");
            }

            // Execute right query
            Opcode right_op = static_cast<Opcode>(readByte());
            auto right_result = std::make_unique<ResultSet>();

            if (right_op == Opcode::SELECT)
            {
                executeSelect();
                right_result = std::move(current_result_set_);
            }
            else
            {
                error("INTERSECT ALL: Right side must be a SELECT statement");
            }

            // Verify schema compatibility
            if (left_result->columnCount() != right_result->columnCount())
            {
                error("INTERSECT ALL: Column count mismatch");
            }

            // Build multiset (with counts) from right side
            std::map<std::vector<std::string>, size_t> right_counts;
            for (size_t r = 0; r < right_result->rowCount(); ++r)
            {
                std::vector<std::string> row_key;
                for (size_t c = 0; c < right_result->columnCount(); ++c)
                {
                    Value val = right_result->getValue(r, c);
                    row_key.push_back(val.isNull() ? "\0NULL\0" : val.toString());
                }
                right_counts[row_key]++;
            }

            // Create result set
            current_result_set_ = std::make_unique<ResultSet>();
            for (size_t i = 0; i < left_result->columnCount(); ++i)
            {
                current_result_set_->addColumn(
                    left_result->columnName(i),
                    left_result->columnType(i)
                );
            }

            // For each row in left, check if it's in right
            for (size_t r = 0; r < left_result->rowCount(); ++r)
            {
                std::vector<Value> row;
                std::vector<std::string> row_key;

                for (size_t c = 0; c < left_result->columnCount(); ++c)
                {
                    Value val = left_result->getValue(r, c);
                    row.push_back(val);
                    row_key.push_back(val.isNull() ? "\0NULL\0" : val.toString());
                }

                // Check if this row exists in right (with count > 0)
                auto it = right_counts.find(row_key);
                if (it != right_counts.end() && it->second > 0)
                {
                    current_result_set_->addRow(row);
                    it->second--; // Decrement count for ALL semantics
                }
            }
        }

        void Executor::executeIntersect()
        {
            // INTERSECT: Keep only rows that appear in both sides (without duplicates)
            executeIntersectAll(); // Get intersection with duplicates

            // Deduplicate
            std::set<std::vector<std::string>> unique_rows;
            auto original_result = std::move(current_result_set_);
            current_result_set_ = std::make_unique<ResultSet>();

            // Copy schema
            for (size_t i = 0; i < original_result->columnCount(); ++i)
            {
                current_result_set_->addColumn(
                    original_result->columnName(i),
                    original_result->columnType(i)
                );
            }

            // Add only unique rows
            for (size_t r = 0; r < original_result->rowCount(); ++r)
            {
                std::vector<Value> row;
                std::vector<std::string> row_key;

                for (size_t c = 0; c < original_result->columnCount(); ++c)
                {
                    Value val = original_result->getValue(r, c);
                    row.push_back(val);
                    row_key.push_back(val.isNull() ? "\0NULL\0" : val.toString());
                }

                if (unique_rows.insert(row_key).second)
                {
                    current_result_set_->addRow(row);
                }
            }
        }

        void Executor::executeExceptAll()
        {
            // EXCEPT ALL: Rows in left but not in right (with duplicates)
            // For each row in left, if count in left > count in right, include (count_left - count_right) times

            // Execute left query
            Opcode left_op = static_cast<Opcode>(readByte());
            auto left_result = std::make_unique<ResultSet>();

            if (left_op == Opcode::SELECT)
            {
                executeSelect();
                left_result = std::move(current_result_set_);
            }
            else
            {
                error("EXCEPT ALL: Left side must be a SELECT statement");
            }

            // Execute right query
            Opcode right_op = static_cast<Opcode>(readByte());
            auto right_result = std::make_unique<ResultSet>();

            if (right_op == Opcode::SELECT)
            {
                executeSelect();
                right_result = std::move(current_result_set_);
            }
            else
            {
                error("EXCEPT ALL: Right side must be a SELECT statement");
            }

            // Verify schema compatibility
            if (left_result->columnCount() != right_result->columnCount())
            {
                error("EXCEPT ALL: Column count mismatch");
            }

            // Build multiset from right side
            std::map<std::vector<std::string>, size_t> right_counts;
            for (size_t r = 0; r < right_result->rowCount(); ++r)
            {
                std::vector<std::string> row_key;
                for (size_t c = 0; c < right_result->columnCount(); ++c)
                {
                    Value val = right_result->getValue(r, c);
                    row_key.push_back(val.isNull() ? "\0NULL\0" : val.toString());
                }
                right_counts[row_key]++;
            }

            // Create result set
            current_result_set_ = std::make_unique<ResultSet>();
            for (size_t i = 0; i < left_result->columnCount(); ++i)
            {
                current_result_set_->addColumn(
                    left_result->columnName(i),
                    left_result->columnType(i)
                );
            }

            // For each row in left, include if not in right (or fewer times in right)
            for (size_t r = 0; r < left_result->rowCount(); ++r)
            {
                std::vector<Value> row;
                std::vector<std::string> row_key;

                for (size_t c = 0; c < left_result->columnCount(); ++c)
                {
                    Value val = left_result->getValue(r, c);
                    row.push_back(val);
                    row_key.push_back(val.isNull() ? "\0NULL\0" : val.toString());
                }

                // Check count in right
                auto it = right_counts.find(row_key);
                if (it == right_counts.end() || it->second == 0)
                {
                    // Not in right, or already used up all occurrences
                    current_result_set_->addRow(row);
                }
                else
                {
                    // In right, decrement count
                    it->second--;
                }
            }
        }

        void Executor::executeExcept()
        {
            // EXCEPT: Rows in left but not in right (without duplicates)
            executeExceptAll(); // Get difference with duplicates

            // Deduplicate
            std::set<std::vector<std::string>> unique_rows;
            auto original_result = std::move(current_result_set_);
            current_result_set_ = std::make_unique<ResultSet>();

            // Copy schema
            for (size_t i = 0; i < original_result->columnCount(); ++i)
            {
                current_result_set_->addColumn(
                    original_result->columnName(i),
                    original_result->columnType(i)
                );
            }

            // Add only unique rows
            for (size_t r = 0; r < original_result->rowCount(); ++r)
            {
                std::vector<Value> row;
                std::vector<std::string> row_key;

                for (size_t c = 0; c < original_result->columnCount(); ++c)
                {
                    Value val = original_result->getValue(r, c);
                    row.push_back(val);
                    row_key.push_back(val.isNull() ? "\0NULL\0" : val.toString());
                }

                if (unique_rows.insert(row_key).second)
                {
                    current_result_set_->addRow(row);
                }
            }
        }

        void Executor::executeRecursiveCTE(const std::string& cte_name, size_t base_pc)
        {
            // Recursive CTE execution with UNION ALL
            // Format: UNION ALL <base_case_SELECT> <recursive_term_SELECT>

            // Check recursion depth limit
            if (cte_recursion_depth_ >= query_limits_.max_cte_recursion_depth)
            {
                error("CTE recursion depth limit exceeded (" +
                      std::to_string(query_limits_.max_cte_recursion_depth) + ")");
            }

            incrementCTEDepth();

            // STEP 1: Execute base case (left side of UNION ALL)
            Opcode base_op = static_cast<Opcode>(readByte());
            if (base_op != Opcode::SELECT)
            {
                error("Recursive CTE base case must be a SELECT statement");
            }

            executeSelect();

            // Store base case results in CTE
            std::vector<std::vector<Value>> all_rows;
            std::vector<std::string> col_names;
            std::vector<core::DataType> col_types;

            if (current_result_set_)
            {
                // Extract column metadata
                for (size_t i = 0; i < current_result_set_->columnCount(); ++i)
                {
                    col_names.push_back(current_result_set_->columnName(i));
                    col_types.push_back(current_result_set_->columnType(i));
                }

                // Extract all rows from base case
                for (size_t r = 0; r < current_result_set_->rowCount(); ++r)
                {
                    std::vector<Value> row;
                    for (size_t c = 0; c < current_result_set_->columnCount(); ++c)
                    {
                        row.push_back(current_result_set_->getValue(r, c));
                    }
                    all_rows.push_back(std::move(row));
                }
            }

            // Initialize CTE with base case results
            cte_results_[cte_name] = all_rows;
            cte_column_names_[cte_name] = col_names;
            cte_column_types_[cte_name] = col_types;

            // Track working table (new rows from each iteration)
            std::vector<std::vector<Value>> working_table = all_rows;

            // STEP 2: Iteratively execute recursive term
            size_t iteration = 0;
            const size_t max_iterations = 10000;  // Safety limit

            // Save the position of recursive term SELECT
            size_t recursive_term_pc = pc_;

            while (!working_table.empty() && iteration < max_iterations)
            {
                iteration++;

                // Reset PC to recursive term
                pc_ = recursive_term_pc;

                // Execute recursive term
                Opcode recursive_op = static_cast<Opcode>(readByte());
                if (recursive_op != Opcode::SELECT)
                {
                    error("Recursive CTE recursive term must be a SELECT statement");
                }

                executeSelect();

                // Collect new rows
                std::vector<std::vector<Value>> new_rows;
                std::set<std::vector<std::string>> existing_row_keys;

                // Build set of existing rows for cycle detection
                for (const auto& row : all_rows)
                {
                    std::vector<std::string> row_key;
                    for (const auto& val : row)
                    {
                        row_key.push_back(val.isNull() ? "\0NULL\0" : val.toString());
                    }
                    existing_row_keys.insert(row_key);
                }

                // Check each result row
                if (current_result_set_)
                {
                    for (size_t r = 0; r < current_result_set_->rowCount(); ++r)
                    {
                        std::vector<Value> row;
                        std::vector<std::string> row_key;

                        for (size_t c = 0; c < current_result_set_->columnCount(); ++c)
                        {
                            Value val = current_result_set_->getValue(r, c);
                            row.push_back(val);
                            row_key.push_back(val.isNull() ? "\0NULL\0" : val.toString());
                        }

                        // Only add if not already seen (cycle detection)
                        if (existing_row_keys.find(row_key) == existing_row_keys.end())
                        {
                            new_rows.push_back(std::move(row));
                            existing_row_keys.insert(row_key);
                        }
                    }
                }

                // If no new rows, we're done
                if (new_rows.empty())
                {
                    break;
                }

                // Add new rows to CTE and working table
                for (const auto& row : new_rows)
                {
                    all_rows.push_back(row);
                }

                working_table = std::move(new_rows);

                // Update CTE with accumulated results
                cte_results_[cte_name] = all_rows;

                // Check query limits
                checkQueryLimits();
            }

            if (iteration >= max_iterations)
            {
                error("Recursive CTE exceeded maximum iterations (" + std::to_string(max_iterations) + ")");
            }

            decrementCTEDepth();

            // Final CTE results are in cte_results_[cte_name]
            // (already updated in the loop)
        }

        void Executor::executeSweep()
        {
            // Execute SWEEP DATABASE command (Phase 3 Task 3.3)
            // This triggers a manual foreground sweep

            // Get sweep manager
            auto sweep_mgr = db_->sweep_manager();
            if (!sweep_mgr)
            {
                error("Sweep manager not available");
            }

            // Execute foreground sweep
            core::ErrorContext err_ctx;
            auto status = sweep_mgr->executeSweep(true, &err_ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Sweep failed";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
            }

            // Success - sweep completed
        }

        void Executor::executeStartTransaction()
        {
            // Execute START TRANSACTION statement (Phase 2 Task 2.6, Phase 3 Task 3.6)

            // Read transaction mode (1 byte: 0 = READ_WRITE, 1 = READ_ONLY)
            uint8_t mode_byte = readByte();
            bool read_only = (mode_byte == 1);

            // Read isolation level (1 byte: 0 = READ_COMMITTED, 1 = SNAPSHOT, 2 =
            // SNAPSHOT_TABLE_STABILITY)
            uint8_t isolation_byte = readByte();
            core::IsolationLevel isolation;
            switch (isolation_byte)
            {
                case 0:
                    isolation = core::IsolationLevel::READ_COMMITTED;
                    break;
                case 1:
                    isolation = core::IsolationLevel::SNAPSHOT;
                    break;
                case 2:
                    isolation = core::IsolationLevel::SNAPSHOT_TABLE_STABILITY;
                    break;
                default:
                    error("Unknown isolation level: " + std::to_string(isolation_byte));
                    return;
            }

            // Read wait flag (1 byte: 0 = NO WAIT, 1 = WAIT)
            uint8_t wait_byte = readByte();
            bool wait = (wait_byte == 1);

            // Read commit outstanding flag (1 byte: 0 = false, 1 = true)
            uint8_t commit_outstanding_byte = readByte();
            bool commit_outstanding = (commit_outstanding_byte == 1);

            // Read lock timeout (uint32, 0 = no lock timeout)
            uint32_t lock_timeout = readInt32();

            // Read table reservations list (Phase 3 Task 3.6)
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for table reservations");
            }

            uint32_t reservation_count = readInt32();
            std::vector<core::ConnectionContext::TableReservation> reservations;

            for (uint32_t i = 0; i < reservation_count; i++)
            {
                // Read TABLE_REF opcode
                if (readByte() != static_cast<uint8_t>(Opcode::TABLE_REF))
                {
                    error("Expected TABLE_REF in table reservation");
                }

                std::string table_name = readString();

                // Read lock mode (1 byte: 0 = SHARED, 1 = PROTECTED)
                uint8_t lock_mode_byte = readByte();
                core::TableLockMode lock_mode = (lock_mode_byte == 0)
                                                    ? core::TableLockMode::SHARED
                                                    : core::TableLockMode::PROTECTED;

                // Read for_write flag (1 byte: 0 = FOR READ, 1 = FOR WRITE)
                uint8_t for_write_byte = readByte();
                bool for_write = (for_write_byte == 1);

                // Add to reservations list
                reservations.push_back({table_name, lock_mode, for_write});
            }

            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST after table reservations");
            }

            // Get connection context
            auto conn_ctx = core::ConnectionContext::getCurrent();
            if (!conn_ctx)
            {
                error("No connection context available");
            }

            // Apply transaction settings
            core::ErrorContext err_ctx;

            // Set wait and timeout settings
            conn_ctx->setWaitForLocks(wait);
            conn_ctx->setLockTimeout(lock_timeout);

            // Start new transaction (commits current if commit_outstanding = true)
            auto status =
                conn_ctx->startTransaction(read_only, isolation, commit_outstanding, &err_ctx);
            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to start transaction";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
            }

            // Apply table reservations if any
            if (!reservations.empty())
            {
                status = conn_ctx->reserveTables(reservations, &err_ctx);
                if (status != core::Status::OK)
                {
                    std::string err_msg = "Failed to reserve tables";
                    if (!err_ctx.message.empty())
                    {
                        err_msg += ": " + err_ctx.message;
                    }
                    error(err_msg);
                }
            }

            // Success - transaction started with new settings
        }

        void Executor::executeSetTransaction()
        {
            // Execute SET TRANSACTION statement (Phase 3 Task 3.6)
            // Similar to START TRANSACTION but without commit_outstanding flag

            // Read transaction mode (1 byte: 0 = READ_WRITE, 1 = READ_ONLY)
            uint8_t mode_byte = readByte();
            bool read_only = (mode_byte == 1);

            // Read isolation level (1 byte: 0 = READ_COMMITTED, 1 = SNAPSHOT, 2 =
            // SNAPSHOT_TABLE_STABILITY)
            uint8_t isolation_byte = readByte();
            core::IsolationLevel isolation;
            switch (isolation_byte)
            {
                case 0:
                    isolation = core::IsolationLevel::READ_COMMITTED;
                    break;
                case 1:
                    isolation = core::IsolationLevel::SNAPSHOT;
                    break;
                case 2:
                    isolation = core::IsolationLevel::SNAPSHOT_TABLE_STABILITY;
                    break;
                default:
                    error("Unknown isolation level: " + std::to_string(isolation_byte));
                    return;
            }

            // Read wait flag (1 byte: 0 = NO WAIT, 1 = WAIT)
            uint8_t wait_byte = readByte();
            bool wait = (wait_byte == 1);

            // Read lock timeout (uint32, 0 = no lock timeout)
            uint32_t lock_timeout = readInt32();

            // Read table reservations list (Phase 3 Task 3.6)
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for table reservations");
            }

            uint32_t reservation_count = readInt32();
            std::vector<core::ConnectionContext::TableReservation> reservations;

            for (uint32_t i = 0; i < reservation_count; i++)
            {
                // Read TABLE_REF opcode
                if (readByte() != static_cast<uint8_t>(Opcode::TABLE_REF))
                {
                    error("Expected TABLE_REF in table reservation");
                }

                std::string table_name = readString();

                // Read lock mode (1 byte: 0 = SHARED, 1 = PROTECTED)
                uint8_t lock_mode_byte = readByte();
                core::TableLockMode lock_mode = (lock_mode_byte == 0)
                                                    ? core::TableLockMode::SHARED
                                                    : core::TableLockMode::PROTECTED;

                // Read for_write flag (1 byte: 0 = FOR READ, 1 = FOR WRITE)
                uint8_t for_write_byte = readByte();
                bool for_write = (for_write_byte == 1);

                // Add to reservations list
                reservations.push_back({table_name, lock_mode, for_write});
            }

            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST after table reservations");
            }

            // Get connection context
            auto conn_ctx = core::ConnectionContext::getCurrent();
            if (!conn_ctx)
            {
                error("No connection context available");
            }

            // Apply transaction settings (staged for next transaction)
            core::ErrorContext err_ctx;

            // Set wait and timeout settings
            conn_ctx->setWaitForLocks(wait);
            conn_ctx->setLockTimeout(lock_timeout);

            // Start new transaction with commit_outstanding = false (stages settings)
            auto status = conn_ctx->startTransaction(read_only, isolation, false, &err_ctx);
            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to set transaction parameters";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
            }

            // Apply table reservations if any
            if (!reservations.empty())
            {
                status = conn_ctx->reserveTables(reservations, &err_ctx);
                if (status != core::Status::OK)
                {
                    std::string err_msg = "Failed to reserve tables";
                    if (!err_ctx.message.empty())
                    {
                        err_msg += ": " + err_ctx.message;
                    }
                    error(err_msg);
                }
            }

            // Success - transaction parameters set for next transaction
        }

        void Executor::executeCommit()
        {
            // Execute COMMIT statement (Phase 2 Task 2.6)

            // Get connection context
            auto conn_ctx = core::ConnectionContext::getCurrent();
            if (!conn_ctx)
            {
                error("No connection context available");
            }

            // Commit current transaction and start new one
            core::ErrorContext err_ctx;
            auto status = conn_ctx->commit(&err_ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Commit failed";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
            }

            // Success - transaction committed
        }

        void Executor::executeRollback()
        {
            // Execute ROLLBACK statement (Phase 2 Task 2.6)

            // Get connection context
            auto conn_ctx = core::ConnectionContext::getCurrent();
            if (!conn_ctx)
            {
                error("No connection context available");
            }

            // Rollback current transaction and start new one
            core::ErrorContext err_ctx;
            auto status = conn_ctx->rollback(&err_ctx);

            if (status != core::Status::OK)
            {
                std::string err_msg = "Rollback failed";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
            }

            // Success - transaction rolled back
        }

        void Executor::executeCreateTrigger()
        {
            // Wave 2: Trigger Executor Implementation
            // Read trigger definition from bytecode

            std::string trigger_name = readString();
            std::string table_name = readString();

            auto timing = static_cast<core::CatalogManager::TriggerTiming>(readByte());
            auto event = static_cast<core::CatalogManager::TriggerEvent>(readByte());
            auto granularity = static_cast<core::CatalogManager::TriggerGranularity>(readByte());

            std::string procedure_name = readString();

            // Get default schema (PUBLIC)
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get default schema");
            }

            // Get table from catalog
            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Table not found: " + table_name);
            }

            // Create TriggerInfo
            core::CatalogManager::TriggerInfo trigger_info;
            trigger_info.trigger_name = trigger_name;
            trigger_info.table_id = table_info.table_id;
            trigger_info.table_name = table_name;
            trigger_info.timing = timing;
            trigger_info.event = event;
            trigger_info.granularity = granularity;
            trigger_info.procedure_name = procedure_name;
            trigger_info.enabled = true;
            trigger_info.created_time = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            // Store in catalog
            core::ErrorContext err_ctx;
            status = db_->catalog_manager()->createTrigger(trigger_info, &err_ctx);
            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to create trigger";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
            }
        }

        void Executor::executeDropTrigger()
        {
            // Wave 2: Trigger Executor Implementation
            std::string trigger_name = readString();

            core::ErrorContext err_ctx;
            auto status = db_->catalog_manager()->dropTrigger(trigger_name, &err_ctx);
            if (status != core::Status::OK)
            {
                std::string err_msg = "Failed to drop trigger";
                if (!err_ctx.message.empty())
                {
                    err_msg += ": " + err_ctx.message;
                }
                error(err_msg);
            }
        }

        bool Executor::fireTrigger(const TriggerContext& ctx)
        {
            // Wave 2: Trigger Executor Implementation
            const auto& trigger = ctx.trigger();

            // Look up trigger procedure
            auto it = trigger_procedures_.find(trigger.procedure_name);
            if (it == trigger_procedures_.end())
            {
                // For Phase 2, just log warning if procedure not found
                std::cerr << "Warning: Trigger procedure '"
                          << trigger.procedure_name << "' not registered\n";
                return true;  // Continue operation
            }

            // Execute procedure
            try
            {
                bool should_continue = it->second(ctx);
                return should_continue;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Trigger '" << trigger.trigger_name
                          << "' failed: " << e.what() << "\n";

                // BEFORE triggers: failure prevents operation
                if (trigger.timing == core::CatalogManager::TriggerTiming::BEFORE)
                {
                    return false;
                }

                // AFTER triggers: log error but continue
                return true;
            }
        }

        void Executor::registerTriggerProcedure(
            const std::string& name,
            TriggerProcedure procedure)
        {
            trigger_procedures_[name] = std::move(procedure);
        }

        void Executor::executeMonitoringQuery(const std::string &table_name)
        {
            // Handle monitoring/system table queries (MON_ tables)
            current_result_set_ = std::make_unique<ResultSet>();

            if (table_name == "MON_DATABASE")
            {
                // Add columns for transaction markers
                current_result_set_->addColumn("MON$DATABASE_NAME", core::DataType::VARCHAR);
                current_result_set_->addColumn("MON$NEXT_TRANSACTION", core::DataType::INT64);
                current_result_set_->addColumn("MON$OLDEST_TRANSACTION", core::DataType::INT64);
                current_result_set_->addColumn("MON$OLDEST_ACTIVE", core::DataType::INT64);
                current_result_set_->addColumn("MON$OLDEST_SNAPSHOT", core::DataType::INT64);

                // Get transaction markers from transaction manager
                auto txn_mgr = db_->transaction_manager();
                uint64_t next_xid = txn_mgr->getCurrentXid();
                uint64_t oit = txn_mgr->getOldestXid();
                uint64_t oat = txn_mgr->getOldestActiveXid();
                uint64_t ost = txn_mgr->getOldestSnapshot();

                // Create the result row
                std::vector<Value> row;
                row.push_back(Value::makeVarchar("SCRATCHBIRD"));
                row.push_back(Value::makeInt64(static_cast<int64_t>(next_xid)));
                row.push_back(Value::makeInt64(static_cast<int64_t>(oit)));
                row.push_back(Value::makeInt64(static_cast<int64_t>(oat)));
                row.push_back(Value::makeInt64(static_cast<int64_t>(ost)));

                current_result_set_->addRow(std::move(row));
            }
            else if (table_name == "MON_SWEEP")
            {
                // Add columns for sweep statistics
                current_result_set_->addColumn("MON$SWEEP_COUNT", core::DataType::INT64);
                current_result_set_->addColumn("MON$LAST_SWEEP_TIME", core::DataType::INT64);
                current_result_set_->addColumn("MON$LAST_DURATION_MS", core::DataType::INT64);
                current_result_set_->addColumn("MON$OIT_BEFORE", core::DataType::INT64);
                current_result_set_->addColumn("MON$OIT_AFTER", core::DataType::INT64);
                current_result_set_->addColumn("MON$TOTAL_SWEPT", core::DataType::INT64);
                current_result_set_->addColumn("MON$IN_PROGRESS", core::DataType::BOOLEAN);

                // Get sweep statistics from sweep manager
                auto sweep_mgr = db_->sweep_manager();
                if (sweep_mgr)
                {
                    auto stats = sweep_mgr->getStatistics();

                    // Create the result row
                    std::vector<Value> row;
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.sweep_count)));
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.last_sweep_time)));
                    row.push_back(
                        Value::makeInt64(static_cast<int64_t>(stats.last_sweep_duration_ms)));
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.last_oit_before)));
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.last_oit_after)));
                    row.push_back(
                        Value::makeInt64(static_cast<int64_t>(stats.total_transactions_swept)));
                    row.push_back(Value::makeBoolean(stats.sweep_in_progress));

                    current_result_set_->addRow(std::move(row));
                }
                else
                {
                    // SweepManager not available - return row with zeros
                    std::vector<Value> row;
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeBoolean(false));

                    current_result_set_->addRow(std::move(row));
                }
            }
            else if (table_name == "MON_GARBAGE_COLLECTION")
            {
                // Add columns for GC statistics
                current_result_set_->addColumn("MON$TUPLES_REMOVED", core::DataType::INT64);
                current_result_set_->addColumn("MON$PAGES_CLEANED", core::DataType::INT64);
                current_result_set_->addColumn("MON$COOPERATIVE_RUNS", core::DataType::INT64);
                current_result_set_->addColumn("MON$BACKGROUND_RUNS", core::DataType::INT64);
                current_result_set_->addColumn("MON$LAST_BG_TIME", core::DataType::INT64);
                current_result_set_->addColumn("MON$LAST_BG_DURATION_MS", core::DataType::INT64);
                current_result_set_->addColumn("MON$DIRTY_PAGE_COUNT", core::DataType::INT64);
                current_result_set_->addColumn("MON$SPACE_RECLAIMED", core::DataType::INT64);

                // Enhanced metrics - Duration histogram
                current_result_set_->addColumn("MON$DURATION_0_10MS", core::DataType::INT64);
                current_result_set_->addColumn("MON$DURATION_10_50MS", core::DataType::INT64);
                current_result_set_->addColumn("MON$DURATION_50_100MS", core::DataType::INT64);
                current_result_set_->addColumn("MON$DURATION_100_500MS", core::DataType::INT64);
                current_result_set_->addColumn("MON$DURATION_500_1000MS", core::DataType::INT64);
                current_result_set_->addColumn("MON$DURATION_1000MS_PLUS", core::DataType::INT64);

                // Enhanced metrics - Page efficiency
                current_result_set_->addColumn("MON$PAGES_NO_GARBAGE", core::DataType::INT64);
                current_result_set_->addColumn("MON$MAX_SPACE_RECLAIMED_PAGE",
                                               core::DataType::INT64);

                // Enhanced metrics - Garbage accumulation
                current_result_set_->addColumn("MON$TOTAL_DIRTY_MARKED", core::DataType::INT64);

                // Current tuning parameters
                current_result_set_->addColumn("MON$COOPERATIVE_RATE", core::DataType::INT64);
                current_result_set_->addColumn("MON$BACKGROUND_INTERVAL_MS", core::DataType::INT64);

                // Get GC statistics from garbage collector
                auto gc = db_->garbage_collector();
                if (gc)
                {
                    auto stats = gc->getStatistics();

                    // Create the result row
                    std::vector<Value> row;
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.tuples_removed)));
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.pages_cleaned)));
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.cooperative_runs)));
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.background_runs)));
                    row.push_back(
                        Value::makeInt64(static_cast<int64_t>(stats.last_background_time)));
                    row.push_back(
                        Value::makeInt64(static_cast<int64_t>(stats.last_background_duration_ms)));
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.dirty_page_count)));
                    row.push_back(
                        Value::makeInt64(static_cast<int64_t>(stats.space_reclaimed_bytes)));

                    // Enhanced metrics - Duration histogram
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.duration_0_10ms)));
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.duration_10_50ms)));
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.duration_50_100ms)));
                    row.push_back(Value::makeInt64(static_cast<int64_t>(stats.duration_100_500ms)));
                    row.push_back(
                        Value::makeInt64(static_cast<int64_t>(stats.duration_500_1000ms)));
                    row.push_back(
                        Value::makeInt64(static_cast<int64_t>(stats.duration_1000ms_plus)));

                    // Enhanced metrics - Page efficiency
                    row.push_back(
                        Value::makeInt64(static_cast<int64_t>(stats.pages_with_no_garbage)));
                    row.push_back(Value::makeInt64(
                        static_cast<int64_t>(stats.max_space_reclaimed_single_page)));

                    // Enhanced metrics - Garbage accumulation
                    row.push_back(
                        Value::makeInt64(static_cast<int64_t>(stats.total_dirty_pages_marked)));

                    // Current tuning parameters
                    row.push_back(
                        Value::makeInt64(static_cast<int64_t>(stats.current_cooperative_rate)));
                    row.push_back(Value::makeInt64(
                        static_cast<int64_t>(stats.current_background_interval_ms)));

                    current_result_set_->addRow(std::move(row));
                }
                else
                {
                    // GarbageCollector not available - return row with zeros
                    std::vector<Value> row;
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));

                    // Enhanced metrics - Duration histogram (zeros)
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));

                    // Enhanced metrics - Page efficiency (zeros)
                    row.push_back(Value::makeInt64(0));
                    row.push_back(Value::makeInt64(0));

                    // Enhanced metrics - Garbage accumulation (zeros)
                    row.push_back(Value::makeInt64(0));

                    // Current tuning parameters (defaults)
                    row.push_back(Value::makeInt64(100));  // Default cooperative_rate
                    row.push_back(Value::makeInt64(5000)); // Default background_interval_ms

                    current_result_set_->addRow(std::move(row));
                }
            }
            else if (table_name == "MON_ACTIVE_TRANSACTIONS")
            {
                // Add columns for active transaction information
                current_result_set_->addColumn("MON$TRANSACTION_ID", core::DataType::INT64);
                current_result_set_->addColumn("MON$PROC_ID", core::DataType::INT64);
                current_result_set_->addColumn("MON$AGE_SECONDS", core::DataType::INT64);
                current_result_set_->addColumn("MON$ISOLATION_LEVEL", core::DataType::INT64);
                current_result_set_->addColumn("MON$IS_READ_ONLY", core::DataType::BOOLEAN);
                current_result_set_->addColumn("MON$START_TIME", core::DataType::INT64);

                // Get all active backends from ProcArray
                std::vector<core::ProcessControlBlock> active_backends;
                core::ErrorContext err_ctx;
                auto status =
                    core::ProcArrayManager::getAllActiveBackends(&active_backends, &err_ctx);

                if (status == core::Status::OK)
                {
                    // Get current time for age calculation
                    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now().time_since_epoch());

                    // Add a row for each active backend with a transaction
                    for (const auto &backend : active_backends)
                    {
                        // Skip backends without active transactions
                        if (backend.xid == 0 || backend.xact_start_time == 0)
                        {
                            continue;
                        }

                        // Calculate transaction age
                        std::chrono::microseconds backend_start_time(backend.xact_start_time);
                        uint64_t age_microseconds = (now - backend_start_time).count();
                        uint64_t age_seconds = age_microseconds / 1000000;

                        // Create the result row
                        std::vector<Value> row;
                        row.push_back(Value::makeInt64(static_cast<int64_t>(backend.xid)));
                        row.push_back(Value::makeInt64(static_cast<int64_t>(backend.proc_id)));
                        row.push_back(Value::makeInt64(static_cast<int64_t>(age_seconds)));
                        row.push_back(
                            Value::makeInt64(static_cast<int64_t>(backend.isolation_level)));
                        row.push_back(Value::makeBoolean(backend.is_read_only));
                        row.push_back(
                            Value::makeInt64(static_cast<int64_t>(backend.xact_start_time)));

                        current_result_set_->addRow(std::move(row));
                    }
                }
                // If getAllActiveBackends fails, return empty result set (no error)
            }
            else
            {
                error("Unknown monitoring table: " + table_name);
            }
        }

        void Executor::evaluateExpression()
        {
            Opcode op = static_cast<Opcode>(readByte());

            switch (op)
            {
                case Opcode::LITERAL_NULL:
                    push(Value::makeNull());
                    break;

                case Opcode::LITERAL_INT32:
                    push(Value::makeInt32(static_cast<int32_t>(readInt32())));
                    break;

                case Opcode::LITERAL_INT64:
                    push(Value::makeInt64(static_cast<int64_t>(readInt64())));
                    break;

                case Opcode::LITERAL_DOUBLE:
                    push(Value::makeFloat64(readDouble()));
                    break;

                case Opcode::LITERAL_STRING:
                    push(Value::makeVarchar(readString()));
                    break;

                case Opcode::COLUMN_REF:
                {
                    // Column reference - lookup value from current row context
                    std::string col_name = readString();

                    if (!current_row_values_ || !current_row_columns_)
                    {
                        error("Column reference outside of row context");
                    }

                    // Find column in current row
                    auto it = std::find_if(current_row_columns_->begin(),
                                           current_row_columns_->end(), [&col_name](const auto &c)
                                           { return c.column_name == col_name; });

                    if (it == current_row_columns_->end())
                    {
                        error("Column not found in row: " + col_name);
                    }

                    size_t col_idx = std::distance(current_row_columns_->begin(), it);
                    push((*current_row_values_)[col_idx]);
                    break;
                }

                // Arithmetic operators
                case Opcode::EXPR_ADD:
                case Opcode::EXPR_SUBTRACT:
                case Opcode::EXPR_MULTIPLY:
                case Opcode::EXPR_DIVIDE:
                case Opcode::EXPR_MODULO:
                // Comparison operators
                case Opcode::EXPR_EQ:
                case Opcode::EXPR_NE:
                case Opcode::EXPR_LT:
                case Opcode::EXPR_GT:
                case Opcode::EXPR_LE:
                case Opcode::EXPR_GE:
                // Logical operators
                case Opcode::EXPR_AND:
                case Opcode::EXPR_OR:
                    executeBinaryOp(op);
                    break;

                // Type conversion
                case Opcode::EXPR_CAST:
                {
                    // Read try_cast flag
                    bool is_try_cast = readByte() != 0;

                    // Read target type
                    Opcode type_op = static_cast<Opcode>(readByte());
                    core::DataType target_type = core::DataType::UNKNOWN;
                    uint32_t precision = 0;

                    switch (type_op)
                    {
                        case Opcode::TYPE_INTEGER:
                            target_type = core::DataType::INT32;
                            break;
                        case Opcode::TYPE_BIGINT:
                            target_type = core::DataType::INT64;
                            break;
                        case Opcode::TYPE_DOUBLE:
                            target_type = core::DataType::FLOAT64;
                            break;
                        case Opcode::TYPE_VARCHAR:
                            target_type = core::DataType::VARCHAR;
                            precision = readInt32();
                            break;
                        default:
                            error("Unknown type in CAST");
                    }

                    // Pop value to cast (Value is already TypedValue)
                    Value value = pop();

                    // Perform cast using TypedValue conversion
                    auto converted = value.convertTo(target_type);

                    if (!converted)
                    {
                        if (is_try_cast)
                        {
                            // TRY_CAST returns NULL on failure
                            push(Value::makeNull());
                        }
                        else
                        {
                            // CAST throws error on failure
                            error("Failed to cast value to target type");
                        }
                    }
                    else
                    {
                        // Push converted value
                        push(*converted);
                    }
                    break;
                }

                // String functions
                case Opcode::FUNC_LENGTH:
                {
                    // LENGTH returns character count (charset-aware)
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("LENGTH expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();
                        // Default to UTF-8 for string values
                        uint32_t char_len = charset_manager_.getCharLength(
                            reinterpret_cast<const uint8_t *>(str.data()), str.length(),
                            core::CharacterSet::UTF8);
                        push(Value::makeInt32(static_cast<int32_t>(char_len)));
                    }
                    break;
                }

                case Opcode::FUNC_SUBSTRING:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 3)
                    {
                        error("SUBSTRING expects 3 arguments, got " + std::to_string(arg_count));
                    }

                    // Pop args in reverse order (length, start, str)
                    Value length_val = pop();
                    Value start_val = pop();
                    Value str_val = pop();

                    if (str_val.isNull() || start_val.isNull() || length_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = str_val.toString();
                        int32_t char_start = static_cast<int32_t>(start_val.toInt64());
                        int32_t char_length = static_cast<int32_t>(length_val.toInt64());

                        // SQL uses 1-based indexing
                        if (char_start < 1)
                            char_start = 1;
                        char_start--; // Convert to 0-based

                        const uint8_t *str_bytes = reinterpret_cast<const uint8_t *>(str.data());
                        uint32_t total_chars = charset_manager_.getCharLength(
                            str_bytes, str.length(), core::CharacterSet::UTF8);

                        if (char_start >= static_cast<int32_t>(total_chars) || char_length <= 0)
                        {
                            push(Value::makeVarchar(""));
                        }
                        else
                        {
                            // Find byte offset for start position
                            uint32_t byte_start = core::utf8::byte_length(str_bytes, char_start);

                            // Find byte length for the substring
                            uint32_t remaining_chars = std::min(static_cast<uint32_t>(char_length),
                                                                total_chars - char_start);
                            uint32_t byte_length =
                                core::utf8::byte_length(str_bytes + byte_start, remaining_chars);

                            std::string result = str.substr(byte_start, byte_length);
                            push(Value::makeVarchar(result));
                        }
                    }
                    break;
                }

                case Opcode::FUNC_UPPER:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("UPPER expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();
                        // Use UTF-8 aware uppercase function
                        std::string result = core::utf8::to_upper(str);
                        push(Value::makeVarchar(result));
                    }
                    break;
                }

                case Opcode::FUNC_LOWER:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("LOWER expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();
                        // Use UTF-8 aware lowercase function
                        std::string result = core::utf8::to_lower(str);
                        push(Value::makeVarchar(result));
                    }
                    break;
                }

                case Opcode::FUNC_TRIM:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("TRIM expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();

                        // Trim leading whitespace
                        size_t start = 0;
                        while (start < str.length() &&
                               std::isspace(static_cast<unsigned char>(str[start])))
                        {
                            start++;
                        }

                        // Trim trailing whitespace
                        size_t end = str.length();
                        while (end > start &&
                               std::isspace(static_cast<unsigned char>(str[end - 1])))
                        {
                            end--;
                        }

                        std::string result = str.substr(start, end - start);
                        push(Value::makeVarchar(result));
                    }
                    break;
                }

                case Opcode::FUNC_CHAR_LENGTH:
                {
                    // CHAR_LENGTH returns character count (same as LENGTH for now)
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("CHAR_LENGTH expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();
                        uint32_t char_len = charset_manager_.getCharLength(
                            reinterpret_cast<const uint8_t *>(str.data()), str.length(),
                            core::CharacterSet::UTF8);
                        push(Value::makeInt32(static_cast<int32_t>(char_len)));
                    }
                    break;
                }

                case Opcode::FUNC_OCTET_LENGTH:
                {
                    // OCTET_LENGTH returns byte count
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("OCTET_LENGTH expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();
                        push(Value::makeInt32(static_cast<int32_t>(str.length())));
                    }
                    break;
                }

                case Opcode::FUNC_CONVERT:
                {
                    // CONVERT(str, from_charset, to_charset)
                    uint8_t arg_count = readByte();
                    if (arg_count != 3)
                    {
                        error("CONVERT expects 3 arguments, got " + std::to_string(arg_count));
                    }

                    // Pop args: to_charset, from_charset, str
                    Value to_cs_val = pop();
                    Value from_cs_val = pop();
                    Value str_val = pop();

                    if (str_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = str_val.toString();
                        auto from_cs = static_cast<core::CharacterSet>(from_cs_val.toInt64());
                        auto to_cs = static_cast<core::CharacterSet>(to_cs_val.toInt64());

                        std::vector<uint8_t> output;
                        auto status =
                            charset_manager_.convert(reinterpret_cast<const uint8_t *>(str.data()),
                                                     str.length(), from_cs, output, to_cs, nullptr);

                        if (status != core::Status::OK)
                        {
                            error("Character set conversion failed");
                        }

                        std::string result(output.begin(), output.end());
                        push(Value::makeVarchar(result));
                    }
                    break;
                }

                case Opcode::FUNC_COLLATE:
                {
                    // COLLATE applies a collation to an expression (metadata only, actual
                    // comparison elsewhere) For now, we'll just pass through the value but could
                    // store collation metadata
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("COLLATE expects 2 arguments (expr, collation_id), got " +
                              std::to_string(arg_count));
                    }

                    Value collation_id = pop();
                    Value expr = pop();

                    // For now, just return the expression value
                    // In a full implementation, we'd attach collation metadata to the value
                    push(expr);
                    break;
                }

                // Aggregate functions (Note: proper aggregation requires SELECT-level support)
                // These implementations assume aggregation context is handled by caller
                case Opcode::AGG_SUM:
                case Opcode::AGG_AVG:
                case Opcode::AGG_MIN:
                case Opcode::AGG_MAX:
                case Opcode::AGG_COUNT:
                case Opcode::ARRAY_AGG:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("Aggregate function expects 1 argument");
                    }

                    // For now, just evaluate the argument expression
                    // Full aggregation support requires refactoring SELECT execution
                    // to accumulate values across rows
                    error("Aggregate functions require full aggregation support (not yet "
                          "implemented in executor)");
                    break;
                }

                // Temporal functions
                case Opcode::FUNC_DATE_ADD:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("DATE_ADD expects 2 arguments");
                    }

                    Value days_val = pop();
                    Value date_val = pop();

                    if (date_val.isNull() || days_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        // Treat date as Unix timestamp (seconds since epoch)
                        // Add days * 86400 seconds
                        int64_t timestamp = date_val.toInt64();
                        int64_t days = days_val.toInt64();
                        int64_t result = timestamp + (days * 86400);
                        push(Value::makeInt64(result));
                    }
                    break;
                }

                case Opcode::FUNC_DATE_SUB:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("DATE_SUB expects 2 arguments");
                    }

                    Value days_val = pop();
                    Value date_val = pop();

                    if (date_val.isNull() || days_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        int64_t timestamp = date_val.toInt64();
                        int64_t days = days_val.toInt64();
                        int64_t result = timestamp - (days * 86400);
                        push(Value::makeInt64(result));
                    }
                    break;
                }

                case Opcode::FUNC_DATE_DIFF:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("DATE_DIFF expects 2 arguments");
                    }

                    Value date2_val = pop();
                    Value date1_val = pop();

                    if (date1_val.isNull() || date2_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        int64_t timestamp1 = date1_val.toInt64();
                        int64_t timestamp2 = date2_val.toInt64();
                        int64_t diff_days = (timestamp1 - timestamp2) / 86400;
                        push(Value::makeInt64(diff_days));
                    }
                    break;
                }

                case Opcode::FUNC_NOW:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 0)
                    {
                        error("NOW expects 0 arguments");
                    }

                    // Return current Unix timestamp
                    auto now = std::chrono::system_clock::now();
                    auto timestamp =
                        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
                            .count();
                    push(Value::makeInt64(timestamp));
                    break;
                }

                case Opcode::FUNC_AT_TIME_ZONE:
                {
                    // timestamp AT TIME ZONE timezone_id
                    // Converts GMT timestamp to specified timezone for display
                    // Returns formatted string in target timezone
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("AT TIME ZONE expects 2 arguments (timestamp, timezone_id)");
                    }

                    Value tz_val = pop();        // timezone_id
                    Value timestamp_val = pop(); // timestamp in GMT

                    if (timestamp_val.isNull() || tz_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        int64_t gmt_microseconds = timestamp_val.toInt64();
                        uint16_t timezone_id = static_cast<uint16_t>(tz_val.toInt64());

                        // Format timestamp in target timezone
                        std::string result =
                            timezone_manager_.formatTimestamp(gmt_microseconds, timezone_id, true);
                        push(Value::makeVarchar(result));
                    }
                    break;
                }

                case Opcode::FUNC_CURRENT_DATE:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 0)
                    {
                        error("CURRENT_DATE expects 0 arguments");
                    }

                    // Return current date as Unix timestamp (midnight)
                    auto now = std::chrono::system_clock::now();
                    auto timestamp =
                        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
                            .count();
                    // Round down to midnight
                    timestamp = (timestamp / 86400) * 86400;
                    push(Value::makeInt64(timestamp));
                    break;
                }

                // JSON functions (Phase 1 Task 7)
                case Opcode::JSON_EXTRACT:
                case Opcode::JSON_ARROW:
                case Opcode::JSON_DOUBLE_ARROW:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("JSON extraction expects 2 arguments (json, path)");
                    }

                    Value path = pop();
                    Value json_data = pop();

                    if (json_data.isNull() || path.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        try {
                            // Parse JSON data
                            json j = json::parse(json_data.toString());

                            // Parse JSONPath and extract value
                            std::vector<std::string> path_components = parseJSONPath(path.toString());
                            json result = extractJSONValue(j, path_components);

                            // Return based on operator type
                            if (op == Opcode::JSON_DOUBLE_ARROW)
                            {
                                // ->> returns text
                                push(jsonToValue(result, true));
                            }
                            else
                            {
                                // -> and JSON_EXTRACT return JSON
                                push(jsonToValue(result, false));
                            }
                        } catch (const json::exception& e) {
                            // Invalid JSON, return null
                            push(Value::makeNull());
                        }
                    }
                    break;
                }

                case Opcode::JSONB_EXTRACT_PATH:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count < 2)
                    {
                        error("JSONB_EXTRACT_PATH expects at least 2 arguments");
                    }

                    // Pop path elements (in reverse order)
                    std::vector<std::string> path_components;
                    for (uint8_t i = 0; i < arg_count - 1; i++)
                    {
                        Value path_elem = pop();
                        path_components.insert(path_components.begin(), path_elem.toString());
                    }
                    Value json_data = pop();

                    if (json_data.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        try {
                            // Parse JSON data
                            json j = json::parse(json_data.toString());

                            // Extract value using path components
                            json result = extractJSONValue(j, path_components);

                            // Return as JSON
                            push(jsonToValue(result, false));
                        } catch (const json::exception& e) {
                            // Invalid JSON, return null
                            push(Value::makeNull());
                        }
                    }
                    break;
                }

                case Opcode::JSON_HASH_ARROW:
                case Opcode::JSON_HASH_DOUBLE_ARROW:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("JSON path operator expects 2 arguments (json, path_array)");
                    }

                    Value path_array = pop();
                    Value json_data = pop();

                    if (json_data.isNull() || path_array.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        try {
                            // Parse JSON data
                            json j = json::parse(json_data.toString());

                            // Parse path array (PostgreSQL uses text arrays like '{field,subfield}')
                            // For now, assume comma-separated path
                            std::string path_str = path_array.toString();
                            std::vector<std::string> path_components;

                            // Remove braces if present
                            if (!path_str.empty() && path_str[0] == '{' && path_str.back() == '}') {
                                path_str = path_str.substr(1, path_str.length() - 2);
                            }

                            // Split by comma
                            std::stringstream ss(path_str);
                            std::string component;
                            while (std::getline(ss, component, ',')) {
                                // Trim whitespace
                                component.erase(0, component.find_first_not_of(" \t"));
                                component.erase(component.find_last_not_of(" \t") + 1);
                                path_components.push_back(component);
                            }

                            // Extract value using path components
                            json result = extractJSONValue(j, path_components);

                            // Return based on operator type
                            if (op == Opcode::JSON_HASH_DOUBLE_ARROW)
                            {
                                // #>> returns text
                                push(jsonToValue(result, true));
                            }
                            else
                            {
                                // #> returns JSON
                                push(jsonToValue(result, false));
                            }
                        } catch (const json::exception& e) {
                            // Invalid JSON, return null
                            push(Value::makeNull());
                        }
                    }
                    break;
                }

                case Opcode::JSON_OBJECT:
                case Opcode::JSONB_BUILD_OBJECT:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count % 2 != 0)
                    {
                        error("JSON_OBJECT expects even number of arguments (key-value pairs)");
                    }

                    // Pop key-value pairs (in reverse order due to stack)
                    std::vector<std::pair<Value, Value>> pairs;
                    for (uint8_t i = 0; i < arg_count / 2; i++)
                    {
                        Value value = pop();
                        Value key = pop();
                        pairs.push_back({key, value});
                    }

                    // Reverse to get correct order
                    std::reverse(pairs.begin(), pairs.end());

                    // Build JSON object
                    json obj = json::object();
                    for (const auto& pair : pairs) {
                        std::string key_str = pair.first.toString();
                        json val_json = valueToJSON(pair.second);
                        obj[key_str] = val_json;
                    }

                    // Return as JSON
                    push(Value::makeJSON(obj.dump()));
                    break;
                }

                case Opcode::JSON_ARRAY:
                case Opcode::JSONB_BUILD_ARRAY:
                {
                    uint8_t arg_count = readByte();

                    // Pop array elements (in reverse order due to stack)
                    std::vector<Value> elements;
                    for (uint8_t i = 0; i < arg_count; i++)
                    {
                        elements.push_back(pop());
                    }

                    // Reverse to get correct order
                    std::reverse(elements.begin(), elements.end());

                    // Build JSON array
                    json arr = json::array();
                    for (const auto& elem : elements) {
                        arr.push_back(valueToJSON(elem));
                    }

                    // Return as JSON
                    push(Value::makeJSON(arr.dump()));
                    break;
                }

                case Opcode::JSON_SET:
                case Opcode::JSONB_SET:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 3)
                    {
                        error("JSON_SET expects 3 arguments (json, path, value)");
                    }

                    Value new_value = pop();
                    Value path = pop();
                    Value json_data = pop();

                    if (json_data.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        try {
                            // Parse JSON data
                            json j = json::parse(json_data.toString());

                            // Parse JSONPath
                            std::vector<std::string> path_components = parseJSONPath(path.toString());

                            // Navigate to parent and set value
                            if (!path_components.empty()) {
                                json* current = &j;

                                // Navigate to parent
                                for (size_t i = 0; i < path_components.size() - 1; i++) {
                                    const auto& component = path_components[i];

                                    // Try as array index
                                    try {
                                        size_t idx = std::stoull(component);
                                        if (current->is_array() && idx < current->size()) {
                                            current = &((*current)[idx]);
                                            continue;
                                        }
                                    } catch (...) {}

                                    // Try as object key
                                    if (current->is_object()) {
                                        if (!current->contains(component)) {
                                            (*current)[component] = json::object();
                                        }
                                        current = &((*current)[component]);
                                    } else {
                                        // Cannot navigate further
                                        push(json_data);
                                        break;
                                    }
                                }

                                // Set the final value
                                const auto& final_key = path_components.back();
                                if (current->is_object()) {
                                    (*current)[final_key] = valueToJSON(new_value);
                                } else if (current->is_array()) {
                                    try {
                                        size_t idx = std::stoull(final_key);
                                        if (idx < current->size()) {
                                            (*current)[idx] = valueToJSON(new_value);
                                        }
                                    } catch (...) {}
                                }
                            }

                            // Return modified JSON
                            push(Value::makeJSON(j.dump()));
                        } catch (const json::exception& e) {
                            // Invalid JSON, return original
                            push(json_data);
                        }
                    }
                    break;
                }

                case Opcode::JSON_INSERT:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 3)
                    {
                        error("JSON_INSERT expects 3 arguments (json, path, value)");
                    }

                    Value new_value = pop();
                    Value path = pop();
                    Value json_data = pop();

                    if (json_data.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        try {
                            // Parse JSON data
                            json j = json::parse(json_data.toString());

                            // Parse JSONPath
                            std::vector<std::string> path_components = parseJSONPath(path.toString());

                            // JSON_INSERT only inserts if path doesn't exist
                            if (!path_components.empty()) {
                                json* current = &j;
                                bool exists = true;

                                // Navigate to parent, checking if path exists
                                for (size_t i = 0; i < path_components.size() - 1 && exists; i++) {
                                    const auto& component = path_components[i];

                                    // Try as array index
                                    try {
                                        size_t idx = std::stoull(component);
                                        if (current->is_array() && idx < current->size()) {
                                            current = &((*current)[idx]);
                                            continue;
                                        }
                                    } catch (...) {}

                                    // Try as object key
                                    if (current->is_object() && current->contains(component)) {
                                        current = &((*current)[component]);
                                    } else {
                                        exists = false;
                                    }
                                }

                                // Check if final key exists
                                if (exists) {
                                    const auto& final_key = path_components.back();
                                    if (current->is_object() && current->contains(final_key)) {
                                        // Path exists, don't insert
                                        push(json_data);
                                        break;
                                    } else if (current->is_array()) {
                                        try {
                                            size_t idx = std::stoull(final_key);
                                            if (idx < current->size()) {
                                                // Path exists, don't insert
                                                push(json_data);
                                                break;
                                            }
                                        } catch (...) {}
                                    }

                                    // Path doesn't exist, insert value
                                    if (current->is_object()) {
                                        (*current)[final_key] = valueToJSON(new_value);
                                    }
                                }
                            }

                            // Return modified JSON
                            push(Value::makeJSON(j.dump()));
                        } catch (const json::exception& e) {
                            // Invalid JSON, return original
                            push(json_data);
                        }
                    }
                    break;
                }

                case Opcode::JSON_REMOVE:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("JSON_REMOVE expects 2 arguments (json, path)");
                    }

                    Value path = pop();
                    Value json_data = pop();

                    if (json_data.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        try {
                            // Parse JSON data
                            json j = json::parse(json_data.toString());

                            // Parse JSONPath
                            std::vector<std::string> path_components = parseJSONPath(path.toString());

                            // Navigate to parent and remove value
                            if (!path_components.empty()) {
                                json* current = &j;

                                // Navigate to parent
                                for (size_t i = 0; i < path_components.size() - 1; i++) {
                                    const auto& component = path_components[i];

                                    // Try as array index
                                    try {
                                        size_t idx = std::stoull(component);
                                        if (current->is_array() && idx < current->size()) {
                                            current = &((*current)[idx]);
                                            continue;
                                        }
                                    } catch (...) {}

                                    // Try as object key
                                    if (current->is_object() && current->contains(component)) {
                                        current = &((*current)[component]);
                                    } else {
                                        // Path not found, return original
                                        push(json_data);
                                        break;
                                    }
                                }

                                // Remove the final key
                                const auto& final_key = path_components.back();
                                if (current->is_object() && current->contains(final_key)) {
                                    current->erase(final_key);
                                } else if (current->is_array()) {
                                    try {
                                        size_t idx = std::stoull(final_key);
                                        if (idx < current->size()) {
                                            current->erase(current->begin() + idx);
                                        }
                                    } catch (...) {}
                                }
                            }

                            // Return modified JSON
                            push(Value::makeJSON(j.dump()));
                        } catch (const json::exception& e) {
                            // Invalid JSON, return original
                            push(json_data);
                        }
                    }
                    break;
                }

                // Conditional expressions (Phase 1 Task 8)
                case Opcode::COALESCE:
                {
                    uint8_t arg_count = readByte();

                    // Pop all arguments (in reverse order)
                    std::vector<Value> args;
                    for (uint8_t i = 0; i < arg_count; i++)
                    {
                        args.push_back(pop());
                    }

                    // Reverse to get correct order
                    std::reverse(args.begin(), args.end());

                    // Return first non-NULL value
                    for (const auto& arg : args)
                    {
                        if (!arg.isNull())
                        {
                            push(arg);
                            break;
                        }
                    }

                    // If all NULL, push NULL
                    if (args.empty() || std::all_of(args.begin(), args.end(),
                                                     [](const Value& v) { return v.isNull(); }))
                    {
                        push(Value::makeNull());
                    }
                    break;
                }

                case Opcode::NULLIF:
                {
                    // Pop two arguments
                    Value expr2 = pop();
                    Value expr1 = pop();

                    // If either is NULL, return NULL
                    if (expr1.isNull() || expr2.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        // Compare values (same logic as EXPR_EQ)
                        bool are_equal;
                        if (core::TypeSystem::isString(expr1.type()) ||
                            core::TypeSystem::isString(expr2.type()))
                        {
                            are_equal = compareStrings(expr1.toString(), expr2.toString()) == 0;
                        }
                        else if (expr1.type() == core::DataType::FLOAT64 ||
                                 expr2.type() == core::DataType::FLOAT64)
                        {
                            are_equal = expr1.toDouble() == expr2.toDouble();
                        }
                        else
                        {
                            are_equal = expr1.toInt64() == expr2.toInt64();
                        }

                        // If equal, return NULL; otherwise return expr1
                        if (are_equal)
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            push(expr1);
                        }
                    }
                    break;
                }

                case Opcode::CASE_WHEN:
                {
                    uint8_t flags = readByte();
                    uint8_t when_count = readByte();

                    bool has_case_operand = (flags & 0x01) != 0;
                    bool has_else = (flags & 0x02) != 0;

                    // Pop ELSE result if present
                    Value else_result;
                    if (has_else)
                    {
                        else_result = pop();
                    }

                    // Pop all WHEN results and conditions (in reverse)
                    std::vector<Value> results;
                    std::vector<Value> conditions;
                    for (uint8_t i = 0; i < when_count; i++)
                    {
                        results.push_back(pop());
                        conditions.push_back(pop());
                    }
                    std::reverse(results.begin(), results.end());
                    std::reverse(conditions.begin(), conditions.end());

                    // Pop case operand if present (simple CASE)
                    Value case_operand;
                    if (has_case_operand)
                    {
                        case_operand = pop();
                    }

                    // Evaluate WHEN clauses
                    Value result = Value::makeNull();
                    bool found_match = false;

                    for (size_t i = 0; i < when_count; i++)
                    {
                        bool matches = false;

                        if (has_case_operand)
                        {
                            // Simple CASE: compare case_operand with condition
                            if (!case_operand.isNull() && !conditions[i].isNull())
                            {
                                // Use same comparison logic as EXPR_EQ
                                if (core::TypeSystem::isString(case_operand.type()) ||
                                    core::TypeSystem::isString(conditions[i].type()))
                                {
                                    matches = compareStrings(case_operand.toString(), conditions[i].toString()) == 0;
                                }
                                else if (case_operand.type() == core::DataType::FLOAT64 ||
                                         conditions[i].type() == core::DataType::FLOAT64)
                                {
                                    matches = case_operand.toDouble() == conditions[i].toDouble();
                                }
                                else
                                {
                                    matches = case_operand.toInt64() == conditions[i].toInt64();
                                }
                            }
                        }
                        else
                        {
                            // Searched CASE: evaluate condition as boolean
                            matches = (!conditions[i].isNull() && conditions[i].toInt64() != 0);
                        }

                        if (matches)
                        {
                            result = results[i];
                            found_match = true;
                            break;
                        }
                    }

                    // If no match found, use ELSE result (or NULL if no ELSE)
                    if (!found_match)
                    {
                        result = has_else ? else_result : Value::makeNull();
                    }

                    push(result);
                    break;
                }

                // Array functions (Phase 2 Task 12)
                case Opcode::ARRAY_TO_STRING:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count < 2 || arg_count > 3)
                    {
                        error("ARRAY_TO_STRING expects 2 or 3 arguments (array, delimiter [, null_string])");
                    }

                    Value null_string = (arg_count == 3) ? pop() : Value::makeNull();
                    Value delimiter = pop();
                    Value array = pop();

                    if (array.isNull() || delimiter.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        try {
                            // Parse JSON array
                            json j_array = json::parse(array.toString());
                            if (!j_array.is_array())
                            {
                                push(Value::makeNull());
                                break;
                            }

                            std::string result;
                            std::string delim = delimiter.toString();
                            std::string null_str = null_string.isNull() ? "" : null_string.toString();
                            bool first = true;

                            for (const auto& elem : j_array)
                            {
                                if (!first) result += delim;
                                first = false;

                                if (elem.is_null())
                                {
                                    result += null_str;
                                }
                                else if (elem.is_string())
                                {
                                    result += elem.get<std::string>();
                                }
                                else
                                {
                                    result += elem.dump();
                                }
                            }

                            push(Value::makeText(result));
                        } catch (const json::exception& e) {
                            push(Value::makeNull());
                        }
                    }
                    break;
                }

                case Opcode::STRING_TO_ARRAY:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count < 2 || arg_count > 3)
                    {
                        error("STRING_TO_ARRAY expects 2 or 3 arguments (string, delimiter [, null_string])");
                    }

                    Value null_string = (arg_count == 3) ? pop() : Value::makeNull();
                    Value delimiter = pop();
                    Value str = pop();

                    if (str.isNull() || delimiter.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string text = str.toString();
                        std::string delim = delimiter.toString();
                        std::string null_str = null_string.isNull() ? "" : null_string.toString();

                        json j_array = json::array();

                        if (delim.empty())
                        {
                            // Empty delimiter: split into characters
                            for (char c : text)
                            {
                                j_array.push_back(std::string(1, c));
                            }
                        }
                        else
                        {
                            // Split by delimiter
                            size_t start = 0;
                            size_t end = text.find(delim);
                            while (end != std::string::npos)
                            {
                                std::string part = text.substr(start, end - start);
                                if (!null_string.isNull() && part == null_str)
                                {
                                    j_array.push_back(nullptr);
                                }
                                else
                                {
                                    j_array.push_back(part);
                                }
                                start = end + delim.length();
                                end = text.find(delim, start);
                            }
                            // Add last part
                            std::string part = text.substr(start);
                            if (!null_string.isNull() && part == null_str)
                            {
                                j_array.push_back(nullptr);
                            }
                            else
                            {
                                j_array.push_back(part);
                            }
                        }

                        push(Value::makeJSON(j_array.dump()));
                    }
                    break;
                }

                // Extended opcodes for array functions (manipulation, operators, accessors)
                case Opcode::EXTENDED_OPCODE:
                {
                    uint8_t ext_op = readByte();

                    // Array manipulation functions
                    if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_APPEND))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("ARRAY_APPEND expects 2 arguments (array, element)");
                        }

                        Value element = pop();
                        Value array = pop();

                        if (array.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array = json::parse(array.toString());
                                if (!j_array.is_array())
                                {
                                    j_array = json::array();
                                }

                                // Append element
                                j_array.push_back(valueToJSON(element));
                                push(Value::makeJSON(j_array.dump()));
                            } catch (const json::exception& e) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_PREPEND))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("ARRAY_PREPEND expects 2 arguments (element, array)");
                        }

                        Value array = pop();
                        Value element = pop();

                        if (array.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array = json::parse(array.toString());
                                if (!j_array.is_array())
                                {
                                    j_array = json::array();
                                }

                                // Prepend element
                                j_array.insert(j_array.begin(), valueToJSON(element));
                                push(Value::makeJSON(j_array.dump()));
                            } catch (const json::exception& e) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_CAT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("ARRAY_CAT expects 2 arguments (array1, array2)");
                        }

                        Value array2 = pop();
                        Value array1 = pop();

                        if (array1.isNull() || array2.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array1 = json::parse(array1.toString());
                                json j_array2 = json::parse(array2.toString());

                                if (!j_array1.is_array()) j_array1 = json::array();
                                if (!j_array2.is_array()) j_array2 = json::array();

                                // Concatenate arrays
                                for (const auto& elem : j_array2)
                                {
                                    j_array1.push_back(elem);
                                }
                                push(Value::makeJSON(j_array1.dump()));
                            } catch (const json::exception& e) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_REMOVE))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("ARRAY_REMOVE expects 2 arguments (array, element)");
                        }

                        Value element = pop();
                        Value array = pop();

                        if (array.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array = json::parse(array.toString());
                                if (!j_array.is_array())
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                json element_json = valueToJSON(element);
                                json result = json::array();

                                // Remove all occurrences of element
                                for (const auto& elem : j_array)
                                {
                                    if (elem != element_json)
                                    {
                                        result.push_back(elem);
                                    }
                                }
                                push(Value::makeJSON(result.dump()));
                            } catch (const json::exception& e) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_REPLACE))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 3)
                        {
                            error("ARRAY_REPLACE expects 3 arguments (array, from, to)");
                        }

                        Value to_value = pop();
                        Value from_value = pop();
                        Value array = pop();

                        if (array.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array = json::parse(array.toString());
                                if (!j_array.is_array())
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                json from_json = valueToJSON(from_value);
                                json to_json = valueToJSON(to_value);
                                json result = json::array();

                                // Replace all occurrences
                                for (const auto& elem : j_array)
                                {
                                    if (elem == from_json)
                                    {
                                        result.push_back(to_json);
                                    }
                                    else
                                    {
                                        result.push_back(elem);
                                    }
                                }
                                push(Value::makeJSON(result.dump()));
                            } catch (const json::exception& e) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    // Array operators
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_OVERLAP))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("Array && operator expects 2 arguments");
                        }

                        Value array2 = pop();
                        Value array1 = pop();

                        if (array1.isNull() || array2.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array1 = json::parse(array1.toString());
                                json j_array2 = json::parse(array2.toString());

                                if (!j_array1.is_array() || !j_array2.is_array())
                                {
                                    push(Value::makeBoolean(false));
                                    break;
                                }

                                // Check for common elements
                                bool has_overlap = false;
                                for (const auto& elem1 : j_array1)
                                {
                                    for (const auto& elem2 : j_array2)
                                    {
                                        if (elem1 == elem2)
                                        {
                                            has_overlap = true;
                                            break;
                                        }
                                    }
                                    if (has_overlap) break;
                                }
                                push(Value::makeBoolean(has_overlap));
                            } catch (const json::exception& e) {
                                push(Value::makeBoolean(false));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_CONTAINS))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("Array @> operator expects 2 arguments");
                        }

                        Value array2 = pop();
                        Value array1 = pop();

                        if (array1.isNull() || array2.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array1 = json::parse(array1.toString());
                                json j_array2 = json::parse(array2.toString());

                                if (!j_array1.is_array() || !j_array2.is_array())
                                {
                                    push(Value::makeBoolean(false));
                                    break;
                                }

                                // Check if array1 contains all elements of array2
                                bool contains_all = true;
                                for (const auto& elem2 : j_array2)
                                {
                                    bool found = false;
                                    for (const auto& elem1 : j_array1)
                                    {
                                        if (elem1 == elem2)
                                        {
                                            found = true;
                                            break;
                                        }
                                    }
                                    if (!found)
                                    {
                                        contains_all = false;
                                        break;
                                    }
                                }
                                push(Value::makeBoolean(contains_all));
                            } catch (const json::exception& e) {
                                push(Value::makeBoolean(false));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_CONTAINED_BY))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("Array <@ operator expects 2 arguments");
                        }

                        Value array2 = pop();
                        Value array1 = pop();

                        if (array1.isNull() || array2.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array1 = json::parse(array1.toString());
                                json j_array2 = json::parse(array2.toString());

                                if (!j_array1.is_array() || !j_array2.is_array())
                                {
                                    push(Value::makeBoolean(false));
                                    break;
                                }

                                // Check if array1 is subset of array2
                                bool is_subset = true;
                                for (const auto& elem1 : j_array1)
                                {
                                    bool found = false;
                                    for (const auto& elem2 : j_array2)
                                    {
                                        if (elem1 == elem2)
                                        {
                                            found = true;
                                            break;
                                        }
                                    }
                                    if (!found)
                                    {
                                        is_subset = false;
                                        break;
                                    }
                                }
                                push(Value::makeBoolean(is_subset));
                            } catch (const json::exception& e) {
                                push(Value::makeBoolean(false));
                            }
                        }
                    }
                    // Array accessor functions
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_LENGTH))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count < 1 || arg_count > 2)
                        {
                            error("ARRAY_LENGTH expects 1 or 2 arguments (array [, dimension])");
                        }

                        Value dimension = (arg_count == 2) ? pop() : Value::makeInt32(1);
                        Value array = pop();

                        if (array.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array = json::parse(array.toString());
                                if (!j_array.is_array())
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                // For now, only support 1D arrays (dimension = 1)
                                push(Value::makeInt64(j_array.size()));
                            } catch (const json::exception& e) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_DIMS))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ARRAY_DIMS expects 1 argument (array)");
                        }

                        Value array = pop();

                        if (array.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array = json::parse(array.toString());
                                if (!j_array.is_array())
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                // Return dimensions as text: [1:n]
                                std::string dims = "[1:" + std::to_string(j_array.size()) + "]";
                                push(Value::makeText(dims));
                            } catch (const json::exception& e) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_UPPER))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count < 1 || arg_count > 2)
                        {
                            error("ARRAY_UPPER expects 1 or 2 arguments (array [, dimension])");
                        }

                        Value dimension = (arg_count == 2) ? pop() : Value::makeInt32(1);
                        Value array = pop();

                        if (array.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array = json::parse(array.toString());
                                if (!j_array.is_array())
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                // Upper bound = size (1-indexed)
                                push(Value::makeInt64(j_array.size()));
                            } catch (const json::exception& e) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_LOWER))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count < 1 || arg_count > 2)
                        {
                            error("ARRAY_LOWER expects 1 or 2 arguments (array [, dimension])");
                        }

                        Value dimension = (arg_count == 2) ? pop() : Value::makeInt32(1);
                        Value array = pop();

                        if (array.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                json j_array = json::parse(array.toString());
                                if (!j_array.is_array())
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                // Lower bound = 1 (PostgreSQL uses 1-based indexing)
                                push(Value::makeInt64(1));
                            } catch (const json::exception& e) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    // Array construction (Phase 2 Task 12)
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ARRAY_CONSTRUCT))
                    {
                        uint8_t count = readByte();

                        // Pop all elements from stack (in reverse order)
                        std::vector<Value> elements;
                        elements.reserve(count);
                        for (uint8_t i = 0; i < count; i++)
                        {
                            elements.push_back(pop());
                        }
                        std::reverse(elements.begin(), elements.end());

                        // Build JSON array
                        json arr = json::array();
                        for (const auto& elem : elements)
                        {
                            arr.push_back(valueToJSON(elem));
                        }

                        push(Value::makeJSON(arr.dump()));
                    }
                    // Subquery execution (Phase 2 Wave 2 - Agent B)
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_SCALAR))
                    {
                        // Save execution context
                        auto saved_result_set = std::move(current_result_set_);
                        auto saved_table = current_table_;
                        size_t saved_pc = pc_;

                        // Execute subquery
                        current_result_set_ = std::make_unique<ResultSet>();

                        // Read and execute the nested SELECT
                        Opcode subquery_op = static_cast<Opcode>(readByte());
                        if (subquery_op == Opcode::SELECT)
                        {
                            executeSelect();

                            // Validate: must return exactly one row and one column
                            if (!current_result_set_ || current_result_set_->rowCount() == 0)
                            {
                                // Scalar subquery with no rows returns NULL
                                push(Value::makeNull());
                            }
                            else if (current_result_set_->rowCount() > 1)
                            {
                                error("Scalar subquery returned more than one row");
                            }
                            else if (current_result_set_->columnCount() != 1)
                            {
                                error("Scalar subquery must return exactly one column");
                            }
                            else
                            {
                                // Return the single value
                                push(current_result_set_->getValue(0, 0));
                            }
                        }
                        else
                        {
                            error("Subquery must be a SELECT statement");
                        }

                        // Skip to EXT_SUBQUERY_END marker
                        while (pc_ < bytecode_size_)
                        {
                            uint8_t b = readByte();
                            if (b == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
                            {
                                uint8_t ext = readByte();
                                if (ext == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_END))
                                {
                                    break;
                                }
                            }
                        }

                        // Restore context
                        current_result_set_ = std::move(saved_result_set);
                        current_table_ = saved_table;
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_EXISTS))
                    {
                        // Save execution context
                        auto saved_result_set = std::move(current_result_set_);
                        auto saved_table = current_table_;
                        size_t saved_pc = pc_;

                        // Execute subquery
                        current_result_set_ = std::make_unique<ResultSet>();

                        // Read and execute the nested SELECT
                        Opcode subquery_op = static_cast<Opcode>(readByte());
                        if (subquery_op == Opcode::SELECT)
                        {
                            executeSelect();

                            // EXISTS returns true if any rows returned
                            bool has_rows = current_result_set_ && current_result_set_->rowCount() > 0;
                            push(Value::makeBoolean(has_rows));
                        }
                        else
                        {
                            error("Subquery must be a SELECT statement");
                        }

                        // Skip to EXT_SUBQUERY_END marker
                        while (pc_ < bytecode_size_)
                        {
                            uint8_t b = readByte();
                            if (b == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
                            {
                                uint8_t ext = readByte();
                                if (ext == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_END))
                                {
                                    break;
                                }
                            }
                        }

                        // Restore context
                        current_result_set_ = std::move(saved_result_set);
                        current_table_ = saved_table;
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_IN))
                    {
                        // Pop test value (left operand of IN)
                        Value test_value = pop();

                        // Save execution context
                        auto saved_result_set = std::move(current_result_set_);
                        auto saved_table = current_table_;
                        size_t saved_pc = pc_;

                        // Execute subquery
                        current_result_set_ = std::make_unique<ResultSet>();

                        // Read and execute the nested SELECT
                        Opcode subquery_op = static_cast<Opcode>(readByte());
                        if (subquery_op == Opcode::SELECT)
                        {
                            executeSelect();

                            // Validate: must return exactly one column
                            if (!current_result_set_ || current_result_set_->columnCount() != 1)
                            {
                                error("IN subquery must return exactly one column");
                            }

                            // Build set for membership test with NULL tracking
                            bool has_null = false;
                            bool found_match = false;

                            for (size_t r = 0; r < current_result_set_->rowCount(); r++)
                            {
                                Value row_value = current_result_set_->getValue(r, 0);
                                if (row_value.isNull())
                                {
                                    has_null = true;
                                }
                                else if (!test_value.isNull() && row_value.equals(test_value))
                                {
                                    found_match = true;
                                    break;  // Early exit on match
                                }
                            }

                            // SQL NULL semantics for IN
                            if (test_value.isNull())
                            {
                                push(Value::makeNull());  // NULL IN (...) → NULL
                            }
                            else if (found_match)
                            {
                                push(Value::makeBoolean(true));  // Value found
                            }
                            else if (has_null)
                            {
                                push(Value::makeNull());  // Not found but NULLs present → NULL
                            }
                            else
                            {
                                push(Value::makeBoolean(false));  // Not found and no NULLs
                            }
                        }
                        else
                        {
                            error("Subquery must be a SELECT statement");
                        }

                        // Skip to EXT_SUBQUERY_END marker
                        while (pc_ < bytecode_size_)
                        {
                            uint8_t b = readByte();
                            if (b == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
                            {
                                uint8_t ext = readByte();
                                if (ext == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_END))
                                {
                                    break;
                                }
                            }
                        }

                        // Restore context
                        current_result_set_ = std::move(saved_result_set);
                        current_table_ = saved_table;
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_NOT_IN))
                    {
                        // Pop test value (left operand of NOT IN)
                        Value test_value = pop();

                        // Save execution context
                        auto saved_result_set = std::move(current_result_set_);
                        auto saved_table = current_table_;
                        size_t saved_pc = pc_;

                        // Execute subquery
                        current_result_set_ = std::make_unique<ResultSet>();

                        // Read and execute the nested SELECT
                        Opcode subquery_op = static_cast<Opcode>(readByte());
                        if (subquery_op == Opcode::SELECT)
                        {
                            executeSelect();

                            // Validate: must return exactly one column
                            if (!current_result_set_ || current_result_set_->columnCount() != 1)
                            {
                                error("NOT IN subquery must return exactly one column");
                            }

                            // Build set for membership test with NULL tracking
                            bool has_null = false;
                            bool found_match = false;

                            for (size_t r = 0; r < current_result_set_->rowCount(); r++)
                            {
                                Value row_value = current_result_set_->getValue(r, 0);
                                if (row_value.isNull())
                                {
                                    has_null = true;
                                }
                                else if (!test_value.isNull() && row_value.equals(test_value))
                                {
                                    found_match = true;
                                    break;  // Early exit on match
                                }
                            }

                            // SQL NULL semantics for NOT IN (inverse of IN)
                            if (test_value.isNull())
                            {
                                push(Value::makeNull());  // NULL NOT IN (...) → NULL
                            }
                            else if (found_match)
                            {
                                push(Value::makeBoolean(false));  // Value found → false
                            }
                            else if (has_null)
                            {
                                push(Value::makeNull());  // Not found but NULLs present → NULL
                            }
                            else
                            {
                                push(Value::makeBoolean(true));  // Not found and no NULLs → true
                            }
                        }
                        else
                        {
                            error("Subquery must be a SELECT statement");
                        }

                        // Skip to EXT_SUBQUERY_END marker
                        while (pc_ < bytecode_size_)
                        {
                            uint8_t b = readByte();
                            if (b == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
                            {
                                uint8_t ext = readByte();
                                if (ext == static_cast<uint8_t>(Opcode::EXT_SUBQUERY_END))
                                {
                                    break;
                                }
                            }
                        }

                        // Restore context
                        current_result_set_ = std::move(saved_result_set);
                        current_table_ = saved_table;
                    }
                    // Spatial functions (Phase 2 Task 9.1)
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_POINT))
                    {
                        // ST_Point(x, y) - create point from coordinates
                        // Stack: [x, y] (y is on top)
                        Value y_val = pop();
                        Value x_val = pop();

                        if (y_val.isNull() || x_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                double x = x_val.toDouble();
                                double y = y_val.toDouble();

                                core::Point point{x, y};
                                push(Value::makePoint(point));
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_MAKELINE))
                    {
                        // ST_MakeLine(point1, point2, ...) - create linestring from points
                        uint8_t arg_count = readByte();

                        if (arg_count < 2)
                        {
                            error("ST_MakeLine expects at least 2 points");
                            break;
                        }

                        // Pop all points from stack (in reverse order)
                        std::vector<Value> point_values;
                        for (uint8_t i = 0; i < arg_count; i++)
                        {
                            point_values.push_back(pop());
                        }

                        // Reverse to get correct order
                        std::reverse(point_values.begin(), point_values.end());

                        // Check for nulls
                        bool has_null = false;
                        for (const auto& pv : point_values)
                        {
                            if (pv.isNull() || pv.type() != core::DataType::POINT)
                            {
                                has_null = true;
                                break;
                            }
                        }

                        if (has_null)
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                core::LineString linestring;
                                for (const auto& pv : point_values)
                                {
                                    linestring.points.push_back(pv.getPoint());
                                }

                                if (linestring.isValid())
                                {
                                    push(Value::makeLineString(linestring));
                                }
                                else
                                {
                                    push(Value::makeNull());
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_MAKEPOLYGON))
                    {
                        // ST_MakePolygon(linestring) - create polygon from linestring
                        Value linestring_val = pop();

                        if (linestring_val.isNull() || linestring_val.type() != core::DataType::LINESTRING)
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                core::LineString ls = linestring_val.getLineString();

                                // Create polygon from linestring (exterior ring)
                                core::Polygon polygon;
                                polygon.rings.push_back(ls.points);

                                if (polygon.isValid())
                                {
                                    push(Value::makePolygon(polygon));
                                }
                                else
                                {
                                    push(Value::makeNull());
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_ASTEXT))
                    {
                        // ST_AsText(geom) - convert geometry to WKT string
                        Value geom = pop();

                        if (geom.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                std::string wkt;

                                if (geom.type() == core::DataType::POINT)
                                {
                                    wkt = spatial::WKTParser::pointToWKT(geom.getPoint());
                                }
                                else if (geom.type() == core::DataType::LINESTRING)
                                {
                                    wkt = spatial::WKTParser::lineStringToWKT(geom.getLineString());
                                }
                                else if (geom.type() == core::DataType::POLYGON)
                                {
                                    wkt = spatial::WKTParser::polygonToWKT(geom.getPolygon());
                                }
                                else if (geom.type() == core::DataType::MULTIPOINT)
                                {
                                    // Generate MULTIPOINT WKT: MULTIPOINT((x1 y1), (x2 y2), ...)
                                    core::MultiPoint mp = geom.getMultiPoint();
                                    std::ostringstream oss;
                                    oss << "MULTIPOINT(";
                                    for (size_t i = 0; i < mp.points.size(); i++)
                                    {
                                        if (i > 0) oss << ", ";
                                        oss << "(" << mp.points[i].x << " " << mp.points[i].y << ")";
                                    }
                                    oss << ")";
                                    wkt = oss.str();
                                }
                                else if (geom.type() == core::DataType::MULTILINESTRING)
                                {
                                    // Generate MULTILINESTRING WKT
                                    core::MultiLineString mls = geom.getMultiLineString();
                                    std::ostringstream oss;
                                    oss << "MULTILINESTRING(";
                                    for (size_t i = 0; i < mls.linestrings.size(); i++)
                                    {
                                        if (i > 0) oss << ", ";
                                        oss << "(";
                                        const auto& ls = mls.linestrings[i];
                                        for (size_t j = 0; j < ls.points.size(); j++)
                                        {
                                            if (j > 0) oss << ", ";
                                            oss << ls.points[j].x << " " << ls.points[j].y;
                                        }
                                        oss << ")";
                                    }
                                    oss << ")";
                                    wkt = oss.str();
                                }
                                else if (geom.type() == core::DataType::MULTIPOLYGON)
                                {
                                    // Generate MULTIPOLYGON WKT
                                    core::MultiPolygon mpoly = geom.getMultiPolygon();
                                    std::ostringstream oss;
                                    oss << "MULTIPOLYGON(";
                                    for (size_t i = 0; i < mpoly.polygons.size(); i++)
                                    {
                                        if (i > 0) oss << ", ";
                                        oss << "(";
                                        const auto& poly = mpoly.polygons[i];
                                        for (size_t j = 0; j < poly.rings.size(); j++)
                                        {
                                            if (j > 0) oss << ", ";
                                            oss << "(";
                                            const auto& ring = poly.rings[j];
                                            for (size_t k = 0; k < ring.size(); k++)
                                            {
                                                if (k > 0) oss << ", ";
                                                oss << ring[k].x << " " << ring[k].y;
                                            }
                                            oss << ")";
                                        }
                                        oss << ")";
                                    }
                                    oss << ")";
                                    wkt = oss.str();
                                }
                                else if (geom.type() == core::DataType::GEOMETRYCOLLECTION)
                                {
                                    // Generate GEOMETRYCOLLECTION WKT
                                    core::GeometryCollection gc = geom.getGeometryCollection();
                                    std::ostringstream oss;
                                    oss << "GEOMETRYCOLLECTION(";
                                    for (size_t i = 0; i < gc.geometries.size(); i++)
                                    {
                                        if (i > 0) oss << ", ";

                                        // Recursively convert each geometry to WKT
                                        const auto& g = gc.geometries[i];
                                        if (g->type() == core::DataType::POINT)
                                        {
                                            oss << spatial::WKTParser::pointToWKT(g->getPoint());
                                        }
                                        else if (g->type() == core::DataType::LINESTRING)
                                        {
                                            oss << spatial::WKTParser::lineStringToWKT(g->getLineString());
                                        }
                                        else if (g->type() == core::DataType::POLYGON)
                                        {
                                            oss << spatial::WKTParser::polygonToWKT(g->getPolygon());
                                        }
                                        // TODO: Handle nested multi-geometry types if needed
                                    }
                                    oss << ")";
                                    wkt = oss.str();
                                }
                                else
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                push(Value::makeText(wkt));
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_ASBINARY))
                    {
                        // ST_AsBinary(geom) - convert geometry to WKB binary
                        Value geom = pop();

                        if (geom.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                std::vector<uint8_t> wkb;

                                if (geom.type() == core::DataType::POINT)
                                {
                                    wkb = spatial::WKBSerializer::serializePoint(geom.getPoint());
                                }
                                else if (geom.type() == core::DataType::LINESTRING)
                                {
                                    wkb = spatial::WKBSerializer::serializeLineString(geom.getLineString());
                                }
                                else if (geom.type() == core::DataType::POLYGON)
                                {
                                    wkb = spatial::WKBSerializer::serializePolygon(geom.getPolygon());
                                }
                                else if (geom.type() == core::DataType::MULTIPOINT)
                                {
                                    wkb = spatial::WKBSerializer::serializeMultiPoint(geom.getMultiPoint());
                                }
                                else if (geom.type() == core::DataType::MULTILINESTRING)
                                {
                                    wkb = spatial::WKBSerializer::serializeMultiLineString(geom.getMultiLineString());
                                }
                                else if (geom.type() == core::DataType::MULTIPOLYGON)
                                {
                                    wkb = spatial::WKBSerializer::serializeMultiPolygon(geom.getMultiPolygon());
                                }
                                else if (geom.type() == core::DataType::GEOMETRYCOLLECTION)
                                {
                                    wkb = spatial::WKBSerializer::serializeGeometryCollection(geom.getGeometryCollection());
                                }
                                else
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                // Store as binary string (or BYTEA if available)
                                std::string binary_str(reinterpret_cast<const char*>(wkb.data()), wkb.size());
                                push(Value::makeText(binary_str));
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_GEOMETRYTYPE))
                    {
                        // ST_GeometryType(geom) - get geometry type name
                        Value geom = pop();

                        if (geom.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string type_name;
                            if (geom.type() == core::DataType::POINT)
                            {
                                type_name = "POINT";
                            }
                            else if (geom.type() == core::DataType::LINESTRING)
                            {
                                type_name = "LINESTRING";
                            }
                            else if (geom.type() == core::DataType::POLYGON)
                            {
                                type_name = "POLYGON";
                            }
                            else if (geom.type() == core::DataType::MULTIPOINT)
                            {
                                type_name = "MULTIPOINT";
                            }
                            else if (geom.type() == core::DataType::MULTILINESTRING)
                            {
                                type_name = "MULTILINESTRING";
                            }
                            else if (geom.type() == core::DataType::MULTIPOLYGON)
                            {
                                type_name = "MULTIPOLYGON";
                            }
                            else if (geom.type() == core::DataType::GEOMETRYCOLLECTION)
                            {
                                type_name = "GEOMETRYCOLLECTION";
                            }
                            else
                            {
                                push(Value::makeNull());
                                break;
                            }

                            push(Value::makeText(type_name));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_ISVALID))
                    {
                        // ST_IsValid(geom) - validate geometry
                        Value geom = pop();

                        if (geom.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            bool is_valid = false;

                            if (geom.type() == core::DataType::POINT)
                            {
                                // Points are always valid
                                is_valid = true;
                            }
                            else if (geom.type() == core::DataType::LINESTRING)
                            {
                                is_valid = geom.getLineString().isValid();
                            }
                            else if (geom.type() == core::DataType::POLYGON)
                            {
                                is_valid = geom.getPolygon().isValid();
                            }
                            else
                            {
                                push(Value::makeNull());
                                break;
                            }

                            push(Value::makeBoolean(is_valid));
                        }
                    }
                    // ========== Phase 2 Task 9.3: Spatial Geometric Operations ==========
#ifdef HAVE_GEOS
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_BUFFER))
                    {
                        // ST_Buffer(geom, distance) - create buffer polygon around geometry
                        Value distance_val = pop();
                        Value geom_val = pop();

                        if (geom_val.isNull() || distance_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                double distance = distance_val.toDouble();
                                spatial::GEOSContext ctx;

                                // Convert ScratchBird geometry to GEOS
                                spatial::GEOSGeomPtr geos_geom(nullptr, ctx.handle());

                                if (geom_val.type() == core::DataType::POINT)
                                {
                                    geos_geom = spatial::pointToGEOS(geom_val.getPoint(), ctx);
                                }
                                else if (geom_val.type() == core::DataType::LINESTRING)
                                {
                                    geos_geom = spatial::lineStringToGEOS(geom_val.getLineString(), ctx);
                                }
                                else if (geom_val.type() == core::DataType::POLYGON)
                                {
                                    geos_geom = spatial::polygonToGEOS(geom_val.getPolygon(), ctx);
                                }
                                else
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                if (!geos_geom)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                // Create buffer (8 segments per quadrant for smoothness)
                                GEOSGeometry* buffered = GEOSBuffer_r(ctx.handle(), geos_geom.get(), distance, 8);
                                if (!buffered)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                spatial::GEOSGeomPtr buffered_ptr(buffered, ctx.handle());

                                // Convert back to ScratchBird geometry
                                auto result = spatial::geosToTypedValue(buffered_ptr.get(), ctx);
                                if (result)
                                {
                                    push(*result);
                                }
                                else
                                {
                                    push(Value::makeNull());
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_CONVEXHULL))
                    {
                        // ST_ConvexHull(geom) - compute convex hull of geometry
                        Value geom_val = pop();

                        if (geom_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                // Convert ScratchBird geometry to GEOS
                                spatial::GEOSGeomPtr geos_geom(nullptr, ctx.handle());

                                if (geom_val.type() == core::DataType::POINT)
                                {
                                    geos_geom = spatial::pointToGEOS(geom_val.getPoint(), ctx);
                                }
                                else if (geom_val.type() == core::DataType::LINESTRING)
                                {
                                    geos_geom = spatial::lineStringToGEOS(geom_val.getLineString(), ctx);
                                }
                                else if (geom_val.type() == core::DataType::POLYGON)
                                {
                                    geos_geom = spatial::polygonToGEOS(geom_val.getPolygon(), ctx);
                                }
                                else
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                if (!geos_geom)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                // Compute convex hull
                                GEOSGeometry* hull = GEOSConvexHull_r(ctx.handle(), geos_geom.get());
                                if (!hull)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                spatial::GEOSGeomPtr hull_ptr(hull, ctx.handle());

                                // Convert back to ScratchBird geometry
                                auto result = spatial::geosToTypedValue(hull_ptr.get(), ctx);
                                if (result)
                                {
                                    push(*result);
                                }
                                else
                                {
                                    push(Value::makeNull());
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_ENVELOPE))
                    {
                        // ST_Envelope(geom) - compute minimum bounding box (envelope) of geometry
                        Value geom_val = pop();

                        if (geom_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                // Convert ScratchBird geometry to GEOS
                                spatial::GEOSGeomPtr geos_geom(nullptr, ctx.handle());

                                if (geom_val.type() == core::DataType::POINT)
                                {
                                    geos_geom = spatial::pointToGEOS(geom_val.getPoint(), ctx);
                                }
                                else if (geom_val.type() == core::DataType::LINESTRING)
                                {
                                    geos_geom = spatial::lineStringToGEOS(geom_val.getLineString(), ctx);
                                }
                                else if (geom_val.type() == core::DataType::POLYGON)
                                {
                                    geos_geom = spatial::polygonToGEOS(geom_val.getPolygon(), ctx);
                                }
                                else
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                if (!geos_geom)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                // Compute envelope (bounding box)
                                GEOSGeometry* envelope = GEOSEnvelope_r(ctx.handle(), geos_geom.get());
                                if (!envelope)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                spatial::GEOSGeomPtr envelope_ptr(envelope, ctx.handle());

                                // Convert back to ScratchBird geometry
                                auto result = spatial::geosToTypedValue(envelope_ptr.get(), ctx);
                                if (result)
                                {
                                    push(*result);
                                }
                                else
                                {
                                    push(Value::makeNull());
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    // ========== Phase 2 Task 9.3: Spatial Predicates (G2/G4) ==========
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_INTERSECTS))
                    {
                        // ST_Intersects(geom1, geom2) - do geometries intersect?
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                // Convert both geometries to GEOS
                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    char result = GEOSIntersects_r(ctx.handle(), g1.get(), g2.get());
                                    if (result == 2) // Error
                                    {
                                        push(Value::makeNull());
                                    }
                                    else
                                    {
                                        push(Value::makeBoolean(result == 1));
                                    }
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_CONTAINS))
                    {
                        // ST_Contains(geom1, geom2) - does geom1 contain geom2?
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    char result = GEOSContains_r(ctx.handle(), g1.get(), g2.get());
                                    if (result == 2)
                                    {
                                        push(Value::makeNull());
                                    }
                                    else
                                    {
                                        push(Value::makeBoolean(result == 1));
                                    }
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_WITHIN))
                    {
                        // ST_Within(geom1, geom2) - is geom1 within geom2?
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    char result = GEOSWithin_r(ctx.handle(), g1.get(), g2.get());
                                    if (result == 2)
                                    {
                                        push(Value::makeNull());
                                    }
                                    else
                                    {
                                        push(Value::makeBoolean(result == 1));
                                    }
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_EQUALS))
                    {
                        // ST_Equals(geom1, geom2) - are geometries spatially equal?
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    char result = GEOSEquals_r(ctx.handle(), g1.get(), g2.get());
                                    if (result == 2) // Error
                                    {
                                        push(Value::makeNull());
                                    }
                                    else
                                    {
                                        push(Value::makeBoolean(result == 1));
                                    }
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_DISJOINT))
                    {
                        // ST_Disjoint(geom1, geom2) - are geometries disjoint?
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    char result = GEOSDisjoint_r(ctx.handle(), g1.get(), g2.get());
                                    if (result == 2)
                                    {
                                        push(Value::makeNull());
                                    }
                                    else
                                    {
                                        push(Value::makeBoolean(result == 1));
                                    }
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_OVERLAPS))
                    {
                        // ST_Overlaps(geom1, geom2) - do geometries overlap?
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    char result = GEOSOverlaps_r(ctx.handle(), g1.get(), g2.get());
                                    if (result == 2)
                                    {
                                        push(Value::makeNull());
                                    }
                                    else
                                    {
                                        push(Value::makeBoolean(result == 1));
                                    }
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_TOUCHES))
                    {
                        // ST_Touches(geom1, geom2) - do geometries touch?
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    char result = GEOSTouches_r(ctx.handle(), g1.get(), g2.get());
                                    if (result == 2)
                                    {
                                        push(Value::makeNull());
                                    }
                                    else
                                    {
                                        push(Value::makeBoolean(result == 1));
                                    }
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_CROSSES))
                    {
                        // ST_Crosses(geom1, geom2) - do geometries cross?
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    char result = GEOSCrosses_r(ctx.handle(), g1.get(), g2.get());
                                    if (result == 2)
                                    {
                                        push(Value::makeNull());
                                    }
                                    else
                                    {
                                        push(Value::makeBoolean(result == 1));
                                    }
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    // ========== Phase 2 Task 9.3: Spatial Processing Functions (G4) ==========
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_INTERSECTION))
                    {
                        // ST_Intersection(geom1, geom2) - compute intersection geometry
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                GEOSGeometry* result_geom = GEOSIntersection_r(ctx.handle(), g1.get(), g2.get());
                                if (!result_geom)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                spatial::GEOSGeomPtr result_ptr(result_geom, ctx.handle());

                                // Check if result is empty
                                if (GEOSisEmpty_r(ctx.handle(), result_ptr.get()) == 1)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    auto result = spatial::geosToTypedValue(result_ptr.get(), ctx);
                                    if (result)
                                    {
                                        push(*result);
                                    }
                                    else
                                    {
                                        push(Value::makeNull());
                                    }
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_UNION))
                    {
                        // ST_Union(geom1, geom2) - compute union geometry
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                GEOSGeometry* result_geom = GEOSUnion_r(ctx.handle(), g1.get(), g2.get());
                                if (!result_geom)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                spatial::GEOSGeomPtr result_ptr(result_geom, ctx.handle());

                                auto result = spatial::geosToTypedValue(result_ptr.get(), ctx);
                                if (result)
                                {
                                    push(*result);
                                }
                                else
                                {
                                    push(Value::makeNull());
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_DIFFERENCE))
                    {
                        // ST_Difference(geom1, geom2) - compute difference (geom1 - geom2)
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                GEOSGeometry* result_geom = GEOSDifference_r(ctx.handle(), g1.get(), g2.get());
                                if (!result_geom)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                spatial::GEOSGeomPtr result_ptr(result_geom, ctx.handle());

                                // Check if result is empty
                                if (GEOSisEmpty_r(ctx.handle(), result_ptr.get()) == 1)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    auto result = spatial::geosToTypedValue(result_ptr.get(), ctx);
                                    if (result)
                                    {
                                        push(*result);
                                    }
                                    else
                                    {
                                        push(Value::makeNull());
                                    }
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    // ========== Phase 2 Task 9.3: Spatial Metrics (G4) ==========
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_AREA))
                    {
                        // ST_Area(geom) - compute area of polygon
                        Value geom_val = pop();

                        if (geom_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g(nullptr, ctx.handle());

                                if (geom_val.type() == core::DataType::POINT)
                                    g = spatial::pointToGEOS(geom_val.getPoint(), ctx);
                                else if (geom_val.type() == core::DataType::LINESTRING)
                                    g = spatial::lineStringToGEOS(geom_val.getLineString(), ctx);
                                else if (geom_val.type() == core::DataType::POLYGON)
                                    g = spatial::polygonToGEOS(geom_val.getPolygon(), ctx);

                                if (!g)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                double area;
                                if (GEOSArea_r(ctx.handle(), g.get(), &area) == 0)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    push(Value::makeFloat64(area));
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_LENGTH))
                    {
                        // ST_Length(geom) - compute length of linestring
                        Value geom_val = pop();

                        if (geom_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g(nullptr, ctx.handle());

                                if (geom_val.type() == core::DataType::POINT)
                                    g = spatial::pointToGEOS(geom_val.getPoint(), ctx);
                                else if (geom_val.type() == core::DataType::LINESTRING)
                                    g = spatial::lineStringToGEOS(geom_val.getLineString(), ctx);
                                else if (geom_val.type() == core::DataType::POLYGON)
                                    g = spatial::polygonToGEOS(geom_val.getPolygon(), ctx);

                                if (!g)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                double length;
                                if (GEOSLength_r(ctx.handle(), g.get(), &length) == 0)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    push(Value::makeFloat64(length));
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_DISTANCE))
                    {
                        // ST_Distance(geom1, geom2) - compute distance between geometries
                        Value geom2_val = pop();
                        Value geom1_val = pop();

                        if (geom1_val.isNull() || geom2_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g1(nullptr, ctx.handle());
                                spatial::GEOSGeomPtr g2(nullptr, ctx.handle());

                                if (geom1_val.type() == core::DataType::POINT)
                                    g1 = spatial::pointToGEOS(geom1_val.getPoint(), ctx);
                                else if (geom1_val.type() == core::DataType::LINESTRING)
                                    g1 = spatial::lineStringToGEOS(geom1_val.getLineString(), ctx);
                                else if (geom1_val.type() == core::DataType::POLYGON)
                                    g1 = spatial::polygonToGEOS(geom1_val.getPolygon(), ctx);

                                if (geom2_val.type() == core::DataType::POINT)
                                    g2 = spatial::pointToGEOS(geom2_val.getPoint(), ctx);
                                else if (geom2_val.type() == core::DataType::LINESTRING)
                                    g2 = spatial::lineStringToGEOS(geom2_val.getLineString(), ctx);
                                else if (geom2_val.type() == core::DataType::POLYGON)
                                    g2 = spatial::polygonToGEOS(geom2_val.getPolygon(), ctx);

                                if (!g1 || !g2)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                double distance;
                                if (GEOSDistance_r(ctx.handle(), g1.get(), g2.get(), &distance) == 0)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    push(Value::makeFloat64(distance));
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_PERIMETER))
                    {
                        // ST_Perimeter(geom) - compute perimeter of polygon
                        Value geom_val = pop();

                        if (geom_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                spatial::GEOSContext ctx;

                                spatial::GEOSGeomPtr g(nullptr, ctx.handle());

                                if (geom_val.type() == core::DataType::POINT)
                                    g = spatial::pointToGEOS(geom_val.getPoint(), ctx);
                                else if (geom_val.type() == core::DataType::LINESTRING)
                                    g = spatial::lineStringToGEOS(geom_val.getLineString(), ctx);
                                else if (geom_val.type() == core::DataType::POLYGON)
                                    g = spatial::polygonToGEOS(geom_val.getPolygon(), ctx);

                                if (!g)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                // For GEOS, perimeter is computed using GEOSLength on the boundary
                                GEOSGeometry* boundary = GEOSBoundary_r(ctx.handle(), g.get());
                                if (!boundary)
                                {
                                    push(Value::makeNull());
                                    break;
                                }

                                spatial::GEOSGeomPtr boundary_ptr(boundary, ctx.handle());

                                double perimeter;
                                if (GEOSLength_r(ctx.handle(), boundary_ptr.get(), &perimeter) == 0)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    push(Value::makeFloat64(perimeter));
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
#endif // HAVE_GEOS
                    // ========== Multi-Geometry Constructor Functions (OGC Simple Features) ==========
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_MULTIPOINT))
                    {
                        // ST_MultiPoint(point1, point2, ...) - create MULTIPOINT from points
                        uint8_t arg_count = readByte();

                        if (arg_count < 1)
                        {
                            error("ST_MultiPoint expects at least 1 point");
                            push(Value::makeNull());
                        }
                        else
                        {
                            // Pop all points from stack (in reverse order)
                            std::vector<Value> point_values;
                            for (uint8_t i = 0; i < arg_count; i++)
                            {
                                point_values.push_back(pop());
                            }

                            // Reverse to get correct order
                            std::reverse(point_values.begin(), point_values.end());

                            // Check for nulls and validate types
                            bool has_error = false;
                            std::vector<core::Point> points;

                            for (const auto& pv : point_values)
                            {
                                if (pv.isNull() || pv.type() != core::DataType::POINT)
                                {
                                    has_error = true;
                                    break;
                                }
                                points.push_back(pv.getPoint());
                            }

                            if (has_error)
                            {
                                push(Value::makeNull());
                            }
                            else
                            {
                                try {
                                    core::MultiPoint multipoint(points);
                                    if (multipoint.isValid())
                                    {
                                        push(Value::makeMultiPoint(multipoint));
                                    }
                                    else
                                    {
                                        push(Value::makeNull());
                                    }
                                } catch (...) {
                                    push(Value::makeNull());
                                }
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_MULTILINESTRING))
                    {
                        // ST_MultiLineString(linestring1, linestring2, ...) - create MULTILINESTRING
                        uint8_t arg_count = readByte();

                        if (arg_count < 1)
                        {
                            error("ST_MultiLineString expects at least 1 linestring");
                            push(Value::makeNull());
                        }
                        else
                        {
                            // Pop all linestrings from stack (in reverse order)
                            std::vector<Value> ls_values;
                            for (uint8_t i = 0; i < arg_count; i++)
                            {
                                ls_values.push_back(pop());
                            }

                            // Reverse to get correct order
                            std::reverse(ls_values.begin(), ls_values.end());

                            // Check for nulls and validate types
                            bool has_error = false;
                            std::vector<core::LineString> linestrings;

                            for (const auto& lv : ls_values)
                            {
                                if (lv.isNull() || lv.type() != core::DataType::LINESTRING)
                                {
                                    has_error = true;
                                    break;
                                }
                                linestrings.push_back(lv.getLineString());
                            }

                            if (has_error)
                            {
                                push(Value::makeNull());
                            }
                            else
                            {
                                try {
                                    core::MultiLineString mls(linestrings);
                                    if (mls.isValid())
                                    {
                                        push(Value::makeMultiLineString(mls));
                                    }
                                    else
                                    {
                                        push(Value::makeNull());
                                    }
                                } catch (...) {
                                    push(Value::makeNull());
                                }
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_MULTIPOLYGON))
                    {
                        // ST_MultiPolygon(polygon1, polygon2, ...) - create MULTIPOLYGON
                        uint8_t arg_count = readByte();

                        if (arg_count < 1)
                        {
                            error("ST_MultiPolygon expects at least 1 polygon");
                            push(Value::makeNull());
                        }
                        else
                        {
                            // Pop all polygons from stack (in reverse order)
                            std::vector<Value> poly_values;
                            for (uint8_t i = 0; i < arg_count; i++)
                            {
                                poly_values.push_back(pop());
                            }

                            // Reverse to get correct order
                            std::reverse(poly_values.begin(), poly_values.end());

                            // Check for nulls and validate types
                            bool has_error = false;
                            std::vector<core::Polygon> polygons;

                            for (const auto& pv : poly_values)
                            {
                                if (pv.isNull() || pv.type() != core::DataType::POLYGON)
                                {
                                    has_error = true;
                                    break;
                                }
                                polygons.push_back(pv.getPolygon());
                            }

                            if (has_error)
                            {
                                push(Value::makeNull());
                            }
                            else
                            {
                                try {
                                    core::MultiPolygon mpoly(polygons);
                                    if (mpoly.isValid())
                                    {
                                        push(Value::makeMultiPolygon(mpoly));
                                    }
                                    else
                                    {
                                        push(Value::makeNull());
                                    }
                                } catch (...) {
                                    push(Value::makeNull());
                                }
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_GEOMETRYCOLLECTION) ||
                             ext_op == static_cast<uint8_t>(Opcode::EXT_ST_COLLECT))
                    {
                        // ST_GeometryCollection / ST_Collect - create heterogeneous collection
                        uint8_t arg_count = readByte();

                        if (arg_count < 1)
                        {
                            error("ST_GeometryCollection/ST_Collect expects at least 1 geometry");
                            push(Value::makeNull());
                        }
                        else
                        {
                            // Pop all geometries from stack (in reverse order)
                            std::vector<Value> geom_values;
                            for (uint8_t i = 0; i < arg_count; i++)
                            {
                                geom_values.push_back(pop());
                            }

                            // Reverse to get correct order
                            std::reverse(geom_values.begin(), geom_values.end());

                            // Create vector of shared pointers to TypedValue
                            std::vector<std::shared_ptr<core::TypedValue>> geometries;

                            for (const auto& gv : geom_values)
                            {
                                if (gv.isNull())
                                {
                                    // Skip nulls in collection
                                    continue;
                                }

                                // Check if it's a geometry type
                                core::DataType dt = gv.type();
                                if (dt == core::DataType::POINT ||
                                    dt == core::DataType::LINESTRING ||
                                    dt == core::DataType::POLYGON ||
                                    dt == core::DataType::MULTIPOINT ||
                                    dt == core::DataType::MULTILINESTRING ||
                                    dt == core::DataType::MULTIPOLYGON ||
                                    dt == core::DataType::GEOMETRYCOLLECTION)
                                {
                                    geometries.push_back(std::make_shared<core::TypedValue>(gv));
                                }
                            }

                            if (geometries.empty())
                            {
                                push(Value::makeNull());
                            }
                            else
                            {
                                try {
                                    core::GeometryCollection gc;
                                    gc.geometries = geometries;

                                    if (gc.isValid())
                                    {
                                        push(Value::makeGeometryCollection(gc));
                                    }
                                    else
                                    {
                                        push(Value::makeNull());
                                    }
                                } catch (...) {
                                    push(Value::makeNull());
                                }
                            }
                        }
                    }
                    // ========== Multi-Geometry Accessor Functions ==========
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_NUMGEOMETRIES))
                    {
                        // ST_NumGeometries(multi_geom) - get count of geometries in collection
                        Value geom_val = pop();

                        if (geom_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                size_t count = 0;

                                switch (geom_val.type())
                                {
                                    case core::DataType::MULTIPOINT:
                                        count = geom_val.getMultiPoint().numGeometries();
                                        break;
                                    case core::DataType::MULTILINESTRING:
                                        count = geom_val.getMultiLineString().numGeometries();
                                        break;
                                    case core::DataType::MULTIPOLYGON:
                                        count = geom_val.getMultiPolygon().numGeometries();
                                        break;
                                    case core::DataType::GEOMETRYCOLLECTION:
                                        count = geom_val.getGeometryCollection().numGeometries();
                                        break;
                                    default:
                                        // For simple geometries, return 1
                                        count = 1;
                                        break;
                                }

                                push(Value::makeInt32(static_cast<int32_t>(count)));
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ST_GEOMETRYN))
                    {
                        // ST_GeometryN(multi_geom, n) - get Nth geometry (1-indexed)
                        Value index_val = pop();
                        Value geom_val = pop();

                        if (geom_val.isNull() || index_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            try {
                                int32_t index = index_val.getInt32();

                                // SQL uses 1-based indexing
                                if (index < 1)
                                {
                                    push(Value::makeNull());
                                }
                                else
                                {
                                    size_t idx = static_cast<size_t>(index - 1);

                                    switch (geom_val.type())
                                    {
                                        case core::DataType::MULTIPOINT:
                                        {
                                            core::MultiPoint mp = geom_val.getMultiPoint();
                                            if (idx < mp.points.size())
                                            {
                                                push(Value::makePoint(mp.points[idx]));
                                            }
                                            else
                                            {
                                                push(Value::makeNull());
                                            }
                                            break;
                                        }
                                        case core::DataType::MULTILINESTRING:
                                        {
                                            core::MultiLineString mls = geom_val.getMultiLineString();
                                            if (idx < mls.linestrings.size())
                                            {
                                                push(Value::makeLineString(mls.linestrings[idx]));
                                            }
                                            else
                                            {
                                                push(Value::makeNull());
                                            }
                                            break;
                                        }
                                        case core::DataType::MULTIPOLYGON:
                                        {
                                            core::MultiPolygon mpoly = geom_val.getMultiPolygon();
                                            if (idx < mpoly.polygons.size())
                                            {
                                                push(Value::makePolygon(mpoly.polygons[idx]));
                                            }
                                            else
                                            {
                                                push(Value::makeNull());
                                            }
                                            break;
                                        }
                                        case core::DataType::GEOMETRYCOLLECTION:
                                        {
                                            core::GeometryCollection gc = geom_val.getGeometryCollection();
                                            if (idx < gc.geometries.size())
                                            {
                                                push(*gc.geometries[idx]);
                                            }
                                            else
                                            {
                                                push(Value::makeNull());
                                            }
                                            break;
                                        }
                                        default:
                                            // For simple geometries, only index 1 is valid
                                            if (index == 1)
                                            {
                                                push(geom_val);
                                            }
                                            else
                                            {
                                                push(Value::makeNull());
                                            }
                                            break;
                                    }
                                }
                            } catch (...) {
                                push(Value::makeNull());
                            }
                        }
                    }
                    // ========== Phase 2 Task 13: Text Search and Regex Operators ==========
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REGEX_MATCH))
                    {
                        // ~ operator (regex match case-sensitive)
                        Value pattern = pop();
                        Value text = pop();

                        if (text.isNull() || pattern.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            bool matches = matchRegex(text.toString(), pattern.toString(), false);
                            push(Value::makeBoolean(matches));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REGEX_MATCH_CI))
                    {
                        // ~* operator (regex match case-insensitive)
                        Value pattern = pop();
                        Value text = pop();

                        if (text.isNull() || pattern.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            bool matches = matchRegex(text.toString(), pattern.toString(), true);
                            push(Value::makeBoolean(matches));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REGEX_NOT_MATCH))
                    {
                        // !~ operator (regex not match case-sensitive)
                        Value pattern = pop();
                        Value text = pop();

                        if (text.isNull() || pattern.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            bool matches = matchRegex(text.toString(), pattern.toString(), false);
                            push(Value::makeBoolean(!matches));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REGEX_NOT_MATCH_CI))
                    {
                        // !~* operator (regex not match case-insensitive)
                        Value pattern = pop();
                        Value text = pop();

                        if (text.isNull() || pattern.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            bool matches = matchRegex(text.toString(), pattern.toString(), true);
                            push(Value::makeBoolean(!matches));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REGEXP_MATCHES))
                    {
                        // REGEXP_MATCHES(str, pattern [, flags])
                        uint8_t arg_count = readByte();

                        Value flags_val;
                        std::string flags_str = "";
                        if (arg_count == 3)
                        {
                            flags_val = pop();
                            if (!flags_val.isNull())
                            {
                                flags_str = flags_val.toString();
                            }
                        }

                        Value pattern = pop();
                        Value text = pop();

                        if (text.isNull() || pattern.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            auto matches = regexMatches(text.toString(), pattern.toString(), flags_str);

                            // Return as JSON array
                            json arr = json::array();
                            for (const auto& match : matches)
                            {
                                arr.push_back(match);
                            }
                            push(Value::makeJSON(arr.dump()));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REGEXP_REPLACE))
                    {
                        // REGEXP_REPLACE(str, pattern, replacement [, flags])
                        uint8_t arg_count = readByte();

                        Value flags_val;
                        std::string flags_str = "";
                        if (arg_count == 4)
                        {
                            flags_val = pop();
                            if (!flags_val.isNull())
                            {
                                flags_str = flags_val.toString();
                            }
                        }

                        Value replacement = pop();
                        Value pattern = pop();
                        Value text = pop();

                        if (text.isNull() || pattern.isNull() || replacement.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string result = regexReplace(text.toString(), pattern.toString(),
                                                               replacement.toString(), flags_str);
                            push(Value::makeText(result));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REGEXP_SPLIT_TO_ARRAY))
                    {
                        // REGEXP_SPLIT_TO_ARRAY(str, pattern [, flags])
                        uint8_t arg_count = readByte();

                        Value flags_val;
                        std::string flags_str = "";
                        if (arg_count == 3)
                        {
                            flags_val = pop();
                            if (!flags_val.isNull())
                            {
                                flags_str = flags_val.toString();
                            }
                        }

                        Value pattern = pop();
                        Value text = pop();

                        if (text.isNull() || pattern.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            auto parts = regexSplit(text.toString(), pattern.toString(), flags_str);

                            // Return as JSON array
                            json arr = json::array();
                            for (const auto& part : parts)
                            {
                                arr.push_back(part);
                            }
                            push(Value::makeJSON(arr.dump()));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REGEXP_SPLIT_TO_TABLE))
                    {
                        // REGEXP_SPLIT_TO_TABLE(str, pattern [, flags]) - table-returning function
                        // For now, returns same as REGEXP_SPLIT_TO_ARRAY (array)
                        // Parser/executor will handle table expansion
                        uint8_t arg_count = readByte();

                        Value flags_val;
                        std::string flags_str = "";
                        if (arg_count == 3)
                        {
                            flags_val = pop();
                            if (!flags_val.isNull())
                            {
                                flags_str = flags_val.toString();
                            }
                        }

                        Value pattern = pop();
                        Value text = pop();

                        if (text.isNull() || pattern.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            auto parts = regexSplit(text.toString(), pattern.toString(), flags_str);

                            // Return as JSON array
                            json arr = json::array();
                            for (const auto& part : parts)
                            {
                                arr.push_back(part);
                            }
                            push(Value::makeJSON(arr.dump()));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_STRING_TO_TABLE))
                    {
                        // STRING_TO_TABLE(str, delimiter) - table-returning function
                        // For now, returns array (parser will handle table expansion)
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("STRING_TO_TABLE expects 2 arguments");
                        }

                        Value delimiter = pop();
                        Value text = pop();

                        if (text.isNull() || delimiter.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string str = text.toString();
                            std::string delim = delimiter.toString();

                            // Split string by delimiter
                            std::vector<std::string> parts;
                            size_t start = 0;
                            size_t pos = str.find(delim);

                            while (pos != std::string::npos)
                            {
                                parts.push_back(str.substr(start, pos - start));
                                start = pos + delim.length();
                                pos = str.find(delim, start);
                            }
                            parts.push_back(str.substr(start));

                            // Return as JSON array
                            json arr = json::array();
                            for (const auto& part : parts)
                            {
                                arr.push_back(part);
                            }
                            push(Value::makeJSON(arr.dump()));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_UNNEST_TEXT))
                    {
                        // UNNEST_TEXT(array) - table-returning function
                        // Takes a text array and returns as table rows
                        // For now, just passes through the array
                        Value array = pop();

                        if (array.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            // If already an array (JSON), pass through
                            // If a simple value, wrap in array
                            try {
                                json j_array = json::parse(array.toString());
                                if (j_array.is_array())
                                {
                                    push(Value::makeJSON(j_array.dump()));
                                }
                                else
                                {
                                    json arr = json::array();
                                    arr.push_back(j_array);
                                    push(Value::makeJSON(arr.dump()));
                                }
                            } catch (...) {
                                // If not JSON, wrap in array
                                json arr = json::array();
                                arr.push_back(array.toString());
                                push(Value::makeJSON(arr.dump()));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SPLIT_PART))
                    {
                        // SPLIT_PART(str, delimiter, field)
                        uint8_t arg_count = readByte();
                        if (arg_count != 3)
                        {
                            error("SPLIT_PART expects 3 arguments");
                        }

                        Value field_val = pop();
                        Value delimiter = pop();
                        Value text = pop();

                        if (text.isNull() || delimiter.isNull() || field_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string str = text.toString();
                            std::string delim = delimiter.toString();
                            int32_t field = static_cast<int32_t>(field_val.toInt64());

                            if (field < 1)
                            {
                                push(Value::makeText(""));
                            }
                            else
                            {
                                // Split string by delimiter
                                std::vector<std::string> parts;
                                size_t start = 0;
                                size_t pos = str.find(delim);

                                while (pos != std::string::npos)
                                {
                                    parts.push_back(str.substr(start, pos - start));
                                    start = pos + delim.length();
                                    pos = str.find(delim, start);
                                }
                                parts.push_back(str.substr(start));

                                // Return the requested field (1-indexed)
                                if (field <= static_cast<int32_t>(parts.size()))
                                {
                                    push(Value::makeText(parts[field - 1]));
                                }
                                else
                                {
                                    push(Value::makeText(""));
                                }
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_STRPOS))
                    {
                        // STRPOS(str, substring)
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("STRPOS expects 2 arguments");
                        }

                        Value substring = pop();
                        Value text = pop();

                        if (text.isNull() || substring.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string str = text.toString();
                            std::string substr = substring.toString();

                            size_t pos = str.find(substr);
                            if (pos != std::string::npos)
                            {
                                push(Value::makeInt32(static_cast<int32_t>(pos + 1))); // 1-indexed
                            }
                            else
                            {
                                push(Value::makeInt32(0));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_POSITION))
                    {
                        // POSITION(substring IN string)
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("POSITION expects 2 arguments");
                        }

                        Value text = pop();
                        Value substring = pop();

                        if (text.isNull() || substring.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string str = text.toString();
                            std::string substr = substring.toString();

                            size_t pos = str.find(substr);
                            if (pos != std::string::npos)
                            {
                                push(Value::makeInt32(static_cast<int32_t>(pos + 1))); // 1-indexed
                            }
                            else
                            {
                                push(Value::makeInt32(0));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_OVERLAY))
                    {
                        // OVERLAY(str PLACING newstr FROM start [FOR length])
                        uint8_t arg_count = readByte();

                        Value length_val;
                        bool has_length = (arg_count == 4);
                        if (has_length)
                        {
                            length_val = pop();
                        }

                        Value start_val = pop();
                        Value newstr = pop();
                        Value text = pop();

                        if (text.isNull() || newstr.isNull() || start_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string str = text.toString();
                            std::string new_str = newstr.toString();
                            int32_t start = static_cast<int32_t>(start_val.toInt64());
                            int32_t length = has_length && !length_val.isNull()
                                ? static_cast<int32_t>(length_val.toInt64())
                                : static_cast<int32_t>(new_str.length());

                            if (start < 1 || start > static_cast<int32_t>(str.length()) + 1)
                            {
                                push(Value::makeText(str));
                            }
                            else
                            {
                                // Replace substring
                                std::string result = str.substr(0, start - 1) + new_str;
                                if (start - 1 + length < static_cast<int32_t>(str.length()))
                                {
                                    result += str.substr(start - 1 + length);
                                }
                                push(Value::makeText(result));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_QUOTE_LITERAL))
                    {
                        // QUOTE_LITERAL(str) - escape and quote a string literal
                        Value text = pop();

                        if (text.isNull())
                        {
                            push(Value::makeText("NULL"));
                        }
                        else
                        {
                            std::string str = text.toString();
                            std::string result = "'";

                            // Escape single quotes by doubling them
                            for (char c : str)
                            {
                                if (c == '\'')
                                {
                                    result += "''";
                                }
                                else
                                {
                                    result += c;
                                }
                            }

                            result += "'";
                            push(Value::makeText(result));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_QUOTE_IDENT))
                    {
                        // QUOTE_IDENT(str) - escape and quote an identifier
                        Value text = pop();

                        if (text.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string str = text.toString();
                            std::string result = "\"";

                            // Escape double quotes by doubling them
                            for (char c : str)
                            {
                                if (c == '"')
                                {
                                    result += "\"\"";
                                }
                                else
                                {
                                    result += c;
                                }
                            }

                            result += "\"";
                            push(Value::makeText(result));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_INITCAP))
                    {
                        // INITCAP(str) - capitalize first letter of each word
                        Value text = pop();

                        if (text.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string str = text.toString();
                            bool capitalize_next = true;

                            for (char& c : str)
                            {
                                if (std::isspace(static_cast<unsigned char>(c)) || !std::isalnum(static_cast<unsigned char>(c)))
                                {
                                    capitalize_next = true;
                                }
                                else if (capitalize_next)
                                {
                                    c = std::toupper(static_cast<unsigned char>(c));
                                    capitalize_next = false;
                                }
                                else
                                {
                                    c = std::tolower(static_cast<unsigned char>(c));
                                }
                            }

                            push(Value::makeText(str));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ASCII))
                    {
                        // ASCII(str) - get ASCII code of first character
                        Value text = pop();

                        if (text.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string str = text.toString();
                            if (str.empty())
                            {
                                push(Value::makeInt32(0));
                            }
                            else
                            {
                                push(Value::makeInt32(static_cast<int32_t>(static_cast<unsigned char>(str[0]))));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_CHR))
                    {
                        // CHR(code) - convert ASCII code to character
                        Value code_val = pop();

                        if (code_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            int32_t code = static_cast<int32_t>(code_val.toInt64());
                            if (code < 0 || code > 255)
                            {
                                error("CHR value out of range (0-255)");
                            }
                            else
                            {
                                std::string result(1, static_cast<char>(code));
                                push(Value::makeText(result));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REPEAT))
                    {
                        // REPEAT(str, count)
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("REPEAT expects 2 arguments");
                        }

                        Value count_val = pop();
                        Value text = pop();

                        if (text.isNull() || count_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string str = text.toString();
                            int32_t count = static_cast<int32_t>(count_val.toInt64());

                            if (count < 0)
                            {
                                push(Value::makeText(""));
                            }
                            else
                            {
                                std::string result;
                                result.reserve(str.length() * count);
                                for (int32_t i = 0; i < count; i++)
                                {
                                    result += str;
                                }
                                push(Value::makeText(result));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_REVERSE))
                    {
                        // REVERSE(str)
                        Value text = pop();

                        if (text.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string str = text.toString();
                            std::reverse(str.begin(), str.end());
                            push(Value::makeText(str));
                        }
                    }
                    // ========== Text Search Functions (Task 14 Phase 5) ==========
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_TO_TSVECTOR))
                    {
                        // TO_TSVECTOR(config, text) or TO_TSVECTOR(text)
                        // 2 args: config name, text
                        // 1 arg: text (uses 'simple' config)
                        uint8_t arg_count = readByte();

                        std::string config_name;
                        Value text_val;

                        if (arg_count == 2)
                        {
                            text_val = pop();
                            Value config_val = pop();
                            config_name = config_val.toString();
                        }
                        else if (arg_count == 1)
                        {
                            text_val = pop();
                            config_name = "simple";
                        }
                        else
                        {
                            error("TO_TSVECTOR expects 1 or 2 arguments, got " + std::to_string(arg_count));
                        }

                        if (text_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string text = text_val.toString();
                            auto result = core::to_tsvector(config_name, text);

                            if (result.has_value())
                            {
                                push(Value::makeTSVector(*result));
                            }
                            else
                            {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_TO_TSQUERY))
                    {
                        // TO_TSQUERY(config, query) or TO_TSQUERY(query)
                        uint8_t arg_count = readByte();

                        std::string config_name;
                        Value query_val;

                        if (arg_count == 2)
                        {
                            query_val = pop();
                            Value config_val = pop();
                            config_name = config_val.toString();
                        }
                        else if (arg_count == 1)
                        {
                            query_val = pop();
                            config_name = "simple";
                        }
                        else
                        {
                            error("TO_TSQUERY expects 1 or 2 arguments, got " + std::to_string(arg_count));
                        }

                        if (query_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string query_text = query_val.toString();
                            auto result = core::to_tsquery(config_name, query_text);

                            if (result.has_value())
                            {
                                push(Value::makeTSQuery(*result));
                            }
                            else
                            {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_PLAINTO_TSQUERY))
                    {
                        // PLAINTO_TSQUERY(config, text) or PLAINTO_TSQUERY(text)
                        uint8_t arg_count = readByte();

                        std::string config_name;
                        Value text_val;

                        if (arg_count == 2)
                        {
                            text_val = pop();
                            Value config_val = pop();
                            config_name = config_val.toString();
                        }
                        else if (arg_count == 1)
                        {
                            text_val = pop();
                            config_name = "simple";
                        }
                        else
                        {
                            error("PLAINTO_TSQUERY expects 1 or 2 arguments, got " + std::to_string(arg_count));
                        }

                        if (text_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string text = text_val.toString();
                            auto result = core::plainto_tsquery(config_name, text);

                            if (result.has_value())
                            {
                                push(Value::makeTSQuery(*result));
                            }
                            else
                            {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_PHRASETO_TSQUERY))
                    {
                        // PHRASETO_TSQUERY(config, text) or PHRASETO_TSQUERY(text)
                        uint8_t arg_count = readByte();

                        std::string config_name;
                        Value text_val;

                        if (arg_count == 2)
                        {
                            text_val = pop();
                            Value config_val = pop();
                            config_name = config_val.toString();
                        }
                        else if (arg_count == 1)
                        {
                            text_val = pop();
                            config_name = "simple";
                        }
                        else
                        {
                            error("PHRASETO_TSQUERY expects 1 or 2 arguments, got " + std::to_string(arg_count));
                        }

                        if (text_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            std::string text = text_val.toString();
                            auto result = core::phraseto_tsquery(config_name, text);

                            if (result.has_value())
                            {
                                push(Value::makeTSQuery(*result));
                            }
                            else
                            {
                                push(Value::makeNull());
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_TSMATCH))
                    {
                        // tsvector @@ tsquery or text @@ tsquery
                        Value right_val = pop();
                        Value left_val = pop();

                        if (left_val.isNull() || right_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            bool match = false;

                            // Handle tsvector @@ tsquery
                            if (left_val.type() == core::DataType::TSVECTOR &&
                                right_val.type() == core::DataType::TSQUERY)
                            {
                                auto vec = left_val.getTSVector();
                                auto query = right_val.getTSQuery();
                                match = core::ts_match(*vec, *query);
                            }
                            // Handle text @@ tsquery (implicit to_tsvector)
                            else if ((left_val.type() == core::DataType::TEXT ||
                                     left_val.type() == core::DataType::VARCHAR) &&
                                    right_val.type() == core::DataType::TSQUERY)
                            {
                                std::string text = left_val.toString();
                                auto query = right_val.getTSQuery();
                                match = core::ts_match_text(text, *query);
                            }
                            // Handle tsquery @@ tsvector (reversed)
                            else if (left_val.type() == core::DataType::TSQUERY &&
                                    right_val.type() == core::DataType::TSVECTOR)
                            {
                                auto query = left_val.getTSQuery();
                                auto vec = right_val.getTSVector();
                                match = core::ts_match(*vec, *query);
                            }
                            else
                            {
                                error("@@ operator requires tsvector and tsquery operands");
                            }

                            push(Value::makeBoolean(match));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_TS_RANK))
                    {
                        // TS_RANK(tsvector, tsquery [, normalization])
                        uint8_t arg_count = readByte();

                        int normalization = 0;
                        Value query_val;
                        Value vec_val;

                        if (arg_count == 3)
                        {
                            Value norm_val = pop();
                            query_val = pop();
                            vec_val = pop();
                            normalization = static_cast<int>(norm_val.toInt64());
                        }
                        else if (arg_count == 2)
                        {
                            query_val = pop();
                            vec_val = pop();
                        }
                        else
                        {
                            error("TS_RANK expects 2 or 3 arguments, got " + std::to_string(arg_count));
                        }

                        if (vec_val.isNull() || query_val.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else if (vec_val.type() != core::DataType::TSVECTOR ||
                                query_val.type() != core::DataType::TSQUERY)
                        {
                            error("TS_RANK requires tsvector and tsquery arguments");
                        }
                        else
                        {
                            auto vec = vec_val.getTSVector();
                            auto query = query_val.getTSQuery();
                            double rank = core::ts_rank(*vec, *query, normalization);
                            push(Value::makeFloat64(rank));
                        }
                    }
                    // ===== Mathematical Functions (ALPHA Phase A - Critical Priority) =====
                    // Trigonometric functions
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_SIN))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("SIN expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            push(Value::makeFloat64(std::sin(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_COS))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("COS expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            push(Value::makeFloat64(std::cos(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_TAN))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("TAN expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            push(Value::makeFloat64(std::tan(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_ASIN))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ASIN expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            if (x < -1.0 || x > 1.0)
                            {
                                error("ASIN argument must be in range [-1, 1]");
                            }
                            push(Value::makeFloat64(std::asin(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_ACOS))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ACOS expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            if (x < -1.0 || x > 1.0)
                            {
                                error("ACOS argument must be in range [-1, 1]");
                            }
                            push(Value::makeFloat64(std::acos(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_ATAN))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ATAN expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            push(Value::makeFloat64(std::atan(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_ATAN2))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("ATAN2 expects 2 arguments, got " + std::to_string(arg_count));
                        }

                        // Pop in reverse order (x, then y)
                        Value x = pop();
                        Value y = pop();

                        if (y.isNull() || x.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            push(Value::makeFloat64(std::atan2(y.toDouble(), x.toDouble())));
                        }
                    }
                    // Angle conversion functions
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_DEGREES))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("DEGREES expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double radians = arg.toDouble();
                            push(Value::makeFloat64(radians * 180.0 / M_PI));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_RADIANS))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("RADIANS expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double degrees = arg.toDouble();
                            push(Value::makeFloat64(degrees * M_PI / 180.0));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_PI))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 0)
                        {
                            error("PI expects 0 arguments, got " + std::to_string(arg_count));
                        }

                        push(Value::makeFloat64(M_PI));
                    }
                    // Algebraic functions
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_ABS))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ABS expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            // Handle different numeric types
                            if (arg.type() == core::DataType::INT64)
                            {
                                int64_t val = arg.toInt64();
                                push(Value::makeInt64(val < 0 ? -val : val));
                            }
                            else if (arg.type() == core::DataType::INT32)
                            {
                                int32_t val = arg.toInt64();
                                push(Value::makeInt32(val < 0 ? -val : val));
                            }
                            else
                            {
                                double val = arg.toDouble();
                                push(Value::makeFloat64(std::abs(val)));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_SIGN))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("SIGN expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double val = arg.toDouble();
                            int32_t sign = (val > 0.0) ? 1 : ((val < 0.0) ? -1 : 0);
                            push(Value::makeInt32(sign));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_ROUND))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count < 1 || arg_count > 2)
                        {
                            error("ROUND expects 1 or 2 arguments, got " + std::to_string(arg_count));
                        }

                        int32_t precision = 0;
                        if (arg_count == 2)
                        {
                            Value prec_val = pop();
                            if (!prec_val.isNull())
                            {
                                precision = prec_val.toInt64();
                            }
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double val = arg.toDouble();
                            double multiplier = std::pow(10.0, precision);
                            push(Value::makeFloat64(std::round(val * multiplier) / multiplier));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_CEIL))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("CEIL expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double val = arg.toDouble();
                            push(Value::makeFloat64(std::ceil(val)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_FLOOR))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("FLOOR expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double val = arg.toDouble();
                            push(Value::makeFloat64(std::floor(val)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_TRUNC))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count < 1 || arg_count > 2)
                        {
                            error("TRUNC expects 1 or 2 arguments, got " + std::to_string(arg_count));
                        }

                        int32_t precision = 0;
                        if (arg_count == 2)
                        {
                            Value prec_val = pop();
                            if (!prec_val.isNull())
                            {
                                precision = prec_val.toInt64();
                            }
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double val = arg.toDouble();
                            double multiplier = std::pow(10.0, precision);
                            push(Value::makeFloat64(std::trunc(val * multiplier) / multiplier));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_MOD))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("MOD expects 2 arguments, got " + std::to_string(arg_count));
                        }

                        // Pop in reverse order (divisor, dividend)
                        Value divisor = pop();
                        Value dividend = pop();

                        if (dividend.isNull() || divisor.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double y = divisor.toDouble();
                            if (y == 0.0)
                            {
                                error("Division by zero in MOD");
                            }
                            double x = dividend.toDouble();
                            push(Value::makeFloat64(std::fmod(x, y)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_SQRT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("SQRT expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            if (x < 0.0)
                            {
                                error("SQRT argument must be non-negative");
                            }
                            push(Value::makeFloat64(std::sqrt(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_CBRT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("CBRT expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            push(Value::makeFloat64(std::cbrt(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_POWER))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("POWER expects 2 arguments, got " + std::to_string(arg_count));
                        }

                        // Pop in reverse order (exponent, base)
                        Value exponent = pop();
                        Value base = pop();

                        if (base.isNull() || exponent.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = base.toDouble();
                            double y = exponent.toDouble();
                            push(Value::makeFloat64(std::pow(x, y)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_EXP))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("EXP expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            push(Value::makeFloat64(std::exp(x)));
                        }
                    }
                    // Logarithmic functions
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_LN))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("LN expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            if (x <= 0.0)
                            {
                                error("LN argument must be positive");
                            }
                            push(Value::makeFloat64(std::log(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_LOG))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count < 1 || arg_count > 2)
                        {
                            error("LOG expects 1 or 2 arguments, got " + std::to_string(arg_count));
                        }

                        if (arg_count == 1)
                        {
                            // LOG(x) - base 10 logarithm
                            Value arg = pop();
                            if (arg.isNull())
                            {
                                push(Value::makeNull());
                            }
                            else
                            {
                                double x = arg.toDouble();
                                if (x <= 0.0)
                                {
                                    error("LOG argument must be positive");
                                }
                                push(Value::makeFloat64(std::log10(x)));
                            }
                        }
                        else
                        {
                            // LOG(base, x) - logarithm with specified base
                            Value x_val = pop();
                            Value base_val = pop();

                            if (base_val.isNull() || x_val.isNull())
                            {
                                push(Value::makeNull());
                            }
                            else
                            {
                                double base = base_val.toDouble();
                                double x = x_val.toDouble();

                                if (base <= 0.0 || base == 1.0)
                                {
                                    error("LOG base must be positive and not equal to 1");
                                }
                                if (x <= 0.0)
                                {
                                    error("LOG argument must be positive");
                                }

                                // Change of base formula: log_b(x) = ln(x) / ln(b)
                                push(Value::makeFloat64(std::log(x) / std::log(base)));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_LOG10))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("LOG10 expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            if (x <= 0.0)
                            {
                                error("LOG10 argument must be positive");
                            }
                            push(Value::makeFloat64(std::log10(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_FUNC_LOG2))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("LOG2 expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = arg.toDouble();
                            if (x <= 0.0)
                            {
                                error("LOG2 argument must be positive");
                            }
                            push(Value::makeFloat64(std::log2(x)));
                        }
                    }
                    // Statistical functions (0xF3-0xF8)
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_STDDEV_SAMP))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("STDDEV_SAMP expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeStdDevSamp();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_STDDEV_POP))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("STDDEV_POP expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeStdDevPop();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_VAR_SAMP))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("VAR_SAMP expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeVarSamp();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_VAR_POP))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("VAR_POP expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeVarPop();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_CORR))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("CORR expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeCorr();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_COVAR_POP))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("COVAR_POP expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeCovarPop();
                    }
                    // Cryptographic functions (0xF9-0xFE)
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_MD5))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("MD5 expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeMD5();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SHA1))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("SHA1 expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeSHA1();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SHA256))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("SHA256 expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeSHA256();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SHA512))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("SHA512 expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeSHA512();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_ENCODE))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("ENCODE expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeEncode();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_DECODE))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("DECODE expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeDecode();
                    }
                    // XML functions (0x45-0x4E)
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLPARSE))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("XMLPARSE expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeXMLParse();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLSERIALIZE))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 3)
                        {
                            error("XMLSERIALIZE expects 3 arguments, got " + std::to_string(arg_count));
                        }
                        executeXMLSerialize();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLELEMENT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("XMLELEMENT expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeXMLElement();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLCONCAT))
                    {
                        // Variable arguments - already handled in executeXMLConcat
                        executeXMLConcat();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLFOREST))
                    {
                        // Variable arguments - already handled in executeXMLForest
                        executeXMLForest();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLCOMMENT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("XMLCOMMENT expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeXMLComment();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLROOT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 3)
                        {
                            error("XMLROOT expects 3 arguments, got " + std::to_string(arg_count));
                        }
                        executeXMLRoot();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XPATH))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("XPATH expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeXPath();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLEXISTS))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("XMLEXISTS expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeXMLExists();
                    }
                    // Bit manipulation - Byte/Bit access (0x06-0x09)
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_GET_BYTE))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("GET_BYTE expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeGetByte();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SET_BYTE))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 3)
                        {
                            error("SET_BYTE expects 3 arguments, got " + std::to_string(arg_count));
                        }
                        executeSetByte();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_GET_BIT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("GET_BIT expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeGetBit();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_SET_BIT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 3)
                        {
                            error("SET_BIT expects 3 arguments, got " + std::to_string(arg_count));
                        }
                        executeSetBit();
                    }
                    // Bit manipulation - Bitwise operations (0x15-0x1B)
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_BIT_AND))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("BIT_AND expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeBitAnd();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_BIT_OR))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("BIT_OR expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeBitOr();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_BIT_XOR))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("BIT_XOR expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeBitXor();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_BIT_NOT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("BIT_NOT expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeBitNot();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_BIT_SHIFT_LEFT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("BIT_SHIFT_LEFT expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeBitShiftLeft();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_BIT_SHIFT_RIGHT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("BIT_SHIFT_RIGHT expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeBitShiftRight();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_BIT_SHIFT_RIGHT_LOGICAL))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("BIT_SHIFT_RIGHT_LOGICAL expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeBitShiftRightLogical();
                    }
                    // Bit manipulation - Utility functions (0x25-0x27)
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_BIT_COUNT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("BIT_COUNT expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeBitCount();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_BIT_LENGTH))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("BIT_LENGTH expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeBitLength();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_BIT_MASK))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("BIT_MASK expects 1 argument, got " + std::to_string(arg_count));
                        }
                        executeBitMask();
                    }
                    // XML functions (0x45-0x49)
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLPARSE))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("XMLPARSE expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeXMLParse();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLSERIALIZE))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("XMLSERIALIZE expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeXMLSerialize();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLELEMENT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("XMLELEMENT expects 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeXMLElement();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLCONCAT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count < 2)
                        {
                            error("XMLCONCAT expects at least 2 arguments, got " + std::to_string(arg_count));
                        }
                        executeXMLConcat();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_XMLFOREST))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count < 1)
                        {
                            error("XMLFOREST expects at least 1 argument, got " + std::to_string(arg_count));
                        }
                        executeXMLForest();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_EXTRACT))
                    {
                        executeExtract();
                    }
                    // Index operation opcodes (November 19, 2025)
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_INDEX_INSERT))
                    {
                        executeIndexInsert();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_INDEX_SEARCH))
                    {
                        executeIndexSearch();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_INDEX_SCAN))
                    {
                        executeIndexScan();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_INDEX_DELETE))
                    {
                        executeIndexDelete();
                    }
                    // Specialized index operations
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_GIN_INSERT))
                    {
                        executeGinInsert();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_GIN_SEARCH))
                    {
                        executeGinSearch();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_HNSW_INSERT))
                    {
                        executeHnswInsert();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_HNSW_SEARCH))
                    {
                        executeHnswSearch();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_COLUMNSTORE_INSERT))
                    {
                        executeColumnstoreInsert();
                    }
                    else if (ext_op == static_cast<uint8_t>(Opcode::EXT_COLUMNSTORE_SCAN))
                    {
                        executeColumnstoreScan();
                    }
                    else
                    {
                        error("Unknown extended opcode: " + std::to_string(ext_op));
                    }
                    break;
                }

                default:
                    error("Unknown expression opcode: " + std::to_string(static_cast<int>(op)));
            }
        }

        void Executor::executeBinaryOp(Opcode op)
        {
            // Pop right operand first (stack order)
            Value right = pop();
            Value left = pop();

            // Handle NULL propagation
            if (left.isNull() || right.isNull())
            {
                push(Value::makeNull()); // NULL result
                return;
            }

            switch (op)
            {
                case Opcode::EXPR_ADD:
                {
                    if (left.type() == core::DataType::FLOAT64 ||
                        right.type() == core::DataType::FLOAT64)
                        push(Value::makeFloat64(left.toDouble() + right.toDouble()));
                    else
                        push(Value::makeInt64(left.toInt64() + right.toInt64()));
                    break;
                }
                case Opcode::EXPR_SUBTRACT:
                {
                    if (left.type() == core::DataType::FLOAT64 ||
                        right.type() == core::DataType::FLOAT64)
                        push(Value::makeFloat64(left.toDouble() - right.toDouble()));
                    else
                        push(Value::makeInt64(left.toInt64() - right.toInt64()));
                    break;
                }
                case Opcode::EXPR_MULTIPLY:
                {
                    if (left.type() == core::DataType::FLOAT64 ||
                        right.type() == core::DataType::FLOAT64)
                        push(Value::makeFloat64(left.toDouble() * right.toDouble()));
                    else
                        push(Value::makeInt64(left.toInt64() * right.toInt64()));
                    break;
                }
                case Opcode::EXPR_DIVIDE:
                {
                    if (right.toDouble() == 0.0)
                        error("Division by zero");
                    if (left.type() == core::DataType::FLOAT64 ||
                        right.type() == core::DataType::FLOAT64)
                        push(Value::makeFloat64(left.toDouble() / right.toDouble()));
                    else
                        push(Value::makeInt64(left.toInt64() / right.toInt64()));
                    break;
                }
                case Opcode::EXPR_MODULO:
                {
                    if (right.toInt64() == 0)
                        error("Modulo by zero");
                    push(Value::makeInt64(left.toInt64() % right.toInt64()));
                    break;
                }

                // Comparison operators (collation-aware for strings)
                case Opcode::EXPR_EQ:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) ||
                        core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) == 0;
                    else if (left.type() == core::DataType::FLOAT64 ||
                             right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() == right.toDouble();
                    else
                        result = left.toInt64() == right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_NE:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) ||
                        core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) != 0;
                    else if (left.type() == core::DataType::FLOAT64 ||
                             right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() != right.toDouble();
                    else
                        result = left.toInt64() != right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_LT:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) ||
                        core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) < 0;
                    else if (left.type() == core::DataType::FLOAT64 ||
                             right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() < right.toDouble();
                    else
                        result = left.toInt64() < right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_GT:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) ||
                        core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) > 0;
                    else if (left.type() == core::DataType::FLOAT64 ||
                             right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() > right.toDouble();
                    else
                        result = left.toInt64() > right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_LE:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) ||
                        core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) <= 0;
                    else if (left.type() == core::DataType::FLOAT64 ||
                             right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() <= right.toDouble();
                    else
                        result = left.toInt64() <= right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_GE:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) ||
                        core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) >= 0;
                    else if (left.type() == core::DataType::FLOAT64 ||
                             right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() >= right.toDouble();
                    else
                        result = left.toInt64() >= right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }

                // Logical operators
                case Opcode::EXPR_AND:
                    push(Value::makeBoolean(left.toBoolean() && right.toBoolean()));
                    break;
                case Opcode::EXPR_OR:
                    push(Value::makeBoolean(left.toBoolean() || right.toBoolean()));
                    break;

                // Pattern matching
                case Opcode::EXPR_LIKE:
                {
                    std::string text = left.toString();
                    std::string pattern = right.toString();
                    bool result = matchPattern(text, pattern, false);
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_ILIKE:
                {
                    std::string text = left.toString();
                    std::string pattern = right.toString();
                    bool result = matchPattern(text, pattern, true);
                    push(Value::makeBoolean(result));
                    break;
                }

                default:
                    error("Unknown binary operator: " + std::to_string(static_cast<int>(op)));
            }
        }

        void Executor::error(const std::string &msg)
        {
            throw std::runtime_error(msg);
        }

        bool Executor::matchPattern(const std::string &text, const std::string &pattern,
                                    bool case_insensitive)
        {
            // Convert to lowercase for case-insensitive matching
            std::string t = text;
            std::string p = pattern;

            if (case_insensitive)
            {
                for (char &c : t)
                    c = std::tolower(static_cast<unsigned char>(c));
                for (char &c : p)
                    c = std::tolower(static_cast<unsigned char>(c));
            }

            // Simple pattern matching with % (any chars) and _ (single char)
            return matchPatternRecursive(t, 0, p, 0);
        }

        bool Executor::matchPatternRecursive(const std::string &text, size_t text_pos,
                                             const std::string &pattern, size_t pattern_pos)
        {
            // End of pattern
            if (pattern_pos == pattern.length())
            {
                return text_pos == text.length();
            }

            // % wildcard - matches zero or more characters
            if (pattern[pattern_pos] == '%')
            {
                // Skip consecutive % wildcards
                while (pattern_pos < pattern.length() && pattern[pattern_pos] == '%')
                {
                    pattern_pos++;
                }

                // If % is at the end, match rest of text
                if (pattern_pos == pattern.length())
                {
                    return true;
                }

                // Try matching at different positions
                for (size_t i = text_pos; i <= text.length(); i++)
                {
                    if (matchPatternRecursive(text, i, pattern, pattern_pos))
                    {
                        return true;
                    }
                }
                return false;
            }

            // End of text but pattern remains
            if (text_pos == text.length())
            {
                return false;
            }

            // _ wildcard - matches exactly one character
            if (pattern[pattern_pos] == '_')
            {
                return matchPatternRecursive(text, text_pos + 1, pattern, pattern_pos + 1);
            }

            // Regular character match
            if (text[text_pos] == pattern[pattern_pos])
            {
                return matchPatternRecursive(text, text_pos + 1, pattern, pattern_pos + 1);
            }

            return false;
        }

        int Executor::compareStrings(const std::string &left, const std::string &right,
                                     uint32_t collation_id) const
        {
            // Use charset manager for collation-aware comparison
            // Default collation_id = 101 (utf8_general_ci - case insensitive)
            return charset_manager_.compare(
                reinterpret_cast<const uint8_t *>(left.data()), left.length(),
                reinterpret_cast<const uint8_t *>(right.data()), right.length(), collation_id);
        }

        // ========== Phase 2 Task 13: Text Search and Regex Functions ==========

        bool Executor::matchRegex(const std::string &text, const std::string &pattern, bool case_insensitive)
        {
            try
            {
                std::regex::flag_type flags = std::regex::ECMAScript;
                if (case_insensitive)
                {
                    flags |= std::regex::icase;
                }
                std::regex re(pattern, flags);
                return std::regex_search(text, re);
            }
            catch (const std::regex_error &e)
            {
                // SECURITY FIX (HIGH-1): Log detailed error internally, return generic error to client
                LOG_ERROR(EXECUTOR, "Invalid regular expression '%s': %s", pattern.c_str(), e.what());
                error("Invalid regular expression");
                return false;
            }
        }

        std::vector<std::string> Executor::regexMatches(const std::string &text, const std::string &pattern, const std::string &flags)
        {
            std::vector<std::string> results;
            try
            {
                std::regex::flag_type re_flags = std::regex::ECMAScript;
                bool global = false;

                // Parse flags
                for (char f : flags)
                {
                    if (f == 'i')
                        re_flags |= std::regex::icase;
                    else if (f == 'g')
                        global = true;
                    // 'm' for multiline is implied in ECMAScript
                }

                std::regex re(pattern, re_flags);

                if (global)
                {
                    // Find all matches
                    auto words_begin = std::sregex_iterator(text.begin(), text.end(), re);
                    auto words_end = std::sregex_iterator();

                    for (std::sregex_iterator i = words_begin; i != words_end; ++i)
                    {
                        std::smatch match = *i;
                        results.push_back(match.str());
                    }
                }
                else
                {
                    // Find first match only
                    std::smatch match;
                    if (std::regex_search(text, match, re))
                    {
                        results.push_back(match.str());
                    }
                }
            }
            catch (const std::regex_error &e)
            {
                // SECURITY FIX (HIGH-1): Log detailed error internally, return generic error to client
                LOG_ERROR(EXECUTOR, "Invalid regular expression '%s': %s", pattern.c_str(), e.what());
                error("Invalid regular expression");
            }
            return results;
        }

        std::string Executor::regexReplace(const std::string &text, const std::string &pattern,
                                           const std::string &replacement, const std::string &flags)
        {
            try
            {
                std::regex::flag_type re_flags = std::regex::ECMAScript;
                bool global = false;

                // Parse flags
                for (char f : flags)
                {
                    if (f == 'i')
                        re_flags |= std::regex::icase;
                    else if (f == 'g')
                        global = true;
                }

                std::regex re(pattern, re_flags);

                if (global)
                {
                    return std::regex_replace(text, re, replacement);
                }
                else
                {
                    // Replace first match only
                    std::smatch match;
                    std::string result = text;
                    if (std::regex_search(result, match, re))
                    {
                        result.replace(match.position(), match.length(), replacement);
                    }
                    return result;
                }
            }
            catch (const std::regex_error &e)
            {
                // SECURITY FIX (HIGH-1): Log detailed error internally, return generic error to client
                LOG_ERROR(EXECUTOR, "Invalid regular expression '%s': %s", pattern.c_str(), e.what());
                error("Invalid regular expression");
                return text;
            }
        }

        std::vector<std::string> Executor::regexSplit(const std::string &text, const std::string &pattern,
                                                      const std::string &flags)
        {
            std::vector<std::string> results;
            try
            {
                std::regex::flag_type re_flags = std::regex::ECMAScript;

                // Parse flags
                for (char f : flags)
                {
                    if (f == 'i')
                        re_flags |= std::regex::icase;
                }

                std::regex re(pattern, re_flags);

                // Use sregex_token_iterator to split by regex
                std::sregex_token_iterator iter(text.begin(), text.end(), re, -1);
                std::sregex_token_iterator end;

                for (; iter != end; ++iter)
                {
                    results.push_back(*iter);
                }

                // If no matches found, return the whole string
                if (results.empty())
                {
                    results.push_back(text);
                }
            }
            catch (const std::regex_error &e)
            {
                // SECURITY FIX (HIGH-1): Log detailed error internally, return generic error to client
                LOG_ERROR(EXECUTOR, "Invalid regular expression '%s': %s", pattern.c_str(), e.what());
                error("Invalid regular expression");
                results.push_back(text);
            }
            return results;
        }

        bool
        Executor::deserializeTuple(const uint8_t *tuple_data, uint32_t tuple_size,
                                   const std::vector<core::CatalogManager::ColumnInfo> &columns,
                                   std::vector<Value> &values_out)
        {
            if (tuple_size < sizeof(core::TupleHeader))
            {
                return false; // Malformed tuple
            }

            // Read TupleHeader
            const auto *header = reinterpret_cast<const core::TupleHeader *>(tuple_data);

            // Check if tuple is deleted
            if (header->isDeleted())
            {
                return false;
            }

            // Get null bitmap if present
            const uint8_t *null_bitmap = nullptr;
            if (header->hasNulls() && header->null_bitmap_offset > 0 &&
                header->null_bitmap_offset < tuple_size)
            {
                null_bitmap = tuple_data + header->null_bitmap_offset;
            }

            // Read column data
            size_t data_offset = sizeof(core::TupleHeader);
            if (header->hasNulls() && null_bitmap)
            {
                // Skip past null bitmap
                size_t bitmap_bytes = (columns.size() + 7) / 8;
                data_offset = header->null_bitmap_offset + bitmap_bytes;
            }

            values_out.clear();
            values_out.reserve(columns.size());

            for (size_t i = 0; i < columns.size(); i++)
            {
                // Check if column is null
                if (null_bitmap)
                {
                    size_t byte_offset = i / 8;
                    size_t bit_pos = i % 8;
                    if (null_bitmap[byte_offset] & (1 << bit_pos))
                    {
                        values_out.push_back(Value::makeNull()); // NULL value
                        continue;
                    }
                }

                // Deserialize value based on column type
                core::DataType col_type = static_cast<core::DataType>(columns[i].data_type);

                switch (col_type)
                {
                    case core::DataType::INT32:
                    {
                        if (data_offset + sizeof(int32_t) > tuple_size)
                            return false;

                        int32_t val;
                        std::memcpy(&val, tuple_data + data_offset, sizeof(int32_t));
                        values_out.push_back(Value::makeInt32(val));
                        data_offset += sizeof(int32_t);
                        break;
                    }
                    case core::DataType::INT64:
                    {
                        if (data_offset + sizeof(int64_t) > tuple_size)
                            return false;

                        int64_t val;
                        std::memcpy(&val, tuple_data + data_offset, sizeof(int64_t));
                        values_out.push_back(Value::makeInt64(val));
                        data_offset += sizeof(int64_t);
                        break;
                    }
                    case core::DataType::FLOAT64:
                    {
                        if (data_offset + sizeof(double) > tuple_size)
                            return false;

                        double val;
                        std::memcpy(&val, tuple_data + data_offset, sizeof(double));
                        values_out.push_back(Value::makeFloat64(val));
                        data_offset += sizeof(double);
                        break;
                    }
                    case core::DataType::VARCHAR:
                    {
                        if (data_offset + sizeof(uint32_t) > tuple_size)
                            return false;

                        uint32_t len;
                        std::memcpy(&len, tuple_data + data_offset, sizeof(uint32_t));
                        data_offset += sizeof(uint32_t);

                        if (data_offset + len > tuple_size)
                            return false;

                        std::string str(reinterpret_cast<const char *>(tuple_data + data_offset),
                                        len);
                        values_out.push_back(Value::makeVarchar(str));
                        data_offset += len;
                        break;
                    }
                    default:
                        return false; // Unsupported type
                }
            }

            return true;
        }

        // ===== JOIN Execution (Phase 1, Task 3.3) =====

        // Helper: Execute a child plan and return its result set
        std::unique_ptr<ResultSet> Executor::executeChildPlan()
        {
            // Save current result set
            auto saved_result_set = std::move(current_result_set_);

            // Execute the child SELECT statement
            // The bytecode should be a complete SELECT statement
            uint8_t opcode = readByte();
            if (opcode != static_cast<uint8_t>(Opcode::SELECT))
            {
                error("Expected SELECT opcode in JOIN child plan");
            }

            // Execute the SELECT - this will populate current_result_set_
            executeSelect();

            // Retrieve and return the result
            auto child_result = std::move(current_result_set_);

            // Restore previous result set
            current_result_set_ = std::move(saved_result_set);

            return child_result;
        }

        // Helper: Combine two rows into one
        std::vector<Value> Executor::combineRows(const std::vector<Value> &outer_row,
                                                  const std::vector<Value> &inner_row)
        {
            std::vector<Value> combined;
            combined.reserve(outer_row.size() + inner_row.size());
            combined.insert(combined.end(), outer_row.begin(), outer_row.end());
            combined.insert(combined.end(), inner_row.begin(), inner_row.end());
            return combined;
        }

        // Helper: Evaluate join condition with two rows
        bool Executor::evaluateJoinCondition(const std::vector<Value> &outer_row,
                                              const std::vector<Value> &inner_row,
                                              const std::vector<core::CatalogManager::ColumnInfo> &outer_columns,
                                              const std::vector<core::CatalogManager::ColumnInfo> &inner_columns,
                                              size_t condition_start_pc, size_t condition_end_pc)
        {
            // Save current PC
            size_t saved_pc = pc_;
            pc_ = condition_start_pc;

            // Combine rows for evaluation
            std::vector<Value> combined_row = combineRows(outer_row, inner_row);

            // Combine column info
            std::vector<core::CatalogManager::ColumnInfo> combined_columns;
            combined_columns.reserve(outer_columns.size() + inner_columns.size());
            combined_columns.insert(combined_columns.end(), outer_columns.begin(), outer_columns.end());
            combined_columns.insert(combined_columns.end(), inner_columns.begin(), inner_columns.end());

            // Set up row context
            current_row_values_ = &combined_row;
            current_row_columns_ = &combined_columns;

            bool result = true;
            try
            {
                evaluateExpression();
                Value condition_result = pop();
                result = condition_result.toBoolean();
            }
            catch (...)
            {
                current_row_values_ = nullptr;
                current_row_columns_ = nullptr;
                pc_ = saved_pc;
                throw;
            }

            // Restore state
            current_row_values_ = nullptr;
            current_row_columns_ = nullptr;
            pc_ = saved_pc;

            return result;
        }

        void Executor::executeNestedLoopJoin()
        {
            // Read join type
            if (readByte() != static_cast<uint8_t>(Opcode::JOIN_TYPE))
            {
                error("Expected JOIN_TYPE");
            }
            uint8_t join_type_byte = readByte();
            parser::JoinType join_type = static_cast<parser::JoinType>(join_type_byte);

            // Execute outer (left) child plan
            auto outer_result = executeChildPlan();
            if (!outer_result)
            {
                error("Failed to execute outer child plan");
            }

            // Save PC position before inner child bytecode
            size_t inner_child_start_pc = pc_;

            // Execute inner child once to get schema (then we'll re-execute for each outer row)
            auto inner_schema_result = executeChildPlan();
            if (!inner_schema_result)
            {
                error("Failed to execute inner child plan");
            }

            // Save PC position after inner child bytecode
            size_t inner_child_end_pc = pc_;

            // Check for join condition
            bool has_condition = false;
            size_t condition_start_pc = 0;
            size_t condition_end_pc = 0;

            if (pc_ < bytecode_size_ &&
                bytecode_[pc_] == static_cast<uint8_t>(Opcode::JOIN_CONDITION))
            {
                has_condition = true;
                readByte(); // Consume JOIN_CONDITION opcode
                condition_start_pc = pc_;

                // Skip over condition expression
                int depth = 1;
                while (pc_ < bytecode_size_ && depth > 0)
                {
                    Opcode op = static_cast<Opcode>(readByte());

                    if (op == Opcode::LITERAL_INT32)
                    {
                        pc_ += 4;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_INT64)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_DOUBLE)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_STRING || op == Opcode::COLUMN_REF)
                    {
                        uint32_t len = readInt32();
                        pc_ += len;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_NULL)
                    {
                        depth++;
                    }
                    else if (op >= Opcode::EXPR_ADD && op <= Opcode::EXPR_OR)
                    {
                        depth--;
                    }
                }
                condition_end_pc = pc_;
            }

            // Build combined result set schema
            current_result_set_ = std::make_unique<ResultSet>();

            // Add outer columns
            for (size_t i = 0; i < outer_result->columnCount(); i++)
            {
                current_result_set_->addColumn(outer_result->columnName(i),
                                               outer_result->columnType(i));
            }

            // Add inner columns
            for (size_t i = 0; i < inner_schema_result->columnCount(); i++)
            {
                current_result_set_->addColumn(inner_schema_result->columnName(i),
                                               inner_schema_result->columnType(i));
            }

            // Get column info for condition evaluation (we'll need to reconstruct this)
            std::vector<core::CatalogManager::ColumnInfo> outer_columns;
            for (size_t i = 0; i < outer_result->columnCount(); i++)
            {
                core::CatalogManager::ColumnInfo col;
                col.column_name = outer_result->columnName(i);
                col.data_type = static_cast<uint16_t>(outer_result->columnType(i));
                outer_columns.push_back(col);
            }

            std::vector<core::CatalogManager::ColumnInfo> inner_columns;
            for (size_t i = 0; i < inner_schema_result->columnCount(); i++)
            {
                core::CatalogManager::ColumnInfo col;
                col.column_name = inner_schema_result->columnName(i);
                col.data_type = static_cast<uint16_t>(inner_schema_result->columnType(i));
                inner_columns.push_back(col);
            }

            // Perform nested loop join
            for (size_t outer_idx = 0; outer_idx < outer_result->rowCount(); outer_idx++)
            {
                // Get outer row
                std::vector<Value> outer_row;
                for (size_t col = 0; col < outer_result->columnCount(); col++)
                {
                    outer_row.push_back(outer_result->getValue(outer_idx, col));
                }

                // Re-execute inner child for this outer row
                size_t saved_pc = pc_;
                pc_ = inner_child_start_pc;
                auto inner_result = executeChildPlan();
                pc_ = saved_pc;

                bool outer_row_matched = false;

                // For each inner row
                for (size_t inner_idx = 0; inner_idx < inner_result->rowCount(); inner_idx++)
                {
                    // Get inner row
                    std::vector<Value> inner_row;
                    for (size_t col = 0; col < inner_result->columnCount(); col++)
                    {
                        inner_row.push_back(inner_result->getValue(inner_idx, col));
                    }

                    // Evaluate join condition
                    bool condition_met = true;
                    if (has_condition)
                    {
                        condition_met = evaluateJoinCondition(outer_row, inner_row,
                                                               outer_columns, inner_columns,
                                                               condition_start_pc, condition_end_pc);
                    }

                    if (condition_met)
                    {
                        outer_row_matched = true;
                        // Combine and add to result
                        auto combined_row = combineRows(outer_row, inner_row);
                        current_result_set_->addRow(std::move(combined_row));
                    }
                }

                // Handle outer joins - add NULL-padded row if no match
                if (!outer_row_matched && (join_type == parser::JoinType::LEFT ||
                                           join_type == parser::JoinType::FULL))
                {
                    std::vector<Value> null_inner_row;
                    for (size_t i = 0; i < inner_columns.size(); i++)
                    {
                        null_inner_row.push_back(Value()); // NULL value
                    }
                    auto combined_row = combineRows(outer_row, null_inner_row);
                    current_result_set_->addRow(std::move(combined_row));
                }
            }

            // Handle RIGHT and FULL outer joins - add unmatched inner rows with NULL outer
            if (join_type == parser::JoinType::RIGHT || join_type == parser::JoinType::FULL)
            {
                // Re-execute inner child one more time
                size_t saved_pc = pc_;
                pc_ = inner_child_start_pc;
                auto inner_result = executeChildPlan();
                pc_ = saved_pc;

                // For each inner row, check if it matched any outer row
                for (size_t inner_idx = 0; inner_idx < inner_result->rowCount(); inner_idx++)
                {
                    std::vector<Value> inner_row;
                    for (size_t col = 0; col < inner_result->columnCount(); col++)
                    {
                        inner_row.push_back(inner_result->getValue(inner_idx, col));
                    }

                    bool inner_row_matched = false;

                    // Check against all outer rows
                    for (size_t outer_idx = 0; outer_idx < outer_result->rowCount(); outer_idx++)
                    {
                        std::vector<Value> outer_row;
                        for (size_t col = 0; col < outer_result->columnCount(); col++)
                        {
                            outer_row.push_back(outer_result->getValue(outer_idx, col));
                        }

                        bool condition_met = true;
                        if (has_condition)
                        {
                            condition_met = evaluateJoinCondition(outer_row, inner_row,
                                                                   outer_columns, inner_columns,
                                                                   condition_start_pc, condition_end_pc);
                        }

                        if (condition_met)
                        {
                            inner_row_matched = true;
                            break;
                        }
                    }

                    // Add NULL-padded row if no match
                    if (!inner_row_matched)
                    {
                        std::vector<Value> null_outer_row;
                        for (size_t i = 0; i < outer_columns.size(); i++)
                        {
                            null_outer_row.push_back(Value()); // NULL value
                        }
                        auto combined_row = combineRows(null_outer_row, inner_row);
                        current_result_set_->addRow(std::move(combined_row));
                    }
                }
            }
        }

        void Executor::executeHashJoin()
        {
            // Read join type
            if (readByte() != static_cast<uint8_t>(Opcode::JOIN_TYPE))
            {
                error("Expected JOIN_TYPE");
            }
            uint8_t join_type_byte = readByte();
            parser::JoinType join_type = static_cast<parser::JoinType>(join_type_byte);

            // Execute outer (probe) child plan
            auto outer_result = executeChildPlan();
            if (!outer_result)
            {
                error("Failed to execute outer child plan");
            }

            // Execute inner (build) child plan
            auto inner_result = executeChildPlan();
            if (!inner_result)
            {
                error("Failed to execute inner child plan");
            }

            // Read hash keys for outer side
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for outer hash keys");
            }
            uint32_t outer_key_count = readInt32();
            std::vector<size_t> outer_key_positions;

            // For now, we'll skip the expression bytecode and just track count
            // A full implementation would evaluate these expressions
            for (uint32_t i = 0; i < outer_key_count; i++)
            {
                // Skip hash key expression (simplified - assume COLUMN_REF)
                if (readByte() == static_cast<uint8_t>(Opcode::COLUMN_REF))
                {
                    std::string col_name = readString();
                    // Find column index
                    for (size_t col_idx = 0; col_idx < outer_result->columnCount(); col_idx++)
                    {
                        if (outer_result->columnName(col_idx) == col_name)
                        {
                            outer_key_positions.push_back(col_idx);
                            break;
                        }
                    }
                }
            }

            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST for outer hash keys");
            }

            // Read hash keys for inner side
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for inner hash keys");
            }
            uint32_t inner_key_count = readInt32();
            std::vector<size_t> inner_key_positions;

            for (uint32_t i = 0; i < inner_key_count; i++)
            {
                if (readByte() == static_cast<uint8_t>(Opcode::COLUMN_REF))
                {
                    std::string col_name = readString();
                    for (size_t col_idx = 0; col_idx < inner_result->columnCount(); col_idx++)
                    {
                        if (inner_result->columnName(col_idx) == col_name)
                        {
                            inner_key_positions.push_back(col_idx);
                            break;
                        }
                    }
                }
            }

            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST for inner hash keys");
            }

            // Check for optional join condition
            bool has_condition = false;
            size_t condition_start_pc = 0;
            size_t condition_end_pc = 0;

            if (pc_ < bytecode_size_ &&
                bytecode_[pc_] == static_cast<uint8_t>(Opcode::JOIN_CONDITION))
            {
                has_condition = true;
                readByte(); // Consume JOIN_CONDITION
                condition_start_pc = pc_;

                // Skip condition expression (same logic as nested loop join)
                int depth = 1;
                while (pc_ < bytecode_size_ && depth > 0)
                {
                    Opcode op = static_cast<Opcode>(readByte());
                    if (op == Opcode::LITERAL_INT32)
                    {
                        pc_ += 4;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_INT64)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_DOUBLE)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_STRING || op == Opcode::COLUMN_REF)
                    {
                        uint32_t len = readInt32();
                        pc_ += len;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_NULL)
                    {
                        depth++;
                    }
                    else if (op >= Opcode::EXPR_ADD && op <= Opcode::EXPR_OR)
                    {
                        depth--;
                    }
                }
                condition_end_pc = pc_;
            }

            // Build hash table from inner result
            // Hash table: map from hash key -> vector of row indices
            std::map<std::string, std::vector<size_t>> hash_table;

            for (size_t inner_idx = 0; inner_idx < inner_result->rowCount(); inner_idx++)
            {
                // Build hash key from inner row
                std::string hash_key;
                for (size_t key_pos : inner_key_positions)
                {
                    Value val = inner_result->getValue(inner_idx, key_pos);
                    hash_key += val.toString() + "|";
                }

                hash_table[hash_key].push_back(inner_idx);
            }

            // Build combined result set schema
            current_result_set_ = std::make_unique<ResultSet>();

            for (size_t i = 0; i < outer_result->columnCount(); i++)
            {
                current_result_set_->addColumn(outer_result->columnName(i),
                                               outer_result->columnType(i));
            }

            for (size_t i = 0; i < inner_result->columnCount(); i++)
            {
                current_result_set_->addColumn(inner_result->columnName(i),
                                               inner_result->columnType(i));
            }

            // Get column info for condition evaluation
            std::vector<core::CatalogManager::ColumnInfo> outer_columns;
            for (size_t i = 0; i < outer_result->columnCount(); i++)
            {
                core::CatalogManager::ColumnInfo col;
                col.column_name = outer_result->columnName(i);
                col.data_type = static_cast<uint16_t>(outer_result->columnType(i));
                outer_columns.push_back(col);
            }

            std::vector<core::CatalogManager::ColumnInfo> inner_columns;
            for (size_t i = 0; i < inner_result->columnCount(); i++)
            {
                core::CatalogManager::ColumnInfo col;
                col.column_name = inner_result->columnName(i);
                col.data_type = static_cast<uint16_t>(inner_result->columnType(i));
                inner_columns.push_back(col);
            }

            // Probe phase: for each outer row, probe hash table
            std::vector<bool> outer_row_matched(outer_result->rowCount(), false);
            std::vector<bool> inner_row_matched(inner_result->rowCount(), false);

            for (size_t outer_idx = 0; outer_idx < outer_result->rowCount(); outer_idx++)
            {
                // Build hash key from outer row
                std::string hash_key;
                for (size_t key_pos : outer_key_positions)
                {
                    Value val = outer_result->getValue(outer_idx, key_pos);
                    hash_key += val.toString() + "|";
                }

                // Probe hash table
                auto it = hash_table.find(hash_key);
                if (it != hash_table.end())
                {
                    // Found matching inner rows
                    for (size_t inner_idx : it->second)
                    {
                        // Get rows
                        std::vector<Value> outer_row;
                        for (size_t col = 0; col < outer_result->columnCount(); col++)
                        {
                            outer_row.push_back(outer_result->getValue(outer_idx, col));
                        }

                        std::vector<Value> inner_row;
                        for (size_t col = 0; col < inner_result->columnCount(); col++)
                        {
                            inner_row.push_back(inner_result->getValue(inner_idx, col));
                        }

                        // Evaluate residual join condition if present
                        bool condition_met = true;
                        if (has_condition)
                        {
                            condition_met = evaluateJoinCondition(outer_row, inner_row,
                                                                   outer_columns, inner_columns,
                                                                   condition_start_pc, condition_end_pc);
                        }

                        if (condition_met)
                        {
                            outer_row_matched[outer_idx] = true;
                            inner_row_matched[inner_idx] = true;

                            // Add combined row to result
                            auto combined_row = combineRows(outer_row, inner_row);
                            current_result_set_->addRow(std::move(combined_row));
                        }
                    }
                }

                // Handle LEFT/FULL outer join - add NULL-padded row if no match
                if (!outer_row_matched[outer_idx] &&
                    (join_type == parser::JoinType::LEFT || join_type == parser::JoinType::FULL))
                {
                    std::vector<Value> outer_row;
                    for (size_t col = 0; col < outer_result->columnCount(); col++)
                    {
                        outer_row.push_back(outer_result->getValue(outer_idx, col));
                    }

                    std::vector<Value> null_inner_row;
                    for (size_t i = 0; i < inner_columns.size(); i++)
                    {
                        null_inner_row.push_back(Value()); // NULL
                    }

                    auto combined_row = combineRows(outer_row, null_inner_row);
                    current_result_set_->addRow(std::move(combined_row));
                }
            }

            // Handle RIGHT/FULL outer join - add unmatched inner rows with NULL outer
            if (join_type == parser::JoinType::RIGHT || join_type == parser::JoinType::FULL)
            {
                for (size_t inner_idx = 0; inner_idx < inner_result->rowCount(); inner_idx++)
                {
                    if (!inner_row_matched[inner_idx])
                    {
                        std::vector<Value> null_outer_row;
                        for (size_t i = 0; i < outer_columns.size(); i++)
                        {
                            null_outer_row.push_back(Value()); // NULL
                        }

                        std::vector<Value> inner_row;
                        for (size_t col = 0; col < inner_result->columnCount(); col++)
                        {
                            inner_row.push_back(inner_result->getValue(inner_idx, col));
                        }

                        auto combined_row = combineRows(null_outer_row, inner_row);
                        current_result_set_->addRow(std::move(combined_row));
                    }
                }
            }
        }

        // ===== PSQL - Stored Procedures and Functions (Phase 2 Task 10.2, Phase 4) =====

        // Variable Stack Implementation
        void Executor::VariableStack::pushFrame()
        {
            if (frames_.empty())
            {
                frames_.push_back(std::make_unique<VariableFrame>());
            }
            else
            {
                frames_.push_back(std::make_unique<VariableFrame>(frames_.back().get()));
            }
        }

        void Executor::VariableStack::popFrame()
        {
            if (!frames_.empty())
            {
                frames_.pop_back();
            }
        }

        void Executor::VariableStack::declareVariable(const std::string& name, const Value& value)
        {
            if (frames_.empty())
            {
                throw std::runtime_error("No variable frame available");
            }
            frames_.back()->variables[name] = value;
        }

        Value& Executor::VariableStack::getVariable(const std::string& name)
        {
            // Search from current frame up to parent frames
            for (auto it = frames_.rbegin(); it != frames_.rend(); ++it)
            {
                auto var_it = (*it)->variables.find(name);
                if (var_it != (*it)->variables.end())
                {
                    return var_it->second;
                }
            }
            throw std::runtime_error("Variable not found: " + name);
        }

        void Executor::VariableStack::setVariable(const std::string& name, const Value& value)
        {
            // Search from current frame up to parent frames
            for (auto it = frames_.rbegin(); it != frames_.rend(); ++it)
            {
                auto var_it = (*it)->variables.find(name);
                if (var_it != (*it)->variables.end())
                {
                    var_it->second = value;
                    return;
                }
            }
            throw std::runtime_error("Variable not found: " + name);
        }

        bool Executor::VariableStack::hasVariable(const std::string& name) const
        {
            for (auto it = frames_.rbegin(); it != frames_.rend(); ++it)
            {
                if ((*it)->variables.find(name) != (*it)->variables.end())
                {
                    return true;
                }
            }
            return false;
        }

        // PSQL Statement Execution

        void Executor::executeFunction()
        {
            // Read function name
            std::string function_name = readString();

            // Read parameter count
            uint8_t param_count = readByte();

            // Security Phase 3.1: Lookup function and check permissions
            core::CatalogManager::FunctionInfo function_info;
            core::ErrorContext err_ctx;
            auto status = db_->catalog_manager()->getFunction(function_name, function_info, &err_ctx);
            if (status != core::Status::OK)
            {
                error("Function not found: " + function_name);
            }

            // Check EXECUTE permission
            auto ctx = core::ConnectionContext::getCurrent();
            if (ctx && !ctx->isSuperuser())
            {
                if (!db_->catalog_manager()->hasObjectPermission(
                    function_info.function_id,
                    ctx->getCurrentUserId(),
                    0x0001, // PERM_EXECUTE
                    &err_ctx))
                {
                    error("Permission denied: EXECUTE on function " + function_name);
                }
            }

            // Security Phase 3.1: Push security context based on SQL SECURITY mode
            bool security_context_pushed = false;
            if (ctx)
            {
                if (function_info.sql_security == core::CatalogManager::FunctionInfo::SqlSecurity::DEFINER)
                {
                    // Execute with owner's privileges
                    bool owner_is_superuser = false;
                    core::CatalogManager::UserInfo owner_info;
                    if (db_->catalog_manager()->getUser(function_info.owner_id, owner_info, &err_ctx) == core::Status::OK)
                    {
                        owner_is_superuser = owner_info.is_superuser;
                    }

                    ctx->pushSecurityContext(
                        function_info.owner_id,
                        core::ID(),  // No role for DEFINER mode
                        owner_is_superuser,
                        core::ConnectionContext::SecurityMode::DEFINER,
                        function_info.function_id
                    );
                    security_context_pushed = true;
                }
                else
                {
                    // INVOKER: Execute with caller's privileges
                    ctx->pushSecurityContext(
                        ctx->getCurrentUserId(),
                        ctx->getActiveRoleId(),
                        ctx->isSuperuser(),
                        core::ConnectionContext::SecurityMode::INVOKER,
                        function_info.function_id
                    );
                    security_context_pushed = true;
                }
            }

            // Initialize variable stack if not already done
            if (!variable_stack_)
            {
                variable_stack_ = std::make_unique<VariableStack>();
            }

            // Push new frame for function
            variable_stack_->pushFrame();

            // Read parameters and bind to variables
            for (uint8_t i = 0; i < param_count; ++i)
            {
                uint8_t mode = readByte();  // IN, OUT, INOUT
                std::string param_name = readString();
                uint8_t type_code = readByte();

                // For now, parameters are passed on the stack
                // Pop value from stack and bind to variable
                if (!stack_.empty() && mode == 0)  // IN parameter
                {
                    Value param_value = pop();
                    variable_stack_->declareVariable(param_name, param_value);
                }
                else
                {
                    // OUT/INOUT parameters initialized to NULL
                    variable_stack_->declareVariable(param_name, Value());
                }
            }

            // Read return type
            uint8_t return_type = readByte();

            // Execute function body (should be a BLOCK)
            uint8_t block_opcode = readByte();
            if (block_opcode == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
            {
                uint8_t ext_opcode = readByte();
                if (ext_opcode == static_cast<uint8_t>(Opcode::EXT_BLOCK))
                {
                    executeBlock();
                }
            }

            // Pop function frame
            variable_stack_->popFrame();

            // Security Phase 3.1: Pop security context
            if (security_context_pushed && ctx)
            {
                ctx->popSecurityContext();
            }

            // Push return value onto stack
            if (return_requested_)
            {
                push(return_value_);
                return_requested_ = false;
            }
            else
            {
                push(Value());  // NULL if no return
            }
        }

        void Executor::executeProcedure()
        {
            // Read procedure name
            std::string procedure_name = readString();

            // Read parameter count
            uint8_t param_count = readByte();

            // Security Phase 3.1: Lookup procedure and check permissions
            core::CatalogManager::ProcedureInfo procedure_info;
            core::ErrorContext err_ctx;
            auto status = db_->catalog_manager()->getProcedure(procedure_name, procedure_info, &err_ctx);
            if (status != core::Status::OK)
            {
                error("Procedure not found: " + procedure_name);
            }

            // Check EXECUTE permission
            auto ctx = core::ConnectionContext::getCurrent();
            if (ctx && !ctx->isSuperuser())
            {
                if (!db_->catalog_manager()->hasObjectPermission(
                    procedure_info.procedure_id,
                    ctx->getCurrentUserId(),
                    0x0001, // PERM_EXECUTE
                    &err_ctx))
                {
                    error("Permission denied: EXECUTE on procedure " + procedure_name);
                }
            }

            // Security Phase 3.1: Push security context based on SQL SECURITY mode
            bool security_context_pushed = false;
            if (ctx)
            {
                if (procedure_info.sql_security == core::CatalogManager::ProcedureInfo::SqlSecurity::DEFINER)
                {
                    // Execute with owner's privileges
                    bool owner_is_superuser = false;
                    core::CatalogManager::UserInfo owner_info;
                    if (db_->catalog_manager()->getUser(procedure_info.owner_id, owner_info, &err_ctx) == core::Status::OK)
                    {
                        owner_is_superuser = owner_info.is_superuser;
                    }

                    ctx->pushSecurityContext(
                        procedure_info.owner_id,
                        core::ID(),  // No role for DEFINER mode
                        owner_is_superuser,
                        core::ConnectionContext::SecurityMode::DEFINER,
                        procedure_info.procedure_id
                    );
                    security_context_pushed = true;
                }
                else
                {
                    // INVOKER: Execute with caller's privileges
                    ctx->pushSecurityContext(
                        ctx->getCurrentUserId(),
                        ctx->getActiveRoleId(),
                        ctx->isSuperuser(),
                        core::ConnectionContext::SecurityMode::INVOKER,
                        procedure_info.procedure_id
                    );
                    security_context_pushed = true;
                }
            }

            // Initialize variable stack if not already done
            if (!variable_stack_)
            {
                variable_stack_ = std::make_unique<VariableStack>();
            }

            // Push new frame for procedure
            variable_stack_->pushFrame();

            // Read parameters and bind to variables
            for (uint8_t i = 0; i < param_count; ++i)
            {
                uint8_t mode = readByte();  // IN, OUT, INOUT
                std::string param_name = readString();
                uint8_t type_code = readByte();

                // Parameters passed on stack (IN) or initialized to NULL (OUT/INOUT)
                if (!stack_.empty() && mode == 0)  // IN parameter
                {
                    Value param_value = pop();
                    variable_stack_->declareVariable(param_name, param_value);
                }
                else
                {
                    variable_stack_->declareVariable(param_name, Value());
                }
            }

            // Execute procedure body (should be a BLOCK)
            uint8_t block_opcode = readByte();
            if (block_opcode == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
            {
                uint8_t ext_opcode = readByte();
                if (ext_opcode == static_cast<uint8_t>(Opcode::EXT_BLOCK))
                {
                    executeBlock();
                }
            }

            // Pop procedure frame
            variable_stack_->popFrame();

            // Security Phase 3.1: Pop security context
            if (security_context_pushed && ctx)
            {
                ctx->popSecurityContext();
            }

            // Procedures don't return values
            return_requested_ = false;
        }

        void Executor::executeBlock()
        {
            // Read variable declaration count
            uint8_t var_count = readByte();

            // Push new block frame
            if (!variable_stack_)
            {
                variable_stack_ = std::make_unique<VariableStack>();
            }
            variable_stack_->pushFrame();

            // Process variable declarations
            for (uint8_t i = 0; i < var_count; ++i)
            {
                // Should encounter EXT_DECLARE opcode
                uint8_t ext_marker = readByte();
                if (ext_marker == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
                {
                    uint8_t ext_opcode = readByte();
                    if (ext_opcode == static_cast<uint8_t>(Opcode::EXT_DECLARE))
                    {
                        executeVarDeclaration();
                    }
                }
            }

            // Execute statements until END or RETURN
            while (pc_ < bytecode_size_)
            {
                // Check for return request
                if (return_requested_)
                {
                    break;
                }

                uint8_t opcode = readByte();

                // Check for extended opcodes
                if (opcode == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
                {
                    uint8_t ext_opcode = readByte();

                    // Check for block end or other PSQL opcodes
                    if (ext_opcode == 0xFF)  // END marker (using 0xFF as END)
                    {
                        break;
                    }

                    // Handle PSQL statement opcodes
                    switch (static_cast<Opcode>(ext_opcode))
                    {
                        case Opcode::EXT_ASSIGN:
                            executeAssignment();
                            break;
                        case Opcode::EXT_IF:
                            executeIfStatement();
                            break;
                        case Opcode::EXT_LOOP:
                            executeLoopStatement();
                            break;
                        case Opcode::EXT_WHILE:
                            executeWhileStatement();
                            break;
                        case Opcode::EXT_EXIT:
                            executeExitStatement();
                            break;
                        case Opcode::EXT_RETURN:
                            executeReturnStatement();
                            break;
                        case Opcode::EXT_RAISE:
                            executeRaiseStatement();
                            break;
                        default:
                            // Unknown opcode, skip
                            break;
                    }
                }
                else
                {
                    // Regular SQL statement opcode - execute normally
                    pc_--;  // Back up to re-read opcode
                    break;  // Exit block processing
                }
            }

            // Pop block frame
            variable_stack_->popFrame();
        }

        void Executor::executeVarDeclaration()
        {
            // Read variable name
            std::string var_name = readString();

            // Read type code
            uint8_t type_code = readByte();

            // Read has_default flag
            bool has_default = readByte() != 0;

            Value default_value;
            if (has_default)
            {
                // Evaluate default expression (should be on stack or inline)
                evaluateExpression();
                default_value = pop();
            }

            // Declare variable in current frame
            variable_stack_->declareVariable(var_name, default_value);
        }

        void Executor::executeAssignment()
        {
            // Read variable name
            std::string var_name = readString();

            // Evaluate expression (pushes result onto stack)
            evaluateExpression();

            // Pop value and assign to variable
            Value value = pop();
            variable_stack_->setVariable(var_name, value);
        }

        void Executor::executeIfStatement()
        {
            // Evaluate condition (should already be on stack or needs evaluation)
            evaluateExpression();
            Value condition = pop();

            // Read jump offset (for false branch)
            uint32_t false_offset = readInt32();

            // Check condition
            bool condition_true = false;
            condition_true = condition.toBoolean();
            

            if (!condition_true)
            {
                // Jump to ELSE or END IF
                pc_ = false_offset;
            }
            // Otherwise, continue executing THEN block
        }

        void Executor::executeLoopStatement()
        {
            // Read loop end offset
            uint32_t loop_end_offset = readInt32();

            // Read optional label
            std::string label = readString();

            // Remember loop start
            size_t loop_start = pc_;

            // Push loop state
            loop_stack_.emplace_back(loop_start, loop_end_offset, label);

            // Execute loop body until EXIT
            while (pc_ < bytecode_size_)
            {
                // Check for exit request
                if (!loop_stack_.empty() && loop_stack_.back().exit_requested)
                {
                    loop_stack_.back().exit_requested = false;
                    break;
                }

                // Execute next statement
                uint8_t opcode = readByte();
                if (opcode == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
                {
                    uint8_t ext_opcode = readByte();

                    if (ext_opcode == 0xFE)  // END LOOP marker
                    {
                        // Loop back to start
                        pc_ = loop_start;
                        continue;
                    }

                    // Handle other opcodes...
                    pc_ -= 2;  // Back up
                    break;
                }
            }

            // Pop loop state
            if (!loop_stack_.empty())
            {
                loop_stack_.pop_back();
            }
        }

        void Executor::executeWhileStatement()
        {
            // Remember loop start
            size_t loop_start = pc_;

            // Read loop end offset
            uint32_t loop_end_offset = readInt32();

            // Read optional label
            std::string label = readString();

            // Push loop state
            loop_stack_.emplace_back(loop_start, loop_end_offset, label);

            // Execute while loop
            while (pc_ < bytecode_size_)
            {
                // Evaluate condition
                evaluateExpression();
                Value condition = pop();

                bool condition_true = false;
                condition_true = condition.toBoolean();

                if (!condition_true)
                {
                    // Exit loop
                    pc_ = loop_end_offset;
                    break;
                }

                // Check for exit request
                if (!loop_stack_.empty() && loop_stack_.back().exit_requested)
                {
                    loop_stack_.back().exit_requested = false;
                    pc_ = loop_end_offset;
                    break;
                }

                // Execute loop body (simplified - would need proper body parsing)
                // For now, just advance to next statement
                break;
            }

            // Pop loop state
            if (!loop_stack_.empty())
            {
                loop_stack_.pop_back();
            }
        }

        void Executor::executeExitStatement()
        {
            // Read optional label
            std::string label = readString();

            // Read optional WHEN condition flag
            bool has_when = readByte() != 0;

            if (has_when)
            {
                // Evaluate WHEN condition
                evaluateExpression();
                Value condition = pop();

                bool condition_true = false;
                condition_true = condition.toBoolean();

                if (!condition_true)
                {
                    return;  // Don't exit
                }
            }

            // Find matching loop
            if (label.empty())
            {
                // Exit innermost loop
                if (!loop_stack_.empty())
                {
                    loop_stack_.back().exit_requested = true;
                    pc_ = loop_stack_.back().loop_end_pc;
                }
            }
            else
            {
                // Exit labeled loop
                for (auto it = loop_stack_.rbegin(); it != loop_stack_.rend(); ++it)
                {
                    if (it->label == label)
                    {
                        it->exit_requested = true;
                        pc_ = it->loop_end_pc;
                        break;
                    }
                }
            }
        }

        void Executor::executeReturnStatement()
        {
            // Read has_value flag
            bool has_value = readByte() != 0;

            if (has_value)
            {
                // Evaluate return expression
                evaluateExpression();
                return_value_ = pop();
            }
            else
            {
                return_value_ = Value();  // NULL
            }

            return_requested_ = true;
        }

        void Executor::executeRaiseStatement()
        {
            // Read exception level (EXCEPTION, NOTICE, WARNING, etc.)
            uint8_t level = readByte();

            // Read message
            std::string message = readString();

            // Check for re-raise (empty message)
            if (message.empty() && has_current_exception_)
            {
                // Re-raise the current exception
                message = current_exception_message_;
                // arg_count will be 0 for re-raise
            }

            // Read argument count
            uint8_t arg_count = readByte();

            // Evaluate arguments
            std::vector<Value> args;
            for (uint8_t i = 0; i < arg_count; ++i)
            {
                evaluateExpression();
                args.push_back(pop());
            }

            // Format message with arguments (simple implementation)
            // In a real implementation, would use printf-style formatting
            std::string formatted_message = message;
            for (size_t i = 0; i < args.size(); ++i)
            {
                // Simple placeholder replacement: %1, %2, etc.
                std::string placeholder = "%" + std::to_string(i + 1);
                size_t pos = formatted_message.find(placeholder);
                if (pos != std::string::npos)
                {
                    formatted_message.replace(pos, placeholder.length(), args[i].toString());
                }
            }

            // PSQL exception levels
            // 0 = EXCEPTION (error), 1 = WARNING, 2 = NOTICE, 3 = LOG, 4 = INFO, 5 = DEBUG

            // For WARNING, NOTICE, LOG, INFO, DEBUG - just log and continue
            if (level >= 1)
            {
                // In a real implementation, would log to appropriate channel
                // For now, just continue execution
                return;
            }

            // Level 0 = EXCEPTION - search for exception handler
            std::string exception_name = "EXCEPTION";  // Default exception type

            // Search exception stack from top (innermost) to bottom (outermost)
            for (auto it = exception_stack_.rbegin(); it != exception_stack_.rend(); ++it)
            {
                const ExceptionFrame& frame = *it;

                // Search handlers in this frame
                for (const auto& handler : frame.handlers)
                {
                    const std::string& handler_exception = handler.first;
                    uint32_t handler_pc = handler.second;

                    // Check if handler matches (exact match or "ALL" catches everything)
                    if (handler_exception == exception_name || handler_exception == "ALL")
                    {
                        // Found matching handler - jump to it
                        // Store exception message in a special variable for handler to access
                        if (variable_stack_)
                        {
                            variable_stack_->declareVariable("SQLSTATE", Value::makeText(formatted_message));
                            variable_stack_->declareVariable("SQLERRM", Value::makeText(formatted_message));
                        }

                        // Jump to handler
                        pc_ = handler_pc;
                        return;
                    }
                }
            }

            // No handler found - throw C++ exception to propagate up
            throw std::runtime_error("PSQL EXCEPTION: " + formatted_message);
        }

        // PSQL Variable Operations

        void Executor::executeVarLoad()
        {
            // Read variable name
            std::string var_name = readString();

            // Load variable value onto stack
            if (variable_stack_ && variable_stack_->hasVariable(var_name))
            {
                push(variable_stack_->getVariable(var_name));
            }
            else
            {
                error("Variable not found: " + var_name);
            }
        }

        void Executor::executeVarStore()
        {
            // Read variable name
            std::string var_name = readString();

            // Pop value from stack
            Value value = pop();

            // Store to variable
            if (variable_stack_)
            {
                variable_stack_->setVariable(var_name, value);
            }
            else
            {
                error("No variable stack available");
            }
        }

        // PSQL Control Flow Helpers

        void Executor::executeJump()
        {
            // Read jump offset
            uint32_t offset = readInt32();
            pc_ = offset;
        }

        void Executor::executeJumpIfTrue()
        {
            // Read jump offset
            uint32_t offset = readInt32();

            // Pop condition
            Value condition = pop();

            // Use toBoolean() which handles various value types
            bool is_true = condition.toBoolean();

            if (is_true)
            {
                pc_ = offset;
            }
        }

        void Executor::executeJumpIfFalse()
        {
            // Read jump offset
            uint32_t offset = readInt32();

            // Pop condition
            Value condition = pop();

            // Use toBoolean() which handles various value types
            bool is_false = !condition.toBoolean();

            if (is_false)
            {
                pc_ = offset;
            }
        }

        // ===== PSQL Exception Handling =====

        void Executor::executeTryStatement()
        {
            // Read TRY block end offset
            uint32_t try_end_pc = readInt32();

            // Read exception handler count
            uint8_t handler_count = readByte();

            // Create exception frame
            ExceptionFrame frame(pc_, try_end_pc);

            // Read exception handlers
            for (uint8_t i = 0; i < handler_count; ++i)
            {
                std::string exception_name = readString();
                uint32_t handler_pc = readInt32();
                frame.handlers.emplace_back(exception_name, handler_pc);
            }

            // Push exception frame onto stack
            exception_stack_.push_back(frame);

            // Remember the original stack size (for cleanup on exception)
            size_t original_exception_stack_size = exception_stack_.size();

            try
            {
                // Execute TRY block (bytecode execution continues normally)
                // The TRY block ends at try_end_pc
                // Execution will naturally flow through the TRY block
                // and the exception handlers are registered for any RAISE that occurs
            }
            catch (const std::exception& e)
            {
                // C++ exception caught - this could be from RAISE or other errors
                // Find matching exception handler
                std::string exception_msg = e.what();

                // Pop exception frame
                if (exception_stack_.size() >= original_exception_stack_size)
                {
                    exception_stack_.pop_back();
                }

                // Re-throw if no handler found (handled by executeRaiseStatement)
                throw;
            }

            // Note: Exception frame is NOT popped here because we want it active
            // during TRY block execution. It will be popped when we exit the TRY block
            // or when an exception is caught.
        }

        void Executor::executeExceptHandler()
        {
            // Read exception name (what exception this handler catches)
            std::string exception_name = readString();

            // Read handler end offset
            uint32_t handler_end_pc = readInt32();

            // Set current exception for re-raise support
            // The exception message should be in SQLERRM variable
            if (variable_stack_ && variable_stack_->hasVariable("SQLERRM"))
            {
                has_current_exception_ = true;
                current_exception_message_ = variable_stack_->getVariable("SQLERRM").toString();
            }

            // Execute handler block
            // The bytecode execution will continue normally through the handler
            // and stop at handler_end_pc

            // Pop the exception frame since we're now in the handler
            if (!exception_stack_.empty())
            {
                exception_stack_.pop_back();
            }

            // Clear current exception when handler completes
            // (This should actually be done when exiting the handler, but for simplicity
            // we'll clear it here. A more robust implementation would track handler scope.)
        }

        // ===== Security Statements (ALPHA Phase 1 - Security System Phase 2) =====

        void Executor::executeCreateUser()
        {
            // Decode bytecode
            std::string username = readString();
            uint8_t flags = readByte();
            bool has_password = flags & 0x01;
            bool is_superuser = flags & 0x02;
            std::string password;
            if (has_password)
            {
                password = readString();
            }

            // Permission check: Only superusers can create users
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                error("Permission denied: CREATE USER (superuser only)");
            }

            // Hash password if provided (Security Phase 3.0)
            std::string password_hash;
            if (has_password)
            {
                try
                {
                    password_hash = core::PasswordHash::hashPassword(password);
                }
                catch (const std::exception& e)
                {
                    // SECURITY FIX (HIGH-1): Log detailed error internally, return generic error to client
                    LOG_ERROR(EXECUTOR, "Password hashing failed during CREATE USER: %s", e.what());
                    error("Password hashing failed");
                }
            }

            // Call catalog manager
            core::ID user_id;
            core::ID default_schema_id;  // TODO: Get from database or use a default
            // For now, use zero UUID as default schema (will need proper handling)
            std::memset(&default_schema_id, 0, sizeof(default_schema_id));

            core::ErrorContext err_ctx;
            auto status = db_->catalog_manager()->createUser(
                username, password_hash, default_schema_id, is_superuser, user_id, &err_ctx);

            if (status != core::Status::OK)
            {
                error("CREATE USER failed: " + std::string("Operation failed"));
            }
        }

        void Executor::executeAlterUser()
        {
            // Decode bytecode
            std::string username = readString();
            uint8_t flags = readByte();
            bool change_password = flags & 0x01;
            bool change_superuser = flags & 0x02;
            bool is_superuser = flags & 0x04;
            std::string password;
            if (change_password)
            {
                password = readString();
            }

            // Permission check: Only superusers can alter users
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                error("Permission denied: ALTER USER (superuser only)");
            }

            // Look up user by name
            core::CatalogManager::UserInfo user_info;
            core::ErrorContext err_ctx;
            auto get_status = db_->catalog_manager()->getUserByName(
                username, user_info, &err_ctx);

            if (get_status != core::Status::OK)
            {
                error("User '" + username + "' not found");
            }

            // Prepare password hash if changing (Security Phase 3.0)
            std::string password_hash = user_info.password_hash;  // Keep existing if not changing
            if (change_password)
            {
                try
                {
                    password_hash = core::PasswordHash::hashPassword(password);
                }
                catch (const std::exception& e)
                {
                    // SECURITY FIX (HIGH-1): Log detailed error internally, return generic error to client
                    LOG_ERROR(EXECUTOR, "Password hashing failed during ALTER USER: %s", e.what());
                    error("Password hashing failed");
                }
            }

            // Prepare updated fields (Security Phase 3.0)
            core::ID default_schema_id = user_info.default_schema_id;
            bool is_active = user_info.is_active;
            bool is_superuser_updated = change_superuser ? is_superuser : user_info.is_superuser;

            // Call catalog manager update
            auto status = db_->catalog_manager()->updateUser(
                user_info.user_id, password_hash, default_schema_id, is_active,
                is_superuser_updated, &err_ctx);

            if (status != core::Status::OK)
            {
                error("ALTER USER failed: " + std::string("Operation failed"));
            }
        }

        void Executor::executeDropUser()
        {
            // Decode bytecode
            std::string username = readString();
            uint8_t flags = readByte();
            bool if_exists = flags & 0x01;
            bool cascade = flags & 0x02;

            // Security Check: Only superusers can drop users
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                error("Permission denied: only superusers can drop users");
                return;
            }

            // Look up user by name
            core::CatalogManager::UserInfo user_info;
            core::ErrorContext err_ctx;
            auto get_status = db_->catalog_manager()->getUserByName(
                username, user_info, &err_ctx);

            if (get_status != core::Status::OK)
            {
                if (if_exists)
                {
                    // Silently succeed if IF EXISTS specified
                    return;
                }
                error("User '" + username + "' not found");
            }

            // Security Phase 3.0: CASCADE support
            auto status = db_->catalog_manager()->deleteUser(
                user_info.user_id, cascade, &err_ctx);

            if (status != core::Status::OK)
            {
                error("DROP USER failed: " + std::string("Operation failed"));
            }

            // Security Phase 3.2.3: Invalidate all cache entries for this user
            // User no longer exists, so all cached permissions are now invalid
            db_->permission_cache()->invalidateUser(user_info.user_id);
        }

        void Executor::executeCreateRole()
        {
            // Decode bytecode
            std::string rolename = readString();

            // Security Check: Only superusers can create roles
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                error("Permission denied: only superusers can create roles");
                return;
            }

            // Get current user ID as the owner
            // TODO: Get from connection context when implemented
            core::ID owner_id;
            std::memset(&owner_id, 0, sizeof(owner_id)); // Placeholder: system user

            // Call catalog manager
            core::ID role_uuid;
            core::ErrorContext err_ctx;
            auto status = db_->catalog_manager()->createRole(
                rolename, owner_id, role_uuid, &err_ctx);

            if (status != core::Status::OK)
            {
                error("CREATE ROLE failed: " + std::string("Operation failed"));
            }
        }

        void Executor::executeDropRole()
        {
            // Decode bytecode
            std::string rolename = readString();
            uint8_t flags = readByte();
            bool if_exists = flags & 0x01;
            bool cascade = flags & 0x02;

            // Security Check: Only superusers can drop roles
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                error("Permission denied: only superusers can drop roles");
                return;
            }

            // Look up role by name
            core::CatalogManager::RoleInfo role_info;
            core::ErrorContext err_ctx;
            auto get_status = db_->catalog_manager()->getRoleByName(
                rolename, role_info, &err_ctx);

            if (get_status != core::Status::OK)
            {
                if (if_exists)
                {
                    return; // Silently succeed
                }
                error("Role '" + rolename + "' not found");
            }

            // Security Phase 3.0: CASCADE support
            auto status = db_->catalog_manager()->deleteRole(
                role_info.role_id, cascade, &err_ctx);

            if (status != core::Status::OK)
            {
                error("DROP ROLE failed: " + std::string("Operation failed"));
            }

            // Security Phase 3.2.3: Invalidate entire cache for role drops
            // Role memberships affect many users, so safer to invalidate everything
            db_->permission_cache()->invalidateAll();
        }

        void Executor::executeCreateGroup()
        {
            // Decode bytecode
            std::string groupname = readString();

            // Security Check: Only superusers can create groups
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                error("Permission denied: only superusers can create groups");
                return;
            }

            // Call catalog manager
            core::ID group_uuid;
            core::ErrorContext err_ctx;
            // Default to LOCAL group type, empty external_id
            auto status = db_->catalog_manager()->createGroup(
                groupname, core::CatalogManager::GroupType::LOCAL, "", group_uuid, &err_ctx);

            if (status != core::Status::OK)
            {
                error("CREATE GROUP failed: " + std::string("Operation failed"));
            }
        }

        void Executor::executeDropGroup()
        {
            // Decode bytecode
            std::string groupname = readString();
            uint8_t flags = readByte();
            bool if_exists = flags & 0x01;
            bool cascade = flags & 0x02;

            // Security Check: Only superusers can drop groups
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                error("Permission denied: only superusers can drop groups");
                return;
            }

            // Look up group by name
            core::CatalogManager::GroupInfo group_info;
            core::ErrorContext err_ctx;
            auto get_status = db_->catalog_manager()->getGroupByName(
                groupname, group_info, &err_ctx);

            if (get_status != core::Status::OK)
            {
                if (if_exists)
                {
                    return; // Silently succeed
                }
                error("Group '" + groupname + "' not found");
            }

            // Security Phase 3.0: CASCADE support
            auto status = db_->catalog_manager()->deleteGroup(
                group_info.group_id, cascade, &err_ctx);

            if (status != core::Status::OK)
            {
                error("DROP GROUP failed: " + std::string("Operation failed"));
            }

            // Security Phase 3.2.3: Invalidate entire cache for group drops
            // Group memberships affect many users, so safer to invalidate everything
            db_->permission_cache()->invalidateAll();
        }

        void Executor::executeGrantPrivilege()
        {
            // Decode bytecode
            uint32_t privileges = readInt32();
            uint8_t object_type_byte = readByte();
            std::string object_name = readString();
            uint8_t grantee_type_byte = readByte();
            std::string grantee_name = readString();
            uint8_t flags = readByte();
            bool with_grant_option = flags & 0x01;
            bool has_column_list = flags & 0x02;  // Security Phase 3.3.4

            // Security Phase 3.3.4: Decode column list if present
            std::vector<std::string> column_names;
            if (has_column_list)
            {
                uint32_t column_count = readInt32();
                column_names.reserve(column_count);
                for (uint32_t i = 0; i < column_count; ++i)
                {
                    column_names.push_back(readString());
                }
            }

            core::ErrorContext err_ctx;

            // Convert object_type_byte to enum
            core::CatalogManager::PermissionObjectType object_type =
                static_cast<core::CatalogManager::PermissionObjectType>(object_type_byte);

            // Look up object ID based on object type
            // TODO: For now, only TABLE is supported. Need schema-qualified name handling.
            // Placeholder: use zero UUID for current schema
            core::ID object_id;
            if (object_type == core::CatalogManager::PermissionObjectType::TABLE)
            {
                core::ID schema_id;
                std::memset(&schema_id, 0, sizeof(schema_id)); // TODO: Get actual current schema
                core::CatalogManager::TableInfo table_info;
                auto get_obj_status = db_->catalog_manager()->getTable(
                    schema_id, object_name, table_info, &err_ctx);
                if (get_obj_status != core::Status::OK)
                {
                    error("Table '" + object_name + "' not found");
                }
                object_id = table_info.table_id;

                // Security Check: Only superusers or object owners can grant
                if (conn_ctx_ && !conn_ctx_->isSuperuser())
                {
                    // Check if current user is the table owner
                    bool is_owner = (std::memcmp(&conn_ctx_->getCurrentUserId(),
                                                  &table_info.owner_id,
                                                  sizeof(core::ID)) == 0);
                    if (!is_owner)
                    {
                        error("Permission denied: only superusers or table owners can grant privileges");
                        return;
                    }
                }
            }
            else
            {
                // TODO: Implement lookup for other object types
                error("Object type not yet supported: " + std::to_string(object_type_byte));
            }

            // Look up grantee ID based on grantee type
            core::ID grantee_id;
            core::CatalogManager::GranteeType grantee_type =
                static_cast<core::CatalogManager::GranteeType>(grantee_type_byte);

            if (grantee_type == core::CatalogManager::GranteeType::USER)
            {
                core::CatalogManager::UserInfo user_info;
                auto get_grantee = db_->catalog_manager()->getUserByName(
                    grantee_name, user_info, &err_ctx);
                if (get_grantee != core::Status::OK)
                {
                    error("User '" + grantee_name + "' not found");
                }
                grantee_id = user_info.user_id;
            }
            else if (grantee_type == core::CatalogManager::GranteeType::ROLE)
            {
                core::CatalogManager::RoleInfo role_info;
                auto get_grantee = db_->catalog_manager()->getRoleByName(
                    grantee_name, role_info, &err_ctx);
                if (get_grantee != core::Status::OK)
                {
                    error("Role '" + grantee_name + "' not found");
                }
                grantee_id = role_info.role_id;
            }
            else if (grantee_type == core::CatalogManager::GranteeType::GROUP)
            {
                core::CatalogManager::GroupInfo group_info;
                auto get_grantee = db_->catalog_manager()->getGroupByName(
                    grantee_name, group_info, &err_ctx);
                if (get_grantee != core::Status::OK)
                {
                    error("Group '" + grantee_name + "' not found");
                }
                grantee_id = group_info.group_id;
            }
            else if (grantee_type == core::CatalogManager::GranteeType::PUBLIC)
            {
                // PUBLIC uses zero UUID
                std::memset(&grantee_id, 0, sizeof(grantee_id));
            }
            else
            {
                error("Invalid grantee type: " + std::to_string(grantee_type_byte));
            }

            // Get grantor ID (current user)
            // TODO: Get from connection context when implemented
            core::ID grantor_id;
            std::memset(&grantor_id, 0, sizeof(grantor_id)); // Placeholder: system user

            // Security Phase 3.3.4: Branch based on column-level vs table-level
            core::Status status;
            if (has_column_list && !column_names.empty())
            {
                // Column-level permissions - grant for each column
                for (const auto& column_name : column_names)
                {
                    status = db_->catalog_manager()->grantColumnPermission(
                        object_id, column_name, grantee_id, grantee_type,
                        privileges, with_grant_option, grantor_id, &err_ctx);

                    if (status != core::Status::OK)
                    {
                        error("GRANT PRIVILEGE on column '" + column_name + "' failed");
                    }
                }
            }
            else
            {
                // Table-level permission
                status = db_->catalog_manager()->grantPermission(
                    object_id, object_type, grantee_id, grantee_type,
                    privileges, with_grant_option, grantor_id, &err_ctx);

                if (status != core::Status::OK)
                {
                    error("GRANT PRIVILEGE failed: " + std::string("Operation failed"));
                }
            }

            // Security Phase 3.2.3: Invalidate permission cache for affected user and object
            // This ensures subsequent permission checks will fetch fresh data from catalog
            db_->permission_cache()->invalidateUser(grantee_id);
            db_->permission_cache()->invalidateObject(object_id);
        }

        void Executor::executeRevokePrivilege()
        {
            // Decode bytecode
            uint32_t privileges = readInt32();
            uint8_t object_type_byte = readByte();
            std::string object_name = readString();
            uint8_t grantee_type_byte = readByte();
            std::string grantee_name = readString();
            uint8_t flags = readByte();
            bool cascade = flags & 0x01;
            bool has_column_list = flags & 0x02;  // Security Phase 3.3.4

            // Security Phase 3.3.4: Decode column list if present
            std::vector<std::string> column_names;
            if (has_column_list)
            {
                uint32_t column_count = readInt32();
                column_names.reserve(column_count);
                for (uint32_t i = 0; i < column_count; ++i)
                {
                    column_names.push_back(readString());
                }
            }

            core::ErrorContext err_ctx;

            // Convert object_type_byte to enum
            core::CatalogManager::PermissionObjectType object_type =
                static_cast<core::CatalogManager::PermissionObjectType>(object_type_byte);

            // Look up object ID based on object type
            // TODO: For now, only TABLE is supported. Need schema-qualified name handling.
            core::ID object_id;
            if (object_type == core::CatalogManager::PermissionObjectType::TABLE)
            {
                core::ID schema_id;
                std::memset(&schema_id, 0, sizeof(schema_id)); // TODO: Get actual current schema
                core::CatalogManager::TableInfo table_info;
                auto get_obj_status = db_->catalog_manager()->getTable(
                    schema_id, object_name, table_info, &err_ctx);
                if (get_obj_status != core::Status::OK)
                {
                    error("Table '" + object_name + "' not found");
                }
                object_id = table_info.table_id;

                // Security Check: Only superusers or object owners can revoke
                if (conn_ctx_ && !conn_ctx_->isSuperuser())
                {
                    // Check if current user is the table owner
                    bool is_owner = (std::memcmp(&conn_ctx_->getCurrentUserId(),
                                                  &table_info.owner_id,
                                                  sizeof(core::ID)) == 0);
                    if (!is_owner)
                    {
                        error("Permission denied: only superusers or table owners can revoke privileges");
                        return;
                    }
                }
            }
            else
            {
                // TODO: Implement lookup for other object types
                error("Object type not yet supported: " + std::to_string(object_type_byte));
            }

            // Look up grantee ID based on grantee type
            core::ID grantee_id;
            core::CatalogManager::GranteeType grantee_type =
                static_cast<core::CatalogManager::GranteeType>(grantee_type_byte);

            if (grantee_type == core::CatalogManager::GranteeType::USER)
            {
                core::CatalogManager::UserInfo user_info;
                auto get_grantee = db_->catalog_manager()->getUserByName(
                    grantee_name, user_info, &err_ctx);
                if (get_grantee != core::Status::OK)
                {
                    error("User '" + grantee_name + "' not found");
                }
                grantee_id = user_info.user_id;
            }
            else if (grantee_type == core::CatalogManager::GranteeType::ROLE)
            {
                core::CatalogManager::RoleInfo role_info;
                auto get_grantee = db_->catalog_manager()->getRoleByName(
                    grantee_name, role_info, &err_ctx);
                if (get_grantee != core::Status::OK)
                {
                    error("Role '" + grantee_name + "' not found");
                }
                grantee_id = role_info.role_id;
            }
            else if (grantee_type == core::CatalogManager::GranteeType::GROUP)
            {
                core::CatalogManager::GroupInfo group_info;
                auto get_grantee = db_->catalog_manager()->getGroupByName(
                    grantee_name, group_info, &err_ctx);
                if (get_grantee != core::Status::OK)
                {
                    error("Group '" + grantee_name + "' not found");
                }
                grantee_id = group_info.group_id;
            }
            else if (grantee_type == core::CatalogManager::GranteeType::PUBLIC)
            {
                // PUBLIC uses zero UUID
                std::memset(&grantee_id, 0, sizeof(grantee_id));
            }
            else
            {
                error("Invalid grantee type: " + std::to_string(grantee_type_byte));
            }

            // Security Phase 3.3.4: Branch based on column-level vs table-level
            core::Status status;
            if (has_column_list && !column_names.empty())
            {
                // Column-level permissions - revoke for each column
                for (const auto& column_name : column_names)
                {
                    status = db_->catalog_manager()->revokeColumnPermission(
                        object_id, column_name, grantee_id, grantee_type,
                        privileges, &err_ctx);

                    if (status != core::Status::OK)
                    {
                        error("REVOKE PRIVILEGE on column '" + column_name + "' failed");
                    }
                }
            }
            else
            {
                // Table-level permission
                // TODO: CASCADE option not yet implemented in catalog manager
                status = db_->catalog_manager()->revokePermission(
                    object_id, object_type, grantee_id, grantee_type,
                    privileges, &err_ctx);

                if (status != core::Status::OK)
                {
                    error("REVOKE PRIVILEGE failed: " + std::string("Operation failed"));
                }
            }

            // Security Phase 3.2.3: Invalidate permission cache for affected user and object
            // This ensures subsequent permission checks will fetch fresh data from catalog
            db_->permission_cache()->invalidateUser(grantee_id);
            db_->permission_cache()->invalidateObject(object_id);
        }

        void Executor::executeGrantRole()
        {
            // Decode bytecode
            std::string rolename = readString();
            uint8_t grantee_type_byte = readByte();
            std::string grantee_name = readString();

            // Security Check: Only superusers can grant roles
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                error("Permission denied: only superusers can grant roles");
                return;
            }

            core::ErrorContext err_ctx;

            // Look up role by name
            core::CatalogManager::RoleInfo role_info;
            auto get_role = db_->catalog_manager()->getRoleByName(
                rolename, role_info, &err_ctx);
            if (get_role != core::Status::OK)
            {
                error("Role '" + rolename + "' not found");
            }

            // For now, only USER grantees are supported (SQL standard doesn't allow GRANT ROLE TO ROLE)
            // However, some databases do support it, so we check the type
            if (grantee_type_byte != 0) // 0 = USER
            {
                error("GRANT ROLE only supports USER grantees (got type: " + std::to_string(grantee_type_byte) + ")");
            }

            // Look up user by name
            core::CatalogManager::UserInfo user_info;
            auto get_grantee = db_->catalog_manager()->getUserByName(
                grantee_name, user_info, &err_ctx);
            if (get_grantee != core::Status::OK)
            {
                error("User '" + grantee_name + "' not found");
            }

            // Get grantor ID (current user)
            // TODO: Get from connection context when implemented
            core::ID granted_by;
            std::memset(&granted_by, 0, sizeof(granted_by)); // Placeholder: system user

            // TODO: WITH ADMIN OPTION not yet implemented in bytecode
            bool with_admin_option = false;

            // Grant role to user
            auto status = db_->catalog_manager()->grantRole(
                role_info.role_id, user_info.user_id, granted_by, with_admin_option, &err_ctx);

            if (status != core::Status::OK)
            {
                error("GRANT ROLE failed: " + std::string("Operation failed"));
            }
        }

        void Executor::executeRevokeRole()
        {
            // Decode bytecode
            std::string rolename = readString();
            uint8_t grantee_type_byte = readByte();
            std::string grantee_name = readString();
            uint8_t flags = readByte();
            bool cascade = flags & 0x01;

            // Security Check: Only superusers can revoke roles
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                error("Permission denied: only superusers can revoke roles");
                return;
            }

            core::ErrorContext err_ctx;

            // Look up role by name
            core::CatalogManager::RoleInfo role_info;
            auto get_role = db_->catalog_manager()->getRoleByName(
                rolename, role_info, &err_ctx);
            if (get_role != core::Status::OK)
            {
                error("Role '" + rolename + "' not found");
            }

            // For now, only USER grantees are supported (matching GRANT ROLE behavior)
            if (grantee_type_byte != 0) // 0 = USER
            {
                error("REVOKE ROLE only supports USER grantees (got type: " + std::to_string(grantee_type_byte) + ")");
            }

            // Look up user by name
            core::CatalogManager::UserInfo user_info;
            auto get_grantee = db_->catalog_manager()->getUserByName(
                grantee_name, user_info, &err_ctx);
            if (get_grantee != core::Status::OK)
            {
                error("User '" + grantee_name + "' not found");
            }

            // TODO: CASCADE option not yet implemented in catalog manager
            // Revoke role from user
            auto status = db_->catalog_manager()->revokeRole(
                role_info.role_id, user_info.user_id, &err_ctx);

            if (status != core::Status::OK)
            {
                error("REVOKE ROLE failed: " + std::string("Operation failed"));
            }
        }

        void Executor::executeSetRole()
        {
            // Decode bytecode
            uint8_t flags = readByte();
            bool is_reset = flags & 0x01;

            // Check connection context is available
            if (!conn_ctx_)
            {
                error("SET ROLE requires connection context");
            }

            if (is_reset)
            {
                // RESET ROLE: Clear active role
                conn_ctx_->clearActiveRole();
            }
            else
            {
                // SET ROLE: Set active role
                std::string rolename = readString();

                // Look up role by name
                core::CatalogManager::RoleInfo role_info;
                core::ErrorContext err_ctx;
                auto get_role = db_->catalog_manager()->getRoleByName(
                    rolename, role_info, &err_ctx);

                if (get_role != core::Status::OK)
                {
                    error("Role '" + rolename + "' not found");
                }

                // Verify user has been granted this role
                const core::ID& current_user = conn_ctx_->getCurrentUserId();
                std::vector<core::CatalogManager::RoleMembershipInfo> user_roles;
                auto check_status = db_->catalog_manager()->getUserRoles(
                    current_user, user_roles, &err_ctx);

                if (check_status != core::Status::OK)
                {
                    error("Failed to check role membership for user");
                }

                // Check if role_info.role_id is in user_roles
                bool has_role = false;
                for (const auto& membership : user_roles)
                {
                    if (membership.role_id == role_info.role_id)
                    {
                        has_role = true;
                        break;
                    }
                }

                if (!has_role)
                {
                    error("Permission denied: Role '" + rolename + "' not granted to current user");
                }

                // Update session with active role
                conn_ctx_->setActiveRole(role_info.role_id);
            }
        }

        void Executor::executeSetSessionAuth()
        {
            // Decode bytecode
            uint8_t flags = readByte();
            bool is_reset = flags & 0x01;

            // Check connection context is available
            if (!conn_ctx_)
            {
                error("SET SESSION AUTHORIZATION requires connection context");
            }

            // Permission check: Only superusers can change session authorization
            if (!conn_ctx_->isSuperuser())
            {
                error("Permission denied: SET SESSION AUTHORIZATION (superuser only)");
            }

            // TODO: Implement session user tracking
            // For now, SET SESSION AUTHORIZATION is not fully implemented because:
            // 1. We need to track the "original" connection user separately from "effective" user
            // 2. RESET SESSION AUTHORIZATION should restore to the original user
            // 3. This requires extending ConnectionContext with original_user_id_ field
            //
            // Placeholder behavior for now:
            if (is_reset)
            {
                // RESET SESSION AUTHORIZATION: Not yet implemented
                error("RESET SESSION AUTHORIZATION not yet implemented (requires session user tracking)");
            }
            else
            {
                // SET SESSION AUTHORIZATION: Not yet implemented
                std::string username = readString();
                error("SET SESSION AUTHORIZATION not yet implemented (requires session user tracking)");
            }
        }

        // Security Phase 3.4.4 - Row-Level Security Policy Execution
        void Executor::executeCreatePolicy()
        {
            // Decode bytecode
            std::string policy_name = readString();
            std::string table_name = readString();
            uint8_t policy_command = readByte();

            // Read role count and roles
            uint32_t role_count = readInt32();
            std::vector<std::string> roles;
            roles.reserve(role_count);
            for (uint32_t i = 0; i < role_count; i++)
            {
                roles.push_back(readString());
            }

            uint8_t flags = readByte();
            bool has_using_expr = flags & 0x01;
            bool has_with_check_expr = flags & 0x02;

            // Phase 3.5: Read expression bytecode and serialize to string for TOAST storage
            // The expressions are stored as SBLR bytecode, which will be evaluated at DML time
            // For now, we serialize the bytecode to a string format for catalog storage
            std::string using_expr;
            std::string with_check_expr;

            if (has_using_expr)
            {
                // Read expression bytecode - the expression is already in SBLR format
                // We need to serialize this bytecode for storage in the catalog
                size_t expr_start = pc_;

                // Evaluate the expression structure to find its end
                // This will skip over the expression bytecode
                evaluateExpression();

                size_t expr_end = pc_;
                size_t expr_length = expr_end - expr_start;

                // Serialize bytecode as hex string for catalog storage
                // Format: "0xXXXXXX..." representing the SBLR bytecode
                using_expr.reserve(2 + expr_length * 2);
                using_expr = "0x";
                for (size_t i = expr_start; i < expr_end; i++)
                {
                    char buf[3];
                    snprintf(buf, sizeof(buf), "%02x", bytecode_[i]);
                    using_expr += buf;
                }
            }

            if (has_with_check_expr)
            {
                // Read WITH CHECK expression bytecode
                size_t expr_start = pc_;

                // Evaluate the expression structure to find its end
                evaluateExpression();

                size_t expr_end = pc_;
                size_t expr_length = expr_end - expr_start;

                // Serialize bytecode as hex string for catalog storage
                with_check_expr.reserve(2 + expr_length * 2);
                with_check_expr = "0x";
                for (size_t i = expr_start; i < expr_end; i++)
                {
                    char buf[3];
                    snprintf(buf, sizeof(buf), "%02x", bytecode_[i]);
                    with_check_expr += buf;
                }
            }

            // Look up schema and table first
            core::CatalogManager::SchemaInfo schema_info;
            auto schema_status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (schema_status != core::Status::OK)
            {
                error("Failed to get schema PUBLIC");
            }

            core::CatalogManager::TableInfo table_info;
            core::ErrorContext err_ctx;
            auto get_status = db_->catalog_manager()->getTable(
                schema_info.schema_id, table_name, table_info, &err_ctx);

            if (get_status != core::Status::OK)
            {
                error("Table '" + table_name + "' not found");
            }

            // Security Check: Only superusers or table owners can create policies
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                // Check if current user is the table owner
                bool is_owner = (std::memcmp(&conn_ctx_->getCurrentUserId(),
                                              &table_info.owner_id,
                                              sizeof(core::ID)) == 0);
                if (!is_owner)
                {
                    error("Permission denied: only superusers or table owners can create policies");
                    return;
                }
            }

            // Convert policy command to PolicyType
            core::CatalogManager::PolicyType policy_type;
            switch (policy_command)
            {
            case 0: policy_type = core::CatalogManager::PolicyType::ALL; break;
            case 1: policy_type = core::CatalogManager::PolicyType::SELECT; break;
            case 2: policy_type = core::CatalogManager::PolicyType::INSERT; break;
            case 3: policy_type = core::CatalogManager::PolicyType::UPDATE; break;
            case 4: policy_type = core::CatalogManager::PolicyType::DELETE; break;
            default: error("Invalid policy command: " + std::to_string(policy_command));
            }

            // Create policy
            core::ID policy_id;
            auto status = db_->catalog_manager()->createPolicy(
                table_info.table_id, policy_name, policy_type, roles,
                using_expr, with_check_expr, policy_id, &err_ctx);

            if (status != core::Status::OK)
            {
                error("CREATE POLICY failed: Operation failed");
            }
        }

        void Executor::executeDropPolicy()
        {
            // Decode bytecode
            std::string policy_name = readString();
            std::string table_name = readString();
            uint8_t flags = readByte();
            bool if_exists = flags & 0x01;
            bool cascade = flags & 0x02;

            // Look up schema and table first
            core::CatalogManager::SchemaInfo schema_info;
            auto schema_status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (schema_status != core::Status::OK)
            {
                error("Failed to get schema PUBLIC");
            }

            core::CatalogManager::TableInfo table_info;
            core::ErrorContext err_ctx;
            auto get_status = db_->catalog_manager()->getTable(
                schema_info.schema_id, table_name, table_info, &err_ctx);

            if (get_status != core::Status::OK)
            {
                if (if_exists)
                {
                    return; // Silently succeed if IF EXISTS specified
                }
                error("Table '" + table_name + "' not found");
            }

            // Security Check: Only superusers or table owners can drop policies
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                // Check if current user is the table owner
                bool is_owner = (std::memcmp(&conn_ctx_->getCurrentUserId(),
                                              &table_info.owner_id,
                                              sizeof(core::ID)) == 0);
                if (!is_owner)
                {
                    error("Permission denied: only superusers or table owners can drop policies");
                    return;
                }
            }

            // Drop policy
            auto status = db_->catalog_manager()->dropPolicy(
                table_info.table_id, policy_name, &err_ctx);

            if (status != core::Status::OK)
            {
                if (if_exists && status == core::Status::NOT_FOUND)
                {
                    return; // Silently succeed if IF EXISTS specified
                }
                error("DROP POLICY failed: Operation failed");
            }
        }

        void Executor::executeAlterTableRLS()
        {
            // Decode bytecode
            std::string table_name = readString();
            uint8_t action = readByte();

            // Look up schema and table first
            core::CatalogManager::SchemaInfo schema_info;
            auto schema_status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (schema_status != core::Status::OK)
            {
                error("Failed to get schema PUBLIC");
            }

            core::CatalogManager::TableInfo table_info;
            core::ErrorContext err_ctx;
            auto get_status = db_->catalog_manager()->getTable(
                schema_info.schema_id, table_name, table_info, &err_ctx);

            if (get_status != core::Status::OK)
            {
                error("Table '" + table_name + "' not found");
            }

            // Security Check: Only superusers or table owners can alter RLS
            if (conn_ctx_ && !conn_ctx_->isSuperuser())
            {
                // Check if current user is the table owner
                bool is_owner = (std::memcmp(&conn_ctx_->getCurrentUserId(),
                                              &table_info.owner_id,
                                              sizeof(core::ID)) == 0);
                if (!is_owner)
                {
                    error("Permission denied: only superusers or table owners can alter table row level security");
                    return;
                }
            }

            // Determine RLS settings based on action
            bool enabled = false;
            bool forced = false;

            switch (action)
            {
            case 0: // ENABLE
                enabled = true;
                forced = table_info.rls_forced; // Keep existing forced setting
                break;
            case 1: // DISABLE
                enabled = false;
                forced = false; // Disabling also clears forced
                break;
            case 2: // FORCE
                enabled = true; // Force implies enable
                forced = true;
                break;
            case 3: // NO_FORCE
                enabled = table_info.rls_enabled; // Keep existing enabled setting
                forced = false;
                break;
            default:
                error("Invalid RLS action: " + std::to_string(action));
            }

            // Update table RLS settings
            auto status = db_->catalog_manager()->setTableRLS(
                table_info.table_id, enabled, forced, &err_ctx);

            if (status != core::Status::OK)
            {
                error("ALTER TABLE ROW LEVEL SECURITY failed: Operation failed");
            }
        }

        // Security context helpers (Phase 2 - Security System)
        const core::ID& Executor::getCurrentUserID() const
        {
            if (!conn_ctx_)
            {
                // Return zero UUID if no connection context
                static const core::ID zero_id = {};
                return zero_id;
            }
            return conn_ctx_->getCurrentUserId();
        }

        const core::ID& Executor::getActiveRoleID() const
        {
            if (!conn_ctx_)
            {
                // Return zero UUID if no connection context
                static const core::ID zero_id = {};
                return zero_id;
            }
            return conn_ctx_->getActiveRoleId();
        }

        bool Executor::isSuperuser() const
        {
            if (!conn_ctx_)
            {
                return false; // No connection context = no privileges
            }
            return conn_ctx_->isSuperuser();
        }

        // Permission check helper (Phase 2 - Security System)
        bool Executor::checkPermission(const core::ID& object_id,
                                      core::CatalogManager::PermissionObjectType object_type,
                                      uint32_t required_privilege)
        {
            // If no connection context, deny access (should never happen in production)
            if (!conn_ctx_)
            {
                return false;
            }

            // Superusers bypass all permission checks (zero overhead!)
            if (conn_ctx_->isSuperuser())
            {
                return true;
            }

            // Get current user ID
            const core::ID& current_user_id = conn_ctx_->getCurrentUserId();

            // Check if object_id is zero UUID (invalid object)
            static const core::ID zero_id = {};
            if (object_id == zero_id)
            {
                return false; // Can't have permissions on invalid object
            }

            // Security Phase 3.2.3: Check global permission cache first
            core::PermissionCache::CacheKey cache_key{
                current_user_id,
                object_id,
                object_type,
                static_cast<core::CatalogManager::Privilege>(required_privilege)
            };

            auto cached_result = db_->permission_cache()->lookup(cache_key);
            if (cached_result.has_value())
            {
                // Cache hit! Return cached result
                return cached_result.value();
            }

            // Cache miss - query catalog manager
            core::ErrorContext err_ctx;
            bool has_permission = false;
            auto status = db_->catalog_manager()->hasPermission(
                current_user_id, object_id, object_type,
                static_cast<core::CatalogManager::Privilege>(required_privilege),
                has_permission, &err_ctx);

            if (status != core::Status::OK)
            {
                // Permission check failed - deny access
                return false;
            }

            // Cache result in global cache (persists across statements!)
            db_->permission_cache()->insert(cache_key, has_permission);

            return has_permission;
        }

        bool Executor::checkPermission(const core::ID& object_id,
                                      core::CatalogManager::PermissionObjectType object_type,
                                      uint32_t required_privilege,
                                      core::PermissionCheckMode mode)
        {
            // SECURITY ENHANCEMENT (MEDIUM-1): Support for verified permission checks
            // This eliminates the tiny race window between REVOKE and cache invalidation

            // If no connection context, deny access (should never happen in production)
            if (!conn_ctx_)
            {
                return false;
            }

            // Superusers bypass all permission checks (zero overhead!)
            if (conn_ctx_->isSuperuser())
            {
                return true;
            }

            // Get current user ID
            const core::ID& current_user_id = conn_ctx_->getCurrentUserId();

            // Check if object_id is zero UUID (invalid object)
            static const core::ID zero_id = {};
            if (object_id == zero_id)
            {
                return false; // Can't have permissions on invalid object
            }

            // Use the new checkPermission method in PermissionCache that supports mode parameter
            core::PermissionCache::CacheKey cache_key{
                current_user_id,
                object_id,
                object_type,
                static_cast<core::CatalogManager::Privilege>(required_privilege)
            };

            core::ErrorContext err_ctx;
            return db_->permission_cache()->checkPermission(
                db_->catalog_manager(),
                cache_key,
                mode,
                &err_ctx);
        }

        // ===== Query Execution Limit Checks (MEDIUM-3 DoS Protection) =====

        void Executor::checkQueryLimits()
        {
            // SECURITY ENHANCEMENT (MEDIUM-3): Check all query execution limits
            checkTimeout();
            checkCTEDepth();
        }

        void Executor::checkTimeout()
        {
            // Check if query has exceeded time limit
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - query_start_time_).count();

            if (elapsed_ms > static_cast<int64_t>(query_limits_.max_execution_time_ms))
            {
                LOG_WARNING(EXECUTOR, "Query timeout exceeded: %lld ms (limit: %llu ms)",
                          elapsed_ms, query_limits_.max_execution_time_ms);
                error("Query execution timeout exceeded");
            }
        }

        void Executor::checkCTEDepth()
        {
            // Check if CTE recursion depth exceeded
            if (cte_recursion_depth_ > query_limits_.max_cte_recursion_depth)
            {
                LOG_WARNING(EXECUTOR, "CTE recursion depth exceeded: %u (limit: %u)",
                          cte_recursion_depth_, query_limits_.max_cte_recursion_depth);
                error("Maximum CTE recursion depth exceeded");
            }
        }

        void Executor::incrementCTEDepth()
        {
            ++cte_recursion_depth_;
            checkCTEDepth();  // Check immediately after increment
        }

        void Executor::decrementCTEDepth()
        {
            if (cte_recursion_depth_ > 0)
            {
                --cte_recursion_depth_;
            }
        }

        void Executor::trackRowsProcessed(uint64_t count)
        {
            rows_processed_ += count;

            // Check row limits
            if (rows_processed_ > query_limits_.max_intermediate_rows)
            {
                LOG_WARNING(EXECUTOR, "Intermediate row limit exceeded: %llu (limit: %llu)",
                          rows_processed_, query_limits_.max_intermediate_rows);
                error("Maximum intermediate row count exceeded");
            }
        }

        // ===== Row-Level Security Helpers (Phase 3.5 - RLS DML Enforcement) =====

        bool Executor::shouldEnforceRLS(const core::ID& table_id)
        {
            // No connection context - enforce RLS (conservative)
            if (!conn_ctx_)
            {
                return true;
            }

            // Get table info to check RLS settings and owner
            core::CatalogManager::TableInfo table_info;
            core::ErrorContext err_ctx;
            auto status = db_->catalog_manager()->getTable(table_id, table_info, &err_ctx);
            if (status != core::Status::OK)
            {
                // Can't get table info - enforce RLS (conservative)
                return true;
            }

            // Check if RLS is enabled on the table
            if (!table_info.rls_enabled)
            {
                return false; // RLS not enabled for this table
            }

            // Check if FORCE RLS is set
            if (table_info.rls_forced)
            {
                return true; // FORCE RLS - even owners and superusers must obey
            }

            // Superusers bypass RLS (unless FORCE RLS is set, checked above)
            if (conn_ctx_->isSuperuser())
            {
                return false;
            }

            // Table owners bypass RLS (unless FORCE RLS is set, checked above)
            if (conn_ctx_->getCurrentUserId() == table_info.owner_id)
            {
                return false;
            }

            // Non-owner, non-superuser, RLS enabled - enforce RLS
            return true;
        }

        bool Executor::checkRLSPolicies(const core::ID& table_id,
                                       const std::vector<Value>& row_values,
                                       const std::vector<core::CatalogManager::ColumnInfo>& columns,
                                       core::CatalogManager::PolicyType policy_type,
                                       bool is_with_check)
        {
            // Check if RLS should be enforced for this user
            if (!shouldEnforceRLS(table_id))
            {
                return true; // Bypass RLS
            }

            // Get active policies for this table and operation type
            std::vector<core::CatalogManager::PolicyInfo> all_policies;
            core::ErrorContext err_ctx;
            auto status = db_->catalog_manager()->getTablePolicies(
                table_id, policy_type, all_policies, &err_ctx);

            if (status != core::Status::OK)
            {
                // Error getting policies - deny access (conservative)
                return false;
            }

            // Filter to only enabled policies
            std::vector<core::CatalogManager::PolicyInfo> policies;
            for (const auto& p : all_policies)
            {
                if (p.is_enabled)
                {
                    policies.push_back(p);
                }
            }

            // No policies = allow (RLS enabled but no restrictions)
            if (policies.empty())
            {
                return true;
            }

            // Check each policy (AND semantics - all must pass)
            for (const auto& policy : policies)
            {
                // Check if policy applies to current user/role
                if (!policyAppliesToUser(policy))
                {
                    continue; // Skip policies that don't apply
                }

                // Get the appropriate expression (USING or WITH CHECK)
                const std::string& expr_hex = is_with_check ? policy.with_check_expr
                                                             : policy.using_expr;

                // Skip if expression is empty
                if (expr_hex.empty())
                {
                    // No expression means policy always passes
                    continue;
                }

                // Deserialize expression from hex
                std::vector<uint8_t> expr_bytecode = hexToBytes(expr_hex);
                if (expr_bytecode.empty())
                {
                    // Failed to deserialize - deny access (conservative)
                    return false;
                }

                // Evaluate expression with row context
                bool result = evaluatePolicyExpression(expr_bytecode, row_values, columns);
                if (!result)
                {
                    // Policy violation - deny access
                    return false;
                }
            }

            // All applicable policies passed
            return true;
        }

        bool Executor::policyAppliesToUser(const core::CatalogManager::PolicyInfo& policy)
        {
            // If policy has no role restrictions, it applies to everyone
            if (policy.role_ids.empty())
            {
                return true;
            }

            // Check if current user or active role is in the policy's role list
            if (!conn_ctx_)
            {
                return false; // No context - conservative denial
            }

            const core::ID& current_user_id = conn_ctx_->getCurrentUserId();
            const core::ID& active_role_id = conn_ctx_->getActiveRoleId();

            // Phase 3 Polish: O(1) UUID membership check with hash set
            std::unordered_set<core::ID, core::IDHash> policy_role_set(policy.role_ids.begin(), policy.role_ids.end());

            // Check if current user ID is directly in the policy's role list
            if (policy_role_set.count(current_user_id) > 0)
            {
                return true;
            }

            // Check if active role is in the policy's role list
            core::ID zero_id{};  // Zero-initialized UUID
            if (active_role_id != zero_id && policy_role_set.count(active_role_id) > 0)
            {
                return true;
            }

            // Phase 3 Polish: Transitive role membership (roles inherited from groups)
            // Get all effective roles for the current user (including transitive)
            core::ErrorContext err_ctx;
            std::vector<core::ID> effective_roles;
            if (db_->catalog_manager()->getEffectiveRoles(current_user_id, effective_roles, &err_ctx) == core::Status::OK)
            {
                for (const auto& role_id : effective_roles)
                {
                    if (policy_role_set.count(role_id) > 0)
                    {
                        return true; // User has this role transitively
                    }
                }
            }

            return false; // User/role not in policy's role list
        }

        std::vector<uint8_t> Executor::hexToBytes(const std::string& hex_str)
        {
            std::vector<uint8_t> bytes;

            // Check for "0x" prefix
            size_t start_pos = 0;
            if (hex_str.size() >= 2 && hex_str[0] == '0' && hex_str[1] == 'x')
            {
                start_pos = 2;
            }

            // Hex string must have even number of characters
            size_t hex_len = hex_str.size() - start_pos;
            if (hex_len % 2 != 0)
            {
                return {}; // Invalid hex string
            }

            bytes.reserve(hex_len / 2);

            for (size_t i = start_pos; i < hex_str.size(); i += 2)
            {
                char high = hex_str[i];
                char low = hex_str[i + 1];

                // Convert hex characters to nibbles
                auto hex_to_nibble = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return -1; // Invalid hex digit
                };

                int high_nibble = hex_to_nibble(high);
                int low_nibble = hex_to_nibble(low);

                if (high_nibble < 0 || low_nibble < 0)
                {
                    return {}; // Invalid hex digit
                }

                bytes.push_back(static_cast<uint8_t>((high_nibble << 4) | low_nibble));
            }

            return bytes;
        }

        bool Executor::evaluatePolicyExpression(const std::vector<uint8_t>& expr_bytecode,
                                               const std::vector<Value>& row_values,
                                               const std::vector<core::CatalogManager::ColumnInfo>& columns)
        {
            // Save current execution state
            size_t saved_pc = pc_;
            const uint8_t* saved_bytecode = bytecode_;
            size_t saved_bytecode_size = bytecode_size_;

            // Set up new execution context with expression bytecode
            bytecode_ = expr_bytecode.data();
            bytecode_size_ = expr_bytecode.size();
            pc_ = 0;

            // Set up row context so column references resolve to row_values
            current_row_values_ = &row_values;
            current_row_columns_ = &columns;

            try
            {
                // Evaluate the expression
                evaluateExpression();

                // Get result from stack
                if (stack_.empty())
                {
                    // No result - expression invalid
                    // Restore state
                    bytecode_ = saved_bytecode;
                    bytecode_size_ = saved_bytecode_size;
                    pc_ = saved_pc;
                    current_row_values_ = nullptr;
                    current_row_columns_ = nullptr;
                    return false;
                }

                Value result = stack_.top();
                stack_.pop();

                // Restore execution state
                bytecode_ = saved_bytecode;
                bytecode_size_ = saved_bytecode_size;
                pc_ = saved_pc;
                current_row_values_ = nullptr;
                current_row_columns_ = nullptr;

                // Convert result to boolean using Value API
                return result.toBoolean();
            }
            catch (...)
            {
                // Expression evaluation failed - restore state and deny access
                bytecode_ = saved_bytecode;
                bytecode_size_ = saved_bytecode_size;
                pc_ = saved_pc;
                current_row_values_ = nullptr;
                current_row_columns_ = nullptr;
                return false;
            }
        }

        // ALPHA Phase A: Evaluate DEFAULT value expression for a column
        // For now, supports simple constant defaults (numbers, strings, booleans, NULL)
        // Future: Support function calls like NOW(), CURRENT_USER, etc.
        Value Executor::evaluateDefaultValue(const core::CatalogManager::ColumnInfo& column)
        {
            // ALPHA Phase A: Prefer bytecode expression over simple string value
            if (!column.default_expr.empty())
            {
                // Evaluate DEFAULT expression bytecode
                std::vector<uint8_t> expr_bytecode = hexToBytes(column.default_expr);
                if (expr_bytecode.empty())
                {
                    DEBUG_LOG_DB("Failed to deserialize DEFAULT expression for column "
                               << column.column_name << " - using NULL");
                    return Value::makeNull();
                }

                // Save execution state
                const uint8_t *saved_bytecode = bytecode_;
                size_t saved_bytecode_size = bytecode_size_;
                size_t saved_pc = pc_;

                // Set up bytecode for expression evaluation
                bytecode_ = expr_bytecode.data();
                bytecode_size_ = expr_bytecode.size();
                pc_ = 0;

                try
                {
                    // Evaluate the DEFAULT expression
                    evaluateExpression();

                    // Get result from stack
                    if (stack_.empty())
                    {
                        // No result - expression invalid
                        bytecode_ = saved_bytecode;
                        bytecode_size_ = saved_bytecode_size;
                        pc_ = saved_pc;
                        DEBUG_LOG_DB("DEFAULT expression for column " << column.column_name
                                   << " produced no result - using NULL");
                        return Value::makeNull();
                    }

                    Value result = stack_.top();
                    stack_.pop();

                    // Restore execution state
                    bytecode_ = saved_bytecode;
                    bytecode_size_ = saved_bytecode_size;
                    pc_ = saved_pc;

                    return result;
                }
                catch (...)
                {
                    // Expression evaluation failed - restore state and return NULL
                    bytecode_ = saved_bytecode;
                    bytecode_size_ = saved_bytecode_size;
                    pc_ = saved_pc;
                    DEBUG_LOG_DB("DEFAULT expression evaluation failed for column "
                               << column.column_name << " - using NULL");
                    return Value::makeNull();
                }
            }

            // Fallback to simple string parsing (backward compatibility)
            const std::string& default_str = column.default_value;

            // Handle NULL
            if (default_str == "NULL" || default_str.empty())
            {
                return Value::makeNull();
            }

            // Handle boolean literals
            if (default_str == "TRUE" || default_str == "true" || default_str == "t")
            {
                return Value::makeBoolean(true);
            }
            if (default_str == "FALSE" || default_str == "false" || default_str == "f")
            {
                return Value::makeBoolean(false);
            }

            // Handle string literals (enclosed in single quotes)
            if (default_str.size() >= 2 && default_str.front() == '\'' && default_str.back() == '\'')
            {
                std::string str_value = default_str.substr(1, default_str.size() - 2);
                // Handle escaped quotes
                size_t pos = 0;
                while ((pos = str_value.find("''", pos)) != std::string::npos)
                {
                    str_value.replace(pos, 2, "'");
                    pos++;
                }
                return Value::makeVarchar(str_value);
            }

            // Handle numeric literals
            try
            {
                // Check if it's a float (contains '.' or 'e'/'E')
                if (default_str.find('.') != std::string::npos ||
                    default_str.find('e') != std::string::npos ||
                    default_str.find('E') != std::string::npos)
                {
                    double d = std::stod(default_str);
                    return Value::makeFloat64(d);
                }
                else
                {
                    // Try as int64
                    int64_t i = std::stoll(default_str);

                    // Check if it fits in int32
                    if (i >= INT32_MIN && i <= INT32_MAX)
                    {
                        return Value::makeInt32(static_cast<int32_t>(i));
                    }
                    return Value::makeInt64(i);
                }
            }
            catch (const std::exception&)
            {
                // Not a valid number - return NULL as fallback
                DEBUG_LOG_DB("Invalid DEFAULT value for column " << column.column_name
                           << ": '" << default_str << "' - using NULL");
                return Value::makeNull();
            }
        }

        // ALPHA Phase A+: Find an index that covers the specified columns (Nov 19, 2025)
        // This enables O(log n) constraint checking instead of O(n) table scans
        bool Executor::findIndexForColumns(const core::ID& table_id,
                                           const std::vector<core::ID>& column_ids,
                                           core::CatalogManager::IndexInfo& index_out)
        {
            // Get all indexes for the table
            std::vector<core::CatalogManager::IndexInfo> indexes;
            auto status = db_->catalog_manager()->listIndexesForTable(table_id, indexes, nullptr);
            if (status != core::Status::OK)
            {
                return false; // No indexes available
            }

            // Look for an index that exactly matches the columns (in order)
            for (const auto& index_info : indexes)
            {
                // Check if index columns match requested columns
                if (index_info.column_ids.size() == column_ids.size())
                {
                    bool all_match = true;
                    for (size_t i = 0; i < column_ids.size(); i++)
                    {
                        if (index_info.column_ids[i] != column_ids[i])
                        {
                            all_match = false;
                            break;
                        }
                    }

                    if (all_match)
                    {
                        // Found a matching index
                        // Prefer B-Tree and Hash indexes for equality searches
                        if (index_info.index_type == core::CatalogManager::IndexType::BTREE ||
                            index_info.index_type == core::CatalogManager::IndexType::HASH)
                        {
                            index_out = index_info;
                            return true;
                        }
                    }
                }
            }

            // No exact match found - check if we can use a prefix of a composite index
            // For single-column searches, we can use the first column of a composite index
            if (column_ids.size() == 1)
            {
                for (const auto& index_info : indexes)
                {
                    if (!index_info.column_ids.empty() &&
                        index_info.column_ids[0] == column_ids[0])
                    {
                        // First column matches - can use this index
                        if (index_info.index_type == core::CatalogManager::IndexType::BTREE ||
                            index_info.index_type == core::CatalogManager::IndexType::HASH)
                        {
                            index_out = index_info;
                            return true;
                        }
                    }
                }
            }

            return false; // No suitable index found
        }

        // ALPHA Phase A+: Search an index for matching values (Nov 19, 2025)
        // Returns TIDs of rows that match the specified values
        core::Status Executor::searchIndexForValues(const core::CatalogManager::IndexInfo& index_info,
                                                    const std::vector<Value>& values,
                                                    uint64_t current_xid,
                                                    std::vector<core::TID>& tids_out)
        {
            // Build index key from values
            std::vector<uint8_t> key_bytes;
            serializeIndexKey(values, key_bytes);

            // Use the routeIndexSearch helper to search the appropriate index type
            core::ErrorContext ctx;
            auto status = routeIndexSearch(
                static_cast<IndexType>(index_info.index_type),
                index_info.index_id,
                key_bytes,
                current_xid,
                &tids_out,
                &ctx
            );

            if (status != core::Status::OK)
            {
                DEBUG_LOG_DB("Index search failed for index " << index_info.index_name
                           << ": " << ctx.message);
            }

            return status;
        }

        // ALPHA Phase A: Evaluate CHECK constraint for a column
        // Returns true if constraint passes, false if it fails
        bool Executor::evaluateCheckConstraint(const core::CatalogManager::ColumnInfo& column,
                                               const std::vector<Value>& row_values,
                                               const std::vector<core::CatalogManager::ColumnInfo>& columns)
        {
            // No CHECK constraint - check both the direct expression and OID fields
            if (column.check_expr.empty() && column.check_expr_oid == 0)
            {
                return true;
            }

            // Prefer direct check_expr field (hex bytecode) over TOAST OID
            std::string expr_hex = column.check_expr;

            // If check_expr is empty but check_expr_oid is set, try to load from TOAST
            // SECURITY FIX (Nov 19, 2025): Reject instead of allowing to prevent bypass
            if (expr_hex.empty() && column.check_expr_oid != 0)
            {
                // CONSERVATIVE APPROACH: Reject the operation when CHECK expression is in TOAST
                // but TOAST loading not implemented. This prevents security bypass.
                // TODO: Implement proper TOAST loading when infrastructure is complete:
                //   1. Get TOAST manager for catalog table
                //   2. Construct ToastPointer from check_expr_oid
                //   3. Call detoastValue() to retrieve expression bytecode
                //   4. Use retrieved bytecode for evaluation
                //
                // For now, fail-safe by rejecting rows with TOASTed CHECK expressions
                DEBUG_LOG_DB("CHECK constraint on column " << column.column_name
                           << " uses TOAST (check_expr_oid=" << column.check_expr_oid
                           << ") - TOAST loading not yet implemented");

                // Fail-safe: Reject to prevent security bypass
                // (Large CHECK expressions should not silently bypass enforcement)
                error("CHECK constraint on column '" + column.column_name +
                      "' uses TOAST storage which is not yet supported. "
                      "Please recreate the constraint with a simpler expression.");
                return false; // Never reached (error() throws), but explicit for clarity
            }

            // Skip if expression is still empty (no CHECK constraint at all)
            if (expr_hex.empty())
            {
                return true;
            }

            // Deserialize expression from hex
            std::vector<uint8_t> expr_bytecode = hexToBytes(expr_hex);
            if (expr_bytecode.empty())
            {
                // Failed to deserialize - deny access (conservative)
                DEBUG_LOG_DB("Failed to deserialize CHECK constraint expression for column "
                           << column.column_name << " - denying row");
                return false;
            }

            // Evaluate expression with row context using existing RLS infrastructure
            bool result = evaluatePolicyExpression(expr_bytecode, row_values, columns);

            DEBUG_LOG_DB("CHECK constraint on column " << column.column_name
                       << " evaluated to " << (result ? "TRUE" : "FALSE"));

            return result;
        }

        // ALPHA Phase A+: Check for UNIQUE constraint violation (optimized with indexes, Nov 19, 2025)
        // Returns true if a duplicate value exists (violation), false if value is unique
        bool Executor::checkUniqueViolation(const core::ID& table_id,
                                            const core::CatalogManager::ColumnInfo& column,
                                            const Value& value,
                                            const std::vector<core::CatalogManager::ColumnInfo>& all_columns)
        {
            // OPTIMIZATION: Try to use an index for O(log n) lookup instead of O(n) scan
            core::CatalogManager::IndexInfo index_info;
            std::vector<core::ID> column_ids = { column.column_id };

            if (findIndexForColumns(table_id, column_ids, index_info))
            {
                // Found an index! Use it for fast lookup
                DEBUG_LOG_DB("UNIQUE constraint check using index '" << index_info.index_name
                           << "' for column " << column.column_name);

                std::vector<Value> search_values = { value };
                std::vector<core::TID> matching_tids;
                uint64_t current_xid = db_->storage_engine()->getCurrentXid();

                auto status = searchIndexForValues(index_info, search_values, current_xid, matching_tids);

                if (status == core::Status::OK)
                {
                    // Index search succeeded - check if any rows found
                    bool has_duplicate = !matching_tids.empty();

                    if (has_duplicate)
                    {
                        DEBUG_LOG_DB("UNIQUE violation detected via index: found "
                                   << matching_tids.size() << " matching row(s)");
                    }

                    return has_duplicate;
                }
                else
                {
                    // Index search failed - fall through to sequential scan
                    DEBUG_LOG_DB("Index search failed, falling back to sequential scan");
                }
            }

            // FALLBACK: No suitable index found or index search failed - use sequential scan
            DEBUG_LOG_DB("UNIQUE constraint check using sequential scan for column " << column.column_name);

            // Get column index
            size_t col_index = 0;
            for (size_t i = 0; i < all_columns.size(); i++)
            {
                if (all_columns[i].column_id == column.column_id)
                {
                    col_index = i;
                    break;
                }
            }

            // Scan table to check for existing value
            auto scan_iter = db_->storage_engine()->createScan(table_id, nullptr);
            if (!scan_iter)
            {
                // Can't create scan - conservative: treat as violation
                return true;
            }

            // Scan all tuples
            core::Tuple tuple;
            while (scan_iter->next(&tuple, nullptr) == core::Status::OK)
            {
                // Deserialize tuple data
                std::vector<Value> row_values;
                if (!deserializeTuple(tuple.data, tuple.data_size, all_columns, row_values))
                {
                    continue; // Skip malformed tuples
                }

                // Check if this row has the same value in the UNIQUE column
                if (col_index < row_values.size() && !row_values[col_index].isNull())
                {
                    // Compare values
                    if (valuesEqual(value, row_values[col_index]))
                    {
                        // Found a duplicate!
                        return true;
                    }
                }
            }

            // No duplicate found
            return false;
        }

        // ALPHA Phase A+: Check for UNIQUE constraint violation during UPDATE (optimized, Nov 19, 2025)
        // Similar to checkUniqueViolation, but excludes the row being updated (identified by TID)
        bool Executor::checkUniqueViolationForUpdate(const core::ID& table_id,
                                                     const core::CatalogManager::ColumnInfo& column,
                                                     const Value& value,
                                                     const std::vector<core::CatalogManager::ColumnInfo>& all_columns,
                                                     const core::TID& exclude_tid)
        {
            // OPTIMIZATION: Try to use an index for O(log n) lookup instead of O(n) scan
            core::CatalogManager::IndexInfo index_info;
            std::vector<core::ID> column_ids = { column.column_id };

            if (findIndexForColumns(table_id, column_ids, index_info))
            {
                // Found an index! Use it for fast lookup
                DEBUG_LOG_DB("UNIQUE constraint check (UPDATE) using index '" << index_info.index_name
                           << "' for column " << column.column_name);

                std::vector<Value> search_values = { value };
                std::vector<core::TID> matching_tids;
                uint64_t current_xid = db_->storage_engine()->getCurrentXid();

                auto status = searchIndexForValues(index_info, search_values, current_xid, matching_tids);

                if (status == core::Status::OK)
                {
                    // Index search succeeded - check if any rows found (excluding the one being updated)
                    bool has_duplicate = false;
                    for (const auto& tid : matching_tids)
                    {
                        // Skip the row being updated
                        if (tid.gpid != exclude_tid.gpid || tid.slot != exclude_tid.slot)
                        {
                            has_duplicate = true;
                            break;
                        }
                    }

                    if (has_duplicate)
                    {
                        DEBUG_LOG_DB("UNIQUE violation detected via index (UPDATE): found duplicate(s) "
                                   << "excluding TID " << exclude_tid.value());
                    }

                    return has_duplicate;
                }
                else
                {
                    // Index search failed - fall through to sequential scan
                    DEBUG_LOG_DB("Index search failed, falling back to sequential scan");
                }
            }

            // FALLBACK: No suitable index found or index search failed - use sequential scan
            DEBUG_LOG_DB("UNIQUE constraint check (UPDATE) using sequential scan for column " << column.column_name);

            // Get column index
            size_t col_index = 0;
            for (size_t i = 0; i < all_columns.size(); i++)
            {
                if (all_columns[i].column_id == column.column_id)
                {
                    col_index = i;
                    break;
                }
            }

            // Scan table to check for existing value
            auto scan_iter = db_->storage_engine()->createScan(table_id, nullptr);
            if (!scan_iter)
            {
                // Can't create scan - conservative: treat as violation
                return true;
            }

            // Scan all tuples
            core::Tuple tuple;
            while (scan_iter->next(&tuple, nullptr) == core::Status::OK)
            {
                // Skip the row being updated (exclude by TID)
                if (tuple.tid.gpid == exclude_tid.gpid && tuple.tid.slot == exclude_tid.slot)
                {
                    continue;
                }

                // Deserialize tuple data
                std::vector<Value> row_values;
                if (!deserializeTuple(tuple.data, tuple.data_size, all_columns, row_values))
                {
                    continue; // Skip malformed tuples
                }

                // Check if this row has the same value in the UNIQUE column
                if (col_index < row_values.size() && !row_values[col_index].isNull())
                {
                    // Compare values
                    if (valuesEqual(value, row_values[col_index]))
                    {
                        // Found a duplicate!
                        return true;
                    }
                }
            }

            // No duplicate found
            return false;
        }

        // ALPHA Phase A: Compare two values for equality (for UNIQUE constraint checking)
        bool Executor::valuesEqual(const Value& a, const Value& b)
        {
            // If types differ, values are not equal
            if (a.type() != b.type())
            {
                return false;
            }

            // Handle NULL (NULLs are never equal, even to other NULLs, per SQL standard)
            if (a.isNull() || b.isNull())
            {
                return false;
            }

            // Compare based on type
            switch (a.type())
            {
                case core::DataType::INT32:
                    return a.getInt32() == b.getInt32();
                case core::DataType::INT64:
                    return a.getInt64() == b.getInt64();
                case core::DataType::FLOAT64:
                    return a.getFloat64() == b.getFloat64();
                case core::DataType::VARCHAR:
                case core::DataType::TEXT:
                    return a.getVarchar() == b.getVarchar();
                case core::DataType::BOOLEAN:
                    return a.getBoolean() == b.getBoolean();
                default:
                    // For other types, conservatively return false
                    return false;
            }
        }

        // ALPHA Phase A+: Check if FK constraint is satisfied (optimized with indexes, Nov 19, 2025)
        // Returns true if the FK value(s) exist in the parent table, false otherwise
        bool Executor::checkForeignKeyExists(const core::ID& parent_table_id,
                                            const std::vector<std::string>& parent_columns,
                                            const std::vector<Value>& fk_values,
                                            const std::vector<core::CatalogManager::ColumnInfo>& parent_cols)
        {
            // MATCH SIMPLE: If any FK value is NULL, the constraint is automatically satisfied
            for (const auto& val : fk_values)
            {
                if (val.isNull())
                {
                    return true; // NULL in FK = no constraint
                }
            }

            // Get parent column IDs (for index lookup) and indices (for fallback scan)
            std::vector<core::ID> parent_col_ids;
            std::vector<size_t> parent_col_indices;
            for (const auto& col_name : parent_columns)
            {
                for (size_t i = 0; i < parent_cols.size(); i++)
                {
                    if (parent_cols[i].column_name == col_name)
                    {
                        parent_col_ids.push_back(parent_cols[i].column_id);
                        parent_col_indices.push_back(i);
                        break;
                    }
                }
            }

            // OPTIMIZATION: Try to use an index on the parent table for O(log n) lookup
            core::CatalogManager::IndexInfo index_info;
            if (findIndexForColumns(parent_table_id, parent_col_ids, index_info))
            {
                // Found an index! Use it for fast lookup
                DEBUG_LOG_DB("FK constraint check using index '" << index_info.index_name
                           << "' on parent table");

                std::vector<core::TID> matching_tids;
                uint64_t current_xid = db_->storage_engine()->getCurrentXid();

                auto status = searchIndexForValues(index_info, fk_values, current_xid, matching_tids);

                if (status == core::Status::OK)
                {
                    // Index search succeeded - check if any matching rows found
                    bool exists = !matching_tids.empty();

                    if (exists)
                    {
                        DEBUG_LOG_DB("FK constraint satisfied via index: found "
                                   << matching_tids.size() << " matching parent row(s)");
                    }
                    else
                    {
                        DEBUG_LOG_DB("FK violation detected via index: no matching parent row");
                    }

                    return exists;
                }
                else
                {
                    // Index search failed - fall through to sequential scan
                    DEBUG_LOG_DB("Index search failed, falling back to sequential scan");
                }
            }

            // FALLBACK: No suitable index found or index search failed - use sequential scan
            DEBUG_LOG_DB("FK constraint check using sequential scan on parent table");

            // Scan parent table to find matching row
            auto scan_iter = db_->storage_engine()->createScan(parent_table_id, nullptr);
            if (!scan_iter)
            {
                return false; // Can't scan - fail safely
            }

            core::Tuple tuple;
            while (scan_iter->next(&tuple, nullptr) == core::Status::OK)
            {
                // Deserialize tuple
                std::vector<Value> row_values;
                if (!deserializeTuple(tuple.data, tuple.data_size, parent_cols, row_values))
                {
                    continue;
                }

                // Check if all FK columns match
                bool all_match = true;
                for (size_t i = 0; i < fk_values.size() && i < parent_col_indices.size(); i++)
                {
                    size_t col_idx = parent_col_indices[i];
                    if (col_idx >= row_values.size() ||
                        !valuesEqual(fk_values[i], row_values[col_idx]))
                    {
                        all_match = false;
                        break;
                    }
                }

                if (all_match)
                {
                    return true; // Found matching row
                }
            }

            return false; // No matching row found - FK violation
        }

        // ALPHA Phase A: Apply FK referential action on DELETE
        void Executor::applyFKActionOnDelete(const core::ID& parent_table_id,
                                            const std::vector<Value>& deleted_key_values,
                                            const std::vector<core::CatalogManager::ColumnInfo>& parent_cols)
        {
            // Get all FKs that reference this parent table
            std::vector<core::CatalogManager::ForeignKeyInfo> referencing_fks;
            db_->catalog_manager()->getReferencingForeignKeys(parent_table_id, referencing_fks);

            if (referencing_fks.empty())
            {
                return; // No foreign keys reference this table
            }

            // Process each referencing FK
            for (const auto& fk : referencing_fks)
            {
                if (!fk.is_enabled)
                {
                    continue; // Skip disabled FKs
                }

                // Get child table metadata
                core::CatalogManager::TableInfo child_table;
                if (db_->catalog_manager()->getTable(fk.child_table_id, child_table) != core::Status::OK)
                {
                    error("Failed to get child table for FK enforcement");
                }

                std::vector<core::CatalogManager::ColumnInfo> child_columns;
                if (db_->catalog_manager()->getColumns(fk.child_table_id, child_columns) != core::Status::OK)
                {
                    error("Failed to get child columns for FK enforcement");
                }

                // Build column ID and index maps for FK columns
                std::vector<core::ID> fk_col_ids;
                std::vector<size_t> fk_col_indices;
                for (const auto& col_name : fk.child_columns)
                {
                    bool found = false;
                    for (size_t i = 0; i < child_columns.size(); i++)
                    {
                        if (child_columns[i].column_name == col_name)
                        {
                            fk_col_ids.push_back(child_columns[i].column_id);
                            fk_col_indices.push_back(i);
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        error("FK child column not found: " + col_name);
                    }
                }

                // OPTIMIZATION: Try to use an index on the child table for O(log n) lookup
                std::vector<core::TID> matching_tids;
                core::CatalogManager::IndexInfo child_index_info;

                if (findIndexForColumns(fk.child_table_id, fk_col_ids, child_index_info))
                {
                    // Found an index! Use it for fast lookup of referencing child rows
                    DEBUG_LOG_DB("FK CASCADE using index '" << child_index_info.index_name
                               << "' on child table " << child_table.table_name);

                    uint64_t current_xid = db_->storage_engine()->getCurrentXid();
                    auto status = searchIndexForValues(child_index_info, deleted_key_values, current_xid, matching_tids);

                    if (status != core::Status::OK)
                    {
                        // Index search failed - fall through to sequential scan
                        DEBUG_LOG_DB("Index search failed for CASCADE, falling back to sequential scan");
                        matching_tids.clear();  // Clear partial results
                    }
                    else
                    {
                        DEBUG_LOG_DB("FK CASCADE found " << matching_tids.size()
                                   << " referencing child rows via index");
                    }
                }

                // FALLBACK: No suitable index or index search failed - use sequential scan
                if (matching_tids.empty())
                {
                    DEBUG_LOG_DB("FK CASCADE using sequential scan on child table " << child_table.table_name);

                    auto scan_iter = db_->storage_engine()->createScan(fk.child_table_id, nullptr);
                    if (!scan_iter)
                    {
                        error("Failed to create scan for child table");
                    }

                    core::Tuple tuple;
                    while (scan_iter->next(&tuple, nullptr) == core::Status::OK)
                    {
                        std::vector<Value> row_values;
                        if (!deserializeTuple(tuple.data, tuple.data_size, child_columns, row_values))
                        {
                            continue;
                        }

                        // Check if this row references the deleted parent row
                        bool matches = true;
                        for (size_t i = 0; i < fk_col_indices.size(); i++)
                        {
                            size_t col_idx = fk_col_indices[i];

                            // MATCH SIMPLE: NULL doesn't count as a reference
                            if (row_values[col_idx].isNull())
                            {
                                matches = false;
                                break;
                            }

                            if (!valuesEqual(row_values[col_idx], deleted_key_values[i]))
                            {
                                matches = false;
                                break;
                            }
                        }

                        if (matches)
                        {
                            matching_tids.push_back(tuple.tid);
                        }
                    }
                }

                // Apply FK action
                if (!matching_tids.empty())
                {
                    switch (fk.on_delete)
                    {
                        case core::CatalogManager::FKAction::RESTRICT:
                        case core::CatalogManager::FKAction::NO_ACTION:
                        {
                            error("Foreign key constraint violation: cannot delete parent row referenced by " +
                                  std::to_string(matching_tids.size()) + " child rows in table '" +
                                  std::string(child_table.table_name) + "' (FK: " + fk.fk_name + ")");
                            break;
                        }

                        case core::CatalogManager::FKAction::CASCADE:
                        {
                            DEBUG_LOG_DB("FK CASCADE DELETE: deleting " + std::to_string(matching_tids.size()) +
                                       " child rows from table " + std::string(child_table.table_name));

                            uint64_t xmax = db_->storage_engine()->getCurrentXid();
                            for (const auto& tid : matching_tids)
                            {
                                uint32_t page_id = static_cast<uint32_t>(core::getPageNumber(tid));
                                uint16_t item_id = core::getSlot(tid);
                                auto status = db_->storage_engine()->deleteTuple(fk.child_table_id, page_id, item_id, nullptr);
                                if (status != core::Status::OK)
                                {
                                    DEBUG_LOG_DB("FK CASCADE DELETE failed for child row");
                                }
                            }
                            break;
                        }

                        case core::CatalogManager::FKAction::SET_NULL:
                        {
                            // ALPHA Phase B: SET NULL implementation (for DELETE)
                            DEBUG_LOG_DB("FK SET NULL (DELETE): setting FK columns to NULL in " +
                                       std::to_string(matching_tids.size()) + " child rows in table " +
                                       std::string(child_table.table_name));

                            // Create NULL values for all FK columns
                            std::vector<Value> null_values(fk_col_indices.size(), Value::makeNull());

                            // For each matching child row, set FK columns to NULL
                            for (const auto& tid : matching_tids)
                            {
                                // Fetch the current child tuple
                                core::Tuple child_tuple;
                                auto fetch_status = db_->storage_engine()->getTuple(
                                    fk.child_table_id, tid, &child_tuple, nullptr);

                                if (fetch_status != core::Status::OK)
                                {
                                    DEBUG_LOG_DB("FK SET NULL: failed to fetch child tuple");
                                    continue;
                                }

                                // Modify the FK columns to NULL
                                std::vector<uint8_t> new_tuple_data;
                                if (!modifyTupleColumns(child_tuple.data, child_tuple.data_size,
                                                       child_columns, fk_col_indices,
                                                       null_values, new_tuple_data))
                                {
                                    error("FK SET NULL: failed to modify child tuple for FK: " + fk.fk_name);
                                }

                                // Update the child row using storage engine
                                uint32_t page_id = static_cast<uint32_t>(core::getPageNumber(tid));
                                uint16_t item_id = core::getSlot(tid);
                                uint32_t new_page_id;
                                uint16_t new_item_id;

                                auto update_status = db_->storage_engine()->updateTuple(
                                    fk.child_table_id, page_id, item_id,
                                    new_tuple_data.data(), static_cast<uint32_t>(new_tuple_data.size()),
                                    &new_page_id, &new_item_id, nullptr);

                                if (update_status != core::Status::OK)
                                {
                                    error("FK SET NULL: failed to update child row for FK: " + fk.fk_name);
                                }

                                DEBUG_LOG_DB("FK SET NULL: updated child row (" +
                                           std::to_string(page_id) + "," + std::to_string(item_id) + ")");
                            }
                            break;
                        }

                        case core::CatalogManager::FKAction::SET_DEFAULT:
                        {
                            // ALPHA Phase B: SET DEFAULT implementation (for DELETE)
                            DEBUG_LOG_DB("FK SET DEFAULT (DELETE): setting FK columns to DEFAULT in " +
                                       std::to_string(matching_tids.size()) + " child rows in table " +
                                       std::string(child_table.table_name));

                            // Get DEFAULT values for FK columns
                            std::vector<Value> default_values;
                            for (size_t i = 0; i < fk_col_indices.size(); i++)
                            {
                                size_t col_idx = fk_col_indices[i];
                                const auto& col_info = child_columns[col_idx];

                                // Check if column has a DEFAULT value
                                if (col_info.default_value.empty())
                                {
                                    // No DEFAULT defined - use NULL
                                    default_values.push_back(Value::makeNull());
                                }
                                else
                                {
                                    // Parse simple default value (literals only for Phase B)
                                    // TODO: Evaluate default_expr bytecode for complex defaults
                                    core::DataType col_type = static_cast<core::DataType>(col_info.data_type);
                                    Value default_val;

                                    try
                                    {
                                        switch (col_type)
                                        {
                                            case core::DataType::INT32:
                                                default_val = Value::makeInt32(std::stoi(col_info.default_value));
                                                break;
                                            case core::DataType::INT64:
                                                default_val = Value::makeInt64(std::stoll(col_info.default_value));
                                                break;
                                            case core::DataType::FLOAT64:
                                                default_val = Value::makeFloat64(std::stod(col_info.default_value));
                                                break;
                                            case core::DataType::VARCHAR:
                                                default_val = Value::makeVarchar(col_info.default_value);
                                                break;
                                            default:
                                                default_val = Value::makeNull();
                                                break;
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Parsing failed - use NULL
                                        default_val = Value::makeNull();
                                    }

                                    default_values.push_back(default_val);
                                }
                            }

                            // For each matching child row, set FK columns to DEFAULT
                            for (const auto& tid : matching_tids)
                            {
                                // Fetch the current child tuple
                                core::Tuple child_tuple;
                                auto fetch_status = db_->storage_engine()->getTuple(
                                    fk.child_table_id, tid, &child_tuple, nullptr);

                                if (fetch_status != core::Status::OK)
                                {
                                    DEBUG_LOG_DB("FK SET DEFAULT: failed to fetch child tuple");
                                    continue;
                                }

                                // Modify the FK columns to DEFAULT values
                                std::vector<uint8_t> new_tuple_data;
                                if (!modifyTupleColumns(child_tuple.data, child_tuple.data_size,
                                                       child_columns, fk_col_indices,
                                                       default_values, new_tuple_data))
                                {
                                    error("FK SET DEFAULT: failed to modify child tuple for FK: " + fk.fk_name);
                                }

                                // Update the child row using storage engine
                                uint32_t page_id = static_cast<uint32_t>(core::getPageNumber(tid));
                                uint16_t item_id = core::getSlot(tid);
                                uint32_t new_page_id;
                                uint16_t new_item_id;

                                auto update_status = db_->storage_engine()->updateTuple(
                                    fk.child_table_id, page_id, item_id,
                                    new_tuple_data.data(), static_cast<uint32_t>(new_tuple_data.size()),
                                    &new_page_id, &new_item_id, nullptr);

                                if (update_status != core::Status::OK)
                                {
                                    error("FK SET DEFAULT: failed to update child row for FK: " + fk.fk_name);
                                }

                                DEBUG_LOG_DB("FK SET DEFAULT: updated child row (" +
                                           std::to_string(page_id) + "," + std::to_string(item_id) + ")");
                            }
                            break;
                        }
                    }
                }
            }
        }

        // ALPHA Phase A: Apply FK referential action on UPDATE
        void Executor::applyFKActionOnUpdate(const core::ID& parent_table_id,
                                            const std::vector<Value>& old_key_values,
                                            const std::vector<Value>& new_key_values,
                                            const std::vector<core::CatalogManager::ColumnInfo>& parent_cols)
        {
            // Check if key actually changed
            bool key_changed = false;
            for (size_t i = 0; i < old_key_values.size() && i < new_key_values.size(); i++)
            {
                if (!valuesEqual(old_key_values[i], new_key_values[i]))
                {
                    key_changed = true;
                    break;
                }
            }

            if (!key_changed)
            {
                return; // Key unchanged - no action needed
            }

            // Get all FKs that reference this parent table
            std::vector<core::CatalogManager::ForeignKeyInfo> referencing_fks;
            db_->catalog_manager()->getReferencingForeignKeys(parent_table_id, referencing_fks);

            if (referencing_fks.empty())
            {
                return; // No foreign keys reference this table
            }

            // Process each referencing FK
            for (const auto& fk : referencing_fks)
            {
                if (!fk.is_enabled)
                {
                    continue; // Skip disabled FKs
                }

                // Get child table metadata
                core::CatalogManager::TableInfo child_table;
                if (db_->catalog_manager()->getTable(fk.child_table_id, child_table) != core::Status::OK)
                {
                    error("Failed to get child table for FK enforcement");
                }

                std::vector<core::CatalogManager::ColumnInfo> child_columns;
                if (db_->catalog_manager()->getColumns(fk.child_table_id, child_columns) != core::Status::OK)
                {
                    error("Failed to get child columns for FK enforcement");
                }

                // Build column ID and index maps for FK columns
                std::vector<core::ID> fk_col_ids;
                std::vector<size_t> fk_col_indices;
                for (const auto& col_name : fk.child_columns)
                {
                    bool found = false;
                    for (size_t i = 0; i < child_columns.size(); i++)
                    {
                        if (child_columns[i].column_name == col_name)
                        {
                            fk_col_ids.push_back(child_columns[i].column_id);
                            fk_col_indices.push_back(i);
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        error("FK child column not found: " + col_name);
                    }
                }

                // OPTIMIZATION: Try to use an index on the child table for O(log n) lookup
                std::vector<core::TID> matching_tids;
                core::CatalogManager::IndexInfo child_index_info;

                if (findIndexForColumns(fk.child_table_id, fk_col_ids, child_index_info))
                {
                    // Found an index! Use it for fast lookup of referencing child rows
                    DEBUG_LOG_DB("FK CASCADE UPDATE using index '" << child_index_info.index_name
                               << "' on child table " << child_table.table_name);

                    uint64_t current_xid = db_->storage_engine()->getCurrentXid();
                    auto status = searchIndexForValues(child_index_info, old_key_values, current_xid, matching_tids);

                    if (status != core::Status::OK)
                    {
                        // Index search failed - fall through to sequential scan
                        DEBUG_LOG_DB("Index search failed for CASCADE UPDATE, falling back to sequential scan");
                        matching_tids.clear();  // Clear partial results
                    }
                    else
                    {
                        DEBUG_LOG_DB("FK CASCADE UPDATE found " << matching_tids.size()
                                   << " referencing child rows via index");
                    }
                }

                // FALLBACK: No suitable index or index search failed - use sequential scan
                if (matching_tids.empty())
                {
                    DEBUG_LOG_DB("FK CASCADE UPDATE using sequential scan on child table " << child_table.table_name);

                    auto scan_iter = db_->storage_engine()->createScan(fk.child_table_id, nullptr);
                    if (!scan_iter)
                    {
                        error("Failed to create scan for child table");
                    }

                    core::Tuple tuple;
                    while (scan_iter->next(&tuple, nullptr) == core::Status::OK)
                    {
                        std::vector<Value> row_values;
                        if (!deserializeTuple(tuple.data, tuple.data_size, child_columns, row_values))
                        {
                            continue;
                        }

                        // Check if this row references the OLD parent key
                        bool matches = true;
                        for (size_t i = 0; i < fk_col_indices.size(); i++)
                        {
                            size_t col_idx = fk_col_indices[i];

                            // MATCH SIMPLE: NULL doesn't count as a reference
                            if (row_values[col_idx].isNull())
                            {
                                matches = false;
                                break;
                            }

                            if (!valuesEqual(row_values[col_idx], old_key_values[i]))
                            {
                                matches = false;
                                break;
                            }
                        }

                        if (matches)
                        {
                            matching_tids.push_back(tuple.tid);
                        }
                    }
                }

                // Apply FK action
                if (!matching_tids.empty())
                {
                    switch (fk.on_update)
                    {
                        case core::CatalogManager::FKAction::RESTRICT:
                        case core::CatalogManager::FKAction::NO_ACTION:
                        {
                            error("Foreign key constraint violation: cannot update parent key referenced by " +
                                  std::to_string(matching_tids.size()) + " child rows in table '" +
                                  std::string(child_table.table_name) + "' (FK: " + fk.fk_name + ")");
                            break;
                        }

                        case core::CatalogManager::FKAction::CASCADE:
                        {
                            // ALPHA Phase B: CASCADE UPDATE implementation
                            DEBUG_LOG_DB("FK CASCADE UPDATE: updating " + std::to_string(matching_tids.size()) +
                                       " child rows in table " + std::string(child_table.table_name));

                            // For each matching child row, update FK columns to new parent key values
                            for (const auto& tid : matching_tids)
                            {
                                // Fetch the current child tuple
                                core::Tuple child_tuple;
                                auto fetch_status = db_->storage_engine()->getTuple(
                                    fk.child_table_id, tid, &child_tuple, nullptr);

                                if (fetch_status != core::Status::OK)
                                {
                                    DEBUG_LOG_DB("FK CASCADE UPDATE: failed to fetch child tuple");
                                    continue;
                                }

                                // Modify the FK columns with new parent key values
                                std::vector<uint8_t> new_tuple_data;
                                if (!modifyTupleColumns(child_tuple.data, child_tuple.data_size,
                                                       child_columns, fk_col_indices,
                                                       new_key_values, new_tuple_data))
                                {
                                    error("FK CASCADE UPDATE: failed to modify child tuple for FK: " + fk.fk_name);
                                }

                                // Update the child row using storage engine
                                uint32_t page_id = static_cast<uint32_t>(core::getPageNumber(tid));
                                uint16_t item_id = core::getSlot(tid);
                                uint32_t new_page_id;
                                uint16_t new_item_id;

                                auto update_status = db_->storage_engine()->updateTuple(
                                    fk.child_table_id, page_id, item_id,
                                    new_tuple_data.data(), static_cast<uint32_t>(new_tuple_data.size()),
                                    &new_page_id, &new_item_id, nullptr);

                                if (update_status != core::Status::OK)
                                {
                                    error("FK CASCADE UPDATE: failed to update child row for FK: " + fk.fk_name);
                                }

                                DEBUG_LOG_DB("FK CASCADE UPDATE: updated child row (" +
                                           std::to_string(page_id) + "," + std::to_string(item_id) + ")");
                            }
                            break;
                        }

                        case core::CatalogManager::FKAction::SET_NULL:
                        {
                            // ALPHA Phase B: SET NULL implementation (for UPDATE)
                            DEBUG_LOG_DB("FK SET NULL (UPDATE): setting FK columns to NULL in " +
                                       std::to_string(matching_tids.size()) + " child rows in table " +
                                       std::string(child_table.table_name));

                            // Create NULL values for all FK columns
                            std::vector<Value> null_values(fk_col_indices.size(), Value::makeNull());

                            // For each matching child row, set FK columns to NULL
                            for (const auto& tid : matching_tids)
                            {
                                // Fetch the current child tuple
                                core::Tuple child_tuple;
                                auto fetch_status = db_->storage_engine()->getTuple(
                                    fk.child_table_id, tid, &child_tuple, nullptr);

                                if (fetch_status != core::Status::OK)
                                {
                                    DEBUG_LOG_DB("FK SET NULL: failed to fetch child tuple");
                                    continue;
                                }

                                // Modify the FK columns to NULL
                                std::vector<uint8_t> new_tuple_data;
                                if (!modifyTupleColumns(child_tuple.data, child_tuple.data_size,
                                                       child_columns, fk_col_indices,
                                                       null_values, new_tuple_data))
                                {
                                    error("FK SET NULL: failed to modify child tuple for FK: " + fk.fk_name);
                                }

                                // Update the child row using storage engine
                                uint32_t page_id = static_cast<uint32_t>(core::getPageNumber(tid));
                                uint16_t item_id = core::getSlot(tid);
                                uint32_t new_page_id;
                                uint16_t new_item_id;

                                auto update_status = db_->storage_engine()->updateTuple(
                                    fk.child_table_id, page_id, item_id,
                                    new_tuple_data.data(), static_cast<uint32_t>(new_tuple_data.size()),
                                    &new_page_id, &new_item_id, nullptr);

                                if (update_status != core::Status::OK)
                                {
                                    error("FK SET NULL: failed to update child row for FK: " + fk.fk_name);
                                }

                                DEBUG_LOG_DB("FK SET NULL: updated child row (" +
                                           std::to_string(page_id) + "," + std::to_string(item_id) + ")");
                            }
                            break;
                        }

                        case core::CatalogManager::FKAction::SET_DEFAULT:
                        {
                            // ALPHA Phase B: SET DEFAULT implementation (for UPDATE)
                            DEBUG_LOG_DB("FK SET DEFAULT (UPDATE): setting FK columns to DEFAULT in " +
                                       std::to_string(matching_tids.size()) + " child rows in table " +
                                       std::string(child_table.table_name));

                            // Get DEFAULT values for FK columns
                            std::vector<Value> default_values;
                            for (size_t i = 0; i < fk_col_indices.size(); i++)
                            {
                                size_t col_idx = fk_col_indices[i];
                                const auto& col_info = child_columns[col_idx];

                                // Check if column has a DEFAULT value
                                if (col_info.default_value.empty())
                                {
                                    // No DEFAULT defined - use NULL
                                    default_values.push_back(Value::makeNull());
                                }
                                else
                                {
                                    // Parse simple default value (literals only for Phase B)
                                    // TODO: Evaluate default_expr bytecode for complex defaults
                                    core::DataType col_type = static_cast<core::DataType>(col_info.data_type);
                                    Value default_val;

                                    try
                                    {
                                        switch (col_type)
                                        {
                                            case core::DataType::INT32:
                                                default_val = Value::makeInt32(std::stoi(col_info.default_value));
                                                break;
                                            case core::DataType::INT64:
                                                default_val = Value::makeInt64(std::stoll(col_info.default_value));
                                                break;
                                            case core::DataType::FLOAT64:
                                                default_val = Value::makeFloat64(std::stod(col_info.default_value));
                                                break;
                                            case core::DataType::VARCHAR:
                                                default_val = Value::makeVarchar(col_info.default_value);
                                                break;
                                            default:
                                                default_val = Value::makeNull();
                                                break;
                                        }
                                    }
                                    catch (...)
                                    {
                                        // Parsing failed - use NULL
                                        default_val = Value::makeNull();
                                    }

                                    default_values.push_back(default_val);
                                }
                            }

                            // For each matching child row, set FK columns to DEFAULT
                            for (const auto& tid : matching_tids)
                            {
                                // Fetch the current child tuple
                                core::Tuple child_tuple;
                                auto fetch_status = db_->storage_engine()->getTuple(
                                    fk.child_table_id, tid, &child_tuple, nullptr);

                                if (fetch_status != core::Status::OK)
                                {
                                    DEBUG_LOG_DB("FK SET DEFAULT: failed to fetch child tuple");
                                    continue;
                                }

                                // Modify the FK columns to DEFAULT values
                                std::vector<uint8_t> new_tuple_data;
                                if (!modifyTupleColumns(child_tuple.data, child_tuple.data_size,
                                                       child_columns, fk_col_indices,
                                                       default_values, new_tuple_data))
                                {
                                    error("FK SET DEFAULT: failed to modify child tuple for FK: " + fk.fk_name);
                                }

                                // Update the child row using storage engine
                                uint32_t page_id = static_cast<uint32_t>(core::getPageNumber(tid));
                                uint16_t item_id = core::getSlot(tid);
                                uint32_t new_page_id;
                                uint16_t new_item_id;

                                auto update_status = db_->storage_engine()->updateTuple(
                                    fk.child_table_id, page_id, item_id,
                                    new_tuple_data.data(), static_cast<uint32_t>(new_tuple_data.size()),
                                    &new_page_id, &new_item_id, nullptr);

                                if (update_status != core::Status::OK)
                                {
                                    error("FK SET DEFAULT: failed to update child row for FK: " + fk.fk_name);
                                }

                                DEBUG_LOG_DB("FK SET DEFAULT: updated child row (" +
                                           std::to_string(page_id) + "," + std::to_string(item_id) + ")");
                            }
                            break;
                        }
                    }
                }
            }
        }

        // ALPHA Phase B: Tuple modification helpers for FK actions
        // Serialize tuple from column values (all columns)
        bool Executor::serializeTupleFromValues(const std::vector<Value>& values,
                                               const std::vector<core::CatalogManager::ColumnInfo>& columns,
                                               std::vector<uint8_t>& tuple_data_out)
        {
            if (values.size() != columns.size())
            {
                return false; // Mismatch in value/column count
            }

            tuple_data_out.clear();

            // Reserve space for TupleHeader
            size_t header_offset = tuple_data_out.size();
            tuple_data_out.resize(tuple_data_out.size() + sizeof(core::TupleHeader));

            // Determine if we need a null bitmap
            bool has_nulls = false;
            for (const auto& val : values)
            {
                if (val.isNull())
                {
                    has_nulls = true;
                    break;
                }
            }

            // Add null bitmap if needed (one bit per column)
            size_t null_bitmap_offset = 0;
            if (has_nulls)
            {
                null_bitmap_offset = tuple_data_out.size();
                size_t bitmap_bytes = (columns.size() + 7) / 8;
                tuple_data_out.resize(tuple_data_out.size() + bitmap_bytes);
                // Initialize bitmap to zero
                std::fill(tuple_data_out.begin() + null_bitmap_offset,
                         tuple_data_out.begin() + null_bitmap_offset + bitmap_bytes, 0);
            }

            // Serialize each column value
            for (size_t i = 0; i < values.size(); i++)
            {
                const auto& value = values[i];
                const auto& col_info = columns[i];

                if (value.isNull())
                {
                    // Set null bit in bitmap
                    size_t bit_offset = i;
                    size_t byte_offset = null_bitmap_offset + (bit_offset / 8);
                    size_t bit_pos = bit_offset % 8;
                    tuple_data_out[byte_offset] |= (1 << bit_pos);
                    // Don't write any data for null values
                    continue;
                }

                // Serialize value based on column type
                core::DataType col_type = static_cast<core::DataType>(col_info.data_type);

                switch (col_type)
                {
                    case core::DataType::INT32:
                    {
                        int32_t val = static_cast<int32_t>(value.toInt64());
                        size_t offset = tuple_data_out.size();
                        tuple_data_out.resize(offset + sizeof(int32_t));
                        std::memcpy(&tuple_data_out[offset], &val, sizeof(int32_t));
                        break;
                    }
                    case core::DataType::INT64:
                    {
                        int64_t val = value.toInt64();
                        size_t offset = tuple_data_out.size();
                        tuple_data_out.resize(offset + sizeof(int64_t));
                        std::memcpy(&tuple_data_out[offset], &val, sizeof(int64_t));
                        break;
                    }
                    case core::DataType::FLOAT64:
                    {
                        double val = value.toDouble();
                        size_t offset = tuple_data_out.size();
                        tuple_data_out.resize(offset + sizeof(double));
                        std::memcpy(&tuple_data_out[offset], &val, sizeof(double));
                        break;
                    }
                    case core::DataType::VARCHAR:
                    {
                        std::string str = value.toString();
                        // Write length prefix (4 bytes) then data
                        uint32_t len = static_cast<uint32_t>(str.size());
                        size_t offset = tuple_data_out.size();
                        tuple_data_out.resize(offset + sizeof(uint32_t) + len);
                        std::memcpy(&tuple_data_out[offset], &len, sizeof(uint32_t));
                        std::memcpy(&tuple_data_out[offset + sizeof(uint32_t)], str.data(), len);
                        break;
                    }
                    default:
                        return false; // Unsupported type
                }
            }

            // Initialize TupleHeader
            auto* header = reinterpret_cast<core::TupleHeader*>(&tuple_data_out[header_offset]);
            std::memset(header, 0, sizeof(core::TupleHeader));
            header->infomask = has_nulls ? core::TupleHeader::HEAP_HAS_NULLS : 0;
            header->null_bitmap_offset = has_nulls ? static_cast<uint16_t>(null_bitmap_offset) : 0;

            return true;
        }

        // Modify specific columns in a tuple and reserialize
        bool Executor::modifyTupleColumns(const uint8_t* original_tuple, uint32_t original_size,
                                         const std::vector<core::CatalogManager::ColumnInfo>& all_columns,
                                         const std::vector<size_t>& column_indices,
                                         const std::vector<Value>& new_values,
                                         std::vector<uint8_t>& new_tuple_out)
        {
            if (column_indices.size() != new_values.size())
            {
                return false; // Mismatch in indices/values count
            }

            // Deserialize original tuple to get all current values
            std::vector<Value> current_values;
            if (!deserializeTuple(original_tuple, original_size, all_columns, current_values))
            {
                return false; // Failed to deserialize
            }

            // Replace specified column values with new values
            for (size_t i = 0; i < column_indices.size(); i++)
            {
                size_t col_idx = column_indices[i];
                if (col_idx >= current_values.size())
                {
                    return false; // Invalid column index
                }
                current_values[col_idx] = new_values[i];
            }

            // Reserialize with modified values
            return serializeTupleFromValues(current_values, all_columns, new_tuple_out);
        }

        // ========================================================================
        // BIT MANIPULATION FUNCTIONS (Nov 14, 2025)
        // ========================================================================

        void Executor::executeGetByte()
        {
            // GET_BYTE(bytes, offset) - extract byte at offset
            Value offset_val = stack_.top(); stack_.pop();
            Value bytes_val = stack_.top(); stack_.pop();

            int32_t offset = static_cast<int32_t>(offset_val.toInt64());
            std::string bytes = bytes_val.toString();

            if (offset < 0 || static_cast<size_t>(offset) >= bytes.size())
            {
                error("GET_BYTE: offset out of range");
            }

            uint8_t byte = static_cast<uint8_t>(bytes[offset]);
            stack_.push(Value::makeInt64(byte));
        }

        void Executor::executeSetByte()
        {
            // SET_BYTE(bytes, offset, value) - set byte at offset
            Value value_val = stack_.top(); stack_.pop();
            Value offset_val = stack_.top(); stack_.pop();
            Value bytes_val = stack_.top(); stack_.pop();

            int32_t offset = static_cast<int32_t>(offset_val.toInt64());
            int32_t byte_value = static_cast<int32_t>(value_val.toInt64());
            std::string bytes = bytes_val.toString();

            if (offset < 0 || static_cast<size_t>(offset) >= bytes.size())
            {
                error("SET_BYTE: offset out of range");
            }
            if (byte_value < 0 || byte_value > 255)
            {
                error("SET_BYTE: value must be 0-255");
            }

            bytes[offset] = static_cast<char>(byte_value);
            stack_.push(Value::makeVarchar(bytes));
        }

        void Executor::executeGetBit()
        {
            // GET_BIT(bytes, bit_offset) - get bit at bit offset
            Value bit_offset_val = stack_.top(); stack_.pop();
            Value bytes_val = stack_.top(); stack_.pop();

            int32_t bit_offset = static_cast<int32_t>(bit_offset_val.toInt64());
            std::string bytes = bytes_val.toString();

            int32_t byte_offset = bit_offset / 8;
            int32_t bit_pos = bit_offset % 8;

            if (byte_offset < 0 || static_cast<size_t>(byte_offset) >= bytes.size())
            {
                error("GET_BIT: bit offset out of range");
            }

            uint8_t byte = static_cast<uint8_t>(bytes[byte_offset]);
            int32_t bit = (byte >> (7 - bit_pos)) & 1;  // MSB first
            stack_.push(Value::makeInt64(bit));
        }

        void Executor::executeSetBit()
        {
            // SET_BIT(bytes, bit_offset, value) - set bit at bit offset
            Value value_val = stack_.top(); stack_.pop();
            Value bit_offset_val = stack_.top(); stack_.pop();
            Value bytes_val = stack_.top(); stack_.pop();

            int32_t bit_offset = static_cast<int32_t>(bit_offset_val.toInt64());
            int32_t bit_value = static_cast<int32_t>(value_val.toInt64());
            std::string bytes = bytes_val.toString();

            int32_t byte_offset = bit_offset / 8;
            int32_t bit_pos = bit_offset % 8;

            if (byte_offset < 0 || static_cast<size_t>(byte_offset) >= bytes.size())
            {
                error("SET_BIT: bit offset out of range");
            }
            if (bit_value != 0 && bit_value != 1)
            {
                error("SET_BIT: bit value must be 0 or 1");
            }

            uint8_t byte = static_cast<uint8_t>(bytes[byte_offset]);
            if (bit_value)
            {
                byte |= (1 << (7 - bit_pos));  // Set bit (MSB first)
            }
            else
            {
                byte &= ~(1 << (7 - bit_pos)); // Clear bit
            }
            bytes[byte_offset] = static_cast<char>(byte);

            stack_.push(Value::makeVarchar(bytes));
        }

        void Executor::executeBitAnd()
        {
            // BIT_AND(a, b) / a & b
            Value b = stack_.top(); stack_.pop();
            Value a = stack_.top(); stack_.pop();
            stack_.push(Value::makeInt64(a.toInt64() & b.toInt64()));
        }

        void Executor::executeBitOr()
        {
            // BIT_OR(a, b) / a | b
            Value b = stack_.top(); stack_.pop();
            Value a = stack_.top(); stack_.pop();
            stack_.push(Value::makeInt64(a.toInt64() | b.toInt64()));
        }

        void Executor::executeBitXor()
        {
            // BIT_XOR(a, b) / a ^ b
            Value b = stack_.top(); stack_.pop();
            Value a = stack_.top(); stack_.pop();
            stack_.push(Value::makeInt64(a.toInt64() ^ b.toInt64()));
        }

        void Executor::executeBitNot()
        {
            // BIT_NOT(a) / ~a
            Value a = stack_.top(); stack_.pop();
            stack_.push(Value::makeInt64(~a.toInt64()));
        }

        void Executor::executeBitShiftLeft()
        {
            // BIT_SHIFT_LEFT(a, n) / a << n
            Value n = stack_.top(); stack_.pop();
            Value a = stack_.top(); stack_.pop();
            stack_.push(Value::makeInt64(a.toInt64() << n.toInt64()));
        }

        void Executor::executeBitShiftRight()
        {
            // BIT_SHIFT_RIGHT(a, n) / a >> n (arithmetic right shift)
            Value n = stack_.top(); stack_.pop();
            Value a = stack_.top(); stack_.pop();
            stack_.push(Value::makeInt64(a.toInt64() >> n.toInt64()));
        }

        void Executor::executeBitShiftRightLogical()
        {
            // BIT_SHIFT_RIGHT_LOGICAL(a, n) / a >>> n (logical right shift, zero-fill)
            Value n = stack_.top(); stack_.pop();
            Value a = stack_.top(); stack_.pop();
            uint64_t unsigned_a = static_cast<uint64_t>(a.toInt64());
            stack_.push(Value::makeInt64(static_cast<int64_t>(unsigned_a >> n.toInt64())));
        }

        void Executor::executeBitCount()
        {
            // BIT_COUNT(a) - population count (count set bits)
            Value a = stack_.top(); stack_.pop();
            uint64_t val = static_cast<uint64_t>(a.toInt64());

            // Brian Kernighan's algorithm
            int count = 0;
            while (val)
            {
                val &= (val - 1);  // Clear lowest set bit
                count++;
            }

            stack_.push(Value::makeInt32(count));
        }

        void Executor::executeBitLength()
        {
            // BIT_LENGTH(bytes) - length in bits
            Value bytes_val = stack_.top(); stack_.pop();
            std::string bytes = bytes_val.toString();
            stack_.push(Value::makeInt32(static_cast<int32_t>(bytes.size() * 8)));
        }

        void Executor::executeBitMask()
        {
            // BIT_MASK(length) - create mask of N ones
            Value length_val = stack_.top(); stack_.pop();
            int32_t length = static_cast<int32_t>(length_val.toInt64());

            if (length < 0 || length > 64)
            {
                error("BIT_MASK: length must be 0-64");
            }

            uint64_t mask;
            if (length == 64)
            {
                mask = 0xFFFFFFFFFFFFFFFFULL;
            }
            else
            {
                mask = (1ULL << length) - 1;
            }

            stack_.push(Value::makeInt64(static_cast<int64_t>(mask)));
        }

        // ========================================================================
        // STATISTICAL FUNCTIONS (Nov 14, 2025) - TODO: Implement as aggregates
        // ========================================================================

        void Executor::executeStdDevSamp()
        {
            // TODO: Implement as aggregate function with Welford's algorithm
            error("STDDEV_SAMP not yet implemented");
        }

        void Executor::executeStdDevPop()
        {
            // TODO: Implement as aggregate function
            error("STDDEV_POP not yet implemented");
        }

        void Executor::executeVarSamp()
        {
            // TODO: Implement as aggregate function with Welford's algorithm
            error("VAR_SAMP not yet implemented");
        }

        void Executor::executeVarPop()
        {
            // TODO: Implement as aggregate function
            error("VAR_POP not yet implemented");
        }

        void Executor::executeCorr()
        {
            // TODO: Implement as aggregate function
            error("CORR not yet implemented");
        }

        void Executor::executeCovarPop()
        {
            // TODO: Implement as aggregate function
            error("COVAR_POP not yet implemented");
        }

        // ========================================================================
        // CRYPTOGRAPHIC FUNCTIONS (Nov 14, 2025) - TODO: Implement with OpenSSL
        // ========================================================================

        void Executor::executeMD5()
        {
            // Pop input string/bytes from stack
            Value input = stack_.top(); stack_.pop();

            // Handle NULL input
            if (input.isNull())
            {
                stack_.push(Value::makeNull());
                return;
            }

            // Convert input to string
            std::string data = input.toString();

            // Calculate MD5 hash (suppressing OpenSSL 3.0 deprecation for now)
            unsigned char hash[MD5_DIGEST_LENGTH];
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            MD5(reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), hash);
            #pragma GCC diagnostic pop

            // Convert to hex string
            std::stringstream ss;
            for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
            {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
            }

            stack_.push(Value::makeVarchar(ss.str()));
        }

        void Executor::executeSHA1()
        {
            // Pop input string/bytes from stack
            Value input = stack_.top(); stack_.pop();

            // Handle NULL input
            if (input.isNull())
            {
                stack_.push(Value::makeNull());
                return;
            }

            // Convert input to string
            std::string data = input.toString();

            // Calculate SHA1 hash
            unsigned char hash[SHA_DIGEST_LENGTH];
            SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), hash);

            // Convert to hex string
            std::stringstream ss;
            for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
            {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
            }

            stack_.push(Value::makeVarchar(ss.str()));
        }

        void Executor::executeSHA256()
        {
            // Pop input string/bytes from stack
            Value input = stack_.top(); stack_.pop();

            // Handle NULL input
            if (input.isNull())
            {
                stack_.push(Value::makeNull());
                return;
            }

            // Convert input to string
            std::string data = input.toString();

            // Calculate SHA256 hash
            unsigned char hash[SHA256_DIGEST_LENGTH];
            SHA256(reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), hash);

            // Convert to hex string
            std::stringstream ss;
            for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
            {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
            }

            stack_.push(Value::makeVarchar(ss.str()));
        }

        void Executor::executeSHA512()
        {
            // Pop input string/bytes from stack
            Value input = stack_.top(); stack_.pop();

            // Handle NULL input
            if (input.isNull())
            {
                stack_.push(Value::makeNull());
                return;
            }

            // Convert input to string
            std::string data = input.toString();

            // Calculate SHA512 hash
            unsigned char hash[SHA512_DIGEST_LENGTH];
            SHA512(reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), hash);

            // Convert to hex string
            std::stringstream ss;
            for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
            {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
            }

            stack_.push(Value::makeVarchar(ss.str()));
        }

        // ========================================================================
        // XML HELPER FUNCTIONS (Nov 14, 2025)
        // ========================================================================

#ifdef HAVE_LIBXML2
        // Helper: Convert xmlChar* to std::string
        static std::string xmlCharToString(const xmlChar* xmlStr)
        {
            if (!xmlStr) return "";
            return std::string(reinterpret_cast<const char*>(xmlStr));
        }

        // Helper: Convert xmlDocPtr to string representation
        static std::string xmlDocToString(xmlDocPtr doc)
        {
            if (!doc) return "";

            xmlChar* xmlbuff;
            int buffersize;
            xmlDocDumpFormatMemory(doc, &xmlbuff, &buffersize, 1);

            std::string result(reinterpret_cast<char*>(xmlbuff), buffersize);
            xmlFree(xmlbuff);

            return result;
        }

        // Helper: Get last XML error message
        static std::string getLastXMLError()
        {
            xmlErrorPtr err = xmlGetLastError();
            if (err) {
                return std::string(err->message);
            }
            return "Unknown XML error";
        }

        // Helper: Initialize libxml2 (call once)
        static void initLibXML2()
        {
            static bool initialized = false;
            if (!initialized) {
                xmlInitParser();
                initialized = true;
            }
        }

        // Helper: Cleanup libxml2 (call at shutdown)
        static void cleanupLibXML2()
        {
            xmlCleanupParser();
        }
#endif

        // ========================================================================
        // XML FUNCTIONS (Nov 14, 2025)
        // ========================================================================

        void Executor::executeXMLParse()
        {
#ifdef HAVE_LIBXML2
            // Initialize libxml2
            initLibXML2();

            // XMLPARSE(DOCUMENT|CONTENT, xml_text)
            // Pop xml_text from stack
            Value xml_text = stack_.top(); stack_.pop();

            // Pop document_or_content flag (ignored for now)
            Value mode = stack_.top(); stack_.pop();

            if (xml_text.isNull())
            {
                stack_.push(Value::makeNull());
                return;
            }

            std::string xml_str = xml_text.toString();

            // Parse XML with full validation using libxml2
            xmlDocPtr doc = xmlReadMemory(
                xml_str.c_str(),
                xml_str.length(),
                nullptr,
                nullptr,
                XML_PARSE_NONET | XML_PARSE_NOENT
            );

            if (!doc)
            {
                std::string err_msg = getLastXMLError();
                xmlResetLastError();
                // SECURITY FIX (HIGH-1): Log detailed error internally, return generic error to client
                LOG_ERROR(EXECUTOR, "Invalid XML: %s", err_msg.c_str());
                error("Invalid XML");
            }

            // Convert back to string (validates and normalizes)
            std::string result = xmlDocToString(doc);
            xmlFreeDoc(doc);

            stack_.push(Value::makeVarchar(result));
#else
            // Fallback: basic string-based implementation without libxml2
            Value xml_text = stack_.top(); stack_.pop();
            Value mode = stack_.top(); stack_.pop();

            if (xml_text.isNull())
            {
                stack_.push(Value::makeNull());
                return;
            }

            std::string xml_str = xml_text.toString();

            // Basic XML validation - check if it looks like XML
            if (xml_str.empty() || xml_str.find('<') == std::string::npos)
            {
                error("Invalid XML: must contain at least one element");
            }

            // Return the XML as a string
            stack_.push(Value::makeVarchar(xml_str));
#endif
        }

        void Executor::executeXMLSerialize()
        {
            // XMLSERIALIZE(CONTENT|DOCUMENT xml AS type)
            // Pop type (e.g., VARCHAR)
            Value type = stack_.top(); stack_.pop();

            // Pop XML value
            Value xml = stack_.top(); stack_.pop();

            // Pop mode (CONTENT or DOCUMENT)
            Value mode = stack_.top(); stack_.pop();

            if (xml.isNull())
            {
                stack_.push(Value::makeNull());
                return;
            }

            // Convert XML to string
            std::string xml_str = xml.toString();

            // Return as VARCHAR
            stack_.push(Value::makeVarchar(xml_str));
        }

        void Executor::executeXMLElement()
        {
            // XMLELEMENT(NAME element_name [, attributes] [, content])
            // For simplicity: XMLELEMENT(NAME name, content)

            // Pop content
            Value content = stack_.top(); stack_.pop();

            // Pop element name
            Value name = stack_.top(); stack_.pop();

            if (name.isNull())
            {
                error("XML element name cannot be NULL");
            }

            std::string element_name = name.toString();
            std::string element_content = content.isNull() ? "" : content.toString();

            // Build XML element
            std::string xml = "<" + element_name + ">" + element_content + "</" + element_name + ">";

            stack_.push(Value::makeVarchar(xml));
        }

        void Executor::executeXMLConcat()
        {
            // XMLCONCAT(xml1, xml2, ...)
            // Number of arguments is on the stack
            uint8_t arg_count = readByte();

            std::vector<std::string> xml_parts;

            for (uint8_t i = 0; i < arg_count; i++)
            {
                Value xml_val = stack_.top(); stack_.pop();
                if (!xml_val.isNull())
                {
                    xml_parts.push_back(xml_val.toString());
                }
            }

            // Concatenate all XML parts
            std::string result;
            for (auto it = xml_parts.rbegin(); it != xml_parts.rend(); ++it)
            {
                result += *it;
            }

            stack_.push(Value::makeVarchar(result));
        }

        void Executor::executeXMLForest()
        {
            // XMLFOREST(expr1 AS name1, expr2 AS name2, ...)
            // Creates: <name1>expr1</name1><name2>expr2</name2>...

            uint8_t arg_count = readByte();

            std::vector<std::pair<std::string, std::string>> elements;

            for (uint8_t i = 0; i < arg_count; i++)
            {
                // Pop name
                Value name = stack_.top(); stack_.pop();
                // Pop value
                Value value = stack_.top(); stack_.pop();

                if (!value.isNull() && !name.isNull())
                {
                    elements.push_back({name.toString(), value.toString()});
                }
            }

            // Build XML forest
            std::string result;
            for (auto it = elements.rbegin(); it != elements.rend(); ++it)
            {
                result += "<" + it->first + ">" + it->second + "</" + it->first + ">";
            }

            stack_.push(Value::makeVarchar(result));
        }

        void Executor::executeXMLComment()
        {
            // XMLCOMMENT(text)
            Value text = stack_.top(); stack_.pop();

            if (text.isNull())
            {
                stack_.push(Value::makeNull());
                return;
            }

            std::string comment_text = text.toString();

            // Check for invalid comment content (-- is not allowed in XML comments)
            if (comment_text.find("--") != std::string::npos)
            {
                error("XML comment cannot contain '--'");
            }

            std::string xml = "<!--" + comment_text + "-->";
            stack_.push(Value::makeVarchar(xml));
        }

        void Executor::executeXMLRoot()
        {
            // XMLROOT(xml, VERSION version [, STANDALONE yes|no])
            // Simplified: XMLROOT(xml, version, standalone)

            // Pop standalone flag (yes=1, no=0, omitted=-1)
            Value standalone = stack_.top(); stack_.pop();

            // Pop version
            Value version = stack_.top(); stack_.pop();

            // Pop XML content
            Value xml = stack_.top(); stack_.pop();

            if (xml.isNull())
            {
                stack_.push(Value::makeNull());
                return;
            }

            std::string xml_content = xml.toString();
            std::string version_str = version.isNull() ? "1.0" : version.toString();

            // Build XML declaration
            std::string declaration = "<?xml version=\"" + version_str + "\"";

            if (!standalone.isNull())
            {
                int standalone_val = static_cast<int>(standalone.toInt64());
                if (standalone_val == 1)
                {
                    declaration += " standalone=\"yes\"";
                }
                else if (standalone_val == 0)
                {
                    declaration += " standalone=\"no\"";
                }
            }

            declaration += "?>";

            std::string result = declaration + xml_content;
            stack_.push(Value::makeVarchar(result));
        }

        void Executor::executeXPath()
        {
            // XPATH(xpath_expr, xml)
            Value xml = stack_.top(); stack_.pop();
            Value xpath_expr = stack_.top(); stack_.pop();

            if (xml.isNull() || xpath_expr.isNull())
            {
                stack_.push(Value::makeNull());
                return;
            }

#ifdef HAVE_LIBXML2
            initLibXML2();

            std::string xml_str = xml.toString();
            std::string xpath = xpath_expr.toString();

            // Parse XML
            xmlDocPtr doc = xmlReadMemory(
                xml_str.c_str(),
                xml_str.length(),
                nullptr,
                nullptr,
                XML_PARSE_NONET | XML_PARSE_NOENT
            );

            if (!doc)
            {
                std::string err_msg = getLastXMLError();
                xmlResetLastError();
                error("Invalid XML in XPATH: " + err_msg);
            }

            // Create XPath context
            xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
            if (!xpathCtx)
            {
                xmlFreeDoc(doc);
                error("Failed to create XPath context");
            }

            // Evaluate XPath expression
            xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(
                reinterpret_cast<const xmlChar*>(xpath.c_str()),
                xpathCtx
            );

            if (!xpathObj)
            {
                xmlXPathFreeContext(xpathCtx);
                xmlFreeDoc(doc);
                error("Invalid XPath expression: " + xpath);
            }

            // Convert result to SQL array of strings
            std::vector<std::string> results;

            if (xpathObj->type == XPATH_NODESET && xpathObj->nodesetval)
            {
                xmlNodeSetPtr nodeset = xpathObj->nodesetval;
                for (int i = 0; i < nodeset->nodeNr; i++)
                {
                    xmlNodePtr node = nodeset->nodeTab[i];
                    xmlChar* content = xmlNodeGetContent(node);
                    if (content)
                    {
                        results.push_back(xmlCharToString(content));
                        xmlFree(content);
                    }
                }
            }

            // Cleanup
            xmlXPathFreeObject(xpathObj);
            xmlXPathFreeContext(xpathCtx);
            xmlFreeDoc(doc);

            // Build JSON array result
            json j_array = json::array();
            for (const auto& res : results)
            {
                j_array.push_back(res);
            }

            stack_.push(Value::makeVarchar(j_array.dump()));
#else
            error("XPATH requires libxml2 library - not available in this build");
#endif
        }

        void Executor::executeXMLExists()
        {
            // XMLEXISTS(xpath_expr, xml) - returns boolean
            Value xml = stack_.top(); stack_.pop();
            Value xpath_expr = stack_.top(); stack_.pop();

            if (xml.isNull() || xpath_expr.isNull())
            {
                stack_.push(Value::makeNull());
                return;
            }

#ifdef HAVE_LIBXML2
            initLibXML2();

            std::string xml_str = xml.toString();
            std::string xpath = xpath_expr.toString();

            // Parse XML
            xmlDocPtr doc = xmlReadMemory(
                xml_str.c_str(),
                xml_str.length(),
                nullptr,
                nullptr,
                XML_PARSE_NONET | XML_PARSE_NOENT
            );

            if (!doc)
            {
                std::string err_msg = getLastXMLError();
                xmlResetLastError();
                error("Invalid XML in XMLEXISTS: " + err_msg);
            }

            // Create XPath context
            xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
            if (!xpathCtx)
            {
                xmlFreeDoc(doc);
                error("Failed to create XPath context");
            }

            // Evaluate XPath expression
            xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(
                reinterpret_cast<const xmlChar*>(xpath.c_str()),
                xpathCtx
            );

            bool exists = false;

            if (xpathObj)
            {
                // Check if any nodes were found
                if (xpathObj->type == XPATH_NODESET && xpathObj->nodesetval)
                {
                    exists = (xpathObj->nodesetval->nodeNr > 0);
                }
                else if (xpathObj->type == XPATH_BOOLEAN)
                {
                    exists = xpathObj->boolval;
                }

                xmlXPathFreeObject(xpathObj);
            }

            // Cleanup
            xmlXPathFreeContext(xpathCtx);
            xmlFreeDoc(doc);

            stack_.push(Value::makeBoolean(exists));
#else
            error("XMLEXISTS requires libxml2 library - not available in this build");
#endif
        }

        void Executor::executeExtract()
        {
            // EXTRACT(field FROM value) - extract sub-information from complex types
            // Bytecode format: field_id (uint8_t), then source expression is already evaluated

            // Read field ID from bytecode
            uint8_t field_id = readByte();
            ExtractField field = static_cast<ExtractField>(field_id);

            // Source value is on top of stack (already evaluated by bytecode)
            Value source = pop();

            // Handle NULL source
            if (source.isNull())
            {
                push(Value::makeNull());
                return;
            }

            // Dispatch based on source type and field
            core::DataType source_type = source.type();

            // ===== TEMPORAL TYPES =====
            if (source_type == core::DataType::DATE)
            {
                int64_t days = source.getDate();

                switch (field)
                {
                    case ExtractField::YEAR:
                        push(Value::makeInt32(core::TypeExtractor::extractYear(days)));
                        return;
                    case ExtractField::MONTH:
                        push(Value::makeInt32(core::TypeExtractor::extractMonth(days)));
                        return;
                    case ExtractField::DAY:
                        push(Value::makeInt32(core::TypeExtractor::extractDay(days)));
                        return;
                    case ExtractField::DOW:
                        push(Value::makeInt32(core::TypeExtractor::extractDayOfWeek(days)));
                        return;
                    case ExtractField::DOY:
                        push(Value::makeInt32(core::TypeExtractor::extractDayOfYear(days)));
                        return;
                    case ExtractField::QUARTER:
                    {
                        int32_t month = core::TypeExtractor::extractMonth(days);
                        push(Value::makeInt32((month - 1) / 3 + 1));  // Quarter 1-4
                        return;
                    }
                    case ExtractField::EPOCH:
                    {
                        // Epoch for DATE: days * seconds_per_day
                        push(Value::makeInt64(days * 86400));
                        return;
                    }
                    default:
                        error("Field '" + std::to_string(field_id) + "' not valid for DATE type");
                        return;
                }
            }
            else if (source_type == core::DataType::TIME)
            {
                int64_t microseconds = source.getTime();

                switch (field)
                {
                    case ExtractField::HOUR:
                        push(Value::makeInt32(core::TypeExtractor::extractHour(microseconds)));
                        return;
                    case ExtractField::MINUTE:
                        push(Value::makeInt32(core::TypeExtractor::extractMinute(microseconds)));
                        return;
                    case ExtractField::SECOND:
                        push(Value::makeInt32(core::TypeExtractor::extractSecond(microseconds)));
                        return;
                    case ExtractField::MICROSECOND:
                        push(Value::makeInt32(core::TypeExtractor::extractMicrosecond(microseconds)));
                        return;
                    case ExtractField::MILLISECOND:
                    {
                        int32_t us = core::TypeExtractor::extractMicrosecond(microseconds);
                        push(Value::makeInt32(us / 1000));  // Millisecond part only
                        return;
                    }
                    case ExtractField::EPOCH:
                    {
                        // Epoch for TIME: microseconds / 1000000 (seconds since midnight)
                        push(Value::makeInt64(microseconds / 1000000));
                        return;
                    }
                    default:
                        error("Field '" + std::to_string(field_id) + "' not valid for TIME type");
                        return;
                }
            }
            else if (source_type == core::DataType::TIMESTAMP)
            {
                int64_t microseconds = source.getTimestamp();

                switch (field)
                {
                    case ExtractField::YEAR:
                        push(Value::makeInt32(core::TypeExtractor::extractTimestampYear(microseconds)));
                        return;
                    case ExtractField::MONTH:
                        push(Value::makeInt32(core::TypeExtractor::extractTimestampMonth(microseconds)));
                        return;
                    case ExtractField::DAY:
                        push(Value::makeInt32(core::TypeExtractor::extractTimestampDay(microseconds)));
                        return;
                    case ExtractField::HOUR:
                        push(Value::makeInt32(core::TypeExtractor::extractTimestampHour(microseconds)));
                        return;
                    case ExtractField::MINUTE:
                        push(Value::makeInt32(core::TypeExtractor::extractTimestampMinute(microseconds)));
                        return;
                    case ExtractField::SECOND:
                        push(Value::makeInt32(core::TypeExtractor::extractTimestampSecond(microseconds)));
                        return;
                    case ExtractField::MICROSECOND:
                        push(Value::makeInt32(core::TypeExtractor::extractTimestampMicrosecond(microseconds)));
                        return;
                    case ExtractField::MILLISECOND:
                    {
                        int32_t us = core::TypeExtractor::extractTimestampMicrosecond(microseconds);
                        push(Value::makeInt32(us / 1000));
                        return;
                    }
                    case ExtractField::DOW:
                    {
                        // Convert microseconds to days
                        int64_t days = microseconds / 86400000000LL;
                        push(Value::makeInt32(core::TypeExtractor::extractDayOfWeek(days)));
                        return;
                    }
                    case ExtractField::DOY:
                    {
                        int64_t days = microseconds / 86400000000LL;
                        push(Value::makeInt32(core::TypeExtractor::extractDayOfYear(days)));
                        return;
                    }
                    case ExtractField::QUARTER:
                    {
                        int32_t month = core::TypeExtractor::extractTimestampMonth(microseconds);
                        push(Value::makeInt32((month - 1) / 3 + 1));
                        return;
                    }
                    case ExtractField::EPOCH:
                    {
                        // Epoch: seconds since Unix epoch
                        push(Value::makeInt64(microseconds / 1000000));
                        return;
                    }
                    default:
                        error("Field '" + std::to_string(field_id) + "' not valid for TIMESTAMP type");
                        return;
                }
            }
            else if (source_type == core::DataType::INTERVAL)
            {
                core::Interval interval = source.getInterval();

                switch (field)
                {
                    case ExtractField::YEAR:
                        // Years from months (12 months = 1 year)
                        push(Value::makeInt32(interval.months / 12));
                        return;
                    case ExtractField::MONTH:
                        // Remaining months after extracting full years
                        push(Value::makeInt32(interval.months % 12));
                        return;
                    case ExtractField::DAY:
                        push(Value::makeInt32(interval.days));
                        return;
                    case ExtractField::HOUR:
                    {
                        // Hours from microseconds
                        int64_t hours = interval.microseconds / 3600000000LL;
                        push(Value::makeInt32(static_cast<int32_t>(hours)));
                        return;
                    }
                    case ExtractField::MINUTE:
                    {
                        // Minutes from microseconds (after extracting hours)
                        int64_t total_minutes = interval.microseconds / 60000000LL;
                        int32_t minutes = static_cast<int32_t>(total_minutes % 60);
                        push(Value::makeInt32(minutes));
                        return;
                    }
                    case ExtractField::SECOND:
                    {
                        // Seconds from microseconds (after extracting minutes)
                        int64_t total_seconds = interval.microseconds / 1000000LL;
                        int32_t seconds = static_cast<int32_t>(total_seconds % 60);
                        push(Value::makeInt32(seconds));
                        return;
                    }
                    case ExtractField::MICROSECOND:
                    {
                        // Microsecond component only (fractional part of second)
                        int32_t us = static_cast<int32_t>(interval.microseconds % 1000000);
                        push(Value::makeInt32(us));
                        return;
                    }
                    case ExtractField::MILLISECOND:
                    {
                        // Millisecond component only
                        int32_t ms = static_cast<int32_t>((interval.microseconds % 1000000) / 1000);
                        push(Value::makeInt32(ms));
                        return;
                    }
                    case ExtractField::EPOCH:
                    {
                        // Total interval as seconds (approximate, assumes 30 days/month)
                        double total_seconds =
                            (interval.months * 2592000.0) +  // 30 days/month in seconds
                            (interval.days * 86400.0) +       // days to seconds
                            (interval.microseconds / 1000000.0); // microseconds to seconds
                        push(Value::makeFloat64(total_seconds));
                        return;
                    }
                    default:
                        error("Field '" + std::to_string(field_id) + "' not valid for INTERVAL type");
                        return;
                }
            }
            // ===== UUID TYPE =====
            else if (source_type == core::DataType::UUID)
            {
                std::vector<uint8_t> uuid = source.getUUID();

                switch (field)
                {
                    case ExtractField::VERSION:
                        push(Value::makeInt32(core::TypeExtractor::extractUUIDVersion(uuid)));
                        return;
                    case ExtractField::VARIANT:
                        push(Value::makeInt32(core::TypeExtractor::extractUUIDVariant(uuid)));
                        return;
                    case ExtractField::TIMESTAMP:
                    {
                        core::ErrorContext ctx;
                        auto timestamp = core::TypeExtractor::extractUUIDTimestamp(uuid, &ctx);
                        if (timestamp.has_value())
                        {
                            push(Value::makeInt64(*timestamp));
                        }
                        else
                        {
                            error("UUID timestamp extraction failed: only supported for UUIDv1 and UUIDv7");
                        }
                        return;
                    }
                    default:
                        error("Field '" + std::to_string(field_id) + "' not yet implemented for UUID type");
                        return;
                }
            }
            // ===== ARRAY TYPE =====
            else if (source_type == core::DataType::ARRAY)
            {
                auto array = source.getArray();
                if (!array)
                {
                    push(Value::makeNull());
                    return;
                }

                switch (field)
                {
                    case ExtractField::CARDINALITY:
                        push(Value::makeInt32(static_cast<int32_t>(array->getTotalElements())));
                        return;
                    case ExtractField::NDIMS:
                        push(Value::makeInt32(static_cast<int32_t>(array->getRank())));
                        return;
                    case ExtractField::LOWER:
                        // PostgreSQL arrays are 1-indexed by default
                        push(Value::makeInt32(1));
                        return;
                    case ExtractField::UPPER:
                    {
                        auto dims = array->getDimensions();
                        if (!dims.empty())
                        {
                            push(Value::makeInt32(static_cast<int32_t>(dims[0])));
                        }
                        else
                        {
                            push(Value::makeNull());
                        }
                        return;
                    }
                    default:
                        error("Field '" + std::to_string(field_id) + "' not yet implemented for ARRAY type");
                        return;
                }
            }
            // ===== SPATIAL TYPES =====
            else if (source_type == core::DataType::POINT)
            {
                core::Point point = source.getPoint();

                switch (field)
                {
                    case ExtractField::X:
                        push(Value::makeFloat64(point.x));
                        return;
                    case ExtractField::Y:
                        push(Value::makeFloat64(point.y));
                        return;
                    case ExtractField::SRID:
                        push(Value::makeInt32(point.srid));
                        return;
                    default:
                        error("Field '" + std::to_string(field_id) + "' not valid for POINT type");
                        return;
                }
            }
            else
            {
                error("EXTRACT not yet supported for this data type");
            }
        }

        void Executor::executeEncode()
        {
            // TODO: Implement ENCODE(data, format) - base64, hex, escape
            error("ENCODE not yet implemented");
        }

        void Executor::executeDecode()
        {
            // TODO: Implement DECODE(text, format)
            error("DECODE not yet implemented");
        }

        // ============================================================================
        // INDEX OPERATION EXECUTORS (November 19, 2025)
        // ============================================================================

        void Executor::executeIndexInsert()
        {
            // Read index UUID (16 bytes)
            core::ID index_uuid;
            for (int i = 0; i < 16; i++)
            {
                if (pc_ >= bytecode_size_)
                {
                    error("Incomplete index UUID in EXT_INDEX_INSERT");
                    return;
                }
                index_uuid.bytes[i] = bytecode_[pc_++];
            }

            // Read index type (1 byte)
            if (pc_ >= bytecode_size_)
            {
                error("Missing index type in EXT_INDEX_INSERT");
                return;
            }
            IndexType index_type = static_cast<IndexType>(bytecode_[pc_++]);

            // Read key length (2 bytes, little-endian)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete key length in EXT_INDEX_INSERT");
                return;
            }
            uint16_t key_len = bytecode_[pc_] | (bytecode_[pc_ + 1] << 8);
            pc_ += 2;

            // Read key data
            if (pc_ + key_len > bytecode_size_)
            {
                error("Incomplete key data in EXT_INDEX_INSERT");
                return;
            }
            std::vector<uint8_t> key(&bytecode_[0] + pc_, &bytecode_[0] + pc_ + key_len);
            pc_ += key_len;

            // Read TID (10 bytes: GPID 8 + slot 2)
            if (pc_ + 10 > bytecode_size_)
            {
                error("Incomplete TID in EXT_INDEX_INSERT");
                return;
            }
            uint64_t gpid = 0;
            for (int i = 0; i < 8; i++)
            {
                gpid |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }
            uint16_t slot = bytecode_[pc_] | (bytecode_[pc_ + 1] << 8);
            pc_ += 2;
            core::TID tid(gpid, slot);

            // Read xmin (8 bytes)
            if (pc_ + 8 > bytecode_size_)
            {
                error("Incomplete xmin in EXT_INDEX_INSERT");
                return;
            }
            uint64_t xmin = 0;
            for (int i = 0; i < 8; i++)
            {
                xmin |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Route to appropriate index implementation
            core::ErrorContext err_ctx;
            core::Status status = routeIndexInsert(index_type, index_uuid, key, tid, xmin, &err_ctx);

            if (status != core::Status::OK)
            {
                // SECURITY FIX (LOW-7): Log detailed error internally, return generic error to client
                LOG_ERROR(EXECUTOR, "Index insert failed: %s", err_ctx.message.c_str());
                error("Index insert failed");
            }
        }

        void Executor::executeIndexSearch()
        {
            // Read index UUID (16 bytes)
            core::ID index_uuid;
            for (int i = 0; i < 16; i++)
            {
                if (pc_ >= bytecode_size_)
                {
                    error("Incomplete index UUID in EXT_INDEX_SEARCH");
                    return;
                }
                index_uuid.bytes[i] = bytecode_[pc_++];
            }

            // Read index type (1 byte)
            if (pc_ >= bytecode_size_)
            {
                error("Missing index type in EXT_INDEX_SEARCH");
                return;
            }
            IndexType index_type = static_cast<IndexType>(bytecode_[pc_++]);

            // Read key length (2 bytes)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete key length in EXT_INDEX_SEARCH");
                return;
            }
            uint16_t key_len = bytecode_[pc_] | (bytecode_[pc_ + 1] << 8);
            pc_ += 2;

            // Read key data
            if (pc_ + key_len > bytecode_size_)
            {
                error("Incomplete key data in EXT_INDEX_SEARCH");
                return;
            }
            std::vector<uint8_t> key(&bytecode_[0] + pc_, &bytecode_[0] + pc_ + key_len);
            pc_ += key_len;

            // Read current_xid (8 bytes)
            if (pc_ + 8 > bytecode_size_)
            {
                error("Incomplete current_xid in EXT_INDEX_SEARCH");
                return;
            }
            uint64_t current_xid = 0;
            for (int i = 0; i < 8; i++)
            {
                current_xid |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Route to appropriate index implementation
            std::vector<core::TID> results;
            core::ErrorContext err_ctx;
            core::Status status = routeIndexSearch(index_type, index_uuid, key, current_xid, &results, &err_ctx);

            if (status != core::Status::OK)
            {
                // SECURITY FIX (LOW-7): Log detailed error internally, return generic error to client
                LOG_ERROR(EXECUTOR, "Index search failed: %s", err_ctx.message.c_str());
                error("Index search failed");
                return;
            }

            // Push results count onto stack
            push(Value::makeInt64(static_cast<int64_t>(results.size())));

            // Store results in execution context for subsequent operations
            // (In a real implementation, you'd store this in the executor state)
        }

        void Executor::executeIndexScan()
        {
            // Read index UUID (16 bytes)
            core::ID index_uuid;
            for (int i = 0; i < 16; i++)
            {
                if (pc_ >= bytecode_size_)
                {
                    error("Incomplete index UUID in EXT_INDEX_SCAN");
                    return;
                }
                index_uuid.bytes[i] = bytecode_[pc_++];
            }

            // Read index type (1 byte)
            if (pc_ >= bytecode_size_)
            {
                error("Missing index type in EXT_INDEX_SCAN");
                return;
            }
            IndexType index_type = static_cast<IndexType>(bytecode_[pc_++]);

            // Read start key length (2 bytes, little-endian)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete start key length in EXT_INDEX_SCAN");
                return;
            }
            uint16_t start_key_len = bytecode_[pc_++];
            start_key_len |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);

            // Read start key (if not unbounded)
            std::vector<uint8_t> start_key_data;
            const std::vector<uint8_t>* start_key_ptr = nullptr;
            if (start_key_len != 0xFFFF)
            {
                if (pc_ + start_key_len > bytecode_size_)
                {
                    error("Incomplete start key data in EXT_INDEX_SCAN");
                    return;
                }
                start_key_data.resize(start_key_len);
                for (uint16_t i = 0; i < start_key_len; i++)
                {
                    start_key_data[i] = bytecode_[pc_++];
                }
                start_key_ptr = &start_key_data;
            }

            // Read end key length (2 bytes, little-endian)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete end key length in EXT_INDEX_SCAN");
                return;
            }
            uint16_t end_key_len = bytecode_[pc_++];
            end_key_len |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);

            // Read end key (if not unbounded)
            std::vector<uint8_t> end_key_data;
            const std::vector<uint8_t>* end_key_ptr = nullptr;
            if (end_key_len != 0xFFFF)
            {
                if (pc_ + end_key_len > bytecode_size_)
                {
                    error("Incomplete end key data in EXT_INDEX_SCAN");
                    return;
                }
                end_key_data.resize(end_key_len);
                for (uint16_t i = 0; i < end_key_len; i++)
                {
                    end_key_data[i] = bytecode_[pc_++];
                }
                end_key_ptr = &end_key_data;
            }

            // Read flags (1 byte)
            if (pc_ >= bytecode_size_)
            {
                error("Missing flags in EXT_INDEX_SCAN");
                return;
            }
            uint8_t flags = bytecode_[pc_++];
            bool start_inclusive = (flags & 0x01) != 0;
            bool end_inclusive = (flags & 0x02) != 0;

            // Read current_xid (8 bytes, little-endian)
            if (pc_ + 7 >= bytecode_size_)
            {
                error("Incomplete current_xid in EXT_INDEX_SCAN");
                return;
            }
            uint64_t current_xid = 0;
            for (int i = 0; i < 8; i++)
            {
                current_xid |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Route to appropriate index implementation
            std::vector<core::TID> results;
            core::ErrorContext err_ctx;
            core::Status status = routeIndexScan(index_type, index_uuid, start_key_ptr, end_key_ptr,
                                                start_inclusive, end_inclusive, current_xid,
                                                &results, &err_ctx);

            if (status != core::Status::OK)
            {
                error("Index scan failed: " + std::string(err_ctx.message));
                return;
            }

            // Push results count onto stack
            push(Value::makeInt64(static_cast<int64_t>(results.size())));

            // Store results in execution context for subsequent operations
            // (Implementation-dependent: could push array, store in temp table, etc.)
        }

        void Executor::executeIndexDelete()
        {
            // Read index UUID (16 bytes)
            core::ID index_uuid;
            for (int i = 0; i < 16; i++)
            {
                if (pc_ >= bytecode_size_)
                {
                    error("Incomplete index UUID in EXT_INDEX_DELETE");
                    return;
                }
                index_uuid.bytes[i] = bytecode_[pc_++];
            }

            // Read index type (1 byte)
            if (pc_ >= bytecode_size_)
            {
                error("Missing index type in EXT_INDEX_DELETE");
                return;
            }
            IndexType index_type = static_cast<IndexType>(bytecode_[pc_++]);

            // Read key length (2 bytes)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete key length in EXT_INDEX_DELETE");
                return;
            }
            uint16_t key_len = bytecode_[pc_] | (bytecode_[pc_ + 1] << 8);
            pc_ += 2;

            // Read key data
            if (pc_ + key_len > bytecode_size_)
            {
                error("Incomplete key data in EXT_INDEX_DELETE");
                return;
            }
            std::vector<uint8_t> key(&bytecode_[0] + pc_, &bytecode_[0] + pc_ + key_len);
            pc_ += key_len;

            // Read TID (10 bytes)
            if (pc_ + 10 > bytecode_size_)
            {
                error("Incomplete TID in EXT_INDEX_DELETE");
                return;
            }
            uint64_t gpid = 0;
            for (int i = 0; i < 8; i++)
            {
                gpid |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }
            uint16_t slot = bytecode_[pc_] | (bytecode_[pc_ + 1] << 8);
            pc_ += 2;
            core::TID tid(gpid, slot);

            // Read xmax (8 bytes)
            if (pc_ + 8 > bytecode_size_)
            {
                error("Incomplete xmax in EXT_INDEX_DELETE");
                return;
            }
            uint64_t xmax = 0;
            for (int i = 0; i < 8; i++)
            {
                xmax |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Route to appropriate index implementation
            core::ErrorContext err_ctx;
            core::Status status = routeIndexDelete(index_type, index_uuid, key, tid, xmax, &err_ctx);

            if (status != core::Status::OK)
            {
                // SECURITY FIX (LOW-7): Log detailed error internally, return generic error to client
                LOG_ERROR(EXECUTOR, "Index delete failed: %s", err_ctx.message.c_str());
                error("Index delete failed");
            }
        }

        // ============================================================================
        // SPECIALIZED INDEX OPERATIONS (November 19, 2025)
        // Operations for indexes with unique APIs (GIN, HNSW, Columnstore)
        // ============================================================================

        void Executor::executeGinInsert()
        {
            // GIN INSERT Bytecode Format:
            // - Index UUID (16 bytes)
            // - Value length (2 bytes)
            // - Value data (variable)
            // - TID: GPID (8 bytes) + slot (2 bytes)
            // - xmin (8 bytes)
            // - Extractor ID (2 bytes) - identifies the key extraction function

            // Read index UUID (16 bytes)
            core::ID index_uuid;
            for (int i = 0; i < 16; i++)
            {
                if (pc_ >= bytecode_size_)
                {
                    error("Incomplete index UUID in EXT_GIN_INSERT");
                    return;
                }
                index_uuid.bytes[i] = bytecode_[pc_++];
            }

            // Read value length (2 bytes, little-endian)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete value length in EXT_GIN_INSERT");
                return;
            }
            uint16_t value_len = bytecode_[pc_++];
            value_len |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);

            // Read value data
            if (pc_ + value_len > bytecode_size_)
            {
                error("Incomplete value data in EXT_GIN_INSERT");
                return;
            }
            std::vector<uint8_t> value(&bytecode_[0] + pc_, &bytecode_[0] + pc_ + value_len);
            pc_ += value_len;

            // Read TID (10 bytes: GPID 8 + slot 2)
            if (pc_ + 10 > bytecode_size_)
            {
                error("Incomplete TID in EXT_GIN_INSERT");
                return;
            }
            uint64_t gpid = 0;
            for (int i = 0; i < 8; i++)
            {
                gpid |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }
            uint16_t slot = bytecode_[pc_++];
            slot |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);
            core::TID tid(gpid, slot);

            // Read xmin (8 bytes, little-endian)
            if (pc_ + 7 >= bytecode_size_)
            {
                error("Incomplete xmin in EXT_GIN_INSERT");
                return;
            }
            uint64_t xmin = 0;
            for (int i = 0; i < 8; i++)
            {
                xmin |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Read extractor ID (2 bytes)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete extractor ID in EXT_GIN_INSERT");
                return;
            }
            uint16_t extractor_id = bytecode_[pc_++];
            extractor_id |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);

            // Get the key extractor function from registry
            // NOTE: This assumes a key extractor registry exists
            // For now, we'll use a placeholder implementation
            core::ErrorContext err_ctx;
            core::CatalogManager::IndexInfo index_info;
            auto status = db_->catalog_manager()->getIndex(index_uuid, index_info, &err_ctx);
            if (status != core::Status::OK)
            {
                error("Index not found: " + std::string(err_ctx.message));
                return;
            }

            auto gin = core::GinIndex::open(db_, index_uuid, index_info.root_page, &err_ctx);
            if (!gin)
            {
                error("Failed to open GIN index");
                return;
            }

            // Get key extractor from registry (November 20, 2025)
            auto extractor = GinExtractorRegistry::instance().getExtractor(extractor_id);
            if (!extractor) {
                error("Invalid GIN extractor ID: " + std::to_string(extractor_id));
                return;
            }

            // Note: xmin is not used in GIN insert API - GIN handles transaction tracking internally
            (void)xmin; // Suppress unused parameter warning
            core::Status gin_status = gin->insert(value.data(), value.size(), tid, extractor, &err_ctx);

            if (gin_status != core::Status::OK)
            {
                error("GIN insert failed: " + std::string(err_ctx.message));
            }
        }

        void Executor::executeGinSearch()
        {
            // GIN SEARCH Bytecode Format:
            // - Index UUID (16 bytes)
            // - Query length (2 bytes)
            // - Query data (variable)
            // - current_xid (8 bytes)
            // - Extractor ID (2 bytes)

            // Read index UUID (16 bytes)
            core::ID index_uuid;
            for (int i = 0; i < 16; i++)
            {
                if (pc_ >= bytecode_size_)
                {
                    error("Incomplete index UUID in EXT_GIN_SEARCH");
                    return;
                }
                index_uuid.bytes[i] = bytecode_[pc_++];
            }

            // Read query length (2 bytes, little-endian)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete query length in EXT_GIN_SEARCH");
                return;
            }
            uint16_t query_len = bytecode_[pc_++];
            query_len |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);

            // Read query data
            if (pc_ + query_len > bytecode_size_)
            {
                error("Incomplete query data in EXT_GIN_SEARCH");
                return;
            }
            std::vector<uint8_t> query(&bytecode_[0] + pc_, &bytecode_[0] + pc_ + query_len);
            pc_ += query_len;

            // Read current_xid (8 bytes, little-endian)
            if (pc_ + 7 >= bytecode_size_)
            {
                error("Incomplete current_xid in EXT_GIN_SEARCH");
                return;
            }
            uint64_t current_xid = 0;
            for (int i = 0; i < 8; i++)
            {
                current_xid |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Read extractor ID (2 bytes)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete extractor ID in EXT_GIN_SEARCH");
                return;
            }
            uint16_t extractor_id = bytecode_[pc_++];
            extractor_id |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);

            // Open GIN index and perform search
            core::ErrorContext err_ctx;
            core::CatalogManager::IndexInfo index_info;
            auto status_get = db_->catalog_manager()->getIndex(index_uuid, index_info, &err_ctx);
            if (status_get != core::Status::OK)
            {
                error("Index not found: " + std::string(err_ctx.message));
                return;
            }

            auto gin = core::GinIndex::open(db_, index_uuid, index_info.root_page, &err_ctx);
            if (!gin)
            {
                error("Failed to open GIN index");
                return;
            }

            std::vector<core::TID> results;
            // GIN uses find() for searching, not search()
            core::Status status = gin->find(query.data(), query.size(), current_xid, &results, &err_ctx);

            if (status != core::Status::OK)
            {
                error("GIN search failed: " + std::string(err_ctx.message));
                return;
            }

            // Push results count onto stack
            push(Value::makeInt64(static_cast<int64_t>(results.size())));
        }

        void Executor::executeHnswInsert()
        {
            // HNSW INSERT Bytecode Format:
            // - Index UUID (16 bytes)
            // - Vector dimension (2 bytes)
            // - Vector data (dimension * 4 bytes for float32)
            // - TID: GPID (8 bytes) + slot (2 bytes)
            // - xmin (8 bytes)

            // Read index UUID (16 bytes)
            core::ID index_uuid;
            for (int i = 0; i < 16; i++)
            {
                if (pc_ >= bytecode_size_)
                {
                    error("Incomplete index UUID in EXT_HNSW_INSERT");
                    return;
                }
                index_uuid.bytes[i] = bytecode_[pc_++];
            }

            // Read vector dimension (2 bytes, little-endian)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete vector dimension in EXT_HNSW_INSERT");
                return;
            }
            uint16_t dimension = bytecode_[pc_++];
            dimension |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);

            // Read vector data (dimension * 4 bytes for float32)
            uint32_t vector_bytes = dimension * 4;
            if (pc_ + vector_bytes > bytecode_size_)
            {
                error("Incomplete vector data in EXT_HNSW_INSERT");
                return;
            }
            std::vector<float> vector_data(dimension);
            for (uint16_t i = 0; i < dimension; i++)
            {
                uint32_t float_bits = 0;
                for (int j = 0; j < 4; j++)
                {
                    float_bits |= (static_cast<uint32_t>(bytecode_[pc_++]) << (j * 8));
                }
                // Reinterpret bits as float
                std::memcpy(&vector_data[i], &float_bits, sizeof(float));
            }

            // Read TID (10 bytes: GPID 8 + slot 2)
            if (pc_ + 10 > bytecode_size_)
            {
                error("Incomplete TID in EXT_HNSW_INSERT");
                return;
            }
            uint64_t gpid = 0;
            for (int i = 0; i < 8; i++)
            {
                gpid |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }
            uint16_t slot = bytecode_[pc_++];
            slot |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);
            core::TID tid(gpid, slot);

            // Read xmin (8 bytes, little-endian)
            if (pc_ + 7 >= bytecode_size_)
            {
                error("Incomplete xmin in EXT_HNSW_INSERT");
                return;
            }
            uint64_t xmin = 0;
            for (int i = 0; i < 8; i++)
            {
                xmin |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Create VectorValue and insert into HNSW index
            core::ErrorContext err_ctx;
            core::CatalogManager::IndexInfo index_info;
            auto status_idx = db_->catalog_manager()->getIndex(index_uuid, index_info, &err_ctx);
            if (status_idx != core::Status::OK)
            {
                error("Index not found: " + std::string(err_ctx.message));
                return;
            }

            auto hnsw = core::HnswIndex::open(db_, index_uuid, index_info.root_page, &err_ctx);
            if (!hnsw)
            {
                error("Failed to open HNSW index");
                return;
            }

            // Create VectorValue from float vector
            core::VectorValue vec(vector_data);

            // HNSW insert doesn't take xmin parameter - it manages versioning internally
            core::Status status = hnsw->insert(vec, tid, &err_ctx);

            if (status != core::Status::OK)
            {
                error("HNSW insert failed: " + std::string(err_ctx.message));
            }
        }

        void Executor::executeHnswSearch()
        {
            // HNSW SEARCH Bytecode Format:
            // - Index UUID (16 bytes)
            // - Vector dimension (2 bytes)
            // - Vector data (dimension * 4 bytes for float32)
            // - k (2 bytes) - number of nearest neighbors
            // - current_xid (8 bytes)

            // Read index UUID (16 bytes)
            core::ID index_uuid;
            for (int i = 0; i < 16; i++)
            {
                if (pc_ >= bytecode_size_)
                {
                    error("Incomplete index UUID in EXT_HNSW_SEARCH");
                    return;
                }
                index_uuid.bytes[i] = bytecode_[pc_++];
            }

            // Read vector dimension (2 bytes, little-endian)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete vector dimension in EXT_HNSW_SEARCH");
                return;
            }
            uint16_t dimension = bytecode_[pc_++];
            dimension |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);

            // Read vector data (dimension * 4 bytes for float32)
            uint32_t vector_bytes = dimension * 4;
            if (pc_ + vector_bytes > bytecode_size_)
            {
                error("Incomplete vector data in EXT_HNSW_SEARCH");
                return;
            }
            std::vector<float> vector_data(dimension);
            for (uint16_t i = 0; i < dimension; i++)
            {
                uint32_t float_bits = 0;
                for (int j = 0; j < 4; j++)
                {
                    float_bits |= (static_cast<uint32_t>(bytecode_[pc_++]) << (j * 8));
                }
                std::memcpy(&vector_data[i], &float_bits, sizeof(float));
            }

            // Read k (2 bytes, little-endian)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete k in EXT_HNSW_SEARCH");
                return;
            }
            uint16_t k = bytecode_[pc_++];
            k |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);

            // Read current_xid (8 bytes, little-endian)
            if (pc_ + 7 >= bytecode_size_)
            {
                error("Incomplete current_xid in EXT_HNSW_SEARCH");
                return;
            }
            uint64_t current_xid = 0;
            for (int i = 0; i < 8; i++)
            {
                current_xid |= (static_cast<uint64_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Open HNSW index and perform k-NN search
            core::ErrorContext err_ctx;
            core::CatalogManager::IndexInfo index_info;
            auto status_hnsw = db_->catalog_manager()->getIndex(index_uuid, index_info, &err_ctx);
            if (status_hnsw != core::Status::OK)
            {
                error("Index not found: " + std::string(err_ctx.message));
                return;
            }

            auto hnsw = core::HnswIndex::open(db_, index_uuid, index_info.root_page, &err_ctx);
            if (!hnsw)
            {
                error("Failed to open HNSW index");
                return;
            }

            // Create VectorValue for query from float vector
            core::VectorValue query_vec(vector_data);

            std::vector<core::HnswSearchResult> results;
            core::Status status = hnsw->search(query_vec, k, current_xid, &results, &err_ctx);

            if (status != core::Status::OK)
            {
                error("HNSW search failed: " + std::string(err_ctx.message));
                return;
            }

            // Push results count onto stack
            push(Value::makeInt64(static_cast<int64_t>(results.size())));
        }

        void Executor::executeColumnstoreInsert()
        {
            // COLUMNSTORE INSERT Bytecode Format:
            // - Index UUID (16 bytes)
            // - Column ID (2 bytes)
            // - Row count (4 bytes)
            // - Column data length (4 bytes)
            // - Column data (variable)

            // Read index UUID (16 bytes)
            core::ID index_uuid;
            for (int i = 0; i < 16; i++)
            {
                if (pc_ >= bytecode_size_)
                {
                    error("Incomplete index UUID in EXT_COLUMNSTORE_INSERT");
                    return;
                }
                index_uuid.bytes[i] = bytecode_[pc_++];
            }

            // Read column ID (2 bytes, little-endian)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete column ID in EXT_COLUMNSTORE_INSERT");
                return;
            }
            uint16_t column_id = bytecode_[pc_++];
            column_id |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);

            // Read row count (4 bytes, little-endian)
            if (pc_ + 3 >= bytecode_size_)
            {
                error("Incomplete row count in EXT_COLUMNSTORE_INSERT");
                return;
            }
            uint32_t row_count = 0;
            for (int i = 0; i < 4; i++)
            {
                row_count |= (static_cast<uint32_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Read data length (4 bytes, little-endian)
            if (pc_ + 3 >= bytecode_size_)
            {
                error("Incomplete data length in EXT_COLUMNSTORE_INSERT");
                return;
            }
            uint32_t data_len = 0;
            for (int i = 0; i < 4; i++)
            {
                data_len |= (static_cast<uint32_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Read column data
            if (pc_ + data_len > bytecode_size_)
            {
                error("Incomplete column data in EXT_COLUMNSTORE_INSERT");
                return;
            }
            std::vector<uint8_t> column_data(bytecode_ + pc_, bytecode_ + pc_ + data_len);
            pc_ += data_len;

            // Open columnstore index and insert
            core::ErrorContext err_ctx;
            core::CatalogManager::IndexInfo index_info;
            auto status = db_->catalog_manager()->getIndex(index_uuid, index_info, &err_ctx);
            if (status != core::Status::OK)
            {
                error("Index not found: " + std::string(err_ctx.message));
                return;
            }

            auto columnstore = core::ColumnstoreIndex::open(db_, index_uuid, index_info.root_page, &err_ctx);
            if (!columnstore)
            {
                error("Failed to open Columnstore index");
                return;
            }

            core::Status insert_status = columnstore->insertColumn(column_id, row_count, column_data, &err_ctx);

            if (insert_status != core::Status::OK)
            {
                error("Columnstore insert failed: " + std::string(err_ctx.message));
            }
        }

        void Executor::executeColumnstoreScan()
        {
            // COLUMNSTORE SCAN Bytecode Format:
            // - Index UUID (16 bytes)
            // - Column ID (2 bytes)
            // - Start row (4 bytes)
            // - End row (4 bytes)

            // Read index UUID (16 bytes)
            core::ID index_uuid;
            for (int i = 0; i < 16; i++)
            {
                if (pc_ >= bytecode_size_)
                {
                    error("Incomplete index UUID in EXT_COLUMNSTORE_SCAN");
                    return;
                }
                index_uuid.bytes[i] = bytecode_[pc_++];
            }

            // Read column ID (2 bytes, little-endian)
            if (pc_ + 1 >= bytecode_size_)
            {
                error("Incomplete column ID in EXT_COLUMNSTORE_SCAN");
                return;
            }
            uint16_t column_id = bytecode_[pc_++];
            column_id |= (static_cast<uint16_t>(bytecode_[pc_++]) << 8);

            // Read start row (4 bytes, little-endian)
            if (pc_ + 3 >= bytecode_size_)
            {
                error("Incomplete start row in EXT_COLUMNSTORE_SCAN");
                return;
            }
            uint32_t start_row = 0;
            for (int i = 0; i < 4; i++)
            {
                start_row |= (static_cast<uint32_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Read end row (4 bytes, little-endian)
            if (pc_ + 3 >= bytecode_size_)
            {
                error("Incomplete end row in EXT_COLUMNSTORE_SCAN");
                return;
            }
            uint32_t end_row = 0;
            for (int i = 0; i < 4; i++)
            {
                end_row |= (static_cast<uint32_t>(bytecode_[pc_++]) << (i * 8));
            }

            // Open columnstore index and scan
            core::ErrorContext err_ctx;
            core::CatalogManager::IndexInfo index_info;
            auto status = db_->catalog_manager()->getIndex(index_uuid, index_info, &err_ctx);
            if (status != core::Status::OK)
            {
                error("Index not found: " + std::string(err_ctx.message));
                return;
            }

            auto columnstore = core::ColumnstoreIndex::open(db_, index_uuid, index_info.root_page, &err_ctx);
            if (!columnstore)
            {
                error("Failed to open Columnstore index");
                return;
            }

            std::vector<uint8_t> result_data;
            core::Status scan_status = columnstore->scanColumn(column_id, start_row, end_row, &result_data, &err_ctx);

            if (scan_status != core::Status::OK)
            {
                error("Columnstore scan failed: " + std::string(err_ctx.message));
                return;
            }

            // Push result data length onto stack
            push(Value::makeInt64(static_cast<int64_t>(result_data.size())));
        }

        // Index routing helpers - route to specific index implementations
        core::Status Executor::routeIndexInsert(IndexType type, const core::ID& index_uuid,
                                              const std::vector<uint8_t>& key,
                                              const core::TID& tid, uint64_t xmin,
                                              core::ErrorContext* ctx)
        {
            // Get index info from catalog
            core::CatalogManager::IndexInfo index_info;
            auto status = db_->catalog_manager()->getIndex(index_uuid, index_info, ctx);
            if (status != core::Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, core::Status::INDEX_NOT_FOUND, "Index not found");
                return core::Status::INDEX_NOT_FOUND;
            }

            // Route to appropriate index type (with caching - November 19, 2025)
            switch (type)
            {
                case IndexType::BTREE:
                {
                    auto btree = getOrOpenIndex<core::BTree>(index_uuid, type, index_info.root_page, ctx);
                    if (btree)
                    {
                        return btree->insert(key, tid, xmin, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::HASH:
                {
                    auto hash_idx = getOrOpenIndex<core::HashIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (hash_idx)
                    {
                        // HashIndex::insert() expects (void*, size_t, TID, uint64_t, ErrorContext*)
                        return hash_idx->insert(key.data(), key.size(), tid, xmin, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::RTREE:
                {
                    auto rtree = getOrOpenIndex<core::RTreeIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (rtree)
                    {
                        return rtree->insert(key, tid, xmin, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::GIST:
                {
                    // GiST index has incomplete type issues - not fully integrated yet
                    // TODO: Complete GiST index integration
                    SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                      "GiST index operations not yet fully supported");
                    return core::Status::NOT_SUPPORTED;
                }

                case IndexType::SPGIST:
                {
                    auto spgist = getOrOpenIndex<core::SPGiSTIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (spgist)
                    {
                        return spgist->insert(key, tid, xmin, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::BRIN:
                {
                    auto brin = getOrOpenIndex<core::BrinIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (brin)
                    {
                        // BRIN indexes work on block ranges, not individual tuples
                        // Convert TID to block number
                        uint32_t block_num = static_cast<uint32_t>(core::getPageNumber(tid.gpid));
                        return brin->insert(key, block_num, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::BITMAP:
                {
                    auto bitmap = getOrOpenIndex<core::BitmapIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (bitmap)
                    {
                        // BitmapIndex::insert() expects (void*, size_t, TID, ErrorContext*)
                        return bitmap->insert(key.data(), key.size(), tid, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::LSM:
                {
                    auto lsm = getOrOpenIndex<core::LSMTreeIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (lsm)
                    {
                        // Serialize TID to byte vector (page_num + slot_num)
                        std::vector<uint8_t> value(sizeof(uint32_t) * 2);
                        uint32_t page = static_cast<uint32_t>(core::getPageNumber(tid.gpid));
                        uint32_t slot = tid.slot;
                        std::memcpy(value.data(), &page, sizeof(uint32_t));
                        std::memcpy(value.data() + sizeof(uint32_t), &slot, sizeof(uint32_t));

                        return lsm->put(key, value, xmin, ctx);  // LSM uses put(), not insert()
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::GIN:
                {
                    auto gin = getOrOpenIndex<core::GinIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (gin)
                    {
                        // GIN requires a key_extractor function to decompose values into searchable keys
                        // Get the default extractor from the registry
                        auto key_extractor = GinExtractorRegistry::instance().getExtractor(
                            static_cast<uint16_t>(GinExtractorId::DEFAULT));

                        if (!key_extractor)
                        {
                            // Fallback to default extractor
                            key_extractor = GinExtractorRegistry::defaultExtractor;
                        }

                        return gin->insert(key.data(), key.size(), tid, key_extractor, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::HNSW:
                {
                    auto hnsw = getOrOpenIndex<core::HnswIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (hnsw)
                    {
                        // Decode vector from key bytes
                        auto vector_opt = core::Vector::decode(key);
                        if (!vector_opt.has_value())
                        {
                            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                                                  "Failed to decode vector from key");
                            return core::Status::INVALID_ARGUMENT;
                        }
                        // HNSW insert doesn't take xmin parameter - it manages versioning internally
                        return hnsw->insert(vector_opt.value(), tid, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::COLUMNSTORE:
                    // Columnstore requires bulk load operations (not row-level insert/delete)
                    SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                          "Columnstore requires bulk load operations (use LOAD_COLUMNS opcode)");
                    return core::Status::NOT_SUPPORTED;

                default:
                    SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Unknown index type");
                    return core::Status::INTERNAL_ERROR;
            }
        }

        core::Status Executor::routeIndexSearch(IndexType type, const core::ID& index_uuid,
                                              const std::vector<uint8_t>& key,
                                              uint64_t current_xid,
                                              std::vector<core::TID>* results_out,
                                              core::ErrorContext* ctx)
        {
            // Get index info from catalog
            core::CatalogManager::IndexInfo index_info;
            auto status = db_->catalog_manager()->getIndex(index_uuid, index_info, ctx);
            if (status != core::Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, core::Status::INDEX_NOT_FOUND, "Index not found");
                return core::Status::INDEX_NOT_FOUND;
            }

            // Route to appropriate index type
            switch (type)
            {
                case IndexType::BTREE:
                {
                    auto btree = getOrOpenIndex<core::BTree>(index_uuid, type, index_info.root_page, ctx);
                    if (btree)
                    {
                        return btree->search(key, current_xid, results_out, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::HASH:
                {
                    auto hash_idx = getOrOpenIndex<core::HashIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (hash_idx)
                    {
                        // HashIndex uses find() for point lookups, not search()
                        return hash_idx->find(key.data(), key.size(), current_xid, results_out, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::RTREE:
                {
                    auto rtree = getOrOpenIndex<core::RTreeIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (rtree)
                    {
                        return rtree->search(key, current_xid, results_out, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::GIST:
                {
                    // GiST index has incomplete type issues - not fully integrated yet
                    // TODO: Complete GiST index integration
                    SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                      "GiST index operations not yet fully supported");
                    return core::Status::NOT_SUPPORTED;
                }

                case IndexType::SPGIST:
                {
                    auto spgist = getOrOpenIndex<core::SPGiSTIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (spgist)
                    {
                        return spgist->search(key, current_xid, results_out, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::BRIN:
                {
                    auto brin = getOrOpenIndex<core::BrinIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (brin)
                    {
                        // BRIN doesn't have search() - it uses scan() for range queries
                        // For a point lookup, treat the key as both min and max value
                        std::vector<uint32_t> block_numbers;
                        core::Status status = brin->scan(&key, &key, current_xid, &block_numbers, ctx);

                        if (status != core::Status::OK)
                        {
                            return status;
                        }

                        // BRIN returns block numbers, not TIDs
                        // This is a design limitation - the caller needs to scan the returned blocks
                        // For now, return NOT_SUPPORTED since we can't return block numbers via TID vector
                        SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                         "BRIN point lookup requires block-level scan (use range scan instead)");
                        return core::Status::NOT_SUPPORTED;
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::BITMAP:
                {
                    auto bitmap = getOrOpenIndex<core::BitmapIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (bitmap)
                    {
                        // BitmapIndex::scan() expects (void*, size_t, uint64_t, ErrorContext*)
                        auto scanner = bitmap->scan(key.data(), key.size(), current_xid, ctx);
                        if (!scanner)
                        {
                            return core::Status::INTERNAL_ERROR;
                        }

                        // Iterate over scanner and collect matching TIDs
                        results_out->clear();
                        while (scanner->hasNext())
                        {
                            core::TID tid;
                            core::Status status = scanner->next(&tid, ctx);
                            if (status != core::Status::OK)
                            {
                                return status;
                            }
                            results_out->push_back(tid);
                        }
                        return core::Status::OK;
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::LSM:
                {
                    auto lsm = getOrOpenIndex<core::LSMTreeIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (lsm)
                    {
                        std::vector<uint8_t> value;
                        bool found = false;
                        core::Status status = lsm->get(key, current_xid, &value, &found, ctx);
                        if (status == core::Status::OK && found && value.size() >= sizeof(uint32_t) * 2)
                        {
                            // Deserialize TID from byte vector
                            uint32_t page, slot;
                            std::memcpy(&page, value.data(), sizeof(uint32_t));
                            std::memcpy(&slot, value.data() + sizeof(uint32_t), sizeof(uint32_t));
                            core::TID result_tid(page, slot);
                            results_out->push_back(result_tid);
                        }
                        return status;
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::GIN:
                    // GIN (Generalized Inverted Index) requires specialized query operators
                    // Generic key search doesn't apply to inverted indexes
                    SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                          "GIN requires specialized query operators (e.g., @>, @@)");
                    return core::Status::NOT_SUPPORTED;

                case IndexType::HNSW:
                    // HNSW uses k-NN search, not exact key search
                    SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                          "HNSW requires k-NN search operator (<-> with LIMIT)");
                    return core::Status::NOT_SUPPORTED;

                case IndexType::COLUMNSTORE:
                    // Columnstore uses specialized scan operations
                    SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                          "Columnstore requires specialized scan operations");
                    return core::Status::NOT_SUPPORTED;

                default:
                    SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Unknown index type");
                    return core::Status::INTERNAL_ERROR;
            }
        }

        core::Status Executor::routeIndexDelete(IndexType type, const core::ID& index_uuid,
                                              const std::vector<uint8_t>& key,
                                              const core::TID& tid, uint64_t xmax,
                                              core::ErrorContext* ctx)
        {
            // Get index info from catalog
            core::CatalogManager::IndexInfo index_info;
            auto status = db_->catalog_manager()->getIndex(index_uuid, index_info, ctx);
            if (status != core::Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, core::Status::INDEX_NOT_FOUND, "Index not found");
                return core::Status::INDEX_NOT_FOUND;
            }

            // Route to appropriate index type
            // Use remove() or markDeleted() depending on what's available
            switch (type)
            {
                case IndexType::BTREE:
                {
                    auto btree = getOrOpenIndex<core::BTree>(index_uuid, type, index_info.root_page, ctx);
                    if (btree)
                    {
                        // B-Tree has markDeleted() for MGA-compliant soft deletion
                        return btree->markDeleted(key, tid, xmax, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::HASH:
                {
                    auto hash_idx = getOrOpenIndex<core::HashIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (hash_idx)
                    {
                        // HashIndex::remove() expects (void*, size_t, TID, uint64_t, ErrorContext*)
                        return hash_idx->remove(key.data(), key.size(), tid, xmax, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::RTREE:
                {
                    auto rtree = getOrOpenIndex<core::RTreeIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (rtree)
                    {
                        return rtree->remove(key, tid, xmax, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::GIST:
                {
                    // GiST index has incomplete type issues - not fully integrated yet
                    // TODO: Complete GiST index integration
                    SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                      "GiST index operations not yet fully supported");
                    return core::Status::NOT_SUPPORTED;
                }

                case IndexType::SPGIST:
                {
                    auto spgist = getOrOpenIndex<core::SPGiSTIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (spgist)
                    {
                        return spgist->remove(key, tid, xmax, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::BRIN:
                {
                    auto brin = getOrOpenIndex<core::BrinIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (brin)
                    {
                        // BRIN remove is a no-op (returns OK)
                        return brin->remove(key, 0, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::BITMAP:
                {
                    auto bitmap = getOrOpenIndex<core::BitmapIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (bitmap)
                    {
                        // BitmapIndex::remove() only takes TID, not key or xmax
                        // The xmax is handled internally by the index
                        return bitmap->remove(tid, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::LSM:
                {
                    auto lsm = getOrOpenIndex<core::LSMTreeIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (lsm)
                    {
                        // LSMTreeIndex remove only needs key and transaction ID
                        return lsm->remove(key, xmax, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::GIN:
                {
                    auto gin = getOrOpenIndex<core::GinIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (gin)
                    {
                        // GIN requires a key_extractor function to decompose values into searchable keys
                        // Get the default extractor from the registry
                        auto key_extractor = GinExtractorRegistry::instance().getExtractor(
                            static_cast<uint16_t>(GinExtractorId::DEFAULT));

                        if (!key_extractor)
                        {
                            // Fallback to default extractor
                            key_extractor = GinExtractorRegistry::defaultExtractor;
                        }

                        return gin->remove(key.data(), key.size(), tid, key_extractor, xmax, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::HNSW:
                {
                    auto hnsw = getOrOpenIndex<core::HnswIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (hnsw)
                    {
                        // HNSW remove() only takes TID (doesn't need key or xmax)
                        // It manages xmax internally
                        return hnsw->remove(tid, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::COLUMNSTORE:
                    // Columnstore requires bulk operations
                    SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                          "Columnstore requires bulk load operations");
                    return core::Status::NOT_SUPPORTED;

                default:
                    SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Unknown index type");
                    return core::Status::INTERNAL_ERROR;
            }
        }

        core::Status Executor::routeIndexScan(IndexType type, const core::ID& index_uuid,
                                             const std::vector<uint8_t>* start_key,
                                             const std::vector<uint8_t>* end_key,
                                             bool start_inclusive, bool end_inclusive,
                                             uint64_t current_xid,
                                             std::vector<core::TID>* results_out,
                                             core::ErrorContext* ctx)
        {
            // Get index info from catalog
            core::CatalogManager::IndexInfo index_info;
            auto status = db_->catalog_manager()->getIndex(index_uuid, index_info, ctx);
            if (status != core::Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, core::Status::INDEX_NOT_FOUND, "Index not found");
                return core::Status::INDEX_NOT_FOUND;
            }

            // Route to appropriate index type
            // Only indexes that support range scans are routed
            switch (type)
            {
                case IndexType::BTREE:
                {
                    auto btree = getOrOpenIndex<core::BTree>(index_uuid, type, index_info.root_page, ctx);
                    if (btree)
                    {
                        // Use B-Tree rangeScan() to get an iterator
                        auto iter = btree->rangeScan(start_key, end_key, current_xid,
                                                    start_inclusive, end_inclusive, ctx);
                        if (!iter)
                        {
                            if (ctx) {
                                ctx->set(core::Status::INTERNAL_ERROR, "Failed to create B-Tree iterator",
                                        __FILE__, __LINE__, __func__);
                            }
                            return core::Status::INTERNAL_ERROR;
                        }

                        // Collect results from iterator
                        results_out->clear();
                        while (iter->hasNext())
                        {
                            std::vector<uint8_t> key_result;
                            core::TID tid_result;
                            core::Status status = iter->next(&key_result, &tid_result, ctx);
                            if (status == core::Status::OK)
                            {
                                results_out->push_back(tid_result);
                            }
                            else
                            {
                                break;
                            }
                        }
                        return core::Status::OK;
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::RTREE:
                {
                    auto rtree = getOrOpenIndex<core::RTreeIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (rtree)
                    {
                        // R-Tree spatial scan
                        // For R-Tree, start_key and end_key represent the bounding box
                        if (!start_key || !end_key)
                        {
                            if (ctx) {
                                ctx->set(core::Status::INVALID_ARGUMENT, "R-Tree scan requires bounding box (start and end keys)",
                                        __FILE__, __LINE__, __func__);
                            }
                            return core::Status::INVALID_ARGUMENT;
                        }

                        // Use search() with bounding box
                        return rtree->search(*start_key, current_xid, results_out, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::GIST:
                {
                    // GiST index has incomplete type issues - not fully integrated yet
                    // TODO: Complete GiST index integration
                    SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                      "GiST index operations not yet fully supported");
                    return core::Status::NOT_SUPPORTED;
                }

                case IndexType::SPGIST:
                {
                    auto spgist = getOrOpenIndex<core::SPGiSTIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (spgist)
                    {
                        // SP-GiST scan
                        if (!start_key)
                        {
                            if (ctx) {
                                ctx->set(core::Status::INVALID_ARGUMENT, "SP-GiST scan requires search predicate",
                                        __FILE__, __LINE__, __func__);
                            }
                            return core::Status::INVALID_ARGUMENT;
                        }

                        return spgist->search(*start_key, current_xid, results_out, ctx);
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::BRIN:
                {
                    auto brin = getOrOpenIndex<core::BrinIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (brin)
                    {
                        // BRIN block range scan
                        if (!start_key || !end_key)
                        {
                            if (ctx) {
                                ctx->set(core::Status::INVALID_ARGUMENT, "BRIN scan requires value range",
                                        __FILE__, __LINE__, __func__);
                            }
                            return core::Status::INVALID_ARGUMENT;
                        }

                        // BRIN returns block numbers that might contain values in range
                        std::vector<uint32_t> block_numbers;
                        core::Status status = brin->scan(start_key, end_key, current_xid, &block_numbers, ctx);

                        if (status != core::Status::OK)
                        {
                            return status;
                        }

                        // BRIN returns block numbers, not TIDs
                        // This is a design limitation - the caller needs to scan the returned blocks
                        // For now, return NOT_SUPPORTED since we can't return block numbers via TID vector
                        SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                         "BRIN range scan requires block-level scan (use heap scan on returned blocks)");
                        return core::Status::NOT_SUPPORTED;
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::BITMAP:
                {
                    auto bitmap = getOrOpenIndex<core::BitmapIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (bitmap)
                    {
                        // Bitmap scan for matching bits
                        if (!start_key)
                        {
                            if (ctx) {
                                ctx->set(core::Status::INVALID_ARGUMENT, "Bitmap scan requires value",
                                        __FILE__, __LINE__, __func__);
                            }
                            return core::Status::INVALID_ARGUMENT;
                        }

                        // Create scanner with proper parameters (data pointer, size, xid, ctx)
                        auto scanner = bitmap->scan(start_key->data(), start_key->size(), current_xid, ctx);
                        if (!scanner)
                        {
                            return core::Status::INTERNAL_ERROR;
                        }

                        // Iterate over scanner and collect matching TIDs
                        while (scanner->hasNext())
                        {
                            core::TID tid;
                            core::Status status = scanner->next(&tid, ctx);
                            if (status != core::Status::OK)
                            {
                                return status;
                            }
                            results_out->push_back(tid);
                        }

                        return core::Status::OK;
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::LSM:
                {
                    auto lsm = getOrOpenIndex<core::LSMTreeIndex>(index_uuid, type, index_info.root_page, ctx);
                    if (lsm)
                    {
                        // LSM-Tree range scan
                        std::vector<core::MemtableEntry> entries;
                        std::vector<uint8_t> start_key_vec = start_key ? *start_key : std::vector<uint8_t>();
                        std::vector<uint8_t> end_key_vec = end_key ? *end_key : std::vector<uint8_t>();

                        core::Status status = lsm->scan(start_key_vec, end_key_vec, current_xid, &entries, ctx);
                        if (status != core::Status::OK)
                        {
                            return status;
                        }

                        // Collect results - deserialize TIDs from entry values
                        results_out->clear();
                        for (const auto &entry : entries)
                        {
                            if (entry.value.size() >= sizeof(uint32_t) * 2)
                            {
                                uint32_t page, slot;
                                std::memcpy(&page, entry.value.data(), sizeof(uint32_t));
                                std::memcpy(&slot, entry.value.data() + sizeof(uint32_t), sizeof(uint32_t));
                                results_out->push_back(core::TID(page, slot));
                            }
                        }
                        return core::Status::OK;
                    }
                    return core::Status::INTERNAL_ERROR;
                }

                case IndexType::HASH:
                {
                    // Hash indexes don't support range scans
                    if (ctx) {
                        ctx->set(core::Status::NOT_SUPPORTED, "Hash indexes do not support range scans",
                                __FILE__, __LINE__, __func__);
                    }
                    return core::Status::NOT_SUPPORTED;
                }

                case IndexType::GIN:
                {
                    // GIN (Generalized Inverted Index) doesn't support range scans
                    if (ctx) {
                        ctx->set(core::Status::NOT_SUPPORTED, "GIN indexes do not support range scans",
                                __FILE__, __LINE__, __func__);
                    }
                    return core::Status::NOT_SUPPORTED;
                }

                case IndexType::HNSW:
                {
                    // HNSW uses k-NN search, not range scans
                    if (ctx) {
                        ctx->set(core::Status::NOT_SUPPORTED, "HNSW indexes do not support range scans (use k-NN search)",
                                __FILE__, __LINE__, __func__);
                    }
                    return core::Status::NOT_SUPPORTED;
                }

                case IndexType::COLUMNSTORE:
                {
                    // Columnstore uses specialized scan operations
                    if (ctx) {
                        ctx->set(core::Status::NOT_SUPPORTED, "Columnstore uses specialized scan operations",
                                __FILE__, __LINE__, __func__);
                    }
                    return core::Status::NOT_SUPPORTED;
                }

                default:
                {
                    if (ctx) {
                        ctx->set(core::Status::INTERNAL_ERROR, "Unknown index type",
                                __FILE__, __LINE__, __func__);
                    }
                    return core::Status::INTERNAL_ERROR;
                }
            }
        }

    } // namespace sblr
} // namespace scratchbird