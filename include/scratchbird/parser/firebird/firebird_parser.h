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
 * Firebird SQL Parser
 *
 * Parser for Firebird 5.0 SQL syntax. Produces AST v3 nodes that can be
 * processed by the V3 emitter and SBLR v3 container pipeline.
 *
 * Key Firebird-specific syntax:
 * - FIRST/SKIP instead of LIMIT/OFFSET
 * - RETURNING clause on INSERT/UPDATE/DELETE
 * - CREATE OR ALTER, RECREATE statements
 * - EXECUTE BLOCK
 * - PSQL features (stored procedures, triggers, functions)
 * - Generator/Sequence syntax
 * - Exception handling (EXCEPTION, WHEN)
 *
 * Reference: Firebird 5.0 Language Reference
 */

#include "scratchbird/parser/firebird/firebird_lexer.h"
#include "scratchbird/parser/ast_v3.h"
#include <memory>
#include <vector>
#include <functional>

namespace scratchbird::parser::firebird {

using namespace scratchbird::parser::v3;

/**
 * Parser error information
 */
struct ParseError {
    SourceLocation location;
    std::string message;
    std::string hint;

    std::string format() const;
};

/**
 * Parser error reporter
 */
class ParserErrorReporter {
public:
    virtual ~ParserErrorReporter() = default;

    virtual void reportError(const ParseError& error) = 0;
    virtual bool hasErrors() const = 0;
    virtual size_t errorCount() const = 0;
};

/**
 * Simple error collector
 */
class SimpleParserErrorReporter : public ParserErrorReporter {
public:
    void reportError(const ParseError& error) override;
    bool hasErrors() const override { return !errors_.empty(); }
    size_t errorCount() const override { return errors_.size(); }

    const std::vector<ParseError>& errors() const { return errors_; }
    void clear() { errors_.clear(); }

private:
    std::vector<ParseError> errors_;
};

/**
 * Parse result containing AST and any errors
 */
struct ParseResult {
    std::unique_ptr<Statement> statement;
    bool success = false;
    std::vector<ParseError> errors;
};

/**
 * Firebird SQL Parser
 *
 * Parses Firebird SQL syntax and produces AST v3 nodes.
 */
class Parser {
public:
    /**
     * Create parser for given input
     * @param input SQL source text (must remain valid for parser lifetime)
     * @param dialect SQL dialect (default: Dialect 3)
     */
    explicit Parser(std::string_view input, SQLDialect dialect = SQLDialect::DIALECT_3);
    ~Parser();

    // Non-copyable, non-movable
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;
    Parser(Parser&&) = delete;
    Parser& operator=(Parser&&) = delete;

    /**
     * Parse a single statement
     */
    ParseResult parseStatement();

    /**
     * Parse all statements in the input
     */
    std::vector<ParseResult> parseAll();

    /**
     * String pool for AST nodes
     */
    v3::StringPool& stringPool() { return string_pool_; }
    const v3::StringPool& stringPool() const { return string_pool_; }

    /**
     * Error reporting
     */
    void setErrorReporter(ParserErrorReporter* reporter) { error_reporter_ = reporter; }
    ParserErrorReporter* errorReporter() const { return error_reporter_; }

    /**
     * SQL dialect
     */
    SQLDialect dialect() const { return lexer_.dialect(); }
    void setDialect(SQLDialect dialect) { lexer_.setDialect(dialect); }

private:
    // Lexer
    Lexer lexer_;
    Token current_token_;
    Token lookahead_token_;
    bool has_lookahead_ = false;

    // String pool for AST (separate from lexer's pool)
    v3::StringPool string_pool_;

    // Error reporting
    ParserErrorReporter* error_reporter_ = nullptr;

    // Token navigation
    Token& current() { return current_token_; }
    Token peek();
    void advance();
    bool atEnd() const { return current_token_.type == TokenType::END_OF_FILE; }

    // Token matching
    bool check(TokenType type) const { return current_token_.type == type; }
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);
    bool checkKeyword(TokenType type) const;
    bool matchKeyword(TokenType type);

    // Error handling
    void error(const std::string& message, const std::string& hint = "");
    void synchronize();

    // Convert Firebird StringPool ID to v3 StringPool ID
    v3::StringPool::StringId internFromLexer(StringPool::StringId lexer_id);

    // Get text from current token
    std::string_view currentText();

    // =========================================================================
    // Statement Parsing
    // =========================================================================

    Statement* parseStatementInternal();

