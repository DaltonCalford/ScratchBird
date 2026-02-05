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
 * ScratchBird Parser v2.0 - Main Parser Class
 *
 * This is the main parser implementation using the "Smart Parser, Dumb Lexer"
 * architecture. It uses ParserState for contextual keyword matching and
 * produces an unresolved AST with SchemaPath references.
 *
 * Design:
 * - Recursive descent parser
 * - State-aware dispatch for contextual keywords
 * - Arena-allocated AST nodes
 * - Comprehensive error reporting
 *
 * See: docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md
 */

#include "scratchbird/parser/lexer_v2.h"
#include "scratchbird/parser/parser_state_v2.h"
#include "scratchbird/parser/schema_path_v2.h"
#include "scratchbird/parser/ast_v2.h"
#include <vector>
#include <string>
#include <memory>

namespace scratchbird::parser::v2 {

/**
 * Parser error information
 */
struct ParseError {
    SourceSpan span;
    std::string message;
    std::string hint;
};

/**
 * Parse result containing AST and any errors
 */
class ParseResult {
public:
    ParseResult() = default;

    bool success() const { return errors_.empty() && statement_ != nullptr; }
    Statement* statement() const { return statement_; }
    const std::vector<ParseError>& errors() const { return errors_; }

    void setStatement(Statement* stmt) { statement_ = stmt; }
    void addError(const ParseError& error) { errors_.push_back(error); }
    void addError(SourceSpan span, const std::string& message, const std::string& hint = "") {
        errors_.push_back({span, message, hint});
    }

private:
    Statement* statement_ = nullptr;
    std::vector<ParseError> errors_;
};

/**
 * SQL Parser v2.0
 *
 * Parses SQL text into an unresolved AST using the Gatekeeper keyword model.
 */
class Parser {
public:
    /**
     * Create parser for given input
     * @param input SQL source text
     */
    explicit Parser(std::string_view input);
    ~Parser();

    // Non-copyable
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    /**
     * Parse a single SQL statement
     */
    ParseResult parseStatement();

    /**
     * Parse a PSQL body (DECLARE...BEGIN...END or single PSQL statement)
     */
    ParseResult parsePsqlBody();

    /**
     * Parse multiple statements (semicolon-separated)
     */
    std::vector<ParseResult> parseStatements();

    /**
     * Access to string pool for identifier lookup
     */
    StringPool& stringPool() { return lexer_.stringPool(); }
    const StringPool& stringPool() const { return lexer_.stringPool(); }

    /**
     * Access to AST arena
     */
    ASTArena& arena() { return arena_; }

    /**
     * Get parse errors
     */
    const std::vector<ParseError>& errors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }

private:
    Lexer lexer_;
    ParserState state_;
    ASTArena arena_;
    std::vector<ParseError> errors_;

    // Error handling
    void error(const std::string& message);
    void error(const std::string& message, const std::string& hint);
    void errorAt(SourceSpan span, const std::string& message, const std::string& hint = "");

    // Synchronization for error recovery
    void synchronize();

    // ==========================================================================
    // Statement Dispatch
    // ==========================================================================

    Statement* parseStatementInternal();

    // DDL statements
    Statement* parseCreate();
    Statement* parseAlter();
    Statement* parseDrop();
    Statement* parseTruncate();

