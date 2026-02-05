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
using v2::PolicyType;

// Forward declarations
class ResolvedStatement;
class ResolvedExpression;
struct ResolvedSelectStmt;

// =============================================================================
// Resolved Reference Types
// =============================================================================

/**
 * Resolved table reference - points to actual catalog entry
 */
struct ResolvedTableRef {
    ID table_uuid;                      // UUID of table/view in catalog
    ID schema_uuid;                     // Schema containing the object
    StringPool::StringId name;          // Original table name (for bytecode emission)
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
 * Resolved CTE definition
 */
struct ResolvedCTE {
    StringPool::StringId name = StringPool::INVALID_ID;
    std::vector<StringPool::StringId> column_names;
    ResolvedSelectStmt* query = nullptr;
};

/**
 * Resolved WITH clause
 */
struct ResolvedWithClause {
    bool recursive = false;
    std::vector<ResolvedCTE> ctes;
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

enum class FunctionKind : uint8_t
{
    BUILTIN,
    FUNCTION,
    PROCEDURE,
    UDR,
};

struct ResolvedFunctionRef {
    ID function_uuid;                   // UUID of function in catalog (zero for built-in)
    StringPool::StringId function_name; // Function name
    bool is_builtin = true;             // True for system functions
    bool is_aggregate = false;          // True for aggregate functions
    bool is_window = false;             // True for window functions
    FunctionKind kind = FunctionKind::BUILTIN; // Function kind for catalog dispatch
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
    bool is_domain = false;
    ID domain_id{};

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
 * Resolved variable reference (PSQL)
 */
struct ResolvedVariableExpr : public ResolvedExpression {
    StringPool::StringId name = StringPool::INVALID_ID;
};

/**
 * Resolved parameter placeholder expression
 */
struct ResolvedParameterExpr : public ResolvedExpression {
    bool is_named = false;
    uint32_t index = 0;
    StringPool::StringId name = StringPool::INVALID_ID;
};

// =============================================================================
// PSQL Resolved Statements
// =============================================================================

enum class ResolvedPsqlStmtKind : uint8_t {
    BLOCK,
    DECLARE_VAR,
    ASSIGN,
    IF,
    WHILE_LOOP,
    LOOP,
    LEAVE,
    CONTINUE,
    EXIT,
    RETURN,
    SUSPEND,
    RAISE,
    DECLARE_CURSOR,
    OPEN_CURSOR,
    FETCH_CURSOR,
    CLOSE_CURSOR,
    FOR_SELECT,
    FOR_EXECUTE,
    EXECUTE_PROCEDURE,
    EXECUTE_STATEMENT,
    SQL_STATEMENT
};

struct ResolvedPsqlStatement {
    virtual ~ResolvedPsqlStatement() = default;
    ResolvedPsqlStmtKind kind = ResolvedPsqlStmtKind::SQL_STATEMENT;
    SourceSpan span;
};

struct ResolvedPsqlVariable {
    StringPool::StringId name = StringPool::INVALID_ID;
    ResolvedType type;
    bool has_default = false;
    bool not_null = false;
    ResolvedExpression* default_value = nullptr;
};

struct ResolvedPsqlExceptionHandler {
    parser::v2::WhenExceptionStmt::ExceptionType type =
        parser::v2::WhenExceptionStmt::ExceptionType::ANY;
    int32_t sqlcode = 0;
    StringPool::StringId gdscode = StringPool::INVALID_ID;
    StringPool::StringId exception_name = StringPool::INVALID_ID;
    ResolvedPsqlStatement* handler = nullptr;
};

struct ResolvedPsqlBlock : public ResolvedPsqlStatement {
    std::vector<ResolvedPsqlVariable> variables;
    std::vector<ResolvedPsqlStatement*> statements;
    std::vector<ResolvedPsqlExceptionHandler> exception_handlers;
};

struct ResolvedPsqlDeclareVar : public ResolvedPsqlStatement {
    StringPool::StringId name = StringPool::INVALID_ID;
    ResolvedType type;
    bool has_default = false;
    bool not_null = false;
    ResolvedExpression* default_value = nullptr;
};

struct ResolvedPsqlAssign : public ResolvedPsqlStatement {
    StringPool::StringId variable = StringPool::INVALID_ID;
    ResolvedExpression* value = nullptr;
};

struct ResolvedPsqlIf : public ResolvedPsqlStatement {
    ResolvedExpression* condition = nullptr;
    ResolvedPsqlStatement* then_branch = nullptr;
    ResolvedPsqlStatement* else_branch = nullptr;
};

struct ResolvedPsqlWhile : public ResolvedPsqlStatement {
    ResolvedExpression* condition = nullptr;
    ResolvedPsqlStatement* body = nullptr;
    StringPool::StringId label = StringPool::INVALID_ID;
};

struct ResolvedPsqlLoop : public ResolvedPsqlStatement {
    ResolvedPsqlStatement* body = nullptr;
    StringPool::StringId label = StringPool::INVALID_ID;
};

struct ResolvedPsqlLeave : public ResolvedPsqlStatement {
    StringPool::StringId label = StringPool::INVALID_ID;
};

struct ResolvedPsqlContinue : public ResolvedPsqlStatement {
    StringPool::StringId label = StringPool::INVALID_ID;
};

struct ResolvedPsqlExit : public ResolvedPsqlStatement {
};

struct ResolvedPsqlReturn : public ResolvedPsqlStatement {
    bool has_value = false;
    ResolvedExpression* value = nullptr;
};

struct ResolvedPsqlSuspend : public ResolvedPsqlStatement {
};

struct ResolvedPsqlRaise : public ResolvedPsqlStatement {
    StringPool::StringId exception_name = StringPool::INVALID_ID;
    ResolvedExpression* message = nullptr;
};

struct ResolvedPsqlDeclareCursor : public ResolvedPsqlStatement {
    StringPool::StringId cursor_name = StringPool::INVALID_ID;
    ResolvedStatement* select_stmt = nullptr;
    bool scroll = false;
};

struct ResolvedPsqlOpenCursor : public ResolvedPsqlStatement {
    StringPool::StringId cursor_name = StringPool::INVALID_ID;
};

struct ResolvedPsqlFetchCursor : public ResolvedPsqlStatement {
    StringPool::StringId cursor_name = StringPool::INVALID_ID;
    parser::v2::FetchCursorStmt::Direction direction =
        parser::v2::FetchCursorStmt::Direction::NEXT;
    ResolvedExpression* offset = nullptr;
    std::vector<StringPool::StringId> into_variables;
};

struct ResolvedPsqlCloseCursor : public ResolvedPsqlStatement {
    StringPool::StringId cursor_name = StringPool::INVALID_ID;
};

struct ResolvedPsqlForSelect : public ResolvedPsqlStatement {
    ResolvedStatement* select_stmt = nullptr;
    std::vector<StringPool::StringId> into_variables;
    ResolvedPsqlStatement* body = nullptr;
};

struct ResolvedPsqlForExecute : public ResolvedPsqlStatement {
    ResolvedExpression* sql = nullptr;
    std::vector<ResolvedExpression*> parameters;
    std::vector<StringPool::StringId> into_variables;
    ResolvedPsqlStatement* body = nullptr;
};

struct ResolvedPsqlExecuteProcedure : public ResolvedPsqlStatement {
    SchemaPath procedure_path;
    std::vector<ResolvedExpression*> arguments;
    std::vector<StringPool::StringId> returning_variables;
};

struct ResolvedPsqlExecuteStatement : public ResolvedPsqlStatement {
    ResolvedExpression* sql = nullptr;
    std::vector<ResolvedExpression*> parameters;
    std::vector<StringPool::StringId> into_variables;
};

struct ResolvedPsqlSqlStatement : public ResolvedPsqlStatement {
    ResolvedStatement* statement = nullptr;
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
    core::CastFormat format = core::CastFormat::DEFAULT;
    bool implicit = false;  // True if inserted by semantic analyzer
};

/**
 * Resolved element selector for EXTRACT/ALTER_ELEMENT
 */
struct ResolvedElementSelector {
    uint8_t field_id = 0;
    StringPool::StringId field_name = StringPool::INVALID_ID;
    std::vector<ResolvedExpression*> args;
};

/**
 * Resolved EXTRACT expression
 */
struct ResolvedExtractExpr : public ResolvedExpression {
    ResolvedElementSelector selector;
    ResolvedExpression* source = nullptr;
};

/**
 * Resolved ALTER_ELEMENT expression
 */
struct ResolvedAlterElementExpr : public ResolvedExpression {
    ResolvedElementSelector selector;
    ResolvedExpression* source = nullptr;
    ResolvedExpression* new_value = nullptr;
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
    LikeMatchKind match_kind = LikeMatchKind::LIKE;
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
    ResolvedWithClause* with = nullptr;

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
    parser::GroupingType grouping_type = parser::GroupingType::STANDARD;
    std::vector<std::vector<ResolvedExpression*>> grouping_sets;

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
    ResolvedWithClause* with = nullptr;

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
    ResolvedWithClause* with = nullptr;

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
    ResolvedWithClause* with = nullptr;

