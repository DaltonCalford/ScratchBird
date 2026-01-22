/**
 * ScratchBird Parser v2.0 - Main Parser Implementation
 *
 * See: include/scratchbird/parser/parser_v2.h
 */

#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/core/catalog_manager.h"
#include <cctype>
#include <cstring>
#include <algorithm>
#include <limits>

namespace scratchbird::parser::v2 {

// =============================================================================
// Constructor / Destructor
// =============================================================================

Parser::Parser(std::string_view input)
    : lexer_(input)
    , state_(lexer_)
    , arena_()
{
}

Parser::~Parser() = default;

// =============================================================================
// Error Handling
// =============================================================================

void Parser::error(const std::string& message) {
    errorAt(SourceSpan(currentLocation(), 1), message);
}

void Parser::error(const std::string& message, const std::string& hint) {
    errorAt(SourceSpan(currentLocation(), 1), message, hint);
}

void Parser::errorAt(SourceSpan span, const std::string& message, const std::string& hint) {
    errors_.push_back({span, message, hint});
}

void Parser::synchronize() {
    advance();

    while (!isAtEnd()) {
        // Synchronize at statement boundaries
        if (previous().type == TokenType::SEMICOLON) {
            return;
        }

        // Or at statement starters
        switch (current().type) {
            case TokenType::KW_SELECT:
            case TokenType::KW_INSERT:
            case TokenType::KW_UPDATE:
            case TokenType::KW_DELETE:
            case TokenType::KW_CREATE:
            case TokenType::KW_ALTER:
            case TokenType::KW_DROP:
            case TokenType::KW_TRUNCATE:
            case TokenType::KW_GRANT:
            case TokenType::KW_REVOKE:
            case TokenType::KW_BEGIN:
            case TokenType::KW_PREPARE:
            case TokenType::KW_COMMIT:
            case TokenType::KW_ROLLBACK:
                return;
            default:
                break;
        }

        advance();
    }
}

// =============================================================================
// Utility Methods
// =============================================================================

bool Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) {
        advance();
        return true;
    }
    error(message);
    return false;
}

bool Parser::expectContextual(const char* keyword, const std::string& message) {
    if (matchContextual(keyword)) {
        return true;
    }
    error(message);
    return false;
}

StringPool::StringId Parser::currentIdentifier() {
    if (!isIdentifier()) {
        return StringPool::INVALID_ID;
    }
    StringPool::StringId id = state_.currentStringId();
    advance();
    return id;
}

StringPool::StringId Parser::expectIdentifier(const std::string& message) {
    if (!isIdentifier()) {
        error(message);
        return StringPool::INVALID_ID;
    }
    return currentIdentifier();
}

SourceSpan Parser::makeSpan(SourceLocation start) const {
    return SourceSpan(start,
        static_cast<uint32_t>(currentLocation().offset - start.offset));
}

// =============================================================================
// Main Parse Entry Points
// =============================================================================

ParseResult Parser::parseStatement() {
    ParseResult result;

    Statement* stmt = parseStatementInternal();
    result.setStatement(stmt);

    // Copy errors
    for (const auto& err : errors_) {
        result.addError(err);
    }

    return result;
}

std::vector<ParseResult> Parser::parseStatements() {
    std::vector<ParseResult> results;

    while (!isAtEnd()) {
        errors_.clear();
        ParseResult result = parseStatement();
        results.push_back(std::move(result));

        // Skip optional semicolon
        match(TokenType::SEMICOLON);
    }

    return results;
}

// =============================================================================
// Statement Dispatch
// =============================================================================

Statement* Parser::parseStatementInternal() {
    ParseModeGuard guard(state_, ParseMode::STATEMENT);

    // Gatekeeper dispatch
    if (check(TokenType::KW_WITH))      return parseWithStatement();
    if (match(TokenType::KW_CREATE))    return parseCreate();
    if (match(TokenType::KW_ALTER))     return parseAlter();
    if (match(TokenType::KW_DROP))      return parseDrop();
    if (match(TokenType::KW_TRUNCATE))  return parseTruncate();

    // DML statements
    if (match(TokenType::KW_SELECT))    return parseSelect();
    if (match(TokenType::KW_INSERT))    return parseInsert();
    if (match(TokenType::KW_UPDATE))    return parseUpdate();
    if (match(TokenType::KW_DELETE))    return parseDelete();
    if (match(TokenType::KW_COPY))      return parseCopy();

    // Transaction statements
    if (match(TokenType::KW_BEGIN))     return parseStartTransaction();
    if (match(TokenType::KW_START))     return parseStartTransaction();
    if (match(TokenType::KW_PREPARE))   return parsePrepareTransaction();
    if (match(TokenType::KW_COMMIT))    return parseCommit();
    if (match(TokenType::KW_ROLLBACK))  return parseRollback();
    if (matchContextual("SAVEPOINT"))   return parseSavepoint();
    if (matchContextual("RELEASE"))     return parseReleaseSavepoint();

    // Session statements
    if (match(TokenType::KW_SET))       return parseSet();
    if (match(TokenType::KW_SHOW))      return parseShow();
    if (matchContextual("RESET"))       return parseReset();

    // Utility statements
    if (match(TokenType::KW_EXPLAIN))   return parseExplain();
    if (match(TokenType::KW_EXECUTE))   return parseExecuteStatement();

    // DCL statements
    if (match(TokenType::KW_GRANT))     return parseGrant();
    if (match(TokenType::KW_REVOKE))    return parseRevoke();

    // Connection statements
    if (matchContextual("CONNECT"))     return parseConnect();
    if (matchContextual("DISCONNECT"))  return parseDisconnect();

    // Metadata statements
    if (matchContextual("COMMENT"))     return parseComment();

    // MERGE statement
    if (matchContextual("MERGE"))       return parseMerge();

    error("Expected SQL statement");
    return nullptr;
}

// =============================================================================
// CREATE Statement Dispatch
// =============================================================================

Statement* Parser::parseCreate() {
    ParseModeGuard guard(state_, ParseMode::DDL);

    // Check for OR REPLACE
    bool or_replace = false;
    if (checkContextual("OR")) {
        matchContextual("OR");
        expectContextual("REPLACE", "Expected REPLACE after OR");
        or_replace = true;
    }

    // Check for UNIQUE (for CREATE UNIQUE INDEX)
    bool unique = false;
    if (checkContextual("UNIQUE")) {
        matchContextual("UNIQUE");
        unique = true;
    }

    // Check for TEMPORARY/TEMP (including GLOBAL TEMPORARY for Firebird-style GTT)
    bool temporary = false;
    TempTableType temp_type = TempTableType::NONE;
    if (checkContextual("GLOBAL")) {
        matchContextual("GLOBAL");
        if (checkContextual("TEMPORARY") || checkContextual("TEMP")) {
            matchContextual("TEMPORARY") || matchContextual("TEMP");
            temporary = true;
            temp_type = TempTableType::GLOBAL;
        } else {
            error("Expected TEMPORARY after GLOBAL");
        }
    } else if (checkContextual("TEMPORARY") || checkContextual("TEMP")) {
        matchContextual("TEMPORARY") || matchContextual("TEMP");
        temporary = true;
        temp_type = TempTableType::SESSION;
    }

    // Check for UNLOGGED
    bool unlogged = false;
    if (checkContextual("UNLOGGED")) {
        matchContextual("UNLOGGED");
        unlogged = true;
    }

    // Check for MATERIALIZED (for CREATE MATERIALIZED VIEW)
    bool materialized = false;
    if (checkContextual("MATERIALIZED")) {
        matchContextual("MATERIALIZED");
        materialized = true;
    }

    // Dispatch based on object type
    if (matchContextual("SCHEMA")) {
        return parseCreateSchema();
    }

    if (matchContextual("DATABASE")) {
        return parseCreateDatabase();
    }

    if (matchContextual("DOMAIN")) {
        return parseCreateDomain();
    }

    if (matchContextual("TABLE")) {
        auto* stmt = parseCreateTable(or_replace, temp_type);
        if (stmt) {
            stmt->temp_type = temp_type;
            stmt->unlogged = unlogged;
        }
        return stmt;
    }

    if (matchContextual("INDEX")) {
        auto* stmt = parseCreateIndex();
        if (stmt) {
            stmt->unique = unique;
        }
        return stmt;
    }

    if (matchContextual("VIEW")) {
        auto* stmt = parseCreateView(or_replace);
        if (stmt) {
            stmt->temporary = temporary;
            stmt->materialized = materialized;
        }
        return stmt;
    }

    if (matchContextual("SEQUENCE")) {
        auto* stmt = parseCreateSequence();
        if (stmt) {
            stmt->or_replace = or_replace;
            stmt->temporary = temporary;
        }
        return stmt;
    }

    if (matchContextual("FUNCTION"))   return parseCreateFunction(or_replace);
    if (matchContextual("PROCEDURE"))  return parseCreateProcedure(or_replace);
    if (matchContextual("TRIGGER"))    return parseCreateTrigger(or_replace);
    if (matchContextual("USER"))       return parseCreateUser();
    if (matchContextual("ROLE"))       return parseCreateRole();

    error("Expected object type after CREATE (TABLE, INDEX, VIEW, SEQUENCE, ROLE, USER, ...)");
    return nullptr;
}

// =============================================================================
// CREATE USER / ROLE
// =============================================================================

CreateUserStmt* Parser::parseCreateUser() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateUserStmt>();
    stmt->user_name = expectIdentifier("Expected user name");

    if (match(TokenType::KW_WITH) || matchContextual("WITH")) {
        // Optional WITH before options
    }

    while (true) {
        if (matchContextual("PASSWORD")) {
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal for PASSWORD");
                break;
            }
            stmt->has_password = true;
            stmt->password = current().value.string_id;
            advance();
        } else if (matchContextual("SUPERUSER")) {
            stmt->is_superuser = true;
        } else if (matchContextual("NOSUPERUSER")) {
            stmt->is_superuser = false;
        } else if (match(TokenType::KW_WITH) || matchContextual("WITH")) {
            continue;
        } else {
            break;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CreateRoleStmt* Parser::parseCreateRole() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateRoleStmt>();
    stmt->role_name = expectIdentifier("Expected role name");
    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE TABLE
// =============================================================================

CreateTableStmt* Parser::parseCreateTable(bool or_replace, TempTableType temp_type) {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateTableStmt>();
    stmt->or_replace = or_replace;
    stmt->temp_type = temp_type;

    // Check for IF NOT EXISTS (IF, NOT, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    // Parse table name (schema path)
    stmt->table_path = parseSchemaPath(state_);
    if (stmt->table_path.isEmpty()) {
        error("Expected table name");
        return stmt;
    }

    // Parse column definitions and constraints
    if (!expect(TokenType::LEFT_PAREN, "Expected '(' after table name")) {
        return stmt;
    }

    ParseModeGuard colGuard(state_, ParseMode::COLUMN_DEF);

    // Parse comma-separated list of columns and constraints
    bool first = true;
    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (!first) {
            if (!expect(TokenType::COMMA, "Expected ',' between column definitions")) {
                break;
            }
        }
        first = false;

        // Check if this is a table constraint
        if (checkContextual("CONSTRAINT") ||
            checkContextual("PRIMARY") ||
            checkContextual("UNIQUE") ||
            checkContextual("FOREIGN") ||
            checkContextual("CHECK")) {
            TableConstraint* constraint = parseTableConstraint();
            if (constraint) {
                stmt->constraints.push_back(constraint);
            }
        } else {
            // Column definition
            ColumnDef* col = parseColumnDef();
            if (col) {
                stmt->columns.push_back(col);
            }
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after column definitions");

    // Parse optional table options
    bool has_on_commit = false;
    while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        if (check(TokenType::KW_ON) || checkContextual("ON")) {
            if (has_on_commit) {
                error("ON COMMIT specified more than once");
            }
            if (!(match(TokenType::KW_ON) || matchContextual("ON"))) {
                error("Expected ON");
            }
            if (!(match(TokenType::KW_COMMIT) || matchContextual("COMMIT"))) {
                error("Expected COMMIT after ON");
            }
            if (match(TokenType::KW_DELETE) || matchContextual("DELETE")) {
                stmt->on_commit = TempOnCommitAction::DELETE_ROWS;
                matchContextual("ROWS");
            } else if (matchContextual("PRESERVE")) {
                stmt->on_commit = TempOnCommitAction::PRESERVE_ROWS;
                matchContextual("ROWS");
            } else if (match(TokenType::KW_DROP) || matchContextual("DROP")) {
                stmt->on_commit = TempOnCommitAction::DROP;
            } else {
                error("Expected DELETE, PRESERVE, or DROP after ON COMMIT");
            }
            has_on_commit = true;
        } else if (checkContextual("TABLESPACE")) {
            matchContextual("TABLESPACE");
            stmt->tablespace = parseSchemaPath(state_);
            stmt->has_tablespace = true;
        } else if (checkContextual("INHERITS")) {
            matchContextual("INHERITS");
            expect(TokenType::LEFT_PAREN, "Expected '(' after INHERITS");
            // Parse parent tables
            do {
                stmt->inherits.push_back(parseSchemaPath(state_));
            } while (match(TokenType::COMMA));
            expect(TokenType::RIGHT_PAREN, "Expected ')' after parent table list");
        } else if (checkContextual("PARTITION")) {
            matchContextual("PARTITION");
            expectContextual("BY", "Expected BY after PARTITION");
            stmt->is_partitioned = true;
            // Parse partition type
            if (matchContextual("RANGE") || matchContextual("LIST") || matchContextual("HASH")) {
                stmt->partition_by = previous().value.string_id;
            }
            expect(TokenType::LEFT_PAREN, "Expected '(' after partition type");
            // Parse partition columns
            do {
                stmt->partition_columns.push_back(expectIdentifier("Expected partition column name"));
            } while (match(TokenType::COMMA));
            expect(TokenType::RIGHT_PAREN, "Expected ')' after partition columns");
        } else {
            break;  // Unknown option, stop parsing
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// Column Definition
// =============================================================================

ColumnDef* Parser::parseColumnDef() {
    SourceLocation start = currentLocation();

    auto* col = arena_.create<ColumnDef>();

    // Column name
    col->name = expectIdentifier("Expected column name");
    if (col->name == StringPool::INVALID_ID) {
        return col;
    }

    // Data type
    col->type = parseTypeName();

    // Column constraints
    col->constraints = parseColumnConstraints();

    col->span = makeSpan(start);
    return col;
}

// =============================================================================
// Type Name Parsing
// =============================================================================

TypeName Parser::parseTypeName() {
    ParseModeGuard guard(state_, ParseMode::TYPE_NAME);
    SourceLocation start = currentLocation();

    TypeName type;

    if (state_.check(TokenType::DOT) || state_.check(TokenType::DOUBLE_DOT) ||
        state_.check(TokenType::EXCLAIM_COLON)) {
        type.schema_path = parseSchemaPath(state_);
        type.has_schema_path = true;
        type.span = makeSpan(start);
        return type;
    }

    // Type name (contextual keyword)
    if (!isIdentifier()) {
        error("Expected data type name");
        return type;
    }

    Token next = state_.lexer().peekToken();
    if (next.type == TokenType::DOT) {
        type.schema_path = parseSchemaPath(state_);
        type.has_schema_path = true;
        type.span = makeSpan(start);
        return type;
    }

    type.name = currentIdentifier();

    // Handle two-word type names
    std::string_view type_text = stringPool().get(type.name);
    auto to_upper = [](std::string_view s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return result;
    };
    std::string upper_type = to_upper(type_text);

    // DOUBLE PRECISION -> FLOAT64/DOUBLE
    if (upper_type == "DOUBLE") {
        if (matchContextual("PRECISION")) {
            // "DOUBLE PRECISION" - combine into a single type name
            type.name = stringPool().intern("DOUBLE PRECISION");
        }
    }
    // CHARACTER VARYING -> VARCHAR (alternative syntax)
    else if (upper_type == "CHARACTER") {
        if (matchContextual("VARYING")) {
            type.name = stringPool().intern("VARCHAR");
        }
    }
    // BIT VARYING -> VARBIT
    else if (upper_type == "BIT") {
        if (matchContextual("VARYING")) {
            type.name = stringPool().intern("VARBIT");
        }
    }

    // Check for type parameters: (precision, scale) or (length)
    if (match(TokenType::LEFT_PAREN)) {
        // First parameter
        if (check(TokenType::INTEGER_LITERAL)) {
            type.length = static_cast<int32_t>(current().value.int_value);
            type.precision = type.length;
            advance();

            // Optional second parameter (scale)
            if (match(TokenType::COMMA)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    type.scale = static_cast<int32_t>(current().value.int_value);
                    advance();
                }
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after type parameters");
    }

    // Check for array notation: []
    if (match(TokenType::LEFT_BRACKET)) {
        type.is_array = true;
        if (check(TokenType::INTEGER_LITERAL)) {
            type.array_size = static_cast<int32_t>(current().value.int_value);
            advance();
        }
        expect(TokenType::RIGHT_BRACKET, "Expected ']' after array size");
    }

    auto peek_contextual = [&](const Token& token, const char* keyword) -> bool {
        if (token.type != TokenType::IDENTIFIER) {
            return false;
        }
        return caseInsensitiveEquals(stringPool().get(token.value.string_id), keyword);
    };

    // Check for WITH/WITHOUT TIME ZONE (don't consume unrelated WITH tokens)
    if (check(TokenType::KW_WITH)) {
        Token next = state_.lexer().peekToken();
        if (peek_contextual(next, "TIME")) {
            match(TokenType::KW_WITH);
            expectContextual("TIME", "Expected TIME after WITH");
            expectContextual("ZONE", "Expected ZONE after WITH TIME");
            type.with_time_zone = true;
        }
    } else if (state_.checkContextual("WITHOUT")) {
        Token next = state_.lexer().peekToken();
        if (peek_contextual(next, "TIME")) {
            matchContextual("WITHOUT");
            expectContextual("TIME", "Expected TIME after WITHOUT");
            matchContextual("ZONE");
            type.with_time_zone = false;
        }
    }

    type.span = makeSpan(start);
    return type;
}

// =============================================================================
// Column Constraints
// =============================================================================

std::vector<ColumnConstraint> Parser::parseColumnConstraints() {
    std::vector<ColumnConstraint> constraints;

    while (true) {
        // Check for CONSTRAINT name
        StringPool::StringId constraint_name = StringPool::INVALID_ID;
        if (matchContextual("CONSTRAINT")) {
            constraint_name = expectIdentifier("Expected constraint name");
        }

        ColumnConstraint constraint;
        constraint.name = constraint_name;
        bool found = false;

        if (match(TokenType::KW_NOT)) {
            expect(TokenType::KW_NULL, "Expected NULL after NOT");
            constraint.type = ConstraintType::NOT_NULL;
            found = true;
        } else if (match(TokenType::KW_NULL)) {
            constraint.type = ConstraintType::NULL_ALLOWED;
            found = true;
        } else if (matchContextual("PRIMARY")) {
            expectContextual("KEY", "Expected KEY after PRIMARY");
            constraint.type = ConstraintType::PRIMARY_KEY;
            found = true;
        } else if (matchContextual("UNIQUE")) {
            constraint.type = ConstraintType::UNIQUE;
            found = true;
        } else if (matchContextual("CHECK")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
            constraint.type = ConstraintType::CHECK;
            constraint.check_expr = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
            found = true;
        } else if (match(TokenType::KW_DEFAULT)) {
            constraint.type = ConstraintType::DEFAULT;
            constraint.default_expr = parseExpression();
            found = true;
        } else if (matchContextual("REFERENCES")) {
            constraint.type = ConstraintType::REFERENCES;
            constraint.ref_table = parseSchemaPath(state_);

            // Optional column list
            if (match(TokenType::LEFT_PAREN)) {
                do {
                    constraint.ref_columns.push_back(expectIdentifier("Expected column name"));
                } while (match(TokenType::COMMA));
                expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
            }

            // ON DELETE / ON UPDATE (DELETE and UPDATE are Gatekeeper keywords)
            while (match(TokenType::KW_ON)) {
                if (match(TokenType::KW_DELETE)) {
                    constraint.on_delete = parseForeignKeyAction();
                } else if (match(TokenType::KW_UPDATE)) {
                    constraint.on_update = parseForeignKeyAction();
                }
            }
            found = true;
        } else if (matchContextual("COLLATE")) {
            constraint.type = ConstraintType::COLLATE;
            constraint.collation = expectIdentifier("Expected collation name");
            found = true;
        } else if (matchContextual("GENERATED")) {
            constraint.type = ConstraintType::GENERATED;
            if (matchContextual("ALWAYS")) {
                constraint.generated_always = true;
            } else if (matchContextual("BY")) {
                expectContextual("DEFAULT", "Expected DEFAULT after BY");
            }

            if (match(TokenType::KW_AS)) {
                if (matchContextual("IDENTITY")) {
                    constraint.generated_as_identity = true;
                } else {
                    expect(TokenType::LEFT_PAREN, "Expected '(' after AS");
                    constraint.generated_expr = parseExpression();
                    expect(TokenType::RIGHT_PAREN, "Expected ')' after expression");
                }
            }
            found = true;
        }

        if (!found) {
            break;
        }

        constraint.span = makeSpan(currentLocation());
        constraints.push_back(constraint);
    }

    return constraints;
}

std::vector<DomainConstraint> Parser::parseDomainConstraints() {
    std::vector<DomainConstraint> constraints;

    while (true) {
        StringPool::StringId constraint_name = StringPool::INVALID_ID;
        SourceLocation constraint_start = currentLocation();
        if (matchContextual("CONSTRAINT")) {
            constraint_name = expectIdentifier("Expected constraint name");
        }

        DomainConstraint constraint;
        constraint.name = constraint_name;
        bool found = false;

        if (match(TokenType::KW_NOT)) {
            expect(TokenType::KW_NULL, "Expected NULL after NOT");
            constraint.type = DomainConstraintType::NOT_NULL;
            found = true;
        } else if (match(TokenType::KW_NULL)) {
            constraint.type = DomainConstraintType::NULL_ALLOWED;
            found = true;
        } else if (matchContextual("CHECK")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
            Expression* expr = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
            constraint.type = DomainConstraintType::CHECK;
            constraint.expression = extractExpressionText(expr);
            found = true;
        } else if (match(TokenType::KW_DEFAULT)) {
            Expression* expr = parseExpression();
            constraint.type = DomainConstraintType::DEFAULT;
            constraint.expression = extractExpressionText(expr);
            found = true;
        }

        if (!found) {
            if (constraint_name != StringPool::INVALID_ID) {
                error("Expected domain constraint after CONSTRAINT name");
            }
            break;
        }

        constraint.span = makeSpan(constraint_start);
        constraints.push_back(std::move(constraint));
    }

    return constraints;
}

void Parser::parseDomainIntegrityBlock(CreateDomainStmt* stmt) {
    if (!stmt) {
        return;
    }

    stmt->has_integrity = true;
    expect(TokenType::LEFT_PAREN, "Expected '(' after WITH INTEGRITY");

    auto parse_bool = [&]() -> bool {
        if (match(TokenType::KW_TRUE)) {
            return true;
        }
        if (match(TokenType::KW_FALSE)) {
            return false;
        }
        error("Expected TRUE or FALSE");
        return false;
    };

    auto parse_value_string = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return std::string(stringPool().get(id));
        }
        if (isIdentifier()) {
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }
        if (isGatekeeperKeyword(current().type)) {
            std::string text(state_.getTokenText(current()));
            advance();
            return text;
        }
        error("Expected identifier or string literal");
        return {};
    };

    auto to_upper = [](std::string_view input) {
        std::string out;
        out.reserve(input.size());
        for (char c : input) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        return out;
    };

    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (matchContextual("UNIQUENESS")) {
            expect(TokenType::EQUAL, "Expected '=' after UNIQUENESS");
            stmt->integrity.has_uniqueness = true;
            stmt->integrity.uniqueness = parse_bool();
        } else if (matchContextual("NORMALIZATION_FUNCTION")) {
            expect(TokenType::EQUAL, "Expected '=' after NORMALIZATION_FUNCTION");
            stmt->integrity.normalization_enabled = true;
            stmt->integrity.normalization_function = parse_value_string();
        } else if (matchContextual("NORMALIZATION")) {
            expect(TokenType::EQUAL, "Expected '=' after NORMALIZATION");
            std::string value = parse_value_string();
            std::string normalized = to_upper(value);
            if (normalized == "NONE") {
                stmt->integrity.normalization_enabled = false;
                stmt->integrity.normalization_function.clear();
            } else {
                stmt->integrity.normalization_enabled = true;
                stmt->integrity.normalization_function = value;
            }
        } else {
            error("Unknown WITH INTEGRITY option");
        }

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH INTEGRITY options");
}

void Parser::parseDomainSecurityBlock(CreateDomainStmt* stmt) {
    if (!stmt) {
        return;
    }

    stmt->has_security = true;
    expect(TokenType::LEFT_PAREN, "Expected '(' after WITH SECURITY");

    auto parse_bool = [&]() -> bool {
        if (match(TokenType::KW_TRUE)) {
            return true;
        }
        if (match(TokenType::KW_FALSE)) {
            return false;
        }
        error("Expected TRUE or FALSE");
        return false;
    };

    auto parse_value_string = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return std::string(stringPool().get(id));
        }
        if (isIdentifier()) {
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }
        if (isGatekeeperKeyword(current().type)) {
            std::string text(state_.getTokenText(current()));
            advance();
            return text;
        }
        error("Expected identifier or string literal");
        return {};
    };

    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (matchContextual("MASKING")) {
            expect(TokenType::EQUAL, "Expected '=' after MASKING");
            stmt->security.has_masking = true;
            stmt->security.masking = parse_value_string();
        } else if (matchContextual("MASK_PATTERN")) {
            expect(TokenType::EQUAL, "Expected '=' after MASK_PATTERN");
            stmt->security.has_mask_pattern = true;
            stmt->security.mask_pattern = parse_value_string();
        } else if (matchContextual("ENCRYPTION")) {
            expect(TokenType::EQUAL, "Expected '=' after ENCRYPTION");
            stmt->security.has_encryption = true;
            stmt->security.encryption = parse_value_string();
        } else if (matchContextual("AUDIT_ACCESS")) {
            expect(TokenType::EQUAL, "Expected '=' after AUDIT_ACCESS");
            stmt->security.has_audit_access = true;
            stmt->security.audit_access = parse_bool();
        } else if (matchContextual("REQUIRE_PRIVILEGE")) {
            expect(TokenType::EQUAL, "Expected '=' after REQUIRE_PRIVILEGE");
            stmt->security.has_required_privilege = true;
            stmt->security.required_privilege = parse_value_string();
        } else if (matchContextual("REQUIRE")) {
            expectContextual("PRIVILEGE", "Expected PRIVILEGE after REQUIRE");
            expect(TokenType::EQUAL, "Expected '=' after REQUIRE PRIVILEGE");
            stmt->security.has_required_privilege = true;
            stmt->security.required_privilege = parse_value_string();
        } else {
            error("Unknown WITH SECURITY option");
        }

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH SECURITY options");
}

void Parser::parseDomainValidationBlock(CreateDomainStmt* stmt) {
    if (!stmt) {
        return;
    }

    stmt->has_validation = true;
    expect(TokenType::LEFT_PAREN, "Expected '(' after WITH VALIDATION");

    auto parse_value_string = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return std::string(stringPool().get(id));
        }
        if (isIdentifier()) {
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }
        if (isGatekeeperKeyword(current().type)) {
            std::string text(state_.getTokenText(current()));
            advance();
            return text;
        }
        error("Expected identifier or string literal");
        return {};
    };

    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (matchContextual("FUNCTION")) {
            expect(TokenType::EQUAL, "Expected '=' after FUNCTION");
            stmt->validation.has_function = true;
            stmt->validation.function = parse_value_string();
        } else if (matchContextual("ERROR_MESSAGE")) {
            expect(TokenType::EQUAL, "Expected '=' after ERROR_MESSAGE");
            stmt->validation.has_error_message = true;
            stmt->validation.error_message = parse_value_string();
        } else {
            error("Unknown WITH VALIDATION option");
        }

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH VALIDATION options");
}

