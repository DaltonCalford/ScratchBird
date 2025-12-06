#include "scratchbird/parser/ast.h"
#include <iostream>
#include <iomanip>

namespace scratchbird
{
    namespace parser
    {

        // ===== Arena Allocator =====

        ASTArena::ASTArena()
        {
            // Start with one block
            blocks_.emplace_back();
            blocks_.back().data = std::make_unique<uint8_t[]>(BLOCK_SIZE);
            blocks_.back().size = BLOCK_SIZE;
            blocks_.back().used = 0;
        }

        ASTArena::~ASTArena()
        {
            // Call destructors for all objects with non-trivial destructors
            // Iterate in reverse order (LIFO) for proper destruction order
            for (auto it = destructors_.rbegin(); it != destructors_.rend(); ++it)
            {
                it->second(it->first); // Call destructor function pointer
            }

            // Blocks are automatically freed by unique_ptr
        }

        void ASTArena::registerDestructor(void *obj, void (*destructor)(void *))
        {
            destructors_.emplace_back(obj, destructor);
        }

        void *ASTArena::allocate(size_t size)
        {
            // Align to 8 bytes
            size = (size + 7) & ~7;

            // Find a block with enough space
            for (auto &block : blocks_)
            {
                if (block.used + size <= block.size)
                {
                    void *ptr = block.data.get() + block.used;
                    block.used += size;
                    return ptr;
                }
            }

            // Need a new block
            size_t block_size = std::max(BLOCK_SIZE, size);
            blocks_.emplace_back();
            blocks_.back().data = std::make_unique<uint8_t[]>(block_size);
            blocks_.back().size = block_size;
            blocks_.back().used = size;

            return blocks_.back().data.get();
        }

        void ASTArena::reset()
        {
            // Keep first block, reset others
            if (!blocks_.empty())
            {
                blocks_[0].used = 0;
                blocks_.resize(1);
            }
        }

        // ===== Visitor Accept Methods =====

        void LiteralExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void IdentifierExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void BinaryOpExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void ColumnDef::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateTableStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateIndexStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DropTableStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DropIndexStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void TruncateTableStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void AlterTableStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateSequenceStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void AlterSequenceStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DropSequenceStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateViewStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DropViewStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void RefreshMaterializedViewStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateTablespaceStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DropTablespaceStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void AttachTablespaceStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DetachTablespaceStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void AlterTablespaceStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void AlterTableSetTablespaceStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void InsertStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SelectStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SetOperationStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void UpdateStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DeleteStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void MergeStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void AnalyzeStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void ExplainStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void StartTransactionStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CommitStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void RollbackStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SavepointStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void ReleaseSavepointStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void RollbackToSavepointStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateTypeStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateDomainStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CallStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SweepStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void ShowStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DescribeStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SetTransactionStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateTriggerStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DropTriggerStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateDatabaseTriggerStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        // ===== PSQL - Stored Procedures and Functions (Phase 2 Task 10.2) =====

        void CreateFunctionStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateProcedureStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void BlockStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void VarDeclarationStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void AssignmentStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void IfStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void LoopStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void WhileStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void ExitStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void ReturnStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void RaiseStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        // ===== AST Printer =====

        void ASTPrinter::printIndent()
        {
            for (int i = 0; i < indent_; i++)
            {
                out_ << " ";
            }
        }

        void ASTPrinter::visit(CreateTableStmt *node)
        {
            printIndent();
            out_ << "CREATE TABLE " << pool_.get(node->tableName()) << " (\n";
            increaseIndent();

            bool first = true;
            for (auto *col : node->columns())
            {
                if (!first)
                    out_ << ",\n";
                col->accept(this);
                first = false;
            }

            decreaseIndent();
            out_ << "\n";
            printIndent();
            out_ << ")";
        }

        void ASTPrinter::visit(CreateIndexStmt *node)
        {
            printIndent();
            out_ << "CREATE";
            if (node->isUnique())
                out_ << " UNIQUE";
            out_ << " INDEX " << pool_.get(node->indexName())
                 << " ON " << pool_.get(node->tableName()) << " (";
            bool first = true;
            for (auto col_id : node->columns())
            {
                if (!first)
                    out_ << ", ";
                out_ << pool_.get(col_id);
                first = false;
            }
            out_ << ")";
        }

        void ASTPrinter::visit(AlterTablespaceStmt *node)
        {
            printIndent();
            out_ << "ALTER TABLESPACE " << pool_.get(node->tablespaceName());

            for (const auto &alteration : node->alterations())
            {
                switch (alteration.type)
                {
                    case TablespaceAlterationType::SET_AUTOEXTEND:
                        out_ << " AUTOEXTEND " << (alteration.autoextend_enabled ? "ON" : "OFF");
                        break;
                    case TablespaceAlterationType::SET_AUTOEXTEND_SIZE:
                        out_ << " AUTOEXTEND_SIZE " << alteration.size_value;
                        break;
                    case TablespaceAlterationType::SET_MAXSIZE:
                        out_ << " MAXSIZE ";
                        if (alteration.size_value == 0)
                            out_ << "UNLIMITED";
                        else
                            out_ << alteration.size_value;
                        break;
                    case TablespaceAlterationType::RENAME_TO:
                        out_ << " RENAME TO " << pool_.get(alteration.new_name);
                        break;
                }
            }
        }

