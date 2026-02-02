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
 * ScratchBird Lexer v2.0 Implementation
 *
 * Implements the "Smart Parser, Dumb Lexer" architecture with Gatekeeper keywords.
 *
 * See: include/scratchbird/parser/lexer_v2.h
 * See: docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md
 */

#include "scratchbird/parser/lexer_v2.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>

namespace scratchbird::parser::v2 {

// =============================================================================
// StringPool Implementation
// =============================================================================

StringPool::StringPool() {
    // Reserve ID 0 as invalid
    strings_.emplace_back("");
}

StringPool::~StringPool() = default;

StringPool::StringId StringPool::intern(std::string_view str) {
    // Convert string_view to string for lookup (required for std::unordered_map<std::string>)
    std::string key(str);
    auto it = lookup_.find(key);
    if (it != lookup_.end()) {
        return it->second;
    }

    StringId id = static_cast<StringId>(strings_.size());
    strings_.emplace_back(str);
    // Store the key in the lookup map (key already owns the string data)
    lookup_[std::move(key)] = id;
    return id;
}

std::string_view StringPool::get(StringId id) const {
    if (id == INVALID_ID || id >= strings_.size()) {
        return "";
    }
    return strings_[id];
}

void StringPool::clear() {
    strings_.clear();
    lookup_.clear();
    // Reserve ID 0 as invalid
    strings_.emplace_back("");
}

// =============================================================================
// Token Implementation
// =============================================================================

Token::Token()
    : type(TokenType::ERROR)
    , span()
    , is_delimited(false)
{
    value.int_value = 0;
}

Token Token::makeEOF(SourceLocation loc) {
    Token t;
    t.type = TokenType::END_OF_FILE;
    t.span = SourceSpan(loc, 0);
    t.value.int_value = 0;
    return t;
}

Token Token::makeError(SourceLocation loc, uint32_t len) {
    Token t;
    t.type = TokenType::ERROR;
    t.span = SourceSpan(loc, len);
    t.value.int_value = 0;
    return t;
}

Token Token::makeInteger(SourceLocation loc, uint32_t len, int64_t val) {
    Token t;
    t.type = TokenType::INTEGER_LITERAL;
    t.span = SourceSpan(loc, len);
    t.value.int_value = val;
    return t;
}

Token Token::makeFloat(SourceLocation loc, uint32_t len, double val) {
    Token t;
    t.type = TokenType::FLOAT_LITERAL;
    t.span = SourceSpan(loc, len);
    t.value.float_value = val;
    return t;
}

Token Token::makeString(SourceLocation loc, uint32_t len, StringPool::StringId id) {
    Token t;
    t.type = TokenType::STRING_LITERAL;
    t.span = SourceSpan(loc, len);
    t.value.string_id = id;
    return t;
}

Token Token::makeBlob(SourceLocation loc, uint32_t len, StringPool::StringId id) {
    Token t;
    t.type = TokenType::BLOB_LITERAL;
    t.span = SourceSpan(loc, len);
    t.value.string_id = id;
    return t;
}

Token Token::makeIdentifier(SourceLocation loc, uint32_t len, StringPool::StringId id, bool delimited) {
    Token t;
    t.type = TokenType::IDENTIFIER;
    t.span = SourceSpan(loc, len);
    t.is_delimited = delimited;
    t.value.string_id = id;
    return t;
}

Token Token::makeParameter(SourceLocation loc, uint32_t len, uint32_t index) {
    Token t;
    t.type = TokenType::PARAMETER;
    t.span = SourceSpan(loc, len);
    t.value.param_index = index;
    return t;
}

Token Token::makeKeyword(SourceLocation loc, uint32_t len, TokenType kwType) {
    Token t;
    t.type = kwType;
    t.span = SourceSpan(loc, len);
    t.value.int_value = 0;
    return t;
}

Token Token::makeOperator(SourceLocation loc, uint32_t len, TokenType opType) {
    Token t;
    t.type = opType;
    t.span = SourceSpan(loc, len);
    t.value.int_value = 0;
    return t;
}

Token Token::makePunctuation(SourceLocation loc, uint32_t len, TokenType punctType) {
    Token t;
    t.type = punctType;
    t.span = SourceSpan(loc, len);
    t.value.int_value = 0;
    return t;
}

// =============================================================================
// SimpleErrorReporter Implementation
// =============================================================================

void SimpleErrorReporter::reportError(const LexerError& error) {
    errors_.push_back(error);
}

// =============================================================================
// Gatekeeper Keyword Table
// =============================================================================

namespace {

struct GatekeeperEntry {
    const char* text;
    TokenType type;
};

// Gatekeeper keywords - these are the ONLY reserved keywords
// All other SQL keywords are contextual and emitted as IDENTIFIER
static const GatekeeperEntry GATEKEEPER_KEYWORDS[] = {
    // Statement initiators
    {"SELECT", TokenType::KW_SELECT},
    {"INSERT", TokenType::KW_INSERT},
    {"UPDATE", TokenType::KW_UPDATE},
    {"DELETE", TokenType::KW_DELETE},
    {"MERGE", TokenType::KW_MERGE},
    {"CREATE", TokenType::KW_CREATE},
    {"ALTER", TokenType::KW_ALTER},
    {"DROP", TokenType::KW_DROP},
    {"TRUNCATE", TokenType::KW_TRUNCATE},
    {"COPY", TokenType::KW_COPY},
    {"GRANT", TokenType::KW_GRANT},
    {"REVOKE", TokenType::KW_REVOKE},
    {"COMMIT", TokenType::KW_COMMIT},
    {"ROLLBACK", TokenType::KW_ROLLBACK},
    {"BEGIN", TokenType::KW_BEGIN},
    {"END", TokenType::KW_END},
    {"DECLARE", TokenType::KW_DECLARE},
    {"SET", TokenType::KW_SET},
    {"SHOW", TokenType::KW_SHOW},
    {"EXPLAIN", TokenType::KW_EXPLAIN},
    {"ANALYZE", TokenType::KW_ANALYZE},
    {"CALL", TokenType::KW_CALL},
    {"EXECUTE", TokenType::KW_EXECUTE},
    {"PREPARE", TokenType::KW_PREPARE},

    // Clause initiators
    {"FROM", TokenType::KW_FROM},
    {"WHERE", TokenType::KW_WHERE},
    {"GROUP", TokenType::KW_GROUP},
    {"HAVING", TokenType::KW_HAVING},
    {"ORDER", TokenType::KW_ORDER},
    {"LIMIT", TokenType::KW_LIMIT},
    {"OFFSET", TokenType::KW_OFFSET},
    {"UNION", TokenType::KW_UNION},
    {"INTERSECT", TokenType::KW_INTERSECT},
    {"EXCEPT", TokenType::KW_EXCEPT},
    {"WITH", TokenType::KW_WITH},

    // Expression keywords
    {"AND", TokenType::KW_AND},
    {"OR", TokenType::KW_OR},
    {"NOT", TokenType::KW_NOT},
    {"IS", TokenType::KW_IS},
    {"IN", TokenType::KW_IN},
    {"BETWEEN", TokenType::KW_BETWEEN},
    {"LIKE", TokenType::KW_LIKE},
    {"DIV", TokenType::KW_DIV},
    {"STARTING", TokenType::KW_STARTING},
    {"CONTAINING", TokenType::KW_CONTAINING},
    {"CASE", TokenType::KW_CASE},
    {"WHEN", TokenType::KW_WHEN},
    {"THEN", TokenType::KW_THEN},
    {"ELSE", TokenType::KW_ELSE},
    {"NULL", TokenType::KW_NULL},
    {"TRUE", TokenType::KW_TRUE},
    {"FALSE", TokenType::KW_FALSE},
    {"EXISTS", TokenType::KW_EXISTS},
    {"CAST", TokenType::KW_CAST},
    {"AS", TokenType::KW_AS},

    // Join keywords (only JOIN, ON, USING, LATERAL are Gatekeepers)
    // LEFT, RIGHT, INNER, OUTER, FULL, CROSS, NATURAL are contextual
    {"JOIN", TokenType::KW_JOIN},
    {"ON", TokenType::KW_ON},
    {"USING", TokenType::KW_USING},
    {"LATERAL", TokenType::KW_LATERAL},

    // Values clause
    {"VALUES", TokenType::KW_VALUES},
    {"INTO", TokenType::KW_INTO},
    {"DEFAULT", TokenType::KW_DEFAULT},

    // Transaction (only START is Gatekeeper)
    {"START", TokenType::KW_START},

    // Procedure/function control flow (only IF, RETURN are Gatekeepers)
    {"IF", TokenType::KW_IF},
    {"RETURN", TokenType::KW_RETURN},
};

static constexpr size_t GATEKEEPER_COUNT = sizeof(GATEKEEPER_KEYWORDS) / sizeof(GATEKEEPER_KEYWORDS[0]);

// Case-insensitive string comparison
bool caseInsensitiveEqual(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(a[i])) !=
            std::toupper(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

} // anonymous namespace

// =============================================================================
// Lexer Implementation
// =============================================================================

Lexer::Lexer(std::string_view input)
    : input_(input)
    , pos_(0)
    , line_(1)
    , column_(1)
    , error_reporter_(nullptr)
{
}

Lexer::~Lexer() = default;

char Lexer::current() const {
    return pos_ < input_.size() ? input_[pos_] : '\0';
}

char Lexer::peek(size_t offset) const {
    size_t pos = pos_ + offset;
    return pos < input_.size() ? input_[pos] : '\0';
}

void Lexer::advance() {
    if (pos_ < input_.size()) {
        if (input_[pos_] == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        ++pos_;
    }
}

void Lexer::advanceN(size_t n) {
    for (size_t i = 0; i < n && pos_ < input_.size(); ++i) {
        advance();
    }
}

bool Lexer::atEnd() const {
    return pos_ >= input_.size();
}

SourceLocation Lexer::currentLocation() const {
    return SourceLocation(line_, column_, static_cast<uint32_t>(pos_));
}

std::string_view Lexer::getTokenText(const Token& token) const {
    return getTokenText(token.span);
}

std::string_view Lexer::getTokenText(const SourceSpan& span) const {
    if (span.start.offset + span.length > input_.size()) {
        return "";
    }
    return input_.substr(span.start.offset, span.length);
}

void Lexer::skipWhitespace() {
    while (!atEnd()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
        } else if (c == '-' && peek() == '-') {
            skipLineComment();
        } else if (c == '/' && peek() == '*') {
            skipBlockComment();
        } else {
            break;
        }
    }
}

void Lexer::skipLineComment() {
    // Skip --
    advance();
    advance();
    // Skip to end of line
    while (!atEnd() && current() != '\n') {
        advance();
    }
    // Skip newline
    if (!atEnd()) {
        advance();
    }
}

void Lexer::skipBlockComment() {
    SourceLocation start = currentLocation();
    // Skip /*
    advance();
    advance();

    int depth = 1;
    while (!atEnd() && depth > 0) {
        if (current() == '/' && peek() == '*') {
            advance();
            advance();
            ++depth;
        } else if (current() == '*' && peek() == '/') {
            advance();
            advance();
            --depth;
        } else {
            advance();
        }
    }

    if (depth > 0) {
        reportError(start, static_cast<uint32_t>(pos_ - start.offset),
                    "Unterminated block comment", "Add */ to close the comment");
    }
}

Token Lexer::nextToken() {
    // Return lookahead if available
    if (lookahead_) {
        Token t = *lookahead_;
        lookahead_.reset();
        return t;
    }

    skipWhitespace();

    if (atEnd()) {
        return Token::makeEOF(currentLocation());
    }

    SourceLocation start = currentLocation();
    char c = current();

    // Escape string E'...' - MUST check before identifier
    if ((c == 'E' || c == 'e') && peek() == '\'') {
        return scanEscapeString();
    }

    // Blob literal X'...' - MUST check before identifier
    if ((c == 'X' || c == 'x') && peek() == '\'') {
        return scanBlobLiteral();
    }

    // Identifier or keyword
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return scanIdentifierOrKeyword();
    }