    ResolvedTableRef target_table;

    // USING clause
    std::vector<ResolvedTableRef*> using_tables;
    std::vector<ResolvedJoin*> using_joins;

    // WHERE clause
    ResolvedExpression* where = nullptr;

    // RETURNING clause
    std::vector<ResolvedSelectItem> returning;
};

/**
 * Resolved MERGE statement
 */
struct ResolvedMergeStmt : public ResolvedStatement {
    SchemaPath target_table;
    StringPool::StringId target_alias = StringPool::INVALID_ID;
    SchemaPath source_table;
    StringPool::StringId source_alias = StringPool::INVALID_ID;
    ResolvedExpression* on_condition = nullptr;

    struct WhenMatched {
        ResolvedExpression* and_condition = nullptr;
        std::vector<std::pair<StringPool::StringId, ResolvedExpression*>> assignments;
        bool is_delete = false;
    };
    std::vector<WhenMatched> when_matched;

    struct WhenNotMatched {
        ResolvedExpression* and_condition = nullptr;
        std::vector<StringPool::StringId> columns;
        std::vector<ResolvedExpression*> values;
    };
    std::vector<WhenNotMatched> when_not_matched;

    struct WhenNotMatchedBySource {
        ResolvedExpression* and_condition = nullptr;
        std::vector<std::pair<StringPool::StringId, ResolvedExpression*>> assignments;
        bool is_delete = false;
    };
    std::vector<WhenNotMatchedBySource> when_not_matched_by_source;
};

/**
 * Resolved COPY statement
 */
struct ResolvedCopyOptions {
    enum class Format : uint8_t {
        TEXT = 0,
        CSV = 1,
        BINARY = 2
    };

    enum class OnError : uint8_t {
        ABORT = 0,
        SKIP = 1
    };

    Format format = Format::TEXT;
    char delimiter = '\t';
    std::string null_string = "\\N";
    bool header = false;

    char quote = '"';
    char escape = '\\';
    std::string encoding;

    uint32_t batch_size = 10000;
    uint32_t max_errors = 0;
    OnError on_error = OnError::ABORT;
};

struct ResolvedCopyStmt : public ResolvedStatement {
    bool has_table = false;
    ResolvedTableRef target_table;
    StringPool::StringId table_path = StringPool::INVALID_ID;
    ResolvedSelectStmt* query = nullptr;

    // Column list (optional)
    std::vector<uint32_t> target_column_indexes;

    enum class Direction {
        FROM,
        TO
    };
    Direction direction = Direction::FROM;

