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
 * Schema: Databases are emulated as schemas at:
 *   /remote/emulated/postgresql/localhost/{database}/
 *
 * Supported statements:
 * - DDL: CREATE/ALTER/DROP TABLE, INDEX, VIEW, SEQUENCE, FUNCTION, etc.
 * - DML: SELECT, INSERT, UPDATE, DELETE, MERGE
 * - Transaction: BEGIN, COMMIT, ROLLBACK, SAVEPOINT
 * - Admin: ANALYZE, EXPLAIN, SET, SHOW
 * - PL/pgSQL: Functions, Procedures (limited)
 */

#include "pg_lexer.h"
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
 * Compilation/parse result containing bytecode or errors
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

    void setBytecode(std::vector<uint8_t> bc) {
        bytecode_ = std::move(bc);
    }

private:
    std::vector<ParseError> errors_;
    std::vector<uint8_t> bytecode_;
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
           std::string_view default_schema = "/remote/emulated/postgresql/localhost/");

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
    bool resolveDomainId(const std::string& type_name, core::ID& domain_id_out);

    // Statement parsing
    void parseStatementInternal();
    void parseSelectStmt();
    void parseInsertStmt();
    void parseUpdateStmt();
    void parseDeleteStmt();
    void parseMergeStmt();
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
        std::vector<uint8_t> expr_bytecode;
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
    void parseWithClause();

    // Expression parsing (generates bytecode)
    void parseExpression();
    std::string parseExpressionText();
    void parseOrExpr();
    void parseAndExpr();
    void parseNotExpr();
    void parseComparisonExpr();
    void parseIsExpr();
    void parseInExpr();
    void parseBetweenExpr();
    void parseLikeExpr();
    void parseBitwiseOrExpr();
    void parseBitwiseXorExpr();
    void parseBitwiseAndExpr();
    void parseShiftExpr();
    void parseAdditiveExpr();
    void parseMultiplicativeExpr();
    void parseUnaryExpr();
    void parsePostfixExpr();
    void parsePostfixTail();
    void parsePrimaryExpr();
    void parseFunctionCall(const std::string& name);
    void parseCaseExpr();
    void parseCastExpr();
    void parseExtractExpr();
    void parseAlterElementExpr();
    sblr::ExtractField parseElementSelector(uint8_t& arg_count);
    void parseArrayConstructor();
    void parseSubquery();
    void parseTypeCast();  // For :: operator

    // Type conversion helpers
    sblr::Opcode typeToOpcode(PgDataType::Kind kind);

    // Identifier helpers
    std::string parseIdentifier();
    std::string parseQualifiedName();
    void resolveTableName(std::string& schema, std::string& table);
    std::vector<uint8_t> captureExpressionBytecode();
    bool isNonReservedKeyword(TokenType type) const;
};

} // namespace scratchbird::parser::postgresql