    // Number
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return scanNumber();
    }

    // String literal
    if (c == '\'') {
        return scanString();
    }

    // Quoted identifier "..."
    if (c == '"') {
        return scanQuotedIdentifier();
    }

    // Parameter $1 or :name
    if (c == '$' || c == ':') {
        return scanParameter();
    }

    // Operators and punctuation
    return scanOperator();
}

Token Lexer::peekToken() {
    if (!lookahead_) {
        lookahead_ = nextToken();
    }
    return *lookahead_;
}

Token Lexer::scanIdentifierOrKeyword() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;

    // Consume identifier characters
    while (!atEnd()) {
        char c = current();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            advance();
        } else {
            break;
        }
    }

    std::string_view text = input_.substr(startPos, pos_ - startPos);
    uint32_t len = static_cast<uint32_t>(text.size());

    // Check if it's a Gatekeeper keyword
    TokenType kwType = checkGatekeeperKeyword(text);
    if (kwType != TokenType::IDENTIFIER) {
        return Token::makeKeyword(start, len, kwType);
    }

    // It's an identifier - intern it
    StringPool::StringId id = string_pool_.intern(text);
    return Token::makeIdentifier(start, len, id, false);
}

Token Lexer::scanNumber() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;
    bool isFloat = false;
    bool isHex = false;
    bool isBinary = false;

    // Check for hex (0x) or binary (0b)
    if (current() == '0') {
        char next = peek();
        if (next == 'x' || next == 'X') {
            advance(); // 0
            advance(); // x
            isHex = true;
            while (!atEnd() && std::isxdigit(static_cast<unsigned char>(current()))) {
                advance();
            }
        } else if (next == 'b' || next == 'B') {
            advance(); // 0
            advance(); // b
            isBinary = true;
            while (!atEnd() && (current() == '0' || current() == '1')) {
                advance();
            }
        }
    }

    if (!isHex && !isBinary) {
        // Decimal integer part
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
            advance();
        }

        // Decimal point
        if (current() == '.' && std::isdigit(static_cast<unsigned char>(peek()))) {
            isFloat = true;
            advance(); // .
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
                advance();
            }
        }

        // Exponent
        if (current() == 'e' || current() == 'E') {
            isFloat = true;
            advance();
            if (current() == '+' || current() == '-') {
                advance();
            }
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
                advance();
            }
        }
    }

    std::string_view text = input_.substr(startPos, pos_ - startPos);
    uint32_t len = static_cast<uint32_t>(text.size());

    if (isFloat) {
        double val = 0.0;
        auto result = std::from_chars(text.data(), text.data() + text.size(), val);
        if (result.ec != std::errc()) {
            return makeError("Invalid floating-point number");
        }
        return Token::makeFloat(start, len, val);
    } else {
        int64_t val = 0;
        int base = isHex ? 16 : (isBinary ? 2 : 10);
        const char* begin = text.data();
        if (isHex || isBinary) {
            begin += 2; // Skip 0x or 0b
        }
        auto result = std::from_chars(begin, text.data() + text.size(), val, base);
        if (result.ec != std::errc()) {
            return makeError("Invalid integer number");
        }
        return Token::makeInteger(start, len, val);
    }
}

