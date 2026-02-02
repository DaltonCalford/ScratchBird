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
 * PostgreSQL Parser Token Types
 *
 * Token types for the PostgreSQL 16 emulation parser.
 * This is SEPARATE from the ScratchBird V2 parser tokens.
 *
 * PostgreSQL has ~130 keywords with ~96 reserved.
 * Unlike MySQL's ~262 reserved keywords, PostgreSQL is more permissive
 * with identifier naming.
 *
 * Key PostgreSQL-specific features:
 * - Double-quoted identifiers ("identifier") vs MySQL's backticks
 * - Type cast operator (::)
 * - Dollar-quoted strings ($...$, $tag$...$tag$)
 * - E'...' escape strings
 * - DISTINCT ON clause
 * - Extensive JSON/JSONB operators
 * - Array operators (@>, <@, &&, ||)
 * - Range operators
 */

#include <cstdint>
#include <string_view>

namespace scratchbird::parser::postgresql {

/**
 * PostgreSQL Token Types
 */
enum class TokenType : uint16_t {
    // ===== Special Tokens =====
    END_OF_FILE = 0,
    ERROR,

    // ===== Literals =====
    INTEGER_LITERAL,       // 123
    FLOAT_LITERAL,         // 123.456, 1.23e10
    STRING_LITERAL,        // 'hello'
    DOLLAR_STRING,         // $$hello$$, $tag$hello$tag$
    ESCAPE_STRING,         // E'hello\n'
    BIT_STRING,            // B'1010'
    HEX_STRING,            // X'1A2B'
    IDENTIFIER,            // column_name (lowercase by default)
    QUOTED_IDENTIFIER,     // "Identifier" (preserves case)
    PARAMETER,             // $1, $2, etc.

    // ===== Operators =====
    // Arithmetic
    PLUS,                  // +
    MINUS,                 // -
    STAR,                  // *
    SLASH,                 // /
    PERCENT,               // %
    CARET,                 // ^ (exponentiation)

    // Comparison
    EQUAL,                 // =
    NOT_EQUAL,             // <> or !=
    LESS_THAN,             // <
    GREATER_THAN,          // >
    LESS_EQUAL,            // <=
    GREATER_EQUAL,         // >=

    // String
    DOUBLE_PIPE,           // || (concatenation)

    // Bitwise
    AMPERSAND,             // &
    PIPE,                  // |
    TILDE,                 // ~
    SHIFT_LEFT,            // <<
    SHIFT_RIGHT,           // >>
    HASH,                  // # (bitwise XOR)

    // Type cast
    DOUBLE_COLON,          // :: (type cast)

    // JSON operators
    ARROW,                 // ->
    DOUBLE_ARROW,          // ->>
    HASH_ARROW,            // #>
    HASH_DOUBLE_ARROW,     // #>>
    AT_GREATER,            // @> (contains)
    LESS_AT,               // <@ (contained by)
    QUESTION,              // ? (key exists)
    QUESTION_PIPE,         // ?| (any key exists)
    QUESTION_AMPERSAND,    // ?& (all keys exist)
    AT_QUESTION,           // @? (JSON path exists)
    AT_AT,                 // @@ (JSON path match / text search match)

    // Array operators
    DOUBLE_AMPERSAND,      // && (overlap)

    // Range operators
    MINUS_PIPE_MINUS,      // -|- (adjacent)

    // Regex operators
    TILDE_STAR,            // ~* (case-insensitive match)
    EXCLAIM_TILDE,         // !~ (not match)
    EXCLAIM_TILDE_STAR,    // !~* (case-insensitive not match)

    // Square root / Cube root
    PIPE_SLASH,            // |/ (square root)
    DOUBLE_PIPE_SLASH,     // ||/ (cube root)

    // Factorial
    EXCLAIM,               // ! (factorial, postfix or prefix)
    DOUBLE_EXCLAIM,        // !! (factorial prefix)

    // Absolute value
    AT_SIGN,               // @ (absolute value)

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

    // ===== PostgreSQL Reserved Keywords =====
    // These cannot be used as identifiers without double-quoting

    // Statement initiators
    KW_SELECT,
    KW_INSERT,
    KW_UPDATE,
    KW_DELETE,
    KW_CREATE,
    KW_ALTER,
    KW_DROP,
    KW_TRUNCATE,
    KW_GRANT,
    KW_REVOKE,
    KW_MERGE,
    KW_ANALYZE,
    KW_EXPLAIN,
    KW_CALL,
    KW_DO,

