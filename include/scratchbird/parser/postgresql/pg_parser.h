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
 * PostgreSQL Parser
 *
 * Recursive-descent parser for PostgreSQL 16 SQL dialect.
 * This parser generates SBLR bytecode directly for execution.
 *
 * Schema: Emulated PostgreSQL databases are rooted at:
 *   emulated.postgresql.localhost.databases.{database}
 *
 * Supported statements:
 * - DDL: CREATE/ALTER/DROP TABLE, INDEX, VIEW, SEQUENCE, FUNCTION, etc.
 * - DML: SELECT, INSERT, UPDATE, DELETE, MERGE
 * - Transaction: BEGIN, COMMIT, ROLLBACK, SAVEPOINT
 * - Admin: ANALYZE, EXPLAIN, SET, SHOW
 * - PL/pgSQL: Functions, Procedures (limited)
 */

#include "pg_lexer.h"
#include "scratchbird/parser/ast_v3.h"
#include <scratchbird/sblr/opcodes.h>
#include <scratchbird/core/types.h>
#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace scratchbird {
namespace core {
class Database;
}
}

namespace scratchbird::parser::postgresql {

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
 * Parse result containing AST or errors
 */
class ParseResult {
public:
    ParseResult() = default;

    bool success() const { return errors_.empty(); }

    const std::vector<ParseError>& errors() const { return errors_; }

    void addError(const std::string& message, SourceLocation loc) {
        errors_.emplace_back(message, loc);
    }

    const std::vector<uint8_t>& bytecode() const { return bytecode_; }
    std::vector<uint8_t>& bytecode() { return bytecode_; }
    void setBytecode(std::vector<uint8_t> bc) { bytecode_ = std::move(bc); }

    parser::v3::Statement* statement() const { return statement_; }
    parser::v3::ASTArena* arena() const { return arena_.get(); }
    parser::v3::StringPool& stringPool() { return string_pool_; }
    const parser::v3::StringPool& stringPool() const { return string_pool_; }

    void setStatement(parser::v3::Statement* stmt) { statement_ = stmt; }
    void setArena(std::unique_ptr<parser::v3::ASTArena> arena) { arena_ = std::move(arena); }

private:
    std::vector<ParseError> errors_;
    std::vector<uint8_t> bytecode_;
    parser::v3::Statement* statement_ = nullptr;
    std::unique_ptr<parser::v3::ASTArena> arena_;
    parser::v3::StringPool string_pool_;
};

/**
 * PostgreSQL data type representation
 */
struct PgDataType {
    enum class Kind {
        // Integer types
        SMALLINT, INTEGER, BIGINT, INT128, UINT128,
        // Floating point
        REAL, DOUBLE_PRECISION, DECIMAL, NUMERIC, MONEY,
        // Serial types (pseudo-types for auto-increment)
        SMALLSERIAL, SERIAL, BIGSERIAL,
        // Character types
        CHAR, VARCHAR, TEXT,
        // Binary types
        BYTEA,
        // Date/Time types
        DATE, TIME, TIMETZ, TIMESTAMP, TIMESTAMPTZ, INTERVAL,
        // Boolean
        BOOLEAN,
        // UUID
        UUID,
        // JSON types
        JSON, JSONB, JSONPATH,
        // Array (any type can be an array)
        ARRAY,
        // Geometric types
        POINT, LINE, LSEG, BOX, PATH, POLYGON, CIRCLE,
        // Network types
        CIDR, INET, MACADDR, MACADDR8,
        // Bit string types
        BIT, VARBIT,
        // Text search types
        TSVECTOR, TSQUERY,
        // Range types
        INT4RANGE, INT8RANGE, NUMRANGE, DATERANGE, TSRANGE, TSTZRANGE,
        // XML
        XML,
        // User-defined/enum
        ENUM, DOMAIN, COMPOSITE
    };

