#pragma once

/**
 * ScratchBird Parser v2.0 - Resolved AST Node Definitions
 *
 * This module defines the "resolved" AST nodes produced by the semantic analyzer.
 * Unlike the unresolved AST (which uses StringPool IDs and SchemaPath references),
 * the resolved AST uses UUIDs that directly reference catalog entries.
 *
 * Key differences from Unresolved AST:
 * - Table/View references contain UUIDs instead of SchemaPath
 * - Column references contain table UUID + column index
 * - All identifiers are resolved to their catalog entries
 * - Type information is attached to all expressions
 *
 * See: docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md Section 8.5
 */

#include "scratchbird/parser/ast_v2.h"
#include "scratchbird/core/types.h"  // For ID (UUID), DataType
#include <vector>
#include <memory>
#include <optional>

namespace scratchbird::parser::v2 {

// Import types from core namespace
using core::ID;
using core::DataType;

// Forward declarations
class ResolvedStatement;
class ResolvedExpression;

// =============================================================================
// Resolved Reference Types
// =============================================================================

/**
 * Resolved table reference - points to actual catalog entry
 */
struct ResolvedTableRef {
    ID table_uuid;                      // UUID of table/view in catalog
    ID schema_uuid;                     // Schema containing the object
    StringPool::StringId name;          // Original table name (for v1 bytecode compat)
    StringPool::StringId alias;         // Runtime alias (if any)
    bool has_alias = false;

    enum class ObjectType : uint8_t {
        TABLE,
        VIEW,
        MATERIALIZED_VIEW,
        CTE,                            // Common Table Expression
        SUBQUERY,
        FUNCTION,                       // Table-valued function
    };
    ObjectType object_type = ObjectType::TABLE;

    // For CTEs and subqueries
    ResolvedStatement* subquery = nullptr;

    // Column information (populated during resolution)
    struct ColumnInfo {
        StringPool::StringId name;
        DataType data_type;
        bool is_nullable;
        uint32_t column_index;
    };
    std::vector<ColumnInfo> columns;
};

/**
 * Resolved column reference - points to specific table and column
 */
struct ResolvedColumnRef {
    ID table_uuid;                      // Table containing the column
    uint32_t column_index;              // Zero-based column index
    DataType data_type;                 // Resolved data type
    bool is_nullable;                   // Nullability
    StringPool::StringId column_name;   // Original column name (for display)
    StringPool::StringId table_alias;   // Table alias used in query (if any)
};

/**
 * Resolved function reference
 */
// Forward declaration for return type
struct ResolvedType;

struct ResolvedFunctionRef {
    ID function_uuid;                   // UUID of function in catalog (zero for built-in)
    StringPool::StringId function_name; // Function name
    bool is_builtin = true;             // True for system functions
    bool is_aggregate = false;          // True for aggregate functions
    bool is_window = false;             // True for window functions
    ResolvedType* return_type = nullptr; // Return type (owned by arena)
};

/**
 * Resolved schema reference
 */
struct ResolvedSchemaRef {
    ID schema_uuid;
    StringPool::StringId schema_name;
};

/**
 * Resolved sequence reference
 */
struct ResolvedSequenceRef {
    ID sequence_uuid;
    ID schema_uuid;
    StringPool::StringId sequence_name;
};

// =============================================================================
// Type Information
// =============================================================================

/**
 * Resolved type information attached to expressions
 */
struct ResolvedType {
    DataType data_type;
    bool is_nullable = true;

    // For numeric types
    std::optional<int32_t> precision;
    std::optional<int32_t> scale;

    // For string/binary types
    std::optional<int32_t> length;

    // For array types
    bool is_array = false;
    std::optional<int32_t> array_size;

    // Type modifiers
    bool with_time_zone = false;