    // Clause keywords
    KW_FROM,
    KW_WHERE,
    KW_GROUP,
    KW_HAVING,
    KW_ORDER,
    KW_BY,
    KW_LIMIT,
    KW_OFFSET,
    KW_FETCH,
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
    KW_FULL,
    KW_CROSS,
    KW_NATURAL,
    KW_OUTER,
    KW_ON,
    KW_USING,
    KW_LATERAL,

    // Expression keywords
    KW_AND,
    KW_OR,
    KW_NOT,
    KW_IS,
    KW_IN,
    KW_BETWEEN,
    KW_LIKE,
    KW_ILIKE,
    KW_SIMILAR,
    KW_CASE,
    KW_WHEN,
    KW_THEN,
    KW_ELSE,
    KW_END,
    KW_NULL,
    KW_TRUE,
    KW_FALSE,
    KW_EXISTS,
    KW_CAST,
    KW_AS,
    KW_ANY,
    KW_SOME,
    KW_ALL,

    // DML keywords
    KW_INTO,
    KW_VALUES,
    KW_DEFAULT,
    KW_RETURNING,
    KW_CONFLICT,
    KW_NOTHING,
    KW_SET,
    KW_ONLY,

    // DDL keywords
    KW_TABLE,
    KW_DATABASE,
    KW_SCHEMA,
    KW_INDEX,
    KW_VIEW,
    KW_MATERIALIZED,
    KW_SEQUENCE,
    KW_FUNCTION,
    KW_PROCEDURE,
    KW_TRIGGER,
    KW_TYPE,
    KW_TYPES,
    KW_DOMAIN,
    KW_CONSTRAINT,
    KW_COLUMN,
    KW_ADD,
    KW_RENAME,
    KW_TO,
    KW_IF,
    KW_TEMPORARY,
    KW_TEMP,
    KW_UNLOGGED,
    KW_UNIQUE,
    KW_PRIMARY,
    KW_KEY,
    KW_FOREIGN,
    KW_REFERENCES,
    KW_CHECK,
    KW_CASCADE,
    KW_RESTRICT,
    KW_LOCATION,
    KW_UNLIMITED,
    KW_NO,
    KW_ACTION,
    KW_INITIALLY,
    KW_DEFERRED,
    KW_IMMEDIATE,
    KW_DEFERRABLE,
    KW_CONCURRENTLY,
    KW_INCLUDE,
    KW_NULLS,
    KW_INHERITS,
    KW_PARTITION,
    KW_RANGE,
    KW_LIST,
    KW_HASH,
    KW_FOR,
    KW_COLLATE,
    KW_GENERATED,
    KW_ALWAYS,
    KW_IDENTITY,
    KW_STORED,
    KW_REPLACE,
    // Note: KW_OR already defined in Expression keywords section

    // Type keywords
    KW_SMALLINT,
    KW_INT,
    KW_INTEGER,
    KW_BIGINT,
    KW_INT128,
    KW_UINT128,
    KW_REAL,
    KW_DOUBLE,
    KW_PRECISION,
    KW_DECIMAL,
    KW_NUMERIC,
    KW_SMALLSERIAL,
    KW_SERIAL,
    KW_BIGSERIAL,
    KW_MONEY,
    KW_CHAR,
    KW_CHARACTER,
    KW_VARCHAR,
    KW_VARYING,
    KW_TEXT,
    KW_BYTEA,
    KW_DATE,
    KW_TIME,
    KW_TIMESTAMP,
    KW_INTERVAL,
    KW_BOOLEAN,
    KW_BOOL,
    KW_UUID,
    KW_JSON,
    KW_JSONB,
    KW_JSONPATH,
    KW_XML,
    KW_ARRAY,
    KW_POINT,
    KW_LINE,
    KW_LSEG,
    KW_BOX,
    KW_PATH,
    KW_POLYGON,
    KW_CIRCLE,
    KW_CIDR,
    KW_INET,
    KW_MACADDR,
    KW_MACADDR8,
    KW_BIT,
    KW_TSVECTOR,
    KW_TSQUERY,
    KW_INT4RANGE,
    KW_INT8RANGE,
    KW_NUMRANGE,
    KW_DATERANGE,
    KW_TSRANGE,
    KW_TSTZRANGE,
    KW_OID,
    KW_REGCLASS,
    KW_REGTYPE,
    KW_WITHOUT,
    KW_ZONE,