Token Lexer::scanString() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;

    advance(); // Opening '

    std::string value;
    while (!atEnd()) {
        char c = current();
        if (c == '\'') {
            if (peek() == '\'') {
                // Escaped single quote
                value += '\'';
                advance();
                advance();
            } else {
                // End of string
                advance();
                break;
            }
        } else {
            value += c;
            advance();
        }
    }

    uint32_t len = static_cast<uint32_t>(pos_ - startPos);
    StringPool::StringId id = string_pool_.intern(value);
    return Token::makeString(start, len, id);
}

Token Lexer::scanEscapeString() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;

    advance(); // E
    advance(); // Opening '

    std::string value;
    while (!atEnd()) {
        char c = current();
        if (c == '\'') {
            if (peek() == '\'') {
                value += '\'';
                advance();
                advance();
            } else {
                advance();
                break;
            }
        } else if (c == '\\') {
            advance();
            if (!atEnd()) {
                char esc = current();
                switch (esc) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '\'': value += '\''; break;
                    case '0': value += '\0'; break;
                    default: value += esc; break;
                }
                advance();
            }
        } else {
            value += c;
            advance();
        }
    }

    uint32_t len = static_cast<uint32_t>(pos_ - startPos);
    StringPool::StringId id = string_pool_.intern(value);
    return Token::makeString(start, len, id);
}

