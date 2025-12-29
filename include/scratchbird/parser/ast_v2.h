#pragma once

/**
 * ScratchBird Parser v2.0 - AST Node Definitions
 *
 * This module defines the Abstract Syntax Tree nodes for the v2.0 parser.
 * These are "unresolved" AST nodes - they contain StringPool IDs and
 * SchemaPath references that will be resolved to UUIDs by the semantic analyzer.
 *
 * Design:
 * - All nodes derive from ASTNode base class
 * - Statements derive from Statement
 * - Expressions derive from Expression
 * - Memory managed by ASTArena (arena allocator)
 *
 * See: docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md Section 9
 */

#include "scratchbird/parser/lexer_v2.h"
#include "scratchbird/parser/schema_path_v2.h"
#include "scratchbird/parser/shared_types.h"
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <functional>
#include <type_traits>

namespace scratchbird::parser::v2 {

// Import shared types from parser namespace
using parser::JoinType;
using parser::WindowFunc;
using parser::FrameBoundaryType;
using parser::FrameMode;
using parser::FrameExclusion;
using parser::SubqueryType;
using parser::GroupingType;
using parser::SortOrder;
using parser::NullsOrder;

// Forward declarations
class Statement;
class Expression;
class ASTVisitor;

// =============================================================================
// AST Node Kinds
// =============================================================================

enum class ASTKind : uint16_t {
    // Statements - DDL
    CreateTableStmt,
    CreateIndexStmt,
    CreateViewStmt,
    CreateSequenceStmt,
    CreateFunctionStmt,
    CreateProcedureStmt,
    CreateTriggerStmt,
    CreateTypeStmt,
    CreateDomainStmt,
    AlterTableStmt,
    RenameObjectStmt,
    MoveObjectStmt,
    DropTableStmt,
    DropIndexStmt,
    DropViewStmt,
    TruncateTableStmt,

    // Statements - DML
    SelectStmt,
    InsertStmt,
    UpdateStmt,
    DeleteStmt,
    MergeStmt,

    // Statements - Transaction
    StartTransactionStmt,
    PrepareTransactionStmt,
    CommitStmt,
    RollbackStmt,
    SavepointStmt,
    ReleaseSavepointStmt,

    // Statements - Session
    SetStmt,
    ResetStmt,
    ShowStmt,
    ExplainStmt,

    // Statements - DCL (Data Control Language)
    GrantStmt,
    RevokeStmt,

    // Statements - Connection
    ConnectStmt,
    DisconnectStmt,

    // Statements - Metadata
    CommentStmt,

    // Statements - PSQL (Procedural SQL)
    ExecuteBlockStmt,
    CompoundStmt,       // BEGIN...END block
    DeclareVariableStmt,
    AssignmentStmt,
    IfStmt,
    WhileStmt,
    ForSelectStmt,
    LoopStmt,
    LeaveStmt,
    ContinueStmt,
    ExitStmt,
    SuspendStmt,
    ReturnStmt,
    ExceptionRaiseStmt,
    WhenExceptionStmt,
    PostEventStmt,
    DeclareCursorStmt,
    OpenCursorStmt,
    FetchCursorStmt,
    CloseCursorStmt,

    // Expressions
    LiteralExpr,
    ColumnRefExpr,
    BinaryExpr,
    UnaryExpr,
    FunctionCallExpr,
    CastExpr,
    CaseExpr,
    SubqueryExpr,
    ExistsExpr,
    InExpr,
    BetweenExpr,
    LikeExpr,
    IsNullExpr,
    ArrayExpr,

    // Other nodes
    ColumnDef,
    TableConstraint,
    TypeName,
    SelectItem,
    FromClause,
    JoinClause,
    WindowSpec,
    OrderByItem,
    GroupByClause,
};

// =============================================================================
// Base Classes
// =============================================================================

/**
 * Base class for all AST nodes
 */
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual ASTKind kind() const = 0;

    SourceSpan span;  // Source location for error reporting

protected:
    ASTNode() = default;
    explicit ASTNode(SourceSpan s) : span(s) {}
};

/**
 * Base class for all statements
 */
class Statement : public ASTNode {
public:
    virtual void accept(ASTVisitor& visitor) = 0;

protected:
    Statement() = default;
    explicit Statement(SourceSpan s) : ASTNode(s) {}
};

/**
 * Base class for all expressions
 */
class Expression : public ASTNode {
public:
    virtual void accept(ASTVisitor& visitor) = 0;

protected:
    Expression() = default;
    explicit Expression(SourceSpan s) : ASTNode(s) {}
};

// =============================================================================
// Type System
// =============================================================================

/**
 * SQL data type with optional parameters
 */
struct TypeName {
    StringPool::StringId name = StringPool::INVALID_ID;  // INT, VARCHAR, etc.

    // Type parameters (precision, scale, length)
    std::optional<int32_t> length;      // VARCHAR(100)
    std::optional<int32_t> precision;   // DECIMAL(10, 2)
    std::optional<int32_t> scale;       // DECIMAL(10, 2)

    // Array type
    bool is_array = false;
    std::optional<int32_t> array_size;  // INT[10] or INT[]

    // Type modifiers
    bool with_time_zone = false;        // TIMESTAMP WITH TIME ZONE

    SourceSpan span;
};

/**
 * Column constraint types
 */
enum class ConstraintType : uint8_t {
    NOT_NULL,
    NULL_ALLOWED,
    PRIMARY_KEY,
    UNIQUE,
    CHECK,
    DEFAULT,
    REFERENCES,        // Foreign key
    GENERATED,         // GENERATED ALWAYS AS
    COLLATE,
};

/**
 * Foreign key actions
 */
enum class ForeignKeyAction : uint8_t {
    NO_ACTION,
    RESTRICT,
    CASCADE,
    SET_NULL,
    SET_DEFAULT,
};

/**
 * Column constraint
 */
struct ColumnConstraint {
    ConstraintType type;
    StringPool::StringId name = StringPool::INVALID_ID;  // Optional constraint name

    // For CHECK constraint
    Expression* check_expr = nullptr;

    // For DEFAULT constraint
    Expression* default_expr = nullptr;

    // For REFERENCES (foreign key)
    SchemaPath ref_table;
    std::vector<StringPool::StringId> ref_columns;
    ForeignKeyAction on_delete = ForeignKeyAction::NO_ACTION;
    ForeignKeyAction on_update = ForeignKeyAction::NO_ACTION;

    // For COLLATE
    StringPool::StringId collation = StringPool::INVALID_ID;

    // For GENERATED
    bool generated_always = false;
    bool generated_as_identity = false;
    Expression* generated_expr = nullptr;

    SourceSpan span;
};

/**
 * Column definition in CREATE TABLE
 */
struct ColumnDef : public ASTNode {
    ASTKind kind() const override { return ASTKind::ColumnDef; }

    StringPool::StringId name = StringPool::INVALID_ID;
    TypeName type;
    std::vector<ColumnConstraint> constraints;

    // Computed column
    bool is_computed = false;
    Expression* computed_expr = nullptr;
    bool computed_stored = false;  // STORED vs VIRTUAL
};

/**
 * Table constraint types
 */
