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

            // CREATE TABLE helpers
            ColumnDef *parseColumnDef();
            TypeName parseTypeName();

            // Expression parsers
            Expression *parseExpression();
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