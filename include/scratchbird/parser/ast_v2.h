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
struct WindowSpec;
class SelectStmt;
class CreatePolicyStmt;
class AlterPolicyStmt;
class DropPolicyStmt;

// =============================================================================
// AST Node Kinds
// =============================================================================

enum class ASTKind : uint16_t {
    // Statements - DDL
    CreateTableStmt,
    CreateIndexStmt,
    CreateViewStmt,
    CreateSequenceStmt,
    AlterSequenceStmt,
    CreateSchemaStmt,
    DropSchemaStmt,
    AlterSchemaStmt,
    CreateDatabaseStmt,
    CreateTablespaceStmt,
    AlterTablespaceStmt,
    DropTablespaceStmt,
    AttachTablespaceStmt,
    DetachTablespaceStmt,
    DropDatabaseStmt,
    AlterDatabaseStmt,
    CreateFunctionStmt,
    CreateProcedureStmt,
    CreateTriggerStmt,
    CreatePackageStmt,
    CreateUserStmt,
    CreateRoleStmt,
    CreateGroupStmt,
    CreateExceptionStmt,
    CreateJobStmt,
    CreateTypeStmt,
    CreateDomainStmt,
    CreateForeignServerStmt,
    CreateForeignTableStmt,
    CreateUserMappingStmt,
    CreateSynonymStmt,
    CreateUdrStmt,
    AlterTypeStmt,
    DropTypeStmt,
    AlterDomainStmt,
    DropDomainStmt,
    AlterTableStmt,
    AlterIndexStmt,
    RenameObjectStmt,
    MoveObjectStmt,
    DropTableStmt,
    DropIndexStmt,
    DropViewStmt,
    DropSequenceStmt,
    DropFunctionStmt,
    DropProcedureStmt,
    DropTriggerStmt,
    DropPackageStmt,
    DropRoleStmt,
    DropGroupStmt,
    DropExceptionStmt,
    DropForeignServerStmt,
    DropForeignTableStmt,
    DropUserMappingStmt,
    DropSynonymStmt,
    DropUdrStmt,
    DropJobStmt,
    TruncateTableStmt,

    // Statements - DML
    SelectStmt,
    InsertStmt,
    UpdateStmt,
    DeleteStmt,
    CopyStmt,
    MergeStmt,
    ExecuteProcedureStmt,
    ExecuteStatementStmt,

    // Statements - Transaction
    StartTransactionStmt,
    PrepareTransactionStmt,
    CommitStmt,
    RollbackStmt,
    SavepointStmt,
    ReleaseSavepointStmt,

    // Statements - Session
    SetStmt,
    AlterSystemStmt,
    ResetStmt,
    ShowStmt,
    ExplainStmt,
    AnalyzeStmt,
    SweepDatabaseStmt,
    AlterJobStmt,
    ExecuteJobStmt,
    CancelJobRunStmt,

    // Statements - DCL (Data Control Language)
    GrantStmt,
    RevokeStmt,
    CreatePolicyStmt,
    AlterPolicyStmt,
    DropPolicyStmt,

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
    ForExecuteStmt,
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
    ParameterExpr,
    BinaryExpr,
    UnaryExpr,
    FunctionCallExpr,
    CastExpr,
    ExtractExpr,
    AlterElementExpr,
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
    bool has_schema_path = false;
    SchemaPath schema_path;

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

/**
 * Domain constraint types (CREATE DOMAIN)
 */
enum class DomainConstraintType : uint8_t {
    NOT_NULL = 0,
    NULL_ALLOWED = 1,
    DEFAULT = 2,
    CHECK = 3,
};

/**
 * Domain kinds (CREATE DOMAIN)
 */
enum class DomainKind : uint8_t {
    BASIC = 0,
    RECORD = 1,
    ENUM = 2,
    SET = 3,
    VARIANT = 4,
    RANGE = 5,
    BASE = 6,
    SHELL = 7,
};

/**
 * Domain-level constraint
 */
struct DomainConstraint {
    DomainConstraintType type = DomainConstraintType::CHECK;
    StringPool::StringId name = StringPool::INVALID_ID;  // Optional constraint name
    std::string expression;  // Raw expression text for DEFAULT/CHECK
    SourceSpan span;
};

/**
 * Domain RECORD field definition
 */
struct DomainRecordField {
    StringPool::StringId name = StringPool::INVALID_ID;
    TypeName type;
    bool nullable = true;
    bool has_default = false;
    std::string default_value;
    SourceSpan span;
};

/**
 * Domain ENUM value definition
 */
struct DomainEnumValue {
    StringPool::StringId label = StringPool::INVALID_ID;
    bool has_position = false;
    int32_t position = 0;
    SourceSpan span;
};

enum class TypeKind : uint8_t {
    ENUM = 0,
    RECORD = 1,
    RANGE = 2,
    BASE = 3,
    SHELL = 4,
};

enum class BaseTypeAlignment : uint8_t {
    CHAR = 0,
    SHORT = 1,
    INT = 2,
    DOUBLE = 3,
};

enum class BaseTypeStorageMode : uint8_t {
    PLAIN = 0,
    EXTERNAL = 1,
    EXTENDED = 2,
    MAIN = 3,
};

struct RangeTypeOptions {
    TypeName subtype;
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

struct BaseTypeOptions {
    TypeName storage;
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
 * Domain integrity options (WITH INTEGRITY)
 */
struct DomainIntegrityOptions {
    bool has_uniqueness = false;
    bool uniqueness = false;
    bool normalization_enabled = false;
    std::string normalization_function;
};

/**
 * Domain security options (WITH SECURITY)
 */
struct DomainSecurityOptions {
    bool has_masking = false;
    std::string masking;
    bool has_mask_pattern = false;
    std::string mask_pattern;
    bool has_encryption = false;
    std::string encryption;
    bool has_audit_access = false;
    bool audit_access = false;
    bool has_required_privilege = false;
    std::string required_privilege;
};

/**
 * Domain validation options (WITH VALIDATION)
 */
struct DomainValidationOptions {
    bool has_function = false;
    std::string function;
    bool has_error_message = false;
    std::string error_message;
};

/**
 * Domain quality options (WITH QUALITY)
 */
struct DomainQualityOptions {
    bool has_parse_function = false;
    std::string parse_function;
    bool has_standardize_function = false;
    std::string standardize_function;
    bool has_enrich_function = false;
    std::string enrich_function;
};

/**
 * CREATE DATABASE option
 */
struct DatabaseOption {
    StringPool::StringId key = StringPool::INVALID_ID;
    StringPool::StringId value = StringPool::INVALID_ID;
};

// =============================================================================
// DDL Statements
// =============================================================================

// Temporary table type (scratchbird normalized)
enum class TempTableType : uint8_t {
    NONE = 0,
    SESSION = 1,
    TRANSACTION = 2,
    GLOBAL = 3
};

// ON COMMIT action for temporary tables
enum class TempOnCommitAction : uint8_t {
    NONE = 0,
    DELETE_ROWS = 1,
    PRESERVE_ROWS = 2,
    DROP = 3
};

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
    TempTableType temp_type = TempTableType::NONE;
    TempOnCommitAction on_commit = TempOnCommitAction::NONE;
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

