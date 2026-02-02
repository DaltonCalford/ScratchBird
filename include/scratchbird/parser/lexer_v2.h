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
 * ScratchBird Lexer v2.0 - "Smart Parser, Dumb Lexer" Architecture
 *
 * This lexer implements the Gatekeeper keyword model where only ~35 truly
 * reserved keywords are recognized by the lexer. All other identifiers
 * (including contextual keywords like NAME, TYPE, VALUE, DATA) are emitted
 * as IDENTIFIER tokens and resolved by the parser based on context.
 *
 * Design Goals:
 * 1. Minimize reserved keywords to maximize identifier flexibility
 * 2. Allow natural column/table names without quoting
 * 3. Simple, fast lexer with context-free tokenization
 * 4. Support for all SQL literal types and operators
 *
 * See: docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>

namespace scratchbird::parser::v2 {

// Forward declarations
class StringPool;
class ErrorReporter;

/**
 * Token Types for Lexer v2.0
 *
 * Categories:
 * - Special tokens (EOF, ERROR)
 * - Literals (numbers, strings, identifiers)
 * - Operators and punctuation
 * - Gatekeeper keywords only (~35 reserved words)
 */
enum class TokenType : uint16_t {
    // ===== Special Tokens =====
    END_OF_FILE = 0,
    ERROR,

    // ===== Literals =====
    INTEGER_LITERAL,      // 123, 0x1A, 0b1010
    FLOAT_LITERAL,        // 123.456, 1.23e10
    STRING_LITERAL,       // 'hello', E'escape\nstring'
    BLOB_LITERAL,         // X'DEADBEEF'
    IDENTIFIER,           // column_name, "Quoted Identifier"
    PARAMETER,            // $1, $2, :named_param

    // ===== Operators =====
    // Arithmetic
    PLUS,                 // +
    MINUS,                // -
    STAR,                 // *
    SLASH,                // /
    PERCENT,              // %
    CARET,                // ^ (power)
    DOUBLE_PIPE,          // || (concatenation)

    // Comparison
    EQUAL,                // =
    NOT_EQUAL,            // <> or !=
    LESS_THAN,            // <
    GREATER_THAN,         // >
    LESS_EQUAL,           // <=
    GREATER_EQUAL,        // >=

    // Bitwise
    AMPERSAND,            // &
    PIPE,                 // |
    TILDE,                // ~
    SHIFT_LEFT,           // <<
    SHIFT_RIGHT,          // >>

    // JSON operators
    ARROW,                // ->
    DOUBLE_ARROW,         // ->>
    HASH_ARROW,           // #>
    HASH_DOUBLE_ARROW,    // #>>
    AT_SIGN,              // @
    QUESTION_MARK,        // ?
    QUESTION_PIPE,        // ?|
    QUESTION_AMPERSAND,   // ?&

    // Array/Range operators
    DOUBLE_COLON,         // :: (type cast)
    AT_GREATER,           // @> (contains)
    LESS_AT,              // <@ (contained by)
    DOUBLE_AMPERSAND,     // && (overlap)
    MINUS_PIPE_MINUS,     // -|- (adjacent)

    // Regex operators
    TILDE_STAR,           // ~* (case-insensitive match)
    EXCLAIM_TILDE,        // !~ (not match)
    EXCLAIM_TILDE_STAR,   // !~* (case-insensitive not match)

    // Assignment
    COLON_EQUALS,         // :=
    EQUALS_GREATER,       // => (named parameter)

    // ===== Punctuation =====
    LEFT_PAREN,           // (
    RIGHT_PAREN,          // )
    LEFT_BRACKET,         // [
    RIGHT_BRACKET,        // ]
    LEFT_BRACE,           // {
    RIGHT_BRACE,          // }
    COMMA,                // ,
    SEMICOLON,            // ;
    DOT,                  // .
    DOUBLE_DOT,           // .. (parent schema navigation)
    EXCLAIM_COLON,        // !: (no search path prefix)
    COLON,                // :

    // ===== Gatekeeper Keywords =====
    // These are the ONLY reserved keywords recognized by the lexer.
    // All other SQL keywords are contextual and emitted as IDENTIFIER.

    // Statement initiators (must be reserved to detect statement type)
    KW_SELECT,
    KW_INSERT,
    KW_UPDATE,
    KW_DELETE,
    KW_MERGE,
    KW_CREATE,
    KW_ALTER,
    KW_DROP,
    KW_TRUNCATE,
    KW_COPY,
    KW_GRANT,
    KW_REVOKE,
    KW_COMMIT,
    KW_ROLLBACK,
    KW_BEGIN,
    KW_END,
    KW_DECLARE,
    KW_SET,
    KW_SHOW,
    KW_EXPLAIN,
    KW_ANALYZE,
    KW_CALL,
    KW_EXECUTE,
    KW_PREPARE,

