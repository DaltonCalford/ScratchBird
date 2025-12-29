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
    CreateTableStmt* parseCreateTable(bool or_replace = false);
    CreateIndexStmt* parseCreateIndex();
    CreateViewStmt* parseCreateView(bool or_replace = false);
    CreateSequenceStmt* parseCreateSequence();

    // ALTER statements
    AlterTableStmt* parseAlterTable();

    // DROP statements
    DropTableStmt* parseDropTable();
    DropIndexStmt* parseDropIndex();
    DropViewStmt* parseDropView();

    // TRUNCATE statement
    TruncateTableStmt* parseTruncateTable();

    // ==========================================================================
    // DML Statements
    // ==========================================================================

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
    ExplainStmt* parseExplain();

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
    // Column and Type Parsing
    // ==========================================================================

    ColumnDef* parseColumnDef();
    TypeName parseTypeName();
    std::vector<ColumnConstraint> parseColumnConstraints();
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
    Expression* parseOrExpr();
    Expression* parseAndExpr();
    Expression* parseNotExpr();
    Expression* parseComparisonExpr();
    Expression* parseAddExpr();
    Expression* parseMulExpr();
    Expression* parseUnaryExpr();
    Expression* parsePrimaryExpr();
    Expression* parseLiteral();
    Expression* parseFunctionCall(SchemaPath path);
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