    bool target_is_stdin = false;
    bool target_is_stdout = false;
    StringPool::StringId target = StringPool::INVALID_ID;  // File path when not STDIN/STDOUT

    ResolvedCopyOptions options;
};

/**
 * Resolved COMMENT ON statement
 */
struct ResolvedCommentStmt : public ResolvedStatement {
    CommentObjectType object_type = CommentObjectType::TABLE;
    ID object_id{};
    bool is_null = false;
    std::string comment_text;
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
    StringPool::StringId collation_name = StringPool::INVALID_ID;

    // Constraints
    bool is_primary_key = false;
    bool is_unique = false;
    ResolvedExpression* check_expr = nullptr;

    // Foreign key
    bool has_fk = false;
    ID fk_table_uuid;
    uint32_t fk_column_index;
    SchemaPath fk_table_path;
    std::vector<StringPool::StringId> fk_column_names;
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
    Type constraint_type = Type::CHECK;

    // For PRIMARY KEY and UNIQUE
    std::vector<uint32_t> column_indexes;
    std::vector<StringPool::StringId> column_names;

    // For FOREIGN KEY
    ID fk_table_uuid;
    SchemaPath fk_table_path;
    std::vector<uint32_t> fk_column_indexes;
    std::vector<StringPool::StringId> fk_column_names;
    ForeignKeyAction on_delete = ForeignKeyAction::NO_ACTION;
    ForeignKeyAction on_update = ForeignKeyAction::NO_ACTION;

    // For CHECK
    ResolvedExpression* check_expr = nullptr;
};

/**
 * Resolved domain constraint
 */
struct ResolvedDomainConstraint {
    DomainConstraintType type = DomainConstraintType::CHECK;
    StringPool::StringId name = StringPool::INVALID_ID;
    std::string expression;
};

/**
 * Resolved domain RECORD field definition
 */
struct ResolvedDomainRecordField {
    StringPool::StringId name = StringPool::INVALID_ID;
    ResolvedType type;
    bool nullable = true;
    std::string default_value;
};

/**
 * Resolved domain ENUM value definition
 */
struct ResolvedDomainEnumValue {
    StringPool::StringId label = StringPool::INVALID_ID;
    int32_t position = 0;
};

struct ResolvedRangeOptions {
    ResolvedType subtype;
    bool has_subtype = false;
    bool has_subtype_collation = false;
    std::string subtype_collation;
    bool has_subtype_opclass = false;
    std::string subtype_opclass;
    bool has_canonical = false;
    std::string canonical;
    bool has_subtype_diff = false;
    std::string subtype_diff;
    bool has_multirange = false;
    bool multirange = false;
};

struct ResolvedBaseOptions {
    ResolvedType storage;
    bool has_storage = false;
    std::string input_function;
    std::string output_function;
    bool has_receive = false;
    std::string receive_function;
    bool has_send = false;
    std::string send_function;
    bool has_typmod_in = false;
    std::string typmod_in_function;
    bool has_typmod_out = false;
    std::string typmod_out_function;
    bool has_analyze = false;
    std::string analyze_function;
    bool has_alignment = false;
    BaseTypeAlignment alignment = BaseTypeAlignment::INT;
    bool has_storage_mode = false;
    BaseTypeStorageMode storage_mode = BaseTypeStorageMode::PLAIN;
    bool has_category = false;
    char category = '\0';
    bool has_preferred = false;
    bool preferred = false;
};

/**
 * Resolved CREATE TABLE statement
 */
struct ResolvedCreateTableStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId table_name;
    StringPool::StringId table_path = StringPool::INVALID_ID;
    bool if_not_exists = false;
    bool or_replace = false;
    TempTableType temp_type = TempTableType::NONE;
    TempOnCommitAction on_commit = TempOnCommitAction::NONE;
    bool unlogged = false;

    std::vector<ResolvedColumnDef> columns;
    std::vector<ResolvedTableConstraint> constraints;

    // Storage options
    uint16_t tablespace_id = 0;
    StringPool::StringId tablespace_name = StringPool::INVALID_ID;

    // Partitioning metadata (native parser)
    bool is_partitioned = false;
    StringPool::StringId partition_by = StringPool::INVALID_ID;
    std::vector<StringPool::StringId> partition_columns;

    // Inheritance metadata (native parser)
    std::vector<StringPool::StringId> inherits;

    // CREATE TABLE AS SELECT
    ResolvedSelectStmt* as_query = nullptr;
};

/**
 * Resolved CREATE INDEX statement
 */
struct ResolvedCreateIndexStmt : public ResolvedStatement {
    StringPool::StringId index_name;
    bool unique = false;
    bool if_not_exists = false;
    bool concurrent = false;

    StringPool::StringId table_path = StringPool::INVALID_ID;
    ID table_uuid;
    std::vector<uint32_t> column_indexes;
    std::vector<bool> column_desc;  // DESC for each column
    std::vector<StringPool::StringId> column_names;
    std::vector<uint32_t> include_column_indexes;
    std::vector<StringPool::StringId> include_column_names;
    std::vector<ResolvedExpression*> expressions;  // Expression index keys (all keys must be expressions)

    StringPool::StringId index_method;  // btree, hash, gin, etc.
    ResolvedExpression* where_clause = nullptr;  // Partial index

    uint16_t tablespace_id = 0;
    StringPool::StringId tablespace_name = StringPool::INVALID_ID;

    bool bloom_filter_enabled = false;
    bool bloom_fpr_set = false;
    double bloom_fpr = 0.01;
};

/**
 * Resolved CREATE VIEW statement
 */
struct ResolvedCreateViewStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId view_name;
    bool or_replace = false;
    bool if_not_exists = false;
    bool temporary = false;
    bool materialized = false;

    std::vector<StringPool::StringId> column_names;
    ResolvedSelectStmt* query = nullptr;