    // Comparison helpers
    bool isNumeric() const;
    bool isString() const;
    bool isBoolean() const;
    bool isTemporal() const;
    bool isComparableTo(const ResolvedType& other) const;
    bool isAssignableTo(const ResolvedType& target) const;
};

// =============================================================================
// Resolved Expression Base
// =============================================================================

/**
 * Base class for resolved expressions
 */
class ResolvedExpression {
public:
    virtual ~ResolvedExpression() = default;

    // All resolved expressions have type information
    ResolvedType type;
    SourceSpan span;

protected:
    ResolvedExpression() = default;
};

/**
 * Resolved literal expression
 */
struct ResolvedLiteral : public ResolvedExpression {
    LiteralType literal_type = LiteralType::NULL_VALUE;

    union {
        int64_t int_value;
        double float_value;
        bool bool_value;
    };
    StringPool::StringId string_value = StringPool::INVALID_ID;  // For STRING/BLOB

    bool is_null = false;
    bool is_default = false;
};

/**
 * Resolved column reference expression
 */
struct ResolvedColumnRefExpr : public ResolvedExpression {
    ResolvedColumnRef column;
};

/**
 * Resolved binary expression
 */
struct ResolvedBinaryExpr : public ResolvedExpression {
    BinaryOp op;
    ResolvedExpression* left = nullptr;
    ResolvedExpression* right = nullptr;
};

/**
 * Resolved unary expression
 */
struct ResolvedUnaryExpr : public ResolvedExpression {
    UnaryOp op;
    ResolvedExpression* operand = nullptr;
};

// Forward declarations for resolved types
struct ResolvedWindowSpec;
struct ResolvedOrderByItem;

/**
 * Resolved function call
 *
 * Supports regular functions, aggregate functions, and window functions.
 */
struct ResolvedFunctionCall : public ResolvedExpression {
    ResolvedFunctionRef function;
    std::vector<ResolvedExpression*> arguments;

    // Aggregate options
    bool distinct = false;
    ResolvedExpression* filter = nullptr;

    // STRING_AGG specific options
    ResolvedExpression* separator = nullptr;  // For STRING_AGG(expr, separator)
    std::vector<ResolvedOrderByItem*> internal_order_by;  // For STRING_AGG ... ORDER BY

    // Window function options
    bool is_window = false;
    ResolvedWindowSpec* window = nullptr;  // Window specification (OVER clause)
};

/**
 * Resolved CAST expression
 */
struct ResolvedCast : public ResolvedExpression {
    ResolvedExpression* expr = nullptr;
    ResolvedType target_type;
    bool implicit = false;  // True if inserted by semantic analyzer
};

/**
 * Resolved CASE expression
 */
struct ResolvedCase : public ResolvedExpression {
    ResolvedExpression* operand = nullptr;  // For simple CASE

    struct WhenClause {
        ResolvedExpression* when_expr;
        ResolvedExpression* then_expr;
    };
    std::vector<WhenClause> when_clauses;

    ResolvedExpression* else_expr = nullptr;
};

/**
 * Resolved subquery expression
 */
struct ResolvedSubqueryExpr : public ResolvedExpression {
    ResolvedStatement* subquery = nullptr;
    bool is_scalar = true;  // True for scalar subquery, false for row/set
};

/**
 * Resolved EXISTS expression
 */
struct ResolvedExistsExpr : public ResolvedExpression {
    bool negated = false;
    ResolvedStatement* subquery = nullptr;
};

/**
 * Resolved IN expression
 */
struct ResolvedInExpr : public ResolvedExpression {
    ResolvedExpression* expr = nullptr;
    bool negated = false;