    // Aggregate/Window keywords
    KW_DISTINCT,
    KW_ASC,
    KW_DESC,
    KW_FIRST,
    KW_LAST,
    KW_ROLLUP,
    KW_CUBE,
    KW_GROUPING,
    KW_SETS,
    KW_OVER,
    KW_WINDOW,
    KW_ROWS,
    KW_GROUPS,
    KW_UNBOUNDED,
    KW_PRECEDING,
    KW_FOLLOWING,
    KW_CURRENT,
    KW_ROW,
    KW_TIES,
    KW_EXCLUDE,

    // Transaction keywords
    KW_BEGIN,
    KW_START,
    KW_TRANSACTION,
    KW_COMMIT,
    KW_ROLLBACK,
    KW_SAVEPOINT,
    KW_RELEASE,
    KW_WORK,
    KW_READ,
    KW_WRITE,
    KW_COMMITTED,
    KW_UNCOMMITTED,
    KW_REPEATABLE,
    KW_SERIALIZABLE,
    KW_ISOLATION,
    KW_LEVEL,

    // PL/pgSQL keywords
    KW_DECLARE,
    KW_RETURN,
    KW_RETURNS,
    KW_LANGUAGE,
    KW_PLPGSQL,
    KW_SQL,
    KW_IMMUTABLE,
    KW_STABLE,
    KW_VOLATILE,
    KW_STRICT,
    KW_CALLED,
    KW_INPUT,
    KW_SECURITY,
    KW_DEFINER,
    KW_INVOKER,
    KW_EXTERNAL,
    KW_COST,
    KW_PARALLEL,
    KW_SAFE,
    KW_RESTRICTED,
    KW_UNSAFE,
    KW_LEAKPROOF,
    KW_LOOP,
    KW_WHILE,
    KW_EXIT,
    KW_CONTINUE,
    KW_FOREACH,
    KW_SLICE,
    KW_RAISE,
    KW_NOTICE,
    KW_WARNING,
    KW_EXCEPTION,
    KW_DEBUG,
    KW_LOG,
    KW_INFO,
    KW_ASSERT,
    KW_GET,
    KW_DIAGNOSTICS,
    KW_STACKED,
    KW_CURSOR,
    KW_SCROLL,
    KW_HOLD,
    KW_MOVE,
    KW_CLOSE,
    KW_OPEN,

    // Trigger keywords
    KW_BEFORE,
    KW_AFTER,
    KW_INSTEAD,
    KW_OF,
    KW_EACH,
    KW_STATEMENT,
    KW_EXECUTE,
    KW_NEW,
    KW_OLD,
    KW_REFERENCING,

    // Security keywords
    KW_USER,
    KW_ROLE,
    KW_POLICY,
    // Note: KW_GROUP already defined in Clause keywords section
    KW_PUBLIC,
    KW_PRIVILEGES,
    KW_OPTION,
    KW_ADMIN,
    KW_PASSWORD,
    KW_SUPERUSER,
    KW_NOSUPERUSER,
    KW_CREATEDB,
    KW_NOCREATEDB,
    KW_CREATEROLE,
    KW_NOCREATEROLE,
    KW_LOGIN,
    KW_NOLOGIN,
    KW_REPLICATION,
    KW_NOREPLICATION,
    KW_INHERIT,
    KW_NOINHERIT,
    KW_BYPASSRLS,
    KW_NOBYPASSRLS,
    KW_VALID,
    KW_UNTIL,
    KW_CONNECTION,

    // System/Session keywords
    KW_LOCAL,
    KW_SESSION,
    KW_CURRENT_USER,
    KW_SESSION_USER,
    KW_CURRENT_ROLE,
    KW_CURRENT_SCHEMA,
    KW_CURRENT_CATALOG,
    KW_CURRENT_DATE,
    KW_CURRENT_TIME,
    KW_CURRENT_TIMESTAMP,
    KW_LOCALTIME,
    KW_LOCALTIMESTAMP,

    // Misc keywords
    KW_ANALYZE_KW,
    KW_VERBOSE,
    KW_FORCE,
    KW_ENABLE,
    KW_DISABLE,
    KW_MATCH,
    KW_PARTIAL,
    KW_SIMPLE,
    KW_OWNER,
    KW_OWNED,
    KW_NONE,
    KW_EXTRACT,
    KW_ALTER_ELEMENT,
    KW_POSITION,
    KW_SUBSTRING,
    KW_TRIM,
    KW_LEADING,
    KW_TRAILING,
    KW_BOTH,
    KW_COALESCE,
    KW_NULLIF,
    KW_GREATEST,
    KW_LEAST,
    KW_NORMALIZE,
    KW_NORMALIZED,
    KW_NFC,
    KW_NFD,
    KW_NFKC,
    KW_NFKD,
    KW_OVERLAPS,
    KW_PLACING,
    KW_SYMMETRIC,
    KW_ASYMMETRIC,
    KW_FREEZE,
    KW_TABLESAMPLE,
    KW_WITHIN,
    KW_VARIADIC,