    bool check_option = false;
};

struct ResolvedCreateSequenceStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId sequence_name;
    bool or_replace = false;
    bool if_not_exists = false;
    bool temporary = false;

    std::optional<int64_t> start_with;
    std::optional<int64_t> increment_by;
    std::optional<int64_t> min_value;
    std::optional<int64_t> max_value;
    std::optional<int64_t> cache;
    bool cycle = false;
    bool has_owned_by = false;
    StringPool::StringId owned_by_table = StringPool::INVALID_ID;
    StringPool::StringId owned_by_column = StringPool::INVALID_ID;
};

struct ResolvedAlterSequenceStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId sequence_name;

    std::optional<int64_t> increment_by;
    std::optional<int64_t> min_value;
    std::optional<int64_t> max_value;
    std::optional<int64_t> restart_with;
    std::optional<int64_t> cache;
    std::optional<bool> cycle;
};

struct ResolvedRoutineParam {
    uint8_t mode = 0;  // 0=IN, 1=OUT, 2=INOUT
    StringPool::StringId name = StringPool::INVALID_ID;
    ResolvedType type;
};

/**
 * Resolved CREATE FUNCTION statement
 */
struct ResolvedCreateFunctionStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId function_name;
    bool or_replace = false;
    bool deterministic = false;
    RoutineSqlSecurity sql_security = RoutineSqlSecurity::INVOKER;
    std::vector<ResolvedRoutineParam> params;
    ResolvedType return_type;
    std::string body;
    std::vector<uint8_t> psql_bytecode;
};

/**
 * Resolved CREATE PROCEDURE statement
 */
struct ResolvedCreateProcedureStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId procedure_name;
    bool or_replace = false;
    RoutineSqlSecurity sql_security = RoutineSqlSecurity::INVOKER;
    std::vector<ResolvedRoutineParam> params;
    std::string body;
    std::vector<uint8_t> psql_bytecode;
};

/**
 * Resolved CREATE TRIGGER statement
 */
struct ResolvedCreateTriggerStmt : public ResolvedStatement {
    StringPool::StringId trigger_name;
    SchemaPath table_path;
    bool active = true;
    TriggerTiming timing = TriggerTiming::BEFORE;
    uint8_t event_mask = 1u << static_cast<uint8_t>(TriggerEvent::INSERT);
    TriggerGranularity granularity = TriggerGranularity::FOR_EACH_ROW;
    std::string body;
    std::vector<uint8_t> psql_bytecode;
};

/**
 * Resolved EXECUTE BLOCK statement
 */
struct ResolvedExecuteBlockStmt : public ResolvedStatement {
    std::vector<ResolvedPsqlVariable> input_params;
    std::vector<ResolvedPsqlVariable> output_params;
    ResolvedPsqlBlock* body = nullptr;
};

/**
 * Resolved EXECUTE PROCEDURE statement
 */
struct ResolvedExecuteProcedureStmt : public ResolvedStatement {
    SchemaPath procedure_path;
    std::vector<ResolvedExpression*> arguments;
    std::vector<StringPool::StringId> returning_variables;
};

/**
 * Resolved EXECUTE STATEMENT (dynamic SQL) statement
 */
struct ResolvedExecuteStatementStmt : public ResolvedStatement {
    ResolvedExpression* sql = nullptr;
    std::vector<ResolvedExpression*> parameters;
    std::vector<StringPool::StringId> into_variables;
};

/**
 * Resolved CREATE PACKAGE statement
 */
struct ResolvedCreatePackageStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId package_name;
    bool or_replace = false;
    bool is_body = false;
    std::string header;
    std::string body;
};

/**
 * Resolved CREATE USER statement
 */
struct ResolvedCreateUserStmt : public ResolvedStatement {
    StringPool::StringId user_name = StringPool::INVALID_ID;
    bool has_password = false;
    bool is_superuser = false;
    std::string password;
};

/**
 * Resolved CREATE ROLE statement
 */
struct ResolvedCreateRoleStmt : public ResolvedStatement {
    StringPool::StringId role_name = StringPool::INVALID_ID;
};

/**
 * Resolved CREATE JOB statement
 */
struct ResolvedCreateJobStmt : public ResolvedStatement {
    bool or_alter = false;
    bool recreate = false;

    StringPool::StringId job_name = StringPool::INVALID_ID;
    JobType job_type = JobType::SQL;
    StringPool::StringId job_sql = StringPool::INVALID_ID;
    StringPool::StringId procedure_name = StringPool::INVALID_ID;
    StringPool::StringId external_command = StringPool::INVALID_ID;

    JobScheduleKind schedule_kind = JobScheduleKind::CRON;
    StringPool::StringId cron_expression = StringPool::INVALID_ID;
    StringPool::StringId at_timestamp = StringPool::INVALID_ID;
    int64_t interval_seconds = 0;
    StringPool::StringId starts_at = StringPool::INVALID_ID;
    StringPool::StringId ends_at = StringPool::INVALID_ID;

    bool has_max_retries = false;
    uint32_t max_retries = 0;
    bool has_retry_backoff = false;
    uint32_t retry_backoff_seconds = 0;
    bool has_timeout = false;
    uint32_t timeout_seconds = 0;

    bool has_on_completion = false;
    JobOnCompletion on_completion = JobOnCompletion::PRESERVE;

    bool has_run_as = false;
    StringPool::StringId run_as_role = StringPool::INVALID_ID;
    bool has_description = false;
    StringPool::StringId description = StringPool::INVALID_ID;
    bool has_state = false;
    JobState state = JobState::ENABLED;

    StringPool::StringId job_class = StringPool::INVALID_ID;
    StringPool::StringId partition_strategy = StringPool::INVALID_ID;
    StringPool::StringId partition_expression = StringPool::INVALID_ID;
    StringPool::StringId partition_shard = StringPool::INVALID_ID;

