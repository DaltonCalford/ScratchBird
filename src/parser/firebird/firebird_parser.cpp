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
 * Firebird SQL Parser Implementation
 *
 * Core parser infrastructure, statement dispatch, and expression parsing.
 */

#include "scratchbird/parser/firebird/firebird_parser.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <sstream>

namespace scratchbird::parser::firebird {

// Namespace aliases to avoid conflicts
namespace fb = scratchbird::parser::firebird;
namespace ast = scratchbird::parser::v2;

// =============================================================================
// ParseError
// =============================================================================

std::string ParseError::format() const {
    std::ostringstream oss;
    oss << "Error at line " << location.line << ", column " << location.column
        << ": " << message;
    if (!hint.empty()) {
        oss << " (" << hint << ")";
    }
    return oss.str();
}

// =============================================================================
// SimpleParserErrorReporter
// =============================================================================

void SimpleParserErrorReporter::reportError(const ParseError& error) {
    errors_.push_back(error);
}

// =============================================================================
// Parser - Constructor/Destructor
// =============================================================================

Parser::Parser(std::string_view input, SQLDialect dialect)
    : lexer_(input, dialect) {
    // Prime the parser with the first token
    advance();
}

Parser::~Parser() = default;

// =============================================================================
// Token Navigation
// =============================================================================

Token Parser::peek() {
    if (!has_lookahead_) {
        lookahead_token_ = lexer_.nextToken();
        has_lookahead_ = true;
    }
    return lookahead_token_;
}

void Parser::advance() {
    if (has_lookahead_) {
        current_token_ = lookahead_token_;
        has_lookahead_ = false;
    } else {
        current_token_ = lexer_.nextToken();
    }
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        Token token = current_token_;
        advance();
        return token;
    }

    error(message);
    return current_token_;
}

bool Parser::checkKeyword(TokenType type) const {
    return current_token_.type == type;
}

bool Parser::matchKeyword(TokenType type) {
    if (checkKeyword(type)) {
        advance();
        return true;
    }
    return false;
}

// =============================================================================
// Error Handling
// =============================================================================

void Parser::error(const std::string& message, const std::string& hint) {
    ParseError err;
    err.location = current_token_.span.start;
    err.message = message;
    err.hint = hint;

    if (error_reporter_) {
        error_reporter_->reportError(err);
    }
}

void Parser::synchronize() {
    advance();

    while (!atEnd()) {
        // Synchronize on statement-ending tokens
        if (current_token_.type == TokenType::SEMICOLON) {
            advance();
            return;
        }

        // Synchronize on statement-starting keywords
        if (isStatementStart()) {
            return;
        }

        advance();
    }
}

bool Parser::isStatementStart() const {
    switch (current_token_.type) {
        case TokenType::KW_SELECT:
        case TokenType::KW_INSERT:
        case TokenType::KW_UPDATE:
        case TokenType::KW_DELETE:
        case TokenType::KW_CREATE:
        case TokenType::KW_ALTER:
        case TokenType::KW_DROP:
        case TokenType::KW_GRANT:
        case TokenType::KW_REVOKE:
        case TokenType::KW_COMMIT:
        case TokenType::KW_ROLLBACK:
        case TokenType::KW_SET:
        case TokenType::KW_EXECUTE:
        case TokenType::KW_DECLARE:
        case TokenType::KW_BEGIN:
        case TokenType::KW_RECREATE:
        case TokenType::KW_COMMENT:
        case TokenType::KW_MERGE:
            return true;
        default:
            return false;
    }
}

bool Parser::isExpressionStart() const {
    switch (current_token_.type) {
        case TokenType::INTEGER_LITERAL:
        case TokenType::FLOAT_LITERAL:
        case TokenType::STRING_LITERAL:
        case TokenType::BLOB_LITERAL:
        case TokenType::IDENTIFIER:
        case TokenType::PARAMETER:
        case TokenType::LEFT_PAREN:
        case TokenType::KW_NOT:
        case TokenType::MINUS:
        case TokenType::PLUS:
        case TokenType::KW_CASE:
        case TokenType::KW_CAST:
        case TokenType::KW_EXISTS:
        case TokenType::KW_NULL:
        case TokenType::KW_TRUE:
        case TokenType::KW_FALSE:
        case TokenType::KW_CURRENT_DATE:
        case TokenType::KW_CURRENT_TIME:
        case TokenType::KW_CURRENT_TIMESTAMP:
        case TokenType::KW_CURRENT_USER:
        case TokenType::KW_CURRENT_ROLE:
        case TokenType::KW_CURRENT_CONNECTION:
        case TokenType::KW_CURRENT_TRANSACTION:
            return true;
        default:
            return false;
    }
}

// =============================================================================
// String Pool Conversion
// =============================================================================

ast::StringPool::StringId Parser::internFromLexer(fb::StringPool::StringId lexer_id) {
    if (lexer_id == fb::StringPool::INVALID_ID) {
        return ast::StringPool::INVALID_ID;
    }
    std::string_view text = lexer_.stringPool().get(lexer_id);
    return string_pool_.intern(text);
}

// Check if current token is a non-reserved keyword that can be used as an identifier
bool Parser::isNonReservedKeyword() const {
    // Non-reserved keywords in Firebird can be used as identifiers without quoting
    // This is a subset of keywords that are commonly used as column/table names
    switch (current_token_.type) {
        case TokenType::KW_NAME:
        case TokenType::KW_VALUE:
        case TokenType::KW_TYPE:
        case TokenType::KW_ACTION:
        case TokenType::KW_ACTIVE:
        case TokenType::KW_FIRST:
        case TokenType::KW_SKIP:
        case TokenType::KW_COUNT:
        case TokenType::KW_AVG:
        case TokenType::KW_SUM:
        case TokenType::KW_KEY:
        case TokenType::KW_LEVEL:
        case TokenType::KW_MAX:
        case TokenType::KW_MIN:
        case TokenType::KW_POSITION:
        case TokenType::KW_ROWS:
        case TokenType::KW_SIZE:
        case TokenType::KW_SOURCE:
        case TokenType::KW_COMMENT:
        case TokenType::KW_SEGMENT:
        case TokenType::KW_SEQUENCE:
        case TokenType::KW_GENERATOR:
        case TokenType::KW_RETAIN:
        case TokenType::KW_RENAME:
        case TokenType::KW_WORK:
        case TokenType::KW_ZONE:
        case TokenType::KW_SECURITY:
        case TokenType::KW_TRANSACTION:
        case TokenType::KW_SNAPSHOT:
        case TokenType::KW_READ:
        case TokenType::KW_WRITE:
        case TokenType::KW_WAIT:
        case TokenType::KW_LOCK:
        case TokenType::KW_TIMEOUT:
        case TokenType::KW_DATABASE:
        case TokenType::KW_MODE:
        case TokenType::KW_COLLATION:
        case TokenType::KW_CONNECTIONS:
        case TokenType::KW_PAGE:
        case TokenType::KW_PAGES:
        case TokenType::KW_PASSWORD:
        case TokenType::KW_PLUGIN:
        case TokenType::KW_ROLE:
        case TokenType::KW_USER:
        case TokenType::KW_SESSION:
        case TokenType::KW_SYSTEM:
        case TokenType::KW_DATA:
        case TokenType::KW_FILE:
        case TokenType::KW_MESSAGE:
        case TokenType::KW_RDB_GET_CONTEXT:
        case TokenType::KW_RDB_SET_CONTEXT:
            return true;
        default:
            return false;
    }
}

// Parse an identifier, accepting either IDENTIFIER token or non-reserved keywords
ast::StringPool::StringId Parser::parseIdentifier() {
    ast::StringPool::StringId id = ast::StringPool::INVALID_ID;

    if (check(TokenType::IDENTIFIER)) {
        id = internFromLexer(current_token_.value.string_id);
        advance();
    } else if (isNonReservedKeyword()) {
        // Non-reserved keywords used as identifiers - get text from lexer
        std::string_view text = lexer_.getTokenText(current_token_);
        id = string_pool_.intern(text);
        advance();
    } else {
        error("Expected identifier");
    }

    return id;
}

bool Parser::matchIdentifierText(const char* keyword) {
    if (!check(TokenType::IDENTIFIER) && !isNonReservedKeyword()) {
        return false;
    }

    std::string_view text = lexer_.getTokenText(current_token_);
    size_t len = std::strlen(keyword);
    if (text.size() != len) {
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
        char b = static_cast<char>(std::tolower(static_cast<unsigned char>(keyword[i])));
        if (a != b) {
            return false;
        }
    }

    advance();
    return true;
}

std::string_view Parser::currentText() {
    return lexer_.getTokenText(current_token_);
}

std::string Parser::extractExpressionText(ast::Expression* expr) {
    if (!expr) {
        return {};
    }
    std::string_view input = lexer_.input();
    size_t offset = expr->span.start.offset;
    size_t length = expr->span.length;
    if (offset >= input.size() || length == 0 || offset + length > input.size()) {
        return {};
    }
    std::string_view text = input.substr(offset, length);
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(start, end - start + 1));
}

std::string Parser::captureStatementBody() {
    std::string_view input = lexer_.input();
    if (input.empty() || atEnd()) {
        return {};
    }

    size_t start = current_token_.span.start.offset;
    size_t end = start;
    bool saw_begin = false;
    int begin_depth = 0;
    Token last = current_token_;

    auto trim = [](std::string_view text) -> std::string {
        size_t first = text.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) {
            return {};
        }
        size_t last = text.find_last_not_of(" \t\r\n");
        return std::string(text.substr(first, last - first + 1));
    };

    while (!atEnd()) {
        if (checkKeyword(TokenType::KW_BEGIN)) {
            saw_begin = true;
            begin_depth++;
        } else if (checkKeyword(TokenType::KW_END)) {
            if (saw_begin && begin_depth > 0) {
                begin_depth--;
                if (begin_depth == 0) {
                    end = current_token_.span.start.offset + current_token_.span.length;
                    advance();
                    if (end > input.size()) {
                        end = input.size();
                    }
                    return trim(input.substr(start, end - start));
                }
            }
        }

        if (!saw_begin && check(TokenType::SEMICOLON)) {
            end = last.span.start.offset + last.span.length;
            if (end > input.size()) {
                end = input.size();
            }
            return trim(input.substr(start, end - start));
        }

        last = current_token_;
        advance();
    }

    if (end == start) {
        end = last.span.start.offset + last.span.length;
        if (end > input.size()) {
            end = input.size();
        }
    }

    return trim(input.substr(start, end - start));
}

// =============================================================================
// SourceSpan Conversion
// =============================================================================

