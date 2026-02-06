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
 * MySQL Parser
 *
 * Recursive-descent parser for MySQL 8.0 SQL dialect.
 * Unlike the ScratchBird V2 parser which produces AST nodes,
 * this parser generates SBLR bytecode directly for execution.
 *
 * Schema: Databases are emulated as schemas at:
 *   /remote/emulated/mysql/localhost/{database}/
 *
 * Supported statements:
 * - DDL: CREATE/ALTER/DROP TABLE, INDEX, VIEW, DATABASE
 * - DML: SELECT, INSERT, UPDATE, DELETE, REPLACE
 * - Transaction: BEGIN, COMMIT, ROLLBACK, SAVEPOINT
 * - Admin: SHOW, DESCRIBE, USE, SET
 * - Stored programs: CREATE PROCEDURE/FUNCTION (limited)
 */

#include "mysql_lexer.h"
#include <scratchbird/sblr/opcodes.h>
#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace scratchbird {
namespace core {
class Database;
}
}

namespace scratchbird::parser::mysql {

/**
 * Parse error with location information
 */
struct ParseError {
    std::string message;
    SourceLocation location;

    ParseError(std::string msg, SourceLocation loc)
        : message(std::move(msg)), location(loc) {}
};

/**
 * Compilation/parse result containing bytecode or errors
 */
class ParseResult {
public:
    ParseResult() = default;

    bool success() const { return errors_.empty(); }

    const std::vector<ParseError>& errors() const { return errors_; }
    const std::vector<std::string>& warnings() const { return warnings_; }

    void addError(const std::string& message, SourceLocation loc) {
        errors_.emplace_back(message, loc);
    }
    void addWarning(const std::string& message) {
        warnings_.push_back(message);
    }

    const std::vector<uint8_t>& bytecode() const { return bytecode_; }
    std::vector<uint8_t>& bytecode() { return bytecode_; }

    void setBytecode(std::vector<uint8_t> bc) {
        bytecode_ = std::move(bc);
    }

private:
    std::vector<ParseError> errors_;
    std::vector<std::string> warnings_;
    std::vector<uint8_t> bytecode_;
};

enum class MySQLCompatMode : uint8_t {
    MYSQL57 = 0,
    MYSQL80 = 1,
};

/**
 * MySQL data type representation
 */
struct MySQLDataType {
    enum class Kind {
        // Integer types
        TINYINT, SMALLINT, MEDIUMINT, INT, BIGINT, INT128, UINT128,
        // Floating point
        FLOAT, DOUBLE, DECIMAL,
        // String types
        CHAR, VARCHAR, TEXT, TINYTEXT, MEDIUMTEXT, LONGTEXT,
        // Binary types
        BINARY, VARBINARY, BLOB, TINYBLOB, MEDIUMBLOB, LONGBLOB,
        // Date/Time types
        DATE, TIME, DATETIME, TIMESTAMP, YEAR,
        // Other types
        BIT, BOOL, ENUM, SET, JSON, GEOMETRY, POINT, LINESTRING, POLYGON
    };

    Kind kind;
    int length = 0;         // For CHAR, VARCHAR, etc.
    int precision = 0;      // For DECIMAL
    int scale = 0;          // For DECIMAL
    bool unsigned_ = false;
    bool zerofill = false;
    bool nullable = true;
    std::string charset;
    std::string collation;
    std::vector<std::string> enum_values;  // For ENUM/SET