    // Clause initiators (must be reserved to detect clause boundaries)
    KW_FROM,
    KW_WHERE,
    KW_GROUP,
    KW_HAVING,
    KW_ORDER,
    KW_LIMIT,
    KW_OFFSET,
    KW_UNION,
    KW_INTERSECT,
    KW_EXCEPT,
    KW_WITH,

    // Expression keywords (must be reserved for expression parsing)
    KW_AND,
    KW_OR,
    KW_NOT,
    KW_IS,
    KW_IN,
    KW_BETWEEN,
    KW_LIKE,
    KW_DIV,
    KW_STARTING,
    KW_CONTAINING,
    KW_CASE,
    KW_WHEN,
    KW_THEN,
    KW_ELSE,
    KW_NULL,
    KW_TRUE,
    KW_FALSE,
    KW_EXISTS,
    KW_CAST,
    KW_AS,

    // Join keywords (only JOIN and ON are Gatekeepers)
    // LEFT, RIGHT, INNER, OUTER, FULL, CROSS, NATURAL are now contextual
    // to allow them as column names without quoting
    KW_JOIN,
    KW_ON,
    KW_USING,
    KW_LATERAL,

    // Values clause
    KW_VALUES,
    KW_INTO,
    KW_DEFAULT,

    // Transaction (only START is Gatekeeper)
    // TRANSACTION, SAVEPOINT, RELEASE are contextual after START
    KW_START,

    // Procedure/function control flow (only IF, RETURN are Gatekeepers)
    // Other procedure keywords are contextual within procedure blocks
    KW_IF,
    KW_RETURN,

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
    // Use std::string as key to avoid dangling string_views when vector reallocates
    std::unordered_map<std::string, StringId> lookup_;
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
        StringPool::StringId string_id; // IDENTIFIER, STRING_LITERAL, BLOB_LITERAL
        uint32_t param_index;           // PARAMETER ($1, $2, etc.)
    } value;

    Token();

    // Factory methods
    static Token makeEOF(SourceLocation loc);
    static Token makeError(SourceLocation loc, uint32_t len);
    static Token makeInteger(SourceLocation loc, uint32_t len, int64_t val);
    static Token makeFloat(SourceLocation loc, uint32_t len, double val);
    static Token makeString(SourceLocation loc, uint32_t len, StringPool::StringId id);
    static Token makeBlob(SourceLocation loc, uint32_t len, StringPool::StringId id);
    static Token makeIdentifier(SourceLocation loc, uint32_t len, StringPool::StringId id, bool delimited = false);
    static Token makeParameter(SourceLocation loc, uint32_t len, uint32_t index);
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
 * Error reporter interface
 */
class ErrorReporter {
public:
    virtual ~ErrorReporter() = default;

    virtual void reportError(const LexerError& error) = 0;
    virtual bool hasErrors() const = 0;
    virtual size_t errorCount() const = 0;
};

/**
 * Simple error collector implementation
 */
class SimpleErrorReporter : public ErrorReporter {
public:
    void reportError(const LexerError& error) override;
    bool hasErrors() const override { return !errors_.empty(); }
    size_t errorCount() const override { return errors_.size(); }

    const std::vector<LexerError>& errors() const { return errors_; }
    void clear() { errors_.clear(); }

private:
    std::vector<LexerError> errors_;
};

/**
 * Lexer v2.0 - Gatekeeper Keyword Model
 *
 * This lexer recognizes only ~35 reserved keywords (Gatekeepers).
 * All other identifiers, including contextual keywords, are emitted
 * as IDENTIFIER tokens for the parser to resolve.
 */
class Lexer {
public:
    /**
     * Create lexer for given input
     * @param input SQL source text (must remain valid for lexer lifetime)
     */
    explicit Lexer(std::string_view input);
    ~Lexer();

    // Non-copyable, non-movable (owns resources)
    Lexer(const Lexer&) = delete;
    Lexer& operator=(const Lexer&) = delete;
    Lexer(Lexer&&) = delete;
    Lexer& operator=(Lexer&&) = delete;

    /**
     * Get next token from input
     * Returns END_OF_FILE when input is exhausted
     */
    Token nextToken();