enum class TableConstraintType : uint8_t {
    PRIMARY_KEY,
    UNIQUE,
    FOREIGN_KEY,
    CHECK,
    EXCLUDE,  // PostgreSQL exclusion constraint
};

/**
 * Table-level constraint in CREATE TABLE
 */
struct TableConstraint : public ASTNode {
    ASTKind kind() const override { return ASTKind::TableConstraint; }

    TableConstraintType type;
    StringPool::StringId name = StringPool::INVALID_ID;  // Optional constraint name

    // Columns for PRIMARY KEY, UNIQUE, FOREIGN KEY
    std::vector<StringPool::StringId> columns;

    // For CHECK constraint
    Expression* check_expr = nullptr;

    // For FOREIGN KEY
    SchemaPath ref_table;
    std::vector<StringPool::StringId> ref_columns;
    ForeignKeyAction on_delete = ForeignKeyAction::NO_ACTION;
    ForeignKeyAction on_update = ForeignKeyAction::NO_ACTION;

    // Index options
    bool using_index = false;
    StringPool::StringId index_method = StringPool::INVALID_ID;  // BTREE, HASH, etc.
};

// =============================================================================
// DDL Statements
// =============================================================================

/**
 * CREATE TABLE statement
 */
class CreateTableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateTableStmt; }
    void accept(ASTVisitor& visitor) override;

    // Options
    bool or_replace = false;
    bool if_not_exists = false;
    bool temporary = false;
    bool unlogged = false;

    // Table path
    SchemaPath table_path;

    // Column definitions
    std::vector<ColumnDef*> columns;

    // Table constraints
    std::vector<TableConstraint*> constraints;

    // Storage options
    SchemaPath tablespace;
    bool has_tablespace = false;

    // Inheritance (PostgreSQL-style)
    std::vector<SchemaPath> inherits;

    // Partitioning
    bool is_partitioned = false;
    StringPool::StringId partition_by = StringPool::INVALID_ID;  // RANGE, LIST, HASH
    std::vector<StringPool::StringId> partition_columns;
};

/**
 * Index type
 */
enum class IndexType : uint8_t {
    BTREE,
    HASH,
    GIN,
    GIST,
    BRIN,
    BITMAP,
};

/**
 * Index column specification
 */
struct IndexColumn {
    StringPool::StringId column = StringPool::INVALID_ID;
    Expression* expr = nullptr;  // For expression indexes
    bool ascending = true;
    bool nulls_first = false;
    bool nulls_last = false;
    StringPool::StringId opclass = StringPool::INVALID_ID;  // Operator class
};

/**
 * CREATE INDEX statement
 */
class CreateIndexStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateIndexStmt; }
    void accept(ASTVisitor& visitor) override;

    bool unique = false;
    bool concurrent = false;
    bool if_not_exists = false;

    StringPool::StringId index_name = StringPool::INVALID_ID;
    SchemaPath table_path;

    IndexType index_type = IndexType::BTREE;
    std::vector<IndexColumn> columns;

    // Partial index
    Expression* where_clause = nullptr;

    // Include columns (covering index)
    std::vector<StringPool::StringId> include_columns;

    // Storage
    SchemaPath tablespace;
    bool has_tablespace = false;
};

/**
 * CREATE VIEW statement
 */
class CreateViewStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateViewStmt; }
    void accept(ASTVisitor& visitor) override;

    bool or_replace = false;
    bool temporary = false;
    bool materialized = false;
    bool if_not_exists = false;

    SchemaPath view_path;
    std::vector<StringPool::StringId> column_names;  // Optional explicit column names

    Statement* query = nullptr;  // SELECT statement

    // View options
    bool with_check_option = false;
    bool check_option_local = false;  // LOCAL vs CASCADED

    // For materialized views
    bool with_data = true;
    SchemaPath tablespace;
    bool has_tablespace = false;
};

/**
 * CREATE SEQUENCE statement
 */
class CreateSequenceStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateSequenceStmt; }
    void accept(ASTVisitor& visitor) override;

    bool or_replace = false;
    bool temporary = false;
    bool if_not_exists = false;

    SchemaPath sequence_path;

    // Sequence options
    std::optional<int64_t> start_with;
    std::optional<int64_t> increment_by;
    std::optional<int64_t> min_value;
    std::optional<int64_t> max_value;
    bool no_min_value = false;
    bool no_max_value = false;
    std::optional<int64_t> cache;
    bool cycle = false;

    // Owned by column
    SchemaPath owned_by_table;
    StringPool::StringId owned_by_column = StringPool::INVALID_ID;
    bool has_owned_by = false;
};

/**
 * DDL object types for rename/move statements.
 * Values align with core::CatalogManager::ObjectType.
 */
enum class DdlObjectType : uint8_t {
    SCHEMA = 0,
    TABLE = 1,
    COLUMN = 2,
    INDEX = 3,
    VIEW = 4,
    SEQUENCE = 5,
    CONSTRAINT = 6,
    TRIGGER = 7,
    PROCEDURE = 8,
    FUNCTION = 9,
    DOMAIN = 10,
    ROLE = 12,
    USER = 13,
    GROUP = 14,
    TABLESPACE = 15,
    DATABASE = 16,
    PACKAGE = 22,
    UDR = 23,
    EXCEPTION = 24,
    FOREIGN_TABLE = 32,
    SYNONYM = 38,
};

/**
 * ALTER TABLE action types
 */
enum class AlterTableAction : uint8_t {
    ADD_COLUMN,
    DROP_COLUMN,
    ALTER_COLUMN,
    RENAME_COLUMN,
    ADD_CONSTRAINT,
    DROP_CONSTRAINT,
    RENAME_CONSTRAINT,
    RENAME_TABLE,
    SET_TABLESPACE,
    SET_SCHEMA,
    ENABLE_RLS,
    DISABLE_RLS,
};

/**
 * ALTER TABLE statement
 */
class AlterTableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterTableStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    bool only = false;  // Only this table, not descendants

    SchemaPath table_path;

    // Action details
    AlterTableAction action;

    // For ADD/DROP/ALTER COLUMN
    ColumnDef* column = nullptr;
    StringPool::StringId column_name = StringPool::INVALID_ID;

    // For RENAME COLUMN
    StringPool::StringId new_name = StringPool::INVALID_ID;

    // For ADD/DROP CONSTRAINT
    TableConstraint* constraint = nullptr;
    StringPool::StringId constraint_name = StringPool::INVALID_ID;
    bool cascade = false;

    // For SET TABLESPACE
    SchemaPath tablespace;

    // For SET SCHEMA
    SchemaPath target_schema;
};

/**
 * RENAME OBJECT statement (generic)
 */
class RenameObjectStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::RenameObjectStmt; }
    void accept(ASTVisitor& visitor) override;

    DdlObjectType object_type = DdlObjectType::TABLE;
    bool if_exists = false;
    SchemaPath object_path;
    StringPool::StringId new_name = StringPool::INVALID_ID;
};

/**
 * MOVE OBJECT statement (generic)
 */
class MoveObjectStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::MoveObjectStmt; }
    void accept(ASTVisitor& visitor) override;

    DdlObjectType object_type = DdlObjectType::TABLE;
    bool if_exists = false;
    SchemaPath object_path;
    SchemaPath target_schema;
    bool has_new_name = false;
    StringPool::StringId new_name = StringPool::INVALID_ID;
};

/**
 * DROP TABLE statement
 */
class DropTableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropTableStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    std::vector<SchemaPath> tables;
    bool cascade = false;
    bool restrict = false;
};

/**
 * DROP INDEX statement
 */
class DropIndexStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropIndexStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    bool concurrent = false;
    std::vector<SchemaPath> indexes;
    bool cascade = false;
};

/**
 * DROP VIEW statement
 */
class DropViewStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropViewStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    bool materialized = false;
    std::vector<SchemaPath> views;
    bool cascade = false;
};

/**
 * TRUNCATE TABLE statement
 *
 * TRUNCATE performs a fast table truncation by starting a background
 * thread that deletes all rows and sweeps garbage (ASYNC mode, default).
 * SYNC mode blocks until the truncation is complete.
 */
class TruncateTableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::TruncateTableStmt; }
    void accept(ASTVisitor& visitor) override;

    std::vector<SchemaPath> tables;
    bool restart_identity = false;
    bool continue_identity = false;
    bool cascade = false;
    bool sync_mode = false;  // SYNC blocks, ASYNC (default) is non-blocking
};

/**
 * EXPLAIN statement
 *
 * Shows the execution plan for a query without executing it.
 */
class ExplainStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ExplainStmt; }
    void accept(ASTVisitor& visitor) override;

    Statement* query = nullptr;  // The statement to explain
    bool analyze = false;        // EXPLAIN ANALYZE (actually execute)
    bool verbose = false;        // VERBOSE output
    bool costs = true;           // Show cost estimates (default true)
    bool buffers = false;        // Show buffer usage
    bool timing = true;          // Show timing (with ANALYZE)
    bool format_json = false;    // JSON output format
    bool format_xml = false;     // XML output format
    bool format_yaml = false;    // YAML output format
};

// =============================================================================
// DCL Statements (Data Control Language)
// =============================================================================

/**
 * Privilege types for GRANT/REVOKE
 */
enum class PrivilegeType : uint8_t {
    SELECT,
    INSERT,
    UPDATE,
    DELETE,
    TRUNCATE,
    REFERENCES,
    TRIGGER,
    EXECUTE,
    USAGE,
    ALL
};

/**
 * Object types for GRANT/REVOKE
 */
enum class PrivilegeObjectType : uint8_t {
    TABLE,
    VIEW,
    SEQUENCE,
    FUNCTION,
    PROCEDURE,
    SCHEMA,
    DATABASE,
    ALL_TABLES_IN_SCHEMA,
    ALL_SEQUENCES_IN_SCHEMA,
    ALL_FUNCTIONS_IN_SCHEMA
};

/**
 * GRANT statement
 *
 * Grants privileges on database objects to users/roles.
 */
class GrantStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::GrantStmt; }
    void accept(ASTVisitor& visitor) override;

    std::vector<PrivilegeType> privileges;
    PrivilegeObjectType object_type = PrivilegeObjectType::TABLE;
    std::vector<SchemaPath> objects;          // Tables, views, etc.
    std::vector<StringPool::StringId> grantees;  // Users/roles
    bool with_grant_option = false;
    bool is_public = false;                   // GRANT ... TO PUBLIC
};

/**
 * REVOKE statement
 *
 * Revokes privileges from users/roles.
 */
class RevokeStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::RevokeStmt; }
    void accept(ASTVisitor& visitor) override;

    std::vector<PrivilegeType> privileges;
    PrivilegeObjectType object_type = PrivilegeObjectType::TABLE;
    std::vector<SchemaPath> objects;
    std::vector<StringPool::StringId> grantees;
    bool grant_option_for = false;  // REVOKE GRANT OPTION FOR
    bool cascade = false;
    bool is_public = false;
};

// =============================================================================
// Connection Statements
// =============================================================================

/**
 * CONNECT statement
 *
 * Connects to a database.
 */
class ConnectStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ConnectStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId database = StringPool::INVALID_ID;
    StringPool::StringId user = StringPool::INVALID_ID;
    StringPool::StringId password = StringPool::INVALID_ID;
    StringPool::StringId role = StringPool::INVALID_ID;
    StringPool::StringId charset = StringPool::INVALID_ID;
};

/**
 * DISCONNECT statement
 *
 * Disconnects from a database connection.
 */
class DisconnectStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DisconnectStmt; }
    void accept(ASTVisitor& visitor) override;

    enum class Target {
        ALL,
        CURRENT,
        NAMED
    };
    Target target = Target::CURRENT;
    StringPool::StringId connection_name = StringPool::INVALID_ID;
};

// =============================================================================
// Metadata Statements
// =============================================================================

/**
 * Comment object types
 */
enum class CommentObjectType : uint8_t {
    TABLE,
    COLUMN,
    INDEX,
    VIEW,
    SEQUENCE,
    FUNCTION,
    PROCEDURE,
    TRIGGER,
    SCHEMA,
    DATABASE,
    ROLE,
    CONSTRAINT
};

/**
 * COMMENT ON statement
 *
 * Sets a comment on a database object.
 */
class CommentStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CommentStmt; }
    void accept(ASTVisitor& visitor) override;

    CommentObjectType object_type = CommentObjectType::TABLE;
    SchemaPath object_path;                     // Object being commented
    StringPool::StringId column_name = StringPool::INVALID_ID;  // For COMMENT ON COLUMN
    StringPool::StringId comment_text = StringPool::INVALID_ID; // The comment (or INVALID for NULL)
    bool is_null = false;                       // COMMENT ON ... IS NULL (removes comment)
};

// =============================================================================
// MERGE Statement (SQL:2003 standard)
// =============================================================================

/**
 * MERGE statement
 *
 * Performs INSERT/UPDATE/DELETE based on matching condition.
 */
class MergeStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::MergeStmt; }
    void accept(ASTVisitor& visitor) override;

    SchemaPath target_table;
    StringPool::StringId target_alias = StringPool::INVALID_ID;

    // Source: can be table or subquery
    SchemaPath source_table;
    StringPool::StringId source_alias = StringPool::INVALID_ID;
    Statement* source_query = nullptr;  // For USING (subquery)

    Expression* on_condition = nullptr;

    // WHEN MATCHED THEN UPDATE
    struct WhenMatched {
        Expression* and_condition = nullptr;  // WHEN MATCHED AND condition
        std::vector<std::pair<StringPool::StringId, Expression*>> assignments;
        bool is_delete = false;  // WHEN MATCHED THEN DELETE
    };
    std::vector<WhenMatched> when_matched;

    // WHEN NOT MATCHED THEN INSERT
    struct WhenNotMatched {
        Expression* and_condition = nullptr;
        std::vector<StringPool::StringId> columns;
        std::vector<Expression*> values;
    };
    std::vector<WhenNotMatched> when_not_matched;

    // WHEN NOT MATCHED BY SOURCE (SQL Server extension)
    struct WhenNotMatchedBySource {
        Expression* and_condition = nullptr;
        std::vector<std::pair<StringPool::StringId, Expression*>> assignments;
        bool is_delete = false;
    };
    std::vector<WhenNotMatchedBySource> when_not_matched_by_source;
};

