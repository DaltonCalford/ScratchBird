#pragma once

#include "scratchbird/parser/ast.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/optimizer/plan_node.h"
#include <vector>
#include <memory>
#include <cstring>
#include <unordered_set>

// Forward declaration
namespace scratchbird::core {
    class Database;
}

namespace scratchbird
{
    namespace sblr
    {

        // Bytecode generation result
        class BytecodeResult
        {
        public:
            bool success() const
            {
                // Success requires no errors AND bytecode was generated
                return errors_.empty() && !bytecode_.empty();
            }
            const std::vector<uint8_t> &bytecode() const
            {
                return bytecode_;
            }
            const std::vector<std::string> &errors() const
            {
                return errors_;
            }

            void addError(const std::string &error)
            {
                errors_.push_back(error);
            }

            // Bytecode writing helpers
            void writeByte(uint8_t byte)
            {
                bytecode_.push_back(byte);
            }
            void writeOpcode(Opcode op)
            {
                writeByte(static_cast<uint8_t>(op));
            }

            void writeInt32(uint32_t value)
            {
                bytecode_.resize(bytecode_.size() + 4);
                sblr::writeInt32(&bytecode_[bytecode_.size() - 4], value);
            }

            void writeInt64(uint64_t value)
            {
                bytecode_.resize(bytecode_.size() + 8);
                sblr::writeInt64(&bytecode_[bytecode_.size() - 8], value);
            }

            void writeDouble(double value)
            {
                // Write as 64-bit little-endian (IEEE 754 double)
                // Use memcpy to avoid aliasing issues, then serialize bytes
                uint64_t bits;
                std::memcpy(&bits, &value, sizeof(double));
                bytecode_.resize(bytecode_.size() + 8);
                sblr::writeInt64(&bytecode_[bytecode_.size() - 8], bits);
            }

            void writeString(const std::string &str)
            {
                // Check for overflow when converting size_t to uint32_t
                if (str.size() > UINT32_MAX)
                {
                    addError("String length exceeds maximum allowed size (4GB)");
                    writeInt32(0);
                    return;
                }
                // Write length as 32-bit value
                writeInt32(static_cast<uint32_t>(str.size()));
                // Write string data
                bytecode_.insert(bytecode_.end(), str.begin(), str.end());
            }

        private:
            std::vector<uint8_t> bytecode_;
            std::vector<std::string> errors_;
        };

        // Bytecode generator - converts AST to SBLR bytecode
        class BytecodeGenerator : public parser::ASTVisitor
        {
        public:
            BytecodeGenerator(const parser::StringPool &string_pool,
                            core::Database *database = nullptr);
            ~BytecodeGenerator();

            // Generate bytecode for a statement
            BytecodeResult generate(parser::Statement *stmt);

            // NEW: Generate bytecode from optimized PlanNode (Phase 1, Task 1.3)
            BytecodeResult generateFromPlan(std::shared_ptr<optimizer::PlanNode> plan,
                                           parser::SelectStmt *original_stmt);