    // CREATE TABLE AS SELECT
    SelectStmt* as_query = nullptr;
};

/**
 * Index type
 */
enum class IndexType : uint8_t {
    BTREE,
    HASH,
    GIN,
    GIST,
    SPGIST,
    BRIN,
    RTREE,
    HNSW,
    BITMAP,
    COLUMNSTORE,
    LSM,
    FULLTEXT,
    IVF,
    ZONEMAP,
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
 * Index options
 */
struct IndexOptions {
    bool bloom_filter_set = false;
    bool bloom_filter_enabled = false;
    bool bloom_fpr_set = false;
    double bloom_fpr = 0.01;
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
    IndexOptions options;

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
 * ALTER SEQUENCE statement
 */
class AlterSequenceStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterSequenceStmt; }
    void accept(ASTVisitor& visitor) override;

    SchemaPath sequence_path;

    std::optional<int64_t> increment_by;
    std::optional<int64_t> min_value;
    std::optional<int64_t> max_value;
    std::optional<int64_t> restart_with;
    std::optional<int64_t> cache;
    std::optional<bool> cycle;
};

/**
 * Routine parameter definition (procedures/functions)
 */
enum class RoutineParamMode : uint8_t {
    IN = 0,
    OUT = 1,
    INOUT = 2
};

struct RoutineParam {
    RoutineParamMode mode = RoutineParamMode::IN;
    StringPool::StringId name = StringPool::INVALID_ID;
    TypeName type;
    Expression* default_value = nullptr;
    bool has_default = false;
};

enum class RoutineSqlSecurity : uint8_t {
    INVOKER = 0,
    DEFINER = 1
};

/**
 * CREATE FUNCTION statement
 */
class CreateFunctionStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateFunctionStmt; }
    void accept(ASTVisitor& visitor) override;

    bool or_replace = false;
    bool deterministic = false;
    RoutineSqlSecurity sql_security = RoutineSqlSecurity::INVOKER;

    SchemaPath function_path;
    std::vector<RoutineParam> params;
    TypeName return_type;
    StringPool::StringId body = StringPool::INVALID_ID;
};

/**
 * CREATE PROCEDURE statement
 */
class CreateProcedureStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateProcedureStmt; }
    void accept(ASTVisitor& visitor) override;

    bool or_replace = false;
    RoutineSqlSecurity sql_security = RoutineSqlSecurity::INVOKER;

    SchemaPath procedure_path;
    std::vector<RoutineParam> params;
    StringPool::StringId body = StringPool::INVALID_ID;
};

enum class TriggerTiming : uint8_t {
    BEFORE = 0,
    AFTER = 1,
    INSTEAD_OF = 2
};

enum class TriggerEvent : uint8_t {
    INSERT = 0,
    UPDATE = 1,
    DELETE = 2
};

enum class TriggerGranularity : uint8_t {
    FOR_EACH_ROW = 0,
    FOR_EACH_STATEMENT = 1
};

/**
 * CREATE TRIGGER statement
 */
class CreateTriggerStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateTriggerStmt; }
    void accept(ASTVisitor& visitor) override;

    bool or_replace = false;
    bool active = true;

    StringPool::StringId trigger_name = StringPool::INVALID_ID;
    SchemaPath table_path;

    TriggerTiming timing = TriggerTiming::BEFORE;
    uint8_t event_mask = 1u << static_cast<uint8_t>(TriggerEvent::INSERT);
    TriggerGranularity granularity = TriggerGranularity::FOR_EACH_ROW;

    StringPool::StringId body = StringPool::INVALID_ID;
};

/**
 * CREATE PACKAGE statement
 */
class CreatePackageStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreatePackageStmt; }
    void accept(ASTVisitor& visitor) override;

    bool or_replace = false;
    bool is_body = false;  // CREATE PACKAGE BODY

    SchemaPath package_path;
    StringPool::StringId header = StringPool::INVALID_ID;
    StringPool::StringId body = StringPool::INVALID_ID;
};

/**
 * CREATE USER statement
 */
class CreateUserStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateUserStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId user_name = StringPool::INVALID_ID;
    bool has_password = false;
    StringPool::StringId password = StringPool::INVALID_ID;
    bool is_superuser = false;
};

/**
 * CREATE ROLE statement
 */
class CreateRoleStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateRoleStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId role_name = StringPool::INVALID_ID;
};

/**
 * CREATE GROUP statement
 */
class CreateGroupStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateGroupStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId group_name = StringPool::INVALID_ID;
};

/**
 * Policy type for RLS policies
 */
enum class PolicyType : uint8_t {
    ALL = 0,
    SELECT = 1,
    INSERT = 2,
    UPDATE = 3,
    DELETE = 4
};

/**
 * CREATE POLICY statement for Row-Level Security
 */
class CreatePolicyStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreatePolicyStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId policy_name = StringPool::INVALID_ID;
    SchemaPath table_path;
    PolicyType policy_type = PolicyType::ALL;
    bool is_permissive = true;  // true = PERMISSIVE, false = RESTRICTIVE
    std::vector<StringPool::StringId> roles;  // Empty = PUBLIC
    Expression* using_expr = nullptr;  // USING expression (for SELECT, UPDATE, DELETE)
    Expression* with_check_expr = nullptr;  // WITH CHECK expression (for INSERT, UPDATE)
};

/**
 * ALTER POLICY statement
 */
class AlterPolicyStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterPolicyStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId policy_name = StringPool::INVALID_ID;
    SchemaPath table_path;
    std::vector<StringPool::StringId> roles;  // Empty = no change
    Expression* using_expr = nullptr;  // nullptr = no change
    Expression* with_check_expr = nullptr;  // nullptr = no change
};

/**
 * DROP POLICY statement
 */
class DropPolicyStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropPolicyStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId policy_name = StringPool::INVALID_ID;
    SchemaPath table_path;
    bool if_exists = false;
};

struct OptionPair {
    std::string key;
    std::string value;
};

struct ForeignColumnDef {
    StringPool::StringId name = StringPool::INVALID_ID;
    TypeName type;
    std::string type_text;
    std::vector<OptionPair> options;
};

/**
 * CREATE SERVER statement (FDW)
 */
class CreateForeignServerStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateForeignServerStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId server_name = StringPool::INVALID_ID;
    bool has_server_type = false;
    std::string server_type;
    bool has_server_version = false;
    std::string server_version;
    StringPool::StringId fdw_name = StringPool::INVALID_ID;
    std::vector<OptionPair> options;
};

/**
 * CREATE FOREIGN TABLE statement
 */
class CreateForeignTableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateForeignTableStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_not_exists = false;
    SchemaPath table_path;
    std::vector<ForeignColumnDef> columns;
    StringPool::StringId server_name = StringPool::INVALID_ID;
    std::vector<OptionPair> options;
};

/**
 * CREATE USER MAPPING statement
 */
enum class UserMappingTarget : uint8_t {
    USER_NAME = 0,
    CURRENT_USER = 1,
    SESSION_USER = 2,
    PUBLIC_ROLE = 3,
};

class CreateUserMappingStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateUserMappingStmt; }
    void accept(ASTVisitor& visitor) override;

    UserMappingTarget target = UserMappingTarget::USER_NAME;
    StringPool::StringId user_name = StringPool::INVALID_ID;
    StringPool::StringId server_name = StringPool::INVALID_ID;
    std::vector<OptionPair> options;
};

/**
 * CREATE SYNONYM statement
 */
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
    COMPOSITE_TYPE = 11,
    ROLE = 12,
    USER = 13,
    GROUP = 14,
    TABLESPACE = 15,
    DATABASE = 16,
    PACKAGE = 22,
    UDR = 23,
    EXCEPTION = 24,
    FOREIGN_SERVER = 31,
    FOREIGN_TABLE = 32,
    USER_MAPPING = 33,
    SYNONYM = 38,
};

class CreateSynonymStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateSynonymStmt; }
    void accept(ASTVisitor& visitor) override;

    bool is_public = false;
    SchemaPath synonym_path;
    DdlObjectType target_type = DdlObjectType::TABLE;
    SchemaPath target_path;
};

/**
 * CREATE UDR statement
 */
enum class UdrObjectType : uint8_t {
    FUNCTION = 0,
    PROCEDURE = 1,
    TRIGGER = 2,
};

class CreateUdrStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateUdrStmt; }
    void accept(ASTVisitor& visitor) override;

    SchemaPath udr_path;
    UdrObjectType udr_type = UdrObjectType::FUNCTION;
    std::string library_path;
    std::string entry_point;
    std::string signature;
    bool has_signature = false;
};

/**
 * CREATE EXCEPTION statement
 */
class CreateExceptionStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateExceptionStmt; }
    void accept(ASTVisitor& visitor) override;

    bool or_replace = false;
    SchemaPath exception_path;
    StringPool::StringId message = StringPool::INVALID_ID;
};

// =============================================================================
// Scheduler Job Statements
// =============================================================================

enum class JobType : uint8_t {
    SQL = 0,
    PROCEDURE = 1,
    EXTERNAL = 2
};

enum class JobScheduleKind : uint8_t {
    CRON = 0,
    AT = 1,
    EVERY = 2
};

enum class JobState : uint8_t {
    ENABLED = 0,
    DISABLED = 1,
    PAUSED = 2
};

enum class JobOnCompletion : uint8_t {
    PRESERVE = 0,
    DROP = 1
};

/**
 * CREATE JOB statement
 */
class CreateJobStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateJobStmt; }
    void accept(ASTVisitor& visitor) override;

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
 * ALTER JOB statement
 */
class AlterJobStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterJobStmt; }
    void accept(ASTVisitor& visitor) override;

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
 * DROP JOB statement
 */
class DropJobStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropJobStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId job_name = StringPool::INVALID_ID;
    bool keep_history = false;
};

/**
 * EXECUTE JOB statement
 */
class ExecuteJobStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ExecuteJobStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId job_name = StringPool::INVALID_ID;
};

/**
 * CANCEL JOB RUN statement
 */
class CancelJobRunStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CancelJobRunStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId job_run_uuid = StringPool::INVALID_ID;
};

/**
 * CREATE SCHEMA statement
 */
class CreateSchemaStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateSchemaStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_not_exists = false;
    SchemaPath schema_path;
    bool has_owner = false;
    StringPool::StringId owner = StringPool::INVALID_ID;
};

/**
 * DROP SCHEMA statement
 */
class DropSchemaStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropSchemaStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    std::vector<SchemaPath> schemas;
    bool cascade = false;
    bool restrict = false;
};

/**
 * CREATE DATABASE statement
 */
class CreateDatabaseStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateDatabaseStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_not_exists = false;
    SchemaPath database_path;
    StringPool::StringId source_spec = StringPool::INVALID_ID;  // Original spec/path
    std::vector<DatabaseOption> options;
    std::vector<StringPool::StringId> aliases;
};

/**
 * CREATE TABLESPACE statement
 */
class CreateTablespaceStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateTablespaceStmt; }
    void accept(ASTVisitor& visitor) override;

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

enum class TablespaceAlterAction : uint8_t {
    SET_AUTOEXTEND = 0,
    SET_AUTOEXTEND_SIZE = 1,
    SET_MAXSIZE = 2,
    RENAME_TO = 3,
};

struct TablespaceAlteration {
    TablespaceAlterAction action = TablespaceAlterAction::SET_AUTOEXTEND;
    bool autoextend_enabled = false;
    uint32_t size_mb = 0;
    StringPool::StringId new_name = StringPool::INVALID_ID;
};

/**
 * ALTER TABLESPACE statement
 */
class AlterTablespaceStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterTablespaceStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId tablespace_name = StringPool::INVALID_ID;
    std::vector<TablespaceAlteration> alterations;
};

/**
 * DROP TABLESPACE statement
 */
class DropTablespaceStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropTablespaceStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId tablespace_name = StringPool::INVALID_ID;
    bool force = false;
};

/**
 * ALTER TABLESPACE ... ATTACH statement
 */
class AttachTablespaceStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AttachTablespaceStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId tablespace_name = StringPool::INVALID_ID;
    std::string location;
    bool validate = false;
    bool allow_mismatch = false;
};

/**
 * ALTER TABLESPACE ... DETACH statement
 */
class DetachTablespaceStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DetachTablespaceStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId tablespace_name = StringPool::INVALID_ID;
    bool force = false;
};

/**
 * CREATE DOMAIN statement (basic domains)
 */
class CreateDomainStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateDomainStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_not_exists = false;
    SchemaPath domain_path;
    DomainKind domain_kind = DomainKind::BASIC;
    TypeName base_type;
    std::vector<DomainRecordField> record_fields;
    std::vector<DomainEnumValue> enum_values;
    TypeName set_element_type;
    std::vector<TypeName> variant_allowed_types;

    std::vector<DomainConstraint> constraints;

    bool has_inherits = false;
    SchemaPath parent_domain_path;

    bool has_collation = false;
    std::string collation_name;

    bool has_dialect = false;
    std::string dialect_tag;
    bool has_compat = false;
    std::string compat_name;

    bool enum_wrap = false;

    bool has_security = false;
    DomainSecurityOptions security;

    bool has_integrity = false;
    DomainIntegrityOptions integrity;

    bool has_validation = false;
    DomainValidationOptions validation;

    bool has_quality = false;
    DomainQualityOptions quality;
};

/**
 * CREATE TYPE statement
 */
class CreateTypeStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateTypeStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_not_exists = false;
    SchemaPath type_path;
    TypeKind type_kind = TypeKind::RECORD;
    std::vector<DomainRecordField> record_fields;
    std::vector<DomainEnumValue> enum_values;
    RangeTypeOptions range_options;
    BaseTypeOptions base_options;
    bool is_shell = false;
    bool has_dialect = false;
    std::string dialect_tag;
    bool has_compat = false;
    std::string compat_name;
    bool has_comment = false;
    std::string comment;
};

/**
 * ALTER TYPE statement
 */
enum class AlterTypeAction : uint8_t {
    RENAME_TO = 50,
    SET_SCHEMA = 51,
    ADD_VALUE = 52,
    RENAME_VALUE = 53,
    SET_OPTIONS = 54,
    FINALIZE = 55,
};

class AlterTypeStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterTypeStmt; }
    void accept(ASTVisitor& visitor) override;

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
    RangeTypeOptions range_options;
    BaseTypeOptions base_options;
};

/**
 * DROP TYPE statement
 */
class DropTypeStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropTypeStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    std::vector<SchemaPath> types;
    bool cascade = false;
    bool restrict = false;
};

/**
 * ALTER DOMAIN statement
 */
enum class AlterDomainAction : uint8_t {
    SET_DEFAULT = 0,
    DROP_DEFAULT = 1,
    ADD_CHECK = 2,
    DROP_CONSTRAINT = 3,
    RENAME = 4,
    SET_COMPAT = 5,
    DROP_COMPAT = 6,
};

class AlterDomainStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterDomainStmt; }
    void accept(ASTVisitor& visitor) override;

    AlterDomainAction action = AlterDomainAction::SET_DEFAULT;
    SchemaPath domain_path;
    std::string value;  // Default/check expression or compat name
    StringPool::StringId constraint_name = StringPool::INVALID_ID;
    StringPool::StringId new_name = StringPool::INVALID_ID;
};

/**
 * DROP DOMAIN statement
 */
class DropDomainStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropDomainStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    std::vector<SchemaPath> domains;
    bool restrict = false;
};

/**
 * DROP DATABASE statement
 */
class DropDatabaseStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropDatabaseStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    SchemaPath database_path;
    bool force = false;
};

/**
 * ALTER SCHEMA statement
 */
enum class AlterSchemaAction : uint8_t {
    RENAME = 1,
    SET_OWNER = 2,
    SET_PATH = 3,
};

class AlterSchemaStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterSchemaStmt; }
    void accept(ASTVisitor& visitor) override;

    AlterSchemaAction action = AlterSchemaAction::RENAME;
    SchemaPath schema_path;
    StringPool::StringId new_name = StringPool::INVALID_ID;
    StringPool::StringId owner = StringPool::INVALID_ID;
    SchemaPath new_path;
};

/**
 * ALTER DATABASE statement
 */
enum class AlterDatabaseAction : uint8_t {
    RENAME = 1,
    SET_OWNER = 2,
    ADD_ALIAS = 3,
    DROP_ALIAS = 4,
    SET_OPTIONS = 5,
};

struct AlterDatabaseOption {
    StringPool::StringId key = StringPool::INVALID_ID;
    StringPool::StringId value = StringPool::INVALID_ID;
};

class AlterDatabaseStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterDatabaseStmt; }
    void accept(ASTVisitor& visitor) override;

    AlterDatabaseAction action = AlterDatabaseAction::RENAME;
    SchemaPath database_path;
    StringPool::StringId new_name = StringPool::INVALID_ID;
    StringPool::StringId owner = StringPool::INVALID_ID;
    StringPool::StringId alias = StringPool::INVALID_ID;
    std::vector<AlterDatabaseOption> options;
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
    FORCE_RLS,
    NO_FORCE_RLS,
    ATTACH_PARTITION,
    DETACH_PARTITION,
    ENABLE_TRIGGER,
    DISABLE_TRIGGER,
    SET_STATISTICS,
    SET_STORAGE,
    INHERIT,
    NO_INHERIT,
    VALIDATE_CONSTRAINT,
    ALTER_COLUMN_SET_DEFAULT,
    ALTER_COLUMN_DROP_DEFAULT,
    ALTER_COLUMN_SET_NOT_NULL,
    ALTER_COLUMN_DROP_NOT_NULL,
    ALTER_COLUMN_POSITION,
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
    Expression* default_expr = nullptr;
    bool has_default_expr = false;
    int32_t position_1_based = 0;
    bool has_position = false;

    // For RENAME COLUMN
    StringPool::StringId new_name = StringPool::INVALID_ID;

    // For ADD/DROP CONSTRAINT
    TableConstraint* constraint = nullptr;
    StringPool::StringId constraint_name = StringPool::INVALID_ID;
    bool cascade = false;

    // For ENABLE/DISABLE TRIGGER
    StringPool::StringId trigger_name = StringPool::INVALID_ID;
    bool trigger_all = false;

    // For SET STATISTICS
    int32_t statistics_target = 0;
    bool has_statistics_target = false;

    // For SET STORAGE
    StringPool::StringId storage_type = StringPool::INVALID_ID;

    // For SET TABLESPACE
    SchemaPath tablespace;

    // For SET SCHEMA
    SchemaPath target_schema;

    // For ATTACH/DETACH PARTITION
    SchemaPath partition_path;
    StringPool::StringId partition_bounds = StringPool::INVALID_ID;
    bool has_partition_bounds = false;

    // For INHERIT/NO INHERIT
    SchemaPath inherit_parent;
    bool has_inherit_parent = false;
};

/**
 * ALTER INDEX action types
 */
enum class AlterIndexAction : uint8_t {
    ACTIVE,
    INACTIVE,
    SET_OPTIONS,
};

/**
 * ALTER INDEX statement
 */
class AlterIndexStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterIndexStmt; }
    void accept(ASTVisitor& visitor) override;

    SchemaPath index_path;
    AlterIndexAction action = AlterIndexAction::ACTIVE;
    IndexOptions options;
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
 * DROP SEQUENCE statement
 */
class DropSequenceStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropSequenceStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    std::vector<SchemaPath> sequences;
    bool cascade = false;
};

/**
 * DROP FUNCTION statement
 */
class DropFunctionStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropFunctionStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    std::vector<SchemaPath> functions;
};

/**
 * DROP PROCEDURE statement
 */
class DropProcedureStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropProcedureStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    std::vector<SchemaPath> procedures;
};

/**
 * DROP TRIGGER statement
 */
class DropTriggerStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropTriggerStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    std::vector<SchemaPath> triggers;
};

/**
 * DROP PACKAGE statement
 */
class DropPackageStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropPackageStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    std::vector<SchemaPath> packages;
};

/**
 * DROP ROLE statement
 */
class DropRoleStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropRoleStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    bool cascade = false;
    std::vector<SchemaPath> roles;
};

/**
 * DROP GROUP statement
 */
class DropGroupStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropGroupStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    bool cascade = false;
    std::vector<SchemaPath> groups;
};

/**
 * DROP SERVER statement (FDW)
 */
class DropForeignServerStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropForeignServerStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    bool cascade = false;
    StringPool::StringId server_name = StringPool::INVALID_ID;
};

/**
 * DROP FOREIGN TABLE statement
 */
class DropForeignTableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropForeignTableStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    bool cascade = false;
    std::vector<SchemaPath> tables;
};

/**
 * DROP USER MAPPING statement
 */
class DropUserMappingStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropUserMappingStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    UserMappingTarget target = UserMappingTarget::USER_NAME;
    StringPool::StringId user_name = StringPool::INVALID_ID;
    StringPool::StringId server_name = StringPool::INVALID_ID;
};

/**
 * DROP SYNONYM statement
 */
class DropSynonymStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropSynonymStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    std::vector<SchemaPath> synonyms;
};

/**
 * DROP UDR statement
 */
class DropUdrStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropUdrStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    bool cascade = false;
    std::vector<SchemaPath> udrs;
};

/**
 * DROP EXCEPTION statement
 */
class DropExceptionStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropExceptionStmt; }
    void accept(ASTVisitor& visitor) override;

    bool if_exists = false;
    std::vector<SchemaPath> exceptions;
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

/**
 * ANALYZE statement
 *
 * Supports:
 * - ANALYZE [VERBOSE] table_name [COLUMN column_name] [SAMPLE sample_rate]
 * - ANALYZE [VERBOSE] table_name [(column_name)]
 */
class AnalyzeStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AnalyzeStmt; }
    void accept(ASTVisitor& visitor) override;

    SchemaPath table_path;
    StringPool::StringId column_name = StringPool::INVALID_ID;
    bool has_column = false;
    bool has_sample = false;
    double sample_rate = 0.0;
    bool verbose = false;
};

/**
 * SWEEP DATABASE statement
 *
 * Triggers a manual database sweep/garbage collection.
 */
class SweepDatabaseStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::SweepDatabaseStmt; }
    void accept(ASTVisitor& visitor) override;
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
    COPY,
    CREATE_JOB,
    VIEW_JOB_HISTORY,
    EXECUTE_EXTERNAL_JOB,
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
    JOB,
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

/**
 * EXECUTE PROCEDURE statement (Firebird)
 */
class ExecuteProcedureStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ExecuteProcedureStmt; }
    void accept(ASTVisitor& visitor) override;

    SchemaPath procedure_path;
    std::vector<Expression*> arguments;
    std::vector<StringPool::StringId> returning_variables;
};

/**
 * EXECUTE STATEMENT (dynamic SQL)
 */
class ExecuteStatementStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ExecuteStatementStmt; }
    void accept(ASTVisitor& visitor) override;

    Expression* sql = nullptr;
    std::vector<Expression*> parameters;
    std::vector<StringPool::StringId> into_variables;
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
 * FOR EXECUTE STATEMENT ... DO
 */
class ForExecuteStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ForExecuteStmt; }
    void accept(ASTVisitor& visitor) override;

    Expression* sql = nullptr;
    std::vector<Expression*> parameters;
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
 * Parameter placeholder expression ($1, :name)
 */
class ParameterExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::ParameterExpr; }
    void accept(ASTVisitor& visitor) override;

    bool is_named = false;
    uint32_t index = 0;
    StringPool::StringId name = StringPool::INVALID_ID;
};

/**
 * Binary operator types
 */
enum class BinaryOp : uint8_t {
    // Arithmetic
    ADD, SUB, MUL, DIV, DIV_INT, MOD, POWER,
    // Comparison
    EQ, NE, LT, LE, GT, GE,
    // Logical
    AND, OR,
    // String
    CONCAT,
    REGEX_MATCH, REGEX_MATCH_CI, REGEX_NOT_MATCH, REGEX_NOT_MATCH_CI,
    // Bitwise
    BIT_AND, BIT_OR, BIT_XOR, SHIFT_LEFT, SHIFT_RIGHT,
    // JSON
    JSON_EXTRACT, JSON_EXTRACT_TEXT,
    JSON_HASH_EXTRACT, JSON_HASH_EXTRACT_TEXT,
    JSON_EXISTS, JSON_EXISTS_ANY, JSON_EXISTS_ALL,
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
    WindowSpec* window = nullptr;
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
    std::optional<StringPool::StringId> format;
};

/**
 * Element selector for EXTRACT/ALTER_ELEMENT
 */
struct ElementSelector {
    enum class Kind : uint8_t {
        IDENTIFIER,
        STRING_LITERAL,
        INTEGER_EXPR
    };

    Kind kind = Kind::IDENTIFIER;
    StringPool::StringId identifier = StringPool::INVALID_ID;
    std::vector<Expression*> args;
    StringPool::StringId string_literal = StringPool::INVALID_ID;
    Expression* expr = nullptr;
};

/**
 * EXTRACT expression
 */
class ExtractExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::ExtractExpr; }
    void accept(ASTVisitor& visitor) override;

    ElementSelector selector;
    Expression* source = nullptr;
};

/**
 * ALTER_ELEMENT expression
 */
class AlterElementExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::AlterElementExpr; }
    void accept(ASTVisitor& visitor) override;

    ElementSelector selector;
    Expression* source = nullptr;
    Expression* new_value = nullptr;
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
struct WithClause;

/**
 * SELECT statement
 */
class SelectStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::SelectStmt; }
    void accept(ASTVisitor& visitor) override;

    // WITH clause
    WithClause* with = nullptr;

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
    GroupingType grouping_type = GroupingType::STANDARD;
    std::vector<std::vector<Expression*>> grouping_sets;

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

/**
 * COPY statement
 */
