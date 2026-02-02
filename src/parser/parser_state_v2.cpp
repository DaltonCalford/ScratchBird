/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird Parser v2.0 - Parser State Machine Implementation
 *
 * See: include/scratchbird/parser/parser_state_v2.h
 * See: docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md
 */

#include "scratchbird/parser/parser_state_v2.h"
#include <cassert>

namespace scratchbird::parser::v2 {

// =============================================================================
// ParserState Implementation
// =============================================================================

ParserState::ParserState(Lexer& lexer)
    : lexer_(lexer)
{
    // Initialize with STATEMENT mode
    mode_stack_.push(ParseMode::STATEMENT);

    // Prime the token stream
    current_token_ = lexer_.nextToken();
}

ParserState::~ParserState() = default;

void ParserState::pushMode(ParseMode mode) {
    mode_stack_.push(mode);
}

void ParserState::popMode() {
    if (mode_stack_.size() > 1) {
        mode_stack_.pop();
    }
}

ParseMode ParserState::currentMode() const {
    return mode_stack_.top();
}

bool ParserState::isInMode(ParseMode mode) const {
    // Check if mode is anywhere in the stack
    std::stack<ParseMode> temp = mode_stack_;
    while (!temp.empty()) {
        if (temp.top() == mode) {
            return true;
        }
        temp.pop();
    }
    return false;
}

bool ParserState::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

void ParserState::advance() {
    previous_token_ = current_token_;
    if (!isAtEnd()) {
        current_token_ = lexer_.nextToken();
    }
}

bool ParserState::matchContextual(const char* keyword) {
    if (current_token_.type != TokenType::IDENTIFIER) {
        return false;
    }

    std::string_view text = lexer_.stringPool().get(current_token_.value.string_id);
    if (caseInsensitiveEquals(text, keyword)) {
        advance();
        return true;
    }
    return false;
}

bool ParserState::checkContextual(const char* keyword) const {
    if (current_token_.type != TokenType::IDENTIFIER) {
        return false;
    }

    std::string_view text = lexer_.stringPool().get(current_token_.value.string_id);
    return caseInsensitiveEquals(text, keyword);
}

void ParserState::expectContextual(const char* keyword, const char* errorMsg) {
    if (!matchContextual(keyword)) {
        reportError(errorMsg);
        // Error recovery: advance anyway to avoid infinite loops
        if (!isAtEnd()) {
            advance();
        }
    }
}

bool ParserState::isIdentifier() const {
    return current_token_.type == TokenType::IDENTIFIER;
}

std::string_view ParserState::currentIdentifierText() const {
    if (current_token_.type != TokenType::IDENTIFIER) {
        return "";
    }
    return lexer_.stringPool().get(current_token_.value.string_id);
}

StringPool::StringId ParserState::currentStringId() const {
    if (current_token_.type != TokenType::IDENTIFIER) {
        return StringPool::INVALID_ID;
    }
    return current_token_.value.string_id;
}

void ParserState::setErrorHandler(ErrorHandler handler, void* context) {
    error_handler_ = handler;
    error_context_ = context;
}

void ParserState::reportError(const char* message) {
    if (error_handler_) {
        error_handler_(message, current_token_.span.start, error_context_);
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* parseModeToString(ParseMode mode) {
    switch (mode) {
        case ParseMode::STATEMENT:      return "STATEMENT";
        case ParseMode::DDL:            return "DDL";
        case ParseMode::DML_SELECT:     return "DML_SELECT";
        case ParseMode::DML_INSERT:     return "DML_INSERT";
        case ParseMode::DML_UPDATE:     return "DML_UPDATE";
        case ParseMode::DML_DELETE:     return "DML_DELETE";
        case ParseMode::DML_MERGE:      return "DML_MERGE";
        case ParseMode::EXPRESSION:     return "EXPRESSION";
        case ParseMode::SESSION:        return "SESSION";
        case ParseMode::TRANSACTION:    return "TRANSACTION";
        case ParseMode::PSQL:           return "PSQL";
        case ParseMode::COLUMN_DEF:     return "COLUMN_DEF";
        case ParseMode::TABLE_REF:      return "TABLE_REF";
        case ParseMode::TYPE_NAME:      return "TYPE_NAME";
        case ParseMode::SECURITY:       return "SECURITY";
        case ParseMode::WINDOW:         return "WINDOW";
        case ParseMode::WITH_CLAUSE:    return "WITH_CLAUSE";
        case ParseMode::VALUES_CLAUSE:  return "VALUES_CLAUSE";
        case ParseMode::ORDER_BY:       return "ORDER_BY";
        case ParseMode::GROUP_BY:       return "GROUP_BY";
    }
    return "UNKNOWN";
}

} // namespace scratchbird::parser::v2