    // DDL Statements
    Statement* parseCreateStatement();
    Statement* parseAlterStatement();
    Statement* parseDropStatement();
    Statement* parseRecreateStatement();
    Statement* parseDeclareStatement();
    Statement* parseDeclareFilter();
    Statement* parseDeclareExternalFunction();

    // DDL - CREATE variants
    Statement* parseCreateDatabase();
    Statement* parseCreateTable();
    Statement* parseCreateOrAlterTable();
    Statement* parseCreateIndex();
    Statement* parseCreateView();
    Statement* parseCreateSequence();    // CREATE GENERATOR/SEQUENCE
    Statement* parseCreateProcedure(bool or_replace);
    Statement* parseCreateFunction(bool or_replace);
    Statement* parseCreateTrigger(bool or_replace);
    Statement* parseCreateDomain();
    Statement* parseCreateException(bool or_replace);
    Statement* parseCreateRole(bool or_replace);
    Statement* parseCreatePackage(bool or_replace);
    Statement* parseCreateCollationStatement();
    Statement* parseCreateShadowStatement();
    Statement* parseCreateMappingStatement();
    Statement* parseCreateUserStatement();
    Statement* parseCreateSchemaStatement();

    // DDL - DROP variants
    Statement* parseDropDatabase();
    Statement* parseDropCollationStatement(bool if_exists);
    Statement* parseDropShadowStatement(bool if_exists);
    Statement* parseDropMappingStatement(bool if_exists);
    Statement* parseDropUserStatement(bool if_exists);
    Statement* parseDropSchemaStatement(bool if_exists);

    // DDL - ALTER variants
    Statement* parseAlterDatabase();
    Statement* parseAlterExternalFunctionStatement();
    Statement* parseAlterRoleStatement();
    Statement* parseAlterUserStatement();
    Statement* parseAlterSchemaStatement();

    // DML Statements
    Statement* parseSelectStatement();
    Statement* parseInsertStatement();
    Statement* parseUpdateStatement();
    Statement* parseDeleteStatement();
    Statement* parseMergeStatement();
    Statement* parseUpdateOrInsertStatement();
    Statement* parseExecuteProcedure();
    Statement* parseExecuteBlock();

    // Transaction Statements
    Statement* parseSetTransaction();
    Statement* parseCommit();
    Statement* parseRollback();
    Statement* parseSavepoint();
    Statement* parseReleaseSavepoint();

    // Session Statements
    Statement* parseSetStatement();
    Statement* parseShowStatement();

    // DCL Statements
    Statement* parseGrantStatement();
    Statement* parseRevokeStatement();

    // Metadata Statements
    Statement* parseCommentStatement();

    // =========================================================================
    // Expression Parsing
    // =========================================================================

    Expression* parseExpression();
    Expression* parseOrExpression();
    Expression* parseAndExpression();
    Expression* parseNotExpression();
    Expression* parseComparisonExpression();
    Expression* parseAddExpression();
    Expression* parseMultExpression();
    Expression* parseUnaryExpression();
    Expression* parsePrimaryExpression();

    // Expression helpers
    Expression* parseFunctionCall(const SchemaPath& name);
    Expression* parseIifExpression();
    Expression* parseCaseExpression();
    Expression* parseCastExpression();
    Expression* parseExtractExpression();
    Expression* parseAlterElementExpression();
    Expression* parseSubqueryExpression();
    Expression* parseExistsExpression();
    Expression* parseInExpression(Expression* left);
    Expression* parseBetweenExpression(Expression* left);
    Expression* parseLikeExpression(Expression* left, LikeMatchKind kind);
    Expression* parseIsNullExpression(Expression* left);
    Expression* parseArrayExpression();
    ElementSelector parseElementSelector();
    WindowSpec* parseWindowSpec();
    void parseWindowFrame(WindowSpec* spec);
    FrameBoundType parseWindowFrameBound(Expression** value_out);

    // =========================================================================
    // Type Parsing
    // =========================================================================

    TypeName parseTypeName();
    TypeName parseFirebirdType();  // Firebird-specific types

    // =========================================================================
    // Clause Parsing
    // =========================================================================

    // SELECT clauses
    std::vector<SelectItem*> parseSelectList();
    SelectItem* parseSelectItem();
    TableRefNode* parseFromClause();
    JoinNode* parseJoinClause();
    Expression* parseWhereClause();
    std::vector<Expression*> parseGroupByClause();
    Expression* parseHavingClause();
    std::vector<OrderByItem*> parseOrderByClause();

