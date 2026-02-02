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
 * ScratchBird Parser v2.0 - Parser State Machine
 *
 * This module implements the state-aware dispatch system for the v2.0 parser.
 * The parser uses explicit state tracking to enable contextual keyword matching,
 * allowing most SQL keywords to be used as identifiers in appropriate contexts.
 *
 * Design Philosophy: "Smart Parser, Dumb Lexer"
 * - The lexer emits IDENTIFIER for most tokens
 * - The parser determines meaning from context using state tracking
 * - Only ~35 Gatekeeper keywords are globally reserved
 *
 * See: docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md
 */

#include "scratchbird/parser/lexer_v2.h"
#include <stack>
#include <string_view>
#include <cctype>

namespace scratchbird::parser::v2 {

/**
 * Parse modes - tracks current parsing context for contextual keyword matching
 */
enum class ParseMode : uint8_t {
    STATEMENT,      // Top-level, expecting statement start
    DDL,            // After CREATE/ALTER/DROP
    DML_SELECT,     // Inside SELECT statement
    DML_INSERT,     // Inside INSERT statement
    DML_UPDATE,     // Inside UPDATE statement
    DML_DELETE,     // Inside DELETE statement
    DML_MERGE,      // Inside MERGE statement
    EXPRESSION,     // Parsing expression
    SESSION,        // After SET/RESET/SHOW
    TRANSACTION,    // After START TRANSACTION / SET TRANSACTION
    PSQL,           // Inside procedural block
    COLUMN_DEF,     // Parsing column definitions
    TABLE_REF,      // Parsing table reference (FROM, JOIN)
    TYPE_NAME,      // Parsing type name
    SECURITY,       // After GRANT/REVOKE
    WINDOW,         // Inside window specification
    WITH_CLAUSE,    // Inside WITH clause (CTE)
    VALUES_CLAUSE,  // Inside VALUES clause
    ORDER_BY,       // Inside ORDER BY clause
    GROUP_BY,       // Inside GROUP BY clause
};

/**
 * Case-insensitive string comparison utility
 */
inline bool caseInsensitiveEquals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(a[i])) !=
            std::toupper(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

/**
 * Parser State Machine
 *
 * Tracks the current parsing context and provides contextual keyword matching.
 * This enables the "Smart Parser, Dumb Lexer" architecture where most keywords
 * are only recognized in appropriate syntactic positions.
 *
 * Usage:
 *   ParserState state(lexer);
 *   state.pushMode(ParseMode::DDL);
 *   if (state.matchContextual("TABLE")) { ... }
 *   state.popMode();
 */
class ParserState {
public:
    explicit ParserState(Lexer& lexer);
    ~ParserState();

    // Mode stack management
    void pushMode(ParseMode mode);
    void popMode();
    ParseMode currentMode() const;
    bool isInMode(ParseMode mode) const;

    // Token access (delegates to lexer)
    Token current() const { return current_token_; }
    Token previous() const { return previous_token_; }
    bool isAtEnd() const { return current_token_.type == TokenType::END_OF_FILE; }

    // Basic token matching
    bool check(TokenType type) const { return current_token_.type == type; }
    bool match(TokenType type);
    void advance();

    /**
     * Contextual keyword matching - the core of the "Smart Parser" design
     *
     * These methods check if the current IDENTIFIER token matches a specific
     * keyword string. This allows keywords like TABLE, INDEX, USER to be
     * recognized only in appropriate contexts (e.g., after CREATE).
     */

    /**
     * Check if current token is an IDENTIFIER matching the given keyword
     * If match, advance and return true. Otherwise return false.
     */
    bool matchContextual(const char* keyword);

    /**
     * Check if current token is an IDENTIFIER matching the given keyword
     * Does NOT advance the lexer.
     */
    bool checkContextual(const char* keyword) const;

    /**
     * Require the current token to be an IDENTIFIER matching the keyword.
     * If not, reports an error with the given message.
     * Always advances (or enters error recovery).
     */
    void expectContextual(const char* keyword, const char* errorMsg);

    /**
     * Check if current token can be used as an identifier
     * Returns true for IDENTIFIER tokens (quoted or unquoted)
     */
    bool isIdentifier() const;

    /**
     * Get the string value of current token (if identifier)
     * Returns empty string_view if not an identifier
     */
    std::string_view currentIdentifierText() const;

    /**
     * Get the string ID of current identifier token
     * Returns INVALID_ID if not an identifier
     */
    StringPool::StringId currentStringId() const;

    // Lexer and string pool access
    Lexer& lexer() { return lexer_; }
    const Lexer& lexer() const { return lexer_; }
    StringPool& stringPool() { return lexer_.stringPool(); }
    const StringPool& stringPool() const { return lexer_.stringPool(); }

    // Source location helpers
    SourceLocation currentLocation() const { return current_token_.span.start; }
    std::string_view getTokenText(const Token& token) const { return lexer_.getTokenText(token); }

    // Error handling (to be implemented by parser)
    using ErrorHandler = void(*)(const char* message, SourceLocation loc, void* context);
    void setErrorHandler(ErrorHandler handler, void* context);

private:
    Lexer& lexer_;
    Token previous_token_;
    Token current_token_;
    std::stack<ParseMode> mode_stack_;

    // Error handling
    ErrorHandler error_handler_ = nullptr;
    void* error_context_ = nullptr;

    void reportError(const char* message);
};

/**
 * RAII helper for mode stack management
 *
 * Usage:
 *   {
 *       ParseModeGuard guard(state, ParseMode::DDL);
 *       // ... parsing code ...
 *   } // mode automatically popped
 */
class ParseModeGuard {
public:
    ParseModeGuard(ParserState& state, ParseMode mode)
        : state_(state)
    {
        state_.pushMode(mode);
    }

    ~ParseModeGuard() {
        state_.popMode();
    }

    // Non-copyable, non-movable
    ParseModeGuard(const ParseModeGuard&) = delete;
    ParseModeGuard& operator=(const ParseModeGuard&) = delete;
    ParseModeGuard(ParseModeGuard&&) = delete;
    ParseModeGuard& operator=(ParseModeGuard&&) = delete;

private:
    ParserState& state_;
};

/**
 * Convert ParseMode to string for debugging
 */
const char* parseModeToString(ParseMode mode);

} // namespace scratchbird::parser::v2
