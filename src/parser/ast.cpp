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

        void UpdateStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void DeleteStmt::accept(ASTVisitor *visitor)
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

        void SweepStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SetTransactionStmt::accept(ASTVisitor *visitor)
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

            out_ << ") VALUES (";

            first = true;
            for (auto *val : node->values())
            {
                if (!first)
                    out_ << ", ";
                val->accept(this);
                first = false;
            }

            out_ << ")";
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

    } // namespace parser
} // namespace scratchbird