        void ASTPrinter::visit(AlterTableSetTablespaceStmt *node)
        {
            printIndent();
            out_ << "ALTER TABLE " << pool_.get(node->tableName())
                 << " SET TABLESPACE " << pool_.get(node->tablespaceName());
            if (node->online())
                out_ << " ONLINE";
        }

        void ASTPrinter::visit(CreateTablespaceStmt *node)
        {
            printIndent();
            out_ << "CREATE TABLESPACE " << pool_.get(node->tablespaceName())
                 << " LOCATION '" << pool_.get(node->location()) << "'";

            if (!node->autoextendEnabled())
            {
                out_ << " AUTOEXTEND OFF";
            }
            else
            {
                out_ << " AUTOEXTEND ON AUTOEXTEND_SIZE " << node->autoextendSizeMB();
            }

            if (node->maxSizeMB() > 0)
            {
                out_ << " MAXSIZE " << node->maxSizeMB();
            }
            else
            {
                out_ << " MAXSIZE UNLIMITED";
            }

            if (node->preallocPages() > 0)
            {
                out_ << " PREALLOC " << node->preallocPages();
            }
        }

        void ASTPrinter::visit(DropTablespaceStmt *node)
        {
            printIndent();
            out_ << "DROP TABLESPACE " << pool_.get(node->tablespaceName());
            if (node->force())
            {
                out_ << " FORCE";
            }
        }

        void ASTPrinter::visit(AttachTablespaceStmt *node)
        {
            printIndent();
            out_ << "ATTACH TABLESPACE '" << pool_.get(node->filePath()) << "'";
            if (node->tablespaceName() != 0)
            {
                out_ << " AS '" << pool_.get(node->tablespaceName()) << "'";
            }
        }

        void ASTPrinter::visit(DetachTablespaceStmt *node)
        {
            printIndent();
            out_ << "DETACH TABLESPACE " << pool_.get(node->tablespaceName());
            if (node->force())
            {
                out_ << " FORCE";
            }
        }

        void ASTPrinter::visit(InsertStmt *node)
        {
            printIndent();
            out_ << "INSERT INTO " << pool_.get(node->tableName()) << " (";

            bool first = true;
            for (auto col_id : node->columns())
            {
                if (!first)
                    out_ << ", ";
                out_ << pool_.get(col_id);
                first = false;
            }

            out_ << ") VALUES ";

            // Print each value row
            bool first_row = true;
            for (const auto& values : node->valueRows())
            {
                if (!first_row)
                    out_ << ", ";
                out_ << "(";

                first = true;
                for (auto *val : values)
                {
                    if (!first)
                        out_ << ", ";
                    val->accept(this);
                    first = false;
                }

                out_ << ")";
                first_row = false;
            }
        }

        void ASTPrinter::visit(SelectStmt *node)
        {
            printIndent();
            out_ << "SELECT ";

            bool first = true;
            for (const auto &item : node->selectList())
            {
                if (!first)
                    out_ << ", ";
                if (item.is_star)
                {
                    out_ << "*";
                }
                else
                {
                    item.expr->accept(this);
                    if (item.alias != 0)
                    {
                        out_ << " AS " << pool_.get(item.alias);
                    }
                }
                first = false;
            }

            // Print FROM clause with JOINs (Phase 1 Task 3.1)
            out_ << " FROM " << pool_.get(node->fromClause().base_table.table_name);
            if (node->fromClause().base_table.alias != 0)
            {
                out_ << " AS " << pool_.get(node->fromClause().base_table.alias);
            }

            // Print JOIN clauses
            for (const auto &join : node->fromClause().joins)
            {
                // Print join type
                if (join.natural)
                    out_ << " NATURAL";

                switch (join.join_type)
                {
                    case JoinType::INNER:
                        out_ << " INNER JOIN";
                        break;
                    case JoinType::LEFT:
                        out_ << " LEFT OUTER JOIN";
                        break;
                    case JoinType::RIGHT:
                        out_ << " RIGHT OUTER JOIN";
                        break;
                    case JoinType::FULL:
                        out_ << " FULL OUTER JOIN";
                        break;
                    case JoinType::CROSS:
                        out_ << " CROSS JOIN";
                        break;
                }

                // Print right table
                out_ << " " << pool_.get(join.right_table.table_name);
                if (join.right_table.alias != 0)
                {
                    out_ << " AS " << pool_.get(join.right_table.alias);
                }

                // Print join condition
                switch (join.condition_type)
                {
                    case JoinConditionType::ON:
                        out_ << " ON ";
                        if (join.on_condition)
                            join.on_condition->accept(this);
                        break;
                    case JoinConditionType::USING:
                        out_ << " USING (";
                        first = true;
                        for (auto col_id : join.using_columns)
                        {
                            if (!first)
                                out_ << ", ";
                            out_ << pool_.get(col_id);
                            first = false;
                        }
                        out_ << ")";
                        break;
                    case JoinConditionType::NATURAL:
                    case JoinConditionType::CROSS:
                        // No condition to print
                        break;
                }
            }

            if (node->whereClause())
            {
                out_ << " WHERE ";
                node->whereClause()->accept(this);
            }
        }

        void ASTPrinter::visit(UpdateStmt *node)
        {
            // Phase 1 Task 2.1: UPDATE statement
            printIndent();
            out_ << "UPDATE " << pool_.get(node->tableName()) << " SET ";

            bool first = true;
            for (const auto &assign : node->assignments())
            {
                if (!first)
                    out_ << ", ";
                out_ << pool_.get(assign.column_name) << " = ";
                assign.value->accept(this);
                first = false;
            }

            if (node->whereClause())
            {
                out_ << " WHERE ";
                node->whereClause()->accept(this);
            }
        }