// =============================================================================
// PSQL Statements (Procedural SQL)
// =============================================================================

/**
 * Variable declaration for PSQL
 */
struct VariableDecl {
    StringPool::StringId name = StringPool::INVALID_ID;
    TypeName type;
    Expression* default_value = nullptr;
    bool not_null = false;
    bool is_cursor = false;
    Statement* cursor_query = nullptr;  // FOR cursor select statement
};

/**
 * EXECUTE BLOCK statement (anonymous block)
 */
class ExecuteBlockStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ExecuteBlockStmt; }
    void accept(ASTVisitor& visitor) override;

    // Input parameters
    std::vector<VariableDecl> input_params;

    // Output parameters (for EXECUTE BLOCK RETURNS)
    std::vector<VariableDecl> output_params;

    // Local variable declarations
    std::vector<VariableDecl> variables;

    // Body (compound statement)
    Statement* body = nullptr;
};

/**
 * Compound statement (BEGIN...END block)
 */
class CompoundStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CompoundStmt; }
    void accept(ASTVisitor& visitor) override;

    std::vector<Statement*> statements;

    // Exception handlers
    std::vector<Statement*> exception_handlers;  // WHEN...DO statements
};

/**
 * DECLARE VARIABLE statement
 */
class DeclareVariableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DeclareVariableStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId name = StringPool::INVALID_ID;
    TypeName type;
    Expression* default_value = nullptr;
    bool not_null = false;
};

/**
 * Assignment statement (variable := expression)
 */
class AssignmentStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AssignmentStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId variable = StringPool::INVALID_ID;
    Expression* value = nullptr;
};

/**
 * IF statement
 */
class IfStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::IfStmt; }
    void accept(ASTVisitor& visitor) override;

    Expression* condition = nullptr;
    Statement* then_branch = nullptr;
    Statement* else_branch = nullptr;  // Can be another IfStmt for ELSE IF
};

/**
 * WHILE statement
 */
class WhileStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::WhileStmt; }
    void accept(ASTVisitor& visitor) override;

    Expression* condition = nullptr;
    Statement* body = nullptr;
};

/**
 * FOR SELECT statement
 */
class ForSelectStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ForSelectStmt; }
    void accept(ASTVisitor& visitor) override;

    Statement* select_stmt = nullptr;
    std::vector<StringPool::StringId> into_variables;
    Statement* body = nullptr;
};

/**
 * Simple LOOP statement (exits via LEAVE or WHEN condition)
 */
class LoopStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::LoopStmt; }
    void accept(ASTVisitor& visitor) override;

    Statement* body = nullptr;
};

/**
 * LEAVE statement (break out of loop)
 */
class LeaveStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::LeaveStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId label = StringPool::INVALID_ID;  // Optional loop label
};

/**
 * CONTINUE statement (next iteration)
 */
class ContinueStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ContinueStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId label = StringPool::INVALID_ID;  // Optional loop label
};

/**
 * EXIT statement (exit procedure/function)
 */
class ExitStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ExitStmt; }
    void accept(ASTVisitor& visitor) override;
};

/**
 * SUSPEND statement (yield row in selectable procedure)
 */
class SuspendStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::SuspendStmt; }
    void accept(ASTVisitor& visitor) override;
};

/**
 * RETURN statement
 */
class ReturnStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ReturnStmt; }
    void accept(ASTVisitor& visitor) override;

    Expression* value = nullptr;  // Optional return value
};

/**
 * EXCEPTION statement (raise exception)
 */
class ExceptionRaiseStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ExceptionRaiseStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId exception_name = StringPool::INVALID_ID;
    Expression* message = nullptr;  // Optional custom message
};

/**
 * WHEN exception handler
 */
class WhenExceptionStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::WhenExceptionStmt; }
    void accept(ASTVisitor& visitor) override;

    enum class ExceptionType {
        ANY,           // WHEN ANY
        SQLCODE,       // WHEN SQLCODE value
        GDSCODE,       // WHEN GDSCODE value
        EXCEPTION      // WHEN EXCEPTION name
    };

    ExceptionType type = ExceptionType::ANY;
    int32_t sqlcode = 0;
    StringPool::StringId gdscode = StringPool::INVALID_ID;
    StringPool::StringId exception_name = StringPool::INVALID_ID;

    Statement* handler = nullptr;
};

/**
 * POST_EVENT statement
 */
class PostEventStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::PostEventStmt; }
    void accept(ASTVisitor& visitor) override;

    Expression* event_name = nullptr;  // String expression or variable
};

/**
 * DECLARE CURSOR statement
 */
class DeclareCursorStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DeclareCursorStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId cursor_name = StringPool::INVALID_ID;
    Statement* select_stmt = nullptr;
    bool scroll = false;  // SCROLL cursor
};

/**
 * OPEN cursor statement
 */
class OpenCursorStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::OpenCursorStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId cursor_name = StringPool::INVALID_ID;
};

/**
 * FETCH cursor statement
 */
class FetchCursorStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::FetchCursorStmt; }
    void accept(ASTVisitor& visitor) override;

    enum class Direction {
        NEXT,
        PRIOR,
        FIRST,
        LAST,
        ABSOLUTE,
        RELATIVE
    };

    StringPool::StringId cursor_name = StringPool::INVALID_ID;
    Direction direction = Direction::NEXT;
    Expression* offset = nullptr;  // For ABSOLUTE/RELATIVE
    std::vector<StringPool::StringId> into_variables;
};

/**
 * CLOSE cursor statement
 */
class CloseCursorStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CloseCursorStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId cursor_name = StringPool::INVALID_ID;
};

// =============================================================================
// Expression Nodes (Basic set for DDL)
// =============================================================================

/**
 * Literal value types
 */
enum class LiteralType : uint8_t {
    INTEGER,
    FLOAT,
    STRING,
    BLOB,
    BOOLEAN,
    NULL_VALUE,
    DEFAULT,  // For INSERT/UPDATE DEFAULT values
};

/**
 * Literal expression
 */
class LiteralExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::LiteralExpr; }
    void accept(ASTVisitor& visitor) override;

    LiteralType literal_type;

    union {
        int64_t int_value;
        double float_value;
        bool bool_value;
    };
    StringPool::StringId string_value = StringPool::INVALID_ID;  // For STRING/BLOB
};

/**
 * Column reference expression
 */
class ColumnRefExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::ColumnRefExpr; }
    void accept(ASTVisitor& visitor) override;

    ColumnRef column;
};

/**
 * Binary operator types
 */
enum class BinaryOp : uint8_t {
    // Arithmetic
    ADD, SUB, MUL, DIV, MOD, POWER,
    // Comparison
    EQ, NE, LT, LE, GT, GE,
    // Logical
    AND, OR,
    // String
    CONCAT,
    // Bitwise
    BIT_AND, BIT_OR, BIT_XOR, SHIFT_LEFT, SHIFT_RIGHT,
    // JSON
    JSON_EXTRACT, JSON_EXTRACT_TEXT,
    // Array
    ARRAY_CONTAINS, ARRAY_CONTAINED_BY, ARRAY_OVERLAP,
};