Token Lexer::scanBlobLiteral() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;

    advance(); // X
    advance(); // Opening '

    std::string hexValue;
    while (!atEnd()) {
        char c = current();
        if (c == '\'') {
            advance();
            break;
        } else if (std::isxdigit(static_cast<unsigned char>(c))) {
            hexValue += c;
            advance();
        } else if (c == ' ' || c == '\t' || c == '\n') {
            // Whitespace allowed in blob literals
            advance();
        } else {
            return makeError("Invalid character in blob literal");
        }
    }

    if (hexValue.size() % 2 != 0) {
        return makeError("Blob literal must have even number of hex digits");
    }

    uint32_t len = static_cast<uint32_t>(pos_ - startPos);
    StringPool::StringId id = string_pool_.intern(hexValue);
    return Token::makeBlob(start, len, id);
}

Token Lexer::scanQuotedIdentifier() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;

    advance(); // Opening "

    std::string value;
    while (!atEnd()) {
        char c = current();
        if (c == '"') {
            if (peek() == '"') {
                // Escaped double quote
                value += '"';
                advance();
                advance();
            } else {
                // End of identifier
                advance();
                break;
            }
        } else {
            value += c;
            advance();
        }
    }

    uint32_t len = static_cast<uint32_t>(pos_ - startPos);
    StringPool::StringId id = string_pool_.intern(value);
    return Token::makeIdentifier(start, len, id, true); // delimited = true
}

Token Lexer::scanParameter() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;
    char prefix = current();

    advance(); // $ or :

    if (prefix == '$') {
        // Positional parameter $1, $2, etc.
        if (!std::isdigit(static_cast<unsigned char>(current()))) {
            return makeError("Expected digit after $");
        }

        uint32_t index = 0;
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
            index = index * 10 + (current() - '0');
            advance();
        }

        uint32_t len = static_cast<uint32_t>(pos_ - startPos);
        return Token::makeParameter(start, len, index);
    } else {
        // Check for := (assignment)
        if (current() == '=') {
            advance();
            return Token::makeOperator(start, 2, TokenType::COLON_EQUALS);
        }

        // Check for :: (type cast)
        if (current() == ':') {
            advance();
            return Token::makeOperator(start, 2, TokenType::DOUBLE_COLON);
        }

        // Named parameter :name
        if (!std::isalpha(static_cast<unsigned char>(current())) && current() != '_') {
            // Just a colon
            return Token::makePunctuation(start, 1, TokenType::COLON);
        }

        while (!atEnd()) {
            char c = current();
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                advance();
            } else {
                break;
            }
        }

        std::string_view text = input_.substr(startPos + 1, pos_ - startPos - 1);
        uint32_t len = static_cast<uint32_t>(pos_ - startPos);
        StringPool::StringId id = string_pool_.intern(text);

        // Use param_index = string_id for named parameters
        Token t;
        t.type = TokenType::PARAMETER;
        t.span = SourceSpan(start, len);
        t.value.string_id = id;
        return t;
    }
}

Token Lexer::scanOperator() {
    SourceLocation start = currentLocation();
    char c = current();
    char n = peek();
    char n2 = peek(2);

    // Multi-character operators (longest match first)

    // Three-character operators
    if (c == '#' && n == '>' && n2 == '>') {
        advanceN(3);
        return Token::makeOperator(start, 3, TokenType::HASH_DOUBLE_ARROW);
    }
    if (c == '!' && n == '~' && n2 == '*') {
        advanceN(3);
        return Token::makeOperator(start, 3, TokenType::EXCLAIM_TILDE_STAR);
    }
    if (c == '-' && n == '|' && n2 == '-') {
        advanceN(3);
        return Token::makeOperator(start, 3, TokenType::MINUS_PIPE_MINUS);
    }

    // Two-character operators
    if (c == '-' && n == '>') {
        if (n2 == '>') {
            advanceN(3);
            return Token::makeOperator(start, 3, TokenType::DOUBLE_ARROW);
        }
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::ARROW);
    }
    if (c == '#' && n == '>') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::HASH_ARROW);
    }
    if (c == '!' && n == ':') {
        advanceN(2);
        return Token::makePunctuation(start, 2, TokenType::EXCLAIM_COLON);
    }
    if (c == '<' && n == '>') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::NOT_EQUAL);
    }
    if (c == '!' && n == '=') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::NOT_EQUAL);
    }
    if (c == '<' && n == '=') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::LESS_EQUAL);
    }
    if (c == '>' && n == '=') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::GREATER_EQUAL);
    }
    if (c == '<' && n == '<') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::SHIFT_LEFT);
    }
    if (c == '>' && n == '>') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::SHIFT_RIGHT);
    }
    if (c == '|' && n == '|') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::DOUBLE_PIPE);
    }
    if (c == '&' && n == '&') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::DOUBLE_AMPERSAND);
    }
    if (c == ':' && n == ':') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::DOUBLE_COLON);
    }
    if (c == ':' && n == '=') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::COLON_EQUALS);
    }
    if (c == '=' && n == '>') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::EQUALS_GREATER);
    }
    if (c == '@' && n == '>') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::AT_GREATER);
    }
    if (c == '<' && n == '@') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::LESS_AT);
    }
    if (c == '~' && n == '*') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::TILDE_STAR);
    }
    if (c == '!' && n == '~') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::EXCLAIM_TILDE);
    }
    if (c == '?' && n == '|') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::QUESTION_PIPE);
    }
    if (c == '?' && n == '&') {
        advanceN(2);
        return Token::makeOperator(start, 2, TokenType::QUESTION_AMPERSAND);
    }

    // Double dot for parent schema navigation
    if (c == '.' && n == '.') {
        advanceN(2);
        return Token::makePunctuation(start, 2, TokenType::DOUBLE_DOT);
    }

    // Single-character operators and punctuation
    advance();
    switch (c) {
        case '+': return Token::makeOperator(start, 1, TokenType::PLUS);
        case '-': return Token::makeOperator(start, 1, TokenType::MINUS);
        case '*': return Token::makeOperator(start, 1, TokenType::STAR);
        case '/': return Token::makeOperator(start, 1, TokenType::SLASH);
        case '%': return Token::makeOperator(start, 1, TokenType::PERCENT);
        case '^': return Token::makeOperator(start, 1, TokenType::CARET);
        case '=': return Token::makeOperator(start, 1, TokenType::EQUAL);
        case '<': return Token::makeOperator(start, 1, TokenType::LESS_THAN);
        case '>': return Token::makeOperator(start, 1, TokenType::GREATER_THAN);
        case '&': return Token::makeOperator(start, 1, TokenType::AMPERSAND);
        case '|': return Token::makeOperator(start, 1, TokenType::PIPE);
        case '~': return Token::makeOperator(start, 1, TokenType::TILDE);
        case '@': return Token::makeOperator(start, 1, TokenType::AT_SIGN);
        case '?': return Token::makeOperator(start, 1, TokenType::QUESTION_MARK);
        case '(': return Token::makePunctuation(start, 1, TokenType::LEFT_PAREN);
        case ')': return Token::makePunctuation(start, 1, TokenType::RIGHT_PAREN);
        case '[': return Token::makePunctuation(start, 1, TokenType::LEFT_BRACKET);
        case ']': return Token::makePunctuation(start, 1, TokenType::RIGHT_BRACKET);
        case '{': return Token::makePunctuation(start, 1, TokenType::LEFT_BRACE);
        case '}': return Token::makePunctuation(start, 1, TokenType::RIGHT_BRACE);
        case ',': return Token::makePunctuation(start, 1, TokenType::COMMA);
        case ';': return Token::makePunctuation(start, 1, TokenType::SEMICOLON);
        case '.': return Token::makePunctuation(start, 1, TokenType::DOT);
        case ':': return Token::makePunctuation(start, 1, TokenType::COLON);
        default:
            return makeError(std::string("Unexpected character: ") + c);
    }
}

