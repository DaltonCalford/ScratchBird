#pragma once

#include "scratchbird/parser/ast.h"
#include "scratchbird/parser/symbol_table.h"
#include "scratchbird/parser/lexer.h" // For ErrorReporter
#include <memory>
#include <vector>

namespace scratchbird
{
    namespace parser
    {

        // Semantic error
        struct SemanticError
        {
            SourceLocation location;
            std::string message;

            SemanticError(const SourceLocation &loc, const std::string &msg)
                : location(loc), message(msg)
            {
            }
        };

        // Result of semantic analysis
        class SemanticResult
        {
        public:
            bool success() const
            {
                return errors_.empty();
            }
            const std::vector<SemanticError> &errors() const
            {
                return errors_;
            }

            void addError(const SemanticError &error)
            {
                errors_.push_back(error);
            }

        private:
            std::vector<SemanticError> errors_;
        };

        // Expression type information
        struct ExpressionType
        {
            TypeName type;
            bool is_nullable;

            ExpressionType() : type(DataType::INTEGER), is_nullable(true) {}
            ExpressionType(const TypeName &t, bool nullable = false)
                : type(t), is_nullable(nullable)
            {
            }
        };

        // Semantic analyzer - performs type checking and name resolution
        class SemanticAnalyzer : public ASTVisitor
        {
        public:
            SemanticAnalyzer(const StringPool &string_pool);
            ~SemanticAnalyzer();

            // Analyze a statement
            SemanticResult analyze(Statement *stmt);

            // ASTVisitor interface
            void visit(CreateTableStmt *node) override;
            void visit(InsertStmt *node) override;
            void visit(SelectStmt *node) override;
            void visit(LiteralExpr *node) override;
            void visit(IdentifierExpr *node) override;
            void visit(BinaryOpExpr *node) override;
            void visit(ColumnDef *node) override;

            // Get expression type after analysis
            const ExpressionType *getExpressionType(Expression *expr) const;

        private:
            const StringPool &string_pool_;
            SymbolTable symbol_table_;
            SemanticResult *current_result_;
            std::unordered_map<Expression *, ExpressionType> expression_types_;

            // Current table context (for column resolution)
            TableSymbol *current_table_ = nullptr;

            // Helper methods
            void reportError(const SourceLocation &loc, const std::string &message);
            void reportError(const ASTNode *node, const std::string &message);

            // Type checking helpers
            void checkExpression(Expression *expr);
            void setExpressionType(Expression *expr, const ExpressionType &type);

            // Name resolution helpers
            TableSymbol *resolveTable(StringPool::StringId name);
            const ColumnSymbol *resolveColumn(StringPool::StringId name);
        };

        // Convenience function for semantic analysis
        std::unique_ptr<SemanticResult> analyzeAST(Statement *stmt, const StringPool &pool);

    } // namespace parser
} // namespace scratchbird