    MySQLDataType() : kind(Kind::INT) {}
    explicit MySQLDataType(Kind k) : kind(k) {}
};

/**
 * Column definition for CREATE TABLE
 */
struct ColumnDef {
    std::string name;
    MySQLDataType type;
    bool primary_key = false;
    bool unique = false;
    bool auto_increment = false;
    bool has_default = false;
    enum class DefaultLiteralType {
        NONE,
        NULL_VALUE,
        STRING,
        INT,
        FLOAT
    };
    DefaultLiteralType default_literal_type = DefaultLiteralType::NONE;
    int64_t default_int_value = 0;
    double default_float_value = 0.0;
    std::string default_value;
    bool default_is_null = false;
    bool default_is_expr = false;
    std::vector<uint8_t> default_expr_bytecode;
    bool is_generated = false;
    bool generated_stored = true;
    std::vector<uint8_t> generated_expr_bytecode;
    std::string comment;
};

/**
 * Index definition for CREATE TABLE/INDEX
 */
struct IndexDef {
    enum class Type { NORMAL, UNIQUE, PRIMARY, FULLTEXT, SPATIAL };
    Type type = Type::NORMAL;
    std::string name;
    std::vector<std::string> columns;
    std::vector<int> column_lengths;  // Prefix lengths for text columns
    std::string algorithm;  // BTREE, HASH, RTREE
    std::string comment;
};

/**
 * Foreign key definition
 */
struct ForeignKeyDef {
    std::string name;
    std::vector<std::string> columns;
    std::string ref_table;
    std::vector<std::string> ref_columns;
    std::string on_delete;  // CASCADE, SET NULL, etc.
    std::string on_update;
};

/**
 * Expression types for SELECT, WHERE, etc.
 */
enum class ExprType {
    LITERAL_INT,
    LITERAL_FLOAT,
    LITERAL_STRING,
    LITERAL_NULL,
    LITERAL_BOOL,
    COLUMN_REF,
    BINARY_OP,
    UNARY_OP,
    FUNCTION_CALL,
    CASE_EXPR,
    SUBQUERY,
    EXISTS_EXPR,
    IN_EXPR,
    BETWEEN_EXPR,
    LIKE_EXPR,
    IS_NULL_EXPR,
    CAST_EXPR,
    AGGREGATE_CALL,
    WINDOW_CALL,
    USER_VARIABLE,
    SYSTEM_VARIABLE,
    PLACEHOLDER
};

/**
 * Forward declarations for expression tree
 */
struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

/**
 * Expression node (simplified for SBLR generation)
 */
struct Expr {
    ExprType type;

    // For literals
    int64_t int_value = 0;
    double float_value = 0.0;
    std::string string_value;
    bool bool_value = false;

    // For column references: schema.table.column
    std::string schema;
    std::string table;
    std::string column;

    // For binary/unary ops
    std::string op;
    ExprPtr left;
    ExprPtr right;

    // For function calls
    std::string function_name;
    std::vector<ExprPtr> args;
    bool distinct = false;  // For aggregates

    // For CASE
    ExprPtr case_value;
    std::vector<std::pair<ExprPtr, ExprPtr>> when_clauses;
    ExprPtr else_clause;

    // For window functions
    std::vector<ExprPtr> partition_by;
    std::vector<std::pair<ExprPtr, bool>> order_by;  // expr, is_desc

    Expr() : type(ExprType::LITERAL_NULL) {}
    explicit Expr(ExprType t) : type(t) {}
};

/**
 * Table reference for FROM clause
 */
struct TableRef {
    enum class Type { TABLE, SUBQUERY, JOIN };
    Type type;
    std::string schema;
    std::string table;
    std::string alias;

    // For joins
    std::string join_type;  // INNER, LEFT, RIGHT, CROSS
    std::unique_ptr<TableRef> left;
    std::unique_ptr<TableRef> right;
    ExprPtr join_condition;
    std::vector<std::string> using_columns;

    // For subqueries
    std::unique_ptr<struct SelectStmt> subquery;

    TableRef() : type(Type::TABLE) {}
};

/**
 * SELECT statement structure
 */
struct SelectStmt {
    bool distinct = false;
    std::vector<std::pair<ExprPtr, std::string>> select_list;  // expr, alias
    std::unique_ptr<TableRef> from;
    ExprPtr where;
    std::vector<ExprPtr> group_by;
    ExprPtr having;
    std::vector<std::pair<ExprPtr, bool>> order_by;  // expr, is_desc
    int64_t limit = -1;
    int64_t offset = 0;
    bool for_update = false;

