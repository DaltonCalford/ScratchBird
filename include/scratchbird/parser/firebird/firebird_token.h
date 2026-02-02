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
 * Firebird SQL Lexer - Token Definitions
 *
 * This file defines all token types for the Firebird 5.0 SQL parser.
 * Unlike ScratchBird's "Gatekeeper" model with ~50 reserved words,
 * Firebird has ~200 reserved words that cannot be used as identifiers
 * without double-quoting.
 *
 * Reference: Firebird 5.0 Language Reference, Appendix C
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>

namespace scratchbird::parser::firebird {

/**
 * SQL Dialect for Firebird compatibility
 */
enum class SQLDialect : uint8_t {
    DIALECT_1 = 1,  // Legacy: DATE is TIMESTAMP, no quoted identifiers
    DIALECT_2 = 2,  // Transitional
    DIALECT_3 = 3   // Modern: strict types, quoted identifiers
};

/**
 * Firebird Token Types
 *
 * Categories:
 * - Special tokens (EOF, ERROR)
 * - Literals (numbers, strings, identifiers)
 * - Operators and punctuation
 * - Reserved keywords (~200)
 */
enum class TokenType : uint16_t {
    // ===== Special Tokens =====
    END_OF_FILE = 0,
    ERROR,

    // ===== Literals =====
    INTEGER_LITERAL,      // 123, 0x1A
    FLOAT_LITERAL,        // 123.456, 1.23e10
    STRING_LITERAL,       // 'hello', 'it''s'
    Q_STRING_LITERAL,     // Q'{text}', Q'[text]', etc.
    BLOB_LITERAL,         // X'DEADBEEF'
    IDENTIFIER,           // column_name
    QUOTED_IDENTIFIER,    // "Quoted Identifier"
    PARAMETER,            // :named_param, ?

    // ===== Operators =====
    // Arithmetic
    PLUS,                 // +
    MINUS,                // -
    STAR,                 // *
    SLASH,                // /
    DOUBLE_PIPE,          // || (concatenation)

    // Comparison
    EQUAL,                // =
    NOT_EQUAL,            // <>
    NOT_EQUAL_BANG,       // !=
    LESS_THAN,            // <
    GREATER_THAN,         // >
    LESS_EQUAL,           // <=
    GREATER_EQUAL,        // >=

    // Firebird-specific comparison operators
    NOT_LESS,             // !< or ~< or ^<
    NOT_GREATER,          // !> or ~> or ^>
    NOT_EQUAL_TILDE,      // ~=
    NOT_EQUAL_CARET,      // ^=

    // Assignment
    COLON_EQUALS,         // := (PSQL assignment)

    // ===== Punctuation =====
    LEFT_PAREN,           // (
    RIGHT_PAREN,          // )
    LEFT_BRACKET,         // [
    RIGHT_BRACKET,        // ]
    COMMA,                // ,
    SEMICOLON,            // ;
    DOT,                  // .
    COLON,                // :

    // ===== Reserved Keywords (Firebird 5.0) =====
    // Alphabetically ordered for easy lookup