    Kind kind;
    int length = 0;          // For CHAR, VARCHAR, BIT
    int precision = 0;       // For NUMERIC, DECIMAL, TIME, TIMESTAMP
    int scale = 0;           // For NUMERIC, DECIMAL
    bool with_time_zone = false;  // For TIME, TIMESTAMP
    bool nullable = true;
    std::string type_name;   // For user-defined types
    Kind element_kind = Kind::INTEGER;  // For ARRAY types (element base kind)
    int array_dimensions = 0;  // For ARRAY types (count of [] groups)
    int array_size = 0;  // For ARRAY types (fixed size, 0 = unspecified)
    std::string element_type;  // For ARRAY types (element domain/type name)

    PgDataType() : kind(Kind::INTEGER) {}
    explicit PgDataType(Kind k) : kind(k) {}
};

/**
 * Column definition for CREATE TABLE
 */
struct ColumnDef {
    std::string name;
    PgDataType type;
    bool primary_key = false;
    bool unique = false;
    bool not_null = false;
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
    std::vector<uint8_t> generated_expr_bytecode;
    bool generated_stored = true;  // STORED vs VIRTUAL
    bool is_identity = false;
    bool identity_always = true;   // ALWAYS vs BY DEFAULT
    std::string collation;
};

/**
 * Index definition for CREATE INDEX
 */
struct IndexDef {
    enum class Method { BTREE, HASH, GIN, GIST, SPGIST, BRIN };
    Method method = Method::BTREE;
    std::string name;
    std::string table_name;
    std::vector<std::string> columns;
    std::vector<bool> column_desc;     // ASC/DESC per column
    std::vector<bool> column_nulls_first;  // NULLS FIRST/LAST per column
    bool unique = false;
    bool concurrent = false;
    bool if_not_exists = false;
    std::string where_clause;          // Partial index predicate
    std::vector<std::string> include_columns;  // INCLUDE columns
};

/**
 * Foreign key definition
 */
struct ForeignKeyDef {
    std::string name;
    std::vector<std::string> columns;
    std::string ref_table;
    std::vector<std::string> ref_columns;
    std::string on_delete;  // CASCADE, SET NULL, SET DEFAULT, RESTRICT, NO ACTION
    std::string on_update;
    std::string match_type; // FULL, PARTIAL, SIMPLE
    bool deferrable = false;
    bool initially_deferred = false;
};

/**
 * PostgreSQL Parser
 *
 * Parses PostgreSQL SQL and generates SBLR bytecode for execution.
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
           std::string_view default_schema = "");

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
     * Get the parser's string pool.
     */
    parser::v3::StringPool& stringPool() { return string_pool_; }
    const parser::v3::StringPool& stringPool() const { return string_pool_; }

private:
    Lexer lexer_;
    core::Database* db_;
    std::string default_schema_;
    Token current_token_;
    std::vector<uint8_t> bytecode_;
    std::vector<ParseError> errors_;
    bool emit_enabled_ = true;
    bool pending_index_unique_ = false;
    bool pending_or_replace_ = false;
    bool pending_create_temp_ = false;
    bool pending_create_unlogged_ = false;

    std::unique_ptr<parser::v3::ASTArena> arena_;
    parser::v3::StringPool string_pool_;
    parser::v3::Statement* statement_ = nullptr;

    // Token management
    void advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool matchKeyword(TokenType kw);
    Token consume(TokenType type, const std::string& message);
    Token consumeKeyword(TokenType kw, const std::string& message);
    bool matchIdentifierKeyword(const char* keyword);

    // Error handling
    void error(const std::string& message);
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
    void emitUUID(const core::ID& uuid);
    void emitDebugSpan(const SourceSpan& span);
    void emitTypeDefinition(const PgDataType& type);
    sblr::Opcode typeToOpcode(PgDataType::Kind kind);
    bool resolveDomainId(const std::string& type_name, core::ID& domain_id_out);