    std::vector<StringPool::StringId> depends_on;
};

/**
 * Resolved ALTER JOB statement
 */
struct ResolvedAlterJobStmt : public ResolvedStatement {
    StringPool::StringId job_name = StringPool::INVALID_ID;

    bool has_schedule = false;
    JobScheduleKind schedule_kind = JobScheduleKind::CRON;
    StringPool::StringId cron_expression = StringPool::INVALID_ID;
    StringPool::StringId at_timestamp = StringPool::INVALID_ID;
    int64_t interval_seconds = 0;
    StringPool::StringId starts_at = StringPool::INVALID_ID;
    StringPool::StringId ends_at = StringPool::INVALID_ID;

    bool has_job_body = false;
    JobType job_type = JobType::SQL;
    StringPool::StringId job_sql = StringPool::INVALID_ID;
    StringPool::StringId procedure_name = StringPool::INVALID_ID;
    StringPool::StringId external_command = StringPool::INVALID_ID;

    bool has_state = false;
    JobState state = JobState::ENABLED;
    bool has_max_retries = false;
    uint32_t max_retries = 0;
    bool has_retry_backoff = false;
    uint32_t retry_backoff_seconds = 0;
    bool has_timeout = false;
    uint32_t timeout_seconds = 0;
    bool has_on_completion = false;
    JobOnCompletion on_completion = JobOnCompletion::PRESERVE;
    bool has_run_as = false;
    StringPool::StringId run_as_role = StringPool::INVALID_ID;
    bool has_description = false;
    StringPool::StringId description = StringPool::INVALID_ID;

    bool has_job_class = false;
    StringPool::StringId job_class = StringPool::INVALID_ID;
    bool has_partition = false;
    StringPool::StringId partition_strategy = StringPool::INVALID_ID;
    StringPool::StringId partition_expression = StringPool::INVALID_ID;
    StringPool::StringId partition_shard = StringPool::INVALID_ID;

    bool has_depends_on = false;
    bool clear_depends_on = false;
    std::vector<StringPool::StringId> depends_on;

    bool has_secret = false;
    bool drop_secret = false;
    StringPool::StringId secret_key = StringPool::INVALID_ID;
    StringPool::StringId secret_value = StringPool::INVALID_ID;
};

/**
 * Resolved DROP JOB statement
 */
struct ResolvedDropJobStmt : public ResolvedStatement {
    StringPool::StringId job_name = StringPool::INVALID_ID;
    bool keep_history = false;
};

/**
 * Resolved EXECUTE JOB statement
 */
struct ResolvedExecuteJobStmt : public ResolvedStatement {
    StringPool::StringId job_name = StringPool::INVALID_ID;
};

/**
 * Resolved CANCEL JOB RUN statement
 */
struct ResolvedCancelJobRunStmt : public ResolvedStatement {
    StringPool::StringId job_run_uuid = StringPool::INVALID_ID;
};

/**
 * Resolved CREATE EXCEPTION statement
 */
struct ResolvedCreateExceptionStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId exception_name = StringPool::INVALID_ID;
    bool or_replace = false;
    std::string message;
};

/**
 * Resolved CREATE SCHEMA statement
 */
struct ResolvedCreateSchemaStmt : public ResolvedStatement {
    bool if_not_exists = false;
    SchemaPath schema_path;
    StringPool::StringId owner = StringPool::INVALID_ID;
};

/**
 * Resolved DROP SCHEMA statement
 */
struct ResolvedDropSchemaStmt : public ResolvedStatement {
    bool if_exists = false;
    bool cascade = false;
    bool restrict = false;
    std::vector<SchemaPath> schema_paths;
};

struct ResolvedDatabaseOption {
    std::string key;
    std::string value;
};

/**
 * Resolved CREATE DATABASE statement
 */
struct ResolvedCreateDatabaseStmt : public ResolvedStatement {
    bool if_not_exists = false;
    SchemaPath database_path;
    std::string source_spec;
    std::vector<ResolvedDatabaseOption> options;
    std::vector<std::string> aliases;
};

struct ResolvedTablespaceAlteration {
    TablespaceAlterAction action = TablespaceAlterAction::SET_AUTOEXTEND;
    bool autoextend_enabled = false;
    uint32_t size_mb = 0;
    StringPool::StringId new_name = StringPool::INVALID_ID;
};

/**
 * Resolved CREATE TABLESPACE statement
 */
struct ResolvedCreateTablespaceStmt : public ResolvedStatement {
    StringPool::StringId tablespace_name = StringPool::INVALID_ID;
    std::string location;
    bool has_autoextend = false;
    bool autoextend_enabled = false;
    bool has_autoextend_size = false;
    uint32_t autoextend_size_mb = 0;
    bool has_maxsize = false;
    bool maxsize_unlimited = false;
    uint32_t maxsize_mb = 0;
    bool has_prealloc = false;
    uint32_t prealloc_mb = 0;
};

/**
 * Resolved ALTER TABLESPACE statement
 */
struct ResolvedAlterTablespaceStmt : public ResolvedStatement {
    StringPool::StringId tablespace_name = StringPool::INVALID_ID;
    std::vector<ResolvedTablespaceAlteration> alterations;
};

/**
 * Resolved DROP TABLESPACE statement
 */
struct ResolvedDropTablespaceStmt : public ResolvedStatement {
    StringPool::StringId tablespace_name = StringPool::INVALID_ID;
    bool force = false;
};

/**
 * Resolved ATTACH TABLESPACE statement
 */
struct ResolvedAttachTablespaceStmt : public ResolvedStatement {
    StringPool::StringId tablespace_name = StringPool::INVALID_ID;
    std::string location;
    bool validate = false;
    bool allow_mismatch = false;
};

/**
 * Resolved DETACH TABLESPACE statement
 */