void Parser::parseDomainQualityBlock(CreateDomainStmt* stmt) {
    if (!stmt) {
        return;
    }

    stmt->has_quality = true;
    expect(TokenType::LEFT_PAREN, "Expected '(' after WITH QUALITY");

    auto parse_value_string = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return std::string(stringPool().get(id));
        }
        if (isIdentifier()) {
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }
        if (isGatekeeperKeyword(current().type)) {
            std::string text(state_.getTokenText(current()));
            advance();
            return text;
        }
        error("Expected identifier or string literal");
        return {};
    };

    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (matchContextual("PARSE_FUNCTION")) {
            expect(TokenType::EQUAL, "Expected '=' after PARSE_FUNCTION");
            stmt->quality.has_parse_function = true;
            stmt->quality.parse_function = parse_value_string();
        } else if (matchContextual("STANDARDIZE_FUNCTION")) {
            expect(TokenType::EQUAL, "Expected '=' after STANDARDIZE_FUNCTION");
            stmt->quality.has_standardize_function = true;
            stmt->quality.standardize_function = parse_value_string();
        } else if (matchContextual("ENRICH_FUNCTION")) {
            expect(TokenType::EQUAL, "Expected '=' after ENRICH_FUNCTION");
            stmt->quality.has_enrich_function = true;
            stmt->quality.enrich_function = parse_value_string();
        } else {
            error("Unknown WITH QUALITY option");
        }

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH QUALITY options");
}

void Parser::parseDomainOptionsBlock(CreateDomainStmt* stmt) {
    if (!stmt) {
        return;
    }

    expect(TokenType::LEFT_PAREN, "Expected '(' after WITH OPTIONS");

    auto parse_bool = [&]() -> bool {
        if (match(TokenType::KW_TRUE)) {
            return true;
        }
        if (match(TokenType::KW_FALSE)) {
            return false;
        }
        error("Expected TRUE or FALSE");
        return false;
    };

    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (matchContextual("WRAP")) {
            expect(TokenType::EQUAL, "Expected '=' after WRAP");
            stmt->enum_wrap = parse_bool();
        } else {
            error("Unknown WITH OPTIONS entry for CREATE DOMAIN");
            while (!check(TokenType::COMMA) && !check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
                advance();
            }
        }

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH OPTIONS options");
}

std::string Parser::extractExpressionText(Expression* expr) {
    if (!expr) {
        return {};
    }

    std::string_view text = state_.lexer().getTokenText(expr->span);
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(start, end - start + 1));
}