    // Either values or subquery
    std::vector<ResolvedExpression*> values;
    ResolvedStatement* subquery = nullptr;
    bool has_subquery = false;
};

/**
 * Resolved BETWEEN expression
 */
struct ResolvedBetweenExpr : public ResolvedExpression {
    ResolvedExpression* expr = nullptr;
    bool negated = false;
    bool symmetric = false;
    ResolvedExpression* low = nullptr;
    ResolvedExpression* high = nullptr;
};

/**
 * Resolved LIKE expression
 */
struct ResolvedLikeExpr : public ResolvedExpression {
    ResolvedExpression* expr = nullptr;
    bool negated = false;
    bool case_insensitive = false;
    ResolvedExpression* pattern = nullptr;
    ResolvedExpression* escape = nullptr;
};

/**
 * Resolved IS NULL expression
 */
struct ResolvedIsNullExpr : public ResolvedExpression {
    ResolvedExpression* expr = nullptr;
    bool negated = false;
};

/**
 * Resolved array expression
 */
struct ResolvedArrayExpr : public ResolvedExpression {
    std::vector<ResolvedExpression*> elements;
    ResolvedStatement* subquery = nullptr;
    bool has_subquery = false;
};

// =============================================================================
// Resolved Statement Base
// =============================================================================

/**
 * Base class for resolved statements
 */
class ResolvedStatement {
public:
    virtual ~ResolvedStatement() = default;
    SourceSpan span;

protected:
    ResolvedStatement() = default;
};

// =============================================================================
// Resolved DML Statements
// =============================================================================

/**
 * Resolved SELECT item
 */
struct ResolvedSelectItem {
    enum class ItemType {
        EXPRESSION,
        STAR,           // SELECT *
        TABLE_STAR,     // SELECT t.*
    };

    ItemType item_type = ItemType::EXPRESSION;
    ResolvedExpression* expr = nullptr;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;

    // For TABLE_STAR
    ID table_uuid;  // Table for t.*

    // Result type (for subquery type checking)
    ResolvedType type;
};

/**
 * Resolved JOIN
 */
struct ResolvedJoin {
    JoinType join_type = JoinType::INNER;
    ResolvedTableRef* left = nullptr;
    ResolvedTableRef* right = nullptr;
    ResolvedExpression* on_condition = nullptr;
    std::vector<ResolvedColumnRef> using_columns;  // Resolved USING columns
    bool has_using = false;
};

/**
 * Resolved ORDER BY item
 */
struct ResolvedOrderByItem {
    ResolvedExpression* expr = nullptr;
    bool ascending = true;
    bool nulls_first = false;
    bool nulls_last = false;
};

/**
 * Resolved frame boundary (for window functions)
 */
struct ResolvedFrameBoundary {
    FrameBoundaryType type = FrameBoundaryType::UNBOUNDED_PRECEDING;
    ResolvedExpression* offset = nullptr;  // For PRECEDING/FOLLOWING with offset
};

/**
 * Resolved window specification (OVER clause)
 *
 * Used by window functions for:
 * - PARTITION BY: divide rows into groups
 * - ORDER BY: define order within each partition
 * - Frame clause: define which rows to include in the window frame
 */
struct ResolvedWindowSpec {
    // PARTITION BY expressions
    std::vector<ResolvedExpression*> partition_by;

    // ORDER BY items with direction
    std::vector<ResolvedOrderByItem*> order_by;

    // Frame clause
    bool has_frame = false;
    FrameMode frame_mode = FrameMode::RANGE;
    ResolvedFrameBoundary frame_start;
    ResolvedFrameBoundary frame_end;
    FrameExclusion frame_exclusion = FrameExclusion::NO_OTHERS;

    // Named window reference (for WINDOW clause)
    StringPool::StringId window_name = StringPool::INVALID_ID;
};

/**
 * Resolved SELECT statement
 */
struct ResolvedSelectStmt : public ResolvedStatement {
    bool distinct = false;
    bool all = false;

    // Select list with resolved types
    std::vector<ResolvedSelectItem> select_list;

    // FROM clause with resolved table references
    std::vector<ResolvedTableRef*> from_tables;
    std::vector<ResolvedJoin*> joins;

    // WHERE clause
    ResolvedExpression* where = nullptr;

    // GROUP BY clause (resolved expressions)
    std::vector<ResolvedExpression*> group_by;