        void ASTPrinter::visit(DeleteStmt *node)
        {
            // Phase 1 Task 2.2: DELETE statement
            printIndent();
            out_ << "DELETE FROM " << pool_.get(node->tableName());

            if (node->whereClause())
            {
                out_ << " WHERE ";
                node->whereClause()->accept(this);
            }
        }

        void ASTPrinter::visit(MergeStmt *node)
        {
            // ALPHA Phase 1 - Advanced SQL: MERGE statement
            printIndent();
            out_ << "MERGE INTO " << pool_.get(node->targetTable());
            out_ << " USING ";
            if (node->source())
            {
                node->source()->accept(this);
            }
            out_ << " ON ";
            if (node->onCondition())
            {
                node->onCondition()->accept(this);
            }
            // Print WHEN clauses
            for (const auto& when : node->whenClauses())
            {
                out_ << " WHEN ";
                switch (when.type)
                {
                    case MergeStmt::WhenClause::MATCHED:
                        out_ << "MATCHED";
                        break;
                    case MergeStmt::WhenClause::NOT_MATCHED:
                        out_ << "NOT MATCHED";
                        break;
                    case MergeStmt::WhenClause::NOT_MATCHED_BY_SOURCE:
                        out_ << "NOT MATCHED BY SOURCE";
                        break;
                }
                if (when.condition)
                {
                    out_ << " AND ";
                    when.condition->accept(this);
                }
                out_ << " THEN";
                // Note: Would need to print UPDATE/INSERT/DELETE details here
                // but keeping simple for now
            }
        }

        void ASTPrinter::visit(AnalyzeStmt *node)
        {
            printIndent();
            out_ << "ANALYZE " << pool_.get(node->tableName());

            if (!node->analyzeAllColumns())
            {
                out_ << " COLUMN " << pool_.get(node->columnName());
            }

            if (node->sampleRate() > 0.0f)
            {
                out_ << " SAMPLE " << node->sampleRate();
            }
        }

        void ASTPrinter::visit(ExplainStmt *node)
        {
            // Phase 1 Task 1.5: EXPLAIN command
            printIndent();
            out_ << "EXPLAIN ";

            // Print the nested query
            if (node->query())
            {
                node->query()->accept(this);
            }
        }

        void ASTPrinter::visit(StartTransactionStmt *node)
        {
            printIndent();
            out_ << "START TRANSACTION";

            if (node->mode() == TransactionMode::READ_ONLY)
            {
                out_ << " READ ONLY";
            }
            else
            {
                out_ << " READ WRITE";
            }

            out_ << " ISOLATION LEVEL ";
            switch (node->isolation())
            {
                case IsolationLevel::READ_COMMITTED:
                    out_ << "READ COMMITTED";
                    break;
                case IsolationLevel::SNAPSHOT:
                    out_ << "SNAPSHOT";
                    break;
                case IsolationLevel::SNAPSHOT_TABLE_STABILITY:
                    out_ << "SNAPSHOT TABLE STABILITY";
                    break;
            }

            if (!node->wait())
            {
                out_ << " NO WAIT";
            }

            if (node->commitOutstanding())
            {
                out_ << " WITH COMMIT OUTSTANDING";
            }
        }

        void ASTPrinter::visit(CommitStmt *node)
        {
            printIndent();
            out_ << "COMMIT";
        }

        void ASTPrinter::visit(RollbackStmt *node)
        {
            printIndent();
            out_ << "ROLLBACK";
        }

        void ASTPrinter::visit(SweepStmt *node)
        {
            printIndent();
            out_ << "SWEEP DATABASE";
        }

        void ASTPrinter::visit(SetTransactionStmt *node)
        {
            printIndent();
            out_ << "SET TRANSACTION";

            if (node->mode() == TransactionMode::READ_ONLY)
            {
                out_ << " READ ONLY";
            }
            else
            {
                out_ << " READ WRITE";
            }

            out_ << " ISOLATION LEVEL ";
            switch (node->isolation())
            {
                case IsolationLevel::READ_COMMITTED:
                    out_ << "READ COMMITTED";
                    break;
                case IsolationLevel::SNAPSHOT:
                    out_ << "SNAPSHOT";
                    break;
                case IsolationLevel::SNAPSHOT_TABLE_STABILITY:
                    out_ << "SNAPSHOT TABLE STABILITY";
                    break;
            }

            if (!node->wait())
            {
                out_ << " NO WAIT";
            }

            if (node->lockTimeout() > 0)
            {
                out_ << " LOCK TIMEOUT " << node->lockTimeout();
            }

            if (!node->tableReservations().empty())
            {
                out_ << " RESERVING ";
                bool first = true;
                for (const auto &res : node->tableReservations())
                {
                    if (!first)
                        out_ << ", ";
                    out_ << pool_.get(res.table_name);
                    if (res.lock_mode == TableLockMode::SHARED)
                        out_ << " FOR SHARED " << (res.for_write ? "WRITE" : "READ");
                    else
                        out_ << " FOR PROTECTED " << (res.for_write ? "WRITE" : "READ");
                    first = false;
                }
            }
        }