            // ASTVisitor interface
            void visit(parser::CreateTableStmt *node) override;
            void visit(parser::CreateIndexStmt *node) override;             // Phase 2 Task 2.3
            void visit(parser::CreateTablespaceStmt *node) override;        // Phase 2 Task 2.1
            void visit(parser::AlterTablespaceStmt *node) override;         // Phase 2 Task 2.2
            void visit(parser::AlterTableSetTablespaceStmt *node) override; // Phase 4 Task 4.1.6
            void visit(parser::DropTableStmt *node) override;               // ALPHA Phase 1 - DDL Modifications
            void visit(parser::DropIndexStmt *node) override;               // ALPHA Phase 1 - DDL Modifications
            void visit(parser::TruncateTableStmt *node) override;            // ALPHA Phase 1 - DDL Modifications (TRUNCATE TABLE ASYNC)
            void visit(parser::AlterTableStmt *node) override;              // ALPHA Phase 1 - DDL Modifications
            void visit(parser::CreateSequenceStmt *node) override;           // ALPHA Phase 1 - Sequences
            void visit(parser::AlterSequenceStmt *node) override;            // ALPHA Phase 1 - Sequences
            void visit(parser::DropSequenceStmt *node) override;             // ALPHA Phase 1 - Sequences
            void visit(parser::CreateViewStmt *node) override;               // ALPHA Phase 1 - Views
            void visit(parser::DropViewStmt *node) override;                 // ALPHA Phase 1 - Views
            void visit(parser::RefreshMaterializedViewStmt *node) override;  // ALPHA Phase 1 - Materialized Views
            void visit(parser::DropTablespaceStmt *node) override;          // Phase 2 Task 2.1
            void visit(parser::AttachTablespaceStmt *node) override;        // Phase 6 Task 6.1
            void visit(parser::DetachTablespaceStmt *node) override;        // Phase 6 Task 6.2
            void visit(parser::InsertStmt *node) override;
            void visit(parser::SelectStmt *node) override;
            void visit(parser::UpdateStmt *node) override;           // Phase 1 Task 2.1
            void visit(parser::DeleteStmt *node) override;           // Phase 1 Task 2.2
            void visit(parser::AnalyzeStmt *node) override;          // Phase 1 Task 1.1.2
            void visit(parser::ExplainStmt *node) override;          // Phase 1 Task 1.5
            void visit(parser::StartTransactionStmt *node) override; // Phase 2 Task 2.6
            void visit(parser::SetTransactionStmt *node) override;   // Phase 3 Task 3.6
            void visit(parser::CommitStmt *node) override;           // Phase 2 Task 2.6
            void visit(parser::RollbackStmt *node) override;         // Phase 2 Task 2.6
            void visit(parser::SweepStmt *node) override;            // Phase 3 Task 3.3
            void visit(parser::CreateTriggerStmt *node) override;    // Phase 2 Wave 2 Agent C
            void visit(parser::DropTriggerStmt *node) override;      // Phase 2 Wave 2 Agent C

            // PSQL - Stored Procedures and Functions (Phase 2 Task 10.2)
            void visit(parser::CreateFunctionStmt *node) override;
            void visit(parser::CreateProcedureStmt *node) override;
            void visit(parser::BlockStmt *node) override;
            void visit(parser::VarDeclarationStmt *node) override;
            void visit(parser::AssignmentStmt *node) override;
            void visit(parser::IfStmt *node) override;
            void visit(parser::LoopStmt *node) override;
            void visit(parser::WhileStmt *node) override;
            void visit(parser::ExitStmt *node) override;
            void visit(parser::ReturnStmt *node) override;
            void visit(parser::RaiseStmt *node) override;

            void visit(parser::LiteralExpr *node) override;
            void visit(parser::IdentifierExpr *node) override;
            void visit(parser::BinaryOpExpr *node) override;
            void visit(parser::CastExpr *node) override;
            void visit(parser::FunctionCallExpr *node) override;
            void visit(parser::AggregateExpr *node) override;  // Phase 1 Task 4.1
            void visit(parser::WindowFuncExpr *node) override;  // Phase 1 Task 6.3
            void visit(parser::WindowSpec *node) override;      // Phase 1 Task 6.3
            void visit(parser::JSONFuncExpr *node) override;    // Phase 1 Task 7
            void visit(parser::CoalesceExpr *node) override;    // Phase 1 Task 8
            void visit(parser::NullIfExpr *node) override;      // Phase 1 Task 8
            void visit(parser::CaseExpr *node) override;        // Phase 1 Task 8
            void visit(parser::ArrayLiteral *node) override;    // Phase 2 Task 12
            void visit(parser::SubqueryExpr *node) override;    // Phase 2 Wave 2 - Agent B
            void visit(parser::SequenceFunctionExpr *node) override; // ALPHA Phase 1 - Sequences
            void visit(parser::ExtractExpr *node) override;      // EXTRACT(field FROM value)
            void visit(parser::ColumnDef *node) override;