/**
 * Binary expression
 */
class BinaryExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::BinaryExpr; }
    void accept(ASTVisitor& visitor) override;

    BinaryOp op;
    Expression* left = nullptr;
    Expression* right = nullptr;
};

/**
 * Unary operator types
 */
enum class UnaryOp : uint8_t {
    NEGATE,     // -x
    NOT,        // NOT x
    BIT_NOT,    // ~x
    IS_NULL,    // x IS NULL
    IS_NOT_NULL,// x IS NOT NULL
};

/**
 * Unary expression
 */
class UnaryExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::UnaryExpr; }
    void accept(ASTVisitor& visitor) override;

    UnaryOp op;
    Expression* operand = nullptr;
};

/**
 * Function call expression
 */
class FunctionCallExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::FunctionCallExpr; }
    void accept(ASTVisitor& visitor) override;

    SchemaPath function_path;
    std::vector<Expression*> arguments;

    // Aggregate options
    bool distinct = false;
    Expression* filter = nullptr;  // FILTER (WHERE ...)

    // Window function
    bool is_window = false;
    // WindowSpec* window = nullptr;  // TODO: Add when implementing SELECT
};

/**
 * CAST expression
 */
class CastExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::CastExpr; }
    void accept(ASTVisitor& visitor) override;

    Expression* expr = nullptr;
    TypeName target_type;
};

// =============================================================================
// DML Supporting Structures
// =============================================================================

// JoinType is imported from shared_types.h via using declaration above

/**
 * Table reference - can be a table, subquery, or function call
 */
struct TableRefNode : public ASTNode {
    ASTKind kind() const override { return ASTKind::FromClause; }

    enum class Type : uint8_t {
        TABLE,      // Simple table reference
        SUBQUERY,   // (SELECT ...) AS alias
        FUNCTION,   // function(...) AS alias
        JOIN,       // Joined tables
    };

    Type ref_type = Type::TABLE;

    // For TABLE type
    SchemaPath table_path;

    // For SUBQUERY type
    Statement* subquery = nullptr;

    // For FUNCTION type
    FunctionCallExpr* function = nullptr;

    // Alias (optional for TABLE, required for SUBQUERY/FUNCTION)
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;

    // Column aliases for derived tables: (SELECT ...) AS t(a, b, c)
    std::vector<StringPool::StringId> column_aliases;
};

/**
 * Join clause
 */
struct JoinNode : public ASTNode {
    ASTKind kind() const override { return ASTKind::JoinClause; }

    JoinType join_type = JoinType::INNER;

    // Left side (can be table or another join)
    TableRefNode* left = nullptr;

    // Right side
    TableRefNode* right = nullptr;

    // Join condition: ON expr
    Expression* on_condition = nullptr;

    // Join condition: USING (col1, col2, ...)
    std::vector<StringPool::StringId> using_columns;
    bool has_using = false;
};

/**
 * SELECT item - what's being selected
 */
struct SelectItem : public ASTNode {
    ASTKind kind() const override { return ASTKind::SelectItem; }

    enum class Type : uint8_t {
        EXPRESSION,  // expr [AS alias]
        STAR,        // *
        TABLE_STAR,  // table.*
    };

    Type item_type = Type::EXPRESSION;

    // For EXPRESSION
    Expression* expr = nullptr;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;

    // For TABLE_STAR
    SchemaPath table_path;  // table.* or schema.table.*
};

/**
 * ORDER BY item
 */
struct OrderByItem : public ASTNode {
    ASTKind kind() const override { return ASTKind::OrderByItem; }

    Expression* expr = nullptr;
    bool ascending = true;
    bool nulls_first = false;
    bool nulls_last = false;
    bool has_nulls_spec = false;
};

/**
 * Window frame bound
 */
enum class FrameBoundType : uint8_t {
    UNBOUNDED_PRECEDING,
    UNBOUNDED_FOLLOWING,
    CURRENT_ROW,
    VALUE_PRECEDING,    // N PRECEDING
    VALUE_FOLLOWING,    // N FOLLOWING
};

/**
 * Window frame type
 */
enum class FrameType : uint8_t {
    ROWS,
    RANGE,
    GROUPS,
};

/**
 * Window specification for window functions
 */
struct WindowSpec : public ASTNode {
    ASTKind kind() const override { return ASTKind::WindowSpec; }

    // PARTITION BY
    std::vector<Expression*> partition_by;

    // ORDER BY
    std::vector<OrderByItem*> order_by;

    // Frame specification
    bool has_frame = false;
    FrameType frame_type = FrameType::RANGE;
    FrameBoundType frame_start = FrameBoundType::UNBOUNDED_PRECEDING;
    FrameBoundType frame_end = FrameBoundType::CURRENT_ROW;
    Expression* frame_start_value = nullptr;  // For VALUE_PRECEDING/FOLLOWING
    Expression* frame_end_value = nullptr;

    // Named window reference
    StringPool::StringId ref_name = StringPool::INVALID_ID;
    bool has_ref = false;
};

/**
 * Set operation types
 */
enum class SetOpType : uint8_t {
    NONE,
    UNION,
    INTERSECT,
    EXCEPT,
};

// =============================================================================
// DML Statements
// =============================================================================

// Forward declarations
class SelectStmt;

/**
 * SELECT statement
 */
class SelectStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::SelectStmt; }
    void accept(ASTVisitor& visitor) override;

    // SELECT [DISTINCT | ALL]
    bool distinct = false;
    bool all = false;

    // Select list
    std::vector<SelectItem*> items;

    // FROM clause
    TableRefNode* from = nullptr;
    std::vector<JoinNode*> joins;

    // WHERE clause
    Expression* where = nullptr;

    // GROUP BY clause
    std::vector<Expression*> group_by;

    // HAVING clause
    Expression* having = nullptr;

    // WINDOW definitions
    std::vector<std::pair<StringPool::StringId, WindowSpec*>> windows;

    // ORDER BY clause
    std::vector<OrderByItem*> order_by;

    // LIMIT/OFFSET
    Expression* limit = nullptr;
    Expression* offset = nullptr;

    // Set operations (UNION, INTERSECT, EXCEPT)
    SetOpType set_op = SetOpType::NONE;
    bool set_op_all = false;
    SelectStmt* set_op_right = nullptr;

    // FOR UPDATE/SHARE
    bool for_update = false;
    bool for_share = false;
    bool nowait = false;
    bool skip_locked = false;
};

/**
 * Common Table Expression (CTE)
 */
struct CTE {
    StringPool::StringId name = StringPool::INVALID_ID;
    std::vector<StringPool::StringId> column_names;  // Optional
    Statement* query = nullptr;
    bool materialized = false;
    bool not_materialized = false;
    bool recursive = false;
};

/**
 * WITH clause (Common Table Expressions)
 */
struct WithClause {
    bool recursive = false;
    std::vector<CTE> ctes;
};

/**
 * INSERT ... ON CONFLICT action
 */
enum class ConflictAction : uint8_t {
    NOTHING,    // DO NOTHING
    UPDATE,     // DO UPDATE SET ...
};