    KW_ADD,
    KW_ADMIN,
    KW_ALL,
    KW_ALTER,
    KW_ALTER_ELEMENT,
    KW_AND,
    KW_ANY,
    KW_AS,
    KW_AT,
    KW_AVG,
    KW_BEGIN,
    KW_BETWEEN,
    KW_BIGINT,
    KW_BINARY,
    KW_BIT_LENGTH,
    KW_BLOB,
    KW_BOOLEAN,
    KW_BOTH,
    KW_BY,
    KW_CASE,
    KW_CAST,
    KW_CHAR,
    KW_CHARACTER,
    KW_CHARACTER_LENGTH,
    KW_CHAR_LENGTH,
    KW_CHECK,
    KW_CLOSE,
    KW_COLLATE,
    KW_COLUMN,
    KW_COMMENT,
    KW_COMMIT,
    KW_CONNECT,
    KW_CONSTRAINT,
    KW_CORR,
    KW_COUNT,
    KW_COVAR_POP,
    KW_COVAR_SAMP,
    KW_CREATE,
    KW_CROSS,
    KW_CURRENT,
    KW_CURRENT_CONNECTION,
    KW_CURRENT_DATE,
    KW_CURRENT_ROLE,
    KW_CURRENT_TIME,
    KW_CURRENT_TIMESTAMP,
    KW_CURRENT_TRANSACTION,
    KW_CURRENT_USER,
    KW_CURSOR,
    KW_DATE,
    KW_DAY,
    KW_DEC,
    KW_DECFLOAT,
    KW_DECIMAL,
    KW_DECLARE,
    KW_DEFAULT,
    KW_DELETE,
    KW_DELETING,
    KW_DETERMINISTIC,
    KW_DISCONNECT,
    KW_DISTINCT,
    KW_DOUBLE,
    KW_DROP,
    KW_ELSE,
    KW_END,
    KW_ESCAPE,
    KW_EXECUTE,
    KW_EXISTS,
    KW_EXTERNAL,
    KW_EXTRACT,
    KW_FALSE,
    KW_FETCH,
    KW_FILTER,
    KW_FLOAT,
    KW_FOR,
    KW_FOREIGN,
    KW_FROM,
    KW_FULL,
    KW_FUNCTION,
    KW_GDSCODE,
    KW_GLOBAL,
    KW_GRANT,
    KW_GROUP,
    KW_HAVING,
    KW_HOUR,
    KW_IN,
    KW_INDEX,
    KW_INNER,
    KW_INSENSITIVE,
    KW_INSERT,
    KW_INSERTING,
    KW_INT,
    KW_INT128,
    KW_UINT128,
    KW_INTEGER,
    KW_INTO,
    KW_IS,
    KW_JOIN,
    KW_LATERAL,
    KW_LEADING,
    KW_LEFT,
    KW_LIKE,
    KW_LOCAL,
    KW_LOCALTIME,
    KW_LOCALTIMESTAMP,
    KW_LOOP,
    KW_LONG,
    KW_LOWER,
    KW_MAX,
    KW_MERGE,
    KW_MIN,
    KW_MINUTE,
    KW_MONTH,
    KW_NATIONAL,
    KW_NATURAL,
    KW_NCHAR,
    KW_NO,
    KW_NOT,
    KW_NULL,
    KW_NUMERIC,
    KW_OCTET_LENGTH,
    KW_OF,
    KW_OFFSET,
    KW_ON,
    KW_ONLY,
    KW_OPEN,
    KW_OR,
    KW_ORDER,
    KW_OUTER,
    KW_OVER,
    KW_PARAMETER,
    KW_PLAN,
    KW_POSITION,
    KW_POST_EVENT,
    KW_PRECISION,
    KW_PRIMARY,
    KW_PROCEDURE,
    KW_PUBLICATION,
    KW_RDB_DB_KEY,
    KW_RDB_ERROR,
    KW_RDB_GET_CONTEXT,
    KW_RDB_GET_TRANSACTION_CN,
    KW_RDB_RECORD_VERSION,
    KW_RDB_ROLE_IN_USE,
    KW_RDB_SET_CONTEXT,
    KW_RDB_SYSTEM_PRIVILEGE,
    KW_REAL,
    KW_RECORD_VERSION,
    KW_RECREATE,
    KW_RECURSIVE,
    KW_REFERENCES,
    KW_REGR_AVGX,
    KW_REGR_AVGY,
    KW_REGR_COUNT,
    KW_REGR_INTERCEPT,
    KW_REGR_R2,
    KW_REGR_SLOPE,
    KW_REGR_SXX,
    KW_REGR_SXY,
    KW_REGR_SYY,
    KW_RELEASE,
    KW_RESETTING,
    KW_RETURN,
    KW_RETURNING_VALUES,
    KW_RETURNS,
    KW_REVOKE,
    KW_RIGHT,
    KW_ROLLBACK,
    KW_ROW,
    KW_ROWS,
    KW_ROW_COUNT,
    KW_SAVEPOINT,
    KW_SCROLL,
    KW_SECOND,
    KW_SELECT,
    KW_SENSITIVE,
    KW_SET,
    KW_SIMILAR,
    KW_SMALLINT,
    KW_SOME,
    KW_SQLCODE,
    KW_SQLSTATE,
    KW_START,
    KW_STDDEV_POP,
    KW_STDDEV_SAMP,
    KW_SUM,
    KW_TABLE,
    KW_THEN,
    KW_TIME,
    KW_TIMESTAMP,
    KW_TIMEZONE_HOUR,
    KW_TIMEZONE_MINUTE,
    KW_TO,
    KW_TRAILING,
    KW_TRIGGER,
    KW_TRIM,
    KW_TRUE,
    KW_UNBOUNDED,
    KW_UNION,
    KW_UNIQUE,
    KW_UNKNOWN,
    KW_UPDATE,
    KW_UPDATING,
    KW_UPPER,
    KW_USER,
    KW_USING,
    KW_VALUE,
    KW_VALUES,
    KW_VARBINARY,
    KW_VARCHAR,
    KW_VARIABLE,
    KW_VARYING,
    KW_VAR_POP,
    KW_VAR_SAMP,
    KW_VIEW,
    KW_WHEN,
    KW_WHERE,
    KW_WHILE,
    KW_WINDOW,
    KW_WITH,
    KW_WITHOUT,
    KW_YEAR,

