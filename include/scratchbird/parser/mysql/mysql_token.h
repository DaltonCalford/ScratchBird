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
 * MySQL Parser Token Types
 *
 * Token types for the MySQL 8.0 emulation parser.
 * This is SEPARATE from the ScratchBird V2 parser tokens.
 *
 * MySQL has ~679 keywords with ~262 reserved.
 * Unlike the V2 "Gatekeeper" model, MySQL has many more reserved keywords
 * that cannot be used as identifiers without backtick quoting.
 */

#include <cstdint>
#include <string_view>

namespace scratchbird::parser::mysql {

/**
 * MySQL Token Types
 */
enum class TokenType : uint16_t {
    // ===== Special Tokens =====
    END_OF_FILE = 0,
    ERROR,

    // ===== Literals =====
    INTEGER_LITERAL,       // 123
    HEX_LITERAL,           // 0x1A, X'1A'
    FLOAT_LITERAL,         // 123.456, 1.23e10
    STRING_LITERAL,        // 'hello', "hello"
    BIT_LITERAL,           // b'1010', B'1010'
    IDENTIFIER,            // column_name
    BACKTICK_IDENTIFIER,   // `identifier`
    PLACEHOLDER,           // ? (prepared statement)
    USER_VARIABLE,         // @var_name
    SYSTEM_VARIABLE,       // @@var_name

    // ===== Operators =====
    // Arithmetic
    PLUS,                  // +
    MINUS,                 // -
    STAR,                  // *
    SLASH,                 // /
    DIV,                   // DIV (integer division)
    PERCENT,               // % or MOD

    // Comparison
    EQUAL,                 // =
    NOT_EQUAL,             // <> or !=
    NULL_SAFE_EQUAL,       // <=>
    LESS_THAN,             // <
    GREATER_THAN,          // >
    LESS_EQUAL,            // <=
    GREATER_EQUAL,         // >=

    // Bitwise
    AMPERSAND,             // &
    PIPE,                  // |
    CARET,                 // ^
    TILDE,                 // ~
    SHIFT_LEFT,            // <<
    SHIFT_RIGHT,           // >>

    // Logical
    AND_OP,                // &&
    OR_OP,                 // ||
    NOT_OP,                // !

    // JSON
    ARROW,                 // ->
    DOUBLE_ARROW,          // ->>

    // Assignment
    COLON_EQUAL,           // :=

    // ===== Punctuation =====
    LEFT_PAREN,            // (
    RIGHT_PAREN,           // )
    LEFT_BRACKET,          // [
    RIGHT_BRACKET,         // ]
    LEFT_BRACE,            // {
    RIGHT_BRACE,           // }
    COMMA,                 // ,
    SEMICOLON,             // ;
    DOT,                   // .
    COLON,                 // :

    // ===== MySQL Reserved Keywords =====
    // Statement initiators
    KW_SELECT,
    KW_INSERT,
    KW_UPDATE,
    KW_DELETE,
    KW_REPLACE,
    KW_CREATE,
    KW_ALTER,
    KW_DROP,
    KW_TRUNCATE,
    KW_GRANT,
    KW_REVOKE,
    KW_CALL,
    KW_USE,
    KW_SHOW,
    KW_DESCRIBE,
    KW_EXPLAIN,
    KW_SET,
    KW_RESET,
    KW_BEGIN,
    KW_START,
    KW_COMMIT,
    KW_ROLLBACK,
    KW_SAVEPOINT,
    KW_RELEASE,
    KW_LOCK,
    KW_UNLOCK,

    // Clause keywords
    KW_FROM,
    KW_WHERE,
    KW_GROUP,
    KW_HAVING,
    KW_ORDER,
    KW_BY,
    KW_LIMIT,
    KW_OFFSET,
    KW_UNLIMITED,
    KW_UNION,
    KW_INTERSECT,
    KW_EXCEPT,
    KW_WITH,
    KW_RECURSIVE,

    // Join keywords
    KW_JOIN,
    KW_INNER,
    KW_LEFT,
    KW_RIGHT,
    KW_CROSS,
    KW_OUTER,
    KW_NATURAL,
    KW_ON,
    KW_USING,
    KW_STRAIGHT_JOIN,
    KW_LATERAL,

    // Expression keywords
    KW_AND,
    KW_OR,
    KW_XOR,
    KW_NOT,
    KW_IS,
    KW_IN,
    KW_BETWEEN,
    KW_LIKE,
    KW_REGEXP,
    KW_RLIKE,
    KW_CASE,
    KW_WHEN,
    KW_THEN,
    KW_ELSE,
    KW_END,
    KW_OFF,
    KW_NULL,
    KW_TRUE,
    KW_FALSE,
    KW_EXISTS,
    KW_CAST,
    KW_EXTRACT,
    KW_CONVERT,
    KW_AS,
    KW_ESCAPE,
    KW_ALTER_ELEMENT,