        void ASTPrinter::visit(LiteralExpr *node)
        {
            switch (node->literalType())
            {
                case LiteralExpr::INTEGER:
                    out_ << node->intValue();
                    break;
                case LiteralExpr::FLOAT:
                    out_ << std::fixed << std::setprecision(6) << node->floatValue();
                    break;
                case LiteralExpr::STRING:
                    out_ << "'" << pool_.get(node->stringValue()) << "'";
                    break;
                case LiteralExpr::NULL_LITERAL:
                    out_ << "NULL";
                    break;
            }
        }

        void ASTPrinter::visit(IdentifierExpr *node)
        {
            // Phase 1 Task 3.1: Handle qualified identifiers (table.column)
            if (node->isQualified())
            {
                out_ << pool_.get(node->qualifier()) << "." << pool_.get(node->name());
            }
            else
            {
                out_ << pool_.get(node->name());
            }
        }

        void ASTPrinter::visit(BinaryOpExpr *node)
        {
            out_ << "(";
            node->left()->accept(this);

            switch (node->op())
            {
                case BinaryOp::ADD:
                    out_ << " + ";
                    break;
                case BinaryOp::SUBTRACT:
                    out_ << " - ";
                    break;
                case BinaryOp::MULTIPLY:
                    out_ << " * ";
                    break;
                case BinaryOp::DIVIDE:
                    out_ << " / ";
                    break;
                case BinaryOp::MODULO:
                    out_ << " % ";
                    break;
                case BinaryOp::EQ:
                    out_ << " = ";
                    break;
                case BinaryOp::NE:
                    out_ << " <> ";
                    break;
                case BinaryOp::LT:
                    out_ << " < ";
                    break;
                case BinaryOp::GT:
                    out_ << " > ";
                    break;
                case BinaryOp::LE:
                    out_ << " <= ";
                    break;
                case BinaryOp::GE:
                    out_ << " >= ";
                    break;
                case BinaryOp::AND:
                    out_ << " AND ";
                    break;
                case BinaryOp::OR:
                    out_ << " OR ";
                    break;
                case BinaryOp::LIKE:
                    out_ << " LIKE ";
                    break;
                case BinaryOp::ILIKE:
                    out_ << " ILIKE ";
                    break;
            }

            node->right()->accept(this);
            out_ << ")";
        }

        void ASTPrinter::visit(ColumnDef *node)
        {
            printIndent();
            out_ << pool_.get(node->name()) << " ";

            switch (node->type().type)
            {
                case DataType::INT32:
                    out_ << "INTEGER";
                    break;
                case DataType::INT64:
                    out_ << "BIGINT";
                    break;
                case DataType::FLOAT64:
                    out_ << "DOUBLE";
                    break;
                case DataType::VARCHAR:
                    out_ << "VARCHAR(" << node->type().precision << ")";
                    break;
            }

            if (!node->nullable())
            {
                out_ << " NOT NULL";
            }
        }

        void CastExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void FunctionCallExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SequenceFunctionExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void ExtractExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void AggregateExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void WindowFuncExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void WindowSpec::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void JSONFuncExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CoalesceExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void NullIfExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CaseExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void GroupingExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void ArrayLiteral::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SubqueryExpr::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void ASTPrinter::visit(CastExpr *node)
        {
            out_ << (node->isTryCast() ? "TRY_CAST(" : "CAST(");
            node->expr()->accept(this);
            out_ << " AS ";

            auto &type = node->targetType();
            out_ << core::TypeSystem::getTypeName(type.type);

            if (type.precision > 0)
            {
                out_ << "(" << type.precision;
                if (type.scale > 0)
                    out_ << ", " << type.scale;
                out_ << ")";
            }
            out_ << ")";
        }

        void ASTPrinter::visit(FunctionCallExpr *node)
        {
            out_ << pool_.get(node->name()) << "(";

            bool first = true;
            for (auto *arg : node->args())
            {
                if (!first)
                    out_ << ", ";
                arg->accept(this);
                first = false;
            }

            out_ << ")";
        }

        void ASTPrinter::visit(AggregateExpr *node)
        {
            // Print aggregate function name
            switch (node->func())
            {
            case AggregateFunc::COUNT:
                out_ << "COUNT(";
                break;
            case AggregateFunc::SUM:
                out_ << "SUM(";
                break;
            case AggregateFunc::AVG:
                out_ << "AVG(";
                break;
            case AggregateFunc::MIN:
                out_ << "MIN(";
                break;
            case AggregateFunc::MAX:
                out_ << "MAX(";
                break;
            }

            if (node->distinct())
            {
                out_ << "DISTINCT ";
            }

            if (node->arg())
            {
                node->arg()->accept(this);
            }
            else
            {
                out_ << "*";  // COUNT(*)
            }

            out_ << ")";
        }

        void ASTPrinter::visit(WindowFuncExpr *node)
        {
            // Print window function name
            switch (node->func())
            {
            case WindowFunc::ROW_NUMBER:
                out_ << "ROW_NUMBER(";
                break;
            case WindowFunc::RANK:
                out_ << "RANK(";
                break;
            case WindowFunc::DENSE_RANK:
                out_ << "DENSE_RANK(";
                break;
            case WindowFunc::LAG:
                out_ << "LAG(";
                break;
            case WindowFunc::LEAD:
                out_ << "LEAD(";
                break;
            case WindowFunc::FIRST_VALUE:
                out_ << "FIRST_VALUE(";
                break;
            case WindowFunc::LAST_VALUE:
                out_ << "LAST_VALUE(";
                break;
            case WindowFunc::NTH_VALUE:
                out_ << "NTH_VALUE(";
                break;
            }

            // Print arguments
            bool first = true;
            for (auto *arg : node->args())
            {
                if (!first)
                    out_ << ", ";
                arg->accept(this);
                first = false;
            }

            out_ << ") OVER (";

            // Print window specification
            if (node->windowSpec())
            {
                node->windowSpec()->accept(this);
            }

            out_ << ")";
        }