    // HAVING clause
    ResolvedExpression* having = nullptr;

    // ORDER BY clause
    std::vector<ResolvedOrderByItem*> order_by;

    // LIMIT/OFFSET
    ResolvedExpression* limit = nullptr;
    ResolvedExpression* offset = nullptr;

    // Set operations
    SetOpType set_op = SetOpType::NONE;
    bool set_op_all = false;
    ResolvedSelectStmt* set_op_right = nullptr;

    // Locking
    bool for_update = false;
    bool for_share = false;

    // Result column information
    std::vector<ResolvedType> result_types;
    std::vector<StringPool::StringId> result_names;
};

/**
 * Resolved INSERT statement
 */
struct ResolvedInsertStmt : public ResolvedStatement {
    ResolvedTableRef target_table;

    // Resolved target columns (indexes into table)
    std::vector<uint32_t> target_column_indexes;

    // Source
    enum class Source { VALUES, SELECT, DEFAULT };
    Source source = Source::VALUES;

    // For VALUES source (expressions with resolved types)
    std::vector<std::vector<ResolvedExpression*>> values_rows;

    // For SELECT source
    ResolvedSelectStmt* select_source = nullptr;

    // ON CONFLICT
    struct OnConflict {
        std::vector<uint32_t> conflict_columns;  // Column indexes
        ID constraint_uuid;                       // Or constraint
        ConflictAction action = ConflictAction::NOTHING;
        std::vector<std::pair<uint32_t, ResolvedExpression*>> update_assignments;
        ResolvedExpression* where = nullptr;
    };
    std::unique_ptr<OnConflict> on_conflict;

    // RETURNING clause
    std::vector<ResolvedSelectItem> returning;
};

/**
 * Resolved UPDATE statement
 */
struct ResolvedUpdateStmt : public ResolvedStatement {
    ResolvedTableRef target_table;

    // SET clause: column index -> expression
    std::vector<std::pair<uint32_t, ResolvedExpression*>> assignments;

    // FROM clause (for UPDATE ... FROM ... syntax)
    std::vector<ResolvedTableRef*> from_tables;
    std::vector<ResolvedJoin*> joins;

    // WHERE clause
    ResolvedExpression* where = nullptr;

    // RETURNING clause
    std::vector<ResolvedSelectItem> returning;
};

/**
 * Resolved DELETE statement
 */
struct ResolvedDeleteStmt : public ResolvedStatement {
    ResolvedTableRef target_table;

    // USING clause
    std::vector<ResolvedTableRef*> using_tables;
    std::vector<ResolvedJoin*> using_joins;

    // WHERE clause
    ResolvedExpression* where = nullptr;

    // RETURNING clause
    std::vector<ResolvedSelectItem> returning;
};

// =============================================================================
// Resolved DDL Statements
// =============================================================================

/**
 * Resolved column definition
 */
struct ResolvedColumnDef {
    StringPool::StringId name;
    ResolvedType type;
    bool is_nullable = true;
    ResolvedExpression* default_value = nullptr;

    // Constraints
    bool is_primary_key = false;
    bool is_unique = false;
    ResolvedExpression* check_expr = nullptr;

    // Foreign key
    bool has_fk = false;
    ID fk_table_uuid;
    uint32_t fk_column_index;
    ForeignKeyAction on_delete = ForeignKeyAction::NO_ACTION;
    ForeignKeyAction on_update = ForeignKeyAction::NO_ACTION;
};

/**
 * Resolved table constraint
 */
struct ResolvedTableConstraint {
    StringPool::StringId name = StringPool::INVALID_ID;

    enum class Type {
        PRIMARY_KEY,
        UNIQUE,
        FOREIGN_KEY,
        CHECK,
    };
    Type constraint_type;

    // For PRIMARY KEY and UNIQUE
    std::vector<uint32_t> column_indexes;