    // Copy keywords
    KW_COPY,
    KW_STDIN,
    KW_STDOUT,
    KW_DELIMITER,
    KW_CSV,
    KW_HEADER,
    KW_QUOTE,
    KW_ESCAPE,
    KW_ENCODING,
    KW_FORMAT,

    // Enum keywords
    KW_ENUM,

    // Additional keywords needed by parser
    KW_NAME,
    KW_VALUE,
    KW_DATA,
    KW_YEAR,
    KW_MONTH,
    KW_DAY,
    KW_HOUR,
    KW_MINUTE,
    KW_SECOND,
    KW_OPTIONS,
    KW_COMMENT,
    KW_VERSION,
    KW_MODE,
    KW_NEXT,
    KW_PRIOR,
    KW_ABSOLUTE,
    KW_RELATIVE,
    KW_SHOW,
    KW_UNKNOWN,
    KW_COUNT,
    KW_TABLESPACE,
    KW_VIRTUAL,
    KW_INT2,
    KW_INT4,
    KW_INT8,
    KW_FLOAT4,
    KW_FLOAT8,
    KW_SERIAL2,
    KW_SERIAL4,
    KW_SERIAL8,
    KW_CASCADED,
    KW_INCREMENT,
    KW_MINVALUE,
    KW_MAXVALUE,
    KW_CYCLE,
    KW_CACHE,
    KW_TEMPLATE,
    KW_AUTHORIZATION,
    KW_OUT,
    KW_INOUT,
    KW_SETOF,
    KW_RESTART,
    KW_MATCHED,
    KW_PRESERVE,
    KW_REINDEX,
    KW_REASSIGN,
    KW_CLUSTER,
    KW_REFRESH,
    KW_DISCARD,
    KW_RESET,
    KW_LISTEN,
    KW_NOTIFY,
    KW_UNLISTEN,
    KW_PREPARE,
    KW_DEALLOCATE,
    KW_FETCH_KW,
    KW_LOCK,
    KW_SHARE,
    KW_ACCESS,
    KW_NOWAIT,
    KW_VACUUM,
    KW_WAIT,
    KW_SKIP,
    KW_LOCKED,
    KW_CONSTRAINTS,
    KW_SEARCH_PATH,
    KW_TABLES,
    KW_DATABASES,
    KW_COLUMNS,
    KW_INDEXES,
    KW_CHAIN,
    KW_USAGE,
    KW_CONNECT,
    KW_IN_KW,
    KW_SEQUENCES,
    KW_FUNCTIONS,
    KW_SCHEMAS,
    KW_ONLY_KW,
    KW_OBJECTS,
    KW_ZONE_KW,
    KW_CHARACTERISTICS,
    KW_SNAPSHOT,
    KW_COSTS,
    KW_BUFFERS,
    KW_TIMING,
    KW_OFF,

    // Additional operator tokens
    LESS,              // < (alias for LESS_THAN)
    GREATER,           // > (alias for GREATER_THAN)
    LESS_GREATER,      // <>
    LEFT_SHIFT,        // <<
    RIGHT_SHIFT,       // >>
    CONCAT,            // ||

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
        int64_t int_value;       // INTEGER_LITERAL
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

    static Token makeDollarString(SourceLocation loc, uint32_t len, uint32_t id) {
        Token t;
        t.type = TokenType::DOLLAR_STRING;
        t.span = SourceSpan(loc, len);
        t.value.string_id = id;
        return t;
    }

    static Token makeEscapeString(SourceLocation loc, uint32_t len, uint32_t id) {
        Token t;
        t.type = TokenType::ESCAPE_STRING;
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

    static Token makeQuotedIdentifier(SourceLocation loc, uint32_t len, uint32_t id) {
        Token t;
        t.type = TokenType::QUOTED_IDENTIFIER;
        t.span = SourceSpan(loc, len);
        t.value.string_id = id;
        return t;
    }

    static Token makeParameter(SourceLocation loc, uint32_t len, int64_t index) {
        Token t;
        t.type = TokenType::PARAMETER;
        t.span = SourceSpan(loc, len);
        t.value.int_value = index;
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

    static Token makePunctuation(SourceLocation loc, uint32_t len, TokenType punct) {
        Token t;
        t.type = punct;
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

} // namespace scratchbird::parser::postgresql
