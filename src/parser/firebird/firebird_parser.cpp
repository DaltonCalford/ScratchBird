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

std::string_view Parser::currentText() {
    return lexer_.getTokenText(current_token_);
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

} // anonymous namespace

// =============================================================================
// Top-Level Parsing
// =============================================================================

ParseResult Parser::parseStatement() {
    ParseResult result;

    try {
        Statement* stmt = parseStatementInternal();
        if (stmt) {
            result.statement.reset(stmt);
            result.success = true;
        }
    } catch (const std::exception& e) {
        error(e.what());
    }

    // Consume optional semicolon
    match(TokenType::SEMICOLON);

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

    // Note: Firebird doesn't have SHOW - it uses ISQL-specific commands
    // Session commands are typically SET-based in Firebird

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

    // IN
    if (matchKeyword(TokenType::KW_IN)) {
        return parseInExpression(left);
    }

    // BETWEEN
    if (matchKeyword(TokenType::KW_BETWEEN)) {
        return parseBetweenExpression(left);
    }

    // LIKE / SIMILAR TO / CONTAINING / STARTING
    if (matchKeyword(TokenType::KW_LIKE) ||
        matchKeyword(TokenType::KW_CONTAINING) ||
        matchKeyword(TokenType::KW_STARTING)) {
        return parseLikeExpression(left);
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

    // CASE expression
    if (matchKeyword(TokenType::KW_CASE)) {
        return parseCaseExpression();
    }

    // CAST expression
    if (matchKeyword(TokenType::KW_CAST)) {
        return parseCastExpression();
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
        // TODO: Parse window specification
    }

    return expr;
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
    consume(TokenType::RIGHT_PAREN, "Expected ')' after CAST");

    return expr;
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

Expression* Parser::parseLikeExpression(Expression* left) {
    auto* expr = allocate<ast::LikeExpr>();
    expr->expr = left;

    // The keyword was already matched
    // TODO: Track which variant (LIKE, CONTAINING, STARTING, SIMILAR TO)

    expr->pattern = parseAddExpression();

    // Check for ESCAPE clause
    if (matchKeyword(TokenType::KW_ESCAPE)) {
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

    do {
        if (check(TokenType::IDENTIFIER)) {
            path.components.push_back(internFromLexer(current_token_.value.string_id));
            advance();
        } else {
            error("Expected identifier in schema path");
            break;
        }
    } while (match(TokenType::DOT));

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
        return parseCreateProcedure();
    }
    if (matchKeyword(TokenType::KW_FUNCTION)) {
        return parseCreateFunction();
    }
    if (matchKeyword(TokenType::KW_TRIGGER)) {
        return parseCreateTrigger();
    }
    if (matchKeyword(TokenType::KW_DOMAIN)) {
        return parseCreateDomain();
    }
    if (matchKeyword(TokenType::KW_EXCEPTION)) {
        return parseCreateException();
    }
    if (matchKeyword(TokenType::KW_ROLE)) {
        return parseCreateRole();
    }
    if (matchKeyword(TokenType::KW_PACKAGE)) {
        return parseCreatePackage();
    }

    error("Unknown CREATE object type");
    return nullptr;
}

Statement* Parser::parseAlterStatement() {
    if (matchKeyword(TokenType::KW_TABLE)) {
        return parseAlterTableImpl();
    }
    if (matchKeyword(TokenType::KW_DOMAIN)) {
        return parseAlterDomainImpl();
    }
    if (matchKeyword(TokenType::KW_INDEX)) {
        return parseAlterIndexImpl();
    }
    error("ALTER statement for this object type not yet implemented");
    return nullptr;
}

Statement* Parser::parseDropStatement() {
    // Handle IF EXISTS
    bool if_exists = false;

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
    if (matchKeyword(TokenType::KW_GENERATOR) || matchKeyword(TokenType::KW_SEQUENCE)) {
        return parseDropSequenceImpl(if_exists);
    }

    error("DROP statement for this object type not yet implemented");
    return nullptr;
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

    error("RECREATE statement for this object type not yet implemented");
    return nullptr;
}

// Helper implementation for CREATE TABLE
ast::CreateTableStmt* Parser::parseCreateTableImpl(bool or_replace, bool temporary, bool global_temp) {
    auto* stmt = allocate<ast::CreateTableStmt>();
    stmt->or_replace = or_replace;
    stmt->temporary = temporary;

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
    if (temporary && matchKeyword(TokenType::KW_ON)) {
        consume(TokenType::KW_COMMIT, "Expected COMMIT after ON");
        if (matchKeyword(TokenType::KW_DELETE)) {
            // ON COMMIT DELETE ROWS (default for Firebird)
        } else if (matchKeyword(TokenType::KW_PRESERVE)) {
            // ON COMMIT PRESERVE ROWS
        }
        matchKeyword(TokenType::KW_ROWS);  // Optional ROWS keyword
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
            stmt->action = ast::AlterTableAction::ALTER_COLUMN;
            // Parse column alteration (TYPE, SET DEFAULT, DROP DEFAULT, etc.)
        }
    } else if (matchKeyword(TokenType::KW_RENAME)) {
        error("ALTER TABLE RENAME is not supported in Firebird parser");
        return nullptr;
    } else if (matchKeyword(TokenType::KW_SET)) {
        error("ALTER TABLE SET is not supported in Firebird parser");
        return nullptr;
    }

    return stmt;
}

Statement* Parser::parseAlterDomainImpl() {
    ast::SchemaPath domain_path = parseSchemaPath();

    if (matchKeyword(TokenType::KW_TO)) {
        auto* stmt = allocate<ast::RenameObjectStmt>();
        stmt->object_type = ast::DdlObjectType::DOMAIN;
        stmt->if_exists = false;
        stmt->object_path = domain_path;
        stmt->new_name = parseIdentifier();
        return stmt;
    }

    error("ALTER DOMAIN supports only TO <new_name> in Firebird parser");
    return nullptr;
}

// Stub for ALTER INDEX
Statement* Parser::parseAlterIndexImpl() {
    error("ALTER INDEX is not supported in Firebird parser");
    return nullptr;
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

Statement* Parser::parseDropSequenceImpl(bool if_exists) {
    // No DropSequenceStmt in AST yet, use error for now
    error("DROP SEQUENCE/GENERATOR not yet implemented");
    return nullptr;
}

// Wrappers that delegate to impl methods
Statement* Parser::parseCreateTable() { return parseCreateTableImpl(false, false, false); }
Statement* Parser::parseCreateOrAlterTable() { return parseCreateTableImpl(true, false, false); }
Statement* Parser::parseCreateIndex() { return parseCreateIndexImpl(false, false); }
Statement* Parser::parseCreateView() { return parseCreateViewImpl(false); }
Statement* Parser::parseCreateSequence() { return parseCreateSequenceImpl(); }
Statement* Parser::parseCreateProcedure() {
    error("CREATE PROCEDURE not yet implemented");
    return nullptr;
}
Statement* Parser::parseCreateFunction() {
    error("CREATE FUNCTION not yet implemented");
    return nullptr;
}
Statement* Parser::parseCreateTrigger() {
    error("CREATE TRIGGER not yet implemented");
    return nullptr;
}
Statement* Parser::parseCreateDomain() {
    error("CREATE DOMAIN not yet implemented");
    return nullptr;
}
Statement* Parser::parseCreateException() {
    error("CREATE EXCEPTION not yet implemented");
    return nullptr;
}
Statement* Parser::parseCreateRole() {
    error("CREATE ROLE not yet implemented");
    return nullptr;
}
Statement* Parser::parseCreatePackage() {
    error("CREATE PACKAGE not yet implemented");
    return nullptr;
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
    // MERGE INTO target USING source ON condition
    // WHEN MATCHED THEN UPDATE SET ...
    // WHEN NOT MATCHED THEN INSERT ...
    error("MERGE statement not yet implemented");
    return nullptr;
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
    if (matchKeyword(TokenType::KW_MATCHING)) {
        // Parse matching columns (used for determining update vs insert)
        consume(TokenType::LEFT_PAREN, "Expected '(' after MATCHING");
        do {
            parseIdentifier();  // Just consume for now
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ')' after MATCHING columns");
    }

    // RETURNING clause
    if (matchKeyword(TokenType::KW_RETURNING)) {
        stmt->returning = parseReturningClause();
    }

    return stmt;
}

Statement* Parser::parseExecuteProcedure() {
    // EXECUTE PROCEDURE name(args) [RETURNING_VALUES vars]
    error("EXECUTE PROCEDURE statement not yet implemented");
    return nullptr;
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
    error("SET statement not yet implemented");
    return nullptr;
}

Statement* Parser::parseShowStatement() {
    error("SHOW statement not yet implemented");
    return nullptr;
}

// DCL statements
Statement* Parser::parseGrantStatement() {
    error("GRANT statement not yet implemented");
    return nullptr;
}

Statement* Parser::parseRevokeStatement() {
    error("REVOKE statement not yet implemented");
    return nullptr;
}

// Metadata statements
Statement* Parser::parseCommentStatement() {
    error("COMMENT statement not yet implemented");
    return nullptr;
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
        error("FOR EXECUTE STATEMENT not yet implemented");
        return nullptr;
    }

    error("Expected SELECT or EXECUTE after FOR");
    return nullptr;
}

// Parse LOOP statement
Statement* Parser::parseLoopStatement() {
    // LOOP statement END LOOP
    error("LOOP statement not yet implemented");
    return nullptr;
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
        // EXECUTE STATEMENT 'dynamic sql'
        error("EXECUTE STATEMENT not yet implemented");
        return nullptr;
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