        void ASTPrinter::visit(WindowSpec *node)
        {
            bool needs_space = false;

            // PARTITION BY
            if (!node->partitionBy().empty())
            {
                out_ << "PARTITION BY ";
                bool first = true;
                for (auto *expr : node->partitionBy())
                {
                    if (!first)
                        out_ << ", ";
                    expr->accept(this);
                    first = false;
                }
                needs_space = true;
            }

            // ORDER BY
            if (!node->orderBy().empty())
            {
                if (needs_space)
                    out_ << " ";
                out_ << "ORDER BY ";
                for (size_t i = 0; i < node->orderBy().size(); i++)
                {
                    if (i > 0)
                        out_ << ", ";
                    node->orderBy()[i]->accept(this);
                    out_ << (node->orderAscending()[i] ? " ASC" : " DESC");
                }
                needs_space = true;
            }

            // Frame clause
            if (node->hasFrame())
            {
                if (needs_space)
                    out_ << " ";
                out_ << (node->frameMode() == FrameMode::ROWS ? "ROWS" : "RANGE");
                out_ << " BETWEEN ";

                // Start boundary
                switch (node->frameStart().type)
                {
                case FrameBoundaryType::UNBOUNDED_PRECEDING:
                    out_ << "UNBOUNDED PRECEDING";
                    break;
                case FrameBoundaryType::PRECEDING:
                    if (node->frameStart().offset)
                        node->frameStart().offset->accept(this);
                    out_ << " PRECEDING";
                    break;
                case FrameBoundaryType::CURRENT_ROW:
                    out_ << "CURRENT ROW";
                    break;
                case FrameBoundaryType::FOLLOWING:
                    if (node->frameStart().offset)
                        node->frameStart().offset->accept(this);
                    out_ << " FOLLOWING";
                    break;
                case FrameBoundaryType::UNBOUNDED_FOLLOWING:
                    out_ << "UNBOUNDED FOLLOWING";
                    break;
                }

                out_ << " AND ";

                // End boundary
                switch (node->frameEnd().type)
                {
                case FrameBoundaryType::UNBOUNDED_PRECEDING:
                    out_ << "UNBOUNDED PRECEDING";
                    break;
                case FrameBoundaryType::PRECEDING:
                    if (node->frameEnd().offset)
                        node->frameEnd().offset->accept(this);
                    out_ << " PRECEDING";
                    break;
                case FrameBoundaryType::CURRENT_ROW:
                    out_ << "CURRENT ROW";
                    break;
                case FrameBoundaryType::FOLLOWING:
                    if (node->frameEnd().offset)
                        node->frameEnd().offset->accept(this);
                    out_ << " FOLLOWING";
                    break;
                case FrameBoundaryType::UNBOUNDED_FOLLOWING:
                    out_ << "UNBOUNDED FOLLOWING";
                    break;
                }
            }
        }

        void ASTPrinter::visit(JSONFuncExpr *node)
        {
            // Print JSON function name
            switch (node->func())
            {
            case JSONFunc::JSON_EXTRACT:
                out_ << "JSON_EXTRACT(";
                break;
            case JSONFunc::JSONB_EXTRACT_PATH:
                out_ << "JSONB_EXTRACT_PATH(";
                break;
            case JSONFunc::ARROW:
                // Binary operator format: arg1 -> arg2
                if (node->args().size() >= 2)
                {
                    node->args()[0]->accept(this);
                    out_ << " -> ";
                    node->args()[1]->accept(this);
                    return;
                }
                out_ << "ARROW(";
                break;
            case JSONFunc::DOUBLE_ARROW:
                // Binary operator format: arg1 ->> arg2
                if (node->args().size() >= 2)
                {
                    node->args()[0]->accept(this);
                    out_ << " ->> ";
                    node->args()[1]->accept(this);
                    return;
                }
                out_ << "DOUBLE_ARROW(";
                break;
            case JSONFunc::HASH_ARROW:
                // Binary operator format: arg1 #> arg2
                if (node->args().size() >= 2)
                {
                    node->args()[0]->accept(this);
                    out_ << " #> ";
                    node->args()[1]->accept(this);
                    return;
                }
                out_ << "HASH_ARROW(";
                break;
            case JSONFunc::HASH_DOUBLE_ARROW:
                // Binary operator format: arg1 #>> arg2
                if (node->args().size() >= 2)
                {
                    node->args()[0]->accept(this);
                    out_ << " #>> ";
                    node->args()[1]->accept(this);
                    return;
                }
                out_ << "HASH_DOUBLE_ARROW(";
                break;
            case JSONFunc::JSON_OBJECT:
                out_ << "JSON_OBJECT(";
                break;
            case JSONFunc::JSON_ARRAY:
                out_ << "JSON_ARRAY(";
                break;
            case JSONFunc::JSONB_BUILD_OBJECT:
                out_ << "JSONB_BUILD_OBJECT(";
                break;
            case JSONFunc::JSONB_BUILD_ARRAY:
                out_ << "JSONB_BUILD_ARRAY(";
                break;
            case JSONFunc::JSON_SET:
                out_ << "JSON_SET(";
                break;
            case JSONFunc::JSON_INSERT:
                out_ << "JSON_INSERT(";
                break;
            case JSONFunc::JSON_REMOVE:
                out_ << "JSON_REMOVE(";
                break;
            case JSONFunc::JSONB_SET:
                out_ << "JSONB_SET(";
                break;
            }

            // Print arguments (for non-operator functions)
            bool first = true;
            for (auto *arg : node->args())
            {
                if (!first)
                    out_ << ", ";
                arg->accept(this);
                first = false;
            }

            out_ << ")";
        }