namespace {

// Convert Firebird SourceSpan to v2 SourceSpan
ast::SourceSpan toV2Span(const fb::SourceSpan& span) {
    ast::SourceLocation loc;
    loc.line = span.start.line;
    loc.column = span.start.column;
    loc.offset = span.start.offset;
    return ast::SourceSpan(loc, span.length);
}

struct FirebirdDatabaseSpec {
    std::string server;
    std::string file_path;
};

FirebirdDatabaseSpec parseFirebirdDatabaseSpec(std::string_view spec) {
    FirebirdDatabaseSpec result;
    result.file_path = std::string(spec);

    size_t colon = result.file_path.find(':');
    if (colon != std::string::npos) {
        bool is_drive = (colon == 1 &&
                         std::isalpha(static_cast<unsigned char>(result.file_path[0])) &&
                         result.file_path.size() > 2 &&
                         (result.file_path[2] == '\\' || result.file_path[2] == '/'));
        if (!is_drive) {
            result.server = result.file_path.substr(0, colon);
            result.file_path.erase(0, colon + 1);
        }
    }

    return result;
}

std::vector<std::string> splitFirebirdPathComponents(std::string_view path) {
    std::string working(path);
    std::vector<std::string> components;

    if (working.size() >= 2 && std::isalpha(static_cast<unsigned char>(working[0])) &&
        working[1] == ':') {
        std::string drive(1, static_cast<char>(std::tolower(static_cast<unsigned char>(working[0]))));
        components.push_back(drive);
        working.erase(0, 2);
    }

    while (!working.empty() && (working.front() == '/' || working.front() == '\\')) {
        working.erase(working.begin());
    }

    std::string current;
    for (char ch : working) {
        if (ch == '/' || ch == '\\') {
            if (!current.empty()) {
                components.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        components.push_back(current);
    }

    if (!components.empty()) {
        components.pop_back();  // Drop file name
    }

    return components;
}

std::string deriveFirebirdDatabaseName(std::string_view file_path) {
    size_t last_sep = file_path.find_last_of("/\\");
    std::string base = (last_sep == std::string_view::npos)
        ? std::string(file_path)
        : std::string(file_path.substr(last_sep + 1));

    if (base.empty()) {
        return base;
    }

    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < base.size()) {
        std::string ext = base.substr(dot + 1);
        for (char& ch : ext) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (ext == "fdb" || ext == "gdb") {
            base = base.substr(0, dot);
        }
    }

    return base;
}

ast::SchemaPath buildEmulatedDatabasePath(ast::StringPool& pool,
                                          std::string_view dialect,
                                          std::string_view server,
                                          const std::vector<std::string>& path_components,
                                          std::string_view db_name) {
    ast::SchemaPath path;
    path.type = ast::PathType::ABSOLUTE;
    path.components.push_back(pool.intern("emulation"));
    path.components.push_back(pool.intern(dialect));
    path.components.push_back(pool.intern(server));
    for (const auto& comp : path_components) {
        if (!comp.empty()) {
            path.components.push_back(pool.intern(comp));
        }
    }
    path.components.push_back(pool.intern(db_name));
    return path;
}

} // anonymous namespace

// =============================================================================
// Top-Level Parsing
// =============================================================================

ParseResult Parser::parseStatement() {
    ParseResult result;
    SimpleParserErrorReporter local_errors;
    ParserErrorReporter* prev_reporter = error_reporter_;
    if (!error_reporter_) {
        error_reporter_ = &local_errors;
    }
    size_t error_count_before = error_reporter_ ? error_reporter_->errorCount() : 0;

    try {
        Statement* stmt = parseStatementInternal();
        if (stmt) {
            result.statement.reset(stmt);
        }
    } catch (const std::exception& e) {
        error(e.what());
    }

    // Consume optional semicolon
    match(TokenType::SEMICOLON);

    bool has_new_errors = error_reporter_ &&
        error_reporter_->errorCount() > error_count_before;
    if (has_new_errors) {
        if (error_reporter_ == &local_errors) {
            result.errors = local_errors.errors();
        }
    } else if (result.statement) {
        result.success = true;
    }

    if (error_reporter_ == &local_errors) {
        error_reporter_ = prev_reporter;
    }

    return result;
}

std::vector<ParseResult> Parser::parseAll() {
    std::vector<ParseResult> results;

    while (!atEnd()) {
        // Skip semicolons between statements
        while (match(TokenType::SEMICOLON)) {}

        if (atEnd()) break;

        results.push_back(parseStatement());
    }

    return results;
}

Statement* Parser::parseStatementInternal() {
    // DDL
    if (matchKeyword(TokenType::KW_CREATE)) {
        return parseCreateStatement();
    }
    if (matchKeyword(TokenType::KW_ALTER)) {
        return parseAlterStatement();
    }
    if (matchKeyword(TokenType::KW_DROP)) {
        return parseDropStatement();
    }
    if (matchKeyword(TokenType::KW_RECREATE)) {
        return parseRecreateStatement();
    }

    // DML
    if (matchKeyword(TokenType::KW_SELECT)) {
        return parseSelectStatement();
    }
    if (matchKeyword(TokenType::KW_INSERT)) {
        return parseInsertStatement();
    }
    if (matchKeyword(TokenType::KW_UPDATE)) {
        // Check for UPDATE OR INSERT
        if (check(TokenType::KW_OR)) {
            return parseUpdateOrInsertStatement();
        }
        return parseUpdateStatement();
    }
    if (matchKeyword(TokenType::KW_DELETE)) {
        return parseDeleteStatement();
    }
    if (matchKeyword(TokenType::KW_MERGE)) {
        return parseMergeStatement();
    }

    // EXECUTE
    if (matchKeyword(TokenType::KW_EXECUTE)) {
        if (matchKeyword(TokenType::KW_PROCEDURE)) {
            return parseExecuteProcedure();
        }
        if (matchKeyword(TokenType::KW_BLOCK)) {
            return parseExecuteBlock();
        }
        // EXECUTE STATEMENT in PSQL
        return parseExecuteStatement();
    }

    // Transaction
    if (matchKeyword(TokenType::KW_COMMIT)) {
        return parseCommit();
    }
    if (matchKeyword(TokenType::KW_ROLLBACK)) {
        return parseRollback();
    }
    if (matchKeyword(TokenType::KW_SET)) {
        if (checkKeyword(TokenType::KW_TRANSACTION)) {
            return parseSetTransaction();
        }
        return parseSetStatement();
    }
    if (matchKeyword(TokenType::KW_SAVEPOINT)) {
        return parseSavepoint();
    }
    if (matchKeyword(TokenType::KW_RELEASE)) {
        return parseReleaseSavepoint();
    }

    // DCL
    if (matchKeyword(TokenType::KW_GRANT)) {
        return parseGrantStatement();
    }
    if (matchKeyword(TokenType::KW_REVOKE)) {
        return parseRevokeStatement();
    }

    // Metadata
    if (matchKeyword(TokenType::KW_COMMENT)) {
        return parseCommentStatement();
    }

    if (matchKeyword(TokenType::KW_SHOW)) {
        return parseShowStatement();
    }

    // PSQL blocks
    if (matchKeyword(TokenType::KW_DECLARE)) {
        return parseDeclareVariable();
    }
    if (matchKeyword(TokenType::KW_BEGIN)) {
        return parseBeginEndBlock();
    }

    error("Expected statement");
    return nullptr;
}

// =============================================================================
// Expression Parsing
// =============================================================================

Expression* Parser::parseExpression() {
    return parseOrExpression();
}

Expression* Parser::parseOrExpression() {
    Expression* left = parseAndExpression();

    while (matchKeyword(TokenType::KW_OR)) {
        Expression* right = parseAndExpression();
        auto* expr = allocate<ast::BinaryExpr>();
        expr->op = ast::BinaryOp::OR;
        expr->left = left;
        expr->right = right;
        left = expr;
    }

    return left;
}

Expression* Parser::parseAndExpression() {
    Expression* left = parseNotExpression();

    while (matchKeyword(TokenType::KW_AND)) {
        Expression* right = parseNotExpression();
        auto* expr = allocate<ast::BinaryExpr>();
        expr->op = ast::BinaryOp::AND;
        expr->left = left;
        expr->right = right;
        left = expr;
    }

    return left;
}

Expression* Parser::parseNotExpression() {
    if (matchKeyword(TokenType::KW_NOT)) {
        Expression* operand = parseNotExpression();
        auto* expr = allocate<ast::UnaryExpr>();
        expr->op = ast::UnaryOp::NOT;
        expr->operand = operand;
        return expr;
    }

    return parseComparisonExpression();
}

Expression* Parser::parseComparisonExpression() {
    Expression* left = parseAddExpression();

    // IS NULL / IS NOT NULL
    if (matchKeyword(TokenType::KW_IS)) {
        return parseIsNullExpression(left);
    }

    // NOT IN / NOT BETWEEN / NOT LIKE
    if (matchKeyword(TokenType::KW_NOT)) {
        if (matchKeyword(TokenType::KW_IN)) {
            auto* expr = static_cast<ast::InExpr*>(parseInExpression(left));
            expr->negated = true;
            return expr;
        }
        if (matchKeyword(TokenType::KW_BETWEEN)) {
            auto* expr = static_cast<ast::BetweenExpr*>(parseBetweenExpression(left));
            expr->negated = true;
            return expr;
        }
        if (matchKeyword(TokenType::KW_LIKE)) {
            auto* expr = static_cast<ast::LikeExpr*>(parseLikeExpression(left, ast::LikeMatchKind::LIKE));
            expr->negated = true;
            return expr;
        }
        if (matchKeyword(TokenType::KW_CONTAINING)) {
            auto* expr = static_cast<ast::LikeExpr*>(parseLikeExpression(left, ast::LikeMatchKind::CONTAINING));
            expr->negated = true;
            return expr;
        }
        if (matchKeyword(TokenType::KW_STARTING)) {
            matchKeyword(TokenType::KW_WITH);
            auto* expr = static_cast<ast::LikeExpr*>(parseLikeExpression(left, ast::LikeMatchKind::STARTING));
            expr->negated = true;
            return expr;
        }
        if (matchKeyword(TokenType::KW_SIMILAR)) {
            consume(TokenType::KW_TO, "Expected TO after SIMILAR");
            auto* expr = static_cast<ast::LikeExpr*>(parseLikeExpression(left, ast::LikeMatchKind::SIMILAR));
            expr->negated = true;
            return expr;
        }
        error("Expected IN, BETWEEN, LIKE, CONTAINING, STARTING, or SIMILAR after NOT");
        return left;
    }

    // IN
    if (matchKeyword(TokenType::KW_IN)) {
        return parseInExpression(left);
    }

    // BETWEEN
    if (matchKeyword(TokenType::KW_BETWEEN)) {
        return parseBetweenExpression(left);
    }

    // LIKE / SIMILAR TO / CONTAINING / STARTING
    if (matchKeyword(TokenType::KW_LIKE)) {
        return parseLikeExpression(left, ast::LikeMatchKind::LIKE);
    }
    if (matchKeyword(TokenType::KW_CONTAINING)) {
        return parseLikeExpression(left, ast::LikeMatchKind::CONTAINING);
    }
    if (matchKeyword(TokenType::KW_STARTING)) {
        matchKeyword(TokenType::KW_WITH);
        return parseLikeExpression(left, ast::LikeMatchKind::STARTING);
    }
    if (matchKeyword(TokenType::KW_SIMILAR)) {
        consume(TokenType::KW_TO, "Expected TO after SIMILAR");
        return parseLikeExpression(left, ast::LikeMatchKind::SIMILAR);
    }

    // Comparison operators
    ast::BinaryOp op;
    bool hasOp = false;

    if (match(TokenType::EQUAL)) {
        op = ast::BinaryOp::EQ;
        hasOp = true;
    } else if (match(TokenType::NOT_EQUAL) || match(TokenType::NOT_EQUAL)) {
        op = ast::BinaryOp::NE;
        hasOp = true;
    } else if (match(TokenType::LESS_THAN)) {
        op = ast::BinaryOp::LT;
        hasOp = true;
    } else if (match(TokenType::LESS_EQUAL)) {
        op = ast::BinaryOp::LE;
        hasOp = true;
    } else if (match(TokenType::GREATER_THAN)) {
        op = ast::BinaryOp::GT;
        hasOp = true;
    } else if (match(TokenType::GREATER_EQUAL)) {
        op = ast::BinaryOp::GE;
        hasOp = true;
    }
    // Firebird-specific: !<, !>, ~=, ^=
    else if (match(TokenType::NOT_LESS)) {
        op = ast::BinaryOp::GE;  // !< is same as >=
        hasOp = true;
    } else if (match(TokenType::NOT_GREATER)) {
        op = ast::BinaryOp::LE;  // !> is same as <=
        hasOp = true;
    } else if (match(TokenType::NOT_EQUAL_TILDE) || match(TokenType::NOT_EQUAL_CARET)) {
        op = ast::BinaryOp::NE;  // ~= and ^= are not equals
        hasOp = true;
    }

    if (hasOp) {
        Expression* right = parseAddExpression();
        auto* expr = allocate<ast::BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = right;
        return expr;
    }

    return left;
}

Expression* Parser::parseAddExpression() {
    Expression* left = parseMultExpression();

    while (true) {
        ast::BinaryOp op;
        if (match(TokenType::PLUS)) {
            op = ast::BinaryOp::ADD;
        } else if (match(TokenType::MINUS)) {
            op = ast::BinaryOp::SUB;
        } else if (match(TokenType::DOUBLE_PIPE)) {
            op = ast::BinaryOp::CONCAT;
        } else {
            break;
        }

        Expression* right = parseMultExpression();
        auto* expr = allocate<ast::BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = right;
        left = expr;
    }

    return left;
}

Expression* Parser::parseMultExpression() {
    Expression* left = parseUnaryExpression();

    while (true) {
        ast::BinaryOp op;
        if (match(TokenType::STAR)) {
            op = ast::BinaryOp::MUL;
        } else if (match(TokenType::SLASH)) {
            op = ast::BinaryOp::DIV;
        } else {
            break;
        }

        Expression* right = parseUnaryExpression();
        auto* expr = allocate<ast::BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = right;
        left = expr;
    }

    return left;
}

Expression* Parser::parseUnaryExpression() {
    if (match(TokenType::MINUS)) {
        Expression* operand = parseUnaryExpression();
        auto* expr = allocate<ast::UnaryExpr>();
        expr->op = ast::UnaryOp::NEGATE;
        expr->operand = operand;
        return expr;
    }

    if (match(TokenType::PLUS)) {
        return parseUnaryExpression();  // Unary plus is a no-op
    }

    return parsePrimaryExpression();
}

Expression* Parser::parsePrimaryExpression() {
    // Literals
    if (check(TokenType::INTEGER_LITERAL)) {
        auto* expr = allocate<ast::LiteralExpr>();
        expr->span = toV2Span(current_token_.span);
        expr->literal_type = ast::LiteralType::INTEGER;
        expr->int_value = current_token_.value.int_value;
        advance();
        return expr;
    }

    if (check(TokenType::FLOAT_LITERAL)) {
        auto* expr = allocate<ast::LiteralExpr>();
        expr->span = toV2Span(current_token_.span);
        expr->literal_type = ast::LiteralType::FLOAT;
        expr->float_value = current_token_.value.float_value;
        advance();
        return expr;
    }

    if (check(TokenType::STRING_LITERAL) || check(TokenType::Q_STRING_LITERAL)) {
        auto* expr = allocate<ast::LiteralExpr>();
        expr->span = toV2Span(current_token_.span);
        expr->literal_type = ast::LiteralType::STRING;
        expr->string_value = internFromLexer(current_token_.value.string_id);
        advance();
        return expr;
    }

    if (check(TokenType::BLOB_LITERAL)) {
        auto* expr = allocate<ast::LiteralExpr>();
        expr->span = toV2Span(current_token_.span);
        expr->literal_type = ast::LiteralType::BLOB;
        expr->string_value = internFromLexer(current_token_.value.string_id);
        advance();
        return expr;
    }

    // NULL
    if (matchKeyword(TokenType::KW_NULL)) {
        auto* expr = allocate<ast::LiteralExpr>();
        expr->literal_type = ast::LiteralType::NULL_VALUE;
        return expr;
    }

    // TRUE/FALSE
    if (matchKeyword(TokenType::KW_TRUE)) {
        auto* expr = allocate<ast::LiteralExpr>();
        expr->literal_type = ast::LiteralType::BOOLEAN;
        expr->bool_value = true;
        return expr;
    }
    if (matchKeyword(TokenType::KW_FALSE)) {
        auto* expr = allocate<ast::LiteralExpr>();
        expr->literal_type = ast::LiteralType::BOOLEAN;
        expr->bool_value = false;
        return expr;
    }

    // Context variables (Firebird-specific)
    if (checkKeyword(TokenType::KW_CURRENT_DATE)) {
        advance();
        auto* expr = allocate<ast::FunctionCallExpr>();
        expr->function_path.components.push_back(string_pool_.intern("CURRENT_DATE"));
        return expr;
    }
    if (checkKeyword(TokenType::KW_CURRENT_TIME)) {
        advance();
        auto* expr = allocate<ast::FunctionCallExpr>();
        expr->function_path.components.push_back(string_pool_.intern("CURRENT_TIME"));
        return expr;
    }
    if (checkKeyword(TokenType::KW_CURRENT_TIMESTAMP)) {
        advance();
        auto* expr = allocate<ast::FunctionCallExpr>();
        expr->function_path.components.push_back(string_pool_.intern("CURRENT_TIMESTAMP"));
        return expr;
    }
    if (checkKeyword(TokenType::KW_CURRENT_USER)) {
        advance();
        auto* expr = allocate<ast::FunctionCallExpr>();
        expr->function_path.components.push_back(string_pool_.intern("CURRENT_USER"));
        return expr;
    }
    if (checkKeyword(TokenType::KW_CURRENT_ROLE)) {
        advance();
        auto* expr = allocate<ast::FunctionCallExpr>();
        expr->function_path.components.push_back(string_pool_.intern("CURRENT_ROLE"));
        return expr;
    }
    if (checkKeyword(TokenType::KW_CURRENT_CONNECTION)) {
        advance();
        auto* expr = allocate<ast::FunctionCallExpr>();
        expr->function_path.components.push_back(string_pool_.intern("CURRENT_CONNECTION"));
        return expr;
    }
    if (checkKeyword(TokenType::KW_CURRENT_TRANSACTION)) {
        advance();
        auto* expr = allocate<ast::FunctionCallExpr>();
        expr->function_path.components.push_back(string_pool_.intern("CURRENT_TRANSACTION"));
        return expr;
    }
    if (checkKeyword(TokenType::KW_LOCALTIME)) {
        advance();
        auto* expr = allocate<ast::FunctionCallExpr>();
        expr->function_path.components.push_back(string_pool_.intern("LOCALTIME"));
        return expr;
    }
    if (checkKeyword(TokenType::KW_LOCALTIMESTAMP)) {
        advance();
        auto* expr = allocate<ast::FunctionCallExpr>();
        expr->function_path.components.push_back(string_pool_.intern("LOCALTIMESTAMP"));
        return expr;
    }
    if (checkKeyword(TokenType::KW_DATEADD)) {
        advance();
        ast::SchemaPath path;
        path.components.push_back(string_pool_.intern("DATEADD"));
        return parseFunctionCall(path);
    }
    if (checkKeyword(TokenType::KW_RDB_GET_CONTEXT)) {
        advance();
        ast::SchemaPath path;
        path.components.push_back(string_pool_.intern("RDB$GET_CONTEXT"));
        return parseFunctionCall(path);
    }

    // CASE expression
    if (matchKeyword(TokenType::KW_CASE)) {
        return parseCaseExpression();
    }

    // CAST expression
    if (matchKeyword(TokenType::KW_CAST)) {
        return parseCastExpression();
    }

    if (matchKeyword(TokenType::KW_EXTRACT)) {
        return parseExtractExpression();
    }

    if (matchKeyword(TokenType::KW_ALTER_ELEMENT)) {
        return parseAlterElementExpression();
    }

    // EXISTS subquery
    if (matchKeyword(TokenType::KW_EXISTS)) {
        return parseExistsExpression();
    }

    // Parenthesized expression or subquery
    if (match(TokenType::LEFT_PAREN)) {
        if (checkKeyword(TokenType::KW_SELECT)) {
            return parseSubqueryExpression();
        }
        Expression* expr = parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after expression");
        return expr;
    }

    // Parameter - store as a column reference with special naming
    if (check(TokenType::PARAMETER)) {
        auto* expr = allocate<ast::ColumnRefExpr>();
        expr->span = toV2Span(current_token_.span);
        // Store parameter name as a column reference
        expr->column = ast::ColumnRef(
            internFromLexer(current_token_.value.string_id),
            toV2Span(current_token_.span)
        );
        advance();
        return expr;
    }

    // Identifier or non-reserved keyword (column reference or function call)
    // Non-reserved keywords like COUNT, MAX, MIN can be used as function names
    if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
        ast::SourceSpan span = toV2Span(current_token_.span);
        ast::StringPool::StringId id;

        if (check(TokenType::IDENTIFIER)) {
            id = internFromLexer(current_token_.value.string_id);
        } else {
            // Non-reserved keyword - get text from lexer
            std::string_view text = lexer_.getTokenText(current_token_);
            id = string_pool_.intern(text);
        }
        advance();

        auto is_context_literal = [&](std::string_view name) {
            std::string upper;
            upper.reserve(name.size());
            for (char c : name) {
                upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
            return upper == "TODAY" || upper == "YESTERDAY" || upper == "TOMORROW";
        };

        if (is_context_literal(string_pool_.get(id))) {
            auto* expr = allocate<ast::FunctionCallExpr>();
            expr->function_path.components.push_back(id);
            return expr;
        }

        // Check for function call
        if (check(TokenType::LEFT_PAREN)) {
            ast::SchemaPath path;
            path.components.push_back(id);
            return parseFunctionCall(path);
        }

        // Check for qualified name (schema.table.column or table.column)
        std::vector<ast::StringPool::StringId> parts;
        parts.push_back(id);

        while (match(TokenType::DOT)) {
            if (!check(TokenType::IDENTIFIER) && !isNonReservedKeyword()) {
                error("Expected identifier after '.'");
                break;
            }
            if (check(TokenType::IDENTIFIER)) {
                parts.push_back(internFromLexer(current_token_.value.string_id));
            } else {
                std::string_view text = lexer_.getTokenText(current_token_);
                parts.push_back(string_pool_.intern(text));
            }
            if (parts.size() > 2) {
                error("Firebird does not support schema-qualified names");
            }
            advance();
        }

        // Check for function call on qualified name
        if (check(TokenType::LEFT_PAREN)) {
            ast::SchemaPath path;
            path.components = std::move(parts);
            return parseFunctionCall(path);
        }

        // It's a column reference
        auto* expr = allocate<ast::ColumnRefExpr>();
        expr->span = span;

        // Build ColumnRef from parts
        if (parts.size() == 1) {
            // Unqualified column: just column_name
            expr->column = ast::ColumnRef(parts[0], span);
        } else {
            // Qualified: table.column or schema.table.column
            ast::SchemaPath table_path;
            for (size_t i = 0; i < parts.size() - 1; ++i) {
                table_path.components.push_back(parts[i]);
            }
            expr->column = ast::ColumnRef(std::move(table_path), parts.back(), span);
        }
        return expr;
    }

    error("Expected expression");
    return nullptr;
}

// =============================================================================
// Expression Helpers
// =============================================================================

Expression* Parser::parseFunctionCall(const ast::SchemaPath& name) {
    auto* expr = allocate<ast::FunctionCallExpr>();
    expr->function_path = name;

    consume(TokenType::LEFT_PAREN, "Expected '(' after function name");

    // Parse arguments
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            // Check for DISTINCT or ALL
            if (matchKeyword(TokenType::KW_DISTINCT)) {
                expr->distinct = true;
            } else if (matchKeyword(TokenType::KW_ALL)) {
                expr->distinct = false;
            }

            // Check for * (COUNT(*))
            if (match(TokenType::STAR)) {
                // For COUNT(*), add a special marker expression
                auto* star_expr = allocate<ast::LiteralExpr>();
                star_expr->literal_type = ast::LiteralType::NULL_VALUE;
                expr->arguments.push_back(star_expr);
            } else {
                expr->arguments.push_back(parseExpression());
            }
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_PAREN, "Expected ')' after function arguments");

    // Check for OVER clause (window function)
    if (matchKeyword(TokenType::KW_OVER)) {
        expr->is_window = true;
        expr->window = parseWindowSpec();
    }

    return expr;
}

ast::WindowSpec* Parser::parseWindowSpec() {
    auto* spec = allocate<ast::WindowSpec>();

    consume(TokenType::LEFT_PAREN, "Expected '(' after OVER");

    // PARTITION BY
    if (matchKeyword(TokenType::KW_PARTITION)) {
        consume(TokenType::KW_BY, "Expected BY after PARTITION");
        do {
            spec->partition_by.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    // ORDER BY
    if (matchKeyword(TokenType::KW_ORDER)) {
        consume(TokenType::KW_BY, "Expected BY after ORDER");
        spec->order_by = parseOrderByClause();
    }

    // Frame clause (ROWS/RANGE)
    if (checkKeyword(TokenType::KW_ROWS) || checkKeyword(TokenType::KW_RANGE)) {
        parseWindowFrame(spec);
    }

    consume(TokenType::RIGHT_PAREN, "Expected ')' after window specification");

    return spec;
}

void Parser::parseWindowFrame(ast::WindowSpec* spec) {
    if (!spec) {
        error("Window specification required for frame clause");
        return;
    }

    if (matchKeyword(TokenType::KW_ROWS)) {
        spec->frame_type = ast::FrameType::ROWS;
    } else if (matchKeyword(TokenType::KW_RANGE)) {
        spec->frame_type = ast::FrameType::RANGE;
    } else {
        return;
    }

    spec->has_frame = true;

    if (matchKeyword(TokenType::KW_BETWEEN)) {
        spec->frame_start = parseWindowFrameBound(&spec->frame_start_value);
        consume(TokenType::KW_AND, "Expected AND in window frame");
        spec->frame_end = parseWindowFrameBound(&spec->frame_end_value);
    } else {
        spec->frame_start = parseWindowFrameBound(&spec->frame_start_value);
        spec->frame_end = ast::FrameBoundType::CURRENT_ROW;
        spec->frame_end_value = nullptr;
    }
}

ast::FrameBoundType Parser::parseWindowFrameBound(ast::Expression** value_out) {
    if (value_out) {
        *value_out = nullptr;
    }

    if (matchKeyword(TokenType::KW_UNBOUNDED)) {
        if (matchKeyword(TokenType::KW_PRECEDING)) {
            return ast::FrameBoundType::UNBOUNDED_PRECEDING;
        }
        if (matchKeyword(TokenType::KW_FOLLOWING)) {
            return ast::FrameBoundType::UNBOUNDED_FOLLOWING;
        }
        error("Expected PRECEDING or FOLLOWING after UNBOUNDED");
        return ast::FrameBoundType::UNBOUNDED_PRECEDING;
    }

    if (matchKeyword(TokenType::KW_CURRENT)) {
        consume(TokenType::KW_ROW, "Expected ROW after CURRENT");
        return ast::FrameBoundType::CURRENT_ROW;
    }

    // Value-based bound: <expr> PRECEDING|FOLLOWING
    Expression* value = parseExpression();
    if (matchKeyword(TokenType::KW_PRECEDING)) {
        if (value_out) {
            *value_out = value;
        }
        return ast::FrameBoundType::VALUE_PRECEDING;
    }
    if (matchKeyword(TokenType::KW_FOLLOWING)) {
        if (value_out) {
            *value_out = value;
        }
        return ast::FrameBoundType::VALUE_FOLLOWING;
    }

    error("Expected PRECEDING or FOLLOWING after window frame offset");
    return ast::FrameBoundType::CURRENT_ROW;
}

Expression* Parser::parseCaseExpression() {
    auto* expr = allocate<ast::CaseExpr>();

    // Simple CASE (CASE expr WHEN value THEN result ...)
    // or Searched CASE (CASE WHEN condition THEN result ...)
    if (!checkKeyword(TokenType::KW_WHEN)) {
        expr->operand = parseExpression();
    }

    // Parse WHEN clauses
    while (matchKeyword(TokenType::KW_WHEN)) {
        ast::CaseExpr::WhenClause clause;
        clause.when_expr = parseExpression();
        consume(TokenType::KW_THEN, "Expected THEN after WHEN condition");
        clause.then_expr = parseExpression();
        expr->when_clauses.push_back(clause);
    }

    // Parse ELSE clause
    if (matchKeyword(TokenType::KW_ELSE)) {
        expr->else_expr = parseExpression();
    }

    consume(TokenType::KW_END, "Expected END after CASE expression");

    return expr;
}

Expression* Parser::parseCastExpression() {
    auto* expr = allocate<ast::CastExpr>();

    consume(TokenType::LEFT_PAREN, "Expected '(' after CAST");
    expr->expr = parseExpression();
    consume(TokenType::KW_AS, "Expected AS in CAST expression");
    expr->target_type = parseTypeName();
    // CAST ... USING <format> (see docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md)
    if (matchKeyword(TokenType::KW_USING)) {
        expr->format = parseIdentifier();
    }
    consume(TokenType::RIGHT_PAREN, "Expected ')' after CAST");

    return expr;
}

Expression* Parser::parseExtractExpression() {
    auto* expr = allocate<ast::ExtractExpr>();
    consume(TokenType::LEFT_PAREN, "Expected '(' after EXTRACT");
    expr->selector = parseElementSelector();
    consume(TokenType::KW_FROM, "Expected FROM in EXTRACT expression");
    expr->source = parseExpression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after EXTRACT expression");
    return expr;
}

Expression* Parser::parseAlterElementExpression() {
    auto* expr = allocate<ast::AlterElementExpr>();
    consume(TokenType::LEFT_PAREN, "Expected '(' after ALTER_ELEMENT");
    expr->selector = parseElementSelector();
    consume(TokenType::KW_IN, "Expected IN in ALTER_ELEMENT expression");
    expr->source = parseExpression();
    consume(TokenType::KW_TO, "Expected TO in ALTER_ELEMENT expression");
    expr->new_value = parseExpression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after ALTER_ELEMENT expression");
    return expr;
}

ElementSelector Parser::parseElementSelector() {
    ElementSelector selector;

    if (check(TokenType::STRING_LITERAL) || check(TokenType::Q_STRING_LITERAL)) {
        selector.kind = ElementSelector::Kind::STRING_LITERAL;
        selector.string_literal = internFromLexer(current_token_.value.string_id);
        advance();
        return selector;
    }

    if (check(TokenType::INTEGER_LITERAL) || check(TokenType::PLUS) ||
        check(TokenType::MINUS) || check(TokenType::LEFT_PAREN)) {
        selector.kind = ElementSelector::Kind::INTEGER_EXPR;
        selector.expr = parseExpression();
        return selector;
    }

    if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
        selector.kind = ElementSelector::Kind::IDENTIFIER;
        selector.identifier = parseIdentifier();
        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    selector.args.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected ')' after element selector arguments");
        }
        return selector;
    }

    error("Expected element selector");
    return selector;
}

Expression* Parser::parseSubqueryExpression() {
    auto* expr = allocate<ast::SubqueryExpr>();
    // parseSelectStatement will be implemented in firebird_parser_dml.cpp
    expr->subquery = static_cast<ast::SelectStmt*>(parseSelectStatement());
    consume(TokenType::RIGHT_PAREN, "Expected ')' after subquery");
    return expr;
}

Expression* Parser::parseExistsExpression() {
    auto* expr = allocate<ast::ExistsExpr>();
    consume(TokenType::LEFT_PAREN, "Expected '(' after EXISTS");
    consume(TokenType::KW_SELECT, "Expected SELECT in EXISTS");
    expr->subquery = static_cast<ast::SelectStmt*>(parseSelectStatement());
    consume(TokenType::RIGHT_PAREN, "Expected ')' after EXISTS subquery");
    return expr;
}