    // Statement parsing
    parser::v3::Statement* parseStatementInternal();
    parser::v3::SelectStmt* parseSelectStmt();
    parser::v3::InsertStmt* parseInsertStmt();
    parser::v3::UpdateStmt* parseUpdateStmt();
    parser::v3::DeleteStmt* parseDeleteStmt();
    parser::v3::MergeStmt* parseMergeStmt();
    parser::v3::Statement* parseCreateStmtV3();
    parser::v3::CreateTableStmt* parseCreateTableV3(bool or_replace,
                                                    bool is_temp,
                                                    bool is_unlogged);
    parser::v3::CreateIndexStmt* parseCreateIndexV3(bool unique);
    parser::v3::CreateViewStmt* parseCreateViewV3(bool materialized,
                                                  bool or_replace,
                                                  bool temporary);
    parser::v3::CreateSequenceStmt* parseCreateSequenceV3(bool or_replace,
                                                          bool temporary);
    parser::v3::CreateFunctionStmt* parseCreateFunctionV3(bool or_replace);
    parser::v3::CreateProcedureStmt* parseCreateProcedureV3(bool or_replace);
    parser::v3::CreateTriggerStmt* parseCreateTriggerV3(bool or_replace);
    parser::v3::CreateTypeStmt* parseCreateTypeV3();
    parser::v3::CreateDomainStmt* parseCreateDomainV3();
    parser::v3::TypeName parseTypeNameV3();
    parser::v3::ColumnDef* parseColumnDefV3();
    parser::v3::TableConstraint* parseTableConstraintV3();
    parser::v3::Statement* parseAlterStmtV3();
    parser::v3::Statement* parseDropStmtV3();
    parser::v3::TruncateTableStmt* parseTruncateStmtV3();
    parser::v3::Statement* parseSetStmtV3();
    parser::v3::Statement* parseResetStmtV3();
    parser::v3::Statement* parseShowStmtV3();
    parser::v3::Statement* parseBeginStmtV3();
    parser::v3::Statement* parsePrepareStmtV3();
    parser::v3::Statement* parseExecutePreparedStmtV3();
    parser::v3::Statement* parseDeallocateStmtV3();
    parser::v3::Statement* parseCommitStmtV3();
    parser::v3::Statement* parseRollbackStmtV3();
    parser::v3::Statement* parseSavepointStmtV3();
    parser::v3::Statement* parseReleaseStmtV3();
    parser::v3::Statement* parseCloseCursorStmtV3();
    parser::v3::Statement* parseFetchCursorStmtV3(bool move_only);
    parser::v3::Statement* parseCallStmtV3();
    parser::v3::Statement* parseDoStmtV3();
    parser::v3::Statement* parseGrantStmtV3();
    parser::v3::Statement* parseRevokeStmtV3();
    parser::v3::Statement* parseAnalyzeStmtV3();
    parser::v3::Statement* parseReindexStmtV3();
    parser::v3::Statement* parseVacuumStmtV3();
    parser::v3::Statement* parseExplainStmtV3();
    parser::v3::Statement* parseCopyStmtV3();
    parser::v3::Statement* parseListenStmtV3();
    parser::v3::Statement* parseNotifyStmtV3();
    parser::v3::Statement* parseUnlistenStmtV3();
    parser::v3::Statement* parseLockTableStmtV3();
    void parseCreateStmt();
    void parseAlterStmt();
    void parseDropStmt();
    void parseTruncateStmt();
    void parseSetStmt();
    void parseShowStmt();
    void parseBeginStmt();
    void parsePrepareStmt();
    void parseCommitStmt();
    void parseRollbackStmt();
    void parseSavepointStmt();
    void parseReleaseStmt();
    void parseGrantStmt();
    void parseRevokeStmt();
    void parseAnalyzeStmt();
    void parseExplainStmt();
    void parseCopyStmt();

    // DDL parsing
    void parseCreateTable();
    void parseCreateIndex();
    void parseCreateView();
    void parseCreateMaterializedView();
    void parseCreateSequence();
    void parseCreateDatabase();
    void parseCreateSchema();
    void parseCreateFunction();
    void parseCreateProcedure();
    void parseCreateTrigger();
    void parseCreateType();
    void parseCreateDomain();
    void parseCreatePolicy();
    void parseCreateTablespace();
    void parseAlterDomain();
    void parseDropDomain();
    void parseDropPolicy();
    void parseAlterTablespace();
    void parseDropTablespace();
    ColumnDef parseColumnDef();
    PgDataType parseDataType();
    IndexDef parseIndexDef();
    ForeignKeyDef parseForeignKeyDef();