            // Security statements (ALPHA Phase 1 - Security System Phase 2)
            void visit(parser::CreateUserStmt *node) override;
            void visit(parser::AlterUserStmt *node) override;
            void visit(parser::DropUserStmt *node) override;
            void visit(parser::CreateRoleStmt *node) override;
            void visit(parser::DropRoleStmt *node) override;
            void visit(parser::CreateGroupStmt *node) override;
            void visit(parser::DropGroupStmt *node) override;
            void visit(parser::GrantPrivilegeStmt *node) override;
            void visit(parser::RevokePrivilegeStmt *node) override;
            void visit(parser::GrantRoleStmt *node) override;
            void visit(parser::RevokeRoleStmt *node) override;
            void visit(parser::SetRoleStmt *node) override;
            void visit(parser::SetSessionAuthStmt *node) override;
            void visit(parser::CreatePolicyStmt *node) override;  // Security Phase 3.4.4
            void visit(parser::DropPolicyStmt *node) override;    // Security Phase 3.4.4
            void visit(parser::AlterTableRLSStmt *node) override; // Security Phase 3.4.4

        private:
            const parser::StringPool &string_pool_;
            BytecodeResult *current_result_;
            core::Database *database_;  // NEW: For accessing query planner

            // CTE tracking (Phase 2 Wave 2 - CTE implementation)
            std::unordered_set<parser::StringPool::StringId> active_ctes_;

            // Helper to write string from string pool
            void writeStringId(parser::StringPool::StringId id);

            // Helper to check if a name is an active CTE
            bool isActiveCTE(parser::StringPool::StringId name_id) const;

            // Helper to write data type
            void writeDataType(const parser::TypeName &type);

            // Helper to generate expression bytecode
            void generateExpression(parser::Expression *expr);

            // NEW: Plan node bytecode generation (Phase 1, Task 1.3)
            void generateSeqScanPlan(scratchbird::optimizer::SeqScanNode *node,
                                    parser::SelectStmt *stmt);
            void generateIndexScanPlan(scratchbird::optimizer::IndexScanNode *node,
                                      parser::SelectStmt *stmt);

            // NEW: JOIN plan node bytecode generation (Phase 1, Task 3.3)
            void generateNestedLoopJoinPlan(scratchbird::optimizer::NestedLoopJoinNode *node,
                                          parser::SelectStmt *stmt);
            void generateHashJoinPlan(scratchbird::optimizer::HashJoinNode *node,
                                    parser::SelectStmt *stmt);
            void generateJoinPlan(scratchbird::optimizer::PlanNode *node,
                                parser::SelectStmt *stmt);

            // NEW: Direct SELECT generation (fallback when planner unavailable)
            void generateDirectSelect(parser::SelectStmt *node);

            // NEW: Aggregation, sorting, and limiting bytecode generation (Phase 1, Task 4-5)
            void generateAggregatePlan(scratchbird::optimizer::AggregateNode *node,
                                      parser::SelectStmt *stmt);
            void generateSortPlan(scratchbird::optimizer::SortNode *node,
                                 parser::SelectStmt *stmt);
            void generateLimitPlan(scratchbird::optimizer::LimitNode *node,
                                  parser::SelectStmt *stmt);

            // NEW: Window function bytecode generation (Phase 1, Task 6.3)
            void generateWindowPlan(scratchbird::optimizer::WindowNode *node,
                                   parser::SelectStmt *stmt);

            // Helper to generate aggregate function bytecode
            void generateAggregateFunc(parser::AggregateExpr *agg_expr);

            // Helper to generate window function bytecode
            void generateWindowFunc(const optimizer::WindowNode::WindowFunction& win_func);
            void generateWindowSpec(const parser::WindowSpec *spec);
            void generateFrameClause(const parser::WindowSpec *spec);

            // PSQL - Control flow helpers (Phase 2 Task 10.2)
            int allocateLabel();  // Allocate new label ID
            void patchJump(int label_id, int target_offset);  // Patch jump to target
            int getCurrentOffset() const;  // Get current bytecode offset
            void emitLabel(int label_id);  // Mark label position

        private:
            // PSQL label tracking
            int next_label_id_ = 0;
            std::unordered_map<int, int> label_positions_;  // label_id -> bytecode offset
            std::vector<std::pair<int, int>> pending_patches_;  // (bytecode_offset, label_id) to patch
        };

        // Debug helper - disassemble bytecode to readable format
        class BytecodeDisassembler
        {
        public:
            static std::string disassemble(const std::vector<uint8_t> &bytecode);

        private:
            static std::string opcodeToString(Opcode op);
        };

    } // namespace sblr
} // namespace scratchbird