    // CREATE statements
    CreateTableStmt* parseCreateTable(bool or_replace = false,
                                      TempTableType temp_type = TempTableType::NONE);
    CreateIndexStmt* parseCreateIndex();
    CreateViewStmt* parseCreateView(bool or_replace = false);
    CreateSequenceStmt* parseCreateSequence();
    CreateSchemaStmt* parseCreateSchema();
    CreateDatabaseStmt* parseCreateDatabase();
    CreateTablespaceStmt* parseCreateTablespace();
    CreateDomainStmt* parseCreateDomain();
    CreateFunctionStmt* parseCreateFunction(bool or_replace = false);
    CreateProcedureStmt* parseCreateProcedure(bool or_replace = false);
    CreateTriggerStmt* parseCreateTrigger(bool or_replace = false);
    CreatePackageStmt* parseCreatePackage(bool or_replace = false);
    CreateExceptionStmt* parseCreateException(bool or_replace = false);
    CreateTypeStmt* parseCreateType(bool or_replace = false);
    CreateUserStmt* parseCreateUser();
    CreateRoleStmt* parseCreateRole();
    CreateGroupStmt* parseCreateGroup();
    CreatePolicyStmt* parseCreatePolicy();
    CreateForeignServerStmt* parseCreateForeignServer();
    CreateForeignTableStmt* parseCreateForeignTable();
    CreateUserMappingStmt* parseCreateUserMapping();
    CreateSynonymStmt* parseCreateSynonym(bool is_public = false);
    CreateUdrStmt* parseCreateUdr();
    CreateJobStmt* parseCreateJob(bool or_alter = false, bool recreate = false);

    // ALTER statements
    AlterTableStmt* parseAlterTable();
    AlterSchemaStmt* parseAlterSchema();
    AlterDatabaseStmt* parseAlterDatabase();
    Statement* parseAlterTablespace();
    AlterTypeStmt* parseAlterType();
    AlterDomainStmt* parseAlterDomain();
    AlterJobStmt* parseAlterJob();
    AlterPolicyStmt* parseAlterPolicy();
    AlterSystemStmt* parseAlterSystem();

    // DROP statements
    DropTableStmt* parseDropTable();
    DropIndexStmt* parseDropIndex();
    DropViewStmt* parseDropView();
    DropSchemaStmt* parseDropSchema();
    DropDatabaseStmt* parseDropDatabase();
    DropTablespaceStmt* parseDropTablespace();
    AttachTablespaceStmt* parseAttachTablespace(const StringPool::StringId tablespace_name);
    DetachTablespaceStmt* parseDetachTablespace(const StringPool::StringId tablespace_name);
    DropDomainStmt* parseDropDomain();
    DropFunctionStmt* parseDropFunction();
    DropProcedureStmt* parseDropProcedure();
    DropTriggerStmt* parseDropTrigger();
    DropPackageStmt* parseDropPackage();
    DropRoleStmt* parseDropRole();
    DropGroupStmt* parseDropGroup();
    DropPolicyStmt* parseDropPolicy();
    DropExceptionStmt* parseDropException();
    DropSequenceStmt* parseDropSequence();
    DropTypeStmt* parseDropType();
    DropJobStmt* parseDropJob();
    DropForeignServerStmt* parseDropForeignServer();
    DropForeignTableStmt* parseDropForeignTable();
    DropUserMappingStmt* parseDropUserMapping();
    DropSynonymStmt* parseDropSynonym();
    DropUdrStmt* parseDropUdr();

    // TRUNCATE statement
    TruncateTableStmt* parseTruncateTable();

    // ==========================================================================
    // DML Statements
    // ==========================================================================

    // WITH clause
    Statement* parseWithStatement();
    WithClause* parseWithClause();
    SelectStmt* parseSelectWithClause();

    // SELECT statement
    SelectStmt* parseSelect();
    void parseSelectList(SelectStmt* stmt);
    SelectItem* parseSelectItem();
    void parseFromClause(SelectStmt* stmt);
    TableRefNode* parseTableRef();
    JoinNode* parseJoin(TableRefNode* left);
    JoinType parseJoinType();
    void parseWhereClause(SelectStmt* stmt);
    void parseGroupByClause(SelectStmt* stmt);
    void parseHavingClause(SelectStmt* stmt);
    void parseOrderByClause(SelectStmt* stmt);
    OrderByItem* parseOrderByItem();
    void parseLimitClause(SelectStmt* stmt);
    void parseSetOperation(SelectStmt* stmt);
    WindowSpec* parseWindowSpec();
    void parseWindowFrame(WindowSpec* spec);
    FrameBoundType parseWindowFrameBound(Expression** value_out);