TokenType Lexer::checkGatekeeperKeyword(std::string_view text) const {
    for (size_t i = 0; i < GATEKEEPER_COUNT; ++i) {
        if (caseInsensitiveEqual(text, GATEKEEPER_KEYWORDS[i].text)) {
            return GATEKEEPER_KEYWORDS[i].type;
        }
    }
    return TokenType::IDENTIFIER;
}

Token Lexer::makeError(const std::string& message) {
    SourceLocation loc = currentLocation();
    reportError(loc, 1, message);
    return Token::makeError(loc, 1);
}

void Lexer::reportError(SourceLocation loc, uint32_t len,
                        const std::string& message, const std::string& hint) {
    if (error_reporter_) {
        LexerError error;
        error.span = SourceSpan(loc, len);
        error.message = message;
        error.hint = hint;
        error_reporter_->reportError(error);
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::END_OF_FILE: return "END_OF_FILE";
        case TokenType::ERROR: return "ERROR";
        case TokenType::INTEGER_LITERAL: return "INTEGER_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::BLOB_LITERAL: return "BLOB_LITERAL";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::PARAMETER: return "PARAMETER";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::CARET: return "CARET";
        case TokenType::DOUBLE_PIPE: return "DOUBLE_PIPE";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::NOT_EQUAL: return "NOT_EQUAL";
        case TokenType::LESS_THAN: return "LESS_THAN";
        case TokenType::GREATER_THAN: return "GREATER_THAN";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::AMPERSAND: return "AMPERSAND";
        case TokenType::PIPE: return "PIPE";
        case TokenType::TILDE: return "TILDE";
        case TokenType::SHIFT_LEFT: return "SHIFT_LEFT";
        case TokenType::SHIFT_RIGHT: return "SHIFT_RIGHT";
        case TokenType::ARROW: return "ARROW";
        case TokenType::DOUBLE_ARROW: return "DOUBLE_ARROW";
        case TokenType::HASH_ARROW: return "HASH_ARROW";
        case TokenType::HASH_DOUBLE_ARROW: return "HASH_DOUBLE_ARROW";
        case TokenType::AT_SIGN: return "AT_SIGN";
        case TokenType::QUESTION_MARK: return "QUESTION_MARK";
        case TokenType::QUESTION_PIPE: return "QUESTION_PIPE";
        case TokenType::QUESTION_AMPERSAND: return "QUESTION_AMPERSAND";
        case TokenType::DOUBLE_COLON: return "DOUBLE_COLON";
        case TokenType::AT_GREATER: return "AT_GREATER";
        case TokenType::LESS_AT: return "LESS_AT";
        case TokenType::DOUBLE_AMPERSAND: return "DOUBLE_AMPERSAND";
        case TokenType::MINUS_PIPE_MINUS: return "MINUS_PIPE_MINUS";
        case TokenType::TILDE_STAR: return "TILDE_STAR";
        case TokenType::EXCLAIM_TILDE: return "EXCLAIM_TILDE";
        case TokenType::EXCLAIM_TILDE_STAR: return "EXCLAIM_TILDE_STAR";
        case TokenType::COLON_EQUALS: return "COLON_EQUALS";
        case TokenType::EQUALS_GREATER: return "EQUALS_GREATER";
        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACKET: return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::DOT: return "DOT";
        case TokenType::DOUBLE_DOT: return "DOUBLE_DOT";
        case TokenType::EXCLAIM_COLON: return "EXCLAIM_COLON";
        case TokenType::COLON: return "COLON";
        // Gatekeeper keywords
        case TokenType::KW_SELECT: return "KW_SELECT";
        case TokenType::KW_INSERT: return "KW_INSERT";
        case TokenType::KW_UPDATE: return "KW_UPDATE";
        case TokenType::KW_DELETE: return "KW_DELETE";
        case TokenType::KW_MERGE: return "KW_MERGE";
        case TokenType::KW_CREATE: return "KW_CREATE";
        case TokenType::KW_ALTER: return "KW_ALTER";
        case TokenType::KW_DROP: return "KW_DROP";
        case TokenType::KW_TRUNCATE: return "KW_TRUNCATE";
        case TokenType::KW_COPY: return "KW_COPY";
        case TokenType::KW_GRANT: return "KW_GRANT";
        case TokenType::KW_REVOKE: return "KW_REVOKE";
        case TokenType::KW_COMMIT: return "KW_COMMIT";
        case TokenType::KW_ROLLBACK: return "KW_ROLLBACK";
        case TokenType::KW_BEGIN: return "KW_BEGIN";
        case TokenType::KW_END: return "KW_END";
        case TokenType::KW_DECLARE: return "KW_DECLARE";
        case TokenType::KW_SET: return "KW_SET";
        case TokenType::KW_SHOW: return "KW_SHOW";
        case TokenType::KW_EXPLAIN: return "KW_EXPLAIN";
        case TokenType::KW_ANALYZE: return "KW_ANALYZE";
        case TokenType::KW_CALL: return "KW_CALL";
        case TokenType::KW_EXECUTE: return "KW_EXECUTE";
        case TokenType::KW_PREPARE: return "KW_PREPARE";
        case TokenType::KW_FROM: return "KW_FROM";
        case TokenType::KW_WHERE: return "KW_WHERE";
        case TokenType::KW_GROUP: return "KW_GROUP";
        case TokenType::KW_HAVING: return "KW_HAVING";
        case TokenType::KW_ORDER: return "KW_ORDER";
        case TokenType::KW_LIMIT: return "KW_LIMIT";
        case TokenType::KW_OFFSET: return "KW_OFFSET";
        case TokenType::KW_UNION: return "KW_UNION";
        case TokenType::KW_INTERSECT: return "KW_INTERSECT";
        case TokenType::KW_EXCEPT: return "KW_EXCEPT";
        case TokenType::KW_WITH: return "KW_WITH";
        case TokenType::KW_AND: return "KW_AND";
        case TokenType::KW_OR: return "KW_OR";
        case TokenType::KW_NOT: return "KW_NOT";
        case TokenType::KW_IS: return "KW_IS";
        case TokenType::KW_IN: return "KW_IN";
        case TokenType::KW_BETWEEN: return "KW_BETWEEN";
        case TokenType::KW_LIKE: return "KW_LIKE";
        case TokenType::KW_DIV: return "KW_DIV";
        case TokenType::KW_STARTING: return "KW_STARTING";
        case TokenType::KW_CONTAINING: return "KW_CONTAINING";
        case TokenType::KW_CASE: return "KW_CASE";
        case TokenType::KW_WHEN: return "KW_WHEN";
        case TokenType::KW_THEN: return "KW_THEN";
        case TokenType::KW_ELSE: return "KW_ELSE";
        case TokenType::KW_NULL: return "KW_NULL";
        case TokenType::KW_TRUE: return "KW_TRUE";
        case TokenType::KW_FALSE: return "KW_FALSE";
        case TokenType::KW_EXISTS: return "KW_EXISTS";
        case TokenType::KW_CAST: return "KW_CAST";
        case TokenType::KW_AS: return "KW_AS";
        case TokenType::KW_JOIN: return "KW_JOIN";
        case TokenType::KW_ON: return "KW_ON";
        case TokenType::KW_USING: return "KW_USING";
        case TokenType::KW_LATERAL: return "KW_LATERAL";
        case TokenType::KW_VALUES: return "KW_VALUES";
        case TokenType::KW_INTO: return "KW_INTO";
        case TokenType::KW_DEFAULT: return "KW_DEFAULT";
        case TokenType::KW_START: return "KW_START";
        case TokenType::KW_IF: return "KW_IF";
        case TokenType::KW_RETURN: return "KW_RETURN";
        case TokenType::TOKEN_TYPE_COUNT: return "TOKEN_TYPE_COUNT";
    }
    return "UNKNOWN";
}

