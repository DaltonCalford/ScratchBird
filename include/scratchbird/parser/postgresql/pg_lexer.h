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
 * PostgreSQL Parser Lexer
 *
 * Lexer for PostgreSQL 16 SQL dialect. Unlike the ScratchBird V2 parser's
 * "Gatekeeper" model with ~35 reserved keywords, PostgreSQL has ~96 reserved
 * keywords that cannot be used as identifiers without double-quote quoting.
 *
 * PostgreSQL-specific features:
 * - Double-quoted identifiers ("identifier")
 * - Dollar-quoted strings ($$...$$, $tag$...$tag$)
 * - Escape strings (E'...')
 * - Unicode escape strings (U&'...')
 * - Bit strings (B'1010')
 * - Hex strings (X'1A2B')
 * - Type cast operator (::)
 * - PostgreSQL-specific operators (@>, <@, ?|, ?&, etc.)
 * - Positional parameters ($1, $2, etc.)
 */

#include "pg_token.h"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace scratchbird::parser::postgresql {

/**
 * String pool for efficient identifier storage.
 */
class StringPool {
public:
    StringPool() = default;
    ~StringPool() = default;

    // Non-copyable, movable
    StringPool(const StringPool&) = delete;
    StringPool& operator=(const StringPool&) = delete;
    StringPool(StringPool&&) = default;
    StringPool& operator=(StringPool&&) = default;

    /**
     * Intern a string and return its ID.
     * If the string already exists, returns existing ID.
     */
    uint32_t intern(std::string_view str);

    /**
     * Get string by ID.
     */
    std::string_view get(uint32_t id) const;

    /**
     * Get number of interned strings.
     */
    size_t size() const { return strings_.size(); }

    /**
     * Clear all interned strings.
     */
    void clear();

private:
    std::vector<std::string> strings_;
    std::unordered_map<std::string, uint32_t> index_;
};

/**
 * PostgreSQL SQL Lexer
 *
 * Tokenizes PostgreSQL 16 SQL syntax including all reserved keywords,
 * operators, literals, and PostgreSQL-specific constructs.
 */
class Lexer {
public:
    /**
     * Create a lexer for the given SQL input.
     */
    explicit Lexer(std::string_view input);
    ~Lexer() = default;

    // Non-copyable
    Lexer(const Lexer&) = delete;
    Lexer& operator=(const Lexer&) = delete;

    /**
     * Get the next token from the input.
     */
    Token nextToken();

    /**
     * Peek at the next token without consuming it.
     */
    Token peek();

    /**
     * Check if we've reached end of input.
     */
    bool isAtEnd() const { return current_ >= input_.size(); }

    /**
     * Get the current source location.
     */
    SourceLocation currentLocation() const { return location_; }

    /**
     * Get the string pool for accessing interned identifiers.
     */
    StringPool& stringPool() { return string_pool_; }
    const StringPool& stringPool() const { return string_pool_; }

    /**
     * Get the original input string.
     */
    std::string_view input() const { return input_; }

    /**
     * Get text for a source span.
     */
    std::string_view getSpanText(const SourceSpan& span) const {
        return input_.substr(span.start.offset, span.length);
    }

private:
    // Input management
    std::string_view input_;
    size_t current_ = 0;
    SourceLocation location_;

    // Token lookahead
    Token peeked_token_;
    bool has_peeked_ = false;

    // String interning
    StringPool string_pool_;

    // Character utilities
    char advance();
    char peek_char() const;
    char peek_char_ahead(size_t offset) const;
    bool match(char expected);
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment();

    // Token scanning
    Token scanToken();
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanString(char quote);
    Token scanDollarString();
    Token scanEscapeString();
    Token scanQuotedIdentifier();
    Token scanBitString();
    Token scanHexString();
    Token scanParameter();
    Token scanOperator();

    // Keyword lookup
    TokenType lookupKeyword(std::string_view identifier) const;

    // Error handling
    Token makeError(const std::string& message);

    // Static keyword table
    static const std::unordered_map<std::string_view, TokenType> keywords_;
    static std::unordered_map<std::string_view, TokenType> initKeywords();
};

/**
 * Check if a character is valid for PostgreSQL identifier start.
 * PostgreSQL identifiers can start with letter or underscore.
 */
inline bool isIdentifierStart(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_' ||
           (static_cast<unsigned char>(c) >= 0x80);
}

/**
 * Check if a character is valid for PostgreSQL identifier continuation.
 */
inline bool isIdentifierContinue(char c) {
    return isIdentifierStart(c) ||
           (c >= '0' && c <= '9') ||
           c == '$';  // PostgreSQL allows $ in identifiers after the first char
}

/**
 * Check if a character is a digit.
 */
inline bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

/**
 * Check if a character is a hex digit.
 */
inline bool isHexDigit(char c) {
    return isDigit(c) ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/**
 * Check if a character is whitespace.
 */
inline bool isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

} // namespace scratchbird::parser::postgresql