    // For FOREIGN KEY
    ID fk_table_uuid;
    std::vector<uint32_t> fk_column_indexes;
    ForeignKeyAction on_delete = ForeignKeyAction::NO_ACTION;
    ForeignKeyAction on_update = ForeignKeyAction::NO_ACTION;

    // For CHECK
    ResolvedExpression* check_expr = nullptr;
};

/**
 * Resolved CREATE TABLE statement
 */
struct ResolvedCreateTableStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId table_name;
    bool if_not_exists = false;

    std::vector<ResolvedColumnDef> columns;
    std::vector<ResolvedTableConstraint> constraints;

    // Storage options
    uint16_t tablespace_id = 0;
};

/**
 * Resolved CREATE INDEX statement
 */
struct ResolvedCreateIndexStmt : public ResolvedStatement {
    StringPool::StringId index_name;
    bool unique = false;
    bool if_not_exists = false;
    bool concurrent = false;

    ID table_uuid;
    std::vector<uint32_t> column_indexes;
    std::vector<bool> column_desc;  // DESC for each column

    StringPool::StringId index_method;  // btree, hash, gin, etc.
    ResolvedExpression* where_clause = nullptr;  // Partial index

    uint16_t tablespace_id = 0;
};

/**
 * Resolved CREATE VIEW statement
 */
struct ResolvedCreateViewStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId view_name;
    bool or_replace = false;
    bool materialized = false;

    std::vector<StringPool::StringId> column_names;
    ResolvedSelectStmt* query = nullptr;

    bool check_option = false;
};

/**
 * Resolved DROP statement (table, view, index)
 */
struct ResolvedDropStmt : public ResolvedStatement {
    enum class ObjectType {
        TABLE,
        VIEW,
        INDEX,
        SEQUENCE,
    };

    ObjectType object_type;
    std::vector<ID> object_uuids;
    bool if_exists = false;
    bool cascade = false;
};

/**
 * Resolved TRUNCATE TABLE statement
 *
 * TRUNCATE performs a fast table truncation by starting a background
 * thread that deletes all rows and sweeps garbage.
 */
struct ResolvedTruncateTableStmt : public ResolvedStatement {
    std::vector<ID> table_uuids;      // Tables to truncate
    bool cascade = false;             // CASCADE to dependent tables
    bool restart_identity = false;    // RESTART IDENTITY for sequences
    bool async_mode = true;           // ASYNC (default) or SYNC mode
};

/**
 * Resolved EXPLAIN statement
 *
 * Shows the execution plan for a query without executing it.
 */
struct ResolvedExplainStmt : public ResolvedStatement {
    ResolvedStatement* query = nullptr;  // The statement to explain
    bool analyze = false;                 // EXPLAIN ANALYZE (actually execute)
    bool verbose = false;                 // VERBOSE output
    bool costs = true;                    // Show cost estimates (default true)
    bool buffers = false;                 // Show buffer usage
    bool timing = true;                   // Show timing (with ANALYZE)
    bool format_json = false;             // JSON output format
    bool format_xml = false;              // XML output format
    bool format_yaml = false;             // YAML output format
};

// =============================================================================
// Resolved Transaction/Session Statements
// =============================================================================

/**
 * Resolved START TRANSACTION
 */
struct ResolvedStartTransactionStmt : public ResolvedStatement {
    bool has_isolation_level = false;
    IsolationLevel isolation_level = IsolationLevel::READ_COMMITTED;
    bool has_access_mode = false;
    TransactionAccess access_mode = TransactionAccess::READ_WRITE;
    bool deferrable = false;
};

/**
 * Resolved COMMIT
 */
struct ResolvedCommitStmt : public ResolvedStatement {
    bool and_chain = false;
};

/**
 * Resolved ROLLBACK
 */
struct ResolvedRollbackStmt : public ResolvedStatement {
    bool to_savepoint = false;
    StringPool::StringId savepoint_name = StringPool::INVALID_ID;
    bool and_chain = false;
};

