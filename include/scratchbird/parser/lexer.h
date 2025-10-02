#pragma once

#include "scratchbird/parser/token.h"
#include <string>
#include <vector>
#include <memory>

namespace scratchbird
{
    namespace parser
    {

        // Forward declaration
        class ErrorReporter;

        // Lexer for SQL input
        class Lexer
        {
        public:
            // Create lexer for given input
            explicit Lexer(std::string_view input);
            ~Lexer();

            // Get next token
            Token nextToken();

            // Peek at next token without consuming
            Token peekToken();

            // Get current source location
            SourceLocation currentLocation() const;

            // Get source text for a token
            std::string_view getTokenText(const Token &token) const;

            // Error reporting
            void setErrorReporter(ErrorReporter *reporter)
            {
                error_reporter_ = reporter;
            }

            // String pool access
            StringPool &stringPool()
            {
                return string_pool_;
            }
            const StringPool &stringPool() const
            {
                return string_pool_;
            }

        private:
            // Lexer state
            enum State
            {
                INITIAL,
                IN_IDENTIFIER,
                IN_NUMBER,
                IN_STRING,
                IN_COMMENT_LINE,
                IN_COMMENT_BLOCK
            };

            // Input management
            std::string_view input_;
            size_t current_pos_;
            size_t line_;
            size_t column_;

            // String interning
            StringPool string_pool_;

            // Error reporting
            ErrorReporter *error_reporter_;

            // Lookahead
            bool has_lookahead_;
            Token lookahead_token_;

            // Helper methods
            char currentChar() const;
            char peekChar(size_t offset = 1) const;
            void advance();
            void skipWhitespace();

            // Token scanners
            Token scanIdentifier();
            Token scanNumber();
            Token scanString();
            Token scanOperator();
            void scanComment();

            // Keyword detection
            TokenType checkKeyword(std::string_view text) const;

            // Error handling
            Token makeError(const std::string &message);
        };

        // Error reporting interface
        class ErrorReporter
        {
        public:
            virtual ~ErrorReporter() = default;

            struct Error
            {
                SourceLocation location;
                std::string message;
                std::string hint;
            };

            virtual void reportError(const Error &error) = 0;
            virtual bool hasErrors() const = 0;
            virtual size_t errorCount() const = 0;
        };

        // Simple error reporter implementation
        class SimpleErrorReporter : public ErrorReporter
        {
        public:
            void reportError(const Error &error) override;
            bool hasErrors() const override
            {
                return !errors_.empty();
            }
            size_t errorCount() const override
            {
                return errors_.size();
            }

            const std::vector<Error> &errors() const
            {
                return errors_;
            }
            void clear()
            {
                errors_.clear();
            }

        private:
            std::vector<Error> errors_;
        };

    } // namespace parser
} // namespace scratchbird