struct ResolvedDetachTablespaceStmt : public ResolvedStatement {
    StringPool::StringId tablespace_name = StringPool::INVALID_ID;
    bool force = false;
};

/**
 * Resolved CREATE GROUP statement
 */
struct ResolvedCreateGroupStmt : public ResolvedStatement {
    StringPool::StringId group_name = StringPool::INVALID_ID;
};

/**
 * Resolved CREATE POLICY statement
 */
struct ResolvedCreatePolicyStmt : public ResolvedStatement {
    StringPool::StringId policy_name = StringPool::INVALID_ID;
    SchemaPath table_path;
    PolicyType policy_type = PolicyType::ALL;
    bool is_permissive = true;
    std::vector<StringPool::StringId> roles;
    ResolvedExpression* using_expr = nullptr;
    ResolvedExpression* with_check_expr = nullptr;
};

/**
 * Resolved ALTER POLICY statement
 */
struct ResolvedAlterPolicyStmt : public ResolvedStatement {
    StringPool::StringId policy_name = StringPool::INVALID_ID;
    SchemaPath table_path;
    std::vector<StringPool::StringId> roles;
    ResolvedExpression* using_expr = nullptr;
    ResolvedExpression* with_check_expr = nullptr;
};

/**
 * Resolved DROP POLICY statement
 */
struct ResolvedDropPolicyStmt : public ResolvedStatement {
    StringPool::StringId policy_name = StringPool::INVALID_ID;
    SchemaPath table_path;
    bool if_exists = false;
};

/**
 * Resolved DROP GROUP statement
 */
struct ResolvedDropGroupStmt : public ResolvedStatement {
    bool if_exists = false;
    bool cascade = false;
    std::vector<SchemaPath> groups;
};

/**
 * Resolved CREATE FOREIGN SERVER statement
 */
struct ResolvedCreateForeignServerStmt : public ResolvedStatement {
    StringPool::StringId server_name = StringPool::INVALID_ID;
    std::string server_type;
    std::string host;
    uint16_t port = 0;
    std::string options_json;
};

/**
 * Resolved DROP FOREIGN SERVER statement
 */
struct ResolvedDropForeignServerStmt : public ResolvedStatement {
    bool if_exists = false;
    bool cascade = false;
    StringPool::StringId server_name = StringPool::INVALID_ID;
};

/**
 * Resolved CREATE FOREIGN TABLE statement
 */
struct ResolvedCreateForeignTableStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId table_name = StringPool::INVALID_ID;
    bool if_not_exists = false;
    StringPool::StringId server_name = StringPool::INVALID_ID;
    std::string remote_schema;
    std::string remote_table;
    std::string column_mapping_json;
};

/**
 * Resolved DROP FOREIGN TABLE statement
 */
struct ResolvedDropForeignTableStmt : public ResolvedStatement {
    bool if_exists = false;
    bool cascade = false;
    std::vector<SchemaPath> tables;
};

/**
 * Resolved CREATE USER MAPPING statement
 */
struct ResolvedCreateUserMappingStmt : public ResolvedStatement {
    UserMappingTarget target = UserMappingTarget::USER_NAME;
    StringPool::StringId user_name = StringPool::INVALID_ID;
    StringPool::StringId server_name = StringPool::INVALID_ID;
    std::string remote_user;
    std::string remote_credentials;
};

/**
 * Resolved DROP USER MAPPING statement
 */
struct ResolvedDropUserMappingStmt : public ResolvedStatement {
    bool if_exists = false;
    UserMappingTarget target = UserMappingTarget::USER_NAME;
    StringPool::StringId user_name = StringPool::INVALID_ID;
    StringPool::StringId server_name = StringPool::INVALID_ID;
};

/**
 * Resolved CREATE SYNONYM statement
 */
struct ResolvedCreateSynonymStmt : public ResolvedStatement {
    bool is_public = false;
    SchemaPath synonym_path;
    DdlObjectType target_type = DdlObjectType::TABLE;
    SchemaPath target_path;
};

/**
 * Resolved DROP SYNONYM statement
 */
struct ResolvedDropSynonymStmt : public ResolvedStatement {
    bool if_exists = false;
    std::vector<SchemaPath> synonyms;
};

/**
 * Resolved CREATE UDR statement
 */
struct ResolvedCreateUdrStmt : public ResolvedStatement {
    ResolvedSchemaRef schema;
    StringPool::StringId udr_name = StringPool::INVALID_ID;
    UdrObjectType udr_type = UdrObjectType::FUNCTION;
    std::string library_path;
    std::string entry_point;
    std::string signature;
    bool has_signature = false;
};

/**
 * Resolved DROP UDR statement
 */
struct ResolvedDropUdrStmt : public ResolvedStatement {
    bool if_exists = false;
    bool cascade = false;
    std::vector<SchemaPath> udrs;
};

/**
 * Resolved CREATE DOMAIN statement
 */
struct ResolvedCreateDomainStmt : public ResolvedStatement {
    bool if_not_exists = false;
    bool is_type = false;
    SchemaPath domain_path;
    DomainKind domain_kind = DomainKind::BASIC;
    ResolvedType base_type;
    std::vector<ResolvedDomainRecordField> record_fields;
    std::vector<ResolvedDomainEnumValue> enum_values;
    ResolvedType set_element_type;
    std::vector<ResolvedType> variant_allowed_types;
    ResolvedRangeOptions range_options;
    ResolvedBaseOptions base_options;
    bool is_shell = false;
    bool nullable = true;
    std::string default_value;
    std::vector<ResolvedDomainConstraint> constraints;
    bool has_inherits = false;
    ID parent_domain_id{};
    bool has_collation = false;
    std::string collation_name;
    bool has_dialect = false;
    std::string dialect_tag;
    bool has_compat = false;
    std::string compat_name;
    bool enum_wrap = false;
    bool has_integrity = false;
    DomainIntegrityOptions integrity;
    bool has_security = false;
    DomainSecurityOptions security;
    bool has_validation = false;
    DomainValidationOptions validation;
    bool has_quality = false;
    DomainQualityOptions quality;
    bool has_comment = false;
    std::string comment;
};