        void ASTPrinter::visit(CoalesceExpr *node)
        {
            out_ << "COALESCE(";
            bool first = true;
            for (auto *arg : node->args())
            {
                if (!first)
                    out_ << ", ";
                arg->accept(this);
                first = false;
            }
            out_ << ")";
        }

        void ASTPrinter::visit(NullIfExpr *node)
        {
            out_ << "NULLIF(";
            node->expr1()->accept(this);
            out_ << ", ";
            node->expr2()->accept(this);
            out_ << ")";
        }

        void ASTPrinter::visit(CaseExpr *node)
        {
            out_ << "CASE ";

            // Simple CASE: print case operand
            if (node->isSimpleCase())
            {
                node->caseOperand()->accept(this);
                out_ << " ";
            }

            // Print WHEN clauses
            for (const auto& when : node->whenClauses())
            {
                out_ << "WHEN ";
                when.condition->accept(this);
                out_ << " THEN ";
                when.result->accept(this);
                out_ << " ";
            }

            // Print ELSE clause if present
            if (node->elseResult())
            {
                out_ << "ELSE ";
                node->elseResult()->accept(this);
                out_ << " ";
            }

            out_ << "END";
        }

        void ASTPrinter::visit(GroupingExpr *node)
        {
            out_ << "GROUPING(";
            node->arg()->accept(this);
            out_ << ")";
        }

        void ASTPrinter::visit(ArrayLiteral *node)
        {
            out_ << "ARRAY[";
            bool first = true;
            for (auto *elem : node->elements())
            {
                if (!first)
                    out_ << ", ";
                elem->accept(this);
                first = false;
            }
            out_ << "]";
        }

        void ASTPrinter::visit(SubqueryExpr *node)
        {
            switch (node->type())
            {
            case SubqueryType::SCALAR:
                out_ << "(";
                node->query()->accept(this);
                out_ << ")";
                break;
            case SubqueryType::EXISTS:
                out_ << "EXISTS (";
                node->query()->accept(this);
                out_ << ")";
                break;
            case SubqueryType::IN:
                out_ << "IN (";
                node->query()->accept(this);
                out_ << ")";
                break;
            case SubqueryType::NOT_IN:
                out_ << "NOT IN (";
                node->query()->accept(this);
                out_ << ")";
                break;
            case SubqueryType::ARRAY:
                out_ << "ARRAY(";
                node->query()->accept(this);
                out_ << ")";
                break;
            }
        }

        // Procedural language statement visitors (stub implementations)
        void ASTPrinter::visit(CreateTriggerStmt *node)
        {
            out_ << "CREATE TRIGGER";
        }

        void ASTPrinter::visit(DropTriggerStmt *node)
        {
            out_ << "DROP TRIGGER";
        }

        void ASTPrinter::visit(CreateDatabaseTriggerStmt *node)
        {
            out_ << "CREATE TRIGGER (DATABASE)";
        }

        void ASTPrinter::visit(CreateFunctionStmt *node)
        {
            out_ << "CREATE FUNCTION";
        }

        void ASTPrinter::visit(CreateProcedureStmt *node)
        {
            out_ << "CREATE PROCEDURE";
        }

        void ASTPrinter::visit(BlockStmt *node)
        {
            out_ << "BEGIN ... END";
        }

        void ASTPrinter::visit(VarDeclarationStmt *node)
        {
            out_ << "DECLARE";
        }

        void ASTPrinter::visit(AssignmentStmt *node)
        {
            out_ << "ASSIGNMENT";
        }

        void ASTPrinter::visit(IfStmt *node)
        {
            out_ << "IF";
        }

        void ASTPrinter::visit(LoopStmt *node)
        {
            out_ << "LOOP";
        }

        void ASTPrinter::visit(WhileStmt *node)
        {
            out_ << "WHILE";
        }

        void ASTPrinter::visit(ExitStmt *node)
        {
            out_ << "EXIT";
        }

        void ASTPrinter::visit(ReturnStmt *node)
        {
            out_ << "RETURN";
        }

        void ASTPrinter::visit(RaiseStmt *node)
        {
            out_ << "RAISE";
        }

        // ===== Security Statement Visitors (ALPHA Phase 1 - Security System Phase 2) =====

        void ASTPrinter::visit(CreateUserStmt *node)
        {
            printIndent();
            out_ << "CREATE USER " << pool_.get(node->username());
            if (node->hasPassword())
            {
                out_ << " WITH PASSWORD '***'";
            }
            if (node->isSuperuser())
            {
                out_ << " SUPERUSER";
            }
            out_ << "\n";
        }

        void ASTPrinter::visit(AlterUserStmt *node)
        {
            printIndent();
            out_ << "ALTER USER " << pool_.get(node->username());
            if (node->changePassword())
            {
                out_ << " WITH PASSWORD '***'";
            }
            if (node->changeSuperuser())
            {
                out_ << (node->isSuperuser() ? " SUPERUSER" : " NOSUPERUSER");
            }
            out_ << "\n";
        }