    // UNION support
    std::unique_ptr<SelectStmt> union_stmt;
    bool union_all = false;
};

/**
 * MySQL Parser
 *
 * Parses MySQL SQL and generates SBLR bytecode for execution.
 */
class Parser {
public:
    /**
     * Create a parser for the given SQL input.
     * @param input The SQL statement(s) to parse
     * @param db The database context (for schema resolution)
     * @param default_schema Default schema path for unqualified tables
     */
    Parser(std::string_view input,
           core::Database* db = nullptr,
           std::string_view default_schema = "/remote/emulated/mysql/localhost/");

    ~Parser() = default;

    // Non-copyable
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    /**
     * Parse and compile a single statement.
     * Returns bytecode ready for execution.
     */
    ParseResult parseStatement();

    /**
     * Parse all statements in the input.
     * Useful for multi-statement queries.
     */
    std::vector<ParseResult> parseAll();

    /**
     * Get the lexer's string pool.
     */
    StringPool& stringPool() { return lexer_.stringPool(); }
    const StringPool& stringPool() const { return lexer_.stringPool(); }

    void setCompatibilityMode(MySQLCompatMode mode) { compat_mode_ = mode; }
    MySQLCompatMode compatibilityMode() const { return compat_mode_; }

private:
    struct SelectItem {
        enum class Kind {
            Star,
            Column,
            Expression
        };
        Kind kind = Kind::Expression;
        std::string column_name;
        std::vector<uint8_t> expr_bytecode;
        std::string alias;
    };

    struct WindowFunctionSpec {
        bool is_aggregate = false;
        bool is_extended = false;
        sblr::Opcode func_opcode = sblr::Opcode::WIN_ROW_NUMBER;
        sblr::ExtendedOpcode ext_opcode = sblr::ExtendedOpcode::EXT_WIN_CUME_DIST;
        std::vector<std::vector<uint8_t>> args;
        sblr::Opcode agg_opcode = sblr::Opcode::AGG_COUNT;
        std::vector<uint8_t> agg_expr;
        bool agg_count_star = false;
        std::vector<std::string> partition_columns;
        std::vector<std::string> order_columns;
        bool has_frame = false;
        sblr::Opcode frame_mode = sblr::Opcode::FRAME_ROWS;
        sblr::Opcode frame_start = sblr::Opcode::FRAME_UNBOUNDED_PRECEDING;
        sblr::Opcode frame_end = sblr::Opcode::FRAME_CURRENT_ROW;
        std::string output_column;
        std::string function_name;
        // C4: Named window reference
        std::string named_window_ref;
    };
    
    // C4: Named window definition for WINDOW clause
    struct NamedWindowDef {
        std::string name;
        std::string ref_name;  // For WINDOW w2 AS (w1 ...) - references another window
        std::vector<std::string> partition_columns;
        std::vector<std::string> order_columns;
        bool has_frame = false;
        sblr::Opcode frame_mode = sblr::Opcode::FRAME_ROWS;
        sblr::Opcode frame_start = sblr::Opcode::FRAME_UNBOUNDED_PRECEDING;
        sblr::Opcode frame_end = sblr::Opcode::FRAME_CURRENT_ROW;
    };

    Lexer lexer_;
    core::Database* db_;
    std::string default_schema_;
    Token current_token_;
    std::vector<uint8_t> bytecode_;
    std::vector<ParseError> errors_;
    uint32_t next_placeholder_index_ = 1;
    bool emit_enabled_ = true;
    bool pending_or_replace_ = false;
    bool in_on_duplicate_update_ = false;
    MySQLCompatMode compat_mode_ = MySQLCompatMode::MYSQL57;
    std::vector<std::string> warnings_;
    std::vector<WindowFunctionSpec> window_specs_;
    bool last_expr_was_window_ = false;
    size_t last_window_index_ = 0;
    // C4: Named windows from WINDOW clause
    std::unordered_map<std::string, NamedWindowDef> named_windows_;

    // Token management
    void advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool matchKeyword(TokenType kw);
    bool matchIdentifierKeyword(const char* keyword);
    Token consume(TokenType type, const std::string& message);
    Token consumeKeyword(TokenType kw, const std::string& message);

    // Error handling
    void error(const std::string& message);
    void warning(const std::string& message);
    void synchronize();