Expression* Parser::parseInExpression(Expression* left) {
    auto* expr = allocate<ast::InExpr>();
    expr->expr = left;

    // Check for NOT IN
    if (matchKeyword(TokenType::KW_NOT)) {
        consume(TokenType::KW_IN, "Expected IN after NOT");
        expr->negated = true;
    }

    consume(TokenType::LEFT_PAREN, "Expected '(' after IN");

    if (checkKeyword(TokenType::KW_SELECT)) {
        // Subquery
        expr->subquery = static_cast<ast::SelectStmt*>(parseSelectStatement());
        expr->has_subquery = true;
    } else {
        // Value list
        do {
            expr->values.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_PAREN, "Expected ')' after IN list");

    return expr;
}

Expression* Parser::parseBetweenExpression(Expression* left) {
    auto* expr = allocate<ast::BetweenExpr>();
    expr->expr = left;

    // Check for NOT BETWEEN
    if (matchKeyword(TokenType::KW_NOT)) {
        consume(TokenType::KW_BETWEEN, "Expected BETWEEN after NOT");
        expr->negated = true;
    }

    expr->low = parseAddExpression();
    consume(TokenType::KW_AND, "Expected AND in BETWEEN expression");
    expr->high = parseAddExpression();

    return expr;
}

Expression* Parser::parseLikeExpression(Expression* left, ast::LikeMatchKind kind) {
    auto* expr = allocate<ast::LikeExpr>();
    expr->expr = left;
    expr->match_kind = kind;
    if (kind == ast::LikeMatchKind::CONTAINING) {
        expr->case_insensitive = true;
    }

    expr->pattern = parseAddExpression();

    // Check for ESCAPE clause
    if (matchKeyword(TokenType::KW_ESCAPE)) {
        if (kind == ast::LikeMatchKind::CONTAINING || kind == ast::LikeMatchKind::STARTING) {
            error("ESCAPE is not allowed with CONTAINING/STARTING predicates");
        }
        expr->escape = parseAddExpression();
    }

    return expr;
}

Expression* Parser::parseIsNullExpression(Expression* left) {
    auto* expr = allocate<ast::IsNullExpr>();
    expr->expr = left;

    // IS NOT NULL
    if (matchKeyword(TokenType::KW_NOT)) {
        expr->negated = true;
        consume(TokenType::KW_NULL, "Expected NULL after IS NOT");
    } else {
        consume(TokenType::KW_NULL, "Expected NULL after IS");
    }

    return expr;
}

Expression* Parser::parseArrayExpression() {
    auto* expr = allocate<ast::ArrayExpr>();

    consume(TokenType::LEFT_BRACKET, "Expected '[' for array");

    if (!check(TokenType::RIGHT_BRACKET)) {
        do {
            expr->elements.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_BRACKET, "Expected ']' after array");

    return expr;
}

// =============================================================================
// Type Parsing
// =============================================================================

ast::TypeName Parser::parseTypeName() {
    ast::TypeName type;
    type.span = toV2Span(current_token_.span);

    // Parse base type name
    if (check(TokenType::IDENTIFIER)) {
        type.name = internFromLexer(current_token_.value.string_id);
        advance();
    } else if (isFirebirdTypeName()) {
        type = parseFirebirdType();
    } else {
        error("Expected type name");
        return type;
    }

    // Parse type parameters
    if (match(TokenType::LEFT_PAREN)) {
        // First parameter (length or precision)
        if (check(TokenType::INTEGER_LITERAL)) {
            type.precision = static_cast<int32_t>(current_token_.value.int_value);
            advance();

            // Second parameter (scale)
            if (match(TokenType::COMMA)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    type.scale = static_cast<int32_t>(current_token_.value.int_value);
                    advance();
                }
            }
        }
        consume(TokenType::RIGHT_PAREN, "Expected ')' after type parameters");
    }

    // Array type
    if (match(TokenType::LEFT_BRACKET)) {
        type.is_array = true;
        if (check(TokenType::INTEGER_LITERAL)) {
            type.array_size = static_cast<int32_t>(current_token_.value.int_value);
            advance();
        }
        consume(TokenType::RIGHT_BRACKET, "Expected ']' after array size");
    }

    // WITH TIME ZONE
    if (matchKeyword(TokenType::KW_WITH)) {
        consume(TokenType::KW_TIME, "Expected TIME after WITH");
        consume(TokenType::KW_ZONE, "Expected ZONE after TIME");
        type.with_time_zone = true;
    }

    return type;
}

bool Parser::isFirebirdTypeName() const {
    switch (current_token_.type) {
        // Standard SQL types
        case TokenType::KW_INTEGER:
        case TokenType::KW_INT:
        case TokenType::KW_SMALLINT:
        case TokenType::KW_BIGINT:
        case TokenType::KW_FLOAT:
        case TokenType::KW_DOUBLE:
        case TokenType::KW_PRECISION:
        case TokenType::KW_REAL:
        case TokenType::KW_DECIMAL:
        case TokenType::KW_NUMERIC:
        case TokenType::KW_CHAR:
        case TokenType::KW_CHARACTER:
        case TokenType::KW_VARCHAR:
        case TokenType::KW_NCHAR:
        case TokenType::KW_BOOLEAN:
        case TokenType::KW_DATE:
        case TokenType::KW_TIME:
        case TokenType::KW_TIMESTAMP:
        case TokenType::KW_BLOB:
        // Firebird-specific types
        case TokenType::KW_INT128:
        case TokenType::KW_UINT128:
        case TokenType::KW_DECFLOAT:
        case TokenType::KW_VARBINARY:
            return true;
        default:
            return false;
    }
}

ast::TypeName Parser::parseFirebirdType() {
    ast::TypeName type;
    type.span = toV2Span(current_token_.span);

    // Map Firebird type keywords to standard type names
    switch (current_token_.type) {
        case TokenType::KW_INTEGER:
        case TokenType::KW_INT:
            type.name = string_pool_.intern("INTEGER");
            advance();
            break;

        case TokenType::KW_SMALLINT:
            type.name = string_pool_.intern("SMALLINT");
            advance();
            break;

        case TokenType::KW_BIGINT:
            type.name = string_pool_.intern("BIGINT");
            advance();
            break;

        case TokenType::KW_INT128:
            type.name = string_pool_.intern("INT128");
            advance();
            break;
        case TokenType::KW_UINT128:
            type.name = string_pool_.intern("UINT128");
            advance();
            break;

        case TokenType::KW_FLOAT:
            type.name = string_pool_.intern("FLOAT");
            advance();
            break;

        case TokenType::KW_DOUBLE:
            type.name = string_pool_.intern("DOUBLE PRECISION");
            advance();
            if (checkKeyword(TokenType::KW_PRECISION)) advance();
            break;

        case TokenType::KW_REAL:
            type.name = string_pool_.intern("REAL");
            advance();
            break;

        case TokenType::KW_DECIMAL:
            type.name = string_pool_.intern("DECIMAL");
            advance();
            break;

        case TokenType::KW_NUMERIC:
            type.name = string_pool_.intern("NUMERIC");
            advance();
            break;

        case TokenType::KW_DECFLOAT:
            type.name = string_pool_.intern("DECFLOAT");
            advance();
            break;

        case TokenType::KW_CHAR:
        case TokenType::KW_CHARACTER:
            type.name = string_pool_.intern("CHAR");
            advance();
            if (matchKeyword(TokenType::KW_VARYING)) {
                type.name = string_pool_.intern("VARCHAR");
            }
            break;

        case TokenType::KW_VARCHAR:
            type.name = string_pool_.intern("VARCHAR");
            advance();
            break;

        case TokenType::KW_NCHAR:
            type.name = string_pool_.intern("NCHAR");
            advance();
            if (matchKeyword(TokenType::KW_VARYING)) {
                type.name = string_pool_.intern("NCHAR VARYING");
            }
            break;

        case TokenType::KW_VARBINARY:
            type.name = string_pool_.intern("VARBINARY");
            advance();
            break;

        case TokenType::KW_BOOLEAN:
            type.name = string_pool_.intern("BOOLEAN");
            advance();
            break;

        case TokenType::KW_DATE:
            type.name = string_pool_.intern("DATE");
            advance();
            break;

        case TokenType::KW_TIME:
            type.name = string_pool_.intern("TIME");
            advance();
            break;

        case TokenType::KW_TIMESTAMP:
            type.name = string_pool_.intern("TIMESTAMP");
            advance();
            break;

        case TokenType::KW_BLOB:
            type.name = string_pool_.intern("BLOB");
            advance();
            // BLOB SUB_TYPE n or BLOB SUB_TYPE TEXT/BINARY
            if (matchKeyword(TokenType::KW_SUB_TYPE)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    type.precision = static_cast<int32_t>(current_token_.value.int_value);
                    advance();
                } else if (check(TokenType::IDENTIFIER)) {
                    // Check for TEXT or BINARY as identifier
                    std::string_view subtype = lexer_.getTokenText(current_token_);
                    if (subtype == "TEXT" || subtype == "text") {
                        type.precision = 1;  // SUB_TYPE TEXT
                    } else if (subtype == "BINARY" || subtype == "binary") {
                        type.precision = 0;  // SUB_TYPE BINARY
                    }
                    advance();
                } else if (matchKeyword(TokenType::KW_BINARY)) {
                    type.precision = 0;  // SUB_TYPE BINARY as keyword
                }
            }
            break;

        default:
            error("Expected type name");
            break;
    }

    return type;
}

// =============================================================================
// Schema Path Parsing
// =============================================================================

ast::SchemaPath Parser::parseSchemaPath() {
    ast::SchemaPath path;

    if (!check(TokenType::IDENTIFIER)) {
        error("Expected identifier");
        return path;
    }

    if (check(TokenType::IDENTIFIER)) {
        path.components.push_back(internFromLexer(current_token_.value.string_id));
        advance();
    }

    while (match(TokenType::DOT)) {
        error("Firebird does not support schema-qualified names");
        if (check(TokenType::IDENTIFIER)) {
            path.components.push_back(internFromLexer(current_token_.value.string_id));
            advance();
        } else {
            error("Expected identifier in schema path");
            break;
        }
    }

    return path;
}

ast::SchemaPath Parser::parseTableReference() {
    return parseSchemaPath();
}

// =============================================================================
// DDL Implementation
// =============================================================================

Statement* Parser::parseCreateStatement() {
    // Handle OR REPLACE / OR ALTER
    bool or_replace = false;
    if (matchKeyword(TokenType::KW_OR)) {
        if (matchKeyword(TokenType::KW_REPLACE) || matchKeyword(TokenType::KW_ALTER)) {
            or_replace = true;
        } else {
            error("Expected REPLACE or ALTER after OR");
            return nullptr;
        }
    }

    // Handle GLOBAL TEMPORARY
    bool temporary = false;
    bool global_temp = false;
    if (matchKeyword(TokenType::KW_GLOBAL)) {
        if (matchKeyword(TokenType::KW_TEMPORARY)) {
            temporary = true;
            global_temp = true;
        } else {
            error("Expected TEMPORARY after GLOBAL");
            return nullptr;
        }
    } else if (matchKeyword(TokenType::KW_TEMPORARY)) {
        temporary = true;
    }

    // Handle UNIQUE for indexes
    bool unique = false;
    if (matchKeyword(TokenType::KW_UNIQUE)) {
        unique = true;
    }

    // Dispatch based on object type
    if (matchKeyword(TokenType::KW_DATABASE)) {
        return parseCreateDatabase();
    }
    if (matchKeyword(TokenType::KW_TABLE)) {
        auto* stmt = parseCreateTableImpl(or_replace, temporary, global_temp);
        return stmt;
    }
    if (matchKeyword(TokenType::KW_INDEX)) {
        auto* stmt = parseCreateIndexImpl(unique, false);
        return stmt;
    }
    if (check(TokenType::KW_ASCENDING) || check(TokenType::KW_DESCENDING)) {
        // CREATE ASC[ENDING]/DESC[ENDING] INDEX
        bool ascending = matchKeyword(TokenType::KW_ASCENDING);
        if (!ascending) matchKeyword(TokenType::KW_DESCENDING);
        consume(TokenType::KW_INDEX, "Expected INDEX");
        auto* stmt = parseCreateIndexImpl(unique, !ascending);
        return stmt;
    }
    if (matchKeyword(TokenType::KW_VIEW)) {
        auto* stmt = parseCreateViewImpl(or_replace);
        return stmt;
    }
    if (matchKeyword(TokenType::KW_GENERATOR) || matchKeyword(TokenType::KW_SEQUENCE)) {
        return parseCreateSequenceImpl();
    }
    if (matchKeyword(TokenType::KW_PROCEDURE)) {
        return parseCreateProcedure(or_replace);
    }
    if (matchKeyword(TokenType::KW_FUNCTION)) {
        return parseCreateFunction(or_replace);
    }
    if (matchKeyword(TokenType::KW_TRIGGER)) {
        return parseCreateTrigger(or_replace);
    }
    if (matchKeyword(TokenType::KW_DOMAIN)) {
        return parseCreateDomain();
    }
    if (matchKeyword(TokenType::KW_EXCEPTION)) {
        return parseCreateException(or_replace);
    }
    if (matchKeyword(TokenType::KW_ROLE)) {
        return parseCreateRole(or_replace);
    }
    if (matchKeyword(TokenType::KW_PACKAGE)) {
        return parseCreatePackage(or_replace);
    }

    error("Unknown CREATE object type");
    return nullptr;
}

Statement* Parser::parseCreateDatabase() {
    auto* stmt = allocate<ast::CreateDatabaseStmt>();
    stmt->span = toV2Span(current_token_.span);

    auto parseValueTokenText = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL) || check(TokenType::Q_STRING_LITERAL)) {
            auto id = internFromLexer(current_token_.value.string_id);
            std::string value = std::string(string_pool_.get(id));
            advance();
            return value;
        }
        if (check(TokenType::IDENTIFIER)) {
            auto id = parseIdentifier();
            return std::string(string_pool_.get(id));
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            std::string value = std::to_string(current_token_.value.int_value);
            advance();
            return value;
        }
        return {};
    };

    auto add_option = [&](const std::string& key, const std::string& value) {
        if (key.empty() || value.empty()) {
            return;
        }
        ast::DatabaseOption opt;
        opt.key = string_pool_.intern(key);
        opt.value = string_pool_.intern(value);
        stmt->options.push_back(opt);
    };

    std::string db_path_text;
    if (check(TokenType::STRING_LITERAL) || check(TokenType::Q_STRING_LITERAL)) {
        auto id = internFromLexer(current_token_.value.string_id);
        db_path_text = std::string(string_pool_.get(id));
        advance();
    } else if (check(TokenType::IDENTIFIER)) {
        auto id = parseIdentifier();
        db_path_text = std::string(string_pool_.get(id));
    } else {
        error("Expected database path after CREATE DATABASE");
        return stmt;
    }

    FirebirdDatabaseSpec spec = parseFirebirdDatabaseSpec(db_path_text);
    std::string server = spec.server.empty() ? "localhost" : spec.server;
    std::string db_name = deriveFirebirdDatabaseName(spec.file_path);
    if (db_name.empty()) {
        error("Database name is empty");
        return stmt;
    }

    auto path_components = splitFirebirdPathComponents(spec.file_path);
    stmt->database_path = buildEmulatedDatabasePath(string_pool_,
                                                    "firebird",
                                                    server,
                                                    path_components,
                                                    db_name);
    if (!db_path_text.empty()) {
        stmt->source_spec = string_pool_.intern(db_path_text);
    }

    // Optional Firebird parameters (ignored for emulation)
    while (!atEnd() && !check(TokenType::SEMICOLON)) {
        if (matchKeyword(TokenType::KW_USER)) {
            std::string value = parseValueTokenText();
            if (value.empty()) {
                error("Expected identifier or string literal after USER");
                break;
            }
            add_option("user", value);
        } else if (matchKeyword(TokenType::KW_PASSWORD)) {
            std::string value = parseValueTokenText();
            if (value.empty()) {
                error("Expected identifier or string literal after PASSWORD");
                break;
            }
            add_option("password", value);
        } else if (matchKeyword(TokenType::KW_PAGE)) {
            matchKeyword(TokenType::KW_SIZE);
            std::string value = parseValueTokenText();
            if (value.empty()) {
                error("Expected page size after PAGE [SIZE]");
                break;
            }
            add_option("page_size", value);
        } else if (matchKeyword(TokenType::KW_DEFAULT)) {
            if (matchKeyword(TokenType::KW_CHARACTER)) {
                matchKeyword(TokenType::KW_SET);
                std::string value = parseValueTokenText();
                if (value.empty()) {
                    error("Expected character set after DEFAULT CHARACTER SET");
                    break;
                }
                add_option("default_character_set", value);
            } else if (matchKeyword(TokenType::KW_COLLATION)) {
                std::string value = parseValueTokenText();
                if (value.empty()) {
                    error("Expected collation after DEFAULT COLLATION");
                    break;
                }
                add_option("default_collation", value);
            } else {
                break;
            }
        } else {
            break;
        }
    }

    return stmt;
}

Statement* Parser::parseAlterStatement() {
    if (matchKeyword(TokenType::KW_TABLE)) {
        return parseAlterTableImpl();
    }
    if (matchKeyword(TokenType::KW_DATABASE)) {
        return parseAlterDatabase();
    }
    if (matchKeyword(TokenType::KW_DOMAIN)) {
        return parseAlterDomainImpl();
    }
    if (matchKeyword(TokenType::KW_INDEX)) {
        return parseAlterIndexImpl();
    }
    if (matchKeyword(TokenType::KW_SEQUENCE) || matchKeyword(TokenType::KW_GENERATOR)) {
        auto* stmt = allocate<ast::AlterSequenceStmt>();
        stmt->sequence_path = parseSchemaPath();
        while (!atEnd() && !check(TokenType::SEMICOLON)) {
            if (matchKeyword(TokenType::KW_RESTART)) {
                matchKeyword(TokenType::KW_WITH);
                if (check(TokenType::INTEGER_LITERAL)) {
                    stmt->restart_with = current_token_.value.int_value;
                    advance();
                } else {
                    error("Expected integer value after RESTART");
                }
            } else if (matchKeyword(TokenType::KW_INCREMENT)) {
                matchKeyword(TokenType::KW_BY);
                if (check(TokenType::INTEGER_LITERAL)) {
                    stmt->increment_by = current_token_.value.int_value;
                    advance();
                } else {
                    error("Expected integer value after INCREMENT");
                }
            } else {
                break;
            }
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_VIEW)) {
        return parseCreateViewImpl(true);
    }
    if (matchKeyword(TokenType::KW_ROLE)) {
        parseIdentifier();
        error("ALTER ROLE is not supported in Firebird parser yet");
        return nullptr;
    }
    if (matchKeyword(TokenType::KW_PROCEDURE)) {
        return parseCreateProcedure(true);
    }
    if (matchKeyword(TokenType::KW_FUNCTION)) {
        return parseCreateFunction(true);
    }
    if (matchKeyword(TokenType::KW_TRIGGER)) {
        return parseCreateTrigger(true);
    }
    if (matchKeyword(TokenType::KW_PACKAGE)) {
        return parseCreatePackage(true);
    }
    if (matchKeyword(TokenType::KW_EXCEPTION)) {
        return parseCreateException(true);
    }
    if (matchKeyword(TokenType::KW_USER)) {
        error("ALTER USER is not supported in Firebird parser yet");
        return nullptr;
    }
    if (matchKeyword(TokenType::KW_MAPPING)) {
        error("ALTER MAPPING is not supported in Firebird parser yet");
        return nullptr;
    }
    if (matchKeyword(TokenType::KW_SHADOW)) {
        error("ALTER SHADOW is not supported in Firebird parser yet");
        return nullptr;
    }
    error("ALTER statement for this object type not yet implemented");
    return nullptr;
}