    // ===== Non-Reserved Keywords (contextual) =====
    // These are recognized as keywords in specific contexts
    // but can be used as identifiers without quoting

    KW_ABS,
    KW_ABSOLUTE,
    KW_ACCENT,
    KW_ACOS,
    KW_ACOSH,
    KW_ACTION,
    KW_ACTIVE,
    KW_AFTER,
    KW_ALWAYS,
    KW_ASC,
    KW_ASCENDING,
    KW_ASCII_CHAR,
    KW_ASCII_VAL,
    KW_ASIN,
    KW_ASINH,
    KW_ATAN,
    KW_ATAN2,
    KW_ATANH,
    KW_AUTO,
    KW_AUTONOMOUS,
    KW_BACKUP,
    KW_BASE64_DECODE,
    KW_BASE64_ENCODE,
    KW_BEFORE,
    KW_BIND,
    KW_BIN_AND,
    KW_BIN_NOT,
    KW_BIN_OR,
    KW_BIN_SHL,
    KW_BIN_SHR,
    KW_BIN_XOR,
    KW_BLOB_APPEND,
    KW_BLOCK,
    KW_BODY,
    KW_BREAK,
    KW_CALLER,
    KW_CASCADE,
    KW_CEIL,
    KW_CEILING,
    KW_CHAR_TO_UUID,
    KW_CLEAR,
    KW_COALESCE,
    KW_COLLATION,
    KW_COMMITTED,
    KW_COMMON,
    KW_COMPARE_DECFLOAT,
    KW_COMPUTED,
    KW_CONDITIONAL,
    KW_CONNECTIONS,
    KW_CONSISTENCY,
    KW_CONTAINING,
    KW_CONTINUE,
    KW_COS,
    KW_COSH,
    KW_COT,
    KW_COUNTER,
    KW_CRYPT_HASH,
    KW_CSTRING,
    KW_CTR_BIG_ENDIAN,
    KW_CTR_LENGTH,
    KW_CTR_LITTLE_ENDIAN,
    KW_CUME_DIST,
    KW_DATA,
    KW_DATABASE,
    KW_DATEADD,
    KW_DATEDIFF,
    KW_DDL,
    KW_DEBUG,
    KW_DECODE,
    KW_DECRYPT,
    KW_DEFINER,
    KW_DENSE_RANK,
    KW_DESC,
    KW_DESCENDING,
    KW_DESCRIPTOR,
    KW_DIFFERENCE,
    KW_DISABLE,
    KW_DO,
    KW_DOMAIN,
    KW_ENABLE,
    KW_ENCRYPT,
    KW_ENGINE,
    KW_ENTRY_POINT,
    KW_EXCEPTION,
    KW_EXCESS,
    KW_EXCLUDE,
    KW_EXIT,
    KW_EXP,
    KW_EXTENDED,
    KW_FILE,
    KW_FIRST,
    KW_FIRSTNAME,
    KW_FIRST_DAY,
    KW_FIRST_VALUE,
    KW_FLOOR,
    KW_FOLLOWING,
    KW_FREE_IT,
    KW_GENERATED,
    KW_GENERATOR,
    KW_GEN_ID,
    KW_GEN_UUID,
    KW_GRANTED,
    KW_HASH,
    KW_HEX_DECODE,
    KW_HEX_ENCODE,
    KW_IDENTITY,
    KW_IDLE,
    KW_IF,
    KW_IGNORE,
    KW_IIF,
    KW_INACTIVE,
    KW_INCLUDE,
    KW_INCREMENT,
    KW_INPUT_TYPE,
    KW_INVOKER,
    KW_ISOLATION,
    KW_IV,
    KW_KEY,
    KW_LAG,
    KW_LAST,
    KW_LASTNAME,
    KW_LAST_DAY,
    KW_LAST_VALUE,
    KW_LEAD,
    KW_LEAVE,
    KW_LEGACY,
    KW_LENGTH,
    KW_LEVEL,
    KW_LIFETIME,
    KW_LIMBO,
    KW_LINGER,
    KW_LIST,
    KW_LN,
    KW_LOCK,
    KW_LOCKED,
    KW_LOG,
    KW_LOG10,
    KW_LPAD,
    KW_LPARAM,
    KW_MAKE_DBKEY,
    KW_MANUAL,
    KW_MAPPING,
    KW_MATCHED,
    KW_MATCHING,
    KW_MAXVALUE,
    KW_MESSAGE,
    KW_MIDDLENAME,
    KW_MILLISECOND,
    KW_MINVALUE,
    KW_MOD,
    KW_MODE,
    KW_MODULE_NAME,
    KW_NAME,
    KW_NAMES,
    KW_NATIVE,
    KW_NEXT,
    KW_NORMALIZE_DECFLOAT,
    KW_NTH_VALUE,
    KW_NTILE,
    KW_NULLIF,
    KW_NULLS,
    KW_NUMBER,
    KW_OLDEST,
    KW_OPTION,
    KW_OS_NAME,
    KW_OTHERS,
    KW_OUTPUT_TYPE,
    KW_OVERFLOW,
    KW_OVERLAY,
    KW_OVERRIDING,
    KW_PACKAGE,
    KW_PAD,
    KW_PAGE,
    KW_PAGES,
    KW_PAGE_SIZE,
    KW_PARTITION,
    KW_PASSWORD,
    KW_PERCENT_RANK,
    KW_PI,
    KW_PKCS_1_5,
    KW_PLACING,
    KW_PLUGIN,
    KW_POOL,
    KW_POWER,
    KW_PRECEDING,
    KW_PRESERVE,
    KW_PRIOR,
    KW_PRIVILEGE,
    KW_PRIVILEGES,
    KW_PROTECTED,
    KW_QUANTIZE,
    KW_RAND,
    KW_RANGE,
    KW_RANK,
    KW_READ,
    KW_RELATIVE,
    KW_RENAME,
    KW_REPLACE,
    KW_REQUESTS,
    KW_RESERV,
    KW_RESERVING,
    KW_RESET,
    KW_RESTART,
    KW_RESTRICT,
    KW_RETAIN,
    KW_RETURNING,
    KW_REVERSE,
    KW_ROLE,
    KW_ROUND,
    KW_ROW_NUMBER,
    KW_RPAD,
    KW_RSA_DECRYPT,
    KW_RSA_ENCRYPT,
    KW_RSA_PRIVATE,
    KW_RSA_PUBLIC,
    KW_RSA_SIGN_HASH,
    KW_RSA_VERIFY_HASH,
    KW_SALT_LENGTH,
    KW_SCALAR_ARRAY,
    KW_SCHEMA,
    KW_SECURITY,
    KW_SEGMENT,
    KW_SEQUENCE,
    KW_SERVERWIDE,
    KW_SESSION,
    KW_SHADOW,
    KW_SHARED,
    KW_SHOW,
    KW_SIGN,
    KW_SIGNATURE,
    KW_SIN,
    KW_SINGULAR,
    KW_SINH,
    KW_SIZE,
    KW_SKIP,
    KW_SNAPSHOT,
    KW_SORT,
    KW_SOURCE,
    KW_SPACE,
    KW_SQL,
    KW_SQRT,
    KW_STARTING,
    KW_STATEMENT,
    KW_STATISTICS,
    KW_SUB_TYPE,
    KW_SUSPEND,
    KW_SYSTEM,
    KW_TAGS,
    KW_TAN,
    KW_TANH,
    KW_TEMPORARY,
    KW_TIES,
    KW_TIMEOUT,
    KW_TRANSACTION,
    KW_TRAPS,
    KW_TRUSTED,
    KW_TRUNC,
    KW_TYPE,
    KW_UNCOMMITTED,
    KW_UNDO,
    KW_UNICODE_CHAR,
    KW_UNICODE_VAL,
    KW_UUID_TO_CHAR,
    KW_WAIT,
    KW_WEEK,
    KW_WEEKDAY,
    KW_WORK,
    KW_WRITE,
    KW_YEARDAY,
    KW_ZONE,

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
 * String pool for interning identifiers and string literals
 */
class StringPool {
public:
    using StringId = uint32_t;
    static constexpr StringId INVALID_ID = 0;

