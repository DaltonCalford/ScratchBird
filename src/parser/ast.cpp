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

        void AnalyzeStmt::accept(ASTVisitor *visitor)
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

            out_ << " FROM " << pool_.get(node->tableName());

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
            out_ << pool_.get(node->name());
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

    } // namespace parser
} // namespace scratchbird