Statement* Parser::parseDropStatement() {
    // Handle IF EXISTS
    bool if_exists = false;

    if (matchKeyword(TokenType::KW_DATABASE)) {
        return parseDropDatabase();
    }

    if (matchKeyword(TokenType::KW_TABLE)) {
        if (matchKeyword(TokenType::KW_IF)) {
            consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        return parseDropTableImpl(if_exists);
    }
    if (matchKeyword(TokenType::KW_INDEX)) {
        if (matchKeyword(TokenType::KW_IF)) {
            consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        return parseDropIndexImpl(if_exists);
    }
    if (matchKeyword(TokenType::KW_VIEW)) {
        if (matchKeyword(TokenType::KW_IF)) {
            consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        return parseDropViewImpl(if_exists);
    }
    if (matchKeyword(TokenType::KW_DOMAIN)) {
        if (matchKeyword(TokenType::KW_IF)) {
            consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        return parseDropDomainImpl(if_exists);
    }
    if (matchKeyword(TokenType::KW_GENERATOR) || matchKeyword(TokenType::KW_SEQUENCE)) {
        return parseDropSequenceImpl(if_exists);
    }
    if (matchKeyword(TokenType::KW_PROCEDURE)) {
        if (matchKeyword(TokenType::KW_IF)) {
            consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        return parseDropProcedureImpl(if_exists);
    }
    if (matchKeyword(TokenType::KW_FUNCTION)) {
        if (matchKeyword(TokenType::KW_IF)) {
            consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        return parseDropFunctionImpl(if_exists);
    }
    if (matchKeyword(TokenType::KW_TRIGGER)) {
        if (matchKeyword(TokenType::KW_IF)) {
            consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        return parseDropTriggerImpl(if_exists);
    }
    if (matchKeyword(TokenType::KW_PACKAGE)) {
        if (matchKeyword(TokenType::KW_IF)) {
            consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        return parseDropPackageImpl(if_exists);
    }
    if (matchKeyword(TokenType::KW_ROLE)) {
        if (matchKeyword(TokenType::KW_IF)) {
            consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        return parseDropRoleImpl(if_exists);
    }
    if (matchKeyword(TokenType::KW_EXCEPTION)) {
        if (matchKeyword(TokenType::KW_IF)) {
            consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        return parseDropExceptionImpl(if_exists);
    }
    if (matchKeyword(TokenType::KW_USER)) {
        error("DROP USER is not supported in Firebird parser yet");
        return nullptr;
    }
    if (matchKeyword(TokenType::KW_MAPPING)) {
        error("DROP MAPPING is not supported in Firebird parser yet");
        return nullptr;
    }
    if (matchKeyword(TokenType::KW_SHADOW)) {
        error("DROP SHADOW is not supported in Firebird parser yet");
        return nullptr;
    }

    error("DROP statement for this object type not yet implemented");
    return nullptr;
}

Statement* Parser::parseDropDatabase() {
    auto* stmt = allocate<ast::DropDatabaseStmt>();
    stmt->span = toV2Span(current_token_.span);

    if (matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    std::string db_path_text;
    if (check(TokenType::STRING_LITERAL) || check(TokenType::Q_STRING_LITERAL)) {
        auto id = internFromLexer(current_token_.value.string_id);
        db_path_text = std::string(string_pool_.get(id));
        advance();
    } else if (check(TokenType::IDENTIFIER)) {
        auto id = parseIdentifier();
        db_path_text = std::string(string_pool_.get(id));
    } else {
        error("Expected database path after DROP DATABASE");
        return stmt;
    }

    FirebirdDatabaseSpec spec = parseFirebirdDatabaseSpec(db_path_text);
    std::string server = spec.server.empty() ? "localhost" : spec.server;
    std::string db_name = deriveFirebirdDatabaseName(spec.file_path);
    if (db_name.empty()) {
        error("Database name is empty");
        return stmt;
    }

    auto path_components = splitFirebirdPathComponents(spec.file_path);
    stmt->database_path = buildEmulatedDatabasePath(string_pool_,
                                                    "firebird",
                                                    server,
                                                    path_components,
                                                    db_name);
    return stmt;
}

Statement* Parser::parseAlterDatabase() {
    auto* stmt = allocate<ast::AlterDatabaseStmt>();
    stmt->span = toV2Span(current_token_.span);

    auto matchIdentifierText = [&](const char* keyword) -> bool {
        if (!check(TokenType::IDENTIFIER)) {
            return false;
        }
        std::string_view text = lexer_.getTokenText(current_token_);
        size_t len = std::strlen(keyword);
        if (text.size() != len) {
            return false;
        }
        for (size_t i = 0; i < len; ++i) {
            char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
            char b = static_cast<char>(std::tolower(static_cast<unsigned char>(keyword[i])));
            if (a != b) {
                return false;
            }
        }
        advance();
        return true;
    };

    auto parseAliasName = [&]() -> ast::StringPool::StringId {
        if (check(TokenType::STRING_LITERAL) || check(TokenType::Q_STRING_LITERAL)) {
            auto id = internFromLexer(current_token_.value.string_id);
            advance();
            return id;
        }
        return parseIdentifier();
    };

    std::string db_path_text;
    if (check(TokenType::STRING_LITERAL) || check(TokenType::Q_STRING_LITERAL)) {
        auto id = internFromLexer(current_token_.value.string_id);
        db_path_text = std::string(string_pool_.get(id));
        advance();
    } else if (check(TokenType::IDENTIFIER)) {
        auto id = parseIdentifier();
        db_path_text = std::string(string_pool_.get(id));
    } else {
        error("Expected database path after ALTER DATABASE");
        return stmt;
    }

    FirebirdDatabaseSpec spec = parseFirebirdDatabaseSpec(db_path_text);
    std::string server = spec.server.empty() ? "localhost" : spec.server;
    std::string db_name = deriveFirebirdDatabaseName(spec.file_path);
    if (db_name.empty()) {
        error("Database name is empty");
        return stmt;
    }

    auto path_components = splitFirebirdPathComponents(spec.file_path);
    stmt->database_path = buildEmulatedDatabasePath(string_pool_,
                                                    "firebird",
                                                    server,
                                                    path_components,
                                                    db_name);

    if (matchIdentifierText("ALIAS")) {
        if (matchKeyword(TokenType::KW_ADD)) {
            stmt->action = ast::AlterDatabaseAction::ADD_ALIAS;
            stmt->alias = parseAliasName();
            return stmt;
        }
        if (matchKeyword(TokenType::KW_DROP)) {
            stmt->action = ast::AlterDatabaseAction::DROP_ALIAS;
            stmt->alias = parseAliasName();
            return stmt;
        }
        error("Expected ADD or DROP after ALIAS");
        return stmt;
    }

    if (matchKeyword(TokenType::KW_RENAME)) {
        // Firebird does not support ALTER DATABASE ... RENAME TO ...
        error("ALTER DATABASE RENAME is not supported in Firebird");
        return stmt;
    }

    if (matchKeyword(TokenType::KW_SET)) {
        auto parseValueTokenText = [&]() -> std::string {
            if (check(TokenType::STRING_LITERAL) || check(TokenType::Q_STRING_LITERAL)) {
                auto id = internFromLexer(current_token_.value.string_id);
                std::string value = std::string(string_pool_.get(id));
                advance();
                return value;
            }
            if (check(TokenType::IDENTIFIER)) {
                auto id = parseIdentifier();
                return std::string(string_pool_.get(id));
            }
            if (check(TokenType::INTEGER_LITERAL)) {
                std::string value = std::to_string(current_token_.value.int_value);
                advance();
                return value;
            }
            return {};
        };

        while (!atEnd() && !check(TokenType::SEMICOLON)) {
            if (matchKeyword(TokenType::KW_DEFAULT)) {
                if (matchKeyword(TokenType::KW_CHARACTER)) {
                    matchKeyword(TokenType::KW_SET);
                    std::string value = parseValueTokenText();
                    if (value.empty()) {
                        error("Expected character set after DEFAULT CHARACTER SET");
                        break;
                    }
                    ast::AlterDatabaseOption opt;
                    opt.key = string_pool_.intern("default_character_set");
                    opt.value = string_pool_.intern(value);
                    stmt->options.push_back(opt);
                } else if (matchKeyword(TokenType::KW_COLLATION)) {
                    std::string value = parseValueTokenText();
                    if (value.empty()) {
                        error("Expected collation after DEFAULT COLLATION");
                        break;
                    }
                    ast::AlterDatabaseOption opt;
                    opt.key = string_pool_.intern("default_collation");
                    opt.value = string_pool_.intern(value);
                    stmt->options.push_back(opt);
                } else {
                    error("Expected CHARACTER SET or COLLATION after DEFAULT");
                    break;
                }
            } else {
                break;
            }
        }

        if (!stmt->options.empty()) {
            stmt->action = ast::AlterDatabaseAction::SET_OPTIONS;
            return stmt;
        }
    }

    if (matchIdentifierText("OWNER")) {
        consume(TokenType::KW_TO, "Expected TO after OWNER");
        stmt->action = ast::AlterDatabaseAction::SET_OWNER;
        stmt->owner = parseIdentifier();
        return stmt;
    }

    error("ALTER DATABASE options are not supported in Firebird parser");
    return stmt;
}

Statement* Parser::parseRecreateStatement() {
    // RECREATE = DROP IF EXISTS + CREATE
    if (matchKeyword(TokenType::KW_TABLE)) {
        auto* stmt = parseCreateTableImpl(false, false, false);
        if (stmt) {
            static_cast<ast::CreateTableStmt*>(stmt)->or_replace = true;
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_VIEW)) {
        auto* stmt = parseCreateViewImpl(true);
        return stmt;
    }
    if (matchKeyword(TokenType::KW_INDEX)) {
        auto* stmt = parseCreateIndexImpl(false, false);
        return stmt;
    }
    if (matchKeyword(TokenType::KW_SEQUENCE) || matchKeyword(TokenType::KW_GENERATOR)) {
        auto* stmt = parseCreateSequenceImpl();
        if (stmt) {
            stmt->or_replace = true;
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_PROCEDURE)) {
        return parseCreateProcedure(true);
    }
    if (matchKeyword(TokenType::KW_FUNCTION)) {
        return parseCreateFunction(true);
    }
    if (matchKeyword(TokenType::KW_TRIGGER)) {
        return parseCreateTrigger(true);
    }
    if (matchKeyword(TokenType::KW_PACKAGE)) {
        return parseCreatePackage(true);
    }
    if (matchKeyword(TokenType::KW_EXCEPTION)) {
        return parseCreateException(true);
    }
    if (matchKeyword(TokenType::KW_ROLE)) {
        parseIdentifier();
        error("RECREATE ROLE is not supported in Firebird parser yet");
        return nullptr;
    }
    if (matchKeyword(TokenType::KW_USER)) {
        error("RECREATE USER is not supported in Firebird parser yet");
        return nullptr;
    }
    if (matchKeyword(TokenType::KW_MAPPING)) {
        error("RECREATE MAPPING is not supported in Firebird parser yet");
        return nullptr;
    }
    if (matchKeyword(TokenType::KW_SHADOW)) {
        error("RECREATE SHADOW is not supported in Firebird parser yet");
        return nullptr;
    }

    error("RECREATE statement for this object type not yet implemented");
    return nullptr;
}

// Helper implementation for CREATE TABLE
ast::CreateTableStmt* Parser::parseCreateTableImpl(bool or_replace, bool temporary, bool global_temp) {
    auto* stmt = allocate<ast::CreateTableStmt>();
    stmt->or_replace = or_replace;
    if (temporary || global_temp) {
        stmt->temp_type = ast::TempTableType::GLOBAL;
        stmt->on_commit = ast::TempOnCommitAction::DELETE_ROWS;  // Firebird default
    }

    // Table name
    stmt->table_path = parseSchemaPath();

    // Column definitions
    consume(TokenType::LEFT_PAREN, "Expected '(' after table name");

    do {
        // Check for table constraint first
        if (check(TokenType::KW_PRIMARY) || check(TokenType::KW_FOREIGN) ||
            check(TokenType::KW_UNIQUE) || check(TokenType::KW_CHECK) ||
            check(TokenType::KW_CONSTRAINT)) {
            auto* constraint = parseTableConstraint();
            if (constraint) {
                stmt->constraints.push_back(constraint);
            }
        } else {
            // Column definition
            auto* col = parseColumnDef();
            if (col) {
                stmt->columns.push_back(col);
            }
        }
    } while (match(TokenType::COMMA));

    consume(TokenType::RIGHT_PAREN, "Expected ')' after column definitions");

    // Optional ON COMMIT clause for GTT
    if ((temporary || global_temp) && matchKeyword(TokenType::KW_ON)) {
        consume(TokenType::KW_COMMIT, "Expected COMMIT after ON");
        if (matchKeyword(TokenType::KW_DELETE)) {
            stmt->on_commit = ast::TempOnCommitAction::DELETE_ROWS;
        } else if (matchKeyword(TokenType::KW_PRESERVE)) {
            stmt->on_commit = ast::TempOnCommitAction::PRESERVE_ROWS;
        }
        matchKeyword(TokenType::KW_ROWS);  // Optional ROWS keyword
    }

    if (matchKeyword(TokenType::KW_PARTITION)) {
        error("Firebird does not support partitioned tables");
        if (matchKeyword(TokenType::KW_BY)) {
            if (match(TokenType::LEFT_PAREN)) {
                int depth = 1;
                while (depth > 0 && !atEnd()) {
                    if (match(TokenType::LEFT_PAREN)) {
                        depth++;
                    } else if (match(TokenType::RIGHT_PAREN)) {
                        depth--;
                    } else {
                        advance();
                    }
                }
            }
        }
    }

    return stmt;
}

// Helper implementation for CREATE INDEX
ast::CreateIndexStmt* Parser::parseCreateIndexImpl(bool unique, bool descending) {
    auto* stmt = allocate<ast::CreateIndexStmt>();
    stmt->unique = unique;

    // Index name
    if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
        stmt->index_name = parseIdentifier();
    }

    // ON table
    consume(TokenType::KW_ON, "Expected ON after index name");
    stmt->table_path = parseSchemaPath();

    // Column list
    consume(TokenType::LEFT_PAREN, "Expected '(' after table name");

    do {
        ast::IndexColumn col;

        // Could be column name or expression
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            col.column = parseIdentifier();
        } else {
            col.expr = parseExpression();
        }

        col.ascending = !descending;

        // Optional ASC/DESC per column
        if (matchKeyword(TokenType::KW_ASC) || matchKeyword(TokenType::KW_ASCENDING)) {
            col.ascending = true;
        } else if (matchKeyword(TokenType::KW_DESC) || matchKeyword(TokenType::KW_DESCENDING)) {
            col.ascending = false;
        }

        stmt->columns.push_back(col);
    } while (match(TokenType::COMMA));

    consume(TokenType::RIGHT_PAREN, "Expected ')' after column list");

    // Optional WHERE clause (partial index)
    if (matchKeyword(TokenType::KW_WHERE)) {
        stmt->where_clause = parseExpression();
    }

    return stmt;
}

// Helper implementation for CREATE VIEW
ast::CreateViewStmt* Parser::parseCreateViewImpl(bool or_replace) {
    auto* stmt = allocate<ast::CreateViewStmt>();
    stmt->or_replace = or_replace;

    // View name
    stmt->view_path = parseSchemaPath();

    // Optional column list
    if (match(TokenType::LEFT_PAREN)) {
        do {
            stmt->column_names.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ')' after column list");
    }

    // AS
    consume(TokenType::KW_AS, "Expected AS before view query");

    // The SELECT statement (parse as subquery for now)
    stmt->query = parseSelectStatement();

    // Optional WITH CHECK OPTION
    if (matchKeyword(TokenType::KW_WITH)) {
        if (matchKeyword(TokenType::KW_CHECK)) {
            consume(TokenType::KW_OPTION, "Expected OPTION after CHECK");
            stmt->with_check_option = true;
        }
    }

    return stmt;
}

// Helper implementation for CREATE SEQUENCE/GENERATOR
ast::CreateSequenceStmt* Parser::parseCreateSequenceImpl() {
    auto* stmt = allocate<ast::CreateSequenceStmt>();

    // Sequence name
    stmt->sequence_path = parseSchemaPath();

    // Optional clauses
    while (!atEnd() && !check(TokenType::SEMICOLON)) {
        if (matchKeyword(TokenType::KW_START)) {
            matchKeyword(TokenType::KW_WITH);  // Optional WITH
            stmt->start_with = current_token_.value.int_value;
            match(TokenType::INTEGER_LITERAL);
        } else if (matchKeyword(TokenType::KW_INCREMENT)) {
            matchKeyword(TokenType::KW_BY);  // Optional BY
            stmt->increment_by = current_token_.value.int_value;
            match(TokenType::INTEGER_LITERAL);
        } else {
            break;
        }
    }

    return stmt;
}

// Helper implementations for ALTER statements
ast::AlterTableStmt* Parser::parseAlterTableImpl() {
    auto* stmt = allocate<ast::AlterTableStmt>();
    stmt->table_path = parseSchemaPath();

    // Parse the alter action
    if (matchKeyword(TokenType::KW_ADD)) {
        if (matchKeyword(TokenType::KW_COLUMN) || check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->action = ast::AlterTableAction::ADD_COLUMN;
            stmt->column = parseColumnDef();
        } else if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            stmt->action = ast::AlterTableAction::ADD_CONSTRAINT;
            // Parse constraint
        }
    } else if (matchKeyword(TokenType::KW_DROP)) {
        if (matchKeyword(TokenType::KW_COLUMN)) {
            stmt->action = ast::AlterTableAction::DROP_COLUMN;
            stmt->column_name = parseIdentifier();
        } else if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            stmt->action = ast::AlterTableAction::DROP_CONSTRAINT;
            stmt->constraint_name = parseIdentifier();
        }
    } else if (matchKeyword(TokenType::KW_ALTER)) {
        matchKeyword(TokenType::KW_COLUMN);  // Optional in Firebird
        stmt->column_name = parseIdentifier();
        if (matchKeyword(TokenType::KW_TO)) {
            stmt->action = ast::AlterTableAction::RENAME_COLUMN;
            stmt->new_name = parseIdentifier();
        } else {
            if (matchKeyword(TokenType::KW_POSITION)) {
                if (!check(TokenType::INTEGER_LITERAL)) {
                    error("ALTER TABLE ALTER COLUMN POSITION requires an integer literal");
                    return nullptr;
                }
                stmt->action = ast::AlterTableAction::ALTER_COLUMN_POSITION;
                stmt->position_1_based = current_token_.value.int_value;
                stmt->has_position = true;
                match(TokenType::INTEGER_LITERAL);
                return stmt;
            }
            if (matchKeyword(TokenType::KW_SET)) {
                if (matchKeyword(TokenType::KW_DEFAULT)) {
                    stmt->action = ast::AlterTableAction::ALTER_COLUMN_SET_DEFAULT;
                    stmt->default_expr = parseExpression();
                    stmt->has_default_expr = (stmt->default_expr != nullptr);
                    return stmt;
                }
                if (matchKeyword(TokenType::KW_NOT)) {
                    consume(TokenType::KW_NULL, "Expected NULL after SET NOT");
                    stmt->action = ast::AlterTableAction::ALTER_COLUMN_SET_NOT_NULL;
                    return stmt;
                }
                error("Expected DEFAULT or NOT NULL after SET");
                return nullptr;
            }
            if (matchKeyword(TokenType::KW_DROP)) {
                if (matchKeyword(TokenType::KW_DEFAULT)) {
                    stmt->action = ast::AlterTableAction::ALTER_COLUMN_DROP_DEFAULT;
                    return stmt;
                }
                if (matchKeyword(TokenType::KW_NOT)) {
                    consume(TokenType::KW_NULL, "Expected NULL after DROP NOT");
                    stmt->action = ast::AlterTableAction::ALTER_COLUMN_DROP_NOT_NULL;
                    return stmt;
                }
                error("Expected DEFAULT or NOT NULL after DROP");
                return nullptr;
            }

            stmt->action = ast::AlterTableAction::ALTER_COLUMN;
            // Parse column alteration (TYPE, SET DEFAULT, DROP DEFAULT, etc.)
        }
    } else if (matchKeyword(TokenType::KW_RENAME)) {
        stmt->action = ast::AlterTableAction::RENAME_TABLE;
        if (matchKeyword(TokenType::KW_TO)) {
            stmt->new_name = parseIdentifier();
        } else {
            stmt->new_name = parseIdentifier();
        }
    } else if (matchKeyword(TokenType::KW_SET)) {
        error("ALTER TABLE SET is not supported in Firebird parser");
        return nullptr;
    }

    return stmt;
}

Statement* Parser::parseAlterDomainImpl() {
    auto* stmt = allocate<ast::AlterDomainStmt>();
    stmt->domain_path = parseSchemaPath();

    if (matchKeyword(TokenType::KW_SET)) {
        if (matchKeyword(TokenType::KW_DEFAULT)) {
            stmt->action = ast::AlterDomainAction::SET_DEFAULT;
            stmt->value = extractExpressionText(parseExpression());
            return stmt;
        }
        error("Expected DEFAULT after SET in ALTER DOMAIN");
        return stmt;
    }

    if (matchKeyword(TokenType::KW_DROP)) {
        if (matchKeyword(TokenType::KW_DEFAULT)) {
            stmt->action = ast::AlterDomainAction::DROP_DEFAULT;
            return stmt;
        }
        if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            stmt->action = ast::AlterDomainAction::DROP_CONSTRAINT;
            stmt->constraint_name = parseIdentifier();
            return stmt;
        }
        error("Expected DEFAULT or CONSTRAINT after DROP in ALTER DOMAIN");
        return stmt;
    }

    if (matchKeyword(TokenType::KW_ADD)) {
        if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            if (!check(TokenType::KW_CHECK) && !check(TokenType::KW_DEFAULT) &&
                !check(TokenType::KW_NOT) && !check(TokenType::KW_NULL)) {
                // Constraint names aren't supported for ALTER DOMAIN ADD CHECK yet.
                parseIdentifier();
            }
        }

        if (matchKeyword(TokenType::KW_CHECK)) {
            consume(TokenType::LEFT_PAREN, "Expected ( after CHECK");
            stmt->action = ast::AlterDomainAction::ADD_CHECK;
            stmt->value = extractExpressionText(parseExpression());
            consume(TokenType::RIGHT_PAREN, "Expected ) after CHECK expression");
            return stmt;
        }
        error("Expected CHECK after ADD in ALTER DOMAIN");
        return stmt;
    }

    if (matchKeyword(TokenType::KW_TO)) {
        stmt->action = ast::AlterDomainAction::RENAME;
        stmt->new_name = parseIdentifier();
        return stmt;
    }

    error("Expected SET, DROP, ADD, or TO after domain name");
    return stmt;
}

// Stub for ALTER INDEX
Statement* Parser::parseAlterIndexImpl() {
    auto* stmt = allocate<ast::AlterIndexStmt>();
    stmt->span = toV2Span(current_token_.span);

    stmt->index_path = parseSchemaPath();

    if (matchKeyword(TokenType::KW_ACTIVE)) {
        stmt->action = ast::AlterIndexAction::ACTIVE;
    } else if (matchKeyword(TokenType::KW_INACTIVE)) {
        stmt->action = ast::AlterIndexAction::INACTIVE;
    } else if (matchKeyword(TokenType::KW_SET)) {
        stmt->action = ast::AlterIndexAction::SET_OPTIONS;
        consume(TokenType::LEFT_PAREN, "Expected '(' after SET in ALTER INDEX");

        auto equals_ignore = [](std::string_view lhs, std::string_view rhs) {
            if (lhs.size() != rhs.size()) {
                return false;
            }
            for (size_t i = 0; i < lhs.size(); ++i) {
                char a = static_cast<char>(std::tolower(static_cast<unsigned char>(lhs[i])));
                char b = static_cast<char>(std::tolower(static_cast<unsigned char>(rhs[i])));
                if (a != b) {
                    return false;
                }
            }
            return true;
        };

        auto parse_bool = [&]() -> bool {
            if (matchKeyword(TokenType::KW_TRUE)) return true;
            if (matchKeyword(TokenType::KW_FALSE)) return false;
            if (check(TokenType::INTEGER_LITERAL)) {
                bool value = current_token_.value.int_value != 0;
                advance();
                return value;
            }
            if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
                auto id = parseIdentifier();
                auto text = string_pool_.get(id);
                if (equals_ignore(text, "TRUE")) {
                    return true;
                }
                if (equals_ignore(text, "FALSE")) {
                    return false;
                }
            }
            error("Expected boolean value for index option");
            return false;
        };

        auto parse_double = [&]() -> double {
            if (check(TokenType::FLOAT_LITERAL)) {
                double value = current_token_.value.float_value;
                advance();
                return value;
            }
            if (check(TokenType::INTEGER_LITERAL)) {
                double value = static_cast<double>(current_token_.value.int_value);
                advance();
                return value;
            }
            error("Expected numeric value for index option");
            return 0.0;
        };

        while (!check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            auto name_id = parseIdentifier();
            auto name_text = string_pool_.get(name_id);
            consume(TokenType::EQUAL, "Expected '=' after index option name");

            if (equals_ignore(name_text, "BLOOM_FILTER")) {
                stmt->options.bloom_filter_enabled = parse_bool();
                stmt->options.bloom_filter_set = true;
            } else if (equals_ignore(name_text, "BLOOM_FPR")) {
                stmt->options.bloom_fpr = parse_double();
                stmt->options.bloom_fpr_set = true;
            } else {
                error("Unknown index option");
                break;
            }

            if (!match(TokenType::COMMA)) {
                break;
            }
        }

        consume(TokenType::RIGHT_PAREN, "Expected ')' after index options");
    } else {
        error("Expected ACTIVE, INACTIVE, or SET after ALTER INDEX");
    }

    return stmt;
}

Statement* Parser::parseAlterRenameMoveImpl(ast::DdlObjectType object_type) {
    bool if_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    ast::SchemaPath object_path = parseSchemaPath();

    if (matchKeyword(TokenType::KW_RENAME)) {
        consume(TokenType::KW_TO, "Expected TO after RENAME");
        auto* stmt = allocate<ast::RenameObjectStmt>();
        stmt->object_type = object_type;
        stmt->if_exists = if_exists;
        stmt->object_path = object_path;
        stmt->new_name = parseIdentifier();
        return stmt;
    }

    error("Expected RENAME TO after object name");
    return nullptr;
}

// Helper implementations for DROP statements
ast::DropTableStmt* Parser::parseDropTableImpl(bool if_exists) {
    auto* stmt = allocate<ast::DropTableStmt>();
    stmt->if_exists = if_exists;

    // Check IF EXISTS again (in case it wasn't parsed yet)
    if (!if_exists && matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    // Parse one or more table names
    do {
        stmt->tables.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    // CASCADE or RESTRICT
    if (matchKeyword(TokenType::KW_CASCADE)) {
        stmt->cascade = true;
    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
        stmt->restrict = true;
    }

    return stmt;
}

ast::DropIndexStmt* Parser::parseDropIndexImpl(bool if_exists) {
    auto* stmt = allocate<ast::DropIndexStmt>();
    stmt->if_exists = if_exists;

    // Check IF EXISTS again
    if (!if_exists && matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    // Parse index name as SchemaPath
    do {
        ast::SchemaPath path;
        path.components.push_back(parseIdentifier());
        stmt->indexes.push_back(path);
    } while (match(TokenType::COMMA));

    return stmt;
}

ast::DropViewStmt* Parser::parseDropViewImpl(bool if_exists) {
    auto* stmt = allocate<ast::DropViewStmt>();
    stmt->if_exists = if_exists;

    // Check IF EXISTS again
    if (!if_exists && matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    // Parse one or more view names
    do {
        stmt->views.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    // CASCADE or RESTRICT
    if (matchKeyword(TokenType::KW_CASCADE)) {
        stmt->cascade = true;
    }

    return stmt;
}

ast::DropDomainStmt* Parser::parseDropDomainImpl(bool if_exists) {
    auto* stmt = allocate<ast::DropDomainStmt>();
    stmt->if_exists = if_exists;

    if (!if_exists && matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->domains.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    if (matchKeyword(TokenType::KW_RESTRICT)) {
        stmt->restrict = true;
    } else if (matchKeyword(TokenType::KW_CASCADE)) {
        error("DROP DOMAIN does not support CASCADE");
    }

    return stmt;
}

Statement* Parser::parseDropSequenceImpl(bool if_exists) {
    auto* stmt = allocate<ast::DropSequenceStmt>();
    stmt->if_exists = if_exists;

    if (!if_exists && matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->sequences.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    if (matchKeyword(TokenType::KW_CASCADE)) {
        stmt->cascade = true;
    }

    return stmt;
}

Statement* Parser::parseDropFunctionImpl(bool if_exists) {
    auto* stmt = allocate<ast::DropFunctionStmt>();
    stmt->if_exists = if_exists;

    if (!if_exists && matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->functions.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    return stmt;
}

Statement* Parser::parseDropProcedureImpl(bool if_exists) {
    auto* stmt = allocate<ast::DropProcedureStmt>();
    stmt->if_exists = if_exists;

    if (!if_exists && matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->procedures.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    return stmt;
}

Statement* Parser::parseDropTriggerImpl(bool if_exists) {
    auto* stmt = allocate<ast::DropTriggerStmt>();
    stmt->if_exists = if_exists;

    if (stmt->if_exists) {
        error("DROP TRIGGER does not support IF EXISTS");
    }

    do {
        stmt->triggers.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    return stmt;
}

Statement* Parser::parseDropPackageImpl(bool if_exists) {
    auto* stmt = allocate<ast::DropPackageStmt>();
    stmt->if_exists = if_exists;

    if (!if_exists && matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->packages.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    return stmt;
}

Statement* Parser::parseDropRoleImpl(bool if_exists) {
    auto* stmt = allocate<ast::DropRoleStmt>();
    stmt->if_exists = if_exists;

    if (!if_exists && matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->roles.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    if (matchKeyword(TokenType::KW_CASCADE)) {
        stmt->cascade = true;
    }

    return stmt;
}

Statement* Parser::parseDropExceptionImpl(bool if_exists) {
    auto* stmt = allocate<ast::DropExceptionStmt>();
    stmt->if_exists = if_exists;

    if (!if_exists && matchKeyword(TokenType::KW_IF)) {
        consume(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->exceptions.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    return stmt;
}

// Wrappers that delegate to impl methods
Statement* Parser::parseCreateTable() { return parseCreateTableImpl(false, false, false); }
Statement* Parser::parseCreateOrAlterTable() { return parseCreateTableImpl(true, false, false); }
Statement* Parser::parseCreateIndex() { return parseCreateIndexImpl(false, false); }
Statement* Parser::parseCreateView() { return parseCreateViewImpl(false); }
Statement* Parser::parseCreateSequence() { return parseCreateSequenceImpl(); }
Statement* Parser::parseCreateProcedure(bool or_replace) {
    auto* stmt = allocate<ast::CreateProcedureStmt>();
    stmt->or_replace = or_replace;
    stmt->procedure_path = parseSchemaPath();

    if (match(TokenType::LEFT_PAREN)) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                ast::RoutineParam param;
                if (matchKeyword(TokenType::KW_IN)) {
                    param.mode = ast::RoutineParamMode::IN;
                } else if (matchIdentifierText("OUT")) {
                    param.mode = ast::RoutineParamMode::OUT;
                } else if (matchIdentifierText("INOUT")) {
                    param.mode = ast::RoutineParamMode::INOUT;
                }
                param.name = parseIdentifier();
                param.type = parseTypeName();
                if (matchKeyword(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                    param.default_value = parseExpression();
                    param.has_default = true;
                }
                stmt->params.push_back(std::move(param));
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected ')' after procedure parameters");
    }

    if (matchKeyword(TokenType::KW_RETURNS)) {
        consume(TokenType::LEFT_PAREN, "Expected '(' after RETURNS");
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                ast::RoutineParam param;
                param.mode = ast::RoutineParamMode::OUT;
                param.name = parseIdentifier();
                param.type = parseTypeName();
                stmt->params.push_back(std::move(param));
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected ')' after RETURNS");
    }

    if (matchKeyword(TokenType::KW_SQL)) {
        consume(TokenType::KW_SECURITY, "Expected SECURITY after SQL");
        if (matchKeyword(TokenType::KW_DEFINER)) {
            stmt->sql_security = ast::RoutineSqlSecurity::DEFINER;
        } else if (matchKeyword(TokenType::KW_INVOKER)) {
            stmt->sql_security = ast::RoutineSqlSecurity::INVOKER;
        }
    }

    consume(TokenType::KW_AS, "Expected AS before procedure body");
    std::string body = captureStatementBody();
    if (!body.empty()) {
        stmt->body = string_pool_.intern(body);
    }
    return stmt;
}
Statement* Parser::parseCreateFunction(bool or_replace) {
    auto* stmt = allocate<ast::CreateFunctionStmt>();
    stmt->or_replace = or_replace;
    stmt->function_path = parseSchemaPath();

    if (match(TokenType::LEFT_PAREN)) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                ast::RoutineParam param;
                if (matchKeyword(TokenType::KW_IN)) {
                    param.mode = ast::RoutineParamMode::IN;
                } else if (matchIdentifierText("OUT")) {
                    param.mode = ast::RoutineParamMode::OUT;
                } else if (matchIdentifierText("INOUT")) {
                    param.mode = ast::RoutineParamMode::INOUT;
                }
                param.name = parseIdentifier();
                param.type = parseTypeName();
                if (matchKeyword(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                    param.default_value = parseExpression();
                    param.has_default = true;
                }
                stmt->params.push_back(std::move(param));
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected ')' after function parameters");
    }

    consume(TokenType::KW_RETURNS, "Expected RETURNS");
    stmt->return_type = parseTypeName();

    while (matchKeyword(TokenType::KW_DETERMINISTIC)) {
        stmt->deterministic = true;
    }

    if (matchKeyword(TokenType::KW_SQL)) {
        consume(TokenType::KW_SECURITY, "Expected SECURITY after SQL");
        if (matchKeyword(TokenType::KW_DEFINER)) {
            stmt->sql_security = ast::RoutineSqlSecurity::DEFINER;
        } else if (matchKeyword(TokenType::KW_INVOKER)) {
            stmt->sql_security = ast::RoutineSqlSecurity::INVOKER;
        }
    }

    consume(TokenType::KW_AS, "Expected AS before function body");
    std::string body = captureStatementBody();
    if (!body.empty()) {
        stmt->body = string_pool_.intern(body);
    }
    return stmt;
}
Statement* Parser::parseCreateTrigger(bool or_replace) {
    auto* stmt = allocate<ast::CreateTriggerStmt>();
    stmt->or_replace = or_replace;
    stmt->trigger_name = parseIdentifier();

    if (matchKeyword(TokenType::KW_FOR)) {
        stmt->table_path = parseSchemaPath();
    } else {
        stmt->table_path = parseSchemaPath();
    }

    if (matchKeyword(TokenType::KW_ACTIVE)) {
        stmt->active = true;
    } else if (matchKeyword(TokenType::KW_INACTIVE)) {
        stmt->active = false;
    }

    if (matchKeyword(TokenType::KW_BEFORE)) {
        stmt->timing = ast::TriggerTiming::BEFORE;
    } else if (matchKeyword(TokenType::KW_AFTER)) {
        stmt->timing = ast::TriggerTiming::AFTER;
    } else {
        error("Expected BEFORE or AFTER in CREATE TRIGGER");
    }

    stmt->event_mask = 0;
    auto add_event = [&](ast::TriggerEvent event) {
        stmt->event_mask |= static_cast<uint8_t>(1u << static_cast<uint8_t>(event));
    };

    if (matchKeyword(TokenType::KW_INSERT)) {
        add_event(ast::TriggerEvent::INSERT);
    } else if (matchKeyword(TokenType::KW_UPDATE)) {
        add_event(ast::TriggerEvent::UPDATE);
    } else if (matchKeyword(TokenType::KW_DELETE)) {
        add_event(ast::TriggerEvent::DELETE);
    }

    if (stmt->event_mask == 0) {
        error("Expected INSERT, UPDATE, or DELETE in CREATE TRIGGER");
    }

    while (matchKeyword(TokenType::KW_OR)) {
        if (matchKeyword(TokenType::KW_INSERT)) {
            add_event(ast::TriggerEvent::INSERT);
        } else if (matchKeyword(TokenType::KW_UPDATE)) {
            add_event(ast::TriggerEvent::UPDATE);
        } else if (matchKeyword(TokenType::KW_DELETE)) {
            add_event(ast::TriggerEvent::DELETE);
        } else {
            error("Expected trigger event after OR");
            break;
        }
    }

    if (matchKeyword(TokenType::KW_FOR)) {
        matchIdentifierText("EACH");
        if (matchKeyword(TokenType::KW_ROW)) {
            stmt->granularity = ast::TriggerGranularity::FOR_EACH_ROW;
        }
    }

    if (matchKeyword(TokenType::KW_POSITION)) {
        if (check(TokenType::INTEGER_LITERAL)) {
            advance();
        }
    }

    consume(TokenType::KW_AS, "Expected AS before trigger body");
    std::string body = captureStatementBody();
    if (!body.empty()) {
        stmt->body = string_pool_.intern(body);
    }

    return stmt;
}
Statement* Parser::parseCreateDomain() {
    auto* stmt = allocate<ast::CreateDomainStmt>();
    stmt->domain_kind = ast::DomainKind::BASIC;
    stmt->domain_path = parseSchemaPath();

    // Optional AS keyword
    matchKeyword(TokenType::KW_AS);
    stmt->base_type = parseTypeName();

    while (!atEnd()) {
        if (matchKeyword(TokenType::KW_COLLATE)) {
            auto collation_id = parseIdentifier();
            stmt->has_collation = true;
            stmt->collation_name = std::string(string_pool_.get(collation_id));
            continue;
        }

        ast::StringPool::StringId constraint_name = ast::StringPool::INVALID_ID;
        if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            if (!check(TokenType::KW_CHECK) && !check(TokenType::KW_DEFAULT) &&
                !check(TokenType::KW_NOT) && !check(TokenType::KW_NULL)) {
                constraint_name = parseIdentifier();
            }
        }

        ast::DomainConstraint constraint;
        bool have_constraint = false;

        if (matchKeyword(TokenType::KW_NOT)) {
            consume(TokenType::KW_NULL, "Expected NULL after NOT");
            constraint.type = ast::DomainConstraintType::NOT_NULL;
            have_constraint = true;
        } else if (matchKeyword(TokenType::KW_NULL)) {
            constraint.type = ast::DomainConstraintType::NULL_ALLOWED;
            have_constraint = true;
        } else if (matchKeyword(TokenType::KW_DEFAULT)) {
            constraint.type = ast::DomainConstraintType::DEFAULT;
            constraint.expression = extractExpressionText(parseExpression());
            have_constraint = true;
        } else if (matchKeyword(TokenType::KW_CHECK)) {
            consume(TokenType::LEFT_PAREN, "Expected ( after CHECK");
            constraint.type = ast::DomainConstraintType::CHECK;
            constraint.expression = extractExpressionText(parseExpression());
            consume(TokenType::RIGHT_PAREN, "Expected ) after CHECK expression");
            have_constraint = true;
        }

        if (!have_constraint) {
            if (constraint_name != ast::StringPool::INVALID_ID) {
                error("Expected constraint after CONSTRAINT");
            }
            break;
        }

        constraint.name = constraint_name;
        stmt->constraints.push_back(std::move(constraint));
    }

    if (checkKeyword(TokenType::KW_WITH)) {
        error("Firebird CREATE DOMAIN does not support WITH blocks");
        synchronize();
    }

    stmt->has_dialect = true;
    stmt->dialect_tag = "firebird";
    return stmt;
}
Statement* Parser::parseCreateException(bool or_replace) {
    auto* stmt = allocate<ast::CreateExceptionStmt>();
    stmt->or_replace = or_replace;
    stmt->exception_path = parseSchemaPath();

    if (check(TokenType::STRING_LITERAL) || check(TokenType::Q_STRING_LITERAL)) {
        stmt->message = internFromLexer(current_token_.value.string_id);
        advance();
    } else if (check(TokenType::IDENTIFIER)) {
        stmt->message = parseIdentifier();
    } else {
        error("Expected exception message");
    }

    return stmt;
}

Statement* Parser::parseCreateRole(bool /*or_replace*/) {
    auto* stmt = allocate<ast::CreateRoleStmt>();
    stmt->role_name = parseIdentifier();
    return stmt;
}

Statement* Parser::parseCreatePackage(bool or_replace) {
    auto* stmt = allocate<ast::CreatePackageStmt>();
    stmt->or_replace = or_replace;

    if (matchKeyword(TokenType::KW_BODY)) {
        stmt->is_body = true;
    }

    stmt->package_path = parseSchemaPath();
    consume(TokenType::KW_AS, "Expected AS before package body");
    std::string body = captureStatementBody();
    if (!body.empty()) {
        if (stmt->is_body) {
            stmt->body = string_pool_.intern(body);
        } else {
            stmt->header = string_pool_.intern(body);
        }
    }

    return stmt;
}

// =============================================================================
// DML Implementation
// =============================================================================

Statement* Parser::parseSelectStatement() {
    auto* stmt = allocate<ast::SelectStmt>();

    // FIRST/SKIP (Firebird's LIMIT/OFFSET, comes right after SELECT)
    auto firstSkip = parseFirstSkip();
    if (firstSkip.first) {
        stmt->limit = firstSkip.first;
    }
    if (firstSkip.skip) {
        stmt->offset = firstSkip.skip;
    }

    // DISTINCT / ALL
    if (matchKeyword(TokenType::KW_DISTINCT)) {
        stmt->distinct = true;
    } else if (matchKeyword(TokenType::KW_ALL)) {
        stmt->all = true;
    }

    // Select list
    stmt->items = parseSelectList();

    // FROM clause
    if (matchKeyword(TokenType::KW_FROM)) {
        stmt->from = parseFromClause();

        // JOIN clauses
        while (check(TokenType::KW_INNER) || check(TokenType::KW_LEFT) ||
               check(TokenType::KW_RIGHT) || check(TokenType::KW_FULL) ||
               check(TokenType::KW_CROSS) || check(TokenType::KW_JOIN) ||
               check(TokenType::KW_NATURAL)) {
            auto* join = parseJoinClause();
            if (join) {
                if (!join->left) {
                    join->left = stmt->from;
                }
                stmt->joins.push_back(join);
            }
        }
    }

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        stmt->where = parseExpression();
    }

    // GROUP BY clause
    if (matchKeyword(TokenType::KW_GROUP)) {
        consume(TokenType::KW_BY, "Expected BY after GROUP");
        stmt->group_by = parseGroupByClause();
    }

    // HAVING clause
    if (matchKeyword(TokenType::KW_HAVING)) {
        stmt->having = parseExpression();
    }

    // UNION (INTERSECT/EXCEPT not yet in Firebird token set)
    if (matchKeyword(TokenType::KW_UNION)) {
        stmt->set_op = ast::SetOpType::UNION;
        if (matchKeyword(TokenType::KW_ALL)) {
            stmt->set_op_all = true;
        }
        stmt->set_op_right = static_cast<ast::SelectStmt*>(parseSelectStatement());
    }

    // ORDER BY clause
    if (matchKeyword(TokenType::KW_ORDER)) {
        consume(TokenType::KW_BY, "Expected BY after ORDER");
        stmt->order_by = parseOrderByClause();
    }

    // FOR UPDATE
    if (matchKeyword(TokenType::KW_FOR)) {
        if (matchKeyword(TokenType::KW_UPDATE)) {
            stmt->for_update = true;
            if (matchKeyword(TokenType::KW_WITH)) {
                consume(TokenType::KW_LOCK, "Expected LOCK after WITH");
            }
            // NOWAIT not yet in Firebird token set
        }
    }

    // ROWS clause (Firebird's alternative to FIRST/SKIP at end)
    if (matchKeyword(TokenType::KW_ROWS)) {
        stmt->limit = parseExpression();
        if (matchKeyword(TokenType::KW_TO)) {
            // ROWS m TO n means rows m through n
            stmt->offset = stmt->limit;
            stmt->limit = parseExpression();
        }
    }

    return stmt;
}

Statement* Parser::parseInsertStatement() {
    auto* stmt = allocate<ast::InsertStmt>();

    // INTO table
    consume(TokenType::KW_INTO, "Expected INTO after INSERT");
    stmt->table_path = parseSchemaPath();

    // Optional alias
    if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
        if (!check(TokenType::LEFT_PAREN) && !check(TokenType::KW_VALUES) &&
            !check(TokenType::KW_SELECT) && !check(TokenType::KW_DEFAULT)) {
            stmt->alias = parseIdentifier();
            stmt->has_alias = true;
        }
    }

    // Column list
    if (match(TokenType::LEFT_PAREN)) {
        do {
            stmt->columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ')' after column list");
    }

    // VALUES or SELECT or DEFAULT VALUES
    if (matchKeyword(TokenType::KW_VALUES)) {
        stmt->source = ast::InsertStmt::Source::VALUES;
        do {
            consume(TokenType::LEFT_PAREN, "Expected '(' before values");
            std::vector<ast::Expression*> row;
            do {
                if (matchKeyword(TokenType::KW_DEFAULT)) {
                    row.push_back(nullptr);  // nullptr = DEFAULT
                } else {
                    row.push_back(parseExpression());
                }
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected ')' after values");
            stmt->values_rows.push_back(row);
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_SELECT)) {
        stmt->source = ast::InsertStmt::Source::SELECT;
        // Go back and parse SELECT
        stmt->select_source = static_cast<ast::SelectStmt*>(parseSelectStatement());
    } else if (matchKeyword(TokenType::KW_DEFAULT)) {
        consume(TokenType::KW_VALUES, "Expected VALUES after DEFAULT");
        stmt->source = ast::InsertStmt::Source::DEFAULT;
    }

    // RETURNING clause (Firebird extension)
    if (matchKeyword(TokenType::KW_RETURNING)) {
        stmt->returning = parseReturningClause();
    }

    return stmt;
}

Statement* Parser::parseUpdateStatement() {
    auto* stmt = allocate<ast::UpdateStmt>();

    // Table reference
    stmt->table_path = parseSchemaPath();

    // Optional alias
    if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
        if (!check(TokenType::KW_SET)) {
            stmt->alias = parseIdentifier();
            stmt->has_alias = true;
        }
    }

    // SET clause
    consume(TokenType::KW_SET, "Expected SET after table name");
    do {
        ast::StringPool::StringId col = parseIdentifier();
        consume(TokenType::EQUAL, "Expected '=' in SET clause");
        ast::Expression* expr = parseExpression();
        stmt->set_items.push_back({col, expr});
    } while (match(TokenType::COMMA));

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        stmt->where = parseExpression();
    }

    // RETURNING clause (Firebird extension)
    if (matchKeyword(TokenType::KW_RETURNING)) {
        stmt->returning = parseReturningClause();
    }

    return stmt;
}

Statement* Parser::parseDeleteStatement() {
    auto* stmt = allocate<ast::DeleteStmt>();

    // FROM table
    consume(TokenType::KW_FROM, "Expected FROM after DELETE");
    stmt->table_path = parseSchemaPath();

    // Optional alias
    if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
        if (!check(TokenType::KW_WHERE) && !check(TokenType::KW_RETURNING)) {
            stmt->alias = parseIdentifier();
            stmt->has_alias = true;
        }
    }

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        stmt->where = parseExpression();
    }

    // RETURNING clause (Firebird extension)
    if (matchKeyword(TokenType::KW_RETURNING)) {
        stmt->returning = parseReturningClause();
    }

    return stmt;
}

Statement* Parser::parseMergeStatement() {
    auto* stmt = allocate<ast::MergeStmt>();

    auto matchIdentifierText = [&](const char* keyword) -> bool {
        if (!check(TokenType::IDENTIFIER)) {
            return false;
        }
        std::string_view text = lexer_.getTokenText(current_token_);
        size_t len = std::strlen(keyword);
        if (text.size() != len) {
            return false;
        }
        for (size_t i = 0; i < len; ++i) {
            char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
            char b = static_cast<char>(std::tolower(static_cast<unsigned char>(keyword[i])));
            if (a != b) {
                return false;
            }
        }
        advance();
        return true;
    };

    consume(TokenType::KW_INTO, "Expected INTO after MERGE");
    stmt->target_table = parseSchemaPath();

    if (matchKeyword(TokenType::KW_AS) ||
        (check(TokenType::IDENTIFIER) && !checkKeyword(TokenType::KW_USING))) {
        stmt->target_alias = parseIdentifier();
    }

    consume(TokenType::KW_USING, "Expected USING");
    if (match(TokenType::LEFT_PAREN)) {
        if (checkKeyword(TokenType::KW_SELECT)) {
            advance();
            stmt->source_query = parseSelectStatement();
        } else {
            error("Expected SELECT after '(' in USING");
        }
        consume(TokenType::RIGHT_PAREN, "Expected ')' after USING subquery");
    } else {
        stmt->source_table = parseSchemaPath();
    }

    if (matchKeyword(TokenType::KW_AS) ||
        (check(TokenType::IDENTIFIER) && !checkKeyword(TokenType::KW_ON))) {
        stmt->source_alias = parseIdentifier();
    }

    consume(TokenType::KW_ON, "Expected ON");
    stmt->on_condition = parseExpression();

    while (matchKeyword(TokenType::KW_WHEN)) {
        if (matchKeyword(TokenType::KW_MATCHED)) {
            ast::MergeStmt::WhenMatched when;

            if (matchKeyword(TokenType::KW_AND)) {
                when.and_condition = parseExpression();
            }

            consume(TokenType::KW_THEN, "Expected THEN");

            if (matchKeyword(TokenType::KW_UPDATE)) {
                consume(TokenType::KW_SET, "Expected SET");
                do {
                    auto col = parseIdentifier();
                    consume(TokenType::EQUAL, "Expected '=' in assignment");
                    auto* expr = parseExpression();
                    when.assignments.emplace_back(col, expr);
                } while (match(TokenType::COMMA));
            } else if (matchKeyword(TokenType::KW_DELETE)) {
                when.is_delete = true;
            } else {
                error("Expected UPDATE or DELETE after THEN");
                return stmt;
            }

            stmt->when_matched.push_back(std::move(when));
        } else if (matchKeyword(TokenType::KW_NOT)) {
            consume(TokenType::KW_MATCHED, "Expected MATCHED after NOT");

            if (matchKeyword(TokenType::KW_BY)) {
                consume(TokenType::KW_SOURCE, "Expected SOURCE after BY");
                ast::MergeStmt::WhenNotMatchedBySource when;
                if (matchKeyword(TokenType::KW_AND)) {
                    when.and_condition = parseExpression();
                }
                consume(TokenType::KW_THEN, "Expected THEN");
                if (matchKeyword(TokenType::KW_UPDATE)) {
                    consume(TokenType::KW_SET, "Expected SET");
                    do {
                        auto col = parseIdentifier();
                        consume(TokenType::EQUAL, "Expected '=' in assignment");
                        auto* expr = parseExpression();
                        when.assignments.emplace_back(col, expr);
                    } while (match(TokenType::COMMA));
                } else if (matchKeyword(TokenType::KW_DELETE)) {
                    when.is_delete = true;
                }
                stmt->when_not_matched_by_source.push_back(std::move(when));
            } else {
                matchKeyword(TokenType::KW_BY);
                matchIdentifierText("TARGET");

                ast::MergeStmt::WhenNotMatched when;
                if (matchKeyword(TokenType::KW_AND)) {
                    when.and_condition = parseExpression();
                }
                consume(TokenType::KW_THEN, "Expected THEN");
                consume(TokenType::KW_INSERT, "Expected INSERT");

                if (match(TokenType::LEFT_PAREN)) {
                    do {
                        when.columns.push_back(parseIdentifier());
                    } while (match(TokenType::COMMA));
                    consume(TokenType::RIGHT_PAREN, "Expected ')'");
                }

                consume(TokenType::KW_VALUES, "Expected VALUES");
                consume(TokenType::LEFT_PAREN, "Expected '('");
                do {
                    when.values.push_back(parseExpression());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected ')'");

                stmt->when_not_matched.push_back(std::move(when));
            }
        } else {
            error("Expected MATCHED or NOT MATCHED after WHEN");
            return stmt;
        }
    }

    return stmt;
}

Statement* Parser::parseUpdateOrInsertStatement() {
    // UPDATE OR INSERT INTO table (cols) VALUES (vals) MATCHING (cols)
    auto* stmt = allocate<ast::InsertStmt>();

    consume(TokenType::KW_INTO, "Expected INTO after UPDATE OR INSERT");
    stmt->table_path = parseSchemaPath();

    // Column list
    if (match(TokenType::LEFT_PAREN)) {
        do {
            stmt->columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ')' after column list");
    }

    // VALUES
    consume(TokenType::KW_VALUES, "Expected VALUES");
    stmt->source = ast::InsertStmt::Source::VALUES;
    consume(TokenType::LEFT_PAREN, "Expected '(' before values");
    std::vector<ast::Expression*> row;
    do {
        row.push_back(parseExpression());
    } while (match(TokenType::COMMA));
    consume(TokenType::RIGHT_PAREN, "Expected ')' after values");
    stmt->values_rows.push_back(row);

    // MATCHING clause (Firebird-specific)
    std::vector<ast::StringPool::StringId> matching_columns;
    if (matchKeyword(TokenType::KW_MATCHING)) {
        // Parse matching columns (used for determining update vs insert)
        consume(TokenType::LEFT_PAREN, "Expected '(' after MATCHING");
        do {
            matching_columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ')' after MATCHING columns");
    }

    if (stmt->columns.empty()) {
        error("UPDATE OR INSERT requires an explicit column list");
    } else if (!stmt->values_rows.empty()) {
        const auto& values = stmt->values_rows.front();
        if (values.size() != stmt->columns.size()) {
            error("UPDATE OR INSERT column count doesn't match VALUES count");
        } else {
            auto* on_conflict = allocate<ast::OnConflictClause>();
            on_conflict->action = ast::ConflictAction::UPDATE;
            on_conflict->columns = matching_columns;
            for (size_t i = 0; i < stmt->columns.size(); ++i) {
                on_conflict->set_items.push_back({stmt->columns[i], values[i]});
            }
            stmt->on_conflict = on_conflict;
        }
    }

    // RETURNING clause
    if (matchKeyword(TokenType::KW_RETURNING)) {
        stmt->returning = parseReturningClause();
    }

    return stmt;
}

Statement* Parser::parseExecuteProcedure() {
    auto* stmt = allocate<ast::ExecuteProcedureStmt>();

    stmt->procedure_path = parseSchemaPath();

    if (match(TokenType::LEFT_PAREN)) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                stmt->arguments.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected ')' after arguments");
    } else if (!checkKeyword(TokenType::KW_RETURNING_VALUES) &&
               !check(TokenType::SEMICOLON) && !atEnd()) {
        do {
            stmt->arguments.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    if (matchKeyword(TokenType::KW_RETURNING_VALUES)) {
        do {
            stmt->returning_variables.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_RETURNING)) {
        if (matchKeyword(TokenType::KW_VALUES)) {
            do {
                stmt->returning_variables.push_back(parseIdentifier());
            } while (match(TokenType::COMMA));
        }
    }

    return stmt;
}

// Transaction statements
Statement* Parser::parseSetTransaction() {
    auto* stmt = allocate<ast::SetStmt>();
    stmt->set_type = ast::SetStmt::SetType::TRANSACTION;

    consume(TokenType::KW_TRANSACTION, "Expected TRANSACTION after SET");

    auto matchIdentifierText = [&](const char* keyword) -> bool {
        if (!check(TokenType::IDENTIFIER)) {
            return false;
        }
        std::string_view text = lexer_.getTokenText(current_token_);
        size_t len = std::strlen(keyword);
        if (text.size() != len) {
            return false;
        }
        for (size_t i = 0; i < len; ++i) {
            char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
            char b = static_cast<char>(std::tolower(static_cast<unsigned char>(keyword[i])));
            if (a != b) {
                return false;
            }
        }
        advance();
        return true;
    };

    auto parseReadCommittedVariant = [&]() {
        if (stmt->has_read_committed_mode) {
            error("READ COMMITTED mode specified more than once");
            return;
        }
        if (matchKeyword(TokenType::KW_READ)) {
            consume(TokenType::KW_CONSISTENCY, "Expected CONSISTENCY after READ COMMITTED READ");
            stmt->has_read_committed_mode = true;
            stmt->read_committed_mode = ast::ReadCommittedMode::READ_CONSISTENCY;
        } else if (matchKeyword(TokenType::KW_RECORD_VERSION)) {
            stmt->has_read_committed_mode = true;
            stmt->read_committed_mode = ast::ReadCommittedMode::RECORD_VERSION;
        } else if (checkKeyword(TokenType::KW_NO)) {
            Token next = peek();
            if (next.type == TokenType::KW_RECORD_VERSION) {
                advance();
                consume(TokenType::KW_RECORD_VERSION, "Expected RECORD_VERSION after NO");
                stmt->has_read_committed_mode = true;
                stmt->read_committed_mode = ast::ReadCommittedMode::NO_RECORD_VERSION;
            }
        }
    };

    while (!atEnd() && current_token_.type != TokenType::SEMICOLON) {
        if (matchKeyword(TokenType::KW_READ)) {
            if (matchKeyword(TokenType::KW_ONLY)) {
                stmt->has_access_mode = true;
                stmt->access_mode = ast::TransactionAccess::READ_ONLY;
            } else if (matchKeyword(TokenType::KW_WRITE)) {
                stmt->has_access_mode = true;
                stmt->access_mode = ast::TransactionAccess::READ_WRITE;
            } else if (matchKeyword(TokenType::KW_COMMITTED)) {
                stmt->has_isolation_level = true;
                stmt->isolation_level = ast::IsolationLevel::READ_COMMITTED;
                parseReadCommittedVariant();
            } else {
                error("Expected ONLY, WRITE, or COMMITTED after READ");
                advance();
            }
        } else if (matchKeyword(TokenType::KW_SNAPSHOT)) {
            stmt->has_isolation_level = true;
            if (matchKeyword(TokenType::KW_TABLE)) {
                if (!matchIdentifierText("STABILITY")) {
                    error("Expected STABILITY after SNAPSHOT TABLE");
                }
                stmt->isolation_level = ast::IsolationLevel::SERIALIZABLE;
            } else {
                stmt->isolation_level = ast::IsolationLevel::REPEATABLE_READ;
            }
        } else if (matchKeyword(TokenType::KW_WAIT)) {
            stmt->has_wait_mode = true;
            stmt->wait_mode = ast::TransactionWaitMode::WAIT;
        } else if (matchKeyword(TokenType::KW_NO)) {
            if (matchKeyword(TokenType::KW_WAIT)) {
                stmt->has_wait_mode = true;
                stmt->wait_mode = ast::TransactionWaitMode::NO_WAIT;
            } else {
                error("Expected WAIT after NO");
                advance();
            }
        } else if (matchKeyword(TokenType::KW_LOCK)) {
            consume(TokenType::KW_TIMEOUT, "Expected TIMEOUT after LOCK");
            if (check(TokenType::INTEGER_LITERAL)) {
                int64_t value = current_token_.value.int_value;
                advance();
                if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
                    error("LOCK TIMEOUT out of range");
                } else {
                    stmt->has_lock_timeout = true;
                    stmt->lock_timeout_seconds = static_cast<uint32_t>(value);
                }
            } else {
                error("Expected integer literal after LOCK TIMEOUT");
                advance();
            }
        } else if (matchKeyword(TokenType::KW_RESERVING)) {
            do {
                ast::StringPool::StringId table_name = parseIdentifier();
                consume(TokenType::KW_FOR, "Expected FOR after RESERVING table name");

                ast::TableLockMode lock_mode = ast::TableLockMode::SHARED;
                if (matchKeyword(TokenType::KW_SHARED)) {
                    lock_mode = ast::TableLockMode::SHARED;
                } else if (matchKeyword(TokenType::KW_PROTECTED)) {
                    lock_mode = ast::TableLockMode::PROTECTED;
                } else {
                    error("Expected SHARED or PROTECTED in RESERVING clause");
                }

                bool for_write = false;
                if (matchKeyword(TokenType::KW_READ)) {
                    for_write = false;
                } else if (matchKeyword(TokenType::KW_WRITE)) {
                    for_write = true;
                } else {
                    error("Expected READ or WRITE in RESERVING clause");
                }

                stmt->table_reservations.emplace_back(table_name, lock_mode, for_write);
            } while (match(TokenType::COMMA));
        } else {
            error("Unexpected token in SET TRANSACTION");
            advance();
        }

        match(TokenType::COMMA);
    }

    return stmt;
}

Statement* Parser::parseCommit() {
    auto* stmt = allocate<ast::CommitStmt>();
    // Check for RETAIN (Firebird-specific, similar to AND CHAIN)
    if (matchKeyword(TokenType::KW_RETAIN)) {
        stmt->retaining = true;  // COMMIT RETAINING
        stmt->and_chain = true;  // Semantically equivalent to AND CHAIN
    }
    return stmt;
}

Statement* Parser::parseRollback() {
    auto* stmt = allocate<ast::RollbackStmt>();
    // Check for TO SAVEPOINT
    if (matchKeyword(TokenType::KW_TO)) {
        consume(TokenType::KW_SAVEPOINT, "Expected SAVEPOINT after TO");
        if (check(TokenType::IDENTIFIER)) {
            stmt->to_savepoint = true;
            stmt->savepoint_name = internFromLexer(current_token_.value.string_id);
            advance();
        }
    }
    // Check for RETAIN (Firebird-specific, similar to AND CHAIN)
    if (matchKeyword(TokenType::KW_RETAIN)) {
        stmt->retaining = true;  // ROLLBACK RETAINING
        stmt->and_chain = true;  // Semantically equivalent to AND CHAIN
    }
    return stmt;
}

Statement* Parser::parseSavepoint() {
    auto* stmt = allocate<ast::SavepointStmt>();
    if (check(TokenType::IDENTIFIER)) {
        stmt->name = internFromLexer(current_token_.value.string_id);
        advance();
    } else {
        error("Expected savepoint name");
    }
    return stmt;
}

Statement* Parser::parseReleaseSavepoint() {
    consume(TokenType::KW_SAVEPOINT, "Expected SAVEPOINT after RELEASE");
    auto* stmt = allocate<ast::ReleaseSavepointStmt>();
    if (check(TokenType::IDENTIFIER)) {
        stmt->name = internFromLexer(current_token_.value.string_id);
        advance();
    } else {
        error("Expected savepoint name");
    }
    return stmt;
}

// Session statements
Statement* Parser::parseSetStatement() {
    auto* stmt = allocate<ast::SetStmt>();
    stmt->set_type = ast::SetStmt::SetType::VARIABLE;

    auto matchIdentifierText = [&](const char* keyword) -> bool {
        if (!check(TokenType::IDENTIFIER)) {
            return false;
        }
        std::string_view text = lexer_.getTokenText(current_token_);
        size_t len = std::strlen(keyword);
        if (text.size() != len) {
            return false;
        }
        for (size_t i = 0; i < len; ++i) {
            char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
            char b = static_cast<char>(std::tolower(static_cast<unsigned char>(keyword[i])));
            if (a != b) {
                return false;
            }
        }
        advance();
        return true;
    };

    auto parseAutocommitMode = [&]() -> ast::AutocommitMode {
        if (matchKeyword(TokenType::KW_ON)) {
            return ast::AutocommitMode::ON;
        }
        if (matchIdentifierText("OFF")) {
            return ast::AutocommitMode::OFF;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            int64_t value = current_token_.value.int_value;
            advance();
            return value == 0 ? ast::AutocommitMode::OFF : ast::AutocommitMode::ON;
        }
        error("Expected AUTOCOMMIT mode (ON/OFF/1/0)");
        return ast::AutocommitMode::UNCHANGED;
    };

    if (matchKeyword(TokenType::KW_SQL)) {
        if (!matchIdentifierText("DIALECT")) {
            error("Expected DIALECT after SET SQL");
            return stmt;
        }
        stmt->set_type = ast::SetStmt::SetType::SQL_DIALECT;
        if (check(TokenType::INTEGER_LITERAL)) {
            stmt->sql_dialect = static_cast<uint8_t>(current_token_.value.int_value);
            advance();
        } else {
            error("Expected SQL dialect number");
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_NAMES)) {
        stmt->set_type = ast::SetStmt::SetType::NAMES;
        stmt->value = parseExpression();
        return stmt;
    }

    if (matchIdentifierText("LOCAL_TIMEOUT")) {
        stmt->set_type = ast::SetStmt::SetType::LOCAL_TIMEOUT;
        if (check(TokenType::INTEGER_LITERAL)) {
            stmt->local_timeout_seconds = static_cast<uint32_t>(current_token_.value.int_value);
            advance();
        } else {
            error("Expected integer after SET LOCAL_TIMEOUT");
        }
        return stmt;
    }

    if (matchIdentifierText("AUTOCOMMIT")) {
        stmt->set_type = ast::SetStmt::SetType::AUTOCOMMIT;
        stmt->has_autocommit = true;
        stmt->autocommit_mode = parseAutocommitMode();
        return stmt;
    }

    if (matchKeyword(TokenType::KW_ROLE)) {
        stmt->set_type = ast::SetStmt::SetType::ROLE;
        if (matchIdentifierText("NONE")) {
            stmt->is_default = true;
        } else {
            stmt->value = parseExpression();
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_SESSION)) {
        if (matchIdentifierText("AUTHORIZATION")) {
            stmt->set_type = ast::SetStmt::SetType::SESSION_AUTHORIZATION;
            if (matchIdentifierText("DEFAULT")) {
                stmt->is_default = true;
            } else {
                stmt->value = parseExpression();
            }
            return stmt;
        }
    }

    stmt->name = parseIdentifier();
    if (match(TokenType::EQUAL) || matchKeyword(TokenType::KW_TO)) {
        if (matchKeyword(TokenType::KW_DEFAULT)) {
            stmt->is_default = true;
        } else {
            stmt->value = parseExpression();
        }
    }

    return stmt;
}

Statement* Parser::parseShowStatement() {
    auto* stmt = allocate<ast::ShowStmt>();

    auto matchIdentifierText = [&](const char* keyword) -> bool {
        if (!check(TokenType::IDENTIFIER)) {
            return false;
        }
        std::string_view text = lexer_.getTokenText(current_token_);
        size_t len = std::strlen(keyword);
        if (text.size() != len) {
            return false;
        }
        for (size_t i = 0; i < len; ++i) {
            char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
            char b = static_cast<char>(std::tolower(static_cast<unsigned char>(keyword[i])));
            if (a != b) {
                return false;
            }
        }
        advance();
        return true;
    };

    if (matchKeyword(TokenType::KW_TABLE)) {
        stmt->show_type = ast::ShowStmt::ShowType::TABLE;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_INDEX)) {
        stmt->show_type = ast::ShowStmt::ShowType::INDEX;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_TRIGGER)) {
        stmt->show_type = ast::ShowStmt::ShowType::TRIGGER;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_VIEW)) {
        stmt->show_type = ast::ShowStmt::ShowType::VIEW;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_PROCEDURE)) {
        stmt->show_type = ast::ShowStmt::ShowType::PROCEDURE;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_FUNCTION)) {
        stmt->show_type = ast::ShowStmt::ShowType::FUNCTION;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_DOMAIN)) {
        stmt->show_type = ast::ShowStmt::ShowType::DOMAIN;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_GENERATOR) || matchKeyword(TokenType::KW_SEQUENCE)) {
        stmt->show_type = ast::ShowStmt::ShowType::GENERATOR;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_SCHEMA)) {
        stmt->show_type = ast::ShowStmt::ShowType::SCHEMA;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_ROLE)) {
        stmt->show_type = ast::ShowStmt::ShowType::ROLE;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchIdentifierText("GRANTS")) {
        stmt->show_type = ast::ShowStmt::ShowType::GRANTS;
        if (matchKeyword(TokenType::KW_FOR)) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchIdentifierText("CHECKS")) {
        stmt->show_type = ast::ShowStmt::ShowType::CHECKS;
        stmt->name = parseIdentifier();
        return stmt;
    }
    if (matchIdentifierText("COLLATIONS")) {
        stmt->show_type = ast::ShowStmt::ShowType::COLLATIONS;
        if (matchKeyword(TokenType::KW_LIKE)) {
            if (check(TokenType::STRING_LITERAL)) {
                stmt->like_pattern = internFromLexer(current_token_.value.string_id);
                advance();
            }
        }
        return stmt;
    }
    if (matchIdentifierText("COMMENTS") || matchIdentifierText("COMMENT")) {
        stmt->show_type = ast::ShowStmt::ShowType::COMMENTS;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchIdentifierText("DEPENDENCIES")) {
        stmt->show_type = ast::ShowStmt::ShowType::DEPENDENCIES;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_PACKAGE)) {
        stmt->show_type = ast::ShowStmt::ShowType::PACKAGE;
        if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            stmt->name = parseIdentifier();
        }
        return stmt;
    }
    if (matchKeyword(TokenType::KW_SQL)) {
        if (matchIdentifierText("DIALECT")) {
            stmt->show_type = ast::ShowStmt::ShowType::SQL_DIALECT;
            return stmt;
        }
    }
    if (matchIdentifierText("VERSION")) {
        stmt->show_type = ast::ShowStmt::ShowType::VERSION;
        return stmt;
    }
    if (matchKeyword(TokenType::KW_DATABASE)) {
        stmt->show_type = ast::ShowStmt::ShowType::DATABASE;
        return stmt;
    }
    if (matchKeyword(TokenType::KW_SYSTEM)) {
        stmt->show_type = ast::ShowStmt::ShowType::SYSTEM;
        return stmt;
    }
    if (matchIdentifierText("METRICS")) {
        stmt->show_type = ast::ShowStmt::ShowType::METRICS;
        return stmt;
    }

    stmt->show_type = ast::ShowStmt::ShowType::VARIABLE;
    if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
        stmt->name = parseIdentifier();
    } else {
        error("Expected SHOW option");
    }
    return stmt;
}

// DCL statements
Statement* Parser::parseGrantStatement() {
    auto* stmt = allocate<ast::GrantStmt>();

    do {
        if (matchKeyword(TokenType::KW_SELECT)) {
            stmt->privileges.push_back(ast::PrivilegeType::SELECT);
        } else if (matchKeyword(TokenType::KW_INSERT)) {
            stmt->privileges.push_back(ast::PrivilegeType::INSERT);
        } else if (matchKeyword(TokenType::KW_UPDATE)) {
            stmt->privileges.push_back(ast::PrivilegeType::UPDATE);
        } else if (matchKeyword(TokenType::KW_DELETE)) {
            stmt->privileges.push_back(ast::PrivilegeType::DELETE);
        } else if (matchIdentifierText("TRUNCATE")) {
            stmt->privileges.push_back(ast::PrivilegeType::TRUNCATE);
        } else if (matchKeyword(TokenType::KW_REFERENCES)) {
            stmt->privileges.push_back(ast::PrivilegeType::REFERENCES);
        } else if (matchKeyword(TokenType::KW_TRIGGER)) {
            stmt->privileges.push_back(ast::PrivilegeType::TRIGGER);
        } else if (matchKeyword(TokenType::KW_EXECUTE)) {
            stmt->privileges.push_back(ast::PrivilegeType::EXECUTE);
        } else if (matchIdentifierText("USAGE")) {
            stmt->privileges.push_back(ast::PrivilegeType::USAGE);
        } else if (matchIdentifierText("COPY")) {
            stmt->privileges.push_back(ast::PrivilegeType::COPY);
        } else if (matchKeyword(TokenType::KW_ALL)) {
            stmt->privileges.push_back(ast::PrivilegeType::ALL);
        } else {
            error("Expected privilege type");
            return nullptr;
        }
    } while (match(TokenType::COMMA));

    consume(TokenType::KW_ON, "Expected ON");
    if (matchKeyword(TokenType::KW_TABLE)) {
        stmt->object_type = ast::PrivilegeObjectType::TABLE;
    } else if (matchKeyword(TokenType::KW_VIEW)) {
        stmt->object_type = ast::PrivilegeObjectType::VIEW;
    } else if (matchKeyword(TokenType::KW_SEQUENCE) || matchKeyword(TokenType::KW_GENERATOR)) {
        stmt->object_type = ast::PrivilegeObjectType::SEQUENCE;
    } else if (matchKeyword(TokenType::KW_FUNCTION)) {
        stmt->object_type = ast::PrivilegeObjectType::FUNCTION;
    } else if (matchKeyword(TokenType::KW_PROCEDURE)) {
        stmt->object_type = ast::PrivilegeObjectType::PROCEDURE;
    } else if (matchKeyword(TokenType::KW_SCHEMA)) {
        stmt->object_type = ast::PrivilegeObjectType::SCHEMA;
    } else if (matchKeyword(TokenType::KW_DATABASE)) {
        stmt->object_type = ast::PrivilegeObjectType::DATABASE;
    }

    do {
        stmt->objects.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    consume(TokenType::KW_TO, "Expected TO");
    do {
        if (matchIdentifierText("PUBLIC")) {
            stmt->is_public = true;
        } else {
            stmt->grantees.push_back(parseIdentifier());
        }
    } while (match(TokenType::COMMA));

    if (matchKeyword(TokenType::KW_WITH)) {
        consume(TokenType::KW_GRANT, "Expected GRANT");
        if (!matchKeyword(TokenType::KW_OPTION)) {
            if (!check(TokenType::IDENTIFIER)) {
                error("Expected OPTION after WITH GRANT");
            } else {
                advance();
            }
        }
        stmt->with_grant_option = true;
    }

    return stmt;
}

Statement* Parser::parseRevokeStatement() {
    auto* stmt = allocate<ast::RevokeStmt>();

    if (matchKeyword(TokenType::KW_GRANT)) {
        if (!matchKeyword(TokenType::KW_OPTION)) {
            if (!check(TokenType::IDENTIFIER)) {
                error("Expected OPTION after GRANT");
            } else {
                advance();
            }
        }
        if (!matchKeyword(TokenType::KW_FOR)) {
            if (!check(TokenType::IDENTIFIER)) {
                error("Expected FOR after GRANT OPTION");
            } else {
                advance();
            }
        }
        stmt->grant_option_for = true;
    }

    do {
        if (matchKeyword(TokenType::KW_SELECT)) {
            stmt->privileges.push_back(ast::PrivilegeType::SELECT);
        } else if (matchKeyword(TokenType::KW_INSERT)) {
            stmt->privileges.push_back(ast::PrivilegeType::INSERT);
        } else if (matchKeyword(TokenType::KW_UPDATE)) {
            stmt->privileges.push_back(ast::PrivilegeType::UPDATE);
        } else if (matchKeyword(TokenType::KW_DELETE)) {
            stmt->privileges.push_back(ast::PrivilegeType::DELETE);
        } else if (matchIdentifierText("TRUNCATE")) {
            stmt->privileges.push_back(ast::PrivilegeType::TRUNCATE);
        } else if (matchKeyword(TokenType::KW_REFERENCES)) {
            stmt->privileges.push_back(ast::PrivilegeType::REFERENCES);
        } else if (matchKeyword(TokenType::KW_TRIGGER)) {
            stmt->privileges.push_back(ast::PrivilegeType::TRIGGER);
        } else if (matchKeyword(TokenType::KW_EXECUTE)) {
            stmt->privileges.push_back(ast::PrivilegeType::EXECUTE);
        } else if (matchIdentifierText("USAGE")) {
            stmt->privileges.push_back(ast::PrivilegeType::USAGE);
        } else if (matchIdentifierText("COPY")) {
            stmt->privileges.push_back(ast::PrivilegeType::COPY);
        } else if (matchKeyword(TokenType::KW_ALL)) {
            stmt->privileges.push_back(ast::PrivilegeType::ALL);
        } else {
            error("Expected privilege type");
            return nullptr;
        }
    } while (match(TokenType::COMMA));

    consume(TokenType::KW_ON, "Expected ON");
    if (matchKeyword(TokenType::KW_TABLE)) {
        stmt->object_type = ast::PrivilegeObjectType::TABLE;
    } else if (matchKeyword(TokenType::KW_VIEW)) {
        stmt->object_type = ast::PrivilegeObjectType::VIEW;
    } else if (matchKeyword(TokenType::KW_SEQUENCE) || matchKeyword(TokenType::KW_GENERATOR)) {
        stmt->object_type = ast::PrivilegeObjectType::SEQUENCE;
    } else if (matchKeyword(TokenType::KW_FUNCTION)) {
        stmt->object_type = ast::PrivilegeObjectType::FUNCTION;
    } else if (matchKeyword(TokenType::KW_PROCEDURE)) {
        stmt->object_type = ast::PrivilegeObjectType::PROCEDURE;
    } else if (matchKeyword(TokenType::KW_SCHEMA)) {
        stmt->object_type = ast::PrivilegeObjectType::SCHEMA;
    } else if (matchKeyword(TokenType::KW_DATABASE)) {
        stmt->object_type = ast::PrivilegeObjectType::DATABASE;
    }

    do {
        stmt->objects.push_back(parseSchemaPath());
    } while (match(TokenType::COMMA));

    consume(TokenType::KW_FROM, "Expected FROM");
    do {
        if (matchIdentifierText("PUBLIC")) {
            stmt->is_public = true;
        } else {
            stmt->grantees.push_back(parseIdentifier());
        }
    } while (match(TokenType::COMMA));

    if (matchKeyword(TokenType::KW_CASCADE)) {
        stmt->cascade = true;
    } else {
        matchKeyword(TokenType::KW_RESTRICT);
    }

    return stmt;
}

// Metadata statements
Statement* Parser::parseCommentStatement() {
    auto* stmt = allocate<ast::CommentStmt>();

    consume(TokenType::KW_ON, "Expected ON after COMMENT");

    if (matchKeyword(TokenType::KW_TABLE)) {
        stmt->object_type = ast::CommentObjectType::TABLE;
        stmt->object_path = parseSchemaPath();
    } else if (matchKeyword(TokenType::KW_COLUMN)) {
        stmt->object_type = ast::CommentObjectType::COLUMN;
        ast::SchemaPath path;
        if (check(TokenType::IDENTIFIER)) {
            path.components.push_back(parseIdentifier());
            if (match(TokenType::DOT)) {
                stmt->column_name = parseIdentifier();
            }
        }
        stmt->object_path = path;
    } else if (matchKeyword(TokenType::KW_INDEX)) {
        stmt->object_type = ast::CommentObjectType::INDEX;
        stmt->object_path = parseSchemaPath();
    } else if (matchKeyword(TokenType::KW_VIEW)) {
        stmt->object_type = ast::CommentObjectType::VIEW;
        stmt->object_path = parseSchemaPath();
    } else if (matchKeyword(TokenType::KW_SEQUENCE) || matchKeyword(TokenType::KW_GENERATOR)) {
        stmt->object_type = ast::CommentObjectType::SEQUENCE;
        stmt->object_path = parseSchemaPath();
    } else if (matchKeyword(TokenType::KW_FUNCTION)) {
        stmt->object_type = ast::CommentObjectType::FUNCTION;
        stmt->object_path = parseSchemaPath();
    } else if (matchKeyword(TokenType::KW_PROCEDURE)) {
        stmt->object_type = ast::CommentObjectType::PROCEDURE;
        stmt->object_path = parseSchemaPath();
    } else if (matchKeyword(TokenType::KW_TRIGGER)) {
        stmt->object_type = ast::CommentObjectType::TRIGGER;
        stmt->object_path = parseSchemaPath();
    } else if (matchKeyword(TokenType::KW_SCHEMA)) {
        stmt->object_type = ast::CommentObjectType::SCHEMA;
        stmt->object_path = parseSchemaPath();
    } else if (matchKeyword(TokenType::KW_DATABASE)) {
        stmt->object_type = ast::CommentObjectType::DATABASE;
        stmt->object_path = parseSchemaPath();
    } else if (matchKeyword(TokenType::KW_ROLE)) {
        stmt->object_type = ast::CommentObjectType::ROLE;
        stmt->object_path = parseSchemaPath();
    } else if (matchKeyword(TokenType::KW_CONSTRAINT)) {
        stmt->object_type = ast::CommentObjectType::CONSTRAINT;
        stmt->object_path = parseSchemaPath();
    } else {
        error("Expected object type after COMMENT ON");
        return nullptr;
    }

    consume(TokenType::KW_IS, "Expected IS after COMMENT ON");
    if (matchKeyword(TokenType::KW_NULL)) {
        stmt->is_null = true;
    } else if (check(TokenType::STRING_LITERAL) || check(TokenType::Q_STRING_LITERAL)) {
        stmt->comment_text = internFromLexer(current_token_.value.string_id);
        advance();
    } else {
        error("Expected string literal or NULL after IS");
    }

    return stmt;
}

// =============================================================================
// PSQL Statements
// =============================================================================

// Parse a single PSQL statement (used inside BEGIN...END blocks)
Statement* Parser::parsePSQLBlock() {
    // Check what kind of PSQL statement we have
    if (checkKeyword(TokenType::KW_IF)) {
        advance();
        return parseIfStatement();
    }
    if (checkKeyword(TokenType::KW_WHILE)) {
        advance();
        return parseWhileStatement();
    }
    if (checkKeyword(TokenType::KW_FOR)) {
        advance();
        return parseForStatement();
    }
    if (checkKeyword(TokenType::KW_LOOP)) {
        advance();
        return parseLoopStatement();
    }
    // Note: LOOP keyword not in Firebird token set - use WHILE instead
    if (checkKeyword(TokenType::KW_LEAVE)) {
        advance();
        return parseLeaveStatement();
    }
    if (checkKeyword(TokenType::KW_CONTINUE)) {
        advance();
        return parseContinueStatement();
    }
    if (checkKeyword(TokenType::KW_EXIT)) {
        advance();
        return parseExitStatement();
    }
    if (checkKeyword(TokenType::KW_SUSPEND)) {
        advance();
        return parseSuspendStatement();
    }
    if (checkKeyword(TokenType::KW_RETURN)) {
        advance();
        return parseReturnStatement();
    }
    if (checkKeyword(TokenType::KW_EXCEPTION)) {
        advance();
        return parseExceptionStatement();
    }
    if (checkKeyword(TokenType::KW_POST_EVENT)) {
        advance();
        return parsePostEventStatement();
    }
    if (checkKeyword(TokenType::KW_OPEN)) {
        advance();
        return parseOpenCursor();
    }
    if (checkKeyword(TokenType::KW_FETCH)) {
        advance();
        return parseFetchCursor();
    }
    if (checkKeyword(TokenType::KW_CLOSE)) {
        advance();
        return parseCloseCursor();
    }
    if (checkKeyword(TokenType::KW_BEGIN)) {
        advance();
        return parseBeginEndBlock();
    }
    if (checkKeyword(TokenType::KW_EXECUTE)) {
        advance();
        return parseExecuteStatement();
    }

    // Assignment statement: variable := expression
    if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
        // Look ahead for :=
        Token saved = current_token_;
        ast::StringPool::StringId varName = parseIdentifier();

        if (check(TokenType::COLON_EQUALS)) {
            advance();  // consume :=
            auto* stmt = allocate<ast::AssignmentStmt>();
            stmt->variable = varName;
            stmt->value = parseExpression();
            match(TokenType::SEMICOLON);  // Optional semicolon
            return stmt;
        }

        // Not an assignment - must be a DML statement (SELECT, INSERT, etc.)
        // We've already consumed the identifier, need to handle this differently
        // For now, report error
        error("Expected := for assignment or statement keyword");
        return nullptr;
    }

    // Try DML statements
    if (checkKeyword(TokenType::KW_SELECT)) {
        advance();
        return parseSelectStatement();
    }
    if (checkKeyword(TokenType::KW_INSERT)) {
        advance();
        return parseInsertStatement();
    }
    if (checkKeyword(TokenType::KW_UPDATE)) {
        advance();
        return parseUpdateStatement();
    }
    if (checkKeyword(TokenType::KW_DELETE)) {
        advance();
        return parseDeleteStatement();
    }

    error("Expected PSQL statement");
    return nullptr;
}

// Parse BEGIN...END block
Statement* Parser::parseBeginEndBlock() {
    auto* stmt = allocate<ast::CompoundStmt>();

    // Parse statements until END
    while (!checkKeyword(TokenType::KW_END) && !atEnd()) {
        // Check for WHEN (exception handler at end of block)
        if (checkKeyword(TokenType::KW_WHEN)) {
            break;
        }

        Statement* innerStmt = parsePSQLBlock();
        if (innerStmt) {
            stmt->statements.push_back(innerStmt);
        }

        // Optional semicolon between statements
        match(TokenType::SEMICOLON);
    }

    // Parse exception handlers (WHEN...DO)
    while (checkKeyword(TokenType::KW_WHEN)) {
        advance();
        Statement* handler = parseWhenStatement();
        if (handler) {
            stmt->exception_handlers.push_back(handler);
        }
    }

    consume(TokenType::KW_END, "Expected END");
    return stmt;
}

// Parse DECLARE VARIABLE statement
Statement* Parser::parseDeclareVariable() {
    // DECLARE VARIABLE name type [= default]
    auto* stmt = allocate<ast::DeclareVariableStmt>();

    consume(TokenType::KW_VARIABLE, "Expected VARIABLE after DECLARE");
    stmt->name = parseIdentifier();
    stmt->type = parseTypeName();

    // Optional NOT NULL
    if (matchKeyword(TokenType::KW_NOT)) {
        consume(TokenType::KW_NULL, "Expected NULL after NOT");
        stmt->not_null = true;
    }

    // Optional default value
    if (match(TokenType::EQUAL) || matchKeyword(TokenType::KW_DEFAULT)) {
        stmt->default_value = parseExpression();
    }

    return stmt;
}

// Parse DECLARE section (multiple variable declarations)
Statement* Parser::parseDeclareSection() {
    // This returns the first declaration; caller should loop
    return parseDeclareVariable();
}

// Parse IF statement
Statement* Parser::parseIfStatement() {
    // IF condition THEN statement [ELSE statement]
    auto* stmt = allocate<ast::IfStmt>();

    consume(TokenType::LEFT_PAREN, "Expected '(' after IF");
    stmt->condition = parseExpression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after condition");

    consume(TokenType::KW_THEN, "Expected THEN after IF condition");

    // Parse THEN branch
    if (checkKeyword(TokenType::KW_BEGIN)) {
        advance();
        stmt->then_branch = parseBeginEndBlock();
    } else {
        stmt->then_branch = parsePSQLBlock();
    }

    // Optional ELSE
    if (matchKeyword(TokenType::KW_ELSE)) {
        if (checkKeyword(TokenType::KW_IF)) {
            advance();
            stmt->else_branch = parseIfStatement();  // ELSE IF
        } else if (checkKeyword(TokenType::KW_BEGIN)) {
            advance();
            stmt->else_branch = parseBeginEndBlock();
        } else {
            stmt->else_branch = parsePSQLBlock();
        }
    }

    return stmt;
}

// Parse WHILE statement
Statement* Parser::parseWhileStatement() {
    // WHILE condition DO statement
    auto* stmt = allocate<ast::WhileStmt>();

    consume(TokenType::LEFT_PAREN, "Expected '(' after WHILE");
    stmt->condition = parseExpression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after condition");

    consume(TokenType::KW_DO, "Expected DO after WHILE condition");

    if (checkKeyword(TokenType::KW_BEGIN)) {
        advance();
        stmt->body = parseBeginEndBlock();
    } else {
        stmt->body = parsePSQLBlock();
    }

    return stmt;
}

// Parse FOR statement (FOR SELECT...INTO...DO or FOR variable = start TO end DO)
Statement* Parser::parseForStatement() {
    // FOR SELECT ... INTO vars DO statement
    if (checkKeyword(TokenType::KW_SELECT)) {
        advance();
        auto* stmt = allocate<ast::ForSelectStmt>();
        stmt->select_stmt = parseSelectStatement();

        // INTO variable list
        consume(TokenType::KW_INTO, "Expected INTO after FOR SELECT");
        do {
            stmt->into_variables.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));

        consume(TokenType::KW_DO, "Expected DO after INTO variables");

        if (checkKeyword(TokenType::KW_BEGIN)) {
            advance();
            stmt->body = parseBeginEndBlock();
        } else {
            stmt->body = parsePSQLBlock();
        }

        return stmt;
    }

    // FOR EXECUTE STATEMENT ... INTO ... DO
    if (checkKeyword(TokenType::KW_EXECUTE)) {
        advance();
        auto* stmt = allocate<ast::ForExecuteStmt>();

        consume(TokenType::KW_STATEMENT, "Expected STATEMENT after EXECUTE");
        stmt->sql = parseExpression();

        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    stmt->parameters.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters");
        }

        if (matchKeyword(TokenType::KW_INTO)) {
            do {
                stmt->into_variables.push_back(parseIdentifier());
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::KW_DO, "Expected DO");
        if (checkKeyword(TokenType::KW_BEGIN)) {
            advance();
            stmt->body = parseBeginEndBlock();
        } else {
            stmt->body = parsePSQLBlock();
        }

        return stmt;
    }

    error("Expected SELECT or EXECUTE after FOR");
    return nullptr;
}

// Parse LOOP statement
Statement* Parser::parseLoopStatement() {
    auto* loop = allocate<ast::LoopStmt>();
    auto* body = allocate<ast::CompoundStmt>();

    while (!atEnd()) {
        if (checkKeyword(TokenType::KW_END)) {
            Token next = peek();
            if (next.type == TokenType::KW_LOOP) {
                advance();
                consume(TokenType::KW_LOOP, "Expected LOOP after END");
                break;
            }
        }

        Statement* inner = parsePSQLBlock();
        if (inner) {
            body->statements.push_back(inner);
        }
        match(TokenType::SEMICOLON);
    }

    loop->body = body;
    return loop;
}

// Parse LEAVE statement (break)
Statement* Parser::parseLeaveStatement() {
    auto* stmt = allocate<ast::LeaveStmt>();

    // Optional label
    if (check(TokenType::IDENTIFIER)) {
        stmt->label = parseIdentifier();
    }

    return stmt;
}

// Parse CONTINUE statement
Statement* Parser::parseContinueStatement() {
    auto* stmt = allocate<ast::ContinueStmt>();

    // Optional label
    if (check(TokenType::IDENTIFIER)) {
        stmt->label = parseIdentifier();
    }

    return stmt;
}

// Parse EXIT statement
Statement* Parser::parseExitStatement() {
    return allocate<ast::ExitStmt>();
}

// Parse SUSPEND statement
Statement* Parser::parseSuspendStatement() {
    return allocate<ast::SuspendStmt>();
}

// Parse RETURN statement
Statement* Parser::parseReturnStatement() {
    auto* stmt = allocate<ast::ReturnStmt>();

    // Optional return value
    if (!check(TokenType::SEMICOLON) && !checkKeyword(TokenType::KW_END)) {
        stmt->value = parseExpression();
    }

    return stmt;
}

// Parse WHEN exception handler
Statement* Parser::parseWhenStatement() {
    // WHEN ANY/SQLCODE/GDSCODE/EXCEPTION DO statement
    auto* stmt = allocate<ast::WhenExceptionStmt>();

    if (matchKeyword(TokenType::KW_ANY)) {
        stmt->type = ast::WhenExceptionStmt::ExceptionType::ANY;
    } else if (matchKeyword(TokenType::KW_SQLCODE)) {
        stmt->type = ast::WhenExceptionStmt::ExceptionType::SQLCODE;
        if (check(TokenType::INTEGER_LITERAL)) {
            stmt->sqlcode = static_cast<int32_t>(current_token_.value.int_value);
            advance();
        }
    } else if (matchKeyword(TokenType::KW_GDSCODE)) {
        stmt->type = ast::WhenExceptionStmt::ExceptionType::GDSCODE;
        stmt->gdscode = parseIdentifier();
    } else if (matchKeyword(TokenType::KW_EXCEPTION)) {
        stmt->type = ast::WhenExceptionStmt::ExceptionType::EXCEPTION;
        stmt->exception_name = parseIdentifier();
    } else {
        error("Expected ANY, SQLCODE, GDSCODE, or EXCEPTION after WHEN");
        return nullptr;
    }

    consume(TokenType::KW_DO, "Expected DO after WHEN clause");

    if (checkKeyword(TokenType::KW_BEGIN)) {
        advance();
        stmt->handler = parseBeginEndBlock();
    } else {
        stmt->handler = parsePSQLBlock();
    }

    // Optional terminator after handler statement
    match(TokenType::SEMICOLON);

    return stmt;
}

// Parse EXCEPTION statement (raise exception)
Statement* Parser::parseExceptionStatement() {
    auto* stmt = allocate<ast::ExceptionRaiseStmt>();

    stmt->exception_name = parseIdentifier();

    // Optional custom message
    if (check(TokenType::STRING_LITERAL)) {
        stmt->message = parsePrimaryExpression();
    }

    return stmt;
}

// Parse POST_EVENT statement
Statement* Parser::parsePostEventStatement() {
    auto* stmt = allocate<ast::PostEventStmt>();
    stmt->event_name = parseExpression();
    return stmt;
}

// Parse DECLARE CURSOR statement
Statement* Parser::parseDeclareCursor() {
    // DECLARE cursor_name [SCROLL] CURSOR FOR select_statement
    auto* stmt = allocate<ast::DeclareCursorStmt>();

    stmt->cursor_name = parseIdentifier();

    if (matchKeyword(TokenType::KW_SCROLL)) {
        stmt->scroll = true;
    }

    consume(TokenType::KW_CURSOR, "Expected CURSOR");
    consume(TokenType::KW_FOR, "Expected FOR after CURSOR");

    if (checkKeyword(TokenType::KW_SELECT)) {
        advance();
        stmt->select_stmt = parseSelectStatement();
    } else {
        error("Expected SELECT after FOR");
    }

    return stmt;
}

// Parse OPEN cursor statement
Statement* Parser::parseOpenCursor() {
    auto* stmt = allocate<ast::OpenCursorStmt>();
    stmt->cursor_name = parseIdentifier();
    return stmt;
}

// Parse FETCH cursor statement
Statement* Parser::parseFetchCursor() {
    auto* stmt = allocate<ast::FetchCursorStmt>();

    // Optional direction
    if (matchKeyword(TokenType::KW_NEXT)) {
        stmt->direction = ast::FetchCursorStmt::Direction::NEXT;
    } else if (matchKeyword(TokenType::KW_PRIOR)) {
        stmt->direction = ast::FetchCursorStmt::Direction::PRIOR;
    } else if (matchKeyword(TokenType::KW_FIRST)) {
        stmt->direction = ast::FetchCursorStmt::Direction::FIRST;
    } else if (matchKeyword(TokenType::KW_LAST)) {
        stmt->direction = ast::FetchCursorStmt::Direction::LAST;
    } else if (matchKeyword(TokenType::KW_ABSOLUTE)) {
        stmt->direction = ast::FetchCursorStmt::Direction::ABSOLUTE;
        stmt->offset = parseExpression();
    } else if (matchKeyword(TokenType::KW_RELATIVE)) {
        stmt->direction = ast::FetchCursorStmt::Direction::RELATIVE;
        stmt->offset = parseExpression();
    }

    // Cursor name
    stmt->cursor_name = parseIdentifier();

    // INTO variables
    if (matchKeyword(TokenType::KW_INTO)) {
        do {
            stmt->into_variables.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
    }

    return stmt;
}

// Parse CLOSE cursor statement
Statement* Parser::parseCloseCursor() {
    auto* stmt = allocate<ast::CloseCursorStmt>();
    stmt->cursor_name = parseIdentifier();
    return stmt;
}

// Parse EXECUTE statement (EXECUTE STATEMENT or EXECUTE PROCEDURE)
Statement* Parser::parseExecuteStatement() {
    if (matchKeyword(TokenType::KW_BLOCK)) {
        return parseExecuteBlock();
    }

    if (matchKeyword(TokenType::KW_PROCEDURE)) {
        return parseExecuteProcedure();
    }

    if (matchKeyword(TokenType::KW_STATEMENT)) {
        auto* stmt = allocate<ast::ExecuteStatementStmt>();
        stmt->sql = parseExpression();

        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    stmt->parameters.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters");
        }

        if (matchKeyword(TokenType::KW_INTO)) {
            do {
                stmt->into_variables.push_back(parseIdentifier());
            } while (match(TokenType::COMMA));
        }

        return stmt;
    }

    error("Expected BLOCK, PROCEDURE, or STATEMENT after EXECUTE");
    return nullptr;
}

// Parse EXECUTE BLOCK statement
Statement* Parser::parseExecuteBlock() {
    auto* stmt = allocate<ast::ExecuteBlockStmt>();

    // Optional input parameters
    if (match(TokenType::LEFT_PAREN)) {
        // Input parameters: (param = value, ...)
        do {
            ast::VariableDecl param;
            param.name = parseIdentifier();
            consume(TokenType::EQUAL, "Expected '=' in parameter");
            param.default_value = parseExpression();
            stmt->input_params.push_back(param);
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ')' after input parameters");
    }

    // Optional RETURNS clause
    if (matchKeyword(TokenType::KW_RETURNS)) {
        consume(TokenType::LEFT_PAREN, "Expected '(' after RETURNS");
        do {
            ast::VariableDecl param;
            param.name = parseIdentifier();
            param.type = parseTypeName();
            stmt->output_params.push_back(param);
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ')' after output parameters");
    }

    // AS clause
    consume(TokenType::KW_AS, "Expected AS before block body");

    // Optional variable declarations
    while (checkKeyword(TokenType::KW_DECLARE)) {
        advance();
        ast::VariableDecl var;
        consume(TokenType::KW_VARIABLE, "Expected VARIABLE after DECLARE");
        var.name = parseIdentifier();
        var.type = parseTypeName();

        if (matchKeyword(TokenType::KW_NOT)) {
            consume(TokenType::KW_NULL, "Expected NULL after NOT");
            var.not_null = true;
        }

        if (match(TokenType::EQUAL) || matchKeyword(TokenType::KW_DEFAULT)) {
            var.default_value = parseExpression();
        }

        stmt->variables.push_back(var);
        match(TokenType::SEMICOLON);
    }

    // BEGIN...END block
    consume(TokenType::KW_BEGIN, "Expected BEGIN");
    stmt->body = parseBeginEndBlock();

    return stmt;
}

// Clause parsing stubs
std::vector<ast::SelectItem*> Parser::parseSelectList() {
    std::vector<ast::SelectItem*> items;
    do {
        items.push_back(parseSelectItem());
    } while (match(TokenType::COMMA));
    return items;
}

ast::SelectItem* Parser::parseSelectItem() {
    auto* item = allocate<ast::SelectItem>();

    // Check for *
    if (match(TokenType::STAR)) {
        item->item_type = ast::SelectItem::Type::STAR;
        return item;
    }

    // Check for table.* or just expression
    // Parse expression first
    item->expr = parseExpression();
    item->item_type = ast::SelectItem::Type::EXPRESSION;

    // Check for alias
    if (matchKeyword(TokenType::KW_AS)) {
        item->alias = parseIdentifier();
        item->has_alias = true;
    } else if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
        // Implicit alias (no AS keyword)
        // But not ASC/DESC which are ORDER BY modifiers
        if (!check(TokenType::COMMA) && !check(TokenType::KW_FROM) &&
            !check(TokenType::KW_WHERE) && !check(TokenType::KW_ORDER) &&
            !check(TokenType::KW_GROUP) && !check(TokenType::KW_HAVING) &&
            !check(TokenType::KW_ASC) && !check(TokenType::KW_ASCENDING) &&
            !check(TokenType::KW_DESC) && !check(TokenType::KW_DESCENDING) &&
            !check(TokenType::KW_NULLS) &&
            !check(TokenType::SEMICOLON) && !check(TokenType::RIGHT_PAREN)) {
            item->alias = parseIdentifier();
            item->has_alias = true;
        }
    }

    return item;
}

ast::TableRefNode* Parser::parseFromClause() {
    auto* ref = allocate<ast::TableRefNode>();

    // Subquery: (SELECT ...)
    if (match(TokenType::LEFT_PAREN)) {
        if (check(TokenType::KW_SELECT)) {
            advance();  // consume SELECT
            ref->ref_type = ast::TableRefNode::Type::SUBQUERY;
            ref->subquery = parseSelectStatement();
            consume(TokenType::RIGHT_PAREN, "Expected ')' after subquery");

            // Alias is required for subquery
            if (matchKeyword(TokenType::KW_AS)) {
                ref->alias = parseIdentifier();
                ref->has_alias = true;
            } else if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
                ref->alias = parseIdentifier();
                ref->has_alias = true;
            }
        } else {
            // Parenthesized table reference
            auto* inner = parseFromClause();
            consume(TokenType::RIGHT_PAREN, "Expected ')' after table reference");
            return inner;
        }
    } else {
        // Simple table reference
        ref->ref_type = ast::TableRefNode::Type::TABLE;
        ref->table_path = parseSchemaPath();

        // Optional alias
        if (matchKeyword(TokenType::KW_AS)) {
            ref->alias = parseIdentifier();
            ref->has_alias = true;
        } else if (check(TokenType::IDENTIFIER) || isNonReservedKeyword()) {
            // Check it's not a keyword that starts a new clause
            if (!check(TokenType::KW_WHERE) && !check(TokenType::KW_ORDER) &&
                !check(TokenType::KW_GROUP) && !check(TokenType::KW_HAVING) &&
                !check(TokenType::KW_INNER) && !check(TokenType::KW_LEFT) &&
                !check(TokenType::KW_RIGHT) && !check(TokenType::KW_FULL) &&
                !check(TokenType::KW_CROSS) && !check(TokenType::KW_JOIN) &&
                !check(TokenType::KW_ON) && !check(TokenType::KW_UNION)) {
                ref->alias = parseIdentifier();
                ref->has_alias = true;
            }
        }
    }

    return ref;
}

ast::JoinNode* Parser::parseJoinClause() {
    auto* join = allocate<ast::JoinNode>();

    // Parse join type
    bool natural = false;
    if (matchKeyword(TokenType::KW_NATURAL)) {
        natural = true;
    }

    if (matchKeyword(TokenType::KW_INNER)) {
        join->join_type = ast::JoinType::INNER;
    } else if (matchKeyword(TokenType::KW_LEFT)) {
        matchKeyword(TokenType::KW_OUTER);  // Optional OUTER
        join->join_type = natural ? ast::JoinType::NATURAL_LEFT : ast::JoinType::LEFT;
    } else if (matchKeyword(TokenType::KW_RIGHT)) {
        matchKeyword(TokenType::KW_OUTER);  // Optional OUTER
        join->join_type = natural ? ast::JoinType::NATURAL_RIGHT : ast::JoinType::RIGHT;
    } else if (matchKeyword(TokenType::KW_FULL)) {
        matchKeyword(TokenType::KW_OUTER);  // Optional OUTER
        join->join_type = natural ? ast::JoinType::NATURAL_FULL : ast::JoinType::FULL;
    } else if (matchKeyword(TokenType::KW_CROSS)) {
        join->join_type = ast::JoinType::CROSS;
    } else if (natural) {
        join->join_type = ast::JoinType::NATURAL;
    }

    consume(TokenType::KW_JOIN, "Expected JOIN");

    // Right side table
    join->right = parseFromClause();

    // Join condition (not for CROSS or NATURAL joins)
    if (join->join_type != ast::JoinType::CROSS &&
        join->join_type != ast::JoinType::NATURAL &&
        join->join_type != ast::JoinType::NATURAL_LEFT &&
        join->join_type != ast::JoinType::NATURAL_RIGHT &&
        join->join_type != ast::JoinType::NATURAL_FULL) {

        if (matchKeyword(TokenType::KW_ON)) {
            join->on_condition = parseExpression();
        } else if (matchKeyword(TokenType::KW_USING)) {
            join->has_using = true;
            consume(TokenType::LEFT_PAREN, "Expected '(' after USING");
            do {
                join->using_columns.push_back(parseIdentifier());
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected ')' after USING columns");
        }
    }

    return join;
}

Expression* Parser::parseWhereClause() {
    return parseExpression();
}

std::vector<Expression*> Parser::parseGroupByClause() {
    std::vector<Expression*> exprs;
    do {
        exprs.push_back(parseExpression());
    } while (match(TokenType::COMMA));
    return exprs;
}

Expression* Parser::parseHavingClause() {
    return parseExpression();
}

std::vector<ast::OrderByItem*> Parser::parseOrderByClause() {
    std::vector<ast::OrderByItem*> items;
    do {
        auto* item = allocate<ast::OrderByItem>();
        item->expr = parseExpression();

        // ASC/DESC
        if (matchKeyword(TokenType::KW_ASC) || matchKeyword(TokenType::KW_ASCENDING)) {
            item->ascending = true;
        } else if (matchKeyword(TokenType::KW_DESC) || matchKeyword(TokenType::KW_DESCENDING)) {
            item->ascending = false;
        }

        // NULLS FIRST/LAST
        if (matchKeyword(TokenType::KW_NULLS)) {
            if (matchKeyword(TokenType::KW_FIRST)) {
                item->nulls_first = true;
            } else if (matchKeyword(TokenType::KW_LAST)) {
                item->nulls_last = true;
            }
        }

        items.push_back(item);
    } while (match(TokenType::COMMA));
    return items;
}

Parser::FirstSkip Parser::parseFirstSkip() {
    FirstSkip result;

    // FIRST n
    if (matchKeyword(TokenType::KW_FIRST)) {
        result.first = parseExpression();
    }

    // SKIP n
    if (matchKeyword(TokenType::KW_SKIP)) {
        result.skip = parseExpression();
    }

    return result;
}

std::vector<ast::SelectItem*> Parser::parseReturningClause() {
    std::vector<ast::SelectItem*> items;
    do {
        items.push_back(parseSelectItem());
    } while (match(TokenType::COMMA));
    return items;
}
ast::ColumnDef* Parser::parseColumnDef() {
    auto* col = allocate<ast::ColumnDef>();

    // Column name
    col->name = parseIdentifier();
    if (col->name == ast::StringPool::INVALID_ID) {
        return nullptr;
    }

    // Type
    col->type = parseTypeName();

    // Parse column constraints
    col->constraints = parseColumnConstraints();

    return col;
}

std::vector<ast::ColumnConstraint> Parser::parseColumnConstraints() {
    std::vector<ast::ColumnConstraint> constraints;

    while (!atEnd() && !check(TokenType::COMMA) && !check(TokenType::RIGHT_PAREN)) {
        ast::ColumnConstraint constraint;

        // Optional constraint name: CONSTRAINT name
        if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            constraint.name = parseIdentifier();
        }

        if (matchKeyword(TokenType::KW_NOT)) {
            consume(TokenType::KW_NULL, "Expected NULL after NOT");
            constraint.type = ast::ConstraintType::NOT_NULL;
            constraints.push_back(constraint);
        } else if (matchKeyword(TokenType::KW_NULL)) {
            constraint.type = ast::ConstraintType::NULL_ALLOWED;
            constraints.push_back(constraint);
        } else if (matchKeyword(TokenType::KW_PRIMARY)) {
            consume(TokenType::KW_KEY, "Expected KEY after PRIMARY");
            constraint.type = ast::ConstraintType::PRIMARY_KEY;
            constraints.push_back(constraint);
        } else if (matchKeyword(TokenType::KW_UNIQUE)) {
            constraint.type = ast::ConstraintType::UNIQUE;
            constraints.push_back(constraint);
        } else if (matchKeyword(TokenType::KW_REFERENCES)) {
            constraint.type = ast::ConstraintType::REFERENCES;
            constraint.ref_table = parseSchemaPath();
            if (match(TokenType::LEFT_PAREN)) {
                do {
                    constraint.ref_columns.push_back(parseIdentifier());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected ')' after column list");
            }
            // ON DELETE/UPDATE actions
            while (matchKeyword(TokenType::KW_ON)) {
                if (matchKeyword(TokenType::KW_DELETE)) {
                    if (matchKeyword(TokenType::KW_CASCADE)) {
                        constraint.on_delete = ast::ForeignKeyAction::CASCADE;
                    } else if (matchKeyword(TokenType::KW_SET)) {
                        if (matchKeyword(TokenType::KW_NULL)) {
                            constraint.on_delete = ast::ForeignKeyAction::SET_NULL;
                        } else if (matchKeyword(TokenType::KW_DEFAULT)) {
                            constraint.on_delete = ast::ForeignKeyAction::SET_DEFAULT;
                        }
                    } else if (matchKeyword(TokenType::KW_NO)) {
                        consume(TokenType::KW_ACTION, "Expected ACTION after NO");
                        constraint.on_delete = ast::ForeignKeyAction::NO_ACTION;
                    }
                } else if (matchKeyword(TokenType::KW_UPDATE)) {
                    if (matchKeyword(TokenType::KW_CASCADE)) {
                        constraint.on_update = ast::ForeignKeyAction::CASCADE;
                    } else if (matchKeyword(TokenType::KW_SET)) {
                        if (matchKeyword(TokenType::KW_NULL)) {
                            constraint.on_update = ast::ForeignKeyAction::SET_NULL;
                        } else if (matchKeyword(TokenType::KW_DEFAULT)) {
                            constraint.on_update = ast::ForeignKeyAction::SET_DEFAULT;
                        }
                    } else if (matchKeyword(TokenType::KW_NO)) {
                        consume(TokenType::KW_ACTION, "Expected ACTION after NO");
                        constraint.on_update = ast::ForeignKeyAction::NO_ACTION;
                    }
                }
            }
            constraints.push_back(constraint);
        } else if (matchKeyword(TokenType::KW_CHECK)) {
            constraint.type = ast::ConstraintType::CHECK;
            consume(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
            constraint.check_expr = parseExpression();
            consume(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
            constraints.push_back(constraint);
        } else if (matchKeyword(TokenType::KW_DEFAULT)) {
            constraint.type = ast::ConstraintType::DEFAULT;
            constraint.default_expr = parseExpression();
            constraints.push_back(constraint);
        } else if (matchKeyword(TokenType::KW_GENERATED)) {
            // GENERATED ALWAYS AS IDENTITY / GENERATED BY DEFAULT AS IDENTITY
            constraint.type = ast::ConstraintType::GENERATED;
            matchKeyword(TokenType::KW_ALWAYS) || matchKeyword(TokenType::KW_BY);
            if (check(TokenType::KW_DEFAULT)) advance();
            consume(TokenType::KW_AS, "Expected AS after GENERATED");
            if (matchKeyword(TokenType::KW_IDENTITY)) {
                // Identity column
            }
            constraints.push_back(constraint);
        } else if (matchKeyword(TokenType::KW_COMPUTED)) {
            // COMPUTED BY (expr) - Firebird computed column
            if (matchKeyword(TokenType::KW_BY)) {
                consume(TokenType::LEFT_PAREN, "Expected '(' after COMPUTED BY");
                constraint.type = ast::ConstraintType::GENERATED;
                constraint.default_expr = parseExpression();
                consume(TokenType::RIGHT_PAREN, "Expected ')' after computed expression");
                constraints.push_back(constraint);
            }
        } else {
            // No more constraints
            break;
        }
    }

    return constraints;
}

ast::TableConstraint* Parser::parseTableConstraint() {
    auto* constraint = allocate<ast::TableConstraint>();

    // Optional constraint name
    if (matchKeyword(TokenType::KW_CONSTRAINT)) {
        constraint->name = parseIdentifier();
    }

    if (matchKeyword(TokenType::KW_PRIMARY)) {
        consume(TokenType::KW_KEY, "Expected KEY after PRIMARY");
        constraint->type = ast::TableConstraintType::PRIMARY_KEY;
        consume(TokenType::LEFT_PAREN, "Expected '(' after PRIMARY KEY");
        do {
            constraint->columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ')' after column list");
    } else if (matchKeyword(TokenType::KW_FOREIGN)) {
        consume(TokenType::KW_KEY, "Expected KEY after FOREIGN");
        constraint->type = ast::TableConstraintType::FOREIGN_KEY;
        consume(TokenType::LEFT_PAREN, "Expected '(' after FOREIGN KEY");
        do {
            constraint->columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ')' after column list");
        consume(TokenType::KW_REFERENCES, "Expected REFERENCES after column list");
        constraint->ref_table = parseSchemaPath();
        if (match(TokenType::LEFT_PAREN)) {
            do {
                constraint->ref_columns.push_back(parseIdentifier());
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected ')' after referenced columns");
        }
    } else if (matchKeyword(TokenType::KW_UNIQUE)) {
        constraint->type = ast::TableConstraintType::UNIQUE;
        consume(TokenType::LEFT_PAREN, "Expected '(' after UNIQUE");
        do {
            constraint->columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ')' after column list");
    } else if (matchKeyword(TokenType::KW_CHECK)) {
        constraint->type = ast::TableConstraintType::CHECK;
        consume(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
        constraint->check_expr = parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
    }

    return constraint;
}

} // namespace scratchbird::parser::firebird
