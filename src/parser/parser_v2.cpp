/**
 * ScratchBird Parser v2.0 - Main Parser Implementation
 *
 * See: include/scratchbird/parser/parser_v2.h
 */

#include "scratchbird/parser/parser_v2.h"
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
    if (match(TokenType::KW_CREATE))    return parseCreate();
    if (match(TokenType::KW_ALTER))     return parseAlter();
    if (match(TokenType::KW_DROP))      return parseDrop();
    if (match(TokenType::KW_TRUNCATE))  return parseTruncate();

    // DML statements
    if (match(TokenType::KW_SELECT))    return parseSelect();
    if (match(TokenType::KW_INSERT))    return parseInsert();
    if (match(TokenType::KW_UPDATE))    return parseUpdate();
    if (match(TokenType::KW_DELETE))    return parseDelete();

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

    // Check for TEMPORARY/TEMP
    bool temporary = false;
    if (checkContextual("TEMPORARY") || checkContextual("TEMP")) {
        matchContextual("TEMPORARY") || matchContextual("TEMP");
        temporary = true;
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

    if (matchContextual("TABLE")) {
        auto* stmt = parseCreateTable(or_replace);
        if (stmt) {
            stmt->temporary = temporary;
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

    // TODO: Add more CREATE types
    // if (matchContextual("FUNCTION"))   return parseCreateFunction(or_replace);
    // if (matchContextual("PROCEDURE"))  return parseCreateProcedure(or_replace);
    // if (matchContextual("TRIGGER"))    return parseCreateTrigger();

    error("Expected object type after CREATE (TABLE, INDEX, VIEW, SEQUENCE, ...)");
    return nullptr;
}

// =============================================================================
// CREATE TABLE
// =============================================================================

CreateTableStmt* Parser::parseCreateTable(bool or_replace) {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateTableStmt>();
    stmt->or_replace = or_replace;

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
    while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        if (checkContextual("TABLESPACE")) {
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

    // Type name (contextual keyword)
    if (!isIdentifier()) {
        error("Expected data type name");
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

    // Check for WITH TIME ZONE (WITH is a Gatekeeper keyword)
    if (match(TokenType::KW_WITH)) {
        if (matchContextual("TIME")) {
            expectContextual("ZONE", "Expected ZONE after WITH TIME");
            type.with_time_zone = true;
        }
    } else if (matchContextual("WITHOUT")) {
        matchContextual("TIME");
        matchContextual("ZONE");
        type.with_time_zone = false;
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
        else if (matchContextual("BRIN")) stmt->index_type = IndexType::BRIN;
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

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    stmt->database_path = parseSchemaPath(state_);
    if (stmt->database_path.isEmpty()) {
        error("Expected database name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// ALTER Statements
// =============================================================================

Statement* Parser::parseAlter() {
    ParseModeGuard guard(state_, ParseMode::DDL);

    if (matchContextual("TABLE")) return parseAlterTable();

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
    if (matchContextual("INDEX")) return parse_rename_move(DdlObjectType::INDEX);
    if (matchContextual("SEQUENCE")) return parse_rename_move(DdlObjectType::SEQUENCE);
    if (matchContextual("DOMAIN")) return parse_rename_move(DdlObjectType::DOMAIN);
    if (matchContextual("TRIGGER")) return parse_rename_move(DdlObjectType::TRIGGER);
    if (matchContextual("FUNCTION")) return parse_rename_move(DdlObjectType::FUNCTION);
    if (matchContextual("PROCEDURE")) return parse_rename_move(DdlObjectType::PROCEDURE);
    if (matchContextual("PACKAGE")) return parse_rename_move(DdlObjectType::PACKAGE);
    if (matchContextual("SCHEMA")) return parse_rename_move(DdlObjectType::SCHEMA);
    if (matchContextual("TABLESPACE")) return parse_rename_move(DdlObjectType::TABLESPACE);
    if (matchContextual("ROLE")) return parse_rename_move(DdlObjectType::ROLE);
    if (matchContextual("USER")) return parse_rename_move(DdlObjectType::USER);
    if (matchContextual("GROUP")) return parse_rename_move(DdlObjectType::GROUP);

    error("Expected object type after ALTER");
    return nullptr;
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
        // Look ahead for table.*
        SchemaPath path = parseSchemaPath(state_);

        // parseSchemaPath consumes dots between identifiers, but stops when
        // it sees a non-identifier after a dot. So if current token is STAR,
        // it means the path ended with a dot followed by *
        if (match(TokenType::STAR)) {
            item->item_type = SelectItem::Type::TABLE_STAR;
            item->table_path = std::move(path);
            item->span = makeSpan(start);
            return item;
        }

        // Not table.*, so it's an expression starting with identifier
        item->item_type = SelectItem::Type::EXPRESSION;
        // We need to convert path back to expression
        if (check(TokenType::LEFT_PAREN)) {
            // Function call
            item->expr = parseFunctionCall(std::move(path));
        } else {
            // Column reference
            auto* colRef = arena_.create<ColumnRefExpr>();
            if (path.components.size() == 1) {
                colRef->column.column_name = path.components[0];
            } else {
                colRef->column.column_name = path.objectName();
                colRef->column.has_table_qualifier = true;
                colRef->column.table_path.type = path.type;
                colRef->column.table_path.components = path.schemaComponents();
            }
            item->expr = colRef;
        }
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

        // Check if it's a SELECT (subquery) or just a grouped table reference
        if (check(TokenType::KW_SELECT)) {
            advance();  // consume SELECT
            node->ref_type = TableRefNode::Type::SUBQUERY;
            node->subquery = parseSelect();
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
        if (match(TokenType::KW_SELECT)) {
            node->ref_type = TableRefNode::Type::SUBQUERY;
            node->subquery = parseSelect();
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

    if (match(TokenType::KW_SELECT)) {
        expr->subquery = parseSelect();
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

    if (check(TokenType::KW_SELECT)) {
        advance();
        expr->subquery = parseSelect();
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
    return parseOrExpr();
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
            }
            return expr;
        } else if (matchContextual("ILIKE")) {
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->case_insensitive = true;
            }
            return expr;
        } else if (matchContextual("SIMILAR")) {
            expectContextual("TO", "Expected TO after SIMILAR");
            // SIMILAR TO is treated as LIKE with regex semantics
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
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
        return parseLikeExpr(left);
    }

    // ILIKE (PostgreSQL case-insensitive LIKE)
    if (matchContextual("ILIKE")) {
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->case_insensitive = true;
        }
        return expr;
    }

    // SIMILAR TO
    if (matchContextual("SIMILAR")) {
        expectContextual("TO", "Expected TO after SIMILAR");
        // SIMILAR TO is treated as LIKE with regex semantics
        return parseLikeExpr(left);
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

Expression* Parser::parseMulExpr() {
    Expression* left = parseUnaryExpr();

    while (true) {
        BinaryOp op;
        if (match(TokenType::STAR)) op = BinaryOp::MUL;
        else if (match(TokenType::SLASH)) op = BinaryOp::DIV;
        else if (match(TokenType::PERCENT)) op = BinaryOp::MOD;
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
        if (check(TokenType::KW_SELECT)) {
            // Scalar subquery
            advance();
            auto* subq = arena_.create<SubqueryExpr>();
            subq->subquery = parseSelect();
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

    // Parse arguments
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            expr->arguments.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after function arguments");

    return expr;
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

    // Check for scope
    if (matchContextual("SESSION")) {
        stmt->scope = SetStmt::Scope::SESSION;
        // Could also be SET SESSION AUTHORIZATION
        if (matchContextual("AUTHORIZATION")) {
            stmt->set_type = SetStmt::SetType::SESSION_AUTHORIZATION;
            if (matchContextual("DEFAULT")) {
                stmt->is_default = true;
            } else {
                stmt->value = parseExpression();
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

    if (matchContextual("ROLE")) {
        stmt->set_type = SetStmt::SetType::ROLE;
        if (matchContextual("NONE") || matchContextual("DEFAULT")) {
            stmt->is_default = true;
        } else {
            stmt->value = parseExpression();
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    // SET PARSER VERSION 1|2
    if (matchContextual("PARSER")) {
        expectContextual("VERSION", "Expected VERSION after PARSER");
        stmt->set_type = SetStmt::SetType::PARSER_VERSION;

        // Expect integer 1 or 2
        if (check(TokenType::INTEGER_LITERAL)) {
            int64_t version = current().value.int_value;
            advance();
            if (version == 1 || version == 2) {
                stmt->parser_version = static_cast<uint8_t>(version);
            } else {
                error("Parser version must be 1 or 2");
            }
        } else {
            error("Expected parser version (1 or 2)");
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    // Regular SET name = value / SET name TO value
    stmt->set_type = SetStmt::SetType::VARIABLE;
    stmt->name = expectIdentifier("Expected variable name");

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
    else if (matchContextual("CREATE")) {
        expectContextual("TABLE", "Expected TABLE after CREATE");
        stmt->show_type = ShowStmt::ShowType::CREATE_TABLE;
        stmt->name = expectIdentifier("Expected table name");
    }
    // SHOW TABLE name - Firebird style detailed table info
    else if (matchContextual("TABLE")) {
        stmt->show_type = ShowStmt::ShowType::TABLE;
        stmt->name = expectIdentifier("Expected table name");
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
    // SHOW PARSER VERSION
    else if (matchContextual("PARSER")) {
        expectContextual("VERSION", "Expected VERSION after PARSER");
        stmt->show_type = ShowStmt::ShowType::PARSER_VERSION;
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
        if (matchContextual("SELECT")) {
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
        if (matchContextual("SELECT")) {
            stmt->privileges.push_back(PrivilegeType::SELECT);
        } else if (match(TokenType::KW_INSERT)) {
            stmt->privileges.push_back(PrivilegeType::INSERT);
        } else if (match(TokenType::KW_UPDATE)) {
            stmt->privileges.push_back(PrivilegeType::UPDATE);
        } else if (match(TokenType::KW_DELETE)) {
            stmt->privileges.push_back(PrivilegeType::DELETE);
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

} // namespace scratchbird::parser::v2