struct CopyOptions {
    enum class Format : uint8_t {
        TEXT = 0,
        CSV = 1,
        BINARY = 2
    };

    enum class OnError : uint8_t {
        ABORT = 0,
        SKIP = 1
    };

    bool format_set = false;
    Format format = Format::TEXT;

    bool delimiter_set = false;
    StringPool::StringId delimiter = StringPool::INVALID_ID;

    bool null_set = false;
    StringPool::StringId null_string = StringPool::INVALID_ID;

    bool header_set = false;
    bool header = false;

    bool quote_set = false;
    StringPool::StringId quote = StringPool::INVALID_ID;

    bool escape_set = false;
    StringPool::StringId escape = StringPool::INVALID_ID;

    bool encoding_set = false;
    StringPool::StringId encoding = StringPool::INVALID_ID;

    bool batch_size_set = false;
    int64_t batch_size = 0;

    bool max_errors_set = false;
    int64_t max_errors = 0;

    bool on_error_set = false;
    OnError on_error = OnError::ABORT;
};

class CopyStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CopyStmt; }
    void accept(ASTVisitor& visitor) override;

    // COPY (SELECT ...) TO ...
    SelectStmt* query = nullptr;

    // Target table
    SchemaPath table_path;

    // Column list (optional)
    std::vector<StringPool::StringId> columns;

    enum class Direction {
        FROM,
        TO
    };
    Direction direction = Direction::FROM;

    // Target source/destination
    bool target_is_stdin = false;
    bool target_is_stdout = false;
    StringPool::StringId target = StringPool::INVALID_ID;  // File path when not STDIN/STDOUT

    CopyOptions options;
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
        SQL_DIALECT,    // SET SQL DIALECT n
        NAMES,          // SET NAMES charset
        LOCAL_TIMEOUT,  // SET LOCAL_TIMEOUT n
        SESSION_AUTHORIZATION,  // SET SESSION AUTHORIZATION ...
        ROLE,           // SET ROLE ...
        // Firebird-specific
        TERM,           // SET TERM new_terminator
        STATISTICS_INDEX,  // SET STATISTICS INDEX index_name
        GENERATOR,      // SET GENERATOR name TO value
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

    // For SET SQL DIALECT
    uint8_t sql_dialect = 0;  // 1, 2, or 3 (0 = not set)

    // For SET LOCAL_TIMEOUT
    uint32_t local_timeout_seconds = 0;
};

/**
 * ALTER SYSTEM SET statement
 *
 * Supports:
 * - ALTER SYSTEM SET section.key = value
 */
class AlterSystemStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterSystemStmt; }
    void accept(ASTVisitor& visitor) override;

    StringPool::StringId name = StringPool::INVALID_ID;
    Expression* value = nullptr;
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
 * - SHOW JOBS [LIKE 'pattern']
 * - SHOW JOB name
 * - SHOW JOB RUNS [FOR] job_name
 * - SHOW CHECKS table
 * - SHOW COLLATIONS [LIKE 'pattern']
 * - SHOW SQL DIALECT
 * - SHOW VERSION
 * - SHOW DATABASE
 * - SHOW SYSTEM
 * - SHOW METRICS
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
        JOBS,               // SHOW JOBS [LIKE pattern]
        JOB,                // SHOW JOB name
        JOB_RUNS,           // SHOW JOB RUNS [FOR] job_name
        CHECKS,             // SHOW CHECKS table
        COLLATIONS,         // SHOW COLLATIONS [LIKE pattern]
        COMMENTS,           // SHOW COMMENTS [object_name]
        DEPENDENCIES,       // SHOW DEPENDENCIES [object_name]
        PACKAGE,            // SHOW PACKAGE package_name
        SQL_DIALECT,        // SHOW SQL DIALECT
        VERSION,            // SHOW VERSION
        DATABASE,           // SHOW DATABASE (current database info)
        SYSTEM,             // SHOW SYSTEM (system tables/info)
        METRICS,            // SHOW METRICS (metrics export)
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
enum class LikeMatchKind : uint8_t {
    LIKE,
    ILIKE,
    SIMILAR,
    CONTAINING,
    STARTING
};

class LikeExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::LikeExpr; }
    void accept(ASTVisitor& visitor) override;

    Expression* expr = nullptr;
    bool negated = false;  // NOT LIKE
    bool case_insensitive = false;  // ILIKE
    LikeMatchKind match_kind = LikeMatchKind::LIKE;
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
    virtual void visit(AlterSequenceStmt* stmt) = 0;
    virtual void visit(CreateSchemaStmt* stmt) = 0;
    virtual void visit(DropSchemaStmt* stmt) = 0;
    virtual void visit(AlterSchemaStmt* stmt) = 0;
    virtual void visit(CreateDatabaseStmt* stmt) = 0;
    virtual void visit(CreateTablespaceStmt* stmt) = 0;
    virtual void visit(AlterTablespaceStmt* stmt) = 0;
    virtual void visit(DropTablespaceStmt* stmt) = 0;
    virtual void visit(AttachTablespaceStmt* stmt) = 0;
    virtual void visit(DetachTablespaceStmt* stmt) = 0;
    virtual void visit(CreateFunctionStmt* stmt) = 0;
    virtual void visit(CreateProcedureStmt* stmt) = 0;
    virtual void visit(CreateTriggerStmt* stmt) = 0;
    virtual void visit(CreatePackageStmt* stmt) = 0;
    virtual void visit(CreateUserStmt* stmt) = 0;
    virtual void visit(CreateRoleStmt* stmt) = 0;
    virtual void visit(CreateGroupStmt* stmt) = 0;
    virtual void visit(CreatePolicyStmt* stmt) = 0;
    virtual void visit(CreateExceptionStmt* stmt) = 0;
    virtual void visit(CreateJobStmt* stmt) = 0;
    virtual void visit(CreateTypeStmt* stmt) = 0;
    virtual void visit(CreateDomainStmt* stmt) = 0;
    virtual void visit(CreateForeignServerStmt* stmt) = 0;
    virtual void visit(CreateForeignTableStmt* stmt) = 0;
    virtual void visit(CreateUserMappingStmt* stmt) = 0;
    virtual void visit(CreateSynonymStmt* stmt) = 0;
    virtual void visit(CreateUdrStmt* stmt) = 0;
    virtual void visit(AlterTypeStmt* stmt) = 0;
    virtual void visit(DropTypeStmt* stmt) = 0;
    virtual void visit(AlterDomainStmt* stmt) = 0;
    virtual void visit(DropDomainStmt* stmt) = 0;
    virtual void visit(DropDatabaseStmt* stmt) = 0;
    virtual void visit(AlterDatabaseStmt* stmt) = 0;
    virtual void visit(AlterTableStmt* stmt) = 0;
    virtual void visit(AlterPolicyStmt* stmt) = 0;
    virtual void visit(AlterIndexStmt* stmt) = 0;
    virtual void visit(RenameObjectStmt* stmt) = 0;
    virtual void visit(MoveObjectStmt* stmt) = 0;
    virtual void visit(DropTableStmt* stmt) = 0;
    virtual void visit(DropPolicyStmt* stmt) = 0;
    virtual void visit(DropIndexStmt* stmt) = 0;
    virtual void visit(DropViewStmt* stmt) = 0;
    virtual void visit(DropSequenceStmt* stmt) = 0;
    virtual void visit(DropFunctionStmt* stmt) = 0;
    virtual void visit(DropProcedureStmt* stmt) = 0;
    virtual void visit(DropTriggerStmt* stmt) = 0;
    virtual void visit(DropPackageStmt* stmt) = 0;
    virtual void visit(DropRoleStmt* stmt) = 0;
    virtual void visit(DropGroupStmt* stmt) = 0;
    virtual void visit(DropExceptionStmt* stmt) = 0;
    virtual void visit(DropForeignServerStmt* stmt) = 0;
    virtual void visit(DropForeignTableStmt* stmt) = 0;
    virtual void visit(DropUserMappingStmt* stmt) = 0;
    virtual void visit(DropSynonymStmt* stmt) = 0;
    virtual void visit(DropUdrStmt* stmt) = 0;
    virtual void visit(DropJobStmt* stmt) = 0;
    virtual void visit(TruncateTableStmt* stmt) = 0;
    virtual void visit(AlterJobStmt* stmt) = 0;
    virtual void visit(ExecuteJobStmt* stmt) = 0;
    virtual void visit(CancelJobRunStmt* stmt) = 0;

    // DML statements
    virtual void visit(SelectStmt* stmt) = 0;
    virtual void visit(InsertStmt* stmt) = 0;
    virtual void visit(UpdateStmt* stmt) = 0;
    virtual void visit(DeleteStmt* stmt) = 0;
    virtual void visit(CopyStmt* stmt) = 0;
    virtual void visit(MergeStmt* stmt) = 0;
    virtual void visit(ExecuteProcedureStmt* stmt) = 0;
    virtual void visit(ExecuteStatementStmt* stmt) = 0;

    // Transaction statements
    virtual void visit(StartTransactionStmt* stmt) = 0;
    virtual void visit(PrepareTransactionStmt* stmt) = 0;
    virtual void visit(CommitStmt* stmt) = 0;
    virtual void visit(RollbackStmt* stmt) = 0;
    virtual void visit(SavepointStmt* stmt) = 0;
    virtual void visit(ReleaseSavepointStmt* stmt) = 0;

    // Session statements
    virtual void visit(SetStmt* stmt) = 0;
    virtual void visit(AlterSystemStmt* stmt) = 0;
    virtual void visit(ResetStmt* stmt) = 0;
    virtual void visit(ShowStmt* stmt) = 0;
    virtual void visit(ExplainStmt* stmt) = 0;
    virtual void visit(AnalyzeStmt* stmt) = 0;
    virtual void visit(SweepDatabaseStmt* stmt) = 0;

    // DCL statements
    virtual void visit(GrantStmt* stmt) = 0;
    virtual void visit(RevokeStmt* stmt) = 0;

    // Connection statements
    virtual void visit(ConnectStmt* stmt) = 0;
    virtual void visit(DisconnectStmt* stmt) = 0;

    // Metadata statements
    virtual void visit(CommentStmt* stmt) = 0;

    // PSQL statements
    virtual void visit(ExecuteBlockStmt* stmt) = 0;
    virtual void visit(CompoundStmt* stmt) = 0;
    virtual void visit(DeclareVariableStmt* stmt) = 0;
    virtual void visit(AssignmentStmt* stmt) = 0;
    virtual void visit(IfStmt* stmt) = 0;
    virtual void visit(WhileStmt* stmt) = 0;
    virtual void visit(ForSelectStmt* stmt) = 0;
    virtual void visit(ForExecuteStmt* stmt) = 0;
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
    virtual void visit(ParameterExpr* expr) = 0;
    virtual void visit(BinaryExpr* expr) = 0;
    virtual void visit(UnaryExpr* expr) = 0;
    virtual void visit(FunctionCallExpr* expr) = 0;
    virtual void visit(CastExpr* expr) = 0;
    virtual void visit(ExtractExpr* expr) = 0;
    virtual void visit(AlterElementExpr* expr) = 0;
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