    // DML keywords
    KW_INTO,
    KW_VALUES,
    KW_VALUE,
    KW_DEFAULT,
    KW_DUPLICATE,
    KW_KEY,
    KW_IGNORE,
    KW_LOW_PRIORITY,
    KW_DELAYED,
    KW_HIGH_PRIORITY,
    KW_QUICK,
    KW_RETURNING,

    // DDL keywords
    KW_TABLE,
    KW_TABLES,
    KW_DATABASE,
    KW_DATABASES,
    KW_SCHEMA,
    KW_SCHEMAS,
    KW_INDEX,
    KW_INDEXES,
    KW_VIEW,
    KW_PROCEDURE,
    KW_FUNCTION,
    KW_TRIGGER,
    KW_EVENT,
    KW_COLUMN,
    KW_COLUMNS,
    KW_ADD,
    KW_CHANGE,
    KW_MODIFY,
    KW_RENAME,
    KW_TO,
    KW_IF,
    KW_TEMPORARY,
    KW_UNIQUE,
    KW_PRIMARY,
    KW_FOREIGN,
    KW_REFERENCES,
    KW_CONSTRAINT,
    KW_CHECK,
    KW_CASCADE,
    KW_RESTRICT,
    KW_NO,
    KW_ACTION,
    KW_FULLTEXT,
    KW_SPATIAL,
    KW_HASH,
    KW_BTREE,
    KW_ENGINE,
    KW_CHARSET,
    KW_CHARACTER,
    KW_COLLATE,
    KW_AUTO_INCREMENT,
    KW_COMMENT,
    KW_PARTITION,
    KW_PARTITIONS,
    KW_ALGORITHM,
    KW_DEFINER,
    KW_INVOKER,
    KW_SQL,
    KW_SECURITY,
    KW_ENCRYPTION,

    // Type keywords
    KW_TINYINT,
    KW_SMALLINT,
    KW_MEDIUMINT,
    KW_INT,
    KW_INTEGER,
    KW_BIGINT,
    KW_INT128,
    KW_UINT128,
    KW_FLOAT,
    KW_DOUBLE,
    KW_REAL,
    KW_DECIMAL,
    KW_NUMERIC,
    KW_BIT,
    KW_BOOL,
    KW_BOOLEAN,
    KW_CHAR,
    KW_VARCHAR,
    KW_BINARY,
    KW_VARBINARY,
    KW_TINYTEXT,
    KW_TEXT,
    KW_MEDIUMTEXT,
    KW_LONGTEXT,
    KW_TINYBLOB,
    KW_BLOB,
    KW_MEDIUMBLOB,
    KW_LONGBLOB,
    KW_DATE,
    KW_TIME,
    KW_DATETIME,
    KW_TIMESTAMP,
    KW_YEAR,
    KW_ENUM,
    KW_JSON,
    KW_GEOMETRY,
    KW_POINT,
    KW_LINESTRING,
    KW_POLYGON,

    // Type modifiers
    KW_UNSIGNED,
    KW_ZEROFILL,
    KW_PRECISION,
    KW_VARYING,
    KW_ZONE,

    // Aggregate keywords
    KW_ALL,
    KW_DISTINCT,
    KW_DISTINCTROW,
    KW_ASC,
    KW_DESC,
    KW_FIRST,
    KW_LAST,
    KW_NULLS,
    KW_ROLLUP,
    KW_CUBE,
    KW_GROUPING,

    // Window function keywords
    KW_OVER,
    KW_WINDOW,
    KW_ROWS,
    KW_RANGE,
    KW_GROUPS,
    KW_UNBOUNDED,
    KW_PRECEDING,
    KW_FOLLOWING,
    KW_CURRENT,
    KW_ROW,

    // Transaction keywords
    KW_TRANSACTION,
    KW_WORK,
    KW_CHAIN,
    KW_READ,
    KW_WRITE,
    KW_ONLY,
    KW_COMMITTED,
    KW_UNCOMMITTED,
    KW_REPEATABLE,
    KW_SERIALIZABLE,
    KW_ISOLATION,
    KW_LEVEL,

    // Stored program keywords
    KW_DECLARE,
    KW_HANDLER,
    KW_CONTINUE,
    KW_EXIT,
    KW_UNDO,
    KW_SQLSTATE,
    KW_SQLEXCEPTION,
    KW_SQLWARNING,
    KW_FOUND,
    KW_FOR,
    KW_EACH,
    KW_LOOP,
    KW_WHILE,
    KW_REPEAT,
    KW_UNTIL,
    KW_LEAVE,
    KW_ITERATE,
    KW_RETURN,
    KW_RETURNS,
    KW_DETERMINISTIC,
    KW_MODIFIES,
    KW_READS,
    KW_CONTAINS,
    KW_LANGUAGE,
    KW_INOUT,
    KW_OUT,
    KW_CURSOR,
    KW_OPEN,
    KW_CLOSE,
    KW_FETCH,
    KW_SIGNAL,
    KW_RESIGNAL,
    KW_CONDITION,

    // Trigger keywords
    KW_BEFORE,
    KW_AFTER,
    KW_FOLLOWS,
    KW_PRECEDES,