    StringPool();
    ~StringPool();

    // Intern a string, returning its ID
    StringId intern(std::string_view str);

    // Get string by ID (returns empty string_view for invalid ID)
    std::string_view get(StringId id) const;

    // Clear all interned strings
    void clear();

    // Get number of interned strings
    size_t size() const { return strings_.size(); }

private:
    std::vector<std::string> strings_;
    std::unordered_map<std::string_view, StringId> lookup_;
};

/**
 * Token structure
 */
struct Token {
    TokenType type;
    SourceSpan span;

    // Identifier handling:
    // - Unquoted: case-insensitive (stored as-is, compared uppercase)
    // - "Quoted": case-sensitive (delimited identifier)
    bool is_delimited;

    // Value union
    union {
        int64_t int_value;              // INTEGER_LITERAL
        double float_value;             // FLOAT_LITERAL
        StringPool::StringId string_id; // IDENTIFIER, STRING_LITERAL, etc.
    } value;

    Token();

    // Factory methods
    static Token makeEOF(SourceLocation loc);
    static Token makeError(SourceLocation loc, uint32_t len);
    static Token makeInteger(SourceLocation loc, uint32_t len, int64_t val);
    static Token makeFloat(SourceLocation loc, uint32_t len, double val);
    static Token makeString(SourceLocation loc, uint32_t len, StringPool::StringId id);
    static Token makeQString(SourceLocation loc, uint32_t len, StringPool::StringId id);
    static Token makeBlob(SourceLocation loc, uint32_t len, StringPool::StringId id);
    static Token makeIdentifier(SourceLocation loc, uint32_t len, StringPool::StringId id, bool delimited = false);
    static Token makeParameter(SourceLocation loc, uint32_t len, StringPool::StringId id);
    static Token makeKeyword(SourceLocation loc, uint32_t len, TokenType kwType);
    static Token makeOperator(SourceLocation loc, uint32_t len, TokenType opType);
    static Token makePunctuation(SourceLocation loc, uint32_t len, TokenType punctType);
};

/**
 * Error information from lexer
 */
struct LexerError {
    SourceSpan span;
    std::string message;
    std::string hint;
};

/**
 * Convert token type to string for debugging
 */
const char* tokenTypeToString(TokenType type);

/**
 * Check if a token type is a reserved keyword
 */
bool isReservedKeyword(TokenType type);

/**
 * Check if a token type is a non-reserved keyword
 */
bool isNonReservedKeyword(TokenType type);

/**
 * Check if a token type is an operator
 */
bool isOperator(TokenType type);

/**
 * Check if a token type is punctuation
 */
bool isPunctuation(TokenType type);

} // namespace scratchbird::parser::firebird
