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
 * Firebird SQL Lexer
 *
 * Tokenizer for Firebird 5.0 SQL syntax. Unlike ScratchBird's "Gatekeeper"
 * lexer model, this lexer recognizes all ~200 reserved keywords as distinct
 * token types, following Firebird's exact reserved word rules.
 *
 * Key differences from ScratchBird lexer:
 * - ~200 reserved keywords (vs ~50 gatekeepers)
 * - Q-string literal support: Q'{text}'
 * - Charset-prefixed strings: _UTF8'text'
 * - Firebird-specific operators: !<, !>, ~=, ^=, ^<, ^>
 * - SQL Dialect support (1, 2, 3)
 *
 * Reference: Firebird 5.0 Language Reference
 */

#include "scratchbird/parser/firebird/firebird_token.h"
#include <optional>
#include <vector>

namespace scratchbird::parser::firebird {

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
 * Firebird SQL Lexer
 *
 * Tokenizes Firebird SQL input, recognizing all reserved keywords
 * and Firebird-specific syntax.
 */
class Lexer {
public:
    /**
     * Create lexer for given input
     * @param input SQL source text (must remain valid for lexer lifetime)
     * @param dialect SQL dialect (default: Dialect 3)
     */
    explicit Lexer(std::string_view input, SQLDialect dialect = SQLDialect::DIALECT_3);
    ~Lexer();

    // Non-copyable, non-movable
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
     * Get SQL dialect
     */
    SQLDialect dialect() const { return dialect_; }

    /**
     * Set SQL dialect (can be changed during parsing for SET SQL DIALECT)
     */
    void setDialect(SQLDialect dialect) { dialect_ = dialect; }

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
    SQLDialect dialect_;

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
    void skipLineComment();      // -- comment
    void skipBlockComment();     // /* comment */

    // Token scanners
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanString();
    Token scanQString();           // Q'{...}'
    Token scanCharsetString();     // _charset'...'
    Token scanBlobLiteral();       // X'...'
    Token scanQuotedIdentifier();  // "..."
    Token scanParameter();         // :name or ?
    Token scanOperator();

    // Keyword lookup
    TokenType lookupKeyword(std::string_view text) const;

    // Error handling
    Token makeError(const std::string& message);
    void reportError(SourceLocation loc, uint32_t len, const std::string& message,
                     const std::string& hint = "");

    // Initialize keyword tables
    static void initKeywordTables();
    static bool tables_initialized_;
    static std::unordered_map<std::string, TokenType> reserved_keywords_;
    static std::unordered_map<std::string, TokenType> non_reserved_keywords_;
};

} // namespace scratchbird::parser::firebird