    // Firebird-specific: FIRST/SKIP
    struct FirstSkip {
        Expression* first = nullptr;
        Expression* skip = nullptr;
    };
    FirstSkip parseFirstSkip();

    // RETURNING clause
    std::vector<SelectItem*> parseReturningClause();

    // Column and table references
    ColumnDef* parseColumnDef();
    std::vector<ColumnConstraint> parseColumnConstraints();
    TableConstraint* parseTableConstraint();
    SchemaPath parseTableReference();
    SchemaPath parseSchemaPath();
    Expression* parseBlobFilterSubtypeLiteral();
    bool parseOptionalIfNotExists();
    ExecuteProcedureStmt* makeExecuteProcedureCall(std::string_view procedure_name);
    LiteralExpr* makeStringLiteralExpr(v3::StringPool::StringId value);
    LiteralExpr* makeBooleanLiteralExpr(bool value);
    void skipRemainingStatementTokens();

    // Extract raw expression text from parsed expression span
    std::string extractExpressionText(Expression* expr);

    // =========================================================================
    // PSQL Parsing (Procedural SQL)
    // =========================================================================

    // Block parsing
    Statement* parsePSQLBlock();
    Statement* parseBeginEndBlock();

    // Variable declarations
    Statement* parseDeclareVariable();
    Statement* parseDeclareSection();

    // Control flow
    Statement* parseIfStatement();
    Statement* parseWhileStatement();
    Statement* parseForStatement();
    Statement* parseLoopStatement();
    Statement* parseLeaveStatement();
    Statement* parseContinueStatement();
    Statement* parseExitStatement();
    Statement* parseSuspendStatement();
    Statement* parseReturnStatement();

    // Exception handling
    Statement* parseWhenStatement();
    Statement* parseExceptionStatement();
    Statement* parsePostEventStatement();

    // Cursor operations
    Statement* parseDeclareCursor();
    Statement* parseOpenCursor();
    Statement* parseFetchCursor();
    Statement* parseCloseCursor();

    // Dynamic SQL
    Statement* parseExecuteStatement();

    // =========================================================================
    // Utility Methods
    // =========================================================================

    // Check if token is a statement-starting keyword
    bool isStatementStart() const;

    // Check if token is an expression-starting token
    bool isExpressionStart() const;

    // Check if token is a Firebird type name keyword
    bool isFirebirdTypeName() const;

    // Check if current token is a non-reserved keyword (can be used as identifier)
    bool isNonReservedKeyword() const;

    // Match a non-reserved keyword by text (case-insensitive)
    bool matchIdentifierText(const char* keyword);

    // Parse an identifier (or non-reserved keyword used as identifier)
    v3::StringPool::StringId parseIdentifier();

    // Capture raw SQL body text for procedures/functions/triggers
    std::string captureStatementBody();

    // Allocate AST node (for future arena allocation)
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        return new T(std::forward<Args>(args)...);
    }

private:
    // DDL implementation helpers
    v3::CreateTableStmt* parseCreateTableImpl(bool or_replace, bool temporary, bool global_temp);
    v3::CreateIndexStmt* parseCreateIndexImpl(bool unique, bool descending);
    v3::CreateViewStmt* parseCreateViewImpl(bool or_replace);
    v3::CreateSequenceStmt* parseCreateSequenceImpl();
    v3::AlterTableStmt* parseAlterTableImpl();
    Statement* parseAlterDomainImpl();
    Statement* parseAlterIndexImpl();
    Statement* parseAlterRenameMoveImpl(v3::DdlObjectType object_type);
    v3::DropTableStmt* parseDropTableImpl(bool if_exists);
    v3::DropIndexStmt* parseDropIndexImpl(bool if_exists);
    v3::DropViewStmt* parseDropViewImpl(bool if_exists);
    v3::DropDomainStmt* parseDropDomainImpl(bool if_exists);
    Statement* parseDropSequenceImpl(bool if_exists);
    Statement* parseDropFunctionImpl(bool if_exists);
    Statement* parseDropProcedureImpl(bool if_exists);
    Statement* parseDropTriggerImpl(bool if_exists);
    Statement* parseDropPackageImpl(bool if_exists);
    Statement* parseDropRoleImpl(bool if_exists);
    Statement* parseDropExceptionImpl(bool if_exists);
};

} // namespace scratchbird::parser::firebird