    struct SelectItem {
        enum class Kind {
            Star,
            Column,
            Expression
        };
        Kind kind = Kind::Expression;
        std::string column_name;
        parser::v3::Expression* expr = nullptr;
        std::string alias;
    };

    // DML clause parsing
    void parseSelectList(std::vector<SelectItem>& items);
    void parseFromClause();
    void parseJoinClause();
    void parseWhereClause();
    void parseGroupByClause();
    void parseHavingClause();
    void parseWindowClause();
    void parseFrameBound();
    void parseOrderByClause();
    void parseLimitClause();
    void parseOffsetClause();
    void parseFetchClause();
    void parseForClause();
    void parseOnConflictClause();
    void parseReturningClause();
    parser::v3::WithClause* parseWithClause();

    // Expression parsing (builds AST)
    parser::v3::Expression* parseExpression();
    std::string parseExpressionText();
    parser::v3::Expression* parseOrExpr();
    parser::v3::Expression* parseAndExpr();
    parser::v3::Expression* parseNotExpr();
    parser::v3::Expression* parseComparisonExpr();
    parser::v3::Expression* parseIsExpr();
    parser::v3::Expression* parseInExpr();
    parser::v3::Expression* parseBetweenExpr();
    parser::v3::Expression* parseLikeExpr();
    parser::v3::Expression* parseBitwiseOrExpr();
    parser::v3::Expression* parseBitwiseXorExpr();
    parser::v3::Expression* parseBitwiseAndExpr();
    parser::v3::Expression* parseShiftExpr();
    parser::v3::Expression* parseAdditiveExpr();
    parser::v3::Expression* parseMultiplicativeExpr();
    parser::v3::Expression* parseUnaryExpr();
    parser::v3::Expression* parsePostfixExpr();
    parser::v3::Expression* parsePostfixTail(parser::v3::Expression* base);
    parser::v3::Expression* parsePrimaryExpr();
    parser::v3::Expression* parseFunctionCall(const std::string& name);
    parser::v3::Expression* parseCaseExpr();
    parser::v3::Expression* parseCastExpr();
    parser::v3::Expression* parseExtractExpr();
    parser::v3::Expression* parseAlterElementExpr();
    parser::v3::ElementSelector parseElementSelector();
    parser::v3::Expression* parseArrayConstructor();
    parser::v3::SelectStmt* parseSubquery();
    parser::v3::Expression* parseTypeCast(parser::v3::Expression* base);  // For :: operator

    // Identifier helpers
    std::string parseIdentifier();
    parser::v3::StringPool::StringId parseIdentifierId();
    std::string parseQualifiedName();
    void resolveTableName(std::string& schema, std::string& table);
    parser::v3::ASTArena* arena() { return arena_.get(); }
    bool isNonReservedKeyword(TokenType type) const;

    parser::v3::StringPool::StringId internFromLexer(uint32_t lexer_id);
    std::vector<uint8_t> captureExpressionBytecode();

    parser::v3::Expression* makeBinary(parser::v3::BinaryOp op,
                                       parser::v3::Expression* left,
                                       parser::v3::Expression* right);
    parser::v3::Expression* makeUnary(parser::v3::UnaryOp op,
                                      parser::v3::Expression* operand);
    parser::v3::Expression* makeLiteralInt(int64_t value);
    parser::v3::Expression* makeLiteralFloat(double value);
    parser::v3::Expression* makeLiteralString(const std::string& value);
    parser::v3::Expression* makeLiteralBool(bool value);
    parser::v3::Expression* makeLiteralNull();
    parser::v3::Expression* makeColumnRef(const std::vector<std::string>& parts);

};

} // namespace scratchbird::parser::postgresql