/**
 * Resolved ALTER TYPE statement
 */
struct ResolvedAlterTypeStmt : public ResolvedStatement {
    AlterTypeAction action = AlterTypeAction::RENAME_TO;
    SchemaPath type_path;
    StringPool::StringId new_name = StringPool::INVALID_ID;
    StringPool::StringId new_schema = StringPool::INVALID_ID;
    StringPool::StringId value_label = StringPool::INVALID_ID;
    bool has_before = false;
    StringPool::StringId before_label = StringPool::INVALID_ID;
    bool has_after = false;
    StringPool::StringId after_label = StringPool::INVALID_ID;
    StringPool::StringId old_label = StringPool::INVALID_ID;
    StringPool::StringId new_label = StringPool::INVALID_ID;
    bool is_range_options = false;
    bool is_base_options = false;
    ResolvedRangeOptions range_options;
    ResolvedBaseOptions base_options;
};

/**
 * Resolved DROP TYPE statement
 */
struct ResolvedDropTypeStmt : public ResolvedStatement {
    bool if_exists = false;
    std::vector<SchemaPath> types;
    bool cascade = false;
    bool restrict = false;
};

/**
 * Resolved ALTER DOMAIN statement
 */
struct ResolvedAlterDomainStmt : public ResolvedStatement {
    AlterDomainAction action = AlterDomainAction::SET_DEFAULT;
    SchemaPath domain_path;
    std::string value;
    StringPool::StringId constraint_name = StringPool::INVALID_ID;
    StringPool::StringId new_name = StringPool::INVALID_ID;
};

/**
 * Resolved DROP DOMAIN statement
 */
struct ResolvedDropDomainStmt : public ResolvedStatement {
    bool if_exists = false;
    std::vector<SchemaPath> domains;
    bool restrict = false;
};

/**
 * Resolved DROP DATABASE statement
 */
struct ResolvedDropDatabaseStmt : public ResolvedStatement {
    bool if_exists = false;
    bool force = false;
    SchemaPath database_path;
};

/**
 * Resolved ALTER SCHEMA statement
 */
struct ResolvedAlterSchemaStmt : public ResolvedStatement {
    AlterSchemaAction action = AlterSchemaAction::RENAME;
    SchemaPath schema_path;
    StringPool::StringId new_name = StringPool::INVALID_ID;
    StringPool::StringId owner = StringPool::INVALID_ID;
    SchemaPath new_path;
};

/**
 * Resolved ALTER DATABASE statement
 */
struct ResolvedAlterDatabaseStmt : public ResolvedStatement {
    AlterDatabaseAction action = AlterDatabaseAction::RENAME;
    SchemaPath database_path;
    StringPool::StringId new_name = StringPool::INVALID_ID;
    StringPool::StringId owner = StringPool::INVALID_ID;
    std::string alias;
    std::vector<std::pair<std::string, std::string>> options;
};

/**
 * Resolved ALTER TABLE statement
 */
struct ResolvedAlterTableStmt : public ResolvedStatement {
    AlterTableAction action = AlterTableAction::ADD_COLUMN;
    bool if_exists = false;
    bool only = false;
    bool cascade = false;

    ID table_uuid;
    ID schema_uuid;
    StringPool::StringId table_name = StringPool::INVALID_ID;
    StringPool::StringId qualified_table_name = StringPool::INVALID_ID;

    // Column operations
    ResolvedColumnDef column_def;
    bool has_column_def = false;
    StringPool::StringId column_name = StringPool::INVALID_ID;
    ResolvedExpression* default_expr = nullptr;
    bool has_default_expr = false;
    int32_t position_1_based = 0;
    bool has_position = false;

    // Constraint operations
    ResolvedTableConstraint constraint;
    bool has_constraint = false;
    StringPool::StringId constraint_name = StringPool::INVALID_ID;

    // Tablespace operations
    StringPool::StringId tablespace_name = StringPool::INVALID_ID;
    bool tablespace_online = false;

    // Trigger operations
    StringPool::StringId trigger_name = StringPool::INVALID_ID;
    bool trigger_all = false;

    // Statistics/storage operations
    int32_t statistics_target = 0;
    bool has_statistics_target = false;
    StringPool::StringId storage_type = StringPool::INVALID_ID;

    // RLS operations
    uint8_t rls_action = 0;

    // Partition operations
    StringPool::StringId partition_name = StringPool::INVALID_ID;
    StringPool::StringId partition_bounds = StringPool::INVALID_ID;
    bool has_partition_bounds = false;

    // Inheritance operations
    StringPool::StringId inherit_parent = StringPool::INVALID_ID;
    bool has_inherit_parent = false;
};

/**
 * Resolved ALTER INDEX statement
 */
struct ResolvedAlterIndexStmt : public ResolvedStatement {
    SchemaPath index_path;
    bool active = true;
    AlterIndexAction action = AlterIndexAction::ACTIVE;
    IndexOptions options;
};

/**
 * Resolved RENAME OBJECT statement
 */
struct ResolvedRenameObjectStmt : public ResolvedStatement {
    DdlObjectType object_type = DdlObjectType::TABLE;
    bool if_exists = false;
    bool has_uuid = false;
    ID object_uuid;
    SchemaPath object_path;
    StringPool::StringId new_name = StringPool::INVALID_ID;
};

/**
 * Resolved MOVE OBJECT statement
 */