    // Security keywords
    KW_USER,
    KW_ROLE,
    KW_IDENTIFIED,
    KW_PASSWORD,
    KW_GRANT_KW,  // GRANT as keyword (differentiate from statement)
    KW_REVOKE_KW,
    KW_OPTION,
    KW_ADMIN,
    KW_PUBLIC,
    KW_PRIVILEGES,
    KW_USAGE,

    // Misc keywords
    KW_FORCE,
    KW_MATCH,
    KW_FULL,
    KW_PARTIAL,
    KW_SIMPLE,
    KW_GENERATED,
    KW_ALWAYS,
    KW_STORED,
    KW_VIRTUAL,
    KW_VISIBLE,
    KW_INVISIBLE,
    KW_ANALYZE,
    KW_OPTIMIZE,
    KW_DATA,
    KW_INFILE,
    KW_OUTFILE,
    KW_DUMPFILE,
    KW_LOAD,
    KW_LINES,
    KW_TERMINATED,
    KW_ENCLOSED,
    KW_ESCAPED,
    KW_OPTIONALLY,
    KW_STARTING,
    KW_LOCAL,
    KW_GLOBAL,
    KW_SESSION,
    KW_STATUS,
    KW_VARIABLES,
    KW_PROCESSLIST,
    KW_WARNINGS,
    KW_ERRORS,
    KW_PROFILE,
    KW_PROFILES,

    // ===== End of token types =====
    TOKEN_TYPE_COUNT
};

/**
 * Source location in input text
 */
struct SourceLocation {
    uint32_t line;
    uint32_t column;
    uint32_t offset;  // Byte offset from start of input

    SourceLocation() : line(1), column(1), offset(0) {}
    SourceLocation(uint32_t l, uint32_t c, uint32_t o) : line(l), column(c), offset(o) {}
};

/**
 * Source span - range of text in input
 */
struct SourceSpan {
    SourceLocation start;
    uint32_t length;

    SourceSpan() : length(0) {}
    SourceSpan(SourceLocation s, uint32_t len) : start(s), length(len) {}
};

/**
 * Token structure
 */
struct Token {
    TokenType type;
    SourceSpan span;

    // Value storage
    union {
        int64_t int_value;       // INTEGER_LITERAL, HEX_LITERAL
        double float_value;      // FLOAT_LITERAL
        uint32_t string_id;      // IDENTIFIER, STRING_LITERAL, etc.
    } value;

    Token() : type(TokenType::ERROR), span(), value{0} {}

    static Token makeEOF(SourceLocation loc) {
        Token t;
        t.type = TokenType::END_OF_FILE;
        t.span = SourceSpan(loc, 0);
        return t;
    }

    static Token makeError(SourceLocation loc, uint32_t len) {
        Token t;
        t.type = TokenType::ERROR;
        t.span = SourceSpan(loc, len);
        return t;
    }

    static Token makeInteger(SourceLocation loc, uint32_t len, int64_t val) {
        Token t;
        t.type = TokenType::INTEGER_LITERAL;
        t.span = SourceSpan(loc, len);
        t.value.int_value = val;
        return t;
    }

    static Token makeFloat(SourceLocation loc, uint32_t len, double val) {
        Token t;
        t.type = TokenType::FLOAT_LITERAL;
        t.span = SourceSpan(loc, len);
        t.value.float_value = val;
        return t;
    }

    static Token makeString(SourceLocation loc, uint32_t len, uint32_t id) {
        Token t;
        t.type = TokenType::STRING_LITERAL;
        t.span = SourceSpan(loc, len);
        t.value.string_id = id;
        return t;
    }

    static Token makeIdentifier(SourceLocation loc, uint32_t len, uint32_t id) {
        Token t;
        t.type = TokenType::IDENTIFIER;
        t.span = SourceSpan(loc, len);
        t.value.string_id = id;
        return t;
    }

    static Token makeBacktickIdentifier(SourceLocation loc, uint32_t len, uint32_t id) {
        Token t;
        t.type = TokenType::BACKTICK_IDENTIFIER;
        t.span = SourceSpan(loc, len);
        t.value.string_id = id;
        return t;
    }

    static Token makeKeyword(SourceLocation loc, uint32_t len, TokenType kw) {
        Token t;
        t.type = kw;
        t.span = SourceSpan(loc, len);
        t.value.int_value = 0;
        return t;
    }

    static Token makeOperator(SourceLocation loc, uint32_t len, TokenType op) {
        Token t;
        t.type = op;
        t.span = SourceSpan(loc, len);
        t.value.int_value = 0;
        return t;
    }
};

/**
 * Convert token type to string for debugging
 */
const char* tokenTypeToString(TokenType type);

/**
 * Check if token type is a reserved keyword
 */
bool isReservedKeyword(TokenType type);

/**
 * Check if token type is an operator
 */
bool isOperator(TokenType type);

/**
 * Check if token type is punctuation
 */
bool isPunctuation(TokenType type);

/**
 * Check if token is a type name keyword
 */
bool isTypeKeyword(TokenType type);

} // namespace scratchbird::parser::mysql