/**
 * Resolved SAVEPOINT
 */
struct ResolvedSavepointStmt : public ResolvedStatement {
    StringPool::StringId name;
};

/**
 * Resolved SET statement
 */
struct ResolvedSetStmt : public ResolvedStatement {
    SetStmt::SetType set_type;
    SetStmt::Scope scope = SetStmt::Scope::SESSION;

    StringPool::StringId variable_name = StringPool::INVALID_ID;
    bool is_default = false;
    ResolvedExpression* value = nullptr;

    // For SET TRANSACTION
    bool has_isolation_level = false;
    IsolationLevel isolation_level = IsolationLevel::READ_COMMITTED;
    bool has_access_mode = false;
    TransactionAccess access_mode = TransactionAccess::READ_WRITE;

    // For SET PARSER VERSION
    uint8_t parser_version = 0;  // 1 or 2 (0 = not set)
};

/**
 * Resolved SHOW statement
 *
 * Supports various SHOW commands:
 * - Session: SHOW variable_name, SHOW ALL, SHOW TRANSACTION ISOLATION LEVEL
 * - Catalog: SHOW TABLES, SHOW DATABASES, SHOW COLUMNS, SHOW INDEXES, SHOW CREATE TABLE
 * - Firebird ISQL: SHOW TABLE, SHOW INDEX, SHOW TRIGGER, SHOW VIEW, SHOW PROCEDURE,
 *                  SHOW FUNCTION, SHOW DOMAIN, SHOW GENERATOR, SHOW SCHEMA, SHOW ROLE,
 *                  SHOW GRANTS, SHOW CHECKS, SHOW COLLATIONS, SHOW SQL DIALECT,
 *                  SHOW VERSION, SHOW DATABASE, SHOW SYSTEM, SHOW PARSER VERSION
 */
struct ResolvedShowStmt : public ResolvedStatement {
    ShowStmt::ShowType show_type;

    // Object name for commands that take one (TABLE, INDEX, TRIGGER, etc.)
    StringPool::StringId variable_name = StringPool::INVALID_ID;

    // For SHOW TABLES FROM database, SHOW COLUMNS FROM table
    StringPool::StringId from_name = StringPool::INVALID_ID;

    // For LIKE pattern filtering
    StringPool::StringId like_pattern = StringPool::INVALID_ID;
};

/**
 * Resolved ANALYZE statement
 * V2 MIGRATION: Added for optimizer V2 compatibility
 */
struct ResolvedAnalyzeStmt : public ResolvedStatement {
    // Table to analyze (optional - if empty, analyze all tables)
    ResolvedTableRef* table = nullptr;

    // Specific columns to analyze (optional - if empty, analyze all columns)
    std::vector<StringPool::StringId> columns;

    // VERBOSE option
    bool verbose = false;
};

// =============================================================================
// Arena for Resolved AST
// =============================================================================

/**
 * Arena allocator for resolved AST nodes
 */
class ResolvedASTArena {
public:
    ResolvedASTArena(size_t block_size = 64 * 1024);
    ~ResolvedASTArena();

    // Non-copyable
    ResolvedASTArena(const ResolvedASTArena&) = delete;
    ResolvedASTArena& operator=(const ResolvedASTArena&) = delete;

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        T* obj = new (mem) T(std::forward<Args>(args)...);
        if constexpr (!std::is_trivially_destructible_v<T>) {
            trackDestructor([obj]() { obj->~T(); });
        }
        return obj;
    }

    void* allocate(size_t size, size_t alignment);
    void trackDestructor(std::function<void()> dtor);
    void reset();

private:
    struct Block {
        char* data;
        size_t size;
        size_t used;
        Block* next;
    };

    Block* current_block_;
    size_t block_size_;
    size_t total_allocated_;
    std::vector<std::function<void()>> destructors_;

    Block* allocateBlock(size_t size);
    void callDestructors();
};

} // namespace scratchbird::parser::v2
