#include "scratchbird/parser/symbol_table.h"

namespace scratchbird
{
    namespace parser
    {

        // ===== Scope Implementation =====

        void Scope::addTable(StringPool::StringId name, std::unique_ptr<TableSymbol> table)
        {
            tables_[name] = std::move(table);
        }

        void Scope::addColumn(StringPool::StringId name, const ColumnSymbol &column)
        {
            columns_[name] = column;
        }

        TableSymbol *Scope::findTable(StringPool::StringId name) const
        {
            // Look in current scope
            auto it = tables_.find(name);
            if (it != tables_.end())
            {
                return it->second.get();
            }

            // Look in parent scope
            if (parent_)
            {
                return parent_->findTable(name);
            }

            return nullptr;
        }

        const ColumnSymbol *Scope::findColumn(StringPool::StringId name) const
        {
            // Look in current scope
            auto it = columns_.find(name);
            if (it != columns_.end())
            {
                return &it->second;
            }

            // Look in parent scope
            if (parent_)
            {
                return parent_->findColumn(name);
            }

            return nullptr;
        }

        // ===== SymbolTable Implementation =====

        SymbolTable::SymbolTable()
        {
            // Start with global scope
            scopes_.emplace_back(std::make_unique<Scope>());
            current_scope_ = scopes_.back().get();
        }

        SymbolTable::~SymbolTable() = default;

        void SymbolTable::pushScope()
        {
            scopes_.emplace_back(std::make_unique<Scope>(current_scope_));
            current_scope_ = scopes_.back().get();
        }

        void SymbolTable::popScope()
        {
            // Never pop the global scope (invariant: global scope always remains)
            if (scopes_.size() > 1)
            {
                scopes_.pop_back();
                current_scope_ = scopes_.back().get();
            }
            // If scopes_.size() <= 1, we're at global scope and don't pop
        }

        // ===== TypeChecker Implementation =====

        bool TypeChecker::areCompatible(const TypeName &t1, const TypeName &t2)
        {
            // Same type is always compatible
            if (t1.type == t2.type)
            {
                // For VARCHAR, precision doesn't affect compatibility
                return true;
            }

            // Numeric types are compatible
            if ((t1.type == DataType::INTEGER || t1.type == DataType::BIGINT ||
                 t1.type == DataType::DOUBLE) &&
                (t2.type == DataType::INTEGER || t2.type == DataType::BIGINT ||
                 t2.type == DataType::DOUBLE))
            {
                return true;
            }

            return false;
        }

        bool TypeChecker::canAssign(const TypeName &target, const TypeName &source)
        {
            // Same type can always be assigned
            if (target.type == source.type)
            {
                // For VARCHAR, check precision
                if (target.type == DataType::VARCHAR && target.precision > 0 &&
                    source.precision > 0)
                {
                    return source.precision <= target.precision;
                }
                return true;
            }

            // Allow numeric promotions
            switch (target.type)
            {
                case DataType::BIGINT:
                    return source.type == DataType::INTEGER;
                case DataType::DOUBLE:
                    return source.type == DataType::INTEGER || source.type == DataType::BIGINT;
                default:
                    return false;
            }
        }

        TypeName TypeChecker::getBinaryOpResultType(BinaryOp op, const TypeName &left,
                                                    const TypeName &right)
        {
            // Comparison operators always return boolean (represented as INTEGER for now)
            switch (op)
            {
                case BinaryOp::EQ:
                case BinaryOp::NE:
                case BinaryOp::LT:
                case BinaryOp::GT:
                case BinaryOp::LE:
                case BinaryOp::GE:
                case BinaryOp::AND:
                case BinaryOp::OR:
                    return TypeName(DataType::INTEGER); // Boolean type
                default:
                    break;
            }

            // Arithmetic operators
            // If either operand is DOUBLE, result is DOUBLE
            if (left.type == DataType::DOUBLE || right.type == DataType::DOUBLE)
            {
                return TypeName(DataType::DOUBLE);
            }

            // If either operand is BIGINT, result is BIGINT
            if (left.type == DataType::BIGINT || right.type == DataType::BIGINT)
            {
                return TypeName(DataType::BIGINT);
            }

            // Otherwise, result is INTEGER
            return TypeName(DataType::INTEGER);
        }

        bool TypeChecker::supportsComparison(const TypeName &type)
        {
            // All types support comparison
            return true;
        }

        bool TypeChecker::supportsArithmetic(const TypeName &type)
        {
            return type.type == DataType::INTEGER || type.type == DataType::BIGINT ||
                   type.type == DataType::DOUBLE;
        }

    } // namespace parser
} // namespace scratchbird