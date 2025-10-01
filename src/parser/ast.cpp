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
            // Blocks are automatically freed by unique_ptr
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

        void InsertStmt::accept(ASTVisitor *visitor)
        {
            visitor->visit(this);
        }

        void SelectStmt::accept(ASTVisitor *visitor)
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
                case DataType::INTEGER:
                    out_ << "INTEGER";
                    break;
                case DataType::BIGINT:
                    out_ << "BIGINT";
                    break;
                case DataType::DOUBLE:
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

    } // namespace parser
} // namespace scratchbird