    // INSERT statement
    InsertStmt* parseInsert();
    void parseInsertColumns(InsertStmt* stmt);
    void parseValuesClause(InsertStmt* stmt);
    void parseOnConflict(InsertStmt* stmt);

    // UPDATE statement
    UpdateStmt* parseUpdate();
    void parseSetClause(UpdateStmt* stmt);

    // DELETE statement
    DeleteStmt* parseDelete();

    // COPY statement
    CopyStmt* parseCopy();

    // RETURNING clause (shared by INSERT, UPDATE, DELETE)
    void parseReturningClause(std::vector<SelectItem*>& returning);

    // ==========================================================================
    // Transaction Statements
    // ==========================================================================

    StartTransactionStmt* parseStartTransaction();
    PrepareTransactionStmt* parsePrepareTransaction();
    CommitStmt* parseCommit();
    RollbackStmt* parseRollback();
    SavepointStmt* parseSavepoint();
    ReleaseSavepointStmt* parseReleaseSavepoint();

    // ==========================================================================
    // Session Statements
    // ==========================================================================

    SetStmt* parseSet();
    ResetStmt* parseReset();
    ShowStmt* parseShow();
    ShowStmt* parseDescribe();
    ExplainStmt* parseExplain();
    AnalyzeStmt* parseAnalyze();
    SweepDatabaseStmt* parseSweep();
    ExecuteJobStmt* parseExecuteJob();
    CancelJobRunStmt* parseCancelJobRun();
    ExecuteProcedureStmt* parseCall();

    // ==========================================================================
    // DCL Statements (Data Control Language)
    // ==========================================================================

    GrantStmt* parseGrant();
    RevokeStmt* parseRevoke();

    // ==========================================================================
    // Connection Statements
    // ==========================================================================

    ConnectStmt* parseConnect();
    DisconnectStmt* parseDisconnect();

    // ==========================================================================
    // Metadata Statements
    // ==========================================================================

    CommentStmt* parseComment();

    // ==========================================================================
    // MERGE Statement
    // ==========================================================================

    MergeStmt* parseMerge();

    // ==========================================================================
    // PSQL Statements (Procedural SQL)
    // ==========================================================================

    Statement* parsePSQLStatement();
    Statement* parseBeginEndBlock();
    Statement* parseIfStatement();
    Statement* parseWhileStatement();
    Statement* parseForStatement();
    Statement* parseLoopStatement();
    Statement* parseLeaveStatement();
    Statement* parseContinueStatement();
    Statement* parseExitStatement();
    Statement* parseCaseStatement();
    Statement* parseSuspendStatement();
    Statement* parseReturnStatement();
    Statement* parseExceptionStatement();
    Statement* parseWhenStatement();
    Statement* parseDeclareVariable();
    Statement* parseExecuteStatement();
    ExecuteBlockStmt* parseExecuteBlock();
    ExecuteProcedureStmt* parseExecuteProcedure();
    ExecuteStatementStmt* parseExecuteDynamicStatement();
    DeclareCursorStmt* parseDeclareCursor();
    OpenCursorStmt* parseOpenCursor();
    FetchCursorStmt* parseFetchCursor();
    CloseCursorStmt* parseCloseCursor();
    PostEventStmt* parsePostEventStatement();

    // ==========================================================================
    // Column and Type Parsing
    // ==========================================================================

    ColumnDef* parseColumnDef();
    TypeName parseTypeName();
    std::vector<ColumnConstraint> parseColumnConstraints();
    std::vector<DomainConstraint> parseDomainConstraints();
    void parseDomainIntegrityBlock(CreateDomainStmt* stmt);
    void parseDomainSecurityBlock(CreateDomainStmt* stmt);
    void parseDomainValidationBlock(CreateDomainStmt* stmt);
    void parseDomainQualityBlock(CreateDomainStmt* stmt);
    void parseDomainOptionsBlock(CreateDomainStmt* stmt);
    std::string extractExpressionText(Expression* expr);
    std::string captureStatementBody();
    ColumnConstraint parseColumnConstraint();

