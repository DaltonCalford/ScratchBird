#pragma once

#include "scratchbird/parser/ast.h"
#include "scratchbird/parser/lexer.h"
#include <memory>
#include <vector>

namespace scratchbird
{
    namespace parser
    {

        // Parse result
        class ParseResult
        {
        public:
            ParseResult() = default;

            bool success() const
            {
                return errors_.empty() && statement_ != nullptr;
            }
            Statement *statement() const
            {
                return statement_;
            }
            const std::vector<ErrorReporter::Error> &errors() const
            {
                return errors_;
            }

            void setStatement(Statement *stmt)
            {
                statement_ = stmt;
            }
            void addError(const ErrorReporter::Error &error)
            {
                errors_.push_back(error);
            }

        private:
            Statement *statement_ = nullptr;
            std::vector<ErrorReporter::Error> errors_;
        };

        // SQL Parser - Recursive Descent
        class Parser : public ErrorReporter
        {
        public:
            explicit Parser(Lexer &lexer, ASTArena &arena);
            ~Parser();

            // Parse a single SQL statement
            ParseResult parseStatement();

            // ErrorReporter interface
            void reportError(const Error &error) override;
            bool hasErrors() const override
            {
                return !errors_.empty();
            }
            size_t errorCount() const override
            {
                return errors_.size();
            }

            // String pool access
            StringPool &stringPool()
            {
                return lexer_.stringPool();
            }

            // Security Phase 3.4.7: Public expression parser for RLS policy expressions
            Expression *parseExpression();

        private:
            Lexer &lexer_;
            ASTArena &arena_;
            Token previous_token_; // Previously consumed token
            Token current_token_;  // Current lookahead token
            std::vector<Error> errors_;

            // Token management
            void advance();
            bool match(TokenType type);
            bool consume(TokenType type, const std::string &message);
            Token previous() const
            {
                return previous_token_;
            }
            Token current() const
            {
                return current_token_;
            }
            bool isAtEnd() const
            {
                return current_token_.type == TokenType::END_OF_FILE;
            }
            bool check(TokenType type) const
            {
                return current_token_.type == type;
            }

            // Error handling
            void error(const std::string &message);
            void synchronize();

            // Statement parsers
            Statement *parseCreateTable();
            Statement *parseCreateIndex();           // Phase 2 Task 2.3
            Statement *parseCreateTablespace();      // Phase 2 Task 2.1
            Statement *parseAlterTablespace();       // Phase 2 Task 2.2
            Statement *parseAlterTable();            // Phase 4 Task 4.1.1
            Statement *parseDropTable();             // ALPHA Phase 1 - DDL Modifications
            Statement *parseDropIndex();             // ALPHA Phase 1 - DDL Modifications
            Statement *parseTruncateTable();         // ALPHA Phase 1 - DDL Modifications (TRUNCATE TABLE ASYNC)
            Statement *parseCreateSequence();        // ALPHA Phase 1 - Sequences
            Statement *parseAlterSequence();         // ALPHA Phase 1 - Sequences
            Statement *parseDropSequence();          // ALPHA Phase 1 - Sequences
            Statement *parseCreateView();            // ALPHA Phase 1 - Views
            Statement *parseDropView();              // ALPHA Phase 1 - Views
            Statement *parseDropTablespace();        // Phase 2 Task 2.1
            Statement *parseAttachTablespace();      // Phase 6 Task 6.1
            Statement *parseDetachTablespace();      // Phase 6 Task 6.2
            Statement *parseInsert();
            Statement *parseSelect();
            Statement *parseUpdate();                // Phase 1 Task 2.1
            Statement *parseDelete();                // Phase 1 Task 2.2
            Statement *parseAnalyze();               // Phase 1 Task 1.1.2
            Statement *parseExplain();               // Phase 1 Task 1.5
            Statement *parseStartTransaction();      // Phase 2 Task 2.6
            Statement *parseSetTransaction();        // Phase 3 Task 3.6
            Statement *parseCommit();                // Phase 2 Task 2.6
            Statement *parseRollback();              // Phase 2 Task 2.6
            Statement *parseSweep();                 // Phase 3 Task 3.3
            Statement *parseCreateTrigger();         // Phase 2 Wave 2 Agent C
            Statement *parseDropTrigger();           // Phase 2 Wave 2 Agent C

            // Security statement parsers (ALPHA Phase 1 - Security System Phase 2)
            Statement *parseCreateUser();
            Statement *parseAlterUser();
            Statement *parseDropUser();
            Statement *parseCreateRole();
            Statement *parseDropRole();
            Statement *parseCreateGroup();
            Statement *parseDropGroup();
            Statement *parseGrant();                 // GRANT privilege or role
            Statement *parseRevoke();                // REVOKE privilege or role
            Statement *parseSetRole();               // SET ROLE or RESET ROLE
            Statement *parseSetSessionAuth();        // SET SESSION AUTHORIZATION or RESET
            Statement *parseCreatePolicy();          // Security Phase 3.4: CREATE POLICY
            Statement *parseDropPolicy();            // Security Phase 3.4: DROP POLICY
            Statement *parseAlterTableRLS(const SourceLocation& start_loc, StringPool::StringId table_name);  // Security Phase 3.4

            // PSQL - Stored Procedures and Functions (Phase 2 Task 10.2)
            Statement *parseCreateFunction();
            Statement *parseCreateProcedure();
            std::vector<Parameter*> parseParameterList();
            Parameter* parseParameter();
            BlockStmt *parsePSQLBlock();
            std::vector<VarDeclarationStmt*> parseDeclareSection();
            VarDeclarationStmt *parseVariableDeclaration();
            Statement *parseIfStatement();
            Statement *parseLoopStatement();
            Statement *parseWhileStatement();
            Statement *parseExitStatement();
            Statement *parseReturnStatement();
            Statement *parseRaiseStatement();
            Statement *parseAssignmentOrCall();  // Handles := and procedure calls
            std::vector<ExceptionHandler*> parseExceptionHandlers();

            // CREATE TABLE helpers
            ColumnDef *parseColumnDef();
            TypeName parseTypeName();

            // JOIN helpers (Phase 1 Task 3.1)
            FromClause parseFromClause();
            TableRef parseTableRef();
            JoinClause parseJoinClause(const TableRef &left_table);

            // CTE helpers (Phase 2 Wave 2)
            WithClause *parseWithClause();

            // Aggregation helpers (Phase 1 Task 4.1)
            GroupByClause parseGroupByClause();
            std::vector<OrderByItem> parseOrderByClause();
            void parseLimitClause(SelectStmt *stmt);

            // Window function helpers (Phase 1 Task 6)
            WindowSpec *parseWindowSpec();
            void parseFrameClause(WindowSpec *spec);
            FrameBoundary parseFrameBoundary();

            // Expression parsers (parseExpression now public for RLS Phase 3.4.7)
            Expression *parseComparison();
            Expression *parseTerm();
            Expression *parseFactor();
            Expression *parsePrimary();

            // Helper to get source span
            SourceSpan makeSpan(const SourceLocation &start) const;
            SourceSpan makeSpan(const SourceLocation &start, const SourceLocation &end) const;
        };

        // Convenience function to parse SQL
        std::unique_ptr<ParseResult> parseSQL(const std::string &sql);

    } // namespace parser
} // namespace scratchbird