        void ASTPrinter::visit(DropUserStmt *node)
        {
            printIndent();
            out_ << "DROP USER ";
            if (node->ifExists())
            {
                out_ << "IF EXISTS ";
            }
            out_ << pool_.get(node->username());
            if (node->dropBehavior() == DropUserStmt::DropBehavior::CASCADE)
            {
                out_ << " CASCADE";
            }
            out_ << "\n";
        }

        void ASTPrinter::visit(CreateRoleStmt *node)
        {
            printIndent();
            out_ << "CREATE ROLE " << pool_.get(node->rolename()) << "\n";
        }

        void ASTPrinter::visit(DropRoleStmt *node)
        {
            printIndent();
            out_ << "DROP ROLE ";
            if (node->ifExists())
            {
                out_ << "IF EXISTS ";
            }
            out_ << pool_.get(node->rolename());
            if (node->dropBehavior() == DropRoleStmt::DropBehavior::CASCADE)
            {
                out_ << " CASCADE";
            }
            out_ << "\n";
        }

        void ASTPrinter::visit(CreateGroupStmt *node)
        {
            printIndent();
            out_ << "CREATE GROUP " << pool_.get(node->groupname()) << "\n";
        }

        void ASTPrinter::visit(DropGroupStmt *node)
        {
            printIndent();
            out_ << "DROP GROUP ";
            if (node->ifExists())
            {
                out_ << "IF EXISTS ";
            }
            out_ << pool_.get(node->groupname());
            if (node->dropBehavior() == DropGroupStmt::DropBehavior::CASCADE)
            {
                out_ << " CASCADE";
            }
            out_ << "\n";
        }

        void ASTPrinter::visit(GrantPrivilegeStmt *node)
        {
            printIndent();
            out_ << "GRANT [privileges] ON ";
            // Print object type
            switch (node->objectType())
            {
            case GrantPrivilegeStmt::ObjectType::TABLE:
                out_ << "TABLE";
                break;
            case GrantPrivilegeStmt::ObjectType::VIEW:
                out_ << "VIEW";
                break;
            case GrantPrivilegeStmt::ObjectType::SEQUENCE:
                out_ << "SEQUENCE";
                break;
            case GrantPrivilegeStmt::ObjectType::FUNCTION:
                out_ << "FUNCTION";
                break;
            case GrantPrivilegeStmt::ObjectType::PROCEDURE:
                out_ << "PROCEDURE";
                break;
            case GrantPrivilegeStmt::ObjectType::SCHEMA:
                out_ << "SCHEMA";
                break;
            case GrantPrivilegeStmt::ObjectType::DATABASE:
                out_ << "DATABASE";
                break;
            case GrantPrivilegeStmt::ObjectType::DOMAIN:
                out_ << "DOMAIN";
                break;
            }
            out_ << " " << pool_.get(node->objectName()) << " TO ";
            if (node->granteeType() == GrantPrivilegeStmt::GranteeType::PUBLIC)
            {
                out_ << "PUBLIC";
            }
            else
            {
                out_ << pool_.get(node->granteeName());
            }
            if (node->withGrantOption())
            {
                out_ << " WITH GRANT OPTION";
            }
            out_ << "\n";
        }

        void ASTPrinter::visit(RevokePrivilegeStmt *node)
        {
            printIndent();
            out_ << "REVOKE [privileges] ON ";
            switch (node->objectType())
            {
            case RevokePrivilegeStmt::ObjectType::TABLE:
                out_ << "TABLE";
                break;
            case RevokePrivilegeStmt::ObjectType::VIEW:
                out_ << "VIEW";
                break;
            case RevokePrivilegeStmt::ObjectType::SEQUENCE:
                out_ << "SEQUENCE";
                break;
            case RevokePrivilegeStmt::ObjectType::FUNCTION:
                out_ << "FUNCTION";
                break;
            case RevokePrivilegeStmt::ObjectType::PROCEDURE:
                out_ << "PROCEDURE";
                break;
            case RevokePrivilegeStmt::ObjectType::SCHEMA:
                out_ << "SCHEMA";
                break;
            case RevokePrivilegeStmt::ObjectType::DATABASE:
                out_ << "DATABASE";
                break;
            case RevokePrivilegeStmt::ObjectType::DOMAIN:
                out_ << "DOMAIN";
                break;
            }
            out_ << " " << pool_.get(node->objectName()) << " FROM ";
            if (node->granteeType() == RevokePrivilegeStmt::GranteeType::PUBLIC)
            {
                out_ << "PUBLIC";
            }
            else
            {
                out_ << pool_.get(node->granteeName());
            }
            if (node->revokeBehavior() == RevokePrivilegeStmt::RevokeBehavior::CASCADE)
            {
                out_ << " CASCADE";
            }
            out_ << "\n";
        }

        void ASTPrinter::visit(GrantRoleStmt *node)
        {
            printIndent();
            out_ << "GRANT " << pool_.get(node->rolename())
                 << " TO " << pool_.get(node->granteeName()) << "\n";
        }

        void ASTPrinter::visit(RevokeRoleStmt *node)
        {
            printIndent();
            out_ << "REVOKE " << pool_.get(node->rolename())
                 << " FROM " << pool_.get(node->granteeName());
            if (node->revokeBehavior() == RevokeRoleStmt::RevokeBehavior::CASCADE)
            {
                out_ << " CASCADE";
            }
            out_ << "\n";
        }

        void ASTPrinter::visit(SetRoleStmt *node)
        {
            printIndent();
            if (node->isReset())
            {
                out_ << "RESET ROLE\n";
            }
            else
            {
                out_ << "SET ROLE " << pool_.get(node->rolename()) << "\n";
            }
        }

