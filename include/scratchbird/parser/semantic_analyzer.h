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
            void visit(DropTableStmt *node) override;               // ALPHA Phase 1 - DDL Modifications
            void visit(DropIndexStmt *node) override;               // ALPHA Phase 1 - DDL Modifications
            void visit(TruncateTableStmt *node) override;           // ALPHA Phase 1 - DDL Modifications (TRUNCATE TABLE ASYNC)
            void visit(AlterTableStmt *node) override;              // ALPHA Phase 1 - DDL Modifications
            void visit(CreateSequenceStmt *node) override;          // ALPHA Phase 1 - Sequences
            void visit(AlterSequenceStmt *node) override;           // ALPHA Phase 1 - Sequences
            void visit(DropSequenceStmt *node) override;            // ALPHA Phase 1 - Sequences
            void visit(CreateViewStmt *node) override;              // ALPHA Phase 1 - Views
            void visit(DropViewStmt *node) override;                // ALPHA Phase 1 - Views
            void visit(RefreshMaterializedViewStmt *node) override; // ALPHA Phase 1 - Materialized Views
            void visit(CreateTablespaceStmt *node) override;        // Phase 2 Task 2.1
            void visit(AlterTablespaceStmt *node) override;         // Phase 2 Task 2.2
            void visit(AlterTableSetTablespaceStmt *node) override; // Phase 4 Task 4.1.1
            void visit(DropTablespaceStmt *node) override;          // Phase 2 Task 2.1
            void visit(AttachTablespaceStmt *node) override;        // Phase 6 Task 6.1
            void visit(DetachTablespaceStmt *node) override;        // Phase 6 Task 6.2
            void visit(InsertStmt *node) override;
            void visit(SelectStmt *node) override;
            void visit(SetOperationStmt *node) override;            // UNION/INTERSECT/EXCEPT
            void visit(UpdateStmt *node) override;                  // Phase 1 Task 2.1
            void visit(DeleteStmt *node) override;                  // Phase 1 Task 2.2
            void visit(MergeStmt *node) override;                   // ALPHA Phase 1 - Advanced SQL
            void visit(AnalyzeStmt *node) override;                 // Phase 1 Task 1.1.2
            void visit(ExplainStmt *node) override;                 // Phase 1 Task 1.5
            void visit(StartTransactionStmt *node) override; // Phase 2 Task 2.6
            void visit(SetTransactionStmt *node) override;   // Phase 3 Task 3.6
            void visit(CommitStmt *node) override;           // Phase 2 Task 2.6
            void visit(RollbackStmt *node) override;         // Phase 2 Task 2.6
            void visit(SweepStmt *node) override;            // Phase 3 Task 3.3
            void visit(ShowStmt *node) override;             // ALPHA Phase 1 - Developer Experience
            void visit(DescribeStmt *node) override;         // ALPHA Phase 1 - Developer Experience
            void visit(CreateTriggerStmt *node) override;    // Phase 2 Wave 2 Agent C
            void visit(DropTriggerStmt *node) override;      // Phase 2 Wave 2 Agent C
            void visit(CreateFunctionStmt *node) override;   // Phase 2 Task 10.2 - PSQL
            void visit(CreateProcedureStmt *node) override;  // Phase 2 Task 10.2 - PSQL
            void visit(BlockStmt *node) override;            // Phase 2 Task 10.2 - PSQL
            void visit(VarDeclarationStmt *node) override;   // Phase 2 Task 10.2 - PSQL
            void visit(AssignmentStmt *node) override;       // Phase 2 Task 10.2 - PSQL
            void visit(IfStmt *node) override;               // Phase 2 Task 10.2 - PSQL
            void visit(LoopStmt *node) override;             // Phase 2 Task 10.2 - PSQL
            void visit(WhileStmt *node) override;            // Phase 2 Task 10.2 - PSQL
            void visit(ExitStmt *node) override;             // Phase 2 Task 10.2 - PSQL
            void visit(ReturnStmt *node) override;           // Phase 2 Task 10.2 - PSQL
            void visit(RaiseStmt *node) override;            // Phase 2 Task 10.2 - PSQL
            void visit(LiteralExpr *node) override;
            void visit(IdentifierExpr *node) override;
            void visit(BinaryOpExpr *node) override;
            void visit(CastExpr *node) override;
            void visit(FunctionCallExpr *node) override;
            void visit(SequenceFunctionExpr *node) override;  // ALPHA Phase 1 - Sequences
            void visit(ExtractExpr *node) override;           // EXTRACT(field FROM value)
            void visit(AggregateExpr *node) override;  // Phase 1 Task 4.1
            void visit(WindowFuncExpr *node) override; // Phase 1 Task 6
            void visit(WindowSpec *node) override;     // Phase 1 Task 6
            void visit(JSONFuncExpr *node) override;   // Phase 1 Task 7
            void visit(CoalesceExpr *node) override;   // Phase 1 Task 8
            void visit(NullIfExpr *node) override;     // Phase 1 Task 8
            void visit(CaseExpr *node) override;       // Phase 1 Task 8
            void visit(GroupingExpr *node) override;   // Phase 3: Advanced Grouping
            void visit(ArrayLiteral *node) override;   // Phase 2 Task 12
            void visit(SubqueryExpr *node) override;   // Phase 2 Wave 2 - Agent B
            void visit(ColumnDef *node) override;

            // Security statements (ALPHA Phase 1 - Security System Phase 2)
            void visit(CreateUserStmt *node) override;
            void visit(AlterUserStmt *node) override;
            void visit(DropUserStmt *node) override;
            void visit(CreateRoleStmt *node) override;
            void visit(DropRoleStmt *node) override;
            void visit(CreateGroupStmt *node) override;
            void visit(DropGroupStmt *node) override;
            void visit(GrantPrivilegeStmt *node) override;
            void visit(RevokePrivilegeStmt *node) override;
            void visit(GrantRoleStmt *node) override;
            void visit(RevokeRoleStmt *node) override;
            void visit(SetRoleStmt *node) override;
            void visit(SetSessionAuthStmt *node) override;
            void visit(SetConstraintsStmt *node) override;   // P2-7: SET CONSTRAINTS
            void visit(CreatePolicyStmt *node) override;     // Security Phase 3.4
            void visit(DropPolicyStmt *node) override;       // Security Phase 3.4
            void visit(AlterTableRLSStmt *node) override;    // Security Phase 3.4

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