    /**
     * Peek at next token without consuming
     */
    Token peekToken();

    /**
     * Get current source location
     */
    SourceLocation currentLocation() const;

    /**
     * Get source text for a token span
     */
    std::string_view getTokenText(const Token& token) const;
    std::string_view getTokenText(const SourceSpan& span) const;

    /**
     * Get original input text
     */
    std::string_view input() const { return input_; }

    /**
     * String pool access
     */
    StringPool& stringPool() { return string_pool_; }
    const StringPool& stringPool() const { return string_pool_; }

    /**
     * Error reporting
     */
    void setErrorReporter(ErrorReporter* reporter) { error_reporter_ = reporter; }
    ErrorReporter* errorReporter() const { return error_reporter_; }

private:
    // Input state
    std::string_view input_;
    size_t pos_;
    uint32_t line_;
    uint32_t column_;

    // Resources
    StringPool string_pool_;
    ErrorReporter* error_reporter_;

    // Lookahead
    std::optional<Token> lookahead_;

    // Character access
    char current() const;
    char peek(size_t offset = 1) const;
    void advance();
    void advanceN(size_t n);
    bool atEnd() const;

    // Whitespace and comments
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment();

    // Token scanners
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanString();
    Token scanEscapeString();      // E'...'
    Token scanBlobLiteral();       // X'...'
    Token scanQuotedIdentifier();  // "..."
    Token scanParameter();         // $1, :name
    Token scanOperator();

    // Gatekeeper keyword lookup
    TokenType checkGatekeeperKeyword(std::string_view text) const;

    // Error handling
    Token makeError(const std::string& message);
    void reportError(SourceLocation loc, uint32_t len, const std::string& message,
                     const std::string& hint = "");
};

/**
 * Contextual keyword table for parser use
 *
 * The parser uses this to check if an IDENTIFIER token matches
 * a contextual keyword in the current parsing context.
 */
struct ContextualKeyword {
    const char* text;
    uint16_t context_flags;  // Bit flags for valid contexts
};

/**
 * Context flags for contextual keyword matching
 */
enum class KeywordContext : uint16_t {
    NONE           = 0,
    TYPE_NAME      = 1 << 0,   // Type declarations (INTEGER, VARCHAR, etc.)
    DDL_OBJECT     = 1 << 1,   // DDL object types (TABLE, VIEW, INDEX, etc.)
    COLUMN_DEF     = 1 << 2,   // Column definitions (PRIMARY, REFERENCES, etc.)
    WINDOW_FRAME   = 1 << 3,   // Window frame (ROWS, RANGE, PRECEDING, etc.)
    AGGREGATE      = 1 << 4,   // Aggregate functions (COUNT, SUM, AVG, etc.)
    INTERVAL_UNIT  = 1 << 5,   // Interval units (YEAR, MONTH, DAY, etc.)
    TRANSACTION    = 1 << 6,   // Transaction options (READ, WRITE, ISOLATION, etc.)
    SECURITY       = 1 << 7,   // Security (USER, ROLE, GRANT, etc.)
    PROCEDURE      = 1 << 8,   // Procedure/function (FUNCTION, PROCEDURE, etc.)
    TRIGGER        = 1 << 9,   // Trigger (BEFORE, AFTER, etc.)
    CONSTRAINT     = 1 << 10,  // Constraints (UNIQUE, FOREIGN, etc.)
    SHOW_COMMAND   = 1 << 11,  // SHOW targets (TABLES, DATABASES, etc.)
    EXTRACT_FIELD  = 1 << 12,  // EXTRACT fields (YEAR, MONTH, etc.)
    COLLATION      = 1 << 13,  // Collation (COLLATE, etc.)
    OVER_CLAUSE    = 1 << 14,  // OVER clause (PARTITION, ORDER, etc.)
};

/**
 * Check if an identifier matches a contextual keyword in given context
 *
 * @param text The identifier text (should be uppercase for comparison)
 * @param context The current parsing context
 * @return The keyword token type if matched, or std::nullopt
 */
std::optional<TokenType> matchContextualKeyword(std::string_view text, KeywordContext context);

/**
 * Convert token type to string for debugging
 */
const char* tokenTypeToString(TokenType type);

/**
 * Check if a token type is a Gatekeeper keyword
 */
bool isGatekeeperKeyword(TokenType type);

/**
 * Check if a token type is an operator
 */
bool isOperator(TokenType type);

/**
 * Check if a token type is punctuation
 */
bool isPunctuation(TokenType type);

} // namespace scratchbird::parser::v2