        void ASTPrinter::visit(SetSessionAuthStmt *node)
        {
            printIndent();
            if (node->isReset())
            {
                out_ << "RESET SESSION AUTHORIZATION\n";
            }
            else
            {
                out_ << "SET SESSION AUTHORIZATION " << pool_.get(node->username()) << "\n";
            }
        }

        // P2-7: SET CONSTRAINTS statement
        void ASTPrinter::visit(SetConstraintsStmt *node)
        {
            printIndent();
            out_ << "SET CONSTRAINTS ";
            if (node->allConstraints())
            {
                out_ << "ALL";
            }
            else
            {
                bool first = true;
                for (auto name_id : node->constraintNames())
                {
                    if (!first) out_ << ", ";
                    out_ << pool_.get(name_id);
                    first = false;
                }
            }
            out_ << (node->isDeferred() ? " DEFERRED" : " IMMEDIATE") << "\n";
        }

        // Firebird ISQL Session Commands
        void ASTPrinter::visit(SetSqlDialectStmt *node)
        {
            printIndent();
            out_ << "SET SQL DIALECT " << static_cast<int>(node->dialect()) << "\n";
        }

        void ASTPrinter::visit(SetNamesStmt *node)
        {
            printIndent();
            out_ << "SET NAMES " << pool_.get(node->charsetName()) << "\n";
        }

        void ASTPrinter::visit(SetLocalTimeoutStmt *node)
        {
            printIndent();
            out_ << "SET LOCAL_TIMEOUT " << node->timeoutSeconds() << "\n";
        }

        // SAVEPOINT statements
        void ASTPrinter::visit(SavepointStmt *node)
        {
            printIndent();
            out_ << "SAVEPOINT " << pool_.get(node->name()) << "\n";
        }

        void ASTPrinter::visit(ReleaseSavepointStmt *node)
        {
            printIndent();
            out_ << "RELEASE SAVEPOINT " << pool_.get(node->name()) << "\n";
        }

        void ASTPrinter::visit(RollbackToSavepointStmt *node)
        {
            printIndent();
            out_ << "ROLLBACK TO SAVEPOINT " << pool_.get(node->name()) << "\n";
        }

        // User Defined Types
        void ASTPrinter::visit(CreateTypeStmt *node)
        {
            printIndent();
            out_ << "CREATE TYPE " << pool_.get(node->name());
            switch (node->typeKind())
            {
            case UserTypeKind::COMPOSITE:
                out_ << " AS (\n";
                increaseIndent();
                {
                    const auto& names = node->fieldNames();
                    const auto& types = node->fieldTypes();
                    for (size_t i = 0; i < names.size(); ++i)
                    {
                        printIndent();
                        out_ << pool_.get(names[i]) << " " << core::TypeSystem::getTypeName(types[i].type) << "\n";
                    }
                }
                decreaseIndent();
                printIndent();
                out_ << ")";
                break;
            case UserTypeKind::ENUM:
                out_ << " AS ENUM (";
                {
                    bool first = true;
                    for (auto val : node->enumValues())
                    {
                        if (!first) out_ << ", ";
                        out_ << "'" << pool_.get(val) << "'";
                        first = false;
                    }
                }
                out_ << ")";
                break;
            case UserTypeKind::RANGE:
                out_ << " AS RANGE (SUBTYPE = ";
                // For RANGE, the subtype is stored as first field
                if (!node->fieldTypes().empty())
                {
                    out_ << core::TypeSystem::getTypeName(node->fieldTypes()[0].type);
                }
                out_ << ")";
                break;
            }
            out_ << "\n";
        }

        void ASTPrinter::visit(CreateDomainStmt *node)
        {
            printIndent();
            out_ << "CREATE DOMAIN " << pool_.get(node->name())
                 << " AS " << core::TypeSystem::getTypeName(node->baseType().type);
            if (node->defaultValue())
            {
                out_ << " DEFAULT ";
                node->defaultValue()->accept(this);
            }
            if (node->isNotNull())
            {
                out_ << " NOT NULL";
            }
            if (node->checkExpr())
            {
                out_ << " CHECK (";
                node->checkExpr()->accept(this);
                out_ << ")";
            }
            out_ << "\n";
        }

        void ASTPrinter::visit(CallStmt *node)
        {
            printIndent();
            out_ << "CALL " << pool_.get(node->procedureName()) << "(";
            bool first = true;
            for (auto* arg : node->arguments())
            {
                if (!first)
                {
                    out_ << ", ";
                }
                first = false;
                arg->accept(this);
            }
            out_ << ")\n";
        }

        // Accept methods for security statements
        void CreateUserStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void AlterUserStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DropUserStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateRoleStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DropRoleStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void CreateGroupStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DropGroupStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void GrantPrivilegeStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void RevokePrivilegeStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void GrantRoleStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void RevokeRoleStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SetRoleStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SetSessionAuthStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        // P2-7: SET CONSTRAINTS statement
        void SetConstraintsStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        // Firebird ISQL compatibility: SET SQL DIALECT, SET NAMES, SET LOCAL_TIMEOUT
        void SetSqlDialectStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SetNamesStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SetLocalTimeoutStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        // Security Phase 3.4: Policy statements
        void CreatePolicyStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DropPolicyStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void AlterTableRLSStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

    } // namespace parser
} // namespace scratchbird