struct ResolvedMoveObjectStmt : public ResolvedStatement {
    DdlObjectType object_type = DdlObjectType::TABLE;
    bool if_exists = false;
    bool has_uuid = false;
    ID object_uuid;
    SchemaPath object_path;
    SchemaPath target_schema;
    bool has_new_name = false;
    StringPool::StringId new_name = StringPool::INVALID_ID;
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
        FUNCTION,
        PROCEDURE,
        TRIGGER,
        PACKAGE,
        ROLE,
        EXCEPTION,
    };

    ObjectType object_type;
    std::vector<SchemaPath> object_paths;
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
    std::vector<SchemaPath> table_paths; // Tables to truncate
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

/**
 * Resolved ANALYZE statement
 */
struct ResolvedAnalyzeStmt : public ResolvedStatement {
    SchemaPath table_path;
    StringPool::StringId column_name = StringPool::INVALID_ID;
    bool has_column = false;
    bool has_sample = false;
    double sample_rate = 0.0;
    bool verbose = false;
};

/**
 * Resolved SWEEP DATABASE statement
 */
struct ResolvedSweepDatabaseStmt : public ResolvedStatement {
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
    bool has_read_committed_mode = false;
    ReadCommittedMode read_committed_mode = ReadCommittedMode::DEFAULT;
    bool has_deferrable = false;
    bool deferrable = false;

    // Firebird legacy options
    bool has_wait_mode = false;
    TransactionWaitMode wait_mode = TransactionWaitMode::WAIT;
    bool has_lock_timeout = false;
    uint32_t lock_timeout_seconds = 0;
    std::vector<TableReservation> table_reservations;

    // ScratchBird extensions
    bool has_autocommit = false;
    AutocommitMode autocommit_mode = AutocommitMode::UNCHANGED;
    TransactionConflictAction conflict_action = TransactionConflictAction::DEFAULT;
    bool has_conflict_error_code = false;
    int32_t conflict_error_code = 0;
};

/**
 * Resolved PREPARE TRANSACTION (2PC)
 */
struct ResolvedPrepareTransactionStmt : public ResolvedStatement {
    StringPool::StringId gid = StringPool::INVALID_ID;
};

/**
 * Resolved COMMIT
 */
struct ResolvedCommitStmt : public ResolvedStatement {
    bool and_chain = false;
    bool and_no_chain = false;
    bool retaining = false;
    bool is_prepared = false;
    StringPool::StringId prepared_gid = StringPool::INVALID_ID;
};

/**
 * Resolved ROLLBACK
 */
struct ResolvedRollbackStmt : public ResolvedStatement {
    bool to_savepoint = false;
    StringPool::StringId savepoint_name = StringPool::INVALID_ID;
    bool and_chain = false;
    bool and_no_chain = false;
    bool retaining = false;
    bool is_prepared = false;
    StringPool::StringId prepared_gid = StringPool::INVALID_ID;
};

/**
 * Resolved SAVEPOINT
 */
struct ResolvedSavepointStmt : public ResolvedStatement {
    StringPool::StringId name;
};

/**
 * Resolved RELEASE SAVEPOINT
 */
struct ResolvedReleaseSavepointStmt : public ResolvedStatement {
    StringPool::StringId name;
};

/**
 * Resolved CONNECT statement
 */
struct ResolvedConnectStmt : public ResolvedStatement {
    StringPool::StringId database = StringPool::INVALID_ID;
    StringPool::StringId user = StringPool::INVALID_ID;
    bool has_password = false;
    std::string password;
    StringPool::StringId role = StringPool::INVALID_ID;
    StringPool::StringId charset = StringPool::INVALID_ID;
};

/**
 * Resolved DISCONNECT statement
 */
struct ResolvedDisconnectStmt : public ResolvedStatement {
    DisconnectStmt::Target target = DisconnectStmt::Target::CURRENT;
    StringPool::StringId connection_name = StringPool::INVALID_ID;
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
    bool has_read_committed_mode = false;
    ReadCommittedMode read_committed_mode = ReadCommittedMode::DEFAULT;
    bool has_deferrable = false;
    bool deferrable = false;

    // Firebird legacy options for SET TRANSACTION
    bool has_wait_mode = false;
    TransactionWaitMode wait_mode = TransactionWaitMode::WAIT;
    bool has_lock_timeout = false;
    uint32_t lock_timeout_seconds = 0;
    std::vector<TableReservation> table_reservations;

    // SET AUTOCOMMIT or SET TRANSACTION AUTOCOMMIT
    bool has_autocommit = false;
    AutocommitMode autocommit_mode = AutocommitMode::UNCHANGED;
    TransactionConflictAction conflict_action = TransactionConflictAction::DEFAULT;
    bool has_conflict_error_code = false;
    int32_t conflict_error_code = 0;

    // For SET SQL DIALECT
    uint8_t sql_dialect = 0;  // 1, 2, or 3 (0 = not set)

    // For SET LOCAL_TIMEOUT
    uint32_t local_timeout_seconds = 0;
};

/**
 * Resolved ALTER SYSTEM SET statement
 */
struct ResolvedAlterSystemStmt : public ResolvedStatement {
    StringPool::StringId variable_name = StringPool::INVALID_ID;
    ResolvedExpression* value = nullptr;
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
 *                  SHOW VERSION, SHOW DATABASE, SHOW SYSTEM
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

// =============================================================================
// Resolved DCL Statements
// =============================================================================

struct ResolvedGrantStmt : public ResolvedStatement {
    uint32_t privileges = 0;
    PrivilegeObjectType object_type = PrivilegeObjectType::TABLE;
    std::vector<SchemaPath> objects;
    std::vector<StringPool::StringId> grantees;
    bool with_grant_option = false;
    bool is_public = false;
};

struct ResolvedRevokeStmt : public ResolvedStatement {
    uint32_t privileges = 0;
    PrivilegeObjectType object_type = PrivilegeObjectType::TABLE;
    std::vector<SchemaPath> objects;
    std::vector<StringPool::StringId> grantees;
    bool grant_option_for = false;
    bool cascade = false;
    bool is_public = false;
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