/**
 * ON CONFLICT clause for INSERT
 */
struct OnConflictClause {
    // Conflict target
    std::vector<StringPool::StringId> columns;      // (col1, col2)
    StringPool::StringId constraint_name = StringPool::INVALID_ID;  // ON CONSTRAINT name
    Expression* where_target = nullptr;             // WHERE for partial index

    // Action
    ConflictAction action = ConflictAction::NOTHING;

    // For DO UPDATE
    std::vector<std::pair<StringPool::StringId, Expression*>> set_items;
    Expression* where_action = nullptr;  // WHERE clause for DO UPDATE
};

/**
 * INSERT statement
 */
class InsertStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::InsertStmt; }
    void accept(ASTVisitor& visitor) override;

    // WITH clause
    WithClause* with = nullptr;

    // Target table
    SchemaPath table_path;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;

    // Column list (optional)
    std::vector<StringPool::StringId> columns;

    // Source
    enum class Source { VALUES, SELECT, DEFAULT };
    Source source = Source::VALUES;

    // For VALUES source
    std::vector<std::vector<Expression*>> values_rows;

    // For SELECT source
    SelectStmt* select_source = nullptr;

    // ON CONFLICT (UPSERT)
    OnConflictClause* on_conflict = nullptr;

    // RETURNING clause
    std::vector<SelectItem*> returning;
};

/**
 * UPDATE statement
 */
class UpdateStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::UpdateStmt; }
    void accept(ASTVisitor& visitor) override;

    // WITH clause
    WithClause* with = nullptr;

    // Target table
    SchemaPath table_path;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;

    // SET clause: column = expression pairs
    std::vector<std::pair<StringPool::StringId, Expression*>> set_items;

    // FROM clause (for UPDATE ... FROM ... syntax)
    TableRefNode* from = nullptr;
    std::vector<JoinNode*> joins;

    // WHERE clause
    Expression* where = nullptr;

    // RETURNING clause
    std::vector<SelectItem*> returning;
};

/**
 * DELETE statement
 */
class DeleteStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DeleteStmt; }
    void accept(ASTVisitor& visitor) override;

    // WITH clause
    WithClause* with = nullptr;

    // Target table
    SchemaPath table_path;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;

    // USING clause (for DELETE ... USING ... syntax)
    TableRefNode* using_clause = nullptr;
    std::vector<JoinNode*> using_joins;

    // WHERE clause
    Expression* where = nullptr;

    // RETURNING clause
    std::vector<SelectItem*> returning;
};

// =============================================================================
// Session & Transaction Statements
// =============================================================================

/**
 * Transaction isolation levels
 */
enum class IsolationLevel : uint8_t {
    READ_UNCOMMITTED,
    READ_COMMITTED,
    REPEATABLE_READ,
    SERIALIZABLE,
};

/**
 * Transaction wait mode (Firebird legacy)
 */
enum class TransactionWaitMode : uint8_t {
    NO_WAIT = 0,
    WAIT = 1,
};

/**
 * Transaction access mode
 */
enum class TransactionAccess : uint8_t {
    READ_WRITE,
    READ_ONLY,
};

/**
 * Read committed variants (Firebird legacy)
 */
enum class ReadCommittedMode : uint8_t {
    DEFAULT = 0,
    READ_CONSISTENCY = 1,
    RECORD_VERSION = 2,
    NO_RECORD_VERSION = 3,
};

/**
 * Transaction conflict action (ScratchBird extension)
 */
enum class TransactionConflictAction : uint8_t {
    DEFAULT = 0,
    COMMIT = 1,
    ROLLBACK = 2,
    ERROR = 3,
    KEEP = 4,
};

/**
 * Autocommit mode for transaction payloads
 */
enum class AutocommitMode : uint8_t {
    UNCHANGED = 0,
    ON = 1,
    OFF = 2,
};

/**
 * Table lock mode for RESERVING clause (Firebird legacy)
 */
enum class TableLockMode : uint8_t {
    SHARED = 0,
    PROTECTED = 1,
};

/**
 * Table reservation entry for RESERVING clause
 */
struct TableReservation {
    StringPool::StringId table_name = StringPool::INVALID_ID;
    TableLockMode lock_mode = TableLockMode::SHARED;
    bool for_write = false;

    TableReservation(StringPool::StringId name, TableLockMode mode, bool write)
        : table_name(name), lock_mode(mode), for_write(write) {}
};

/**
 * START TRANSACTION / BEGIN statement
 */
class StartTransactionStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::StartTransactionStmt; }
    void accept(ASTVisitor& visitor) override;

    // Transaction characteristics
    bool has_isolation_level = false;
    IsolationLevel isolation_level = IsolationLevel::READ_COMMITTED;

    bool has_access_mode = false;
    TransactionAccess access_mode = TransactionAccess::READ_WRITE;

    bool has_read_committed_mode = false;
    ReadCommittedMode read_committed_mode = ReadCommittedMode::DEFAULT;

    bool deferrable = false;
    bool not_deferrable = false;

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
 * PREPARE TRANSACTION statement (2PC)
 */
class PrepareTransactionStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::PrepareTransactionStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId gid = StringPool::INVALID_ID;
};

/**
 * COMMIT statement
 */
class CommitStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CommitStmt; }
    void accept(ASTVisitor& visitor) override;

    // COMMIT AND CHAIN (start new transaction)
    bool and_chain = false;
    // COMMIT AND NO CHAIN (default)
    bool and_no_chain = false;
    // COMMIT RETAINING (Firebird semantics)
    bool retaining = false;

    // COMMIT PREPARED
    bool is_prepared = false;
    StringPool::StringId prepared_gid = StringPool::INVALID_ID;
};

/**
 * ROLLBACK statement
 */
class RollbackStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::RollbackStmt; }
    void accept(ASTVisitor& visitor) override;

    // ROLLBACK TO SAVEPOINT name
    bool to_savepoint = false;
    StringPool::StringId savepoint_name = StringPool::INVALID_ID;

    // ROLLBACK AND CHAIN
    bool and_chain = false;
    bool and_no_chain = false;
    // ROLLBACK RETAINING (Firebird semantics)
    bool retaining = false;

    // ROLLBACK PREPARED
    bool is_prepared = false;
    StringPool::StringId prepared_gid = StringPool::INVALID_ID;
};

/**
 * SAVEPOINT statement
 */
class SavepointStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::SavepointStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId name = StringPool::INVALID_ID;
};

/**
 * RELEASE SAVEPOINT statement
 */
class ReleaseSavepointStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ReleaseSavepointStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId name = StringPool::INVALID_ID;
};

/**
 * SET statement for session variables
 *
 * Supports:
 * - SET name = value
 * - SET name TO value
 * - SET name TO DEFAULT
 * - SET SESSION name = value
 * - SET LOCAL name = value
 * - SET TIME ZONE 'timezone'
 * - SET TRANSACTION ...
 */
class SetStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::SetStmt; }
    void accept(ASTVisitor& visitor) override;

    // Scope
    enum class Scope : uint8_t {
        SESSION,    // Default - persists for session
        LOCAL,      // Only for current transaction
    };
    Scope scope = Scope::SESSION;

    // Special SET variants
    enum class SetType : uint8_t {
        VARIABLE,       // SET name = value
        TIME_ZONE,      // SET TIME ZONE ...
        TRANSACTION,    // SET TRANSACTION ...
        AUTOCOMMIT,     // SET AUTOCOMMIT ...
        SESSION_AUTHORIZATION,  // SET SESSION AUTHORIZATION ...
        ROLE,           // SET ROLE ...
        PARSER_VERSION, // SET PARSER VERSION 1|2
    };
    SetType set_type = SetType::VARIABLE;

    // Variable name (for VARIABLE type)
    StringPool::StringId name = StringPool::INVALID_ID;

    // Value (can be DEFAULT, expression, or identifier)
    bool is_default = false;
    Expression* value = nullptr;
    std::vector<Expression*> values;  // For multi-value settings

    // For SET TRANSACTION
    bool has_isolation_level = false;
    IsolationLevel isolation_level = IsolationLevel::READ_COMMITTED;
    bool has_access_mode = false;
    TransactionAccess access_mode = TransactionAccess::READ_WRITE;
    bool has_read_committed_mode = false;
    ReadCommittedMode read_committed_mode = ReadCommittedMode::DEFAULT;
    bool deferrable = false;
    bool not_deferrable = false;

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

    // For SET PARSER VERSION
    uint8_t parser_version = 0;  // 1 or 2 (0 = not set)
};

/**
 * RESET statement
 *
 * Supports:
 * - RESET name
 * - RESET ALL
 */
class ResetStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ResetStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId name = StringPool::INVALID_ID;
    bool reset_all = false;
};

/**
 * SHOW statement
 *
 * Supports:
 * - SHOW name (variable)
 * - SHOW ALL
 * - SHOW TRANSACTION ISOLATION LEVEL
 * - SHOW TABLES [FROM database] [LIKE 'pattern']
 * - SHOW DATABASES [LIKE 'pattern']
 * - SHOW COLUMNS FROM table [LIKE 'pattern']
 * - SHOW INDEXES FROM table
 * - SHOW CREATE TABLE table
 * - SHOW TABLE name (Firebird: detailed table info)
 * - SHOW INDEX name
 * - SHOW TRIGGER name
 * - SHOW VIEW name
 * - SHOW PROCEDURE name
 * - SHOW FUNCTION name
 * - SHOW DOMAIN name
 * - SHOW GENERATOR/SEQUENCE name
 * - SHOW SCHEMA [name]
 * - SHOW ROLE name
 * - SHOW GRANTS [FOR name]
 * - SHOW CHECKS table
 * - SHOW COLLATIONS [LIKE 'pattern']
 * - SHOW SQL DIALECT
 * - SHOW VERSION
 * - SHOW DATABASE
 * - SHOW SYSTEM
 */
class ShowStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ShowStmt; }
    void accept(ASTVisitor& visitor) override;

    enum class ShowType : uint8_t {
        // Session variables
        VARIABLE,           // SHOW name
        ALL,                // SHOW ALL
        TRANSACTION_ISOLATION_LEVEL,  // SHOW TRANSACTION ISOLATION LEVEL

        // Basic catalog queries (MySQL/PostgreSQL style)
        TABLES,             // SHOW TABLES [FROM db] [LIKE pattern]
        DATABASES,          // SHOW DATABASES [LIKE pattern]
        COLUMNS,            // SHOW COLUMNS FROM table [LIKE pattern]
        INDEXES,            // SHOW INDEXES FROM table
        CREATE_TABLE,       // SHOW CREATE TABLE table

        // Firebird ISQL style (detailed object info)
        TABLE,              // SHOW TABLE name
        INDEX,              // SHOW INDEX name
        TRIGGER,            // SHOW TRIGGER name
        VIEW,               // SHOW VIEW name
        PROCEDURE,          // SHOW PROCEDURE name
        FUNCTION,           // SHOW FUNCTION name
        DOMAIN,             // SHOW DOMAIN name
        GENERATOR,          // SHOW GENERATOR/SEQUENCE name
        SCHEMA,             // SHOW SCHEMA [name]
        ROLE,               // SHOW ROLE name
        GRANTS,             // SHOW GRANTS [FOR name]
        CHECKS,             // SHOW CHECKS table
        COLLATIONS,         // SHOW COLLATIONS [LIKE pattern]
        SQL_DIALECT,        // SHOW SQL DIALECT
        VERSION,            // SHOW VERSION
        DATABASE,           // SHOW DATABASE (current database info)
        SYSTEM,             // SHOW SYSTEM (system tables/info)
        PARSER_VERSION,     // SHOW PARSER VERSION
    };
    ShowType show_type = ShowType::VARIABLE;

    // Object name for commands that take one (TABLE, INDEX, TRIGGER, etc.)
    StringPool::StringId name = StringPool::INVALID_ID;

    // For SHOW TABLES FROM database, SHOW COLUMNS FROM table
    StringPool::StringId from_name = StringPool::INVALID_ID;

    // For LIKE pattern filtering
    StringPool::StringId like_pattern = StringPool::INVALID_ID;
};

// =============================================================================
// Additional Expression Nodes for DML
// =============================================================================

/**
 * CASE expression
 */
class CaseExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::CaseExpr; }
    void accept(ASTVisitor& visitor) override;

    // Simple CASE: CASE expr WHEN val THEN result ...
    Expression* operand = nullptr;  // nullptr for searched CASE

    // WHEN clauses
    struct WhenClause {
        Expression* when_expr;
        Expression* then_expr;
    };
    std::vector<WhenClause> when_clauses;

    // ELSE clause (optional)
    Expression* else_expr = nullptr;
};

/**
 * Subquery expression (scalar subquery)
 */
class SubqueryExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::SubqueryExpr; }
    void accept(ASTVisitor& visitor) override;

    SelectStmt* subquery = nullptr;
};

/**
 * EXISTS expression
 */
class ExistsExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::ExistsExpr; }
    void accept(ASTVisitor& visitor) override;

    bool negated = false;  // NOT EXISTS
    SelectStmt* subquery = nullptr;
};

/**
 * IN expression
 */
class InExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::InExpr; }
    void accept(ASTVisitor& visitor) override;

    Expression* expr = nullptr;
    bool negated = false;  // NOT IN

    // Either a list of values or a subquery
    std::vector<Expression*> values;
    SelectStmt* subquery = nullptr;
    bool has_subquery = false;
};

/**
 * BETWEEN expression
 */
class BetweenExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::BetweenExpr; }
    void accept(ASTVisitor& visitor) override;

    Expression* expr = nullptr;
    bool negated = false;  // NOT BETWEEN
    bool symmetric = false;  // BETWEEN SYMMETRIC
    Expression* low = nullptr;
    Expression* high = nullptr;
};

/**
 * LIKE/ILIKE expression
 */
class LikeExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::LikeExpr; }
    void accept(ASTVisitor& visitor) override;

    Expression* expr = nullptr;
    bool negated = false;  // NOT LIKE
    bool case_insensitive = false;  // ILIKE
    Expression* pattern = nullptr;
    Expression* escape = nullptr;  // ESCAPE 'x'
};