    // Bytecode emission helpers
    void emit(sblr::Opcode op);
    void emitByte(uint8_t byte);
    void emitU16(uint16_t val);
    void emitUVarint(uint64_t val);
    void emitU32(uint32_t val);
    void emitU64(uint64_t val);
    void emitI64(int64_t val);
    void emitF64(double val);
    void emitString(std::string_view str);
    void emitTypeDefinition(const MySQLDataType& type);
    void emitDebugSpan(const SourceSpan& span);

    // Statement parsing
    void parseStatementInternal();
    void parseSelectStmt();
    void parseInsertStmt();
    void parseUpdateStmt();
    void parseDeleteStmt();
    void parseReplaceStmt();
    void parseCreateStmt();
    void parseRenameStmt();
    void parseAlterStmt();
    void parseAlterRole();
    void parseAlterView();
    void parseAlterProcedure();
    void parseAlterFunction();
    void parseKillStmt();
    void parseFlushStmt();
    void parseWindowClause();
    void parseFrameBound();
    void parseMatchAgainstExpr();
    void parseDropStmt();
    void parseTruncateStmt();
    void parseSetStmt();
    void parseShowStmt();
    void parseDescribeStmt();
    void parseUseStmt();
    void parseBeginStmt();
    void parseCommitStmt();
    void parseRollbackStmt();
    void parseSavepointStmt();
    void parseReleaseStmt();
    void parseLockStmt();
    void parseUnlockStmt();
    void parseGrantStmt();
    void parseRevokeStmt();
    void parseExplainStmt();
    void parsePrepareStmt();
    void parseExecuteStmt();
    void parseDeallocateStmt();

    // DDL parsing
    void parseCreateTable();
    void parseCreateIndex();
    void parseCreateView();
    void parseCreateDatabase();
    void parseCreateTablespace();
    void parseCreateUser();
    void parseCreateRole();
    void parseAlterUser();
    void parseAlterTablespace();
    void parseDropUser();
    void parseDropRole();
    void parseDropTablespace();
    void parseCreateProcedure();
    void parseCreateFunction();
    void parseCreateTrigger();
    ColumnDef parseColumnDef();
    MySQLDataType parseDataType();
    IndexDef parseIndexDef();
    ForeignKeyDef parseForeignKeyDef();

    // DML clause parsing
    void parseSelectList(std::vector<SelectItem>& items);
    void parseFromClause();
    void parseWhereClause();
    void parseGroupByClause();
    void parseHavingClause();
    void parseOrderByClause();
    void parseLimitClause();
    void parseWindowSpecForFunction(WindowFunctionSpec& spec);
    std::string parseWindowColumnName();
    bool parseWindowFrameBound(sblr::Opcode& bound_out);
    void emitWindowSpecs();

    // Expression parsing (generates bytecode)
    void parseExpression();
    void parseOrExpr();
    void parseXorExpr();
    void parseAndExpr();
    void parseNotExpr();
    void parseComparisonExpr();
    void parseBitwiseOrExpr();
    void parseBitwiseXorExpr();
    void parseBitwiseAndExpr();
    void parseShiftExpr();
    void parseAdditiveExpr();
    void parseMultiplicativeExpr();
    void parseUnaryExpr();
    void parsePrimaryExpr();
    void parseFunctionCall(const std::string& name);
    void parseCaseExpr();
    void parseCastExpr();
    void parseExtractExpr();
    void parseAlterElementExpr();
    sblr::ExtractField parseElementSelector(uint8_t& arg_count);
    void parseSubquery();

    // Table reference parsing
    std::unique_ptr<TableRef> parseTableRef();
    std::unique_ptr<TableRef> parseJoinClause(std::unique_ptr<TableRef> left);

    // Type conversion helpers
    sblr::Opcode typeToOpcode(MySQLDataType::Kind kind);

    // Identifier helpers
    std::string parseIdentifier();
    std::string parseQualifiedName();
    void resolveTableName(std::string& schema, std::string& table);
    std::vector<uint8_t> captureExpressionBytecode();
};

} // namespace scratchbird::parser::mysql
