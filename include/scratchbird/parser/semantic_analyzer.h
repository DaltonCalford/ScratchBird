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

            ExpressionType() : type(DataType::INT32), is_nullable(true) {}
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
            void visit(CreateIndexStmt *node) override;             // Phase 2 Task 2.3
            void visit(CreateTablespaceStmt *node) override;        // Phase 2 Task 2.1
            void visit(AlterTablespaceStmt *node) override;         // Phase 2 Task 2.2
            void visit(AlterTableSetTablespaceStmt *node) override; // Phase 4 Task 4.1.1
            void visit(DropTablespaceStmt *node) override;          // Phase 2 Task 2.1
            void visit(AttachTablespaceStmt *node) override;        // Phase 6 Task 6.1
            void visit(DetachTablespaceStmt *node) override;        // Phase 6 Task 6.2
            void visit(InsertStmt *node) override;
            void visit(SelectStmt *node) override;
            void visit(UpdateStmt *node) override;                  // Phase 1 Task 2.1
            void visit(DeleteStmt *node) override;                  // Phase 1 Task 2.2
            void visit(AnalyzeStmt *node) override;                 // Phase 1 Task 1.1.2
            void visit(ExplainStmt *node) override;                 // Phase 1 Task 1.5
            void visit(StartTransactionStmt *node) override; // Phase 2 Task 2.6
            void visit(SetTransactionStmt *node) override;   // Phase 3 Task 3.6
            void visit(CommitStmt *node) override;           // Phase 2 Task 2.6
            void visit(RollbackStmt *node) override;         // Phase 2 Task 2.6
            void visit(SweepStmt *node) override;            // Phase 3 Task 3.3
            void visit(LiteralExpr *node) override;
            void visit(IdentifierExpr *node) override;
            void visit(BinaryOpExpr *node) override;
            void visit(CastExpr *node) override;
            void visit(FunctionCallExpr *node) override;
            void visit(AggregateExpr *node) override;  // Phase 1 Task 4.1
            void visit(WindowFuncExpr *node) override; // Phase 1 Task 6
            void visit(WindowSpec *node) override;     // Phase 1 Task 6
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

            // Aggregation context (Phase 1 Task 4.1)
            bool in_aggregate_ = false;      // Currently inside an aggregate function
            bool has_aggregates_ = false;    // SELECT list contains aggregates
            bool in_group_by_ = false;       // Currently analyzing GROUP BY clause

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