bool isGatekeeperKeyword(TokenType type) {
    return type >= TokenType::KW_SELECT && type < TokenType::TOKEN_TYPE_COUNT;
}

bool isOperator(TokenType type) {
    return type >= TokenType::PLUS && type <= TokenType::EQUALS_GREATER;
}

bool isPunctuation(TokenType type) {
    return type >= TokenType::LEFT_PAREN && type <= TokenType::COLON;
}

// =============================================================================
// Contextual Keyword Matching (for parser use)
// =============================================================================

namespace {

// Type names (contextual)
struct ContextualKeywordEntry {
    const char* text;
    uint16_t contexts;  // Bit flags for valid contexts
};

// This table is used by the parser, not the lexer
// The lexer emits IDENTIFIER; the parser checks this table in context
static const ContextualKeywordEntry CONTEXTUAL_KEYWORDS[] = {
    // Type names
    {"INT", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"INTEGER", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"SMALLINT", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"BIGINT", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"TINYINT", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"REAL", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"FLOAT", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"DOUBLE", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"DECIMAL", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"NUMERIC", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"CHAR", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"VARCHAR", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"TEXT", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"BOOLEAN", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"BOOL", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"DATE", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"TIME", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"TIMESTAMP", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"INTERVAL", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"UUID", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"JSON", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"JSONB", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"BLOB", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"BYTEA", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"ARRAY", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},
    {"VECTOR", static_cast<uint16_t>(KeywordContext::TYPE_NAME)},

    // DDL object types
    {"TABLE", static_cast<uint16_t>(KeywordContext::DDL_OBJECT)},
    {"VIEW", static_cast<uint16_t>(KeywordContext::DDL_OBJECT)},
    {"INDEX", static_cast<uint16_t>(KeywordContext::DDL_OBJECT)},
    {"SEQUENCE", static_cast<uint16_t>(KeywordContext::DDL_OBJECT)},
    {"FUNCTION", static_cast<uint16_t>(KeywordContext::DDL_OBJECT) | static_cast<uint16_t>(KeywordContext::PROCEDURE)},
    {"PROCEDURE", static_cast<uint16_t>(KeywordContext::DDL_OBJECT) | static_cast<uint16_t>(KeywordContext::PROCEDURE)},
    {"TRIGGER", static_cast<uint16_t>(KeywordContext::DDL_OBJECT) | static_cast<uint16_t>(KeywordContext::TRIGGER)},
    {"SCHEMA", static_cast<uint16_t>(KeywordContext::DDL_OBJECT)},
    {"DATABASE", static_cast<uint16_t>(KeywordContext::DDL_OBJECT)},
    {"DOMAIN", static_cast<uint16_t>(KeywordContext::DDL_OBJECT)},
    {"TYPE", static_cast<uint16_t>(KeywordContext::DDL_OBJECT)},
    {"MATERIALIZED", static_cast<uint16_t>(KeywordContext::DDL_OBJECT)},

    // Column definition keywords
    {"PRIMARY", static_cast<uint16_t>(KeywordContext::COLUMN_DEF) | static_cast<uint16_t>(KeywordContext::CONSTRAINT)},
    {"FOREIGN", static_cast<uint16_t>(KeywordContext::COLUMN_DEF) | static_cast<uint16_t>(KeywordContext::CONSTRAINT)},
    {"KEY", static_cast<uint16_t>(KeywordContext::COLUMN_DEF) | static_cast<uint16_t>(KeywordContext::CONSTRAINT)},
    {"REFERENCES", static_cast<uint16_t>(KeywordContext::COLUMN_DEF) | static_cast<uint16_t>(KeywordContext::CONSTRAINT)},
    {"UNIQUE", static_cast<uint16_t>(KeywordContext::COLUMN_DEF) | static_cast<uint16_t>(KeywordContext::CONSTRAINT)},
    {"CHECK", static_cast<uint16_t>(KeywordContext::COLUMN_DEF) | static_cast<uint16_t>(KeywordContext::CONSTRAINT)},
    {"CONSTRAINT", static_cast<uint16_t>(KeywordContext::COLUMN_DEF) | static_cast<uint16_t>(KeywordContext::CONSTRAINT)},
    {"IDENTITY", static_cast<uint16_t>(KeywordContext::COLUMN_DEF)},
    {"GENERATED", static_cast<uint16_t>(KeywordContext::COLUMN_DEF)},

    // Window frame keywords
    {"OVER", static_cast<uint16_t>(KeywordContext::OVER_CLAUSE)},
    {"PARTITION", static_cast<uint16_t>(KeywordContext::OVER_CLAUSE)},
    {"ROWS", static_cast<uint16_t>(KeywordContext::WINDOW_FRAME)},
    {"RANGE", static_cast<uint16_t>(KeywordContext::WINDOW_FRAME)},
    {"GROUPS", static_cast<uint16_t>(KeywordContext::WINDOW_FRAME)},
    {"UNBOUNDED", static_cast<uint16_t>(KeywordContext::WINDOW_FRAME)},
    {"PRECEDING", static_cast<uint16_t>(KeywordContext::WINDOW_FRAME)},
    {"FOLLOWING", static_cast<uint16_t>(KeywordContext::WINDOW_FRAME)},
    {"CURRENT", static_cast<uint16_t>(KeywordContext::WINDOW_FRAME)},

    // Aggregate functions
    {"COUNT", static_cast<uint16_t>(KeywordContext::AGGREGATE)},
    {"SUM", static_cast<uint16_t>(KeywordContext::AGGREGATE)},
    {"AVG", static_cast<uint16_t>(KeywordContext::AGGREGATE)},
    {"MIN", static_cast<uint16_t>(KeywordContext::AGGREGATE)},
    {"MAX", static_cast<uint16_t>(KeywordContext::AGGREGATE)},

    // Trigger keywords
    {"BEFORE", static_cast<uint16_t>(KeywordContext::TRIGGER)},
    {"AFTER", static_cast<uint16_t>(KeywordContext::TRIGGER)},
    {"INSTEAD", static_cast<uint16_t>(KeywordContext::TRIGGER)},

    // Security keywords
    {"USER", static_cast<uint16_t>(KeywordContext::SECURITY)},
    {"ROLE", static_cast<uint16_t>(KeywordContext::SECURITY)},
    {"PASSWORD", static_cast<uint16_t>(KeywordContext::SECURITY)},

    // SHOW command targets
    {"TABLES", static_cast<uint16_t>(KeywordContext::SHOW_COMMAND)},
    {"DATABASES", static_cast<uint16_t>(KeywordContext::SHOW_COMMAND)},
    {"SCHEMAS", static_cast<uint16_t>(KeywordContext::SHOW_COMMAND)},
    {"COLUMNS", static_cast<uint16_t>(KeywordContext::SHOW_COMMAND)},
    {"INDEXES", static_cast<uint16_t>(KeywordContext::SHOW_COMMAND)},

    // EXTRACT fields
    {"YEAR", static_cast<uint16_t>(KeywordContext::EXTRACT_FIELD) | static_cast<uint16_t>(KeywordContext::INTERVAL_UNIT)},
    {"MONTH", static_cast<uint16_t>(KeywordContext::EXTRACT_FIELD) | static_cast<uint16_t>(KeywordContext::INTERVAL_UNIT)},
    {"DAY", static_cast<uint16_t>(KeywordContext::EXTRACT_FIELD) | static_cast<uint16_t>(KeywordContext::INTERVAL_UNIT)},
    {"HOUR", static_cast<uint16_t>(KeywordContext::EXTRACT_FIELD) | static_cast<uint16_t>(KeywordContext::INTERVAL_UNIT)},
    {"MINUTE", static_cast<uint16_t>(KeywordContext::EXTRACT_FIELD) | static_cast<uint16_t>(KeywordContext::INTERVAL_UNIT)},
    {"SECOND", static_cast<uint16_t>(KeywordContext::EXTRACT_FIELD) | static_cast<uint16_t>(KeywordContext::INTERVAL_UNIT)},

    // Transaction keywords
    {"READ", static_cast<uint16_t>(KeywordContext::TRANSACTION)},
    {"WRITE", static_cast<uint16_t>(KeywordContext::TRANSACTION)},
    {"ONLY", static_cast<uint16_t>(KeywordContext::TRANSACTION)},
    {"ISOLATION", static_cast<uint16_t>(KeywordContext::TRANSACTION)},
    {"LEVEL", static_cast<uint16_t>(KeywordContext::TRANSACTION)},
    {"COMMITTED", static_cast<uint16_t>(KeywordContext::TRANSACTION)},
    {"SNAPSHOT", static_cast<uint16_t>(KeywordContext::TRANSACTION)},

    // Collation
    {"COLLATE", static_cast<uint16_t>(KeywordContext::COLLATION)},

    // ========================================================================
    // Demoted from Gatekeeper (can now be used as column names without quoting)
    // ========================================================================

    // Join modifiers (demoted - LEFT, RIGHT are common column names)
    {"LEFT", static_cast<uint16_t>(KeywordContext::NONE)},
    {"RIGHT", static_cast<uint16_t>(KeywordContext::NONE)},
    {"INNER", static_cast<uint16_t>(KeywordContext::NONE)},
    {"OUTER", static_cast<uint16_t>(KeywordContext::NONE)},
    {"FULL", static_cast<uint16_t>(KeywordContext::NONE)},
    {"CROSS", static_cast<uint16_t>(KeywordContext::NONE)},
    {"NATURAL", static_cast<uint16_t>(KeywordContext::NONE)},

    // Subquery/CTE qualifiers (demoted - ALL, DISTINCT follow SELECT/UNION)
    {"ALL", static_cast<uint16_t>(KeywordContext::NONE)},
    {"DISTINCT", static_cast<uint16_t>(KeywordContext::NONE)},
    {"RECURSIVE", static_cast<uint16_t>(KeywordContext::NONE)},

    // Transaction keywords (demoted - only valid after START)
    {"TRANSACTION", static_cast<uint16_t>(KeywordContext::TRANSACTION)},
    {"SAVEPOINT", static_cast<uint16_t>(KeywordContext::TRANSACTION)},
    {"RELEASE", static_cast<uint16_t>(KeywordContext::TRANSACTION)},

    // Procedure keywords (demoted - only valid in procedure blocks)
    {"ELSIF", static_cast<uint16_t>(KeywordContext::PROCEDURE)},
    {"LOOP", static_cast<uint16_t>(KeywordContext::PROCEDURE)},
    {"WHILE", static_cast<uint16_t>(KeywordContext::PROCEDURE)},
    {"FOR", static_cast<uint16_t>(KeywordContext::PROCEDURE)},
    {"EXIT", static_cast<uint16_t>(KeywordContext::PROCEDURE)},
    {"CONTINUE", static_cast<uint16_t>(KeywordContext::PROCEDURE)},
    {"RAISE", static_cast<uint16_t>(KeywordContext::PROCEDURE)},
    {"EXCEPTION", static_cast<uint16_t>(KeywordContext::PROCEDURE)},
    {"TRY", static_cast<uint16_t>(KeywordContext::PROCEDURE)},
};

static constexpr size_t CONTEXTUAL_KEYWORD_COUNT = sizeof(CONTEXTUAL_KEYWORDS) / sizeof(CONTEXTUAL_KEYWORDS[0]);

} // anonymous namespace

std::optional<TokenType> matchContextualKeyword(std::string_view text, KeywordContext context) {
    // Note: This function returns std::nullopt for now since contextual keywords
    // don't have separate token types in v2 - they're matched by the parser directly.
    // This is a placeholder for future enhancement where we might want to
    // return semantic information about the matched keyword.
    (void)text;
    (void)context;
    return std::nullopt;
}

} // namespace scratchbird::parser::v2