    // ==========================================================================
    // Table Constraint Parsing
    // ==========================================================================

    TableConstraint* parseTableConstraint();
    void parsePrimaryKeyConstraint(TableConstraint* constraint);
    void parseUniqueConstraint(TableConstraint* constraint);
    void parseForeignKeyConstraint(TableConstraint* constraint);
    void parseCheckConstraint(TableConstraint* constraint);
    ForeignKeyAction parseForeignKeyAction();

    // ==========================================================================
    // Expression Parsing
    // ==========================================================================

    Expression* parseExpression();
    Expression* parseExpressionWithLeft(Expression* left);
    Expression* parseOrExpr();
    Expression* parseOrExprWithLeft(Expression* left);
    Expression* parseAndExpr();
    Expression* parseAndExprWithLeft(Expression* left);
    Expression* parseNotExpr();
    Expression* parseComparisonExpr();
    Expression* parseComparisonExprWithLeft(Expression* left);
    Expression* parseBitOrExpr();
    Expression* parseBitOrExprWithLeft(Expression* left);
    Expression* parseBitAndExpr();
    Expression* parseBitAndExprWithLeft(Expression* left);
    Expression* parseShiftExpr();
    Expression* parseShiftExprWithLeft(Expression* left);
    Expression* parseAddExpr();
    Expression* parseAddExprWithLeft(Expression* left);
    Expression* parseMulExpr();
    Expression* parseMulExprWithLeft(Expression* left);
    Expression* parsePowerExpr();
    Expression* parseUnaryExpr();
    Expression* parsePrimaryExpr();
    Expression* parseLiteral();
    Expression* parseFunctionCall(SchemaPath path);
    Expression* parseExtractExpr();
    Expression* parseAlterElementExpr();
    ElementSelector parseElementSelector();
    Expression* parseParenExpr();
    Expression* parseCastExpr();
    Expression* parseCaseExpr();
    Expression* parseExistsExpr();
    Expression* parseInExpr(Expression* left);
    Expression* parseBetweenExpr(Expression* left);
    Expression* parseLikeExpr(Expression* left);
    Expression* parseIsNullExpr(Expression* left);
    Expression* parseArrayExpr();

    // ==========================================================================
    // Utility Methods
    // ==========================================================================

    // Token helpers
    bool check(TokenType type) const { return state_.check(type); }
    bool match(TokenType type) { return state_.match(type); }
    void advance() { state_.advance(); }
    Token current() const { return state_.current(); }
    Token previous() const { return state_.previous(); }
    bool isAtEnd() const { return state_.isAtEnd(); }

    // Contextual keyword helpers
    bool checkContextual(const char* keyword) const { return state_.checkContextual(keyword); }
    bool matchContextual(const char* keyword) { return state_.matchContextual(keyword); }

    // Expect methods (consume or error)
    bool expect(TokenType type, const std::string& message);
    bool expectContextual(const char* keyword, const std::string& message);

    // Identifier helpers
    bool isIdentifier() const { return state_.isIdentifier(); }
    StringPool::StringId currentIdentifier();
    StringPool::StringId expectIdentifier(const std::string& message);

    // Source location helpers
    SourceLocation currentLocation() const { return state_.currentLocation(); }
    SourceSpan makeSpan(SourceLocation start) const;

    // List parsing helpers
    template<typename T, typename ParseFunc>
    std::vector<T> parseCommaSeparatedList(ParseFunc parseItem);
};

/**
 * Convenience function to parse SQL
 */
inline ParseResult parseSQL(std::string_view sql) {
    Parser parser(sql);
    return parser.parseStatement();
}

} // namespace scratchbird::parser::v2