/**
 * IS NULL / IS NOT NULL expression
 */
class IsNullExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::IsNullExpr; }
    void accept(ASTVisitor& visitor) override;

    Expression* expr = nullptr;
    bool negated = false;  // IS NOT NULL
};

/**
 * Array expression [1, 2, 3] or ARRAY[...]
 */
class ArrayExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::ArrayExpr; }
    void accept(ASTVisitor& visitor) override;

    std::vector<Expression*> elements;

    // Subquery array: ARRAY(SELECT ...)
    SelectStmt* subquery = nullptr;
    bool has_subquery = false;
};

// =============================================================================
// AST Visitor
// =============================================================================

/**
 * Visitor interface for AST traversal
 */
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    // DDL statements
    virtual void visit(CreateTableStmt* stmt) = 0;
    virtual void visit(CreateIndexStmt* stmt) = 0;
    virtual void visit(CreateViewStmt* stmt) = 0;
    virtual void visit(CreateSequenceStmt* stmt) = 0;
    virtual void visit(AlterTableStmt* stmt) = 0;
    virtual void visit(RenameObjectStmt* stmt) = 0;
    virtual void visit(MoveObjectStmt* stmt) = 0;
    virtual void visit(DropTableStmt* stmt) = 0;
    virtual void visit(DropIndexStmt* stmt) = 0;
    virtual void visit(DropViewStmt* stmt) = 0;
    virtual void visit(TruncateTableStmt* stmt) = 0;

    // DML statements
    virtual void visit(SelectStmt* stmt) = 0;
    virtual void visit(InsertStmt* stmt) = 0;
    virtual void visit(UpdateStmt* stmt) = 0;
    virtual void visit(DeleteStmt* stmt) = 0;

    // Transaction statements
    virtual void visit(StartTransactionStmt* stmt) = 0;
    virtual void visit(PrepareTransactionStmt* stmt) = 0;
    virtual void visit(CommitStmt* stmt) = 0;
    virtual void visit(RollbackStmt* stmt) = 0;
    virtual void visit(SavepointStmt* stmt) = 0;
    virtual void visit(ReleaseSavepointStmt* stmt) = 0;

    // Session statements
    virtual void visit(SetStmt* stmt) = 0;
    virtual void visit(ResetStmt* stmt) = 0;
    virtual void visit(ShowStmt* stmt) = 0;
    virtual void visit(ExplainStmt* stmt) = 0;

    // DCL statements
    virtual void visit(GrantStmt* stmt) = 0;
    virtual void visit(RevokeStmt* stmt) = 0;

    // Connection statements
    virtual void visit(ConnectStmt* stmt) = 0;
    virtual void visit(DisconnectStmt* stmt) = 0;

    // Metadata statements
    virtual void visit(CommentStmt* stmt) = 0;

    // DML (additional)
    virtual void visit(MergeStmt* stmt) = 0;

    // PSQL statements
    virtual void visit(ExecuteBlockStmt* stmt) = 0;
    virtual void visit(CompoundStmt* stmt) = 0;
    virtual void visit(DeclareVariableStmt* stmt) = 0;
    virtual void visit(AssignmentStmt* stmt) = 0;
    virtual void visit(IfStmt* stmt) = 0;
    virtual void visit(WhileStmt* stmt) = 0;
    virtual void visit(ForSelectStmt* stmt) = 0;
    virtual void visit(LoopStmt* stmt) = 0;
    virtual void visit(LeaveStmt* stmt) = 0;
    virtual void visit(ContinueStmt* stmt) = 0;
    virtual void visit(ExitStmt* stmt) = 0;
    virtual void visit(SuspendStmt* stmt) = 0;
    virtual void visit(ReturnStmt* stmt) = 0;
    virtual void visit(ExceptionRaiseStmt* stmt) = 0;
    virtual void visit(WhenExceptionStmt* stmt) = 0;
    virtual void visit(PostEventStmt* stmt) = 0;
    virtual void visit(DeclareCursorStmt* stmt) = 0;
    virtual void visit(OpenCursorStmt* stmt) = 0;
    virtual void visit(FetchCursorStmt* stmt) = 0;
    virtual void visit(CloseCursorStmt* stmt) = 0;

    // Expressions
    virtual void visit(LiteralExpr* expr) = 0;
    virtual void visit(ColumnRefExpr* expr) = 0;
    virtual void visit(BinaryExpr* expr) = 0;
    virtual void visit(UnaryExpr* expr) = 0;
    virtual void visit(FunctionCallExpr* expr) = 0;
    virtual void visit(CastExpr* expr) = 0;
    virtual void visit(CaseExpr* expr) = 0;
    virtual void visit(SubqueryExpr* expr) = 0;
    virtual void visit(ExistsExpr* expr) = 0;
    virtual void visit(InExpr* expr) = 0;
    virtual void visit(BetweenExpr* expr) = 0;
    virtual void visit(LikeExpr* expr) = 0;
    virtual void visit(IsNullExpr* expr) = 0;
    virtual void visit(ArrayExpr* expr) = 0;
};

// =============================================================================
// AST Arena (Memory Management)
// =============================================================================

/**
 * Arena allocator for AST nodes
 *
 * All AST nodes are allocated from the arena and freed together
 * when parsing is complete. This avoids individual allocations
 * and simplifies memory management.
 *
 * IMPORTANT: The arena tracks destructors for objects that contain
 * heap-allocated members (like std::vector, std::string). These
 * destructors are called in reverse order when the arena is destroyed.
 */
class ASTArena {
public:
    ASTArena(size_t block_size = 64 * 1024);  // 64KB blocks
    ~ASTArena();

    // Non-copyable, non-movable
    ASTArena(const ASTArena&) = delete;
    ASTArena& operator=(const ASTArena&) = delete;

    /**
     * Allocate a new AST node
     *
     * The node's destructor will be tracked and called when the arena
     * is destroyed, ensuring proper cleanup of any heap-allocated
     * members (std::vector, std::string, etc.).
     */
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        T* obj = new (mem) T(std::forward<Args>(args)...);
        // Track destructor for types that need cleanup
        // (those with non-trivial destructors)
        if constexpr (!std::is_trivially_destructible_v<T>) {
            trackDestructor([obj]() { obj->~T(); });
        }
        return obj;
    }

    /**
     * Allocate raw memory (no destructor tracking)
     */
    void* allocate(size_t size, size_t alignment);

    /**
     * Track a destructor to be called when arena is destroyed
     */
    void trackDestructor(std::function<void()> dtor);

    /**
     * Reset arena (call destructors and free all allocations)
     */
    void reset();

    /**
     * Get total allocated bytes
     */
    size_t totalAllocated() const { return total_allocated_; }

    /**
     * Get number of tracked destructors
     */
    size_t destructorCount() const { return destructors_.size(); }

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

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Convert ASTKind to string for debugging
 */
const char* astKindToString(ASTKind kind);

/**
 * Convert BinaryOp to string
 */
const char* binaryOpToString(BinaryOp op);

/**
 * Convert UnaryOp to string
 */
const char* unaryOpToString(UnaryOp op);

} // namespace scratchbird::parser::v2