std::string Parser::captureStatementBody() {
    std::string_view input = state_.lexer().input();
    if (input.empty() || isAtEnd()) {
        return {};
    }

    size_t start = current().span.start.offset;
    size_t end = start;
    bool saw_begin = false;
    int begin_depth = 0;
    Token last = current();

    auto trim = [](std::string_view text) -> std::string {
        size_t first = text.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) {
            return {};
        }
        size_t last_pos = text.find_last_not_of(" \t\r\n");
        return std::string(text.substr(first, last_pos - first + 1));
    };

    while (!isAtEnd()) {
        if (check(TokenType::KW_BEGIN)) {
            saw_begin = true;
            begin_depth++;
        } else if (check(TokenType::KW_END)) {
            if (saw_begin && begin_depth > 0) {
                begin_depth--;
                if (begin_depth == 0) {
                    end = current().span.start.offset + current().span.length;
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

        last = current();
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

ForeignKeyAction Parser::parseForeignKeyAction() {
    if (matchContextual("CASCADE")) return ForeignKeyAction::CASCADE;
    if (matchContextual("RESTRICT")) return ForeignKeyAction::RESTRICT;
    if (matchContextual("NO")) {
        expectContextual("ACTION", "Expected ACTION after NO");
        return ForeignKeyAction::NO_ACTION;
    }
    if (match(TokenType::KW_SET)) {
        if (match(TokenType::KW_NULL)) return ForeignKeyAction::SET_NULL;
        if (match(TokenType::KW_DEFAULT)) return ForeignKeyAction::SET_DEFAULT;
    }
    return ForeignKeyAction::NO_ACTION;
}

// =============================================================================
// Table Constraints
// =============================================================================

TableConstraint* Parser::parseTableConstraint() {
    SourceLocation start = currentLocation();

    auto* constraint = arena_.create<TableConstraint>();

    // Optional CONSTRAINT name
    if (matchContextual("CONSTRAINT")) {
        constraint->name = expectIdentifier("Expected constraint name");
    }

    // Constraint type
    if (matchContextual("PRIMARY")) {
        expectContextual("KEY", "Expected KEY after PRIMARY");
        parsePrimaryKeyConstraint(constraint);
    } else if (matchContextual("UNIQUE")) {
        parseUniqueConstraint(constraint);
    } else if (matchContextual("FOREIGN")) {
        expectContextual("KEY", "Expected KEY after FOREIGN");
        parseForeignKeyConstraint(constraint);
    } else if (matchContextual("CHECK")) {
        parseCheckConstraint(constraint);
    } else {
        error("Expected constraint type (PRIMARY KEY, UNIQUE, FOREIGN KEY, CHECK)");
    }

    constraint->span = makeSpan(start);
    return constraint;
}

void Parser::parsePrimaryKeyConstraint(TableConstraint* constraint) {
    constraint->type = TableConstraintType::PRIMARY_KEY;

    expect(TokenType::LEFT_PAREN, "Expected '(' after PRIMARY KEY");
    do {
        constraint->columns.push_back(expectIdentifier("Expected column name"));
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");

    // Optional USING INDEX
    if (checkContextual("USING")) {
        matchContextual("USING");
        expectContextual("INDEX", "Expected INDEX after USING");
        constraint->using_index = true;
        constraint->index_method = expectIdentifier("Expected index method");
    }
}

void Parser::parseUniqueConstraint(TableConstraint* constraint) {
    constraint->type = TableConstraintType::UNIQUE;

    expect(TokenType::LEFT_PAREN, "Expected '(' after UNIQUE");
    do {
        constraint->columns.push_back(expectIdentifier("Expected column name"));
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
}

void Parser::parseForeignKeyConstraint(TableConstraint* constraint) {
    constraint->type = TableConstraintType::FOREIGN_KEY;

    expect(TokenType::LEFT_PAREN, "Expected '(' after FOREIGN KEY");
    do {
        constraint->columns.push_back(expectIdentifier("Expected column name"));
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");

    expectContextual("REFERENCES", "Expected REFERENCES after column list");
    constraint->ref_table = parseSchemaPath(state_);

    // Optional referenced column list
    if (match(TokenType::LEFT_PAREN)) {
        do {
            constraint->ref_columns.push_back(expectIdentifier("Expected column name"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
    }

    // ON DELETE / ON UPDATE (DELETE and UPDATE are Gatekeeper keywords)
    while (match(TokenType::KW_ON)) {
        if (match(TokenType::KW_DELETE)) {
            constraint->on_delete = parseForeignKeyAction();
        } else if (match(TokenType::KW_UPDATE)) {
            constraint->on_update = parseForeignKeyAction();
        }
    }
}

void Parser::parseCheckConstraint(TableConstraint* constraint) {
    constraint->type = TableConstraintType::CHECK;

    expect(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
    constraint->check_expr = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
}

// =============================================================================
// CREATE INDEX
// =============================================================================

CreateIndexStmt* Parser::parseCreateIndex() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateIndexStmt>();

    // Check for CONCURRENTLY
    if (matchContextual("CONCURRENTLY")) {
        stmt->concurrent = true;
    }

    // Check for IF NOT EXISTS (IF, NOT, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    // Index name (optional for some databases)
    // ON is a Gatekeeper keyword, so check TokenType
    if (isIdentifier() && !check(TokenType::KW_ON)) {
        stmt->index_name = currentIdentifier();
    }

    // ON table_name (ON is a Gatekeeper keyword)
    expect(TokenType::KW_ON, "Expected ON after index name");
    stmt->table_path = parseSchemaPath(state_);

    // Optional USING method (USING is a Gatekeeper keyword)
    if (match(TokenType::KW_USING)) {
        if (matchContextual("BTREE")) stmt->index_type = IndexType::BTREE;
        else if (matchContextual("HASH")) stmt->index_type = IndexType::HASH;
        else if (matchContextual("GIN")) stmt->index_type = IndexType::GIN;
        else if (matchContextual("GIST")) stmt->index_type = IndexType::GIST;
        else if (matchContextual("SPGIST")) stmt->index_type = IndexType::SPGIST;
        else if (matchContextual("BRIN")) stmt->index_type = IndexType::BRIN;
        else if (matchContextual("RTREE")) stmt->index_type = IndexType::RTREE;
        else if (matchContextual("HNSW")) stmt->index_type = IndexType::HNSW;
        else if (matchContextual("BITMAP")) stmt->index_type = IndexType::BITMAP;
        else if (matchContextual("COLUMNSTORE")) stmt->index_type = IndexType::COLUMNSTORE;
        else if (matchContextual("LSM")) stmt->index_type = IndexType::LSM;
        else error("Unknown index type");
    }

    // Column list
    expect(TokenType::LEFT_PAREN, "Expected '(' after table name");
    do {
        IndexColumn col;

        // Could be column name or expression
        if (isIdentifier()) {
            col.column = currentIdentifier();
        } else if (check(TokenType::LEFT_PAREN)) {
            // Expression index
            col.expr = parseParenExpr();
        }

        // Sort order
        if (matchContextual("ASC")) col.ascending = true;
        else if (matchContextual("DESC")) col.ascending = false;

        // NULLS FIRST/LAST
        if (matchContextual("NULLS")) {
            if (matchContextual("FIRST")) col.nulls_first = true;
            else if (matchContextual("LAST")) col.nulls_last = true;
        }

        stmt->columns.push_back(col);
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");

    // INCLUDE clause
    if (matchContextual("INCLUDE")) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after INCLUDE");
        do {
            stmt->include_columns.push_back(expectIdentifier("Expected column name"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after INCLUDE columns");
    }

    // WHERE clause (partial index)
    if (match(TokenType::KW_WHERE)) {
        stmt->where_clause = parseExpression();
    }

    // TABLESPACE
    if (matchContextual("TABLESPACE")) {
        stmt->tablespace = parseSchemaPath(state_);
        stmt->has_tablespace = true;
    }

    // WITH (index options)
    if (match(TokenType::KW_WITH)) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after WITH");

        auto parse_bool = [&]() -> bool {
            if (check(TokenType::INTEGER_LITERAL)) {
                bool value = current().value.int_value != 0;
                advance();
                return value;
            }
            if (isIdentifier()) {
                auto text = stringPool().get(current().value.string_id);
                advance();
                if (caseInsensitiveEquals(text, "TRUE")) {
                    return true;
                }
                if (caseInsensitiveEquals(text, "FALSE")) {
                    return false;
                }
            }
            error("Expected boolean value for index option");
            return false;
        };

        auto parse_double = [&]() -> double {
            if (check(TokenType::FLOAT_LITERAL)) {
                double value = current().value.float_value;
                advance();
                return value;
            }
            if (check(TokenType::INTEGER_LITERAL)) {
                double value = static_cast<double>(current().value.int_value);
                advance();
                return value;
            }
            error("Expected numeric value for index option");
            return 0.0;
        };

        while (!check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            if (!isIdentifier()) {
                error("Expected index option name");
                break;
            }

            auto opt_name = stringPool().get(current().value.string_id);
            advance();
            expect(TokenType::EQUAL, "Expected '=' after index option name");

            if (caseInsensitiveEquals(opt_name, "BLOOM_FILTER")) {
                stmt->options.bloom_filter_enabled = parse_bool();
                stmt->options.bloom_filter_set = true;
            } else if (caseInsensitiveEquals(opt_name, "BLOOM_FPR")) {
                stmt->options.bloom_fpr = parse_double();
                stmt->options.bloom_fpr_set = true;
            } else {
                error("Unknown index option");
                return stmt;
            }

            if (!match(TokenType::COMMA)) {
                break;
            }
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after index options");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE VIEW
// =============================================================================

CreateViewStmt* Parser::parseCreateView(bool or_replace) {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateViewStmt>();
    stmt->or_replace = or_replace;

    // Check for IF NOT EXISTS (IF, NOT, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    // View name
    stmt->view_path = parseSchemaPath(state_);

    // Optional column name list
    if (match(TokenType::LEFT_PAREN)) {
        do {
            stmt->column_names.push_back(expectIdentifier("Expected column name"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
    }

    // AS SELECT ...
    expect(TokenType::KW_AS, "Expected AS before SELECT");

    // Parse the SELECT statement
    if (match(TokenType::KW_SELECT)) {
        stmt->query = parseSelect();
    } else {
        error("Expected SELECT after AS in CREATE VIEW");
    }

    // WITH CHECK OPTION
    if (match(TokenType::KW_WITH)) {
        if (matchContextual("CHECK")) {
            expectContextual("OPTION", "Expected OPTION after CHECK");
            stmt->with_check_option = true;
        }
        if (matchContextual("LOCAL")) {
            stmt->check_option_local = true;
        } else if (matchContextual("CASCADED")) {
            stmt->check_option_local = false;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE SEQUENCE
// =============================================================================

CreateSequenceStmt* Parser::parseCreateSequence() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateSequenceStmt>();

    // Check for IF NOT EXISTS (IF, NOT, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    // Sequence name
    stmt->sequence_path = parseSchemaPath(state_);

    // Sequence options
    while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        // START is a Gatekeeper keyword
        if (match(TokenType::KW_START)) {
            match(TokenType::KW_WITH);  // WITH is also a Gatekeeper keyword
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->start_with = current().value.int_value;
                advance();
            }
        } else if (matchContextual("INCREMENT")) {
            matchContextual("BY");
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->increment_by = current().value.int_value;
                advance();
            }
        } else if (matchContextual("MINVALUE")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->min_value = current().value.int_value;
                advance();
            }
        } else if (matchContextual("MAXVALUE")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->max_value = current().value.int_value;
                advance();
            }
        } else if (checkContextual("NO")) {
            matchContextual("NO");
            if (matchContextual("MINVALUE")) stmt->no_min_value = true;
            else if (matchContextual("MAXVALUE")) stmt->no_max_value = true;
            else if (matchContextual("CYCLE")) stmt->cycle = false;
        } else if (matchContextual("CACHE")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->cache = current().value.int_value;
                advance();
            }
        } else if (matchContextual("CYCLE")) {
            stmt->cycle = true;
        } else if (matchContextual("OWNED")) {
            expectContextual("BY", "Expected BY after OWNED");
            stmt->owned_by_table = parseSchemaPath(state_);
            expect(TokenType::DOT, "Expected '.' before column name");
            stmt->owned_by_column = expectIdentifier("Expected column name");
            stmt->has_owned_by = true;
        } else {
            break;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE SCHEMA
// =============================================================================

CreateSchemaStmt* Parser::parseCreateSchema() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateSchemaStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    if (!checkContextual("AUTHORIZATION")) {
        stmt->schema_path = parseSchemaPath(state_);
    }

    if (matchContextual("AUTHORIZATION")) {
        stmt->has_owner = true;
        stmt->owner = expectIdentifier("Expected owner name");
        if (stmt->schema_path.isEmpty()) {
            SchemaPath path;
            path.components.push_back(stmt->owner);
            stmt->schema_path = path;
        }
    }

    if (stmt->schema_path.isEmpty()) {
        error("Expected schema name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE DATABASE
// =============================================================================

CreateDatabaseStmt* Parser::parseCreateDatabase() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateDatabaseStmt>();
    // Spec: docs/specifications/ddl/DDL_DATABASES.md

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    auto parse_string_or_identifier = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return std::string(stringPool().get(id));
        }
        if (isIdentifier()) {
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }
        return {};
    };

    auto parse_option_value = [&](bool* ok) -> std::string {
        *ok = true;
        bool negate = false;
        bool saw_sign = false;
        if (match(TokenType::MINUS)) {
            negate = true;
            saw_sign = true;
        } else {
            if (match(TokenType::PLUS)) {
                saw_sign = true;
            }
        }

        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            if (saw_sign) {
                *ok = false;
                return {};
            }
            return std::string(stringPool().get(id));
        }
        if (check(TokenType::INTEGER_LITERAL) || check(TokenType::FLOAT_LITERAL)) {
            std::string text = std::string(state_.getTokenText(current()));
            advance();
            if (negate) {
                return "-" + text;
            }
            return text;
        }
        if (isIdentifier()) {
            if (saw_sign) {
                *ok = false;
                return {};
            }
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }

        *ok = false;
        return {};
    };

    auto parse_alias_id = [&]() -> StringPool::StringId {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return id;
        }
        if (isIdentifier()) {
            return currentIdentifier();
        }
        error("Expected alias name");
        return StringPool::INVALID_ID;
    };

    if (matchContextual("EMULATED")) {
        std::string dialect = parse_string_or_identifier();
        if (dialect.empty()) {
            error("Expected emulation dialect after EMULATED");
        }
        auto normalize_lower = [](std::string value) {
            for (char& ch : value) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return value;
        };
        dialect = normalize_lower(dialect);

        std::string server;
        bool server_set = false;
        auto parse_server = [&]() {
            server = parse_string_or_identifier();
            if (server.empty()) {
                error("Expected server name");
            }
            server_set = true;
        };

        if (match(TokenType::KW_ON)) {
            expectContextual("SERVER", "Expected SERVER after ON");
            parse_server();
        } else if (matchContextual("SERVER")) {
            parse_server();
        }

        std::string source_spec = parse_string_or_identifier();
        if (source_spec.empty()) {
            error("Expected emulated database source specification");
        }

        auto parse_options_block = [&]() {
            if (matchContextual("OPTIONS") || matchContextual("OPTION")) {
                // OPTIONS keyword consumed
            }
            expect(TokenType::LEFT_PAREN, "Expected '(' after WITH");
            while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
                std::string key = parse_string_or_identifier();
                if (key.empty()) {
                    error("Expected option key");
                    break;
                }

                bool has_value = false;
                std::string value;
                if (match(TokenType::EQUAL) || match(TokenType::EQUALS_GREATER)) {
                    value = parse_option_value(&has_value);
                } else {
                    value = parse_option_value(&has_value);
                }

                if (!has_value) {
                    error("Expected option value");
                    break;
                }

                DatabaseOption opt;
                opt.key = stringPool().intern(key);
                opt.value = stringPool().intern(value);
                stmt->options.push_back(opt);

                if (!match(TokenType::COMMA)) {
                    break;
                }
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after options");
        };

        auto parse_alias_list = [&]() {
            bool has_paren = match(TokenType::LEFT_PAREN);
            while (!isAtEnd()) {
                StringPool::StringId alias = parse_alias_id();
                if (alias != StringPool::INVALID_ID) {
                    stmt->aliases.push_back(alias);
                }
                if (match(TokenType::COMMA)) {
                    continue;
                }
                break;
            }
            if (has_paren) {
                expect(TokenType::RIGHT_PAREN, "Expected ')' after alias list");
            }
        };

        while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
            if (!server_set && match(TokenType::KW_ON)) {
                expectContextual("SERVER", "Expected SERVER after ON");
                parse_server();
                continue;
            }
            if (!server_set && matchContextual("SERVER")) {
                parse_server();
                continue;
            }
            if (matchContextual("ALIAS") || matchContextual("ALIASES")) {
                parse_alias_list();
                continue;
            }
            if (match(TokenType::KW_WITH)) {
                parse_options_block();
                continue;
            }
            break;
        }

        auto is_windows_drive = [](std::string_view spec) {
            return spec.size() >= 2 &&
                   std::isalpha(static_cast<unsigned char>(spec[0])) &&
                   spec[1] == ':' &&
                   (spec.size() == 2 || spec[2] == '/' || spec[2] == '\\');
        };

        std::string spec_server;
        std::string spec_path = source_spec;
        if (!is_windows_drive(spec_path)) {
            size_t colon = spec_path.find(':');
            if (colon != std::string::npos) {
                spec_server = spec_path.substr(0, colon);
                spec_path = spec_path.substr(colon + 1);
            }
        }

        if (!spec_server.empty()) {
            if (server_set && scratchbird::core::IdentifierUtils::toUpper(server) !=
                                  scratchbird::core::IdentifierUtils::toUpper(spec_server)) {
                error("Server specified twice with different values");
            }
            server = spec_server;
        }

        if (server.empty()) {
            server = "localhost";
        }
        server = normalize_lower(server);

        auto split_path_components = [&](std::string_view path_in) {
            std::vector<std::string> components;
            std::string path(path_in);
            while (!path.empty() && (path.front() == '/' || path.front() == '\\')) {
                path.erase(path.begin());
            }

            if (is_windows_drive(path)) {
                char drive = static_cast<char>(std::tolower(static_cast<unsigned char>(path[0])));
                components.push_back(std::string(1, drive));
                path.erase(0, 2);
                if (!path.empty() && (path.front() == '/' || path.front() == '\\')) {
                    path.erase(path.begin());
                }
            }

            std::string current;
            for (char ch : path) {
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
            return components;
        };

        std::string db_name;
        std::vector<std::string> path_components;
        bool looks_like_path = spec_path.find('/') != std::string::npos ||
                               spec_path.find('\\') != std::string::npos ||
                               is_windows_drive(spec_path);

        if (looks_like_path) {
            auto components = split_path_components(spec_path);
            if (!components.empty()) {
                db_name = components.back();
                components.pop_back();
                path_components = std::move(components);
            }

            size_t dot = db_name.find_last_of('.');
            if (dot != std::string::npos && dot > 0) {
                db_name = db_name.substr(0, dot);
            }
        } else {
            db_name = spec_path;
        }

        if (db_name.empty()) {
            error("Emulated database name is empty");
        }

        SchemaPath path;
        path.type = PathType::ABSOLUTE;
        path.components.push_back(stringPool().intern("remote"));
        path.components.push_back(stringPool().intern("emulation"));
        path.components.push_back(stringPool().intern(dialect));
        path.components.push_back(stringPool().intern(server));
        for (const auto& comp : path_components) {
            if (!comp.empty()) {
                path.components.push_back(stringPool().intern(comp));
            }
        }
        path.components.push_back(stringPool().intern(db_name));
        stmt->database_path = std::move(path);
        if (!source_spec.empty()) {
            stmt->source_spec = stringPool().intern(source_spec);
        }
    } else {
        stmt->database_path = parseSchemaPath(state_);
        if (stmt->database_path.isEmpty()) {
            error("Expected database name");
        }
        std::string spec = schemaPathToString(stmt->database_path, stringPool());
        if (!spec.empty()) {
            stmt->source_spec = stringPool().intern(spec);
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE DOMAIN
// =============================================================================

CreateDomainStmt* Parser::parseCreateDomain() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateDomainStmt>();
    // Spec: docs/specifications/DDL_DOMAINS_COMPREHENSIVE.md

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    stmt->domain_path = parseSchemaPath(state_);
    if (stmt->domain_path.isEmpty()) {
        error("Expected domain name");
    }

    // Optional AS keyword
    match(TokenType::KW_AS);

    if (matchContextual("RECORD")) {
        stmt->domain_kind = DomainKind::RECORD;
        expect(TokenType::LEFT_PAREN, "Expected '(' after RECORD");
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            SourceLocation field_start = currentLocation();
            DomainRecordField field;
            field.name = expectIdentifier("Expected field name");
            field.type = parseTypeName();

            if (match(TokenType::KW_NOT)) {
                expect(TokenType::KW_NULL, "Expected NULL after NOT");
                field.nullable = false;
            } else if (match(TokenType::KW_NULL)) {
                field.nullable = true;
            }

            if (match(TokenType::KW_DEFAULT)) {
                Expression* expr = parseExpression();
                field.default_value = extractExpressionText(expr);
                field.has_default = true;
            }

            field.span = makeSpan(field_start);
            stmt->record_fields.push_back(std::move(field));

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after RECORD fields");
    } else if (matchContextual("ENUM")) {
        stmt->domain_kind = DomainKind::ENUM;
        expect(TokenType::LEFT_PAREN, "Expected '(' after ENUM");
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            SourceLocation value_start = currentLocation();
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal for ENUM label");
                break;
            }
            DomainEnumValue value;
            value.label = current().value.string_id;
            advance();
            if (match(TokenType::EQUAL)) {
                if (!check(TokenType::INTEGER_LITERAL)) {
                    error("Expected integer position after '='");
                } else {
                    value.has_position = true;
                    value.position = static_cast<int32_t>(current().value.int_value);
                    advance();
                }
            }
            value.span = makeSpan(value_start);
            stmt->enum_values.push_back(std::move(value));

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after ENUM values");
    } else if (match(TokenType::KW_SET) || matchContextual("SET")) {
        stmt->domain_kind = DomainKind::SET;
        expectContextual("OF", "Expected OF after SET");
        stmt->set_element_type = parseTypeName();
    } else if (matchContextual("VARIANT")) {
        stmt->domain_kind = DomainKind::VARIANT;
        expect(TokenType::LEFT_PAREN, "Expected '(' after VARIANT");
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            stmt->variant_allowed_types.push_back(parseTypeName());
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after VARIANT types");
    } else {
        stmt->domain_kind = DomainKind::BASIC;
        stmt->base_type = parseTypeName();
    }

    if (matchContextual("INHERITS")) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after INHERITS");
        stmt->parent_domain_path = parseSchemaPath(state_);
        stmt->has_inherits = true;
        expect(TokenType::RIGHT_PAREN, "Expected ')' after INHERITS");
    }

    auto parse_option_string = [&]() -> std::string {
        expect(TokenType::LEFT_PAREN, "Expected '('");
        std::string out;
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            out = std::string(stringPool().get(id));
            advance();
        } else if (isIdentifier()) {
            auto id = currentIdentifier();
            out = std::string(stringPool().get(id));
            advance();
        } else {
            error("Expected identifier or string literal");
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')'");
        return out;
    };

    while (true) {
        bool parsed_any = false;

        // Domain constraints and options (DEFAULT/NOT NULL/CHECK)
        StringPool::StringId constraint_name = StringPool::INVALID_ID;
        SourceLocation constraint_start = currentLocation();
        if (matchContextual("CONSTRAINT")) {
            constraint_name = expectIdentifier("Expected constraint name");
        }

        DomainConstraint constraint;
        constraint.name = constraint_name;
        bool found_constraint = false;

        if (match(TokenType::KW_NOT)) {
            expect(TokenType::KW_NULL, "Expected NULL after NOT");
            constraint.type = DomainConstraintType::NOT_NULL;
            found_constraint = true;
        } else if (match(TokenType::KW_NULL)) {
            constraint.type = DomainConstraintType::NULL_ALLOWED;
            found_constraint = true;
        } else if (matchContextual("CHECK")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
            Expression* expr = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
            constraint.type = DomainConstraintType::CHECK;
            constraint.expression = extractExpressionText(expr);
            found_constraint = true;
        } else if (match(TokenType::KW_DEFAULT)) {
            Expression* expr = parseExpression();
            constraint.type = DomainConstraintType::DEFAULT;
            constraint.expression = extractExpressionText(expr);
            found_constraint = true;
        }

        if (found_constraint) {
            constraint.span = makeSpan(constraint_start);
            stmt->constraints.push_back(std::move(constraint));
            parsed_any = true;
        } else if (constraint_name != StringPool::INVALID_ID) {
            error("Expected domain constraint after CONSTRAINT name");
            parsed_any = true;
        }

        if (matchContextual("COLLATE")) {
            stmt->has_collation = true;
            stmt->collation_name = std::string(stringPool().get(expectIdentifier("Expected collation name")));
            parsed_any = true;
        }

        if (match(TokenType::KW_WITH)) {
            if (matchContextual("DIALECT")) {
                stmt->dialect_tag = parse_option_string();
                stmt->has_dialect = true;
            } else if (matchContextual("COMPAT")) {
                stmt->compat_name = parse_option_string();
                stmt->has_compat = true;
            } else if (matchContextual("INTEGRITY")) {
                parseDomainIntegrityBlock(stmt);
            } else if (matchContextual("SECURITY")) {
                parseDomainSecurityBlock(stmt);
            } else if (matchContextual("VALIDATION")) {
                parseDomainValidationBlock(stmt);
            } else if (matchContextual("QUALITY")) {
                parseDomainQualityBlock(stmt);
            } else if (matchContextual("OPTIONS")) {
                parseDomainOptionsBlock(stmt);
            } else {
                error("WITH block type not supported for CREATE DOMAIN");
                if (match(TokenType::LEFT_PAREN)) {
                    int depth = 1;
                    while (!isAtEnd() && depth > 0) {
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
            parsed_any = true;
        }

        if (!parsed_any) {
            break;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CreateFunctionStmt* Parser::parseCreateFunction(bool or_replace) {
    auto* stmt = arena_.create<CreateFunctionStmt>();
    stmt->or_replace = or_replace;
    stmt->function_path = parseSchemaPath(state_);

    if (match(TokenType::LEFT_PAREN)) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                RoutineParam param;
                if (matchContextual("IN")) {
                    param.mode = RoutineParamMode::IN;
                } else if (matchContextual("OUT")) {
                    param.mode = RoutineParamMode::OUT;
                } else if (matchContextual("INOUT")) {
                    param.mode = RoutineParamMode::INOUT;
                }

                param.name = expectIdentifier("Expected parameter name");
                param.type = parseTypeName();
                if (match(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                    param.default_value = parseExpression();
                    param.has_default = true;
                }
                stmt->params.push_back(std::move(param));
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after function parameters");
    }

    if (!matchContextual("RETURNS")) {
        error("Expected RETURNS in CREATE FUNCTION");
    }
    stmt->return_type = parseTypeName();

    while (matchContextual("DETERMINISTIC")) {
        stmt->deterministic = true;
    }

    if (matchContextual("SQL")) {
        expectContextual("SECURITY", "Expected SECURITY after SQL");
        if (matchContextual("DEFINER")) {
            stmt->sql_security = RoutineSqlSecurity::DEFINER;
        } else if (matchContextual("INVOKER")) {
            stmt->sql_security = RoutineSqlSecurity::INVOKER;
        }
    }

    if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
        error("Expected AS before function body");
    }
    std::string body = captureStatementBody();
    if (!body.empty()) {
        stmt->body = stringPool().intern(body);
    }

    return stmt;
}

CreateProcedureStmt* Parser::parseCreateProcedure(bool or_replace) {
    auto* stmt = arena_.create<CreateProcedureStmt>();
    stmt->or_replace = or_replace;
    stmt->procedure_path = parseSchemaPath(state_);

    if (match(TokenType::LEFT_PAREN)) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                RoutineParam param;
                if (matchContextual("IN")) {
                    param.mode = RoutineParamMode::IN;
                } else if (matchContextual("OUT")) {
                    param.mode = RoutineParamMode::OUT;
                } else if (matchContextual("INOUT")) {
                    param.mode = RoutineParamMode::INOUT;
                }

                param.name = expectIdentifier("Expected parameter name");
                param.type = parseTypeName();
                if (match(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                    param.default_value = parseExpression();
                    param.has_default = true;
                }
                stmt->params.push_back(std::move(param));
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after procedure parameters");
    }

    if (matchContextual("RETURNS")) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after RETURNS");
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                RoutineParam param;
                param.mode = RoutineParamMode::OUT;
                param.name = expectIdentifier("Expected return parameter name");
                param.type = parseTypeName();
                stmt->params.push_back(std::move(param));
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after RETURNS parameters");
    }

    if (matchContextual("SQL")) {
        expectContextual("SECURITY", "Expected SECURITY after SQL");
        if (matchContextual("DEFINER")) {
            stmt->sql_security = RoutineSqlSecurity::DEFINER;
        } else if (matchContextual("INVOKER")) {
            stmt->sql_security = RoutineSqlSecurity::INVOKER;
        }
    }

    if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
        error("Expected AS before procedure body");
    }
    std::string body = captureStatementBody();
    if (!body.empty()) {
        stmt->body = stringPool().intern(body);
    }

    return stmt;
}

CreateTriggerStmt* Parser::parseCreateTrigger(bool or_replace) {
    auto* stmt = arena_.create<CreateTriggerStmt>();
    stmt->or_replace = or_replace;
    stmt->trigger_name = expectIdentifier("Expected trigger name");

    bool has_table_path = false;
    if (matchContextual("FOR")) {
        stmt->table_path = parseSchemaPath(state_);
        has_table_path = true;
    }

    if (matchContextual("ACTIVE")) {
        stmt->active = true;
    } else if (matchContextual("INACTIVE")) {
        stmt->active = false;
    }

    if (matchContextual("BEFORE")) {
        stmt->timing = TriggerTiming::BEFORE;
    } else if (matchContextual("AFTER")) {
        stmt->timing = TriggerTiming::AFTER;
    } else {
        error("Expected BEFORE or AFTER in CREATE TRIGGER");
    }

    stmt->event_mask = 0;
    auto add_event = [&](TriggerEvent event) {
        stmt->event_mask |= static_cast<uint8_t>(1u << static_cast<uint8_t>(event));
    };
    auto match_event = [&](TriggerEvent event, TokenType kw, const char* text) -> bool {
        if (match(kw) || matchContextual(text)) {
            add_event(event);
            return true;
        }
        return false;
    };

    if (!match_event(TriggerEvent::INSERT, TokenType::KW_INSERT, "INSERT") &&
        !match_event(TriggerEvent::UPDATE, TokenType::KW_UPDATE, "UPDATE") &&
        !match_event(TriggerEvent::DELETE, TokenType::KW_DELETE, "DELETE")) {
        // No-op; handled by event_mask check below.
    }

    if (stmt->event_mask == 0) {
        error("Expected INSERT, UPDATE, or DELETE in CREATE TRIGGER");
    }

    while (match(TokenType::KW_OR) || matchContextual("OR")) {
        if (match_event(TriggerEvent::INSERT, TokenType::KW_INSERT, "INSERT") ||
            match_event(TriggerEvent::UPDATE, TokenType::KW_UPDATE, "UPDATE") ||
            match_event(TriggerEvent::DELETE, TokenType::KW_DELETE, "DELETE")) {
            continue;
        }
        error("Expected trigger event after OR");
        break;
    }

    if (!has_table_path) {
        if (match(TokenType::KW_ON) || matchContextual("ON")) {
            stmt->table_path = parseSchemaPath(state_);
            has_table_path = true;
        } else {
            error("Expected ON <table> or FOR <table> in CREATE TRIGGER");
        }
    }

    if (matchContextual("FOR")) {
        matchContextual("EACH");
        if (matchContextual("ROW")) {
            stmt->granularity = TriggerGranularity::FOR_EACH_ROW;
        } else if (matchContextual("STATEMENT")) {
            stmt->granularity = TriggerGranularity::FOR_EACH_STATEMENT;
        }
    }

    if (matchContextual("POSITION")) {
        if (check(TokenType::INTEGER_LITERAL)) {
            advance();
        }
    }

    if (check(TokenType::KW_EXECUTE)) {
        std::string body = captureStatementBody();
        if (!body.empty()) {
            stmt->body = stringPool().intern(body);
        }
        return stmt;
    }

    if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
        error("Expected AS before trigger body");
    }
    std::string body = captureStatementBody();
    if (!body.empty()) {
        stmt->body = stringPool().intern(body);
    }

    return stmt;
}

// =============================================================================
// ALTER Statements
// =============================================================================

Statement* Parser::parseAlter() {
    ParseModeGuard guard(state_, ParseMode::DDL);

    if (matchContextual("TABLE")) return parseAlterTable();
    if (matchContextual("SCHEMA")) return parseAlterSchema();
    if (matchContextual("DATABASE")) return parseAlterDatabase();
    if (matchContextual("DOMAIN")) return parseAlterDomain();

    auto parse_rename_move = [&](DdlObjectType object_type) -> Statement* {
        SourceLocation start = currentLocation();

        bool if_exists = false;
        if (match(TokenType::KW_IF)) {
            expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }

        SchemaPath object_path = parseSchemaPath(state_);

        if (matchContextual("RENAME")) {
            expectContextual("TO", "Expected TO after RENAME");
            auto* stmt = arena_.create<RenameObjectStmt>();
            stmt->object_type = object_type;
            stmt->if_exists = if_exists;
            stmt->object_path = object_path;
            stmt->new_name = expectIdentifier("Expected new name");
            stmt->span = makeSpan(start);
            return stmt;
        }

        if (match(TokenType::KW_SET)) {
            if (matchContextual("SCHEMA")) {
                auto* stmt = arena_.create<MoveObjectStmt>();
                stmt->object_type = object_type;
                stmt->if_exists = if_exists;
                stmt->object_path = object_path;
                stmt->target_schema = parseSchemaPath(state_);
                stmt->span = makeSpan(start);
                return stmt;
            }
        }

        error("Expected RENAME TO or SET SCHEMA after object name");
        return nullptr;
    };

    if (matchContextual("VIEW")) return parse_rename_move(DdlObjectType::VIEW);
    if (matchContextual("INDEX")) {
        SourceLocation start = currentLocation();
        bool if_exists = false;
        if (match(TokenType::KW_IF)) {
            expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }

        SchemaPath index_path = parseSchemaPath(state_);
        if (index_path.isEmpty()) {
            error("Expected index name");
            return nullptr;
        }

        if (matchContextual("RENAME")) {
            expectContextual("TO", "Expected TO after RENAME");
            auto* stmt = arena_.create<RenameObjectStmt>();
            stmt->object_type = DdlObjectType::INDEX;
            stmt->if_exists = if_exists;
            stmt->object_path = index_path;
            stmt->new_name = expectIdentifier("Expected new name");
            stmt->span = makeSpan(start);
            return stmt;
        }

        if (match(TokenType::KW_SET)) {
            if (matchContextual("SCHEMA")) {
                auto* stmt = arena_.create<MoveObjectStmt>();
                stmt->object_type = DdlObjectType::INDEX;
                stmt->if_exists = if_exists;
                stmt->object_path = index_path;
                stmt->target_schema = parseSchemaPath(state_);
                stmt->span = makeSpan(start);
                return stmt;
            }

            auto* stmt = arena_.create<AlterIndexStmt>();
            stmt->index_path = index_path;
            stmt->action = AlterIndexAction::SET_OPTIONS;

            expect(TokenType::LEFT_PAREN, "Expected '(' after SET in ALTER INDEX");

            auto parse_bool = [&]() -> bool {
                if (check(TokenType::INTEGER_LITERAL)) {
                    bool value = current().value.int_value != 0;
                    advance();
                    return value;
                }
                if (isIdentifier()) {
                    auto text = stringPool().get(current().value.string_id);
                    advance();
                    if (caseInsensitiveEquals(text, "TRUE")) {
                        return true;
                    }
                    if (caseInsensitiveEquals(text, "FALSE")) {
                        return false;
                    }
                }
                error("Expected boolean value for index option");
                return false;
            };

            auto parse_double = [&]() -> double {
                if (check(TokenType::FLOAT_LITERAL)) {
                    double value = current().value.float_value;
                    advance();
                    return value;
                }
                if (check(TokenType::INTEGER_LITERAL)) {
                    double value = static_cast<double>(current().value.int_value);
                    advance();
                    return value;
                }
                error("Expected numeric value for index option");
                return 0.0;
            };

            while (!check(TokenType::RIGHT_PAREN) &&
                   !check(TokenType::SEMICOLON) &&
                   !check(TokenType::END_OF_FILE)) {
                if (!isIdentifier()) {
                    error("Expected index option name");
                    break;
                }

                auto opt_name = stringPool().get(current().value.string_id);
                advance();
                expect(TokenType::EQUAL, "Expected '=' after index option name");

                if (caseInsensitiveEquals(opt_name, "BLOOM_FILTER")) {
                    stmt->options.bloom_filter_enabled = parse_bool();
                    stmt->options.bloom_filter_set = true;
                } else if (caseInsensitiveEquals(opt_name, "BLOOM_FPR")) {
                    stmt->options.bloom_fpr = parse_double();
                    stmt->options.bloom_fpr_set = true;
                } else {
                    error("Unknown index option");
                    return stmt;
                }

                if (!match(TokenType::COMMA)) {
                    break;
                }
            }

            expect(TokenType::RIGHT_PAREN, "Expected ')' after index options");
            stmt->span = makeSpan(start);
            return stmt;
        }

        error("Expected RENAME TO or SET after index name");
        return nullptr;
    }
    if (matchContextual("SEQUENCE")) return parse_rename_move(DdlObjectType::SEQUENCE);
    if (matchContextual("DOMAIN")) return parse_rename_move(DdlObjectType::DOMAIN);
    if (matchContextual("TRIGGER")) return parse_rename_move(DdlObjectType::TRIGGER);
    if (matchContextual("FUNCTION")) return parse_rename_move(DdlObjectType::FUNCTION);
    if (matchContextual("PROCEDURE")) return parse_rename_move(DdlObjectType::PROCEDURE);
    if (matchContextual("PACKAGE")) return parse_rename_move(DdlObjectType::PACKAGE);
    if (matchContextual("TABLESPACE")) return parse_rename_move(DdlObjectType::TABLESPACE);
    if (matchContextual("ROLE")) return parse_rename_move(DdlObjectType::ROLE);
    if (matchContextual("USER")) return parse_rename_move(DdlObjectType::USER);
    if (matchContextual("GROUP")) return parse_rename_move(DdlObjectType::GROUP);

    error("Expected object type after ALTER");
    return nullptr;
}

AlterSchemaStmt* Parser::parseAlterSchema() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AlterSchemaStmt>();
    stmt->schema_path = parseSchemaPath(state_);
    if (stmt->schema_path.isEmpty()) {
        error("Expected schema name");
    }

    if (matchContextual("RENAME")) {
        expectContextual("TO", "Expected TO after RENAME");
        stmt->action = AlterSchemaAction::RENAME;
        stmt->new_name = expectIdentifier("Expected new schema name");
    } else if (matchContextual("OWNER")) {
        expectContextual("TO", "Expected TO after OWNER");
        stmt->action = AlterSchemaAction::SET_OWNER;
        stmt->owner = expectIdentifier("Expected owner name");
    } else if (matchContextual("SET")) {
        if (matchContextual("PATH")) {
            stmt->action = AlterSchemaAction::SET_PATH;
            stmt->new_path = parseSchemaPath(state_);
        } else {
            error("Expected PATH after SET");
        }
    } else {
        error("Expected RENAME TO or OWNER TO after schema name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

AlterDatabaseStmt* Parser::parseAlterDatabase() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AlterDatabaseStmt>();
    stmt->database_path = parseSchemaPath(state_);
    if (stmt->database_path.isEmpty()) {
        error("Expected database name");
    }

    if (matchContextual("RENAME")) {
        expectContextual("TO", "Expected TO after RENAME");
        stmt->action = AlterDatabaseAction::RENAME;
        stmt->new_name = expectIdentifier("Expected new database name");
    } else if (matchContextual("OWNER")) {
        expectContextual("TO", "Expected TO after OWNER");
        stmt->action = AlterDatabaseAction::SET_OWNER;
        stmt->owner = expectIdentifier("Expected owner name");
    } else if (matchContextual("ALIAS")) {
        if (matchContextual("ADD")) {
            stmt->action = AlterDatabaseAction::ADD_ALIAS;
            stmt->alias = expectIdentifier("Expected alias name");
        } else if (matchContextual("DROP")) {
            stmt->action = AlterDatabaseAction::DROP_ALIAS;
            stmt->alias = expectIdentifier("Expected alias name");
        } else {
            error("Expected ADD or DROP after ALIAS");
        }
    } else {
        error("Expected RENAME TO, OWNER TO, or ALIAS ADD/DROP after database name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

AlterDomainStmt* Parser::parseAlterDomain() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AlterDomainStmt>();
    stmt->domain_path = parseSchemaPath(state_);
    if (stmt->domain_path.isEmpty()) {
        error("Expected domain name");
    }

    if (match(TokenType::KW_SET)) {
        if (match(TokenType::KW_DEFAULT)) {
            stmt->action = AlterDomainAction::SET_DEFAULT;
            stmt->value = extractExpressionText(parseExpression());
        } else if (matchContextual("COMPAT")) {
            stmt->action = AlterDomainAction::SET_COMPAT;
            if (check(TokenType::STRING_LITERAL)) {
                stmt->value = std::string(stringPool().get(current().value.string_id));
                advance();
            } else {
                stmt->value = std::string(stringPool().get(expectIdentifier("Expected compat name")));
            }
        } else {
            error("Expected DEFAULT or COMPAT after SET");
        }
    } else if (match(TokenType::KW_DROP)) {
        if (match(TokenType::KW_DEFAULT)) {
            stmt->action = AlterDomainAction::DROP_DEFAULT;
        } else if (matchContextual("CONSTRAINT")) {
            stmt->action = AlterDomainAction::DROP_CONSTRAINT;
            stmt->constraint_name = expectIdentifier("Expected constraint name");
        } else if (matchContextual("COMPAT")) {
            stmt->action = AlterDomainAction::DROP_COMPAT;
        } else {
            error("Expected DEFAULT, CONSTRAINT, or COMPAT after DROP");
        }
    } else if (matchContextual("ADD")) {
        if (matchContextual("CHECK")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
            stmt->action = AlterDomainAction::ADD_CHECK;
            stmt->value = extractExpressionText(parseExpression());
            expect(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
        } else {
            error("Expected CHECK after ADD");
        }
    } else if (matchContextual("RENAME")) {
        expectContextual("TO", "Expected TO after RENAME");
        stmt->action = AlterDomainAction::RENAME;
        stmt->new_name = expectIdentifier("Expected new domain name");
    } else {
        error("Expected SET, DROP, ADD, or RENAME after domain name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

AlterTableStmt* Parser::parseAlterTable() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AlterTableStmt>();

    // IF EXISTS (IF, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    // ONLY
    if (checkContextual("ONLY")) {
        matchContextual("ONLY");
        stmt->only = true;
    }

    // Table name
    stmt->table_path = parseSchemaPath(state_);

    // Action
    if (matchContextual("ADD")) {
        if (matchContextual("COLUMN")) {
            stmt->action = AlterTableAction::ADD_COLUMN;
            stmt->column = parseColumnDef();
        } else if (matchContextual("CONSTRAINT")) {
            stmt->action = AlterTableAction::ADD_CONSTRAINT;
            // Parse constraint inline (don't re-read CONSTRAINT keyword)
            auto* constraint = arena_.create<TableConstraint>();
            constraint->name = expectIdentifier("Expected constraint name");

            // Constraint type
            if (matchContextual("PRIMARY")) {
                expectContextual("KEY", "Expected KEY after PRIMARY");
                parsePrimaryKeyConstraint(constraint);
            } else if (matchContextual("UNIQUE")) {
                parseUniqueConstraint(constraint);
            } else if (matchContextual("FOREIGN")) {
                expectContextual("KEY", "Expected KEY after FOREIGN");
                parseForeignKeyConstraint(constraint);
            } else if (matchContextual("CHECK")) {
                parseCheckConstraint(constraint);
            }
            stmt->constraint = constraint;
        } else {
            // Assume ADD COLUMN without COLUMN keyword
            stmt->action = AlterTableAction::ADD_COLUMN;
            stmt->column = parseColumnDef();
        }
    } else if (match(TokenType::KW_DROP)) {
        // DROP is a Gatekeeper keyword
        if (matchContextual("COLUMN")) {
            stmt->action = AlterTableAction::DROP_COLUMN;
            stmt->column_name = expectIdentifier("Expected column name");
            if (matchContextual("CASCADE")) stmt->cascade = true;
        } else if (matchContextual("CONSTRAINT")) {
            stmt->action = AlterTableAction::DROP_CONSTRAINT;
            stmt->constraint_name = expectIdentifier("Expected constraint name");
            if (matchContextual("CASCADE")) stmt->cascade = true;
        } else {
            // DROP without COLUMN/CONSTRAINT - assume DROP COLUMN
            stmt->action = AlterTableAction::DROP_COLUMN;
            stmt->column_name = expectIdentifier("Expected column name");
            if (matchContextual("CASCADE")) stmt->cascade = true;
        }
    } else if (matchContextual("RENAME")) {
        if (matchContextual("COLUMN")) {
            stmt->action = AlterTableAction::RENAME_COLUMN;
            stmt->column_name = expectIdentifier("Expected column name");
            expectContextual("TO", "Expected TO after column name");
            stmt->new_name = expectIdentifier("Expected new column name");
        } else if (matchContextual("CONSTRAINT")) {
            stmt->action = AlterTableAction::RENAME_CONSTRAINT;
            stmt->constraint_name = expectIdentifier("Expected constraint name");
            expectContextual("TO", "Expected TO after constraint name");
            stmt->new_name = expectIdentifier("Expected new constraint name");
        } else if (matchContextual("TO")) {
            stmt->action = AlterTableAction::RENAME_TABLE;
            stmt->new_name = expectIdentifier("Expected new table name");
        }
    } else if (match(TokenType::KW_SET)) {
        if (matchContextual("TABLESPACE")) {
            stmt->action = AlterTableAction::SET_TABLESPACE;
            stmt->tablespace = parseSchemaPath(state_);
        } else if (matchContextual("SCHEMA")) {
            stmt->action = AlterTableAction::SET_SCHEMA;
            stmt->target_schema = parseSchemaPath(state_);
        }
    } else if (matchContextual("ENABLE")) {
        expectContextual("ROW", "Expected ROW after ENABLE");
        expectContextual("LEVEL", "Expected LEVEL after ROW");
        expectContextual("SECURITY", "Expected SECURITY after LEVEL");
        stmt->action = AlterTableAction::ENABLE_RLS;
    } else if (matchContextual("DISABLE")) {
        expectContextual("ROW", "Expected ROW after DISABLE");
        expectContextual("LEVEL", "Expected LEVEL after ROW");
        expectContextual("SECURITY", "Expected SECURITY after LEVEL");
        stmt->action = AlterTableAction::DISABLE_RLS;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// DROP Statements
// =============================================================================

Statement* Parser::parseDrop() {
    ParseModeGuard guard(state_, ParseMode::DDL);

    if (matchContextual("SCHEMA")) return parseDropSchema();
    if (matchContextual("DATABASE")) return parseDropDatabase();
    if (matchContextual("TABLE")) return parseDropTable();
    if (matchContextual("INDEX")) return parseDropIndex();
    if (matchContextual("VIEW")) return parseDropView();
    if (matchContextual("DOMAIN")) return parseDropDomain();
    if (matchContextual("FUNCTION")) return parseDropFunction();
    if (matchContextual("PROCEDURE")) return parseDropProcedure();
    if (matchContextual("TRIGGER")) return parseDropTrigger();
    if (matchContextual("PACKAGE")) return parseDropPackage();
    if (matchContextual("ROLE")) return parseDropRole();
    if (matchContextual("EXCEPTION")) return parseDropException();
    if (matchContextual("MATERIALIZED")) {
        expectContextual("VIEW", "Expected VIEW after MATERIALIZED");
        auto* stmt = parseDropView();
        if (stmt) stmt->materialized = true;
        return stmt;
    }

    error("Expected object type after DROP");
    return nullptr;
}

// =============================================================================
// DROP SCHEMA
// =============================================================================

DropSchemaStmt* Parser::parseDropSchema() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropSchemaStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->schemas.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) stmt->cascade = true;
    else if (matchContextual("RESTRICT")) stmt->restrict = true;

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// DROP DATABASE
// =============================================================================

DropDatabaseStmt* Parser::parseDropDatabase() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropDatabaseStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    stmt->database_path = parseSchemaPath(state_);
    if (stmt->database_path.isEmpty()) {
        error("Expected database name");
    }

    if (matchContextual("CASCADE") || matchContextual("FORCE")) {
        stmt->force = true;
    } else if (matchContextual("RESTRICT")) {
        stmt->force = false;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// DROP DOMAIN
// =============================================================================

DropDomainStmt* Parser::parseDropDomain() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropDomainStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->domains.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) {
        error("DROP DOMAIN does not support CASCADE");
    } else if (matchContextual("RESTRICT")) {
        stmt->restrict = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropTableStmt* Parser::parseDropTable() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropTableStmt>();

    // IF EXISTS (IF, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    // Table names
    do {
        stmt->tables.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    // CASCADE / RESTRICT
    if (matchContextual("CASCADE")) stmt->cascade = true;
    else if (matchContextual("RESTRICT")) stmt->restrict = true;

    stmt->span = makeSpan(start);
    return stmt;
}

DropIndexStmt* Parser::parseDropIndex() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropIndexStmt>();

    // CONCURRENTLY
    if (matchContextual("CONCURRENTLY")) {
        stmt->concurrent = true;
    }

    // IF EXISTS (IF, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    // Index names
    do {
        stmt->indexes.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    // CASCADE
    if (matchContextual("CASCADE")) stmt->cascade = true;

    stmt->span = makeSpan(start);
    return stmt;
}

DropViewStmt* Parser::parseDropView() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropViewStmt>();

    // IF EXISTS (IF, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    // View names
    do {
        stmt->views.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    // CASCADE
    if (matchContextual("CASCADE")) stmt->cascade = true;

    stmt->span = makeSpan(start);
    return stmt;
}

DropFunctionStmt* Parser::parseDropFunction() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropFunctionStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->functions.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    stmt->span = makeSpan(start);
    return stmt;
}

DropProcedureStmt* Parser::parseDropProcedure() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropProcedureStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->procedures.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    stmt->span = makeSpan(start);
    return stmt;
}

DropTriggerStmt* Parser::parseDropTrigger() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropTriggerStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->triggers.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    stmt->span = makeSpan(start);
    return stmt;
}

DropPackageStmt* Parser::parseDropPackage() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropPackageStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->packages.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    stmt->span = makeSpan(start);
    return stmt;
}

DropRoleStmt* Parser::parseDropRole() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropRoleStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->roles.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropExceptionStmt* Parser::parseDropException() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropExceptionStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->exceptions.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// TRUNCATE Statement
// =============================================================================

Statement* Parser::parseTruncate() {
    ParseModeGuard guard(state_, ParseMode::DDL);

    if (matchContextual("TABLE")) {
        return parseTruncateTable();
    }

    // TRUNCATE without TABLE keyword
    return parseTruncateTable();
}

TruncateTableStmt* Parser::parseTruncateTable() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<TruncateTableStmt>();

    // Table names
    do {
        stmt->tables.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    // Options
    while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        if (matchContextual("RESTART")) {
            expectContextual("IDENTITY", "Expected IDENTITY after RESTART");
            stmt->restart_identity = true;
        } else if (matchContextual("CONTINUE")) {
            expectContextual("IDENTITY", "Expected IDENTITY after CONTINUE");
            stmt->continue_identity = true;
        } else if (matchContextual("CASCADE")) {
            stmt->cascade = true;
        } else {
            break;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// WITH Clause
// =============================================================================

Statement* Parser::parseWithStatement() {
    WithClause* with = parseWithClause();
    if (!with) {
        return nullptr;
    }

    if (match(TokenType::KW_SELECT)) {
        auto* stmt = parseSelect();
        stmt->with = with;
        return stmt;
    }
    if (match(TokenType::KW_INSERT)) {
        auto* stmt = parseInsert();
        stmt->with = with;
        return stmt;
    }
    if (match(TokenType::KW_UPDATE)) {
        auto* stmt = parseUpdate();
        stmt->with = with;
        return stmt;
    }
    if (match(TokenType::KW_DELETE)) {
        auto* stmt = parseDelete();
        stmt->with = with;
        return stmt;
    }

    error("Expected SELECT, INSERT, UPDATE, or DELETE after WITH clause");
    return nullptr;
}

WithClause* Parser::parseWithClause() {
    ParseModeGuard guard(state_, ParseMode::WITH_CLAUSE);

    if (!match(TokenType::KW_WITH)) {
        error("Expected WITH");
        return nullptr;
    }

    auto* with = arena_.create<WithClause>();

    if (matchContextual("RECURSIVE")) {
        with->recursive = true;
    }

    do {
        CTE cte;
        cte.name = expectIdentifier("Expected CTE name");

        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    cte.column_names.push_back(expectIdentifier("Expected CTE column name"));
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after CTE column list");
        }

        if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
            error("Expected AS in CTE definition");
        }

        if (matchContextual("MATERIALIZED")) {
            cte.materialized = true;
        } else if (matchContextual("NOT")) {
            expectContextual("MATERIALIZED", "Expected MATERIALIZED after NOT");
            cte.not_materialized = true;
        }

        expect(TokenType::LEFT_PAREN, "Expected '(' before CTE query");

        if (!match(TokenType::KW_SELECT)) {
            error("Expected SELECT in CTE query");
            synchronize();
        } else {
            cte.query = parseSelect();
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after CTE query");
        cte.recursive = with->recursive;

        with->ctes.push_back(std::move(cte));
    } while (match(TokenType::COMMA));

    return with;
}

SelectStmt* Parser::parseSelectWithClause() {
    WithClause* with = nullptr;
    if (check(TokenType::KW_WITH)) {
        with = parseWithClause();
    }

    if (!match(TokenType::KW_SELECT)) {
        error("Expected SELECT");
        return nullptr;
    }

    auto* stmt = parseSelect();
    stmt->with = with;
    return stmt;
}

// =============================================================================
// SELECT Statement
// =============================================================================

SelectStmt* Parser::parseSelect() {
    SourceLocation start = currentLocation();
    ParseModeGuard guard(state_, ParseMode::DML_SELECT);

    auto* stmt = arena_.create<SelectStmt>();

    // DISTINCT or ALL
    if (matchContextual("DISTINCT")) {
        stmt->distinct = true;
    } else if (matchContextual("ALL")) {
        stmt->all = true;
    }

    // Parse select list
    parseSelectList(stmt);

    // FROM clause (optional for SELECT without tables)
    if (match(TokenType::KW_FROM)) {
        parseFromClause(stmt);
    }

    // WHERE clause
    if (match(TokenType::KW_WHERE)) {
        parseWhereClause(stmt);
    }

    // GROUP BY clause
    if (match(TokenType::KW_GROUP)) {
        expectContextual("BY", "Expected BY after GROUP");
        parseGroupByClause(stmt);
    }

    // HAVING clause
    if (match(TokenType::KW_HAVING)) {
        parseHavingClause(stmt);
    }

    // ORDER BY clause (ORDER is a Gatekeeper keyword)
    if (match(TokenType::KW_ORDER)) {
        expectContextual("BY", "Expected BY after ORDER");
        parseOrderByClause(stmt);
    }

    // LIMIT/OFFSET clause (LIMIT is a Gatekeeper keyword)
    if (match(TokenType::KW_LIMIT)) {
        parseLimitClause(stmt);
    }

    // OFFSET without LIMIT (PostgreSQL style)
    if (matchContextual("OFFSET")) {
        stmt->offset = parseExpression();
        if (matchContextual("ROWS") || matchContextual("ROW")) {
            // Optional ROWS/ROW keyword
        }
    }

    // FETCH clause (SQL:2008 standard)
    if (matchContextual("FETCH")) {
        if (matchContextual("FIRST") || matchContextual("NEXT")) {
            if (!checkContextual("ROW") && !checkContextual("ROWS")) {
                stmt->limit = parseExpression();
            }
            if (matchContextual("ROW") || matchContextual("ROWS")) {
                expectContextual("ONLY", "Expected ONLY after ROW/ROWS");
            }
        }
    }

    // Set operations (UNION, INTERSECT, EXCEPT)
    parseSetOperation(stmt);

    // FOR UPDATE/SHARE (FOR is contextual)
    if (matchContextual("FOR")) {
        if (match(TokenType::KW_UPDATE)) {
            stmt->for_update = true;
        } else if (matchContextual("SHARE")) {
            stmt->for_share = true;
        } else if (matchContextual("NO")) {
            // FOR NO KEY UPDATE - treat as FOR UPDATE
            expectContextual("KEY", "Expected KEY after NO");
            if (match(TokenType::KW_UPDATE)) {
                stmt->for_update = true;
            }
        } else if (matchContextual("KEY")) {
            // FOR KEY SHARE - treat as FOR SHARE
            expectContextual("SHARE", "Expected SHARE after KEY");
            stmt->for_share = true;
        }

        // OF table_name - skip tables, AST doesn't support this
        if (matchContextual("OF")) {
            do {
                parseSchemaPath(state_);  // Parse but ignore
            } while (match(TokenType::COMMA));
        }

        // NOWAIT / SKIP LOCKED
        if (matchContextual("NOWAIT")) {
            stmt->nowait = true;
        } else if (matchContextual("SKIP")) {
            expectContextual("LOCKED", "Expected LOCKED after SKIP");
            stmt->skip_locked = true;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

void Parser::parseSelectList(SelectStmt* stmt) {
    do {
        SelectItem* item = parseSelectItem();
        if (item) {
            stmt->items.push_back(item);
        }
    } while (match(TokenType::COMMA));
}

SelectItem* Parser::parseSelectItem() {
    auto* item = arena_.create<SelectItem>();
    SourceLocation start = currentLocation();

    // Check for * (select all)
    if (match(TokenType::STAR)) {
        item->item_type = SelectItem::Type::STAR;
        item->span = makeSpan(start);
        return item;
    }

    // Check for table.* (qualified star)
    if (isIdentifier()) {
        SchemaPath path = parseSchemaPath(state_);
        bool saw_trailing_dot = (previous().type == TokenType::DOT);

        if (saw_trailing_dot && match(TokenType::STAR)) {
            item->item_type = SelectItem::Type::TABLE_STAR;
            item->table_path = std::move(path);
            item->span = makeSpan(start);
            return item;
        }

        item->item_type = SelectItem::Type::EXPRESSION;
        Expression* left = nullptr;
        if (check(TokenType::LEFT_PAREN)) {
            left = parseFunctionCall(std::move(path));
        } else {
            auto* colRef = arena_.create<ColumnRefExpr>();
            if (path.components.size() == 1) {
                colRef->column.column_name = path.components[0];
            } else {
                colRef->column.column_name = path.objectName();
                colRef->column.has_table_qualifier = true;
                colRef->column.table_path.type = path.type;
                colRef->column.table_path.components = path.schemaComponents();
            }
            left = colRef;
        }
        item->expr = parseExpressionWithLeft(left);
    } else {
        // Parse expression
        item->item_type = SelectItem::Type::EXPRESSION;
        item->expr = parseExpression();
    }

    // Optional alias
    if (match(TokenType::KW_AS)) {
        item->alias = expectIdentifier("Expected alias after AS");
        item->has_alias = true;
    } else if (isIdentifier() && !check(TokenType::KW_FROM) && !check(TokenType::COMMA)) {
        // Alias without AS keyword
        item->alias = currentIdentifier();
        item->has_alias = true;
    }

    item->span = makeSpan(start);
    return item;
}

void Parser::parseFromClause(SelectStmt* stmt) {
    ParseModeGuard guard(state_, ParseMode::TABLE_REF);

    // Parse first table reference
    stmt->from = parseTableRef();

    // Parse joins
    while (true) {
        // Check for join keywords
        if (check(TokenType::KW_JOIN) ||
            check(TokenType::KW_ON) ||
            checkContextual("LEFT") ||
            checkContextual("RIGHT") ||
            checkContextual("INNER") ||
            checkContextual("OUTER") ||
            checkContextual("FULL") ||
            checkContextual("CROSS") ||
            checkContextual("NATURAL")) {

            JoinNode* join = parseJoin(stmt->from);
            if (join) {
                stmt->joins.push_back(join);
            }
        } else if (match(TokenType::COMMA)) {
            // Cross join (implicit)
            auto* join = arena_.create<JoinNode>();
            join->join_type = JoinType::CROSS;
            join->right = parseTableRef();
            stmt->joins.push_back(join);
        } else {
            break;
        }
    }
}

TableRefNode* Parser::parseTableRef() {
    auto* node = arena_.create<TableRefNode>();
    SourceLocation start = currentLocation();

    // Check for subquery
    if (check(TokenType::LEFT_PAREN)) {
        advance();  // consume (

        // Check if it's a SELECT/CTE (subquery) or just a grouped table reference
        if (check(TokenType::KW_SELECT) || check(TokenType::KW_WITH)) {
            node->ref_type = TableRefNode::Type::SUBQUERY;
            node->subquery = parseSelectWithClause();
        } else {
            // Could be a nested table reference - parse as expression for now
            // and let it be resolved later
            node->ref_type = TableRefNode::Type::TABLE;
            node->table_path = parseSchemaPath(state_);
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after subquery");
    } else if (matchContextual("LATERAL")) {
        // LATERAL subquery
        expect(TokenType::LEFT_PAREN, "Expected '(' after LATERAL");
        if (check(TokenType::KW_SELECT) || check(TokenType::KW_WITH)) {
            node->ref_type = TableRefNode::Type::SUBQUERY;
            node->subquery = parseSelectWithClause();
        } else {
            error("Expected SELECT after LATERAL");
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after LATERAL subquery");
    } else {
        // Table name or function
        node->table_path = parseSchemaPath(state_);

        // Check for function call syntax
        if (check(TokenType::LEFT_PAREN)) {
            node->ref_type = TableRefNode::Type::FUNCTION;
            auto* funcExpr = dynamic_cast<FunctionCallExpr*>(parseFunctionCall(std::move(node->table_path)));
            node->function = funcExpr;
        } else {
            node->ref_type = TableRefNode::Type::TABLE;
        }
    }

    // Optional alias
    if (match(TokenType::KW_AS)) {
        node->alias = expectIdentifier("Expected alias after AS");
        node->has_alias = true;
    } else if (isIdentifier() &&
               !check(TokenType::KW_WHERE) &&
               !check(TokenType::KW_JOIN) &&
               !check(TokenType::KW_ON) &&
               !check(TokenType::KW_GROUP) &&
               !check(TokenType::KW_ORDER) &&
               !check(TokenType::KW_HAVING) &&
               !check(TokenType::KW_LIMIT) &&
               !check(TokenType::KW_UNION) &&
               !check(TokenType::KW_INTERSECT) &&
               !check(TokenType::KW_EXCEPT) &&
               !check(TokenType::COMMA) &&
               !checkContextual("LEFT") &&
               !checkContextual("RIGHT") &&
               !checkContextual("INNER") &&
               !checkContextual("OUTER") &&
               !checkContextual("FULL") &&
               !checkContextual("CROSS") &&
               !checkContextual("NATURAL") &&
               !checkContextual("FOR") &&
               !checkContextual("OFFSET") &&
               !checkContextual("FETCH") &&
               !checkContextual("RETURNING")) {
        // Alias without AS keyword
        node->alias = currentIdentifier();
        node->has_alias = true;
    }

    // Optional column aliases: table AS alias(col1, col2)
    if (node->has_alias && check(TokenType::LEFT_PAREN)) {
        advance();
        do {
            node->column_aliases.push_back(expectIdentifier("Expected column alias"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after column aliases");
    }

    node->span = makeSpan(start);
    return node;
}

JoinType Parser::parseJoinType() {
    bool natural = false;
    if (matchContextual("NATURAL")) {
        natural = true;
    }

    JoinType type = JoinType::INNER;

    if (matchContextual("LEFT")) {
        matchContextual("OUTER");  // optional
        type = natural ? JoinType::NATURAL_LEFT : JoinType::LEFT;
    } else if (matchContextual("RIGHT")) {
        matchContextual("OUTER");  // optional
        type = natural ? JoinType::NATURAL_RIGHT : JoinType::RIGHT;
    } else if (matchContextual("FULL")) {
        matchContextual("OUTER");  // optional
        type = natural ? JoinType::NATURAL_FULL : JoinType::FULL;
    } else if (matchContextual("CROSS")) {
        type = JoinType::CROSS;
    } else if (matchContextual("INNER")) {
        type = natural ? JoinType::NATURAL : JoinType::INNER;
    } else if (natural) {
        type = JoinType::NATURAL;
    }

    return type;
}

JoinNode* Parser::parseJoin(TableRefNode* left) {
    auto* join = arena_.create<JoinNode>();
    SourceLocation start = currentLocation();

    join->left = left;
    join->join_type = parseJoinType();

    // JOIN keyword
    expect(TokenType::KW_JOIN, "Expected JOIN");

    // Right table
    join->right = parseTableRef();

    // ON or USING clause (not for CROSS or NATURAL joins)
    if (join->join_type != JoinType::CROSS &&
        join->join_type != JoinType::NATURAL &&
        join->join_type != JoinType::NATURAL_LEFT &&
        join->join_type != JoinType::NATURAL_RIGHT &&
        join->join_type != JoinType::NATURAL_FULL) {

        if (match(TokenType::KW_ON)) {
            join->on_condition = parseExpression();
        } else if (match(TokenType::KW_USING)) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after USING");
            do {
                join->using_columns.push_back(expectIdentifier("Expected column name"));
            } while (match(TokenType::COMMA));
            join->has_using = true;
            expect(TokenType::RIGHT_PAREN, "Expected ')' after USING columns");
        }
    }

    join->span = makeSpan(start);
    return join;
}

void Parser::parseWhereClause(SelectStmt* stmt) {
    ParseModeGuard guard(state_, ParseMode::EXPRESSION);
    stmt->where = parseExpression();
}

void Parser::parseGroupByClause(SelectStmt* stmt) {
    if (matchContextual("ROLLUP")) {
        stmt->grouping_type = GroupingType::ROLLUP;
        expect(TokenType::LEFT_PAREN, "Expected '(' after ROLLUP");
        do {
            stmt->group_by.push_back(parseExpression());
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after ROLLUP list");
        return;
    }

    if (matchContextual("CUBE")) {
        stmt->grouping_type = GroupingType::CUBE;
        expect(TokenType::LEFT_PAREN, "Expected '(' after CUBE");
        do {
            stmt->group_by.push_back(parseExpression());
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after CUBE list");
        return;
    }

    if (matchContextual("GROUPING")) {
        if (!matchContextual("SETS")) {
            error("Expected SETS after GROUPING");
        }
        stmt->grouping_type = GroupingType::GROUPING_SETS;
        expect(TokenType::LEFT_PAREN, "Expected '(' after GROUPING SETS");

        do {
            std::vector<Expression*> grouping_set;
            if (match(TokenType::LEFT_PAREN)) {
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        grouping_set.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                }
                expect(TokenType::RIGHT_PAREN, "Expected ')' after grouping set");
            } else {
                grouping_set.push_back(parseExpression());
            }
            stmt->grouping_sets.push_back(std::move(grouping_set));
        } while (match(TokenType::COMMA));

        expect(TokenType::RIGHT_PAREN, "Expected ')' after GROUPING SETS list");
        return;
    }

    do {
        stmt->group_by.push_back(parseExpression());
    } while (match(TokenType::COMMA));
}

void Parser::parseHavingClause(SelectStmt* stmt) {
    ParseModeGuard guard(state_, ParseMode::EXPRESSION);
    stmt->having = parseExpression();
}

void Parser::parseOrderByClause(SelectStmt* stmt) {
    do {
        OrderByItem* item = parseOrderByItem();
        if (item) {
            stmt->order_by.push_back(item);
        }
    } while (match(TokenType::COMMA));
}

OrderByItem* Parser::parseOrderByItem() {
    auto* item = arena_.create<OrderByItem>();
    SourceLocation start = currentLocation();

    item->expr = parseExpression();

    // ASC or DESC
    if (matchContextual("ASC")) {
        item->ascending = true;
    } else if (matchContextual("DESC")) {
        item->ascending = false;
    }

    // NULLS FIRST or NULLS LAST
    if (matchContextual("NULLS")) {
        if (matchContextual("FIRST")) {
            item->nulls_first = true;
        } else if (matchContextual("LAST")) {
            item->nulls_last = true;
        }
    }

    item->span = makeSpan(start);
    return item;
}

void Parser::parseLimitClause(SelectStmt* stmt) {
    // LIMIT ALL or LIMIT expression
    if (matchContextual("ALL")) {
        // LIMIT ALL means no limit
    } else {
        stmt->limit = parseExpression();
    }

    // Optional OFFSET (OFFSET is a Gatekeeper keyword)
    if (match(TokenType::KW_OFFSET)) {
        stmt->offset = parseExpression();
    }
}

void Parser::parseSetOperation(SelectStmt* stmt) {
    SetOpType op = SetOpType::NONE;

    if (match(TokenType::KW_UNION)) {
        op = SetOpType::UNION;
    } else if (match(TokenType::KW_INTERSECT)) {
        op = SetOpType::INTERSECT;
    } else if (match(TokenType::KW_EXCEPT)) {
        op = SetOpType::EXCEPT;
    } else {
        return;
    }

    stmt->set_op = op;

    // ALL modifier
    if (matchContextual("ALL")) {
        stmt->set_op_all = true;
    }

    // Parse right side SELECT
    if (match(TokenType::KW_SELECT)) {
        stmt->set_op_right = parseSelect();
    } else if (check(TokenType::LEFT_PAREN)) {
        // Parenthesized SELECT
        advance();
        if (match(TokenType::KW_SELECT)) {
            stmt->set_op_right = parseSelect();
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after SELECT");
    } else {
        error("Expected SELECT after set operation");
    }
}

// =============================================================================
// INSERT Statement
// =============================================================================

InsertStmt* Parser::parseInsert() {
    SourceLocation start = currentLocation();
    ParseModeGuard guard(state_, ParseMode::DML_INSERT);

    auto* stmt = arena_.create<InsertStmt>();

    // INTO (optional in some databases)
    match(TokenType::KW_INTO);

    // Table name
    stmt->table_path = parseSchemaPath(state_);

    // Optional alias
    if (match(TokenType::KW_AS)) {
        stmt->alias = expectIdentifier("Expected alias after AS");
        stmt->has_alias = true;
    } else if (isIdentifier() && !check(TokenType::LEFT_PAREN) &&
               !check(TokenType::KW_VALUES) && !check(TokenType::KW_SELECT) &&
               !check(TokenType::KW_DEFAULT)) {
        stmt->alias = currentIdentifier();
        stmt->has_alias = true;
    }

    // Column list (optional)
    if (check(TokenType::LEFT_PAREN) && !check(TokenType::KW_SELECT)) {
        parseInsertColumns(stmt);
    }

    // VALUES, SELECT, or DEFAULT VALUES
    if (match(TokenType::KW_VALUES)) {
        stmt->source = InsertStmt::Source::VALUES;
        parseValuesClause(stmt);
    } else if (match(TokenType::KW_SELECT)) {
        stmt->source = InsertStmt::Source::SELECT;
        stmt->select_source = parseSelect();
    } else if (match(TokenType::KW_DEFAULT)) {
        expect(TokenType::KW_VALUES, "Expected VALUES after DEFAULT");
        stmt->source = InsertStmt::Source::DEFAULT;
    }

    // ON CONFLICT clause
    if (match(TokenType::KW_ON)) {
        if (matchContextual("CONFLICT")) {
            parseOnConflict(stmt);
        }
    }

    // RETURNING clause (RETURNING is contextual)
    if (matchContextual("RETURNING")) {
        parseReturningClause(stmt->returning);
    }

    stmt->span = makeSpan(start);
    return stmt;
}

void Parser::parseInsertColumns(InsertStmt* stmt) {
    expect(TokenType::LEFT_PAREN, "Expected '(' before column list");
    do {
        stmt->columns.push_back(expectIdentifier("Expected column name"));
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
}

void Parser::parseValuesClause(InsertStmt* stmt) {
    // Parse multiple value rows
    do {
        std::vector<Expression*> row;
        expect(TokenType::LEFT_PAREN, "Expected '(' before values");
        do {
            if (match(TokenType::KW_DEFAULT)) {
                // DEFAULT value
                auto* expr = arena_.create<LiteralExpr>();
                expr->literal_type = LiteralType::DEFAULT;
                row.push_back(expr);
            } else {
                row.push_back(parseExpression());
            }
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after values");
        stmt->values_rows.push_back(std::move(row));
    } while (match(TokenType::COMMA));
}

void Parser::parseOnConflict(InsertStmt* stmt) {
    stmt->on_conflict = arena_.create<OnConflictClause>();

    // Conflict target (optional)
    if (check(TokenType::LEFT_PAREN)) {
        advance();
        do {
            stmt->on_conflict->columns.push_back(expectIdentifier("Expected column name"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after conflict columns");
    } else if (match(TokenType::KW_ON)) {
        expectContextual("CONSTRAINT", "Expected CONSTRAINT after ON");
        stmt->on_conflict->constraint_name = expectIdentifier("Expected constraint name");
    }

    // WHERE clause for partial index
    if (match(TokenType::KW_WHERE)) {
        stmt->on_conflict->where_target = parseExpression();
    }

    // DO action
    expectContextual("DO", "Expected DO after conflict target");

    if (matchContextual("NOTHING")) {
        stmt->on_conflict->action = ConflictAction::NOTHING;
    } else if (match(TokenType::KW_UPDATE)) {
        stmt->on_conflict->action = ConflictAction::UPDATE;
        expect(TokenType::KW_SET, "Expected SET after UPDATE");

        // Parse SET assignments
        do {
            StringPool::StringId column = expectIdentifier("Expected column name");
            expect(TokenType::EQUAL, "Expected '=' in SET clause");
            Expression* value = nullptr;
            if (matchContextual("EXCLUDED")) {
                expect(TokenType::DOT, "Expected '.' after EXCLUDED");
                // Create column reference to EXCLUDED.column
                auto* ref = arena_.create<ColumnRefExpr>();
                ref->column.column_name = expectIdentifier("Expected column name");
                ref->column.has_table_qualifier = true;
                // Use "EXCLUDED" as qualifier
                value = ref;
            } else {
                value = parseExpression();
            }
            stmt->on_conflict->set_items.push_back({column, value});
        } while (match(TokenType::COMMA));

        // Optional WHERE clause for UPDATE
        if (match(TokenType::KW_WHERE)) {
            stmt->on_conflict->where_action = parseExpression();
        }
    }
}

void Parser::parseReturningClause(std::vector<SelectItem*>& returning) {
    do {
        SelectItem* item = parseSelectItem();
        if (item) {
            returning.push_back(item);
        }
    } while (match(TokenType::COMMA));
}

// =============================================================================
// UPDATE Statement
// =============================================================================

UpdateStmt* Parser::parseUpdate() {
    SourceLocation start = currentLocation();
    ParseModeGuard guard(state_, ParseMode::DML_UPDATE);

    auto* stmt = arena_.create<UpdateStmt>();

    // ONLY modifier (contextual keyword)
    // Note: AST doesn't have 'only' field, skip for now

    // Table name
    stmt->table_path = parseSchemaPath(state_);

    // Optional alias
    if (match(TokenType::KW_AS)) {
        stmt->alias = expectIdentifier("Expected alias after AS");
        stmt->has_alias = true;
    } else if (isIdentifier() && !check(TokenType::KW_SET)) {
        stmt->alias = currentIdentifier();
        stmt->has_alias = true;
    }

    // SET clause
    expect(TokenType::KW_SET, "Expected SET in UPDATE statement");
    parseSetClause(stmt);

    // FROM clause (PostgreSQL extension)
    if (match(TokenType::KW_FROM)) {
        ParseModeGuard fromGuard(state_, ParseMode::TABLE_REF);
        stmt->from = parseTableRef();

        // Parse joins in FROM clause
        while (check(TokenType::KW_JOIN) ||
               checkContextual("LEFT") ||
               checkContextual("RIGHT") ||
               checkContextual("INNER") ||
               checkContextual("FULL") ||
               checkContextual("CROSS") ||
               checkContextual("NATURAL")) {
            JoinNode* join = parseJoin(stmt->from);
            if (join) {
                stmt->joins.push_back(join);
            }
        }
    }

    // WHERE clause
    if (match(TokenType::KW_WHERE)) {
        ParseModeGuard whereGuard(state_, ParseMode::EXPRESSION);
        // WHERE CURRENT OF is for cursors - not handling for now
        stmt->where = parseExpression();
    }

    // RETURNING clause (RETURNING is contextual)
    if (matchContextual("RETURNING")) {
        parseReturningClause(stmt->returning);
    }

    stmt->span = makeSpan(start);
    return stmt;
}

void Parser::parseSetClause(UpdateStmt* stmt) {
    do {
        // Single column assignment: col = expr
        StringPool::StringId column = expectIdentifier("Expected column name");
        expect(TokenType::EQUAL, "Expected '=' in SET clause");

        Expression* value = nullptr;
        if (match(TokenType::KW_DEFAULT)) {
            auto* expr = arena_.create<LiteralExpr>();
            expr->literal_type = LiteralType::DEFAULT;
            value = expr;
        } else {
            value = parseExpression();
        }

        stmt->set_items.push_back({column, value});
    } while (match(TokenType::COMMA));
}

// =============================================================================
// DELETE Statement
// =============================================================================

DeleteStmt* Parser::parseDelete() {
    SourceLocation start = currentLocation();
    ParseModeGuard guard(state_, ParseMode::DML_DELETE);

    auto* stmt = arena_.create<DeleteStmt>();

    // FROM keyword (required in standard SQL)
    expect(TokenType::KW_FROM, "Expected FROM in DELETE statement");

    // ONLY modifier (contextual keyword)
    // Note: AST doesn't have 'only' field, skip for now

    // Table name
    stmt->table_path = parseSchemaPath(state_);

    // Optional alias
    if (match(TokenType::KW_AS)) {
        stmt->alias = expectIdentifier("Expected alias after AS");
        stmt->has_alias = true;
    } else if (isIdentifier() &&
               !check(TokenType::KW_WHERE) &&
               !check(TokenType::KW_USING) &&
               !checkContextual("RETURNING")) {
        stmt->alias = currentIdentifier();
        stmt->has_alias = true;
    }

    // USING clause (PostgreSQL extension)
    if (match(TokenType::KW_USING)) {
        ParseModeGuard usingGuard(state_, ParseMode::TABLE_REF);
        stmt->using_clause = parseTableRef();

        // Parse joins in USING clause
        while (check(TokenType::KW_JOIN) ||
               checkContextual("LEFT") ||
               checkContextual("RIGHT") ||
               checkContextual("INNER") ||
               checkContextual("FULL") ||
               checkContextual("CROSS") ||
               checkContextual("NATURAL")) {
            JoinNode* join = parseJoin(stmt->using_clause);
            if (join) {
                stmt->using_joins.push_back(join);
            }
        }
    }

    // WHERE clause
    if (match(TokenType::KW_WHERE)) {
        ParseModeGuard whereGuard(state_, ParseMode::EXPRESSION);
        // WHERE CURRENT OF is for cursors - not handling for now
        stmt->where = parseExpression();
    }

    // RETURNING clause (RETURNING is contextual)
    if (matchContextual("RETURNING")) {
        parseReturningClause(stmt->returning);
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// COPY Statement
// =============================================================================

CopyStmt* Parser::parseCopy() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<CopyStmt>();

    // Spec: docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md <copy_command>

    bool has_query = false;
    if (match(TokenType::LEFT_PAREN)) {
        has_query = true;
        if (!match(TokenType::KW_SELECT)) {
            error("Expected SELECT after '(' in COPY");
            return nullptr;
        }
        stmt->query = parseSelect();
        expect(TokenType::RIGHT_PAREN, "Expected ')' after COPY query");
    } else {
        // Target table
        stmt->table_path = parseSchemaPath(state_);

        // Optional column list
        if (match(TokenType::LEFT_PAREN)) {
            do {
                stmt->columns.push_back(expectIdentifier("Expected column name"));
            } while (match(TokenType::COMMA));
            expect(TokenType::RIGHT_PAREN, "Expected ')' after COPY column list");
        }
    }

    // Direction
    if (match(TokenType::KW_FROM)) {
        stmt->direction = CopyStmt::Direction::FROM;
        if (matchContextual("STDIN")) {
            stmt->target_is_stdin = true;
        } else if (check(TokenType::STRING_LITERAL)) {
            stmt->target = current().value.string_id;
            advance();
        } else {
            error("Expected STDIN or string literal for COPY FROM");
            return nullptr;
        }
    } else if (matchContextual("TO")) {
        stmt->direction = CopyStmt::Direction::TO;
        if (matchContextual("STDOUT")) {
            stmt->target_is_stdout = true;
        } else if (check(TokenType::STRING_LITERAL)) {
            stmt->target = current().value.string_id;
            advance();
        } else {
            error("Expected STDOUT or string literal for COPY TO");
            return nullptr;
        }
    } else {
        error("Expected COPY FROM or COPY TO");
        return nullptr;
    }

    if (has_query && stmt->direction == CopyStmt::Direction::FROM) {
        error("COPY (SELECT ...) only supports TO");
        return nullptr;
    }

    // Optional WITH (...) options
    bool has_with = match(TokenType::KW_WITH);
    if (has_with || check(TokenType::LEFT_PAREN)) {
        if (!match(TokenType::LEFT_PAREN)) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after WITH in COPY");
        }

        auto read_option_value = [&]() -> StringPool::StringId {
            if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
                auto id = current().value.string_id;
                advance();
                return id;
            }
            return StringPool::INVALID_ID;
        };

        auto parse_format = [&](StringPool::StringId id) {
            if (id == StringPool::INVALID_ID) {
                error("Expected COPY FORMAT value");
                return;
            }
            auto text = stringPool().get(id);
            if (caseInsensitiveEquals(text, "CSV")) {
                stmt->options.format = CopyOptions::Format::CSV;
                stmt->options.format_set = true;
            } else if (caseInsensitiveEquals(text, "TEXT")) {
                stmt->options.format = CopyOptions::Format::TEXT;
                stmt->options.format_set = true;
            } else if (caseInsensitiveEquals(text, "BINARY")) {
                stmt->options.format = CopyOptions::Format::BINARY;
                stmt->options.format_set = true;
            } else {
                error("Unsupported COPY FORMAT value");
            }
        };

        while (!check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            if (matchContextual("FORMAT")) {
                auto id = read_option_value();
                if (id == StringPool::INVALID_ID) {
                    error("Expected COPY FORMAT value");
                    break;
                }
                parse_format(id);
            } else if (matchContextual("DELIMITER")) {
                matchContextual("AS");
                if (!check(TokenType::STRING_LITERAL)) {
                    error("Expected string literal for COPY DELIMITER");
                    break;
                }
                stmt->options.delimiter = current().value.string_id;
                stmt->options.delimiter_set = true;
                advance();
            } else if (match(TokenType::KW_NULL)) {
                matchContextual("AS");
                if (!check(TokenType::STRING_LITERAL)) {
                    error("Expected string literal for COPY NULL");
                    break;
                }
                stmt->options.null_string = current().value.string_id;
                stmt->options.null_set = true;
                advance();
            } else if (matchContextual("HEADER")) {
                stmt->options.header = true;
                stmt->options.header_set = true;
                if (match(TokenType::KW_TRUE) || match(TokenType::KW_ON) || matchContextual("ON")) {
                    stmt->options.header = true;
                } else if (match(TokenType::KW_FALSE) || matchContextual("OFF")) {
                    stmt->options.header = false;
                }
            } else if (matchContextual("QUOTE")) {
                if (!check(TokenType::STRING_LITERAL)) {
                    error("Expected string literal for COPY QUOTE");
                    break;
                }
                stmt->options.quote = current().value.string_id;
                stmt->options.quote_set = true;
                advance();
            } else if (matchContextual("ESCAPE")) {
                if (!check(TokenType::STRING_LITERAL)) {
                    error("Expected string literal for COPY ESCAPE");
                    break;
                }
                stmt->options.escape = current().value.string_id;
                stmt->options.escape_set = true;
                advance();
            } else if (matchContextual("ENCODING")) {
                auto id = read_option_value();
                if (id == StringPool::INVALID_ID) {
                    error("Expected COPY ENCODING value");
                    break;
                }
                stmt->options.encoding = id;
                stmt->options.encoding_set = true;
            } else if (matchContextual("CSV") ||
                       matchContextual("TEXT") ||
                       matchContextual("BINARY")) {
                parse_format(previous().value.string_id);
            } else {
                error("Unsupported COPY option");
                break;
            }

            if (!match(TokenType::COMMA)) {
                break;
            }
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after COPY options");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// Additional Expression Parsing for DML
// =============================================================================

Expression* Parser::parseCaseExpr() {
    auto* expr = arena_.create<CaseExpr>();
    SourceLocation start = currentLocation();

    // CASE already consumed

    // Simple CASE (CASE operand WHEN...) or searched CASE (CASE WHEN...)
    if (!check(TokenType::KW_WHEN)) {
        expr->operand = parseExpression();
    }

    // WHEN clauses
    while (match(TokenType::KW_WHEN)) {
        CaseExpr::WhenClause when;
        when.when_expr = parseExpression();
        expect(TokenType::KW_THEN, "Expected THEN after WHEN condition");
        when.then_expr = parseExpression();
        expr->when_clauses.push_back(when);
    }

    // ELSE clause
    if (match(TokenType::KW_ELSE)) {
        expr->else_expr = parseExpression();
    }

    expect(TokenType::KW_END, "Expected END after CASE expression");

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseExistsExpr() {
    auto* expr = arena_.create<ExistsExpr>();
    SourceLocation start = currentLocation();

    // EXISTS already consumed
    expect(TokenType::LEFT_PAREN, "Expected '(' after EXISTS");

    if (check(TokenType::KW_SELECT) || check(TokenType::KW_WITH)) {
        expr->subquery = parseSelectWithClause();
    } else {
        error("Expected SELECT after EXISTS");
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after EXISTS subquery");

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseInExpr(Expression* left) {
    auto* expr = arena_.create<InExpr>();
    SourceLocation start = currentLocation();

    expr->expr = left;

    // NOT IN was already handled, just IN here
    // IN already consumed

    expect(TokenType::LEFT_PAREN, "Expected '(' after IN");

    if (check(TokenType::KW_SELECT) || check(TokenType::KW_WITH)) {
        expr->subquery = parseSelectWithClause();
    } else {
        // List of values
        do {
            expr->values.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after IN list");

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseBetweenExpr(Expression* left) {
    auto* expr = arena_.create<BetweenExpr>();
    SourceLocation start = currentLocation();

    expr->expr = left;

    // BETWEEN already consumed
    // Parse at additive level (not AND level) so AND isn't consumed by logical expression
    expr->low = parseAddExpr();

    expect(TokenType::KW_AND, "Expected AND in BETWEEN expression");

    // Parse high bound at additive level as well
    expr->high = parseAddExpr();

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseLikeExpr(Expression* left) {
    auto* expr = arena_.create<LikeExpr>();
    SourceLocation start = currentLocation();

    expr->expr = left;

    // LIKE already consumed
    expr->pattern = parseExpression();

    // ESCAPE clause
    if (matchContextual("ESCAPE")) {
        expr->escape = parseExpression();
    }

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseIsNullExpr(Expression* left) {
    auto* expr = arena_.create<IsNullExpr>();
    SourceLocation start = currentLocation();

    expr->expr = left;
    // is_not is set by the caller

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseArrayExpr() {
    auto* expr = arena_.create<ArrayExpr>();
    SourceLocation start = currentLocation();

    // ARRAY already consumed
    expect(TokenType::LEFT_BRACKET, "Expected '[' after ARRAY");

    if (!check(TokenType::RIGHT_BRACKET)) {
        do {
            expr->elements.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    expect(TokenType::RIGHT_BRACKET, "Expected ']' after ARRAY elements");

    expr->span = makeSpan(start);
    return expr;
}

// =============================================================================
// Expression Parsing (Basic for DDL)
// =============================================================================

Expression* Parser::parseExpression() {
    SourceLocation start = currentLocation();
    Expression* expr = parseOrExpr();
    if (expr && expr->span.length == 0) {
        expr->span = makeSpan(start);
    }
    return expr;
}

Expression* Parser::parseExpressionWithLeft(Expression* left) {
    SourceLocation start = currentLocation();
    Expression* expr = parseOrExprWithLeft(left);
    if (expr && expr->span.length == 0) {
        expr->span = makeSpan(start);
    }
    return expr;
}

Expression* Parser::parseOrExpr() {
    Expression* left = parseAndExpr();

    while (match(TokenType::KW_OR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::OR;
        expr->left = left;
        expr->right = parseAndExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseOrExprWithLeft(Expression* left) {
    left = parseAndExprWithLeft(left);

    while (match(TokenType::KW_OR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::OR;
        expr->left = left;
        expr->right = parseAndExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseAndExpr() {
    Expression* left = parseNotExpr();

    while (match(TokenType::KW_AND)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::AND;
        expr->left = left;
        expr->right = parseNotExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseAndExprWithLeft(Expression* left) {
    left = parseComparisonExprWithLeft(left);

    while (match(TokenType::KW_AND)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::AND;
        expr->left = left;
        expr->right = parseNotExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseNotExpr() {
    if (match(TokenType::KW_NOT)) {
        auto* expr = arena_.create<UnaryExpr>();
        expr->op = UnaryOp::NOT;
        expr->operand = parseNotExpr();
        return expr;
    }

    return parseComparisonExpr();
}

Expression* Parser::parseComparisonExpr() {
    Expression* left = parseAddExpr();

    // IS NULL / IS NOT NULL / IS TRUE / IS FALSE / IS UNKNOWN
    if (match(TokenType::KW_IS)) {
        bool is_not = match(TokenType::KW_NOT);

        if (match(TokenType::KW_NULL)) {
            auto* expr = arena_.create<IsNullExpr>();
            expr->expr = left;
            expr->negated = is_not;
            return expr;
        } else if (match(TokenType::KW_TRUE)) {
            // IS [NOT] TRUE
            auto* expr = arena_.create<BinaryExpr>();
            expr->op = is_not ? BinaryOp::NE : BinaryOp::EQ;
            expr->left = left;
            auto* rhs = arena_.create<LiteralExpr>();
            rhs->literal_type = LiteralType::BOOLEAN;
            rhs->bool_value = true;
            expr->right = rhs;
            return expr;
        } else if (match(TokenType::KW_FALSE)) {
            // IS [NOT] FALSE
            auto* expr = arena_.create<BinaryExpr>();
            expr->op = is_not ? BinaryOp::NE : BinaryOp::EQ;
            expr->left = left;
            auto* rhs = arena_.create<LiteralExpr>();
            rhs->literal_type = LiteralType::BOOLEAN;
            rhs->bool_value = false;
            expr->right = rhs;
            return expr;
        } else if (matchContextual("DISTINCT")) {
            // IS [NOT] DISTINCT FROM
            expectContextual("FROM", "Expected FROM after DISTINCT");
            auto* expr = arena_.create<BinaryExpr>();
            expr->op = is_not ? BinaryOp::EQ : BinaryOp::NE;  // IS DISTINCT FROM = not equal (null-safe)
            expr->left = left;
            expr->right = parseAddExpr();
            return expr;
        }

        error("Expected NULL, TRUE, FALSE, or DISTINCT after IS");
        return left;
    }

    // NOT IN / NOT BETWEEN / NOT LIKE
    if (match(TokenType::KW_NOT)) {
        if (match(TokenType::KW_IN)) {
            auto* expr = parseInExpr(left);
            if (auto* inExpr = dynamic_cast<InExpr*>(expr)) {
                inExpr->negated = true;
            }
            return expr;
        } else if (match(TokenType::KW_BETWEEN)) {
            auto* expr = parseBetweenExpr(left);
            if (auto* betweenExpr = dynamic_cast<BetweenExpr*>(expr)) {
                betweenExpr->negated = true;
            }
            return expr;
        } else if (match(TokenType::KW_LIKE)) {
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::LIKE;
            }
            return expr;
        } else if (matchContextual("ILIKE")) {
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->case_insensitive = true;
                likeExpr->match_kind = LikeMatchKind::ILIKE;
            }
            return expr;
        } else if (matchContextual("SIMILAR")) {
            expectContextual("TO", "Expected TO after SIMILAR");
            // SIMILAR TO is treated as LIKE with regex semantics
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::SIMILAR;
            }
            return expr;
        }
        error("Expected IN, BETWEEN, LIKE, ILIKE, or SIMILAR after NOT");
        return left;
    }

    // IN
    if (match(TokenType::KW_IN)) {
        return parseInExpr(left);
    }

    // BETWEEN
    if (match(TokenType::KW_BETWEEN)) {
        return parseBetweenExpr(left);
    }

    // LIKE
    if (match(TokenType::KW_LIKE)) {
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::LIKE;
        }
        return expr;
    }

    // ILIKE (PostgreSQL case-insensitive LIKE)
    if (matchContextual("ILIKE")) {
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->case_insensitive = true;
            likeExpr->match_kind = LikeMatchKind::ILIKE;
        }
        return expr;
    }

    // SIMILAR TO
    if (matchContextual("SIMILAR")) {
        expectContextual("TO", "Expected TO after SIMILAR");
        // SIMILAR TO is treated as LIKE with regex semantics
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::SIMILAR;
        }
        return expr;
    }

    if (match(TokenType::TILDE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_MATCH;
        expr->left = left;
        expr->right = parseAddExpr();
        return expr;
    }
    if (match(TokenType::TILDE_STAR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_MATCH_CI;
        expr->left = left;
        expr->right = parseAddExpr();
        return expr;
    }
    if (match(TokenType::EXCLAIM_TILDE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_NOT_MATCH;
        expr->left = left;
        expr->right = parseAddExpr();
        return expr;
    }
    if (match(TokenType::EXCLAIM_TILDE_STAR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_NOT_MATCH_CI;
        expr->left = left;
        expr->right = parseAddExpr();
        return expr;
    }

    // Comparison operators
    BinaryOp op;
    bool found = false;

    if (match(TokenType::EQUAL)) { op = BinaryOp::EQ; found = true; }
    else if (match(TokenType::NOT_EQUAL)) { op = BinaryOp::NE; found = true; }
    else if (match(TokenType::LESS_THAN)) { op = BinaryOp::LT; found = true; }
    else if (match(TokenType::LESS_EQUAL)) { op = BinaryOp::LE; found = true; }
    else if (match(TokenType::GREATER_THAN)) { op = BinaryOp::GT; found = true; }
    else if (match(TokenType::GREATER_EQUAL)) { op = BinaryOp::GE; found = true; }

    if (found) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseAddExpr();
        return expr;
    }

    return left;
}

Expression* Parser::parseComparisonExprWithLeft(Expression* left) {
    left = parseAddExprWithLeft(left);

    if (match(TokenType::KW_IS)) {
        bool is_not = match(TokenType::KW_NOT);

        if (match(TokenType::KW_NULL)) {
            auto* expr = arena_.create<IsNullExpr>();
            expr->expr = left;
            expr->negated = is_not;
            return expr;
        } else if (match(TokenType::KW_TRUE)) {
            auto* expr = arena_.create<BinaryExpr>();
            expr->op = is_not ? BinaryOp::NE : BinaryOp::EQ;
            expr->left = left;
            auto* rhs = arena_.create<LiteralExpr>();
            rhs->literal_type = LiteralType::BOOLEAN;
            rhs->bool_value = true;
            expr->right = rhs;
            return expr;
        } else if (match(TokenType::KW_FALSE)) {
            auto* expr = arena_.create<BinaryExpr>();
            expr->op = is_not ? BinaryOp::NE : BinaryOp::EQ;
            expr->left = left;
            auto* rhs = arena_.create<LiteralExpr>();
            rhs->literal_type = LiteralType::BOOLEAN;
            rhs->bool_value = false;
            expr->right = rhs;
            return expr;
        } else if (matchContextual("DISTINCT")) {
            expectContextual("FROM", "Expected FROM after DISTINCT");
            auto* expr = arena_.create<BinaryExpr>();
            expr->op = is_not ? BinaryOp::EQ : BinaryOp::NE;
            expr->left = left;
            expr->right = parseAddExpr();
            return expr;
        }

        error("Expected NULL, TRUE, FALSE, or DISTINCT after IS");
        return left;
    }

    if (match(TokenType::KW_NOT)) {
        if (match(TokenType::KW_IN)) {
            auto* expr = parseInExpr(left);
            if (auto* inExpr = dynamic_cast<InExpr*>(expr)) {
                inExpr->negated = true;
            }
            return expr;
        } else if (match(TokenType::KW_BETWEEN)) {
            auto* expr = parseBetweenExpr(left);
            if (auto* betweenExpr = dynamic_cast<BetweenExpr*>(expr)) {
                betweenExpr->negated = true;
            }
            return expr;
        } else if (match(TokenType::KW_LIKE)) {
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::LIKE;
            }
            return expr;
        } else if (matchContextual("ILIKE")) {
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->case_insensitive = true;
                likeExpr->match_kind = LikeMatchKind::ILIKE;
            }
            return expr;
        } else if (matchContextual("SIMILAR")) {
            expectContextual("TO", "Expected TO after SIMILAR");
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::SIMILAR;
            }
            return expr;
        }
        error("Expected IN, BETWEEN, LIKE, ILIKE, or SIMILAR after NOT");
        return left;
    }

    if (match(TokenType::KW_IN)) {
        return parseInExpr(left);
    }

    if (match(TokenType::KW_BETWEEN)) {
        return parseBetweenExpr(left);
    }

    if (match(TokenType::KW_LIKE)) {
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::LIKE;
        }
        return expr;
    }

    if (matchContextual("ILIKE")) {
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->case_insensitive = true;
            likeExpr->match_kind = LikeMatchKind::ILIKE;
        }
        return expr;
    }

    if (matchContextual("SIMILAR")) {
        expectContextual("TO", "Expected TO after SIMILAR");
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::SIMILAR;
        }
        return expr;
    }

    if (match(TokenType::TILDE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_MATCH;
        expr->left = left;
        expr->right = parseAddExpr();
        return expr;
    }
    if (match(TokenType::TILDE_STAR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_MATCH_CI;
        expr->left = left;
        expr->right = parseAddExpr();
        return expr;
    }
    if (match(TokenType::EXCLAIM_TILDE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_NOT_MATCH;
        expr->left = left;
        expr->right = parseAddExpr();
        return expr;
    }
    if (match(TokenType::EXCLAIM_TILDE_STAR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_NOT_MATCH_CI;
        expr->left = left;
        expr->right = parseAddExpr();
        return expr;
    }

    BinaryOp op;
    bool found = false;

    if (match(TokenType::EQUAL)) { op = BinaryOp::EQ; found = true; }
    else if (match(TokenType::NOT_EQUAL)) { op = BinaryOp::NE; found = true; }
    else if (match(TokenType::LESS_THAN)) { op = BinaryOp::LT; found = true; }
    else if (match(TokenType::LESS_EQUAL)) { op = BinaryOp::LE; found = true; }
    else if (match(TokenType::GREATER_THAN)) { op = BinaryOp::GT; found = true; }
    else if (match(TokenType::GREATER_EQUAL)) { op = BinaryOp::GE; found = true; }

    if (found) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseAddExpr();
        return expr;
    }

    return left;
}

Expression* Parser::parseAddExpr() {
    Expression* left = parseMulExpr();

    while (true) {
        BinaryOp op;
        if (match(TokenType::PLUS)) op = BinaryOp::ADD;
        else if (match(TokenType::MINUS)) op = BinaryOp::SUB;
        else if (match(TokenType::DOUBLE_PIPE)) op = BinaryOp::CONCAT;
        else break;

        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseMulExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseAddExprWithLeft(Expression* left) {
    left = parseMulExprWithLeft(left);

    while (true) {
        BinaryOp op;
        if (match(TokenType::PLUS)) op = BinaryOp::ADD;
        else if (match(TokenType::MINUS)) op = BinaryOp::SUB;
        else if (match(TokenType::DOUBLE_PIPE)) op = BinaryOp::CONCAT;
        else break;

        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseMulExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseMulExpr() {
    Expression* left = parseUnaryExpr();

    while (true) {
        BinaryOp op;
        if (match(TokenType::STAR)) op = BinaryOp::MUL;
        else if (match(TokenType::SLASH)) op = BinaryOp::DIV;
        else if (match(TokenType::PERCENT)) op = BinaryOp::MOD;
        else if (match(TokenType::ARROW)) op = BinaryOp::JSON_EXTRACT;
        else if (match(TokenType::DOUBLE_ARROW)) op = BinaryOp::JSON_EXTRACT_TEXT;
        else if (match(TokenType::HASH_ARROW)) op = BinaryOp::JSON_HASH_EXTRACT;
        else if (match(TokenType::HASH_DOUBLE_ARROW)) op = BinaryOp::JSON_HASH_EXTRACT_TEXT;
        else break;

        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseUnaryExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseMulExprWithLeft(Expression* left) {
    while (true) {
        BinaryOp op;
        if (match(TokenType::STAR)) op = BinaryOp::MUL;
        else if (match(TokenType::SLASH)) op = BinaryOp::DIV;
        else if (match(TokenType::PERCENT)) op = BinaryOp::MOD;
        else if (match(TokenType::ARROW)) op = BinaryOp::JSON_EXTRACT;
        else if (match(TokenType::DOUBLE_ARROW)) op = BinaryOp::JSON_EXTRACT_TEXT;
        else if (match(TokenType::HASH_ARROW)) op = BinaryOp::JSON_HASH_EXTRACT;
        else if (match(TokenType::HASH_DOUBLE_ARROW)) op = BinaryOp::JSON_HASH_EXTRACT_TEXT;
        else break;

        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseUnaryExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseUnaryExpr() {
    if (match(TokenType::MINUS)) {
        auto* expr = arena_.create<UnaryExpr>();
        expr->op = UnaryOp::NEGATE;
        expr->operand = parseUnaryExpr();
        return expr;
    }

    return parsePrimaryExpr();
}

Expression* Parser::parsePrimaryExpr() {
    // Literals
    if (check(TokenType::INTEGER_LITERAL) || check(TokenType::FLOAT_LITERAL) ||
        check(TokenType::STRING_LITERAL) || check(TokenType::BLOB_LITERAL) ||
        check(TokenType::KW_TRUE) || check(TokenType::KW_FALSE) ||
        check(TokenType::KW_NULL)) {
        return parseLiteral();
    }

    if (matchContextual("EXTRACT")) {
        return parseExtractExpr();
    }

    if (matchContextual("ALTER_ELEMENT")) {
        return parseAlterElementExpr();
    }

    // CAST expression
    if (match(TokenType::KW_CAST)) {
        return parseCastExpr();
    }

    // CASE expression
    if (match(TokenType::KW_CASE)) {
        return parseCaseExpr();
    }

    // EXISTS expression
    if (match(TokenType::KW_EXISTS)) {
        return parseExistsExpr();
    }

    // ARRAY expression
    if (matchContextual("ARRAY")) {
        return parseArrayExpr();
    }

    // Parenthesized expression or subquery
    if (check(TokenType::LEFT_PAREN)) {
        advance();
        if (check(TokenType::KW_SELECT) || check(TokenType::KW_WITH)) {
            // Scalar subquery
            auto* subq = arena_.create<SubqueryExpr>();
            subq->subquery = parseSelectWithClause();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after subquery");
            return subq;
        }
        // Regular parenthesized expression
        Expression* expr = parseExpression();
        expect(TokenType::RIGHT_PAREN, "Expected ')'");
        return expr;
    }

    // Column reference or function call
    if (isIdentifier() || check(TokenType::DOT) || check(TokenType::DOUBLE_DOT)) {
        SchemaPath path = parseSchemaPath(state_);

        // Check for function call
        if (check(TokenType::LEFT_PAREN)) {
            return parseFunctionCall(std::move(path));
        }

        // Column reference
        auto* expr = arena_.create<ColumnRefExpr>();
        if (path.components.size() == 1) {
            expr->column.column_name = path.components[0];
        } else {
            expr->column.column_name = path.objectName();
            expr->column.has_table_qualifier = true;
            expr->column.table_path.type = path.type;
            expr->column.table_path.components = path.schemaComponents();
        }
        return expr;
    }

    error("Expected expression");
    return nullptr;
}

Expression* Parser::parseLiteral() {
    auto* expr = arena_.create<LiteralExpr>();

    if (check(TokenType::INTEGER_LITERAL)) {
        expr->literal_type = LiteralType::INTEGER;
        expr->int_value = current().value.int_value;
        advance();
    } else if (check(TokenType::FLOAT_LITERAL)) {
        expr->literal_type = LiteralType::FLOAT;
        expr->float_value = current().value.float_value;
        advance();
    } else if (check(TokenType::STRING_LITERAL)) {
        expr->literal_type = LiteralType::STRING;
        expr->string_value = current().value.string_id;
        advance();
    } else if (check(TokenType::BLOB_LITERAL)) {
        expr->literal_type = LiteralType::BLOB;
        expr->string_value = current().value.string_id;
        advance();
    } else if (match(TokenType::KW_TRUE)) {
        expr->literal_type = LiteralType::BOOLEAN;
        expr->bool_value = true;
    } else if (match(TokenType::KW_FALSE)) {
        expr->literal_type = LiteralType::BOOLEAN;
        expr->bool_value = false;
    } else if (match(TokenType::KW_NULL)) {
        expr->literal_type = LiteralType::NULL_VALUE;
    }

    return expr;
}

Expression* Parser::parseFunctionCall(SchemaPath path) {
    auto* expr = arena_.create<FunctionCallExpr>();
    expr->function_path = std::move(path);

    expect(TokenType::LEFT_PAREN, "Expected '(' for function call");

    std::string upper_name;
    if (!expr->function_path.components.empty())
    {
        std::string_view func_name = stringPool().get(expr->function_path.components.back());
        upper_name.reserve(func_name.size());
        for (char c : func_name)
        {
            upper_name.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }

    // Special-case COUNT(*)
    if (check(TokenType::STAR)) {
        if (upper_name != "COUNT") {
            error("'*' is only allowed in COUNT(*)");
        }

        // COUNT(*) is equivalent to COUNT(1)
        auto* literal = arena_.create<LiteralExpr>();
        literal->literal_type = LiteralType::INTEGER;
        literal->int_value = 1;
        expr->arguments.push_back(literal);

        advance();  // consume '*'
        expect(TokenType::RIGHT_PAREN, "Expected ')' after function arguments");
        return expr;
    }

    if (upper_name == "POSITION")
    {
        if (check(TokenType::RIGHT_PAREN))
        {
            error("POSITION requires arguments");
            return expr;
        }

        Expression* needle = parseAddExpr();
        expect(TokenType::KW_IN, "Expected IN in POSITION");
        Expression* haystack = parseAddExpr();
        expr->arguments.push_back(needle);
        expr->arguments.push_back(haystack);
        expect(TokenType::RIGHT_PAREN, "Expected ')' after POSITION arguments");
        return expr;
    }

    if (upper_name == "OVERLAY")
    {
        if (check(TokenType::RIGHT_PAREN))
        {
            error("OVERLAY requires arguments");
            return expr;
        }

        Expression* source = parseExpression();
        if (!expectContextual("PLACING", "Expected PLACING in OVERLAY"))
        {
            return expr;
        }
        Expression* replacement = parseExpression();
        expect(TokenType::KW_FROM, "Expected FROM in OVERLAY");
        Expression* start_pos = parseExpression();
        Expression* length = nullptr;
        if (matchContextual("FOR"))
        {
            length = parseExpression();
        }

        expr->arguments.push_back(source);
        expr->arguments.push_back(replacement);
        expr->arguments.push_back(start_pos);
        if (length)
        {
            expr->arguments.push_back(length);
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after OVERLAY arguments");
        return expr;
    }

    // Parse arguments
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            expr->arguments.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after function arguments");

    return expr;
}

Expression* Parser::parseExtractExpr() {
    // Spec: docs/specifications/EXTRACT_AND_ALTER_ELEMENT.md
    auto* expr = arena_.create<ExtractExpr>();
    expect(TokenType::LEFT_PAREN, "Expected '(' after EXTRACT");
    expr->selector = parseElementSelector();
    expect(TokenType::KW_FROM, "Expected FROM in EXTRACT expression");
    expr->source = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after EXTRACT expression");
    return expr;
}

Expression* Parser::parseAlterElementExpr() {
    // Spec: docs/specifications/EXTRACT_AND_ALTER_ELEMENT.md
    auto* expr = arena_.create<AlterElementExpr>();
    expect(TokenType::LEFT_PAREN, "Expected '(' after ALTER_ELEMENT");
    expr->selector = parseElementSelector();
    expect(TokenType::KW_IN, "Expected IN in ALTER_ELEMENT expression");
    expr->source = parseExpression();
    expectContextual("TO", "Expected TO in ALTER_ELEMENT expression");
    expr->new_value = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after ALTER_ELEMENT expression");
    return expr;
}

ElementSelector Parser::parseElementSelector() {
    ElementSelector selector;

    if (check(TokenType::STRING_LITERAL)) {
        selector.kind = ElementSelector::Kind::STRING_LITERAL;
        selector.string_literal = current().value.string_id;
        advance();
        return selector;
    }

    if (check(TokenType::INTEGER_LITERAL) || check(TokenType::PLUS) ||
        check(TokenType::MINUS) || check(TokenType::LEFT_PAREN)) {
        selector.kind = ElementSelector::Kind::INTEGER_EXPR;
        selector.expr = parseExpression();
        return selector;
    }

    if (isIdentifier()) {
        selector.kind = ElementSelector::Kind::IDENTIFIER;
        selector.identifier = expectIdentifier("Expected element identifier");
        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    selector.args.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after element selector arguments");
        }
        return selector;
    }

    error("Expected element selector");
    return selector;
}

Expression* Parser::parseParenExpr() {
    expect(TokenType::LEFT_PAREN, "Expected '('");
    Expression* expr = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')'");
    return expr;
}

Expression* Parser::parseCastExpr() {
    auto* expr = arena_.create<CastExpr>();

    expect(TokenType::LEFT_PAREN, "Expected '(' after CAST");
    expr->expr = parseExpression();
    expect(TokenType::KW_AS, "Expected AS in CAST expression");
    expr->target_type = parseTypeName();
    // CAST ... USING <format> (see docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md)
    if (match(TokenType::KW_USING)) {
        expr->format = expectIdentifier("Expected CAST USING format");
    }
    expect(TokenType::RIGHT_PAREN, "Expected ')' after CAST");

    return expr;
}

// =============================================================================
// Transaction Statements
// =============================================================================

StartTransactionStmt* Parser::parseStartTransaction() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<StartTransactionStmt>();

    // START TRANSACTION or BEGIN [TRANSACTION] [WORK]
    // We've already consumed BEGIN or START
    if (previous().type == TokenType::KW_START) {
        // START must be followed by TRANSACTION
        expectContextual("TRANSACTION", "Expected TRANSACTION after START");
    } else {
        // BEGIN can optionally have TRANSACTION or WORK
        matchContextual("TRANSACTION");
        matchContextual("WORK");
    }

    auto parseAutocommitMode = [&]() -> AutocommitMode {
        if (match(TokenType::KW_ON) || matchContextual("ON")) {
            return AutocommitMode::ON;
        }
        if (matchContextual("OFF")) {
            return AutocommitMode::OFF;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            int64_t value = current().value.int_value;
            advance();
            if (value == 0) {
                return AutocommitMode::OFF;
            }
            if (value == 1) {
                return AutocommitMode::ON;
            }
            error("AUTOCOMMIT expects 0/1 or ON/OFF");
            return AutocommitMode::UNCHANGED;
        }
        error("Expected AUTOCOMMIT mode (ON/OFF/1/0)");
        return AutocommitMode::UNCHANGED;
    };

    auto parseConflictClause =
        [&](TransactionConflictAction& action, bool& has_error_code, int32_t& error_code) {
            if (!matchContextual("CONFLICT")) {
                error("Expected CONFLICT after ON");
                return;
            }

            if (action != TransactionConflictAction::DEFAULT) {
                error("ON CONFLICT specified more than once");
            }

            if (match(TokenType::KW_COMMIT)) {
                action = TransactionConflictAction::COMMIT;
            } else if (match(TokenType::KW_ROLLBACK)) {
                action = TransactionConflictAction::ROLLBACK;
            } else if (matchContextual("ERROR")) {
                action = TransactionConflictAction::ERROR;
                if (check(TokenType::INTEGER_LITERAL)) {
                    int64_t value = current().value.int_value;
                    advance();
                    if (value < std::numeric_limits<int32_t>::min() ||
                        value > std::numeric_limits<int32_t>::max()) {
                        error("ON CONFLICT ERROR code out of range");
                    } else {
                        has_error_code = true;
                        error_code = static_cast<int32_t>(value);
                    }
                }
            } else if (matchContextual("KEEP")) {
                action = TransactionConflictAction::KEEP;
            } else {
                error("Expected conflict action (COMMIT, ROLLBACK, ERROR, KEEP)");
            }
        };

    auto applySnapshotIsolation = [&]() {
        stmt->has_isolation_level = true;
        if (matchContextual("TABLE")) {
            expectContextual("STABILITY", "Expected STABILITY after SNAPSHOT TABLE");
            stmt->isolation_level = IsolationLevel::SERIALIZABLE;
        } else {
            stmt->isolation_level = IsolationLevel::REPEATABLE_READ;
        }
    };

    auto parseReadCommittedVariant = [&]() {
        auto tokenMatches = [&](const Token& token, const char* keyword) -> bool {
            std::string_view text = state_.lexer().getTokenText(token);
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
            return true;
        };

        if (stmt->has_read_committed_mode) {
            error("READ COMMITTED mode specified more than once");
            return;
        }
        if (matchContextual("READ")) {
            if (matchContextual("CONSISTENCY")) {
                stmt->has_read_committed_mode = true;
                stmt->read_committed_mode = ReadCommittedMode::READ_CONSISTENCY;
            } else {
                error("Expected CONSISTENCY after READ COMMITTED READ");
            }
        } else if (matchContextual("RECORD_VERSION")) {
            stmt->has_read_committed_mode = true;
            stmt->read_committed_mode = ReadCommittedMode::RECORD_VERSION;
        } else if (matchContextual("RECORD")) {
            expectContextual("VERSION", "Expected VERSION after RECORD");
            stmt->has_read_committed_mode = true;
            stmt->read_committed_mode = ReadCommittedMode::RECORD_VERSION;
        } else if (checkContextual("NO")) {
            Token next = state_.lexer().peekToken();
            if (next.type == TokenType::IDENTIFIER &&
                (tokenMatches(next, "RECORD") || tokenMatches(next, "RECORD_VERSION"))) {
                matchContextual("NO");
                if (matchContextual("RECORD_VERSION")) {
                    stmt->has_read_committed_mode = true;
                    stmt->read_committed_mode = ReadCommittedMode::NO_RECORD_VERSION;
                } else {
                    expectContextual("RECORD", "Expected RECORD after NO");
                    expectContextual("VERSION", "Expected VERSION after NO RECORD");
                    stmt->has_read_committed_mode = true;
                    stmt->read_committed_mode = ReadCommittedMode::NO_RECORD_VERSION;
                }
            }
        }
    };

    // Parse transaction characteristics (SQL-standard + Firebird legacy) in any order.
    while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        if (match(TokenType::KW_ON)) {
            parseConflictClause(stmt->conflict_action, stmt->has_conflict_error_code,
                                stmt->conflict_error_code);
        } else if (matchContextual("ISOLATION")) {
            expectContextual("LEVEL", "Expected LEVEL after ISOLATION");

            stmt->has_isolation_level = true;
            if (matchContextual("READ")) {
                if (matchContextual("UNCOMMITTED")) {
                    stmt->isolation_level = IsolationLevel::READ_UNCOMMITTED;
                } else if (matchContextual("COMMITTED")) {
                    stmt->isolation_level = IsolationLevel::READ_COMMITTED;
                    parseReadCommittedVariant();
                } else {
                    error("Expected UNCOMMITTED or COMMITTED after READ");
                }
            } else if (matchContextual("REPEATABLE")) {
                expectContextual("READ", "Expected READ after REPEATABLE");
                stmt->isolation_level = IsolationLevel::REPEATABLE_READ;
            } else if (matchContextual("SERIALIZABLE")) {
                stmt->isolation_level = IsolationLevel::SERIALIZABLE;
            } else if (matchContextual("SNAPSHOT")) {
                applySnapshotIsolation();
            } else {
                error("Expected isolation level");
            }
        } else if (matchContextual("READ")) {
            if (matchContextual("ONLY")) {
                stmt->has_access_mode = true;
                stmt->access_mode = TransactionAccess::READ_ONLY;
            } else if (matchContextual("WRITE")) {
                stmt->has_access_mode = true;
                stmt->access_mode = TransactionAccess::READ_WRITE;
            } else if (matchContextual("COMMITTED")) {
                stmt->has_isolation_level = true;
                stmt->isolation_level = IsolationLevel::READ_COMMITTED;
                parseReadCommittedVariant();
            } else if (matchContextual("UNCOMMITTED")) {
                stmt->has_isolation_level = true;
                stmt->isolation_level = IsolationLevel::READ_UNCOMMITTED;
            } else {
                error("Expected ONLY, WRITE, COMMITTED, or UNCOMMITTED after READ");
            }
        } else if (matchContextual("SNAPSHOT")) {
            applySnapshotIsolation();
        } else if (matchContextual("DEFERRABLE")) {
            stmt->deferrable = true;
        } else if (match(TokenType::KW_NOT)) {
            if (matchContextual("DEFERRABLE")) {
                stmt->not_deferrable = true;
            } else if (matchContextual("WAIT")) {
                stmt->has_wait_mode = true;
                stmt->wait_mode = TransactionWaitMode::NO_WAIT;
            } else {
                error("Expected DEFERRABLE or WAIT after NOT");
            }
        } else if (matchContextual("WAIT")) {
            stmt->has_wait_mode = true;
            stmt->wait_mode = TransactionWaitMode::WAIT;
        } else if (matchContextual("NO")) {
            if (matchContextual("WAIT")) {
                stmt->has_wait_mode = true;
                stmt->wait_mode = TransactionWaitMode::NO_WAIT;
            } else {
                error("Expected WAIT after NO");
            }
        } else if (matchContextual("LOCK")) {
            expectContextual("TIMEOUT", "Expected TIMEOUT after LOCK");
            if (stmt->has_lock_timeout) {
                error("LOCK TIMEOUT specified more than once");
            }
            if (!check(TokenType::INTEGER_LITERAL)) {
                error("Expected integer literal after LOCK TIMEOUT");
            } else {
                int64_t value = current().value.int_value;
                advance();
                if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
                    error("LOCK TIMEOUT out of range");
                } else {
                    stmt->has_lock_timeout = true;
                    stmt->lock_timeout_seconds = static_cast<uint32_t>(value);
                }
            }
        } else if (matchContextual("RESERVING")) {
            // Table reservations are parsed into a simple list for later resolution.
            do {
                StringPool::StringId table_name =
                    expectIdentifier("Expected table name after RESERVING");
                expectContextual("FOR", "Expected FOR after RESERVING table name");

                TableLockMode lock_mode = TableLockMode::SHARED;
                if (matchContextual("SHARED")) {
                    lock_mode = TableLockMode::SHARED;
                } else if (matchContextual("PROTECTED")) {
                    lock_mode = TableLockMode::PROTECTED;
                } else {
                    error("Expected SHARED or PROTECTED in RESERVING clause");
                }

                bool for_write = false;
                if (matchContextual("READ")) {
                    for_write = false;
                } else if (matchContextual("WRITE")) {
                    for_write = true;
                } else {
                    error("Expected READ or WRITE in RESERVING clause");
                }

                stmt->table_reservations.emplace_back(table_name, lock_mode, for_write);
            } while (match(TokenType::COMMA));
        } else if (matchContextual("AUTOCOMMIT")) {
            stmt->has_autocommit = true;
            stmt->autocommit_mode = parseAutocommitMode();
        } else {
            // Unknown characteristic, stop parsing
            break;
        }

        // Optional comma between characteristics
        match(TokenType::COMMA);
    }

    stmt->span = makeSpan(start);
    return stmt;
}

PrepareTransactionStmt* Parser::parsePrepareTransaction() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<PrepareTransactionStmt>();

    // PREPARE TRANSACTION 'gid'
    expectContextual("TRANSACTION", "Expected TRANSACTION after PREPARE");

    if (!check(TokenType::STRING_LITERAL)) {
        error("Expected string literal after PREPARE TRANSACTION");
    } else {
        stmt->gid = current().value.string_id;
        advance();
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CommitStmt* Parser::parseCommit() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<CommitStmt>();

    // COMMIT [WORK] [AND [NO] CHAIN]
    matchContextual("WORK");
    matchContextual("TRANSACTION");

    if (matchContextual("PREPARED")) {
        stmt->is_prepared = true;
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal after COMMIT PREPARED");
        } else {
            stmt->prepared_gid = current().value.string_id;
            advance();
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (match(TokenType::KW_AND)) {
        if (matchContextual("NO")) {
            expectContextual("CHAIN", "Expected CHAIN after NO");
            stmt->and_no_chain = true;
        } else {
            expectContextual("CHAIN", "Expected CHAIN after AND");
            stmt->and_chain = true;
        }
    }

    if (matchContextual("RETAINING")) {
        stmt->retaining = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

RollbackStmt* Parser::parseRollback() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<RollbackStmt>();

    // ROLLBACK [WORK] [AND [NO] CHAIN]
    // ROLLBACK [WORK] TO [SAVEPOINT] name
    matchContextual("WORK");
    matchContextual("TRANSACTION");

    if (matchContextual("PREPARED")) {
        stmt->is_prepared = true;
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal after ROLLBACK PREPARED");
        } else {
            stmt->prepared_gid = current().value.string_id;
            advance();
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("TO")) {
        matchContextual("SAVEPOINT");  // Optional SAVEPOINT keyword
        stmt->to_savepoint = true;
        stmt->savepoint_name = expectIdentifier("Expected savepoint name");
    } else if (match(TokenType::KW_AND)) {
        if (matchContextual("NO")) {
            expectContextual("CHAIN", "Expected CHAIN after NO");
            stmt->and_no_chain = true;
        } else {
            expectContextual("CHAIN", "Expected CHAIN after AND");
            stmt->and_chain = true;
        }
    }

    if (matchContextual("RETAINING")) {
        stmt->retaining = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

SavepointStmt* Parser::parseSavepoint() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<SavepointStmt>();

    // SAVEPOINT name
    stmt->name = expectIdentifier("Expected savepoint name");

    stmt->span = makeSpan(start);
    return stmt;
}

ReleaseSavepointStmt* Parser::parseReleaseSavepoint() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<ReleaseSavepointStmt>();

    // RELEASE [SAVEPOINT] name
    matchContextual("SAVEPOINT");  // Optional SAVEPOINT keyword
    stmt->name = expectIdentifier("Expected savepoint name");

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// Session Statements
// =============================================================================

SetStmt* Parser::parseSet() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<SetStmt>();

    // SET [SESSION | LOCAL] name = value
    // SET [SESSION | LOCAL] name TO value
    // SET [SESSION | LOCAL] name TO DEFAULT
    // SET TIME ZONE value
    // SET TRANSACTION ...
    // SET SESSION AUTHORIZATION ...
    // SET ROLE ...

    auto parseNameOrStringLiteral = [&](const char* error_message) -> StringPool::StringId {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return id;
        }
        if (isIdentifier()) {
            return expectIdentifier(error_message);
        }
        error(error_message);
        return StringPool::INVALID_ID;
    };

    // Check for scope
    if (matchContextual("SESSION")) {
        stmt->scope = SetStmt::Scope::SESSION;
        // Could also be SET SESSION AUTHORIZATION
        if (matchContextual("AUTHORIZATION")) {
            stmt->set_type = SetStmt::SetType::SESSION_AUTHORIZATION;
            if (matchContextual("DEFAULT")) {
                stmt->is_default = true;
            } else {
                stmt->name = parseNameOrStringLiteral("Expected authorization user after SET SESSION AUTHORIZATION");
            }
            stmt->span = makeSpan(start);
            return stmt;
        }
    } else if (matchContextual("LOCAL")) {
        stmt->scope = SetStmt::Scope::LOCAL;
    }

    auto parseAutocommitMode = [&]() -> AutocommitMode {
        if (match(TokenType::KW_ON) || matchContextual("ON")) {
            return AutocommitMode::ON;
        }
        if (matchContextual("OFF")) {
            return AutocommitMode::OFF;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            int64_t value = current().value.int_value;
            advance();
            if (value == 0) {
                return AutocommitMode::OFF;
            }
            if (value == 1) {
                return AutocommitMode::ON;
            }
            error("AUTOCOMMIT expects 0/1 or ON/OFF");
            return AutocommitMode::UNCHANGED;
        }
        error("Expected AUTOCOMMIT mode (ON/OFF/1/0)");
        return AutocommitMode::UNCHANGED;
    };

    auto parseConflictClause =
        [&](TransactionConflictAction& action, bool& has_error_code, int32_t& error_code) {
            if (!matchContextual("CONFLICT")) {
                error("Expected CONFLICT after ON");
                return;
            }

            if (action != TransactionConflictAction::DEFAULT) {
                error("ON CONFLICT specified more than once");
            }

            if (match(TokenType::KW_COMMIT)) {
                action = TransactionConflictAction::COMMIT;
            } else if (match(TokenType::KW_ROLLBACK)) {
                action = TransactionConflictAction::ROLLBACK;
            } else if (matchContextual("ERROR")) {
                action = TransactionConflictAction::ERROR;
                if (check(TokenType::INTEGER_LITERAL)) {
                    int64_t value = current().value.int_value;
                    advance();
                    if (value < std::numeric_limits<int32_t>::min() ||
                        value > std::numeric_limits<int32_t>::max()) {
                        error("ON CONFLICT ERROR code out of range");
                    } else {
                        has_error_code = true;
                        error_code = static_cast<int32_t>(value);
                    }
                }
            } else if (matchContextual("KEEP")) {
                action = TransactionConflictAction::KEEP;
            } else {
                error("Expected conflict action (COMMIT, ROLLBACK, ERROR, KEEP)");
            }
        };

    auto applySnapshotIsolation = [&]() {
        stmt->has_isolation_level = true;
        if (matchContextual("TABLE")) {
            expectContextual("STABILITY", "Expected STABILITY after SNAPSHOT TABLE");
            stmt->isolation_level = IsolationLevel::SERIALIZABLE;
        } else {
            stmt->isolation_level = IsolationLevel::REPEATABLE_READ;
        }
    };

    auto parseReadCommittedVariant = [&]() {
        auto tokenMatches = [&](const Token& token, const char* keyword) -> bool {
            std::string_view text = state_.lexer().getTokenText(token);
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
            return true;
        };

        if (stmt->has_read_committed_mode) {
            error("READ COMMITTED mode specified more than once");
            return;
        }
        if (matchContextual("READ")) {
            if (matchContextual("CONSISTENCY")) {
                stmt->has_read_committed_mode = true;
                stmt->read_committed_mode = ReadCommittedMode::READ_CONSISTENCY;
            } else {
                error("Expected CONSISTENCY after READ COMMITTED READ");
            }
        } else if (matchContextual("RECORD_VERSION")) {
            stmt->has_read_committed_mode = true;
            stmt->read_committed_mode = ReadCommittedMode::RECORD_VERSION;
        } else if (matchContextual("RECORD")) {
            expectContextual("VERSION", "Expected VERSION after RECORD");
            stmt->has_read_committed_mode = true;
            stmt->read_committed_mode = ReadCommittedMode::RECORD_VERSION;
        } else if (checkContextual("NO")) {
            Token next = state_.lexer().peekToken();
            if (next.type == TokenType::IDENTIFIER &&
                (tokenMatches(next, "RECORD") || tokenMatches(next, "RECORD_VERSION"))) {
                matchContextual("NO");
                if (matchContextual("RECORD_VERSION")) {
                    stmt->has_read_committed_mode = true;
                    stmt->read_committed_mode = ReadCommittedMode::NO_RECORD_VERSION;
                } else {
                    expectContextual("RECORD", "Expected RECORD after NO");
                    expectContextual("VERSION", "Expected VERSION after NO RECORD");
                    stmt->has_read_committed_mode = true;
                    stmt->read_committed_mode = ReadCommittedMode::NO_RECORD_VERSION;
                }
            }
        }
    };

    // Check for special SET variants
    if (matchContextual("TIME")) {
        expectContextual("ZONE", "Expected ZONE after TIME");
        stmt->set_type = SetStmt::SetType::TIME_ZONE;

        if (matchContextual("LOCAL") || matchContextual("DEFAULT")) {
            stmt->is_default = true;
        } else {
            stmt->value = parseExpression();
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("AUTOCOMMIT")) {
        stmt->set_type = SetStmt::SetType::AUTOCOMMIT;
        stmt->has_autocommit = true;
        stmt->autocommit_mode = parseAutocommitMode();

        if (match(TokenType::KW_ON)) {
            parseConflictClause(stmt->conflict_action, stmt->has_conflict_error_code,
                                stmt->conflict_error_code);
        }

        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("TRANSACTION")) {
        stmt->set_type = SetStmt::SetType::TRANSACTION;

        // Parse transaction characteristics (same as START TRANSACTION)
        while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
            if (match(TokenType::KW_ON)) {
                parseConflictClause(stmt->conflict_action, stmt->has_conflict_error_code,
                                    stmt->conflict_error_code);
            } else if (matchContextual("ISOLATION")) {
                expectContextual("LEVEL", "Expected LEVEL after ISOLATION");

                stmt->has_isolation_level = true;
                if (matchContextual("READ")) {
                    if (matchContextual("UNCOMMITTED")) {
                        stmt->isolation_level = IsolationLevel::READ_UNCOMMITTED;
                    } else if (matchContextual("COMMITTED")) {
                        stmt->isolation_level = IsolationLevel::READ_COMMITTED;
                        parseReadCommittedVariant();
                    } else {
                        error("Expected UNCOMMITTED or COMMITTED after READ");
                    }
                } else if (matchContextual("REPEATABLE")) {
                    expectContextual("READ", "Expected READ after REPEATABLE");
                    stmt->isolation_level = IsolationLevel::REPEATABLE_READ;
                } else if (matchContextual("SERIALIZABLE")) {
                    stmt->isolation_level = IsolationLevel::SERIALIZABLE;
                } else if (matchContextual("SNAPSHOT")) {
                    applySnapshotIsolation();
                } else {
                    error("Expected isolation level");
                }
            } else if (matchContextual("READ")) {
                if (matchContextual("ONLY")) {
                    stmt->has_access_mode = true;
                    stmt->access_mode = TransactionAccess::READ_ONLY;
                } else if (matchContextual("WRITE")) {
                    stmt->has_access_mode = true;
                    stmt->access_mode = TransactionAccess::READ_WRITE;
                } else if (matchContextual("COMMITTED")) {
                    stmt->has_isolation_level = true;
                    stmt->isolation_level = IsolationLevel::READ_COMMITTED;
                    parseReadCommittedVariant();
                } else if (matchContextual("UNCOMMITTED")) {
                    stmt->has_isolation_level = true;
                    stmt->isolation_level = IsolationLevel::READ_UNCOMMITTED;
                } else {
                    error("Expected ONLY, WRITE, COMMITTED, or UNCOMMITTED after READ");
                }
            } else if (matchContextual("SNAPSHOT")) {
                applySnapshotIsolation();
            } else if (matchContextual("DEFERRABLE")) {
                stmt->deferrable = true;
            } else if (match(TokenType::KW_NOT)) {
                if (matchContextual("DEFERRABLE")) {
                    stmt->not_deferrable = true;
                } else if (matchContextual("WAIT")) {
                    stmt->has_wait_mode = true;
                    stmt->wait_mode = TransactionWaitMode::NO_WAIT;
                } else {
                    error("Expected DEFERRABLE or WAIT after NOT");
                }
            } else if (matchContextual("WAIT")) {
                stmt->has_wait_mode = true;
                stmt->wait_mode = TransactionWaitMode::WAIT;
            } else if (matchContextual("NO")) {
                if (matchContextual("WAIT")) {
                    stmt->has_wait_mode = true;
                    stmt->wait_mode = TransactionWaitMode::NO_WAIT;
                } else {
                    error("Expected WAIT after NO");
                }
            } else if (matchContextual("LOCK")) {
                expectContextual("TIMEOUT", "Expected TIMEOUT after LOCK");
                if (stmt->has_lock_timeout) {
                    error("LOCK TIMEOUT specified more than once");
                }
                if (!check(TokenType::INTEGER_LITERAL)) {
                    error("Expected integer literal after LOCK TIMEOUT");
                } else {
                    int64_t value = current().value.int_value;
                    advance();
                    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
                        error("LOCK TIMEOUT out of range");
                    } else {
                        stmt->has_lock_timeout = true;
                        stmt->lock_timeout_seconds = static_cast<uint32_t>(value);
                    }
                }
            } else if (matchContextual("RESERVING")) {
                // Table reservations are parsed into a simple list for later resolution.
                do {
                    StringPool::StringId table_name =
                        expectIdentifier("Expected table name after RESERVING");
                    expectContextual("FOR", "Expected FOR after RESERVING table name");

                    TableLockMode lock_mode = TableLockMode::SHARED;
                    if (matchContextual("SHARED")) {
                        lock_mode = TableLockMode::SHARED;
                    } else if (matchContextual("PROTECTED")) {
                        lock_mode = TableLockMode::PROTECTED;
                    } else {
                        error("Expected SHARED or PROTECTED in RESERVING clause");
                    }

                    bool for_write = false;
                    if (matchContextual("READ")) {
                        for_write = false;
                    } else if (matchContextual("WRITE")) {
                        for_write = true;
                    } else {
                        error("Expected READ or WRITE in RESERVING clause");
                    }

                    stmt->table_reservations.emplace_back(table_name, lock_mode, for_write);
                } while (match(TokenType::COMMA));
            } else if (matchContextual("AUTOCOMMIT")) {
                stmt->has_autocommit = true;
                stmt->autocommit_mode = parseAutocommitMode();
            } else {
                break;
            }
            match(TokenType::COMMA);
        }

        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("SQL")) {
        expectContextual("DIALECT", "Expected DIALECT after SQL");
        stmt->set_type = SetStmt::SetType::SQL_DIALECT;

        if (check(TokenType::INTEGER_LITERAL)) {
            int64_t dialect = current().value.int_value;
            advance();
            if (dialect >= 1 && dialect <= 3) {
                stmt->sql_dialect = static_cast<uint8_t>(dialect);
            } else {
                error("SQL DIALECT must be 1, 2, or 3");
            }
        } else {
            error("Expected SQL dialect number (1, 2, or 3)");
        }

        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("NAMES")) {
        stmt->set_type = SetStmt::SetType::NAMES;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected character set name after SET NAMES");
        } else {
            error("Expected character set name after SET NAMES");
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("LOCAL_TIMEOUT")) {
        stmt->set_type = SetStmt::SetType::LOCAL_TIMEOUT;
        if (check(TokenType::INTEGER_LITERAL)) {
            int64_t value = current().value.int_value;
            advance();
            if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
                error("LOCAL_TIMEOUT out of range");
            } else {
                stmt->local_timeout_seconds = static_cast<uint32_t>(value);
            }
        } else {
            error("Expected integer literal after SET LOCAL_TIMEOUT");
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("ROLE")) {
        stmt->set_type = SetStmt::SetType::ROLE;
        if (matchContextual("NONE") || matchContextual("DEFAULT")) {
            stmt->is_default = true;
        } else {
            stmt->name = parseNameOrStringLiteral("Expected role name after SET ROLE");
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    auto parseVariableAssignment = [&](StringPool::StringId name_id) -> SetStmt* {
        stmt->set_type = SetStmt::SetType::VARIABLE;
        stmt->name = name_id;

        // = or TO
        if (!match(TokenType::EQUAL) && !matchContextual("TO")) {
            error("Expected '=' or TO after variable name");
        }

        // Value can be DEFAULT or an expression (or list of values)
        if (match(TokenType::KW_DEFAULT) || matchContextual("DEFAULT")) {
            stmt->is_default = true;
        } else {
            // Parse value(s) - some settings accept comma-separated lists
            stmt->value = parseExpression();
            while (match(TokenType::COMMA)) {
                stmt->values.push_back(stmt->value);
                stmt->value = parseExpression();
            }
            if (!stmt->values.empty()) {
                stmt->values.push_back(stmt->value);
                stmt->value = nullptr;  // Use values list instead
            }
        }

        stmt->span = makeSpan(start);
        return stmt;
    };

    if (matchContextual("PARSER")) {
        if (matchContextual("VERSION")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                advance();
            }
            error("SET PARSER VERSION is not supported");
            stmt->span = makeSpan(start);
            return stmt;
        }
        return parseVariableAssignment(stringPool().intern("PARSER"));
    }

    // Regular SET name = value / SET name TO value
    StringPool::StringId var_name = expectIdentifier("Expected variable name");
    return parseVariableAssignment(var_name);
}

ResetStmt* Parser::parseReset() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<ResetStmt>();

    // RESET name
    // RESET ALL
    if (matchContextual("ALL")) {
        stmt->reset_all = true;
    } else {
        stmt->name = expectIdentifier("Expected variable name or ALL");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

ShowStmt* Parser::parseShow() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<ShowStmt>();

    // Helper to parse optional LIKE clause
    auto parseLikeClause = [&]() {
        if (match(TokenType::KW_LIKE)) {
            if (check(TokenType::STRING_LITERAL)) {
                stmt->like_pattern = current().value.string_id;
                advance();
            } else {
                error("Expected string pattern after LIKE");
            }
        }
    };

    // Helper to parse optional FROM clause
    auto parseFromClause = [&]() {
        if (match(TokenType::KW_FROM)) {
            stmt->from_name = expectIdentifier("Expected name after FROM");
        }
    };

    // SHOW ALL
    if (matchContextual("ALL")) {
        stmt->show_type = ShowStmt::ShowType::ALL;
    }
    // SHOW TRANSACTION ISOLATION LEVEL
    else if (matchContextual("TRANSACTION")) {
        expectContextual("ISOLATION", "Expected ISOLATION after TRANSACTION");
        expectContextual("LEVEL", "Expected LEVEL after ISOLATION");
        stmt->show_type = ShowStmt::ShowType::TRANSACTION_ISOLATION_LEVEL;
    }
    // SHOW TABLES [FROM db] [LIKE pattern]
    else if (matchContextual("TABLES")) {
        stmt->show_type = ShowStmt::ShowType::TABLES;
        parseFromClause();
        parseLikeClause();
    }
    // SHOW DATABASES [LIKE pattern]
    else if (matchContextual("DATABASES")) {
        stmt->show_type = ShowStmt::ShowType::DATABASES;
        parseLikeClause();
    }
    // SHOW COLUMNS FROM table [LIKE pattern]
    else if (matchContextual("COLUMNS")) {
        stmt->show_type = ShowStmt::ShowType::COLUMNS;
        expect(TokenType::KW_FROM, "Expected FROM after COLUMNS");
        stmt->from_name = expectIdentifier("Expected table name after FROM");
        parseLikeClause();
    }
    // SHOW INDEXES FROM table
    else if (matchContextual("INDEXES") || matchContextual("INDEX")) {
        // Check if this is "SHOW INDEX name" (Firebird) or "SHOW INDEXES FROM table"
        if (check(TokenType::KW_FROM)) {
            stmt->show_type = ShowStmt::ShowType::INDEXES;
            advance();  // consume FROM
            stmt->from_name = expectIdentifier("Expected table name after FROM");
        } else if (check(TokenType::IDENTIFIER) || check(TokenType::STRING_LITERAL)) {
            // SHOW INDEX name - Firebird style
            stmt->show_type = ShowStmt::ShowType::INDEX;
            stmt->name = expectIdentifier("Expected index name");
        } else {
            // SHOW INDEXES (list all)
            stmt->show_type = ShowStmt::ShowType::INDEXES;
        }
    }
    // SHOW CREATE TABLE name
    else if (match(TokenType::KW_CREATE) || matchContextual("CREATE")) {
        expectContextual("TABLE", "Expected TABLE after CREATE");
        stmt->show_type = ShowStmt::ShowType::CREATE_TABLE;
        stmt->name = expectIdentifier("Expected table name");
    }
    // SHOW TABLE name - Firebird style detailed table info
    else if (matchContextual("TABLE")) {
        if (check(TokenType::IDENTIFIER)) {
            stmt->show_type = ShowStmt::ShowType::TABLE;
            stmt->name = expectIdentifier("Expected table name");
        } else {
            stmt->show_type = ShowStmt::ShowType::TABLES;
        }
    }
    // SHOW TRIGGER name
    else if (matchContextual("TRIGGER") || matchContextual("TRIGGERS")) {
        stmt->show_type = ShowStmt::ShowType::TRIGGER;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected trigger name");
        }
    }
    // SHOW VIEW name
    else if (matchContextual("VIEW") || matchContextual("VIEWS")) {
        stmt->show_type = ShowStmt::ShowType::VIEW;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected view name");
        }
    }
    // SHOW PROCEDURE name
    else if (matchContextual("PROCEDURE") || matchContextual("PROCEDURES")) {
        stmt->show_type = ShowStmt::ShowType::PROCEDURE;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected procedure name");
        }
    }
    // SHOW FUNCTION name
    else if (matchContextual("FUNCTION") || matchContextual("FUNCTIONS")) {
        stmt->show_type = ShowStmt::ShowType::FUNCTION;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected function name");
        }
    }
    // SHOW DOMAIN name
    else if (matchContextual("DOMAIN") || matchContextual("DOMAINS")) {
        stmt->show_type = ShowStmt::ShowType::DOMAIN;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected domain name");
        }
    }
    // SHOW GENERATOR/SEQUENCE name
    else if (matchContextual("GENERATOR") || matchContextual("GENERATORS") ||
             matchContextual("SEQUENCE") || matchContextual("SEQUENCES")) {
        stmt->show_type = ShowStmt::ShowType::GENERATOR;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected sequence/generator name");
        }
    }
    // SHOW SCHEMA [name]
    else if (matchContextual("SCHEMA") || matchContextual("SCHEMAS")) {
        stmt->show_type = ShowStmt::ShowType::SCHEMA;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected schema name");
        }
    }
    // SHOW ROLE name
    else if (matchContextual("ROLE") || matchContextual("ROLES")) {
        stmt->show_type = ShowStmt::ShowType::ROLE;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected role name");
        }
    }
    // SHOW GRANTS [FOR name]
    else if (matchContextual("GRANTS")) {
        stmt->show_type = ShowStmt::ShowType::GRANTS;
        if (matchContextual("FOR")) {
            stmt->name = expectIdentifier("Expected object name after FOR");
        }
    }
    // SHOW CHECKS table
    else if (matchContextual("CHECKS") || matchContextual("CHECK")) {
        stmt->show_type = ShowStmt::ShowType::CHECKS;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected table name");
        }
    }
    // SHOW COLLATIONS [LIKE pattern]
    else if (matchContextual("COLLATIONS") || matchContextual("COLLATION")) {
        stmt->show_type = ShowStmt::ShowType::COLLATIONS;
        parseLikeClause();
    }
    // SHOW COMMENTS [object_name]
    else if (matchContextual("COMMENTS") || matchContextual("COMMENT")) {
        stmt->show_type = ShowStmt::ShowType::COMMENTS;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected object name");
        }
    }
    // SHOW DEPENDENCIES [object_name]
    else if (matchContextual("DEPENDENCIES") || matchContextual("DEPENDENCY")) {
        stmt->show_type = ShowStmt::ShowType::DEPENDENCIES;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected object name");
        }
    }
    // SHOW PACKAGE name
    else if (matchContextual("PACKAGE") || matchContextual("PACKAGES")) {
        stmt->show_type = ShowStmt::ShowType::PACKAGE;
        stmt->name = expectIdentifier("Expected package name");
    }
    // SHOW SQL DIALECT
    else if (matchContextual("SQL")) {
        expectContextual("DIALECT", "Expected DIALECT after SQL");
        stmt->show_type = ShowStmt::ShowType::SQL_DIALECT;
    }
    // SHOW VERSION
    else if (matchContextual("VERSION")) {
        stmt->show_type = ShowStmt::ShowType::VERSION;
    }
    // SHOW DATABASE
    else if (matchContextual("DATABASE")) {
        stmt->show_type = ShowStmt::ShowType::DATABASE;
    }
    // SHOW SYSTEM
    else if (matchContextual("SYSTEM")) {
        stmt->show_type = ShowStmt::ShowType::SYSTEM;
    }
    // SHOW PARSER VERSION (unsupported)
    else if (matchContextual("PARSER")) {
        expectContextual("VERSION", "Expected VERSION after PARSER");
        error("SHOW PARSER VERSION is not supported");
        stmt->show_type = ShowStmt::ShowType::VARIABLE;
    }
    // Default: SHOW variable_name
    else {
        stmt->show_type = ShowStmt::ShowType::VARIABLE;
        stmt->name = expectIdentifier("Expected variable name or SHOW keyword");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// EXPLAIN Statement
// =============================================================================

ExplainStmt* Parser::parseExplain() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<ExplainStmt>();

    // Parse options: EXPLAIN [ANALYZE] [VERBOSE] [options] query
    // Options can be in parentheses: EXPLAIN (ANALYZE, VERBOSE, COSTS ON) query

    // Check for parenthesized options
    if (check(TokenType::LEFT_PAREN)) {
        advance();
        do {
            if (matchContextual("ANALYZE")) {
                stmt->analyze = true;
            } else if (matchContextual("VERBOSE")) {
                stmt->verbose = true;
            } else if (matchContextual("COSTS")) {
                // COSTS ON/OFF
                if (match(TokenType::KW_ON) || matchContextual("ON")) {
                    stmt->costs = true;
                } else if (matchContextual("OFF")) {
                    stmt->costs = false;
                }
            } else if (matchContextual("BUFFERS")) {
                stmt->buffers = true;
            } else if (matchContextual("TIMING")) {
                // TIMING ON/OFF
                if (match(TokenType::KW_ON) || matchContextual("ON")) {
                    stmt->timing = true;
                } else if (matchContextual("OFF")) {
                    stmt->timing = false;
                }
            } else if (matchContextual("FORMAT")) {
                if (matchContextual("JSON")) {
                    stmt->format_json = true;
                } else if (matchContextual("XML")) {
                    stmt->format_xml = true;
                } else if (matchContextual("YAML")) {
                    stmt->format_yaml = true;
                } else if (matchContextual("TEXT")) {
                    // Default text format
                }
            } else {
                break;  // Unknown option
            }
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after EXPLAIN options");
    } else {
        // Non-parenthesized options (simpler form)
        if (matchContextual("ANALYZE")) {
            stmt->analyze = true;
        }
        if (matchContextual("VERBOSE")) {
            stmt->verbose = true;
        }
    }

    // Parse the statement to explain
    if (match(TokenType::KW_SELECT)) {
        stmt->query = parseSelect();
    } else if (match(TokenType::KW_INSERT)) {
        stmt->query = parseInsert();
    } else if (match(TokenType::KW_UPDATE)) {
        stmt->query = parseUpdate();
    } else if (match(TokenType::KW_DELETE)) {
        stmt->query = parseDelete();
    } else {
        error("Expected SELECT, INSERT, UPDATE, or DELETE after EXPLAIN");
        return nullptr;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// DCL Statement Parsing (GRANT/REVOKE)
// =============================================================================

GrantStmt* Parser::parseGrant() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<GrantStmt>();

    // Parse privileges
    do {
        if (match(TokenType::KW_SELECT) || matchContextual("SELECT")) {
            stmt->privileges.push_back(PrivilegeType::SELECT);
        } else if (match(TokenType::KW_INSERT)) {
            stmt->privileges.push_back(PrivilegeType::INSERT);
        } else if (match(TokenType::KW_UPDATE)) {
            stmt->privileges.push_back(PrivilegeType::UPDATE);
        } else if (match(TokenType::KW_DELETE)) {
            stmt->privileges.push_back(PrivilegeType::DELETE);
        } else if (matchContextual("TRUNCATE")) {
            stmt->privileges.push_back(PrivilegeType::TRUNCATE);
        } else if (matchContextual("REFERENCES")) {
            stmt->privileges.push_back(PrivilegeType::REFERENCES);
        } else if (matchContextual("TRIGGER")) {
            stmt->privileges.push_back(PrivilegeType::TRIGGER);
        } else if (matchContextual("EXECUTE")) {
            stmt->privileges.push_back(PrivilegeType::EXECUTE);
        } else if (matchContextual("USAGE")) {
            stmt->privileges.push_back(PrivilegeType::USAGE);
        } else if (match(TokenType::KW_COPY)) {
            stmt->privileges.push_back(PrivilegeType::COPY);
        } else if (matchContextual("ALL")) {
            matchContextual("PRIVILEGES");  // Optional
            stmt->privileges.push_back(PrivilegeType::ALL);
        } else {
            error("Expected privilege type");
            return nullptr;
        }
    } while (match(TokenType::COMMA));

    // ON object_type
    expect(TokenType::KW_ON, "Expected ON");

    if (matchContextual("TABLE")) {
        stmt->object_type = PrivilegeObjectType::TABLE;
    } else if (matchContextual("SEQUENCE")) {
        stmt->object_type = PrivilegeObjectType::SEQUENCE;
    } else if (matchContextual("FUNCTION")) {
        stmt->object_type = PrivilegeObjectType::FUNCTION;
    } else if (matchContextual("PROCEDURE")) {
        stmt->object_type = PrivilegeObjectType::PROCEDURE;
    } else if (matchContextual("SCHEMA")) {
        stmt->object_type = PrivilegeObjectType::SCHEMA;
    } else if (matchContextual("DATABASE")) {
        stmt->object_type = PrivilegeObjectType::DATABASE;
    } else {
        // Default to TABLE
        stmt->object_type = PrivilegeObjectType::TABLE;
    }

    // Parse object names
    do {
        stmt->objects.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    // TO grantees
    expectContextual("TO", "Expected TO");

    do {
        if (matchContextual("PUBLIC")) {
            stmt->is_public = true;
        } else {
            stmt->grantees.push_back(expectIdentifier("Expected grantee name"));
        }
    } while (match(TokenType::COMMA));

    // WITH GRANT OPTION
    if (match(TokenType::KW_WITH)) {
        expect(TokenType::KW_GRANT, "Expected GRANT after WITH");
        expectContextual("OPTION", "Expected OPTION after GRANT");
        stmt->with_grant_option = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

RevokeStmt* Parser::parseRevoke() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<RevokeStmt>();

    // REVOKE GRANT OPTION FOR?
    if (match(TokenType::KW_GRANT)) {
        expectContextual("OPTION", "Expected OPTION after GRANT");
        expectContextual("FOR", "Expected FOR after OPTION");
        stmt->grant_option_for = true;
    }

    // Parse privileges
    do {
        if (match(TokenType::KW_SELECT) || matchContextual("SELECT")) {
            stmt->privileges.push_back(PrivilegeType::SELECT);
        } else if (match(TokenType::KW_INSERT)) {
            stmt->privileges.push_back(PrivilegeType::INSERT);
        } else if (match(TokenType::KW_UPDATE)) {
            stmt->privileges.push_back(PrivilegeType::UPDATE);
        } else if (match(TokenType::KW_DELETE)) {
            stmt->privileges.push_back(PrivilegeType::DELETE);
        } else if (match(TokenType::KW_COPY)) {
            stmt->privileges.push_back(PrivilegeType::COPY);
        } else if (matchContextual("ALL")) {
            matchContextual("PRIVILEGES");
            stmt->privileges.push_back(PrivilegeType::ALL);
        } else {
            error("Expected privilege type");
            return nullptr;
        }
    } while (match(TokenType::COMMA));

    // ON object_type
    expect(TokenType::KW_ON, "Expected ON");

    if (matchContextual("TABLE")) {
        stmt->object_type = PrivilegeObjectType::TABLE;
    } else if (matchContextual("SEQUENCE")) {
        stmt->object_type = PrivilegeObjectType::SEQUENCE;
    } else if (matchContextual("FUNCTION")) {
        stmt->object_type = PrivilegeObjectType::FUNCTION;
    } else if (matchContextual("PROCEDURE")) {
        stmt->object_type = PrivilegeObjectType::PROCEDURE;
    } else if (matchContextual("SCHEMA")) {
        stmt->object_type = PrivilegeObjectType::SCHEMA;
    } else if (matchContextual("DATABASE")) {
        stmt->object_type = PrivilegeObjectType::DATABASE;
    } else {
        stmt->object_type = PrivilegeObjectType::TABLE;
    }

    // Parse object names
    do {
        stmt->objects.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    // FROM grantees
    expect(TokenType::KW_FROM, "Expected FROM");

    do {
        if (matchContextual("PUBLIC")) {
            stmt->is_public = true;
        } else {
            stmt->grantees.push_back(expectIdentifier("Expected grantee name"));
        }
    } while (match(TokenType::COMMA));

    // CASCADE/RESTRICT
    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    } else {
        matchContextual("RESTRICT");  // Optional, default
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// Connection Statement Parsing
// =============================================================================

ConnectStmt* Parser::parseConnect() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<ConnectStmt>();

    // CONNECT [TO] database
    matchContextual("TO");  // Optional

    stmt->database = expectIdentifier("Expected database name");

    // Optional USER/PASSWORD/ROLE/CHARSET
    while (true) {
        if (matchContextual("USER")) {
            stmt->user = expectIdentifier("Expected user name");
        } else if (matchContextual("PASSWORD")) {
            if (check(TokenType::STRING_LITERAL)) {
                stmt->password = current().value.string_id;
                advance();
            } else {
                stmt->password = expectIdentifier("Expected password");
            }
        } else if (matchContextual("ROLE")) {
            stmt->role = expectIdentifier("Expected role name");
        } else if (matchContextual("CHARSET") || matchContextual("CHARACTER")) {
            if (checkContextual("SET")) {
                matchContextual("SET");
            }
            stmt->charset = expectIdentifier("Expected charset name");
        } else {
            break;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DisconnectStmt* Parser::parseDisconnect() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<DisconnectStmt>();

    if (matchContextual("ALL")) {
        stmt->target = DisconnectStmt::Target::ALL;
    } else if (matchContextual("CURRENT")) {
        stmt->target = DisconnectStmt::Target::CURRENT;
    } else if (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        stmt->target = DisconnectStmt::Target::NAMED;
        stmt->connection_name = expectIdentifier("Expected connection name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// Metadata Statement Parsing (COMMENT)
// =============================================================================

CommentStmt* Parser::parseComment() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<CommentStmt>();

    expect(TokenType::KW_ON, "Expected ON after COMMENT");

    // Parse object type
    if (matchContextual("TABLE")) {
        stmt->object_type = CommentObjectType::TABLE;
        stmt->object_path = parseSchemaPath(state_);
    } else if (matchContextual("COLUMN")) {
        stmt->object_type = CommentObjectType::COLUMN;
        stmt->object_path = parseSchemaPath(state_);
    } else if (matchContextual("INDEX")) {
        stmt->object_type = CommentObjectType::INDEX;
        stmt->object_path = parseSchemaPath(state_);
    } else if (matchContextual("VIEW")) {
        stmt->object_type = CommentObjectType::VIEW;
        stmt->object_path = parseSchemaPath(state_);
    } else if (matchContextual("SEQUENCE")) {
        stmt->object_type = CommentObjectType::SEQUENCE;
        stmt->object_path = parseSchemaPath(state_);
    } else if (matchContextual("FUNCTION")) {
        stmt->object_type = CommentObjectType::FUNCTION;
        stmt->object_path = parseSchemaPath(state_);
    } else if (matchContextual("PROCEDURE")) {
        stmt->object_type = CommentObjectType::PROCEDURE;
        stmt->object_path = parseSchemaPath(state_);
    } else if (matchContextual("TRIGGER")) {
        stmt->object_type = CommentObjectType::TRIGGER;
        stmt->object_path = parseSchemaPath(state_);
    } else if (matchContextual("SCHEMA")) {
        stmt->object_type = CommentObjectType::SCHEMA;
        stmt->object_path = parseSchemaPath(state_);
    } else if (matchContextual("DATABASE")) {
        stmt->object_type = CommentObjectType::DATABASE;
        stmt->object_path = parseSchemaPath(state_);
    } else if (matchContextual("ROLE")) {
        stmt->object_type = CommentObjectType::ROLE;
        stmt->object_path = parseSchemaPath(state_);
    } else if (matchContextual("CONSTRAINT")) {
        stmt->object_type = CommentObjectType::CONSTRAINT;
        stmt->object_path = parseSchemaPath(state_);
    } else {
        error("Expected object type after COMMENT ON");
        return nullptr;
    }

    // IS 'comment' or IS NULL
    expect(TokenType::KW_IS, "Expected IS");

    if (match(TokenType::KW_NULL)) {
        stmt->is_null = true;
    } else if (check(TokenType::STRING_LITERAL)) {
        stmt->comment_text = current().value.string_id;
        advance();
    } else {
        error("Expected string literal or NULL after IS");
        return nullptr;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// MERGE Statement Parsing
// =============================================================================

MergeStmt* Parser::parseMerge() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<MergeStmt>();

    // MERGE INTO target_table [AS alias]
    expect(TokenType::KW_INTO, "Expected INTO after MERGE");
    stmt->target_table = parseSchemaPath(state_);

    if (match(TokenType::KW_AS) || (check(TokenType::IDENTIFIER) && !checkContextual("USING"))) {
        stmt->target_alias = expectIdentifier("Expected target alias");
    }

    // USING source
    expectContextual("USING", "Expected USING");

    if (match(TokenType::LEFT_PAREN)) {
        // Subquery
        stmt->source_query = parseSelect();
        expect(TokenType::RIGHT_PAREN, "Expected ) after subquery");
    } else {
        stmt->source_table = parseSchemaPath(state_);
    }

    // Source alias
    if (match(TokenType::KW_AS) || (check(TokenType::IDENTIFIER) && !checkContextual("ON"))) {
        stmt->source_alias = expectIdentifier("Expected source alias");
    }

    // ON condition
    expect(TokenType::KW_ON, "Expected ON");
    stmt->on_condition = parseExpression();

    // WHEN clauses
    while (match(TokenType::KW_WHEN)) {
        if (matchContextual("MATCHED")) {
            MergeStmt::WhenMatched when;

            // AND condition
            if (match(TokenType::KW_AND)) {
                when.and_condition = parseExpression();
            }

            expect(TokenType::KW_THEN, "Expected THEN");

            if (match(TokenType::KW_UPDATE)) {
                expect(TokenType::KW_SET, "Expected SET after UPDATE");
                // Parse assignments
                do {
                    auto col = expectIdentifier("Expected column name");
                    expect(TokenType::EQUAL, "Expected = in assignment");
                    auto* expr = parseExpression();
                    when.assignments.emplace_back(col, expr);
                } while (match(TokenType::COMMA));
            } else if (match(TokenType::KW_DELETE)) {
                when.is_delete = true;
            } else {
                error("Expected UPDATE or DELETE after THEN");
                return nullptr;
            }

            stmt->when_matched.push_back(std::move(when));

        } else if (match(TokenType::KW_NOT)) {
            expectContextual("MATCHED", "Expected MATCHED after NOT");

            // Check for BY SOURCE (SQL Server extension)
            if (matchContextual("BY")) {
                expectContextual("SOURCE", "Expected SOURCE after BY");
                MergeStmt::WhenNotMatchedBySource when;

                if (match(TokenType::KW_AND)) {
                    when.and_condition = parseExpression();
                }

                expect(TokenType::KW_THEN, "Expected THEN");

                if (match(TokenType::KW_UPDATE)) {
                    expect(TokenType::KW_SET, "Expected SET after UPDATE");
                    do {
                        auto col = expectIdentifier("Expected column name");
                        expect(TokenType::EQUAL, "Expected = in assignment");
                        auto* expr = parseExpression();
                        when.assignments.emplace_back(col, expr);
                    } while (match(TokenType::COMMA));
                } else if (match(TokenType::KW_DELETE)) {
                    when.is_delete = true;
                }

                stmt->when_not_matched_by_source.push_back(std::move(when));

            } else {
                // WHEN NOT MATCHED [BY TARGET] THEN INSERT
                matchContextual("BY");
                matchContextual("TARGET");

                MergeStmt::WhenNotMatched when;

                if (match(TokenType::KW_AND)) {
                    when.and_condition = parseExpression();
                }

                expect(TokenType::KW_THEN, "Expected THEN");
                expect(TokenType::KW_INSERT, "Expected INSERT");

                // Optional column list
                if (match(TokenType::LEFT_PAREN)) {
                    do {
                        when.columns.push_back(expectIdentifier("Expected column name"));
                    } while (match(TokenType::COMMA));
                    expect(TokenType::RIGHT_PAREN, "Expected )");
                }

                // VALUES
                expect(TokenType::KW_VALUES, "Expected VALUES");
                expect(TokenType::LEFT_PAREN, "Expected (");
                do {
                    when.values.push_back(parseExpression());
                } while (match(TokenType::COMMA));
                expect(TokenType::RIGHT_PAREN, "Expected )");

                stmt->when_not_matched.push_back(std::move(when));
            }
        } else {
            error("Expected MATCHED or NOT MATCHED after WHEN");
            return nullptr;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// PSQL Statements (Procedural SQL)
// =============================================================================

Statement* Parser::parsePSQLStatement() {
    ParseModeGuard guard(state_, ParseMode::PSQL);

    if (check(TokenType::KW_WITH)) {
        return parseWithStatement();
    }
    if (check(TokenType::KW_IF)) {
        advance();
        return parseIfStatement();
    }
    if (checkContextual("WHILE")) {
        advance();
        return parseWhileStatement();
    }
    if (checkContextual("FOR")) {
        advance();
        return parseForStatement();
    }
    if (checkContextual("LOOP")) {
        advance();
        return parseLoopStatement();
    }
    if (checkContextual("LEAVE")) {
        advance();
        return parseLeaveStatement();
    }
    if (checkContextual("CONTINUE")) {
        advance();
        return parseContinueStatement();
    }
    if (checkContextual("EXIT")) {
        advance();
        return parseExitStatement();
    }
    if (checkContextual("SUSPEND")) {
        advance();
        return parseSuspendStatement();
    }
    if (match(TokenType::KW_RETURN)) {
        return parseReturnStatement();
    }
    if (checkContextual("EXCEPTION")) {
        advance();
        return parseExceptionStatement();
    }
    if (checkContextual("POST_EVENT")) {
        advance();
        return parsePostEventStatement();
    }
    if (checkContextual("OPEN")) {
        advance();
        return parseOpenCursor();
    }
    if (checkContextual("FETCH")) {
        advance();
        return parseFetchCursor();
    }
    if (checkContextual("CLOSE")) {
        advance();
        return parseCloseCursor();
    }
    if (check(TokenType::KW_BEGIN)) {
        advance();
        return parseBeginEndBlock();
    }
    if (match(TokenType::KW_EXECUTE)) {
        return parseExecuteStatement();
    }
    if (match(TokenType::KW_DECLARE)) {
        if (matchContextual("VARIABLE")) {
            return parseDeclareVariable();
        }
        return parseDeclareCursor();
    }

    // Assignment: variable := expression
    if (isIdentifier()) {
        StringPool::StringId var_name = currentIdentifier();
        if (match(TokenType::COLON_EQUALS)) {
            auto* stmt = arena_.create<AssignmentStmt>();
            stmt->variable = var_name;
            stmt->value = parseExpression();
            return stmt;
        }
        error("Expected := for assignment or statement keyword");
        return nullptr;
    }

    // DML statements
    if (match(TokenType::KW_SELECT)) return parseSelect();
    if (match(TokenType::KW_INSERT)) return parseInsert();
    if (match(TokenType::KW_UPDATE)) return parseUpdate();
    if (match(TokenType::KW_DELETE)) return parseDelete();

    error("Expected PSQL statement");
    return nullptr;
}

Statement* Parser::parseBeginEndBlock() {
    auto* stmt = arena_.create<CompoundStmt>();

    while (!check(TokenType::KW_END) && !isAtEnd()) {
        if (check(TokenType::KW_WHEN)) {
            break;
        }

        Statement* inner = parsePSQLStatement();
        if (inner) {
            stmt->statements.push_back(inner);
        }

        match(TokenType::SEMICOLON);
    }

    while (check(TokenType::KW_WHEN)) {
        advance();
        Statement* handler = parseWhenStatement();
        if (handler) {
            stmt->exception_handlers.push_back(handler);
        }
    }

    expect(TokenType::KW_END, "Expected END");
    return stmt;
}

Statement* Parser::parseIfStatement() {
    auto* stmt = arena_.create<IfStmt>();

    expect(TokenType::LEFT_PAREN, "Expected '(' after IF");
    stmt->condition = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after condition");

    expect(TokenType::KW_THEN, "Expected THEN after IF condition");

    if (check(TokenType::KW_BEGIN)) {
        advance();
        stmt->then_branch = parseBeginEndBlock();
    } else {
        stmt->then_branch = parsePSQLStatement();
    }

    if (match(TokenType::KW_ELSE)) {
        if (check(TokenType::KW_IF)) {
            advance();
            stmt->else_branch = parseIfStatement();
        } else if (check(TokenType::KW_BEGIN)) {
            advance();
            stmt->else_branch = parseBeginEndBlock();
        } else {
            stmt->else_branch = parsePSQLStatement();
        }
    }

    return stmt;
}

Statement* Parser::parseWhileStatement() {
    auto* stmt = arena_.create<WhileStmt>();

    expect(TokenType::LEFT_PAREN, "Expected '(' after WHILE");
    stmt->condition = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after condition");

    if (!matchContextual("DO")) {
        error("Expected DO after WHILE condition");
    }

    if (check(TokenType::KW_BEGIN)) {
        advance();
        stmt->body = parseBeginEndBlock();
    } else {
        stmt->body = parsePSQLStatement();
    }

    return stmt;
}

Statement* Parser::parseForStatement() {
    if (check(TokenType::KW_SELECT)) {
        advance();
        auto* stmt = arena_.create<ForSelectStmt>();
        stmt->select_stmt = parseSelect();

        if (!match(TokenType::KW_INTO)) {
            error("Expected INTO after FOR SELECT");
        }
        do {
            stmt->into_variables.push_back(expectIdentifier("Expected variable name"));
        } while (match(TokenType::COMMA));

        if (!matchContextual("DO")) {
            error("Expected DO after INTO variables");
        }

        if (check(TokenType::KW_BEGIN)) {
            advance();
            stmt->body = parseBeginEndBlock();
        } else {
            stmt->body = parsePSQLStatement();
        }

        return stmt;
    }

    if (check(TokenType::KW_EXECUTE)) {
        advance();
        auto* stmt = arena_.create<ForExecuteStmt>();

        if (!matchContextual("STATEMENT")) {
            error("Expected STATEMENT after EXECUTE");
        }
        stmt->sql = parseExpression();

        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    stmt->parameters.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after parameters");
        }

        if (match(TokenType::KW_INTO) || matchContextual("INTO")) {
            do {
                stmt->into_variables.push_back(expectIdentifier("Expected variable name"));
            } while (match(TokenType::COMMA));
        }

        if (!matchContextual("DO")) {
            error("Expected DO");
        }

        if (check(TokenType::KW_BEGIN)) {
            advance();
            stmt->body = parseBeginEndBlock();
        } else {
            stmt->body = parsePSQLStatement();
        }

        return stmt;
    }

    error("Expected SELECT or EXECUTE after FOR");
    return nullptr;
}

Statement* Parser::parseLoopStatement() {
    auto* loop = arena_.create<LoopStmt>();
    auto* body = arena_.create<CompoundStmt>();

    while (!isAtEnd()) {
        if (check(TokenType::KW_END)) {
            advance();
            if (matchContextual("LOOP")) {
                break;
            }
            error("Expected LOOP after END");
        }

        Statement* inner = parsePSQLStatement();
        if (inner) {
            body->statements.push_back(inner);
        }
        match(TokenType::SEMICOLON);
    }

    loop->body = body;
    return loop;
}

Statement* Parser::parseLeaveStatement() {
    auto* stmt = arena_.create<LeaveStmt>();
    if (isIdentifier()) {
        stmt->label = currentIdentifier();
    }
    return stmt;
}

Statement* Parser::parseContinueStatement() {
    auto* stmt = arena_.create<ContinueStmt>();
    if (isIdentifier()) {
        stmt->label = currentIdentifier();
    }
    return stmt;
}

Statement* Parser::parseExitStatement() {
    return arena_.create<ExitStmt>();
}

Statement* Parser::parseSuspendStatement() {
    return arena_.create<SuspendStmt>();
}

Statement* Parser::parseReturnStatement() {
    auto* stmt = arena_.create<ReturnStmt>();

    if (!check(TokenType::SEMICOLON) && !check(TokenType::KW_END)) {
        stmt->value = parseExpression();
    }

    return stmt;
}

Statement* Parser::parseExceptionStatement() {
    auto* stmt = arena_.create<ExceptionRaiseStmt>();
    stmt->exception_name = expectIdentifier("Expected exception name");

    if (!check(TokenType::SEMICOLON) && !check(TokenType::KW_END)) {
        stmt->message = parseExpression();
    }

    return stmt;
}

Statement* Parser::parseWhenStatement() {
    auto* stmt = arena_.create<WhenExceptionStmt>();

    if (matchContextual("ANY")) {
        stmt->type = WhenExceptionStmt::ExceptionType::ANY;
    } else if (matchContextual("SQLCODE")) {
        stmt->type = WhenExceptionStmt::ExceptionType::SQLCODE;
        if (check(TokenType::INTEGER_LITERAL)) {
            stmt->sqlcode = static_cast<int32_t>(current().value.int_value);
            advance();
        }
    } else if (matchContextual("GDSCODE")) {
        stmt->type = WhenExceptionStmt::ExceptionType::GDSCODE;
        stmt->gdscode = expectIdentifier("Expected GDSCODE identifier");
    } else if (matchContextual("EXCEPTION")) {
        stmt->type = WhenExceptionStmt::ExceptionType::EXCEPTION;
        stmt->exception_name = expectIdentifier("Expected exception name");
    } else {
        error("Expected ANY, SQLCODE, GDSCODE, or EXCEPTION after WHEN");
        return nullptr;
    }

    if (!matchContextual("DO")) {
        error("Expected DO after WHEN clause");
    }

    if (check(TokenType::KW_BEGIN)) {
        advance();
        stmt->handler = parseBeginEndBlock();
    } else {
        stmt->handler = parsePSQLStatement();
    }

    match(TokenType::SEMICOLON);
    return stmt;
}

Statement* Parser::parseDeclareVariable() {
    auto* stmt = arena_.create<DeclareVariableStmt>();

    stmt->name = expectIdentifier("Expected variable name");
    stmt->type = parseTypeName();

    if (match(TokenType::KW_NOT)) {
        expect(TokenType::KW_NULL, "Expected NULL after NOT");
        stmt->not_null = true;
    }

    if (match(TokenType::EQUAL) || match(TokenType::KW_DEFAULT)) {
        stmt->default_value = parseExpression();
    }

    return stmt;
}

Statement* Parser::parseExecuteStatement() {
    if (matchContextual("BLOCK")) {
        return parseExecuteBlock();
    }
    if (matchContextual("PROCEDURE")) {
        return parseExecuteProcedure();
    }
    if (matchContextual("STATEMENT")) {
        return parseExecuteDynamicStatement();
    }

    error("Expected BLOCK, PROCEDURE, or STATEMENT after EXECUTE");
    return nullptr;
}

ExecuteBlockStmt* Parser::parseExecuteBlock() {
    auto* stmt = arena_.create<ExecuteBlockStmt>();

    if (match(TokenType::LEFT_PAREN)) {
        do {
            VariableDecl param;
            param.name = expectIdentifier("Expected parameter name");
            expect(TokenType::EQUAL, "Expected '=' in parameter");
            param.default_value = parseExpression();
            stmt->input_params.push_back(param);
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after input parameters");
    }

    if (matchContextual("RETURNS")) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after RETURNS");
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                VariableDecl param;
                param.name = expectIdentifier("Expected output parameter name");
                param.type = parseTypeName();
                stmt->output_params.push_back(param);
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after output parameters");
    }

    if (!match(TokenType::KW_AS) && !matchContextual("AS")) {
        error("Expected AS before EXECUTE BLOCK body");
    }

    while (check(TokenType::KW_DECLARE)) {
        advance();
        if (!matchContextual("VARIABLE")) {
            error("Expected VARIABLE after DECLARE");
            break;
        }
        VariableDecl var;
        var.name = expectIdentifier("Expected variable name");
        var.type = parseTypeName();

        if (match(TokenType::KW_NOT)) {
            expect(TokenType::KW_NULL, "Expected NULL after NOT");
            var.not_null = true;
        }

        if (match(TokenType::EQUAL) || match(TokenType::KW_DEFAULT)) {
            var.default_value = parseExpression();
        }

        stmt->variables.push_back(var);
        match(TokenType::SEMICOLON);
    }

    expect(TokenType::KW_BEGIN, "Expected BEGIN");
    stmt->body = parseBeginEndBlock();
    return stmt;
}

ExecuteProcedureStmt* Parser::parseExecuteProcedure() {
    auto* stmt = arena_.create<ExecuteProcedureStmt>();
    stmt->procedure_path = parseSchemaPath(state_);

    if (match(TokenType::LEFT_PAREN)) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                stmt->arguments.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after arguments");
    } else if (!checkContextual("RETURNING") &&
               !check(TokenType::SEMICOLON) && !isAtEnd()) {
        do {
            stmt->arguments.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    if (matchContextual("RETURNING")) {
        if (match(TokenType::KW_VALUES) || matchContextual("VALUES")) {
            // Optional VALUES keyword
        }
        do {
            stmt->returning_variables.push_back(expectIdentifier("Expected variable name"));
        } while (match(TokenType::COMMA));
    }

    return stmt;
}

ExecuteStatementStmt* Parser::parseExecuteDynamicStatement() {
    auto* stmt = arena_.create<ExecuteStatementStmt>();
    stmt->sql = parseExpression();

    if (match(TokenType::LEFT_PAREN)) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                stmt->parameters.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after parameters");
    }

    if (match(TokenType::KW_INTO) || matchContextual("INTO")) {
        do {
            stmt->into_variables.push_back(expectIdentifier("Expected variable name"));
        } while (match(TokenType::COMMA));
    }

    return stmt;
}

DeclareCursorStmt* Parser::parseDeclareCursor() {
    auto* stmt = arena_.create<DeclareCursorStmt>();

    if (matchContextual("CURSOR")) {
        stmt->cursor_name = expectIdentifier("Expected cursor name");
    } else {
        stmt->cursor_name = expectIdentifier("Expected cursor name");
        if (matchContextual("SCROLL")) {
            stmt->scroll = true;
        }
        if (!matchContextual("CURSOR")) {
            error("Expected CURSOR after cursor name");
        }
    }

    if (matchContextual("SCROLL")) {
        stmt->scroll = true;
    }

    if (!matchContextual("FOR")) {
        error("Expected FOR after CURSOR");
    }

    if (!check(TokenType::KW_SELECT) && !check(TokenType::KW_WITH)) {
        error("Expected SELECT after FOR");
    } else {
        stmt->select_stmt = parseSelectWithClause();
    }

    return stmt;
}

OpenCursorStmt* Parser::parseOpenCursor() {
    auto* stmt = arena_.create<OpenCursorStmt>();
    stmt->cursor_name = expectIdentifier("Expected cursor name");
    return stmt;
}

FetchCursorStmt* Parser::parseFetchCursor() {
    auto* stmt = arena_.create<FetchCursorStmt>();

    if (matchContextual("NEXT")) {
        stmt->direction = FetchCursorStmt::Direction::NEXT;
    } else if (matchContextual("PRIOR")) {
        stmt->direction = FetchCursorStmt::Direction::PRIOR;
    } else if (matchContextual("FIRST")) {
        stmt->direction = FetchCursorStmt::Direction::FIRST;
    } else if (matchContextual("LAST")) {
        stmt->direction = FetchCursorStmt::Direction::LAST;
    } else if (matchContextual("ABSOLUTE")) {
        stmt->direction = FetchCursorStmt::Direction::ABSOLUTE;
        stmt->offset = parseExpression();
    } else if (matchContextual("RELATIVE")) {
        stmt->direction = FetchCursorStmt::Direction::RELATIVE;
        stmt->offset = parseExpression();
    }

    stmt->cursor_name = expectIdentifier("Expected cursor name");

    if (match(TokenType::KW_INTO) || matchContextual("INTO")) {
        do {
            stmt->into_variables.push_back(expectIdentifier("Expected variable name"));
        } while (match(TokenType::COMMA));
    }

    return stmt;
}

CloseCursorStmt* Parser::parseCloseCursor() {
    auto* stmt = arena_.create<CloseCursorStmt>();
    stmt->cursor_name = expectIdentifier("Expected cursor name");
    return stmt;
}

PostEventStmt* Parser::parsePostEventStatement() {
    auto* stmt = arena_.create<PostEventStmt>();
    stmt->event_name = parseExpression();
    return stmt;
}

} // namespace scratchbird::parser::v2
